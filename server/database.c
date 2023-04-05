#include "include/database_include.h"
#define MAX_NAME_LENGTH 20

void db_close(struct database* database) {
    file_close(database->source_file);
    free(database->database_header);
    free(database);
}

struct database* db_create_in_file(const char *const file) {
    struct database* database = (struct database*) (malloc(sizeof(struct database) + sizeof(struct database_header)));

    FILE *f = NULL;

    if (file_open(&f, file, "wb+") == OK) {
        printf("File created\n");
    } else {
        printf("Couldn't create file\n");
        return NULL;
    }

    struct database_header* new_1 =(database_header*) malloc(sizeof(struct database_header));
    strncpy(new_1->name, "", 20);
    strncpy(new_1->name, file, strlen(file));
    new_1->table_count = 0;
    new_1->page_count = 0;
//    new_1->page_size = DEFAULT_PAGE_SIZE_BYTES;

    new_1->last_page_number = 0;
    new_1->database = database;

    struct page_header* page_header = page_add_real(new_1);

    database->source_file = f;
    database->database_header = new_1;

    database_save_to_file(f, new_1, page_header);

    return database;
}

struct database* db_get_from_file(const char *const file) {
    struct database* database =(struct database*) malloc(sizeof(struct database));
    struct database_header* header = (struct database_header*) malloc(sizeof(struct database_header));

    FILE *f = NULL;
    if (file_open(&f, file, "rb+") == OK)
        printf("File opened\n");
    else {
        printf("Could not open file\n");
        return NULL;
    }

    enum file_status result = database_header_read(f, header);
    if (result == OK) {
        header->database = database;
        database->source_file = f;
        database->database_header = header;
        return database;
    } else {
        printf("Error while reading existing database");
        return NULL;
    }
}

struct database* db_get(const char *const file, const enum database_state state) {
    if (state == SAVED_IN_FILE)
        return db_get_from_file(file);
    else
        return db_create_in_file(file);
}



struct table* table_create(struct schema* schema, const char* table_name, struct database*  database) {
    struct table_header* header = (table_header*)malloc(sizeof(struct table_header));
    if (is_table_present(database->source_file, database->database_header->table_count, table_name, header)) {
        printf("Can't create two relations with the same name");
        free(header);
        return NULL;
    }
    struct table* new_1 = (table*)malloc(sizeof(struct table));
    new_1->schema = schema;

    struct table_header* table_header = ( struct table_header*)malloc(sizeof(struct table_header));
    strncpy(table_header->name, "", MAX_NAME_LENGTH);
    strncpy(table_header->name, table_name, strlen(table_name));
    table_header->page_number_first = 0;
    table_header->page_number_last = 0;
    table_header->database = database;
    table_header->table = new_1;
    table_header->schema = *schema;
    table_header->page_count = 0;
    table_header->is_available = true;

    struct page_header* new_1_page = page_add(table_header, database->database_header);
    new_1->table_header = table_header;

    database->database_header->table_count++;

    table_header->real_number = database->database_header->table_count;

    page_header_write_real(database->source_file, database->database_header, table_header);

    database_header_save_changes(database->source_file, database->database_header);

    table_page_write(database->source_file, new_1_page, new_1->schema);

    return new_1;
}

struct table* table_get(const char *const table_name, struct database* database) {
    struct table* new_1 = (struct table*) malloc(sizeof(struct table));
    struct table_header* table_header = (struct table_header*) malloc(sizeof(struct table_header));
    struct schema* schema = (struct schema*) malloc(sizeof(struct schema));

    if (table_header_read(database->source_file, table_name, table_header, database->database_header->table_count) == OK) {
        new_1->schema = schema;
        new_1->table_header = table_header;
        table_read_columns(database->source_file, new_1);
        new_1->table_header->database = database;
        return new_1;
    } else {
        printf("Could not get table\n");
        return NULL;
    }
}

void table_close(struct table* table) {
    //free(created_relation->schema->start);
    free(table->table_header);
    //free(created_relation->schema);
    free(table);
}

struct page_header* page_create(struct database_header* database_header, struct table_header* table_header) {
    struct page_header* new_1 =(struct page_header*) malloc(sizeof (struct page_header));
    if (!new_1) {
        return NULL;
    }

    new_1->write_ptr = 0;
    new_1->next_page_number = 0;
//    new_1->is_dirty = false;
    new_1->remaining_space = 20;
    new_1->page_number = database_header->page_count;

    if (!table_header) {
        strncpy(new_1->table_name, "", MAX_TABLE_NAME_LENGTH);
    } else {
//        new_1->real_number = table_header->real_number;
        strncpy(new_1->table_name, "", MAX_TABLE_NAME_LENGTH);
        strncpy(new_1->table_name, table_header->name, MAX_TABLE_NAME_LENGTH);
    }

    return new_1;
}

struct page_header* page_add_real(struct database_header* database_header) {

    database_header->page_count++;

    struct page_header* new_1 = page_create(database_header, NULL);

    if (new_1) {

        if (database_header->last_page_number != 0) {
            database_update_last_page(database_header->database->source_file, database_header,
                                      new_1->page_number);
        }

        database_header->last_page_number = new_1->page_number;

        if (database_header->page_count != 1) {
            database_header_save_changes(database_header->database->source_file, database_header);
        }

        return new_1;

    } else return NULL;
}

struct page_header* page_add(struct table_header* table_header, struct database_header* database_header) {
    database_header->page_count++;
    table_header->page_count++;

    struct page_header* new_1 = page_create(database_header, table_header);

    if (new_1) {
        if (table_header->page_number_last != 0) {
            file_update_last_page(table_header->database->source_file, table_header->page_number_last,
                                  new_1->page_number);
        } else {
            table_header->page_number_first = new_1->page_number;
        }

        table_header->page_number_last = new_1->page_number;

        if (table_header->page_count != 1) {
            table_header_save_changes(database_header->database->source_file, table_header);
            database_header_save_changes(database_header->database->source_file, database_header);
        }

        return new_1;
    }
    return NULL;
}

struct query* query_make(enum query_types operation, struct table* table,const char* columns[],const void* vals[], int32_t cnt) {
    struct query* new_1 = (query*) malloc(sizeof(struct query));
    new_1->operation = operation;
    new_1->name = columns;
    new_1->value = vals;
    new_1->number = cnt;
    new_1->table = table;
    return new_1;
}

struct query_join* query_join_make(struct table* left, struct table* right,const char* left_column,const char* right_column) {
    struct query_join* new_1 = (struct query_join*)malloc(sizeof(struct query_join));
    new_1->left = left;
    new_1->right = right;
    new_1->left_column = left_column;
    new_1->right_column = right_column;
    return new_1;
}

char* query_execute(struct query *query, bool show_output, char* buf) {

    switch (query->operation) {
        case SELECT:
            printf("plrcm");
            return row_select(query, show_output, buf);
            break;
        case DELETE:
            return row_delete(query, show_output, buf);
            break;
        case UPDATE:
            return row_update(query, show_output,buf);
            break;
    }

}

char* query_join_execute(struct query_join* query, char* buf) {
    bool is_column_present_one = false;
    bool is_column_present_two = false;
    char name_one[MAX_NAME_LENGTH];
    char name_two[MAX_NAME_LENGTH];

    enum data_type type_one;
    enum data_type type_two;
    uint16_t size_one;
    uint16_t size_two;

    struct column* col = query->left->schema->start;

    for (size_t i = 0; i < query->left->schema->count; i++) {
        if (strcmp(col->name, query->left_column) == 0) {
            type_one = query->left->schema->start[i].data_type;
            is_column_present_one = true;
            strncpy(name_one, query->left->schema->start[i].name, MAX_NAME_LENGTH);
            size_one = query->left->schema->start[i].size;
            break;
        } else col = col->next;
    }

    col = query->right->schema->start;

    for (size_t i = 0; i < query->right->schema->count; i++) {
        if (strcmp(col->name, query->right_column) == 0) {
            type_two = query->right->schema->start[i].data_type;
            is_column_present_two = true;
            strncpy(name_two, query->right->schema->start[i].name, MAX_NAME_LENGTH);
            size_two = query->right->schema->start[i].size;
            break;
        } else col = col->next;
    }

    if (is_column_present_one && is_column_present_two) {
        struct query_params* query_one =(query_params*) malloc(sizeof(struct query_params));
        struct query_params* query_two =(query_params*) malloc(sizeof(struct query_params));

        uint32_t offset_two = column_get_offset(query->right->schema->start, name_two, query->right->schema->count);
        uint32_t offset_one = column_get_offset(query->left->schema->start, name_one, query->left->schema->count);



        if (offset_one != -1 && offset_two != -1) {
            query_one->data_type = type_one;
            query_one->size = size_one;
            query_one->offset = offset_one;
            strncpy(query_one->name, "", MAX_NAME_LENGTH);
            strncpy(query_one->name, name_one, MAX_NAME_LENGTH);

            query_two->data_type = type_two;
            query_two->size = size_two;
            query_two->offset = offset_two;
            strncpy(query_two->name, "", MAX_NAME_LENGTH);
            strncpy(query_two->name, name_two, MAX_NAME_LENGTH);

            buf = query_join(query->left-> table_header->database->source_file, query->left, query->right, query_one, query_two, buf);
        }

        free(query_one);
        free(query_two);

    } else printf("Couldn't complete query, attributes not present\n");
    return buf;
}

void query_close(struct query* query) {
    free(query);
}

void query_join_close(struct query_join* query) {
    free(query);
}

bool is_enough_space(struct page_header* page_header, uint32_t required) {
    if (page_header->remaining_space >= required) {
        return true;
    } else return false;
}




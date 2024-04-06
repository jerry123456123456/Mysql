#include <stdio.h>
#include <mysql/mysql.h>
#include <stdlib.h>
#include <string.h>

#define CONFIG_FILE "config.txt"

#define SQL_SELECT "select * from class"
#define INSERT_IMG "insert class(class_id,class_name,u_img) values(3,'三班',?);"
#define SELECT_IMG "select u_img from class where class_id=3;"
#define FILE_IMG (1024*1024)

// 数据库连接参数结构体
typedef struct {
    char server_ip[50];
    int server_port;
    char username[50];
    char passwd[50];
    char default_db[50];
} DBConfig;

DBConfig readConfigFile(const char* filename) {
    DBConfig config;
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        printf("Failed to open config file.\n");
        exit(-1);
    }

    fgets(config.server_ip, sizeof(config.server_ip), file);
    fscanf(file, "%d\n", &config.server_port);
    fgets(config.username, sizeof(config.username), file);
    fgets(config.passwd, sizeof(config.passwd), file);
    fgets(config.default_db, sizeof(config.default_db), file);

    // 去掉读取进来的换行符
    config.server_ip[strcspn(config.server_ip, "\n")] = 0;
    config.username[strcspn(config.username, "\n")] = 0;
    config.passwd[strcspn(config.passwd, "\n")] = 0;
    config.default_db[strcspn(config.default_db, "\n")] = 0;

    fclose(file);

    return config;
}

int read_image(char *filename,char *buffer){
    if(filename==NULL||buffer==NULL)return -1;
    FILE *fp=fopen(filename,"rb");
    if(fp==NULL){
	    printf("fopen failed\n");
	    return -2;
    }
    fseek(fp,0,SEEK_END);
    int length=ftell(fp);
    fseek(fp,0,SEEK_SET);
    int size=fread(buffer,1,length,fp);
    if(size!=length){
	    printf("fread failed:%d\n",size);
    	    return -3;
    }
    fclose(fp);
    return size;
}

int write_image(char *filename,char *buffer,int length){
    if(filename==NULL||buffer==NULL)return -1;
    FILE *fp=fopen(filename,"wb+"); //以写的方式打开，如果没有这个文件就创建
    if(fp==NULL){
            printf("fopen failed\n");
            return -2;
    }
    int size=fwrite(buffer,1,length,fp);
    if(size!=length){
	    printf("fwrite failed\n");
	    return -3;
    }
    fclose(fp);
    return 0;
}

int mysql_write(MYSQL *handle,char *buffer,int length){
    if(handle==NULL||buffer==NULL||length<=0)return -1;
    MYSQL_STMT *stmt=mysql_stmt_init(handle);
    int ret=mysql_stmt_prepare(stmt,INSERT_IMG,strlen(INSERT_IMG));
    if(ret){
	    printf("mysql_stmt_prepare:%s\n",mysql_error(handle));
	    return -2;
    }
    MYSQL_BIND param={0};
    param.buffer_type=MYSQL_TYPE_LONG_BLOB;
    param.buffer=NULL;
    param.is_null=0;
    param.length=NULL;
    ret=mysql_stmt_bind_param(stmt,&param);
    if(ret){
            printf("mysql_stmt_bind_param:%s\n",mysql_error(handle));
            return -3;
    }
    ret=mysql_stmt_send_long_data(stmt,0,buffer,length);
    if(ret){
            printf("mysql_stmt_send_long_data:%s\n",mysql_error(handle));
            return -4;
    }
    ret=mysql_stmt_execute(stmt);
    if(ret){
            printf("mysql_stmt_execute:%s\n",mysql_error(handle));
            return -5;
    }
    ret=mysql_stmt_close(stmt);
    if(ret){
            printf("mysql_stmt_close:%s\n",mysql_error(handle));
            return -6;
    }
    return ret;
}

int mysql_read(MYSQL *handle,char *buffer,int length){
    if(handle==NULL||buffer==NULL||length<=0)return -1;

    MYSQL_STMT *stmt=mysql_stmt_init(handle);
    int ret=mysql_stmt_prepare(stmt,SELECT_IMG,strlen(SELECT_IMG));
    if(ret){
            printf("mysql_stmt_prepare:%s\n",mysql_error(handle));
            return -2;
    }
    MYSQL_BIND result={0};
    result.buffer_type=MYSQL_TYPE_LONG_BLOB;
    unsigned long total_length=0;
    result.length=&total_length;

    ret=mysql_stmt_bind_result(stmt,&result);
    if(ret){
            printf("mysql_stmt_bind_result:%s\n",mysql_error(handle));
            return -3;
    }
    ret=mysql_stmt_execute(stmt);
    if(ret){
            printf("mysql_stmt_execute:%s\n",mysql_error(handle));
            return -4;
    }
    ret =mysql_stmt_store_result(stmt);
    if(ret){
            printf("mysql_stmt_store_result:%s\n",mysql_error(handle));
            return -5;
    }
    while(1){
	ret=mysql_stmt_fetch(stmt);
        if(ret!=0&&ret!=MYSQL_DATA_TRUNCATED)break;
	int start=0;
	while(start<(int)total_length){
		result.buffer=buffer+start;
		result.buffer_length=1;
		mysql_stmt_fetch_column(stmt,&result,0,start);
		start+=result.buffer_length;
	}
    }
    mysql_stmt_close(stmt);
    return total_length;
}

int main() {
    MYSQL mysql;
    mysql_init(&mysql);

    DBConfig dbConfig = readConfigFile(CONFIG_FILE);

    if (!mysql_real_connect(&mysql, dbConfig.server_ip, dbConfig.username,
                           dbConfig.passwd, dbConfig.default_db,
                           dbConfig.server_port, NULL, 0)) {
        printf("Failed to connect to database: %s\n", mysql_error(&mysql));
        return -1;
    }

    printf("write image\n");
    //mysql_select(&mysql);
    char buffer[FILE_IMG]={0};
    int length=read_image("a.jpg",buffer);
    if(length<=0)goto Exit;
    mysql_write(&mysql,buffer,length);
    
    printf("read image\n");
    memset(buffer,0,FILE_IMG);
    length=mysql_read(&mysql,buffer,FILE_IMG);

    write_image("b.jpg",buffer,length);

Exit:
    mysql_close(&mysql);
    return 0;
}

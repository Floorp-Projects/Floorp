/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* this little program will sequentially dump out
 * every record in the database
 */
#include "extcache.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <assert.h>

#ifdef _sgi
#include <sys/endian.h> 
#endif /* _sgi */

/* URL methods
 */
#define URL_GET_METHOD  0
#define URL_POST_METHOD 1
#define URL_HEAD_METHOD 2

static DB *
net_OpenExtCacheFatDB(char *filename)
{
	DB *rv;
    HASHINFO hash_info = {
        16*1024,  /* bucket size */
        0,        /* fill factor */
        0,        /* number of elements */
        0,        /* bytes to cache */
        0,        /* hash function */
        0};       /* byte order */


	rv = dbopen(filename,
									O_RDWR | O_CREAT,
									0644,
									DB_HASH,
				&hash_info);

	if(!rv)
	  {
		printf("Could not open cache database: %s\n", filename);
		exit(1);
	  }
	
	return(rv);
}

int main(int argc, char **argv)
{
    char url[4028];
    struct stat stat_s;
    net_CacheObject * cache_obj;
	DB * ext_cache_database=0;
	DBT key;
	DBT data;
    int len;
    char *end;

    memset(&cache_obj, 0, sizeof(net_CacheObject));

    if(argc != 2)
      {
        printf("Usage:\n"
                "%s database\n"
                "\n"
                "database: path and name of the database\n", argv[0]);
        exit(1);
      }

    /* open the cache database */
    ext_cache_database = net_OpenExtCacheFatDB(argv[1]);

	if(!ext_cache_database)
	  {
		perror("Could not open cache database");
		exit(1);
	  }

	while(!(ext_cache_database->seq)(ext_cache_database, &key, &data, 0))
	  {

        if(key.size == XP_STRLEN(EXT_CACHE_NAME_STRING)
            && !XP_STRCMP(key.data, EXT_CACHE_NAME_STRING))
          {
            /* make sure it's a terminated string */
            if(((char *)data.data)[data.size-1] == '\0')
                printf("\n\nDatabase Name: %s\n", (char*)data.data);
            else
                 printf("\n\nDatabase Name is corrupted!\n");
			printf("\n--------------------------------------\n");
			continue;
		  }

		/* try and convert the db struct to a cache struct */
	    cache_obj = net_DBDataToCacheStruct(&data);

		if(!cache_obj)
		  {
			printf("Malformed database entry:\n");
			printf("key: ");
			fwrite(key.data, 1, key.size, stdout);
			printf("\ndata: ");
			fwrite(data.data, 1, data.size, stdout);
			printf("\n");
			printf("--------------------------------------\n");
			continue;
		  }

	    /* the URL is 8 bytes into the key struct
	     */
	    printf("URL: %s\n",(char*)key.data+8);
	    printf("file: %s\n", cache_obj->filename);
	    printf("is_relative_path: %s\n", cache_obj->is_relative_path ? "TRUE" : "FALSE");
	    printf("content_type: %s\n", cache_obj->content_type);
	    printf("content_length: %d\n", cache_obj->content_length);
	    printf("last_modified: %s\n", ctime(&cache_obj->last_modified));
		printf("--------------------------------------\n");
	  }
}


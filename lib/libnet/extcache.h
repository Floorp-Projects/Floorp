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

#ifndef EXT_CACHE_H
#define EXT_CACHE_H

#ifndef EXT_DB_ROUTINES
#include "mcom_db.h"
#endif

#ifdef EXT_DB_ROUTINES
#define Bool char
#define uint32 unsigned int
#define int32  int
#define XP_NEW(structure)  ((structure *) malloc(sizeof(structure)))
#define XP_ALLOC   (void *) malloc
#define XP_MEMCPY  memcpy
#define XP_MEMSET  memset
#define TRACEMSG(x) printf x
#define FREEIF(x)  do { if(x) free(x); } while(0)
#define FREE free
#define XP_STRLEN  strlen
#define XP_STRCHR  strchr
#define XP_STRCMP  strcmp
#define XP_ASSERT  assert
#define MODULE_PRIVATE
#define PRIVATE static
#define TRUE  !0
#define FALSE 0

#include <stdio.h>
#include <string.h>
#include <db.h>
#endif 

#ifndef EXT_DB_ROUTINES
#include "mkutils.h"

#ifndef NSPR20
#include "prosdep.h"  /* for IS_LITTLE_ENDIAN / IS_BIG_ENDIAN */
#else
#include "prtypes.h"
#endif

#endif /* EXT_DB_ROUTINES */

#if !defined(IS_LITTLE_ENDIAN) && !defined(IS_BIG_ENDIAN)
ERROR! Must have a byte order
#endif

#ifdef IS_LITTLE_ENDIAN
#define COPY_INT32(_a,_b)  XP_MEMCPY(_a, _b, sizeof(int32));
#else
#define COPY_INT32(_a,_b)  /* swap */                   \
    do {                                                \
    ((char *)(_a))[0] = ((char *)(_b))[3];              \
    ((char *)(_a))[1] = ((char *)(_b))[2];              \
    ((char *)(_a))[2] = ((char *)(_b))[1];              \
    ((char *)(_a))[3] = ((char *)(_b))[0];              \
    } while(0)
#endif

#define EXT_CACHE_NAME_STRING "INT_ExternalCacheNameString"

/* Internal WARNING!!  Some slots of this structure 
 * are shared with URL_Struct and
 * History_entry.  If you add a slot, decide whether it needs to be shared
 * as well.
 */
typedef struct _net_CacheObject {
    time_t  last_modified;
    time_t  last_accessed;
    time_t  expires;
    Bool    is_netsite;
    uint32  content_length;

    char    * filename;                 /* cache file name */
    int32     filename_len;             /* optimization */
    Bool      is_relative_path;         /* is the path relative? */

    /* Security information */
    int32   security_on;                /* is security on? */
    unsigned char *sec_info;

    time_t  lock_date;                  /* the file is locked if this
                                         * is non-zero.  The date
                                         * represents the time the
                                         * lock was put in place.
                                         * Locks are only valid for
                                         * one session
                                         */

    int32       method;
    char    * address;
    uint32    post_data_size;
    char    * post_data;
    char    * post_headers;
    char    * content_type;
    char    * content_encoding;
    char    * charset;

    Bool      incomplete_file;          /* means that the whole
                                         * file is not there.
                                         * This can only be true
                                         * if the server supports byteranges
                                         */
    uint32    real_content_length;      /* the whole content length
                                         * i.e. the server size of a truncated 
                                         * client file
                                         */
    char    * page_services_url;
    char    * etag;			/* HTTP/1.1 Etag */

} net_CacheObject;

/* this is the version number of the cache database entry.
 * It should be incremented in integer ingrements up
 * to MAXINT32
 */
#define CACHE_FORMAT_VERSION 5

/* these defines specify the exact byte position
 * of the first 4 elements in the DBT data struct
 * Change these if you change the order of entry into
 * the DBT
 */
#define LAST_MODIFIED_BYTE_POSITION   \
                sizeof(int32)+sizeof(int32)
#define LAST_ACCESSED_BYTE_POSITION   \
                sizeof(int32)+sizeof(int32)+sizeof(time_t)
#define EXPIRES_BYTE_POSITION         \
                sizeof(int32)+sizeof(int32)+sizeof(time_t)+sizeof(time_t)
#define CONTENT_LENGTH_BYTE_POSITION  \
                sizeof(int32)+sizeof(int32)+sizeof(time_t)+sizeof(time_t) \
                +sizeof(time_t)
#define IS_NETSITE_BYTE_POSITION  \
                sizeof(int32)+sizeof(int32)+sizeof(time_t)+sizeof(time_t) \
                +sizeof(time_t)+sizeof(int32)

#define LOCK_DATE_BYTE_POSITION  \
                sizeof(int32)+sizeof(int32)+sizeof(time_t)+sizeof(time_t) \
                +sizeof(time_t)+sizeof(int32)+sizeof(char)

#define FILENAME_SIZE_BYTE_POSITION   \
                sizeof(int32)+sizeof(int32)+sizeof(time_t)+sizeof(time_t) \
                +sizeof(time_t)+sizeof(uint32)+sizeof(char)+sizeof(time_t)
#define FILENAME_BYTE_POSITION        \
                sizeof(int32)+sizeof(int32)+sizeof(time_t)+sizeof(time_t) \
                +sizeof(time_t)+sizeof(uint32)+sizeof(char)+sizeof(time_t) \
                +sizeof(int32)

/* generates a key for use in the cache database
 * from a CacheObject struct
 *
 * Key is based on the address and the post_data
 */
extern DBT *
net_GenCacheDBKey(char *address, char *post_data, int32 post_data_size);

/* returns a static string that contains the
 * URL->address of the key
 *
 * returns NULL on error
 */
extern char *
net_GetAddressFromCacheKey(DBT *key);


/* allocs and copies a new DBT from an existing DBT
 */
extern DBT * net_CacheDBTDup(DBT *obj);

/* free the cache object
 */
extern void net_freeCacheObj (net_CacheObject * cache_obj);

/* takes a cache object and returns a malloc'd
 * (void *) suitible for passing in as a database
 * data storage object
 */
extern DBT * net_CacheStructToDBData(net_CacheObject * old_obj);

/* takes a database storage object and returns a malloc'd
 * cache data object.  The cache object needs all of
 * it's parts free'd.
 *
 * returns NULL on parse error
 */
extern net_CacheObject * net_DBDataToCacheStruct(DBT * db_obj);

/* checks a date within a DBT struct so
 * that we don't have to convert it into a CacheObject
 *
 * This works because of the fixed length record format
 * of the first part of the specific DBT format I'm
 * using
 *
 * returns 0 on error
 */
extern time_t net_GetTimeInCacheDBT(DBT *data, int byte_position);

/* Sets a date within a DBT struct so
 * that we don't have to convert it into a CacheObject
 *
 * This works because of the fixed length record format
 * of the first part of the specific DBT format I'm
 * using
 *
 * returns 0 on error
 */
extern void net_SetTimeInCacheDBT(DBT *data, int byte_position, time_t date);

/* Gets the filename within a cache DBT struct so
 * that we don't have to convert it into a CacheObject
 *
 * This works because of the fixed length record format
 * of the first part of the specific DBT format I'm
 * using
 *
 * returns NULL on error
 */
extern char * net_GetFilenameInCacheDBT(DBT *data);

/* Gets a int32 within a DBT struct so
 * that we don't have to convert it into a CacheObject
 *
 * This works because of the fixed length record format
 * of the first part of the specific DBT format I'm
 * using
 *
 * returns 0 on error
 */
extern time_t net_GetInt32InCacheDBT(DBT *data, int byte_position);

/* free's a DBT struct
 */
extern void net_FreeCacheDBTdata(DBT *stuff);

/* stores a cache object in the DBM database
 */
extern void net_ExtCacheStore(DB *database, net_CacheObject * obj);

/* takes a database storage object and returns an un-malloc'd
 * cache data object. The structure returned has pointers
 * directly into the database memory and are only valid
 * until the next call to any database function
 *
 * do not free anything returned by this structure
 */
extern net_CacheObject * net_Fast_DBDataToCacheStruct(DBT *obj);

/* returns true if this DBT looks like a valid
 * entry.  It looks at the checksum and the
 * version number to see if it's valid
 */
extern Bool net_IsValidCacheDBT(DBT *obj);

#endif /* EXT_CACHE_H */

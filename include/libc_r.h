/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* libc_r.h  --  macros, defines, etc. to make using reentrant libc calls */
/*               a bit easier.  This was initially done for AIX pthreads, */
/*               but should be usable for anyone...                       */

/* Most of these use locally defined space instead of static library space. */
/* Because of this, we use the _INIT_R to declare/allocate space (stack),   */
/* and the plain routines to actually do it..._WARNING_: avoid allocating   */
/* memory wherever possible.  Memory allocation is fairly expensive, at     */
/* least on AIX...use arrays instead (which allocate from the stack.)       */
/* I know the names are a bit strange, but I wanted to be fairly certain    */
/* that we didn't have any namespace corruption...in general, the inits are */
/* R_<name>_INIT_R(), and the actual calls are R_<name>_R().                */

#ifndef _LIBC_R_H
#define _LIBC_R_H

/************/
/*  strtok  */
/************/
#define R_STRTOK_INIT_R() \
    char *r_strtok_r=NULL

#define R_STRTOK_R(return,source,delim) \     
    return=strtok_r(source,delim,&r_strtok_r)

#define R_STRTOK_NORET_R(source,delim) \
    strtok_r(source,delim,&r_strtok_r)

/**************/
/*  strerror  */
/**************/
#define R_MAX_STRERROR_LEN_R 8192     /* Straight from limits.h */

#define R_STRERROR_INIT_R() \
    char r_strerror_r[R_MAX_STRERROR_LEN_R]

#define R_STRERROR_R(val) \
    strerror_r(val,r_strerror_r,R_MAX_STRERROR_LEN_R)

/*****************/
/*  time things  */
/*****************/
#define R_ASCTIME_INIT_R() \
    char r_asctime_r[26]

#define R_ASCTIME_R(val) \
    asctime_r(val,r_asctime_r)

#define R_CTIME_INIT_R() \
    char r_ctime_r[26]

#define R_CTIME_R(val) \
    ctime_r(val,r_ctime_r)

#define R_GMTIME_INIT_R() \
    struct tm r_gmtime_r

#define R_GMTIME_R(time) \
    gmtime_r(time,&r_gmtime_r)

#define R_LOCALTIME_INIT_R() \
   struct tm r_localtime_r

#define R_LOCALTIME_R(val) \
   localtime_r(val,&r_localtime_r)
    
/***********/
/*  crypt  */
/***********/
#include <crypt.h>
#define R_CRYPT_INIT_R() \
    CRYPTD r_cryptd_r; \
    bzero(&r_cryptd_r,sizeof(CRYPTD)) 

#define R_CRYPT_R(pass,salt) \
    crypt_r(pass,salt,&r_cryptd_r)

/**************/
/*  pw stuff  */
/**************/
#define R_MAX_PW_LEN_R 1024
/* The following must be after the last declaration, but */
/* before the first bit of code...                       */
#define R_GETPWNAM_INIT_R(pw_ptr) \
    struct passwd r_getpwnam_pw_r; \
    char r_getpwnam_line_r[R_MAX_PW_LEN_R]; \
    pw_ptr = &r_getpwnam_pw_r

#define R_GETPWNAM_R(name) \
    getpwnam_r(name,&r_getpwnam_pw_r,r_getpwnam_line_r,R_MAX_PW_LEN_R)

/*******************/
/*  gethost stuff  */
/*******************/
#define R_GETHOSTBYADDR_INIT_R() \
    struct hostent r_gethostbyaddr_r; \
    struct hostent_data r_gethostbyaddr_data_r

#define R_GETHOSTBYADDR_R(addr,len,type,xptr_ent) \
    bzero(&r_gethostbyaddr_r,sizeof(struct hostent)); \
    bzero(&r_gethostbyaddr_data_r,sizeof(struct hostent_data)); \
    xptr_ent = &r_gethostbyaddr_r; \
    if (gethostbyaddr_r(addr,len,type, \
       &r_gethostbyaddr_r,&r_gethostbyaddr_data_r) == -1) { \
           xptr_ent = NULL; \
    }

#define R_GETHOSTBYNAME_INIT_R() \
    struct hostent r_gethostbyname_r; \
    struct hostent_data r_gethostbyname_data_r

#define R_GETHOSTBYNAME_R(name,xptr_ent) \
    bzero(&r_gethostbyname_r,sizeof(struct hostent)); \
    bzero(&r_gethostbyname_data_r,sizeof(struct hostent_data)); \
    xptr_ent = &r_gethostbyname_r; \
    if (gethostbyname_r(name, \
       &r_gethostbyname_r,&r_gethostbyname_data_r) == -1) { \
          xptr_ent = NULL; \
    }

#endif /* _LIBC_R_H */

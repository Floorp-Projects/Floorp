/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

/*
 * Prototypes for "zip" file reader support
 */

#ifndef _NS_ZIP_H_
#define _NS_ZIP_H_

PR_BEGIN_EXTERN_C

#include <time.h>
#include "prtypes.h"

/* XXX: IMO, the following should go into NSPR */
#if defined(XP_PC) && !defined(_WIN32)
#include <malloc.h> /* needed for _halloc() */
#ifndef HUGEP
#define HUGEP __huge
#endif
#ifndef CHECK_SIZE_LIMIT
#define CHECK_SIZE_LIMIT(x) PR_ASSERT( (x) <= 0xFFFFL )
#endif
#else /* else for defined(XP_PC) && !defined(_WIN32) */
#ifndef HUGEP
#define HUGEP
#endif
#ifndef CHECK_SIZE_LIMIT
#define CHECK_SIZE_LIMIT(x) ((void)0)
#endif
#endif /* defined(XP_PC) && !defined(_WIN32) */

/* XXX: IMO, the following should go into NSPR */
#ifndef FD_ERROR1
#define FD_ERROR1		((PRFileDesc*)-1)
#endif
#ifndef FD_ERROR2
#define FD_ERROR2		((PRFileDesc*)-2)
#endif
#ifndef FD_IS_ERROR
#define FD_IS_ERROR(fd)		((fd) == NULL || fd == FD_ERROR1 || fd == FD_ERROR2)
#endif


/*
 * Central directory entry
 */
typedef struct {
    char *fn;		/* file name */
    PRUint32 len;	/* file size */
    PRUint32 size;	/* file compressed size */
    int method;		/* Compression method */
    int mod;		/* file modification time */
    long off;		/* local file header offset */
} direl_t;

/*
 * Zip file
 */
typedef struct {
    char *fn;		/* zip file name */
    PRFileDesc *fd;	/* zip file descriptor */
    direl_t *dir;	/* zip file directory */
    PRUint32 nel;	/* number of directory entries */
    PRUint32 cenoff;	/* Offset of central directory (CEN) */
    PRUint32 endoff;	/* Offset of end-of-central-directory record */
} ns_zip_t;

struct stat;

PR_PUBLIC_API(PRBool)  ns_zip_lock(void);
PR_PUBLIC_API(void)    ns_zip_unlock(void);
PR_PUBLIC_API(ns_zip_t *) ns_zip_open(const char *fn);
PR_PUBLIC_API(void)    ns_zip_close(ns_zip_t *zip);
PR_PUBLIC_API(PRBool)  ns_zip_stat(ns_zip_t *zip, const char *fn, struct stat *sbuf);
PR_PUBLIC_API(PRBool)  ns_zip_get(ns_zip_t *zip, const char *fn, void *buf, PRInt32 len);
PR_PUBLIC_API(PRUint32) ns_zip_get_no_of_elements(ns_zip_t *zip, const char *fn_suffix);
PR_PUBLIC_API(PRUint32) ns_zip_list_files(ns_zip_t *zip, const char *fn_suffix, void HUGEP *buf, PRUint32 len);

PR_END_EXTERN_C

#endif /* !_NS_ZIP_H_ */ 

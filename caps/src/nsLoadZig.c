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

#include "zig.h"
#include "nsZip.h"
#include "nsLoadZig.h"
#include "prmem.h"
#include "plstr.h"

#define MANIFEST_SUFFIX_STR ".mf"
#define META_INF_STR "META-INF"
#define MANIFEST_MF_STR "manifest" MANIFEST_SUFFIX_STR
#define META_INF_STR_LEN 8
#define MANIFEST_STR_LEN 11

PR_BEGIN_EXTERN_C

#ifdef XP_UNIX
#include <sys/param.h> /* for MAXPATHLEN */
#endif

#ifdef MOZ_SECURITY

static PRInt32 
load_files(ns_zip_t* zip, ZIG* zig, char *fn_suffix,
           int (*callbackFnName) (int status, ZIG *zig, const char *metafile, 
                                  char *pathname, char *errortext));
static PRInt32 
loadSignatures(ns_zip_t *zip, ZIG *zig,
               int (*callbackFnName) (int status, ZIG *zig, const char *metafile, 
                                      char *pathname, char *errortext));


static PRInt32 
load_files(ns_zip_t* zip, ZIG* zig, char *fn_suffix, 
           int (*callbackFnName) (int status, ZIG *zig, const char *metafile, 
                                  char *pathname, char *errortext))
{
    struct stat st;
    char HUGEP * classData;
    char HUGEP * fileData;
    char HUGEP * dataPtr;
    char* fn;
    int len;
    PRInt32 dataSize;
    PRInt32 noOfSigs;
    PRInt32 i;
    int ret;

    dataSize = ns_zip_get_no_of_elements(zip, fn_suffix) * MAXPATHLEN;

    /* No elements of the given suffix were found... */
    if (dataSize == 0) {
        return 0;
    }
    if ((fileData = (char HUGEP * )PR_MALLOC(dataSize)) == 0) {
        return -1;
    }
    noOfSigs = ns_zip_list_files(zip, fn_suffix, fileData, dataSize);
    
#if defined(XP_PC) && !defined(_WIN32)
    /* 
     * Create a temporary buffer to hold each pathname from fileData.
     * This is necessary since the pathnames returned by ns_zip_list_files(...)
     * could cross segment boundaries
     */
    if ((fn = (char *)malloc(MAXPATHLEN+1)) == NULL) {
        PR_Free(fileData);
        return -1;
    }
#endif

    dataPtr = fileData;
    for (i = 0; i < noOfSigs; i++, len=strlen(fn), dataPtr += len +1) {
#if !defined(XP_PC) || defined(_WIN32)
        fn = dataPtr;
#else
        /* 
         * Copy the pathname into the temp buffer.  This insures that the
         * pathname will not cross a segment boundary! 
         */
        HUGE_STRNCPY(fn, dataPtr, MAXPATHLEN);
        fn[MAXPATHLEN] ='\0';
#endif
        if (PL_strncasecmp (fn, META_INF_STR, META_INF_STR_LEN)) {
            continue;
        }
        if ((fn[META_INF_STR_LEN] != '/') && (fn[META_INF_STR_LEN] != '\\')) {
            continue;
        }
        if ((!PL_strcasecmp(fn_suffix, MANIFEST_SUFFIX_STR)) && 
            (PL_strncasecmp(&fn[META_INF_STR_LEN+1], MANIFEST_MF_STR, MANIFEST_STR_LEN))) {
            continue;
        }
        if (!ns_zip_stat(zip, fn, &st)) {
		    callbackFnName(-1, zig, fn, zip->fn, "ns_zip_stat failed");
            break;
        }
        if ((classData = (char HUGEP *)PR_MALLOC(st.st_size + 1)) == 0) {
		    callbackFnName(-1, zig, fn, zip->fn, "malloc failed while setting up zig");
            break;
        }
        if (!ns_zip_get(zip, fn, classData, st.st_size)) {
		    callbackFnName(-1, zig, fn, zip->fn, "ns_zip_get failed");
            PR_Free(classData);
            break;
        }
        classData[st.st_size] = '\0';
        if ((ret = SOB_parse_manifest(classData, st.st_size, fn, zip->fn, 
                               zig)) < 0) {
            PR_Free(classData);
		    callbackFnName(ret, zig, fn, zip->fn, SOB_get_error(ret));
            break;
        }

        PR_Free(classData);
    }

    PR_Free(fileData);
#if defined(XP_PC) && !defined(_WIN32)
    /* Free the temporary buffer */
    free(fn);
#endif

    return (i < noOfSigs) ? -1 : noOfSigs;
}


static PRInt32
loadSignatures(ns_zip_t* zip, ZIG *zig, 
               int (*callbackFnName) (int status, ZIG *zig, const char *metafile, 
                                      char *pathname, char *errortext))
{
    PRInt32 noOfSigs;
	int ret_val;

    if ((noOfSigs = load_files(zip, zig, ".sf", callbackFnName)) < 0)
	    return noOfSigs;
    if ((ret_val = load_files(zip, zig, ".rsa", callbackFnName)) < 0)
        return ret_val;
    if ((ret_val = load_files(zip, zig, ".dsa", callbackFnName)) < 0)
        return ret_val;
    return noOfSigs;
}

#endif /* MOZ_SECURITY */

PR_PUBLIC_API(void *)
nsInitializeZig(ns_zip_t *zip,
                int (*callbackFnName) (int status, ZIG *zig, 
                                       const char *metafile, 
                                       char *pathname, char *errortext))
{
    ZIG *zig = NULL;
#ifdef MOZ_SECURITY

    zig = SOB_new();
    if (zig == NULL) {
        return NULL;
    }

	SOB_set_callback(ZIG_CB_SIGNAL, zig, callbackFnName);

    if (load_files(zip, zig, MANIFEST_SUFFIX_STR, callbackFnName) <= 0) {
	    /* If the number of manifest files loaded are zero
	     * then this is an old style zip file. No need to 
	     * to allocate zig or verification of signatures.
	     */
        SOB_destroy(zig);
        return NULL;
    }

    if (loadSignatures(zip, zig, callbackFnName) < 0) {
        SOB_destroy(zig);
        return NULL;
    }
#endif /* MOZ_SECURITY */
    return (void *)zig;
}

PR_END_EXTERN_C

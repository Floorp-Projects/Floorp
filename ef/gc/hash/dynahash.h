/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; -*- 
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

/*******************************************************************************
 * Hash Tables: Based on Larson's in-memory adaption of linear hashing 
 * [CACM, vol 31, #4, 1988 pp 446-457]
 ******************************************************************************/

#ifndef dynahash_h__
#define dynahash_h__

#include "prtypes.h"
             
#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/*******************************************************************************
 * PLDynahash
 ******************************************************************************/

#define PL_DYNAHASH_SIZE                (1 << 16)
#define PL_DYNAHASH_BUCKETSIZE_SHIFT    3
#define PL_DYNAHASH_BUCKETSIZE          (1 << PL_DYNAHASH_BUCKETSIZE_SHIFT)
#define PL_DYNAHASH_MAX_DIR_SIZE        (PL_DYNAHASH_SIZE / PL_DYNAHASH_BUCKETSIZE)
#define PL_DYNAHASH_LOAD_FACTOR         2

#define PL_DYNAHASH_MAGIC_NUMBER        65521U

typedef struct PLDynahashElement {
    struct PLDynahashElement* next;
    /* other data follows */
} PLDynahashElement;

typedef PLDynahashElement*  PLHashBucket;   /* array of PLDynahashElement handles */

typedef PLHashBucket*   PLHashDir;      /* array of HashBucket handles */

typedef PRUword
(PR_CALLBACK* PLHashFun)(void* context, PLDynahashElement* x);

typedef PRBool
(PR_CALLBACK* PLEqualFun)(void* context, PLDynahashElement* x, PLDynahashElement* y);

typedef void
(PR_CALLBACK* PLDestroyFun)(void* context, PLDynahashElement* x);

typedef struct PLDynahash {
    PLHashDir*          directory;
    PLHashFun           hash;
    PLEqualFun          equal;
    PLDestroyFun        destroy;
    PRUint32            keyCount;
    PRUint16            nextBucketToSplit;
    PRUint16            maxSize;
    PRUint16            bucketCount;
    void*               context;
#ifdef PL_DYNAHASH_STATISTICS
    PRUint32            accesses;
    PRUint32            collisions;
    PRUint32            expansions;
#endif /* PL_DYNAHASH_STATISTICS */
} PLDynahash;

/******************************************************************************/

PR_EXTERN(PRStatus)
PL_DynahashInit(PLDynahash* self, PRUint32 initialSize, PLHashFun hash,
                PLEqualFun equal, PLDestroyFun del, void* context);

PR_EXTERN(void)
PL_DynahashCleanup(PLDynahash* self);
    
PR_EXTERN(PLDynahash*)
PL_NewDynahash(PRUint32 initialSize,
               PLHashFun hash, PLEqualFun equal,
               PLDestroyFun del, void* context);

PR_EXTERN(void)
PL_DynahashDestroy(PLDynahash* self);

PR_EXTERN(PLDynahashElement*)
PL_DynahashLookup(PLDynahash* self, PLDynahashElement* element);
    
PR_EXTERN(PRStatus)
PL_DynahashAdd(PLDynahash* self, PLDynahashElement* element, PRBool replace,
               PLDynahashElement* *elementToDestroy);
    
PR_EXTERN(PLDynahashElement*)
PL_DynahashRemove(PLDynahash* self, PLDynahashElement* element);

PR_EXTERN(void)
PL_DynahashEmpty(PLDynahash* self);

#define PL_DynahashSize(self)        ((self)->keyCount)

#ifdef DEBUG

PR_EXTERN(PRBool)
PL_DynahashIsValid(PLDynahash* self);

PR_EXTERN(PRBool)
PL_DynahashTest(void);

#endif /* NDEBUG */
    
/*******************************************************************************
 * Hash Table Iterator
 ******************************************************************************/

typedef struct PLDynahashIterator {
    PLDynahash*         table;
    PRBool              done;
    PRUint32            bucket;
    PRInt32             bin;
    PRInt32             curno;
    PRUint32            lbucket;
    PRInt32             lbin;
    PRInt32             lcurno;
    PLHashBucket*       b;
    PLDynahashElement*  nextp;
    PLDynahashElement*  curp;
} PLDynahashIterator;

PR_EXTERN(void)
PL_DynahashIteratorInit(PLDynahashIterator* self, PLDynahash* ht);

PR_EXTERN(PLDynahashElement*)
PL_DynahashIteratorNext(PLDynahashIterator* self);

#define PL_CurrentDynahashIterator(self)        ((self)->curp)

PR_EXTERN(PRUword)
PL_DynahashIteratorCurrentAddress(PLDynahashIterator* self);

PR_EXTERN(PLDynahashElement*)
PL_DynahashIteratorRemoveCurrent(PLDynahashIterator* self);

#define PL_DynahashIteratorIsDone(self)         ((self)->done)

PR_EXTERN(void)
PL_DynahashIteratorReset(PLDynahashIterator* self, PLDynahash* ht);

/******************************************************************************/
#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif /* dynahash_h__ */
/******************************************************************************/

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/*
 * This is a copy of the NSPR hash-table library, but it has been slightly
 * modified to allow an additional argument to be passed into the hash
 * key-comparision function.  This is used to maintain thread-safety by
 * passing in a JNIEnv pointer to the key-comparison function rather
 * than storing it in a global.  All types,function names, etc. have
 * been renamed from their original NSPR names to protect the innocent.
 */

#ifndef jsj_hash_h___
#define jsj_hash_h___
/*
 * API to portable hash table code.
 */
#include <stddef.h>
#include <stdio.h>
#include "prtypes.h"

PR_BEGIN_EXTERN_C

typedef struct JSJHashEntry  JSJHashEntry;
typedef struct JSJHashTable  JSJHashTable;
typedef PRUint32 JSJHashNumber;
#define JSJ_HASH_BITS 32
typedef JSJHashNumber (*JSJHashFunction)(const void *key, void *arg);
typedef PRIntn (*JSJHashComparator)(const void *v1, const void *v2, void *arg);
typedef PRIntn (*JSJHashEnumerator)(JSJHashEntry *he, PRIntn i, void *arg);

/* Flag bits in JSJHashEnumerator's return value */
#define HT_ENUMERATE_NEXT       0       /* continue enumerating entries */
#define HT_ENUMERATE_STOP       1       /* stop enumerating entries */
#define HT_ENUMERATE_REMOVE     2       /* remove and free the current entry */
#define HT_ENUMERATE_UNHASH     4       /* just unhash the current entry */

typedef struct JSJHashAllocOps {
    void *              (*allocTable)(void *pool, size_t size);
    void                (*freeTable)(void *pool, void *item);
    JSJHashEntry *      (*allocEntry)(void *pool, const void *key);
    void                (*freeEntry)(void *pool, JSJHashEntry *he, PRUintn flag);
} JSJHashAllocOps;

#define HT_FREE_VALUE   0               /* just free the entry's value */
#define HT_FREE_ENTRY   1               /* free value and entire entry */

struct JSJHashEntry {
    JSJHashEntry        *next;          /* hash chain linkage */
    JSJHashNumber       keyHash;        /* key hash function result */
    const void          *key;           /* ptr to opaque key */
    void                *value;         /* ptr to opaque value */
};

struct JSJHashTable {
    JSJHashEntry         **buckets;      /* vector of hash buckets */
    PRUint32              nentries;       /* number of entries in table */
    PRUint32              shift;          /* multiplicative hash shift */
    JSJHashFunction     keyHash;        /* key hash function */
    JSJHashComparator   keyCompare;     /* key comparison function */
    JSJHashComparator   valueCompare;   /* value comparison function */
    JSJHashAllocOps     *allocOps;      /* allocation operations */
    void                *allocPriv;     /* allocation private data */
#ifdef HASHMETER
    PRUint32              nlookups;       /* total number of lookups */
    PRUint32              nsteps;         /* number of hash chains traversed */
    PRUint32              ngrows;         /* number of table expansions */
    PRUint32              nshrinks;       /* number of table contractions */
#endif
};

/*
 * Create a new hash table.
 * If allocOps is null, use default allocator ops built on top of malloc().
 */
PR_EXTERN(JSJHashTable *)
JSJ_NewHashTable(PRUint32 n, JSJHashFunction keyHash,
                JSJHashComparator keyCompare, JSJHashComparator valueCompare,
                JSJHashAllocOps *allocOps, void *allocPriv);

PR_EXTERN(void)
JSJ_HashTableDestroy(JSJHashTable *ht);

/* Low level access methods */
PR_EXTERN(JSJHashEntry **)
JSJ_HashTableRawLookup(JSJHashTable *ht, JSJHashNumber keyHash, const void *key, void *arg);

PR_EXTERN(JSJHashEntry *)
JSJ_HashTableRawAdd(JSJHashTable *ht, JSJHashEntry **hep, JSJHashNumber keyHash,
                   const void *key, void *value, void *arg);

PR_EXTERN(void)
JSJ_HashTableRawRemove(JSJHashTable *ht, JSJHashEntry **hep, JSJHashEntry *he, void *arg);

/* Higher level access methods */
PR_EXTERN(JSJHashEntry *)
JSJ_HashTableAdd(JSJHashTable *ht, const void *key, void *value, void *arg);

PR_EXTERN(PRBool)
JSJ_HashTableRemove(JSJHashTable *ht, const void *key, void *arg);

PR_EXTERN(PRIntn)
JSJ_HashTableEnumerateEntries(JSJHashTable *ht, JSJHashEnumerator f, void *arg);

PR_EXTERN(void *)
JSJ_HashTableLookup(JSJHashTable *ht, const void *key, void *arg);

PR_EXTERN(PRIntn)
JSJ_HashTableDump(JSJHashTable *ht, JSJHashEnumerator dump, FILE *fp);

/* General-purpose C string hash function. */
PR_EXTERN(JSJHashNumber)
JSJ_HashString(const void *key);

/* Compare strings using strcmp(), return true if equal. */
PR_EXTERN(int)
JSJ_CompareStrings(const void *v1, const void *v2);

/* Stub function just returns v1 == v2 */
PR_EXTERN(PRIntn)
JSJ_CompareValues(const void *v1, const void *v2);

PR_END_EXTERN_C

#endif /* jsj_hash_h___ */

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef prhash_h___
#define prhash_h___
/*
 * API to portable hash table code.
 */
#include <stddef.h>
#include <stdio.h>
#include "prtypes.h"

PR_BEGIN_EXTERN_C

typedef uint32 prhashcode;
#define PR_HASH_BITS 32
#define PR_GOLDEN_RATIO 0x9E3779B9U

typedef prhashcode (*PRHashFunction)(const void *key);
typedef intN (*PRHashComparator)(const void *v1, const void *v2);
typedef intN (*PRHashEnumerator)(PRHashEntry *he, intN i, void *arg);

/* Flag bits in PRHashEnumerator's return value */
#define HT_ENUMERATE_NEXT       0       /* continue enumerating entries */
#define HT_ENUMERATE_STOP       1       /* stop enumerating entries */
#define HT_ENUMERATE_REMOVE     2       /* remove and free the current entry */
#define HT_ENUMERATE_UNHASH     4       /* just unhash the current entry */

typedef struct PRHashAllocOps {
    void *              (*allocTable)(void *pool, size_t size);
    void                (*freeTable)(void *pool, void *item);
    PRHashEntry *       (*allocEntry)(void *pool, const void *key);
    void                (*freeEntry)(void *pool, PRHashEntry *he, uintN flag);
} PRHashAllocOps;

#define HT_FREE_VALUE   0               /* just free the entry's value */
#define HT_FREE_ENTRY   1               /* free value and entire entry */

struct PRHashEntry {
    PRHashEntry         *next;          /* hash chain linkage */
    prhashcode          keyHash;        /* key hash function result */
    const void          *key;           /* ptr to opaque key */
    void                *value;         /* ptr to opaque value */
};

struct PRHashTable {
    PRHashEntry         **buckets;      /* vector of hash buckets */
    uint32              nentries;       /* number of entries in table */
    uint32              shift;          /* multiplicative hash shift */
    PRHashFunction      keyHash;        /* key hash function */
    PRHashComparator    keyCompare;     /* key comparison function */
    PRHashComparator    valueCompare;   /* value comparison function */
    PRHashAllocOps      *allocOps;      /* allocation operations */
    void                *allocPriv;     /* allocation private data */
#ifdef HASHMETER
    uint32              nlookups;       /* total number of lookups */
    uint32              nsteps;         /* number of hash chains traversed */
    uint32              ngrows;         /* number of table expansions */
    uint32              nshrinks;       /* number of table contractions */
#endif
};

/*
 * Create a new hash table.
 * If allocOps is null, use default allocator ops built on top of malloc().
 */
extern PR_PUBLIC_API(PRHashTable *)
PR_NewHashTable(uint32 n, PRHashFunction keyHash,
                PRHashComparator keyCompare, PRHashComparator valueCompare,
                PRHashAllocOps *allocOps, void *allocPriv);

extern PR_PUBLIC_API(void)
PR_HashTableDestroy(PRHashTable *ht);

/* Low level access methods */
extern PR_PUBLIC_API(PRHashEntry **)
PR_HashTableRawLookup(PRHashTable *ht, prhashcode keyHash, const void *key);

extern PR_PUBLIC_API(PRHashEntry *)
PR_HashTableRawAdd(PRHashTable *ht, PRHashEntry **hep, prhashcode keyHash,
                   const void *key, void *value);

extern PR_PUBLIC_API(void)
PR_HashTableRawRemove(PRHashTable *ht, PRHashEntry **hep, PRHashEntry *he);

/* Higher level access methods */
extern PR_PUBLIC_API(PRHashEntry *)
PR_HashTableAdd(PRHashTable *ht, const void *key, void *value);

extern PR_PUBLIC_API(PRBool)
PR_HashTableRemove(PRHashTable *ht, const void *key);

extern PR_PUBLIC_API(intN)
PR_HashTableEnumerateEntries(PRHashTable *ht, PRHashEnumerator f, void *arg);

extern PR_PUBLIC_API(void *)
PR_HashTableLookup(PRHashTable *ht, const void *key);

extern PR_PUBLIC_API(intN)
PR_HashTableDump(PRHashTable *ht, PRHashEnumerator dump, FILE *fp);

/* General-purpose C string hash function. */
extern PR_PUBLIC_API(prhashcode)
PR_HashString(const void *key);

/* Compare strings using strcmp(), return true if equal. */
extern PR_PUBLIC_API(int)
PR_CompareStrings(const void *v1, const void *v2);

/* Stub function just returns v1 == v2 */
PR_PUBLIC_API(intN)
PR_CompareValues(const void *v1, const void *v2);

PR_END_EXTERN_C

#endif /* prhash_h___ */

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jshash_h___
#define jshash_h___

/*
 * API to portable hash table code.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

extern "C" {

typedef uint32_t JSHashNumber;
typedef struct JSHashEntry JSHashEntry;
typedef struct JSHashTable JSHashTable;

#define JS_HASH_BITS 32
#define JS_GOLDEN_RATIO 0x9E3779B9U

typedef JSHashNumber (* JSHashFunction)(const void *key);
typedef int (* JSHashComparator)(const void *v1, const void *v2);
typedef int (* JSHashEnumerator)(JSHashEntry *he, int i, void *arg);

/* Flag bits in JSHashEnumerator's return value */
#define HT_ENUMERATE_NEXT       0       /* continue enumerating entries */
#define HT_ENUMERATE_STOP       1       /* stop enumerating entries */
#define HT_ENUMERATE_REMOVE     2       /* remove and free the current entry */

typedef struct JSHashAllocOps {
    void *              (*allocTable)(void *pool, size_t size);
    void                (*freeTable)(void *pool, void *item, size_t size);
    JSHashEntry *       (*allocEntry)(void *pool, const void *key);
    void                (*freeEntry)(void *pool, JSHashEntry *he, unsigned flag);
} JSHashAllocOps;

#define HT_FREE_VALUE   0               /* just free the entry's value */
#define HT_FREE_ENTRY   1               /* free value and entire entry */

struct JSHashEntry {
    JSHashEntry         *next;          /* hash chain linkage */
    JSHashNumber        keyHash;        /* key hash function result */
    const void          *key;           /* ptr to opaque key */
    void                *value;         /* ptr to opaque value */
};

struct JSHashTable {
    JSHashEntry         **buckets;      /* vector of hash buckets */
    uint32_t            nentries;       /* number of entries in table */
    uint32_t            shift;          /* multiplicative hash shift */
    JSHashFunction      keyHash;        /* key hash function */
    JSHashComparator    keyCompare;     /* key comparison function */
    JSHashComparator    valueCompare;   /* value comparison function */
    JSHashAllocOps      *allocOps;      /* allocation operations */
    void                *allocPriv;     /* allocation private data */
#ifdef JS_HASHMETER
    uint32_t            nlookups;       /* total number of lookups */
    uint32_t            nsteps;         /* number of hash chains traversed */
    uint32_t            ngrows;         /* number of table expansions */
    uint32_t            nshrinks;       /* number of table contractions */
#endif
};

/*
 * Create a new hash table.
 * If allocOps is null, use default allocator ops built on top of malloc().
 */
extern JSHashTable * 
JS_NewHashTable(uint32_t n, JSHashFunction keyHash,
                JSHashComparator keyCompare, JSHashComparator valueCompare,
                JSHashAllocOps *allocOps, void *allocPriv);

extern void
JS_HashTableDestroy(JSHashTable *ht);

/* Low level access methods */
extern JSHashEntry **
JS_HashTableRawLookup(JSHashTable *ht, JSHashNumber keyHash, const void *key);

extern JSHashEntry *
JS_HashTableRawAdd(JSHashTable *ht, JSHashEntry **&hep, JSHashNumber keyHash,
                   const void *key, void *value);

extern void
JS_HashTableRawRemove(JSHashTable *ht, JSHashEntry **hep, JSHashEntry *he);

/* Higher level access methods */
extern JSHashEntry *
JS_HashTableAdd(JSHashTable *ht, const void *key, void *value);

extern bool
JS_HashTableRemove(JSHashTable *ht, const void *key);

extern int
JS_HashTableEnumerateEntries(JSHashTable *ht, JSHashEnumerator f, void *arg);

extern void *
JS_HashTableLookup(JSHashTable *ht, const void *key);

extern int
JS_HashTableDump(JSHashTable *ht, JSHashEnumerator dump, FILE *fp);

/* General-purpose C string hash function. */
extern JSHashNumber
JS_HashString(const void *key);

/* Stub function just returns v1 == v2 */
extern int
JS_CompareValues(const void *v1, const void *v2);

} // extern "C"

#endif /* jshash_h___ */

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

/*
 * PR hash table package.
 */
#include <stdlib.h>
#include <string.h>
#include "prtypes.h"
#include "prassert.h"
#include "prhash.h"

/* Compute the number of buckets in ht */
#define NBUCKETS(ht)    PR_BIT(PR_HASH_BITS - (ht)->shift)

/* The smallest table has 16 buckets */
#define MINBUCKETSLOG2  4
#define MINBUCKETS      PR_BIT(MINBUCKETSLOG2)

/* Compute the maximum entries given n buckets that we will tolerate, ~90% */
#define OVERLOADED(n)   ((n) - ((n) >> 3))

/* Compute the number of entries below which we shrink the table by half */
#define UNDERLOADED(n)  (((n) > MINBUCKETS) ? ((n) >> 2) : 0)

/*
** Stubs for default hash allocator ops.
*/
static void *
DefaultAllocTable(void *pool, size_t size)
{
    return malloc(size);
}

static void
DefaultFreeTable(void *pool, void *item)
{
    free(item);
}

static PRHashEntry *
DefaultAllocEntry(void *pool, const void *key)
{
    return malloc(sizeof(PRHashEntry));
}

static void
DefaultFreeEntry(void *pool, PRHashEntry *he, uintN flag)
{
    if (flag == HT_FREE_ENTRY)
        free(he);
}

static PRHashAllocOps defaultHashAllocOps = {
    DefaultAllocTable, DefaultFreeTable,
    DefaultAllocEntry, DefaultFreeEntry
};

PR_PUBLIC_API(PRHashTable *)
PR_NewHashTable(uint32 n, PRHashFunction keyHash,
                PRHashComparator keyCompare, PRHashComparator valueCompare,
                PRHashAllocOps *allocOps, void *allocPriv)
{
    PRHashTable *ht;
    size_t nb;

    if (n <= MINBUCKETS) {
        n = MINBUCKETSLOG2;
    } else {
        n = PR_CeilingLog2(n);
        if ((int32)n < 0)
            return NULL;
    }

    if (!allocOps) allocOps = &defaultHashAllocOps;

    ht = (*allocOps->allocTable)(allocPriv, sizeof *ht);
    if (!ht)
	return NULL;
    memset(ht, 0, sizeof *ht);
    ht->shift = PR_HASH_BITS - n;
    n = PR_BIT(n);
#if defined(XP_PC) && defined _MSC_VER && _MSC_VER <= 800
    if (n > 16000) {
        (*allocOps->freeTable)(allocPriv, ht);
        return NULL;
    }
#endif  /* WIN16 */
    nb = n * sizeof(PRHashEntry *);
    ht->buckets = (*allocOps->allocTable)(allocPriv, nb);
    if (!ht->buckets) {
        (*allocOps->freeTable)(allocPriv, ht);
        return NULL;
    }
    memset(ht->buckets, 0, nb);

    ht->keyHash = keyHash;
    ht->keyCompare = keyCompare;
    ht->valueCompare = valueCompare;
    ht->allocOps = allocOps;
    ht->allocPriv = allocPriv;
    return ht;
}

PR_PUBLIC_API(void)
PR_HashTableDestroy(PRHashTable *ht)
{
    uint32 i, n;
    PRHashEntry *he, *next;
    PRHashAllocOps *allocOps = ht->allocOps;
    void *allocPriv = ht->allocPriv;

    n = NBUCKETS(ht);
    for (i = 0; i < n; i++) {
        for (he = ht->buckets[i]; he; he = next) {
            next = he->next;
            (*allocOps->freeEntry)(allocPriv, he, HT_FREE_ENTRY);
        }
    }
#ifdef DEBUG
    memset(ht->buckets, 0xDB, n * sizeof ht->buckets[0]);
#endif
    (*allocOps->freeTable)(allocPriv, ht->buckets);
#ifdef DEBUG
    memset(ht, 0xDB, sizeof *ht);
#endif
    (*allocOps->freeTable)(allocPriv, ht);
}

/*
** Multiplicative hash, from Knuth 6.4.
*/
PR_PUBLIC_API(PRHashEntry **)
PR_HashTableRawLookup(PRHashTable *ht, prhashcode keyHash, const void *key)
{
    PRHashEntry *he, **hep, **hep0;
    prhashcode h;

#ifdef HASHMETER
    ht->nlookups++;
#endif
    h = keyHash * PR_GOLDEN_RATIO;
    h >>= ht->shift;
    hep = hep0 = &ht->buckets[h];
    while ((he = *hep) != NULL) {
        if (he->keyHash == keyHash && (*ht->keyCompare)(key, he->key)) {
            /* Move to front of chain if not already there */
            if (hep != hep0) {
                *hep = he->next;
                he->next = *hep0;
                *hep0 = he;
            }
            return hep0;
        }
        hep = &he->next;
#ifdef HASHMETER
        ht->nsteps++;
#endif
    }
    return hep;
}

PR_PUBLIC_API(PRHashEntry *)
PR_HashTableRawAdd(PRHashTable *ht, PRHashEntry **hep,
                   prhashcode keyHash, const void *key, void *value)
{
    uint32 i, n;
    PRHashEntry *he, *next, **oldbuckets;
    size_t nb;

    /* Grow the table if it is overloaded */
    n = NBUCKETS(ht);
    if (ht->nentries >= OVERLOADED(n)) {
#ifdef HASHMETER
        ht->ngrows++;
#endif
        ht->shift--;
        oldbuckets = ht->buckets;
#if defined(XP_PC) && defined _MSC_VER && _MSC_VER <= 800
        if (2 * n > 16000)
            return NULL;
#endif  /* WIN16 */
        nb = 2 * n * sizeof(PRHashEntry *);
        ht->buckets = (*ht->allocOps->allocTable)(ht->allocPriv, nb);
        if (!ht->buckets) {
            ht->buckets = oldbuckets;
            return NULL;
	}
        memset(ht->buckets, 0, nb);

        for (i = 0; i < n; i++) {
            for (he = oldbuckets[i]; he; he = next) {
                next = he->next;
                hep = PR_HashTableRawLookup(ht, he->keyHash, he->key);
                PR_ASSERT(*hep == NULL);
                he->next = NULL;
                *hep = he;
            }
        }
#ifdef DEBUG
        memset(oldbuckets, 0xDB, n * sizeof oldbuckets[0]);
#endif
        (*ht->allocOps->freeTable)(ht->allocPriv, oldbuckets);
        hep = PR_HashTableRawLookup(ht, keyHash, key);
    }

    /* Make a new key value entry */
    he = (*ht->allocOps->allocEntry)(ht->allocPriv, key);
    if (!he)
	return NULL;
    he->keyHash = keyHash;
    he->key = key;
    he->value = value;
    he->next = *hep;
    *hep = he;
    ht->nentries++;
    return he;
}

PR_PUBLIC_API(PRHashEntry *)
PR_HashTableAdd(PRHashTable *ht, const void *key, void *value)
{
    prhashcode keyHash;
    PRHashEntry *he, **hep;

    keyHash = (*ht->keyHash)(key);
    hep = PR_HashTableRawLookup(ht, keyHash, key);
    if ((he = *hep) != NULL) {
        /* Hit; see if values match */
        if ((*ht->valueCompare)(he->value, value)) {
            /* key,value pair is already present in table */
            return he;
        }
        if (he->value)
            (*ht->allocOps->freeEntry)(ht->allocPriv, he, HT_FREE_VALUE);
        he->value = value;
        return he;
    }
    return PR_HashTableRawAdd(ht, hep, keyHash, key, value);
}

PR_PUBLIC_API(void)
PR_HashTableRawRemove(PRHashTable *ht, PRHashEntry **hep, PRHashEntry *he)
{
    uint32 i, n;
    PRHashEntry *next, **oldbuckets;
    size_t nb;

    *hep = he->next;
    (*ht->allocOps->freeEntry)(ht->allocPriv, he, HT_FREE_ENTRY);

    /* Shrink table if it's underloaded */
    n = NBUCKETS(ht);
    if (--ht->nentries < UNDERLOADED(n)) {
#ifdef HASHMETER
        ht->nshrinks++;
#endif
        ht->shift++;
        oldbuckets = ht->buckets;
        nb = n * sizeof(PRHashEntry*) / 2;
        ht->buckets = (*ht->allocOps->allocTable)(ht->allocPriv, nb);
        if (!ht->buckets) {
            ht->buckets = oldbuckets;
            return;
        }
        memset(ht->buckets, 0, nb);

        for (i = 0; i < n; i++) {
            for (he = oldbuckets[i]; he; he = next) {
                next = he->next;
                hep = PR_HashTableRawLookup(ht, he->keyHash, he->key);
                PR_ASSERT(*hep == NULL);
                he->next = NULL;
                *hep = he;
            }
        }
#ifdef DEBUG
        memset(oldbuckets, 0xDB, n * sizeof oldbuckets[0]);
#endif
        (*ht->allocOps->freeTable)(ht->allocPriv, oldbuckets);
    }
}

PR_PUBLIC_API(PRBool)
PR_HashTableRemove(PRHashTable *ht, const void *key)
{
    prhashcode keyHash;
    PRHashEntry *he, **hep;

    keyHash = (*ht->keyHash)(key);
    hep = PR_HashTableRawLookup(ht, keyHash, key);
    if ((he = *hep) == NULL)
        return PR_FALSE;

    /* Hit; remove element */
    PR_HashTableRawRemove(ht, hep, he);
    return PR_TRUE;
}

PR_PUBLIC_API(void *)
PR_HashTableLookup(PRHashTable *ht, const void *key)
{
    prhashcode keyHash;
    PRHashEntry *he, **hep;

    keyHash = (*ht->keyHash)(key);
    hep = PR_HashTableRawLookup(ht, keyHash, key);
    if ((he = *hep) != NULL) {
        return he->value;
    }
    return NULL;
}

/*
** Iterate over the entries in the hash table calling func for each
** entry found. Stop if "f" says to (return value & PR_ENUMERATE_STOP).
** Return a count of the number of elements scanned.
*/
PR_PUBLIC_API(int)
PR_HashTableEnumerateEntries(PRHashTable *ht, PRHashEnumerator f, void *arg)
{
    PRHashEntry *he, **hep;
    uint32 i, nbuckets;
    int rv, n = 0;
    PRHashEntry *todo = NULL;

    nbuckets = NBUCKETS(ht);
    for (i = 0; i < nbuckets; i++) {
        hep = &ht->buckets[i];
        while ((he = *hep) != NULL) {
            rv = (*f)(he, n, arg);
            n++;
            if (rv & (HT_ENUMERATE_REMOVE | HT_ENUMERATE_UNHASH)) {
                *hep = he->next;
                if (rv & HT_ENUMERATE_REMOVE) {
                    he->next = todo;
                    todo = he;
                }
            } else {
                hep = &he->next;
            }
            if (rv & HT_ENUMERATE_STOP) {
                goto out;
            }
        }
    }

out:
    hep = &todo;
    while ((he = *hep) != NULL) {
        PR_HashTableRawRemove(ht, hep, he);
    }
    return n;
}

#ifdef HASHMETER
#include <math.h>
#include <stdio.h>

PR_PUBLIC_API(void)
PR_HashTableDumpMeter(PRHashTable *ht, PRHashEnumerator dump, FILE *fp)
{
    double mean, variance;
    uint32 nchains, nbuckets;
    uint32 i, n, maxChain, maxChainLen;
    PRHashEntry *he;

    variance = 0;
    nchains = 0;
    maxChainLen = 0;
    nbuckets = NBUCKETS(ht);
    for (i = 0; i < nbuckets; i++) {
        he = ht->buckets[i];
        if (!he)
            continue;
        nchains++;
        for (n = 0; he; he = he->next)
            n++;
        variance += n * n;
        if (n > maxChainLen) {
            maxChainLen = n;
            maxChain = i;
        }
    }
    mean = (double)ht->nentries / nchains;
    variance = fabs(variance / nchains - mean * mean);

    fprintf(fp, "\nHash table statistics:\n");
    fprintf(fp, "     number of lookups: %u\n", ht->nlookups);
    fprintf(fp, "     number of entries: %u\n", ht->nentries);
    fprintf(fp, "       number of grows: %u\n", ht->ngrows);
    fprintf(fp, "     number of shrinks: %u\n", ht->nshrinks);
    fprintf(fp, "   mean steps per hash: %g\n", (double)ht->nsteps
                                                / ht->nlookups);
    fprintf(fp, "mean hash chain length: %g\n", mean);
    fprintf(fp, "    standard deviation: %g\n", sqrt(variance));
    fprintf(fp, " max hash chain length: %u\n", maxChainLen);
    fprintf(fp, "        max hash chain: [%u]\n", maxChain);

    for (he = ht->buckets[maxChain], i = 0; he; he = he->next, i++)
        if ((*dump)(he, i, fp) != HT_ENUMERATE_NEXT)
            break;
}
#endif /* HASHMETER */

PR_PUBLIC_API(int)
PR_HashTableDump(PRHashTable *ht, PRHashEnumerator dump, FILE *fp)
{
    int count;

    count = PR_HashTableEnumerateEntries(ht, dump, fp);
#ifdef HASHMETER
    PR_HashTableDumpMeter(ht, dump, fp);
#endif
    return count;
}

PR_PUBLIC_API(prhashcode)
PR_HashString(const void *key)
{
    prhashcode h;
    const unsigned char *s;

    h = 0;
    for (s = key; *s; s++)
        h = (h >> 28) ^ (h << 4) ^ *s;
    return h;
}

PR_PUBLIC_API(int)
PR_CompareStrings(const void *v1, const void *v2)
{
    return strcmp(v1, v2) == 0;
}

PR_PUBLIC_API(int)
PR_CompareValues(const void *v1, const void *v2)
{
    return v1 == v2;
}

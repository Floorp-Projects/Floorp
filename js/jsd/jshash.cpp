/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * PR hash table package.
 */

#include "jshash.h"

#include "mozilla/MathAlgorithms.h"

#include <stdlib.h>
#include <string.h>

#include "jstypes.h"

#include "js/Utility.h"

using namespace js;

using mozilla::CeilingLog2Size;
using mozilla::RotateLeft;

/* Compute the number of buckets in ht */
#define NBUCKETS(ht)    JS_BIT(JS_HASH_BITS - (ht)->shift)

/* The smallest table has 16 buckets */
#define MINBUCKETSLOG2  4
#define MINBUCKETS      JS_BIT(MINBUCKETSLOG2)

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
    return js_malloc(size);
}

static void
DefaultFreeTable(void *pool, void *item, size_t size)
{
    js_free(item);
}

static JSHashEntry *
DefaultAllocEntry(void *pool, const void *key)
{
    return (JSHashEntry*) js_malloc(sizeof(JSHashEntry));
}

static void
DefaultFreeEntry(void *pool, JSHashEntry *he, unsigned flag)
{
    if (flag == HT_FREE_ENTRY)
        js_free(he);
}

static const JSHashAllocOps defaultHashAllocOps = {
    DefaultAllocTable, DefaultFreeTable,
    DefaultAllocEntry, DefaultFreeEntry
};

JSHashTable *
JS_NewHashTable(uint32_t n, JSHashFunction keyHash,
                JSHashComparator keyCompare, JSHashComparator valueCompare,
                const JSHashAllocOps *allocOps, void *allocPriv)
{
    JSHashTable *ht;
    size_t nb;

    if (n <= MINBUCKETS) {
        n = MINBUCKETSLOG2;
    } else {
        n = CeilingLog2Size(n);
        if (int32_t(n) < 0)
            return nullptr;
    }

    if (!allocOps) allocOps = &defaultHashAllocOps;

    ht = (JSHashTable*) allocOps->allocTable(allocPriv, sizeof *ht);
    if (!ht)
        return nullptr;
    memset(ht, 0, sizeof *ht);
    ht->shift = JS_HASH_BITS - n;
    n = JS_BIT(n);
    nb = n * sizeof(JSHashEntry *);
    ht->buckets = (JSHashEntry**) allocOps->allocTable(allocPriv, nb);
    if (!ht->buckets) {
        allocOps->freeTable(allocPriv, ht, nb);
        return nullptr;
    }
    memset(ht->buckets, 0, nb);

    ht->keyHash = keyHash;
    ht->keyCompare = keyCompare;
    ht->valueCompare = valueCompare;
    ht->allocOps = allocOps;
    ht->allocPriv = allocPriv;
    return ht;
}

void
JS_HashTableDestroy(JSHashTable *ht)
{
    uint32_t i, n;
    JSHashEntry *he, **hep;
    const JSHashAllocOps *allocOps = ht->allocOps;
    void *allocPriv = ht->allocPriv;

    n = NBUCKETS(ht);
    for (i = 0; i < n; i++) {
        hep = &ht->buckets[i];
        while ((he = *hep) != nullptr) {
            *hep = he->next;
            allocOps->freeEntry(allocPriv, he, HT_FREE_ENTRY);
        }
    }
#ifdef DEBUG
    memset(ht->buckets, 0xDB, n * sizeof ht->buckets[0]);
#endif
    allocOps->freeTable(allocPriv, ht->buckets, n * sizeof ht->buckets[0]);
#ifdef DEBUG
    memset(ht, 0xDB, sizeof *ht);
#endif
    allocOps->freeTable(allocPriv, ht, sizeof *ht);
}

/*
 * Multiplicative hash, from Knuth 6.4.
 */
#define BUCKET_HEAD(ht, keyHash)                                              \
    (&(ht)->buckets[((keyHash) * JS_GOLDEN_RATIO) >> (ht)->shift])

JSHashEntry **
JS_HashTableRawLookup(JSHashTable *ht, JSHashNumber keyHash, const void *key)
{
    JSHashEntry *he, **hep, **hep0;

#ifdef JS_HASHMETER
    ht->nlookups++;
#endif
    hep = hep0 = BUCKET_HEAD(ht, keyHash);
    while ((he = *hep) != nullptr) {
        if (he->keyHash == keyHash && ht->keyCompare(key, he->key)) {
            /* Move to front of chain if not already there */
            if (hep != hep0) {
                *hep = he->next;
                he->next = *hep0;
                *hep0 = he;
            }
            return hep0;
        }
        hep = &he->next;
#ifdef JS_HASHMETER
        ht->nsteps++;
#endif
    }
    return hep;
}

static bool
Resize(JSHashTable *ht, uint32_t newshift)
{
    size_t nb, nentries, i;
    JSHashEntry **oldbuckets, *he, *next, **hep;
    size_t nold = NBUCKETS(ht);

    JS_ASSERT(newshift < JS_HASH_BITS);

    nb = (size_t)1 << (JS_HASH_BITS - newshift);

    /* Integer overflow protection. */
    if (nb > (size_t)-1 / sizeof(JSHashEntry*))
        return false;
    nb *= sizeof(JSHashEntry*);

    oldbuckets = ht->buckets;
    ht->buckets = (JSHashEntry**)ht->allocOps->allocTable(ht->allocPriv, nb);
    if (!ht->buckets) {
        ht->buckets = oldbuckets;
        return false;
    }
    memset(ht->buckets, 0, nb);

    ht->shift = newshift;
    nentries = ht->nentries;

    for (i = 0; nentries != 0; i++) {
        for (he = oldbuckets[i]; he; he = next) {
            JS_ASSERT(nentries != 0);
            --nentries;
            next = he->next;
            hep = BUCKET_HEAD(ht, he->keyHash);

            /*
             * We do not require unique entries, instead appending he to the
             * chain starting at hep.
             */
            while (*hep)
                hep = &(*hep)->next;
            he->next = nullptr;
            *hep = he;
        }
    }
#ifdef DEBUG
    memset(oldbuckets, 0xDB, nold * sizeof oldbuckets[0]);
#endif
    ht->allocOps->freeTable(ht->allocPriv, oldbuckets,
                            nold * sizeof oldbuckets[0]);
    return true;
}

JSHashEntry *
JS_HashTableRawAdd(JSHashTable *ht, JSHashEntry **&hep,
                   JSHashNumber keyHash, const void *key, void *value)
{
    uint32_t n;
    JSHashEntry *he;

    /* Grow the table if it is overloaded */
    n = NBUCKETS(ht);
    if (ht->nentries >= OVERLOADED(n)) {
        if (!Resize(ht, ht->shift - 1))
            return nullptr;
#ifdef JS_HASHMETER
        ht->ngrows++;
#endif
        hep = JS_HashTableRawLookup(ht, keyHash, key);
    }

    /* Make a new key value entry */
    he = ht->allocOps->allocEntry(ht->allocPriv, key);
    if (!he)
        return nullptr;
    he->keyHash = keyHash;
    he->key = key;
    he->value = value;
    he->next = *hep;
    *hep = he;
    ht->nentries++;
    return he;
}

JSHashEntry *
JS_HashTableAdd(JSHashTable *ht, const void *key, void *value)
{
    JSHashNumber keyHash;
    JSHashEntry *he, **hep;

    keyHash = ht->keyHash(key);
    hep = JS_HashTableRawLookup(ht, keyHash, key);
    if ((he = *hep) != nullptr) {
        /* Hit; see if values match */
        if (ht->valueCompare(he->value, value)) {
            /* key,value pair is already present in table */
            return he;
        }
        if (he->value)
            ht->allocOps->freeEntry(ht->allocPriv, he, HT_FREE_VALUE);
        he->value = value;
        return he;
    }
    return JS_HashTableRawAdd(ht, hep, keyHash, key, value);
}

void
JS_HashTableRawRemove(JSHashTable *ht, JSHashEntry **hep, JSHashEntry *he)
{
    uint32_t n;

    *hep = he->next;
    ht->allocOps->freeEntry(ht->allocPriv, he, HT_FREE_ENTRY);

    /* Shrink table if it's underloaded */
    n = NBUCKETS(ht);
    if (--ht->nentries < UNDERLOADED(n)) {
        Resize(ht, ht->shift + 1);
#ifdef JS_HASHMETER
        ht->nshrinks++;
#endif
    }
}

bool
JS_HashTableRemove(JSHashTable *ht, const void *key)
{
    JSHashNumber keyHash;
    JSHashEntry *he, **hep;

    keyHash = ht->keyHash(key);
    hep = JS_HashTableRawLookup(ht, keyHash, key);
    if ((he = *hep) == nullptr)
        return false;

    /* Hit; remove element */
    JS_HashTableRawRemove(ht, hep, he);
    return true;
}

void *
JS_HashTableLookup(JSHashTable *ht, const void *key)
{
    JSHashNumber keyHash;
    JSHashEntry *he, **hep;

    keyHash = ht->keyHash(key);
    hep = JS_HashTableRawLookup(ht, keyHash, key);
    if ((he = *hep) != nullptr) {
        return he->value;
    }
    return nullptr;
}

/*
** Iterate over the entries in the hash table calling func for each
** entry found. Stop if "f" says to (return value & JS_ENUMERATE_STOP).
** Return a count of the number of elements scanned.
*/
int
JS_HashTableEnumerateEntries(JSHashTable *ht, JSHashEnumerator f, void *arg)
{
    JSHashEntry *he, **hep, **bucket;
    uint32_t nlimit, n, nbuckets, newlog2;
    int rv;

    nlimit = ht->nentries;
    n = 0;
    for (bucket = ht->buckets; n != nlimit; ++bucket) {
        hep = bucket;
        while ((he = *hep) != nullptr) {
            JS_ASSERT(n < nlimit);
            rv = f(he, n, arg);
            n++;
            if (rv & HT_ENUMERATE_REMOVE) {
                *hep = he->next;
                ht->allocOps->freeEntry(ht->allocPriv, he, HT_FREE_ENTRY);
                --ht->nentries;
            } else {
                hep = &he->next;
            }
            if (rv & HT_ENUMERATE_STOP) {
                goto out;
            }
        }
    }

out:
    /* Shrink table if removal of entries made it underloaded */
    if (ht->nentries != nlimit) {
        JS_ASSERT(ht->nentries < nlimit);
        nbuckets = NBUCKETS(ht);
        if (MINBUCKETS < nbuckets && ht->nentries < UNDERLOADED(nbuckets)) {
            newlog2 = CeilingLog2Size(ht->nentries);
            if (newlog2 < MINBUCKETSLOG2)
                newlog2 = MINBUCKETSLOG2;

            /*  Check that we really shrink the table. */
            JS_ASSERT(JS_HASH_BITS - ht->shift > newlog2);
            Resize(ht, JS_HASH_BITS - newlog2);
        }
    }
    return (int)n;
}

#ifdef JS_HASHMETER
#include <stdio.h>

void
JS_HashTableDumpMeter(JSHashTable *ht, JSHashEnumerator dump, FILE *fp)
{
    double sqsum, mean, sigma;
    uint32_t nchains, nbuckets;
    uint32_t i, n, maxChain, maxChainLen;
    JSHashEntry *he;

    sqsum = 0;
    nchains = 0;
    maxChain = maxChainLen = 0;
    nbuckets = NBUCKETS(ht);
    for (i = 0; i < nbuckets; i++) {
        he = ht->buckets[i];
        if (!he)
            continue;
        nchains++;
        for (n = 0; he; he = he->next)
            n++;
        sqsum += n * n;
        if (n > maxChainLen) {
            maxChainLen = n;
            maxChain = i;
        }
    }

    mean = JS_MeanAndStdDev(nchains, ht->nentries, sqsum, &sigma);

    fprintf(fp, "\nHash table statistics:\n");
    fprintf(fp, "     number of lookups: %u\n", ht->nlookups);
    fprintf(fp, "     number of entries: %u\n", ht->nentries);
    fprintf(fp, "       number of grows: %u\n", ht->ngrows);
    fprintf(fp, "     number of shrinks: %u\n", ht->nshrinks);
    fprintf(fp, "   mean steps per hash: %g\n", (double)ht->nsteps
                                                / ht->nlookups);
    fprintf(fp, "mean hash chain length: %g\n", mean);
    fprintf(fp, "    standard deviation: %g\n", sigma);
    fprintf(fp, " max hash chain length: %u\n", maxChainLen);
    fprintf(fp, "        max hash chain: [%u]\n", maxChain);

    for (he = ht->buckets[maxChain], i = 0; he; he = he->next, i++)
        if (dump(he, i, fp) != HT_ENUMERATE_NEXT)
            break;
}
#endif /* JS_HASHMETER */

int
JS_HashTableDump(JSHashTable *ht, JSHashEnumerator dump, FILE *fp)
{
    int count;

    count = JS_HashTableEnumerateEntries(ht, dump, fp);
#ifdef JS_HASHMETER
    JS_HashTableDumpMeter(ht, dump, fp);
#endif
    return count;
}

JSHashNumber
JS_HashString(const void *key)
{
    JSHashNumber h;
    const unsigned char *s;

    h = 0;
    for (s = (const unsigned char *)key; *s; s++)
        h = RotateLeft(h, 4) ^ *s;
    return h;
}

int
JS_CompareValues(const void *v1, const void *v2)
{
    return v1 == v2;
}

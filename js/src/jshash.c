/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

/*
 * PR hash table package.
 */
#include "jsstddef.h"
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsbit.h"
#include "jsutil.h" /* Added by JSIFY */
#include "jshash.h" /* Added by JSIFY */

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
    return malloc(size);
}

static void
DefaultFreeTable(void *pool, void *item)
{
    free(item);
}

static JSHashEntry *
DefaultAllocEntry(void *pool, const void *key)
{
    return (JSHashEntry*) malloc(sizeof(JSHashEntry));
}

static void
DefaultFreeEntry(void *pool, JSHashEntry *he, uintN flag)
{
    if (flag == HT_FREE_ENTRY)
        free(he);
}

static JSHashAllocOps defaultHashAllocOps = {
    DefaultAllocTable, DefaultFreeTable,
    DefaultAllocEntry, DefaultFreeEntry
};

JS_PUBLIC_API(JSHashTable *)
JS_NewHashTable(uint32 n, JSHashFunction keyHash,
                JSHashComparator keyCompare, JSHashComparator valueCompare,
                JSHashAllocOps *allocOps, void *allocPriv)
{
    JSHashTable *ht;
    size_t nb;

    if (n <= MINBUCKETS) {
        n = MINBUCKETSLOG2;
    } else {
        n = JS_CeilingLog2(n);
        if ((int32)n < 0)
            return NULL;
    }

    if (!allocOps) allocOps = &defaultHashAllocOps;

    ht = (JSHashTable*) (*allocOps->allocTable)(allocPriv, sizeof *ht);
    if (!ht)
	return NULL;
    memset(ht, 0, sizeof *ht);
    ht->shift = JS_HASH_BITS - n;
    n = JS_BIT(n);
#if defined(XP_PC) && defined _MSC_VER && _MSC_VER <= 800
    if (n > 16000) {
        (*allocOps->freeTable)(allocPriv, ht);
        return NULL;
    }
#endif  /* WIN16 */
    nb = n * sizeof(JSHashEntry *);
    ht->buckets = (JSHashEntry**) (*allocOps->allocTable)(allocPriv, nb);
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

JS_PUBLIC_API(void)
JS_HashTableDestroy(JSHashTable *ht)
{
    uint32 i, n;
    JSHashEntry *he, **hep;
    JSHashAllocOps *allocOps = ht->allocOps;
    void *allocPriv = ht->allocPriv;

    n = NBUCKETS(ht);
    for (i = 0; i < n; i++) {
        hep = &ht->buckets[i];
        while ((he = *hep) != NULL) {
            *hep = he->next;
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
JS_PUBLIC_API(JSHashEntry **)
JS_HashTableRawLookup(JSHashTable *ht, JSHashNumber keyHash, const void *key)
{
    JSHashEntry *he, **hep, **hep0;
    JSHashNumber h;

#ifdef HASHMETER
    ht->nlookups++;
#endif
    h = keyHash * JS_GOLDEN_RATIO;
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

JS_PUBLIC_API(JSHashEntry *)
JS_HashTableRawAdd(JSHashTable *ht, JSHashEntry **hep,
                   JSHashNumber keyHash, const void *key, void *value)
{
    uint32 i, n;
    JSHashEntry *he, *next, **oldbuckets;
    size_t nb;

    /* Grow the table if it is overloaded */
    n = NBUCKETS(ht);
    if (ht->nentries >= OVERLOADED(n)) {
        oldbuckets = ht->buckets;
#if defined(XP_PC) && defined _MSC_VER && _MSC_VER <= 800
        if (2 * n > 16000)
            return NULL;
#endif  /* WIN16 */
        nb = 2 * n * sizeof(JSHashEntry *);
        ht->buckets = (JSHashEntry**) (*ht->allocOps->allocTable)(ht->allocPriv, nb);
        if (!ht->buckets) {
            ht->buckets = oldbuckets;
            return NULL;
	}
        memset(ht->buckets, 0, nb);
#ifdef HASHMETER
        ht->ngrows++;
#endif
        ht->shift--;

        for (i = 0; i < n; i++) {
            for (he = oldbuckets[i]; he; he = next) {
                next = he->next;
                hep = JS_HashTableRawLookup(ht, he->keyHash, he->key);
                JS_ASSERT(*hep == NULL);
                he->next = NULL;
                *hep = he;
            }
        }
#ifdef DEBUG
        memset(oldbuckets, 0xDB, n * sizeof oldbuckets[0]);
#endif
        (*ht->allocOps->freeTable)(ht->allocPriv, oldbuckets);
        hep = JS_HashTableRawLookup(ht, keyHash, key);
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

JS_PUBLIC_API(JSHashEntry *)
JS_HashTableAdd(JSHashTable *ht, const void *key, void *value)
{
    JSHashNumber keyHash;
    JSHashEntry *he, **hep;

    keyHash = (*ht->keyHash)(key);
    hep = JS_HashTableRawLookup(ht, keyHash, key);
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
    return JS_HashTableRawAdd(ht, hep, keyHash, key, value);
}

JS_PUBLIC_API(void)
JS_HashTableRawRemove(JSHashTable *ht, JSHashEntry **hep, JSHashEntry *he)
{
    uint32 i, n;
    JSHashEntry *next, **oldbuckets;
    size_t nb;

    *hep = he->next;
    (*ht->allocOps->freeEntry)(ht->allocPriv, he, HT_FREE_ENTRY);

    /* Shrink table if it's underloaded */
    n = NBUCKETS(ht);
    if (--ht->nentries < UNDERLOADED(n)) {
        oldbuckets = ht->buckets;
        nb = n * sizeof(JSHashEntry*) / 2;
        ht->buckets = (JSHashEntry**) (*ht->allocOps->allocTable)(ht->allocPriv, nb);
        if (!ht->buckets) {
            ht->buckets = oldbuckets;
            return;
        }
        memset(ht->buckets, 0, nb);
#ifdef HASHMETER
        ht->nshrinks++;
#endif
        ht->shift++;

        for (i = 0; i < n; i++) {
            for (he = oldbuckets[i]; he; he = next) {
                next = he->next;
                hep = JS_HashTableRawLookup(ht, he->keyHash, he->key);
                JS_ASSERT(*hep == NULL);
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

JS_PUBLIC_API(JSBool)
JS_HashTableRemove(JSHashTable *ht, const void *key)
{
    JSHashNumber keyHash;
    JSHashEntry *he, **hep;

    keyHash = (*ht->keyHash)(key);
    hep = JS_HashTableRawLookup(ht, keyHash, key);
    if ((he = *hep) == NULL)
        return JS_FALSE;

    /* Hit; remove element */
    JS_HashTableRawRemove(ht, hep, he);
    return JS_TRUE;
}

JS_PUBLIC_API(void *)
JS_HashTableLookup(JSHashTable *ht, const void *key)
{
    JSHashNumber keyHash;
    JSHashEntry *he, **hep;

    keyHash = (*ht->keyHash)(key);
    hep = JS_HashTableRawLookup(ht, keyHash, key);
    if ((he = *hep) != NULL) {
        return he->value;
    }
    return NULL;
}

/*
** Iterate over the entries in the hash table calling func for each
** entry found. Stop if "f" says to (return value & JS_ENUMERATE_STOP).
** Return a count of the number of elements scanned.
*/
JS_PUBLIC_API(int)
JS_HashTableEnumerateEntries(JSHashTable *ht, JSHashEnumerator f, void *arg)
{
    JSHashEntry *he, **hep;
    uint32 i, nbuckets;
    int rv, n = 0;
    JSHashEntry *todo = NULL;

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
        JS_HashTableRawRemove(ht, hep, he);
    }
    return n;
}

#ifdef HASHMETER
#include <math.h>
#include <stdio.h>

JS_PUBLIC_API(void)
JS_HashTableDumpMeter(JSHashTable *ht, JSHashEnumerator dump, FILE *fp)
{
    double sqsum, mean, variance, sigma;
    uint32 nchains, nbuckets, nentries;
    uint32 i, n, maxChain, maxChainLen;
    JSHashEntry *he;

    sqsum = 0;
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
        sqsum += n * n;
        if (n > maxChainLen) {
            maxChainLen = n;
            maxChain = i;
        }
    }
    nentries = ht->nentries;
    mean = (double)nentries / nchains;
    variance = nchains * sqsum - nentries * nentries;
    if (variance < 0 || nchains == 1)
        variance = 0;
    else
        variance /= nchains * (nchains - 1);
    sigma = sqrt(variance);

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
        if ((*dump)(he, i, fp) != HT_ENUMERATE_NEXT)
            break;
}
#endif /* HASHMETER */

JS_PUBLIC_API(int)
JS_HashTableDump(JSHashTable *ht, JSHashEnumerator dump, FILE *fp)
{
    int count;

    count = JS_HashTableEnumerateEntries(ht, dump, fp);
#ifdef HASHMETER
    JS_HashTableDumpMeter(ht, dump, fp);
#endif
    return count;
}

JS_PUBLIC_API(JSHashNumber)
JS_HashString(const void *key)
{
    JSHashNumber h;
    const unsigned char *s;

    h = 0;
    for (s = (const unsigned char *)key; *s; s++)
        h = (h >> (JS_HASH_BITS - 4)) ^ (h << 4) ^ *s;
    return h;
}

JS_PUBLIC_API(int)
JS_CompareValues(const void *v1, const void *v2)
{
    return v1 == v2;
}

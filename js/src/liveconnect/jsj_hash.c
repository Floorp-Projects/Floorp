/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
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
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
/*
 * This is a copy of the NSPR hash-table library, but it has been slightly
 * modified to allow an additional argument to be passed into the hash
 * key-comparison function.  This is used to maintain thread-safety by
 * passing in a JNIEnv pointer to the key-comparison function rather
 * than storing it in a global.  All types,function names, etc. have
 * been renamed from their original NSPR names to protect the innocent.
 */

#include <stdlib.h>
#include <string.h>

#include "jsj_hash.h"
#include "jstypes.h"
#include "jsutil.h"
#include "jsbit.h"


/* Compute the number of buckets in ht */
#define NBUCKETS(ht)    (1 << (JSJ_HASH_BITS - (ht)->shift))

/* The smallest table has 16 buckets */
#define MINBUCKETSLOG2  4
#define MINBUCKETS      (1 << MINBUCKETSLOG2)

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

static JSJHashEntry *
DefaultAllocEntry(void *pool, const void *key)
{
    return malloc(sizeof(JSJHashEntry));
}

static void
DefaultFreeEntry(void *pool, JSJHashEntry *he, JSUintn flag)
{
    if (flag == HT_FREE_ENTRY)
        free(he);
}

static JSJHashAllocOps defaultHashAllocOps = {
    DefaultAllocTable, DefaultFreeTable,
    DefaultAllocEntry, DefaultFreeEntry
};

JS_EXPORT_API(JSJHashTable *)
JSJ_NewHashTable(JSUint32 n, JSJHashFunction keyHash,
                JSJHashComparator keyCompare, JSJHashComparator valueCompare,
                JSJHashAllocOps *allocOps, void *allocPriv)
{
    JSJHashTable *ht;
    JSUint32 nb;

    if (n <= MINBUCKETS) {
        n = MINBUCKETSLOG2;
    } else {
        n = JS_CeilingLog2(n);
        if ((JSInt32)n < 0)
            return 0;
    }

    if (!allocOps) allocOps = &defaultHashAllocOps;

    ht = (*allocOps->allocTable)(allocPriv, sizeof *ht);
    if (!ht)
    return 0;
    memset(ht, 0, sizeof *ht);
    ht->shift = JSJ_HASH_BITS - n;
    n = 1 << n;
#if (defined(XP_WIN) || defined(XP_OS2)) && !defined(_WIN32)
    if (n > 16000) {
        (*allocOps->freeTable)(allocPriv, ht);
        return 0;
    }
#endif  /* WIN16 */
    nb = n * sizeof(JSJHashEntry *);
    ht->buckets = (*allocOps->allocTable)(allocPriv, nb);
    if (!ht->buckets) {
        (*allocOps->freeTable)(allocPriv, ht);
        return 0;
    }
    memset(ht->buckets, 0, nb);

    ht->keyHash = keyHash;
    ht->keyCompare = keyCompare;
    ht->valueCompare = valueCompare;
    ht->allocOps = allocOps;
    ht->allocPriv = allocPriv;
    return ht;
}

JS_EXPORT_API(void)
JSJ_HashTableDestroy(JSJHashTable *ht)
{
    JSUint32 i, n;
    JSJHashEntry *he, *next;
    JSJHashAllocOps *allocOps = ht->allocOps;
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
#define GOLDEN_RATIO    0x9E3779B9U

JS_EXPORT_API(JSJHashEntry **)
JSJ_HashTableRawLookup(JSJHashTable *ht, JSJHashNumber keyHash, const void *key, void *arg)
{
    JSJHashEntry *he, **hep, **hep0;
    JSJHashNumber h;

#ifdef HASHMETER
    ht->nlookups++;
#endif
    h = keyHash * GOLDEN_RATIO;
    h >>= ht->shift;
    hep = hep0 = &ht->buckets[h];
    while ((he = *hep) != 0) {
        if (he->keyHash == keyHash && (*ht->keyCompare)(key, he->key, arg)) {
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

JS_EXPORT_API(JSJHashEntry *)
JSJ_HashTableRawAdd(JSJHashTable *ht, JSJHashEntry **hep,
                    JSJHashNumber keyHash, const void *key, void *value,
                    void *arg)
{
    JSUint32 i, n;
    JSJHashEntry *he, *next, **oldbuckets;
    JSUint32 nb;

    /* Grow the table if it is overloaded */
    n = NBUCKETS(ht);
    if (ht->nentries >= OVERLOADED(n)) {
#ifdef HASHMETER
        ht->ngrows++;
#endif
        ht->shift--;
        oldbuckets = ht->buckets;
#if (defined(XP_WIN) || defined(XP_OS2)) && !defined(_WIN32)
        if (2 * n > 16000)
            return 0;
#endif  /* WIN16 */
        nb = 2 * n * sizeof(JSJHashEntry *);
        ht->buckets = (*ht->allocOps->allocTable)(ht->allocPriv, nb);
        if (!ht->buckets) {
            ht->buckets = oldbuckets;
            return 0;
        }
        memset(ht->buckets, 0, nb);

        for (i = 0; i < n; i++) {
            for (he = oldbuckets[i]; he; he = next) {
                next = he->next;
                hep = JSJ_HashTableRawLookup(ht, he->keyHash, he->key, arg);
                JS_ASSERT(*hep == 0);
                he->next = 0;
                *hep = he;
            }
        }
#ifdef DEBUG
        memset(oldbuckets, 0xDB, n * sizeof oldbuckets[0]);
#endif
        (*ht->allocOps->freeTable)(ht->allocPriv, oldbuckets);
        hep = JSJ_HashTableRawLookup(ht, keyHash, key, arg);
    }

    /* Make a new key value entry */
    he = (*ht->allocOps->allocEntry)(ht->allocPriv, key);
    if (!he)
    return 0;
    he->keyHash = keyHash;
    he->key = key;
    he->value = value;
    he->next = *hep;
    *hep = he;
    ht->nentries++;
    return he;
}

JS_EXPORT_API(JSJHashEntry *)
JSJ_HashTableAdd(JSJHashTable *ht, const void *key, void *value, void *arg)
{
    JSJHashNumber keyHash;
    JSJHashEntry *he, **hep;

    keyHash = (*ht->keyHash)(key, arg);
    hep = JSJ_HashTableRawLookup(ht, keyHash, key, arg);
    if ((he = *hep) != 0) {
        /* Hit; see if values match */
        if ((*ht->valueCompare)(he->value, value, arg)) {
            /* key,value pair is already present in table */
            return he;
        }
        if (he->value)
            (*ht->allocOps->freeEntry)(ht->allocPriv, he, HT_FREE_VALUE);
        he->value = value;
        return he;
    }
    return JSJ_HashTableRawAdd(ht, hep, keyHash, key, value, arg);
}

JS_EXPORT_API(void)
JSJ_HashTableRawRemove(JSJHashTable *ht, JSJHashEntry **hep, JSJHashEntry *he, void *arg)
{
    JSUint32 i, n;
    JSJHashEntry *next, **oldbuckets;
    JSUint32 nb;

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
        nb = n * sizeof(JSJHashEntry*) / 2;
        ht->buckets = (*ht->allocOps->allocTable)(ht->allocPriv, nb);
        if (!ht->buckets) {
            ht->buckets = oldbuckets;
            return;
        }
        memset(ht->buckets, 0, nb);

        for (i = 0; i < n; i++) {
            for (he = oldbuckets[i]; he; he = next) {
                next = he->next;
                hep = JSJ_HashTableRawLookup(ht, he->keyHash, he->key, arg);
                JS_ASSERT(*hep == 0);
                he->next = 0;
                *hep = he;
            }
        }
#ifdef DEBUG
        memset(oldbuckets, 0xDB, n * sizeof oldbuckets[0]);
#endif
        (*ht->allocOps->freeTable)(ht->allocPriv, oldbuckets);
    }
}

JS_EXPORT_API(JSBool)
JSJ_HashTableRemove(JSJHashTable *ht, const void *key, void *arg)
{
    JSJHashNumber keyHash;
    JSJHashEntry *he, **hep;

    keyHash = (*ht->keyHash)(key, arg);
    hep = JSJ_HashTableRawLookup(ht, keyHash, key, arg);
    if ((he = *hep) == 0)
        return JS_FALSE;

    /* Hit; remove element */
    JSJ_HashTableRawRemove(ht, hep, he, arg);
    return JS_TRUE;
}

JS_EXPORT_API(void *)
JSJ_HashTableLookup(JSJHashTable *ht, const void *key, void *arg)
{
    JSJHashNumber keyHash;
    JSJHashEntry *he, **hep;

    keyHash = (*ht->keyHash)(key, arg);
    hep = JSJ_HashTableRawLookup(ht, keyHash, key, arg);
    if ((he = *hep) != 0) {
        return he->value;
    }
    return 0;
}

/*
** Iterate over the entries in the hash table calling func for each
** entry found. Stop if "f" says to (return value & JS_ENUMERATE_STOP).
** Return a count of the number of elements scanned.
*/
JS_EXPORT_API(int)
JSJ_HashTableEnumerateEntries(JSJHashTable *ht, JSJHashEnumerator f, void *arg)
{
    JSJHashEntry *he, **hep;
    JSUint32 i, nbuckets;
    int rv, n = 0;
    JSJHashEntry *todo = 0;

    nbuckets = NBUCKETS(ht);
    for (i = 0; i < nbuckets; i++) {
        hep = &ht->buckets[i];
        while ((he = *hep) != 0) {
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
    while ((he = *hep) != 0) {
        JSJ_HashTableRawRemove(ht, hep, he, arg);
    }
    return n;
}

#ifdef HASHMETER
#include <math.h>
#include <stdio.h>

JS_EXPORT_API(void)
JSJ_HashTableDumpMeter(JSJHashTable *ht, JSJHashEnumerator dump, FILE *fp)
{
    double mean, variance;
    JSUint32 nchains, nbuckets;
    JSUint32 i, n, maxChain, maxChainLen;
    JSJHashEntry *he;

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

JS_EXPORT_API(int)
JSJ_HashTableDump(JSJHashTable *ht, JSJHashEnumerator dump, FILE *fp)
{
    int count;

    count = JSJ_HashTableEnumerateEntries(ht, dump, fp);
#ifdef HASHMETER
    JSJ_HashTableDumpMeter(ht, dump, fp);
#endif
    return count;
}

JS_EXPORT_API(JSJHashNumber)
JSJ_HashString(const void *key)
{
    JSJHashNumber h;
    const unsigned char *s;

    h = 0;
    for (s = key; *s; s++)
        h = (h >> 28) ^ (h << 4) ^ *s;
    return h;
}

JS_EXPORT_API(int)
JSJ_CompareStrings(const void *v1, const void *v2)
{
    return strcmp(v1, v2) == 0;
}

JS_EXPORT_API(int)
JSJ_CompareValues(const void *v1, const void *v2)
{
    return v1 == v2;
}

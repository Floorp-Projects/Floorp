/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla JavaScript code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999,2000 Netscape Communications Corporation.
 * All Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/*
 * Double hashing implementation.
 */
#include <stdlib.h>
#include <string.h>
#include "jsbit.h"
#include "jsdhash.h"
#include "jsutil.h"     /* for JS_ASSERT */

#ifdef JS_DHASHMETER
# define METER(x)       x
#else
# define METER(x)       /* nothing */
#endif

JS_PUBLIC_API(void *)
JS_DHashAllocTable(JSDHashTable *table, uint32 nbytes)
{
    return malloc(nbytes);
}

JS_PUBLIC_API(void)
JS_DHashFreeTable(JSDHashTable *table, void *ptr)
{
    free(ptr);
}

JS_PUBLIC_API(JSDHashNumber)
JS_DHashStringKey(JSDHashTable *table, const void *key)
{
    const char *s;
    size_t n, m;
    JSDHashNumber h;

    s = key;
    n = strlen(s);
    h = 0;
    if (n < 16) {
	/* Hash every char in a short string. */
	for (; n; s++, n--)
	    h = (h >> 28) ^ (h << 4) ^ *s;
    } else {
	/* Sample a la java.lang.String.hash(). */
	for (m = n / 8; n >= m; s += m, n -= m)
	    h = (h >> 28) ^ (h << 4) ^ *s;
    }
    return h;
}

JS_PUBLIC_API(const void *)
JS_DHashGetKeyStub(JSDHashTable *table, JSDHashEntryHdr *entry)
{
    JSDHashEntryStub *stub = (JSDHashEntryStub *)entry;
    
    return stub->key;
}

JS_PUBLIC_API(JSDHashNumber)
JS_DHashVoidPtrKeyStub(JSDHashTable *table, const void *key)
{
    return (JSDHashNumber)key >> 2;
}

JS_PUBLIC_API(JSBool)
JS_DHashMatchEntryStub(JSDHashTable *table,
                       const JSDHashEntryHdr *entry,
                       const void *key)
{
    JSDHashEntryStub *stub = (JSDHashEntryStub *)entry;

    return stub->key == key;
}

JS_PUBLIC_API(void)
JS_DHashMoveEntryStub(JSDHashTable *table,
                      const JSDHashEntryHdr *from,
                      JSDHashEntryHdr *to)
{
    memcpy(to, from, table->entrySize);
}

JS_PUBLIC_API(void)
JS_DHashClearEntryStub(JSDHashTable *table, JSDHashEntryHdr *entry)
{
    memset(entry, 0, table->entrySize);
}

JS_PUBLIC_API(void)
JS_DHashFinalizeStub(JSDHashTable *table)
{
}

static JSDHashTableOps stub_ops = {
    JS_DHashAllocTable,
    JS_DHashFreeTable,
    JS_DHashGetKeyStub,
    JS_DHashVoidPtrKeyStub,
    JS_DHashMatchEntryStub,
    JS_DHashMoveEntryStub,
    JS_DHashClearEntryStub,
    JS_DHashFinalizeStub
};

JS_PUBLIC_API(JSDHashTableOps *)
JS_DHashGetStubOps(void)
{
    return &stub_ops;
}

JS_PUBLIC_API(JSDHashTable *)
JS_NewDHashTable(JSDHashTableOps *ops, void *data, uint32 entrySize,
                 uint32 capacity)
{
    JSDHashTable *table;

    table = (JSDHashTable *) malloc(sizeof *table);
    if (!table)
        return NULL;
    if (!JS_DHashTableInit(table, ops, data, entrySize, capacity)) {
        free(table);
        return NULL;
    }
    return table;
}

JS_PUBLIC_API(void)
JS_DHashTableDestroy(JSDHashTable *table)
{
    JS_DHashTableFinish(table);
    free(table);
}

JS_PUBLIC_API(JSBool)
JS_DHashTableInit(JSDHashTable *table, JSDHashTableOps *ops, void *data,
                  uint32 entrySize, uint32 capacity)
{
    int log2;
    uint32 nbytes;

    table->ops = ops;
    table->data = data;
    if (capacity < JS_DHASH_MIN_SIZE)
        capacity = JS_DHASH_MIN_SIZE;
    log2 = JS_CeilingLog2(capacity);
    table->hashShift = JS_DHASH_BITS - log2;
    table->sizeLog2 = log2;
    table->sizeMask = JS_BITMASK(table->sizeLog2);
    table->entrySize = entrySize;
    table->entryCount = table->removedCount = 0;
    nbytes = capacity * entrySize;
    table->entryStore = ops->allocTable(table, nbytes);
    if (!table->entryStore)
        return JS_FALSE;
    memset(table->entryStore, 0, nbytes);
    METER(memset(&table->stats, 0, sizeof table->stats));
    return JS_TRUE;
}

JS_PUBLIC_API(void)
JS_DHashTableFinish(JSDHashTable *table)
{
    table->ops->finalize(table);
    table->ops->freeTable(table, table->entryStore);
}

/*
 * Double hashing needs the second hash code to be relatively prime to table
 * size, so we simply make hash2 odd.
 */
#define HASH1(hash0, shift)         ((hash0) >> (shift))
#define HASH2(hash0,log2,shift)     ((((hash0) << (log2)) >> (shift)) | 1)

/* Reserve keyHash 0 for free entries and 1 for removed-entry sentinels. */
#define MARK_ENTRY_FREE(entry)      ((entry)->keyHash = 0)
#define MARK_ENTRY_REMOVED(entry)   ((entry)->keyHash = 1)
#define ENTRY_IS_LIVE(entry)        ((entry)->keyHash >= 2)
#define ENSURE_LIVE_KEYHASH(hash0)  if (hash0 < 2) hash0 -= 2; else (void)0

/* Compute the address of the indexed entry in table. */
#define ADDRESS_ENTRY(table, index) \
    ((JSDHashEntryHdr *)((table)->entryStore + (index) * (table)->entrySize))

static JSDHashEntryHdr *
SearchTable(JSDHashTable *table, const void *key, JSDHashNumber keyHash)
{
    JSDHashNumber hash1, hash2;
    int hashShift;
    JSDHashEntryHdr *entry;
    JSDHashMatchEntry matchEntry;

    METER(table->stats.searches++);

    /* Compute the primary hash address. */
    hashShift = table->hashShift;
    hash1 = HASH1(keyHash, hashShift);
    entry = ADDRESS_ENTRY(table, hash1);

    /* Miss: return space for a new entry. */
    if (JS_DHASH_ENTRY_IS_FREE(entry)) {
        METER(table->stats.misses++);
        return entry;
    }

    /* Hit: return entry. */
    matchEntry = table->ops->matchEntry;
    if (entry->keyHash == keyHash && matchEntry(table, entry, key)) {
        METER(table->stats.hits++);
        return entry;
    }

    /* Collision: double hash. */
    hash2 = HASH2(keyHash, table->sizeLog2, hashShift);
    do {
        METER(table->stats.steps++);
        hash1 -= hash2;
        hash1 &= table->sizeMask;
        entry = ADDRESS_ENTRY(table, hash1);
        if (JS_DHASH_ENTRY_IS_FREE(entry)) {
            METER(table->stats.misses++);
            return entry;
        }
    } while (entry->keyHash != keyHash || !matchEntry(table, entry, key));

    METER(table->stats.hits++);
    return entry;
}

JS_PUBLIC_API(JSDHashEntryHdr *)
JS_DHashTableOperate(JSDHashTable *table, const void *key, JSDHashOperator op)
{
    int change;
    JSDHashNumber keyHash;
    uint32 i, size, capacity, nbytes, entrySize;
    JSDHashEntryHdr *entry, *oldEntry, *newEntry;
    char *entryStore, *newEntryStore, *entryAddr;
    JSDHashGetKey getKey;
    JSDHashMoveEntry moveEntry;

    /* Usually we don't grow or shrink the table. */
    change = 0;

    /* Avoid 0 and 1 hash codes, they indicate free and deleted entries. */
    keyHash = table->ops->hashKey(table, key);
    ENSURE_LIVE_KEYHASH(keyHash);
    keyHash *= JS_DHASH_GOLDEN_RATIO;
    entry = SearchTable(table, key, keyHash);

    switch (op) {
      case JS_DHASH_LOOKUP:
        METER(table->stats.lookups++);
        break;

      case JS_DHASH_ADD:
        if (JS_DHASH_ENTRY_IS_FREE(entry)) {
            /* Initialize the entry, indicating that it's no longer free. */
            METER(table->stats.addMisses++);
            entry->keyHash = keyHash;
            table->entryCount++;

            /* If alpha is >= .75, set change to trigger table growth below. */
            size = JS_BIT(table->sizeLog2);
            if (table->entryCount + table->removedCount >= size - (size >> 2)) {
                METER(table->stats.grows++);
                change = 1;
                capacity = size << 1;
            }
        }
        METER(else table->stats.addHits++);
        break;

      case JS_DHASH_REMOVE:
        if (JS_DHASH_ENTRY_IS_BUSY(entry)) {
            /* Clear this entry and mark it as "removed". */
            METER(table->stats.removeHits++);
            JS_DHashTableRawRemove(table, entry);

            /* Shrink if alpha is <= .25 and table isn't too small already. */
            size = JS_BIT(table->sizeLog2);
            if (size > JS_DHASH_MIN_SIZE && table->entryCount <= size >> 2) {
                METER(table->stats.shrinks++);
                change = -1;
                capacity = size >> 1;
            }
        }
        METER(else table->stats.removeMisses++);
        entry = NULL;
        break;

      default:
        JS_ASSERT(0);
    }

    if (change) {
        entrySize = table->entrySize;
        nbytes = capacity * entrySize;
        newEntryStore = table->ops->allocTable(table, nbytes);
        if (!newEntryStore) {
            /* If we just grabbed the last free entry, undo and fail hard. */
            if (op == JS_DHASH_ADD &&
                table->entryCount + table->removedCount == size) {
                METER(table->stats.addFailures++);
                MARK_ENTRY_FREE(entry);
                table->entryCount--;
                entry = NULL;
            }
        } else {
            memset(newEntryStore, 0, nbytes);
            entryStore = table->entryStore;
            table->entryStore = newEntryStore;

            table->sizeLog2 += change;
            table->sizeMask = JS_BITMASK(table->sizeLog2);
            table->hashShift = JS_DHASH_BITS - table->sizeLog2;
            table->removedCount = 0;

            getKey = table->ops->getKey;
            moveEntry = table->ops->moveEntry;
            entryAddr = entryStore;
            for (i = 0; i < size; i++) {
                oldEntry = (JSDHashEntryHdr *)entryAddr;
                if (oldEntry != entry && ENTRY_IS_LIVE(oldEntry)) {
                    newEntry = SearchTable(table, getKey(table,oldEntry),
                                           oldEntry->keyHash);
                    JS_ASSERT(JS_DHASH_ENTRY_IS_FREE(newEntry));
                    moveEntry(table, oldEntry, newEntry);
                    newEntry->keyHash = oldEntry->keyHash;
                }
                entryAddr += entrySize;
            }
            table->ops->freeTable(table, entryStore);

            if (op == JS_DHASH_ADD) {
                entry = SearchTable(table, key, keyHash);
                JS_ASSERT(JS_DHASH_ENTRY_IS_FREE(entry));
                entry->keyHash = keyHash;
            }
        }
    }

    return entry;
}

JS_PUBLIC_API(void)
JS_DHashTableRawRemove(JSDHashTable *table, JSDHashEntryHdr *entry)
{
    table->ops->clearEntry(table, entry);
    MARK_ENTRY_REMOVED(entry);
    table->removedCount++;
    table->entryCount--;
}

JS_PUBLIC_API(uint32)
JS_DHashTableEnumerate(JSDHashTable *table, JSDHashEnumerator etor, void *arg)
{
    char *entryAddr;
    uint32 i, j, n, entrySize;
    JSDHashEntryHdr *entry;
    JSDHashOperator op;

    entryAddr = table->entryStore;
    entrySize = table->entrySize;
    n = JS_BIT(table->sizeLog2);
    for (i = j = 0; i < n; i++) {
        entry = (JSDHashEntryHdr *)entryAddr;
        if (ENTRY_IS_LIVE(entry)) {
            op = etor(table, entry, j++, arg);
            if (op & JS_DHASH_REMOVE) {
                METER(table->stats.removeEnums++);
                JS_DHashTableRawRemove(table, entry);
            }
            if (op & JS_DHASH_STOP)
                break;
        }
        entryAddr += entrySize;
    }
    return j;
}

#ifdef JS_DHASHMETER
#include <math.h>
#include <stdio.h>

JS_PUBLIC_API(void)
JS_DHashTableDumpMeter(JSDHashTable *table, JSDHashEnumerator dump, FILE *fp)
{
    char *entryAddr;
    uint32 entrySize, entryCount;
    uint32 i, tableSize, chainLen, maxChainLen, chainCount;
    JSDHashNumber hash1, hash2, saveHash1, maxChainHash1, maxChainHash2;
    double sqsum, mean, variance, sigma;
    JSDHashEntryHdr *entry, *probe;

    entryAddr = table->entryStore;
    entrySize = table->entrySize;
    tableSize = JS_BIT(table->sizeLog2);
    chainCount = maxChainLen = 0;
    hash2 = 0;
    sqsum = 0;

    for (i = 0; i < tableSize; i++) {
        entry = (JSDHashEntryHdr *)entryAddr;
        entryAddr += entrySize;
        if (!ENTRY_IS_LIVE(entry))
            continue;
        hash1 = saveHash1 = HASH1(entry->keyHash, table->hashShift);
        probe = ADDRESS_ENTRY(table, hash1);
        chainLen = 1;
        if (probe == entry) {
            /* Start of a (possibly unit-length) chain. */
            chainCount++;
        } else {
            hash2 = HASH2(entry->keyHash, table->sizeLog2, table->hashShift);
            do {
                chainLen++;
                hash1 -= hash2;
                hash1 &= table->sizeMask;
                probe = ADDRESS_ENTRY(table, hash1);
            } while (probe != entry);
        }
        sqsum += chainLen * chainLen;
        if (chainLen > maxChainLen) {
            maxChainLen = chainLen;
            maxChainHash1 = saveHash1;
            maxChainHash2 = hash2;
        }
    }

    entryCount = table->entryCount;
    mean = (double)entryCount / chainCount;
    variance = chainCount * sqsum - entryCount * entryCount;
    if (variance < 0 || chainCount == 1)
        variance = 0;
    else
        variance /= chainCount * (chainCount - 1);
    sigma = sqrt(variance);

    fprintf(fp, "Double hashing statistics:\n");
    fprintf(fp, "    table size (in entries): %u\n", tableSize);
    fprintf(fp, "          number of entries: %u\n", table->entryCount);
    fprintf(fp, "  number of removed entries: %u\n", table->removedCount);
    fprintf(fp, "         number of searches: %u\n", table->stats.searches);
    fprintf(fp, "             number of hits: %u\n", table->stats.hits);
    fprintf(fp, "           number of misses: %u\n", table->stats.misses);
    fprintf(fp, "      mean steps per search: %g\n", (double)table->stats.steps
                                                     / table->stats.searches);
    fprintf(fp, "     mean hash chain length: %g\n", mean);
    fprintf(fp, "         standard deviation: %g\n", sigma);
    fprintf(fp, "  maximum hash chain length: %u\n", maxChainLen);
    fprintf(fp, "          number of lookups: %u\n", table->stats.lookups);
    fprintf(fp, " adds that made a new entry: %u\n", table->stats.addMisses);
    fprintf(fp, "   adds that found an entry: %u\n", table->stats.addHits);
    fprintf(fp, "               add failures: %u\n", table->stats.addFailures);
    fprintf(fp, "             useful removes: %u\n", table->stats.removeHits);
    fprintf(fp, "            useless removes: %u\n", table->stats.removeMisses);
    fprintf(fp, "  removes while enumerating: %u\n", table->stats.removeEnums);
    fprintf(fp, "            number of grows: %u\n", table->stats.grows);
    fprintf(fp, "          number of shrinks: %u\n", table->stats.shrinks);

    if (maxChainLen && hash2) {
        fputs("Maximum hash chain:\n", fp);
        hash1 = maxChainHash1;
        hash2 = maxChainHash2;
        entry = ADDRESS_ENTRY(table, hash1);
        i = 0;
        do {
            if (dump(table, entry, i++, fp) != JS_DHASH_NEXT)
                break;
            hash1 -= hash2;
            hash1 &= table->sizeMask;
            entry = ADDRESS_ENTRY(table, hash1);
        } while (JS_DHASH_ENTRY_IS_BUSY(entry));
    }
}
#endif /* JS_DHASHMETER */

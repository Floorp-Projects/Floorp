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
 * Original Contributor: 
 *   Brendan Eich <brendan@mozilla.org>
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
#include <stdio.h>
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
    JSDHashNumber h;
    const unsigned char *s;

    h = 0;
    for (s = key; *s != '\0'; s++)
        h = (h >> (JS_DHASH_BITS - 4)) ^ (h << 4) ^ *s;
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

#ifdef DEBUG
    if (entrySize > 6 * sizeof(void *)) {
        fprintf(stderr,
                "jsdhash: for the table at address 0x%p, the given entrySize"
                " of %lu %s favors chaining over double hashing.\n",
                table,
                (unsigned long) entrySize,
                (entrySize > 16 * sizeof(void*)) ? "definitely" : "probably");
    }
#endif

    table->ops = ops;
    table->data = data;
    if (capacity < JS_DHASH_MIN_SIZE)
        capacity = JS_DHASH_MIN_SIZE;
    log2 = JS_CeilingLog2(capacity);
    capacity = JS_BIT(log2);
    table->hashShift = JS_DHASH_BITS - log2;
    table->sizeLog2 = log2;
    table->sizeMask = JS_BITMASK(log2);
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

JS_PUBLIC_API(void)
JS_DHashTableFinish(JSDHashTable *table)
{
    char *entryAddr, *entryLimit;
    uint32 entrySize;

    table->ops->finalize(table);

    /* Clear any remaining live entries. */
    entryAddr = table->entryStore;
    entrySize = table->entrySize;
    entryLimit = entryAddr + JS_BIT(table->sizeLog2) * entrySize;
    while (entryAddr < entryLimit) {
        JSDHashEntryHdr *entry = (JSDHashEntryHdr *)entryAddr;
        if (ENTRY_IS_LIVE(entry)) {
            METER(table->stats.removeEnums++);
            table->ops->clearEntry(table, entry);
        }

        entryAddr += entrySize;
    }

    table->ops->freeTable(table, table->entryStore);
}

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

static JSBool
ChangeTable(JSDHashTable *table, int deltaLog2, JSDHashEntryHdr *skipEntry)
{
    int oldLog2, newLog2;
    uint32 oldCapacity, newCapacity;
    char *newEntryStore, *oldEntryStore, *oldEntryAddr;
    uint32 entrySize, i, nbytes;
    JSDHashEntryHdr *oldEntry, *newEntry;
    JSDHashGetKey getKey;
    JSDHashMoveEntry moveEntry;

    /* Look, but don't touch, until we succeed in getting new entry store. */
    oldLog2 = table->sizeLog2;
    newLog2 = oldLog2 + deltaLog2;
    oldCapacity = JS_BIT(oldLog2);
    newCapacity = JS_BIT(newLog2);
    entrySize = table->entrySize;
    nbytes = newCapacity * entrySize;

    newEntryStore = table->ops->allocTable(table, nbytes);
    if (!newEntryStore)
        return JS_FALSE;

    table->hashShift = JS_DHASH_BITS - newLog2;
    table->sizeLog2 = newLog2;
    table->sizeMask = JS_BITMASK(newLog2);
    table->removedCount = 0;

    memset(newEntryStore, 0, nbytes);
    oldEntryAddr = oldEntryStore = table->entryStore;
    table->entryStore = newEntryStore;
    getKey = table->ops->getKey;
    moveEntry = table->ops->moveEntry;

    /* Copy only live entries, leaving removed ones (and skipEntry) behind. */
    for (i = 0; i < oldCapacity; i++) {
        oldEntry = (JSDHashEntryHdr *)oldEntryAddr;
        if (oldEntry != skipEntry && ENTRY_IS_LIVE(oldEntry)) {
            newEntry = SearchTable(table, getKey(table, oldEntry),
                                   oldEntry->keyHash);
            JS_ASSERT(JS_DHASH_ENTRY_IS_FREE(newEntry));
            moveEntry(table, oldEntry, newEntry);
            newEntry->keyHash = oldEntry->keyHash;
        }
        oldEntryAddr += entrySize;
    }

    table->ops->freeTable(table, oldEntryStore);
    return JS_TRUE;
}

JS_PUBLIC_API(JSDHashEntryHdr *)
JS_DHashTableOperate(JSDHashTable *table, const void *key, JSDHashOperator op)
{
    int biasedDeltaLog2;
    JSDHashNumber keyHash;
    JSDHashEntryHdr *entry;
    uint32 size;

/*
 * Usually we don't grow or shrink the table, so optimize for test-not-zero
 * by biasing the deltaLog2 of -1 (shrink), 0 (compress), or 1 (grow) so that
 * the biased no-change value is 0.
 */
#define DELTA_LOG2_BIAS 2

    biasedDeltaLog2 = 0;

    /* Avoid 0 and 1 hash codes, they indicate free and removed entries. */
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

            /* If alpha is >= .75, set biasedDeltaLog2 to trigger growth. */
            size = JS_BIT(table->sizeLog2);
            if (table->entryCount + table->removedCount >= size - (size >> 2)) {
                if (table->removedCount >= size >> 2) {
                    METER(table->stats.compresses++);
                    biasedDeltaLog2 = 0 + DELTA_LOG2_BIAS;
                } else {
                    METER(table->stats.grows++);
                    biasedDeltaLog2 = 1 + DELTA_LOG2_BIAS;
                }
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
                biasedDeltaLog2 = -1 + DELTA_LOG2_BIAS;
            }
        }
        METER(else table->stats.removeMisses++);
        entry = NULL;
        break;

      default:
        JS_ASSERT(0);
    }

    if (biasedDeltaLog2) {
        if (!ChangeTable(table, biasedDeltaLog2 - DELTA_LOG2_BIAS, entry)) {
            /* If we just grabbed the last free entry, undo and fail hard. */
            if (op == JS_DHASH_ADD &&
                table->entryCount + table->removedCount == size) {
                METER(table->stats.addFailures++);
                MARK_ENTRY_FREE(entry);
                table->entryCount--;
                entry = NULL;
            }
        } else {
            if (op == JS_DHASH_ADD) {
                /* If the table grew, add the new (skipped) entry. */ 
                entry = SearchTable(table, key, keyHash);
                JS_ASSERT(JS_DHASH_ENTRY_IS_FREE(entry));
                entry->keyHash = keyHash;
            }
        }
    }

#undef DELTA_LOG2_BIAS

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
    char *entryAddr, *entryLimit;
    uint32 i, capacity, entrySize;
    JSDHashEntryHdr *entry;
    JSDHashOperator op;

    entryAddr = table->entryStore;
    entrySize = table->entrySize;
    capacity = JS_BIT(table->sizeLog2);
    entryLimit = entryAddr + capacity * entrySize;
    i = 0;
    while (entryAddr < entryLimit) {
        entry = (JSDHashEntryHdr *)entryAddr;
        if (ENTRY_IS_LIVE(entry)) {
            op = etor(table, entry, i++, arg);
            if (op & JS_DHASH_REMOVE) {
                METER(table->stats.removeEnums++);
                JS_DHashTableRawRemove(table, entry);
            }
            if (op & JS_DHASH_STOP)
                break;
        }
        entryAddr += entrySize;
    }

    /* Shrink or compress if enough entries were removed that alpha < .5. */
    if (table->removedCount >= capacity >> 2) {
        METER(table->stats.enumShrinks++);
        capacity = table->entryCount;
        capacity += capacity >> 1;
        if (capacity < JS_DHASH_MIN_SIZE)
            capacity = JS_DHASH_MIN_SIZE;
        (void) ChangeTable(table,
                           JS_CeilingLog2(capacity) - table->sizeLog2,
                           NULL);
    }
    return i;
}

#ifdef JS_DHASHMETER
#include <math.h>

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
    fprintf(fp, "       number of compresses: %u\n", table->stats.compresses);
    fprintf(fp, "number of enumerate shrinks: %u\n", table->stats.enumShrinks);

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

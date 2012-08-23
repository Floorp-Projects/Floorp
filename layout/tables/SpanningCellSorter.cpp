/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=4:et:sw=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Code to sort cells by their colspan, used by BasicTableLayoutStrategy.
 */

#include "SpanningCellSorter.h"
#include "nsQuickSort.h"
#include "nsIPresShell.h"

//#define DEBUG_SPANNING_CELL_SORTER

SpanningCellSorter::SpanningCellSorter()
  : mState(ADDING)
  , mSortedHashTable(nullptr)
{
    memset(mArray, 0, sizeof(mArray));
    mHashTable.entryCount = 0;
}

SpanningCellSorter::~SpanningCellSorter()
{
    if (mHashTable.entryCount) {
        PL_DHashTableFinish(&mHashTable);
        mHashTable.entryCount = 0;
    }
    delete [] mSortedHashTable;
}

/* static */ PLDHashTableOps
SpanningCellSorter::HashTableOps = {
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    HashTableHashKey,
    HashTableMatchEntry,
    PL_DHashMoveEntryStub,
    PL_DHashClearEntryStub,
    PL_DHashFinalizeStub,
    nullptr
};

/* static */ PLDHashNumber
SpanningCellSorter::HashTableHashKey(PLDHashTable *table, const void *key)
{
    return NS_PTR_TO_INT32(key);
}

/* static */ bool
SpanningCellSorter::HashTableMatchEntry(PLDHashTable *table,
                                        const PLDHashEntryHdr *hdr,
                                        const void *key)
{
    const HashTableEntry *entry = static_cast<const HashTableEntry*>(hdr);
    return NS_PTR_TO_INT32(key) == entry->mColSpan;
}

bool
SpanningCellSorter::AddCell(int32_t aColSpan, int32_t aRow, int32_t aCol)
{
    NS_ASSERTION(mState == ADDING, "cannot call AddCell after GetNext");
    NS_ASSERTION(aColSpan >= ARRAY_BASE, "cannot add cells with colspan<2");

    Item *i = (Item*) mozilla::AutoStackArena::Allocate(sizeof(Item));
    NS_ENSURE_TRUE(i != nullptr, false);

    i->row = aRow;
    i->col = aCol;

    if (UseArrayForSpan(aColSpan)) {
        int32_t index = SpanToIndex(aColSpan);
        i->next = mArray[index];
        mArray[index] = i;
    } else {
        if (!mHashTable.entryCount &&
            !PL_DHashTableInit(&mHashTable, &HashTableOps, nullptr,
                               sizeof(HashTableEntry), PL_DHASH_MIN_SIZE)) {
            NS_NOTREACHED("table init failed");
            mHashTable.entryCount = 0;
            return false;
        }
        HashTableEntry *entry = static_cast<HashTableEntry*>
                                           (PL_DHashTableOperate(&mHashTable, NS_INT32_TO_PTR(aColSpan),
                                 PL_DHASH_ADD));
        NS_ENSURE_TRUE(entry, false);

        NS_ASSERTION(entry->mColSpan == 0 || entry->mColSpan == aColSpan,
                     "wrong entry");
        NS_ASSERTION((entry->mColSpan == 0) == (entry->mItems == nullptr),
                     "entry should be either new or properly initialized");
        entry->mColSpan = aColSpan;

        i->next = entry->mItems;
        entry->mItems = i;
    }

    return true;
}

/* static */ PLDHashOperator
SpanningCellSorter::FillSortedArray(PLDHashTable *table, PLDHashEntryHdr *hdr,
                                    uint32_t number, void *arg)
{
    HashTableEntry *entry = static_cast<HashTableEntry*>(hdr);
    HashTableEntry **sh = static_cast<HashTableEntry**>(arg);

    sh[number] = entry;

    return PL_DHASH_NEXT;
}

/* static */ int
SpanningCellSorter::SortArray(const void *a, const void *b, void *closure)
{
    int32_t spanA = (*static_cast<HashTableEntry*const*>(a))->mColSpan;
    int32_t spanB = (*static_cast<HashTableEntry*const*>(b))->mColSpan;

    if (spanA < spanB)
        return -1;
    if (spanA == spanB)
        return 0;
    return 1;
}

SpanningCellSorter::Item*
SpanningCellSorter::GetNext(int32_t *aColSpan)
{
    NS_ASSERTION(mState != DONE, "done enumerating, stop calling");

    switch (mState) {
        case ADDING:
            /* prepare to enumerate the array */
            mState = ENUMERATING_ARRAY;
            mEnumerationIndex = 0;
            /* fall through */
        case ENUMERATING_ARRAY:
            while (mEnumerationIndex < ARRAY_SIZE && !mArray[mEnumerationIndex])
                ++mEnumerationIndex;
            if (mEnumerationIndex < ARRAY_SIZE) {
                Item *result = mArray[mEnumerationIndex];
                *aColSpan = IndexToSpan(mEnumerationIndex);
                NS_ASSERTION(result, "logic error");
#ifdef DEBUG_SPANNING_CELL_SORTER
                printf("SpanningCellSorter[%p]:"
                       " returning list for colspan=%d from array\n",
                       static_cast<void*>(this), *aColSpan);
#endif
                ++mEnumerationIndex;
                return result;
            }
            /* prepare to enumerate the hash */
            mState = ENUMERATING_HASH;
            mEnumerationIndex = 0;
            if (mHashTable.entryCount) {
                HashTableEntry **sh =
                    new HashTableEntry*[mHashTable.entryCount];
                if (!sh) {
                    // give up
                    mState = DONE;
                    return nullptr;
                }
                PL_DHashTableEnumerate(&mHashTable, FillSortedArray, sh);
                NS_QuickSort(sh, mHashTable.entryCount, sizeof(sh[0]),
                             SortArray, nullptr);
                mSortedHashTable = sh;
            }
            /* fall through */
        case ENUMERATING_HASH:
            if (mEnumerationIndex < mHashTable.entryCount) {
                Item *result = mSortedHashTable[mEnumerationIndex]->mItems;
                *aColSpan = mSortedHashTable[mEnumerationIndex]->mColSpan;
                NS_ASSERTION(result, "holes in hash table");
#ifdef DEBUG_SPANNING_CELL_SORTER
                printf("SpanningCellSorter[%p]:"
                       " returning list for colspan=%d from hash\n",
                       static_cast<void*>(this), *aColSpan);
#endif
                ++mEnumerationIndex;
                return result;
            }
            mState = DONE;
            /* fall through */
        case DONE:
            ;
    }
    return nullptr;
}

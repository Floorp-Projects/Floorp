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
  , mHashTable(&HashTableOps, sizeof(HashTableEntry))
  , mSortedHashTable(nullptr)
{
    memset(mArray, 0, sizeof(mArray));
}

SpanningCellSorter::~SpanningCellSorter()
{
    delete [] mSortedHashTable;
}

/* static */ const PLDHashTableOps
SpanningCellSorter::HashTableOps = {
    HashTableHashKey,
    HashTableMatchEntry,
    PLDHashTable::MoveEntryStub,
    PLDHashTable::ClearEntryStub,
    nullptr
};

/* static */ PLDHashNumber
SpanningCellSorter::HashTableHashKey(const void *key)
{
    return NS_PTR_TO_INT32(key);
}

/* static */ bool
SpanningCellSorter::HashTableMatchEntry(const PLDHashEntryHdr *hdr,
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
        auto entry = static_cast<HashTableEntry*>
            (mHashTable.Add(NS_INT32_TO_PTR(aColSpan), fallible));
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
            MOZ_FALLTHROUGH;
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
            if (mHashTable.EntryCount() > 0) {
                HashTableEntry **sh =
                    new HashTableEntry*[mHashTable.EntryCount()];
                int32_t j = 0;
                for (auto iter = mHashTable.Iter(); !iter.Done(); iter.Next()) {
                    sh[j++] = static_cast<HashTableEntry*>(iter.Get());
                }
                NS_QuickSort(sh, mHashTable.EntryCount(), sizeof(sh[0]),
                             SortArray, nullptr);
                mSortedHashTable = sh;
            }
            MOZ_FALLTHROUGH;
        case ENUMERATING_HASH:
            if (mEnumerationIndex < mHashTable.EntryCount()) {
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
            MOZ_FALLTHROUGH;
        case DONE:
            ;
    }
    return nullptr;
}

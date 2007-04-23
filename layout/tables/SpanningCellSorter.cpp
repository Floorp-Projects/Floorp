/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=4:et:sw=4:
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla's table layout code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
 * Code to sort cells by their colspan, used by BasicTableLayoutStrategy.
 */

#include "SpanningCellSorter.h"
#include "nsQuickSort.h"

//#define DEBUG_SPANNING_CELL_SORTER

SpanningCellSorter::SpanningCellSorter(nsIPresShell *aPresShell)
  : mPresShell(aPresShell)
  , mState(ADDING)
  , mSortedHashTable(nsnull)
{
    memset(mArray, 0, sizeof(mArray));
    mHashTable.entryCount = 0;
    mPresShell->PushStackMemory();
}

SpanningCellSorter::~SpanningCellSorter()
{
    if (mHashTable.entryCount) {
        PL_DHashTableFinish(&mHashTable);
        mHashTable.entryCount = 0;
    }
    delete [] mSortedHashTable;
    mPresShell->PopStackMemory();
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
    nsnull
};

/* static */ PR_CALLBACK PLDHashNumber
SpanningCellSorter::HashTableHashKey(PLDHashTable *table, const void *key)
{
    return NS_PTR_TO_INT32(key);
}

/* static */ PR_CALLBACK PRBool
SpanningCellSorter::HashTableMatchEntry(PLDHashTable *table,
                                        const PLDHashEntryHdr *hdr,
                                        const void *key)
{
    const HashTableEntry *entry = NS_STATIC_CAST(const HashTableEntry*, hdr);
    return NS_PTR_TO_INT32(key) == entry->mColSpan;
}

PRBool
SpanningCellSorter::AddCell(PRInt32 aColSpan, PRInt32 aRow, PRInt32 aCol)
{
    NS_ASSERTION(mState == ADDING, "cannot call AddCell after GetNext");
    NS_ASSERTION(aColSpan >= ARRAY_BASE, "cannot add cells with colspan<2");

    Item *i = (Item*) mPresShell->AllocateStackMemory(sizeof(Item));
    NS_ENSURE_TRUE(i != nsnull, PR_FALSE);

    i->row = aRow;
    i->col = aCol;

    if (UseArrayForSpan(aColSpan)) {
        PRInt32 index = SpanToIndex(aColSpan);
        i->next = mArray[index];
        mArray[index] = i;
    } else {
        if (!mHashTable.entryCount &&
            !PL_DHashTableInit(&mHashTable, &HashTableOps, nsnull,
                               sizeof(HashTableEntry), PL_DHASH_MIN_SIZE)) {
            NS_NOTREACHED("table init failed");
            mHashTable.entryCount = 0;
            return PR_FALSE;
        }
        HashTableEntry *entry = NS_STATIC_CAST(HashTableEntry*,
            PL_DHashTableOperate(&mHashTable, NS_INT32_TO_PTR(aColSpan),
                                 PL_DHASH_ADD));
        NS_ENSURE_TRUE(entry, PR_FALSE);

        NS_ASSERTION(entry->mColSpan == 0 || entry->mColSpan == aColSpan,
                     "wrong entry");
        NS_ASSERTION((entry->mColSpan == 0) == (entry->mItems == nsnull),
                     "entry should be either new or properly initialized");
        entry->mColSpan = aColSpan;

        i->next = entry->mItems;
        entry->mItems = i;
    }

    return PR_TRUE;
}

/* static */ PR_CALLBACK PLDHashOperator
SpanningCellSorter::FillSortedArray(PLDHashTable *table, PLDHashEntryHdr *hdr,
                                    PRUint32 number, void *arg)
{
    HashTableEntry *entry = NS_STATIC_CAST(HashTableEntry*, hdr);
    HashTableEntry **sh = NS_STATIC_CAST(HashTableEntry**, arg);

    sh[number] = entry;

    return PL_DHASH_NEXT;
}

/* static */ int
SpanningCellSorter::SortArray(const void *a, const void *b, void *closure)
{
    PRInt32 spanA = (*NS_STATIC_CAST(HashTableEntry*const*, a))->mColSpan;
    PRInt32 spanB = (*NS_STATIC_CAST(HashTableEntry*const*, b))->mColSpan;

    if (spanA < spanB)
        return -1;
    if (spanA == spanB)
        return 0;
    return 1;
}

SpanningCellSorter::Item*
SpanningCellSorter::GetNext(PRInt32 *aColSpan)
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
                       NS_STATIC_CAST(void*, this), *aColSpan);
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
                    return nsnull;
                }
                PL_DHashTableEnumerate(&mHashTable, FillSortedArray, sh);
                NS_QuickSort(sh, mHashTable.entryCount, sizeof(sh[0]),
                             SortArray, nsnull);
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
                       NS_STATIC_CAST(void*, this), *aColSpan);
#endif
                ++mEnumerationIndex;
                return result;
            }
            mState = DONE;
            /* fall through */
        case DONE:
            ;
    }
    return nsnull;
}

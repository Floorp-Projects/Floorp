/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=4:et:sw=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Code to sort cells by their colspan, used by BasicTableLayoutStrategy.
 */

#include "pldhash.h"
#include "nsDebug.h"
#include "StackArena.h"

class nsIPresShell;

/**
 * The SpanningCellSorter is responsible for accumulating lists of cells
 * with colspans so that those cells can later be enumerated, sorted
 * from lowest number of columns spanned to highest.  It does not use a
 * stable sort (in fact, it currently reverses).
 */
class NS_STACK_CLASS SpanningCellSorter {
public:
    SpanningCellSorter(nsIPresShell *aPresShell);
    ~SpanningCellSorter();

    struct Item {
        PRInt32 row, col;
        Item *next;
    };

    /**
     * Add a cell to the sorter.  Returns false on out of memory.
     * aColSpan is the number of columns spanned, and aRow/aCol are the
     * position of the cell in the table (for GetCellInfoAt).
     */
    bool AddCell(PRInt32 aColSpan, PRInt32 aRow, PRInt32 aCol);

    /**
     * Get the next *list* of cells.  Each list contains all the cells
     * for a colspan value, and the lists are given in order from lowest
     * to highest colspan.  The colspan value is filled in to *aColSpan.
     */
    Item* GetNext(PRInt32 *aColSpan);
private:
    nsIPresShell *mPresShell;

    enum State { ADDING, ENUMERATING_ARRAY, ENUMERATING_HASH, DONE };
    State mState;

    // store small colspans in an array for fast sorting and
    // enumeration, and large colspans in a hash table

    enum { ARRAY_BASE = 2 };
    enum { ARRAY_SIZE = 8 };
    Item *mArray[ARRAY_SIZE];
    PRInt32 SpanToIndex(PRInt32 aSpan) { return aSpan - ARRAY_BASE; }
    PRInt32 IndexToSpan(PRInt32 aIndex) { return aIndex + ARRAY_BASE; }
    bool UseArrayForSpan(PRInt32 aSpan) {
        NS_ASSERTION(SpanToIndex(aSpan) >= 0, "cell without colspan");
        return SpanToIndex(aSpan) < ARRAY_SIZE;
    }

    PLDHashTable mHashTable;
    struct HashTableEntry : public PLDHashEntryHdr {
        PRInt32 mColSpan;
        Item *mItems;
    };

    static PLDHashTableOps HashTableOps;

    static PLDHashNumber
        HashTableHashKey(PLDHashTable *table, const void *key);
    static bool
        HashTableMatchEntry(PLDHashTable *table, const PLDHashEntryHdr *hdr,
                            const void *key);

    static PLDHashOperator
        FillSortedArray(PLDHashTable *table, PLDHashEntryHdr *hdr,
                        PRUint32 number, void *arg);

    static int SortArray(const void *a, const void *b, void *closure);

    /* state used only during enumeration */
    PRUint32 mEnumerationIndex; // into mArray or mSortedHashTable
    HashTableEntry **mSortedHashTable;

    /*
     * operator new is forbidden since we use the pres shell's stack
     * memory, which much be pushed and popped at points matching a
     * push/pop on the C++ stack.
     */
    void* operator new(size_t sz) CPP_THROW_NEW { return nsnull; }
};


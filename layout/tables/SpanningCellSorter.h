/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=4:et:sw=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SpanningCellSorter_h
#define SpanningCellSorter_h

/*
 * Code to sort cells by their colspan, used by BasicTableLayoutStrategy.
 */

#include "PLDHashTable.h"
#include "nsDebug.h"
#include "StackArena.h"

/**
 * The SpanningCellSorter is responsible for accumulating lists of cells
 * with colspans so that those cells can later be enumerated, sorted
 * from lowest number of columns spanned to highest.  It does not use a
 * stable sort (in fact, it currently reverses).
 */
class MOZ_STACK_CLASS SpanningCellSorter {
public:
    SpanningCellSorter();
    ~SpanningCellSorter();

    struct Item {
        int32_t row, col;
        Item *next;
    };

    /**
     * Add a cell to the sorter.  Returns false on out of memory.
     * aColSpan is the number of columns spanned, and aRow/aCol are the
     * position of the cell in the table (for GetCellInfoAt).
     */
    bool AddCell(int32_t aColSpan, int32_t aRow, int32_t aCol);

    /**
     * Get the next *list* of cells.  Each list contains all the cells
     * for a colspan value, and the lists are given in order from lowest
     * to highest colspan.  The colspan value is filled in to *aColSpan.
     */
    Item* GetNext(int32_t *aColSpan);
private:

    enum State { ADDING, ENUMERATING_ARRAY, ENUMERATING_HASH, DONE };
    State mState;

    // store small colspans in an array for fast sorting and
    // enumeration, and large colspans in a hash table

    enum { ARRAY_BASE = 2 };
    enum { ARRAY_SIZE = 8 };
    Item *mArray[ARRAY_SIZE];
    int32_t SpanToIndex(int32_t aSpan) { return aSpan - ARRAY_BASE; }
    int32_t IndexToSpan(int32_t aIndex) { return aIndex + ARRAY_BASE; }
    bool UseArrayForSpan(int32_t aSpan) {
        NS_ASSERTION(SpanToIndex(aSpan) >= 0, "cell without colspan");
        return SpanToIndex(aSpan) < ARRAY_SIZE;
    }

    PLDHashTable mHashTable;
    struct HashTableEntry : public PLDHashEntryHdr {
        int32_t mColSpan;
        Item *mItems;
    };

    static const PLDHashTableOps HashTableOps;

    static PLDHashNumber HashTableHashKey(const void *key);
    static bool
        HashTableMatchEntry(const PLDHashEntryHdr *hdr, const void *key);

    static int SortArray(const void *a, const void *b, void *closure);

    /* state used only during enumeration */
    uint32_t mEnumerationIndex; // into mArray or mSortedHashTable
    HashTableEntry **mSortedHashTable;

    /*
     * operator new is forbidden since we use the pres shell's stack
     * memory, which much be pushed and popped at points matching a
     * push/pop on the C++ stack.
     */
    void* operator new(size_t sz) CPP_THROW_NEW { return nullptr; }
};

#endif

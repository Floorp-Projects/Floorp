/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Chris Waterson <waterson@netscape.com>
 */

#ifndef nsTemplateMatchSet_h__
#define nsTemplateMatchSet_h__

#include "nsRuleNetwork.h"
#include "nsFixedSizeAllocator.h"
#include "nsTemplateMatch.h"
#include "plhash.h"

/**
 * A collection of unique nsTemplateMatch objects.
 */
class nsTemplateMatchSet
{
public:
    class ConstIterator;
    friend class ConstIterator;

    class Iterator;
    friend class Iterator;

    nsTemplateMatchSet();
    ~nsTemplateMatchSet();

private:
    nsTemplateMatchSet(const nsTemplateMatchSet& aMatchSet); // XXX not to be implemented
    void operator=(const nsTemplateMatchSet& aMatchSet); // XXX not to be implemented

protected:
    struct List {
        static void* operator new(size_t aSize, nsFixedSizeAllocator& aPool) {
            return aPool.Alloc(aSize); }

        static void operator delete(void* aPtr, size_t aSize) {
            nsFixedSizeAllocator::Free(aPtr, aSize); }

        nsTemplateMatch* mMatch;
        List* mNext;
        List* mPrev;
    };

    List mHead;

    // Lazily created when we pass a size threshold.
    PLHashTable* mMatches;
    PRInt32 mCount;

    const nsTemplateMatch* mLastMatch;

    static PLHashNumber PR_CALLBACK HashMatch(const void* aMatch) {
        const nsTemplateMatch* match = NS_STATIC_CAST(const nsTemplateMatch*, aMatch);
        return Instantiation::Hash(&match->mInstantiation) ^ (PLHashNumber(match->mRule) >> 2); }

    static PRIntn PR_CALLBACK CompareMatches(const void* aLeft, const void* aRight) {
        const nsTemplateMatch* left  = NS_STATIC_CAST(const nsTemplateMatch*, aLeft);
        const nsTemplateMatch* right = NS_STATIC_CAST(const nsTemplateMatch*, aRight);
        return *left == *right; }

    enum { kHashTableThreshold = 8 };

    static PLHashAllocOps gAllocOps;

    static void* PR_CALLBACK AllocTable(void* aPool, PRSize aSize) {
        return new char[aSize]; }

    static void PR_CALLBACK FreeTable(void* aPool, void* aItem) {
        delete[] NS_STATIC_CAST(char*, aItem); }

    static PLHashEntry* PR_CALLBACK AllocEntry(void* aPool, const void* aKey) {
        nsFixedSizeAllocator* pool = NS_STATIC_CAST(nsFixedSizeAllocator*, aPool);
        PLHashEntry* entry = NS_STATIC_CAST(PLHashEntry*, pool->Alloc(sizeof(PLHashEntry)));
        return entry; }

    static void PR_CALLBACK FreeEntry(void* aPool, PLHashEntry* aEntry, PRUintn aFlag) {
        if (aFlag == HT_FREE_ENTRY)
            nsFixedSizeAllocator::Free(aEntry, sizeof(PLHashEntry)); }

public:
    // Used to initialize the nsFixedSizeAllocator that's used to pool
    // entries.
    enum {
        kEntrySize = sizeof(List),
        kIndexSize = sizeof(PLHashEntry)
    };

    class ConstIterator {
    protected:
        friend class Iterator; // XXXwaterson so broken.
        List* mCurrent;

    public:
        ConstIterator() : mCurrent(nsnull) {}

        ConstIterator(List* aCurrent) : mCurrent(aCurrent) {}

        ConstIterator(const ConstIterator& aConstIterator)
            : mCurrent(aConstIterator.mCurrent) {}

        ConstIterator& operator=(const ConstIterator& aConstIterator) {
            mCurrent = aConstIterator.mCurrent;
            return *this; }

        ConstIterator& operator++() {
            mCurrent = mCurrent->mNext;
            return *this; }

        ConstIterator operator++(int) {
            ConstIterator result(*this);
            mCurrent = mCurrent->mNext;
            return result; }

        ConstIterator& operator--() {
            mCurrent = mCurrent->mPrev;
            return *this; }

        ConstIterator operator--(int) {
            ConstIterator result(*this);
            mCurrent = mCurrent->mPrev;
            return result; }

        const nsTemplateMatch& operator*() const {
            return *mCurrent->mMatch; }

        const nsTemplateMatch* operator->() const {
            return mCurrent->mMatch; }

        PRBool operator==(const ConstIterator& aConstIterator) const {
            return mCurrent == aConstIterator.mCurrent; }

        PRBool operator!=(const ConstIterator& aConstIterator) const {
            return mCurrent != aConstIterator.mCurrent; }
    };

    ConstIterator First() const { return ConstIterator(mHead.mNext); }
    ConstIterator Last() const { return ConstIterator(NS_CONST_CAST(List*, &mHead)); }

    class Iterator : public ConstIterator {
    public:
        Iterator() {}

        Iterator(List* aCurrent) : ConstIterator(aCurrent) {}

        Iterator& operator++() {
            mCurrent = mCurrent->mNext;
            return *this; }

        Iterator operator++(int) {
            Iterator result(*this);
            mCurrent = mCurrent->mNext;
            return result; }

        Iterator& operator--() {
            mCurrent = mCurrent->mPrev;
            return *this; }

        Iterator operator--(int) {
            Iterator result(*this);
            mCurrent = mCurrent->mPrev;
            return result; }

        nsTemplateMatch& operator*() const {
            return *mCurrent->mMatch; }

        nsTemplateMatch* operator->() const {
            return mCurrent->mMatch; }

        PRBool operator==(const ConstIterator& aConstIterator) const {
            return mCurrent == aConstIterator.mCurrent; }

        PRBool operator!=(const ConstIterator& aConstIterator) const {
            return mCurrent != aConstIterator.mCurrent; }

        friend class nsTemplateMatchSet;
    };

    Iterator First() { return Iterator(mHead.mNext); }
    Iterator Last() { return Iterator(&mHead); }

    PRBool Empty() const { return First() == Last(); }

    PRInt32 Count() const { return mCount; }

    ConstIterator Find(const nsTemplateMatch& aMatch) const;
    Iterator Find(const nsTemplateMatch& aMatch);

    PRBool Contains(const nsTemplateMatch& aMatch) const {
        return Find(aMatch) != Last(); }

    nsTemplateMatch* FindMatchWithHighestPriority() const;

    const nsTemplateMatch* GetLastMatch() const { return mLastMatch; }
    void SetLastMatch(const nsTemplateMatch* aMatch) { mLastMatch = aMatch; }

    Iterator Insert(nsFixedSizeAllocator& aPool, Iterator aIterator, nsTemplateMatch* aMatch);

    Iterator Add(nsFixedSizeAllocator& aPool, nsTemplateMatch* aMatch) {
        return Insert(aPool, Last(), aMatch); }

    nsresult CopyInto(nsTemplateMatchSet& aMatchSet, nsFixedSizeAllocator& aPool) const;

    void Clear();

    Iterator Erase(Iterator aIterator);

    void Remove(nsTemplateMatch* aMatch);
};

#endif // nsTemplateMatchSet_h__

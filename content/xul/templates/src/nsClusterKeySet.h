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

#ifndef nsClusterKeySet_h__
#define nsClusterKeySet_h__

#include "nsClusterKey.h"
#include "nsFixedSizeAllocator.h"

/**
 * A collection of nsClusterKey objects.
 */
class nsClusterKeySet {
public:
    class ConstIterator;
    friend class ConstIterator;

protected:
    class Entry {
    private:
        // Hide so that only Create() and Destroy() can be used to
        // allocate and deallocate from the heap
        static void* operator new(size_t) { return 0; }
        static void operator delete(void*, size_t) {}

    public:
        Entry() { MOZ_COUNT_CTOR(nsClusterKeySet::Entry); }

        Entry(const nsClusterKey& aKey) : mKey(aKey) {
            MOZ_COUNT_CTOR(nsClusterKeySet::Entry); }

        ~Entry() { MOZ_COUNT_DTOR(nsClusterKeySet::Entry); }

        static Entry*
        Create(nsFixedSizeAllocator& aPool, const nsClusterKey& aKey) {
            void* place = aPool.Alloc(sizeof(Entry));
            return place ? ::new (place) Entry(aKey) : nsnull; }

        static void
        Destroy(nsFixedSizeAllocator& aPool, Entry* aEntry) {
            aEntry->~Entry();
            aPool.Free(aEntry, sizeof(*aEntry)); }

        PLHashEntry  mHashEntry;
        nsClusterKey mKey;
        Entry*       mPrev;
        Entry*       mNext;
    };

    PLHashTable* mTable;
    Entry mHead;

    nsFixedSizeAllocator mPool;

public:
    nsClusterKeySet();
    ~nsClusterKeySet();

    class ConstIterator {
    protected:
        Entry* mCurrent;

    public:
        ConstIterator(Entry* aEntry) : mCurrent(aEntry) {}

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

        const nsClusterKey& operator*() const {
            return mCurrent->mKey; }

        const nsClusterKey* operator->() const {
            return &mCurrent->mKey; }

        PRBool operator==(const ConstIterator& aConstIterator) const {
            return mCurrent == aConstIterator.mCurrent; }

        PRBool operator!=(const ConstIterator& aConstIterator) const {
            return mCurrent != aConstIterator.mCurrent; }
    };

    ConstIterator First() const { return ConstIterator(mHead.mNext); }
    ConstIterator Last() const { return ConstIterator(NS_CONST_CAST(Entry*, &mHead)); }

    PRBool Contains(const nsClusterKey& aKey);
    nsresult Add(const nsClusterKey& aKey);

protected:
    static PLHashAllocOps gAllocOps;

    static void* PR_CALLBACK AllocTable(void* aPool, PRSize aSize) {
        return new char[aSize]; }

    static void PR_CALLBACK FreeTable(void* aPool, void* aItem) {
        delete[] NS_STATIC_CAST(char*, aItem); }

    static PLHashEntry* PR_CALLBACK AllocEntry(void* aPool, const void* aKey) {
        nsFixedSizeAllocator* pool = NS_STATIC_CAST(nsFixedSizeAllocator*, aPool);
        const nsClusterKey* key = NS_STATIC_CAST(const nsClusterKey*, aKey);
        Entry* entry = Entry::Create(*pool, *key);
        return NS_REINTERPRET_CAST(PLHashEntry*, entry); }

    static void PR_CALLBACK FreeEntry(void* aPool, PLHashEntry* aEntry, PRUintn aFlag) {
        nsFixedSizeAllocator* pool = NS_STATIC_CAST(nsFixedSizeAllocator*, aPool);
        if (aFlag == HT_FREE_ENTRY)
            Entry::Destroy(*pool, NS_REINTERPRET_CAST(Entry*, aEntry)); }
};

#endif // nsClusterKeySet_h__

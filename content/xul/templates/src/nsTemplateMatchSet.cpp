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

#include "nsTemplateMatchSet.h"
#include "nsTemplateRule.h"

PLHashAllocOps nsTemplateMatchSet::gAllocOps = {
    AllocTable, FreeTable, AllocEntry, FreeEntry };


nsTemplateMatchSet::nsTemplateMatchSet()
    : mMatches(nsnull), mCount(0), mLastMatch(nsnull)
{
    MOZ_COUNT_CTOR(nsTemplateMatchSet);
    mHead.mNext = mHead.mPrev = &mHead;
}

#ifdef NEED_CPP_UNUSED_IMPLEMENTATIONS
nsTemplateMatchSet::nsTemplateMatchSet(const nsTemplateMatchSet& aMatchSet) {}
void nsTemplateMatchSet::operator=(const nsTemplateMatchSet& aMatchSet) {}
#endif

nsTemplateMatchSet::~nsTemplateMatchSet()
{
    if (mMatches) {
        PL_HashTableDestroy(mMatches);
        mMatches = nsnull;
    }

    Clear();
    MOZ_COUNT_DTOR(nsTemplateMatchSet);
}


nsTemplateMatchSet::Iterator
nsTemplateMatchSet::Insert(nsFixedSizeAllocator& aPool, Iterator aIterator, nsTemplateMatch* aMatch)
{
    if (++mCount > kHashTableThreshold && !mMatches) {
        // If we've exceeded a high-water mark, then hash everything.
        mMatches = PL_NewHashTable(2 * kHashTableThreshold,
                                   HashMatch,
                                   CompareMatches,
                                   PL_CompareValues,
                                   &gAllocOps,
                                   &aPool);

        Iterator last = Last();
        for (Iterator match = First(); match != last; ++match) {
            // The sole purpose of the hashtable is to make
            // determining nsTemplateMatchSet membership an O(1)
            // operation. Since the linked list is maintaining
            // storage, we can use a pointer to the nsTemplateMatch object
            // (obtained from the iterator) as the key. We'll just
            // stuff something non-zero into the table as a value.
            PL_HashTableAdd(mMatches, match.operator->(), match.mCurrent);
        }
    }

    List* newelement = new (aPool) List();
    if (newelement) {
        newelement->mMatch = aMatch;
        aMatch->AddRef();

        aIterator.mCurrent->mPrev->mNext = newelement;

        newelement->mNext = aIterator.mCurrent;
        newelement->mPrev = aIterator.mCurrent->mPrev;

        aIterator.mCurrent->mPrev = newelement;

        if (mMatches)
            PL_HashTableAdd(mMatches, newelement->mMatch, newelement);
    }
    return aIterator;
}


nsresult
nsTemplateMatchSet::CopyInto(nsTemplateMatchSet& aMatchSet, nsFixedSizeAllocator& aPool) const
{
    aMatchSet.Clear();
    for (nsTemplateMatchSet::ConstIterator match = First(); match != Last(); ++match)
        aMatchSet.Add(aPool, NS_CONST_CAST(nsTemplateMatch*, match.operator->()));

    return NS_OK;
}

void
nsTemplateMatchSet::Clear()
{
    Iterator match = First();
    while (match != Last())
        Erase(match++);
}

nsTemplateMatchSet::ConstIterator
nsTemplateMatchSet::Find(const nsTemplateMatch& aMatch) const
{
    if (mMatches) {
        List* list = NS_STATIC_CAST(List*, PL_HashTableLookup(mMatches, &aMatch));
        if (list)
            return ConstIterator(list);
    }
    else {
        ConstIterator last = Last();
        for (ConstIterator i = First(); i != last; ++i) {
            if (*i == aMatch)
                return i;
        }
    }

    return Last();
}

nsTemplateMatchSet::Iterator
nsTemplateMatchSet::Find(const nsTemplateMatch& aMatch)
{
    if (mMatches) {
        List* list = NS_STATIC_CAST(List*, PL_HashTableLookup(mMatches, &aMatch));
        if (list)
            return Iterator(list);
    }
    else {
        Iterator last = Last();
        for (Iterator i = First(); i != last; ++i) {
            if (*i == aMatch)
                return i;
        }
    }

    return Last();
}

nsTemplateMatch*
nsTemplateMatchSet::FindMatchWithHighestPriority() const
{
    // Find the rule with the "highest priority"; i.e., the rule with
    // the lowest value for GetPriority().
    nsTemplateMatch* result = nsnull;
    PRInt32 max = ~(1 << 31); // XXXwaterson portable?
    for (ConstIterator match = First(); match != Last(); ++match) {
        PRInt32 priority = match->mRule->GetPriority();
        if (priority < max) {
            result = NS_CONST_CAST(nsTemplateMatch*, match.operator->());
            max = priority;
        }
    }
    return result;
}

nsTemplateMatchSet::Iterator
nsTemplateMatchSet::Erase(Iterator aIterator)
{
    Iterator result = aIterator;

    --mCount;

    if (mMatches)
        PL_HashTableRemove(mMatches, aIterator.operator->());

    ++result;
    aIterator.mCurrent->mNext->mPrev = aIterator.mCurrent->mPrev;
    aIterator.mCurrent->mPrev->mNext = aIterator.mCurrent->mNext;

    aIterator->Release();
    delete aIterator.mCurrent;
    return result;
}

void
nsTemplateMatchSet::Remove(nsTemplateMatch* aMatch)
{
    Iterator doomed = Find(*aMatch);
    if (doomed != Last())
        Erase(doomed);
}


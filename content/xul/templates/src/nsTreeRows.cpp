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

#include "nsOutlinerRows.h"
#include "nsTemplateMatch.h"
#include "nsTemplateRule.h"

nsOutlinerRows::Subtree*
nsOutlinerRows::EnsureSubtreeFor(Subtree* aParent,
                                 PRInt32 aChildIndex)
{
    Subtree* subtree = GetSubtreeFor(aParent, aChildIndex);

    if (! subtree) {
        subtree = aParent->mRows[aChildIndex].mSubtree = new Subtree(aParent);
        InvalidateCachedRow();
    }

    return subtree;
}

nsOutlinerRows::Subtree*
nsOutlinerRows::GetSubtreeFor(const Subtree* aParent,
                              PRInt32 aChildIndex,
                              PRInt32* aSubtreeSize)
{
    NS_PRECONDITION(aParent, "no parent");
    NS_PRECONDITION(aChildIndex >= 0, "bad child index");

    Subtree* result = nsnull;

    if (aChildIndex < aParent->mCount)
        result = aParent->mRows[aChildIndex].mSubtree;

    if (aSubtreeSize)
        *aSubtreeSize = result ? result->mSubtreeSize : 0;

    return result;
}

void
nsOutlinerRows::RemoveSubtreeFor(Subtree* aParent, PRInt32 aChildIndex)
{
    NS_PRECONDITION(aParent, "no parent");
    NS_PRECONDITION(aChildIndex >= 0 && aChildIndex < aParent->mCount, "bad child index");

    Row& row = aParent->mRows[aChildIndex];

    if (row.mSubtree) {
        PRInt32 subtreeSize = row.mSubtree->GetSubtreeSize();

        delete row.mSubtree;
        row.mSubtree = nsnull;

        for (Subtree* subtree = aParent; subtree != nsnull; subtree = subtree->mParent)
            subtree->mSubtreeSize -= subtreeSize;
    }

    InvalidateCachedRow();
}

nsOutlinerRows::iterator
nsOutlinerRows::First()
{
    iterator result;
    Subtree* current = &mRoot;
    while (current && current->Count()) {
        result.Push(current, 0);
        current = GetSubtreeFor(current, 0);
    }
    result.SetRowIndex(0);
    return result;
}

nsOutlinerRows::iterator
nsOutlinerRows::Last()
{
    iterator result;

    // Build up a path along the rightmost edge of the tree
    Subtree* current = &mRoot;
    while (current && current->Count()) {
        result.Push(current, current->Count() - 1);
        current = GetSubtreeFor(current, current->Count() - 1);
    }

    // Now, at the bottom rightmost leaf, advance us one off the end.
    result.mLink[result.mTop].mChildIndex++;

    // Our row index will be the size of the root subree, plus one.
    result.SetRowIndex(mRoot.GetSubtreeSize() + 1);

    return result;
}

nsOutlinerRows::iterator
nsOutlinerRows::operator[](PRInt32 aRow)
{
    // See if we're just lucky, and end up with something
    // nearby. (This tends to happen a lot due to the way that we get
    // asked for rows n' stuff.)
    PRInt32 last = mLastRow.GetRowIndex();
    if (last != -1) {
        if (aRow == last)
            return mLastRow;
        else if (last + 1 == aRow)
            return ++mLastRow;
        else if (last - 1 == aRow)
            return --mLastRow;
    }

    // Nope. Construct a path to the specified index. This is a little
    // bit better than O(n), because we can skip over subtrees. (So it
    // ends up being approximately linear in the subtree size, instead
    // of the entire view size. But, most of the time, big views are
    // flat. Oh well.)
    iterator result;
    Subtree* current = &mRoot;

    PRInt32 index = 0;
    result.SetRowIndex(aRow);

    do {
        PRInt32 subtreeSize;
        Subtree* subtree = GetSubtreeFor(current, index, &subtreeSize);

        if (subtreeSize >= aRow) {
            result.Push(current, index);
            current = subtree;
            index = 0;
            --aRow;
        }
        else {
            ++index;
            aRow -= subtreeSize + 1;
        }
    } while (aRow >= 0);

    mLastRow = result;
    return result;
}

nsOutlinerRows::iterator
nsOutlinerRows::Find(nsConflictSet& aConflictSet, nsIRDFResource* aMember)
{
    // XXX Mmm, scan through the rows one-by-one...
    iterator last = Last();
    iterator iter;

    for (iter = First(); iter != last; ++iter) {
        nsTemplateMatch* match = iter->mMatch;

        Value val;
        match->GetAssignmentFor(aConflictSet, match->mRule->GetMemberVariable(), &val);

        if (VALUE_TO_IRDFRESOURCE(val) == aMember)
            break;
    }

    return iter;
}

void
nsOutlinerRows::Clear()
{
    mRoot.Clear();
    InvalidateCachedRow();
}

//----------------------------------------------------------------------
//
// nsOutlinerRows::Subtree
//

nsOutlinerRows::Subtree::~Subtree()
{
    Clear();
}

void
nsOutlinerRows::Subtree::Clear()
{
    for (PRInt32 i = mCount - 1; i >= 0; --i)
        delete mRows[i].mSubtree;

    delete[] mRows;

    mRows = nsnull;
    mCount = mCapacity = 0;
}

PRBool
nsOutlinerRows::Subtree::InsertRowAt(nsTemplateMatch* aMatch, PRInt32 aIndex)
{
    if (mCount >= mCapacity || aIndex >= mCapacity) {
        PRInt32 newCapacity = NS_MAX(mCapacity * 2, aIndex + 1);
        Row* newRows = new Row[newCapacity];
        if (! newRows)
            return PR_FALSE;

        for (PRInt32 i = mCount - 1; i >= 0; --i)
            newRows[i] = mRows[i];

        delete[] mRows;

        mRows = newRows;
        mCapacity = newCapacity;
    }

    for (PRInt32 i = mCount - 1; i >= aIndex; --i)
        mRows[i + 1] = mRows[i];

    mRows[aIndex].mMatch = aMatch;
    mRows[aIndex].mSubtree = nsnull;
    ++mCount;

    for (Subtree* subtree = this; subtree != nsnull; subtree = subtree->mParent)
        ++subtree->mSubtreeSize;

    return PR_TRUE;
}

void
nsOutlinerRows::Subtree::RemoveRowAt(PRInt32 aIndex)
{
    NS_PRECONDITION(aIndex >= 0 && aIndex < Count(), "bad index");
    if (aIndex < 0 || aIndex >= Count())
        return;

    PRInt32 subtreeSize = mRows[aIndex].mSubtree
        ? mRows[aIndex].mSubtree->GetSubtreeSize()
        : 0;

    ++subtreeSize;

    delete mRows[aIndex].mSubtree;

    for (PRInt32 i = aIndex + 1; i < mCount; ++i)
        mRows[i - 1] = mRows[i];

    --mCount;

    for (Subtree* subtree = this; subtree != nsnull; subtree = subtree->mParent)
        subtree->mSubtreeSize -= subtreeSize;
}

//----------------------------------------------------------------------
//
// nsOutlinerRows::iterator
//

nsOutlinerRows::iterator::iterator(const iterator& aIterator)
    : mTop(aIterator.mTop),
      mRowIndex(aIterator.mRowIndex)
{
    for (PRInt32 i = mTop; i >= 0; --i)
        mLink[i] = aIterator.mLink[i];
}

nsOutlinerRows::iterator&
nsOutlinerRows::iterator::operator=(const iterator& aIterator)
{
    mTop = aIterator.mTop;
    mRowIndex = aIterator.mRowIndex;
    for (PRInt32 i = mTop; i >= 0; --i)
        mLink[i] = aIterator.mLink[i];
    return *this;
}

void
nsOutlinerRows::iterator::Push(Subtree* aParent, PRInt32 aChildIndex)
{
    if (mTop < kMaxDepth - 1) {
        ++mTop;
        mLink[mTop].mParent     = aParent;
        mLink[mTop].mChildIndex = aChildIndex;
    }
    else
        NS_ERROR("overflow");
}

PRBool
nsOutlinerRows::iterator::operator==(const iterator& aIterator) const
{
    if (mTop != aIterator.mTop)
        return PR_FALSE;

    if (mTop == -1)
        return PR_TRUE;

    return PRBool(mLink[mTop] == aIterator.mLink[mTop]);
}

void
nsOutlinerRows::iterator::Next()
{
    NS_PRECONDITION(mTop >= 0, "cannot increment an uninitialized iterator");

    // Increment the absolute row index
    ++mRowIndex;

    Link& top = mLink[mTop];

    // Is there a child subtree? If so, descend into the child
    // subtree.
    Subtree* subtree = top.GetRow().mSubtree;

    if (subtree && subtree->Count()) {
        Push(subtree, 0);
        return;
    }

    // Have we exhausted the current subtree?
    if (top.mChildIndex >= top.mParent->Count() - 1) {
        // Yep. See if we've just iterated path the last element in
        // the tree, period. Walk back up the stack, looking for any
        // unfinished subtrees.
        PRInt32 unfinished;
        for (unfinished = mTop - 1; unfinished >= 0; --unfinished) {
            const Link& link = mLink[unfinished];
            if (link.mChildIndex < link.mParent->Count() - 1)
                break;
        }

        // If there are no unfinished subtrees in the stack, then this
        // iterator is exhausted. Leave it in the same state that
        // Last() does.
        if (unfinished < 0)
            return;

        // Otherwise, we ran off the end of one of the inner
        // subtrees. Pop up to the next unfinished level in the stack.
        mTop = unfinished;
    }

    // Advance to the next child in this subtree
    ++(mLink[mTop].mChildIndex);
}

void
nsOutlinerRows::iterator::Prev()
{
    NS_PRECONDITION(mTop >= 0, "cannot increment an uninitialized iterator");

    // Decrement the absolute row index
    --mRowIndex;

    // Move to the previous child in this subtree
    --(mLink[mTop].mChildIndex);

    // Have we exhausted the current subtree?
    if (mLink[mTop].mChildIndex < 0) {
        // Yep. See if we've just iterated back to the first element
        // in the tree, period. Walk back up the stack, looking for
        // any unfinished subtrees.
        PRInt32 unfinished;
        for (unfinished = mTop - 1; unfinished >= 0; --unfinished) {
            const Link& link = mLink[unfinished];
            if (link.mChildIndex >= 0)
                break;
        }

        // If there are no unfinished subtrees in the stack, then this
        // iterator is exhausted. Leave it in the same state that
        // First() does.
        if (unfinished < 0)
            return;

        // Otherwise, we ran off the end of one of the inner
        // subtrees. Pop up to the next unfinished level in the stack.
        mTop = unfinished;
        return;
    }

    // Is there a child subtree immediately prior to our current
    // position? If so, descend into it, grovelling down to the
    // deepest, rightmost left edge.
    Subtree* parent = mLink[mTop].GetParent();
    PRInt32 index = mLink[mTop].GetChildIndex();

    Subtree* subtree = (*parent)[index].mSubtree;

    if (subtree && subtree->Count()) {
        do {
            index = subtree->Count() - 1;
            Push(subtree, index);

            parent = subtree;
            subtree = (*parent)[index].mSubtree;
        } while (subtree && subtree->Count());
    }
}

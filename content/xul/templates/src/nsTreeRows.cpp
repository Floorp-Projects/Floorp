/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson <waterson@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsString.h"
#include "nsTreeRows.h"

nsTreeRows::Subtree*
nsTreeRows::EnsureSubtreeFor(Subtree* aParent,
                             PRInt32 aChildIndex)
{
    Subtree* subtree = GetSubtreeFor(aParent, aChildIndex);

    if (! subtree) {
        subtree = aParent->mRows[aChildIndex].mSubtree = new Subtree(aParent);
        InvalidateCachedRow();
    }

    return subtree;
}

nsTreeRows::Subtree*
nsTreeRows::GetSubtreeFor(const Subtree* aParent,
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
nsTreeRows::RemoveSubtreeFor(Subtree* aParent, PRInt32 aChildIndex)
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

nsTreeRows::iterator
nsTreeRows::First()
{
    iterator result;
    result.Append(&mRoot, 0);
    result.SetRowIndex(0);
    return result;
}

nsTreeRows::iterator
nsTreeRows::Last()
{
    iterator result;

    // Build up a path along the rightmost edge of the tree
    Subtree* current = &mRoot;
    PRInt32 count = current->Count();
    do  {
        PRInt32 last = count - 1;
        result.Append(current, last);
        current = count ? GetSubtreeFor(current, last) : nsnull;
    } while (current && ((count = current->Count()) != 0));

    // Now, at the bottom rightmost leaf, advance us one off the end.
    result.GetTop().mChildIndex++;

    // Our row index will be the size of the root subree, plus one.
    result.SetRowIndex(mRoot.GetSubtreeSize() + 1);

    return result;
}

nsTreeRows::iterator
nsTreeRows::operator[](PRInt32 aRow)
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
            result.Append(current, index);
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

nsTreeRows::iterator
nsTreeRows::FindByResource(nsIRDFResource* aResource)
{
    // XXX Mmm, scan through the rows one-by-one...
    iterator last = Last();
    iterator iter;

    nsresult rv;
    nsAutoString resourceid;
    bool stringmode = false;

    for (iter = First(); iter != last; ++iter) {
        if (!stringmode) {
            nsCOMPtr<nsIRDFResource> findres;
            rv = iter->mMatch->mResult->GetResource(getter_AddRefs(findres));
            if (NS_FAILED(rv)) return last;

            if (findres == aResource)
                break;

            if (! findres) {
                const char *uri;
                aResource->GetValueConst(&uri);
                CopyUTF8toUTF16(uri, resourceid);

                // set stringmode and fall through
                stringmode = PR_TRUE;
            }
        }

        // additional check because previous block could change stringmode
        if (stringmode) {
            nsAutoString findid;
            rv = iter->mMatch->mResult->GetId(findid);
            if (NS_FAILED(rv)) return last;

            if (resourceid.Equals(findid))
                break;
        }
    }

    return iter;
}

nsTreeRows::iterator
nsTreeRows::Find(nsIXULTemplateResult *aResult)
{
    // XXX Mmm, scan through the rows one-by-one...
    iterator last = Last();
    iterator iter;

    for (iter = First(); iter != last; ++iter) {
        if (aResult == iter->mMatch->mResult)
          break;
    }

    return iter;
}

void
nsTreeRows::Clear()
{
    mRoot.Clear();
    InvalidateCachedRow();
}

//----------------------------------------------------------------------
//
// nsTreeRows::Subtree
//

nsTreeRows::Subtree::~Subtree()
{
    Clear();
}

void
nsTreeRows::Subtree::Clear()
{
    for (PRInt32 i = mCount - 1; i >= 0; --i)
        delete mRows[i].mSubtree;

    delete[] mRows;

    mRows = nsnull;
    mCount = mCapacity = mSubtreeSize = 0;
}

nsTreeRows::iterator
nsTreeRows::Subtree::InsertRowAt(nsTemplateMatch* aMatch, PRInt32 aIndex)
{
    if (mCount >= mCapacity || aIndex >= mCapacity) {
        PRInt32 newCapacity = NS_MAX(mCapacity * 2, aIndex + 1);
        Row* newRows = new Row[newCapacity];
        if (! newRows)
            return iterator();

        for (PRInt32 i = mCount - 1; i >= 0; --i)
            newRows[i] = mRows[i];

        delete[] mRows;

        mRows = newRows;
        mCapacity = newCapacity;
    }

    for (PRInt32 i = mCount - 1; i >= aIndex; --i)
        mRows[i + 1] = mRows[i];

    mRows[aIndex].mMatch = aMatch;
    mRows[aIndex].mContainerType = eContainerType_Unknown;
    mRows[aIndex].mContainerState = eContainerState_Unknown;
    mRows[aIndex].mContainerFill = eContainerFill_Unknown;
    mRows[aIndex].mSubtree = nsnull;
    ++mCount;

    // Now build an iterator that points to the newly inserted element.
    PRInt32 rowIndex = 0;
    iterator result;
    result.Push(this, aIndex);

    for ( ; --aIndex >= 0; ++rowIndex) {
        // Account for open subtrees in the absolute row index.
        const Subtree *subtree = mRows[aIndex].mSubtree;
        if (subtree)
            rowIndex += subtree->mSubtreeSize;
    }

    Subtree *subtree = this;
    do {
        // Note that the subtree's size has expanded.
        ++subtree->mSubtreeSize;

        Subtree *parent = subtree->mParent;
        if (! parent)
            break;

        // Account for open subtrees in the absolute row index.
        PRInt32 count = parent->Count();
        for (aIndex = 0; aIndex < count; ++aIndex, ++rowIndex) {
            const Subtree *child = (*parent)[aIndex].mSubtree;
            if (subtree == child)
                break;

            if (child)
                rowIndex += child->mSubtreeSize;
        }

        NS_ASSERTION(aIndex < count, "couldn't find subtree in parent");

        result.Push(parent, aIndex);
        subtree = parent;
        ++rowIndex; // One for the parent row.
    } while (1);

    result.SetRowIndex(rowIndex);
    return result;
}

void
nsTreeRows::Subtree::RemoveRowAt(PRInt32 aIndex)
{
    NS_PRECONDITION(aIndex >= 0 && aIndex < Count(), "bad index");
    if (aIndex < 0 || aIndex >= Count())
        return;

    // How big is the subtree we're going to be removing?
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
// nsTreeRows::iterator
//

nsTreeRows::iterator::iterator(const iterator& aIterator)
    : mRowIndex(aIterator.mRowIndex),
      mLink(aIterator.mLink)
{
}

nsTreeRows::iterator&
nsTreeRows::iterator::operator=(const iterator& aIterator)
{
    mRowIndex = aIterator.mRowIndex;
    mLink = aIterator.mLink;
    return *this;
}

void
nsTreeRows::iterator::Append(Subtree* aParent, PRInt32 aChildIndex)
{
    Link *link = mLink.AppendElement();
    if (link) {
        link->mParent     = aParent;
        link->mChildIndex = aChildIndex;
    }
    else
        NS_ERROR("out of memory");
}

void
nsTreeRows::iterator::Push(Subtree *aParent, PRInt32 aChildIndex)
{
    Link *link = mLink.InsertElementAt(0);
    if (link) {
        link->mParent     = aParent;
        link->mChildIndex = aChildIndex;
    }
    else
        NS_ERROR("out of memory");
}

bool
nsTreeRows::iterator::operator==(const iterator& aIterator) const
{
    if (GetDepth() != aIterator.GetDepth())
        return PR_FALSE;

    if (GetDepth() == 0)
        return PR_TRUE;

    return GetTop() == aIterator.GetTop();
}

void
nsTreeRows::iterator::Next()
{
    NS_PRECONDITION(GetDepth() > 0, "cannot increment an uninitialized iterator");

    // Increment the absolute row index
    ++mRowIndex;

    Link& top = GetTop();

    // Is there a child subtree? If so, descend into the child
    // subtree.
    Subtree* subtree = top.GetRow().mSubtree;

    if (subtree && subtree->Count()) {
        Append(subtree, 0);
        return;
    }

    // Have we exhausted the current subtree?
    if (top.mChildIndex >= top.mParent->Count() - 1) {
        // Yep. See if we've just iterated path the last element in
        // the tree, period. Walk back up the stack, looking for any
        // unfinished subtrees.
        PRInt32 unfinished;
        for (unfinished = GetDepth() - 2; unfinished >= 0; --unfinished) {
            const Link& link = mLink[unfinished];
            if (link.mChildIndex < link.mParent->Count() - 1)
                break;
        }

        // If there are no unfinished subtrees in the stack, then this
        // iterator is exhausted. Leave it in the same state that
        // Last() does.
        if (unfinished < 0) {
            top.mChildIndex++;
            return;
        }

        // Otherwise, we ran off the end of one of the inner
        // subtrees. Pop up to the next unfinished level in the stack.
        mLink.SetLength(unfinished + 1);
    }

    // Advance to the next child in this subtree
    ++(GetTop().mChildIndex);
}

void
nsTreeRows::iterator::Prev()
{
    NS_PRECONDITION(GetDepth() > 0, "cannot increment an uninitialized iterator");

    // Decrement the absolute row index
    --mRowIndex;

    // Move to the previous child in this subtree
    --(GetTop().mChildIndex);

    // Have we exhausted the current subtree?
    if (GetTop().mChildIndex < 0) {
        // Yep. See if we've just iterated back to the first element
        // in the tree, period. Walk back up the stack, looking for
        // any unfinished subtrees.
        PRInt32 unfinished;
        for (unfinished = GetDepth() - 2; unfinished >= 0; --unfinished) {
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
        mLink.SetLength(unfinished + 1);
        return;
    }

    // Is there a child subtree immediately prior to our current
    // position? If so, descend into it, grovelling down to the
    // deepest, rightmost left edge.
    Subtree* parent = GetTop().GetParent();
    PRInt32 index = GetTop().GetChildIndex();

    Subtree* subtree = (*parent)[index].mSubtree;

    if (subtree && subtree->Count()) {
        do {
            index = subtree->Count() - 1;
            Append(subtree, index);

            parent = subtree;
            subtree = (*parent)[index].mSubtree;
        } while (subtree && subtree->Count());
    }
}

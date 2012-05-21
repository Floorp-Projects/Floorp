/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * used by nsCSSFrameConstructor to determine and iterate the child list
 * used to construct frames (normal children or something from XBL)
 */

#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsINodeList.h"

/**
 * Helper class for iterating children during frame construction.
 * This class should always be used in lieu of the straight content
 * node APIs, since it handles XBL-generated anonymous content as
 * well.
 */
class NS_STACK_CLASS ChildIterator
{
protected:
  nsIContent* mContent;
  // If mNodes is non-null (so XBLInvolved() is true), mIndex is the
  // index into mNodes for our current position.  Otherwise, mChild is
  // our current child (which might be null if we're done iterating).
  union {
    PRUint32 mIndex;
    nsIContent* mChild;
  };
  nsINodeList* mNodes;

public:
  ChildIterator()
    : mContent(nsnull), mChild(0), mNodes(nsnull) {}

  ChildIterator(const ChildIterator& aOther)
    : mContent(aOther.mContent),
      mNodes(aOther.mNodes) {
    if (XBLInvolved()) {
      mIndex = aOther.mIndex;
    } else {
      mChild = aOther.mChild;
    }
  }

  ChildIterator& operator=(const ChildIterator& aOther) {
    mContent = aOther.mContent;
    mNodes = aOther.mNodes;
    if (XBLInvolved()) {
      mIndex = aOther.mIndex;
    } else {
      mChild = aOther.mChild;
    }
    return *this;
  }

  ChildIterator& operator++() {
    if (XBLInvolved()) {
      ++mIndex;
    } else {
      NS_ASSERTION(mChild, "Walking off end of list?");
      mChild = mChild->GetNextSibling();
    }

    return *this;
  }

  ChildIterator operator++(int) {
    ChildIterator result(*this);
    ++(*this);
    return result;
  }

  ChildIterator& operator--() {
    if (XBLInvolved()) {
      --mIndex;
    } else if (mChild) {
      mChild = mChild->GetPreviousSibling();
      NS_ASSERTION(mChild, "Walking off beginning of list");
    } else {
      mChild = mContent->GetLastChild();
    }
    return *this;
  }

  ChildIterator operator--(int) {
    ChildIterator result(*this);
    --(*this);
    return result;
  }

  nsIContent* get() const {
    if (XBLInvolved()) {
      return mNodes->GetNodeAt(mIndex);
    }

    return mChild;
  }

  nsIContent* operator*() const { return get(); }

  bool operator==(const ChildIterator& aOther) const {
    if (XBLInvolved()) {
      return mContent == aOther.mContent && mIndex == aOther.mIndex;
    }

    return mContent == aOther.mContent && mChild == aOther.mChild;
  }

  bool operator!=(const ChildIterator& aOther) const {
    return !aOther.operator==(*this);
  }

  void seek(nsIContent* aContent) {
    if (XBLInvolved()) {
      PRInt32 index = mNodes->IndexOf(aContent);
      // XXXbz I wish we could assert that index != -1, but it seems to not be
      // the case in some XBL cases with filtered insertion points and no
      // default insertion point.  I will now claim that XBL's management of
      // its insertion points is broken in those cases, since it's returning an
      // insertion parent for a node that doesn't actually have the node in its
      // child list according to ChildIterator.  See bug 474324.
      if (index != -1) {
        mIndex = index;
      } else {
        // If aContent isn't going to get hit by this iterator, just seek to the
        // end of the list for lack of anything better to do.
        mIndex = length();
      }
    } else if (aContent->GetParent() == mContent) {
      mChild = aContent;
    } else {
      // XXXbz I wish we could assert this doesn't happen, but I think that's
      // not necessarily the case when called from ContentInserted if
      // first-letter frames are about.
      mChild = nsnull;
    }
  }

  bool XBLInvolved() const { return mNodes != nsnull; }

  /**
   * Create a pair of ChildIterators for a content node. aFirst will
   * point to the first child of aContent; aLast will point one past
   * the last child of aContent.
   */
  static nsresult Init(nsIContent*    aContent,
                       ChildIterator* aFirst,
                       ChildIterator* aLast);

private:
  PRUint32 length() {
    NS_PRECONDITION(XBLInvolved(), "Don't call me");
    PRUint32 l;
    mNodes->GetLength(&l);
    return l;
  }
};

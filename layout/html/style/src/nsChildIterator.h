/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMNode.h"

/**
 * Helper class for iterating children during frame construction.
 * This class should always be used in lieu of the straight content
 * node APIs, since it handles XBL-generated anonymous content as
 * well.
 */
class ChildIterator
{
protected:
  nsCOMPtr<nsIContent> mContent;
  PRUint32 mIndex;
  nsCOMPtr<nsIDOMNodeList> mNodes;

public:
  ChildIterator()
    : mIndex(0) {}

  ChildIterator(const ChildIterator& aOther)
    : mContent(aOther.mContent),
      mIndex(aOther.mIndex),
      mNodes(aOther.mNodes) {}

  ChildIterator& operator=(const ChildIterator& aOther) {
    mContent = aOther.mContent;
    mIndex = aOther.mIndex;
    mNodes = aOther.mNodes;
    return *this;
  }

  ChildIterator& operator++() {
    ++mIndex;
    return *this;
  }

  ChildIterator operator++(int) {
    ChildIterator result(*this);
    ++mIndex;
    return result;
  }

  ChildIterator& operator--() {
    --mIndex;
    return *this;
  }

  ChildIterator operator--(int) {
    ChildIterator result(*this);
    --mIndex;
    return result;
  }

  already_AddRefed<nsIContent> get() const {
    nsIContent* result = nsnull;
    if (mNodes) {
      nsCOMPtr<nsIDOMNode> node;
      mNodes->Item(mIndex, getter_AddRefs(node));
      CallQueryInterface(node, &result);
    } else {
      result = mContent->GetChildAt(PRInt32(mIndex));
      NS_IF_ADDREF(result);
    }

    return result;
  }

  already_AddRefed<nsIContent> operator*() const { return get(); }

  PRBool operator==(const ChildIterator& aOther) const {
    return mContent == aOther.mContent && mIndex == aOther.mIndex;
  }

  PRBool operator!=(const ChildIterator& aOther) const {
    return !aOther.operator==(*this);
  }

  PRUint32 index() {
    return mIndex;
  }

  void seek(PRUint32 aIndex) {
    // Make sure that aIndex is reasonable.  This should be |#ifdef
    // DEBUG|, but we need these numbers for the temporary workaround
    // for bug 133219.
    PRUint32 length;
    if (mNodes)
      mNodes->GetLength(&length);
    else
      length = mContent->GetChildCount();

    NS_ASSERTION(PRInt32(aIndex) >= 0 && aIndex <= length, "out of bounds");

    // Temporary workaround for bug 133219.
    if (aIndex > length)
      aIndex = length;

    mIndex = aIndex;
  }

  /**
   * Create a pair of ChildIterators for a content node. aFirst will
   * point to the first child of aContent; aLast will point one past
   * the last child of aContent.
   */
  static nsresult Init(nsIContent*    aContent,
                       ChildIterator* aFirst,
                       ChildIterator* aLast);
};

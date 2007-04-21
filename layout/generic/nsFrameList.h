/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

/* class for maintaining a linked list of child frames */

#ifndef nsFrameList_h___
#define nsFrameList_h___

#include "nsIFrame.h"

/**
 * A class for managing a singly linked list of frames. Frames are
 * linked together through their next-sibling pointer.
 */
class nsFrameList {
public:
  nsFrameList() {
    mFirstChild = nsnull;
  }

  nsFrameList(nsIFrame* aHead) {
    mFirstChild = aHead;
#ifdef DEBUG
    CheckForLoops();
#endif
  }

  ~nsFrameList() {
  }

  void DestroyFrames();

  void SetFrames(nsIFrame* aFrameList) {
    mFirstChild = aFrameList;
#ifdef DEBUG
    CheckForLoops();
#endif
  }

  void AppendFrames(nsIFrame* aParent, nsIFrame* aFrameList);

  void AppendFrames(nsIFrame* aParent, nsFrameList& aFrameList) {
    AppendFrames(aParent, aFrameList.mFirstChild);
    aFrameList.mFirstChild = nsnull;
  }

  void AppendFrame(nsIFrame* aParent, nsIFrame* aFrame);

  // Take aFrame out of the frame list. This also disconnects aFrame
  // from the sibling list. This will return PR_FALSE if aFrame is
  // nsnull or if aFrame is not in the list. The second frame is
  // a hint for the prev-sibling of aFrame; if the hint is correct,
  // then this is O(1) time.
  PRBool RemoveFrame(nsIFrame* aFrame, nsIFrame* aPrevSiblingHint = nsnull);

  // Remove the first child from the list. The caller is assumed to be
  // holding a reference to the first child. This call is equivalent
  // in behavior to calling RemoveFrame(FirstChild()).
  PRBool RemoveFirstChild();

  // Take aFrame out of the frame list and then destroy it. This also
  // disconnects aFrame from the sibling list. This will return
  // PR_FALSE if aFrame is nsnull or if aFrame is not in the list.
  PRBool DestroyFrame(nsIFrame* aFrame);

  void InsertFrame(nsIFrame* aParent,
                   nsIFrame* aPrevSibling,
                   nsIFrame* aNewFrame);

  void InsertFrames(nsIFrame* aParent,
                    nsIFrame* aPrevSibling,
                    nsIFrame* aFrameList);

  void InsertFrames(nsIFrame* aParent, nsIFrame* aPrevSibling,
                    nsFrameList& aFrameList) {
    InsertFrames(aParent, aPrevSibling, aFrameList.FirstChild());
    aFrameList.mFirstChild = nsnull;
  }

  PRBool Split(nsIFrame* aAfterFrame, nsIFrame** aNextFrameResult);

  /**
   * Sort the frames according to content order so that the first
   * frame in the list is the first in content order. Frames for
   * the same content will be ordered so that a prev in flow
   * comes before its next in flow.
   */
  void SortByContentOrder();

  nsIFrame* FirstChild() const {
    return mFirstChild;
  }

  nsIFrame* LastChild() const;

  nsIFrame* FrameAt(PRInt32 aIndex) const;

  PRBool IsEmpty() const {
    return nsnull == mFirstChild;
  }

  PRBool NotEmpty() const {
    return nsnull != mFirstChild;
  }

  PRBool ContainsFrame(const nsIFrame* aFrame) const;

  PRInt32 GetLength() const;

  nsIFrame* GetPrevSiblingFor(nsIFrame* aFrame) const;

#ifdef IBMBIDI
  /**
   * Return the frame before this frame in visual order (after Bidi reordering).
   * If aFrame is null, return the last frame in visual order.
   */
  nsIFrame* GetPrevVisualFor(nsIFrame* aFrame) const;

  /**
   * Return the frame after this frame in visual order (after Bidi reordering).
   * If aFrame is null, return the first frame in visual order.
   */
  nsIFrame* GetNextVisualFor(nsIFrame* aFrame) const;
#endif // IBMBIDI

  void VerifyParent(nsIFrame* aParent) const;

#ifdef NS_DEBUG
  void List(FILE* out) const;
#endif

private:
#ifdef DEBUG
  void CheckForLoops();
#endif
  
protected:
  nsIFrame* mFirstChild;
};

#endif /* nsFrameList_h___ */

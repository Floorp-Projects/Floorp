/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
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
  }

  ~nsFrameList() {
  }

  void DeleteFrames(nsIPresContext& aPresContext);

  void SetFrames(nsIFrame* aFrameList) {
    mFirstChild = aFrameList;
  }

  void AppendFrames(nsIFrame* aParent, nsIFrame* aFrameList);

  void AppendFrames(nsIFrame* aParent, nsFrameList& aFrameList) {
    AppendFrames(aParent, aFrameList.mFirstChild);
    aFrameList.mFirstChild = nsnull;
  }

  void AppendFrame(nsIFrame* aParent, nsIFrame* aFrame);

  // Take aFrame out of the frame list. This also disconnects aFrame
  // from the sibling list. This will return PR_FALSE if aFrame is
  // nsnull or if aFrame is not in the list.
  PRBool RemoveFrame(nsIFrame* aFrame);

  // Remove the first child from the list. The caller is assumed to be
  // holding a reference to the first child. This call is equivalent
  // in behavior to calling RemoveFrame(FirstChild()).
  PRBool RemoveFirstChild();

  // Take aFrame out of the frame list and then delete it. This also
  // disconnects aFrame from the sibling list. This will return
  // PR_FALSE if aFrame is nsnull or if aFrame is not in the list.
  PRBool DeleteFrame(nsIPresContext& aPresContext, nsIFrame* aFrame);

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

  PRBool ReplaceFrame(nsIFrame* aParent,
                      nsIFrame* aOldFrame,
                      nsIFrame* aNewFrame);

  PRBool ReplaceAndDeleteFrame(nsIPresContext& aPresContext,
                               nsIFrame* aParent,
                               nsIFrame* aOldFrame,
                               nsIFrame* aNewFrame);

  PRBool Split(nsIFrame* aAfterFrame, nsIFrame** aNextFrameResult);

  nsIFrame* PullFrame(nsIFrame* aParent,
                      nsIFrame* aLastChild,
                      nsFrameList& aFromList);

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

  void VerifyParent(nsIFrame* aParent) const;

  void List(FILE* out) const;

protected:
  nsIFrame* mFirstChild;
};

#endif /* nsFrameList_h___ */

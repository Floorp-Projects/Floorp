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

  void AppendFrames(nsIFrame* aParent, nsIFrame* aFrameList);

  void AppendFrames(nsIFrame* aParent, nsFrameList& aFrameList) {
    AppendFrames(aParent, aFrameList.mFirstChild);
  }

  void AppendFrame(nsIFrame* aParent, nsIFrame* aFrame);

  PRBool RemoveFrame(nsIFrame* aFrame);

  PRBool DeleteFrame(nsIPresContext& aPresContext, nsIFrame* aFrame);

  void InsertFrame(nsIFrame* aParent,
                   nsIFrame* aPrevSibling,
                   nsIFrame* aNewFrame);

  void InsertFrames(nsIFrame* aParent,
                    nsIFrame* aPrevSibling,
                    nsIFrame* aFrameList);

  PRBool ReplaceFrame(nsIFrame* aParent,
                      nsIFrame* aOldFrame,
                      nsIFrame* aNewFrame);

  PRBool ReplaceAndDeleteFrame(nsIPresContext& aPresContext,
                               nsIFrame* aParent,
                               nsIFrame* aOldFrame,
                               nsIFrame* aNewFrame);

  PRBool Split(nsIFrame* aAfterFrame, nsIFrame** aNextFrameResult);

  PRBool PullFrame(nsIFrame* aParent,
                   nsFrameList& aFromList,
                   nsIFrame** aResult);

  void Join(nsIFrame* aParent, nsFrameList& aList) {
    AppendFrames(aParent, aList.mFirstChild);
    aList.mFirstChild = nsnull;
  }

  nsIFrame* FirstChild() const {
    return mFirstChild;
  }

  nsIFrame* LastChild() const;

  PRBool IsEmpty() const {
    return nsnull == mFirstChild;
  }

  PRBool ContainsFrame(nsIFrame* aFrame) const;

  PRInt32 GetLength() const;

  nsIFrame* GetPrevSiblingFor(nsIFrame* aFrame) const;

protected:
  nsIFrame* mFirstChild;
};

#endif /* nsFrameList_h___ */

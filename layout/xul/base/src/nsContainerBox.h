/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Author:
 * Eric D Vaughan
 *
 * Contributor(s): 
 */

#ifndef nsContainerBox_h___
#define nsContainerBox_h___

#include "nsBox.h"
#include "nsCOMPtr.h" 
#include "nsIBoxLayout.h"

class nsContainerBox : public nsBox {

public:

  NS_IMETHOD GetChildBox(nsIBox** aBox);

  nsContainerBox(nsIPresShell* aShell);

  NS_IMETHOD SetLayoutManager(nsIBoxLayout* aLayout);
  NS_IMETHOD GetLayoutManager(nsIBoxLayout** aLayout);
  NS_IMETHOD GetPrefSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetMinSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetMaxSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetAscent(nsBoxLayoutState& aBoxLayoutState, nscoord& aAscent);
  NS_IMETHOD Layout(nsBoxLayoutState& aBoxLayoutState);

  virtual nsIBox* GetBoxAt(PRInt32 aIndex);
  virtual nsIBox* GetBox(nsIFrame* aFrame);
  virtual PRInt32 GetIndexOf(nsIBox* aBox);
  virtual PRInt32 GetChildCount();
  virtual void ClearChildren(nsIPresShell* aShell);
  virtual PRInt32 CreateBoxList(nsIPresShell* aShell, nsIFrame* aList, nsIBox*& first, nsIBox*& last);
  virtual void RemoveAfter(nsIPresShell* aShell, nsIBox* aPrev);
  virtual void Remove(nsIPresShell* aShell, nsIFrame* aChild);
  virtual void Prepend(nsIPresShell* aShell, nsIFrame* aList);
  virtual void Append(nsIPresShell* aShell, nsIFrame* aList);
  virtual void Insert(nsIPresShell* aShell, nsIFrame* aPrevFrame, nsIFrame* aList);
  virtual void InsertAfter(nsIPresShell* aShell, nsIBox* aPrev, nsIFrame* aList);
  virtual void InitChildren(nsIPresShell* aShell, nsIFrame* aList);
  virtual nsIBox* GetPrevious(nsIFrame* aChild);
  virtual void SanityCheck(nsFrameList& aFrameList);
  virtual void SetDebugOnChildList(nsBoxLayoutState& aState, nsIBox* aChild, PRBool aDebug);

  // XXX Eventually these will move into nsIFrame.
  // These methods are used for XBL <children>.
  NS_IMETHOD GetInsertionPoint(nsIFrame** aFrame);
  NS_IMETHOD SetInsertionPoint(nsIFrame* aFrame);

protected:

  virtual nsresult LayoutChildAt(nsBoxLayoutState& aState, nsIBox* aBox, const nsRect& aRect);

  nsIBox* mFirstChild;
  nsIBox* mLastChild;
  PRInt32 mChildCount;
  nsCOMPtr<nsIBoxLayout> mLayoutManager;
  nsIFrame* mInsertionPoint;
};

#endif


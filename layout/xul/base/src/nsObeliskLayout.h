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
 * Contributor(s): 
 */

/**
 
  Author:
  Eric D Vaughan

**/

#ifndef nsObeliskLayout_h___
#define nsObeliskLayout_h___

#include "nsMonumentLayout.h"
#include "nsCOMPtr.h"

class nsObeliskLayout : public nsMonumentLayout, 
                        public nsBoxSizeListener
{
public:

  friend nsresult NS_NewObeliskLayout(nsIPresShell* aPresShell, nsCOMPtr<nsIBoxLayout>& aNewLayout);

  NS_IMETHOD GetPrefSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetMinSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetMaxSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD CastToObelisk(nsObeliskLayout** aObelisk);
  NS_IMETHOD ChildBecameDirty(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aChild);
  NS_IMETHOD BecameDirty(nsIBox* aBox, nsBoxLayoutState& aState);
  NS_IMETHOD ChildrenInserted(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aPrevBox, nsIBox* aChildList);
  NS_IMETHOD ChildrenAppended(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aChildList);
  NS_IMETHOD ChildrenRemoved(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aChildList);

protected:

  void ChildNeedsLayout(nsIBox* aBox, nsIBoxLayout* aChild);
  virtual void UpdateMonuments(nsIBox* aBox, nsBoxLayoutState& aState);

  /*
  virtual void ComputeChildSizes(nsIBox* aBox,
                         nsBoxLayoutState& aState, 
                         nscoord& aGivenSize, 
                         nsBoxSize* aBoxSizes, 
                         nsComputedBoxSize*& aComputedBoxSizes);
  */

  virtual void PopulateBoxSizes(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState, nsBoxSize*& aBoxSizes, nsComputedBoxSize*& aComputedBoxSizes, nscoord& aMinSize, nscoord& aMaxSize, PRInt32& aFlexes);
  virtual void WillBeDestroyed(nsIBox* aBox, nsBoxLayoutState& aState, nsBoxSizeList& aList);
  virtual void Desecrated(nsIBox* aBox, nsBoxLayoutState& aState, nsBoxSizeList& aList);

  nsObeliskLayout(nsIPresShell* aShell);
  virtual ~nsObeliskLayout();

private:
  nsBoxSizeList* mOtherMonumentList;

}; // class nsObeliskLayout

#endif


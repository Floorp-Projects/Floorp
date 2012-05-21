/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGridLayout2_h___
#define nsGridLayout2_h___

#include "nsStackLayout.h"
#include "nsIGridPart.h"
#include "nsCOMPtr.h"
#include "nsIFrame.h"
#include "nsGrid.h"

class nsGridRowGroupLayout;
class nsGridRowLayout;
class nsGridRow;
class nsBoxLayoutState;
class nsGridCell;

/**
 * The nsBoxLayout implementation for a grid.
 */
class nsGridLayout2 : public nsStackLayout, 
                      public nsIGridPart
{
public:

  friend nsresult NS_NewGridLayout2(nsIPresShell* aPresShell, nsBoxLayout** aNewLayout);

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD Layout(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState);
  virtual void IntrinsicWidthsDirty(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState);

  virtual nsGridRowGroupLayout* CastToRowGroupLayout() { return nsnull; }
  virtual nsGridLayout2* CastToGridLayout() { return this; }
  virtual nsGrid* GetGrid(nsIBox* aBox, PRInt32* aIndex, nsGridRowLayout* aRequestor=nsnull);
  virtual nsIGridPart* GetParentGridPart(nsIBox* aBox, nsIBox** aParentBox) {
    NS_NOTREACHED("Should not be called"); return nsnull;
  }
  virtual nsSize GetMinSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetMaxSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetPrefSize(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState);
  virtual void CountRowsColumns(nsIBox* aBox, PRInt32& aRowCount, PRInt32& aComputedColumnCount) { aRowCount++; }
  virtual void DirtyRows(nsIBox* aBox, nsBoxLayoutState& aState) { }
  virtual PRInt32 BuildRows(nsIBox* aBox, nsGridRow* aRows);
  virtual nsMargin GetTotalMargin(nsIBox* aBox, bool aIsHorizontal);
  virtual Type GetType() { return eGrid; }
  virtual void ChildrenInserted(nsIBox* aBox, nsBoxLayoutState& aState,
                                nsIBox* aPrevBox,
                                const nsFrameList::Slice& aNewChildren);
  virtual void ChildrenAppended(nsIBox* aBox, nsBoxLayoutState& aState,
                                const nsFrameList::Slice& aNewChildren);
  virtual void ChildrenRemoved(nsIBox* aBox, nsBoxLayoutState& aState,
                             nsIBox* aChildList);
  virtual void ChildrenSet(nsIBox* aBox, nsBoxLayoutState& aState,
                         nsIBox* aChildList);

  virtual nsIGridPart* AsGridPart() { return this; }

  static void AddOffset(nsBoxLayoutState& aState, nsIBox* aChild, nsSize& aSize);

protected:

  nsGridLayout2(nsIPresShell* aShell);
  nsGrid mGrid;

private:
  void AddWidth(nsSize& aSize, nscoord aSize2, bool aIsHorizontal);


}; // class nsGridLayout2


#endif


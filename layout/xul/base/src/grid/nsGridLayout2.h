/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGridLayout2_h___
#define nsGridLayout2_h___

#include "nsStackLayout.h"
#include "nsIGridPart.h"
#include "nsCoord.h"
#include "nsGrid.h"

class nsIPresContext;
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

  NS_IMETHOD Layout(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState);
  virtual void IntrinsicWidthsDirty(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState);

  virtual nsGridRowGroupLayout* CastToRowGroupLayout() { return nullptr; }
  virtual nsGridLayout2* CastToGridLayout() { return this; }
  virtual nsGrid* GetGrid(nsIFrame* aBox, PRInt32* aIndex, nsGridRowLayout* aRequestor=nullptr);
  virtual nsIGridPart* GetParentGridPart(nsIFrame* aBox, nsIFrame** aParentBox) {
    NS_NOTREACHED("Should not be called"); return nullptr;
  }
  virtual nsSize GetMinSize(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetMaxSize(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetPrefSize(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState);
  virtual void CountRowsColumns(nsIFrame* aBox, PRInt32& aRowCount, PRInt32& aComputedColumnCount) { aRowCount++; }
  virtual void DirtyRows(nsIFrame* aBox, nsBoxLayoutState& aState) { }
  virtual PRInt32 BuildRows(nsIFrame* aBox, nsGridRow* aRows);
  virtual nsMargin GetTotalMargin(nsIFrame* aBox, bool aIsHorizontal);
  virtual Type GetType() { return eGrid; }
  virtual void ChildrenInserted(nsIFrame* aBox, nsBoxLayoutState& aState,
                                nsIFrame* aPrevBox,
                                const nsFrameList::Slice& aNewChildren);
  virtual void ChildrenAppended(nsIFrame* aBox, nsBoxLayoutState& aState,
                                const nsFrameList::Slice& aNewChildren);
  virtual void ChildrenRemoved(nsIFrame* aBox, nsBoxLayoutState& aState,
                               nsIFrame* aChildList);
  virtual void ChildrenSet(nsIFrame* aBox, nsBoxLayoutState& aState,
                           nsIFrame* aChildList);

  virtual nsIGridPart* AsGridPart() { return this; }

  static void AddOffset(nsBoxLayoutState& aState, nsIFrame* aChild, nsSize& aSize);

protected:

  nsGridLayout2(nsIPresShell* aShell);
  nsGrid mGrid;

private:
  void AddWidth(nsSize& aSize, nscoord aSize2, bool aIsHorizontal);


}; // class nsGridLayout2


#endif


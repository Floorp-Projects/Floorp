/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGridLayout2_h___
#define nsGridLayout2_h___

#include "mozilla/Attributes.h"
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

  NS_IMETHOD Layout(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState) MOZ_OVERRIDE;
  virtual void IntrinsicWidthsDirty(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState);

  virtual nsGridRowGroupLayout* CastToRowGroupLayout() { return nullptr; }
  virtual nsGridLayout2* CastToGridLayout() MOZ_OVERRIDE { return this; }
  virtual nsGrid* GetGrid(nsIFrame* aBox, int32_t* aIndex, nsGridRowLayout* aRequestor=nullptr) MOZ_OVERRIDE;
  virtual nsIGridPart* GetParentGridPart(nsIFrame* aBox, nsIFrame** aParentBox) MOZ_OVERRIDE {
    NS_NOTREACHED("Should not be called"); return nullptr;
  }
  virtual nsSize GetMinSize(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState) MOZ_OVERRIDE;
  virtual nsSize GetMaxSize(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState) MOZ_OVERRIDE;
  virtual nsSize GetPrefSize(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState) MOZ_OVERRIDE;
  virtual void CountRowsColumns(nsIFrame* aBox, int32_t& aRowCount, int32_t& aComputedColumnCount) MOZ_OVERRIDE { aRowCount++; }
  virtual void DirtyRows(nsIFrame* aBox, nsBoxLayoutState& aState) MOZ_OVERRIDE { }
  virtual int32_t BuildRows(nsIFrame* aBox, nsGridRow* aRows) MOZ_OVERRIDE;
  virtual nsMargin GetTotalMargin(nsIFrame* aBox, bool aIsHorizontal) MOZ_OVERRIDE;
  virtual Type GetType() MOZ_OVERRIDE { return eGrid; }
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


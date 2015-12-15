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

class nsGridRowGroupLayout;
class nsGridRowLayout;
class nsGridRow;
class nsBoxLayoutState;

/**
 * The nsBoxLayout implementation for a grid.
 */
class nsGridLayout2 final : public nsStackLayout, 
                            public nsIGridPart
{
public:

  friend nsresult NS_NewGridLayout2(nsIPresShell* aPresShell, nsBoxLayout** aNewLayout);

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD Layout(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState) override;
  virtual void IntrinsicISizesDirty(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState) override;

  virtual nsGridRowGroupLayout* CastToRowGroupLayout() override { return nullptr; }
  virtual nsGridLayout2* CastToGridLayout() override { return this; }
  virtual nsGrid* GetGrid(nsIFrame* aBox, int32_t* aIndex, nsGridRowLayout* aRequestor=nullptr) override;
  virtual nsIGridPart* GetParentGridPart(nsIFrame* aBox, nsIFrame** aParentBox) override {
    NS_NOTREACHED("Should not be called"); return nullptr;
  }
  virtual nsSize GetMinSize(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetMaxSize(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetPrefSize(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState) override;
  virtual void CountRowsColumns(nsIFrame* aBox, int32_t& aRowCount, int32_t& aComputedColumnCount) override { aRowCount++; }
  virtual void DirtyRows(nsIFrame* aBox, nsBoxLayoutState& aState) override { }
  virtual int32_t BuildRows(nsIFrame* aBox, nsGridRow* aRows) override;
  virtual nsMargin GetTotalMargin(nsIFrame* aBox, bool aIsHorizontal) override;
  virtual Type GetType() override { return eGrid; }
  virtual void ChildrenInserted(nsIFrame* aBox, nsBoxLayoutState& aState,
                                nsIFrame* aPrevBox,
                                const nsFrameList::Slice& aNewChildren) override;
  virtual void ChildrenAppended(nsIFrame* aBox, nsBoxLayoutState& aState,
                                const nsFrameList::Slice& aNewChildren) override;
  virtual void ChildrenRemoved(nsIFrame* aBox, nsBoxLayoutState& aState,
                               nsIFrame* aChildList) override;
  virtual void ChildrenSet(nsIFrame* aBox, nsBoxLayoutState& aState,
                           nsIFrame* aChildList) override;

  virtual nsIGridPart* AsGridPart() override { return this; }

  static void AddOffset(nsIFrame* aChild, nsSize& aSize);

protected:

  explicit nsGridLayout2(nsIPresShell* aShell);
  virtual ~nsGridLayout2();
  nsGrid mGrid;

private:
  void AddWidth(nsSize& aSize, nscoord aSize2, bool aIsHorizontal);


}; // class nsGridLayout2


#endif


/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**

  Author:
  Eric D Vaughan

**/

#ifndef nsGridRowLayout_h___
#define nsGridRowLayout_h___

#include "mozilla/Attributes.h"
#include "nsSprocketLayout.h"
#include "nsIGridPart.h"
class nsGridRowGroupLayout;
class nsGridLayout2;
class nsBoxLayoutState;
class nsGrid;

/**
 * A common base class for nsGridRowLeafLayout (the nsBoxLayout object
 * for a grid row or column) and nsGridRowGroupLayout (the nsBoxLayout
 * object for a grid row group or column group).
 */
// XXXldb This needs a name that indicates that it's a base class for
// both row and rows (row-group).
class nsGridRowLayout : public nsSprocketLayout,
                        public nsIGridPart
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  virtual nsGridRowGroupLayout* CastToRowGroupLayout() override { return nullptr; }
  virtual nsGridLayout2* CastToGridLayout() override { return nullptr; }
  virtual nsGrid* GetGrid(nsIFrame* aBox, int32_t* aIndex, nsGridRowLayout* aRequestor=nullptr) override;
  virtual nsIGridPart* GetParentGridPart(nsIFrame* aBox, nsIFrame** aParentBox) override;
  virtual void ChildrenInserted(nsIFrame* aBox, nsBoxLayoutState& aState,
                                nsIFrame* aPrevBox,
                                const nsFrameList::Slice& aNewChildren) override;
  virtual void ChildrenAppended(nsIFrame* aBox, nsBoxLayoutState& aState,
                                const nsFrameList::Slice& aNewChildren) override;
  virtual void ChildrenRemoved(nsIFrame* aBox, nsBoxLayoutState& aState, nsIFrame* aChildList) override;
  virtual void ChildrenSet(nsIFrame* aBox, nsBoxLayoutState& aState, nsIFrame* aChildList) override;
  virtual nsMargin GetTotalMargin(nsIFrame* aBox, bool aIsHorizontal) override;

  virtual nsIGridPart* AsGridPart() override { return this; }

protected:
  virtual void ChildAddedOrRemoved(nsIFrame* aBox, nsBoxLayoutState& aState)=0;

  nsGridRowLayout();
  virtual ~nsGridRowLayout();
};

#endif


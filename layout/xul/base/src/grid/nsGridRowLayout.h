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

  virtual nsGridRowGroupLayout* CastToRowGroupLayout() { return nsnull; }
  virtual nsGridLayout2* CastToGridLayout() { return nsnull; }
  virtual nsGrid* GetGrid(nsIBox* aBox, PRInt32* aIndex, nsGridRowLayout* aRequestor=nsnull);
  virtual nsIGridPart* GetParentGridPart(nsIBox* aBox, nsIBox** aParentBox);
  virtual void ChildrenInserted(nsIBox* aBox, nsBoxLayoutState& aState,
                                nsIBox* aPrevBox,
                                const nsFrameList::Slice& aNewChildren);
  virtual void ChildrenAppended(nsIBox* aBox, nsBoxLayoutState& aState,
                                const nsFrameList::Slice& aNewChildren);
  virtual void ChildrenRemoved(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aChildList);
  virtual void ChildrenSet(nsIBox* aBox, nsBoxLayoutState& aState, nsIBox* aChildList);
  virtual nsMargin GetTotalMargin(nsIBox* aBox, bool aIsHorizontal);

  virtual nsIGridPart* AsGridPart() { return this; }

protected:
  virtual void ChildAddedOrRemoved(nsIBox* aBox, nsBoxLayoutState& aState)=0;

  nsGridRowLayout();
};

#endif


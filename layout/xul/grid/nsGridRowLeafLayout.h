/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 
  Author:
  Eric D Vaughan

**/

#ifndef nsGridRowLeafLayout_h___
#define nsGridRowLeafLayout_h___

#include "mozilla/Attributes.h"
#include "nsGridRowLayout.h"
#include "nsCOMPtr.h"

/**
 * The nsBoxLayout implementation for nsGridRowLeafFrame.
 */
// XXXldb This needs a better name that indicates that it's for any grid
// row.
class nsGridRowLeafLayout final : public nsGridRowLayout
{
public:

  friend already_AddRefed<nsBoxLayout> NS_NewGridRowLeafLayout();

  virtual nsSize GetXULPrefSize(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetXULMinSize(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetXULMaxSize(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState) override;
  virtual void ChildAddedOrRemoved(nsIFrame* aBox, nsBoxLayoutState& aState) override;
  NS_IMETHOD XULLayout(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState) override;
  virtual void CountRowsColumns(nsIFrame* aBox, int32_t& aRowCount, int32_t& aComputedColumnCount) override;
  virtual void DirtyRows(nsIFrame* aBox, nsBoxLayoutState& aState) override;
  virtual int32_t BuildRows(nsIFrame* aBox, nsGridRow* aRows) override;
  virtual Type GetType() override { return eRowLeaf; }

protected:

  virtual void PopulateBoxSizes(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState,
                                nsBoxSize*& aBoxSizes, nscoord& aMinSize,
                                nscoord& aMaxSize, int32_t& aFlexes) override;
  virtual void ComputeChildSizes(nsIFrame* aBox,
                                 nsBoxLayoutState& aState,
                                 nscoord& aGivenSize,
                                 nsBoxSize* aBoxSizes,
                                 nsComputedBoxSize*& aComputedBoxSizes) override;


  nsGridRowLeafLayout();
  virtual ~nsGridRowLeafLayout();
  //virtual void AddBorderAndPadding(nsIFrame* aBox, nsSize& aSize);

private:

}; // class nsGridRowLeafLayout

#endif


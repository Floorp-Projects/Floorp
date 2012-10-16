/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 
  Author:
  Eric D Vaughan

**/

#ifndef nsGridRowGroupLayout_h___
#define nsGridRowGroupLayout_h___

#include "mozilla/Attributes.h"
#include "nsGridRowLayout.h"

/**
 * The nsBoxLayout implementation for nsGridRowGroupFrame.
 */
class nsGridRowGroupLayout : public nsGridRowLayout
{
public:

  friend already_AddRefed<nsBoxLayout> NS_NewGridRowGroupLayout();

  virtual nsGridRowGroupLayout* CastToRowGroupLayout() { return this; }
  virtual nsSize GetMinSize(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetPrefSize(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetMaxSize(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState);
  virtual void CountRowsColumns(nsIFrame* aBox, int32_t& aRowCount, int32_t& aComputedColumnCount) MOZ_OVERRIDE;
  virtual void DirtyRows(nsIFrame* aBox, nsBoxLayoutState& aState) MOZ_OVERRIDE;
  virtual int32_t BuildRows(nsIFrame* aBox, nsGridRow* aRows) MOZ_OVERRIDE;
  virtual nsMargin GetTotalMargin(nsIFrame* aBox, bool aIsHorizontal) MOZ_OVERRIDE;
  virtual int32_t GetRowCount() MOZ_OVERRIDE { return mRowCount; }
  virtual Type GetType() MOZ_OVERRIDE { return eRowGroup; }

protected:
  nsGridRowGroupLayout();
  virtual ~nsGridRowGroupLayout();

  virtual void ChildAddedOrRemoved(nsIFrame* aBox, nsBoxLayoutState& aState) MOZ_OVERRIDE;
  static void AddWidth(nsSize& aSize, nscoord aSize2, bool aIsHorizontal);

private:
  int32_t mRowCount;
};

#endif


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

  virtual nsGridRowGroupLayout* CastToRowGroupLayout() override { return this; }
  virtual nsSize GetXULMinSize(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetXULPrefSize(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState) override;
  virtual nsSize GetXULMaxSize(nsIFrame* aBox, nsBoxLayoutState& aBoxLayoutState) override;
  virtual void CountRowsColumns(nsIFrame* aBox, int32_t& aRowCount, int32_t& aComputedColumnCount) override;
  virtual void DirtyRows(nsIFrame* aBox, nsBoxLayoutState& aState) override;
  virtual int32_t BuildRows(nsIFrame* aBox, nsGridRow* aRows) override;
  virtual nsMargin GetTotalMargin(nsIFrame* aBox, bool aIsHorizontal) override;
  virtual int32_t GetRowCount() override { return mRowCount; }
  virtual Type GetType() override { return eRowGroup; }

protected:
  nsGridRowGroupLayout();
  virtual ~nsGridRowGroupLayout();

  virtual void ChildAddedOrRemoved(nsIFrame* aBox, nsBoxLayoutState& aState) override;
  static void AddWidth(nsSize& aSize, nscoord aSize2, bool aIsHorizontal);

private:
  int32_t mRowCount;
};

#endif


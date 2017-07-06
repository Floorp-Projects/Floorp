/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIGridPart_h___
#define nsIGridPart_h___

#include "nsISupports.h"

class nsGridRowGroupLayout;
class nsGrid;
class nsGridRowLayout;
class nsGridRow;
class nsGridLayout2;

// 07373ed7-e947-4a5e-b36c-69f7c195677b
#define NS_IGRIDPART_IID \
{ 0x07373ed7, 0xe947, 0x4a5e, \
  { 0xb3, 0x6c, 0x69, 0xf7, 0xc1, 0x95, 0x67, 0x7b } }

/**
 * An additional interface implemented by nsBoxLayout implementations
 * for parts of a grid (excluding cells, which are not special).
 */
class nsIGridPart : public nsISupports {

public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IGRIDPART_IID)

  virtual nsGridRowGroupLayout* CastToRowGroupLayout()=0;
  virtual nsGridLayout2* CastToGridLayout()=0;

  /**
   * @param aBox [IN] The other half of the |this| parameter, i.e., the box
   *                  whose layout manager is |this|.
   * @param aIndex [INOUT] For callers not setting aRequestor, the value
   *                       pointed to by aIndex is incremented by the index
   *                       of the row (aBox) within its row group; if aBox
   *                       is not a row/column, it is untouched.
   *                       The implementation does this by doing the aIndex
   *                       incrementing in the call to the parent row group
   *                       when aRequestor is non-null.
   * @param aRequestor [IN] Non-null if and only if this is a recursive
   *                   call from the GetGrid method on a child grid part,
   *                   in which case it is a pointer to that grid part.
   *                   (This may only be non-null for row groups and
   *                   grids.)
   * @return The grid of which aBox (a row, row group, or grid) is a part.
   */
  virtual nsGrid* GetGrid(nsIFrame* aBox, int32_t* aIndex, nsGridRowLayout* aRequestor=nullptr)=0;

  /**
   * @param aBox [IN] The other half of the |this| parameter, i.e., the box
   *                  whose layout manager is |this|.
   * @param aParentBox [OUT] The box representing the next level up in
   *                   the grid (i.e., row group for a row, grid for a
   *                   row group).
   * @returns The layout manager for aParentBox.
   */
  virtual nsIGridPart* GetParentGridPart(nsIFrame* aBox, nsIFrame** aParentBox) = 0;

  /**
   * @param aBox [IN] The other half of the |this| parameter, i.e., the box
   *                  whose layout manager is |this|.
   * @param aRowCount [INOUT] Row count
   * @param aComputedColumnCount [INOUT] Column count
   */
  virtual void CountRowsColumns(nsIFrame* aBox, int32_t& aRowCount, int32_t& aComputedColumnCount)=0;
  virtual void DirtyRows(nsIFrame* aBox, nsBoxLayoutState& aState)=0;
  virtual int32_t BuildRows(nsIFrame* aBox, nsGridRow* aRows)=0;
  virtual nsMargin GetTotalMargin(nsIFrame* aBox, bool aIsHorizontal)=0;
  virtual int32_t GetRowCount() { return 1; }

  /**
   * Return the level of the grid hierarchy this grid part represents.
   */
  enum Type { eGrid, eRowGroup, eRowLeaf };
  virtual Type GetType()=0;

  /**
   * Return whether this grid part is an appropriate parent for the argument.
   */
  bool CanContain(nsIGridPart* aPossibleChild) {
    Type thisType = GetType(), childType = aPossibleChild->GetType();
    return thisType + 1 == childType || (thisType == eRowGroup && childType == eRowGroup);
  }

};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIGridPart, NS_IGRIDPART_IID)

#endif


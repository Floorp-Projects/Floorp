/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsIGridPart_h___
#define nsIGridPart_h___

#include "nsISupports.h"
#include "nsFrame.h"

class nsGridRowGroupLayout;
class nsGrid;
class nsGridRowLayout;
class nsGridRow;
class nsGridLayout2;

// adaee669-f8db-42d7-8817-a2299a341404
#define NS_IGRIDPART_IID \
{ 0xadaee669, 0xf8db, 0x42d7, \
  { 0x88, 0x17, 0xa2, 0x29, 0x9a, 0x34, 0x14, 0x04 } }

/**
 * An additional interface implemented by nsIBoxLayout implementations
 * for parts of a grid (excluding cells, which are not special).
 */
class nsIGridPart : public nsISupports {

public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IGRIDPART_IID)

  NS_IMETHOD CastToRowGroupLayout(nsGridRowGroupLayout** aRowGroup)=0;
  NS_IMETHOD CastToGridLayout(nsGridLayout2** aGrid)=0;

  /**
   * @param aBox [IN] The other half of the |this| parameter, i.e., the box
   *                  whose layout manager is |this|.
   * @param aList [OUT] The grid of which aBox (a row, row group, or grid)
   *                    is a part.
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
   */
  NS_IMETHOD GetGrid(nsIBox* aBox, nsGrid** aList, PRInt32* aIndex, nsGridRowLayout* aRequestor=nsnull)=0;

  /**
   * @param aBox [IN] The other half of the |this| parameter, i.e., the box
   *                  whose layout manager is |this|.
   * @param aParentBox [OUT] The box representing the next level up in
   *                   the grid (i.e., row group for a row, grid for a
   *                   row group).
   * @param aParentGridRow [OUT] The layout manager for aParentBox.
   */
  NS_IMETHOD GetParentGridPart(nsIBox* aBox, nsIBox** aParentBox, nsIGridPart** aParentGridRow)=0;

  NS_IMETHOD CountRowsColumns(nsIBox* aBox, PRInt32& aRowCount, PRInt32& aComputedColumnCount)=0;
  NS_IMETHOD DirtyRows(nsIBox* aBox, nsBoxLayoutState& aState)=0;
  NS_IMETHOD BuildRows(nsIBox* aBox, nsGridRow* aRows, PRInt32* aCount)=0;
  NS_IMETHOD GetTotalMargin(nsIBox* aBox, nsMargin& aMargin, PRBool aIsHorizontal)=0;
  NS_IMETHOD GetRowCount(PRInt32& aRowCount)=0;
  
  /**
   * Return the level of the grid hierarchy this grid part represents.
   */
  enum Type { eGrid, eRowGroup, eRowLeaf };
  NS_IMETHOD_(Type) GetType()=0;

  /**
   * Return whether this grid part is an appropriate parent for the argument.
   */
  PRBool CanContain(nsIGridPart* aPossibleChild) {
    Type thisType = GetType(), childType = aPossibleChild->GetType();
    return thisType + 1 == childType || (thisType == eRowGroup && childType == eRowGroup);
  }

};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIGridPart, NS_IGRIDPART_IID)

#endif


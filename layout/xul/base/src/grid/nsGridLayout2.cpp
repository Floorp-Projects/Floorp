/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsGridLayout2.h"
#include "nsGridRowGroupLayout.h"
#include "nsBox.h"
#include "nsIScrollableFrame.h"
#include "nsSprocketLayout.h"

nsresult
NS_NewGridLayout2( nsIPresShell* aPresShell, nsIBoxLayout** aNewLayout)
{
  *aNewLayout = new nsGridLayout2(aPresShell);
  NS_IF_ADDREF(*aNewLayout);

  return NS_OK;
  
} 

nsGridLayout2::nsGridLayout2(nsIPresShell* aPresShell):nsStackLayout()
{
}

NS_IMETHODIMP
nsGridLayout2::Layout(nsIBox* aBox, nsBoxLayoutState& aBoxLayoutState)
{
  mGrid.SetBox(aBox);
  nsresult rv = nsStackLayout::Layout(aBox, aBoxLayoutState);
#ifdef DEBUG_grid
  mGrid.PrintCellMap();
#endif
  return rv;
}

NS_IMETHODIMP
nsGridLayout2::GetGrid(nsIBox* aBox, nsGrid** aGrid, PRInt32* aIndex, nsGridRowLayout* aRequestor)
{
  mGrid.SetBox(aBox);
  *aGrid = &mGrid;
  return NS_OK;
}

NS_IMETHODIMP
nsGridLayout2::GetParentGridPart(nsIBox* aBox, nsIBox** aParentBox, nsIGridPart** aParentGridRow)
{
  NS_ERROR("Should not be called");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGridLayout2::GetMinSize(nsIBox* aBox, nsBoxLayoutState& aState, nsSize& aSize)
{
  return nsStackLayout::GetMinSize(aBox, aState, aSize);
}

NS_IMETHODIMP
nsGridLayout2::GetPrefSize(nsIBox* aBox, nsBoxLayoutState& aState, nsSize& aSize)
{
  return nsStackLayout::GetPrefSize(aBox, aState, aSize); 
}

NS_IMETHODIMP
nsGridLayout2::GetMaxSize(nsIBox* aBox, nsBoxLayoutState& aState, nsSize& aSize)
{
  return nsStackLayout::GetMaxSize(aBox, aState, aSize); 
}

NS_IMETHODIMP
nsGridLayout2::CountRowsColumns(nsIBox* aRowBox, PRInt32& aRowCount, PRInt32& aComputedColumnCount)
{
  NS_ERROR("Should not be called");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGridLayout2::DirtyRows(nsIBox* aBox, nsBoxLayoutState& aState)
{
  NS_ERROR("Should not be called");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGridLayout2::BuildRows(nsIBox* aBox, nsGridRow* aRows, PRInt32* aCount)
{
  NS_ERROR("Should not be called");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGridLayout2::CastToRowGroupLayout(nsGridRowGroupLayout** aRowGroup)
{
  (*aRowGroup) = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsGridLayout2::CastToGridLayout(nsGridLayout2** aGridLayout)
{
  (*aGridLayout) = this;
  return NS_OK;
}

NS_IMETHODIMP
nsGridLayout2::GetTotalMargin(nsIBox* aBox, nsMargin& aMargin, PRBool aIsRow)
{
  aMargin.left = 0;
  aMargin.right = 0;
  aMargin.top = 0;
  aMargin.bottom = 0;

  return NS_OK;
}

NS_IMETHODIMP
nsGridLayout2::GetRowCount(PRInt32& aRowCount)
{
  NS_ERROR("Should not be called");
  return NS_OK;
}


NS_IMPL_ADDREF_INHERITED(nsGridLayout2, nsStackLayout);
NS_IMPL_RELEASE_INHERITED(nsGridLayout2, nsStackLayout);

NS_INTERFACE_MAP_BEGIN(nsGridLayout2)
  NS_INTERFACE_MAP_ENTRY(nsIGridPart)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIGridPart)
NS_INTERFACE_MAP_END_INHERITING(nsStackLayout)

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

#include "nsGridRowLeafFrame.h"
#include "nsGridRowLeafLayout.h"
#include "nsGridRow.h"
#include "nsBoxLayoutState.h"
#include "nsGridLayout2.h"

nsresult
NS_NewGridRowLeafFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRBool aIsRoot, nsIBoxLayout* aLayoutManager)
{
    NS_PRECONDITION(aNewFrame, "null OUT ptr");
    if (nsnull == aNewFrame) {
        return NS_ERROR_NULL_POINTER;
    }
    nsGridRowLeafFrame* it = new (aPresShell) nsGridRowLeafFrame (aPresShell, aIsRoot, aLayoutManager);
    if (nsnull == it)
        return NS_ERROR_OUT_OF_MEMORY;

    *aNewFrame = it;
    return NS_OK;

} 

nsGridRowLeafFrame::nsGridRowLeafFrame(nsIPresShell* aPresShell, PRBool aIsRoot, nsIBoxLayout* aLayoutManager)
:nsBoxFrame(aPresShell, aIsRoot, aLayoutManager)
{

}

/*
 * Our border and padding could be affected by our columns or rows
 * Lets go check it out.
 */
NS_IMETHODIMP
nsGridRowLeafFrame::GetBorderAndPadding(nsMargin& aBorderAndPadding)
{
  // if our columns have made our padding larger add it in.
  nsMargin borderPadding(0,0,0,0);
  nsresult rv = nsBoxFrame::GetBorderAndPadding(aBorderAndPadding);

  nsCOMPtr<nsIBoxLayout> layout;
  GetLayoutManager(getter_AddRefs(layout));
  if (!layout)
    return rv;

  nsCOMPtr<nsIGridPart> part = do_QueryInterface(layout);
  if (!part)
    return rv;
    
  nsGrid* grid = nsnull;
  PRInt32 index = 0;
  part->GetGrid(this, &grid, &index);

  if (!grid) 
    return rv;

  PRInt32 isHorizontal = IsHorizontal();

  nsBoxLayoutState state((nsIPresContext*)nsnull);

  PRInt32 firstIndex = 0;
  PRInt32 lastIndex = 0;
  nsGridRow* firstRow = nsnull;
  nsGridRow* lastRow = nsnull;
  grid->GetFirstAndLastRow(state, firstIndex, lastIndex, firstRow, lastRow, isHorizontal);

  // only the first and last rows can be affected.
  if (firstRow && firstRow->GetBox() == this) {
    
    nscoord top = 0;
    nscoord bottom = 0;
    grid->GetRowOffsets(state, firstIndex, top, bottom, isHorizontal);

    if (isHorizontal) {
      if (top > aBorderAndPadding.top)
        aBorderAndPadding.top = top;
    } else {
      if (top > aBorderAndPadding.left)
        aBorderAndPadding.left = top;
    } 
  }

  if (lastRow && lastRow->GetBox() == this) {
    
    nscoord top = 0;
    nscoord bottom = 0;
    grid->GetRowOffsets(state, lastIndex, top, bottom, isHorizontal);

    if (isHorizontal) {
      if (bottom > aBorderAndPadding.bottom)
        aBorderAndPadding.bottom = bottom;
    } else {
      if (bottom > aBorderAndPadding.right)
        aBorderAndPadding.right = bottom;
    }
    
  }  
  
  return rv;
}



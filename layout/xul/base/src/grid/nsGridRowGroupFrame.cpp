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

#include "nsGridRowGroupFrame.h"
#include "nsGridRowLeafLayout.h"
#include "nsGridRow.h"
#include "nsBoxLayoutState.h"
#include "nsGridLayout2.h"

nsresult
NS_NewGridRowGroupFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRBool aIsRoot, nsIBoxLayout* aLayoutManager)
{
    NS_PRECONDITION(aNewFrame, "null OUT ptr");
    if (nsnull == aNewFrame) {
        return NS_ERROR_NULL_POINTER;
    }
    nsGridRowGroupFrame* it = new (aPresShell) nsGridRowGroupFrame (aPresShell, aIsRoot, aLayoutManager);
    if (nsnull == it)
        return NS_ERROR_OUT_OF_MEMORY;

    *aNewFrame = it;
    return NS_OK;

} 

nsGridRowGroupFrame::nsGridRowGroupFrame(nsIPresShell* aPresShell, PRBool aIsRoot, nsIBoxLayout* aLayoutManager)
:nsBoxFrame(aPresShell, aIsRoot, aLayoutManager)
{

}

/**
 * This is redefined because row groups have a funny property. If they are flexible
 * then their flex must be equal to the sum of their children's flexes.
 */
NS_IMETHODIMP
nsGridRowGroupFrame::GetFlex(nsBoxLayoutState& aState, nscoord& aFlex)
{
  // if we are flexible out flexibility is determined by our columns.
  // so first get the our flex. If not 0 then our flex is the sum of
  // our columns flexes.

  if (!DoesNeedRecalc(mFlex)) {
     aFlex = mFlex;
     return NS_OK;
  }

  nsBoxFrame::GetFlex(aState, aFlex);


  if (aFlex == 0)
    return NS_OK;

  // ok we are flexible add up our children
  nscoord totalFlex = 0;
  nsIBox* child = nsnull;
  GetChildBox(&child);
  while (child)
  {
    PRInt32 flex = 0;
    child->GetFlex(aState, flex);
    totalFlex += flex;;
    child->GetNextBox(&child);
  }

  aFlex = totalFlex;
  mFlex = aFlex;

  return NS_OK;
}



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

#include "nsGridLayout.h"
#include "nsTempleLayout.h"
#include "nsIBox.h"
#include "nsIScrollableFrame.h"

nsresult
NS_NewGridLayout( nsIPresShell* aPresShell, nsCOMPtr<nsIBoxLayout>& aNewLayout)
{
  aNewLayout = new nsGridLayout(aPresShell);

  return NS_OK;
  
} 

nsGridLayout::nsGridLayout(nsIPresShell* aPresShell):nsStackLayout()
{
}

/**
 * Get the monuments in the other temple at the give monument index
 */
NS_IMETHODIMP
nsGridLayout::GetOtherMonumentsAt(nsIBox* aBox, PRInt32 aIndexOfObelisk, nsBoxSizeList** aList, nsMonumentLayout* aRequestor)
{
  nsTempleLayout* temple = nsnull;
  nsIBox* templeBox = nsnull;
  GetOtherTemple(aBox, &temple, &templeBox, aRequestor);
  if (temple)
  {
    nsresult rv = temple->GetMonumentsAt(templeBox, aIndexOfObelisk, aList);
    NS_RELEASE(temple);
    return rv;
  }
  else
    *aList = nsnull;

  return NS_OK;
}

/**
 * a Grid always has 2 temples. This is called by one temple to get the other
 */
NS_IMETHODIMP
nsGridLayout::GetOtherTemple(nsIBox* aBox, nsTempleLayout** aTemple, nsIBox** aTempleBox, nsMonumentLayout* aRequestor)
{
  // this is really easy. We know the index of the temple who requested
  // so find our other temple who doesn't have that index.

  nsIBox* child = nsnull;
  aBox->GetChildBox(&child);

  while(child)
  {
    nsIBox* oldBox = child;
    nsresult rv = NS_OK;
    nsCOMPtr<nsIScrollableFrame> scrollFrame = do_QueryInterface(child, &rv);
    if (scrollFrame) {
       nsIFrame* scrolledFrame = nsnull;
       scrollFrame->GetScrolledFrame(nsnull, scrolledFrame);
       NS_ASSERTION(scrolledFrame,"Error no scroll frame!!");
       nsCOMPtr<nsIBox> b = do_QueryInterface(scrolledFrame);
       child = b;
    }

    nsCOMPtr<nsIBoxLayout> layout;
    child->GetLayoutManager(getter_AddRefs(layout));

    // must find a temple that is not our requestor and is a monument.
    if (layout != aRequestor) {
       nsCOMPtr<nsIMonument> monument( do_QueryInterface(layout) );
       if (monument)
       {
         nsTempleLayout* temple = nsnull;
         monument->CastToTemple(&temple);
         if (temple) {
            // yes its a temple. 
            *aTemple = temple;
            *aTempleBox = child;
            NS_ADDREF(temple);
            return NS_OK;
         }
       }
    }

    if (scrollFrame) {
      child = oldBox;
    }

    child->GetNextBox(&child);
  }

  *aTemple = nsnull;

  return NS_OK;
}


NS_IMETHODIMP
nsGridLayout::CastToTemple(nsTempleLayout** aTemple)
{
  *aTemple = nsnull;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGridLayout::CastToObelisk(nsObeliskLayout** aObelisk)
{
  *aObelisk = nsnull;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGridLayout::CastToGrid(nsGridLayout** aGrid)
{
  *aGrid = this;
  return NS_OK;
}

NS_IMETHODIMP
nsGridLayout::GetParentMonument(nsIBox* aBox, nsCOMPtr<nsIBox>& aParentBox, nsIMonument** aParentMonument)
{
  NS_ERROR("Should not be called");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGridLayout::GetOtherMonuments(nsIBox* aBox, nsBoxSizeList** aList)
{
  NS_ERROR("Should not be called");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGridLayout::GetMonumentsAt(nsIBox* aBox, PRInt32 aMonumentIndex, nsBoxSizeList** aList)
{
  NS_ERROR("Should not be called");
  return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsGridLayout::BuildBoxSizeList(nsIBox* aBox, nsBoxLayoutState& aState, nsBoxSize*& aFirst, nsBoxSize*& aLast, PRBool aIsHorizontal)
{
  NS_ERROR("Should not be called");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGridLayout::GetMonumentList(nsIBox* aBox, nsBoxLayoutState& aState, nsBoxSizeList** aList)
{
  NS_ERROR("Should not be called");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGridLayout::EnscriptionChanged(nsBoxLayoutState& aState, PRInt32 aIndex)
{
  NS_ERROR("Should Never be Called!");
  return NS_OK;
}

NS_IMETHODIMP
nsGridLayout::DesecrateMonuments(nsIBox* aBox, nsBoxLayoutState& aState)
{
  return NS_OK;
}

NS_IMPL_ADDREF_INHERITED(nsGridLayout, nsStackLayout);
NS_IMPL_RELEASE_INHERITED(nsGridLayout, nsStackLayout);

NS_INTERFACE_MAP_BEGIN(nsGridLayout)
  NS_INTERFACE_MAP_ENTRY(nsIMonument)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIMonument)
NS_INTERFACE_MAP_END_INHERITING(nsStackLayout)

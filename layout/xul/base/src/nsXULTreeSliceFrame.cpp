/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

#include "nsCOMPtr.h"
#include "nsXULTreeSliceFrame.h"

//
// NS_NewXULTreeSliceFrame
//
// Creates a new TreeSlice frame
//
nsresult
NS_NewXULTreeSliceFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRBool aIsRoot, 
                        nsIBoxLayout* aLayoutManager)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsXULTreeSliceFrame* it = new (aPresShell) nsXULTreeSliceFrame(aPresShell, aIsRoot, aLayoutManager);
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewXULTreeSliceFrame


NS_IMETHODIMP_(nsrefcnt) 
nsXULTreeSliceFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt)
nsXULTreeSliceFrame::Release(void)
{
  return NS_OK;
}

//
// QueryInterface
//
NS_INTERFACE_MAP_BEGIN(nsXULTreeSliceFrame)
  NS_INTERFACE_MAP_ENTRY(nsIXULTreeSlice)
NS_INTERFACE_MAP_END_INHERITING(nsBoxFrame)

// Constructor
nsXULTreeSliceFrame::nsXULTreeSliceFrame(nsIPresShell* aPresShell, PRBool aIsRoot, nsIBoxLayout* aLayoutManager)
:nsBoxFrame(aPresShell, aIsRoot, aLayoutManager) 
{}

// Destructor
nsXULTreeSliceFrame::~nsXULTreeSliceFrame()
{
}

nsresult
nsXULTreeSliceFrame::GetPrefSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  nsresult rv = nsBoxFrame::GetPrefSize(aState, aSize);
  if (NS_FAILED(rv)) return rv;

  aSize.height = PR_MAX(mRect.height, aSize.height);
  return NS_OK;
}

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
 * Contributor(s): 
 */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsSpringFrame.h"
#include "nsIStyleContext.h"

nsresult
NS_NewSpringFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame )
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsSpringFrame* it = new (aPresShell) nsSpringFrame (aPresShell);
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewSpringFrame

NS_IMETHODIMP  nsSpringFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                             const nsPoint& aPoint, 
                                             nsFramePaintLayer aWhichLayer,    
                                             nsIFrame**     aFrame)
{
  if (!mRect.Contains(aPoint)) 
    return NS_ERROR_FAILURE;

  /*
  // see if it is in our border, padding, or inset
  nsRect r(mRect);
  nsMargin m;
  GetInset(m);
  r.Deflate(m);
  GetBorderAndPadding(m);
  r.Deflate(m);
  if (!r.Contains(aPoint)) {
  */
    *aFrame = this;
    /*
    return NS_OK;
  }
  */

  return NS_OK;
}

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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Author: Eric D Vaughan (evaughan@netscape.com)
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsScrollPortFrame.h"
#include "nsIFormControlFrame.h"


nsresult
NS_NewScrollPortFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsScrollPortFrame* it = new (aPresShell) nsScrollPortFrame (aPresShell);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

PRBool
nsScrollPortFrame::NeedsClipWidget()
{
    // XXX: This code will go away when a general solution for creating
    // widgets only when needed is implemented.
  nsIFrame* parentFrame;
  GetParent(&parentFrame);
  nsIFormControlFrame* fcFrame;

  while (parentFrame) {
    if ((NS_SUCCEEDED(parentFrame->QueryInterface(NS_GET_IID(nsIFormControlFrame), (void**)&fcFrame)))) {
      return(PR_FALSE);
    }
    parentFrame->GetParent(&parentFrame); 
  }
 
  return PR_TRUE;
}

#ifdef NS_DEBUG
NS_IMETHODIMP
nsScrollPortFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("ScrollPortFrame", aResult);
}
#endif

nsScrollPortFrame::nsScrollPortFrame(nsIPresShell* aShell):nsScrollBoxFrame(aShell)
{
}

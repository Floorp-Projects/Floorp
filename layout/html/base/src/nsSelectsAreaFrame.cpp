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
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#include "nsSelectsAreaFrame.h"
#include "nsCOMPtr.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIContent.h"
#include "nsIStyleContext.h"

nsresult
NS_NewSelectsAreaFrame(nsIPresShell* aShell, nsIFrame** aNewFrame, PRUint32 aFlags)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsSelectsAreaFrame* it = new (aShell) nsSelectsAreaFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  it->SetFlags(aFlags);
  *aNewFrame = it;
  return NS_OK;
}

/*NS_IMETHODIMP
nsSelectsAreaFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kAreaFrameIID)) {
    nsIAreaFrame* tmp = (nsIAreaFrame*)this;
    *aInstancePtr = (void*)tmp;
    return NS_OK;
  }
  return nsAreaFrame::QueryInterface(aIID, aInstancePtr);
}
*/

//---------------------------------------------------------
PRBool 
nsSelectsAreaFrame::IsOptionElement(nsIContent* aContent)
{
  PRBool result = PR_FALSE;
 
  nsCOMPtr<nsIDOMHTMLOptionElement> optElem;
  if (NS_SUCCEEDED(aContent->QueryInterface(NS_GET_IID(nsIDOMHTMLOptionElement),(void**) getter_AddRefs(optElem)))) {      
    if (optElem != nsnull) {
      result = PR_TRUE;
    }
  }
 
  return result;
}

//---------------------------------------------------------
PRBool 
nsSelectsAreaFrame::IsOptionElementFrame(nsIFrame *aFrame)
{
  nsIContent *content = nsnull;
  aFrame->GetContent(&content);
  PRBool result = PR_FALSE;
  if (nsnull != content) {
    result = IsOptionElement(content);
    NS_RELEASE(content);
  }
  return(result);
}

//---------------------------------------------------------
NS_IMETHODIMP
nsSelectsAreaFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                     const nsPoint& aPoint,
                                     nsFramePaintLayer aWhichLayer,
                                     nsIFrame** aFrame)
{

  PRBool inThisFrame = mRect.Contains(aPoint);

  if (!((mState & NS_FRAME_OUTSIDE_CHILDREN) || inThisFrame )) {
    return NS_ERROR_FAILURE;
  }

  nsresult result = nsAreaFrame::GetFrameForPoint(aPresContext, aPoint, aWhichLayer, aFrame);

  if (result == NS_OK) {
    nsIFrame* selectedFrame = *aFrame;
    while ((nsnull != selectedFrame) && (PR_FALSE == IsOptionElementFrame(selectedFrame))) {
      selectedFrame->GetParent(&selectedFrame);
    }  
    if (nsnull != selectedFrame) {
      *aFrame = selectedFrame;
    }
    // else, keep the original result as *aFrame, which could be this frame
  }

  return result;
}

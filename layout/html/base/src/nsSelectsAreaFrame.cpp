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
 */
#include "nsSelectsAreaFrame.h"
#include "nsCOMPtr.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIContent.h"
#include "nsIAreaFrame.h"

static NS_DEFINE_IID(kAreaFrameIID, NS_IAREAFRAME_IID);

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
  if (NS_SUCCEEDED(aContent->QueryInterface(nsCOMTypeInfo<nsIDOMHTMLOptionElement>::GetIID(),(void**) getter_AddRefs(optElem)))) {      
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
                                     nsIFrame** aFrame)
{
  nsAreaFrame::GetFrameForPoint(aPresContext, aPoint, aFrame);

  nsIFrame* selectedFrame = *aFrame;
  
  while ((nsnull != selectedFrame) && (PR_FALSE == IsOptionElementFrame(selectedFrame))) {
    selectedFrame->GetParent(&selectedFrame);
  }  
  if (nsnull != selectedFrame) {
    *aFrame = selectedFrame;
  }

  return NS_OK;
}

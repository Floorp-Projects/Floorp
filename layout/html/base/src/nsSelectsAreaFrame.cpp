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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsSelectsAreaFrame.h"
#include "nsCOMPtr.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIContent.h"
#include "nsListControlFrame.h"

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
  // We need NS_BLOCK_SPACE_MGR to ensure that the options inside the select
  // aren't expanded by right floats outside the select.
  it->SetFlags(aFlags | NS_BLOCK_SPACE_MGR);
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
  nsIContent *content = aFrame->GetContent();
  if (content) {
    return IsOptionElement(content);
  }
  return PR_FALSE;
}

//---------------------------------------------------------
NS_IMETHODIMP
nsSelectsAreaFrame::GetFrameForPoint(nsPresContext* aPresContext,
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
    while (selectedFrame && !IsOptionElementFrame(selectedFrame)) {
      selectedFrame = selectedFrame->GetParent();
    }  
    if (selectedFrame) {
      *aFrame = selectedFrame;
    }
    // else, keep the original result as *aFrame, which could be this frame
  }

  return result;
}

NS_IMETHODIMP
nsSelectsAreaFrame::Paint(nsPresContext*      aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect&        aDirtyRect,
                          nsFramePaintLayer    aWhichLayer,
                          PRUint32             aFlags)
{
  nsAreaFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer, aFlags);

  nsIFrame* frame = this;
  while (frame) {
    frame = frame->GetParent();
    if (frame->GetType() == nsLayoutAtoms::listControlFrame) {
      nsListControlFrame* listFrame = NS_STATIC_CAST(nsListControlFrame*, frame);
      listFrame->PaintFocus(aRenderingContext, aWhichLayer);
      return NS_OK;
    }
  }

  return NS_OK;
}

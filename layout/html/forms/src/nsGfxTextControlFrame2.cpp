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

#include "nsGfxTextControlFrame2.h"
#include "nsFormFrame.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIHTMLContent.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"
#include "nsIPresState.h"
#include "nsIFileWidget.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"
#include "nsIView.h"
#include "nsHTMLParts.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIFormControl.h"
#include "nsINameSpaceManager.h"
#include "nsCOMPtr.h"
#include "nsFileSpec.h"
#include "nsISupportsArray.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsDocument.h"
#include "nsIDOMMouseListener.h"
#include "nsIPresShell.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIStatefulFrame.h"
#include "nsISupportsPrimitives.h"
#include "nsIComponentManager.h"
#include "nsIElementFactory.h"
#include "nsIServiceManager.h"


static NS_DEFINE_IID(kIAnonymousContentCreatorIID,     NS_IANONYMOUS_CONTENT_CREATOR_IID);

nsresult
NS_NewGfxTextControlFrame2(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsGfxTextControlFrame2* it = new (aPresShell) nsGfxTextControlFrame2();
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

NS_IMPL_QUERY_INTERFACE_INHERITED1(nsGfxTextControlFrame2, nsHTMLContainerFrame,nsIAnonymousContentCreator);
NS_IMPL_ADDREF_INHERITED(nsGfxTextControlFrame2, nsHTMLContainerFrame);
NS_IMPL_RELEASE_INHERITED(nsGfxTextControlFrame2, nsHTMLContainerFrame);
 

nsGfxTextControlFrame2::nsGfxTextControlFrame2()
{
}

nsGfxTextControlFrame2::~nsGfxTextControlFrame2()
{
}

NS_IMETHODIMP
nsGfxTextControlFrame2::CreateAnonymousContent(nsIPresContext* aPresContext,
                                           nsISupportsArray& aChildList)
{
  // create horzontal scrollbar
  nsCAutoString progID = NS_ELEMENT_FACTORY_PROGID_PREFIX;
  progID += "http://www.w3.org/TR/REC-html40";
  nsresult rv;
  NS_WITH_SERVICE(nsIElementFactory, elementFactory, progID, &rv);
  if (!elementFactory)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> content;
  elementFactory->CreateInstanceByTag(NS_ConvertToString("div"), getter_AddRefs(content));
  aChildList.AppendElement(content);
//create editor
//create selection
//init editor with div.
  return NS_OK;
}

NS_IMETHODIMP nsGfxTextControlFrame2::Reflow(nsIPresContext*          aPresContext, 
                                         nsHTMLReflowMetrics&     aDesiredSize,
                                         const nsHTMLReflowState& aReflowState, 
                                         nsReflowStatus&          aStatus)
{
  // assuming 1 child
  nsIFrame* child = mFrames.FirstChild();
  //mFrames.FirstChild(aPresContext,nsnull,&child);
  if (!child)
    return nsHTMLContainerFrame::Reflow(aPresContext,aDesiredSize,aReflowState,aStatus);
  nsSize availSize(aReflowState.availableWidth, aReflowState.availableHeight);
  nsHTMLReflowState kidReflowState(aPresContext, aReflowState, child,
                                   availSize);

  kidReflowState.mComputedWidth = aReflowState.mComputedWidth;
  kidReflowState.mComputedHeight = aReflowState.mComputedHeight;

  if (aReflowState.reason == eReflowReason_Incremental)
  {
    if (aReflowState.reflowCommand) {
       nsIFrame* incrementalChild = nsnull;
       aReflowState.reflowCommand->GetNext(incrementalChild);
       NS_ASSERTION(incrementalChild == child, "Child is not in our list!!");
    }
  }

  nsresult rv = ReflowChild(child, aPresContext, aDesiredSize, kidReflowState, 0, 0, 0, aStatus);

  return rv;
}


PRIntn
nsGfxTextControlFrame2::GetSkipSides() const
{
  return 0;
}


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
#include "nsHTMLParts.h"
#include "nsContainerFrame.h"
#include "nsCSSRendering.h"
#include "nsIDocument.h"
#include "nsIReflowCommand.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsViewsCID.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIWidget.h"
#include "nsHTMLIIDs.h"
#include "nsPageFrame.h"
#include "nsIRenderingContext.h"
#include "nsGUIEvent.h"
#include "nsIDOMEvent.h"
#include "nsStyleConsts.h"
#include "nsIViewManager.h"
#include "nsHTMLAtoms.h"
#include "nsIEventStateManager.h"
#include "nsIDeviceContext.h"
#include "nsIScrollableView.h"
#include "nsLayoutAtoms.h"
#include "nsIPresShell.h"
#include "nsBoxFrame.h"
#include "nsStackLayout.h"
#include "nsIRootBox.h"
#include "nsIAnonymousContentCreator.h"
#include "nsIElementFactory.h"
#include "nsINodeInfo.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"

// Interface IDs

//#define DEBUG_REFLOW

class nsDocElementBoxFrame : public nsBoxFrame, public nsIAnonymousContentCreator {
public:

  friend nsresult NS_NewBoxFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  nsDocElementBoxFrame(nsIPresShell* aShell);

  NS_DECL_ISUPPORTS_INHERITED

  // nsIAnonymousContentCreator
  NS_IMETHOD CreateAnonymousContent(nsIPresContext* aPresContext,
                                    nsISupportsArray& aAnonymousItems);
  NS_IMETHOD CreateFrameFor(nsIPresContext*   aPresContext,
                            nsIContent *      aContent,
                            nsIFrame**        aFrame) { if (aFrame) *aFrame = nsnull; return NS_ERROR_FAILURE; }

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;
#endif
};

//----------------------------------------------------------------------

nsresult
NS_NewDocElementBoxFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsDocElementBoxFrame* it = new (aPresShell) nsDocElementBoxFrame (aPresShell);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *aNewFrame = it;

  return NS_OK;
}

nsDocElementBoxFrame::nsDocElementBoxFrame(nsIPresShell* aShell):nsBoxFrame(aShell, PR_TRUE)
{
}

NS_IMETHODIMP
nsDocElementBoxFrame::CreateAnonymousContent(nsIPresContext* aPresContext,
                                       nsISupportsArray& aAnonymousItems)
{
  nsresult rv;
  nsCOMPtr<nsIElementFactory> elementFactory = 
           do_GetService(NS_ELEMENT_FACTORY_CONTRACTID_PREFIX "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul", &rv);
  if (!elementFactory)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsINodeInfoManager> nodeInfoManager;
  nsCOMPtr<nsIDocument> doc;
  mContent->GetDocument(*getter_AddRefs(doc));
  doc->GetNodeInfoManager(*getter_AddRefs(nodeInfoManager));
  NS_ENSURE_TRUE(nodeInfoManager, NS_ERROR_FAILURE);

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nodeInfoManager->GetNodeInfo(NS_ConvertASCIItoUCS2("popupgroup"), nsString(), NS_ConvertASCIItoUCS2("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"),
    *getter_AddRefs(nodeInfo));

  nsCOMPtr<nsIContent> content;
  elementFactory->CreateInstanceByTag(nodeInfo, getter_AddRefs(content));

  aAnonymousItems.AppendElement(content);
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt) 
nsDocElementBoxFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt)
nsDocElementBoxFrame::Release(void)
{
  return NS_OK;
}

NS_INTERFACE_MAP_BEGIN(nsDocElementBoxFrame)
  NS_INTERFACE_MAP_ENTRY(nsIAnonymousContentCreator)
NS_INTERFACE_MAP_END_INHERITING(nsBoxFrame)

#ifdef DEBUG
NS_IMETHODIMP
nsDocElementBoxFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("DocElementBox", aResult);
}

NS_IMETHODIMP
nsDocElementBoxFrame::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = sizeof(*this);
  return NS_OK;
}
#endif

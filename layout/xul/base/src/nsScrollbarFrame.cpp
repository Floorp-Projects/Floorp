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

#include "nsScrollbarFrame.h"
#include "nsScrollbarButtonFrame.h"
#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsISupportsArray.h"
#include "nsXULAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsIXMLContent.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDOMElement.h"
#include "nsXMLElement.h"
#include "nsIStyledContent.h"
#include "nsGenericElement.h"
#include "nsIStyleRule.h"
#include "nsHTMLValue.h"
#include "nsIAnonymousContent.h"
#include "nsIView.h"


class AnonymousElement : public nsXMLElement, public nsIAnonymousContent
{
public:
  AnonymousElement() {}

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus* aEventStatus);
};

#if 0
NS_IMETHODIMP
AnonymousElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const {
  return this->nsXMLElement::SizeOf(aSizer, aResult);
}
#endif

NS_IMETHODIMP
AnonymousElement::HandleDOMEvent(nsIPresContext* aPresContext,
                              nsEvent* aEvent,
                              nsIDOMEvent** aDOMEvent,
                              PRUint32 aFlags,
                              nsEventStatus* aEventStatus)
{
  /*
  // if our parent is not anonymous then we don't want to bubble the event
  // so lets set our parent in nsnull to prevent it. Then we will set it
  // back.
  nsIContent* parent = nsnull;
  GetParent(parent);

  nsCOMPtr<nsIAnonymousContent> anonymousParent(do_QueryInterface(parent));


  if (!anonymousParent) 
    SetParent(nsnull);
*/

  nsresult rv = nsXMLElement::HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                             aFlags, aEventStatus);

  /*
  if (!anonymousParent)
    SetParent(parent);
*/
  return rv;
}

NS_IMPL_ADDREF_INHERITED(AnonymousElement, nsXMLElement)
NS_IMPL_RELEASE_INHERITED(AnonymousElement, nsXMLElement)

//
// QueryInterface
//

NS_INTERFACE_MAP_BEGIN(AnonymousElement)
  NS_INTERFACE_MAP_ENTRY(nsIAnonymousContent)
NS_INTERFACE_MAP_END_INHERITING(nsXMLElement)


nsresult NS_CreateAnonymousNode(nsIContent* aParent, nsIAtom* aTag, PRInt32 aNameSpaceId, nsCOMPtr<nsIContent>& aNewNode)
{
    NS_ENSURE_ARG_POINTER(aParent);

    // create the xml element
    //NS_NewXMLElement(getter_AddRefs(content), aTag);

    nsCOMPtr<nsIDocument> doc;
    aParent->GetDocument(*getter_AddRefs(doc));

    nsCOMPtr<nsINodeInfoManager> nodeInfoManager;
    doc->GetNodeInfoManager(*getter_AddRefs(nodeInfoManager));

    nsCOMPtr<nsINodeInfo> nodeInfo;
    nodeInfoManager->GetNodeInfo(aTag, nsnull, aNameSpaceId,
                                 *getter_AddRefs(nodeInfo));

    nsXMLElement *content = new AnonymousElement();

    if (!content)
      return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = content->Init(nodeInfo);

    if (NS_FAILED(rv)) {
      delete content;

      return rv;
    }

    aNewNode = content;

  /*
    nsCOMPtr<nsIDocument> document;
    aParent->GetDocument(*getter_AddRefs(document));

    nsCOMPtr<nsIDOMDocument> domDocument(do_QueryInterface(document));
    nsCOMPtr<nsIDOMElement> element;
    nsString name;
    aTag->ToString(name);
    domDocument->CreateElement(name, getter_AddRefs(element));
    aNewNode = do_QueryInterface(element);
*/

    return NS_OK;
}


//
// NS_NewToolbarFrame
//
// Creates a new Toolbar frame and returns it in |aNewFrame|
//
nsresult
NS_NewScrollbarFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame )
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsScrollbarFrame* it = new (aPresShell) nsScrollbarFrame (aPresShell);
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewScrollbarFrame


//
// QueryInterface
//
NS_INTERFACE_MAP_BEGIN(nsScrollbarFrame)
  NS_INTERFACE_MAP_ENTRY(nsIScrollbarFrame)
NS_INTERFACE_MAP_END_INHERITING(nsBoxFrame)


NS_IMETHODIMP
nsScrollbarFrame::Init(nsIPresContext*  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIStyleContext* aContext,
              nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  CreateViewForFrame(aPresContext,this,aContext,PR_TRUE);
  nsIView* view;
  GetView(aPresContext, &view);
  view->SetContentTransparency(PR_TRUE);
  return rv;
}


NS_IMETHODIMP
nsScrollbarFrame::AttributeChanged(nsIPresContext* aPresContext,
                               nsIContent* aChild,
                               PRInt32 aNameSpaceID,
                               nsIAtom* aAttribute,
                               PRInt32 aHint)
{
  nsresult rv = nsBoxFrame::AttributeChanged(aPresContext, aChild,
                                              aNameSpaceID, aAttribute, aHint);
  /*// if the current position changes
  if (       aAttribute == nsXULAtoms::curpos || 
             aAttribute == nsXULAtoms::maxpos || 
             aAttribute == nsXULAtoms::pageincrement ||
             aAttribute == nsXULAtoms::increment) {
     // tell the slider its attribute changed so it can 
     // update itself
     nsIFrame* slider;
     nsScrollbarButtonFrame::GetChildWithTag(aPresContext, nsXULAtoms::slider, this, slider);
     if (slider)
        slider->AttributeChanged(aPresContext, aChild, aNameSpaceID, aAttribute, aHint);
  }
*/

  return rv;
}

NS_IMETHODIMP
nsScrollbarFrame::HandlePress(nsIPresContext* aPresContext, 
                     nsGUIEvent*     aEvent,
                     nsEventStatus*  aEventStatus)
{
  return NS_OK;
}

NS_IMETHODIMP
nsScrollbarFrame::HandleMultiplePress(nsIPresContext* aPresContext, 
                     nsGUIEvent*     aEvent,
                     nsEventStatus*  aEventStatus)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsScrollbarFrame::HandleDrag(nsIPresContext* aPresContext, 
                              nsGUIEvent*     aEvent,
                              nsEventStatus*  aEventStatus)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsScrollbarFrame::HandleRelease(nsIPresContext* aPresContext, 
                                 nsGUIEvent*     aEvent,
                                 nsEventStatus*  aEventStatus)
{
  return NS_OK;
}


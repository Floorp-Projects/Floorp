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

#include "nsISupportsArray.h"
#include "nsIDocument.h"
#include "nsXMLElement.h"
#include "nsHTMLValue.h"
#include "nsIAnonymousContent.h"
#include "nsINodeInfo.h"

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

  NS_IMETHOD Init(nsINodeInfo *aInfo);
};

#if 0
NS_IMETHODIMP
AnonymousElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const {
  return this->nsXMLElement::SizeOf(aSizer, aResult);
}
#endif

NS_IMETHODIMP
AnonymousElement::Init(nsINodeInfo *aInfo)
{
  return nsXMLElement::Init(aInfo);
}

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

nsresult NS_NewAnonymousContent2(nsIContent **aNewNode)
{
    NS_ENSURE_ARG_POINTER(aNewNode);
    *aNewNode = nsnull;

    nsIContent *content = new AnonymousElement();

    if (!content)
      return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(content);
    nsresult rv = content->QueryInterface(NS_GET_IID(nsIContent),(void**)aNewNode);
    NS_RELEASE(content);

    return rv;
}

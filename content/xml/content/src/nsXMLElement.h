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
 *   Peter Annema <disttsc@bart.nl>
 */

#ifndef nsXMLElement_h___
#define nsXMLElement_h___

#include "nsCOMPtr.h"
#include "nsIDOMElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIXMLContent.h"
#include "nsIJSScriptObject.h"
#include "nsGenericElement.h"
#include "nsIStyledContent.h"

class nsIEventListenerManager;
class nsIHTMLAttributes;
class nsIURI;
class nsIWebShell;

class nsXMLElement : public nsGenericContainerElement,
                     public nsIDOMElement
{
public:
  nsXMLElement();
  virtual ~nsXMLElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_IDOMNODE_NO_CLONENODE(nsGenericContainerElement::)

  // nsIDOMElement
  NS_FORWARD_IDOMELEMENT(nsGenericContainerElement::)

  // nsIScriptObjectOwner methods
  NS_IMETHOD GetScriptObject(nsIScriptContext* aContext, void** aScriptObject);

  // nsIXMLContent
  NS_IMETHOD SetContainingNameSpace(nsINameSpace* aNameSpace);
  NS_IMETHOD GetContainingNameSpace(nsINameSpace*& aNameSpace) const;
  NS_IMETHOD MaybeTriggerAutoLink(nsIWebShell *aShell);

  // nsIStyledContent
  NS_IMETHOD GetID(nsIAtom*& aResult) const;

  // nsIContent
  NS_IMETHOD SetAttribute(nsINodeInfo *aNodeInfo,
                          const nsAReadableString& aValue,
                          PRBool aNotify);
  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus* aEventStatus);
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;

protected:
  nsresult GetXMLBaseURI(nsIURI **aURI);  // XXX This should perhaps be moved to nsIXMLContent
  PRBool mIsLink;
  nsINameSpace* mNameSpace;
};

#endif // nsXMLElement_h___

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsIDOMHTMLFrameElement.h"
#include "nsIDOMNSHTMLFrameElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIWebNavigation.h"
#include "nsIChromeEventHandler.h"
#include "nsIDocShell.h"
#include "nsDOMError.h"


class nsHTMLFrameElement : public nsGenericHTMLElement,
                           public nsIDOMHTMLFrameElement,
                           public nsIDOMNSHTMLFrameElement,
                           public nsIChromeEventHandler
{
public:
  nsHTMLFrameElement();
  virtual ~nsHTMLFrameElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLFrameElement
  NS_DECL_NSIDOMHTMLFRAMEELEMENT

  // nsIDOMNSHTMLFrameElement
  NS_DECL_NSIDOMNSHTMLFRAMEELEMENT

  // nsIChromeEventHandler
  NS_DECL_NSICHROMEEVENTHANDLER

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAString& aResult) const;
  NS_IMETHOD_(PRBool) HasAttributeDependentStyle(const nsIAtom* aAttribute) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
};

nsresult
NS_NewHTMLFrameElement(nsIHTMLContent** aInstancePtrResult,
                       nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLFrameElement* it = new nsHTMLFrameElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = it->Init(aNodeInfo);

  if (NS_FAILED(rv)) {
    delete it;

    return rv;
  }

  *aInstancePtrResult = NS_STATIC_CAST(nsIHTMLContent *, it);
  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}


nsHTMLFrameElement::nsHTMLFrameElement()
{
}

nsHTMLFrameElement::~nsHTMLFrameElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLFrameElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLFrameElement, nsGenericElement)


// QueryInterface implementation for nsHTMLFrameElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLFrameElement, nsGenericHTMLElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLFrameElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSHTMLFrameElement)
  NS_INTERFACE_MAP_ENTRY(nsIChromeEventHandler)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLFrameElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


nsresult
nsHTMLFrameElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLFrameElement* it = new nsHTMLFrameElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);

  nsresult rv = it->Init(mNodeInfo);

  if (NS_FAILED(rv))
    return rv;

  CopyInnerTo(it, aDeep);

  *aReturn = NS_STATIC_CAST(nsIDOMNode *, it);

  NS_ADDREF(*aReturn);

  return NS_OK;
}


NS_IMPL_STRING_ATTR(nsHTMLFrameElement, FrameBorder, frameborder)
NS_IMPL_URI_ATTR(nsHTMLFrameElement, LongDesc, longdesc)
NS_IMPL_STRING_ATTR(nsHTMLFrameElement, MarginHeight, marginheight)
NS_IMPL_STRING_ATTR(nsHTMLFrameElement, MarginWidth, marginwidth)
NS_IMPL_STRING_ATTR(nsHTMLFrameElement, Name, name)
NS_IMPL_BOOL_ATTR(nsHTMLFrameElement, NoResize, noresize)
NS_IMPL_STRING_ATTR(nsHTMLFrameElement, Scrolling, scrolling)
NS_IMPL_URI_ATTR(nsHTMLFrameElement, Src, src)


NS_IMETHODIMP
nsHTMLFrameElement::GetContentDocument(nsIDOMDocument** aContentDocument)
{
  NS_ENSURE_ARG_POINTER(aContentDocument);
  *aContentDocument = nsnull;

  if (!mDocument) {
    return NS_OK;
  }

  nsIDocument* content_document = mDocument->GetSubDocumentFor(this);

  if (!content_document) {
    return NS_OK;
  }

  return CallQueryInterface(content_document, aContentDocument);
}

NS_IMETHODIMP
nsHTMLFrameElement::GetContentWindow(nsIDOMWindow** aContentWindow)
{
  NS_ENSURE_ARG_POINTER(aContentWindow);
  *aContentWindow = nsnull;

  nsCOMPtr<nsIDOMDocument> content_doc;

  nsresult rv = GetContentDocument(getter_AddRefs(content_doc));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDocument> doc(do_QueryInterface(content_doc));

  if (!doc) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMWindow> window (do_QueryInterface(doc->GetScriptGlobalObject()));

  *aContentWindow = window;
  NS_IF_ADDREF(*aContentWindow);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFrameElement::StringToAttribute(nsIAtom* aAttribute,
                                      const nsAString& aValue,
                                      nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::bordercolor) {
    if (aResult.ParseColor(aValue, nsGenericHTMLElement::GetOwnerDocument())) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::frameborder) {
    if (ParseFrameborderValue(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::marginwidth) {
    if (aResult.ParseSpecialIntValue(aValue, eHTMLUnit_Integer, PR_TRUE, PR_FALSE)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::marginheight) {
    if (aResult.ParseSpecialIntValue(aValue, eHTMLUnit_Integer, PR_TRUE, PR_FALSE)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::scrolling) {
    if (ParseScrollingValue(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLFrameElement::AttributeToString(nsIAtom* aAttribute,
                                      const nsHTMLValue& aValue,
                                      nsAString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::frameborder) {
    FrameborderValueToString(aValue, aResult);
    return NS_CONTENT_ATTR_HAS_VALUE;
  } 
  else if (aAttribute == nsHTMLAtoms::scrolling) {
    ScrollingValueToString(aValue, aResult);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }

  return nsGenericHTMLElement::AttributeToString(aAttribute, aValue, aResult);
}

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  nsGenericHTMLElement::MapScrollingAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(PRBool)
nsHTMLFrameElement::HasAttributeDependentStyle(const nsIAtom* aAttribute) const
{
  static const AttributeDependenceEntry* const map[] = {
    sScrollingAttributeMap,
    sCommonAttributeMap,
  };
  
  return FindAttributeDependence(aAttribute, map, NS_ARRAY_LENGTH(map));
}

NS_IMETHODIMP
nsHTMLFrameElement::GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const
{
  aMapRuleFunc = &MapAttributesIntoRule;
  return NS_OK;
}


//*****************************************************************************
// nsHTMLFrameElement::nsIChromeEventHandler
//*****************************************************************************   

NS_IMETHODIMP
nsHTMLFrameElement::HandleChromeEvent(nsIPresContext* aPresContext,
                                      nsEvent* aEvent,
                                      nsIDOMEvent** aDOMEvent,
                                      PRUint32 aFlags, 
                                      nsEventStatus* aEventStatus)
{
  return HandleDOMEvent(aPresContext, aEvent, aDOMEvent, aFlags,aEventStatus);
}


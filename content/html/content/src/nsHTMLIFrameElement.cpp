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
#include "nsIDOMHTMLIFrameElement.h"
#include "nsIDOMNSHTMLFrameElement.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIWebNavigation.h"
#include "nsIHTMLAttributes.h"
#include "nsIChromeEventHandler.h"
#include "nsDOMError.h"
#include "nsIRuleNode.h"

class nsHTMLIFrameElement : public nsGenericHTMLContainerElement,
                            public nsIDOMHTMLIFrameElement,
                            public nsIDOMNSHTMLFrameElement,
                            public nsIChromeEventHandler,
                            public nsIDOMEventListener
{
public:
  nsHTMLIFrameElement();
  virtual ~nsHTMLIFrameElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLContainerElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLIFrameElement
  NS_DECL_NSIDOMHTMLIFRAMEELEMENT

  // nsIDOMNSHTMLFrameElement
  NS_DECL_NSIDOMNSHTMLFRAMEELEMENT

  // nsIChromeEventHandler
  NS_DECL_NSICHROMEEVENTHANDLER

  // nsIDOMEventListener
  NS_DECL_NSIDOMEVENTLISTENER

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAReadableString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAWritableString& aResult) const;
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                      PRInt32& aHint) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
};

nsresult
NS_NewHTMLIFrameElement(nsIHTMLContent** aInstancePtrResult,
                        nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLIFrameElement* it = new nsHTMLIFrameElement();

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


nsHTMLIFrameElement::nsHTMLIFrameElement()
{
}

nsHTMLIFrameElement::~nsHTMLIFrameElement()
{
}


NS_IMPL_ADDREF(nsHTMLIFrameElement);
NS_IMPL_RELEASE(nsHTMLIFrameElement);

// QueryInterface implementation for nsHTMLIFrameElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLIFrameElement,
                                    nsGenericHTMLContainerElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLIFrameElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSHTMLFrameElement)
  NS_INTERFACE_MAP_ENTRY(nsIChromeEventHandler)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLIFrameElement)
NS_HTML_CONTENT_INTERFACE_MAP_END

nsresult
nsHTMLIFrameElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLIFrameElement* it = new nsHTMLIFrameElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);

  nsresult rv = it->Init(mNodeInfo);

  if (NS_FAILED(rv))
    return rv;

  CopyInnerTo(this, it, aDeep);

  *aReturn = NS_STATIC_CAST(nsIDOMNode *, it);

  NS_ADDREF(*aReturn);

  return NS_OK;
}


NS_IMPL_STRING_ATTR(nsHTMLIFrameElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLIFrameElement, FrameBorder, frameborder)
NS_IMPL_STRING_ATTR(nsHTMLIFrameElement, Height, height)
NS_IMPL_STRING_ATTR(nsHTMLIFrameElement, LongDesc, longdesc)
NS_IMPL_STRING_ATTR(nsHTMLIFrameElement, MarginHeight, marginheight)
NS_IMPL_STRING_ATTR(nsHTMLIFrameElement, MarginWidth, marginwidth)
NS_IMPL_STRING_ATTR(nsHTMLIFrameElement, Name, name)
NS_IMPL_STRING_ATTR(nsHTMLIFrameElement, Scrolling, scrolling)
NS_IMPL_STRING_ATTR(nsHTMLIFrameElement, Src, src)
NS_IMPL_STRING_ATTR(nsHTMLIFrameElement, Width, width)


NS_IMETHODIMP
nsHTMLIFrameElement::GetContentDocument(nsIDOMDocument** aContentDocument)
{
  NS_ENSURE_ARG_POINTER(aContentDocument);

  *aContentDocument = nsnull;

  NS_ENSURE_TRUE(mDocument, NS_OK);

  nsCOMPtr<nsIPresShell> presShell;

  mDocument->GetShellAt(0, getter_AddRefs(presShell));
  NS_ENSURE_TRUE(presShell, NS_OK);

  nsCOMPtr<nsISupports> tmp;

  presShell->GetSubShellFor(this, getter_AddRefs(tmp));
  NS_ENSURE_TRUE(tmp, NS_OK);

  nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(tmp);
  NS_ENSURE_TRUE(webNav, NS_OK);

  nsCOMPtr<nsIDOMDocument> domDoc;

  webNav->GetDocument(getter_AddRefs(domDoc));

  *aContentDocument = domDoc;

  NS_IF_ADDREF(*aContentDocument);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLIFrameElement::GetContentWindow(nsIDOMWindow** aContentWindow)
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

  nsCOMPtr<nsIScriptGlobalObject> globalObj;
  doc->GetScriptGlobalObject(getter_AddRefs(globalObj));

  nsCOMPtr<nsIDOMWindow> window (do_QueryInterface(globalObj));

  *aContentWindow = window;
  NS_IF_ADDREF(*aContentWindow);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLIFrameElement::StringToAttribute(nsIAtom* aAttribute,
                                       const nsAReadableString& aValue,
                                       nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::marginwidth) {
    if (ParseValueOrPercent(aValue, aResult, eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::marginheight) {
    if (ParseValueOrPercent(aValue, aResult, eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::width) {
    if (ParseValueOrPercent(aValue, aResult, eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::height) {
    if (ParseValueOrPercent(aValue, aResult, eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::frameborder) {
    if (ParseFrameborderValue(!InNavQuirksMode(mDocument), aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::scrolling) {
    if (ParseScrollingValue(PR_TRUE, aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::align) {
    if (ParseAlignValue(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLIFrameElement::AttributeToString(nsIAtom* aAttribute,
                                       const nsHTMLValue& aValue,
                                       nsAWritableString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::frameborder) {
    FrameborderValueToString(PR_TRUE, aValue, aResult);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (aAttribute == nsHTMLAtoms::scrolling) {
    ScrollingValueToString(PR_TRUE, aValue, aResult);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  else if (aAttribute == nsHTMLAtoms::align) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      AlignValueToString(aValue, aResult);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  return nsGenericHTMLContainerElement::AttributeToString(aAttribute, aValue,
                                                          aResult);
}

static void
MapAttributesIntoRule(const nsIHTMLMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  if (!aData || !aAttributes)
    return;

  if (aData->mSID == eStyleStruct_Border && aData->mMarginData) {
    // frameborder: 0 | 1 (| NO | YES in quirks mode)
    // If frameborder is 0 or No, set border to 0
    // else leave it as the value set in html.css
    nsHTMLValue value;
    aAttributes->GetAttribute(nsHTMLAtoms::frameborder, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated) {
      PRInt32 frameborder = value.GetIntValue();
      if (NS_STYLE_FRAME_0 == frameborder ||
          NS_STYLE_FRAME_NO == frameborder ||
          NS_STYLE_FRAME_OFF == frameborder) {
        if (aData->mMarginData->mBorderWidth->mLeft.GetUnit() == eCSSUnit_Null)
          aData->mMarginData->mBorderWidth->mLeft.SetFloatValue(0.0f, eCSSUnit_Pixel);
        if (aData->mMarginData->mBorderWidth->mRight.GetUnit() == eCSSUnit_Null)
          aData->mMarginData->mBorderWidth->mRight.SetFloatValue(0.0f, eCSSUnit_Pixel);
        if (aData->mMarginData->mBorderWidth->mTop.GetUnit() == eCSSUnit_Null)
          aData->mMarginData->mBorderWidth->mTop.SetFloatValue(0.0f, eCSSUnit_Pixel);
        if (aData->mMarginData->mBorderWidth->mBottom.GetUnit() == eCSSUnit_Null)
          aData->mMarginData->mBorderWidth->mBottom.SetFloatValue(0.0f, eCSSUnit_Pixel);
      }
    }
  }
  else if (aData->mSID == eStyleStruct_Position) {
    // width: value
    nsHTMLValue value;
    if (aData->mPositionData->mWidth.GetUnit() == eCSSUnit_Null) {
      aAttributes->GetAttribute(nsHTMLAtoms::width, value);
      if (value.GetUnit() == eHTMLUnit_Pixel)
        aData->mPositionData->mWidth.SetFloatValue((float)value.GetPixelValue(), eCSSUnit_Pixel);
      else if (value.GetUnit() == eHTMLUnit_Percent)
        aData->mPositionData->mWidth.SetPercentValue(value.GetPercentValue());
    }

    // height: value
    if (aData->mPositionData->mHeight.GetUnit() == eCSSUnit_Null) {
      aAttributes->GetAttribute(nsHTMLAtoms::height, value);
      if (value.GetUnit() == eHTMLUnit_Pixel)
        aData->mPositionData->mHeight.SetFloatValue((float)value.GetPixelValue(), eCSSUnit_Pixel);   
      else if (value.GetUnit() == eHTMLUnit_Percent)
        aData->mPositionData->mHeight.SetPercentValue(value.GetPercentValue());
    }
  }

  nsGenericHTMLElement::MapAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP
nsHTMLIFrameElement::GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                              PRInt32& aHint) const
{
  if ((aAttribute == nsHTMLAtoms::width) ||
      (aAttribute == nsHTMLAtoms::height)) {
    aHint = NS_STYLE_HINT_REFLOW;
  }
  else if (aAttribute == nsHTMLAtoms::align) {
    aHint = NS_STYLE_HINT_FRAMECHANGE;
  }
  else if (aAttribute == nsHTMLAtoms::frameborder) {
    aHint = NS_STYLE_HINT_REFLOW;
  }
  else if (!GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    aHint = NS_STYLE_HINT_CONTENT;
  }

  return NS_OK;
}



NS_IMETHODIMP
nsHTMLIFrameElement::GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const
{
  aMapRuleFunc = &MapAttributesIntoRule;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLIFrameElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}


//*****************************************************************************
// nsHTMLIFrameElement::nsIChromeEventHandler
//*****************************************************************************   

NS_IMETHODIMP
nsHTMLIFrameElement::HandleChromeEvent(nsIPresContext* aPresContext,
                                       nsEvent* aEvent,
                                       nsIDOMEvent** aDOMEvent,
                                       PRUint32 aFlags, 
                                       nsEventStatus* aEventStatus)
{
   return HandleDOMEvent(aPresContext, aEvent, aDOMEvent, aFlags,aEventStatus);
}

nsresult
nsHTMLIFrameElement::HandleEvent(nsIDOMEvent *aEvent)
{
  return HandleFrameOnloadEvent(aEvent);
}

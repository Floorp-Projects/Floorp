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
#include "nsIDOMEventReceiver.h"
#include "nsIDOMWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsIFrameLoader.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIWebNavigation.h"
#include "nsMappedAttributes.h"
#include "nsIChromeEventHandler.h"
#include "nsDOMError.h"
#include "nsRuleNode.h"
#include "nsIDocShell.h"
#include "nsIInterfaceRequestorUtils.h"

class nsHTMLIFrameElement : public nsGenericHTMLElement,
                            public nsIDOMHTMLIFrameElement,
                            public nsIDOMNSHTMLFrameElement,
                            public nsIChromeEventHandler,
                            public nsIFrameLoaderOwner
{
public:
  nsHTMLIFrameElement();
  virtual ~nsHTMLIFrameElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLIFrameElement
  NS_DECL_NSIDOMHTMLIFRAMEELEMENT

  // nsIDOMNSHTMLFrameElement
  NS_DECL_NSIDOMNSHTMLFRAMEELEMENT

  // nsIChromeEventHandler
  NS_DECL_NSICHROMEEVENTHANDLER

  // nsIFrameLoaderOwner
  NS_IMETHOD GetFrameLoader(nsIFrameLoader **aFrameLoader);
  NS_IMETHOD SetFrameLoader(nsIFrameLoader *aFrameLoader);

  // nsIContent
  virtual void SetParent(nsIContent *aParent);
  virtual void SetDocument(nsIDocument *aDocument, PRBool aDeep,
                           PRBool aCompileEventHandlers);

  virtual PRBool ParseAttribute(nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAString& aResult) const;
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, PRBool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify)
  {
    nsresult rv = nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix,
                                                aValue, aNotify);

    if (NS_SUCCEEDED(rv) && aNameSpaceID == kNameSpaceID_None &&
        aName == nsHTMLAtoms::src) {
      return LoadSrc();
    }

    return rv;
  }

  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;

protected:
  // This doesn't really ensure a frame loade in all cases, only when
  // it makes sense.
  nsresult EnsureFrameLoader();
  nsresult LoadSrc();

  nsCOMPtr<nsIFrameLoader> mFrameLoader;
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
  if (mFrameLoader) {
    mFrameLoader->Destroy();
  }
}


NS_IMPL_ADDREF(nsHTMLIFrameElement)
NS_IMPL_RELEASE(nsHTMLIFrameElement)

// QueryInterface implementation for nsHTMLIFrameElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLIFrameElement, nsGenericHTMLElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLIFrameElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSHTMLFrameElement)
  NS_INTERFACE_MAP_ENTRY(nsIChromeEventHandler)
  NS_INTERFACE_MAP_ENTRY(nsIFrameLoaderOwner)
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

  CopyInnerTo(it, aDeep);

  *aReturn = NS_STATIC_CAST(nsIDOMNode *, it);

  NS_ADDREF(*aReturn);

  return NS_OK;
}


NS_IMPL_STRING_ATTR(nsHTMLIFrameElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLIFrameElement, FrameBorder, frameborder)
NS_IMPL_STRING_ATTR(nsHTMLIFrameElement, Height, height)
NS_IMPL_URI_ATTR(nsHTMLIFrameElement, LongDesc, longdesc)
NS_IMPL_STRING_ATTR(nsHTMLIFrameElement, MarginHeight, marginheight)
NS_IMPL_STRING_ATTR(nsHTMLIFrameElement, MarginWidth, marginwidth)
NS_IMPL_STRING_ATTR(nsHTMLIFrameElement, Name, name)
NS_IMPL_STRING_ATTR(nsHTMLIFrameElement, Scrolling, scrolling)
NS_IMPL_URI_ATTR(nsHTMLIFrameElement, Src, src)
NS_IMPL_STRING_ATTR(nsHTMLIFrameElement, Width, width)

NS_IMETHODIMP
nsHTMLIFrameElement::GetContentDocument(nsIDOMDocument** aContentDocument)
{
  NS_ENSURE_ARG_POINTER(aContentDocument);
  *aContentDocument = nsnull;

  nsresult rv = EnsureFrameLoader();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mFrameLoader) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocShell> doc_shell;
  mFrameLoader->GetDocShell(getter_AddRefs(doc_shell));

  nsCOMPtr<nsIDOMWindow> win(do_GetInterface(doc_shell));

  if (!win) {
    return NS_OK;
  }

  return win->GetDocument(aContentDocument);
}

NS_IMETHODIMP
nsHTMLIFrameElement::GetContentWindow(nsIDOMWindow** aContentWindow)
{
  NS_ENSURE_ARG_POINTER(aContentWindow);

  nsresult rv = EnsureFrameLoader();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mFrameLoader) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocShell> doc_shell;
  mFrameLoader->GetDocShell(getter_AddRefs(doc_shell));

  nsCOMPtr<nsIDOMWindow> win(do_GetInterface(doc_shell));

  *aContentWindow = win;
  NS_IF_ADDREF(*aContentWindow);

  return NS_OK;
}

nsresult
nsHTMLIFrameElement::EnsureFrameLoader()
{
  if (!GetParent() || !mDocument || mFrameLoader) {
    // If frame loader is there, we just keep it around, cached
    return NS_OK;
  }

  nsresult rv = NS_NewFrameLoader(getter_AddRefs(mFrameLoader));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mFrameLoader->Init(this);
  return rv;
}

NS_IMETHODIMP
nsHTMLIFrameElement::GetFrameLoader(nsIFrameLoader **aFrameLoader)
{
  *aFrameLoader = mFrameLoader;
  NS_IF_ADDREF(*aFrameLoader);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLIFrameElement::SetFrameLoader(nsIFrameLoader *aFrameLoader)
{
  mFrameLoader = aFrameLoader;

  return NS_OK;
}

nsresult
nsHTMLIFrameElement::LoadSrc()
{
  nsresult rv = EnsureFrameLoader();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mFrameLoader) {
    return NS_OK;
  }

  rv = mFrameLoader->LoadFrame();
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to load URL");

  return rv;
}


void
nsHTMLIFrameElement::SetParent(nsIContent *aParent)
{
  nsGenericHTMLElement::SetParent(aParent);

  // When parent is being set to null on the element's destruction, do not
  // call LoadSrc().
  if (!GetParent() || !mDocument) {
    return;
  }

  LoadSrc();
}

void
nsHTMLIFrameElement::SetDocument(nsIDocument *aDocument, PRBool aDeep,
                                 PRBool aCompileEventHandlers)
{
  const nsIDocument *old_doc = mDocument;

  nsGenericHTMLElement::SetDocument(aDocument, aDeep,
                                    aCompileEventHandlers);

  if (!aDocument && mFrameLoader) {
    // This iframe is being taken out of the document, destroy the
    // iframe's frame loader (doing that will tear down the window in
    // this iframe).

    mFrameLoader->Destroy();

    mFrameLoader = nsnull;
  }

  // When document is being set to null on the element's destruction,
  // or when the document is being set to what the document already
  // is, do not call LoadSrc().
  if (GetParent() && aDocument && aDocument != old_doc) {
    LoadSrc();
  }
}

PRBool
nsHTMLIFrameElement::ParseAttribute(nsIAtom* aAttribute,
                                    const nsAString& aValue,
                                    nsAttrValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::marginwidth) {
    return aResult.ParseSpecialIntValue(aValue, PR_TRUE, PR_FALSE);
  }
  if (aAttribute == nsHTMLAtoms::marginheight) {
    return aResult.ParseSpecialIntValue(aValue, PR_TRUE, PR_FALSE);
  }
  if (aAttribute == nsHTMLAtoms::width) {
    return aResult.ParseSpecialIntValue(aValue, PR_TRUE, PR_FALSE);
  }
  if (aAttribute == nsHTMLAtoms::height) {
    return aResult.ParseSpecialIntValue(aValue, PR_TRUE, PR_FALSE);
  }
  if (aAttribute == nsHTMLAtoms::frameborder) {
    return ParseFrameborderValue(aValue, aResult);
  }
  if (aAttribute == nsHTMLAtoms::scrolling) {
    return ParseScrollingValue(aValue, aResult);
  }
  if (aAttribute == nsHTMLAtoms::align) {
    return ParseAlignValue(aValue, aResult);
  }

  return nsGenericHTMLElement::ParseAttribute(aAttribute, aValue, aResult);
}

NS_IMETHODIMP
nsHTMLIFrameElement::AttributeToString(nsIAtom* aAttribute,
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
  else if (aAttribute == nsHTMLAtoms::align) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      VAlignValueToString(aValue, aResult);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  return nsGenericHTMLElement::AttributeToString(aAttribute, aValue, aResult);
}

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  if (aData->mSID == eStyleStruct_Border) {
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
        if (aData->mMarginData->mBorderWidth.mLeft.GetUnit() == eCSSUnit_Null)
          aData->mMarginData->mBorderWidth.mLeft.SetFloatValue(0.0f, eCSSUnit_Pixel);
        if (aData->mMarginData->mBorderWidth.mRight.GetUnit() == eCSSUnit_Null)
          aData->mMarginData->mBorderWidth.mRight.SetFloatValue(0.0f, eCSSUnit_Pixel);
        if (aData->mMarginData->mBorderWidth.mTop.GetUnit() == eCSSUnit_Null)
          aData->mMarginData->mBorderWidth.mTop.SetFloatValue(0.0f, eCSSUnit_Pixel);
        if (aData->mMarginData->mBorderWidth.mBottom.GetUnit() == eCSSUnit_Null)
          aData->mMarginData->mBorderWidth.mBottom.SetFloatValue(0.0f, eCSSUnit_Pixel);
      }
    }
  }
  else if (aData->mSID == eStyleStruct_Position) {
    // width: value
    nsHTMLValue value;
    if (aData->mPositionData->mWidth.GetUnit() == eCSSUnit_Null) {
      aAttributes->GetAttribute(nsHTMLAtoms::width, value);
      if (value.GetUnit() == eHTMLUnit_Integer)
        aData->mPositionData->mWidth.SetFloatValue((float)value.GetIntValue(), eCSSUnit_Pixel);
      else if (value.GetUnit() == eHTMLUnit_Percent)
        aData->mPositionData->mWidth.SetPercentValue(value.GetPercentValue());
    }

    // height: value
    if (aData->mPositionData->mHeight.GetUnit() == eCSSUnit_Null) {
      aAttributes->GetAttribute(nsHTMLAtoms::height, value);
      if (value.GetUnit() == eHTMLUnit_Integer)
        aData->mPositionData->mHeight.SetFloatValue((float)value.GetIntValue(), eCSSUnit_Pixel);
      else if (value.GetUnit() == eHTMLUnit_Percent)
        aData->mPositionData->mHeight.SetPercentValue(value.GetPercentValue());
    }
  }

  nsGenericHTMLElement::MapScrollingAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(PRBool)
nsHTMLIFrameElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsHTMLAtoms::width },
    { &nsHTMLAtoms::height },
    { &nsHTMLAtoms::frameborder },
    { nsnull },
  };

  static const MappedAttributeEntry* const map[] = {
    attributes,
    sScrollingAttributeMap,
    sImageAlignAttributeMap,
    sCommonAttributeMap,
  };
  
  return FindAttributeDependence(aAttribute, map, NS_ARRAY_LENGTH(map));
}



NS_IMETHODIMP
nsHTMLIFrameElement::GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const
{
  aMapRuleFunc = &MapAttributesIntoRule;
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


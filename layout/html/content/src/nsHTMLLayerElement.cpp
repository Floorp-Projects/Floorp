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
#include "nsIDOMHTMLElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMHTMLLayerElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsIMutableStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIHTMLAttributes.h"

//#define _I32_MIN  (-2147483647 - 1) /* minimum signed 32 bit value */

static NS_DEFINE_IID(kIDOMHTMLLayerElementIID, NS_IDOMHTMLLAYERELEMENT_IID);
static NS_DEFINE_IID(kIDOMDocumentIID, NS_IDOMDOCUMENT_IID);

class nsHTMLLayerElement : public nsIDOMHTMLLayerElement,
                           public nsIJSScriptObject,
                           public nsIHTMLContent
{
public:
  nsHTMLLayerElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLLayerElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLLayerElement
  NS_IMETHOD    GetTop(PRInt32* aTop);
  NS_IMETHOD    SetTop(PRInt32 aTop);
  NS_IMETHOD    GetLeft(PRInt32* aLeft);
  NS_IMETHOD    SetLeft(PRInt32 aLeft);
  NS_IMETHOD    GetVisibility(nsString& aVisibility);
  NS_IMETHOD    SetVisibility(const nsString& aVisibility);
  NS_IMETHOD    GetBackground(nsString& aBackground);
  NS_IMETHOD    SetBackground(const nsString& aBackground);
  NS_IMETHOD    GetBgColor(nsString& aBgColor);
  NS_IMETHOD    SetBgColor(const nsString& aBgColor);
  NS_IMETHOD    GetName(nsString& aName);
  NS_IMETHOD    SetName(const nsString& aName);
  NS_IMETHOD    GetZIndex(PRInt32* aZIndex);
  NS_IMETHOD    SetZIndex(PRInt32 aZIndex);
  NS_IMETHOD    GetDocument(nsIDOMDocument** aReturn);

  // nsIJSScriptObject
  NS_IMPL_IJSSCRIPTOBJECT_USING_GENERIC(mInner)

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC(mInner)

  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC(mInner)

protected:
  nsGenericHTMLContainerElement mInner;
};

nsresult
NS_NewHTMLLayerElement(nsIHTMLContent** aInstancePtrResult,
                       nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);
  NS_ENSURE_ARG_POINTER(aNodeInfo);

  nsIHTMLContent* it = new nsHTMLLayerElement(aNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLLayerElement::nsHTMLLayerElement(nsINodeInfo *aNodeInfo)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aNodeInfo);
}

nsHTMLLayerElement::~nsHTMLLayerElement()
{
}

NS_IMPL_ADDREF(nsHTMLLayerElement)

NS_IMPL_RELEASE(nsHTMLLayerElement)

nsresult
nsHTMLLayerElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLLayerElementIID)) {
    nsIDOMHTMLLayerElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_INT_ATTR(nsHTMLLayerElement, Top, top)
NS_IMPL_INT_ATTR(nsHTMLLayerElement, Left, left)
NS_IMPL_STRING_ATTR(nsHTMLLayerElement, Visibility, visibility)
NS_IMPL_STRING_ATTR(nsHTMLLayerElement, Background, background)
NS_IMPL_STRING_ATTR(nsHTMLLayerElement, BgColor, bgcolor)
NS_IMPL_STRING_ATTR(nsHTMLLayerElement, Name, name)
NS_IMPL_INT_ATTR(nsHTMLLayerElement, ZIndex, zindex)

NS_IMETHODIMP    
nsHTMLLayerElement::GetDocument(nsIDOMDocument** aDocument)
{
  // XXX This is cheating. We should really return the layer's
  // internal document.
  nsresult result = NS_OK;
  nsIDocument* document;
  
  result = mInner.GetDocument(document);
  if (NS_SUCCEEDED(result)) {
    result = document->QueryInterface(kIDOMDocumentIID, (void**)aDocument);
    NS_RELEASE(document);
  }
  else {
    *aDocument = nsnull;
  }

  return result;
}

nsresult
nsHTMLLayerElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLLayerElement* it = new nsHTMLLayerElement(mInner.mNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

static nsGenericHTMLElement::EnumTable kVisibilityTable[] = {
  {"hidden", NS_STYLE_VISIBILITY_HIDDEN},
  {"visible", NS_STYLE_VISIBILITY_VISIBLE},
  {"show", NS_STYLE_VISIBILITY_VISIBLE},
  {"hide", NS_STYLE_VISIBILITY_HIDDEN},
  {"inherit", NS_STYLE_VISIBILITY_VISIBLE},
  {0}
};

NS_IMETHODIMP
nsHTMLLayerElement::StringToAttribute(nsIAtom*        aAttribute,
                                      const nsString& aValue,
                                      nsHTMLValue&    aResult)
{
  // XXX CLIP
  if ((aAttribute == nsHTMLAtoms::top)   ||
      (aAttribute == nsHTMLAtoms::left)  ||
      (aAttribute == nsHTMLAtoms::width) ||
      (aAttribute == nsHTMLAtoms::height)) {
    if (nsGenericHTMLElement::ParseValueOrPercent(aValue, aResult, eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if ((aAttribute == nsHTMLAtoms::zindex) ||
           (aAttribute == nsHTMLAtoms::z_index)) {
    if (nsGenericHTMLElement::ParseValue(aValue, 0, aResult, eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::visibility) {
    if (nsGenericHTMLElement::ParseEnumValue(aValue, kVisibilityTable, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::bgcolor) {
    if (nsGenericHTMLElement::ParseColor(aValue, mInner.mDocument, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  // ABOVE, BELOW, OnMouseOver, OnMouseOut, OnFocus, OnBlur, OnLoad
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLLayerElement::AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::visibility) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      nsGenericHTMLElement::EnumValueToString(aValue, kVisibilityTable, aResult);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

static void
MapAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                  nsIMutableStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  // Note: ua.css specifies that the 'position' is absolute
  nsHTMLValue      value;
  float            p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);
  nsStylePosition* position = (nsStylePosition*)
    aContext->GetMutableStyleData(eStyleStruct_Position);

  // Left
  aAttributes->GetAttribute(nsHTMLAtoms::left, value);
  if (value.GetUnit() == eHTMLUnit_Pixel) {
    nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
    position->mOffset.SetLeft(nsStyleCoord(twips));
  }
  else if (value.GetUnit() == eHTMLUnit_Percent) {
    position->mOffset.SetLeft(nsStyleCoord(value.GetPercentValue(), eStyleUnit_Percent));
  }

  // Top
  aAttributes->GetAttribute(nsHTMLAtoms::top, value);
  if (value.GetUnit() == eHTMLUnit_Pixel) {
    nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
    position->mOffset.SetTop(nsStyleCoord(twips));
  }
  else if (value.GetUnit() == eHTMLUnit_Percent) {
    position->mOffset.SetTop(nsStyleCoord(value.GetPercentValue(), eStyleUnit_Percent));
  }

  // Width
  aAttributes->GetAttribute(nsHTMLAtoms::width, value);
  if (value.GetUnit() == eHTMLUnit_Pixel) {
    nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
    position->mWidth.SetCoordValue(twips);
  }
  else if (value.GetUnit() == eHTMLUnit_Percent) {
    position->mWidth.SetPercentValue(value.GetPercentValue());
  }

  // Height
  aAttributes->GetAttribute(nsHTMLAtoms::height, value);
  if (value.GetUnit() == eHTMLUnit_Pixel) {
    nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
    position->mHeight.SetCoordValue(twips);
  }
  else if (value.GetUnit() == eHTMLUnit_Percent) {
    position->mHeight.SetPercentValue(value.GetPercentValue());
  }

  // Z-index
  aAttributes->GetAttribute(nsHTMLAtoms::zindex, value);
  if (value.GetUnit() == eHTMLUnit_Integer) {
    position->mZIndex.SetIntValue(value.GetIntValue(), eStyleUnit_Integer);
  }
  aAttributes->GetAttribute(nsHTMLAtoms::z_index, value);
  if (value.GetUnit() == eHTMLUnit_Integer) {
    position->mZIndex.SetIntValue(value.GetIntValue(), eStyleUnit_Integer);
  }

  // Visibility
  aAttributes->GetAttribute(nsHTMLAtoms::visibility, value);
  if (value.GetUnit() == eHTMLUnit_Enumerated) {
    nsStyleDisplay* display = (nsStyleDisplay*)
      aContext->GetMutableStyleData(eStyleStruct_Display);

    display->mVisible = value.GetIntValue();
  }

  // Background and bgcolor
  nsGenericHTMLElement::MapBackgroundAttributesInto(aAttributes, aContext, aPresContext);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
}

NS_IMETHODIMP
nsHTMLLayerElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                             PRInt32& aHint) const
{
  if ((aAttribute == nsHTMLAtoms::visibility) ||
      (aAttribute == nsHTMLAtoms::zindex) ||
      (aAttribute == nsHTMLAtoms::z_index)) {
    aHint = NS_STYLE_HINT_VISUAL;
  }
  else if ((aAttribute == nsHTMLAtoms::left) || 
           (aAttribute == nsHTMLAtoms::top) ||
           (aAttribute == nsHTMLAtoms::width) ||
           (aAttribute == nsHTMLAtoms::height)) {
    aHint = NS_STYLE_HINT_REFLOW;
  }
  else if (! nsGenericHTMLElement::GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    if (! nsGenericHTMLElement::GetBackgroundAttributesImpact(aAttribute, aHint)) {
      aHint = NS_STYLE_HINT_CONTENT;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLLayerElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                                 nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLLayerElement::HandleDOMEvent(nsIPresContext* aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus* aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}


NS_IMETHODIMP
nsHTMLLayerElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  return mInner.SizeOf(aSizer, aResult, sizeof(*this));
}

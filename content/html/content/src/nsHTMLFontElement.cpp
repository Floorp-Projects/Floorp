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
#include "nsCOMPtr.h"
#include "nsIDOMHTMLFontElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIDeviceContext.h"
#include "nsIStyleContext.h"
#include "nsIMutableStyleContext.h"
#include "nsStyleConsts.h"
#include "nsStyleUtil.h"
#include "nsIPresContext.h"
#include "nsIHTMLAttributes.h"

static NS_DEFINE_IID(kIDOMHTMLFontElementIID, NS_IDOMHTMLFONTELEMENT_IID);

class nsHTMLFontElement : public nsIDOMHTMLFontElement,
                          public nsIJSScriptObject,
                          public nsIHTMLContent
{
public:
  nsHTMLFontElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLFontElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLFontElement
  NS_IMETHOD GetColor(nsString& aColor);
  NS_IMETHOD SetColor(const nsString& aColor);
  NS_IMETHOD GetFace(nsString& aFace);
  NS_IMETHOD SetFace(const nsString& aFace);
  NS_IMETHOD GetSize(nsString& aSize);
  NS_IMETHOD SetSize(const nsString& aSize);

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
NS_NewHTMLFontElement(nsIHTMLContent** aInstancePtrResult,
                      nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);
  NS_ENSURE_ARG_POINTER(aNodeInfo);

  nsIHTMLContent* it = new nsHTMLFontElement(aNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}


nsHTMLFontElement::nsHTMLFontElement(nsINodeInfo *aNodeInfo)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aNodeInfo);
}

nsHTMLFontElement::~nsHTMLFontElement()
{
}

NS_IMPL_ADDREF(nsHTMLFontElement)

NS_IMPL_RELEASE(nsHTMLFontElement)

nsresult
nsHTMLFontElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLFontElementIID)) {
    nsIDOMHTMLFontElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLFontElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLFontElement* it = new nsHTMLFontElement(mInner.mNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMPL_STRING_ATTR(nsHTMLFontElement, Color, color)
NS_IMPL_STRING_ATTR(nsHTMLFontElement, Face, face)
NS_IMPL_STRING_ATTR(nsHTMLFontElement, Size, size)

NS_IMETHODIMP
nsHTMLFontElement::StringToAttribute(nsIAtom* aAttribute,
                              const nsString& aValue,
                              nsHTMLValue& aResult)
{
  if ((aAttribute == nsHTMLAtoms::size) ||
      (aAttribute == nsHTMLAtoms::pointSize) ||
      (aAttribute == nsHTMLAtoms::fontWeight)) {
    nsAutoString tmp(aValue);
      //rickg: fixed flaw where ToInteger error code was not being checked.
      //       This caused wrong default value for font size.
    PRInt32 ec, v = tmp.ToInteger(&ec);
    if(NS_SUCCEEDED(ec)){
      tmp.CompressWhitespace(PR_TRUE, PR_FALSE);
      PRUnichar ch = tmp.IsEmpty() ? 0 : tmp.First();
      aResult.SetIntValue(v, ((ch == '+') || (ch == '-')) ?
                          eHTMLUnit_Integer : eHTMLUnit_Enumerated);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::color) {
    if (nsGenericHTMLElement::ParseColor(aValue, mInner.mDocument, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLFontElement::AttributeToString(nsIAtom* aAttribute,
                                     const nsHTMLValue& aValue,
                                     nsString& aResult) const
{
  if ((aAttribute == nsHTMLAtoms::size) ||
      (aAttribute == nsHTMLAtoms::pointSize) ||
      (aAttribute == nsHTMLAtoms::fontWeight)) {
    aResult.Truncate();
    if (aValue.GetUnit() == eHTMLUnit_Enumerated) {
      aResult.AppendInt(aValue.GetIntValue(), 10);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
    else if (aValue.GetUnit() == eHTMLUnit_Integer) {
      PRInt32 value = aValue.GetIntValue(); 
      if (value >= 0) {
        aResult.AppendWithConversion('+');
      }
      aResult.AppendInt(value, 10);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
    return NS_CONTENT_ATTR_NOT_THERE;
  }
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

static void
MapFontAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                      nsIMutableStyleContext* aContext,
                      nsIPresContext* aPresContext)
{
  if (nsnull != aAttributes) {
    nsHTMLValue value;
    nsStyleFont* font = (nsStyleFont*)
      aContext->GetMutableStyleData(eStyleStruct_Font);
    const nsStyleFont* parentFont = font;
    nsIStyleContext* parentContext = aContext->GetParent();
    if (nsnull != parentContext) {
      parentFont = (const nsStyleFont*)
        parentContext->GetStyleData(eStyleStruct_Font);
    }
    const nsFont& defaultFont = aPresContext->GetDefaultFontDeprecated(); 
    const nsFont& defaultFixedFont = aPresContext->GetDefaultFixedFontDeprecated(); 

    // face: string list
    aAttributes->GetAttribute(nsHTMLAtoms::face, value);
    if (value.GetUnit() == eHTMLUnit_String) {

      nsCOMPtr<nsIDeviceContext> dc;
      aPresContext->GetDeviceContext(getter_AddRefs(dc));
      if (dc) {
        nsAutoString  familyList;

        value.GetStringValue(familyList);
          
        font->mFont.name = familyList;
        nsAutoString face;
        if (NS_OK == dc->FirstExistingFont(font->mFont, face)) {
          if (face.EqualsIgnoreCase("-moz-fixed")) {
            font->mFlags |= NS_STYLE_FONT_USE_FIXED;
          } else {
            font->mFlags &= ~NS_STYLE_FONT_USE_FIXED;
          }
/* bug 12737
          else {
						nsCompatibility mode;
		        aPresContext->GetCompatibilityMode(&mode);
		        if (eCompatibility_NavQuirks == mode) {
            	font->mFixedFont.name = familyList;
            }
          }
*/
        }
        else {
          font->mFont.name = defaultFont.name;
          font->mFixedFont.name= defaultFixedFont.name;
        }
        font->mFlags |= NS_STYLE_FONT_FACE_EXPLICIT;
      }
    }

    // pointSize: int, enum
    aAttributes->GetAttribute(nsHTMLAtoms::pointSize, value);
    if (value.GetUnit() == eHTMLUnit_Integer) {
      // XXX should probably sanitize value
      font->mFont.size = parentFont->mFont.size +
        NSIntPointsToTwips(value.GetIntValue());
      font->mFixedFont.size = parentFont->mFixedFont.size +
        NSIntPointsToTwips(value.GetIntValue());
      font->mFlags |= NS_STYLE_FONT_SIZE_EXPLICIT;
    }
    else if (value.GetUnit() == eHTMLUnit_Enumerated) {
      font->mFont.size = NSIntPointsToTwips(value.GetIntValue());
      font->mFixedFont.size = NSIntPointsToTwips(value.GetIntValue());
      font->mFlags |= NS_STYLE_FONT_SIZE_EXPLICIT;
    }
    else {
      // size: int, enum , NOTE: this does not count as an explicit size
      // also this has no effect if font is already explicit (quirk mode)
      //
      // NOTE: we now do not emulate this quirk - it is too stupid, IE does it right, and it
      //       messes up other blocks (ie. Headings) when implemented... see bug 25810

#if 0 // removing the quirk...
      nsCompatibility mode;
      aPresContext->GetCompatibilityMode(&mode);
      if ((eCompatibility_Standard == mode) || 
          (0 == (font->mFlags & NS_STYLE_FONT_SIZE_EXPLICIT))) {
#else
      if (1){ 
#endif
        aAttributes->GetAttribute(nsHTMLAtoms::size, value);
        if ((value.GetUnit() == eHTMLUnit_Integer) ||
            (value.GetUnit() == eHTMLUnit_Enumerated)) { 
          PRInt32 size = value.GetIntValue();
        
          if (size != 0) {  // bug 32063: ignore <font size="">
	          if (value.GetUnit() == eHTMLUnit_Integer) { // int (+/-)
	            size = 3 + size;  // XXX should be BASEFONT, not three
	          }
	          size = ((0 < size) ? ((size < 8) ? size : 7) : 1); 
	          PRInt32 scaler;
	          aPresContext->GetFontScaler(&scaler);
	          float scaleFactor = nsStyleUtil::GetScalingFactor(scaler);
	          font->mFont.size =
	            nsStyleUtil::CalcFontPointSize(size, (PRInt32)defaultFont.size,
	                                           scaleFactor, aPresContext);
	          font->mFixedFont.size =
	            nsStyleUtil::CalcFontPointSize(size,
	                                           (PRInt32)defaultFixedFont.size,
	                                           scaleFactor, aPresContext);
					}
        }
      }
    }

    // fontWeight: int, enum
    aAttributes->GetAttribute(nsHTMLAtoms::fontWeight, value);
    if (value.GetUnit() == eHTMLUnit_Integer) { // +/-
      PRInt32 intValue = (value.GetIntValue() / 100);
      PRInt32 weight = nsStyleUtil::ConstrainFontWeight(parentFont->mFont.weight + 
                                                        (intValue * NS_STYLE_FONT_WEIGHT_BOLDER));
      font->mFont.weight = weight;
      font->mFixedFont.weight = weight;
    }
    else if (value.GetUnit() == eHTMLUnit_Enumerated) {
      PRInt32 weight = value.GetIntValue();
      weight = nsStyleUtil::ConstrainFontWeight((weight / 100) * 100);
      font->mFont.weight = weight;
      font->mFixedFont.weight = weight;
    }

    NS_IF_RELEASE(parentContext);
  }
}

static void
MapAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                  nsIMutableStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  if (nsnull != aAttributes) {
    nsHTMLValue value;

    // color: color
    if (NS_CONTENT_ATTR_NOT_THERE != aAttributes->GetAttribute(nsHTMLAtoms::color, value)) {
      const nsStyleFont* font = (const nsStyleFont*)
        aContext->GetStyleData(eStyleStruct_Font);
      nsStyleColor* color = (nsStyleColor*)
        aContext->GetMutableStyleData(eStyleStruct_Color);
      nsStyleText* text = (nsStyleText*)
        aContext->GetMutableStyleData(eStyleStruct_Text);
      if (((eHTMLUnit_Color == value.GetUnit())) ||
          (eHTMLUnit_ColorName == value.GetUnit())) {
        color->mColor = value.GetColorValue();
        text->mTextDecoration = font->mFont.decorations;  // re-apply inherited text decoration, so colors sync
      }
    }
  }
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
}

NS_IMETHODIMP
nsHTMLFontElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                            PRInt32& aHint) const
{
  if (aAttribute == nsHTMLAtoms::color) {
    aHint = NS_STYLE_HINT_VISUAL;
  }
  else if ((aAttribute == nsHTMLAtoms::face) ||
           (aAttribute == nsHTMLAtoms::pointSize) ||
           (aAttribute == nsHTMLAtoms::size) ||
           (aAttribute == nsHTMLAtoms::fontWeight)) {
    aHint = NS_STYLE_HINT_REFLOW;
  }
  else if (! nsGenericHTMLElement::GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    aHint = NS_STYLE_HINT_CONTENT;
  }

  return NS_OK;
}


NS_IMETHODIMP
nsHTMLFontElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                                nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = &MapFontAttributesInto;
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFontElement::HandleDOMEvent(nsIPresContext* aPresContext,
                                  nsEvent* aEvent,
                                  nsIDOMEvent** aDOMEvent,
                                  PRUint32 aFlags,
                                  nsEventStatus* aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}


NS_IMETHODIMP
nsHTMLFontElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  return mInner.SizeOf(aSizer, aResult, sizeof(*this));
}

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
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsIMutableStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIHTMLAttributes.h"
#include "nsIJSScriptObject.h"
#include "nsSize.h"
#include "nsIDocument.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMDocument.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"
#include "nsLayoutUtils.h"
#include "nsIWebShell.h"
#include "nsIFrame.h"
#include "nsImageFrame.h"
#include "nsLayoutAtoms.h"


// XXX nav attrs: suppress

class nsHTMLSpacerElement : public nsIDOMHTMLElement,
                            public nsIJSScriptObject,
                            public nsIHTMLContent
{
public:
  nsHTMLSpacerElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLSpacerElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC(mInner)

  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC(mInner)

  // nsIJSScriptObject
  NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC(mInner)
  virtual PRBool    AddProperty(JSContext *aContext, JSObject *aObj, 
                        jsval aID, jsval *aVp);
  virtual PRBool    DeleteProperty(JSContext *aContext, JSObject *aObj, 
                        jsval aID, jsval *aVp);
  virtual PRBool    GetProperty(JSContext *aContext, JSObject *aObj, 
                        jsval aID, jsval *aVp);
  virtual PRBool    SetProperty(JSContext *aContext, JSObject *aObj, 
                        jsval aID, jsval *aVp);
  virtual PRBool    EnumerateProperty(JSContext *aContext, JSObject *aObj);
  virtual PRBool    Resolve(JSContext *aContext, JSObject *aObj, jsval aID,
                            PRBool *aDidDefineProperty);
  virtual PRBool    Convert(JSContext *aContext, JSObject *aObj, jsval aID);
  virtual void      Finalize(JSContext *aContext, JSObject *aObj);

protected:
  nsGenericHTMLLeafElement mInner;
};

nsresult
NS_NewHTMLSpacerElement(nsIHTMLContent** aInstancePtrResult,
                        nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);
  NS_ENSURE_ARG_POINTER(aNodeInfo);

  nsIHTMLContent* it = new nsHTMLSpacerElement(aNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIHTMLContent), (void**) aInstancePtrResult);
}


nsHTMLSpacerElement::nsHTMLSpacerElement(nsINodeInfo *aNodeInfo)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aNodeInfo);
}

nsHTMLSpacerElement::~nsHTMLSpacerElement()
{
}

NS_IMPL_ADDREF(nsHTMLSpacerElement)

NS_IMPL_RELEASE(nsHTMLSpacerElement)

nsresult
nsHTMLSpacerElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  // Note that this has to stay above the generic element
  // QI macro, since it overrides the nsIJSScriptObject implementation
  // from the generic element.
  if (aIID.Equals(NS_GET_IID(nsIJSScriptObject))) {
    nsIJSScriptObject* tmp = this;
    *aInstancePtr = (void*) tmp;
    AddRef();
    return NS_OK;
  }                                                             
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  return NS_NOINTERFACE;
}

nsresult
nsHTMLSpacerElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLSpacerElement* it = new nsHTMLSpacerElement(mInner.mNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(NS_GET_IID(nsIDOMNode), (void**) aReturn);
}

NS_IMETHODIMP
nsHTMLSpacerElement::StringToAttribute(nsIAtom* aAttribute,
                                       const nsAReadableString& aValue,
                                       nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::size) {
    if (nsGenericHTMLElement::ParseValue(aValue, 0, aResult, eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::align) {
    if (nsGenericHTMLElement::ParseAlignValue(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if ((aAttribute == nsHTMLAtoms::width) ||
           (aAttribute == nsHTMLAtoms::height)) {
    if (nsGenericHTMLElement::ParseValueOrPercent(aValue, aResult,
                                                  eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLSpacerElement::AttributeToString(nsIAtom* aAttribute,
                                       const nsHTMLValue& aValue,
                                       nsAWritableString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::align) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      nsGenericHTMLElement::AlignValueToString(aValue, aResult);
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
  NS_PRECONDITION(aContext, "no style context");
  NS_PRECONDITION(aPresContext, "no pres context");

  if (aAttributes && aPresContext && aContext) {
    nsHTMLValue value;
    nsStyleDisplay* display = (nsStyleDisplay*)
      aContext->GetMutableStyleData(eStyleStruct_Display);
    nsStylePosition* position = (nsStylePosition*)
      aContext->GetMutableStyleData(eStyleStruct_Position);
    aAttributes->GetAttribute(nsHTMLAtoms::align, value);
    if (eHTMLUnit_Enumerated == value.GetUnit()) {
      switch (value.GetIntValue()) {
      case NS_STYLE_TEXT_ALIGN_LEFT:
        display->mFloats = NS_STYLE_FLOAT_LEFT;
        break;
      case NS_STYLE_TEXT_ALIGN_RIGHT:
        display->mFloats = NS_STYLE_FLOAT_RIGHT;
        break;
      default:
        break;
      }
    }

    nsGenericHTMLElement::MapImageAttributesInto(aAttributes, aContext, aPresContext);

    float p2t;
    aPresContext->GetScaledPixelsToTwips(&p2t);
    PRBool typeIsBlock = PR_FALSE;
    aAttributes->GetAttribute(nsHTMLAtoms::type, value);
    if (eHTMLUnit_String == value.GetUnit()) {
      nsAutoString tmp;
      value.GetStringValue(tmp);
      if (tmp.EqualsIgnoreCase("line") ||
          tmp.EqualsIgnoreCase("vert") ||
          tmp.EqualsIgnoreCase("vertical") ||
          tmp.EqualsIgnoreCase("block")) {
        // This is not strictly 100% compatible: if the spacer is given
        // a width of zero then it is basically ignored.
        display->mDisplay = NS_STYLE_DISPLAY_BLOCK;
        if (tmp.EqualsIgnoreCase("block")) {
          typeIsBlock = PR_TRUE;
        }
      }
    }
    if (typeIsBlock)
    {
      // width: value
      aAttributes->GetAttribute(nsHTMLAtoms::width, value);
      if (value.GetUnit() == eHTMLUnit_Pixel) {
        nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
        position->mWidth.SetCoordValue(twips);
      }
      else if (value.GetUnit() == eHTMLUnit_Percent) {
        position->mWidth.SetPercentValue(value.GetPercentValue());
      }
      // height: value
      aAttributes->GetAttribute(nsHTMLAtoms::height, value);
      if (value.GetUnit() == eHTMLUnit_Pixel) {
        nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
        position->mHeight.SetCoordValue(twips);
      }
      else if (value.GetUnit() == eHTMLUnit_Percent) {
        position->mHeight.SetPercentValue(value.GetPercentValue());
      }
    }
    else 
    {
      // size: value
      aAttributes->GetAttribute(nsHTMLAtoms::size, value);
      if (value.GetUnit() == eHTMLUnit_Pixel) {
        nscoord twips = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
        position->mWidth.SetCoordValue(twips);
      }
    }
  }
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
}


NS_IMETHODIMP
nsHTMLSpacerElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                             PRInt32& aHint) const
{
  if ((aAttribute == nsHTMLAtoms::usemap) ||
      (aAttribute == nsHTMLAtoms::ismap)) {
    aHint = NS_STYLE_HINT_FRAMECHANGE;
  }
  else if (aAttribute == nsHTMLAtoms::align) {
    aHint = NS_STYLE_HINT_REFLOW;
  }
  else if (! nsGenericHTMLElement::GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    if (! nsGenericHTMLElement::GetImageMappedAttributesImpact(aAttribute, aHint)) {
      if (! nsGenericHTMLElement::GetImageBorderAttributeImpact(aAttribute, aHint)) {
        aHint = NS_STYLE_HINT_CONTENT;
      }
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsHTMLSpacerElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                                 nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLSpacerElement::HandleDOMEvent(nsIPresContext* aPresContext,
                                   nsEvent* aEvent,
                                   nsIDOMEvent** aDOMEvent,
                                   PRUint32 aFlags,
                                   nsEventStatus* aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}

PRBool    
nsHTMLSpacerElement::AddProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp)
{
  return mInner.AddProperty(aContext, aObj, aID, aVp);
}

PRBool    
nsHTMLSpacerElement::DeleteProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp)
{
  return mInner.DeleteProperty(aContext, aObj, aID, aVp);
}

PRBool    
nsHTMLSpacerElement::GetProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp)
{
  return mInner.GetProperty(aContext, aObj, aID, aVp);
}

PRBool    
nsHTMLSpacerElement::SetProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp)
{
  return NS_OK;
}

PRBool    
nsHTMLSpacerElement::EnumerateProperty(JSContext *aContext, JSObject *aObj)
{
  return mInner.EnumerateProperty(aContext, aObj);
}

PRBool    
nsHTMLSpacerElement::Resolve(JSContext *aContext, JSObject *aObj, jsval aID,
                             PRBool *aDidDefineProperty)
{
  return mInner.Resolve(aContext, aObj, aID, aDidDefineProperty);
}

PRBool    
nsHTMLSpacerElement::Convert(JSContext *aContext, JSObject *aObj, jsval aID)
{
  return mInner.Convert(aContext, aObj, aID);
}

void      
nsHTMLSpacerElement::Finalize(JSContext *aContext, JSObject *aObj)
{
  mInner.Finalize(aContext, aObj);
}

NS_IMETHODIMP
nsHTMLSpacerElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  return mInner.SizeOf(aSizer, aResult, sizeof(*this));
}



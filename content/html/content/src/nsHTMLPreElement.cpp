/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsIDOMHTMLPreElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIHTMLAttributes.h"

// XXX wrap, variable, cols, tabstop

static NS_DEFINE_IID(kIDOMHTMLPreElementIID, NS_IDOMHTMLPREELEMENT_IID);

class nsHTMLPreElement : public nsIDOMHTMLPreElement,
                         public nsIScriptObjectOwner,
                         public nsIDOMEventReceiver,
                         public nsIHTMLContent
{
public:
  nsHTMLPreElement(nsIAtom* aTag);
  virtual ~nsHTMLPreElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLPreElement
  NS_IMETHOD GetWidth(PRInt32* aWidth);
  NS_IMETHOD SetWidth(PRInt32 aWidth);

  // nsIScriptObjectOwner
  NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC(mInner)

  // nsIDOMEventReceiver
  NS_IMPL_IDOMEVENTRECEIVER_USING_GENERIC(mInner)

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC(mInner)

  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC(mInner)

protected:
  nsGenericHTMLContainerElement mInner;
};

nsresult
NS_NewHTMLPreElement(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLPreElement(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLPreElement::nsHTMLPreElement(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
}

nsHTMLPreElement::~nsHTMLPreElement()
{
}

NS_IMPL_ADDREF(nsHTMLPreElement)

NS_IMPL_RELEASE(nsHTMLPreElement)

nsresult
nsHTMLPreElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLPreElementIID)) {
    nsIDOMHTMLPreElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    mRefCnt++;
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLPreElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLPreElement* it = new nsHTMLPreElement(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMPL_INT_ATTR(nsHTMLPreElement, Width, width)

NS_IMETHODIMP
nsHTMLPreElement::StringToAttribute(nsIAtom* aAttribute,
                                    const nsString& aValue,
                                    nsHTMLValue& aResult)
{
  if ((aAttribute == nsHTMLAtoms::wrap) ||
      (aAttribute == nsHTMLAtoms::variable)) {
    aResult.SetEmptyValue();
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  if (aAttribute == nsHTMLAtoms::cols) {
    if (nsGenericHTMLElement::ParseValue(aValue, 0, aResult,
                                         eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  if (aAttribute == nsHTMLAtoms::width) {
    if (nsGenericHTMLElement::ParseValue(aValue, 0, aResult,
                                         eHTMLUnit_Integer)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  if (aAttribute == nsHTMLAtoms::tabstop) {
    PRInt32 ec, tabstop = aValue.ToInteger(&ec);
    if (tabstop <= 0) {
      tabstop = 8;
    }
    aResult.SetIntValue(tabstop, eHTMLUnit_Integer);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLPreElement::AttributeToString(nsIAtom* aAttribute,
                                    const nsHTMLValue& aValue,
                                    nsString& aResult) const
{
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

static void
MapAttributesInto(nsIHTMLAttributes* aAttributes,
                  nsIStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  if (nsnull != aAttributes) {
    nsHTMLValue value;

    // wrap: empty
    aAttributes->GetAttribute(nsHTMLAtoms::wrap, value);
    if (value.GetUnit() == eHTMLUnit_Empty) {
      nsStyleText* text = (nsStyleText*)
        aContext->GetMutableStyleData(eStyleStruct_Text);
      text->mWhiteSpace = NS_STYLE_WHITESPACE_MOZ_PRE_WRAP;
    }
      
    // variable: empty
    aAttributes->GetAttribute(nsHTMLAtoms::variable, value);
    if (value.GetUnit() == eHTMLUnit_Empty) {
      nsStyleFont* font = (nsStyleFont*)
        aContext->GetMutableStyleData(eStyleStruct_Font);
      font->mFont.name = "serif";
    }

    // cols: int (nav4 attribute)
    aAttributes->GetAttribute(nsHTMLAtoms::cols, value);
    if (value.GetUnit() == eHTMLUnit_Integer) {
      nsStylePosition* position = (nsStylePosition*)
        aContext->GetMutableStyleData(eStyleStruct_Position);
      position->mWidth.SetIntValue(value.GetIntValue(), eStyleUnit_Chars);
      // Force wrap property on since we want to wrap at a width
      // boundary not just a newline.
      nsStyleText* text = (nsStyleText*)
        aContext->GetMutableStyleData(eStyleStruct_Text);
      text->mWhiteSpace = NS_STYLE_WHITESPACE_MOZ_PRE_WRAP;
    }

    // width: int (html4 attribute == nav4 cols)
    aAttributes->GetAttribute(nsHTMLAtoms::width, value);
    if (value.GetUnit() == eHTMLUnit_Integer) {
      nsStylePosition* position = (nsStylePosition*)
        aContext->GetMutableStyleData(eStyleStruct_Position);
      position->mWidth.SetIntValue(value.GetIntValue(), eStyleUnit_Chars);
      // Force wrap property on since we want to wrap at a width
      // boundary not just a newline.
      nsStyleText* text = (nsStyleText*)
        aContext->GetMutableStyleData(eStyleStruct_Text);
      text->mWhiteSpace = NS_STYLE_WHITESPACE_MOZ_PRE_WRAP;
    }

    // tabstop: int
    aAttributes->GetAttribute(nsHTMLAtoms::tabstop, value);
    if (value.GetUnit() == eHTMLUnit_Integer) {
      // XXX set
    }
  }
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext,
                                                aPresContext);
}

NS_IMETHODIMP
nsHTMLPreElement::GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const
{
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLPreElement::HandleDOMEvent(nsIPresContext& aPresContext,
                                 nsEvent* aEvent,
                                 nsIDOMEvent** aDOMEvent,
                                 PRUint32 aFlags,
                                 nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}

NS_IMETHODIMP
nsHTMLPreElement::GetStyleHintForAttributeChange(
    const nsIAtom* aAttribute,
    PRInt32 *aHint) const
{
  nsGenericHTMLElement::GetStyleHintForCommonAttributes(this, aAttribute, aHint);
  return NS_OK;
}

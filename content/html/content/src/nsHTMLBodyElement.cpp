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
#include "nsIDOMHTMLBodyElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsStyleUtil.h"
#include "nsIDocument.h"
#include "nsIHTMLDocument.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIStyleRule.h"
#include "nsIWebShell.h"
#include "nsIHTMLAttributes.h"
#include "nsIHTMLContentContainer.h"

static NS_DEFINE_IID(kIHTMLDocumentIID, NS_IHTMLDOCUMENT_IID);
static NS_DEFINE_IID(kIStyleRuleIID, NS_ISTYLE_RULE_IID);
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kIDOMHTMLBodyElementIID, NS_IDOMHTMLBODYELEMENT_IID);
static NS_DEFINE_IID(kIHTMLContentContainerIID, NS_IHTMLCONTENTCONTAINER_IID);

//----------------------------------------------------------------------

class nsHTMLBodyElement;

class BodyRule: public nsIStyleRule {
public:
  BodyRule(nsHTMLBodyElement* aPart, nsIHTMLStyleSheet* aSheet);
  ~BodyRule();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Equals(const nsIStyleRule* aRule, PRBool& aResult) const;
  NS_IMETHOD HashValue(PRUint32& aValue) const;
  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const;

  // Strength is an out-of-band weighting, always 0 here
  NS_IMETHOD GetStrength(PRInt32& aStrength);

  NS_IMETHOD MapStyleInto(nsIStyleContext* aContext,
                          nsIPresContext* aPresContext);

  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  nsHTMLBodyElement*  mPart;  // not ref-counted, cleared by content 
  nsIHTMLStyleSheet*  mSheet; // not ref-counted, cleared by content
};

//----------------------------------------------------------------------

// special subclass of inner class to override set document
class nsBodyInner: public nsGenericHTMLContainerElement
{
public:
  nsBodyInner();
  ~nsBodyInner();

  nsresult SetDocument(nsIDocument* aDocument, PRBool aDeep);

  BodyRule*   mStyleRule;
};

nsBodyInner::nsBodyInner()
  : nsGenericHTMLContainerElement(),
    mStyleRule(nsnull)
{
}

nsBodyInner::~nsBodyInner()
{
  if (nsnull != mStyleRule) {
    mStyleRule->mPart = nsnull;
    mStyleRule->mSheet = nsnull;
    NS_RELEASE(mStyleRule);
  }
}

nsresult nsBodyInner::SetDocument(nsIDocument* aDocument, PRBool aDeep)
{
  if (nsnull != mStyleRule) {
    mStyleRule->mPart = nsnull;
    mStyleRule->mSheet = nsnull;
    NS_RELEASE(mStyleRule); // destroy old style rule since the sheet will probably change
  }
  return nsGenericHTMLContainerElement::SetDocument(aDocument, aDeep);
}

//----------------------------------------------------------------------

class nsHTMLBodyElement : public nsIDOMHTMLBodyElement,
                          public nsIScriptObjectOwner,
                          public nsIDOMEventReceiver,
                          public nsIHTMLContent
{
public:
  nsHTMLBodyElement(nsIAtom* aTag);
  ~nsHTMLBodyElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLBodyElement
  NS_IMETHOD GetALink(nsString& aALink);
  NS_IMETHOD SetALink(const nsString& aALink);
  NS_IMETHOD GetBackground(nsString& aBackground);
  NS_IMETHOD SetBackground(const nsString& aBackground);
  NS_IMETHOD GetBgColor(nsString& aBgColor);
  NS_IMETHOD SetBgColor(const nsString& aBgColor);
  NS_IMETHOD GetLink(nsString& aLink);
  NS_IMETHOD SetLink(const nsString& aLink);
  NS_IMETHOD GetText(nsString& aText);
  NS_IMETHOD SetText(const nsString& aText);
  NS_IMETHOD GetVLink(nsString& aVLink);
  NS_IMETHOD SetVLink(const nsString& aVLink);

  // nsIScriptObjectOwner
  NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC(mInner)

  // nsIDOMEventReceiver
  NS_IMPL_IDOMEVENTRECEIVER_USING_GENERIC(mInner)

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC(mInner)

  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC2(mInner)

protected:
  nsBodyInner mInner;

friend BodyRule;
};


//----------------------------------------------------------------------

BodyRule::BodyRule(nsHTMLBodyElement* aPart, nsIHTMLStyleSheet* aSheet)
{
  NS_INIT_REFCNT();
  mPart = aPart;
  mSheet = aSheet;
}

BodyRule::~BodyRule()
{
}

NS_IMPL_ISUPPORTS(BodyRule, kIStyleRuleIID);

NS_IMETHODIMP
BodyRule::Equals(const nsIStyleRule* aRule, PRBool& aResult) const
{
  aResult = PRBool(this == aRule);
  return NS_OK;
}

NS_IMETHODIMP
BodyRule::HashValue(PRUint32& aValue) const
{
  aValue = (PRUint32)(mPart);
  return NS_OK;
}

NS_IMETHODIMP
BodyRule::GetStyleSheet(nsIStyleSheet*& aSheet) const
{
  NS_IF_ADDREF(mSheet);
  aSheet = mSheet;
  return NS_OK;
}

// Strength is an out-of-band weighting, useful for mapping CSS ! important
// always 0 here
NS_IMETHODIMP
BodyRule::GetStrength(PRInt32& aStrength)
{
  aStrength = 0;
  return NS_OK;
}

NS_IMETHODIMP
BodyRule::MapStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext)
{
  if (nsnull != mPart) {

    nsStyleSpacing* styleSpacing = (nsStyleSpacing*)(aContext->GetMutableStyleData(eStyleStruct_Spacing));

    if (nsnull != styleSpacing) {
      nsHTMLValue   value;
      nsStyleCoord  zero(0);
      PRInt32       count = 0;
      PRInt32       attrCount;
      mPart->GetAttributeCount(attrCount);

      if (0 < attrCount) {
        // if marginwidth/marginheigth is set in our attribute zero out left,right/top,bottom padding
        // nsBodyFrame::DidSetStyleContext will add the appropriate values to padding 
        mPart->GetAttribute(nsHTMLAtoms::marginwidth, value);
        if (eHTMLUnit_Pixel == value.GetUnit()) {
          styleSpacing->mPadding.SetLeft(zero);
          styleSpacing->mPadding.SetRight(zero);
          count++;
        }

        mPart->GetAttribute(nsHTMLAtoms::marginheight, value);
        if (eHTMLUnit_Pixel == value.GetUnit()) {
          styleSpacing->mPadding.SetTop(zero);
          styleSpacing->mPadding.SetBottom(zero);
          count++;
        }

        if (count < attrCount) {  // more to go...
          nsMapAttributesFunc func;
          mPart->GetAttributeMappingFunction(func);
          (*func)(mPart->mInner.mAttributes, aContext, aPresContext);
        }
      }

      if (count < 2) {
        // if marginwidth or marginheight is set in the web shell zero out left,right,top,bottom padding
        // nsBodyFrame::DidSetStyleContext will add the appropriate values to padding 
        nsISupports* container;
        aPresContext->GetContainer(&container);
        if (nsnull != container) {
          nsIWebShell* webShell = nsnull;
          container->QueryInterface(kIWebShellIID, (void**) &webShell);
          if (nsnull != webShell) {
            PRInt32 marginWidth, marginHeight;
            webShell->GetMarginWidth(marginWidth);
            webShell->GetMarginHeight(marginHeight);
            if ((marginWidth >= 0) || (marginHeight >= 0)) { // nav quirk
              styleSpacing->mPadding.SetLeft(zero);
              styleSpacing->mPadding.SetRight(zero);
              styleSpacing->mPadding.SetTop(zero);
              styleSpacing->mPadding.SetBottom(zero);
            }
            NS_RELEASE(webShell);
          }
          NS_RELEASE(container);
        }
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
BodyRule::List(FILE* out, PRInt32 aIndent) const
{
  return NS_OK;
}

//----------------------------------------------------------------------

nsresult
NS_NewHTMLBodyElement(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLBodyElement(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLBodyElement::nsHTMLBodyElement(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
}

nsHTMLBodyElement::~nsHTMLBodyElement()
{
}

NS_IMPL_ADDREF(nsHTMLBodyElement)

NS_IMPL_RELEASE(nsHTMLBodyElement)

nsresult
nsHTMLBodyElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLBodyElementIID)) {
    nsIDOMHTMLBodyElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    mRefCnt++;
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLBodyElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLBodyElement* it = new nsHTMLBodyElement(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMPL_STRING_ATTR(nsHTMLBodyElement, ALink, alink)
NS_IMPL_STRING_ATTR(nsHTMLBodyElement, Background, background)
NS_IMPL_STRING_ATTR(nsHTMLBodyElement, BgColor, bgcolor)
NS_IMPL_STRING_ATTR(nsHTMLBodyElement, Link, link)
NS_IMPL_STRING_ATTR(nsHTMLBodyElement, Text, text)
NS_IMPL_STRING_ATTR(nsHTMLBodyElement, VLink, vlink)

NS_IMETHODIMP
nsHTMLBodyElement::StringToAttribute(nsIAtom* aAttribute,
                                     const nsString& aValue,
                                     nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::background) {
    nsAutoString href(aValue);
    href.StripWhitespace();
    aResult.SetStringValue(href);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  if ((aAttribute == nsHTMLAtoms::bgcolor) ||
      (aAttribute == nsHTMLAtoms::text) ||
      (aAttribute == nsHTMLAtoms::link) ||
      (aAttribute == nsHTMLAtoms::alink) ||
      (aAttribute == nsHTMLAtoms::vlink)) {
    nsGenericHTMLElement::ParseColor(aValue, aResult);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  if ((aAttribute == nsHTMLAtoms::marginwidth) ||
      (aAttribute == nsHTMLAtoms::marginheight)) {
    nsGenericHTMLElement::ParseValue(aValue, 0, aResult, eHTMLUnit_Pixel);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLBodyElement::AttributeToString(nsIAtom* aAttribute,
                                     nsHTMLValue& aValue,
                                     nsString& aResult) const
{
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

static nscolor ColorNameToRGB(const nsHTMLValue& aValue)
{
  nsAutoString buffer;
  aValue.GetStringValue(buffer);
  char cbuf[40];
  buffer.ToCString(cbuf, sizeof(cbuf));

  nscolor color;
  NS_ColorNameToRGB(cbuf, &color);
  return color;
}

static void
MapAttributesInto(nsIHTMLAttributes* aAttributes,
                  nsIStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  if (nsnull != aAttributes) {
    nsHTMLValue value;
    nsGenericHTMLElement::MapBackgroundAttributesInto(aAttributes, aContext, aPresContext);

    aAttributes->GetAttribute(nsHTMLAtoms::text, value);
    if (eHTMLUnit_Color == value.GetUnit()) {
      nsStyleColor* color = (nsStyleColor*)
        aContext->GetMutableStyleData(eStyleStruct_Color);
      color->mColor = value.GetColorValue();
    }
    else if (eHTMLUnit_String == value.GetUnit()) {
      nsStyleColor* color = (nsStyleColor*)
        aContext->GetMutableStyleData(eStyleStruct_Color);
      color->mColor = ColorNameToRGB(value);
    }

    nsIPresShell* presShell = aPresContext->GetShell();
    if (nsnull != presShell) {
      nsIDocument*  doc = presShell->GetDocument();
      if (nsnull != doc) {
        nsIHTMLContentContainer*  htmlContainer;
        if (NS_OK == doc->QueryInterface(kIHTMLContentContainerIID,
                                         (void**)&htmlContainer)) {
          nsIHTMLStyleSheet* styleSheet;
          if (NS_OK == htmlContainer->GetAttributeStyleSheet(&styleSheet)) {
            aAttributes->GetAttribute(nsHTMLAtoms::link, value);
            if (eHTMLUnit_Color == value.GetUnit()) {
              styleSheet->SetLinkColor(value.GetColorValue());
            }
            else if (eHTMLUnit_String == value.GetUnit()) {
              styleSheet->SetLinkColor(ColorNameToRGB(value));
            }

            aAttributes->GetAttribute(nsHTMLAtoms::alink, value);
            if (eHTMLUnit_Color == value.GetUnit()) {
              styleSheet->SetActiveLinkColor(value.GetColorValue());
            }
            else if (eHTMLUnit_String == value.GetUnit()) {
              styleSheet->SetActiveLinkColor(ColorNameToRGB(value));
            }

            aAttributes->GetAttribute(nsHTMLAtoms::vlink, value);
            if (eHTMLUnit_Color == value.GetUnit()) {
              styleSheet->SetVisitedLinkColor(value.GetColorValue());
            }
            else if (eHTMLUnit_String == value.GetUnit()) {
              styleSheet->SetVisitedLinkColor(ColorNameToRGB(value));
            }
            NS_RELEASE(styleSheet);
          }
          NS_RELEASE(htmlContainer);
        }
        NS_RELEASE(doc);
      }
      NS_RELEASE(presShell);
    }

    // marginwidth/marginheight
    // XXX: see ua.css for related code in the BODY rule
    float p2t;
    aPresContext->GetScaledPixelsToTwips(p2t);
    nsStyleSpacing* spacing = (nsStyleSpacing*)
      aContext->GetMutableStyleData(eStyleStruct_Spacing);
    aAttributes->GetAttribute(nsHTMLAtoms::marginwidth, value);
    if (eHTMLUnit_Pixel == value.GetUnit()) {
      nscoord v = NSToCoordFloor(value.GetPixelValue() * p2t);
      nsStyleCoord c(v);
      spacing->mPadding.SetLeft(c);
      spacing->mPadding.SetRight(c);
    }
    aAttributes->GetAttribute(nsHTMLAtoms::marginheight, value);
    if (eHTMLUnit_Pixel == value.GetUnit()) {
      nscoord v = NSToCoordFloor(value.GetPixelValue() * p2t);
      nsStyleCoord c(v);
      spacing->mPadding.SetTop(c);
      spacing->mPadding.SetBottom(c);
    }

    // set up the basefont (defaults to 3)
    nsStyleFont* font = (nsStyleFont*)aContext->GetMutableStyleData(eStyleStruct_Font);
    const nsFont& defaultFont = aPresContext->GetDefaultFont(); 
    const nsFont& defaultFixedFont = aPresContext->GetDefaultFixedFont(); 
    PRInt32 scaler = aPresContext->GetFontScaler();
    float scaleFactor = nsStyleUtil::GetScalingFactor(scaler);
    font->mFont.size = nsStyleUtil::CalcFontPointSize(3, (PRInt32)defaultFont.size, scaleFactor);
    font->mFixedFont.size = nsStyleUtil::CalcFontPointSize(3, (PRInt32)defaultFixedFont.size, scaleFactor);
  }
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
}

NS_IMETHODIMP
nsHTMLBodyElement::GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const
{
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLBodyElement::HandleDOMEvent(nsIPresContext& aPresContext,
                                  nsEvent* aEvent,
                                  nsIDOMEvent** aDOMEvent,
                                  PRUint32 aFlags,
                                  nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}


static nsIHTMLStyleSheet* GetAttrStyleSheet(nsIDocument* aDocument)
{
  nsIHTMLStyleSheet*  sheet = nsnull;
  nsIHTMLContentContainer*  htmlContainer;
  
  if (nsnull != aDocument) {
    if (NS_OK == aDocument->QueryInterface(kIHTMLContentContainerIID, (void**)&htmlContainer)) {
      htmlContainer->GetAttributeStyleSheet(&sheet);
      NS_RELEASE(htmlContainer);
    }
  }
  NS_ASSERTION(nsnull != sheet, "can't get attribute style sheet");
  return sheet;
}

NS_IMETHODIMP
nsHTMLBodyElement::GetStyleRule(nsIStyleRule*& aResult)
{
  if (nsnull == mInner.mStyleRule) {
    nsIHTMLStyleSheet*  sheet = nsnull;

    if (nsnull != mInner.mDocument) {  // find style sheet
      sheet = GetAttrStyleSheet(mInner.mDocument);
    }

    mInner.mStyleRule = new BodyRule(this, sheet);
    NS_IF_RELEASE(sheet);
    NS_IF_ADDREF(mInner.mStyleRule);
  }
  NS_IF_ADDREF(mInner.mStyleRule);
  aResult = mInner.mStyleRule;
  return NS_OK;
}



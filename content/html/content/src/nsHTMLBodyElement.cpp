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
#include "nsCOMPtr.h"
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
#include "nsIHTMLCSSStyleSheet.h"
#include "nsICSSStyleRule.h"
#include "nsIWebShell.h"
#include "nsIHTMLAttributes.h"
#include "nsIHTMLContentContainer.h"

static NS_DEFINE_IID(kIHTMLDocumentIID, NS_IHTMLDOCUMENT_IID);
static NS_DEFINE_IID(kIStyleRuleIID, NS_ISTYLE_RULE_IID);
static NS_DEFINE_IID(kICSSStyleRuleIID, NS_ICSS_STYLE_RULE_IID);
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kIDOMHTMLBodyElementIID, NS_IDOMHTMLBODYELEMENT_IID);
static NS_DEFINE_IID(kIHTMLContentContainerIID, NS_IHTMLCONTENTCONTAINER_IID);

//----------------------------------------------------------------------

class nsHTMLBodyElement;

class BodyRule: public nsIStyleRule {
public:
  BodyRule(nsHTMLBodyElement* aPart, nsIHTMLStyleSheet* aSheet);
  virtual ~BodyRule();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Equals(const nsIStyleRule* aRule, PRBool& aResult) const;
  NS_IMETHOD HashValue(PRUint32& aValue) const;
  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const;

  // Strength is an out-of-band weighting, always 0 here
  NS_IMETHOD GetStrength(PRInt32& aStrength) const;

  NS_IMETHOD MapStyleInto(nsIStyleContext* aContext,
                          nsIPresContext* aPresContext);

  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  nsHTMLBodyElement*  mPart;  // not ref-counted, cleared by content 
  nsIHTMLStyleSheet*  mSheet; // not ref-counted, cleared by content
};

//----------------------------------------------------------------------

class BodyFixupRule : public nsIStyleRule {
public:
  BodyFixupRule(nsHTMLBodyElement* aPart, nsIHTMLCSSStyleSheet* aSheet);
  virtual ~BodyFixupRule();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Equals(const nsIStyleRule* aRule, PRBool& aValue) const;
  NS_IMETHOD HashValue(PRUint32& aValue) const;
  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const;
  // Strength is an out-of-band weighting, always maxint here
  NS_IMETHOD GetStrength(PRInt32& aStrength) const;

  NS_IMETHOD MapStyleInto(nsIStyleContext* aContext, 
                          nsIPresContext* aPresContext);

  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  nsHTMLBodyElement*    mPart;  // not ref-counted, cleared by content 
  nsIHTMLCSSStyleSheet* mSheet; // not ref-counted, cleared by content 
};

//----------------------------------------------------------------------

// special subclass of inner class to override set document
class nsBodyInner: public nsGenericHTMLContainerElement
{
public:
  nsBodyInner();
  virtual ~nsBodyInner();

  nsresult SetDocument(nsIDocument* aDocument, PRBool aDeep);

  BodyRule*       mContentStyleRule;
  BodyFixupRule*  mInlineStyleRule;
};

nsBodyInner::nsBodyInner()
  : nsGenericHTMLContainerElement(),
    mContentStyleRule(nsnull),
    mInlineStyleRule(nsnull)
{
}

nsBodyInner::~nsBodyInner()
{
  if (nsnull != mContentStyleRule) {
    mContentStyleRule->mPart = nsnull;
    mContentStyleRule->mSheet = nsnull;
    NS_RELEASE(mContentStyleRule);
  }
  if (nsnull != mInlineStyleRule) {
    mInlineStyleRule->mPart = nsnull;
    mInlineStyleRule->mSheet = nsnull;
    NS_RELEASE(mInlineStyleRule);
  }
}

nsresult nsBodyInner::SetDocument(nsIDocument* aDocument, PRBool aDeep)
{
  if (nsnull != mContentStyleRule) {
    mContentStyleRule->mPart = nsnull;
    mContentStyleRule->mSheet = nsnull;
    NS_RELEASE(mContentStyleRule); // destroy old style rule since the sheet will probably change
  }
  if (nsnull != mInlineStyleRule) {
    mInlineStyleRule->mPart = nsnull;
    mInlineStyleRule->mSheet = nsnull;
    NS_RELEASE(mInlineStyleRule); // destroy old style rule since the sheet will probably change
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
  virtual ~nsHTMLBodyElement();

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
friend BodyFixupRule;
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
BodyRule::GetStrength(PRInt32& aStrength) const
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
      PRInt32       count = 0;
      PRInt32       attrCount;
      float         p2t;
      mPart->GetAttributeCount(attrCount);
      aPresContext->GetScaledPixelsToTwips(&p2t);
      nscoord bodyMarginWidth  = -1;
      nscoord bodyMarginHeight = -1;

      if (0 < attrCount) {
        // if marginwidth/marginheigth is set reflect them as 'margin'
        mPart->GetHTMLAttribute(nsHTMLAtoms::marginwidth, value);
        if (eHTMLUnit_Pixel == value.GetUnit()) {
          bodyMarginWidth = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
          if (bodyMarginWidth < 0) {
            bodyMarginWidth = 0;
          }
          nsStyleCoord  widthCoord(bodyMarginWidth);
          styleSpacing->mMargin.SetLeft(widthCoord);
          styleSpacing->mMargin.SetRight(widthCoord);
          count++;
        }

        mPart->GetHTMLAttribute(nsHTMLAtoms::marginheight, value);
        if (eHTMLUnit_Pixel == value.GetUnit()) {
          bodyMarginHeight = NSIntPixelsToTwips(value.GetPixelValue(), p2t);
          if (bodyMarginHeight < 0) {
            bodyMarginHeight = 0;
          }
      
          nsStyleCoord  heightCoord(bodyMarginHeight);
          styleSpacing->mMargin.SetTop(heightCoord);
          styleSpacing->mMargin.SetBottom(heightCoord);
          count++;
        }

        if (count < attrCount) {  // more to go...
          nsMapAttributesFunc func;
          mPart->GetAttributeMappingFunction(func);
          (*func)(mPart->mInner.mAttributes, aContext, aPresContext);
        }
      }

      // XXX This is all pretty hokey...

      // if marginwidth or marginheight is set in the <frame> and not set in the <body>
      // reflect them as margin in the <body>
      if ((0 > bodyMarginWidth) || (0 > bodyMarginHeight)) {
        nsISupports* container;
        aPresContext->GetContainer(&container);
        if (nsnull != container) {
          nsCompatibility mode;
          aPresContext->GetCompatibilityMode(&mode);
          nsIWebShell* webShell = nsnull;
          container->QueryInterface(kIWebShellIID, (void**) &webShell);
          if (nsnull != webShell) {
            nscoord pixel = NSIntPixelsToTwips(1, p2t);
            nscoord frameMarginWidth, frameMarginHeight; 
            webShell->GetMarginWidth(frameMarginWidth); // -1 indicates not set   
            webShell->GetMarginHeight(frameMarginHeight); 
            if ((frameMarginWidth >= 0) && (0 > bodyMarginWidth)) { // set in <frame> & not in <body> 
              if (eCompatibility_NavQuirks == mode) { // allow 0 margins
                if ((0 > bodyMarginHeight) && (0 > frameMarginHeight)) { // another nav quirk 
                  frameMarginHeight = 0;
                }
              } else { // margins are at least 1 pixel
                if (0 == frameMarginWidth) {
                  frameMarginWidth = pixel;
                }
              }
            }
            if ((frameMarginHeight >= 0) && (0 > bodyMarginHeight)) { // set in <frame> & not in <body> 
              if (eCompatibility_NavQuirks == mode) { // allow 0 margins
                if ((0 > bodyMarginWidth) && (0 > frameMarginWidth)) { // another nav quirk 
                  frameMarginWidth = 0;
                }
              } else { // margins are at least 1 pixel
                if (0 == frameMarginHeight) {
                  frameMarginHeight = pixel;
                }
              }
            }

            if ((0 > bodyMarginWidth) && (frameMarginWidth >= 0)) {
              nsStyleCoord widthCoord(frameMarginWidth);
              styleSpacing->mMargin.SetLeft(widthCoord);
              styleSpacing->mMargin.SetRight(widthCoord);
            }

            if ((0 > bodyMarginHeight) && (frameMarginHeight >= 0)) {
              nsStyleCoord heightCoord(frameMarginHeight);
              styleSpacing->mMargin.SetTop(heightCoord);
              styleSpacing->mMargin.SetBottom(heightCoord);
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

BodyFixupRule::BodyFixupRule(nsHTMLBodyElement* aPart, nsIHTMLCSSStyleSheet* aSheet)
  : mPart(aPart),
    mSheet(aSheet)
{
  NS_INIT_REFCNT();
}

BodyFixupRule::~BodyFixupRule()
{
}

NS_IMPL_ADDREF(BodyFixupRule);
NS_IMPL_RELEASE(BodyFixupRule);

nsresult BodyFixupRule::QueryInterface(const nsIID& aIID,
                                       void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  if (aIID.Equals(kIStyleRuleIID)) {
    *aInstancePtrResult = (void*) ((nsIStyleRule*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kICSSStyleRuleIID)) {
    nsIStyleRule* inlineRule = nsnull;
    if (nsnull != mPart) {
      mPart->mInner.GetInlineStyleRule(inlineRule);
    }
    if (nsnull != inlineRule) {
      nsresult result = inlineRule->QueryInterface(kICSSStyleRuleIID, aInstancePtrResult);
      NS_RELEASE(inlineRule);
      return result;
    }
    return NS_NOINTERFACE;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*) ((nsISupports*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP
BodyFixupRule::Equals(const nsIStyleRule* aRule, PRBool& aResult) const
{
  aResult = PRBool(this == aRule);
  return NS_OK;
}

NS_IMETHODIMP
BodyFixupRule::HashValue(PRUint32& aValue) const
{
  aValue = (PRUint32)(mPart);
  return NS_OK;
}

NS_IMETHODIMP
BodyFixupRule::GetStyleSheet(nsIStyleSheet*& aSheet) const
{
  NS_IF_ADDREF(mSheet);
  aSheet = mSheet;
  return NS_OK;
}

// Strength is an out-of-band weighting, always MaxInt here
NS_IMETHODIMP
BodyFixupRule::GetStrength(PRInt32& aStrength) const
{
  aStrength = 2000000000;
  return NS_OK;
}

NS_IMETHODIMP
BodyFixupRule::MapStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext)
{
  // Invoke style mapping of inline rule
  nsIStyleRule* inlineRule = nsnull;
  if (nsnull != mPart) {
    mPart->mInner.GetInlineStyleRule(inlineRule);
    if (nsnull != inlineRule) {
      inlineRule->MapStyleInto(aContext, aPresContext);
      NS_RELEASE(inlineRule);
    }
  }

  // XXX do any other body processing here
  
  const nsStyleColor* styleColor;
  styleColor = (const nsStyleColor*)aContext->GetStyleData(eStyleStruct_Color);

  // Fixup default presentation colors (NAV QUIRK)
  if (nsnull != styleColor) {
    aPresContext->SetDefaultColor(styleColor->mColor);
    aPresContext->SetDefaultBackgroundColor(styleColor->mBackgroundColor);
  }
  
  // Use the CSS precedence rules for dealing with BODY background: if the value
  // of the 'background' property for the HTML element is different from
  // 'transparent' then use it, else use the value of the 'background' property
  // for the BODY element

  // See if the BODY has a background specified
  if (!styleColor->BackgroundIsTransparent()) {
    // Get the parent style context
    nsIStyleContext*  parentContext = aContext->GetParent();

    // Look at its 'background' property
    const nsStyleColor* parentStyleColor;
    parentStyleColor = (const nsStyleColor*)parentContext->GetStyleData(eStyleStruct_Color);

    // See if it's 'transparent'
    if (parentStyleColor->BackgroundIsTransparent()) {
      // Have the parent (initial containing block) use the BODY's background
      nsStyleColor* mutableStyleColor;
      mutableStyleColor = (nsStyleColor*)parentContext->GetMutableStyleData(eStyleStruct_Color);

      mutableStyleColor->mBackgroundAttachment = styleColor->mBackgroundAttachment;
      mutableStyleColor->mBackgroundFlags = styleColor->mBackgroundFlags;
      mutableStyleColor->mBackgroundRepeat = styleColor->mBackgroundRepeat;
      mutableStyleColor->mBackgroundColor = styleColor->mBackgroundColor;
      mutableStyleColor->mBackgroundXPosition = styleColor->mBackgroundXPosition;
      mutableStyleColor->mBackgroundYPosition = styleColor->mBackgroundYPosition;
      mutableStyleColor->mBackgroundImage = styleColor->mBackgroundImage;

      // Reset the BODY's background to transparent
      mutableStyleColor = (nsStyleColor*)aContext->GetMutableStyleData(eStyleStruct_Color);
      mutableStyleColor->mBackgroundFlags = NS_STYLE_BG_COLOR_TRANSPARENT |
                                            NS_STYLE_BG_IMAGE_NONE;
      mutableStyleColor->mBackgroundImage.SetLength(0);
    }
    NS_RELEASE(parentContext);
  }


  return NS_OK;
}

NS_IMETHODIMP
BodyFixupRule::List(FILE* out, PRInt32 aIndent) const
{
  // Indent
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);
 
  fputs("Special BODY tag fixup rule\n", out);
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
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
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
                                     const nsHTMLValue& aValue,
                                     nsString& aResult) const
{
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

static PRBool ColorNameToRGB(const nsHTMLValue& aValue, nscolor* aColor)
{
  nsAutoString buffer;
  aValue.GetStringValue(buffer);
  char cbuf[40];
  buffer.ToCString(cbuf, sizeof(cbuf));

  return NS_ColorNameToRGB(cbuf, aColor);
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
      nscolor backgroundColor;
      if (ColorNameToRGB(value, &backgroundColor)) {
        nsStyleColor* color = (nsStyleColor*)
          aContext->GetMutableStyleData(eStyleStruct_Color);
        color->mColor = backgroundColor;
      }
    }

    nsCOMPtr<nsIPresShell> presShell;
    aPresContext->GetShell(getter_AddRefs(presShell));
    if (presShell) {
      nsCOMPtr<nsIDocument> doc;
      presShell->GetDocument(getter_AddRefs(doc));
      if (doc) {
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
              nscolor linkColor;
              if (ColorNameToRGB(value, &linkColor)) {
                styleSheet->SetLinkColor(linkColor);
              }
            }

            aAttributes->GetAttribute(nsHTMLAtoms::alink, value);
            if (eHTMLUnit_Color == value.GetUnit()) {
              styleSheet->SetActiveLinkColor(value.GetColorValue());
            }
            else if (eHTMLUnit_String == value.GetUnit()) {
              nscolor linkColor;
              if (ColorNameToRGB(value, &linkColor)) {
                styleSheet->SetActiveLinkColor(linkColor);
              }
            }

            aAttributes->GetAttribute(nsHTMLAtoms::vlink, value);
            if (eHTMLUnit_Color == value.GetUnit()) {
              styleSheet->SetVisitedLinkColor(value.GetColorValue());
            }
            else if (eHTMLUnit_String == value.GetUnit()) {
              nscolor linkColor;
              if (ColorNameToRGB(value, &linkColor)) {
                styleSheet->SetVisitedLinkColor(linkColor);
              }
            }
            NS_RELEASE(styleSheet);
          }
          NS_RELEASE(htmlContainer);
        }
      }
    }

    // marginwidth/marginheight
    // XXX: see ua.css for related code in the BODY rule
    float p2t;
    aPresContext->GetScaledPixelsToTwips(&p2t);
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
    const nsFont& defaultFont = aPresContext->GetDefaultFontDeprecated(); 
    const nsFont& defaultFixedFont = aPresContext->GetDefaultFixedFontDeprecated(); 
    PRInt32 scaler;
    aPresContext->GetFontScaler(&scaler);
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
nsHTMLBodyElement::GetContentStyleRule(nsIStyleRule*& aResult)
{
  if (nsnull == mInner.mContentStyleRule) {
    nsIHTMLStyleSheet*  sheet = nsnull;

    if (nsnull != mInner.mDocument) {  // find style sheet
      sheet = GetAttrStyleSheet(mInner.mDocument);
    }

    mInner.mContentStyleRule = new BodyRule(this, sheet);
    NS_IF_RELEASE(sheet);
    NS_IF_ADDREF(mInner.mContentStyleRule);
  }
  NS_IF_ADDREF(mInner.mContentStyleRule);
  aResult = mInner.mContentStyleRule;
  return NS_OK;
}

static nsIHTMLCSSStyleSheet* GetInlineStyleSheet(nsIDocument* aDocument)
{
  nsIHTMLCSSStyleSheet*  sheet = nsnull;
  nsIHTMLContentContainer*  htmlContainer;
  
  if (nsnull != aDocument) {
    if (NS_OK == aDocument->QueryInterface(kIHTMLContentContainerIID, (void**)&htmlContainer)) {
      htmlContainer->GetInlineStyleSheet(&sheet);
      NS_RELEASE(htmlContainer);
    }
  }
  NS_ASSERTION(nsnull != sheet, "can't get inline style sheet");
  return sheet;
}

NS_IMETHODIMP
nsHTMLBodyElement::GetInlineStyleRule(nsIStyleRule*& aResult)
{
  if (nsnull == mInner.mInlineStyleRule) {
    nsIHTMLCSSStyleSheet*  sheet = nsnull;

    if (nsnull != mInner.mDocument) {  // find style sheet
      sheet = GetInlineStyleSheet(mInner.mDocument);
    }

    mInner.mInlineStyleRule = new BodyFixupRule(this, sheet);
    NS_IF_RELEASE(sheet);
    NS_IF_ADDREF(mInner.mInlineStyleRule);
  }
  NS_IF_ADDREF(mInner.mInlineStyleRule);
  aResult = mInner.mInlineStyleRule;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLBodyElement::GetStyleHintForAttributeChange(
    const nsIAtom* aAttribute,
    PRInt32 *aHint) const
{
  if ((aAttribute == nsHTMLAtoms::link) ||
      (aAttribute == nsHTMLAtoms::vlink) ||
      (aAttribute == nsHTMLAtoms::alink) ||
      (aAttribute == nsHTMLAtoms::bgcolor) ||
      (aAttribute == nsHTMLAtoms::background) ||
      (aAttribute == nsHTMLAtoms::text))
  {
    *aHint = NS_STYLE_HINT_VISUAL;
  }
  else {
    nsGenericHTMLElement::GetStyleHintForCommonAttributes(this, aAttribute, aHint);
  }

  return NS_OK;
}

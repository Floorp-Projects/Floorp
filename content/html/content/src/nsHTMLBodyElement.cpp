/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Daniel Glazman <glazman@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsIDOMHTMLBodyElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsIHTMLDocument.h"
#include "nsHTMLStyleSheet.h"
#include "nsIHTMLCSSStyleSheet.h"
#include "nsICSSStyleRule.h"
#include "nsIContentViewer.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsMappedAttributes.h"
#include "nsISupportsArray.h"
#include "nsRuleData.h"
#include "nsIFrame.h"
#include "nsIDocShell.h"
#include "nsCOMPtr.h"
#include "nsIView.h"
#include "nsLayoutAtoms.h"
#include "nsRuleWalker.h"
#include "nsIViewManager.h"

//----------------------------------------------------------------------

class nsHTMLBodyElement;

class BodyRule: public nsIStyleRule {
public:
  BodyRule(nsHTMLBodyElement* aPart, nsHTMLStyleSheet* aSheet);
  virtual ~BodyRule();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const;

  // The new mapping function.
  NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);

#ifdef DEBUG
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

  nsHTMLBodyElement*  mPart;  // not ref-counted, cleared by content 
  nsHTMLStyleSheet*  mSheet; // not ref-counted, cleared by content
};

//----------------------------------------------------------------------

class nsHTMLBodyElement : public nsGenericHTMLElement,
                          public nsIDOMHTMLBodyElement
{
public:
  nsHTMLBodyElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLBodyElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLBodyElement
  NS_DECL_NSIDOMHTMLBODYELEMENT

  virtual PRBool ParseAttribute(nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual void SetDocument(nsIDocument* aDocument, PRBool aDeep,
                           PRBool aCompileEventHandlers);
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
  NS_IMETHOD WalkContentStyleRules(nsRuleWalker* aRuleWalker);
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;

protected:
  BodyRule* mContentStyleRule;
};

//----------------------------------------------------------------------

BodyRule::BodyRule(nsHTMLBodyElement* aPart, nsHTMLStyleSheet* aSheet)
{
  mPart = aPart;
  mSheet = aSheet;
}

BodyRule::~BodyRule()
{
}

NS_IMPL_ISUPPORTS1(BodyRule, nsIStyleRule)

NS_IMETHODIMP
BodyRule::GetStyleSheet(nsIStyleSheet*& aSheet) const
{
  NS_IF_ADDREF(mSheet);
  aSheet = mSheet;
  return NS_OK;
}

NS_IMETHODIMP
BodyRule::MapRuleInfoInto(nsRuleData* aData)
{
  if (!aData || (aData->mSID != eStyleStruct_Margin) || !aData->mMarginData || !mPart)
    return NS_OK; // We only care about margins.

  nsHTMLValue value;
  
  PRInt32 bodyMarginWidth  = -1;
  PRInt32 bodyMarginHeight = -1;
  PRInt32 bodyTopMargin = -1;
  PRInt32 bodyBottomMargin = -1;
  PRInt32 bodyLeftMargin = -1;
  PRInt32 bodyRightMargin = -1;

  // check the mode (fortunately, the ruleData has a presContext for us to use!)
  NS_ASSERTION(aData->mPresContext, "null presContext in ruleNode was unexpected");
  nsCompatibility mode = aData->mPresContext->CompatibilityMode();


  if (mPart->GetAttrCount() > 0) {
    // if marginwidth/marginheight are set, reflect them as 'margin'
    mPart->GetHTMLAttribute(nsHTMLAtoms::marginwidth, value);
    if (eHTMLUnit_Integer == value.GetUnit()) {
      bodyMarginWidth = value.GetIntValue();
      if (bodyMarginWidth < 0) bodyMarginWidth = 0;
      nsCSSRect& margin = aData->mMarginData->mMargin;
      if (margin.mLeft.GetUnit() == eCSSUnit_Null)
        margin.mLeft.SetFloatValue((float)bodyMarginWidth, eCSSUnit_Pixel);
      if (margin.mRight.GetUnit() == eCSSUnit_Null)
        margin.mRight.SetFloatValue((float)bodyMarginWidth, eCSSUnit_Pixel);
    }

    mPart->GetHTMLAttribute(nsHTMLAtoms::marginheight, value);
    if (eHTMLUnit_Integer == value.GetUnit()) {
      bodyMarginHeight = value.GetIntValue();
      if (bodyMarginHeight < 0) bodyMarginHeight = 0;
      nsCSSRect& margin = aData->mMarginData->mMargin;
      if (margin.mTop.GetUnit() == eCSSUnit_Null)
        margin.mTop.SetFloatValue((float)bodyMarginHeight, eCSSUnit_Pixel);
      if (margin.mBottom.GetUnit() == eCSSUnit_Null)
        margin.mBottom.SetFloatValue((float)bodyMarginHeight, eCSSUnit_Pixel);
    }

    if (eCompatibility_NavQuirks == mode){
      // topmargin (IE-attribute)
      mPart->GetHTMLAttribute(nsHTMLAtoms::topmargin, value);
      if (eHTMLUnit_Integer == value.GetUnit()) {
        bodyTopMargin = value.GetIntValue();
        if (bodyTopMargin < 0) bodyTopMargin = 0;
        nsCSSRect& margin = aData->mMarginData->mMargin;
        if (margin.mTop.GetUnit() == eCSSUnit_Null)
          margin.mTop.SetFloatValue((float)bodyTopMargin, eCSSUnit_Pixel);
      }

      // bottommargin (IE-attribute)
      mPart->GetHTMLAttribute(nsHTMLAtoms::bottommargin, value);
      if (eHTMLUnit_Integer == value.GetUnit()) {
        bodyBottomMargin = value.GetIntValue();
        if (bodyBottomMargin < 0) bodyBottomMargin = 0;
        nsCSSRect& margin = aData->mMarginData->mMargin;
        if (margin.mBottom.GetUnit() == eCSSUnit_Null)
          margin.mBottom.SetFloatValue((float)bodyBottomMargin, eCSSUnit_Pixel);
      }

      // leftmargin (IE-attribute)
      mPart->GetHTMLAttribute(nsHTMLAtoms::leftmargin, value);
      if (eHTMLUnit_Integer == value.GetUnit()) {
        bodyLeftMargin = value.GetIntValue();
        if (bodyLeftMargin < 0) bodyLeftMargin = 0;
        nsCSSRect& margin = aData->mMarginData->mMargin;
        if (margin.mLeft.GetUnit() == eCSSUnit_Null)
          margin.mLeft.SetFloatValue((float)bodyLeftMargin, eCSSUnit_Pixel);
      }

      // rightmargin (IE-attribute)
      mPart->GetHTMLAttribute(nsHTMLAtoms::rightmargin, value);
      if (eHTMLUnit_Integer == value.GetUnit()) {
        bodyRightMargin = value.GetIntValue();
        if (bodyRightMargin < 0) bodyRightMargin = 0;
        nsCSSRect& margin = aData->mMarginData->mMargin;
        if (margin.mRight.GetUnit() == eCSSUnit_Null)
          margin.mRight.SetFloatValue((float)bodyRightMargin, eCSSUnit_Pixel);
      }
    }

  }

  // if marginwidth or marginheight is set in the <frame> and not set in the <body>
  // reflect them as margin in the <body>
  if (bodyMarginWidth == -1 || bodyMarginHeight == -1) {
    nsCOMPtr<nsISupports> container = aData->mPresContext->GetContainer();
    if (container) {
      nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(container));
      if (docShell) {
        nscoord frameMarginWidth=-1;  // default value
        nscoord frameMarginHeight=-1; // default value
        docShell->GetMarginWidth(&frameMarginWidth); // -1 indicates not set   
        docShell->GetMarginHeight(&frameMarginHeight); 
        if ((frameMarginWidth >= 0) && (bodyMarginWidth == -1)) { // set in <frame> & not in <body> 
          if (eCompatibility_NavQuirks == mode) {
            if ((bodyMarginHeight == -1) && (0 > frameMarginHeight)) // nav quirk 
              frameMarginHeight = 0;
          }
        }
        if ((frameMarginHeight >= 0) && (bodyMarginHeight == -1)) { // set in <frame> & not in <body> 
          if (eCompatibility_NavQuirks == mode) {
            if ((bodyMarginWidth == -1) && (0 > frameMarginWidth)) // nav quirk
              frameMarginWidth = 0;
          }
        }

        if ((bodyMarginWidth == -1) && (frameMarginWidth >= 0)) {
          nsCSSRect& margin = aData->mMarginData->mMargin;
          if (margin.mLeft.GetUnit() == eCSSUnit_Null)
            margin.mLeft.SetFloatValue((float)frameMarginWidth, eCSSUnit_Pixel);
          if (margin.mRight.GetUnit() == eCSSUnit_Null)
            margin.mRight.SetFloatValue((float)frameMarginWidth, eCSSUnit_Pixel);
        }

        if ((bodyMarginHeight == -1) && (frameMarginHeight >= 0)) {
          nsCSSRect& margin = aData->mMarginData->mMargin;
          if (margin.mTop.GetUnit() == eCSSUnit_Null)
            margin.mTop.SetFloatValue((float)frameMarginHeight, eCSSUnit_Pixel);
          if (margin.mBottom.GetUnit() == eCSSUnit_Null)
            margin.mBottom.SetFloatValue((float)frameMarginHeight, eCSSUnit_Pixel);
        }
      }
    }
  }
  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
BodyRule::List(FILE* out, PRInt32 aIndent) const
{
  return NS_OK;
}
#endif

//----------------------------------------------------------------------


NS_IMPL_NS_NEW_HTML_ELEMENT(Body)


nsHTMLBodyElement::nsHTMLBodyElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo),
    mContentStyleRule(nsnull)
{
}

nsHTMLBodyElement::~nsHTMLBodyElement()
{
  if (mContentStyleRule) {
    mContentStyleRule->mPart = nsnull;
    mContentStyleRule->mSheet = nsnull;
    NS_RELEASE(mContentStyleRule);
  }
}


NS_IMPL_ADDREF_INHERITED(nsHTMLBodyElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLBodyElement, nsGenericElement) 

// QueryInterface implementation for nsHTMLBodyElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLBodyElement, nsGenericHTMLElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLBodyElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLBodyElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


NS_IMPL_HTML_DOM_CLONENODE(Body)


NS_IMPL_URI_ATTR(nsHTMLBodyElement, Background, background)

#define NS_IMPL_HTMLBODY_COLOR_ATTR(attr_, func_, default_)         \
NS_IMETHODIMP                                                       \
nsHTMLBodyElement::Get##func_(nsAString& aColor)                    \
{                                                                   \
  aColor.Truncate();                                                \
  nsAutoString color;                                               \
  nscolor attrColor;                                                \
  if (NS_CONTENT_ATTR_NOT_THERE ==                                  \
      GetAttr(kNameSpaceID_None, nsHTMLAtoms::attr_, color)) {      \
                                                                    \
    nsCOMPtr<nsPresContext> presContext;                           \
    GetPresContext(this, getter_AddRefs(presContext));              \
                                                                    \
    if (presContext) {                                              \
      attrColor = presContext->Default##default_();                 \
      NS_RGBToHex(attrColor, aColor);                               \
    }                                                               \
  } else if (NS_ColorNameToRGB(color, &attrColor)) {                \
    NS_RGBToHex(attrColor, aColor);                                 \
  } else {                                                          \
    aColor.Assign(color);                                           \
  }                                                                 \
  return NS_OK;                                                     \
}                                                                   \
NS_IMETHODIMP                                                       \
nsHTMLBodyElement::Set##func_(const nsAString& aColor)              \
{                                                                   \
  return SetAttr(kNameSpaceID_None, nsHTMLAtoms::attr_, aColor,     \
                 PR_TRUE);                                          \
}

NS_IMPL_HTMLBODY_COLOR_ATTR(vlink, VLink, VisitedLinkColor)
NS_IMPL_HTMLBODY_COLOR_ATTR(alink, ALink, ActiveLinkColor)
NS_IMPL_HTMLBODY_COLOR_ATTR(link, Link, LinkColor)
// XXX Should text check the body frame's style struct for color,
// like we do for bgColor?
NS_IMPL_HTMLBODY_COLOR_ATTR(text, Text, Color)

NS_IMETHODIMP 
nsHTMLBodyElement::GetBgColor(nsAString& aBgColor)
{
  aBgColor.Truncate();

  nsAutoString attr;
  nscolor bgcolor;
  nsresult rv = GetAttr(kNameSpaceID_None, nsHTMLAtoms::bgcolor, attr);

  // If we don't have an attribute, find the actual color used for
  // (generally from the user agent style sheet) for compatibility
  if (rv == NS_CONTENT_ATTR_NOT_THERE) {
    if (mDocument) {
      // Make sure the style is up-to-date, since we need it
      mDocument->FlushPendingNotifications(Flush_Style);

      nsIFrame* frame = GetPrimaryFrameFor(this, mDocument, PR_FALSE);
    
      if (frame) {
        bgcolor = frame->GetStyleBackground()->mBackgroundColor;
        NS_RGBToHex(bgcolor, aBgColor);
      }
    }
  }
  else if (NS_ColorNameToRGB(attr, &bgcolor)) {
    // If we have a color name which we can convert to an nscolor,
    // then we should use the hex value instead of the color name.
    NS_RGBToHex(bgcolor, aBgColor);
  }
  else {
    // Otherwise, just assign whatever the attribute value is.
    aBgColor.Assign(attr);
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLBodyElement::SetBgColor(const nsAString& aBgColor)
{
  return SetAttr(kNameSpaceID_None, nsHTMLAtoms::bgcolor, aBgColor, PR_TRUE); 
}

PRBool
nsHTMLBodyElement::ParseAttribute(nsIAtom* aAttribute,
                                  const nsAString& aValue,
                                  nsAttrValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::bgcolor ||
      aAttribute == nsHTMLAtoms::text ||
      aAttribute == nsHTMLAtoms::link ||
      aAttribute == nsHTMLAtoms::alink ||
      aAttribute == nsHTMLAtoms::vlink) {
    return aResult.ParseColor(aValue, nsGenericHTMLElement::GetOwnerDocument());
  }
  if (aAttribute == nsHTMLAtoms::marginwidth ||
      aAttribute == nsHTMLAtoms::marginheight ||
      aAttribute == nsHTMLAtoms::topmargin ||
      aAttribute == nsHTMLAtoms::bottommargin ||
      aAttribute == nsHTMLAtoms::leftmargin ||
      aAttribute == nsHTMLAtoms::rightmargin) {
    return aResult.ParseIntWithBounds(aValue, 0);
  }

  return nsGenericHTMLElement::ParseAttribute(aAttribute, aValue, aResult);
}

void
nsHTMLBodyElement::SetDocument(nsIDocument* aDocument, PRBool aDeep,
                               PRBool aCompileEventHandlers)
{
  if (aDocument != mDocument && mContentStyleRule) {
    mContentStyleRule->mPart = nsnull;
    mContentStyleRule->mSheet = nsnull;

    // destroy old style rule since the sheet will probably change
    NS_RELEASE(mContentStyleRule);
  }
  nsGenericHTMLElement::SetDocument(aDocument, aDeep, aCompileEventHandlers);
}

static 
void MapAttributesIntoRule(const nsMappedAttributes* aAttributes, nsRuleData* aData)
{
  if (aData->mSID == eStyleStruct_Display) {
    // When display if first asked for, go ahead and get our colors set up.
    nsIPresShell *presShell = aData->mPresContext->GetPresShell();
    if (presShell) {
      nsCOMPtr<nsIDocument> doc;
      presShell->GetDocument(getter_AddRefs(doc));
      if (doc) {
        nsHTMLStyleSheet* styleSheet = doc->GetAttributeStyleSheet();
        if (styleSheet) {
          const nsAttrValue* value;
          nscolor color;
          value = aAttributes->GetAttr(nsHTMLAtoms::link);
          if (value && value->GetColorValue(color)) {
            styleSheet->SetLinkColor(color);
          }

          value = aAttributes->GetAttr(nsHTMLAtoms::alink);
          if (value && value->GetColorValue(color)) {
            styleSheet->SetActiveLinkColor(color);
          }

          value = aAttributes->GetAttr(nsHTMLAtoms::vlink);
          if (value && value->GetColorValue(color)) {
            styleSheet->SetVisitedLinkColor(color);
          }
        }
      }
    }
  }

  if (aData->mSID == eStyleStruct_Color) {
    if (aData->mColorData->mColor.GetUnit() == eCSSUnit_Null) {
      // color: color
      nscolor color;
      const nsAttrValue* value = aAttributes->GetAttr(nsHTMLAtoms::text);
      if (value && value->GetColorValue(color))
        aData->mColorData->mColor.SetColorValue(color);
    }
  }

  nsGenericHTMLElement::MapBackgroundAttributesInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP
nsHTMLBodyElement::GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const
{
  aMapRuleFunc = &MapAttributesIntoRule;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLBodyElement::WalkContentStyleRules(nsRuleWalker* aRuleWalker)
{
  nsGenericHTMLElement::WalkContentStyleRules(aRuleWalker);

  if (!mContentStyleRule && mDocument) {
    mContentStyleRule = new BodyRule(this, mDocument->GetAttributeStyleSheet());
    NS_IF_ADDREF(mContentStyleRule);
  }
  if (aRuleWalker && mContentStyleRule) {
    aRuleWalker->Forward(mContentStyleRule);
  }
  return NS_OK;
}

NS_IMETHODIMP_(PRBool)
nsHTMLBodyElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsHTMLAtoms::link },
    { &nsHTMLAtoms::vlink },
    { &nsHTMLAtoms::alink },
    { &nsHTMLAtoms::text },
    // These aren't mapped through attribute mapping, but they are
    // mapped through a style rule, so it is attribute dependent style.
    // XXXldb But we don't actually replace the body rule when we have
    // dynamic changes...
    { &nsHTMLAtoms::marginwidth },
    { &nsHTMLAtoms::marginheight },
    { nsnull },
  };

  static const MappedAttributeEntry* const map[] = {
    attributes,
    sCommonAttributeMap,
    sBackgroundAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map, NS_ARRAY_LENGTH(map));
}

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
 *   Daniel Glazman <glazman@netscape.com>
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
#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsIDOMHTMLBodyElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsIHTMLDocument.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIHTMLCSSStyleSheet.h"
#include "nsICSSStyleRule.h"
#include "nsIWebShell.h"
#include "nsIContentViewer.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsHTMLAttributes.h"
#include "nsIHTMLContentContainer.h"
#include "nsISupportsArray.h"
#include "nsIFrame.h"
#include "nsIDocShell.h"
#include "nsIFrameManager.h"
#include "nsCOMPtr.h"
#include "nsIStyleSet.h"
#include "nsISizeOfHandler.h"
#include "nsIView.h"
#include "nsLayoutAtoms.h"
#include "nsRuleWalker.h"
#include "nsIViewManager.h"

//----------------------------------------------------------------------

class nsHTMLBodyElement;

class BodyRule: public nsIStyleRule {
public:
  BodyRule(nsHTMLBodyElement* aPart, nsIHTMLStyleSheet* aSheet);
  virtual ~BodyRule();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const;

  // The new mapping function.
  NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);

#ifdef DEBUG
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  virtual void SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize);
#endif

  nsHTMLBodyElement*  mPart;  // not ref-counted, cleared by content 
  nsIHTMLStyleSheet*  mSheet; // not ref-counted, cleared by content
};

//----------------------------------------------------------------------

class nsHTMLBodyElement : public nsGenericHTMLContainerElement,
                          public nsIDOMHTMLBodyElement
{
public:
  nsHTMLBodyElement();
  virtual ~nsHTMLBodyElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLContainerElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLBodyElement
  NS_DECL_NSIDOMHTMLBODYELEMENT

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep,
                         PRBool aCompileEventHandlers);
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
  NS_IMETHOD WalkContentStyleRules(nsRuleWalker* aRuleWalker);
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                      nsChangeHint& aHint) const;
#ifdef DEBUG
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif

protected:
  BodyRule* mContentStyleRule;
};

//----------------------------------------------------------------------

BodyRule::BodyRule(nsHTMLBodyElement* aPart, nsIHTMLStyleSheet* aSheet)
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

  nsHTMLValue   value;
  PRInt32       attrCount;
  mPart->GetAttrCount(attrCount);
  
  PRInt32 bodyMarginWidth  = -1;
  PRInt32 bodyMarginHeight = -1;
  PRInt32 bodyTopMargin = -1;
  PRInt32 bodyLeftMargin = -1;

  // check the mode (fortunately, the ruleData has a presContext for us to use!)
  nsCompatibility mode;
  NS_ASSERTION(aData->mPresContext, "null presContext in ruleNode was unexpected");
  aData->mPresContext->GetCompatibilityMode(&mode);


  if (attrCount > 0) {
    // if marginwidth/marginheight are set, reflect them as 'margin'
    mPart->GetHTMLAttribute(nsHTMLAtoms::marginwidth, value);
    if (eHTMLUnit_Pixel == value.GetUnit()) {
      bodyMarginWidth = value.GetPixelValue();
      if (bodyMarginWidth < 0) bodyMarginWidth = 0;
      nsCSSRect* margin = aData->mMarginData->mMargin;
      if (margin->mLeft.GetUnit() == eCSSUnit_Null)
        margin->mLeft.SetFloatValue((float)bodyMarginWidth, eCSSUnit_Pixel);
      if (margin->mRight.GetUnit() == eCSSUnit_Null)
        margin->mRight.SetFloatValue((float)bodyMarginWidth, eCSSUnit_Pixel);
    }

    mPart->GetHTMLAttribute(nsHTMLAtoms::marginheight, value);
    if (eHTMLUnit_Pixel == value.GetUnit()) {
      bodyMarginHeight = value.GetPixelValue();
      if (bodyMarginHeight < 0) bodyMarginHeight = 0;
      nsCSSRect* margin = aData->mMarginData->mMargin;
      if (margin->mTop.GetUnit() == eCSSUnit_Null)
        margin->mTop.SetFloatValue((float)bodyMarginHeight, eCSSUnit_Pixel);
      if (margin->mBottom.GetUnit() == eCSSUnit_Null)
        margin->mBottom.SetFloatValue((float)bodyMarginHeight, eCSSUnit_Pixel);
    }

    if (eCompatibility_NavQuirks == mode){
      // topmargin (IE-attribute)
      mPart->GetHTMLAttribute(nsHTMLAtoms::topmargin, value);
      if (eHTMLUnit_Pixel == value.GetUnit()) {
        bodyTopMargin = value.GetPixelValue();
        if (bodyTopMargin < 0) bodyTopMargin = 0;
        nsCSSRect* margin = aData->mMarginData->mMargin;
        if (margin->mTop.GetUnit() == eCSSUnit_Null)
          margin->mTop.SetFloatValue((float)bodyTopMargin, eCSSUnit_Pixel);
      }

      // leftmargin (IE-attribute)
      mPart->GetHTMLAttribute(nsHTMLAtoms::leftmargin, value);
      if (eHTMLUnit_Pixel == value.GetUnit()) {
        bodyLeftMargin = value.GetPixelValue();
        if (bodyLeftMargin < 0) bodyLeftMargin = 0;
        nsCSSRect* margin = aData->mMarginData->mMargin;
        if (margin->mLeft.GetUnit() == eCSSUnit_Null)
          margin->mLeft.SetFloatValue((float)bodyLeftMargin, eCSSUnit_Pixel);
      }
    }

  }

  // if marginwidth or marginheight is set in the <frame> and not set in the <body>
  // reflect them as margin in the <body>
  if (bodyMarginWidth == -1 || bodyMarginHeight == -1) {
    nsCOMPtr<nsISupports> container;
    aData->mPresContext->GetContainer(getter_AddRefs(container));
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
          nsCSSRect* margin = aData->mMarginData->mMargin;
          if (margin->mLeft.GetUnit() == eCSSUnit_Null)
            margin->mLeft.SetFloatValue((float)frameMarginWidth, eCSSUnit_Pixel);
          if (margin->mRight.GetUnit() == eCSSUnit_Null)
            margin->mRight.SetFloatValue((float)frameMarginWidth, eCSSUnit_Pixel);
        }

        if ((bodyMarginHeight == -1) && (frameMarginHeight >= 0)) {
          nsCSSRect* margin = aData->mMarginData->mMargin;
          if (margin->mTop.GetUnit() == eCSSUnit_Null)
            margin->mTop.SetFloatValue((float)frameMarginHeight, eCSSUnit_Pixel);
          if (margin->mBottom.GetUnit() == eCSSUnit_Null)
            margin->mBottom.SetFloatValue((float)frameMarginHeight, eCSSUnit_Pixel);
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

/******************************************************************************
* SizeOf method:
*
*  Self (reported as BodyRule's size): 
*    1) sizeof(*this)
*
*  Contained / Aggregated data (not reported as BodyRule's size):
*    1) delegate to mSheet if it exists
*
*  Children / siblings / parents:
*    none
*    
******************************************************************************/
void BodyRule::SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize)
{
  NS_ASSERTION(aSizeOfHandler != nsnull, "SizeOf handler cannot be null");

  // first get the unique items collection
  UNIQUE_STYLE_ITEMS(uniqueItems);
  if(! uniqueItems->AddItem((void*)this)){
    return;
  }

  PRUint32 localSize=0;

  // create a tag for this instance
  nsCOMPtr<nsIAtom> tag;
  tag = do_GetAtom("BodyRule");
  // get the size of an empty instance and add to the sizeof handler
  aSize = sizeof(*this);
  aSizeOfHandler->AddSize(tag, aSize);

  if(mSheet){
    mSheet->SizeOf(aSizeOfHandler, localSize);
  }

  return;
}
#endif

//----------------------------------------------------------------------

nsresult
NS_NewHTMLBodyElement(nsIHTMLContent** aInstancePtrResult,
                      nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLBodyElement* it = new nsHTMLBodyElement();

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


nsHTMLBodyElement::nsHTMLBodyElement()
  : nsGenericHTMLContainerElement(),
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
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLBodyElement,
                                    nsGenericHTMLContainerElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLBodyElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLBodyElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


nsresult
nsHTMLBodyElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLBodyElement* it = new nsHTMLBodyElement();

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


NS_IMPL_STRING_ATTR(nsHTMLBodyElement, Background, background)


NS_IMETHODIMP
nsHTMLBodyElement::GetVLink(nsAString& aVlinkColor)
{
  nsresult rv = GetAttr(kNameSpaceID_None, nsHTMLAtoms::vlink, aVlinkColor);

  // If we don't have an attribute, find the default color from the
  // UA stylesheet.
  if (rv == NS_CONTENT_ATTR_NOT_THERE) {
    nsCOMPtr<nsIPresContext> context;
    GetPresContext(this, getter_AddRefs(context));

    if (context) {
      nscolor vlinkColor;
      context->GetDefaultVisitedLinkColor(&vlinkColor);

      nsHTMLValue value(vlinkColor);
      ColorToString(value, aVlinkColor);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLBodyElement::SetVLink(const nsAString& aVlinkColor)
{
  return SetAttr(kNameSpaceID_None, nsHTMLAtoms::vlink, aVlinkColor, PR_TRUE);
}

NS_IMETHODIMP
nsHTMLBodyElement::GetALink(nsAString& aAlinkColor)
{
  nsresult rv = GetAttr(kNameSpaceID_None, nsHTMLAtoms::alink, aAlinkColor);

  // If we don't have an attribute, find the default color from the
  // UA stylesheet.
  if (rv == NS_CONTENT_ATTR_NOT_THERE) {
    nsCOMPtr<nsIPresContext> context;
    GetPresContext(this, getter_AddRefs(context));

    if (context) {
      // XXX We don't have the backend or the UI to get ALINKs from the
      // UA stylesheet yet, so we'll piggyback to the default link color like IE.
      nscolor alinkColor;
      context->GetDefaultLinkColor(&alinkColor);

      nsHTMLValue value(alinkColor);
      ColorToString(value, aAlinkColor);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLBodyElement::SetALink(const nsAString& aAlinkColor)
{
  return SetAttr(kNameSpaceID_None, nsHTMLAtoms::alink, aAlinkColor, PR_TRUE);
}

NS_IMETHODIMP
nsHTMLBodyElement::GetLink(nsAString& aLinkColor)
{
  nsresult rv = GetAttr(kNameSpaceID_None, nsHTMLAtoms::link, aLinkColor);

  // If we don't have an attribute, find the default color from the
  // UA stylesheet.
  if (rv == NS_CONTENT_ATTR_NOT_THERE) {
    nsCOMPtr<nsIPresContext> context;
    GetPresContext(this, getter_AddRefs(context));

    if (context) {
      nscolor linkColor;
      context->GetDefaultLinkColor(&linkColor);

      nsHTMLValue value(linkColor);
      ColorToString(value, aLinkColor);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLBodyElement::SetLink(const nsAString& aLinkColor)
{
  return SetAttr(kNameSpaceID_None, nsHTMLAtoms::link, aLinkColor, PR_TRUE);
}

NS_IMETHODIMP
nsHTMLBodyElement::GetText(nsAString& aTextColor)
{
  nsresult rv = GetAttr(kNameSpaceID_None, nsHTMLAtoms::text, aTextColor);

  // If we don't have an attribute, find the default color from the
  // UA stylesheet.
  if (rv == NS_CONTENT_ATTR_NOT_THERE) {
    nsCOMPtr<nsIPresContext> context;
    GetPresContext(this, getter_AddRefs(context));

    if (context) {
      nscolor textColor;
      context->GetDefaultColor(&textColor);

      nsHTMLValue value(textColor);
      ColorToString(value, aTextColor);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLBodyElement::SetText(const nsAString& aTextColor)
{
  return SetAttr(kNameSpaceID_None, nsHTMLAtoms::text, aTextColor, PR_TRUE);
}

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
    // XXX This should just use nsGenericHTMLElement::GetPrimaryFrame()
    if (mDocument) {
      // Make sure the presentation is up-to-date
      rv = mDocument->FlushPendingNotifications();
      NS_ENSURE_SUCCESS(rv, rv);
    }

    nsCOMPtr<nsIPresContext> context;
    GetPresContext(this, getter_AddRefs(context));

    if (context) {
      nsCOMPtr<nsIPresShell> shell;
      rv = context->GetShell(getter_AddRefs(shell));
      NS_ENSURE_SUCCESS(rv, rv);
    
      nsIFrame* frame;
      rv = shell->GetPrimaryFrameFor(this, &frame);
      NS_ENSURE_SUCCESS(rv, rv);

      if (frame) {
        const nsStyleBackground* StyleBackground;
        rv = frame->GetStyleData(eStyleStruct_Background,
                                   (const nsStyleStruct*&)StyleBackground);
        NS_ENSURE_SUCCESS(rv, rv);

        bgcolor = StyleBackground->mBackgroundColor;
        ColorToString(bgcolor, aBgColor);
      }
    }
  }
  else if (NS_ColorNameToRGB(attr, &bgcolor)) {
    // If we have a color name which we can convert to an nscolor,
    // then we should use the hex value instead of the color name.
    ColorToString(bgcolor, aBgColor);
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

NS_IMETHODIMP
nsHTMLBodyElement::StringToAttribute(nsIAtom* aAttribute,
                                     const nsAString& aValue,
                                     nsHTMLValue& aResult)
{
  if ((aAttribute == nsHTMLAtoms::bgcolor) ||
      (aAttribute == nsHTMLAtoms::text) ||
      (aAttribute == nsHTMLAtoms::link) ||
      (aAttribute == nsHTMLAtoms::alink) ||
      (aAttribute == nsHTMLAtoms::vlink)) {
    if (ParseColor(aValue, mDocument, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if ((aAttribute == nsHTMLAtoms::marginwidth) ||
           (aAttribute == nsHTMLAtoms::marginheight) ||
           (aAttribute == nsHTMLAtoms::topmargin) ||
           (aAttribute == nsHTMLAtoms::leftmargin)) {
    if (ParseValue(aValue, 0, aResult, eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLBodyElement::SetDocument(nsIDocument* aDocument, PRBool aDeep,
                               PRBool aCompileEventHandlers)
{
  if (nsnull != mContentStyleRule) {
    mContentStyleRule->mPart = nsnull;
    mContentStyleRule->mSheet = nsnull;

    // destroy old style rule since the sheet will probably change
    NS_RELEASE(mContentStyleRule);
  }
  return nsGenericHTMLContainerElement::SetDocument(aDocument, aDeep,
                                                    aCompileEventHandlers);
}

static 
void MapAttributesIntoRule(const nsIHTMLMappedAttributes* aAttributes, nsRuleData* aData)
{
  if (!aAttributes || !aData)
    return;
  
  if (aData->mDisplayData && aData->mSID == eStyleStruct_Display) {
    // When display if first asked for, go ahead and get our colors set up.
    nsHTMLValue value;
    
    nsCOMPtr<nsIPresShell> presShell;
    aData->mPresContext->GetShell(getter_AddRefs(presShell));
    if (presShell) {
      nsCOMPtr<nsIDocument> doc;
      presShell->GetDocument(getter_AddRefs(doc));
      if (doc) {
        nsCOMPtr<nsIHTMLContentContainer> htmlContainer =
          do_QueryInterface(doc);
        if (htmlContainer) {
          nsCOMPtr<nsIHTMLStyleSheet> styleSheet;
          htmlContainer->GetAttributeStyleSheet(getter_AddRefs(styleSheet));
          if (styleSheet) {
            aAttributes->GetAttribute(nsHTMLAtoms::link, value);
            if ((eHTMLUnit_Color == value.GetUnit()) || 
                (eHTMLUnit_ColorName == value.GetUnit())) {
              styleSheet->SetLinkColor(value.GetColorValue());
            }

            aAttributes->GetAttribute(nsHTMLAtoms::alink, value);
            if ((eHTMLUnit_Color == value.GetUnit()) || 
                (eHTMLUnit_ColorName == value.GetUnit())) {
              styleSheet->SetActiveLinkColor(value.GetColorValue());
            }

            aAttributes->GetAttribute(nsHTMLAtoms::vlink, value);
            if ((eHTMLUnit_Color == value.GetUnit()) ||
                (eHTMLUnit_ColorName == value.GetUnit())) {
              styleSheet->SetVisitedLinkColor(value.GetColorValue());
            }
          }
        }
      }
    }
  }

  if (aData->mColorData && aData->mSID == eStyleStruct_Color) {
    if (aData->mColorData->mColor.GetUnit() == eCSSUnit_Null) {
      // color: color
      nsHTMLValue value;
      aAttributes->GetAttribute(nsHTMLAtoms::text, value);
      if (((eHTMLUnit_Color == value.GetUnit())) ||
          (eHTMLUnit_ColorName == value.GetUnit()))
        aData->mColorData->mColor.SetColorValue(value.GetColorValue());
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

static nsIHTMLStyleSheet* GetAttrStyleSheet(nsIDocument* aDocument)
{
  nsIHTMLStyleSheet* sheet = nsnull;

  if (aDocument) {
    nsCOMPtr<nsIHTMLContentContainer> container(do_QueryInterface(aDocument));

    if (container) {
      container->GetAttributeStyleSheet(&sheet);
    }
  }

  NS_ASSERTION(nsnull != sheet, "can't get attribute style sheet");
  return sheet;
}

NS_IMETHODIMP
nsHTMLBodyElement::WalkContentStyleRules(nsRuleWalker* aRuleWalker)
{
  nsGenericHTMLContainerElement::WalkContentStyleRules(aRuleWalker);

  if (!mContentStyleRule) {
    nsCOMPtr<nsIHTMLStyleSheet> sheet;

    if (mDocument) {  // find style sheet
      sheet = dont_AddRef(GetAttrStyleSheet(mDocument));
    }

    mContentStyleRule = new BodyRule(this, sheet);
    NS_IF_ADDREF(mContentStyleRule);
  }
  if (aRuleWalker && mContentStyleRule) {
    aRuleWalker->Forward(mContentStyleRule);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLBodyElement::GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                            nsChangeHint& aHint) const
{
  if ((aAttribute == nsHTMLAtoms::link) ||
      (aAttribute == nsHTMLAtoms::vlink) ||
      (aAttribute == nsHTMLAtoms::alink) ||
      (aAttribute == nsHTMLAtoms::text)) {
    aHint = NS_STYLE_HINT_VISUAL;
  }
  else if ((aAttribute == nsHTMLAtoms::marginwidth) ||
           (aAttribute == nsHTMLAtoms::marginheight)) {
    aHint = NS_STYLE_HINT_REFLOW;
  }
  else if (!GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    if (!GetBackgroundAttributesImpact(aAttribute, aHint)) {
      aHint = NS_STYLE_HINT_CONTENT;
    }
  }

  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
nsHTMLBodyElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
#endif

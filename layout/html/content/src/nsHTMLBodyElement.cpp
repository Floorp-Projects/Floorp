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
#include "nsIDOMHTMLBodyElement.h"
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
#include "nsStyleUtil.h"
#include "nsIDocument.h"
#include "nsIHTMLDocument.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIHTMLCSSStyleSheet.h"
#include "nsICSSStyleRule.h"
#include "nsIWebShell.h"
#include "nsIContentViewer.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIHTMLAttributes.h"
#include "nsIHTMLContentContainer.h"
#include "nsISupportsArray.h"
#include "nsIFrame.h"
#include "nsIDocShell.h"
#include "nsIFrameManager.h"
#include "nsCOMPtr.h"
#include "nsIStyleSet.h"
#include "nsISizeOfHandler.h"


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

  NS_IMETHOD MapFontStyleInto(nsIMutableStyleContext* aContext,
                              nsIPresContext* aPresContext);
  NS_IMETHOD MapStyleInto(nsIMutableStyleContext* aContext,
                          nsIPresContext* aPresContext);

  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  virtual void SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize);

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

  NS_IMETHOD MapFontStyleInto(nsIMutableStyleContext* aContext, 
                              nsIPresContext* aPresContext);
  NS_IMETHOD MapStyleInto(nsIMutableStyleContext* aContext, 
                          nsIPresContext* aPresContext);

  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  virtual void SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize);

  void HandleFixedBackground(nsIPresContext* aPresContext,
                             nsIPresShell *aPresShell, PRBool aIsFixed);

  nsHTMLBodyElement*    mPart;  // not ref-counted, cleared by content 
  nsIHTMLCSSStyleSheet* mSheet; // not ref-counted, cleared by content 
};

//----------------------------------------------------------------------

// special subclass of inner class to override set document
class nsBodySuper: public nsGenericHTMLContainerElement
{
public:
  nsBodySuper();
  virtual ~nsBodySuper();

  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep,
                         PRBool aCompileEventHandlers);

  BodyRule*       mContentStyleRule;
  BodyFixupRule*  mInlineStyleRule;
};


nsBodySuper::nsBodySuper() : nsGenericHTMLContainerElement(),
                             mContentStyleRule(nsnull),
                             mInlineStyleRule(nsnull)
{
}

nsBodySuper::~nsBodySuper()
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

NS_IMETHODIMP
nsBodySuper::SetDocument(nsIDocument* aDocument, PRBool aDeep,
                         PRBool aCompileEventHandlers)
{
  if (nsnull != mContentStyleRule) {
    mContentStyleRule->mPart = nsnull;
    mContentStyleRule->mSheet = nsnull;

    // destroy old style rule since the sheet will probably change
    NS_RELEASE(mContentStyleRule);
  }
  if (nsnull != mInlineStyleRule) {
    mInlineStyleRule->mPart = nsnull;
    mInlineStyleRule->mSheet = nsnull;

    // destroy old style rule since the sheet will probably change
    NS_RELEASE(mInlineStyleRule);
  }
  return nsGenericHTMLContainerElement::SetDocument(aDocument, aDeep,
                                                    aCompileEventHandlers);
}

//----------------------------------------------------------------------

class nsHTMLBodyElement : public nsBodySuper,
                          public nsIDOMHTMLBodyElement
{
public:
  nsHTMLBodyElement();
  virtual ~nsHTMLBodyElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_IDOMNODE_NO_CLONENODE(nsGenericHTMLContainerElement::)

  // nsIDOMElement
  NS_FORWARD_IDOMELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_IDOMHTMLELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLBodyElement
  NS_DECL_IDOMHTMLBODYELEMENT

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAReadableString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc, 
                                          nsMapAttributesFunc& aMapFunc) const;
  NS_IMETHOD GetContentStyleRules(nsISupportsArray* aRules);
  NS_IMETHOD GetInlineStyleRules(nsISupportsArray* aRules);
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                      PRInt32& aHint) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;

protected:

friend class BodyRule;
friend class BodyFixupRule;
};


NS_IMPL_ADDREF_INHERITED(nsHTMLBodyElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLBodyElement, nsGenericElement) 


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

NS_IMPL_ISUPPORTS(BodyRule, NS_GET_IID(nsIStyleRule));

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

// Strength is an out-of-band weighting, useful for mapping CSS !
// important always 0 here
NS_IMETHODIMP
BodyRule::GetStrength(PRInt32& aStrength) const
{
  aStrength = 0;
  return NS_OK;
}

NS_IMETHODIMP
BodyRule::MapFontStyleInto(nsIMutableStyleContext* aContext,
                           nsIPresContext* aPresContext)
{
  // set up the basefont (defaults to 3)
  nsStyleFont* font = (nsStyleFont*)aContext->GetMutableStyleData(eStyleStruct_Font);
  PRInt32 scaler;
  aPresContext->GetFontScaler(&scaler);
  float scaleFactor = nsStyleUtil::GetScalingFactor(scaler);
  // apply font scaling to the body
  font->mFont.size = NSToCoordFloor(float(font->mFont.size) * scaleFactor);
  if (font->mFont.size < 1) {
    font->mFont.size = 1;
  }
  font->mFixedFont.size = NSToCoordFloor(float(font->mFixedFont.size) * scaleFactor);
  if (font->mFixedFont.size < 1) {
    font->mFixedFont.size = 1;
  }

  return NS_OK;
}

NS_IMETHODIMP
BodyRule::MapStyleInto(nsIMutableStyleContext* aContext,
                       nsIPresContext* aPresContext)
{
  if (mPart) {
    nsStyleSpacing* styleSpacing = (nsStyleSpacing*)(aContext->GetMutableStyleData(eStyleStruct_Spacing));

    if (nsnull != styleSpacing) {
      nsHTMLValue   value;
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
          nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(container));
          if (docShell) {
            nscoord frameMarginWidth=-1;  // default value
            nscoord frameMarginHeight=-1; // default value
            docShell->GetMarginWidth(&frameMarginWidth); // -1 indicates not set   
            docShell->GetMarginHeight(&frameMarginHeight); 
            if ((frameMarginWidth >= 0) && (0 > bodyMarginWidth)) { // set in <frame> & not in <body> 
              if (eCompatibility_NavQuirks == mode) {
                if ((0 > bodyMarginHeight) && (0 > frameMarginHeight)) { // nav quirk 
                  frameMarginHeight = 0;
                }
              }
            }
            if ((frameMarginHeight >= 0) && (0 > bodyMarginHeight)) { // set in <frame> & not in <body> 
              if (eCompatibility_NavQuirks == mode) {
                if ((0 > bodyMarginWidth) && (0 > frameMarginWidth)) { // nav quirk
                  frameMarginWidth = 0;
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
  tag = getter_AddRefs(NS_NewAtom("BodyRule"));
  // get the size of an empty instance and add to the sizeof handler
  aSize = sizeof(*this);
  aSizeOfHandler->AddSize(tag, aSize);

  if(mSheet){
    mSheet->SizeOf(aSizeOfHandler, localSize);
  }

  return;
}

//----------------------------------------------------------------------


BodyFixupRule::BodyFixupRule(nsHTMLBodyElement* aPart,
                             nsIHTMLCSSStyleSheet* aSheet)
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


NS_INTERFACE_MAP_BEGIN(BodyFixupRule)
   NS_INTERFACE_MAP_ENTRY(nsIStyleRule)
   NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

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
BodyFixupRule::MapFontStyleInto(nsIMutableStyleContext* aContext,
                                nsIPresContext* aPresContext)
{
  return NS_OK;
}

NS_IMETHODIMP
BodyFixupRule::MapStyleInto(nsIMutableStyleContext* aContext,
                            nsIPresContext* aPresContext)
{
  NS_ENSURE_ARG(aContext);
  NS_ENSURE_ARG(aPresContext);

  // get the context data for the BODY, HTML element, and CANVAS
  nsCOMPtr<nsIStyleContext> parentContext;
  nsCOMPtr<nsIStyleContext> canvasContext;
  parentContext = dont_AddRef(aContext->GetParent());

  if (parentContext){
    canvasContext = getter_AddRefs(parentContext->GetParent());
  }

  if (!(parentContext && canvasContext && aContext)) {
    return NS_ERROR_FAILURE;
  }

  // get the context data for the background information
  PRBool bFixedBackground = PR_FALSE;
  nsStyleColor* canvasStyleColor;
  nsStyleColor* htmlStyleColor;
  nsStyleColor* bodyStyleColor;
  bodyStyleColor = (nsStyleColor*)aContext->GetMutableStyleData(eStyleStruct_Color);
  htmlStyleColor = (nsStyleColor*)parentContext->GetMutableStyleData(eStyleStruct_Color);
  canvasStyleColor = (nsStyleColor*)canvasContext->GetMutableStyleData(eStyleStruct_Color);
  nsStyleColor* styleColor = bodyStyleColor; // default to BODY

  NS_ASSERTION(bodyStyleColor && htmlStyleColor && canvasStyleColor, "null context data");
  if (!(bodyStyleColor && htmlStyleColor && canvasStyleColor)){
    return NS_ERROR_FAILURE;
  }

  // Use the CSS precedence rules for dealing with background: if the value
  // of the 'background' property for the HTML element is different from
  // 'transparent' then use it, else use the value of the 'background' property
  // for the BODY element

  // See if the BODY or HTML has a non-transparent background 
  // or if the HTML element was previously propagated from its child (the BODY)
  //
  // Also, check if the canvas has a non-transparent background in case it is being
  // cleared via the DOM
  if ((!bodyStyleColor->BackgroundIsTransparent()) || 
      (!htmlStyleColor->BackgroundIsTransparent()) || 
      (!canvasStyleColor->BackgroundIsTransparent()) ||
      (NS_STYLE_BG_PROPAGATED_FROM_CHILD == (htmlStyleColor->mBackgroundFlags & NS_STYLE_BG_PROPAGATED_FROM_CHILD)) ) {

    // if HTML background is not transparent then we use its background for the canvas,
    // otherwise we use the BODY's background
    if (!(htmlStyleColor->BackgroundIsTransparent())) {
      styleColor = htmlStyleColor;
    } else {
      styleColor = bodyStyleColor;
    }

    // set the canvas bg values
    canvasStyleColor->mBackgroundAttachment = styleColor->mBackgroundAttachment;
    canvasStyleColor->mBackgroundRepeat = styleColor->mBackgroundRepeat;
    canvasStyleColor->mBackgroundColor = styleColor->mBackgroundColor;
    canvasStyleColor->mBackgroundXPosition = styleColor->mBackgroundXPosition;
    canvasStyleColor->mBackgroundYPosition = styleColor->mBackgroundYPosition;
    canvasStyleColor->mBackgroundImage = styleColor->mBackgroundImage;
    canvasStyleColor->mBackgroundFlags = (styleColor->mBackgroundFlags & ~NS_STYLE_BG_PROPAGATED_TO_PARENT);
    canvasStyleColor->mBackgroundFlags |= NS_STYLE_BG_PROPAGATED_FROM_CHILD;

    bFixedBackground = 
      canvasStyleColor->mBackgroundAttachment == NS_STYLE_BG_ATTACHMENT_FIXED ? PR_TRUE : PR_FALSE;

    // reset the background values for the context that was propogated
    styleColor->mBackgroundImage.SetLength(0);
    styleColor->mBackgroundAttachment = NS_STYLE_BG_ATTACHMENT_SCROLL;
    styleColor->mBackgroundFlags = NS_STYLE_BG_COLOR_TRANSPARENT |
                                   NS_STYLE_BG_IMAGE_NONE |
                                   NS_STYLE_BG_PROPAGATED_TO_PARENT;  
    // NOTE: if this was the BODY then this flag is somewhat erroneous
    // as it was propogated to the GRANDPARENT!  We patch this next by
    // marking the HTML's background as propagated too, so we can walk
    // up the chain of contexts that have to propagation bit set (see
    // nsCSSStyleRule.cpp MapDeclarationColorInto)

    if (styleColor == bodyStyleColor) {
      htmlStyleColor->mBackgroundFlags |= NS_STYLE_BG_PROPAGATED_TO_PARENT;
    }
  }

  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));
  if (presShell) {
    nsCOMPtr<nsIDocument> doc;
    presShell->GetDocument(getter_AddRefs(doc));
    if (doc) {
      nsCOMPtr<nsIHTMLContentContainer> htmlContainer(do_QueryInterface(doc));

      if (htmlContainer) {
        nsCOMPtr<nsIHTMLStyleSheet> styleSheet;
        htmlContainer->GetAttributeStyleSheet(getter_AddRefs(styleSheet));

        if (styleSheet) {
          styleSheet->SetDocumentForegroundColor(styleColor->mColor);
          if (!(styleColor->mBackgroundFlags &
                NS_STYLE_BG_COLOR_TRANSPARENT)) {
            styleSheet->SetDocumentBackgroundColor(styleColor->mBackgroundColor);
          }
        }
      }
    }

    // take care of some special requirements for fixed backgrounds
    HandleFixedBackground(aPresContext, presShell, bFixedBackground);
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

void BodyFixupRule::HandleFixedBackground(nsIPresContext* aPresContext, 
                                          nsIPresShell *aPresShell, 
                                          PRBool aIsFixed)
{
  // we have to tell the canvas' view if we want to bitblt on scroll or not
  // - if we have a fixed background we cannot bitblt when we scroll,
  //   otherewise we can
  nsIView*      viewportView = nsnull;
  nsIFrame*     canvasFrame = nsnull;

  nsCOMPtr<nsIFrameManager> manager;
  nsresult rv = aPresShell->GetFrameManager(getter_AddRefs(manager));
  if (NS_SUCCEEDED(rv) && manager) {
    manager->GetCanvasFrame(aPresContext, &canvasFrame);
  }

  if (canvasFrame) {
    canvasFrame->GetView(aPresContext, (nsIView**)&viewportView);
  }
  if (viewportView) {
    if (aIsFixed) {
      viewportView->SetViewFlags(NS_VIEW_PUBLIC_FLAG_DONT_BITBLT);
    } else {
      viewportView->ClearViewFlags(NS_VIEW_PUBLIC_FLAG_DONT_BITBLT);
    }
  }
}

/******************************************************************************
* SizeOf method:
*
*  Self (reported as BodyFixupRule's size): 
*    1) sizeof(*this)
*
*  Contained / Aggregated data (not reported as BodyFixupRule's size):
*    1) Delegates to the mSheet if it exists
*
*  Children / siblings / parents:
*    none
*    
******************************************************************************/
void BodyFixupRule::SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize)
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
  tag = getter_AddRefs(NS_NewAtom("BodyFixupRule"));
  // get the size of an empty instance and add to the sizeof handler
  aSize = sizeof(*this);
  aSizeOfHandler->AddSize(tag, aSize);

  if(mSheet){
    mSheet->SizeOf(aSizeOfHandler, localSize);
  }

  return;
}

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
{
}

nsHTMLBodyElement::~nsHTMLBodyElement()
{
}


NS_IMPL_HTMLCONTENT_QI(nsHTMLBodyElement, nsGenericHTMLContainerElement,
                       nsIDOMHTMLBodyElement)


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


NS_IMPL_STRING_ATTR(nsHTMLBodyElement, ALink, alink)
NS_IMPL_STRING_ATTR(nsHTMLBodyElement, Background, background)
NS_IMPL_STRING_ATTR(nsHTMLBodyElement, Link, link)
NS_IMPL_STRING_ATTR(nsHTMLBodyElement, Text, text)
NS_IMPL_STRING_ATTR(nsHTMLBodyElement, VLink, vlink)


NS_IMETHODIMP 
nsHTMLBodyElement::GetBgColor(nsAWritableString& aBgColor)
{
  // If we don't have an attribute, find the actual color used for
  // (generally from the user agent style sheet) for compatibility
  if (NS_CONTENT_ATTR_NOT_THERE == nsBodySuper::GetAttribute(kNameSpaceID_None, nsHTMLAtoms::bgcolor, aBgColor)) {
    nsresult result = NS_OK;
    if (mDocument) {
      // Make sure the presentation is up-to-date
      result = mDocument->FlushPendingNotifications();
      if (NS_FAILED(result)) {
        return result;
      }
    }

    nsCOMPtr<nsIPresContext> context;
    result = GetPresContext(this, getter_AddRefs(context));
    if (NS_FAILED(result)) {
      return result;
    }
    
    nsCOMPtr<nsIPresShell> shell;
    result = context->GetShell(getter_AddRefs(shell));
    if (NS_FAILED(result)) {
      return result;
    }
    
    nsIFrame* frame;
    result = shell->GetPrimaryFrameFor(this, &frame);
    if (NS_FAILED(result)) {
      return result;
    }

    if (frame) {
      const nsStyleColor* styleColor;
      result = frame->GetStyleData(eStyleStruct_Color,
                                   (const nsStyleStruct*&)styleColor);
      if (NS_FAILED(result)) {
        return result;
      }

      nsHTMLValue value(styleColor->mBackgroundColor);
      ColorToString(value, aBgColor);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLBodyElement::SetBgColor(const nsAReadableString& aBgColor)
{
  return nsBodySuper::SetAttribute(kNameSpaceID_None, nsHTMLAtoms::bgcolor,
                                   aBgColor, PR_TRUE); 
}

NS_IMETHODIMP
nsHTMLBodyElement::StringToAttribute(nsIAtom* aAttribute,
                                     const nsAReadableString& aValue,
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
           (aAttribute == nsHTMLAtoms::marginheight)) {
    if (ParseValue(aValue, 0, aResult, eHTMLUnit_Pixel)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  return NS_CONTENT_ATTR_NOT_THERE;
}

static void
MapAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                  nsIMutableStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  if (nsnull != aAttributes) {
    nsHTMLValue value;
    nsGenericHTMLElement::MapBackgroundAttributesInto(aAttributes, aContext,
                                                      aPresContext);

    aAttributes->GetAttribute(nsHTMLAtoms::text, value);
    if ((eHTMLUnit_Color == value.GetUnit()) || 
        (eHTMLUnit_ColorName == value.GetUnit())){
      nsStyleColor* color = (nsStyleColor*)
        aContext->GetMutableStyleData(eStyleStruct_Color);
      color->mColor = value.GetColorValue();
    }

    nsCOMPtr<nsIPresShell> presShell;
    aPresContext->GetShell(getter_AddRefs(presShell));
    if (presShell) {
      nsCOMPtr<nsIDocument> doc;
      presShell->GetDocument(getter_AddRefs(doc));
      if (doc) {
        nsIHTMLContentContainer*  htmlContainer;
        if (NS_OK == doc->QueryInterface(NS_GET_IID(nsIHTMLContentContainer),
                                         (void**)&htmlContainer)) {
          nsIHTMLStyleSheet* styleSheet;
          if (NS_OK == htmlContainer->GetAttributeStyleSheet(&styleSheet)) {
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
            NS_RELEASE(styleSheet);
          }
          NS_RELEASE(htmlContainer);
        }
      }
    }

  }
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext,
                                                aPresContext);
}

NS_IMETHODIMP
nsHTMLBodyElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc, 
                                                nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
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
nsHTMLBodyElement::GetContentStyleRules(nsISupportsArray* aRules)
{
  nsBodySuper::GetContentStyleRules(aRules);

  if (!mContentStyleRule) {
    nsCOMPtr<nsIHTMLStyleSheet> sheet;

    if (mDocument) {  // find style sheet
      sheet = dont_AddRef(GetAttrStyleSheet(mDocument));
    }

    mContentStyleRule = new BodyRule(this, sheet);
    NS_IF_ADDREF(mContentStyleRule);
  }
  if (aRules && mContentStyleRule) {
    aRules->AppendElement(mContentStyleRule);
  }
  return NS_OK;
}

static nsIHTMLCSSStyleSheet* GetInlineStyleSheet(nsIDocument* aDocument)
{
  nsIHTMLCSSStyleSheet* sheet = nsnull;

  if (aDocument) {
    nsCOMPtr<nsIHTMLContentContainer> container(do_QueryInterface(aDocument));

    if (container) {
      container->GetInlineStyleSheet(&sheet);
    }
  }

  NS_ASSERTION(nsnull != sheet, "can't get attribute style sheet");
  return sheet;
}

NS_IMETHODIMP
nsHTMLBodyElement::GetInlineStyleRules(nsISupportsArray* aRules)
{
  PRBool useBodyFixupRule = PR_FALSE;

  nsGenericHTMLContainerElement::GetInlineStyleRules(aRules);

  // The BodyFixupRule only applies when we have HTML as the parent of the BODY
  // - check if this is the case, and set the flag to use the rule only if 
  //   the HTML is the parent of the BODY
  nsCOMPtr<nsIContent> parentElement;
  GetParent(*getter_AddRefs(parentElement));
  if (parentElement) {
    nsCOMPtr<nsIAtom> tag;
    parentElement->GetTag(*getter_AddRefs(tag));

    if (tag.get() == nsHTMLAtoms::html) {
      // create the fixup rule
      useBodyFixupRule = PR_TRUE;
    }
  }

  if (!mInlineStyleRule && useBodyFixupRule) {
    nsCOMPtr<nsIHTMLCSSStyleSheet> sheet;

    if (mDocument) {  // find style sheet
      sheet = dont_AddRef(GetInlineStyleSheet(mDocument));
    }

    mInlineStyleRule = new BodyFixupRule(this, sheet);
    NS_IF_ADDREF(mInlineStyleRule);
  }

  if (aRules && mInlineStyleRule) {
    aRules->AppendElement(mInlineStyleRule);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLBodyElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                            PRInt32& aHint) const
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

NS_IMETHODIMP
nsHTMLBodyElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}

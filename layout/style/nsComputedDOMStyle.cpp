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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsIComputedDOMStyle.h"
#include "nsIPresShell.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIDOMElement.h"
#include "nsIStyleContext.h"
#include "nsIScriptObjectOwner.h"

#include "nsCOMPtr.h"
#include "nsDOMError.h"


class nsComputedDOMStyle : public nsIComputedDOMStyle,
                           public nsIScriptObjectOwner
{
public:
  NS_DECL_ISUPPORTS

  // nsIComputedDOMStyle
  NS_IMETHOD Init(nsIDOMElement *aElement, const nsString& aPseudoElt,
                  nsIPresShell *aPresShell);

  // nsIDOMCSS2Properties
  NS_DECL_IDOMCSS2PROPERTIES

  // nsIDOMCSSStyleDeclaration
  NS_DECL_IDOMCSSSTYLEDECLARATION

  // nsIScriptObjectOwner
  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void* aScriptObject);

  // nsComputedDOMStyle
  nsComputedDOMStyle();
  virtual ~nsComputedDOMStyle();

private:
  nsCOMPtr<nsIPresShell> mPresShell;
  nsCOMPtr<nsIContent> mContent;
  nsString mPseudo;
  void* mScriptObject;
};

nsresult
NS_NewComputedDOMStyle(nsIComputedDOMStyle** aComputedStyle)
{
  NS_ENSURE_ARG_POINTER(aComputedStyle);

  *aComputedStyle = new nsComputedDOMStyle();
  NS_ENSURE_TRUE(*aComputedStyle, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aComputedStyle);

  return NS_OK;
}


nsComputedDOMStyle::nsComputedDOMStyle() : mScriptObject(nsnull)
{
  NS_INIT_REFCNT();
}


nsComputedDOMStyle::~nsComputedDOMStyle()
{
}


NS_IMPL_ADDREF(nsComputedDOMStyle);
NS_IMPL_RELEASE(nsComputedDOMStyle);


NS_INTERFACE_MAP_BEGIN(nsComputedDOMStyle)
   NS_INTERFACE_MAP_ENTRY(nsIComputedDOMStyle)
   NS_INTERFACE_MAP_ENTRY(nsIDOMCSS2Properties)
   NS_INTERFACE_MAP_ENTRY(nsIDOMCSSStyleDeclaration)
   NS_INTERFACE_MAP_ENTRY(nsIScriptObjectOwner)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIComputedDOMStyle)
NS_INTERFACE_MAP_END


NS_IMETHODIMP
nsComputedDOMStyle::GetScriptObject(nsIScriptContext *aContext, 
                                  void** aScriptObject)
{
  nsresult res = NS_OK;

  if (!mScriptObject) {
    nsISupports *supports = NS_STATIC_CAST(nsIComputedDOMStyle *, this);

    // XXX Should be done through factory
    res = NS_NewScriptCSS2Properties(aContext, supports, mContent,
                                     &mScriptObject);
  }

  *aScriptObject = mScriptObject;

  return res;
}


NS_IMETHODIMP
nsComputedDOMStyle::SetScriptObject(void* aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}


NS_IMETHODIMP
nsComputedDOMStyle::Init(nsIDOMElement *aElement, const nsString& aPseudoElt,
                         nsIPresShell *aPresShell)
{
  NS_ENSURE_ARG_POINTER(aElement);
  NS_ENSURE_ARG_POINTER(aPresShell);

  mPresShell = aPresShell;

  mContent = do_QueryInterface(aElement);
  if (!mContent) {
    // This should not happen, all our elements support nsIContent!
    return NS_ERROR_FAILURE;
  }

  mPseudo = aPseudoElt;

  return NS_OK;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetCssText(nsString& aCssText)
{
  aCssText.Truncate();
  return NS_OK;
}


NS_IMETHODIMP
nsComputedDOMStyle::SetCssText(const nsString& aCssText)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}


NS_IMETHODIMP
nsComputedDOMStyle::GetLength(PRUint32* aLength)
{
  NS_ENSURE_ARG_POINTER(aLength);
  *aLength = 0;
  return NS_OK;
}


NS_IMETHODIMP
nsComputedDOMStyle::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  NS_ENSURE_ARG_POINTER(aParentRule);
  *aParentRule = nsnull;

  return NS_OK;
}


NS_IMETHODIMP
nsComputedDOMStyle::GetPropertyValue(const nsString& aPropertyName,
                                     nsString& aReturn)
{
  // TBI
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}


NS_IMETHODIMP
nsComputedDOMStyle::GetPropertyCSSValue(const nsString& aPropertyName,
                                        nsIDOMCSSValue** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  return NS_OK;
}


NS_IMETHODIMP
nsComputedDOMStyle::RemoveProperty(const nsString& aPropertyName,
                                   nsString& aReturn)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}


NS_IMETHODIMP
nsComputedDOMStyle::GetPropertyPriority(const nsString& aPropertyName,
                                        nsString& aReturn)
{
  aReturn.Truncate();
  return NS_OK;
}


NS_IMETHODIMP
nsComputedDOMStyle::SetProperty(const nsString& aPropertyName,
                                const nsString& aValue,
                                const nsString& aPriority)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}


NS_IMETHODIMP
nsComputedDOMStyle::Item(PRUint32 aIndex, nsString& aReturn)
{
  aReturn.Truncate();
  return NS_OK;
}


// nsIDOMCSS2Properties

NS_IMETHODIMP 
nsComputedDOMStyle::SetAzimuth(const nsString& aAzimuth)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetBackground(const nsString& aBackground)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetBackgroundAttachment(const nsString& aBackgroundAttachment)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetBackgroundColor(const nsString& aBackgroundColor)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetBackgroundImage(const nsString& aBackgroundImage)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetBackgroundPosition(const nsString& aBackgroundPosition)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetBackgroundRepeat(const nsString& aBackgroundRepeat)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetBehavior(const nsString& aBehavior)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetBorder(const nsString& aBorder)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetBorderCollapse(const nsString& aBorderCollapse)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetBorderColor(const nsString& aBorderColor)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetBorderSpacing(const nsString& aBorderSpacing)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetBorderStyle(const nsString& aBorderStyle)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetBorderTop(const nsString& aBorderTop)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetBorderRight(const nsString& aBorderRight)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetBorderBottom(const nsString& aBorderBottom)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetBorderLeft(const nsString& aBorderLeft)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetBorderTopColor(const nsString& aBorderTopColor)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetBorderRightColor(const nsString& aBorderRightColor)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetBorderBottomColor(const nsString& aBorderBottomColor)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetBorderLeftColor(const nsString& aBorderLeftColor)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetBorderTopStyle(const nsString& aBorderTopStyle)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetBorderRightStyle(const nsString& aBorderRightStyle)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetBorderBottomStyle(const nsString& aBorderBottomStyle)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetBorderLeftStyle(const nsString& aBorderLeftStyle)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetBorderTopWidth(const nsString& aBorderTopWidth)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetBorderRightWidth(const nsString& aBorderRightWidth)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetBorderBottomWidth(const nsString& aBorderBottomWidth)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetBorderLeftWidth(const nsString& aBorderLeftWidth)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetBorderWidth(const nsString& aBorderWidth)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetBottom(const nsString& aBottom)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetCaptionSide(const nsString& aCaptionSide)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetClear(const nsString& aClear)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetClip(const nsString& aClip)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetColor(const nsString& aColor)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetContent(const nsString& aContent)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetCounterIncrement(const nsString& aCounterIncrement)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetCounterReset(const nsString& aCounterReset)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetCssFloat(const nsString& aCssFloat)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetCue(const nsString& aCue)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetCueAfter(const nsString& aCueAfter)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetCueBefore(const nsString& aCueBefore)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetCursor(const nsString& aCursor)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetDirection(const nsString& aDirection)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetDisplay(const nsString& aDisplay)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetElevation(const nsString& aElevation)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetEmptyCells(const nsString& aEmptyCells)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetFont(const nsString& aFont)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetFontFamily(const nsString& aFontFamily)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetFontSize(const nsString& aFontSize)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetFontSizeAdjust(const nsString& aFontSizeAdjust)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetFontStretch(const nsString& aFontStretch)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetFontStyle(const nsString& aFontStyle)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetFontVariant(const nsString& aFontVariant)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetFontWeight(const nsString& aFontWeight)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetHeight(const nsString& aHeight)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetLeft(const nsString& aLeft)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetLetterSpacing(const nsString& aLetterSpacing)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetLineHeight(const nsString& aLineHeight)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetListStyle(const nsString& aListStyle)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetListStyleImage(const nsString& aListStyleImage)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetListStylePosition(const nsString& aListStylePosition)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetListStyleType(const nsString& aListStyleType)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetMargin(const nsString& aMargin)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetMarginTop(const nsString& aMarginTop)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetMarginRight(const nsString& aMarginRight)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetMarginBottom(const nsString& aMarginBottom)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetMarginLeft(const nsString& aMarginLeft)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetMarkerOffset(const nsString& aMarkerOffset)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetMarks(const nsString& aMarks)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetMaxHeight(const nsString& aMaxHeight)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetMaxWidth(const nsString& aMaxWidth)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetMinHeight(const nsString& aMinHeight)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetMinWidth(const nsString& aMinWidth)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetOrphans(const nsString& aOrphans)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetOutline(const nsString& aOutline)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetOutlineColor(const nsString& aOutlineColor)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetOutlineStyle(const nsString& aOutlineStyle)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetOutlineWidth(const nsString& aOutlineWidth)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetOverflow(const nsString& aOverflow)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetPadding(const nsString& aPadding)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetPaddingTop(const nsString& aPaddingTop)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetPaddingRight(const nsString& aPaddingRight)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetPaddingBottom(const nsString& aPaddingBottom)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetPaddingLeft(const nsString& aPaddingLeft)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetPage(const nsString& aPage)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetPageBreakAfter(const nsString& aPageBreakAfter)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetPageBreakBefore(const nsString& aPageBreakBefore)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetPageBreakInside(const nsString& aPageBreakInside)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetPause(const nsString& aPause)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetPauseAfter(const nsString& aPauseAfter)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetPauseBefore(const nsString& aPauseBefore)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetPitch(const nsString& aPitch)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetPitchRange(const nsString& aPitchRange)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetPlayDuring(const nsString& aPlayDuring)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetPosition(const nsString& aPosition)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetQuotes(const nsString& aQuotes)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetRichness(const nsString& aRichness)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetRight(const nsString& aRight)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetSize(const nsString& aSize)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetSpeak(const nsString& aSpeak)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetSpeakHeader(const nsString& aSpeakHeader)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetSpeakNumeral(const nsString& aSpeakNumeral)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetSpeakPunctuation(const nsString& aSpeakPunctuation)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetSpeechRate(const nsString& aSpeechRate)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetStress(const nsString& aStress)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetTableLayout(const nsString& aTableLayout)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetTextAlign(const nsString& aTextAlign)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetTextDecoration(const nsString& aTextDecoration)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetTextIndent(const nsString& aTextIndent)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetTextShadow(const nsString& aTextShadow)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetTextTransform(const nsString& aTextTransform)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetTop(const nsString& aTop)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetUnicodeBidi(const nsString& aUnicodeBidi)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetVerticalAlign(const nsString& aVerticalAlign)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetVisibility(const nsString& aVisibility)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetVoiceFamily(const nsString& aVoiceFamily)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetVolume(const nsString& aVolume)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetWhiteSpace(const nsString& aWhiteSpace)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetWidows(const nsString& aWidows)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetWidth(const nsString& aWidth)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetWordSpacing(const nsString& aWordSpacing)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::SetZIndex(const nsString& aZIndex)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP 
nsComputedDOMStyle::SetOpacity(const nsString& aOpacity)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}


// Property getters

NS_IMETHODIMP 
nsComputedDOMStyle::GetAzimuth(nsString& aAzimuth)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBackground(nsString& aBackground)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBackgroundAttachment(nsString& aBackgroundAttachment)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBackgroundColor(nsString& aBackgroundColor)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBackgroundImage(nsString& aBackgroundImage)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBackgroundPosition(nsString& aBackgroundPosition)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBackgroundRepeat(nsString& aBackgroundRepeat)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBehavior(nsString& aBehavior)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorder(nsString& aBorder)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderCollapse(nsString& aBorderCollapse)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderColor(nsString& aBorderColor)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderSpacing(nsString& aBorderSpacing)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderStyle(nsString& aBorderStyle)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderTop(nsString& aBorderTop)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderRight(nsString& aBorderRight)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderBottom(nsString& aBorderBottom)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderLeft(nsString& aBorderLeft)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderTopColor(nsString& aBorderTopColor)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderRightColor(nsString& aBorderRightColor)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderBottomColor(nsString& aBorderBottomColor)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderLeftColor(nsString& aBorderLeftColor)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderTopStyle(nsString& aBorderTopStyle)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderRightStyle(nsString& aBorderRightStyle)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderBottomStyle(nsString& aBorderBottomStyle)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderLeftStyle(nsString& aBorderLeftStyle)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderTopWidth(nsString& aBorderTopWidth)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderRightWidth(nsString& aBorderRightWidth)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderBottomWidth(nsString& aBorderBottomWidth)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderLeftWidth(nsString& aBorderLeftWidth)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderWidth(nsString& aBorderWidth)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBottom(nsString& aBottom)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetCaptionSide(nsString& aCaptionSide)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetClear(nsString& aClear)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetClip(nsString& aClip)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetColor(nsString& aColor)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetContent(nsString& aContent)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetCounterIncrement(nsString& aCounterIncrement)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetCounterReset(nsString& aCounterReset)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetCssFloat(nsString& aCssFloat)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetCue(nsString& aCue)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetCueAfter(nsString& aCueAfter)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetCueBefore(nsString& aCueBefore)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetCursor(nsString& aCursor)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetDirection(nsString& aDirection)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetDisplay(nsString& aDisplay)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetElevation(nsString& aElevation)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetEmptyCells(nsString& aEmptyCells)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetFont(nsString& aFont)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetFontFamily(nsString& aFontFamily)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetFontSize(nsString& aFontSize)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetFontSizeAdjust(nsString& aFontSizeAdjust)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetFontStretch(nsString& aFontStretch)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetFontStyle(nsString& aFontStyle)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetFontVariant(nsString& aFontVariant)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetFontWeight(nsString& aFontWeight)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetHeight(nsString& aHeight)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetLeft(nsString& aLeft)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetLetterSpacing(nsString& aLetterSpacing)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetLineHeight(nsString& aLineHeight)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetListStyle(nsString& aListStyle)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetListStyleImage(nsString& aListStyleImage)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetListStylePosition(nsString& aListStylePosition)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetListStyleType(nsString& aListStyleType)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetMargin(nsString& aMargin)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetMarginTop(nsString& aMarginTop)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetMarginRight(nsString& aMarginRight)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetMarginBottom(nsString& aMarginBottom)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetMarginLeft(nsString& aMarginLeft)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetMarkerOffset(nsString& aMarkerOffset)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetMarks(nsString& aMarks)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetMaxHeight(nsString& aMaxHeight)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetMaxWidth(nsString& aMaxWidth)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetMinHeight(nsString& aMinHeight)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetMinWidth(nsString& aMinWidth)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetOrphans(nsString& aOrphans)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetOutline(nsString& aOutline)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetOutlineColor(nsString& aOutlineColor)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetOutlineStyle(nsString& aOutlineStyle)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetOutlineWidth(nsString& aOutlineWidth)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetOverflow(nsString& aOverflow)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPadding(nsString& aPadding)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPaddingTop(nsString& aPaddingTop)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPaddingRight(nsString& aPaddingRight)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPaddingBottom(nsString& aPaddingBottom)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPaddingLeft(nsString& aPaddingLeft)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPage(nsString& aPage)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPageBreakAfter(nsString& aPageBreakAfter)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPageBreakBefore(nsString& aPageBreakBefore)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPageBreakInside(nsString& aPageBreakInside)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPause(nsString& aPause)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPauseAfter(nsString& aPauseAfter)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPauseBefore(nsString& aPauseBefore)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPitch(nsString& aPitch)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPitchRange(nsString& aPitchRange)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPlayDuring(nsString& aPlayDuring)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPosition(nsString& aPosition)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetQuotes(nsString& aQuotes)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetRichness(nsString& aRichness)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetRight(nsString& aRight)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetSize(nsString& aSize)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetSpeak(nsString& aSpeak)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetSpeakHeader(nsString& aSpeakHeader)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetSpeakNumeral(nsString& aSpeakNumeral)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetSpeakPunctuation(nsString& aSpeakPunctuation)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetSpeechRate(nsString& aSpeechRate)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetStress(nsString& aStress)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetTableLayout(nsString& aTableLayout)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetTextAlign(nsString& aTextAlign)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetTextDecoration(nsString& aTextDecoration)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetTextIndent(nsString& aTextIndent)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetTextShadow(nsString& aTextShadow)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetTextTransform(nsString& aTextTransform)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetTop(nsString& aTop)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetUnicodeBidi(nsString& aUnicodeBidi)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetVerticalAlign(nsString& aVerticalAlign)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetVisibility(nsString& aVisibility)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetVoiceFamily(nsString& aVoiceFamily)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetVolume(nsString& aVolume)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetWhiteSpace(nsString& aWhiteSpace)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetWidows(nsString& aWidows)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetWidth(nsString& aWidth)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetWordSpacing(nsString& aWordSpacing)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetZIndex(nsString& aZIndex)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetOpacity(nsString& aOpacity)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}


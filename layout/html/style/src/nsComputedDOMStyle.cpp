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
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIDOMElement.h"
#include "nsIStyleContext.h"
#include "nsIScriptObjectOwner.h"
#include "nsROCSSPrimitiveValue.h"

#include "nsCSSProps.h"

#include "nsCOMPtr.h"
#include "nsDOMError.h"

/*
 * This is the implementation of the readonly CSSStyleDeclaration that is
 * returned by the getComputedStyle() function.
 *
 * This file is very much work in progress, there's a lot of code in this
 * file that is #ifdef'd out placeholders.
 */

class nsComputedDOMStyle : public nsIComputedDOMStyle,
                           public nsIScriptObjectOwner
{
public:
  NS_DECL_ISUPPORTS

  // nsIComputedDOMStyle
  NS_IMETHOD Init(nsIDOMElement *aElement, const nsString& aPseudoElt,
                  nsIPresShell *aPresShell);

  // nsIDOMCSSStyleDeclaration
  NS_DECL_IDOMCSSSTYLEDECLARATION

  // nsIScriptObjectOwner
  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void* aScriptObject);

  // nsComputedDOMStyle
  nsComputedDOMStyle();
  virtual ~nsComputedDOMStyle();

private:
  nsresult GetAbsoluteFrameRect(nsIFrame *aFrame, nsRect& aRect);
  nsresult GetWidth(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetHeight(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetLeft(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetTop(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetRight(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBottom(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);

  nsresult GetDisplay(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);

  nsCOMPtr<nsIPresShell> mPresShell;
  nsCOMPtr<nsIContent> mContent;
  nsString mPseudo;

  float mT2P; // For unit conversions

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


nsComputedDOMStyle::nsComputedDOMStyle() : mPresShell(nsnull),
                                           mContent(nsnull),
                                           mT2P(0.0f),
                                           mScriptObject(nsnull)
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
    res = NS_NewScriptCSSStyleDeclaration(aContext, supports, mContent,
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

  nsCOMPtr<nsIPresContext> presCtx;
  mPresShell->GetPresContext(getter_AddRefs(presCtx));
  NS_ENSURE_TRUE(presCtx, NS_ERROR_FAILURE);

  presCtx->GetTwipsToPixels(&mT2P);

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

  nsIFrame *frame = nsnull;
  mPresShell->GetPrimaryFrameFor(mContent, &frame);

  nsCOMPtr<nsIDOMCSSPrimitiveValue> val;
  nsresult rv = NS_OK;

  nsCSSProperty prop = nsCSSProps::LookupProperty(aPropertyName);

  switch (prop) {
  case eCSSProperty_width :
    rv = GetWidth(frame, *getter_AddRefs(val)); break;
  case eCSSProperty_height :
    rv = GetHeight(frame, *getter_AddRefs(val)); break;
  case eCSSProperty_left :
    rv = GetLeft(frame, *getter_AddRefs(val)); break;
  case eCSSProperty_top :
    rv = GetTop(frame, *getter_AddRefs(val)); break;
  case eCSSProperty_right :
    rv = GetRight(frame, *getter_AddRefs(val)); break;
  case eCSSProperty_bottom :
    rv = GetBottom(frame, *getter_AddRefs(val)); break;
  default :
    break;
  }

  if (val) {
    val->QueryInterface(NS_GET_IID(nsIDOMCSSValue), (void **)aReturn);
  }

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


// Property getters...

#if 0

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
#endif

NS_IMETHODIMP
nsComputedDOMStyle::GetBottom(nsIFrame *aFrame,
                              nsIDOMCSSPrimitiveValue*& aValue)
{
  nsISupports *tmp = NS_STATIC_CAST(nsIComputedDOMStyle *, this);

  nsRect rect;
  GetAbsoluteFrameRect(aFrame, rect);

  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue(tmp, mT2P);
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetTwips(rect.y + rect.height);

  return val->QueryInterface(NS_GET_IID(nsIDOMCSSPrimitiveValue),
                             (void **)&aValue);
}

#if 0
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
#endif

NS_IMETHODIMP
nsComputedDOMStyle::GetDisplay(nsIFrame *aFrame,
                               nsIDOMCSSPrimitiveValue*& aValue)
{
  nsISupports *tmp = NS_STATIC_CAST(nsIComputedDOMStyle *, this);

  nsRect rect;
  GetAbsoluteFrameRect(aFrame, rect);

  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue(tmp, mT2P);
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  const nsStyleDisplay* display;
  aFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);

  if (display) {
    switch (display->mDisplay) {
    case NS_STYLE_DISPLAY_NONE :
      val->SetString("none"); break;
    case NS_STYLE_DISPLAY_BLOCK :
      val->SetString("block"); break;
    case NS_STYLE_DISPLAY_INLINE :
      val->SetString("inline"); break;
    case NS_STYLE_DISPLAY_INLINE_BLOCK :
      val->SetString("inline_block"); break;
    case NS_STYLE_DISPLAY_LIST_ITEM :
      val->SetString("list-item"); break;
    case NS_STYLE_DISPLAY_MARKER :
      val->SetString("marker"); break;
    case NS_STYLE_DISPLAY_RUN_IN :
      val->SetString("run-in"); break;
    case NS_STYLE_DISPLAY_COMPACT :
      val->SetString("compact"); break;
    case NS_STYLE_DISPLAY_TABLE :
      val->SetString("table"); break;
    case NS_STYLE_DISPLAY_INLINE_TABLE :
      val->SetString("inline-table"); break;
    case NS_STYLE_DISPLAY_TABLE_ROW_GROUP :
      val->SetString("table-row-croup"); break;
    case NS_STYLE_DISPLAY_TABLE_COLUMN :
      val->SetString("table-column"); break;
    case NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP :
      val->SetString("table-column-group"); break;
    case NS_STYLE_DISPLAY_TABLE_HEADER_GROUP :
      val->SetString("table-header-group"); break;
    case NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP :
      val->SetString("table-footer-group"); break;
    case NS_STYLE_DISPLAY_TABLE_ROW :
      val->SetString("table-row"); break;
    case NS_STYLE_DISPLAY_TABLE_CELL :
      val->SetString("table-cell"); break;
    case NS_STYLE_DISPLAY_TABLE_CAPTION :
      val->SetString("table-caption"); break;
    case NS_STYLE_DISPLAY_MENU :
      val->SetString("menu"); break;
    default :
      val->SetString(""); break;

      break;
    }
  }

  return val->QueryInterface(NS_GET_IID(nsIDOMCSSPrimitiveValue),
                             (void **)&aValue);

  return NS_OK;
}

#if 0
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

#endif

nsresult
nsComputedDOMStyle::GetHeight(nsIFrame *aFrame,
                             nsIDOMCSSPrimitiveValue*& aValue)
{
  nsISupports *tmp = NS_STATIC_CAST(nsIComputedDOMStyle *, this);

  nsRect rect;
  GetAbsoluteFrameRect(aFrame, rect);

  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue(tmp, mT2P);
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetTwips(rect.height);

  return val->QueryInterface(NS_GET_IID(nsIDOMCSSPrimitiveValue),
                             (void **)&aValue);
}

NS_IMETHODIMP
nsComputedDOMStyle::GetLeft(nsIFrame *aFrame,
                            nsIDOMCSSPrimitiveValue*& aValue)
{
  nsISupports *tmp = NS_STATIC_CAST(nsIComputedDOMStyle *, this);

  nsRect rect;
  GetAbsoluteFrameRect(aFrame, rect);

  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue(tmp, mT2P);
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetTwips(rect.x);

  return val->QueryInterface(NS_GET_IID(nsIDOMCSSPrimitiveValue),
                             (void **)&aValue);
}

#if 0
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

nsresult GetMargins(nsIFrame *aFrame, nscoord& left, nscoord& top,
                    nscoord& right, nscoord& bottom)
{
  const nsStyleSpacing* spacing;
  nsStyleCoord coord;
  frame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct*&)spacing);
  if (spacing) {
    if (eStyleUnit_Coord == spacing->mBorder.GetLeftUnit()) {
      origin.x += spacing->mBorder.GetLeft(coord).GetCoordValue();
    }
    if (eStyleUnit_Coord == spacing->mBorder.GetTopUnit()) {
      origin.y += spacing->mBorder.GetTop(coord).GetCoordValue();
    }
  }
}

NS_IMETHODIMP
nsComputedDOMStyle::GetMarginTop(nsIFrame *aFrame,
                                 nsIDOMCSSPrimitiveValue*& aValue)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetMarginRight(nsIFrame *aFrame,
                                   nsIDOMCSSPrimitiveValue*& aValue)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetMarginBottom(nsIFrame *aFrame,
                                    nsIDOMCSSPrimitiveValue*& aValue)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetMarginLeft(nsIFrame *aFrame,
                                  nsIDOMCSSPrimitiveValue*& aValue)
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
#endif

NS_IMETHODIMP
nsComputedDOMStyle::GetRight(nsIFrame *aFrame,
                             nsIDOMCSSPrimitiveValue*& aValue)
{
  nsISupports *tmp = NS_STATIC_CAST(nsIComputedDOMStyle *, this);

  nsRect rect;
  GetAbsoluteFrameRect(aFrame, rect);

  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue(tmp, mT2P);
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetTwips(rect.x + rect.width);

  return val->QueryInterface(NS_GET_IID(nsIDOMCSSPrimitiveValue),
                             (void **)&aValue);
}

#if 0
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
#endif

NS_IMETHODIMP
nsComputedDOMStyle::GetTop(nsIFrame *aFrame,
                           nsIDOMCSSPrimitiveValue*& aValue)
{
  nsISupports *tmp = NS_STATIC_CAST(nsIComputedDOMStyle *, this);

  nsRect rect;
  GetAbsoluteFrameRect(aFrame, rect);

  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue(tmp, mT2P);
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetTwips(rect.y);

  return val->QueryInterface(NS_GET_IID(nsIDOMCSSPrimitiveValue),
                             (void **)&aValue);
}

#if 0
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
#endif

nsresult
nsComputedDOMStyle::GetAbsoluteFrameRect(nsIFrame *aFrame, nsRect& aRect)
{
  nsresult res = NS_OK;

  aRect.x = aRect.y = 0;
  aRect.Empty();
 
  // Flush all pending notifications so that our frames are uptodate
  mPresShell->FlushPendingNotifications();

  // Get it's origin
  nsPoint origin;
  aFrame->GetOrigin(origin);

  // Get the union of all rectangles in this and continuation frames
  nsIFrame* next = aFrame;
  do {
    nsRect rect;
    next->GetRect(rect);
    aRect.UnionRect(aRect, rect);
    next->GetNextInFlow(&next);
  } while (nsnull != next);
        
  nsIFrame* parent;
  aFrame->GetParent(&parent);
  while (parent) {
    // Add the parent's origin to our own to get to the
    // right coordinate system
    nsPoint parentOrigin;
    parent->GetOrigin(parentOrigin);
    origin += parentOrigin;

    parent->GetParent(&parent);
  }

  // For the origin, add in the border for the frame
  const nsStyleSpacing* spacing;
  nsStyleCoord coord;
  aFrame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct*&)spacing);
  if (spacing) {
    if (eStyleUnit_Coord == spacing->mBorder.GetLeftUnit()) {
      origin.x += spacing->mBorder.GetLeft(coord).GetCoordValue();
      aRect.width -= spacing->mBorder.GetLeft(coord).GetCoordValue();
      //aRect.width -= spacing->mMargin.GetLeft(coord).GetCoordValue();
      aRect.width -= spacing->mPadding.GetLeft(coord).GetCoordValue();
    }
    if (eStyleUnit_Coord == spacing->mBorder.GetTopUnit()) {
      origin.y += spacing->mBorder.GetTop(coord).GetCoordValue();
      aRect.height -= spacing->mBorder.GetTop(coord).GetCoordValue();
      //aRect.height -= spacing->mMargin.GetTop(coord).GetCoordValue();
      aRect.height -= spacing->mPadding.GetTop(coord).GetCoordValue();
    }
    if (eStyleUnit_Coord == spacing->mBorder.GetRightUnit()) {
      aRect.width -= spacing->mBorder.GetRight(coord).GetCoordValue();
      //aRect.width -= spacing->mMargin.GetRight(coord).GetCoordValue();
      aRect.width -= spacing->mPadding.GetRight(coord).GetCoordValue();
    }
    if (eStyleUnit_Coord == spacing->mBorder.GetBottomUnit()) {
      aRect.height -= spacing->mBorder.GetBottom(coord).GetCoordValue();
      //aRect.height -= spacing->mMargin.GetBottom(coord).GetCoordValue();
      aRect.height -= spacing->mPadding.GetBottom(coord).GetCoordValue();
    }
  }

  aRect.x = origin.x;
  aRect.y = origin.y;

  return res;
}     


nsresult
nsComputedDOMStyle::GetWidth(nsIFrame *aFrame,
                             nsIDOMCSSPrimitiveValue*& aValue)
{
  nsISupports *tmp = NS_STATIC_CAST(nsIComputedDOMStyle *, this);

  nsRect rect;
  GetAbsoluteFrameRect(aFrame, rect);

  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue(tmp, mT2P);
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  val->SetTwips(rect.width);

  return val->QueryInterface(NS_GET_IID(nsIDOMCSSPrimitiveValue),
                             (void **)&aValue);
}

#if 0

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

#endif

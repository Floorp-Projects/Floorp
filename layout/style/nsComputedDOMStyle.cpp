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
#include "nsWeakReference.h"
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
  NS_IMETHOD Init(nsIDOMElement *aElement, const nsAReadableString& aPseudoElt,
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
  //Helpers
  nsresult GetAbsoluteFrameRect(nsIFrame *aFrame, nsRect& aRect);
  nsresult GetStyleData(nsStyleStructID aID, const nsStyleStruct*& aStyleStruct,nsIFrame* aFrame=0);   
  nsresult GetPaddingWidthFor(PRUint8 aSide, nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBorderStyleFor(PRUint8 aSide, nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBorderWidthFor(PRUint8 aSide, nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);

  // Properties
  nsresult GetWidth(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetHeight(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetLeft(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetTop(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetRight(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBottom(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);

  //Font properties
  nsresult GetColor(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetFontFamily(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetFontStyle(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetFontSize(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetFontWeight(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetFontVariant(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);

  // Background properties
  nsresult GetBackgroundColor(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBackgroundImage(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetDisplay(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);

  // Padding properties
  nsresult GetPadding(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetPaddingTop(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetPaddingBottom(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetPaddingLeft(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetPaddingRight(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);

  // BorderProperties
  nsresult GetBorderStyle(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBorderWidth(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBorderCollapse(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBorderSpacing(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBorderTopStyle(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBorderBottomStyle(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBorderLeftStyle(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBorderRightStyle(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBorderTopWidth(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBorderBottomWidth(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBorderLeftWidth(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  nsresult GetBorderRightWidth(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);
  
  // z-index
  nsresult GetZIndex(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);

  nsresult GetBehavior(nsIFrame *aFrame, nsIDOMCSSPrimitiveValue*& aValue);

  nsROCSSPrimitiveValue* GetROCSSPrimitiveValue();

  nsWeakPtr mPresShellWeak; // XXX could this be high cost?
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


nsComputedDOMStyle::nsComputedDOMStyle() : mPresShellWeak(nsnull),
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
nsComputedDOMStyle::Init(nsIDOMElement *aElement, const nsAReadableString& aPseudoElt,
                         nsIPresShell *aPresShell)
{
  NS_ENSURE_ARG_POINTER(aElement);
  NS_ENSURE_ARG_POINTER(aPresShell);

  mPresShellWeak = getter_AddRefs(NS_GetWeakReference(aPresShell)); 

  mContent = do_QueryInterface(aElement);
  if (!mContent) {
    // This should not happen, all our elements support nsIContent!
    return NS_ERROR_FAILURE;
  }

  mPseudo = aPseudoElt;
  
  nsCOMPtr<nsIPresShell> presShell=do_QueryReferent(mPresShellWeak);
  
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);
  
  nsCOMPtr<nsIPresContext> presCtx;

  presShell->GetPresContext(getter_AddRefs(presCtx));
  
  NS_ENSURE_TRUE(presCtx, NS_ERROR_FAILURE);

  presCtx->GetTwipsToPixels(&mT2P);

  return NS_OK;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetCssText(nsAWritableString& aCssText)
{
  aCssText.Truncate();
  return NS_OK;
}


NS_IMETHODIMP
nsComputedDOMStyle::SetCssText(const nsAReadableString& aCssText)
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
nsComputedDOMStyle::GetPropertyValue(const nsAReadableString& aPropertyName,
                                     nsAWritableString& aReturn)
{
  nsCOMPtr<nsIDOMCSSValue> val;

  aReturn.Truncate();

  nsresult rv = GetPropertyCSSValue(aPropertyName, getter_AddRefs(val));
  NS_ENSURE_SUCCESS(rv, rv);

  if (val) {
    rv = val->GetCssText(aReturn);
  }

  return rv;
}


NS_IMETHODIMP
nsComputedDOMStyle::GetPropertyCSSValue(const nsAReadableString& aPropertyName,
                                        nsIDOMCSSValue** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsCOMPtr<nsIPresShell> presShell=do_QueryReferent(mPresShellWeak);
  
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);
  
  nsIFrame *frame = nsnull;
  presShell->GetPrimaryFrameFor(mContent, &frame);

  nsCOMPtr<nsIDOMCSSPrimitiveValue> val;
  nsresult rv = NS_OK;

  // XXX FIX THIS!!!
  nsCSSProperty prop = nsCSSProps::LookupProperty(aPropertyName);

  switch (prop) {
  case eCSSProperty_behavior :
    rv = GetBehavior(frame, *getter_AddRefs(val)); break;
  case eCSSProperty_display :
    rv = GetDisplay(frame, *getter_AddRefs(val)); break;
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
  case eCSSProperty_color :
    rv = GetColor(frame, *getter_AddRefs(val)); break;
  case eCSSProperty_font_family :
    rv = GetFontFamily(frame, *getter_AddRefs(val)); break;
  case eCSSProperty_font_size :
    rv = GetFontSize(frame, *getter_AddRefs(val)); break;
  case eCSSProperty_font_style :
    rv = GetFontStyle(frame, *getter_AddRefs(val)); break;
  case eCSSProperty_font_weight :
    rv = GetFontWeight(frame, *getter_AddRefs(val)); break;
  case eCSSProperty_font_variant :
    rv = GetFontVariant(frame, *getter_AddRefs(val)); break;
  case eCSSProperty_background_color :
    rv = GetBackgroundColor(frame, *getter_AddRefs(val)); break;
  case eCSSProperty_background_image :
    rv = GetBackgroundImage(frame, *getter_AddRefs(val)); break;
  case eCSSProperty_padding :
    rv = GetPadding(frame, *getter_AddRefs(val)); break;
  case eCSSProperty_padding_top :
    rv = GetPaddingTop(frame, *getter_AddRefs(val)); break;
  case eCSSProperty_padding_bottom :
    rv = GetPaddingBottom(frame, *getter_AddRefs(val)); break;
  case eCSSProperty_padding_left :
    rv = GetPaddingLeft(frame, *getter_AddRefs(val)); break;
  case eCSSProperty_padding_right :
    rv = GetPaddingRight(frame, *getter_AddRefs(val)); break;
  case eCSSProperty_border_collapse :
    rv = GetBorderCollapse(frame, *getter_AddRefs(val)); break;
  case eCSSProperty_border_top_style :
    rv = GetBorderTopStyle(frame, *getter_AddRefs(val)); break;
  case eCSSProperty_border_bottom_style :
    rv = GetBorderBottomStyle(frame, *getter_AddRefs(val)); break;
  case eCSSProperty_border_left_style :
    rv = GetBorderLeftStyle(frame, *getter_AddRefs(val)); break;
  case eCSSProperty_border_right_style :
    rv = GetBorderRightStyle(frame, *getter_AddRefs(val)); break;
  case eCSSProperty_border_top_width :
    rv = GetBorderTopWidth(frame, *getter_AddRefs(val)); break;
  case eCSSProperty_border_bottom_width :
    rv = GetBorderBottomWidth(frame, *getter_AddRefs(val)); break;
  case eCSSProperty_border_left_width :
    rv = GetBorderLeftWidth(frame, *getter_AddRefs(val)); break;
  case eCSSProperty_border_right_width :
    rv = GetBorderRightWidth(frame, *getter_AddRefs(val)); break;
  case eCSSProperty_z_index:
    rv = GetZIndex(frame, *getter_AddRefs(val)); break;
  default :
    break;
  }

  if (val) {
    rv = val->QueryInterface(NS_GET_IID(nsIDOMCSSValue), (void **)aReturn);
  }

  return rv;
}


NS_IMETHODIMP
nsComputedDOMStyle::RemoveProperty(const nsAReadableString& aPropertyName,
                                   nsAWritableString& aReturn)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}


NS_IMETHODIMP
nsComputedDOMStyle::GetPropertyPriority(const nsAReadableString& aPropertyName,
                                        nsAWritableString& aReturn)
{
  aReturn.Truncate();
  return NS_OK;
}


NS_IMETHODIMP
nsComputedDOMStyle::SetProperty(const nsAReadableString& aPropertyName,
                                const nsAReadableString& aValue,
                                const nsAReadableString& aPriority)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}


NS_IMETHODIMP
nsComputedDOMStyle::Item(PRUint32 aIndex, nsAWritableString& aReturn)
{
  aReturn.Truncate();
  return NS_OK;
}


// Property getters...

#if 0

NS_IMETHODIMP 
nsComputedDOMStyle::GetAzimuth(nsAWritableString& aAzimuth)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBackground(nsAWritableString& aBackground)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBackgroundAttachment(nsAWritableString& aBackgroundAttachment)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBackgroundColor(nsAWritableString& aBackgroundColor)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBackgroundImage(nsAWritableString& aBackgroundImage)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBackgroundPosition(nsAWritableString& aBackgroundPosition)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBackgroundRepeat(nsAWritableString& aBackgroundRepeat)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

#endif

nsresult
nsComputedDOMStyle::GetBehavior(nsIFrame *aFrame,
                                nsIDOMCSSPrimitiveValue*& aValue)
{
  nsISupports *tmp = NS_STATIC_CAST(nsIComputedDOMStyle *, this);

  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue(tmp, mT2P);
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);

  nsStyleUserInterface uiStyle;
  const nsStyleUserInterface* ui = nsnull;

  if (aFrame) {
    aFrame->GetStyleData(eStyleStruct_UserInterface, (const nsStyleStruct*&)ui);
  } else {
      
    nsCOMPtr<nsIPresShell> presShell=do_QueryReferent(mPresShellWeak);
  
    NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);
    
    nsCOMPtr<nsIPresContext> presCtx;
    presShell->GetPresContext(getter_AddRefs(presCtx));
    NS_ENSURE_TRUE(presCtx, NS_ERROR_FAILURE);

    nsCOMPtr<nsIStyleContext> styleCtx;
    presCtx->ResolveStyleContextFor(mContent, nsnull, PR_FALSE,
                                    getter_AddRefs(styleCtx));
    NS_ENSURE_TRUE(styleCtx, NS_ERROR_FAILURE);

    styleCtx->GetStyle(eStyleStruct_UserInterface, uiStyle);
    ui = &uiStyle;
  }

  if (ui) {
    val->SetString(ui->mBehavior);
  } else {
    val->SetString("");
  }

  return val->QueryInterface(NS_GET_IID(nsIDOMCSSPrimitiveValue),
                             (void **)&aValue);
}

#if 0

NS_IMETHODIMP
nsComputedDOMStyle::GetBorder(nsAWritableString& aBorder)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderCollapse(nsAWritableString& aBorderCollapse)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderColor(nsAWritableString& aBorderColor)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderSpacing(nsAWritableString& aBorderSpacing)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderStyle(nsAWritableString& aBorderStyle)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderTop(nsAWritableString& aBorderTop)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderRight(nsAWritableString& aBorderRight)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderBottom(nsAWritableString& aBorderBottom)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderLeft(nsAWritableString& aBorderLeft)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderTopColor(nsAWritableString& aBorderTopColor)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderRightColor(nsAWritableString& aBorderRightColor)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderBottomColor(nsAWritableString& aBorderBottomColor)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderLeftColor(nsAWritableString& aBorderLeftColor)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderTopStyle(nsAWritableString& aBorderTopStyle)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderRightStyle(nsAWritableString& aBorderRightStyle)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderBottomStyle(nsAWritableString& aBorderBottomStyle)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderLeftStyle(nsAWritableString& aBorderLeftStyle)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderTopWidth(nsAWritableString& aBorderTopWidth)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderRightWidth(nsAWritableString& aBorderRightWidth)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderBottomWidth(nsAWritableString& aBorderBottomWidth)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderLeftWidth(nsAWritableString& aBorderLeftWidth)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetBorderWidth(nsAWritableString& aBorderWidth)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}
#endif

nsresult
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

nsresult
nsComputedDOMStyle::GetColor(nsIFrame *aFrame,
                             nsIDOMCSSPrimitiveValue*& aValue)
{
  nsresult result=NS_OK;
  
  if(aFrame) {
    const nsStyleColor* color_info=nsnull;
    GetStyleData(eStyleStruct_Color,(const nsStyleStruct*&)color_info,aFrame);
    if(color_info) {
      nsROCSSPrimitiveValue* val=GetROCSSPrimitiveValue();
      if(val) {
        result=val->QueryInterface(NS_GET_IID(nsIDOMCSSPrimitiveValue),(void **)&aValue);
      }
      else {
        result=NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }
  return result;
}

nsresult
nsComputedDOMStyle::GetFontFamily(nsIFrame *aFrame,
                             nsIDOMCSSPrimitiveValue*& aValue)
{
  nsresult result=NS_OK;
  
  if(aFrame) {
    const nsStyleFont* font=nsnull;
    GetStyleData(eStyleStruct_Font,(const nsStyleStruct*&)font,aFrame);
    if(font) {
      nsROCSSPrimitiveValue* val=GetROCSSPrimitiveValue();
      if(val) {
        val->SetString(font->mFont.name);
        result=val->QueryInterface(NS_GET_IID(nsIDOMCSSPrimitiveValue),(void **)&aValue);
      }
      else {
        result=NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }
  return result;
}

nsresult
nsComputedDOMStyle::GetFontSize(nsIFrame *aFrame,
                             nsIDOMCSSPrimitiveValue*& aValue)
{
  nsresult result=NS_OK;
  
  if(aFrame) {
    const nsStyleFont* font=nsnull;
    GetStyleData(eStyleStruct_Font,(const nsStyleStruct*&)font,aFrame);
    if(font) {
      nsROCSSPrimitiveValue* val=GetROCSSPrimitiveValue();
      if(val) {
        val->SetTwips(font->mFont.size);
        result=val->QueryInterface(NS_GET_IID(nsIDOMCSSPrimitiveValue),(void **)&aValue);
      }
      else {
        result=NS_ERROR_OUT_OF_MEMORY;;
      }
    }
  }
  return result;
}

nsresult
nsComputedDOMStyle::GetFontStyle(nsIFrame *aFrame,
                                 nsIDOMCSSPrimitiveValue*& aValue)
{
  nsresult result=NS_OK;
  
  if(aFrame) {
    const nsStyleFont* font=nsnull;
    GetStyleData(eStyleStruct_Font,(const nsStyleStruct*&)font,aFrame);
    if(font) {
      nsROCSSPrimitiveValue* val=GetROCSSPrimitiveValue();
      if(val) {
        const nsCString& style=nsCSSProps::SearchKeywordTable(font->mFont.style,nsCSSProps::kFontStyleKTable);
        val->SetString(style);
        result=val->QueryInterface(NS_GET_IID(nsIDOMCSSPrimitiveValue),(void **)&aValue);
      }
      else {
        result=NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }
  return result;
}

nsresult
nsComputedDOMStyle::GetFontWeight(nsIFrame *aFrame,
                             nsIDOMCSSPrimitiveValue*& aValue)
{
  nsresult result=NS_OK;
  
  if(aFrame) {
    const nsStyleFont* font=nsnull;
    GetStyleData(eStyleStruct_Font,(const nsStyleStruct*&)font,aFrame);
    if(font) {
      nsROCSSPrimitiveValue* val=GetROCSSPrimitiveValue();
      if(val) {
        const nsCString& str_weight=nsCSSProps::SearchKeywordTable(font->mFont.weight,nsCSSProps::kFontWeightKTable);
        if(str_weight.Length()>0) {
          val->SetString(str_weight);
        }
        else {
          nsAutoString num_weight;
          num_weight.AppendInt(font->mFont.weight, 10);
          val->SetString(num_weight);
        }
        result=val->QueryInterface(NS_GET_IID(nsIDOMCSSPrimitiveValue),(void **)&aValue);
      }
      else {
        result=NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }
  return result;
}

nsresult
nsComputedDOMStyle::GetFontVariant(nsIFrame *aFrame,
                             nsIDOMCSSPrimitiveValue*& aValue)
{
  nsresult result=NS_OK;
  
  if(aFrame) {
    const nsStyleFont* font=nsnull;
    GetStyleData(eStyleStruct_Font,(const nsStyleStruct*&)font,aFrame);
    if(font) {
      nsROCSSPrimitiveValue* val=GetROCSSPrimitiveValue();
      if(val) {
        const nsCString& variant=nsCSSProps::SearchKeywordTable(font->mFont.variant,nsCSSProps::kFontVariantKTable);
        val->SetString(variant);
        result=val->QueryInterface(NS_GET_IID(nsIDOMCSSPrimitiveValue),(void **)&aValue);
      }
      else {
        result=NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }
  return result;
}

nsresult
nsComputedDOMStyle::GetBackgroundColor(nsIFrame *aFrame,
                             nsIDOMCSSPrimitiveValue*& aValue)
{
  nsresult result=NS_OK;
  
  if(aFrame) {
    const nsStyleColor* color=nsnull;
    GetStyleData(eStyleStruct_Color,(const nsStyleStruct*&)color,aFrame);
    if(color) {
      nsROCSSPrimitiveValue* val=GetROCSSPrimitiveValue();
      if(val) {
        result=val->QueryInterface(NS_GET_IID(nsIDOMCSSPrimitiveValue),(void **)&aValue);
      }
      else {
        result=NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }
  return result;
}

nsresult
nsComputedDOMStyle::GetBackgroundImage(nsIFrame *aFrame,
                             nsIDOMCSSPrimitiveValue*& aValue)
{
  nsresult result=NS_OK;
  
  if(aFrame) {
    const nsStyleColor* color=nsnull;
    GetStyleData(eStyleStruct_Color,(const nsStyleStruct*&)color,aFrame);
    if(color) {
      nsROCSSPrimitiveValue* val=GetROCSSPrimitiveValue();
      if(val) {
        val->SetString(color->mBackgroundImage);
        result=val->QueryInterface(NS_GET_IID(nsIDOMCSSPrimitiveValue),(void **)&aValue);
      }
      else {
        result=NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }
  return result;
}

nsresult
nsComputedDOMStyle::GetPadding(nsIFrame *aFrame,
                               nsIDOMCSSPrimitiveValue*& aValue)
{
  nsresult result=NS_OK;
  
  nsROCSSPrimitiveValue* val=GetROCSSPrimitiveValue();
  if(val) {
    val->SetString("");// Do nothing
    result=val->QueryInterface(NS_GET_IID(nsIDOMCSSPrimitiveValue),(void **)&aValue);
  }
  return result;
}

nsresult
nsComputedDOMStyle::GetPaddingTop(nsIFrame *aFrame,
                                  nsIDOMCSSPrimitiveValue*& aValue)
{
  return GetPaddingWidthFor(NS_SIDE_TOP,aFrame,aValue);
}

nsresult
nsComputedDOMStyle::GetPaddingBottom(nsIFrame *aFrame,
                                     nsIDOMCSSPrimitiveValue*& aValue)
{
  return GetPaddingWidthFor(NS_SIDE_BOTTOM,aFrame,aValue);
}

nsresult
nsComputedDOMStyle::GetPaddingLeft(nsIFrame *aFrame,
                               nsIDOMCSSPrimitiveValue*& aValue)
{
  return GetPaddingWidthFor(NS_SIDE_LEFT,aFrame,aValue);
}

nsresult
nsComputedDOMStyle::GetPaddingRight(nsIFrame *aFrame,
                               nsIDOMCSSPrimitiveValue*& aValue)
{
  return GetPaddingWidthFor(NS_SIDE_RIGHT,aFrame,aValue);
}

nsresult
nsComputedDOMStyle::GetBorderCollapse(nsIFrame *aFrame, 
                                      nsIDOMCSSPrimitiveValue*& aValue)
{
  nsresult result=NS_OK;
    
  if(aFrame) {
   const nsStyleTable* table;
   GetStyleData(eStyleStruct_Table,(const nsStyleStruct*&)table,aFrame);
    if(table) {
      nsROCSSPrimitiveValue* val=GetROCSSPrimitiveValue();
      if(val) {
        const nsCString& ident=nsCSSProps::SearchKeywordTable(table->mBorderCollapse,nsCSSProps::kBorderCollapseKTable);
        val->SetString(ident);
        result=val->QueryInterface(NS_GET_IID(nsIDOMCSSPrimitiveValue),(void **)&aValue);
      }
      else {
        result=NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }
  return result;
}

nsresult 
nsComputedDOMStyle::GetBorderSpacing(nsIFrame *aFrame, 
                                     nsIDOMCSSPrimitiveValue*& aValue)
{
  nsresult result=NS_OK;
  return result;
}

nsresult 
nsComputedDOMStyle::GetBorderStyle(nsIFrame *aFrame, 
                                   nsIDOMCSSPrimitiveValue*& aValue)
{
  nsresult result=NS_OK;
  
  nsROCSSPrimitiveValue* val=GetROCSSPrimitiveValue();
  if(val) {
    val->SetString("");// Do nothing
    result=val->QueryInterface(NS_GET_IID(nsIDOMCSSPrimitiveValue),(void **)&aValue);
  }
  return result;
}

nsresult 
nsComputedDOMStyle::GetBorderTopStyle(nsIFrame *aFrame, 
                                      nsIDOMCSSPrimitiveValue*& aValue)
{
  return GetBorderStyleFor(NS_SIDE_TOP,aFrame,aValue);
}

nsresult 
nsComputedDOMStyle::GetBorderBottomStyle(nsIFrame *aFrame, 
                                         nsIDOMCSSPrimitiveValue*& aValue)
{
  return GetBorderStyleFor(NS_SIDE_BOTTOM,aFrame,aValue);
}
nsresult 
nsComputedDOMStyle::GetBorderLeftStyle(nsIFrame *aFrame, 
                                       nsIDOMCSSPrimitiveValue*& aValue)
{
  return GetBorderStyleFor(NS_SIDE_LEFT,aFrame,aValue);
}

nsresult 
nsComputedDOMStyle::GetBorderRightStyle(nsIFrame *aFrame, 
                                        nsIDOMCSSPrimitiveValue*& aValue)
{
  return GetBorderStyleFor(NS_SIDE_RIGHT,aFrame,aValue);
}

nsresult 
nsComputedDOMStyle::GetBorderWidth(nsIFrame *aFrame, 
                                      nsIDOMCSSPrimitiveValue*& aValue)
{  
  nsresult result=NS_OK;
  
  nsROCSSPrimitiveValue* val=GetROCSSPrimitiveValue();
  if(val) {
    val->SetString(""); // Do nothing
    result=val->QueryInterface(NS_GET_IID(nsIDOMCSSPrimitiveValue),(void **)&aValue);
  }
  
  return result;
}

nsresult 
nsComputedDOMStyle::GetBorderTopWidth(nsIFrame *aFrame, 
                                      nsIDOMCSSPrimitiveValue*& aValue)
{
  return GetBorderWidthFor(NS_SIDE_TOP,aFrame,aValue);
}

nsresult 
nsComputedDOMStyle::GetBorderBottomWidth(nsIFrame *aFrame, 
                                         nsIDOMCSSPrimitiveValue*& aValue)
{
  return GetBorderWidthFor(NS_SIDE_BOTTOM,aFrame,aValue);
}

nsresult 
nsComputedDOMStyle::GetBorderLeftWidth(nsIFrame *aFrame, 
                                       nsIDOMCSSPrimitiveValue*& aValue)
{
   return GetBorderWidthFor(NS_SIDE_LEFT,aFrame,aValue);
}

nsresult 
nsComputedDOMStyle::GetBorderRightWidth(nsIFrame *aFrame, 
                                        nsIDOMCSSPrimitiveValue*& aValue)
{
   return GetBorderWidthFor(NS_SIDE_RIGHT,aFrame,aValue);
}

nsresult 
nsComputedDOMStyle::GetZIndex(nsIFrame *aFrame, 
                              nsIDOMCSSPrimitiveValue*& aValue)
{
  nsresult result=NS_OK;
    
  if(aFrame) {
   const nsStylePosition* position;
   GetStyleData(eStyleStruct_Position,(const nsStyleStruct*&)position,aFrame);
    if(position) {
      nsROCSSPrimitiveValue* val=GetROCSSPrimitiveValue();
      if(val) {
        nsAutoString zindex;
        do {
          GetStyleData(eStyleStruct_Position,(const nsStyleStruct*&)position,aFrame);
          if(position){
            if(position->mZIndex.GetUnit()==eStyleUnit_Integer) {
              zindex.AppendInt(position->mZIndex.GetIntValue(), 10);
              break;
            }
            aFrame->GetParent(&aFrame);
          }
        }while(aFrame && position);
        
        val->SetString(zindex);
        result=val->QueryInterface(NS_GET_IID(nsIDOMCSSPrimitiveValue),(void **)&aValue);
      }
      else {
        result=NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }
  return result;
}

#if 0
NS_IMETHODIMP
nsComputedDOMStyle::GetCaptionSide(nsAWritableString& aCaptionSide)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetClear(nsAWritableString& aClear)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetClip(nsAWritableString& aClip)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetColor(nsAWritableString& aColor)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetContent(nsAWritableString& aContent)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetCounterIncrement(nsAWritableString& aCounterIncrement)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetCounterReset(nsAWritableString& aCounterReset)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetCssFloat(nsAWritableString& aCssFloat)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetCue(nsAWritableString& aCue)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetCueAfter(nsAWritableString& aCueAfter)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetCueBefore(nsAWritableString& aCueBefore)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetCursor(nsAWritableString& aCursor)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetDirection(nsAWritableString& aDirection)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}
#endif

nsresult
nsComputedDOMStyle::GetDisplay(nsIFrame *aFrame,
                               nsIDOMCSSPrimitiveValue*& aValue)
{
  nsISupports *tmp = NS_STATIC_CAST(nsIComputedDOMStyle *, this);

  nsROCSSPrimitiveValue *val = new nsROCSSPrimitiveValue(tmp, mT2P);
  NS_ENSURE_TRUE(val, NS_ERROR_OUT_OF_MEMORY);


  const nsStyleDisplay* display = nsnull;

  if (aFrame)
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
nsComputedDOMStyle::GetElevation(nsAWritableString& aElevation)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetEmptyCells(nsAWritableString& aEmptyCells)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetFont(nsAWritableString& aFont)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetFontFamily(nsAWritableString& aFontFamily)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetFontSize(nsAWritableString& aFontSize)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetFontSizeAdjust(nsAWritableString& aFontSizeAdjust)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetFontStretch(nsAWritableString& aFontStretch)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetFontStyle(nsAWritableString& aFontStyle)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetFontVariant(nsAWritableString& aFontVariant)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetFontWeight(nsAWritableString& aFontWeight)
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

nsresult
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
nsComputedDOMStyle::GetLetterSpacing(nsAWritableString& aLetterSpacing)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetLineHeight(nsAWritableString& aLineHeight)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetListStyle(nsAWritableString& aListStyle)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetListStyleImage(nsAWritableString& aListStyleImage)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetListStylePosition(nsAWritableString& aListStylePosition)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetListStyleType(nsAWritableString& aListStyleType)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetMargin(nsAWritableString& aMargin)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

nsresult GetMargins(nsIFrame *aFrame, nscoord& left, nscoord& top,
                    nscoord& right, nscoord& bottom)
{
  const nsStyleBorder* border;
  nsStyleCoord coord;
  frame->GetStyleData(eStyleStruct_Border, (const nsStyleStruct*&)border);
  if (border) {
    if (eStyleUnit_Coord == border->mBorder.GetLeftUnit()) {
      origin.x += border->mBorder.GetLeft(coord).GetCoordValue();
    }
    if (eStyleUnit_Coord == border->mBorder.GetTopUnit()) {
      origin.y += border->mBorder.GetTop(coord).GetCoordValue();
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
nsComputedDOMStyle::GetMarkerOffset(nsAWritableString& aMarkerOffset)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetMarks(nsAWritableString& aMarks)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetMaxHeight(nsAWritableString& aMaxHeight)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetMaxWidth(nsAWritableString& aMaxWidth)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetMinHeight(nsAWritableString& aMinHeight)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetMinWidth(nsAWritableString& aMinWidth)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetOrphans(nsAWritableString& aOrphans)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetOutline(nsAWritableString& aOutline)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetOutlineColor(nsAWritableString& aOutlineColor)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetOutlineStyle(nsAWritableString& aOutlineStyle)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetOutlineWidth(nsAWritableString& aOutlineWidth)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetOverflow(nsAWritableString& aOverflow)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPadding(nsAWritableString& aPadding)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPaddingTop(nsAWritableString& aPaddingTop)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPaddingRight(nsAWritableString& aPaddingRight)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPaddingBottom(nsAWritableString& aPaddingBottom)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPaddingLeft(nsAWritableString& aPaddingLeft)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPage(nsAWritableString& aPage)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPageBreakAfter(nsAWritableString& aPageBreakAfter)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPageBreakBefore(nsAWritableString& aPageBreakBefore)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPageBreakInside(nsAWritableString& aPageBreakInside)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPause(nsAWritableString& aPause)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPauseAfter(nsAWritableString& aPauseAfter)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPauseBefore(nsAWritableString& aPauseBefore)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPitch(nsAWritableString& aPitch)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPitchRange(nsAWritableString& aPitchRange)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPlayDuring(nsAWritableString& aPlayDuring)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetPosition(nsAWritableString& aPosition)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetQuotes(nsAWritableString& aQuotes)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetRichness(nsAWritableString& aRichness)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}
#endif

nsresult
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
nsComputedDOMStyle::GetSize(nsAWritableString& aSize)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetSpeak(nsAWritableString& aSpeak)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetSpeakHeader(nsAWritableString& aSpeakHeader)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetSpeakNumeral(nsAWritableString& aSpeakNumeral)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetSpeakPunctuation(nsAWritableString& aSpeakPunctuation)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetSpeechRate(nsAWritableString& aSpeechRate)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetStress(nsAWritableString& aStress)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetTableLayout(nsAWritableString& aTableLayout)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetTextAlign(nsAWritableString& aTextAlign)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetTextDecoration(nsAWritableString& aTextDecoration)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetTextIndent(nsAWritableString& aTextIndent)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetTextShadow(nsAWritableString& aTextShadow)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetTextTransform(nsAWritableString& aTextTransform)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}
#endif

nsresult
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
nsComputedDOMStyle::GetUnicodeBidi(nsAWritableString& aUnicodeBidi)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetVerticalAlign(nsAWritableString& aVerticalAlign)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetVisibility(nsAWritableString& aVisibility)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetVoiceFamily(nsAWritableString& aVoiceFamily)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetVolume(nsAWritableString& aVolume)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetWhiteSpace(nsAWritableString& aWhiteSpace)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetWidows(nsAWritableString& aWidows)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}
#endif

nsROCSSPrimitiveValue*
nsComputedDOMStyle::GetROCSSPrimitiveValue() {
  nsISupports *tmp = NS_STATIC_CAST(nsIComputedDOMStyle *, this);

  nsROCSSPrimitiveValue *primitiveValue = new nsROCSSPrimitiveValue(tmp, mT2P);
  
  NS_ASSERTION(primitiveValue!=0, "ran out of memory");

  return primitiveValue;
}

nsresult
nsComputedDOMStyle::GetAbsoluteFrameRect(nsIFrame *aFrame, nsRect& aRect)
{
  nsresult res = NS_OK;

  aRect.x = aRect.y = 0;
  aRect.Empty();
 
  if (!aFrame) {
    return NS_OK;
  }

  // Flush all pending notifications so that our frames are uptodate
  nsCOMPtr<nsIPresShell> presShell=do_QueryReferent(mPresShellWeak);
  
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);
  
  presShell->FlushPendingNotifications();

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
  const nsStyleBorder* border;
  const nsStylePadding* padding;
  nsStyleCoord coord;
  aFrame->GetStyleData(eStyleStruct_Border, (const nsStyleStruct*&)border);
  aFrame->GetStyleData(eStyleStruct_Padding, (const nsStyleStruct*&)padding);
  if (border && padding) {
    if (eStyleUnit_Coord == border->mBorder.GetLeftUnit()) {
      origin.x += border->mBorder.GetLeft(coord).GetCoordValue();
      aRect.width -= border->mBorder.GetLeft(coord).GetCoordValue();
      //aRect.width -= margin->mMargin.GetLeft(coord).GetCoordValue();
      aRect.width -= padding->mPadding.GetLeft(coord).GetCoordValue();
    }
    if (eStyleUnit_Coord == border->mBorder.GetTopUnit()) {
      origin.y += border->mBorder.GetTop(coord).GetCoordValue();
      aRect.height -= border->mBorder.GetTop(coord).GetCoordValue();
      //aRect.height -= margin->mMargin.GetTop(coord).GetCoordValue();
      aRect.height -= padding->mPadding.GetTop(coord).GetCoordValue();
    }
    if (eStyleUnit_Coord == border->mBorder.GetRightUnit()) {
      aRect.width -= border->mBorder.GetRight(coord).GetCoordValue();
      //aRect.width -= margin->mMargin.GetRight(coord).GetCoordValue();
      aRect.width -= padding->mPadding.GetRight(coord).GetCoordValue();
    }
    if (eStyleUnit_Coord == border->mBorder.GetBottomUnit()) {
      aRect.height -= border->mBorder.GetBottom(coord).GetCoordValue();
      //aRect.height -= margin->mMargin.GetBottom(coord).GetCoordValue();
      aRect.height -= padding->mPadding.GetBottom(coord).GetCoordValue();
    }
  }

  aRect.x = origin.x;
  aRect.y = origin.y;

  return res;
}  
   
nsresult
nsComputedDOMStyle::GetStyleData(nsStyleStructID aID, const nsStyleStruct*& aStyleStruct,nsIFrame* aFrame) {
  nsresult result=NS_OK;
  if(aFrame) {
    aFrame->GetStyleData(aID,aStyleStruct);
  }
  else {
    nsCOMPtr<nsIPresShell> presShell=do_QueryReferent(mPresShellWeak);
  
    NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);
    
    nsCOMPtr<nsIPresContext> pctx;
    presShell->GetPresContext(getter_AddRefs(pctx));
    if(pctx) {
      nsCOMPtr<nsIStyleContext> sctx;
      pctx->ResolveStyleContextFor(mContent, nsnull, PR_FALSE,getter_AddRefs(sctx));
      if(sctx) {
        aStyleStruct=sctx->GetStyleData(aID);
      }
    }
  }
  return result;
}

nsresult 
nsComputedDOMStyle::GetPaddingWidthFor(PRUint8 aSide,
                                       nsIFrame *aFrame,
                                       nsIDOMCSSPrimitiveValue*& aValue) 
{
  nsresult result=NS_OK;
  
  if(aFrame) {
    const nsStylePadding* padding=nsnull;
    GetStyleData(eStyleStruct_Padding,(const nsStyleStruct*&)padding,aFrame);
    if(padding) {
       nsROCSSPrimitiveValue* val=GetROCSSPrimitiveValue();
       if(val) {
         nsStyleCoord coord;
         switch(aSide) {
           case NS_SIDE_TOP:
             padding->mPadding.GetTop(coord); break;
           case NS_SIDE_BOTTOM : 
             padding->mPadding.GetBottom(coord); break;
           case NS_SIDE_LEFT :
             padding->mPadding.GetLeft(coord); break;
           case NS_SIDE_RIGHT :
             padding->mPadding.GetRight(coord); break;
           default:
             break;
         }    
         
         if(coord.GetUnit()==eStyleUnit_Percent){
           nsIFrame* parent=nsnull;
           aFrame->GetParent(&parent);
           if(parent) {
             nsRect rect;
             parent->GetRect(rect);
             val->SetTwips(nscoord(coord.GetPercentValue() * rect.width));
           }
         }
         else {
           val->SetTwips(coord.GetCoordValue());
         }
         
         result=val->QueryInterface(NS_GET_IID(nsIDOMCSSPrimitiveValue),(void **)&aValue);
       }
       else {
         result=NS_ERROR_OUT_OF_MEMORY;
       }
    }
  }
  return result;
}

nsresult 
nsComputedDOMStyle::GetBorderWidthFor(PRUint8 aSide,
                                      nsIFrame *aFrame, 
                                      nsIDOMCSSPrimitiveValue*& aValue)
{
  nsresult result=NS_OK;
    
  if(aFrame) {
   const nsStyleBorder* border;
   GetStyleData(eStyleStruct_Border,(const nsStyleStruct*&)border,aFrame);
    if(border) {
      nsROCSSPrimitiveValue* val=GetROCSSPrimitiveValue();
      if(val) {
        nsStyleCoord coord;
        switch(aSide) {
          case NS_SIDE_TOP:
            border->mBorder.GetTop(coord); break;
          case NS_SIDE_BOTTOM : 
            border->mBorder.GetBottom(coord); break;
          case NS_SIDE_LEFT :
            border->mBorder.GetLeft(coord); break;
          case NS_SIDE_RIGHT :
            border->mBorder.GetRight(coord); break;
          default:
            break;
        }

        if(coord.GetUnit()==eStyleUnit_Coord) {
          val->SetTwips(coord.GetCoordValue());
        }
        else if(coord.GetUnit()==eStyleUnit_Integer) {
          const nsCString& width=nsCSSProps::SearchKeywordTable(coord.GetIntValue(),nsCSSProps::kBorderWidthKTable);
          if(width.Length()>0) {
            val->SetString(width);
          }
        }
        result=val->QueryInterface(NS_GET_IID(nsIDOMCSSPrimitiveValue),(void **)&aValue);
      }
      else {
        result=NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }
  return result;
}

nsresult 
nsComputedDOMStyle::GetBorderStyleFor(PRUint8 aSide,
                                      nsIFrame *aFrame, 
                                      nsIDOMCSSPrimitiveValue*& aValue)
{
  nsresult result=NS_OK;
    
  if(aFrame) {
   const nsStyleBorder* border;
   GetStyleData(eStyleStruct_Border,(const nsStyleStruct*&)border,aFrame);
    if(border) {
      nsROCSSPrimitiveValue* val=GetROCSSPrimitiveValue();
      if(val) {
        const nsCString& style=nsCSSProps::SearchKeywordTable(border->GetBorderStyle(aSide),nsCSSProps::kBorderStyleKTable);
        val->SetString(style);
        result=val->QueryInterface(NS_GET_IID(nsIDOMCSSPrimitiveValue),(void **)&aValue);
      }
      else {
         result=NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }
  return result;
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
nsComputedDOMStyle::GetWordSpacing(nsAWritableString& aWordSpacing)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetZIndex(nsAWritableString& aZIndex)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

NS_IMETHODIMP
nsComputedDOMStyle::GetOpacity(nsAWritableString& aOpacity)
{
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

#endif

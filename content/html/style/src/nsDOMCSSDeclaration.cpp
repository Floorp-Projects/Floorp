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

#include "nsDOMCSSDeclaration.h"
#include "nsICSSStyleRule.h"
#include "nsICSSParser.h"
#include "nsIStyleRule.h"
#include "nsICSSDeclaration.h"
#include "nsCSSProps.h"
#include "nsIURL.h"

nsDOMCSSDeclaration::nsDOMCSSDeclaration()
{
  NS_INIT_REFCNT();
  mScriptObject = nsnull;
}

nsDOMCSSDeclaration::~nsDOMCSSDeclaration()
{
}

NS_IMPL_ADDREF(nsDOMCSSDeclaration);
NS_IMPL_RELEASE(nsDOMCSSDeclaration);

static NS_DEFINE_IID(kIDOMCSS2PropertiesIID, NS_IDOMCSS2PROPERTIES_IID);
static NS_DEFINE_IID(kIDOMCSSStyleDeclarationIID, NS_IDOMCSSSTYLEDECLARATION_IID);
static NS_DEFINE_IID(kICSSStyleRuleIID, NS_ICSS_STYLE_RULE_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

NS_IMETHODIMP
nsDOMCSSDeclaration::QueryInterface(REFNSIID aIID,
                                      void** aInstancePtr)
{
  NS_PRECONDITION(nsnull != aInstancePtr, "null ptr");
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIDOMCSS2PropertiesIID)) {
    nsIDOMCSS2Properties *tmp = this;
    AddRef();
    *aInstancePtr = (void*) tmp;
    return NS_OK;
  }
  if (aIID.Equals(kIDOMCSSStyleDeclarationIID)) {
    nsIDOMCSSStyleDeclaration *tmp = this;
    AddRef();
    *aInstancePtr = (void*) tmp;
    return NS_OK;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    nsIScriptObjectOwner *tmp = this;
    AddRef();
    *aInstancePtr = (void*) tmp;
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsIDOMCSSStyleDeclaration *tmp = this;
    nsISupports *tmp2 = tmp;
    AddRef();
    *aInstancePtr = (void*) tmp2;
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetScriptObject(nsIScriptContext* aContext,
                                       void** aScriptObject)
{
  nsresult res = NS_OK;

  if (nsnull == mScriptObject) {
    nsISupports *parent = nsnull;

    res = GetParent(&parent);
    if (NS_OK == res) {
      nsISupports *supports = (nsISupports *)(nsIDOMCSS2Properties *)this;
      // XXX Should be done through factory
      res = NS_NewScriptCSS2Properties(aContext, 
                                       supports,
                                       parent, 
                                       (void**)&mScriptObject);
      NS_RELEASE(parent);
    }
  }
  *aScriptObject = mScriptObject;

  return res;
}

NS_IMETHODIMP 
nsDOMCSSDeclaration::SetScriptObject(void* aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

NS_IMETHODIMP    
nsDOMCSSDeclaration::GetCssText(nsString& aCssText)
{
  // XXX TBI
  return NS_OK;
}

NS_IMETHODIMP    
nsDOMCSSDeclaration::SetCssText(const nsString& aCssText)
{
  // XXX TBI
  return NS_OK;
}

NS_IMETHODIMP    
nsDOMCSSDeclaration::GetLength(PRUint32* aLength)
{
  nsICSSDeclaration *decl;
  nsresult result = GetCSSDeclaration(&decl, PR_FALSE);
 
  *aLength = 0;
  if ((NS_OK == result) && (nsnull != decl)) {
    result = decl->Count(aLength);
    NS_RELEASE(decl);
  }

  return result;
}

NS_IMETHODIMP    
nsDOMCSSDeclaration::Item(PRUint32 aIndex, nsString& aReturn)
{
  nsICSSDeclaration *decl;
  nsresult result = GetCSSDeclaration(&decl, PR_FALSE);

  aReturn.SetLength(0);
  if ((NS_OK == result) && (nsnull != decl)) {
    result = decl->GetNthProperty(aIndex, aReturn);
    NS_RELEASE(decl);
  }

  return result;
}

NS_IMETHODIMP    
nsDOMCSSDeclaration::GetPropertyValue(const nsString& aPropertyName, 
                                     nsString& aReturn)
{
  nsCSSValue val;
  nsICSSDeclaration *decl;
  nsresult result = GetCSSDeclaration(&decl, PR_FALSE);

  aReturn.SetLength(0);
  if ((NS_OK == result) && (nsnull != decl)) {
    result = decl->GetValue(aPropertyName, aReturn);
    NS_RELEASE(decl);
  }

  return result;
}

NS_IMETHODIMP    
nsDOMCSSDeclaration::GetPropertyPriority(const nsString& aPropertyName, 
                                        nsString& aReturn)
{
  nsICSSDeclaration *decl;
  nsresult result = GetCSSDeclaration(&decl, PR_FALSE);
  PRBool isImportant = PR_FALSE;

  if ((NS_OK == result) && (nsnull != decl)) {
    result = decl->GetValueIsImportant(aPropertyName, isImportant);
    NS_RELEASE(decl);
  }

  if ((NS_OK == result) && isImportant) {
    aReturn.SetString("!important");    
  }
  else {
    aReturn.SetLength(0);
  }

  return result;
}

NS_IMETHODIMP    
nsDOMCSSDeclaration::SetProperty(const nsString& aPropertyName, 
                                 const nsString& aValue, 
                                 const nsString& aPriority)
{
  nsAutoString declString;
  nsICSSDeclaration *decl;
  nsresult result = GetCSSDeclaration(&decl, PR_TRUE);

  if ((NS_OK == result) && (nsnull != decl)) {
    declString=aPropertyName;
    declString.Append(":");
    declString.Append(aValue);
    declString.Append(aPriority);

    nsICSSParser* css;
    result = NS_NewCSSParser(&css);
    if (NS_OK == result) {
      PRInt32 hint;
      nsIURL* baseURL = nsnull;
      GetBaseURL(&baseURL);
      result = css->ParseAndAppendDeclaration(declString, baseURL, decl, &hint);
      NS_IF_RELEASE(baseURL);
      if (NS_OK == result) {
        result = StylePropertyChanged(aPropertyName, hint);
      }
      NS_RELEASE(css);
    }
    NS_RELEASE(decl);
  }

  return result;
}

NS_IMETHODIMP 
nsDOMCSSDeclaration::GetAzimuth(nsString& aAzimuth)
{
  return GetPropertyValue("azimuth", aAzimuth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetAzimuth(const nsString& aAzimuth)
{
  return SetProperty("azimuth", aAzimuth, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBackground(nsString& aBackground)
{
  return GetPropertyValue("background", aBackground);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBackground(const nsString& aBackground)
{
  return SetProperty("background", aBackground, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBackgroundAttachment(nsString& aBackgroundAttachment)
{
  return GetPropertyValue("background-attachment", aBackgroundAttachment);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBackgroundAttachment(const nsString& aBackgroundAttachment)
{
  return SetProperty("background-attachment", aBackgroundAttachment, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBackgroundColor(nsString& aBackgroundColor)
{
  return GetPropertyValue("background-color", aBackgroundColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBackgroundColor(const nsString& aBackgroundColor)
{
  return SetProperty("background-color", aBackgroundColor, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBackgroundImage(nsString& aBackgroundImage)
{
  return GetPropertyValue("background-image", aBackgroundImage);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBackgroundImage(const nsString& aBackgroundImage)
{
  return SetProperty("background-image", aBackgroundImage, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBackgroundPosition(nsString& aBackgroundPosition)
{
  return GetPropertyValue("background-position", aBackgroundPosition);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBackgroundPosition(const nsString& aBackgroundPosition)
{
  return SetProperty("background-position", aBackgroundPosition, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBackgroundRepeat(nsString& aBackgroundRepeat)
{
  return GetPropertyValue("background-repeat", aBackgroundRepeat);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBackgroundRepeat(const nsString& aBackgroundRepeat)
{
  return SetProperty("background-repeat", aBackgroundRepeat, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorder(nsString& aBorder)
{
  return GetPropertyValue("border", aBorder);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorder(const nsString& aBorder)
{
  return SetProperty("border", aBorder, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderCollapse(nsString& aBorderCollapse)
{
  return GetPropertyValue("border-collapse", aBorderCollapse);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderCollapse(const nsString& aBorderCollapse)
{
  return SetProperty("border-collapse", aBorderCollapse, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderColor(nsString& aBorderColor)
{
  return GetPropertyValue("border-color", aBorderColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderColor(const nsString& aBorderColor)
{
  return SetProperty("border-color", aBorderColor, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderSpacing(nsString& aBorderSpacing)
{
  return GetPropertyValue("border-spacing", aBorderSpacing);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderSpacing(const nsString& aBorderSpacing)
{
  return SetProperty("border-spacing", aBorderSpacing, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderStyle(nsString& aBorderStyle)
{
  return GetPropertyValue("border-style", aBorderStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderStyle(const nsString& aBorderStyle)
{
  return SetProperty("border-style", aBorderStyle, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderTop(nsString& aBorderTop)
{
  return GetPropertyValue("border-top", aBorderTop);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderTop(const nsString& aBorderTop)
{
  return SetProperty("border-top", aBorderTop, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderRight(nsString& aBorderRight)
{
  return GetPropertyValue("border-right", aBorderRight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderRight(const nsString& aBorderRight)
{
  return SetProperty("border-right", aBorderRight, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderBottom(nsString& aBorderBottom)
{
  return GetPropertyValue("border-bottom", aBorderBottom);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderBottom(const nsString& aBorderBottom)
{
  return SetProperty("border-bottom", aBorderBottom, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderLeft(nsString& aBorderLeft)
{
  return GetPropertyValue("border-left", aBorderLeft);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderLeft(const nsString& aBorderLeft)
{
  return SetProperty("border-left", aBorderLeft, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderTopColor(nsString& aBorderTopColor)
{
  return GetPropertyValue("border-top-color", aBorderTopColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderTopColor(const nsString& aBorderTopColor)
{
  return SetProperty("border-top-color", aBorderTopColor, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderRightColor(nsString& aBorderRightColor)
{
  return GetPropertyValue("border-right-color", aBorderRightColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderRightColor(const nsString& aBorderRightColor)
{
  return SetProperty("border-right-color", aBorderRightColor, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderBottomColor(nsString& aBorderBottomColor)
{
  return GetPropertyValue("border-bottom-color", aBorderBottomColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderBottomColor(const nsString& aBorderBottomColor)
{
  return SetProperty("border-bottom-color", aBorderBottomColor, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderLeftColor(nsString& aBorderLeftColor)
{
  return GetPropertyValue("border-left-color", aBorderLeftColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderLeftColor(const nsString& aBorderLeftColor)
{
  return SetProperty("border-left-color", aBorderLeftColor, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderTopStyle(nsString& aBorderTopStyle)
{
  return GetPropertyValue("border-top-style", aBorderTopStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderTopStyle(const nsString& aBorderTopStyle)
{
  return SetProperty("border-top-style", aBorderTopStyle, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderRightStyle(nsString& aBorderRightStyle)
{
  return GetPropertyValue("border-right-style", aBorderRightStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderRightStyle(const nsString& aBorderRightStyle)
{
  return SetProperty("border-right-style", aBorderRightStyle, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderBottomStyle(nsString& aBorderBottomStyle)
{
  return GetPropertyValue("border-bottom-style", aBorderBottomStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderBottomStyle(const nsString& aBorderBottomStyle)
{
  return SetProperty("border-bottom-style", aBorderBottomStyle, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderLeftStyle(nsString& aBorderLeftStyle)
{
  return GetPropertyValue("border-left-style", aBorderLeftStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderLeftStyle(const nsString& aBorderLeftStyle)
{
  return SetProperty("border-left-style", aBorderLeftStyle, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderTopWidth(nsString& aBorderTopWidth)
{
  return GetPropertyValue("border-top-width", aBorderTopWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderTopWidth(const nsString& aBorderTopWidth)
{
  return SetProperty("border-top-width", aBorderTopWidth, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderRightWidth(nsString& aBorderRightWidth)
{
  return GetPropertyValue("border-right-width", aBorderRightWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderRightWidth(const nsString& aBorderRightWidth)
{
  return SetProperty("border-right-width", aBorderRightWidth, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderBottomWidth(nsString& aBorderBottomWidth)
{
  return GetPropertyValue("border-bottom-width", aBorderBottomWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderBottomWidth(const nsString& aBorderBottomWidth)
{
  return SetProperty("border-bottom-width", aBorderBottomWidth, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderLeftWidth(nsString& aBorderLeftWidth)
{
  return GetPropertyValue("border-left-width", aBorderLeftWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderLeftWidth(const nsString& aBorderLeftWidth)
{
  return SetProperty("border-left-width", aBorderLeftWidth, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderWidth(nsString& aBorderWidth)
{
  return GetPropertyValue("border-width", aBorderWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderWidth(const nsString& aBorderWidth)
{
  return SetProperty("border-width", aBorderWidth, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBottom(nsString& aBottom)
{
  return GetPropertyValue("bottom", aBottom);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBottom(const nsString& aBottom)
{
  return SetProperty("bottom", aBottom, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCaptionSide(nsString& aCaptionSide)
{
  return GetPropertyValue("caption-side", aCaptionSide);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCaptionSide(const nsString& aCaptionSide)
{
  return SetProperty("caption-side", aCaptionSide, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetClear(nsString& aClear)
{
  return GetPropertyValue("clear", aClear);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetClear(const nsString& aClear)
{
  return SetProperty("clear", aClear, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetClip(nsString& aClip)
{
  return GetPropertyValue("clip", aClip);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetClip(const nsString& aClip)
{
  return SetProperty("clip", aClip, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetColor(nsString& aColor)
{
  return GetPropertyValue("color", aColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetColor(const nsString& aColor)
{
  return SetProperty("color", aColor, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetContent(nsString& aContent)
{
  return GetPropertyValue("content", aContent);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetContent(const nsString& aContent)
{
  return SetProperty("content", aContent, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCounterIncrement(nsString& aCounterIncrement)
{
  return GetPropertyValue("counter-increment", aCounterIncrement);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCounterIncrement(const nsString& aCounterIncrement)
{
  return SetProperty("counter-increment", aCounterIncrement, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCounterReset(nsString& aCounterReset)
{
  return GetPropertyValue("counter-reset", aCounterReset);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCounterReset(const nsString& aCounterReset)
{
  return SetProperty("counter-reset", aCounterReset, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCssFloat(nsString& aCssFloat)
{
  return GetPropertyValue("float", aCssFloat);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCssFloat(const nsString& aCssFloat)
{
  return SetProperty("float", aCssFloat, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCue(nsString& aCue)
{
  return GetPropertyValue("cue", aCue);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCue(const nsString& aCue)
{
  return SetProperty("cue", aCue, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCueAfter(nsString& aCueAfter)
{
  return GetPropertyValue("cue-after", aCueAfter);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCueAfter(const nsString& aCueAfter)
{
  return SetProperty("cue-after", aCueAfter, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCueBefore(nsString& aCueBefore)
{
  return GetPropertyValue("cue-before", aCueBefore);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCueBefore(const nsString& aCueBefore)
{
  return SetProperty("cue-before", aCueBefore, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCursor(nsString& aCursor)
{
  return GetPropertyValue("cursor", aCursor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCursor(const nsString& aCursor)
{
  return SetProperty("cursor", aCursor, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetDirection(nsString& aDirection)
{
  return GetPropertyValue("direction", aDirection);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetDirection(const nsString& aDirection)
{
  return SetProperty("direction", aDirection, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetDisplay(nsString& aDisplay)
{
  return GetPropertyValue("display", aDisplay);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetDisplay(const nsString& aDisplay)
{
  return SetProperty("display", aDisplay, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetElevation(nsString& aElevation)
{
  return GetPropertyValue("elevation", aElevation);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetElevation(const nsString& aElevation)
{
  return SetProperty("elevation", aElevation, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetEmptyCells(nsString& aEmptyCells)
{
  return GetPropertyValue("empty-cells", aEmptyCells);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetEmptyCells(const nsString& aEmptyCells)
{
  return SetProperty("empty-cells", aEmptyCells, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFont(nsString& aFont)
{
  return GetPropertyValue("font", aFont);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFont(const nsString& aFont)
{
  return SetProperty("font", aFont, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFontFamily(nsString& aFontFamily)
{
  return GetPropertyValue("font-family", aFontFamily);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFontFamily(const nsString& aFontFamily)
{
  return SetProperty("font-family", aFontFamily, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFontSize(nsString& aFontSize)
{
  return GetPropertyValue("font-size", aFontSize);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFontSize(const nsString& aFontSize)
{
  return SetProperty("font-size", aFontSize, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFontSizeAdjust(nsString& aFontSizeAdjust)
{
  return GetPropertyValue("font-size-adjust", aFontSizeAdjust);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFontSizeAdjust(const nsString& aFontSizeAdjust)
{
  return SetProperty("font-size-adjust", aFontSizeAdjust, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFontStretch(nsString& aFontStretch)
{
  return GetPropertyValue("font-stretch", aFontStretch);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFontStretch(const nsString& aFontStretch)
{
  return SetProperty("font-stretch", aFontStretch, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFontStyle(nsString& aFontStyle)
{
  return GetPropertyValue("font-style", aFontStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFontStyle(const nsString& aFontStyle)
{
  return SetProperty("font-style", aFontStyle, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFontVariant(nsString& aFontVariant)
{
  return GetPropertyValue("font-variant", aFontVariant);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFontVariant(const nsString& aFontVariant)
{
  return SetProperty("font-variant", aFontVariant, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFontWeight(nsString& aFontWeight)
{
  return GetPropertyValue("font-weight", aFontWeight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFontWeight(const nsString& aFontWeight)
{
  return SetProperty("font-weight", aFontWeight, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetHeight(nsString& aHeight)
{
  return GetPropertyValue("height", aHeight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetHeight(const nsString& aHeight)
{
  return SetProperty("height", aHeight, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetLeft(nsString& aLeft)
{
  return GetPropertyValue("left", aLeft);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetLeft(const nsString& aLeft)
{
  return SetProperty("left", aLeft, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetLetterSpacing(nsString& aLetterSpacing)
{
  return GetPropertyValue("letter-spacing", aLetterSpacing);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetLetterSpacing(const nsString& aLetterSpacing)
{
  return SetProperty("letter-spacing", aLetterSpacing, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetLineHeight(nsString& aLineHeight)
{
  return GetPropertyValue("line-height", aLineHeight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetLineHeight(const nsString& aLineHeight)
{
  return SetProperty("line-height", aLineHeight, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetListStyle(nsString& aListStyle)
{
  return GetPropertyValue("list-style", aListStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetListStyle(const nsString& aListStyle)
{
  return SetProperty("list-style", aListStyle, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetListStyleImage(nsString& aListStyleImage)
{
  return GetPropertyValue("list-style-image", aListStyleImage);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetListStyleImage(const nsString& aListStyleImage)
{
  return SetProperty("list-style-image", aListStyleImage, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetListStylePosition(nsString& aListStylePosition)
{
  return GetPropertyValue("list-style-position", aListStylePosition);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetListStylePosition(const nsString& aListStylePosition)
{
  return SetProperty("list-style-position", aListStylePosition, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetListStyleType(nsString& aListStyleType)
{
  return GetPropertyValue("list-style-type", aListStyleType);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetListStyleType(const nsString& aListStyleType)
{
  return SetProperty("list-style-type", aListStyleType, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMargin(nsString& aMargin)
{
  return GetPropertyValue("margin", aMargin);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMargin(const nsString& aMargin)
{
  return SetProperty("margin", aMargin, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMarginTop(nsString& aMarginTop)
{
  return GetPropertyValue("margin-top", aMarginTop);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMarginTop(const nsString& aMarginTop)
{
  return SetProperty("margin-top", aMarginTop, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMarginRight(nsString& aMarginRight)
{
  return GetPropertyValue("margin-right", aMarginRight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMarginRight(const nsString& aMarginRight)
{
  return SetProperty("margin-right", aMarginRight, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMarginBottom(nsString& aMarginBottom)
{
  return GetPropertyValue("margin-bottom", aMarginBottom);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMarginBottom(const nsString& aMarginBottom)
{
  return SetProperty("margin-bottom", aMarginBottom, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMarginLeft(nsString& aMarginLeft)
{
  return GetPropertyValue("margin-left", aMarginLeft);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMarginLeft(const nsString& aMarginLeft)
{
  return SetProperty("margin-left", aMarginLeft, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMarkerOffset(nsString& aMarkerOffset)
{
  return GetPropertyValue("marker-offset", aMarkerOffset);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMarkerOffset(const nsString& aMarkerOffset)
{
  return SetProperty("marker-offset", aMarkerOffset, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMarks(nsString& aMarks)
{
  return GetPropertyValue("marks", aMarks);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMarks(const nsString& aMarks)
{
  return SetProperty("marks", aMarks, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMaxHeight(nsString& aMaxHeight)
{
  return GetPropertyValue("max-height", aMaxHeight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMaxHeight(const nsString& aMaxHeight)
{
  return SetProperty("max-height", aMaxHeight, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMaxWidth(nsString& aMaxWidth)
{
  return GetPropertyValue("max-width", aMaxWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMaxWidth(const nsString& aMaxWidth)
{
  return SetProperty("max-width", aMaxWidth, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMinHeight(nsString& aMinHeight)
{
  return GetPropertyValue("min-height", aMinHeight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMinHeight(const nsString& aMinHeight)
{
  return SetProperty("min-height", aMinHeight, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMinWidth(nsString& aMinWidth)
{
  return GetPropertyValue("min-width", aMinWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMinWidth(const nsString& aMinWidth)
{
  return SetProperty("min-width", aMinWidth, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetOrphans(nsString& aOrphans)
{
  return GetPropertyValue("orphans", aOrphans);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetOrphans(const nsString& aOrphans)
{
  return SetProperty("orphans", aOrphans, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetOutline(nsString& aOutline)
{
  return GetPropertyValue("outline", aOutline);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetOutline(const nsString& aOutline)
{
  return SetProperty("outline", aOutline, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetOutlineColor(nsString& aOutlineColor)
{
  return GetPropertyValue("outline-color", aOutlineColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetOutlineColor(const nsString& aOutlineColor)
{
  return SetProperty("outline-color", aOutlineColor, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetOutlineStyle(nsString& aOutlineStyle)
{
  return GetPropertyValue("outline-style", aOutlineStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetOutlineStyle(const nsString& aOutlineStyle)
{
  return SetProperty("outline-style", aOutlineStyle, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetOutlineWidth(nsString& aOutlineWidth)
{
  return GetPropertyValue("outline-width", aOutlineWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetOutlineWidth(const nsString& aOutlineWidth)
{
  return SetProperty("outline-width", aOutlineWidth, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetOverflow(nsString& aOverflow)
{
  return GetPropertyValue("overflow", aOverflow);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetOverflow(const nsString& aOverflow)
{
  return SetProperty("overflow", aOverflow, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPadding(nsString& aPadding)
{
  return GetPropertyValue("padding", aPadding);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPadding(const nsString& aPadding)
{
  return SetProperty("padding", aPadding, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPaddingTop(nsString& aPaddingTop)
{
  return GetPropertyValue("padding-top", aPaddingTop);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPaddingTop(const nsString& aPaddingTop)
{
  return SetProperty("padding-top", aPaddingTop, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPaddingRight(nsString& aPaddingRight)
{
  return GetPropertyValue("padding-right", aPaddingRight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPaddingRight(const nsString& aPaddingRight)
{
  return SetProperty("padding-right", aPaddingRight, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPaddingBottom(nsString& aPaddingBottom)
{
  return GetPropertyValue("padding-bottom", aPaddingBottom);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPaddingBottom(const nsString& aPaddingBottom)
{
  return SetProperty("padding-bottom", aPaddingBottom, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPaddingLeft(nsString& aPaddingLeft)
{
  return GetPropertyValue("padding-left", aPaddingLeft);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPaddingLeft(const nsString& aPaddingLeft)
{
  return SetProperty("padding-left", aPaddingLeft, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPage(nsString& aPage)
{
  return GetPropertyValue("page", aPage);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPage(const nsString& aPage)
{
  return SetProperty("page", aPage, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPageBreakAfter(nsString& aPageBreakAfter)
{
  return GetPropertyValue("page-break-after", aPageBreakAfter);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPageBreakAfter(const nsString& aPageBreakAfter)
{
  return SetProperty("page-break-after", aPageBreakAfter, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPageBreakBefore(nsString& aPageBreakBefore)
{
  return GetPropertyValue("page-break-before", aPageBreakBefore);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPageBreakBefore(const nsString& aPageBreakBefore)
{
  return SetProperty("page-break-before", aPageBreakBefore, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPageBreakInside(nsString& aPageBreakInside)
{
  return GetPropertyValue("page-break-inside", aPageBreakInside);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPageBreakInside(const nsString& aPageBreakInside)
{
  return SetProperty("page-break-inside", aPageBreakInside, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPause(nsString& aPause)
{
  return GetPropertyValue("pause", aPause);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPause(const nsString& aPause)
{
  return SetProperty("pause", aPause, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPauseAfter(nsString& aPauseAfter)
{
  return GetPropertyValue("pause-after", aPauseAfter);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPauseAfter(const nsString& aPauseAfter)
{
  return SetProperty("pause-after", aPauseAfter, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPauseBefore(nsString& aPauseBefore)
{
  return GetPropertyValue("pause-before", aPauseBefore);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPauseBefore(const nsString& aPauseBefore)
{
  return SetProperty("pause-before", aPauseBefore, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPitch(nsString& aPitch)
{
  return GetPropertyValue("pitch", aPitch);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPitch(const nsString& aPitch)
{
  return SetProperty("pitch", aPitch, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPitchRange(nsString& aPitchRange)
{
  return GetPropertyValue("pitch-range", aPitchRange);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPitchRange(const nsString& aPitchRange)
{
  return SetProperty("pitch-range", aPitchRange, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPlayDuring(nsString& aPlayDuring)
{
  return GetPropertyValue("play-during", aPlayDuring);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPlayDuring(const nsString& aPlayDuring)
{
  return SetProperty("play-during", aPlayDuring, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPosition(nsString& aPosition)
{
  return GetPropertyValue("position", aPosition);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPosition(const nsString& aPosition)
{
  return SetProperty("position", aPosition, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetQuotes(nsString& aQuotes)
{
  return GetPropertyValue("quotes", aQuotes);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetQuotes(const nsString& aQuotes)
{
  return SetProperty("quotes", aQuotes, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetRichness(nsString& aRichness)
{
  return GetPropertyValue("richness", aRichness);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetRichness(const nsString& aRichness)
{
  return SetProperty("richness", aRichness, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetRight(nsString& aRight)
{
  return GetPropertyValue("right", aRight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetRight(const nsString& aRight)
{
  return SetProperty("right", aRight, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetSize(nsString& aSize)
{
  return GetPropertyValue("size", aSize);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetSize(const nsString& aSize)
{
  return SetProperty("size", aSize, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetSpeak(nsString& aSpeak)
{
  return GetPropertyValue("speak", aSpeak);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetSpeak(const nsString& aSpeak)
{
  return SetProperty("speak", aSpeak, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetSpeakHeader(nsString& aSpeakHeader)
{
  return GetPropertyValue("speak-header", aSpeakHeader);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetSpeakHeader(const nsString& aSpeakHeader)
{
  return SetProperty("speak-header", aSpeakHeader, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetSpeakNumeral(nsString& aSpeakNumeral)
{
  return GetPropertyValue("speak-numeral", aSpeakNumeral);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetSpeakNumeral(const nsString& aSpeakNumeral)
{
  return SetProperty("speak-numeral", aSpeakNumeral, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetSpeakPunctuation(nsString& aSpeakPunctuation)
{
  return GetPropertyValue("speak-punctuation", aSpeakPunctuation);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetSpeakPunctuation(const nsString& aSpeakPunctuation)
{
  return SetProperty("speak-punctuation", aSpeakPunctuation, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetSpeechRate(nsString& aSpeechRate)
{
  return GetPropertyValue("speech-rate", aSpeechRate);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetSpeechRate(const nsString& aSpeechRate)
{
  return SetProperty("speech-rate", aSpeechRate, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetStress(nsString& aStress)
{
  return GetPropertyValue("stress", aStress);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetStress(const nsString& aStress)
{
  return SetProperty("stress", aStress, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetTableLayout(nsString& aTableLayout)
{
  return GetPropertyValue("table-layout", aTableLayout);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetTableLayout(const nsString& aTableLayout)
{
  return SetProperty("table-layout", aTableLayout, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetTextAlign(nsString& aTextAlign)
{
  return GetPropertyValue("text-align", aTextAlign);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetTextAlign(const nsString& aTextAlign)
{
  return SetProperty("text-align", aTextAlign, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetTextDecoration(nsString& aTextDecoration)
{
  return GetPropertyValue("text-decoration", aTextDecoration);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetTextDecoration(const nsString& aTextDecoration)
{
  return SetProperty("text-decoration", aTextDecoration, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetTextIndent(nsString& aTextIndent)
{
  return GetPropertyValue("text-indent", aTextIndent);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetTextIndent(const nsString& aTextIndent)
{
  return SetProperty("text-indent", aTextIndent, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetTextShadow(nsString& aTextShadow)
{
  return GetPropertyValue("text-shadow", aTextShadow);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetTextShadow(const nsString& aTextShadow)
{
  return SetProperty("text-shadow", aTextShadow, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetTextTransform(nsString& aTextTransform)
{
  return GetPropertyValue("text-transform", aTextTransform);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetTextTransform(const nsString& aTextTransform)
{
  return SetProperty("text-transform", aTextTransform, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetTop(nsString& aTop)
{
  return GetPropertyValue("top", aTop);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetTop(const nsString& aTop)
{
  return SetProperty("top", aTop, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetUnicodeBidi(nsString& aUnicodeBidi)
{
  return GetPropertyValue("unicode-bidi", aUnicodeBidi);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetUnicodeBidi(const nsString& aUnicodeBidi)
{
  return SetProperty("unicode-bidi", aUnicodeBidi, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetVerticalAlign(nsString& aVerticalAlign)
{
  return GetPropertyValue("vertical-align", aVerticalAlign);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetVerticalAlign(const nsString& aVerticalAlign)
{
  return SetProperty("vertical-align", aVerticalAlign, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetVisibility(nsString& aVisibility)
{
  return GetPropertyValue("visibility", aVisibility);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetVisibility(const nsString& aVisibility)
{
  return SetProperty("visibility", aVisibility, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetVoiceFamily(nsString& aVoiceFamily)
{
  return GetPropertyValue("voice-family", aVoiceFamily);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetVoiceFamily(const nsString& aVoiceFamily)
{
  return SetProperty("voice-family", aVoiceFamily, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetVolume(nsString& aVolume)
{
  return GetPropertyValue("volume", aVolume);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetVolume(const nsString& aVolume)
{
  return SetProperty("volume", aVolume, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetWhiteSpace(nsString& aWhiteSpace)
{
  return GetPropertyValue("white-space", aWhiteSpace);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetWhiteSpace(const nsString& aWhiteSpace)
{
  return SetProperty("white-space", aWhiteSpace, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetWidows(nsString& aWidows)
{
  return GetPropertyValue("widows", aWidows);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetWidows(const nsString& aWidows)
{
  return SetProperty("widows", aWidows, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetWidth(nsString& aWidth)
{
  return GetPropertyValue("width", aWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetWidth(const nsString& aWidth)
{
  return SetProperty("width", aWidth, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetWordSpacing(nsString& aWordSpacing)
{
  return GetPropertyValue("word-spacing", aWordSpacing);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetWordSpacing(const nsString& aWordSpacing)
{
  return SetProperty("word-spacing", aWordSpacing, "");
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetZIndex(nsString& aZIndex)
{
  return GetPropertyValue("z-index", aZIndex);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetZIndex(const nsString& aZIndex)
{
  return SetProperty("z-index", aZIndex, "");
}


NS_IMETHODIMP 
nsDOMCSSDeclaration::GetOpacity(nsString& aOpacity)
{
  return GetPropertyValue("opacity", aOpacity);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetOpacity(const nsString& aOpacity)
{
  return SetProperty("opacity", aOpacity, "");
}

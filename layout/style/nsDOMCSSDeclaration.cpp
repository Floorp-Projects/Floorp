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

#include "nsDOMCSSDeclaration.h"
#include "nsIDOMCSSRule.h"
#include "nsICSSParser.h"
#include "nsIStyleRule.h"
#include "nsICSSDeclaration.h"
#include "nsCSSProps.h"
#include "nsCOMPtr.h"
#include "nsIURL.h"

#include "nsContentUtils.h"


nsDOMCSSDeclaration::nsDOMCSSDeclaration()
{
  NS_INIT_REFCNT();
}

nsDOMCSSDeclaration::~nsDOMCSSDeclaration()
{
}


// QueryInterface implementation for CSSStyleSheetImpl
NS_INTERFACE_MAP_BEGIN(nsDOMCSSDeclaration)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSStyleDeclaration)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSS2Properties)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSCSS2Properties)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMCSS2Properties)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CSSStyleDeclaration)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsDOMCSSDeclaration);
NS_IMPL_RELEASE(nsDOMCSSDeclaration);


NS_IMETHODIMP
nsDOMCSSDeclaration::GetCssText(nsAWritableString& aCssText)
{
  nsCOMPtr<nsICSSDeclaration> decl;
  aCssText.Truncate();
  GetCSSDeclaration(getter_AddRefs(decl), PR_FALSE);
  NS_ASSERTION(decl, "null CSSDeclaration");

  if (decl) {
    decl->ToString(aCssText);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCssText(const nsAReadableString& aCssText)
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
nsDOMCSSDeclaration::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  NS_ENSURE_ARG_POINTER(aParentRule);
  *aParentRule = nsnull;

  nsCOMPtr<nsISupports> parent;

  GetParent(getter_AddRefs(parent));

  if (parent) {
    parent->QueryInterface(NS_GET_IID(nsIDOMCSSRule), (void **)aParentRule);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPropertyCSSValue(const nsAReadableString& aPropertyName,
                                         nsIDOMCSSValue** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  // We don't support CSSValue yet so we'll just return null...
  *aReturn = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSDeclaration::Item(PRUint32 aIndex, nsAWritableString& aReturn)
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
nsDOMCSSDeclaration::GetPropertyValue(const nsAReadableString& aPropertyName, 
                                     nsAWritableString& aReturn)
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
nsDOMCSSDeclaration::GetPropertyPriority(const nsAReadableString& aPropertyName, 
                                        nsAWritableString& aReturn)
{
  nsICSSDeclaration *decl;
  nsresult result = GetCSSDeclaration(&decl, PR_FALSE);
  PRBool isImportant = PR_FALSE;

  if ((NS_OK == result) && (nsnull != decl)) {
    result = decl->GetValueIsImportant(aPropertyName, isImportant);
    NS_RELEASE(decl);
  }

  if ((NS_OK == result) && isImportant) {
    aReturn.Assign(NS_LITERAL_STRING("!important"));    
  }
  else {
    aReturn.SetLength(0);
  }

  return result;
}

NS_IMETHODIMP    
nsDOMCSSDeclaration::SetProperty(const nsAReadableString& aPropertyName, 
                                 const nsAReadableString& aValue, 
                                 const nsAReadableString& aPriority)
{
  if (!aValue.Length()) {
     // If the new value of the property is an empty string we remove the
     // property.
    nsAutoString tmp;
    return RemoveProperty(aPropertyName, tmp);
  }

  nsAutoString declString;

  declString.Assign(aPropertyName);
  declString.Append(PRUnichar(':'));
  declString.Append(aValue);
  declString.Append(aPriority);

  return ParseDeclaration(declString, PR_TRUE, PR_FALSE);
}

NS_IMETHODIMP 
nsDOMCSSDeclaration::GetAzimuth(nsAWritableString& aAzimuth)
{
  return GetPropertyValue(NS_LITERAL_STRING("azimuth"), aAzimuth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetAzimuth(const nsAReadableString& aAzimuth)
{
  return SetProperty(NS_LITERAL_STRING("azimuth"), aAzimuth, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBackground(nsAWritableString& aBackground)
{
  return GetPropertyValue(NS_LITERAL_STRING("background"), aBackground);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBackground(const nsAReadableString& aBackground)
{
  return SetProperty(NS_LITERAL_STRING("background"), aBackground, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBackgroundAttachment(nsAWritableString& aBackgroundAttachment)
{
  return GetPropertyValue(NS_LITERAL_STRING("background-attachment"), aBackgroundAttachment);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBackgroundAttachment(const nsAReadableString& aBackgroundAttachment)
{
  return SetProperty(NS_LITERAL_STRING("background-attachment"), aBackgroundAttachment, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBackgroundColor(nsAWritableString& aBackgroundColor)
{
  return GetPropertyValue(NS_LITERAL_STRING("background-color"), aBackgroundColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBackgroundColor(const nsAReadableString& aBackgroundColor)
{
  return SetProperty(NS_LITERAL_STRING("background-color"), aBackgroundColor, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBackgroundImage(nsAWritableString& aBackgroundImage)
{
  return GetPropertyValue(NS_LITERAL_STRING("background-image"), aBackgroundImage);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBackgroundImage(const nsAReadableString& aBackgroundImage)
{
  return SetProperty(NS_LITERAL_STRING("background-image"), aBackgroundImage, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBackgroundPosition(nsAWritableString& aBackgroundPosition)
{
  return GetPropertyValue(NS_LITERAL_STRING("background-position"), aBackgroundPosition);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBackgroundPosition(const nsAReadableString& aBackgroundPosition)
{
  return SetProperty(NS_LITERAL_STRING("background-position"), aBackgroundPosition, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBackgroundRepeat(nsAWritableString& aBackgroundRepeat)
{
  return GetPropertyValue(NS_LITERAL_STRING("background-repeat"), aBackgroundRepeat);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBackgroundRepeat(const nsAReadableString& aBackgroundRepeat)
{
  return SetProperty(NS_LITERAL_STRING("background-repeat"), aBackgroundRepeat, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorder(nsAWritableString& aBorder)
{
  return GetPropertyValue(NS_LITERAL_STRING("border"), aBorder);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorder(const nsAReadableString& aBorder)
{
  return SetProperty(NS_LITERAL_STRING("border"), aBorder, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderCollapse(nsAWritableString& aBorderCollapse)
{
  return GetPropertyValue(NS_LITERAL_STRING("border-collapse"), aBorderCollapse);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderCollapse(const nsAReadableString& aBorderCollapse)
{
  return SetProperty(NS_LITERAL_STRING("border-collapse"), aBorderCollapse, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderColor(nsAWritableString& aBorderColor)
{
  return GetPropertyValue(NS_LITERAL_STRING("border-color"), aBorderColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderColor(const nsAReadableString& aBorderColor)
{
  return SetProperty(NS_LITERAL_STRING("border-color"), aBorderColor, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderSpacing(nsAWritableString& aBorderSpacing)
{
  return GetPropertyValue(NS_LITERAL_STRING("border-spacing"), aBorderSpacing);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderSpacing(const nsAReadableString& aBorderSpacing)
{
  return SetProperty(NS_LITERAL_STRING("border-spacing"), aBorderSpacing, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderStyle(nsAWritableString& aBorderStyle)
{
  return GetPropertyValue(NS_LITERAL_STRING("border-style"), aBorderStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderStyle(const nsAReadableString& aBorderStyle)
{
  return SetProperty(NS_LITERAL_STRING("border-style"), aBorderStyle, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderTop(nsAWritableString& aBorderTop)
{
  return GetPropertyValue(NS_LITERAL_STRING("border-top"), aBorderTop);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderTop(const nsAReadableString& aBorderTop)
{
  return SetProperty(NS_LITERAL_STRING("border-top"), aBorderTop, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderRight(nsAWritableString& aBorderRight)
{
  return GetPropertyValue(NS_LITERAL_STRING("border-right"), aBorderRight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderRight(const nsAReadableString& aBorderRight)
{
  return SetProperty(NS_LITERAL_STRING("border-right"), aBorderRight, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderBottom(nsAWritableString& aBorderBottom)
{
  return GetPropertyValue(NS_LITERAL_STRING("border-bottom"), aBorderBottom);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderBottom(const nsAReadableString& aBorderBottom)
{
  return SetProperty(NS_LITERAL_STRING("border-bottom"), aBorderBottom, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderLeft(nsAWritableString& aBorderLeft)
{
  return GetPropertyValue(NS_LITERAL_STRING("border-left"), aBorderLeft);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderLeft(const nsAReadableString& aBorderLeft)
{
  return SetProperty(NS_LITERAL_STRING("border-left"), aBorderLeft, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderTopColor(nsAWritableString& aBorderTopColor)
{
  return GetPropertyValue(NS_LITERAL_STRING("border-top-color"), aBorderTopColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderTopColor(const nsAReadableString& aBorderTopColor)
{
  return SetProperty(NS_LITERAL_STRING("border-top-color"), aBorderTopColor, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderRightColor(nsAWritableString& aBorderRightColor)
{
  return GetPropertyValue(NS_LITERAL_STRING("border-right-color"), aBorderRightColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderRightColor(const nsAReadableString& aBorderRightColor)
{
  return SetProperty(NS_LITERAL_STRING("border-right-color"), aBorderRightColor, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderBottomColor(nsAWritableString& aBorderBottomColor)
{
  return GetPropertyValue(NS_LITERAL_STRING("border-bottom-color"), aBorderBottomColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderBottomColor(const nsAReadableString& aBorderBottomColor)
{
  return SetProperty(NS_LITERAL_STRING("border-bottom-color"), aBorderBottomColor, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderLeftColor(nsAWritableString& aBorderLeftColor)
{
  return GetPropertyValue(NS_LITERAL_STRING("border-left-color"), aBorderLeftColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderLeftColor(const nsAReadableString& aBorderLeftColor)
{
  return SetProperty(NS_LITERAL_STRING("border-left-color"), aBorderLeftColor, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderTopStyle(nsAWritableString& aBorderTopStyle)
{
  return GetPropertyValue(NS_LITERAL_STRING("border-top-style"), aBorderTopStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderTopStyle(const nsAReadableString& aBorderTopStyle)
{
  return SetProperty(NS_LITERAL_STRING("border-top-style"), aBorderTopStyle, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderRightStyle(nsAWritableString& aBorderRightStyle)
{
  return GetPropertyValue(NS_LITERAL_STRING("border-right-style"), aBorderRightStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderRightStyle(const nsAReadableString& aBorderRightStyle)
{
  return SetProperty(NS_LITERAL_STRING("border-right-style"), aBorderRightStyle, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderBottomStyle(nsAWritableString& aBorderBottomStyle)
{
  return GetPropertyValue(NS_LITERAL_STRING("border-bottom-style"), aBorderBottomStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderBottomStyle(const nsAReadableString& aBorderBottomStyle)
{
  return SetProperty(NS_LITERAL_STRING("border-bottom-style"), aBorderBottomStyle, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderLeftStyle(nsAWritableString& aBorderLeftStyle)
{
  return GetPropertyValue(NS_LITERAL_STRING("border-left-style"), aBorderLeftStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderLeftStyle(const nsAReadableString& aBorderLeftStyle)
{
  return SetProperty(NS_LITERAL_STRING("border-left-style"), aBorderLeftStyle, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderTopWidth(nsAWritableString& aBorderTopWidth)
{
  return GetPropertyValue(NS_LITERAL_STRING("border-top-width"), aBorderTopWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderTopWidth(const nsAReadableString& aBorderTopWidth)
{
  return SetProperty(NS_LITERAL_STRING("border-top-width"), aBorderTopWidth, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderRightWidth(nsAWritableString& aBorderRightWidth)
{
  return GetPropertyValue(NS_LITERAL_STRING("border-right-width"), aBorderRightWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderRightWidth(const nsAReadableString& aBorderRightWidth)
{
  return SetProperty(NS_LITERAL_STRING("border-right-width"), aBorderRightWidth, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderBottomWidth(nsAWritableString& aBorderBottomWidth)
{
  return GetPropertyValue(NS_LITERAL_STRING("border-bottom-width"), aBorderBottomWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderBottomWidth(const nsAReadableString& aBorderBottomWidth)
{
  return SetProperty(NS_LITERAL_STRING("border-bottom-width"), aBorderBottomWidth, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderLeftWidth(nsAWritableString& aBorderLeftWidth)
{
  return GetPropertyValue(NS_LITERAL_STRING("border-left-width"), aBorderLeftWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderLeftWidth(const nsAReadableString& aBorderLeftWidth)
{
  return SetProperty(NS_LITERAL_STRING("border-left-width"), aBorderLeftWidth, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderWidth(nsAWritableString& aBorderWidth)
{
  return GetPropertyValue(NS_LITERAL_STRING("border-width"), aBorderWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderWidth(const nsAReadableString& aBorderWidth)
{
  return SetProperty(NS_LITERAL_STRING("border-width"), aBorderWidth, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBottom(nsAWritableString& aBottom)
{
  return GetPropertyValue(NS_LITERAL_STRING("bottom"), aBottom);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBottom(const nsAReadableString& aBottom)
{
  return SetProperty(NS_LITERAL_STRING("bottom"), aBottom, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCaptionSide(nsAWritableString& aCaptionSide)
{
  return GetPropertyValue(NS_LITERAL_STRING("caption-side"), aCaptionSide);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCaptionSide(const nsAReadableString& aCaptionSide)
{
  return SetProperty(NS_LITERAL_STRING("caption-side"), aCaptionSide, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetClear(nsAWritableString& aClear)
{
  return GetPropertyValue(NS_LITERAL_STRING("clear"), aClear);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetClear(const nsAReadableString& aClear)
{
  return SetProperty(NS_LITERAL_STRING("clear"), aClear, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetClip(nsAWritableString& aClip)
{
  return GetPropertyValue(NS_LITERAL_STRING("clip"), aClip);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetClip(const nsAReadableString& aClip)
{
  return SetProperty(NS_LITERAL_STRING("clip"), aClip, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetColor(nsAWritableString& aColor)
{
  return GetPropertyValue(NS_LITERAL_STRING("color"), aColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetColor(const nsAReadableString& aColor)
{
  return SetProperty(NS_LITERAL_STRING("color"), aColor, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetContent(nsAWritableString& aContent)
{
  return GetPropertyValue(NS_LITERAL_STRING("content"), aContent);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetContent(const nsAReadableString& aContent)
{
  return SetProperty(NS_LITERAL_STRING("content"), aContent, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCounterIncrement(nsAWritableString& aCounterIncrement)
{
  return GetPropertyValue(NS_LITERAL_STRING("counter-increment"), aCounterIncrement);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCounterIncrement(const nsAReadableString& aCounterIncrement)
{
  return SetProperty(NS_LITERAL_STRING("counter-increment"), aCounterIncrement, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCounterReset(nsAWritableString& aCounterReset)
{
  return GetPropertyValue(NS_LITERAL_STRING("counter-reset"), aCounterReset);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCounterReset(const nsAReadableString& aCounterReset)
{
  return SetProperty(NS_LITERAL_STRING("counter-reset"), aCounterReset, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCssFloat(nsAWritableString& aCssFloat)
{
  return GetPropertyValue(NS_LITERAL_STRING("float"), aCssFloat);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCssFloat(const nsAReadableString& aCssFloat)
{
  return SetProperty(NS_LITERAL_STRING("float"), aCssFloat, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCue(nsAWritableString& aCue)
{
  return GetPropertyValue(NS_LITERAL_STRING("cue"), aCue);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCue(const nsAReadableString& aCue)
{
  return SetProperty(NS_LITERAL_STRING("cue"), aCue, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCueAfter(nsAWritableString& aCueAfter)
{
  return GetPropertyValue(NS_LITERAL_STRING("cue-after"), aCueAfter);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCueAfter(const nsAReadableString& aCueAfter)
{
  return SetProperty(NS_LITERAL_STRING("cue-after"), aCueAfter, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCueBefore(nsAWritableString& aCueBefore)
{
  return GetPropertyValue(NS_LITERAL_STRING("cue-before"), aCueBefore);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCueBefore(const nsAReadableString& aCueBefore)
{
  return SetProperty(NS_LITERAL_STRING("cue-before"), aCueBefore, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCursor(nsAWritableString& aCursor)
{
  return GetPropertyValue(NS_LITERAL_STRING("cursor"), aCursor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCursor(const nsAReadableString& aCursor)
{
  return SetProperty(NS_LITERAL_STRING("cursor"), aCursor, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetDirection(nsAWritableString& aDirection)
{
  return GetPropertyValue(NS_LITERAL_STRING("direction"), aDirection);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetDirection(const nsAReadableString& aDirection)
{
  return SetProperty(NS_LITERAL_STRING("direction"), aDirection, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetDisplay(nsAWritableString& aDisplay)
{
  return GetPropertyValue(NS_LITERAL_STRING("display"), aDisplay);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetDisplay(const nsAReadableString& aDisplay)
{
  return SetProperty(NS_LITERAL_STRING("display"), aDisplay, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetElevation(nsAWritableString& aElevation)
{
  return GetPropertyValue(NS_LITERAL_STRING("elevation"), aElevation);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetElevation(const nsAReadableString& aElevation)
{
  return SetProperty(NS_LITERAL_STRING("elevation"), aElevation, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetEmptyCells(nsAWritableString& aEmptyCells)
{
  return GetPropertyValue(NS_LITERAL_STRING("empty-cells"), aEmptyCells);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetEmptyCells(const nsAReadableString& aEmptyCells)
{
  return SetProperty(NS_LITERAL_STRING("empty-cells"), aEmptyCells, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFont(nsAWritableString& aFont)
{
  return GetPropertyValue(NS_LITERAL_STRING("font"), aFont);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFont(const nsAReadableString& aFont)
{
  return SetProperty(NS_LITERAL_STRING("font"), aFont, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFontFamily(nsAWritableString& aFontFamily)
{
  return GetPropertyValue(NS_LITERAL_STRING("font-family"), aFontFamily);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFontFamily(const nsAReadableString& aFontFamily)
{
  return SetProperty(NS_LITERAL_STRING("font-family"), aFontFamily, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFontSize(nsAWritableString& aFontSize)
{
  return GetPropertyValue(NS_LITERAL_STRING("font-size"), aFontSize);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFontSize(const nsAReadableString& aFontSize)
{
  return SetProperty(NS_LITERAL_STRING("font-size"), aFontSize, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFontSizeAdjust(nsAWritableString& aFontSizeAdjust)
{
  return GetPropertyValue(NS_LITERAL_STRING("font-size-adjust"), aFontSizeAdjust);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFontSizeAdjust(const nsAReadableString& aFontSizeAdjust)
{
  return SetProperty(NS_LITERAL_STRING("font-size-adjust"), aFontSizeAdjust, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFontStretch(nsAWritableString& aFontStretch)
{
  return GetPropertyValue(NS_LITERAL_STRING("font-stretch"), aFontStretch);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFontStretch(const nsAReadableString& aFontStretch)
{
  return SetProperty(NS_LITERAL_STRING("font-stretch"), aFontStretch, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFontStyle(nsAWritableString& aFontStyle)
{
  return GetPropertyValue(NS_LITERAL_STRING("font-style"), aFontStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFontStyle(const nsAReadableString& aFontStyle)
{
  return SetProperty(NS_LITERAL_STRING("font-style"), aFontStyle, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFontVariant(nsAWritableString& aFontVariant)
{
  return GetPropertyValue(NS_LITERAL_STRING("font-variant"), aFontVariant);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFontVariant(const nsAReadableString& aFontVariant)
{
  return SetProperty(NS_LITERAL_STRING("font-variant"), aFontVariant, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFontWeight(nsAWritableString& aFontWeight)
{
  return GetPropertyValue(NS_LITERAL_STRING("font-weight"), aFontWeight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFontWeight(const nsAReadableString& aFontWeight)
{
  return SetProperty(NS_LITERAL_STRING("font-weight"), aFontWeight, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetHeight(nsAWritableString& aHeight)
{
  return GetPropertyValue(NS_LITERAL_STRING("height"), aHeight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetHeight(const nsAReadableString& aHeight)
{
  return SetProperty(NS_LITERAL_STRING("height"), aHeight, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetLeft(nsAWritableString& aLeft)
{
  return GetPropertyValue(NS_LITERAL_STRING("left"), aLeft);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetLeft(const nsAReadableString& aLeft)
{
  return SetProperty(NS_LITERAL_STRING("left"), aLeft, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetLetterSpacing(nsAWritableString& aLetterSpacing)
{
  return GetPropertyValue(NS_LITERAL_STRING("letter-spacing"), aLetterSpacing);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetLetterSpacing(const nsAReadableString& aLetterSpacing)
{
  return SetProperty(NS_LITERAL_STRING("letter-spacing"), aLetterSpacing, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetLineHeight(nsAWritableString& aLineHeight)
{
  return GetPropertyValue(NS_LITERAL_STRING("line-height"), aLineHeight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetLineHeight(const nsAReadableString& aLineHeight)
{
  return SetProperty(NS_LITERAL_STRING("line-height"), aLineHeight, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetListStyle(nsAWritableString& aListStyle)
{
  return GetPropertyValue(NS_LITERAL_STRING("list-style"), aListStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetListStyle(const nsAReadableString& aListStyle)
{
  return SetProperty(NS_LITERAL_STRING("list-style"), aListStyle, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetListStyleImage(nsAWritableString& aListStyleImage)
{
  return GetPropertyValue(NS_LITERAL_STRING("list-style-image"), aListStyleImage);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetListStyleImage(const nsAReadableString& aListStyleImage)
{
  return SetProperty(NS_LITERAL_STRING("list-style-image"), aListStyleImage, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetListStylePosition(nsAWritableString& aListStylePosition)
{
  return GetPropertyValue(NS_LITERAL_STRING("list-style-position"), aListStylePosition);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetListStylePosition(const nsAReadableString& aListStylePosition)
{
  return SetProperty(NS_LITERAL_STRING("list-style-position"), aListStylePosition, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetListStyleType(nsAWritableString& aListStyleType)
{
  return GetPropertyValue(NS_LITERAL_STRING("list-style-type"), aListStyleType);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetListStyleType(const nsAReadableString& aListStyleType)
{
  return SetProperty(NS_LITERAL_STRING("list-style-type"), aListStyleType, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMargin(nsAWritableString& aMargin)
{
  return GetPropertyValue(NS_LITERAL_STRING("margin"), aMargin);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMargin(const nsAReadableString& aMargin)
{
  return SetProperty(NS_LITERAL_STRING("margin"), aMargin, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMarginTop(nsAWritableString& aMarginTop)
{
  return GetPropertyValue(NS_LITERAL_STRING("margin-top"), aMarginTop);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMarginTop(const nsAReadableString& aMarginTop)
{
  return SetProperty(NS_LITERAL_STRING("margin-top"), aMarginTop, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMarginRight(nsAWritableString& aMarginRight)
{
  return GetPropertyValue(NS_LITERAL_STRING("margin-right"), aMarginRight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMarginRight(const nsAReadableString& aMarginRight)
{
  return SetProperty(NS_LITERAL_STRING("margin-right"), aMarginRight, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMarginBottom(nsAWritableString& aMarginBottom)
{
  return GetPropertyValue(NS_LITERAL_STRING("margin-bottom"), aMarginBottom);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMarginBottom(const nsAReadableString& aMarginBottom)
{
  return SetProperty(NS_LITERAL_STRING("margin-bottom"), aMarginBottom, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMarginLeft(nsAWritableString& aMarginLeft)
{
  return GetPropertyValue(NS_LITERAL_STRING("margin-left"), aMarginLeft);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMarginLeft(const nsAReadableString& aMarginLeft)
{
  return SetProperty(NS_LITERAL_STRING("margin-left"), aMarginLeft, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMarkerOffset(nsAWritableString& aMarkerOffset)
{
  return GetPropertyValue(NS_LITERAL_STRING("marker-offset"), aMarkerOffset);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMarkerOffset(const nsAReadableString& aMarkerOffset)
{
  return SetProperty(NS_LITERAL_STRING("marker-offset"), aMarkerOffset, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMarks(nsAWritableString& aMarks)
{
  return GetPropertyValue(NS_LITERAL_STRING("marks"), aMarks);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMarks(const nsAReadableString& aMarks)
{
  return SetProperty(NS_LITERAL_STRING("marks"), aMarks, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMaxHeight(nsAWritableString& aMaxHeight)
{
  return GetPropertyValue(NS_LITERAL_STRING("max-height"), aMaxHeight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMaxHeight(const nsAReadableString& aMaxHeight)
{
  return SetProperty(NS_LITERAL_STRING("max-height"), aMaxHeight, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMaxWidth(nsAWritableString& aMaxWidth)
{
  return GetPropertyValue(NS_LITERAL_STRING("max-width"), aMaxWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMaxWidth(const nsAReadableString& aMaxWidth)
{
  return SetProperty(NS_LITERAL_STRING("max-width"), aMaxWidth, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMinHeight(nsAWritableString& aMinHeight)
{
  return GetPropertyValue(NS_LITERAL_STRING("min-height"), aMinHeight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMinHeight(const nsAReadableString& aMinHeight)
{
  return SetProperty(NS_LITERAL_STRING("min-height"), aMinHeight, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMinWidth(nsAWritableString& aMinWidth)
{
  return GetPropertyValue(NS_LITERAL_STRING("min-width"), aMinWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMinWidth(const nsAReadableString& aMinWidth)
{
  return SetProperty(NS_LITERAL_STRING("min-width"), aMinWidth, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetOrphans(nsAWritableString& aOrphans)
{
  return GetPropertyValue(NS_LITERAL_STRING("orphans"), aOrphans);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetOrphans(const nsAReadableString& aOrphans)
{
  return SetProperty(NS_LITERAL_STRING("orphans"), aOrphans, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetOutline(nsAWritableString& aOutline)
{
  // XXX Because of the renaming to -moz-outline, this does nothing.
  return GetPropertyValue(NS_LITERAL_STRING("outline"), aOutline);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetOutline(const nsAReadableString& aOutline)
{
  // XXX Because of the renaming to -moz-outline, this does nothing.
  return SetProperty(NS_LITERAL_STRING("outline"), aOutline, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetOutlineColor(nsAWritableString& aOutlineColor)
{
  // XXX Because of the renaming to -moz-outline, this does nothing.
  return GetPropertyValue(NS_LITERAL_STRING("outline-color"), aOutlineColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetOutlineColor(const nsAReadableString& aOutlineColor)
{
  // XXX Because of the renaming to -moz-outline, this does nothing.
  return SetProperty(NS_LITERAL_STRING("outline-color"), aOutlineColor, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetOutlineStyle(nsAWritableString& aOutlineStyle)
{
  // XXX Because of the renaming to -moz-outline, this does nothing.
  return GetPropertyValue(NS_LITERAL_STRING("outline-style"), aOutlineStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetOutlineStyle(const nsAReadableString& aOutlineStyle)
{
  // XXX Because of the renaming to -moz-outline, this does nothing.
  return SetProperty(NS_LITERAL_STRING("outline-style"), aOutlineStyle, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetOutlineWidth(nsAWritableString& aOutlineWidth)
{
  // XXX Because of the renaming to -moz-outline, this does nothing.
  return GetPropertyValue(NS_LITERAL_STRING("outline-width"), aOutlineWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetOutlineWidth(const nsAReadableString& aOutlineWidth)
{
  // XXX Because of the renaming to -moz-outline, this does nothing.
  return SetProperty(NS_LITERAL_STRING("outline-width"), aOutlineWidth, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetOverflow(nsAWritableString& aOverflow)
{
  return GetPropertyValue(NS_LITERAL_STRING("overflow"), aOverflow);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetOverflow(const nsAReadableString& aOverflow)
{
  return SetProperty(NS_LITERAL_STRING("overflow"), aOverflow, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPadding(nsAWritableString& aPadding)
{
  return GetPropertyValue(NS_LITERAL_STRING("padding"), aPadding);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPadding(const nsAReadableString& aPadding)
{
  return SetProperty(NS_LITERAL_STRING("padding"), aPadding, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPaddingTop(nsAWritableString& aPaddingTop)
{
  return GetPropertyValue(NS_LITERAL_STRING("padding-top"), aPaddingTop);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPaddingTop(const nsAReadableString& aPaddingTop)
{
  return SetProperty(NS_LITERAL_STRING("padding-top"), aPaddingTop, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPaddingRight(nsAWritableString& aPaddingRight)
{
  return GetPropertyValue(NS_LITERAL_STRING("padding-right"), aPaddingRight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPaddingRight(const nsAReadableString& aPaddingRight)
{
  return SetProperty(NS_LITERAL_STRING("padding-right"), aPaddingRight, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPaddingBottom(nsAWritableString& aPaddingBottom)
{
  return GetPropertyValue(NS_LITERAL_STRING("padding-bottom"), aPaddingBottom);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPaddingBottom(const nsAReadableString& aPaddingBottom)
{
  return SetProperty(NS_LITERAL_STRING("padding-bottom"), aPaddingBottom, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPaddingLeft(nsAWritableString& aPaddingLeft)
{
  return GetPropertyValue(NS_LITERAL_STRING("padding-left"), aPaddingLeft);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPaddingLeft(const nsAReadableString& aPaddingLeft)
{
  return SetProperty(NS_LITERAL_STRING("padding-left"), aPaddingLeft, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPage(nsAWritableString& aPage)
{
  return GetPropertyValue(NS_LITERAL_STRING("page"), aPage);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPage(const nsAReadableString& aPage)
{
  return SetProperty(NS_LITERAL_STRING("page"), aPage, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPageBreakAfter(nsAWritableString& aPageBreakAfter)
{
  return GetPropertyValue(NS_LITERAL_STRING("page-break-after"), aPageBreakAfter);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPageBreakAfter(const nsAReadableString& aPageBreakAfter)
{
  return SetProperty(NS_LITERAL_STRING("page-break-after"), aPageBreakAfter, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPageBreakBefore(nsAWritableString& aPageBreakBefore)
{
  return GetPropertyValue(NS_LITERAL_STRING("page-break-before"), aPageBreakBefore);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPageBreakBefore(const nsAReadableString& aPageBreakBefore)
{
  return SetProperty(NS_LITERAL_STRING("page-break-before"), aPageBreakBefore, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPageBreakInside(nsAWritableString& aPageBreakInside)
{
  return GetPropertyValue(NS_LITERAL_STRING("page-break-inside"), aPageBreakInside);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPageBreakInside(const nsAReadableString& aPageBreakInside)
{
  return SetProperty(NS_LITERAL_STRING("page-break-inside"), aPageBreakInside, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPause(nsAWritableString& aPause)
{
  return GetPropertyValue(NS_LITERAL_STRING("pause"), aPause);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPause(const nsAReadableString& aPause)
{
  return SetProperty(NS_LITERAL_STRING("pause"), aPause, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPauseAfter(nsAWritableString& aPauseAfter)
{
  return GetPropertyValue(NS_LITERAL_STRING("pause-after"), aPauseAfter);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPauseAfter(const nsAReadableString& aPauseAfter)
{
  return SetProperty(NS_LITERAL_STRING("pause-after"), aPauseAfter, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPauseBefore(nsAWritableString& aPauseBefore)
{
  return GetPropertyValue(NS_LITERAL_STRING("pause-before"), aPauseBefore);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPauseBefore(const nsAReadableString& aPauseBefore)
{
  return SetProperty(NS_LITERAL_STRING("pause-before"), aPauseBefore, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPitch(nsAWritableString& aPitch)
{
  return GetPropertyValue(NS_LITERAL_STRING("pitch"), aPitch);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPitch(const nsAReadableString& aPitch)
{
  return SetProperty(NS_LITERAL_STRING("pitch"), aPitch, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPitchRange(nsAWritableString& aPitchRange)
{
  return GetPropertyValue(NS_LITERAL_STRING("pitch-range"), aPitchRange);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPitchRange(const nsAReadableString& aPitchRange)
{
  return SetProperty(NS_LITERAL_STRING("pitch-range"), aPitchRange, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPlayDuring(nsAWritableString& aPlayDuring)
{
  return GetPropertyValue(NS_LITERAL_STRING("play-during"), aPlayDuring);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPlayDuring(const nsAReadableString& aPlayDuring)
{
  return SetProperty(NS_LITERAL_STRING("play-during"), aPlayDuring, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPosition(nsAWritableString& aPosition)
{
  return GetPropertyValue(NS_LITERAL_STRING("position"), aPosition);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPosition(const nsAReadableString& aPosition)
{
  return SetProperty(NS_LITERAL_STRING("position"), aPosition, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetQuotes(nsAWritableString& aQuotes)
{
  return GetPropertyValue(NS_LITERAL_STRING("quotes"), aQuotes);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetQuotes(const nsAReadableString& aQuotes)
{
  return SetProperty(NS_LITERAL_STRING("quotes"), aQuotes, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetRichness(nsAWritableString& aRichness)
{
  return GetPropertyValue(NS_LITERAL_STRING("richness"), aRichness);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetRichness(const nsAReadableString& aRichness)
{
  return SetProperty(NS_LITERAL_STRING("richness"), aRichness, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetRight(nsAWritableString& aRight)
{
  return GetPropertyValue(NS_LITERAL_STRING("right"), aRight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetRight(const nsAReadableString& aRight)
{
  return SetProperty(NS_LITERAL_STRING("right"), aRight, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetSize(nsAWritableString& aSize)
{
  return GetPropertyValue(NS_LITERAL_STRING("size"), aSize);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetSize(const nsAReadableString& aSize)
{
  return SetProperty(NS_LITERAL_STRING("size"), aSize, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetSpeak(nsAWritableString& aSpeak)
{
  return GetPropertyValue(NS_LITERAL_STRING("speak"), aSpeak);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetSpeak(const nsAReadableString& aSpeak)
{
  return SetProperty(NS_LITERAL_STRING("speak"), aSpeak, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetSpeakHeader(nsAWritableString& aSpeakHeader)
{
  return GetPropertyValue(NS_LITERAL_STRING("speak-header"), aSpeakHeader);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetSpeakHeader(const nsAReadableString& aSpeakHeader)
{
  return SetProperty(NS_LITERAL_STRING("speak-header"), aSpeakHeader, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetSpeakNumeral(nsAWritableString& aSpeakNumeral)
{
  return GetPropertyValue(NS_LITERAL_STRING("speak-numeral"), aSpeakNumeral);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetSpeakNumeral(const nsAReadableString& aSpeakNumeral)
{
  return SetProperty(NS_LITERAL_STRING("speak-numeral"), aSpeakNumeral, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetSpeakPunctuation(nsAWritableString& aSpeakPunctuation)
{
  return GetPropertyValue(NS_LITERAL_STRING("speak-punctuation"), aSpeakPunctuation);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetSpeakPunctuation(const nsAReadableString& aSpeakPunctuation)
{
  return SetProperty(NS_LITERAL_STRING("speak-punctuation"), aSpeakPunctuation, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetSpeechRate(nsAWritableString& aSpeechRate)
{
  return GetPropertyValue(NS_LITERAL_STRING("speech-rate"), aSpeechRate);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetSpeechRate(const nsAReadableString& aSpeechRate)
{
  return SetProperty(NS_LITERAL_STRING("speech-rate"), aSpeechRate, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetStress(nsAWritableString& aStress)
{
  return GetPropertyValue(NS_LITERAL_STRING("stress"), aStress);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetStress(const nsAReadableString& aStress)
{
  return SetProperty(NS_LITERAL_STRING("stress"), aStress, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetTableLayout(nsAWritableString& aTableLayout)
{
  return GetPropertyValue(NS_LITERAL_STRING("table-layout"), aTableLayout);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetTableLayout(const nsAReadableString& aTableLayout)
{
  return SetProperty(NS_LITERAL_STRING("table-layout"), aTableLayout, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetTextAlign(nsAWritableString& aTextAlign)
{
  return GetPropertyValue(NS_LITERAL_STRING("text-align"), aTextAlign);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetTextAlign(const nsAReadableString& aTextAlign)
{
  return SetProperty(NS_LITERAL_STRING("text-align"), aTextAlign, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetTextDecoration(nsAWritableString& aTextDecoration)
{
  return GetPropertyValue(NS_LITERAL_STRING("text-decoration"), aTextDecoration);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetTextDecoration(const nsAReadableString& aTextDecoration)
{
  return SetProperty(NS_LITERAL_STRING("text-decoration"), aTextDecoration, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetTextIndent(nsAWritableString& aTextIndent)
{
  return GetPropertyValue(NS_LITERAL_STRING("text-indent"), aTextIndent);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetTextIndent(const nsAReadableString& aTextIndent)
{
  return SetProperty(NS_LITERAL_STRING("text-indent"), aTextIndent, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetTextShadow(nsAWritableString& aTextShadow)
{
  return GetPropertyValue(NS_LITERAL_STRING("text-shadow"), aTextShadow);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetTextShadow(const nsAReadableString& aTextShadow)
{
  return SetProperty(NS_LITERAL_STRING("text-shadow"), aTextShadow, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetTextTransform(nsAWritableString& aTextTransform)
{
  return GetPropertyValue(NS_LITERAL_STRING("text-transform"), aTextTransform);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetTextTransform(const nsAReadableString& aTextTransform)
{
  return SetProperty(NS_LITERAL_STRING("text-transform"), aTextTransform, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetTop(nsAWritableString& aTop)
{
  return GetPropertyValue(NS_LITERAL_STRING("top"), aTop);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetTop(const nsAReadableString& aTop)
{
  return SetProperty(NS_LITERAL_STRING("top"), aTop, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetUnicodeBidi(nsAWritableString& aUnicodeBidi)
{
  return GetPropertyValue(NS_LITERAL_STRING("unicode-bidi"), aUnicodeBidi);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetUnicodeBidi(const nsAReadableString& aUnicodeBidi)
{
  return SetProperty(NS_LITERAL_STRING("unicode-bidi"), aUnicodeBidi, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetVerticalAlign(nsAWritableString& aVerticalAlign)
{
  return GetPropertyValue(NS_LITERAL_STRING("vertical-align"), aVerticalAlign);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetVerticalAlign(const nsAReadableString& aVerticalAlign)
{
  return SetProperty(NS_LITERAL_STRING("vertical-align"), aVerticalAlign, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetVisibility(nsAWritableString& aVisibility)
{
  return GetPropertyValue(NS_LITERAL_STRING("visibility"), aVisibility);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetVisibility(const nsAReadableString& aVisibility)
{
  return SetProperty(NS_LITERAL_STRING("visibility"), aVisibility, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetVoiceFamily(nsAWritableString& aVoiceFamily)
{
  return GetPropertyValue(NS_LITERAL_STRING("voice-family"), aVoiceFamily);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetVoiceFamily(const nsAReadableString& aVoiceFamily)
{
  return SetProperty(NS_LITERAL_STRING("voice-family"), aVoiceFamily, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetVolume(nsAWritableString& aVolume)
{
  return GetPropertyValue(NS_LITERAL_STRING("volume"), aVolume);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetVolume(const nsAReadableString& aVolume)
{
  return SetProperty(NS_LITERAL_STRING("volume"), aVolume, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetWhiteSpace(nsAWritableString& aWhiteSpace)
{
  return GetPropertyValue(NS_LITERAL_STRING("white-space"), aWhiteSpace);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetWhiteSpace(const nsAReadableString& aWhiteSpace)
{
  return SetProperty(NS_LITERAL_STRING("white-space"), aWhiteSpace, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetWidows(nsAWritableString& aWidows)
{
  return GetPropertyValue(NS_LITERAL_STRING("widows"), aWidows);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetWidows(const nsAReadableString& aWidows)
{
  return SetProperty(NS_LITERAL_STRING("widows"), aWidows, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetWidth(nsAWritableString& aWidth)
{
  return GetPropertyValue(NS_LITERAL_STRING("width"), aWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetWidth(const nsAReadableString& aWidth)
{
  return SetProperty(NS_LITERAL_STRING("width"), aWidth, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetWordSpacing(nsAWritableString& aWordSpacing)
{
  return GetPropertyValue(NS_LITERAL_STRING("word-spacing"), aWordSpacing);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetWordSpacing(const nsAReadableString& aWordSpacing)
{
  return SetProperty(NS_LITERAL_STRING("word-spacing"), aWordSpacing, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetZIndex(nsAWritableString& aZIndex)
{
  return GetPropertyValue(NS_LITERAL_STRING("z-index"), aZIndex);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetZIndex(const nsAReadableString& aZIndex)
{
  return SetProperty(NS_LITERAL_STRING("z-index"), aZIndex, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMozBinding(nsAWritableString& aBehavior)
{
  return GetPropertyValue(NS_LITERAL_STRING("-moz-binding"), aBehavior);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMozBinding(const nsAReadableString& aBehavior)
{
  return SetProperty(NS_LITERAL_STRING("-moz-binding"), aBehavior, nsAutoString());
}

NS_IMETHODIMP 
nsDOMCSSDeclaration::GetMozOpacity(nsAWritableString& aOpacity)
{
  return GetPropertyValue(NS_LITERAL_STRING("-moz-opacity"), aOpacity);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMozOpacity(const nsAReadableString& aOpacity)
{
  return SetProperty(NS_LITERAL_STRING("-moz-opacity"), aOpacity, nsAutoString());
}

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
    nsCOMPtr<nsISupports> parent;

    res = GetParent(getter_AddRefs(parent));
    if (NS_OK == res) {
      nsISupports *supports = (nsISupports *)(nsIDOMCSS2Properties *)this;
      // XXX Should be done through factory
      res = NS_NewScriptCSS2Properties(aContext, 
                                       supports,
                                       parent, 
                                       (void**)&mScriptObject);
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
nsDOMCSSDeclaration::GetCssText(nsAWritableString& aCssText)
{
  aCssText.Truncate();
  // XXX TBI
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
  return GetPropertyValue(NS_ConvertASCIItoUCS2("azimuth"), aAzimuth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetAzimuth(const nsAReadableString& aAzimuth)
{
  return SetProperty(NS_ConvertASCIItoUCS2("azimuth"), aAzimuth, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBackground(nsAWritableString& aBackground)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("background"), aBackground);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBackground(const nsAReadableString& aBackground)
{
  return SetProperty(NS_ConvertASCIItoUCS2("background"), aBackground, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBackgroundAttachment(nsAWritableString& aBackgroundAttachment)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("background-attachment"), aBackgroundAttachment);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBackgroundAttachment(const nsAReadableString& aBackgroundAttachment)
{
  return SetProperty(NS_ConvertASCIItoUCS2("background-attachment"), aBackgroundAttachment, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBackgroundColor(nsAWritableString& aBackgroundColor)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("background-color"), aBackgroundColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBackgroundColor(const nsAReadableString& aBackgroundColor)
{
  return SetProperty(NS_ConvertASCIItoUCS2("background-color"), aBackgroundColor, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBackgroundImage(nsAWritableString& aBackgroundImage)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("background-image"), aBackgroundImage);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBackgroundImage(const nsAReadableString& aBackgroundImage)
{
  return SetProperty(NS_ConvertASCIItoUCS2("background-image"), aBackgroundImage, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBackgroundPosition(nsAWritableString& aBackgroundPosition)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("background-position"), aBackgroundPosition);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBackgroundPosition(const nsAReadableString& aBackgroundPosition)
{
  return SetProperty(NS_ConvertASCIItoUCS2("background-position"), aBackgroundPosition, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBackgroundRepeat(nsAWritableString& aBackgroundRepeat)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("background-repeat"), aBackgroundRepeat);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBackgroundRepeat(const nsAReadableString& aBackgroundRepeat)
{
  return SetProperty(NS_ConvertASCIItoUCS2("background-repeat"), aBackgroundRepeat, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBehavior(nsAWritableString& aBehavior)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("behavior"), aBehavior);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBehavior(const nsAReadableString& aBehavior)
{
  return SetProperty(NS_ConvertASCIItoUCS2("behavior"), aBehavior, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorder(nsAWritableString& aBorder)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border"), aBorder);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorder(const nsAReadableString& aBorder)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border"), aBorder, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderCollapse(nsAWritableString& aBorderCollapse)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-collapse"), aBorderCollapse);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderCollapse(const nsAReadableString& aBorderCollapse)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-collapse"), aBorderCollapse, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderColor(nsAWritableString& aBorderColor)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-color"), aBorderColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderColor(const nsAReadableString& aBorderColor)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-color"), aBorderColor, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderSpacing(nsAWritableString& aBorderSpacing)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-spacing"), aBorderSpacing);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderSpacing(const nsAReadableString& aBorderSpacing)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-spacing"), aBorderSpacing, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderStyle(nsAWritableString& aBorderStyle)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-style"), aBorderStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderStyle(const nsAReadableString& aBorderStyle)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-style"), aBorderStyle, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderTop(nsAWritableString& aBorderTop)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-top"), aBorderTop);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderTop(const nsAReadableString& aBorderTop)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-top"), aBorderTop, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderRight(nsAWritableString& aBorderRight)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-right"), aBorderRight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderRight(const nsAReadableString& aBorderRight)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-right"), aBorderRight, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderBottom(nsAWritableString& aBorderBottom)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-bottom"), aBorderBottom);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderBottom(const nsAReadableString& aBorderBottom)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-bottom"), aBorderBottom, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderLeft(nsAWritableString& aBorderLeft)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-left"), aBorderLeft);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderLeft(const nsAReadableString& aBorderLeft)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-left"), aBorderLeft, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderTopColor(nsAWritableString& aBorderTopColor)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-top-color"), aBorderTopColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderTopColor(const nsAReadableString& aBorderTopColor)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-top-color"), aBorderTopColor, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderRightColor(nsAWritableString& aBorderRightColor)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-right-color"), aBorderRightColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderRightColor(const nsAReadableString& aBorderRightColor)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-right-color"), aBorderRightColor, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderBottomColor(nsAWritableString& aBorderBottomColor)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-bottom-color"), aBorderBottomColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderBottomColor(const nsAReadableString& aBorderBottomColor)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-bottom-color"), aBorderBottomColor, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderLeftColor(nsAWritableString& aBorderLeftColor)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-left-color"), aBorderLeftColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderLeftColor(const nsAReadableString& aBorderLeftColor)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-left-color"), aBorderLeftColor, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderTopStyle(nsAWritableString& aBorderTopStyle)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-top-style"), aBorderTopStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderTopStyle(const nsAReadableString& aBorderTopStyle)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-top-style"), aBorderTopStyle, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderRightStyle(nsAWritableString& aBorderRightStyle)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-right-style"), aBorderRightStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderRightStyle(const nsAReadableString& aBorderRightStyle)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-right-style"), aBorderRightStyle, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderBottomStyle(nsAWritableString& aBorderBottomStyle)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-bottom-style"), aBorderBottomStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderBottomStyle(const nsAReadableString& aBorderBottomStyle)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-bottom-style"), aBorderBottomStyle, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderLeftStyle(nsAWritableString& aBorderLeftStyle)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-left-style"), aBorderLeftStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderLeftStyle(const nsAReadableString& aBorderLeftStyle)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-left-style"), aBorderLeftStyle, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderTopWidth(nsAWritableString& aBorderTopWidth)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-top-width"), aBorderTopWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderTopWidth(const nsAReadableString& aBorderTopWidth)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-top-width"), aBorderTopWidth, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderRightWidth(nsAWritableString& aBorderRightWidth)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-right-width"), aBorderRightWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderRightWidth(const nsAReadableString& aBorderRightWidth)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-right-width"), aBorderRightWidth, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderBottomWidth(nsAWritableString& aBorderBottomWidth)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-bottom-width"), aBorderBottomWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderBottomWidth(const nsAReadableString& aBorderBottomWidth)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-bottom-width"), aBorderBottomWidth, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderLeftWidth(nsAWritableString& aBorderLeftWidth)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-left-width"), aBorderLeftWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderLeftWidth(const nsAReadableString& aBorderLeftWidth)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-left-width"), aBorderLeftWidth, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderWidth(nsAWritableString& aBorderWidth)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-width"), aBorderWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderWidth(const nsAReadableString& aBorderWidth)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-width"), aBorderWidth, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBottom(nsAWritableString& aBottom)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("bottom"), aBottom);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBottom(const nsAReadableString& aBottom)
{
  return SetProperty(NS_ConvertASCIItoUCS2("bottom"), aBottom, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCaptionSide(nsAWritableString& aCaptionSide)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("caption-side"), aCaptionSide);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCaptionSide(const nsAReadableString& aCaptionSide)
{
  return SetProperty(NS_ConvertASCIItoUCS2("caption-side"), aCaptionSide, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetClear(nsAWritableString& aClear)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("clear"), aClear);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetClear(const nsAReadableString& aClear)
{
  return SetProperty(NS_ConvertASCIItoUCS2("clear"), aClear, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetClip(nsAWritableString& aClip)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("clip"), aClip);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetClip(const nsAReadableString& aClip)
{
  return SetProperty(NS_ConvertASCIItoUCS2("clip"), aClip, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetColor(nsAWritableString& aColor)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("color"), aColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetColor(const nsAReadableString& aColor)
{
  return SetProperty(NS_ConvertASCIItoUCS2("color"), aColor, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetContent(nsAWritableString& aContent)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("content"), aContent);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetContent(const nsAReadableString& aContent)
{
  return SetProperty(NS_ConvertASCIItoUCS2("content"), aContent, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCounterIncrement(nsAWritableString& aCounterIncrement)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("counter-increment"), aCounterIncrement);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCounterIncrement(const nsAReadableString& aCounterIncrement)
{
  return SetProperty(NS_ConvertASCIItoUCS2("counter-increment"), aCounterIncrement, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCounterReset(nsAWritableString& aCounterReset)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("counter-reset"), aCounterReset);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCounterReset(const nsAReadableString& aCounterReset)
{
  return SetProperty(NS_ConvertASCIItoUCS2("counter-reset"), aCounterReset, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCssFloat(nsAWritableString& aCssFloat)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("float"), aCssFloat);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCssFloat(const nsAReadableString& aCssFloat)
{
  return SetProperty(NS_ConvertASCIItoUCS2("float"), aCssFloat, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCue(nsAWritableString& aCue)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("cue"), aCue);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCue(const nsAReadableString& aCue)
{
  return SetProperty(NS_ConvertASCIItoUCS2("cue"), aCue, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCueAfter(nsAWritableString& aCueAfter)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("cue-after"), aCueAfter);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCueAfter(const nsAReadableString& aCueAfter)
{
  return SetProperty(NS_ConvertASCIItoUCS2("cue-after"), aCueAfter, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCueBefore(nsAWritableString& aCueBefore)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("cue-before"), aCueBefore);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCueBefore(const nsAReadableString& aCueBefore)
{
  return SetProperty(NS_ConvertASCIItoUCS2("cue-before"), aCueBefore, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCursor(nsAWritableString& aCursor)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("cursor"), aCursor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCursor(const nsAReadableString& aCursor)
{
  return SetProperty(NS_ConvertASCIItoUCS2("cursor"), aCursor, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetDirection(nsAWritableString& aDirection)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("direction"), aDirection);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetDirection(const nsAReadableString& aDirection)
{
  return SetProperty(NS_ConvertASCIItoUCS2("direction"), aDirection, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetDisplay(nsAWritableString& aDisplay)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("display"), aDisplay);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetDisplay(const nsAReadableString& aDisplay)
{
  return SetProperty(NS_ConvertASCIItoUCS2("display"), aDisplay, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetElevation(nsAWritableString& aElevation)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("elevation"), aElevation);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetElevation(const nsAReadableString& aElevation)
{
  return SetProperty(NS_ConvertASCIItoUCS2("elevation"), aElevation, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetEmptyCells(nsAWritableString& aEmptyCells)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("empty-cells"), aEmptyCells);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetEmptyCells(const nsAReadableString& aEmptyCells)
{
  return SetProperty(NS_ConvertASCIItoUCS2("empty-cells"), aEmptyCells, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFont(nsAWritableString& aFont)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("font"), aFont);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFont(const nsAReadableString& aFont)
{
  return SetProperty(NS_ConvertASCIItoUCS2("font"), aFont, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFontFamily(nsAWritableString& aFontFamily)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("font-family"), aFontFamily);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFontFamily(const nsAReadableString& aFontFamily)
{
  return SetProperty(NS_ConvertASCIItoUCS2("font-family"), aFontFamily, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFontSize(nsAWritableString& aFontSize)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("font-size"), aFontSize);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFontSize(const nsAReadableString& aFontSize)
{
  return SetProperty(NS_ConvertASCIItoUCS2("font-size"), aFontSize, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFontSizeAdjust(nsAWritableString& aFontSizeAdjust)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("font-size-adjust"), aFontSizeAdjust);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFontSizeAdjust(const nsAReadableString& aFontSizeAdjust)
{
  return SetProperty(NS_ConvertASCIItoUCS2("font-size-adjust"), aFontSizeAdjust, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFontStretch(nsAWritableString& aFontStretch)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("font-stretch"), aFontStretch);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFontStretch(const nsAReadableString& aFontStretch)
{
  return SetProperty(NS_ConvertASCIItoUCS2("font-stretch"), aFontStretch, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFontStyle(nsAWritableString& aFontStyle)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("font-style"), aFontStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFontStyle(const nsAReadableString& aFontStyle)
{
  return SetProperty(NS_ConvertASCIItoUCS2("font-style"), aFontStyle, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFontVariant(nsAWritableString& aFontVariant)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("font-variant"), aFontVariant);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFontVariant(const nsAReadableString& aFontVariant)
{
  return SetProperty(NS_ConvertASCIItoUCS2("font-variant"), aFontVariant, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFontWeight(nsAWritableString& aFontWeight)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("font-weight"), aFontWeight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFontWeight(const nsAReadableString& aFontWeight)
{
  return SetProperty(NS_ConvertASCIItoUCS2("font-weight"), aFontWeight, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetHeight(nsAWritableString& aHeight)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("height"), aHeight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetHeight(const nsAReadableString& aHeight)
{
  return SetProperty(NS_ConvertASCIItoUCS2("height"), aHeight, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetLeft(nsAWritableString& aLeft)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("left"), aLeft);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetLeft(const nsAReadableString& aLeft)
{
  return SetProperty(NS_ConvertASCIItoUCS2("left"), aLeft, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetLetterSpacing(nsAWritableString& aLetterSpacing)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("letter-spacing"), aLetterSpacing);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetLetterSpacing(const nsAReadableString& aLetterSpacing)
{
  return SetProperty(NS_ConvertASCIItoUCS2("letter-spacing"), aLetterSpacing, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetLineHeight(nsAWritableString& aLineHeight)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("line-height"), aLineHeight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetLineHeight(const nsAReadableString& aLineHeight)
{
  return SetProperty(NS_ConvertASCIItoUCS2("line-height"), aLineHeight, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetListStyle(nsAWritableString& aListStyle)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("list-style"), aListStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetListStyle(const nsAReadableString& aListStyle)
{
  return SetProperty(NS_ConvertASCIItoUCS2("list-style"), aListStyle, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetListStyleImage(nsAWritableString& aListStyleImage)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("list-style-image"), aListStyleImage);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetListStyleImage(const nsAReadableString& aListStyleImage)
{
  return SetProperty(NS_ConvertASCIItoUCS2("list-style-image"), aListStyleImage, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetListStylePosition(nsAWritableString& aListStylePosition)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("list-style-position"), aListStylePosition);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetListStylePosition(const nsAReadableString& aListStylePosition)
{
  return SetProperty(NS_ConvertASCIItoUCS2("list-style-position"), aListStylePosition, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetListStyleType(nsAWritableString& aListStyleType)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("list-style-type"), aListStyleType);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetListStyleType(const nsAReadableString& aListStyleType)
{
  return SetProperty(NS_ConvertASCIItoUCS2("list-style-type"), aListStyleType, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMargin(nsAWritableString& aMargin)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("margin"), aMargin);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMargin(const nsAReadableString& aMargin)
{
  return SetProperty(NS_ConvertASCIItoUCS2("margin"), aMargin, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMarginTop(nsAWritableString& aMarginTop)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("margin-top"), aMarginTop);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMarginTop(const nsAReadableString& aMarginTop)
{
  return SetProperty(NS_ConvertASCIItoUCS2("margin-top"), aMarginTop, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMarginRight(nsAWritableString& aMarginRight)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("margin-right"), aMarginRight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMarginRight(const nsAReadableString& aMarginRight)
{
  return SetProperty(NS_ConvertASCIItoUCS2("margin-right"), aMarginRight, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMarginBottom(nsAWritableString& aMarginBottom)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("margin-bottom"), aMarginBottom);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMarginBottom(const nsAReadableString& aMarginBottom)
{
  return SetProperty(NS_ConvertASCIItoUCS2("margin-bottom"), aMarginBottom, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMarginLeft(nsAWritableString& aMarginLeft)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("margin-left"), aMarginLeft);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMarginLeft(const nsAReadableString& aMarginLeft)
{
  return SetProperty(NS_ConvertASCIItoUCS2("margin-left"), aMarginLeft, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMarkerOffset(nsAWritableString& aMarkerOffset)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("marker-offset"), aMarkerOffset);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMarkerOffset(const nsAReadableString& aMarkerOffset)
{
  return SetProperty(NS_ConvertASCIItoUCS2("marker-offset"), aMarkerOffset, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMarks(nsAWritableString& aMarks)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("marks"), aMarks);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMarks(const nsAReadableString& aMarks)
{
  return SetProperty(NS_ConvertASCIItoUCS2("marks"), aMarks, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMaxHeight(nsAWritableString& aMaxHeight)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("max-height"), aMaxHeight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMaxHeight(const nsAReadableString& aMaxHeight)
{
  return SetProperty(NS_ConvertASCIItoUCS2("max-height"), aMaxHeight, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMaxWidth(nsAWritableString& aMaxWidth)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("max-width"), aMaxWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMaxWidth(const nsAReadableString& aMaxWidth)
{
  return SetProperty(NS_ConvertASCIItoUCS2("max-width"), aMaxWidth, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMinHeight(nsAWritableString& aMinHeight)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("min-height"), aMinHeight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMinHeight(const nsAReadableString& aMinHeight)
{
  return SetProperty(NS_ConvertASCIItoUCS2("min-height"), aMinHeight, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMinWidth(nsAWritableString& aMinWidth)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("min-width"), aMinWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMinWidth(const nsAReadableString& aMinWidth)
{
  return SetProperty(NS_ConvertASCIItoUCS2("min-width"), aMinWidth, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetOrphans(nsAWritableString& aOrphans)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("orphans"), aOrphans);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetOrphans(const nsAReadableString& aOrphans)
{
  return SetProperty(NS_ConvertASCIItoUCS2("orphans"), aOrphans, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetOutline(nsAWritableString& aOutline)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("outline"), aOutline);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetOutline(const nsAReadableString& aOutline)
{
  return SetProperty(NS_ConvertASCIItoUCS2("outline"), aOutline, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetOutlineColor(nsAWritableString& aOutlineColor)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("outline-color"), aOutlineColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetOutlineColor(const nsAReadableString& aOutlineColor)
{
  return SetProperty(NS_ConvertASCIItoUCS2("outline-color"), aOutlineColor, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetOutlineStyle(nsAWritableString& aOutlineStyle)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("outline-style"), aOutlineStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetOutlineStyle(const nsAReadableString& aOutlineStyle)
{
  return SetProperty(NS_ConvertASCIItoUCS2("outline-style"), aOutlineStyle, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetOutlineWidth(nsAWritableString& aOutlineWidth)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("outline-width"), aOutlineWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetOutlineWidth(const nsAReadableString& aOutlineWidth)
{
  return SetProperty(NS_ConvertASCIItoUCS2("outline-width"), aOutlineWidth, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetOverflow(nsAWritableString& aOverflow)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("overflow"), aOverflow);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetOverflow(const nsAReadableString& aOverflow)
{
  return SetProperty(NS_ConvertASCIItoUCS2("overflow"), aOverflow, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPadding(nsAWritableString& aPadding)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("padding"), aPadding);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPadding(const nsAReadableString& aPadding)
{
  return SetProperty(NS_ConvertASCIItoUCS2("padding"), aPadding, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPaddingTop(nsAWritableString& aPaddingTop)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("padding-top"), aPaddingTop);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPaddingTop(const nsAReadableString& aPaddingTop)
{
  return SetProperty(NS_ConvertASCIItoUCS2("padding-top"), aPaddingTop, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPaddingRight(nsAWritableString& aPaddingRight)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("padding-right"), aPaddingRight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPaddingRight(const nsAReadableString& aPaddingRight)
{
  return SetProperty(NS_ConvertASCIItoUCS2("padding-right"), aPaddingRight, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPaddingBottom(nsAWritableString& aPaddingBottom)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("padding-bottom"), aPaddingBottom);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPaddingBottom(const nsAReadableString& aPaddingBottom)
{
  return SetProperty(NS_ConvertASCIItoUCS2("padding-bottom"), aPaddingBottom, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPaddingLeft(nsAWritableString& aPaddingLeft)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("padding-left"), aPaddingLeft);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPaddingLeft(const nsAReadableString& aPaddingLeft)
{
  return SetProperty(NS_ConvertASCIItoUCS2("padding-left"), aPaddingLeft, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPage(nsAWritableString& aPage)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("page"), aPage);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPage(const nsAReadableString& aPage)
{
  return SetProperty(NS_ConvertASCIItoUCS2("page"), aPage, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPageBreakAfter(nsAWritableString& aPageBreakAfter)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("page-break-after"), aPageBreakAfter);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPageBreakAfter(const nsAReadableString& aPageBreakAfter)
{
  return SetProperty(NS_ConvertASCIItoUCS2("page-break-after"), aPageBreakAfter, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPageBreakBefore(nsAWritableString& aPageBreakBefore)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("page-break-before"), aPageBreakBefore);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPageBreakBefore(const nsAReadableString& aPageBreakBefore)
{
  return SetProperty(NS_ConvertASCIItoUCS2("page-break-before"), aPageBreakBefore, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPageBreakInside(nsAWritableString& aPageBreakInside)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("page-break-inside"), aPageBreakInside);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPageBreakInside(const nsAReadableString& aPageBreakInside)
{
  return SetProperty(NS_ConvertASCIItoUCS2("page-break-inside"), aPageBreakInside, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPause(nsAWritableString& aPause)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("pause"), aPause);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPause(const nsAReadableString& aPause)
{
  return SetProperty(NS_ConvertASCIItoUCS2("pause"), aPause, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPauseAfter(nsAWritableString& aPauseAfter)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("pause-after"), aPauseAfter);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPauseAfter(const nsAReadableString& aPauseAfter)
{
  return SetProperty(NS_ConvertASCIItoUCS2("pause-after"), aPauseAfter, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPauseBefore(nsAWritableString& aPauseBefore)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("pause-before"), aPauseBefore);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPauseBefore(const nsAReadableString& aPauseBefore)
{
  return SetProperty(NS_ConvertASCIItoUCS2("pause-before"), aPauseBefore, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPitch(nsAWritableString& aPitch)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("pitch"), aPitch);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPitch(const nsAReadableString& aPitch)
{
  return SetProperty(NS_ConvertASCIItoUCS2("pitch"), aPitch, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPitchRange(nsAWritableString& aPitchRange)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("pitch-range"), aPitchRange);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPitchRange(const nsAReadableString& aPitchRange)
{
  return SetProperty(NS_ConvertASCIItoUCS2("pitch-range"), aPitchRange, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPlayDuring(nsAWritableString& aPlayDuring)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("play-during"), aPlayDuring);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPlayDuring(const nsAReadableString& aPlayDuring)
{
  return SetProperty(NS_ConvertASCIItoUCS2("play-during"), aPlayDuring, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPosition(nsAWritableString& aPosition)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("position"), aPosition);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPosition(const nsAReadableString& aPosition)
{
  return SetProperty(NS_ConvertASCIItoUCS2("position"), aPosition, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetQuotes(nsAWritableString& aQuotes)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("quotes"), aQuotes);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetQuotes(const nsAReadableString& aQuotes)
{
  return SetProperty(NS_ConvertASCIItoUCS2("quotes"), aQuotes, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetRichness(nsAWritableString& aRichness)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("richness"), aRichness);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetRichness(const nsAReadableString& aRichness)
{
  return SetProperty(NS_ConvertASCIItoUCS2("richness"), aRichness, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetRight(nsAWritableString& aRight)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("right"), aRight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetRight(const nsAReadableString& aRight)
{
  return SetProperty(NS_ConvertASCIItoUCS2("right"), aRight, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetSize(nsAWritableString& aSize)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("size"), aSize);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetSize(const nsAReadableString& aSize)
{
  return SetProperty(NS_ConvertASCIItoUCS2("size"), aSize, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetSpeak(nsAWritableString& aSpeak)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("speak"), aSpeak);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetSpeak(const nsAReadableString& aSpeak)
{
  return SetProperty(NS_ConvertASCIItoUCS2("speak"), aSpeak, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetSpeakHeader(nsAWritableString& aSpeakHeader)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("speak-header"), aSpeakHeader);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetSpeakHeader(const nsAReadableString& aSpeakHeader)
{
  return SetProperty(NS_ConvertASCIItoUCS2("speak-header"), aSpeakHeader, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetSpeakNumeral(nsAWritableString& aSpeakNumeral)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("speak-numeral"), aSpeakNumeral);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetSpeakNumeral(const nsAReadableString& aSpeakNumeral)
{
  return SetProperty(NS_ConvertASCIItoUCS2("speak-numeral"), aSpeakNumeral, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetSpeakPunctuation(nsAWritableString& aSpeakPunctuation)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("speak-punctuation"), aSpeakPunctuation);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetSpeakPunctuation(const nsAReadableString& aSpeakPunctuation)
{
  return SetProperty(NS_ConvertASCIItoUCS2("speak-punctuation"), aSpeakPunctuation, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetSpeechRate(nsAWritableString& aSpeechRate)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("speech-rate"), aSpeechRate);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetSpeechRate(const nsAReadableString& aSpeechRate)
{
  return SetProperty(NS_ConvertASCIItoUCS2("speech-rate"), aSpeechRate, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetStress(nsAWritableString& aStress)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("stress"), aStress);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetStress(const nsAReadableString& aStress)
{
  return SetProperty(NS_ConvertASCIItoUCS2("stress"), aStress, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetTableLayout(nsAWritableString& aTableLayout)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("table-layout"), aTableLayout);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetTableLayout(const nsAReadableString& aTableLayout)
{
  return SetProperty(NS_ConvertASCIItoUCS2("table-layout"), aTableLayout, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetTextAlign(nsAWritableString& aTextAlign)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("text-align"), aTextAlign);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetTextAlign(const nsAReadableString& aTextAlign)
{
  return SetProperty(NS_ConvertASCIItoUCS2("text-align"), aTextAlign, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetTextDecoration(nsAWritableString& aTextDecoration)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("text-decoration"), aTextDecoration);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetTextDecoration(const nsAReadableString& aTextDecoration)
{
  return SetProperty(NS_ConvertASCIItoUCS2("text-decoration"), aTextDecoration, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetTextIndent(nsAWritableString& aTextIndent)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("text-indent"), aTextIndent);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetTextIndent(const nsAReadableString& aTextIndent)
{
  return SetProperty(NS_ConvertASCIItoUCS2("text-indent"), aTextIndent, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetTextShadow(nsAWritableString& aTextShadow)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("text-shadow"), aTextShadow);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetTextShadow(const nsAReadableString& aTextShadow)
{
  return SetProperty(NS_ConvertASCIItoUCS2("text-shadow"), aTextShadow, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetTextTransform(nsAWritableString& aTextTransform)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("text-transform"), aTextTransform);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetTextTransform(const nsAReadableString& aTextTransform)
{
  return SetProperty(NS_ConvertASCIItoUCS2("text-transform"), aTextTransform, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetTop(nsAWritableString& aTop)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("top"), aTop);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetTop(const nsAReadableString& aTop)
{
  return SetProperty(NS_ConvertASCIItoUCS2("top"), aTop, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetUnicodeBidi(nsAWritableString& aUnicodeBidi)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("unicode-bidi"), aUnicodeBidi);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetUnicodeBidi(const nsAReadableString& aUnicodeBidi)
{
  return SetProperty(NS_ConvertASCIItoUCS2("unicode-bidi"), aUnicodeBidi, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetVerticalAlign(nsAWritableString& aVerticalAlign)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("vertical-align"), aVerticalAlign);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetVerticalAlign(const nsAReadableString& aVerticalAlign)
{
  return SetProperty(NS_ConvertASCIItoUCS2("vertical-align"), aVerticalAlign, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetVisibility(nsAWritableString& aVisibility)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("visibility"), aVisibility);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetVisibility(const nsAReadableString& aVisibility)
{
  return SetProperty(NS_ConvertASCIItoUCS2("visibility"), aVisibility, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetVoiceFamily(nsAWritableString& aVoiceFamily)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("voice-family"), aVoiceFamily);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetVoiceFamily(const nsAReadableString& aVoiceFamily)
{
  return SetProperty(NS_ConvertASCIItoUCS2("voice-family"), aVoiceFamily, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetVolume(nsAWritableString& aVolume)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("volume"), aVolume);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetVolume(const nsAReadableString& aVolume)
{
  return SetProperty(NS_ConvertASCIItoUCS2("volume"), aVolume, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetWhiteSpace(nsAWritableString& aWhiteSpace)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("white-space"), aWhiteSpace);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetWhiteSpace(const nsAReadableString& aWhiteSpace)
{
  return SetProperty(NS_ConvertASCIItoUCS2("white-space"), aWhiteSpace, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetWidows(nsAWritableString& aWidows)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("widows"), aWidows);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetWidows(const nsAReadableString& aWidows)
{
  return SetProperty(NS_ConvertASCIItoUCS2("widows"), aWidows, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetWidth(nsAWritableString& aWidth)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("width"), aWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetWidth(const nsAReadableString& aWidth)
{
  return SetProperty(NS_ConvertASCIItoUCS2("width"), aWidth, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetWordSpacing(nsAWritableString& aWordSpacing)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("word-spacing"), aWordSpacing);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetWordSpacing(const nsAReadableString& aWordSpacing)
{
  return SetProperty(NS_ConvertASCIItoUCS2("word-spacing"), aWordSpacing, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetZIndex(nsAWritableString& aZIndex)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("z-index"), aZIndex);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetZIndex(const nsAReadableString& aZIndex)
{
  return SetProperty(NS_ConvertASCIItoUCS2("z-index"), aZIndex, nsAutoString());
}


NS_IMETHODIMP 
nsDOMCSSDeclaration::GetOpacity(nsAWritableString& aOpacity)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("opacity"), aOpacity);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetOpacity(const nsAReadableString& aOpacity)
{
  return SetProperty(NS_ConvertASCIItoUCS2("opacity"), aOpacity, nsAutoString());
}

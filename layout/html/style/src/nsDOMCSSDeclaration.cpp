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
nsDOMCSSDeclaration::GetCssText(nsString& aCssText)
{
  aCssText.Truncate();
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
nsDOMCSSDeclaration::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  nsCOMPtr<nsISupports> parent;

  GetParent(getter_AddRefs(parent));

  if (parent) {
    parent->QueryInterface(NS_GET_IID(nsIDOMCSSRule), (void **)aParentRule);
  } else {
    NS_ENSURE_ARG_POINTER(aParentRule);
    *aParentRule = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPropertyCSSValue(const nsString& aPropertyName,
                                         nsIDOMCSSValue** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  // We don't support CSSValue yet so we'll just return null...
  *aReturn = nsnull;

  return NS_OK;
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
    aReturn.AssignWithConversion("!important");    
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
  if (!aValue.Length()) {
     // If the new value of the property is an empty string we remove the
     // property.
    nsAutoString tmp;
    return RemoveProperty(aPropertyName, tmp);
  }

  nsAutoString declString;

  declString.Assign(aPropertyName);
  declString.AppendWithConversion(":");
  declString.Append(aValue);
  declString.Append(aPriority);

  return ParseDeclaration(declString);
}

NS_IMETHODIMP 
nsDOMCSSDeclaration::GetAzimuth(nsString& aAzimuth)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("azimuth"), aAzimuth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetAzimuth(const nsString& aAzimuth)
{
  return SetProperty(NS_ConvertASCIItoUCS2("azimuth"), aAzimuth, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBackground(nsString& aBackground)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("background"), aBackground);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBackground(const nsString& aBackground)
{
  return SetProperty(NS_ConvertASCIItoUCS2("background"), aBackground, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBackgroundAttachment(nsString& aBackgroundAttachment)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("background-attachment"), aBackgroundAttachment);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBackgroundAttachment(const nsString& aBackgroundAttachment)
{
  return SetProperty(NS_ConvertASCIItoUCS2("background-attachment"), aBackgroundAttachment, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBackgroundColor(nsString& aBackgroundColor)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("background-color"), aBackgroundColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBackgroundColor(const nsString& aBackgroundColor)
{
  return SetProperty(NS_ConvertASCIItoUCS2("background-color"), aBackgroundColor, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBackgroundImage(nsString& aBackgroundImage)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("background-image"), aBackgroundImage);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBackgroundImage(const nsString& aBackgroundImage)
{
  return SetProperty(NS_ConvertASCIItoUCS2("background-image"), aBackgroundImage, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBackgroundPosition(nsString& aBackgroundPosition)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("background-position"), aBackgroundPosition);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBackgroundPosition(const nsString& aBackgroundPosition)
{
  return SetProperty(NS_ConvertASCIItoUCS2("background-position"), aBackgroundPosition, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBackgroundRepeat(nsString& aBackgroundRepeat)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("background-repeat"), aBackgroundRepeat);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBackgroundRepeat(const nsString& aBackgroundRepeat)
{
  return SetProperty(NS_ConvertASCIItoUCS2("background-repeat"), aBackgroundRepeat, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBehavior(nsString& aBehavior)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("behavior"), aBehavior);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBehavior(const nsString& aBehavior)
{
  return SetProperty(NS_ConvertASCIItoUCS2("behavior"), aBehavior, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorder(nsString& aBorder)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border"), aBorder);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorder(const nsString& aBorder)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border"), aBorder, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderCollapse(nsString& aBorderCollapse)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-collapse"), aBorderCollapse);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderCollapse(const nsString& aBorderCollapse)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-collapse"), aBorderCollapse, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderColor(nsString& aBorderColor)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-color"), aBorderColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderColor(const nsString& aBorderColor)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-color"), aBorderColor, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderSpacing(nsString& aBorderSpacing)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-spacing"), aBorderSpacing);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderSpacing(const nsString& aBorderSpacing)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-spacing"), aBorderSpacing, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderStyle(nsString& aBorderStyle)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-style"), aBorderStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderStyle(const nsString& aBorderStyle)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-style"), aBorderStyle, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderTop(nsString& aBorderTop)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-top"), aBorderTop);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderTop(const nsString& aBorderTop)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-top"), aBorderTop, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderRight(nsString& aBorderRight)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-right"), aBorderRight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderRight(const nsString& aBorderRight)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-right"), aBorderRight, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderBottom(nsString& aBorderBottom)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-bottom"), aBorderBottom);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderBottom(const nsString& aBorderBottom)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-bottom"), aBorderBottom, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderLeft(nsString& aBorderLeft)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-left"), aBorderLeft);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderLeft(const nsString& aBorderLeft)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-left"), aBorderLeft, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderTopColor(nsString& aBorderTopColor)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-top-color"), aBorderTopColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderTopColor(const nsString& aBorderTopColor)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-top-color"), aBorderTopColor, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderRightColor(nsString& aBorderRightColor)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-right-color"), aBorderRightColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderRightColor(const nsString& aBorderRightColor)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-right-color"), aBorderRightColor, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderBottomColor(nsString& aBorderBottomColor)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-bottom-color"), aBorderBottomColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderBottomColor(const nsString& aBorderBottomColor)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-bottom-color"), aBorderBottomColor, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderLeftColor(nsString& aBorderLeftColor)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-left-color"), aBorderLeftColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderLeftColor(const nsString& aBorderLeftColor)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-left-color"), aBorderLeftColor, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderTopStyle(nsString& aBorderTopStyle)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-top-style"), aBorderTopStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderTopStyle(const nsString& aBorderTopStyle)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-top-style"), aBorderTopStyle, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderRightStyle(nsString& aBorderRightStyle)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-right-style"), aBorderRightStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderRightStyle(const nsString& aBorderRightStyle)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-right-style"), aBorderRightStyle, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderBottomStyle(nsString& aBorderBottomStyle)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-bottom-style"), aBorderBottomStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderBottomStyle(const nsString& aBorderBottomStyle)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-bottom-style"), aBorderBottomStyle, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderLeftStyle(nsString& aBorderLeftStyle)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-left-style"), aBorderLeftStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderLeftStyle(const nsString& aBorderLeftStyle)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-left-style"), aBorderLeftStyle, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderTopWidth(nsString& aBorderTopWidth)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-top-width"), aBorderTopWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderTopWidth(const nsString& aBorderTopWidth)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-top-width"), aBorderTopWidth, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderRightWidth(nsString& aBorderRightWidth)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-right-width"), aBorderRightWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderRightWidth(const nsString& aBorderRightWidth)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-right-width"), aBorderRightWidth, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderBottomWidth(nsString& aBorderBottomWidth)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-bottom-width"), aBorderBottomWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderBottomWidth(const nsString& aBorderBottomWidth)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-bottom-width"), aBorderBottomWidth, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderLeftWidth(nsString& aBorderLeftWidth)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-left-width"), aBorderLeftWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderLeftWidth(const nsString& aBorderLeftWidth)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-left-width"), aBorderLeftWidth, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBorderWidth(nsString& aBorderWidth)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("border-width"), aBorderWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBorderWidth(const nsString& aBorderWidth)
{
  return SetProperty(NS_ConvertASCIItoUCS2("border-width"), aBorderWidth, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetBottom(nsString& aBottom)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("bottom"), aBottom);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetBottom(const nsString& aBottom)
{
  return SetProperty(NS_ConvertASCIItoUCS2("bottom"), aBottom, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCaptionSide(nsString& aCaptionSide)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("caption-side"), aCaptionSide);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCaptionSide(const nsString& aCaptionSide)
{
  return SetProperty(NS_ConvertASCIItoUCS2("caption-side"), aCaptionSide, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetClear(nsString& aClear)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("clear"), aClear);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetClear(const nsString& aClear)
{
  return SetProperty(NS_ConvertASCIItoUCS2("clear"), aClear, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetClip(nsString& aClip)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("clip"), aClip);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetClip(const nsString& aClip)
{
  return SetProperty(NS_ConvertASCIItoUCS2("clip"), aClip, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetColor(nsString& aColor)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("color"), aColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetColor(const nsString& aColor)
{
  return SetProperty(NS_ConvertASCIItoUCS2("color"), aColor, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetContent(nsString& aContent)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("content"), aContent);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetContent(const nsString& aContent)
{
  return SetProperty(NS_ConvertASCIItoUCS2("content"), aContent, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCounterIncrement(nsString& aCounterIncrement)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("counter-increment"), aCounterIncrement);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCounterIncrement(const nsString& aCounterIncrement)
{
  return SetProperty(NS_ConvertASCIItoUCS2("counter-increment"), aCounterIncrement, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCounterReset(nsString& aCounterReset)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("counter-reset"), aCounterReset);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCounterReset(const nsString& aCounterReset)
{
  return SetProperty(NS_ConvertASCIItoUCS2("counter-reset"), aCounterReset, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCssFloat(nsString& aCssFloat)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("float"), aCssFloat);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCssFloat(const nsString& aCssFloat)
{
  return SetProperty(NS_ConvertASCIItoUCS2("float"), aCssFloat, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCue(nsString& aCue)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("cue"), aCue);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCue(const nsString& aCue)
{
  return SetProperty(NS_ConvertASCIItoUCS2("cue"), aCue, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCueAfter(nsString& aCueAfter)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("cue-after"), aCueAfter);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCueAfter(const nsString& aCueAfter)
{
  return SetProperty(NS_ConvertASCIItoUCS2("cue-after"), aCueAfter, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCueBefore(nsString& aCueBefore)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("cue-before"), aCueBefore);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCueBefore(const nsString& aCueBefore)
{
  return SetProperty(NS_ConvertASCIItoUCS2("cue-before"), aCueBefore, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetCursor(nsString& aCursor)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("cursor"), aCursor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetCursor(const nsString& aCursor)
{
  return SetProperty(NS_ConvertASCIItoUCS2("cursor"), aCursor, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetDirection(nsString& aDirection)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("direction"), aDirection);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetDirection(const nsString& aDirection)
{
  return SetProperty(NS_ConvertASCIItoUCS2("direction"), aDirection, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetDisplay(nsString& aDisplay)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("display"), aDisplay);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetDisplay(const nsString& aDisplay)
{
  return SetProperty(NS_ConvertASCIItoUCS2("display"), aDisplay, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetElevation(nsString& aElevation)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("elevation"), aElevation);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetElevation(const nsString& aElevation)
{
  return SetProperty(NS_ConvertASCIItoUCS2("elevation"), aElevation, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetEmptyCells(nsString& aEmptyCells)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("empty-cells"), aEmptyCells);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetEmptyCells(const nsString& aEmptyCells)
{
  return SetProperty(NS_ConvertASCIItoUCS2("empty-cells"), aEmptyCells, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFont(nsString& aFont)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("font"), aFont);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFont(const nsString& aFont)
{
  return SetProperty(NS_ConvertASCIItoUCS2("font"), aFont, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFontFamily(nsString& aFontFamily)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("font-family"), aFontFamily);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFontFamily(const nsString& aFontFamily)
{
  return SetProperty(NS_ConvertASCIItoUCS2("font-family"), aFontFamily, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFontSize(nsString& aFontSize)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("font-size"), aFontSize);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFontSize(const nsString& aFontSize)
{
  return SetProperty(NS_ConvertASCIItoUCS2("font-size"), aFontSize, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFontSizeAdjust(nsString& aFontSizeAdjust)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("font-size-adjust"), aFontSizeAdjust);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFontSizeAdjust(const nsString& aFontSizeAdjust)
{
  return SetProperty(NS_ConvertASCIItoUCS2("font-size-adjust"), aFontSizeAdjust, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFontStretch(nsString& aFontStretch)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("font-stretch"), aFontStretch);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFontStretch(const nsString& aFontStretch)
{
  return SetProperty(NS_ConvertASCIItoUCS2("font-stretch"), aFontStretch, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFontStyle(nsString& aFontStyle)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("font-style"), aFontStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFontStyle(const nsString& aFontStyle)
{
  return SetProperty(NS_ConvertASCIItoUCS2("font-style"), aFontStyle, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFontVariant(nsString& aFontVariant)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("font-variant"), aFontVariant);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFontVariant(const nsString& aFontVariant)
{
  return SetProperty(NS_ConvertASCIItoUCS2("font-variant"), aFontVariant, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetFontWeight(nsString& aFontWeight)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("font-weight"), aFontWeight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetFontWeight(const nsString& aFontWeight)
{
  return SetProperty(NS_ConvertASCIItoUCS2("font-weight"), aFontWeight, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetHeight(nsString& aHeight)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("height"), aHeight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetHeight(const nsString& aHeight)
{
  return SetProperty(NS_ConvertASCIItoUCS2("height"), aHeight, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetLeft(nsString& aLeft)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("left"), aLeft);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetLeft(const nsString& aLeft)
{
  return SetProperty(NS_ConvertASCIItoUCS2("left"), aLeft, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetLetterSpacing(nsString& aLetterSpacing)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("letter-spacing"), aLetterSpacing);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetLetterSpacing(const nsString& aLetterSpacing)
{
  return SetProperty(NS_ConvertASCIItoUCS2("letter-spacing"), aLetterSpacing, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetLineHeight(nsString& aLineHeight)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("line-height"), aLineHeight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetLineHeight(const nsString& aLineHeight)
{
  return SetProperty(NS_ConvertASCIItoUCS2("line-height"), aLineHeight, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetListStyle(nsString& aListStyle)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("list-style"), aListStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetListStyle(const nsString& aListStyle)
{
  return SetProperty(NS_ConvertASCIItoUCS2("list-style"), aListStyle, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetListStyleImage(nsString& aListStyleImage)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("list-style-image"), aListStyleImage);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetListStyleImage(const nsString& aListStyleImage)
{
  return SetProperty(NS_ConvertASCIItoUCS2("list-style-image"), aListStyleImage, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetListStylePosition(nsString& aListStylePosition)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("list-style-position"), aListStylePosition);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetListStylePosition(const nsString& aListStylePosition)
{
  return SetProperty(NS_ConvertASCIItoUCS2("list-style-position"), aListStylePosition, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetListStyleType(nsString& aListStyleType)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("list-style-type"), aListStyleType);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetListStyleType(const nsString& aListStyleType)
{
  return SetProperty(NS_ConvertASCIItoUCS2("list-style-type"), aListStyleType, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMargin(nsString& aMargin)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("margin"), aMargin);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMargin(const nsString& aMargin)
{
  return SetProperty(NS_ConvertASCIItoUCS2("margin"), aMargin, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMarginTop(nsString& aMarginTop)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("margin-top"), aMarginTop);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMarginTop(const nsString& aMarginTop)
{
  return SetProperty(NS_ConvertASCIItoUCS2("margin-top"), aMarginTop, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMarginRight(nsString& aMarginRight)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("margin-right"), aMarginRight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMarginRight(const nsString& aMarginRight)
{
  return SetProperty(NS_ConvertASCIItoUCS2("margin-right"), aMarginRight, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMarginBottom(nsString& aMarginBottom)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("margin-bottom"), aMarginBottom);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMarginBottom(const nsString& aMarginBottom)
{
  return SetProperty(NS_ConvertASCIItoUCS2("margin-bottom"), aMarginBottom, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMarginLeft(nsString& aMarginLeft)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("margin-left"), aMarginLeft);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMarginLeft(const nsString& aMarginLeft)
{
  return SetProperty(NS_ConvertASCIItoUCS2("margin-left"), aMarginLeft, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMarkerOffset(nsString& aMarkerOffset)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("marker-offset"), aMarkerOffset);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMarkerOffset(const nsString& aMarkerOffset)
{
  return SetProperty(NS_ConvertASCIItoUCS2("marker-offset"), aMarkerOffset, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMarks(nsString& aMarks)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("marks"), aMarks);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMarks(const nsString& aMarks)
{
  return SetProperty(NS_ConvertASCIItoUCS2("marks"), aMarks, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMaxHeight(nsString& aMaxHeight)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("max-height"), aMaxHeight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMaxHeight(const nsString& aMaxHeight)
{
  return SetProperty(NS_ConvertASCIItoUCS2("max-height"), aMaxHeight, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMaxWidth(nsString& aMaxWidth)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("max-width"), aMaxWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMaxWidth(const nsString& aMaxWidth)
{
  return SetProperty(NS_ConvertASCIItoUCS2("max-width"), aMaxWidth, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMinHeight(nsString& aMinHeight)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("min-height"), aMinHeight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMinHeight(const nsString& aMinHeight)
{
  return SetProperty(NS_ConvertASCIItoUCS2("min-height"), aMinHeight, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetMinWidth(nsString& aMinWidth)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("min-width"), aMinWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetMinWidth(const nsString& aMinWidth)
{
  return SetProperty(NS_ConvertASCIItoUCS2("min-width"), aMinWidth, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetOrphans(nsString& aOrphans)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("orphans"), aOrphans);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetOrphans(const nsString& aOrphans)
{
  return SetProperty(NS_ConvertASCIItoUCS2("orphans"), aOrphans, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetOutline(nsString& aOutline)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("outline"), aOutline);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetOutline(const nsString& aOutline)
{
  return SetProperty(NS_ConvertASCIItoUCS2("outline"), aOutline, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetOutlineColor(nsString& aOutlineColor)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("outline-color"), aOutlineColor);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetOutlineColor(const nsString& aOutlineColor)
{
  return SetProperty(NS_ConvertASCIItoUCS2("outline-color"), aOutlineColor, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetOutlineStyle(nsString& aOutlineStyle)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("outline-style"), aOutlineStyle);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetOutlineStyle(const nsString& aOutlineStyle)
{
  return SetProperty(NS_ConvertASCIItoUCS2("outline-style"), aOutlineStyle, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetOutlineWidth(nsString& aOutlineWidth)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("outline-width"), aOutlineWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetOutlineWidth(const nsString& aOutlineWidth)
{
  return SetProperty(NS_ConvertASCIItoUCS2("outline-width"), aOutlineWidth, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetOverflow(nsString& aOverflow)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("overflow"), aOverflow);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetOverflow(const nsString& aOverflow)
{
  return SetProperty(NS_ConvertASCIItoUCS2("overflow"), aOverflow, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPadding(nsString& aPadding)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("padding"), aPadding);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPadding(const nsString& aPadding)
{
  return SetProperty(NS_ConvertASCIItoUCS2("padding"), aPadding, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPaddingTop(nsString& aPaddingTop)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("padding-top"), aPaddingTop);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPaddingTop(const nsString& aPaddingTop)
{
  return SetProperty(NS_ConvertASCIItoUCS2("padding-top"), aPaddingTop, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPaddingRight(nsString& aPaddingRight)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("padding-right"), aPaddingRight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPaddingRight(const nsString& aPaddingRight)
{
  return SetProperty(NS_ConvertASCIItoUCS2("padding-right"), aPaddingRight, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPaddingBottom(nsString& aPaddingBottom)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("padding-bottom"), aPaddingBottom);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPaddingBottom(const nsString& aPaddingBottom)
{
  return SetProperty(NS_ConvertASCIItoUCS2("padding-bottom"), aPaddingBottom, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPaddingLeft(nsString& aPaddingLeft)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("padding-left"), aPaddingLeft);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPaddingLeft(const nsString& aPaddingLeft)
{
  return SetProperty(NS_ConvertASCIItoUCS2("padding-left"), aPaddingLeft, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPage(nsString& aPage)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("page"), aPage);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPage(const nsString& aPage)
{
  return SetProperty(NS_ConvertASCIItoUCS2("page"), aPage, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPageBreakAfter(nsString& aPageBreakAfter)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("page-break-after"), aPageBreakAfter);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPageBreakAfter(const nsString& aPageBreakAfter)
{
  return SetProperty(NS_ConvertASCIItoUCS2("page-break-after"), aPageBreakAfter, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPageBreakBefore(nsString& aPageBreakBefore)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("page-break-before"), aPageBreakBefore);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPageBreakBefore(const nsString& aPageBreakBefore)
{
  return SetProperty(NS_ConvertASCIItoUCS2("page-break-before"), aPageBreakBefore, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPageBreakInside(nsString& aPageBreakInside)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("page-break-inside"), aPageBreakInside);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPageBreakInside(const nsString& aPageBreakInside)
{
  return SetProperty(NS_ConvertASCIItoUCS2("page-break-inside"), aPageBreakInside, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPause(nsString& aPause)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("pause"), aPause);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPause(const nsString& aPause)
{
  return SetProperty(NS_ConvertASCIItoUCS2("pause"), aPause, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPauseAfter(nsString& aPauseAfter)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("pause-after"), aPauseAfter);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPauseAfter(const nsString& aPauseAfter)
{
  return SetProperty(NS_ConvertASCIItoUCS2("pause-after"), aPauseAfter, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPauseBefore(nsString& aPauseBefore)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("pause-before"), aPauseBefore);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPauseBefore(const nsString& aPauseBefore)
{
  return SetProperty(NS_ConvertASCIItoUCS2("pause-before"), aPauseBefore, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPitch(nsString& aPitch)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("pitch"), aPitch);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPitch(const nsString& aPitch)
{
  return SetProperty(NS_ConvertASCIItoUCS2("pitch"), aPitch, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPitchRange(nsString& aPitchRange)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("pitch-range"), aPitchRange);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPitchRange(const nsString& aPitchRange)
{
  return SetProperty(NS_ConvertASCIItoUCS2("pitch-range"), aPitchRange, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPlayDuring(nsString& aPlayDuring)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("play-during"), aPlayDuring);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPlayDuring(const nsString& aPlayDuring)
{
  return SetProperty(NS_ConvertASCIItoUCS2("play-during"), aPlayDuring, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetPosition(nsString& aPosition)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("position"), aPosition);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetPosition(const nsString& aPosition)
{
  return SetProperty(NS_ConvertASCIItoUCS2("position"), aPosition, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetQuotes(nsString& aQuotes)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("quotes"), aQuotes);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetQuotes(const nsString& aQuotes)
{
  return SetProperty(NS_ConvertASCIItoUCS2("quotes"), aQuotes, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetRichness(nsString& aRichness)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("richness"), aRichness);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetRichness(const nsString& aRichness)
{
  return SetProperty(NS_ConvertASCIItoUCS2("richness"), aRichness, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetRight(nsString& aRight)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("right"), aRight);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetRight(const nsString& aRight)
{
  return SetProperty(NS_ConvertASCIItoUCS2("right"), aRight, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetSize(nsString& aSize)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("size"), aSize);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetSize(const nsString& aSize)
{
  return SetProperty(NS_ConvertASCIItoUCS2("size"), aSize, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetSpeak(nsString& aSpeak)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("speak"), aSpeak);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetSpeak(const nsString& aSpeak)
{
  return SetProperty(NS_ConvertASCIItoUCS2("speak"), aSpeak, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetSpeakHeader(nsString& aSpeakHeader)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("speak-header"), aSpeakHeader);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetSpeakHeader(const nsString& aSpeakHeader)
{
  return SetProperty(NS_ConvertASCIItoUCS2("speak-header"), aSpeakHeader, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetSpeakNumeral(nsString& aSpeakNumeral)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("speak-numeral"), aSpeakNumeral);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetSpeakNumeral(const nsString& aSpeakNumeral)
{
  return SetProperty(NS_ConvertASCIItoUCS2("speak-numeral"), aSpeakNumeral, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetSpeakPunctuation(nsString& aSpeakPunctuation)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("speak-punctuation"), aSpeakPunctuation);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetSpeakPunctuation(const nsString& aSpeakPunctuation)
{
  return SetProperty(NS_ConvertASCIItoUCS2("speak-punctuation"), aSpeakPunctuation, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetSpeechRate(nsString& aSpeechRate)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("speech-rate"), aSpeechRate);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetSpeechRate(const nsString& aSpeechRate)
{
  return SetProperty(NS_ConvertASCIItoUCS2("speech-rate"), aSpeechRate, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetStress(nsString& aStress)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("stress"), aStress);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetStress(const nsString& aStress)
{
  return SetProperty(NS_ConvertASCIItoUCS2("stress"), aStress, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetTableLayout(nsString& aTableLayout)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("table-layout"), aTableLayout);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetTableLayout(const nsString& aTableLayout)
{
  return SetProperty(NS_ConvertASCIItoUCS2("table-layout"), aTableLayout, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetTextAlign(nsString& aTextAlign)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("text-align"), aTextAlign);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetTextAlign(const nsString& aTextAlign)
{
  return SetProperty(NS_ConvertASCIItoUCS2("text-align"), aTextAlign, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetTextDecoration(nsString& aTextDecoration)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("text-decoration"), aTextDecoration);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetTextDecoration(const nsString& aTextDecoration)
{
  return SetProperty(NS_ConvertASCIItoUCS2("text-decoration"), aTextDecoration, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetTextIndent(nsString& aTextIndent)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("text-indent"), aTextIndent);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetTextIndent(const nsString& aTextIndent)
{
  return SetProperty(NS_ConvertASCIItoUCS2("text-indent"), aTextIndent, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetTextShadow(nsString& aTextShadow)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("text-shadow"), aTextShadow);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetTextShadow(const nsString& aTextShadow)
{
  return SetProperty(NS_ConvertASCIItoUCS2("text-shadow"), aTextShadow, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetTextTransform(nsString& aTextTransform)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("text-transform"), aTextTransform);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetTextTransform(const nsString& aTextTransform)
{
  return SetProperty(NS_ConvertASCIItoUCS2("text-transform"), aTextTransform, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetTop(nsString& aTop)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("top"), aTop);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetTop(const nsString& aTop)
{
  return SetProperty(NS_ConvertASCIItoUCS2("top"), aTop, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetUnicodeBidi(nsString& aUnicodeBidi)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("unicode-bidi"), aUnicodeBidi);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetUnicodeBidi(const nsString& aUnicodeBidi)
{
  return SetProperty(NS_ConvertASCIItoUCS2("unicode-bidi"), aUnicodeBidi, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetVerticalAlign(nsString& aVerticalAlign)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("vertical-align"), aVerticalAlign);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetVerticalAlign(const nsString& aVerticalAlign)
{
  return SetProperty(NS_ConvertASCIItoUCS2("vertical-align"), aVerticalAlign, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetVisibility(nsString& aVisibility)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("visibility"), aVisibility);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetVisibility(const nsString& aVisibility)
{
  return SetProperty(NS_ConvertASCIItoUCS2("visibility"), aVisibility, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetVoiceFamily(nsString& aVoiceFamily)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("voice-family"), aVoiceFamily);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetVoiceFamily(const nsString& aVoiceFamily)
{
  return SetProperty(NS_ConvertASCIItoUCS2("voice-family"), aVoiceFamily, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetVolume(nsString& aVolume)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("volume"), aVolume);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetVolume(const nsString& aVolume)
{
  return SetProperty(NS_ConvertASCIItoUCS2("volume"), aVolume, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetWhiteSpace(nsString& aWhiteSpace)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("white-space"), aWhiteSpace);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetWhiteSpace(const nsString& aWhiteSpace)
{
  return SetProperty(NS_ConvertASCIItoUCS2("white-space"), aWhiteSpace, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetWidows(nsString& aWidows)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("widows"), aWidows);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetWidows(const nsString& aWidows)
{
  return SetProperty(NS_ConvertASCIItoUCS2("widows"), aWidows, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetWidth(nsString& aWidth)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("width"), aWidth);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetWidth(const nsString& aWidth)
{
  return SetProperty(NS_ConvertASCIItoUCS2("width"), aWidth, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetWordSpacing(nsString& aWordSpacing)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("word-spacing"), aWordSpacing);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetWordSpacing(const nsString& aWordSpacing)
{
  return SetProperty(NS_ConvertASCIItoUCS2("word-spacing"), aWordSpacing, nsAutoString());
}

NS_IMETHODIMP
nsDOMCSSDeclaration::GetZIndex(nsString& aZIndex)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("z-index"), aZIndex);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetZIndex(const nsString& aZIndex)
{
  return SetProperty(NS_ConvertASCIItoUCS2("z-index"), aZIndex, nsAutoString());
}


NS_IMETHODIMP 
nsDOMCSSDeclaration::GetOpacity(nsString& aOpacity)
{
  return GetPropertyValue(NS_ConvertASCIItoUCS2("opacity"), aOpacity);
}

NS_IMETHODIMP
nsDOMCSSDeclaration::SetOpacity(const nsString& aOpacity)
{
  return SetProperty(NS_ConvertASCIItoUCS2("opacity"), aOpacity, nsAutoString());
}

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

#include "nsDOMStyleDeclaration.h"
#include "nsGenericHTMLElement.h"
#include "nsICSSStyleRule.h"
#include "nsICSSParser.h"
#include "nsIStyleRule.h"
#include "nsICSSDeclaration.h"
#include "nsCSSProps.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLContent.h"
#include "nsIDocument.h"

nsDOMStyleDeclaration::nsDOMStyleDeclaration(nsIHTMLContent *aContent)
{
  NS_INIT_REFCNT();
  // This reference is not reference-counted. The content
  // object tells us when its about to go away.
  mContent = aContent;
  mScriptObject = nsnull;
}

nsDOMStyleDeclaration::~nsDOMStyleDeclaration()
{
}

NS_IMPL_ADDREF(nsDOMStyleDeclaration);
NS_IMPL_RELEASE(nsDOMStyleDeclaration);

static NS_DEFINE_IID(kIDOMCSSStyleDeclarationIID, NS_IDOMCSSSTYLEDECLARATION_IID);
static NS_DEFINE_IID(kICSSStyleRuleIID, NS_ICSS_STYLE_RULE_IID);

NS_IMETHODIMP
nsDOMStyleDeclaration::QueryInterface(REFNSIID aIID,
                                      void** aInstancePtr)
{
  NS_PRECONDITION(nsnull != aInstancePtr, "null ptr");
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
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
nsDOMStyleDeclaration::GetScriptObject(nsIScriptContext* aContext,
                                       void** aScriptObject)
{
  nsresult res = NS_OK;

  if (nsnull == mScriptObject) {
    nsISupports *parent = nsnull;
    if (nsnull != mContent) {
      res = mContent->QueryInterface(kISupportsIID, (void **)&parent);
      if (NS_OK != res) {
        return res;
      }
    }
    nsISupports *supports = (nsISupports *)(nsIDOMCSSStyleDeclaration *)this;
    // XXX Should be done through factory
    res = NS_NewScriptCSSStyleDeclaration(aContext, 
                                          supports,
                                          parent, 
                                          (void**)&mScriptObject);
    NS_RELEASE(parent);
  }
  *aScriptObject = mScriptObject;

  return res;
}

NS_IMETHODIMP 
nsDOMStyleDeclaration::SetScriptObject(void* aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

void 
nsDOMStyleDeclaration::DropContent()
{
  mContent = nsnull;
}

nsresult
nsDOMStyleDeclaration::GetContentStyle(nsICSSDeclaration **aDecl,
                                       PRBool aAllocate)
{
  nsHTMLValue val;
  nsIStyleRule* rule;
  nsICSSStyleRule*  cssRule;
  nsresult result = NS_OK;

  *aDecl = nsnull;
  if (nsnull != mContent) {
    mContent->GetAttribute(nsHTMLAtoms::style, val);
    if (eHTMLUnit_ISupports == val.GetUnit()) {
      rule = (nsIStyleRule*) val.GetISupportsValue();
      result = rule->QueryInterface(kICSSStyleRuleIID, (void**)&cssRule);
      if (NS_OK == result) {
        *aDecl = cssRule->GetDeclaration();
        NS_RELEASE(cssRule);
      }      
      NS_RELEASE(rule);
    }
    else if (PR_TRUE == aAllocate) {
      result = NS_NewCSSDeclaration(aDecl);
      if (NS_OK == result) {
        result = NS_NewCSSStyleRule(&cssRule, nsCSSSelector());
        if (NS_OK == result) {
          cssRule->SetDeclaration(*aDecl);
          cssRule->SetWeight(0x7fffffff);
          rule = (nsIStyleRule *)cssRule;
          result = mContent->SetAttribute(nsHTMLAtoms::style, 
                                          nsHTMLValue(cssRule), 
                                          PR_FALSE);
          NS_RELEASE(cssRule);
        }
        else {
          NS_RELEASE(*aDecl);
        }
      }
    }
  }

  return result;
}

NS_IMETHODIMP    
nsDOMStyleDeclaration::GetLength(PRUint32* aLength)
{
  nsICSSDeclaration *decl;
  nsresult result = GetContentStyle(&decl, PR_FALSE);
 
  *aLength = 0;
  if ((NS_OK == result) && (nsnull != decl)) {
    result = decl->Count(aLength);
    NS_RELEASE(decl);
  }

  return result;
}

NS_IMETHODIMP    
nsDOMStyleDeclaration::Item(PRUint32 aIndex, nsString& aReturn)
{
  nsICSSDeclaration *decl;
  nsresult result = GetContentStyle(&decl, PR_FALSE);

  aReturn.SetLength(0);
  if ((NS_OK == result) && (nsnull != decl)) {
    result = decl->GetNthProperty(aIndex, aReturn);
    NS_RELEASE(decl);
  }

  return result;
}

NS_IMETHODIMP    
nsDOMStyleDeclaration::GetPropertyValue(const nsString& aPropertyName, 
                                     nsString& aReturn)
{
  nsCSSValue val;
  nsICSSDeclaration *decl;
  nsresult result = GetContentStyle(&decl, PR_FALSE);

  aReturn.SetLength(0);
  if ((NS_OK == result) && (nsnull != decl)) {
    char prop[50];
    aPropertyName.ToCString(prop, sizeof(prop));
    result = decl->GetValue(prop, val);
    NS_RELEASE(decl);
    if (NS_OK == result) {
      PRInt32 propID = nsCSSProps::LookupName(prop);
      nsCSSValue::ValueToString(aReturn, val, propID);
    }
  }

  return result;
}

NS_IMETHODIMP    
nsDOMStyleDeclaration::GetPropertyPriority(const nsString& aPropertyName, 
                                        nsString& aReturn)
{
  nsICSSDeclaration *decl;
  nsresult result = GetContentStyle(&decl, PR_FALSE);
  PRBool isImportant = PR_FALSE;

  if ((NS_OK == result) && (nsnull != decl)) {
    char prop[50];
    aPropertyName.ToCString(prop, sizeof(prop));
    result = decl->GetValueIsImportant(prop, isImportant);
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
nsDOMStyleDeclaration::SetProperty(const nsString& aPropertyName, 
                                   const nsString& aValue, 
                                   const nsString& aPriority)
{
  nsAutoString declString;
  nsICSSDeclaration *decl;
  nsresult result = GetContentStyle(&decl, PR_TRUE);

  if ((NS_OK == result) && (nsnull != decl)) {
    declString.SetString(aPropertyName);
    declString.Append(":");
    declString.Append(aValue);

    nsICSSParser* css;
    result = NS_NewCSSParser(&css);
    if (NS_OK == result) {
      result = css->ParseAndAppendDeclaration(declString, nsnull, *decl);
      if (NS_OK == result) {
        if (aPriority.Equals("!important")) {
          char prop[50];
          aPropertyName.ToCString(prop, sizeof(prop));
          decl->SetValueImportant(prop);
        }
        nsIDocument *doc;
        result = mContent->GetDocument(doc);
        if (NS_OK == result) {
          doc->AttributeChanged(mContent, nsHTMLAtoms::style);
          NS_RELEASE(doc);
        }
      }
      NS_RELEASE(css);
    }
    NS_RELEASE(decl);
  }

  return result;
}

NS_IMETHODIMP 
nsDOMStyleDeclaration::GetAzimuth(nsString& aAzimuth)
{
  return GetPropertyValue("azimuth", aAzimuth);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetAzimuth(const nsString& aAzimuth)
{
  return SetProperty("azimuth", aAzimuth, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetBackground(nsString& aBackground)
{
  return GetPropertyValue("background", aBackground);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetBackground(const nsString& aBackground)
{
  return SetProperty("background", aBackground, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetBackgroundAttachment(nsString& aBackgroundAttachment)
{
  return GetPropertyValue("background-attachment", aBackgroundAttachment);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetBackgroundAttachment(const nsString& aBackgroundAttachment)
{
  return SetProperty("background-attachment", aBackgroundAttachment, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetBackgroundColor(nsString& aBackgroundColor)
{
  return GetPropertyValue("background-color", aBackgroundColor);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetBackgroundColor(const nsString& aBackgroundColor)
{
  return SetProperty("background-color", aBackgroundColor, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetBackgroundImage(nsString& aBackgroundImage)
{
  return GetPropertyValue("background-image", aBackgroundImage);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetBackgroundImage(const nsString& aBackgroundImage)
{
  return SetProperty("background-image", aBackgroundImage, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetBackgroundPosition(nsString& aBackgroundPosition)
{
  return GetPropertyValue("background-position", aBackgroundPosition);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetBackgroundPosition(const nsString& aBackgroundPosition)
{
  return SetProperty("background-position", aBackgroundPosition, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetBackgroundRepeat(nsString& aBackgroundRepeat)
{
  return GetPropertyValue("background-repeat", aBackgroundRepeat);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetBackgroundRepeat(const nsString& aBackgroundRepeat)
{
  return SetProperty("background-repeat", aBackgroundRepeat, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetBorder(nsString& aBorder)
{
  return GetPropertyValue("border", aBorder);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetBorder(const nsString& aBorder)
{
  return SetProperty("border", aBorder, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetBorderCollapse(nsString& aBorderCollapse)
{
  return GetPropertyValue("border-collapse", aBorderCollapse);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetBorderCollapse(const nsString& aBorderCollapse)
{
  return SetProperty("border-collapse", aBorderCollapse, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetBorderColor(nsString& aBorderColor)
{
  return GetPropertyValue("border-color", aBorderColor);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetBorderColor(const nsString& aBorderColor)
{
  return SetProperty("border-color", aBorderColor, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetBorderSpacing(nsString& aBorderSpacing)
{
  return GetPropertyValue("border-spacing", aBorderSpacing);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetBorderSpacing(const nsString& aBorderSpacing)
{
  return SetProperty("border-spacing", aBorderSpacing, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetBorderStyle(nsString& aBorderStyle)
{
  return GetPropertyValue("border-style", aBorderStyle);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetBorderStyle(const nsString& aBorderStyle)
{
  return SetProperty("border-style", aBorderStyle, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetBorderTop(nsString& aBorderTop)
{
  return GetPropertyValue("border-top", aBorderTop);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetBorderTop(const nsString& aBorderTop)
{
  return SetProperty("border-top", aBorderTop, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetBorderRight(nsString& aBorderRight)
{
  return GetPropertyValue("border-right", aBorderRight);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetBorderRight(const nsString& aBorderRight)
{
  return SetProperty("border-right", aBorderRight, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetBorderBottom(nsString& aBorderBottom)
{
  return GetPropertyValue("border-bottom", aBorderBottom);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetBorderBottom(const nsString& aBorderBottom)
{
  return SetProperty("border-bottom", aBorderBottom, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetBorderLeft(nsString& aBorderLeft)
{
  return GetPropertyValue("border-left", aBorderLeft);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetBorderLeft(const nsString& aBorderLeft)
{
  return SetProperty("border-left", aBorderLeft, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetBorderTopColor(nsString& aBorderTopColor)
{
  return GetPropertyValue("border-top-color", aBorderTopColor);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetBorderTopColor(const nsString& aBorderTopColor)
{
  return SetProperty("border-top-color", aBorderTopColor, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetBorderRightColor(nsString& aBorderRightColor)
{
  return GetPropertyValue("border-right-color", aBorderRightColor);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetBorderRightColor(const nsString& aBorderRightColor)
{
  return SetProperty("border-right-color", aBorderRightColor, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetBorderBottomColor(nsString& aBorderBottomColor)
{
  return GetPropertyValue("border-bottom-color", aBorderBottomColor);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetBorderBottomColor(const nsString& aBorderBottomColor)
{
  return SetProperty("border-bottom-color", aBorderBottomColor, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetBorderLeftColor(nsString& aBorderLeftColor)
{
  return GetPropertyValue("border-left-color", aBorderLeftColor);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetBorderLeftColor(const nsString& aBorderLeftColor)
{
  return SetProperty("border-left-color", aBorderLeftColor, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetBorderTopStyle(nsString& aBorderTopStyle)
{
  return GetPropertyValue("border-top-style", aBorderTopStyle);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetBorderTopStyle(const nsString& aBorderTopStyle)
{
  return SetProperty("border-top-style", aBorderTopStyle, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetBorderRightStyle(nsString& aBorderRightStyle)
{
  return GetPropertyValue("border-right-style", aBorderRightStyle);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetBorderRightStyle(const nsString& aBorderRightStyle)
{
  return SetProperty("border-right-style", aBorderRightStyle, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetBorderBottomStyle(nsString& aBorderBottomStyle)
{
  return GetPropertyValue("border-bottom-style", aBorderBottomStyle);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetBorderBottomStyle(const nsString& aBorderBottomStyle)
{
  return SetProperty("border-bottom-style", aBorderBottomStyle, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetBorderLeftStyle(nsString& aBorderLeftStyle)
{
  return GetPropertyValue("border-left-style", aBorderLeftStyle);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetBorderLeftStyle(const nsString& aBorderLeftStyle)
{
  return SetProperty("border-left-style", aBorderLeftStyle, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetBorderTopWidth(nsString& aBorderTopWidth)
{
  return GetPropertyValue("border-top-width", aBorderTopWidth);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetBorderTopWidth(const nsString& aBorderTopWidth)
{
  return SetProperty("border-top-width", aBorderTopWidth, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetBorderRightWidth(nsString& aBorderRightWidth)
{
  return GetPropertyValue("border-right-width", aBorderRightWidth);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetBorderRightWidth(const nsString& aBorderRightWidth)
{
  return SetProperty("border-right-width", aBorderRightWidth, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetBorderBottomWidth(nsString& aBorderBottomWidth)
{
  return GetPropertyValue("border-bottom-width", aBorderBottomWidth);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetBorderBottomWidth(const nsString& aBorderBottomWidth)
{
  return SetProperty("border-bottom-width", aBorderBottomWidth, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetBorderLeftWidth(nsString& aBorderLeftWidth)
{
  return GetPropertyValue("border-left-width", aBorderLeftWidth);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetBorderLeftWidth(const nsString& aBorderLeftWidth)
{
  return SetProperty("border-left-width", aBorderLeftWidth, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetBorderWidth(nsString& aBorderWidth)
{
  return GetPropertyValue("border-width", aBorderWidth);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetBorderWidth(const nsString& aBorderWidth)
{
  return SetProperty("border-width", aBorderWidth, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetBottom(nsString& aBottom)
{
  return GetPropertyValue("bottom", aBottom);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetBottom(const nsString& aBottom)
{
  return SetProperty("bottom", aBottom, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetCaptionSide(nsString& aCaptionSide)
{
  return GetPropertyValue("caption-side", aCaptionSide);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetCaptionSide(const nsString& aCaptionSide)
{
  return SetProperty("caption-side", aCaptionSide, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetClear(nsString& aClear)
{
  return GetPropertyValue("clear", aClear);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetClear(const nsString& aClear)
{
  return SetProperty("clear", aClear, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetClip(nsString& aClip)
{
  return GetPropertyValue("clip", aClip);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetClip(const nsString& aClip)
{
  return SetProperty("clip", aClip, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetColor(nsString& aColor)
{
  return GetPropertyValue("color", aColor);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetColor(const nsString& aColor)
{
  return SetProperty("color", aColor, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetContent(nsString& aContent)
{
  return GetPropertyValue("content", aContent);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetContent(const nsString& aContent)
{
  return SetProperty("content", aContent, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetCounterIncrement(nsString& aCounterIncrement)
{
  return GetPropertyValue("counter-increment", aCounterIncrement);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetCounterIncrement(const nsString& aCounterIncrement)
{
  return SetProperty("counter-increment", aCounterIncrement, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetCounterReset(nsString& aCounterReset)
{
  return GetPropertyValue("counter-reset", aCounterReset);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetCounterReset(const nsString& aCounterReset)
{
  return SetProperty("counter-reset", aCounterReset, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetCue(nsString& aCue)
{
  return GetPropertyValue("cue", aCue);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetCue(const nsString& aCue)
{
  return SetProperty("cue", aCue, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetCueAfter(nsString& aCueAfter)
{
  return GetPropertyValue("cue-after", aCueAfter);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetCueAfter(const nsString& aCueAfter)
{
  return SetProperty("cue-after", aCueAfter, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetCueBefore(nsString& aCueBefore)
{
  return GetPropertyValue("cue-before", aCueBefore);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetCueBefore(const nsString& aCueBefore)
{
  return SetProperty("cue-before", aCueBefore, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetCursor(nsString& aCursor)
{
  return GetPropertyValue("cursor", aCursor);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetCursor(const nsString& aCursor)
{
  return SetProperty("cursor", aCursor, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetDirection(nsString& aDirection)
{
  return GetPropertyValue("direction", aDirection);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetDirection(const nsString& aDirection)
{
  return SetProperty("direction", aDirection, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetDisplay(nsString& aDisplay)
{
  return GetPropertyValue("display", aDisplay);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetDisplay(const nsString& aDisplay)
{
  return SetProperty("display", aDisplay, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetElevation(nsString& aElevation)
{
  return GetPropertyValue("elevation", aElevation);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetElevation(const nsString& aElevation)
{
  return SetProperty("elevation", aElevation, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetEmptyCells(nsString& aEmptyCells)
{
  return GetPropertyValue("empty-cells", aEmptyCells);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetEmptyCells(const nsString& aEmptyCells)
{
  return SetProperty("empty-cells", aEmptyCells, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetStyleFloat(nsString& aStyleFloat)
{
  return GetPropertyValue("style-float", aStyleFloat);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetStyleFloat(const nsString& aStyleFloat)
{
  return SetProperty("style-float", aStyleFloat, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetFont(nsString& aFont)
{
  return GetPropertyValue("font", aFont);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetFont(const nsString& aFont)
{
  return SetProperty("font", aFont, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetFontFamily(nsString& aFontFamily)
{
  return GetPropertyValue("font-family", aFontFamily);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetFontFamily(const nsString& aFontFamily)
{
  return SetProperty("font-family", aFontFamily, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetFontSize(nsString& aFontSize)
{
  return GetPropertyValue("font-size", aFontSize);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetFontSize(const nsString& aFontSize)
{
  return SetProperty("font-size", aFontSize, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetFontSizeAdjust(nsString& aFontSizeAdjust)
{
  return GetPropertyValue("font-size-adjust", aFontSizeAdjust);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetFontSizeAdjust(const nsString& aFontSizeAdjust)
{
  return SetProperty("font-size-adjust", aFontSizeAdjust, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetFontStretch(nsString& aFontStretch)
{
  return GetPropertyValue("font-stretch", aFontStretch);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetFontStretch(const nsString& aFontStretch)
{
  return SetProperty("font-stretch", aFontStretch, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetFontStyle(nsString& aFontStyle)
{
  return GetPropertyValue("font-style", aFontStyle);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetFontStyle(const nsString& aFontStyle)
{
  return SetProperty("font-style", aFontStyle, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetFontVariant(nsString& aFontVariant)
{
  return GetPropertyValue("font-variant", aFontVariant);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetFontVariant(const nsString& aFontVariant)
{
  return SetProperty("font-variant", aFontVariant, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetFontWeight(nsString& aFontWeight)
{
  return GetPropertyValue("font-weight", aFontWeight);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetFontWeight(const nsString& aFontWeight)
{
  return SetProperty("font-weight", aFontWeight, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetHeight(nsString& aHeight)
{
  return GetPropertyValue("height", aHeight);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetHeight(const nsString& aHeight)
{
  return SetProperty("height", aHeight, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetLeft(nsString& aLeft)
{
  return GetPropertyValue("left", aLeft);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetLeft(const nsString& aLeft)
{
  return SetProperty("left", aLeft, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetLetterSpacing(nsString& aLetterSpacing)
{
  return GetPropertyValue("letter-spacing", aLetterSpacing);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetLetterSpacing(const nsString& aLetterSpacing)
{
  return SetProperty("letter-spacing", aLetterSpacing, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetLineHeight(nsString& aLineHeight)
{
  return GetPropertyValue("line-height", aLineHeight);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetLineHeight(const nsString& aLineHeight)
{
  return SetProperty("line-height", aLineHeight, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetListStyle(nsString& aListStyle)
{
  return GetPropertyValue("list-style", aListStyle);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetListStyle(const nsString& aListStyle)
{
  return SetProperty("list-style", aListStyle, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetListStyleImage(nsString& aListStyleImage)
{
  return GetPropertyValue("list-style-image", aListStyleImage);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetListStyleImage(const nsString& aListStyleImage)
{
  return SetProperty("list-style-image", aListStyleImage, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetListStylePosition(nsString& aListStylePosition)
{
  return GetPropertyValue("list-style-position", aListStylePosition);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetListStylePosition(const nsString& aListStylePosition)
{
  return SetProperty("list-style-position", aListStylePosition, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetListStyleType(nsString& aListStyleType)
{
  return GetPropertyValue("list-style-type", aListStyleType);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetListStyleType(const nsString& aListStyleType)
{
  return SetProperty("list-style-type", aListStyleType, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetMargin(nsString& aMargin)
{
  return GetPropertyValue("margin", aMargin);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetMargin(const nsString& aMargin)
{
  return SetProperty("margin", aMargin, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetMarginTop(nsString& aMarginTop)
{
  return GetPropertyValue("margin-top", aMarginTop);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetMarginTop(const nsString& aMarginTop)
{
  return SetProperty("margin-top", aMarginTop, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetMarginRight(nsString& aMarginRight)
{
  return GetPropertyValue("margin-right", aMarginRight);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetMarginRight(const nsString& aMarginRight)
{
  return SetProperty("margin-right", aMarginRight, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetMarginBottom(nsString& aMarginBottom)
{
  return GetPropertyValue("margin-bottom", aMarginBottom);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetMarginBottom(const nsString& aMarginBottom)
{
  return SetProperty("margin-bottom", aMarginBottom, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetMarginLeft(nsString& aMarginLeft)
{
  return GetPropertyValue("margin-left", aMarginLeft);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetMarginLeft(const nsString& aMarginLeft)
{
  return SetProperty("margin-left", aMarginLeft, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetMarkerOffset(nsString& aMarkerOffset)
{
  return GetPropertyValue("marker-offset", aMarkerOffset);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetMarkerOffset(const nsString& aMarkerOffset)
{
  return SetProperty("marker-offset", aMarkerOffset, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetMarks(nsString& aMarks)
{
  return GetPropertyValue("marks", aMarks);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetMarks(const nsString& aMarks)
{
  return SetProperty("marks", aMarks, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetMaxHeight(nsString& aMaxHeight)
{
  return GetPropertyValue("max-height", aMaxHeight);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetMaxHeight(const nsString& aMaxHeight)
{
  return SetProperty("max-height", aMaxHeight, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetMaxWidth(nsString& aMaxWidth)
{
  return GetPropertyValue("max-width", aMaxWidth);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetMaxWidth(const nsString& aMaxWidth)
{
  return SetProperty("max-width", aMaxWidth, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetMinHeight(nsString& aMinHeight)
{
  return GetPropertyValue("min-height", aMinHeight);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetMinHeight(const nsString& aMinHeight)
{
  return SetProperty("min-height", aMinHeight, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetMinWidth(nsString& aMinWidth)
{
  return GetPropertyValue("min-width", aMinWidth);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetMinWidth(const nsString& aMinWidth)
{
  return SetProperty("min-width", aMinWidth, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetOrphans(nsString& aOrphans)
{
  return GetPropertyValue("orphans", aOrphans);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetOrphans(const nsString& aOrphans)
{
  return SetProperty("orphans", aOrphans, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetOutline(nsString& aOutline)
{
  return GetPropertyValue("outline", aOutline);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetOutline(const nsString& aOutline)
{
  return SetProperty("outline", aOutline, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetOutlineColor(nsString& aOutlineColor)
{
  return GetPropertyValue("outline-color", aOutlineColor);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetOutlineColor(const nsString& aOutlineColor)
{
  return SetProperty("outline-color", aOutlineColor, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetOutlineStyle(nsString& aOutlineStyle)
{
  return GetPropertyValue("outline-style", aOutlineStyle);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetOutlineStyle(const nsString& aOutlineStyle)
{
  return SetProperty("outline-style", aOutlineStyle, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetOutlineWidth(nsString& aOutlineWidth)
{
  return GetPropertyValue("outline-width", aOutlineWidth);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetOutlineWidth(const nsString& aOutlineWidth)
{
  return SetProperty("outline-width", aOutlineWidth, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetOverflow(nsString& aOverflow)
{
  return GetPropertyValue("overflow", aOverflow);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetOverflow(const nsString& aOverflow)
{
  return SetProperty("overflow", aOverflow, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetPadding(nsString& aPadding)
{
  return GetPropertyValue("padding", aPadding);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetPadding(const nsString& aPadding)
{
  return SetProperty("padding", aPadding, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetPaddingTop(nsString& aPaddingTop)
{
  return GetPropertyValue("padding-top", aPaddingTop);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetPaddingTop(const nsString& aPaddingTop)
{
  return SetProperty("padding-top", aPaddingTop, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetPaddingRight(nsString& aPaddingRight)
{
  return GetPropertyValue("padding-right", aPaddingRight);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetPaddingRight(const nsString& aPaddingRight)
{
  return SetProperty("padding-right", aPaddingRight, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetPaddingBottom(nsString& aPaddingBottom)
{
  return GetPropertyValue("padding-bottom", aPaddingBottom);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetPaddingBottom(const nsString& aPaddingBottom)
{
  return SetProperty("padding-bottom", aPaddingBottom, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetPaddingLeft(nsString& aPaddingLeft)
{
  return GetPropertyValue("padding-left", aPaddingLeft);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetPaddingLeft(const nsString& aPaddingLeft)
{
  return SetProperty("padding-left", aPaddingLeft, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetPage(nsString& aPage)
{
  return GetPropertyValue("page", aPage);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetPage(const nsString& aPage)
{
  return SetProperty("page", aPage, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetPageBreakAfter(nsString& aPageBreakAfter)
{
  return GetPropertyValue("page-break-after", aPageBreakAfter);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetPageBreakAfter(const nsString& aPageBreakAfter)
{
  return SetProperty("page-break-after", aPageBreakAfter, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetPageBreakBefore(nsString& aPageBreakBefore)
{
  return GetPropertyValue("page-break-before", aPageBreakBefore);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetPageBreakBefore(const nsString& aPageBreakBefore)
{
  return SetProperty("page-break-before", aPageBreakBefore, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetPageBreakInside(nsString& aPageBreakInside)
{
  return GetPropertyValue("page-break-inside", aPageBreakInside);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetPageBreakInside(const nsString& aPageBreakInside)
{
  return SetProperty("page-break-inside", aPageBreakInside, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetPause(nsString& aPause)
{
  return GetPropertyValue("pause", aPause);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetPause(const nsString& aPause)
{
  return SetProperty("pause", aPause, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetPauseAfter(nsString& aPauseAfter)
{
  return GetPropertyValue("pause-after", aPauseAfter);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetPauseAfter(const nsString& aPauseAfter)
{
  return SetProperty("pause-after", aPauseAfter, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetPauseBefore(nsString& aPauseBefore)
{
  return GetPropertyValue("pause-before", aPauseBefore);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetPauseBefore(const nsString& aPauseBefore)
{
  return SetProperty("pause-before", aPauseBefore, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetPitch(nsString& aPitch)
{
  return GetPropertyValue("pitch", aPitch);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetPitch(const nsString& aPitch)
{
  return SetProperty("pitch", aPitch, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetPitchRange(nsString& aPitchRange)
{
  return GetPropertyValue("pitch-range", aPitchRange);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetPitchRange(const nsString& aPitchRange)
{
  return SetProperty("pitch-range", aPitchRange, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetPlayDuring(nsString& aPlayDuring)
{
  return GetPropertyValue("play-during", aPlayDuring);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetPlayDuring(const nsString& aPlayDuring)
{
  return SetProperty("play-during", aPlayDuring, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetPosition(nsString& aPosition)
{
  return GetPropertyValue("position", aPosition);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetPosition(const nsString& aPosition)
{
  return SetProperty("position", aPosition, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetQuotes(nsString& aQuotes)
{
  return GetPropertyValue("quotes", aQuotes);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetQuotes(const nsString& aQuotes)
{
  return SetProperty("quotes", aQuotes, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetRichness(nsString& aRichness)
{
  return GetPropertyValue("richness", aRichness);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetRichness(const nsString& aRichness)
{
  return SetProperty("richness", aRichness, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetRight(nsString& aRight)
{
  return GetPropertyValue("right", aRight);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetRight(const nsString& aRight)
{
  return SetProperty("right", aRight, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetSize(nsString& aSize)
{
  return GetPropertyValue("size", aSize);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetSize(const nsString& aSize)
{
  return SetProperty("size", aSize, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetSpeak(nsString& aSpeak)
{
  return GetPropertyValue("speak", aSpeak);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetSpeak(const nsString& aSpeak)
{
  return SetProperty("speak", aSpeak, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetSpeakHeader(nsString& aSpeakHeader)
{
  return GetPropertyValue("speak-header", aSpeakHeader);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetSpeakHeader(const nsString& aSpeakHeader)
{
  return SetProperty("speak-header", aSpeakHeader, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetSpeakNumeral(nsString& aSpeakNumeral)
{
  return GetPropertyValue("speak-numeral", aSpeakNumeral);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetSpeakNumeral(const nsString& aSpeakNumeral)
{
  return SetProperty("speak-numeral", aSpeakNumeral, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetSpeakPunctuation(nsString& aSpeakPunctuation)
{
  return GetPropertyValue("speak-punctuation", aSpeakPunctuation);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetSpeakPunctuation(const nsString& aSpeakPunctuation)
{
  return SetProperty("speak-punctuation", aSpeakPunctuation, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetSpeechRate(nsString& aSpeechRate)
{
  return GetPropertyValue("speech-rate", aSpeechRate);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetSpeechRate(const nsString& aSpeechRate)
{
  return SetProperty("speech-rate", aSpeechRate, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetStress(nsString& aStress)
{
  return GetPropertyValue("stress", aStress);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetStress(const nsString& aStress)
{
  return SetProperty("stress", aStress, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetTableLayout(nsString& aTableLayout)
{
  return GetPropertyValue("table-layout", aTableLayout);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetTableLayout(const nsString& aTableLayout)
{
  return SetProperty("table-layout", aTableLayout, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetTextAlign(nsString& aTextAlign)
{
  return GetPropertyValue("text-align", aTextAlign);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetTextAlign(const nsString& aTextAlign)
{
  return SetProperty("text-align", aTextAlign, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetTextDecoration(nsString& aTextDecoration)
{
  return GetPropertyValue("text-decoration", aTextDecoration);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetTextDecoration(const nsString& aTextDecoration)
{
  return SetProperty("text-decoration", aTextDecoration, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetTextIndent(nsString& aTextIndent)
{
  return GetPropertyValue("text-indent", aTextIndent);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetTextIndent(const nsString& aTextIndent)
{
  return SetProperty("text-indent", aTextIndent, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetTextShadow(nsString& aTextShadow)
{
  return GetPropertyValue("text-shadow", aTextShadow);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetTextShadow(const nsString& aTextShadow)
{
  return SetProperty("text-shadow", aTextShadow, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetTextTransform(nsString& aTextTransform)
{
  return GetPropertyValue("text-transform", aTextTransform);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetTextTransform(const nsString& aTextTransform)
{
  return SetProperty("text-transform", aTextTransform, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetTop(nsString& aTop)
{
  return GetPropertyValue("top", aTop);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetTop(const nsString& aTop)
{
  return SetProperty("top", aTop, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetUnicodeBidi(nsString& aUnicodeBidi)
{
  return GetPropertyValue("unicode-bidi", aUnicodeBidi);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetUnicodeBidi(const nsString& aUnicodeBidi)
{
  return SetProperty("unicode-bidi", aUnicodeBidi, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetVerticalAlign(nsString& aVerticalAlign)
{
  return GetPropertyValue("vertical-align", aVerticalAlign);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetVerticalAlign(const nsString& aVerticalAlign)
{
  return SetProperty("vertical-align", aVerticalAlign, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetVisibility(nsString& aVisibility)
{
  return GetPropertyValue("visibility", aVisibility);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetVisibility(const nsString& aVisibility)
{
  return SetProperty("visibility", aVisibility, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetVoiceFamily(nsString& aVoiceFamily)
{
  return GetPropertyValue("voice-family", aVoiceFamily);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetVoiceFamily(const nsString& aVoiceFamily)
{
  return SetProperty("voice-family", aVoiceFamily, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetVolume(nsString& aVolume)
{
  return GetPropertyValue("volume", aVolume);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetVolume(const nsString& aVolume)
{
  return SetProperty("volume", aVolume, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetWhiteSpace(nsString& aWhiteSpace)
{
  return GetPropertyValue("white-space", aWhiteSpace);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetWhiteSpace(const nsString& aWhiteSpace)
{
  return SetProperty("white-space", aWhiteSpace, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetWidows(nsString& aWidows)
{
  return GetPropertyValue("widows", aWidows);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetWidows(const nsString& aWidows)
{
  return SetProperty("widows", aWidows, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetWidth(nsString& aWidth)
{
  return GetPropertyValue("width", aWidth);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetWidth(const nsString& aWidth)
{
  return SetProperty("width", aWidth, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetWordSpacing(nsString& aWordSpacing)
{
  return GetPropertyValue("word-spacing", aWordSpacing);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetWordSpacing(const nsString& aWordSpacing)
{
  return SetProperty("word-spacing", aWordSpacing, "");
}

NS_IMETHODIMP
nsDOMStyleDeclaration::GetZIndex(nsString& aZIndex)
{
  return GetPropertyValue("z-index", aZIndex);
}

NS_IMETHODIMP
nsDOMStyleDeclaration::SetZIndex(const nsString& aZIndex)
{
  return SetProperty("z-index", aZIndex, "");
}


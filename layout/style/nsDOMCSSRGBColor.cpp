/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Christopher A. Aillon <christopher@aillon.com>
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher A. Aillon <christopher@aillon.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* DOM object representing color values in DOM computed style */

#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsIDOMCSSPrimitiveValue.h"
#include "nsDOMCSSRGBColor.h"
#include "nsContentUtils.h"

nsDOMCSSRGBColor::nsDOMCSSRGBColor(nsIDOMCSSPrimitiveValue* aRed,
                                   nsIDOMCSSPrimitiveValue* aGreen,
                                   nsIDOMCSSPrimitiveValue* aBlue,
                                   nsIDOMCSSPrimitiveValue* aAlpha,
                                   bool aHasAlpha)
  : mRed(aRed), mGreen(aGreen), mBlue(aBlue), mAlpha(aAlpha)
  , mHasAlpha(aHasAlpha)
{
}

nsDOMCSSRGBColor::~nsDOMCSSRGBColor(void)
{
}

DOMCI_DATA(CSSRGBColor, nsDOMCSSRGBColor)

NS_INTERFACE_MAP_BEGIN(nsDOMCSSRGBColor)
  NS_INTERFACE_MAP_ENTRY(nsIDOMRGBColor)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSRGBAColor)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CSSRGBColor)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsDOMCSSRGBColor)
NS_IMPL_RELEASE(nsDOMCSSRGBColor)


NS_IMETHODIMP
nsDOMCSSRGBColor::GetRed(nsIDOMCSSPrimitiveValue** aRed)
{
  NS_ENSURE_TRUE(mRed, NS_ERROR_NOT_INITIALIZED);
  *aRed = mRed;
  NS_ADDREF(*aRed);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSRGBColor::GetGreen(nsIDOMCSSPrimitiveValue** aGreen)
{
  NS_ENSURE_TRUE(mGreen, NS_ERROR_NOT_INITIALIZED);
  *aGreen = mGreen;
  NS_ADDREF(*aGreen);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSRGBColor::GetBlue(nsIDOMCSSPrimitiveValue** aBlue)
{
  NS_ENSURE_TRUE(mBlue, NS_ERROR_NOT_INITIALIZED);
  *aBlue = mBlue;
  NS_ADDREF(*aBlue);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSRGBColor::GetAlpha(nsIDOMCSSPrimitiveValue** aAlpha)
{
  NS_ENSURE_TRUE(mAlpha, NS_ERROR_NOT_INITIALIZED);
  *aAlpha = mAlpha;
  NS_ADDREF(*aAlpha);
  return NS_OK;
}

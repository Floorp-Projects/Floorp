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
 * Boris Zbarsky <bzbarsky@mit.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsIDOMCSSPrimitiveValue.h"
#include "nsDOMCSSRect.h"
#include "nsContentUtils.h"

nsDOMCSSRect::nsDOMCSSRect(nsIDOMCSSPrimitiveValue* aTop,
                           nsIDOMCSSPrimitiveValue* aRight,
                           nsIDOMCSSPrimitiveValue* aBottom,
                           nsIDOMCSSPrimitiveValue* aLeft)
  : mTop(aTop), mRight(aRight), mBottom(aBottom), mLeft(aLeft)
{
}

nsDOMCSSRect::~nsDOMCSSRect(void)
{
}

// QueryInterface implementation for nsCSSRect
NS_INTERFACE_MAP_BEGIN(nsDOMCSSRect)
  NS_INTERFACE_MAP_ENTRY(nsIDOMRect)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CSSRect)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsDOMCSSRect)
NS_IMPL_RELEASE(nsDOMCSSRect)

  
NS_IMETHODIMP
nsDOMCSSRect::GetTop(nsIDOMCSSPrimitiveValue** aTop)
{
  NS_ENSURE_TRUE(mTop, NS_ERROR_NOT_INITIALIZED);
  *aTop = mTop;
  NS_ADDREF(*aTop);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSRect::GetRight(nsIDOMCSSPrimitiveValue** aRight)
{
  NS_ENSURE_TRUE(mRight, NS_ERROR_NOT_INITIALIZED);
  *aRight = mRight;
  NS_ADDREF(*aRight);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSRect::GetBottom(nsIDOMCSSPrimitiveValue** aBottom)
{
  NS_ENSURE_TRUE(mBottom, NS_ERROR_NOT_INITIALIZED);
  *aBottom = mBottom;
  NS_ADDREF(*aBottom);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSRect::GetLeft(nsIDOMCSSPrimitiveValue** aLeft)
{
  NS_ENSURE_TRUE(mLeft, NS_ERROR_NOT_INITIALIZED);
  *aLeft = mLeft;
  NS_ADDREF(*aLeft);
  return NS_OK;
}

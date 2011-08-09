/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick Beard
 *   Taras Glek
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

#include "nsScriptableRegion.h"
#include "nsCOMPtr.h"
#include "nsIXPConnect.h"
#include "nsServiceManagerUtils.h"
#include "jsapi.h"

nsScriptableRegion::nsScriptableRegion()
{
}

NS_IMPL_ISUPPORTS1(nsScriptableRegion, nsIScriptableRegion)

NS_IMETHODIMP nsScriptableRegion::Init()
{
  return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::SetToRegion(nsIScriptableRegion *aRegion)
{
  aRegion->GetRegion(&mRegion);
  return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::SetToRect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  mRegion = nsIntRect(aX, aY, aWidth, aHeight);
  return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::IntersectRegion(nsIScriptableRegion *aRegion)
{
  nsIntRegion region;
  aRegion->GetRegion(&region);
  mRegion.And(mRegion, region);
  return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::IntersectRect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  mRegion.And(mRegion, nsIntRect(aX, aY, aWidth, aHeight));
  return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::UnionRegion(nsIScriptableRegion *aRegion)
{
  nsIntRegion region;
  aRegion->GetRegion(&region);
  mRegion.Or(mRegion, region);
  return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::UnionRect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  mRegion.Or(mRegion, nsIntRect(aX, aY, aWidth, aHeight));
  return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::SubtractRegion(nsIScriptableRegion *aRegion)
{
  nsIntRegion region;
  aRegion->GetRegion(&region);
  mRegion.Sub(mRegion, region);
  return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::SubtractRect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
  mRegion.Sub(mRegion, nsIntRect(aX, aY, aWidth, aHeight));
  return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::IsEmpty(PRBool *isEmpty)
{
  *isEmpty = mRegion.IsEmpty();
  return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::IsEqualRegion(nsIScriptableRegion *aRegion, PRBool *isEqual)
{
  nsIntRegion region;
  aRegion->GetRegion(&region);
  *isEqual = mRegion.IsEqual(region);
  return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::GetBoundingBox(PRInt32 *aX, PRInt32 *aY, PRInt32 *aWidth, PRInt32 *aHeight)
{
  nsIntRect boundRect = mRegion.GetBounds();
  *aX = boundRect.x;
  *aY = boundRect.y;
  *aWidth = boundRect.width;
  *aHeight = boundRect.height;
  return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::Offset(PRInt32 aXOffset, PRInt32 aYOffset)
{
  mRegion.MoveBy(aXOffset, aYOffset);
  return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::ContainsRect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight, PRBool *containsRect)
{
  *containsRect = mRegion.Contains(nsIntRect(aX, aY, aWidth, aHeight));
  return NS_OK;
}


NS_IMETHODIMP nsScriptableRegion::GetRegion(nsIntRegion* outRgn)
{
  *outRgn = mRegion;
  return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::GetRects() {
  nsAXPCNativeCallContext *ncc = nsnull;
  nsresult rv;
  nsCOMPtr<nsIXPConnect> xpConnect = do_GetService(nsIXPConnect::GetCID(), &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = xpConnect->GetCurrentNativeCallContext(&ncc);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!ncc)
    return NS_ERROR_FAILURE;
  
  jsval *retvalPtr;
  ncc->GetRetValPtr(&retvalPtr);

  PRUint32 numRects = mRegion.GetNumRects();

  if (!numRects) {
    *retvalPtr = JSVAL_NULL;
    ncc->SetReturnValueWasSet(PR_TRUE);
    return NS_OK;
  }

  JSContext *cx = nsnull;

  rv = ncc->GetJSContext(&cx);
  NS_ENSURE_SUCCESS(rv, rv);

  JSObject *destArray = JS_NewArrayObject(cx, numRects*4, NULL);
  *retvalPtr = OBJECT_TO_JSVAL(destArray);
  ncc->SetReturnValueWasSet(PR_TRUE);

  uint32 n = 0;
  nsIntRegionRectIterator iter(mRegion);
  const nsIntRect *rect;

  while ((rect = iter.Next())) {
    // This will contain bogus data if values don't fit in 31 bit
    JS_DefineElement(cx, destArray, n, INT_TO_JSVAL(rect->x), NULL, NULL, JSPROP_ENUMERATE);
    JS_DefineElement(cx, destArray, n+1, INT_TO_JSVAL(rect->y), NULL, NULL, JSPROP_ENUMERATE);
    JS_DefineElement(cx, destArray, n+2, INT_TO_JSVAL(rect->width), NULL, NULL, JSPROP_ENUMERATE);
    JS_DefineElement(cx, destArray, n+3, INT_TO_JSVAL(rect->height), NULL, NULL, JSPROP_ENUMERATE);

    n += 4;
  }

  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

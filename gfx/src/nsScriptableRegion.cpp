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

nsScriptableRegion::nsScriptableRegion(nsIRegion* region) : mRegion(nsnull), mRectSet(nsnull)
{
	mRegion = region;
	NS_IF_ADDREF(mRegion);
}

nsScriptableRegion::~nsScriptableRegion()
{
  if (mRegion) {
    mRegion->FreeRects(mRectSet);
    NS_RELEASE(mRegion);
  }
}

NS_IMPL_ISUPPORTS1(nsScriptableRegion, nsIScriptableRegion)

NS_IMETHODIMP nsScriptableRegion::Init()
{
	return mRegion->Init();
}

NS_IMETHODIMP nsScriptableRegion::SetToRegion(nsIScriptableRegion *aRegion)
{
	nsCOMPtr<nsIRegion> region(do_QueryInterface(aRegion));
	mRegion->SetTo(*region);
	return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::SetToRect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
	mRegion->SetTo(aX, aY, aWidth, aHeight);
	return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::IntersectRegion(nsIScriptableRegion *aRegion)
{
	nsCOMPtr<nsIRegion> region(do_QueryInterface(aRegion));
	mRegion->Intersect(*region);
	return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::IntersectRect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
	mRegion->Intersect(aX, aY, aWidth, aHeight);
	return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::UnionRegion(nsIScriptableRegion *aRegion)
{
	nsCOMPtr<nsIRegion> region(do_QueryInterface(aRegion));
	mRegion->Union(*region);
	return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::UnionRect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
	mRegion->Union(aX, aY, aWidth, aHeight);
	return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::SubtractRegion(nsIScriptableRegion *aRegion)
{
	nsCOMPtr<nsIRegion> region(do_QueryInterface(aRegion));
	mRegion->Subtract(*region);
	return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::SubtractRect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight)
{
	mRegion->Subtract(aX, aY, aWidth, aHeight);
	return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::IsEmpty(PRBool *isEmpty)
{
	*isEmpty = mRegion->IsEmpty();
	return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::IsEqualRegion(nsIScriptableRegion *aRegion, PRBool *isEqual)
{
	nsCOMPtr<nsIRegion> region(do_QueryInterface(aRegion));
	*isEqual = mRegion->IsEqual(*region);
	return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::GetBoundingBox(PRInt32 *aX, PRInt32 *aY, PRInt32 *aWidth, PRInt32 *aHeight)
{
	mRegion->GetBoundingBox(aX, aY, aWidth, aHeight);
	return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::Offset(PRInt32 aXOffset, PRInt32 aYOffset)
{
	mRegion->Offset(aXOffset, aYOffset);
	return NS_OK;
}

NS_IMETHODIMP nsScriptableRegion::ContainsRect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight, PRBool *containsRect)
{
	*containsRect = mRegion->ContainsRect(aX, aY, aWidth, aHeight);
	return NS_OK;
}


NS_IMETHODIMP nsScriptableRegion::GetRegion(nsIRegion** outRgn)
{
  *outRgn = mRegion;
  NS_IF_ADDREF(*outRgn);
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
  
  rv = mRegion->GetRects(&mRectSet);
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (!mRectSet->mNumRects) {
    *retvalPtr = JSVAL_NULL;
    ncc->SetReturnValueWasSet(PR_TRUE);
    return NS_OK;
  }

  JSContext *cx = nsnull;
  
  rv = ncc->GetJSContext(&cx);
  NS_ENSURE_SUCCESS(rv, rv);
  
  JSObject *destArray = JS_NewArrayObject(cx, mRectSet->mNumRects*4, NULL);
  *retvalPtr = OBJECT_TO_JSVAL(destArray);
  ncc->SetReturnValueWasSet(PR_TRUE);
  
  for(PRUint32 i = 0; i < mRectSet->mNumRects; i++) {
    nsRegionRect &rect = mRectSet->mRects[i];
    int n = i*4;
    // This will contain bogus data if values don't fit in 31 bit
    JS_DefineElement(cx, destArray, n, INT_TO_JSVAL(rect.x), NULL, NULL, JSPROP_ENUMERATE);
    JS_DefineElement(cx, destArray, n+1, INT_TO_JSVAL(rect.y), NULL, NULL, JSPROP_ENUMERATE);
    JS_DefineElement(cx, destArray, n+2, INT_TO_JSVAL(rect.width), NULL, NULL, JSPROP_ENUMERATE);
    JS_DefineElement(cx, destArray, n+3, INT_TO_JSVAL(rect.height), NULL, NULL, JSPROP_ENUMERATE);
  }

  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

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
 * The Original Code is Mozilla SVG Project code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "DOMSVGPoint.h"
#include "DOMSVGPointList.h"
#include "SVGPoint.h"
#include "SVGAnimatedPointList.h"
#include "nsSVGElement.h"
#include "nsIDOMSVGPoint.h"
#include "nsDOMError.h"
#include "nsIDOMSVGMatrix.h"
#include "nsContentUtils.h" // NS_ENSURE_FINITE
#include "DOMSVGMatrix.h"

// See the architecture comment in DOMSVGPointList.h.

using namespace mozilla;

// We could use NS_IMPL_CYCLE_COLLECTION_1, except that in Unlink() we need to
// clear our list's weak ref to us to be safe. (The other option would be to
// not unlink and rely on the breaking of the other edges in the cycle, as
// NS_SVG_VAL_IMPL_CYCLE_COLLECTION does.)
NS_IMPL_CYCLE_COLLECTION_CLASS(DOMSVGPoint)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DOMSVGPoint)
  // We may not belong to a list, so we must null check tmp->mList.
  if (tmp->mList) {
    tmp->mList->mItems[tmp->mListIndex] = nsnull;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mList)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DOMSVGPoint)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMSVGPoint)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMSVGPoint)

DOMCI_DATA(SVGPoint, DOMSVGPoint)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMSVGPoint)
  NS_INTERFACE_MAP_ENTRY(DOMSVGPoint) // pseudo-interface
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGPoint)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGPoint)
NS_INTERFACE_MAP_END


NS_IMETHODIMP
DOMSVGPoint::GetX(float* aX)
{
#ifdef MOZ_SMIL
  if (mIsAnimValItem && HasOwner()) {
    Element()->FlushAnimations(); // May make HasOwner() == PR_FALSE
  }
#endif
  *aX = HasOwner() ? InternalItem().mX : mPt.mX;
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGPoint::SetX(float aX)
{
  if (mIsAnimValItem || mIsReadonly) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }

  NS_ENSURE_FINITE(aX, NS_ERROR_ILLEGAL_VALUE);

  if (HasOwner()) {
    InternalItem().mX = aX;
    Element()->DidChangePointList(PR_TRUE);
#ifdef MOZ_SMIL
    if (mList->AttrIsAnimating()) {
      Element()->AnimationNeedsResample();
    }
#endif
    return NS_OK;
  }
  mPt.mX = aX;
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGPoint::GetY(float* aY)
{
#ifdef MOZ_SMIL
  if (mIsAnimValItem && HasOwner()) {
    Element()->FlushAnimations(); // May make HasOwner() == PR_FALSE
  }
#endif
  *aY = HasOwner() ? InternalItem().mY : mPt.mY;
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGPoint::SetY(float aY)
{
  if (mIsAnimValItem || mIsReadonly) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }

  NS_ENSURE_FINITE(aY, NS_ERROR_ILLEGAL_VALUE);

  if (HasOwner()) {
    InternalItem().mY = aY;
    Element()->DidChangePointList(PR_TRUE);
#ifdef MOZ_SMIL
    if (mList->AttrIsAnimating()) {
      Element()->AnimationNeedsResample();
    }
#endif
    return NS_OK;
  }
  mPt.mY = aY;
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGPoint::MatrixTransform(nsIDOMSVGMatrix *matrix,
                             nsIDOMSVGPoint **_retval)
{
  nsCOMPtr<DOMSVGMatrix> domMatrix = do_QueryInterface(matrix);
  if (!domMatrix)
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  float x = HasOwner() ? InternalItem().mX : mPt.mX;
  float y = HasOwner() ? InternalItem().mY : mPt.mY;

  gfxPoint pt = domMatrix->Matrix().Transform(gfxPoint(x, y));
  NS_ADDREF(*_retval = new DOMSVGPoint(pt));

  return NS_OK;
}

void
DOMSVGPoint::InsertingIntoList(DOMSVGPointList *aList,
                               PRUint32 aListIndex,
                               bool aIsAnimValItem)
{
  NS_ABORT_IF_FALSE(!HasOwner(), "Inserting item that already has an owner");

  mList = aList;
  mListIndex = aListIndex;
  mIsReadonly = PR_FALSE;
  mIsAnimValItem = aIsAnimValItem;

  NS_ABORT_IF_FALSE(IndexIsValid(), "Bad index for DOMSVGPoint!");
}

void
DOMSVGPoint::RemovingFromList()
{
  mPt = InternalItem();
  mList = nsnull;
  NS_ABORT_IF_FALSE(!mIsReadonly, "mIsReadonly set for list");
  mIsAnimValItem = PR_FALSE;
}

SVGPoint&
DOMSVGPoint::InternalItem()
{
  return mList->InternalList().mItems[mListIndex];
}

#ifdef DEBUG
bool
DOMSVGPoint::IndexIsValid()
{
  return mListIndex < mList->InternalList().Length();
}
#endif


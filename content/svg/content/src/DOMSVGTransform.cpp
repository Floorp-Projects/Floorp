/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
 *   Brian Birtles <birtles@gmail.com>
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

#include "DOMSVGTransform.h"
#include "DOMSVGMatrix.h"
#include "SVGAnimatedTransformList.h"
#include "nsDOMError.h"
#include <math.h>
#include "nsContentUtils.h"

namespace mozilla {

//----------------------------------------------------------------------
// nsISupports methods:

// We could use NS_IMPL_CYCLE_COLLECTION_1, except that in Unlink() we need to
// clear our list's weak ref to us to be safe. (The other option would be to
// not unlink and rely on the breaking of the other edges in the cycle, as
// NS_SVG_VAL_IMPL_CYCLE_COLLECTION does.)
NS_IMPL_CYCLE_COLLECTION_CLASS(DOMSVGTransform)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DOMSVGTransform)
  // We may not belong to a list, so we must null check tmp->mList.
  if (tmp->mList) {
    tmp->mList->mItems[tmp->mListIndex] = nsnull;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mList)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DOMSVGTransform)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMSVGTransform)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMSVGTransform)

} // namespace mozilla
DOMCI_DATA(SVGTransform, mozilla::DOMSVGTransform)
namespace mozilla {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMSVGTransform)
  NS_INTERFACE_MAP_ENTRY(mozilla::DOMSVGTransform)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGTransform)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMSVGTransform)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGTransform)
NS_INTERFACE_MAP_END


//----------------------------------------------------------------------
// Ctors:

DOMSVGTransform::DOMSVGTransform(DOMSVGTransformList *aList,
                                 PRUint32 aListIndex,
                                 bool aIsAnimValItem)
  : mList(aList)
  , mListIndex(aListIndex)
  , mIsAnimValItem(aIsAnimValItem)
  , mTransform(nsnull)
  , mMatrixTearoff(nsnull)
{
  // These shifts are in sync with the members in the header.
  NS_ABORT_IF_FALSE(aList &&
                    aListIndex <= MaxListIndex() &&
                    aIsAnimValItem < (1 << 1), "bad arg");

  NS_ABORT_IF_FALSE(IndexIsValid(), "Bad index for DOMSVGNumber!");
}

DOMSVGTransform::DOMSVGTransform()
  : mList(nsnull)
  , mListIndex(0)
  , mIsAnimValItem(PR_FALSE)
  , mTransform(new SVGTransform()) // Default ctor for objects not in a list
                                   // initialises to matrix type with identity
                                   // matrix
  , mMatrixTearoff(nsnull)
{
}

DOMSVGTransform::DOMSVGTransform(const gfxMatrix &aMatrix)
  : mList(nsnull)
  , mListIndex(0)
  , mIsAnimValItem(PR_FALSE)
  , mTransform(new SVGTransform(aMatrix))
  , mMatrixTearoff(nsnull)
{
}

DOMSVGTransform::DOMSVGTransform(const SVGTransform &aTransform)
  : mList(nsnull)
  , mListIndex(0)
  , mIsAnimValItem(PR_FALSE)
  , mTransform(new SVGTransform(aTransform))
  , mMatrixTearoff(nsnull)
{
}


//----------------------------------------------------------------------
// nsIDOMSVGTransform methods:

/* readonly attribute unsigned short type; */
NS_IMETHODIMP
DOMSVGTransform::GetType(PRUint16 *aType)
{
  *aType = Transform().Type();
  return NS_OK;
}

/* readonly attribute nsIDOMSVGMatrix matrix; */
NS_IMETHODIMP
DOMSVGTransform::GetMatrix(nsIDOMSVGMatrix * *aMatrix)
{
  if (!mMatrixTearoff) {
    mMatrixTearoff = new DOMSVGMatrix(*this);
  }

  NS_ADDREF(*aMatrix = mMatrixTearoff);
  return NS_OK;
}

/* readonly attribute float angle; */
NS_IMETHODIMP
DOMSVGTransform::GetAngle(float *aAngle)
{
  *aAngle = Transform().Angle();
  return NS_OK;
}

/* void setMatrix (in nsIDOMSVGMatrix matrix); */
NS_IMETHODIMP
DOMSVGTransform::SetMatrix(nsIDOMSVGMatrix *matrix)
{
  if (mIsAnimValItem)
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;

  nsCOMPtr<DOMSVGMatrix> domMatrix = do_QueryInterface(matrix);
  if (!domMatrix)
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;

  Transform().SetMatrix(domMatrix->Matrix());
  NotifyElementOfChange();

  return NS_OK;
}

/* void setTranslate (in float tx, in float ty); */
NS_IMETHODIMP
DOMSVGTransform::SetTranslate(float tx, float ty)
{
  if (mIsAnimValItem) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }
  NS_ENSURE_FINITE2(tx, ty, NS_ERROR_ILLEGAL_VALUE);

  Transform().SetTranslate(tx, ty);
  NotifyElementOfChange();

  return NS_OK;
}

/* void setScale (in float sx, in float sy); */
NS_IMETHODIMP
DOMSVGTransform::SetScale(float sx, float sy)
{
  if (mIsAnimValItem) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }
  NS_ENSURE_FINITE2(sx, sy, NS_ERROR_ILLEGAL_VALUE);

  Transform().SetScale(sx, sy);
  NotifyElementOfChange();

  return NS_OK;
}

/* void setRotate (in float angle, in float cx, in float cy); */
NS_IMETHODIMP
DOMSVGTransform::SetRotate(float angle, float cx, float cy)
{
  if (mIsAnimValItem) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }
  NS_ENSURE_FINITE3(angle, cx, cy, NS_ERROR_ILLEGAL_VALUE);

  Transform().SetRotate(angle, cx, cy);
  NotifyElementOfChange();

  return NS_OK;
}

/* void setSkewX (in float angle); */
NS_IMETHODIMP
DOMSVGTransform::SetSkewX(float angle)
{
  if (mIsAnimValItem) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }
  NS_ENSURE_FINITE(angle, NS_ERROR_ILLEGAL_VALUE);

  nsresult rv = Transform().SetSkewX(angle);
  if (NS_FAILED(rv))
    return rv;
  NotifyElementOfChange();

  return NS_OK;
}

/* void setSkewY (in float angle); */
NS_IMETHODIMP
DOMSVGTransform::SetSkewY(float angle)
{
  if (mIsAnimValItem) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }
  NS_ENSURE_FINITE(angle, NS_ERROR_ILLEGAL_VALUE);

  nsresult rv = Transform().SetSkewY(angle);
  if (NS_FAILED(rv))
    return rv;
  NotifyElementOfChange();

  return NS_OK;
}


//----------------------------------------------------------------------
// List management methods:

void
DOMSVGTransform::InsertingIntoList(DOMSVGTransformList *aList,
                                   PRUint32 aListIndex,
                                   bool aIsAnimValItem)
{
  NS_ABORT_IF_FALSE(!HasOwner(), "Inserting item that is already in a list");

  mList = aList;
  mListIndex = aListIndex;
  mIsAnimValItem = aIsAnimValItem;
  mTransform = nsnull;

  NS_ABORT_IF_FALSE(IndexIsValid(), "Bad index for DOMSVGLength!");
}

void
DOMSVGTransform::RemovingFromList()
{
  NS_ABORT_IF_FALSE(!mTransform,
      "Item in list also has another non-list value associated with it");

  mTransform = new SVGTransform(InternalItem());
  mList = nsnull;
  mIsAnimValItem = PR_FALSE;
}

SVGTransform&
DOMSVGTransform::InternalItem()
{
  SVGAnimatedTransformList *alist = Element()->GetAnimatedTransformList();
  return mIsAnimValItem && alist->mAnimVal ?
    (*alist->mAnimVal)[mListIndex] :
    alist->mBaseVal[mListIndex];
}

const SVGTransform&
DOMSVGTransform::InternalItem() const
{
  return const_cast<DOMSVGTransform *>(this)->InternalItem();
}

#ifdef DEBUG
bool
DOMSVGTransform::IndexIsValid()
{
  SVGAnimatedTransformList *alist = Element()->GetAnimatedTransformList();
  return (mIsAnimValItem &&
          mListIndex < alist->GetAnimValue().Length()) ||
         (!mIsAnimValItem &&
          mListIndex < alist->GetBaseValue().Length());
}
#endif // DEBUG


//----------------------------------------------------------------------
// Interface for DOMSVGMatrix's use

void
DOMSVGTransform::SetMatrix(const gfxMatrix& aMatrix)
{
  NS_ABORT_IF_FALSE(!mIsAnimValItem,
      "Attempting to modify read-only transform");
  Transform().SetMatrix(aMatrix);
  NotifyElementOfChange();
}

void
DOMSVGTransform::ClearMatrixTearoff(DOMSVGMatrix* aMatrix)
{
  NS_ABORT_IF_FALSE(mMatrixTearoff == aMatrix,
      "Unexpected matrix pointer to be cleared");
  mMatrixTearoff = nsnull;
}


//----------------------------------------------------------------------
// Implementation helpers

void
DOMSVGTransform::NotifyElementOfChange()
{
  if (HasOwner()) {
    Element()->DidChangeTransformList(PR_TRUE);
#ifdef MOZ_SMIL
    if (mList->mAList->IsAnimating()) {
      Element()->AnimationNeedsResample();
    }
#endif // MOZ_SMIL
  }
}

} // namespace mozilla

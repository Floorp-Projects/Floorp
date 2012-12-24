/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMSVGTransform.h"
#include "DOMSVGMatrix.h"
#include "SVGAnimatedTransformList.h"
#include "nsError.h"
#include <math.h>
#include "nsContentUtils.h"
#include "nsAttrValueInlines.h"
#include "mozilla/dom/SVGTransformBinding.h"

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
    tmp->mList->mItems[tmp->mListIndex] = nullptr;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK(mList)
NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DOMSVGTransform)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(DOMSVGTransform)
NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMSVGTransform)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMSVGTransform)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMSVGTransform)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(mozilla::DOMSVGTransform)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END


JSObject*
DOMSVGTransform::WrapObject(JSContext* aCx, JSObject* aScope, bool* aTriedToWrap)
{
  return mozilla::dom::SVGTransformBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

//----------------------------------------------------------------------
// Ctors:

DOMSVGTransform::DOMSVGTransform(DOMSVGTransformList *aList,
                                 uint32_t aListIndex,
                                 bool aIsAnimValItem)
  : mList(aList)
  , mListIndex(aListIndex)
  , mIsAnimValItem(aIsAnimValItem)
  , mTransform(nullptr)
  , mMatrixTearoff(nullptr)
{
  SetIsDOMBinding();
  // These shifts are in sync with the members in the header.
  NS_ABORT_IF_FALSE(aList &&
                    aListIndex <= MaxListIndex(), "bad arg");

  NS_ABORT_IF_FALSE(IndexIsValid(), "Bad index for DOMSVGNumber!");
}

DOMSVGTransform::DOMSVGTransform()
  : mList(nullptr)
  , mListIndex(0)
  , mIsAnimValItem(false)
  , mTransform(new SVGTransform()) // Default ctor for objects not in a list
                                   // initialises to matrix type with identity
                                   // matrix
  , mMatrixTearoff(nullptr)
{
  SetIsDOMBinding();
}

DOMSVGTransform::DOMSVGTransform(const gfxMatrix &aMatrix)
  : mList(nullptr)
  , mListIndex(0)
  , mIsAnimValItem(false)
  , mTransform(new SVGTransform(aMatrix))
  , mMatrixTearoff(nullptr)
{
  SetIsDOMBinding();
}

DOMSVGTransform::DOMSVGTransform(const SVGTransform &aTransform)
  : mList(nullptr)
  , mListIndex(0)
  , mIsAnimValItem(false)
  , mTransform(new SVGTransform(aTransform))
  , mMatrixTearoff(nullptr)
{
  SetIsDOMBinding();
}


uint16_t
DOMSVGTransform::Type() const
{
  return Transform().Type();
}

already_AddRefed<DOMSVGMatrix>
DOMSVGTransform::Matrix()
{
  if (!mMatrixTearoff) {
    mMatrixTearoff = new DOMSVGMatrix(*this);
  }
  nsRefPtr<DOMSVGMatrix> matrix = mMatrixTearoff;
  return matrix.forget();
}

float
DOMSVGTransform::Angle() const
{
  return Transform().Angle();
}

void
DOMSVGTransform::SetMatrix(DOMSVGMatrix& aMatrix, ErrorResult& rv)
{
  if (mIsAnimValItem) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }
  SetMatrix(aMatrix.Matrix());
}

void
DOMSVGTransform::SetTranslate(float tx, float ty, ErrorResult& rv)
{
  if (mIsAnimValItem) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  if (Transform().Type() == SVG_TRANSFORM_TRANSLATE &&
      Matrixgfx().x0 == tx && Matrixgfx().y0 == ty) {
    return;
  }

  nsAttrValue emptyOrOldValue = NotifyElementWillChange();
  Transform().SetTranslate(tx, ty);
  NotifyElementDidChange(emptyOrOldValue);
}

void
DOMSVGTransform::SetScale(float sx, float sy, ErrorResult& rv)
{
  if (mIsAnimValItem) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  if (Transform().Type() == SVG_TRANSFORM_SCALE &&
      Matrixgfx().xx == sx && Matrixgfx().yy == sy) {
    return;
  }
  nsAttrValue emptyOrOldValue = NotifyElementWillChange();
  Transform().SetScale(sx, sy);
  NotifyElementDidChange(emptyOrOldValue);
}

void
DOMSVGTransform::SetRotate(float angle, float cx, float cy, ErrorResult& rv)
{
  if (mIsAnimValItem) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  if (Transform().Type() == SVG_TRANSFORM_ROTATE) {
    float currentCx, currentCy;
    Transform().GetRotationOrigin(currentCx, currentCy);
    if (Transform().Angle() == angle && currentCx == cx && currentCy == cy) {
      return;
    }
  }

  nsAttrValue emptyOrOldValue = NotifyElementWillChange();
  Transform().SetRotate(angle, cx, cy);
  NotifyElementDidChange(emptyOrOldValue);
}

void
DOMSVGTransform::SetSkewX(float angle, ErrorResult& rv)
{
  if (mIsAnimValItem) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  if (Transform().Type() == SVG_TRANSFORM_SKEWX &&
      Transform().Angle() == angle) {
    return;
  }

  nsAttrValue emptyOrOldValue = NotifyElementWillChange();
  nsresult result = Transform().SetSkewX(angle);
  if (NS_FAILED(result)) {
    rv.Throw(result);
    return;
  }
  NotifyElementDidChange(emptyOrOldValue);
}

void
DOMSVGTransform::SetSkewY(float angle, ErrorResult& rv)
{
  if (mIsAnimValItem) {
    rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  if (Transform().Type() == SVG_TRANSFORM_SKEWY &&
      Transform().Angle() == angle) {
    return;
  }

  nsAttrValue emptyOrOldValue = NotifyElementWillChange();
  nsresult result = Transform().SetSkewY(angle);
  if (NS_FAILED(result)) {
    rv.Throw(result);
    return;
  }
  NotifyElementDidChange(emptyOrOldValue);
}

//----------------------------------------------------------------------
// List management methods:

void
DOMSVGTransform::InsertingIntoList(DOMSVGTransformList *aList,
                                   uint32_t aListIndex,
                                   bool aIsAnimValItem)
{
  NS_ABORT_IF_FALSE(!HasOwner(), "Inserting item that is already in a list");

  mList = aList;
  mListIndex = aListIndex;
  mIsAnimValItem = aIsAnimValItem;
  mTransform = nullptr;

  NS_ABORT_IF_FALSE(IndexIsValid(), "Bad index for DOMSVGLength!");
}

void
DOMSVGTransform::RemovingFromList()
{
  NS_ABORT_IF_FALSE(!mTransform,
      "Item in list also has another non-list value associated with it");

  mTransform = new SVGTransform(InternalItem());
  mList = nullptr;
  mIsAnimValItem = false;
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

  if (Transform().Type() == SVG_TRANSFORM_MATRIX &&
      SVGTransform::MatricesEqual(Matrixgfx(), aMatrix)) {
    return;
  }

  nsAttrValue emptyOrOldValue = NotifyElementWillChange();
  Transform().SetMatrix(aMatrix);
  NotifyElementDidChange(emptyOrOldValue);
}

void
DOMSVGTransform::ClearMatrixTearoff(DOMSVGMatrix* aMatrix)
{
  NS_ABORT_IF_FALSE(mMatrixTearoff == aMatrix,
      "Unexpected matrix pointer to be cleared");
  mMatrixTearoff = nullptr;
}


//----------------------------------------------------------------------
// Implementation helpers

void
DOMSVGTransform::NotifyElementDidChange(const nsAttrValue& aEmptyOrOldValue)
{
  if (HasOwner()) {
    Element()->DidChangeTransformList(aEmptyOrOldValue);
    if (mList->mAList->IsAnimating()) {
      Element()->AnimationNeedsResample();
    }
  }
}

} // namespace mozilla

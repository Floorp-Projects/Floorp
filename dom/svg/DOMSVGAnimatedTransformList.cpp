/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMSVGAnimatedTransformList.h"

#include "DOMSVGTransformList.h"
#include "SVGAnimatedTransformList.h"
#include "SVGAttrTearoffTable.h"
#include "mozilla/dom/SVGAnimatedTransformListBinding.h"

namespace mozilla {
namespace dom {

static SVGAttrTearoffTable<SVGAnimatedTransformList,
                           DOMSVGAnimatedTransformList>
    sSVGAnimatedTransformListTearoffTable;

NS_SVG_VAL_IMPL_CYCLE_COLLECTION_WRAPPERCACHED(DOMSVGAnimatedTransformList,
                                               mElement)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(DOMSVGAnimatedTransformList, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(DOMSVGAnimatedTransformList, Release)

JSObject* DOMSVGAnimatedTransformList::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return SVGAnimatedTransformList_Binding::Wrap(aCx, this, aGivenProto);
}

//----------------------------------------------------------------------
already_AddRefed<DOMSVGTransformList> DOMSVGAnimatedTransformList::BaseVal() {
  if (!mBaseVal) {
    mBaseVal = new DOMSVGTransformList(this, InternalAList().GetBaseValue());
  }
  RefPtr<DOMSVGTransformList> baseVal = mBaseVal;
  return baseVal.forget();
}

already_AddRefed<DOMSVGTransformList> DOMSVGAnimatedTransformList::AnimVal() {
  if (!mAnimVal) {
    mAnimVal = new DOMSVGTransformList(this, InternalAList().GetAnimValue());
  }
  RefPtr<DOMSVGTransformList> animVal = mAnimVal;
  return animVal.forget();
}

/* static */ already_AddRefed<DOMSVGAnimatedTransformList>
DOMSVGAnimatedTransformList::GetDOMWrapper(SVGAnimatedTransformList* aList,
                                           SVGElement* aElement) {
  RefPtr<DOMSVGAnimatedTransformList> wrapper =
      sSVGAnimatedTransformListTearoffTable.GetTearoff(aList);
  if (!wrapper) {
    wrapper = new DOMSVGAnimatedTransformList(aElement);
    sSVGAnimatedTransformListTearoffTable.AddTearoff(aList, wrapper);
  }
  return wrapper.forget();
}

/* static */ DOMSVGAnimatedTransformList*
DOMSVGAnimatedTransformList::GetDOMWrapperIfExists(
    SVGAnimatedTransformList* aList) {
  return sSVGAnimatedTransformListTearoffTable.GetTearoff(aList);
}

DOMSVGAnimatedTransformList::~DOMSVGAnimatedTransformList() {
  // Script no longer has any references to us, to our base/animVal objects, or
  // to any of their list items.
  sSVGAnimatedTransformListTearoffTable.RemoveTearoff(&InternalAList());
}

void DOMSVGAnimatedTransformList::InternalBaseValListWillChangeLengthTo(
    uint32_t aNewLength) {
  // When the number of items in our internal counterpart's baseVal changes,
  // we MUST keep our baseVal in sync. If we don't, script will either see a
  // list that is too short and be unable to access indexes that should be
  // valid, or else, MUCH WORSE, script will see a list that is too long and be
  // able to access "items" at indexes that are out of bounds (read/write to
  // bad memory)!!

  RefPtr<DOMSVGAnimatedTransformList> kungFuDeathGrip;
  if (mBaseVal) {
    if (aNewLength < mBaseVal->LengthNoFlush()) {
      // InternalListLengthWillChange might clear last reference to |this|.
      // Retain a temporary reference to keep from dying before returning.
      kungFuDeathGrip = this;
    }
    mBaseVal->InternalListLengthWillChange(aNewLength);
  }

  // If our attribute is not animating, then our animVal mirrors our baseVal
  // and we must sync its length too. (If our attribute is animating, then the
  // SMIL engine takes care of calling InternalAnimValListWillChangeLengthTo()
  // if necessary.)

  if (!IsAnimating()) {
    InternalAnimValListWillChangeLengthTo(aNewLength);
  }
}

void DOMSVGAnimatedTransformList::InternalAnimValListWillChangeLengthTo(
    uint32_t aNewLength) {
  if (mAnimVal) {
    mAnimVal->InternalListLengthWillChange(aNewLength);
  }
}

bool DOMSVGAnimatedTransformList::IsAnimating() const {
  return InternalAList().IsAnimating();
}

SVGAnimatedTransformList& DOMSVGAnimatedTransformList::InternalAList() {
  return *mElement->GetAnimatedTransformList();
}

const SVGAnimatedTransformList& DOMSVGAnimatedTransformList::InternalAList()
    const {
  return *mElement->GetAnimatedTransformList();
}

}  // namespace dom
}  // namespace mozilla

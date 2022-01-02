/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMSVGAnimatedNumberList.h"

#include "DOMSVGNumberList.h"
#include "SVGAnimatedNumberList.h"
#include "SVGAttrTearoffTable.h"
#include "mozilla/dom/SVGAnimatedNumberListBinding.h"
#include "mozilla/dom/SVGElement.h"
#include "mozilla/RefPtr.h"

// See the architecture comment in this file's header.

namespace mozilla {
namespace dom {

static inline SVGAttrTearoffTable<SVGAnimatedNumberList,
                                  DOMSVGAnimatedNumberList>&
SVGAnimatedNumberListTearoffTable() {
  static SVGAttrTearoffTable<SVGAnimatedNumberList, DOMSVGAnimatedNumberList>
      sSVGAnimatedNumberListTearoffTable;
  return sSVGAnimatedNumberListTearoffTable;
}

NS_SVG_VAL_IMPL_CYCLE_COLLECTION_WRAPPERCACHED(DOMSVGAnimatedNumberList,
                                               mElement)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(DOMSVGAnimatedNumberList, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(DOMSVGAnimatedNumberList, Release)

JSObject* DOMSVGAnimatedNumberList::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return mozilla::dom::SVGAnimatedNumberList_Binding::Wrap(aCx, this,
                                                           aGivenProto);
}

already_AddRefed<DOMSVGNumberList> DOMSVGAnimatedNumberList::BaseVal() {
  if (!mBaseVal) {
    mBaseVal = new DOMSVGNumberList(this, InternalAList().GetBaseValue());
  }
  RefPtr<DOMSVGNumberList> baseVal = mBaseVal;
  return baseVal.forget();
}

already_AddRefed<DOMSVGNumberList> DOMSVGAnimatedNumberList::AnimVal() {
  if (!mAnimVal) {
    mAnimVal = new DOMSVGNumberList(this, InternalAList().GetAnimValue());
  }
  RefPtr<DOMSVGNumberList> animVal = mAnimVal;
  return animVal.forget();
}

/* static */
already_AddRefed<DOMSVGAnimatedNumberList>
DOMSVGAnimatedNumberList::GetDOMWrapper(SVGAnimatedNumberList* aList,
                                        dom::SVGElement* aElement,
                                        uint8_t aAttrEnum) {
  RefPtr<DOMSVGAnimatedNumberList> wrapper =
      SVGAnimatedNumberListTearoffTable().GetTearoff(aList);
  if (!wrapper) {
    wrapper = new DOMSVGAnimatedNumberList(aElement, aAttrEnum);
    SVGAnimatedNumberListTearoffTable().AddTearoff(aList, wrapper);
  }
  return wrapper.forget();
}

/* static */
DOMSVGAnimatedNumberList* DOMSVGAnimatedNumberList::GetDOMWrapperIfExists(
    SVGAnimatedNumberList* aList) {
  return SVGAnimatedNumberListTearoffTable().GetTearoff(aList);
}

DOMSVGAnimatedNumberList::~DOMSVGAnimatedNumberList() {
  // Script no longer has any references to us, to our base/animVal objects, or
  // to any of their list items.
  SVGAnimatedNumberListTearoffTable().RemoveTearoff(&InternalAList());
}

void DOMSVGAnimatedNumberList::InternalBaseValListWillChangeTo(
    const SVGNumberList& aNewValue) {
  // When the number of items in our internal counterpart's baseVal changes,
  // we MUST keep our baseVal in sync. If we don't, script will either see a
  // list that is too short and be unable to access indexes that should be
  // valid, or else, MUCH WORSE, script will see a list that is too long and be
  // able to access "items" at indexes that are out of bounds (read/write to
  // bad memory)!!

  RefPtr<DOMSVGAnimatedNumberList> kungFuDeathGrip;
  if (mBaseVal) {
    if (aNewValue.Length() < mBaseVal->LengthNoFlush()) {
      // InternalListLengthWillChange might clear last reference to |this|.
      // Retain a temporary reference to keep from dying before returning.
      kungFuDeathGrip = this;
    }
    mBaseVal->InternalListLengthWillChange(aNewValue.Length());
  }

  // If our attribute is not animating, then our animVal mirrors our baseVal
  // and we must sync its length too. (If our attribute is animating, then the
  // SMIL engine takes care of calling InternalAnimValListWillChangeTo() if
  // necessary.)

  if (!IsAnimating()) {
    InternalAnimValListWillChangeTo(aNewValue);
  }
}

void DOMSVGAnimatedNumberList::InternalAnimValListWillChangeTo(
    const SVGNumberList& aNewValue) {
  if (mAnimVal) {
    mAnimVal->InternalListLengthWillChange(aNewValue.Length());
  }
}

bool DOMSVGAnimatedNumberList::IsAnimating() const {
  return InternalAList().IsAnimating();
}

SVGAnimatedNumberList& DOMSVGAnimatedNumberList::InternalAList() {
  return *mElement->GetAnimatedNumberList(mAttrEnum);
}

const SVGAnimatedNumberList& DOMSVGAnimatedNumberList::InternalAList() const {
  return *mElement->GetAnimatedNumberList(mAttrEnum);
}

}  // namespace dom
}  // namespace mozilla

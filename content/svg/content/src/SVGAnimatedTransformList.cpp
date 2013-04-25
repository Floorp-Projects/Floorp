/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGAnimatedTransformList.h"
#include "DOMSVGTransformList.h"
#include "nsSVGAnimatedTransformList.h"
#include "nsSVGAttrTearoffTable.h"
#include "mozilla/dom/SVGAnimatedTransformListBinding.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

static
  nsSVGAttrTearoffTable<nsSVGAnimatedTransformList, SVGAnimatedTransformList>
  sSVGAnimatedTransformListTearoffTable;

NS_SVG_VAL_IMPL_CYCLE_COLLECTION_WRAPPERCACHED(SVGAnimatedTransformList, mElement)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(SVGAnimatedTransformList, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(SVGAnimatedTransformList, Release)

JSObject*
SVGAnimatedTransformList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return SVGAnimatedTransformListBinding::Wrap(aCx, aScope, this);
}

//----------------------------------------------------------------------
already_AddRefed<DOMSVGTransformList>
SVGAnimatedTransformList::BaseVal()
{
  if (!mBaseVal) {
    mBaseVal = new DOMSVGTransformList(this, InternalAList().GetBaseValue());
  }
  nsRefPtr<DOMSVGTransformList> baseVal = mBaseVal;
  return baseVal.forget();
}

already_AddRefed<DOMSVGTransformList>
SVGAnimatedTransformList::AnimVal()
{
  if (!mAnimVal) {
    mAnimVal = new DOMSVGTransformList(this, InternalAList().GetAnimValue());
  }
  nsRefPtr<DOMSVGTransformList> animVal = mAnimVal;
  return animVal.forget();
}

/* static */ already_AddRefed<SVGAnimatedTransformList>
SVGAnimatedTransformList::GetDOMWrapper(nsSVGAnimatedTransformList *aList,
                                        nsSVGElement *aElement)
{
  nsRefPtr<SVGAnimatedTransformList> wrapper =
    sSVGAnimatedTransformListTearoffTable.GetTearoff(aList);
  if (!wrapper) {
    wrapper = new SVGAnimatedTransformList(aElement);
    sSVGAnimatedTransformListTearoffTable.AddTearoff(aList, wrapper);
  }
  return wrapper.forget();
}

/* static */ SVGAnimatedTransformList*
SVGAnimatedTransformList::GetDOMWrapperIfExists(
  nsSVGAnimatedTransformList *aList)
{
  return sSVGAnimatedTransformListTearoffTable.GetTearoff(aList);
}

SVGAnimatedTransformList::~SVGAnimatedTransformList()
{
  // Script no longer has any references to us, to our base/animVal objects, or
  // to any of their list items.
  sSVGAnimatedTransformListTearoffTable.RemoveTearoff(&InternalAList());
}

void
SVGAnimatedTransformList::InternalBaseValListWillChangeLengthTo(
  uint32_t aNewLength)
{
  // When the number of items in our internal counterpart's baseVal changes,
  // we MUST keep our baseVal in sync. If we don't, script will either see a
  // list that is too short and be unable to access indexes that should be
  // valid, or else, MUCH WORSE, script will see a list that is too long and be
  // able to access "items" at indexes that are out of bounds (read/write to
  // bad memory)!!

  nsRefPtr<SVGAnimatedTransformList> kungFuDeathGrip;
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

void
SVGAnimatedTransformList::InternalAnimValListWillChangeLengthTo(
  uint32_t aNewLength)
{
  if (mAnimVal) {
    mAnimVal->InternalListLengthWillChange(aNewLength);
  }
}

bool
SVGAnimatedTransformList::IsAnimating() const
{
  return InternalAList().IsAnimating();
}

nsSVGAnimatedTransformList&
SVGAnimatedTransformList::InternalAList()
{
  return *mElement->GetAnimatedTransformList();
}

const nsSVGAnimatedTransformList&
SVGAnimatedTransformList::InternalAList() const
{
  return *mElement->GetAnimatedTransformList();
}

} // namespace dom
} // namespace mozilla

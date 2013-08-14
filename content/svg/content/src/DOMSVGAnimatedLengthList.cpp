/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMSVGAnimatedLengthList.h"
#include "DOMSVGLengthList.h"
#include "SVGAnimatedLengthList.h"
#include "nsSVGElement.h"
#include "nsCOMPtr.h"
#include "nsSVGAttrTearoffTable.h"
#include "mozilla/dom/SVGAnimatedLengthListBinding.h"

// See the architecture comment in this file's header.

namespace mozilla {

static inline
nsSVGAttrTearoffTable<SVGAnimatedLengthList, DOMSVGAnimatedLengthList>&
SVGAnimatedLengthListTearoffTable()
{
  static nsSVGAttrTearoffTable<SVGAnimatedLengthList, DOMSVGAnimatedLengthList>
    sSVGAnimatedLengthListTearoffTable;
  return sSVGAnimatedLengthListTearoffTable;
}

NS_SVG_VAL_IMPL_CYCLE_COLLECTION_WRAPPERCACHED(DOMSVGAnimatedLengthList, mElement)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(DOMSVGAnimatedLengthList, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(DOMSVGAnimatedLengthList, Release)

JSObject*
DOMSVGAnimatedLengthList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return dom::SVGAnimatedLengthListBinding::Wrap(aCx, aScope, this);
}

already_AddRefed<DOMSVGLengthList>
DOMSVGAnimatedLengthList::BaseVal()
{
  if (!mBaseVal) {
    mBaseVal = new DOMSVGLengthList(this, InternalAList().GetBaseValue());
  }
  nsRefPtr<DOMSVGLengthList> baseVal = mBaseVal;
  return baseVal.forget();
}

already_AddRefed<DOMSVGLengthList>
DOMSVGAnimatedLengthList::AnimVal()
{
  if (!mAnimVal) {
    mAnimVal = new DOMSVGLengthList(this, InternalAList().GetAnimValue());
  }
  nsRefPtr<DOMSVGLengthList> animVal = mAnimVal;
  return animVal.forget();
}

/* static */ already_AddRefed<DOMSVGAnimatedLengthList>
DOMSVGAnimatedLengthList::GetDOMWrapper(SVGAnimatedLengthList *aList,
                                        nsSVGElement *aElement,
                                        uint8_t aAttrEnum,
                                        uint8_t aAxis)
{
  nsRefPtr<DOMSVGAnimatedLengthList> wrapper =
    SVGAnimatedLengthListTearoffTable().GetTearoff(aList);
  if (!wrapper) {
    wrapper = new DOMSVGAnimatedLengthList(aElement, aAttrEnum, aAxis);
    SVGAnimatedLengthListTearoffTable().AddTearoff(aList, wrapper);
  }
  return wrapper.forget();
}

/* static */ DOMSVGAnimatedLengthList*
DOMSVGAnimatedLengthList::GetDOMWrapperIfExists(SVGAnimatedLengthList *aList)
{
  return SVGAnimatedLengthListTearoffTable().GetTearoff(aList);
}

DOMSVGAnimatedLengthList::~DOMSVGAnimatedLengthList()
{
  // Script no longer has any references to us, to our base/animVal objects, or
  // to any of their list items.
  SVGAnimatedLengthListTearoffTable().RemoveTearoff(&InternalAList());
}

void
DOMSVGAnimatedLengthList::InternalBaseValListWillChangeTo(const SVGLengthList& aNewValue)
{
  // When the number of items in our internal counterpart's baseVal changes,
  // we MUST keep our baseVal in sync. If we don't, script will either see a
  // list that is too short and be unable to access indexes that should be
  // valid, or else, MUCH WORSE, script will see a list that is too long and be
  // able to access "items" at indexes that are out of bounds (read/write to
  // bad memory)!!

  nsRefPtr<DOMSVGAnimatedLengthList> kungFuDeathGrip;
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

void
DOMSVGAnimatedLengthList::InternalAnimValListWillChangeTo(const SVGLengthList& aNewValue)
{
  if (mAnimVal) {
    mAnimVal->InternalListLengthWillChange(aNewValue.Length());
  }
}

bool
DOMSVGAnimatedLengthList::IsAnimating() const
{
  return InternalAList().IsAnimating();
}

SVGAnimatedLengthList&
DOMSVGAnimatedLengthList::InternalAList()
{
  return *mElement->GetAnimatedLengthList(mAttrEnum);
}

const SVGAnimatedLengthList&
DOMSVGAnimatedLengthList::InternalAList() const
{
  return *mElement->GetAnimatedLengthList(mAttrEnum);
}

} // namespace mozilla

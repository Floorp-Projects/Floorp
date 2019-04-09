/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMSVGAnimatedLength.h"

#include "mozilla/dom/SVGAnimatedLengthBinding.h"
#include "SVGAnimatedLength.h"
#include "DOMSVGLength.h"

namespace mozilla {
namespace dom {

NS_SVG_VAL_IMPL_CYCLE_COLLECTION_WRAPPERCACHED(DOMSVGAnimatedLength,
                                               mSVGElement)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(DOMSVGAnimatedLength, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(DOMSVGAnimatedLength, Release)

JSObject* DOMSVGAnimatedLength::WrapObject(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto) {
  return SVGAnimatedLength_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<DOMSVGLength> DOMSVGAnimatedLength::BaseVal() {
  return mVal->ToDOMBaseVal(mSVGElement);
}

already_AddRefed<DOMSVGLength> DOMSVGAnimatedLength::AnimVal() {
  return mVal->ToDOMAnimVal(mSVGElement);
}

}  // namespace dom
}  // namespace mozilla

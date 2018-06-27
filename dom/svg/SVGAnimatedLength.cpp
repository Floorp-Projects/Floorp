/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGAnimatedLength.h"
#include "mozilla/dom/SVGAnimatedLengthBinding.h"
#include "nsSVGLength2.h"
#include "DOMSVGLength.h"

namespace mozilla {
namespace dom {

NS_SVG_VAL_IMPL_CYCLE_COLLECTION_WRAPPERCACHED(SVGAnimatedLength, mSVGElement)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(SVGAnimatedLength, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(SVGAnimatedLength, Release)

JSObject*
SVGAnimatedLength::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGAnimatedLength_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<DOMSVGLength>
SVGAnimatedLength::BaseVal()
{
  RefPtr<DOMSVGLength> angle;
  mVal->ToDOMBaseVal(getter_AddRefs(angle), mSVGElement);
  return angle.forget();
}

already_AddRefed<DOMSVGLength>
SVGAnimatedLength::AnimVal()
{
  RefPtr<DOMSVGLength> angle;
  mVal->ToDOMAnimVal(getter_AddRefs(angle), mSVGElement);
  return angle.forget();
}

} // namespace dom
} // namespace mozilla

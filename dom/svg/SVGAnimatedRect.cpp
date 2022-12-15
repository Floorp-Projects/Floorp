/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGAnimatedRect.h"
#include "mozilla/dom/SVGAnimatedRectBinding.h"
#include "mozilla/dom/SVGElement.h"
#include "mozilla/dom/SVGRect.h"
#include "SVGAnimatedViewBox.h"

namespace mozilla::dom {

NS_SVG_VAL_IMPL_CYCLE_COLLECTION_WRAPPERCACHED(SVGAnimatedRect, mSVGElement)

SVGAnimatedRect::SVGAnimatedRect(SVGAnimatedViewBox* aVal,
                                 SVGElement* aSVGElement)
    : mVal(aVal), mSVGElement(aSVGElement) {}

SVGAnimatedRect::~SVGAnimatedRect() {
  SVGAnimatedViewBox::sSVGAnimatedRectTearoffTable.RemoveTearoff(mVal);
}

already_AddRefed<SVGRect> SVGAnimatedRect::GetBaseVal() {
  return mVal->ToDOMBaseVal(mSVGElement);
}

already_AddRefed<SVGRect> SVGAnimatedRect::GetAnimVal() {
  return mVal->ToDOMAnimVal(mSVGElement);
}

JSObject* SVGAnimatedRect::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return SVGAnimatedRect_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom

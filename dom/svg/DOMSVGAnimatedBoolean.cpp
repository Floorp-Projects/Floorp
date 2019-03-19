/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMSVGAnimatedBoolean.h"

#include "mozilla/dom/SVGAnimatedBooleanBinding.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_SVG_VAL_IMPL_CYCLE_COLLECTION_WRAPPERCACHED(DOMSVGAnimatedBoolean,
                                               mSVGElement)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(DOMSVGAnimatedBoolean, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(DOMSVGAnimatedBoolean, Release)

JSObject* DOMSVGAnimatedBoolean::WrapObject(JSContext* aCx,
                                            JS::Handle<JSObject*> aGivenProto) {
  return SVGAnimatedBoolean_Binding::Wrap(aCx, this, aGivenProto);
}

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGAnimatedBoolean.h"
#include "mozilla/dom/SVGAnimatedBooleanBinding.h"
#include "nsContentUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_SVG_VAL_IMPL_CYCLE_COLLECTION_WRAPPERCACHED(SVGAnimatedBoolean, mSVGElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(SVGAnimatedBoolean)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SVGAnimatedBoolean)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SVGAnimatedBoolean)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
SVGAnimatedBoolean::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return SVGAnimatedBooleanBinding::Wrap(aCx, aScope, this);
}


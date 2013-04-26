/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGAnimatedAngle.h"
#include "nsSVGAngle.h"
#include "mozilla/dom/SVGAnimatedAngleBinding.h"
#include "nsContentUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_SVG_VAL_IMPL_CYCLE_COLLECTION_WRAPPERCACHED(SVGAnimatedAngle, mSVGElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(SVGAnimatedAngle)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SVGAnimatedAngle)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SVGAnimatedAngle)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
SVGAnimatedAngle::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return SVGAnimatedAngleBinding::Wrap(aCx, aScope, this);
}

already_AddRefed<SVGAngle>
SVGAnimatedAngle::BaseVal()
{
  return mVal->ToDOMBaseVal(mSVGElement);
}

already_AddRefed<SVGAngle>
SVGAnimatedAngle::AnimVal()
{
  return mVal->ToDOMAnimVal(mSVGElement);
}


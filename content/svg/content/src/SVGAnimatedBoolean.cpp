/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGAnimatedBoolean.h"
#include "nsSVGBoolean.h"
#include "mozilla/dom/SVGAnimatedBooleanBinding.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTING_ADDREF(SVGAnimatedBoolean)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SVGAnimatedBoolean)

NS_IMPL_CYCLE_COLLECTION_CLASS(SVGAnimatedBoolean)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(SVGAnimatedBoolean)
// No unlinking mElement, we'd need to null out the value pointer (the object it
// points to is held by the element) and null-check it everywhere.
NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(SVGAnimatedBoolean)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSVGElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(SVGAnimatedBoolean)
NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SVGAnimatedBoolean)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
SVGAnimatedBoolean::WrapObject(JSContext* aCx, JSObject* aScope, bool* aTriedToWrap)
{
  return SVGAnimatedBooleanBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}


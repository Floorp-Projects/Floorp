/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DOMRect.h"

#include "nsPresContext.h"
#include "mozilla/dom/DOMRectListBinding.h"
#include "mozilla/dom/DOMRectBinding.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DOMRectReadOnly, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMRectReadOnly)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMRectReadOnly)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMRectReadOnly)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
DOMRectReadOnly::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  MOZ_ASSERT(mParent);
  return DOMRectReadOnly_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<DOMRectReadOnly>
DOMRectReadOnly::Constructor(const GlobalObject& aGlobal, double aX, double aY,
                             double aWidth, double aHeight, ErrorResult& aRv)
{
  RefPtr<DOMRectReadOnly> obj =
    new DOMRectReadOnly(aGlobal.GetAsSupports(), aX, aY, aWidth, aHeight);
  return obj.forget();
}

// -----------------------------------------------------------------------------

JSObject*
DOMRect::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  MOZ_ASSERT(mParent);
  return DOMRect_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<DOMRect>
DOMRect::Constructor(const GlobalObject& aGlobal, double aX, double aY,
                     double aWidth, double aHeight, ErrorResult& aRv)
{
  RefPtr<DOMRect> obj =
    new DOMRect(aGlobal.GetAsSupports(), aX, aY, aWidth, aHeight);
  return obj.forget();
}

// -----------------------------------------------------------------------------

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DOMRectList, mParent, mArray)

NS_INTERFACE_TABLE_HEAD(DOMRectList)
  NS_WRAPPERCACHE_INTERFACE_TABLE_ENTRY
  NS_INTERFACE_TABLE0(DOMRectList)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(DOMRectList)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMRectList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMRectList)

JSObject*
DOMRectList::WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto)
{
  return mozilla::dom::DOMRectList_Binding::Wrap(cx, this, aGivenProto);
}

static double
RoundFloat(double aValue)
{
  return floor(aValue + 0.5);
}

void
DOMRect::SetLayoutRect(const nsRect& aLayoutRect)
{
  double scale = 65536.0;
  // Round to the nearest 1/scale units. We choose scale so it can be represented
  // exactly by machine floating point.
  double scaleInv = 1/scale;
  double t2pScaled = scale/nsPresContext::AppUnitsPerCSSPixel();
  double x = RoundFloat(aLayoutRect.x*t2pScaled)*scaleInv;
  double y = RoundFloat(aLayoutRect.y*t2pScaled)*scaleInv;
  SetRect(x, y, RoundFloat(aLayoutRect.XMost()*t2pScaled)*scaleInv - x,
          RoundFloat(aLayoutRect.YMost()*t2pScaled)*scaleInv - y);
}

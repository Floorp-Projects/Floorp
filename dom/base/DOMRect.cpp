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

JSObject* DOMRectReadOnly::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  MOZ_ASSERT(mParent);
  return DOMRectReadOnly_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<DOMRectReadOnly> DOMRectReadOnly::FromRect(
    const GlobalObject& aGlobal, const DOMRectInit& aInit) {
  RefPtr<DOMRectReadOnly> obj = new DOMRectReadOnly(
      aGlobal.GetAsSupports(), aInit.mX, aInit.mY, aInit.mWidth, aInit.mHeight);
  return obj.forget();
}

already_AddRefed<DOMRectReadOnly> DOMRectReadOnly::Constructor(
    const GlobalObject& aGlobal, double aX, double aY, double aWidth,
    double aHeight) {
  RefPtr<DOMRectReadOnly> obj =
      new DOMRectReadOnly(aGlobal.GetAsSupports(), aX, aY, aWidth, aHeight);
  return obj.forget();
}

// https://drafts.fxtf.org/geometry/#structured-serialization
bool DOMRectReadOnly::WriteStructuredClone(
    JSContext* aCx, JSStructuredCloneWriter* aWriter) const {
#define WriteDouble(d)                                                       \
  JS_WriteUint32Pair(aWriter, (BitwiseCast<uint64_t>(d) >> 32) & 0xffffffff, \
                     BitwiseCast<uint64_t>(d) & 0xffffffff)

  return WriteDouble(mX) && WriteDouble(mY) && WriteDouble(mWidth) &&
         WriteDouble(mHeight);

#undef WriteDouble
}

// static
already_AddRefed<DOMRectReadOnly> DOMRectReadOnly::ReadStructuredClone(
    JSContext* aCx, nsIGlobalObject* aGlobal,
    JSStructuredCloneReader* aReader) {
  RefPtr<DOMRectReadOnly> retval = new DOMRectReadOnly(aGlobal);
  if (!retval->ReadStructuredClone(aReader)) {
    return nullptr;
  }
  return retval.forget();
}

bool DOMRectReadOnly::ReadStructuredClone(JSStructuredCloneReader* aReader) {
  uint32_t high;
  uint32_t low;

#define ReadDouble(d)                             \
  if (!JS_ReadUint32Pair(aReader, &high, &low)) { \
    return false;                                 \
  }                                               \
  (*(d) = BitwiseCast<double>(static_cast<uint64_t>(high) << 32 | low))

  ReadDouble(&mX);
  ReadDouble(&mY);
  ReadDouble(&mWidth);
  ReadDouble(&mHeight);

  return true;

#undef ReadDouble
}

// -----------------------------------------------------------------------------

JSObject* DOMRect::WrapObject(JSContext* aCx,
                              JS::Handle<JSObject*> aGivenProto) {
  MOZ_ASSERT(mParent);
  return DOMRect_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<DOMRect> DOMRect::FromRect(const GlobalObject& aGlobal,
                                            const DOMRectInit& aInit) {
  RefPtr<DOMRect> obj = new DOMRect(aGlobal.GetAsSupports(), aInit.mX, aInit.mY,
                                    aInit.mWidth, aInit.mHeight);
  return obj.forget();
}

already_AddRefed<DOMRect> DOMRect::Constructor(const GlobalObject& aGlobal,
                                               double aX, double aY,
                                               double aWidth, double aHeight) {
  RefPtr<DOMRect> obj =
      new DOMRect(aGlobal.GetAsSupports(), aX, aY, aWidth, aHeight);
  return obj.forget();
}

// static
already_AddRefed<DOMRect> DOMRect::ReadStructuredClone(
    JSContext* aCx, nsIGlobalObject* aGlobal,
    JSStructuredCloneReader* aReader) {
  RefPtr<DOMRect> retval = new DOMRect(aGlobal);
  if (!retval->ReadStructuredClone(aReader)) {
    return nullptr;
  }
  return retval.forget();
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

JSObject* DOMRectList::WrapObject(JSContext* cx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return mozilla::dom::DOMRectList_Binding::Wrap(cx, this, aGivenProto);
}

static double RoundFloat(double aValue) { return floor(aValue + 0.5); }

void DOMRect::SetLayoutRect(const nsRect& aLayoutRect) {
  double scale = 65536.0;
  // Round to the nearest 1/scale units. We choose scale so it can be
  // represented exactly by machine floating point.
  double scaleInv = 1 / scale;
  double t2pScaled = scale / AppUnitsPerCSSPixel();
  double x = RoundFloat(aLayoutRect.x * t2pScaled) * scaleInv;
  double y = RoundFloat(aLayoutRect.y * t2pScaled) * scaleInv;
  SetRect(x, y, RoundFloat(aLayoutRect.XMost() * t2pScaled) * scaleInv - x,
          RoundFloat(aLayoutRect.YMost() * t2pScaled) * scaleInv - y);
}

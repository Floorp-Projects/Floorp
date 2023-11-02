/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/dom/Element.h"
#include "nsContentUtils.h"
#include "nsRFPService.h"
#include "Performance.h"
#include "PerformanceMainThread.h"
#include "LargestContentfulPaint.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(LargestContentfulPaint, PerformanceEntry,
                                   mPerformance, mURI, mElement)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(LargestContentfulPaint)
NS_INTERFACE_MAP_END_INHERITING(PerformanceEntry)

NS_IMPL_ADDREF_INHERITED(LargestContentfulPaint, PerformanceEntry)
NS_IMPL_RELEASE_INHERITED(LargestContentfulPaint, PerformanceEntry)

LargestContentfulPaint::LargestContentfulPaint(Performance* aPerformance,
                                               DOMHighResTimeStamp aRenderTime,
                                               DOMHighResTimeStamp aLoadTime,
                                               unsigned long aSize,
                                               nsIURI* aURI, Element* aElement)
    : PerformanceEntry(aPerformance->GetParentObject(), u""_ns,
                       kLargestContentfulPaintName),
      mPerformance(static_cast<PerformanceMainThread*>(aPerformance)),
      mRenderTime(aRenderTime),
      mLoadTime(aLoadTime),
      mSize(aSize),
      mURI(aURI),
      mElement(aElement) {
  MOZ_ASSERT(mPerformance);
  MOZ_ASSERT(mElement);
  // The element could be a pseudo-element
  if (mElement->ChromeOnlyAccess()) {
    mElement = Element::FromNodeOrNull(
        aElement->FindFirstNonChromeOnlyAccessContent());
  }

  if (const Element* element = GetElement()) {
    mId = element->GetID();
  }
}

JSObject* LargestContentfulPaint::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return LargestContentfulPaint_Binding::Wrap(aCx, this, aGivenProto);
}

Element* LargestContentfulPaint::GetElement() const {
  return mElement ? nsContentUtils::GetAnElementForTiming(
                        mElement, mElement->GetComposedDoc(), nullptr)
                  : nullptr;
}

DOMHighResTimeStamp LargestContentfulPaint::RenderTime() const {
  return nsRFPService::ReduceTimePrecisionAsMSecs(
      mRenderTime, mPerformance->GetRandomTimelineSeed(),
      mPerformance->GetRTPCallerType());
}

DOMHighResTimeStamp LargestContentfulPaint::LoadTime() const {
  return nsRFPService::ReduceTimePrecisionAsMSecs(
      mLoadTime, mPerformance->GetRandomTimelineSeed(),
      mPerformance->GetRTPCallerType());
}

DOMHighResTimeStamp LargestContentfulPaint::StartTime() const {
  DOMHighResTimeStamp startTime = !mRenderTime ? mLoadTime : mRenderTime;
  return nsRFPService::ReduceTimePrecisionAsMSecs(
      startTime, mPerformance->GetRandomTimelineSeed(),
      mPerformance->GetRTPCallerType());
}

void LargestContentfulPaint::GetUrl(nsAString& aUrl) {
  if (mURI) {
    CopyUTF8toUTF16(mURI->GetSpecOrDefault(), aUrl);
  }
}
}  // namespace mozilla::dom

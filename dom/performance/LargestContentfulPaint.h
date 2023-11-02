/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_LargestContentfulPaint_h___
#define mozilla_dom_LargestContentfulPaint_h___

#include "nsCycleCollectionParticipant.h"
#include "mozilla/dom/PerformanceEntry.h"
#include "mozilla/dom/PerformanceLargestContentfulPaintBinding.h"
#include "nsIWeakReferenceUtils.h"

class nsTextFrame;
namespace mozilla::dom {

static constexpr nsLiteralString kLargestContentfulPaintName =
    u"largest-contentful-paint"_ns;

class Performance;
class PerformanceMainThread;

// https://w3c.github.io/largest-contentful-paint/
class LargestContentfulPaint final : public PerformanceEntry {
 public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(LargestContentfulPaint,
                                           PerformanceEntry)

  LargestContentfulPaint(Performance* aPerformance,
                         DOMHighResTimeStamp aRenderTime,
                         DOMHighResTimeStamp aLoadTime, unsigned long aSize,
                         nsIURI* aURI, Element* aElement);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  DOMHighResTimeStamp RenderTime() const;
  DOMHighResTimeStamp LoadTime() const;
  DOMHighResTimeStamp StartTime() const override;

  unsigned long Size() const { return mSize; }
  void GetId(nsAString& aId) const {
    if (mId) {
      mId->ToString(aId);
    }
  }
  void GetUrl(nsAString& aUrl);

  Element* GetElement() const;

 private:
  ~LargestContentfulPaint() = default;

  RefPtr<PerformanceMainThread> mPerformance;

  DOMHighResTimeStamp mRenderTime;
  DOMHighResTimeStamp mLoadTime;
  unsigned long mSize;
  nsCOMPtr<nsIURI> mURI;

  RefPtr<Element> mElement;
  RefPtr<nsAtom> mId;
};
}  // namespace mozilla::dom
#endif

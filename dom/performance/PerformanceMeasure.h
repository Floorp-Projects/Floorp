/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_performancemeasure_h___
#define mozilla_dom_performancemeasure_h___

#include "mozilla/dom/PerformanceEntry.h"

namespace mozilla::dom {

// http://www.w3.org/TR/user-timing/#performancemeasure
class PerformanceMeasure final : public PerformanceEntry {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(PerformanceMeasure,
                                                         PerformanceEntry);

  PerformanceMeasure(nsISupports* aParent, const nsAString& aName,
                     DOMHighResTimeStamp aStartTime,
                     DOMHighResTimeStamp aEndTime,
                     const JS::Handle<JS::Value>& aDetail);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  virtual DOMHighResTimeStamp StartTime() const override { return mStartTime; }

  virtual DOMHighResTimeStamp Duration() const override { return mDuration; }

  void GetDetail(JSContext* aCx, JS::MutableHandle<JS::Value> aRetval);

  size_t SizeOfIncludingThis(
      mozilla::MallocSizeOf aMallocSizeOf) const override;

 protected:
  virtual ~PerformanceMeasure();
  DOMHighResTimeStamp mStartTime;
  DOMHighResTimeStamp mDuration;

 private:
  JS::Heap<JS::Value> mDetail;
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_performancemeasure_h___ */

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_performancemark_h___
#define mozilla_dom_performancemark_h___

#include "mozilla/dom/PerformanceEntry.h"
#include "mozilla/ProfilerMarkers.h"

namespace mozilla::dom {

struct PerformanceMarkOptions;

// http://www.w3.org/TR/user-timing/#performancemark
class PerformanceMark final : public PerformanceEntry {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(PerformanceMark,
                                                         PerformanceEntry);

 private:
  PerformanceMark(nsISupports* aParent, const nsAString& aName,
                  DOMHighResTimeStamp aStartTime,
                  const JS::Handle<JS::Value>& aDetail,
                  DOMHighResTimeStamp aUnclampedStartTime);

 public:
  static already_AddRefed<PerformanceMark> Constructor(
      const GlobalObject& aGlobal, const nsAString& aMarkName,
      const PerformanceMarkOptions& aMarkOptions, ErrorResult& aRv);

  static already_AddRefed<PerformanceMark> Constructor(
      JSContext* aCx, nsIGlobalObject* aGlobal, const nsAString& aMarkName,
      const PerformanceMarkOptions& aMarkOptions, ErrorResult& aRv);

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  virtual DOMHighResTimeStamp StartTime() const override { return mStartTime; }

  virtual DOMHighResTimeStamp UnclampedStartTime() const override {
    MOZ_ASSERT(profiler_thread_is_being_profiled_for_markers(),
               "This should only be called when the Gecko Profiler is active.");
    return mUnclampedStartTime;
  }

  void GetDetail(JSContext* aCx, JS::MutableHandle<JS::Value> aRetval);

  size_t SizeOfIncludingThis(
      mozilla::MallocSizeOf aMallocSizeOf) const override;

 protected:
  virtual ~PerformanceMark();
  DOMHighResTimeStamp mStartTime;

 private:
  JS::Heap<JS::Value> mDetail;
  // This is used by the Gecko Profiler only to be able to add precise markers.
  // It's not exposed to JS
  DOMHighResTimeStamp mUnclampedStartTime;
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_performancemark_h___ */

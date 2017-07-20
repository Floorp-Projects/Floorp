/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PerformanceMainThread_h
#define mozilla_dom_PerformanceMainThread_h

#include "Performance.h"

namespace mozilla {
namespace dom {

class PerformanceMainThread final : public Performance
                                  , public nsIObserver
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIOBSERVER
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(PerformanceMainThread,
                                                         Performance)

  static already_AddRefed<PerformanceMainThread>
  Create(nsPIDOMWindowInner* aWindow, nsDOMNavigationTiming* aDOMTiming,
         nsITimedChannel* aChannel);

  virtual PerformanceTiming* Timing() override;

  virtual PerformanceNavigation* Navigation() override;

  virtual void AddEntry(nsIHttpChannel* channel,
                        nsITimedChannel* timedChannel) override;

  TimeStamp CreationTimeStamp() const override;

  DOMHighResTimeStamp CreationTime() const override;

  virtual void GetMozMemory(JSContext *aCx,
                            JS::MutableHandle<JSObject*> aObj) override;

  virtual nsDOMNavigationTiming* GetDOMTiming() const override
  {
    return mDOMTiming;
  }

  virtual nsITimedChannel* GetChannel() const override
  {
    return mChannel;
  }

  void Shutdown() override;

protected:
  PerformanceMainThread(nsPIDOMWindowInner* aWindow,
                        nsDOMNavigationTiming* aDOMTiming,
                        nsITimedChannel* aChannel);

  ~PerformanceMainThread();

  nsISupports* GetAsISupports() override
  {
    Performance* performance = this;
    return performance;
  }

  void InsertUserEntry(PerformanceEntry* aEntry) override;

  bool IsPerformanceTimingAttribute(const nsAString& aName) override;

  DOMHighResTimeStamp
  GetPerformanceTimingFromString(const nsAString& aTimingName) override;

  void DispatchBufferFullEvent() override;

  RefPtr<nsDOMNavigationTiming> mDOMTiming;
  nsCOMPtr<nsITimedChannel> mChannel;
  RefPtr<PerformanceTiming> mTiming;
  RefPtr<PerformanceNavigation> mNavigation;
  JS::Heap<JSObject*> mMozMemory;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PerformanceMainThread_h

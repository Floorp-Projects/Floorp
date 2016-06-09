/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PerformanceWorker_h
#define mozilla_dom_PerformanceWorker_h

#include "Performance.h"

namespace mozilla {
namespace dom {

namespace workers {
class WorkerPrivate;
}

class PerformanceWorker final : public Performance
{
public:
  PerformanceWorker(workers::WorkerPrivate* aWorkerPrivate);

  // Performance WebIDL methods
  DOMHighResTimeStamp Now() const override;

  virtual PerformanceTiming* Timing() override
  {
    MOZ_CRASH("This should not be called on workers.");
    return nullptr;
  }

  virtual PerformanceNavigation* Navigation() override
  {
    MOZ_CRASH("This should not be called on workers.");
    return nullptr;
  }

  virtual void AddEntry(nsIHttpChannel* channel,
                        nsITimedChannel* timedChannel) override
  {
    MOZ_CRASH("This should not be called on workers.");
  }

  TimeStamp CreationTimeStamp() const override;

  DOMHighResTimeStamp CreationTime() const override;

  virtual void GetMozMemory(JSContext *aCx,
                            JS::MutableHandle<JSObject*> aObj) override
  {
    MOZ_CRASH("This should not be called on workers.");
  }

  virtual nsDOMNavigationTiming* GetDOMTiming() const override
  {
    MOZ_CRASH("This should not be called on workers.");
    return nullptr;
  }

  virtual nsITimedChannel* GetChannel() const override
  {
    MOZ_CRASH("This should not be called on workers.");
    return nullptr;
  }

  virtual Performance* GetParentPerformance() const override
  {
    MOZ_CRASH("This should not be called on workers.");
    return nullptr;
  }

protected:
  ~PerformanceWorker();

  nsISupports* GetAsISupports() override
  {
    return nullptr;
  }

  void InsertUserEntry(PerformanceEntry* aEntry) override;

  bool IsPerformanceTimingAttribute(const nsAString& aName) override;

  DOMHighResTimeStamp
  GetPerformanceTimingFromString(const nsAString& aTimingName) override;

  void DispatchBufferFullEvent() override
  {
    MOZ_CRASH("This should not be called on workers.");
  }

private:
  workers::WorkerPrivate* mWorkerPrivate;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PerformanceWorker_h

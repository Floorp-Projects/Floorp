/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Performance_h
#define mozilla_dom_Performance_h

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "nsCOMPtr.h"
#include "nsDOMNavigationTiming.h"

class nsITimedChannel;
class nsIHttpChannel;

namespace mozilla {

class ErrorResult;

namespace dom {

class PerformanceEntry;
class PerformanceNavigation;
class PerformanceObserver;
class PerformanceTiming;

namespace workers {
class WorkerPrivate;
}

// Base class for main-thread and worker Performance API
class Performance : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(Performance,
                                           DOMEventTargetHelper)

  static bool IsEnabled(JSContext* aCx, JSObject* aGlobal);

  static bool IsObserverEnabled(JSContext* aCx, JSObject* aGlobal);

  static already_AddRefed<Performance>
  CreateForMainThread(nsPIDOMWindowInner* aWindow,
                      nsDOMNavigationTiming* aDOMTiming,
                      nsITimedChannel* aChannel,
                      Performance* aParentPerformance);

  static already_AddRefed<Performance>
  CreateForWorker(workers::WorkerPrivate* aWorkerPrivate);

  JSObject* WrapObject(JSContext *cx,
                       JS::Handle<JSObject*> aGivenProto) override;

  void GetEntries(nsTArray<RefPtr<PerformanceEntry>>& aRetval);

  void GetEntriesByType(const nsAString& aEntryType,
                        nsTArray<RefPtr<PerformanceEntry>>& aRetval);

  void GetEntriesByName(const nsAString& aName,
                        const Optional<nsAString>& aEntryType,
                        nsTArray<RefPtr<PerformanceEntry>>& aRetval);

  virtual void AddEntry(nsIHttpChannel* channel,
                        nsITimedChannel* timedChannel) = 0;

  void ClearResourceTimings();

  virtual DOMHighResTimeStamp Now() const = 0;

  void Mark(const nsAString& aName, ErrorResult& aRv);

  void ClearMarks(const Optional<nsAString>& aName);

  void Measure(const nsAString& aName,
               const Optional<nsAString>& aStartMark,
               const Optional<nsAString>& aEndMark,
               ErrorResult& aRv);

  void ClearMeasures(const Optional<nsAString>& aName);

  void SetResourceTimingBufferSize(uint64_t aMaxSize);

  void AddObserver(PerformanceObserver* aObserver);
  void RemoveObserver(PerformanceObserver* aObserver);
  void NotifyObservers();
  void CancelNotificationObservers();

  virtual PerformanceTiming* Timing() = 0;

  virtual PerformanceNavigation* Navigation() = 0;

  IMPL_EVENT_HANDLER(resourcetimingbufferfull)

  virtual void GetMozMemory(JSContext *aCx,
                            JS::MutableHandle<JSObject*> aObj) = 0;

  virtual nsDOMNavigationTiming* GetDOMTiming() const = 0;

  virtual nsITimedChannel* GetChannel() const = 0;

  virtual Performance* GetParentPerformance() const = 0;

protected:
  Performance();
  explicit Performance(nsPIDOMWindowInner* aWindow);

  virtual ~Performance();

  virtual void InsertUserEntry(PerformanceEntry* aEntry);
  void InsertResourceEntry(PerformanceEntry* aEntry);

  void ClearUserEntries(const Optional<nsAString>& aEntryName,
                        const nsAString& aEntryType);

  DOMHighResTimeStamp ResolveTimestampFromName(const nsAString& aName,
                                               ErrorResult& aRv);

  virtual nsISupports* GetAsISupports() = 0;

  virtual void DispatchBufferFullEvent() = 0;

  virtual TimeStamp CreationTimeStamp() const = 0;

  virtual DOMHighResTimeStamp CreationTime() const = 0;

  virtual bool IsPerformanceTimingAttribute(const nsAString& aName) = 0;

  virtual DOMHighResTimeStamp
  GetPerformanceTimingFromString(const nsAString& aTimingName) = 0;

  bool IsResourceEntryLimitReached() const
  {
    return mResourceEntries.Length() >= mResourceTimingBufferSize;
  }

  void LogEntry(PerformanceEntry* aEntry, const nsACString& aOwner) const;
  void TimingNotification(PerformanceEntry* aEntry, const nsACString& aOwner,
                          uint64_t epoch);

  void RunNotificationObserversTask();
  void QueueEntry(PerformanceEntry* aEntry);

  DOMHighResTimeStamp RoundTime(double aTime) const;

  nsTObserverArray<PerformanceObserver*> mObservers;

private:
  nsTArray<RefPtr<PerformanceEntry>> mUserEntries;
  nsTArray<RefPtr<PerformanceEntry>> mResourceEntries;

  uint64_t mResourceTimingBufferSize;
  static const uint64_t kDefaultResourceTimingBufferSize = 150;
  bool mPendingNotificationObserversTask;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Performance_h

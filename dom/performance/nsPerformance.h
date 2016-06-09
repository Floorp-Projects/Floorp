/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsPerformance_h___
#define nsPerformance_h___

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "mozilla/Attributes.h"
#include "nsWrapperCache.h"
#include "nsDOMNavigationTiming.h"
#include "nsContentUtils.h"
#include "nsPIDOMWindow.h"
#include "js/TypeDecls.h"
#include "js/RootingAPI.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/DOMEventTargetHelper.h"

class nsITimedChannel;
class nsPerformance;
class nsIHttpChannel;

namespace mozilla {

class ErrorResult;

namespace dom {

class PerformanceEntry;
class PerformanceObserver;
class PerformanceTiming;

// Script "performance.navigation" object
class PerformanceNavigation final : public nsWrapperCache
{
public:
  enum PerformanceNavigationType {
    TYPE_NAVIGATE = 0,
    TYPE_RELOAD = 1,
    TYPE_BACK_FORWARD = 2,
    TYPE_RESERVED = 255,
  };

  explicit PerformanceNavigation(nsPerformance* aPerformance);

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(PerformanceNavigation)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(PerformanceNavigation)

  nsDOMNavigationTiming* GetDOMTiming() const;
  PerformanceTiming* GetPerformanceTiming() const;

  nsPerformance* GetParentObject() const
  {
    return mPerformance;
  }

  virtual JSObject* WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;

  // PerformanceNavigation WebIDL methods
  uint16_t Type() const {
    return GetDOMTiming()->GetType();
  }

  uint16_t RedirectCount() const;

private:
  ~PerformanceNavigation();
  RefPtr<nsPerformance> mPerformance;
};

} // namespace dom
} // namespace mozilla

// Base class for main-thread and worker Performance API
class PerformanceBase : public mozilla::DOMEventTargetHelper 
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PerformanceBase,
                                           DOMEventTargetHelper)

  PerformanceBase();
  explicit PerformanceBase(nsPIDOMWindowInner* aWindow);

  typedef mozilla::dom::PerformanceEntry PerformanceEntry;
  typedef mozilla::dom::PerformanceObserver PerformanceObserver;

  void GetEntries(nsTArray<RefPtr<PerformanceEntry>>& aRetval);
  void GetEntriesByType(const nsAString& aEntryType,
                        nsTArray<RefPtr<PerformanceEntry>>& aRetval);
  void GetEntriesByName(const nsAString& aName,
                        const mozilla::dom::Optional<nsAString>& aEntryType,
                        nsTArray<RefPtr<PerformanceEntry>>& aRetval);
  void ClearResourceTimings();

  virtual DOMHighResTimeStamp Now() const = 0;

  void Mark(const nsAString& aName, mozilla::ErrorResult& aRv);
  void ClearMarks(const mozilla::dom::Optional<nsAString>& aName);
  void Measure(const nsAString& aName,
               const mozilla::dom::Optional<nsAString>& aStartMark,
               const mozilla::dom::Optional<nsAString>& aEndMark,
               mozilla::ErrorResult& aRv);
  void ClearMeasures(const mozilla::dom::Optional<nsAString>& aName);

  void SetResourceTimingBufferSize(uint64_t aMaxSize);

  void AddObserver(PerformanceObserver* aObserver);
  void RemoveObserver(PerformanceObserver* aObserver);
  void NotifyObservers();
  void CancelNotificationObservers();

protected:
  virtual ~PerformanceBase();

  virtual void InsertUserEntry(PerformanceEntry* aEntry);
  void InsertResourceEntry(PerformanceEntry* aEntry);

  void ClearUserEntries(const mozilla::dom::Optional<nsAString>& aEntryName,
                        const nsAString& aEntryType);

  DOMHighResTimeStamp ResolveTimestampFromName(const nsAString& aName,
                                               mozilla::ErrorResult& aRv);

  virtual nsISupports* GetAsISupports() = 0;

  virtual void DispatchBufferFullEvent() = 0;

  virtual mozilla::TimeStamp CreationTimeStamp() const = 0;

  virtual DOMHighResTimeStamp CreationTime() const = 0;

  virtual bool IsPerformanceTimingAttribute(const nsAString& aName) = 0;

  virtual DOMHighResTimeStamp
  GetPerformanceTimingFromString(const nsAString& aTimingName) = 0;

  bool IsResourceEntryLimitReached() const
  {
    return mResourceEntries.Length() >= mResourceTimingBufferSize;
  }

  void LogEntry(PerformanceEntry* aEntry, const nsACString& aOwner) const;
  void TimingNotification(PerformanceEntry* aEntry, const nsACString& aOwner, uint64_t epoch);

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

// Script "performance" object
class nsPerformance final : public PerformanceBase
{
public:
  nsPerformance(nsPIDOMWindowInner* aWindow,
                nsDOMNavigationTiming* aDOMTiming,
                nsITimedChannel* aChannel,
                nsPerformance* aParentPerformance);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(nsPerformance,
                                                         PerformanceBase)

  static bool IsEnabled(JSContext* aCx, JSObject* aGlobal);

  static bool IsObserverEnabled(JSContext* aCx, JSObject* aGlobal);

  nsDOMNavigationTiming* GetDOMTiming() const
  {
    return mDOMTiming;
  }

  nsITimedChannel* GetChannel() const
  {
    return mChannel;
  }

  nsPerformance* GetParentPerformance() const
  {
    return mParentPerformance;
  }

  JSObject* WrapObject(JSContext *cx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // Performance WebIDL methods
  DOMHighResTimeStamp Now() const override;

  mozilla::dom::PerformanceTiming* Timing();
  mozilla::dom::PerformanceNavigation* Navigation();

  void AddEntry(nsIHttpChannel* channel,
                nsITimedChannel* timedChannel);

  using PerformanceBase::GetEntries;
  using PerformanceBase::GetEntriesByType;
  using PerformanceBase::GetEntriesByName;
  using PerformanceBase::ClearResourceTimings;

  using PerformanceBase::Mark;
  using PerformanceBase::ClearMarks;
  using PerformanceBase::Measure;
  using PerformanceBase::ClearMeasures;
  using PerformanceBase::SetResourceTimingBufferSize;

  void GetMozMemory(JSContext *aCx, JS::MutableHandle<JSObject*> aObj);

  IMPL_EVENT_HANDLER(resourcetimingbufferfull)

  mozilla::TimeStamp CreationTimeStamp() const override;

  DOMHighResTimeStamp CreationTime() const override;

protected:
  ~nsPerformance();

  nsISupports* GetAsISupports() override
  {
    return this;
  }

  void InsertUserEntry(PerformanceEntry* aEntry) override;

  bool IsPerformanceTimingAttribute(const nsAString& aName) override;

  DOMHighResTimeStamp
  GetPerformanceTimingFromString(const nsAString& aTimingName) override;

  void DispatchBufferFullEvent() override;

  RefPtr<nsDOMNavigationTiming> mDOMTiming;
  nsCOMPtr<nsITimedChannel> mChannel;
  RefPtr<mozilla::dom::PerformanceTiming> mTiming;
  RefPtr<mozilla::dom::PerformanceNavigation> mNavigation;
  RefPtr<nsPerformance> mParentPerformance;
  JS::Heap<JSObject*> mMozMemory;
};

inline nsDOMNavigationTiming*
mozilla::dom::PerformanceNavigation::GetDOMTiming() const
{
  return mPerformance->GetDOMTiming();
}

inline mozilla::dom::PerformanceTiming*
mozilla::dom::PerformanceNavigation::GetPerformanceTiming() const
{
  return mPerformance->Timing();
}

#endif /* nsPerformance_h___ */

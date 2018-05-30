/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformanceMainThread.h"
#include "PerformanceNavigation.h"
#include "mozilla/dom/DOMPrefs.h"
#include "nsICacheInfoChannel.h"

namespace mozilla {
namespace dom {

namespace {

void
GetURLSpecFromChannel(nsITimedChannel* aChannel, nsAString& aSpec)
{
  aSpec.AssignLiteral("document");

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aChannel);
  if (!channel) {
    return;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = channel->GetURI(getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv)) || !uri) {
    return;
  }

  nsAutoCString spec;
  rv = uri->GetSpec(spec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  aSpec = NS_ConvertUTF8toUTF16(spec);
}

} // anonymous

NS_IMPL_CYCLE_COLLECTION_CLASS(PerformanceMainThread)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(PerformanceMainThread,
                                                Performance)
NS_IMPL_CYCLE_COLLECTION_UNLINK(mTiming,
                                mNavigation,
                                mDocEntry)
  tmp->mMozMemory = nullptr;
  mozilla::DropJSObjects(this);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(PerformanceMainThread,
                                                  Performance)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTiming,
                                    mNavigation,
                                    mDocEntry)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(PerformanceMainThread,
                                                Performance)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mMozMemory)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_ADDREF_INHERITED(PerformanceMainThread, Performance)
NS_IMPL_RELEASE_INHERITED(PerformanceMainThread, Performance)

// QueryInterface implementation for PerformanceMainThread
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PerformanceMainThread)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END_INHERITING(Performance)

PerformanceMainThread::PerformanceMainThread(nsPIDOMWindowInner* aWindow,
                                             nsDOMNavigationTiming* aDOMTiming,
                                             nsITimedChannel* aChannel)
  : Performance(aWindow)
  , mDOMTiming(aDOMTiming)
  , mChannel(aChannel)
{
  MOZ_ASSERT(aWindow, "Parent window object should be provided");
}

PerformanceMainThread::~PerformanceMainThread()
{
  mozilla::DropJSObjects(this);
}

void
PerformanceMainThread::GetMozMemory(JSContext *aCx,
                                    JS::MutableHandle<JSObject*> aObj)
{
  if (!mMozMemory) {
    mMozMemory = js::gc::NewMemoryInfoObject(aCx);
    if (mMozMemory) {
      mozilla::HoldJSObjects(this);
    }
  }

  aObj.set(mMozMemory);
}

PerformanceTiming*
PerformanceMainThread::Timing()
{
  if (!mTiming) {
    // For navigation timing, the third argument (an nsIHttpChannel) is null
    // since the cross-domain redirect were already checked.  The last argument
    // (zero time) for performance.timing is the navigation start value.
    mTiming = new PerformanceTiming(this, mChannel, nullptr,
                                    mDOMTiming->GetNavigationStart());
  }

  return mTiming;
}

void
PerformanceMainThread::DispatchBufferFullEvent()
{
  RefPtr<Event> event = NS_NewDOMEvent(this, nullptr, nullptr);
  // it bubbles, and it isn't cancelable
  event->InitEvent(NS_LITERAL_STRING("resourcetimingbufferfull"), true, false);
  event->SetTrusted(true);
  DispatchEvent(*event);
}

PerformanceNavigation*
PerformanceMainThread::Navigation()
{
  if (!mNavigation) {
    mNavigation = new PerformanceNavigation(this);
  }

  return mNavigation;
}

/**
 * An entry should be added only after the resource is loaded.
 * This method is not thread safe and can only be called on the main thread.
 */
void
PerformanceMainThread::AddEntry(nsIHttpChannel* channel,
                                nsITimedChannel* timedChannel)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoString initiatorType;
  nsAutoString entryName;

  UniquePtr<PerformanceTimingData> performanceTimingData(
    PerformanceTimingData::Create(timedChannel, channel, 0, initiatorType,
                                  entryName));
  if (!performanceTimingData) {
    return;
  }

  // The PerformanceResourceTiming object will use the PerformanceTimingData
  // object to get all the required timings.
  RefPtr<PerformanceResourceTiming> performanceEntry =
    new PerformanceResourceTiming(std::move(performanceTimingData), this,
                                  entryName);

  performanceEntry->SetInitiatorType(initiatorType);
  InsertResourceEntry(performanceEntry);
}

// To be removed once bug 1124165 lands
bool
PerformanceMainThread::IsPerformanceTimingAttribute(const nsAString& aName)
{
  // Note that toJSON is added to this list due to bug 1047848
  static const char* attributes[] =
    {"navigationStart", "unloadEventStart", "unloadEventEnd", "redirectStart",
     "redirectEnd", "fetchStart", "domainLookupStart", "domainLookupEnd",
     "connectStart", "secureConnectionStart", "connectEnd", "requestStart", "responseStart",
     "responseEnd", "domLoading", "domInteractive",
     "domContentLoadedEventStart", "domContentLoadedEventEnd", "domComplete",
     "loadEventStart", "loadEventEnd", nullptr};

  for (uint32_t i = 0; attributes[i]; ++i) {
    if (aName.EqualsASCII(attributes[i])) {
      return true;
    }
  }

  return false;
}

DOMHighResTimeStamp
PerformanceMainThread::GetPerformanceTimingFromString(const nsAString& aProperty)
{
  if (!IsPerformanceTimingAttribute(aProperty)) {
    return 0;
  }
  if (aProperty.EqualsLiteral("navigationStart")) {
    // DOMHighResTimeStamp is in relation to navigationStart, so this will be
    // zero.
    return GetDOMTiming()->GetNavigationStart();
  }
  if (aProperty.EqualsLiteral("unloadEventStart")) {
    return GetDOMTiming()->GetUnloadEventStart();
  }
  if (aProperty.EqualsLiteral("unloadEventEnd")) {
    return GetDOMTiming()->GetUnloadEventEnd();
  }
  if (aProperty.EqualsLiteral("redirectStart")) {
    return Timing()->RedirectStart();
  }
  if (aProperty.EqualsLiteral("redirectEnd")) {
    return Timing()->RedirectEnd();
  }
  if (aProperty.EqualsLiteral("fetchStart")) {
    return Timing()->FetchStart();
  }
  if (aProperty.EqualsLiteral("domainLookupStart")) {
    return Timing()->DomainLookupStart();
  }
  if (aProperty.EqualsLiteral("domainLookupEnd")) {
    return Timing()->DomainLookupEnd();
  }
  if (aProperty.EqualsLiteral("connectStart")) {
    return Timing()->ConnectStart();
  }
  if (aProperty.EqualsLiteral("secureConnectionStart")) {
    return Timing()->SecureConnectionStart();
  }
  if (aProperty.EqualsLiteral("connectEnd")) {
    return Timing()->ConnectEnd();
  }
  if (aProperty.EqualsLiteral("requestStart")) {
    return Timing()->RequestStart();
  }
  if (aProperty.EqualsLiteral("responseStart")) {
    return Timing()->ResponseStart();
  }
  if (aProperty.EqualsLiteral("responseEnd")) {
    return Timing()->ResponseEnd();
  }
  if (aProperty.EqualsLiteral("domLoading")) {
    return GetDOMTiming()->GetDomLoading();
  }
  if (aProperty.EqualsLiteral("domInteractive")) {
    return GetDOMTiming()->GetDomInteractive();
  }
  if (aProperty.EqualsLiteral("domContentLoadedEventStart")) {
    return GetDOMTiming()->GetDomContentLoadedEventStart();
  }
  if (aProperty.EqualsLiteral("domContentLoadedEventEnd")) {
    return GetDOMTiming()->GetDomContentLoadedEventEnd();
  }
  if (aProperty.EqualsLiteral("domComplete")) {
    return GetDOMTiming()->GetDomComplete();
  }
  if (aProperty.EqualsLiteral("loadEventStart")) {
    return GetDOMTiming()->GetLoadEventStart();
  }
  if (aProperty.EqualsLiteral("loadEventEnd"))  {
    return GetDOMTiming()->GetLoadEventEnd();
  }
  MOZ_CRASH("IsPerformanceTimingAttribute and GetPerformanceTimingFromString are out of sync");
  return 0;
}

void
PerformanceMainThread::InsertUserEntry(PerformanceEntry* aEntry)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoCString uri;
  uint64_t markCreationEpoch = 0;

  if (DOMPrefs::PerformanceLoggingEnabled() ||
      nsContentUtils::SendPerformanceTimingNotifications()) {
    nsresult rv = NS_ERROR_FAILURE;
    nsCOMPtr<nsPIDOMWindowInner> owner = GetOwner();
    if (owner && owner->GetDocumentURI()) {
      rv = owner->GetDocumentURI()->GetHost(uri);
    }

    if(NS_FAILED(rv)) {
      // If we have no URI, just put in "none".
      uri.AssignLiteral("none");
    }
    markCreationEpoch = static_cast<uint64_t>(PR_Now() / PR_USEC_PER_MSEC);

    if (DOMPrefs::PerformanceLoggingEnabled()) {
      Performance::LogEntry(aEntry, uri);
    }
  }

  if (nsContentUtils::SendPerformanceTimingNotifications()) {
    TimingNotification(aEntry, uri, markCreationEpoch);
  }

  Performance::InsertUserEntry(aEntry);
}

TimeStamp
PerformanceMainThread::CreationTimeStamp() const
{
  return GetDOMTiming()->GetNavigationStartTimeStamp();
}

DOMHighResTimeStamp
PerformanceMainThread::CreationTime() const
{
  return GetDOMTiming()->GetNavigationStart();
}

void
PerformanceMainThread::EnsureDocEntry()
{
  if (!mDocEntry && nsContentUtils::IsPerformanceNavigationTimingEnabled()) {

    UniquePtr<PerformanceTimingData> timing(
      new PerformanceTimingData(mChannel, nullptr, 0));

    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel);
    if (httpChannel) {
      timing->SetPropertiesFromHttpChannel(httpChannel);
    }

    nsAutoString name;
    GetURLSpecFromChannel(mChannel, name);

    mDocEntry = new PerformanceNavigationTiming(std::move(timing), this, name);
  }
}

void
PerformanceMainThread::CreateDocumentEntry(nsITimedChannel* aChannel)
{
  MOZ_ASSERT(aChannel);
  MOZ_ASSERT(!mDocEntry, "mDocEntry should be null.");

  if (!nsContentUtils::IsPerformanceNavigationTimingEnabled()) {
    return;
  }

  nsAutoString name;
  GetURLSpecFromChannel(aChannel, name);

  UniquePtr<PerformanceTimingData> timing(
      new PerformanceTimingData(aChannel, nullptr, 0));
  mDocEntry = new PerformanceNavigationTiming(std::move(timing), this, name);
}

void
PerformanceMainThread::GetEntries(nsTArray<RefPtr<PerformanceEntry>>& aRetval)
{
  // We return an empty list when 'privacy.resistFingerprinting' is on.
  if (nsContentUtils::ShouldResistFingerprinting()) {
    aRetval.Clear();
    return;
  }

  aRetval = mResourceEntries;
  aRetval.AppendElements(mUserEntries);

  EnsureDocEntry();
  if (mDocEntry) {
    aRetval.AppendElement(mDocEntry);
  }

  aRetval.Sort(PerformanceEntryComparator());
}

void
PerformanceMainThread::GetEntriesByType(const nsAString& aEntryType,
                                        nsTArray<RefPtr<PerformanceEntry>>& aRetval)
{
  // We return an empty list when 'privacy.resistFingerprinting' is on.
  if (nsContentUtils::ShouldResistFingerprinting()) {
    aRetval.Clear();
    return;
  }

  if (aEntryType.EqualsLiteral("navigation")) {
    aRetval.Clear();
    EnsureDocEntry();
    if (mDocEntry) {
      aRetval.AppendElement(mDocEntry);
    }
    return;
  }

  Performance::GetEntriesByType(aEntryType, aRetval);
}

void
PerformanceMainThread::GetEntriesByName(const nsAString& aName,
                                        const Optional<nsAString>& aEntryType,
                                        nsTArray<RefPtr<PerformanceEntry>>& aRetval)
{
  // We return an empty list when 'privacy.resistFingerprinting' is on.
  if (nsContentUtils::ShouldResistFingerprinting()) {
    aRetval.Clear();
    return;
  }

  if (aName.EqualsLiteral("document")) {
    aRetval.Clear();
    EnsureDocEntry();
    if (mDocEntry) {
      aRetval.AppendElement(mDocEntry);
    }
    return;
  }

  Performance::GetEntriesByName(aName, aEntryType, aRetval);
}

} // dom namespace
} // mozilla namespace

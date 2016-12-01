/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformanceMainThread.h"
#include "PerformanceNavigation.h"
#include "nsICacheInfoChannel.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(PerformanceMainThread)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(PerformanceMainThread,
                                                Performance)
NS_IMPL_CYCLE_COLLECTION_UNLINK(mTiming,
                                mNavigation,
                                mParentPerformance)
  tmp->mMozMemory = nullptr;
  mozilla::DropJSObjects(this);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(PerformanceMainThread,
                                                  Performance)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTiming,
                                    mNavigation,
                                    mParentPerformance)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
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
                                             nsITimedChannel* aChannel,
                                             Performance* aParentPerformance)
  : Performance(aWindow)
  , mDOMTiming(aDOMTiming)
  , mChannel(aChannel)
  , mParentPerformance(aParentPerformance)
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
    // For navigation timing, the third argument (an nsIHtttpChannel) is null
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
  DispatchDOMEvent(nullptr, event, nullptr, nullptr);
}

PerformanceNavigation*
PerformanceMainThread::Navigation()
{
  if (!mNavigation) {
    mNavigation = new PerformanceNavigation(this);
  }

  return mNavigation;
}

DOMHighResTimeStamp
PerformanceMainThread::Now() const
{
  return RoundTime(GetDOMTiming()->TimeStampToDOMHighRes(TimeStamp::Now()));
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

  // Check if resource timing is prefed off.
  if (!nsContentUtils::IsResourceTimingEnabled()) {
    return;
  }

  // Don't add the entry if the buffer is full
  if (IsResourceEntryLimitReached()) {
    return;
  }

  if (channel && timedChannel) {
    nsAutoCString name;
    nsAutoString initiatorType;
    nsCOMPtr<nsIURI> originalURI;

    timedChannel->GetInitiatorType(initiatorType);

    // According to the spec, "The name attribute must return the resolved URL
    // of the requested resource. This attribute must not change even if the
    // fetch redirected to a different URL."
    channel->GetOriginalURI(getter_AddRefs(originalURI));
    originalURI->GetSpec(name);
    NS_ConvertUTF8toUTF16 entryName(name);

    // The nsITimedChannel argument will be used to gather all the timings.
    // The nsIHttpChannel argument will be used to check if any cross-origin
    // redirects occurred.
    // The last argument is the "zero time" (offset). Since we don't want
    // any offset for the resource timing, this will be set to "0" - the
    // resource timing returns a relative timing (no offset).
    RefPtr<PerformanceTiming> performanceTiming =
        new PerformanceTiming(this, timedChannel, channel,
            0);

    // The PerformanceResourceTiming object will use the PerformanceTiming
    // object to get all the required timings.
    RefPtr<PerformanceResourceTiming> performanceEntry =
      new PerformanceResourceTiming(performanceTiming, this, entryName);

    nsAutoCString protocol;
    channel->GetProtocolVersion(protocol);

    // If this is a local fetch, nextHopProtocol should be set to empty string.
    nsCOMPtr<nsICacheInfoChannel> cachedChannel = do_QueryInterface(channel);
    if (cachedChannel) {
      bool isFromCache;
      if (NS_SUCCEEDED(cachedChannel->IsFromCache(&isFromCache))
          && isFromCache) {
        protocol.Truncate();
      }
    }

    performanceEntry->SetNextHopProtocol(NS_ConvertUTF8toUTF16(protocol));

    uint64_t encodedBodySize = 0;
    channel->GetEncodedBodySize(&encodedBodySize);
    performanceEntry->SetEncodedBodySize(encodedBodySize);

    uint64_t transferSize = 0;
    channel->GetTransferSize(&transferSize);
    performanceEntry->SetTransferSize(transferSize);

    uint64_t decodedBodySize = 0;
    channel->GetDecodedBodySize(&decodedBodySize);
    if (decodedBodySize == 0) {
      decodedBodySize = encodedBodySize;
    }
    performanceEntry->SetDecodedBodySize(decodedBodySize);

    // If the initiator type had no valid value, then set it to the default
    // ("other") value.
    if (initiatorType.IsEmpty()) {
      initiatorType = NS_LITERAL_STRING("other");
    }
    performanceEntry->SetInitiatorType(initiatorType);
    InsertResourceEntry(performanceEntry);
  }
}

// To be removed once bug 1124165 lands
bool
PerformanceMainThread::IsPerformanceTimingAttribute(const nsAString& aName)
{
  // Note that toJSON is added to this list due to bug 1047848
  static const char* attributes[] =
    {"navigationStart", "unloadEventStart", "unloadEventEnd", "redirectStart",
     "redirectEnd", "fetchStart", "domainLookupStart", "domainLookupEnd",
     "connectStart", "connectEnd", "requestStart", "responseStart",
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

  if (nsContentUtils::IsUserTimingLoggingEnabled() ||
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

    if (nsContentUtils::IsUserTimingLoggingEnabled()) {
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

} // dom namespace
} // mozilla namespace

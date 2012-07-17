/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPerformance.h"
#include "TimeStamp.h"
#include "nsCOMPtr.h"
#include "nscore.h"
#include "nsIDocShell.h"
#include "nsITimedChannel.h"
#include "nsDOMClassInfoID.h"
#include "nsDOMNavigationTiming.h"
#include "nsContentUtils.h"
#include "nsIDOMWindow.h"
#include "mozilla/dom/PerformanceBinding.h"
#include "mozilla/dom/PerformanceTimingBinding.h"
#include "mozilla/dom/PerformanceNavigationBinding.h"

using namespace mozilla;

DOMCI_DATA(PerformanceTiming, nsPerformanceTiming)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(nsPerformanceTiming, mPerformance)
NS_IMPL_CYCLE_COLLECTING_ADDREF(nsPerformanceTiming)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsPerformanceTiming)

// QueryInterface implementation for nsPerformanceTiming
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsPerformanceTiming)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMPerformanceTiming)
  NS_INTERFACE_MAP_ENTRY(nsIDOMPerformanceTiming)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(PerformanceTiming)
NS_INTERFACE_MAP_END

nsPerformanceTiming::nsPerformanceTiming(nsPerformance* aPerformance,
                                         nsITimedChannel* aChannel)
  : mPerformance(aPerformance),
    mChannel(aChannel)
{
  MOZ_ASSERT(aPerformance, "Parent performance object should be provided");
  SetIsDOMBinding();
}

nsPerformanceTiming::~nsPerformanceTiming()
{
}

NS_IMETHODIMP
nsPerformanceTiming::GetNavigationStart(DOMTimeMilliSec* aTime)
{
  *aTime = GetNavigationStart();
  return NS_OK;
}

NS_IMETHODIMP
nsPerformanceTiming::GetUnloadEventStart(DOMTimeMilliSec* aTime)
{
  *aTime = GetUnloadEventStart();
  return NS_OK;
}

NS_IMETHODIMP
nsPerformanceTiming::GetUnloadEventEnd(DOMTimeMilliSec* aTime)
{
  *aTime = GetUnloadEventEnd();
  return NS_OK;
}

NS_IMETHODIMP
nsPerformanceTiming::GetRedirectStart(DOMTimeMilliSec* aTime)
{
  *aTime = GetRedirectStart();
  return NS_OK;
}

NS_IMETHODIMP
nsPerformanceTiming::GetRedirectEnd(DOMTimeMilliSec* aTime)
{
  *aTime = GetRedirectEnd();
  return NS_OK;
}

NS_IMETHODIMP
nsPerformanceTiming::GetFetchStart(DOMTimeMilliSec* aTime)
{
  *aTime = GetFetchStart();
  return NS_OK;
}

NS_IMETHODIMP
nsPerformanceTiming::GetDomainLookupStart(DOMTimeMilliSec* aTime)
{
  *aTime = GetDomainLookupStart();
  return NS_OK;
}

DOMTimeMilliSec
nsPerformanceTiming::GetDomainLookupStart() const
{
  if (!mChannel) {
    return GetFetchStart();
  }
  mozilla::TimeStamp stamp;
  mChannel->GetDomainLookupStart(&stamp);
  return GetDOMTiming()->TimeStampToDOMOrFetchStart(stamp);
}

NS_IMETHODIMP
nsPerformanceTiming::GetDomainLookupEnd(DOMTimeMilliSec* aTime)
{
  *aTime = GetDomainLookupEnd();
  return NS_OK;
}

DOMTimeMilliSec
nsPerformanceTiming::GetDomainLookupEnd() const
{
  if (!mChannel) {
    return GetFetchStart();
  }
  mozilla::TimeStamp stamp;
  mChannel->GetDomainLookupEnd(&stamp);
  return GetDOMTiming()->TimeStampToDOMOrFetchStart(stamp);
}

NS_IMETHODIMP
nsPerformanceTiming::GetConnectStart(DOMTimeMilliSec* aTime)
{
  *aTime = GetConnectStart();
  return NS_OK;
}

DOMTimeMilliSec
nsPerformanceTiming::GetConnectStart() const
{
  if (!mChannel) {
    return GetFetchStart();
  }
  mozilla::TimeStamp stamp;
  mChannel->GetConnectStart(&stamp);
  return GetDOMTiming()->TimeStampToDOMOrFetchStart(stamp);
}

NS_IMETHODIMP
nsPerformanceTiming::GetConnectEnd(DOMTimeMilliSec* aTime)
{
  *aTime = GetConnectEnd();
  return NS_OK;
}

DOMTimeMilliSec
nsPerformanceTiming::GetConnectEnd() const
{
  if (!mChannel) {
    return GetFetchStart();
  }
  mozilla::TimeStamp stamp;
  mChannel->GetConnectEnd(&stamp);
  return GetDOMTiming()->TimeStampToDOMOrFetchStart(stamp);
}

NS_IMETHODIMP
nsPerformanceTiming::GetRequestStart(DOMTimeMilliSec* aTime)
{
  *aTime = GetRequestStart();
  return NS_OK;
}

DOMTimeMilliSec
nsPerformanceTiming::GetRequestStart() const
{
  if (!mChannel) {
    return GetFetchStart();
  }
  mozilla::TimeStamp stamp;
  mChannel->GetRequestStart(&stamp);
  return GetDOMTiming()->TimeStampToDOMOrFetchStart(stamp);
}

NS_IMETHODIMP
nsPerformanceTiming::GetResponseStart(DOMTimeMilliSec* aTime)
{
  *aTime = GetResponseStart();
  return NS_OK;
}

DOMTimeMilliSec
nsPerformanceTiming::GetResponseStart() const
{
  if (!mChannel) {
    return GetFetchStart();
  }
  mozilla::TimeStamp stamp;
  mChannel->GetResponseStart(&stamp);
  mozilla::TimeStamp cacheStamp;
  mChannel->GetCacheReadStart(&cacheStamp);
  if (stamp.IsNull() || (!cacheStamp.IsNull() && cacheStamp < stamp)) {
    stamp = cacheStamp;
  }
  return GetDOMTiming()->TimeStampToDOMOrFetchStart(stamp);
}

NS_IMETHODIMP
nsPerformanceTiming::GetResponseEnd(DOMTimeMilliSec* aTime)
{
  *aTime = GetResponseEnd();
  return NS_OK;
}

DOMTimeMilliSec
nsPerformanceTiming::GetResponseEnd() const
{
  if (!mChannel) {
    return GetFetchStart();
  }
  mozilla::TimeStamp stamp;
  mChannel->GetResponseEnd(&stamp);
  mozilla::TimeStamp cacheStamp;
  mChannel->GetCacheReadEnd(&cacheStamp);
  if (stamp.IsNull() || (!cacheStamp.IsNull() && cacheStamp < stamp)) {
    stamp = cacheStamp;
  }
  return GetDOMTiming()->TimeStampToDOMOrFetchStart(stamp);
}

NS_IMETHODIMP
nsPerformanceTiming::GetDomLoading(DOMTimeMilliSec* aTime)
{
  *aTime = GetDomLoading();
  return NS_OK;
}

NS_IMETHODIMP
nsPerformanceTiming::GetDomInteractive(DOMTimeMilliSec* aTime)
{
  *aTime = GetDomInteractive();
  return NS_OK;
}

NS_IMETHODIMP
nsPerformanceTiming::GetDomContentLoadedEventStart(DOMTimeMilliSec* aTime)
{
  *aTime = GetDomContentLoadedEventStart();
  return NS_OK;
}

NS_IMETHODIMP
nsPerformanceTiming::GetDomContentLoadedEventEnd(DOMTimeMilliSec* aTime)
{
  *aTime = GetDomContentLoadedEventEnd();
  return NS_OK;
}

NS_IMETHODIMP
nsPerformanceTiming::GetDomComplete(DOMTimeMilliSec* aTime)
{
  *aTime = GetDomComplete();
  return NS_OK;
}

NS_IMETHODIMP
nsPerformanceTiming::GetLoadEventStart(DOMTimeMilliSec* aTime)
{
  *aTime = GetLoadEventStart();
  return NS_OK;
}

NS_IMETHODIMP
nsPerformanceTiming::GetLoadEventEnd(DOMTimeMilliSec* aTime)
{
  *aTime = GetLoadEventEnd();
  return NS_OK;
}

JSObject*
nsPerformanceTiming::WrapObject(JSContext *cx, JSObject *scope,
                                bool *triedToWrap)
{
  return dom::PerformanceTimingBinding::Wrap(cx, scope, this,
                                             triedToWrap);
}



DOMCI_DATA(PerformanceNavigation, nsPerformanceNavigation)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(nsPerformanceNavigation, mPerformance)
NS_IMPL_CYCLE_COLLECTING_ADDREF(nsPerformanceNavigation)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsPerformanceNavigation)

// QueryInterface implementation for nsPerformanceNavigation
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsPerformanceNavigation)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMPerformanceNavigation)
  NS_INTERFACE_MAP_ENTRY(nsIDOMPerformanceNavigation)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(PerformanceNavigation)
NS_INTERFACE_MAP_END

nsPerformanceNavigation::nsPerformanceNavigation(nsPerformance* aPerformance)
  : mPerformance(aPerformance)
{
  MOZ_ASSERT(aPerformance, "Parent performance object should be provided");
  SetIsDOMBinding();
}

nsPerformanceNavigation::~nsPerformanceNavigation()
{
}

NS_IMETHODIMP
nsPerformanceNavigation::GetType(
    nsDOMPerformanceNavigationType* aNavigationType)
{
  *aNavigationType = GetType();
  return NS_OK;
}

NS_IMETHODIMP
nsPerformanceNavigation::GetRedirectCount(PRUint16* aRedirectCount)
{
  *aRedirectCount = GetRedirectCount();
  return NS_OK;
}

JSObject*
nsPerformanceNavigation::WrapObject(JSContext *cx, JSObject *scope,
                                    bool *triedToWrap)
{
  return dom::PerformanceNavigationBinding::Wrap(cx, scope, this,
                                                 triedToWrap);
}


DOMCI_DATA(Performance, nsPerformance)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_3(nsPerformance,
                                        mWindow, mTiming,
                                        mNavigation)
NS_IMPL_CYCLE_COLLECTING_ADDREF(nsPerformance)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsPerformance)

nsPerformance::nsPerformance(nsIDOMWindow* aWindow,
                             nsDOMNavigationTiming* aDOMTiming,
                             nsITimedChannel* aChannel)
  : mWindow(aWindow),
    mDOMTiming(aDOMTiming),
    mChannel(aChannel)
{
  MOZ_ASSERT(aWindow, "Parent window object should be provided");
  SetIsDOMBinding();
}

nsPerformance::~nsPerformance()
{
}

// QueryInterface implementation for nsPerformance
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsPerformance)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMPerformance)
  NS_INTERFACE_MAP_ENTRY(nsIDOMPerformance)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Performance)
NS_INTERFACE_MAP_END


//
// nsIDOMPerformance methods
//

nsPerformanceTiming*
nsPerformance::GetTiming()
{
  if (!mTiming) {
    mTiming = new nsPerformanceTiming(this, mChannel);
  }
  return mTiming;
}

NS_IMETHODIMP
nsPerformance::GetTiming(nsIDOMPerformanceTiming** aTiming)
{
  nsRefPtr<nsPerformanceTiming> timing = GetTiming();
  timing.forget(aTiming);
  return NS_OK;
}

nsPerformanceNavigation*
nsPerformance::GetNavigation()
{
  if (!mNavigation) {
    mNavigation = new nsPerformanceNavigation(this);
  }
  return mNavigation;
}

NS_IMETHODIMP
nsPerformance::GetNavigation(nsIDOMPerformanceNavigation** aNavigation)
{
  nsRefPtr<nsPerformanceNavigation> navigation = GetNavigation();
  navigation.forget(aNavigation);
  return NS_OK;
}

NS_IMETHODIMP
nsPerformance::Now(DOMHighResTimeStamp* aNow)
{
  *aNow = Now();
  return NS_OK;
}

DOMHighResTimeStamp
nsPerformance::Now()
{
  return GetDOMTiming()->TimeStampToDOMHighRes(mozilla::TimeStamp::Now());
}

JSObject*
nsPerformance::WrapObject(JSContext *cx, JSObject *scope,
                          bool *triedToWrap)
{
  return dom::PerformanceBinding::Wrap(cx, scope, this,
                                       triedToWrap);
}


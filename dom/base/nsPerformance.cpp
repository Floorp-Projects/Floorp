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
#include "nsDOMNavigationTiming.h"
#include "nsContentUtils.h"
#include "nsIDOMWindow.h"
#include "nsDOMClassInfoID.h"
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
  NS_INTERFACE_MAP_ENTRY(nsISupports)
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
  NS_INTERFACE_MAP_ENTRY(nsISupports)
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
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END


nsPerformanceTiming*
nsPerformance::GetTiming()
{
  if (!mTiming) {
    mTiming = new nsPerformanceTiming(this, mChannel);
  }
  return mTiming;
}

nsPerformanceNavigation*
nsPerformance::GetNavigation()
{
  if (!mNavigation) {
    mNavigation = new nsPerformanceNavigation(this);
  }
  return mNavigation;
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


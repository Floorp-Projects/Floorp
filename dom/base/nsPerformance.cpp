/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPerformance.h"
#include "nsCOMPtr.h"
#include "nsITimedChannel.h"
#include "nsDOMNavigationTiming.h"
#include "nsContentUtils.h"
#include "nsIDOMWindow.h"
#include "mozilla/dom/PerformanceBinding.h"
#include "mozilla/dom/PerformanceTimingBinding.h"
#include "mozilla/dom/PerformanceNavigationBinding.h"
#include "mozilla/TimeStamp.h"

using namespace mozilla;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(nsPerformanceTiming, mPerformance)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsPerformanceTiming, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsPerformanceTiming, Release)

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
nsPerformanceTiming::DomainLookupStart() const
{
  if (!nsContentUtils::IsPerformanceTimingEnabled()) {
    return 0;
  }
  if (!mChannel) {
    return FetchStart();
  }
  mozilla::TimeStamp stamp;
  mChannel->GetDomainLookupStart(&stamp);
  return GetDOMTiming()->TimeStampToDOMOrFetchStart(stamp);
}

DOMTimeMilliSec
nsPerformanceTiming::DomainLookupEnd() const
{
  if (!nsContentUtils::IsPerformanceTimingEnabled()) {
    return 0;
  }
  if (!mChannel) {
    return FetchStart();
  }
  mozilla::TimeStamp stamp;
  mChannel->GetDomainLookupEnd(&stamp);
  return GetDOMTiming()->TimeStampToDOMOrFetchStart(stamp);
}

DOMTimeMilliSec
nsPerformanceTiming::ConnectStart() const
{
  if (!nsContentUtils::IsPerformanceTimingEnabled()) {
    return 0;
  }
  if (!mChannel) {
    return FetchStart();
  }
  mozilla::TimeStamp stamp;
  mChannel->GetConnectStart(&stamp);
  return GetDOMTiming()->TimeStampToDOMOrFetchStart(stamp);
}

DOMTimeMilliSec
nsPerformanceTiming::ConnectEnd() const
{
  if (!nsContentUtils::IsPerformanceTimingEnabled()) {
    return 0;
  }
  if (!mChannel) {
    return FetchStart();
  }
  mozilla::TimeStamp stamp;
  mChannel->GetConnectEnd(&stamp);
  return GetDOMTiming()->TimeStampToDOMOrFetchStart(stamp);
}

DOMTimeMilliSec
nsPerformanceTiming::RequestStart() const
{
  if (!nsContentUtils::IsPerformanceTimingEnabled()) {
    return 0;
  }
  if (!mChannel) {
    return FetchStart();
  }
  mozilla::TimeStamp stamp;
  mChannel->GetRequestStart(&stamp);
  return GetDOMTiming()->TimeStampToDOMOrFetchStart(stamp);
}

DOMTimeMilliSec
nsPerformanceTiming::ResponseStart() const
{
  if (!nsContentUtils::IsPerformanceTimingEnabled()) {
    return 0;
  }
  if (!mChannel) {
    return FetchStart();
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
nsPerformanceTiming::ResponseEnd() const
{
  if (!nsContentUtils::IsPerformanceTimingEnabled()) {
    return 0;
  }
  if (!mChannel) {
    return FetchStart();
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
nsPerformanceTiming::WrapObject(JSContext *cx, JS::Handle<JSObject*> scope)
{
  return dom::PerformanceTimingBinding::Wrap(cx, scope, this);
}


NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(nsPerformanceNavigation, mPerformance)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsPerformanceNavigation, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsPerformanceNavigation, Release)

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
nsPerformanceNavigation::WrapObject(JSContext *cx, JS::Handle<JSObject*> scope)
{
  return dom::PerformanceNavigationBinding::Wrap(cx, scope, this);
}


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
nsPerformance::Timing()
{
  if (!mTiming) {
    mTiming = new nsPerformanceTiming(this, mChannel);
  }
  return mTiming;
}

nsPerformanceNavigation*
nsPerformance::Navigation()
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
nsPerformance::WrapObject(JSContext *cx, JS::Handle<JSObject*> scope)
{
  return dom::PerformanceBinding::Wrap(cx, scope, this);
}


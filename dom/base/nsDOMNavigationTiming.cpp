/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMNavigationTiming.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsIScriptSecurityManager.h"
#include "prtime.h"
#include "nsIURI.h"
#include "mozilla/TimeStamp.h"

nsDOMNavigationTiming::nsDOMNavigationTiming()
{
  Clear();
}

nsDOMNavigationTiming::~nsDOMNavigationTiming()
{
}

void
nsDOMNavigationTiming::Clear()
{
  mNavigationType = mozilla::dom::PerformanceNavigation::TYPE_RESERVED;
  mNavigationStart = 0;
  mFetchStart = 0;
  mRedirectStart = 0;
  mRedirectEnd = 0;
  mRedirectCount = 0;
  mBeforeUnloadStart = 0;
  mUnloadStart = 0;
  mUnloadEnd = 0;
  mLoadEventStart = 0;
  mLoadEventEnd = 0;
  mDOMLoading = 0;
  mDOMInteractive = 0;
  mDOMContentLoadedEventStart = 0;
  mDOMContentLoadedEventEnd = 0;
  mDOMComplete = 0;
  mRedirectCheck = NOT_CHECKED;

  mLoadEventStartSet = false;
  mLoadEventEndSet = false;
  mDOMLoadingSet = false;
  mDOMInteractiveSet = false;
  mDOMContentLoadedEventStartSet = false;
  mDOMContentLoadedEventEndSet = false;
  mDOMCompleteSet = false;
}

DOMTimeMilliSec
nsDOMNavigationTiming::TimeStampToDOM(mozilla::TimeStamp aStamp) const
{
  if (aStamp.IsNull()) {
    return 0;
  }
  mozilla::TimeDuration duration = aStamp - mNavigationStartTimeStamp;
  return mNavigationStart + static_cast<int32_t>(duration.ToMilliseconds());
}

DOMTimeMilliSec
nsDOMNavigationTiming::TimeStampToDOMOrFetchStart(mozilla::TimeStamp aStamp) const
{
  if (!aStamp.IsNull()) {
    return TimeStampToDOM(aStamp);
  } else {
    return GetFetchStart();
  }
}

DOMTimeMilliSec nsDOMNavigationTiming::DurationFromStart(){
  return TimeStampToDOM(mozilla::TimeStamp::Now());
}

void
nsDOMNavigationTiming::NotifyNavigationStart()
{
  mNavigationStart = PR_Now() / PR_USEC_PER_MSEC;
  mNavigationStartTimeStamp = mozilla::TimeStamp::Now();
}

void
nsDOMNavigationTiming::NotifyFetchStart(nsIURI* aURI, nsDOMPerformanceNavigationType aNavigationType)
{
  mFetchStart = DurationFromStart();
  mNavigationType = aNavigationType;
  // At the unload event time we don't really know the loading uri.
  // Need it for later check for unload timing access.
  mLoadedURI = aURI;
}

void
nsDOMNavigationTiming::NotifyRedirect(nsIURI* aOldURI, nsIURI* aNewURI)
{
  if (mRedirects.Count() == 0) {
    mRedirectStart = mFetchStart;
  }
  mFetchStart = DurationFromStart();
  mRedirectEnd = mFetchStart;

  // At the unload event time we don't really know the loading uri.
  // Need it for later check for unload timing access.
  mLoadedURI = aNewURI;

  mRedirects.AppendObject(aOldURI);
}

void
nsDOMNavigationTiming::NotifyBeforeUnload()
{
  mBeforeUnloadStart = DurationFromStart();
}

void
nsDOMNavigationTiming::NotifyUnloadAccepted(nsIURI* aOldURI)
{
  mUnloadStart = mBeforeUnloadStart;
  mUnloadedURI = aOldURI;
}

void
nsDOMNavigationTiming::NotifyUnloadEventStart()
{
  mUnloadStart = DurationFromStart();
}

void
nsDOMNavigationTiming::NotifyUnloadEventEnd()
{
  mUnloadEnd = DurationFromStart();
}

void
nsDOMNavigationTiming::NotifyLoadEventStart()
{
  if (!mLoadEventStartSet) {
    mLoadEventStart = DurationFromStart();
    mLoadEventStartSet = true;
  }
}

void
nsDOMNavigationTiming::NotifyLoadEventEnd()
{
  if (!mLoadEventEndSet) {
    mLoadEventEnd = DurationFromStart();
    mLoadEventEndSet = true;
  }
}

bool
nsDOMNavigationTiming::ReportRedirects()
{
  if (mRedirectCheck == NOT_CHECKED) {
    mRedirectCount = mRedirects.Count();
    if (mRedirects.Count() == 0) {
      mRedirectCheck = NO_REDIRECTS;
    } else {
      mRedirectCheck = CHECK_PASSED;
      nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
      for (int i = mRedirects.Count() - 1; i >= 0; --i) {
        nsIURI * curr = mRedirects[i];
        nsresult rv = ssm->CheckSameOriginURI(curr, mLoadedURI, false);
        if (!NS_SUCCEEDED(rv)) {
          mRedirectCheck = CHECK_FAILED;
          mRedirectCount = 0;
          break;
        }
      }
      // All we need to know is in mRedirectCheck now. Clear history.
      mRedirects.Clear();
    }
  }
  return mRedirectCheck == CHECK_PASSED;
}

void
nsDOMNavigationTiming::SetDOMLoadingTimeStamp(nsIURI* aURI, mozilla::TimeStamp aValue)
{
  if (!mDOMLoadingSet) {
    mLoadedURI = aURI;
    mDOMLoading = TimeStampToDOM(aValue);
    mDOMLoadingSet = true;
  }
}

void
nsDOMNavigationTiming::NotifyDOMLoading(nsIURI* aURI)
{
  if (!mDOMLoadingSet) {
    mLoadedURI = aURI;
    mDOMLoading = DurationFromStart();
    mDOMLoadingSet = true;
  }
}

void
nsDOMNavigationTiming::NotifyDOMInteractive(nsIURI* aURI)
{
  if (!mDOMInteractiveSet) {
    mLoadedURI = aURI;
    mDOMInteractive = DurationFromStart();
    mDOMInteractiveSet = true;
  }
}

void
nsDOMNavigationTiming::NotifyDOMComplete(nsIURI* aURI)
{
  if (!mDOMCompleteSet) {
    mLoadedURI = aURI;
    mDOMComplete = DurationFromStart();
    mDOMCompleteSet = true;
  }
}

void
nsDOMNavigationTiming::NotifyDOMContentLoadedStart(nsIURI* aURI)
{
  if (!mDOMContentLoadedEventStartSet) {
    mLoadedURI = aURI;
    mDOMContentLoadedEventStart = DurationFromStart();
    mDOMContentLoadedEventStartSet = true;
  }
}

void
nsDOMNavigationTiming::NotifyDOMContentLoadedEnd(nsIURI* aURI)
{
  if (!mDOMContentLoadedEventEndSet) {
    mLoadedURI = aURI;
    mDOMContentLoadedEventEnd = DurationFromStart();
    mDOMContentLoadedEventEndSet = true;
  }
}

uint16_t
nsDOMNavigationTiming::GetRedirectCount()
{
  if (ReportRedirects()) {
    return mRedirectCount;
  }
  return 0;
}

DOMTimeMilliSec
nsDOMNavigationTiming::GetRedirectStart()
{
  if (ReportRedirects()) {
    return mRedirectStart;
  }
  return 0;
}

DOMTimeMilliSec
nsDOMNavigationTiming::GetRedirectEnd()
{
  if (ReportRedirects()) {
    return mRedirectEnd;
  }
  return 0;
}

DOMTimeMilliSec
nsDOMNavigationTiming::GetUnloadEventStart()
{
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  nsresult rv = ssm->CheckSameOriginURI(mLoadedURI, mUnloadedURI, false);
  if (NS_SUCCEEDED(rv)) {
    return mUnloadStart;
  }
  return 0;
}

DOMTimeMilliSec
nsDOMNavigationTiming::GetUnloadEventEnd()
{
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  nsresult rv = ssm->CheckSameOriginURI(mLoadedURI, mUnloadedURI, false);
  if (NS_SUCCEEDED(rv)) {
    return mUnloadEnd;
  }
  return 0;
}


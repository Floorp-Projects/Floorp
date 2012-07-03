/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMNavigationTiming_h___
#define nsDOMNavigationTiming_h___

#include "nsIDOMPerformanceTiming.h"
#include "nsIDOMPerformanceNavigation.h"
#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "mozilla/TimeStamp.h"
#include "nsIURI.h"

class nsDOMNavigationTimingClock;

class nsDOMNavigationTiming
{
public:
  nsDOMNavigationTiming();

  NS_INLINE_DECL_REFCOUNTING(nsDOMNavigationTiming)
  nsresult GetType(nsDOMPerformanceNavigationType* aNavigationType);
  nsresult GetRedirectCount(PRUint16* aCount);

  nsresult GetRedirectStart(DOMTimeMilliSec* aRedirectStart);
  nsresult GetRedirectEnd(DOMTimeMilliSec* aEnd);
  nsresult GetNavigationStart(DOMTimeMilliSec* aNavigationStart);
  nsresult GetUnloadEventStart(DOMTimeMilliSec* aStart);
  nsresult GetUnloadEventEnd(DOMTimeMilliSec* aEnd);
  nsresult GetFetchStart(DOMTimeMilliSec* aStart);
  nsresult GetDomLoading(DOMTimeMilliSec* aTime);
  nsresult GetDomInteractive(DOMTimeMilliSec* aTime);
  nsresult GetDomContentLoadedEventStart(DOMTimeMilliSec* aStart);
  nsresult GetDomContentLoadedEventEnd(DOMTimeMilliSec* aEnd);
  nsresult GetDomComplete(DOMTimeMilliSec* aTime);
  nsresult GetLoadEventStart(DOMTimeMilliSec* aStart);
  nsresult GetLoadEventEnd(DOMTimeMilliSec* aEnd);

  void NotifyNavigationStart();
  void NotifyFetchStart(nsIURI* aURI, nsDOMPerformanceNavigationType aNavigationType);
  void NotifyRedirect(nsIURI* aOldURI, nsIURI* aNewURI);
  void NotifyBeforeUnload();
  void NotifyUnloadAccepted(nsIURI* aOldURI);
  void NotifyUnloadEventStart();
  void NotifyUnloadEventEnd();
  void NotifyLoadEventStart();
  void NotifyLoadEventEnd();

  // Document changes state to 'loading' before connecting to timing
  void SetDOMLoadingTimeStamp(nsIURI* aURI, mozilla::TimeStamp aValue);
  void NotifyDOMLoading(nsIURI* aURI);
  void NotifyDOMInteractive(nsIURI* aURI);
  void NotifyDOMComplete(nsIURI* aURI);
  void NotifyDOMContentLoadedStart(nsIURI* aURI);
  void NotifyDOMContentLoadedEnd(nsIURI* aURI);
  nsresult TimeStampToDOM(mozilla::TimeStamp aStamp, DOMTimeMilliSec* aResult);
  nsresult TimeStampToDOMOrFetchStart(mozilla::TimeStamp aStamp, 
                                      DOMTimeMilliSec* aResult);

  inline DOMHighResTimeStamp TimeStampToDOMHighRes(mozilla::TimeStamp aStamp)
  {
    mozilla::TimeDuration duration = aStamp - mNavigationStartTimeStamp;
    return duration.ToMilliseconds();
  }

private:
  nsDOMNavigationTiming(const nsDOMNavigationTiming &){};
  ~nsDOMNavigationTiming();

  void Clear();
  bool ReportRedirects();

  nsCOMPtr<nsIURI> mUnloadedURI;
  nsCOMPtr<nsIURI> mLoadedURI;
  nsCOMArray<nsIURI> mRedirects;

  typedef enum { NOT_CHECKED,
                 CHECK_PASSED,
                 NO_REDIRECTS,
                 CHECK_FAILED} RedirectCheckState;
  RedirectCheckState mRedirectCheck;
  PRInt16 mRedirectCount;

  nsDOMPerformanceNavigationType mNavigationType;
  DOMTimeMilliSec mNavigationStart;
  mozilla::TimeStamp mNavigationStartTimeStamp;
  DOMTimeMilliSec DurationFromStart();

  DOMTimeMilliSec mFetchStart;
  DOMTimeMilliSec mRedirectStart;
  DOMTimeMilliSec mRedirectEnd;
  DOMTimeMilliSec mBeforeUnloadStart;
  DOMTimeMilliSec mUnloadStart;
  DOMTimeMilliSec mUnloadEnd;
  DOMTimeMilliSec mNavigationEnd;
  DOMTimeMilliSec mLoadEventStart;
  DOMTimeMilliSec mLoadEventEnd;

  DOMTimeMilliSec mDOMLoading;
  DOMTimeMilliSec mDOMInteractive;
  DOMTimeMilliSec mDOMContentLoadedEventStart;
  DOMTimeMilliSec mDOMContentLoadedEventEnd;
  DOMTimeMilliSec mDOMComplete;
};

#endif /* nsDOMNavigationTiming_h___ */

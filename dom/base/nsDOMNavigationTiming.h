/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMNavigationTiming_h___
#define nsDOMNavigationTiming_h___

#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "mozilla/TimeStamp.h"

class nsIURI;

typedef unsigned long long DOMTimeMilliSec;
typedef double DOMHighResTimeStamp;
typedef unsigned short nsDOMPerformanceNavigationType;

namespace mozilla {
namespace dom {
namespace PerformanceNavigation {
static const nsDOMPerformanceNavigationType TYPE_NAVIGATE = 0;
static const nsDOMPerformanceNavigationType TYPE_RELOAD = 1;
static const nsDOMPerformanceNavigationType TYPE_BACK_FORWARD = 2;
static const nsDOMPerformanceNavigationType TYPE_RESERVED = 255;
}
}
}

class nsDOMNavigationTiming MOZ_FINAL
{
public:
  nsDOMNavigationTiming();

  NS_INLINE_DECL_REFCOUNTING(nsDOMNavigationTiming)

  nsDOMPerformanceNavigationType GetType() const {
    return mNavigationType;
  }
  inline DOMHighResTimeStamp GetNavigationStartHighRes() const {
    return mNavigationStartHighRes;
  }
  DOMTimeMilliSec GetNavigationStart() const {
    return static_cast<int64_t>(GetNavigationStartHighRes());
  }
  mozilla::TimeStamp GetNavigationStartTimeStamp() const {
    return mNavigationStartTimeStamp;
  }
  DOMTimeMilliSec GetUnloadEventStart();
  DOMTimeMilliSec GetUnloadEventEnd();
  DOMTimeMilliSec GetDomLoading() const {
    return mDOMLoading;
  }
  DOMTimeMilliSec GetDomInteractive() const {
    return mDOMInteractive;
  }
  DOMTimeMilliSec GetDomContentLoadedEventStart() const {
    return mDOMContentLoadedEventStart;
  }
  DOMTimeMilliSec GetDomContentLoadedEventEnd() const {
    return mDOMContentLoadedEventEnd;
  }
  DOMTimeMilliSec GetDomComplete() const {
    return mDOMComplete;
  }
  DOMTimeMilliSec GetLoadEventStart() const {
    return mLoadEventStart;
  }
  DOMTimeMilliSec GetLoadEventEnd() const {
    return mLoadEventEnd;
  }

  void NotifyNavigationStart();
  void NotifyFetchStart(nsIURI* aURI, nsDOMPerformanceNavigationType aNavigationType);
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
  DOMTimeMilliSec TimeStampToDOM(mozilla::TimeStamp aStamp) const;

  inline DOMHighResTimeStamp TimeStampToDOMHighRes(mozilla::TimeStamp aStamp)
  {
    mozilla::TimeDuration duration = aStamp - mNavigationStartTimeStamp;
    return duration.ToMilliseconds();
  }

private:
  nsDOMNavigationTiming(const nsDOMNavigationTiming &) MOZ_DELETE;
  ~nsDOMNavigationTiming();

  void Clear();

  nsCOMPtr<nsIURI> mUnloadedURI;
  nsCOMPtr<nsIURI> mLoadedURI;

  nsDOMPerformanceNavigationType mNavigationType;
  DOMHighResTimeStamp mNavigationStartHighRes;
  mozilla::TimeStamp mNavigationStartTimeStamp;
  DOMTimeMilliSec DurationFromStart();

  DOMTimeMilliSec mBeforeUnloadStart;
  DOMTimeMilliSec mUnloadStart;
  DOMTimeMilliSec mUnloadEnd;
  DOMTimeMilliSec mLoadEventStart;
  DOMTimeMilliSec mLoadEventEnd;

  DOMTimeMilliSec mDOMLoading;
  DOMTimeMilliSec mDOMInteractive;
  DOMTimeMilliSec mDOMContentLoadedEventStart;
  DOMTimeMilliSec mDOMContentLoadedEventEnd;
  DOMTimeMilliSec mDOMComplete;

  // Booleans to keep track of what things we've already been notified
  // about.  We don't update those once we've been notified about them
  // once.
  bool mLoadEventStartSet : 1;
  bool mLoadEventEndSet : 1;
  bool mDOMLoadingSet : 1;
  bool mDOMInteractiveSet : 1;
  bool mDOMContentLoadedEventStartSet : 1;
  bool mDOMContentLoadedEventEndSet : 1;
  bool mDOMCompleteSet : 1;
};

#endif /* nsDOMNavigationTiming_h___ */

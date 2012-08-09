/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsPerformance_h___
#define nsPerformance_h___

#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "mozilla/Attributes.h"
#include "nsWrapperCache.h"
#include "nsDOMNavigationTiming.h"

class nsIURI;
class nsITimedChannel;
class nsIDOMWindow;
class nsPerformance;
struct JSObject;
struct JSContext;

// Script "performance.timing" object
class nsPerformanceTiming MOZ_FINAL : public nsISupports,
                                      public nsWrapperCache
{
public:
  nsPerformanceTiming(nsPerformance* aPerformance,
                      nsITimedChannel* aChannel);
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsPerformanceTiming)

  nsDOMNavigationTiming* GetDOMTiming() const;

  nsPerformance* GetParentObject() const
  {
    return mPerformance;
  }

  JSObject* WrapObject(JSContext *cx, JSObject *scope, bool *triedToWrap);

  // PerformanceNavigation WebIDL methods
  DOMTimeMilliSec GetNavigationStart() const {
    return GetDOMTiming()->GetNavigationStart();
  }
  DOMTimeMilliSec GetUnloadEventStart() {
    return GetDOMTiming()->GetUnloadEventStart();
  }
  DOMTimeMilliSec GetUnloadEventEnd() {
    return GetDOMTiming()->GetUnloadEventEnd();
  }
  DOMTimeMilliSec GetRedirectStart() {
    return GetDOMTiming()->GetRedirectStart();
  }
  DOMTimeMilliSec GetRedirectEnd() {
    return GetDOMTiming()->GetRedirectEnd();
  }
  DOMTimeMilliSec GetFetchStart() const {
    return GetDOMTiming()->GetFetchStart();
  }
  DOMTimeMilliSec GetDomainLookupStart() const;
  DOMTimeMilliSec GetDomainLookupEnd() const;
  DOMTimeMilliSec GetConnectStart() const;
  DOMTimeMilliSec GetConnectEnd() const;
  DOMTimeMilliSec GetRequestStart() const;
  DOMTimeMilliSec GetResponseStart() const;
  DOMTimeMilliSec GetResponseEnd() const;
  DOMTimeMilliSec GetDomLoading() const {
    return GetDOMTiming()->GetDomLoading();
  }
  DOMTimeMilliSec GetDomInteractive() const {
    return GetDOMTiming()->GetDomInteractive();
  }
  DOMTimeMilliSec GetDomContentLoadedEventStart() const {
    return GetDOMTiming()->GetDomContentLoadedEventStart();
  }
  DOMTimeMilliSec GetDomContentLoadedEventEnd() const {
    return GetDOMTiming()->GetDomContentLoadedEventEnd();
  }
  DOMTimeMilliSec GetDomComplete() const {
    return GetDOMTiming()->GetDomComplete();
  }
  DOMTimeMilliSec GetLoadEventStart() const {
    return GetDOMTiming()->GetLoadEventStart();
  }
  DOMTimeMilliSec GetLoadEventEnd() const {
    return GetDOMTiming()->GetLoadEventEnd();
  }

private:
  ~nsPerformanceTiming();
  nsRefPtr<nsPerformance> mPerformance;
  nsCOMPtr<nsITimedChannel> mChannel;
};

// Script "performance.navigation" object
class nsPerformanceNavigation MOZ_FINAL : public nsISupports,
                                          public nsWrapperCache
{
public:
  explicit nsPerformanceNavigation(nsPerformance* aPerformance);
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsPerformanceNavigation)

  nsDOMNavigationTiming* GetDOMTiming() const;

  nsPerformance* GetParentObject() const
  {
    return mPerformance;
  }

  JSObject* WrapObject(JSContext *cx, JSObject *scope, bool *triedToWrap);

  // PerformanceNavigation WebIDL methods
  PRUint16 GetType() const {
    return GetDOMTiming()->GetType();
  }
  PRUint16 GetRedirectCount() const {
    return GetDOMTiming()->GetRedirectCount();
  }

private:
  ~nsPerformanceNavigation();
  nsRefPtr<nsPerformance> mPerformance;
};

// Script "performance" object
class nsPerformance MOZ_FINAL : public nsISupports,
                                public nsWrapperCache
{
public:
  nsPerformance(nsIDOMWindow* aWindow,
                nsDOMNavigationTiming* aDOMTiming,
                nsITimedChannel* aChannel);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsPerformance)

  nsDOMNavigationTiming* GetDOMTiming() const
  {
    return mDOMTiming;
  }

  nsIDOMWindow* GetParentObject() const
  {
    return mWindow.get();
  }

  JSObject* WrapObject(JSContext *cx, JSObject *scope, bool *triedToWrap);

  // Performance WebIDL methods
  DOMHighResTimeStamp Now();
  nsPerformanceTiming* GetTiming();
  nsPerformanceNavigation* GetNavigation();

private:
  ~nsPerformance();

  nsCOMPtr<nsIDOMWindow> mWindow;
  nsRefPtr<nsDOMNavigationTiming> mDOMTiming;
  nsCOMPtr<nsITimedChannel> mChannel;
  nsRefPtr<nsPerformanceTiming> mTiming;
  nsRefPtr<nsPerformanceNavigation> mNavigation;
};

inline nsDOMNavigationTiming*
nsPerformanceNavigation::GetDOMTiming() const
{
  return mPerformance->GetDOMTiming();
}

inline nsDOMNavigationTiming*
nsPerformanceTiming::GetDOMTiming() const
{
  return mPerformance->GetDOMTiming();
}

#endif /* nsPerformance_h___ */


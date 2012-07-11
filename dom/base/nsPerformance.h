/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsPerformance_h___
#define nsPerformance_h___

#include "nsIDOMPerformance.h"
#include "nsIDOMPerformanceTiming.h"
#include "nsIDOMPerformanceNavigation.h"
#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "mozilla/Attributes.h"

class nsIURI;
class nsDOMNavigationTiming;
class nsITimedChannel;

// Script "performance.timing" object
class nsPerformanceTiming MOZ_FINAL : public nsIDOMPerformanceTiming
{
public:
  nsPerformanceTiming(nsDOMNavigationTiming* aDOMTiming, nsITimedChannel* aChannel);
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMPERFORMANCETIMING
private:
  ~nsPerformanceTiming();
  nsRefPtr<nsDOMNavigationTiming> mDOMTiming;
  nsCOMPtr<nsITimedChannel> mChannel;
};

// Script "performance.navigation" object
class nsPerformanceNavigation MOZ_FINAL : public nsIDOMPerformanceNavigation
{
public:
  nsPerformanceNavigation(nsDOMNavigationTiming* data);
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMPERFORMANCENAVIGATION
private:
  ~nsPerformanceNavigation();
  nsRefPtr<nsDOMNavigationTiming> mData;
};

// Script "performance" object
class nsPerformance MOZ_FINAL : public nsIDOMPerformance
{
public:
  nsPerformance(nsDOMNavigationTiming* aDOMTiming, nsITimedChannel* aChannel);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMPERFORMANCE

private:
  ~nsPerformance();

  nsRefPtr<nsDOMNavigationTiming> mDOMTiming;
  nsCOMPtr<nsITimedChannel> mChannel;
  nsCOMPtr<nsIDOMPerformanceTiming> mTiming;
  nsCOMPtr<nsIDOMPerformanceNavigation> mNavigation;
};

#endif /* nsPerformance_h___ */


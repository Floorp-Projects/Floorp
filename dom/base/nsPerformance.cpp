/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is implementation of Web Timing draft specification
 * http://dev.w3.org/2006/webapi/WebTiming/
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Sergey Novikov <sergeyn@google.com> (original author)
 *   Igor Bazarny <igor.bazarny@gmail.com> (update to match bearly-final spec)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsPerformance.h"
#include "nsCOMPtr.h"
#include "nscore.h"
#include "nsIDocShell.h"
#include "nsDOMClassInfo.h"
#include "nsDOMNavigationTiming.h"

DOMCI_DATA(PerformanceTiming, nsPerformanceTiming)

NS_IMPL_ADDREF(nsPerformanceTiming)
NS_IMPL_RELEASE(nsPerformanceTiming)

// QueryInterface implementation for nsPerformanceTiming
NS_INTERFACE_MAP_BEGIN(nsPerformanceTiming)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMPerformanceTiming)
  NS_INTERFACE_MAP_ENTRY(nsIDOMPerformanceTiming)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(PerformanceTiming)
NS_INTERFACE_MAP_END

nsPerformanceTiming::nsPerformanceTiming(nsDOMNavigationTiming* aData)
{
  NS_ASSERTION(aData, "Timing data should be provided");
  mData = aData;
}

nsPerformanceTiming::~nsPerformanceTiming()
{
}

NS_IMETHODIMP
nsPerformanceTiming::GetNavigationStart(DOMTimeMilliSec* aTime)
{
  return mData->GetNavigationStart(aTime);
}

NS_IMETHODIMP
nsPerformanceTiming::GetUnloadEventStart(DOMTimeMilliSec* aTime)
{
  return mData->GetUnloadEventStart(aTime);
}

NS_IMETHODIMP
nsPerformanceTiming::GetUnloadEventEnd(DOMTimeMilliSec* aTime)
{
  return mData->GetUnloadEventEnd(aTime);
}

NS_IMETHODIMP
nsPerformanceTiming::GetRedirectStart(DOMTimeMilliSec* aTime)
{
  return mData->GetRedirectStart(aTime);
}

NS_IMETHODIMP
nsPerformanceTiming::GetRedirectEnd(DOMTimeMilliSec* aTime)
{
  return mData->GetRedirectEnd(aTime);
}

NS_IMETHODIMP
nsPerformanceTiming::GetFetchStart(DOMTimeMilliSec* aTime)
{
  return mData->GetFetchStart(aTime);
}

NS_IMETHODIMP
nsPerformanceTiming::GetDomainLookupStart(DOMTimeMilliSec* aTime)
{
  return mData->GetDomainLookupStart(aTime);
}

NS_IMETHODIMP
nsPerformanceTiming::GetDomainLookupEnd(DOMTimeMilliSec* aTime)
{
  return mData->GetDomainLookupEnd(aTime);
}

NS_IMETHODIMP
nsPerformanceTiming::GetConnectStart(DOMTimeMilliSec* aTime)
{
  return mData->GetConnectStart(aTime);
}

NS_IMETHODIMP
nsPerformanceTiming::GetConnectEnd(DOMTimeMilliSec* aTime)
{
  return mData->GetConnectEnd(aTime);
}

NS_IMETHODIMP
nsPerformanceTiming::GetHandshakeStart(DOMTimeMilliSec* aTime)
{
  return mData->GetHandshakeStart(aTime);
}

NS_IMETHODIMP
nsPerformanceTiming::GetRequestStart(DOMTimeMilliSec* aTime)
{
  return mData->GetRequestStart(aTime);
}

NS_IMETHODIMP
nsPerformanceTiming::GetResponseStart(DOMTimeMilliSec* aTime)
{
  return mData->GetResponseStart(aTime);
}

NS_IMETHODIMP
nsPerformanceTiming::GetResponseEnd(DOMTimeMilliSec* aTime)
{
  return mData->GetResponseEnd(aTime);
}

NS_IMETHODIMP
nsPerformanceTiming::GetDomLoading(DOMTimeMilliSec* aTime)
{
  return mData->GetDomLoading(aTime);
}

NS_IMETHODIMP
nsPerformanceTiming::GetDomInteractive(DOMTimeMilliSec* aTime)
{
  return mData->GetDomInteractive(aTime);
}

NS_IMETHODIMP
nsPerformanceTiming::GetDomContentLoadedEventStart(DOMTimeMilliSec* aTime)
{
  return mData->GetDomContentLoadedEventStart(aTime);
}

NS_IMETHODIMP
nsPerformanceTiming::GetDomContentLoadedEventEnd(DOMTimeMilliSec* aTime)
{
  return mData->GetDomContentLoadedEventEnd(aTime);
}

NS_IMETHODIMP
nsPerformanceTiming::GetDomComplete(DOMTimeMilliSec* aTime)
{
  return mData->GetDomComplete(aTime);
}

NS_IMETHODIMP
nsPerformanceTiming::GetLoadEventStart(DOMTimeMilliSec* aTime)
{
  return mData->GetLoadEventStart(aTime);
}

NS_IMETHODIMP
nsPerformanceTiming::GetLoadEventEnd(DOMTimeMilliSec* aTime)
{
  return mData->GetLoadEventEnd(aTime);
}



DOMCI_DATA(PerformanceNavigation, nsPerformanceNavigation)

NS_IMPL_ADDREF(nsPerformanceNavigation)
NS_IMPL_RELEASE(nsPerformanceNavigation)

// QueryInterface implementation for nsPerformanceNavigation
NS_INTERFACE_MAP_BEGIN(nsPerformanceNavigation)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMPerformanceNavigation)
  NS_INTERFACE_MAP_ENTRY(nsIDOMPerformanceNavigation)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(PerformanceNavigation)
NS_INTERFACE_MAP_END

nsPerformanceNavigation::nsPerformanceNavigation(nsDOMNavigationTiming* aData)
{
  NS_ASSERTION(aData, "Timing data should be provided");
  mData = aData;
}

nsPerformanceNavigation::~nsPerformanceNavigation()
{
}

NS_IMETHODIMP
nsPerformanceNavigation::GetType(
    nsDOMPerformanceNavigationType* aNavigationType)
{
  return mData->GetType(aNavigationType);
}

NS_IMETHODIMP
nsPerformanceNavigation::GetRedirectCount(PRUint16* aRedirectCount)
{
  return mData->GetRedirectCount(aRedirectCount);
}


DOMCI_DATA(Performance, nsPerformance)

NS_IMPL_ADDREF(nsPerformance)
NS_IMPL_RELEASE(nsPerformance)

nsPerformance::nsPerformance(nsDOMNavigationTiming* aTiming)
{
  NS_ASSERTION(aTiming, "Timing data should be provided");
  mData = aTiming;
}

nsPerformance::~nsPerformance()
{
}

// QueryInterface implementation for nsPerformance
NS_INTERFACE_MAP_BEGIN(nsPerformance)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMPerformance)
  NS_INTERFACE_MAP_ENTRY(nsIDOMPerformance)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Performance)
NS_INTERFACE_MAP_END

//
// nsIDOMPerformance methods
//
NS_IMETHODIMP
nsPerformance::GetTiming(nsIDOMPerformanceTiming** aTiming)
{
  if (!mTiming) {
    mTiming = new nsPerformanceTiming(mData);
  }
  NS_IF_ADDREF(*aTiming = mTiming);
  return NS_OK;
}

NS_IMETHODIMP
nsPerformance::GetNavigation(nsIDOMPerformanceNavigation** aNavigation)
{
  if (!mNavigation) {
    mNavigation = new nsPerformanceNavigation(mData);
  }
  NS_IF_ADDREF(*aNavigation = mNavigation);
  return NS_OK;
}

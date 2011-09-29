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
 *   Igor Bazarny <igor.bazarny@gmail.com> (lots of improvements)
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

#include "nsDOMNavigationTiming.h"
#include "nsCOMPtr.h"
#include "nscore.h"
#include "TimeStamp.h"
#include "nsContentUtils.h"

#include "nsIDOMEventTarget.h"
#include "nsIDocument.h"
#include "nsIScriptSecurityManager.h"

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
  mNavigationType = nsIDOMPerformanceNavigation::TYPE_RESERVED;
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
}

nsresult 
nsDOMNavigationTiming::TimeStampToDOM(mozilla::TimeStamp aStamp, 
                                      DOMTimeMilliSec* aResult)
{
  if (aStamp.IsNull()) {
    *aResult = 0;
    return NS_OK;
  }
  mozilla::TimeDuration duration = aStamp - mNavigationStartTimeStamp;
  *aResult = mNavigationStart + static_cast<PRInt32>(duration.ToMilliseconds());
  return NS_OK;
}

nsresult 
nsDOMNavigationTiming::TimeStampToDOMOrFetchStart(mozilla::TimeStamp aStamp, 
                                                  DOMTimeMilliSec* aResult)
{
  if (!aStamp.IsNull()) {
    return TimeStampToDOM(aStamp, aResult);
  } else {
    return GetFetchStart(aResult);
  }
}

DOMTimeMilliSec nsDOMNavigationTiming::DurationFromStart(){
  DOMTimeMilliSec result; 
  TimeStampToDOM(mozilla::TimeStamp::Now(), &result);
  return result;
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
  mLoadEventStart = DurationFromStart();
}

void
nsDOMNavigationTiming::NotifyLoadEventEnd()
{
  mLoadEventEnd = DurationFromStart();
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
        nsresult rv = ssm->CheckSameOriginURI(curr, mLoadedURI, PR_FALSE);
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
  mLoadedURI = aURI;
  TimeStampToDOM(aValue, &mDOMLoading);
}

void
nsDOMNavigationTiming::NotifyDOMLoading(nsIURI* aURI)
{
  mLoadedURI = aURI;
  mDOMLoading = DurationFromStart();
}

void
nsDOMNavigationTiming::NotifyDOMInteractive(nsIURI* aURI)
{
  mLoadedURI = aURI;
  mDOMInteractive = DurationFromStart();
}

void
nsDOMNavigationTiming::NotifyDOMComplete(nsIURI* aURI)
{
  mLoadedURI = aURI;
  mDOMComplete = DurationFromStart();
}

void
nsDOMNavigationTiming::NotifyDOMContentLoadedStart(nsIURI* aURI)
{
  mLoadedURI = aURI;
  mDOMContentLoadedEventStart = DurationFromStart();
}

void
nsDOMNavigationTiming::NotifyDOMContentLoadedEnd(nsIURI* aURI)
{
  mLoadedURI = aURI;
  mDOMContentLoadedEventEnd = DurationFromStart();
}

nsresult
nsDOMNavigationTiming::GetType(
    nsDOMPerformanceNavigationType* aNavigationType)
{
  *aNavigationType = mNavigationType;
  return NS_OK;
}

nsresult
nsDOMNavigationTiming::GetRedirectCount(PRUint16* aRedirectCount)
{
  *aRedirectCount = 0;
  if (ReportRedirects()) {
    *aRedirectCount = mRedirectCount;
  }
  return NS_OK;
}

nsresult
nsDOMNavigationTiming::GetRedirectStart(DOMTimeMilliSec* aRedirectStart)
{
  *aRedirectStart = 0;
  if (ReportRedirects()) {
    *aRedirectStart = mRedirectStart;
  }
  return NS_OK;
}

nsresult
nsDOMNavigationTiming::GetRedirectEnd(DOMTimeMilliSec* aEnd)
{
  *aEnd = 0;
  if (ReportRedirects()) {
    *aEnd = mRedirectEnd;
  }
  return NS_OK;
}

nsresult
nsDOMNavigationTiming::GetNavigationStart(DOMTimeMilliSec* aNavigationStart)
{
  *aNavigationStart = mNavigationStart;
  return NS_OK;
}

nsresult
nsDOMNavigationTiming::GetUnloadEventStart(DOMTimeMilliSec* aStart)
{
  *aStart = 0;
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  nsresult rv = ssm->CheckSameOriginURI(mLoadedURI, mUnloadedURI, PR_FALSE);
  if (NS_SUCCEEDED(rv)) {
    *aStart = mUnloadStart;
  }
  return NS_OK;
}

nsresult
nsDOMNavigationTiming::GetUnloadEventEnd(DOMTimeMilliSec* aEnd)
{
  *aEnd = 0;
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  nsresult rv = ssm->CheckSameOriginURI(mLoadedURI, mUnloadedURI, PR_FALSE);
  if (NS_SUCCEEDED(rv)) {
    *aEnd = mUnloadEnd;
  }
  return NS_OK;
}

nsresult
nsDOMNavigationTiming::GetFetchStart(DOMTimeMilliSec* aStart)
{
  *aStart = mFetchStart;
  return NS_OK;
}

nsresult
nsDOMNavigationTiming::GetDomLoading(DOMTimeMilliSec* aTime)
{
  *aTime = mDOMLoading;
  return NS_OK;
}

nsresult
nsDOMNavigationTiming::GetDomInteractive(DOMTimeMilliSec* aTime)
{
  *aTime = mDOMInteractive;
  return NS_OK;
}

nsresult
nsDOMNavigationTiming::GetDomContentLoadedEventStart(DOMTimeMilliSec* aStart)
{
  *aStart = mDOMContentLoadedEventStart;
  return NS_OK;
}

nsresult
nsDOMNavigationTiming::GetDomContentLoadedEventEnd(DOMTimeMilliSec* aEnd)
{
  *aEnd = mDOMContentLoadedEventEnd;
  return NS_OK;
}

nsresult
nsDOMNavigationTiming::GetDomComplete(DOMTimeMilliSec* aTime)
{
  *aTime = mDOMComplete;
  return NS_OK;
}

nsresult
nsDOMNavigationTiming::GetLoadEventStart(DOMTimeMilliSec* aStart)
{
  *aStart = mLoadEventStart;
  return NS_OK;
}

nsresult
nsDOMNavigationTiming::GetLoadEventEnd(DOMTimeMilliSec* aEnd)
{
  *aEnd = mLoadEventEnd;
  return NS_OK;
}

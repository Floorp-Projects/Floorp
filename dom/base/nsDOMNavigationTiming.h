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
class nsIDocument;

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

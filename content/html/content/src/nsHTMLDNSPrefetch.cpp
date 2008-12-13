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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick McManus <mcmanus@ducksong.com>
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

#include "nsHTMLDNSPrefetch.h"
#include "nsCOMPtr.h"
#include "nsString.h"

#include "nsNetUtil.h"

#include "nsIDNSListener.h"
#include "nsIWebProgressListener.h"
#include "nsIWebProgress.h"
#include "nsCURILoader.h"
#include "nsIDNSRecord.h"
#include "nsIDNSService.h"
#include "nsICancelable.h"
#include "nsContentUtils.h"
#include "nsGkAtoms.h"
#include "nsIDocument.h"
#include "nsThreadUtils.h"

static NS_DEFINE_CID(kDNSServiceCID, NS_DNSSERVICE_CID);
static PRBool sDisablePrefetchHTTPSPref;
static PRBool sInitialized = PR_FALSE;
static nsIDNSService *sDNSService = nsnull;
static nsHTMLDNSPrefetch::nsDeferrals *sPrefetches = nsnull;

nsresult
nsHTMLDNSPrefetch::Initialize()
{
  if (sInitialized) {
    NS_WARNING("Initialize() called twice");
    return NS_OK;
  }
  
  sPrefetches = new nsHTMLDNSPrefetch::nsDeferrals();
  if (!sPrefetches)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(sPrefetches);
  sPrefetches->Activate();

  nsContentUtils::AddBoolPrefVarCache("network.dns.disablePrefetchFromHTTPS", 
                                      &sDisablePrefetchHTTPSPref);
  
  // Default is false, so we need an explicit call to prime the cache.
  sDisablePrefetchHTTPSPref = 
    nsContentUtils::GetBoolPref("network.dns.disablePrefetchFromHTTPS", PR_TRUE);
  
  NS_IF_RELEASE(sDNSService);
  nsresult rv;
  rv = CallGetService(kDNSServiceCID, &sDNSService);
  if (NS_FAILED(rv)) return rv;
  
  sInitialized = PR_TRUE;
  return NS_OK;
}

nsresult
nsHTMLDNSPrefetch::Shutdown()
{
  if (!sInitialized) {
    NS_WARNING("Not Initialized");
    return NS_OK;
  }
  sInitialized = PR_FALSE;
  NS_IF_RELEASE(sDNSService);
  NS_IF_RELEASE(sPrefetches);
  
  return NS_OK;
}

PRBool
nsHTMLDNSPrefetch::IsSecureBaseContext (nsIDocument *aDocument)
{
  nsIURI *docURI = aDocument->GetDocumentURI();
  nsCAutoString scheme;
  docURI->GetScheme(scheme);
  return scheme.EqualsLiteral("https");
}

PRBool
nsHTMLDNSPrefetch::IsAllowed (nsIDocument *aDocument)
{
  if (IsSecureBaseContext(aDocument) && sDisablePrefetchHTTPSPref)
    return PR_FALSE;
    
  // Check whether the x-dns-prefetch-control HTTP response header is set to override 
  // the default. This may also be set by meta tag. Chromium treats any value other
  // than 'on' (case insensitive) as 'off'.

  nsAutoString control;
  aDocument->GetHeaderData(nsGkAtoms::headerDNSPrefetchControl, control);
  
  if (!control.IsEmpty() && !control.LowerCaseEqualsLiteral("on"))
    return PR_FALSE;

  // There is no need to do prefetch on non UI scenarios such as XMLHttpRequest.
  if (!aDocument->GetWindow())
    return PR_FALSE;

  return PR_TRUE;
}

nsresult
nsHTMLDNSPrefetch::Prefetch(nsIURI *aURI, PRUint16 flags)
{
  if (!(sInitialized && sPrefetches && sDNSService))
    return NS_ERROR_NOT_AVAILABLE;

  return sPrefetches->Add(flags, aURI);
}

nsresult
nsHTMLDNSPrefetch::PrefetchLow(nsIURI *aURI)
{
  return Prefetch(aURI, nsIDNSService::RESOLVE_PRIORITY_LOW);
}

nsresult
nsHTMLDNSPrefetch::PrefetchMedium(nsIURI *aURI)
{
  return Prefetch(aURI, nsIDNSService::RESOLVE_PRIORITY_MEDIUM);
}

nsresult
nsHTMLDNSPrefetch::PrefetchHigh(nsIURI *aURI)
{
  return Prefetch(aURI, 0);
}

nsresult
nsHTMLDNSPrefetch::Prefetch(nsAString &hostname, PRUint16 flags)
{
  if (!(sInitialized && sDNSService && sPrefetches))
    return NS_ERROR_NOT_AVAILABLE;

  nsCOMPtr<nsICancelable> tmpOutstanding;
  return sDNSService->AsyncResolve(NS_ConvertUTF16toUTF8(hostname), flags,
                                   sPrefetches, nsnull, getter_AddRefs(tmpOutstanding));
}

nsresult
nsHTMLDNSPrefetch::PrefetchLow(nsAString &hostname)
{
  return Prefetch(hostname, nsIDNSService::RESOLVE_PRIORITY_LOW);
}

nsresult
nsHTMLDNSPrefetch::PrefetchMedium(nsAString &hostname)
{
  return Prefetch(hostname, nsIDNSService::RESOLVE_PRIORITY_MEDIUM);
}

nsresult
nsHTMLDNSPrefetch::PrefetchHigh(nsAString &hostname)
{
  return Prefetch(hostname, 0);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////

nsHTMLDNSPrefetch::nsDeferrals::nsDeferrals()
  : mHead(0),
    mTail(0),
    mActiveLoaderCount(0)
{
}

nsHTMLDNSPrefetch::nsDeferrals::~nsDeferrals()
{
  while (mHead != mTail) {
    mEntries[mTail].mURI = nsnull;
    mTail = (mTail + 1) & sMaxDeferredMask;
  }
}

NS_IMPL_THREADSAFE_ISUPPORTS3(nsHTMLDNSPrefetch::nsDeferrals,
                              nsIDNSListener,
                              nsIWebProgressListener,
                              nsISupportsWeakReference)

nsresult
nsHTMLDNSPrefetch::nsDeferrals::Add(PRUint16 flags, nsIURI *aURI)
{
  // The FIFO has no lock, so it can only be accessed on main thread
  NS_ASSERTION(NS_IsMainThread(), "nsDeferrals::Add must be on main thread");

  if (((mHead + 1) & sMaxDeferredMask) == mTail)
    return NS_ERROR_DNS_LOOKUP_QUEUE_FULL;
    
  mEntries[mHead].mFlags = flags;
  mEntries[mHead].mURI = aURI;
  mHead = (mHead + 1) & sMaxDeferredMask;

  return NS_OK;
}

void
nsHTMLDNSPrefetch::nsDeferrals::SubmitQueue()
{
  nsCString hostName;
  if (!sDNSService) return;

  while (mHead != mTail) {
    mEntries[mTail].mURI->GetAsciiHost(hostName);
    if (!hostName.IsEmpty()) {
        
      nsCOMPtr<nsICancelable> tmpOutstanding;

      sDNSService->AsyncResolve(hostName, 
                                mEntries[mTail].mFlags,
                                this, nsnull, getter_AddRefs(tmpOutstanding));
    }
    mEntries[mTail].mURI = nsnull;
    mTail = (mTail + 1) & sMaxDeferredMask;
  }
}

void
nsHTMLDNSPrefetch::nsDeferrals::Activate()
{
  // Register as an observer for the document loader  
  nsCOMPtr<nsIWebProgress> progress = 
    do_GetService(NS_DOCUMENTLOADER_SERVICE_CONTRACTID);
  if (progress)
    progress->AddProgressListener(this, nsIWebProgress::NOTIFY_STATE_DOCUMENT);
}

//////////// nsIDNSListener method

NS_IMETHODIMP
nsHTMLDNSPrefetch::nsDeferrals::OnLookupComplete(nsICancelable *request,
                                                 nsIDNSRecord  *rec,
                                                 nsresult       status)
{
  return NS_OK;
}

//////////// nsIWebProgressListener methods

NS_IMETHODIMP 
nsHTMLDNSPrefetch::nsDeferrals::OnStateChange(nsIWebProgress* aWebProgress, 
                                              nsIRequest *aRequest, 
                                              PRUint32 progressStateFlags, 
                                              nsresult aStatus)
{
  // The FIFO has no lock, so it can only be accessed on main thread
  NS_ASSERTION(NS_IsMainThread(), "nsDeferrals::OnStateChange must be on main thread");
  
  if (progressStateFlags & STATE_IS_DOCUMENT) {
    if (progressStateFlags & STATE_STOP) {

      // Initialization may have missed a STATE_START notification, so do
      // not go negative
      if (mActiveLoaderCount)
        mActiveLoaderCount--;

      if (!mActiveLoaderCount)
        SubmitQueue();
    }
    else if (progressStateFlags & STATE_START)
      mActiveLoaderCount++;
  }
            
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDNSPrefetch::nsDeferrals::OnProgressChange(nsIWebProgress *aProgress,
                                                 nsIRequest *aRequest, 
                                                 PRInt32 curSelfProgress, 
                                                 PRInt32 maxSelfProgress, 
                                                 PRInt32 curTotalProgress, 
                                                 PRInt32 maxTotalProgress)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDNSPrefetch::nsDeferrals::OnLocationChange(nsIWebProgress* aWebProgress,
                                                 nsIRequest* aRequest,
                                                 nsIURI *location)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLDNSPrefetch::nsDeferrals::OnStatusChange(nsIWebProgress* aWebProgress,
                                               nsIRequest* aRequest,
                                               nsresult aStatus,
                                               const PRUnichar* aMessage)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLDNSPrefetch::nsDeferrals::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                                 nsIRequest *aRequest, 
                                                 PRUint32 state)
{
  return NS_OK;
}

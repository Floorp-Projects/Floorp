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
 * The Original Code is Google Safe Browsing.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com>
 *   based on JavaScript code by Fritz Schneider <fritz@google.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsCURILoader.h"
#include "nsDocNavStartProgressListener.h"
#include "nsIChannel.h"
#include "nsINestedURI.h"
#include "nsIRequest.h"
#include "nsITimer.h"
#include "nsIURI.h"
#include "nsIWebProgress.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsStringAPI.h"
#include "prlog.h"

NS_IMPL_ISUPPORTS4(nsDocNavStartProgressListener,
                   nsIDocNavStartProgressListener,
                   nsIWebProgressListener,
                   nsIObserver,
                   nsISupportsWeakReference)

// NSPR_LOG_MODULES=DocNavStart:5
#if defined(PR_LOGGING)
static const PRLogModuleInfo *gDocNavStartProgressListenerLog = nsnull;
#define LOG(args) PR_LOG(gDocNavStartProgressListenerLog, PR_LOG_DEBUG, args)
#else
#define LOG(args)
#endif

nsDocNavStartProgressListener::nsDocNavStartProgressListener() :
  mEnabled(PR_FALSE), mDelay(0), mRequests(nsnull), mTimers(nsnull)
{
#if defined(PR_LOGGING)
  if (!gDocNavStartProgressListenerLog)
    gDocNavStartProgressListenerLog = PR_NewLogModule("DocNavStart");
#endif
        
}


nsDocNavStartProgressListener::~nsDocNavStartProgressListener()
{
  // Clean up items left in our queues.
  mRequests.Clear();

  // Cancel pending timers.
  PRUint32 length = mTimers.Count();

  for (PRUint32 i = 0; i < length; ++i) {
    mTimers[i]->Cancel();
  }

  mTimers.Clear();
  
  mCallback = nsnull;
}

// nsDocNavStartProgressListener::AttachListeners

nsresult
nsDocNavStartProgressListener::AttachListeners()
{
  nsresult rv;
  nsCOMPtr<nsIWebProgress> webProgressService = do_GetService(
      NS_DOCUMENTLOADER_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return webProgressService->AddProgressListener(this,
      nsIWebProgress::NOTIFY_LOCATION);
}


// nsDocNavStartProgressListener::DetachListeners

nsresult
nsDocNavStartProgressListener::DetachListeners()
{
  nsresult rv;
  nsCOMPtr<nsIWebProgress> webProgressService = do_GetService(
      NS_DOCUMENTLOADER_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return webProgressService->RemoveProgressListener(this);
}

// Helper method for checking a request URI.
nsresult
nsDocNavStartProgressListener::GetRequestUri(nsIRequest* aReq, nsIURI** uri)
{
  nsCOMPtr<nsIChannel> channel;
  nsresult rv;
  channel = do_QueryInterface(aReq, &rv);
  if (NS_FAILED(rv))
    return rv;

  rv = channel->GetURI(uri);
  if (NS_FAILED(rv))
    return rv;
  return NS_OK;
}


// nsIDocNavStartProgressCallback ***********************************************

// nsDocNavStartProgressListener::GetGlobalProgressListenerEnabled

NS_IMETHODIMP
nsDocNavStartProgressListener::GetGlobalProgressListenerEnabled(PRBool* aEnabled)
{
  *aEnabled = mEnabled;
  return NS_OK;
}


// nsDocNavStartProgressListener::SetGlobalProgressListenerEnabled

NS_IMETHODIMP
nsDocNavStartProgressListener::SetGlobalProgressListenerEnabled(PRBool aEnabled)
{
  if (aEnabled && ! mEnabled) {
    // enable component
    mEnabled = PR_TRUE;
    return AttachListeners();
  } else if (! aEnabled && mEnabled) {
    // disable component
    mEnabled = PR_FALSE;
    return DetachListeners();
  }
  return NS_OK; // nothing to do
}

NS_IMETHODIMP
nsDocNavStartProgressListener::GetDelay(PRUint32* aDelay)
{
  *aDelay = mDelay;
  return NS_OK;
}

NS_IMETHODIMP
nsDocNavStartProgressListener::SetDelay(PRUint32 aDelay)
{
  mDelay = aDelay;
  return NS_OK;
}

// nsDocNavStartProgressListener::GetCallback

NS_IMETHODIMP
nsDocNavStartProgressListener::GetCallback(
    nsIDocNavStartProgressCallback** aCallback)
{
  NS_ENSURE_ARG_POINTER(aCallback);
  *aCallback = mCallback;
  NS_IF_ADDREF(*aCallback);
  return NS_OK;
}


// nsDocNavStartProgressListener::SetCallback

NS_IMETHODIMP
nsDocNavStartProgressListener::SetCallback(
    nsIDocNavStartProgressCallback* aCallback)
{
  mCallback = aCallback;
  return NS_OK;
}

NS_IMETHODIMP
nsDocNavStartProgressListener::IsSpurious(nsIURI* aURI, PRBool* isSpurious)
{
  nsCAutoString scheme;
  nsresult rv = aURI->GetScheme(scheme);
  NS_ENSURE_SUCCESS(rv, rv);

  *isSpurious = scheme.Equals("about") ||
                scheme.Equals("chrome") ||
                scheme.Equals("file") ||
                scheme.Equals("javascript");

  if (!*isSpurious) {
    // If there's a nested URI, we want to check the inner URI's scheme
    nsCOMPtr<nsINestedURI> nestedURI = do_QueryInterface(aURI);
    if (nestedURI) {
      nsCOMPtr<nsIURI> innerURI;
      rv = nestedURI->GetInnerURI(getter_AddRefs(innerURI));
      NS_ENSURE_SUCCESS(rv, rv);
      return IsSpurious(innerURI, isSpurious);
    }
  }

  return NS_OK;
}

// nsIWebProgressListener ******************************************************


// nsDocNavStartProgressListener::OnStateChange

NS_IMETHODIMP
nsDocNavStartProgressListener::OnStateChange(nsIWebProgress *aWebProgress,
                                            nsIRequest *aRequest,
                                            PRUint32 aStateFlags,
                                            nsresult aStatus)
{
  return NS_OK;
}


// nsDocNavStartProgressListener::OnProgressChange

NS_IMETHODIMP
nsDocNavStartProgressListener::OnProgressChange(nsIWebProgress *aWebProgress,
                                               nsIRequest *aRequest,
                                               PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress,
                                               PRInt32 aCurTotalProgress,
                                               PRInt32 aMaxTotalProgress)
{
  return NS_OK;
}


// nsDocNavStartProgressListener::OnLocationChange

NS_IMETHODIMP
nsDocNavStartProgressListener::OnLocationChange(nsIWebProgress *aWebProgress,
                                               nsIRequest *aRequest,
                                               nsIURI *aLocation)
{
  nsresult rv;
  nsCAutoString uriString;
  nsCOMPtr<nsIURI> uri;

  // ignore requests with no URI
  rv = GetRequestUri(aRequest, getter_AddRefs(uri));
  if (NS_FAILED(rv))
    return NS_OK;
  rv = uri->GetAsciiSpec(uriString);
  if (NS_FAILED(rv))
    return NS_OK;

  LOG(("Firing OnLocationChange for %s", uriString.get()));

  // We store the request and a timer in queue.  When the timer fires,
  // we use the request in the front of the queue.
  nsCOMPtr<nsITimer> timer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  NS_ENSURE_TRUE(timer, rv);

  rv = timer->Init(this, mDelay, nsITimer::TYPE_ONE_SHOT);
  NS_ENSURE_SUCCESS(rv, rv);

  mRequests.AppendObject(aRequest);
  mTimers.AppendObject(timer);

  return NS_OK;
}


// nsDocNavStartProgressListener::OnStatusChange

NS_IMETHODIMP
nsDocNavStartProgressListener::OnStatusChange(nsIWebProgress *aWebProgress,
                                             nsIRequest *aRequest,
                                             nsresult aStatus,
                                             const PRUnichar *aMessage)
{
  return NS_OK;
}


// nsDocNavStartProgressListener::OnSecurityChange

NS_IMETHODIMP
nsDocNavStartProgressListener::OnSecurityChange(nsIWebProgress *aWebProgress,
                                               nsIRequest *aRequest,
                                               PRUint32 aState)
{
  return NS_OK;
}

// nsIObserver ****************************************************************

NS_IMETHODIMP
nsDocNavStartProgressListener::Observe(nsISupports *subject, const char *topic,
                                       const PRUnichar *data)
{
  if (strcmp(topic, NS_TIMER_CALLBACK_TOPIC) == 0) {
    // Timer callback, pop the front of the request queue and call the callback.
#ifdef DEBUG
    PRUint32 length = mRequests.Count();
    NS_ASSERTION(length > 0, "timer callback with empty request queue?");
    length = mTimers.Count();
    NS_ASSERTION(length > 0, "timer callback with empty timer queue?");
#endif

    nsIRequest* request = mRequests[0];

    if (mCallback) {
      PRBool isSpurious;

      nsCOMPtr<nsIURI> uri;
      nsresult rv = GetRequestUri(request, getter_AddRefs(uri));
      NS_ENSURE_SUCCESS(rv, rv);
      
      rv = IsSpurious(uri, &isSpurious);
      NS_ENSURE_SUCCESS(rv, rv);

      if (!isSpurious) {
        nsCString uriString;
        rv = uri->GetAsciiSpec(uriString);
        NS_ENSURE_SUCCESS(rv, rv);
        
        // We don't care about URL fragments so we take that off.
        PRInt32 pos = uriString.FindChar('#');
        if (pos > -1) {
          uriString.SetLength(pos);
        }
        
        LOG(("Firing DocNavStart for %s", uriString.get()));
        mCallback->OnDocNavStart(request, uriString);
      }
    }

    mRequests.RemoveObjectAt(0);
    mTimers.RemoveObjectAt(0);
  }
  return NS_OK;
}

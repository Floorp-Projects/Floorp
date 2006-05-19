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
#include "nsIRequest.h"
#include "nsIWebProgress.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"

NS_IMPL_ISUPPORTS3(nsDocNavStartProgressListener,
                   nsIDocNavStartProgressListener,
                   nsIWebProgressListener,
                   nsISupportsWeakReference)

nsDocNavStartProgressListener::nsDocNavStartProgressListener() :
  mEnabled(PR_FALSE)
{

}


nsDocNavStartProgressListener::~nsDocNavStartProgressListener()
{

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
      nsIWebProgress::NOTIFY_STATE_DOCUMENT);
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

// nsIDocNavStartProgressCallback ***********************************************

// nsDocNavStartProgressListener::GetEnabled

NS_IMETHODIMP
nsDocNavStartProgressListener::GetEnabled(PRBool* aEnabled)
{
  *aEnabled = mEnabled;
  return NS_OK;
}


// nsDocNavStartProgressListener::SetEnabled

NS_IMETHODIMP
nsDocNavStartProgressListener::SetEnabled(PRBool aEnabled)
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


// nsIWebProgressListener ******************************************************


// nsDocNavStartProgressListener::OnStateChange

NS_IMETHODIMP
nsDocNavStartProgressListener::OnStateChange(nsIWebProgress *aWebProgress,
                                            nsIRequest *aRequest,
                                            PRUint32 aStateFlags,
                                            nsresult aStatus)
{
  if (mEnabled && aStateFlags & STATE_START && aStateFlags & STATE_IS_REQUEST) {
    // might be for us, check load flags
    nsresult rv;
#ifdef DEBUG
    nsLoadFlags loadFlags;
    rv = aRequest->GetLoadFlags(&loadFlags);
    NS_ASSERTION(NS_SUCCEEDED(rv) && loadFlags & nsIChannel::LOAD_DOCUMENT_URI,
                 "Unexpected load flags, we only registered for loads");
#endif
    // send notification
    nsCAutoString name;
    rv = aRequest->GetName(name);
    if (NS_FAILED(rv))
      return NS_OK; // ignore requests with no name
    if (mCallback)
      mCallback->OnDocNavStart(aRequest, name);
  }
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


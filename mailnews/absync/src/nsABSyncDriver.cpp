/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsABSyncDriver.h"
#include "nsIAbSync.h"
#include "nsAbSyncCID.h"
#include "nsIServiceManager.h"
#include "nsTextFormatter.h"
#include "nsIStringBundle.h"
#include "prmem.h"
#include "nsString.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(nsAbSyncDriver, nsIAbSyncDriver)
//NS_IMPL_ISUPPORTS1(nsAbSyncDriver, nsIAbSyncDriver)

static NS_DEFINE_CID(kAbSync, NS_ABSYNC_SERVICE_CID);
static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

nsAbSyncDriver::nsAbSyncDriver()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */

  mTransactionID = -1;
  mStatus = nsnull;
}

nsAbSyncDriver::~nsAbSyncDriver()
{
  /* destructor code */
}

/**
  * Notify the observer that the AB Sync Authorization operation has begun. 
  *
  */
NS_IMETHODIMP 
nsAbSyncDriver::OnStartAuthOperation(void)
{
  if (mStatus)
  {
    PRUnichar   *outValue = nsnull;

    // Tweak the button...
    mStatus->StartMeteors();
    mStatus->ShowProgress(0);

    outValue = GetString(NS_LITERAL_STRING("syncStartingAuth").get());
    
    mStatus->ShowStatusString(outValue);
    PR_FREEIF(outValue);
  }

  return NS_OK;
}

/**
   * Notify the observer that the AB Sync operation has been completed.  
   * 
   * This method is called regardless of whether the the operation was 
   * successful.
   * 
   *  aTransactionID    - the ID for this particular request
   *  aStatus           - Status code for the sync request
   *  aMsg              - A text string describing the error (if any).
   *  aCookie           - hmmm...cooookies!
   */
NS_IMETHODIMP 
nsAbSyncDriver::OnStopAuthOperation(nsresult aStatus, const PRUnichar *aMsg, const char *aCookie)
{
  if (mStatus)
  {
    PRUnichar   *outValue = nsnull;

    if (NS_SUCCEEDED(aStatus))
      outValue = GetString(NS_LITERAL_STRING("syncAuthSuccess").get());
    else
      outValue = GetString(NS_LITERAL_STRING("syncAuthFailed").get());
    
    mStatus->ShowStatusString(outValue);
    PR_FREEIF(outValue);
  }

  return NS_OK;
}

/* void OnStartOperation (in PRInt32 aTransactionID, in PRUint32 aMsgSize); */
NS_IMETHODIMP nsAbSyncDriver::OnStartOperation(PRInt32 aTransactionID, PRUint32 aMsgSize)
{
  if (mStatus)
  {
    PRUnichar   *outValue = nsnull;
    PRUnichar   *msgValue = nsnull;

    // Tweak the button...
    mStatus->StartMeteors();
    mStatus->ShowProgress(50);

    outValue = GetString(NS_LITERAL_STRING("syncStarting").get());
    msgValue = nsTextFormatter::smprintf(outValue, aMsgSize);

    mStatus->ShowStatusString(msgValue);

    PR_FREEIF(outValue);
    PR_FREEIF(msgValue);
  }

  return NS_OK;
}

/* void OnProgress (in PRInt32 aTransactionID, in PRUint32 aProgress, in PRUint32 aProgressMax); */
NS_IMETHODIMP nsAbSyncDriver::OnProgress(PRInt32 aTransactionID, PRUint32 aProgress, PRUint32 aProgressMax)
{
  if (mStatus)
  {
    PRUnichar   *outValue = nsnull;
    PRUnichar   *msgValue = nsnull;

    outValue = GetString(NS_LITERAL_STRING("syncProgress").get());
    msgValue = nsTextFormatter::smprintf(outValue, aProgress);

    mStatus->ShowStatusString(msgValue);
    PR_FREEIF(outValue);
    PR_FREEIF(msgValue);
  }

  return NS_OK;
}

/* void OnStatus (in PRInt32 aTransactionID, in wstring aMsg); */
NS_IMETHODIMP nsAbSyncDriver::OnStatus(PRInt32 aTransactionID, const PRUnichar *aMsg)
{
  return mStatus->ShowStatusString(aMsg);
}

/* void OnStopOperation (in PRInt32 aTransactionID, in nsresult aStatus, in wstring aMsg); */
NS_IMETHODIMP nsAbSyncDriver::OnStopOperation(PRInt32 aTransactionID, nsresult aStatus, const PRUnichar *aMsg)
{
  if (mStatus)
  {
    PRUnichar   *outValue = nsnull;

    // Tweak the button...
    mStatus->StopMeteors();
    mStatus->CloseWindow();

    if (NS_SUCCEEDED(aStatus))
      outValue = GetString(NS_LITERAL_STRING("syncDoneSuccess").get());
    else
    {
      if (mCancelled)
        outValue = GetString(NS_LITERAL_STRING("syncDoneCancelled").get());
      else
        outValue = GetString(NS_LITERAL_STRING("syncDoneFailed").get());
    }
    
    mStatus->ShowStatusString(outValue);
    PR_FREEIF(outValue);
  }

  return NS_OK;
}

/* void KickIt (); */
NS_IMETHODIMP nsAbSyncDriver::KickIt(nsIMsgStatusFeedback *aStatus, nsIDOMWindowInternal *aDocShell)
{
  nsresult rv = NS_OK;
  PRInt32  stateVar;
	nsCOMPtr<nsIAbSync> sync(do_GetService(kAbSync, &rv)); 
	if (NS_FAILED(rv) || !sync) 
		return rv;

  mCancelled = PR_FALSE;
  sync->GetCurrentState(&stateVar);
  if (stateVar != nsIAbSyncState::nsIAbSyncIdle)
    return NS_ERROR_FAILURE;

  mStatus = aStatus;

  // Add ourselves to the party!
  sync->AddSyncListener((nsIAbSyncListener *)this);

  rv = sync->PerformAbSync(aDocShell, &mTransactionID);
  if (NS_SUCCEEDED(rv))
  {
    if (mStatus)
    {
      PRUnichar   *msgValue = nsnull;
      msgValue = GetString(NS_LITERAL_STRING("syncStartingAuth").get());
      mStatus->ShowStatusString(msgValue);
      PR_FREEIF(msgValue);
    }
  }
  else
  {
    // We failed, turn the button back on...
    mStatus->StopMeteors();
    mStatus->CloseWindow();
  }
  return rv;
}

#define AB_STRING_URL       "chrome://messenger/locale/addressbook/absync.properties"

PRUnichar *
nsAbSyncDriver::GetString(const PRUnichar *aStringName)
{
	nsresult    res = NS_OK;
  PRUnichar   *ptrv = nsnull;

	if (!mStringBundle)
	{
		static const char propertyURL[] = AB_STRING_URL;

		nsCOMPtr<nsIStringBundleService> sBundleService = 
		         do_GetService(kStringBundleServiceCID, &res); 
		if (NS_SUCCEEDED(res) && (nsnull != sBundleService)) 
		{
			res = sBundleService->CreateBundle(propertyURL, getter_AddRefs(mStringBundle));
		}
	}

	if (mStringBundle)
		res = mStringBundle->GetStringFromName(aStringName, &ptrv);

  if ( NS_SUCCEEDED(res) && (ptrv) )
    return ptrv;
  else
    return nsCRT::strdup(aStringName);
}

// 
// What if we want to stop the operation? Call this!
//
NS_IMETHODIMP nsAbSyncDriver::CancelIt()
{
  nsresult rv = NS_OK;
  PRInt32  stateVar;

  mCancelled = PR_TRUE;
  nsCOMPtr<nsIAbSync> sync(do_GetService(kAbSync, &rv)); 
	if (NS_FAILED(rv) || !sync) 
		return rv;

  sync->GetCurrentState(&stateVar);
  if (stateVar == nsIAbSyncState::nsIAbSyncIdle)
    return NS_ERROR_FAILURE;

  // Cancel this mess...
  return sync->CancelAbSync();
}

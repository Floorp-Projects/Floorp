/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsABSyncDriver.h"
#include "nsIAbSync.h"
#include "nsAbSyncCID.h"
#include "nsIServiceManager.h"
#include "nsTextFormatter.h"
#include "nsIStringBundle.h"
#include "prmem.h"

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

    outValue = GetString(NS_ConvertASCIItoUCS2("syncStartingAuth").GetUnicode());
    
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
      outValue = GetString(NS_ConvertASCIItoUCS2("syncAuthSuccess").GetUnicode());
    else
      outValue = GetString(NS_ConvertASCIItoUCS2("syncAuthFailed").GetUnicode());
    
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

    outValue = GetString(NS_ConvertASCIItoUCS2("syncStarting").GetUnicode());
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

    outValue = GetString(NS_ConvertASCIItoUCS2("syncProgress").GetUnicode());
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
      outValue = GetString(NS_ConvertASCIItoUCS2("syncDoneSuccess").GetUnicode());
    else
    {
      if (mCancelled)
        outValue = GetString(NS_ConvertASCIItoUCS2("syncDoneCancelled").GetUnicode());
      else
        outValue = GetString(NS_ConvertASCIItoUCS2("syncDoneFailed").GetUnicode());
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
	NS_WITH_SERVICE(nsIAbSync, sync, kAbSync, &rv); 
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
      msgValue = GetString(NS_ConvertASCIItoUCS2("syncStartingAuth").GetUnicode());
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
		char    *propertyURL = AB_STRING_URL;

		NS_WITH_SERVICE(nsIStringBundleService, sBundleService, kStringBundleServiceCID, &res); 
		if (NS_SUCCEEDED(res) && (nsnull != sBundleService)) 
		{
			nsILocale   *locale = nsnull;
			res = sBundleService->CreateBundle(propertyURL, locale, getter_AddRefs(mStringBundle));
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
  NS_WITH_SERVICE(nsIAbSync, sync, kAbSync, &rv); 
	if (NS_FAILED(rv) || !sync) 
		return rv;

  sync->GetCurrentState(&stateVar);
  if (stateVar == nsIAbSyncState::nsIAbSyncIdle)
    return NS_ERROR_FAILURE;

  // Cancel this mess...
  return sync->CancelAbSync();
}

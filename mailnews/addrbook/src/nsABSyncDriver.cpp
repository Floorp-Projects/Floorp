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
#include "nsAbSyncDriver.h"
#include "nsIAbSync.h"
#include "nsAbSyncCID.h"
#include "nsIServiceManager.h"

NS_IMPL_ISUPPORTS1(nsAbSyncDriver, nsIAbSyncDriver)

static NS_DEFINE_CID(kAbSync, NS_ABSYNC_SERVICE_CID);

nsAbSyncDriver::nsAbSyncDriver()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */

  mTransactionID = -1;
}

nsAbSyncDriver::~nsAbSyncDriver()
{
  /* destructor code */
}

/* void OnStartOperation (in PRInt32 aTransactionID, in PRUint32 aMsgSize); */
NS_IMETHODIMP nsAbSyncDriver::OnStartOperation(PRInt32 aTransactionID, PRUint32 aMsgSize)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void OnProgress (in PRInt32 aTransactionID, in PRUint32 aProgress, in PRUint32 aProgressMax); */
NS_IMETHODIMP nsAbSyncDriver::OnProgress(PRInt32 aTransactionID, PRUint32 aProgress, PRUint32 aProgressMax)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void OnStatus (in PRInt32 aTransactionID, in wstring aMsg); */
NS_IMETHODIMP nsAbSyncDriver::OnStatus(PRInt32 aTransactionID, const PRUnichar *aMsg)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void OnStopOperation (in PRInt32 aTransactionID, in nsresult aStatus, in wstring aMsg); */
NS_IMETHODIMP nsAbSyncDriver::OnStopOperation(PRInt32 aTransactionID, nsresult aStatus, const PRUnichar *aMsg)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void KickIt (); */
NS_IMETHODIMP nsAbSyncDriver::KickIt()
{
  nsresult rv = NS_OK;
	NS_WITH_SERVICE(nsIAbSync, sync, kAbSync, &rv); 
	if (NS_FAILED(rv) || !sync) 
		return rv;

  // Add ourselves to the party!
  sync->AddSyncListener((nsIAbSyncListener *)this);

  rv = sync->PerformAbSync(&mTransactionID);
  return rv;
}

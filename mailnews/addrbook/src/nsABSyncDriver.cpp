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
	nsCOMPtr<nsIAbSync> sync(do_GetService(kAbSync, &rv)); 
  NS_ENSURE_SUCCESS(rv, rv);

  // Add ourselves to the party!
  sync->AddSyncListener((nsIAbSyncListener *)this);

  rv = sync->PerformAbSync(&mTransactionID);
  return rv;
}

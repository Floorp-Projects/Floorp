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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Jan Varga
 * Portions created by the Initial Developer are Copyright (C) 2003
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozSqlConnection.h"
#include "mozSqlRequest.h"

mozSqlRequest::mozSqlRequest(mozISqlConnection* aConnection)
  : mAffectedRows(-1),
    mIsQuery(PR_TRUE),
    mStatus(mozISqlRequest::STATUS_NONE)
{
  mConnection = do_GetWeakReference(aConnection);
}

mozSqlRequest::~mozSqlRequest()
{
}


NS_IMPL_THREADSAFE_ISUPPORTS1(mozSqlRequest,
                              mozISqlRequest);

NS_IMETHODIMP
mozSqlRequest::GetErrorMessage(nsAString & aErrorMessage)
{
  aErrorMessage = mErrorMessage;
  return NS_OK;
}

NS_IMETHODIMP
mozSqlRequest::GetResult(mozISqlResult * *aResult)
{
  NS_IF_ADDREF(*aResult = mResult);
  return NS_OK;
}

NS_IMETHODIMP
mozSqlRequest::GetAffectedRows(PRInt32 *aAffectedRows)
{
  *aAffectedRows = mAffectedRows;
  return NS_OK;
}

NS_IMETHODIMP
mozSqlRequest::GetLastID(PRInt32* aLastID)
{
  *aLastID = mLastID;
  return NS_OK;
}

NS_IMETHODIMP
mozSqlRequest::GetQuery(nsAString & aQuery)
{
  aQuery = mQuery;
  return NS_OK;
}

NS_IMETHODIMP
mozSqlRequest::GetCtxt(nsISupports * *aCtxt)
{
  NS_IF_ADDREF(*aCtxt = mCtxt);
  return NS_OK;
}

NS_IMETHODIMP
mozSqlRequest::GetObserver(mozISqlRequestObserver * *aObserver)
{
  NS_IF_ADDREF(*aObserver = mObserver);
  return NS_OK;
}

NS_IMETHODIMP
mozSqlRequest::GetStatus(PRInt32 *aStatus)
{
  *aStatus = mStatus;
  return NS_OK;
}

NS_IMETHODIMP
mozSqlRequest::Cancel()
{
  nsCOMPtr<mozISqlConnection> connection = do_QueryReferent(mConnection);
  if (!connection)
    return NS_ERROR_FAILURE;

  mozISqlConnection* connectionRaw = connection.get();
  return ((mozSqlConnection*)connectionRaw)->CancelRequest(this);
}

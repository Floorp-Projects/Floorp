#include "mozSqlConnection.h"
#include "mozSqlRequest.h"

mozSqlRequest::mozSqlRequest(mozISqlConnection* aConnection)
  : mAffectedRows(-1),
    mIsQuery(PR_TRUE),
    mStatus(mozISqlRequest::STATUS_NONE)
{
  NS_INIT_ISUPPORTS();
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

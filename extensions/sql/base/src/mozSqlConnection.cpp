#include "nsIProxyObjectManager.h"
#include "mozSqlRequest.h"
#include "mozSqlConnection.h"

mozSqlConnection::mozSqlConnection()
  : mLock(nsnull),
    mCondVar(nsnull),
    mThread(nsnull),
    mShutdown(PR_FALSE),
    mWaiting(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
  mExecLock = PR_NewLock();
}

mozSqlConnection::~mozSqlConnection()
{
  mRequests.Clear();

  if (mCondVar)
    PR_DestroyCondVar(mCondVar);
  PR_DestroyLock(mExecLock);
  if (mLock)
    PR_DestroyLock(mLock);
}

// We require a special implementation of Release, which knows about
// a circular strong reference
NS_IMPL_THREADSAFE_ADDREF(mozSqlConnection)
NS_IMPL_THREADSAFE_QUERY_INTERFACE3(mozSqlConnection,
                                    mozISqlConnection,
                                    nsIRunnable,
				    nsISupportsWeakReference)
NS_IMETHODIMP_(nsrefcnt)
mozSqlConnection::Release()
{
  PR_AtomicDecrement((PRInt32*)&mRefCnt);
  // Delete if the last reference is our strong circular reference.
  if (mThread && mRefCnt == 1) {
    PR_Lock(mLock);
    mRequests.Clear();
    mShutdown = PR_TRUE;
    if (mWaiting)
      PR_NotifyCondVar(mCondVar);
    else
      CancelExec();
    PR_Unlock(mLock);
    return 0;
  }
  else if (mRefCnt == 0) {
    delete this;
    return 0;
  }
  return mRefCnt;
}

NS_IMETHODIMP
mozSqlConnection::GetServerVersion(nsAString& aServerVersion) 
{
  aServerVersion = mServerVersion;
  return NS_OK;
}

NS_IMETHODIMP
mozSqlConnection::GetErrorMessage(nsAString& aErrorMessage) 
{
  aErrorMessage = mErrorMessage;
  return NS_OK;
}

NS_IMETHODIMP
mozSqlConnection::GetLastID(PRInt32* aLastID) 
{
  *aLastID = mLastID;
  return NS_OK;
}

NS_IMETHODIMP
mozSqlConnection::Init(const nsAString & aHost, PRInt32 aPort,
                       const nsAString & aDatabase, const nsAString & aUsername,
                       const nsAString & aPassword)
{
  // descendants have to implement this themselves
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozSqlConnection::ExecuteQuery(const nsAString& aQuery, mozISqlResult** _retval)
{
  PR_Lock(mExecLock);
  nsresult rv = RealExec(aQuery, _retval, nsnull);
  PR_Unlock(mExecLock);
  return rv;
}

NS_IMETHODIMP
mozSqlConnection::ExecuteUpdate(const nsAString& aUpdate, PRInt32* _retval)
{
  PR_Lock(mExecLock);
  nsresult rv = RealExec(aUpdate, nsnull, _retval);
  PR_Unlock(mExecLock);
  return rv;
}

NS_IMETHODIMP
mozSqlConnection::AsyncExecuteQuery(const nsAString& aQuery, nsISupports* aCtxt, 
                                    mozISqlRequestObserver* aObserver,
                                    mozISqlRequest **_retval)
{
  if (!mThread) {
    mLock = PR_NewLock();
    mCondVar = PR_NewCondVar(mLock);
    NS_NewThread(getter_AddRefs(mThread), this, 0, PR_UNJOINABLE_THREAD);
  }

  mozSqlRequest* request = new mozSqlRequest(this);
  if (! request)
    return NS_ERROR_OUT_OF_MEMORY;

  request->mIsQuery = PR_TRUE;
  request->mQuery = aQuery;
  request->mCtxt = aCtxt;

  nsresult rv = NS_GetProxyForObject(NS_CURRENT_EVENTQ,
                                     NS_GET_IID(mozISqlRequestObserver),
                                     aObserver,
                                     PROXY_SYNC | PROXY_ALWAYS,
                                     getter_AddRefs(request->mObserver));
  if (NS_FAILED(rv))
    return rv;

  PR_Lock(mLock);
  mRequests.AppendObject(request);
  if (mWaiting && mRequests.Count() == 1)
    PR_NotifyCondVar(mCondVar);
  PR_Unlock(mLock);

  NS_ADDREF(*_retval = request);

  return NS_OK;
}

NS_IMETHODIMP
mozSqlConnection::AsyncExecuteUpdate(const nsAString& aQuery, nsISupports* aCtxt, 
                                     mozISqlRequestObserver* aObserver,
                                     mozISqlRequest **_retval)
{
  return NS_OK;
}

NS_IMETHODIMP
mozSqlConnection::BeginTransaction()
{
  PRInt32 affectedRows;
  return ExecuteUpdate(NS_LITERAL_STRING("begin"), &affectedRows);
}

NS_IMETHODIMP
mozSqlConnection::CommitTransaction()
{
  PRInt32 affectedRows;
  return ExecuteUpdate(NS_LITERAL_STRING("commit"), &affectedRows);
}

NS_IMETHODIMP
mozSqlConnection::RollbackTransaction()
{
  PRInt32 affectedRows;
  return ExecuteUpdate(NS_LITERAL_STRING("rollback"), &affectedRows);
}

NS_IMETHODIMP
mozSqlConnection::GetPrimaryKeys(const nsAString& aSchema, const nsAString& aTable, mozISqlResult** _retval)
{
  // descendants have to implement this themselves
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozSqlConnection::Run()
{
  while(!mShutdown) {
    PR_Lock(mLock);

    while (mRequests.Count()) {
      mCurrentRequest = mRequests[0];
      mRequests.RemoveObjectAt(0);


      mozSqlRequest* r = (mozSqlRequest*)mCurrentRequest.get();

      r->mObserver->OnStartRequest(mCurrentRequest, r->mCtxt);

      r->mStatus = mozISqlRequest::STATUS_EXECUTED;

      PR_Unlock(mLock);

      nsresult rv = ExecuteQuery(r->mQuery, getter_AddRefs(r->mResult));

      PR_Lock(mLock);

      if (NS_SUCCEEDED(rv))
        r->mStatus = mozISqlRequest::STATUS_COMPLETE;
      else {
        r->mStatus = mozISqlRequest::STATUS_ERROR;
	GetErrorMessage(r->mErrorMessage);
      }
        
      r->mObserver->OnStopRequest(mCurrentRequest, r->mCtxt);

      mCurrentRequest = nsnull;

    }
    
    mWaiting = PR_TRUE;
    PR_WaitCondVar(mCondVar, PR_INTERVAL_NO_TIMEOUT);
    mWaiting = PR_FALSE;
    PR_Unlock(mLock);
  }

  return NS_OK;
}

nsresult
mozSqlConnection::CancelRequest(mozISqlRequest* aRequest)
{
  PR_Lock(mLock);
  if (mCurrentRequest == aRequest)
    CancelExec();
  else {
    if (mRequests.RemoveObject(aRequest))
      ((mozSqlRequest*)aRequest)->mStatus = mozISqlRequest::STATUS_CANCELLED;
  }
  PR_Unlock(mLock);

  return NS_OK;
}

#ifndef mozSqlConnection_h
#define mozSqlConnection_h                                                  

#include "prcvar.h"
#include "nsString.h"
#include "nsCOMArray.h"
#include "nsWeakReference.h"
#include "nsIThread.h"
#include "nsIRunnable.h"
#include "mozISqlConnection.h"
#include "mozISqlRequest.h"
#include "mozISqlResult.h"

class mozSqlConnection : public mozISqlConnection,
                         public nsIRunnable,
                         public nsSupportsWeakReference
{
  public:
    mozSqlConnection();
    virtual ~mozSqlConnection();

    NS_DECL_ISUPPORTS

    NS_DECL_MOZISQLCONNECTION

    NS_DECL_NSIRUNNABLE

    friend class mozSqlRequest;
    friend class mozSqlResult;

  protected:
    virtual nsresult RealExec(const nsAString& aQuery,
                              mozISqlResult** aResult, PRInt32* aAffectedRows) = 0;
    virtual nsresult CancelExec() = 0;
    virtual nsresult GetIDName(nsAString& aIDName) = 0;

    nsresult CancelRequest(mozISqlRequest* aRequest);

    nsString                    mServerVersion;
    nsString                    mErrorMessage;
    PRInt32                     mLastID;

    PRLock*                     mLock;
    PRCondVar*                  mCondVar;
    PRLock*                     mExecLock;
    nsCOMPtr<nsIThread>         mThread;
    nsCOMArray<mozISqlRequest>  mRequests;
    nsCOMPtr<mozISqlRequest>    mCurrentRequest;
    PRBool                      mShutdown;
    PRBool                      mWaiting;
};

#endif // mozSqlConnection_h

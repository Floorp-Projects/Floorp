#ifndef mozSqlRequest_h
#define mozSqlRequest_h                                                  

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIWeakReference.h"
#include "mozISqlConnection.h"
#include "mozISqlResult.h"
#include "mozISqlRequest.h"
#include "mozISqlRequestObserver.h"

class mozSqlRequest : public mozISqlRequest
{
  public:
    mozSqlRequest(mozISqlConnection* aConnection);
    virtual ~mozSqlRequest();

    NS_DECL_ISUPPORTS

    NS_DECL_MOZISQLREQUEST

    friend class mozSqlConnection;

  protected:
    nsCOMPtr<nsIWeakReference>          mConnection;

    nsString                            mErrorMessage;
    nsCOMPtr<mozISqlResult>             mResult;
    PRInt32                             mAffectedRows;
    PRInt32                             mLastID;

    PRBool                              mIsQuery;
    nsString                            mQuery;
    nsCOMPtr<nsISupports>               mCtxt;
    nsCOMPtr<mozISqlRequestObserver>    mObserver;

    PRInt32                             mStatus;

};

#endif // mozSqlRequest_h

#ifndef mozSqlResultPgsql_h
#define mozSqlResultPgsql_h                                                  

#include "libpq-fe.h"
#include "mozSqlResult.h"
#include "mozISqlResultPgsql.h"

class mozSqlResultPgsql : public mozSqlResult,
                          public mozISqlResultPgsql
{
  public:
    mozSqlResultPgsql(mozISqlConnection* aConnection,
                      const nsAString& aQuery);
    void SetResult(PGresult* aResult,
                   PGresult* aTypes);
    virtual ~mozSqlResultPgsql();

    NS_DECL_ISUPPORTS

    NS_DECL_MOZISQLRESULTPGSQL 

  protected:
    PRInt32 GetColType(PRInt32 aColumnIndex);

    virtual nsresult BuildColumnInfo();
    virtual nsresult BuildRows();
    virtual void ClearNativeResult();

    nsresult EnsureTablePrivileges();
    virtual nsresult CanInsert(PRBool* _retval);
    virtual nsresult CanUpdate(PRBool* _retval);
    virtual nsresult CanDelete(PRBool* _retval);

  private:
    PGresult*   mResult;
    PGresult*   mTypes;
};

#endif // mozSqlResultPgsql_h

#ifndef mozSqlConnectionPgsql_h
#define mozSqlConnectionPgsql_h                                                  

#include "libpq-fe.h"
#include "mozSqlConnection.h"
#include "mozISqlConnectionPgsql.h"

#define MOZ_SQLCONNECTIONPGSQL_CLASSNAME "PosgreSQL SQL Connection"
#define MOZ_SQLCONNECTIONPGSQL_CID \
{0x0cf1eefe, 0x611d, 0x48fa, {0xae, 0x27, 0x0a, 0x6f, 0x40, 0xd6, 0xa3, 0x3e }}
#define MOZ_SQLCONNECTIONPGSQL_CONTRACTID "@mozilla.org/sql/connection;1?type=pgsql"

#define SERVER_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))

class mozSqlConnectionPgsql : public mozSqlConnection,
                              public mozISqlConnectionPgsql
{
  public:
    mozSqlConnectionPgsql();
    virtual ~mozSqlConnectionPgsql();

    NS_DECL_ISUPPORTS

    NS_IMETHOD Init(const nsAString& aHost, PRInt32 aPort,
                    const nsAString& aDatabase, const nsAString& aUsername,
		    const nsAString& aPassword);

    NS_IMETHOD GetPrimaryKeys(const nsAString& aSchema, const nsAString& aTable, mozISqlResult** _retval);

    NS_DECL_MOZISQLCONNECTIONPGSQL

  protected:
    nsresult Setup();

    virtual nsresult RealExec(const nsAString& aQuery,
                              mozISqlResult** aResult, PRInt32* aAffectedRows);

    virtual nsresult CancelExec();

    virtual nsresult GetIDName(nsAString& aIDName);

  private:
    PGconn*             mConnection;
    PRInt32             mVersion;
};

#endif // mozSqlConnectionPgsql_h

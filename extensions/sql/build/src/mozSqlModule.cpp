#include "nsIGenericFactory.h"
#include "mozSqlService.h"
#ifdef MOZ_ENABLE_PGSQL
#include "mozSqlConnectionPgsql.h"
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(mozSqlService, Init)
#ifdef MOZ_ENABLE_PGSQL
NS_GENERIC_FACTORY_CONSTRUCTOR(mozSqlConnectionPgsql)
#endif

static nsModuleComponentInfo components[] =
{
  { MOZ_SQLSERVICE_CLASSNAME,
    MOZ_SQLSERVICE_CID,
    MOZ_SQLSERVICE_CONTRACTID,
    mozSqlServiceConstructor
  },
  { MOZ_SQLSERVICE_CLASSNAME,
    MOZ_SQLSERVICE_CID,
    MOZ_SQLDATASOURCE_CONTRACTID,
    mozSqlServiceConstructor
  },
#ifdef MOZ_ENABLE_PGSQL
  { MOZ_SQLCONNECTIONPGSQL_CLASSNAME,
    MOZ_SQLCONNECTIONPGSQL_CID,
    MOZ_SQLCONNECTIONPGSQL_CONTRACTID,
    mozSqlConnectionPgsqlConstructor
  }
#endif
};

NS_IMPL_NSGETMODULE("sql", components)

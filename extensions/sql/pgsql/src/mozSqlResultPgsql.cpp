#include "nsReadableUtils.h"
#include "mozSqlResultPgsql.h"

mozSqlResultPgsql::mozSqlResultPgsql(mozISqlConnection* aConnection,
                                     const nsAString& aQuery)
  : mozSqlResult(aConnection, aQuery),
    mResult(nsnull),
    mTypes(nsnull)
{
}

void
mozSqlResultPgsql::SetResult(PGresult* aResult,
                             PGresult* aTypes)
{
  mResult = aResult;
  mTypes = aTypes;
}

mozSqlResultPgsql::~mozSqlResultPgsql()
{
  ClearNativeResult();
}

NS_IMPL_ADDREF_INHERITED(mozSqlResultPgsql, mozSqlResult)
NS_IMPL_RELEASE_INHERITED(mozSqlResultPgsql, mozSqlResult)

// QueryInterface
NS_INTERFACE_MAP_BEGIN(mozSqlResultPgsql)
  NS_INTERFACE_MAP_ENTRY(mozISqlResultPgsql)
NS_INTERFACE_MAP_END_INHERITING(mozSqlResult)

PRInt32
mozSqlResultPgsql::GetColType(PRInt32 aColumnIndex)
{
  PRInt32 oid = PQftype(mResult, aColumnIndex);

  for (PRInt32 i = 0; i < PQntuples(mTypes); i++) {
    char* value = PQgetvalue(mTypes, i, 0);
    PRInt32 o;
    sscanf(value, "%d", &o);
    if (o == oid) {
      char* type = PQgetvalue(mTypes, i, 1);
      if (! strcmp(type, "int2"))
        return mozISqlResult::TYPE_INT;
      else if (! strcmp(type, "int4"))
        return mozISqlResult::TYPE_INT;
      else if (! strcmp(type, "float4"))
        return mozISqlResult::TYPE_STRING;
      else if (! strcmp(type, "numeric"))
        return mozISqlResult::TYPE_STRING;
      else if (! strcmp(type, "date"))
        return mozISqlResult::TYPE_STRING;
      else if (! strcmp(type, "time"))
        return mozISqlResult::TYPE_STRING;
      else if (! strcmp(type, "timestamp"))
        return mozISqlResult::TYPE_STRING;
      else if (! strcmp(type, "bool"))
        return mozISqlResult::TYPE_BOOL;
      else
        return mozISqlResult::TYPE_STRING;
    }
  }

  return mozISqlResult::TYPE_STRING;
}

nsresult
mozSqlResultPgsql::BuildColumnInfo()
{
  for (PRInt32 i = 0; i < PQnfields(mResult); i++) {
    char* n = PQfname(mResult, i);
    PRUnichar* name = ToNewUnicode(NS_ConvertUTF8toUCS2(n));
    PRInt32 type = GetColType(i);
    PRInt32 size = PQfsize(mResult, i);
    PRInt32 mod = PQfmod(mResult, i);

    nsCAutoString uri(NS_LITERAL_CSTRING("http://www.mozilla.org/SQL-rdf#"));
    uri.Append(n);
    nsCOMPtr<nsIRDFResource> property;
    gRDFService->GetResource(uri.get(), getter_AddRefs(property));

    ColumnInfo* columnInfo = ColumnInfo::Create(mAllocator, name, type, size, mod, property);
    mColumnInfo.AppendElement(columnInfo); 
  }

  return NS_OK;
}

nsresult
mozSqlResultPgsql::BuildRows()
{
  for(PRInt32 i = 0; i < PQntuples(mResult); i++) {
    nsCOMPtr<nsIRDFResource> resource;
    nsresult rv = gRDFService->GetAnonymousResource(getter_AddRefs(resource));
    if (NS_FAILED(rv)) return rv;

    Row* row = Row::Create(mAllocator, resource, mColumnInfo);

    for (PRInt32 j = 0; j < mColumnInfo.Count(); j++) {
      if (! PQgetisnull(mResult, i, j)) {
        char* value = PQgetvalue(mResult, i, j);
        Cell* cell = row->mCells[j];
        cell->SetNull(PR_FALSE);
        PRInt32 type = cell->GetType();
        if (type == mozISqlResult::TYPE_STRING)
          cell->SetString(ToNewUnicode(NS_ConvertUTF8toUCS2(value)));
        else if (type == mozISqlResult::TYPE_INT)
          sscanf(value, "%d", &cell->mInt);
        else if (type == mozISqlResult::TYPE_FLOAT)
          sscanf(value, "%f", &cell->mFloat);
        else if (type == mozISqlResult::TYPE_DECIMAL)
          sscanf(value, "%f", &cell->mFloat);
        else if (type == mozISqlResult::TYPE_DATE ||
                 type == mozISqlResult::TYPE_TIME ||
                 type == mozISqlResult::TYPE_DATETIME)
          PR_ParseTimeString(value, PR_FALSE, &cell->mDate);
        else if (type == mozISqlResult::TYPE_BOOL)
          cell->mBool = !strcmp(value, "t");
      }
    }

    mRows.AppendElement(row);
    nsVoidKey key(resource);
    mSources.Put(&key, row);
  }

  return NS_OK;
}

void
mozSqlResultPgsql::ClearNativeResult()
{
  if (mResult) {
    PQclear(mResult);
    mResult = nsnull;
  }
  if (mTypes) {
    PQclear(mTypes);
    mTypes = nsnull;
  }
}

nsresult
mozSqlResultPgsql::EnsureTablePrivileges()
{
  nsresult rv = EnsureTableName();
  if (NS_FAILED(rv))
    return rv;

  NS_NAMED_LITERAL_STRING(select, "select ");
  NS_NAMED_LITERAL_STRING(func, "has_table_privilege(SESSION_USER, '");
  NS_NAMED_LITERAL_STRING(comma, ", ");
  NS_NAMED_LITERAL_STRING(ins, "', 'INSERT')");
  NS_NAMED_LITERAL_STRING(upd, "', 'UPDATE')");
  NS_NAMED_LITERAL_STRING(del, "','DELETE')");

  nsCOMPtr<mozISqlResult> result;
  rv = mConnection->ExecuteQuery(
    select + func + mTableName + ins +
    comma + func + mTableName + upd +
    comma + func + mTableName + del,
    getter_AddRefs(result));
  if (NS_FAILED(rv)) {
    mConnection->GetErrorMessage(mErrorMessage);
    return rv;
  }

  nsCOMPtr<mozISqlResultEnumerator> enumerator;
  rv = result->Enumerate(getter_AddRefs(enumerator));
  if (NS_FAILED(rv))
    return rv;

  rv = enumerator->First();
  if (NS_FAILED(rv))
    return rv;

  rv = enumerator->GetBool(0, &mCanInsert);
  if (NS_FAILED(rv))
    return rv;
  rv = enumerator->GetBool(1, &mCanUpdate);
  if (NS_FAILED(rv))
    return rv;
  return enumerator->GetBool(2, &mCanDelete);
}

nsresult
mozSqlResultPgsql::CanInsert(PRBool* _retval)
{
  if (mCanInsert >= 0) {
    *_retval = mCanInsert;
    return NS_OK;
  }

  nsresult rv = EnsureTablePrivileges();
  if (NS_FAILED(rv))
    return rv;
  *_retval = mCanInsert;

  return NS_OK;
}

nsresult
mozSqlResultPgsql::CanUpdate(PRBool* _retval)
{
  if (mCanUpdate >= 0) {
    *_retval = mCanUpdate;
    return NS_OK;
  }

  nsresult rv = EnsureTablePrivileges();
  if (NS_FAILED(rv))
    return rv;
  *_retval = mCanUpdate;

  return NS_OK;
}

nsresult
mozSqlResultPgsql::CanDelete(PRBool* _retval)
{
  if (mCanDelete >= 0) {
    *_retval = mCanDelete;
    return NS_OK;
  }

  nsresult rv = EnsureTablePrivileges();
  if (NS_FAILED(rv))
    return rv;
  *_retval = mCanDelete;

  return NS_OK;
}

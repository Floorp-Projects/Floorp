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

#include "prprf.h"
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
    PR_sscanf(value, "%d", &o);
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
    PRUnichar* name = UTF8ToNewUnicode(nsDependentCString(n));
    PRInt32 type = GetColType(i);
    PRInt32 size = PQfsize(mResult, i);
    PRInt32 mod = PQfmod(mResult, i);

    nsCAutoString uri(NS_LITERAL_CSTRING("http://www.mozilla.org/SQL-rdf#"));
    uri.Append(n);
    nsCOMPtr<nsIRDFResource> property;
    gRDFService->GetResource(uri, getter_AddRefs(property));

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
          cell->SetString(UTF8ToNewUnicode(nsDependentCString(value)));
        else if (type == mozISqlResult::TYPE_INT)
          PR_sscanf(value, "%d", &cell->mInt);
        else if (type == mozISqlResult::TYPE_FLOAT)
          PR_sscanf(value, "%f", &cell->mFloat);
        else if (type == mozISqlResult::TYPE_DECIMAL)
          PR_sscanf(value, "%f", &cell->mFloat);
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

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
 * The Initial Developer of the Original Code is mozilla.org
 * Portions created by the Initial Developer are Copyright (C) 2006
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
#include "mozSqlResultODBC.h"

mozSqlResultODBC::mozSqlResultODBC(mozISqlConnection* aConnection,
                                   const nsAString& aQuery)
  : mozSqlResult(aConnection, aQuery),
    mResult(nsnull)
{
}

void
mozSqlResultODBC::SetResult(SQLHSTMT aResult)
{
  mResult = aResult;
}

mozSqlResultODBC::~mozSqlResultODBC()
{
  ClearNativeResult();
}

NS_IMPL_ADDREF_INHERITED(mozSqlResultODBC, mozSqlResult)
NS_IMPL_RELEASE_INHERITED(mozSqlResultODBC, mozSqlResult)

// QueryInterface
NS_INTERFACE_MAP_BEGIN(mozSqlResultODBC)
  NS_INTERFACE_MAP_ENTRY(mozISqlResultODBC)
NS_INTERFACE_MAP_END_INHERITING(mozSqlResult)

PRInt32
mozSqlResultODBC::GetColType(PRInt32 aColumnIndex)
{
  switch (aColumnIndex){
    case SQL_INTEGER:
    case SQL_NUMERIC:
    case SQL_SMALLINT:
    case SQL_REAL:
      return mozISqlResult::TYPE_INT;

    case SQL_FLOAT:
    case SQL_DOUBLE:
      return mozISqlResult::TYPE_FLOAT;

    case SQL_DECIMAL:
      return mozISqlResult::TYPE_DECIMAL;

    case SQL_TYPE_DATE:
      return mozISqlResult::TYPE_DATE;

    case SQL_TYPE_TIME:
      return mozISqlResult::TYPE_TIME;

    case SQL_DATETIME:
      return mozISqlResult::TYPE_DATETIME;

    default:
      return mozISqlResult::TYPE_STRING;
  }
}

nsresult
mozSqlResultODBC::BuildColumnInfo()
{
  SQLSMALLINT count;
  SQLNumResultCols(mResult,&count);

  for (PRInt32 i = 0, l = count; i < l; i++) {

    SQLCHAR     colname[512];
    SQLSMALLINT coltype;

    SQLDescribeCol(mResult, i + 1, colname, sizeof(colname), NULL, &coltype, NULL, NULL, NULL);

    char* n = (char*) colname;

    PRUnichar* name = ToNewUnicode(nsDependentCString(n));
    PRInt32 type = GetColType(coltype);

    nsCAutoString uri(NS_LITERAL_CSTRING("http://www.mozilla.org/SQL-rdf#"));
    uri.Append(n);

    nsCOMPtr<nsIRDFResource> property;
    gRDFService->GetResource(uri, getter_AddRefs(property));

    ColumnInfo* columnInfo = ColumnInfo::Create(mAllocator, name, type,
                                                (PRInt32) 0-1, (PRInt32) 0-1,
                                                (PRInt32) 0-1, property);
    mColumnInfo.AppendElement(columnInfo);

  }

  return NS_OK;
}

nsresult
mozSqlResultODBC::BuildRows()
{
  while (SQL_SUCCEEDED(SQLFetch(mResult))) {
    nsCOMPtr<nsIRDFResource> resource;
    nsresult rv = gRDFService->GetAnonymousResource(getter_AddRefs(resource));
    if (NS_FAILED(rv)) return rv;

    Row* row = Row::Create(mAllocator, resource, mColumnInfo);

    for (PRInt32 j = 0; j < mColumnInfo.Count(); j++) {
      SQLINTEGER indicator;
      SQLCHAR buf[512];
      if(SQL_SUCCEEDED(SQLGetData(mResult, j+1, SQL_C_TCHAR, buf, sizeof(buf), &indicator)) && indicator != SQL_NULL_DATA) {
        char* value = (char*)buf;
        Cell* cell = row->mCells[j];
        cell->SetNull(PR_FALSE);
        PRInt32 type = cell->GetType();
        if (type == mozISqlResult::TYPE_STRING)
          cell->SetString(ToNewUnicode(nsDependentCString(value)));
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
mozSqlResultODBC::ClearNativeResult()
{
  if (mResult) {
    SQLFreeHandle(SQL_HANDLE_STMT, mResult);
    mResult = nsnull;
  }
}

nsresult
mozSqlResultODBC::CanInsert(PRBool* _retval)
{
  return PR_TRUE;
}

nsresult
mozSqlResultODBC::CanUpdate(PRBool* _retval)
{
  return PR_TRUE;
}

nsresult
mozSqlResultODBC::CanDelete(PRBool* _retval)
{
  return PR_TRUE;
}

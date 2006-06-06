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

#include "nsReadableUtils.h"
#include "mozSqlConnectionODBC.h"
#include "mozSqlResultODBC.h"

mozSqlConnectionODBC::mozSqlConnectionODBC()
  : mConnection(nsnull)
{
}

mozSqlConnectionODBC::~mozSqlConnectionODBC()
{
  if (mConnection) {
    SQLDisconnect(mConnection);
    SQLFreeHandle(SQL_HANDLE_DBC, mConnection);
    SQLFreeHandle(SQL_HANDLE_ENV, mEnv);
  }
}

NS_IMPL_ADDREF_INHERITED(mozSqlConnectionODBC, mozSqlConnection)
NS_IMPL_RELEASE_INHERITED(mozSqlConnectionODBC, mozSqlConnection)

// QueryInterface
NS_INTERFACE_MAP_BEGIN(mozSqlConnectionODBC)
  NS_INTERFACE_MAP_ENTRY(mozISqlConnectionODBC)
NS_INTERFACE_MAP_END_INHERITING(mozSqlConnection)

NS_IMETHODIMP
mozSqlConnectionODBC::Init(const nsAString &aHost, PRInt32 aPort,
                           const nsAString &aDatabase, const nsAString &aUsername,
                           const nsAString &aPassword)
{
  if (mConnection)
    return NS_OK;

  SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &mEnv);
  SQLSetEnvAttr(mEnv, SQL_ATTR_ODBC_VERSION, (void *) SQL_OV_ODBC3, 0);
  SQLAllocHandle(SQL_HANDLE_DBC, mEnv, &mConnection);

  NS_LossyConvertUTF16toASCII inString(NS_LITERAL_STRING("DSN=") + aDatabase +
                                       NS_LITERAL_STRING(";UID=") + aUsername +
                                       NS_LITERAL_STRING(";PWD=") + aPassword);
  char outString[1024];
  SQLSMALLINT outStringLength;
  if (!SQL_SUCCEEDED(SQLDriverConnect(mConnection, NULL,
                                      (SQLCHAR*)inString.get(),
                                      inString.Length(),
                                      (SQLCHAR*)outString, sizeof(outString),
                                      &outStringLength,
                                      SQL_DRIVER_COMPLETE))) {
    SetError(mConnection, SQL_HANDLE_DBC);

    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
mozSqlConnectionODBC::GetPrimaryKeys(const nsAString& aSchema,
                                     const nsAString& aTable,
                                     mozISqlResult** _retval)
{
  //SQLPrimaryKeys isn't implemented in most drivers

  SQLHSTMT hstmt;
  SQLAllocHandle(SQL_HANDLE_STMT, mConnection, &hstmt);

  if(!SQL_SUCCEEDED(SQLSpecialColumns(hstmt, SQL_BEST_ROWID, NULL, 0,
                    (SQLCHAR*) NULL, SQL_NTS, 
                    (SQLCHAR*) NS_LossyConvertUTF16toASCII(aTable).get(), SQL_NTS, 0, 0))){
    SetError(hstmt, SQL_HANDLE_STMT);

    return NS_ERROR_FAILURE;
  }


  nsAutoString select;
  PRInt32 i = 0;
  select.AssignLiteral("");
  
  while (SQL_SUCCEEDED(SQLFetch(hstmt))) {
    i++;
    if(i > 1) select.AppendLiteral(" UNION ");
    select.AppendLiteral("select NULL as TABLE_SCHEM, '");
    select.Append(aTable);
    select.AppendLiteral("' as TABLE_NAME, '");

    SQLCHAR colname[512];
    SQLGetData(hstmt, 2, SQL_C_CHAR, colname, sizeof(colname), NULL);
    select.AppendASCII((char*)colname);

    select.AppendLiteral("' as COLUMN_NAME, ");

    char str[16];
    sprintf(str, "%d", i);
    select.AppendLiteral(str);

    select.AppendLiteral(" as KEY_SEQ, NULL as PK_NAME");
  }

  if(select.IsEmpty()) {
   select.AppendLiteral("SELECT * FROM ");
   select.Append(aTable);
   select.AppendLiteral(" WHERE 1 = 2");
  }

  select.AppendLiteral(";");

  return RealExec(select, _retval, nsnull);
}

nsresult
mozSqlConnectionODBC::RealExec(const nsAString& aQuery,
                               mozISqlResult** aResult, PRInt32* aAffectedRows)
{
  if (!mConnection)
    return NS_ERROR_NOT_INITIALIZED;

  SQLHSTMT hstmt;
  SQLAllocHandle(SQL_HANDLE_STMT, mConnection, &hstmt);

  if(!SQL_SUCCEEDED(SQLExecDirect(hstmt, (SQLCHAR*) NS_LossyConvertUTF16toASCII(aQuery).get(), SQL_NTS))){
    SetError(hstmt, SQL_HANDLE_STMT);

    return NS_ERROR_FAILURE;
  }

  if (!aResult){
    if (!aAffectedRows)
      return NS_ERROR_NULL_POINTER;

    SQLINTEGER count;
    SQLRowCount(hstmt, &count);
    *aAffectedRows = (PRInt32) count;

    return NS_OK;
  }

  mozSqlResult *mozresult = new mozSqlResultODBC(this, aQuery);
  if (!mozresult){
    SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  ((mozSqlResultODBC*)mozresult)->SetResult(hstmt);
  nsresult rv = mozresult->Init();
  if (NS_FAILED(rv)){
    delete mozresult;
    return rv;
  }

  *aResult = mozresult;
  NS_ADDREF(*aResult);

  return NS_OK;
}

nsresult
mozSqlConnectionODBC::CancelExec()
{
  // SQLCancel needs the SQLHSTMT, other ways?
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
mozSqlConnectionODBC::GetIDName(nsAString& aIDName)
{
  // not sure what to return here
  aIDName.AssignLiteral("");

  return NS_OK;
}

void
mozSqlConnectionODBC::SetError(SQLHANDLE aHandle, SQLSMALLINT aType)
{
  SQLCHAR state[6], text[SQL_MAX_MESSAGE_LENGTH];
  SQLINTEGER native;

  char err[256];

  SQLINTEGER i = 1;

  mErrorMessage.Assign(NS_LITERAL_STRING(""));

  while (SQL_SUCCEEDED(SQLGetDiagRec(aType, aHandle, i++, state, &native, text, sizeof(text), NULL))) {
    sprintf((char *)err,"%s\nState: %s; Native Error: %lX\n", text, state, native);

    mErrorMessage.Append(ToNewUnicode(nsDependentCString((char *)err)));
  }
}

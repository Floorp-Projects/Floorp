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
 * The Initial Developer of the Original Code is Neil Deakin
 * Portions created by the Initial Developer are Copyright (C) 2004
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
#include "mozSqlConnectionMysql.h"
#include "mozSqlResultMysql.h"

mozSqlConnectionMysql::mozSqlConnectionMysql()
  : mConnection(nsnull)
{
}

mozSqlConnectionMysql::~mozSqlConnectionMysql()
{
  if (mConnection)
    mysql_close(mConnection);
}

NS_IMPL_ADDREF_INHERITED(mozSqlConnectionMysql, mozSqlConnection)
NS_IMPL_RELEASE_INHERITED(mozSqlConnectionMysql, mozSqlConnection)

// QueryInterface
NS_INTERFACE_MAP_BEGIN(mozSqlConnectionMysql)
  NS_INTERFACE_MAP_ENTRY(mozISqlConnectionMysql)
NS_INTERFACE_MAP_END_INHERITING(mozSqlConnection)

NS_IMETHODIMP
mozSqlConnectionMysql::Init(const nsAString &aHost, PRInt32 aPort,
                            const nsAString &aDatabase, const nsAString &aUsername,
                            const nsAString &aPassword)
{
  if (mConnection)
    return NS_OK;

  if (aPort == -1)
    aPort = 0;

  mConnection = mysql_init((MYSQL *) nsnull);
  if (!mConnection){
    return NS_ERROR_FAILURE;
  }

  if (!mysql_real_connect(mConnection, 
                          NS_ConvertUTF16toUTF8(aHost).get(),
                          NS_ConvertUTF16toUTF8(aUsername).get(),
                          NS_ConvertUTF16toUTF8(aPassword).get(),
                          NS_ConvertUTF16toUTF8(aDatabase).get(),
                          aPort, nsnull, 0)){
    mErrorMessage.Assign(NS_ConvertUTF8toUTF16(mysql_error(mConnection)));
    mConnection = nsnull;

    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
mozSqlConnectionMysql::GetServerVersion(nsAString &aServerVersion)
{
  if (!mConnection)
    return NS_ERROR_NOT_INITIALIZED;

  if (mServerVersion.IsEmpty()){
    mServerVersion.Assign(NS_ConvertUTF8toUTF16(mysql_get_server_info(mConnection)));
  }
  aServerVersion.Assign(mServerVersion);

  return NS_OK;
}

NS_IMETHODIMP
mozSqlConnectionMysql::GetPrimaryKeys(const nsAString& aSchema,
                                      const nsAString& aTable,
                                      mozISqlResult** aResult)
{
  // XXX this can be done with 'show columns from <table>', but it can't be
  //     filtered just for primary keys, which are listed in column 3

  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
mozSqlConnectionMysql::RealExec(const nsAString& aQuery,
                                mozISqlResult** aResult, PRInt32* aAffectedRows)
{
  if (!mConnection)
    return NS_ERROR_NOT_INITIALIZED;

  if (mysql_query(mConnection, NS_ConvertUTF16toUTF8(aQuery).get())){
    mErrorMessage.Assign(NS_ConvertUTF8toUTF16(mysql_error(mConnection)));

    return NS_ERROR_FAILURE;
  }

  if (!aResult){
    if (!aAffectedRows)
      return NS_ERROR_NULL_POINTER;

    my_ulonglong numrows = (PRInt32)mysql_affected_rows(mConnection);
    *aAffectedRows = ((numrows == (my_ulonglong)-1) ? 0 : (PRInt32)numrows);

    return NS_OK;
  }
  
  *aResult = nsnull;

  MYSQL_RES *rowresults = mysql_store_result(mConnection);
  if (!rowresults){
    mErrorMessage.Assign(NS_ConvertUTF8toUTF16(mysql_error(mConnection)));

    return NS_ERROR_FAILURE;
  }

  mozSqlResult *mozresult = new mozSqlResultMysql(this, aQuery);
  if (!mozresult){
    mysql_free_result(rowresults);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  ((mozSqlResultMysql*)mozresult)->SetResult(rowresults);
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
mozSqlConnectionMysql::CancelExec()
{
  unsigned long id = mysql_thread_id(mConnection);
  mysql_kill(mConnection, id);

  if (mysql_errno(mConnection)) {
    mErrorMessage.Assign(NS_ConvertUTF8toUTF16(mysql_error(mConnection)));
    return NS_ERROR_FAILURE;
  }

  mysql_ping(mConnection);

  return NS_OK;
}

nsresult
mozSqlConnectionMysql::GetIDName(nsAString& aIDName)
{
  aIDName.Truncate();

  return NS_OK;
}

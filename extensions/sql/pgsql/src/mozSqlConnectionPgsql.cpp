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
#include "mozSqlConnectionPgsql.h"
#include "mozSqlResultPgsql.h"

mozSqlConnectionPgsql::mozSqlConnectionPgsql()
  : mConnection(nsnull)
{
}

mozSqlConnectionPgsql::~mozSqlConnectionPgsql()
{
  if (mConnection)
    PQfinish(mConnection);
}

NS_IMPL_ADDREF_INHERITED(mozSqlConnectionPgsql, mozSqlConnection)
NS_IMPL_RELEASE_INHERITED(mozSqlConnectionPgsql, mozSqlConnection)

// QueryInterface
NS_INTERFACE_MAP_BEGIN(mozSqlConnectionPgsql)
  NS_INTERFACE_MAP_ENTRY(mozISqlConnectionPgsql)
NS_INTERFACE_MAP_END_INHERITING(mozSqlConnection)

NS_IMETHODIMP
mozSqlConnectionPgsql::Init(const nsAString & aHost, PRInt32 aPort,
                            const nsAString & aDatabase, const nsAString & aUsername,
                            const nsAString & aPassword)
{
  if (mConnection)
    return NS_OK;

  if (aPort == -1)
    aPort = 5432;
  char port[11];
  char options[] = "";
  char tty[] = "";
  PR_snprintf(port, 11, "%d", aPort);

  mConnection = PQsetdbLogin(NS_ConvertUCS2toUTF8(aHost).get(),
                             port, options, tty,
                             NS_ConvertUCS2toUTF8(aDatabase).get(),
                             NS_ConvertUCS2toUTF8(aUsername).get(),
                             NS_ConvertUCS2toUTF8(aPassword).get());

  return Setup();
}

NS_IMETHODIMP
mozSqlConnectionPgsql::GetPrimaryKeys(const nsAString& aSchema, const nsAString& aTable, mozISqlResult** _retval)
{
  nsAutoString select;
  nsAutoString from;
  nsAutoString where;
  if (mVersion >= SERVER_VERSION(7,3,0)) {
    select.AssignLiteral("SELECT n.nspname AS TABLE_SCHEM, ");
    from.AssignLiteral(" FROM pg_catalog.pg_namespace n, pg_catalog.pg_class ct, pg_catalog.pg_class ci, pg_catalog.pg_attribute a, pg_catalog.pg_index i");
    where.AssignLiteral(" AND ct.relnamespace = n.oid ");
    if (!aSchema.IsEmpty()) {
      where.Append(NS_LITERAL_STRING(" AND n.nspname = '") + aSchema);
      where.Append(PRUnichar('\''));
    }
  }
  else {
    select.AssignLiteral("SELECT NULL AS TABLE_SCHEM, ");
    from.AssignLiteral(" FROM pg_class ct, pg_class ci, pg_attribute a, pg_index i ");
  }

  if (!aTable.IsEmpty()) {
    where.Append(NS_LITERAL_STRING(" AND ct.relname = '") + aTable);
    where.Append(PRUnichar('\''));
  }

  NS_NAMED_LITERAL_STRING(select2, " ct.relname AS TABLE_NAME, a.attname AS COLUMN_NAME, a.attnum AS KEY_SEQ, ci.relname AS PK_NAME ");
  NS_NAMED_LITERAL_STRING(where2, " WHERE ct.oid=i.indrelid AND ci.oid=i.indexrelid AND a.attrelid=ci.oid AND i.indisprimary ");
  NS_NAMED_LITERAL_STRING(order2, " ORDER BY table_name, pk_name, key_seq");

  return RealExec(select + select2 + from + where2 + where + order2, _retval, nsnull);
}

nsresult
mozSqlConnectionPgsql::Setup()
{
  if (PQstatus(mConnection) == CONNECTION_BAD) {
    CopyUTF8toUTF16(PQerrorMessage(mConnection), mErrorMessage);
    mConnection = nsnull;
    return NS_ERROR_FAILURE;
  }

  PQsetClientEncoding(mConnection, "UNICODE");

/*
  PGresult* result = PQexec(mConnection, "SET DATESTYLE TO US");
  PRInt32 stat = PQresultStatus(result);
  if (stat != PGRES_COMMAND_OK) {
    mErrorMessage.Assign(NS_ConvertUTF8toUCS2(PQresultErrorMessage(result)));
    CopyUTF8toUTF16(PQresultErrorMessage(result), mErrorMessage);
    PQfinish(mConnection);
    mConnection = nsnull;
    return NS_ERROR_FAILURE;
  }
*/

  PGresult* result = PQexec(mConnection, "select version()");
  PRInt32 stat = PQresultStatus(result);
  if (stat != PGRES_TUPLES_OK) {
    CopyUTF8toUTF16(PQresultErrorMessage(result), mErrorMessage);
    return NS_ERROR_FAILURE;
  }
  char* version = PQgetvalue(result, 0, 0);
  NS_ConvertUTF8toUCS2 buffer(version);
  nsAString::const_iterator start, end, iter;
  buffer.BeginReading(iter);
  buffer.EndReading(end);
  while (iter != end && !nsCRT::IsAsciiSpace(*iter))
    ++iter;
  while (iter != end && nsCRT::IsAsciiSpace(*iter))
    ++iter;
  start = iter;
  while (iter != end && !nsCRT::IsAsciiSpace(*iter))
    ++iter;
  mServerVersion = Substring(start,iter);

  PRInt32 numbers[3] = {0,0,0};
  mServerVersion.BeginReading(iter);
  mServerVersion.EndReading(end);
  for (PRInt32 i = 0; i < 3; i++) {
    start = iter;
    while (iter != end && *iter != PRUnichar('.'))
      ++iter;
    nsAutoString v(Substring(start,iter));
    PRInt32 err;
    numbers[i] = v.ToInteger(&err);
    while (iter != end && *iter == PRUnichar('.'))
      ++iter;
  }
  mVersion = SERVER_VERSION(numbers[0], numbers[1], numbers[2]);

  return NS_OK;
}

nsresult
mozSqlConnectionPgsql::RealExec(const nsAString& aQuery,
                                mozISqlResult** aResult, PRInt32* aAffectedRows)
{
  if (! mConnection)
    return NS_ERROR_NOT_INITIALIZED;

  PGresult* r;
  r = PQexec(mConnection, NS_ConvertUCS2toUTF8(aQuery).get());
  PRInt32 stat = PQresultStatus(r);

  if (PQstatus(mConnection) == CONNECTION_BAD) {
    PQreset(mConnection);
    nsresult rv = Setup();
    if (NS_FAILED(rv))
      return rv;
    r = PQexec(mConnection, NS_ConvertUCS2toUTF8(aQuery).get());
    stat = PQresultStatus(r);
  }

  if (stat == PGRES_TUPLES_OK) {
    if (!aResult)
      return NS_ERROR_NULL_POINTER;

    static char select1[] = "select t.oid, case when t.typbasetype = 0 then t.typname else (select t2.typname from pg_type t2 where t2.oid=t.typbasetype) end as typname from pg_type t where t.oid in (";
    static char select2[] = "select oid, typname from pg_type where oid in (";
    char* select;
    if (mVersion >= SERVER_VERSION(7,3,0))
      select = select1;
    else
      select = select2;
    PRInt32 columnCount = PQnfields(r);
    char* query = (char*)malloc(strlen(select) + columnCount * 11 + 2);
    strcpy(query, select);
    for (PRInt32 i = 0; i < columnCount; i++) {
      PRInt32 oid = PQftype(r, i);
      char oidStr[11];
      if (i)
        sprintf(oidStr, ",%d", oid);
      else
        sprintf(oidStr, "%d", oid);
      strcat(query, oidStr);
    }
    strcat(query, ")");
    PGresult* types = PQexec(mConnection, query);
    free(query);
    stat = PQresultStatus(types);
    if (stat != PGRES_TUPLES_OK) {
      CopyUTF8toUTF16(PQresultErrorMessage(types), mErrorMessage);
      return NS_ERROR_FAILURE;
    }

    if (*aResult) {
      ((mozSqlResultPgsql*)*aResult)->SetResult(r, types);
      nsresult rv = ((mozSqlResult*)*aResult)->Rebuild();
      if (NS_FAILED(rv))
        return rv;
      NS_ADDREF(*aResult);
    }
    else {
      mozSqlResult* result = new mozSqlResultPgsql(this, aQuery);
      if (! result)
        return NS_ERROR_OUT_OF_MEMORY;
      ((mozSqlResultPgsql*)result)->SetResult(r, types);
      nsresult rv = result->Init();
      if (NS_FAILED(rv))
        return rv;
      NS_ADDREF(*aResult = result);
    }
  }
  else if (stat == PGRES_COMMAND_OK) {
    if (!aAffectedRows)
      return NS_ERROR_NULL_POINTER;
    PR_sscanf(PQcmdTuples(r), "%d", aAffectedRows);
    mLastID = PQoidValue(r);
  }
  else {
    CopyUTF8toUTF16(PQresultErrorMessage(r), mErrorMessage);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
mozSqlConnectionPgsql::CancelExec()
{
  if (!PQrequestCancel(mConnection)) {
    CopyUTF8toUTF16(PQerrorMessage(mConnection), mErrorMessage);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
mozSqlConnectionPgsql::GetIDName(nsAString& aIDName)
{
  aIDName.AssignLiteral("OID");
  return NS_OK;
}

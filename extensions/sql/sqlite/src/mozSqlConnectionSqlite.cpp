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
 *   Valia Vaneeva <fattie@altlinux.ru>
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
#include "mozSqlConnectionSqlite.h"
#include "mozSqlResultSqlite.h"
#include <ctype.h>
#include "nsLocalFile.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsUnicharUtils.h"

mozSqlConnectionSqlite::mozSqlConnectionSqlite()
  : mConnection(nsnull)
{
}

mozSqlConnectionSqlite::~mozSqlConnectionSqlite()
{
  if (mConnection)
    sqlite3_close(mConnection);
}

NS_IMPL_ADDREF_INHERITED(mozSqlConnectionSqlite, mozSqlConnection)
NS_IMPL_RELEASE_INHERITED(mozSqlConnectionSqlite, mozSqlConnection)

// QueryInterface
NS_INTERFACE_MAP_BEGIN(mozSqlConnectionSqlite)
  NS_INTERFACE_MAP_ENTRY(mozISqlConnectionSqlite)
NS_INTERFACE_MAP_END_INHERITING(mozSqlConnection)

NS_IMETHODIMP
mozSqlConnectionSqlite::Init(const nsAString& aHost,
                             PRInt32 aPort,
                             const nsAString& aDatabase,
                             const nsAString& aUsername,
                             const nsAString& aPassword)
{
  if (mConnection)
    return NS_OK;

  nsAString::const_iterator start;
  nsresult rv;
  nsAutoString path;
  aDatabase.BeginReading(start);

  nsCOMPtr<nsILocalFile> file(do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;
    
  rv = file->InitWithPath(aDatabase);

  if (rv == NS_ERROR_FILE_UNRECOGNIZED_PATH) {
    nsCOMPtr<nsIProperties> directoryService = do_GetService(
                                               NS_DIRECTORY_SERVICE_CONTRACTID,
                                               &rv);
    if (NS_FAILED(rv))
      return rv;
    rv = directoryService->Get(NS_APP_USER_PROFILE_50_DIR,
                               NS_GET_IID(nsILocalFile),
                               getter_AddRefs(file));
    if (NS_FAILED(rv))
      return rv;
    rv = file->Append(aDatabase);
    if (NS_FAILED(rv))
      return rv;
    file->GetPath(path);
    if (path.IsEmpty())
      return NS_ERROR_FAILURE;
  }
  else if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;
  else
    path = aDatabase;

  PRBool exists;
  rv = file->Exists(&exists);
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;
  
  rv = file->IsWritable(&mWritable);
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;
  	
  rv = sqlite3_open(NS_ConvertUCS2toUTF8(path).get(), &mConnection);
  if (rv != SQLITE_OK)
    return rv;

  return Setup();
}

NS_IMETHODIMP
mozSqlConnectionSqlite::GetPrimaryKeys(const nsAString& aSchema,
                                       const nsAString& aTable,
                                       mozISqlResult** _retval)
{
  if (! mConnection)
    return NS_ERROR_NOT_INITIALIZED;
    
  /* the purpose of this all is to get primary keys in a structure used in
   * GetPrimaryKeys() for pgsql. as sqlite3 doesn't allow to do it using a
   * single select query, we have to create it by hand
   * i thought of other variants but this one seems to be the easiest, if you
   * know how to implement GetPrimaryKeys() in a better way, please tell me
   */
  char **r, *errmsg;
  PRInt32 stat, nrow, ncolumn;
  nsAutoString preselect, select1, select2, uni, select, common;
  
  common.AssignLiteral("select NULL as TABLE_SCHEM, '");
  select1.AssignLiteral("' as COLUMN_NAME, ");
  select2.AssignLiteral(" as KEY_SEQ, NULL as PK_NAME");
  uni.AssignLiteral(" UNION ");
  
  // first select: we need it to get the "create table" statement
  preselect.AssignLiteral("select sql from sqlite_master where type='table' and name='");
  if (!aTable.IsEmpty()) {
    preselect.Append(aTable);
    common.Append(aTable);
  }
  else
    return NS_ERROR_FAILURE;
  preselect.Append(NS_LITERAL_STRING("';"));
  common.Append(NS_LITERAL_STRING("' as TABLE_NAME, '"));
  
  stat = sqlite3_get_table(mConnection, NS_ConvertUCS2toUTF8(preselect).get(),
                           &r, &nrow, &ncolumn, &errmsg);
  if (stat != SQLITE_OK) {
    CopyUTF8toUTF16(errmsg, mErrorMessage);
    sqlite3_free_table(r);
    return NS_ERROR_FAILURE;
  }
  
  // now we parse that statement in order to find primary key columns
  nsAutoString aToken = NS_ConvertUTF8toUCS2(nsDependentCString("PRIMARY KEY"));
  nsAutoString aKeyColumn;
  NS_ConvertUTF8toUCS2 buffer(r[1]);
  nsAString::const_iterator start, end, iter, iter2;
  buffer.BeginReading(start);
  buffer.EndReading(end);
  
  if (CaseInsensitiveFindInReadable(aToken, start, end))
    iter = end;
  else
    return NS_ERROR_FAILURE;
  buffer.BeginReading(start);
  buffer.EndReading(end);

  PRInt32 count = 1;
  while (iter != end && (*iter == PRUnichar(' ') || *iter == PRUnichar('\t') ||
         *iter == PRUnichar('\n')))
    ++iter;
  
  // if we have "primary key (colname1, colname2, ...)" we can have multiple
  // columns
  if (*iter == PRUnichar('(')) {
    char str[16];
    ++iter;
    while (iter != end && *iter != PRUnichar(')')) {
      while ((*iter == PRUnichar(' ') || *iter == PRUnichar('\n') ||
             *iter == PRUnichar('\t') || *iter == PRUnichar(',')) &&
             *iter != PRUnichar(')') && iter != end)
        ++iter;
      if (iter != end && *iter != PRUnichar(')')) {
      	// we get column names and create a fake select which selects nothing
      	if (count > 1)
          select.Append(uni);
        select.Append(common);
        iter2 = iter;
    	while (iter2 != end && *iter2 != PRUnichar(',') &&
               *iter2 != PRUnichar(' ') && *iter2 != PRUnichar('\n') &&
               *iter2 != PRUnichar('\t') && *iter2 != PRUnichar(')'))
      	  ++iter2;
    	aKeyColumn = Substring(iter,iter2);
    	select.Append(aKeyColumn);
    	iter = iter2;
      	select.Append(select1);
        PRInt32 i = 0, j, tmp, cnt = count;
        do {
          str[i++] = cnt % 10 + 48;
          str[i] = '\0';
        } while ((cnt /= 10) > 0);
        for (i = 0, j = strlen(str) - 1; i < j; i++, j--) {
	  tmp = str[i];
          str[i] = str[j];
          str[j] = tmp;
    	}
      	select.Append(UTF8ToNewUnicode(nsDependentCString(str)));
      	select.Append(select2);
      	count++;
      }
    }
  }
  // we have only one primary key column: "colname ... primary key ..."
  else {
    PRInt32 openParenth = 0;
    while (iter != start && (*iter != PRUnichar(',') && openParenth == 0)) {
      if (*iter == PRUnichar(')'))
        openParenth++;
      else if (*iter == PRUnichar('('))
        openParenth--;
      --iter;
    }
    if (iter == start) {
      while (*iter != PRUnichar('(') && iter != end)
        ++iter;
    }
    ++iter;
    while ((*iter == PRUnichar(' ') || *iter == PRUnichar('\n') ||
           *iter == PRUnichar('\t')) && iter != end)
      ++iter;
    select.Append(common);
    iter2 = iter;
    while (iter2 != end && *iter2 != PRUnichar(' ') &&
           *iter2 != PRUnichar('\n') && *iter2 != PRUnichar('\t'))
      ++iter2;
    aKeyColumn = Substring(iter,iter2);
    select.Append(aKeyColumn);
    select.Append(select1);
    select.Append(PRUnichar('1'));
    select.Append(select2);
  }
  select.Append(PRUnichar(';'));
  
  sqlite3_free_table(r);

  /* by this time we have either this select:
   *   select NULL as TABLE_SCHEM, 'table_name' as TABLE_NAME, 'colname1' as
   *   COLUMN_NAME, 1 as KEY_SEQ, NULL as PK_NAME UNION select NULL as
   *   TABLE_SCHEM, 'table_name' as TABLE_NAME, 'colname2' as COLUMN_NAME, 2 as
   *   KEY_SEQ, NULL as PK_NAME ...;
   * or this one:
   *   select NULL as TABLE_SCHEM, 'table_name' as TABLE_NAME, 'colname' as
   *   COLUMN_NAME, 1 as KEY_SEQ, NULL as PK_NAME;
   * anyway, they do not select anything, they just assign the values
   */
  return RealExec(select, _retval, nsnull);
}

nsresult
mozSqlConnectionSqlite::Setup()
{
  if (sqlite3_errcode(mConnection) != SQLITE_OK) {
    CopyUTF8toUTF16(sqlite3_errmsg(mConnection), mErrorMessage);
    
    mConnection = nsnull;
    return NS_ERROR_FAILURE;
  }

  NS_ConvertUTF8toUCS2 buffer(sqlite3_version);
  nsAString::const_iterator start, end, iter;
  PRInt32 numbers[3] = {0,0,0};
  buffer.BeginReading(iter);
  buffer.EndReading(end);
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

  if (mVersion < SERVER_VERSION(3,0,2))
    return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult
mozSqlConnectionSqlite::RealExec(const nsAString& aQuery,
                                 mozISqlResult** aResult,
                                 PRInt32* aAffectedRows)
{
  //sqlite3_changes doesn't reset its count to 0 after selects
  static PRInt32 oldChange = 0;
  
  if (! mConnection)
    return NS_ERROR_NOT_INITIALIZED;

  char **r, *errmsg;
  PRInt32 stat, nrow, ncolumn;

  stat = sqlite3_get_table(mConnection, NS_ConvertUCS2toUTF8(aQuery).get(), &r,
                           &nrow, &ncolumn, &errmsg);
  PRInt32 changed = sqlite3_total_changes(mConnection) - oldChange;
  oldChange += changed;

  if (stat == SQLITE_OK && !changed) {
    if (!aResult)
      return NS_ERROR_NULL_POINTER;
    
    if (*aResult) {
      ((mozSqlResultSqlite*)*aResult)->SetResult(r, nrow, ncolumn, mWritable);
      nsresult rv = ((mozSqlResult*)*aResult)->Rebuild();
      if (NS_FAILED(rv))
        return rv;
      NS_ADDREF(*aResult);
    }
    else {
      mozSqlResult* result = new mozSqlResultSqlite(this, aQuery);
      if (!result)
        return NS_ERROR_OUT_OF_MEMORY;
      ((mozSqlResultSqlite*)result)->SetResult(r, nrow, ncolumn, mWritable);
      nsresult rv = result->Init();
      if (NS_FAILED(rv))
        return rv;
      NS_ADDREF(*aResult = result);
    }
  }
  else if (stat == SQLITE_OK && changed) {
    if (!aAffectedRows)
      return NS_ERROR_NULL_POINTER;
    *aAffectedRows = changed;
    mLastID = sqlite3_last_insert_rowid(mConnection);
  }
  else {
    CopyUTF8toUTF16(errmsg, mErrorMessage);
    sqlite3_free_table(r);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
mozSqlConnectionSqlite::CancelExec()
{
  sqlite3_interrupt(mConnection);
  
  if (sqlite3_errcode(mConnection) != SQLITE_OK) {
    CopyUTF8toUTF16(sqlite3_errmsg(mConnection), mErrorMessage);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
mozSqlConnectionSqlite::GetIDName(nsAString& aIDName)
{
  aIDName.AssignLiteral("OID");
  return NS_OK;
}

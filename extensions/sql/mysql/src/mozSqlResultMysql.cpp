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

#include "prprf.h"
#include "nsReadableUtils.h"
#include "mozSqlResultMysql.h"

mozSqlResultMysql::mozSqlResultMysql(mozISqlConnection* aConnection,
                                     const nsAString& aQuery)
  : mozSqlResult(aConnection, aQuery),
    mResult(nsnull)
{
}

void
mozSqlResultMysql::SetResult(MYSQL_RES *aResult)
{
  if (mResult){
    mysql_free_result(mResult);
  }

  mResult = aResult;
}

mozSqlResultMysql::~mozSqlResultMysql()
{
  ClearNativeResult();
}

NS_IMPL_ADDREF_INHERITED(mozSqlResultMysql, mozSqlResult)
NS_IMPL_RELEASE_INHERITED(mozSqlResultMysql, mozSqlResult)

// QueryInterface
NS_INTERFACE_MAP_BEGIN(mozSqlResultMysql)
  NS_INTERFACE_MAP_ENTRY(mozISqlResultMysql)
NS_INTERFACE_MAP_END_INHERITING(mozSqlResult)

PRInt32
mozSqlResultMysql::GetColType(MYSQL_FIELD *aField)
{
  switch (aField->type){
    case FIELD_TYPE_TINY:
    case FIELD_TYPE_SHORT:
    case FIELD_TYPE_LONG:
    case FIELD_TYPE_INT24:
    case FIELD_TYPE_LONGLONG:
      return mozISqlResult::TYPE_INT;

    case FIELD_TYPE_DECIMAL:
      return mozISqlResult::TYPE_DECIMAL;

    case FIELD_TYPE_FLOAT:
    case FIELD_TYPE_DOUBLE:
      return mozISqlResult::TYPE_FLOAT;

    case FIELD_TYPE_DATE:
      return mozISqlResult::TYPE_DATE;

    case FIELD_TYPE_TIME:
      return mozISqlResult::TYPE_TIME;

    case FIELD_TYPE_DATETIME:
      return mozISqlResult::TYPE_DATETIME;

    default:
      // handles these types:
      //   FIELD_TYPE_TIMESTAMP, FIELD_TYPE_YEAR, FIELD_TYPE_STRING,
      //   FIELD_TYPE_VAR_STRING, FIELD_TYPE_BLOB, FIELD_TYPE_SET,
      //   FIELD_TYPE_ENUM, FIELD_TYPE_NULL, FIELD_TYPE_CHAR

      return mozISqlResult::TYPE_STRING;
  }
}

nsresult
mozSqlResultMysql::BuildColumnInfo()
{
  MYSQL_FIELD *field;

  while ((field = mysql_fetch_field(mResult))){
    PRUnichar* name = UTF8ToNewUnicode(nsDependentCString(field->name));
    PRInt32 type = GetColType(field);

    nsCAutoString uri(NS_LITERAL_CSTRING("http://www.mozilla.org/SQL-rdf#"));
    uri.Append(field->name);

    nsCOMPtr<nsIRDFResource> property;
    gRDFService->GetResource(uri, getter_AddRefs(property));

    ColumnInfo* columnInfo = ColumnInfo::Create(mAllocator, name, type,
                                                (PRInt32)field->length, (PRInt32)field->length,
                                                (field->flags & PRI_KEY_FLAG), property);
    mColumnInfo.AppendElement(columnInfo); 
  }

  return NS_OK;
}

nsresult
mozSqlResultMysql::BuildRows()
{
  MYSQL_ROW resultrow;
  while ((resultrow = mysql_fetch_row(mResult))){
    nsCOMPtr<nsIRDFResource> resource;
    nsresult rv = gRDFService->GetAnonymousResource(getter_AddRefs(resource));
    if (NS_FAILED(rv)) return rv;

    Row* row = Row::Create(mAllocator, resource, mColumnInfo);

    for (PRInt32 j = 0; j < mColumnInfo.Count(); j++) {
      char* value = resultrow[j];
      if (value){
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
mozSqlResultMysql::ClearNativeResult()
{
  if (mResult) {
    mysql_free_result(mResult);
    mResult = nsnull;
  }
}

nsresult
mozSqlResultMysql::EnsurePrimaryKeys()
{
  return NS_OK;
}

nsresult
mozSqlResultMysql::AppendKeys(Row* aRow, nsAutoString& aKeys)
{
  PRInt32 i;
  for (i = 0; i < mColumnInfo.Count(); i++) {
    if (((ColumnInfo*)mColumnInfo[i])->mIsPrimaryKey){
      if (i){
        aKeys.Append(NS_LITERAL_STRING(" AND "));
      }
      aKeys.Append(((ColumnInfo*)mColumnInfo[i])->mName);
      aKeys.Append(PRUnichar('='));

      Cell* cell = aRow->mCells[i];
      AppendValue(cell, aKeys);
    }
  }

  return NS_OK;
}

nsresult
mozSqlResultMysql::CanInsert(PRBool* _retval)
{
  return PR_TRUE;
}

nsresult
mozSqlResultMysql::CanUpdate(PRBool* _retval)
{
  return PR_TRUE;
}

nsresult
mozSqlResultMysql::CanDelete(PRBool* _retval)
{
  return PR_TRUE;
}

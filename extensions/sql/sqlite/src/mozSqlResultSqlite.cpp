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
#include "mozSqlResultSqlite.h"

mozSqlResultSqlite::mozSqlResultSqlite(mozISqlConnection* aConnection,
                                       const nsAString& aQuery)
  : mozSqlResult(aConnection, aQuery),
    mResult(nsnull),
    mColumnCount(0),
    mRowCount(0),
    mWritable(PR_FALSE)
{
}

void
mozSqlResultSqlite::SetResult(char** aResult, PRInt32 aRowCount,
                              PRInt32 aColumnCount, PRBool aWritable)
{
  mResult = aResult;
  mColumnCount = aColumnCount;
  mRowCount = aRowCount;
  mWritable = aWritable;
}

mozSqlResultSqlite::~mozSqlResultSqlite()
{
  ClearNativeResult();
}

NS_IMPL_ADDREF_INHERITED(mozSqlResultSqlite, mozSqlResult)
NS_IMPL_RELEASE_INHERITED(mozSqlResultSqlite, mozSqlResult)

// QueryInterface
NS_INTERFACE_MAP_BEGIN(mozSqlResultSqlite)
  NS_INTERFACE_MAP_ENTRY(mozISqlResultSqlite)
NS_INTERFACE_MAP_END_INHERITING(mozSqlResult)

PRInt32
mozSqlResultSqlite::GetColType(PRInt32 aColumnIndex)
{
  return mozISqlResult::TYPE_STRING;
}

nsresult
mozSqlResultSqlite::BuildColumnInfo()
{
  for (PRInt32 i = 0; i < mColumnCount; i++) {
    char* n = mResult[i];
    PRUnichar* name = UTF8ToNewUnicode(nsDependentCString(n));
    PRInt32 type = GetColType(i);
    PRInt32 size = -1;
    PRInt32 mod = -1;

    nsCAutoString uri(NS_LITERAL_CSTRING("http://www.mozilla.org/SQL-rdf#"));
    uri.Append(n);
    nsCOMPtr<nsIRDFResource> property;
    gRDFService->GetResource(uri, getter_AddRefs(property));

    ColumnInfo* columnInfo = ColumnInfo::Create(mAllocator, name, type, size,
                                                mod, property);
    mColumnInfo.AppendElement(columnInfo); 
  }

  return NS_OK;
}

nsresult
mozSqlResultSqlite::BuildRows()
{
  for(PRInt32 i = 1; i <= mRowCount; i++) {
    nsCOMPtr<nsIRDFResource> resource;
    nsresult rv = gRDFService->GetAnonymousResource(getter_AddRefs(resource));
    if (NS_FAILED(rv)) return rv;

    Row* row = Row::Create(mAllocator, resource, mColumnInfo);

    for (PRInt32 j = 0; j < mColumnInfo.Count(); j++) {
      if (mResult[j+i*mColumnCount]) {
        char* value = mResult[j+i*mColumnCount];
        Cell* cell = row->mCells[j];
        cell->SetNull(PR_FALSE);
        cell->SetString(UTF8ToNewUnicode(nsDependentCString(value)));
      }
    }

    mRows.AppendElement(row);
    nsVoidKey key(resource);
    mSources.Put(&key, row);
  }

  return NS_OK;
}

void
mozSqlResultSqlite::ClearNativeResult()
{
  if (mResult) {
    sqlite3_free_table(mResult);
    mResult = nsnull;
  }
}

nsresult
mozSqlResultSqlite::CanInsert(PRBool* _retval)
{
  *_retval = mWritable;

  return NS_OK;
}

nsresult
mozSqlResultSqlite::CanUpdate(PRBool* _retval)
{
  *_retval = mWritable;

  return NS_OK;
}

nsresult
mozSqlResultSqlite::CanDelete(PRBool* _retval)
{
  *_retval = mWritable;

  return NS_OK;
}

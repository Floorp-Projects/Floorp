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

#include <stdio.h>
#include "nsCRT.h"
#include "nsIVariant.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "rdf.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsDateTimeFormatCID.h"
#include "mozSqlResult.h"
#include "mozSqlConnection.h"
#include "nsITreeColumns.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kDateTimeFormatCID, NS_DATETIMEFORMAT_CID);

PRInt32                 mozSqlResult::gRefCnt = 0;
nsIRDFService*          mozSqlResult::gRDFService;
nsIDateTimeFormat*      mozSqlResult::gFormat;
nsIRDFResource*         mozSqlResult::kSQL_ResultRoot;
nsIRDFResource*         mozSqlResult::kNC_Child;
nsIRDFLiteral*          mozSqlResult::kNullLiteral;
nsIRDFLiteral*          mozSqlResult::kEmptyLiteral;
nsIRDFLiteral*          mozSqlResult::kTrueLiteral;
nsIRDFLiteral*          mozSqlResult::kFalseLiteral;

mozSqlResult::mozSqlResult(mozISqlConnection* aConnection,
                         const nsAString& aQuery)
  : mDisplayNullAsText(PR_FALSE),
    mConnection(aConnection),
    mQuery(aQuery),
    mSources(nsnull, nsnull, nsnull, nsnull),
    mCanInsert(-1),
    mCanUpdate(-1),
    mCanDelete(-1)
{
}

nsresult
mozSqlResult::Init()
{
  nsresult rv;

  if (gRefCnt++ == 0) {
    rv = nsServiceManager::GetService(kRDFServiceCID, NS_GET_IID(nsIRDFService),
                                      (nsISupports**) &gRDFService);
    if (NS_FAILED(rv)) return rv;

    rv = nsComponentManager::CreateInstance(kDateTimeFormatCID,
                                            nsnull,
                                            NS_GET_IID(nsIDateTimeFormat),
                                            (void**) &gFormat);
    if (NS_FAILED(rv)) return rv;

    rv = gRDFService->GetResource(NS_LITERAL_CSTRING("SQL:ResultRoot"),
                                  &kSQL_ResultRoot);
    if (NS_FAILED(rv)) return rv;
    rv = gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "child"),
                                  &kNC_Child);
    if (NS_FAILED(rv)) return rv;

    rv = gRDFService->GetLiteral(NS_LITERAL_STRING("null").get(), &kNullLiteral);
    if (NS_FAILED(rv)) return rv;
    rv = gRDFService->GetLiteral(NS_LITERAL_STRING("").get(), &kEmptyLiteral);
    if (NS_FAILED(rv)) return rv;
    rv = gRDFService->GetLiteral(NS_LITERAL_STRING("true").get(), &kTrueLiteral);
    if (NS_FAILED(rv)) return rv;
    rv = gRDFService->GetLiteral(NS_LITERAL_STRING("false").get(), &kFalseLiteral);
    if (NS_FAILED(rv)) return rv;
  }

  static const size_t kBucketSizes[] = {
    sizeof(ColumnInfo),
    sizeof(Cell),
    sizeof(Row)
  };
  static const PRInt32 kNumBuckets = sizeof(kBucketSizes) / sizeof(size_t);
  static const PRInt32 kInitialSize = 16;

  mAllocator.Init("mozSqlResult", kBucketSizes, kNumBuckets, kInitialSize);

  return Rebuild();
}

nsresult
mozSqlResult::Rebuild()
{
  ClearRows();
  ClearColumnInfo();

  nsresult rv = BuildColumnInfo();
  if (NS_FAILED(rv)) return rv;

  rv = BuildRows();
  if (NS_FAILED(rv)) return rv;

  ClearNativeResult();

  return NS_OK;
}

mozSqlResult::~mozSqlResult()
{
  ClearRows();
  ClearColumnInfo();

  if (--gRefCnt == 0) {
    NS_IF_RELEASE(kFalseLiteral);
    NS_IF_RELEASE(kTrueLiteral);
    NS_IF_RELEASE(kEmptyLiteral);
    NS_IF_RELEASE(kNullLiteral);
    NS_IF_RELEASE(kNC_Child);
    NS_IF_RELEASE(kSQL_ResultRoot);

    NS_IF_RELEASE(gFormat);
    nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
    gRDFService = nsnull;
  }
}


NS_IMPL_THREADSAFE_ISUPPORTS5(mozSqlResult,
                              mozISqlResult,
                              mozISqlDataSource,
                              nsIRDFDataSource,
                              nsIRDFRemoteDataSource,
                              nsITreeView)

NS_IMETHODIMP
mozSqlResult::GetDisplayNullAsText(PRBool* aDisplayNullAsText)
{
  *aDisplayNullAsText = mDisplayNullAsText;
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::SetDisplayNullAsText(PRBool aDisplayNullAsText)
{
  mDisplayNullAsText = aDisplayNullAsText;
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::GetConnection(mozISqlConnection** aConnection)
{
  NS_ADDREF(*aConnection = mConnection);
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::GetQuery(nsAString& aQuery)
{
  aQuery = mQuery;
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::GetTableName(nsAString& aTableName)
{
  nsresult rv = EnsureTableName();
  if (NS_FAILED(rv))
    return rv;
  aTableName = mTableName;
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::GetRowCount(PRInt32 *aRowCount)
{
  *aRowCount = mRows.Count();
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::GetColumnCount(PRInt32 *aColumnCount)
{
  *aColumnCount = mColumnInfo.Count();
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::GetColumnName(PRInt32 aColumnIndex, nsAString& _retval)
{
  _retval.Assign(((ColumnInfo*)mColumnInfo[aColumnIndex])->mName);
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::GetColumnIndex(const nsAString & aColumnName, PRInt32 *_retval)
{
  *_retval = -1;

  for (PRInt32 i = 0; i < mColumnInfo.Count(); i++) {
    PRUnichar* name = ((ColumnInfo*)mColumnInfo[i])->mName;
    if (aColumnName.Equals(name)) {
      *_retval = i;
      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::GetColumnType(PRInt32 aColumnIndex, PRInt32* _retval)
{
  if (aColumnIndex < 0 || aColumnIndex >= mColumnInfo.Count())
    return NS_ERROR_INVALID_ARG;

  *_retval = ((ColumnInfo*)mColumnInfo[aColumnIndex])->mType;

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::GetColumnTypeAsString(PRInt32 aColumnIndex, nsAString& _retval)
{
  if (aColumnIndex < 0 || aColumnIndex >= mColumnInfo.Count())
    return NS_ERROR_INVALID_ARG;

  PRInt32 type = ((ColumnInfo*)mColumnInfo[aColumnIndex])->mType;
  switch (type) {
    case mozISqlResult::TYPE_STRING:
      _retval.Assign(NS_LITERAL_STRING("string"));
      break;
    case mozISqlResult::TYPE_INT:
      _retval.Assign(NS_LITERAL_STRING("int"));
      break;
    case mozISqlResult::TYPE_FLOAT:
      _retval.Assign(NS_LITERAL_STRING("float"));
      break;
    case mozISqlResult::TYPE_DECIMAL:
      _retval.Assign(NS_LITERAL_STRING("decimal"));
      break;
    case mozISqlResult::TYPE_DATE:
      _retval.Assign(NS_LITERAL_STRING("date"));
      break;
    case mozISqlResult::TYPE_TIME:
      _retval.Assign(NS_LITERAL_STRING("time"));
      break;
    case mozISqlResult::TYPE_DATETIME:
      _retval.Assign(NS_LITERAL_STRING("datetime"));
      break;
    case mozISqlResult::TYPE_BOOL:
      _retval.Assign(NS_LITERAL_STRING("bool"));
      break;
  }

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::GetColumnDisplaySize(PRInt32 aColumnIndex, PRInt32* _retval)
{
  if (aColumnIndex < 0 || aColumnIndex >= mColumnInfo.Count())
    return NS_ERROR_INVALID_ARG;

  ColumnInfo* columnInfo = ((ColumnInfo*)mColumnInfo[aColumnIndex]);
  PRInt32 mod = columnInfo->mMod - 4;

  switch (columnInfo->mType) {
    case mozISqlResult::TYPE_STRING:
      *_retval = mod;
      break;
    case mozISqlResult::TYPE_INT:
      *_retval = 11; // -2147483648 to +2147483647
      break;
    case mozISqlResult::TYPE_FLOAT:
      *_retval = 11;
      break;
    case mozISqlResult::TYPE_DECIMAL:
      *_retval = ((mod >> 16) & 0xffff) + 1 + (mod & 0xffff);
      break;
    case mozISqlResult::TYPE_DATE:
      *_retval = 14; // "01/01/4713 BC" - "31/12/32767 AD"
      break;
    case mozISqlResult::TYPE_TIME:
      *_retval = 8;  // 00:00:00-23:59:59
      break;
    case mozISqlResult::TYPE_DATETIME:
      *_retval = 22;
      break;
    case mozISqlResult::TYPE_BOOL:
      *_retval = 1;
      break;
    default:
      *_retval = columnInfo->mSize;
  }

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::Enumerate(mozISqlResultEnumerator** _retval)
{
  mozISqlResultEnumerator* enumerator = new mozSqlResultEnumerator(this);
  if (! enumerator)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*_retval = enumerator);
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::Open(mozISqlInputStream** _retval)
{
  mozSqlResultStream* stream = new mozSqlResultStream(this);
  if (! stream)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*_retval = stream);
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::Reload()
{
  mozISqlResult* result = this;
  nsresult rv = mConnection->ExecuteQuery(mQuery, &result);
  if (NS_FAILED(rv))
    return rv;
  NS_RELEASE(result);

  return NS_OK;
}


NS_IMETHODIMP
mozSqlResult::GetResourceAtIndex(PRInt32 aRowIndex, nsIRDFResource** _retval)
{
  if (aRowIndex < 0 || aRowIndex >= mRows.Count())
    return NS_ERROR_INVALID_ARG;

  NS_ADDREF(*_retval = ((Row*)mRows[aRowIndex])->mSource);
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::GetIndexOfResource(nsIRDFResource *aResource, PRInt32* _retval)
{
  *_retval = -1;

  for (PRInt32 i = 0; i < mRows.Count(); i++) {
    if (((Row*)mRows[i])->mSource == aResource) {
      *_retval = i;
      break;
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
mozSqlResult::GetURI(char **aURI)
{
  *aURI = nsCRT::strdup("rdf:result");
  if (! *aURI)
    return NS_ERROR_OUT_OF_MEMORY;
  
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::GetSource(nsIRDFResource* aPoperty,
                        nsIRDFNode* aTarget,
                        PRBool aTruthValue,
                        nsIRDFResource** _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozSqlResult::GetSources(nsIRDFResource* aProperty,
                         nsIRDFNode* aTarget,
                         PRBool aTruthValue,
                         nsISimpleEnumerator** _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozSqlResult::GetTarget(nsIRDFResource* aSource,
                        nsIRDFResource* aProperty,
                        PRBool aTruthValue,
                        nsIRDFNode** _retval)
{
  *_retval = nsnull;

  nsVoidKey key(aSource);
  Row* row = NS_STATIC_CAST(Row*, mSources.Get(&key));
  if (! row)
    return NS_RDF_NO_VALUE;

  PRInt32 columnIndex = -1;
  for (PRInt32 i = 0; i < mColumnInfo.Count(); i++) {
    nsIRDFResource* property = ((ColumnInfo*)mColumnInfo[i])->mProperty;
    if (property == aProperty) {
      columnIndex = i;
      break;
    }
  }
  if (columnIndex == -1)
    return NS_RDF_NO_VALUE;

  nsCOMPtr<nsIRDFNode> node;

  Cell* cell = row->mCells[columnIndex];
  if (cell->IsNull())
    if (mDisplayNullAsText)
      node = kNullLiteral;
    else
      node = kEmptyLiteral;
  else {
    PRInt32 type = cell->GetType();
    if (type == mozISqlResult::TYPE_STRING) {
      nsCOMPtr<nsIRDFLiteral> literal;
      gRDFService->GetLiteral(cell->mString, getter_AddRefs(literal));
      node = literal;
    }
    else if (type == mozISqlResult::TYPE_INT) {
      nsCOMPtr<nsIRDFInt> literal;
      gRDFService->GetIntLiteral(cell->mInt, getter_AddRefs(literal));
      node = literal;
    }
    else if (type == mozISqlResult::TYPE_FLOAT ||
             type == mozISqlResult::TYPE_DECIMAL) {
      nsCOMPtr<nsIRDFInt> literal;
      gRDFService->GetIntLiteral(cell->mInt, getter_AddRefs(literal));
      node = literal;
    }
    else if (type == mozISqlResult::TYPE_DATE ||
             type == mozISqlResult::TYPE_TIME ||
             type == mozISqlResult::TYPE_DATETIME) {
      nsCOMPtr<nsIRDFDate> literal;
      gRDFService->GetDateLiteral(cell->mDate, getter_AddRefs(literal));
      node = literal;
    }
    else if (type == mozISqlResult::TYPE_BOOL)
      node = cell->mBool ? kTrueLiteral : kFalseLiteral;
  }

  NS_IF_ADDREF(*_retval = node);
  
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::GetTargets(nsIRDFResource* aSource,
                         nsIRDFResource* aProperty,
                         PRBool aTruthValue,
                         nsISimpleEnumerator** _retval)
{
  if (aSource == kSQL_ResultRoot &&
      aProperty == kNC_Child &&
      aTruthValue) {
    nsISimpleEnumerator* enumerator = new mozSqlResultEnumerator(this);
    if (! enumerator)
      return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*_retval = enumerator);

    return NS_OK;
  }    

  return NS_RDF_NO_VALUE;
}

NS_IMETHODIMP
mozSqlResult::Assert(nsIRDFResource* aSource,
                     nsIRDFResource* aProperty,
                     nsIRDFNode* aTarget,
                     PRBool aTruthValue)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
mozSqlResult::Unassert(nsIRDFResource* aSource,
                       nsIRDFResource* aProperty,
                       nsIRDFNode* aTarget)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozSqlResult::Change(nsIRDFResource* aSource,
                     nsIRDFResource* aProperty,
                     nsIRDFNode* aOldTarget,
                     nsIRDFNode* aNewTarget)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozSqlResult::Move(nsIRDFResource* aOldSource,
                   nsIRDFResource* aNewSource,
                   nsIRDFResource* aProperty,
                   nsIRDFNode* aTarget)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozSqlResult::HasAssertion(nsIRDFResource* aSource,
                           nsIRDFResource* aProperty,
                           nsIRDFNode *aTarget,
                           PRBool aTruthValue,
                           PRBool* _retval)
{
  *_retval = PR_FALSE;

  if (aSource == kSQL_ResultRoot &&
      aProperty == kNC_Child &&
      aTruthValue) {
    nsVoidKey key(aTarget);
    Row* row = NS_STATIC_CAST(Row*, mSources.Get(&key));
    if (row)
      *_retval = PR_TRUE;
  }

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::AddObserver(nsIRDFObserver *aObserver)
{
  mObservers.AppendObject(aObserver);

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::RemoveObserver(nsIRDFObserver *aObserver)
{
  mObservers.RemoveObject(aObserver);

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::HasArcIn(nsIRDFNode* aNode,
                       nsIRDFResource* aArc,
                       PRBool* _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}                                                                                       

NS_IMETHODIMP
mozSqlResult::HasArcOut(nsIRDFResource* aSource,
                        nsIRDFResource*aArc,
                        PRBool* _retval)
{                                                                                               
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozSqlResult::ArcLabelsIn(nsIRDFNode* aTarget,
                          nsISimpleEnumerator** _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozSqlResult::ArcLabelsOut(nsIRDFResource* aSource,
                           nsISimpleEnumerator** _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozSqlResult::GetAllResources(nsISimpleEnumerator** _retval)
{
  nsISimpleEnumerator* enumerator = new mozSqlResultEnumerator(this);
  if (! enumerator)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval = enumerator);

  return NS_OK;
}


NS_IMETHODIMP
mozSqlResult::GetAllCmds(nsIRDFResource* aSource,
                         nsISimpleEnumerator** _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozSqlResult::IsCommandEnabled(nsISupportsArray* aSources,
                               nsIRDFResource* aCommand,
                               nsISupportsArray* aArguments,
                               PRBool* _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozSqlResult::DoCommand(nsISupportsArray* aSources,
                        nsIRDFResource* aCommand,
                        nsISupportsArray* aArguments)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozSqlResult::BeginUpdateBatch()
{
  for (PRInt32 i = 0; i < mObservers.Count(); i++) {
    mObservers[i]->OnBeginUpdateBatch(this);
  }
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::EndUpdateBatch()
{
  for (PRInt32 i = 0; i < mObservers.Count(); i++) {
    mObservers[i]->OnEndUpdateBatch(this);
  }
  return NS_OK;
}


NS_IMETHODIMP
mozSqlResult::GetLoaded(PRBool* aResult)
{
  *aResult = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::Init(const char* aURI)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozSqlResult::Refresh(PRBool aBlocking)
{
  if (aBlocking)
    return Reload();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozSqlResult::Flush()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozSqlResult::FlushTo(const char *aURI)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


/*
NS_IMETHODIMP
mozSqlResult::GetRowCount(PRInt32 *aRowCount)
{
  *aRowCount = mRows.Count();
  return NS_OK;
}
*/

NS_IMETHODIMP
mozSqlResult::GetSelection(nsITreeSelection * *aSelection)
{
  NS_IF_ADDREF(*aSelection = mSelection);
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::SetSelection(nsITreeSelection * aSelection)
{
  mSelection = aSelection;
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::GetRowProperties(PRInt32 index, nsISupportsArray *properties)
{
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::GetCellProperties(PRInt32 row, nsITreeColumn* col, nsISupportsArray *properties)
{
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::GetColumnProperties(nsITreeColumn* aCol, nsISupportsArray *properties)
{
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::IsContainer(PRInt32 index, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::IsContainerOpen(PRInt32 index, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::IsContainerEmpty(PRInt32 index, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::IsSeparator(PRInt32 index, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::IsSorted(PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::CanDrop(PRInt32 index, PRInt32 orientation, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::Drop(PRInt32 row, PRInt32 orientation)
{
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::GetParentIndex(PRInt32 rowIndex, PRInt32 *_retval)
{
  *_retval = -1;
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::HasNextSibling(PRInt32 rowIndex, PRInt32 afterIndex, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::GetLevel(PRInt32 index, PRInt32 *_retval)
{
  *_retval = 0;
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::GetImageSrc(PRInt32 row, nsITreeColumn* col, nsAString & _retval)
{
  _retval.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::GetProgressMode(PRInt32 row, nsITreeColumn* col, PRInt32 *_retval)
{
  *_retval = 0;
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::GetCellValue(PRInt32 row, nsITreeColumn* col, nsAString & _retval)
{
  PRInt32 columnIndex;
  col->GetIndex(&columnIndex);

  Cell* cell = ((Row*)mRows[row])->mCells[columnIndex];
  if (! cell->IsNull()) {
    PRInt32 type = cell->GetType();
    if (type == mozISqlResult::TYPE_BOOL) {
      if (cell->mBool)
        _retval.Assign(NS_LITERAL_STRING("true"));
      else
        _retval.Assign(NS_LITERAL_STRING("false"));
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::GetCellText(PRInt32 row, nsITreeColumn* col, nsAString & _retval)
{
  PRInt32 columnIndex;
  col->GetIndex(&columnIndex);

  Cell* cell = ((Row*)mRows[row])->mCells[columnIndex];
  if (cell->IsNull()) {
    if (mDisplayNullAsText)
      _retval.Assign(NS_LITERAL_STRING("null"));
  }
  else {
    PRInt32 type = cell->GetType();
    if (type == mozISqlResult::TYPE_STRING)
      _retval.Assign(cell->mString);
    else if (type == mozISqlResult::TYPE_INT) {
      nsAutoString s;
      s.AppendInt(cell->mInt);
      _retval.Assign(s);
    }
    else if (type == mozISqlResult::TYPE_FLOAT ||
             type == mozISqlResult::TYPE_DECIMAL) {
      nsAutoString s;
      s.AppendFloat(cell->mFloat);
      _retval.Assign(s);
    }
    else if (type == mozISqlResult::TYPE_DATE ||
             type == mozISqlResult::TYPE_TIME ||
             type == mozISqlResult::TYPE_DATETIME) {
      nsAutoString value;
      mozSqlResult::gFormat->FormatPRTime(nsnull,
                                          type != mozISqlResult::TYPE_TIME ? kDateFormatShort : kDateFormatNone,
                                          type != mozISqlResult::TYPE_DATE ? kTimeFormatSeconds : kTimeFormatNone,
                                          PRTime(cell->mDate),
                                          value);
      _retval.Assign(value);
    }
    else if (type == mozISqlResult::TYPE_BOOL) {
      if (cell->mBool)
        _retval.Assign(NS_LITERAL_STRING("true"));
      else
        _retval.Assign(NS_LITERAL_STRING("false"));
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::SetTree(nsITreeBoxObject *tree)
{
  mBoxObject = tree;
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::ToggleOpenState(PRInt32 index)
{
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::CycleHeader(nsITreeColumn* aCol)
{
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::SelectionChanged()
{
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::CycleCell(PRInt32 row, nsITreeColumn* aCol)
{
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::IsEditable(PRInt32 row, nsITreeColumn* col, PRBool *_retval)
{
  return CanUpdate(_retval);
}

NS_IMETHODIMP
mozSqlResult::SetCellValue(PRInt32 row, nsITreeColumn* col, const nsAString& value)
{
  PRInt32 columnIndex;
  col->GetIndex(&columnIndex);

  Row* srcRow = (Row*)mRows[row];
  Row* buffer = Row::Create(mAllocator, nsnull, mColumnInfo, srcRow);

  Cell* cell = buffer->mCells[columnIndex];

  if (value.EqualsLiteral("true")) {
    cell->mBool = PR_TRUE;
  }
  else if (value.EqualsLiteral("false")) {
    cell->mBool = PR_FALSE;
  }
  
  PRInt32 count;
  nsresult rv = UpdateRow(row, buffer, &count);
  if (NS_FAILED(rv))
    return rv;

  if (mBoxObject)
    mBoxObject->InvalidateCell(row, col);

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::SetCellText(PRInt32 row, nsITreeColumn* col, const nsAString& value)
{
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::PerformAction(const PRUnichar *action)
{
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::PerformActionOnRow(const PRUnichar *action, PRInt32 row)
{
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResult::PerformActionOnCell(const PRUnichar *action, PRInt32 row, nsITreeColumn* aCol)
{
  return NS_OK;
}


void
mozSqlResult::ClearColumnInfo()
{
  for (PRInt32 i = 0; i < mColumnInfo.Count(); i++) {
    ColumnInfo* columnInfo = (ColumnInfo*)mColumnInfo[i];
    ColumnInfo::Destroy(mAllocator, columnInfo);
  }
  mColumnInfo.Clear();
}

void
mozSqlResult::ClearRows()
{
  for (PRInt32 i = 0; i < mRows.Count(); i++) {
    Row* row = (Row*)mRows[i];
    Row::Destroy(mAllocator, mColumnInfo.Count(), row);
  }
  mRows.Clear();
  mSources.Reset();
}

nsresult
mozSqlResult::EnsureTableName()
{
  if (!mTableName.IsEmpty())
    return NS_OK;

  nsAString::const_iterator start, end;
  mQuery.BeginReading(start);
  mQuery.EndReading(end);

  NS_NAMED_LITERAL_STRING(from, "from");
  nsAString::const_iterator iter = end;
  if (FindInReadable(from, start, iter, nsCaseInsensitiveStringComparator())) {
    while (iter != end && nsCRT::IsAsciiSpace(*iter))
      ++iter;
    start = iter;
    while (iter != end && !nsCRT::IsAsciiSpace(*iter))
      ++iter;
    mTableName.Assign(Substring(start, iter));
  }
  else
    return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult
mozSqlResult::EnsurePrimaryKeys()
{
  if (mPrimaryKeys)
    return NS_OK;

  nsAutoString schema;
  nsAutoString table;
  nsAString::const_iterator start, s;
  nsAString::const_iterator end, e;
  mTableName.BeginReading(start);
  mTableName.EndReading(end);
  s = start;
  e = end;
  if (FindInReadable(NS_LITERAL_STRING("."), s, e)) {
    schema.Assign(Substring(start, s));
    table.Assign(Substring(e, end));
  }
  else {
    table.Assign(mTableName);
  }

  nsCOMPtr<mozISqlResult> result;
  nsresult rv = mConnection->GetPrimaryKeys(schema, table, getter_AddRefs(result));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<mozISqlResultEnumerator> primaryKeys;
  rv = result->Enumerate(getter_AddRefs(primaryKeys));
  if (NS_FAILED(rv))
    return rv;

  rv = primaryKeys->First();
  if (NS_FAILED(rv))
    return rv;

  mPrimaryKeys = primaryKeys;
  return NS_OK;
}

void
mozSqlResult::AppendValue(Cell* aCell, nsAutoString& aValues)
{
  if (aCell->IsNull())
    aValues.Append(NS_LITERAL_STRING("NULL"));
  else if (aCell->IsDefault())
    aValues.Append(NS_LITERAL_STRING("DEFAULT"));
  else {
    PRInt32 type = aCell->GetType();
    if (type == mozISqlResult::TYPE_STRING) {
      aValues.Append(PRUnichar('\''));
      aValues.Append(aCell->mString);
      aValues.Append(PRUnichar('\''));
    }
    else if (type == mozISqlResult::TYPE_INT)
      aValues.AppendInt(aCell->mInt);
    else if (type == mozISqlResult::TYPE_FLOAT ||
             type == mozISqlResult::TYPE_DECIMAL)
      aValues.AppendFloat(aCell->mFloat);
    else if (type == mozISqlResult::TYPE_DATE ||
             type == mozISqlResult::TYPE_TIME ||
             type == mozISqlResult::TYPE_DATETIME) {
      aValues.Append(PRUnichar('\''));
      nsAutoString value;
      gFormat->FormatPRTime(nsnull,
                            type != mozISqlResult::TYPE_TIME ? kDateFormatLong : kDateFormatNone,
                            type != mozISqlResult::TYPE_DATE ? kTimeFormatSeconds : kTimeFormatNone,
                            PRTime(aCell->mDate),
                            value);
      aValues.Append(value);
      aValues.Append(PRUnichar('\''));
    }
    else if (type == mozISqlResult::TYPE_BOOL) {
      aValues.Append(PRUnichar('\''));
      aValues.AppendInt(aCell->mBool);
      aValues.Append(PRUnichar('\''));
    }
  }
}

nsresult
mozSqlResult::AppendKeys(Row* aRow, nsAutoString& aKeys)
{
  mPrimaryKeys->BeforeFirst();

  PRBool hasNext = PR_FALSE;
  do {
    if (hasNext)
      aKeys.Append(NS_LITERAL_STRING(" AND "));

    mPrimaryKeys->Next(&hasNext);

    nsAutoString value;
    mPrimaryKeys->GetString(2, value);
    aKeys.Append(value);
    aKeys.Append(PRUnichar('='));

    PRInt32 index;
    GetColumnIndex(value, &index);
    if (index == -1) {
      mErrorMessage = NS_LITERAL_STRING("MOZSQL: The result doesn't contain all primary key fields");
      return NS_ERROR_FAILURE;
    }

    Cell* cell = aRow->mCells[index];
    AppendValue(cell, aKeys);

  } while(hasNext);

  return NS_OK;
}

nsresult
mozSqlResult::GetValues(Row* aRow, mozISqlResult** aResult, PRBool aUseID)
{
  nsAutoString query(mQuery);
  nsAString::const_iterator start;
  nsAString::const_iterator end;
  query.BeginReading(start);
  query.EndReading(end);

  NS_NAMED_LITERAL_STRING(where, "WHERE");
  nsAString::const_iterator s = start;
  nsAString::const_iterator e = end;
  if (FindInReadable(where, s, e, nsCaseInsensitiveStringComparator())) {
    nsAutoString keys(PRUnichar(' '));

    if (aUseID) {
      nsAutoString IDName;
      ((mozSqlConnection*)mConnection.get())->GetIDName(IDName);
      PRInt32 lastID;
      mConnection->GetLastID(&lastID);
      keys.Append(IDName);
      keys.Append(PRUnichar('='));
      keys.AppendInt(lastID);
    }
    else {
      nsresult rv = AppendKeys(aRow, keys);
      if (NS_FAILED(rv))
        return rv;
    }

    keys.Append(NS_LITERAL_STRING(" AND "));
    query.Insert(keys, Distance(start, e));
  }
  else {
    NS_NAMED_LITERAL_STRING(from, "FROM");
    s = start;
    e = end;
    if (FindInReadable(from, s, e, nsCaseInsensitiveStringComparator())) {
      while (e != end && nsCRT::IsAsciiSpace(*e))
        ++e;
      while (e != end && !nsCRT::IsAsciiSpace(*e))
        ++e;
      nsAutoString keys(NS_LITERAL_STRING(" WHERE "));

      if (aUseID) {
        nsAutoString IDName;
        ((mozSqlConnection*)mConnection.get())->GetIDName(IDName);
        PRInt32 lastID;
        mConnection->GetLastID(&lastID);
        keys.Append(IDName);
        keys.Append(PRUnichar('='));
        keys.AppendInt(lastID);
      }
      else {
        nsresult rv = AppendKeys(aRow, keys);
        if (NS_FAILED(rv))
          return rv;
      }

      query.Insert(keys, Distance(start, e));
    }
  }


  nsCOMPtr<mozISqlResult> result;
  nsresult rv = mConnection->ExecuteQuery(query, getter_AddRefs(result));

  if (NS_FAILED(rv)) {
    mConnection->GetErrorMessage(mErrorMessage);
    return rv;
  }

  NS_ADDREF(*aResult = result);

  return NS_OK;
}

nsresult
mozSqlResult::CopyValues(mozISqlResult* aResult, Row* aRow)
{
  nsCOMPtr<mozISqlResultEnumerator> enumerator;
  nsresult rv = aResult->Enumerate(getter_AddRefs(enumerator));
  if (NS_FAILED(rv))
    return rv;

  rv = enumerator->First();
  if (NS_FAILED(rv))
    return rv;

  PRInt32 columnCount;
  aResult->GetColumnCount(&columnCount);
  for (PRInt32 i = 0; i < columnCount; i++) {
    Cell* cell = aRow->mCells[i];
    PRBool isNull;
    enumerator->IsNull(i, &isNull);
    if (isNull)
      cell->SetNull(PR_TRUE);
    else {
      cell->SetNull(PR_FALSE);
      PRInt32 type;
      aResult->GetColumnType(i, &type);
      if (type == mozISqlResult::TYPE_STRING) {
        nsAutoString value;
        enumerator->GetString(i, value);
        cell->SetString(ToNewUnicode(value));
      }
      else if (type == mozISqlResult::TYPE_INT)
        enumerator->GetInt(i, &cell->mInt);
      else if (type == mozISqlResult::TYPE_FLOAT)
        enumerator->GetFloat(i, &cell->mFloat);
      else if (type == mozISqlResult::TYPE_DECIMAL)
        enumerator->GetDecimal(i, &cell->mFloat);
      else if (type == mozISqlResult::TYPE_DATE)
        enumerator->GetDate(i, &cell->mDate);
      else if (type == mozISqlResult::TYPE_TIME)
        enumerator->GetDate(i, &cell->mDate);
      else if (type == mozISqlResult::TYPE_DATETIME)
        enumerator->GetDate(i, &cell->mDate);
      else if (type == mozISqlResult::TYPE_BOOL)
        enumerator->GetBool(i, &cell->mBool);
    }
  }

  return NS_OK;
}

nsresult
mozSqlResult::InsertRow(Row* aSrcRow, PRInt32* _retval)
{
  *_retval = -1;

  nsresult rv = EnsureTableName();
  if (NS_FAILED(rv))
    return rv;

  rv = EnsurePrimaryKeys();
  if (NS_FAILED(rv))
    return rv;

  nsAutoString names;
  nsAutoString values;

  names.Append(PRUnichar('('));
  values.Append(PRUnichar('('));
  PRInt32 i;
  for (i = 0; i < mColumnInfo.Count(); i++) {
    if (i) {
      names.Append(NS_LITERAL_STRING(", "));
      values.Append(NS_LITERAL_STRING(", "));
    }
    names.Append(((ColumnInfo*)mColumnInfo[i])->mName);

    Cell* cell = aSrcRow->mCells[i];
    AppendValue(cell, values);
  }
  names.Append(PRUnichar(')'));
  values.Append(PRUnichar(')'));

  PRInt32 affectedRows;
  rv = mConnection->ExecuteUpdate(NS_LITERAL_STRING("INSERT INTO ") +
    mTableName + names + NS_LITERAL_STRING(" VALUES") + values, &affectedRows);

  if (NS_FAILED(rv)) {
    mConnection->GetErrorMessage(mErrorMessage);
    return rv;
  }

  nsCOMPtr<mozISqlResult> result;
  rv = GetValues(aSrcRow, getter_AddRefs(result), PR_TRUE);
  if (NS_FAILED(rv))
    return rv;

  PRInt32 rowCount;
  result->GetRowCount(&rowCount);
  if (rowCount == 0) {
    *_retval = 0;
    return NS_OK;
  }

  rv = CopyValues(result, aSrcRow);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIRDFResource> resource;
  gRDFService->GetAnonymousResource(getter_AddRefs(resource));

  Row* row = Row::Create(mAllocator, resource, mColumnInfo, aSrcRow);
  mRows.AppendElement(row);
  nsVoidKey key(resource);
  mSources.Put(&key, row);

  for (i = 0; i < mObservers.Count(); i++)
    mObservers[i]->OnAssert(this, kSQL_ResultRoot, kNC_Child, resource);

  if (mBoxObject)
    mBoxObject->RowCountChanged(mRows.Count() - 1, 1);

  *_retval = 1;
  return NS_OK;
}

nsresult
mozSqlResult::UpdateRow(PRInt32 aRowIndex, Row* aSrcRow, PRInt32* _retval)
{
  *_retval = -1;

  nsresult rv = EnsureTableName();
  if (NS_FAILED(rv))
    return rv;

  rv = EnsurePrimaryKeys();
  if (NS_FAILED(rv))
    return rv;

  nsAutoString values;
  PRInt32 i;
  for (i = 0; i < mColumnInfo.Count(); i++) {
    if (i)
      values.Append(NS_LITERAL_STRING(", "));
    values.Append(((ColumnInfo*)mColumnInfo[i])->mName);
    values.Append(PRUnichar('='));

    Cell* cell = aSrcRow->mCells[i];
    AppendValue(cell, values);
  }

  Row* row = (Row*)mRows[aRowIndex];

  nsAutoString keys;
  rv = AppendKeys(row, keys);
  if (NS_FAILED(rv))
    return rv;

  PRInt32 affectedRows;
  rv = mConnection->ExecuteUpdate(NS_LITERAL_STRING("UPDATE ") + mTableName +
    NS_LITERAL_STRING(" SET ") + values + NS_LITERAL_STRING(" WHERE ") + keys,
    &affectedRows);

  if (NS_FAILED(rv)) {
    mConnection->GetErrorMessage(mErrorMessage);
    return rv;
  }

  nsCOMPtr<mozISqlResult> result;
  rv = GetValues(aSrcRow, getter_AddRefs(result), PR_FALSE);
  if (NS_FAILED(rv))
    return rv;

  PRInt32 rowCount;
  result->GetRowCount(&rowCount);
  if (rowCount == 0) {
    mRows.RemoveElementAt(aRowIndex);
    nsVoidKey key(row->mSource);
    mSources.Remove(&key);

    for (PRInt32 i = 0; i < mObservers.Count(); i++)
      mObservers[i]->OnUnassert(this, kSQL_ResultRoot, kNC_Child, row->mSource);

    if (mBoxObject)
      mBoxObject->RowCountChanged(aRowIndex, -1);

    Row::Destroy(mAllocator, mColumnInfo.Count(), row);

    *_retval = 0;
    return NS_OK;
  }

  rv = CopyValues(result, row);
  if (NS_FAILED(rv))
    return rv;

  for (i = 0; i < mColumnInfo.Count(); i++) {
    nsCOMPtr<nsIRDFNode> oldNode;
    nsCOMPtr<nsIRDFNode> newNode;

    Cell* cell = row->mCells[i];
    if (cell->IsNull())
      if (mDisplayNullAsText)
        newNode = kNullLiteral;
      else
        newNode = kEmptyLiteral;
    else {
      PRInt32 type = cell->GetType();
      if (type == mozISqlResult::TYPE_STRING) {
        nsCOMPtr<nsIRDFLiteral> literal;
        PRUnichar* value = cell->mString;
        gRDFService->GetLiteral(value, getter_AddRefs(literal));
        newNode = literal;
      }
      else if (type == mozISqlResult::TYPE_INT) {
        nsCOMPtr<nsIRDFInt> literal;
        PRInt32 value = cell->mInt;
        gRDFService->GetIntLiteral(value, getter_AddRefs(literal));
        newNode = literal;
      }
      else if (type == mozISqlResult::TYPE_FLOAT ||
               type == mozISqlResult::TYPE_DECIMAL) {
        nsCOMPtr<nsIRDFInt> literal;
        PRInt32 value = cell->mInt;
        gRDFService->GetIntLiteral(value, getter_AddRefs(literal));
        newNode = literal;
      }
      else if (type == mozISqlResult::TYPE_DATE ||
               type == mozISqlResult::TYPE_TIME ||
               type == mozISqlResult::TYPE_DATETIME) {
        nsCOMPtr<nsIRDFDate> literal;
        PRInt64 value = cell->mDate;
        gRDFService->GetDateLiteral(value, getter_AddRefs(literal));
        newNode = literal;
      }
      else if (type == mozISqlResult::TYPE_BOOL)
        newNode = cell->mBool ? kTrueLiteral : kFalseLiteral;
    }

    for (PRInt32 j = 0; j < mObservers.Count(); j++) {
      nsIRDFResource* source = row->mSource;
      nsIRDFResource* property = ((ColumnInfo*)mColumnInfo[i])->mProperty;
      mObservers[j]->OnChange(this, source, property, oldNode, newNode);
    }
  }

  if (mBoxObject)
    mBoxObject->InvalidateRow(aRowIndex);

  *_retval = 1;
  return NS_OK;
}

nsresult
mozSqlResult::DeleteRow(PRInt32 aRowIndex, PRInt32* _retval)
{
  *_retval = -1;

  nsresult rv = EnsureTableName();
  if (NS_FAILED(rv))
    return rv;

  rv = EnsurePrimaryKeys();
  if (NS_FAILED(rv))
    return rv;

  Row* row = (Row*)mRows[aRowIndex];

  nsAutoString keys;
  rv = AppendKeys(row, keys);
  if (NS_FAILED(rv))
    return rv;

  PRInt32 affectedRows;
  rv = mConnection->ExecuteUpdate(NS_LITERAL_STRING("DELETE FROM ") +
    mTableName + NS_LITERAL_STRING(" WHERE ") + keys, &affectedRows);

  if (NS_FAILED(rv)) {
    mConnection->GetErrorMessage(mErrorMessage);
    return rv;
  }

  mRows.RemoveElementAt(aRowIndex);
  nsVoidKey key(row->mSource);
  mSources.Remove(&key);

  for (PRInt32 i = 0; i < mObservers.Count(); i++)
    mObservers[i]->OnUnassert(this, kSQL_ResultRoot, kNC_Child, row->mSource);

  if (mBoxObject)
    mBoxObject->RowCountChanged(aRowIndex, -1);

  Row::Destroy(mAllocator, mColumnInfo.Count(), row);

  *_retval = 1;
  return NS_OK;
}

nsresult
mozSqlResult::GetCondition(Row* aRow, nsAString& aCurrentCondition)
{
  nsresult rv = EnsureTableName();
  if (NS_FAILED(rv))
    return rv;

  rv = EnsurePrimaryKeys();
  if (NS_FAILED(rv))
    return rv;

  nsAutoString keys;
  rv = AppendKeys(aRow, keys);
  if (NS_FAILED(rv))
    return rv;

  aCurrentCondition = keys;

  return NS_OK;
}


mozSqlResultEnumerator::mozSqlResultEnumerator(mozSqlResult* aResult)
  : mResult(aResult),
    mCurrentIndex(-1),
    mCurrentRow(nsnull)
{
  NS_ADDREF(mResult);

  mBuffer = Row::Create(mResult->mAllocator, nsnull, mResult->mColumnInfo);
}

mozSqlResultEnumerator::~mozSqlResultEnumerator()
{
  Row::Destroy(mResult->mAllocator, mResult->mColumnInfo.Count(), mBuffer);

  NS_RELEASE(mResult);
}


NS_IMPL_ISUPPORTS2(mozSqlResultEnumerator,
                   mozISqlResultEnumerator,
                   nsISimpleEnumerator)

NS_IMETHODIMP
mozSqlResultEnumerator::GetErrorMessage(nsAString& aErrorMessage)
{
  aErrorMessage = mResult->mErrorMessage;
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultEnumerator::Next(PRBool* _retval)
{
  if (mCurrentIndex + 1 > mResult->mRows.Count() - 1)
    return NS_ERROR_FAILURE;

  mCurrentIndex++;
  mCurrentRow = (Row*)mResult->mRows[mCurrentIndex];

  *_retval = mCurrentIndex < mResult->mRows.Count() - 1;
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultEnumerator::Previous(PRBool* _retval)
{
  if (mCurrentIndex - 1 < 0)
    return NS_ERROR_FAILURE;

  mCurrentIndex--;
  mCurrentRow = (Row*)mResult->mRows[mCurrentIndex];

  *_retval = mCurrentIndex > 0;
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultEnumerator::BeforeFirst()
{
  mCurrentIndex = -1;
  mCurrentRow = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultEnumerator::First()
{
  if (mResult->mRows.Count() == 0)
    return NS_ERROR_FAILURE;

  mCurrentIndex = 0;
  mCurrentRow = (Row*)mResult->mRows[mCurrentIndex];

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultEnumerator::Last()
{
  if (mResult->mRows.Count() == 0)
    return NS_ERROR_FAILURE;

  mCurrentIndex = mResult->mRows.Count() - 1;
  mCurrentRow = (Row*)mResult->mRows[mCurrentIndex];

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultEnumerator::Relative(PRInt32 aRows)
{
  if (mResult->mRows.Count() == 0)
    return NS_ERROR_FAILURE;

  PRInt32 newIndex = mCurrentIndex + aRows;
  if (newIndex < 0 || newIndex > mResult->mRows.Count() - 1)
    return NS_ERROR_FAILURE;

  mCurrentIndex = newIndex;
  mCurrentRow = (Row*)mResult->mRows[mCurrentIndex];

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultEnumerator::Absolute(PRInt32 aRowIndex)
{
  if (mResult->mRows.Count() == 0)
    return NS_ERROR_FAILURE;

  if (aRowIndex < 0 || aRowIndex > mResult->mRows.Count() - 1)
    return NS_ERROR_FAILURE;

  mCurrentIndex = aRowIndex;
  mCurrentRow = (Row*)mResult->mRows[mCurrentIndex];

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultEnumerator::IsNull(PRInt32 aColumnIndex, PRBool* _retval)
{
  if (! mCurrentRow)
    return NS_ERROR_FAILURE;

  Cell* cell = mCurrentRow->mCells[aColumnIndex];
  *_retval = cell->IsNull();

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultEnumerator::GetVariant(PRInt32 aColumnIndex, nsIVariant** _retval)
{
  if (! mCurrentRow)
    return NS_ERROR_FAILURE;

  nsresult rv;
  nsCOMPtr<nsIWritableVariant> variant = do_CreateInstance(NS_VARIANT_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  Cell* cell = mCurrentRow->mCells[aColumnIndex];
  if (! cell->IsNull()) {
    PRInt32 type = cell->GetType();
    if (type == mozISqlResult::TYPE_STRING)
      variant->SetAsWString(cell->mString);
    else if (type == mozISqlResult::TYPE_INT)
      variant->SetAsInt32(cell->mInt);
    else if (type == mozISqlResult::TYPE_FLOAT ||
             type == mozISqlResult::TYPE_DECIMAL)
      variant->SetAsFloat(cell->mFloat);
    else if (type == mozISqlResult::TYPE_DATE ||
             type == mozISqlResult::TYPE_TIME ||
             type == mozISqlResult::TYPE_DATETIME) {
      nsAutoString value;
      mozSqlResult::gFormat->FormatPRTime(nsnull,
                                          type != mozISqlResult::TYPE_TIME ? kDateFormatShort : kDateFormatNone,
                                          type != mozISqlResult::TYPE_DATE ? kTimeFormatSeconds : kTimeFormatNone,
                                          PRTime(cell->mDate),
                                          value);
      variant->SetAsAString(value);
    }
    else if (type == mozISqlResult::TYPE_BOOL)
      variant->SetAsBool(cell->mBool);
  }

  NS_ADDREF(*_retval = variant);

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultEnumerator::GetString(PRInt32 aColumnIndex, nsAString& _retval)
{
  if (! mCurrentRow)
    return NS_ERROR_FAILURE;

  Cell* cell = mCurrentRow->mCells[aColumnIndex];

  if (cell->GetType() != mozISqlResult::TYPE_STRING)
    return NS_ERROR_FAILURE;

  if (! cell->IsNull())
    _retval.Assign(cell->mString);

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultEnumerator::GetInt(PRInt32 aColumnIndex, PRInt32* _retval)
{
  if (aColumnIndex < 0 || aColumnIndex >= mResult->mColumnInfo.Count())
    return NS_ERROR_INVALID_ARG;

  if (! mCurrentRow)
    return NS_ERROR_FAILURE;

  Cell* cell = mCurrentRow->mCells[aColumnIndex];

  if (cell->GetType() != mozISqlResult::TYPE_INT)
    return NS_ERROR_FAILURE;

  *_retval = cell->IsNull() ? 0 : cell->mInt;

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultEnumerator::GetFloat(PRInt32 aColumnIndex, float* _retval)
{
  if (aColumnIndex < 0 || aColumnIndex >= mResult->mColumnInfo.Count())
    return NS_ERROR_INVALID_ARG;

  if (! mCurrentRow)
    return NS_ERROR_FAILURE;

  Cell* cell = mCurrentRow->mCells[aColumnIndex];

  if (cell->GetType() != mozISqlResult::TYPE_FLOAT)
    return NS_ERROR_FAILURE;

  *_retval = cell->IsNull() ? 0 : cell->mFloat;

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultEnumerator::GetDecimal(PRInt32 aColumnIndex, float* _retval)
{
  if (aColumnIndex < 0 || aColumnIndex >= mResult->mColumnInfo.Count())
    return NS_ERROR_INVALID_ARG;

  if (! mCurrentRow)
    return NS_ERROR_FAILURE;

  Cell* cell = mCurrentRow->mCells[aColumnIndex];

  if (cell->GetType() != mozISqlResult::TYPE_DECIMAL)
    return NS_ERROR_FAILURE;

  *_retval = cell->IsNull() ? 0 : cell->mFloat;

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultEnumerator::GetDate(PRInt32 aColumnIndex, PRInt64* _retval)
{
  if (aColumnIndex < 0 || aColumnIndex >= mResult->mColumnInfo.Count())
    return NS_ERROR_INVALID_ARG;

  if (! mCurrentRow)
    return NS_ERROR_FAILURE;

  Cell* cell = mCurrentRow->mCells[aColumnIndex];

  if (cell->GetType() != mozISqlResult::TYPE_DATE)
    return NS_ERROR_FAILURE;

  *_retval = cell->IsNull() ? 0 : cell->mDate;

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultEnumerator::GetBool(PRInt32 aColumnIndex, PRBool* _retval)
{
  if (aColumnIndex < 0 || aColumnIndex >= mResult->mColumnInfo.Count())
    return NS_ERROR_INVALID_ARG;

  if (! mCurrentRow)
    return NS_ERROR_FAILURE;

  Cell* cell = mCurrentRow->mCells[aColumnIndex];

  if (cell->GetType() != mozISqlResult::TYPE_BOOL)
    return NS_ERROR_FAILURE;

  *_retval = cell->IsNull() ? 0 : cell->mBool;

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultEnumerator::SetNull(PRInt32 aColumnIndex)
{
  if (aColumnIndex < 0 || aColumnIndex >= mResult->mColumnInfo.Count())
    return NS_ERROR_INVALID_ARG;

  if (! mBuffer)
    return NS_ERROR_FAILURE;

  Cell* cell = mBuffer->mCells[aColumnIndex];
  cell->SetNull(PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultEnumerator::SetDefault(PRInt32 aColumnIndex)
{
  if (aColumnIndex < 0 || aColumnIndex >= mResult->mColumnInfo.Count())
    return NS_ERROR_INVALID_ARG;

  if (! mBuffer)
    return NS_ERROR_FAILURE;

  Cell* cell = mBuffer->mCells[aColumnIndex];
  cell->SetDefault(PR_TRUE);

  return NS_OK;
}


NS_IMETHODIMP
mozSqlResultEnumerator::Copy(PRInt32 aColumnIndex)
{
  if (aColumnIndex < 0 || aColumnIndex >= mResult->mColumnInfo.Count())
    return NS_ERROR_INVALID_ARG;

  if (! mCurrentRow)
    return NS_ERROR_FAILURE;
  if (! mBuffer)
    return NS_ERROR_FAILURE;

  Cell* currentCell = mCurrentRow->mCells[aColumnIndex];
  Cell* bufferCell = mBuffer->mCells[aColumnIndex];
  Cell::Copy(currentCell, bufferCell);

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultEnumerator::SetVariant(PRInt32 aColumnIndex, nsIVariant* aValue)
{
  if (aColumnIndex < 0 || aColumnIndex >= mResult->mColumnInfo.Count())
    return NS_ERROR_INVALID_ARG;

  if (! mBuffer)
    return NS_ERROR_FAILURE;

  Cell* cell = mBuffer->mCells[aColumnIndex];
  cell->SetNull(PR_FALSE);
  PRInt32 type = cell->GetType();
  if (type == mozISqlResult::TYPE_STRING) {
    PRUnichar* value;
    aValue->GetAsWString(&value);
    cell->SetString(value);
  }
  else if (type == mozISqlResult::TYPE_INT) {
    PRInt32 value;
    aValue->GetAsInt32(&value);
    cell->mInt = value;
  }
  else if (type == mozISqlResult::TYPE_FLOAT ||
           type == mozISqlResult::TYPE_DECIMAL) {
    float value;
    aValue->GetAsFloat(&value);
    cell->mFloat = value;
  }
  else if (type == mozISqlResult::TYPE_DATE ||
           type == mozISqlResult::TYPE_TIME ||
           type == mozISqlResult::TYPE_DATETIME) {
    nsCAutoString value;
    aValue->GetAsACString(value);
    PR_ParseTimeString(value.get(), PR_FALSE, &cell->mDate);
  }
  else if (type == mozISqlResult::TYPE_BOOL) {
    PRBool value;
    aValue->GetAsBool(&value);
    cell->mBool = value;
  }

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultEnumerator::SetString(PRInt32 aColumnIndex, const nsAString& aValue)
{
  if (aColumnIndex < 0 || aColumnIndex >= mResult->mColumnInfo.Count())
    return NS_ERROR_INVALID_ARG;

  if (! mBuffer)
    return NS_ERROR_FAILURE;

  Cell* cell = mBuffer->mCells[aColumnIndex];

  if (cell->GetType() != mozISqlResult::TYPE_STRING)
    return NS_ERROR_FAILURE;

  cell->SetNull(PR_FALSE);
  cell->SetString(ToNewUnicode(aValue));

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultEnumerator::SetInt(PRInt32 aColumnIndex, PRInt32 aValue)
{
  if (aColumnIndex < 0 || aColumnIndex >= mResult->mColumnInfo.Count())
    return NS_ERROR_INVALID_ARG;

  if (! mBuffer)
    return NS_ERROR_FAILURE;

  Cell* cell = mBuffer->mCells[aColumnIndex];

  if (cell->GetType() != mozISqlResult::TYPE_INT)
    return NS_ERROR_FAILURE;

  cell->SetNull(PR_FALSE);
  cell->mInt = aValue;

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultEnumerator::SetFloat(PRInt32 aColumnIndex, float aValue)
{
  if (aColumnIndex < 0 || aColumnIndex >= mResult->mColumnInfo.Count())
    return NS_ERROR_INVALID_ARG;

  if (! mBuffer)
    return NS_ERROR_FAILURE;

  Cell* cell = mBuffer->mCells[aColumnIndex];

  if (cell->GetType() != mozISqlResult::TYPE_FLOAT)
    return NS_ERROR_FAILURE;

  cell->SetNull(PR_FALSE);
  cell->mFloat = aValue;

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultEnumerator::SetDecimal(PRInt32 aColumnIndex, float aValue)
{
  if (aColumnIndex < 0 || aColumnIndex >= mResult->mColumnInfo.Count())
    return NS_ERROR_INVALID_ARG;

  if (! mBuffer)
    return NS_ERROR_FAILURE;

  Cell* cell = mBuffer->mCells[aColumnIndex];

  if (cell->GetType() != mozISqlResult::TYPE_DECIMAL)
    return NS_ERROR_FAILURE;

  cell->SetNull(PR_FALSE);
  cell->mFloat = aValue;

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultEnumerator::SetDate(PRInt32 aColumnIndex, PRInt64 aValue)
{
  if (aColumnIndex < 0 || aColumnIndex >= mResult->mColumnInfo.Count())
    return NS_ERROR_INVALID_ARG;

  if (! mBuffer)
    return NS_ERROR_FAILURE;

  Cell* cell = mBuffer->mCells[aColumnIndex];

  if (cell->GetType() != mozISqlResult::TYPE_DATE)
    return NS_ERROR_FAILURE;

  cell->SetNull(PR_FALSE);
  cell->mDate = aValue;

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultEnumerator::SetBool(PRInt32 aColumnIndex, PRBool aValue)
{
  if (aColumnIndex < 0 || aColumnIndex >= mResult->mColumnInfo.Count())
    return NS_ERROR_INVALID_ARG;

  if (! mBuffer)
    return NS_ERROR_FAILURE;

  Cell* cell = mBuffer->mCells[aColumnIndex];

  if (cell->GetType() != mozISqlResult::TYPE_BOOL)
    return NS_ERROR_FAILURE;

  cell->SetNull(PR_FALSE);
  cell->mBool = aValue;

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultEnumerator::SetNullValues()
{
  for (PRInt32 i = 0; i < mResult->mColumnInfo.Count(); i++) {
    Cell* cell = mBuffer->mCells[i];
    cell->SetNull(PR_TRUE);
  }

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultEnumerator::SetDefaultValues()
{
  for (PRInt32 i = 0; i < mResult->mColumnInfo.Count(); i++) {
    Cell* cell = mBuffer->mCells[i];
    cell->SetDefault(PR_TRUE);
  }

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultEnumerator::CopyValues()
{
  if (! mCurrentRow)
    return NS_ERROR_FAILURE;

  Row::Copy(mResult->mColumnInfo.Count(), mCurrentRow, mBuffer);

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultEnumerator::CanInsert(PRBool* _retval)
{
  return mResult->CanInsert(_retval);
}

NS_IMETHODIMP
mozSqlResultEnumerator::CanUpdate(PRBool* _retval)
{
  return mResult->CanUpdate(_retval);
}

NS_IMETHODIMP
mozSqlResultEnumerator::CanDelete(PRBool* _retval)
{
  return mResult->CanDelete(_retval);
}

NS_IMETHODIMP
mozSqlResultEnumerator::InsertRow(PRInt32* _retval)
{
  return mResult->InsertRow(mBuffer, _retval);
}

NS_IMETHODIMP
mozSqlResultEnumerator::UpdateRow(PRInt32* _retval)
{
  return mResult->UpdateRow(mCurrentIndex, mBuffer, _retval);
}

NS_IMETHODIMP
mozSqlResultEnumerator::DeleteRow(PRInt32* _retval)
{
  nsresult rv = mResult->DeleteRow(mCurrentIndex, _retval);
  if (NS_FAILED(rv)) return rv;

  mCurrentIndex = -1;
  mCurrentRow = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultEnumerator::GetCurrentCondition(nsAString& aCurrentCondition)
{
  if (! mCurrentRow)
    return NS_ERROR_FAILURE;

  return mResult->GetCondition(mCurrentRow, aCurrentCondition);
}


NS_IMETHODIMP mozSqlResultEnumerator::HasMoreElements(PRBool* _retval)
{                                                                                               
  *_retval = mCurrentIndex < mResult->mRows.Count() - 1;
  
  return NS_OK;
}

NS_IMETHODIMP mozSqlResultEnumerator::GetNext(nsISupports** _retval)
{
  PRBool hasNext;
  Next(&hasNext);

  NS_ADDREF(*_retval = mCurrentRow->mSource);

  return NS_OK;
}


mozSqlResultStream::mozSqlResultStream(mozSqlResult* aResult)
  : mResult(aResult),
    mInitialized(PR_FALSE),
    mPosition(0)
{
  NS_ADDREF(mResult);
}


mozSqlResultStream::~mozSqlResultStream()
{
  NS_RELEASE(mResult);
}


NS_IMPL_ISUPPORTS2(mozSqlResultStream,
                   mozISqlInputStream,
                   nsIInputStream)


NS_IMETHODIMP
mozSqlResultStream::GetColumnHeader(PRInt32 aColumnIndex, nsAString& aHeader)
{
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultStream::SetColumnHeader(PRInt32 aColumnIndex, const nsAString& aHeader)
{
  return NS_OK;
}


NS_IMETHODIMP
mozSqlResultStream::Close()
{
  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultStream::Available(PRUint32* _retval)
{
  nsresult rv = EnsureBuffer();
  if (NS_FAILED(rv))
    return rv;

  *_retval = mBuffer.Length() - mPosition;

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultStream::Read(char* aBuffer, PRUint32 aCount, PRUint32* _retval)
{
  if (aCount == 0) {
    *_retval = 0;
    return NS_OK;
  }

  nsresult rv = EnsureBuffer();
  if (NS_FAILED(rv))
    return rv;

  if (aCount > mBuffer.Length() - mPosition)
    aCount = mBuffer.Length() - mPosition;

  memcpy(aBuffer, mBuffer.get() + mPosition, aCount);
  mPosition += aCount;

  *_retval = aCount;

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultStream::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure, PRUint32 aCount, PRUint32* _retval)
{
  if (aCount == 0) {
    *_retval = 0;
    return NS_OK;
  }

  nsresult rv = EnsureBuffer();
  if (NS_FAILED(rv))
    return rv;

  if (aCount > mBuffer.Length() - mPosition)
    aCount = mBuffer.Length() - mPosition;

  rv = aWriter(this, aClosure, mBuffer.get() + mPosition, 0, aCount, _retval);
  if (NS_SUCCEEDED(rv));
    mPosition += aCount;

  return NS_OK;
}

NS_IMETHODIMP
mozSqlResultStream::IsNonBlocking(PRBool* _retval)
{
  *_retval = PR_TRUE;
  return NS_OK;
}


nsresult
mozSqlResultStream::EnsureBuffer()
{
  if (!mInitialized) {
    mBuffer.Append(NS_LITERAL_CSTRING("<?xml version=\"1.0\"?>\n"));
    mBuffer.Append(NS_LITERAL_CSTRING("<document>\n<body>\n"));
    PRInt32 rowCount = mResult->mRows.Count();
    PRInt32 columnCount = mResult->mColumnInfo.Count();
    for (PRInt32 i = 0; i < rowCount; i++) {
      mBuffer.Append(NS_LITERAL_CSTRING("<row>\n"));
      Row* row = (Row*)mResult->mRows[i];
      for (PRInt32 j = 0; j < columnCount; j++) {
        mBuffer.Append(NS_LITERAL_CSTRING("<cell>\n"));
        Cell* cell = row->mCells[j];
        if (cell->IsNull())
          mBuffer.Append(NS_LITERAL_CSTRING("null"));
        else {
          PRInt32 type = cell->GetType();
          if (type == mozISqlResult::TYPE_STRING)
            mBuffer.Append(NS_ConvertUCS2toUTF8(cell->mString));
          else if (type == mozISqlResult::TYPE_INT)
            mBuffer.AppendInt(cell->mInt);
          else if (type == mozISqlResult::TYPE_FLOAT ||
                   type == mozISqlResult::TYPE_DECIMAL)
            mBuffer.AppendFloat(cell->mFloat);
          else if (type == mozISqlResult::TYPE_DATE ||
                   type == mozISqlResult::TYPE_TIME ||
                   type == mozISqlResult::TYPE_DATETIME) {
            nsAutoString value;
            mozSqlResult::gFormat->FormatPRTime(nsnull,
                                  type != mozISqlResult::TYPE_TIME ? kDateFormatLong : kDateFormatNone,
                                  type != mozISqlResult::TYPE_DATE ? kTimeFormatSeconds : kTimeFormatNone,
                                  PRTime(cell->mDate),
                                  value);
            mBuffer.Append(NS_ConvertUCS2toUTF8(value));
          }
          else if (type == mozISqlResult::TYPE_BOOL) {
            if (cell->mBool)
              mBuffer.Append(NS_LITERAL_CSTRING("true"));
            else
              mBuffer.Append(NS_LITERAL_CSTRING("false"));
          }
        }
        mBuffer.Append(NS_LITERAL_CSTRING("</cell>\n"));
      }
      mBuffer.Append(NS_LITERAL_CSTRING("</row>\n"));
    }
    mBuffer.Append(NS_LITERAL_CSTRING("</body>\n</document>\n"));

    mInitialized = PR_TRUE;
  }

  return NS_OK;
}

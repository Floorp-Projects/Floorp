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

#ifndef mozSqlResult_h
#define mozSqlResult_h                                                  

#include "nsCRT.h"
#include "nsFixedSizeAllocator.h"
#include "nsVoidArray.h"
#include "nsCOMArray.h"
#include "nsHashtable.h"
#include "nsCOMPtr.h"
#include "nsWeakReference.h"
#include "nsISimpleEnumerator.h"
#include "nsIRDFService.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsITreeView.h"
#include "nsITreeSelection.h"
#include "nsITreeBoxObject.h"
#include "nsIDateTimeFormat.h"
#include "nsIInputStream.h"
#include "mozISqlConnection.h"
#include "mozISqlDataSource.h"
#include "mozISqlResult.h"
#include "mozISqlResultEnumerator.h"
#include "mozISqlInputStream.h"

#define CELL_FLAG_NULL          0x80
#define CELL_FLAG_DEFAULT       0x40
#define CELL_FLAG_MASK          ~(CELL_FLAG_NULL | CELL_FLAG_DEFAULT)

class ColumnInfo {
  public:
    static ColumnInfo*
    Create(nsFixedSizeAllocator& aAllocator,
           PRUnichar* aName,
	   PRInt32 aType,
	   PRInt32 aSize,
	   PRInt32 aMod,
           nsIRDFResource* aProperty) {
      void* place = aAllocator.Alloc(sizeof(ColumnInfo));
      return place ? ::new(place) ColumnInfo(aName, aType, aSize, aMod, aProperty) : nsnull;
    }

    static void
    Destroy(nsFixedSizeAllocator& aAllocator, ColumnInfo* aColumnInfo) {
      aColumnInfo->~ColumnInfo();
      aAllocator.Free(aColumnInfo, sizeof(ColumnInfo));
    }

    ColumnInfo(PRUnichar* aName, PRInt32 aType, PRInt32 aSize, PRInt32 aMod, nsIRDFResource* aProperty)
      : mName(aName),
        mType(aType),
        mSize(aSize),
        mMod(aMod),
	mProperty(aProperty) {
      NS_IF_ADDREF(mProperty);
    }

    ~ColumnInfo() {
      if (mName)
        nsMemory::Free(mName);
      NS_IF_RELEASE(mProperty);
    }

    PRUnichar*          mName;
    PRInt32             mType;
    PRInt32             mSize;
    PRInt32             mMod;
    nsIRDFResource*     mProperty;

  private:
    // Hide so that only Create() and Destroy() can be used to
    // allocate and deallocate from the heap
    static void* operator new(size_t) CPP_THROW_NEW { return 0; } 
    static void operator delete(void*, size_t) {}
};

class Cell {
  public:
    static Cell*
    Create(nsFixedSizeAllocator& aAllocator,
           PRInt32 aType) {
      void* place = aAllocator.Alloc(sizeof(Cell));
      return place ? ::new(place) Cell(aType) : nsnull;
    }

    static Cell*
    Create(nsFixedSizeAllocator& aAllocator,
           PRInt32 aType,
           Cell* aSrcCell) {
      void* place = aAllocator.Alloc(sizeof(Cell));
      if (! place)
        return nsnull;
      Cell* newCell = ::new(place) Cell(aType);
      Copy(aSrcCell, newCell);
      return newCell;
    }

    static void
    Copy(Cell* aSrcCell, Cell* aDestCell) {
      if (aSrcCell->IsNull())
        aDestCell->SetNull(PR_TRUE);
      else {
        aDestCell->SetNull(PR_FALSE);
        PRInt32 type = aSrcCell->GetType();
        if (type == mozISqlResult::TYPE_STRING)
          aDestCell->SetString(nsCRT::strdup(aSrcCell->mString));
        else if (type == mozISqlResult::TYPE_INT)
          aDestCell->mInt = aSrcCell->mInt;
        else if (type == mozISqlResult::TYPE_FLOAT ||
                 type == mozISqlResult::TYPE_DECIMAL)
          aDestCell->mFloat = aSrcCell->mFloat;
        else if (type == mozISqlResult::TYPE_DATE ||
                 type == mozISqlResult::TYPE_TIME ||
                 type == mozISqlResult::TYPE_DATETIME)
          aDestCell->mDate = aSrcCell->mDate;
        else if (type == mozISqlResult::TYPE_BOOL)
          aDestCell->mBool = aSrcCell->mBool;
      }
    }

    static void
    Destroy(nsFixedSizeAllocator& aAllocator, Cell* aCell) {
      aCell->~Cell();
      aAllocator.Free(aCell, sizeof(Cell));
    }

    Cell(PRInt32 aType)
      : mString(nsnull),
        mType(aType | CELL_FLAG_NULL) {
    }

    ~Cell() {
      if ((GetType() == mozISqlResult::TYPE_STRING) && mString) {
        nsMemory::Free(mString);
      }
    }

    void SetString(PRUnichar* aString) {
     if (mString)
        nsMemory::Free(mString);
      mString = aString;
    }

    PRInt32 GetType() {
      return mType & CELL_FLAG_MASK;
    }

    void SetNull(PRBool aNull) {
      mType &= CELL_FLAG_MASK;
      if (aNull)
        mType |= CELL_FLAG_NULL;
    }

    void SetDefault(PRBool aDefault) {
      mType &= CELL_FLAG_MASK;
      if (aDefault)
        mType |= CELL_FLAG_DEFAULT;
    }

    PRBool IsNull() {
      return mType & CELL_FLAG_NULL;
    }

    PRBool IsDefault() {
      return mType & CELL_FLAG_DEFAULT;
    }

    union {
      PRUnichar*        mString;
      PRInt32           mInt;
      float             mFloat;
      PRInt64           mDate;
      PRBool            mBool;
    };

  private:
    static void* operator new(size_t) CPP_THROW_NEW { return 0; } 
    static void operator delete(void*, size_t) {}

    PRInt8              mType;
};

class Row {
  public:
    static Row*
    Create(nsFixedSizeAllocator& aAllocator,
           nsIRDFResource* aSource,
           nsVoidArray& aColumnInfo) {
      void* place = aAllocator.Alloc(sizeof(Row));
      if (! place)
        return nsnull;
      Row* newRow = ::new(place) Row(aSource, aColumnInfo.Count());
      for (PRInt32 i = 0; i < aColumnInfo.Count(); i++) {
        Cell* newCell = Cell::Create(aAllocator, ((ColumnInfo*)aColumnInfo[i])->mType);
        newRow->mCells[i] = newCell;
      }
      return newRow;
    }

    static Row*
    Create(nsFixedSizeAllocator& aAllocator,
           nsIRDFResource* aSource,
           nsVoidArray& aColumnInfo,
           Row* aSrcRow) {
      void* place = aAllocator.Alloc(sizeof(Row));
      if (! place)
        return nsnull;
      Row* newRow = ::new(place) Row(aSource, aColumnInfo.Count());
      for (PRInt32 i = 0; i < aColumnInfo.Count(); i++) {
        Cell* srcCell = aSrcRow->mCells[i];
        Cell* newCell = Cell::Create(aAllocator, ((ColumnInfo*)aColumnInfo[i])->mType, srcCell);
        newRow->mCells[i] = newCell;
      }
      return newRow;
    }

    static void
    Copy(PRInt32 aColumnCount, Row* aSrcRow, Row* aDestRow) {
      for (PRInt32 i = 0; i < aColumnCount; i++) {
        Cell* srcCell = aSrcRow->mCells[i];
        Cell* destCell = aDestRow->mCells[i];
        Cell::Copy(srcCell, destCell);
      }
    }

    static void
    Destroy(nsFixedSizeAllocator& aAllocator, PRInt32 aColumnCount, Row* aRow) {
      for (PRInt32 i = 0; i < aColumnCount; i++)
        Cell::Destroy(aAllocator, aRow->mCells[i]);
      aRow->~Row();
      aAllocator.Free(aRow, sizeof(*aRow));
    }

    Row(nsIRDFResource* aSource, PRInt32 aColumnCount)
      : mSource(aSource)
    {
      NS_IF_ADDREF(mSource);
      mCells = new Cell*[aColumnCount];
    }

    ~Row() {
      delete[] mCells;
      NS_IF_RELEASE(mSource);
    }
    
    nsIRDFResource*     mSource;
    Cell**              mCells;
  private:
    static void* operator new(size_t) CPP_THROW_NEW { return 0; } 
    static void operator delete(void*, size_t) {}
};

class mozSqlResult : public mozISqlResult,
                     public mozISqlDataSource,
                     public nsIRDFDataSource,
                     public nsIRDFRemoteDataSource,
                     public nsITreeView
{
  public:
    mozSqlResult(mozISqlConnection* aConnection,
                 const nsAString& aQuery);
    nsresult Init();
    nsresult Rebuild();
    virtual ~mozSqlResult();

    NS_DECL_ISUPPORTS

    //NS_DECL_MOZISQLRESULT
    NS_IMETHOD GetConnection(mozISqlConnection * *aConnection);
    NS_IMETHOD GetQuery(nsAString & aQuery);
    NS_IMETHOD GetTableName(nsAString & aTableName);
    //NS_IMETHOD GetRowCount(PRInt32 *aRowCount);
    NS_IMETHOD GetColumnCount(PRInt32 *aColumnCount);
    NS_IMETHOD GetColumnName(PRInt32 aColumnIndex, nsAString & _retval);
    NS_IMETHOD GetColumnIndex(const nsAString & aColumnName, PRInt32 *_retval);
    NS_IMETHOD GetColumnType(PRInt32 aColumnIndex, PRInt32 *_retval);
    NS_IMETHOD GetColumnTypeAsString(PRInt32 aColumnIndex, nsAString & _retval);
    NS_IMETHOD GetColumnDisplaySize(PRInt32 aColumnIndex, PRInt32 *_retval);
    NS_IMETHOD Enumerate(mozISqlResultEnumerator **_retval);
    NS_IMETHOD Open(mozISqlInputStream **_retval);
    NS_IMETHOD Reload(void);

    NS_DECL_MOZISQLDATASOURCE

    NS_DECL_NSIRDFDATASOURCE

    NS_DECL_NSIRDFREMOTEDATASOURCE

    NS_DECL_NSITREEVIEW

    friend class mozSqlResultEnumerator;
    friend class mozSqlResultStream;

  protected:
    virtual nsresult BuildColumnInfo() = 0 ;
    virtual nsresult BuildRows() = 0;
    virtual void ClearNativeResult() = 0;

    void ClearColumnInfo();
    void ClearRows();

    nsresult EnsureTableName();
    nsresult EnsurePrimaryKeys();

    void AppendValue(Cell* aCell, nsAutoString& aValues);
    nsresult AppendKeys(Row* aRow, nsAutoString& aKeys);
    nsresult GetValues(Row* aRow, mozISqlResult** aResult, PRBool aUseID);
    nsresult CopyValues(mozISqlResult* aResult, Row* aRow);

    virtual nsresult CanInsert(PRBool* _retval) = 0;
    virtual nsresult CanUpdate(PRBool* _retval) = 0;
    virtual nsresult CanDelete(PRBool* _retval) = 0;

    nsresult InsertRow(Row* aSrcRow, PRInt32* _retval);
    nsresult UpdateRow(PRInt32 aRowIndex, Row* aSrcRow, PRInt32* _retval);
    nsresult DeleteRow(PRInt32 aRowIndex, PRInt32* _retval);
    nsresult GetCondition(Row* aRow, nsAString& aCurrentCondition);

    static PRInt32                      gRefCnt;
    static nsIRDFService*               gRDFService;
    static nsIDateTimeFormat*           gFormat;
    static nsIRDFResource*              kSQL_ResultRoot;
    static nsIRDFResource*              kNC_Child;
    static nsIRDFLiteral*               kNullLiteral;
    static nsIRDFLiteral*               kTrueLiteral;
    static nsIRDFLiteral*               kFalseLiteral;

    nsCOMPtr<mozISqlConnection>         mConnection;
    nsString                            mErrorMessage;
    nsString                            mQuery;
    nsString                            mTableName;
    nsFixedSizeAllocator                mAllocator;
    nsAutoVoidArray                     mColumnInfo;
    nsVoidArray                         mRows;
    nsObjectHashtable                   mSources;
    nsCOMArray<nsIRDFObserver>          mObservers;
    nsCOMPtr<nsITreeSelection>          mSelection;
    nsCOMPtr<nsITreeBoxObject>          mBoxObject;
    nsCOMPtr<mozISqlResultEnumerator>   mPrimaryKeys;
    PRInt32                             mCanInsert;
    PRInt32                             mCanUpdate;
    PRInt32                             mCanDelete;
};

class mozSqlResultEnumerator : public mozISqlResultEnumerator,
                               public nsISimpleEnumerator
{
  public:
    mozSqlResultEnumerator(mozSqlResult* aResult);
    virtual ~mozSqlResultEnumerator();

    NS_DECL_ISUPPORTS

    NS_DECL_MOZISQLRESULTENUMERATOR

    NS_DECL_NSISIMPLEENUMERATOR

  private:
    mozSqlResult*               mResult;
    PRInt32                     mCurrentIndex;
    Row*                        mCurrentRow;
    Row*                        mBuffer;
};

class mozSqlResultStream : public mozISqlInputStream,
                           public nsIInputStream
{
  public:
    mozSqlResultStream(mozSqlResult* aResult);
    virtual ~mozSqlResultStream();

    NS_DECL_ISUPPORTS

    NS_DECL_MOZISQLINPUTSTREAM

    NS_DECL_NSIINPUTSTREAM

  protected:
    nsresult EnsureBuffer();

  private:
    mozSqlResult*               mResult;
    nsCAutoString               mBuffer;
    PRBool                      mInitialized;
    PRUint32                    mPosition;
};

#endif // mozSqlResult_h

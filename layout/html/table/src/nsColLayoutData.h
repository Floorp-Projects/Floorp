/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsColLayoutData_h__
#define nsColLayoutData_h__

#include "nscore.h"
#include "nsSize.h"
#include "nsIFrame.h"
#include "nsTableCol.h"

class nsVoidArray;
class nsCellLayoutData;
class nsTableFrame;
class nsTableColFrame;


/** Simple data class that represents in-process reflow information about a column.
  */
class nsColLayoutData
{
public:
  nsColLayoutData(nsTableCol *aCol);

  // NOT VIRTUAL BECAUSE THIS CLASS SHOULD **NEVER** BE SUBCLASSED  
  ~nsColLayoutData();

  nsTableCol * GetCol();

  void SetCol(nsTableCol * aCol);

  nsTableColFrame *GetColFrame();

  void SetColFrame(nsTableColFrame *aColFrame);

  nsVoidArray * GetCells();

  PRInt32 Count() const;
  
  nsCellLayoutData* ElementAt(PRInt32 aIndex) const;

  PRInt32 IndexOf(nsCellLayoutData* aCellLayoutData) const;
  PRInt32 IndexOf(nsTableCell* aTableCell) const;
  
  nsCellLayoutData* GetNext(nsCellLayoutData* aCellLayoutData) const;
  
  nsCellLayoutData* GetPrevious(nsCellLayoutData* aCellLayoutData) const;



  PRBool AppendElement(nsCellLayoutData* aCellLayoutData);

  PRBool ReplaceElementAt(nsCellLayoutData* aCellLayoutData, PRInt32 aIndex);
  
  void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  
private:

  nsTableCol        *mCol;
  nsTableColFrame   *mColFrame;
  nsVoidArray       *mCells;

};

/* ---------- inlines ---------- */

inline nsTableCol * nsColLayoutData::GetCol()
{
  NS_IF_ADDREF(mCol);
  return mCol; 
};

inline void nsColLayoutData::SetCol(nsTableCol * aCol)
{
  if (aCol != mCol)
  {
    NS_IF_ADDREF(aCol);
    NS_IF_RELEASE(mCol);
    mCol = aCol; 
  }
}

inline nsTableColFrame * nsColLayoutData::GetColFrame()
{  return mColFrame;}

inline void nsColLayoutData::SetColFrame(nsTableColFrame *aColFrame)
{  mColFrame = aColFrame;}


inline nsCellLayoutData* nsColLayoutData::ElementAt(PRInt32 aIndex) const
{  return (nsCellLayoutData*)mCells->ElementAt(aIndex);}

inline PRBool nsColLayoutData::AppendElement(nsCellLayoutData* aCellLayoutData)
{  return mCells->AppendElement((void*)aCellLayoutData);}

inline PRBool nsColLayoutData::ReplaceElementAt(nsCellLayoutData* aCellLayoutData, PRInt32 aIndex)
{  return mCells->ReplaceElementAt((void*)aCellLayoutData,aIndex);}

inline nsVoidArray * nsColLayoutData::GetCells()
{ return mCells; }

inline PRInt32 nsColLayoutData::Count() const
{ return mCells->Count(); }


inline PRInt32 nsColLayoutData::IndexOf(nsCellLayoutData* aCellLayoutData) const
{  return mCells->IndexOf((void*)aCellLayoutData);}



#endif

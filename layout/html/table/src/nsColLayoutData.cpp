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
#include "nsColLayoutData.h"
#include "nsTableCol.h"
#include "nsVoidArray.h"
#include "nsCellLayoutData.h"

nsColLayoutData::nsColLayoutData(nsTableCol *aCol)
{
  mCol = aCol;
  mCells = new nsVoidArray();
}

nsColLayoutData::~nsColLayoutData()
{
  if (nsnull!=mCells)
  {
    PRInt32 count = mCells->Count();
    for (PRInt32 i = 0; i < count; i++)
    {
      nsCellLayoutData *data = (nsCellLayoutData *)(mCells->ElementAt(i));
      delete data;
    }
    delete mCells;
  }
  mCells = 0;
}

nsTableCol * nsColLayoutData::GetCol()
{
  NS_IF_ADDREF(mCol);
  return mCol; 
};

void nsColLayoutData::SetCol(nsTableCol * aCol)
{
  if (aCol != mCol)
  {
    NS_IF_ADDREF(aCol);
    NS_IF_RELEASE(mCol);
    mCol = aCol; 
  }
};


nsCellLayoutData* nsColLayoutData::ElementAt(PRInt32 aIndex) const
{
  return (nsCellLayoutData*)mCells->ElementAt(aIndex);
}


PRBool nsColLayoutData::AppendElement(nsCellLayoutData* aCellLayoutData)
{
  return mCells->AppendElement((void*)aCellLayoutData);
}

PRBool nsColLayoutData::ReplaceElementAt(nsCellLayoutData* aCellLayoutData, PRInt32 aIndex)
{
  return mCells->ReplaceElementAt((void*)aCellLayoutData,aIndex);
}

nsVoidArray * nsColLayoutData::GetCells()
{ return mCells; }

PRInt32 nsColLayoutData::Count() const
{ return mCells->Count(); }


PRInt32 nsColLayoutData::IndexOf(nsCellLayoutData* aCellLayoutData) const
{
  return mCells->IndexOf((void*)aCellLayoutData);
}

PRInt32 nsColLayoutData::IndexOf(nsTableCell* aTableCell) const
{
  PRInt32 count = this->Count();
  PRInt32 result = -1;
  if (aTableCell != nsnull)
  {
    for (PRInt32 index = 0; index < count; index++)
    {
      nsCellLayoutData* cellData = ElementAt(index);
      if (cellData != nsnull)
      {
        nsTableCellFrame* frame = cellData->GetCellFrame();
        if (frame != nsnull)
        {
          nsTableCell* cell;
           
          frame->GetContent((nsIContent*&)cell);
          if (cell == aTableCell)
          {
            result = index;
            NS_RELEASE(cell);
            break;
          }
          NS_IF_RELEASE(cell);
        }
      }
    }
  }
  return result;
}


nsCellLayoutData* nsColLayoutData::GetNext(nsCellLayoutData* aCellLayoutData) const
{
  PRInt32 index = IndexOf(aCellLayoutData);
  if (index != -1)
    return ElementAt(index+1);
  return (nsCellLayoutData*)nsnull;
}

nsCellLayoutData* nsColLayoutData::GetPrevious(nsCellLayoutData* aCellLayoutData) const
{
  PRInt32 index = IndexOf(aCellLayoutData);
  if (index != -1)
    return ElementAt(index-1);
  return (nsCellLayoutData*)nsnull;
}


void nsColLayoutData::List(FILE* out, PRInt32 aIndent) const
{
  if (nsnull!=mCells)
  {
    PRInt32 count = mCells->Count();
    for (PRInt32 i = 0; i < count; i++)
    {
      for (PRInt32 indent = aIndent; --indent >= 0; ) fputs("  ", out);
      fprintf(out,"Cell Data [%d] \n",i);

      nsCellLayoutData *data = (nsCellLayoutData *)(mCells->ElementAt(i));
      data->List(out,aIndent+1);
    }
  }
}



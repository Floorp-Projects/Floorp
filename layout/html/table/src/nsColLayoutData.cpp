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
#include "nsVoidArray.h"
#include "nsCellLayoutData.h"
#include "nsTableCell.h"

nsColLayoutData::nsColLayoutData()
{
  mCells = new nsVoidArray();
  mColFrame = nsnull;
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

PRInt32 nsColLayoutData::IndexOf(nsIContent* aCell) const
{
  PRInt32 count = this->Count();
  PRInt32 result = -1;
  if (aCell != nsnull)
  {
    for (PRInt32 index = 0; index < count; index++)
    {
      nsCellLayoutData* cellData = ElementAt(index);
      if (cellData != nsnull)
      {
        nsTableCellFrame* frame = cellData->GetCellFrame();
        if (frame != nsnull)
        {
          nsIContent* cell;
           
          frame->GetContent(cell);
          if (cell == aCell)
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



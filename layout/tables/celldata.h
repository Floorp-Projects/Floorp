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
#ifndef CellData_h__
#define CellData_h__

class nsTableCellFrame;

/** Data stored by nsCellMap to rationalize rowspan and colspan cells.
  * if mOrigCell is null then mSpanCell will be the rowspan/colspan source.
  * if mSpanCell2 is non-null then it will point to a 2nd cell that overlaps this position
  * @see nsCellMap
  * @see nsTableFrame::BuildCellMap
  * @see nsTableFrame::GrowCellMap
  * @see nsTableFrame::BuildCellIntoMap
  * 
  */
class CellData
{
public:
  CellData();
  CellData(nsTableCellFrame* aOrigCell, CellData* aRowSpanData, CellData* aColSpanData)
    : mOrigCell(aOrigCell), mRowSpanData(aRowSpanData), mColSpanData(aColSpanData)
  {}

  ~CellData();

  nsTableCellFrame* mOrigCell;  
  CellData*         mRowSpanData;  
  CellData*         mColSpanData; 

};

#endif

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef CellData_h__
#define CellData_h__

#include "nsISupports.h"

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

#ifndef NS_BUILD_REFCNT_LOGGING
  CellData(nsTableCellFrame* aOrigCell, CellData* aRowSpanData, CellData* aColSpanData)
    : mOrigCell(aOrigCell), mRowSpanData(aRowSpanData), mColSpanData(aColSpanData)
  {
  }
#else
  CellData(nsTableCellFrame* aOrigCell, CellData* aRowSpanData,
           CellData* aColSpanData);
#endif

  ~CellData();

  nsTableCellFrame* mOrigCell;  
  CellData*         mRowSpanData;  
  CellData*         mColSpanData; 

};

#endif

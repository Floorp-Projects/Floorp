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
#ifndef nsCellMap_h__
#define nsCellMap_h__

#include "nscore.h"

class CellData;

/**
  * acts like a 2-dimensional array, so all offsets are 0-indexed
  * to the outside world.
  TODO:  inline methods
  */
class nsCellMap
{
protected:
  /** storage for CellData pointers */
  PRInt32 *mCells;

  /** the number of rows */
  int mRowCount;      // in java, we could just do fCellMap.length;

  /** the number of columns (the max of all row lengths) */
  int mColCount;      // in java, we could just do fCellMap[i].length

public:
  nsCellMap(int aRows, int aColumns);

  // NOT VIRTUAL BECAUSE THIS CLASS SHOULD **NEVER** BE SUBCLASSED  
  ~nsCellMap();

  void Reset(int aRows, int aColumns);

  CellData * GetCellAt(int aRow, int aColumn) const;

  void SetCellAt(CellData *aCell, int aRow, int aColumn);

  void GrowTo(int aColCount);

  int GetColCount() const;

  int GetRowCount() const;

  void DumpCellMap() const;

};

#endif

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- 
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights 
 * Reserved. 
 */

#ifndef _MORKROW_
#define _MORKROW_ 1

#ifndef _MORK_
#include "mork.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

class nsIMdbRow;
class nsIMdbCell;
#define morkDerived_kRow  /*i*/ 0x5277 /* ascii 'Rw' */

#define morkRow_kMaxTableUses 0x0FFFF /* max for 16-bit unsigned int */
#define morkRow_kMaxLength 0x0FFFF /* max for 16-bit unsigned int */
#define morkRow_kMinusOneRid ((mork_rid) -1)

#define morkRow_kTag 'r' /* magic signature for mRow_Tag */


class morkRow { // row of cells

public: // state is public because the entire Mork system is private
  morkRowSpace*   mRow_Space;  // mRow_Space->mSpace_Scope is the row scope 
  morkRowObject*  mRow_Object; // refcount & other state for object sharing
  morkCell*       mRow_Cells;
  mdbOid          mRow_Oid;

  mork_u2         mRow_Length;     // physical count of cells in mRow_Cells 
  mork_u2         mRow_Seed;       // count changes in mRow_Cells structure

  mork_u2         mRow_TableUses;  // persistent references from tables
  mork_load       mRow_Load;       // is this row clean or dirty?
  mork_u1         mRow_Tag;        // one-byte tag (need u4 alignment pad)
  
public: // other row methods
  morkRow( ) { }
  morkRow(const mdbOid* inOid) :mRow_Oid(*inOid) { }
  void InitRow(morkEnv* ev, const mdbOid* inOid, morkRowSpace* ioSpace,
    mork_size inLength, morkPool* ioPool);
    // if inLength is nonzero, cells will be allocated from ioPool

  morkRowObject* GetRowObject(morkEnv* ev, morkStore* ioStore);
  nsIMdbRow* AcquireRowHandle(morkEnv* ev, morkStore* ioStore);
  nsIMdbCell* AcquireCellHandle(morkEnv* ev, morkCell* ioCell,
    mdb_column inColumn, mork_pos inPos);
  
  mork_u2 AddTableUse(morkEnv* ev);
  mork_u2 CutTableUse(morkEnv* ev);

  void SetRowClean() { mRow_Load = morkLoad_kClean; }
  void SetRowDirty() { mRow_Load = morkLoad_kDirty; }
  
  mork_bool IsRowClean() const { return mRow_Load == morkLoad_kClean; }
  mork_bool IsRowDirty() const { return mRow_Load == morkLoad_kDirty; }

public: // internal row methods

  mork_count CountOverlap(morkEnv* ev, morkCell* ioVector, mork_fill inFill);
  // Count cells in ioVector that change existing cells in this row when
  // ioVector is added to the row (as in TakeCells()).   This is the set
  // of cells with the same columns in ioVector and mRow_Cells, which do
  // not have exactly the same value in mCell_Atom, and which do not both
  // have change status equal to morkChange_kCut (because cutting a cut
  // cell still yields a cell that has been cut).  CountOverlap() also
  // modifies the change attribute of any cell in ioVector to kDup when
  // the change was previously kCut and the same column cell was found
  // in this row with change also equal to kCut; this tells callers later
  // they need not look for that cell in the row again on a second pass.

  void MergeCells(morkEnv* ev, morkCell* ioVector,
    mork_fill inVecLength, mork_fill inOldRowFill, mork_fill inOverlap);
  // MergeCells() is the part of TakeCells() that does the insertion.
  // inOldRowFill is the old value of mRow_Length, and inOverlap is the
  // number of cells in the intersection that must be updated.

  void TakeCells(morkEnv* ev, morkCell* ioVector, mork_fill inVecLength,
    morkStore* ioStore);

  morkCell* NewCell(morkEnv* ev, mdb_column inColumn, mork_pos* outPos,
    morkStore* ioStore);
  morkCell* GetCell(morkEnv* ev, mdb_column inColumn, mork_pos* outPos) const;
  morkCell* CellAt(morkEnv* ev, mork_pos inPos) const;

public: // external row methods

  void DirtyAllRowContent(morkEnv* ev);

  morkStore* GetRowSpaceStore(morkEnv* ev) const;

  void AddColumn(morkEnv* ev, mdb_column inColumn,
    const mdbYarn* inYarn, morkStore* ioStore);

  morkRowCellCursor* NewRowCellCursor(morkEnv* ev, mdb_pos inPos);
  
  void EmptyAllCells(morkEnv* ev);
  void AddRow(morkEnv* ev, const morkRow* inSourceRow);

  void OnZeroTableUse(morkEnv* ev);
  // OnZeroTableUse() is called when CutTableUse() returns zero.

public: // dynamic typing

  mork_bool IsRow() const { return mRow_Tag == morkRow_kTag; }

public: // hash and equal

  mork_u4 HashRow() const
  {
    return (mRow_Oid.mOid_Scope << 16) ^ mRow_Oid.mOid_Id;
  }

  mork_u4 EqualRow(const morkRow* ioRow) const
  {
    return
    (
      ( mRow_Oid.mOid_Scope == ioRow->mRow_Oid.mOid_Scope ) 
      && ( mRow_Oid.mOid_Id == ioRow->mRow_Oid.mOid_Id )
    );
  }

  mork_u4 EqualOid(const mdbOid* ioOid) const
  {
    return
    (
      ( mRow_Oid.mOid_Scope == ioOid->mOid_Scope ) 
      && ( mRow_Oid.mOid_Id == ioOid->mOid_Id )
    );
  }

public: // errors
  static void ZeroColumnError(morkEnv* ev);
  static void LengthBeyondMaxError(morkEnv* ev);
  static void NilCellsError(morkEnv* ev);
  static void NonRowTypeError(morkEnv* ev);
  static void NonRowTypeWarning(morkEnv* ev);
  static void TableUsesUnderflowWarning(morkEnv* ev);

private: // copying is not allowed
  morkRow(const morkRow& other);
  morkRow& operator=(const morkRow& other);
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKROW_ */

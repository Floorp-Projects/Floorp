/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- 
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef _MORKROW_
#define _MORKROW_ 1

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKCELL_
#include "morkCell.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

class nsIMdbRow;
class nsIMdbCell;
#define morkDerived_kRow  /*i*/ 0x5277 /* ascii 'Rw' */

#define morkRow_kMaxGcUses 0x0FF /* max for 8-bit unsigned int */
#define morkRow_kMaxLength 0x0FFFF /* max for 16-bit unsigned int */
#define morkRow_kMinusOneRid ((mork_rid) -1)

#define morkRow_kTag 'r' /* magic signature for mRow_Tag */

#define morkRow_kNotedBit   ((mork_u1) (1 << 0)) /* space has change notes */
#define morkRow_kRewriteBit ((mork_u1) (1 << 1)) /* must rewrite all cells */
#define morkRow_kDirtyBit   ((mork_u1) (1 << 2)) /* row has been changed */

class morkRow { // row of cells

public: // state is public because the entire Mork system is private
  morkRowSpace*   mRow_Space;  // mRow_Space->SpaceScope() is the row scope 
  morkRowObject*  mRow_Object; // refcount & other state for object sharing
  morkCell*       mRow_Cells;
  mdbOid          mRow_Oid;
  
  mork_delta      mRow_Delta;   // space to note a single column change

  mork_u2         mRow_Length;     // physical count of cells in mRow_Cells 
  mork_u2         mRow_Seed;       // count changes in mRow_Cells structure

  mork_u1         mRow_GcUses;  // persistent references from tables
  mork_u1         mRow_Pad;     // for u1 alignment
  mork_u1         mRow_Flags;   // one-byte flags slot
  mork_u1         mRow_Tag;     // one-byte tag (need u4 alignment pad)

public: // interpreting mRow_Delta
  
  mork_bool HasRowDelta() const { return ( mRow_Delta != 0 ); }
  
  void ClearRowDelta() { mRow_Delta = 0; }
  
  void SetRowDelta(mork_column inCol, mork_change inChange)
  { morkDelta_Init(mRow_Delta, inCol, inChange); }
  
  mork_column  GetDeltaColumn() const { return morkDelta_Column(mRow_Delta); }
  mork_change  GetDeltaChange() const { return morkDelta_Change(mRow_Delta); }

public: // noting row changes

  void NoteRowSetAll(morkEnv* ev);
  void NoteRowSetCol(morkEnv* ev, mork_column inCol);
  void NoteRowAddCol(morkEnv* ev, mork_column inCol);
  void NoteRowCutCol(morkEnv* ev, mork_column inCol);

public: // flags bit twiddling

  void SetRowNoted() { mRow_Flags |= morkRow_kNotedBit; }
  void SetRowRewrite() { mRow_Flags |= morkRow_kRewriteBit; }
  void SetRowDirty() { mRow_Flags |= morkRow_kDirtyBit; }

  void ClearRowNoted() { mRow_Flags &= (mork_u1) ~morkRow_kNotedBit; }
  void ClearRowRewrite() { mRow_Flags &= (mork_u1) ~morkRow_kRewriteBit; }
  void SetRowClean() { mRow_Flags = 0; mRow_Delta = 0; }
  
  mork_bool IsRowNoted() const
  { return ( mRow_Flags & morkRow_kNotedBit ) != 0; }
  
  mork_bool IsRowRewrite() const
  { return ( mRow_Flags & morkRow_kRewriteBit ) != 0; }
   
  mork_bool IsRowClean() const
  { return ( mRow_Flags & morkRow_kDirtyBit ) == 0; }
  
  mork_bool IsRowDirty() const
  { return ( mRow_Flags & morkRow_kDirtyBit ) != 0; }
  
  mork_bool IsRowUsed() const
  { return mRow_GcUses != 0; }

public: // other row methods
  morkRow( ) { }
  morkRow(const mdbOid* inOid) :mRow_Oid(*inOid) { }
  void InitRow(morkEnv* ev, const mdbOid* inOid, morkRowSpace* ioSpace,
    mork_size inLength, morkPool* ioPool);
    // if inLength is nonzero, cells will be allocated from ioPool

  morkRowObject* AcquireRowObject(morkEnv* ev, morkStore* ioStore);
  nsIMdbRow* AcquireRowHandle(morkEnv* ev, morkStore* ioStore);
  nsIMdbCell* AcquireCellHandle(morkEnv* ev, morkCell* ioCell,
    mdb_column inColumn, mork_pos inPos);
  
  mork_u2 AddRowGcUse(morkEnv* ev);
  mork_u2 CutRowGcUse(morkEnv* ev);

  
  mork_bool MaybeDirtySpaceStoreAndRow();

public: // internal row methods

  void cut_all_index_entries(morkEnv* ev);

  // void cut_cell_from_space_index(morkEnv* ev, morkCell* ioCell);

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

  mork_aid GetCellAtomAid(morkEnv* ev, mdb_column inColumn) const;
  // GetCellAtomAid() finds the cell with column inColumn, and sees if the
  // atom has a token ID, and returns the atom's ID if there is one.  Or
  // else zero is returned if there is no such column, or no atom, or if
  // the atom has no ID to return.  This method is intended to support
  // efficient updating of column indexes for rows in a row space.

public: // external row methods

  void DirtyAllRowContent(morkEnv* ev);

  morkStore* GetRowSpaceStore(morkEnv* ev) const;

  void AddColumn(morkEnv* ev, mdb_column inColumn,
    const mdbYarn* inYarn, morkStore* ioStore);

  morkAtom* GetColumnAtom(morkEnv* ev, mdb_column inColumn);

  void NextColumn(morkEnv* ev, mdb_column* ioColumn, mdbYarn* outYarn);

  void SeekColumn(morkEnv* ev, mdb_pos inPos, 
	  mdb_column* outColumn, mdbYarn* outYarn);

  void CutColumn(morkEnv* ev, mdb_column inColumn);

  morkRowCellCursor* NewRowCellCursor(morkEnv* ev, mdb_pos inPos);
  
  void EmptyAllCells(morkEnv* ev);
  void AddRow(morkEnv* ev, const morkRow* inSourceRow);
  void SetRow(morkEnv* ev, const morkRow* inSourceRow);
  void CutAllColumns(morkEnv* ev);

  void OnZeroRowGcUse(morkEnv* ev);
  // OnZeroRowGcUse() is called when CutRowGcUse() returns zero.

public: // dynamic typing

  mork_bool IsRow() const { return mRow_Tag == morkRow_kTag; }

public: // hash and equal

  mork_u4 HashRow() const
  {
    return (mRow_Oid.mOid_Scope << 16) ^ mRow_Oid.mOid_Id;
  }

  mork_bool EqualRow(const morkRow* ioRow) const
  {
    return
    (
      ( mRow_Oid.mOid_Scope == ioRow->mRow_Oid.mOid_Scope ) 
      && ( mRow_Oid.mOid_Id == ioRow->mRow_Oid.mOid_Id )
    );
  }

  mork_bool EqualOid(const mdbOid* ioOid) const
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
  static void GcUsesUnderflowWarning(morkEnv* ev);

private: // copying is not allowed
  morkRow(const morkRow& other);
  morkRow& operator=(const morkRow& other);
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKROW_ */


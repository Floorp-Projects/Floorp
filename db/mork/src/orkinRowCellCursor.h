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

#ifndef _ORKINROWCELLCURSOR_
#define _ORKINROWCELLCURSOR_ 1

#ifndef _MDB_
#include "mdb.h"
#endif

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

#ifndef _MORKHANDLE_
#include "morkHandle.h"
#endif

#ifndef _MORKROWCELLCURSOR_
#include "morkRowCellCursor.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkMagic_kRowCellCursor 0x52634375 /* ascii 'RcCu' */

/*| orkinRowCellCursor: cursor class for iterating row cells
**|
**|| row: the cursor is associated with a specific row, which can be
**| set to a different row (which resets the position to -1 so the
**| next cell acquired is the first in the row.
**|
**|| NextCell: get the next cell in the row and return its position and
**| a new instance of a nsIMdbCell to represent this next cell.
|*/
class orkinRowCellCursor :
  public morkHandle, public nsIMdbRowCellCursor { // nsIMdbCursor

// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  // virtual void CloseMorkNode(morkEnv* ev); // morkHandle is fine
  virtual ~orkinRowCellCursor(); // morkHandle destructor does everything
  
protected: // construction is protected (use the static Make() method)
  orkinRowCellCursor(morkEnv* ev, // note morkUsage is always morkUsage_kPool
    morkHandleFace* ioFace,    // must not be nil, cookie for this handle
    morkRowCellCursor* ioObject); // must not be nil, the object for this handle
    
  // void CloseHandle(morkEnv* ev); // don't need to specialize closing

private: // copying is not allowed
  orkinRowCellCursor(const morkHandle& other);
  orkinRowCellCursor& operator=(const morkHandle& other);

// public: // dynamic type identification
  // mork_bool IsHandle() const //
  // { return IsNode() && mNode_Derived == morkDerived_kHandle; }
// } ===== end morkNode methods =====

protected: // morkHandle memory management operators
  void* operator new(size_t inSize, morkPool& ioPool, morkZone& ioZone, morkEnv* ev)
  { return ioPool.NewHandle(ev, inSize, &ioZone); }
  
  void* operator new(size_t inSize, morkPool& ioPool, morkEnv* ev)
  { return ioPool.NewHandle(ev, inSize, (morkZone*) 0); }
  
  void* operator new(size_t inSize, morkHandleFace* ioFace)
  { MORK_USED_1(inSize); return ioFace; }
  
  void operator delete(void* ioAddress)
  { morkNode::OnDeleteAssert(ioAddress); }
  // do NOT call delete on morkHandle instances.  They are collected.
  
public: // construction:

  static orkinRowCellCursor* MakeRowCellCursor(morkEnv* ev, 
    morkRowCellCursor* ioObject);

public: // utilities:

  morkEnv* CanUseRowCellCursor(nsIMdbEnv* mev, mork_bool inMutable,
    mdb_err* outErr, morkRow** outRow) const;

public: // type identification
  mork_bool IsOrkinRowCellCursor() const
  { return mHandle_Magic == morkMagic_kRowCellCursor; }

  mork_bool IsOrkinRowCellCursorHandle() const
  { return this->IsHandle() && this->IsOrkinRowCellCursor(); }

// { ===== begin nsIMdbISupports methods =====
  virtual mdb_err AddRef(); // add strong ref with no
  virtual mdb_err Release(); // cut strong ref
// } ===== end nsIMdbObject methods =====

// { ===== begin nsIMdbObject methods =====

  // { ----- begin attribute methods -----
  virtual mdb_err IsFrozenMdbObject(nsIMdbEnv* ev, mdb_bool* outIsReadonly);
  // same as nsIMdbPort::GetIsPortReadonly() when this object is inside a port.
  // } ----- end attribute methods -----

  // { ----- begin factory methods -----
  virtual mdb_err GetMdbFactory(nsIMdbEnv* ev, nsIMdbFactory** acqFactory); 
  // } ----- end factory methods -----

  // { ----- begin ref counting for well-behaved cyclic graphs -----
  virtual mdb_err GetWeakRefCount(nsIMdbEnv* ev, // weak refs
    mdb_count* outCount);  
  virtual mdb_err GetStrongRefCount(nsIMdbEnv* ev, // strong refs
    mdb_count* outCount);

  virtual mdb_err AddWeakRef(nsIMdbEnv* ev);
  virtual mdb_err AddStrongRef(nsIMdbEnv* ev);

  virtual mdb_err CutWeakRef(nsIMdbEnv* ev);
  virtual mdb_err CutStrongRef(nsIMdbEnv* ev);
  
  virtual mdb_err CloseMdbObject(nsIMdbEnv* ev); // called at strong refs zero
  virtual mdb_err IsOpenMdbObject(nsIMdbEnv* ev, mdb_bool* outOpen);
  // } ----- end ref counting -----
  
// } ===== end nsIMdbObject methods =====

// { ===== begin nsIMdbCursor methods =====

  // { ----- begin attribute methods -----
  virtual mdb_err GetCount(nsIMdbEnv* ev, mdb_count* outCount); // readonly
  virtual mdb_err GetSeed(nsIMdbEnv* ev, mdb_seed* outSeed);    // readonly
  
  virtual mdb_err SetPos(nsIMdbEnv* ev, mdb_pos inPos);   // mutable
  virtual mdb_err GetPos(nsIMdbEnv* ev, mdb_pos* outPos);
  
  virtual mdb_err SetDoFailOnSeedOutOfSync(nsIMdbEnv* ev, mdb_bool inFail);
  virtual mdb_err GetDoFailOnSeedOutOfSync(nsIMdbEnv* ev, mdb_bool* outFail);
  // } ----- end attribute methods -----

// } ===== end nsIMdbCursor methods =====

// { ===== begin nsIMdbRowCellCursor methods =====

  // { ----- begin attribute methods -----
  virtual mdb_err SetRow(nsIMdbEnv* ev, nsIMdbRow* ioRow); // sets pos to -1
  virtual mdb_err GetRow(nsIMdbEnv* ev, nsIMdbRow** acqRow);
  // } ----- end attribute methods -----

  // { ----- begin cell creation methods -----
  virtual mdb_err MakeCell( // get cell at current pos in the row
    nsIMdbEnv* ev, // context
    mdb_column* outColumn, // column for this particular cell
    mdb_pos* outPos, // position of cell in row sequence
    nsIMdbCell** acqCell); // the cell at inPos
  // } ----- end cell creation methods -----

  // { ----- begin cell seeking methods -----
  virtual mdb_err SeekCell( // same as SetRow() followed by MakeCell()
    nsIMdbEnv* ev, // context
    mdb_pos inPos, // position of cell in row sequence
    mdb_column* outColumn, // column for this particular cell
    nsIMdbCell** acqCell); // the cell at inPos
  // } ----- end cell seeking methods -----

  // { ----- begin cell iteration methods -----
  virtual mdb_err NextCell( // get next cell in the row
    nsIMdbEnv* ev, // context
    nsIMdbCell* ioCell, // changes to the next cell in the iteration
    mdb_column* outColumn, // column for this particular cell
    mdb_pos* outPos); // position of cell in row sequence
    
  virtual mdb_err PickNextCell( // get next cell in row within filter set
    nsIMdbEnv* ev, // context
    nsIMdbCell* ioCell, // changes to the next cell in the iteration
    const mdbColumnSet* inFilterSet, // col set of actual caller interest
    mdb_column* outColumn, // column for this particular cell
    mdb_pos* outPos); // position of cell in row sequence

  // Note that inFilterSet should not have too many (many more than 10?)
  // cols, since this might imply a potential excessive consumption of time
  // over many cursor calls when looking for column and filter intersection.
  // } ----- end cell iteration methods -----

// } ===== end nsIMdbRowCellCursor methods =====
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _ORKINROWCELLCURSOR_ */

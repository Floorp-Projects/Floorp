/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-  */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _ORKINROW_
#define _ORKINROW_ 1

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

#ifndef _MORKROW_
#include "morkRow.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkMagic_kRow 0x526F774D /* ascii 'RowM' */

/*| orkinRow: a collection of cells
**|
|*/
class orkinRow : public morkHandle, public nsIMdbRow { // nsIMdbCollection

// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  // virtual void CloseMorkNode(morkEnv* ev); // morkHandle is fine
  virtual ~orkinRow(); // morkHandle destructor does everything
  
protected: // construction is protected (use the static Make() method)
  orkinRow(morkEnv* ev, // note morkUsage is always morkUsage_kPool
    morkHandleFace* ioFace,    // must not be nil, cookie for this handle
    morkRowObject* ioObject); // must not be nil, the object for this handle
    
  // void CloseHandle(morkEnv* ev); // don't need to specialize closing

private: // copying is not allowed
  orkinRow(const morkHandle& other);
  orkinRow& operator=(const morkHandle& other);

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

  static orkinRow* MakeRow(morkEnv* ev, morkRowObject* ioObject);

public: // utilities:

  morkEnv* CanUseRow(nsIMdbEnv* mev, mork_bool inMutable,
    mdb_err* outErr, morkRow** outRow) const;

  morkStore* CanUseRowStore(morkEnv* ev) const;

public: // type identification
  mork_bool IsOrkinRow() const
  { return mHandle_Magic == morkMagic_kRow; }

  mork_bool IsOrkinRowHandle() const
  { return this->IsHandle() && this->IsOrkinRow(); }

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

// { ===== begin nsIMdbCollection methods =====

  // { ----- begin attribute methods -----
  virtual mdb_err GetSeed(nsIMdbEnv* ev,
    mdb_seed* outSeed);    // member change count
  virtual mdb_err GetCount(nsIMdbEnv* ev,
    mdb_count* outCount); // member count

  virtual mdb_err GetPort(nsIMdbEnv* ev,
    nsIMdbPort** acqPort); // collection container
  // } ----- end attribute methods -----

  // { ----- begin cursor methods -----
  virtual mdb_err GetCursor( // make a cursor starting iter at inMemberPos
    nsIMdbEnv* ev, // context
    mdb_pos inMemberPos, // zero-based ordinal pos of member in collection
    nsIMdbCursor** acqCursor); // acquire new cursor instance
  // } ----- end cursor methods -----

  // { ----- begin ID methods -----
  virtual mdb_err GetOid(nsIMdbEnv* ev,
    mdbOid* outOid); // read object identity
  virtual mdb_err BecomeContent(nsIMdbEnv* ev,
    const mdbOid* inOid); // exchange content
  // } ----- end ID methods -----

  // { ----- begin activity dropping methods -----
  virtual mdb_err DropActivity( // tell collection usage no longer expected
    nsIMdbEnv* ev);
  // } ----- end activity dropping methods -----

// } ===== end nsIMdbCollection methods =====

// { ===== begin nsIMdbRow methods =====

  // { ----- begin cursor methods -----
  virtual mdb_err GetRowCellCursor( // make a cursor starting iteration at inRowPos
    nsIMdbEnv* ev, // context
    mdb_pos inRowPos, // zero-based ordinal position of row in table
    nsIMdbRowCellCursor** acqCursor); // acquire new cursor instance
  // } ----- end cursor methods -----

  // { ----- begin column methods -----
  virtual mdb_err AddColumn( // make sure a particular column is inside row
    nsIMdbEnv* ev, // context
    mdb_column inColumn, // column to add
    const mdbYarn* inYarn); // cell value to install

  virtual mdb_err CutColumn( // make sure a column is absent from the row
    nsIMdbEnv* ev, // context
    mdb_column inColumn); // column to ensure absent from row

  virtual mdb_err CutAllColumns( // remove all columns from the row
    nsIMdbEnv* ev); // context
  // } ----- end column methods -----

  // { ----- begin cell methods -----
  virtual mdb_err NewCell( // get cell for specified column, or add new one
    nsIMdbEnv* ev, // context
    mdb_column inColumn, // column to add
    nsIMdbCell** acqCell); // cell column and value
    
  virtual mdb_err AddCell( // copy a cell from another row to this row
    nsIMdbEnv* ev, // context
    const nsIMdbCell* inCell); // cell column and value
    
  virtual mdb_err GetCell( // find a cell in this row
    nsIMdbEnv* ev, // context
    mdb_column inColumn, // column to find
    nsIMdbCell** acqCell); // cell for specified column, or null
    
  virtual mdb_err EmptyAllCells( // make all cells in row empty of content
    nsIMdbEnv* ev); // context
  // } ----- end cell methods -----

  // { ----- begin row methods -----
  virtual mdb_err AddRow( // add all cells in another row to this one
    nsIMdbEnv* ev, // context
    nsIMdbRow* ioSourceRow); // row to union with
    
  virtual mdb_err SetRow( // make exact duplicate of another row
    nsIMdbEnv* ev, // context
    nsIMdbRow* ioSourceRow); // row to duplicate
  // } ----- end row methods -----

  // { ----- begin blob methods -----  
  virtual mdb_err SetCellYarn(nsIMdbEnv* ev, // synonym for AddColumn()
    mdb_column inColumn, // column to write
    const mdbYarn* inYarn);   // reads from yarn slots
  // make this text object contain content from the yarn's buffer
  
  virtual mdb_err GetCellYarn(nsIMdbEnv* ev, 
    mdb_column inColumn, // column to read 
    mdbYarn* outYarn);  // writes some yarn slots 
  // copy content into the yarn buffer, and update mYarn_Fill and mYarn_Form
  
  virtual mdb_err AliasCellYarn(nsIMdbEnv* ev, 
    mdb_column inColumn, // column to alias
    mdbYarn* outYarn); // writes ALL yarn slots
  
  virtual mdb_err NextCellYarn(nsIMdbEnv* ev, // iterative version of GetCellYarn()
    mdb_column* ioColumn, // next column to read
    mdbYarn* outYarn);  // writes some yarn slots 
  // copy content into the yarn buffer, and update mYarn_Fill and mYarn_Form
  //
  // The ioColumn argument is an inout parameter which initially contains the
  // last column accessed and returns the next column corresponding to the
  // content read into the yarn.  Callers should start with a zero column
  // value to say 'no previous column', which causes the first column to be
  // read.  Then the value returned in ioColumn is perfect for the next call
  // to NextCellYarn(), since it will then be the previous column accessed.
  // Callers need only examine the column token returned to see which cell
  // in the row is being read into the yarn.  When no more columns remain,
  // and the iteration has ended, ioColumn will return a zero token again.
  // So iterating over cells starts and ends with a zero column token.

  virtual mdb_err SeekCellYarn( // resembles nsIMdbRowCellCursor::SeekCell()
    nsIMdbEnv* ev, // context
    mdb_pos inPos, // position of cell in row sequence
    mdb_column* outColumn, // column for this particular cell
    mdbYarn* outYarn); // writes some yarn slots
  // copy content into the yarn buffer, and update mYarn_Fill and mYarn_Form
  // Callers can pass nil for outYarn to indicate no interest in content, so
  // only the outColumn value is returned.  NOTE to subclasses: you must be
  // able to ignore outYarn when the pointer is nil; please do not crash.

  // } ----- end blob methods -----

// } ===== end nsIMdbRow methods =====
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _ORKINROW_ */


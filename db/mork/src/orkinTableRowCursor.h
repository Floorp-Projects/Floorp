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

#ifndef _ORKINTABLEROWCURSOR_
#define _ORKINTABLEROWCURSOR_ 1

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

#ifndef _MORKTABLEROWCURSOR_
#include "morkTableRowCursor.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

class morkTableRowCursor;
#define morkMagic_kTableRowCursor 0x54724375 /* ascii 'TrCu' */

/*| orkinTableRowCursor:
|*/
class orkinTableRowCursor :
  public morkHandle, public nsIMdbTableRowCursor { // nsIMdbCursor

// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  // virtual void CloseMorkNode(morkEnv* ev); // morkHandle is fine
  virtual ~orkinTableRowCursor(); // morkHandle destructor does everything
  
protected: // construction is protected (use the static Make() method)
  orkinTableRowCursor(morkEnv* ev, // note morkUsage is always morkUsage_kPool
    morkHandleFace* ioFace,    // must not be nil, cookie for this handle
    morkTableRowCursor* ioObject); // must not be nil, object for this handle
    
  // void CloseHandle(morkEnv* ev); // don't need to specialize closing

private: // copying is not allowed
  orkinTableRowCursor(const morkHandle& other);
  orkinTableRowCursor& operator=(const morkHandle& other);

// public: // dynamic type identification
  // mork_bool IsHandle() const //
  // { return IsNode() && mNode_Derived == morkDerived_kHandle; }
// } ===== end morkNode methods =====

protected: // morkHandle memory management operators
  void* operator new(size_t inSize, morkPool& ioPool, morkEnv* ev)
  { return ioPool.NewHandle(ev, inSize); }
  
  void* operator new(size_t inSize, morkHandleFace* ioFace)
  { return ioFace; }
  
  void operator delete(void* ioAddress)
  { morkNode::OnDeleteAssert(ioAddress); }
  // do NOT call delete on morkHandle instances.  They are collected.
  
public: // construction:

  static orkinTableRowCursor* MakeTableRowCursor(morkEnv* ev, 
    morkTableRowCursor* ioObject);

public: // utilities:

  morkEnv* CanUseTableRowCursor(nsIMdbEnv* mev, mork_bool inMutable,
    mdb_err* outErr) const;

public: // type identification
  mork_bool IsOrkinTableRowCursor() const
  { return mHandle_Magic == morkMagic_kTableRowCursor; }

  mork_bool IsOrkinTableRowCursorHandle() const
  { return this->IsHandle() && this->IsOrkinTableRowCursor(); }

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

// { ===== begin nsIMdbTableRowCursor methods =====

  // { ----- begin attribute methods -----
  virtual mdb_err SetTable(nsIMdbEnv* ev, nsIMdbTable* ioTable); // sets pos to -1
  virtual mdb_err GetTable(nsIMdbEnv* ev, nsIMdbTable** acqTable);
  // } ----- end attribute methods -----

  // { ----- begin oid iteration methods -----
  virtual mdb_err NextRowOid( // get row id of next row in the table
    nsIMdbEnv* ev, // context
    mdbOid* outOid, // out row oid
    mdb_pos* outRowPos);    // zero-based position of the row in table
  // } ----- end oid iteration methods -----

  // { ----- begin row iteration methods -----
  virtual mdb_err NextRow( // get row cells from table for cells already in row
    nsIMdbEnv* ev, // context
    nsIMdbRow** acqRow, // acquire next row in table
    mdb_pos* outRowPos);    // zero-based position of the row in table
  // } ----- end row iteration methods -----

  // { ----- begin copy iteration methods -----
  virtual mdb_err NextRowCopy( // put row cells into sink only when already in sink
    nsIMdbEnv* ev, // context
    nsIMdbRow* ioSinkRow, // sink for row cells read from next row
    mdbOid* outOid, // out row oid
    mdb_pos* outRowPos);    // zero-based position of the row in table

  virtual mdb_err NextRowCopyAll( // put all row cells into sink, adding to sink
    nsIMdbEnv* ev, // context
    nsIMdbRow* ioSinkRow, // sink for row cells read from next row
    mdbOid* outOid, // out row oid
    mdb_pos* outRowPos);    // zero-based position of the row in table
  // } ----- end copy iteration methods -----

// } ===== end nsIMdbTableRowCursor methods =====
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _ORKINTABLEROWCURSOR_ */

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

#ifndef _ORKINCELL_
#define _ORKINCELL_ 1

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

#ifndef _MORKCELL_
#include "morkCell.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkMagic_kCell 0x43656C6C /* ascii 'Cell' */

class orkinCell : public morkHandle, public nsIMdbCell { // nsIMdbBlob

// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  // virtual void CloseMorkNode(morkEnv* ev); // morkHandle is fine
  virtual ~orkinCell(); // morkHandle destructor does everything
  
protected: // construction is protected (use the static Make() method)
  orkinCell(morkEnv* ev, // note morkUsage is always morkUsage_kPool
    morkHandleFace* ioFace,    // must not be nil, cookie for this handle
    morkCellObject* ioObject); // must not be nil, the object for this handle
    
  // void CloseHandle(morkEnv* ev); // don't need to specialize closing

private: // copying is not allowed
  orkinCell(const morkHandle& other);
  orkinCell& operator=(const morkHandle& other);

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

  static orkinCell* MakeCell(morkEnv* ev, morkCellObject* ioObject);

public: // utilities:

  // ResyncWithRow() moved to the morkCellObject class:
  // mork_bool ResyncWithRow(morkEnv* ev); // return ev->Good()

  morkEnv* CanUseCell(nsIMdbEnv* ev, mork_bool inMutable,
    mdb_err* outErr, morkCell** outCell) const;

public: // type identification
  mork_bool IsOrkinCell() const
  { return mHandle_Magic == morkMagic_kCell; }

  mork_bool IsOrkinCellHandle() const
  { return this->IsHandle() && this->IsOrkinCell(); }

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

// { ===== begin nsIMdbBlob methods =====

  // { ----- begin attribute methods -----
  virtual mdb_err SetBlob(nsIMdbEnv* ev,
    nsIMdbBlob* ioBlob); // reads inBlob slots
  // when inBlob is in the same suite, this might be fastest cell-to-cell
  
  virtual mdb_err ClearBlob( // make empty (so content has zero length)
    nsIMdbEnv* ev);
  // clearing a yarn is like SetYarn() with empty yarn instance content
  
  virtual mdb_err GetBlobFill(nsIMdbEnv* ev,
    mdb_fill* outFill);  // size of blob 
  // Same value that would be put into mYarn_Fill, if one called GetYarn()
  // with a yarn instance that had mYarn_Buf==nil and mYarn_Size==0.
  
  virtual mdb_err SetYarn(nsIMdbEnv* ev, 
    const mdbYarn* inYarn);   // reads from yarn slots
  // make this text object contain content from the yarn's buffer
  
  virtual mdb_err GetYarn(nsIMdbEnv* ev, 
    mdbYarn* outYarn);  // writes some yarn slots 
  // copy content into the yarn buffer, and update mYarn_Fill and mYarn_Form
  
  virtual mdb_err AliasYarn(nsIMdbEnv* ev, 
    mdbYarn* outYarn); // writes ALL yarn slots
  // AliasYarn() reveals sensitive internal text buffer state to the caller
  // by setting mYarn_Buf to point into the guts of this text implementation.
  //
  // The caller must take great care to avoid writing on this space, and to
  // avoid calling any method that would cause the state of this text object
  // to change (say by directly or indirectly setting the text to hold more
  // content that might grow the size of the buffer and free the old buffer).
  // In particular, callers should scrupulously avoid making calls into the
  // mdb interface to write any content while using the buffer pointer found
  // in the returned yarn instance.  Best safe usage involves copying content
  // into some other kind of external content representation beyond mdb.
  //
  // (The original design of this method a week earlier included the concept
  // of very fast and efficient cooperative locking via a pointer to some lock
  // member slot.  But let's ignore that complexity in the current design.)
  //
  // AliasYarn() is specifically intended as the first step in transferring
  // content from nsIMdbBlob to a nsString representation, without forcing extra
  // allocations and/or memory copies. (A standard nsIMdbBlob_AsString() utility
  // will use AliasYarn() as the first step in setting a nsString instance.)
  //
  // This is an alternative to the GetYarn() method, which has copy semantics
  // only; AliasYarn() relaxes a robust safety principle only for performance
  // reasons, to accomodate the need for callers to transform text content to
  // some other canonical representation that would necessitate an additional
  // copy and transformation when such is incompatible with the mdbYarn format.
  //
  // The implementation of AliasYarn() should have extremely little overhead
  // besides the virtual dispatch to the method implementation, and the code
  // necessary to populate all the mdbYarn member slots with internal buffer
  // address and metainformation that describes the buffer content.  Note that
  // mYarn_Grow must always be set to nil to indicate no resizing is allowed.
  
  // } ----- end attribute methods -----

// } ===== end nsIMdbBlob methods =====

// { ===== begin nsIMdbCell methods =====

  // { ----- begin attribute methods -----
  virtual mdb_err SetColumn(nsIMdbEnv* ev, mdb_column inColumn); 
  virtual mdb_err GetColumn(nsIMdbEnv* ev, mdb_column* outColumn);
  
  virtual mdb_err GetCellInfo(  // all cell metainfo except actual content
    nsIMdbEnv* ev, 
    mdb_column* outColumn,           // the column in the containing row
    mdb_fill*   outBlobFill,         // the size of text content in bytes
    mdbOid*     outChildOid,         // oid of possible row or table child
    mdb_bool*   outIsRowChild);  // nonzero if child, and a row child

  // Checking all cell metainfo is a good way to avoid forcing a large cell
  // in to memory when you don't actually want to use the content.
  
  virtual mdb_err GetRow(nsIMdbEnv* ev, // parent row for this cell
    nsIMdbRow** acqRow);
  virtual mdb_err GetPort(nsIMdbEnv* ev, // port containing cell
    nsIMdbPort** acqPort);
  // } ----- end attribute methods -----

  // { ----- begin children methods -----
  virtual mdb_err HasAnyChild( // does cell have a child instead of text?
    nsIMdbEnv* ev,
    mdbOid* outOid,  // out id of row or table (or unbound if no child)
    mdb_bool* outIsRow); // nonzero if child is a row (rather than a table)

  virtual mdb_err GetAnyChild( // access table of specific attribute
    nsIMdbEnv* ev, // context
    nsIMdbRow** acqRow, // child row (or null)
    nsIMdbTable** acqTable); // child table (or null)


  virtual mdb_err SetChildRow( // access table of specific attribute
    nsIMdbEnv* ev, // context
    nsIMdbRow* ioRow); // inRow must be bound inside this same db port

  virtual mdb_err GetChildRow( // access row of specific attribute
    nsIMdbEnv* ev, // context
    nsIMdbRow** acqRow); // acquire child row (or nil if no child)


  virtual mdb_err SetChildTable( // access table of specific attribute
    nsIMdbEnv* ev, // context
    nsIMdbTable* inTable); // table must be bound inside this same db port

  virtual mdb_err GetChildTable( // access table of specific attribute
    nsIMdbEnv* ev, // context
    nsIMdbTable** acqTable); // acquire child table (or nil if no child)
  // } ----- end children methods -----

// } ===== end nsIMdbCell methods =====
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _ORKINCELL_ */

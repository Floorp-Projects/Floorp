/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-  */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#ifndef _ORKINSORTING_
#define _ORKINSORTING_ 1

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

#ifndef _MORKSORTING_
#include "morkSorting.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkMagic_kSorting 0x536F7274 /* ascii 'Sort' */

/*| orkinSorting: 
|*/
class orkinSorting : public morkHandle, public nsIMdbSorting { // nsIMdbSorting

// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  // virtual void CloseMorkNode(morkEnv* ev); // morkHandle is fine
  virtual ~orkinSorting(); // morkHandle destructor does everything
  
protected: // construction is protected (use the static Make() method)
  orkinSorting(morkEnv* ev, // note morkUsage is always morkUsage_kPool
    morkHandleFace* ioFace,    // must not be nil, cookie for this handle
    morkSorting* ioObject); // must not be nil, the object for this handle
    
  // void CloseHandle(morkEnv* ev); // don't need to specialize closing

private: // copying is not allowed
  orkinSorting(const orkinSorting& other);
  orkinSorting& operator=(const orkinSorting& other);

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

  static orkinSorting* MakeSorting(morkEnv* ev, morkSorting* ioObject);

public: // utilities:

  morkEnv* CanUseSorting(nsIMdbEnv* mev, mork_bool inMutable,
    mdb_err* outErr) const;

public: // type identification
  mork_bool IsOrkinSorting() const
  { return mHandle_Magic == morkMagic_kSorting; }

  mork_bool IsOrkinSortingHandle() const
  { return this->IsHandle() && this->IsOrkinSorting(); }

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

// { ===== begin nsIMdbSorting methods =====

  // { ----- begin attribute methods -----
  // sorting: note all rows are assumed sorted by row ID as a secondary
  // sort following the primary column sort, when table rows are sorted.
  
  virtual mdb_err GetTable(nsIMdbEnv* ev, nsIMdbTable** acqTable);
  virtual mdb_err GetSortColumn( // query which col is currently sorted
    nsIMdbEnv* ev, // context
    mdb_column* outColumn); // col the table uses for sorting (or zero)

  virtual mdb_err SetNewCompare(nsIMdbEnv* ev,
    nsIMdbCompare* ioNewCompare);
    // Setting the sorting's compare object will typically cause the rows
    // to be resorted, presumably in a lazy fashion when the sorting is
    // next required to be in a valid row ordering state, such as when a
    // call to PosToOid() happens.  ioNewCompare can be nil, in which case
    // implementations should revert to the default sort order, which must
    // be equivalent to whatever is used by nsIMdbFactory::MakeCompare().

  virtual mdb_err GetOldCompare(nsIMdbEnv* ev,
    nsIMdbCompare** acqOldCompare);
    // Get this sorting instance's compare object, which handles the
    // ordering of rows in the sorting, by comparing yarns from the cells
    // in the column being sorted.  Since nsIMdbCompare has no interface
    // to query the state of the compare object, it is not clear what you
    // would do with this object when returned, except maybe compare it
    // as a pointer address to some other instance, to check identities.
  
  // } ----- end attribute methods -----

  // { ----- begin cursor methods -----
  virtual mdb_err GetSortingRowCursor( // make a cursor, starting at inRowPos
    nsIMdbEnv* ev, // context
    mdb_pos inRowPos, // zero-based ordinal position of row in table
    nsIMdbTableRowCursor** acqCursor); // acquire new cursor instance
    // A cursor interface turning same info as PosToOid() or PosToRow().
  // } ----- end row position methods -----

  // { ----- begin row position methods -----
  virtual mdb_err PosToOid( // get row member for a table position
    nsIMdbEnv* ev, // context
    mdb_pos inRowPos, // zero-based ordinal position of row in table
    mdbOid* outOid); // row oid at the specified position
    
  virtual mdb_err PosToRow( // test for the table position of a row member
    nsIMdbEnv* ev, // context
    mdb_pos inRowPos, // zero-based ordinal position of row in table
    nsIMdbRow** acqRow); // acquire row at table position inRowPos
  // } ----- end row position methods -----

// } ===== end nsIMdbSorting methods =====

};
 
//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _ORKINSORTING_ */

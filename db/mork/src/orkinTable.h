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

#ifndef _ORKINTABLE_
#define _ORKINTABLE_ 1

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

#ifndef _MORKTABLE_
#include "morkTable.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkMagic_kTable 0x5461626C /* ascii 'Tabl' */

/*| orkinTable: 
|*/
class orkinTable : public morkHandle, public nsIMdbTable { // nsIMdbTable

// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  // virtual void CloseMorkNode(morkEnv* ev); // morkHandle is fine
  virtual ~orkinTable(); // morkHandle destructor does everything
  
protected: // construction is protected (use the static Make() method)
  orkinTable(morkEnv* ev, // note morkUsage is always morkUsage_kPool
    morkHandleFace* ioFace,    // must not be nil, cookie for this handle
    morkTable* ioObject); // must not be nil, the object for this handle
    
  // void CloseHandle(morkEnv* ev); // don't need to specialize closing

private: // copying is not allowed
  orkinTable(const orkinTable& other);
  orkinTable& operator=(const orkinTable& other);

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

  static orkinTable* MakeTable(morkEnv* ev, morkTable* ioObject);

public: // utilities:

  morkEnv* CanUseTable(nsIMdbEnv* mev, mork_bool inMutable,
    mdb_err* outErr) const;

public: // type identification
  mork_bool IsOrkinTable() const
  { return mHandle_Magic == morkMagic_kTable; }

  mork_bool IsOrkinTableHandle() const
  { return this->IsHandle() && this->IsOrkinTable(); }

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

// { ===== begin nsIMdbTable methods =====

  // { ----- begin meta attribute methods -----
  virtual mdb_err SetTablePriority(nsIMdbEnv* ev, mdb_priority inPrio);
  virtual mdb_err GetTablePriority(nsIMdbEnv* ev, mdb_priority* outPrio);
  
  virtual mdb_err GetTableBeVerbose(nsIMdbEnv* ev, mdb_bool* outBeVerbose);
  virtual mdb_err SetTableBeVerbose(nsIMdbEnv* ev, mdb_bool inBeVerbose);
  
  virtual mdb_err GetTableIsUnique(nsIMdbEnv* ev, mdb_bool* outIsUnique);

  virtual mdb_err GetTableKind(nsIMdbEnv* ev, mdb_kind* outTableKind);
  virtual mdb_err GetRowScope(nsIMdbEnv* ev, mdb_scope* outRowScope);
  
  virtual mdb_err GetMetaRow(
    nsIMdbEnv* ev, // context
    const mdbOid* inOptionalMetaRowOid, // can be nil to avoid specifying 
    mdbOid* outOid, // output meta row oid, can be nil to suppress output
    nsIMdbRow** acqRow); // acquire table's unique singleton meta row
    // The purpose of a meta row is to support the persistent recording of
    // meta info about a table as cells put into the distinguished meta row.
    // Each table has exactly one meta row, which is not considered a member
    // of the collection of rows inside the table.  The only way to tell
    // whether a row is a meta row is by the fact that it is returned by this
    // GetMetaRow() method from some table. Otherwise nothing distinguishes
    // a meta row from any other row.  A meta row can be used anyplace that
    // any other row can be used, and can even be put into other tables (or
    // the same table) as a table member, if this is useful for some reason.
    // The first attempt to access a table's meta row using GetMetaRow() will
    // cause the meta row to be created if it did not already exist.  When the
    // meta row is created, it will have the row oid that was previously
    // requested for this table's meta row; or if no oid was ever explicitly
    // specified for this meta row, then a unique oid will be generated in
    // the row scope named "metaScope" (so obviously MDB clients should not
    // manually allocate any row IDs from that special meta scope namespace).
    // The meta row oid can be specified either when the table is created, or
    // else the first time that GetMetaRow() is called, by passing a non-nil
    // pointer to an oid for parameter inOptionalMetaRowOid.  The meta row's
    // actual oid is returned in outOid (if this is a non-nil pointer), and
    // it will be different from inOptionalMetaRowOid when the meta row was
    // already given a different oid earlier.
  // } ----- end meta attribute methods -----

  // { ----- begin cursor methods -----
  virtual mdb_err GetTableRowCursor( // make a cursor, starting iteration at inRowPos
    nsIMdbEnv* ev, // context
    mdb_pos inRowPos, // zero-based ordinal position of row in table
    nsIMdbTableRowCursor** acqCursor); // acquire new cursor instance
  // } ----- end row position methods -----

  // { ----- begin row position methods -----
  virtual mdb_err PosToOid( // get row member for a table position
    nsIMdbEnv* ev, // context
    mdb_pos inRowPos, // zero-based ordinal position of row in table
    mdbOid* outOid); // row oid at the specified position

  virtual mdb_err OidToPos( // test for the table position of a row member
    nsIMdbEnv* ev, // context
    const mdbOid* inOid, // row to find in table
    mdb_pos* outPos); // zero-based ordinal position of row in table
     
  virtual mdb_err PosToRow( // test for the table position of a row member
    nsIMdbEnv* ev, // context
    mdb_pos inRowPos, // zero-based ordinal position of row in table
    nsIMdbRow** acqRow); // acquire row at table position inRowPos
   
  virtual mdb_err RowToPos( // test for the table position of a row member
    nsIMdbEnv* ev, // context
    nsIMdbRow* ioRow, // row to find in table
    mdb_pos* outPos); // zero-based ordinal position of row in table

  // } ----- end row position methods -----

  // { ----- begin oid set methods -----
  virtual mdb_err AddOid( // make sure the row with inOid is a table member 
    nsIMdbEnv* ev, // context
    const mdbOid* inOid); // row to ensure membership in table

  virtual mdb_err HasOid( // test for the table position of a row member
    nsIMdbEnv* ev, // context
    const mdbOid* inOid, // row to find in table
    mdb_bool* outHasOid); // whether inOid is a member row

  virtual mdb_err CutOid( // make sure the row with inOid is not a member 
    nsIMdbEnv* ev, // context
    const mdbOid* inOid); // row to remove from table
  // } ----- end oid set methods -----

  // { ----- begin row set methods -----
  virtual mdb_err NewRow( // create a new row instance in table
    nsIMdbEnv* ev, // context
    mdbOid* ioOid, // please use zero (unbound) rowId for db-assigned IDs
    nsIMdbRow** acqRow); // create new row

  virtual mdb_err AddRow( // make sure the row with inOid is a table member 
    nsIMdbEnv* ev, // context
    nsIMdbRow* ioRow); // row to ensure membership in table

  virtual mdb_err HasRow( // test for the table position of a row member
    nsIMdbEnv* ev, // context
    nsIMdbRow* ioRow, // row to find in table
    mdb_bool* outHasRow); // whether row is a table member

  virtual mdb_err CutRow( // make sure the row with inOid is not a member 
    nsIMdbEnv* ev, // context
    nsIMdbRow* ioRow); // row to remove from table

  virtual mdb_err CutAllRows( // remove all rows from the table
    nsIMdbEnv* ev); // context
  // } ----- end row set methods -----

  // { ----- begin hinting methods -----
  virtual mdb_err SearchColumnsHint( // advise re future expected search cols  
    nsIMdbEnv* ev, // context
    const mdbColumnSet* inColumnSet); // columns likely to be searched
    
  virtual mdb_err SortColumnsHint( // advise re future expected sort columns  
    nsIMdbEnv* ev, // context
    const mdbColumnSet* inColumnSet); // columns for likely sort requests

  virtual mdb_err StartBatchChangeHint( // advise before many adds and cuts  
    nsIMdbEnv* ev, // context
    const void* inLabel); // intend unique address to match end call
    // If batch starts nest by virtue of nesting calls in the stack, then
    // the address of a local variable makes a good batch start label that
    // can be used at batch end time, and such addresses remain unique.
  virtual mdb_err EndBatchChangeHint( // advise before many adds and cuts  
    nsIMdbEnv* ev, // context
    const void* inLabel); // label matching start label
    // Suppose a table is maintaining one or many sort orders for a table,
    // so that every row added to the table must be inserted in each sort,
    // and every row cut must be removed from each sort.  If a db client
    // intends to make many such changes before needing any information
    // about the order or positions of rows inside a table, then a client
    // might tell the table to start batch changes in order to disable
    // sorting of rows for the interim.  Presumably a table will then do
    // a full sort of all rows at need when the batch changes end, or when
    // a surprise request occurs for row position during batch changes.
  // } ----- end hinting methods -----

  // { ----- begin searching methods -----
  virtual mdb_err FindRowMatches( // search variable number of sorted cols
    nsIMdbEnv* ev, // context
    const mdbYarn* inPrefix, // content to find as prefix in row's column cell
    nsIMdbTableRowCursor** acqCursor); // set of matching rows
    
  virtual mdb_err GetSearchColumns( // query columns used by FindRowMatches()
    nsIMdbEnv* ev, // context
    mdb_count* outCount, // context
    mdbColumnSet* outColSet); // caller supplied space to put columns
    // GetSearchColumns() returns the columns actually searched when the
    // FindRowMatches() method is called.  No more than mColumnSet_Count
    // slots of mColumnSet_Columns will be written, since mColumnSet_Count
    // indicates how many slots are present in the column array.  The
    // actual number of search column used by the table is returned in
    // the outCount parameter; if this number exceeds mColumnSet_Count,
    // then a caller needs a bigger array to read the entire column set.
    // The minimum of mColumnSet_Count and outCount is the number slots
    // in mColumnSet_Columns that were actually written by this method.
    //
    // Callers are expected to change this set of columns by calls to
    // nsIMdbTable::SearchColumnsHint() or SetSearchSorting(), or both.
  // } ----- end searching methods -----

  // { ----- begin sorting methods -----
  // sorting: note all rows are assumed sorted by row ID as a secondary
  // sort following the primary column sort, when table rows are sorted.

  virtual mdb_err
  CanSortColumn( // query which column is currently used for sorting
    nsIMdbEnv* ev, // context
    mdb_column inColumn, // column to query sorting potential
    mdb_bool* outCanSort); // whether the column can be sorted
    
  virtual mdb_err GetSorting( // view same table in particular sorting
    nsIMdbEnv* ev, // context
    mdb_column inColumn, // requested new column for sorting table
    nsIMdbSorting** acqSorting); // acquire sorting for column
    
  virtual mdb_err SetSearchSorting( // use this sorting in FindRowMatches()
    nsIMdbEnv* ev, // context
    mdb_column inColumn, // often same as nsIMdbSorting::GetSortColumn()
    nsIMdbSorting* ioSorting); // requested sorting for some column
    // SetSearchSorting() attempts to inform the table that ioSorting
    // should be used during calls to FindRowMatches() for searching
    // the column which is actually sorted by ioSorting.  This method
    // is most useful in conjunction with nsIMdbSorting::SetCompare(),
    // because otherwise a caller would not be able to override the
    // comparison ordering method used during searchs.  Note that some
    // database implementations might be unable to use an arbitrarily
    // specified sort order, either due to schema or runtime interface
    // constraints, in which case ioSorting might not actually be used.
    // Presumably ioSorting is an instance that was returned from some
    // earlier call to nsIMdbTable::GetSorting().  A caller can also
    // use nsIMdbTable::SearchColumnsHint() to specify desired change
    // in which columns are sorted and searched by FindRowMatches().
    //
    // A caller can pass a nil pointer for ioSorting to request that
    // column inColumn no longer be used at all by FindRowMatches().
    // But when ioSorting is non-nil, then inColumn should match the
    // column actually sorted by ioSorting; when these do not agree,
    // implementations are instructed to give precedence to the column
    // specified by ioSorting (so this means callers might just pass
    // zero for inColumn when ioSorting is also provided, since then
    // inColumn is both redundant and ignored).
  // } ----- end sorting methods -----

  // { ----- begin moving methods -----
  // moving a row does nothing unless a table is currently unsorted
  
  virtual mdb_err MoveOid( // change position of row in unsorted table
    nsIMdbEnv* ev, // context
    const mdbOid* inOid,  // row oid to find in table
    mdb_pos inHintFromPos, // suggested hint regarding start position
    mdb_pos inToPos,       // desired new position for row inRowId
    mdb_pos* outActualPos); // actual new position of row in table

  virtual mdb_err MoveRow( // change position of row in unsorted table
    nsIMdbEnv* ev, // context
    nsIMdbRow* ioRow,  // row oid to find in table
    mdb_pos inHintFromPos, // suggested hint regarding start position
    mdb_pos inToPos,       // desired new position for row inRowId
    mdb_pos* outActualPos); // actual new position of row in table
  // } ----- end moving methods -----
  
  // { ----- begin index methods -----
  virtual mdb_err AddIndex( // create a sorting index for column if possible
    nsIMdbEnv* ev, // context
    mdb_column inColumn, // the column to sort by index
    nsIMdbThumb** acqThumb); // acquire thumb for incremental index building
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the index addition will be finished.
  
  virtual mdb_err CutIndex( // stop supporting a specific column index
    nsIMdbEnv* ev, // context
    mdb_column inColumn, // the column with index to be removed
    nsIMdbThumb** acqThumb); // acquire thumb for incremental index destroy
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the index removal will be finished.
  
  virtual mdb_err HasIndex( // query for current presence of a column index
    nsIMdbEnv* ev, // context
    mdb_column inColumn, // the column to investigate
    mdb_bool* outHasIndex); // whether column has index for this column

  
  virtual mdb_err EnableIndexOnSort( // create an index for col on first sort
    nsIMdbEnv* ev, // context
    mdb_column inColumn); // the column to index if ever sorted
  
  virtual mdb_err QueryIndexOnSort( // check whether index on sort is enabled
    nsIMdbEnv* ev, // context
    mdb_column inColumn, // the column to investigate
    mdb_bool* outIndexOnSort); // whether column has index-on-sort enabled
  
  virtual mdb_err DisableIndexOnSort( // prevent future index creation on sort
    nsIMdbEnv* ev, // context
    mdb_column inColumn); // the column to index if ever sorted
  // } ----- end index methods -----

// } ===== end nsIMdbTable methods =====
};
 
//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _ORKINTABLE_ */

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

#ifndef _MORKTABLE_
#define _MORKTABLE_ 1

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

#ifndef _MORKDEQUE_
#include "morkDeque.h"
#endif

#ifndef _MORKOBJECT_
#include "morkObject.h"
#endif

#ifndef _MORKARRAY_
#include "morkArray.h"
#endif

#ifndef _MORKROWMAP_
#include "morkRowMap.h"
#endif

#ifndef _MORKNODEMAP_
#include "morkNodeMap.h"
#endif

#ifndef _MORKPROBEMAP_
#include "morkProbeMap.h"
#endif

#ifndef _MORKBEAD_
#include "morkBead.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

class nsIMdbTable;
#define morkDerived_kTable  /*i*/ 0x5462 /* ascii 'Tb' */

/*| kStartRowArraySize: starting physical size of array for mTable_RowArray.
**| We want this number very small, so that a table containing exactly one
**| row member will not pay too significantly in space overhead.  But we want
**| a number bigger than one, so there is some space for growth.
|*/
#define morkTable_kStartRowArraySize 3 /* modest starting size for array */

/*| kMakeRowMapThreshold: this is the number of rows in a table which causes
**| a hash table (mTable_RowMap) to be lazily created for faster member row
**| identification, during such operations as cuts and adds.  This number must
**| be small enough that linear searches are not bad for member counts less
**| than this; but this number must also be large enough that creating a hash
**| table does not increase the per-row space overhead by a big percentage.
**| For speed, numbers on the order of ten to twenty are all fine; for space,
**| I believe a number as small as ten will have too much space overhead.
|*/
#define morkTable_kMakeRowMapThreshold 17 /* when to build mTable_RowMap */

#define morkTable_kStartRowMapSlotCount 13
#define morkTable_kMaxTableGcUses 0x0FF /* max for 8-bit unsigned int */

#define morkTable_kUniqueBit   ((mork_u1) (1 << 0))
#define morkTable_kVerboseBit  ((mork_u1) (1 << 1))
#define morkTable_kNotedBit    ((mork_u1) (1 << 2)) /* space has change notes */
#define morkTable_kRewriteBit  ((mork_u1) (1 << 3)) /* must rewrite all rows */
#define morkTable_kNewMetaBit  ((mork_u1) (1 << 4)) /* new table meta row */

class morkTable : public morkObject, public morkLink, public nsIMdbTable { 

  // NOTE the morkLink base is for morkRowSpace::mRowSpace_TablesByPriority

// public: // slots inherited from morkObject (meant to inform only)
  // nsIMdbHeap*    mNode_Heap;

  // mork_base      mNode_Base;     // must equal morkBase_kNode
  // mork_derived   mNode_Derived;  // depends on specific node subclass
  
  // mork_access    mNode_Access;   // kOpen, kClosing, kShut, or kDead
  // mork_usage     mNode_Usage;    // kHeap, kStack, kMember, kGlobal, kNone
  // mork_able      mNode_Mutable;  // can this node be modified?
  // mork_load      mNode_Load;     // is this node clean or dirty?
  
  // mork_uses      mNode_Uses;     // refcount for strong refs
  // mork_refs      mNode_Refs;     // refcount for strong refs + weak refs

  // mork_color   mBead_Color;   // ID for this bead
  // morkHandle*  mObject_Handle;  // weak ref to handle for this object

public: // bead color setter & getter replace obsolete member mTable_Id:

  NS_DECL_ISUPPORTS_INHERITED
  mork_tid     TableId() const { return mBead_Color; }
  void         SetTableId(mork_tid inTid) { mBead_Color = inTid; }

  // we override these so we use xpcom ref-counting semantics.
  virtual mork_refs    AddStrongRef(morkEnv* ev);
  virtual mork_refs    CutStrongRef(morkEnv* ev);
public: // state is public because the entire Mork system is private

// { ===== begin nsIMdbCollection methods =====

  // { ----- begin attribute methods -----
  NS_IMETHOD GetSeed(nsIMdbEnv* ev,
    mdb_seed* outSeed);    // member change count
  NS_IMETHOD GetCount(nsIMdbEnv* ev,
    mdb_count* outCount); // member count

  NS_IMETHOD GetPort(nsIMdbEnv* ev,
    nsIMdbPort** acqPort); // collection container
  // } ----- end attribute methods -----

  // { ----- begin cursor methods -----
  NS_IMETHOD GetCursor( // make a cursor starting iter at inMemberPos
    nsIMdbEnv* ev, // context
    mdb_pos inMemberPos, // zero-based ordinal pos of member in collection
    nsIMdbCursor** acqCursor); // acquire new cursor instance
  // } ----- end cursor methods -----

  // { ----- begin ID methods -----
  NS_IMETHOD GetOid(nsIMdbEnv* ev,
    mdbOid* outOid); // read object identity
  NS_IMETHOD BecomeContent(nsIMdbEnv* ev,
    const mdbOid* inOid); // exchange content
  // } ----- end ID methods -----

  // { ----- begin activity dropping methods -----
  NS_IMETHOD DropActivity( // tell collection usage no longer expected
    nsIMdbEnv* ev);
  // } ----- end activity dropping methods -----

// } ===== end nsIMdbCollection methods =====
  NS_IMETHOD SetTablePriority(nsIMdbEnv* ev, mdb_priority inPrio);
  NS_IMETHOD GetTablePriority(nsIMdbEnv* ev, mdb_priority* outPrio);
  
  NS_IMETHOD GetTableBeVerbose(nsIMdbEnv* ev, mdb_bool* outBeVerbose);
  NS_IMETHOD SetTableBeVerbose(nsIMdbEnv* ev, mdb_bool inBeVerbose);
  
  NS_IMETHOD GetTableIsUnique(nsIMdbEnv* ev, mdb_bool* outIsUnique);
  
  NS_IMETHOD GetTableKind(nsIMdbEnv* ev, mdb_kind* outTableKind);
  NS_IMETHOD GetRowScope(nsIMdbEnv* ev, mdb_scope* outRowScope);
  
  NS_IMETHOD GetMetaRow(
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
    // the row scope named "m" (so obviously MDB clients should not
    // manually allocate any row IDs from that special meta scope namespace).
    // The meta row oid can be specified either when the table is created, or
    // else the first time that GetMetaRow() is called, by passing a non-nil
    // pointer to an oid for parameter inOptionalMetaRowOid.  The meta row's
    // actual oid is returned in outOid (if this is a non-nil pointer), and
    // it will be different from inOptionalMetaRowOid when the meta row was
    // already given a different oid earlier.
  // } ----- end meta attribute methods -----


  // { ----- begin cursor methods -----
  NS_IMETHOD GetTableRowCursor( // make a cursor, starting iteration at inRowPos
    nsIMdbEnv* ev, // context
    mdb_pos inRowPos, // zero-based ordinal position of row in table
    nsIMdbTableRowCursor** acqCursor); // acquire new cursor instance
  // } ----- end row position methods -----

  // { ----- begin row position methods -----
  NS_IMETHOD PosToOid( // get row member for a table position
    nsIMdbEnv* ev, // context
    mdb_pos inRowPos, // zero-based ordinal position of row in table
    mdbOid* outOid); // row oid at the specified position

  NS_IMETHOD OidToPos( // test for the table position of a row member
    nsIMdbEnv* ev, // context
    const mdbOid* inOid, // row to find in table
    mdb_pos* outPos); // zero-based ordinal position of row in table
    
  NS_IMETHOD PosToRow( // test for the table position of a row member
    nsIMdbEnv* ev, // context
    mdb_pos inRowPos, // zero-based ordinal position of row in table
    nsIMdbRow** acqRow); // acquire row at table position inRowPos
    
  NS_IMETHOD RowToPos( // test for the table position of a row member
    nsIMdbEnv* ev, // context
    nsIMdbRow* ioRow, // row to find in table
    mdb_pos* outPos); // zero-based ordinal position of row in table
  // } ----- end row position methods -----

  // { ----- begin oid set methods -----
  NS_IMETHOD AddOid( // make sure the row with inOid is a table member 
    nsIMdbEnv* ev, // context
    const mdbOid* inOid); // row to ensure membership in table

  NS_IMETHOD HasOid( // test for the table position of a row member
    nsIMdbEnv* ev, // context
    const mdbOid* inOid, // row to find in table
    mdb_bool* outHasOid); // whether inOid is a member row

  NS_IMETHOD CutOid( // make sure the row with inOid is not a member 
    nsIMdbEnv* ev, // context
    const mdbOid* inOid); // row to remove from table
  // } ----- end oid set methods -----

  // { ----- begin row set methods -----
  NS_IMETHOD NewRow( // create a new row instance in table
    nsIMdbEnv* ev, // context
    mdbOid* ioOid, // please use minus one (unbound) rowId for db-assigned IDs
    nsIMdbRow** acqRow); // create new row

  NS_IMETHOD AddRow( // make sure the row with inOid is a table member 
    nsIMdbEnv* ev, // context
    nsIMdbRow* ioRow); // row to ensure membership in table

  NS_IMETHOD HasRow( // test for the table position of a row member
    nsIMdbEnv* ev, // context
    nsIMdbRow* ioRow, // row to find in table
    mdb_bool* outHasRow); // whether row is a table member

  NS_IMETHOD CutRow( // make sure the row with inOid is not a member 
    nsIMdbEnv* ev, // context
    nsIMdbRow* ioRow); // row to remove from table

  NS_IMETHOD CutAllRows( // remove all rows from the table
    nsIMdbEnv* ev); // context
  // } ----- end row set methods -----

  // { ----- begin hinting methods -----
  NS_IMETHOD SearchColumnsHint( // advise re future expected search cols  
    nsIMdbEnv* ev, // context
    const mdbColumnSet* inColumnSet); // columns likely to be searched
    
  NS_IMETHOD SortColumnsHint( // advise re future expected sort columns  
    nsIMdbEnv* ev, // context
    const mdbColumnSet* inColumnSet); // columns for likely sort requests
    
  NS_IMETHOD StartBatchChangeHint( // advise before many adds and cuts  
    nsIMdbEnv* ev, // context
    const void* inLabel); // intend unique address to match end call
    // If batch starts nest by virtue of nesting calls in the stack, then
    // the address of a local variable makes a good batch start label that
    // can be used at batch end time, and such addresses remain unique.
    
  NS_IMETHOD EndBatchChangeHint( // advise before many adds and cuts  
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
  NS_IMETHOD FindRowMatches( // search variable number of sorted cols
    nsIMdbEnv* ev, // context
    const mdbYarn* inPrefix, // content to find as prefix in row's column cell
    nsIMdbTableRowCursor** acqCursor); // set of matching rows
    
  NS_IMETHOD GetSearchColumns( // query columns used by FindRowMatches()
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

  NS_IMETHOD
  CanSortColumn( // query which column is currently used for sorting
    nsIMdbEnv* ev, // context
    mdb_column inColumn, // column to query sorting potential
    mdb_bool* outCanSort); // whether the column can be sorted
    
  NS_IMETHOD GetSorting( // view same table in particular sorting
    nsIMdbEnv* ev, // context
    mdb_column inColumn, // requested new column for sorting table
    nsIMdbSorting** acqSorting); // acquire sorting for column
    
  NS_IMETHOD SetSearchSorting( // use this sorting in FindRowMatches()
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
  
  NS_IMETHOD MoveOid( // change position of row in unsorted table
    nsIMdbEnv* ev, // context
    const mdbOid* inOid,  // row oid to find in table
    mdb_pos inHintFromPos, // suggested hint regarding start position
    mdb_pos inToPos,       // desired new position for row inRowId
    mdb_pos* outActualPos); // actual new position of row in table

  NS_IMETHOD MoveRow( // change position of row in unsorted table
    nsIMdbEnv* ev, // context
    nsIMdbRow* ioRow,  // row oid to find in table
    mdb_pos inHintFromPos, // suggested hint regarding start position
    mdb_pos inToPos,       // desired new position for row inRowId
    mdb_pos* outActualPos); // actual new position of row in table
  // } ----- end moving methods -----
  
  // { ----- begin index methods -----
  NS_IMETHOD AddIndex( // create a sorting index for column if possible
    nsIMdbEnv* ev, // context
    mdb_column inColumn, // the column to sort by index
    nsIMdbThumb** acqThumb); // acquire thumb for incremental index building
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the index addition will be finished.
  
  NS_IMETHOD CutIndex( // stop supporting a specific column index
    nsIMdbEnv* ev, // context
    mdb_column inColumn, // the column with index to be removed
    nsIMdbThumb** acqThumb); // acquire thumb for incremental index destroy
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the index removal will be finished.
  
  NS_IMETHOD HasIndex( // query for current presence of a column index
    nsIMdbEnv* ev, // context
    mdb_column inColumn, // the column to investigate
    mdb_bool* outHasIndex); // whether column has index for this column

  
  NS_IMETHOD EnableIndexOnSort( // create an index for col on first sort
    nsIMdbEnv* ev, // context
    mdb_column inColumn); // the column to index if ever sorted
  
  NS_IMETHOD QueryIndexOnSort( // check whether index on sort is enabled
    nsIMdbEnv* ev, // context
    mdb_column inColumn, // the column to investigate
    mdb_bool* outIndexOnSort); // whether column has index-on-sort enabled
  
  NS_IMETHOD DisableIndexOnSort( // prevent future index creation on sort
    nsIMdbEnv* ev, // context
    mdb_column inColumn); // the column to index if ever sorted
  // } ----- end index methods -----

  morkStore*      mTable_Store;   // non-refcnted ptr to port

  // mTable_RowSpace->SpaceScope() is row scope 
  morkRowSpace*   mTable_RowSpace; // non-refcnted ptr to containing space

  morkRow*        mTable_MetaRow; // table's actual meta row
  mdbOid          mTable_MetaRowOid; // oid for meta row
  
  morkRowMap*     mTable_RowMap;     // (strong ref) hash table of all members
  morkArray       mTable_RowArray;   // array of morkRow pointers
  
  morkList        mTable_ChangeList;      // list of table changes
  mork_u2         mTable_ChangesCount; // length of changes list 
  mork_u2         mTable_ChangesMax;   // max list length before rewrite 
  
  // mork_tid        mTable_Id;
  mork_kind       mTable_Kind;
  
  mork_u1         mTable_Flags;         // bit flags
  mork_priority   mTable_Priority;      // 0..9, any other value equals 9
  mork_u1         mTable_GcUses;        // persistent references from cells
  mork_u1         mTable_Pad;      // for u4 alignment

public: // flags bit twiddling
  
  void SetTableUnique() { mTable_Flags |= morkTable_kUniqueBit; }
  void SetTableVerbose() { mTable_Flags |= morkTable_kVerboseBit; }
  void SetTableNoted() { mTable_Flags |= morkTable_kNotedBit; }
  void SetTableRewrite() { mTable_Flags |= morkTable_kRewriteBit; }
  void SetTableNewMeta() { mTable_Flags |= morkTable_kNewMetaBit; }

  void ClearTableUnique() { mTable_Flags &= (mork_u1) ~morkTable_kUniqueBit; }
  void ClearTableVerbose() { mTable_Flags &= (mork_u1) ~morkTable_kVerboseBit; }
  void ClearTableNoted() { mTable_Flags &= (mork_u1) ~morkTable_kNotedBit; }
  void ClearTableRewrite() { mTable_Flags &= (mork_u1) ~morkTable_kRewriteBit; }
  void ClearTableNewMeta() { mTable_Flags &= (mork_u1) ~morkTable_kNewMetaBit; }

  mork_bool IsTableUnique() const
  { return ( mTable_Flags & morkTable_kUniqueBit ) != 0; }
  
  mork_bool IsTableVerbose() const
  { return ( mTable_Flags & morkTable_kVerboseBit ) != 0; }
  
  mork_bool IsTableNoted() const
  { return ( mTable_Flags & morkTable_kNotedBit ) != 0; }
  
  mork_bool IsTableRewrite() const
  { return ( mTable_Flags & morkTable_kRewriteBit ) != 0; }
  
  mork_bool IsTableNewMeta() const
  { return ( mTable_Flags & morkTable_kNewMetaBit ) != 0; }

public: // table dirty handling more complex than morkNode::SetNodeDirty() etc.

  void SetTableDirty() { this->SetNodeDirty(); }
  void SetTableClean(morkEnv* ev);
   
  mork_bool IsTableClean() const { return this->IsNodeClean(); }
  mork_bool IsTableDirty() const { return this->IsNodeDirty(); }

public: // morkNode memory management operators
  void* operator new(size_t inSize, nsIMdbHeap& ioHeap, morkEnv* ev) CPP_THROW_NEW
  { return morkNode::MakeNew(inSize, ioHeap, ev); }
  
 
// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseTable() if open
  virtual ~morkTable(); // assert that close executed earlier
  
public: // morkTable construction & destruction
  morkTable(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioNodeHeap, morkStore* ioStore,
    nsIMdbHeap* ioSlotHeap, morkRowSpace* ioRowSpace,
    const mdbOid* inOptionalMetaRowOid, // can be nil to avoid specifying 
    mork_tid inTableId,
    mork_kind inKind, mork_bool inMustBeUnique);
  void CloseTable(morkEnv* ev); // called by CloseMorkNode();

private: // copying is not allowed
  morkTable(const morkTable& other);
  morkTable& operator=(const morkTable& other);

public: // dynamic type identification
  mork_bool IsTable() const
  { return IsNode() && mNode_Derived == morkDerived_kTable; }
// } ===== end morkNode methods =====

public: // errors
  static void NonTableTypeError(morkEnv* ev);
  static void NonTableTypeWarning(morkEnv* ev);
  static void NilRowSpaceError(morkEnv* ev);

public: // warnings
  static void TableGcUsesUnderflowWarning(morkEnv* ev);

public: // noting table changes

  mork_bool HasChangeOverflow() const
  { return mTable_ChangesCount >= mTable_ChangesMax; }

  void NoteTableSetAll(morkEnv* ev);
  void NoteTableMoveRow(morkEnv* ev, morkRow* ioRow, mork_pos inPos);

  void note_row_change(morkEnv* ev, mork_change inChange, morkRow* ioRow);
  void note_row_move(morkEnv* ev, morkRow* ioRow, mork_pos inNewPos);
  
  void NoteTableAddRow(morkEnv* ev, morkRow* ioRow)
  { this->note_row_change(ev, morkChange_kAdd, ioRow); }
  
  void NoteTableCutRow(morkEnv* ev, morkRow* ioRow)
  { this->note_row_change(ev, morkChange_kCut, ioRow); }
  
protected: // internal row map methods

  morkRow* find_member_row(morkEnv* ev, morkRow* ioRow);
  void build_row_map(morkEnv* ev);

public: // other table methods
  
  mork_bool MaybeDirtySpaceStoreAndTable();

  morkRow* GetMetaRow(morkEnv* ev, const mdbOid* inOptionalMetaRowOid);
  
  mork_u2 AddTableGcUse(morkEnv* ev);
  mork_u2 CutTableGcUse(morkEnv* ev);

  // void DirtyAllTableContent(morkEnv* ev);

  mork_seed TableSeed() const { return mTable_RowArray.mArray_Seed; }
  
  morkRow* SafeRowAt(morkEnv* ev, mork_pos inPos)
  { return (morkRow*) mTable_RowArray.SafeAt(ev, inPos); }

  nsIMdbTable* AcquireTableHandle(morkEnv* ev); // mObject_Handle
  
  mork_count GetRowCount() const { return mTable_RowArray.mArray_Fill; }

  mork_bool IsTableUsed() const
  { return (mTable_GcUses != 0 || this->GetRowCount() != 0); }

  void GetTableOid(morkEnv* ev, mdbOid* outOid);
  mork_pos  ArrayHasOid(morkEnv* ev, const mdbOid* inOid);
  mork_bool MapHasOid(morkEnv* ev, const mdbOid* inOid);
  mork_bool AddRow(morkEnv* ev, morkRow* ioRow); // returns ev->Good()
  mork_bool CutRow(morkEnv* ev, morkRow* ioRow); // returns ev->Good()
  mork_bool CutAllRows(morkEnv* ev); // returns ev->Good()
  
  mork_pos MoveRow(morkEnv* ev, morkRow* ioRow, // change row position
    mork_pos inHintFromPos, // suggested hint regarding start position
    mork_pos inToPos); // desired new position for row ioRow
    // MoveRow() returns the actual position of ioRow afterwards; this
    // position is -1 if and only if ioRow was not found as a member.     


  morkTableRowCursor* NewTableRowCursor(morkEnv* ev, mork_pos inRowPos);

public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakTable(morkTable* me,
    morkEnv* ev, morkTable** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongTable(morkTable* me,
    morkEnv* ev, morkTable** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// use negative values for kCut and kAdd, to keep non-neg move pos distinct:
#define morkTableChange_kCut ((mork_pos) -1) /* shows row was cut */
#define morkTableChange_kAdd ((mork_pos) -2) /* shows row was added */
#define morkTableChange_kNone ((mork_pos) -3) /* unknown change */

class morkTableChange : public morkNext { 
public: // state is public because the entire Mork system is private

  morkRow*  mTableChange_Row; // the row in the change
  
  mork_pos  mTableChange_Pos; // kAdd, kCut, or non-neg for row move

public:
  morkTableChange(morkEnv* ev, mork_change inChange, morkRow* ioRow);
  // use this constructor for inChange == morkChange_kAdd or morkChange_kCut
  
  morkTableChange(morkEnv* ev, morkRow* ioRow, mork_pos inPos);
  // use this constructor when the row is moved

public:
  void UnknownChangeError(morkEnv* ev) const; // morkChange_kAdd or morkChange_kCut
  void NegativeMovePosError(morkEnv* ev) const; // move must be non-neg position
  
public:
  
  mork_bool IsAddRowTableChange() const
  { return ( mTableChange_Pos == morkTableChange_kAdd ); }
  
  mork_bool IsCutRowTableChange() const
  { return ( mTableChange_Pos == morkTableChange_kCut ); }
  
  mork_bool IsMoveRowTableChange() const
  { return ( mTableChange_Pos >= 0 ); }

public:
  
  mork_pos GetMovePos() const { return mTableChange_Pos; }
  // GetMovePos() assumes that IsMoveRowTableChange() is true.
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkDerived_kTableMap  /*i*/ 0x744D /* ascii 'tM' */

/*| morkTableMap: maps mork_token -> morkTable
|*/
#ifdef MORK_BEAD_OVER_NODE_MAPS
class morkTableMap : public morkBeadMap { 
#else /*MORK_BEAD_OVER_NODE_MAPS*/
class morkTableMap : public morkNodeMap { // for mapping tokens to tables
#endif /*MORK_BEAD_OVER_NODE_MAPS*/

public:

  virtual ~morkTableMap();
  morkTableMap(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, nsIMdbHeap* ioSlotHeap);

public: // other map methods

#ifdef MORK_BEAD_OVER_NODE_MAPS
  mork_bool  AddTable(morkEnv* ev, morkTable* ioTable)
  { return this->AddBead(ev, ioTable); }
  // the AddTable() boolean return equals ev->Good().

  mork_bool  CutTable(morkEnv* ev, mork_tid inTid)
  { return this->CutBead(ev, inTid); }
  // The CutTable() boolean return indicates whether removal happened. 
  
  morkTable*  GetTable(morkEnv* ev, mork_tid inTid)
  { return (morkTable*) this->GetBead(ev, inTid); }
  // Note the returned table does NOT have an increase in refcount for this.

  mork_num CutAllTables(morkEnv* ev)
  { return this->CutAllBeads(ev); }
  // CutAllTables() releases all the referenced table values.
  
#else /*MORK_BEAD_OVER_NODE_MAPS*/
  mork_bool  AddTable(morkEnv* ev, morkTable* ioTable)
  { return this->AddNode(ev, ioTable->TableId(), ioTable); }
  // the AddTable() boolean return equals ev->Good().

  mork_bool  CutTable(morkEnv* ev, mork_tid inTid)
  { return this->CutNode(ev, inTid); }
  // The CutTable() boolean return indicates whether removal happened. 
  
  morkTable*  GetTable(morkEnv* ev, mork_tid inTid)
  { return (morkTable*) this->GetNode(ev, inTid); }
  // Note the returned table does NOT have an increase in refcount for this.

  mork_num CutAllTables(morkEnv* ev)
  { return this->CutAllNodes(ev); }
  // CutAllTables() releases all the referenced table values.
#endif /*MORK_BEAD_OVER_NODE_MAPS*/

};

#ifdef MORK_BEAD_OVER_NODE_MAPS
class morkTableMapIter: public morkBeadMapIter {
#else /*MORK_BEAD_OVER_NODE_MAPS*/
class morkTableMapIter: public morkMapIter{ // typesafe wrapper class
#endif /*MORK_BEAD_OVER_NODE_MAPS*/

public:

#ifdef MORK_BEAD_OVER_NODE_MAPS
  morkTableMapIter(morkEnv* ev, morkTableMap* ioMap)
  : morkBeadMapIter(ev, ioMap) { }
 
  morkTableMapIter( ) : morkBeadMapIter()  { }
  void InitTableMapIter(morkEnv* ev, morkTableMap* ioMap)
  { this->InitBeadMapIter(ev, ioMap); }
   
  morkTable* FirstTable(morkEnv* ev)
  { return (morkTable*) this->FirstBead(ev); }
  
  morkTable* NextTable(morkEnv* ev)
  { return (morkTable*) this->NextBead(ev); }
  
  morkTable* HereTable(morkEnv* ev)
  { return (morkTable*) this->HereBead(ev); }
  

#else /*MORK_BEAD_OVER_NODE_MAPS*/
  morkTableMapIter(morkEnv* ev, morkTableMap* ioMap)
  : morkMapIter(ev, ioMap) { }
 
  morkTableMapIter( ) : morkMapIter()  { }
  void InitTableMapIter(morkEnv* ev, morkTableMap* ioMap)
  { this->InitMapIter(ev, ioMap); }
   
  mork_change*
  FirstTable(morkEnv* ev, mork_tid* outTid, morkTable** outTable)
  { return this->First(ev, outTid, outTable); }
  
  mork_change*
  NextTable(morkEnv* ev, mork_tid* outTid, morkTable** outTable)
  { return this->Next(ev, outTid, outTable); }
  
  mork_change*
  HereTable(morkEnv* ev, mork_tid* outTid, morkTable** outTable)
  { return this->Here(ev, outTid, outTable); }
  
  // cutting while iterating hash map might dirty the parent table:
  mork_change*
  CutHereTable(morkEnv* ev, mork_tid* outTid, morkTable** outTable)
  { return this->CutHere(ev, outTid, outTable); }
#endif /*MORK_BEAD_OVER_NODE_MAPS*/
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKTABLE_ */


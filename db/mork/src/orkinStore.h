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

#ifndef _ORKINSTORE_
#define _ORKINSTORE_ 1

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

#ifndef _MORKStore_
#include "morkStore.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkMagic_kStore 0x53746F72 /* ascii 'Stor' */
 
/*| orkinStore: 
|*/
class orkinStore : public morkHandle, public nsIMdbStore { // nsIMdbPort

// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  // virtual void CloseMorkNode(morkEnv* ev); // morkHandle is fine
  virtual ~orkinStore(); // morkHandle destructor does everything
  
protected: // construction is protected (use the static Make() method)
  orkinStore(morkEnv* ev, // note morkUsage is always morkUsage_kPool
    morkHandleFace* ioFace,    // must not be nil, cookie for this handle
    morkStore* ioObject); // must not be nil, the object for this handle
    
  // void CloseHandle(morkEnv* ev); // don't need to specialize closing

private: // copying is not allowed
  orkinStore(const morkHandle& other);
  orkinStore& operator=(const morkHandle& other);

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

  static orkinStore* MakeStore(morkEnv* ev, morkStore* ioObject);

public: // utilities:

  morkEnv* CanUseStore(nsIMdbEnv* mev, mork_bool inMutable,
    mdb_err* outErr) const;

public: // type identification
  mork_bool IsOrkinStore() const
  { return mHandle_Magic == morkMagic_kStore; }

  mork_bool IsOrkinStoreHandle() const
  { return this->IsHandle() && this->IsOrkinStore(); }

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

// { ===== begin nsIMdbPort methods =====

  // { ----- begin attribute methods -----
  virtual mdb_err GetIsPortReadonly(nsIMdbEnv* ev, mdb_bool* outBool);
  virtual mdb_err GetIsStore(nsIMdbEnv* ev, mdb_bool* outBool);
  virtual mdb_err GetIsStoreAndDirty(nsIMdbEnv* ev, mdb_bool* outBool);

  virtual mdb_err GetUsagePolicy(nsIMdbEnv* ev, 
    mdbUsagePolicy* ioUsagePolicy);

  virtual mdb_err SetUsagePolicy(nsIMdbEnv* ev, 
    const mdbUsagePolicy* inUsagePolicy);
  // } ----- end attribute methods -----

  // { ----- begin memory policy methods -----  
  virtual mdb_err IdleMemoryPurge( // do memory management already scheduled
    nsIMdbEnv* ev, // context
    mdb_size* outEstimatedBytesFreed); // approximate bytes actually freed

  virtual mdb_err SessionMemoryPurge( // request specific footprint decrease
    nsIMdbEnv* ev, // context
    mdb_size inDesiredBytesFreed, // approximate number of bytes wanted
    mdb_size* outEstimatedBytesFreed); // approximate bytes actually freed

  virtual mdb_err PanicMemoryPurge( // desperately free all possible memory
    nsIMdbEnv* ev, // context
    mdb_size* outEstimatedBytesFreed); // approximate bytes actually freed
  // } ----- end memory policy methods -----

  // { ----- begin filepath methods -----
  virtual mdb_err GetPortFilePath(
    nsIMdbEnv* ev, // context
    mdbYarn* outFilePath, // name of file holding port content
    mdbYarn* outFormatVersion); // file format description

  virtual mdb_err GetPortFile(
    nsIMdbEnv* ev, // context
    nsIMdbFile** acqFile); // acquire file used by port or store
  // } ----- end filepath methods -----

  // { ----- begin export methods -----
  virtual mdb_err BestExportFormat( // determine preferred export format
    nsIMdbEnv* ev, // context
    mdbYarn* outFormatVersion); // file format description

  virtual mdb_err
  CanExportToFormat( // can export content in given specific format?
    nsIMdbEnv* ev, // context
    const char* inFormatVersion, // file format description
    mdb_bool* outCanExport); // whether ExportSource() might succeed

  virtual mdb_err ExportToFormat( // export content in given specific format
    nsIMdbEnv* ev, // context
    // const char* inFilePath, // the file to receive exported content
    nsIMdbFile* ioFile, // destination abstract file interface
    const char* inFormatVersion, // file format description
    nsIMdbThumb** acqThumb); // acquire thumb for incremental export
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the export will be finished.

  // } ----- end export methods -----

  // { ----- begin token methods -----
  virtual mdb_err TokenToString( // return a string name for an integer token
    nsIMdbEnv* ev, // context
    mdb_token inToken, // token for inTokenName inside this port
    mdbYarn* outTokenName); // the type of table to access
  
  virtual mdb_err StringToToken( // return an integer token for scope name
    nsIMdbEnv* ev, // context
    const char* inTokenName, // Latin1 string to tokenize if possible
    mdb_token* outToken); // token for inTokenName inside this port
    
  // String token zero is never used and never supported. If the port
  // is a mutable store, then StringToToken() to create a new
  // association of inTokenName with a new integer token if possible.
  // But a readonly port will return zero for an unknown scope name.

  virtual mdb_err QueryToken( // like StringToToken(), but without adding
    nsIMdbEnv* ev, // context
    const char* inTokenName, // Latin1 string to tokenize if possible
    mdb_token* outToken); // token for inTokenName inside this port
  
  // QueryToken() will return a string token if one already exists,
  // but unlike StringToToken(), will not assign a new token if not
  // already in use.

  // } ----- end token methods -----

  // { ----- begin row methods -----  
  virtual mdb_err HasRow( // contains a row with the specified oid?
    nsIMdbEnv* ev, // context
    const mdbOid* inOid,  // hypothetical row oid
    mdb_bool* outHasRow); // whether GetRow() might succeed

  virtual mdb_err GetRowRefCount( // get number of tables that contain a row 
    nsIMdbEnv* ev, // context
    const mdbOid* inOid,  // hypothetical row oid
    mdb_count* outRefCount); // number of tables containing inRowKey 
    
  virtual mdb_err GetRow( // access one row with specific oid
    nsIMdbEnv* ev, // context
    const mdbOid* inOid,  // hypothetical row oid
    nsIMdbRow** acqRow); // acquire specific row (or null)

  virtual mdb_err FindRow(nsIMdbEnv* ev, // search for row with matching cell
    mdb_scope inRowScope,   // row scope for row ids
    mdb_column inColumn,   // the column to search (and maintain an index)
    const mdbYarn* inTargetCellValue, // cell value for which to search
    mdbOid* outRowOid, // out row oid on match (or {0,-1} for no match)
    nsIMdbRow** acqRow); // acquire matching row (or nil for no match)
  // FindRow() searches for one row that has a cell in column inColumn with
  // a contained value with the same form (i.e. charset) and is byte-wise
  // identical to the blob described by yarn inTargetCellValue.  Both content
  // and form of the yarn must be an exact match to find a matching row.
  //
  // (In other words, both a yarn's blob bytes and form are significant.  The
  // form is not expected to vary in columns used for identity anyway.  This
  // is intended to make the cost of FindRow() cheaper for MDB implementors,
  // since any cell value atomization performed internally must necessarily
  // make yarn form significant in order to avoid data loss in atomization.)
  //
  // FindRow() can lazily create an index on attribute inColumn for all rows
  // with that attribute in row space scope inRowScope, so that subsequent
  // calls to FindRow() will perform faster.  Such an index might or might
  // not be persistent (but this seems desirable if it is cheap to do so).
  // Note that lazy index creation in readonly DBs is not very feasible.
  //
  // This FindRow() interface assumes that attribute inColumn is effectively
  // an alternative means of unique identification for a row in a rowspace,
  // so correct behavior is only guaranteed when no duplicates for this col
  // appear in the given set of rows.  (If more than one row has the same cell
  // value in this column, no more than one will be found; and cutting one of
  // two duplicate rows can cause the index to assume no other such row lives
  // in the row space, so future calls return nil for negative search results
  // even though some duplicate row might still live within the rowspace.)
  //
  // In other words, the FindRow() implementation is allowed to assume simple
  // hash tables mapping unqiue column keys to associated row values will be
  // sufficient, where any duplication is not recorded because only one copy
  // of a given key need be remembered.  Implementors are not required to sort
  // all rows by the specified column.
  // } ----- end row methods -----

  // { ----- begin table methods -----  
  virtual mdb_err HasTable( // supports a table with the specified oid?
    nsIMdbEnv* ev, // context
    const mdbOid* inOid,  // hypothetical table oid
    mdb_bool* outHasTable); // whether GetTable() might succeed
    
  virtual mdb_err GetTable( // access one table with specific oid
    nsIMdbEnv* ev, // context
    const mdbOid* inOid,  // hypothetical table oid
    nsIMdbTable** acqTable); // acquire specific table (or null)
  
  virtual mdb_err HasTableKind( // supports a table of the specified type?
    nsIMdbEnv* ev, // context
    mdb_scope inRowScope, // rid scope for row ids
    mdb_kind inTableKind, // the type of table to access
    mdb_count* outTableCount, // current number of such tables
    mdb_bool* outSupportsTable); // whether GetTableKind() might succeed
        
  virtual mdb_err GetTableKind( // access one (random) table of specific type
    nsIMdbEnv* ev, // context
    mdb_scope inRowScope,      // row scope for row ids
    mdb_kind inTableKind,      // the type of table to access
    mdb_count* outTableCount, // current number of such tables
    mdb_bool* outMustBeUnique, // whether port can hold only one of these
    nsIMdbTable** acqTable);       // acquire scoped collection of rows
    
  virtual mdb_err
  GetPortTableCursor( // get cursor for all tables of specific type
    nsIMdbEnv* ev, // context
    mdb_scope inRowScope, // row scope for row ids
    mdb_kind inTableKind, // the type of table to access
    nsIMdbPortTableCursor** acqCursor); // all such tables in the port
  // } ----- end table methods -----


  // { ----- begin commit methods -----

  virtual mdb_err ShouldCompress( // store wastes at least inPercentWaste?
    nsIMdbEnv* ev, // context
    mdb_percent inPercentWaste, // 0..100 percent file size waste threshold
    mdb_percent* outActualWaste, // 0..100 percent of file actually wasted
    mdb_bool* outShould); // true when about inPercentWaste% is wasted
  // ShouldCompress() returns true if the store can determine that the file
  // will shrink by an estimated percentage of inPercentWaste% (or more) if
  // CompressCommit() is called, because that percentage of the file seems
  // to be recoverable free space.  The granularity is only in terms of 
  // percentage points, and any value over 100 is considered equal to 100.
  //
  // If a store only has an approximate idea how much space might be saved
  // during a compress, then a best guess should be made.  For example, the
  // Mork implementation might keep track of how much file space began with
  // text content before the first updating transaction, and then consider
  // all content following the start of the first transaction as potentially
  // wasted space if it is all updates and not just new content.  (This is
  // a safe assumption in the sense that behavior will stabilize on a low
  // estimate of wastage after a commit removes all transaction updates.)
  //
  // Some db formats might attempt to keep a very accurate reckoning of free
  // space size, so a very accurate determination can be made.  But other db
  // formats might have difficulty determining size of free space, and might
  // require some lengthy calculation to answer.  This is the reason for
  // passing in the percentage threshold of interest, so that such lengthy
  // computations can terminate early as soon as at least inPercentWaste is
  // found, so that the entire file need not be groveled when unnecessary.
  // However, we hope implementations will always favor fast but imprecise
  // heuristic answers instead of extremely slow but very precise answers.
  //
  // If the outActualWaste parameter is non-nil, it will be used to return
  // the actual estimated space wasted as a percentage of file size.  (This
  // parameter is provided so callers need not call repeatedly with altered
  // inPercentWaste values to isolate the actual wastage figure.)  Note the
  // actual wastage figure returned can exactly equal inPercentWaste even
  // when this grossly underestimates the real figure involved, if the db
  // finds it very expensive to determine the extent of wastage after it is
  // known to at least exceed inPercentWaste.  Note we expect that whenever
  // outShould returns true, that outActualWaste returns >= inPercentWaste.
  //
  // The effect of different inPercentWaste values is not very uniform over
  // the permitted range.  For example, 50 represents 50% wastage, or a file
  // that is about double what it should be ideally.  But 99 represents 99%
  // wastage, or a file that is about ninety-nine times as big as it should
  // be ideally.  In the smaller direction, 25 represents 25% wastage, or
  // a file that is only 33% larger than it should be ideally.
  //
  // Callers can determine what policy they want to use for considering when
  // a file holds too much wasted space, and express this as a percentage
  // of total file size to pass as in the inPercentWaste parameter.  A zero
  // likely returns always trivially true, and 100 always trivially false.
  // The great majority of callers are expected to use values from 25 to 75,
  // since most plausible thresholds for compressing might fall between the
  // extremes of 133% of ideal size and 400% of ideal size.  (Presumably the
  // larger a file gets, the more important the percentage waste involved, so
  // a sliding scale for compress thresholds might use smaller numbers for
  // much bigger file sizes.)
  
  // } ----- end commit methods -----

// } ===== end nsIMdbPort methods =====

// { ===== begin nsIMdbStore methods =====

  // { ----- begin table methods -----
  virtual mdb_err NewTable( // make one new table of specific type
    nsIMdbEnv* ev, // context
    mdb_scope inRowScope,    // row scope for row ids
    mdb_kind inTableKind,    // the type of table to access
    mdb_bool inMustBeUnique, // whether store can hold only one of these
    const mdbOid* inOptionalMetaRowOid, // can be nil to avoid specifying
    nsIMdbTable** acqTable);     // acquire scoped collection of rows
    
  virtual mdb_err NewTableWithOid( // make one new table of specific type
    nsIMdbEnv* ev, // context
    const mdbOid* inOid,   // caller assigned oid
    mdb_kind inTableKind,    // the type of table to access
    mdb_bool inMustBeUnique, // whether store can hold only one of these
    const mdbOid* inOptionalMetaRowOid, // can be nil to avoid specifying
    nsIMdbTable** acqTable);     // acquire scoped collection of rows
  // } ----- end table methods -----

  // { ----- begin row scope methods -----
  virtual mdb_err RowScopeHasAssignedIds(nsIMdbEnv* ev,
    mdb_scope inRowScope,   // row scope for row ids
    mdb_bool* outCallerAssigned, // nonzero if caller assigned specified
    mdb_bool* outStoreAssigned); // nonzero if store db assigned specified

  virtual mdb_err SetCallerAssignedIds(nsIMdbEnv* ev,
    mdb_scope inRowScope,   // row scope for row ids
    mdb_bool* outCallerAssigned, // nonzero if caller assigned specified
    mdb_bool* outStoreAssigned); // nonzero if store db assigned specified

  virtual mdb_err SetStoreAssignedIds(nsIMdbEnv* ev,
    mdb_scope inRowScope,   // row scope for row ids
    mdb_bool* outCallerAssigned, // nonzero if caller assigned specified
    mdb_bool* outStoreAssigned); // nonzero if store db assigned specified
  // } ----- end row scope methods -----

  // { ----- begin row methods -----
  virtual mdb_err NewRowWithOid(nsIMdbEnv* ev, // new row w/ caller assigned oid
    const mdbOid* inOid,   // caller assigned oid
    nsIMdbRow** acqRow); // create new row

  virtual mdb_err NewRow(nsIMdbEnv* ev, // new row with db assigned oid
    mdb_scope inRowScope,   // row scope for row ids
    nsIMdbRow** acqRow); // create new row
  // Note this row must be added to some table or cell child before the
  // store is closed in order to make this row persist across sesssions.
  // } ----- end row methods -----

  // { ----- begin inport/export methods -----
  virtual mdb_err ImportContent( // import content from port
    nsIMdbEnv* ev, // context
    mdb_scope inRowScope, // scope for rows (or zero for all?)
    nsIMdbPort* ioPort, // the port with content to add to store
    nsIMdbThumb** acqThumb); // acquire thumb for incremental import
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the import will be finished.

  virtual mdb_err ImportFile( // import content from port
    nsIMdbEnv* ev, // context
    nsIMdbFile* ioFile, // the file with content to add to store
    nsIMdbThumb** acqThumb); // acquire thumb for incremental import
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the import will be finished.
  // } ----- end inport/export methods -----

  // { ----- begin hinting methods -----
  virtual mdb_err
  ShareAtomColumnsHint( // advise re shared column content atomizing
    nsIMdbEnv* ev, // context
    mdb_scope inScopeHint, // zero, or suggested shared namespace
    const mdbColumnSet* inColumnSet); // cols desired tokenized together

  virtual mdb_err
  AvoidAtomColumnsHint( // advise column with poor atomizing prospects
    nsIMdbEnv* ev, // context
    const mdbColumnSet* inColumnSet); // cols with poor atomizing prospects
  // } ----- end hinting methods -----

  // { ----- begin commit methods -----
  virtual mdb_err SmallCommit( // save minor changes if convenient and uncostly
    nsIMdbEnv* ev); // context
  
  virtual mdb_err LargeCommit( // save important changes if at all possible
    nsIMdbEnv* ev, // context
    nsIMdbThumb** acqThumb); // acquire thumb for incremental commit
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the commit will be finished.  Note the store is effectively write
  // locked until commit is finished or canceled through the thumb instance.
  // Until the commit is done, the store will report it has readonly status.

  virtual mdb_err SessionCommit( // save all changes if large commits delayed
    nsIMdbEnv* ev, // context
    nsIMdbThumb** acqThumb); // acquire thumb for incremental commit
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the commit will be finished.  Note the store is effectively write
  // locked until commit is finished or canceled through the thumb instance.
  // Until the commit is done, the store will report it has readonly status.

  virtual mdb_err
  CompressCommit( // commit and make db physically smaller if possible
    nsIMdbEnv* ev, // context
    nsIMdbThumb** acqThumb); // acquire thumb for incremental commit
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the commit will be finished.  Note the store is effectively write
  // locked until commit is finished or canceled through the thumb instance.
  // Until the commit is done, the store will report it has readonly status.
  // } ----- end commit methods -----

// } ===== end nsIMdbStore methods =====
};
 

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _ORKINSTORE_ */

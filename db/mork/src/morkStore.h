/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-  */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _MORKSTORE_
#define _MORKSTORE_ 1

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKOBJECT_
#include "morkObject.h"
#endif

#ifndef _MORKNODEMAP_
#include "morkNodeMap.h"
#endif

#ifndef _MORKPOOL_
#include "morkPool.h"
#endif

#ifndef _MORKZONE_
#include "morkZone.h"
#endif

#ifndef _MORKATOM_
#include "morkAtom.h"
#endif

#ifndef _MORKROWSPACE_
#include "morkRowSpace.h"
#endif

#ifndef _MORKATOMSPACE_
#include "morkAtomSpace.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkDerived_kPort  /*i*/ 0x7054 /* ascii 'pT' */

#define morkDerived_kStore  /*i*/ 0x7354 /* ascii 'sT' */

/*| kGroundColumnSpace: we use the 'column space' as the default scope
**| for grounding column name IDs, and this is also the default scope for
**| all other explicitly tokenized strings.
|*/
#define morkStore_kGroundColumnSpace 'c' /* for mStore_GroundColumnSpace*/
#define morkStore_kColumnSpaceScope ((mork_scope) 'c') /*kGroundColumnSpace*/
#define morkStore_kValueSpaceScope ((mork_scope) 'v')
#define morkStore_kStreamBufSize (8 * 1024) /* okay buffer size */

#define morkStore_kReservedColumnCount 0x20 /* for well-known columns */

#define morkStore_kNoneToken ((mork_token) 'n')
#define morkStore_kFormColumn ((mork_column) 'f')
#define morkStore_kAtomScopeColumn ((mork_column) 'a')
#define morkStore_kRowScopeColumn ((mork_column) 'r')
#define morkStore_kMetaScope ((mork_scope) 'm')
#define morkStore_kKindColumn ((mork_column) 'k')
#define morkStore_kStatusColumn ((mork_column) 's')

/*| morkStore: 
|*/
class morkStore :  public morkObject, public nsIMdbStore {

public: // state is public because the entire Mork system is private

  NS_DECL_ISUPPORTS_INHERITED

  morkEnv*        mPort_Env;      // non-refcounted env which created port
  morkFactory*    mPort_Factory;  // weak ref to suite factory
  nsIMdbHeap*     mPort_Heap;     // heap in which this port allocs objects
  
// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  
  void ClosePort(morkEnv* ev); // called by CloseMorkNode();

public: // dynamic type identification
  mork_bool IsPort() const
  { return IsNode() && mNode_Derived == morkDerived_kPort; }
// } ===== end morkNode methods =====

public: // other port methods

  // { ----- begin attribute methods -----
//  NS_IMETHOD IsFrozenMdbObject(nsIMdbEnv* ev, mdb_bool* outIsReadonly);
  // same as nsIMdbPort::GetIsPortReadonly() when this object is inside a port.
  // } ----- end attribute methods -----

  // { ----- begin factory methods -----
//  NS_IMETHOD GetMdbFactory(nsIMdbEnv* ev, nsIMdbFactory** acqFactory); 
  // } ----- end factory methods -----

  // { ----- begin ref counting for well-behaved cyclic graphs -----
  NS_IMETHOD GetWeakRefCount(nsIMdbEnv* ev, // weak refs
    mdb_count* outCount);  
  NS_IMETHOD GetStrongRefCount(nsIMdbEnv* ev, // strong refs
    mdb_count* outCount);

  NS_IMETHOD AddWeakRef(nsIMdbEnv* ev);
  NS_IMETHOD AddStrongRef(nsIMdbEnv* ev);

  NS_IMETHOD CutWeakRef(nsIMdbEnv* ev);
  NS_IMETHOD CutStrongRef(nsIMdbEnv* ev);
  
  NS_IMETHOD CloseMdbObject(nsIMdbEnv* ev); // called at strong refs zero
  NS_IMETHOD IsOpenMdbObject(nsIMdbEnv* ev, mdb_bool* outOpen);
  // } ----- end ref counting -----
  
// } ===== end nsIMdbObject methods =====

// { ===== begin nsIMdbPort methods =====

  // { ----- begin attribute methods -----
  NS_IMETHOD GetIsPortReadonly(nsIMdbEnv* ev, mdb_bool* outBool);
  NS_IMETHOD GetIsStore(nsIMdbEnv* ev, mdb_bool* outBool);
  NS_IMETHOD GetIsStoreAndDirty(nsIMdbEnv* ev, mdb_bool* outBool);

  NS_IMETHOD GetUsagePolicy(nsIMdbEnv* ev, 
    mdbUsagePolicy* ioUsagePolicy);

  NS_IMETHOD SetUsagePolicy(nsIMdbEnv* ev, 
    const mdbUsagePolicy* inUsagePolicy);
  // } ----- end attribute methods -----

  // { ----- begin memory policy methods -----  
  NS_IMETHOD IdleMemoryPurge( // do memory management already scheduled
    nsIMdbEnv* ev, // context
    mdb_size* outEstimatedBytesFreed); // approximate bytes actually freed

  NS_IMETHOD SessionMemoryPurge( // request specific footprint decrease
    nsIMdbEnv* ev, // context
    mdb_size inDesiredBytesFreed, // approximate number of bytes wanted
    mdb_size* outEstimatedBytesFreed); // approximate bytes actually freed

  NS_IMETHOD PanicMemoryPurge( // desperately free all possible memory
    nsIMdbEnv* ev, // context
    mdb_size* outEstimatedBytesFreed); // approximate bytes actually freed
  // } ----- end memory policy methods -----

  // { ----- begin filepath methods -----
  NS_IMETHOD GetPortFilePath(
    nsIMdbEnv* ev, // context
    mdbYarn* outFilePath, // name of file holding port content
    mdbYarn* outFormatVersion); // file format description

  NS_IMETHOD GetPortFile(
    nsIMdbEnv* ev, // context
    nsIMdbFile** acqFile); // acquire file used by port or store
  // } ----- end filepath methods -----

  // { ----- begin export methods -----
  NS_IMETHOD BestExportFormat( // determine preferred export format
    nsIMdbEnv* ev, // context
    mdbYarn* outFormatVersion); // file format description

  NS_IMETHOD
  CanExportToFormat( // can export content in given specific format?
    nsIMdbEnv* ev, // context
    const char* inFormatVersion, // file format description
    mdb_bool* outCanExport); // whether ExportSource() might succeed

  NS_IMETHOD ExportToFormat( // export content in given specific format
    nsIMdbEnv* ev, // context
    // const char* inFilePath, // the file to receive exported content
    nsIMdbFile* ioFile, // destination abstract file interface
    const char* inFormatVersion, // file format description
    nsIMdbThumb** acqThumb); // acquire thumb for incremental export
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the export will be finished.

  // } ----- end export methods -----

  // { ----- begin token methods -----
  NS_IMETHOD TokenToString( // return a string name for an integer token
    nsIMdbEnv* ev, // context
    mdb_token inToken, // token for inTokenName inside this port
    mdbYarn* outTokenName); // the type of table to access
  
  NS_IMETHOD StringToToken( // return an integer token for scope name
    nsIMdbEnv* ev, // context
    const char* inTokenName, // Latin1 string to tokenize if possible
    mdb_token* outToken); // token for inTokenName inside this port
    
  // String token zero is never used and never supported. If the port
  // is a mutable store, then StringToToken() to create a new
  // association of inTokenName with a new integer token if possible.
  // But a readonly port will return zero for an unknown scope name.

  NS_IMETHOD QueryToken( // like StringToToken(), but without adding
    nsIMdbEnv* ev, // context
    const char* inTokenName, // Latin1 string to tokenize if possible
    mdb_token* outToken); // token for inTokenName inside this port
  
  // QueryToken() will return a string token if one already exists,
  // but unlike StringToToken(), will not assign a new token if not
  // already in use.

  // } ----- end token methods -----

  // { ----- begin row methods -----  
  NS_IMETHOD HasRow( // contains a row with the specified oid?
    nsIMdbEnv* ev, // context
    const mdbOid* inOid,  // hypothetical row oid
    mdb_bool* outHasRow); // whether GetRow() might succeed

  NS_IMETHOD GetRowRefCount( // get number of tables that contain a row 
    nsIMdbEnv* ev, // context
    const mdbOid* inOid,  // hypothetical row oid
    mdb_count* outRefCount); // number of tables containing inRowKey 
    
  NS_IMETHOD GetRow( // access one row with specific oid
    nsIMdbEnv* ev, // context
    const mdbOid* inOid,  // hypothetical row oid
    nsIMdbRow** acqRow); // acquire specific row (or null)

  NS_IMETHOD FindRow(nsIMdbEnv* ev, // search for row with matching cell
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
  NS_IMETHOD HasTable( // supports a table with the specified oid?
    nsIMdbEnv* ev, // context
    const mdbOid* inOid,  // hypothetical table oid
    mdb_bool* outHasTable); // whether GetTable() might succeed
    
  NS_IMETHOD GetTable( // access one table with specific oid
    nsIMdbEnv* ev, // context
    const mdbOid* inOid,  // hypothetical table oid
    nsIMdbTable** acqTable); // acquire specific table (or null)
  
  NS_IMETHOD HasTableKind( // supports a table of the specified type?
    nsIMdbEnv* ev, // context
    mdb_scope inRowScope, // rid scope for row ids
    mdb_kind inTableKind, // the type of table to access
    mdb_count* outTableCount, // current number of such tables
    mdb_bool* outSupportsTable); // whether GetTableKind() might succeed
        
  NS_IMETHOD GetTableKind( // access one (random) table of specific type
    nsIMdbEnv* ev, // context
    mdb_scope inRowScope,      // row scope for row ids
    mdb_kind inTableKind,      // the type of table to access
    mdb_count* outTableCount, // current number of such tables
    mdb_bool* outMustBeUnique, // whether port can hold only one of these
    nsIMdbTable** acqTable);       // acquire scoped collection of rows
    
  NS_IMETHOD
  GetPortTableCursor( // get cursor for all tables of specific type
    nsIMdbEnv* ev, // context
    mdb_scope inRowScope, // row scope for row ids
    mdb_kind inTableKind, // the type of table to access
    nsIMdbPortTableCursor** acqCursor); // all such tables in the port
  // } ----- end table methods -----


  // { ----- begin commit methods -----

  NS_IMETHOD ShouldCompress( // store wastes at least inPercentWaste?
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
  NS_IMETHOD NewTable( // make one new table of specific type
    nsIMdbEnv* ev, // context
    mdb_scope inRowScope,    // row scope for row ids
    mdb_kind inTableKind,    // the type of table to access
    mdb_bool inMustBeUnique, // whether store can hold only one of these
    const mdbOid* inOptionalMetaRowOid, // can be nil to avoid specifying
    nsIMdbTable** acqTable);     // acquire scoped collection of rows
    
  NS_IMETHOD NewTableWithOid( // make one new table of specific type
    nsIMdbEnv* ev, // context
    const mdbOid* inOid,   // caller assigned oid
    mdb_kind inTableKind,    // the type of table to access
    mdb_bool inMustBeUnique, // whether store can hold only one of these
    const mdbOid* inOptionalMetaRowOid, // can be nil to avoid specifying 
    nsIMdbTable** acqTable);     // acquire scoped collection of rows
  // } ----- end table methods -----

  // { ----- begin row scope methods -----
  NS_IMETHOD RowScopeHasAssignedIds(nsIMdbEnv* ev,
    mdb_scope inRowScope,   // row scope for row ids
    mdb_bool* outCallerAssigned, // nonzero if caller assigned specified
    mdb_bool* outStoreAssigned); // nonzero if store db assigned specified

  NS_IMETHOD SetCallerAssignedIds(nsIMdbEnv* ev,
    mdb_scope inRowScope,   // row scope for row ids
    mdb_bool* outCallerAssigned, // nonzero if caller assigned specified
    mdb_bool* outStoreAssigned); // nonzero if store db assigned specified

  NS_IMETHOD SetStoreAssignedIds(nsIMdbEnv* ev,
    mdb_scope inRowScope,   // row scope for row ids
    mdb_bool* outCallerAssigned, // nonzero if caller assigned specified
    mdb_bool* outStoreAssigned); // nonzero if store db assigned specified
  // } ----- end row scope methods -----

  // { ----- begin row methods -----
  NS_IMETHOD NewRowWithOid(nsIMdbEnv* ev, // new row w/ caller assigned oid
    const mdbOid* inOid,   // caller assigned oid
    nsIMdbRow** acqRow); // create new row

  NS_IMETHOD NewRow(nsIMdbEnv* ev, // new row with db assigned oid
    mdb_scope inRowScope,   // row scope for row ids
    nsIMdbRow** acqRow); // create new row
  // Note this row must be added to some table or cell child before the
  // store is closed in order to make this row persist across sesssions.

  // } ----- end row methods -----

  // { ----- begin inport/export methods -----
  NS_IMETHOD ImportContent( // import content from port
    nsIMdbEnv* ev, // context
    mdb_scope inRowScope, // scope for rows (or zero for all?)
    nsIMdbPort* ioPort, // the port with content to add to store
    nsIMdbThumb** acqThumb); // acquire thumb for incremental import
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the import will be finished.

  NS_IMETHOD ImportFile( // import content from port
    nsIMdbEnv* ev, // context
    nsIMdbFile* ioFile, // the file with content to add to store
    nsIMdbThumb** acqThumb); // acquire thumb for incremental import
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the import will be finished.
  // } ----- end inport/export methods -----

  // { ----- begin hinting methods -----
  NS_IMETHOD
  ShareAtomColumnsHint( // advise re shared column content atomizing
    nsIMdbEnv* ev, // context
    mdb_scope inScopeHint, // zero, or suggested shared namespace
    const mdbColumnSet* inColumnSet); // cols desired tokenized together

  NS_IMETHOD
  AvoidAtomColumnsHint( // advise column with poor atomizing prospects
    nsIMdbEnv* ev, // context
    const mdbColumnSet* inColumnSet); // cols with poor atomizing prospects
  // } ----- end hinting methods -----

  // { ----- begin commit methods -----
  NS_IMETHOD SmallCommit( // save minor changes if convenient and uncostly
    nsIMdbEnv* ev); // context
  
  NS_IMETHOD LargeCommit( // save important changes if at all possible
    nsIMdbEnv* ev, // context
    nsIMdbThumb** acqThumb); // acquire thumb for incremental commit
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the commit will be finished.  Note the store is effectively write
  // locked until commit is finished or canceled through the thumb instance.
  // Until the commit is done, the store will report it has readonly status.

  NS_IMETHOD SessionCommit( // save all changes if large commits delayed
    nsIMdbEnv* ev, // context
    nsIMdbThumb** acqThumb); // acquire thumb for incremental commit
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the commit will be finished.  Note the store is effectively write
  // locked until commit is finished or canceled through the thumb instance.
  // Until the commit is done, the store will report it has readonly status.

  NS_IMETHOD
  CompressCommit( // commit and make db physically smaller if possible
    nsIMdbEnv* ev, // context
    nsIMdbThumb** acqThumb); // acquire thumb for incremental commit
  // Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
  // then the commit will be finished.  Note the store is effectively write
  // locked until commit is finished or canceled through the thumb instance.
  // Until the commit is done, the store will report it has readonly status.
  
  // } ----- end commit methods -----

// } ===== end nsIMdbStore methods =====

public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakPort(morkPort* me,
    morkEnv* ev, morkPort** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongPort(morkPort* me,
    morkEnv* ev, morkPort** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
// public: // slots inherited from morkPort (meant to inform only)
  // nsIMdbHeap*     mNode_Heap;
  // mork_able    mNode_Mutable; // can this node be modified?
  // mork_load    mNode_Load;    // is this node clean or dirty?
  // mork_base    mNode_Base;    // must equal morkBase_kNode
  // mork_derived mNode_Derived; // depends on specific node subclass
  // mork_access  mNode_Access;  // kOpen, kClosing, kShut, or kDead
  // mork_usage   mNode_Usage;   // kHeap, kStack, kMember, kGlobal, kNone
  // mork_uses    mNode_Uses;    // refcount for strong refs
  // mork_refs    mNode_Refs;    // refcount for strong refs + weak refs
 
  // morkEnv*        mPort_Env;      // non-refcounted env which created port
  // morkFactory*    mPort_Factory;  // weak ref to suite factory
  // nsIMdbHeap*     mPort_Heap;     // heap in which this port allocs objects

public: // state is public because the entire Mork system is private

// mStore_OidAtomSpace might be unnecessary; I don't remember why I wanted it.
  morkAtomSpace*   mStore_OidAtomSpace;   // ground atom space for oids
  morkAtomSpace*   mStore_GroundAtomSpace; // ground atom space for scopes
  morkAtomSpace*   mStore_GroundColumnSpace; // ground column space for scopes

  nsIMdbFile*      mStore_File; // the file containing Mork text
  morkStream*      mStore_InStream; // stream using file used by the builder
  morkBuilder*     mStore_Builder; // to parse Mork text and build structures 

  morkStream*      mStore_OutStream; // stream using file used by the writer
  
  morkRowSpaceMap  mStore_RowSpaces;  // maps mork_scope -> morkSpace
  morkAtomSpaceMap mStore_AtomSpaces; // maps mork_scope -> morkSpace
  
  morkZone         mStore_Zone;
  
  morkPool         mStore_Pool;

  // we alloc a max size book atom to reuse space for atom map key searches:
  // morkMaxBookAtom  mStore_BookAtom; // staging area for atom map searches
  
  morkFarBookAtom  mStore_FarBookAtom; // staging area for atom map searches
  
  // GroupIdentity should be one more than largest seen in a parsed db file:
  mork_gid         mStore_CommitGroupIdentity; // transaction ID number
  
  // group positions are used to help compute PercentOfStoreWasted():
  mork_pos         mStore_FirstCommitGroupPos; // start of first group
  mork_pos         mStore_SecondCommitGroupPos; // start of second group
  // If the first commit group is very near the start of the file (say less
  // than 512 bytes), then we might assume the file started nearly empty and
  // that most of the first group is not wasted.  In that case, the pos of
  // the second commit group might make a better estimate of the start of
  // transaction space that might represent wasted file space.  That's why
  // we support fields for both first and second commit group positions.
  //
  // We assume that a zero in either group pos means that the slot has not
  // yet been given a valid value, since the file will always start with a
  // tag, and a commit group cannot actually start at position zero.
  //
  // Either or both the first or second commit group positions might be
  // supplied by either morkWriter (while committing) or morkBuilder (while
  // parsing), since either reading or writing the file might encounter the
  // first transaction groups which came into existence either in the past
  // or in the very recent present.
  
  mork_bool        mStore_CanAutoAssignAtomIdentity;
  mork_bool        mStore_CanDirty; // changes imply the store becomes dirty?
  mork_u1          mStore_CanWriteIncremental; // compress not required?
  mork_u1          mStore_Pad; // for u4 alignment
  
  // mStore_CanDirty should be FALSE when parsing a file while building the
  // content going into the store, because such data structure modifications
  // are actuallly in sync with the file.  So content read from a file must
  // be clean with respect to the file.  After a file is finished parsing,
  // the mStore_CanDirty slot should become TRUE, so that any additional
  // changes at runtime cause structures to be marked dirty with respect to
  // the file which must later be updated with changes during a commit.
  //
  // It might also make sense to set mStore_CanDirty to FALSE while a commit
  // is in progress, lest some internal transformations make more content
  // appear dirty when it should not.  So anyone modifying content during a
  // commit should think about the intended significance regarding dirty.

public: // more specific dirty methods for store:
  void SetStoreDirty() { this->SetNodeDirty(); }
  void SetStoreClean() { this->SetNodeClean(); }
  
  mork_bool IsStoreClean() const { return this->IsNodeClean(); }
  mork_bool IsStoreDirty() const { return this->IsNodeDirty(); }
 
public: // setting dirty based on CanDirty:
 
  void MaybeDirtyStore()
  { if ( mStore_CanDirty ) this->SetStoreDirty(); }
  
public: // space waste analysis

  mork_percent PercentOfStoreWasted(morkEnv* ev);
 
public: // setting store and all subspaces canDirty:
 
  void SetStoreAndAllSpacesCanDirty(morkEnv* ev, mork_bool inCanDirty);

public: // building an atom inside mStore_FarBookAtom from a char* string

  morkFarBookAtom* StageAliasAsFarBookAtom(morkEnv* ev,
    const morkMid* inMid, morkAtomSpace* ioSpace, mork_cscode inForm);

  morkFarBookAtom* StageYarnAsFarBookAtom(morkEnv* ev,
    const mdbYarn* inYarn, morkAtomSpace* ioSpace);

  morkFarBookAtom* StageStringAsFarBookAtom(morkEnv* ev,
    const char* inString, mork_cscode inForm, morkAtomSpace* ioSpace);

public: // determining whether incremental writing is a good use of time:

  mork_bool DoPreferLargeOverCompressCommit(morkEnv* ev);
  // true when mStore_CanWriteIncremental && store has file large enough 

public: // lazy creation of members and nested row or atom spaces

  morkAtomSpace*   LazyGetOidAtomSpace(morkEnv* ev);
  morkAtomSpace*   LazyGetGroundAtomSpace(morkEnv* ev);
  morkAtomSpace*   LazyGetGroundColumnSpace(morkEnv* ev);

  morkStream*      LazyGetInStream(morkEnv* ev);
  morkBuilder*     LazyGetBuilder(morkEnv* ev); 
  void             ForgetBuilder(morkEnv* ev); 

  morkStream*      LazyGetOutStream(morkEnv* ev);
  
  morkRowSpace*    LazyGetRowSpace(morkEnv* ev, mdb_scope inRowScope);
  morkAtomSpace*   LazyGetAtomSpace(morkEnv* ev, mdb_scope inAtomScope);
 
// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseStore() only if open
  virtual ~morkStore(); // assert that CloseStore() executed earlier
  
public: // morkStore construction & destruction
  morkStore(morkEnv* ev, const morkUsage& inUsage,
     nsIMdbHeap* ioNodeHeap, // the heap (if any) for this node instance
     morkFactory* inFactory, // the factory for this
     nsIMdbHeap* ioPortHeap  // the heap to hold all content in the port
     );
  void CloseStore(morkEnv* ev); // called by CloseMorkNode();

private: // copying is not allowed
  morkStore(const morkStore& other);
  morkStore& operator=(const morkStore& other);

public: // dynamic type identification
  morkEnv*  CanUseStore(nsIMdbEnv* mev, mork_bool inMutable, mdb_err* outErr) const;
   mork_bool IsStore() const
  { return IsNode() && mNode_Derived == morkDerived_kStore; }
// } ===== end morkNode methods =====

public: // typing
  static void NonStoreTypeError(morkEnv* ev);
  static void NilStoreFileError(morkEnv* ev);
  static void CannotAutoAssignAtomIdentityError(morkEnv* ev);
  
public: //  store utilties
  
  morkAtom* YarnToAtom(morkEnv* ev, const mdbYarn* inYarn, PRBool createIfMissing = PR_TRUE);
  morkAtom* AddAlias(morkEnv* ev, const morkMid& inMid,
    mork_cscode inForm);

public: // other store methods

  void RenumberAllCollectableContent(morkEnv* ev);

  nsIMdbStore* AcquireStoreHandle(morkEnv* ev); // mObject_Handle

  morkPool* StorePool() { return &mStore_Pool; }

  mork_bool OpenStoreFile(morkEnv* ev, // return value equals ev->Good()
    mork_bool inFrozen,
    // const char* inFilePath,
    nsIMdbFile* ioFile, // db abstract file interface
    const mdbOpenPolicy* inOpenPolicy);

  mork_bool CreateStoreFile(morkEnv* ev, // return value equals ev->Good()
    // const char* inFilePath,
    nsIMdbFile* ioFile, // db abstract file interface
    const mdbOpenPolicy* inOpenPolicy);
    
  morkAtom* CopyAtom(morkEnv* ev, const morkAtom* inAtom);
  // copy inAtom (from some other store) over to this store
    
  mork_token CopyToken(morkEnv* ev, mdb_token inToken, morkStore* inStore);
  // copy inToken from inStore over to this store
    
  mork_token BufToToken(morkEnv* ev, const morkBuf* inBuf);
  mork_token StringToToken(morkEnv* ev, const char* inTokenName);
  mork_token QueryToken(morkEnv* ev, const char* inTokenName);
  void TokenToString(morkEnv* ev, mdb_token inToken, mdbYarn* outTokenName);
  
  mork_bool MidToOid(morkEnv* ev, const morkMid& inMid,
     mdbOid* outOid);
  mork_bool OidToYarn(morkEnv* ev, const mdbOid& inOid, mdbYarn* outYarn);
  mork_bool MidToYarn(morkEnv* ev, const morkMid& inMid,
     mdbYarn* outYarn);

  morkBookAtom* MidToAtom(morkEnv* ev, const morkMid& inMid);
  morkRow* MidToRow(morkEnv* ev, const morkMid& inMid);
  morkTable* MidToTable(morkEnv* ev, const morkMid& inMid);
  
  morkRow* OidToRow(morkEnv* ev, const mdbOid* inOid);
  // OidToRow() finds old row with oid, or makes new one if not found.

  morkTable* OidToTable(morkEnv* ev, const mdbOid* inOid,
    const mdbOid* inOptionalMetaRowOid);
  // OidToTable() finds old table with oid, or makes new one if not found.
  
  static void SmallTokenToOneByteYarn(morkEnv* ev, mdb_token inToken,
    mdbYarn* outYarn);
  
  mork_bool HasTableKind(morkEnv* ev, mdb_scope inRowScope, 
    mdb_kind inTableKind, mdb_count* outTableCount);
  
  morkTable* GetTableKind(morkEnv* ev, mdb_scope inRowScope, 
    mdb_kind inTableKind, mdb_count* outTableCount,
    mdb_bool* outMustBeUnique);

  morkRow* FindRow(morkEnv* ev, mdb_scope inScope, mdb_column inColumn,
    const mdbYarn* inTargetCellValue);
  
  morkRow* GetRow(morkEnv* ev, const mdbOid* inOid);
  morkTable* GetTable(morkEnv* ev, const mdbOid* inOid);
    
  morkTable* NewTable(morkEnv* ev, mdb_scope inRowScope,
    mdb_kind inTableKind, mdb_bool inMustBeUnique,
    const mdbOid* inOptionalMetaRowOid);

  morkPortTableCursor* GetPortTableCursor(morkEnv* ev, mdb_scope inRowScope,
    mdb_kind inTableKind) ;

  morkRow* NewRowWithOid(morkEnv* ev, const mdbOid* inOid);
  morkRow* NewRow(morkEnv* ev, mdb_scope inRowScope);

  morkThumb* MakeCompressCommitThumb(morkEnv* ev, mork_bool inDoCollect);

public: // commit related methods

  mork_bool MarkAllStoreContentDirty(morkEnv* ev);
  // MarkAllStoreContentDirty() visits every object in the store and marks 
  // them dirty, including every table, row, cell, and atom.  The return
  // equals ev->Good(), to show whether any error happened.  This method is
  // intended for use in the beginning of a "compress commit" which writes
  // all store content, whether dirty or not.  We dirty everything first so
  // that later iterations over content can mark things clean as they are
  // written, and organize the process of serialization so that objects are
  // written only at need (because of being dirty).

public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakStore(morkStore* me,
    morkEnv* ev, morkStore** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongStore(morkStore* me,
    morkEnv* ev, morkStore** ioSlot)
  { 
    morkStore* store = *ioSlot;
    if ( me != store )
    {
      if ( store )
      {
        // what if this nulls out the ev and causes asserts?
        // can we move this after the CutStrongRef()?
        *ioSlot = 0;
        store->Release();
      }
      if ( me && me->AddRef() )
        *ioSlot = me;
    }
  }
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKSTORE_ */


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

/*| morkPort: 
|*/
class morkPort : public morkObject { // private mork port

// public: // slots inherited from morkObject (meant to inform only)
  // nsIMdbHeap*     mNode_Heap;
  // mork_able    mNode_Mutable; // can this node be modified?
  // mork_load    mNode_Load;    // is this node clean or dirty?
  // mork_base    mNode_Base;    // must equal morkBase_kNode
  // mork_derived mNode_Derived; // depends on specific node subclass
  // mork_access  mNode_Access;  // kOpen, kClosing, kShut, or kDead
  // mork_usage   mNode_Usage;   // kHeap, kStack, kMember, kGlobal, kNone
  // mork_uses    mNode_Uses;    // refcount for strong refs
  // mork_refs    mNode_Refs;    // refcount for strong refs + weak refs

  // mork_color   mBead_Color;   // ID for this bead
  // morkHandle*  mObject_Handle;  // weak ref to handle for this object

public: // state is public because the entire Mork system is private
  morkEnv*        mPort_Env;      // non-refcounted env which created port
  morkFactory*    mPort_Factory;  // weak ref to suite factory
  nsIMdbHeap*     mPort_Heap;     // heap in which this port allocs objects
  
// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // ClosePort() only if open
  virtual ~morkPort(); // assert that ClosePort() executed earlier
  
public: // morkPort construction & destruction
  morkPort(morkEnv* ev, const morkUsage& inUsage,
     nsIMdbHeap* ioNodeHeap, // the heap (if any) for this node instance
     morkFactory* inFactory, // the factory for this
     nsIMdbHeap* ioPortHeap  // the heap to hold all content in the port
     );
  void ClosePort(morkEnv* ev); // called by CloseMorkNode();

private: // copying is not allowed
  morkPort(const morkPort& other);
  morkPort& operator=(const morkPort& other);

public: // dynamic type identification
  mork_bool IsPort() const
  { return IsNode() && mNode_Derived == morkDerived_kPort; }
// } ===== end morkNode methods =====

public: // other port methods


public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakPort(morkPort* me,
    morkEnv* ev, morkPort** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongPort(morkPort* me,
    morkEnv* ev, morkPort** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};

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
class morkStore : public morkPort {

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
  mork_bool IsStore() const
  { return IsNode() && mNode_Derived == morkDerived_kStore; }
// } ===== end morkNode methods =====

public: // typing
  static void NonStoreTypeError(morkEnv* ev);
  static void NilStoreFileError(morkEnv* ev);
  static void CannotAutoAssignAtomIdentityError(morkEnv* ev);
  
public: //  store utilties
  
  morkAtom* YarnToAtom(morkEnv* ev, const mdbYarn* inYarn);
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
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKSTORE_ */


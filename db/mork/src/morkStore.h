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

  // morkHandle*      mObject_Handle;   // weak ref to handle for this object

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
#define morkStore_kGroundAtomSpace 'a' /* for mStore_GroundAtomSpace*/
#define morkStore_kStreamBufSize (8 * 1024) /* okay buffer size */

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

  morkFile*        mStore_File; // the file containing Mork text
  morkStream*      mStore_InStream; // stream using file used by the builder
  morkBuilder*     mStore_Builder; // to parse Mork text and build structures 

  morkStream*      mStore_OutStream; // stream using file used by the writer
  
  mork_token       mStore_MorkNoneToken; // token for "mork:none"
  mork_column      mStore_CharsetToken; // token for "charset"
  mork_column      mStore_AtomScopeToken; // token for "atomScope"
  mork_column      mStore_RowScopeToken; // token for "rowScope"
  mork_column      mStore_TableScopeToken; // token for "tableScope"
  mork_column      mStore_ColumnScopeToken; // token for "columnScope"
  mork_kind        mStore_TableKindToken; // token for "tableKind"

  morkRowSpaceMap  mStore_RowSpaces;  // maps mork_scope -> morkSpace
  morkAtomSpaceMap mStore_AtomSpaces; // maps mork_scope -> morkSpace
  
  morkPool         mStore_Pool;

  // we alloc a max size book atom to reuse space for atom map key searches:
  morkMaxBookAtom  mStore_BookAtom; // staging area for atom map searches
 
public: // coping with any changes to store token slots above:
 
  void SyncTokenIdChange(morkEnv* ev, const morkBookAtom* inAtom,
    const mdbOid* inOid);

public: // building an atom inside mStore_BookAtom from a char* string

  morkMaxBookAtom* StageAliasAsBookAtom(morkEnv* ev,
    const morkMid* inMid, morkAtomSpace* ioSpace, mork_cscode inForm);

  morkMaxBookAtom* StageYarnAsBookAtom(morkEnv* ev,
    const mdbYarn* inYarn, morkAtomSpace* ioSpace);

  morkMaxBookAtom* StageStringAsBookAtom(morkEnv* ev,
    const char* inString, mork_cscode inForm, morkAtomSpace* ioSpace);
  // StageStringAsBookAtom() returns &mStore_BookAtom if inString is small
  // enough, such that strlen(inString) < morkBookAtom_kMaxBodySize.  And
  // content inside mStore_BookAtom will be the valid atom format for
  // inString. This method is the standard way to stage a string as an
  // atom for searching or adding new atoms into an atom space hash table.

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
    const char* inFilePath,
    const mdbOpenPolicy* inOpenPolicy);

  mork_bool CreateStoreFile(morkEnv* ev, // return value equals ev->Good()
    const char* inFilePath,
    const mdbOpenPolicy* inOpenPolicy);
    
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

  morkTable* OidToTable(morkEnv* ev, const mdbOid* inOid);
  // OidToTable() finds old table with oid, or makes new one if not found.
  
  static void SmallTokenToOneByteYarn(morkEnv* ev, mdb_token inToken,
    mdbYarn* outYarn);
  
  mork_bool HasTableKind(morkEnv* ev, mdb_scope inRowScope, 
    mdb_kind inTableKind, mdb_count* outTableCount);
  
  morkTable* GetTableKind(morkEnv* ev, mdb_scope inRowScope, 
    mdb_kind inTableKind, mdb_count* outTableCount,
    mdb_bool* outMustBeUnique);
  
  morkRow* GetRow(morkEnv* ev, const mdbOid* inOid);
  morkTable* GetTable(morkEnv* ev, const mdbOid* inOid);
    
  morkTable* NewTable(morkEnv* ev, mdb_scope inRowScope,
    mdb_kind inTableKind, mdb_bool inMustBeUnique);

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

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

#ifndef _MORKHANDLE_
#define _MORKHANDLE_ 1

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

#ifndef _MORKDEQUE_
#include "morkDeque.h"
#endif

#ifndef _MORKPOOL_
#include "morkPool.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

class morkPool;
class morkObject;
class morkFactory;

#define morkDerived_kHandle     /*i*/ 0x486E /* ascii 'Hn' */
#define morkHandle_kTag   0x68416E44 /* ascii 'hAnD' */

/*| morkHandle: 
|*/
class morkHandle : public morkNode {
  
// public: // slots inherited from morkNode (meant to inform only)
  // nsIMdbHeap*    mNode_Heap;

  // mork_base      mNode_Base;     // must equal morkBase_kNode
  // mork_derived   mNode_Derived;  // depends on specific node subclass
  
  // mork_access    mNode_Access;   // kOpen, kClosing, kShut, or kDead
  // mork_usage     mNode_Usage;    // kHeap, kStack, kMember, kGlobal, kNone
  // mork_able      mNode_Mutable;  // can this node be modified?
  // mork_load      mNode_Load;     // is this node clean or dirty?
  
  // mork_uses      mNode_Uses;     // refcount for strong refs
  // mork_refs      mNode_Refs;     // refcount for strong refs + weak refs

public: // state is public because the entire Mork system is private

  mork_u4         mHandle_Tag;     // must equal morkHandle_kTag
  morkEnv*        mHandle_Env;     // pool that allocated this handle
  morkHandleFace* mHandle_Face;    // cookie from pool containing this
  morkObject*     mHandle_Object;  // object this handle wraps for MDB API
  mork_magic      mHandle_Magic;   // magic sig different in each subclass
  
// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseHandle() only if open
  virtual ~morkHandle(); // assert that CloseHandle() executed earlier
  
public: // morkHandle construction & destruction
  morkHandle(morkEnv* ev, // note morkUsage is always morkUsage_kPool
    morkHandleFace* ioFace,  // must not be nil, cookie for this handle
    morkObject* ioObject,    // must not be nil, the object for this handle
    mork_magic inMagic);     // magic sig to denote specific subclass
  void CloseHandle(morkEnv* ev); // called by CloseMorkNode();

private: // copying is not allowed
  morkHandle(const morkHandle& other);
  morkHandle& operator=(const morkHandle& other);
  
protected: // special case empty construction for morkHandleFrame
  friend class morkHandleFrame;
  morkHandle() { }

public: // dynamic type identification
  mork_bool IsHandle() const
  { return IsNode() && mNode_Derived == morkDerived_kHandle; }
// } ===== end morkNode methods =====

public: // morkHandle memory management operators
  void* operator new(size_t inSize, morkPool& ioPool, morkZone& ioZone, morkEnv* ev)
  { return ioPool.NewHandle(ev, inSize, &ioZone); }
  
  void* operator new(size_t inSize, morkPool& ioPool, morkEnv* ev)
  { return ioPool.NewHandle(ev, inSize, (morkZone*) 0); }
  
  void* operator new(size_t inSize, morkHandleFace* ioFace)
  { MORK_USED_1(inSize); return ioFace; }
  
  void operator delete(void* ioAddress)
  { morkNode::OnDeleteAssert(ioAddress); }
  // do NOT call delete on morkHandle instances.  They are collected.

public: // other handle methods
  mork_bool GoodHandleTag() const
  { return mHandle_Tag == morkHandle_kTag; }
  
  void NewBadMagicHandleError(morkEnv* ev, mork_magic inMagic) const;
  void NewDownHandleError(morkEnv* ev) const;
  void NilFactoryError(morkEnv* ev) const;
  void NilHandleObjectError(morkEnv* ev) const;
  void NonNodeObjectError(morkEnv* ev) const;
  void NonOpenObjectError(morkEnv* ev) const;
  
  morkObject* GetGoodHandleObject(morkEnv* ev,
    mork_bool inMutable, mork_magic inMagicType, mork_bool inClosedOkay) const;

public: // interface supporting mdbObject methods

  morkEnv* CanUseHandle(nsIMdbEnv* mev, mork_bool inMutable,
    mork_bool inClosedOkay, mdb_err* outErr) const;
    
  // { ----- begin mdbObject style methods -----
  mdb_err Handle_IsFrozenMdbObject(nsIMdbEnv* ev, mdb_bool* outIsReadonly);

  mdb_err Handle_GetMdbFactory(nsIMdbEnv* ev, nsIMdbFactory** acqFactory); 
  mdb_err Handle_GetWeakRefCount(nsIMdbEnv* ev,  mdb_count* outCount);  
  mdb_err Handle_GetStrongRefCount(nsIMdbEnv* ev, mdb_count* outCount);

  mdb_err Handle_AddWeakRef(nsIMdbEnv* ev);
  mdb_err Handle_AddStrongRef(nsIMdbEnv* ev);

  mdb_err Handle_CutWeakRef(nsIMdbEnv* ev);
  mdb_err Handle_CutStrongRef(nsIMdbEnv* ev);
  
  mdb_err Handle_CloseMdbObject(nsIMdbEnv* ev);
  mdb_err Handle_IsOpenMdbObject(nsIMdbEnv* ev, mdb_bool* outOpen);
  // } ----- end mdbObject style methods -----

public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakHandle(morkHandle* me,
    morkEnv* ev, morkHandle** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongHandle(morkHandle* me,
    morkEnv* ev, morkHandle** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};

#define morkHandleFrame_kPadSlotCount 4

/*| morkHandleFrame: an object format used for allocating and maintaining
**| instances of morkHandle, with leading slots used to maintain this in a
**| linked list, and following slots to provide extra footprint that might
**| be needed by any morkHandle subclasses that include very little extra
**| space (by virtue of the fact that each morkHandle subclass is expected
**| to multiply inherit from another base class that has only abstact methods
**| for space overhead related only to some vtable representation).
|*/
class morkHandleFrame {
public:
  morkLink    mHandleFrame_Link;    // list storage without trampling Handle
  morkHandle  mHandleFrame_Handle;
  mork_ip     mHandleFrame_Padding[ morkHandleFrame_kPadSlotCount ];
  
public:
  morkHandle*  AsHandle() { return &mHandleFrame_Handle; }
  
  morkHandleFrame() {}  // actually, morkHandleFrame never gets constructed

private: // copying is not allowed
  morkHandleFrame(const morkHandleFrame& other);
  morkHandleFrame& operator=(const morkHandleFrame& other);
};

#define morkHandleFrame_kHandleOffset \
  mork_OffsetOf(morkHandleFrame,mHandleFrame_Handle)
  
#define morkHandle_AsHandleFrame(h) ((h)->mHandle_Block , \
 ((morkHandleFrame*) (((mork_u1*)(h)) - morkHandleFrame_kHandleOffset)))

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKHANDLE_ */

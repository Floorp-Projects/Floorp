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

#ifndef _MORKENV_
#define _MORKENV_ 1

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKOBJECT_
#include "morkObject.h"
#endif

#ifndef _MORKPOOL_
#include "morkPool.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkDerived_kEnv     /*i*/ 0x4576 /* ascii 'Ev' */

#define morkEnv_kNoError         0 /* no error has happened */

#define morkEnv_kGenericError    1 /* non-specific error code */
#define morkEnv_kNonEnvTypeError 2 /* morkEnv::IsEnv() is false */

#define morkEnv_kStubMethodOnlyError 3
#define morkEnv_kOutOfMemoryError    4
#define morkEnv_kNilPointerError     5
#define morkEnv_kNewNonEnvError      6
#define morkEnv_kNilEnvSlotError     7

#define morkEnv_kBadFactoryError     8
#define morkEnv_kBadFactoryEnvError  9
#define morkEnv_kBadEnvError         10

#define morkEnv_kNonHandleTypeError  11
#define morkEnv_kNonOpenNodeError    12

#define morkEnv_kWeakRefCountEnvBonus 16 /* try to leak all env instances */

/*| morkEnv:
|*/
class morkEnv : public morkObject {

// public: // slots inherited from morkObject (meant to inform only)
  // nsIMdbHeap*       mNode_Heap;

  // mork_base      mNode_Base;     // must equal morkBase_kNode
  // mork_derived   mNode_Derived;  // depends on specific node subclass
  
  // mork_access    mNode_Access;   // kOpen, kClosing, kShut, or kDead
  // mork_usage     mNode_Usage;    // kHeap, kStack, kMember, kGlobal, kNone
  // mork_able      mNode_Mutable;  // can this node be modified?
  // mork_load      mNode_Load;     // is this node clean or dirty?
  
  // mork_uses      mNode_Uses;     // refcount for strong refs
  // mork_refs      mNode_Refs;     // refcount for strong refs + weak refs

  // morkHandle*    mObject_Handle;   // weak ref to handle for this object

public: // state is public because the entire Mork system is private
  
  morkFactory*      mEnv_Factory;  // NON-refcounted factory
  nsIMdbHeap*       mEnv_Heap;     // NON-refcounted heap

  nsIMdbEnv*        mEnv_SelfAsMdbEnv;
  nsIMdbErrorHook*  mEnv_ErrorHook;
  
  morkPool*         mEnv_HandlePool; // pool for re-using handles
    
  mork_u2           mEnv_ErrorCount; 
  mork_u2           mEnv_WarningCount; 
  
  mork_u4           mEnv_ErrorCode; // simple basis for mdb_err style errors
  
  mork_bool         mEnv_DoTrace;
  mork_able         mEnv_AutoClear;
  mork_bool         mEnv_ShouldAbort;
  mork_bool         mEnv_Pad;
  
// { ===== begin morkNode interface =====
public: // morkNode virtual methods
  virtual void CloseMorkNode(morkEnv* ev); // CloseEnv() only if open
  virtual ~morkEnv(); // assert that CloseEnv() executed earlier
  
public: // morkEnv construction & destruction
  morkEnv(const morkUsage& inUsage, nsIMdbHeap* ioHeap,
    morkFactory* ioFactory, nsIMdbHeap* ioSlotHeap);
  morkEnv(morkEnv* ev, const morkUsage& inUsage, nsIMdbHeap* ioHeap, 
     nsIMdbEnv* inSelfAsMdbEnv, morkFactory* ioFactory,
     nsIMdbHeap* ioSlotHeap);
  void CloseEnv(morkEnv* ev); // called by CloseMorkNode();

private: // copying is not allowed
  morkEnv(const morkEnv& other);
  morkEnv& operator=(const morkEnv& other);

public: // dynamic type identification
  mork_bool IsEnv() const
  { return IsNode() && mNode_Derived == morkDerived_kEnv; }
// } ===== end morkNode methods =====

public: // utility env methods

  mork_u1 HexToByte(mork_ch inFirstHex, mork_ch inSecondHex);

  mork_size TokenAsHex(void* outBuf, mork_token inToken);
  // TokenAsHex() is the same as sprintf(outBuf, "%lX", (long) inToken);
 
  mork_size OidAsHex(void* outBuf, const mdbOid& inOid);
  // sprintf(buf, "%lX:^%lX", (long) inOid.mOid_Id, (long) inOid.mOid_Scope);
 
  char* CopyString(nsIMdbHeap* ioHeap, const char* inString);
  void  FreeString(nsIMdbHeap* ioHeap, char* ioString);

public: // other env methods

  nsIMdbEnv* AcquireEnvHandle(morkEnv* ev); // mObject_Handle

  // alloc and free individual handles in mEnv_HandlePool:
  morkHandleFace*  NewHandle(mork_size inSize)
  { return mEnv_HandlePool->NewHandle(this, inSize); }
  
  void ZapHandle(morkHandleFace* ioHandle)
  { mEnv_HandlePool->ZapHandle(this, ioHandle); }

  void EnableAutoClear() { mEnv_AutoClear = morkAble_kEnabled; }
  void DisableAutoClear() { mEnv_AutoClear = morkAble_kDisabled; }
  
  mork_bool DoAutoClear() const
  { return mEnv_AutoClear == morkAble_kEnabled; }

  void NewErrorAndCode(const char* inString, mork_u2 inCode);
  void NewError(const char* inString);
  void NewWarning(const char* inString);

  void ClearMorkErrorsAndWarnings(); // clear both errors & warnings
  void AutoClearMorkErrorsAndWarnings(); // clear if auto is enabled
  
  void StubMethodOnlyError();
  void OutOfMemoryError();
  void NilPointerError();
  void NewNonEnvError();
  void NilEnvSlotError();
    
  void NonEnvTypeError(morkEnv* ev);
  
  // canonical env convenience methods to check for presence of errors:
  mork_bool Good() const { return ( mEnv_ErrorCount == 0 ); }
  mork_bool Bad() const { return ( mEnv_ErrorCount != 0 ); }
  
  nsIMdbEnv* AsMdbEnv() { return mEnv_SelfAsMdbEnv; }
  static morkEnv* FromMdbEnv(nsIMdbEnv* ioEnv); // dynamic type checking
  
  mork_u4 ErrorCode() const { return mEnv_ErrorCode; }
  
  mdb_err AsErr() const { return (mdb_err) mEnv_ErrorCode; }
  //mdb_err AsErr() const { return (mdb_err) ( mEnv_ErrorCount != 0 ); }

public: // typesafe refcounting inlines calling inherited morkNode methods
  static void SlotWeakEnv(morkEnv* me,
    morkEnv* ev, morkEnv** ioSlot)
  { morkNode::SlotWeakNode((morkNode*) me, ev, (morkNode**) ioSlot); }
  
  static void SlotStrongEnv(morkEnv* me,
    morkEnv* ev, morkEnv** ioSlot)
  { morkNode::SlotStrongNode((morkNode*) me, ev, (morkNode**) ioSlot); }
};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _MORKENV_ */

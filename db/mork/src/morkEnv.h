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

// sean was here
#include "nsError.h"

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#define morkDerived_kEnv     /*i*/ 0x4576 /* ascii 'Ev' */

// use NS error codes to make Mork easier to use with the rest of mozilla 
#define morkEnv_kNoError         NS_SUCCEEDED /* no error has happened */
#define morkEnv_kGenericError    NS_ERROR_FAILURE /* non-specific error code */
#define morkEnv_kNonEnvTypeError NS_ERROR_FAILURE /* morkEnv::IsEnv() is false */

#define morkEnv_kStubMethodOnlyError NS_ERROR_NO_INTERFACE
#define morkEnv_kOutOfMemoryError    NS_ERROR_OUT_OF_MEMORY
#define morkEnv_kNilPointerError     NS_ERROR_NULL_POINTER
#define morkEnv_kNewNonEnvError      NS_ERROR_FAILURE 
#define morkEnv_kNilEnvSlotError     NS_ERROR_FAILURE

#define morkEnv_kBadFactoryError     NS_ERROR_FACTORY_NOT_LOADED
#define morkEnv_kBadFactoryEnvError  NS_ERROR_FACTORY_NOT_LOADED
#define morkEnv_kBadEnvError         NS_ERROR_FAILURE

#define morkEnv_kNonHandleTypeError  NS_ERROR_FAILURE
#define morkEnv_kNonOpenNodeError    NS_ERROR_FAILURE 


#define morkEnv_kWeakRefCountEnvBonus 0 /* try NOT to leak all env instances */

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

  // mork_color   mBead_Color;   // ID for this bead
  // morkHandle*  mObject_Handle;  // weak ref to handle for this object

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
  mork_bool         mEnv_BeVerbose;
  
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
  void  StringToYarn(const char* inString, mdbYarn* outYarn);

public: // other env methods

  nsIMdbEnv* AcquireEnvHandle(morkEnv* ev); // mObject_Handle

  // alloc and free individual handles in mEnv_HandlePool:
  morkHandleFace*  NewHandle(mork_size inSize)
  { return mEnv_HandlePool->NewHandle(this, inSize, (morkZone*) 0); }
  
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
  void NilPointerWarning();
  void CantMakeWhenBadError();
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

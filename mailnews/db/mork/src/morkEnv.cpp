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

#ifndef _MDB_
#include "mdb.h"
#endif

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

#ifndef _MORKCH_
#include "morkCh.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _ORKINENV_
#include "orkinEnv.h"
#endif

#ifndef _MORKFACTORY_
#include "morkFactory.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkEnv::CloseMorkNode(morkEnv* ev) /*i*/ // CloseEnv() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseEnv(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkEnv::~morkEnv() /*i*/ // assert CloseEnv() executed earlier
{
  MORK_ASSERT(mEnv_SelfAsMdbEnv==0);
  MORK_ASSERT(mEnv_ErrorHook==0);
}

/*public non-poly*/
morkEnv::morkEnv(const morkUsage& inUsage, nsIMdbHeap* ioHeap,
  morkFactory* ioFactory, nsIMdbHeap* ioSlotHeap)
: morkObject(inUsage, ioHeap)
, mEnv_Factory( ioFactory )
, mEnv_Heap( ioSlotHeap )

, mEnv_SelfAsMdbEnv( 0 )
, mEnv_ErrorHook( 0 )
, mEnv_HandlePool( 0 )
  
, mEnv_ErrorCount( 0 ) 
, mEnv_WarningCount( 0 ) 

, mEnv_ErrorCode( 0 )
  
, mEnv_DoTrace( morkBool_kFalse )
, mEnv_AutoClear( morkAble_kDisabled )
, mEnv_ShouldAbort( morkBool_kFalse )
{
  MORK_ASSERT(ioSlotHeap && ioFactory );
  if ( ioSlotHeap )
  {
    // mEnv_Heap is NOT refcounted:
    // nsIMdbHeap_SlotStrongHeap(ioSlotHeap, this, &mEnv_Heap);
    
    mEnv_HandlePool = new morkPool(morkUsage::kGlobal,
      (nsIMdbHeap*) 0, ioSlotHeap);
      
    MORK_ASSERT(mEnv_HandlePool);
    if ( mEnv_HandlePool && this->Good() )
    {
      mNode_Derived = morkDerived_kEnv;
      mNode_Refs += morkEnv_kWeakRefCountEnvBonus;
    }
  }
}

/*public non-poly*/
morkEnv::morkEnv(morkEnv* ev, /*i*/
  const morkUsage& inUsage, nsIMdbHeap* ioHeap, nsIMdbEnv* inSelfAsMdbEnv,
  morkFactory* ioFactory, nsIMdbHeap* ioSlotHeap)
: morkObject(ev, inUsage, ioHeap, (morkHandle*) 0)
, mEnv_Factory( ioFactory )
, mEnv_Heap( ioSlotHeap )

, mEnv_SelfAsMdbEnv( inSelfAsMdbEnv )
, mEnv_ErrorHook( 0 )
, mEnv_HandlePool( 0 )
  
, mEnv_ErrorCount( 0 ) 
, mEnv_WarningCount( 0 ) 

, mEnv_ErrorCode( 0 )
  
, mEnv_DoTrace( morkBool_kFalse )
, mEnv_AutoClear( morkAble_kDisabled )
{
  // $$$ do we need to refcount the inSelfAsMdbEnv nsIMdbEnv??
  
  if ( ioFactory && inSelfAsMdbEnv && ioSlotHeap)
  {
    // mEnv_Heap is NOT refcounted:
    // nsIMdbHeap_SlotStrongHeap(ioSlotHeap, ev, &mEnv_Heap);

    mEnv_HandlePool = new(*ioSlotHeap, ev) morkPool(ev, 
      morkUsage::kHeap, ioSlotHeap, ioSlotHeap);
      
    MORK_ASSERT(mEnv_HandlePool);
    if ( mEnv_HandlePool && ev->Good() )
    {
      mNode_Derived = morkDerived_kEnv;
      mNode_Refs += morkEnv_kWeakRefCountEnvBonus;
    }
  }
  else
    ev->NilPointerError();
}

/*public non-poly*/ void
morkEnv::CloseEnv(morkEnv* ev) /*i*/ // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      // $$$ release mEnv_SelfAsMdbEnv??
      // $$$ release mEnv_ErrorHook??
      
      mEnv_SelfAsMdbEnv = 0;
      mEnv_ErrorHook = 0;

      // mEnv_Factory is NOT refcounted
      
      this->MarkShut();
    }
    else
      this->NonNodeError(ev);
  }
  else
    ev->NilPointerError();
}

// } ===== end morkNode methods =====
// ````` ````` ````` ````` ````` 

mork_size
morkEnv::OidAsHex(void* outBuf, const mdbOid& inOid)
// sprintf(buf, "%lX:^%lX", (long) inOid.mOid_Id, (long) inOid.mOid_Scope);
{
  mork_u1* p = (mork_u1*) outBuf;
  mork_size outSize = this->TokenAsHex(p, inOid.mOid_Id);
  p += outSize;
  *p++ = ':';
  
  mork_scope scope = inOid.mOid_Scope;
  if ( scope < 0x80 && morkCh_IsName((mork_ch) scope) )
  {
	  *p++ = (mork_u1) scope;
	  *p = 0; // null termination
	  outSize += 2;
  }
  else
  {
	  *p++ = '^';
	  mork_size scopeSize = this->TokenAsHex(p, scope);
	  outSize += scopeSize + 2;
  }
  return outSize;
}


mork_u1
morkEnv::HexToByte(mork_ch inFirstHex, mork_ch inSecondHex)
{
  mork_u1 hi = 0; // high four hex bits
  mork_flags f = morkCh_GetFlags(inFirstHex);
  if ( morkFlags_IsDigit(f) )
    hi = inFirstHex - '0';
  else if ( morkFlags_IsUpper(f) )
    hi = (inFirstHex - 'A') + 10;
  else if ( morkFlags_IsLower(f) )
    hi = (inFirstHex - 'a') + 10;
  
  mork_u1 lo = 0; // low four hex bits
  f = morkCh_GetFlags(inSecondHex);
  if ( morkFlags_IsDigit(f) )
    lo = inSecondHex - '0';
  else if ( morkFlags_IsUpper(f) )
    lo = (inSecondHex - 'A') + 10;
  else if ( morkFlags_IsLower(f) )
    lo = (inSecondHex - 'a') + 10;
    
  return (mork_u1) ((hi << 4) | lo);
}

mork_size
morkEnv::TokenAsHex(void* outBuf, mork_token inToken)
  // TokenAsHex() is the same as sprintf(outBuf, "%lX", (long) inToken);
{
  static const char* morkEnv_kHexDigits = "0123456789ABCDEF";
  char* p = (char*) outBuf;
  char* end = p + 32; // write no more than 32 digits for safety
  if ( inToken )
  {
    // first write all the hex digits in backwards order:
    while ( p < end && inToken ) // more digits to write?
    {
      *p++ = morkEnv_kHexDigits[ inToken & 0x0F ]; // low four bits
      inToken >>= 4; // we fervently hope this does not sign extend
    }
    *p = 0; // end the string with a null byte
    char* s = (char*) outBuf; // first byte in string
    mork_size size = p - s; // distance from start

    // now reverse the string in place:
    // note that p starts on the null byte, so we need predecrement:
    while ( --p > s ) // need to swap another byte in the string?
    {
      char c = *p; // temp for swap
      *p = *s;
      *s++ = c; // move s forward here, and p backward in the test
    }
    return size;
  }
  else // special case for zero integer
  {
    *p++ = '0'; // write a zero digit
    *p = 0; // end with a null byte
    return 1; // one digit in hex representation
  }
}

char*
morkEnv::CopyString(nsIMdbHeap* ioHeap, const char* inString)
{
  char* outString = 0;
  if ( ioHeap && inString )
  {
    mork_size size = MORK_STRLEN(inString) + 1;
    ioHeap->Alloc(this->AsMdbEnv(), size, (void**) &outString);
    if ( outString )
      MORK_STRCPY(outString, inString);
  }
  else
    this->NilPointerError();
  return outString;
}

void
morkEnv::FreeString(nsIMdbHeap* ioHeap, char* ioString)
{
  if ( ioHeap )
  {
    if ( ioString )
      ioHeap->Free(this->AsMdbEnv(), ioString);
  }
  else
    this->NilPointerError();
}

nsIMdbEnv*
morkEnv::AcquireEnvHandle(morkEnv* ev)
{
  nsIMdbEnv* outEnv = 0;
  orkinEnv* e = (orkinEnv*) mObject_Handle;
  if ( e ) // have an old handle?
    e->AddStrongRef(ev->AsMdbEnv());
  else // need new handle?
  {
    e = orkinEnv::MakeEnv(ev, this);
    mObject_Handle = e;
  }
  if ( e )
    outEnv = e;
  return outEnv;
}

void
morkEnv::NewErrorAndCode(const char* inString, mork_u2 inCode)
{
  MORK_ASSERT(morkBool_kFalse); // get developer's attention

  ++mEnv_ErrorCount;
  mEnv_ErrorCode = (inCode)? inCode: morkEnv_kGenericError;
  
  if ( mEnv_ErrorHook )
    mEnv_ErrorHook->OnErrorString(this->AsMdbEnv(), inString);
}

void
morkEnv::NewError(const char* inString)
{
  MORK_ASSERT(morkBool_kFalse); // get developer's attention

  ++mEnv_ErrorCount;
  mEnv_ErrorCode = morkEnv_kGenericError;
  
  if ( mEnv_ErrorHook )
    mEnv_ErrorHook->OnErrorString(this->AsMdbEnv(), inString);
}

void
morkEnv::NewWarning(const char* inString)
{
  MORK_ASSERT(morkBool_kFalse); // get developer's attention
  
  ++mEnv_WarningCount;
  if ( mEnv_ErrorHook )
    mEnv_ErrorHook->OnWarningString(this->AsMdbEnv(), inString);
}

void
morkEnv::StubMethodOnlyError()
{
  this->NewError("method is stub only");
}

void
morkEnv::OutOfMemoryError()
{
  this->NewError("out of memory");
}

void
morkEnv::NilPointerError()
{
  this->NewError("nil pointer");
}

void
morkEnv::NewNonEnvError()
{
  this->NewError("non-env instance");
}

void
morkEnv::NilEnvSlotError()
{
  if ( !mEnv_HandlePool || !mEnv_Factory )
  {
    if ( !mEnv_HandlePool )
      this->NewError("nil mEnv_HandlePool");
    if ( !mEnv_Factory )
      this->NewError("nil mEnv_Factory");
  }
  else
    this->NewError("unknown nil env slot");
}


void morkEnv::NonEnvTypeError(morkEnv* ev)
{
  ev->NewError("non morkEnv");
}

void
morkEnv::ClearMorkErrorsAndWarnings()
{
  mEnv_ErrorCount = 0;
  mEnv_WarningCount = 0;
  mEnv_ErrorCode = 0;
  mEnv_ShouldAbort = morkBool_kFalse;
}

void
morkEnv::AutoClearMorkErrorsAndWarnings()
{
  if ( this->DoAutoClear() )
  {
    mEnv_ErrorCount = 0;
    mEnv_WarningCount = 0;
    mEnv_ErrorCode = 0;
    mEnv_ShouldAbort = morkBool_kFalse;
  }
}

/*static*/ morkEnv*
morkEnv::FromMdbEnv(nsIMdbEnv* ioEnv) // dynamic type checking
{
  morkEnv* outEnv = 0;
  if ( ioEnv )
  {
    // Note this cast is expected to perform some address adjustment of the
    // pointer, so oenv likely does not equal ioEnv.  Do not cast to void*
    // first to force an exactly equal pointer (we tried it and it's wrong).
    orkinEnv* oenv = (orkinEnv*) ioEnv;
    if ( oenv->IsHandle() )
    {
      if ( oenv->IsOpenNode() )
      {
        morkEnv* ev = (morkEnv*) oenv->mHandle_Object;
         if ( ev && ev->IsEnv() )
        {
          if ( ev->DoAutoClear() )
          {
            ev->mEnv_ErrorCount = 0;
            ev->mEnv_WarningCount = 0;
            ev->mEnv_ErrorCode = 0;
          }
          outEnv = ev;
        }
        else
          MORK_ASSERT(outEnv);
       }
      else
        MORK_ASSERT(outEnv);
    }
    else
      MORK_ASSERT(outEnv);
  }
  else
    MORK_ASSERT(outEnv);
  return outEnv;
}


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

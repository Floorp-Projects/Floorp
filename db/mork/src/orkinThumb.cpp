/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- 
 * 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape 
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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

#ifndef _MORKHANDLE_
#include "morkHandle.h"
#endif

#ifndef _MORKTHUMB_
#include "morkThumb.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _ORKINTHUMB_
#include "orkinThumb.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

/* public virtual*/
orkinThumb:: ~orkinThumb() // morkHandle destructor does everything
{
}

/*protected non-poly construction*/
orkinThumb::orkinThumb(morkEnv* ev, // morkUsage is morkUsage_kPool
    morkHandleFace* ioFace,    // must not be nil, cookie for this handle
    morkThumb* ioObject)  // must not be nil, the object for this handle
: morkHandle(ev, ioFace, ioObject, morkMagic_kThumb)
{
  // do not modify mNode_Derived; leave it equal to morkDerived_kHandle
}


/*static */ orkinThumb*
orkinThumb::MakeThumb(morkEnv* ev, morkThumb* ioObject)
{
  mork_bool isEnv = ev->IsEnv();
  MORK_ASSERT(isEnv);
  if ( isEnv )
  {
    morkHandleFace* face = ev->NewHandle(sizeof(orkinThumb));
    if ( face )
      return new(face) orkinThumb(ev, face, ioObject);
    else
      ev->OutOfMemoryError();
  }
    
  return (orkinThumb*) 0;
}

morkEnv*
orkinThumb::CanUseThumb(nsIMdbEnv* mev,
  mork_bool inMutable, mdb_err* outErr) const
{
  morkEnv* outEnv = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    morkThumb* self = (morkThumb*)
      this->GetGoodHandleObject(ev, inMutable, morkMagic_kThumb,
        /*inClosedOkay*/ morkBool_kFalse);
    if ( self )
    {
      if ( self->IsThumb() )
        outEnv = ev;
      else
        self->NonThumbTypeError(ev);
    }
    *outErr = ev->AsErr();
  }
  MORK_ASSERT(outEnv);
  return outEnv;
}


// { ===== begin nsIMdbISupports methods =====
/*virtual*/ mdb_err
orkinThumb::AddRef() // add strong ref with no
{
  morkEnv* ev = mHandle_Env;
  if ( ev && ev->IsEnv() )
    return this->Handle_AddStrongRef(ev->AsMdbEnv());
  else
    return morkEnv_kNonEnvTypeError;
}

/*virtual*/ mdb_err
orkinThumb::Release() // cut strong ref
{
  morkEnv* ev = mHandle_Env;
  if ( ev && ev->IsEnv() )
    return this->Handle_CutStrongRef(ev->AsMdbEnv());
  else
    return morkEnv_kNonEnvTypeError;
}
// } ===== end nsIMdbObject methods =====


// { ===== begin nsIMdbObject methods =====

// { ----- begin attribute methods -----
/*virtual*/ mdb_err
orkinThumb::IsFrozenMdbObject(nsIMdbEnv* mev, mdb_bool* outIsReadonly)
{
  return this->Handle_IsFrozenMdbObject(mev, outIsReadonly);
}
// same as nsIMdbPort::GetIsPortReadonly() when this object is inside a port.
// } ----- end attribute methods -----

// { ----- begin factory methods -----
/*virtual*/ mdb_err
orkinThumb::GetMdbFactory(nsIMdbEnv* mev, nsIMdbFactory** acqFactory)
{
  return this->Handle_GetMdbFactory(mev, acqFactory);
} 
// } ----- end factory methods -----

// { ----- begin ref counting for well-behaved cyclic graphs -----
/*virtual*/ mdb_err
orkinThumb::GetWeakRefCount(nsIMdbEnv* mev, // weak refs
  mdb_count* outCount)
{
  return this->Handle_GetWeakRefCount(mev, outCount);
}  
/*virtual*/ mdb_err
orkinThumb::GetStrongRefCount(nsIMdbEnv* mev, // strong refs
  mdb_count* outCount)
{
  return this->Handle_GetStrongRefCount(mev, outCount);
}

/*virtual*/ mdb_err
orkinThumb::AddWeakRef(nsIMdbEnv* mev)
{
  return this->Handle_AddWeakRef(mev);
}
/*virtual*/ mdb_err
orkinThumb::AddStrongRef(nsIMdbEnv* mev)
{
  return this->Handle_AddStrongRef(mev);
}

/*virtual*/ mdb_err
orkinThumb::CutWeakRef(nsIMdbEnv* mev)
{
  return this->Handle_CutWeakRef(mev);
}
/*virtual*/ mdb_err
orkinThumb::CutStrongRef(nsIMdbEnv* mev)
{
  return this->Handle_CutStrongRef(mev);
}

/*virtual*/ mdb_err
orkinThumb::CloseMdbObject(nsIMdbEnv* mev)
{
  return this->Handle_CloseMdbObject(mev);
}

/*virtual*/ mdb_err
orkinThumb::IsOpenMdbObject(nsIMdbEnv* mev, mdb_bool* outOpen)
{
  return this->Handle_IsOpenMdbObject(mev, outOpen);
}
// } ----- end ref counting -----

// } ===== end nsIMdbObject methods =====


// { ===== begin nsIMdbThumb methods =====
/*virtual*/ mdb_err
orkinThumb::GetProgress(nsIMdbEnv* mev, mdb_count* outTotal,
  mdb_count* outCurrent, mdb_bool* outDone, mdb_bool* outBroken)
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseThumb(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ((morkThumb*) mHandle_Object)->GetProgress(ev, outTotal,
      outCurrent, outDone, outBroken);
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinThumb::DoMore(nsIMdbEnv* mev, mdb_count* outTotal,
  mdb_count* outCurrent, mdb_bool* outDone, mdb_bool* outBroken)
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseThumb(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkThumb* thumb = (morkThumb*) mHandle_Object;
    thumb->DoMore(ev, outTotal, outCurrent, outDone, outBroken);
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinThumb::CancelAndBreakThumb(nsIMdbEnv* mev)
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseThumb(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkThumb* thumb = (morkThumb*) mHandle_Object;
    thumb->mThumb_Done = morkBool_kTrue;
    thumb->mThumb_Broken = morkBool_kTrue;
    thumb->CloseMorkNode(ev); // should I close this here?
    outErr = ev->AsErr();
  }
  return outErr;
}
// } ===== end nsIMdbThumb methods =====

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

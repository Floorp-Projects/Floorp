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

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _ORKINENV_
#include "orkinEnv.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

/* public virtual*/
orkinEnv:: ~orkinEnv() // morkHandle destructor does everything
{
}

/*protected non-poly construction*/
orkinEnv::orkinEnv(morkEnv* ev, // morkUsage is morkUsage_kPool
    morkHandleFace* ioFace,    // must not be nil, cookie for this handle
    morkEnv* ioObject)  // must not be nil, the object for this handle
: morkHandle(ev, ioFace, ioObject, morkMagic_kEnv)
{
  // do not modify mNode_Derived; leave it equal to morkDerived_kHandle
}


/*static */ orkinEnv*
orkinEnv::MakeEnv(morkEnv* ev, morkEnv* ioObject)
{
  mork_bool isEnv = ev->IsEnv();
  MORK_ASSERT(isEnv);
  if ( isEnv )
  {
    morkHandleFace* face = ev->NewHandle(sizeof(orkinEnv));
    if ( face )
      return new(face) orkinEnv(ev, face, ioObject);
    else
      ev->OutOfMemoryError();
  }
    
  return (orkinEnv*) 0;
}

morkEnv*
orkinEnv::CanUseEnv(mork_bool inMutable, mdb_err* outErr) const
{
  MORK_USED_1(inMutable);
  morkEnv* outEnv = 0;
  mdb_err err = morkEnv_kBadEnvError;
  if ( this->IsHandle() )
  {
    if ( this->IsOpenNode() )
    {
      morkEnv* ev = (morkEnv*) this->mHandle_Object;
      if ( ev && ev->IsEnv() )
      {
        outEnv = ev;
        err = 0;
      }
      else
      {
        err = morkEnv_kNonEnvTypeError;
        MORK_ASSERT(outEnv);
      }
    }
    else
    {
      err = morkEnv_kNonOpenNodeError;
      MORK_ASSERT(outEnv);
    }
  }
  else
  {
    err = morkEnv_kNonHandleTypeError;
    MORK_ASSERT(outEnv);
  }
  *outErr = err;
  return outEnv;
}


// { ===== begin nsIMdbISupports methods =====
/*virtual*/ mdb_err
orkinEnv::AddRef() // add strong ref with no
{
  morkEnv* ev = mHandle_Env;
  if ( ev && ev->IsEnv() )
    return this->Handle_AddStrongRef(ev->AsMdbEnv());
  else
    return morkEnv_kNonEnvTypeError;
}

/*virtual*/ mdb_err
orkinEnv::Release() // cut strong ref
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
orkinEnv::IsFrozenMdbObject(nsIMdbEnv* mev, mdb_bool* outIsReadonly)
{
  return this->Handle_IsFrozenMdbObject(mev, outIsReadonly);
}
// same as nsIMdbPort::GetIsPortReadonly() when this object is inside a port.
// } ----- end attribute methods -----

// { ----- begin factory methods -----
/*virtual*/ mdb_err
orkinEnv::GetMdbFactory(nsIMdbEnv* mev, nsIMdbFactory** acqFactory)
{
  return this->Handle_GetMdbFactory(mev, acqFactory);
} 
// } ----- end factory methods -----

// { ----- begin ref counting for well-behaved cyclic graphs -----
/*virtual*/ mdb_err
orkinEnv::GetWeakRefCount(nsIMdbEnv* mev, // weak refs
  mdb_count* outCount)
{
  return this->Handle_GetWeakRefCount(mev, outCount);
}  
/*virtual*/ mdb_err
orkinEnv::GetStrongRefCount(nsIMdbEnv* mev, // strong refs
  mdb_count* outCount)
{
  return this->Handle_GetStrongRefCount(mev, outCount);
}

/*virtual*/ mdb_err
orkinEnv::AddWeakRef(nsIMdbEnv* mev)
{
  return this->Handle_AddWeakRef(mev);
}
/*virtual*/ mdb_err
orkinEnv::AddStrongRef(nsIMdbEnv* mev)
{
  return this->Handle_AddStrongRef(mev);
}

/*virtual*/ mdb_err
orkinEnv::CutWeakRef(nsIMdbEnv* mev)
{
  return this->Handle_CutWeakRef(mev);
}
/*virtual*/ mdb_err
orkinEnv::CutStrongRef(nsIMdbEnv* mev)
{
  return this->Handle_CutStrongRef(mev);
}

/*virtual*/ mdb_err
orkinEnv::CloseMdbObject(nsIMdbEnv* mev)
{
  return this->Handle_CloseMdbObject(mev);
}

/*virtual*/ mdb_err
orkinEnv::IsOpenMdbObject(nsIMdbEnv* mev, mdb_bool* outOpen)
{
  return this->Handle_IsOpenMdbObject(mev, outOpen);
}
// } ----- end ref counting -----

// } ===== end nsIMdbObject methods =====

// { ===== begin nsIMdbEnv methods =====

// { ----- begin attribute methods -----
/*virtual*/ mdb_err
orkinEnv::GetErrorCount(mdb_count* outCount,
  mdb_bool* outShouldAbort)
{
  mdb_err outErr = 0;
  mdb_count count = 1;
  mork_bool shouldAbort = morkBool_kFalse;
  morkEnv* ev = this->CanUseEnv(/*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    count = (mdb_count) ev->mEnv_ErrorCount;
    shouldAbort = ev->mEnv_ShouldAbort;
  }
  if ( outCount )
    *outCount = count;
  if ( outShouldAbort )
    *outShouldAbort = shouldAbort;
  return outErr;
}

/*virtual*/ mdb_err
orkinEnv::GetWarningCount(mdb_count* outCount,
  mdb_bool* outShouldAbort)
{
  mdb_err outErr = 0;
  mdb_count count = 1;
  mork_bool shouldAbort = morkBool_kFalse;
  morkEnv* ev = this->CanUseEnv(/*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    count = (mdb_count) ev->mEnv_WarningCount;
    shouldAbort = ev->mEnv_ShouldAbort;
  }
  if ( outCount )
    *outCount = count;
  if ( outShouldAbort )
    *outShouldAbort = shouldAbort;
  return outErr;
}

/*virtual*/ mdb_err
orkinEnv::GetEnvBeVerbose(mdb_bool* outBeVerbose)
{
  mdb_err outErr = 0;
  mork_bool beVerbose = morkBool_kFalse;
  morkEnv* ev = this->CanUseEnv(/*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    beVerbose = ev->mEnv_BeVerbose;
  }
  if ( outBeVerbose )
    *outBeVerbose = beVerbose;
  return outErr;
}

/*virtual*/ mdb_err
orkinEnv::SetEnvBeVerbose(mdb_bool inBeVerbose)
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseEnv(/*inMutable*/ morkBool_kTrue, &outErr);
  if ( ev )
  {
    ev->mEnv_BeVerbose = inBeVerbose;
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinEnv::GetDoTrace(mdb_bool* outDoTrace)
{
  mdb_err outErr = 0;
  mork_bool doTrace = morkBool_kFalse;
  morkEnv* ev = this->CanUseEnv(/*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    doTrace = ev->mEnv_DoTrace;
  }
  if ( outDoTrace )
    *outDoTrace = doTrace;
  return outErr;
}

/*virtual*/ mdb_err
orkinEnv::SetDoTrace(mdb_bool inDoTrace)
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseEnv(/*inMutable*/ morkBool_kTrue, &outErr);
  if ( ev )
  {
    ev->mEnv_DoTrace = inDoTrace;
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinEnv::GetAutoClear(mdb_bool* outAutoClear)
{
  mdb_err outErr = 0;
  mork_bool autoClear = morkBool_kFalse;
  morkEnv* ev = this->CanUseEnv(/*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    autoClear = ev->DoAutoClear();
  }
  if ( outAutoClear )
    *outAutoClear = autoClear;
  return outErr;
}

/*virtual*/ mdb_err
orkinEnv::SetAutoClear(mdb_bool inAutoClear)
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseEnv(/*inMutable*/ morkBool_kTrue, &outErr);
  if ( ev )
  {
    if ( inAutoClear )
      ev->EnableAutoClear();
    else
      ev->DisableAutoClear();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinEnv::GetErrorHook(nsIMdbErrorHook** acqErrorHook)
{
  mdb_err outErr = 0;
  nsIMdbErrorHook* outErrorHook = 0;
  morkEnv* ev = this->CanUseEnv(/*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    outErrorHook = ev->mEnv_ErrorHook;
  }
  if ( acqErrorHook )
    *acqErrorHook = outErrorHook;
  return outErr;
}

/*virtual*/ mdb_err
orkinEnv::SetErrorHook(
  nsIMdbErrorHook* ioErrorHook) // becomes referenced
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseEnv(/*inMutable*/ morkBool_kTrue, &outErr);
  if ( ev )
  {
    ev->mEnv_ErrorHook = ioErrorHook;
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinEnv::GetHeap(nsIMdbHeap** acqHeap)
{
  mdb_err outErr = 0;
  nsIMdbHeap* outHeap = 0;
  morkEnv* ev = this->CanUseEnv(/*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    nsIMdbHeap* heap = ev->mEnv_Heap;
    if ( heap && heap->HeapAddStrongRef(this) == 0 )
      outHeap = heap;
  }
  if ( acqHeap )
    *acqHeap = outHeap;
  return outErr;
}

/*virtual*/ mdb_err
orkinEnv::SetHeap(
  nsIMdbHeap* ioHeap) // becomes referenced
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseEnv(/*inMutable*/ morkBool_kTrue, &outErr);
  if ( ev )
  {
    nsIMdbHeap_SlotStrongHeap(ioHeap, ev, &ev->mEnv_Heap);
  }
  return outErr;
}
// } ----- end attribute methods -----

/*virtual*/ mdb_err
orkinEnv::ClearErrors() // clear errors beore re-entering db API
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseEnv(/*inMutable*/ morkBool_kTrue, &outErr);
  if ( ev )
  {
    ev->mEnv_ErrorCount = 0;
    ev->mEnv_ErrorCode = 0;
    ev->mEnv_ShouldAbort = morkBool_kFalse;
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinEnv::ClearWarnings() // clear warning
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseEnv(/*inMutable*/ morkBool_kTrue, &outErr);
  if ( ev )
  {
    ev->mEnv_WarningCount = 0;
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinEnv::ClearErrorsAndWarnings() // clear both errors & warnings
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseEnv(/*inMutable*/ morkBool_kTrue, &outErr);
  if ( ev )
  {
    ev->ClearMorkErrorsAndWarnings();
  }
  return outErr;
}
// } ===== end nsIMdbEnv methods =====


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

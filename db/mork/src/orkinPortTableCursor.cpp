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

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _MORKSTORE_
#include "morkStore.h"
#endif

#ifndef _ORKINSTORE_
#include "orkinStore.h"
#endif

#ifndef _MORKPORTTABLECURSOR_
#include "morkPortTableCursor.h"
#endif

#ifndef _ORKINPORTTABLECURSOR_
#include "orkinPortTableCursor.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

/* public virtual*/
orkinPortTableCursor:: ~orkinPortTableCursor() // morkHandle destructor does everything
{
}

/*protected non-poly construction*/
orkinPortTableCursor::orkinPortTableCursor(morkEnv* ev, // morkUsage is morkUsage_kPool
    morkHandleFace* ioFace,    // must not be nil, cookie for this handle
    morkPortTableCursor* ioObject)  // must not be nil, object for this handle
: morkHandle(ev, ioFace, ioObject,
  morkMagic_kPortTableCursor)
{
  // do not modify mNode_Derived; leave it equal to morkDerived_kHandle
}


/*static */ orkinPortTableCursor*
orkinPortTableCursor::MakePortTableCursor(morkEnv* ev,
   morkPortTableCursor* ioObject)
{
  mork_bool isEnv = ev->IsEnv();
  MORK_ASSERT(isEnv);
  if ( isEnv )
  {
    morkHandleFace* face = ev->NewHandle(sizeof(orkinPortTableCursor));
    if ( face )
      return new(face) orkinPortTableCursor(ev, face, ioObject);
    else
      ev->OutOfMemoryError();
  }
    
  return (orkinPortTableCursor*) 0;
}

morkEnv*
orkinPortTableCursor::CanUsePortTableCursor(nsIMdbEnv* mev,
  mork_bool inMutable, mdb_err* outErr) const
{
  morkEnv* outEnv = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    morkPortTableCursor* self = (morkPortTableCursor*)
      this->GetGoodHandleObject(ev, inMutable, morkMagic_kPortTableCursor,
        /*inClosedOkay*/ morkBool_kFalse);
    if ( self )
    {
      if ( self->IsPortTableCursor() )
        outEnv = ev;
      else
        self->NonPortTableCursorTypeError(ev);
    }
    *outErr = ev->AsErr();
  }
  MORK_ASSERT(outEnv);
  return outEnv;
}

// { ===== begin nsIMdbISupports methods =====
/*virtual*/ mdb_err
orkinPortTableCursor::AddRef() // add strong ref with no
{
  morkEnv* ev = mHandle_Env;
  if ( ev && ev->IsEnv() )
    return this->Handle_AddStrongRef(ev->AsMdbEnv());
  else
    return morkEnv_kNonEnvTypeError;
}

/*virtual*/ mdb_err
orkinPortTableCursor::Release() // cut strong ref
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
orkinPortTableCursor::IsFrozenMdbObject(nsIMdbEnv* mev, mdb_bool* outIsReadonly)
{
  return this->Handle_IsFrozenMdbObject(mev, outIsReadonly);
}
// same as nsIMdbPort::GetIsPortReadonly() when this object is inside a port.
// } ----- end attribute methods -----

// { ----- begin factory methods -----
/*virtual*/ mdb_err
orkinPortTableCursor::GetMdbFactory(nsIMdbEnv* mev, nsIMdbFactory** acqFactory)
{
  return this->Handle_GetMdbFactory(mev, acqFactory);
} 
// } ----- end factory methods -----

// { ----- begin ref counting for well-behaved cyclic graphs -----
/*virtual*/ mdb_err
orkinPortTableCursor::GetWeakRefCount(nsIMdbEnv* mev, // weak refs
  mdb_count* outCount)
{
  return this->Handle_GetWeakRefCount(mev, outCount);
}  
/*virtual*/ mdb_err
orkinPortTableCursor::GetStrongRefCount(nsIMdbEnv* mev, // strong refs
  mdb_count* outCount)
{
  return this->Handle_GetStrongRefCount(mev, outCount);
}

/*virtual*/ mdb_err
orkinPortTableCursor::AddWeakRef(nsIMdbEnv* mev)
{
  return this->Handle_AddWeakRef(mev);
}
/*virtual*/ mdb_err
orkinPortTableCursor::AddStrongRef(nsIMdbEnv* mev)
{
  return this->Handle_AddStrongRef(mev);
}

/*virtual*/ mdb_err
orkinPortTableCursor::CutWeakRef(nsIMdbEnv* mev)
{
  return this->Handle_CutWeakRef(mev);
}
/*virtual*/ mdb_err
orkinPortTableCursor::CutStrongRef(nsIMdbEnv* mev)
{
  return this->Handle_CutStrongRef(mev);
}

/*virtual*/ mdb_err
orkinPortTableCursor::CloseMdbObject(nsIMdbEnv* mev)
{
  return this->Handle_CloseMdbObject(mev);
}

/*virtual*/ mdb_err
orkinPortTableCursor::IsOpenMdbObject(nsIMdbEnv* mev, mdb_bool* outOpen)
{
  return this->Handle_IsOpenMdbObject(mev, outOpen);
}
// } ----- end ref counting -----

// } ===== end nsIMdbObject methods =====

// { ===== begin nsIMdbCursor methods =====

// { ----- begin attribute methods -----
/*virtual*/ mdb_err
orkinPortTableCursor::GetCount(nsIMdbEnv* mev, mdb_count* outCount)
{
  mdb_err outErr = 0;
  mdb_count count = 0;
  morkEnv* ev =
    this->CanUsePortTableCursor(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  if ( outCount )
    *outCount = count;
  return outErr;
}

/*virtual*/ mdb_err
orkinPortTableCursor::GetSeed(nsIMdbEnv* mev, mdb_seed* outSeed)
{
  mdb_err outErr = 0;
  mdb_seed seed = 0;
  morkEnv* ev =
    this->CanUsePortTableCursor(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  if ( outSeed )
    *outSeed = seed;
  return outErr;
}

/*virtual*/ mdb_err
orkinPortTableCursor::SetPos(nsIMdbEnv* mev, mdb_pos inPos)
{
  MORK_USED_1(inPos);
  mdb_err outErr = 0;
  morkEnv* ev =
    this->CanUsePortTableCursor(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinPortTableCursor::GetPos(nsIMdbEnv* mev, mdb_pos* outPos)
{
  mdb_err outErr = 0;
  mdb_pos pos = 0;
  morkEnv* ev =
    this->CanUsePortTableCursor(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  if ( outPos )
    *outPos = pos;
    
  return outErr;
}

/*virtual*/ mdb_err
orkinPortTableCursor::SetDoFailOnSeedOutOfSync(nsIMdbEnv* mev, mdb_bool inFail)
{
  MORK_USED_1(inFail);
  mdb_err outErr = 0;
  morkEnv* ev =
    this->CanUsePortTableCursor(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinPortTableCursor::GetDoFailOnSeedOutOfSync(nsIMdbEnv* mev, mdb_bool* outFail)
{
  mdb_err outErr = 0;
  mdb_bool fail = 0;
  morkEnv* ev =
    this->CanUsePortTableCursor(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  if ( outFail )
    *outFail = fail;
  return outErr;
}
// } ----- end attribute methods -----

// } ===== end nsIMdbCursor methods =====

// { ===== begin nsIMdbPortTableCursor methods =====

// { ----- begin attribute methods -----
/*virtual*/ mdb_err
orkinPortTableCursor::SetPort(nsIMdbEnv* mev, nsIMdbPort* ioPort)
{
  MORK_USED_1(ioPort);
  mdb_err outErr = 0;
  morkEnv* ev =
    this->CanUsePortTableCursor(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinPortTableCursor::GetPort(nsIMdbEnv* mev, nsIMdbPort** acqPort)
{
  mdb_err outErr = 0;
  nsIMdbPort* outPort = 0;
  morkEnv* ev =
    this->CanUsePortTableCursor(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkPortTableCursor* cursor = (morkPortTableCursor*) mHandle_Object;
    morkStore* store = cursor->mPortTableCursor_Store;
    if ( store )
      outPort = store->AcquireStoreHandle(ev);
    outErr = ev->AsErr();
  }
  if ( acqPort )
    *acqPort = outPort;
  return outErr;
}

/*virtual*/ mdb_err
orkinPortTableCursor::SetRowScope(nsIMdbEnv* mev, // sets pos to -1
  mdb_scope inRowScope)
{
  mdb_err outErr = 0;
  morkEnv* ev =
    this->CanUsePortTableCursor(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkPortTableCursor* cursor = (morkPortTableCursor*) mHandle_Object;
    cursor->mCursor_Pos = -1;
    
    cursor->SetRowScope(ev, inRowScope);
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinPortTableCursor::GetRowScope(nsIMdbEnv* mev, mdb_scope* outRowScope)
{
  mdb_err outErr = 0;
  mdb_scope rowScope = 0;
  morkEnv* ev =
    this->CanUsePortTableCursor(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkPortTableCursor* cursor = (morkPortTableCursor*) mHandle_Object;
    rowScope = cursor->mPortTableCursor_RowScope;
    outErr = ev->AsErr();
  }
  *outRowScope = rowScope;
  return outErr;
}
// setting row scope to zero iterates over all row scopes in port
  
/*virtual*/ mdb_err
orkinPortTableCursor::SetTableKind(nsIMdbEnv* mev, // sets pos to -1
  mdb_kind inTableKind)
{
  mdb_err outErr = 0;
  morkEnv* ev =
    this->CanUsePortTableCursor(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkPortTableCursor* cursor = (morkPortTableCursor*) mHandle_Object;
    cursor->mCursor_Pos = -1;
    
    cursor->SetTableKind(ev, inTableKind);
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinPortTableCursor::GetTableKind(nsIMdbEnv* mev, mdb_kind* outTableKind)
// setting table kind to zero iterates over all table kinds in row scope
{
  mdb_err outErr = 0;
  mdb_kind tableKind = 0;
  morkEnv* ev =
    this->CanUsePortTableCursor(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkPortTableCursor* cursor = (morkPortTableCursor*) mHandle_Object;
    tableKind = cursor->mPortTableCursor_TableKind;
    outErr = ev->AsErr();
  }
  *outTableKind = tableKind;
  return outErr;
}
// } ----- end attribute methods -----

// { ----- begin table iteration methods -----
/*virtual*/ mdb_err
orkinPortTableCursor::NextTable( // get table at next position in the db
  nsIMdbEnv* mev, // context
  nsIMdbTable** acqTable)
{
  mdb_err outErr = 0;
  nsIMdbTable* outTable = 0;
  morkEnv* ev =
    this->CanUsePortTableCursor(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkPortTableCursor* cursor = (morkPortTableCursor*) mHandle_Object;
    morkTable* table = cursor->NextTable(ev);
    if ( table && ev->Good() )
      outTable = table->AcquireTableHandle(ev);
        
    outErr = ev->AsErr();
  }
  if ( acqTable )
    *acqTable = outTable;
  return outErr;
}
// } ----- end table iteration methods -----

// } ===== end nsIMdbPortTableCursor methods =====


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

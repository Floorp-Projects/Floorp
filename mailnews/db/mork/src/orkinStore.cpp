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

#ifndef _MORKHANDLE_
#include "morkHandle.h"
#endif

#ifndef _MORKSTORE_
#include "morkStore.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _ORKINSTORE_
#include "orkinStore.h"
#endif

#ifndef _MORKTABLE_
#include "morkTable.h"
#endif

#ifndef _MORKPORTTABLECURSOR_
#include "morkPortTableCursor.h"
#endif

#ifndef _ORKINPORTTABLECURSOR_
#include "orkinPortTableCursor.h"
#endif

#ifndef _MORKROW_
#include "morkRow.h"
#endif

#ifndef _MORKROWOBJECT_
#include "morkRowObject.h"
#endif

#ifndef _MORKTHUMB_
#include "morkThumb.h"
#endif

#ifndef _ORKINTHUMB_
#include "orkinThumb.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

/* public virtual*/
orkinStore:: ~orkinStore() // morkHandle destructor does everything
{
}

/*protected non-poly construction*/
orkinStore::orkinStore(morkEnv* ev, // morkUsage is morkUsage_kPool
    morkHandleFace* ioFace,    // must not be nil, cookie for this handle
    morkStore* ioObject)  // must not be nil, the object for this handle
: morkHandle(ev, ioFace, ioObject, morkMagic_kStore)
{
  // do not modify mNode_Derived; leave it equal to morkDerived_kHandle
}


/*static */ orkinStore*
orkinStore::MakeStore(morkEnv* ev, morkStore* ioObject)
{
  mork_bool isEnv = ev->IsEnv();
  MORK_ASSERT(isEnv);
  if ( isEnv )
  {
    morkHandleFace* face = ev->NewHandle(sizeof(orkinStore));
    if ( face )
      return new(face) orkinStore(ev, face, ioObject);
    else
      ev->OutOfMemoryError();
  }
    
  return (orkinStore*) 0;
}

morkEnv*
orkinStore::CanUseStore(nsIMdbEnv* mev,
  mork_bool inMutable, mdb_err* outErr) const
{
  morkEnv* outEnv = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    morkStore* self = (morkStore*)
      this->GetGoodHandleObject(ev, inMutable, morkMagic_kStore);
    if ( self )
    {
      if ( self->IsStore() )
        outEnv = ev;
      else
        self->NonStoreTypeError(ev);
    }
    *outErr = ev->AsErr();
  }
  MORK_ASSERT(outEnv);
  return outEnv;
}

// { ===== begin nsIMdbISupports methods =====
/*virtual*/ mdb_err
orkinStore::AddRef() // add strong ref with no
{
  morkEnv* ev = mHandle_Env;
  if ( ev && ev->IsEnv() )
    return this->Handle_AddStrongRef(ev->AsMdbEnv());
  else
    return morkEnv_kNonEnvTypeError;
}

/*virtual*/ mdb_err
orkinStore::Release() // cut strong ref
{
  morkEnv* ev = mHandle_Env;
  if ( ev && ev->IsEnv() )
    return this->Handle_CutStrongRef(ev->AsMdbEnv());
  else
    return morkEnv_kNonEnvTypeError;
}
// } ===== end nsIMdbISupports methods =====

// { ===== begin nsIMdbObject methods =====

// { ----- begin attribute methods -----
/*virtual*/ mdb_err
orkinStore::IsFrozenMdbObject(nsIMdbEnv* mev, mdb_bool* outIsReadonly)
{
  return this->Handle_IsFrozenMdbObject(mev, outIsReadonly);
}
// same as nsIMdbPort::GetIsPortReadonly() when this object is inside a port.
// } ----- end attribute methods -----

// { ----- begin factory methods -----
/*virtual*/ mdb_err
orkinStore::GetMdbFactory(nsIMdbEnv* mev, nsIMdbFactory** acqFactory)
{
  return this->Handle_GetMdbFactory(mev, acqFactory);
} 
// } ----- end factory methods -----

// { ----- begin ref counting for well-behaved cyclic graphs -----
/*virtual*/ mdb_err
orkinStore::GetWeakRefCount(nsIMdbEnv* mev, // weak refs
  mdb_count* outCount)
{
  return this->Handle_GetWeakRefCount(mev, outCount);
}  
/*virtual*/ mdb_err
orkinStore::GetStrongRefCount(nsIMdbEnv* mev, // strong refs
  mdb_count* outCount)
{
  return this->Handle_GetStrongRefCount(mev, outCount);
}

/*virtual*/ mdb_err
orkinStore::AddWeakRef(nsIMdbEnv* mev)
{
  return this->Handle_AddWeakRef(mev);
}
/*virtual*/ mdb_err
orkinStore::AddStrongRef(nsIMdbEnv* mev)
{
  return this->Handle_AddStrongRef(mev);
}

/*virtual*/ mdb_err
orkinStore::CutWeakRef(nsIMdbEnv* mev)
{
  return this->Handle_CutWeakRef(mev);
}
/*virtual*/ mdb_err
orkinStore::CutStrongRef(nsIMdbEnv* mev)
{
  return this->Handle_CutStrongRef(mev);
}

/*virtual*/ mdb_err
orkinStore::CloseMdbObject(nsIMdbEnv* mev)
{
  return this->Handle_CloseMdbObject(mev);
}

/*virtual*/ mdb_err
orkinStore::IsOpenMdbObject(nsIMdbEnv* mev, mdb_bool* outOpen)
{
  return this->Handle_IsOpenMdbObject(mev, outOpen);
}
// } ----- end ref counting -----

// } ===== end nsIMdbObject methods =====

// { ===== begin nsIMdbPort methods =====

// { ----- begin attribute methods -----
/*virtual*/ mdb_err
orkinStore::GetIsPortReadonly(nsIMdbEnv* mev, mdb_bool* outBool)
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinStore::GetIsStore(nsIMdbEnv* mev, mdb_bool* outBool)
{
  if ( outBool )
    *outBool = morkBool_kTrue;
  return 0;
}

/*virtual*/ mdb_err
orkinStore::GetIsStoreAndDirty(nsIMdbEnv* mev, mdb_bool* outBool)
{
  mdb_err outErr = 0;
  mdb_bool isStoreAndDirty = morkBool_kFalse;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  if ( outBool )
    *outBool = isStoreAndDirty;
  return outErr;
}

/*virtual*/ mdb_err
orkinStore::GetUsagePolicy(nsIMdbEnv* mev, 
  mdbUsagePolicy* ioUsagePolicy)
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinStore::SetUsagePolicy(nsIMdbEnv* mev, 
  const mdbUsagePolicy* inUsagePolicy)
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    // ev->StubMethodOnlyError(); // okay to do nothing?
    outErr = ev->AsErr();
  }
  return outErr;
}
// } ----- end attribute methods -----

// { ----- begin memory policy methods -----  
/*virtual*/ mdb_err
orkinStore::IdleMemoryPurge( // do memory management already scheduled
  nsIMdbEnv* mev, // context
  mdb_size* outEstimatedBytesFreed) // approximate bytes actually freed
{
  mdb_err outErr = 0;
  mdb_size estimatedBytesFreed = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    // ev->StubMethodOnlyError(); // okay to do nothing?
    outErr = ev->AsErr();
  }
  if ( outEstimatedBytesFreed )
    *outEstimatedBytesFreed = estimatedBytesFreed;
  return outErr;
}

/*virtual*/ mdb_err
orkinStore::SessionMemoryPurge( // request specific footprint decrease
  nsIMdbEnv* mev, // context
  mdb_size inDesiredBytesFreed, // approximate number of bytes wanted
  mdb_size* outEstimatedBytesFreed) // approximate bytes actually freed
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    // ev->StubMethodOnlyError(); // okay to do nothing?
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinStore::PanicMemoryPurge( // desperately free all possible memory
  nsIMdbEnv* mev, // context
  mdb_size* outEstimatedBytesFreed) // approximate bytes actually freed
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    // ev->StubMethodOnlyError(); // okay to do nothing?
    outErr = ev->AsErr();
  }
  return outErr;
}
// } ----- end memory policy methods -----

// { ----- begin filepath methods -----
/*virtual*/ mdb_err
orkinStore::GetPortFilePath(
  nsIMdbEnv* mev, // context
  mdbYarn* outFilePath, // name of file holding port content
  mdbYarn* outFormatVersion) // file format description
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  return outErr;
}
// } ----- end filepath methods -----

// { ----- begin export methods -----
/*virtual*/ mdb_err
orkinStore::BestExportFormat( // determine preferred export format
  nsIMdbEnv* mev, // context
  mdbYarn* outFormatVersion) // file format description
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinStore::CanExportToFormat( // can export content in given specific format?
  nsIMdbEnv* mev, // context
  const char* inFormatVersion, // file format description
  mdb_bool* outCanExport) // whether ExportSource() might succeed
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinStore::ExportToFormat( // export content in given specific format
  nsIMdbEnv* mev, // context
  const char* inFilePath, // the file to receive exported content
  const char* inFormatVersion, // file format description
  nsIMdbThumb** acqThumb) // acquire thumb for incremental export
// Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
// then the export will be finished.
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  return outErr;
}

// } ----- end export methods -----

// { ----- begin token methods -----
/*virtual*/ mdb_err
orkinStore::TokenToString( // return a string name for an integer token
  nsIMdbEnv* mev, // context
  mdb_token inToken, // token for inTokenName inside this port
  mdbYarn* outTokenName) // the type of table to access
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ((morkStore*) mHandle_Object)->TokenToString(ev, inToken, outTokenName);
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinStore::StringToToken( // return an integer token for scope name
  nsIMdbEnv* mev, // context
  const char* inTokenName, // Latin1 string to tokenize if possible
  mdb_token* outToken) // token for inTokenName inside this port
  // String token zero is never used and never supported. If the port
  // is a mutable store, then StringToToken() to create a new
  // association of inTokenName with a new integer token if possible.
  // But a readonly port will return zero for an unknown scope name.
{
  mdb_err outErr = 0;
  mdb_token token = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    token = ((morkStore*) mHandle_Object)->StringToToken(ev, inTokenName);
    outErr = ev->AsErr();
  }
  if ( outToken )
    *outToken = token;
  return outErr;
}
  

/*virtual*/ mdb_err
orkinStore::QueryToken( // like StringToToken(), but without adding
  nsIMdbEnv* mev, // context
  const char* inTokenName, // Latin1 string to tokenize if possible
  mdb_token* outToken) // token for inTokenName inside this port
  // QueryToken() will return a string token if one already exists,
  // but unlike StringToToken(), will not assign a new token if not
  // already in use.
{
  mdb_err outErr = 0;
  mdb_token token = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    token = ((morkStore*) mHandle_Object)->QueryToken(ev, inTokenName);
    outErr = ev->AsErr();
  }
  if ( outToken )
    *outToken = token;
  return outErr;
}


// } ----- end token methods -----

// { ----- begin row methods -----  
/*virtual*/ mdb_err
orkinStore::HasRow( // contains a row with the specified oid?
  nsIMdbEnv* mev, // context
  const mdbOid* inOid,  // hypothetical row oid
  mdb_bool* outHasRow) // whether GetRow() might succeed
{
  mdb_err outErr = 0;
  mdb_bool hasRow = morkBool_kFalse;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkStore* store = (morkStore*) mHandle_Object;
    morkRow* row = store->GetRow(ev, inOid);
    if ( row )
      hasRow = morkBool_kTrue;
      
    outErr = ev->AsErr();
  }
  if ( outHasRow )
    *outHasRow = hasRow;
  return outErr;
}
  
/*virtual*/ mdb_err
orkinStore::GetRow( // access one row with specific oid
  nsIMdbEnv* mev, // context
  const mdbOid* inOid,  // hypothetical row oid
  nsIMdbRow** acqRow) // acquire specific row (or null)
{
  mdb_err outErr = 0;
  nsIMdbRow* outRow = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkStore* store = (morkStore*) mHandle_Object;
    morkRow* row = store->GetRow(ev, inOid);
    if ( row && ev->Good() )
      outRow = row->AcquireRowHandle(ev, store);
      
    outErr = ev->AsErr();
  }
  if ( acqRow )
    *acqRow = outRow;
  return outErr;
}

/*virtual*/ mdb_err
orkinStore::GetRowRefCount( // get number of tables that contain a row 
  nsIMdbEnv* mev, // context
  const mdbOid* inOid,  // hypothetical row oid
  mdb_count* outRefCount) // number of tables containing inRowKey 
{
  mdb_err outErr = 0;
  mdb_count count = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkStore* store = (morkStore*) mHandle_Object;
    morkRow* row = store->GetRow(ev, inOid);
    if ( row && ev->Good() )
      count = row->mRow_TableUses;
      
    outErr = ev->AsErr();
  }
  if ( outRefCount )
    *outRefCount = count;
  return outErr;
}
// } ----- end row methods -----

// { ----- begin table methods -----  
/*virtual*/ mdb_err
orkinStore::HasTable( // supports a table with the specified oid?
  nsIMdbEnv* mev, // context
  const mdbOid* inOid,  // hypothetical table oid
  mdb_bool* outHasTable) // whether GetTable() might succeed
{
  mdb_err outErr = 0;
  mork_bool hasTable = morkBool_kFalse;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTable* table = ((morkStore*) mHandle_Object)->GetTable(ev, inOid);
    if ( table )
      hasTable = morkBool_kTrue;
    
    outErr = ev->AsErr();
  }
  if ( outHasTable )
    *outHasTable = hasTable;
  return outErr;
}
  
/*virtual*/ mdb_err
orkinStore::GetTable( // access one table with specific oid
  nsIMdbEnv* mev, // context
  const mdbOid* inOid,  // hypothetical table oid
  nsIMdbTable** acqTable) // acquire specific table (or null)
{
  mdb_err outErr = 0;
  nsIMdbTable* outTable = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTable* table =
      ((morkStore*) mHandle_Object)->GetTable(ev, inOid);
    if ( table && ev->Good() )
      outTable = table->AcquireTableHandle(ev);
    outErr = ev->AsErr();
  }
  if ( acqTable )
    *acqTable = outTable;
  return outErr;
}

/*virtual*/ mdb_err
orkinStore::HasTableKind( // supports a table of the specified type?
  nsIMdbEnv* mev, // context
  mdb_scope inRowScope, // rid scope for row ids
  mdb_kind inTableKind, // the type of table to access
  mdb_count* outTableCount, // current number of such tables
  mdb_bool* outSupportsTable) // whether GetTableKind() might succeed
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    *outSupportsTable =
      ((morkStore*) mHandle_Object)->HasTableKind(ev, inRowScope,
        inTableKind, outTableCount);
    outErr = ev->AsErr();
  }
  return outErr;
}
      
/*virtual*/ mdb_err
orkinStore::GetTableKind( // access one (random) table of specific type
  nsIMdbEnv* mev, // context
  mdb_scope inRowScope,      // row scope for row ids
  mdb_kind inTableKind,      // the type of table to access
  mdb_count* outTableCount, // current number of such tables
  mdb_bool* outMustBeUnique, // whether port can hold only one of these
  nsIMdbTable** acqTable)      // acquire scoped collection of rows
{
  mdb_err outErr = 0;
  nsIMdbTable* outTable = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTable* table =
      ((morkStore*) mHandle_Object)->GetTableKind(ev, inRowScope,
        inTableKind, outTableCount, outMustBeUnique);
    if ( table && ev->Good() )
      outTable = table->AcquireTableHandle(ev);
    outErr = ev->AsErr();
  }
  if ( acqTable )
    *acqTable = outTable;
  return outErr;
}
  
/*virtual*/ mdb_err
orkinStore::GetPortTableCursor( // get cursor for all tables of specific type
  nsIMdbEnv* mev, // context
  mdb_scope inRowScope, // row scope for row ids
  mdb_kind inTableKind, // the type of table to access
  nsIMdbPortTableCursor** acqCursor) // all such tables in the port
{
  mdb_err outErr = 0;
  nsIMdbPortTableCursor* outCursor = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkPortTableCursor* cursor =
      ((morkStore*) mHandle_Object)->GetPortTableCursor(ev, inRowScope,
        inTableKind);
    if ( cursor && ev->Good() )
      outCursor = cursor->AcquirePortTableCursorHandle(ev);

    outErr = ev->AsErr();
  }
  if ( acqCursor )
    *acqCursor = outCursor;
  return outErr;
}
// } ----- end table methods -----

// } ===== end nsIMdbPort methods =====

// { ===== begin nsIMdbStore methods =====

// { ----- begin table methods -----
/*virtual*/ mdb_err
orkinStore::NewTable( // make one new table of specific type
  nsIMdbEnv* mev, // context
  mdb_scope inRowScope,    // row scope for row ids
  mdb_kind inTableKind,    // the type of table to access
  mdb_bool inMustBeUnique, // whether store can hold only one of these
  const mdbOid* inOptionalMetaRowOid, // can be nil to avoid specifying 
  nsIMdbTable** acqTable)     // acquire scoped collection of rows
{
  mdb_err outErr = 0;
  nsIMdbTable* outTable = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTable* table =
      ((morkStore*) mHandle_Object)->NewTable(ev, inRowScope,
        inTableKind, inMustBeUnique, inOptionalMetaRowOid);
    if ( table && ev->Good() )
      outTable = table->AcquireTableHandle(ev);
    outErr = ev->AsErr();
  }
  if ( acqTable )
    *acqTable = outTable;
  return outErr;
}

/*virtual*/ mdb_err
orkinStore::NewTableWithOid( // make one new table of specific type
  nsIMdbEnv* mev, // context
  const mdbOid* inOid,   // caller assigned oid
  mdb_kind inTableKind,    // the type of table to access
  mdb_bool inMustBeUnique, // whether store can hold only one of these
  const mdbOid* inOptionalMetaRowOid, // can be nil to avoid specifying 
  nsIMdbTable** acqTable)     // acquire scoped collection of rows
{
  mdb_err outErr = 0;
  nsIMdbTable* outTable = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTable* table = ((morkStore*) mHandle_Object)->OidToTable(ev, inOid,
      inOptionalMetaRowOid);
    if ( table && ev->Good() )
    {
      table->mTable_Kind = inTableKind;
      if ( inMustBeUnique )
        table->mTable_MustBeUnique = inMustBeUnique;
      outTable = table->AcquireTableHandle(ev);
    }
    outErr = ev->AsErr();
  }
  if ( acqTable )
    *acqTable = outTable;
  return outErr;
}
// } ----- end table methods -----

// { ----- begin row scope methods -----
/*virtual*/ mdb_err
orkinStore::RowScopeHasAssignedIds(nsIMdbEnv* mev,
  mdb_scope inRowScope,   // row scope for row ids
  mdb_bool* outCallerAssigned, // nonzero if caller assigned specified
  mdb_bool* outStoreAssigned) // nonzero if store db assigned specified
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinStore::SetCallerAssignedIds(nsIMdbEnv* mev,
  mdb_scope inRowScope,   // row scope for row ids
  mdb_bool* outCallerAssigned, // nonzero if caller assigned specified
  mdb_bool* outStoreAssigned) // nonzero if store db assigned specified
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinStore::SetStoreAssignedIds(nsIMdbEnv* mev,
  mdb_scope inRowScope,   // row scope for row ids
  mdb_bool* outCallerAssigned, // nonzero if caller assigned specified
  mdb_bool* outStoreAssigned) // nonzero if store db assigned specified
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  return outErr;
}
// } ----- end row scope methods -----

// { ----- begin row methods -----
/*virtual*/ mdb_err
orkinStore::NewRowWithOid(nsIMdbEnv* mev, // new row w/ caller assigned oid
  const mdbOid* inOid,   // caller assigned oid
  nsIMdbRow** acqRow) // create new row
{
  mdb_err outErr = 0;
  nsIMdbRow* outRow = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkStore* store = (morkStore*) mHandle_Object;
    morkRow* row = store->NewRowWithOid(ev, inOid);
    if ( row && ev->Good() )
      outRow = row->AcquireRowHandle(ev, store);
      
    outErr = ev->AsErr();
  }
  if ( acqRow )
    *acqRow = outRow;
  return outErr;
}

/*virtual*/ mdb_err
orkinStore::NewRow(nsIMdbEnv* mev, // new row with db assigned oid
  mdb_scope inRowScope,   // row scope for row ids
  nsIMdbRow** acqRow) // create new row
// Note this row must be added to some table or cell child before the
// store is closed in order to make this row persist across sesssions.
{
  mdb_err outErr = 0;
  nsIMdbRow* outRow = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkStore* store = (morkStore*) mHandle_Object;
    morkRow* row = store->NewRow(ev, inRowScope);
    if ( row && ev->Good() )
      outRow = row->AcquireRowHandle(ev, store);
      
    outErr = ev->AsErr();
  }
  if ( acqRow )
    *acqRow = outRow;
  return outErr;
}
// } ----- end row methods -----

// { ----- begin inport/export methods -----
/*virtual*/ mdb_err
orkinStore::ImportContent( // import content from port
  nsIMdbEnv* mev, // context
  mdb_scope inRowScope, // scope for rows (or zero for all?)
  nsIMdbPort* ioPort, // the port with content to add to store
  nsIMdbThumb** acqThumb) // acquire thumb for incremental import
// Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
// then the import will be finished.
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  return outErr;
}
// } ----- end inport/export methods -----

// { ----- begin hinting methods -----
/*virtual*/ mdb_err
orkinStore::ShareAtomColumnsHint( // advise re shared col content atomizing
  nsIMdbEnv* mev, // context
  mdb_scope inScopeHint, // zero, or suggested shared namespace
  const mdbColumnSet* inColumnSet) // cols desired tokenized together
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    // ev->StubMethodOnlyError(); // okay to do nothing for a hint method
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinStore::AvoidAtomColumnsHint( // advise col w/ poor atomizing prospects
  nsIMdbEnv* mev, // context
  const mdbColumnSet* inColumnSet) // cols with poor atomizing prospects
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    // ev->StubMethodOnlyError(); // okay to do nothing for a hint method
    outErr = ev->AsErr();
  }
  return outErr;
}
// } ----- end hinting methods -----

// { ----- begin commit methods -----
/*virtual*/ mdb_err
orkinStore::SmallCommit( // save minor changes if convenient and uncostly
  nsIMdbEnv* mev) // context
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    // ev->StubMethodOnlyError(); // it's okay to do nothing for small commit
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinStore::LargeCommit( // save important changes if at all possible
  nsIMdbEnv* mev, // context
  nsIMdbThumb** acqThumb) // acquire thumb for incremental commit
// Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
// then the commit will be finished.  Note the store is effectively write
// locked until commit is finished or canceled through the thumb instance.
// Until the commit is done, the store will report it has readonly status.
{
  mdb_err outErr = 0;
  nsIMdbThumb* outThumb = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkStore* store = (morkStore*) mHandle_Object;
    nsIMdbHeap* heap = store->mPort_Heap;
    mork_bool doCollect = morkBool_kFalse;
    morkThumb* thumb = morkThumb::Make_CompressCommit(ev, heap, store, doCollect);
    if ( thumb )
      outThumb = orkinThumb::MakeThumb(ev, thumb);
      
    outErr = ev->AsErr();
  }
  if ( acqThumb )
    *acqThumb = outThumb;
  return outErr;
}

/*virtual*/ mdb_err
orkinStore::SessionCommit( // save all changes if large commits delayed
  nsIMdbEnv* mev, // context
  nsIMdbThumb** acqThumb) // acquire thumb for incremental commit
// Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
// then the commit will be finished.  Note the store is effectively write
// locked until commit is finished or canceled through the thumb instance.
// Until the commit is done, the store will report it has readonly status.
{
  mdb_err outErr = 0;
  nsIMdbThumb* outThumb = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkStore* store = (morkStore*) mHandle_Object;
    nsIMdbHeap* heap = store->mPort_Heap;
    mork_bool doCollect = morkBool_kFalse;
    morkThumb* thumb = morkThumb::Make_CompressCommit(ev, heap, store, doCollect);
    if ( thumb )
      outThumb = orkinThumb::MakeThumb(ev, thumb);
      
    outErr = ev->AsErr();
  }
  if ( acqThumb )
    *acqThumb = outThumb;
  return outErr;
}

/*virtual*/ mdb_err
orkinStore::CompressCommit( // commit and make db smaller if possible
  nsIMdbEnv* mev, // context
  nsIMdbThumb** acqThumb) // acquire thumb for incremental commit
// Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
// then the commit will be finished.  Note the store is effectively write
// locked until commit is finished or canceled through the thumb instance.
// Until the commit is done, the store will report it has readonly status.
// } ----- end commit methods -----
{
  mdb_err outErr = 0;
  nsIMdbThumb* outThumb = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkStore* store = (morkStore*) mHandle_Object;
    nsIMdbHeap* heap = store->mPort_Heap;
    mork_bool doCollect = morkBool_kFalse;
    morkThumb* thumb = morkThumb::Make_CompressCommit(ev, heap, store, doCollect);
    if ( thumb )
      outThumb = orkinThumb::MakeThumb(ev, thumb);
      
    outErr = ev->AsErr();
  }
  if ( acqThumb )
    *acqThumb = outThumb;
  return outErr;
}

// } ===== end nsIMdbStore methods =====


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

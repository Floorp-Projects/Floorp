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

#ifndef _MORKTABLE_
#include "morkTable.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _ORKINTABLE_
#include "orkinTable.h"
#endif

#ifndef _ORKINROW_
#include "orkinRow.h"
#endif

#ifndef _MORKTABLEROWCURSOR_
#include "morkTableRowCursor.h"
#endif

#ifndef _ORKINTABLEROWCURSOR_
#include "orkinTableRowCursor.h"
#endif

#ifndef _MORKROWSPACE_
#include "morkRowSpace.h"
#endif

#ifndef _MORKSTORE_
#include "morkStore.h"
#endif

#ifndef _ORKINSTORE_
#include "orkinStore.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

/* public virtual*/
orkinTable:: ~orkinTable() // morkHandle destructor does everything
{
}

/*protected non-poly construction*/
orkinTable::orkinTable(morkEnv* ev, // morkUsage is morkUsage_kPool
    morkHandleFace* ioFace,    // must not be nil, cookie for this handle
    morkTable* ioObject)  // must not be nil, the object for this handle
: morkHandle(ev, ioFace, ioObject, morkMagic_kTable)
{
  // do not modify mNode_Derived; leave it equal to morkDerived_kHandle
}


/*static */ orkinTable*
orkinTable::MakeTable(morkEnv* ev, morkTable* ioObject)
{
  mork_bool isEnv = ev->IsEnv();
  MORK_ASSERT(isEnv);
  if ( isEnv )
  {
    morkHandleFace* face = ev->NewHandle(sizeof(orkinTable));
    if ( face )
      return new(face) orkinTable(ev, face, ioObject);
    else
      ev->OutOfMemoryError();
  }
    
  return (orkinTable*) 0;
}

morkEnv*
orkinTable::CanUseTable(nsIMdbEnv* mev,
  mork_bool inMutable, mdb_err* outErr) const
{
  morkEnv* outEnv = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    morkTable* self = (morkTable*)
      this->GetGoodHandleObject(ev, inMutable, morkMagic_kTable,
        /*inClosedOkay*/ morkBool_kFalse);
    if ( self )
    {
      if ( self->IsTable() )
        outEnv = ev;
      else
        self->NonTableTypeError(ev);
    }
    *outErr = ev->AsErr();
  }
  MORK_ASSERT(outEnv);
  return outEnv;
}


// { ===== begin nsIMdbISupports methods =====
/*virtual*/ mdb_err
orkinTable::AddRef() // add strong ref with no
{
  morkEnv* ev = mHandle_Env;
  if ( ev && ev->IsEnv() )
    return this->Handle_AddStrongRef(ev->AsMdbEnv());
  else
    return morkEnv_kNonEnvTypeError;
}

/*virtual*/ mdb_err
orkinTable::Release() // cut strong ref
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
orkinTable::IsFrozenMdbObject(nsIMdbEnv* mev, mdb_bool* outIsReadonly)
{
  return this->Handle_IsFrozenMdbObject(mev, outIsReadonly);
}
// same as nsIMdbPort::GetIsPortReadonly() when this object is inside a port.
// } ----- end attribute methods -----

// { ----- begin factory methods -----
/*virtual*/ mdb_err
orkinTable::GetMdbFactory(nsIMdbEnv* mev, nsIMdbFactory** acqFactory)
{
  return this->Handle_GetMdbFactory(mev, acqFactory);
} 
// } ----- end factory methods -----

// { ----- begin ref counting for well-behaved cyclic graphs -----
/*virtual*/ mdb_err
orkinTable::GetWeakRefCount(nsIMdbEnv* mev, // weak refs
  mdb_count* outCount)
{
  return this->Handle_GetWeakRefCount(mev, outCount);
}  
/*virtual*/ mdb_err
orkinTable::GetStrongRefCount(nsIMdbEnv* mev, // strong refs
  mdb_count* outCount)
{
  return this->Handle_GetStrongRefCount(mev, outCount);
}

/*virtual*/ mdb_err
orkinTable::AddWeakRef(nsIMdbEnv* mev)
{
  return this->Handle_AddWeakRef(mev);
}
/*virtual*/ mdb_err
orkinTable::AddStrongRef(nsIMdbEnv* mev)
{
  return this->Handle_AddStrongRef(mev);
}

/*virtual*/ mdb_err
orkinTable::CutWeakRef(nsIMdbEnv* mev)
{
  return this->Handle_CutWeakRef(mev);
}
/*virtual*/ mdb_err
orkinTable::CutStrongRef(nsIMdbEnv* mev)
{
  return this->Handle_CutStrongRef(mev);
}

/*virtual*/ mdb_err
orkinTable::CloseMdbObject(nsIMdbEnv* mev)
{
  return this->Handle_CloseMdbObject(mev);
}

/*virtual*/ mdb_err
orkinTable::IsOpenMdbObject(nsIMdbEnv* mev, mdb_bool* outOpen)
{
  return this->Handle_IsOpenMdbObject(mev, outOpen);
}
// } ----- end ref counting -----

// } ===== end nsIMdbObject methods =====

// { ===== begin nsIMdbCollection methods =====

// { ----- begin attribute methods -----
/*virtual*/ mdb_err
orkinTable::GetSeed(nsIMdbEnv* mev,
  mdb_seed* outSeed)    // member change count
{
  mdb_err outErr = 0;
  mdb_seed seed = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTable* table = (morkTable*) mHandle_Object;
    seed = table->mTable_RowArray.mArray_Seed;
    outErr = ev->AsErr();
  }
  if ( outSeed )
    *outSeed = seed;
  return outErr;
}
  
/*virtual*/ mdb_err
orkinTable::GetCount(nsIMdbEnv* mev,
  mdb_count* outCount) // member count
{
  mdb_err outErr = 0;
  mdb_count count = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTable* table = (morkTable*) mHandle_Object;
    count = table->mTable_RowArray.mArray_Fill;
    outErr = ev->AsErr();
  }
  if ( outCount )
    *outCount = count;
  return outErr;
}

/*virtual*/ mdb_err
orkinTable::GetPort(nsIMdbEnv* mev,
  nsIMdbPort** acqPort) // collection container
{
  mdb_err outErr = 0;
  nsIMdbPort* outPort = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTable* table = (morkTable*) mHandle_Object;
    morkStore* store = table->mTable_Store;
    if ( store )
      outPort = store->AcquireStoreHandle(ev);
    outErr = ev->AsErr();
  }
  if ( acqPort )
    *acqPort = outPort;
  return outErr;
}
// } ----- end attribute methods -----

// { ----- begin cursor methods -----
/*virtual*/ mdb_err
orkinTable::GetCursor( // make a cursor starting iter at inMemberPos
  nsIMdbEnv* mev, // context
  mdb_pos inMemberPos, // zero-based ordinal pos of member in collection
  nsIMdbCursor** acqCursor) // acquire new cursor instance
{
  return this->GetTableRowCursor(mev, inMemberPos,
    (nsIMdbTableRowCursor**) acqCursor);
}
// } ----- end cursor methods -----

// { ----- begin ID methods -----
/*virtual*/ mdb_err
orkinTable::GetOid(nsIMdbEnv* mev,
  mdbOid* outOid) // read object identity
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTable* table = (morkTable*) mHandle_Object;
    table->GetTableOid(ev, outOid);
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinTable::BecomeContent(nsIMdbEnv* mev,
  const mdbOid* inOid) // exchange content
{
  MORK_USED_1(inOid);
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    // remember table->MaybeDirtySpaceStoreAndTable();
    
    morkTable* table = (morkTable*) mHandle_Object;
    MORK_USED_1(table);

    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  return outErr;
}
// } ----- end ID methods -----

// { ----- begin activity dropping methods -----
/*virtual*/ mdb_err
orkinTable::DropActivity( // tell collection usage no longer expected
  nsIMdbEnv* mev)
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTable* table;
    table = (morkTable*) mHandle_Object;
    // ev->StubMethodOnlyError(); // okay to do nothing
    outErr = ev->AsErr();
  }
  return outErr;
}
// } ----- end activity dropping methods -----

// } ===== end nsIMdbCollection methods =====

// { ===== begin nsIMdbTable methods =====

// { ----- begin attribute methods -----

/*virtual*/ mdb_err
orkinTable::SetTablePriority(nsIMdbEnv* mev, mdb_priority inPrio)
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTable* table = (morkTable*) mHandle_Object;
    if ( inPrio > morkPriority_kMax )
      inPrio = morkPriority_kMax;
      
    table->mTable_Priority = inPrio;
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinTable::GetTablePriority(nsIMdbEnv* mev, mdb_priority* outPrio)
{
  mdb_err outErr = 0;
  mork_priority prio = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTable* table = (morkTable*) mHandle_Object;
    prio = table->mTable_Priority;
    if ( prio > morkPriority_kMax )
    {
      prio = morkPriority_kMax;
      table->mTable_Priority = prio;
    }
    outErr = ev->AsErr();
  }
  if ( outPrio )
    *outPrio = prio;
  return outErr;
}


/*virtual*/ mdb_err
orkinTable:: GetTableBeVerbose(nsIMdbEnv* mev, mdb_bool* outBeVerbose)
{
  mdb_err outErr = 0;
  mdb_bool beVerbose = morkBool_kFalse;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTable* table = (morkTable*) mHandle_Object;
    beVerbose = table->IsTableVerbose();
    outErr = ev->AsErr();
  }
  if ( outBeVerbose )
    *outBeVerbose = beVerbose;
  return outErr;
}

/*virtual*/ mdb_err
orkinTable::SetTableBeVerbose(nsIMdbEnv* mev, mdb_bool inBeVerbose)
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTable* table = (morkTable*) mHandle_Object;
    if ( inBeVerbose )
      table->SetTableVerbose();
    else
       table->ClearTableVerbose();
   
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinTable::GetTableIsUnique(nsIMdbEnv* mev, mdb_bool* outIsUnique)
{
  mdb_err outErr = 0;
  mdb_bool isUnique = morkBool_kFalse;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTable* table = (morkTable*) mHandle_Object;
    isUnique = table->IsTableUnique();
    outErr = ev->AsErr();
  }
  if ( outIsUnique )
    *outIsUnique = isUnique;
  return outErr;
}

/*virtual*/ mdb_err
orkinTable::GetTableKind(nsIMdbEnv* mev, mdb_kind* outTableKind)
{
  mdb_err outErr = 0;
  mdb_kind tableKind = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTable* table = (morkTable*) mHandle_Object;
    tableKind = table->mTable_Kind;
    outErr = ev->AsErr();
  }
  if ( outTableKind )
    *outTableKind = tableKind;
  return outErr;
}

/*virtual*/ mdb_err
orkinTable::GetRowScope(nsIMdbEnv* mev, mdb_scope* outRowScope)
{
  mdb_err outErr = 0;
  mdb_scope rowScope = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTable* table = (morkTable*) mHandle_Object;
    morkRowSpace* space = table->mTable_RowSpace;
    if ( space )
      rowScope = space->SpaceScope();
    else
      table->NilRowSpaceError(ev);

    outErr = ev->AsErr();
  }
  if ( outRowScope )
    *outRowScope = rowScope;
  return outErr;
}

/*virtual*/ mdb_err
orkinTable::GetMetaRow( nsIMdbEnv* mev,
  const mdbOid* inOptionalMetaRowOid, // can be nil to avoid specifying 
  mdbOid* outOid, // output meta row oid, can be nil to suppress output
  nsIMdbRow** acqRow) // acquire table's unique singleton meta row
  // The purpose of a meta row is to support the persistent recording of
  // meta info about a table as cells put into the distinguished meta row.
  // Each table has exactly one meta row, which is not considered a member
  // of the collection of rows inside the table.  The only way to tell
  // whether a row is a meta row is by the fact that it is returned by this
  // GetMetaRow() method from some table. Otherwise nothing distinguishes
  // a meta row from any other row.  A meta row can be used anyplace that
  // any other row can be used, and can even be put into other tables (or
  // the same table) as a table member, if this is useful for some reason.
  // The first attempt to access a table's meta row using GetMetaRow() will
  // cause the meta row to be created if it did not already exist.  When the
  // meta row is created, it will have the row oid that was previously
  // requested for this table's meta row; or if no oid was ever explicitly
  // specified for this meta row, then a unique oid will be generated in
  // the row scope named "metaScope" (so obviously MDB clients should not
  // manually allocate any row IDs from that special meta scope namespace).
  // The meta row oid can be specified either when the table is created, or
  // else the first time that GetMetaRow() is called, by passing a non-nil
  // pointer to an oid for parameter inOptionalMetaRowOid.  The meta row's
  // actual oid is returned in outOid (if this is a non-nil pointer), and
  // it will be different from inOptionalMetaRowOid when the meta row was
  // already given a different oid earlier.
{
  mdb_err outErr = 0;
  nsIMdbRow* outRow = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTable* table = (morkTable*) mHandle_Object;
    morkRow* row = table->GetMetaRow(ev, inOptionalMetaRowOid);
    if ( row && ev->Good() )
    {
      if ( outOid )
        *outOid = row->mRow_Oid;
        
      outRow = row->AcquireRowHandle(ev, table->mTable_Store);
    }
    outErr = ev->AsErr();
  }
  if ( acqRow )
    *acqRow = outRow;
    
  if ( ev->Bad() && outOid )
  {
    outOid->mOid_Scope = 0;
    outOid->mOid_Id = morkRow_kMinusOneRid;
  }
  return outErr;
}

// } ----- end attribute methods -----

// { ----- begin cursor methods -----
/*virtual*/ mdb_err
orkinTable::GetTableRowCursor( // make a cursor, starting iteration at inRowPos
  nsIMdbEnv* mev, // context
  mdb_pos inRowPos, // zero-based ordinal position of row in table
  nsIMdbTableRowCursor** acqCursor) // acquire new cursor instance
{
  mdb_err outErr = 0;
  nsIMdbTableRowCursor* outCursor = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTableRowCursor* cursor =
      ((morkTable*) mHandle_Object)->NewTableRowCursor(ev, inRowPos);
    if ( cursor )
    {
      if ( ev->Good() )
      {
        // cursor->mCursor_Seed = (mork_seed) inRowPos;
        outCursor = cursor->AcquireTableRowCursorHandle(ev);
      }
      cursor->CutStrongRef(ev);
    }
      
    outErr = ev->AsErr();
  }
  if ( acqCursor )
    *acqCursor = outCursor;
  return outErr;
}
// } ----- end row position methods -----

// { ----- begin row position methods -----
/*virtual*/ mdb_err
orkinTable::PosToOid( // get row member for a table position
  nsIMdbEnv* mev, // context
  mdb_pos inRowPos, // zero-based ordinal position of row in table
  mdbOid* outOid) // row oid at the specified position
{
  mdb_err outErr = 0;
  mdbOid roid;
  roid.mOid_Scope = 0;
  roid.mOid_Id = (mork_id) -1;
  
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTable* table = (morkTable*) mHandle_Object;
    morkRow* row = table->SafeRowAt(ev, inRowPos);
    if ( row )
      roid = row->mRow_Oid;
    
    outErr = ev->AsErr();
  }
  if ( outOid )
    *outOid = roid;
  return outErr;
}

/*virtual*/ mdb_err
orkinTable::OidToPos( // test for the table position of a row member
  nsIMdbEnv* mev, // context
  const mdbOid* inOid, // row to find in table
  mdb_pos* outPos) // zero-based ordinal position of row in table
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    mork_pos pos = ((morkTable*) mHandle_Object)->ArrayHasOid(ev, inOid);
    if ( outPos )
      *outPos = pos;
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinTable::PosToRow( // get row member for a table position
  nsIMdbEnv* mev, // context
  mdb_pos inRowPos, // zero-based ordinal position of row in table
  nsIMdbRow** acqRow) // acquire row at table position inRowPos
{
  mdb_err outErr = 0;
  nsIMdbRow* outRow = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTable* table = (morkTable*) mHandle_Object;
    morkStore* store = table->mTable_Store;
    morkRow* row = table->SafeRowAt(ev, inRowPos);
    if ( row && store )
      outRow = row->AcquireRowHandle(ev, store);
      
    outErr = ev->AsErr();
  }
  if ( acqRow )
    *acqRow = outRow;
  return outErr;
}

/*virtual*/ mdb_err
orkinTable::RowToPos( // test for the table position of a row member
  nsIMdbEnv* mev, // context
  nsIMdbRow* ioRow, // row to find in table
  mdb_pos* outPos) // zero-based ordinal position of row in table
{
  mdb_err outErr = 0;
  mork_pos pos = -1;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkRow* row = 0;
    orkinRow* orow = (orkinRow*) ioRow;
    if ( orow->CanUseRow(mev, /*inMutable*/ morkBool_kFalse, &outErr, &row) )
    {
      morkTable* table = (morkTable*) mHandle_Object;
      pos = table->ArrayHasOid(ev, &row->mRow_Oid);
    }
    outErr = ev->AsErr();
  }
  if ( outPos )
    *outPos = pos;
  return outErr;
}
  
// Note that HasRow() performs the inverse oid->pos mapping
// } ----- end row position methods -----

// { ----- begin oid set methods -----
/*virtual*/ mdb_err
orkinTable::AddOid( // make sure the row with inOid is a table member 
  nsIMdbEnv* mev, // context
  const mdbOid* inOid) // row to ensure membership in table
{
  MORK_USED_1(inOid);
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinTable::HasOid( // test for the table position of a row member
  nsIMdbEnv* mev, // context
  const mdbOid* inOid, // row to find in table
  mdb_bool* outHasOid) // whether inOid is a member row
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    if ( outHasOid )
      *outHasOid = ((morkTable*) mHandle_Object)->MapHasOid(ev, inOid);
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinTable::CutOid( // make sure the row with inOid is not a member 
  nsIMdbEnv* mev, // context
  const mdbOid* inOid) // row to remove from table
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTable* table = (morkTable*) mHandle_Object;
    morkStore* store = table->mTable_Store;
    if ( inOid && store )
    {
      morkRow* row = store->GetRow(ev, inOid);
      if ( row )
        table->CutRow(ev, row);
    }
    else
      ev->NilPointerError();
      
    outErr = ev->AsErr();
  }
  return outErr;
}
// } ----- end oid set methods -----

// { ----- begin row set methods -----
/*virtual*/ mdb_err
orkinTable::NewRow( // create a new row instance in table
  nsIMdbEnv* mev, // context
  mdbOid* ioOid, // please use zero (unbound) rowId for db-assigned IDs
  nsIMdbRow** acqRow) // create new row
{
  mdb_err outErr = 0;
  nsIMdbRow* outRow = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTable* table = (morkTable*) mHandle_Object;
    morkStore* store = table->mTable_Store;
    if ( ioOid && store )
    {
      morkRow* row = 0;
      if ( ioOid->mOid_Id == morkRow_kMinusOneRid )
        row = store->NewRow(ev, ioOid->mOid_Scope);
      else
        row = store->NewRowWithOid(ev, ioOid);
        
      if ( row && table->AddRow(ev, row) )
        outRow = row->AcquireRowHandle(ev, store);
    }
    else
      ev->NilPointerError();
      
    outErr = ev->AsErr();
  }
  if ( acqRow )
    *acqRow = outRow;
  return outErr;
}

/*virtual*/ mdb_err
orkinTable::AddRow( // make sure the row with inOid is a table member 
  nsIMdbEnv* mev, // context
  nsIMdbRow* ioRow) // row to ensure membership in table
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkRow* row = 0;
    orkinRow* orow = (orkinRow*) ioRow;
    if ( orow->CanUseRow(mev, /*inMutable*/ morkBool_kFalse, &outErr, &row) )
    {
      ((morkTable*) mHandle_Object)->AddRow(ev, row);
    }
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinTable::HasRow( // test for the table position of a row member
  nsIMdbEnv* mev, // context
  nsIMdbRow* ioRow, // row to find in table
  mdb_bool* outBool) // zero-based ordinal position of row in table
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkRow* row = 0;
    orkinRow* orow = (orkinRow*) ioRow;
    if ( orow->CanUseRow(mev, /*inMutable*/ morkBool_kFalse, &outErr, &row) )
    {
      morkTable* table = (morkTable*) mHandle_Object;
      if ( outBool )
        *outBool = table->MapHasOid(ev, &row->mRow_Oid);
    }
    outErr = ev->AsErr();
  }
  return outErr;
}


/*virtual*/ mdb_err
orkinTable::CutRow( // make sure the row with inOid is not a member 
  nsIMdbEnv* mev, // context
  nsIMdbRow* ioRow) // row to remove from table
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkRow* row = 0;
    orkinRow* orow = (orkinRow*) ioRow;
    if ( orow->CanUseRow(mev, /*inMutable*/ morkBool_kFalse, &outErr, &row) )
    {
      ((morkTable*) mHandle_Object)->CutRow(ev, row);
    }
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinTable::CutAllRows( // remove all rows from the table 
  nsIMdbEnv* mev) // context
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ((morkTable*) mHandle_Object)->CutAllRows(ev);
    outErr = ev->AsErr();
  }
  return outErr;
}
// } ----- end row set methods -----

// { ----- begin searching methods -----
/*virtual*/ mdb_err
orkinTable::FindRowMatches( // search variable number of sorted cols
  nsIMdbEnv* mev, // context
  const mdbYarn* inPrefix, // content to find as prefix in row's column cell
  nsIMdbTableRowCursor** acqCursor) // set of matching rows
{
  MORK_USED_1(inPrefix);
  nsIMdbTableRowCursor* outCursor = 0;
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  if ( acqCursor )
    *acqCursor = outCursor;
  return outErr;
}
  
/*virtual*/ mdb_err
orkinTable::GetSearchColumns( // query columns used by FindRowMatches()
  nsIMdbEnv* mev, // context
  mdb_count* outCount, // context
  mdbColumnSet* outColSet) // caller supplied space to put columns
  // GetSearchColumns() returns the columns actually searched when the
  // FindRowMatches() method is called.  No more than mColumnSet_Count
  // slots of mColumnSet_Columns will be written, since mColumnSet_Count
  // indicates how many slots are present in the column array.  The
  // actual number of search column used by the table is returned in
  // the outCount parameter; if this number exceeds mColumnSet_Count,
  // then a caller needs a bigger array to read the entire column set.
  // The minimum of mColumnSet_Count and outCount is the number slots
  // in mColumnSet_Columns that were actually written by this method.
  //
  // Callers are expected to change this set of columns by calls to
  // nsIMdbTable::SearchColumnsHint() or SetSearchSorting(), or both.
{
  MORK_USED_1(outColSet);
  mdb_count count = 0;
  mdb_err outErr = 0;
//  nsIMdbThumb* outThumb = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  if ( outCount )
    *outCount = count;
  return outErr;
}
// } ----- end searching methods -----

// { ----- begin hinting methods -----
/*virtual*/ mdb_err
orkinTable::SearchColumnsHint( // advise re future expected search cols  
  nsIMdbEnv* mev, // context
  const mdbColumnSet* inColumnSet) // columns likely to be searched
{
  MORK_USED_1(inColumnSet);
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    // ev->StubMethodOnlyError(); // legal to do nothing
    outErr = ev->AsErr();
  }
  return outErr;
}
  
/*virtual*/ mdb_err
orkinTable::SortColumnsHint( // advise re future expected sort columns  
  nsIMdbEnv* mev, // context
  const mdbColumnSet* inColumnSet) // columns for likely sort requests
{
  MORK_USED_1(inColumnSet);
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    // ev->StubMethodOnlyError(); // legal to do nothing
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinTable::StartBatchChangeHint( // advise before many adds and cuts  
  nsIMdbEnv* mev, // context
  const void* inLabel) // intend unique address to match end call
  // If batch starts nest by virtue of nesting calls in the stack, then
  // the address of a local variable makes a good batch start label that
  // can be used at batch end time, and such addresses remain unique.
{
  MORK_USED_1(inLabel);
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    // ev->StubMethodOnlyError(); // legal to do nothing
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinTable::EndBatchChangeHint( // advise before many adds and cuts  
  nsIMdbEnv* mev, // context
  const void* inLabel) // label matching start label
  // Suppose a table is maintaining one or many sort orders for a table,
  // so that every row added to the table must be inserted in each sort,
  // and every row cut must be removed from each sort.  If a db client
  // intends to make many such changes before needing any information
  // about the order or positions of rows inside a table, then a client
  // might tell the table to start batch changes in order to disable
  // sorting of rows for the interim.  Presumably a table will then do
  // a full sort of all rows at need when the batch changes end, or when
  // a surprise request occurs for row position during batch changes.
{
  MORK_USED_1(inLabel);
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    // ev->StubMethodOnlyError(); // legal to do nothing
    outErr = ev->AsErr();
  }
  return outErr;
}
// } ----- end hinting methods -----

// { ----- begin sorting methods -----
// sorting: note all rows are assumed sorted by row ID as a secondary
// sort following the primary column sort, when table rows are sorted.

/*virtual*/ mdb_err
orkinTable::CanSortColumn( // query which column is currently used for sorting
  nsIMdbEnv* mev, // context
  mdb_column inColumn, // column to query sorting potential
  mdb_bool* outCanSort) // whether the column can be sorted
{
  MORK_USED_1(inColumn);
  mdb_bool canSort = mdbBool_kFalse;
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    // ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  if ( outCanSort )
    *outCanSort = canSort;
  return outErr;
}

/*virtual*/ mdb_err
orkinTable::GetSorting( // view same table in particular sorting
  nsIMdbEnv* mev, // context
  mdb_column inColumn, // requested new column for sorting table
  nsIMdbSorting** acqSorting) // acquire sorting for column
{
  MORK_USED_1(inColumn);
  nsIMdbSorting* outSorting = 0;
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  if ( acqSorting )
    *acqSorting = outSorting;
  return outErr;
}

/*virtual*/ mdb_err
orkinTable::SetSearchSorting( // use this sorting in FindRowMatches()
  nsIMdbEnv* mev, // context
  mdb_column inColumn, // often same as nsIMdbSorting::GetSortColumn()
  nsIMdbSorting* ioSorting) // requested sorting for some column
  // SetSearchSorting() attempts to inform the table that ioSorting
  // should be used during calls to FindRowMatches() for searching
  // the column which is actually sorted by ioSorting.  This method
  // is most useful in conjunction with nsIMdbSorting::SetCompare(),
  // because otherwise a caller would not be able to override the
  // comparison ordering method used during searchs.  Note that some
  // database implementations might be unable to use an arbitrarily
  // specified sort order, either due to schema or runtime interface
  // constraints, in which case ioSorting might not actually be used.
  // Presumably ioSorting is an instance that was returned from some
  // earlier call to nsIMdbTable::GetSorting().  A caller can also
  // use nsIMdbTable::SearchColumnsHint() to specify desired change
  // in which columns are sorted and searched by FindRowMatches().
  //
  // A caller can pass a nil pointer for ioSorting to request that
  // column inColumn no longer be used at all by FindRowMatches().
  // But when ioSorting is non-nil, then inColumn should match the
  // column actually sorted by ioSorting; when these do not agree,
  // implementations are instructed to give precedence to the column
  // specified by ioSorting (so this means callers might just pass
  // zero for inColumn when ioSorting is also provided, since then
  // inColumn is both redundant and ignored).
{
  MORK_USED_1(inColumn);
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    if ( ioSorting )
    {
      ev->StubMethodOnlyError();
    }
    else
      ev->NilPointerError();
      
    outErr = ev->AsErr();
  }
  return outErr;
}

// } ----- end sorting methods -----

// { ----- begin moving methods -----
// moving a row does nothing unless a table is currently unsorted

/*virtual*/ mdb_err
orkinTable::MoveOid( // change position of row in unsorted table
  nsIMdbEnv* mev, // context
  const mdbOid* inOid,  // row oid to find in table
  mdb_pos inHintFromPos, // suggested hint regarding start position
  mdb_pos inToPos,       // desired new position for row inOid
  mdb_pos* outActualPos) // actual new position of row in table
{
  mdb_err outErr = 0;
  mdb_pos actualPos = -1; // meaning it was never found in table
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTable* table = (morkTable*) mHandle_Object;
    morkStore* store = table->mTable_Store;
    if ( inOid && store )
    {
      morkRow* row = store->GetRow(ev, inOid);
      if ( row )
        actualPos = table->MoveRow(ev, row, inHintFromPos, inToPos);
    }
    else
      ev->NilPointerError();

    outErr = ev->AsErr();
  }
  if ( outActualPos )
    *outActualPos = actualPos;
  return outErr;
}

/*virtual*/ mdb_err
orkinTable::MoveRow( // change position of row in unsorted table
  nsIMdbEnv* mev, // context
  nsIMdbRow* ioRow,  // row oid to find in table
  mdb_pos inHintFromPos, // suggested hint regarding start position
  mdb_pos inToPos,       // desired new position for row ioRow
  mdb_pos* outActualPos) // actual new position of row in table
{
  mdb_pos actualPos = -1; // meaning it was never found in table
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkRow* row = 0;
    orkinRow* orow = (orkinRow*) ioRow;
    if ( orow->CanUseRow(mev, /*inMutable*/ morkBool_kFalse, &outErr, &row) )
    {
      morkTable* table = (morkTable*) mHandle_Object;
      actualPos = table->MoveRow(ev, row, inHintFromPos, inToPos);
    }
    outErr = ev->AsErr();
  }
  if ( outActualPos )
    *outActualPos = actualPos;
  return outErr;
}
// } ----- end moving methods -----

// { ----- begin index methods -----
/*virtual*/ mdb_err
orkinTable::AddIndex( // create a sorting index for column if possible
  nsIMdbEnv* mev, // context
  mdb_column inColumn, // the column to sort by index
  nsIMdbThumb** acqThumb) // acquire thumb for incremental index building
// Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
// then the index addition will be finished.
{
  MORK_USED_1(inColumn);
  nsIMdbThumb* outThumb = 0;
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    // ev->StubMethodOnlyError(); // legal to do nothing
     
    outErr = ev->AsErr();
  }
  if ( acqThumb )
    *acqThumb = outThumb;

  return outErr;
}

/*virtual*/ mdb_err
orkinTable::CutIndex( // stop supporting a specific column index
  nsIMdbEnv* mev, // context
  mdb_column inColumn, // the column with index to be removed
  nsIMdbThumb** acqThumb) // acquire thumb for incremental index destroy
// Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
// then the index removal will be finished.
{
  MORK_USED_1(inColumn);
  mdb_err outErr = 0;
  nsIMdbThumb* outThumb = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    // ev->StubMethodOnlyError(); // legal to do nothing
    
    outErr = ev->AsErr();
  }
  if ( acqThumb )
    *acqThumb = outThumb;
    
  return outErr;
}

/*virtual*/ mdb_err
orkinTable::HasIndex( // query for current presence of a column index
  nsIMdbEnv* mev, // context
  mdb_column inColumn, // the column to investigate
  mdb_bool* outHasIndex) // whether column has index for this column
{
  MORK_USED_1(inColumn);
  mdb_bool hasIndex = morkBool_kFalse;
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
      
    outErr = ev->AsErr();
  }
  if ( outHasIndex )
    *outHasIndex = hasIndex;
  return outErr;
}

/*virtual*/ mdb_err
orkinTable::EnableIndexOnSort( // create an index for col on first sort
  nsIMdbEnv* mev, // context
  mdb_column inColumn) // the column to index if ever sorted
{
  MORK_USED_1(inColumn);
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    // ev->StubMethodOnlyError(); // legal to do nothing
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinTable::QueryIndexOnSort( // check whether index on sort is enabled
  nsIMdbEnv* mev, // context
  mdb_column inColumn, // the column to investigate
  mdb_bool* outIndexOnSort) // whether column has index-on-sort enabled
{
  MORK_USED_1(inColumn);
  mdb_bool indexOnSort = morkBool_kFalse;
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
      
    outErr = ev->AsErr();
  }
  if ( outIndexOnSort )
    *outIndexOnSort = indexOnSort;
  return outErr;
}

/*virtual*/ mdb_err
orkinTable::DisableIndexOnSort( // prevent future index creation on sort
  nsIMdbEnv* mev, // context
  mdb_column inColumn) // the column to index if ever sorted
{
  MORK_USED_1(inColumn);
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseTable(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    // ev->StubMethodOnlyError(); // legal to do nothing
    outErr = ev->AsErr();
  }
  return outErr;
}
// } ----- end index methods -----

// } ===== end nsIMdbTable methods =====


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

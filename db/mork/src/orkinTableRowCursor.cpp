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

#ifndef _MORKFACTORY_
#include "morkFactory.h"
#endif

#ifndef _ORKINFACTORY_
#include "orkinFactory.h"
#endif

#ifndef _MORKTABLEROWCURSOR_
#include "morkTableRowCursor.h"
#endif

#ifndef _ORKINTABLEROWCURSOR_
#include "orkinTableRowCursor.h"
#endif

#ifndef _MORKROW_
#include "morkRow.h"
#endif

#ifndef _MORKARRAY_
#include "morkArray.h"
#endif

#ifndef _MORKTABLE_
#include "morkTable.h"
#endif

#ifndef _ORKINTABLE_
#include "orkinTable.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

/* public virtual*/
orkinTableRowCursor:: ~orkinTableRowCursor()
// morkHandle destructor does everything
{
}

/*protected non-poly construction*/
orkinTableRowCursor::orkinTableRowCursor(morkEnv* ev, //  morkUsage_kPool
    morkHandleFace* ioFace,    // must not be nil, cookie for this handle
    morkTableRowCursor* ioObject)  // must not be nil, object for this handle
: morkHandle(ev, ioFace, ioObject, morkMagic_kTableRowCursor)
{
  // do not modify mNode_Derived; leave it equal to morkDerived_kHandle
}


/*static */ orkinTableRowCursor*
orkinTableRowCursor::MakeTableRowCursor(morkEnv* ev,
   morkTableRowCursor* ioObject)
{
  mork_bool isEnv = ev->IsEnv();
  MORK_ASSERT(isEnv);
  if ( isEnv )
  {
    morkHandleFace* face = ev->NewHandle(sizeof(orkinTableRowCursor));
    if ( face )
      return new(face) orkinTableRowCursor(ev, face, ioObject);
    else
      ev->OutOfMemoryError();
  }
    
  return (orkinTableRowCursor*) 0;
}

morkEnv*
orkinTableRowCursor::CanUseTableRowCursor(nsIMdbEnv* mev,
  mork_bool inMutable, mdb_err* outErr) const
{
  morkEnv* outEnv = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    morkTableRowCursor* self = (morkTableRowCursor*)
      this->GetGoodHandleObject(ev, inMutable, morkMagic_kTableRowCursor,
        /*inClosedOkay*/ morkBool_kFalse);
    if ( self )
    {
      if ( self->IsTableRowCursor() )
      {
        morkTable* table = self->mTableRowCursor_Table;
        if ( table && table->IsOpenNode() )
        {
          outEnv = ev;
        }
      }
      else
        self->NonTableRowCursorTypeError(ev);
    }
    *outErr = ev->AsErr();
  }
  MORK_ASSERT(outEnv);
  return outEnv;
}

// { ===== begin nsIMdbISupports methods =====
/*virtual*/ mdb_err
orkinTableRowCursor::AddRef() // add strong ref with no
{
  morkEnv* ev = mHandle_Env;
  if ( ev && ev->IsEnv() )
    return this->Handle_AddStrongRef(ev->AsMdbEnv());
  else
    return morkEnv_kNonEnvTypeError;
}

/*virtual*/ mdb_err
orkinTableRowCursor::Release() // cut strong ref
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
orkinTableRowCursor::IsFrozenMdbObject(nsIMdbEnv* mev, mdb_bool* outIsReadonly)
{
  return this->Handle_IsFrozenMdbObject(mev, outIsReadonly);
}
// same as nsIMdbPort::GetIsPortReadonly() when this object is inside a port.
// } ----- end attribute methods -----

// { ----- begin factory methods -----
/*virtual*/ mdb_err
orkinTableRowCursor::GetMdbFactory(nsIMdbEnv* mev, nsIMdbFactory** acqFactory)
{
  return this->Handle_GetMdbFactory(mev, acqFactory);
} 
// } ----- end factory methods -----

// { ----- begin ref counting for well-behaved cyclic graphs -----
/*virtual*/ mdb_err
orkinTableRowCursor::GetWeakRefCount(nsIMdbEnv* mev, // weak refs
  mdb_count* outCount)
{
  return this->Handle_GetWeakRefCount(mev, outCount);
}  
/*virtual*/ mdb_err
orkinTableRowCursor::GetStrongRefCount(nsIMdbEnv* mev, // strong refs
  mdb_count* outCount)
{
  return this->Handle_GetStrongRefCount(mev, outCount);
}

/*virtual*/ mdb_err
orkinTableRowCursor::AddWeakRef(nsIMdbEnv* mev)
{
  return this->Handle_AddWeakRef(mev);
}
/*virtual*/ mdb_err
orkinTableRowCursor::AddStrongRef(nsIMdbEnv* mev)
{
  return this->Handle_AddStrongRef(mev);
}

/*virtual*/ mdb_err
orkinTableRowCursor::CutWeakRef(nsIMdbEnv* mev)
{
  return this->Handle_CutWeakRef(mev);
}
/*virtual*/ mdb_err
orkinTableRowCursor::CutStrongRef(nsIMdbEnv* mev)
{
  return this->Handle_CutStrongRef(mev);
}

/*virtual*/ mdb_err
orkinTableRowCursor::CloseMdbObject(nsIMdbEnv* mev)
{
  return this->Handle_CloseMdbObject(mev);
}

/*virtual*/ mdb_err
orkinTableRowCursor::IsOpenMdbObject(nsIMdbEnv* mev, mdb_bool* outOpen)
{
  return this->Handle_IsOpenMdbObject(mev, outOpen);
}
// } ----- end ref counting -----

// } ===== end nsIMdbObject methods =====

// { ===== begin nsIMdbCursor methods =====

// { ----- begin attribute methods -----
/*virtual*/ mdb_err
orkinTableRowCursor::GetCount(nsIMdbEnv* mev, mdb_count* outCount)
{
  mdb_err outErr = 0;
  mdb_count count = 0;
  morkEnv* ev =
    this->CanUseTableRowCursor(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTableRowCursor* cursor = (morkTableRowCursor*) mHandle_Object;
    count = cursor->GetMemberCount(ev);
    outErr = ev->AsErr();
  }
  if ( outCount )
    *outCount = count;
  return outErr;
}

/*virtual*/ mdb_err
orkinTableRowCursor::GetSeed(nsIMdbEnv* mev, mdb_seed* outSeed)
{
  mdb_err outErr = 0;
  mdb_seed seed = 0;
  morkEnv* ev =
    this->CanUseTableRowCursor(mev, /*inMutable*/ morkBool_kFalse, &outErr);
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
orkinTableRowCursor::SetPos(nsIMdbEnv* mev, mdb_pos inPos)
{
  mdb_err outErr = 0;
  morkEnv* ev =
    this->CanUseTableRowCursor(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTableRowCursor* cursor = (morkTableRowCursor*) mHandle_Object;
    cursor->mCursor_Pos = inPos;
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinTableRowCursor::GetPos(nsIMdbEnv* mev, mdb_pos* outPos)
{
  mdb_err outErr = 0;
  mdb_pos pos = 0;
  morkEnv* ev =
    this->CanUseTableRowCursor(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTableRowCursor* cursor = (morkTableRowCursor*) mHandle_Object;
    pos = cursor->mCursor_Pos;
    outErr = ev->AsErr();
  }
  if ( outPos )
    *outPos = pos;
  return outErr;
}

/*virtual*/ mdb_err
orkinTableRowCursor::SetDoFailOnSeedOutOfSync(nsIMdbEnv* mev, mdb_bool inFail)
{
  MORK_USED_1(inFail);
  mdb_err outErr = 0;
  morkEnv* ev =
    this->CanUseTableRowCursor(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTableRowCursor* cursor = (morkTableRowCursor*) mHandle_Object;
    cursor->mCursor_DoFailOnSeedOutOfSync = inFail;
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinTableRowCursor::GetDoFailOnSeedOutOfSync(nsIMdbEnv* mev, mdb_bool* outFail)
{
  mdb_bool fail = morkBool_kFalse;
  mdb_err outErr = 0;
  morkEnv* ev =
    this->CanUseTableRowCursor(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTableRowCursor* cursor = (morkTableRowCursor*) mHandle_Object;
    fail = cursor->mCursor_DoFailOnSeedOutOfSync;
    outErr = ev->AsErr();
  }
  if ( outFail )
    *outFail = fail;
  return outErr;
}
// } ----- end attribute methods -----

// } ===== end nsIMdbCursor methods =====


// { ===== begin nsIMdbTableRowCursor methods =====

// { ----- begin attribute methods -----

/*virtual*/ mdb_err
orkinTableRowCursor::GetTable(nsIMdbEnv* mev, nsIMdbTable** acqTable)
{
  mdb_err outErr = 0;
  nsIMdbTable* outTable = 0;
  morkEnv* ev =
    this->CanUseTableRowCursor(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTableRowCursor* cursor = (morkTableRowCursor*) mHandle_Object;
    morkTable* table = cursor->mTableRowCursor_Table;
    if ( table )
      outTable = table->AcquireTableHandle(ev);
    
    outErr = ev->AsErr();
  }
  if ( acqTable )
    *acqTable = outTable;
  return outErr;
}
// } ----- end attribute methods -----

// { ----- begin oid iteration methods -----
/*virtual*/ mdb_err
orkinTableRowCursor::NextRowOid( // get row id of next row in the table
  nsIMdbEnv* mev, // context
  mdbOid* outOid, // out row oid
  mdb_pos* outRowPos)
{
  mdb_err outErr = 0;
  mork_pos pos = -1;
  morkEnv* ev =
    this->CanUseTableRowCursor(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    if ( outOid )
    {
      pos = ((morkTableRowCursor*) mHandle_Object)->NextRowOid(ev, outOid);
    }
    else
      ev->NilPointerError();
    outErr = ev->AsErr();
  }
  if ( outRowPos )
    *outRowPos = pos;
  return outErr;
}
// } ----- end oid iteration methods -----

// { ----- begin row iteration methods -----
/*virtual*/ mdb_err
orkinTableRowCursor::NextRow( // get row cells from table for cells already in row
  nsIMdbEnv* mev, // context
  nsIMdbRow** acqRow, // acquire next row in table
  mdb_pos* outRowPos)
{
  mdb_err outErr = 0;
  nsIMdbRow* outRow = 0;
  morkEnv* ev =
    this->CanUseTableRowCursor(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTableRowCursor* cursor = (morkTableRowCursor*) mHandle_Object;
      
    mdbOid oid; // place to put oid we intend to ignore
    morkRow* row = cursor->NextRow(ev, &oid, outRowPos);
    if ( row )
    {
      morkStore* store = row->GetRowSpaceStore(ev);
      if ( store )
        outRow = row->AcquireRowHandle(ev, store);
    }
    outErr = ev->AsErr();
  }
  if ( acqRow )
    *acqRow = outRow;
  return outErr;
}
// } ----- end row iteration methods -----


// { ----- begin duplicate row removal methods -----
/*virtual*/ mdb_err
orkinTableRowCursor::CanHaveDupRowMembers(nsIMdbEnv* mev, // cursor might hold dups?
  mdb_bool* outCanHaveDups)
{
  mdb_err outErr = 0;
  mdb_bool canHaveDups = mdbBool_kFalse;
  
  morkEnv* ev =
    this->CanUseTableRowCursor(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTableRowCursor* cursor = (morkTableRowCursor*) mHandle_Object;
    canHaveDups = cursor->CanHaveDupRowMembers(ev);
    outErr = ev->AsErr();
  }
  if ( outCanHaveDups )
    *outCanHaveDups = canHaveDups;
  return outErr;
}
  
/*virtual*/ mdb_err
orkinTableRowCursor::MakeUniqueCursor( // clone cursor, removing duplicate rows
  nsIMdbEnv* mev, // context
  nsIMdbTableRowCursor** acqCursor)    // acquire clone with no dups
  // Note that MakeUniqueCursor() is never necessary for a cursor which was
  // created by table method nsIMdbTable::GetTableRowCursor(), because a table
  // never contains the same row as a member more than once.  However, a cursor
  // created by table method nsIMdbTable::FindRowMatches() might contain the
  // same row more than once, because the same row can generate a hit by more
  // than one column with a matching string prefix.  Note this method can
  // return the very same cursor instance with just an incremented refcount,
  // when the original cursor could not contain any duplicate rows (calling
  // CanHaveDupRowMembers() shows this case on a false return).  Otherwise
  // this method returns a different cursor instance.  Callers should not use
  // this MakeUniqueCursor() method lightly, because it tends to defeat the
  // purpose of lazy programming techniques, since it can force creation of
  // an explicit row collection in a new cursor's representation, in order to
  // inspect the row membership and remove any duplicates; this can have big
  // impact if a collection holds tens of thousands of rows or more, when
  // the original cursor with dups simply referenced rows indirectly by row
  // position ranges, without using an explicit row set representation.
  // Callers are encouraged to use nsIMdbCursor::GetCount() to determine
  // whether the row collection is very large (tens of thousands), and to
  // delay calling MakeUniqueCursor() when possible, until a user interface
  // element actually demands the creation of an explicit set representation.
{
  mdb_err outErr = 0;
  nsIMdbTableRowCursor* outCursor = 0;
  
  morkEnv* ev =
    this->CanUseTableRowCursor(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    if ( this->Handle_AddStrongRef(mev) == 0 )
      outCursor = this;
      
    outErr = ev->AsErr();
  }
  if ( acqCursor )
    *acqCursor = outCursor;
  return outErr;
}
// } ----- end duplicate row removal methods -----

// } ===== end nsIMdbTableRowCursor methods =====


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

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

#ifndef _MORKSORTING_
#include "morkSorting.h"
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

#ifndef _ORKINSORTING_
#include "orkinSorting.h"
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
orkinSorting:: ~orkinSorting() // morkHandle destructor does everything
{
}

/*protected non-poly construction*/
orkinSorting::orkinSorting(morkEnv* ev, // morkUsage is morkUsage_kPool
    morkHandleFace* ioFace,    // must not be nil, cookie for this handle
    morkSorting* ioObject)  // must not be nil, the object for this handle
: morkHandle(ev, ioFace, ioObject, morkMagic_kSorting)
{
  // do not modify mNode_Derived; leave it equal to morkDerived_kHandle
}


/*static */ orkinSorting*
orkinSorting::MakeSorting(morkEnv* ev, morkSorting* ioObject)
{
  mork_bool isEnv = ev->IsEnv();
  MORK_ASSERT(isEnv);
  if ( isEnv )
  {
    morkHandleFace* face = ev->NewHandle(sizeof(orkinSorting));
    if ( face )
      return new(face) orkinSorting(ev, face, ioObject);
    else
      ev->OutOfMemoryError();
  }
    
  return (orkinSorting*) 0;
}

morkEnv*
orkinSorting::CanUseSorting(nsIMdbEnv* mev,
  mork_bool inMutable, mdb_err* outErr) const
{
  morkEnv* outEnv = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    morkSorting* self = (morkSorting*)
      this->GetGoodHandleObject(ev, inMutable, morkMagic_kSorting,
        /*inClosedOkay*/ morkBool_kFalse);
    if ( self )
    {
      if ( self->IsSorting() )
        outEnv = ev;
      else
        self->NonSortingTypeError(ev);
    }
    *outErr = ev->AsErr();
  }
  MORK_ASSERT(outEnv);
  return outEnv;
}


// { ===== begin nsIMdbISupports methods =====
/*virtual*/ mdb_err
orkinSorting::AddRef() // add strong ref with no
{
  morkEnv* ev = mHandle_Env;
  if ( ev && ev->IsEnv() )
    return this->Handle_AddStrongRef(ev->AsMdbEnv());
  else
    return morkEnv_kNonEnvTypeError;
}

/*virtual*/ mdb_err
orkinSorting::Release() // cut strong ref
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
orkinSorting::IsFrozenMdbObject(nsIMdbEnv* mev, mdb_bool* outIsReadonly)
{
  return this->Handle_IsFrozenMdbObject(mev, outIsReadonly);
}
// same as nsIMdbPort::GetIsPortReadonly() when this object is inside a port.
// } ----- end attribute methods -----

// { ----- begin factory methods -----
/*virtual*/ mdb_err
orkinSorting::GetMdbFactory(nsIMdbEnv* mev, nsIMdbFactory** acqFactory)
{
  return this->Handle_GetMdbFactory(mev, acqFactory);
} 
// } ----- end factory methods -----

// { ----- begin ref counting for well-behaved cyclic graphs -----
/*virtual*/ mdb_err
orkinSorting::GetWeakRefCount(nsIMdbEnv* mev, // weak refs
  mdb_count* outCount)
{
  return this->Handle_GetWeakRefCount(mev, outCount);
}  
/*virtual*/ mdb_err
orkinSorting::GetStrongRefCount(nsIMdbEnv* mev, // strong refs
  mdb_count* outCount)
{
  return this->Handle_GetStrongRefCount(mev, outCount);
}

/*virtual*/ mdb_err
orkinSorting::AddWeakRef(nsIMdbEnv* mev)
{
  return this->Handle_AddWeakRef(mev);
}
/*virtual*/ mdb_err
orkinSorting::AddStrongRef(nsIMdbEnv* mev)
{
  return this->Handle_AddStrongRef(mev);
}

/*virtual*/ mdb_err
orkinSorting::CutWeakRef(nsIMdbEnv* mev)
{
  return this->Handle_CutWeakRef(mev);
}
/*virtual*/ mdb_err
orkinSorting::CutStrongRef(nsIMdbEnv* mev)
{
  return this->Handle_CutStrongRef(mev);
}

/*virtual*/ mdb_err
orkinSorting::CloseMdbObject(nsIMdbEnv* mev)
{
  return this->Handle_CloseMdbObject(mev);
}

/*virtual*/ mdb_err
orkinSorting::IsOpenMdbObject(nsIMdbEnv* mev, mdb_bool* outOpen)
{
  return this->Handle_IsOpenMdbObject(mev, outOpen);
}
// } ----- end ref counting -----

// } ===== end nsIMdbObject methods =====


// { ===== begin nsIMdbSorting methods =====

// { ----- begin attribute methods -----


/*virtual*/ mdb_err
orkinSorting::GetTable(nsIMdbEnv* mev, nsIMdbTable** acqTable)
{
  mdb_err outErr = 0;
  nsIMdbTable* outTable = 0;
  morkEnv* ev = this->CanUseSorting(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkSorting* sorting = (morkSorting*) mHandle_Object;
    morkTable* table = sorting->mSorting_Table;
    if ( table && ev->Good() )
    {
      outTable = table->AcquireTableHandle(ev);
    }
    outErr = ev->AsErr();
  }
  if ( acqTable )
    *acqTable = outTable;

  return outErr;
}

/*virtual*/ mdb_err
orkinSorting::GetSortColumn( // query which col is currently sorted
  nsIMdbEnv* mev, // context
  mdb_column* outColumn) // col the table uses for sorting (or zero)
{
  mdb_err outErr = 0;
  mdb_column col = 0;
  morkEnv* ev = this->CanUseSorting(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkSorting* sorting = (morkSorting*) mHandle_Object;
    col = sorting->mSorting_Col;

    outErr = ev->AsErr();
  }
  if ( outColumn )
    *outColumn = col;

  return outErr;
}

/*virtual*/ mdb_err
orkinSorting::SetNewCompare(nsIMdbEnv* mev,
  nsIMdbCompare* ioNewCompare)
  // Setting the sorting's compare object will typically cause the rows
  // to be resorted, presumably in a lazy fashion when the sorting is
  // next required to be in a valid row ordering state, such as when a
  // call to PosToOid() happens.  ioNewCompare can be nil, in which case
  // implementations should revert to the default sort order, which must
  // be equivalent to whatever is used by nsIMdbFactory::MakeCompare().
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseSorting(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    if ( ioNewCompare )
    {
      morkSorting* sorting = (morkSorting*) mHandle_Object;
      nsIMdbCompare_SlotStrongCompare(ioNewCompare, ev,
        &sorting->mSorting_Compare);
    }
    else
      ev->NilPointerError();
      
    outErr = ev->AsErr();
  }

  return outErr;
}

/*virtual*/ mdb_err
orkinSorting::GetOldCompare(nsIMdbEnv* mev,
  nsIMdbCompare** acqOldCompare)
  // Get this sorting instance's compare object, which handles the
  // ordering of rows in the sorting, by comparing yarns from the cells
  // in the column being sorted.  Since nsIMdbCompare has no interface
  // to query the state of the compare object, it is not clear what you
  // would do with this object when returned, except maybe compare it
  // as a pointer address to some other instance, to check identities.
{
  mdb_err outErr = 0;
  nsIMdbCompare* outCompare = 0;
  morkEnv* ev = this->CanUseSorting(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkSorting* sorting = (morkSorting*) mHandle_Object;
    nsIMdbCompare* compare = sorting->mSorting_Compare;
    if ( compare && ev->Good() )
    {
      compare->AddStrongRef(mev);
        
      if ( ev->Good() )
        outCompare = compare;
    }
    outErr = ev->AsErr();
  }
  if ( acqOldCompare )
    *acqOldCompare = outCompare;

  return outErr;
}  

// } ----- end attribute methods -----


// { ----- begin cursor methods -----
/*virtual*/ mdb_err
orkinSorting::GetSortingRowCursor( // make a cursor, starting at inRowPos
  nsIMdbEnv* mev, // context
  mdb_pos inRowPos, // zero-based ordinal position of row in table
  nsIMdbTableRowCursor** acqCursor) // acquire new cursor instance
  // A cursor interface turning same info as PosToOid() or PosToRow().
{
  mdb_err outErr = 0;
  nsIMdbTableRowCursor* outCursor = 0;
  morkEnv* ev = this->CanUseSorting(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkSortingRowCursor* cursor =
      ((morkSorting*) mHandle_Object)->NewSortingRowCursor(ev, inRowPos);
    if ( cursor )
    {
      // $$$$$
      // if ( ev->Good() )
      // {
      //   outCursor = cursor->AcquireSortingRowCursorHandle(ev);
      // }
      // else
      //   cursor->CutStrongRef(ev);
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
orkinSorting::PosToOid( // get row member for a table position
  nsIMdbEnv* mev, // context
  mdb_pos inRowPos, // zero-based ordinal position of row in table
  mdbOid* outOid) // row oid at the specified position
{
  mdb_err outErr = 0;
  mdbOid roid;
  roid.mOid_Scope = 0;
  roid.mOid_Id = (mork_id) -1;
  
  morkEnv* ev = this->CanUseSorting(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkSorting* sorting = (morkSorting*) mHandle_Object;
    morkRow* row = sorting->SafeRowAt(ev, inRowPos);
    if ( row )
      roid = row->mRow_Oid;
    
    outErr = ev->AsErr();
  }
  if ( outOid )
    *outOid = roid;
  return outErr;
}

/*virtual*/ mdb_err
orkinSorting::PosToRow( // get row member for a table position
  nsIMdbEnv* mev, // context
  mdb_pos inRowPos, // zero-based ordinal position of row in table
  nsIMdbRow** acqRow) // acquire row at table position inRowPos
{
  mdb_err outErr = 0;
  nsIMdbRow* outRow = 0;
  morkEnv* ev = this->CanUseSorting(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkSorting* sorting = (morkSorting*) mHandle_Object;
    morkStore* store = sorting->mSorting_Table->mTable_Store;
    morkRow* row = sorting->SafeRowAt(ev, inRowPos);
    if ( row && store )
      outRow = row->AcquireRowHandle(ev, store);
      
    outErr = ev->AsErr();
  }
  if ( acqRow )
    *acqRow = outRow;
  return outErr;
}
  
// Note that HasRow() performs the inverse oid->pos mapping
// } ----- end row position methods -----


// } ===== end nsIMdbSorting methods =====


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

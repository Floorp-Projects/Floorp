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

#ifndef _MORKCELL_
#include "morkCell.h"
#endif

#ifndef _MORKCELLOBJECT_
#include "morkCellObject.h"
#endif

#ifndef _MORKROWOBJECT_
#include "morkRowObject.h"
#endif

#ifndef _ORKINCELL_
#include "orkinCell.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _MORKPOOL_
#include "morkPool.h"
#endif

#ifndef _MORKROW_
#include "morkRow.h"
#endif

#ifndef _MORKATOM_
#include "morkAtom.h"
#endif

#ifndef _MORKSPACE_
#include "morkSpace.h"
#endif

#ifndef _MORKROWSPACE_
#include "morkRowSpace.h"
#endif

#ifndef _MORKSTORE_
#include "morkStore.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

/* public virtual*/
orkinCell:: ~orkinCell() // morkHandle destructor does everything
{
}

/*protected non-poly construction*/
orkinCell::orkinCell(morkEnv* ev, // morkUsage is morkUsage_kPool
    morkHandleFace* ioFace,    // must not be nil, cookie for this handle
    morkCellObject* ioObject)  // must not be nil, the object for this handle
: morkHandle(ev, ioFace, ioObject, morkMagic_kCell)
{
  // do not modify mNode_Derived; leave it equal to morkDerived_kHandle
}


/*static */ orkinCell*
orkinCell::MakeCell(morkEnv* ev, morkCellObject* ioObject)
{
  mork_bool isEnv = ev->IsEnv();
  MORK_ASSERT(isEnv);
  if ( isEnv )
  {
    morkHandleFace* face = ev->NewHandle(sizeof(orkinCell));
    if ( face )
      return new(face) orkinCell(ev, face, ioObject);
    else
      ev->OutOfMemoryError();
  }
    
  return (orkinCell*) 0;
}

// ResyncWithRow() moved to the morkCellObject class:
// mork_bool
// orkinCell::ResyncWithRow(morkEnv* ev)
// {
//   morkCellObject* cellObj = (morkCellObject*) mHandle_Object;
//   morkRow* row = cellObj->mCellObject_Row;
//   mork_pos pos = 0;
//   morkCell* cell = row->GetCell(ev, cellObj->mCellObject_Col, &pos);
//   if ( cell )
//   {
//     cellObj->mCellObject_Pos = pos;
//     cellObj->mCellObject_Cell = cell;
//     cellObj->mCellObject_RowSeed = row->mRow_Seed;
//   }
//   else
//   {
//     cellObj->mCellObject_Cell = 0;
//     cellObj->MissingRowColumnError(ev);
//   }
//   return ev->Good();
// }

morkEnv*
orkinCell::CanUseCell(nsIMdbEnv* mev, mork_bool inMutable,
  mdb_err* outErr, morkCell** outCell) const
{
  morkEnv* outEnv = 0;
  morkCell* cell = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    morkCellObject* cellObj = (morkCellObject*)
      this->GetGoodHandleObject(ev, inMutable, morkMagic_kCell,
        /*inClosedOkay*/ morkBool_kFalse);
    if ( cellObj )
    {
      if ( cellObj->IsCellObject() )
      {
        if ( cellObj->IsMutable() || !inMutable )
        {
          morkRowObject* rowObj = cellObj->mCellObject_RowObject;
          if ( rowObj )
          {
            morkRow* row = cellObj->mCellObject_Row;
            if ( row )
            {
              if ( rowObj->mRowObject_Row == row )
              {
                mork_u2 oldSeed = cellObj->mCellObject_RowSeed;
                if ( row->mRow_Seed == oldSeed || cellObj->ResyncWithRow(ev) )
                {
                  cell = cellObj->mCellObject_Cell;
                  if ( cell )
                  {
                    outEnv = ev;
                  }
                  else
                    cellObj->NilCellError(ev);
                }
              }
              else
                cellObj->WrongRowObjectRowError(ev);
            }
            else
              cellObj->NilRowError(ev);
          }
          else
            cellObj->NilRowObjectError(ev);
        }
        else
          cellObj->NonMutableNodeError(ev);
      }
      else
        cellObj->NonCellObjectTypeError(ev);
    }
    *outErr = ev->AsErr();
  }
  MORK_ASSERT(outEnv);
  *outCell = cell;
  
  return outEnv;
}


// { ===== begin nsIMdbISupports methods =====
/*virtual*/ mdb_err
orkinCell::AddRef() // add strong ref with no
{
  morkEnv* ev = mHandle_Env;
  if ( ev && ev->IsEnv() )
    return this->Handle_AddStrongRef(ev->AsMdbEnv());
  else
    return morkEnv_kNonEnvTypeError;
}

/*virtual*/ mdb_err
orkinCell::Release() // cut strong ref
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
orkinCell::IsFrozenMdbObject(nsIMdbEnv* mev, mdb_bool* outIsReadonly)
{
  return this->Handle_IsFrozenMdbObject(mev, outIsReadonly);
}
// same as nsIMdbPort::GetIsPortReadonly() when this object is inside a port.
// } ----- end attribute methods -----

// { ----- begin factory methods -----
/*virtual*/ mdb_err
orkinCell::GetMdbFactory(nsIMdbEnv* mev, nsIMdbFactory** acqFactory)
{
  return this->Handle_GetMdbFactory(mev, acqFactory);
} 
// } ----- end factory methods -----

// { ----- begin ref counting for well-behaved cyclic graphs -----
/*virtual*/ mdb_err
orkinCell::GetWeakRefCount(nsIMdbEnv* mev, // weak refs
  mdb_count* outCount)
{
  return this->Handle_GetWeakRefCount(mev, outCount);
}  
/*virtual*/ mdb_err
orkinCell::GetStrongRefCount(nsIMdbEnv* mev, // strong refs
  mdb_count* outCount)
{
  return this->Handle_GetStrongRefCount(mev, outCount);
}

/*virtual*/ mdb_err
orkinCell::AddWeakRef(nsIMdbEnv* mev)
{
  return this->Handle_AddWeakRef(mev);
}
/*virtual*/ mdb_err
orkinCell::AddStrongRef(nsIMdbEnv* mev)
{
  return this->Handle_AddStrongRef(mev);
}

/*virtual*/ mdb_err
orkinCell::CutWeakRef(nsIMdbEnv* mev)
{
  return this->Handle_CutWeakRef(mev);
}
/*virtual*/ mdb_err
orkinCell::CutStrongRef(nsIMdbEnv* mev)
{
  return this->Handle_CutStrongRef(mev);
}

/*virtual*/ mdb_err
orkinCell::CloseMdbObject(nsIMdbEnv* mev)
{
  return this->Handle_CloseMdbObject(mev);
}

/*virtual*/ mdb_err
orkinCell::IsOpenMdbObject(nsIMdbEnv* mev, mdb_bool* outOpen)
{
  return this->Handle_IsOpenMdbObject(mev, outOpen);
}
// } ----- end ref counting -----

// } ===== end nsIMdbObject methods =====

// { ===== begin nsIMdbBlob methods =====

// { ----- begin attribute methods -----
/*virtual*/ mdb_err
orkinCell::SetBlob(nsIMdbEnv* mev,
  nsIMdbBlob* ioBlob)
{
  MORK_USED_1(ioBlob);
  mdb_err outErr = 0;
  morkCell* cell = 0;
  morkEnv* ev = this->CanUseCell(mev, /*inMutable*/ morkBool_kTrue,
    &outErr, &cell);
  if ( ev )
  {
    // remember row->MaybeDirtySpaceStoreAndRow();

    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
    
  return outErr;
} // reads inBlob slots
// when inBlob is in the same suite, this might be fastest cell-to-cell

/*virtual*/ mdb_err
orkinCell::ClearBlob( // make empty (so content has zero length)
  nsIMdbEnv* mev)
{
  mdb_err outErr = 0;
  morkCell* cell = 0;
  morkEnv* ev = this->CanUseCell(mev, /*inMutable*/ morkBool_kTrue,
    &outErr, &cell);
  if ( ev )
  {
    // remember row->MaybeDirtySpaceStoreAndRow();

    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
    
  return outErr;
}
// clearing a yarn is like SetYarn() with empty yarn instance content

/*virtual*/ mdb_err
orkinCell::GetBlobFill(nsIMdbEnv* mev,
  mdb_fill* outFill)
// Same value that would be put into mYarn_Fill, if one called GetYarn()
// with a yarn instance that had mYarn_Buf==nil and mYarn_Size==0.
{
  mdb_err outErr = 0;
  mdb_fill fill = 0;
  morkCell* cell = 0;
  morkEnv* ev = this->CanUseCell(mev, /*inMutable*/ morkBool_kTrue,
    &outErr, &cell);
  if ( ev )
  {
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  if ( outFill )
    *outFill = fill;
    
  return outErr;
}  // size of blob 

/*virtual*/ mdb_err
orkinCell::SetYarn(nsIMdbEnv* mev, 
  const mdbYarn* inYarn)
{
  mdb_err outErr = 0;
  morkCell* cell = 0;
  morkEnv* ev = this->CanUseCell(mev, /*inMutable*/ morkBool_kTrue,
    &outErr, &cell);
  if ( ev )
  {
    morkCellObject* cellObj = (morkCellObject*) mHandle_Object;
    morkRow* row = cellObj->mCellObject_Row;
    if ( row )
    {
      morkStore* store = row->GetRowSpaceStore(ev);
      if ( store )
      {
        cell->SetYarn(ev, inYarn, store);
        if ( row->IsRowClean() && store->mStore_CanDirty )
          row->MaybeDirtySpaceStoreAndRow();
      }
    }
    else
      ev->NilPointerError();

    outErr = ev->AsErr();
  }
    
  return outErr;
}   // reads from yarn slots
// make this text object contain content from the yarn's buffer

/*virtual*/ mdb_err
orkinCell::GetYarn(nsIMdbEnv* mev, 
  mdbYarn* outYarn)
{
  mdb_err outErr = 0;
  morkCell* cell = 0;
  morkEnv* ev = this->CanUseCell(mev, /*inMutable*/ morkBool_kTrue,
    &outErr, &cell);
  if ( ev )
  {
    morkAtom* atom = cell->GetAtom();
    atom->GetYarn(outYarn);
    outErr = ev->AsErr();
  }
    
  return outErr;
}  // writes some yarn slots 
// copy content into the yarn buffer, and update mYarn_Fill and mYarn_Form

/*virtual*/ mdb_err
orkinCell::AliasYarn(nsIMdbEnv* mev, 
  mdbYarn* outYarn)
{
  mdb_err outErr = 0;
  morkCell* cell = 0;
  morkEnv* ev = this->CanUseCell(mev, /*inMutable*/ morkBool_kTrue,
    &outErr, &cell);
  if ( ev )
  {
    morkAtom* atom = cell->GetAtom();
    atom->AliasYarn(outYarn);
    outErr = ev->AsErr();
  }
    
  return outErr;
} // writes ALL yarn slots

// } ----- end attribute methods -----

// } ===== end nsIMdbBlob methods =====

// { ===== begin nsIMdbCell methods =====

// { ----- begin attribute methods -----
/*virtual*/ mdb_err
orkinCell::SetColumn(nsIMdbEnv* mev, mdb_column inColumn)
{
  MORK_USED_1(inColumn);
  mdb_err outErr = 0;
  morkCell* cell = 0;
  morkEnv* ev = this->CanUseCell(mev, /*inMutable*/ morkBool_kTrue,
    &outErr, &cell);
  if ( ev )
  {
    // remember row->MaybeDirtySpaceStoreAndRow();

    morkCellObject* cellObj = (morkCellObject*) mHandle_Object;
    MORK_USED_1(cellObj);
    
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
    
  return outErr;
} 

/*virtual*/ mdb_err
orkinCell::GetColumn(nsIMdbEnv* mev, mdb_column* outColumn)
{
  mdb_err outErr = 0;
  mdb_column col = 0;
  morkCell* cell = 0;
  morkEnv* ev = this->CanUseCell(mev, /*inMutable*/ morkBool_kTrue,
    &outErr, &cell);
  if ( ev )
  {
    morkCellObject* cellObj = (morkCellObject*) mHandle_Object;
    col = cellObj->mCellObject_Col;
    outErr = ev->AsErr();
  }
  if ( outColumn )
    *outColumn = col;
  return outErr;
}

/*virtual*/ mdb_err
orkinCell::GetCellInfo(  // all cell metainfo except actual content
  nsIMdbEnv* mev, 
  mdb_column* outColumn,           // the column in the containing row
  mdb_fill*   outBlobFill,         // the size of text content in bytes
  mdbOid*     outChildOid,         // oid of possible row or table child
  mdb_bool*   outIsRowChild)  // nonzero if child, and a row child
// Checking all cell metainfo is a good way to avoid forcing a large cell
// in to memory when you don't actually want to use the content.
{
  mdb_err outErr = 0;
  mdb_bool isRowChild = morkBool_kFalse;
  mdbOid childOid;
  childOid.mOid_Scope = 0;
  childOid.mOid_Id = 0;
  mork_fill blobFill = 0;
  mdb_column column = 0;
  morkCell* cell = 0;
  morkEnv* ev = this->CanUseCell(mev, /*inMutable*/ morkBool_kTrue,
    &outErr, &cell);
  if ( ev )
  {
    morkCellObject* cellObj;
    cellObj = (morkCellObject*) mHandle_Object;
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  if ( outIsRowChild )
    *outIsRowChild = isRowChild;
  if ( outChildOid )
    *outChildOid = childOid;
   if ( outBlobFill )
     *outBlobFill = blobFill;
  if ( outColumn )
    *outColumn = column;
    
  return outErr;
}


/*virtual*/ mdb_err
orkinCell::GetRow(nsIMdbEnv* mev, // parent row for this cell
  nsIMdbRow** acqRow)
{
  mdb_err outErr = 0;
  nsIMdbRow* outRow = 0;
  morkCell* cell = 0;
  morkEnv* ev = this->CanUseCell(mev, /*inMutable*/ morkBool_kTrue,
    &outErr, &cell);
  if ( ev )
  {
    morkCellObject* cellObj = (morkCellObject*) mHandle_Object;
    morkRowObject* rowObj = cellObj->mCellObject_RowObject;
    outRow = rowObj->AcquireRowHandle(ev);
    
    outErr = ev->AsErr();
  }
  if ( acqRow )
    *acqRow = outRow;
  return outErr;
}

/*virtual*/ mdb_err
orkinCell::GetPort(nsIMdbEnv* mev, // port containing cell
  nsIMdbPort** acqPort)
{
  mdb_err outErr = 0;
  nsIMdbPort* outPort = 0;
  morkCell* cell = 0;
  morkEnv* ev = this->CanUseCell(mev, /*inMutable*/ morkBool_kTrue,
    &outErr, &cell);
  if ( ev )
  {
    morkCellObject* cellObj = (morkCellObject*) mHandle_Object;
    morkRow* row = cellObj->mCellObject_Row;
    if ( row )
    {
      morkStore* store = row->GetRowSpaceStore(ev);
      if ( store )
        outPort = store->AcquireStoreHandle(ev);
    }
    else
      ev->NilPointerError();

    outErr = ev->AsErr();
  }
  if ( acqPort )
    *acqPort = outPort;
  return outErr;
}
// } ----- end attribute methods -----

// { ----- begin children methods -----
/*virtual*/ mdb_err
orkinCell::HasAnyChild( // does cell have a child instead of text?
  nsIMdbEnv* mev,
  mdbOid* outOid,  // out id of row or table (or unbound if no child)
  mdb_bool* outIsRow) // nonzero if child is a row (rather than a table)
{
  mdb_err outErr = 0;
  mdb_bool isRow = morkBool_kFalse;
  outOid->mOid_Scope = 0;
  outOid->mOid_Id = morkId_kMinusOne;
  morkCell* cell = 0;
  morkEnv* ev = this->CanUseCell(mev, /*inMutable*/ morkBool_kTrue,
    &outErr, &cell);
  if ( ev )
  {
    morkCellObject* cellObj = (morkCellObject*) mHandle_Object;
    morkAtom* atom = cellObj->GetCellAtom(ev);
    if ( atom )
    {
      isRow = atom->IsRowOid();
      if ( isRow || atom->IsTableOid() )
        *outOid = ((morkOidAtom*) atom)->mOidAtom_Oid;
    }
      
    outErr = ev->AsErr();
  }
  if ( outIsRow )
    *outIsRow = isRow;
    
  return outErr;
}

/*virtual*/ mdb_err
orkinCell::GetAnyChild( // access table of specific attribute
  nsIMdbEnv* mev, // context
  nsIMdbRow** acqRow, // child row (or null)
  nsIMdbTable** acqTable) // child table (or null)
{
  mdb_err outErr = 0;
  nsIMdbRow* outRow = 0;
  nsIMdbTable* outTable = 0;
  morkCell* cell = 0;
  morkEnv* ev = this->CanUseCell(mev, /*inMutable*/ morkBool_kTrue,
    &outErr, &cell);
  if ( ev )
  {
    morkCellObject* cellObj;
    cellObj = (morkCellObject*) mHandle_Object;
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  MORK_ASSERT(acqTable);
  if ( acqTable )
    *acqTable = outTable;
  MORK_ASSERT(acqRow);
  if ( acqRow )
    *acqRow = outRow;
    
  return outErr;
}


/*virtual*/ mdb_err
orkinCell::SetChildRow( // access table of specific attribute
  nsIMdbEnv* mev, // context
  nsIMdbRow* ioRow)
{
  MORK_USED_1(ioRow);
  mdb_err outErr = 0;
  morkCell* cell = 0;
  morkEnv* ev = this->CanUseCell(mev, /*inMutable*/ morkBool_kTrue,
    &outErr, &cell);
  if ( ev )
  {
    // remember row->MaybeDirtySpaceStoreAndRow();

    morkCellObject* cellObj = (morkCellObject*) mHandle_Object;
    MORK_USED_1(cellObj);

    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
    
  return outErr;
} // inRow must be bound inside this same db port

/*virtual*/ mdb_err
orkinCell::GetChildRow( // access row of specific attribute
  nsIMdbEnv* mev, // context
  nsIMdbRow** acqRow) // acquire child row (or nil if no child)
{
  mdb_err outErr = 0;
  nsIMdbRow* outRow = 0;
  morkCell* cell = 0;
  morkEnv* ev = this->CanUseCell(mev, /*inMutable*/ morkBool_kTrue,
    &outErr, &cell);
  if ( ev )
  {
    morkCellObject* cellObj;
    cellObj = (morkCellObject*) mHandle_Object;
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  if ( acqRow )
    *acqRow = outRow;
    
  return outErr;
}


/*virtual*/ mdb_err
orkinCell::SetChildTable( // access table of specific attribute
  nsIMdbEnv* mev, // context
  nsIMdbTable* inTable) // table must be bound inside this same db port
{
  MORK_USED_1(inTable);
  mdb_err outErr = 0;
  morkCell* cell = 0;
  morkEnv* ev = this->CanUseCell(mev, /*inMutable*/ morkBool_kTrue,
    &outErr, &cell);
  if ( ev )
  {
    // remember row->MaybeDirtySpaceStoreAndRow();

    morkCellObject* cellObj = (morkCellObject*) mHandle_Object;
    MORK_USED_1(cellObj);

    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
    
  return outErr;
}

/*virtual*/ mdb_err
orkinCell::GetChildTable( // access table of specific attribute
  nsIMdbEnv* mev, // context
  nsIMdbTable** acqTable) // acquire child tabdle (or nil if no chil)
{
  mdb_err outErr = 0;
  nsIMdbTable* outTable = 0;
  morkCell* cell = 0;
  morkEnv* ev = this->CanUseCell(mev, /*inMutable*/ morkBool_kTrue,
    &outErr, &cell);
  if ( ev )
  {
    morkCellObject* cellObj;
    cellObj = (morkCellObject*) mHandle_Object;
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  if ( acqTable )
    *acqTable = outTable;
    
  return outErr;
}
// } ----- end children methods -----

// } ===== end nsIMdbCell methods =====


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

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

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _MORKCURSOR_
#include "morkCursor.h"
#endif

#ifndef _MORKPORTTABLECURSOR_
#include "morkPortTableCursor.h"
#endif

#ifndef _ORKINPORTTABLECURSOR_
#include "orkinPortTableCursor.h"
#endif

#ifndef _MORKSTORE_
#include "morkStore.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkPortTableCursor::CloseMorkNode(morkEnv* ev) // ClosePortTableCursor() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->ClosePortTableCursor(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkPortTableCursor::~morkPortTableCursor() // ClosePortTableCursor() executed earlier
{
  MORK_ASSERT(this->IsShutNode());
}

/*public non-poly*/
morkPortTableCursor::morkPortTableCursor(morkEnv* ev,
  const morkUsage& inUsage,
  nsIMdbHeap* ioHeap, morkStore* ioStore, mdb_scope inRowScope,
  mdb_kind inTableKind, nsIMdbHeap* ioSlotHeap)
: morkCursor(ev, inUsage, ioHeap)
, mPortTableCursor_Store( 0 )
, mPortTableCursor_RowScope( (mdb_scope) -1 ) // we want != inRowScope
, mPortTableCursor_TableKind( (mdb_kind) -1 ) // we want != inTableKind
, mPortTableCursor_LastTable ( 0 ) // not refcounted
, mPortTableCursor_RowSpace( 0 ) // strong ref to row space
, mPortTableCursor_TablesDidEnd( morkBool_kFalse )
, mPortTableCursor_SpacesDidEnd( morkBool_kFalse )
{
  if ( ev->Good() )
  {
    if ( ioStore )
    {
      mCursor_Pos = -1;
      mCursor_Seed = 0; // let the iterator do it's own seed handling
      morkStore::SlotWeakStore(ioStore, ev, &mPortTableCursor_Store);

      if ( this->SetRowScope(ev, inRowScope) )
        this->SetTableKind(ev, inTableKind);
        
      if ( ev->Good() )
        mNode_Derived = morkDerived_kPortTableCursor;
    }
    else
      ev->NilPointerError();
  }
}

/*public non-poly*/ void
morkPortTableCursor::ClosePortTableCursor(morkEnv* ev) 
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      mCursor_Pos = -1;
      mCursor_Seed = 0;
      mPortTableCursor_LastTable = 0;
      morkStore::SlotWeakStore((morkStore*) 0, ev, &mPortTableCursor_Store);
      morkRowSpace::SlotStrongRowSpace((morkRowSpace*) 0, ev,
        &mPortTableCursor_RowSpace);
      this->CloseCursor(ev);
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

/*static*/ void
morkPortTableCursor::NilCursorStoreError(morkEnv* ev)
{
  ev->NewError("nil mPortTableCursor_Store");
}

/*static*/ void
morkPortTableCursor::NonPortTableCursorTypeError(morkEnv* ev)
{
  ev->NewError("non morkPortTableCursor");
}

orkinPortTableCursor*
morkPortTableCursor::AcquirePortTableCursorHandle(morkEnv* ev)
{
  orkinPortTableCursor* outCursor = 0;
  orkinPortTableCursor* c = (orkinPortTableCursor*) mObject_Handle;
  if ( c ) // have an old handle?
    c->AddStrongRef(ev->AsMdbEnv());
  else // need new handle?
  {
    c = orkinPortTableCursor::MakePortTableCursor(ev, this);
    mObject_Handle = c;
  }
  if ( c )
    outCursor = c;
  return outCursor;
}

mork_bool 
morkPortTableCursor::SetRowScope(morkEnv* ev, mork_scope inRowScope)
{
  mPortTableCursor_RowScope = inRowScope;
  mPortTableCursor_LastTable = 0; // restart iteration of space
  
  mPortTableCursor_TableIter.CloseMapIter(ev);
  mPortTableCursor_TablesDidEnd = morkBool_kTrue;
  mPortTableCursor_SpacesDidEnd = morkBool_kTrue;
  
  morkStore* store = mPortTableCursor_Store;
  if ( store )
  {
    morkRowSpace* space = mPortTableCursor_RowSpace;

    if ( inRowScope ) // intend to cover a specific scope only?
    {
      space = store->LazyGetRowSpace(ev, inRowScope);
      morkRowSpace::SlotStrongRowSpace(space, ev, 
       &mPortTableCursor_RowSpace);
       
      // We want mPortTableCursor_SpacesDidEnd == morkBool_kTrue
      // to show this is the only space to be covered.
    }
    else // prepare space map iter to cover all space scopes
    {
      morkRowSpaceMapIter* rsi = &mPortTableCursor_SpaceIter;
      rsi->InitRowSpaceMapIter(ev, &store->mStore_RowSpaces);
      
      space = 0;
      (void) rsi->FirstRowSpace(ev, (mork_scope*) 0, &space);
      morkRowSpace::SlotStrongRowSpace(space, ev,
        &mPortTableCursor_RowSpace);
        
      if ( space ) // found first space in store
        mPortTableCursor_SpacesDidEnd = morkBool_kFalse;
    }

    this->init_space_tables_map(ev);
  }
  else
    this->NilCursorStoreError(ev);
    
  return ev->Good();
}

void
morkPortTableCursor::init_space_tables_map(morkEnv* ev)
{
  morkRowSpace* space = mPortTableCursor_RowSpace;
  if ( space && ev->Good() )
  {
    morkTableMapIter* ti = &mPortTableCursor_TableIter;
    ti->InitTableMapIter(ev, &space->mRowSpace_Tables);
    if ( ev->Good() )
      mPortTableCursor_TablesDidEnd = morkBool_kFalse;
  }
}


mork_bool
morkPortTableCursor::SetTableKind(morkEnv* ev, mork_kind inTableKind)
{
  mPortTableCursor_TableKind = inTableKind;
  mPortTableCursor_LastTable = 0; // restart iteration of space

  mPortTableCursor_TablesDidEnd = morkBool_kTrue;

  morkRowSpace* space = mPortTableCursor_RowSpace;
  if ( !space && mPortTableCursor_RowScope == 0 )
  {
    this->SetRowScope(ev, 0);
    space = mPortTableCursor_RowSpace;
  }
  this->init_space_tables_map(ev);
  
  return ev->Good();
}

morkRowSpace*
morkPortTableCursor::NextSpace(morkEnv* ev)
{
  morkRowSpace* outSpace = 0;
  mPortTableCursor_LastTable = 0;
  mPortTableCursor_SpacesDidEnd = morkBool_kTrue;
  mPortTableCursor_TablesDidEnd = morkBool_kTrue;

  if ( !mPortTableCursor_RowScope ) // not just one scope?
  {
    morkStore* store = mPortTableCursor_Store;
    if ( store )
    {
      morkRowSpaceMapIter* rsi = &mPortTableCursor_SpaceIter;

      (void) rsi->NextRowSpace(ev, (mork_scope*) 0, &outSpace);
      morkRowSpace::SlotStrongRowSpace(outSpace, ev,
        &mPortTableCursor_RowSpace);
        
      if ( outSpace ) // found next space in store
      {
        mPortTableCursor_SpacesDidEnd = morkBool_kFalse;
        
        this->init_space_tables_map(ev);

        if ( ev->Bad() )
          outSpace = 0;
      }
    }
    else
      this->NilCursorStoreError(ev);
  }
    
  return outSpace;
}

morkTable *
morkPortTableCursor::NextTable(morkEnv* ev)
{
  mork_kind kind = mPortTableCursor_TableKind;
  
  do // until spaces end, or until we find a table in a space
  { 
    morkRowSpace* space = mPortTableCursor_RowSpace;
    if ( mPortTableCursor_TablesDidEnd ) // current space exhausted?
      space = this->NextSpace(ev); // go on to the next space
      
    if ( space ) // have a space remaining that might hold tables?
    {
      mork_tid* key = 0; // ignore keys in table map
      morkTable* table = 0; // old value table in the map
      morkTableMapIter* ti = &mPortTableCursor_TableIter;
      mork_change* c = ( mPortTableCursor_LastTable )?
        ti->NextTable(ev, key, &table) : ti->FirstTable(ev, key, &table);

      for ( ; c && ev->Good(); c = ti->NextTable(ev, key, &table) )
      {
        if ( table && table->IsTable() )
        {
          if ( !kind || kind == table->mTable_Kind )
          {
            mPortTableCursor_LastTable = table; // ti->NextTable() hence
            return table;
          }
        }
        else
          table->NonTableTypeWarning(ev);
      }
      mPortTableCursor_TablesDidEnd = morkBool_kTrue; // space is done
      mPortTableCursor_LastTable = 0; // make sure next space starts fresh
    }
  
  } while ( ev->Good() && !mPortTableCursor_SpacesDidEnd );

  return (morkTable*) 0;
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

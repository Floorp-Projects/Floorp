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

#ifndef _MORKMAP_
#include "morkMap.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _MORKPARSER_
#include "morkParser.h"
#endif

#ifndef _MORKBUILDER_
#include "morkBuilder.h"
#endif

#ifndef _MORKCELL_
#include "morkCell.h"
#endif

#ifndef _MORKSTORE_
#include "morkStore.h"
#endif

#ifndef _MORKTABLE_
#include "morkTable.h"
#endif

#ifndef _MORKROW_
#include "morkRow.h"
#endif

#ifndef _MORKCELL_
#include "morkCell.h"
#endif

#ifndef _MORKATOM_
#include "morkAtom.h"
#endif

#ifndef _MORKATOMSPACE_
#include "morkAtomSpace.h"
#endif

#ifndef _MORKROWSPACE_
#include "morkRowSpace.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkBuilder::CloseMorkNode(morkEnv* ev) // CloseBuilder() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseBuilder(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkBuilder::~morkBuilder() // assert CloseBuilder() executed earlier
{
  MORK_ASSERT(mBuilder_Store==0);
  MORK_ASSERT(mBuilder_Row==0);
  MORK_ASSERT(mBuilder_Table==0);
  MORK_ASSERT(mBuilder_Cell==0);
  MORK_ASSERT(mBuilder_RowSpace==0);
  MORK_ASSERT(mBuilder_AtomSpace==0);
}

/*public non-poly*/
morkBuilder::morkBuilder(morkEnv* ev,
  const morkUsage& inUsage, nsIMdbHeap* ioHeap, 
  morkStream* ioStream, mdb_count inBytesPerParseSegment,
  nsIMdbHeap* ioSlotHeap, morkStore* ioStore)

: morkParser(ev, inUsage, ioHeap, ioStream,
  inBytesPerParseSegment, ioSlotHeap)
  
, mBuilder_Store( 0 )
  
, mBuilder_Table( 0 )
, mBuilder_Row( 0 )
, mBuilder_Cell( 0 )
  
, mBuilder_RowSpace( 0 )
, mBuilder_AtomSpace( 0 )
  
, mBuilder_OidAtomSpace( 0 )
, mBuilder_ScopeAtomSpace( 0 )
  
, mBuilder_PortForm( 0 )
, mBuilder_PortRowScope( (mork_scope) 'r' )
, mBuilder_PortAtomScope( (mork_scope) 'v' )

, mBuilder_TableForm( 0 )
, mBuilder_TableRowScope( (mork_scope) 'r' )
, mBuilder_TableAtomScope( (mork_scope) 'v' )
, mBuilder_TableKind( 0 )
  
, mBuilder_RowForm( 0 )
, mBuilder_RowRowScope( (mork_scope) 'r' )
, mBuilder_RowAtomScope( (mork_scope) 'v' )

, mBuilder_CellForm( 0 )
, mBuilder_CellAtomScope( (mork_scope) 'v' )

, mBuilder_DictForm( 0 )
, mBuilder_DictAtomScope( (mork_scope) 'v' )

, mBuilder_MetaTokenSlot( 0 )
  
, mBuilder_DoCutRow( morkBool_kFalse )
, mBuilder_DoCutCell( morkBool_kFalse )
, mBuilder_CellsVecFill( 0 )
{
  if ( ev->Good() )
  {
    if ( ioStore )
    {
      morkStore::SlotWeakStore(ioStore, ev, &mBuilder_Store);
      if ( ev->Good() )
        mNode_Derived = morkDerived_kBuilder;
    }
    else
      ev->NilPointerError();
  }
   
}

/*public non-poly*/ void
morkBuilder::CloseBuilder(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      mBuilder_Row = 0;
      mBuilder_Cell = 0;
      mBuilder_MetaTokenSlot = 0;
      
      morkTable::SlotStrongTable((morkTable*) 0, ev, &mBuilder_Table);
      morkStore::SlotWeakStore((morkStore*) 0, ev, &mBuilder_Store);

      morkRowSpace::SlotStrongRowSpace((morkRowSpace*) 0, ev,
        &mBuilder_RowSpace);

      morkAtomSpace::SlotStrongAtomSpace((morkAtomSpace*) 0, ev,
        &mBuilder_AtomSpace);

      morkAtomSpace::SlotStrongAtomSpace((morkAtomSpace*) 0, ev,
        &mBuilder_OidAtomSpace);

      morkAtomSpace::SlotStrongAtomSpace((morkAtomSpace*) 0, ev,
        &mBuilder_ScopeAtomSpace);
      this->CloseParser(ev);
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
morkBuilder::NonBuilderTypeError(morkEnv* ev)
{
  ev->NewError("non morkBuilder");
}

/*static*/ void
morkBuilder::NilBuilderCellError(morkEnv* ev)
{
  ev->NewError("nil mBuilder_Cell");
}

/*static*/ void
morkBuilder::NilBuilderRowError(morkEnv* ev)
{
  ev->NewError("nil mBuilder_Row");
}

/*static*/ void
morkBuilder::NilBuilderTableError(morkEnv* ev)
{
  ev->NewError("nil mBuilder_Table");
}

/*static*/ void
morkBuilder::NonColumnSpaceScopeError(morkEnv* ev)
{
  ev->NewError("column space != 'c'");
}

void
morkBuilder::LogGlitch(morkEnv* ev, const morkGlitch& inGlitch, 
  const char* inKind)
{
  ev->NewWarning("parsing glitch");
}

/*virtual*/ void
morkBuilder::MidToYarn(morkEnv* ev,
  const morkMid& inMid,  // typically an alias to concat with strings
  mdbYarn* outYarn)
// The parser might ask that some aliases be turned into yarns, so they
// can be concatenated into longer blobs under some circumstances.  This
// is an alternative to using a long and complex callback for many parts
// for a single cell value.
{
  mBuilder_Store->MidToYarn(ev, inMid, outYarn);
}

/*virtual*/ void
morkBuilder::OnNewPort(morkEnv* ev, const morkPlace& inPlace)
// mp:Start     ::= OnNewPort mp:PortItem* OnPortEnd
// mp:PortItem  ::= mp:Content | mp:Group | OnPortGlitch
// mp:Content   ::= mp:PortRow | mp:Dict | mp:Table | mp:Row
{
  // mParser_InPort = morkBool_kTrue;
  mBuilder_PortForm = 0;
  mBuilder_PortRowScope = (mork_scope) 'r';
  mBuilder_PortAtomScope = (mork_scope) 'v';
}

/*virtual*/ void
morkBuilder::OnPortGlitch(morkEnv* ev, const morkGlitch& inGlitch)  
{
  this->LogGlitch(ev, inGlitch, "port");
}

/*virtual*/ void
morkBuilder::OnPortEnd(morkEnv* ev, const morkSpan& inSpan)
// mp:Start     ::= OnNewPort mp:PortItem* OnPortEnd
{
  // ev->StubMethodOnlyError();
  // nothing to do?
  // mParser_InPort = morkBool_kFalse;
}

/*virtual*/ void
morkBuilder::OnNewGroup(morkEnv* ev, const morkPlace& inPlace, mork_gid inGid)
{
  // mParser_InGroup = morkBool_kTrue;
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnGroupGlitch(morkEnv* ev, const morkGlitch& inGlitch) 
{
  this->LogGlitch(ev, inGlitch, "group");
}

/*virtual*/ void
morkBuilder::OnGroupCommitEnd(morkEnv* ev, const morkSpan& inSpan)  
{
  // mParser_InGroup = morkBool_kFalse;
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnGroupAbortEnd(morkEnv* ev, const morkSpan& inSpan) 
{
  // mParser_InGroup = morkBool_kFalse;
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnNewPortRow(morkEnv* ev, const morkPlace& inPlace, 
  const morkMid& inMid, mork_change inChange)
{
  // mParser_InPortRow = morkBool_kTrue;
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnPortRowGlitch(morkEnv* ev, const morkGlitch& inGlitch)
{
  this->LogGlitch(ev, inGlitch, "port row");
}

/*virtual*/ void
morkBuilder::OnPortRowEnd(morkEnv* ev, const morkSpan& inSpan)
{
  // mParser_InPortRow = morkBool_kFalse;
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkBuilder::OnNewTable(morkEnv* ev, const morkPlace& inPlace,
  const morkMid& inMid, mork_change inChange)
// mp:Table     ::= OnNewTable mp:TableItem* OnTableEnd
// mp:TableItem ::= mp:Row | mp:MetaTable | OnTableGlitch
// mp:MetaTable ::= OnNewMeta mp:MetaItem* mp:Row OnMetaEnd
// mp:Meta      ::= OnNewMeta mp:MetaItem* OnMetaEnd
// mp:MetaItem  ::= mp:Cell | OnMetaGlitch
{
  // mParser_InTable = morkBool_kTrue;
  mBuilder_TableForm = mBuilder_PortForm;
  mBuilder_TableRowScope = mBuilder_PortRowScope;
  mBuilder_TableAtomScope = mBuilder_PortAtomScope;
  mBuilder_TableKind = morkStore_kNoneToken;

  morkTable* table = mBuilder_Store->MidToTable(ev, inMid);
  morkTable::SlotStrongTable(table, ev, &mBuilder_Table);
  if ( table && table->mTable_RowSpace )
    mBuilder_TableRowScope = table->mTable_RowSpace->mSpace_Scope;}

/*virtual*/ void
morkBuilder::OnTableGlitch(morkEnv* ev, const morkGlitch& inGlitch)
{
  this->LogGlitch(ev, inGlitch, "table");
}

/*virtual*/ void
morkBuilder::OnTableEnd(morkEnv* ev, const morkSpan& inSpan)
// mp:Table     ::= OnNewTable mp:TableItem* OnTableEnd
{
  // mParser_InTable = morkBool_kFalse;
  if ( mBuilder_Table )
  {
    morkTable::SlotStrongTable((morkTable*) 0, ev, &mBuilder_Table);
  }
  else
    this->NilBuilderTableError(ev);
    
  mBuilder_Row = 0;
  mBuilder_Cell = 0;

  if ( mBuilder_TableKind == morkStore_kNoneToken )
    ev->NewError("missing table kind");

  mBuilder_CellAtomScope = mBuilder_RowAtomScope =
    mBuilder_TableAtomScope = mBuilder_PortAtomScope;
}

/*virtual*/ void
morkBuilder::OnNewMeta(morkEnv* ev, const morkPlace& inPlace)
// mp:Meta      ::= OnNewMeta mp:MetaItem* OnMetaEnd
// mp:MetaItem  ::= mp:Cell | OnMetaGlitch
// mp:Cell      ::= OnNewCell mp:CellItem? OnCellEnd
// mp:CellItem  ::= mp:Slot | OnCellForm | OnCellGlitch
// mp:Slot      ::= OnValue | OnValueMid | OnRowMid | OnTableMid
{
  // mParser_InMeta = morkBool_kTrue;
  
}

/*virtual*/ void
morkBuilder::OnMetaGlitch(morkEnv* ev, const morkGlitch& inGlitch)
{
  this->LogGlitch(ev, inGlitch, "meta");
}

/*virtual*/ void
morkBuilder::OnMetaEnd(morkEnv* ev, const morkSpan& inSpan)
// mp:Meta      ::= OnNewMeta mp:MetaItem* OnMetaEnd
{
  // mParser_InMeta = morkBool_kFalse;
}

/*virtual*/ void
morkBuilder::OnNewRow(morkEnv* ev, const morkPlace& inPlace, 
  const morkMid& inMid, mork_change inChange)
// mp:Table     ::= OnNewTable mp:TableItem* OnTableEnd
// mp:TableItem ::= mp:Row | mp:MetaTable | OnTableGlitch
// mp:MetaTable ::= OnNewMeta mp:MetaItem* mp:Row OnMetaEnd
// mp:Row       ::= OnNewRow mp:RowItem* OnRowEnd
// mp:RowItem   ::= mp:Cell | mp:Meta | OnRowGlitch
// mp:Cell      ::= OnNewCell mp:CellItem? OnCellEnd
// mp:CellItem  ::= mp:Slot | OnCellForm | OnCellGlitch
// mp:Slot      ::= OnValue | OnValueMid | OnRowMid | OnTableMid
{
  // mParser_InRow = morkBool_kTrue;
  if ( mBuilder_Table )
  {
    mBuilder_CellForm = mBuilder_RowForm = mBuilder_TableForm;
    mBuilder_CellAtomScope = mBuilder_RowAtomScope = mBuilder_TableAtomScope;
    mBuilder_RowRowScope = mBuilder_TableRowScope;
    morkStore* store = mBuilder_Store;
    
    if ( !inMid.mMid_Buf && !inMid.mMid_Oid.mOid_Scope )
    {
      morkMid mid(inMid);
      mid.mMid_Oid.mOid_Scope = mBuilder_RowRowScope;
      mBuilder_Row = store->MidToRow(ev, mid);
    }
    else
    {
      mBuilder_Row = store->MidToRow(ev, inMid);
    }

    morkRow* row = mBuilder_Row;
    if ( row )
    {
      morkTable* table = mBuilder_Table;
      if ( mParser_InMeta )
      {
        if ( !table->mTable_MetaRow )
        {
          table->mTable_MetaRow = row;
          table->mTable_MetaRowOid = row->mRow_Oid;
          row->AddTableUse(ev);
        }
        else
          ev->NewError("duplicate table meta row");
      }
      else
        table->AddRow(ev, row);
    }
  }
  else
    this->NilBuilderTableError(ev);
}

/*virtual*/ void
morkBuilder::OnRowGlitch(morkEnv* ev, const morkGlitch& inGlitch) 
{
  this->LogGlitch(ev, inGlitch, "row");
}

void
morkBuilder::FlushBuilderCells(morkEnv* ev)
{
  if ( mBuilder_Row )
  {
    morkPool* pool = mBuilder_Store->StorePool();
    morkCell* cells = mBuilder_CellsVec;
    mork_fill fill = mBuilder_CellsVecFill;
    mBuilder_Row->TakeCells(ev, cells, fill, mBuilder_Store);

    morkCell* end = cells + fill;
    --cells; // prepare for preincrement
    while ( ++cells < end )
    {
      if ( cells->mCell_Atom )
        cells->SetAtom(ev, (morkAtom*) 0, pool);
    }
    mBuilder_CellsVecFill = 0;
  }
  else
    this->NilBuilderRowError(ev);
}

/*virtual*/ void
morkBuilder::OnRowEnd(morkEnv* ev, const morkSpan& inSpan) 
// mp:Row       ::= OnNewRow mp:RowItem* OnRowEnd
{
  // mParser_InRow = morkBool_kFalse;
  if ( mBuilder_Row )
  {
    this->FlushBuilderCells(ev);
  }
  else
    this->NilBuilderRowError(ev);
    
  mBuilder_Row = 0;
  mBuilder_Cell = 0;
}

/*virtual*/ void
morkBuilder::OnNewDict(morkEnv* ev, const morkPlace& inPlace)
// mp:Dict      ::= OnNewDict mp:DictItem* OnDictEnd
// mp:DictItem  ::= OnAlias | OnAliasGlitch | mp:Meta | OnDictGlitch
{
  // mParser_InDict = morkBool_kTrue;
  
  mBuilder_CellForm = mBuilder_DictForm = mBuilder_PortForm;
  mBuilder_CellAtomScope = mBuilder_DictAtomScope = mBuilder_PortAtomScope;
}

/*virtual*/ void
morkBuilder::OnDictGlitch(morkEnv* ev, const morkGlitch& inGlitch) 
{
  this->LogGlitch(ev, inGlitch, "dict");
}

/*virtual*/ void
morkBuilder::OnDictEnd(morkEnv* ev, const morkSpan& inSpan)  
// mp:Dict      ::= OnNewDict mp:DictItem* OnDictEnd
{
  // mParser_InDict = morkBool_kFalse;

  mBuilder_DictForm = 0;
  mBuilder_DictAtomScope = 0;
}

/*virtual*/ void
morkBuilder::OnAlias(morkEnv* ev, const morkSpan& inSpan,
  const morkMid& inMid)
{
  if ( mParser_InDict )
  {
    morkMid mid = inMid; // local copy for modification
    mid.mMid_Oid.mOid_Scope = mBuilder_DictAtomScope;
    mBuilder_Store->AddAlias(ev, mid, mBuilder_DictForm);
  }
  else
    ev->NewError("alias not in dict");
}

/*virtual*/ void
morkBuilder::OnAliasGlitch(morkEnv* ev, const morkGlitch& inGlitch)
{
  this->LogGlitch(ev, inGlitch, "alias");
}


morkCell* 
morkBuilder::AddBuilderCell(morkEnv* ev,
  const morkMid& inMid, mork_change inChange)
{
  morkCell* outCell = 0;
  mork_column column = inMid.mMid_Oid.mOid_Id;
  
  if ( ev->Good() )
  {
    if ( mBuilder_CellsVecFill >= morkBuilder_kCellsVecSize )
      this->FlushBuilderCells(ev);
    if ( ev->Good() )
    {
      if ( mBuilder_CellsVecFill < morkBuilder_kCellsVecSize )
      {
        mork_fill index = mBuilder_CellsVecFill++;
        outCell = mBuilder_CellsVec + index;
        outCell->SetColumnAndChange(column, inChange);
        outCell->mCell_Atom = 0;
      }
      else
        ev->NewError("out of builder cells");
    }
  }
  return outCell;
}

/*virtual*/ void
morkBuilder::OnNewCell(morkEnv* ev, const morkPlace& inPlace,
    const morkMid* inMid, const morkBuf* inBuf, mork_change inChange)
// Exactly one of inMid and inBuf is nil, and the other is non-nil.
// When hex ID syntax is used for a column, then inMid is not nil, and
// when a naked string names a column, then inBuf is not nil.
  
  // mp:Cell      ::= OnNewCell mp:CellItem? OnCellEnd
  // mp:CellItem  ::= mp:Slot | OnCellForm | OnCellGlitch
  // mp:Slot      ::= OnValue | OnValueMid | OnRowMid | OnTableMid
{
  // mParser_InCell = morkBool_kTrue;
  
  mBuilder_CellAtomScope = mBuilder_RowAtomScope;
  
  mBuilder_Cell = 0; // nil until determined for a row
  morkStore* store = mBuilder_Store;
  mork_scope scope = morkStore_kColumnSpaceScope;
  morkMid tempMid; // space for local and modifiable cell mid
  morkMid* cellMid = &tempMid; // default to local if inMid==0
  
  if ( inMid ) // mid parameter is actually provided?
  {
    *cellMid = *inMid; // bitwise copy for modifiable local mid

    if ( !cellMid->mMid_Oid.mOid_Scope ) 
    {
      if ( cellMid->mMid_Buf )
      {
        scope = store->BufToToken(ev, cellMid->mMid_Buf);
        cellMid->mMid_Buf = 0; // don't do scope lookup again
        ev->NewWarning("column mids need column scope");
      }
      cellMid->mMid_Oid.mOid_Scope = scope;
    }
  }
  else if ( inBuf ) // buf points to naked column string name?
  {
    cellMid->ClearMid();
    cellMid->mMid_Oid.mOid_Id = store->BufToToken(ev, inBuf);
    cellMid->mMid_Oid.mOid_Scope = scope; // kColumnSpaceScope
  }
  else
    ev->NilPointerError(); // either inMid or inBuf must be non-nil

  mork_column column = cellMid->mMid_Oid.mOid_Id;
  
  if ( mBuilder_Row && ev->Good() ) // this cell must be inside a row
  {
      // mBuilder_Cell = this->AddBuilderCell(ev, *cellMid, inChange);

      if ( mBuilder_CellsVecFill >= morkBuilder_kCellsVecSize )
        this->FlushBuilderCells(ev);
      if ( ev->Good() )
      {
        if ( mBuilder_CellsVecFill < morkBuilder_kCellsVecSize )
        {
          mork_fill index = mBuilder_CellsVecFill++;
          morkCell* cell =  mBuilder_CellsVec + index;
          cell->SetColumnAndChange(column, inChange);
          cell->mCell_Atom = 0;
          mBuilder_Cell = cell;
        }
        else
          ev->NewError("out of builder cells");
      }
  }

  else if ( mParser_InMeta &&  ev->Good() ) // cell is in metainfo structure?
  {
    if ( scope == morkStore_kColumnSpaceScope )
    {
      if ( mParser_InTable ) // metainfo for table?
      {
        if ( column == morkStore_kKindColumn )
          mBuilder_MetaTokenSlot = &mBuilder_TableKind;
        else if ( column == morkStore_kRowScopeColumn )
          mBuilder_MetaTokenSlot = &mBuilder_TableRowScope;
        else if ( column == morkStore_kAtomScopeColumn )
          mBuilder_MetaTokenSlot = &mBuilder_TableAtomScope;
        else if ( column == morkStore_kFormColumn )
          mBuilder_MetaTokenSlot = &mBuilder_TableForm;
      }
      else if ( mParser_InDict ) // metainfo for dict?
      {
        if ( column == morkStore_kAtomScopeColumn )
          mBuilder_MetaTokenSlot = &mBuilder_DictAtomScope;
        else if ( column == morkStore_kFormColumn )
          mBuilder_MetaTokenSlot = &mBuilder_DictForm;
      }
      else if ( mParser_InRow ) // metainfo for row?
      {
        if ( column == morkStore_kAtomScopeColumn )
          mBuilder_MetaTokenSlot = &mBuilder_RowAtomScope;
        else if ( column == morkStore_kRowScopeColumn )
          mBuilder_MetaTokenSlot = &mBuilder_RowRowScope;
        else if ( column == morkStore_kFormColumn )
          mBuilder_MetaTokenSlot = &mBuilder_RowForm;
      }
    }
    else
      ev->NewWarning("expected column scope");
  }
}

/*virtual*/ void
morkBuilder::OnCellGlitch(morkEnv* ev, const morkGlitch& inGlitch)
{
  this->LogGlitch(ev, inGlitch, "cell");
}

/*virtual*/ void
morkBuilder::OnCellForm(morkEnv* ev, mork_cscode inCharsetFormat)
{
  morkCell* cell = mBuilder_Cell;
  if ( cell )
  {
    mBuilder_CellForm = inCharsetFormat;
  }
  else
    this->NilBuilderCellError(ev);
}

/*virtual*/ void
morkBuilder::OnCellEnd(morkEnv* ev, const morkSpan& inSpan)
// mp:Cell      ::= OnNewCell mp:CellItem? OnCellEnd
{
  // mParser_InCell = morkBool_kFalse;
  
  mBuilder_MetaTokenSlot = 0;
  mBuilder_CellAtomScope = mBuilder_RowAtomScope;
}

/*virtual*/ void
morkBuilder::OnValue(morkEnv* ev, const morkSpan& inSpan,
  const morkBuf& inBuf)
// mp:CellItem  ::= mp:Slot | OnCellForm | OnCellGlitch
// mp:Slot      ::= OnValue | OnValueMid | OnRowMid | OnTableMid
{
  morkStore* store = mBuilder_Store;
  morkCell* cell = mBuilder_Cell;
  if ( cell )
  {
    mdbYarn yarn;
    yarn.mYarn_Buf = inBuf.mBuf_Body;
    yarn.mYarn_Fill = yarn.mYarn_Size = inBuf.mBuf_Fill;
    yarn.mYarn_More = 0;
    yarn.mYarn_Form = mBuilder_CellForm;
    yarn.mYarn_Grow = 0;
    morkAtom* atom = store->YarnToAtom(ev, &yarn);
    cell->SetAtom(ev, atom, store->StorePool());
  }
  else if ( mParser_InMeta )
  {
    mork_token* metaSlot = mBuilder_MetaTokenSlot;
    if ( metaSlot )
    {
      mork_token token = store->BufToToken(ev, &inBuf);
      if ( token )
      {
        *metaSlot = token;
        if ( metaSlot == &mBuilder_TableKind ) // table kind?
        {
          if ( mParser_InTable && mBuilder_Table )
            mBuilder_Table->mTable_Kind = token;
        }
      }
    }
  }
  else
    this->NilBuilderCellError(ev);
}

/*virtual*/ void
morkBuilder::OnValueMid(morkEnv* ev, const morkSpan& inSpan,
  const morkMid& inMid)
// mp:CellItem  ::= mp:Slot | OnCellForm | OnCellGlitch
// mp:Slot      ::= OnValue | OnValueMid | OnRowMid | OnTableMid
{
  morkStore* store = mBuilder_Store;
  morkCell* cell = mBuilder_Cell;

  morkMid valMid; // local mid for modifications
  mdbOid* valOid = &valMid.mMid_Oid; // ref to oid inside mid
  *valOid = inMid.mMid_Oid; // bitwise copy inMid's oid
  
  if ( inMid.mMid_Buf )
  {
    if ( !valOid->mOid_Scope )
      store->MidToOid(ev, inMid, valOid);
  }
  else if ( !valOid->mOid_Scope )
    valOid->mOid_Scope = mBuilder_CellAtomScope;
  
  if ( cell )
  {
    morkBookAtom* atom = store->MidToAtom(ev, valMid);
    if ( atom )
      cell->SetAtom(ev, atom, store->StorePool());
    else
      ev->NewError("undefined cell value alias");
  }
  else if ( mParser_InMeta )
  {
    mork_token* metaSlot = mBuilder_MetaTokenSlot;
    if ( metaSlot )
    {
      if ( valOid->mOid_Scope == morkStore_kColumnSpaceScope )
      {
        if ( ev->Good() && valMid.HasSomeId() )
        {
          *metaSlot = valOid->mOid_Id;
          if ( metaSlot == &mBuilder_TableKind ) // table kind?
          {
            if ( mParser_InTable && mBuilder_Table )
            {
              mBuilder_Table->mTable_Kind = valOid->mOid_Id;
            }
            else
              ev->NewWarning("mBuilder_TableKind not in table");
          }
        }
      }
      else
        this->NonColumnSpaceScopeError(ev);
    }
  }
  else
    this->NilBuilderCellError(ev);
}

/*virtual*/ void
morkBuilder::OnRowMid(morkEnv* ev, const morkSpan& inSpan,
  const morkMid& inMid)
// mp:CellItem  ::= mp:Slot | OnCellForm | OnCellGlitch
// mp:Slot      ::= OnValue | OnValueMid | OnRowMid | OnTableMid
{
  morkStore* store = mBuilder_Store;
  morkCell* cell = mBuilder_Cell;
  if ( cell )
  {
    mdbOid rowOid = inMid.mMid_Oid;
    if ( inMid.mMid_Buf )
    {
      if ( !rowOid.mOid_Scope )
        store->MidToOid(ev, inMid, &rowOid);
    }
    else if ( !rowOid.mOid_Scope )
      rowOid.mOid_Scope = mBuilder_RowRowScope;
    
    if ( ev->Good() )
     {
       morkPool* pool = store->StorePool();
       morkAtom* atom = pool->NewRowOidAtom(ev, rowOid);
       if ( atom )
       {
         cell->SetAtom(ev, atom, pool);
         morkRow* row = store->OidToRow(ev, &rowOid);
         if ( row ) // found or created such a row?
           row->AddTableUse(ev);
       }
     }
  }
  else
    this->NilBuilderCellError(ev);
}

/*virtual*/ void
morkBuilder::OnTableMid(morkEnv* ev, const morkSpan& inSpan,
  const morkMid& inMid)
// mp:CellItem  ::= mp:Slot | OnCellForm | OnCellGlitch
// mp:Slot      ::= OnValue | OnValueMid | OnRowMid | OnTableMid
{
  morkStore* store = mBuilder_Store;
  morkCell* cell = mBuilder_Cell;
  if ( cell )
  {
    mdbOid tableOid = inMid.mMid_Oid;
    if ( inMid.mMid_Buf )
    {
      if ( !tableOid.mOid_Scope )
        store->MidToOid(ev, inMid, &tableOid);
    }
    else if ( !tableOid.mOid_Scope )
      tableOid.mOid_Scope = mBuilder_RowRowScope;
    
    if ( ev->Good() )
     {
       morkPool* pool = store->StorePool();
       morkAtom* atom = pool->NewTableOidAtom(ev, tableOid);
       if ( atom )
       {
         cell->SetAtom(ev, atom, pool);
         morkTable* table = store->OidToTable(ev, &tableOid,
           /*optionalMetaRowOid*/ (mdbOid*) 0);
         if ( table ) // found or created such a table?
           table->AddCellUse(ev);
       }
     }
  }
  else
    this->NilBuilderCellError(ev);
}


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

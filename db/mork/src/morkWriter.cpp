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

#ifndef _MORKBLOB_
#include "morkBlob.h"
#endif

#ifndef _MORKNODE_
#include "morkNode.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _MORKARRAY_
#include "morkWriter.h"
#endif

#ifndef _MORKFILE_
#include "morkFile.h"
#endif

#ifndef _MORKSTREAM_
#include "morkStream.h"
#endif

#ifndef _MORKSTORE_
#include "morkStore.h"
#endif

#ifndef _MORKATOMSPACE_
#include "morkAtomSpace.h"
#endif

#ifndef _MORKROWSPACE_
#include "morkRowSpace.h"
#endif

#ifndef _MORKROWMAP_
#include "morkRowMap.h"
#endif

#ifndef _MORKATOMMAP_
#include "morkAtomMap.h"
#endif

#ifndef _MORKROW_
#include "morkRow.h"
#endif

#ifndef _MORKTABLE_
#include "morkTable.h"
#endif

#ifndef _MORKCELL_
#include "morkCell.h"
#endif

#ifndef _MORKATOM_
#include "morkAtom.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkWriter::CloseMorkNode(morkEnv* ev) // CloseTable() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseWriter(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkWriter::~morkWriter() // assert CloseTable() executed earlier
{
  MORK_ASSERT(this->IsShutNode());
  MORK_ASSERT(mWriter_Store==0);
}

/*public non-poly*/
morkWriter::morkWriter(morkEnv* ev, const morkUsage& inUsage,
    nsIMdbHeap* ioHeap, morkStore* ioStore, morkFile* ioFile,
    nsIMdbHeap* ioSlotHeap)
: morkNode(ev, inUsage, ioHeap)
, mWriter_Store( 0 )
, mWriter_File( 0 )
, mWriter_Bud( 0 )
, mWriter_Stream( 0 )
, mWriter_SlotHeap( 0 )

, mWriter_TotalCount( morkWriter_kCountNumberOfPhases )
, mWriter_DoneCount( 0 )

, mWriter_LineSize( 0 )
, mWriter_MaxIndent( morkWriter_kMaxIndent )
  
, mWriter_TableCharset( 0 )
, mWriter_TableAtomScope( 0 )
, mWriter_TableRowScope( 0 )
, mWriter_TableTableScope( 0 )
, mWriter_TableColumnScope( 0 )
, mWriter_TableKind( 0 )
  
, mWriter_RowCharset( 0 )
, mWriter_RowAtomScope( 0 )
, mWriter_RowScope( 0 )
  
, mWriter_DictCharset( 0 )
, mWriter_DictAtomScope( 0 )

, mWriter_NeedDirtyAll( morkBool_kFalse )
, mWriter_Phase( morkWriter_kPhaseNothingDone )
, mWriter_DidStartDict( morkBool_kFalse )
, mWriter_DidEndDict( morkBool_kTrue )

, mWriter_TableRowArrayPos( 0 )

// empty constructors for map iterators:
, mWriter_StoreAtomSpacesIter( )
, mWriter_AtomSpaceAtomAidsIter( )
  
, mWriter_StoreRowSpacesIter( )
, mWriter_RowSpaceTablesIter( )
, mWriter_RowSpaceRowsIter( )
{
  mWriter_SafeNameBuf[ 0 ] = 0;
  mWriter_SafeNameBuf[ morkWriter_kMaxColumnNameSize * 2 ] = 0;
  mWriter_ColNameBuf[ 0 ] = 0;
  mWriter_ColNameBuf[ morkWriter_kMaxColumnNameSize ] = 0;
  
  mdbYarn* y = &mWriter_ColYarn;
  y->mYarn_Buf = mWriter_ColNameBuf; // where to put col bytes
  y->mYarn_Fill = 0; // set later by writer
  y->mYarn_Size = morkWriter_kMaxColumnNameSize; // our buf size
  y->mYarn_More = 0; // set later by writer
  y->mYarn_Form = 0; // set later by writer
  y->mYarn_Grow = 0; // do not allow buffer growth
  
  y = &mWriter_SafeYarn;
  y->mYarn_Buf = mWriter_SafeNameBuf; // where to put col bytes
  y->mYarn_Fill = 0; // set later by writer
  y->mYarn_Size = morkWriter_kMaxColumnNameSize * 2; // our buf size
  y->mYarn_More = 0; // set later by writer
  y->mYarn_Form = 0; // set later by writer
  y->mYarn_Grow = 0; // do not allow buffer growth
  
  if ( ev->Good() )
  {
    if ( ioSlotHeap && ioFile && ioStore )
    {
      morkStore::SlotWeakStore(ioStore, ev, &mWriter_Store);
      morkFile::SlotStrongFile(ioFile, ev, &mWriter_File);
      nsIMdbHeap_SlotStrongHeap(ioSlotHeap, ev, &mWriter_SlotHeap);
      if ( ev->Good() )
      {
        morkFile* bud = ioFile->AcquireBud(ev, ioSlotHeap);
        if ( bud )
        {
          if ( ev->Good() )
          {
            mWriter_Bud = bud;
            mork_bool frozen = morkBool_kFalse; // need to modify
            morkStream* stream = new(*ioSlotHeap, ev)
              morkStream(ev, morkUsage::kHeap, ioSlotHeap, bud,
                morkWriter_kStreamBufSize, frozen);
              
            if ( stream )
            {
              if ( ev->Good() )
              {
                mWriter_Stream = stream;
                if ( ev->Good() )
                  mNode_Derived = morkDerived_kWriter;
                  
              }
              else
                stream->CutStrongRef(ev);
            }
          }
          else
            bud->CutStrongRef(ev);
        }
      }
    }
    else
      ev->NilPointerError();
  }
}

/*public non-poly*/ void
morkWriter::CloseWriter(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      morkStore::SlotWeakStore((morkStore*) 0, ev, &mWriter_Store);
      morkFile::SlotStrongFile((morkFile*) 0, ev, &mWriter_File);
      morkFile::SlotStrongFile((morkFile*) 0, ev, &mWriter_Bud);
      morkStream::SlotStrongStream((morkStream*) 0, ev, &mWriter_Stream);
      nsIMdbHeap_SlotStrongHeap((nsIMdbHeap*) 0, ev, &mWriter_SlotHeap);
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
morkWriter::NonWriterTypeError(morkEnv* ev)
{
  ev->NewError("non morkWriter");
}

/*static*/ void
morkWriter::NilWriterStoreError(morkEnv* ev)
{
  ev->NewError("nil mWriter_Store");
}

/*static*/ void
morkWriter::NilWriterBudError(morkEnv* ev)
{
  ev->NewError("nil mWriter_Bud");
}

/*static*/ void
morkWriter::NilWriterStreamError(morkEnv* ev)
{
  ev->NewError("nil mWriter_Stream");
}

/*static*/ void
morkWriter::UnsupportedPhaseError(morkEnv* ev)
{
  ev->NewError("unsupported mWriter_Phase");
}

mork_bool
morkWriter::WriteMore(morkEnv* ev) // call until IsWritingDone() is true
{
  if ( this->IsOpenNode() )
  {
    if ( this->IsWriter() )
    {
      if ( mWriter_Stream )
      {
        if ( ev->Bad() )
        {
          ev->NewWarning("writing stops on error");
          mWriter_Phase = morkWriter_kPhaseWritingDone;
        }
        switch( mWriter_Phase )
        {
          case morkWriter_kPhaseNothingDone:
            OnNothingDone(ev); break;
          
          case morkWriter_kPhaseDirtyAllDone:
            OnDirtyAllDone(ev); break;
          
          case morkWriter_kPhasePutHeaderDone:
            OnPutHeaderDone(ev); break;
          
          case morkWriter_kPhaseRenumberAllDone:
            OnRenumberAllDone(ev); break;
          
          case morkWriter_kPhaseStoreAtomSpaces:
            OnStoreAtomSpaces(ev); break;
          
          case morkWriter_kPhaseAtomSpaceAtomAids:
            OnAtomSpaceAtomAids(ev); break;
          
          case morkWriter_kPhaseStoreRowSpacesTables:
            OnStoreRowSpacesTables(ev); break;
          
          case morkWriter_kPhaseRowSpaceTables:
            OnRowSpaceTables(ev); break;
          
          case morkWriter_kPhaseTableRowArray:
            OnTableRowArray(ev); break;
          
          case morkWriter_kPhaseStoreRowSpacesRows:
            OnStoreRowSpacesRows(ev); break;
          
          case morkWriter_kPhaseRowSpaceRows:
            OnRowSpaceRows(ev); break;
          
          case morkWriter_kPhaseContentDone:
            OnContentDone(ev); break;
          
          case morkWriter_kPhaseWritingDone:
            OnWritingDone(ev); break;
          
          default:
            this->UnsupportedPhaseError(ev);
        }
      }
      else
        this->NilWriterStreamError(ev);
    }
    else
      this->NonWriterTypeError(ev);
  }
  else
    this->NonOpenNodeError(ev);
    
  return ev->Good();
}

static const char* morkWriter_kHexDigits = "0123456789ABCDEF";

mork_size
morkWriter::WriteYarn(morkEnv* ev, const mdbYarn* inYarn)
  // return number of atom bytes written on the current line (which
  // implies that escaped line breaks will make the size value smaller
  // than the entire yarn's size, since only part goes on a last line).
{
  mork_size outSize = 0;
  morkStream* stream = mWriter_Stream;

  const mork_u1* b = (const mork_u1*) inYarn->mYarn_Buf;
  if ( b )
  {
    register int c;
    mork_fill fill = inYarn->mYarn_Fill;
    const mork_u1* end = b + fill;
    while ( b < end )
    {
      c = *b++; // next byte to print
      if ( c < 0x080 && MORK_ISPRINT(c) )
      {
        if ( c == ')' && c == '$' && c == '\\' )
        {
          stream->Putc(ev, '\\');
          ++outSize;
        }
        stream->Putc(ev, c);
        ++outSize;
      }
      else
      {
        outSize += 3; // '$' hex hex
        stream->Putc(ev, '$');
        stream->Putc(ev, morkWriter_kHexDigits[ (c >> 4) & 0x0F ]);
        stream->Putc(ev, morkWriter_kHexDigits[ c & 0x0F ]);
      }
    }
  }
  mWriter_LineSize += outSize;
    
  return outSize;
}

mork_size
morkWriter::WriteAtom(morkEnv* ev, const morkAtom* inAtom)
  // return number of atom bytes written on the current line (which
  // implies that escaped line breaks will make the size value smaller
  // than the entire atom's size, since only part goes on a last line).
{
  mork_size outSize = 0;
  mdbYarn yarn; // to ref content inside atom

  if ( inAtom->AliasYarn(&yarn) )
    outSize = this->WriteYarn(ev, &yarn);
    // mWriter_LineSize += stream->Write(ev, inYarn->mYarn_Buf, outSize);
  else
    inAtom->BadAtomKindError(ev);
    
  return outSize;
}

void
morkWriter::WriteAtomSpaceAsDict(morkEnv* ev, morkAtomSpace* ioSpace)
{
  morkStream* stream = mWriter_Stream;
  mork_scope scope = ioSpace->mSpace_Scope;
  if ( scope < 0x80 )
  {
    if ( mWriter_LineSize )
      stream->PutLineBreak(ev);
    stream->PutString(ev, "< <(atomScope=");
    stream->Putc(ev, (int) scope);
    ++mWriter_LineSize;
    stream->PutString(ev, ")> // (charset=iso-8859-1)");
    mWriter_LineSize = stream->PutIndent(ev, morkWriter_kDictAliasDepth);
  }
  else
    ioSpace->NonAsciiSpaceScopeName(ev);

  if ( ev->Good() )
  {
    char buf[ 64 ]; // buffer for staging the dict alias hex ID
    char* idBuf = buf + 1; // where the id always starts
    buf[ 0 ] = '('; // we always start with open paren
    morkBookAtom* atom = 0;
    morkAtomAidMapIter* ai = &mWriter_AtomSpaceAtomAidsIter;
    ai->InitAtomAidMapIter(ev, &ioSpace->mAtomSpace_AtomAids);
    mork_change* c = 0;
    
    for ( c = ai->FirstAtom(ev, &atom); c && ev->Good();
          c = ai->NextAtom(ev, &atom) )
    {
      if ( atom )
      {
        // temporarily check for expected change from earlier DirtyAll():
        if ( atom->mAtom_Change == morkChange_kAdd )
        {
          atom->mAtom_Change = morkChange_kNil; // neutralize change
          
          this->IndentAsNeeded(ev, morkWriter_kDictAliasDepth);
          mork_size size = ev->TokenAsHex(idBuf, atom->mBookAtom_Id);
          mWriter_LineSize += stream->Write(ev, buf, size+1); //  '('
          
          this->IndentAsNeeded(ev, morkWriter_kDictAliasValueDepth);
          stream->Putc(ev, '='); // end alias
          ++mWriter_LineSize;
          
          this->WriteAtom(ev, atom);
          stream->Putc(ev, ')'); // end alias
          ++mWriter_LineSize;
          
          ++mWriter_DoneCount;
        }
        else // temporarily warn about wrong change slot value
          ev->NewWarning("mAtom_Change != morkChange_kAdd");
      }
      else
        ev->NilPointerError();
    }
    ai->CloseMapIter(ev);
  }
  
  if ( ev->Good() )
  {
    this->IndentAsNeeded(ev, 0);
    stream->PutByteThenNewline(ev, '>'); // end dict
  }
}
 
mork_bool
morkWriter::DirtyAll(morkEnv* ev)
  // DirtyAll() visits every store sub-object and marks 
  // them dirty, including every table, row, cell, and atom.  The return
  // equals ev->Good(), to show whether any error happened.  This method is
  // intended for use in the beginning of a "compress commit" which writes
  // all store content, whether dirty or not.  We dirty everything first so
  // that later iterations over content can mark things clean as they are
  // written, and organize the process of serialization so that objects are
  // written only at need (because of being dirty).  Note the method can 
  // stop early when any error happens, since this will abort any commit.
{
  morkStore* store = mWriter_Store;
  if ( store )
  {
    store->SetDirty();
    mork_change* c = 0;

    if ( ev->Good() )
    {
      morkAtomSpaceMapIter* asi = &mWriter_StoreAtomSpacesIter;
      asi->InitAtomSpaceMapIter(ev, &store->mStore_AtomSpaces);

      mork_scope* key = 0; // ignore keys in map
      morkAtomSpace* space = 0; // old val node in the map
      
      for ( c = asi->FirstAtomSpace(ev, key, &space); c && ev->Good();
            c = asi->NextAtomSpace(ev, key, &space) )
      {
        if ( space )
        {
          if ( space->IsAtomSpace() )
          {
            morkBookAtom* atom = 0;
            morkAtomAidMapIter* ai = &mWriter_AtomSpaceAtomAidsIter;
            ai->InitAtomAidMapIter(ev, &space->mAtomSpace_AtomAids);
            
            for ( c = ai->FirstAtom(ev, &atom); c && ev->Good();
                  c = ai->NextAtom(ev, &atom) )
            {
              if ( atom )
              {
                atom->SetAtomDirty();
                ++mWriter_TotalCount;
              }
              else
                ev->NilPointerError();
            }
            
            ai->CloseMapIter(ev);
          }
          else
            space->NonAtomSpaceTypeError(ev);
        }
        else
          ev->NilPointerError();
      }
    }
    
    if ( ev->Good() )
    {
      morkRowSpaceMapIter* rsi = &mWriter_StoreRowSpacesIter;
      rsi->InitRowSpaceMapIter(ev, &store->mStore_RowSpaces);

      mork_scope* key = 0; // ignore keys in map
      morkRowSpace* space = 0; // old val node in the map
      
      for ( c = rsi->FirstRowSpace(ev, key, &space); c && ev->Good();
            c = rsi->NextRowSpace(ev, key, &space) )
      {
        if ( space )
        {
          if ( space->IsRowSpace() )
          {
            space->SetDirty();
            if ( ev->Good() )
            {
              morkRowMapIter* ri = &mWriter_RowSpaceRowsIter;
              ri->InitRowMapIter(ev, &space->mRowSpace_Rows);

              morkRow* row = 0; // old key row in the map
                
              for ( c = ri->FirstRow(ev, &row); c && ev->Good();
                    c = ri->NextRow(ev, &row) )
              {
                if ( row && row->IsRow() )
                {
                  row->DirtyAllRowContent(ev);
                  ++mWriter_TotalCount;
                }
                else
                  row->NonRowTypeWarning(ev);
              }
              ri->CloseMapIter(ev);
            }
            
            if ( ev->Good() )
            {
              morkTableMapIter* ti = &mWriter_RowSpaceTablesIter;
              ti->InitTableMapIter(ev, &space->mRowSpace_Tables);

              mork_tid* key = 0; // ignore keys in table map
              morkTable* table = 0; // old key row in the map
                
              for ( c = ti->FirstTable(ev, key, &table); c && ev->Good();
                    c = ti->NextTable(ev, key, &table) )
              {
                if ( table && table->IsTable() )
                {
                  // table->DirtyAllTableContent(ev);
                  // only necessary to mark table itself dirty:
                  table->SetDirty();
                  ++mWriter_TotalCount;
                }
                else
                  table->NonTableTypeWarning(ev);
              }
              ti->CloseMapIter(ev);
            }
          }
          else
            space->NonRowSpaceTypeError(ev);
        }
        else
          ev->NilPointerError();
      }
    }
  }
  else
    this->NilWriterStoreError(ev);
  
  return ev->Good();
}

mork_bool
morkWriter::OnNothingDone(morkEnv* ev)
{
  // morkStream* stream = mWriter_Stream;
  if ( mWriter_NeedDirtyAll )
    this->DirtyAll(ev);
    
  if ( ev->Good() )
    mWriter_Phase = morkWriter_kPhaseDirtyAllDone;
  else
    mWriter_Phase = morkWriter_kPhaseWritingDone; // stop on error
    
  return ev->Good();
}

mork_bool
morkWriter::OnDirtyAllDone(morkEnv* ev)
{
  morkStream* stream = mWriter_Stream;
  stream->Seek(ev, 0); // beginning of stream
  if ( ev->Good() )
  {
    stream->PutStringThenNewline(ev, "// <!-- <mdb:mork:z v=\"1.1\"/> -->");
    mWriter_LineSize = 0;
  }
    
  if ( ev->Good() )
    mWriter_Phase = morkWriter_kPhasePutHeaderDone;
  else
    mWriter_Phase = morkWriter_kPhaseWritingDone; // stop on error
    
  return ev->Good();
}

mork_bool
morkWriter::OnPutHeaderDone(morkEnv* ev)
{
  morkStream* stream = mWriter_Stream;
  if ( mWriter_LineSize )
    stream->PutLineBreak(ev);
  stream->PutStringThenNewline(ev, "// OnPutHeaderDone()");
  mWriter_LineSize = 0;
  
  morkStore* store = mWriter_Store;
  if ( store )
    store->RenumberAllCollectableContent(ev);
  else
    this->NilWriterStoreError(ev);
    
  if ( ev->Good() )
    mWriter_Phase = morkWriter_kPhaseRenumberAllDone;
  else
    mWriter_Phase = morkWriter_kPhaseWritingDone; // stop on error
    
  return ev->Good();
}

mork_bool
morkWriter::OnRenumberAllDone(morkEnv* ev)
{
  morkStream* stream = mWriter_Stream;
  if ( mWriter_LineSize )
    stream->PutLineBreak(ev);
  stream->PutStringThenNewline(ev, "// OnRenumberAllDone()");
  mWriter_LineSize = 0;
    
  if ( ev->Good() )
    mWriter_Phase = morkWriter_kPhaseStoreAtomSpaces;
  else
    mWriter_Phase = morkWriter_kPhaseWritingDone; // stop on error

  return ev->Good();
}

mork_bool
morkWriter::OnStoreAtomSpaces(morkEnv* ev)
{
  morkStream* stream = mWriter_Stream;
  if ( mWriter_LineSize )
    stream->PutLineBreak(ev);
  stream->PutStringThenNewline(ev, "// OnStoreAtomSpaces()");
  mWriter_LineSize = 0;
  
  if ( ev->Good() )
  {
    morkStore* store = mWriter_Store;
    if ( store )
    {
      morkAtomSpace* space = store->LazyGetGroundColumnSpace(ev);
      if ( space )
      {
        stream->PutStringThenNewline(ev, "// ground column space dict:");
        mWriter_LineSize = 0;
        this->WriteAtomSpaceAsDict(ev, space);
      }
    }
    else
      this->NilWriterStoreError(ev);
  }
    
  if ( ev->Good() )
    mWriter_Phase = morkWriter_kPhaseStoreRowSpacesTables;
  else
    mWriter_Phase = morkWriter_kPhaseWritingDone; // stop on error

  return ev->Good();
}

mork_bool
morkWriter::OnAtomSpaceAtomAids(morkEnv* ev)
{
  morkStream* stream = mWriter_Stream;
  if ( mWriter_LineSize )
    stream->PutLineBreak(ev);
  stream->PutStringThenNewline(ev, "// OnAtomSpaceAtomAids()");
  mWriter_LineSize = 0;
    
  if ( ev->Good() )
    mWriter_Phase = morkWriter_kPhaseStoreRowSpacesTables;
  else
    mWriter_Phase = morkWriter_kPhaseWritingDone; // stop on error

  return ev->Good();
}

void
morkWriter::WriteAllStoreTables(morkEnv* ev)
{
  morkStore* store = mWriter_Store;
  if ( store && ev->Good() )
  {
    morkRowSpaceMapIter* rsi = &mWriter_StoreRowSpacesIter;
    rsi->InitRowSpaceMapIter(ev, &store->mStore_RowSpaces);

    mork_scope* key = 0; // ignore keys in map
    morkRowSpace* space = 0; // old val node in the map
    mork_change* c = 0;
    
    for ( c = rsi->FirstRowSpace(ev, key, &space); c && ev->Good();
          c = rsi->NextRowSpace(ev, key, &space) )
    {
      if ( space )
      {
        if ( space->IsRowSpace() )
        {
          if ( ev->Good() )
          {
            morkTableMapIter* ti = &mWriter_RowSpaceTablesIter;
            ti->InitTableMapIter(ev, &space->mRowSpace_Tables);

            mork_tid* key = 0; // ignore keys in table map
            morkTable* table = 0; // old key row in the map
              
            for ( c = ti->FirstTable(ev, key, &table); c && ev->Good();
                  c = ti->NextTable(ev, key, &table) )
            {
              if ( table && table->IsTable() )
              {
                // temporarily check for dirty from earlier DirtyAll():
                if ( table->IsDirty() )
                {
                  table->SetClean();
                  
                  if ( this->PutTableDict(ev, table) )
                    this->PutTable(ev, table);
                }
                else // temporarily warn about wrong dirty status:
                  ev->NewWarning("table->IsClean()");
              }
              else
                table->NonTableTypeWarning(ev);
            }
            ti->CloseMapIter(ev);
          }
        }
        else
          space->NonRowSpaceTypeError(ev);
      }
      else
        ev->NilPointerError();
    }
  }
}

mork_bool
morkWriter::OnStoreRowSpacesTables(morkEnv* ev)
{
  morkStream* stream = mWriter_Stream;
  if ( mWriter_LineSize )
    stream->PutLineBreak(ev);
  stream->PutStringThenNewline(ev, "// OnStoreRowSpacesTables()");
  mWriter_LineSize = 0;
  
  // later we'll break this up, but today we'll write all in one shot:
  this->WriteAllStoreTables(ev);
    
  if ( ev->Good() )
    mWriter_Phase = morkWriter_kPhaseStoreRowSpacesRows;
  else
    mWriter_Phase = morkWriter_kPhaseWritingDone; // stop on error

  return ev->Good();
}

mork_bool
morkWriter::OnRowSpaceTables(morkEnv* ev)
{
  morkStream* stream = mWriter_Stream;
  if ( mWriter_LineSize )
    stream->PutLineBreak(ev);
  stream->PutStringThenNewline(ev, "// OnRowSpaceTables()");
  mWriter_LineSize = 0;
    
  if ( ev->Good() )
    mWriter_Phase = morkWriter_kPhaseStoreRowSpacesRows;
  else
    mWriter_Phase = morkWriter_kPhaseWritingDone; // stop on error

  return ev->Good();
}

mork_bool
morkWriter::OnTableRowArray(morkEnv* ev)
{
  morkStream* stream = mWriter_Stream;
  if ( mWriter_LineSize )
    stream->PutLineBreak(ev);
  stream->PutStringThenNewline(ev, "// OnTableRowArray()");
  mWriter_LineSize = 0;
    
  if ( ev->Good() )
    mWriter_Phase = morkWriter_kPhaseStoreRowSpacesRows;
  else
    mWriter_Phase = morkWriter_kPhaseWritingDone; // stop on error

  return ev->Good();
}

mork_bool
morkWriter::OnStoreRowSpacesRows(morkEnv* ev)
{
  morkStream* stream = mWriter_Stream;
  if ( mWriter_LineSize )
    stream->PutLineBreak(ev);
  stream->PutStringThenNewline(ev, "// OnStoreRowSpacesRows()");
  mWriter_LineSize = 0;
    
  if ( ev->Good() )
    mWriter_Phase = morkWriter_kPhaseContentDone;
  else
    mWriter_Phase = morkWriter_kPhaseWritingDone; // stop on error

  return ev->Good();
}

mork_bool
morkWriter::OnRowSpaceRows(morkEnv* ev)
{
  morkStream* stream = mWriter_Stream;
  if ( mWriter_LineSize )
    stream->PutLineBreak(ev);
  stream->PutStringThenNewline(ev, "// OnRowSpaceRows()");
  mWriter_LineSize = 0;
    
  if ( ev->Good() )
    mWriter_Phase = morkWriter_kPhaseContentDone;
  else
    mWriter_Phase = morkWriter_kPhaseWritingDone; // stop on error

  return ev->Good();
}

mork_bool
morkWriter::OnContentDone(morkEnv* ev)
{
  morkStream* stream = mWriter_Stream;
  if ( mWriter_LineSize )
    stream->PutLineBreak(ev);
  stream->PutStringThenNewline(ev, "// OnContentDone()");
  mWriter_LineSize = 0;
  stream->Flush(ev);
  morkFile* bud = mWriter_Bud;
  if ( bud )
  {
    bud->Flush(ev);
    bud->BecomeTrunk(ev);
    morkFile::SlotStrongFile((morkFile*) 0, ev, &mWriter_Bud);
  }
  else
    this->NilWriterBudError(ev);
    
  mWriter_Phase = morkWriter_kPhaseWritingDone; // stop always
  mWriter_DoneCount = mWriter_TotalCount;
  
  return ev->Good();
}

mork_bool
morkWriter::OnWritingDone(morkEnv* ev)
{
  mWriter_DoneCount = mWriter_TotalCount;
  ev->NewWarning("writing is done");
  return ev->Good();
}

mork_bool
morkWriter::PutTable(morkEnv* ev, morkTable* ioTable)
{
  if ( ev->Good() )
    this->StartTable(ev, ioTable);
    
  if ( ev->Good() )
  {
    morkArray* array = &ioTable->mTable_RowArray; // vector of rows
    mork_fill fill = array->mArray_Fill; // count of rows
    morkRow** rows = (morkRow**) array->mArray_Slots;
    if ( rows && fill )
    {
      morkRow** end = rows + fill;
      while ( rows < end && ev->Good() )
      {
        morkRow* r = *rows++; // next row to consider
        if ( r && r->IsRow() )
          this->PutRow(ev, r);
        else
          r->NonRowTypeError(ev);
      }
    }
  }
    
  if ( ev->Good() )
    this->EndTable(ev);

  ++mWriter_DoneCount;
  return ev->Good();
}

mork_bool
morkWriter::PutTableDict(morkEnv* ev, morkTable* ioTable)
{
  morkRowSpace* space = ioTable->mTable_RowSpace;
  mWriter_TableRowScope = space->mSpace_Scope;
  mWriter_TableCharset = 0;     // (charset=iso-8859-1)
  mWriter_TableAtomScope = 'a'; // (atomScope=a)
  mWriter_TableTableScope = 't'; // (tableScope=t)
  mWriter_TableColumnScope = 'c'; // (columnScope=c)
  mWriter_TableKind = ioTable->mTable_Kind;
  
  mWriter_RowCharset = mWriter_TableCharset;
  mWriter_RowAtomScope = mWriter_TableAtomScope;
  mWriter_RowScope = mWriter_TableRowScope;
  
  mWriter_DictCharset = mWriter_TableCharset;
  mWriter_DictAtomScope = mWriter_TableAtomScope;
  
  if ( ev->Good() )
    this->StartDict(ev);

  if ( ev->Good() )
  {
    morkArray* array = &ioTable->mTable_RowArray; // vector of rows
    mork_fill fill = array->mArray_Fill; // count of rows
    morkRow** rows = (morkRow**) array->mArray_Slots;
    if ( rows && fill )
    {
      morkRow** end = rows + fill;
      while ( rows < end && ev->Good() )
      {
        morkRow* r = *rows++; // next row to consider
        if ( r && r->IsRow() )
          this->PutRowDict(ev, r);
        else
          r->NonRowTypeError(ev);
      }
    }
  }
  if ( ev->Good() )
    this->EndDict(ev);
  
  return ev->Good();
}
  
void
morkWriter::WriteTokenToTokenMetaCell(morkEnv* ev,
  mork_token inCol, mork_token inValue)
{
  morkStream* stream = mWriter_Stream;
  char buf[ 128 ]; // buffer for staging the two hex IDs
  char* p = buf;
  *p++ = '('; // we always start with open paren
  *p++ = '^'; // indicates col is hex ID

  mork_size colSize = ev->TokenAsHex(p, inCol);
  p += colSize;
  *p++ = '='; // we always start with open paren
  mWriter_LineSize += stream->Write(ev, buf, colSize + 3);

  this->IndentAsNeeded(ev, morkWriter_kTableMetaCellValueDepth);
  mdbYarn* yarn = &mWriter_ColYarn;
  // mork_u1* yarnBuf = (mork_u1*) yarn->mYarn_Buf;
  mWriter_Store->TokenToString(ev, inValue, yarn);
  this->WriteYarn(ev, yarn);
  stream->Putc(ev, ')');
  ++mWriter_LineSize;
  
  // mork_fill fill = yarn->mYarn_Fill;
  // yarnBuf[ fill ] = ')'; // append terminator
  // mWriter_LineSize += stream->Write(ev, yarnBuf, fill + 1); // +1 for ')'
}
  
void
morkWriter::WriteStringToTokenDictCell(morkEnv* ev,
  const char* inCol, mork_token inValue)
  // Note inCol should begin with '(' and end with '=', with col in between.
{
  morkStream* stream = mWriter_Stream;
  mWriter_LineSize += stream->PutString(ev, inCol);

  this->IndentAsNeeded(ev, morkWriter_kDictMetaCellValueDepth);
  mdbYarn* yarn = &mWriter_ColYarn;
  // mork_u1* yarnBuf = (mork_u1*) yarn->mYarn_Buf;
  mWriter_Store->TokenToString(ev, inValue, yarn);
  this->WriteYarn(ev, yarn);
  stream->Putc(ev, ')');
  ++mWriter_LineSize;
  
  // mork_fill fill = yarn->mYarn_Fill;
  // yarnBuf[ fill ] = ')'; // append terminator
  // mWriter_LineSize += stream->Write(ev, yarnBuf, fill + 1); // +1 for ')'
}

void
morkWriter::StartDict(morkEnv* ev)
{
  morkStream* stream = mWriter_Stream;
  if ( mWriter_DidStartDict )
  {
    if ( mWriter_LineSize )
      stream->PutLineBreak(ev);
    stream->PutStringThenNewline(ev, "> // end dict");
    mWriter_LineSize = 0;
  }
  mWriter_DidStartDict = morkBool_kTrue;
  
  if ( mWriter_LineSize )
    stream->PutLineBreak(ev);
  mWriter_LineSize = 0;
  if ( mWriter_DictCharset || mWriter_DictAtomScope != 'a' )
  {
    stream->Putc(ev, '<');
    stream->Putc(ev, ' ');
    stream->Putc(ev, '<');
    mWriter_LineSize = 3;
    if ( mWriter_DictCharset )
      this->WriteStringToTokenDictCell(ev, "(charset=", mWriter_DictCharset);
    if ( mWriter_DictAtomScope != 'a' )
      this->WriteStringToTokenDictCell(ev, "(atomScope=", mWriter_DictAtomScope);
  
    stream->Putc(ev, '>');
    ++mWriter_LineSize;
  }
  else
  {
    stream->PutString(ev, "< // <(charset=iso-8859-1)(atomScope=a)>");
  }
  mWriter_LineSize = stream->PutIndent(ev, morkWriter_kDictAliasDepth);
}

void
morkWriter::EndDict(morkEnv* ev)
{
  morkStream* stream = mWriter_Stream;
  if ( mWriter_DidStartDict )
  {
    if ( mWriter_LineSize )
      stream->PutLineBreak(ev);
    stream->PutStringThenNewline(ev, "> // end dict");
    mWriter_LineSize = 0;
  }
  mWriter_DidStartDict = morkBool_kFalse;
}

void
morkWriter::StartTable(morkEnv* ev, morkTable* ioTable)
{
  mdbOid toid; // to receive table oid
  ioTable->GetTableOid(ev, &toid);
  
  if ( ev->Good() )
  {
    morkStream* stream = mWriter_Stream;
    if ( mWriter_LineSize )
      stream->PutLineBreak(ev);
    mWriter_LineSize = 0;

    char buf[ 64 ]; // buffer for staging hex
    char* p = buf;
    *p++ = '{';
    mork_size size = ev->OidAsHex(p, toid);
    p += size;
    *p++ = ' ';
    *p = '{';
    mWriter_LineSize += stream->Write(ev, buf, size + 3);
    
    morkStore* store = mWriter_Store;
    mork_scope rs = mWriter_TableRowScope;
    if ( rs )
    {
      this->IndentAsNeeded(ev, morkWriter_kTableMetaCellDepth);
      this->WriteTokenToTokenMetaCell(ev, store->mStore_RowScopeToken, rs);
    }
    mork_kind tk = mWriter_TableKind;
    if ( tk )
    {
      this->IndentAsNeeded(ev, morkWriter_kTableMetaCellDepth);
      this->WriteTokenToTokenMetaCell(ev, store->mStore_TableKindToken, tk);
    }
    stream->Putc(ev, '}');
    stream->Putc(ev, ' ');
    mWriter_LineSize += 2;
  }
}

void
morkWriter::EndTable(morkEnv* ev)
{
  morkStream* stream = mWriter_Stream;
  if ( mWriter_LineSize )
    stream->PutLineBreak(ev);
  stream->PutStringThenNewline(ev, "} // end table");
  mWriter_LineSize = 0;
}

mork_bool
morkWriter::PutRowDict(morkEnv* ev, morkRow* ioRow)
{
  morkCell* cells = ioRow->mRow_Cells;
  if ( cells )
  {
    morkStream* stream = mWriter_Stream;
    char buf[ 64 ]; // buffer for staging the dict alias hex ID
    char* idBuf = buf + 1; // where the id always starts
    buf[ 0 ] = '('; // we always start with open paren

    morkCell* end = cells + ioRow->mRow_Length;
    --cells; // prepare for preincrement:
    while ( ++cells < end && ev->Good() )
    {
      morkAtom* atom = cells->GetAtom();
      if ( atom && atom->mAtom_Change == morkChange_kAdd )
      {
        if ( atom->IsBook() ) // is it possible to write atom ID?
        {
          atom->mAtom_Change = morkChange_kNil; // neutralize change
          
          this->IndentAsNeeded(ev, morkWriter_kDictAliasDepth);
          morkBookAtom* ba = (morkBookAtom*) atom;
          mork_size size = ev->TokenAsHex(idBuf, ba->mBookAtom_Id);
          mWriter_LineSize += stream->Write(ev, buf, size+1); // '('

          this->IndentAsNeeded(ev, morkWriter_kDictAliasValueDepth);
          stream->Putc(ev, '='); // start value
          ++mWriter_LineSize;
      
          this->WriteAtom(ev, atom);
          stream->Putc(ev, ')'); // end alias
          ++mWriter_LineSize;
          
          ++mWriter_DoneCount;
        }
      }
    }
  }
  return ev->Good();
}

mork_bool
morkWriter::PutRowCells(morkEnv* ev, morkRow* ioRow)
{
  morkCell* cells = ioRow->mRow_Cells;
  if ( cells )
  {
    morkStream* stream = mWriter_Stream;
    char buf[ 128 ]; // buffer for staging hex ids
    char* idBuf = buf + 2; // where the id always starts
    buf[ 0 ] = '('; // we always start with open paren
    buf[ 1 ] = '^'; // column is always a hex ID
    
    mork_size colSize = 0; // the size of col hex ID

    morkCell* end = cells + ioRow->mRow_Length;
    --cells; // prepare for preincrement:
    while ( ++cells < end )
    {
      morkAtom* atom = cells->GetAtom();
      if ( atom ) // anything to write at all?
      {
        mork_column col = cells->GetColumn();
        char* p = idBuf;
        colSize = ev->TokenAsHex(p, col);
        p += colSize;
        
        this->IndentAsNeeded(ev, morkWriter_kRowCellDepth);
        if ( atom->IsBook() ) // is it possible to write atom ID?
        {
          *p++ = '^';
          morkBookAtom* ba = (morkBookAtom*) atom;
          mork_size valSize = ev->TokenAsHex(p, ba->mBookAtom_Id);
          p += valSize;
          *p = ')';

          mWriter_LineSize += stream->Write(ev, buf, colSize + valSize + 4);

          if ( atom->mAtom_Change == morkChange_kAdd )
          {
            atom->mAtom_Change = morkChange_kNil;
            ++mWriter_DoneCount;
          }
        }
        else // must write an anonymous atom
        {
          mWriter_LineSize += stream->Write(ev, buf, colSize + 2);

          this->IndentAsNeeded(ev, morkWriter_kRowCellValueDepth);
          stream->Putc(ev, '=');
          ++mWriter_LineSize;
          
          this->WriteAtom(ev, atom);
          stream->Putc(ev, ')'); // end alias
          ++mWriter_LineSize;
        }
      }
    }
  }
  return ev->Good();
}

mork_bool
morkWriter::PutRow(morkEnv* ev, morkRow* ioRow)
{
  morkStream* stream = mWriter_Stream;
  char buf[ 128 ]; // buffer for staging hex
  char* p = buf;
  mdbOid* roid = &ioRow->mRow_Oid;
  mork_size ridSize = 0;

  this->IndentAsNeeded(ev, morkWriter_kRowDepth);

  // if ( ioRow->IsRowDirty() )
  if ( morkBool_kTrue )
  {
    ioRow->SetRowClean();
    mork_rid rid = roid->mOid_Id;
    *p++ = '[';
    if ( roid->mOid_Scope == mWriter_TableRowScope )
      ridSize = ev->TokenAsHex(p, roid->mOid_Id);
    else
      ridSize = ev->OidAsHex(p, *roid);
    
    p += ridSize;
    *p++ = ' ';
    mWriter_LineSize += stream->Write(ev, buf, ridSize + 2);
    
    this->PutRowCells(ev, ioRow);
    stream->Putc(ev, ']'); // end row
    stream->Putc(ev, ' '); // end row
    mWriter_LineSize += 2;
  }
  else
  {
    if ( roid->mOid_Scope == mWriter_TableRowScope )
      ridSize = ev->TokenAsHex(p, roid->mOid_Id);
    else
      ridSize = ev->OidAsHex(p, *roid);

    mWriter_LineSize += stream->Write(ev, buf, ridSize);
    stream->Putc(ev, ' ');
    ++mWriter_LineSize;
  }

  ++mWriter_DoneCount;
  return ev->Good();
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

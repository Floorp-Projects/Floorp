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

// #ifndef _MORKFILE_
// #include "morkFile.h"
// #endif

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

#ifndef _MORKCH_
#include "morkCh.h"
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
    nsIMdbHeap* ioHeap, morkStore* ioStore, nsIMdbFile* ioFile,
    nsIMdbHeap* ioSlotHeap)
: morkNode(ev, inUsage, ioHeap)
, mWriter_Store( 0 )
, mWriter_File( 0 )
, mWriter_Bud( 0 )
, mWriter_Stream( 0 )
, mWriter_SlotHeap( 0 )

, mWriter_CommitGroupIdentity( 0 ) // see mStore_CommitGroupIdentity
, mWriter_GroupBufFill( 0 )

, mWriter_TotalCount( morkWriter_kCountNumberOfPhases )
, mWriter_DoneCount( 0 )

, mWriter_LineSize( 0 )
, mWriter_MaxIndent( morkWriter_kMaxIndent )
, mWriter_MaxLine( morkWriter_kMaxLine )
  
, mWriter_TableForm( 0 )
, mWriter_TableAtomScope( 'v' )
, mWriter_TableRowScope( 0 )
, mWriter_TableKind( 0 )
  
, mWriter_RowForm( 0 )
, mWriter_RowAtomScope( 0 )
, mWriter_RowScope( 0 )
  
, mWriter_DictForm( 0 )
, mWriter_DictAtomScope( 'v' )

, mWriter_NeedDirtyAll( morkBool_kFalse )
, mWriter_Incremental( morkBool_kTrue ) // opposite of mWriter_NeedDirtyAll
, mWriter_DidStartDict( morkBool_kFalse )
, mWriter_DidEndDict( morkBool_kTrue )

, mWriter_SuppressDirtyRowNewline( morkBool_kFalse )
, mWriter_DidStartGroup( morkBool_kFalse )
, mWriter_DidEndGroup( morkBool_kTrue )
, mWriter_Phase( morkWriter_kPhaseNothingDone )

, mWriter_BeVerbose( ev->mEnv_BeVerbose )

, mWriter_TableRowArrayPos( 0 )

// empty constructors for map iterators:
, mWriter_StoreAtomSpacesIter( )
, mWriter_AtomSpaceAtomAidsIter( )
  
, mWriter_StoreRowSpacesIter( )
, mWriter_RowSpaceTablesIter( )
, mWriter_RowSpaceRowsIter( )
{
  mWriter_GroupBuf[ 0 ] = 0;

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
      nsIMdbFile_SlotStrongFile(ioFile, ev, &mWriter_File);
      nsIMdbHeap_SlotStrongHeap(ioSlotHeap, ev, &mWriter_SlotHeap);
      if ( ev->Good() )
      {
        mNode_Derived = morkDerived_kWriter;
      }
    }
    else
      ev->NilPointerError();
  }
}


void
morkWriter::MakeWriterStream(morkEnv* ev) // give writer a suitable stream
{
  mWriter_Incremental = !mWriter_NeedDirtyAll; // opposites
  
  if ( !mWriter_Stream && ev->Good() )
  {
    if ( mWriter_File )
    {
      morkStream* stream = 0;
      mork_bool frozen = morkBool_kFalse; // need to modify
      nsIMdbHeap* heap = mWriter_SlotHeap;
    
      if ( mWriter_Incremental )
      {
        stream = new(*heap, ev)
          morkStream(ev, morkUsage::kHeap, heap, mWriter_File,
            morkWriter_kStreamBufSize, frozen);
      }
      else // compress commit
      {
        nsIMdbFile* bud = 0;
        mWriter_File->AcquireBud(ev->AsMdbEnv(), heap, &bud);
        if ( bud )
        {
          if ( ev->Good() )
          {
            mWriter_Bud = bud;
            stream = new(*heap, ev)
              morkStream(ev, morkUsage::kHeap, heap, bud,
                morkWriter_kStreamBufSize, frozen);
          }
          else
            bud->CutStrongRef(ev->AsMdbEnv());
        }
      }
        
      if ( stream )
      {
        if ( ev->Good() )
          mWriter_Stream = stream;
        else
          stream->CutStrongRef(ev);
      }
    }
    else
      this->NilWriterFileError(ev);
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
      nsIMdbFile_SlotStrongFile((nsIMdbFile*) 0, ev, &mWriter_File);
      nsIMdbFile_SlotStrongFile((nsIMdbFile*) 0, ev, &mWriter_Bud);
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
morkWriter::NilWriterFileError(morkEnv* ev)
{
  ev->NewError("nil mWriter_File");
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
      if ( !mWriter_Stream )
        this->MakeWriterStream(ev);
        
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
  mork_size lineSize = mWriter_LineSize;
  morkStream* stream = mWriter_Stream;

  const mork_u1* b = (const mork_u1*) inYarn->mYarn_Buf;
  if ( b )
  {
    register int c;
    mork_fill fill = inYarn->mYarn_Fill;

    const mork_u1* end = b + fill;
    while ( b < end && ev->Good() )
    {
      if ( lineSize + outSize >= mWriter_MaxLine ) // continue line?
      {
        stream->PutByteThenNewline(ev, '\\');
        mWriter_LineSize = lineSize = outSize = 0;
      }
      
      c = *b++; // next byte to print
      if ( morkCh_IsValue(c) )
      {
        stream->Putc(ev, c);
        ++outSize; // c
      }
      else if ( c == ')' || c == '$' || c == '\\' )
      {
        stream->Putc(ev, '\\');
        stream->Putc(ev, c);
        outSize += 2; // '\' c
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
  {
    if ( mWriter_DidStartDict && yarn.mYarn_Form != mWriter_DictForm )
      this->ChangeDictForm(ev, yarn.mYarn_Form);  
      
    outSize = this->WriteYarn(ev, &yarn);
    // mWriter_LineSize += stream->Write(ev, inYarn->mYarn_Buf, outSize);
  }
  else
    inAtom->BadAtomKindError(ev);
    
  return outSize;
}

void
morkWriter::WriteAtomSpaceAsDict(morkEnv* ev, morkAtomSpace* ioSpace)
{
  morkStream* stream = mWriter_Stream;
  mork_scope scope = ioSpace->SpaceScope();
  if ( scope < 0x80 )
  {
    if ( mWriter_LineSize )
      stream->PutLineBreak(ev);
    stream->PutString(ev, "< <(a=");
    stream->Putc(ev, (int) scope);
    ++mWriter_LineSize;
    stream->PutString(ev, ")> // (f=iso-8859-1)");
    mWriter_LineSize = stream->PutIndent(ev, morkWriter_kDictAliasDepth);
  }
  else
    ioSpace->NonAsciiSpaceScopeName(ev);

  if ( ev->Good() )
  {
    mdbYarn yarn; // to ref content inside atom
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
        if ( atom->IsAtomDirty() )
        {
          atom->SetAtomClean(); // neutralize change
          
          atom->AliasYarn(&yarn);
          mork_size size = ev->TokenAsHex(idBuf, atom->mBookAtom_Id);
          
          if ( yarn.mYarn_Form != mWriter_DictForm )
            this->ChangeDictForm(ev, yarn.mYarn_Form);

          mork_size pending = yarn.mYarn_Fill + size + 
            morkWriter_kYarnEscapeSlop + 4;
          this->IndentOverMaxLine(ev, pending, morkWriter_kDictAliasDepth);
          mWriter_LineSize += stream->Write(ev, buf, size+1); //  + '('
          
          pending -= ( size + 1 );
          this->IndentOverMaxLine(ev, pending, morkWriter_kDictAliasValueDepth);
          stream->Putc(ev, '='); // start alias
          ++mWriter_LineSize;
          
          this->WriteYarn(ev, &yarn);
          stream->Putc(ev, ')'); // end alias
          ++mWriter_LineSize;
          
          ++mWriter_DoneCount;
        }
      }
      else
        ev->NilPointerError();
    }
    ai->CloseMapIter(ev);
  }
  
  if ( ev->Good() )
  {
    ioSpace->SetAtomSpaceClean();
    // this->IndentAsNeeded(ev, 0);
    // stream->PutByteThenNewline(ev, '>'); // end dict
    
    stream->Putc(ev, '>'); // end dict
    ++mWriter_LineSize;
  }
}

/*
(I'm putting the text of this message in file morkWriter.cpp.)

I'm making a change which should cause rows and tables to go away
when a Mork db is compress committed, when the rows and tables
are no longer needed.  Because this is subtle, I'm describing it
here in case misbehavior is ever observed.  Otherwise you'll have
almost no hope of fixing a related bug.

This is done entirely in morkWriter.cpp: morkWriter::DirtyAll(),
which currently marks all rows and tables dirty so they will be
written in a later phase of the commit.  My change is to merely
selectively not mark certain rows and tables dirty, when they seem
to be superfluous.

A row is no longer needed when the mRow_GcUses slot hits zero, and
this is used by the following inline morkRow method:

  mork_bool IsRowUsed() const { return mRow_GcUses != 0; }

Naturally disaster ensues if mRow_GcUses is ever smaller than right.

Similarly, we should drop tables when mTable_GcUses hits zero, but
only when a table contains no row members.  We consider tables to
self reference (and prevent collection) when they contain content.
Again, disaster ensues if mTable_GcUses is ever smaller than right.

  mork_count GetRowCount() const
  { return mTable_RowArray.mArray_Fill; }

  mork_bool IsTableUsed() const
  { return (mTable_GcUses != 0 || this->GetRowCount() != 0); }

Now let's question why the design involves filtering what gets set
to dirty.  Why not apply a filter in the later phase when we write
content?  Because I'm afraid of missing some subtle interaction in
updating table and row relationships.  It seems safer to write a row
or table when it starts out dirty, before morkWriter::DirtyAll() is
called.  So this design calls for writing out rows and tables when
they are still clearly used, and additionally, <i>when we have just
been actively writing to them right before this commit</i>.

Presumably if they are truly useless, they will no longer be dirtied
in later sessions and will get collected during the next compress
commit.  So we wait to collect them until they become all dead, and
not just mostly dead.  (At which time you can feel free to go through
their pockets looking for loose change.)
*/

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
    store->SetStoreDirty();
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
            space->SetAtomSpaceDirty();
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
            space->SetRowSpaceDirty();
            if ( ev->Good() )
            {
#ifdef MORK_ENABLE_PROBE_MAPS
              morkRowProbeMapIter* ri = &mWriter_RowSpaceRowsIter;
#else /*MORK_ENABLE_PROBE_MAPS*/
              morkRowMapIter* ri = &mWriter_RowSpaceRowsIter;
#endif /*MORK_ENABLE_PROBE_MAPS*/
              ri->InitRowMapIter(ev, &space->mRowSpace_Rows);

              morkRow* row = 0; // old key row in the map
                
              for ( c = ri->FirstRow(ev, &row); c && ev->Good();
                    c = ri->NextRow(ev, &row) )
              {
                if ( row && row->IsRow() ) // need to dirty row?
                {
                	if ( row->IsRowUsed() || row->IsRowDirty() )
                	{
	                  row->DirtyAllRowContent(ev);
	                  ++mWriter_TotalCount;
                	}
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

#ifdef MORK_BEAD_OVER_NODE_MAPS
              morkTable* table = ti->FirstTable(ev);
                
              for ( ; table && ev->Good(); table = ti->NextTable(ev) )
#else /*MORK_BEAD_OVER_NODE_MAPS*/
              mork_tid* tableKey = 0; // ignore keys in table map
              morkTable* table = 0; // old key row in the map
                
              for ( c = ti->FirstTable(ev, tableKey, &table); c && ev->Good();
                    c = ti->NextTable(ev, tableKey, &table) )
#endif /*MORK_BEAD_OVER_NODE_MAPS*/
              {
                if ( table && table->IsTable() ) // need to dirty table?
                {
                	if ( table->IsTableUsed() || table->IsTableDirty() )
                	{
	                  // table->DirtyAllTableContent(ev);
	                  // only necessary to mark table itself dirty:
	                  table->SetTableDirty();
	                  table->SetTableRewrite();
	                  ++mWriter_TotalCount;
                	}
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
  mWriter_Incremental = !mWriter_NeedDirtyAll; // opposites
  
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
morkWriter::StartGroup(morkEnv* ev)
{
  morkStream* stream = mWriter_Stream;
  mWriter_DidStartGroup = morkBool_kTrue;
  mWriter_DidEndGroup = morkBool_kFalse;

  char buf[ 64 ];
  char* p = buf;
  *p++ = '@';
  *p++ = '$';
  *p++ = '$';
  *p++ = '{';
  
  mork_token groupID = mWriter_CommitGroupIdentity;
  mork_fill idFill = ev->TokenAsHex(p, groupID);
  mWriter_GroupBufFill = 0;
  // ev->TokenAsHex(mWriter_GroupBuf, groupID);
  if ( idFill < morkWriter_kGroupBufSize )
  {
    MORK_MEMCPY(mWriter_GroupBuf, p, idFill + 1);
    mWriter_GroupBufFill = idFill;
  }
  else
    *mWriter_GroupBuf = 0;
    
  p += idFill;
  *p++ = '{';
  *p++ = '@';
  *p = 0;

  stream->PutLineBreak(ev);
  
  morkStore* store = mWriter_Store;
  if ( store ) // might need to capture commit group position?
  {
    mork_pos groupPos = stream->Tell(ev);
    if ( !store->mStore_FirstCommitGroupPos )
      store->mStore_FirstCommitGroupPos = groupPos;
    else if ( !store->mStore_SecondCommitGroupPos )
      store->mStore_SecondCommitGroupPos = groupPos;
  }
  
  stream->Write(ev, buf, idFill + 6); // '@$${' + idFill + '{@'
  stream->PutLineBreak(ev);
  mWriter_LineSize = 0;
  
  return ev->Good();
}

mork_bool
morkWriter::CommitGroup(morkEnv* ev)
{
  if ( mWriter_DidStartGroup )
  {
    morkStream* stream = mWriter_Stream;
  
    if ( mWriter_LineSize )
      stream->PutLineBreak(ev);
      
    stream->Putc(ev, '@');
    stream->Putc(ev, '$');
    stream->Putc(ev, '$');
    stream->Putc(ev, '}');
    
    mork_fill bufFill = mWriter_GroupBufFill;
    if ( bufFill )
      stream->Write(ev, mWriter_GroupBuf, bufFill);

    stream->Putc(ev, '}');
    stream->Putc(ev, '@');
    stream->PutLineBreak(ev);

    mWriter_LineSize = 0;
  }

  mWriter_DidStartGroup = morkBool_kFalse;
  mWriter_DidEndGroup = morkBool_kTrue;
  
  return ev->Good();
}

mork_bool
morkWriter::AbortGroup(morkEnv* ev)
{
  if ( mWriter_DidStartGroup )
  {
    morkStream* stream = mWriter_Stream;
    stream->PutLineBreak(ev);
    stream->PutStringThenNewline(ev, "@$$}~~}@");
    mWriter_LineSize = 0;
  }
  
  mWriter_DidStartGroup = morkBool_kFalse;
  mWriter_DidEndGroup = morkBool_kTrue;

  return ev->Good();
}


mork_bool
morkWriter::OnDirtyAllDone(morkEnv* ev)
{
  if ( ev->Good() )
  {
    morkStream* stream = mWriter_Stream;
    if ( mWriter_NeedDirtyAll ) // compress commit
    {
      stream->Seek(ev, 0); // beginning of stream
      stream->PutStringThenNewline(ev, morkWriter_kFileHeader);
      mWriter_LineSize = 0;
    }
    else // else mWriter_Incremental
    {
      mork_pos eos = stream->Length(ev); // length is end of stream
      if ( ev->Good() )
      {
        stream->Seek(ev, eos); // goto end of stream
        if ( eos < 128 ) // maybe need file header?
        {
          stream->PutStringThenNewline(ev, morkWriter_kFileHeader);
          mWriter_LineSize = 0;
        }
        this->StartGroup(ev); // begin incremental transaction
      }
    }
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
  
  // if ( mWriter_NeedDirtyAll )
  //   stream->PutStringThenNewline(ev, "// OnPutHeaderDone()");
  mWriter_LineSize = 0;
  
  if ( mWriter_NeedDirtyAll ) // compress commit
  {
    morkStore* store = mWriter_Store;
    if ( store )
      store->RenumberAllCollectableContent(ev);
    else
      this->NilWriterStoreError(ev);
  }
    
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
    
  // if ( mWriter_NeedDirtyAll )
  //  stream->PutStringThenNewline(ev, "// OnRenumberAllDone()");
  mWriter_LineSize = 0;
  
  if ( mWriter_NeedDirtyAll ) // compress commit
  {
  }
    
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

  // if ( mWriter_NeedDirtyAll )
  //   stream->PutStringThenNewline(ev, "// OnStoreAtomSpaces()");
  mWriter_LineSize = 0;
  
  if ( mWriter_NeedDirtyAll ) // compress commit
  {
  }
  
  if ( ev->Good() )
  {
    morkStore* store = mWriter_Store;
    if ( store )
    {
      morkAtomSpace* space = store->LazyGetGroundColumnSpace(ev);
      if ( space && space->IsAtomSpaceDirty() )
      {
        // stream->PutStringThenNewline(ev, "// ground column space dict:");
        
        if ( mWriter_LineSize )
        {
          stream->PutLineBreak(ev);
          mWriter_LineSize = 0;
        }
        this->WriteAtomSpaceAsDict(ev, space);
        space->SetAtomSpaceClean();
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

  // if ( mWriter_NeedDirtyAll )
  //   stream->PutStringThenNewline(ev, "// OnAtomSpaceAtomAids()");
  mWriter_LineSize = 0;
  
  if ( mWriter_NeedDirtyAll ) // compress commit
  {
  }
    
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
          space->SetRowSpaceClean();
          if ( ev->Good() )
          {
            morkTableMapIter* ti = &mWriter_RowSpaceTablesIter;
            ti->InitTableMapIter(ev, &space->mRowSpace_Tables);

#ifdef MORK_BEAD_OVER_NODE_MAPS
            morkTable* table = ti->FirstTable(ev);
              
            for ( ; table && ev->Good(); table = ti->NextTable(ev) )
#else /*MORK_BEAD_OVER_NODE_MAPS*/
            mork_tid* key2 = 0; // ignore keys in table map
            morkTable* table = 0; // old key row in the map
              
            for ( c = ti->FirstTable(ev, key2, &table); c && ev->Good();
                  c = ti->NextTable(ev, key2, &table) )
#endif /*MORK_BEAD_OVER_NODE_MAPS*/
            {
              if ( table && table->IsTable() )
              {
                if ( table->IsTableDirty() )
                {
                  mWriter_BeVerbose =
                    ( ev->mEnv_BeVerbose || table->IsTableVerbose() );
                    
                  if ( this->PutTableDict(ev, table) )
                    this->PutTable(ev, table);

                  table->SetTableClean(ev);
                  mWriter_BeVerbose = ev->mEnv_BeVerbose;
                }
              }
              else
                table->NonTableTypeWarning(ev);
            }
            ti->CloseMapIter(ev);
          }
          if ( ev->Good() )
          {
            mWriter_TableRowScope = 0; // ensure no table context now
            
#ifdef MORK_ENABLE_PROBE_MAPS
            morkRowProbeMapIter* ri = &mWriter_RowSpaceRowsIter;
#else /*MORK_ENABLE_PROBE_MAPS*/
            morkRowMapIter* ri = &mWriter_RowSpaceRowsIter;
#endif /*MORK_ENABLE_PROBE_MAPS*/
            ri->InitRowMapIter(ev, &space->mRowSpace_Rows);

            morkRow* row = 0; // old row in the map
              
            for ( c = ri->FirstRow(ev, &row); c && ev->Good();
                  c = ri->NextRow(ev, &row) )
            {
              if ( row && row->IsRow() )
              {
                // later we should also check that table use count is nonzero:
                if ( row->IsRowDirty() ) // && row->IsRowUsed() ??
                {
                  mWriter_BeVerbose = ev->mEnv_BeVerbose;
                  if ( this->PutRowDict(ev, row) )
                  {
                    if ( ev->Good() && mWriter_DidStartDict )
                    {
                      this->EndDict(ev);
                      if ( mWriter_LineSize < 32 && ev->Good() )
                        mWriter_SuppressDirtyRowNewline = morkBool_kTrue;
                    }
                      
                    if ( ev->Good() )
                      this->PutRow(ev, row);
                  }
                  mWriter_BeVerbose = ev->mEnv_BeVerbose;
                }
              }
              else
                row->NonRowTypeWarning(ev);
            }
            ri->CloseMapIter(ev);
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

  // if ( mWriter_NeedDirtyAll )
  //   stream->PutStringThenNewline(ev, "// OnStoreRowSpacesTables()");
  mWriter_LineSize = 0;
  
  if ( mWriter_NeedDirtyAll ) // compress commit
  {
  }
  
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

  // if ( mWriter_NeedDirtyAll )
  //   stream->PutStringThenNewline(ev, "// OnRowSpaceTables()");
  mWriter_LineSize = 0;
  
  if ( mWriter_NeedDirtyAll ) // compress commit
  {
  }
    
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

  // if ( mWriter_NeedDirtyAll )
  //   stream->PutStringThenNewline(ev, "// OnTableRowArray()");
  mWriter_LineSize = 0;
  
  if ( mWriter_NeedDirtyAll ) // compress commit
  {
  }
    
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

  // if ( mWriter_NeedDirtyAll )
  //   stream->PutStringThenNewline(ev, "// OnStoreRowSpacesRows()");
  mWriter_LineSize = 0;
  
  if ( mWriter_NeedDirtyAll ) // compress commit
  {
  }
    
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

  // if ( mWriter_NeedDirtyAll )
  //   stream->PutStringThenNewline(ev, "// OnRowSpaceRows()");
  mWriter_LineSize = 0;
  
  if ( mWriter_NeedDirtyAll ) // compress commit
  {
  }
    
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

  // if ( mWriter_NeedDirtyAll )
  //   stream->PutStringThenNewline(ev, "// OnContentDone()");
  mWriter_LineSize = 0;
  
  if ( mWriter_Incremental )
  {
    if ( ev->Good() )
      this->CommitGroup(ev);
    else
      this->AbortGroup(ev);
  }
  else if ( mWriter_Store && ev->Good() )
  {
    // after rewriting everything, there are no transaction groups:
    mWriter_Store->mStore_FirstCommitGroupPos = 0;
    mWriter_Store->mStore_SecondCommitGroupPos = 0;
  }
  
  stream->Flush(ev);
  nsIMdbFile* bud = mWriter_Bud;
  if ( bud )
  {
    bud->Flush(ev->AsMdbEnv());
    bud->BecomeTrunk(ev->AsMdbEnv());
    nsIMdbFile_SlotStrongFile((nsIMdbFile*) 0, ev, &mWriter_Bud);
  }
  else if ( !mWriter_Incremental ) // should have a bud?
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
morkWriter::PutTableChange(morkEnv* ev, const morkTableChange* inChange)
{
  if ( inChange->IsAddRowTableChange() )
  {
    this->PutRow(ev, inChange->mTableChange_Row ); // row alone means add
  }
  else if ( inChange->IsCutRowTableChange() )
  {
    mWriter_Stream->Putc(ev, '-'); // prefix '-' indicates cut row
    ++mWriter_LineSize;
    this->PutRow(ev, inChange->mTableChange_Row );
  }
  else if ( inChange->IsMoveRowTableChange() )
  {
    this->PutRow(ev, inChange->mTableChange_Row );
    char buf[ 64 ];
    char* p = buf;
    *p++ = '!'; // for moves, position is indicated by prefix '!'
    mork_size posSize = ev->TokenAsHex(p, inChange->mTableChange_Pos);
    p += posSize;
    *p++ = ' ';
    mWriter_LineSize += mWriter_Stream->Write(ev, buf, posSize + 2);
  }
  else
    inChange->UnknownChangeError(ev);
  
  return ev->Good();
}

mork_bool
morkWriter::PutTable(morkEnv* ev, morkTable* ioTable)
{
  if ( ev->Good() )
    this->StartTable(ev, ioTable);
    
  if ( ev->Good() )
  {
    if ( ioTable->IsTableRewrite() || mWriter_NeedDirtyAll )
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
          this->PutRow(ev, r);
        }
      }
    }
    else // incremental write only table changes
    {
      morkList* list = &ioTable->mTable_ChangeList;
      morkNext* next = list->GetListHead();
      while ( next && ev->Good() )
      {
        this->PutTableChange(ev, (morkTableChange*) next);
        next = next->GetNextLink();
      }
    }
  }
    
  if ( ev->Good() )
    this->EndTable(ev);
  
  ioTable->SetTableClean(ev); // note this also cleans change list
  mWriter_TableRowScope = 0;

  ++mWriter_DoneCount;
  return ev->Good();
}

mork_bool
morkWriter::PutTableDict(morkEnv* ev, morkTable* ioTable)
{
  morkRowSpace* space = ioTable->mTable_RowSpace;
  mWriter_TableRowScope = space->SpaceScope();
  mWriter_TableForm = 0;     // (f=iso-8859-1)
  mWriter_TableAtomScope = 'v'; // (a=v)
  mWriter_TableKind = ioTable->mTable_Kind;
  
  mWriter_RowForm = mWriter_TableForm;
  mWriter_RowAtomScope = mWriter_TableAtomScope;
  mWriter_RowScope = mWriter_TableRowScope;
  
  mWriter_DictForm = mWriter_TableForm;
  mWriter_DictAtomScope = mWriter_TableAtomScope;
  
  // if ( ev->Good() )
  //  this->StartDict(ev); // delay as long as possible

  if ( ev->Good() )
  {
    morkRow* r = ioTable->mTable_MetaRow;
    if ( r )
    {
      if ( r->IsRow() )
        this->PutRowDict(ev, r);
      else
        r->NonRowTypeError(ev);
    }
    morkArray* array = &ioTable->mTable_RowArray; // vector of rows
    mork_fill fill = array->mArray_Fill; // count of rows
    morkRow** rows = (morkRow**) array->mArray_Slots;
    if ( rows && fill )
    {
      morkRow** end = rows + fill;
      while ( rows < end && ev->Good() )
      {
        r = *rows++; // next row to consider
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
  mork_bool isKindCol = ( morkStore_kKindColumn == inCol );
  mork_u1 valSep = (mork_u1) (( isKindCol )? '^' : '=');
  
  char buf[ 128 ]; // buffer for staging the two hex IDs
  char* p = buf;

  if ( inCol < 0x80 )
  {
    stream->Putc(ev, '(');
    stream->Putc(ev, (char) inCol);
    stream->Putc(ev, valSep);
  }
  else
  {
    *p++ = '('; // we always start with open paren
    
    *p++ = '^'; // indicates col is hex ID
    mork_size colSize = ev->TokenAsHex(p, inCol);
    p += colSize;
    *p++ = (char) valSep;
    mWriter_LineSize += stream->Write(ev, buf, colSize + 3);
  }

  if ( isKindCol )
  {
    p = buf;
    mork_size valSize = ev->TokenAsHex(p, inValue);
    p += valSize;
    *p++ = ':';
    *p++ = 'c';
    *p++ = ')';
    mWriter_LineSize += stream->Write(ev, buf, valSize + 3);
  }
  else
  {
    this->IndentAsNeeded(ev, morkWriter_kTableMetaCellValueDepth);
    mdbYarn* yarn = &mWriter_ColYarn;
    // mork_u1* yarnBuf = (mork_u1*) yarn->mYarn_Buf;
    mWriter_Store->TokenToString(ev, inValue, yarn);
    this->WriteYarn(ev, yarn);
    stream->Putc(ev, ')');
    ++mWriter_LineSize;
  }
  
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
morkWriter::ChangeDictAtomScope(morkEnv* ev, mork_scope inScope)
{
  if ( inScope != mWriter_DictAtomScope )
  {
    ev->NewWarning("unexpected atom scope change");
    
    morkStream* stream = mWriter_Stream;
    if ( mWriter_LineSize )
      stream->PutLineBreak(ev);
    mWriter_LineSize = 0;

    char buf[ 128 ]; // buffer for staging the two hex IDs
    char* p = buf;
    *p++ = '<'; // we always start with open paren
    *p++ = '('; // we always start with open paren
    *p++ = (char) morkStore_kAtomScopeColumn;

    mork_size scopeSize = 1; // default to one byte
    if ( inScope >= 0x80 )
    {
      *p++ = '^'; // indicates col is hex ID
      scopeSize = ev->TokenAsHex(p, inScope);
      p += scopeSize;
    }
    else
    {
      *p++ = '='; // indicates col is imm byte
      *p++ = (char) (mork_u1) inScope;
    }

    *p++ = ')';
    *p++ = '>';
    *p = 0;

    mork_size pending = scopeSize + 6;
    this->IndentOverMaxLine(ev, pending, morkWriter_kDictAliasDepth);
    
    mWriter_LineSize += stream->Write(ev, buf, pending);
      
    mWriter_DictAtomScope = inScope;
  }
}

void
morkWriter::ChangeRowForm(morkEnv* ev, mork_cscode inNewForm)
{
  if ( inNewForm != mWriter_RowForm )
  {
    morkStream* stream = mWriter_Stream;
    if ( mWriter_LineSize )
      stream->PutLineBreak(ev);
    mWriter_LineSize = 0;

    char buf[ 128 ]; // buffer for staging the two hex IDs
    char* p = buf;
    *p++ = '['; // we always start with open bracket
    *p++ = '('; // we always start with open paren
    *p++ = (char) morkStore_kFormColumn;

    mork_size formSize = 1; // default to one byte
    if (! morkCh_IsValue(inNewForm))
    {
      *p++ = '^'; // indicates col is hex ID
      formSize = ev->TokenAsHex(p, inNewForm);
      p += formSize;
    }
    else
    {
      *p++ = '='; // indicates col is imm byte
      *p++ = (char) (mork_u1) inNewForm;
    }
    
    *p++ = ')';
    *p++ = ']';
    *p = 0;

    mork_size pending = formSize + 6;
    this->IndentOverMaxLine(ev, pending, morkWriter_kRowCellDepth);
    
    mWriter_LineSize += stream->Write(ev, buf, pending);
      
    mWriter_RowForm = inNewForm;
  }
}

void
morkWriter::ChangeDictForm(morkEnv* ev, mork_cscode inNewForm)
{
  if ( inNewForm != mWriter_DictForm )
  {
    morkStream* stream = mWriter_Stream;
    if ( mWriter_LineSize )
      stream->PutLineBreak(ev);
    mWriter_LineSize = 0;

    char buf[ 128 ]; // buffer for staging the two hex IDs
    char* p = buf;
    *p++ = '<'; // we always start with open angle
    *p++ = '('; // we always start with open paren
    *p++ = (char) morkStore_kFormColumn;

    mork_size formSize = 1; // default to one byte
    if (! morkCh_IsValue(inNewForm))
    {
      *p++ = '^'; // indicates col is hex ID
      formSize = ev->TokenAsHex(p, inNewForm);
      p += formSize;
    }
    else
    {
      *p++ = '='; // indicates col is imm byte
      *p++ = (char) (mork_u1) inNewForm;
    }
    
    *p++ = ')';
    *p++ = '>';
    *p = 0;

    mork_size pending = formSize + 6;
    this->IndentOverMaxLine(ev, pending, morkWriter_kDictAliasDepth);
    
    mWriter_LineSize += stream->Write(ev, buf, pending);
      
    mWriter_DictForm = inNewForm;
  }
}

void
morkWriter::StartDict(morkEnv* ev)
{
  morkStream* stream = mWriter_Stream;
  if ( mWriter_DidStartDict )
  {
    stream->Putc(ev, '>'); // end dict
    ++mWriter_LineSize;
  }
  mWriter_DidStartDict = morkBool_kTrue;
  mWriter_DidEndDict = morkBool_kFalse;
  
  if ( mWriter_LineSize )
    stream->PutLineBreak(ev);
  mWriter_LineSize = 0;
  
  if ( mWriter_TableRowScope ) // blank line before table's dict?
    stream->PutLineBreak(ev);
    
  if ( mWriter_DictForm || mWriter_DictAtomScope != 'v' )
  {
    stream->Putc(ev, '<');
    stream->Putc(ev, ' ');
    stream->Putc(ev, '<');
    mWriter_LineSize = 3;
    if ( mWriter_DictForm )
      this->WriteStringToTokenDictCell(ev, "(f=", mWriter_DictForm);
    if ( mWriter_DictAtomScope != 'v' )
      this->WriteStringToTokenDictCell(ev, "(a=", mWriter_DictAtomScope);
  
    stream->Putc(ev, '>');
    ++mWriter_LineSize;

    mWriter_LineSize = stream->PutIndent(ev, morkWriter_kDictAliasDepth);
  }
  else
  {
    stream->Putc(ev, '<');
    // stream->Putc(ev, ' ');
    ++mWriter_LineSize;
  }
}

void
morkWriter::EndDict(morkEnv* ev)
{
  morkStream* stream = mWriter_Stream;
  if ( mWriter_DidStartDict )
  {
    stream->Putc(ev, '>'); // end dict
    ++mWriter_LineSize;
  }
  mWriter_DidStartDict = morkBool_kFalse;
  mWriter_DidEndDict = morkBool_kTrue;
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
    // stream->PutLineBreak(ev);

    char buf[ 64 + 16 ]; // buffer for staging hex
    char* p = buf;
    *p++ = '{'; // punct 1
    mork_size punctSize = (mWriter_BeVerbose) ? 10 : 3; // counting "{ {/*r=*/ "

    if ( ioTable->IsTableRewrite() && mWriter_Incremental )
    {
      *p++ = '-';
      ++punctSize; // counting '-' // punct ++
      ++mWriter_LineSize;
    }
    mork_size oidSize = ev->OidAsHex(p, toid);
    p += oidSize;
    *p++ = ' '; // punct 2
    *p++ = '{'; // punct 3
    if (mWriter_BeVerbose)
    {
    
      *p++ = '/'; // punct=4
      *p++ = '*'; // punct=5
      *p++ = 'r'; // punct=6
      *p++ = '='; // punct=7

      mork_token tableUses = (mork_token) ioTable->mTable_GcUses;
      mork_size usesSize = ev->TokenAsHex(p, tableUses);
      punctSize += usesSize;
      p += usesSize;
    
      *p++ = '*'; // punct=8
      *p++ = '/'; // punct=9
      *p++ = ' '; // punct=10
    }
    mWriter_LineSize += stream->Write(ev, buf, oidSize + punctSize);
    
    mork_kind tk = mWriter_TableKind;
    if ( tk )
    {
      this->IndentAsNeeded(ev, morkWriter_kTableMetaCellDepth);
      this->WriteTokenToTokenMetaCell(ev, morkStore_kKindColumn, tk);
    }
      
    stream->Putc(ev, '('); // start 's' col cell
    stream->Putc(ev, 's'); // column
    stream->Putc(ev, '='); // column
    mWriter_LineSize += 3;

    int prio = (int) ioTable->mTable_Priority;
    if ( prio > 9 ) // need to force down to max decimal digit?
      prio = 9;
    prio += '0'; // add base digit zero
    stream->Putc(ev, prio); // priority: (s=0
    ++mWriter_LineSize;
    
    if ( ioTable->IsTableUnique() )
    {
      stream->Putc(ev, 'u'); // (s=0u
      ++mWriter_LineSize;
    }
    if ( ioTable->IsTableVerbose() )
    {
      stream->Putc(ev, 'v'); // (s=0uv
      ++mWriter_LineSize;
    }
    
    // stream->Putc(ev, ':'); // (s=0uv:
    // stream->Putc(ev, 'c'); // (s=0uv:c
    stream->Putc(ev, ')'); // end 's' col cell (s=0uv:c)
    mWriter_LineSize += 1; // maybe 3 if we add ':' and 'c'

    morkRow* r = ioTable->mTable_MetaRow;
    if ( r )
    {
      if ( r->IsRow() )
      {
        mWriter_SuppressDirtyRowNewline = morkBool_kTrue;
        this->PutRow(ev, r);
      }
      else
        r->NonRowTypeError(ev);
    }
    
    stream->Putc(ev, '}'); // end meta
    ++mWriter_LineSize;
    
    if ( mWriter_LineSize < mWriter_MaxIndent )
    {
      stream->Putc(ev, ' '); // nice white space
      ++mWriter_LineSize;
    }
  }
}

void
morkWriter::EndTable(morkEnv* ev)
{
  morkStream* stream = mWriter_Stream;
  stream->Putc(ev, '}'); // end table
  ++mWriter_LineSize;

  mWriter_TableAtomScope = 'v'; // (a=v)
}

mork_bool
morkWriter::PutRowDict(morkEnv* ev, morkRow* ioRow)
{
  mWriter_RowForm = mWriter_TableForm;

  morkCell* cells = ioRow->mRow_Cells;
  if ( cells )
  {
    morkStream* stream = mWriter_Stream;
    mdbYarn yarn; // to ref content inside atom
    char buf[ 64 ]; // buffer for staging the dict alias hex ID
    char* idBuf = buf + 1; // where the id always starts
    buf[ 0 ] = '('; // we always start with open paren

    morkCell* end = cells + ioRow->mRow_Length;
    --cells; // prepare for preincrement:
    while ( ++cells < end && ev->Good() )
    {
      morkAtom* atom = cells->GetAtom();
      if ( atom && atom->IsAtomDirty() )
      {
        if ( atom->IsBook() ) // is it possible to write atom ID?
        {
          if ( !this->DidStartDict() )
          {
            this->StartDict(ev);
            if ( ev->Bad() )
              break;
          }
          atom->SetAtomClean(); // neutralize change
          
          this->IndentAsNeeded(ev, morkWriter_kDictAliasDepth);
          morkBookAtom* ba = (morkBookAtom*) atom;
          mork_size size = ev->TokenAsHex(idBuf, ba->mBookAtom_Id);
          mWriter_LineSize += stream->Write(ev, buf, size+1); // '('

          if ( atom->AliasYarn(&yarn) )
          {
            mork_scope atomScope = atom->GetBookAtomSpaceScope(ev);
            if ( atomScope && atomScope != mWriter_DictAtomScope )
              this->ChangeDictAtomScope(ev, atomScope);
            
            if ( mWriter_DidStartDict && yarn.mYarn_Form != mWriter_DictForm )
              this->ChangeDictForm(ev, yarn.mYarn_Form);  
      
            mork_size pending = yarn.mYarn_Fill + morkWriter_kYarnEscapeSlop + 1;
            this->IndentOverMaxLine(ev, pending, morkWriter_kDictAliasValueDepth);
              
            stream->Putc(ev, '='); // start value
            ++mWriter_LineSize;
      
            this->WriteYarn(ev, &yarn);

            stream->Putc(ev, ')'); // end value
            ++mWriter_LineSize;
          }
          else
            atom->BadAtomKindError(ev);
                      
          ++mWriter_DoneCount;
        }
      }
    }
  }
  return ev->Good();
}

mork_bool
morkWriter::IsYarnAllValue(const mdbYarn* inYarn)
{
  mork_fill fill = inYarn->mYarn_Fill;
  const mork_u1* buf = (const mork_u1*) inYarn->mYarn_Buf;
  const mork_u1* end = buf + fill;
  --buf; // prepare for preincrement
  while ( ++buf < end )
  {
    mork_ch c = *buf;
    if ( !morkCh_IsValue(c) )
      return morkBool_kFalse;
  }
  return morkBool_kTrue;
}

mork_bool
morkWriter::PutVerboseCell(morkEnv* ev, morkCell* ioCell, mork_bool inWithVal)
{
  morkStream* stream = mWriter_Stream;
  morkStore* store = mWriter_Store;

  mdbYarn* colYarn = &mWriter_ColYarn;
  
  morkAtom* atom = (inWithVal)? ioCell->GetAtom() : (morkAtom*) 0;
  
  mork_column col = ioCell->GetColumn();
  store->TokenToString(ev, col, colYarn);
  
  mdbYarn yarn; // to ref content inside atom
  atom->AliasYarn(&yarn); // works even when atom==nil
  
  if ( yarn.mYarn_Form != mWriter_RowForm )
    this->ChangeRowForm(ev, yarn.mYarn_Form);

  mork_size pending = yarn.mYarn_Fill + colYarn->mYarn_Fill +
     morkWriter_kYarnEscapeSlop + 3;
  this->IndentOverMaxLine(ev, pending, morkWriter_kRowCellDepth);

  stream->Putc(ev, '('); // start cell
  ++mWriter_LineSize;

  this->WriteYarn(ev, colYarn); // column
  
  pending = yarn.mYarn_Fill + morkWriter_kYarnEscapeSlop;
  this->IndentOverMaxLine(ev, pending, morkWriter_kRowCellValueDepth);
  stream->Putc(ev, '=');
  ++mWriter_LineSize;
  
  this->WriteYarn(ev, &yarn); // value
  
  stream->Putc(ev, ')'); // end cell
  ++mWriter_LineSize;

  return ev->Good();
}

mork_bool
morkWriter::PutVerboseRowCells(morkEnv* ev, morkRow* ioRow)
{
  morkCell* cells = ioRow->mRow_Cells;
  if ( cells )
  {

    morkCell* end = cells + ioRow->mRow_Length;
    --cells; // prepare for preincrement:
    while ( ++cells < end && ev->Good() )
    {
      // note we prefer to avoid writing cells here with no value:
      if ( cells->GetAtom() ) // does cell have any value?
        this->PutVerboseCell(ev, cells, /*inWithVal*/ morkBool_kTrue);
    }
  }
  return ev->Good();
}


mork_bool
morkWriter::PutCell(morkEnv* ev, morkCell* ioCell, mork_bool inWithVal)
{
  morkStream* stream = mWriter_Stream;
  char buf[ 128 ]; // buffer for staging hex ids
  char* idBuf = buf + 2; // where the id always starts
  buf[ 0 ] = '('; // we always start with open paren
  buf[ 1 ] = '^'; // column is always a hex ID
  
  mork_size colSize = 0; // the size of col hex ID
  
  morkAtom* atom = (inWithVal)? ioCell->GetAtom() : (morkAtom*) 0;
  
  mork_column col = ioCell->GetColumn();
  char* p = idBuf;
  colSize = ev->TokenAsHex(p, col);
  p += colSize;

  mdbYarn yarn; // to ref content inside atom
  atom->AliasYarn(&yarn); // works even when atom==nil
  
  if ( yarn.mYarn_Form != mWriter_RowForm )
    this->ChangeRowForm(ev, yarn.mYarn_Form);
  
  if ( atom && atom->IsBook() ) // is it possible to write atom ID?
  {
    this->IndentAsNeeded(ev, morkWriter_kRowCellDepth);
    *p++ = '^';
    morkBookAtom* ba = (morkBookAtom*) atom;

    mork_size valSize = ev->TokenAsHex(p, ba->mBookAtom_Id);
    mork_fill yarnFill = yarn.mYarn_Fill;
    mork_bool putImmYarn = ( yarnFill <= valSize );
    if ( putImmYarn )
      putImmYarn = this->IsYarnAllValue(&yarn);
    
    if ( putImmYarn ) // value no bigger than id?
    {
      p[ -1 ] = '='; // go back and clobber '^' with '=' instead
      if ( yarnFill )
      {
        MORK_MEMCPY(p, yarn.mYarn_Buf, yarnFill);
        p += yarnFill;
      }
      *p++ = ')';
      mork_size distance = (mork_size) (p - buf);
      mWriter_LineSize += stream->Write(ev, buf, distance);
    }
    else
    {
      p += valSize;
      *p = ')';
      mWriter_LineSize += stream->Write(ev, buf, colSize + valSize + 4);
    }

    if ( atom->IsAtomDirty() )
    {
      atom->SetAtomClean();
      ++mWriter_DoneCount;
    }
  }
  else // must write an anonymous atom
  {
    mork_size pending = yarn.mYarn_Fill + colSize +
      morkWriter_kYarnEscapeSlop + 2;
    this->IndentOverMaxLine(ev, pending, morkWriter_kRowCellDepth);

    mWriter_LineSize += stream->Write(ev, buf, colSize + 2);

    pending -= ( colSize + 2 );
    this->IndentOverMaxLine(ev, pending, morkWriter_kRowCellDepth);
    stream->Putc(ev, '=');
    ++mWriter_LineSize;
    
    this->WriteYarn(ev, &yarn);
    stream->Putc(ev, ')'); // end cell
    ++mWriter_LineSize;
  }
  return ev->Good();
}

mork_bool
morkWriter::PutRowCells(morkEnv* ev, morkRow* ioRow)
{
  morkCell* cells = ioRow->mRow_Cells;
  if ( cells )
  {
    morkCell* end = cells + ioRow->mRow_Length;
    --cells; // prepare for preincrement:
    while ( ++cells < end && ev->Good() )
    {
      // note we prefer to avoid writing cells here with no value:
      if ( cells->GetAtom() ) // does cell have any value?
        this->PutCell(ev, cells, /*inWithVal*/ morkBool_kTrue);
    }
  }
  return ev->Good();
}

mork_bool
morkWriter::PutRow(morkEnv* ev, morkRow* ioRow)
{
  if ( ioRow && ioRow->IsRow() )
  {
    mWriter_RowForm = mWriter_TableForm;

    morkStream* stream = mWriter_Stream;
    char buf[ 128 + 16 ]; // buffer for staging hex
    char* p = buf;
    mdbOid* roid = &ioRow->mRow_Oid;
    mork_size ridSize = 0;
    
    mork_scope tableScope = mWriter_TableRowScope;

    if ( ioRow->IsRowDirty() )
    {
      if ( mWriter_SuppressDirtyRowNewline || !mWriter_LineSize )
        mWriter_SuppressDirtyRowNewline = morkBool_kFalse;
      else
      {
        if ( tableScope ) // in a table?
          mWriter_LineSize = stream->PutIndent(ev, morkWriter_kRowDepth);
        else
          mWriter_LineSize = stream->PutIndent(ev, 0); // no indent
      }
      
//      mork_rid rid = roid->mOid_Id;
      *p++ = '['; // start row punct=1
      mork_size punctSize = (mWriter_BeVerbose) ? 9 : 1; // counting "[ /*r=*/ "
      
      mork_bool rowRewrite = ioRow->IsRowRewrite();
            
      if ( rowRewrite && mWriter_Incremental )
      {
        *p++ = '-';
        ++punctSize; // counting '-'
        ++mWriter_LineSize;
      }

      if ( tableScope && roid->mOid_Scope == tableScope )
        ridSize = ev->TokenAsHex(p, roid->mOid_Id);
      else
        ridSize = ev->OidAsHex(p, *roid);
      
      p += ridSize;
      
      if (mWriter_BeVerbose)
      {
        *p++ = ' '; // punct=2
        *p++ = '/'; // punct=3
        *p++ = '*'; // punct=4
        *p++ = 'r'; // punct=5
        *p++ = '='; // punct=6

        mork_size usesSize = ev->TokenAsHex(p, (mork_token) ioRow->mRow_GcUses);
        punctSize += usesSize;
        p += usesSize;
      
        *p++ = '*'; // punct=7
        *p++ = '/'; // punct=8
        *p++ = ' '; // punct=9
      }
      mWriter_LineSize += stream->Write(ev, buf, ridSize + punctSize);
      
      // special case situation where row puts exactly one column:
      if ( !rowRewrite && mWriter_Incremental && ioRow->HasRowDelta() )
      {
        mork_column col = ioRow->GetDeltaColumn();
        morkCell dummy(col, morkChange_kNil, (morkAtom*) 0);
        morkCell* cell = 0;
        
        mork_bool withVal = ( ioRow->GetDeltaChange() != morkChange_kCut );
        
        if ( withVal )
        {
          mork_pos cellPos = 0; // dummy pos
          cell = ioRow->GetCell(ev, col, &cellPos);
        }
        if ( !cell )
          cell = &dummy;
          
        if ( mWriter_BeVerbose )
          this->PutVerboseCell(ev, cell, withVal);
        else
          this->PutCell(ev, cell, withVal);
      }
      else // put entire row?
      {
        if ( mWriter_BeVerbose )
          this->PutVerboseRowCells(ev, ioRow); // write all, verbosely
        else
          this->PutRowCells(ev, ioRow); // write all, hex notation
      }
        
      stream->Putc(ev, ']'); // end row
      ++mWriter_LineSize;
    }
    else
    {
      this->IndentAsNeeded(ev, morkWriter_kRowDepth);

      if ( tableScope && roid->mOid_Scope == tableScope )
        ridSize = ev->TokenAsHex(p, roid->mOid_Id);
      else
        ridSize = ev->OidAsHex(p, *roid);

      mWriter_LineSize += stream->Write(ev, buf, ridSize);
      stream->Putc(ev, ' ');
      ++mWriter_LineSize;
    }

    ++mWriter_DoneCount;

    ioRow->SetRowClean(); // try to do this at the very last
  }
  else
    ioRow->NonRowTypeWarning(ev);
  
  return ev->Good();
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789


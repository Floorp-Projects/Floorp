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

#ifndef _MORKTHUMB_
#include "morkThumb.h"
#endif

#ifndef _ORKINTHUMB_
#include "orkinThumb.h"
#endif

#ifndef _MORKSTORE_
#include "morkStore.h"
#endif

#ifndef _MORKFILE_
#include "morkFile.h"
#endif

#ifndef _MORKWRITER_
#include "morkWriter.h"
#endif

#ifndef _MORKPARSER_
#include "morkParser.h"
#endif

#ifndef _MORKBUILDER_
#include "morkBuilder.h"
#endif

#ifndef _MORKFILE_
#include "morkFile.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkThumb::CloseMorkNode(morkEnv* ev) // CloseThumb() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseThumb(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkThumb::~morkThumb() // assert CloseThumb() executed earlier
{
  MORK_ASSERT(mThumb_Magic==0);
  MORK_ASSERT(mThumb_Store==0);
  MORK_ASSERT(mThumb_File==0);
}

/*public non-poly*/
morkThumb::morkThumb(morkEnv* ev,
  const morkUsage& inUsage, nsIMdbHeap* ioHeap,
  nsIMdbHeap* ioSlotHeap, mork_magic inMagic)
: morkObject(ev, inUsage, ioHeap, (morkHandle*) 0)
, mThumb_Magic( 0 )
, mThumb_Total( 0 )
, mThumb_Current( 0 )

, mThumb_Done( morkBool_kFalse )
, mThumb_Broken( morkBool_kFalse )
, mThumb_Seed( 0 )

, mThumb_Store( 0 )
, mThumb_File( 0 )
, mThumb_Writer( 0 )
, mThumb_Builder( 0 )
, mThumb_SourcePort( 0 )

, mThumb_DoCollect( morkBool_kFalse )
{
  if ( ev->Good() )
  {
    mThumb_Magic = inMagic;
    mNode_Derived = morkDerived_kThumb;
  }
}

/*public non-poly*/ void
morkThumb::CloseThumb(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      mThumb_Magic = 0;
      if ( mThumb_Builder && mThumb_Store )
        mThumb_Store->ForgetBuilder(ev);
      morkBuilder::SlotStrongBuilder((morkBuilder*) 0, ev, &mThumb_Builder);
      
      morkWriter::SlotStrongWriter((morkWriter*) 0, ev, &mThumb_Writer);
      morkFile::SlotStrongFile((morkFile*) 0, ev, &mThumb_File);
      morkStore::SlotStrongStore((morkStore*) 0, ev, &mThumb_Store);
      morkPort::SlotStrongPort((morkPort*) 0, ev, &mThumb_SourcePort);
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

/*static*/ void morkThumb::NonThumbTypeError(morkEnv* ev)
{
  ev->NewError("non morkThumb");
}

/*static*/ void morkThumb::UnsupportedThumbMagicError(morkEnv* ev)
{
  ev->NewError("unsupported mThumb_Magic");
}

/*static*/ void morkThumb::NilThumbStoreError(morkEnv* ev)
{
  ev->NewError("nil mThumb_Store");
}

/*static*/ void morkThumb::NilThumbFileError(morkEnv* ev)
{
  ev->NewError("nil mThumb_File");
}

/*static*/ void morkThumb::NilThumbWriterError(morkEnv* ev)
{
  ev->NewError("nil mThumb_Writer");
}

/*static*/ void morkThumb::NilThumbBuilderError(morkEnv* ev)
{
  ev->NewError("nil mThumb_Builder");
}

/*static*/ void morkThumb::NilThumbSourcePortError(morkEnv* ev)
{
  ev->NewError("nil mThumb_SourcePort");
}

nsIMdbThumb*
morkThumb::AcquireThumbHandle(morkEnv* ev)
{
  nsIMdbThumb* outThumb = 0;
  orkinThumb* t = (orkinThumb*) mObject_Handle;
  if ( t ) // have an old handle?
    t->AddStrongRef(ev->AsMdbEnv());
  else // need new handle?
  {
    t = orkinThumb::MakeThumb(ev, this);
    mObject_Handle = t;
  }
  if ( t )
    outThumb = t;
  return outThumb;
}

/*static*/ morkThumb*
morkThumb::Make_OpenFileStore(morkEnv* ev, nsIMdbHeap* ioHeap, 
  morkStore* ioStore)
{
  morkThumb* outThumb = 0;
  if ( ioHeap && ioStore )
  {
    morkFile* file = ioStore->mStore_File;
    if ( file )
    {
      mork_pos fileEof = file->Length(ev);
      if ( ev->Good() )
      {
        outThumb = new(*ioHeap, ev)
          morkThumb(ev, morkUsage::kHeap, ioHeap, ioHeap,
            morkThumb_kMagic_OpenFileStore);
            
        if ( outThumb )
        {
          morkBuilder* builder = ioStore->LazyGetBuilder(ev);
          if ( builder )
          {
            outThumb->mThumb_Total = fileEof;
            morkStore::SlotStrongStore(ioStore, ev, &outThumb->mThumb_Store);
            morkBuilder::SlotStrongBuilder(builder, ev,
              &outThumb->mThumb_Builder);
          }
        }
      }
    }
    else
      ioStore->NilStoreFileError(ev);
  }
  else
    ev->NilPointerError();
    
  return outThumb;
}


/*static*/ morkThumb*
morkThumb::Make_CompressCommit(morkEnv* ev, 
  nsIMdbHeap* ioHeap, morkStore* ioStore, mork_bool inDoCollect)
{
  morkThumb* outThumb = 0;
  if ( ioHeap && ioStore )
  {
    morkFile* file = ioStore->mStore_File;
    if ( file )
    {
      outThumb = new(*ioHeap, ev)
        morkThumb(ev, morkUsage::kHeap, ioHeap, ioHeap,
          morkThumb_kMagic_CompressCommit);
          
      if ( outThumb )
      {
        morkWriter* writer = new(*ioHeap, ev)
          morkWriter(ev, morkUsage::kHeap, ioHeap, ioStore, file, ioHeap);
        if ( writer )
        {
          writer->mWriter_NeedDirtyAll = morkBool_kTrue;
          outThumb->mThumb_DoCollect = inDoCollect;
          morkStore::SlotStrongStore(ioStore, ev, &outThumb->mThumb_Store);
          morkFile::SlotStrongFile(file, ev, &outThumb->mThumb_File);
          morkWriter::SlotStrongWriter(writer, ev, &outThumb->mThumb_Writer);
        }
      }
    }
    else
      ioStore->NilStoreFileError(ev);
  }
  else
    ev->NilPointerError();
    
  return outThumb;
}

// { ===== begin non-poly methods imitating nsIMdbThumb =====
void morkThumb::GetProgress(morkEnv* ev, mdb_count* outTotal,
  mdb_count* outCurrent, mdb_bool* outDone, mdb_bool* outBroken)
{
  if ( outTotal )
    *outTotal = mThumb_Total;
  if ( outCurrent )
    *outCurrent = mThumb_Current;
  if ( outDone )
    *outDone = mThumb_Done;
  if ( outBroken )
    *outBroken = mThumb_Broken;
}

void morkThumb::DoMore(morkEnv* ev, mdb_count* outTotal,
  mdb_count* outCurrent, mdb_bool* outDone, mdb_bool* outBroken)
{
  if ( !mThumb_Done && !mThumb_Broken )
  {
    switch ( mThumb_Magic )
    {
      case morkThumb_kMagic_OpenFilePort: // 1 /* factory method */
        this->DoMore_OpenFilePort(ev); break;

      case morkThumb_kMagic_OpenFileStore: // 2 /* factory method */
        this->DoMore_OpenFileStore(ev); break;

      case morkThumb_kMagic_ExportToFormat: // 3 /* port method */
        this->DoMore_ExportToFormat(ev); break;

      case morkThumb_kMagic_ImportContent: // 4 /* store method */
        this->DoMore_ImportContent(ev); break;

      case morkThumb_kMagic_LargeCommit: // 5 /* store method */
        this->DoMore_LargeCommit(ev); break;

      case morkThumb_kMagic_SessionCommit: // 6 /* store method */
        this->DoMore_SessionCommit(ev); break;

      case morkThumb_kMagic_CompressCommit: // 7 /* store method */
        this->DoMore_CompressCommit(ev); break;

      case morkThumb_kMagic_SearchManyColumns: // 8 /* table method */
        this->DoMore_SearchManyColumns(ev); break;

      case morkThumb_kMagic_NewSortColumn: // 9 /* table metho) */
        this->DoMore_NewSortColumn(ev); break;

      case morkThumb_kMagic_NewSortColumnWithCompare: // 10 /* table method */
        this->DoMore_NewSortColumnWithCompare(ev); break;

      case morkThumb_kMagic_CloneSortColumn: // 11 /* table method */
        this->DoMore_CloneSortColumn(ev); break;

      case morkThumb_kMagic_AddIndex: // 12 /* table method */
        this->DoMore_AddIndex(ev); break;

      case morkThumb_kMagic_CutIndex: // 13 /* table method */
        this->DoMore_CutIndex(ev); break;

      default:
        this->UnsupportedThumbMagicError(ev);
        break;
    }
  }
  if ( outTotal )
    *outTotal = mThumb_Total;
  if ( outCurrent )
    *outCurrent = mThumb_Current;
  if ( outDone )
    *outDone = mThumb_Done;
  if ( outBroken )
    *outBroken = mThumb_Broken;
}

void morkThumb::CancelAndBreakThumb(morkEnv* ev)
{
  mThumb_Broken = morkBool_kTrue;
}

// } ===== end non-poly methods imitating nsIMdbThumb =====

morkStore*
morkThumb::ThumbToOpenStore(morkEnv* ev)
// for orkinFactory::ThumbToOpenStore() after OpenFileStore()
{
  return mThumb_Store;
}

void morkThumb::DoMore_OpenFilePort(morkEnv* ev)
{
  this->UnsupportedThumbMagicError(ev);
}

void morkThumb::DoMore_OpenFileStore(morkEnv* ev)
{
  morkBuilder* builder = mThumb_Builder;
  if ( builder )
  {
    mork_pos pos = 0;
    builder->ParseMore(ev, &pos, &mThumb_Done, &mThumb_Broken);
    // mThumb_Total = builder->mBuilder_TotalCount;
    // mThumb_Current = builder->mBuilder_DoneCount;
    mThumb_Current = pos;
  }
  else
  {
    this->NilThumbBuilderError(ev);
    mThumb_Broken = morkBool_kTrue;
    mThumb_Done = morkBool_kTrue;
  }
}

void morkThumb::DoMore_ExportToFormat(morkEnv* ev)
{
  this->UnsupportedThumbMagicError(ev);
}

void morkThumb::DoMore_ImportContent(morkEnv* ev)
{
  this->UnsupportedThumbMagicError(ev);
}

void morkThumb::DoMore_LargeCommit(morkEnv* ev)
{
  this->UnsupportedThumbMagicError(ev);
}

void morkThumb::DoMore_SessionCommit(morkEnv* ev)
{
  this->UnsupportedThumbMagicError(ev);
}

void morkThumb::DoMore_CompressCommit(morkEnv* ev)
{
  morkWriter* writer = mThumb_Writer;
  if ( writer )
  {
    writer->WriteMore(ev);
    mThumb_Total = writer->mWriter_TotalCount;
    mThumb_Current = writer->mWriter_DoneCount;
    mThumb_Done = ( ev->Bad() || writer->IsWritingDone() );
    mThumb_Broken = ev->Bad();
  }
  else
  {
    this->NilThumbWriterError(ev);
    mThumb_Broken = morkBool_kTrue;
    mThumb_Done = morkBool_kTrue;
  }
}

void morkThumb::DoMore_SearchManyColumns(morkEnv* ev)
{
  this->UnsupportedThumbMagicError(ev);
}

void morkThumb::DoMore_NewSortColumn(morkEnv* ev)
{
  this->UnsupportedThumbMagicError(ev);
}

void morkThumb::DoMore_NewSortColumnWithCompare(morkEnv* ev)
{
  this->UnsupportedThumbMagicError(ev);
}

void morkThumb::DoMore_CloneSortColumn(morkEnv* ev)
{
  this->UnsupportedThumbMagicError(ev);
}

void morkThumb::DoMore_AddIndex(morkEnv* ev)
{
  this->UnsupportedThumbMagicError(ev);
}

void morkThumb::DoMore_CutIndex(morkEnv* ev)
{
  this->UnsupportedThumbMagicError(ev);
}


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

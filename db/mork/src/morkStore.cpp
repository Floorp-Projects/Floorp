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

#ifndef _MORKBLOB_
#include "morkBlob.h"
#endif

#ifndef _MORKMAP_
#include "morkMap.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _MORKSTORE_
#include "morkStore.h"
#endif

#ifndef _ORKINSTORE_
#include "orkinStore.h"
#endif

#ifndef _MORKFACTORY_
#include "morkFactory.h"
#endif

#ifndef _MORKNODEMAP_
#include "morkNodeMap.h"
#endif

#ifndef _MORKFILE_
#include "morkFile.h"
#endif

#ifndef _MORKBUILDER_
#include "morkBuilder.h"
#endif

#ifndef _MORKATOMSPACE_
#include "morkAtomSpace.h"
#endif

#ifndef _MORKSTREAM_
#include "morkStream.h"
#endif

#ifndef _MORKATOMSPACE_
#include "morkAtomSpace.h"
#endif

#ifndef _MORKROWSPACE_
#include "morkRowSpace.h"
#endif

#ifndef _MORKPORTTABLECURSOR_
#include "morkPortTableCursor.h"
#endif

#ifndef _MORKTABLE_
#include "morkTable.h"
#endif

#ifndef _MORKROWMAP_
#include "morkRowMap.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkPort::CloseMorkNode(morkEnv* ev) // ClosePort() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->ClosePort(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkPort::~morkPort() // assert ClosePort() executed earlier
{
  MORK_ASSERT(this->IsShutNode());
  MORK_ASSERT(mPort_Factory==0);
  MORK_ASSERT(mPort_Heap==0);
}

/*public non-poly*/
morkPort::morkPort(morkEnv* ev, const morkUsage& inUsage,
   nsIMdbHeap* ioNodeHeap, // the heap (if any) for this node instance
   morkFactory* inFactory, // the factory for this
   nsIMdbHeap* ioPortHeap  // the heap to hold all content in the port
   )
: morkObject(ev, inUsage, ioNodeHeap, (morkHandle*) 0)
, mPort_Env( ev )
, mPort_Factory( 0 )
, mPort_Heap( 0 )
{
  if ( ev->Good() )
  {
    if ( inFactory && ioPortHeap )
    {
      morkFactory::SlotWeakFactory(inFactory, ev, &mPort_Factory);
      nsIMdbHeap_SlotStrongHeap(ioPortHeap, ev, &mPort_Heap);
      if ( ev->Good() )
        mNode_Derived = morkDerived_kPort;
    }
    else
      ev->NilPointerError();
  }
}

/*public non-poly*/ void
morkPort::ClosePort(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      morkFactory::SlotWeakFactory((morkFactory*) 0, ev, &mPort_Factory);
      nsIMdbHeap_SlotStrongHeap((nsIMdbHeap*) 0, ev, &mPort_Heap);
      this->CloseObject(ev);
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


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

/*public virtual*/ void
morkStore::CloseMorkNode(morkEnv* ev) // ClosePort() only if open
{
  if ( this->IsOpenNode() )
  {
    this->MarkClosing();
    this->CloseStore(ev);
    this->MarkShut();
  }
}

/*public virtual*/
morkStore::~morkStore() // assert CloseStore() executed earlier
{
  MORK_ASSERT(this->IsShutNode());
  MORK_ASSERT(mStore_File==0);
  MORK_ASSERT(mStore_InStream==0);
  MORK_ASSERT(mStore_OutStream==0);
  MORK_ASSERT(mStore_Builder==0);
  MORK_ASSERT(mStore_OidAtomSpace==0);
  MORK_ASSERT(mStore_GroundAtomSpace==0);
  MORK_ASSERT(mStore_GroundColumnSpace==0);
  MORK_ASSERT(mStore_RowSpaces.IsShutNode());
  MORK_ASSERT(mStore_AtomSpaces.IsShutNode());
  MORK_ASSERT(mStore_Pool.IsShutNode());
}

/*public non-poly*/
morkStore::morkStore(morkEnv* ev, const morkUsage& inUsage,
     nsIMdbHeap* ioNodeHeap, // the heap (if any) for this node instance
     morkFactory* inFactory, // the factory for this
     nsIMdbHeap* ioPortHeap  // the heap to hold all content in the port
     )
: morkPort(ev, inUsage, ioNodeHeap, inFactory, ioPortHeap)
, mStore_File( 0 )
, mStore_InStream( 0 )
, mStore_OutStream( 0 )
, mStore_Builder( 0 )
, mStore_OidAtomSpace( 0 )
, mStore_GroundAtomSpace( 0 )
, mStore_GroundColumnSpace( 0 )
, mStore_RowSpaces(ev, morkUsage::kMember, (nsIMdbHeap*) 0, ioPortHeap)
, mStore_AtomSpaces(ev, morkUsage::kMember, (nsIMdbHeap*) 0, ioPortHeap)
, mStore_Pool(ev, morkUsage::kMember, (nsIMdbHeap*) 0, ioPortHeap)
{
  if ( ev->Good() )
  {
    mNode_Derived = morkDerived_kStore;
    
    if ( ev->Good() )
      mStore_CharsetToken = this->StringToToken(ev, "charset");

    if ( ev->Good() )
      mStore_AtomScopeToken = this->StringToToken(ev, "atomScope");

    if ( ev->Good() )
      mStore_RowScopeToken = this->StringToToken(ev, "rowScope");

    if ( ev->Good() )
      mStore_TableScopeToken = this->StringToToken(ev, "tableScope");

    if ( ev->Good() )
      mStore_ColumnScopeToken = this->StringToToken(ev, "columnScope");

    if ( ev->Good() )
      mStore_TableKindToken = this->StringToToken(ev, "tableKind");
  }
}

/*public non-poly*/ void
morkStore::CloseStore(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {
      morkFile* file = mStore_File;
      if ( file && file->IsOpenNode() )
        file->CloseMorkNode(ev);

      morkAtomSpace::SlotStrongAtomSpace((morkAtomSpace*) 0, ev,
        &mStore_OidAtomSpace);
      morkAtomSpace::SlotStrongAtomSpace((morkAtomSpace*) 0, ev,
        &mStore_GroundAtomSpace);
      morkAtomSpace::SlotStrongAtomSpace((morkAtomSpace*) 0, ev,
        &mStore_GroundColumnSpace);
      mStore_RowSpaces.CloseMorkNode(ev);
      mStore_AtomSpaces.CloseMorkNode(ev);
      morkBuilder::SlotStrongBuilder((morkBuilder*) 0, ev, &mStore_Builder);
      morkFile::SlotStrongFile((morkFile*) 0, ev, &mStore_File);
      morkStream::SlotStrongStream((morkStream*) 0, ev, &mStore_InStream);
      morkStream::SlotStrongStream((morkStream*) 0, ev, &mStore_OutStream);

      mStore_Pool.CloseMorkNode(ev);
      this->ClosePort(ev);
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

void
morkStore::RenumberAllCollectableContent(morkEnv* ev)
{
  // do nothing currently
}

nsIMdbStore*
morkStore::AcquireStoreHandle(morkEnv* ev)
{
  nsIMdbStore* outStore = 0;
  orkinStore* s = (orkinStore*) mObject_Handle;
  if ( s ) // have an old handle?
    s->AddStrongRef(ev->AsMdbEnv());
  else // need new handle?
  {
    s = orkinStore::MakeStore(ev, this);
    mObject_Handle = s;
  }
  if ( s )
    outStore = s;
  return outStore;
}


morkMaxBookAtom*
morkStore::StageYarnAsBookAtom(morkEnv* ev, const mdbYarn* inYarn,
   morkAtomSpace* ioSpace)
{
  if ( inYarn && inYarn->mYarn_Buf )
  {
    mork_size length = inYarn->mYarn_Fill;
    if ( length <= morkBookAtom_kMaxBodySize )
    {
      morkBuf buf(inYarn->mYarn_Buf, length);
      mork_aid dummyAid = 1;
      mStore_BookAtom.InitMaxBookAtom(ev, buf, 
        inYarn->mYarn_Form, ioSpace, dummyAid);
      return &mStore_BookAtom;
    }
  }
  else
    ev->NilPointerError();
    
  return (morkMaxBookAtom*) 0;
}

morkMaxBookAtom*
morkStore::StageStringAsBookAtom(morkEnv* ev, const char* inString,
   mork_cscode inForm, morkAtomSpace* ioSpace)
// StageStringAsBookAtom() returns &mStore_BookAtom if inString is small
// enough, such that strlen(inString) < morkBookAtom_kMaxBodySize.  And
// content inside mStore_BookAtom will be the valid atom format for
// inString. This method is the standard way to stage a string as an
// atom for searching or adding new atoms into an atom space hash table.
{
  if ( inString )
  {
    mork_size length = MORK_STRLEN(inString);
    if ( length <= morkBookAtom_kMaxBodySize )
    {
      morkBuf buf(inString, length);
      mork_aid dummyAid = 1;
      mStore_BookAtom.InitMaxBookAtom(ev, buf, inForm, ioSpace, dummyAid);
      return &mStore_BookAtom;
    }
  }
  else
    ev->NilPointerError();
    
  return (morkMaxBookAtom*) 0;
}

morkAtomSpace* morkStore::LazyGetOidAtomSpace(morkEnv* ev)
{
  if ( !mStore_OidAtomSpace )
  {
  }
  return mStore_OidAtomSpace;
}

morkAtomSpace* morkStore::LazyGetGroundAtomSpace(morkEnv* ev)
{
  if ( !mStore_GroundAtomSpace )
  {
    mork_scope atomScope = morkStore_kGroundAtomSpace;
    nsIMdbHeap* heap = mPort_Heap;
    morkAtomSpace* space = new(*heap, ev) 
      morkAtomSpace(ev, morkUsage::kHeap, atomScope, this, heap, heap);
      
    if ( space ) // successful space creation?
    {
      mStore_GroundAtomSpace = space; // transfer strong ref to this slot
      mStore_AtomSpaces.AddAtomSpace(ev, space);
    }
  }
  return mStore_GroundAtomSpace;
}

morkAtomSpace* morkStore::LazyGetGroundColumnSpace(morkEnv* ev)
{
  if ( !mStore_GroundColumnSpace )
  {
    mork_scope atomScope = morkStore_kGroundColumnSpace;
    nsIMdbHeap* heap = mPort_Heap;
    morkAtomSpace* space = new(*heap, ev) 
      morkAtomSpace(ev, morkUsage::kHeap, atomScope, this, heap, heap);
      
    if ( space ) // successful space creation?
    {
      mStore_GroundColumnSpace = space; // transfer strong ref to this slot
      mStore_AtomSpaces.AddAtomSpace(ev, space);
    }
  }
  return mStore_GroundColumnSpace;
}

morkStream* morkStore::LazyGetInStream(morkEnv* ev)
{
  if ( !mStore_InStream )
  {
    morkFile* file = mStore_File;
    if ( file )
    {
      morkStream* stream = new(*mPort_Heap, ev) 
        morkStream(ev, morkUsage::kHeap, mPort_Heap, file,
          morkStore_kStreamBufSize, /*frozen*/ morkBool_kTrue);
      if ( stream )
      {
        mStore_InStream = stream; // transfer strong ref to this slot
      }
    }
    else
      this->NilStoreFileError(ev);
  }
  return mStore_InStream;
}

morkStream* morkStore::LazyGetOutStream(morkEnv* ev)
{
  if ( !mStore_OutStream )
  {
    morkFile* file = mStore_File;
    if ( file )
    {
      morkStream* stream = new(*mPort_Heap, ev) 
        morkStream(ev, morkUsage::kHeap, mPort_Heap, file,
          morkStore_kStreamBufSize, /*frozen*/ morkBool_kFalse);
      if ( stream )
      {
        mStore_InStream = stream; // transfer strong ref to this slot
      }
    }
    else
      this->NilStoreFileError(ev);
  }
  return mStore_OutStream;
}

morkBuilder* morkStore::LazyGetBuilder(morkEnv* ev)
{
  if ( !mStore_Builder )
  {
    morkStream* stream = this->LazyGetInStream(ev);
    if ( stream )
    {
      nsIMdbHeap* heap = mPort_Heap;
      morkBuilder* builder = new(*heap, ev) 
        morkBuilder(ev, morkUsage::kHeap, heap, stream,
          morkBuilder_kDefaultBytesPerParseSegment, heap, this);
      if ( builder )
      {
        mStore_Builder = builder; // transfer strong ref to this slot
      }
    }
  }
  return mStore_Builder;
}

morkRowSpace*
morkStore::LazyGetRowSpace(morkEnv* ev, mdb_scope inRowScope)
{
  morkRowSpace* outSpace = mStore_RowSpaces.GetRowSpace(ev, inRowScope);
  if ( !outSpace && ev->Good() ) // try to make new space?
  {
    nsIMdbHeap* heap = mPort_Heap;
    outSpace = new(*heap, ev) 
      morkRowSpace(ev, morkUsage::kHeap, inRowScope, this, heap, heap);
      
    if ( outSpace ) // successful space creation?
    {
      // note adding to node map creates it's own strong ref...
      if ( mStore_RowSpaces.AddRowSpace(ev, outSpace) )
        outSpace->CutStrongRef(ev); // ...so we can drop our strong ref
    }
  }
  return outSpace;
}

morkAtomSpace*
morkStore::LazyGetAtomSpace(morkEnv* ev, mdb_scope inAtomScope)
{
  morkAtomSpace* outSpace = mStore_AtomSpaces.GetAtomSpace(ev, inAtomScope);
  if ( !outSpace && ev->Good() ) // try to make new space?
  {
    if ( inAtomScope == morkStore_kGroundAtomSpace )
      outSpace = this->LazyGetGroundAtomSpace(ev);
      
    else if ( inAtomScope == morkStore_kGroundColumnSpace )
      outSpace = this->LazyGetGroundColumnSpace(ev);
    else
    {
      nsIMdbHeap* heap = mPort_Heap;
      outSpace = new(*heap, ev) 
        morkAtomSpace(ev, morkUsage::kHeap, inAtomScope, this, heap, heap);
        
      if ( outSpace ) // successful space creation?
      {
        // note adding to node map creates it's own strong ref...
        if ( mStore_AtomSpaces.AddAtomSpace(ev, outSpace) )
          outSpace->CutStrongRef(ev); // ...so we can drop our strong ref
      }
    }
  }
  return outSpace;
}

/*static*/ void
morkStore::NonStoreTypeError(morkEnv* ev)
{
  ev->NewError("non morkStore");
}

/*static*/ void
morkStore::NilStoreFileError(morkEnv* ev)
{
  ev->NewError("nil mStore_File");
}


mork_bool
morkStore::OpenStoreFile(morkEnv* ev, mork_bool inFrozen,
    const char* inFilePath, const mdbOpenPolicy* inOpenPolicy)
{
  morkFile::SlotStrongFile((morkFile*) 0, ev, &mStore_File);
  if ( ev->Good() )
  {
    morkFile* file = morkFile::OpenOldFile(ev, mPort_Heap,
      inFilePath, inFrozen);
    if ( file )
    {
      if ( ev->Good() )
        morkFile::SlotStrongFile(file, ev, &mStore_File);
      else
        file->CutStrongRef(ev);
    }
  }
  return ev->Good();
}

mork_bool
morkStore::CreateStoreFile(morkEnv* ev,
    const char* inFilePath, const mdbOpenPolicy* inOpenPolicy)
{
  morkFile::SlotStrongFile((morkFile*) 0, ev, &mStore_File);
  if ( ev->Good() )
  {
    morkFile* file = morkFile::CreateNewFile(ev, mPort_Heap,
      inFilePath);
    if ( file )
    {
      if ( ev->Good() )
        morkFile::SlotStrongFile(file, ev, &mStore_File);
      else
        file->CutStrongRef(ev);
    }
  }
  return ev->Good();
}
  
morkAtom*
morkStore::YarnToAtom(morkEnv* ev, const mdbYarn* inYarn)
{
  morkAtom* outAtom = 0;
  if ( ev->Good() )
  {
    morkAtomSpace* groundSpace = this->LazyGetGroundAtomSpace(ev);
    if ( groundSpace )
    {
      morkMaxBookAtom* keyAtom =
        this->StageYarnAsBookAtom(ev, inYarn, groundSpace);
        
      if ( keyAtom )
      {
        morkAtomBodyMap* map = &groundSpace->mAtomSpace_AtomBodies;
        outAtom = map->GetAtom(ev, keyAtom);
        if ( !outAtom )
          outAtom = groundSpace->MakeBookAtomCopy(ev, *keyAtom);
      }
      else if ( ev->Good() )
      {
        morkBuf buf(inYarn->mYarn_Buf, inYarn->mYarn_Fill);
        outAtom = mStore_Pool.NewAnonAtom(ev, buf, inYarn->mYarn_Form);
      }
    }
  }
  return outAtom;
}

// mork_bool
// morkStore::CutBookAtom(morkEnv* ev, morkBookAtom* ioAtom)
// {
// }

void
morkStore::TokenToString(morkEnv* ev, mdb_token inToken, mdbYarn* outTokenName)
{
  if ( inToken > morkAtomSpace_kMaxSevenBitAid )
  {
    morkBookAtom* atom = 0;
    morkAtomSpace* space = mStore_GroundColumnSpace;
    if ( space )
      atom = space->mAtomSpace_AtomAids.GetAid(ev, (mork_aid) inToken);
      
    atom->GetYarn(outTokenName); // note this is safe even when atom==nil
  }
  else // token is an "immediate" single byte string representation?
  {
    mdbYarn* y = outTokenName;
    if ( y->mYarn_Buf && y->mYarn_Size ) // any space in yarn at all?
    {
      mork_u1* buf = (mork_u1*) y->mYarn_Buf; // for byte arithmetic
      buf[ 0 ] = (mork_u1) inToken; // write the single byte
      y->mYarn_Fill = 1;
      y->mYarn_More = 0;
    }
    else // just record we could not write the single byte
    {
      y->mYarn_More = 1;
      y->mYarn_Fill = 0;
    }
  }
}

mork_token
morkStore::StringToToken(morkEnv* ev, const char* inTokenName)
{
  mork_token outToken = 0;
  if ( ev->Good() )
  {
    const mork_u1* s = (const mork_u1*) inTokenName;
    mork_bool nonAscii = ( *s > 0x7F );
    if ( nonAscii || ( *s && s[ 1 ] ) ) // more than one byte?
    {
      mork_cscode form = 0; // default charset
      morkAtomSpace* groundSpace = this->LazyGetGroundColumnSpace(ev);
      if ( groundSpace )
      {
        morkMaxBookAtom* keyAtom =
          this->StageStringAsBookAtom(ev, inTokenName, form, groundSpace);
        if ( keyAtom )
        {
          morkAtomBodyMap* map = &groundSpace->mAtomSpace_AtomBodies;
          morkBookAtom* bookAtom = map->GetAtom(ev, keyAtom);
          if ( bookAtom )
            outToken = bookAtom->mBookAtom_Id;
          else
          {
            bookAtom = groundSpace->MakeBookAtomCopy(ev, *keyAtom);
            if ( bookAtom )
            {
              outToken = bookAtom->mBookAtom_Id;
              bookAtom->MakeCellUseForever(ev);
            }
          }
        }
      }
    }
    else // only a single byte in inTokenName string:
      outToken = *s;
  }
  
  return outToken;
}

mork_token
morkStore::QueryToken(morkEnv* ev, const char* inTokenName)
{
  mork_token outToken = 0;
  if ( ev->Good() )
  {
    const mork_u1* s = (const mork_u1*) inTokenName;
    mork_bool nonAscii = ( *s > 0x7F );
    if ( nonAscii || ( *s && s[ 1 ] ) ) // more than one byte?
    {
      mork_cscode form = 0; // default charset
      morkAtomSpace* groundSpace = this->LazyGetGroundColumnSpace(ev);
      if ( groundSpace )
      {
        morkMaxBookAtom* keyAtom =
          this->StageStringAsBookAtom(ev, inTokenName, form, groundSpace);
        if ( keyAtom )
        {
          morkAtomBodyMap* map = &groundSpace->mAtomSpace_AtomBodies;
          morkBookAtom* bookAtom = map->GetAtom(ev, keyAtom);
          if ( bookAtom )
          {
            outToken = bookAtom->mBookAtom_Id;
            bookAtom->MakeCellUseForever(ev);
          }
        }
      }
    }
    else // only a single byte in inTokenName string:
      outToken = *s;
  }
  
  return outToken;
}

mork_bool
morkStore::HasTableKind(morkEnv* ev, mdb_scope inRowScope, 
  mdb_kind inTableKind, mdb_count* outTableCount)
{
  mork_bool outBool = morkBool_kFalse;
  
  return outBool;
}

morkTable*
morkStore::GetTableKind(morkEnv* ev, mdb_scope inRowScope, 
  mdb_kind inTableKind, mdb_count* outTableCount,
  mdb_bool* outMustBeUnique)
{
  morkTable* outTable = 0;
  if ( ev->Good() )
  {
    morkRowSpace* rowSpace = this->LazyGetRowSpace(ev, inRowScope);
    if ( rowSpace )
    {
      outTable = rowSpace->FindTableByKind(ev, inTableKind);
      if ( outTable )
      {
        if ( outTableCount )
          *outTableCount = outTable->GetRowCount();
        if ( outMustBeUnique )
          *outMustBeUnique = outTable->mTable_MustBeUnique;
      }
    }
  }
  return outTable;
}

morkRow*
morkStore::GetRow(morkEnv* ev, const mdbOid* inOid)
{
  morkRow* outRow = 0;
  if ( ev->Good() )
  {
    morkRowSpace* rowSpace = this->LazyGetRowSpace(ev, inOid->mOid_Scope);
    if ( rowSpace )
    {
      outRow = rowSpace->mRowSpace_Rows.GetOid(ev, inOid);
    }
  }
  return outRow;
}

morkTable*
morkStore::GetTable(morkEnv* ev, const mdbOid* inOid)
{
  morkTable* outTable = 0;
  if ( ev->Good() )
  {
    morkRowSpace* rowSpace = this->LazyGetRowSpace(ev, inOid->mOid_Scope);
    if ( rowSpace )
    {
      outTable = rowSpace->FindTableByTid(ev, inOid->mOid_Id);
    }
  }
  return outTable;
}
  
morkTable*
morkStore::NewTable(morkEnv* ev, mdb_scope inRowScope,
  mdb_kind inTableKind, mdb_bool inMustBeUnique)
{
  morkTable* outTable = 0;
  if ( ev->Good() )
  {
    morkRowSpace* rowSpace = this->LazyGetRowSpace(ev, inRowScope);
    if ( rowSpace )
      outTable = rowSpace->NewTable(ev, inTableKind, inMustBeUnique);
  }
  return outTable;
}

morkPortTableCursor*
morkStore::GetPortTableCursor(morkEnv* ev, mdb_scope inRowScope,
  mdb_kind inTableKind)
{
  morkPortTableCursor* outCursor = 0;
  if ( ev->Good() )
  {
    nsIMdbHeap* heap = mPort_Heap;
    outCursor = new(*heap, ev) 
      morkPortTableCursor(ev, morkUsage::kHeap, heap, this,
        inRowScope, inTableKind, heap);
  }
  return outCursor;
}

morkRow*
morkStore::NewRowWithOid(morkEnv* ev, const mdbOid* inOid)
{
  morkRow* outRow = 0;
  if ( ev->Good() )
  {
    morkRowSpace* rowSpace = this->LazyGetRowSpace(ev, inOid->mOid_Scope);
    if ( rowSpace )
      outRow = rowSpace->NewRowWithOid(ev, inOid);
  }
  return outRow;
}

morkRow*
morkStore::NewRow(morkEnv* ev, mdb_scope inRowScope)
{
  morkRow* outRow = 0;
  if ( ev->Good() )
  {
    morkRowSpace* rowSpace = this->LazyGetRowSpace(ev, inRowScope);
    if ( rowSpace )
      outRow = rowSpace->NewRow(ev);
  }
  return outRow;
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-  */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
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

#ifndef _MORKFACTORY_
#include "morkFactory.h"
#endif

#ifndef _MORKNODEMAP_
#include "morkNodeMap.h"
#endif

#ifndef _MORKROW_
#include "morkRow.h"
#endif

#ifndef _MORKTHUMB_
#include "morkThumb.h"
#endif
// #ifndef _MORKFILE_
// #include "morkFile.h"
// #endif

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

#ifndef _MORKPARSER_
#include "morkParser.h"
#endif

#include "nsCOMPtr.h"

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

// ````` ````` ````` ````` ````` 
// { ===== begin morkNode interface =====

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

/*public non-poly*/ void
morkStore::ClosePort(morkEnv* ev) // called by CloseMorkNode();
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

/*public virtual*/
morkStore::~morkStore() // assert CloseStore() executed earlier
{
  MOZ_COUNT_DTOR(morkStore);
  if (IsOpenNode())
    CloseMorkNode(mMorkEnv);
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
: morkObject(ev, inUsage, ioNodeHeap, morkColor_kNone, (morkHandle*) 0)
, mPort_Env( ev )
, mPort_Factory( 0 )
, mPort_Heap( 0 )
, mStore_OidAtomSpace( 0 )
, mStore_GroundAtomSpace( 0 )
, mStore_GroundColumnSpace( 0 )

, mStore_File( 0 )
, mStore_InStream( 0 )
, mStore_Builder( 0 )
, mStore_OutStream( 0 )

, mStore_RowSpaces(ev, morkUsage::kMember, (nsIMdbHeap*) 0, ioPortHeap)
, mStore_AtomSpaces(ev, morkUsage::kMember, (nsIMdbHeap*) 0, ioPortHeap)
, mStore_Zone(ev, morkUsage::kMember, (nsIMdbHeap*) 0, ioPortHeap)
, mStore_Pool(ev, morkUsage::kMember, (nsIMdbHeap*) 0, ioPortHeap)

, mStore_CommitGroupIdentity( 0 )

, mStore_FirstCommitGroupPos( 0 )
, mStore_SecondCommitGroupPos( 0 )

// disable auto-assignment of atom IDs until someone knows it is okay:
, mStore_CanAutoAssignAtomIdentity( morkBool_kFalse )
, mStore_CanDirty( morkBool_kFalse ) // not until the store is open
, mStore_CanWriteIncremental( morkBool_kTrue ) // always with few exceptions
{
  MOZ_COUNT_CTOR(morkStore);
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
  if ( ev->Good() )
  {
    mNode_Derived = morkDerived_kStore;
    
  }
}

NS_IMPL_ISUPPORTS_INHERITED1(morkStore, morkObject, nsIMdbStore)

/*public non-poly*/ void
morkStore::CloseStore(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {

      nsIMdbFile* file = mStore_File;
      file->AddRef();

      morkFactory::SlotWeakFactory((morkFactory*) 0, ev, &mPort_Factory);
      nsIMdbHeap_SlotStrongHeap((nsIMdbHeap*) 0, ev, &mPort_Heap);
      morkAtomSpace::SlotStrongAtomSpace((morkAtomSpace*) 0, ev,
        &mStore_OidAtomSpace);
      morkAtomSpace::SlotStrongAtomSpace((morkAtomSpace*) 0, ev,
        &mStore_GroundAtomSpace);
      morkAtomSpace::SlotStrongAtomSpace((morkAtomSpace*) 0, ev,
        &mStore_GroundColumnSpace);
      mStore_RowSpaces.CloseMorkNode(ev);
      mStore_AtomSpaces.CloseMorkNode(ev);
      morkBuilder::SlotStrongBuilder((morkBuilder*) 0, ev, &mStore_Builder);
      
      nsIMdbFile_SlotStrongFile((nsIMdbFile*) 0, ev,
        &mStore_File);
      
      file->Release();

      morkStream::SlotStrongStream((morkStream*) 0, ev, &mStore_InStream);
      morkStream::SlotStrongStream((morkStream*) 0, ev, &mStore_OutStream);

      mStore_Pool.CloseMorkNode(ev);
      mStore_Zone.CloseMorkNode(ev);
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


mork_bool morkStore::DoPreferLargeOverCompressCommit(morkEnv* ev)
  // true when mStore_CanWriteIncremental && store has file large enough 
{
  nsIMdbFile* file = mStore_File;
  if ( file && mStore_CanWriteIncremental )
  {
    mdb_pos fileEof = 0;
    file->Eof(ev->AsMdbEnv(), &fileEof);
    if ( ev->Good() && fileEof > 128 )
      return morkBool_kTrue;
  }
  return morkBool_kFalse;
}

mork_percent morkStore::PercentOfStoreWasted(morkEnv* ev)
{
  mork_percent outPercent = 0;
  nsIMdbFile* file = mStore_File;
  
  if ( file )
  {
    mork_pos firstPos = mStore_FirstCommitGroupPos;
    mork_pos secondPos = mStore_SecondCommitGroupPos;
    if ( firstPos || secondPos )
    {
      if ( firstPos < 512 && secondPos > firstPos )
        firstPos = secondPos; // better approximation of first commit
        
      mork_pos fileLength = 0;
      file->Eof(ev->AsMdbEnv(), &fileLength); // end of file
      if ( ev->Good() && fileLength > firstPos )
      {
        mork_size groupContent = fileLength - firstPos;
        outPercent = ( groupContent * 100 ) / fileLength;
      }
    }
  }
  else
    this->NilStoreFileError(ev);
    
  return outPercent;
}

void
morkStore::SetStoreAndAllSpacesCanDirty(morkEnv* ev, mork_bool inCanDirty)
{
  mStore_CanDirty = inCanDirty;
  
  mork_change* c = 0;
  mork_scope* key = 0; // ignore keys in maps

  if ( ev->Good() )
  {
    morkAtomSpaceMapIter asi(ev, &mStore_AtomSpaces);

    morkAtomSpace* atomSpace = 0; // old val node in the map
    
    for ( c = asi.FirstAtomSpace(ev, key, &atomSpace); c && ev->Good();
          c = asi.NextAtomSpace(ev, key, &atomSpace) )
    {
      if ( atomSpace )
      {
        if ( atomSpace->IsAtomSpace() )
          atomSpace->mSpace_CanDirty = inCanDirty;
        else
          atomSpace->NonAtomSpaceTypeError(ev);
      }
      else
        ev->NilPointerError();
    }
  }

  if ( ev->Good() )
  {
    morkRowSpaceMapIter rsi(ev, &mStore_RowSpaces);
    morkRowSpace* rowSpace = 0; // old val node in the map
    
    for ( c = rsi.FirstRowSpace(ev, key, &rowSpace); c && ev->Good();
          c = rsi.NextRowSpace(ev, key, &rowSpace) )
    {
      if ( rowSpace )
      {
        if ( rowSpace->IsRowSpace() )
          rowSpace->mSpace_CanDirty = inCanDirty;
        else
          rowSpace->NonRowSpaceTypeError(ev);
      }
    }
  }
}

void
morkStore::RenumberAllCollectableContent(morkEnv* ev)
{
  MORK_USED_1(ev);
  // do nothing currently
}

nsIMdbStore*
morkStore::AcquireStoreHandle(morkEnv* ev)
{
  return this;
}


morkFarBookAtom*
morkStore::StageAliasAsFarBookAtom(morkEnv* ev, const morkMid* inMid,
   morkAtomSpace* ioSpace, mork_cscode inForm)
{
  if ( inMid && inMid->mMid_Buf )
  {
    const morkBuf* buf = inMid->mMid_Buf;
    mork_size length = buf->mBuf_Fill;
    if ( length <= morkBookAtom_kMaxBodySize )
    {
      mork_aid dummyAid = 1;
      //mStore_BookAtom.InitMaxBookAtom(ev, *buf, 
      //  inForm, ioSpace, dummyAid);
       
      mStore_FarBookAtom.InitFarBookAtom(ev, *buf, 
        inForm, ioSpace, dummyAid);
      return &mStore_FarBookAtom;
    }
  }
  else
    ev->NilPointerError();
    
  return (morkFarBookAtom*) 0;
}

morkFarBookAtom*
morkStore::StageYarnAsFarBookAtom(morkEnv* ev, const mdbYarn* inYarn,
   morkAtomSpace* ioSpace)
{
  if ( inYarn && inYarn->mYarn_Buf )
  {
    mork_size length = inYarn->mYarn_Fill;
    if ( length <= morkBookAtom_kMaxBodySize )
    {
      morkBuf buf(inYarn->mYarn_Buf, length);
      mork_aid dummyAid = 1;
      //mStore_BookAtom.InitMaxBookAtom(ev, buf, 
      //  inYarn->mYarn_Form, ioSpace, dummyAid);
      mStore_FarBookAtom.InitFarBookAtom(ev, buf, 
        inYarn->mYarn_Form, ioSpace, dummyAid);
      return &mStore_FarBookAtom;
    }
  }
  else
    ev->NilPointerError();
    
  return (morkFarBookAtom*) 0;
}

morkFarBookAtom*
morkStore::StageStringAsFarBookAtom(morkEnv* ev, const char* inString,
   mork_cscode inForm, morkAtomSpace* ioSpace)
{
  if ( inString )
  {
    mork_size length = MORK_STRLEN(inString);
    if ( length <= morkBookAtom_kMaxBodySize )
    {
      morkBuf buf(inString, length);
      mork_aid dummyAid = 1;
      //mStore_BookAtom.InitMaxBookAtom(ev, buf, inForm, ioSpace, dummyAid);
      mStore_FarBookAtom.InitFarBookAtom(ev, buf, inForm, ioSpace, dummyAid);
      return &mStore_FarBookAtom;
    }
  }
  else
    ev->NilPointerError();
    
  return (morkFarBookAtom*) 0;
}

morkAtomSpace* morkStore::LazyGetOidAtomSpace(morkEnv* ev)
{
  MORK_USED_1(ev);
  if ( !mStore_OidAtomSpace )
  {
  }
  return mStore_OidAtomSpace;
}

morkAtomSpace* morkStore::LazyGetGroundAtomSpace(morkEnv* ev)
{
  if ( !mStore_GroundAtomSpace )
  {
    mork_scope atomScope = morkStore_kValueSpaceScope;
    nsIMdbHeap* heap = mPort_Heap;
    morkAtomSpace* space = new(*heap, ev) 
      morkAtomSpace(ev, morkUsage::kHeap, atomScope, this, heap, heap);
      
    if ( space ) // successful space creation?
    {
      this->MaybeDirtyStore();
    
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
      this->MaybeDirtyStore();
    
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
    nsIMdbFile* file = mStore_File;
    if ( file )
    {
      morkStream* stream = new(*mPort_Heap, ev) 
        morkStream(ev, morkUsage::kHeap, mPort_Heap, file,
          morkStore_kStreamBufSize, /*frozen*/ morkBool_kTrue);
      if ( stream )
      {
        this->MaybeDirtyStore();
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
    nsIMdbFile* file = mStore_File;
    if ( file )
    {
      morkStream* stream = new(*mPort_Heap, ev) 
        morkStream(ev, morkUsage::kHeap, mPort_Heap, file,
          morkStore_kStreamBufSize, /*frozen*/ morkBool_kFalse);
      if ( stream )
      {
        this->MaybeDirtyStore();
        mStore_InStream = stream; // transfer strong ref to this slot
      }
    }
    else
      this->NilStoreFileError(ev);
  }
  return mStore_OutStream;
}

void
morkStore::ForgetBuilder(morkEnv* ev)
{
  if ( mStore_Builder )
    morkBuilder::SlotStrongBuilder((morkBuilder*) 0, ev, &mStore_Builder);
  if ( mStore_InStream )
    morkStream::SlotStrongStream((morkStream*) 0, ev, &mStore_InStream);
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
      this->MaybeDirtyStore();
    
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
    if ( inAtomScope == morkStore_kValueSpaceScope )
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
        this->MaybeDirtyStore();
    
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

/*static*/ void
morkStore::CannotAutoAssignAtomIdentityError(morkEnv* ev)
{
  ev->NewError("false mStore_CanAutoAssignAtomIdentity");
}


mork_bool
morkStore::OpenStoreFile(morkEnv* ev, mork_bool inFrozen,
    // const char* inFilePath,
    nsIMdbFile* ioFile, // db abstract file interface
    const mdbOpenPolicy* inOpenPolicy)
{
  MORK_USED_2(inOpenPolicy,inFrozen);
  nsIMdbFile_SlotStrongFile(ioFile, ev, &mStore_File);
  
  // if ( ev->Good() )
  // {
  //   morkFile* file = morkFile::OpenOldFile(ev, mPort_Heap,
  //     inFilePath, inFrozen);
  //   if ( ioFile )
  //   {
  //     if ( ev->Good() )
  //       morkFile::SlotStrongFile(file, ev, &mStore_File);
  //     else
  //       file->CutStrongRef(ev);
  //       
  //   }
  // }
  return ev->Good();
}

mork_bool
morkStore::CreateStoreFile(morkEnv* ev,
    // const char* inFilePath,
    nsIMdbFile* ioFile, // db abstract file interface
    const mdbOpenPolicy* inOpenPolicy)
{
  MORK_USED_1(inOpenPolicy);
  nsIMdbFile_SlotStrongFile(ioFile, ev, &mStore_File);
  
  return ev->Good();
}

morkAtom*
morkStore::CopyAtom(morkEnv* ev, const morkAtom* inAtom)
// copy inAtom (from some other store) over to this store
{
  morkAtom* outAtom = 0;
  if ( inAtom )
  {
    mdbYarn yarn;
    if ( inAtom->AliasYarn(&yarn) )
      outAtom = this->YarnToAtom(ev, &yarn, PR_TRUE /* create */);
  }
  return outAtom;
}
 
morkAtom*
morkStore::YarnToAtom(morkEnv* ev, const mdbYarn* inYarn, PRBool createIfMissing /* = PR_TRUE */)
{
  morkAtom* outAtom = 0;
  if ( ev->Good() )
  {
    morkAtomSpace* groundSpace = this->LazyGetGroundAtomSpace(ev);
    if ( groundSpace )
    {
      morkFarBookAtom* keyAtom =
        this->StageYarnAsFarBookAtom(ev, inYarn, groundSpace);
        
      if ( keyAtom )
      {
        morkAtomBodyMap* map = &groundSpace->mAtomSpace_AtomBodies;
        outAtom = map->GetAtom(ev, keyAtom);
        if ( !outAtom && createIfMissing)
        {
          this->MaybeDirtyStore();
          outAtom = groundSpace->MakeBookAtomCopy(ev, *keyAtom);
        }
      }
      else if ( ev->Good() )
      {
        morkBuf b(inYarn->mYarn_Buf, inYarn->mYarn_Fill);
        morkZone* z = &mStore_Zone;
        outAtom = mStore_Pool.NewAnonAtom(ev, b, inYarn->mYarn_Form, z);
      }
    }
  }
  return outAtom;
}

mork_bool
morkStore::MidToOid(morkEnv* ev, const morkMid& inMid, mdbOid* outOid)
{
  *outOid = inMid.mMid_Oid;
  const morkBuf* buf = inMid.mMid_Buf;
  if ( buf && !outOid->mOid_Scope )
  {
    if ( buf->mBuf_Fill <= morkBookAtom_kMaxBodySize )
    {
      if ( buf->mBuf_Fill == 1 )
      {
        mork_u1* name = (mork_u1*) buf->mBuf_Body;
        if ( name )
        {
          outOid->mOid_Scope = (mork_scope) *name;
          return ev->Good();
        }
      }
      morkAtomSpace* groundSpace = this->LazyGetGroundColumnSpace(ev);
      if ( groundSpace )
      {
        mork_cscode form = 0; // default
        mork_aid aid = 1; // dummy
        mStore_FarBookAtom.InitFarBookAtom(ev, *buf, form, groundSpace, aid);
        morkFarBookAtom* keyAtom = &mStore_FarBookAtom;
        morkAtomBodyMap* map = &groundSpace->mAtomSpace_AtomBodies;
        morkBookAtom* bookAtom = map->GetAtom(ev, keyAtom);
        if ( bookAtom )
          outOid->mOid_Scope = bookAtom->mBookAtom_Id;
        else
        {
          this->MaybeDirtyStore();
          bookAtom = groundSpace->MakeBookAtomCopy(ev, *keyAtom);
          if ( bookAtom )
          {
            outOid->mOid_Scope = bookAtom->mBookAtom_Id;
            bookAtom->MakeCellUseForever(ev);
          }
        }
      }
    }
  }
  return ev->Good();
}

morkRow*
morkStore::MidToRow(morkEnv* ev, const morkMid& inMid)
{
  mdbOid tempOid;
  this->MidToOid(ev, inMid, &tempOid);
  return this->OidToRow(ev, &tempOid);
}

morkTable*
morkStore::MidToTable(morkEnv* ev, const morkMid& inMid)
{
  mdbOid tempOid;
  this->MidToOid(ev, inMid, &tempOid);
  return this->OidToTable(ev, &tempOid, /*metarow*/ (mdbOid*) 0);
}

mork_bool
morkStore::MidToYarn(morkEnv* ev, const morkMid& inMid, mdbYarn* outYarn)
{
  mdbOid tempOid;
  this->MidToOid(ev, inMid, &tempOid);
  return this->OidToYarn(ev, tempOid, outYarn);
}

mork_bool
morkStore::OidToYarn(morkEnv* ev, const mdbOid& inOid, mdbYarn* outYarn)
{
  morkBookAtom* atom = 0;
      
  morkAtomSpace* atomSpace = mStore_AtomSpaces.GetAtomSpace(ev, inOid.mOid_Scope);
  if ( atomSpace )
  {
    morkAtomAidMap* map = &atomSpace->mAtomSpace_AtomAids;
    atom = map->GetAid(ev, (mork_aid) inOid.mOid_Id);
  }
  atom->GetYarn(outYarn); // note this is safe even when atom==nil

  return ev->Good();
}

morkBookAtom*
morkStore::MidToAtom(morkEnv* ev, const morkMid& inMid)
{
  morkBookAtom* outAtom = 0;
  mdbOid oid;
  if ( this->MidToOid(ev, inMid, &oid) )
  {
    morkAtomSpace* atomSpace = mStore_AtomSpaces.GetAtomSpace(ev, oid.mOid_Scope);
    if ( atomSpace )
    {
      morkAtomAidMap* map = &atomSpace->mAtomSpace_AtomAids;
      outAtom = map->GetAid(ev, (mork_aid) oid.mOid_Id);
    }
  }
  return outAtom;
}

/*static*/ void
morkStore::SmallTokenToOneByteYarn(morkEnv* ev, mdb_token inToken,
  mdbYarn* outYarn)
{
  MORK_USED_1(ev);
  if ( outYarn->mYarn_Buf && outYarn->mYarn_Size ) // any space in yarn at all?
  {
    mork_u1* buf = (mork_u1*) outYarn->mYarn_Buf; // for byte arithmetic
    buf[ 0 ] = (mork_u1) inToken; // write the single byte
    outYarn->mYarn_Fill = 1;
    outYarn->mYarn_More = 0;
  }
  else // just record we could not write the single byte
  {
    outYarn->mYarn_More = 1;
    outYarn->mYarn_Fill = 0;
  }
}

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
    this->SmallTokenToOneByteYarn(ev, inToken, outTokenName);
}
  
// void
// morkStore::SyncTokenIdChange(morkEnv* ev, const morkBookAtom* inAtom,
//   const mdbOid* inOid)
// {
// mork_token   mStore_MorkNoneToken;    // token for "mork:none"   // fill=9
// mork_column  mStore_CharsetToken;     // token for "charset"     // fill=7
// mork_column  mStore_AtomScopeToken;   // token for "atomScope"   // fill=9
// mork_column  mStore_RowScopeToken;    // token for "rowScope"    // fill=8
// mork_column  mStore_TableScopeToken;  // token for "tableScope"  // fill=10
// mork_column  mStore_ColumnScopeToken; // token for "columnScope" // fill=11
// mork_kind    mStore_TableKindToken;   // token for "tableKind"   // fill=9
// ---------------------ruler-for-token-length-above---123456789012
// 
//   if ( inOid->mOid_Scope == morkStore_kColumnSpaceScope && inAtom->IsWeeBook() )
//   {
//     const mork_u1* body = ((const morkWeeBookAtom*) inAtom)->mWeeBookAtom_Body;
//     mork_size size = inAtom->mAtom_Size;
// 
//     if ( size >= 7 && size <= 11 )
//     {
//       if ( size == 9 )
//       {
//         if ( *body == 'm' )
//         {
//           if ( MORK_MEMCMP(body, "mork:none", 9) == 0 )
//             mStore_MorkNoneToken = inAtom->mBookAtom_Id;
//         }
//         else if ( *body == 'a' )
//         {
//           if ( MORK_MEMCMP(body, "atomScope", 9) == 0 )
//             mStore_AtomScopeToken = inAtom->mBookAtom_Id;
//         }
//         else if ( *body == 't' )
//         {
//           if ( MORK_MEMCMP(body, "tableKind", 9) == 0 )
//             mStore_TableKindToken = inAtom->mBookAtom_Id;
//         }
//       }
//       else if ( size == 7 && *body == 'c' )
//       {
//         if ( MORK_MEMCMP(body, "charset", 7) == 0 )
//           mStore_CharsetToken = inAtom->mBookAtom_Id;
//       }
//       else if ( size == 8 && *body == 'r' )
//       {
//         if ( MORK_MEMCMP(body, "rowScope", 8) == 0 )
//           mStore_RowScopeToken = inAtom->mBookAtom_Id;
//       }
//       else if ( size == 10 && *body == 't' )
//       {
//         if ( MORK_MEMCMP(body, "tableScope", 10) == 0 )
//           mStore_TableScopeToken = inAtom->mBookAtom_Id;
//       }
//       else if ( size == 11 && *body == 'c' )
//       {
//         if ( MORK_MEMCMP(body, "columnScope", 11) == 0 )
//           mStore_ColumnScopeToken = inAtom->mBookAtom_Id;
//       }
//     }
//   }
// }

morkAtom*
morkStore::AddAlias(morkEnv* ev, const morkMid& inMid, mork_cscode inForm)
{
  morkBookAtom* outAtom = 0;
  if ( ev->Good() )
  {
    const mdbOid* oid = &inMid.mMid_Oid;
    morkAtomSpace* atomSpace = this->LazyGetAtomSpace(ev, oid->mOid_Scope);
    if ( atomSpace )
    {
      morkFarBookAtom* keyAtom =
        this->StageAliasAsFarBookAtom(ev, &inMid, atomSpace, inForm);
      if ( keyAtom )
      {
         morkAtomAidMap* map = &atomSpace->mAtomSpace_AtomAids;
        outAtom = map->GetAid(ev, (mork_aid) oid->mOid_Id);
        if ( outAtom )
        {
          if ( !outAtom->EqualFormAndBody(ev, keyAtom) )
              ev->NewError("duplicate alias ID");
        }
        else
        {
          this->MaybeDirtyStore();
          keyAtom->mBookAtom_Id = oid->mOid_Id;
          outAtom = atomSpace->MakeBookAtomCopyWithAid(ev,
            *keyAtom, (mork_aid) oid->mOid_Id);
            
          // if ( outAtom && outAtom->IsWeeBook() )
          // {
          //   if ( oid->mOid_Scope == morkStore_kColumnSpaceScope )
          //   {
          //    mork_size size = outAtom->mAtom_Size;
          //     if ( size >= 7 && size <= 11 )
          //       this->SyncTokenIdChange(ev, outAtom, oid);
          //   }
          // }
        }
      }
    }
  }
  return outAtom;
}

#define morkStore_kMaxCopyTokenSize 512 /* if larger, cannot be copied */
  
mork_token
morkStore::CopyToken(morkEnv* ev, mdb_token inToken, morkStore* inStore)
// copy inToken from inStore over to this store
{
  mork_token outToken = 0;
  if ( inStore == this ) // same store?
    outToken = inToken; // just return token unchanged
  else
  {
    char yarnBuf[ morkStore_kMaxCopyTokenSize ];
    mdbYarn yarn;
    yarn.mYarn_Buf = yarnBuf;
    yarn.mYarn_Fill = 0;
    yarn.mYarn_Size = morkStore_kMaxCopyTokenSize;
    yarn.mYarn_More = 0;
    yarn.mYarn_Form = 0;
    yarn.mYarn_Grow = 0;
    
    inStore->TokenToString(ev, inToken, &yarn);
    if ( ev->Good() )
    {
      morkBuf buf(yarn.mYarn_Buf, yarn.mYarn_Fill);
      outToken = this->BufToToken(ev, &buf);
    }
  }
  return outToken;
}

mork_token
morkStore::BufToToken(morkEnv* ev, const morkBuf* inBuf)
{
  mork_token outToken = 0;
  if ( ev->Good() )
  {
    const mork_u1* s = (const mork_u1*) inBuf->mBuf_Body;
    mork_bool nonAscii = ( *s > 0x7F );
    mork_size length = inBuf->mBuf_Fill;
    if ( nonAscii || length > 1 ) // more than one byte?
    {
      mork_cscode form = 0; // default charset
      morkAtomSpace* space = this->LazyGetGroundColumnSpace(ev);
      if ( space )
      {
        morkFarBookAtom* keyAtom = 0;
        if ( length <= morkBookAtom_kMaxBodySize )
        {
          mork_aid aid = 1; // dummy
          //mStore_BookAtom.InitMaxBookAtom(ev, *inBuf, form, space, aid);
          mStore_FarBookAtom.InitFarBookAtom(ev, *inBuf, form, space, aid);
          keyAtom = &mStore_FarBookAtom;
        }
        if ( keyAtom )
        {
          morkAtomBodyMap* map = &space->mAtomSpace_AtomBodies;
          morkBookAtom* bookAtom = map->GetAtom(ev, keyAtom);
          if ( bookAtom )
            outToken = bookAtom->mBookAtom_Id;
          else
          {
            this->MaybeDirtyStore();
            bookAtom = space->MakeBookAtomCopy(ev, *keyAtom);
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
        morkFarBookAtom* keyAtom =
          this->StageStringAsFarBookAtom(ev, inTokenName, form, groundSpace);
        if ( keyAtom )
        {
          morkAtomBodyMap* map = &groundSpace->mAtomSpace_AtomBodies;
          morkBookAtom* bookAtom = map->GetAtom(ev, keyAtom);
          if ( bookAtom )
            outToken = bookAtom->mBookAtom_Id;
          else
          {
            this->MaybeDirtyStore();
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
        morkFarBookAtom* keyAtom =
          this->StageStringAsFarBookAtom(ev, inTokenName, form, groundSpace);
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
  MORK_USED_2(inRowScope,inTableKind);
  mork_bool outBool = morkBool_kFalse;
  mdb_count tableCount = 0;

  ev->StubMethodOnlyError();
  
  if ( outTableCount )
    *outTableCount = tableCount;
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
          *outMustBeUnique = outTable->IsTableUnique();
      }
    }
  }
  return outTable;
}

morkRow*
morkStore::FindRow(morkEnv* ev, mdb_scope inScope, mdb_column inColumn,
  const mdbYarn* inYarn)
{
  morkRow* outRow = 0;
  if ( ev->Good() )
  {
    morkRowSpace* rowSpace = this->LazyGetRowSpace(ev, inScope);
    if ( rowSpace )
    {
      outRow = rowSpace->FindRow(ev, inColumn, inYarn);
    }
  }
  return outRow;
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
  mdb_kind inTableKind, mdb_bool inMustBeUnique,
  const mdbOid* inOptionalMetaRowOid) // can be nil to avoid specifying 
{
  morkTable* outTable = 0;
  if ( ev->Good() )
  {
    morkRowSpace* rowSpace = this->LazyGetRowSpace(ev, inRowScope);
    if ( rowSpace )
      outTable = rowSpace->NewTable(ev, inTableKind, inMustBeUnique,
        inOptionalMetaRowOid);
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
  NS_IF_ADDREF(outCursor);
  return outCursor;
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
morkStore::OidToRow(morkEnv* ev, const mdbOid* inOid)
  // OidToRow() finds old row with oid, or makes new one if not found.
{
  morkRow* outRow = 0;
  if ( ev->Good() )
  {
    morkRowSpace* rowSpace = this->LazyGetRowSpace(ev, inOid->mOid_Scope);
    if ( rowSpace )
    {
      outRow = rowSpace->mRowSpace_Rows.GetOid(ev, inOid);
      if ( !outRow && ev->Good() )
        outRow = rowSpace->NewRowWithOid(ev, inOid);
    }
  }
  return outRow;
}

morkTable*
morkStore::OidToTable(morkEnv* ev, const mdbOid* inOid,
  const mdbOid* inOptionalMetaRowOid) // can be nil to avoid specifying 
  // OidToTable() finds old table with oid, or makes new one if not found.
{
  morkTable* outTable = 0;
  if ( ev->Good() )
  {
    morkRowSpace* rowSpace = this->LazyGetRowSpace(ev, inOid->mOid_Scope);
    if ( rowSpace )
    {
      outTable = rowSpace->mRowSpace_Tables.GetTable(ev, inOid->mOid_Id);
      if ( !outTable && ev->Good() )
      {
        mork_kind tableKind = morkStore_kNoneToken;
        outTable = rowSpace->NewTableWithTid(ev, inOid->mOid_Id, tableKind,
          inOptionalMetaRowOid);
      }
    }
  }
  return outTable;
}

// { ===== begin nsIMdbObject methods =====

// { ----- begin ref counting for well-behaved cyclic graphs -----
NS_IMETHODIMP
morkStore::GetWeakRefCount(nsIMdbEnv* mev, // weak refs
  mdb_count* outCount)
{
  *outCount = WeakRefsOnly();
  return NS_OK;
}  
NS_IMETHODIMP
morkStore::GetStrongRefCount(nsIMdbEnv* mev, // strong refs
  mdb_count* outCount)
{
  *outCount = StrongRefsOnly();
  return NS_OK;
}
// ### TODO - clean up this cast, if required
NS_IMETHODIMP
morkStore::AddWeakRef(nsIMdbEnv* mev)
{
  morkEnv *ev  = morkEnv::FromMdbEnv(mev);
  return morkNode::AddWeakRef(ev);
}
NS_IMETHODIMP
morkStore::AddStrongRef(nsIMdbEnv* mev)
{
  return AddRef();
}

NS_IMETHODIMP
morkStore::CutWeakRef(nsIMdbEnv* mev)
{
  morkEnv *ev  = morkEnv::FromMdbEnv(mev);
  return morkNode::CutWeakRef(ev);
}
NS_IMETHODIMP
morkStore::CutStrongRef(nsIMdbEnv* mev)
{
  return Release();
}

NS_IMETHODIMP
morkStore::CloseMdbObject(nsIMdbEnv* mev)
{
  morkEnv *ev = morkEnv::FromMdbEnv(mev);
  CloseMorkNode(ev);
  Release();
  return NS_OK;
}

NS_IMETHODIMP
morkStore::IsOpenMdbObject(nsIMdbEnv* mev, mdb_bool* outOpen)
{
  *outOpen = IsOpenNode();
  return NS_OK;
}
// } ----- end ref counting -----

// } ===== end nsIMdbObject methods =====

// { ===== begin nsIMdbPort methods =====

// { ----- begin attribute methods -----
NS_IMETHODIMP
morkStore::GetIsPortReadonly(nsIMdbEnv* mev, mdb_bool* outBool)
{
  mdb_err outErr = 0;
  mdb_bool isReadOnly = morkBool_kFalse;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  if ( outBool )
    *outBool = isReadOnly;
  return outErr;
}

morkEnv*
morkStore::CanUseStore(nsIMdbEnv* mev,
  mork_bool inMutable, mdb_err* outErr) const
{
  morkEnv* outEnv = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    if (IsStore())
      outEnv = ev;
    else
      NonStoreTypeError(ev);
    *outErr = ev->AsErr();
  }
  MORK_ASSERT(outEnv);
  return outEnv;
}

NS_IMETHODIMP
morkStore::GetIsStore(nsIMdbEnv* mev, mdb_bool* outBool)
{
  MORK_USED_1(mev);
 if ( outBool )
    *outBool = morkBool_kTrue;
  return 0;
}

NS_IMETHODIMP
morkStore::GetIsStoreAndDirty(nsIMdbEnv* mev, mdb_bool* outBool)
{
  mdb_err outErr = 0;
  mdb_bool isStoreAndDirty = morkBool_kFalse;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  if ( outBool )
    *outBool = isStoreAndDirty;
  return outErr;
}

NS_IMETHODIMP
morkStore::GetUsagePolicy(nsIMdbEnv* mev, 
  mdbUsagePolicy* ioUsagePolicy)
{
  MORK_USED_1(ioUsagePolicy);
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  return outErr;
}

NS_IMETHODIMP
morkStore::SetUsagePolicy(nsIMdbEnv* mev, 
  const mdbUsagePolicy* inUsagePolicy)
{
  MORK_USED_1(inUsagePolicy);
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    // ev->StubMethodOnlyError(); // okay to do nothing?
    outErr = ev->AsErr();
  }
  return outErr;
}
// } ----- end attribute methods -----

// { ----- begin memory policy methods -----  
NS_IMETHODIMP
morkStore::IdleMemoryPurge( // do memory management already scheduled
  nsIMdbEnv* mev, // context
  mdb_size* outEstimatedBytesFreed) // approximate bytes actually freed
{
  mdb_err outErr = 0;
  mdb_size estimatedBytesFreed = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    // ev->StubMethodOnlyError(); // okay to do nothing?
    outErr = ev->AsErr();
  }
  if ( outEstimatedBytesFreed )
    *outEstimatedBytesFreed = estimatedBytesFreed;
  return outErr;
}

NS_IMETHODIMP
morkStore::SessionMemoryPurge( // request specific footprint decrease
  nsIMdbEnv* mev, // context
  mdb_size inDesiredBytesFreed, // approximate number of bytes wanted
  mdb_size* outEstimatedBytesFreed) // approximate bytes actually freed
{
  MORK_USED_1(inDesiredBytesFreed);
  mdb_err outErr = 0;
  mdb_size estimate = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    // ev->StubMethodOnlyError(); // okay to do nothing?
    outErr = ev->AsErr();
  }
  if ( outEstimatedBytesFreed )
    *outEstimatedBytesFreed = estimate;
  return outErr;
}

NS_IMETHODIMP
morkStore::PanicMemoryPurge( // desperately free all possible memory
  nsIMdbEnv* mev, // context
  mdb_size* outEstimatedBytesFreed) // approximate bytes actually freed
{
  mdb_err outErr = 0;
  mdb_size estimate = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    // ev->StubMethodOnlyError(); // okay to do nothing?
    outErr = ev->AsErr();
  }
  if ( outEstimatedBytesFreed )
    *outEstimatedBytesFreed = estimate;
  return outErr;
}
// } ----- end memory policy methods -----

// { ----- begin filepath methods -----
NS_IMETHODIMP
morkStore::GetPortFilePath(
  nsIMdbEnv* mev, // context
  mdbYarn* outFilePath, // name of file holding port content
  mdbYarn* outFormatVersion) // file format description
{
  mdb_err outErr = 0;
  if ( outFormatVersion )
    outFormatVersion->mYarn_Fill = 0;
  if ( outFilePath )
    outFilePath->mYarn_Fill = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    if ( mStore_File )
      mStore_File->Path(mev, outFilePath);
    else
      NilStoreFileError(ev);
    
    outErr = ev->AsErr();
  }
  return outErr;
}

NS_IMETHODIMP
morkStore::GetPortFile(
  nsIMdbEnv* mev, // context
  nsIMdbFile** acqFile) // acquire file used by port or store
{
  mdb_err outErr = 0;
  if ( acqFile )
    *acqFile = 0;

  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    
    if ( mStore_File )
    {
      if ( acqFile )
      {
        mStore_File->AddRef();
        if ( ev->Good() )
          *acqFile = mStore_File;
      }
    }
    else
      NilStoreFileError(ev);
      
    outErr = ev->AsErr();
  }
  return outErr;
}
// } ----- end filepath methods -----

// { ----- begin export methods -----
NS_IMETHODIMP
morkStore::BestExportFormat( // determine preferred export format
  nsIMdbEnv* mev, // context
  mdbYarn* outFormatVersion) // file format description
{
  mdb_err outErr = 0;
  if ( outFormatVersion )
    outFormatVersion->mYarn_Fill = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  return outErr;
}

NS_IMETHODIMP
morkStore::CanExportToFormat( // can export content in given specific format?
  nsIMdbEnv* mev, // context
  const char* inFormatVersion, // file format description
  mdb_bool* outCanExport) // whether ExportSource() might succeed
{
  MORK_USED_1(inFormatVersion);
  mdb_bool canExport = morkBool_kFalse;
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  if ( outCanExport )
    *outCanExport = canExport;
  return outErr;
}

NS_IMETHODIMP
morkStore::ExportToFormat( // export content in given specific format
  nsIMdbEnv* mev, // context
  // const char* inFilePath, // the file to receive exported content
  nsIMdbFile* ioFile, // destination abstract file interface
  const char* inFormatVersion, // file format description
  nsIMdbThumb** acqThumb) // acquire thumb for incremental export
// Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
// then the export will be finished.
{
  mdb_err outErr = 0;
  nsIMdbThumb* outThumb = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    if ( ioFile && inFormatVersion && acqThumb )
    {
      ev->StubMethodOnlyError();
    }
    else
      ev->NilPointerError();
    
    outErr = ev->AsErr();
  }
  if ( acqThumb )
    *acqThumb = outThumb;
  return outErr;
}

// } ----- end export methods -----

// { ----- begin token methods -----
NS_IMETHODIMP
morkStore::TokenToString( // return a string name for an integer token
  nsIMdbEnv* mev, // context
  mdb_token inToken, // token for inTokenName inside this port
  mdbYarn* outTokenName) // the type of table to access
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    TokenToString(ev, inToken, outTokenName);
    outErr = ev->AsErr();
  }
  return outErr;
}

NS_IMETHODIMP
morkStore::StringToToken( // return an integer token for scope name
  nsIMdbEnv* mev, // context
  const char* inTokenName, // Latin1 string to tokenize if possible
  mdb_token* outToken) // token for inTokenName inside this port
  // String token zero is never used and never supported. If the port
  // is a mutable store, then StringToToken() to create a new
  // association of inTokenName with a new integer token if possible.
  // But a readonly port will return zero for an unknown scope name.
{
  mdb_err outErr = 0;
  mdb_token token = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    token = StringToToken(ev, inTokenName);
    outErr = ev->AsErr();
  }
  if ( outToken )
    *outToken = token;
  return outErr;
}
  

NS_IMETHODIMP
morkStore::QueryToken( // like StringToToken(), but without adding
  nsIMdbEnv* mev, // context
  const char* inTokenName, // Latin1 string to tokenize if possible
  mdb_token* outToken) // token for inTokenName inside this port
  // QueryToken() will return a string token if one already exists,
  // but unlike StringToToken(), will not assign a new token if not
  // already in use.
{
  mdb_err outErr = 0;
  mdb_token token = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    token = QueryToken(ev, inTokenName);
    outErr = ev->AsErr();
  }
  if ( outToken )
    *outToken = token;
  return outErr;
}


// } ----- end token methods -----

// { ----- begin row methods -----  
NS_IMETHODIMP
morkStore::HasRow( // contains a row with the specified oid?
  nsIMdbEnv* mev, // context
  const mdbOid* inOid,  // hypothetical row oid
  mdb_bool* outHasRow) // whether GetRow() might succeed
{
  mdb_err outErr = 0;
  mdb_bool hasRow = morkBool_kFalse;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkRow* row = GetRow(ev, inOid);
    if ( row )
      hasRow = morkBool_kTrue;
      
    outErr = ev->AsErr();
  }
  if ( outHasRow )
    *outHasRow = hasRow;
  return outErr;
}
  
NS_IMETHODIMP
morkStore::GetRow( // access one row with specific oid
  nsIMdbEnv* mev, // context
  const mdbOid* inOid,  // hypothetical row oid
  nsIMdbRow** acqRow) // acquire specific row (or null)
{
  mdb_err outErr = 0;
  nsIMdbRow* outRow = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkRow* row = GetRow(ev, inOid);
    if ( row && ev->Good() )
      outRow = row->AcquireRowHandle(ev, this);
      
    outErr = ev->AsErr();
  }
  if ( acqRow )
    *acqRow = outRow;
  return outErr;
}

NS_IMETHODIMP
morkStore::GetRowRefCount( // get number of tables that contain a row 
  nsIMdbEnv* mev, // context
  const mdbOid* inOid,  // hypothetical row oid
  mdb_count* outRefCount) // number of tables containing inRowKey 
{
  mdb_err outErr = 0;
  mdb_count count = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkRow* row = GetRow(ev, inOid);
    if ( row && ev->Good() )
      count = row->mRow_GcUses;
      
    outErr = ev->AsErr();
  }
  if ( outRefCount )
    *outRefCount = count;
  return outErr;
}

NS_IMETHODIMP
morkStore::FindRow(nsIMdbEnv* mev, // search for row with matching cell
    mdb_scope inRowScope,   // row scope for row ids
    mdb_column inColumn,   // the column to search (and maintain an index)
    const mdbYarn* inTargetCellValue, // cell value for which to search
    mdbOid* outRowOid, // out row oid on match (or {0,-1} for no match)
    nsIMdbRow** acqRow) // acquire matching row (or nil for no match)
  // FindRow() searches for one row that has a cell in column inColumn with
  // a contained value with the same form (i.e. charset) and is byte-wise
  // identical to the blob described by yarn inTargetCellValue.  Both content
  // and form of the yarn must be an exact match to find a matching row.
  //
  // (In other words, both a yarn's blob bytes and form are significant.  The
  // form is not expected to vary in columns used for identity anyway.  This
  // is intended to make the cost of FindRow() cheaper for MDB implementors,
  // since any cell value atomization performed internally must necessarily
  // make yarn form significant in order to avoid data loss in atomization.)
  //
  // FindRow() can lazily create an index on attribute inColumn for all rows
  // with that attribute in row space scope inRowScope, so that subsequent
  // calls to FindRow() will perform faster.  Such an index might or might
  // not be persistent (but this seems desirable if it is cheap to do so).
  // Note that lazy index creation in readonly DBs is not very feasible.
  //
  // This FindRow() interface assumes that attribute inColumn is effectively
  // an alternative means of unique identification for a row in a rowspace,
  // so correct behavior is only guaranteed when no duplicates for this col
  // appear in the given set of rows.  (If more than one row has the same cell
  // value in this column, no more than one will be found; and cutting one of
  // two duplicate rows can cause the index to assume no other such row lives
  // in the row space, so future calls return nil for negative search results
  // even though some duplicate row might still live within the rowspace.)
  //
  // In other words, the FindRow() implementation is allowed to assume simple
  // hash tables mapping unqiue column keys to associated row values will be
  // sufficient, where any duplication is not recorded because only one copy
  // of a given key need be remembered.  Implementors are not required to sort
  // all rows by the specified column.
{
  mdb_err outErr = 0;
  nsIMdbRow* outRow = 0;
  mdbOid rowOid;
  rowOid.mOid_Scope = 0;
  rowOid.mOid_Id = (mdb_id) -1;
  
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkRow* row = FindRow(ev, inRowScope, inColumn, inTargetCellValue);
    if ( row && ev->Good() )
    {
      outRow = row->AcquireRowHandle(ev, this);
      if ( outRow )
        rowOid = row->mRow_Oid;
    }
    outErr = ev->AsErr();
  }
  if ( acqRow )
    *acqRow = outRow;
    
  return outErr;
}

// } ----- end row methods -----

// { ----- begin table methods -----  
NS_IMETHODIMP
morkStore::HasTable( // supports a table with the specified oid?
  nsIMdbEnv* mev, // context
  const mdbOid* inOid,  // hypothetical table oid
  mdb_bool* outHasTable) // whether GetTable() might succeed
{
  mdb_err outErr = 0;
  mork_bool hasTable = morkBool_kFalse;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTable* table = GetTable(ev, inOid);
    if ( table )
      hasTable = morkBool_kTrue;
    
    outErr = ev->AsErr();
  }
  if ( outHasTable )
    *outHasTable = hasTable;
  return outErr;
}
  
NS_IMETHODIMP
morkStore::GetTable( // access one table with specific oid
  nsIMdbEnv* mev, // context
  const mdbOid* inOid,  // hypothetical table oid
  nsIMdbTable** acqTable) // acquire specific table (or null)
{
  mdb_err outErr = 0;
  nsIMdbTable* outTable = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTable* table = GetTable(ev, inOid);
    if ( table && ev->Good() )
      outTable = table->AcquireTableHandle(ev);
    outErr = ev->AsErr();
  }
  if ( acqTable )
    *acqTable = outTable;
  return outErr;
}

NS_IMETHODIMP
morkStore::HasTableKind( // supports a table of the specified type?
  nsIMdbEnv* mev, // context
  mdb_scope inRowScope, // rid scope for row ids
  mdb_kind inTableKind, // the type of table to access
  mdb_count* outTableCount, // current number of such tables
  mdb_bool* outSupportsTable) // whether GetTableKind() might succeed
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    *outSupportsTable = HasTableKind(ev, inRowScope,
        inTableKind, outTableCount);
    outErr = ev->AsErr();
  }
  return outErr;
}
      
NS_IMETHODIMP
morkStore::GetTableKind( // access one (random) table of specific type
  nsIMdbEnv* mev, // context
  mdb_scope inRowScope,      // row scope for row ids
  mdb_kind inTableKind,      // the type of table to access
  mdb_count* outTableCount, // current number of such tables
  mdb_bool* outMustBeUnique, // whether port can hold only one of these
  nsIMdbTable** acqTable)      // acquire scoped collection of rows
{
  mdb_err outErr = 0;
  nsIMdbTable* outTable = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTable* table = GetTableKind(ev, inRowScope,
        inTableKind, outTableCount, outMustBeUnique);
    if ( table && ev->Good() )
      outTable = table->AcquireTableHandle(ev);
    outErr = ev->AsErr();
  }
  if ( acqTable )
    *acqTable = outTable;
  return outErr;
}
  
NS_IMETHODIMP
morkStore::GetPortTableCursor( // get cursor for all tables of specific type
  nsIMdbEnv* mev, // context
  mdb_scope inRowScope, // row scope for row ids
  mdb_kind inTableKind, // the type of table to access
  nsIMdbPortTableCursor** acqCursor) // all such tables in the port
{
  mdb_err outErr = 0;
  nsIMdbPortTableCursor* outCursor = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkPortTableCursor* cursor =
      GetPortTableCursor(ev, inRowScope,
        inTableKind);
    if ( cursor && ev->Good() )
      outCursor = cursor;

    outErr = ev->AsErr();
  }
  if ( acqCursor )
    *acqCursor = outCursor;
  return outErr;
}
// } ----- end table methods -----

// { ----- begin commit methods -----
  
NS_IMETHODIMP
morkStore::ShouldCompress( // store wastes at least inPercentWaste?
  nsIMdbEnv* mev, // context
  mdb_percent inPercentWaste, // 0..100 percent file size waste threshold
  mdb_percent* outActualWaste, // 0..100 percent of file actually wasted
  mdb_bool* outShould) // true when about inPercentWaste% is wasted
{
  mdb_percent actualWaste = 0;
  mdb_bool shouldCompress = morkBool_kFalse;
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    actualWaste = PercentOfStoreWasted(ev);
    if ( inPercentWaste > 100 )
      inPercentWaste = 100;
    shouldCompress = ( actualWaste >= inPercentWaste );
    outErr = ev->AsErr();
  }
  if ( outActualWaste )
    *outActualWaste = actualWaste;
  if ( outShould )
    *outShould = shouldCompress;
  return outErr;
}


// } ===== end nsIMdbPort methods =====

NS_IMETHODIMP
morkStore::NewTable( // make one new table of specific type
  nsIMdbEnv* mev, // context
  mdb_scope inRowScope,    // row scope for row ids
  mdb_kind inTableKind,    // the type of table to access
  mdb_bool inMustBeUnique, // whether store can hold only one of these
  const mdbOid* inOptionalMetaRowOid, // can be nil to avoid specifying 
  nsIMdbTable** acqTable)     // acquire scoped collection of rows
{
  mdb_err outErr = 0;
  nsIMdbTable* outTable = 0;
  morkEnv* ev = this->CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTable* table = NewTable(ev, inRowScope,
        inTableKind, inMustBeUnique, inOptionalMetaRowOid);
    if ( table && ev->Good() )
      outTable = table->AcquireTableHandle(ev);
    outErr = ev->AsErr();
  }
  if ( acqTable )
    *acqTable = outTable;
  return outErr;
}

NS_IMETHODIMP
morkStore::NewTableWithOid( // make one new table of specific type
  nsIMdbEnv* mev, // context
  const mdbOid* inOid,   // caller assigned oid
  mdb_kind inTableKind,    // the type of table to access
  mdb_bool inMustBeUnique, // whether store can hold only one of these
  const mdbOid* inOptionalMetaRowOid, // can be nil to avoid specifying 
  nsIMdbTable** acqTable)     // acquire scoped collection of rows
{
  mdb_err outErr = 0;
  nsIMdbTable* outTable = 0;
  morkEnv* ev = CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkTable* table = OidToTable(ev, inOid,
      inOptionalMetaRowOid);
    if ( table && ev->Good() )
    {
      table->mTable_Kind = inTableKind;
      if ( inMustBeUnique )
        table->SetTableUnique();
      outTable = table->AcquireTableHandle(ev);
    }
    outErr = ev->AsErr();
  }
  if ( acqTable )
    *acqTable = outTable;
  return outErr;
}
// } ----- end table methods -----

// { ----- begin row scope methods -----
NS_IMETHODIMP
morkStore::RowScopeHasAssignedIds(nsIMdbEnv* mev,
  mdb_scope inRowScope,   // row scope for row ids
  mdb_bool* outCallerAssigned, // nonzero if caller assigned specified
  mdb_bool* outStoreAssigned) // nonzero if store db assigned specified
{
  NS_ASSERTION(PR_FALSE, " not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
morkStore::SetCallerAssignedIds(nsIMdbEnv* mev,
  mdb_scope inRowScope,   // row scope for row ids
  mdb_bool* outCallerAssigned, // nonzero if caller assigned specified
  mdb_bool* outStoreAssigned) // nonzero if store db assigned specified
{
  NS_ASSERTION(PR_FALSE, " not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
morkStore::SetStoreAssignedIds(nsIMdbEnv* mev,
  mdb_scope inRowScope,   // row scope for row ids
  mdb_bool* outCallerAssigned, // nonzero if caller assigned specified
  mdb_bool* outStoreAssigned) // nonzero if store db assigned specified
{
  NS_ASSERTION(PR_FALSE, " not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}
// } ----- end row scope methods -----

// { ----- begin row methods -----
NS_IMETHODIMP
morkStore::NewRowWithOid(nsIMdbEnv* mev, // new row w/ caller assigned oid
  const mdbOid* inOid,   // caller assigned oid
  nsIMdbRow** acqRow) // create new row
{
  mdb_err outErr = 0;
  nsIMdbRow* outRow = 0;
  morkEnv* ev = CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkRow* row = NewRowWithOid(ev, inOid);
    if ( row && ev->Good() )
      outRow = row->AcquireRowHandle(ev, this);
      
    outErr = ev->AsErr();
  }
  if ( acqRow )
    *acqRow = outRow;
  return outErr;
}

NS_IMETHODIMP
morkStore::NewRow(nsIMdbEnv* mev, // new row with db assigned oid
  mdb_scope inRowScope,   // row scope for row ids
  nsIMdbRow** acqRow) // create new row
// Note this row must be added to some table or cell child before the
// store is closed in order to make this row persist across sesssions.
{
  mdb_err outErr = 0;
  nsIMdbRow* outRow = 0;
  morkEnv* ev = CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkRow* row = NewRow(ev, inRowScope);
    if ( row && ev->Good() )
      outRow = row->AcquireRowHandle(ev, this);
      
    outErr = ev->AsErr();
  }
  if ( acqRow )
    *acqRow = outRow;
  return outErr;
}
// } ----- end row methods -----

// { ----- begin inport/export methods -----
NS_IMETHODIMP
morkStore::ImportContent( // import content from port
  nsIMdbEnv* mev, // context
  mdb_scope inRowScope, // scope for rows (or zero for all?)
  nsIMdbPort* ioPort, // the port with content to add to store
  nsIMdbThumb** acqThumb) // acquire thumb for incremental import
// Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
// then the import will be finished.
{
  NS_ASSERTION(PR_FALSE, " not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
morkStore::ImportFile( // import content from port
  nsIMdbEnv* mev, // context
  nsIMdbFile* ioFile, // the file with content to add to store
  nsIMdbThumb** acqThumb) // acquire thumb for incremental import
// Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
// then the import will be finished.
{
  NS_ASSERTION(PR_FALSE, " not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}
// } ----- end inport/export methods -----

// { ----- begin hinting methods -----
NS_IMETHODIMP
morkStore::ShareAtomColumnsHint( // advise re shared col content atomizing
  nsIMdbEnv* mev, // context
  mdb_scope inScopeHint, // zero, or suggested shared namespace
  const mdbColumnSet* inColumnSet) // cols desired tokenized together
{
  MORK_USED_2(inColumnSet,inScopeHint);
  mdb_err outErr = 0;
  morkEnv* ev = CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    // ev->StubMethodOnlyError(); // okay to do nothing for a hint method
    outErr = ev->AsErr();
  }
  return outErr;
}

NS_IMETHODIMP
morkStore::AvoidAtomColumnsHint( // advise col w/ poor atomizing prospects
  nsIMdbEnv* mev, // context
  const mdbColumnSet* inColumnSet) // cols with poor atomizing prospects
{
  MORK_USED_1(inColumnSet);
  mdb_err outErr = 0;
  morkEnv* ev = CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    // ev->StubMethodOnlyError(); // okay to do nothing for a hint method
    outErr = ev->AsErr();
  }
  return outErr;
}
// } ----- end hinting methods -----

// { ----- begin commit methods -----
NS_IMETHODIMP
morkStore::SmallCommit( // save minor changes if convenient and uncostly
  nsIMdbEnv* mev) // context
{
  mdb_err outErr = 0;
  morkEnv* ev = CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    // ev->StubMethodOnlyError(); // it's okay to do nothing for small commit
    outErr = ev->AsErr();
  }
  return outErr;
}

NS_IMETHODIMP
morkStore::LargeCommit( // save important changes if at all possible
  nsIMdbEnv* mev, // context
  nsIMdbThumb** acqThumb) // acquire thumb for incremental commit
// Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
// then the commit will be finished.  Note the store is effectively write
// locked until commit is finished or canceled through the thumb instance.
// Until the commit is done, the store will report it has readonly status.
{
  nsresult outErr = 0;
  nsIMdbThumb* outThumb = 0;
  morkEnv* ev = CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    
    morkThumb* thumb = 0;
    // morkFile* file = store->mStore_File;
    if ( DoPreferLargeOverCompressCommit(ev) )
    {
      thumb = morkThumb::Make_LargeCommit(ev, mPort_Heap, this);
    }
    else
    {
      mork_bool doCollect = morkBool_kFalse;
      thumb = morkThumb::Make_CompressCommit(ev, mPort_Heap, this, doCollect);
    }
    
    if ( thumb )
    {
      outThumb = thumb;
      thumb->AddRef();
    }
      
    outErr = ev->AsErr();
  }
  if ( acqThumb )
    *acqThumb = outThumb;
  return outErr;
}

NS_IMETHODIMP
morkStore::SessionCommit( // save all changes if large commits delayed
  nsIMdbEnv* mev, // context
  nsIMdbThumb** acqThumb) // acquire thumb for incremental commit
// Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
// then the commit will be finished.  Note the store is effectively write
// locked until commit is finished or canceled through the thumb instance.
// Until the commit is done, the store will report it has readonly status.
{
  nsresult outErr = NS_OK;
  nsIMdbThumb* outThumb = 0;
  morkEnv* ev = CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    
    morkThumb* thumb = 0;
    if ( DoPreferLargeOverCompressCommit(ev) )
    {
      thumb = morkThumb::Make_LargeCommit(ev, mPort_Heap, this);
    }
    else
    {
      mork_bool doCollect = morkBool_kFalse;
      thumb = morkThumb::Make_CompressCommit(ev, mPort_Heap, this, doCollect);
    }
    
    if ( thumb )
    {
      outThumb = thumb;
      thumb->AddRef();
    }
    outErr = ev->AsErr();
  }
  if ( acqThumb )
    *acqThumb = outThumb;
  return outErr;
}

NS_IMETHODIMP
morkStore::CompressCommit( // commit and make db smaller if possible
  nsIMdbEnv* mev, // context
  nsIMdbThumb** acqThumb) // acquire thumb for incremental commit
// Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
// then the commit will be finished.  Note the store is effectively write
// locked until commit is finished or canceled through the thumb instance.
// Until the commit is done, the store will report it has readonly status.
{
  nsresult outErr = NS_OK;
  nsIMdbThumb* outThumb = 0;
  morkEnv* ev = CanUseStore(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    mork_bool doCollect = morkBool_kFalse;
    morkThumb* thumb = morkThumb::Make_CompressCommit(ev, mPort_Heap, this, doCollect);
    if ( thumb )
    {
      outThumb = thumb;
      thumb->AddRef();
      mStore_CanWriteIncremental = morkBool_kTrue;
    }
      
    outErr = ev->AsErr();
  }
  if ( acqThumb )
    *acqThumb = outThumb;
  return outErr;
}

// } ----- end commit methods -----

// } ===== end nsIMdbStore methods =====


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789


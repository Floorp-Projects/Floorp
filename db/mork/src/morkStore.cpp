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
: morkObject(ev, inUsage, ioNodeHeap, morkColor_kNone, (morkHandle*) 0)
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
  if ( ev->Good() )
  {
    mNode_Derived = morkDerived_kStore;
    
  }
}

/*public non-poly*/ void
morkStore::CloseStore(morkEnv* ev) // called by CloseMorkNode();
{
  if ( this )
  {
    if ( this->IsNode() )
    {

      nsIMdbFile* file = mStore_File;
      if ( file )
        file->CloseMdbObject(ev->AsMdbEnv());

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
      outAtom = this->YarnToAtom(ev, &yarn);
  }
  return outAtom;
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
      morkFarBookAtom* keyAtom =
        this->StageYarnAsFarBookAtom(ev, inYarn, groundSpace);
        
      if ( keyAtom )
      {
        morkAtomBodyMap* map = &groundSpace->mAtomSpace_AtomBodies;
        outAtom = map->GetAtom(ev, keyAtom);
        if ( !outAtom )
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

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789


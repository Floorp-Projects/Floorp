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

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _MORKFACTORY_
#include "morkFactory.h"
#endif

#ifndef _ORKINFACTORY_
#include "orkinFactory.h"
#endif

#ifndef _ORKINENV_
#include "orkinEnv.h"
#endif

#ifndef _MORKROW_
#include "morkRow.h"
#endif

#ifndef _ORKINROW_
#include "orkinRow.h"
#endif

#ifndef _MORKFILE_
#include "morkFile.h"
#endif

#ifndef _MORKWRITER_
#include "morkWriter.h"
#endif

#ifndef _MORKSTORE_
#include "morkStore.h"
#endif

#ifndef _ORKINSTORE_
#include "orkinStore.h"
#endif

#ifndef _ORKINTHUMB_
#include "orkinThumb.h"
#endif

#ifndef _MORKTHUMB_
#include "morkThumb.h"
#endif

#ifndef _ORKINHEAP_
#include "orkinHeap.h"
#endif

#ifndef _ORKINCOMPARE_
#include "orkinCompare.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

/* public virtual*/
orkinFactory:: ~orkinFactory() // morkHandle destructor does everything
{
}

/*protected non-poly construction*/
orkinFactory::orkinFactory(morkEnv* ev, // morkUsage is morkUsage_kPool
    morkHandleFace* ioFace,    // must not be nil, cookie for this handle
    morkFactory* ioObject)  // must not be nil, object for this handle
: morkHandle(ev, ioFace, ioObject, morkMagic_kFactory)
{
  // do not modify mNode_Derived; leave it equal to morkDerived_kHandle
}

extern "C" nsIMdbFactory* MakeMdbFactory() 
{
  return orkinFactory::MakeGlobalFactory();
}

/*static */ orkinFactory*
orkinFactory::MakeGlobalFactory()
// instantiate objects using almost no context information.
{
  morkFactory* factory = new morkFactory(new orkinHeap());
  MORK_ASSERT(factory);
  if ( factory )
    return orkinFactory::MakeFactory(&factory->mFactory_Env, factory);
  else
    return (orkinFactory*) 0;
}

/*static */ orkinFactory*
orkinFactory::MakeFactory(morkEnv* ev,  morkFactory* ioObject)
{
  mork_bool isEnv = ev->IsEnv();
  MORK_ASSERT(isEnv);
  if ( isEnv )
  {
    morkHandleFace* face = ev->NewHandle(sizeof(orkinFactory));
    if ( face )
    {
      orkinFactory* f = new(face) orkinFactory(ev, face, ioObject);
      if ( f )
        f->mNode_Refs += morkFactory_kWeakRefCountBonus;
      return f;
    }
    else
      ev->OutOfMemoryError();
  }
    
  return (orkinFactory*) 0;
}

morkEnv*
orkinFactory::CanUseFactory(nsIMdbEnv* mev, mork_bool inMutable,
  mdb_err* outErr) const
{
  morkEnv* outEnv = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    morkFactory* factory = (morkFactory*)
      this->GetGoodHandleObject(ev, inMutable, morkMagic_kFactory,
        /*inClosedOkay*/ morkBool_kFalse);
    if ( factory )
    {
      if ( factory->IsFactory() )
        outEnv = ev;
      else
        factory->NonFactoryTypeError(ev);
    }
    *outErr = ev->AsErr();
  }
  MORK_ASSERT(outEnv);
  return outEnv;
}

morkEnv* orkinFactory::GetInternalFactoryEnv(mdb_err* outErr)
{
  morkEnv* outEnv = 0;
  morkFactory* f = (morkFactory*) this->mHandle_Object;
  if ( f && f->IsNode() && f->IsOpenNode() && f->IsFactory() )
  {
    morkEnv* fenv = &f->mFactory_Env;
    if ( fenv && fenv->IsNode() && fenv->IsOpenNode() && fenv->IsEnv() )
    {
      fenv->ClearMorkErrorsAndWarnings(); // drop any earlier errors
      outEnv = fenv;
    }
    else
      *outErr = morkEnv_kBadFactoryEnvError;
  }
  else
    *outErr = morkEnv_kBadFactoryError;
    
  return outEnv;
}

// { ===== begin nsIMdbISupports methods =====
/*virtual*/ mdb_err
orkinFactory::AddRef() // add strong ref with no
{
  morkEnv* ev = mHandle_Env;
  if ( ev && ev->IsEnv() )
    return this->Handle_AddStrongRef(ev->AsMdbEnv());
  else
    return morkEnv_kNonEnvTypeError;
}

/*virtual*/ mdb_err
orkinFactory::Release() // cut strong ref
{
  morkEnv* ev = mHandle_Env;
  if ( ev && ev->IsEnv() )
    return this->Handle_CutStrongRef(ev->AsMdbEnv());
  else
    return morkEnv_kNonEnvTypeError;
}
// } ===== end nsIMdbObject methods =====


// { ===== begin nsIMdbObject methods =====

// { ----- begin attribute methods -----
/*virtual*/ mdb_err
orkinFactory::IsFrozenMdbObject(nsIMdbEnv* mev, mdb_bool* outIsReadonly)
{
  return this->Handle_IsFrozenMdbObject(mev, outIsReadonly);
}
// same as nsIMdbPort::GetIsPortReadonly() when this object is inside a port.
// } ----- end attribute methods -----

// { ----- begin factory methods -----
/*virtual*/ mdb_err
orkinFactory::GetMdbFactory(nsIMdbEnv* mev, nsIMdbFactory** acqFactory)
{
  return this->Handle_GetMdbFactory(mev, acqFactory);
} 
// } ----- end factory methods -----

// { ----- begin ref counting for well-behaved cyclic graphs -----
/*virtual*/ mdb_err
orkinFactory::GetWeakRefCount(nsIMdbEnv* mev, // weak refs
  mdb_count* outCount)
{
  return this->Handle_GetWeakRefCount(mev, outCount);
}  
/*virtual*/ mdb_err
orkinFactory::GetStrongRefCount(nsIMdbEnv* mev, // strong refs
  mdb_count* outCount)
{
  return this->Handle_GetStrongRefCount(mev, outCount);
}

/*virtual*/ mdb_err
orkinFactory::AddWeakRef(nsIMdbEnv* mev)
{
  return this->Handle_AddWeakRef(mev);
}
/*virtual*/ mdb_err
orkinFactory::AddStrongRef(nsIMdbEnv* mev)
{
  return this->Handle_AddStrongRef(mev);
}

/*virtual*/ mdb_err
orkinFactory::CutWeakRef(nsIMdbEnv* mev)
{
  return this->Handle_CutWeakRef(mev);
}
/*virtual*/ mdb_err
orkinFactory::CutStrongRef(nsIMdbEnv* mev)
{
  return this->Handle_CutStrongRef(mev);
}

/*virtual*/ mdb_err
orkinFactory::CloseMdbObject(nsIMdbEnv* mev)
{
  return this->Handle_CloseMdbObject(mev);
}

/*virtual*/ mdb_err
orkinFactory::IsOpenMdbObject(nsIMdbEnv* mev, mdb_bool* outOpen)
{
  return this->Handle_IsOpenMdbObject(mev, outOpen);
}
// } ----- end ref counting -----

// } ===== end nsIMdbObject methods =====

// { ===== begin nsIMdbFactory methods =====

// { ----- begin file methods -----
/*virtual*/ mdb_err
orkinFactory::OpenOldFile(nsIMdbEnv* mev, nsIMdbHeap* ioHeap,
  const char* inFilePath,
  mork_bool inFrozen, nsIMdbFile** acqFile)
  // Choose some subclass of nsIMdbFile to instantiate, in order to read
  // (and write if not frozen) the file known by inFilePath.  The file
  // returned should be open and ready for use, and presumably positioned
  // at the first byte position of the file.  The exact manner in which
  // files must be opened is considered a subclass specific detail, and
  // other portions or Mork source code don't want to know how it's done.
{
  mdb_err outErr = 0;
  nsIMdbFile* outFile = 0;
  morkEnv* ev = this->CanUseFactory(mev,
    /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkFactory* factory = (morkFactory*) this->mHandle_Object;
    if ( !ioHeap )
      ioHeap = &factory->mFactory_Heap;
      
    morkFile* file = morkFile::OpenOldFile(ev, ioHeap, inFilePath, inFrozen);
    if ( file )
    {
      outFile = file->AcquireFileHandle(ev);
      file->CutStrongRef(ev);
    }
      
    outErr = ev->AsErr();
  }
  if ( acqFile )
    *acqFile = outFile;
    
  return outErr;
}

/*virtual*/ mdb_err
orkinFactory::CreateNewFile(nsIMdbEnv* mev, nsIMdbHeap* ioHeap,
  const char* inFilePath, nsIMdbFile** acqFile)
  // Choose some subclass of nsIMdbFile to instantiate, in order to read
  // (and write if not frozen) the file known by inFilePath.  The file
  // returned should be created and ready for use, and presumably positioned
  // at the first byte position of the file.  The exact manner in which
  // files must be opened is considered a subclass specific detail, and
  // other portions or Mork source code don't want to know how it's done.
{
  mdb_err outErr = 0;
  nsIMdbFile* outFile = 0;
  morkEnv* ev = this->CanUseFactory(mev,
    /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkFactory* factory = (morkFactory*) this->mHandle_Object;
    if ( !ioHeap )
      ioHeap = &factory->mFactory_Heap;
      
    morkFile* file = morkFile::CreateNewFile(ev, ioHeap, inFilePath);
    if ( file )
    {
      outFile = file->AcquireFileHandle(ev);
      file->CutStrongRef(ev);
    }
      
    outErr = ev->AsErr();
  }
  if ( acqFile )
    *acqFile = outFile;
    
  return outErr;
}
// } ----- end file methods -----

// { ----- begin env methods -----
/*virtual*/ mdb_err
orkinFactory::MakeEnv(nsIMdbHeap* ioHeap, nsIMdbEnv** acqEnv)
// ioHeap can be nil, causing a MakeHeap() style heap instance to be used
{
  mdb_err outErr = 0;
  nsIMdbEnv* outEnv = 0;
  if ( !ioHeap )
    ioHeap = new orkinHeap();

  if ( acqEnv && ioHeap )
  {
    morkEnv* fenv = this->GetInternalFactoryEnv(&outErr);
    if ( fenv )
    {
      morkFactory* factory = (morkFactory*) this->mHandle_Object;
      morkEnv* newEnv = new(*ioHeap, fenv)
        morkEnv(morkUsage::kHeap, ioHeap, factory, ioHeap);

      if ( newEnv )
      {
        newEnv->mNode_Refs += morkEnv_kWeakRefCountEnvBonus;
        
        orkinEnv* oenv = orkinEnv::MakeEnv(fenv, newEnv);
        if ( oenv )
        {
          oenv->mNode_Refs += morkEnv_kWeakRefCountEnvBonus;
          newEnv->mEnv_SelfAsMdbEnv = oenv;
          outEnv = oenv;
        }
        else
          outErr = morkEnv_kOutOfMemoryError;
      }
      else
        outErr = morkEnv_kOutOfMemoryError;
    }
    
    *acqEnv = outEnv;
  }
  else
    outErr = morkEnv_kNilPointerError;
    
  return outErr;
}
// } ----- end env methods -----

// { ----- begin heap methods -----
/*virtual*/ mdb_err
orkinFactory::MakeHeap(nsIMdbEnv* mev, nsIMdbHeap** acqHeap)
{
  mdb_err outErr = 0;
  nsIMdbHeap* outHeap = 0;
  morkEnv* ev = this->CanUseFactory(mev,
    /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    outHeap = new orkinHeap();
    if ( !outHeap )
      ev->OutOfMemoryError();
  }
  MORK_ASSERT(acqHeap);
  if ( acqHeap )
    *acqHeap = outHeap;
  return outErr;
}
// } ----- end heap methods -----

// { ----- begin compare methods -----
/*virtual*/ mdb_err
orkinFactory::MakeCompare(nsIMdbEnv* mev, nsIMdbCompare** acqCompare)
{
  mdb_err outErr = 0;
  nsIMdbCompare* outCompare = 0;
  morkEnv* ev = this->CanUseFactory(mev,
    /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    outCompare = new orkinCompare();
    if ( !outCompare )
      ev->OutOfMemoryError();
  }
  if ( acqCompare )
    *acqCompare = outCompare;
  return outErr;
}
// } ----- end compare methods -----

// { ----- begin row methods -----
/*virtual*/ mdb_err
orkinFactory::MakeRow(nsIMdbEnv* mev, nsIMdbHeap* ioHeap,
  nsIMdbRow** acqRow)
{
  MORK_USED_1(ioHeap);
  mdb_err outErr = 0;
  nsIMdbRow* outRow = 0;
  morkEnv* ev = this->CanUseFactory(mev,
    /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    ev->StubMethodOnlyError();
    outErr = ev->AsErr();
  }
  if ( acqRow )
    *acqRow = outRow;
    
  return outErr;
}
// ioHeap can be nil, causing the heap associated with ev to be used
// } ----- end row methods -----

// { ----- begin port methods -----
/*virtual*/ mdb_err
orkinFactory::CanOpenFilePort(
  nsIMdbEnv* mev, // context
  // const char* inFilePath, // the file to investigate
  // const mdbYarn* inFirst512Bytes,
  nsIMdbFile* ioFile, // db abstract file interface
  mdb_bool* outCanOpen, // whether OpenFilePort() might succeed
  mdbYarn* outFormatVersion)
{
  mdb_err outErr = 0;
  if ( outFormatVersion )
  {
    outFormatVersion->mYarn_Fill = 0;
  }
  mdb_bool canOpenAsPort = morkBool_kFalse;
  morkEnv* ev = this->CanUseFactory(mev,
    /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    if ( ioFile && outCanOpen )
    {
      canOpenAsPort = this->CanOpenMorkTextFile(ev, ioFile);
    }
    else
      ev->NilPointerError();
    
    outErr = ev->AsErr();
  }
    
  if ( outCanOpen )
    *outCanOpen = canOpenAsPort;
    
  return outErr;
}
  
/*virtual*/ mdb_err
orkinFactory::OpenFilePort(
  nsIMdbEnv* mev, // context
  nsIMdbHeap* ioHeap, // can be nil to cause ev's heap attribute to be used
  // const char* inFilePath, // the file to open for readonly import
  nsIMdbFile* ioFile, // db abstract file interface
  const mdbOpenPolicy* inOpenPolicy, // runtime policies for using db
  nsIMdbThumb** acqThumb)
{
  MORK_USED_1(ioHeap);
  mdb_err outErr = 0;
  nsIMdbThumb* outThumb = 0;
  morkEnv* ev = this->CanUseFactory(mev,
    /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    if ( ioFile && inOpenPolicy && acqThumb )
    {
    }
    else
      ev->NilPointerError();
    
    outErr = ev->AsErr();
  }
  if ( acqThumb )
    *acqThumb = outThumb;
  return outErr;
}
// Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
// then call nsIMdbFactory::ThumbToOpenPort() to get the port instance.

/*virtual*/ mdb_err
orkinFactory::ThumbToOpenPort( // redeeming a completed thumb from OpenFilePort()
  nsIMdbEnv* mev, // context
  nsIMdbThumb* ioThumb, // thumb from OpenFilePort() with done status
  nsIMdbPort** acqPort)
{
  mdb_err outErr = 0;
  nsIMdbPort* outPort = 0;
  morkEnv* ev = this->CanUseFactory(mev,
    /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    if ( ioThumb && acqPort )
    {
      orkinThumb* othumb = (orkinThumb*) ioThumb;
      if ( othumb->CanUseThumb(mev, /*inMutable*/ morkBool_kFalse, &outErr) )
      {
        morkThumb* thumb = (morkThumb*) othumb->mHandle_Object;
        morkStore* store = thumb->ThumbToOpenStore(ev);
        if ( store )
        {
          store->mStore_CanAutoAssignAtomIdentity = morkBool_kTrue;
          store->mStore_CanDirty = morkBool_kTrue;
          store->SetStoreAndAllSpacesCanDirty(ev, morkBool_kTrue);
          
          outPort = orkinStore::MakeStore(ev, store);
        }
      }
    }
    else
      ev->NilPointerError();
    
    outErr = ev->AsErr();
  }
  if ( acqPort )
    *acqPort = outPort;
  return outErr;
}
// } ----- end port methods -----

mork_bool
orkinFactory::CanOpenMorkTextFile(morkEnv* ev,
  // const mdbYarn* inFirst512Bytes,
  nsIMdbFile* ioFile)
{
  MORK_USED_1(ev);
  mork_bool outBool = morkBool_kFalse;
  mork_size headSize = MORK_STRLEN(morkWriter_kFileHeader);
  
  char localBuf[ 256 + 4 ]; // for extra for sloppy safety
  mdbYarn localYarn;
  mdbYarn* y = &localYarn;
  y->mYarn_Buf = localBuf; // space to hold content
  y->mYarn_Fill = 0;       // no logical content yet
  y->mYarn_Size = 256;     // physical capacity is 256 bytes
  y->mYarn_More = 0;
  y->mYarn_Form = 0;
  y->mYarn_Grow = 0;
  
  if ( ioFile )
  {
    nsIMdbEnv* menv = ev->AsMdbEnv();
    mdb_size actualSize = 0;
    ioFile->Get(menv, y->mYarn_Buf, y->mYarn_Size, /*pos*/ 0, &actualSize);
    y->mYarn_Fill = actualSize;
    
    if ( y->mYarn_Buf && actualSize >= headSize && ev->Good() )
    {
      mork_u1* buf = (mork_u1*) y->mYarn_Buf;
      outBool = ( MORK_MEMCMP(morkWriter_kFileHeader, buf, headSize) == 0 );
    }
  }
  else
    ev->NilPointerError();

  return outBool;
}

// { ----- begin store methods -----
/*virtual*/ mdb_err
orkinFactory::CanOpenFileStore(
  nsIMdbEnv* mev, // context
  // const char* inFilePath, // the file to investigate
  // const mdbYarn* inFirst512Bytes,
  nsIMdbFile* ioFile, // db abstract file interface
  mdb_bool* outCanOpenAsStore, // whether OpenFileStore() might succeed
  mdb_bool* outCanOpenAsPort, // whether OpenFilePort() might succeed
  mdbYarn* outFormatVersion)
{
  mdb_bool canOpenAsStore = morkBool_kFalse;
  mdb_bool canOpenAsPort = morkBool_kFalse;
  if ( outFormatVersion )
  {
    outFormatVersion->mYarn_Fill = 0;
  }
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseFactory(mev,
    /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    if ( ioFile && outCanOpenAsStore )
    {
      // right now always say true; later we should look for magic patterns
      canOpenAsStore = this->CanOpenMorkTextFile(ev, ioFile);
      canOpenAsPort = canOpenAsStore;
    }
    else
      ev->NilPointerError();
    
    outErr = ev->AsErr();
  }
  if ( outCanOpenAsStore )
    *outCanOpenAsStore = canOpenAsStore;
    
  if ( outCanOpenAsPort )
    *outCanOpenAsPort = canOpenAsPort;
    
  return outErr;
}
  
/*virtual*/ mdb_err
orkinFactory::OpenFileStore( // open an existing database
  nsIMdbEnv* mev, // context
  nsIMdbHeap* ioHeap, // can be nil to cause ev's heap attribute to be used
  // const char* inFilePath, // the file to open for general db usage
  nsIMdbFile* ioFile, // db abstract file interface
  const mdbOpenPolicy* inOpenPolicy, // runtime policies for using db
  nsIMdbThumb** acqThumb)
{
  mdb_err outErr = 0;
  nsIMdbThumb* outThumb = 0;
  morkEnv* ev = this->CanUseFactory(mev,
    /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    if ( !ioHeap ) // need to use heap from env?
      ioHeap = ev->mEnv_Heap;
    
    if ( ioFile && inOpenPolicy && acqThumb )
    {
      morkFactory* factory = (morkFactory*) this->mHandle_Object;
      morkStore* store = new(*ioHeap, ev)
        morkStore(ev, morkUsage::kHeap, ioHeap, factory, ioHeap);
        
      if ( store )
      {
        mork_bool frozen = morkBool_kFalse; // open store mutable access
        if ( store->OpenStoreFile(ev, frozen, ioFile, inOpenPolicy) )
        {
          morkThumb* thumb = morkThumb::Make_OpenFileStore(ev, ioHeap, store);
          if ( thumb )
          {
            outThumb = orkinThumb::MakeThumb(ev, thumb);
            thumb->CutStrongRef(ev);
          }
        }
        store->CutStrongRef(ev); // always cut ref (handle has its own ref)
      }
    }
    else
      ev->NilPointerError();
    
    outErr = ev->AsErr();
  }
  if ( acqThumb )
    *acqThumb = outThumb;
  return outErr;
}
// Call nsIMdbThumb::DoMore() until done, or until the thumb is broken, and
// then call nsIMdbFactory::ThumbToOpenStore() to get the store instance.
  
/*virtual*/ mdb_err
orkinFactory::ThumbToOpenStore( // redeem completed thumb from OpenFileStore()
  nsIMdbEnv* mev, // context
  nsIMdbThumb* ioThumb, // thumb from OpenFileStore() with done status
  nsIMdbStore** acqStore)
{
  mdb_err outErr = 0;
  nsIMdbStore* outStore = 0;
  morkEnv* ev = this->CanUseFactory(mev,
    /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    if ( ioThumb && acqStore )
    {
      orkinThumb* othumb = (orkinThumb*) ioThumb;
      if ( othumb->CanUseThumb(mev, /*inMutable*/ morkBool_kFalse, &outErr) )
      {
        morkThumb* thumb = (morkThumb*) othumb->mHandle_Object;
        morkStore* store = thumb->ThumbToOpenStore(ev);
        if ( store )
        {
          store->mStore_CanAutoAssignAtomIdentity = morkBool_kTrue;
          store->mStore_CanDirty = morkBool_kTrue;
          store->SetStoreAndAllSpacesCanDirty(ev, morkBool_kTrue);
          
          outStore = orkinStore::MakeStore(ev, store);
        }
      }
    }
    else
      ev->NilPointerError();
    
    outErr = ev->AsErr();
  }
  if ( acqStore )
    *acqStore = outStore;
  return outErr;
}

/*virtual*/ mdb_err
orkinFactory::CreateNewFileStore( // create a new db with minimal content
  nsIMdbEnv* mev, // context
  nsIMdbHeap* ioHeap, // can be nil to cause ev's heap attribute to be used
  // const char* inFilePath, // name of file which should not yet exist
  nsIMdbFile* ioFile, // db abstract file interface
  const mdbOpenPolicy* inOpenPolicy, // runtime policies for using db
  nsIMdbStore** acqStore)
{
  mdb_err outErr = 0;
  nsIMdbStore* outStore = 0;
  morkEnv* ev = this->CanUseFactory(mev,
    /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    if ( !ioHeap ) // need to use heap from env?
      ioHeap = ev->mEnv_Heap;
    
    if ( ioFile && inOpenPolicy && acqStore && ioHeap )
    {
      morkFactory* factory = (morkFactory*) this->mHandle_Object;
      morkStore* store = new(*ioHeap, ev)
        morkStore(ev, morkUsage::kHeap, ioHeap, factory, ioHeap);
        
      if ( store )
      {
        store->mStore_CanAutoAssignAtomIdentity = morkBool_kTrue;
        store->mStore_CanDirty = morkBool_kTrue;
        store->SetStoreAndAllSpacesCanDirty(ev, morkBool_kTrue);

        if ( store->CreateStoreFile(ev, ioFile, inOpenPolicy) )
          outStore = orkinStore::MakeStore(ev, store);
          
        store->CutStrongRef(ev); // always cut ref (handle has its own ref)
      }
    }
    else
      ev->NilPointerError();
    
    outErr = ev->AsErr();
  }
  if ( acqStore )
    *acqStore = outStore;
  return outErr;
}
// } ----- end store methods -----

// } ===== end nsIMdbFactory methods =====


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

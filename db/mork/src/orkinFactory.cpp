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
      this->GetGoodHandleObject(ev, inMutable, morkMagic_kFactory);
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
    nsIMdbHeap* outHeap = new orkinHeap();
    if ( !outHeap )
      ev->OutOfMemoryError();
  }
  MORK_ASSERT(acqHeap);
  if ( acqHeap )
    *acqHeap = outHeap;
  return outErr;
}
// } ----- end heap methods -----

// { ----- begin row methods -----
/*virtual*/ mdb_err
orkinFactory::MakeRow(nsIMdbEnv* mev, nsIMdbHeap* ioHeap,
  nsIMdbRow** acqRow)
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseFactory(mev,
    /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    outErr = ev->AsErr();
  }
  return outErr;
}
// ioHeap can be nil, causing the heap associated with ev to be used
// } ----- end row methods -----

// { ----- begin port methods -----
/*virtual*/ mdb_err
orkinFactory::CanOpenFilePort(
  nsIMdbEnv* mev, // context
  const char* inFilePath, // the file to investigate
  const mdbYarn* inFirst512Bytes,
  mdb_bool* outCanOpen, // whether OpenFilePort() might succeed
  mdbYarn* outFormatVersion)
{
  mdb_err outErr = 0;
  mdb_bool canOpenAsPort = morkBool_kFalse;
  morkEnv* ev = this->CanUseFactory(mev,
    /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    if ( inFilePath && inFirst512Bytes && outCanOpen )
    {
      canOpenAsPort = this->CanOpenMorkTextFile(ev, inFirst512Bytes);
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
  const char* inFilePath, // the file to open for readonly import
  const mdbOpenPolicy* inOpenPolicy, // runtime policies for using db
  nsIMdbThumb** acqThumb)
{
  mdb_err outErr = 0;
  nsIMdbThumb* outThumb = 0;
  morkEnv* ev = this->CanUseFactory(mev,
    /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    if ( inFilePath && inOpenPolicy && acqThumb )
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
  const mdbYarn* inFirst512Bytes)
{
  mork_bool outBool = morkBool_kFalse;
  mork_size headSize = MORK_STRLEN(morkWriter_kFileHeader);
  const mdbYarn* y = inFirst512Bytes;
  if ( y && y->mYarn_Buf && y->mYarn_Fill >= headSize )
  {
    mork_u1* buf = (mork_u1*) y->mYarn_Buf;
    outBool = ( MORK_MEMCMP(morkWriter_kFileHeader, buf, headSize) == 0 );
  }
  return outBool;
}

// { ----- begin store methods -----
/*virtual*/ mdb_err
orkinFactory::CanOpenFileStore(
  nsIMdbEnv* mev, // context
  const char* inFilePath, // the file to investigate
  const mdbYarn* inFirst512Bytes,
  mdb_bool* outCanOpenAsStore, // whether OpenFileStore() might succeed
  mdb_bool* outCanOpenAsPort, // whether OpenFilePort() might succeed
  mdbYarn* outFormatVersion)
{
  mdb_bool canOpenAsStore = morkBool_kFalse;
  mdb_bool canOpenAsPort = morkBool_kFalse;
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseFactory(mev,
    /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    if ( inFilePath && inFirst512Bytes && outCanOpenAsStore )
    {
      // right now always say true; later we should look for magic patterns
      canOpenAsStore = this->CanOpenMorkTextFile(ev, inFirst512Bytes);
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
  const char* inFilePath, // the file to open for general db usage
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
    
    if ( inFilePath && inOpenPolicy && acqThumb )
    {
      morkFactory* factory = (morkFactory*) this->mHandle_Object;
      morkStore* store = new(*ioHeap, ev)
        morkStore(ev, morkUsage::kHeap, ioHeap, factory, ioHeap);
        
      if ( store )
      {
        mork_bool frozen = morkBool_kFalse; // open store mutable access
        if ( store->OpenStoreFile(ev, frozen, inFilePath, inOpenPolicy) )
        {
          morkThumb* thumb = morkThumb::Make_OpenFileStore(ev, ioHeap, store);
          if ( thumb )
          {
            outThumb = orkinThumb::MakeThumb(ev, thumb);
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
  const char* inFilePath, // name of file which should not yet exist
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
    
    if ( inFilePath && inOpenPolicy && acqStore && ioHeap )
    {
      morkFactory* factory = (morkFactory*) this->mHandle_Object;
      morkStore* store = new(*ioHeap, ev)
        morkStore(ev, morkUsage::kHeap, ioHeap, factory, ioHeap);
        
      if ( store )
      {
        if ( store->CreateStoreFile(ev, inFilePath, inOpenPolicy) )
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

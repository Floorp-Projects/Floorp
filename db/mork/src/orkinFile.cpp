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

#ifndef _MORKFILE_
#include "morkFile.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _ORKINFILE_
#include "orkinFile.h"
#endif

#ifndef _MORKFILE_
#include "morkFile.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

/* public virtual*/
orkinFile:: ~orkinFile() // morkHandle destructor does everything
{
}

/*protected non-poly construction*/
orkinFile::orkinFile(morkEnv* ev, // morkUsage is morkUsage_kPool
    morkHandleFace* ioFace,    // must not be nil, cookie for this handle
    morkFile* ioObject)  // must not be nil, the object for this handle
: morkHandle(ev, ioFace, ioObject, morkMagic_kFile)
{
  // do not modify mNode_Derived; leave it equal to morkDerived_kHandle
}


/*static */ orkinFile*
orkinFile::MakeFile(morkEnv* ev, morkFile* ioObject)
{
  mork_bool isEnv = ev->IsEnv();
  MORK_ASSERT(isEnv);
  if ( isEnv )
  {
    morkHandleFace* face = ev->NewHandle(sizeof(orkinFile));
    if ( face )
      return new(face) orkinFile(ev, face, ioObject);
    else
      ev->OutOfMemoryError();
  }
    
  return (orkinFile*) 0;
}

morkEnv*
orkinFile::CanUseFile(nsIMdbEnv* mev,
  mork_bool inMutable, mdb_err* outErr) const
{
  morkEnv* outEnv = 0;
  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {  
    morkFile* self = (morkFile*)
      this->GetGoodHandleObject(ev, inMutable, morkMagic_kFile,
        /*inClosedOkay*/ morkBool_kFalse);
    if ( self )
    {
      if ( self->IsFile() )
        outEnv = ev;
      else
        self->NonFileTypeError(ev);
    }
    *outErr = ev->AsErr();
  }
  MORK_ASSERT(outEnv);
  return outEnv;
}


// { ===== begin nsIMdbISupports methods =====
/*virtual*/ mdb_err
orkinFile::AddRef() // add strong ref with no
{
  morkEnv* ev = mHandle_Env;
  if ( ev && ev->IsEnv() )
    return this->Handle_AddStrongRef(ev->AsMdbEnv());
  else
    return morkEnv_kNonEnvTypeError;
}

/*virtual*/ mdb_err
orkinFile::Release() // cut strong ref
{
  morkEnv* ev = mHandle_Env;
  if ( ev && ev->IsEnv() )
    return this->Handle_CutStrongRef(ev->AsMdbEnv());
  else
    return morkEnv_kNonEnvTypeError;
}
// } ===== end nsIMdbISupports methods =====

// { ===== begin nsIMdbObject methods =====

// { ----- begin attribute methods -----
/*virtual*/ mdb_err
orkinFile::IsFrozenMdbObject(nsIMdbEnv* mev, mdb_bool* outIsReadonly)
{
  return this->Handle_IsFrozenMdbObject(mev, outIsReadonly);
}
// same as nsIMdbPort::GetIsPortReadonly() when this object is inside a port.
// } ----- end attribute methods -----

// { ----- begin factory methods -----
/*virtual*/ mdb_err
orkinFile::GetMdbFactory(nsIMdbEnv* mev, nsIMdbFactory** acqFactory)
{
  return this->Handle_GetMdbFactory(mev, acqFactory);
} 
// } ----- end factory methods -----

// { ----- begin ref counting for well-behaved cyclic graphs -----
/*virtual*/ mdb_err
orkinFile::GetWeakRefCount(nsIMdbEnv* mev, // weak refs
  mdb_count* outCount)
{
  return this->Handle_GetWeakRefCount(mev, outCount);
}  
/*virtual*/ mdb_err
orkinFile::GetStrongRefCount(nsIMdbEnv* mev, // strong refs
  mdb_count* outCount)
{
  return this->Handle_GetStrongRefCount(mev, outCount);
}

/*virtual*/ mdb_err
orkinFile::AddWeakRef(nsIMdbEnv* mev)
{
  return this->Handle_AddWeakRef(mev);
}
/*virtual*/ mdb_err
orkinFile::AddStrongRef(nsIMdbEnv* mev)
{
  return this->Handle_AddStrongRef(mev);
}

/*virtual*/ mdb_err
orkinFile::CutWeakRef(nsIMdbEnv* mev)
{
  return this->Handle_CutWeakRef(mev);
}
/*virtual*/ mdb_err
orkinFile::CutStrongRef(nsIMdbEnv* mev)
{
  return this->Handle_CutStrongRef(mev);
}

/*virtual*/ mdb_err
orkinFile::CloseMdbObject(nsIMdbEnv* mev)
{
  return this->Handle_CloseMdbObject(mev);
}

/*virtual*/ mdb_err
orkinFile::IsOpenMdbObject(nsIMdbEnv* mev, mdb_bool* outOpen)
{
  return this->Handle_IsOpenMdbObject(mev, outOpen);
}
// } ----- end ref counting -----

// } ===== end nsIMdbObject methods =====

// { ===== begin nsIMdbFile methods =====

// { ----- begin pos methods -----
/*virtual*/ mdb_err
orkinFile::Tell(nsIMdbEnv* mev, mdb_pos* outPos)
{
  mdb_err outErr = 0;
  mdb_pos pos = -1;
  morkEnv* ev = this->CanUseFile(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkFile* file = (morkFile*) mHandle_Object;
    pos = file->Tell(ev);
    outErr = ev->AsErr();
  }
  if ( outPos )
    *outPos = pos;
  return outErr;
}

/*virtual*/ mdb_err
orkinFile::Seek(nsIMdbEnv* mev, mdb_pos inPos)
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseFile(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkFile* file = (morkFile*) mHandle_Object;
    file->Seek(ev, inPos);
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinFile::Eof(nsIMdbEnv* mev, mdb_pos* outPos)
{
  mdb_err outErr = 0;
  mdb_pos pos = -1;
  morkEnv* ev = this->CanUseFile(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkFile* file = (morkFile*) mHandle_Object;
    pos = file->Length(ev);
    outErr = ev->AsErr();
  }
  if ( outPos )
    *outPos = pos;
  return outErr;
}

// } ----- end pos methods -----

// { ----- begin read methods -----
/*virtual*/ mdb_err
orkinFile::Read(nsIMdbEnv* mev, void* outBuf, mdb_size inSize,
  mdb_size* outActualSize)
{
  mdb_err outErr = 0;
  mdb_size actualSize = 0;
  morkEnv* ev = this->CanUseFile(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkFile* file = (morkFile*) mHandle_Object;
    actualSize = file->Read(ev, outBuf, inSize);
    outErr = ev->AsErr();
  }
  if ( outActualSize )
    *outActualSize = actualSize;
  return outErr;
}

/*virtual*/ mdb_err
orkinFile::Get(nsIMdbEnv* mev, void* outBuf, mdb_size inSize,
  mdb_pos inPos, mdb_size* outActualSize)
{
  mdb_err outErr = 0;
  mdb_size actualSize = 0;
  morkEnv* ev = this->CanUseFile(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkFile* file = (morkFile*) mHandle_Object;
    file->Seek(ev, inPos);
    if ( ev->Good() )
      actualSize = file->Read(ev, outBuf, inSize);
    outErr = ev->AsErr();
  }
  if ( outActualSize )
    *outActualSize = actualSize;
  return outErr;
}

// } ----- end read methods -----
  
// { ----- begin write methods -----
/*virtual*/ mdb_err
orkinFile::Write(nsIMdbEnv* mev, const void* inBuf, mdb_size inSize,
  mdb_size* outActualSize)
{
  mdb_err outErr = 0;
  mdb_size actualSize = 0;
  morkEnv* ev = this->CanUseFile(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkFile* file = (morkFile*) mHandle_Object;
    actualSize = file->Write(ev, inBuf, inSize);
    outErr = ev->AsErr();
  }
  if ( outActualSize )
    *outActualSize = actualSize;
  return outErr;
}

/*virtual*/ mdb_err
orkinFile::Put(nsIMdbEnv* mev, const void* inBuf, mdb_size inSize,
  mdb_pos inPos, mdb_size* outActualSize)
{
  mdb_err outErr = 0;
  mdb_size actualSize = 0;
  morkEnv* ev = this->CanUseFile(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkFile* file = (morkFile*) mHandle_Object;
    file->Seek(ev, inPos);
    if ( ev->Good() )
      actualSize = file->Write(ev, inBuf, inSize);
    outErr = ev->AsErr();
  }
  if ( outActualSize )
    *outActualSize = actualSize;
  return outErr;
}

/*virtual*/ mdb_err
orkinFile::Flush(nsIMdbEnv* mev)
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseFile(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkFile* file = (morkFile*) mHandle_Object;
    file->Flush(ev);
    outErr = ev->AsErr();
  }
  return outErr;
}

// } ----- end attribute methods -----
  
// { ----- begin path methods -----
/*virtual*/ mdb_err
orkinFile::Path(nsIMdbEnv* mev, mdbYarn* outFilePath)
{
  mdb_err outErr = 0;
  if ( outFilePath )
    outFilePath->mYarn_Fill = 0;
  morkEnv* ev = this->CanUseFile(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkFile* file = (morkFile*) mHandle_Object;
    ev->StringToYarn(file->GetFileNameString(), outFilePath);
    outErr = ev->AsErr();
  }
  return outErr;
}

// } ----- end path methods -----
  
// { ----- begin replacement methods -----

/*virtual*/ mdb_err
orkinFile::Steal(nsIMdbEnv* mev, nsIMdbFile* ioThief)
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseFile(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    if ( ioThief )
    {
      // orkinFile* thief = (orkinFile*) ioThief; // unsafe cast -- must type check:
      // thief->CanUseFile(mev, /*inMutable*/ morkBool_kFalse, &outErr);
      
      morkFile* file = (morkFile*) mHandle_Object;
      file->Steal(ev, ioThief);
      outErr = ev->AsErr();
    }
    else
      ev->NilPointerError();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinFile::Thief(nsIMdbEnv* mev, nsIMdbFile** acqThief)
{
  mdb_err outErr = 0;
  nsIMdbFile* outThief = 0;
  morkEnv* ev = this->CanUseFile(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkFile* file = (morkFile*) mHandle_Object;
    outThief = file->GetThief();
    if ( outThief )
      outThief->AddStrongRef(ev->AsMdbEnv());
    outErr = ev->AsErr();
  }
  if ( acqThief )
    *acqThief = outThief;
  return outErr;
}

// } ----- end replacement methods -----

// { ----- begin versioning methods -----

/*virtual*/ mdb_err
orkinFile::BecomeTrunk(nsIMdbEnv* mev)
// If this file is a file version branch created by calling AcquireBud(),
// BecomeTrunk() causes this file's content to replace the original
// file's content, typically by assuming the original file's identity.
// This default implementation of BecomeTrunk() does nothing, and this
// is appropriate behavior for files which are not branches, and is
// also the right behavior for files returned from AcquireBud() which are
// in fact the original file that has been truncated down to zero length.
{
  mdb_err outErr = 0;
  morkEnv* ev = this->CanUseFile(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkFile* file = (morkFile*) mHandle_Object;
    file->BecomeTrunk(ev);
    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinFile::AcquireBud(nsIMdbEnv* mev, nsIMdbHeap* ioHeap,
  nsIMdbFile** acqBud) // acquired file for new version of content
// AcquireBud() starts a new "branch" version of the file, empty of content,
// so that a new version of the file can be written.  This new file
// can later be told to BecomeTrunk() the original file, so the branch
// created by budding the file will replace the original file.  Some
// file subclasses might initially take the unsafe but expedient
// approach of simply truncating this file down to zero length, and
// then returning the same morkFile pointer as this, with an extra
// reference count increment.  Note that the caller of AcquireBud() is
// expected to eventually call CutStrongRef() on the returned file
// in order to release the strong reference.  High quality versions
// of morkFile subclasses will create entirely new files which later
// are renamed to become the old file, so that better transactional
// behavior is exhibited by the file, so crashes protect old files.
// Note that AcquireBud() is an illegal operation on readonly files.
{
  mdb_err outErr = 0;
  nsIMdbFile* outBud = 0;
  morkEnv* ev = this->CanUseFile(mev, /*inMutable*/ morkBool_kFalse, &outErr);
  if ( ev )
  {
    morkFile* file = (morkFile*) mHandle_Object;
    morkFile* bud = file->AcquireBud(ev, ioHeap);
    if ( bud )
    {
      outBud = bud->AcquireFileHandle(ev);
      bud->CutStrongRef(ev);
    }
    outErr = ev->AsErr();
  }
  if ( acqBud )
    *acqBud = outBud;
  return outErr;
}
// } ----- end versioning methods -----

// } ===== end nsIMdbFile methods =====


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

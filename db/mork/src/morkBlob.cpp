/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- 
 * 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape 
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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

#ifndef _MORKENV_
#include "morkEnv.h"
#endif


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

/*static*/ void
morkBuf::NilBufBodyError(morkEnv* ev)
{
  ev->NewError("nil mBuf_Body");
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

/*static*/ void
morkBlob::BlobFillOverSizeError(morkEnv* ev)
{
  ev->NewError("mBuf_Fill > mBlob_Size");
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

mork_bool
morkBlob::GrowBlob(morkEnv* ev, nsIMdbHeap* ioHeap, mork_size inNewSize)
{
  if ( ioHeap )
  {
    if ( !mBuf_Body ) // no body? implies zero sized?
      mBlob_Size = 0;
      
    if ( mBuf_Fill > mBlob_Size ) // fill more than size?
    {
      ev->NewWarning("mBuf_Fill > mBlob_Size");
      mBuf_Fill = mBlob_Size;
    }
      
    if ( inNewSize > mBlob_Size ) // need to allocate larger blob?
    {
      mork_u1* body = 0;
      ioHeap->Alloc(ev->AsMdbEnv(), inNewSize, (void**) &body);
      if ( body && ev->Good() )
      {
        void* oldBody = mBuf_Body;
        if ( mBlob_Size ) // any old content to transfer?
          MORK_MEMCPY(body, oldBody, mBlob_Size);
        
        mBlob_Size = inNewSize; // install new size
        mBuf_Body = body; // install new body
        
        if ( oldBody ) // need to free old buffer body?
          ioHeap->Free(ev->AsMdbEnv(), oldBody);
      }
    }
  }
  else
    ev->NilPointerError();
    
  if ( ev->Good() && mBlob_Size < inNewSize )
    ev->NewError("mBlob_Size < inNewSize");
    
  return ev->Good();
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

morkCoil::morkCoil(morkEnv* ev, nsIMdbHeap* ioHeap)
{
  mBuf_Body = 0;
  mBuf_Fill = 0;
  mBlob_Size = 0;
  mText_Form = 0;
  mCoil_Heap = ioHeap;
  if ( !ioHeap )
    ev->NilPointerError();
}

void
morkCoil::CloseCoil(morkEnv* ev)
{
  void* body = mBuf_Body;
  nsIMdbHeap* heap = mCoil_Heap;

  mBuf_Body = 0;
  mCoil_Heap = 0;
  
  if ( body && heap )
  {
    heap->Free(ev->AsMdbEnv(), body);
  }
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

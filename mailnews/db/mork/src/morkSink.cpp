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

#ifndef _MORKSINK_
#include "morkSink.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

#ifndef _MORKBLOB_
#include "morkBlob.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

/*virtual*/ morkSink::~morkSink()
{
  mSink_At = 0;
  mSink_End = 0;
}

/*virtual*/ void
morkSpool::FlushSink(morkEnv* ev) // sync mSpool_Coil->mBuf_Fill
{
  morkCoil* coil = mSpool_Coil;
  if ( coil )
  {
    mork_u1* body = (mork_u1*) coil->mBuf_Body;
    if ( body )
    {
      mork_u1* at = mSink_At;
      mork_u1* end = mSink_End;
      if ( at >= body && at <= end ) // expected cursor order?
      {
        mork_fill fill = at - body; // current content size
        if ( fill <= coil->mBlob_Size )
          coil->mBuf_Fill = fill;
        else
        {
          coil->BlobFillOverSizeError(ev);
          coil->mBuf_Fill = coil->mBlob_Size; // make it safe
        }
      }
      else
        this->BadSpoolCursorOrderError(ev);
    }
    else
      coil->NilBufBodyError(ev);
  }
  else
    this->NilSpoolCoilError(ev);
}

/*virtual*/ void
morkSpool::SpillPutc(morkEnv* ev, int c) // grow coil and write byte
{
  morkCoil* coil = mSpool_Coil;
  if ( coil )
  {
    mork_u1* body = (mork_u1*) coil->mBuf_Body;
    if ( body )
    {
      mork_u1* at = mSink_At;
      mork_u1* end = mSink_End;
      if ( at >= body && at <= end ) // expected cursor order?
      {
        mork_size size = coil->mBlob_Size;
        mork_fill fill = at - body; // current content size
        if ( fill <= size ) // less content than medium size?
        {
          coil->mBuf_Fill = fill;
          if ( at >= end ) // need to grow the coil?
          {
            if ( size > 2048 ) // grow slower over 2K?
              size += 512;
            else
            {
              mork_size growth = ( size * 4 ) / 3; // grow by 33%
              if ( growth < 64 ) // grow faster under (64 * 3)?
                growth = 64;
              size += growth;
            }
            if ( coil->GrowCoil(ev, size) ) // made coil bigger?
            {
              body = (mork_u1*) coil->mBuf_Body;
              if ( body ) // have a coil body?
              {
                mSink_At = at = body + fill;
                mSink_End = end = body + coil->mBlob_Size;
              }
              else
                coil->NilBufBodyError(ev);
            }
          }
          if ( ev->Good() ) // seem ready to write byte c?
          {
            if ( at < end ) // morkSink::Putc() would succeed?
            {
              *at++ = c;
              mSink_At = at;
              coil->mBuf_Fill = fill + 1;
            }
            else
              this->BadSpoolCursorOrderError(ev);
          }
        }
        else // fill exceeds size
        {
          coil->BlobFillOverSizeError(ev);
          coil->mBuf_Fill = coil->mBlob_Size; // make it safe
        }
      }
      else
        this->BadSpoolCursorOrderError(ev);
    }
    else
      coil->NilBufBodyError(ev);
  }
  else
    this->NilSpoolCoilError(ev);
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// public: // public non-poly morkSink methods

/*virtual*/
morkSpool::~morkSpool()
// Zero all slots to show this sink is disabled, but destroy no memory.
// Note it is typically unnecessary to flush this coil sink, since all
// content is written directly to the coil without any buffering.
{
  mSink_At = 0;
  mSink_End = 0;
  mSpool_Coil = 0;
}

morkSpool::morkSpool(morkEnv* ev, morkCoil* ioCoil)
// After installing the coil, calls Seek(ev, 0) to prepare for writing.
: morkSink()
, mSpool_Coil( 0 )
{
  mSink_At = 0; // set correctly later in Seek()
  mSink_End = 0; // set correctly later in Seek()
  
  if ( ev->Good() )
  {
    if ( ioCoil )
    {
      mSpool_Coil = ioCoil;
      this->Seek(ev, /*pos*/ 0);
    }
    else
      ev->NilPointerError();
  }
}

// ----- All boolean return values below are equal to ev->Good(): -----

/*static*/ void
morkSpool::BadSpoolCursorOrderError(morkEnv* ev)
{
  ev->NewError("bad morkSpool cursor order");
}

/*static*/ void
morkSpool::NilSpoolCoilError(morkEnv* ev)
{
  ev->NewError("nil mSpool_Coil");
}

mork_bool
morkSpool::Seek(morkEnv* ev, mork_pos inPos)
// Changed the current write position in coil's buffer to inPos.
// For example, to start writing the coil from scratch, use inPos==0.
{
  morkCoil* coil = mSpool_Coil;
  if ( coil )
  {
    mork_size minSize = inPos + 64;
    
    if ( coil->mBlob_Size < minSize )
      coil->GrowCoil(ev, minSize);
      
    if ( ev->Good() )
    {
      coil->mBuf_Fill = inPos;
      mork_u1* body = (mork_u1*) coil->mBuf_Body;
      if ( body )
      {
        mSink_At = body + inPos;
        mSink_End = body + coil->mBlob_Size;
      }
      else
        coil->NilBufBodyError(ev);
    }
  }
  else
    this->NilSpoolCoilError(ev);
    
  return ev->Good();
}

mork_bool
morkSpool::Write(morkEnv* ev, const void* inBuf, mork_size inSize)
// write inSize bytes of inBuf to current position inside coil's buffer
{
  // This method is conceptually very similar to morkStream::Write(),
  // and this code was written while looking at that method for clues.
 
  morkCoil* coil = mSpool_Coil;
  if ( coil )
  {
    mork_u1* body = (mork_u1*) coil->mBuf_Body;
    if ( body )
    {
      if ( inBuf && inSize ) // anything to write?
      {
        mork_u1* at = mSink_At;
        mork_u1* end = mSink_End;
        if ( at >= body && at <= end ) // expected cursor order?
        {
          // note coil->mBuf_Fill can be stale after morkSink::Putc():
          mork_pos fill = at - body; // current content size
          mork_num space = end - at; // space left in body
          if ( space < inSize ) // not enough to hold write?
          {
            mork_size minGrowth = space + 16;
            mork_size minSize = coil->mBlob_Size + minGrowth;
            if ( coil->GrowCoil(ev, minSize) )
            {
              body = (mork_u1*) coil->mBuf_Body;
              if ( body )
              {
                mSink_At = at = body + fill;
                mSink_End = end = body + coil->mBlob_Size;
                space = end - at; // space left in body
              }
              else
                coil->NilBufBodyError(ev);
            }
          }
          if ( ev->Good() )
          {
            if ( space >= inSize ) // enough room to hold write?
            {
              MORK_MEMCPY(at, inBuf, inSize); // into body
              mSink_At = at + inSize; // advance past written bytes
              coil->mBuf_Fill = fill + inSize; // "flush" to fix fill
            }
            else
              ev->NewError("insufficient morkSpool space");
          }
        }
        else
          this->BadSpoolCursorOrderError(ev);
      }
    }
    else
      coil->NilBufBodyError(ev);
  }
  else
    this->NilSpoolCoilError(ev);
  
  return ev->Good();
}

mork_bool
morkSpool::PutString(morkEnv* ev, const char* inString)
// call Write() with inBuf=inString and inSize=strlen(inString),
// unless inString is null, in which case we then do nothing at all.
{
  if ( inString )
  {
    mork_size size = MORK_STRLEN(inString);
    this->Write(ev, inString, size);
  }
  return ev->Good();
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

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

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

/*virtual*/ morkSink::~morkSink()
{
  mSink_At = 0;
  mSink_End = 0;
}

/*virtual*/ void
morkSpoolSink::FlushSink(morkEnv* ev) // probably does nothing
{
  ev->StubMethodOnlyError();
}

/*virtual*/ void
morkSpoolSink::SpillPutc(morkEnv* ev, int c) // grow spool and write byte
{
  ev->StubMethodOnlyError();
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// public: // public non-poly morkSink methods

/*virtual*/
morkSpoolSink::~morkSpoolSink()
// Zero all slots to show this sink is disabled, but destroy no memory.
// Note it is typically unnecessary to flush this spool sink, since all
// content is written directly to the spool without any buffering.
{
}

morkSpoolSink::morkSpoolSink(morkEnv* ev, morkSpool* ioSpool)
// After installing the spool, calls Seek(ev, 0) to prepare for writing.
: morkSink()
, mSpoolSink_Spool( 0 )
{
  if ( ev->Good() )
  {
    if ( ioSpool )
    {
      // ev->StubMethodOnlyError();
      mSink_At = 0;
      mSink_End = 0;
      mSpoolSink_Spool = ioSpool;
    }
    else
      ev->NilPointerError();
  }
}

// ----- All boolean return values below are equal to ev->Good(): -----

mork_bool
morkSpoolSink::Seek(morkEnv* ev, mork_pos inPos)
// Changed the current write position in spool's buffer to inPos.
// For example, to start writing the spool from scratch, use inPos==0.
{
  ev->StubMethodOnlyError();
  return ev->Good();
}

mork_bool
morkSpoolSink::Write(morkEnv* ev, const void* inBuf, mork_size inSize)
// write inSize bytes of inBuf to current position inside spool's buffer
{
  ev->StubMethodOnlyError();
  return ev->Good();
}

mork_bool
morkSpoolSink::PutString(morkEnv* ev, const char* inString)
// call Write() with inBuf=inString and inSize=strlen(inString),
// unless inString is null, in which case we then do nothing at all.
{
  ev->StubMethodOnlyError();
  return ev->Good();
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

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

#ifndef _MORKBLOB_
#include "morkBlob.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

morkSpool::morkSpool(morkEnv* ev, nsIMdbHeap* ioHeap)
{
  mBuf_Body = 0;
  mBuf_Fill = 0;
  mBlob_Size = 0;
  mText_Form = 0;
  mSpool_Heap = ioHeap;
  if ( !ioHeap )
    ev->NilPointerError();
}

void
morkSpool::CloseSpool(morkEnv* ev)
{
  void* body = mBuf_Body;
  nsIMdbHeap* heap = mSpool_Heap;
  
  if ( body && heap )
  {
    heap->Free(ev->AsMdbEnv(), body);
  }
  mBuf_Body = 0;
  mSpool_Heap = 0;
}

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

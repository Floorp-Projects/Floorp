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

#ifndef _ORKINHEAP_
#include "orkinHeap.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789


orkinHeap::orkinHeap() // does nothing
{
}

/*virtual*/
orkinHeap::~orkinHeap() // does nothing
{
}

// { ===== begin nsIMdbHeap methods =====
/*virtual*/ mdb_err
orkinHeap::Alloc(nsIMdbEnv* ev, // allocate a piece of memory
  mdb_size inSize,   // requested size of new memory block 
  void** outBlock)  // memory block of inSize bytes, or nil
{
  MORK_USED_1(ev);
  mdb_err outErr = 0;
  void* block = new char[ inSize ];
  if ( !block )
    outErr = morkEnv_kOutOfMemoryError;
    
  MORK_ASSERT(outBlock);
  if ( outBlock )
    *outBlock = block;
  return outErr;
}
  
/*virtual*/ mdb_err
orkinHeap::Free(nsIMdbEnv* ev, // free block allocated earlier by Alloc()
  void* inBlock)
{
  MORK_USED_1(ev);
  MORK_ASSERT(inBlock);
  if ( inBlock )
    delete [] (char *)inBlock;
    
  return 0;
}

/*virtual*/ mdb_err
orkinHeap::AddStrongRef(nsIMdbEnv* ev) // does nothing
{
  MORK_USED_1(ev);
  return 0;
}

/*virtual*/ mdb_err
orkinHeap::CutStrongRef(nsIMdbEnv* ev) // does nothing
{
  MORK_USED_1(ev);
  return 0;
}

// } ===== end nsIMdbHeap methods =====

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

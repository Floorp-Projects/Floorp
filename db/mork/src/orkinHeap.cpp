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

#ifndef _ORKINHEAP_
#include "orkinHeap.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789


orkinHeap::orkinHeap() // does nothing
#ifdef MORK_DEBUG_HEAP_STATS
  : sHeap_AllocCount( 0 )
  , sHeap_FreeCount( 0 )
  , sHeap_BlockCount( 0 )
  
  , sHeap_BlockVolume( 0 )
  , sHeap_HighWaterVolume( 0 )
  , sHeap_HighWaterTenKilo( 0 )
  , sHeap_HighWaterHundredKilo( 0 )
#endif /*MORK_DEBUG_HEAP_STATS*/
{
}

/*virtual*/
orkinHeap::~orkinHeap() // does nothing
{
}

// { ===== begin nsIMdbHeap methods =====
/*virtual*/ mdb_err
orkinHeap::Alloc(nsIMdbEnv* mev, // allocate a piece of memory
  mdb_size inSize,   // requested size of new memory block 
  void** outBlock)  // memory block of inSize bytes, or nil
{
#ifdef MORK_DEBUG_HEAP_STATS
  mdb_size realSize = inSize;
  inSize += 8; // sizeof(mork_u4) * 2
  ++sHeap_AllocCount;
#endif /*MORK_DEBUG_HEAP_STATS*/

  MORK_USED_1(mev);
  mdb_err outErr = 0;
  void* block = ::operator new(inSize);
  if ( !block )
    outErr = morkEnv_kOutOfMemoryError;
#ifdef MORK_DEBUG_HEAP_STATS
  else
  {
    mork_u4* array = (mork_u4*) block;
    *array++ = realSize;
    *array++ = orkinHeap_kTag;
    block = array;
    ++sHeap_BlockCount;
    mork_num blockVol = sHeap_BlockVolume + realSize;
    sHeap_BlockVolume = blockVol;
    if ( blockVol > sHeap_HighWaterVolume )
    {
      sHeap_HighWaterVolume = blockVol;
      
      mork_num tenKiloVol = blockVol / (10 * 1024);
      if ( tenKiloVol > sHeap_HighWaterTenKilo )
      {
        sHeap_HighWaterTenKilo = tenKiloVol;
      
        mork_num hundredKiloVol = blockVol / (100 * 1024);
        if ( hundredKiloVol > sHeap_HighWaterHundredKilo )
          sHeap_HighWaterHundredKilo = hundredKiloVol;
      }
    }
  }
#endif /*MORK_DEBUG_HEAP_STATS*/
    
  MORK_ASSERT(outBlock);
  if ( outBlock )
    *outBlock = block;
  return outErr;
}
  
/*virtual*/ mdb_err
orkinHeap::Free(nsIMdbEnv* mev, // free block allocated earlier by Alloc()
  void* inBlock)
{
#ifdef MORK_DEBUG_HEAP_STATS
  ++sHeap_FreeCount;
#endif /*MORK_DEBUG_HEAP_STATS*/

  MORK_USED_1(mev);
  MORK_ASSERT(inBlock);
  if ( inBlock )
  {
#ifdef MORK_DEBUG_HEAP_STATS
    morkEnv* ev = morkEnv::FromMdbEnv(mev);
    mork_u4* array = (mork_u4*) inBlock;
    if ( *--array != orkinHeap_kTag )
    {
      if ( ev )
        ev->NewWarning("heap block tag not hEaP");
    }
    mork_u4 realSize = *--array;
    inBlock = array;
    
    if ( sHeap_BlockCount )
      --sHeap_BlockCount;
    else if ( ev ) 
      ev->NewWarning("sHeap_BlockCount underflow");
    
    if ( sHeap_BlockVolume >= realSize )
      sHeap_BlockVolume -= realSize;
    else if ( ev )
    {
      sHeap_BlockVolume = 0;
      ev->NewWarning("sHeap_BlockVolume underflow");
    }
#endif /*MORK_DEBUG_HEAP_STATS*/
    
    ::operator delete(inBlock);
  }
  return 0;
}

/*virtual*/ mdb_err
orkinHeap::HeapAddStrongRef(nsIMdbEnv* ev) // does nothing
{
  MORK_USED_1(ev);
  return 0;
}

/*virtual*/ mdb_err
orkinHeap::HeapCutStrongRef(nsIMdbEnv* ev) // does nothing
{
  MORK_USED_1(ev);
  return 0;
}

// } ===== end nsIMdbHeap methods =====

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

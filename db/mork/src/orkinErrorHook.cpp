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

#ifndef _ORKINERRORHOOK_
#include "orkinErrorHook.h"
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
  MORK_ASSERT(inBlock);
  if ( inBlock )
    delete [] inBlock;
    
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

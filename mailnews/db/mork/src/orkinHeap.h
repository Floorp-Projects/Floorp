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

#ifndef _ORKINHEAP_
#define _ORKINHEAP_ 1

#ifndef _MDB_
#include "mdb.h"
#endif

#ifndef _MORK_
#include "mork.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

/*| orkinHeap: 
|*/
class orkinHeap : public nsIMdbHeap { //
  
public:
  orkinHeap(); // does nothing
  virtual ~orkinHeap(); // does nothing
    
private: // copying is not allowed
  orkinHeap(const orkinHeap& other);
  orkinHeap& operator=(const orkinHeap& other);

public:

// { ===== begin nsIMdbHeap methods =====
  virtual mdb_err Alloc(nsIMdbEnv* ev, // allocate a piece of memory
    mdb_size inSize,   // requested size of new memory block 
    void** outBlock);  // memory block of inSize bytes, or nil
    
  virtual mdb_err Free(nsIMdbEnv* ev, // free block allocated earlier by Alloc()
    void* inBlock);
    
  virtual mdb_err AddStrongRef(nsIMdbEnv* ev); // does nothing
  virtual mdb_err CutStrongRef(nsIMdbEnv* ev); // does nothing
// } ===== end nsIMdbHeap methods =====

};

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _ORKINHEAP_ */

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

#ifndef _ORKINCOMPARE_
#define _ORKINCOMPARE_ 1

#ifndef _MDB_
#include "mdb.h"
#endif

#ifndef _MORK_
#include "mork.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

/*| orkinCompare: 
|*/
class orkinCompare : public nsIMdbCompare { //
  
public:
  orkinCompare(); // does nothing
  virtual ~orkinCompare(); // does nothing
    
private: // copying is not allowed
  orkinCompare(const orkinCompare& other);
  orkinCompare& operator=(const orkinCompare& other);

public:

// { ===== begin nsIMdbCompare methods =====
  virtual mdb_err Order(nsIMdbEnv* ev,      // compare first to second yarn
    const mdbYarn* inFirst,   // first yarn in comparison
    const mdbYarn* inSecond,  // second yarn in comparison
    mdb_order* outOrder);     // negative="<", zero="=", positive=">"
    
  virtual mdb_err AddStrongRef(nsIMdbEnv* ev); // does nothing
  virtual mdb_err CutStrongRef(nsIMdbEnv* ev); // does nothing
// } ===== end nsIMdbCompare methods =====

};

extern mdb_order // standard yarn comparison for nsIMdbCompare::ORder()
mdbYarn_Order(const mdbYarn* inSelf, morkEnv* ev, const mdbYarn* inSecond);

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

#endif /* _ORKINCOMPARE_ */

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

#ifndef _ORKINCOMPARE_
#include "orkinCompare.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789


orkinCompare::orkinCompare() // does nothing
{
}

/*virtual*/
orkinCompare::~orkinCompare() // does nothing
{
}


mdb_order // standard yarn comparison for nsIMdbCompare::Order()
mdbYarn_Order(const mdbYarn* inSelf, morkEnv* ev, const mdbYarn* inYarn)
{
  // This code is a close copy of public domain Mithril's AgYarn_CompareLen().
  // Note all comments are from the original public domain Mithril code.
  
  mork_u1* self = (mork_u1*) inSelf->mYarn_Buf;
  mork_u1* yarn = (mork_u1*) inYarn->mYarn_Buf;
  
  mdb_fill selfFill = inSelf->mYarn_Fill;
  mdb_fill yarnFill = inYarn->mYarn_Fill;
  
  if ( selfFill && yarnFill ) /* neither yarn is empty? */
  {
    register int a; /* a byte from self */
    register int b; /* a byte from yarn */
    
    /* predecrement is normally used in loop tests to minimize instructions: */
    ++selfFill; /* prepare for predecrement */
    ++yarnFill; /* prepare for predecrement */
    /* we check only self len at loop top, but yarn len check must follow: */
    
    while ( --selfFill ) /* another byte in self? */
    {
      if ( !--yarnFill ) /* yarn runs out first? self is greater (pos)? */
        return 1;
      
      b = (mork_u1) *yarn++;  /* next byte in other yarn */
      a = (mork_u1) *self++;  /* next byte in self yarn */

      if ( a != b ) /* found first different byte? */
        return ( a - b ); /* return relative order */
    }
    /* if remaining yarn len is exactly 1, it runs out at same time as self: */
    return ( yarnFill == 1 )? 0 : -1; /* yarn is same len? or self is less? */
  }
  else
    return ((mdb_order) selfFill) - ((mdb_order) yarnFill);
}

// { ===== begin nsIMdbCompare methods =====
/*virtual*/ mdb_err
orkinCompare::Order(nsIMdbEnv* mev, // compare first to second yarn
  const mdbYarn* inFirst,   // first yarn in comparison
  const mdbYarn* inSecond,  // second yarn in comparison
  mdb_order* outOrder)      // negative="<", zero="=", positive=">"
{
  mdb_err outErr = 1; // nonzero means FAILURE

  morkEnv* ev = morkEnv::FromMdbEnv(mev);
  if ( ev )
  {
    if ( inFirst && inSecond && outOrder )
    {
      *outOrder = mdbYarn_Order(inFirst, ev, inSecond);
    }
    else
      ev->NilPointerError();

    outErr = ev->AsErr();
  }
  return outErr;
}

/*virtual*/ mdb_err
orkinCompare::AddStrongRef(nsIMdbEnv* ev) // does nothing
{
  MORK_USED_1(ev);
  return 0;
}

/*virtual*/ mdb_err
orkinCompare::CutStrongRef(nsIMdbEnv* ev) // does nothing
{
  MORK_USED_1(ev);
  return 0;
}

// } ===== end nsIMdbCompare methods =====

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

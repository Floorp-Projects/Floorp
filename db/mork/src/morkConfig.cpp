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

#ifndef _MORKCONFIG_
#include "morkConfig.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

/* ----- ----- ----- ----- MORK_OBSOLETE ----- ----- ----- ----- */
#if defined(MORK_OBSOLETE) || defined(MORK_ALONE)

#include <Types.h>
//#include "plstr.h"
  
  static void // copied almost verbatim from the IronDoc debugger sources:
  mork_mac_break_string(register const char* inMessage) /*i*/
  {
    Str255 pascalStr; // to hold Pascal string version of inMessage
    mork_u4 length = MORK_STRLEN(inMessage);
    
    // if longer than maximum 255 bytes, just copy 255 bytes worth
    pascalStr[ 0 ] = (mork_u1) ((length > 255)? 255 : length);
    if ( length ) // anything to copy? */
    {
      register mork_u1* p = ((mork_u1*) &pascalStr) + 1; // after length byte
      register mork_u1* end = p + length; // one past last byte to copy
      while ( p < end ) // more bytes to copy?
      {
        register int c = (mork_u1) *inMessage++;
        if ( c == ';' ) // c is the MacsBug ';' metacharacter?
          c = ':'; // change to ':', rendering harmless in a MacsBug context
        *p++ = (mork_u1) c;
      }
    }
    
    DebugStr(pascalStr); /* call Mac debugger entry point */
  }
#endif /*MORK_OBSOLETE*/
/* ----- ----- ----- ----- MORK_OBSOLETE ----- ----- ----- ----- */

void mork_assertion_signal(const char* inMessage)
{
#if defined(MORK_OBSOLETE) || defined(MORK_ALONE)
  mork_mac_break_string(inMessage);
#endif /*MORK_OBSOLETE*/

#if defined(MORK_WIN) || defined(MORK_MAC)
  // asm { int 3 }
#ifndef MORK_ALONE
  NS_ASSERTION(0, inMessage);
#endif /*MORK_ALONE*/
#endif /*MORK_WIN*/
}

#ifdef MORK_PROVIDE_STDLIB

MORK_LIB_IMPL(mork_i4)
mork_memcmp(const void* inOne, const void* inTwo, mork_size inSize)
{
  register const mork_u1* t = (const mork_u1*) inTwo;
  register const mork_u1* s = (const mork_u1*) inOne;
  const mork_u1* end = s + inSize;
  register mork_i4 delta;
  
  while ( s < end )
  {
    delta = ((mork_i4) *s) - ((mork_i4) *t);
    if ( delta )
      return delta;
    else
    {
      ++t;
      ++s;
    }
  }
  return 0;
}

MORK_LIB_IMPL(void)
mork_memcpy(void* outDst, const void* inSrc, mork_size inSize)
{
  register mork_u1* d = (mork_u1*) outDst;
  mork_u1* end = d + inSize;
  register const mork_u1* s = ((const mork_u1*) inSrc);
  
  while ( inSize >= 8 )
  {
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    
    inSize -= 8;
  }
  
  while ( d < end )
    *d++ = *s++;
}

MORK_LIB_IMPL(void)
mork_memmove(void* outDst, const void* inSrc, mork_size inSize)
{
  register mork_u1* d = (mork_u1*) outDst;
  register const mork_u1* s = (const mork_u1*) inSrc;
  if ( d != s && inSize ) // copy is necessary?
  {
    const mork_u1* srcEnd = s + inSize; // one past last source byte
    
    if ( d > s && d < srcEnd ) // overlap? need to copy backwards?
    {
      s = srcEnd; // start one past last source byte
      d += inSize; // start one past last dest byte
      mork_u1* dstBegin = d; // last byte to write is first in dest range
      while ( d - dstBegin >= 8 )
      {
        *--d = *--s;
        *--d = *--s;
        *--d = *--s;
        *--d = *--s;
        
        *--d = *--s;
        *--d = *--s;
        *--d = *--s;
        *--d = *--s;
      }
      while ( d > dstBegin )
        *--d = *--s;
    }
    else // can copy forwards without any overlap
    {
      mork_u1* dstEnd = d + inSize;
      while ( dstEnd - d >= 8 )
      {
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
      }
      while ( d < dstEnd )
        *d++ = *s++;
    }
  }
}

MORK_LIB_IMPL(void)
mork_memset(void* outDst, int inByte, mork_size inSize)
{
  register mork_u1* d = (mork_u1*) outDst;
  mork_u1* end = d + inSize;
  while ( d < end )
    *d++ = (mork_u1) inByte;
}

MORK_LIB_IMPL(void)
mork_strcpy(void* outDst, const void* inSrc)
{
  // back up one first to support preincrement
  register mork_u1* d = ((mork_u1*) outDst) - 1;
  register const mork_u1* s = ((const mork_u1*) inSrc) - 1;
  while ( ( *++d = *++s ) != 0 )
    /* empty */;
}

MORK_LIB_IMPL(mork_i4)
mork_strcmp(const void* inOne, const void* inTwo)
{
  register const mork_u1* t = (const mork_u1*) inTwo;
  register const mork_u1* s = ((const mork_u1*) inOne);
  register mork_i4 a;
  register mork_i4 b;
  register mork_i4 delta;
  
  do
  {
    a = (mork_i4) *s++;
    b = (mork_i4) *t++;
    delta = a - b;
  }
  while ( !delta && a && b );
  
  return delta;
}

MORK_LIB_IMPL(mork_i4)
mork_strncmp(const void* inOne, const void* inTwo, mork_size inSize)
{
  register const mork_u1* t = (const mork_u1*) inTwo;
  register const mork_u1* s = (const mork_u1*) inOne;
  const mork_u1* end = s + inSize;
  register mork_i4 delta;
  register mork_i4 a;
  register mork_i4 b;
  
  while ( s < end )
  {
    a = (mork_i4) *s++;
    b = (mork_i4) *t++;
    delta = a - b;
    if ( delta || !a || !b )
      return delta;
  }
  return 0;
}

MORK_LIB_IMPL(mork_size)
mork_strlen(const void* inString)
{
  // back up one first to support preincrement
  register const mork_u1* s = ((const mork_u1*) inString) - 1;
  while ( *++s ) // preincrement is cheapest
    /* empty */;
  
  return s - ((const mork_u1*) inString); // distance from original address
}

#endif /*MORK_PROVIDE_STDLIB*/

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

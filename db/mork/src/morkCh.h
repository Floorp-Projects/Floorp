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
 
#ifndef _MORKCH_
#define _MORKCH_ 1

#ifndef _MORK_
#include "mork.h"
#endif

/* this byte char predicate header file derives from public domain Mithril */
/* (that means much of this has a copyright dedicated to the public domain) */

/* Use all 8 pred bits; lose some pred bits only if we need to reuse them. */

/* ch pred bits: W:white D:digit V:value U:upper L:lower N:name  M:more */
#define morkCh_kW      (1 << 0)
#define morkCh_kD      (1 << 1)
#define morkCh_kV      (1 << 2)
#define morkCh_kU      (1 << 3)
#define morkCh_kL      (1 << 4)
#define morkCh_kX      (1 << 5)
#define morkCh_kN      (1 << 6)
#define morkCh_kM      (1 << 7)

extern const mork_u1 morkCh_Type[]; /* 256 byte predicate bits ch map */

/* is a character that normally continues a symbol token: */
#define morkCh_IsSymbol(c)   ( morkCh_Type[ (mork_ch)(c) ] & morkCh_kS )

/* is a numeric decimal digit: (note memory access might be slower) */
/* define morkCh_IsDigit(c)  ( morkCh_Type[ (mork_ch)(c) ] & morkCh_kD ) */
#define morkCh_IsDigit(c)    ( ((mork_ch) c) >= '0' && ((mork_ch) c) <= '9' )

/* is a numeric octal digit: */
#define morkCh_IsOctal(c)    ( ((mork_ch) c) >= '0' && ((mork_ch) c) <= '7' )

/* is a numeric hexadecimal digit: */
#define morkCh_IsHex(c) ( morkCh_Type[ (mork_ch)(c) ] & morkCh_kX )

/* is value (can be printed in Mork value without needing hex or escape): */
#define morkCh_IsValue(c)    ( morkCh_Type[ (mork_ch)(c) ] & morkCh_kV )

/* is white space : */
#define morkCh_IsWhite(c)    ( morkCh_Type[ (mork_ch)(c) ] & morkCh_kW )

/* is name (can start a Mork name): */
#define morkCh_IsName(c)     ( morkCh_Type[ (mork_ch)(c) ] & morkCh_kN )

/* is name (can continue a Mork name): */
#define morkCh_IsMore(c) ( morkCh_Type[ (mork_ch)(c) ] & morkCh_kM )

/* is alphabetic upper or lower case */
#define morkCh_IsAlpha(c)    \
  ( morkCh_Type[ (mork_ch)(c) ] & (morkCh_kL|morkCh_kU) )

/* is alphanumeric, including lower case, upper case, and digits */
#define morkCh_IsAlphaNum(c) \
  (morkCh_Type[ (mork_ch)(c) ]&(morkCh_kL|morkCh_kU|morkCh_kD))

  
#define morkCh_IsAscii(c)         ( ((mork_u1) c) <= 0x7F )
#define morkCh_IsSevenBitChar(c)  ( ((mork_u1) c) <= 0x7F )

/* ````` character case (e.g. for case insensitive operations) ````` */

#define morkCh_ToLower(c)    ((c)-'A'+'a')
#define morkCh_ToUpper(c)    ((c)-'a'+'A')

/* extern int morkCh_IsUpper (int c); */
#define morkCh_IsUpper(c)    ( morkCh_Type[ (mork_ch)(c) ] & morkCh_kU )

/* extern int morkCh_IsLower (int c); */
#define morkCh_IsLower(c)    ( morkCh_Type[ (mork_ch)(c) ] & morkCh_kL )

#endif
/* _MORKCH_ */

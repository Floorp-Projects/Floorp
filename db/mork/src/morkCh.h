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

extern const mork_flags morkCh_Type[]; /* 256 byte predicate bits ch map */

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

/* ````` repeated testing of predicate bits in single flag byte ````` */

#define morkCh_GetFlags(c) ( morkCh_Type[ (mork_ch)(c) ] )

#define morkFlags_IsDigit(f)    ( (f) & morkCh_kD )
#define morkFlags_IsHex(f)      ( (f) & morkCh_kX )
#define morkFlags_IsValue(f)    ( (f) & morkCh_kV )
#define morkFlags_IsWhite(f)    ( (f) & morkCh_kW )
#define morkFlags_IsName(f)     ( (f) & morkCh_kN )
#define morkFlags_IsMore(f)     ( (f) & morkCh_kM )
#define morkFlags_IsAlpha(f)    ( (f) & (morkCh_kL|morkCh_kU) )
#define morkFlags_IsAlphaNum(f) ( (f) & (morkCh_kL|morkCh_kU|morkCh_kD) )

#define morkFlags_IsUpper(f)    ( (f) & morkCh_kU )
#define morkFlags_IsLower(f)    ( (f) & morkCh_kL )

/* ````` character case (e.g. for case insensitive operations) ````` */

  
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

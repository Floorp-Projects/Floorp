/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-  */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _MDB_
#include "mdb.h"
#endif

#ifndef _MORK_
#include "mork.h"
#endif

#ifndef _MORKCH_
#include "morkCh.h"
#endif

#ifndef _MORKENV_
#include "morkEnv.h"
#endif

//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

/* this byte char predicate source file derives from public domain Mithril */
/* (that means much of this has a copyright dedicated to the public domain) */

/*============================================================================*/
/* morkCh_Type */

const mork_flags morkCh_Type[] = /* derives from public domain Mithril table */
{
  0,                /* 0x0 */
  0,                /* 0x1 */
  0,                /* 0x2 */
  0,                /* 0x3 */
  0,                /* 0x4 */
  0,                /* 0x5 */
  0,                /* 0x6 */
  0,                /* 0x7 */
  morkCh_kW,        /* 0x8 backspace */
  morkCh_kW,        /* 0x9 tab */
  morkCh_kW,        /* 0xA linefeed */
  0,                /* 0xB */
  morkCh_kW,        /* 0xC page */
  morkCh_kW,        /* 0xD return */
  0,                /* 0xE */
  0,                /* 0xF */
  0,                /* 0x10 */
  0,                /* 0x11 */
  0,                /* 0x12 */
  0,                /* 0x13 */
  0,                /* 0x14 */
  0,                /* 0x15 */
  0,                /* 0x16 */
  0,                /* 0x17 */
  0,                /* 0x18 */
  0,                /* 0x19 */
  0,                /* 0x1A */
  0,                /* 0x1B */
  0,                /* 0x1C */
  0,                /* 0x1D */
  0,                /* 0x1E */
  0,                /* 0x1F */
  
  morkCh_kV|morkCh_kW,     /* 0x20 space */
  morkCh_kV|morkCh_kM,     /* 0x21 ! */
  morkCh_kV,               /* 0x22 " */
  morkCh_kV,               /* 0x23 # */
  0,                       /* 0x24 $ cannot be kV because needs escape */
  morkCh_kV,               /* 0x25 % */
  morkCh_kV,               /* 0x26 & */
  morkCh_kV,               /* 0x27 ' */
  morkCh_kV,               /* 0x28 ( */
  0,                       /* 0x29 ) cannot be kV because needs escape */
  morkCh_kV,               /* 0x2A * */
  morkCh_kV|morkCh_kM,     /* 0x2B + */
  morkCh_kV,               /* 0x2C , */
  morkCh_kV|morkCh_kM,     /* 0x2D - */
  morkCh_kV,               /* 0x2E . */
  morkCh_kV,               /* 0x2F / */
  
  morkCh_kV|morkCh_kD|morkCh_kX,  /* 0x30 0 */
  morkCh_kV|morkCh_kD|morkCh_kX,  /* 0x31 1 */
  morkCh_kV|morkCh_kD|morkCh_kX,  /* 0x32 2 */
  morkCh_kV|morkCh_kD|morkCh_kX,  /* 0x33 3 */
  morkCh_kV|morkCh_kD|morkCh_kX,  /* 0x34 4 */
  morkCh_kV|morkCh_kD|morkCh_kX,  /* 0x35 5 */
  morkCh_kV|morkCh_kD|morkCh_kX,  /* 0x36 6 */
  morkCh_kV|morkCh_kD|morkCh_kX,  /* 0x37 7 */
  morkCh_kV|morkCh_kD|morkCh_kX,  /* 0x38 8 */
  morkCh_kV|morkCh_kD|morkCh_kX,  /* 0x39 9 */
  morkCh_kV|morkCh_kN|morkCh_kM,        /* 0x3A : */
  morkCh_kV,                /* 0x3B ; */
  morkCh_kV,                /* 0x3C < */
  morkCh_kV,                /* 0x3D = */
  morkCh_kV,                /* 0x3E > */
  morkCh_kV|morkCh_kM,      /* 0x3F ? */
  
  morkCh_kV,                /* 0x40 @  */  
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kU|morkCh_kX,  /* 0x41 A */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kU|morkCh_kX,  /* 0x42 B */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kU|morkCh_kX,  /* 0x43 C */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kU|morkCh_kX,  /* 0x44 D */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kU|morkCh_kX,  /* 0x45 E */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kU|morkCh_kX,  /* 0x46 F */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kU,          /* 0x47 G */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kU,          /* 0x48 H */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kU,          /* 0x49 I */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kU,          /* 0x4A J */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kU,          /* 0x4B K */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kU,          /* 0x4C L */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kU,          /* 0x4D M */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kU,          /* 0x4E N */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kU,          /* 0x4F O */
  
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kU,          /* 0x50 P */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kU,          /* 0x51 Q */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kU,          /* 0x52 R */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kU,          /* 0x53 S */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kU,          /* 0x54 T */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kU,          /* 0x55 U */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kU,          /* 0x56 V */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kU,          /* 0x57 W */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kU,          /* 0x58 X */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kU,          /* 0x59 Y */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kU,          /* 0x5A Z */
  morkCh_kV,                /* 0x5B [ */
  0,                /* 0x5C \ cannot be kV because needs escape */
  morkCh_kV,                /* 0x5D ] */
  morkCh_kV,          /* 0x5E ^ */
  morkCh_kV|morkCh_kN|morkCh_kM,          /* 0x5F _ */
  
  morkCh_kV,                /* 0x60 ` */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kL|morkCh_kX,  /* 0x61 a */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kL|morkCh_kX,  /* 0x62 b */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kL|morkCh_kX,  /* 0x63 c */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kL|morkCh_kX,  /* 0x64 d */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kL|morkCh_kX,  /* 0x65 e */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kL|morkCh_kX,  /* 0x66 f */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kL,          /* 0x67 g */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kL,          /* 0x68 h */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kL,          /* 0x69 i */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kL,          /* 0x6A j */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kL,          /* 0x6B k */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kL,          /* 0x6C l */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kL,          /* 0x6D m */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kL,          /* 0x6E n */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kL,          /* 0x6F o */
  
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kL,          /* 0x70 p */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kL,          /* 0x71 q */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kL,          /* 0x72 r */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kL,          /* 0x73 s */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kL,          /* 0x74 t */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kL,          /* 0x75 u */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kL,          /* 0x76 v */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kL,          /* 0x77 w */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kL,          /* 0x78 x */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kL,          /* 0x79 y */
  morkCh_kV|morkCh_kN|morkCh_kM|morkCh_kL,          /* 0x7A z */
  morkCh_kV,                /* 0x7B { */
  morkCh_kV,                /* 0x7C | */
  morkCh_kV,                /* 0x7D } */
  morkCh_kV,          /* 0x7E ~ */
  morkCh_kW,          /* 0x7F rubout */

/* $"80 81 82 83 84 85 86 87 88 89 8A 8B 8C 8D 8E 8F"   */
  0,    0,    0,    0,    0,    0,    0,    0,  
  0,    0,    0,    0,    0,    0,    0,    0,  

/* $"90 91 92 93 94 95 96 97 98 99 9A 9B 9C 9D 9E 9F"   */
  0,    0,    0,    0,    0,    0,    0,    0,  
  0,    0,    0,    0,    0,    0,    0,    0,  

/* $"A0 A1 A2 A3 A4 A5 A6 A7 A8 A9 AA AB AC AD AE AF"   */
  0,    0,    0,    0,    0,    0,    0,    0,  
  0,    0,    0,    0,    0,    0,    0,    0,  

/* $"B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 BA BB BC BD BE BF"   */
  0,    0,    0,    0,    0,    0,    0,    0,  
  0,    0,    0,    0,    0,    0,    0,    0,  

/* $"C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 CA CB CC CD CE CF"   */
  0,    0,    0,    0,    0,    0,    0,    0,  
  0,    0,    0,    0,    0,    0,    0,    0,  

/* $"D0 D1 D2 D3 D4 D5 D6 D7 D8 D9 DA DB DC DD DE DF"   */
  0,    0,    0,    0,    0,    0,    0,    0,  
  0,    0,    0,    0,    0,    0,    0,    0,  

/* $"E0 E1 E2 E3 E4 E5 E6 E7 E8 E9 EA EB EC ED EE EF"   */
  0,    0,    0,    0,    0,    0,    0,    0,  
  0,    0,    0,    0,    0,    0,    0,    0,  

/* $"F0 F1 F2 F3 F4 F5 F6 F7 F8 F9 FA FB FC FD FE FF"   */
  0,    0,    0,    0,    0,    0,    0,    0,  
  0,    0,    0,    0,    0,    0,    0,    0,  
};


//3456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789

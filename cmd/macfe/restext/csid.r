 /*
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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/*
	File:		csid.r

	Contains:	Type Declarations for Rez and DeRez

	Copyright:	© 1986-1994 by Netscape Communication, Inc.
				All Fights Reserved.
*/

#ifndef __CSID_R__
#define __CSID_R__

/* Duplicate from i18nlib.h */
#define SINGLEBYTE 0x0000 /* 0000 0000 0000 0000 */
#define MULTIBYTE  0x0100 /* 0000 0001 0000 0000 */
#define STATEFUL   0x0200 /* 0000 0010 0000 0000 */
#define WIDECHAR   0x0300 /* 0000 0011 0000 0000 */

/* line-break on spaces */
#define CS_SPACE   0x0400 /* 0000 0100 0000 0000 */

/* Auto Detect Mode */
#define CS_AUTO    0x0800 /* 0000 1000 0000 0000 */


/* Code Set IDs */
/* CS_DEFAULT: used if no charset param in header */
/* CS_UNKNOWN: used for unrecognized charset */

                    /* type                  id   */
#define CS_DEFAULT    (SINGLEBYTE         |   0)
#define CS_ASCII      (SINGLEBYTE         |   1)
#define CS_LATIN1     (SINGLEBYTE         |   2)
#define CS_JIS        (STATEFUL           |   3)
#define CS_SJIS       (MULTIBYTE          |   4)
#define CS_EUCJP      (MULTIBYTE          |   5)
#define CS_JIS_AUTO   (CS_AUTO|STATEFUL   |   3)
#define CS_SJIS_AUTO  (CS_AUTO|MULTIBYTE  |   4)
#define CS_EUCJP_AUTO (CS_AUTO|MULTIBYTE  |   5)
#define CS_MAC_ROMAN  (SINGLEBYTE         |   6)
#define CS_BIG5       (MULTIBYTE          |   7)
#define CS_GB_8BIT    (MULTIBYTE          |   8)
#define CS_CNS_8BIT   (MULTIBYTE          |   9)
#define CS_LATIN2     (SINGLEBYTE         |  10)
#define CS_MAC_CE     (SINGLEBYTE         |  11)
#define CS_KSC_8BIT   (MULTIBYTE|CS_SPACE |  12)
#define CS_2022_KR    (MULTIBYTE          |  13)
#define CS_UNKNOWN    (SINGLEBYTE         | 255)

#endif

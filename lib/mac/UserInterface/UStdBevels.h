/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#pragma once

#include <PP_Prefix.h>

#if defined(powerc) || defined(__powerc)
#pragma options align=mac68k
#endif

typedef struct SBevelColorDesc {
	Int16		frameColor;
	Int16		fillColor;
	Int16		topBevelColor;
	Int16		bottomBevelColor;
} SBevelColorDesc, *SBevelColorDescPtr, **SBevelColorDescHdl;

#if defined(powerc) || defined(__powerc)
#pragma options align=reset
#endif

//
// NOTE: White and Black must be the first two entries!
//
enum {						// R == G == B
	eStdGrayWhite = 0,		//	65535			15
	eStdGrayBlack,			//	0				0
	eStdGray93,				//	61166			14
	eStdGray86,				//	56797			13
	eStdGray80,				//	52428			12
	eStdGray73,				//	48059			11
	eStdGray66,				//	43690			10
	eStdGray60,				//	39321			9
	eStdGray53,				//	34952			8
	eStdGray46,				//	30583			7
	eStdGray40,				//	26214			6
	eStdGray33,				//	21845			5
	eStdGray26,				//	17476			4
	eStdGray20,				//	13107			3
	eStdGray13,				//	8738			2
	eStdGray6				//	4369			1
};

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

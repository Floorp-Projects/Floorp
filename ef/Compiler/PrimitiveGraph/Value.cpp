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
#include "Value.h"
#include "DebugUtils.h"

#ifdef DEBUG_LOG
//
// Print the name of the Condition onto the output stream f.
// Return the number of characters printed.
//
int print(LogModuleObject &f, Condition c)
{
	const char *cName = "??";
	switch (c) {
		case cLt:
			cName = "LT";
			break;
		case cEq:
			cName = "EQ";
			break;
		case cGt:
			cName = "GT";
			break;
		case cUn:
			cName = "UN";
			break;
	}
	return UT_OBJECTLOG(f, PR_LOG_ALWAYS, (cName));
}


const char valueKindShortNames[nValueKinds] =
{
	'v',	// vkVoid
	'i',	// vkInt
	'l',	// vkLong
	'f',	// vkFloat
	'd',	// vkDouble
	'a',	// vkAddr
	'c',	// vkCond
	'm',	// vkMemory
	't'		// vkTuple
};


//
// Return the one-character abbreviation for the given ValueKind.
//
char valueKindShortName(ValueKind vk)
{
	return (uint)vk >= nValueKinds ? '?' : valueKindShortNames[vk];
}


const char valueKindNames[nValueKinds][8] =
{
	"void",		// vkVoid
	"int",		// vkInt
	"long",		// vkLong
	"float",	// vkFloat
	"double",	// vkDouble
	"addr",		// vkAddr
	"cond",		// vkCond
	"memory",	// vkMemory
	"tuple"		// vkTuple
};


//
// Print the name of the ValueKind onto the output stream f.
// Return the number of characters printed.
//
int print(LogModuleObject &f, ValueKind vk)
{
	return UT_OBJECTLOG(f, PR_LOG_ALWAYS, ((uint)vk >= nValueKinds ? "????" : valueKindNames[vk]));
}
#endif


const ValueKind typeKindValueKinds[nTypeKinds] =
{
	vkVoid,		// tkVoid
	vkInt,		// tkBoolean
	vkInt,		// tkUByte
	vkInt,		// tkByte
	vkInt,		// tkChar
	vkInt,		// tkShort
	vkInt,		// tkInt
	vkLong,		// tkLong
	vkFloat,	// tkFloat
	vkDouble,	// tkDouble
	vkAddr,		// tkObject
	vkAddr,		// tkSpecial
	vkAddr,		// tkArray
	vkAddr		// tkInterface
};


// ----------------------------------------------------------------------------
// Value


#ifdef DEBUG_LOG
//
// Print this Value for debugging purposes.
// Return the number of characters printed.
//
int Value::print(LogModuleObject &f, ValueKind vk) const
{
	switch (vk) {
		case vkVoid:
			return UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("VOID"));
		case vkInt:
			return UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%d", i));
		case vkLong:
			{
				int a = printInt64(f, l);
				return a + UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("L"));
			}
		case vkFloat:
			return UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%gf", Value::f));
		case vkDouble:
			return UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("%gd", Value::d));
		case vkAddr:
			return ::print(f, a);
		case vkCond:
			return ::print(f, c);
		case vkMemory:
			return UT_OBJECTLOG(f, PR_LOG_ALWAYS, (m == mConstant ? "mConstant" : "????"));
		case vkTuple:
			return UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("TUPLE"));
		default:
			return UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("????"));
	}
}
#endif

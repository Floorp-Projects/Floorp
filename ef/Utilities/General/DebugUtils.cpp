/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "DebugUtils.h"

#ifdef DEBUG_LOG
//
// Print nSpaces spaces on the output stream f.
//
void printSpaces(LogModuleObject &f, int nSpaces)
{
	while (nSpaces > 0) {
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, (" "));
		nSpaces--;
	}
}


//
// Tab margin spaces over from the beginning of a line.
// f must be at the beginning of a line.  This differs
// from printSpaces in that it could output tabs instead of
// or in addition to spaces.
//
void printMargin(LogModuleObject &f, int margin)
{
	if (tabWidth > 1)
		while (margin >= tabWidth) {
			UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("\t"));
			margin -= tabWidth;
		}
	printSpaces(f, margin);
}


//
// Print the unsigned Int64 for debugging purposes.
// Return the number of characters printed.
//
int printUInt64(LogModuleObject &f, Uint64 l)
{
	char digits[21];
	digits[21] = '\0';
	char *firstDigit = digits + 21;
	do {
		Uint64 q = l / 10;
		*--firstDigit = (char)(l - q*10) + '0';
		l = q;
	} while (l != 0);
	return UT_OBJECTLOG(f, PR_LOG_ALWAYS, (firstDigit));
}


//
// Print the signed Int64 for debugging purposes.
// Return the number of characters printed.
//
int printInt64(LogModuleObject &f, Int64 l)
{
	int a = 0;
	if (l < 0) {
		UT_OBJECTLOG(f, PR_LOG_ALWAYS, ("-"));
		a = 1;
		l = -l;
	}
	return a + printUInt64(f, l);
}
#endif

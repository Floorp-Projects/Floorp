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
// MonitorExpression.cpp
//
// Scott M. Silver

#include <ctype.h>
#include "MonitorExpression.h"
#include <Windows.h>
#include "Symbols.h"
#include "Fundamentals.h"

// for now only handle numbers or registers

const char*
extractMethodFromExpression(const char* inString)
{
	return (inString);
}


void*
evaluateExpression(DebugeeThread& inThread, const char* inString)
{

	PrintableValue	pv;

	if (lookupSymbol(inThread, inString, pv))
		return (pv.value);
	else
		return (0);		// bail
}


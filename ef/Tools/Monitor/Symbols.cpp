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
// Symbols.cpp
//
// Scott M. Silver
//

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "Debugee.h"
#include <assert.h>
#include "Symbols.h"

bool lookupLiteral(const char* inSymbol, PrintableValue& outValue);
bool lookupRegister(DebugeeThread& inThread, const char* inSymbol, PrintableValue& outValue);

// the mother of all symbol lookups
// this searches through all known ways
// of figuring out what the hell a symbol is
bool
lookupSymbol(DebugeeThread& inThread, const char* inSymbol, PrintableValue& outValue)
{
	if (lookupLiteral(inSymbol, outValue))
		return(true);

	if (lookupRegister(inThread, inSymbol, outValue))
		return (true);

	void *address;

	if ((address = inThread.getProcess().getAddress(inSymbol)))
	{
		outValue.fmtString = "%p";
		outValue.value = address;
		return (true);
	}

	return (false);
}


bool
lookupLiteral(const char* inString, PrintableValue& outValue)
{	
	const char*	lc = inString + strlen(inString); // point to 0
	Int32 value;
	char* endp;

	if (*inString == '\0')
		return (false);

	if (*inString == '#')
	{
		value = strtol(inString + 1, &endp, 10); 
		if (endp == lc)
		{
			outValue.value = (void*) value;
			outValue.fmtString = "#%d";
			return (true);
		}
	}
	else if (*inString == '$')
	{
		value = strtol(inString + 1, &endp, 16); 
		if (endp == lc)
		{
			outValue.value = (void*) value;
			outValue.fmtString = "0x%x";
			return (true);
		}
	}
	else 
	{
		value = strtol(inString, &endp, 16); 
		if (endp == lc)
		{
			outValue.value = (void*) value;
			outValue.fmtString = "0x%x";
			return (true);
		}
	}

	return (false);
}

bool
lookupRegister(DebugeeThread& inThread, const char* inSymbol, PrintableValue& outValue)
{
	char* symbol = strdup(inSymbol);
	char* cp = symbol;

	assert(symbol);

	// lowercase the string
	while(*cp)
		*cp++ = tolower(*cp);

	outValue.fmtString = "%.8X";

	bool		found = true;
	CONTEXT		context;

	assert(inThread.getContext(CONTEXT_INTEGER | CONTEXT_CONTROL, context));
	
	if (!strcmp("edi", symbol))
		outValue.value = (void*) context.Edi;
	else if (!strcmp("esi", symbol))
		outValue.value = (void*) context.Esi;
	else if (!strcmp("ebx", symbol))
		outValue.value = (void*) context.Ebx;
	else if (!strcmp("edx", symbol))
		outValue.value = (void*) context.Edx;
	else if (!strcmp("ecx", symbol))
		outValue.value = (void*) context.Ecx;
	else if (!strcmp("eax", symbol))
		outValue.value = (void*) context.Eax;
	else if (!strcmp("ebp", symbol))
		outValue.value = (void*) context.Ebp;
	else if (!strcmp("eip", symbol))
		outValue.value = (void*) context.Eip;
	else if (!strcmp("esp", symbol))
		outValue.value = (void*) context.Esp;
	else
		found = false;

	free(symbol);
	return (found);
}

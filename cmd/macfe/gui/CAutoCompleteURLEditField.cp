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

// CAutoCompleteURLEditField.h

// Subclass of CURLEditField that calls urlMatch function to do
// URL string matching

#include "CAutoCompleteURLEditField.h"

#include <PP_Types.h>		// for Char16 typedef
#include <UKeyFilters.h>	// for UKeyFilters::IsPrintingChar proto
#include <LowMem.h>			// for LMGetTicks() proto

#include "xp_mem.h"	// for XP_FREE macro
#include "glhist.h"	// for urlMatch proto

#include "PascalString.h"	// for CString/CStr255 stuff

const UInt32 CAutoCompleteURLEditField::matchDelay = 20;

CAutoCompleteURLEditField::CAutoCompleteURLEditField(LStream* inStream)
:	CURLEditField(inStream),
		lastKeyPressTicks(0)
{
}

// NOTE: I believe UKeyFilters::IsPrintingChar() makes this method
// i18n unfriendly, but that's just a guess.
Boolean CAutoCompleteURLEditField::HandleKeyPress(const EventRecord& inKeyEvent)
{
	Char16 theChar = inKeyEvent.message & charCodeMask;
	if (UKeyFilters::IsPrintingChar(theChar))
		lastKeyPressTicks = LMGetTicks();
	return CURLEditField::HandleKeyPress(inKeyEvent);
}

void CAutoCompleteURLEditField::SpendTime(const EventRecord	&inMacEvent)
{
	if (lastKeyPressTicks && (LMGetTicks() - lastKeyPressTicks > matchDelay))
	{
		char *result = NULL;
		CStr255 str;
		GetDescriptor(str);
		if (str.Length() > 0)
		{
			// remove selection from search string
			if ((**GetMacTEH()).selEnd - (**GetMacTEH()).selStart > 0)
				str.Delete((**GetMacTEH()).selStart, kStr255Len);
			autoCompStatus status = stillSearching;
			bool firstTime = true;
			while (status == stillSearching)
			{
				status = urlMatch(str, &result, firstTime, false);
				firstTime = false;
			}
			if (status == foundDone)
			{
				unsigned char selStart = str.Length();
				// append search result
				str += (result + str.Length());
				SetDescriptor(str);
				FocusDraw();			// SetDescriptor() mucks with the port rect
				
				// then set selection
				::TESetSelect(selStart, str.Length() + 1, GetMacTEH());
				XP_FREE(result);	// urlMatch uses XP_STRDUP to pass back a new string
			}
		}
		lastKeyPressTicks = 0;
	}
	
	CURLEditField::SpendTime(inMacEvent);
}

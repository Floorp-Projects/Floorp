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

#include <string>
#include "CAutoPtrXP.h"
#include <PP_Types.h>		// for Char16 typedef
#include <UKeyFilters.h>	// for UKeyFilters::IsPrintingChar proto
#include <LowMem.h>			// for LMGetTicks() proto

#include "xp_mem.h"	// for XP_FREE macro
#include "glhist.h"	// for urlMatch proto



CAutoCompleteURLEditField::CAutoCompleteURLEditField(LStream* inStream)
:	CURLEditField(inStream)
	,	mNextMatchTime(ULONG_MAX)
	,	mStartOver(true)
{
}

// NOTE: I believe UKeyFilters::IsPrintingChar() makes this method
// i18n unfriendly, but that's just a guess.
Boolean CAutoCompleteURLEditField::HandleKeyPress(const EventRecord& inKeyEvent)
{
	Char16	theChar = inKeyEvent.message & charCodeMask;

	// NOTE: I believe UKeyFilters::IsPrintingChar() makes this method
	// i18n unfriendly, but that's just a guess.
	if (UKeyFilters::IsPrintingChar(theChar))
		mNextMatchTime = ::LMGetTicks() + kMatchDelay;
	
	// urlMatch is optimized to search from the current entry when
	// the search string is simply getting longer. But if the user
	// edits the existing text, we want to search again from the start
	if (UKeyFilters::IsTEDeleteKey(theChar) ||
		UKeyFilters::IsTECursorKey(theChar) ||
		UKeyFilters::IsExtraEditKey(theChar))
	{
		mStartOver = true;
	}
		
	return Inherited::HandleKeyPress(inKeyEvent);
}

void CAutoCompleteURLEditField::ClickSelf(const SMouseDownEvent& inMouseDown)
{
	Inherited::ClickSelf(inMouseDown);
	// this click can result in a selection that allows the user to
	// overtype selected text. So autocomplete on the whole thing
	// again
	mStartOver = true;
}

#pragma mark -

class CAutocompleteTypingAction : public LTETypingAction
{
	public:
		CAutocompleteTypingAction()
			: LTETypingAction(nil, nil, nil) {}
		void UpdateAfterAutocomplete(Int16 inNewLength);
};

void CAutocompleteTypingAction::UpdateAfterAutocomplete(Int16 inNewLength)
{
	mTypingEnd = inNewLength;
}

#pragma mark -

void CAutoCompleteURLEditField::SpendTime(const EventRecord	&inMacEvent)
{
	if (LMGetTicks() >= mNextMatchTime)
	{
		autoCompStatus status = notFoundDone;	// safe in case of throw below
		
		try
		{
			char urlText[2048];
			Int32 urlLength = sizeof(urlText);
			GetDescriptorLen(urlText, urlLength);
			if (urlLength > 0)
			{
				string originalText = urlText;
				// remove selection from search string
				UInt16 selStart = (**GetMacTEH()).selStart;
				UInt16 selEnd = (**GetMacTEH()).selEnd;
				if (selEnd - selStart > 0)
					memmove(urlText + selStart,
						urlText + selEnd,
						1 + (**GetMacTEH()).teLength - selEnd);
				
#ifdef DEBUG
				if (mStartOver)
					mTimesSearched = 0;
				
				mTimesSearched ++;
#endif
				char* tempResult = NULL;
				status = urlMatch(urlText, &tempResult, mStartOver, false);
				CAutoPtrXP<char> result = tempResult;
				mStartOver = false;

				// urlMatch will return foundDone even if it is returning the same
				// string it found last time.  So don't bother doing all the
				// processing if that is the case.
				if (status == foundDone && result.get() && result.get() != originalText )
				{
					selEnd = strlen(result.get());
					SetDescriptorLen(result.get(), selEnd);

					FocusDraw();			// SetDescriptor() mucks with the port rect					
					// then set selection
					::TESetSelect(selStart, selEnd, GetMacTEH());

					if (mTypingAction)
					{
						// If the user undoes, she wants to undo the autocompleted text AND the typing.
						// This is a static cast, whose purpose is to get at the protected member
						// mTypingEnd of the LTETypingAction class.  Being a static_cast of one kind
						// of class to another kind of class, it is, officially, skanky.  If you don't
						// want to be skanky, you could just cancel
						// the undoability of the user's typed characters after a successful
						// autocomplete, by calling PostAction(nil) instead.
						// This would be doing the user a disservice, though.
						CAutocompleteTypingAction* acTypingAction
							= dynamic_cast<CAutocompleteTypingAction*>(mTypingAction);
						acTypingAction->UpdateAfterAutocomplete(selEnd);
					}
				}
			}
		}
		catch(...)
		{
		}
		// we want to keep searching next time we are called. mNextMatchTime is thus only
		// updated when the user starts typing again, or we complete (successfully or not)
		if (status != stillSearching)
		{
			mStartOver = true;
			mNextMatchTime = ULONG_MAX;
		}
	}
	
	Inherited::SpendTime(inMacEvent);
}

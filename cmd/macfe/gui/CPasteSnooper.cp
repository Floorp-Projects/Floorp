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

// ===========================================================================
//	CPasteSnooper.cp
//	an attachment for filtering characters from a paste command
// ===========================================================================

#include "CPasteSnooper.h"

#include "PascalString.h"
#include "xp_mcom.h"

#include <PP_Messages.h>
#include <UTETextAction.h>

#ifndef __TYPES__
#include <Types.h>
#endif

#ifndef __SCRAP__
#include <Scrap.h>
#endif

#ifndef __TOOLUTILS__
#include <ToolUtils.h>
#endif

#include "CTSMEditField.h"

//========================================================================================
class CPasteActionSnooper : public LTEPasteAction
//========================================================================================
{
public:
					CPasteActionSnooper (
						TEHandle	inMacTEH,
						LCommander	*inTextCommander,
						LPane		*inTextPane,
						char		*inFind,
						char		*inReplace,
						UInt32		inMaxChars);
			
protected:
			void	DoReplace(char inFind, char inReplace);
			void	DoDelete(char inDelete);
			void	StripLeading(char inStrip);
			void	DoFilter(char inFilter);
};

// goal is to make this more general
CPasteSnooper::CPasteSnooper (char *inFind, char *inReplace) :
	// first param to LAttachment says we want to intercept paste commmands
	// second param says that we're going to take care of handling the
	// action and that our host should do nothing
	LAttachment(cmd_Paste, false)
{
	mFind = XP_STRDUP(inFind);
	mReplace = XP_STRDUP(inReplace);
}

CPasteSnooper::~CPasteSnooper ()
{
	XP_FREEIF(mFind);
	XP_FREEIF(mReplace);
}

// there's no beef here. look in CPasteActionSnooper's
// constructor to see where the actual filtering is done
void CPasteSnooper::ExecuteSelf (MessageT inMessage, void * /* ioParam */)
{

	// did we get called with good params?
	if (!mFind || !mReplace)
		return;
	Assert_(strlen(mFind) == strlen(mReplace));
	if (strlen(mFind) != strlen(mReplace))
		return;
		
	// paranoia
	Assert_(inMessage == cmd_Paste);
	
	// if you used something other than an LEditField you're in trouble
	LEditField	*editField = dynamic_cast<LEditField *>(mOwnerHost);
	Assert_(editField != nil);
	if (!editField)
		return;
	
	// Because mMaxChars is a protected data member of LEditField, we can only find
	// out the maximum count if the edit field is a CTSMEditField.
	CTSMEditField* tsmEdit = dynamic_cast<CTSMEditField *>(editField);
	UInt32 maxChars = tsmEdit ? tsmEdit->GetMaxChars() : 32000;

	// here's where the paste gets done... but our CPasteSnooperAction gets a shot
	// at it first to take out all the unwanted characters
	editField->PostAction(
		new CPasteActionSnooper(
				editField->GetMacTEH(),
				editField, editField,
				mFind, mReplace,
				maxChars)
	);
}

// here's where we actually filter stuff
CPasteActionSnooper::CPasteActionSnooper (
	TEHandle	inMacTEH,
	LCommander	*inTextCommander,
	LPane		*inTextPane,
	char		*inFind,
	char		*inReplace,
	UInt32		inMaxChars)
		: LTEPasteAction(inMacTEH, inTextCommander, inTextPane)
{
	for (int i = 0; inFind[i] != '\0'; i++)
		switch (inReplace[i]) {
			case '\b':
				DoDelete(inFind[i]);
				break;
			case '\f':
				DoFilter(inFind[i]);
				break;
			case '\a':
				StripLeading(inFind[i]);
				break;
			default:
				DoReplace(inFind[i], inReplace[i]);
		}
	Int32 maxSize = inMaxChars - (**inMacTEH).teLength + ((**inMacTEH).selEnd - (**inMacTEH).selStart);
	if (mPastedTextH && GetHandleSize(mPastedTextH) > maxSize)
	{
		SysBeep(1);
		SetHandleSize(mPastedTextH, maxSize);
	}
}

// removes all instances of inDelete from mPastedTextH
// Example:
//		if mPastedTextH referenced the string "\r\rHello World!\rMy Name is Andy!\r"
//		a call to DoDelete('\r') would make mPastedTextH
//		reference "Hello World!My Name is Andy!" upon completion
void CPasteActionSnooper::DoDelete (char inDelete) {
	// did our inherited class succeed in allocating memory for the scrap?
	if (*mPastedTextH == nil)
		return;

	Int32	scrapLength = ::GetHandleSize(mPastedTextH);
	
	// make sure this is a legal operation
	if (scrapLength < 0)
		return;
	
	// create a new handle to store our result
	Handle	newScrapText = ::NewHandle(scrapLength);

	// did it work?
	if (*newScrapText == nil)
		return;
		
	 // we're going to work with dereferenced handles from here
	 // on so we'd better lock them
 	::HLock(mPastedTextH);
	::HLock(newScrapText);
	
	// search through mPastedTextH and copy all characters except
	// inFind over to newScrapText
	Int32	srcIndex = 0;
	Int32	destIndex = 0;
	
	do {
		if ((*mPastedTextH)[srcIndex] != inDelete)
			(*newScrapText)[destIndex++] = (*mPastedTextH)[srcIndex];
	} while (++srcIndex < scrapLength);
	
	// unlock our handles
	::HUnlock(newScrapText);
	::HUnlock(mPastedTextH);
	
	// shrink newScrapText down to the minimum size
	::SetHandleSize(newScrapText, destIndex);

	// dispose of the text we were going to paste...
	::DisposeHandle(mPastedTextH);
	
	// ...and replace it with the new text that we really wanted to paste
	mPastedTextH = newScrapText;
}

// strips leading occurences of inFilter in mPastedTextH and then
// clips mPastedTextH right before the next occurence of inFind
// Example:
//		if mPastedTextH referenced the string "\r\rHello World!\rMy Name is Andy!\r"
//		a call to DoFilter('\r') would make mPastedTextH
//		reference "Hello World!" upon completion
void CPasteActionSnooper::DoFilter (char inFilter) {
	StripLeading(inFilter);
	
	// did our inherited class succeed in allocating memory for the scrap?
	if (*mPastedTextH == nil)
		return;

	Int32	scrapLength = ::GetHandleSize(mPastedTextH);
	
	// make sure this is a legal operation
	if (scrapLength < 0)
		return;
	
	 // we're going to work with the dereferenced handle from here
	 // on so I'll just lock it out of habit
 	::HLock(mPastedTextH);

	// only take characters up to the first occurence of inFind
	int	endOfNewScrap = 0;
	while ((*mPastedTextH)[endOfNewScrap] != inFilter && endOfNewScrap < scrapLength)
		endOfNewScrap++;

	// OK, all done with dereferencing
	::HUnlock(mPastedTextH);
	
	// truncate the handle after the first occurence of inFind
	if (endOfNewScrap < scrapLength)
		::SetHandleSize(mPastedTextH, endOfNewScrap);
}

// strips leading occurences of inFind
// Example:
// 		if mPastedTextH referenced the string "\t\tI luv\tCS\t\t"
//		a call to StripLeading('\t') would make mPastedTextH
//		reference "I luv\tCS\t\t" upon completion
void CPasteActionSnooper::StripLeading (char inStrip) {
	// did our inherited class succeed in allocating memory for the scrap?
	if (*mPastedTextH == nil)
		return;

	Int32	scrapLength = ::GetHandleSize(mPastedTextH);
	
	// make sure this is a legal operation
	if (scrapLength < 0)
		return;
	
	 // we're going to work with the dereferenced handle from here
	 // on so we'd better lock it
 	::HLock(mPastedTextH);

	Int32	startOfNewScrap;
	Int32	newScrapSize;
	Handle	newScrapText = nil;

	// skip over initial occurences of inStrip
	startOfNewScrap = 0;
	while ((*mPastedTextH)[startOfNewScrap] == inStrip)
		startOfNewScrap++;
			
	// make a new Handle to hold the characters we really want
	newScrapSize = scrapLength - startOfNewScrap;
	newScrapText = ::NewHandle(newScrapSize);

	// did NewHandle work?
	if (*newScrapText == nil) {
		// better not forget to do this
		::HUnlock(mPastedTextH);
		return;
	}

	// now copy those characters into the new handle
	::HLock(newScrapText);
	::BlockMove((*mPastedTextH + startOfNewScrap), *newScrapText, newScrapSize);
	::HUnlock(newScrapText);
	
	// unlock and dispose of the text we were going to paste...
	::HUnlock(mPastedTextH);
	::DisposeHandle(mPastedTextH);
	
	// ...and replace it with the new text that we really wanted to paste
	mPastedTextH = newScrapText;
}

// replaces all instances of inFind in mPastedTextH with inReplace
// Example:
//		if mPastedTextH referenced the string "\r\rHello World!\rMy Name is Andy!\r"
//		a call to DoReplace('\r', '#') would make mPastedTextH
//		reference "##Hello World!#My Name is Andy!#" upon completion
void CPasteActionSnooper::DoReplace (char inFind, char inReplace) {
	// did our inherited class succeed in allocating memory for the scrap?
	if (*mPastedTextH == nil)
		return;

	Int32	scrapLength = ::GetHandleSize(mPastedTextH);
	
	// make sure this is a legal operation
	if (scrapLength < 0)
		return;
	
	 // we're going to work the dereferenced handle from here
	 // on so we'd better lock it
 	::HLock(mPastedTextH);
	
	// search through mPastedTextH and replace all occurences
	// of inFind with inReplace
	for (int i = 0; i < scrapLength; i++) {
		if ((*mPastedTextH)[i] == inFind)
			(*mPastedTextH)[i] = inReplace;
	}
	
	// unlock our handle and we're done!
	::HUnlock(mPastedTextH);
}

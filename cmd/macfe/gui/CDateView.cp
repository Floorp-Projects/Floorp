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


/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

#include "CDateView.h"


/*====================================================================================*/
	#pragma mark TYPEDEFS
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark CONSTANTS
/*====================================================================================*/

// Pane IDs
static const PaneIDT eDayEditFieldID = 1;
static const PaneIDT eMonthEditFieldID = 2;
static const PaneIDT eYearEditFieldID = 3;

// Resource IDs
static const short cArrowWidth = 11;
static const short cArrowHeight = 9;

static const UInt32 cBroadcastMessageDelay = 12;


/*====================================================================================*/
	#pragma mark INTERNAL CLASS DECLARATIONS
/*====================================================================================*/

class CDateArrowButton : public LButton {

public:

	enum { class_ID = 'DaBt' };
						CDateArrowButton(LStream *inStream);

	// When clicking in button, send these messages
	enum { msg_ClickUpArrow = CDateView::paneID_UpButton, 
		   msg_ClickDownArrow = CDateView::paneID_DownButton };

	virtual void		SimulateHotSpotClick(Int16 inHotSpot);

protected:

	// Overriden methods

	virtual void		DisableSelf(void);
	virtual void		EnableSelf(void);

	virtual Boolean		TrackHotSpot(Int16 inHotSpot, Point inPoint, Int16 inModifiers);
	virtual void		HotSpotAction(Int16 inHotSpot, Boolean inCurrInside, Boolean inPrevInside);
	virtual void		HotSpotResult(short inHotSpot);

	// Instance variables ==========================================================
	
	UInt32				mNextBroadcastTime;		// Last time a message was broadcast
	UInt16				mBroadcastCount;		// Number of continuous braodcast messages
};


/*====================================================================================*/
	#pragma mark INTERNAL FUNCTION PROTOTYPES
/*====================================================================================*/

#ifdef Debug_Signal
	#define AssertFail_(test)		ThrowIfNot_(test)
#else // Debug_Signal
	#define AssertFail_(test)		((void) 0)
#endif // Debug_Signal

static inline Boolean IsBetween(long inVal, long inMin, long inMax) { 
	return ((inVal >= inMin) && (inVal <= inMax)); 
}


/*====================================================================================*/
	#pragma mark CLASS IMPLEMENTATIONS
/*====================================================================================*/

#pragma mark -

/*======================================================================================
	Register classes related to this class.
======================================================================================*/

void CDateView::RegisterDateClasses(void) {

	RegisterClass_(CDateView);
	RegisterClass_(CDateField);
	RegisterClass_(CDateArrowButton);
}


/*======================================================================================
	Constructor.
======================================================================================*/

CDateView::CDateView(LStream *inStream) : 
					 LView(inStream) {

	mDayField = mMonthField = mYearField = nil;
}


/*======================================================================================
	Constructor.
======================================================================================*/

void CDateView::FinishCreateSelf(void) {

	// Load in date/time resources
	
	Intl0Hndl int0H = (Intl0Hndl) ::GetIntlResource(0);
	UInt8 leadingDayChar, leadingMonthChar, separatingChar;
	
	if ( int0H ) {
		switch ( (**int0H).dateOrder ) {
			case mdy:	mDayPosition = 2; mMonthPosition = 1; mYearPosition = 3; break;
			case dmy:	mDayPosition = 1; mMonthPosition = 2; mYearPosition = 3; break;
			case ymd:	mDayPosition = 3; mMonthPosition = 2; mYearPosition = 1; break;
			case myd:	mDayPosition = 3; mMonthPosition = 1; mYearPosition = 2; break;
			case dym:	mDayPosition = 1; mMonthPosition = 3; mYearPosition = 2; break;
			default: /* ydm */	mDayPosition = 2; mMonthPosition = 3; mYearPosition = 1; break;
		}
		leadingDayChar = ((**int0H).shrtDateFmt & dayLdingZ) ? '0' : ' ';
		leadingMonthChar = ((**int0H).shrtDateFmt & mntLdingZ) ? '0' : ' ';
		separatingChar = (**int0H).dateSep;
	} else {
		// Set to defaults
		separatingChar = '/';
		leadingDayChar = '0';
		leadingMonthChar = '0';
		mDayPosition = 2;
		mMonthPosition = 1;
		mYearPosition = 3;
	}

	SetRefreshAllWhenResized(false);
	
	// Must create the subpanes here for correct function of tabbed edit fields
	
	CreateDateFields(leadingDayChar, leadingMonthChar, separatingChar);
}


/*======================================================================================
	Destructor.
======================================================================================*/

CDateView::~CDateView(void) {

}


/*======================================================================================
	Determine if the specified date is valid.
	
	inYear 		(cMinViewYear..cMaxViewYear)
	inMonth 	(1..12)
	inDay 		(1..31)
======================================================================================*/

Boolean CDateView::IsValidDate(Int16 inYear, UInt8 inMonth, UInt8 inDay) {

	if ( (inDay < 1) || (inDay > 31) || (inMonth < 1) || (inMonth > 12) ||
		 (inYear < cMinViewYear) || (inYear > cMaxViewYear) ) {
		 
		return false;
	}
	
	DateTimeRec dtRec = { inYear, inMonth, inDay, 1, 1, 1 };

	UInt32 seconds;
	::DateToSeconds(&dtRec, &seconds);
	::SecondsToDate(seconds, &dtRec);
	
	return ((inDay == dtRec.day) && (inMonth == dtRec.month) && (inYear == dtRec.year));
}


/*======================================================================================
	Get the currently displayed date.
	
	outYear 	(cMinViewYear..cMaxViewYear)
	outMonth 	(1..12)
	outDay 		(1..31)
======================================================================================*/

void CDateView::GetDate(Int16 *outYear, UInt8 *outMonth, UInt8 *outDay) {

	*outYear = mYearField->GetValue() + 2000;
	*outMonth = mMonthField->GetValue();
	*outDay = mDayField->GetValue();
	if ( *outYear > cMaxViewYear ) *outYear -= 100;
}


/*======================================================================================
	Set the displayed date to the date specified by the input parameters. If the
	input parameters specify a date that is not valid, return false and don't do
	anything; otherwise return true.
	
	inYear 		(cMinViewYear..cMaxViewYear)
	inMonth 	(1..12)
	inDay 		(1..31)
======================================================================================*/

Boolean CDateView::SetDate(Int16 inYear, UInt8 inMonth, UInt8 inDay) {

	Boolean rtnVal = IsValidDate(inYear, inMonth, inDay);

	if ( rtnVal ) {
		inYear -= ((inYear > 1999) ? 2000 : 1900);
		SetDateString(mYearField, inYear, '0');
		SetDateString(mMonthField, inMonth, mMonthField->GetLeadingChar());
		SetDateString(mDayField, inDay, mDayField->GetLeadingChar());
	}
	
	return rtnVal;
}


/*======================================================================================
	Set the date displayed to today's date.
======================================================================================*/

void CDateView::SetToToday(void) {

	UInt32 seconds;
	::GetDateTime(&seconds);
	DateTimeRec dtRec;
	::SecondsToDate(seconds, &dtRec);
	
	if ( dtRec.year < cMinViewYear ) {
		dtRec.year = cMinViewYear;
	} else if ( dtRec.year > cMaxViewYear ) {
		dtRec.year = cMaxViewYear;
	}

	SetDate(dtRec.year, dtRec.month, dtRec.day);
}


/*======================================================================================
	Select the specified date field in this view.
======================================================================================*/

void CDateView::SelectDateField(Int16 inField) {

	PaneIDT paneID;
	
	switch ( inField ) {
		case eYearField: 	paneID = eYearEditFieldID; break;
		case eMonthField: 	paneID = eMonthEditFieldID; break;
		case eDayField:		paneID = eDayEditFieldID; break;
		default: Assert_(false); return;
	}
	
	CDateField *newTarget = (CDateField *) FindPaneByID(paneID);
	Assert_(newTarget != nil);
	
	if ( LCommander::SwitchTarget(newTarget) ) {
		newTarget->SelectAll();
	}
}


/*======================================================================================
	Select the first date field in this view if one is not yet selected.
======================================================================================*/

void CDateView::Select(void) {

	// Check if we're already there
	if ( ContainsTarget() ) return;
	
	// Find the first field
	LEditField *newTarget;
	if ( mDayPosition == 1 ) {
		newTarget = mDayField;
	} else if ( mMonthPosition == 1 ) {
		newTarget = mMonthField;
	} else {
		Assert_(mYearPosition == 1);
		newTarget = mYearField;
	}
	
	if ( LCommander::SwitchTarget(newTarget) ) {
		newTarget->SelectAll();
	}
}


/*======================================================================================
	Return true if one of the date fields in this view is the current target.
======================================================================================*/

Boolean CDateView::ContainsTarget(void) {

	return (mDayField->IsTarget() || mMonthField->IsTarget() || mYearField->IsTarget());
}


/*======================================================================================
	Listen to message from broadcasters.
======================================================================================*/

void CDateView::ListenToMessage(MessageT inMessage, void *ioParam) {

	#pragma unused(ioParam)

	switch ( inMessage ) {
	
		case CDateArrowButton::msg_ClickUpArrow:
		case CDateArrowButton::msg_ClickDownArrow:
			DoClickArrow(inMessage == CDateArrowButton::msg_ClickUpArrow);
			break;
	
		case CDateField::msg_HideDateArrows:
		case CDateField::msg_ShowDateArrows:
			ShowHideArrows(inMessage == CDateField::msg_ShowDateArrows);
			break;

		case CDateField::msg_UserChangedText:
			BroadcastMessage(msg_DateViewChanged, this);
			break;
	}
}


/*======================================================================================
	Process a click in an arrow from a key.
======================================================================================*/

void CDateView::DoKeyArrow(Boolean inClickUpArrow) {

	LControl *theControl;
	if ( inClickUpArrow ) {
		theControl = (LControl *) FindPaneByID(paneID_UpButton);
	} else {
		theControl = (LControl *) FindPaneByID(paneID_DownButton);
	}
	if ( theControl && theControl->FocusDraw() ) {
		theControl->SimulateHotSpotClick(1);
	}
}


/*======================================================================================
	Process a click in an arrow.
======================================================================================*/

void CDateView::DoClickArrow(Boolean inClickUpArrow) {

	#pragma unused(inClickUpArrow)

	CDateField *theDateField = (CDateField *) LCommander::GetTarget();
	Assert_(theDateField);
	if ( !theDateField ) return;
		
	Int16 year;
	UInt8 month, day;
	
	GetDate(&year, &month, &day);
	
	// Validate that the current character is OK for the active field
	
	Int16 adder = inClickUpArrow ? 1 : -1;
	switch ( theDateField->GetPaneID() ) {
		case eDayEditFieldID: 
			day += adder;
			if ( day > 31 ) {
				day = 1;
			} else if ( day < 1 ) {
				day = 31;
			}
			break;
		case eMonthEditFieldID: 
			month += adder; 
			if ( month > 12 ) {
				month = 1;
			} else if ( month < 1 ) {
				month = 12;
			}
			break;
		default: 
			year += adder;
			if ( year > cMaxViewYear ) {
				year = cMinViewYear;
			} else if ( year < cMinViewYear ) {
				year = cMaxViewYear;
			}
			break;
	}
	
	if ( !SetDate(year, month, day) ) {
		if ( adder > 0 ) {
			// Day must be wrong, too large!
			SetDate(year, month, 1);
		} else {
			while ( !SetDate(year, month, --day) ) {
				;	// Nothing
			}
		}
	}
}


/*======================================================================================
	Hide or show the arrows.
======================================================================================*/

void CDateView::ShowHideArrows(Boolean inShow) {

	ShowHideArrow(FindPaneByID(paneID_UpButton), inShow);
	ShowHideArrow(FindPaneByID(paneID_DownButton), inShow);
}


/*======================================================================================
	Filter for incoming data characters.
======================================================================================*/

EKeyStatus CDateView::DateFieldFilter(TEHandle inMacTEH, Char16 theKey, Char16& theChar, SInt16 inModifiers) {

	EKeyStatus theKeyStatus = keyStatus_PassUp;
	//Char16 theKey = inKeyEvent.message;
	//Char16 theChar = theKey & charCodeMask;
	
	CDateField *theDateField = (CDateField *) LCommander::GetTarget();
	Assert_(theDateField);
	CDateView *theDateView = (CDateView *) theDateField->GetSuperView();
	Assert_(theDateView);
	
	if ( !theDateField || !theDateView ) return theKeyStatus;
		
	switch (theChar)
	{
		case char_UpArrow:
		case char_DownArrow:
			theKeyStatus = keyStatus_Ignore;
			theDateView->DoKeyArrow(theChar == char_UpArrow);
			break;

		case char_Tab:
		case char_Enter:
		case char_Return:
		case char_Escape:
			theKeyStatus = keyStatus_PassUp;
			break;

		default:
			if ( UKeyFilters::IsPrintingChar(theChar) && UKeyFilters::IsNumberChar(theChar) ) {
			
				PaneIDT paneID = theDateField->GetPaneID();
				Str15 currentText;
				theDateField->GetDescriptor(currentText);
				
				theKeyStatus = keyStatus_Input;
				
				Int16 year;
				UInt8 month, day;
				
				theDateView->GetDate(&year, &month, &day);
				
				// Validate that the current character is OK for the active field
				
				Str15 numString = { 2 };
				long wouldBeNumber;
				Assert_(currentText[0] == 2);
				numString[1] = currentText[2];
				numString[2] = theChar;
				::StringToNum(numString, &wouldBeNumber);
				
				switch ( paneID ) {
					case eDayEditFieldID:
						day = wouldBeNumber;
						break;

					case eMonthEditFieldID:
						month = wouldBeNumber;
						break;

					default: // eYearEditFieldID
						year = wouldBeNumber + 2000;
						if ( year > cMaxViewYear ) year -= 100;
						break;
				}
				
				if ( !theDateView->IsValidDate(year, month, day) ||
					 ((paneID != eYearEditFieldID) && (numString[1] == '0')) ) {
					if ( theChar == '0' ) {
						theKeyStatus = keyStatus_Reject;
					} else {
						currentText[1] = currentText[2] = theDateField->GetLeadingChar();
						theDateField->SetDescriptor(currentText);
					}
				}
			} else {
				theKeyStatus = keyStatus_Reject;
			}
			break;
	}
	return theKeyStatus;
}

/*======================================================================================
	Set the edit field date.
======================================================================================*/

void CDateView::SetDateString(LEditField *inField, UInt8 inValue, UInt8 inLeadingChar) {

	Str15 valueString, tempString;

	::NumToString(inValue, valueString);
	if ( valueString[0] == 1 ) {
		valueString[2] = valueString[1];
		valueString[1] = inLeadingChar;
		valueString[0] = 2;
	}
	inField->GetDescriptor(tempString);
	if ( (tempString[0] != 2) || (tempString[1] != valueString[1]) || 
		 (tempString[2] != valueString[2]) ) {
		
		inField->SetDescriptor(valueString);
		if ( inField->IsTarget() ) {
			inField->SelectAll();
		}
		inField->Draw(nil);
	}
}


/*======================================================================================
	Show or hide the arrow nicely.
======================================================================================*/

void CDateView::ShowHideArrow(LPane *inArrow, Boolean inShow) {

	if ( inArrow ) {
		if ( inShow ) {
			inArrow->DontRefresh(true);
			if ( !inArrow->IsVisible() ) {
				inArrow->Show();
				inArrow->DontRefresh();
				inArrow->Draw(nil);
			}
		} else {
			inArrow->Hide();
		}
	}
}


/*======================================================================================
	Finish creating the search dialog.
======================================================================================*/

void CDateView::CreateDateFields(UInt8 inLeadingDayChar, UInt8 inLeadingMonthChar, 
								 UInt8 inSeparatingChar) {

	// Init the panes in the view
	
	UInt8 separatingString[2] = { 1, inSeparatingChar };
	
	PaneIDT field1ID, field2ID, field3ID;
	UInt8 leadingChar1, leadingChar2, leadingChar3;
	
	switch ( mDayPosition ) {
		case 1: 	
			field1ID = eDayEditFieldID;
			leadingChar1 = inLeadingDayChar;
			if ( mMonthPosition == 2 ) {
				field2ID = eMonthEditFieldID;
				leadingChar2 = inLeadingMonthChar;
				field3ID = eYearEditFieldID;
				leadingChar3 = '0';
			} else {
				field3ID = eMonthEditFieldID;
				leadingChar3 = inLeadingMonthChar;
				field2ID = eYearEditFieldID;
				leadingChar2 = '0';
			}
			break;
		case 2: 	
			field2ID = eDayEditFieldID;
			leadingChar2 = inLeadingDayChar;
			if ( mMonthPosition == 1 ) {
				field1ID = eMonthEditFieldID;
				leadingChar1 = inLeadingMonthChar;
				field3ID = eYearEditFieldID;
				leadingChar3 = '0';
			} else {
				field3ID = eMonthEditFieldID;
				leadingChar3 = inLeadingMonthChar;
				field1ID = eYearEditFieldID;
				leadingChar1 = '0';
			}
			break;
		default:
			field3ID = eDayEditFieldID;
			leadingChar3 = inLeadingDayChar;
			if ( mMonthPosition == 1 ) {
				field1ID = eMonthEditFieldID;
				leadingChar1 = inLeadingMonthChar;
				field2ID = eYearEditFieldID;
				leadingChar2 = '0';
			} else {
				field2ID = eMonthEditFieldID;
				leadingChar2 = inLeadingMonthChar;
				field1ID = eYearEditFieldID;
				leadingChar1 = '0';
			}
			break;
	}
	
	// Field 1
	CDateField *theDateField = (CDateField *) FindPaneByID(paneID_DateField1);
	ThrowIfResFail_(theDateField);
	theDateField->SetPaneID(field1ID);
	theDateField->SetLeadingChar(leadingChar1);
	theDateField->SetKeyFilter(DateFieldFilter);
	theDateField->AddListener(this);

	// Separator 1
	LCaption *theCaption = (LCaption *) FindPaneByID(paneID_Separator1);
	ThrowIfResFail_(theCaption);
	theCaption->SetDescriptor(separatingString);

	// Field 2
	theDateField = (CDateField *) FindPaneByID(paneID_DateField2);
	ThrowIfResFail_(theDateField);
	theDateField->SetPaneID(field2ID);
	theDateField->SetLeadingChar(leadingChar2);
	theDateField->SetKeyFilter(DateFieldFilter);
	theDateField->AddListener(this);

	// Separator 2
	theCaption = (LCaption *) FindPaneByID(paneID_Separator2);
	ThrowIfResFail_(theCaption);
	theCaption->SetDescriptor(separatingString);

	// Field 3
	theDateField = (CDateField *) FindPaneByID(paneID_DateField3);
	ThrowIfResFail_(theDateField);
	theDateField->SetPaneID(field3ID);
	theDateField->SetLeadingChar(leadingChar3);
	theDateField->SetKeyFilter(DateFieldFilter);
	theDateField->AddListener(this);

	// Up/down arrows

	LButton *theButton = (LButton *) FindPaneByID(paneID_UpButton);
	ThrowIfResFail_(theButton);
	theButton->AddListener(this);

	theButton = (LButton *) FindPaneByID(paneID_DownButton);
	ThrowIfResFail_(theButton);
	theButton->AddListener(this);

	mDayField = (CDateField *) FindPaneByID(eDayEditFieldID);
	mMonthField = (CDateField *) FindPaneByID(eMonthEditFieldID);
	mYearField = (CDateField *) FindPaneByID(eYearEditFieldID);

	// Set the date to the current date
	
	ShowHideArrows(eHideArrows);
	SetToToday();
}

#pragma mark -

/*======================================================================================
	Constructor.
======================================================================================*/

CDateField::CDateField(LStream *inStream) :
					   LGAEditField(inStream) {
	mLeadingChar = '/';
	Str15 initString = { 2, mLeadingChar, mLeadingChar };
	SetDescriptor(initString);
}


/*======================================================================================
	Set the text for the date field. This method does not refresh or redraw the
	field after setting the text.
======================================================================================*/

void CDateField::SetDescriptor(ConstStr255Param inDescriptor) {

	if ( FocusExposed() ) {
		//StEmptyVisRgn emptyRgn(GetMacPort());
		::TESetText(inDescriptor + 1, StrLength(inDescriptor), mTextEditH);
	} else {
		::TESetText(inDescriptor + 1, StrLength(inDescriptor), mTextEditH);
	}
}


/*======================================================================================
	Disallow pasting and cutting.
======================================================================================*/

void CDateField::FindCommandStatus(CommandT	inCommand, Boolean &outEnabled,
								   Boolean &outUsesMark, Char16 &outMark,
								   Str255 outName) {

	switch ( inCommand ) {
	
		case cmd_Paste:
		case cmd_Cut:
		case cmd_Clear:
		case cmd_SelectAll:
			outEnabled = false;
			break;
	
		default:
			LEditField::FindCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName);
			break;
	}
}


/*======================================================================================
	Select all text when clicking.
======================================================================================*/

void CDateField::ClickSelf(const SMouseDownEvent &inMouseDown) {

	#pragma unused(inMouseDown)
	
	if ( !IsTarget() ) {
		if ( LCommander::SwitchTarget(this) ) {
			this->SelectAll();
		}
	}
}


/*======================================================================================
	Handle a key press.
======================================================================================*/

Boolean CDateField::HandleKeyPress(const EventRecord &inKeyEvent) {

	Boolean keyHandled;
	
	if ( (inKeyEvent.modifiers & cmdKey) || !mKeyFilter ) {
	
		keyHandled = LCommander::HandleKeyPress(inKeyEvent);
	
	} else {
		
		Char16 theChar = inKeyEvent.message & charCodeMask;
		//EKeyStatus theKeyStatus = (*mKeyFilter)(inKeyEvent, (**mTextEditH).selStart);
		EKeyStatus theKeyStatus = (*mKeyFilter)(mTextEditH, inKeyEvent.message & keyCodeMask, theChar, inKeyEvent.modifiers);
		
		switch ( theKeyStatus ) {
		
			case keyStatus_PassUp:
				keyHandled = LCommander::HandleKeyPress(inKeyEvent);
				break;

			case keyStatus_Reject:
				SysBeep(1);
				break;
				
			case keyStatus_Input: {
		
					// Get the current string
					
					Str15 numString;
					GetDescriptor(numString);
					
					Assert_(numString[0] == 2);
					
					numString[1] = numString[2];
					numString[2] = theChar;
					
					Boolean doDraw = FocusExposed();
					
					SetDescriptor(numString);
					SelectAll();
				
					if ( doDraw ) {
						DrawSelf();
					}
					
					keyHandled = true;
				}
				break;
		}
	}
	
	return keyHandled;
}


/*======================================================================================
	Select all text when clicking.
======================================================================================*/

void CDateField::AdjustCursorSelf(Point inPortPt, const EventRecord &inMacEvent) {

	LPane::AdjustCursorSelf(inPortPt, inMacEvent);
}


/*======================================================================================
	EditField is becoming the Target.
======================================================================================*/

void CDateField::BeTarget(void) {

	StFocusAndClipIfHidden focus(this);
	::TEActivate(mTextEditH);		// Show active selection
	BroadcastMessage(msg_ShowDateArrows);
}


/*======================================================================================
	EditField is no longer the Target.
======================================================================================*/

void CDateField::DontBeTarget(void) {

	StFocusAndClipIfHidden focus(this);
	::TEDeactivate(mTextEditH);		// Show inactive selection
	BroadcastMessage(msg_HideDateArrows);
}


/*======================================================================================
	The user has changed some text, broadcast a message.
======================================================================================*/

void CDateField::UserChangedText(void) {

	LEditField::UserChangedText();
	BroadcastMessage(msg_UserChangedText, this);
}


#pragma mark -

/*======================================================================================
	Constructor.
======================================================================================*/

CDateArrowButton::CDateArrowButton(LStream *inStream) :
					   			   LButton(inStream) {

	mNextBroadcastTime = 0;
	mBroadcastCount = 0;
}


/*======================================================================================
======================================================================================*/

void CDateArrowButton::DisableSelf(void) {

	Draw(nil);
}


/*======================================================================================
======================================================================================*/

void CDateArrowButton::EnableSelf(void) {

	Draw(nil);
}


/*======================================================================================
======================================================================================*/

Boolean CDateArrowButton::TrackHotSpot(Int16 inHotSpot, Point inPoint, Int16 inModifiers) {

	mBroadcastCount = 0;
	mNextBroadcastTime = 0;

	return LButton::TrackHotSpot(inHotSpot, inPoint, inModifiers);
}


/*======================================================================================
======================================================================================*/

void CDateArrowButton::HotSpotAction(Int16 inHotSpot, Boolean inCurrInside, Boolean inPrevInside) {

	LButton::HotSpotAction(inHotSpot, inCurrInside, inPrevInside);
	
	if ( inCurrInside ) {
		if ( mNextBroadcastTime < LMGetTicks() ) {
			BroadcastValueMessage();
			if ( mBroadcastCount > 4 ) {
				mNextBroadcastTime = LMGetTicks() + (cBroadcastMessageDelay/3);
			} else {
				++mBroadcastCount;
				mNextBroadcastTime = LMGetTicks() + cBroadcastMessageDelay;
			}
		}
	} else {
		mBroadcastCount = 0;
	}
}


/*======================================================================================
======================================================================================*/

void CDateArrowButton::HotSpotResult(short inHotSpot) {

	if ( mBroadcastCount ) {
		HotSpotAction(inHotSpot, false, true);
	} else {
		LButton::HotSpotResult(inHotSpot);
	}
}


/*======================================================================================
======================================================================================*/

void CDateArrowButton::SimulateHotSpotClick(Int16 inHotSpot) {

	if ( IsEnabled() ) {
		
		mBroadcastCount = 0;
		mNextBroadcastTime = 0;

		unsigned long endTicks;
		HotSpotAction(inHotSpot, true, false);	// Do action for click inside
		::Delay(4, &endTicks);
		//HotSpotAction(inHotSpot, false, true);	// Undo visual effect
		HotSpotResult(inHotSpot);
		::Delay(4, &endTicks);
	}
}



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

#ifndef __H_CDateView
#define __H_CDateView
#pragma once

/*======================================================================================

	DESCRIPTION:	Implements a view for display and input of a date. Formats the date
					as the user has it set up in their Date & Time control panel.
					
					To use this class, you must include the following PP classes:
					
						LGAEditField
						LCaption
						LButton

	MODIFICATIONS:

	Date			Person			Description
	----			------			-----------
======================================================================================*/


/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

#include <LView.h>
#include <LListener.h>
#include <LGAEditField.h>

class CDateField;


/*====================================================================================*/
	#pragma mark TYPEDEFS
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark CONSTANTS
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark CLASS DECLARATIONS
/*====================================================================================*/

#pragma mark -

class CDateView : public LView,
				  public LListener,
				  public LBroadcaster {
				  
public:

	enum {	// Pane IDs
		paneID_DateField1 = 'DAT1'
	,	paneID_DateField2 = 'DAT2'
	,	paneID_DateField3 = 'DAT3'
	,	paneID_Separator1 = 'SEP1'
	,	paneID_Separator2 = 'SEP2'
	,	paneID_UpButton = 	'BTNU'
	,	paneID_DownButton = 'BTND'
	};

	// Stream creator method

	enum { class_ID = 'DaTe' };
	static void			RegisterDateClasses(void);

						CDateView(LStream *inStream);
	virtual				~CDateView(void);
	
	// Start public interface ----------------------------------------------------------

	enum { cMinViewYear = 1920, cMaxViewYear = 2019 };

	Boolean				IsValidDate(Int16 inYear, UInt8 inMonth, UInt8 inDay);
	void				GetDate(Int16 *outYear, UInt8 *outMonth, UInt8 *outDay);

	Boolean				SetDate(Int16 inYear, UInt8 inMonth, UInt8 inDay);
	void				SetToToday(void);

	enum { eYearField = 1, eMonthField = 2, eDayField = 3 };
	void				SelectDateField(Int16 inField);
	void				Select(void);

	Boolean				ContainsTarget(void);
	
	// Boradcasting messages
	enum { msg_DateViewChanged = 9109221 /*this*/ };

	// End public interface ------------------------------------------------------------

protected:

	// Overriden methods

	virtual void		FinishCreateSelf(void);
	virtual void		ListenToMessage(MessageT inMessage, void *ioParam = nil);
	
	void				DoKeyArrow(Boolean inClickUpArrow);
	void				DoClickArrow(Boolean inClickUpArrow);

	enum { eShowArrows = true, eHideArrows = false };
	void				ShowHideArrows(Boolean inShow);

	// static EKeyStatus	DateFieldFilter(const EventRecord &inKeyEvent, Int16 inKeyPosition = 0);
	static EKeyStatus	DateFieldFilter(TEHandle inMacTEH, Char16 inKeyCode, Char16& ioCharCode, SInt16 inModifiers);
	static void			SetDateString(LEditField *inField, UInt8 inValue, UInt8 inLeadingChar);
	static void			ShowHideArrow(LPane *inArrow, Boolean inShow);

private:

	void				CreateDateFields(UInt8 inLeadingDayChar, UInt8 inLeadingMonthChar, 
								 		 UInt8 inSeparatingChar);

protected:

	// Instance variables ==========================================================
	
	UInt8				mDayPosition;		// Position of the day in the view (1..3)
	UInt8				mMonthPosition;		// Position of the month in the view (1..3)
	UInt8				mYearPosition;		// Position of the year in the view (1..3)
	
	Rect 				mFrameRect;
	
	CDateField			*mDayField;
	CDateField			*mMonthField;
	CDateField			*mYearField;
};


#pragma mark -

class CDateField : public LGAEditField {

public:

	enum { class_ID = 'DaFd' };
						CDateField(LStream *inStream);

	virtual void		SetDescriptor(ConstStr255Param inDescriptor);
	UInt8				GetLeadingChar(void) const { return mLeadingChar; }
	void				SetLeadingChar(UInt8 inChar) { mLeadingChar = inChar; }
	
	// Boradcasting messages
	enum { msg_HideDateArrows = 780954, msg_ShowDateArrows = 780955,
		   msg_UserChangedText = 780956 };

protected:

	// Overriden methods

	virtual void		ClickSelf(const SMouseDownEvent &inMouseDown);
	virtual void		FindCommandStatus(CommandT	inCommand, Boolean &outEnabled,
								  		  Boolean &outUsesMark, Char16 &outMark,
								   		  Str255 outName);
	virtual Boolean		HandleKeyPress(const EventRecord &inKeyEvent);
	virtual void		AdjustCursorSelf(Point inPortPt, const EventRecord &inMacEvent);

	virtual void		BeTarget(void);
	virtual void		DontBeTarget(void);

	virtual void		UserChangedText(void);

	// Instance variables ==========================================================
	
	UInt8				mLeadingChar;		// Character to use for leading number when
};

#endif // __H_CDateView

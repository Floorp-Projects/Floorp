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

#ifndef __H_CSaveWindowStatus
#define __H_CSaveWindowStatus
#pragma once

/*======================================================================================
	AUTHOR:			Ted Morris <tmorris@netscape.com> - 08 NOV 96

	DESCRIPTION:	Defines a mix-in class for saving a window's status to the preferences
					file.

	MODIFICATIONS:

	Date			Person			Description
	----			------			-----------
======================================================================================*/


/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

class LWindow;
class LStream;


#pragma mark -
/*====================================================================================*/
	#pragma mark CLASS DECLARATIONS
/*====================================================================================*/

class CSaveWindowStatus {
					  
public:

						CSaveWindowStatus(LWindow *inWindow) {
							mWindowSelf = inWindow;
							mCanSaveStatus = true;
							mHasSavedStatus = false;
							Assert_(mWindowSelf != nil);
						}

	Boolean				CanSaveStatus() {
							return mCanSaveStatus;
						}
	void				SetCanSaveStatus(Boolean inCanSave) {
							mCanSaveStatus = inCanSave;
						}
	
						enum { 
							cWindowDesktopMargin = 4
						,	cMinSavedStatusSize = sizeof(Int16) + sizeof(Rect)
						};

	static void			GetPaneGlobalBounds(LPane *inPane, Rect *outBounds);
	static void			MoveWindowToAlertPosition(LWindow *inWindow);
	static void			MoveWindowTo(LWindow *inWindow, Point inGlobalTopLeft);
	static void			VerifyWindowBounds(LWindow *inWindow, Rect *ioGlobalBounds);
	static LWindow		*CreateWindow(ResIDT inWindowID, LCommander *inSuperCommander);
	
protected:

	void				FinishCreateWindow(); // from disk
	void				FinishCreateWindow(CSaveWindowStatus* templateStatus);
	void				AttemptCloseWindow();
	void				SaveStatusInfo();
	
						// It is only valid to call HasSavedStatus() after
						// the window has been properly created (i.e. after
						// calling FinishCreateWindow() ).
						
	Boolean				HasSavedStatus()
						{
							return mHasSavedStatus;
						}

	virtual ResIDT		GetStatusResID() const = 0; // client must provide!
	virtual UInt16		GetValidStatusVersion() const = 0; // client must provide!
	virtual void		ReadWindowStatus(LStream *inStatusData);
	virtual void		WriteWindowStatus(LStream *outStatusData);
	
	LWindow				*mWindowSelf;
	Boolean				mCanSaveStatus;
	
private:
	Boolean				mHasSavedStatus;
};
#endif // __H_CSaveWindowStatus


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
 * Copyright (C) 1996 Netscape Communications Corporation.  All Rights
 * Reserved.
 */



// CMailNewsWindow.h

#pragma once

// Mac UI Lib
#include "CNetscapeWindow.h"
#include "CSaveWindowStatus.h"

class CProgressListener;
class CMailNewsContext;
class CMailNewsFolderContext;
class CMailFlexTable;

const PaneIDT cMessageToolbar			= 'BBar';
const PaneIDT cMailNewsLocationToolbar	= 'BnBr';


//======================================
class CMailNewsWindow : public CNetscapeWindow, public CSaveWindowStatus
// Base class for all content windows in mail.
// implements the progress bar and the status line etc.
//======================================
{
private:
	typedef CNetscapeWindow Inherited; // trick suggested by the ANSI committee.
	
protected:
	// Indices into mToolbarShown for tracking visibility of toolbars
	enum { MESSAGE_TOOLBAR, LOCATION_TOOLBAR };
	CMailNewsWindow(LStream *inStream, DataIDT inWindowType);
	
public:
	virtual ~CMailNewsWindow();

	virtual CNSContext* CreateContext() const; // allow each window to create its own
	virtual void FinishCreateSelf();
	virtual CNSContext* GetWindowContext() const { return (CNSContext*)mMailNewsContext; }
	
	// Return the currently active table in the window, nil if none.
	// Can't be const, because derived classes need to call FindPaneByID(), which isn't.
	virtual CMailFlexTable* GetActiveTable() { return nil; }
	virtual CMailFlexTable* GetSearchTable() { return GetActiveTable(); }
	virtual void			FindCommandStatus(
								CommandT			inCommand,
								Boolean				&outEnabled,
								Boolean				&outUsesMark,
								Char16				&outMark,
								Str255				outName);
	virtual Boolean			ObeyCommand(
								CommandT			inCommand,
								void				*ioParam = nil);

	//-----------------------------------
	// Window closing and saving - overrides for CSaveWindowStatus
	//-----------------------------------
public:
	virtual void		AttemptClose();
	virtual Boolean		AttemptQuitSelf(Int32 /* inSaveOption */);
	virtual void		DoClose();
	CProgressListener*	GetProgressListener()	{return mProgressListener;}
protected:
	virtual void		AboutToClose(); // place to put common code from [Attempt|Do]Close()
	virtual void		ReadWindowStatus(LStream *inStatusData);
	virtual void		WriteWindowStatus(LStream *outStatusData);
	virtual void		ReadGlobalDragbarStatus();
	virtual void		WriteGlobalDragbarStatus();
	virtual const char* GetLocationBarPrefName() const;

	//-----------------------------------
	// Preferences
	//-----------------------------------
	virtual	void		DoDefaultPrefs();

	//-----------------------------------
	//data
	//-----------------------------------
protected:
	CProgressListener*		mProgressListener;
	CNSContext*				mMailNewsContext;

};

//======================================
class CMailNewsFolderWindow : public CMailNewsWindow
//======================================
{
private:
	typedef CMailNewsWindow Inherited;
public:
	enum { class_ID = 'mnWN', res_ID = 10507};
	
	virtual							~CMailNewsFolderWindow();
									CMailNewsFolderWindow(LStream *inStream);
	virtual ResIDT					GetStatusResID(void) const { return res_ID; }
	virtual UInt16					GetValidStatusVersion(void) const;

	virtual void					FinishCreateSelf();
	virtual void					CalcStandardBoundsForScreen(const Rect &inScreenBounds,
										Rect &outStdBounds) const;
	
	// Return the currently active table in the window, nil if none
	static CMailNewsFolderWindow*	FindAndShow(Boolean inMakeNew, CommandT inCommand = 0);
	virtual CMailFlexTable*			GetActiveTable();
};


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
 * Copyright (C) 1997 Netscape Communications Corporation.  All Rights
 * Reserved.
 */



// CSubscribeWindow.h

#pragma once

// C/C++ headers
//#include <yvals.h>

// PowerPlant
#include <LListener.h>
#include <LBroadcaster.h>
#include <LEditField.h>
#include <LCommander.h>
#include <LHandleStream.h>
#include <LGAPopup.h>

// Mail/News MacFE stuff
#include "CBrowserContext.h"
#include "CComposeAddressTableView.h"

// UI elements
#include "CMailNewsWindow.h"
#include "CMailComposeWindow.h"
#include "CPatternBevelView.h"
#include "CTabSwitcher.h"
#include "UMailFolderMenus.h"

// Netscape stuff
#include "msgcom.h"
#include "mattach.h"
//#include "MailNewsGUI.h"


class CMailComposeWindow;
class CProgressListener;
class CComposeSession;
class CSubscribeView;


//----------------------------------------------------------------------------
//	USubscribeUtilities
//
//----------------------------------------------------------------------------
class USubscribeUtilities
{
public:
	static void				RegisterSubscribeClasses();
};


//----------------------------------------------------------------------------
//	CNewsHostIterator
//
//----------------------------------------------------------------------------

class CNewsHostIterator
{
	public:
		enum { kMaxNewsHosts = 32 };

							CNewsHostIterator(MSG_Master* master);
		Boolean				Current(MSG_Host*& outNewsHost);
		Boolean				Next(MSG_Host*& outNewsHost);

		Boolean				FindIndex(const MSG_Host* inNewsHost, Int32& outIndex);

		void				ResetTo(Int32 current)		{mIndex = current;};
		Int32				GetCurrentIndex()			{return mIndex;};
		static Int32		GetCount()					{return mHostCount;};
		static MSG_Host**	GetHostList()				{return mHostList;};

	protected:
		static MSG_Host*	mHostList[kMaxNewsHosts];
		static Int32		mHostCount;
		Int32				mIndex;
};


//----------------------------------------------------------------------------
//	CSubscribeHostPopup
//
//----------------------------------------------------------------------------

class CSubscribeHostPopup :	public LGAPopup
{
public:
	enum { class_ID = 'SHpp' };

						CSubscribeHostPopup(LStream* inStream);
	virtual				~CSubscribeHostPopup();

	virtual void		FinishCreateSelf();
	virtual void		ReloadMenu();

protected:
	virtual void		BroadcastValueMessage();

protected:
	CNewsHostIterator*	mNewsHostIterator;
	static void*		sMercutioCallback;
};


//----------------------------------------------------------------------------
//	CSubscribeWindow
//
//----------------------------------------------------------------------------

class CSubscribeWindow :	public CMailNewsWindow,
							public LBroadcaster,
							public LListener
{
private:
	typedef CMailNewsWindow Inherited; // trick suggested by the ANSI committee.

public:
	enum { class_ID = 'Subs', res_ID = 14006 };

protected:
	virtual ResIDT		GetStatusResID(void) const			{ return res_ID; }
	virtual UInt16		GetValidStatusVersion(void) const	{ return 0x0112; }

public:	
	static CSubscribeWindow*	FindAndShow(Boolean makeNew);
	static Boolean				DisplayDialog();

						CSubscribeWindow(LStream* inStream);
	virtual				~CSubscribeWindow();

	virtual void		FinishCreateSelf();
	virtual void		DoClose();

	CMailFlexTable*		GetActiveTable();

	virtual void		CalcStandardBoundsForScreen(
							const Rect 	&inScreenBounds,
							Rect 		&outStdBounds) const;
		
	virtual void		ListenToMessage(MessageT inMessage, void* ioParam);

	virtual void		FindCommandStatus(
								CommandT			inCommand,
								Boolean				&outEnabled,
								Boolean				&outUsesMark,
								Char16				&outMark,
								Str255				outName);

	virtual Boolean		ObeyCommand(
								CommandT			inCommand,
								void				*ioParam);

	virtual Boolean		HandleKeyPress(const EventRecord &inKeyEvent);

protected:
	Boolean				CommandDelegatesToSubscribeList(CommandT inCommand);

	void				HandleAllGroupsTabActivate();
	void				HandleSearchGroupsTabActivate();
	void				HandleNewGroupsTabActivate();
	void				HandleSearchInList();
	
	CSubscribeView*		mList;
};

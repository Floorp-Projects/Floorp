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



// CMailProgressWindow.h

#include "CDownloadProgressWindow.h"

#include "msgcom.h"

class CMailCallbackListener;
class CMailNewsContext;
class CStr255;
class CContextProgress;

//-----------------------------------
class CMailProgressWindow : public CDownloadProgressWindow, public LPeriodical
// This class exists because mail stuff does not get the "progress begin/end/update" calls.
// Also, it has a timer, and shows itself automatically after a short delay.  It also
// takes itself down if nobody told it to come down after some time.
//-----------------------------------
{
	private:
		typedef CDownloadProgressWindow Inherited;
	public:
		enum { class_ID = 'MPWd', res_ID_modal = 10526, res_ID_modeless = 10527 };
	//--------
	// static
	//--------
		static CMailProgressWindow* CreateModalParasite(
										ResIDT inResID,
										MSG_Pane* inPane,
										const CStr255& inCommandName);
		static void					RemoveModalParasite() { delete sModalMailProgressWindow; }
		static CMailProgressWindow*	GetModal() { return sModalMailProgressWindow; }
		static CMailProgressWindow* ObeyMessageLibraryCommand(
										ResIDT inResID,
										MSG_Pane* inPane,
										MSG_CommandType inCommand,
										const CStr255& inCommandName,
										MSG_ViewIndex* inSelectedIndices = nil,
										int32 inNumIndices = 0);
		static CMailProgressWindow* ToggleOffline(
										ResIDT inResID = res_ID_modeless);
		static CMailProgressWindow* SynchronizeForOffline(
										const CStr255& inCommandName,
										ResIDT inResID = res_ID_modeless);
		static CMailProgressWindow* CleanUpFolders();
		static Boolean				GettingMail () { return sGettingMail; }
		
		static MSG_Pane*			JustGiveMeAPane(
										const CStr255& inCommandName,
										MSG_Pane* inParentPane = nil);
										// creates a MSG_Pane attached to
										// a modeless progress window.
	//--------
	// non-static
	//--------
									CMailProgressWindow(LStream* inStream);
		virtual						~CMailProgressWindow();
		virtual ResIDT				GetStatusResID() const; // client must provide!
		virtual UInt16				GetValidStatusVersion() const; // client must provide!

		virtual void				Show();
		virtual	void				ListenToMessage(
											MessageT			inMessage,
											void*				ioParam);
		virtual void				NoteProgressBegin(const CContextProgress& inProgress);

		void						ListenToPane(MSG_Pane* inPane);
		virtual void				SpendTime(const EventRecord&);
		void						SetDelay(UInt16 delay) { mDelay = delay; }
		void						SetCancelCallback(Boolean (*f)()) { mCancelCallback = f; }
		MSG_CommandType				GetCommandBeingServiced() const { return mCommandBeingServiced; }
		
	protected:
	
		static CMailProgressWindow* CreateWindow(
										ResIDT inResID,
										MSG_Pane* inPane,
										const CStr255& inCommandName);
		static CMailProgressWindow* CreateIndependentWindow(
										ResIDT inResID,
										MSG_Pane* inPane,
										const CStr255& inCommandName,
										UInt16 inDelay = 0);
		virtual void				FinishCreateSelf();

//-------
// data
//-------
	protected:
	
		UInt32						mStartTime; // in ticks;
		UInt16						mDelay; // ditto
		CMailCallbackListener*		mCallbackListener;
		UInt32						mLastCall; // ticks
		MSG_Pane*					mPane; // only used for the modeless window.
		MSG_Pane*					mParentPane; // ditto.
		Boolean						(*mCancelCallback)();
		MSG_CommandType				mCommandBeingServiced; // the command we're handling
static	Boolean						sGettingMail; // true if an instance of us is getting mail
static	CMailProgressWindow*		sModalMailProgressWindow;
}; // CMailProgressWindow

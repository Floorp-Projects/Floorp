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

#pragma once

#include <LListener.h>
#include "CWindowMediator.h"
#include "CNetscapeWindow.h"
#include "CProgressListener.h"
#include "CSaveWindowStatus.h"
#include "divview.h"

#include "ntypes.h"

// menubar manager will adjust menubar according to browser window's menubar mode
typedef enum {
	BrWn_Menubar_None, // hide menubar
	BrWn_Menubar_Minimal, // File and Edit menus only
	BrWn_Menubar_Default // standard browser window menubar
} BrWn_Menubar_Mode;


class CBrowserContext;
class CNSContext; // mjc
class CHTMLView;
class CRDFCoordinator;


class CBrowserWindow : public CNetscapeWindow, public CSaveWindowStatus, public LListener
{
	public:
		enum { class_ID = 'BrWn', 
				res_ID = 1010, 
				dialog_res_ID = 1013, 
				titlebarless_res_ID = 1012, 
				floating_res_ID = 1014};
				
		enum EBooleanParms {
			kShow		= true,
			kDontShow	= false,
			kSelect		= true,
			kDontSelect	= false
		};
		
		typedef CNetscapeWindow super;
						
								CBrowserWindow(LStream* inStream);
		virtual					~CBrowserWindow();
		
		virtual	void			SetWindowContext(CBrowserContext* inContext);
		virtual	CNSContext*		GetWindowContext() const;


		virtual void			FindCommandStatus(
										CommandT			inCommand,
										Boolean				&outEnabled,
										Boolean				&outUsesMark,
										Char16				&outMark,
										Str255				outName);

		virtual Boolean			HandleKeyPress(const EventRecord &inKeyEvent);

		virtual Boolean			ObeyCommand(
										CommandT			inCommand,
										void				*ioParam = nil);

		virtual	void			ListenToMessage(
										MessageT			inMessage,
										void* 				ioParam);
		
		virtual void			AttemptClose(void);
								
		virtual void			DoClose(void)
								{
									AttemptCloseWindow();
									CMediatedWindow::DoClose();
								}
								
									// deeje 1997-01-13
								// inFirstTime can be specified when setting chrome info on a window for the first time.
								// This allows some parameters to be set once and locked (eg. size of non-resizable window).
		virtual void			SetChromeInfo(Chrome* theChrome, Boolean inNotifyMenuBarModeChanged = false, Boolean inFirstTime = false);
		virtual void			GetChromeInfo(Chrome* theChrome);
		
		static CBrowserWindow*	MakeNewBrowserWindow(Boolean inShow = kShow, Boolean inSelect = kSelect);

		// FindAndShow returns an empty new CBrowserWindow without putting
		// a new CNSContext in the window
		static CBrowserWindow*	FindAndShow(Boolean inMakeNew = false);
		// FindAndPrepare returns a new CBrowserWindow with a CNSContext already
		// hooked in the window
		static CBrowserWindow*	FindAndPrepareEmpty(Boolean inMakeNew = false);
		
		static CBrowserWindow*	WindowForContext(CBrowserContext* inContext);

		static CBrowserWindow*	GetWindowByID(const Int32 windowID);

		virtual void			Select(); // mjc modified for always on bottom
		virtual void			ActivateSelf();
				
		virtual void			ClickInDrag(const EventRecord &inMacEvent);
		virtual void			ClickInGrow(const EventRecord &inMacEvent);
		
		// override to send javascript move, resize, and zoom events
		// these might be called from handling an AppleEvent.
		virtual void	DoSetPosition(Point inPosition);
		virtual void	DoSetBounds(const Rect &inBounds);
		virtual void	DoSetZoom(Boolean inZoomToStdState);

		virtual void	CalcStandardBoundsForScreen(
												const Rect&		inScreenBounds,
												Rect&			outStdBounds) const;

		virtual BrWn_Menubar_Mode GetMenubarMode() { return mMenubarMode; }
		
		virtual Boolean			IsRestrictedTarget(void);
		Boolean					IsAlwaysLowered(void) { return mAlwaysOnBottom; }
		Boolean					IsZLocked(void) { return mZLocked; }
		Boolean					CommandsAreDisabled(void) { return mCommandsDisabled; }
		
		// AppleEvent support
		virtual void			HandleAppleEvent(
										const AppleEvent	&inAppleEvent,
										AppleEvent			&outAEReply,
										AEDesc				&outResult,
										Int32				inAENumber);

		void					SendAEGetURL(const char* url);
		void 					SendAEGo( OSType direction ); // mjc
		void					DoAEGo(OSType direction);

		static void 	HandleGetURLEvent(
								const AppleEvent	&inAppleEvent,
								AppleEvent			&outAEReply,
								AEDesc				&outResult,
								CBrowserWindow*		inBrowserWindow = nil);
		static void 	HandleOpenURLEvent(
								const AppleEvent	&inAppleEvent,
								AppleEvent			&outAEReply,
								AEDesc				&outResult,
								CBrowserWindow*		inBrowserWindow = nil);

		// enable/disable subview's context-sensitive popup menus
		void			AllowSubviewPopups (Boolean inAllow)
							{ mAllowSubviewPopups = inAllow; }
		Boolean			AllowSubviewPopups (void)
							{ return mAllowSubviewPopups; }
								
		Boolean			IsRootDocInfo() { return mIsRootDocInfo; }
		Boolean			IsViewSource() { return mIsViewSource; }
		Boolean			IsHTMLHelp() { return mIsHTMLHelp; }
								
		// override to send javascript move and resize events when the user performs the action
		// because ClickInDrag moves the window but doesn't execute its AppleEvent.
		virtual void	SendAESetPosition(Point inPosition, Boolean inExecuteAE);
		
		// I18N stuff
		virtual Int16			DefaultCSIDForNewWindow(void);
		virtual Int16 			GetDefaultCSID(void) const;
		
		
			// better bookmarks support
		virtual void			DoDefaultPrefs();
		
			// AEOM support
		virtual void		GetAEProperty(DescType inProperty,
										  const AEDesc &inRequestedType,
										  AEDesc& outPropertyDesc) const;

		
	protected:
		virtual	void				FinishCreateSelf(void);
		const CHTMLView*			GetHTMLView() const { return mHTMLView; }
		CHTMLView*					GetHTMLView() { return mHTMLView; }
		const CRDFCoordinator*		GetNavCenterParentView() const { return mNavCenterParent; }
		CRDFCoordinator*			GetNavCenterParentView() { return mNavCenterParent; }

		virtual void			HandleNetSearchCommand();

		virtual	void			NoteDocTitleChanged(const char* inTitle);
		virtual void			NoteBeginLayout(void);
		virtual	void			NoteFinishedLayout(void);
		virtual void			NoteAllConnectionsComplete(void);

		virtual ResIDT			GetStatusResID(void) const { return res_ID; }
		virtual UInt16			GetValidStatusVersion(void) const { return 0x0113; }
		
		virtual void			ShowStatus(Chrome* inChromeInfo); // 97-05-12 pkc -- pass in Chrome struct
		
		virtual void			ShowDragBars(Boolean inShowNavBar, Boolean inShowLocBar, Boolean inShowDirBar); // mjc
		
		void					ClearSystemGrowIconBevel();
		void					SetSystemGrowIconBevel();

		void					WriteWindowStatus ( LStream *outStatusData ) ;
		void					ReadWindowStatus ( LStream *outStatusData ) ;
		
		// I18N stuff
		virtual void 			SetDefaultCSID(Int16);
		
//Boolean mIsTitled;

		CBrowserContext*		mContext;
		CProgressListener*		mProgressListener;
		Boolean					mAlwaysOnBottom; // javascript alwaysLowered window property - mjc
		Boolean					mZLocked; // javascript z-lock window property
		BrWn_Menubar_Mode		mMenubarMode; // specifies menubar for window
		Boolean					mCommandsDisabled; // javascript hotkeys window option
		
		ProcessSerialNumber		fCloseNotifier;		// apple event support
		Boolean					mSupportsPageServices;
		Boolean					mAllowSubviewPopups;
		
		Boolean					mIsRootDocInfo;
		Boolean					mIsViewSource;
		Boolean					mIsHTMLHelp;
		
	private:
		CRDFCoordinator*		mNavCenterParent;			// top level of navcenter hierarchy
		CHTMLView*				mHTMLView;
};

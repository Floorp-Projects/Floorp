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

/* Portions copyright Metrowerks Corporation. */


// A a temporary solution, some lines have been out-commented
// with the tag //••• REVISIT NOVA MERGE •••
//

// ===========================================================================
// UApp.cp
// main and CFrontApp's methods
// Created by atotic, June 6th, 1994
// ===========================================================================

//#define DONT_DO_MOZILLA_PROFILE
//#define PROFILE_UPDATE_MENUS

#include "fullsoft.h"

#include "uapp.h"

#include "CAppleEventHandler.h"
//#include "shist.h"

#include <LGARadioButton.h>
#include <LGACheckbox.h>

	// macfe
//#include "NavigationServicesSupport.h"
#include "earlmgr.h"
#include "macutil.h"
#include "macgui.h"	// HyperStyle
#include "CBookmarksAttachment.h"
#include "CToolsAttachment.h"
#include "CFontMenuAttachment.h"
#include "CRecentEditMenuAttachment.h"
#include "BookmarksFile.h"
#include "mprint.h"
#include "mimages.h"
#include "mplugin.h"
#include "prpriv.h"

#if defined (JAVA)
#include "mjava.h"
#endif

#include "mregistr.h"
#include "resae.h"
#include "resgui.h"
#include "uerrmgr.h"
#include "ufilemgr.h"
#include "msv2dsk.h"
#include "mversion.h"
#include "xp_sec.h"
#include "xp_trace.h"
#include "CTargetedUpdateMenuRegistry.h"
#include "UDesktop.h"
#include "CNavCenterWindow.h"
#include "UDeferredTask.h"
#include "URobustCreateWindow.h"
#include "URDFUtilities.h"
#ifdef MOZ_MAIL_NEWS
#include "CMessageFolder.h"
#include "UNewFolderDialog.h"
#endif

#include "LMenuSharing.h"
#define MENU_SHARING_FIRST 40

#ifndef NSPR20
#include "CNetwork.h"
#endif
#include "CEnvironment.h"

#include "CWindowMenu.h"
#include "CHistoryMenu.h"
#include "CNSMenuBarManager.h"
#ifdef MOZ_MAIL_NEWS
#include "CThreadWindow.h"
#include "CMailProgressWindow.h"
#include "UMailFolderMenus.h"
#endif

#include "UTearOffPalette.h"
#include "CPaneEnabler.h"
#include "CApplicationEventAttachment.h"
#include "CCloseAllAttachment.h"

#include "CNSContextCallbacks.h"
#include "CBrowserContext.h"

#include "prgc.h"
#include "java.h"

#include "secrng.h"
#include "secnav.h"
#include "mkutils.h" // only for  FREEIF() macro.  JRM 96/10/17

#include "CEditorWindow.h"
#include "CEditView.h"
#include "meditdlg.h"

#ifdef MOZ_MAIL_NEWS
#include "MailNewsSearch.h"
#include "MailNewsFilters.h"
#include "MailNewsAddressBook.h"

#include "CCheckMailContext.h"
#endif // MOZ_MAIL_NEWS

#include "StBlockingDialogHandler.h"
#include "CSecureAttachment.h"
//#include "VTSMProxy.h"

#include "LTSMSupport.h"

#include "CMouseDispatcher.h"
#include "CSuspenderResumer.h"

#if defined(MOZ_MAIL_COMPOSE) || defined(MOZ_MAIL_NEWS)
#include "MailNewsClasses.h"
#include "msgcom.h"
#include "MailNewsgroupWindow_Defines.h"
#endif

#include "BrowserClasses.h"

//#include "CBrowserView.h" // need for CFrontApp::EventKeyUp() 1997-02-24 mjc
#include "CKeyUpReceiver.h" // need in CFrontApp::EventKeyUp()

#include "CToolbarModeManager.h"
#include "CMozillaToolbarPrefsProxy.h"
#include "CSharedPatternWorld.h"

#include "InternetConfig.h"
#include "il_strm.h"            /* Image Library stream converters. */
#ifdef MOCHA
//#include "libmocha.h"
#include "CMochaHacks.h"
#endif

PRThread* mozilla_thread;

PREventQueue *mozilla_event_queue = NULL;

#if defined (JAVA)
#include "MFramePeer.h"
#endif

#include "CLibMsgPeriodical.h"

	// Netscape
#ifndef GLHIST_H
#include "glhist.h"
#endif

#include "edtplug.h"

#include "m_cvstrm.h"
#include "prefapi.h"
#include "NSReg.h"
#ifdef MOZ_SMARTUPDATE
#include "softupdt.h"
#endif
#include <Balloons.h>

// HERE ONLY UNTIL NAV SERVICES CODE MERGED INTO TIP
Boolean SimpleOpenDlog ( short numTypes, const OSType typeList[], FSSpec* outFSSpec ) ;


CAutoPtr<CNSContext> CFrontApp::sRDFContext;



// Now we modify the frequency with which we call WaitNextEvent to try
// to minimize the time we spend in this expensive function. The strategy
// is two-fold:
//	1. 	If we are in the foreground, and there are no pending user
//		events (determined with OSEventAvail), then only call WNE every
//		kWNETicksInterval (4) ticks.
//		Note that OSEvent avail only knows about the Operating System
//		event queue, which contains these events:
//			mouseDown, mouseUp, keyDown, keyUp, autoKey, diskEvent
//
//		If we are in the background, we are nice to front apps and
//		call WNE every time.
//
//	2.	We also adjust the sleep time (the value passed to WNE to determine
//		how long it is before the Event Manager returns control to us).
//		This is done on EventSuspend and EventResume. We set the sleep time
//		to 0 in the foreground, and 4 in the background.

#define DAVIDM_SPEED2 	// Comment out to remove this WNE wrapper code
//======================================================================================
//									PROFILE
//======================================================================================

//#define PROFILE_LOCALLY 1
#if defined(PROFILE) || defined (PROFILE_LOCALLY)
#include <profiler.h>

		// Define this if you want to start profiling when the Caps Lock
		// key is pressed. Usually that's what you want: press Caps Lock,
		// start the command you want to profile, release Caps Lock when
		// the command is done. It works for all the major commands:
		// display a page, send a mail, save an attachment, etc...
#define PROFILE_ON_CAPSLOCK

		// Define this if you want to let the profiler run while you're
		// spending time in other apps. Usually you don't.
//#define PROFILE_WAITNEXTEVENT


		// Legacy stuff: profiles CFrontApp::UpdateMenus().
		// Don't use other profile methods at the same time.
//#define PROFILE_UPDATE_MENUS


//---------------------------------------------
// Paste these in the file you want to profile
// The code to be profiled must be in a project
// with "Emit profiler calls" turned on.
//
//	Example:
//		ProfileStart()
// 			...
//			ProfileSuspend()
//				...
//			ProfileResume()
//			...
//		ProfileStop()
//
// If you want to start the profile when Caps Lock
// is pressed, you can use:
//
//		if (IsThisKeyDown(0x39)) // caps lock
//			ProfileStart()
//
//---------------------------------------------
extern void ProfileStart();
extern void ProfileStop();
extern void	ProfileSuspend();
extern void ProfileResume();
extern Boolean ProfileInProgress();
//---------------------------------------------

static Boolean sProfileInProgress = false;
void ProfileStart()
{
	if (! sProfileInProgress)
	{
		sProfileInProgress = true;
		if (ProfilerInit(collectDetailed, microsecondsTimeBase, 2000, 100))
			return;
		ProfilerSetStatus(true);
	}
}

void ProfileStop()
{
	if (sProfileInProgress)
	{
		ProfilerDump("\pMozilla Profile");
		ProfilerTerm();
		sProfileInProgress = false;
	}
}

void ProfileSuspend()
{
	if (sProfileInProgress)
		ProfilerSetStatus(false);
}

void ProfileResume()
{
	if (sProfileInProgress)
		ProfilerSetStatus(true);
}

Boolean ProfileInProgress()
{
	return sProfileInProgress;
}

#endif // PROFILE
//======================================================================================

#ifdef FORTEZZA
#include "ssl.h"
#endif

#include <LArray.h>

#include "CPrefsDialog.h"
#include "UProcessUtils.h"		// for LaunchApplication

#ifdef MOZ_OFFLINE
#include "UOffline.h"
#endif

#define		LICENSE_REVISION			7

extern NET_StreamClass *IL_NewStream(int, void *, URL_Struct *, MWContext *);
extern void TrySetCursor( int whichCursor );

list<CommandT>	CFrontApp::sCommandsToUpdateBeforeSelectingMenu;

CFrontApp*		CFrontApp::sApplication = NULL;
CAppleEventHandler*		CAppleEventHandler::sAppleEventHandler = NULL;
short					CFrontApp::sHelpMenuOrigLength = 4;
short					CFrontApp::sHelpMenuItemCount = 0;
double			CFrontApp::sHRes = 1.0;
double			CFrontApp::sVRes = 1.0;
static PA_InitData		parser_initdata;

static void		InitDebugging();
static void		ConfirmWeWillRun();
static Boolean	NetscapeIsRunning(ProcessSerialNumber& psn);
static Boolean	LicenseHasExpired();
static void 	CheckForOtherNetscapes();
static void		Assert68020();
static void		AssertSystem7();

static const OSType kConferenceAppSig = 'Ncqπ';
static const OSType kCalendarAppSig = 'NScl';
static const OSType kImportAppSig = 'NSi2';
static const OSType kAOLInstantMessengerSig = 'Oscr';

#ifdef EDITOR
static void OpenEditURL(const char* url){
	if ( url != NULL ) {
		URL_Struct * request = NET_CreateURLStruct ( url, NET_NORMAL_RELOAD );
		if ( request )
			CEditorWindow::MakeEditWindow( NULL, request );
	}
}
#endif // EDITOR

//======================================
class LTimerCallback: public LPeriodical
// class that keeps track of xp timeout callbacks
//======================================
{
public:
				LTimerCallback( TimeoutCallbackFunction func, void* closure, uint32 msecs );
	void		SpendTime( const EventRecord& inMacEvent );

	static LArray*	sTimerList;

protected:
	UInt32		fFireTime;
	TimeoutCallbackFunction fFireFunc;
	void*		fClosure;
};

LArray* LTimerCallback::sTimerList = NULL;

//-----------------------------------
LTimerCallback::LTimerCallback( TimeoutCallbackFunction func, void* closure, uint32 msecs )
:	LPeriodical()
//-----------------------------------
{
	UInt32 ticks = ::TickCount();
	
	// • calculate the fire time in ticks from now
	fFireTime = ticks + ceil(((double)msecs / 100.0) * 6.0);
	fFireFunc = func;
	fClosure = closure;
	this->StartIdling();
}
	
//-----------------------------------
void LTimerCallback::SpendTime( const EventRecord& inMacEvent )
//-----------------------------------
{
	UInt32 ticks = inMacEvent.when;
	
	if ( ticks >= fFireTime )
	{
		// 97-06-18 pkc -- Remove this from sTimerList first because when we call
		// fFireFunc could possibly cause a call to FE_ClearTimeout on this
		// remove from timer list before deleting
		LTimerCallback::sTimerList->Remove(&this);
	
		// let's just say that the fFireFunc gives time to repeaters...
		// We don't want this one going off again before it is deleted!
		fFireTime = LONG_MAX;
		
		if ( fFireFunc )
			(*fFireFunc)( fClosure );
		delete this;
	}
}

//-----------------------------------
void* FE_SetTimeout( TimeoutCallbackFunction func, void* closure, uint32 msecs )
// this function should register a function that will
// be called after the specified interval of time has
// elapsed.  This function should return an id
// that can be passed to FE_ClearTimer to cancel
// the timer request.
//
// func:    The function to be invoked upon expiration of
//          the timer interval
// closure: Data to be passed as the only argument to "func"
// msecs:   The number of milli-seconds in the interval
//-----------------------------------
{	
	Try_
	{
		LTimerCallback* t = new LTimerCallback( func, closure, msecs );
		// insert timer callback into list
		try {
			if (!LTimerCallback::sTimerList)
				LTimerCallback::sTimerList = new LArray(sizeof(LTimerCallback*));
			LTimerCallback::sTimerList->InsertItemsAt(1, LArray::index_Last, &t);
		} catch (...) {
		}
		return t;
	}
	Catch_( inErr )
	{
		return NULL;
	}
	EndCatch_
}

//-----------------------------------
void FE_ClearTimeout( void* timer_id )
// This function cancels a timer that has previously been set.
// Callers should not pass in NULL or a timer_id that has already expired.
//-----------------------------------
{
	XP_ASSERT( timer_id );

	LTimerCallback* timer = (LTimerCallback*)timer_id;
	if (LTimerCallback::sTimerList)
	{
		if (LTimerCallback::sTimerList->FetchIndexOf(&timer) > LArray::index_Bad)
		{
			// this LTimerCallback is in timer list
			// remove from timer list before deleting
			LTimerCallback::sTimerList->Remove(&timer);
			delete timer;
		}
	}
}

#pragma mark -

void Assert68020() 
{
	long response = 0;
	OSErr err = ::Gestalt (gestaltQuickdrawVersion, &response);
	if (err || response == gestaltOriginalQD) {
		::Alert (14000, NULL);
		::ExitToShell ();
	}
}

void AssertSystem7() 
{
	long response = 0;
	OSErr err = ::Gestalt (gestaltSystemVersion, &response);
	if (err || response < 0x700) {
		::Alert (14001, NULL);
		::ExitToShell ();
	}
}

static void RequiredGutsNotFoundAlert()
{
	Str255 gutsFolderName;
	
	GetIndString(gutsFolderName, 14000, 1);
	ParamText(gutsFolderName, NULL, NULL, NULL);
	Alert (14002, NULL);
	ExitToShell();
}

static void AssertRequiredGuts()
{
	//	Initialize the "CPrefs::NetscapeFolder" and "CPrefs::RequiredGutsFolder"
	//	static variables early for use by the XP strings code in "errmgr.cp."
	
	short vRefNum;
	long dirID;
	
	if (HGetVol(NULL, &vRefNum, &dirID) != noErr)
		ExitToShell();
	
	FSSpec netscapeFolderSpec;
	
	if (CFileMgr::FolderSpecFromFolderID(vRefNum, dirID, netscapeFolderSpec) != noErr)
		RequiredGutsNotFoundAlert();
	
	try
	{
		CPrefs::Initialize();
	}
	catch (...)
	{
		RequiredGutsNotFoundAlert();
	}
	CPrefs::SetFolderSpec(netscapeFolderSpec, CPrefs::NetscapeFolder);
	
	FSSpec gutsFolderSpec;

		// Build a partial path to the guts folder starting from a folder we know (the Netscape folder)
	Str255 partialPath;

	{
			// Get the name of the guts folder
		Str255 gutsFolderName;
		GetIndString(gutsFolderName, 14000, 1);	

			// partialPath = ":" + netscapeFolderSpec.name + ":" + gutsFolderName;
			//	( this may _look_ cumbersome, but it's really the most space and time efficient way to catentate 4 pstrings )
		int dest=0;
		partialPath[++dest] = ':';
		for ( int src=0; src<netscapeFolderSpec.name[0]; )
			partialPath[++dest] = netscapeFolderSpec.name[++src];
		partialPath[++dest] = ':';
		for ( int src=0; src<gutsFolderName[0]; )
			partialPath[++dest] = gutsFolderName[++src];
		partialPath[0] = dest;
	}

		// Use the partial path to construct an FSSpec identifying the required guts folder
	FSMakeFSSpec(netscapeFolderSpec.vRefNum, netscapeFolderSpec.parID, partialPath, &gutsFolderSpec);

	{	// Ensure that the folder exists (even if pointed to by an alias) and actually _is_ a folder
		Boolean targetIsFolder, targetWasAliased;
		if ( (ResolveAliasFile(&gutsFolderSpec, true, &targetIsFolder, &targetWasAliased) != noErr)
			|| !targetIsFolder )
			RequiredGutsNotFoundAlert();
	}

		// ...and finally, publish the found spec where other routines in the app can find it
	CPrefs::SetFolderSpec(gutsFolderSpec, CPrefs::RequiredGutsFolder);
}

static void DisableJavaIfNotAvailable ()
{
	short vRefNum;
	long dirID;
	
	if (HGetVol(NULL, &vRefNum, &dirID) != noErr)
		ExitToShell();
	
	FSSpec netscapeFolderSpec;
	
	// This can't fail because we already called AssertRequiredGuts
	CFileMgr::FolderSpecFromFolderID(vRefNum, dirID, netscapeFolderSpec);
		
	FSSpec javaFolderSpec;

	// Build a partial path to the guts folder starting from a folder we know (the Netscape folder)
	Str255 partialPath;

	{
		// Get the name of the guts folder
		Str255 gutsFolderName;
		GetIndString(gutsFolderName, 14000, 1);	

		// jj confirmed it's OK to hard code the name of this folder
		static unsigned char *javaFolderName = "\pJava";

		// partialPath = ":" + netscapeFolderSpec.name + ":" + gutsFolderName + ":" + javaFolderName
		//	( this may _look_ cumbersome, but it's really the most space and time efficient way to catentate 4 pstrings )
		int dest=0;
		partialPath[++dest] = ':';
		for ( int src=0; src<netscapeFolderSpec.name[0]; )
			partialPath[++dest] = netscapeFolderSpec.name[++src];
		partialPath[++dest] = ':';
		for ( int src=0; src<gutsFolderName[0]; )
			partialPath[++dest] = gutsFolderName[++src];
		partialPath[++dest] = ':';
		for ( int src=0; src<javaFolderName[0]; )
			partialPath[++dest] = javaFolderName[++src];
		partialPath[0] = dest;
	}

	// Use the partial path to construct an FSSpec identifying the required guts folder
	OSErr err = FSMakeFSSpec(netscapeFolderSpec.vRefNum, netscapeFolderSpec.parID, partialPath, &javaFolderSpec);
	if (
		(FSMakeFSSpec(netscapeFolderSpec.vRefNum, netscapeFolderSpec.parID, partialPath, &javaFolderSpec) != noErr)
		||
		!CFileMgr::IsFolder(javaFolderSpec)
	)
	{
		static const char* inline_javascript_text ="lockPref(\"security.enable_java\", false)";
		PREF_EvaluateJSBuffer(inline_javascript_text, strlen(inline_javascript_text));
	}

}

static void DisplayErrorDialog ( OSErr err )
{
	if (::GetResource('ALRT', ALRT_ErrorOccurred) && ::GetResource('STR#', 7098))
	{
		short stringIndex;
		switch (err)
		{
			case userCanceledErr:
				return; // no alert
			case memFullErr:
				stringIndex = 1;  //"there was not enough memory."
				break;
			case dskFulErr:
				stringIndex = 3;
				break;
			case fnfErr:
				stringIndex = 4;
				break;
			case dirNFErr:
				stringIndex = 5;
				break;
			case nsvErr:
				stringIndex = 6;
				break;
			case bdNamErr:
				stringIndex = 7;
				break;
			case vLckdErr:
				stringIndex = 8;
				break;
			case fBsyErr:
				stringIndex = 9;
				break;
			case dupFNErr:
				stringIndex = 10;
				break;
			case opWrErr:
				stringIndex = 11;
				break;
			case permErr:
				stringIndex = 12;
				break;
			// ADD OTHER CASES HERE
			default:
				stringIndex = 2;  //"of an unknown error."
				break;
		}
		CStr255 p0, p1;
		::GetIndString(p0, 7098, stringIndex);
		::NumToString(err, p1);
		::ParamText(p0, p1, nil, nil);
		::CautionAlert(ALRT_ErrorOccurred, nil);
	}
	else
		throw;
}

static void DisplayExceptionCodeDialog ( ExceptionCode err )
{
	if (err == userCanceledErr) return;
	
	if (::GetResource('ALRT', ALRT_ErrorOccurred) && ::GetResource('STR#', 7098))
	{
		short stringIndex;
		switch (err)
		{
			case memFullErr:
				err = err_NilPointer; // & fall through...
			case err_NilPointer:
				stringIndex = 1;  //"there was not enough memory."
				break;
			default:
				stringIndex = 2;  //"of an unknown error."
				break;
		}
		CStr255 p0, p1;
		
		::NumToString(err, p1);     // The error is supposed to be numeric, not an OSType
									// Initializing it in the CStr255 constructor doesn't handle this properly

		::GetIndString(p0, 7098, stringIndex);
		::ParamText(p0, p1, nil, nil);
		::CautionAlert(ALRT_ErrorOccurred, nil);
	}
	else
		throw;
}

#ifdef MOZ_MAIL_NEWS
// TRUE if url *must* be loaded in this context type
Boolean URLRequiresContextType( const URL_Struct* url, MWContextType & type)
{
	if ( url )
	{
		if ( MSG_RequiresMailWindow( url->address ) )
			type = MWContextMail;
		else if ( MSG_RequiresNewsWindow( url->address ) )
			type = MWContextNews;
		else if (MSG_RequiresBrowserWindow(url->address))
			type = MWContextBrowser;
		else
			return FALSE;
		return TRUE;
	}
	return FALSE;
}
#endif // MOZ_MAIL_NEWS

extern "C" {
void InitializeASLM();
void CleanupASLM();
}

#pragma mark -

//======================================
class CResumeEventAttachment : public LAttachment
// This silly class fixes a weird bug where the app doesn’t
// get resume events after being sent a getURL AppleEvent.
// This attachment will run whenever we get an event, so
// we can check to see if the event is not a resume event,
// target is nil (meaning we haven’t received a resume 
// event yet), and we’re in the foreground (meaning we got
// switched in without getting the event).  If all those
// are true, we can resume ourselves so we won’t crash 
// because target is nil.
//======================================
{
	public:
					 		CResumeEventAttachment(CFrontApp* app) : LAttachment(msg_Event), fApp(app) {}
	protected:
		virtual void 		ExecuteSelf(MessageT inMessage, void* ioParam);
	private:
				CFrontApp*	fApp;
};

//-----------------------------------
void CResumeEventAttachment::ExecuteSelf(MessageT inMessage, void* ioParam)
//-----------------------------------
{
	EventRecord* event = (EventRecord*) ioParam;
	
	//
	// Do quick tests first to see if we should
	// go ahead and check if we’re in the foreground.
	//
	if (inMessage == msg_Event && event->what != osEvt && LCommander::GetTarget() == nil)
	{
		ProcessSerialNumber frontProcess;
		ProcessSerialNumber currentProcess;
		GetFrontProcess(&frontProcess);
		GetCurrentProcess(&currentProcess);
		Boolean result = false;
		SameProcess(&frontProcess, &currentProcess, &result);
		if (result)
		{
			// 
			// We’re in the foreground, so synthesize
			// a resume event and process it.
			//
			EventRecord resumeEvent;
			memset(&resumeEvent, 0, sizeof(EventRecord));
			resumeEvent.what = osEvt;
			resumeEvent.message = suspendResumeMessage << 24;
			resumeEvent.message |= resumeFlag;
			fApp->DispatchEvent(resumeEvent);
		}
	}
}

#pragma mark -

//======================================
class CSplashScreen : public LWindow
//======================================
{
	public:
		enum { class_ID = 'Spls' };
								
							CSplashScreen(LStream* inStream);
		virtual				~CSplashScreen();
		
		virtual void		SetDescriptor(ConstStringPtr inDescriptor);
		
	protected:
	
		virtual void		FinishCreateSelf(void);
	
		LCaption*			mStatusCaption;
		CResPicture*		mBackPicture;
		LFile*				mPictureResourceFile;
};

#pragma mark -

//======================================
// CLASS CFrontApp
//======================================

//-----------------------------------
CFrontApp::CFrontApp()
//-----------------------------------
:	fCurrentMbar(cBrowserMenubar)
,	fWantedMbar(fCurrentMbar)
,	fStartupAborted(false)
,	fProperStartup(false)
,	mSplashScreen(NULL)
#ifdef MOZ_MAIL_NEWS
,	mLibMsgPeriodical(NULL)
#endif // MOZ_MAIL_NEWS
,	mConferenceApplicationExists(false)
,	mJavaEnabled(false)
,	mHasBookmarksMenu(false)
,	mHasFrontierMenuSharing(false)
, mMouseRgnH( NewRgn() )
	// Performance
	// We really should be adjusting this dynamically
{
	// Set environment features
	
	CEnvironment::SetAllFeatures();

	// Static data members
	
	sApplication = this;
	
	// Add CApplicationEventAttachment
	
	AddAttachment(new CApplicationEventAttachment);
	
	// Add CCloseAllAttachment
	
	AddAttachment(new CCloseAllAttachment(CLOSE_WINDOW, CLOSE_ALL_WINDOWS));
	
	// Inherited data members
	mSleepTime = 1;

	// Indicate we don't want to be able to quit yet
	fSafeToQuit = false;
	fUserWantsToQuit = false;
	
	// Toolbox
	qd.randSeed  = TickCount();					// Toolbox

	AddAttachment(new LClipboard());
	AddAttachment(new CSecureAttachment);
	AddAttachment(new CResumeEventAttachment(this));	
	
#ifdef MOZ_MAIL_NEWS
	// create CLibMsgPeriodical
	mLibMsgPeriodical = new CLibMsgPeriodical;
	mLibMsgPeriodical->StartIdling();
#endif // MOZ_MAIL_NEWS
	
	//
	//	Initial inline support (TSM)
	//
	LTSMSupport::Initialize();

	UHTMLPrinting::InitCustomPageSetup();
	
#ifdef MOZ_SMARTUPDATE
    SU_Startup();
#endif
    NR_StartupRegistry();
    
	// • PowerPlant initialization
	UScreenPort::Initialize();

#if defined(MOZ_MAIL_NEWS) || defined(MOZ_MAIL_COMPOSE)
	RegisterAllMailNewsClasses();
#endif // MOZ_MAIL_NEWS
	RegisterAllBrowserClasses();

	ShowSplashScreen();
	
	InitUTearOffPalette(this);
	CTearOffManager::GetDefaultManager()->AddListener(this);

	// • network
#ifndef NSPR20
	CNetworkDriver::CreateNetworkDriver();
#endif

	TheEarlManager.StartRepeating();

//	WTSMManager::Initialize();
//	AddAttachment(new VTSMDoEvent(msg_Event));

	HyperStyle::InitHyperStyle();
	
	// • parser
	parser_initdata.output_func = LO_ProcessTag;

	// • parsers
	//		mime parsers
	NET_RegisterMIMEDecoders();

	//		external objects go to disk
	NET_RegisterContentTypeConverter ("*", FO_PRESENT, nil, NewFilePipe);
	NET_RegisterContentTypeConverter ("*", FO_SAVE_AS, nil, NewFilePipe);

	FSSpec spec;
	mImportModuleExists = CFileMgr::FindApplication(kImportAppSig, spec) == noErr;
	mAOLMessengerExists = CFileMgr::FindApplication(kAOLInstantMessengerSig, spec) == noErr;
	// Record whether the conference application exists. We do this once here
	// instead of lots 'o times in FindCommandStatus so searching for an app
	// doesn't slow us to a crawl (especially when searchin on network volumes).
	// One drawback of course, is that when the app is installed or removed,
	// the menu won't be enabled or disabled until the user quits and restarts
	// the app. This behavior is different than the pref panel for controlling
	// whether conference launches on startup or not (but that occurence of
	// FindApplication isn't called many times/sec either).
	mConferenceApplicationExists = (CFileMgr::FindApplication(kConferenceAppSig, spec) == noErr);
	mNetcasterContext = NULL;
}

//-----------------------------------
CFrontApp::~CFrontApp()
//-----------------------------------
{
	// Save the current visibility information
	if ( CPrefs::GetLong( CPrefs::StartupAsWhat ) == STARTUP_VISIBLE)
		{
		long winRecord = 0;
		
		CMediatedWindow* theWindow;
		CWindowIterator iter(WindowType_Any);
		while (iter.Next(theWindow))
			{
			switch(theWindow->GetWindowType())
				{
				case WindowType_MailNews:
					winRecord |= (MAIL_STARTUP_ID | NEWS_STARTUP_ID);
					break;
				case WindowType_Browser:
					winRecord |= BROWSER_STARTUP_ID;
					break;
				case WindowType_NavCenter:
					winRecord |= NAVCENTER_STARTUP_ID;
					break;
				case WindowType_Address:
					winRecord |= ADDRESS_STARTUP_ID;
					break;
				default:
					break;
				}
			}
			
		if (winRecord != 0)
			CPrefs::SetLong(winRecord, CPrefs::StartupBitfield);
		}


	{
		// This really comes from ~LCommander. This gets
		// executed after our destructor, and deletes all the windows
		// We need windows to be deleted before we are done,
		// because window destruction callbacks reference the app!
		// DO NOT DO THIS (IT CAUSES UNNECESSARY DEBUG BREAKS): SetTarget(this);
		
		// tj, 5/1/96
		//
		// We must close the windows explicitly, so that each window
		// can interrupt its context (XP_InterruptContext) while the 
		// window is still completely alive
		//
		
		//	Hmmmmm ... better be a hair more careful about traversing that window list.
		//	DDM 21-JUN-96
		
		WindowRef macWindow = LMGetWindowList();
		
		while (macWindow)
		{
			LWindow *ppWindow = LWindow::FetchWindowObject(macWindow);
			
			if (ppWindow)
			{
				ppWindow->DoClose();
				
				//	Start over at the beginning of the list.
				//	This is possibly slow but definitely safe, i.e. we're not using
				//	a block of memory that's been deleted.
				macWindow = LMGetWindowList();
			}
			else
				//	Skip to the next window in the list.
				macWindow = GetNextWindow(macWindow);
		}

		// Delete all SubCommanders
		LArrayIterator iterator(mSubCommanders);
		LCommander	*theSub;
		while (iterator.Next(&theSub))
			delete theSub;
	}
	
	HyperStyle::FinishHyperStyle();
	
	LTSMSupport::DoQuit( 0 );
	
//	WTSMManager::Quit();	
	
//	UpdateMenus();
	TrySetCursor( watchCursor );
	// XP_Saves
	GH_SaveGlobalHistory();
	GH_FreeGlobalHistory();
#ifdef MOZ_MAIL_NEWS
	NET_SaveNewsrcFileMappings();
	CAddressBookManager::CloseAddressBookManager();
	CCheckMailContext::Release(this);
#endif // MOZ_MAIL_NEWS

	//-----
	// We just closed all the windows but we now have to wait for the callbacks
	// in order to really delete the different contexts. That's because
	// CNSContext::NoMoreUsers() calls ET_RemoveWindowContext() which posts
	// an event which calls MochaDone() asynchronously and deletes the context..
	//
	// NOTE: CCheckMailContext::Release() also posts an event in order to delete
	// its context. So make sure that PR_ProcessPendingEvents() is always
	// executed after window->DoClose(), CCheckMailContext::Release() or any
	// other destructor function which deletes a context. A symptom of not
	// deleting the context is that newsgroups which have been unsubscribed
	// in the Message Center reappear when launching the application again.
	PR_ProcessPendingEvents(mozilla_event_queue);
	//-----

#ifndef NSPR20
	if (gNetDriver && !fStartupAborted)
	{
		extern const char*	CacheFilePrefix;
		NET_CleanupCacheDirectory( "", CacheFilePrefix );
		NET_ShutdownNetLib(); 
		delete gNetDriver;
		gNetDriver = NULL;
	}
#else
	if (!fStartupAborted)
	{
		extern const char*	CacheFilePrefix;
		NET_CleanupCacheDirectory( "", CacheFilePrefix );
		NET_ShutdownNetLib(); 
	}
#endif

	if (!fStartupAborted)
		CPrefs::DoWrite();
		
#ifdef MOZ_MAIL_NEWS
		delete mLibMsgPeriodical;
#endif // MOZ_MAIL_NEWS

	SetCursor( &qd.arrow );

	// remove anything we left in the Temporary Items:nscomm40 folder
	CFileMgr::DeleteCommTemporaryItems();
	
	NR_ShutdownRegistry();
#ifdef MOZ_SMARTUPDATE
   	SU_Shutdown();
#endif // MOZ_SMARTUPDATE

	ET_FinishMocha();

	LJ_ShutdownJava();

	DisposeRgn(mMouseRgnH);

	// Un-init RDF
	URDFUtilities::ShutdownRDF();

	sApplication = NULL;
} // CFrontApp::~CFrontApp()

//-----------------------------------
void CFrontApp::Initialize()
//-----------------------------------
{
	// Insert commands to update before seleting menu into list
	
	sCommandsToUpdateBeforeSelectingMenu.push_front(cmd_Redo);
	sCommandsToUpdateBeforeSelectingMenu.push_front(cmd_Close);
	sCommandsToUpdateBeforeSelectingMenu.push_front(cmd_Undo);
	sCommandsToUpdateBeforeSelectingMenu.push_front(cmd_Cut);
	sCommandsToUpdateBeforeSelectingMenu.push_front(cmd_Copy);
	sCommandsToUpdateBeforeSelectingMenu.push_front(cmd_Paste);
	sCommandsToUpdateBeforeSelectingMenu.push_front(cmd_Clear);
	sCommandsToUpdateBeforeSelectingMenu.push_front(cmd_SelectAll);

	new CNSContextCallbacks;
	LDocApplication::Initialize();
	
#ifdef FCAPI
	// initialization for spiral stuff.....
	FCInitialize();
#endif
	
	CMouseDispatcher* md = new CMouseDispatcher;
	AddAttachment(md);

	// 97-09-11 pchen -- setup prefs proxy for CToolbarModeManager so that knows
	// how to get Communicator toolbar style pref
	CMozillaToolbarPrefsProxy* prefsProxy = new CMozillaToolbarPrefsProxy();
	CToolbarModeManager::SetPrefsProxy(prefsProxy);

	// Register callback function when browser.chrome.toolbar_style
	// pref changes
	PREF_RegisterCallback("browser.chrome.toolbar_style",
						  CToolbarModeManager::BroadcastToolbarModeChange,
						  (void*)NULL);
	// 97-05-12 pkc -- setup sHRes and sVRes data members
	CGrafPtr wMgrPort;
	GetCWMgrPort(&wMgrPort);
	sHRes = (**wMgrPort->portPixMap).hRes / 0x00010000; // convert from Fixed to floating point
	sVRes = (**wMgrPort->portPixMap).vRes / 0x00010000; // convert from Fixed to floating point
	
	// 97-08-23 pchen -- register us as listener to LGrowZone
	LGrowZone::GetGrowZone()->AddListener(this);
	
} // CFrontApp::Initialize()
		
void CFrontApp::RegisterMimeType(CMimeMapper * mapper)
{
	CMimeMapper::LoadAction loadAction = mapper->GetLoadAction();
	CStr255 mimeName = mapper->GetMimeName();
	CStr255 pluginName = mapper->GetPluginName();
	void* pdesc = nil;
	if (pluginName.Length() > 0)
		pdesc = GetPluginDesc(pluginName);
		
	switch (loadAction)
	{
		case CMimeMapper::Internal:
			// Disable the previous plug-in, if any
			if (pdesc)
				NPL_EnablePluginType(mimeName, pdesc, false);
#ifdef OLD_IMAGE_LIB
			if (mimeName == IMAGE_GIF || mimeName == IMAGE_JPG || mimeName == IMAGE_XBM || mimeName == IMAGE_PNG)
				NET_RegisterContentTypeConverter(mimeName, FO_PRESENT, nil, IL_ViewStream);
#endif
#ifdef MOZ_MAIL_NEWS // BinHex decoder is in mail/news code, right?
			else if (mimeName == APPLICATION_BINHEX)
				NET_RegisterContentTypeConverter(APPLICATION_BINHEX, FO_PRESENT, nil, fe_MakeBinHexDecodeStream);
#endif // MOZ_MAIL_NEWS
			break;
		
		case CMimeMapper::Plugin:
			//
			// pdesc can be nil when reading prefs on startup -- that's OK,
			// the plug-in code in mplugin.cp will take care of registration.
			// bing: Alternatively, we could call NPL_EnablePluginType(true)
			// here and set up the new description and extensions below.
			//
			if (pdesc)
				NPL_RegisterPluginType(mimeName, mapper->GetExtensions(), mapper->GetDescription(), nil, pdesc, true);
			return;		// XP code will take care of the cdata
			
		default:
			// Disable the previous plug-in, if any
			if (pdesc)
				NPL_EnablePluginType(mimeName, pdesc, false);
			//
			// NewFilePipe will look at the mapper list and
			// decide whether it should launch an app or save.
			//
			NET_RegisterContentTypeConverter(mimeName, FO_PRESENT,  nil, NewFilePipe);
			break;
	}
		
	CStr255 extensions = mapper->GetExtensions();
	CStr255 mimetype = mapper->GetMimeName();
	NET_cdataCommit(mimetype, extensions);
	
	// Special case for View Source
	// montulli knows why is this
	if (mimetype == HTML_VIEWER_APPLICATION_MIME_TYPE) {
		if (loadAction == CMimeMapper::Internal) {
			NET_RegisterContentTypeConverter (TEXT_HTML, FO_VIEW_SOURCE, NULL, INTL_ConvCharCode);
 			NET_RegisterContentTypeConverter (INTERNAL_PARSER, FO_VIEW_SOURCE,TEXT_HTML, net_ColorHTMLStream);
			NET_RegisterContentTypeConverter ("*", FO_VIEW_SOURCE, TEXT_PLAIN, net_ColorHTMLStream);
			NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_VIEW_SOURCE, NULL, INTL_ConvCharCode);	// added by ftang. copy from libnet #ifdef UNIX
  			NET_RegisterContentTypeConverter (MESSAGE_NEWS, FO_VIEW_SOURCE, NULL, INTL_ConvCharCode);	// added by ftang. copy from libnet #ifdef UNIX
		}
		else		// Pipe to disk for launching
		{
			NET_RegisterContentTypeConverter (INTERNAL_PARSER, FO_VIEW_SOURCE, nil, NewFilePipe);
			NET_RegisterContentTypeConverter (TEXT_HTML, FO_VIEW_SOURCE, nil, NewFilePipe);
			NET_RegisterContentTypeConverter ("*", FO_VIEW_SOURCE, nil, NewFilePipe);
		}
	}
	
	// We need to add the description, too, which unfortunately requires
	// looking the cinfo up AGAIN and setting the desc field...
	char* description = (char*) mapper->GetDescription();
	XP_ASSERT(description);
	if (description)
	{
		NET_cdataStruct temp;
		NET_cdataStruct* cdata;
		char* mimetype = (char*) mimeName;
		
		XP_BZERO(&temp, sizeof(temp));
		temp.ci.type = mimetype;
		cdata = NET_cdataExist(&temp);
		XP_ASSERT(cdata);
		if (cdata)
			StrAllocCopy(cdata->ci.desc, description);
	}
}


// NetLib Internals!
extern char * XP_AppName;
extern char * XP_AppVersion;
extern char * XP_AppCodeName;
extern char * XP_AppLanguage;
extern char * XP_AppPlatform;

// Creates a path suitable for use by XP_FILE_OPEN, and xpHotlist file spec

char* GetBookmarksPath( FSSpec& spec, Boolean useDefault )
{
	if ( useDefault )
	{
		spec = CPrefs::GetFilePrototype( CPrefs::MainFolder );
		::GetIndString( spec.name, 300, bookmarkFile );
	}
	char* newPath = CFileMgr::GetURLFromFileSpec( spec );
	if ( newPath )
		::BlockMoveData( newPath + 7, newPath, strlen( newPath )-6 );
	// Hack, gets rid of the file:///
	return newPath;
}

void CFrontApp::InitBookmarks()
{
	CBookmarksAttachment *attachment = new CBookmarksAttachment;
	AddAttachment( attachment ); // Bookmark commands

	UpdateMenus();
}


// static
int CFrontApp::SetBooleanWithPref(const char *prefName, void *boolPtr)
{
	XP_Bool	currentValue = *(Boolean *)boolPtr;
	PREF_GetBoolPref(prefName, &currentValue);
	*(Boolean *)boolPtr = currentValue;
	return 0;
}

// This is called right after startup for us to do our initialization. If
// the user wants to open/print a document then this will be the file
// spec. Otherwise it's nil (normal startup)
void CFrontApp::ProperStartup( FSSpec* file, short fileType ) 
{
	LFile*			prefsFile = NULL;
	Boolean			gotPrefsFile = false;
	static Boolean	properStartup = FALSE;
	
	if ( fStartupAborted )
		return;
	
	if ( properStartup )
	{
		if ( fileType == FILE_TYPE_ODOC
		|| fileType == FILE_TYPE_GETURL
		|| fileType == FILE_TYPE_LDIF)
			DoOpenDoc( file, fileType );
		
		// Cannot open the Profile Manager while app is running
		if ( (fileType == FILE_TYPE_PROFILES ) 
			|| (fileType == STARTUP_TYPE_NETPROFILE) )
			ErrorManager::PlainAlert(ERROR_OPEN_PROFILE_MANAGER);

		return;
	}
		
	properStartup = TRUE;
	
	// Initialize all the modules,
	// Read generic prefs (which will LoadState all the modules)
	// Then start the modules and managers
	
	NET_InitFileFormatTypes(NULL, NULL);	// Needs to be initialized before prefs are read in.
	SECNAV_InitConfigObject();				// ditto

	CPrefs::PrefErr abortStartup = CPrefs::eOK;
	SplashProgress( GetPString(MAC_PROGRESS_PREFS));
	
	if ((fileType == FILE_TYPE_PREFS) && file) {
		gotPrefsFile = true;
		prefsFile = new LFile(*file);
	}

	// Once we get to this point it's safe to quit
	fSafeToQuit = true;

#ifdef DEBUG
	// hack to show the profile manager stuff when a modifier key is down
	KeyMap		theKeys;
	GetKeys(theKeys);
	
	if (theKeys[1] & 0x00008000)		// is the command key down
	{
		prefsFile = NULL;
		fileType = FILE_TYPE_PROFILES;
	}
#endif

	abortStartup = CPrefs::DoRead( prefsFile, fileType );
	
	if (( abortStartup == CPrefs::eAbort ) || fUserWantsToQuit)
	{
		fStartupAborted = true;
		DoQuit();
		return;
	}

	if ( fileType == FILE_TYPE_ASW && file )
		abortStartup = CPrefs::eRunAccountSetup;
		
	XP_Bool startupFlag;
	mHasFrontierMenuSharing = (!(PREF_GetBoolPref("browser.mac.no_menu_sharing", &startupFlag) == PREF_NOERROR && startupFlag));
	mHasBookmarksMenu = (!(PREF_GetBoolPref("browser.mac.no_bookmarks_menu", &startupFlag) == PREF_NOERROR && startupFlag));

#ifndef MOZ_MAIL_NEWS
#ifdef SERVER_ON_RESTART_HACK // not needed in Nova.
	// Do this after we read prefs:
	{
		// Check to see what mail.server_type is and what mail.server_type_on_restart is.
		// Must be careful because our auto admin tool may have created a "Big Brother"
		// preference file that locks mail.server_type. If this is the case we must
		// discard whatever mail.server_type_on_restart is. If the admin hasn't
		// locked this preference then we use mail.server_type_on_restart (this is what
		// gets set in the preference dialog).
		int32	mailServerType, mailServerTypeOnRestart;
		PREF_GetIntPref("mail.server_type" , &mailServerType);
		PREF_GetIntPref("mail.server_type_on_restart" , &mailServerTypeOnRestart);
		
		// The default value of the latter is -1, of the former, 0 (POP3).
		// Neither of these should NEVER be set to -1 after we run the first time.
		if (-1 == mailServerTypeOnRestart)
			mailServerTypeOnRestart = mailServerType;
		else
			mailServerType = mailServerTypeOnRestart;
		// Write the adjusted values back to the prefs database.
		if (!PREF_PrefIsLocked("mail.server_type"))
			PREF_SetIntPref("mail.server_type" , mailServerType);
		PREF_SetIntPref("mail.server_type_on_restart" , mailServerTypeOnRestart);
	}
#endif
#endif // MOZ_MAIL_NEWS	

	// Set the state of network.online to match offline.startup_mode
	Int32 startupNetworkState;
	enum {kOnlineState, kOfflineState, kAskUserForState};
	PREF_GetIntPref("offline.startup_mode" , &startupNetworkState);
	switch (startupNetworkState)
	{
		case kOnlineState:
			PREF_SetBoolPref("network.online" , true);
			break;
		
		case kOfflineState:
			PREF_SetBoolPref("network.online" , false);
			break;
		
		case kAskUserForState:
            		{
            			XP_Bool	onlineLastTime;
            			PREF_GetBoolPref("network.online" , &onlineLastTime);
            			XP_Bool locked = PREF_PrefIsLocked("offline.startup_mode");

						enum
						{
							eAskMeDialog = 12009,
							eWorkOnline,
							eWorkOffline,
							eSetPref
						};

            			StDialogHandler	handler(eAskMeDialog, nil);
						LWindow			*dialog = handler.GetDialog();
						LGARadioButton	*onRButton =
							(LGARadioButton *)dialog->FindPaneByID(eWorkOnline);
						XP_ASSERT(onRButton);
						LGARadioButton	*offRButton =
							(LGARadioButton *)dialog->FindPaneByID(eWorkOffline);
						XP_ASSERT(offRButton);
						if (onlineLastTime)
						{
							onRButton->SetValue(true);
						}
						else
						{
							offRButton->SetValue(true);
						}
						LGACheckbox *setPrefBox =
							(LGACheckbox *)dialog->FindPaneByID(eSetPref);
						XP_ASSERT(setPrefBox);
						if (locked)
						{
							setPrefBox->Disable();
						}
						// Run the dialog
						MessageT message = msg_Nothing;
						do
						{
							message = handler.DoDialog();
						} while (msg_OK != message);
						XP_Bool	goOnline = onRButton->GetValue();
						if (setPrefBox->GetValue())
						{
							int32	newPrefValue =	goOnline ?
													kOnlineState:
													kOfflineState;
							PREF_SetIntPref("offline.startup_mode", newPrefValue);
						}
						if (goOnline)
						{
		                    PREF_SetBoolPref("network.online" , true);
						}
						else
						{
             		       PREF_SetBoolPref("network.online" , false);
						}
            		}
			break;
	}
	
	SetBooleanWithPref("security.enable_java", &mJavaEnabled);
	PREF_RegisterCallback("security.enable_java",
						  SetBooleanWithPref,
						  (void*)&mJavaEnabled);

	// Set CSharedPatternWorld::sUseUtilityPattern from pref
	XP_Bool	useUtilityPattern;
	int	prefResult = PREF_GetBoolPref("browser.mac.use_utility_pattern", &useUtilityPattern);
	if (prefResult == PREF_NOERROR && useUtilityPattern)
	{
		// Just for grins, check to see if utility pattern exists
		PixPatHandle thePattern = NULL;
		thePattern = ::GetPixPat(cUtilityPatternResID);
		if (thePattern)
		{
			CSharedPatternWorld::sUseUtilityPattern = true;
			::DisposePixPat(thePattern);
		}
	}

	
	// NETLIB wait until we've read prefs to determine what to set up
	CStr255		tmp;
	
	//	It is a good idea to allocate enough space for strings.  
	//	Before today, we were not doing so.  This was evil...
	//	So, XP_ALLOC now takes tmp.Length() + 1 as an argument
	//	instead of XP_ALLOC.
	//	dkc	1/25/96
	
	::GetIndString( tmp, ID_STRINGS, APPNAME_STRING_INDEX );
	XP_AppName = (char*)XP_ALLOC( tmp.Length() + 1);
	sprintf( XP_AppName, tmp );
	
	::GetIndString( tmp, ID_STRINGS, APPLANGUAGE_STRING_INDEX );
	XP_AppLanguage = (char*)XP_ALLOC( tmp.Length() + 1);
	sprintf( XP_AppLanguage, tmp );
	
	::GetIndString( tmp, ID_STRINGS, APPVERSION_STRING_INDEX );
	// Admin Kit: support optional user-agent suffix
	{
		char* suffix;
		if ( PREF_CopyConfigString("user_agent", &suffix) == PREF_NOERROR )
		{
			tmp = tmp + "C-" + suffix;
			XP_FREE(suffix);
		}
	}
	
	CStr255		tmp2;
#ifdef powerc
	::GetIndString( tmp2, ID_STRINGS, USERAGENT_PPC_STRING_INDEX );
#else
	::GetIndString( tmp2, ID_STRINGS, USERAGENT_68K_STRING_INDEX );
#endif
	tmp = tmp + " ";
	tmp = tmp + tmp2;
	
	char*		securityText = "X";
	
	XP_AppVersion = (char*)XP_ALLOC( tmp.Length() + strlen( securityText )  + 1);
	sprintf( XP_AppVersion, tmp, securityText );
	
	::GetIndString( tmp2, ID_STRINGS, APPCODENAME_STRING_INDEX );
	XP_AppCodeName = (char*)XP_ALLOC( tmp2.Length()  + 1);
	sprintf( XP_AppCodeName, tmp2 );

#ifdef powerc
    XP_AppPlatform = "MacPPC";
#else
    XP_AppPlatform = "Mac68k";
#endif
	
	SplashProgress( GetPString(MAC_PROGRESS_NET) );
	
	NET_InitNetLib (CPrefs::GetLong( CPrefs::BufferSize ), CPrefs::GetLong( CPrefs::Connections ) );

	SECNAV_EarlyInit();
	NET_ChangeMaxNumberOfConnections(50);
	RNG_SystemInfoForRNG();

	// 1998.01.12 pchen and 1998.02.23 rjc
	// RDF now uses NetLib.  Netlib initializes history.  RDF reimplements
	// history.  Looks like History initialization has been changed to work
	// correctly. Also, GH_InitGlobalHistory() can be safely called twice. 
	URDFUtilities::StartupRDF(); 

	// Load the .jsc startup file, if necessary (autoadmin added 2/11/98 arshad)
//••• REVISIT NOVA MERGE •••	NET_DownloadAutoAdminCfgFile();	
	
	// Install Help and Directory menus
	InstallMenus();  //  arshad - taken from line 1317, moved here for autoadmin to update menus (re: bug 88421)
	
	// 1998-03-17 pchen -- Moved this code here to fix Communicator menu
	// Polaris: remove Calendar/3270 items if they're not installed
	//	(?? maybe these are always available for Polaris (autoadmin lib)
	//	so users get helpful error messages if they try them..?)
	if (CWindowMenu::sWindowMenu)
	{
		FSSpec fspec;
			
		if ( CFileMgr::FindApplication(kCalendarAppSig, fspec) != noErr )
			(CWindowMenu::sWindowMenu)->RemoveCommand(cmd_LaunchCalendar);

		if ( CFileMgr::FindApplication(kAOLInstantMessengerSig, fspec) != noErr )
			(CWindowMenu::sWindowMenu)->RemoveCommand(cmd_LaunchAOLInstantMessenger);

		if ( ! Find3270Applet(fspec) )
			(CWindowMenu::sWindowMenu)->RemoveCommand(cmd_Launch3270);

		if ( ! FE_IsNetcasterInstalled() )
			(CWindowMenu::sWindowMenu)->RemoveCommand(cmd_LaunchNetcaster);
	}

	SECNAV_Init();
	SECNAV_RunInitialSecConfig();		// call after SECNAV_Init()
	
	// Must not call SECNAV_SecurityVersion before SECNAV_Init().
	securityText = SECNAV_SecurityVersion( PR_FALSE );
	XP_FREE(XP_AppVersion);
	XP_AppVersion = (char*)XP_ALLOC( tmp.Length() + strlen( securityText )  + 1);
	sprintf( XP_AppVersion, tmp, securityText );
	
#ifdef MOZ_MAIL_NEWS
	NET_ReadNewsrcFileMappings();
#endif

	SplashProgress(  GetPString(MAC_PROGRESS_BOOKMARK));
	InitBookmarks();
	
	//	Create the NSPR event queue
	
	mozilla_event_queue = PR_CreateEventQueue("Mozilla Event Queue", mozilla_thread);

	LM_InitMocha();		// mocha, mocha, mocha
	NPL_Init();			// plugins

	// The tools menus must be created after the plugins are read in.
	
#ifdef EDITOR
	CToolsAttachment *attachment = new CToolsAttachment;
	if ( attachment )
		AddAttachment( attachment ); // Tools menu
	CFontMenuAttachment *fontattachment = new CFontMenuAttachment;
	if ( fontattachment )
		AddAttachment( fontattachment ); // Font menu
	CRecentEditMenuAttachment *recentattachment = new CRecentEditMenuAttachment;
	if ( recentattachment )
		AddAttachment( recentattachment );
#endif // EDITOR
	UpdateMenus();

#if defined (JAVA)
	//	Initialize the java runtime, start the MToolkit thread running
	//	to keep the network alive.  For now, we have to temporarily
	//	enable java to make sure that the runtime bootstraps itself
	//	minimally.
	//	dkc 3-18-96
	
	this->SplashProgress(  GetPString(MAC_PROGRESS_JAVAINIT));
	
	LJ_SetJavaEnabled((PRBool)CPrefs::GetBoolean(CPrefs::EnableJava));
#endif /* defined (JAVA) */

#if defined(EDITOR) && defined(MOZ_JAVA)
    EDTPLUG_RegisterEditURLCallback(&OpenEditURL);
#endif // EDITOR && MOZ_JAVA

	/*	Okay, this is hacky, but necessary.  It is extremely
		important that the Macintosh reclaims memory.  So,
		we want the finalizer to be at as high a priority as
		possible. */	

	if ((PR_GetGCInfo() != NULL) && (PR_GetGCInfo()->finalizer != NULL))
#ifndef NSPR20
		PR_SetThreadPriority(PR_GetGCInfo()->finalizer, 31);
#else
		PR_SetThreadPriority(PR_GetGCInfo()->finalizer, PR_PRIORITY_URGENT);
#endif

	DestroySplashScreen();
	if ( abortStartup == CPrefs::eRunAccountSetup )
		file = NULL;
		
	Boolean agreedToLicense = AgreedToLicense( file, fileType );
	
	if ( abortStartup == CPrefs::eRunAccountSetup )
		LaunchAccountSetup();

#ifdef MOZ_MAIL_NEWS
	// This is for Biff notification.
	// It should come after menus and after prefs are read in and probably
	// after the online/offline determination is made.
	CCheckMailContext::Initialize(this); // we are the owner/user
#endif // MOZ_MAIL_NEWS
#ifdef MOZ_LOC_INDEP
	InitializeLocationIndependence();
#endif
#ifdef MOZ_MAIL_NEWS
	SplashProgress( GetPString(MAC_PROGRESS_ADDRESS) );
	CAddressBookManager::OpenAddressBookManager();
#endif // MOZ_MAIL_NEWS
//	NET_FinishInitNetLib();
	if (agreedToLicense)
		CreateStartupEnvironment(! gotPrefsFile);
	
	fProperStartup = true;		
}	// CFrontApp::ProperStartup


Boolean CFrontApp::AgreedToLicense( FSSpec* fileSpec, short fileType )
{
	// ML License clicker moved to installer so just open
	DoOpenDoc( fileSpec, fileType );
		
	return true;
}
		
	
	
// Called when application starts up without any documents
// Reads in the default preferences doc, and then creates a default browser window
void CFrontApp::StartUp()
{
	ProperStartup( NULL, FILE_TYPE_PREFS );
//	CMailNewsFolderWindow::FindAndShow(true);
}


pascal void colorPopupMDEFProc(short message, MenuHandle theMenu, 
		Rect *menuRect, Point hitPt, short *whichItem);

// openStartupWindows is only false if we started comm from a desktop icon
// and don't want the prefs-specified components to open up
void CFrontApp::CreateStartupEnvironment(Boolean openStartupWindows)
{
#ifdef EDITOR
	// Composer's color picker MDEF
	Handle		stubMDEFH;
	MenuDefUPP	colorMdefUPP;

	// load/lock (but DON'T detach) MDEF stub and set it to call appropriate routine
	if ( (stubMDEFH = Get1Resource( 'MDEF', 20000 )) != NULL)
	{
		HNoPurge((Handle)stubMDEFH);
		HUnlock((Handle)stubMDEFH);
		MoveHHi((Handle)stubMDEFH);
		HLock((Handle)stubMDEFH);
		if ( (colorMdefUPP = NewMenuDefProc(colorPopupMDEFProc)) != NULL)
		{
			*(ProcPtr *)(*stubMDEFH + 6L) = (ProcPtr)colorMdefUPP;
		}
	}
#endif

	XP_Bool startupFlag;

	// handle new startup preferences
	CommandT command = cmd_Nothing;
	// check to see if we need to bring up a new browser window at startup
	if (PREF_GetBoolPref("general.startup.browser", &startupFlag) == PREF_NOERROR)
	{
		if (startupFlag)
		{
			// Only create a new browser window if the startupFlag indicates
			// that we should and there are no delayed URLs (which could happen
			// if a 'GURL' or 'OURL' event is handled while the profile dialog
			// or license dialog is waiting to be dismissed) and there are no
			// open browser windows (which could happen if an 'odoc' event
			// was processed prior to CreateStartupEnvironment being called).
			// These guards will ensure that we don't open any unnecessary blank
			// browser windows at startup.
			
			CMediatedWindow* window = CWindowMediator::GetWindowMediator()->FetchTopWindow(
																WindowType_Browser,
																regularLayerType,
																true);
			UInt32 howMany = CURLDispatcher::CountDelayedURLs();
			if (openStartupWindows && !window && !CURLDispatcher::CountDelayedURLs())
				MakeNewDocument();
		}
	}
	if (PREF_GetBoolPref("general.startup.netcaster", &startupFlag) == PREF_NOERROR)
	{
		if (openStartupWindows && startupFlag)
			ObeyCommand(cmd_LaunchNetcaster, NULL);
	}
	
	startupFlag = false;
	if (HasFrontierMenuSharing())
	{
		AddAttachment(new LMenuSharingAttachment( msg_AnyMessage, TRUE, MENU_SHARING_FIRST ) ); // Menu Sharing
	}
	
#ifdef MOZ_MAIL_NEWS
	// check to see if we need to bring up mail window at startup
	if (PREF_GetBoolPref("general.startup.mail", &startupFlag) == PREF_NOERROR)
	{
		if (openStartupWindows && startupFlag)
			ObeyCommand(cmd_Inbox, NULL);
	}
	// check to see if we need to bring up news window at startup
	if (PREF_GetBoolPref("general.startup.news", &startupFlag) == PREF_NOERROR)
	{
		if (openStartupWindows && startupFlag)
			ObeyCommand(cmd_NewsGroups, NULL);
	}
#endif // MOZ_MAIL_NEWS
#ifdef EDITOR
	// check to see if we need to bring up an editor window at startup
	if (PREF_GetBoolPref("general.startup.editor", &startupFlag) == PREF_NOERROR)
	{
		if (openStartupWindows && startupFlag)
			ObeyCommand(cmd_NewWindowEditor, NULL);
	}
#endif
#if !defined(MOZ_LITE) && !defined(MOZ_MEDIUM)
	// check to see if we need to launch conference at startup
	if (PREF_GetBoolPref("general.startup.conference", &startupFlag) == PREF_NOERROR)
	{
		if (openStartupWindows && startupFlag)
			ObeyCommand(cmd_LaunchConference, NULL);
	}
	// check to see if we need to launch calendar at startup
	if (PREF_GetBoolPref("general.startup.calendar", &startupFlag) == PREF_NOERROR)
	{
		if (openStartupWindows && startupFlag)
			ObeyCommand(cmd_LaunchCalendar, NULL);
	}

	// check to see if we need to launch AOL Instant Messenger at startup
	if (PREF_GetBoolPref("general.startup.AIM", &startupFlag) == PREF_NOERROR)
	{
		if (openStartupWindows && startupFlag)
			ObeyCommand(cmd_LaunchAOLInstantMessenger, NULL);
	}
#endif // !MOZ_LITE && !MOZ_MEDIUM
#ifdef MOZ_TASKBAR
	if (PREF_GetBoolPref("taskbar.mac.is_open", &startupFlag) == PREF_NOERROR)
	{
		if (openStartupWindows && startupFlag)
			ObeyCommand(cmd_ToggleTaskBar, NULL);
	}
#endif // MOZ_TASKBAR
}

// OpenDocument asserts that there is a preference document read in,
// because we cannot do any document operations before preferences are
// initialized. Pref documents are read automatically in assertion
// Hyper document is read in here
void CFrontApp::OpenDocument( FSSpec* inFileSpec )
{
	FInfo		fndrInfo;
	short		fileType;
	
	OSErr err = ::FSpGetFInfo( inFileSpec, &fndrInfo );
	//what to do on error?
	
	// • how silly is this?
	if ( emPrefsType == fndrInfo.fdType )
		fileType = FILE_TYPE_PREFS;
	else if ( emProfileType == fndrInfo.fdType )
		fileType = FILE_TYPE_PROFILES;
	else if ( 'ASWl' == fndrInfo.fdType )
		fileType = FILE_TYPE_ASW;
	else if ( emLDIFType == fndrInfo.fdType)
		fileType = FILE_TYPE_LDIF;
	else if (emNetprofileType == fndrInfo.fdType)
		fileType = STARTUP_TYPE_NETPROFILE;
	else
		fileType = FILE_TYPE_ODOC;
		
	ProperStartup( inFileSpec, fileType );
}

void CFrontApp::DoOpenDoc( FSSpec* inFileSpec, short fileType )
{
	switch ( fileType )
	{
		case FILE_TYPE_ODOC:
			if ( inFileSpec )
				OpenLocalURL(inFileSpec );
			break;
		case FILE_TYPE_LDIF:
#ifdef MOZ_MAIL_NEWS
			if (inFileSpec)
				CAddressBookManager::ImportLDIF(*inFileSpec);
			break;
#endif // MOZ_MAIL_NEWS
		case FILE_TYPE_PREFS:
		case FILE_TYPE_PROFILES:
			break;
		case FILE_TYPE_GETURL:
		case FILE_TYPE_NONE:
			break;
	}
}

// Opens up a bookmark file
void CFrontApp::OpenBookmarksFile( FSSpec* inFileSpec, CBrowserWindow * /*win*/, Boolean /*delayed*/)
{
	Try_
	{
		vector<char> url(500);
		OSErr err = ReadBookmarksFile(url, *inFileSpec);
		ThrowIfOSErr_(err);
		
		URL_Struct * request = NET_CreateURLStruct( url.begin(), NET_DONT_RELOAD );
		CURLDispatcher::DispatchURL(request, NULL);	
	}
	Catch_(inErr)
	{}
	EndCatch_
}


// Opens a local file. If it is a bookmark, load the URL stored in it
void CFrontApp::OpenLocalURL( FSSpec* inFileSpec , 
							CBrowserWindow * win, 
							char * mime_type, 
							Boolean delayed)
{
	if (inFileSpec == NULL)
		return;
	Boolean dummy, dummy2;
	
// Resolve the aliases
	OSErr err = ::ResolveAliasFile(inFileSpec,TRUE,&dummy,&dummy2);
	
// If we are a bookmark file, open a bookmark
	FInfo info;
	err = ::FSpGetFInfo (inFileSpec, &info);
	if ((info.fdType == emBookmarkFile) && (mime_type == NULL))
	{
		OpenBookmarksFile( inFileSpec, win, delayed);
		return;
	}

// If we are a help file, open a help document
	if ((info.fdType == emHelpType)  && (mime_type == NULL))
	{
		char * help_id = NULL;
		char * map_file_url = CFileMgr::GetURLFromFileSpec(*inFileSpec);

		DebugStr("\pNot implemented");
//•••		NET_GetHTMLHelpFileFromMapFile(*CBookmarksContext::GetInstance(),	// Will this work?
//•••							  map_file_url, help_id, NULL);
		FREEIF( help_id );
		FREEIF( map_file_url );
		return;
	}
	

#ifdef MOZ_MAIL_NEWS
// If we are mailbox file at the root of the local mail hierarchy, we can assume that it's an
// imported mailbox created by the import module. Then just update the folder tree (the new mailbox
// will appear in the folder hierarchy).
	FSSpec mailFolderSpec = CPrefs::GetFilePrototype(CPrefs::MailFolder);
	if (mailFolderSpec.vRefNum == inFileSpec->vRefNum
	&& mailFolderSpec.parID == inFileSpec->parID)
	{
		Assert_(false);
		//MSG_UpdateFolderTree(); not checked in yet by phil.
		return;
	}
#endif //MOZ_MAIL_NEWS
	
// Open it by loading the URL into specified window

	char*		localURL;
	if (win == NULL)
		win = CBrowserWindow::FindAndPrepareEmpty(true);
	if (win != NULL)
	{
		localURL = CFileMgr::GetURLFromFileSpec(*inFileSpec);
		URL_Struct * request = NET_CreateURLStruct (localURL, NET_DONT_RELOAD);
		if (localURL)
			XP_FREE(localURL);
		ThrowIfNil_(request);
		request->content_type = mime_type;
//		if (delayed)
//			win->StartLatentLoading(request);
//		else
//			win->StartLoadingURL (request, FO_CACHE_AND_PRESENT);
		// FIX ME!!! Hook up latent loading!!!
		CBrowserContext* theContext = (CBrowserContext*)win->GetWindowContext();
		theContext->SwitchLoadURL(request, FO_CACHE_AND_PRESENT);
	}
}
	
void CFrontApp::PrintDocument(FSSpec* inFileSpec)
{
	ProperStartup( inFileSpec, FILE_TYPE_ODOC );
	PrintDocument( inFileSpec );
}

// ---------------------------------------------------------------------------
//		• ClickMenuBar
// ---------------------------------------------------------------------------
//	Respond to a mouse click in the menu bar.
//
//	Also, we do a targeted update of menus before calling MenuSelect

void
CFrontApp::ClickMenuBar(
	const EventRecord&	inMacEvent)
{
	StUnhiliteMenu	unhiliter;			// Destructor unhilites menu title
	Int32			menuChoice;

	CTargetedUpdateMenuRegistry::SetCommands(sCommandsToUpdateBeforeSelectingMenu);
	CTargetedUpdateMenuRegistry::UpdateMenus();

	CommandT		menuCmd = LMenuBar::GetCurrentMenuBar()->
								MenuCommandSelection(inMacEvent, menuChoice);
	
	if (menuCmd != cmd_Nothing) {
		SignalIf_(LCommander::GetTarget() == nil);
		LCommander::SetUpdateCommandStatus(true);
		LCommander::GetTarget()->ProcessCommand(menuCmd, &menuChoice);
	}
}

// ---------------------------------------------------------------------------
//		• UpdateMenus
// ---------------------------------------------------------------------------
//	Update the status of all menu items
//
//	General Strategy:
//	(1) Designate every Menu as "unused".
//	(2) Iterate through all menu commands, asking the Target chain for
//		the status of each one. Enable/Disable, mark, and adjust the
//		name of each menu accordingly. Enabling any item in a Menu
//		designates that Menu as "used".
//	(3)	Iterate through each menu, asking for the status of a special
//		synthetic command that indicates the entire menu. At this time,
//		Commanders can perform operations that affect the menu as a whole.
//		Menus that are desinated "used" are enabled; "unused" ones
//		are disabled.
//
//	Changes from LEventDispatcher::UpdateMenus():
//
//		- We maintain a list of commands to update and a bool to indicate
//		  whether to update just those commands or all commands. This is 
//		  useful for targeting specific commands to update without having
//		  to pay the price for updating hundreds of commands. The registry
//		  of commands is maintained separately from CFrontApp. The general
//		  pattern is that the client which calls UpdateMenus will stuff
//		  it's commands in the registry and then call UpdateMenus.
//
//		  One limitation of this targeted updating is that it is possible
//		  that a menu which should be disabled because there are no enabled
//		  items might still be enabled. However, given that the typical
//
//		- We call UpdateMenusSelf() to get some more work done.
//
//		- We cache some stuff during UpdateMenus

void
CFrontApp::UpdateMenus()
{
#ifdef PROFILE_UPDATE_MENUS
	StProfileSection profileThis("\pprofile", 1000, 100);
#endif

	// UDesktop::StUseFrontWindowIsModalCache theFrontWindowIsModalCache;

#ifdef EDITOR
	CAutoPtr<CEditView::StUseCharFormattingCache> stCharCache;

	CEditView *editViewCommander = dynamic_cast<CEditView *>(LCommander::GetTarget());
	if ( editViewCommander )
		stCharCache.reset(new CEditView::StUseCharFormattingCache(*editViewCommander));
#endif

	LMenuBar	*theMenuBar = LMenuBar::GetCurrentMenuBar();
	Int16		menuItem;
	MenuHandle	macMenuH = nil;
	LMenu		*theMenu;
	CommandT	theCommand;
	Boolean		isEnabled;
	Boolean		usesMark;
	Char16		mark;
	Str255		itemName;	
	LCommander	*theTarget = LCommander::GetTarget();
	
	theMenu = nil;					// Designate each menu as "unused"
	while (theMenuBar->FindNextMenu(theMenu)) {
		theMenu->SetUsed(false);
	}
		
									// Loop thru each menu item that has an
									//   associated command
	while (theMenuBar->FindNextCommand(menuItem, macMenuH,
										theMenu, theCommand)) {
										
									// Don't change menu item state for
									//   special commands (all negative
									//   values except cmd_UseMenuItem)
									//   or for commands which are not
									//   in the registry when we are
									//   supposed to only update commands
									//   which are in the registry.
		if ((theCommand > 0 || theCommand == cmd_UseMenuItem) &&
			(!CTargetedUpdateMenuRegistry::UseRegistryToUpdateMenus() ||
			  CTargetedUpdateMenuRegistry::CommandInRegistry(theCommand)))
		{
		
									// For commands that depend on the menu
									//   item, get synthetic command number
			if (theCommand == cmd_UseMenuItem) {
				theCommand = theMenu->SyntheticCommandFromIndex(menuItem);
			}

									// Ask Target if command is enabled,
									//   if the menu item should be marked,
									//   and if the name should be changed
			isEnabled = false;
			usesMark = false;
			itemName[0] = 0;
			
			if (theTarget != nil) {
				theTarget->ProcessCommandStatus(theCommand, isEnabled,
										usesMark, mark, itemName);
			}
			
				// Adjust the state of each menu item as needed.
				// Also designate as "used" the Menu containing an
				// enabled item.
			
			if (isEnabled) {
				::EnableItem(macMenuH, menuItem);
				theMenu->SetUsed(true);
			} else {
				::DisableItem(macMenuH, menuItem);
			}
			
			if (usesMark) {
				::SetItemMark(macMenuH, menuItem, mark);
			}
			
			if (itemName[0] > 0) {
				::SetMenuItemText(macMenuH, menuItem, itemName);
			}
			
		} else if (theCommand < 0 || CTargetedUpdateMenuRegistry::UseRegistryToUpdateMenus()) {
									// Don't change state of items with
									//   negative command numbers or
									//   when the registry is in use.
			if (theMenu->ItemIsEnabled(menuItem)) {
				theMenu->SetUsed(true);
			}
			
		} else {					// Item has command number 0
			::DisableItem(macMenuH, menuItem);
		}
			
	}
	
	if (theTarget != nil) {
									// Loop thru each menu
		theMenu = nil;
		while (theMenuBar->FindNextMenu(theMenu)) {
		
				// The "command" for an entire Menu is the synthetic command
				// number for item zero (negative number that has the Menu ID
				// in the high word and 0 in the low word.

			theCommand = theMenu->SyntheticCommandFromIndex(0);
			
				// The Target chain now has the opportunity to do something
				// to the Menu as a whole, as a call for this special
				// synthetic command is made only once for each menu.
				// For example, a text handling Commander could put a check
				// mark next to the current font in a Fonts menu.
				//
				// The isEnabled parameter [outEnabled as an argument to
				// FindCommandStatus()] should be set to true to enable
				// a menu that does not contain commands that have
				// already been explicitly enabled. For example, a Fonts
				// menu typically has no associated commands (the menu
				// items all use synthetic commands), so a Commander
				// must set outEnabled to true when asked the status
				// of the synthetic command corresponding the Fonts menu.
				//
				// The mark information is ignored, as is the itemName.
				// It would be possible to use the itemName to facilitate
				// dynamic changing of menu titles. However, this is bad
				// interface design which we don't want to encourage.
			
			isEnabled = false;

			theTarget->ProcessCommandStatus(theCommand, isEnabled,
											usesMark, mark, itemName);
											
				// Menu is "used" if it contains commands that were
				// explicity enabled in the loop above, or if this
				// last call passes back "true" for isEnabled.
			
			if (isEnabled) {
				theMenu->SetUsed(true);
			}
			
				// Enable all "used" Menus and disable "unused" ones
				// To avoid unnecessary redraws, we invalidate the menu
				// bar only if a Menu's enableFlags changes (which means
				// that its state has changed).
			
			macMenuH = theMenu->GetMacMenuH();
			Uint32 originalFlags = (**macMenuH).enableFlags;
									
			if (theMenu->IsUsed()) {
				::EnableItem(macMenuH, 0);
			} else {
				if (!CTargetedUpdateMenuRegistry::UseRegistryToUpdateMenus())
				{
					// When we are using the registry, it is OK to enable
					// menus that are now used, but it is inappropriate
					// to disable menus, since we did not update the command
					// status of all menu items and therefore we do not have
					// enough information to disable the whole menu,
					
					::DisableItem(macMenuH, 0);
				}
			}
			
			if (originalFlags != (**macMenuH).enableFlags) {
				::InvalMenuBar();
			}
		}
	}
	
	UpdateMenusSelf();
}

void CFrontApp::UpdateMenusSelf()
{
	if (CWindowMenu::sWindowMenu)
		(CWindowMenu::sWindowMenu)->Update();
	if (CHistoryMenu::sHistoryMenu)
		(CHistoryMenu::sHistoryMenu)->Update();

	CBookmarksAttachment::UpdateMenu();

#ifdef EDITOR
	CToolsAttachment::UpdateMenu();
	CRecentEditMenuAttachment::UpdateMenu();
#endif // EDITOR
	
	UpdateHierarchicalMenus();
	
	CPaneEnabler::UpdatePanes();
#ifdef MOZ_MAIL_NEWS
	CMailFolderMixin::UpdateMailFolderMixinsNow();
#endif // MOZ_MAIL_NEWS

}

void CFrontApp::UpdateHierarchicalMenus()
// This is called after LEventDispatcher::UpdateMenus(), and enables parent
// items of any used submenu.
{
	if (!LCommander::GetTarget())
		return;
	Int16		menuItem;
	MenuHandle	macMenuH = nil;
	LMenu		*theMenu;
	CommandT	theCommand;
	LMenuBar	*theMenuBar = LMenuBar::GetCurrentMenuBar();
	while (theMenuBar->FindNextCommand(menuItem, macMenuH,
										theMenu, theCommand))
	{										
		short menuChar;
		::GetItemCmd(macMenuH, menuItem, &menuChar);
		if (menuChar != hMenuCmd)
			continue;
		// We have an item item with the "hierarchical" flag.

		// If we have a hierarchal menu that HAS a command number (ie CommandFromIndex
		// returns something greater than zero) then we're going to trust that it
		// has been properly enabled already. If we don't give the root menu item the
		// benefit of the doubt then we could end up accidentally enabling it if it has
		// more than 31 items in its sub menu
		if (theMenu->CommandFromIndex(menuItem) < 0) {
		
			// Find the child menu, see if LEventDispatcher::UpdateMenus() has
			// marked it as "used", and if so, make sure we enable this item
			short childMenuID;
			::GetItemMark(macMenuH, menuItem, &childMenuID);
			LMenu* childMenu = theMenuBar->FetchMenu(childMenuID);
			if (childMenu && childMenu->IsUsed())
			{
				::EnableItem(macMenuH, menuItem);
				theMenu->SetUsed(true);
			}
		}
	}
	// Now make sure entire menus get enabled.
	while (theMenuBar->FindNextMenu(theMenu))
	{
		macMenuH = theMenu->GetMacMenuH();
		Uint32 originalFlags = (**macMenuH).enableFlags;
		if (theMenu->IsUsed())
			::EnableItem(macMenuH, 0);
		else
			::DisableItem(macMenuH, 0);
		if (originalFlags != (**macMenuH).enableFlags)
			::InvalMenuBar();
	}
} // CFrontApp::UpdateHierarchicalMenus


/*
	App::::MakeNewDocument
	Called by PowerPlant when a user chooses "New"
	Returns a window (which is also a model object) in this case. Will change.
	Is recordable.
*/
LModelObject* CFrontApp::MakeNewDocument()
{

	try
		{
		enum
		{
			kBlankPage,
			kHomePage,
			kLastPage
		};
		int32	prefValue = kHomePage;
		int	prefResult = PREF_GetIntPref("browser.startup.page", &prefValue);
		if (prefResult != PREF_NOERROR)
		{
			prefValue = kHomePage;
		}
		URL_Struct* theURL;

		switch (prefValue)
		{
			case kBlankPage:
				theURL = nil;
				break;
			case kLastPage:
				char	urlBuffer[1024];
				if (GH_GetMRUPage(urlBuffer, 1024))
				{
					theURL = NET_CreateURLStruct(urlBuffer, NET_DONT_RELOAD);
				}
				else
				{
					theURL = nil;
					//theURL = NET_CreateURLStruct(urlBuffer, NET_DONT_RELOAD);
				}
				break;
			case kHomePage:
			default:
				{
					CStr255 url = CPrefs::GetString(CPrefs::HomePage);
					if (url.Length() > 0)
						theURL = NET_CreateURLStruct(url, NET_DONT_RELOAD);
					else
						theURL = nil;
				}
				break;
		}

		CURLDispatcher::DispatchURL(theURL, NULL, false, true);
		}
	catch (...)
		{
		
		
		}
		
	return NULL;	
}


//
// ChooseDocument
//
// Picks a document, and opens it in a window (uses existing window if one is open). This
// takes advantage of the new NavigationServices stuff if it is available.
//
void CFrontApp::ChooseDocument()
{
	static const OSType myTypes[] = { 'TEXT', 'JPEG', 'GIFf'};
	
	UDesktop::Deactivate();	// Always bracket this
	
	FSSpec fileSpec;
	Boolean fileSelected = SimpleOpenDlog ( 3, myTypes, &fileSpec );

	if ( fileSelected )
		OpenDocument(&fileSpec);

	UDesktop::Activate();
	
} // ChooseDocument


CFrontApp* CFrontApp::GetApplication()
{
	return sApplication;
}

// •• commands

void CFrontApp::ProcessCommandStatus(CommandT	inCommand,Boolean &outEnabled, Boolean &outUsesMark,
								 Char16		&outMark, Str255		outName)
{
	if (GetState() == programState_Quitting)	
	// If we are quitting, all commands are disabled
	{
		outEnabled = FALSE;
		return;
	}
	else
		LApplication::ProcessCommandStatus(inCommand,outEnabled, outUsesMark,outMark, outName);
}

//-----------------------------------
void CFrontApp::FindCommandStatus( CommandT command, Boolean& enabled, 
	Boolean& usesMark, Char16& mark, Str255 outName )
//-----------------------------------
{
	// Allow only close and quit if memory is low.
	if (Memory_MemoryIsLow())
	{
		if (command != cmd_Quit) // cmd_Close and cmd_CloseAll will also be avail., from elsewhere.
			enabled = FALSE;
		else
			LDocApplication::FindCommandStatus (command, enabled, usesMark, mark, outName);
		return;
	}
	// Do not enable menus behind modal dialogs
	if (IsFrontWindowModal())	// Only enable quit when front window is modal
	{
		if (command != cmd_Quit)
			enabled = FALSE;
		else
			LDocApplication::FindCommandStatus (command, enabled, usesMark, mark, outName);
		return;
	}		
	usesMark = FALSE;
	switch ( command )
	{
		case cmd_GoForward:
			if (CApplicationEventAttachment::CurrentEventHasModifiers(optionKey))
			{
				LString::CopyPStr(::GetPString(MENU_FORWARD_ONE_HOST), outName);
			}
			else
			{
				LString::CopyPStr(::GetPString(MENU_FORWARD), outName);
			}
			break;
		case cmd_GoBack:
			if (CApplicationEventAttachment::CurrentEventHasModifiers(optionKey))
			{
				LString::CopyPStr(::GetPString(MENU_BACK_ONE_HOST), outName);
			}
			else
			{
				LString::CopyPStr(::GetPString(MENU_BACK), outName);
			}
			break;

		case cmd_ShowJavaConsole:
#if defined (JAVA)
			usesMark = true;
			mark = LJ_IsConsoleShowing() ? 0x12 : 0;
			enabled = mJavaEnabled;
#endif /* defined (JAVA) */
			break;

#ifdef EDITOR
//		case cmd_New_Document_Heirarchical_Menu:
		case cmd_NewWindowEditor:
		case cmd_NewWindowEditorIFF:
		case cmd_New_Document_Template:
		case cmd_New_Document_Wizard:		
		case cmd_OpenFileEditor:
		case cmd_EditEditor:
#endif // EDITOR
#ifdef MOZ_MAIL_NEWS
		case cmd_MailNewsFolderWindow:
		case cmd_NewsGroups:
	//	case cmd_GetNewMail:
		case cmd_Inbox:
		case cmd_MailNewsSearch:
		case cmd_MailFilters:
		case cmd_SearchAddresses:
		case cmd_AddressBookWindow:
		case cmd_NewFolder:
#endif // MOZ_MAIL_NEWS
#if defined(MOZ_MAIL_COMPOSE) || defined(MOZ_MAIL_NEWS)
		case cmd_NewMailMessage:
#endif // MOZ_MAIL_COMPOSE
#ifdef EDITOR
		case cmd_OpenURLEditor:
#endif // EDITOR
		case cmd_BrowserWindow:
		case cmd_Preferences:
		case 19896: /*cmd_NetToggle*/
		case cmd_OpenURL:
//		case cmd_EditGeneral:
		case cmd_EditNetwork:
		case cmd_EditSecurity:
// old mail/news stuff
//		case cmd_MailWindow:
//		case cmd_NewsWindow:
//		case cmd_MailTo:
		case cmd_CoBrandLogo:
		case cmd_AboutPlugins:
		case cmd_LaunchCalendar:
		case cmd_Launch3270:
		case cmd_LaunchNetcaster:
#ifdef FORTEZZA
		case cmd_FortezzaCard:
		case cmd_FortezzaChange:
		case cmd_FortezzaView:
		case cmd_FortezzaInfo:
		case cmd_FortezzaLogout:
#endif // FORTEZZA
			enabled = TRUE;
		break;			
		
		case cmd_LaunchConference:
			enabled = mConferenceApplicationExists;
			break;
		
		case cmd_BookmarksWindow:
		case cmd_HistoryWindow:
		case cmd_NCNewWindow:
			enabled = !Memory_MemoryIsLow();
			break;
			
		case cmd_LaunchImportModule:
			enabled = mImportModuleExists;
			break;

		case cmd_New:
		case cmd_Open:
			enabled = !Memory_MemoryIsLow();
			break;
#ifdef MOZ_TASKBAR
		case cmd_ToggleTaskBar:
			enabled = TRUE;
			CTaskBarListener::GetToggleTaskBarWindowMenuItemName(outName);
			break;
#endif // MOZ_TASKBAR
#ifdef MOZ_OFFLINE
		// Online/Offline mode
		case cmd_ToggleOffline:
		case cmd_SynchronizeForOffline:
			enabled = TRUE;
			UOffline::FindOfflineCommandStatus(command, enabled, outName);
			break;
#endif // MOZ_OFFLINE
		default:
			ResIDT menuID;
			Int16 menuItem;
			if ( command >= DIR_BUTTON_BASE && command <= DIR_BUTTON_LAST )
				enabled = TRUE;
			else if (IsSyntheticCommand(command, menuID, menuItem) && menuID == cWindowMenuID)
				// --ML 4.0b2 Disable all Window menu items without a command number
				// (i.e. Conference, Calendar, IBM, History)
				enabled = FALSE;
// the code below was used ifndef MAILNEWS
//			else if ((command > cmd_WINDOW_MENU_BASE) && (command < cmd_WINDOW_MENU_BASE_LAST))
//				enabled = TRUE;
			else
				LDocApplication::FindCommandStatus (command, enabled, usesMark, mark, outName);
	}
} // CFrontApp::FindCommandStatus

extern void NET_ToggleTrace();

//-----------------------------------
Boolean CFrontApp::ObeyCommand(CommandT inCommand, void* ioParam)
//-----------------------------------
{
	Boolean bCommandHandled = true;
	
	switch ( inCommand )
		{	
		case cmd_BrowserWindow:
		/*
			URL_Struct* theURL = NULL;
			CURLDispatcher* theDispatcher = CURLDispatcher::GetURLDispatcher();
			Assert_(theDispatcher != NULL);
			theDispatcher->DispatchToView(NULL, theURL, FO_CACHE_AND_PRESENT);
		*/
				// this code is stolen from the editor button, below!  Thanks Kathy!
				// get the front window so we know if an browser window is already in the front
			CMediatedWindow *wp,  *browserwin;
			CWindowIterator iterAny(WindowType_Any);
			iterAny.Next(wp);
			CWindowIterator iter(WindowType_Browser);
			iter.Next(browserwin);
			if (browserwin && (wp == browserwin))
			{
					// count the number of windows; stuff current browser window into browserwin
					// put the window we want to be frontmost into wp (reuse)
					// spec says to cycle through all windows of our type so we bring the
					// backmost-window to the front and push the others down in the stack
				int numWindows = 1;
				while (iter.Next(wp))
				{
					numWindows++;
					browserwin = wp;
				}
				
				if (numWindows == 1)
					browserwin = NULL;		// set to NULL so we create new window below
			}
			
				// if no browser in front window bring the frontmost browser window to front
				// if browser is in front select the window determined above and we're done!
			if (browserwin)
			{
				browserwin->Show();
				browserwin->Select();
				break;
			}
			else		// let's make a new one
			{
				MakeNewDocument();
			}
			break;
		case 19896: /*cmd_NetToggle*/
			NET_ToggleTrace();
		break;
		case cmd_ShowJavaConsole:
#if defined (JAVA)
			{
			if (LJ_IsConsoleShowing())
				LJ_HideConsole();
			else
				LJ_ShowConsole();
			}
#endif /* defined (JAVA) */
			break;
			
		case cmd_HistoryWindow:
		{
		
			CNavCenterWindow* navCenter = dynamic_cast<CNavCenterWindow*>(URobustCreateWindow::CreateWindow(CNavCenterWindow::res_ID, this));
			navCenter->BringPaneToFront ( HT_VIEW_HISTORY );
		}
			break;
			
		case cmd_BookmarksWindow:
		case cmd_NCNewWindow:
		{
			CNavCenterWindow* navCenter = dynamic_cast<CNavCenterWindow*>(URobustCreateWindow::CreateWindow(CNavCenterWindow::res_ID, this));
			navCenter->BringPaneToFront ( HT_VIEW_BOOKMARK );
		}
			break;
			
#if defined(MOZ_MAIL_COMPOSE) || defined(MOZ_MAIL_NEWS)
		case cmd_NewMailMessage:
			MSG_MailDocument(NULL);
			break;
#endif // MOZ_MAIL_COMPOSE || MOZ_MAIL_NEWS

		case cmd_OpenURL:
			DoOpenURLDialog();
			break;
					
#ifdef EDITOR
		
		case cmd_OpenURLEditor:
			DoOpenURLDialogInEditor();
			break;
		
		case cmd_New_Document_Template:
			{
			CStr255 urlString = CPrefs::GetString(CPrefs::EditorNewDocTemplateLocation);
			if ( urlString != CStr255::sEmptyString )
				{
				URL_Struct* theURL = NET_CreateURLStruct(urlString, NET_DONT_RELOAD);
				if (theURL)
					CURLDispatcher::DispatchURL(theURL, NULL, false, true);
				}
			}
			break;
			
		case cmd_New_Document_Wizard:
			char* url;
			if (PREF_CopyConfigString("internal_url.page_from_wizard.url", &url) == PREF_NOERROR)
				{
				URL_Struct* theURL = NET_CreateURLStruct(url, NET_DONT_RELOAD);
				if (theURL)
					CURLDispatcher::DispatchURL(theURL, NULL, false, true);
				}
			break;
			
		case cmd_NewWindowEditorIFF:
		{
			// get the front window so we know if an editor window is already in the front
			CMediatedWindow *wp,  *editwin;
			CWindowIterator iterAny(WindowType_Any);
			iterAny.Next(wp);
			
			CWindowIterator iter(WindowType_Editor);
			iter.Next(editwin);
			
			if ( editwin && ( wp == editwin ) )
			{
				// count the number of windows; stuff current editor window into editwin
				// put the window we want to be frontmost into wp (reuse)
				// spec says to cycle through all windows of our type so we bring the
				// backmost-window to the front and push the others down in the stack
				int numWindows = 1;
				while ( iter.Next(wp) )
				{
					numWindows++;
					editwin = wp;
				}
				
				if ( numWindows == 1 )
					editwin = NULL;		// set to NULL so we create new window below
			}
			
			// if no editor in front window bring the frontmost editor window to front
			// if editor is in front select the window determined above and we're done!
			if ( editwin )
			{
				editwin->Show();
				editwin->Select();
				break;
			}
			// else continue below
		}
		case cmd_NewWindowEditor:
			CEditorWindow::MakeEditWindow( NULL, NULL );
			break;

		case cmd_OpenFileEditor:
			{
				
//			StandardFileReply myReply;
		
			static const OSType myTypes[] = { 'TEXT'};
			
			UDesktop::Deactivate();	// Always bracket this
			
			FSSpec fileSpec;
			Boolean fileSelected = SimpleOpenDlog ( 1, myTypes, &fileSpec );
				
			UDesktop::Activate();
			
			if (!fileSelected) return TRUE;			// we handled it... we just didn't do anything!
					
			char* localURL = CFileMgr::GetURLFromFileSpec(fileSpec);
			if (localURL == NULL) return TRUE;		
			
			NET_cinfo *cinfo = NET_cinfo_find_type (localURL);
			if (cinfo && cinfo->type && (!strcasecomp (cinfo->type, TEXT_HTML) 
			|| !strcasecomp (cinfo->type, UNKNOWN_CONTENT_TYPE) 
			|| !strcasecomp (cinfo->type, TEXT_PLAIN)))
				{
				URL_Struct * request = NET_CreateURLStruct ( localURL, NET_NORMAL_RELOAD );
				if ( request )
					CEditorWindow::MakeEditWindow( NULL, request );
				}
			else
				{
				ErrorManager::PlainAlert( NOT_HTML );
				}

			XP_FREE(localURL);
			}
			break;
			
		case cmd_EditEditor:
			CPrefsDialog::EditPrefs(	CPrefsDialog::eExpandEditor, 
						PrefPaneID::eEditor_Main, CPrefsDialog::eIgnore);
		break;
	
#endif
		
		case cmd_Preferences:
			CPrefsDialog::EditPrefs();
			break;
		
#ifdef MOZ_MAIL_NEWS
		case cmd_NewsGroups:
		case cmd_MailNewsFolderWindow:
		{
			CMailNewsFolderWindow::FindAndShow(true, inCommand);
			break;
		}
		case cmd_GetNewMail:
			// If it came here, we must assume there are no mail/news windows.
			// So open the inbox First.
			CThreadWindow* tw = CThreadWindow::ShowInbox(cmd_GetNewMail);
			break;
		case cmd_Inbox:
			CThreadWindow::ShowInbox(cmd_Nothing);
			break;
		case cmd_MailTo:
			{
			MSG_Mail(NULL);	// Crashes now, libmsg error
			}
			break;
#endif // MOZ_MAIL_NEWS
			
		case cmd_AboutPlugins:
			DoGetURL( "about:plugins" );
			break;

		case cmd_LaunchConference:
			LaunchExternalApp(kConferenceAppSig, CONFERENCE_APP_NAME);
			break;

		case cmd_LaunchImportModule:
			LaunchExternalApp(kImportAppSig, IMPORT_APP_NAME);
			break;

		// --ML Polaris
		case cmd_LaunchCalendar:
			LaunchExternalApp(kCalendarAppSig, CALENDAR_APP_NAME);
			break;

		case cmd_LaunchAOLInstantMessenger:
			LaunchExternalApp(kAOLInstantMessengerSig, AIM_APP_NAME);
			break;
	
		case cmd_Launch3270:
			Launch3270Applet();
			break;

		case cmd_LaunchNetcaster:
			LaunchNetcaster();
			break;


#ifdef FORTEZZA
		case cmd_FortezzaCard:
			SSL_FortezzaMenu(NULL,SSL_FORTEZZA_CARD_SELECT);
			break;
		case cmd_FortezzaChange:
			// er, excuse me, mr fortezza, but CHyperWin::Show() might return NULL....
			CHyperWin* win = GetFrontHyperWin();
			if (win)
				SSL_FortezzaMenu(win->fHyperView->GetContext(),SSL_FORTEZZA_CHANGE_PERSONALITY);
			break;
		case cmd_FortezzaView:
			CHyperWin* win = GetFrontHyperWin();
			if (win)
				SSL_FortezzaMenu(win->fHyperView->GetContext(),SSL_FORTEZZA_VIEW_PERSONALITY);
			break;
		case cmd_FortezzaInfo:
			SSL_FortezzaMenu(NULL,SSL_FORTEZZA_CARD_INFO);
			break;
		case cmd_FortezzaLogout:
			/* sBookMarkContext */
			SSL_FortezzaMenu(NULL,SSL_FORTEZZA_LOGOUT);
			break;
#endif

#ifdef MOZ_MAIL_NEWS
		// This really shouldn't be here, but since the mail/news windows are not up yet,
		// it's here for testing
		case cmd_MailNewsSearch:
			CSearchWindowManager::ShowSearchWindow();
			break;
		case cmd_MailFilters:
			CFiltersWindowManager::ShowFiltersWindow();
			break;
		case cmd_AddressBookWindow:
			CAddressBookManager::ShowAddressBookWindow();
			break;
		case cmd_NewFolder:
		{
			CMessageFolder folder(nil);
			UFolderDialogs::ConductNewFolderDialog(folder);
			break;
		}
	//	case cmd_SearchAddresses:
	//		CSearchWindowManager::ShowSearchAddressesWindow();
	//		break;
#endif // MOZ_MAIL_NEWS
#ifdef MOZ_TASKBAR
		case cmd_ToggleTaskBar:
			CTaskBarListener::ToggleTaskBarWindow();
			break;
#endif // MOZ_TASKBAR
#ifdef MOZ_OFFLINE
		// Online/Offline mode
		case cmd_ToggleOffline:
			UOffline::ObeyToggleOfflineCommand();
			break;

		case cmd_SynchronizeForOffline:
			UOffline::ObeySynchronizeCommand();
			break;
#endif // MOZ_OFFLINE
		default:
			{
			// Directory urls
			if ( inCommand >= DIR_BUTTON_BASE && inCommand <= DIR_BUTTON_LAST )
				DoOpenDirectoryURL( inCommand );

			// Spinning icon
			else if ( inCommand == LOGO_BUTTON || inCommand == cmd_CoBrandLogo )
				DoOpenLogoURL( inCommand );

			else if (!CWindowMenu::ObeyWindowCommand(inCommand))
			{
				ResIDT menuID = ( HiWord( -inCommand ) );
				Int16 itemNum = ( LoWord( -inCommand ) );
				if ( menuID == kHMHelpMenuID )
				{
					itemNum -= sHelpMenuOrigLength;
					DoHelpMenuItem( itemNum );
				}
				else
					bCommandHandled = LDocApplication::ObeyCommand(inCommand, ioParam);
				}
			break;
		}
	} // switch
	return bCommandHandled;
} // CFrontApp::ObeyCommand


//-----------------------------------
void CFrontApp::DoOpenDirectoryURL( CommandT menuCommand )
//-----------------------------------
{
	CStr255		urlString;
	Boolean		isMenuCommand = (menuCommand >= DIR_MENU_BASE);
	short stringID = isMenuCommand ? DIR_MENU_BASE : DIR_BUTTON_BASE;

	char* url;
	if ( PREF_CopyIndexConfigString(
			isMenuCommand ? "menu.places.item" : "toolbar.places.item",
			menuCommand - stringID, "url", &url) == PREF_NOERROR )
	{
		DoGetURL(url);
		XP_FREE(url);
	}
}

void CFrontApp::DoOpenLogoURL( CommandT menuCommand )
{
	// Admin Kit: Handle co-brand button
	char* url;
	if ( menuCommand == LOGO_BUTTON &&
		PREF_CopyConfigString("toolbar.logo.url", &url) == PREF_NOERROR )
	{
		DoGetURL(url);
		XP_FREE(url);
	}
	else
	{
		StringHandle handle = GetString( LOGO_BUTTON_URL_RESID );
		CStr255 urlString = *((unsigned char**)handle);
		if ( urlString != CStr255::sEmptyString )
			DoGetURL( (unsigned char*)urlString );
	}
}

void CFrontApp::DoWindowsMenu(CommandT inCommand)
{
	Int32 index = inCommand - cmd_WINDOW_MENU_BASE;
	LWindow * win;
	if (fWindowsMenu.FetchItemAt(index, &win))
		win->Select();
}

// DoGetURL loads the given url into the frontmost window, or new one if there is no frontmost
// Provides a bottleneck for UI generated requests to load a URL
void CFrontApp::DoGetURL( const cstring& url )
{
	// Check for kiosk mode and bail if it's set so that the user can't manually
	// go to a different URL.  Note that this does NOT prevent dispatching to a different
	// URL from the content.
	if (CAppleEventHandler::GetKioskMode() == KioskOn)
		return;
	
	// Is this REALLY necessary?
	(CFrontApp::GetApplication())->ProperStartup( NULL, FILE_TYPE_NONE );	// Making sure that we have the prefs loaded

	URL_Struct* theURL = NET_CreateURLStruct(url, NET_DONT_RELOAD);
	if (theURL)
		CURLDispatcher::DispatchURL(theURL, NULL);
}

//-----------------------------------------------------------------------------
// Chenge of address
//		AppleEvents has moved to CAppleEventHandler.cp
//			C Hull
//-----------------------------------------------------------------------------

/*------------------------------------------------
AppleEvent Dispach Here
	In: & AppleEvent/ Evenc coming in
		& ReplyEvent/ Reply event back to sender
		& Descriptor/ Not sure why this was extracted, but OK.
		  AppleEvent/ ID The event
	Out: Event Handled by the sAppleEvents entity
------------------------------------------------*/
void CFrontApp::HandleAppleEvent(
	const AppleEvent&	inAppleEvent,
	AppleEvent&			outAEReply,
	AEDesc&				outResult,
	long				inAENumber)
{
	XP_TRACE(("Handling event %d", inAENumber));
	Try_
	{
		if ( CAppleEventHandler::sAppleEventHandler == NULL )
			new CAppleEventHandler;

		ThrowIfNil_(CAppleEventHandler::sAppleEventHandler);
		
		CAppleEventHandler::sAppleEventHandler->HandleAppleEvent(inAppleEvent, 
					outAEReply, 
					outResult, 
					inAENumber);
					
	}			
	Catch_( inErr )
	{}
	EndCatch_
}




void CFrontApp::GetAEProperty(DescType			inProperty,
								const AEDesc	&inRequestedType,
								AEDesc			&outPropertyDesc) const
{
	try
	{
		if (CAppleEventHandler::sAppleEventHandler == NULL)
			new CAppleEventHandler;
		ThrowIfNil_(CAppleEventHandler::sAppleEventHandler);
		
		CAppleEventHandler::sAppleEventHandler->GetAEProperty(inProperty, 
																inRequestedType, 
																outPropertyDesc);
	}			
	catch(...)
	{}
}






void CFrontApp::SetAEProperty(DescType 			inProperty,
								const AEDesc	&inRequestedType,
								AEDesc			&outPropertyDesc)
{
	try
	{
		if (CAppleEventHandler::sAppleEventHandler == NULL)
			new CAppleEventHandler;
		ThrowIfNil_(CAppleEventHandler::sAppleEventHandler);
		
		CAppleEventHandler::sAppleEventHandler->SetAEProperty(inProperty, 
																inRequestedType, 
																outPropertyDesc);
	}			
	catch(...)
	{}
}






//-----------------------------------------------------------------------------
// Printing
//-----------------------------------------------------------------------------

//-----------------------------------
void CFrontApp::SetupPage()
//-----------------------------------
{
	THPrint		hPrintRec;
	Boolean		havePrinter;
	
	hPrintRec = CPrefs::GetPrintRecord();
	
	havePrinter = hPrintRec != 0;

	if (havePrinter)
	{
		UPrintingMgr::ValidatePrintRecord( hPrintRec );
		if ( UPrintingMgr::OpenPrinter() )
		{
			UDesktop::Deactivate();
			UHTMLPrinting::OpenPageSetup( hPrintRec );
			UDesktop::Activate();

			UPrintingMgr::ClosePrinter();
		} else
			havePrinter = false;
	}
	if ( !havePrinter )
		FE_Alert( NULL, GetPString ( NO_PRINTER_RESID ) );
} // CFrontApp::SetupPage


//-----------------------------------------------------------------------------
// Menubar management
//-----------------------------------------------------------------------------
void CFrontApp::SetMenubar( ResIDT mbar, Boolean inUpdateNow )
{
	fWantedMbar = mbar;
	
	if (inUpdateNow) {
		UpdateMenus();
	}
}

void CFrontApp::InstallMenus()
{
	// Menu creation must go here, because until now, the menu bar has not
	// been created. (see LApplication::Run())
	CHistoryMenu* historyMenu = new CHistoryMenu(cHistoryMenuID);
	CWindowMenu* windowMenu = new CWindowMenu(cWindowMenuID);
	// create CNSMenuBarManager
	new CNSMenuBarManager(CWindowMediator::GetWindowMediator());
	LMenuBar::GetCurrentMenuBar()->InstallMenu(historyMenu, InstallMenu_AtEnd);
	CBookmarksAttachment::InstallMenus();
	LMenuBar::GetCurrentMenuBar()->InstallMenu(windowMenu, InstallMenu_AtEnd);
	CWindowMediator::GetWindowMediator()->AddListener(windowMenu);

	// Admin Kit friendly Help and Places menus: labels and URLs are
	// initialized in config.js and may be overridden by config file.
	MenuHandle balloonMenuH;
	HMGetHelpMenuHandle( &balloonMenuH );
	if ( balloonMenuH )
	{
		sHelpMenuOrigLength = CountMItems(balloonMenuH);
		
		sHelpMenuItemCount = BuildConfigurableMenu( balloonMenuH,
			"menu.help.item", HELP_URLS_MENU_STRINGS );
	}
	
	/*	NOTE: Places menu is installed in CBookmarksAttachment::InstallMenus(),
		which calls back to BuildConfigurableMenu. */
}

int CFrontApp::BuildConfigurableMenu(MenuHandle menu, const char* xp_name, short stringsID)
{
	CStr255 entry;
	short i;
	
	// Append configurable items first
	for (i = 1; i <= 25; i++) {
		char*	label;
		if ( PREF_CopyIndexConfigString(xp_name, i - 1, "label", &label) == PREF_NOERROR )
		{
			entry = label;
			XP_FREE(label);
			
			if (entry.IsEmpty())	// blank entry marks end of menu
				break;
			
			StripChar(entry, '&');	// strip ampersands from label
			
			AppendMenu(menu, entry);
		}
		else
			break;
	}
	
	// Then append fixed items, if any (i.e. About Netscape)
	if (stringsID > 0) {
		CStringListRsrc menuStrings( stringsID );
		for (short j = 1; j < menuStrings.CountStrings() + 1; j++) {
			menuStrings.GetString( j, entry );
			AppendMenu(menu, entry);
		}
	}
	
	return i - 1;
}

void CFrontApp::DoHelpMenuItem( short itemNum )
{
	// use configurable URL if defined, else use built-in URL
	char* url;
	if ( itemNum <= sHelpMenuItemCount && PREF_CopyIndexConfigString("menu.help.item",
		itemNum - 1, "url", &url) == PREF_NOERROR )
	{
		DoGetURL( url );
		XP_FREE(url);
	}
	else {
		CStr255		urlString;
	
		GetIndString( urlString, HELP_URLS_RESID, itemNum - sHelpMenuItemCount );
		
		if ( urlString != CStr255::sEmptyString )
			DoGetURL( (unsigned char*)urlString );
	}
}

extern pascal long MDEF_MenuKey( long theMessage, short theModifiers, MenuHandle hMenu );

//-----------------------------------
void CFrontApp::EventKeyDown( const EventRecord& inMacEvent )
// • we override this to patch Mercutio in
//-----------------------------------
{
	CommandT keyCommand = cmd_Nothing;
	Int32 menuChoice = 0;
	// Remap function keys.  Is there a better way?  jrm 97/04/03
	switch ((inMacEvent.message & keyCodeMask) >> 8)
	{
		case vkey_F1:
			keyCommand = cmd_Undo;
			break;
		case vkey_F2:
			keyCommand = cmd_Cut;
			break;
		case vkey_F3:
			keyCommand = cmd_Copy;
			break;
		case vkey_F4:
			keyCommand = cmd_Paste;
			break;
		default:
			// Modal dialogs might have OK/cancel buttons, and handle their own keys.
			// So only convert to command when front window is not modal
			if ((inMacEvent.modifiers & cmdKey) && (inMacEvent.message & charCodeMask) == '.')
			{
				if (!IsFrontWindowModal())
					keyCommand = cmd_Stop;
			}
			// Check if the keystroke is a Menu Equivalent
			else if (LMenuBar::GetCurrentMenuBar()->CouldBeKeyCommand(inMacEvent))
			{
				CTargetedUpdateMenuRegistry::SetCommands(CFrontApp::GetCommandsToUpdateBeforeSelectingMenu());
				CTargetedUpdateMenuRegistry::UpdateMenus();
				
				menuChoice = MDEF_MenuKey(
					inMacEvent.message,
					inMacEvent.modifiers, ::GetMenu( 666 ) );
							
				// If menuChoice is 0, then a modifer key may have been held down that
				//    didn't map to a known command-key equivalent (e.g. cmd-opt-n).
				//    In this case, we will try again with just the cmdKey. Recall that
				//    this is what the Finder does (with cmd-n for New Folder, for instance).
				//    It makes especially good sense when you think about the fact that since
				//    the command and option keys are close, users sometimes accidently hit both.
				//    Without this "fix", cmd_Nothing would be returned.
				if (menuChoice == 0)
				{
					EventModifiers modifiers = cmdKey;
					
					menuChoice = MDEF_MenuKey(inMacEvent.message, modifiers, ::GetMenu( 666 ));
				}				
											 
				if ( HiWord( menuChoice ) != 0 )
					keyCommand = LMenuBar::GetCurrentMenuBar()->FindCommand(
											HiWord(menuChoice),
											LoWord(menuChoice));
			}
	}
	SignalIf_(LCommander::GetTarget() == nil);

	// Don't allow quit events if it isn't safe to quit but remember the request
	if ((keyCommand == cmd_Quit) && !fSafeToQuit)
	{
		keyCommand = cmd_Nothing;
		fUserWantsToQuit = true;
	}
	if (keyCommand != cmd_Nothing)
	{
		LCommander::SetUpdateCommandStatus( true );
		LCommander::GetTarget()->ProcessCommand(keyCommand, &menuChoice);
		::HiliteMenu( 0 );
	}
	else
		LCommander::GetTarget()->ProcessKeyPress(inMacEvent);
} // CFrontApp::EventKeyDown

//-----------------------------------------------------------------------------
// About Box & Splash Screen
//-----------------------------------------------------------------------------


void CFrontApp::ShowAboutBox()
{
	DoGetURL( "about:" );
}

void* FE_AboutData( const char* which, char** data_ret, int32* length_ret,
char** content_type_ret )
{
	char* ss = NULL; // security string, of course
	Handle          data = NULL;

	try {
		// Nuke the ismap portion so we can hide the url
		const char* ismap = strrchr( (char*)which, '?' );
		CStr255 dataName = which ? which : "";
		if ( ismap )
			{
			ismap++;
			for ( int i = 1; i <= dataName.Length(); i++ )
			if (dataName[i] == '?')
				{
				dataName[0] = i - 1;
				break;
				}
			}
		Boolean         doSprintf = FALSE;

		/*
		        about:                  128             text of about box, includes logo
		        about:logo              129             picture for about box
		        about:plugins   130             list of installed plug-ins
		        about:authors   131             text for authors box
		        about:authors?  132             text for team box
		        about:teamlogo  133             picture for team box
		        about:mailintro 134
		        about:rsalogo   135             picture for rsa logo
		        about:mozilla   136             mozilla easter egg
		        about:hype              137             hype sound
		        about:license   2890    license text
		*/
		if ( dataName == "" )
			{
			data = ::GetResource( 'TEXT', ABOUT_ABOUTPAGE_TEXT );
			*content_type_ret = TEXT_MDL;
			doSprintf = true;
			}
		else if ( dataName == "logo" )
			{
			data = ::GetResource ( 'Tang', ABOUT_BIGLOGO_TANG );
			*content_type_ret = IMAGE_GIF;
			}
		else if ( dataName == "plugins" )
			{
			data = ::GetResource ( 'TEXT', ABOUT_PLUGINS_TEXT );
			*content_type_ret = TEXT_MDL;
			}
		else if ( dataName == "authors" )
			{
			data = ::GetResource ( 'TEXT', ABOUT_AUTHORS_TEXT );
			*content_type_ret = TEXT_MDL;
			}
		else if ( dataName == "mailintro" )
			{
			data = ::GetResource( 'TEXT', ABOUT_MAIL_TEXT );
			*content_type_ret = TEXT_MDL;
			}
		else if ( dataName == "rsalogo" )
			{
			data = ::GetResource ('Tang', ABOUT_RSALOGO_TANG);
			*content_type_ret = IMAGE_GIF;
			}
		else if ( dataName == "javalogo" )
			{
			data = ::GetResource ('Tang', ABOUT_JAVALOGO_TANG);
			*content_type_ret = IMAGE_GIF;
			}
		else if (dataName == "qtlogo")
			{
			data = ::GetResource ('Tang', ABOUT_QTLOGO_TANG);
			*content_type_ret = IMAGE_GIF;
			}

		else if (dataName == "insologo")
			{
			data = ::GetResource ('Tang', ABOUT_INSOLOGO_TANG);
			*content_type_ret = IMAGE_GIF;
			}
		else if (dataName == "litronic")
			{
			data = ::GetResource ('Tang', ABOUT_LITRONIC_TANG);
			*content_type_ret = IMAGE_GIF;
			}

		else if (dataName == "mclogo")
			{
			data = ::GetResource ('Tang', ABOUT_MCLOGO_TANG);
			*content_type_ret = IMAGE_GIF;
			}

		else if (dataName == "mmlogo")
			{
			data = ::GetResource ('Tang', ABOUT_MMLOGO_TANG);
			*content_type_ret = IMAGE_GIF;
			}

		else if (dataName == "ncclogo")
			{
			data = ::GetResource ('Tang', ABOUT_NCCLOGO_TANG);
			*content_type_ret = IMAGE_GIF;
			}

		else if (dataName == "odilogo")
			{
			data = ::GetResource ('Tang', ABOUT_ODILOGO_TANG);
			*content_type_ret = IMAGE_GIF;
			}

		else if (dataName == "symlogo")
			{
			data = ::GetResource ('Tang', ABOUT_SYMLOGO_TANG);
			*content_type_ret = IMAGE_GIF;
			}

		else if (dataName == "tdlogo")
			{
			data = ::GetResource ('Tang', ABOUT_TDLOGO_TANG);
			*content_type_ret = IMAGE_GIF;
			}

		else if (dataName == "visilogo")
			{
			data = ::GetResource ('Tang', ABOUT_VISILOGO_TANG);
			*content_type_ret = IMAGE_GIF;
			}

		else if (dataName == "visilogo")
			{
			data = ::GetResource ('Tang', ABOUT_VISILOGO_TANG);
			*content_type_ret = IMAGE_GIF;
			}

		else if (dataName == "coslogo")
			{
			data = ::GetResource ('Tang', ABOUT_COSLOGO_TANG);
			*content_type_ret = IMAGE_JPG;
			}



#ifdef FORTEZZA
		else if (dataName == "litronic")
			{
			data = ::GetResource ('Tang', ABOUT_LITRONIC_TANG);
			*content_type_ret = IMAGE_GIF;
			}
#endif
		else if (dataName == "mozilla")
			{
			data = ::GetResource ( 'TEXT', ABOUT_MOZILLA_TEXT );
			*content_type_ret = TEXT_MDL;
			}
		else if (dataName == "hype")
			{
			data = ::GetResource ( 'Tang', ABOUT_HYPE_TANG );
			*content_type_ret = AUDIO_BASIC;
			}
		else if ( dataName == "license" )
			{
			data = ::GetResource ('TEXT', ABOUT_LICENSE);
			*content_type_ret = TEXT_PLAIN;
			}
		else if ( dataName == "custom" )        // optional resource added by EKit
			{
			data = ::GetResource ( 'TEXT', ABOUT_CUSTOM_TEXT );
			*content_type_ret = TEXT_PLAIN;
			}
		#ifdef EDITOR
		else if ( dataName == "editfilenew" )
			{
			data = ::GetResource ( 'TEXT', ABOUT_NEW_DOCUMENT );
			*content_type_ret = TEXT_HTML;
			}
		#endif
		else if ( dataName == "flamer" )
			{
			data = ::GetResource ('Tang', ABOUT_MOZILLA_FLAME);
			*content_type_ret = IMAGE_GIF;
			}		
		else
			{
			data = ::GetResource ('TEXT', ABOUT_BLANK_TEXT);
			*content_type_ret = TEXT_MDL;
			}

		// prepare the arguments and return
		if ( data )
			{
			UInt32          dataSize;

			::DetachResource( data );
			::HNoPurge( data );
			::MoveHHi( data );

			dataSize = GetHandleSize(data);

			if ( doSprintf )
				{
				// Security stuff ( 3 strings -> 1 )
				char*           s0 = (char *)GetCString(SECURITY_LEVEL_RESID );
				char*           s1 = SECNAV_SecurityVersion( PR_TRUE );
				char*           s2 = SECNAV_SSLCapabilities();
				ThrowIfNil_(s0);
				ThrowIfNil_(s1);
				ThrowIfNil_(s2);
				char * secStr = PR_smprintf( s0, s1, s2 );
				ThrowIfNil_(secStr);

				// Version
				CStr255         vers;
				::GetIndString( vers, ID_STRINGS, APPVERSION_STRING_INDEX );


				// Assure that data terminates with nul by appending the NULL string
					{
					dataSize += 1;
					SetHandleSize( data, dataSize);
					OSErr err = MemError();
					ThrowIfOSErr_(err);
					(*data)[dataSize-1] = '\0';
					}
				
				// Create the final string
				::HLock(data);
				char * finalStr = PR_smprintf((char*)*data, (char*)vers, (char*)vers, (char*)secStr);
				::HUnlock(data);

				XP_FREE( secStr );
				ThrowIfNil_(finalStr);

				// Make handle out of final string

				dataSize = strlen(finalStr) + 1;
				Handle finalHandle = ::NewHandle( dataSize );
				ThrowIfNil_(finalHandle);
				::BlockMoveData( finalStr, *finalHandle, dataSize );
				XP_FREE( finalStr );
				// Exchange
				::DisposeHandle(data);
				data = finalHandle;
				}

            *data_ret = *data;
            *length_ret = dataSize;
			}
		else
			{
		    XP_ASSERT(0);
		    *data_ret = NULL;
		    *length_ret = 0;
		    *content_type_ret = NULL;
			}
        }
	catch(...)
		{
		Assert_(false);
		if (data != NULL)
			{
		    ::DisposeHandle(data);
		    data = NULL;
			}

		if (ss != NULL)
		    XP_FREE(ss);

		*data_ret = NULL;
		*length_ret = 0;
		*content_type_ret = NULL;
		}
		
	if (data)
		::HLock(data);
		
	return data;
}

void FE_FreeAboutData( void* data, const char* /*which*/ )
{
	if ( !data )
		return;
	Handle h = (Handle)data;
	DisposeHandle( h );
}

		
// ===========================================================================
//		• Main Program
// ===========================================================================

XP_BEGIN_PROTOS
OSErr	InitLibraryManager(size_t poolsize, int zoneType, int memType);
void	CleanupLibraryManager(void);
XP_END_PROTOS

extern void 	Memory_DoneWithMemory();
extern UInt8	MemoryCacheFlusher(size_t size);
extern void	 	PreAllocationHook(void);

const Size cSystemHeapFudgeFactor = 96 * 1024;

void main( void )
{
	SetDebugThrow_(debugAction_Nothing);
#ifdef DEBUG
	//SetDebugSignal_(debugAction_SourceDebugger); // SysBreak broken in MetroNub 1.3.2
	SetDebugSignal_(debugAction_LowLevelDebugger);
#else
	SetDebugSignal_(debugAction_Nothing);
#endif
	CFrontApp*			app = NULL;

	// System Heap check hack
	Handle sysHandle = ::NewHandleSys(cSystemHeapFudgeFactor);
	if (sysHandle)
		::DisposeHandle(sysHandle);
	Size maxBlockSize = ::FreeMemSys();
	if (maxBlockSize < cSystemHeapFudgeFactor)
		::ExitToShell();

	// Initialize the tracing package if we're debugging
#ifdef DEBUG
	XP_TraceInit();
#endif

	// pkc -- instantiate main thread object.  As side effect, NSPR will create new
	// subheap if this is the first allocation call, ie, if no static initializers
	// did an allocation.
	
	//-----------------------------------
	// NOTE: SetApplLimit and MaxApplZone are called from NSPR!
	// in MacintoshInitializeMemory()
	//-----------------------------------
	
	// • initialize QuickDraw
	UQDGlobals::InitializeToolbox( &qd );

	// Preference library needs to be initialized at this point because
	// the msglib thread constructor is making a prefapi call
	PREF_Init(NULL);

#if __MC68K__
	{
			// Bug #58086: this hack locks the "Enable Java" preference `off' and
			//	must be removed when Java works for 68K.
		static const char* inline_javascript_text ="lockPref(\"security.enable_java\", false)";
		PREF_EvaluateJSBuffer(inline_javascript_text, strlen(inline_javascript_text));
	}
#else

	// • double check to make sure we have Java installed
	DisableJavaIfNotAvailable();
	
#endif
	
	// NSPR/MOCHA Initialization
#ifndef NSPR20
	PR_Init("Navigator", 0,0,0);
#endif
	mozilla_thread = PR_CurrentThread();
#ifdef NSPR20
	PR_SetThreadGCAble();
#endif

	// • initialize the memory manager
	//		it's important that this is done VERY early
	Memory_InitializeMemory();
	
	// • if we're not on a PowerPC, check that we're on an '020 or later
	//		and have System 7 installed
	ConfirmWeWillRun();
	RNG_RNGInit();		// This needs to be called before the main loop
	ProcessSerialNumber psn;
	if (NetscapeIsRunning(psn))
	{
		ErrorManager::PlainAlert(NO_TWO_NETSCAPES_RESID);
		SetFrontProcess(&psn);
	}
	else
	{
#if defined(PROFILE) || defined (PROFILE_LOCALLY)
#ifndef DONT_DO_MOZILLA_PROFILE
	StartProfiling();	
#endif
#endif
	
	//	NET_ToggleTrace();
		
		app = new CFrontApp;
#if defined(QAP_BUILD)		
		QAP_AssistHook (kQAPAppToForeground, 0, NULL, 0, 0);
#endif
		app->Run();

#if defined(QAP_BUILD)		
		QAP_AssistHook (kQAPAppToBackground, 0, NULL, 0, 0);
#endif

		delete app;
#if defined(PROFILE) || defined (PROFILE_LOCALLY)
#ifndef DONT_DO_MOZILLA_PROFILE
	StopProfiling();
#endif
#endif
	}
	
	Memory_DoneWithMemory();

	// do this at the very end, god knows who's fetching prefs during cleanup
	PREF_Cleanup();
}

static void InitDebugging()
{
	// • debugging Setup
}

static void ConfirmWeWillRun()
{
#ifndef powerc
	Assert68020();
	AssertSystem7();
#endif
	AssertRequiredGuts();
}

//-----------------------------------
static Boolean NetscapeIsRunning(ProcessSerialNumber& psn)
// True if other versions of Netscape are running
//-----------------------------------
{
	OSErr err;
	ProcessInfoRec info;
	ProcessSerialNumber myPsn;
	::GetCurrentProcess(&myPsn);
	
	psn.highLongOfPSN = 0;
	psn.lowLongOfPSN  = kNoProcess;
	do
	{
		err = GetNextProcess(&psn);
		if( err == noErr )
		{

			info.processInfoLength = sizeof(ProcessInfoRec);
			info.processName = NULL;
			info.processAppSpec = NULL;

			err= GetProcessInformation(&psn, &info);
		}
		if ((err == noErr) &&
			((psn.highLongOfPSN != myPsn.highLongOfPSN) || (psn.lowLongOfPSN != myPsn.lowLongOfPSN)) &&
			(info.processSignature == emSignature || info.processSignature == 'MOSM'))
				return TRUE;
	} while (err == noErr);
	return FALSE;
}
 
//-----------------------------------
Boolean	CFrontApp::AttemptQuitSelf(long	/*inSaveOption*/)
//-----------------------------------
{
#ifdef MOZ_OFFLINE
	XP_Bool	value;
	PREF_GetBoolPref("offline.prompt_synch_on_exit", &value);
	if (value)
	{
		// Put up dialog
		StDialogHandler aHandler(20004, NULL);
		LWindow* dialog = aHandler.GetDialog();

		// Get the window title to use later in the progress dialog.
		Str255	windowTitle;
		dialog->GetDescriptor(windowTitle);

		// Run the dialog
		MessageT message;
		do
		{
			message = aHandler.DoDialog();

		} while ((message != 'Yes ') && (message != 'No  ') && (message != msg_Cancel));
		
		// Use the result
		if (message == 'Yes ')
			UOffline::ObeySynchronizeCommand(UOffline::syncModal);

		if (message == msg_Cancel)
			return false;
	}
#endif // MOZ_OFFLINE
	return true;
}

//-----------------------------------
void CFrontApp::DoQuit(
	Int32	inSaveOption)
//-----------------------------------
{
	LDocApplication::DoQuit(inSaveOption);

	if (mState == programState_Quitting)
		CDeferredTaskManager::DoQuit(inSaveOption);
//	if (mState == programState_Quitting)
//		LTSMSupport::DoQuit(inSaveOption);
}

//-----------------------------------
void CFrontApp::DispatchEvent(
	const EventRecord&	inMacEvent)
//		From IM-Text 7-22
//-----------------------------------
{

	try
	{
		try
		{
			if(! LTSMSupport::TSMEvent(inMacEvent))
				LDocApplication::DispatchEvent(inMacEvent);
		}
		catch (int err) // some people do throw-em!  Also "throw memFullErr" will come here.
		{
			throw (OSErr)err;
		}
	}
	// Here we catch anything thrown from anywhere. (OSErr and ExceptionCode).
	// STR# 7098 is reserved for just this purpose.  See ns/cmd/macfe/restext/macfe.r
	// These alerts all use ALRT_ErrorOccurred.  See ns/cmd/macfe/rsrc/MacDialogs.rsrc.
	// The value of ALRT_ErrorOccurred is in reserr.h
	// The alert says
	//		"Your last command could not be completed because <reason>"
	//		"(Error code <nnn>)"
	catch (OSErr err)
	{
		DisplayErrorDialog ( err );
	}
	catch (ExceptionCode err)
	{
		DisplayExceptionCodeDialog ( err );
	}
}
// ---------------------------------------------------------------------------
//		• AdjustCursor
//		From IM-Text 7-22
// ---------------------------------------------------------------------------
void CFrontApp::AdjustCursor( const EventRecord& inMacEvent )
{
#if defined (JAVA)	
	if ( UsingCustomAWTFrameCursor() )
		return;
#endif /* defined (JAVA) */
									// Find out where the mouse is
	WindowPtr	macWindowP;
	Point		globalMouse = inMacEvent.where;
	Int16		thePart = ::FindWindow( globalMouse, &macWindowP );

	Boolean		useArrow = TRUE;	// Assume cursor will be the Arrow
	LWindow* theWindow = NULL;

	if ( macWindowP )
	{		// Mouse is inside a Window
		theWindow = LWindow::FetchWindowObject( macWindowP );
	}
	if (( theWindow ) &&
		(theWindow->IsActive() || theWindow->HasAttribute(windAttr_GetSelectClick)) && // adjust inactive window if one click away - mjc
		theWindow->IsEnabled() )
	{
								// Mouse is inside an active and enabled
								//   PowerPlant Window. Let the Window
								//   adjust the cursor shape.
								
								// Get mouse location in Port coords
		Point	portMouse = globalMouse;
		theWindow->GlobalToPortPoint( portMouse );
		
		theWindow->AdjustCursor( portMouse, inMacEvent );
		useArrow = FALSE;
	}
	if ( useArrow )
	{
		// Window didn't set the cursor
		// Default cursor is the arrow			
		::SetCursor( &UQDGlobals::GetQDGlobals()->arrow );				
	}

	//
	// Rather than trying to calculate an accurate mouse region,
	// we define a region that contains just the one pixel where
	// the mouse is located. This is quick, and handles the common
	// case where this application is in the foreground but the user
	// isn't doing anything. However, any mouse movement will generate
	// a mouse-moved event.
	//		
	::SetRectRgn( mMouseRgnH, globalMouse.h, globalMouse.v,
							 globalMouse.h + 1, globalMouse.v + 1 );
}



void
CFrontApp::EventSuspend	(const EventRecord &inMacEvent)
{
	CSuspenderResumer::Suspend();
#ifdef MOZ_MAIL_NEWS
	CCheckMailContext::SuspendResume();					// Stop all current notification of mail
#endif // MOZ_MAIL_NEWS
	CPluginView::BroadcastPluginEvent(inMacEvent);			// Tell plug-ins about the suspend
	LDocApplication::EventSuspend(inMacEvent);				// Do the suspend

#if defined(QAP_BUILD)	
	QAP_AssistHook (kQAPAppToBackground, 0, NULL, 0, 0);
#endif

#ifdef DAVIDM_SPEED2
	SetSleepTime(4);		// be more friendly in the background
#endif

}
	
void
CFrontApp::EventResume(const EventRecord &inMacEvent)
{
	CSuspenderResumer::Resume();
	LDocApplication::EventResume(inMacEvent);				// Do the resume
	CPluginView::BroadcastPluginEvent(inMacEvent);			// Tell plug-ins about the resume
#ifdef MOZ_MAIL_NEWS
	CCheckMailContext::SuspendResume();					// Reset mail notification
#endif // MOZ_MAIL_NEWS
	CInternetConfigInterface::ResumeEvent();			// check is InternetConfig changed

#if defined(QAP_BUILD)	
	QAP_AssistHook (kQAPAppToForeground, 0, NULL, 0, 0);
#endif

#ifdef DAVIDM_SPEED2
	SetSleepTime(1);		// let's go faster
#endif
}


void
CFrontApp::EventKeyUp(const EventRecord	&inMacEvent)
{
	//
	// Pass key ups down to the target if it’s a plug-in. The static variable
	// sPluginTarget is maintained by CPluginViews when they become or are no
	// longer the target.  (We know calling ProcessKeyPress for a key up is OK
	// because CPluginView overrides it.)
	//
	CPluginView* pluginTarget = CPluginView::sPluginTarget;
	if (pluginTarget != NULL && pluginTarget == sTarget)
		pluginTarget->ProcessKeyPress(inMacEvent);
	else
	{
		/* if it's a CKeyUpReceiver, send the key up. We used to send keyups to
		 * targets if a browser view was on duty, which had two disadvantages: it required
		 * any potential targets in a browser view to handle a key up, and sometimes
		 * the commander chain would be incorrect so key ups were sent to a target in a 
		 * view that was not on duty.
		 */
		if (dynamic_cast<CKeyUpReceiver *>(sTarget))
			sTarget->ProcessKeyPress(inMacEvent);
		/*
		// Pass key ups down to the target if it has a super commander which is
		// a browser view. Key up events must be sent to mocha. 1997-02-24 mjc
		if (CBrowserView::sOnDutyBrowserView != NULL)
			sTarget->ProcessKeyPress(inMacEvent);		
		*/
	}
}


// A C interface to ProcessNextEvent, which needs to be called when waiting for
// the .jsc file to load at startup
extern "C" {
void FEU_StayingAlive();
}


void FEU_StayingAlive()
{
	CFrontApp::GetApplication()->ProcessNextEvent();
}

void
CFrontApp::ProcessNextEvent()
{ 
#ifdef PROFILE 
#ifdef PROFILE_ON_CAPSLOCK 
	if (IsThisKeyDown(0x39)) // caps lock 
		ProfileStart(); 
	else 
		ProfileStop(); 
#endif // PROFILE_ON_CAPSLOCK 
#endif // PROFILE 

#ifdef DAVIDM_SPEED2
	static	UInt32		sNextWNETicks  = 0;
	const	UInt32		kWNETicksInterval = 4;
#endif
 
	try 
	{ 
		// Handle all pending NSPR events 
		PR_ProcessPendingEvents(mozilla_event_queue); 
	}
	catch (OSErr err) 
	{ 
		DisplayErrorDialog ( err ); 
	} 
	catch (ExceptionCode err) 
	{ 
		DisplayExceptionCodeDialog ( err ); 
	}
 
	 // The block surrounded by *** ... from LApplication::ProcessNextEvent() *** 
	 // is identical to LApplication::ProcessNextEvent with one exception: 
	 // the EventRecord is declared static in order to persist across calls 
	 // to ProcessNextEvent to prevent dangling references to the event 
	 // recorded by CApplicationEventAttachment (read the usage notes for 
	 // CApplicationEventAttachment). 
	  
	 // *** Begin block from LApplication::ProcessNextEvent() *** 
	  
	static EventRecord	macEvent;
	Boolean				haveUserEvent = true;			// so we call WNE every time in the background
	
	  // When on duty (application is in the foreground), adjust the 
	  // cursor shape before waiting for the next event. Except for the 
	  // very first time, this is the same as adjusting the cursor 
	  // after every event. 
	  
	if (IsOnDuty()) { 

		// Plug-ins and Java need key up events, which are masked by default 
		// (the event mask lomem is set to everyEvent - keyUp).  To ensure 
		// that it’s always set to mask out nothing, reset it here every time 
		// before we call WaitNextEvent. 
		// 

		SetEventMask(everyEvent);

		// Calling OSEventAvail with a zero event mask will always 
		// pass back a null event. However, it fills the EventRecord 
		// with the information we need to set the cursor shape-- 
		// the mouse location in global coordinates and the state 
		// of the modifier keys. 

		haveUserEvent = ::OSEventAvail(everyEvent, &macEvent);
		AdjustCursor(macEvent); 
	}

#ifdef DAVIDM_SPEED2
#ifndef NSPR20
	CSelectObject socketToCallWith;
	Boolean	doWNE = haveUserEvent || ::LMGetTicks() > sNextWNETicks || ( gNetDriver && !gNetDriver->CanCallNetlib(socketToCallWith) );
#else
	Boolean	doWNE = haveUserEvent || ::LMGetTicks() > sNextWNETicks || NET_PollSockets();
#endif // NSPR_20	
	SetUpdateCommandStatus(false);
	
	if ( doWNE )
	{
#endif // DAVIDM_SPEED2

	// Retrieve the next event. Context switch could happen here. 
	
#ifdef PROFILE 
#ifndef PROFILE_WAITNEXTEVENT 
	 ProfileSuspend(); 
#endif // PROFILE_WAITNEXTEVENT 
#endif // PROFILE 

	 Boolean gotEvent = ::WaitNextEvent(everyEvent, &macEvent, mSleepTime, mMouseRgnH); 
	
#ifdef PROFILE 
#ifndef PROFILE_WAITNEXTEVENT 
	 ProfileResume(); 
#endif // PROFILE_WAITNEXTEVENT 
#endif // PROFILE 
	  
	// Let Attachments process the event. Continue with normal 
	// event dispatching unless suppressed by an Attachment. 

	if (LAttachable::ExecuteAttachments(msg_Event, &macEvent)) { 
		if (gotEvent) { 
			DispatchEvent(macEvent); 
		} else { 
			UseIdleTime(macEvent); 
		} 
	}
	
#ifdef DAVIDM_SPEED2
		sNextWNETicks = ::LMGetTicks() + kWNETicksInterval;
	}
#endif // DAVIDM_SPEED2

  	LPeriodical::DevoteTimeToRepeaters(macEvent);
 
	// Update status of menu items
	if (IsOnDuty() && GetUpdateCommandStatus()) { 
		UpdateMenus(); 
	}

 // *** End block from LApplication::ProcessNextEvent() *** 
}


void CFrontApp::DoOpenURLDialog(void)
{
	StBlockingDialogHandler	theHandler(liLoadItemWind, this);
	LWindow* theDialog = theHandler.GetDialog();
	LEditField* theEdit = (LEditField*)theDialog->FindPaneByID('edit');
	Assert_(theEdit != NULL);
	theDialog->SetLatentSub(theEdit);
	theDialog->Show();

	MessageT theMessage = msg_Nothing;	
	while ((theMessage != msg_Cancel) && (theMessage != msg_OK))
		theMessage = theHandler.DoDialog();
	
	if (theMessage == msg_OK)
		{
		cstring		curl;
		CStr255		purl;
		theEdit->GetDescriptor(purl);
		CleanUpLocationString (purl);
		curl = (unsigned char*)purl;
		DoGetURL(curl);
		}
}


#ifdef EDITOR
void CFrontApp::DoOpenURLDialogInEditor(void)
{
	StBlockingDialogHandler	theHandler(liLoadItemWind, this);
	LWindow* theDialog = theHandler.GetDialog();
	LEditField* theEdit = (LEditField*)theDialog->FindPaneByID('edit');
	Assert_(theEdit != NULL);
	theDialog->SetLatentSub(theEdit);
	theDialog->Show();

	MessageT theMessage = msg_Nothing;	
	while ((theMessage != msg_Cancel) && (theMessage != msg_OK))
		theMessage = theHandler.DoDialog();
	
	if (theMessage == msg_OK)
		{
		cstring		curl;
		CStr255		purl;
		// we really should try to do better parsing here
		theEdit->GetDescriptor( purl );
		CleanUpLocationString( purl );
		curl = (unsigned char*)purl;

		URL_Struct* theURL = NET_CreateURLStruct( curl, NET_DONT_RELOAD );
		if ( theURL )
			CEditorWindow::MakeEditWindow( NULL, theURL );
		}
}
#endif // EDITOR


void CFrontApp::ShowSplashScreen()
{
	// Register the splash screen first so that we can show it.
	RegisterClass_(CSplashScreen);
	RegisterClass_(CResPicture);

	mSplashScreen = (CSplashScreen*)LWindow::CreateWindow(2891, this);
	mSplashScreen->Show();
	mSplashScreen->UpdatePort();
}

void CFrontApp::DestroySplashScreen()
{
	delete mSplashScreen;
	mSplashScreen = NULL;
}

void CFrontApp::SplashProgress(CStr255 inMessage)
{
	if (sApplication->mSplashScreen != NULL)
		sApplication->mSplashScreen->SetDescriptor(inMessage);
}

//-----------------------------------
void CFrontApp::ListenToMessage(MessageT inMessage, void * ioParam)
//-----------------------------------
{
	switch (inMessage)
	{
#ifdef MOZ_MAIL_NEWS
		case cmd_GetNewMail:
		case cmd_NewsGroups:
		case cmd_MailNewsFolderWindow:
#endif // MOZ_MAIL_NEWS
#ifdef EDITOR
		case cmd_NewWindowEditorIFF:
//		case cmd_NewMailMessage:
#endif // EDITOR
#ifdef MOZ_MAIL_NEWS
		case cmd_ToggleOffline:
#endif // MOZ_MAIL_NEWS
		case cmd_BrowserWindow:
			ObeyCommand(inMessage);
			break;
		case msg_GrowZone:
			MemoryIsLow();
			// 1998.01.12 pchen -- replicate fix for bug #85275
			// We don't know how much memory was freed, so just set bytes freed to 0
			*((Int32 *)ioParam) = 0;
			break;
		default:
			break;
	}
}

void CFrontApp::LaunchExternalApp(OSType inAppSig, ResIDT inAppNameStringResourceID)
{
	Assert_(inAppNameStringResourceID);

	FSSpec	appSpec;
	OSErr	err = CFileMgr::FindApplication(inAppSig, appSpec);
	CStr255	theAppName(::GetCString(inAppNameStringResourceID));
	CStr255 theMessage;
	
	if (err == noErr)
	{
		ProcessSerialNumber psn;
		err = UProcessUtils::LaunchApplication(
			appSpec, launchContinue + launchNoFileFlags, psn);
	}
	
	switch (err)
	{
		case noErr:
			break;
			
		case memFullErr:
		case memFragErr:
			theMessage = ::GetCString(MEMORY_ERROR_LAUNCH);
			::StringParamText(theMessage, theAppName);
			ErrorManager::PlainAlert(theMessage);
			break;
			
		case fnfErr:
			theMessage = ::GetCString(FNF_ERROR_LAUNCH);
			::StringParamText(theMessage, theAppName);
			ErrorManager::PlainAlert(theMessage);
			break;
			
		default:
			theMessage = ::GetCString(MISC_ERROR_LAUNCH);
			::StringParamText(theMessage, theAppName);
			ErrorManager::PlainAlert(theMessage);
			break;
	}
}

// Tries to find local file 3270/HE3270EN.HTM
Boolean CFrontApp::Find3270Applet(FSSpec& tn3270File)
{
	Boolean found = false;
	FSSpec appFolder = CPrefs::GetFilePrototype( CPrefs::NetscapeFolder );
	
	CStr255 folderName;	
	::GetIndString( folderName, 300, ibm3270Folder );
	
	if ( CFileMgr::FindWFolderInFolder(appFolder.vRefNum, appFolder.parID,
		 folderName, &tn3270File.vRefNum, &tn3270File.parID) == noErr )
	{
		::GetIndString( tn3270File.name, 300, ibm3270File );

		found = CFileMgr::FileExists(tn3270File);
	}
	return found;
}

void CFrontApp::Launch3270Applet()
{
	Boolean ok = false;
	FSSpec tn3270File;
	if ( Find3270Applet(tn3270File) )
	{
		char* url = CFileMgr::GetURLFromFileSpec(tn3270File);
		if (url) {
			// Opens the URL in a new window (copied from DoGetURL)
			//  !! winfe opens a window with no toolbars, if we care.
			URL_Struct* theURL = NET_CreateURLStruct(url, NET_DONT_RELOAD);
			if (FE_MakeNewWindow(NULL, theURL, NULL, NULL) != NULL)
				ok = true;
			free(url);
		}
	}
	if (!ok) {
		ErrorManager::PlainAlert(ERROR_LAUNCH_IBM3270);
	}
}

FSSpec CFrontApp::CreateAccountSetupSpec()
{
	FSSpec			asw;

	asw = CPrefs::GetFilePrototype( CPrefs::NetscapeFolder );
	::GetIndString( asw.name, 300, aswName );
	return asw;
}

Boolean CFrontApp::LaunchAccountSetup()
{
	FSSpec			tmp = CreateAccountSetupSpec();
	
	LFile			aswFile( tmp );
	Handle			alias;
	OSErr			err;
	FSSpec			startAswFile;
	Boolean			wasChanged;
	
#ifdef DEBUG
	try
	{
#endif

		aswFile.OpenResourceFork( fsRdPerm );

#ifdef DEBUG
	}
	catch ( ... )
	{
		StandardFileReply		reply;
		AliasHandle				alias;
		SFTypeList				types;
		
		UDesktop::Deactivate();
		::StandardGetFile( NULL, -1, types, &reply );
		UDesktop::Activate();

		if ( reply.sfGood )
		{
			LFile		startFile( reply.sfFile );
			
			aswFile.CreateNewFile( emSignature, 'ASWl', smSystemScript ); 
			aswFile.OpenResourceFork( fsWrPerm );
			alias = startFile.MakeAlias();
			if ( alias )
				AddResource( (Handle)alias, 'alis', 1, reply.sfFile.name );
			aswFile.CloseResourceFork();
			aswFile.OpenResourceFork( fsRdPerm );
		}
	}
	
#endif DEBUG

	alias = ::Get1Resource( 'alis', 1 );
	if ( !alias )
		return FALSE;

	err = ::ResolveAlias( NULL, (AliasHandle)alias, &startAswFile, &wasChanged );
	if ( err == noErr )
	{
		CFrontApp::GetApplication()->SendAEOpenDoc( startAswFile );
		return TRUE;
	}
	return FALSE; 
}

static void launchNetcasterCallback(void* closure);

static void launchNetcasterCallback(void* /*closure*/)
{
	FE_RunNetcaster(NULL);
}

// Launch Netcaster on a timeout to avoid interfering with other components because
// it is slow to start up.
void CFrontApp::LaunchNetcaster()
{
	 FE_SetTimeout(launchNetcasterCallback, NULL, 0);
}

// We keep track of the Netcaster context in the app so we can assure ourselves
// that only one Netcaster context will be open at any given time.  These calls
// are used by FE_RunNetcaster to either launch Netcaster (if the netcaster context
// is NULL) or to activate its window (if not).  The alternative would have been
// to use named contexts; the disadvantage is that the Netcaster window could be
// spoofed, and that we would have a performance hit whenever the Netcaster object
// were referenced (such as from JS). - EA

//-----------------------------------
MWContext *CFrontApp::GetNetcasterContext()
//-----------------------------------
{
	return mNetcasterContext;
}

//-----------------------------------
void CFrontApp::SetNetcasterContext(MWContext *context)
//-----------------------------------
{
	mNetcasterContext = context;	
}

// 97-08-23 pchen -- This method should be called when we're "low on memory."
// For now, we just call LJ_ShutdownJava().
//-----------------------------------
void CFrontApp::MemoryIsLow()
//-----------------------------------
{
#if defined (JAVA)
	// Call LJ_ShutdownJava() if Java is running
	if (LJ_GetJavaStatus() == LJJavaStatus_Running)
		LJ_ShutdownJava();
#endif /* defined (JAVA) */
}

#pragma mark -

//======================================
//		class CSplashScreen
//======================================

extern Boolean pref_FindAutoAdminLib(FSSpec& spec);

CSplashScreen::CSplashScreen(LStream* inStream)
	:	LWindow(inStream)
{
	mStatusCaption = NULL;
	mPictureResourceFile = NULL;
}

CSplashScreen::~CSplashScreen()
{
	ResIDT thePicID = mBackPicture->GetPictureID();
	
	// this returns the picture that has already been loaded by the picture pane
	PicHandle thePicHandle = ::GetPicture(thePicID);			
	if (thePicHandle != NULL)
		::ReleaseResource((Handle)thePicHandle);
	
	if (mPictureResourceFile) {
		mPictureResourceFile->CloseResourceFork();
		delete mPictureResourceFile;
	}
}

void CSplashScreen::SetDescriptor(ConstStringPtr inDescriptor)
{
	mStatusCaption->SetDescriptor(inDescriptor);
	// we're relying on the fact that the splash window has erase on
	// update set so that the old caption gets erased in the window's
	// background color (which we're assuming to be black)
	mStatusCaption->UpdatePort();
}

void CSplashScreen::FinishCreateSelf(void)
{
	LWindow::FinishCreateSelf();
	
	mStatusCaption = (LCaption*)FindPaneByID(1);
	Assert_(mStatusCaption != NULL);
	
	mBackPicture = (CResPicture*)FindPaneByID(2);
	Assert_(mBackPicture != NULL);	
	
		// choose the b/w or color splash screen
	ResIDT thePicID = (GetDepth(GetDeepestDevice()) == 1) ? 129 : 128;
	
	// If AutoAdminLib is installed, then load the Communicator Pro
	// splash screen from the library's resource fork.
	// Otherwise, picture comes from app resource.
	try {
		FSSpec autoAdminLib;
		if ( pref_FindAutoAdminLib(autoAdminLib) ) {
			mPictureResourceFile = new LFile(autoAdminLib);
			mPictureResourceFile->OpenResourceFork(fsRdPerm);
			
			mBackPicture->SetResFileID( mPictureResourceFile->GetResourceForkRefNum() );
		}
	}
	catch (...) {
	}
	
	mBackPicture->SetPictureID(thePicID);
}


//
// THIS IS HERE ONLY AS A PLACEHOLDER UNTIL I PUT THE NAV-SERVICES CODE INTO THE SOURCE TREE....
// DO NOT RELY ON THIS IMPLEMENTATION OR EVEN THAT THIS ROUTINE USES THE SF PACKAGE
//
Boolean SimpleOpenDlog ( short numTypes, const OSType typeList[], FSSpec* outFSSpec )
{
	Boolean fileSelected = false;
	StandardFileReply myReply;
	if (CApplicationEventAttachment::CurrentEventHasModifiers(optionKey))
		::StandardGetFile(NULL, numTypes, typeList, &myReply);
	else
		::StandardGetFile(NULL, -1, NULL, &myReply);
	fileSelected = myReply.sfGood;
	*outFSSpec = myReply.sfFile;

	return fileSelected;
	
} // SimpleOpenDialog

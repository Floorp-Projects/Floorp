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

// CAppleEventHandler.cp

// ---- Apple Events
#include "CAppleEventHandler.h"


/*-------------------------------------------------------------*/
//					Includes
/*-------------------------------------------------------------*/

// --- just in case
#include "uapp.h"

// --- Netscape macfe
#include "Netscape_Constants.h"
#include "earlmgr.h"
#include "findw.h"
#include "macgui.h"
#include "macutil.h"
#include "CBookmarksAttachment.h"
#include "BookmarksFile.h"
#include "mimages.h"
#include "mplugin.h"

#if defined (JAVA)
#include "mjava.h"
#endif /* defined (JAVA) */

#include "mregistr.h"
#include "prefw.h"
#include "resae.h"
#include "resgui.h"
#include "uerrmgr.h"
#include "ufilemgr.h"

// -- For getting Profile information
#include "prefapi.h"

// --- Netscape Misc
#include "CEditorWindow.h"
#include "CBrowserWindow.h"

// --- Just plain Misc
#include "mkutils.h" // only for  FREEIF() macro.  JRM 96/10/17

// --- Netscape NS
#include "CNSContextCallbacks.h"

// --- Netscape Browser
#include "CBrowserContext.h"
#include "CBrowserWindow.h"

// --- Netscape MailNews
#ifdef MOZ_MAIL_NEWS
#include "MailNewsSearch.h"
#include "MailNewsFilters.h"
#include "MailNewsAddressBook.h"
#include "MailNewsClasses.h"
#include "MailNewsgroupWindow_Defines.h"
#endif // MOZ_MAIL_NEWS
#include "BrowserClasses.h"

// --- Mac Toolbox
#include <Displays.h>		// For the display notice event
#include <Balloons.h>
#include <Errors.h>
#include <EPPC.h>



#include "CURLDispatcher.h"
#include "xp.h"

/*--------------------------------------------------------------*/
//		Constructor, Destructor and Dispach
/*--------------------------------------------------------------*/

/*----------------------------------------------------------------
	CAppleEventHandler::CAppleEventHandler
	Constructor
----------------------------------------------------------------*/
CAppleEventHandler::CAppleEventHandler()
{
	// Remember our one instance
	sAppleEventHandler = this;
	
	// We assume not operating in kiosk mode unless told otherwise
	fKioskMode = KioskOff;
	
}
/*----------------------------------------------------------------
	CAppleEventHandler::~CAppleEventHandler
	Destructor
----------------------------------------------------------------*/
CAppleEventHandler::~CAppleEventHandler()
{
	// Reste to NULL, we're gone
	sAppleEventHandler = NULL;
}

/*----------------------------------------------------------------
	CAppleEventHandler::HandleAppleEvent
	AppleEvent Dispach Here
	In: & AppleEvent	Incoming apple event
		& ReplyEvent	Reply event we can attach result info to
		& Descriptor	OUtgoing descriptor
		  AppleEvent ID	The Apple Event ID we were called with.
	Out: Event Handled by CAppleEvent
----------------------------------------------------------------*/
void CAppleEventHandler::HandleAppleEvent(
	const AppleEvent	&inAppleEvent,
	AppleEvent			&outAEReply,
	AEDesc				&outResult,
	long				inAENumber)
{
	switch (inAENumber)
	{
		case AE_GetURL:
		case AE_OpenURL:
		case AE_OpenProfileManager:
												// Required Suite
		case ae_OpenApp:
		case ae_OpenDoc:
		case ae_PrintDoc:
		case ae_Quit:
			break;
			
		default:
			if (!(CFrontApp::GetApplication() && CFrontApp::GetApplication()->HasProperlyStartedUp()))
			{
				// Ignore all apple events handled by this handler except get url, open
				// url and the required suite until we have properly started up.
				return;			
			}
			break;
	}

	switch (inAENumber) {	
	
		// ### Mac URL standard Suite
		// -----------------------------------------------------------
		case AE_GetURL:	// Direct object - url, cwin - HyperWindow
			HandleGetURLEvent( inAppleEvent, outAEReply, outResult, inAENumber );
			break;
			
		// ### Spyglass Suite
		// -----------------------------------------------------------
		case AE_OpenURL:
			HandleOpenURLEvent( inAppleEvent, outAEReply, outResult, inAENumber );
			break;
			
		case AE_CancelProgress:
			HandleCancelProgress( inAppleEvent, outAEReply, outResult, inAENumber);
			break;
			
		case AE_FindURL:		// located elsewhere
			CFileMgr::sFileManager.HandleFindURLEvent(inAppleEvent, outAEReply, outResult, inAENumber);
			break;

		case AE_ShowFile:
			HandleShowFile(inAppleEvent, outAEReply, outResult, inAENumber);
			break;
			
		case AE_ParseAnchor:
			HandleParseAnchor(inAppleEvent, outAEReply, outResult, inAENumber);
			break;
			
		case AE_SpyActivate:
			HandleSpyActivate(inAppleEvent, outAEReply, outResult, inAENumber);
			break;
			
		case AE_SpyListWindows:
			HandleSpyListWindows(inAppleEvent, outAEReply, outResult, inAENumber);
			break;
			
		case AE_GetWindowInfo:
			HandleSpyGetWindowInfo(inAppleEvent, outAEReply, outResult, inAENumber);
			break;
			
		case AE_RegisterWinClose:
		case AE_UnregisterWinClose:
			HandleWindowRegistration(inAppleEvent, outAEReply, outResult, inAENumber);
			break;
			
		// --- These Spyglass events are handled elswhere
		case AE_RegisterViewer:
			CPrefs::sMimeTypes.HandleRegisterViewerEvent( inAppleEvent, outAEReply, outResult, inAENumber );
			break;
			
		case AE_UnregisterViewer:
			CPrefs::sMimeTypes.HandleUnregisterViewerEvent( inAppleEvent, outAEReply, outResult, inAENumber );
			break;
			
		case AE_RegisterProtocol:
		case AE_UnregisterProtocol:
		case AE_RegisterURLEcho:
		case AE_UnregisterURLEcho:
			CNotifierRegistry::HandleAppleEvent(inAppleEvent, outAEReply, outResult, inAENumber);
			break;

			
		// ### Netscape Experimental Suite
		// -----------------------------------------------------------
		case AE_OpenBookmark:
			HandleOpenBookmarksEvent( inAppleEvent, outAEReply, outResult, inAENumber );
			break;
			
		case AE_ReadHelpFile:
			HandleReadHelpFileEvent( inAppleEvent, outAEReply, outResult, inAENumber );
			break;
			
		case AE_Go:
			HandleGoEvent (inAppleEvent, outAEReply, outResult, inAENumber );
			break;
			
		case AE_GetWD:	// Return Working Directory
			HandleGetWDEvent( inAppleEvent, outAEReply, outResult, inAENumber );
			break;
		
		case AE_OpenProfileManager:
			CFrontApp::sApplication->ProperStartup( NULL, FILE_TYPE_PROFILES );
			break;
				
		case AE_OpenAddressBook:
			HandleOpenAddressBookEvent( inAppleEvent, outAEReply, outResult, inAENumber );
			break;
		
		case AE_OpenComponent:
			HandleOpenComponentEvent( inAppleEvent, outAEReply, outResult, inAENumber );
			break;

		case AE_HandleCommand:
			HandleCommandEvent( inAppleEvent, outAEReply, outResult, inAENumber );
			break;
			
		case AE_GetActiveProfile:
			HandleGetActiveProfileEvent( inAppleEvent, outAEReply, outResult, inAENumber );
			break;
			
		case AE_GetProfileImportData:
			HandleGetProfileImportDataEvent( inAppleEvent, outAEReply, outResult, inAENumber );
			break;
			
		// ### ????  2014 looks like it's a core suite thing, but not defined in the aedt
		// -----------------------------------------------------------
/*		case 2014:		// Display has changed. Maybe in 2.0, should change the pictures
			{
				StAEDescriptor displayDesc;
				OSErr err = AEGetParamDesc(&inAppleEvent,kAEDisplayNotice,typeWildCard,&displayDesc.mDesc);
				if (err == noErr)
					SysBeep(1);
			}
			break;
*/		

		// ### If all else fails behave like a word processor
		// -----------------------------------------------------------
		default:
			CFrontApp::sApplication->LDocApplication::HandleAppleEvent( inAppleEvent, outAEReply, outResult, inAENumber );
			break;
	}
}

/*--------------------------------------------------------------------------*/
/*			----  Mac URL standard, supported by many apps   ----			*/
/*--------------------------------------------------------------------------*/	

/*----------------------------------------------------------------
	CAppleEventHandler::HandleGetURLEvent			Suite:URL
	Dispach the URL event to the correct window and let the
	window handle the event.

GetURL: Loads the URL (optionaly to disk)

	GetURL  string  -- The url 
		[to  file specification] 	-- file the URL should be loaded into  
		[inside  reference]  		-- Window the URL should be loaded to
		[from  string] 				-- Refererer, to be sent with the HTTP request
----------------------------------------------------------------*/
void CAppleEventHandler::HandleGetURLEvent(
	const AppleEvent	&inAppleEvent,
	AppleEvent			&outAEReply,
	AEDesc				&outResult,
	long				/*inAENumber*/)
{
	// We need to make sure
	
	CFrontApp::sApplication->ProperStartup( NULL, FILE_TYPE_GETURL );

	// Get the window, and let it handle the event
	StAEDescriptor	keyData;
	OSErr err = ::AEGetKeyDesc(&inAppleEvent, AE_www_typeWindow, typeWildCard,&keyData.mDesc);
	
	// TODO: handle dispatching to storage
	
	if (CFrontApp::GetApplication()->HasProperlyStartedUp())
	{
		// A window was specified, open the url in that window
		if ((err == noErr) && (keyData.mDesc.descriptorType == typeObjectSpecifier))
		{
			StAEDescriptor	theToken;
			err = ::AEResolve(&keyData.mDesc, kAEIDoMinimum, &theToken.mDesc);
			ThrowIfOSErr_(err);
			LModelObject * model = CFrontApp::sApplication->GetModelFromToken(theToken.mDesc);
			ThrowIfNil_(model);
			
			CBrowserWindow* win = dynamic_cast<CBrowserWindow*>(model);
			ThrowIfNil_(win);
			
			CBrowserWindow::HandleGetURLEvent(inAppleEvent, outAEReply, outResult, win);		
		}
		// No window or file specified, we will open the url in the frontmost
		// browser window or a new browser window if there are no open browser
		// windows.
		// Or no browser window if one is not appropriate.  Good grief! jrm.
		else
		{
			try
			{
				// 97-09-18 pchen -- first check to see if this is a NetHelp URL, and dispatch
				// it directly
				// 97/10/24 jrm -- Why can't we always dispatch directly?  CURLDispatcher
				// already has the logic to decide what's right!  Trying this.
				MoreExtractFromAEDesc::DispatchURLDirectly(inAppleEvent);
			}
			catch (...)
			{
				// Get a window
				
				CBrowserWindow* win = CBrowserWindow::FindAndShow(true);
				ThrowIfNil_(win);
					
				// Let the window handle the event
				
				CBrowserWindow::HandleGetURLEvent(inAppleEvent, outAEReply, outResult, win);
			}
		}
	}
	else
	{
		CBrowserWindow::HandleGetURLEvent(inAppleEvent, outAEReply, outResult);		
	}
} // CAppleEventHandler::HandleGetURLEvent

/*--------------------------------------------------------------------------*/
/*			 		----   The Spygless Suite   ----						*/
/*--------------------------------------------------------------------------*/	

#define errMakeNewWindow 100

/*----------------------------------------------------------------
	CAppleEventHandler::HandleOpenURLEvent		Suite:Spyglass
	Open to a URL
	In: Incoming apple event, reply event, outtgoing descriptor, eventID

		Flavors of OpenURL
		#define AE_spy_openURL		'OURL'	// typeChar OpenURL
			AE_spy_openURL_into			// typeFSS into
			AE_spy_openURL_wind			// typeLongInteger windowID
			AE_spy_openURL_flag			// typeLongInteger flags
				4 bits used.
				1 = 
				2 = 
				4 = 
				8 = Open into editor window
			AE_spy_openURL_post			// typeWildCard post data
			AE_spy_openURL_mime			// typeChar MIME type
			AE_spy_openURL_prog			// typePSN Progress app
			
	Just like with GetURL, figure out the window, and let it handle the load
	Arguments we care about: AE_spy_openURL_wind
	
	Out: OpenURLEvent handled
----------------------------------------------------------------*/
void CAppleEventHandler::HandleOpenURLEvent(
	const AppleEvent	&inAppleEvent,
	AppleEvent			&outAEReply,
	AEDesc				&outResult,
	long				/*inAENumber*/)
{
	// We need to make sure
	CFrontApp::sApplication->ProperStartup( NULL, FILE_TYPE_GETURL );
	
	// Get the Flags to see if we want an editor
	try
	{
		long flags;
		DescType realType;
		Size actualSize;
		OSErr err = ::AEGetParamPtr(&inAppleEvent, AE_spy_openURL_flag, typeLongInteger,
						 &realType, &flags, sizeof(flags), &actualSize);
		ThrowIfOSErr_(err);
#ifdef EDITOR
		if (flags & 0x8)
		{
			// Get the url (in case we want an editor)
			char* url = NULL;
			MoreExtractFromAEDesc::GetCString(inAppleEvent, keyDirectObject, url);
			ThrowIfNil_(url);
			// MAJOR CRASHING BUG!!!  if building Akbar 3.0 we should NOT CALL MakeEditWindow!!!
			URL_Struct * request = NET_CreateURLStruct (url, NET_DONT_RELOAD);
			XP_FREE (url);
			ThrowIfNil_(request);
			CEditorWindow::MakeEditWindow( NULL, request);
			return;
		}
#endif // EDITOR
	}
	catch(...)
	{
	}

	if (CFrontApp::GetApplication()->HasProperlyStartedUp())
	{
		// Get the window, and let it handle the event
		try
		{
			Int32 windowID;
			Size realSize;
			OSType realType;
			
			OSErr err = ::AEGetParamPtr(&inAppleEvent, AE_spy_openURL_wind, typeLongInteger,
							 &realType, &windowID, sizeof(windowID), &realSize);
			ThrowIfOSErr_(err);
			
			// If 0 then create a new window,  
			//	If FFFF set to front most window
			CBrowserWindow* win = NULL;
			if (windowID == 0)
			{
				win = CURLDispatcher::CreateNewBrowserWindow();
			}
			else if (windowID == 0xFFFFFFFF)	// Frontmost  window
				win = CBrowserWindow::FindAndShow();
			else
				win = CBrowserWindow::GetWindowByID(windowID);
			
			ThrowIfNil_(win);
			CBrowserWindow::HandleOpenURLEvent(inAppleEvent, outAEReply, outResult, win);		
		}
		catch(...)
		{
			// 97-09-18 pchen -- first check to see if this is a NetHelp URL, and dispatch
			// it directly
			// 97/10/24 jrm -- Why can't we always dispatch directly?  CURLDispatcher
			// already has the logic to decide what's right!  Trying this.
			MoreExtractFromAEDesc::DispatchURLDirectly(inAppleEvent);
			return;
		}			
	}
	else
	{
		CBrowserWindow::HandleOpenURLEvent(inAppleEvent, outAEReply, outResult);		
	}
} // CAppleEventHandler::HandleOpenURLEvent


/*----------------------------------------------------------------
	ShowFile
	Similar to odoc, except that it takes a few extra arguments
	keyDirectObject	file spec
	AE_spy_showFile_mime MIME type (optional)
	AE_spy_showFile_win window ID (optional)
	AE_spy_showFile_url url (optional)

	ShowFile: Similar to OpenDocuments, except that it specifies the parent 
		URL, and MIME type of the file
		
	ShowFile  
		alias  					-- File to open
		[MIME type  string]  	-- MIME type
		[Window ID  integer]  	-- Window to open the file in
		[URL  string]  			-- Use this as a base URL
		
	Result:   
		integer  				-- Window ID of the loaded window. 
			0 means ShowFile failed, 
			FFFFFFF means that data was not appropriate type to display in the browser.
----------------------------------------------------------------*/
void CAppleEventHandler::HandleShowFile(const AppleEvent	&inAppleEvent, 
		AppleEvent& /*outAEReply*/, AEDesc& /*outResult*/, long /*inAENumber*/)
{
Assert_(false);
	char * url = NULL;
	char * mimeType = NULL;
	CBrowserWindow * win = NULL;
	FSSpec fileSpec;
	Boolean hasFileSpec = FALSE;
	OSErr err;
	Size	realSize;
	OSType	realType;
	
	// Get the file specs
	err = ::AEGetKeyPtr(&inAppleEvent, keyDirectObject, typeFSS,
						&realType, &fileSpec, sizeof(fileSpec), &realSize);
	ThrowIfOSErr_(err);
	
	// Get the window
	try
	{
		Int32 windowID;
		err = ::AEGetParamPtr(&inAppleEvent, AE_spy_openURL_wind, typeLongInteger,
						 &realType, &windowID, sizeof(windowID), &realSize);
		ThrowIfOSErr_(err);
		win = CBrowserWindow::GetWindowByID(windowID);
		ThrowIfNil_(win);
	}
	catch (...)
	{
		win = CBrowserWindow::FindAndShow();
		ThrowIfNil_(win);
	}
	
	// Get the url
	try
	{
		MoreExtractFromAEDesc::GetCString(inAppleEvent, AE_spy_showFile_url, url);
	}
	catch(...)
	{
	}
		
	// Get the mime type
	try
	{
		MoreExtractFromAEDesc::GetCString(inAppleEvent, AE_spy_showFile_mime, mimeType);
	}
	catch(...)
	{
	}
	
	CFrontApp::sApplication->OpenLocalURL(&fileSpec, win, mimeType);
}

/*----------------------------------------------------------------
	ParseAnchor AppleEvent
	Gets the URL, the relative url, and returns the full url of the relative

	arse anchor: Resolves the relative URL
	parse anchor  
		string  				-- Main URL
		relative to  string  	-- Relative URL
	Result:   
		string  				-- Parsed  URL
----------------------------------------------------------------*/
void CAppleEventHandler::HandleParseAnchor(const AppleEvent	
		&inAppleEvent, AppleEvent &outAEReply, AEDesc& /*outResult*/, long /*inAENumber*/)
{
	char * url = nil;
	char * relative = nil;
	char * parsed = nil;
	MoreExtractFromAEDesc::GetCString(inAppleEvent, keyDirectObject, url);
	MoreExtractFromAEDesc::GetCString(inAppleEvent, AE_spy_parse_rel, relative);
	parsed = NET_MakeAbsoluteURL(url, relative);
	ThrowIfNil_(parsed);
	
	OSErr err = ::AEPutParamPtr(&outAEReply, keyAEResult, typeChar, parsed, strlen(parsed));
	if (parsed)
		XP_FREE(parsed);
}

/*----------------------------------------------------------------
	HandleCancelProgress
	Find the context with this transaction ID, and cancel it
	In: Incoming apple event, reply event, outtgoing descriptor, eventID
	Out: OpenURLEvent handled
	
	cancel progress: 
		Interrupts the download of the document in the given window
		
	cancel progress  
		integer  				-- progress ID, obtained from the progress app
		[in window  integer] 	-- window ID of the progress to cancel
----------------------------------------------------------------*/
void CAppleEventHandler::HandleCancelProgress(const AppleEvent	&inAppleEvent,
									AppleEvent			& /* outAEReply */,
									AEDesc				& /* outResult */,
									long				/* inAENumber */)
{
	// Find the window with this transaction, and let it handle the event
	
	try
	{
		Int32 transactionID;
		StAEDescriptor	transDesc;
		Boolean hasTransactionID = FALSE;
		Boolean hasWindowID = FALSE;
		try
		{
			OSErr err = ::AEGetKeyDesc(&inAppleEvent, keyDirectObject, typeWildCard, &transDesc.mDesc);
			ThrowIfOSErr_(err);
			UExtractFromAEDesc::TheInt32(transDesc.mDesc, transactionID);
			hasTransactionID = TRUE;
		}
		catch(...)
		{
		}
		try
		{
			OSErr err = ::AEGetKeyDesc(&inAppleEvent, AE_spy_CancelProgress_win, typeWildCard, &transDesc.mDesc);
			ThrowIfOSErr_(err);
			UExtractFromAEDesc::TheInt32(transDesc.mDesc, transactionID);
			hasWindowID = TRUE;
		}
		catch(...)
		{
		}
		
		CBrowserWindow* window;
		if (!hasTransactionID && !hasWindowID)	// No arguments, interrupt frontmost by default
			{
			window = CBrowserWindow::FindAndShow();
			if (window && window->GetWindowContext())
				NET_InterruptWindow(*(window->GetWindowContext()));
			}
		else
			{
			CMediatedWindow* theWindow;
			CWindowIterator iter(WindowType_Browser);
			while (iter.Next(theWindow))
				{
				CBrowserWindow* theBrowserWindow = dynamic_cast<CBrowserWindow*>(theWindow);
				CNSContext* nsContext = theBrowserWindow->GetWindowContext();
				if (((nsContext->GetTransactionID() == transactionID) && hasTransactionID) 
					|| ((nsContext->GetContextUniqueID() == transactionID) && hasWindowID))
					NET_InterruptWindow(*nsContext);
				}
			}
	}
	catch(...)
	{
	}
}


/*----------------------------------------------------------------
	webActivate: Makes Netscape the frontmost application, and selects a given 
		window. This event is here for suite completeness/ cross-platform 
		compatibility only, you should use standard AppleEvents instead.
		
	webActivate  
		integer 			 -- window to bring to front
----------------------------------------------------------------*/
void CAppleEventHandler::HandleSpyActivate(const AppleEvent	&inAppleEvent, AppleEvent& /*outAEReply*/,
							AEDesc& /*outResult*/, long /*inAENumber*/)
{
	Size realSize;
	OSType realType;
	CBrowserWindow * win = NULL;
	OSErr err;
	
	MakeFrontApplication();
	// Get the window, and select it
	try
	{
		Int32 windowID;
		
		err = ::AEGetParamPtr(&inAppleEvent, AE_spy_openURL_wind, typeLongInteger,
						 &realType, &windowID, sizeof(windowID), &realSize);
		ThrowIfOSErr_(err);

		win = CBrowserWindow::GetWindowByID(windowID);
		if (win == nil)
			Throw_(errAENoSuchObject);
		UDesktop::SelectDeskWindow(win);
	}
	catch(...){}
}

/*----------------------------------------------------------------
	list windows: Lists the IDs of all the hypertext windows
	
	list windows
	Result:   
		list  		-- List of unique IDs of all the hypertext windows
----------------------------------------------------------------*/
void
CAppleEventHandler::HandleSpyListWindows(
	const AppleEvent&	inAppleEvent,
	AppleEvent&			outAEReply,
	AEDesc&				outResult,
	long				inAENumber)
{
#pragma unused(inAppleEvent, outResult, inAENumber) 

	AEDescList windowList;
	windowList.descriptorType = typeNull;
	OSErr err;
	
	try
	{
		// Create a descriptor list, and add a windowID for each browser window to the list

		CMediatedWindow* 	theWindow = NULL;
		DataIDT				windowType = WindowType_Browser;
		CWindowIterator		iter(windowType);
		
		err = ::AECreateList(nil, 0, FALSE, &windowList);
		ThrowIfOSErr_(err);

		while (iter.Next(theWindow))
		{
			CBrowserWindow* theBrowserWindow = dynamic_cast<CBrowserWindow*>(theWindow);
			ThrowIfNil_(theBrowserWindow);
			
			CBrowserContext* theBrowserContext = (CBrowserContext*)theBrowserWindow->GetWindowContext();
			ThrowIfNil_(theBrowserContext);
			
			StAEDescriptor	theIDDesc(theBrowserContext->GetContextUniqueID());
			err = ::AEPutDesc(&windowList, 0, &theIDDesc.mDesc);
			ThrowIfOSErr_(err);
		}
	}
	catch (...)
	{
		if (windowList.descriptorType != typeNull)
			::AEDisposeDesc(&windowList);
		throw;
	}

	err = ::AEPutParamDesc(&outAEReply, keyAEResult, &windowList);
	ThrowIfOSErr_(err);
	::AEDisposeDesc(&windowList);
}


/*----------------------------------------------------------------
	Gets the window information, a list containing URL and title
	## Another redundant event that has an equivalent event already 
	specified by Apple

	get window info: 
	Returns the information about the window as a list. 
	Currently the list contains the window title and the URL. 
	## You can get the same information using standard Apple Event GetProperty.
	
	get window info  
		integer  			-- window ID
	Result:   
		list				-- contains URL and title strings
----------------------------------------------------------------*/
void CAppleEventHandler::HandleSpyGetWindowInfo(const AppleEvent	&inAppleEvent, AppleEvent &outAEReply,
							AEDesc& /*outResult*/, long /*inAENumber*/)
{
	Size realSize;
	OSType realType;
	CBrowserWindow * win = NULL;			// Window
	OSErr err;
	AEDescList volatile propertyList;
	propertyList.descriptorType = typeNull;
	
	try
	{
			// Get the window, and list its properties
		Int32 windowID;
		
		err = ::AEGetParamPtr(&inAppleEvent, keyDirectObject, typeLongInteger,
						 &realType, &windowID, sizeof(windowID), &realSize);
		ThrowIfOSErr_(err);
		win = CBrowserWindow::GetWindowByID(windowID);
		if (win == nil)
			Throw_(errAENoSuchObject);
		
			// Create the list
		err = ::AECreateList(nil, 0, FALSE, &propertyList);
		ThrowIfOSErr_(err);
		
			// Put the URL as the first item in the list
		cstring url = win->GetWindowContext()->GetCurrentURL();
		err = ::AEPutPtr(&propertyList, 1, typeChar, (char*)url, url.length());
		ThrowIfOSErr_(err);
		
			// Put the title as the second item in the list
		CStr255 ptitle;
		GetWTitle(win->GetMacPort(), ptitle);
		cstring ctitle((char*)ptitle);
		err = ::AEPutPtr(&propertyList, 2, typeChar, (char*)ctitle, ctitle.length());
		ThrowIfOSErr_(err);
		
			// Put the list as the reply
		err = ::AEPutParamDesc(&outAEReply, keyAEResult, &propertyList);
		::AEDisposeDesc(&propertyList);
	}
	catch(...)
	{
		if (propertyList.descriptorType != typeNull)
			::AEDisposeDesc(&propertyList);
		throw;		
	}
}

/*----------------------------------------------------------------
	register window close: 
	Netscape will notify registered application when this window closes
	
	register window close  
		'sign'  		-- Application signature for window  
		integer 		 -- window ID
		
	[Result:   boolean]  -- true if successful

	unregister window close: 
	Undo for register window close unregister window close  
	
		'sign'  		-- Application signature for window  
		integer 		 -- window ID
		
	[Result:   boolean]  -- true if successful
----------------------------------------------------------------*/
void CAppleEventHandler::HandleWindowRegistration(const AppleEvent	&inAppleEvent, AppleEvent &outAEReply,
							AEDesc &outResult, long inAENumber)
{
	Size realSize;
	OSType realType;
	CBrowserWindow * win = NULL;
	OSErr err;

	// Get the window, and select it
	Int32 windowID;
	
	err = ::AEGetParamPtr(&inAppleEvent, AE_spy_registerWinClose_win, typeLongInteger,
					 &realType, &windowID, sizeof(windowID), &realSize);
	ThrowIfOSErr_(err);
	
	win = CBrowserWindow::GetWindowByID(windowID);
	if (win == nil)
		Throw_(errAENoSuchObject);
	win->HandleAppleEvent(inAppleEvent, outAEReply, outResult, inAENumber);
}



/*--------------------------------------------------------------------------*/
/*				----   Experimental Netscape Suite    ----					*/
/*--------------------------------------------------------------------------*/					

/*----------------------------------------------------------------
	Open bookmark: Reads in a bookmark file
	
	Open bookmark  
		alias  -- If not available, reloads the current bookmark file
----------------------------------------------------------------*/
void CAppleEventHandler::HandleOpenBookmarksEvent( const AppleEvent	&inAppleEvent,
									AppleEvent			& /*outAEReply*/,
									AEDesc				& /*outResult*/,
									long				/*inAENumber*/)
{
	FSSpec fileSpec;
	Boolean hasFileSpec = FALSE;
	OSErr err;
	Size	realSize;
	OSType	realType;

	char * path = NULL;
	
	// Get the file specs
	err = ::AEGetKeyPtr(&inAppleEvent, keyDirectObject, typeFSS,
							&realType, &fileSpec, sizeof(fileSpec), &realSize);

	// If none, use the default file
	//		I hate this glue to uapp.  Will change as soon as all AE stuff
	//		is working.
	if (err != noErr)
		path = GetBookmarksPath( fileSpec, TRUE );
	else
		path = GetBookmarksPath( fileSpec, FALSE );
	
	ThrowIfNil_(path);
	
	DebugStr("\pNot implemented");
//¥¥¥	CBookmarksContext::Initialize();
//¥¥¥	BM_ReadBookmarksFromDisk( *CBookmarksContext::GetInstance(), path, NULL );
}

/*----------------------------------------------------------------
	Open AddressBook: Opens the address book
	
	Open AddressBook  
----------------------------------------------------------------*/
void CAppleEventHandler::HandleOpenAddressBookEvent( 
									const AppleEvent	&/*inAppleEvent*/,
									AppleEvent			& /*outAEReply*/,
									AEDesc				& /*outResult*/,
									long				/*inAENumber*/)
{
#ifdef MOZ_MAIL_NEWS
	CAddressBookManager::ShowAddressBookWindow();
#endif // MOZ_MAIL_NEWS
}


/*----------------------------------------------------------------
	Open Component: Opens the component specified in the parameter
	to the Apple Event.
	
	HandleOpenComponentEvent
----------------------------------------------------------------*/
void CAppleEventHandler::HandleOpenComponentEvent( 
									const AppleEvent	&inAppleEvent,
									AppleEvent			& /*outAEReply*/,
									AEDesc				& /*outResult*/,
									long				/*inAENumber*/)
{	
	OSType		componentType;
	DescType	returnedType;
	Size		gotSize = 0;
	
	OSErr err = ::AEGetKeyPtr(&inAppleEvent, keyDirectObject, typeEnumerated, &returnedType, &componentType,
			sizeof(OSType), &gotSize);

	if (err == noErr && gotSize == sizeof(OSType))
	{					  
		switch (componentType)
		{
			case AE_www_comp_navigator:
				CFrontApp::sApplication->ObeyCommand(cmd_BrowserWindow);
				break;

#ifdef MOZ_MAIL_NEWS
			case AE_www_comp_inbox:
				CFrontApp::sApplication->ObeyCommand(cmd_Inbox);
				break;
						
			case AE_www_comp_collabra:
				CFrontApp::sApplication->ObeyCommand(cmd_MailNewsFolderWindow);
				break;
#endif // MOZ_MAIL_NEWS
#ifdef EDITOR
			case AE_www_comp_composer:
				CFrontApp::sApplication->ObeyCommand(cmd_NewWindowEditor);
				break;
#endif // EDITOR
#if !defined(MOZ_LITE) && !defined(MOZ_MEDIUM)
			case AE_www_comp_conference:
				CFrontApp::sApplication->ObeyCommand(cmd_LaunchConference);
				break;
				
			case AE_www_comp_calendar:
				CFrontApp::sApplication->ObeyCommand(cmd_LaunchCalendar);
				break;
				
			case AE_www_comp_ibmHostOnDemand:
				CFrontApp::sApplication->ObeyCommand(cmd_Launch3270);
				break;
#endif
#ifdef MOZ_NETCAST
			case AE_www_comp_netcaster:
				CFrontApp::sApplication->ObeyCommand(cmd_LaunchNetcaster);
				break;
#endif // MOZ_NETCAST
			default:
				; //do nothing
		}
	}
}


/*----------------------------------------------------------------
	HandleCommandEvent
	
	Get new mail
----------------------------------------------------------------*/
void CAppleEventHandler::HandleCommandEvent( 
									const AppleEvent	& inAppleEvent,
									AppleEvent			& /*outAEReply*/,
									AEDesc				& /*outResult*/,
									long				/*inAENumber*/)
{	

	long		commandID;
	DescType	returnedType;
	Size		gotSize = 0;
	
	OSErr err = ::AEGetKeyPtr(&inAppleEvent, keyDirectObject, typeEnumerated, &returnedType,
			&commandID, sizeof(long), &gotSize);

	if (err == noErr && gotSize == sizeof(long))
	{					  
// Is this really only for mail????
#ifdef MOZ_MAIL_NEWS
	CFrontApp::sApplication->ObeyCommand(commandID);
#endif // MOZ_MAIL_NEWS


	}
}


/*----------------------------------------------------------------
	Read help file: 
	Reads in the help file (file should be in the help file format)
	## Questions about the new help file system, ask montulli
		
	Read help file  
		alias
		[with index  string]  	-- Index to the help file. Defaults to  ÔDEFAULTÕ)
		[search text  string]  	-- Optional text to search for
----------------------------------------------------------------*/
void CAppleEventHandler::HandleReadHelpFileEvent(const AppleEvent	&inAppleEvent,
									AppleEvent			& /*outAEReply*/,
									AEDesc				& /*outResult*/,
									long				/*inAENumber*/)
{
	FSSpec fileSpec;
	char * map_file_url = NULL;
	char * help_id = NULL;
	char * search_text = NULL;
	OSErr err;
	Size	realSize;
	OSType	realType;

	CFrontApp::sApplication->ProperStartup(NULL, FILE_TYPE_NONE);

	try
	{
		// Get the file specs
		err = ::AEGetKeyPtr(&inAppleEvent, keyDirectObject, typeFSS,
								&realType, &fileSpec, sizeof(fileSpec), &realSize);
		ThrowIfOSErr_(err);
		
		map_file_url = CFileMgr::GetURLFromFileSpec( fileSpec );
		ThrowIfNil_(map_file_url);

		// Get the help text
		
		try
		{
			MoreExtractFromAEDesc::GetCString(inAppleEvent, AE_www_ReadHelpFileID, help_id);
		}
		catch(...)
		{
		}
			
		// Get the search text (optional)
		try
		{
			MoreExtractFromAEDesc::GetCString(inAppleEvent, AE_www_ReadHelpFileSearchText, search_text);
		}
		catch(...)
		{}
	}
	catch(...)
	{
		throw;
	}
	
		// Will using the bookmark context as a dummy work?
	DebugStr("\pNot implemented");
//¥¥¥	CBookmarksContext::Initialize();
//¥¥¥	NET_GetHTMLHelpFileFromMapFile(*CBookmarksContext::GetInstance(),
//							  map_file_url, help_id, search_text);
	
	FREEIF(map_file_url);
	FREEIF(help_id);
	FREEIF(search_text);
}

/*----------------------------------------------------------------
	Go: navigate a window: back, forward, again(reload), home)
	Go  
		reference  		-- window
		direction  		again/home/backward/forward
----------------------------------------------------------------*/
void CAppleEventHandler::HandleGoEvent( 
	const AppleEvent	&inAppleEvent,
	AppleEvent			&outAEReply,
	AEDesc				&outResult,
	long				inAENumber)
{
	// We need to make sure
	
	CFrontApp::sApplication->ProperStartup( NULL, FILE_TYPE_GETURL );
	CBrowserWindow * win = NULL;

	// Get the window.  If it is a Browser window then
	//	decode the AE and send a message.
	
	// The keyData.mDesc should be a ref to a browser window 
	// If we get it successfulle (ie the user included the "in window 1" 
	// then we can get a hold of the actual BrowserWindow object by using
	// AEResolve and GetModelFromToken somehow
	StAEDescriptor	keyData;
	OSErr err = ::AEGetKeyDesc(&inAppleEvent, AE_www_typeWindow, typeWildCard, &keyData.mDesc);
	if ((err == noErr) && (keyData.mDesc.descriptorType == typeObjectSpecifier))
	{
		StAEDescriptor	theToken;
		err = ::AEResolve(&keyData.mDesc, kAEIDoMinimum, &theToken.mDesc);
		ThrowIfOSErr_(err);
		LModelObject * model = CFrontApp::sApplication->GetModelFromToken(theToken.mDesc);
		ThrowIfNil_(model);
		model->HandleAppleEvent(inAppleEvent, outAEReply, outResult, inAENumber);

	// else the user did not provide an inside reference "inside window 1"
	// so send back an error	
	}else{
		
	}
}

/*----------------------------------------------------------------
	Get workingURL: 
		Get the path to the running application in URL format.  
		This will allow a script to construct a relative URL
		
	Get workingURL
	Result:   
		'tTXT'  -- Will return text of the from ÒFILE://foo/applicationnameÓ
----------------------------------------------------------------*/
void CAppleEventHandler::HandleGetWDEvent( 
	const AppleEvent	& /*inAppleEvent */,
	AppleEvent			& outAEReply,
	AEDesc				& /*outResult */,
	long				/*inAENumber */)
{
	char		*pathBlock = NULL;
	Size		pathLength;
	OSErr		err;

	// Grab the URL of the running application
	pathBlock = PathURLFromProcessSignature (emSignature, 'APPL');
	
	if (outAEReply.dataHandle != NULL && pathBlock != NULL)
	{
		pathLength = XP_STRLEN(pathBlock);
		
		err = AEPutParamPtr( &outAEReply, 
			keyDirectObject, 
			typeChar, 
			pathBlock, pathLength);
	}
	
	if (pathBlock)
		XP_FREE(pathBlock);
	
}


/*----------------------------------------------------------------
	HandleGetActiveProfileEvent: 
		Get the name of the active profile from the prefs.
	
	Result:   
		'tTXT'  -- Will return text of the profile name
----------------------------------------------------------------*/
void CAppleEventHandler::HandleGetActiveProfileEvent( 
	const AppleEvent	& /*inAppleEvent */,
	AppleEvent			&outAEReply,
	AEDesc				& /*outResult */,
	long				/*inAENumber */)
{
	char 	profileName[255];
	int 	len = 255;
	OSErr	err = noErr;
	
	err =  PREF_GetCharPref("profile.name", profileName, &len);
	
	if (err == PREF_NOERROR && outAEReply.dataHandle != nil) {
	
			err = AEPutParamPtr( &outAEReply, 
				keyDirectObject, 
				typeChar, 
				profileName, strlen(profileName));
	}
}

//----------------------------------------------------------------------------------------
void CAppleEventHandler::HandleGetProfileImportDataEvent( 
	const AppleEvent	& /*inAppleEvent */,
	AppleEvent			&outAEReply,
	AEDesc				& /*outResult */,
	long				/*inAENumber */)
//
//		Returns useful data for the external import module.
//	
//	Result:   
//		'tTXT'  -- (Though binary data - see ProfileImportData struct below)
//----------------------------------------------------------------------------------------
{
#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=mac68k
#else
#error "There'll be a big bug here"
#endif
	struct ProfileImportData
	{
		Int16		profileVRefNum;
		long		profileDirID;
		Int16		mailFolderVRefNum;
		long		mailFolderDirID;
		DataIDT		frontWindowKind;
	} data;
#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=reset
#endif
	Assert_(16==sizeof(ProfileImportData));
	FSSpec spec = CPrefs::GetFilePrototype(CPrefs::MainFolder);
	data.profileVRefNum = spec.vRefNum;
	data.profileDirID = spec.parID;
	spec = CPrefs::GetFilePrototype(CPrefs::MailFolder);
	data.mailFolderVRefNum = spec.vRefNum;
	data.mailFolderDirID = spec.parID;
	CWindowIterator iter(WindowType_Any, false);
	CMediatedWindow* frontWindow = nil;
	data.frontWindowKind = iter.Next(frontWindow) ? frontWindow->GetWindowType() : 0;
	/* err =*/ AEPutParamPtr( &outAEReply, 
		keyDirectObject, 
		typeChar, 
		&data, sizeof(data));
} // CAppleEventHandler::HandleGetProfileImportDataEvent

/*--------------------------------------------------------------------------*/
/*				----   Apple Event Object Model support   ----				*/
/*--------------------------------------------------------------------------*/	

void CAppleEventHandler::GetSubModelByUniqueID(DescType inModelID, const AEDesc	&inKeyData, AEDesc &outToken) const
{
	switch (inModelID)
	{
	case cWindow:		// The hyperwindows have unique IDs that can be resolved
						// FFFFFFFFF is the front window, 0 is a new window
		Int32 windowID;
		UExtractFromAEDesc::TheInt32(inKeyData, windowID);
		LWindow*	foundWindow = NULL;
		
		foundWindow = CBrowserWindow::GetWindowByID(windowID);
		if (foundWindow == NULL)
			ThrowOSErr_(errAENoSuchObject);
		else
			CFrontApp::sApplication->PutInToken(foundWindow, outToken);	
		break;
	default:
		CFrontApp::sApplication->LDocApplication::GetSubModelByUniqueID(inModelID, inKeyData, outToken);
		break;
	}
}

void CAppleEventHandler::GetAEProperty(DescType inProperty,
							const AEDesc	&inRequestedType,
							AEDesc			&outPropertyDesc) const
{
	OSErr err;
	
	switch (inProperty)
	{
		case AE_www_typeApplicationAlert:	// application that handles alerts
			err = ::AECreateDesc(typeType, &ErrorManager::sAlertApp,
								sizeof(ErrorManager::sAlertApp), &outPropertyDesc);
			ThrowIfOSErr_(err);
			break;
			
		case AE_www_typeKioskMode:
			err = ::AECreateDesc(typeLongInteger, (const void *)fKioskMode,
								sizeof(fKioskMode), &outPropertyDesc);
			break;
			
		default:
			CFrontApp::sApplication->LApplication::GetAEProperty(inProperty, inRequestedType, outPropertyDesc);
			break;
	}
}

void CAppleEventHandler::SetAEProperty(DescType inProperty,
							const AEDesc	&inValue,
							AEDesc			&outPropertyDesc)
{

	switch (inProperty) {
	case AE_www_typeApplicationAlert:	// application that handles alerts
		try
		{
			OSType newSig;
			UExtractFromAEDesc::TheType(inValue, newSig);
			ErrorManager::sAlertApp = newSig;
		}
		catch(...)	// In case of error, revert to self
		{
			ErrorManager::sAlertApp = emSignature;
		}
		break;
	case AE_www_typeKioskMode:
		try
		{
			Boolean menuBarModeChangeBroadcasted = false;
			Int32 kMode;
			UExtractFromAEDesc::TheInt32(inValue, kMode);
			
			if ((kMode == KioskOn) && (fKioskMode != KioskOn))
			{
				fKioskMode = KioskOn;
				// SetMenubar(KIOSK_MENUBAR);		¥¥¥ this is currently handled by the chrome structure, below
				//										BUT, they want three states for menu bars, and the field is only a boolean
								
				CMediatedWindow*	theIterWindow = NULL;
				DataIDT				windowType = WindowType_Browser;
				CWindowIterator		theWindowIterator(windowType);
				
				while (theWindowIterator.Next(theIterWindow))
				{
					CBrowserWindow* theBrowserWindow = dynamic_cast<CBrowserWindow*>(theIterWindow);
					if (theBrowserWindow != nil)
					{
						Chrome			aChrome;
						theBrowserWindow->GetChromeInfo(&aChrome);
						aChrome.show_button_bar = 0;
						aChrome.show_url_bar = 0;
						aChrome.show_directory_buttons = 0;
						aChrome.show_security_bar = 0;
						aChrome.show_menu = 0;		// ¥¥¥ this isn't designed correctly!	deeje 97-03-13
						
						// Make sure we only broadcast the menubar mode change once!
						theBrowserWindow->SetChromeInfo(&aChrome, !menuBarModeChangeBroadcasted);
						if (!menuBarModeChangeBroadcasted)
						{
							menuBarModeChangeBroadcasted = true;
						}
					}
				}
			}
			else if ((kMode == KioskOff) && (fKioskMode != KioskOff))
			{
				fKioskMode = KioskOff;
				// SetMenubar(MAIN_MENUBAR);		¥¥¥ this is currently handled by the chrome structure, below
				//										BUT, they want three states for menu bars, and the field is only a boolean
								
				CMediatedWindow*	theIterWindow = NULL;
				DataIDT				windowType = WindowType_Browser;
				CWindowIterator		theWindowIterator(windowType);
				
				while (theWindowIterator.Next(theIterWindow))
				{
					CBrowserWindow* theBrowserWindow = dynamic_cast<CBrowserWindow*>(theIterWindow);
					if (theBrowserWindow != nil)
					{
						Chrome			aChrome;
						theBrowserWindow->GetChromeInfo(&aChrome);
						aChrome.show_button_bar = CPrefs::GetBoolean(CPrefs::ShowToolbar);
						aChrome.show_url_bar = CPrefs::GetBoolean(CPrefs::ShowURL);
						aChrome.show_directory_buttons = CPrefs::GetBoolean(CPrefs::ShowDirectory);
						aChrome.show_security_bar = CPrefs::GetBoolean(CPrefs::ShowToolbar) || CPrefs::GetBoolean(CPrefs::ShowURL);
						aChrome.show_menu = 1;		// ¥¥¥ this isn't designed correctly!	deeje 97-03-13
						
						// Make sure we only broadcast the menubar mode change once!
						theBrowserWindow->SetChromeInfo(&aChrome, !menuBarModeChangeBroadcasted);
						if (!menuBarModeChangeBroadcasted)
						{
							menuBarModeChangeBroadcasted = true;
						}
					}
				}
			}
			
		}
		catch(...)
		{}
		break;
	default:
		CFrontApp::sApplication->LApplication::SetAEProperty(inProperty, inValue, outPropertyDesc);
		break;
	}
	
	
}

/*--------------------------------------------------------------------------*/
/*			----   Eudora (send) Mail Suite   ----							*/
/*--------------------------------------------------------------------------*/	


/*-------------------------------------------------------------*/
//	 class EudoraSuite		This supports sending to Eudora
//	Tools used to communicate with Eudora
//	The only real use these have is if we are operating in
//	Browser-only mode and the user wishes to use Eudora to
//	handle mail functions.
//
/*-------------------------------------------------------------*/


// --------------------------------------------------------------
/*	This makes a Null AppleEvent descriptor.    
			 */
// --------------------------------------------------------------
void EudoraSuite::MakeNullDesc (AEDesc *theDesc)
{
	theDesc->descriptorType = typeNull;
	theDesc->dataHandle = nil;
}

// --------------------------------------------------------------
/*	This makes a string AppleEvent descriptor.   
	In: A pascal string
		Pointer to an AEDesc.
		
	Out: AEDesc of type TEXT created and returned. */
// --------------------------------------------------------------
OSErr EudoraSuite::MakeStringDesc (Str255 theStr,AEDesc *theDesc)
{
	return AECreateDesc(kAETextSuite, &theStr[1], StrLength(theStr), theDesc);
}

// --------------------------------------------------------------
/*	This stuffs the required parameters into the AppleEvent. 
			 */
// --------------------------------------------------------------


OSErr EudoraSuite::CreateObjSpecifier (AEKeyword theClass,AEDesc theContainer,
		AEKeyword theForm,AEDesc theData, Boolean /*disposeInputs*/,AEDesc *theSpec)
{
	AEDesc theRec;
	OSErr	err;
	
	err = AECreateList(nil,0,true,&theRec);
	if (!err)
		err = AEPutKeyPtr(&theRec,keyAEKeyForm,typeEnumeration,&theForm,sizeof(theForm));
	if (!err)
		err = AEPutKeyPtr(&theRec,keyAEDesiredClass,cType,&theClass,sizeof(theClass));
	if (!err)
		err = AEPutKeyDesc(&theRec,keyAEKeyData,&theData);
	if (!err)
		err = AEPutKeyDesc(&theRec,keyAEContainer,&theContainer);
	if (!err)
		err = AECoerceDesc(&theRec,cObjectSpecifier,theSpec);
	AEDisposeDesc(&theRec);
	return err;
}

// --------------------------------------------------------------
/*	This creates an AEDesc for the current message.
	(The current message index = 1)     

	In:	Pointer to AEDesc to return
	Out: AEDesc constructed. */
// --------------------------------------------------------------

OSErr EudoraSuite::MakeCurrentMsgSpec (AEDesc *theSpec)
{
	AEDesc theContainer,theIndex;
	OSErr	err;
	long	msgIndex = 1;

	EudoraSuite::MakeNullDesc (&theContainer);
	err = AECreateDesc(cLongInteger, &msgIndex, sizeof(msgIndex), &theIndex);
	if (!err)
		err = EudoraSuite::CreateObjSpecifier(cEuMessage, theContainer,
				formAbsolutePosition, theIndex, true, theSpec);
				
	AEDisposeDesc(&theContainer);
	AEDisposeDesc(&theIndex);
	
	return err;
}


// --------------------------------------------------------------
/*	Send a given Apple Event.  Special case for Eudora, should
	be rewritten, but it works for the moment.
	In:	AppleEvent
	Out: Event sent  */
// --------------------------------------------------------------
OSErr EudoraSuite::SendEvent (AppleEvent *theEvent)
{
	AppleEvent theReply;
	OSErr err;
	
	EudoraSuite::MakeNullDesc(&theReply);
	err = AESend(theEvent,&theReply,kAENoReply + kAENeverInteract 
			+ kAECanSwitchLayer+kAEDontRecord,kAENormalPriority,-1,nil,nil);

	AEDisposeDesc(&theReply);
	AEDisposeDesc(theEvent);

	return err;
}

// --------------------------------------------------------------
/*	Create an Apple Event to be sent to Eudora
	In:	Event Class
		Event ID
		Ptr to Apple Event
	Out: Event constructed and returned.  */
// --------------------------------------------------------------
OSErr EudoraSuite::MakeEvent (AEEventClass eventClass,AEEventID eventID,AppleEvent *theEvent)
{
	AEAddressDesc theTarget;
	OSType theSignature;
	OSErr	err;
	
	theSignature = kEudoraSuite;
	err = AECreateDesc(typeApplSignature,&theSignature,sizeof(theSignature),&theTarget);
	if (!err)
		err = AECreateAppleEvent(eventClass,eventID,&theTarget,0,0,theEvent);
	AEDisposeDesc(&theTarget);
	return err;
}

// --------------------------------------------------------------
/*	This sets the data in a specified field. It operates on the frontmost message 
	in Eudora. It is the equivalent of sending the following AppleScript:
	set field "fieldname" of message 0 to "data"

	Examples for setting up a complete mail message:
		EudoraSuite::SendSetData("\pto",toRecipientPtr);
		EudoraSuite::SendSetData("\pcc",ccRecipientPtr);
		EudoraSuite::SendSetData("\pbcc",bccRecipientPtr);
		EudoraSuite::SendSetData("\psubject",subjectPtr);
		EudoraSuite::SendSetData("\p",bodyPtr);
		
	In:	Field to set the data in (Subject, Address, Content, etc)
		Pointer to text data.
		Size of pointer (allows us to work with XP_Ptrs).
	Out: Apple Event sent to Eudora, setting a given field.  */
// --------------------------------------------------------------
OSErr EudoraSuite::SendSetData(Str31 theFieldName, Ptr thePtr, long thePtrSize)
{
	AEDesc theMsg,theName,theFieldSpec,theText;
	AppleEvent theEvent;
	OSErr err;
	Handle theData;
	
	theData = NewHandle(thePtrSize);
	BlockMove((Ptr)thePtr,*theData,thePtrSize);

	if (theData != nil)
		{

			err = EudoraSuite::MakeCurrentMsgSpec(&theMsg);
			if (!err)
				err = EudoraSuite::MakeStringDesc(theFieldName,&theName);
			if (!err)
				err = EudoraSuite::CreateObjSpecifier(cEuField,theMsg,formName,theName,true,&theFieldSpec);
			if (!err)
				err = EudoraSuite::MakeEvent(kAECoreSuite,kAESetData,&theEvent);
			if (!err)
				err = AEPutParamDesc(&theEvent,keyAEResult,&theFieldSpec);
			AEDisposeDesc(&theFieldSpec);
				
			theText.descriptorType = typeChar;
			theText.dataHandle = theData;
			if (!err)
				err = AEPutParamDesc(&theEvent,keyAEData,&theText);
			if (!err)
				err = EudoraSuite::SendEvent(&theEvent);

			DisposeHandle(theText.dataHandle);
			AEDisposeDesc(&theText);
			AEDisposeDesc(&theMsg);
			AEDisposeDesc(&theName);
		}
	DisposeHandle(theData);
	return err;
	
}
// --------------------------------------------------------------
/*	Everything you need to tell Eudora to construct a new message
	and send it.
	In:	Pointer to the list of e mail addresses to send TO
		Pointer to the list of e mail addresses to send CC
		Pointer to the list of e mail addresses to send BCC
		Pointer to the Subject text
		Priority level of message.
		Pointer to the contents of the mail
		Pointer to an FSSpec (or null if none) for an enclosure.
	Out: Apple Events sent to Eudora telling it to construct the
		message and send it. */
// --------------------------------------------------------------

OSErr  EudoraSuite::SendMessage(
	Ptr		toRecipientPtr, 
	Ptr		ccRecipientPtr,
	Ptr		bccRecipientPtr,
	Ptr		subjectPtr,
	XP_HUGE_CHAR_PTR		bodyPtr,
	long	thePriority,
	FSSpec	*theEnclosurePtr)
{	
	AEDesc nullSpec,theName,theFolder,theMailbox,theInsertRec,theInsl,msgSpec,theEnclList;
	OSType thePos,theClass;
	AppleEvent theEvent;
	OSErr 	err;
	
		
/* This section creates a new message and places it at the end of the out mailbox.
   It is equivalent to  the following AppleScript:
		make message at end of mailbox "out" of mail folder ""
*/


	MakeNullDesc(&nullSpec);
	err = MakeStringDesc("\p",&theName);
	if (!err)
		err = EudoraSuite::CreateObjSpecifier(cEuMailfolder,nullSpec,formName,theName,true,&theFolder);


	if (!err)
		err = MakeStringDesc("\pout",&theName);
	if (!err)
		err = EudoraSuite::CreateObjSpecifier(cEuMailbox,theFolder,formName,theName,true,&theMailbox);
	if (!err)
		err = AECreateList(nil,0,true,&theInsertRec);
	if (!err)
		err = AEPutKeyDesc(&theInsertRec,keyAEObject,&theMailbox);


	thePos=kAEEnd;
	if (!err)
		err = AEPutKeyPtr(&theInsertRec,keyAEPosition,typeEnumeration,&thePos,sizeof(thePos));

	if (!err)
		err = AECoerceDesc(&theInsertRec,typeInsertionLoc,&theInsl);

	if (!err)
		err = EudoraSuite::MakeEvent(kAECoreSuite,kAECreateElement,&theEvent);
	if (!err)
		err = AEPutParamDesc(&theEvent,keyAEInsertHere,&theInsl);

	AEDisposeDesc(&nullSpec);
	AEDisposeDesc(&theName);
	AEDisposeDesc(&theFolder);
	AEDisposeDesc(&theMailbox);
	AEDisposeDesc(&theInsertRec);
	AEDisposeDesc(&theInsl);

	theClass=cEuMessage;
	if (!err)
		err = AEPutParamPtr(&theEvent,keyAEObjectClass,cType,&theClass,sizeof(theClass));
	if (!err)
		err = EudoraSuite::SendEvent(&theEvent);


/* This section fills in various fields.
   It is equivalent to  the following AppleScript:
		set field "to" of message 0 to "data"
*/

	if (!err) {
		if ( toRecipientPtr )
		err = SendSetData("\pto",toRecipientPtr, GetPtrSize(toRecipientPtr));
	}
	
	if (!err) {
		if ( ccRecipientPtr )
		err = SendSetData("\pcc",ccRecipientPtr, GetPtrSize(ccRecipientPtr));
	}
	if (!err) {
		if ( bccRecipientPtr ) 
		err = SendSetData("\pbcc",bccRecipientPtr, GetPtrSize(bccRecipientPtr));
	}
	
	if (!err)
		err = SendSetData("\psubject",subjectPtr, GetPtrSize(subjectPtr));
	if (!err)
		err = SendSetData("\p",bodyPtr, XP_STRLEN(bodyPtr) );

/* This sets the priority of the message. See the constants defined above for the legal
   values.
*/
	
	err = Set_Eudora_Priority(thePriority);		
		

/* This attaches a file to the Eudora message provided it is a proper FSSpec. */
	if (StrLength(theEnclosurePtr->name)>0)
		{
			if (!err)
				err = MakeCurrentMsgSpec(&msgSpec);
			if (!err)
				err = EudoraSuite::MakeEvent(kEudoraSuite,kEuAttach,&theEvent);
			if (!err)
				err = AEPutParamDesc(&theEvent,keyAEResult,&msgSpec);
				if (!err)
					err = AECreateList(nil,0,false,&theEnclList);
				if (!err)
					err = AEPutPtr(&theEnclList,0,typeFSS,&theEnclosurePtr->name,
						sizeof(theEnclosurePtr->name) );
				if (!err)
					err = AEPutParamDesc(&theEvent,keyEuDocumentList,&theEnclList);
				if (!err)
					err = EudoraSuite::SendEvent(&theEvent);
				AEDisposeDesc(&msgSpec);
				AEDisposeDesc(&theEnclList);
			}


/* This tells Eudora to queue the current message. */
	if (!err)
		err = EudoraSuite::MakeCurrentMsgSpec(&msgSpec);
	if (!err)
		err = EudoraSuite::MakeEvent(kEudoraSuite,kEuQueue,&theEvent);
	if (!err)
		err = AEPutParamDesc(&theEvent,keyAEResult,&msgSpec);
	AEDisposeDesc(&msgSpec);
	if (!err)
		err = EudoraSuite::SendEvent(&theEvent);
		
	return err;

}

// --------------------------------------------------------------
/*	Given the priority of a message, this sets the priority in the message.
	This same type of procedure can be used for many of the AppleScript commands in
	the form of:     set <item> of message 0 to <data>
	
	*/
// --------------------------------------------------------------

OSErr EudoraSuite::Set_Eudora_Priority(long thePriority)
{
  AEDesc theMsg,theData,thePropSpec,thePriorityDesc;
  AppleEvent theEvent;
  OSErr theErr;
  AEKeyword theProperty;
  Handle h;
 
  theErr = MakeCurrentMsgSpec(&theMsg);
 
  theProperty = pEuPriority;
  AECreateDesc(typeType,&theProperty,sizeof (theProperty),&thePriorityDesc);
 
  if (!theErr)
 	theErr = CreateObjSpecifier(typeProperty,theMsg,typeProperty,thePriorityDesc,true,&thePropSpec);
  if (!theErr)
    theErr = MakeEvent(kAECoreSuite,kAESetData,&theEvent);
 
  if (!theErr)
    theErr = AEPutKeyDesc(&theEvent, keyDirectObject, &thePropSpec);
  if (!theErr)
    theErr = AEDisposeDesc(&thePropSpec);
 
  h = NewHandle (sizeof(thePriority));
  BlockMove ((Ptr)&thePriority,*h,sizeof(thePriority));
  theData.descriptorType = typeInteger;
  theData.dataHandle = h;
  if (!theErr)
    theErr=AEPutParamDesc(&theEvent,keyAEData,&theData);
  if (!theErr)
    theErr = SendEvent(&theEvent);
	DisposeHandle(h);
    AEDisposeDesc(&theMsg);
    AEDisposeDesc(&theData);
    AEDisposeDesc(&thePriorityDesc);

   return theErr;
 
}



/*-------------------------------------------------------------*/
//	 class MoreExtractFromAEDesc
//	Apple event helpers -- extension of UExtractFromAEDesc.h
//	All the miscelaneous AppleEvent helper routines.
/*-------------------------------------------------------------*/


// --------------------------------------------------------------
/*	Given an AppleEvent, locate a string given a keyword and
	return the string
	In:	Event to search
		AEKeyword assocaated with the string
		C string ptr
	Out: Pointer to a newly created C string returned */
// --------------------------------------------------------------
void MoreExtractFromAEDesc::GetCString(const AppleEvent	&inAppleEvent, 
						AEKeyword keyword, char * & s, Boolean inThrowIfError)
{
	StAEDescriptor desc;
	OSErr err = ::AEGetParamDesc(&inAppleEvent,keyword,typeWildCard,&desc.mDesc);
	
	if (err) {
		if (inThrowIfError)
			ThrowIfOSErr_(err);
		else
			return;
	}
	TheCString(desc, s);
}

// --------------------------------------------------------------
/*	Given an AEDesc of type typeChar, return its string.
	In:	AEDesc containing a string
		C string ptr
	Out: Pointer to a newly created C string returned */
// --------------------------------------------------------------
void MoreExtractFromAEDesc::TheCString(const AEDesc &inDesc, char * & outPtr)
{
	outPtr = nil;
	Handle	dataH;
	AEDesc	coerceDesc = {typeNull, nil};
	if (inDesc.descriptorType == typeChar) {
		dataH = inDesc.dataHandle;		// Descriptor is the type we want
	
	} else {							// Try to coerce to the desired type
		if (AECoerceDesc(&inDesc, typeChar, &coerceDesc) == noErr) {
										// Coercion succeeded
			dataH = coerceDesc.dataHandle;

		} else {						// Coercion failed
			ThrowOSErr_(errAETypeError);
		}
	}
	
	Int32	strLength = GetHandleSize(dataH);
	outPtr = (char *)XP_ALLOC(strLength+1);	// +1 for NULL ending
	ThrowIfNil_( outPtr );
	// Terminate the string
	BlockMoveData(*dataH, outPtr, strLength);
	outPtr[strLength] = 0;
	
	if (coerceDesc.dataHandle != nil) {
		AEDisposeDesc(&coerceDesc);
	}
}

// --------------------------------------------------------------
/*	Add an error string and error code to an AppleEvent.
	Typically used when constructing the return event when an
	error occured
	In:	Apple Event to append to
		Error string
		Error code
	Out: keyErrorNum and keyErrorSting AEDescs are added to the Event. */
// --------------------------------------------------------------
void MoreExtractFromAEDesc::MakeErrorReturn(AppleEvent &event,const 
		CStr255& errorString, OSErr errorCode)
{
	StAEDescriptor	errorNum(errorCode);
	StAEDescriptor	errorText((ConstStringPtr)errorString);
	// We can ignore the errors. If error occurs, it only means that the reply is not handled
	OSErr err = ::AEPutParamDesc(&event, keyErrorNumber, &errorNum.mDesc);
	err = ::AEPutParamDesc(&event, keyErrorString, &errorText.mDesc);	
}

// --------------------------------------------------------------
/*	Display an error dialog if the given AppleEvent contains
	a keyErrorNumber.  a keyErrorString is optional and will be
	displayed if present
	In:	Apple Event
	Out: Error dialog displayed if error data present. */
// --------------------------------------------------------------
Boolean MoreExtractFromAEDesc::DisplayErrorReply(AppleEvent &reply)
{
	DescType realType;
	Size actualSize;
	OSErr	errNumber;
	Str255 errorString;
	// First check for errors
	errNumber = AEUtilities::EventHasErrorReply(reply);
	if (errNumber == noErr)
		return false;
	
	// server returned an error, so get error string
	OSErr err = ::AEGetParamPtr(&reply, keyErrorString, typeChar, 
						&realType, &errorString[1], sizeof(errorString)-1, 
						&actualSize);
	if (err == noErr)
	{
		errorString[0] = actualSize > 255 ? 255 : actualSize;
		ErrorManager::ErrorNotify(errNumber, errorString);
	}
	else
		ErrorManager::ErrorNotify(errNumber, (unsigned char *)*GetString(AE_ERR_RESID));
	return TRUE;
}

// --------------------------------------------------------------
/*	Return the process serial number of the sending process.
	In:	Apple Event send by some app.
	Out: ProcessSerialNumber of the sending app. */
// --------------------------------------------------------------
ProcessSerialNumber	MoreExtractFromAEDesc::ExtractAESender(const AppleEvent &inAppleEvent)
{
	Size realSize;
	DescType realType;
	TargetID target;
	ProcessSerialNumber psn;

	OSErr err = AEGetAttributePtr(&inAppleEvent, keyAddressAttr, typeTargetID, &realType, 
									&target, sizeof(target), &realSize);
	ThrowIfOSErr_(err);
	err = ::GetProcessSerialNumberFromPortName(&target.name,&psn);
	ThrowIfOSErr_(err);
	return psn;
}

//-----------------------------------
void MoreExtractFromAEDesc::DispatchURLDirectly(const AppleEvent &inAppleEvent)
//-----------------------------------
{
	char *url = NULL;
	MoreExtractFromAEDesc::GetCString(inAppleEvent, keyDirectObject, url);
	ThrowIfNil_(url);
	URL_Struct * request = NET_CreateURLStruct (url, NET_DONT_RELOAD);
	XP_FREE (url);
	ThrowIfNil_(request);
	request->internal_url = TRUE; // for attachments in mailto: urls.
	CURLDispatcher::DispatchURL(request, NULL);
}


/*-------------------------------------------------------------*/
//	 class AEUtilities
//	Some more simple Apple Event utility routines.
/*-------------------------------------------------------------*/

// --------------------------------------------------------------
/*	CreateAppleEvent
	Create a new Apple Event from scratch.
	In:	Apple Event suite
		Apple Event ID
		Ptr to return Apple Event
		ProcessSerialNumber of the target app to send event to.
	Out:A new Apple Event is created.  More data may be added to
		the event simply by calling AEPutParamDesc or AEPutParamPtr */
// --------------------------------------------------------------

OSErr AEUtilities::CreateAppleEvent(OSType suite, OSType id, 
		AppleEvent &event, ProcessSerialNumber targetPSN)
{
	AEAddressDesc progressApp;
	OSErr err = ::AECreateDesc(typeProcessSerialNumber, &targetPSN, 
							 sizeof(targetPSN), &progressApp);
	if (err)
		return err;
	err = ::AECreateAppleEvent(suite, id,
									&progressApp,
									kAutoGenerateReturnID,
									kAnyTransactionID,
									&event);
	AEDisposeDesc(&progressApp);
	return err;
}

// --------------------------------------------------------------
/*	Check to see if there is an error in the given AEvent.
	We simply return an OSError equiv to the error value
	in the event.  If none exists (or an error took place
	during access) we return 0.
	In:	Apple Event to test
	Out:Error value returned */
// --------------------------------------------------------------
OSErr AEUtilities::EventHasErrorReply(AppleEvent & reply)
{
	if (reply.descriptorType == typeNull)
		return noErr;
		
	long	errNumber;
	Size realSize;
	DescType realType;
	OSErr err = ::AEGetParamPtr(&reply, keyErrorNumber, typeLongInteger, 
						&realType, &errNumber, sizeof(errNumber), 
						&realSize);
	if (err == noErr)
		return errNumber;
	else
		return noErr; 
}

/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Conrad Carlen <conrad@ingress.com>
 */

#include "CBrowserApp.h"

#include <LGrowZone.h>
#include <PP_Messages.h>
#include <PP_Resources.h>
#include <UDrawingState.h>
#include <UMemoryMgr.h>
#include <URegistrar.h>

#include <LWindow.h>
#include <LCaption.h>

#include <UControlRegistry.h>
#include <UGraphicUtils.h>
#include <UEnvironment.h>

#include <Appearance.h>

#include "ApplIDs.h"
#include "CBrowserWindow.h"
#include "CBrowserShell.h"
#include "CUrlField.h"
#include "CThrobber.h"
#include "UMacUnicode.h"
#include "nsIImageManager.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsIPref.h"
#include "macstdlibextras.h"
#include "SIOUX.h"

#include <TextServices.h>

extern "C" void NS_SetupRegistry();


static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);


// ===========================================================================
//		¥ Main Program
// ===========================================================================

int main()
{
								
	SetDebugThrow_(PP_PowerPlant::debugAction_Alert);	// Set Debugging options
	SetDebugSignal_(PP_PowerPlant::debugAction_Alert);

	PP_PowerPlant::InitializeHeap(3);		// Initialize Memory Manager
											// Parameter is number of Master Pointer
											// blocks to allocate
	
	
	PP_PowerPlant::UQDGlobals::InitializeToolbox(&qd);	// Initialize standard Toolbox managers

#if DEBUG
	::InitializeSIOUX(false);
#endif
	
	::InitTSMAwareApplication();
	
	new PP_PowerPlant::LGrowZone(20000);	// Install a GrowZone function to catch low memory situations.

	{
		CBrowserApp	theApp;			// create instance of your application
	
		theApp.Run();
	}

	::CloseTSMAwareApplication();
	
	return 0;
}


// ---------------------------------------------------------------------------
//		MMozillaApp constructor
// ---------------------------------------------------------------------------

MMozillaApp::MMozillaApp() :
  mpIEventQueueService(nsnull), mpIServiceManager(nsnull),
  mpIPref(nsnull)
{
	nsresult rv = InitWebShellApp();
	if (rv != NS_OK)
		ThrowOSErr_(rv);
}

// ---------------------------------------------------------------------------
//		MMozillaApp destructor
// ---------------------------------------------------------------------------

MMozillaApp::~MMozillaApp()
{
	TermWebShellApp();
}                  

// ---------------------------------------------------------------------------
//		MMozillaApp::InitWebShell
// ---------------------------------------------------------------------------
//	Does once only initialization of WebShell.
//	Called at application startup.

nsresult MMozillaApp::InitWebShellApp()
{
	//RegisterClass_(CWebShell);
	RegisterClass_(CBrowserShell);
	RegisterClass_(CBrowserWindow);
	RegisterClass_(CUrlField);
	RegisterClass_(CThrobber);

	nsresult rv;
	
	// Initialize XPCOM
 	NS_InitXPCOM(&mpIServiceManager, nsnull);
 
 	rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, NULL /* default */);
	if (NS_OK != rv) {
		NS_ASSERTION(PR_FALSE, "Could not AutoRegister");
		return rv;
	}
 	
	// Initialize the Registry
	NS_SetupRegistry();
	
	// Create the Event Queue for the UI thread...
	rv = nsServiceManager::GetService(NS_EVENTQUEUESERVICE_PROGID,
	                                 NS_GET_IID(nsIEventQueueService),
	                                 (nsISupports **)&mpIEventQueueService);
	if (NS_OK != rv) {
		NS_ASSERTION(PR_FALSE, "Could not obtain the event queue service");
		return rv;
	}

	rv = mpIEventQueueService->CreateThreadEventQueue();
	if (NS_OK != rv) {
		NS_ASSERTION(PR_FALSE, "Could not create the event queue for the thread");
		return rv;
	}
			
  // Load preferences
	rv = nsComponentManager::CreateInstance(kPrefCID, NULL, kIPrefIID,
	                                        (void **) &mpIPref);
	if (NS_OK != rv) {
		NS_ASSERTION(PR_FALSE, "Could not create prefs object");
		return rv;
	}
	  
	mpIPref->StartUp();
	mpIPref->ReadUserPrefs();
	
	// HACK ALERT: Since we don't have profiles, we don't have prefs
	// Reduce the default font size here by hand
    mpIPref->SetIntPref("font.size.variable.x-western", 12);
    mpIPref->SetIntPref("font.size.fixed.x-western", 9);
	
	return NS_OK;
}


// ---------------------------------------------------------------------------
//		MMozillaApp::TermWebShell
// ---------------------------------------------------------------------------
//	Does once per application run initialization of WebShell.
//	Called at application quit time.

void MMozillaApp::TermWebShellApp()
{
	nsresult	rv;
	
	UMacUnicode::ReleaseUnit();
	
	if (nsnull != mpIEventQueueService) {
		nsServiceManager::ReleaseService(NS_EVENTQUEUESERVICE_PROGID, mpIEventQueueService);
		mpIEventQueueService = nsnull;
	}
	
	if (nsnull != mpIPref) {
    	mpIPref->ShutDown();
    	NS_RELEASE(mpIPref);
	}
	
	// Shutdown XPCOM?
	rv = NS_ShutdownXPCOM(nsnull);
	NS_ASSERTION(NS_SUCCEEDED(rv), "NS_ShutdownXPCOM failed");

}


// ---------------------------------------------------------------------------
//		¥ CBrowserApp
// ---------------------------------------------------------------------------
//	Constructor

CBrowserApp::CBrowserApp()
{
	if ( PP_PowerPlant::UEnvironment::HasFeature( PP_PowerPlant::env_HasAppearance ) ) {
		::RegisterAppearanceClient();
	}

	RegisterClass_(PP_PowerPlant::LWindow);		// You must register each kind of
	RegisterClass_(PP_PowerPlant::LCaption);	// PowerPlant classes that you use in your PPob resource.
	
	// Register the Appearance Manager/GA classes
	PP_PowerPlant::UControlRegistry::RegisterClasses();

  SetSleepTime(0);			
}


// ---------------------------------------------------------------------------
//		¥ ~CBrowserApp
// ---------------------------------------------------------------------------
//	Destructor
//

CBrowserApp::~CBrowserApp()
{
}

// ---------------------------------------------------------------------------
//		¥ StartUp
// ---------------------------------------------------------------------------
//	This method lets you do something when the application starts up
//	without a document. For example, you could issue your own new command.

void
CBrowserApp::StartUp()
{
	ObeyCommand(PP_PowerPlant::cmd_New, nil);	// EXAMPLE, create a new window
}


// ---------------------------------------------------------------------------
//	¥ ProcessNextEvent												  [public]
// ---------------------------------------------------------------------------
//	Retrieve and handle the next event in the event queue 

void
CBrowserApp::ProcessNextEvent()
{
	EventRecord		macEvent;

		// When on duty (application is in the foreground), adjust the
		// cursor shape before waiting for the next event. Except for the
		// very first time, this is the same as adjusting the cursor
		// after every event.
	
	if (IsOnDuty()) {
			
			// Calling OSEventAvail with a zero event mask will always
			// pass back a null event. However, it fills the EventRecord
			// with the information we need to set the cursor shape--
			// the mouse location in global coordinates and the state
			// of the modifier keys.
			
		::OSEventAvail(0, &macEvent);
		AdjustCursor(macEvent);
	}
	
		// Retrieve the next event. Context switch could happen here.
	
	SetUpdateCommandStatus(false);
	Boolean	gotEvent = ::WaitNextEvent(everyEvent, &macEvent, mSleepTime,
										mMouseRgn);

#if DEBUG
    // NOTE: SIOUX doesn't get any nullEvents because it rudely sets the
    // cursor on nullEvents - even if the mouse is not over its window
		if (gotEvent && SIOUXHandleOneEvent(&macEvent))
			return;
#endif
	
		// Let Attachments process the event. Continue with normal
		// event dispatching unless suppressed by an Attachment.
	
	if (LAttachable::ExecuteAttachments(msg_Event, &macEvent)) {
		if (gotEvent) {
			DispatchEvent(macEvent);
		} else {
			UseIdleTime(macEvent);
		}
	}

									// Repeaters get time after every event
	LPeriodical::DevoteTimeToRepeaters(macEvent);
	
									// Update status of menu items
	if (IsOnDuty() && GetUpdateCommandStatus()) {
		UpdateMenus();
	}
}


// ---------------------------------------------------------------------------
//		¥ ObeyCommand
// ---------------------------------------------------------------------------
//	This method lets the application respond to commands like Menu commands

Boolean
CBrowserApp::ObeyCommand(
	PP_PowerPlant::CommandT	inCommand,
	void					*ioParam)
{
	Boolean		cmdHandled = true;

	switch (inCommand) {
	
		// Handle command messages (defined in PP_Messages.h).
		case PP_PowerPlant::cmd_New:
			
			PP_PowerPlant::LWindow	*theWindow =
									PP_PowerPlant::LWindow::CreateWindow(wind_BrowserWindow, this);
			ThrowIfNil_(theWindow);
			
			// LWindow is not initially visible in PPob resource
			theWindow->Show();
			break;

		// Any that you don't handle, such as cmd_About and cmd_Quit,
		// will be passed up to LApplication
		default:
			cmdHandled = PP_PowerPlant::LApplication::ObeyCommand(inCommand, ioParam);
			break;
	}
	
	return cmdHandled;
}

// ---------------------------------------------------------------------------
//		¥ FindCommandStatus
// ---------------------------------------------------------------------------
//	This function enables menu commands.
//

void
CBrowserApp::FindCommandStatus(
	PP_PowerPlant::CommandT	inCommand,
	Boolean					&outEnabled,
	Boolean					&outUsesMark,
	PP_PowerPlant::Char16	&outMark,
	Str255					outName)
{

	switch (inCommand) {
	
		// Return menu item status according to command messages.
		case PP_PowerPlant::cmd_New:
			outEnabled = true;
			break;

		// Any that you don't handle, such as cmd_About and cmd_Quit,
		// will be passed up to LApplication
		default:
			PP_PowerPlant::LApplication::FindCommandStatus(inCommand, outEnabled,
												outUsesMark, outMark, outName);
			break;
	}
}

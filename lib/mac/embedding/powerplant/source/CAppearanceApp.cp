// ===========================================================================
//	CAppearanceApp.cp 		©1994-1998 Metrowerks Inc. All rights reserved.
// ===========================================================================
//	This file contains the starter code for an Appearance PowerPlant application

#include "CAppearanceApp.h"

#include <LGrowZone.h>
#include <PP_Messages.h>
#include <PP_Resources.h>
#include <PPobClasses.h>
#include <UDrawingState.h>
#include <UMemoryMgr.h>
#include <URegistrar.h>

#include <LWindow.h>
#include <LCaption.h>

#include <UControlRegistry.h>
#include <UGraphicUtils.h>
#include <UEnvironment.h>

#include <Appearance.h>

#include "CWebShell.h"	 							//¥RAPTOR begin
#include "CBrowserWindow.h"
#include "CUrlField.h"
#include "nsIImageManager.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include <TextServices.h>

extern "C" void NS_SetupRegistry();		//¥RAPTOR end



// put declarations for resource ids (ResIDTs) here

const PP_PowerPlant::ResIDT	wind_SampleWindow = 128;	// EXAMPLE, create a new window


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
	
	new PP_PowerPlant::LGrowZone(20000);	// Install a GrowZone function to catch low memory situations.


																			//¥RAPTOR begin
 // Initialize the Registry
 // Note: This can go away when Auto-Registration is implemented in all the Raptor DLLs.
	NS_SetupRegistry();
	nsIImageManager *manager;
	NS_NewImageManager(&manager);
	InitTSMAwareApplication();

	nsresult rv;
	nsIEventQueueService* eventQService;

	// Create the Event Queue for the UI thread...
	rv = nsServiceManager::GetService(NS_EVENTQUEUESERVICE_PROGID,
	                                 NS_GET_IID(nsIEventQueueService),
	                                 (nsISupports **)&eventQService);
	if (NS_OK != rv)
		NS_ASSERTION(PR_FALSE, "Could not obtain the event queue service");

	rv = eventQService->CreateThreadEventQueue();
	if (NS_OK != rv)
		NS_ASSERTION(PR_FALSE, "Could not create the event queue for the thread");
																			//¥RAPTOR end



	CAppearanceApp	theApp;			// create instance of your application
	theApp.Run();



	if (nsnull != eventQService) {			//¥RAPTOR begin
		nsServiceManager::ReleaseService(NS_EVENTQUEUESERVICE_PROGID, eventQService);
		eventQService = nsnull;						//¥RAPTOR end
	}

 ::CloseTSMAwareApplication();
	
	return 0;
}


// ---------------------------------------------------------------------------
//		¥ CAppearanceApp
// ---------------------------------------------------------------------------
//	Constructor

CAppearanceApp::CAppearanceApp()
{
	if ( PP_PowerPlant::UEnvironment::HasFeature( PP_PowerPlant::env_HasAppearance ) ) {
		::RegisterAppearanceClient();
	}

	RegisterClass_(PP_PowerPlant::LWindow);		// You must register each kind of
	RegisterClass_(PP_PowerPlant::LCaption);	// PowerPlant classes that you use
												// in your PPob resource.
	
	// Register the Appearance Manager/GA classes
	PP_PowerPlant::UControlRegistry::RegisterClasses();

	RegisterClass_(CWebShell); 			//¥RAPTOR
	RegisterClass_(CBrowserWindow); //¥RAPTOR
	RegisterClass_(CUrlField); 			//¥RAPTOR

	SetSleepTime(0);	//¥RAPTOR
}


// ---------------------------------------------------------------------------
//		¥ ~CAppearanceApp
// ---------------------------------------------------------------------------
//	Destructor
//

CAppearanceApp::~CAppearanceApp()
{
}

// ---------------------------------------------------------------------------
//		¥ StartUp
// ---------------------------------------------------------------------------
//	This method lets you do something when the application starts up
//	without a document. For example, you could issue your own new command.

void
CAppearanceApp::StartUp()
{
	ObeyCommand(PP_PowerPlant::cmd_New, nil);	// EXAMPLE, create a new window
}

// ---------------------------------------------------------------------------
//		¥ ObeyCommand
// ---------------------------------------------------------------------------
//	This method lets the application respond to commands like Menu commands

Boolean
CAppearanceApp::ObeyCommand(
	PP_PowerPlant::CommandT	inCommand,
	void					*ioParam)
{
	Boolean		cmdHandled = true;

	switch (inCommand) {
	
		// Handle command messages (defined in PP_Messages.h).
		case PP_PowerPlant::cmd_New:
										
			PP_PowerPlant::LWindow	*theWindow =
									PP_PowerPlant::LWindow::CreateWindow(wind_SampleBrowserWindow, this);	//¥RAPTOR: was 'wind_SampleWindow'
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
CAppearanceApp::FindCommandStatus(
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

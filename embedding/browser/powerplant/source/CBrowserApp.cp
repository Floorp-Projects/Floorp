/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *   Conrad Carlen <ccarlen@netscape.com>
 */

#include "CBrowserApp.h"

#include <LGrowZone.h>
#include <PP_Messages.h>
#include <PP_Resources.h>
#include <UDrawingState.h>
#include <UMemoryMgr.h>
#include <URegistrar.h>
#include <LPushButton.h>
#include <LStaticText.h>
#include <LIconControl.h>
#include <LWindow.h>
#include <LTextTableView.h>
#include <UControlRegistry.h>
#include <UGraphicUtils.h>
#include <UEnvironment.h>
#include <Appearance.h>
#include <UConditionalDialogs.h>
#include <LCMAttachment.h>
#include <UCMMUtils.h>

#include "ApplIDs.h"
#include "CBrowserWindow.h"
#include "CBrowserShell.h"
#include "CUrlField.h"
#include "CThrobber.h"
#include "CWebBrowserCMAttachment.h"
#include "UMacUnicode.h"
#include "CAppFileLocationProvider.h"
#include "EmbedEventHandling.h"

#include "nsEmbedAPI.h"

#include "nsIServiceManager.h"
#include "nsIDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIObserverService.h"
#include "nsIPref.h"
#include "nsRepeater.h"
#include "nsILocalFile.h"
#include "nsILocalFileMac.h"
#include "nsIFileChannel.h"
#include "nsIFileSpec.h"
#include "nsMPFileLocProvider.h"
#include "nsXPIDLString.h"
#include "macstdlibextras.h"
#include "SIOUX.h"
#include "nsIURL.h"

#include <TextServices.h>

#if USE_PROFILES
#include "CProfileManager.h"
#include "nsIProfileChangeStatus.h"
#endif

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static const char* kProgramName = "PPEmbed";

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
//		¥ CBrowserApp
// ---------------------------------------------------------------------------
//	Constructor

CBrowserApp::CBrowserApp()
{

#if USE_PROFILES
    mRefCnt = 1;
#endif

	if ( PP_PowerPlant::UEnvironment::HasFeature( PP_PowerPlant::env_HasAppearance ) ) {
		::RegisterAppearanceClient();
	}

	RegisterClass_(PP_PowerPlant::LWindow);	// You must register each kind of
	RegisterClass_(PP_PowerPlant::LCaption);	// PowerPlant classes that you use in your PPob resource.
	RegisterClass_(PP_PowerPlant::LTabGroupView);
    RegisterClass_(PP_PowerPlant::LIconControl);
    RegisterClass_(PP_PowerPlant::LView);
	
	// Register the Appearance Manager/GA classes
	PP_PowerPlant::UControlRegistry::RegisterClasses();
	
	// Register classes used by embedding
	RegisterClass_(CBrowserShell);
	RegisterClass_(CBrowserWindow);
	RegisterClass_(CUrlField);
	RegisterClass_(CThrobber);

#if USE_PROFILES	
	RegisterClass_(LScroller);
	RegisterClass_(LTextTableView);
	RegisterClass_(LColorEraseAttachment);
#endif

   // Contexual Menu Support
   UCMMUtils::Initialize();
   RegisterClass_(LCMAttachment);
   RegisterClass_(CWebBrowserCMAttachment);
   AddAttachment(new LCMAttachment);

   // We need to idle threads often
   SetSleepTime(0);
    
   // Get the directory which contains the mozilla parts
   // In this case it is the app directory but it could
   // be anywhere (an existing install of mozilla)

   nsresult        rv;
   ProcessSerialNumber psn;
   ProcessInfoRec  processInfo;
   FSSpec          appSpec;
   nsCOMPtr<nsILocalFileMac> macDir;
   nsCOMPtr<nsILocalFile>    appDir;  // If this ends up being NULL, default is used

   if (!::GetCurrentProcess(&psn)) {
      processInfo.processInfoLength = sizeof(processInfo);
      processInfo.processName = NULL;
      processInfo.processAppSpec = &appSpec;    
      if (!::GetProcessInformation(&psn, &processInfo)) {
         // Turn the FSSpec of the app into an FSSpec of the app's directory
         ::FSMakeFSSpec(appSpec.vRefNum, appSpec.parID, "\p", &appSpec);
         // Make an nsILocalFile out of it
         rv = NS_NewLocalFileWithFSSpec(&appSpec, PR_TRUE, getter_AddRefs(macDir));
         if (NS_SUCCEEDED(rv))
             appDir = do_QueryInterface(macDir);
      }
   }
   
   CAppFileLocationProvider *fileLocProvider = new CAppFileLocationProvider(kProgramName);
   ThrowIfNil_(fileLocProvider);

   rv = NS_InitEmbedding(appDir, fileLocProvider);

   InitializeWindowCreator();
   InitializeEmbedEventHandling(this);
}


// ---------------------------------------------------------------------------
//		¥ ~CBrowserApp
// ---------------------------------------------------------------------------
//	Destructor
//

CBrowserApp::~CBrowserApp()
{
   nsresult rv;
   NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
   if (NS_SUCCEEDED(rv) && prefs)
      prefs->SavePrefFile(nsnull);

   NS_TermEmbedding();
}

// ---------------------------------------------------------------------------
//		¥ StartUp
// ---------------------------------------------------------------------------
//	This method lets you do something when the application starts up
//	without a document. For example, you could issue your own new command.

void
CBrowserApp::StartUp()
{
    nsresult rv;

#if USE_PROFILES

    // Register for profile changes    
    NS_WITH_SERVICE(nsIObserverService, observerService, NS_OBSERVERSERVICE_CONTRACTID, &rv);
    ThrowIfNil_(observerService);
    observerService->AddObserver(this, NS_LITERAL_STRING("profile-approve-change").get());
    observerService->AddObserver(this, NS_LITERAL_STRING("profile-change-teardown").get());
    observerService->AddObserver(this, NS_LITERAL_STRING("profile-after-change").get());

    CProfileManager *profileMgr = new CProfileManager;
    profileMgr->StartUp();
    AddAttachment(profileMgr);

#else
    
    // If we don't want different user profiles, all that's needed is
    // to make an nsMPFileLocProvider. This will provide the same file
    // locations as the profile service but always within the specified folder.
    
    nsCOMPtr<nsIFile> rootDir;   
    nsMPFileLocProvider *locationProvider = new nsMPFileLocProvider;
    ThrowIfNil_(locationProvider);
    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILES_ROOT_DIR, getter_AddRefs(rootDir));
    ThrowIfNil_(rootDir);
    rv = locationProvider->Initialize(rootDir, "guest");   
    ThrowIfError_(rv);
    
    NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv);
    ThrowIfNil_(prefs);
    // Needed because things read default prefs during startup
    prefs->ResetPrefs();
    prefs->ReadUserPrefs();

#endif


	ObeyCommand(PP_PowerPlant::cmd_New, nil);	// EXAMPLE, create a new window
}

// ---------------------------------------------------------------------------
//		¥ MakeMenuBar
// ---------------------------------------------------------------------------

void
CBrowserApp::MakeMenuBar()
{
    LApplication::MakeMenuBar();
    
    // Insert a menu which is not in the menu bar but which contains
    // items which appear in contextual menus. We have to do this hack
    // because LCMAttachment::AddCommand needs a command which is in
    // some LMenu in order to get the text for a contextual menu item.
    
    LMenuBar::GetCurrentMenuBar()->InstallMenu(new LMenu(menu_Buzzwords), hierMenu);
}

// ---------------------------------------------------------------------------
//	¥ AdjustCursor												      [public]
// ---------------------------------------------------------------------------

void CBrowserApp::AdjustCursor(const EventRecord& inMacEvent)
{
  // Needed in order to give an attachment to the application a
  // msg_AdjustCursor. CEmbedEventAttachment needs this.
  
  if (ExecuteAttachments(msg_AdjustCursor, (void*) &inMacEvent))
	  LEventDispatcher::AdjustCursor(inMacEvent);
}


// ---------------------------------------------------------------------------
//	¥ HandleAppleEvent												  [public]
// ---------------------------------------------------------------------------

void CBrowserApp::HandleAppleEvent(const AppleEvent&	inAppleEvent,
                                   AppleEvent&			outAEReply,
                                   AEDesc&				outResult,
                                   long				    inAENumber)
{
	switch (inAENumber) {
	
		case 5000:
		    {
		        OSErr err;
		        Handle dataH;
		        
        		StAEDescriptor urlDesc;
			    err = ::AEGetParamDesc(&inAppleEvent, keyDirectObject, typeWildCard, urlDesc);
			    ThrowIfOSErr_(err);
			    
			    StAEDescriptor coerceDesc;
			    if (urlDesc.DescriptorType() != typeChar) {
			        err = ::AECoerceDesc(urlDesc, typeChar, coerceDesc);
			        ThrowIfOSErr_(err);
			        dataH = ((AEDesc)coerceDesc).dataHandle;
			    }
			    else
			        dataH = ((AEDesc)urlDesc).dataHandle;
			        
			    Size dataSize = ::GetHandleSize(dataH);
			    StHandleLocker lock(dataH);
			    
       			CBrowserWindow *theWindow = CBrowserWindow::CreateWindow(nsIWebBrowserChrome::CHROME_DEFAULT, -1, -1);
       			ThrowIfNil_(theWindow);
       			theWindow->SetSizeToContent(false);
                theWindow->GetBrowserShell()->LoadURL(*dataH, dataSize);
       			       			
       			theWindow->Show();
			}
		    break;
			
		default:
		    LApplication::HandleAppleEvent(inAppleEvent, outAEReply, outResult, inAENumber);
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
	
		case PP_PowerPlant::cmd_New:
			{
   			CBrowserWindow *theWindow = CBrowserWindow::CreateWindow(nsIWebBrowserChrome::CHROME_DEFAULT, -1, -1);
   			ThrowIfNil_(theWindow);
   			theWindow->SetSizeToContent(false);
            // Just for demo sake, load a URL	
            theWindow->GetBrowserShell()->LoadURL("http://www.mozilla.org");
   			theWindow->Show();
			}
			break;

		case PP_PowerPlant::cmd_Open:
		case cmd_OpenDirectory:
            {
                FSSpec fileSpec;
                if (SelectFileObject(inCommand, fileSpec))
                {
                    nsresult rv;
                    nsCOMPtr<nsILocalFileMac> macFile;
                    
                    rv = NS_NewLocalFileWithFSSpec(&fileSpec, PR_TRUE, getter_AddRefs(macFile));
                    ThrowIfError_(NS_ERROR_GET_CODE(rv));
                    nsCOMPtr<nsILocalFile> localFile(do_QueryInterface(macFile, &rv));
                    ThrowIfError_(NS_ERROR_GET_CODE(rv));
                    nsCOMPtr<nsIFileURL> aURL(do_CreateInstance("@mozilla.org/network/standard-url;1", &rv));
                    ThrowIfError_(NS_ERROR_GET_CODE(rv));
                    
                    rv = aURL->SetFile(localFile);
                    ThrowIfError_(NS_ERROR_GET_CODE(rv));
                    
                    nsXPIDLCString urlSpec;
                    rv = aURL->GetSpec(getter_Copies(urlSpec));
                    ThrowIfError_(NS_ERROR_GET_CODE(rv));
                        
           			CBrowserWindow *theWindow = CBrowserWindow::CreateWindow(nsIWebBrowserChrome::CHROME_DEFAULT, -1, -1);
           			ThrowIfNil_(theWindow);
           			theWindow->SetSizeToContent(false);
                    theWindow->GetBrowserShell()->LoadURL(urlSpec.get());
           			theWindow->Show();
           		}
			}
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

		case PP_PowerPlant::cmd_Open:
		case cmd_OpenDirectory:
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


Boolean CBrowserApp::AttemptQuitSelf(SInt32 inSaveOption)
{	    
   // IMPORTANT: This is one unfortunate thing about Powerplant - Windows don't
   // get destroyed until the destructor of LCommander. We need to delete
   // all of the CBrowserWindows though before we terminate embedding.
    
 	TArrayIterator<LCommander*> iterator(mSubCommanders, LArrayIterator::from_End);
 	LCommander*		theSub;
 	while (iterator.Previous(theSub)) {
 	    if (dynamic_cast<CBrowserWindow*>(theSub)) {
 		    mSubCommanders.RemoveItemsAt(1, iterator.GetCurrentIndex());
 		    delete theSub;
 		}
 	}
    
   return true;
}

nsresult CBrowserApp::InitializePrefs()
{
   nsresult rv;
   NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv);
   if (NS_SUCCEEDED(rv)) {	  

		// We are using the default prefs from mozilla. If you were
		// disributing your own, this would be done simply by editing
		// the default pref files.
		
		PRBool inited;
		rv = prefs->GetBoolPref("ppbrowser.prefs_inited", &inited);
		if (NS_FAILED(rv) || !inited)
		{
            prefs->SetIntPref("font.size.variable.x-western", 12);
            prefs->SetIntPref("font.size.fixed.x-western", 12);
            rv = prefs->SetBoolPref("ppbrowser.prefs_inited", PR_TRUE);
            if (NS_SUCCEEDED(rv))
                rv = prefs->SavePrefFile(nsnull);
        }
        
	}
	else
		NS_ASSERTION(PR_FALSE, "Could not get preferences service");
		
    return rv;
}

Boolean CBrowserApp::SelectFileObject(PP_PowerPlant::CommandT	inCommand,
                                      FSSpec& outSpec)
{
		// LFileChooser presents the standard dialog for asking
		// the user to open a file. It supports both StandardFile
		// and Navigation Services. The latter allows opening
		// multiple files.

	UConditionalDialogs::LFileChooser	chooser;
	
	NavDialogOptions *theDialogOptions = chooser.GetDialogOptions();
	if (theDialogOptions) {
		theDialogOptions->dialogOptionFlags |= kNavSelectAllReadableItem;
	}

    Boolean     result;
	SInt32      dirID;
	
	if (inCommand == cmd_OpenDirectory)
	{
	    result = chooser.AskChooseFolder(outSpec, dirID);
	}
	else
	{
	    result = chooser.AskOpenFile(LFileTypeList(fileTypes_All));
	    if (result)
	        chooser.GetFileSpec(1, outSpec);
	}
    return result;
}
#if USE_PROFILES

// ---------------------------------------------------------------------------
//  CBrowserApp : nsISupports
// ---------------------------------------------------------------------------

NS_IMPL_ISUPPORTS2(CBrowserApp, nsIObserver, nsISupportsWeakReference);

// ---------------------------------------------------------------------------
//  CBrowserApp : nsIObserver
// ---------------------------------------------------------------------------

NS_IMETHODIMP CBrowserApp::Observe(nsISupports *aSubject, const PRUnichar *aTopic, const PRUnichar *someData)
{
    #define CLOSE_WINDOWS_ON_SWITCH 1

    nsresult rv = NS_OK;
    
    if (!nsCRT::strcmp(aTopic, NS_LITERAL_STRING("profile-approve-change").get()))
    {
        // Ask the user if they want to
        DialogItemIndex item = UModalAlerts::StopAlert(alrt_ConfirmProfileSwitch);
        if (item != kStdOkItemIndex)
        {
            nsCOMPtr<nsIProfileChangeStatus> status = do_QueryInterface(aSubject);
            NS_ENSURE_TRUE(status, NS_ERROR_FAILURE);
            status->VetoChange();
        }
    }
    else if (!nsCRT::strcmp(aTopic, NS_LITERAL_STRING("profile-change-teardown").get()))
    {
        // Close all open windows. Alternatively, we could just call CBrowserWindow::Stop()
        // on each. Either way, we have to stop all network activity on this phase.
        
        TArrayIterator<LCommander*> iterator(mSubCommanders, LArrayIterator::from_End);
        LCommander*		theSub;
        while (iterator.Previous(theSub)) {
            CBrowserWindow *browserWindow = dynamic_cast<CBrowserWindow*>(theSub);
            if (browserWindow) {
                browserWindow->Stop();
        	    mSubCommanders.RemoveItemsAt(1, iterator.GetCurrentIndex());
        	    delete browserWindow;
        	}
        }
    }
    else if (!nsCRT::strcmp(aTopic, NS_LITERAL_STRING("profile-after-change").get()))
    {
        InitializePrefs();

        if (!nsCRT::strcmp(someData, NS_LITERAL_STRING("switch").get())) {
            // Make a new default window
            ObeyCommand(PP_PowerPlant::cmd_New, nil);
        }
    }
    return rv;
}

#endif // USE_PROFILES

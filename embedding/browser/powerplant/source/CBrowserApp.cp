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
#include <LCMAttachment.h>
#include <UCMMUtils.h>

#if PP_Target_Carbon
#include <UNavServicesDialogs.h>
#else
#include <UConditionalDialogs.h>
#endif

#include "ApplIDs.h"
#include "CBrowserWindow.h"
#include "CBrowserShell.h"
#include "CUrlField.h"
#include "CThrobber.h"
#include "CIconServicesIcon.h"
#include "CWebBrowserCMAttachment.h"
#include "UMacUnicode.h"
#include "CAppFileLocationProvider.h"
#include "EmbedEventHandling.h"
#include "PromptService.h"

#include "nsEmbedAPI.h"

#include "nsIServiceManager.h"
#include "nsIDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIObserverService.h"
#include "nsObserverService.h"
#include "nsIPref.h"
#include "nsRepeater.h"
#include "nsILocalFile.h"
#include "nsILocalFileMac.h"
#include "nsIFileChannel.h"
#include "nsIFileSpec.h"
#include "nsMPFileLocProvider.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "macstdlibextras.h"
#include "SIOUX.h"
#include "nsIURL.h"

#include <TextServices.h>

#define NATIVE_PROMPTS

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
	

#if __PowerPlant__ >= 0x02100000
	PP_PowerPlant::UQDGlobals::InitializeToolbox();
#else
	PP_PowerPlant::UQDGlobals::InitializeToolbox(&qd);
#endif
    
#if DEBUG
	::InitializeSIOUX(false);
#endif

#if !TARGET_CARBON		
	::InitTSMAwareApplication();
#endif
	
	new PP_PowerPlant::LGrowZone(20000);	// Install a GrowZone function to catch low memory situations.

	{
		CBrowserApp	theApp;			// create instance of your application
	
		theApp.Run();
	}

#if !TARGET_CARBON	
	::CloseTSMAwareApplication();
#endif
	
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
	RegisterClass_(PP_PowerPlant::LDialogBox);
	
	// Register the Appearance Manager/GA classes
	PP_PowerPlant::UControlRegistry::RegisterClasses();
	
	// Register classes used by embedding
	RegisterClass_(CBrowserShell);
	RegisterClass_(CBrowserWindow);
	RegisterClass_(CUrlField);
	RegisterClass_(CThrobber);
	RegisterClass_(CIconServicesIcon);

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

   OverrideComponents();
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
   nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
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
    nsCOMPtr<nsIObserverService> observerService = 
             do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
    ThrowIfNil_(observerService);
    observerService->AddObserver(this, "profile-approve-change", PR_FALSE);
    observerService->AddObserver(this, "profile-change-teardown", PR_FALSE);
    observerService->AddObserver(this, "profile-after-change", PR_FALSE);

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
    
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv));
    ThrowIfNil_(prefs);
    // Needed because things read default prefs during startup
    prefs->ResetPrefs();
    prefs->ReadUserPrefs();

#endif


	ObeyCommand(PP_PowerPlant::cmd_New, nil);	// EXAMPLE, create a new window
}

nsresult
CBrowserApp::OverrideComponents()
{
    nsresult rv = NS_OK;

#ifdef NATIVE_PROMPTS
    #define NS_PROMPTSERVICE_CID \
     {0xa2112d6a, 0x0e28, 0x421f, {0xb4, 0x6a, 0x25, 0xc0, 0xb3, 0x8, 0xcb, 0xd0}}
    static NS_DEFINE_CID(kPromptServiceCID, NS_PROMPTSERVICE_CID);
    
    // Here, we're creating a factory using a method compiled into
    // the application. This is preferable if you do not want to locate
    // and load an external DLL. That approach is used by MfcEmbed if
    // that's of interest.
    
    nsCOMPtr<nsIFactory> promptFactory;
    rv = NS_NewPromptServiceFactory(getter_AddRefs(promptFactory));
    if (NS_FAILED(rv)) return rv;
    rv = nsComponentManager::RegisterFactory(kPromptServiceCID,
                                              "Prompt Service",
                                              "@mozilla.org/embedcomp/prompt-service;1",
                                              promptFactory,
                                              PR_TRUE); // replace existing
#endif

    return rv;
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
		        
        		StAEDescriptor urlDesc;
			    err = ::AEGetParamDesc(&inAppleEvent, keyDirectObject, typeWildCard, urlDesc);
			    ThrowIfOSErr_(err);
			    AEDesc finalDesc;
			    
			    StAEDescriptor coerceDesc;
			    if (urlDesc.DescriptorType() != typeChar) {
			        err = ::AECoerceDesc(urlDesc, typeChar, coerceDesc);
			        ThrowIfOSErr_(err);
			        finalDesc = coerceDesc;
			    }
			    else
			        finalDesc = urlDesc;
			        
			    Size dataSize = ::AEGetDescDataSize(&finalDesc);
			    StPointerBlock dataPtr(dataSize);
			    err = ::AEGetDescData(&finalDesc, dataPtr.Get(), dataSize);
			    ThrowIfOSErr_(err);
			    			    
		        PRUint32 chromeFlags;
			    
			    // If the URL begins with "view-source:", go with less chrome
			    nsDependentCString dataAsStr(dataPtr.Get(), dataSize);
                nsReadingIterator<char> start, end;
                dataAsStr.BeginReading(start);
                dataAsStr.EndReading(end);
                FindInReadable(NS_LITERAL_CSTRING("view-source:"), start, end);
                if ((start != end) && !start.size_backward()) 
                    chromeFlags = nsIWebBrowserChrome::CHROME_WINDOW_CLOSE +
                                  nsIWebBrowserChrome::CHROME_WINDOW_RESIZE;
                else
                    chromeFlags = nsIWebBrowserChrome::CHROME_DEFAULT;
			    
       			CBrowserWindow *theWindow = CBrowserWindow::CreateWindow(chromeFlags, -1, -1);
       			ThrowIfNil_(theWindow);
       			theWindow->SetSizeToContent(false);
                theWindow->GetBrowserShell()->LoadURL(dataAsStr);
       			       			
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
            theWindow->GetBrowserShell()->LoadURL(nsDependentCString("http://www.mozilla.org"));
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
                    theWindow->GetBrowserShell()->LoadURL(urlSpec);
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
	UInt16	                &outMark,
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
   nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv));
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

#if PP_Target_Carbon
	UNavServicesDialogs::LFileChooser	chooser;
#else
	UConditionalDialogs::LFileChooser	chooser;
#endif
	
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

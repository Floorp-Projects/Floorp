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
#include <UNavServicesDialogs.h>

#include "ApplIDs.h"
#include "CBrowserWindow.h"
#include "CBrowserShell.h"
#include "CBrowserChrome.h"
#include "CWindowCreator.h"
#include "CUrlField.h"
#include "CThrobber.h"
#include "CIconServicesIcon.h"
#include "CWebBrowserCMAttachment.h"
#include "UMacUnicode.h"
#include "CAppFileLocationProvider.h"
#include "EmbedEventHandling.h"
#include "AppComponents.h"

#include "nsEmbedAPI.h"

#include "nsIComponentRegistrar.h"
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
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "macstdlibextras.h"
#include "SIOUX.h"
#include "nsNetUtil.h"
#include "nsIWindowWatcher.h"
#include "nsIDOMWindow.h"
#include "nsIDownload.h"
#include "nsCRT.h"

#include <TextServices.h>

#ifdef USE_PROFILES
#include "CProfileManager.h"
#include "nsIProfileChangeStatus.h"
#else
#include "nsProfileDirServiceProvider.h"
#endif

static const char* kProgramName = "PPEmbed";

// ===========================================================================
//      ¥ Main Program
// ===========================================================================

int main()
{
                                
    SetDebugThrow_(PP_PowerPlant::debugAction_Alert);   // Set Debugging options
    SetDebugSignal_(PP_PowerPlant::debugAction_Alert);

    PP_PowerPlant::InitializeHeap(3);       // Initialize Memory Manager
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
    
    new PP_PowerPlant::LGrowZone(20000);    // Install a GrowZone function to catch low memory situations.

    {
        CBrowserApp theApp;         // create instance of your application
    
        theApp.Run();
    }

#if !TARGET_CARBON  
    ::CloseTSMAwareApplication();
#endif
    
    return 0;
}


// ---------------------------------------------------------------------------
//      ¥ CBrowserApp
// ---------------------------------------------------------------------------
//  Constructor

CBrowserApp::CBrowserApp()
{

#ifdef USE_PROFILES
    mRefCnt = 1;
#endif

#if TARGET_CARBON
  InstallCarbonEventHandlers();
#endif

    if ( PP_PowerPlant::UEnvironment::HasFeature( PP_PowerPlant::env_HasAppearance ) ) {
        ::RegisterAppearanceClient();
    }

    RegisterClass_(PP_PowerPlant::LWindow); // You must register each kind of
    RegisterClass_(PP_PowerPlant::LCaption);    // PowerPlant classes that you use in your PPob resource.
    RegisterClass_(PP_PowerPlant::LTabGroupView);
    RegisterClass_(PP_PowerPlant::LIconControl);
    RegisterClass_(PP_PowerPlant::LView);
    RegisterClass_(PP_PowerPlant::LDialogBox);
    
    // Register the Appearance Manager/GA classes
    PP_PowerPlant::UControlRegistry::RegisterClasses();
    
    // QuickTime is used by CThrobber
    UQuickTime::Initialize();
    
    // Register classes used by embedding
    RegisterClass_(CBrowserShell);
    RegisterClass_(CBrowserWindow);
    RegisterClass_(CUrlField);
    RegisterClass_(CThrobber);
    RegisterClass_(CIconServicesIcon);

#ifdef USE_PROFILES 
    RegisterClass_(LScroller);
    RegisterClass_(LTextTableView);
    RegisterClass_(LColorEraseAttachment);
#endif

   // Contexual Menu Support
   UCMMUtils::Initialize();
   RegisterClass_(LCMAttachment);
   RegisterClass_(CWebBrowserCMAttachment);
   AddAttachment(new LCMAttachment);

   SetSleepTime(5);
    
   // Get the directory which contains the mozilla parts
   // In this case it is the app directory but it could
   // be anywhere (an existing install of mozilla)

   nsresult        rv;
   ProcessSerialNumber psn;
   ProcessInfoRec  processInfo;
   FSSpec          appSpec;
   nsCOMPtr<nsILocalFileMac> macDir;

   if (!::GetCurrentProcess(&psn)) {
      processInfo.processInfoLength = sizeof(processInfo);
      processInfo.processName = NULL;
      processInfo.processAppSpec = &appSpec;    
      if (!::GetProcessInformation(&psn, &processInfo)) {
         // Turn the FSSpec of the app into an FSSpec of the app's directory
         OSErr err = ::FSMakeFSSpec(appSpec.vRefNum, appSpec.parID, "\p", &appSpec);
         // Make an nsILocalFile out of it
         if (err == noErr)
            (void)NS_NewLocalFileWithFSSpec(&appSpec, PR_TRUE, getter_AddRefs(macDir));
      }
   }
   
   CAppFileLocationProvider *fileLocProvider = new CAppFileLocationProvider(kProgramName);
   ThrowIfNil_(fileLocProvider);

   rv = NS_InitEmbedding(macDir, fileLocProvider);

   OverrideComponents();
   CWindowCreator::Initialize();
   InitializeEmbedEventHandling(this);
}


// ---------------------------------------------------------------------------
//      ¥ ~CBrowserApp
// ---------------------------------------------------------------------------
//  Destructor
//

CBrowserApp::~CBrowserApp()
{
   nsresult rv;
   nsCOMPtr<nsIPrefService> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
   if (NS_SUCCEEDED(rv) && prefs)
      prefs->SavePrefFile(nsnull);

   NS_TermEmbedding();
}

// ---------------------------------------------------------------------------
//      ¥ StartUp
// ---------------------------------------------------------------------------
//  This method lets you do something when the application starts up
//  without a document. For example, you could issue your own new command.

void
CBrowserApp::StartUp()
{
    nsresult rv;
        
#ifdef USE_PROFILES

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
    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILES_ROOT_DIR, getter_AddRefs(rootDir));
    ThrowIfNil_(rootDir);
    rv = rootDir->AppendNative(nsDependentCString("guest"));
    ThrowIfError_(rv);       
    
    nsCOMPtr<nsProfileDirServiceProvider> locProvider;
    NS_NewProfileDirServiceProvider(PR_TRUE, getter_AddRefs(locProvider));
    if (!locProvider)
      return NS_ERROR_FAILURE;
    
    // Directory service holds an strong reference to any
    // provider that is registered with it. Let it hold the
    // only ref. locProvider won't die when we leave this scope.
    rv = locProvider->Register();
    ThrowIfError_(rv);
    nsCOMPtr<nsILocalFile> profileDir(do_QueryInterface(rootDir));
    rv = locProvider->SetProfileDir(profileDir);
    ThrowIfError_(rv);
#endif


    ObeyCommand(PP_PowerPlant::cmd_New, nil);   // EXAMPLE, create a new window
}

nsresult
CBrowserApp::OverrideComponents()
{
    nsresult rv = NS_OK;

    nsCOMPtr<nsIComponentRegistrar> cr;
    NS_GetComponentRegistrar(getter_AddRefs(cr));
    if (!cr)
        return NS_ERROR_FAILURE;

    int numComponents;
    const nsModuleComponentInfo* componentInfo = GetAppModuleComponentInfo(&numComponents);
    for (int i = 0; i < numComponents; ++i) {
        nsCOMPtr<nsIGenericFactory> componentFactory;
        rv = NS_NewGenericFactory(getter_AddRefs(componentFactory), &(componentInfo[i]));
            if (NS_FAILED(rv)) {
            NS_ASSERTION(PR_FALSE, "Unable to create factory for component");
            continue;
        }

        rv = cr->RegisterFactory(componentInfo[i].mCID,
                             componentInfo[i].mDescription,
                             componentInfo[i].mContractID,
                             componentFactory);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to register factory for component");
    }

    return rv;
}


// ---------------------------------------------------------------------------
//      ¥ MakeMenuBar
// ---------------------------------------------------------------------------

void
CBrowserApp::MakeMenuBar()
{
    LApplication::MakeMenuBar();
    
    // Insert a menu which is not in the menu bar but which contains
    // items which appear only in contextual menus. We have to do this hack
    // because LCMAttachment::AddCommand needs a command which is in
    // some LMenu in order to get the text for a contextual menu item.
    
    LMenuBar::GetCurrentMenuBar()->InstallMenu(new LMenu(menu_Buzzwords), hierMenu);
}

// ---------------------------------------------------------------------------
//  ¥ AdjustCursor                                                    [public]
// ---------------------------------------------------------------------------

void CBrowserApp::AdjustCursor(const EventRecord& inMacEvent)
{
  // Needed in order to give an attachment to the application a
  // msg_AdjustCursor. CEmbedEventAttachment needs this.
  
  if (ExecuteAttachments(msg_AdjustCursor, (void*) &inMacEvent))
      LEventDispatcher::AdjustCursor(inMacEvent);
}


// ---------------------------------------------------------------------------
//  ¥ HandleAppleEvent                                                [public]
// ---------------------------------------------------------------------------

void CBrowserApp::HandleAppleEvent(const AppleEvent&    inAppleEvent,
                                   AppleEvent&          outAEReply,
                                   AEDesc&              outResult,
                                   long                 inAENumber)
{
    switch (inAENumber) {
    
        case 5000:
            {
                OSErr err;
                
                StAEDescriptor urlDesc;
                err = ::AEGetParamDesc(&inAppleEvent, keyDirectObject, typeChar, urlDesc);
                ThrowIfOSErr_(err);
                    
                Size dataSize = ::AEGetDescDataSize(urlDesc);
                StPointerBlock urlPtr(dataSize);
                err = ::AEGetDescData(urlDesc, urlPtr.Get(), dataSize);
                ThrowIfOSErr_(err);

                const nsACString& urlAsStr = Substring(urlPtr.Get(), urlPtr.Get() + dataSize);
                
                // If the URL begins with "view-source:", go with less chrome
                PRUint32 chromeFlags;
                NS_NAMED_LITERAL_CSTRING(kViewSourceProto, "view-source:");
                if (Substring(urlAsStr, 0, kViewSourceProto.Length()).Equals(kViewSourceProto))
                    chromeFlags = nsIWebBrowserChrome::CHROME_WINDOW_CLOSE +
                                  nsIWebBrowserChrome::CHROME_WINDOW_RESIZE;
                else
                    chromeFlags = nsIWebBrowserChrome::CHROME_DEFAULT;
                                                
                // See if we have a referrer
                nsCAutoString referrerAsStr;
                StAEDescriptor referrerDesc;
                err = ::AEGetParamDesc(&inAppleEvent, keyGetURLReferrer, typeChar, referrerDesc);
                if (err == noErr) {
                    dataSize = ::AEGetDescDataSize(referrerDesc);
                    StPointerBlock referrerPtr(dataSize);
                    err = ::AEGetDescData(referrerDesc, referrerPtr.Get(), dataSize);
                    ThrowIfOSErr_(err);
                    referrerAsStr = Substring(referrerPtr.Get(), referrerPtr.Get() + dataSize);
                }           
                LWindow *theWindow = CWindowCreator::CreateWindowInternal(chromeFlags, PR_TRUE, -1, -1);
                ThrowIfNil_(theWindow);
                CBrowserShell *theBrowser = dynamic_cast<CBrowserShell*>(theWindow->FindPaneByID(CBrowserShell::paneID_MainBrowser));
                ThrowIfNil_(theBrowser);
                theBrowser->LoadURL(urlAsStr, referrerAsStr);
                                
                theWindow->Show();
            }
            break;
            
        case ae_ApplicationDied: // We get these from opening downloaded files with Stuffit - ignore.
            break;
            
        default:
            LApplication::HandleAppleEvent(inAppleEvent, outAEReply, outResult, inAENumber);
    }
}


// ---------------------------------------------------------------------------
//      ¥ ObeyCommand
// ---------------------------------------------------------------------------
//  This method lets the application respond to commands like Menu commands

Boolean
CBrowserApp::ObeyCommand(
    PP_PowerPlant::CommandT inCommand,
    void                    *ioParam)
{
    Boolean     cmdHandled = true;

    switch (inCommand) {
    
        case PP_PowerPlant::cmd_About:
      break;
        
        case PP_PowerPlant::cmd_New:
            {           
                LWindow *theWindow = CWindowCreator::CreateWindowInternal(nsIWebBrowserChrome::CHROME_DEFAULT, PR_TRUE, -1, -1);
                ThrowIfNil_(theWindow);
                CBrowserShell *theBrowser = dynamic_cast<CBrowserShell*>(theWindow->FindPaneByID(CBrowserShell::paneID_MainBrowser));
                ThrowIfNil_(theBrowser);
                // Just for demo sake, load a URL
                theBrowser->LoadURL(nsDependentCString("http://www.mozilla.org"));
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

                    nsCAutoString urlSpec;                    
                    rv = NS_GetURLSpecFromFile(macFile, urlSpec);
                    ThrowIfError_(NS_ERROR_GET_CODE(rv));
                                            
                    LWindow *theWindow = CWindowCreator::CreateWindowInternal(nsIWebBrowserChrome::CHROME_DEFAULT, PR_TRUE, -1, -1);
                    ThrowIfNil_(theWindow);
                    CBrowserShell *theBrowser = dynamic_cast<CBrowserShell*>(theWindow->FindPaneByID(CBrowserShell::paneID_MainBrowser));
                    ThrowIfNil_(theBrowser);
                    theBrowser->LoadURL(urlSpec);
                    theWindow->Show();
                }
            }
            break;

        case PP_PowerPlant::cmd_Preferences:
        {
            nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
            ThrowIfNil_(wwatch);
                
            // Note: We're not making this window modal even though it looks like a modal
            // dialog (has OK and Cancel buttons). Reason is, the help window which can
            // be opened from the prefs dialog is non-modal. If the prefs dialog was modal,
            // the help window would be stuck behind it in the non-modal layer.
            
            // And, since its non-modal, we have to check for an already open prefs window
            // and just select it rather than making a new one.
            nsCOMPtr<nsIDOMWindow> extantPrefsWindow;
            wwatch->GetWindowByName(NS_LITERAL_STRING("_prefs").get(), nsnull, getter_AddRefs(extantPrefsWindow));
            if (extantPrefsWindow) {
                // activate the window
                LWindow *extantPrefsLWindow = CBrowserChrome::GetLWindowForDOMWindow(extantPrefsWindow);
                ThrowIfNil_(extantPrefsLWindow);
                extantPrefsLWindow->Select();
            }
            else {
                nsCOMPtr<nsIDOMWindow> domWindow;
                wwatch->OpenWindow(nsnull,
                                  "chrome://communicator/content/pref/pref.xul",
                                  "_prefs",
                                  "centerscreen,chrome,dialog,titlebar",
                                  nsnull,
                                  getter_AddRefs(domWindow));
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
//      ¥ FindCommandStatus
// ---------------------------------------------------------------------------
//  This function enables menu commands.
//

void
CBrowserApp::FindCommandStatus(
    PP_PowerPlant::CommandT inCommand,
    Boolean                 &outEnabled,
    Boolean                 &outUsesMark,
    UInt16                  &outMark,
    Str255                  outName)
{

    switch (inCommand) {
    
        case PP_PowerPlant::cmd_About:
            outEnabled = false;
      break;
      
        case PP_PowerPlant::cmd_New:
            outEnabled = true;
            break;

        case PP_PowerPlant::cmd_Open:
        case cmd_OpenDirectory:
            outEnabled = true;
            break;

        case PP_PowerPlant::cmd_Preferences:
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
    LCommander*     theSub;
    while (iterator.Previous(theSub)) {
        if (dynamic_cast<CBrowserWindow*>(theSub)) {
            mSubCommanders.RemoveItemsAt(1, iterator.GetCurrentIndex());
            delete theSub;
        }
    }
    
   return true;
}

#if TARGET_CARBON

void CBrowserApp::InstallCarbonEventHandlers()
{
    EventTypeSpec appEventList[] = {{kEventClassCommand, kEventCommandProcess},
                                    {kEventClassCommand, kEventCommandUpdateStatus}};

    InstallApplicationEventHandler(NewEventHandlerUPP(AppEventHandler), 2, appEventList, this, NULL);                                    
}

pascal OSStatus CBrowserApp::AppEventHandler(EventHandlerCallRef aHandlerChain,
                                             EventRef event,
                                             void* userData)
{
    HICommand       command;
    OSStatus        result = eventNotHandledErr; /* report failure by default */

    if (::GetEventParameter(event, kEventParamDirectObject, 
                            typeHICommand, NULL, sizeof(HICommand), 
                            NULL, &command) != noErr)
        return result;

    switch (::GetEventKind(event))
    {
        case kEventCommandProcess:
            {
                switch (command.commandID)
                {
                    case kHICommandPreferences:
                        CBrowserApp *theApp = reinterpret_cast<CBrowserApp*>(userData);
                        theApp->ObeyCommand(PP_PowerPlant::cmd_Preferences, nsnull);
                        result = noErr;
                        break;
                    default:
                        break;
                }
            }
            break;

        case kEventCommandUpdateStatus:
            {
                switch (command.commandID)
                {
                    case kHICommandPreferences:
                        ::EnableMenuCommand(nsnull, kHICommandPreferences);
                        result = noErr;
                        break;
                    default:
                        break;
                }
            }
            break;

        default:
            break; 
    }
    return result;
}

#endif // TARGET_CARBON

nsresult CBrowserApp::InitializePrefs()
{
    nsresult rv;
    nsCOMPtr<nsIPrefService> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
    if (NS_FAILED(rv))
        return rv;    

    // We are using the default prefs from mozilla. If you were
    // disributing your own, this would be done simply by editing
    // the default pref files.
    nsCOMPtr<nsIPrefBranch> branch;
    rv = prefs->GetBranch(nsnull, getter_AddRefs(branch));
    if (NS_FAILED(rv))
        return rv;    

    const char kVariableFontSizePref[] = "font.size.variable.x-western";
    const char kFixedFontSizePref[] = "font.size.fixed.x-western";
    
    PRInt32 intValue;
    rv = branch->GetIntPref(kVariableFontSizePref, &intValue);
    if (NS_FAILED(rv))
        branch->SetIntPref(kVariableFontSizePref, 14);
        
    rv = branch->GetIntPref(kFixedFontSizePref, &intValue);    
    if (NS_FAILED(rv))
        branch->SetIntPref(kFixedFontSizePref, 13);
        
    return NS_OK;
}

Boolean CBrowserApp::SelectFileObject(PP_PowerPlant::CommandT   inCommand,
                                      FSSpec& outSpec)
{
    UNavServicesDialogs::LFileChooser   chooser;
    
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

#ifdef USE_PROFILES

// ---------------------------------------------------------------------------
//  CBrowserApp : nsISupports
// ---------------------------------------------------------------------------

NS_IMPL_ISUPPORTS2(CBrowserApp, nsIObserver, nsISupportsWeakReference);

// ---------------------------------------------------------------------------
//  CBrowserApp : nsIObserver
// ---------------------------------------------------------------------------

NS_IMETHODIMP CBrowserApp::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData)
{
    #define CLOSE_WINDOWS_ON_SWITCH 1

    nsresult rv = NS_OK;
    
    if (!nsCRT::strcmp(aTopic, "profile-approve-change"))
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
    else if (!nsCRT::strcmp(aTopic, "profile-change-teardown"))
    {
        // Close all open windows. Alternatively, we could just call CBrowserWindow::Stop()
        // on each. Either way, we have to stop all network activity on this phase.
        
        TArrayIterator<LCommander*> iterator(mSubCommanders, LArrayIterator::from_End);
        LCommander*     theSub;
        while (iterator.Previous(theSub)) {
            CBrowserWindow *browserWindow = dynamic_cast<CBrowserWindow*>(theSub);
            if (browserWindow) {
                //browserWindow->Stop();
                mSubCommanders.RemoveItemsAt(1, iterator.GetCurrentIndex());
                delete browserWindow;
            }
        }
    }
    else if (!nsCRT::strcmp(aTopic, "profile-after-change"))
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

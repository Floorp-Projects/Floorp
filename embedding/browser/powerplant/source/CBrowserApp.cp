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
#include "nsNetUtil.h"
#include "nsIWindowWatcher.h"
#include "nsIDOMWindow.h"
#include "nsIDownload.h"
#include "nsCRT.h"

#if defined(__MWERKS__) && !defined(__MACH__)
#include "macstdlibextras.h"
#include "SIOUX.h"
#endif

#include <TextServices.h>

#ifdef USE_PROFILES
#include "CProfileManager.h"
#include "nsIProfileChangeStatus.h"
#else
#include "nsProfileDirServiceProvider.h"
#endif

#ifdef SHARED_PROFILE
#include "nsIProfileSharingSetup.h"
#define kAppDataFolderName NS_LITERAL_STRING("PPEmbed Suite")
#else
#define kAppDataFolderName NS_LITERAL_STRING("PPEmbed")
#endif

// ===========================================================================
//      ¥ Main Program
// ===========================================================================

int main()
{
                                
    SetDebugThrow_(PP_PowerPlant::debugAction_Alert);   // Set Debugging options
    SetDebugSignal_(PP_PowerPlant::debugAction_Alert);

#ifdef POWERPLANT_IS_FRAMEWORK
    // A framework's Resource Mgr resources must be opened explicitly.
    CFBundleRef powerplantBundle = ::CFBundleGetBundleWithIdentifier(
                                      CFSTR("org.mozilla.PowerPlant"));
    SInt16 powerPlantResRefNum = -1;
    if (powerplantBundle) {
      powerPlantResRefNum = ::CFBundleOpenBundleResourceMap(powerplantBundle);
      ::CFRelease(powerplantBundle);
    }
#endif

    PP_PowerPlant::InitializeHeap(3);       // Initialize Memory Manager
                                            // Parameter is number of Master Pointer
                                            // blocks to allocate
    
    PP_PowerPlant::UQDGlobals::InitializeToolbox();
    
#if defined(__MWERKS__) && !TARGET_CARBON
    ::InitializeSIOUX(false);
#endif

#if !TARGET_CARBON
    new PP_PowerPlant::LGrowZone(20000);    // Install a GrowZone function to catch low memory situations.      
    ::InitTSMAwareApplication();
#endif
    
    {
        CBrowserApp theApp;         // create instance of your application
        theApp.Run();
    }

#if !TARGET_CARBON  
    ::CloseTSMAwareApplication();
#endif
    
    return 0;
}


// ===========================================================================
//  class CStartupTask
// 
//  A one-shot repeater which calls the application method to handle startup.
// ===========================================================================

class CStartUpTask : public LPeriodical
{
public:
    CStartUpTask(CBrowserApp *aBrowserApp) :
        mBrowserApp(aBrowserApp)
    {
        StartRepeating();
    }
    
    ~CStartUpTask() { }
    
    void SpendTime(const EventRecord& inMacEvent)
    {
        StopRepeating();
        mBrowserApp->OnStartUp();
        delete this;
    }
private:
    CBrowserApp     *mBrowserApp;
};

// ---------------------------------------------------------------------------
//      ¥ CBrowserApp
// ---------------------------------------------------------------------------
//  Constructor

CBrowserApp::CBrowserApp()
{

#ifdef USE_PROFILES
    mRefCnt = 1;
#else
    mProfDirServiceProvider = nsnull;
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
   
   CAppFileLocationProvider *fileLocProvider = new CAppFileLocationProvider(kAppDataFolderName);
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
#ifdef USE_PROFILES    
    nsCOMPtr<nsIProfile> profileService = 
        do_GetService(NS_PROFILE_CONTRACTID, &rv);
    if (profileService)
      profileService->ShutDownCurrentProfile(nsIProfile::SHUTDOWN_PERSIST);
#else
    if (mProfDirServiceProvider) {
      mProfDirServiceProvider->Shutdown();
      NS_RELEASE(mProfDirServiceProvider);
    }
#endif

   NS_TermEmbedding();
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
//      ¥ Initialize
// ---------------------------------------------------------------------------
//  Initializes profile manager or directory service provider. If the user has
//  chosen to be prompted to choose a profile at startup so the profile dialog
//  appears and they cancel from that dialog, startup will terminate.

void
CBrowserApp::Initialize()
{
    nsresult rv;

#ifdef SHARED_PROFILE    
    CFBundleRef appBundle = CFBundleGetMainBundle();
    ThrowIfNil_(appBundle);

    nsAutoString suiteMemberStr;
    // We don't get an owning reference here, so no CFRelease.
    CFStringRef bundleStrRef = (CFStringRef)CFBundleGetValueForInfoDictionaryKey(
                                appBundle, CFSTR("ProfilesSuiteMemberName"));
                                
    if (bundleStrRef && (CFGetTypeID(bundleStrRef) == CFStringGetTypeID())) {
        UniChar buffer[256];
        CFIndex strLen = CFStringGetLength(bundleStrRef);
        ThrowIf_(strLen >= (sizeof(buffer) / sizeof(buffer[0])));
        CFStringGetCharacters(bundleStrRef, CFRangeMake(0, strLen), buffer);
        buffer[strLen] = 0;
        suiteMemberStr.Assign(static_cast<PRUnichar*>(buffer));
    }
    ThrowIf_(suiteMemberStr.IsEmpty());
    
    nsCOMPtr<nsIProfileSharingSetup> sharingSetup =
        do_GetService("@mozilla.org/embedcomp/profile-sharing-setup;1");
    ThrowIfNil_(sharingSetup);
    rv = sharingSetup->EnableSharing(suiteMemberStr);
    ThrowIfError_(rv);
#endif
        
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
    // to make an nsProfileDirServiceProvider. This will provide the same file
    // locations as the profile service but always within the specified folder.
    
    nsCOMPtr<nsIFile> appDataDir;
    rv = NS_GetSpecialDirectory(NS_APP_APPLICATION_REGISTRY_DIR, getter_AddRefs(appDataDir));
    ThrowIfNil_(appDataDir);
    
    nsCOMPtr<nsProfileDirServiceProvider> profDirServiceProvider;
    NS_NewProfileDirServiceProvider(PR_TRUE, getter_AddRefs(profDirServiceProvider));
    ThrowIfNil_(profDirServiceProvider);
    rv = profDirServiceProvider->Register();
    ThrowIfError_(rv);
    
    nsCOMPtr<nsILocalFile> localAppDataDir(do_QueryInterface(appDataDir));
    rv = profDirServiceProvider->SetProfileDir(localAppDataDir);
    ThrowIfError_(rv);
    NS_ADDREF(mProfDirServiceProvider = profDirServiceProvider);
#endif

    // Now that we know profile selection wasn't canceled and we're gonna run...
    CStartUpTask *startupTask = new CStartUpTask(this);
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
                    {
                        CBrowserApp *theApp = reinterpret_cast<CBrowserApp*>(userData);
                        theApp->ObeyCommand(PP_PowerPlant::cmd_Preferences, nsnull);
                        result = noErr;
                        break;
                    }
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

// ---------------------------------------------------------------------------
//  CBrowserApp::DoStartUp
//
//  Called from CStartUpTask. We must use this INSTEAD of LApplication::StartUp.
//  Reason is, LApplication::StartUp happens in response to the open application AE
//  which is processed as soon as we process any events - as in while the profile
//  manager dialog is up :-/ 
// ---------------------------------------------------------------------------

void CBrowserApp::OnStartUp()
{
    ObeyCommand(PP_PowerPlant::cmd_New, nil);
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

NS_IMPL_ISUPPORTS2(CBrowserApp, nsIObserver, nsISupportsWeakReference)

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

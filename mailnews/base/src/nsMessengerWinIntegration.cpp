/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Bhuvan Racham <racham@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <windows.h>
#include <shellapi.h>

#include "nsMessengerWinIntegration.h"
#include "nsIMsgAccountManager.h"
#include "nsIMsgMailSession.h"
#include "nsIMsgIncomingServer.h"
#include "nsIMsgIdentity.h"
#include "nsIMsgAccount.h"
#include "nsIRDFResource.h"
#include "nsIMsgFolder.h"
#include "nsCOMPtr.h"
#include "nsMsgBaseCID.h"
#include "nsMsgFolderFlags.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIProfile.h"
#include "nsIDirectoryService.h"

#include "nsIWindowMediator.h"
#include "nsIDOMWindowInternal.h"
#include "nsPIDOMWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocShell.h"
#include "nsIBaseWindow.h"
#include "nsIWidget.h"

#include "nsIMessengerWindowService.h"
#include "prprf.h"
#include "nsIWeakReference.h"
#include "nsIStringBundle.h"
#include "nsIAlertsService.h"

#define XP_SHSetUnreadMailCounts "SHSetUnreadMailCountW"
#define XP_SHEnumerateUnreadMailAccounts "SHEnumerateUnreadMailAccountsW"
#define ShellNotifyWideVersion "Shell_NotifyIconW"
#define UNREADMAILNODEKEY "Software\\Microsoft\\Windows\\CurrentVersion\\UnreadMail\\"
#define SHELL32_DLL NS_LITERAL_CSTRING("shell32.dll")
#define DOUBLE_QUOTE "\""
#define PROFILE_COMMANDLINE_ARG " -p "
#define MAIL_COMMANDLINE_ARG " -mail"
#define TIMER_INTERVAL_PREF "mail.windows_xp_integration.unread_count_interval"
#define IDI_MAILBIFF 101

#define NEW_MAIL_ALERT_ICON "chrome://messenger/skin/icons/new-mail-alert.png"
#define SHOW_ALERT_PREF "mail.biff.show_alert"

// since we are including windows.h in this file, undefine get user name....
#ifdef GetUserName
#undef GetUserName
#endif

// begin shameless copying from nsNativeAppSupportWin
HWND hwndForDOMWindow( nsISupports *window ) 
{
  nsCOMPtr<nsIScriptGlobalObject> ppScriptGlobalObj( do_QueryInterface(window) );
  if ( !ppScriptGlobalObj )
      return 0;
  
  nsCOMPtr<nsIBaseWindow> ppBaseWindow =
      do_QueryInterface( ppScriptGlobalObj->GetDocShell() );
  if (!ppBaseWindow) return 0;

  nsCOMPtr<nsIWidget> ppWidget;
  ppBaseWindow->GetMainWidget( getter_AddRefs( ppWidget ) );

  return (HWND)( ppWidget->GetNativeData( NS_NATIVE_WIDGET ) );
}

static void activateWindow( nsIDOMWindowInternal *win ) 
{
  // Try to get native window handle.
  HWND hwnd = hwndForDOMWindow( win );
  if ( hwnd ) 
  {
    // Restore the window if it is minimized.
    if ( ::IsIconic( hwnd ) ) 
      ::ShowWindow( hwnd, SW_RESTORE );
    // Use the OS call, if possible.
    ::SetForegroundWindow( hwnd );
  } else // Use internal method.  
    win->Focus();
}
// end shameless copying from nsNativeAppWinSupport.cpp

static void openMailWindow(const PRUnichar * aMailWindowName, const char * aFolderUri)
{
  nsCOMPtr<nsIWindowMediator> mediator ( do_GetService(NS_WINDOWMEDIATOR_CONTRACTID) );
  if (!mediator)
    return;

  nsCOMPtr<nsIDOMWindowInternal> domWindow;
  mediator->GetMostRecentWindow(aMailWindowName, getter_AddRefs(domWindow));
  if (domWindow)
  {
    if (aFolderUri)
    {
      nsCOMPtr<nsPIDOMWindow> piDOMWindow(do_QueryInterface(domWindow));
      if (piDOMWindow)
      {
        nsCOMPtr<nsISupports> xpConnectObj;
        piDOMWindow->GetObjectProperty(NS_LITERAL_STRING("MsgWindowCommands").get(), getter_AddRefs(xpConnectObj));
        nsCOMPtr<nsIMsgWindowCommands> msgWindowCommands = do_QueryInterface(xpConnectObj);
        if (msgWindowCommands)
          msgWindowCommands->SelectFolder(aFolderUri);
      }
    }

    activateWindow(domWindow);
  }
  else
  {
    // the user doesn't have a mail window open already so open one for them...
    nsCOMPtr<nsIMessengerWindowService> messengerWindowService =
      do_GetService(NS_MESSENGERWINDOWSERVICE_CONTRACTID);
    // if we want to preselect the first account with new mail,
    // here is where we would try to generate a uri to pass in
    // (and add code to the messenger window service to make that work)
    if (messengerWindowService)
      messengerWindowService->OpenMessengerWindowWithUri(
                                "mail:3pane", aFolderUri, nsMsgKey_None);
  }
}

// Message window encapsulation.
struct MessageWindow 
{
    // ctor/dtor are simplistic
    MessageWindow() 
    {
      Create();
      // dummy code to force creation of our string
      const char * fakeName = mailWinName();
    }

    // Act like an HWND.
    operator HWND() 
    {
        return mHandle;
    }

    // for some reason WindowProc can't access the string literal "mail:3pane", we need to copy that string
    // into a static buffer outside of the WindowProc call. 
    static const char * mailWinName()
    {
      static char mailNameBuffer[128];
      static char *mMailWinName = 0;
      if ( !mMailWinName ) 
      {
        ::_snprintf( mailNameBuffer,
                     sizeof mailNameBuffer,
                     "%s",
                     "mail:3pane" );
        mMailWinName = mailNameBuffer;
      }
      return mMailWinName;
    }

    // Class name: appName + "MessageWindow"
    static const char *className() 
    {
      static char classNameBuffer[128];
      static char *mClassName = 0;
      if ( !mClassName ) 
      {
        ::_snprintf( classNameBuffer,
                     sizeof classNameBuffer,
                     "%s%s",
                     "MailBiffNotification",
                     "MessageWindow" );
        mClassName = classNameBuffer;
      }
      return mClassName;
    }

    // Create: Register class and create window.
    NS_IMETHOD Create() 
    {
      // Try to find window.
      mHandle = ::FindWindow( className(), 0 );
      if (mHandle) 
        return NS_OK;

      WNDCLASS classStruct = { 0,                          // style
                               &MessageWindow::WindowProc, // lpfnWndProc
                               0,                          // cbClsExtra
                               0,                          // cbWndExtra
                               0,                          // hInstance
                               0,                          // hIcon
                               0,                          // hCursor
                               0,                          // hbrBackground
                               0,                          // lpszMenuName
                               className() };              // lpszClassName

      // Register the window class.
      NS_ENSURE_TRUE( ::RegisterClass( &classStruct ), NS_ERROR_FAILURE );
      // Create the window.
      NS_ENSURE_TRUE( ( mHandle = ::CreateWindow( className(),
                                                  0,          // title
                                                  WS_CAPTION, // style
                                                  0,0,0,0,    // x, y, cx, cy
                                                  0,          // parent
                                                  0,          // menu
                                                  0,          // instance
                                                  0 ) ),      // create struct
                        NS_ERROR_FAILURE );
      return NS_OK;
    }

    // Window proc.
    static long CALLBACK WindowProc( HWND msgWindow, UINT msg, WPARAM wp, LPARAM lp ) 
    {
      if ( msg == WM_USER ) 
      {
         if ( lp == WM_LBUTTONDBLCLK ) 
         {
           nsAutoString mailName;
           mailName.AssignWithConversion(mailWinName());
           openMailWindow(mailName.get(), nsnull);
         }
      }
     
      return TRUE;
    }

private:
    HWND mHandle;
}; // struct MessageWindow


nsMessengerWinIntegration::nsMessengerWinIntegration()
{
  mDefaultServerAtom = do_GetAtom("DefaultServer");
  mTotalUnreadMessagesAtom = do_GetAtom("TotalUnreadMessages");

  mFirstTimeFolderUnreadCountChanged = PR_TRUE;
  mInboxURI = nsnull;
  mEmail = nsnull;
  mStoreUnreadCounts = PR_FALSE; 
  mIntervalTime = 0;

  mBiffStateAtom = do_GetAtom("BiffState");
  mBiffIconVisible = PR_FALSE;
  mSuppressBiffIcon = PR_FALSE;
  mAlertInProgress = PR_FALSE;
  mBiffIconInitialized = PR_FALSE;
  mUseWideCharBiffIcon = PR_FALSE;
  NS_NewISupportsArray(getter_AddRefs(mFoldersWithNewMail));
}

nsMessengerWinIntegration::~nsMessengerWinIntegration()
{
  if (mUnreadCountUpdateTimer) {
    mUnreadCountUpdateTimer->Cancel();
    mUnreadCountUpdateTimer = nsnull;
  }

  // one last attempt, update the registry
  nsresult rv = UpdateRegistryWithCurrent();
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to update registry on shutdown");
  
  CRTFREEIF(mInboxURI);
  CRTFREEIF(mEmail);

  DestroyBiffIcon(); 
}

NS_IMPL_ADDREF(nsMessengerWinIntegration)
NS_IMPL_RELEASE(nsMessengerWinIntegration)

NS_INTERFACE_MAP_BEGIN(nsMessengerWinIntegration)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIMessengerOSIntegration)
   NS_INTERFACE_MAP_ENTRY(nsIMessengerOSIntegration)
   NS_INTERFACE_MAP_ENTRY(nsIFolderListener)
   NS_INTERFACE_MAP_ENTRY(nsIAlertListener)
NS_INTERFACE_MAP_END


nsresult
nsMessengerWinIntegration::ResetCurrent()
{
  CRTFREEIF(mInboxURI);
  CRTFREEIF(mEmail);

  mCurrentUnreadCount = -1;
  mLastUnreadCountWrittenToRegistry = -1;

  mDefaultAccountMightHaveAnInbox = PR_TRUE;
  return NS_OK;
}

NOTIFYICONDATA nsMessengerWinIntegration::mAsciiBiffIconData = { sizeof(NOTIFYICONDATA),
                                                    0,
                                                    2,
                                                    NIF_ICON | NIF_MESSAGE | NIF_TIP,
                                                    WM_USER,
                                                    0,
                                                    0 };

NOTIFYICONDATAW nsMessengerWinIntegration::mWideBiffIconData = { sizeof(NOTIFYICONDATAW),
                                                    0,
                                                    2,
                                                    NIF_ICON | NIF_MESSAGE | NIF_TIP,
                                                    WM_USER,
                                                    0,
                                                    0 };

#ifdef MOZ_THUNDERBIRD
#ifdef MOZ_STATIC_BUILD
#define MAIL_DLL_NAME NULL
#else
#define MAIL_DLL_NAME "mail.dll"
#endif
#else
#define MAIL_DLL_NAME "msgbase.dll"
#endif

void nsMessengerWinIntegration::InitializeBiffStatusIcon()
{
  // initialize our biff status bar icon 
  nsresult rv = NS_OK; 
  MessageWindow msgWindow;

  if (mUseWideCharBiffIcon)
  {
    mWideBiffIconData.hWnd = (HWND) msgWindow;
    mWideBiffIconData.hIcon =  ::LoadIcon( ::GetModuleHandle( MAIL_DLL_NAME ), MAKEINTRESOURCE(IDI_MAILBIFF) );
    mWideBiffIconData.szTip[0] = 0;
  }
  else
  {
    mAsciiBiffIconData.hWnd = (HWND) msgWindow;
    mAsciiBiffIconData.hIcon =  ::LoadIcon( ::GetModuleHandle( MAIL_DLL_NAME ), MAKEINTRESOURCE(IDI_MAILBIFF) );
    mAsciiBiffIconData.szTip[0] = 0;
  }

  mBiffIconInitialized = PR_TRUE;
}

nsresult
nsMessengerWinIntegration::Init()
{
  nsresult rv;

  // get pref service
  nsCOMPtr<nsIPref> prefService;
  prefService = do_GetService(NS_PREF_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  // first check the timer value
  PRInt32 timerInterval;
  prefService->GetIntPref(TIMER_INTERVAL_PREF, &timerInterval);

  // return if the timer value is negative or ZERO
  if (timerInterval > 0) 
  { 
    // set the interval for timer. 
    // multiply the value extracted (in seconds) from prefs 
    // with 1000 to convert the interval into milliseconds
    mIntervalTime = timerInterval * 1000; 
  }

  // get directory service to build path for shell dll
  nsCOMPtr<nsIProperties> directoryService = 
    do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  // get path strings needed for unread mail count update
  nsCOMPtr<nsIFile> systemDir;
  rv = directoryService->Get(NS_OS_SYSTEM_DIR, 
                             NS_GET_IID(nsIFile), 
                             getter_AddRefs(systemDir));
  NS_ENSURE_SUCCESS(rv,rv);

  // get path to shell dll.
  nsCAutoString shellFile;
  rv = systemDir->GetNativePath(shellFile);
  NS_ENSURE_SUCCESS(rv,rv);

  mShellDllPath.Assign(shellFile + NS_LITERAL_CSTRING("\\") + SHELL32_DLL);

  // load shell dll. If no such dll found, return 
  HMODULE hModule = ::LoadLibrary(mShellDllPath.get());
  if (!hModule)
    return NS_OK;

  // get process addresses for the unerad mail count functions
  if (hModule) {
    mSHSetUnreadMailCount = (fnSHSetUnreadMailCount)GetProcAddress(hModule, XP_SHSetUnreadMailCounts);
    mSHEnumerateUnreadMailAccounts = (fnSHEnumerateUnreadMailAccounts)GetProcAddress(hModule, XP_SHEnumerateUnreadMailAccounts);
    mShellNotifyWideChar = (fnShellNotifyW)GetProcAddress(hModule, ShellNotifyWideVersion);
    if (mShellNotifyWideChar)
       mUseWideCharBiffIcon = PR_TRUE; // this version of the shell supports I18N friendly ShellNotify routines.
  }

  // if failed to get either of the process addresses, this is not XP platform
  // so we aren't storing unread counts
  if (mSHSetUnreadMailCount && mSHEnumerateUnreadMailAccounts && (PRUint32) mIntervalTime)
    mStoreUnreadCounts = PR_TRUE; 

  nsCOMPtr <nsIMsgAccountManager> accountManager = 
    do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  // because we care if the default server changes
  rv = accountManager->AddRootFolderListener(this);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr<nsIMsgMailSession> mailSession = do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  // because we care if the unread total count changes
  rv = mailSession->AddFolderListener(this, nsIFolderListener::boolPropertyChanged | nsIFolderListener::intPropertyChanged);
  NS_ENSURE_SUCCESS(rv,rv);

  if (mStoreUnreadCounts)
  {
  // get current profile name to fill in commandliner. 
    nsCOMPtr<nsIProfile> profileService = do_GetService(NS_PROFILE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  profileService->GetCurrentProfile(getter_Copies(mProfileName));

  // get application path 
  char appPath[_MAX_PATH] = {0};
  GetModuleFileName(nsnull, appPath, sizeof(appPath));
  WCHAR wideFormatAppPath[_MAX_PATH*2] = {0};
  MultiByteToWideChar(CP_ACP, 0, appPath, strlen(appPath), wideFormatAppPath, _MAX_PATH*2);
  mAppName.Assign((PRUnichar *)wideFormatAppPath);

  rv = ResetCurrent();
  NS_ENSURE_SUCCESS(rv,rv);

  rv = SetupUnreadCountUpdateTimer();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMessengerWinIntegration::OnItemPropertyChanged(nsIRDFResource *, nsIAtom *, char const *, char const *)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerWinIntegration::OnItemUnicharPropertyChanged(nsIRDFResource *, nsIAtom *, const PRUnichar *, const PRUnichar *)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerWinIntegration::OnItemRemoved(nsIRDFResource *, nsISupports *)
{
  return NS_OK;
}

nsresult nsMessengerWinIntegration::GetStringBundle(nsIStringBundle **aBundle)
{
  nsresult rv = NS_OK;
  NS_ENSURE_ARG_POINTER(aBundle);
  nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  nsCOMPtr<nsIStringBundle> bundle;
  if (bundleService && NS_SUCCEEDED(rv))
    bundleService->CreateBundle("chrome://messenger/locale/messenger.properties", getter_AddRefs(bundle));
  NS_IF_ADDREF(*aBundle = bundle);
  return rv;
}

nsresult nsMessengerWinIntegration::ShowAlertMessage(const PRUnichar * aAlertTitle, const PRUnichar * aAlertText, const char * aFolderURI)
{
  nsresult rv;
  
  // if we are already in the process of showing an alert, don't try to show another....
  if (mAlertInProgress) 
    return NS_OK; 

  nsCOMPtr<nsIPref> prefService;
  prefService = do_GetService(NS_PREF_CONTRACTID, &rv);  
  PRBool showAlert = PR_TRUE; 
  
  if (prefService)
    prefService->GetBoolPref(SHOW_ALERT_PREF, &showAlert);
  
  if (showAlert)
  {
    nsCOMPtr<nsIAlertsService> alertsService (do_GetService(NS_ALERTSERVICE_CONTRACTID, &rv));
    if (NS_SUCCEEDED(rv))
    {
      nsCOMPtr<nsIAlertListener> alertListener (do_QueryInterface(NS_STATIC_CAST(nsIMessengerOSIntegration*, this))); 
      rv = alertsService->ShowAlertNotification(NEW_MAIL_ALERT_ICON, aAlertTitle, aAlertText, PR_TRUE, 
                                                NS_ConvertASCIItoUCS2(aFolderURI).get(), alertListener); 
      mAlertInProgress = PR_TRUE;
    }
  }

  if (!showAlert || NS_FAILED(rv)) // go straight to showing the system tray icon.
    OnAlertFinished(nsnull);

  return rv;
}

NS_IMETHODIMP nsMessengerWinIntegration::OnAlertFinished(const PRUnichar * aAlertCookie)
{
  // okay we are done showing the alert....now put an icon in the system tray
  if (!mSuppressBiffIcon)
  {
    GenericShellNotify(NIM_ADD);
    mBiffIconVisible = PR_TRUE;
  }

  mSuppressBiffIcon = PR_FALSE;
  mAlertInProgress = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsMessengerWinIntegration::OnAlertClickCallback(const PRUnichar * aAlertCookie)
{
  // make sure we don't insert the icon in the system tray since the user clicked on the alert.
  mSuppressBiffIcon = PR_TRUE;
  
  nsXPIDLCString folderURI;
  GetFirstFolderWithNewMail(getter_Copies(folderURI));

  openMailWindow(NS_LITERAL_STRING("mail:3pane").get(), folderURI);
 
  return NS_OK;
}

void nsMessengerWinIntegration::FillToolTipInfo()
{
  // iterate over all the folders in mFoldersWithNewMail
  nsXPIDLString accountName;
  nsXPIDLCString hostName; 
  nsAutoString toolTipText;
  nsAutoString animatedAlertText;
  nsCOMPtr<nsISupports> supports;
  nsCOMPtr<nsIMsgFolder> folder;
  nsCOMPtr<nsIWeakReference> weakReference;
  PRInt32 numNewMessages = 0;

  PRUint32 count = 0;
  mFoldersWithNewMail->Count(&count);
  PRUint32 maxTooltipSize = GetToolTipSize();

  for (PRUint32 index = 0; index < count; index++)
  {
    supports = getter_AddRefs(mFoldersWithNewMail->ElementAt(index));
    weakReference = do_QueryInterface(supports);
    folder = do_QueryReferent(weakReference);
    if (folder)
    {
      folder->GetPrettiestName(getter_Copies(accountName));

      numNewMessages = 0;   
      folder->GetNumNewMessages(PR_TRUE, &numNewMessages);
      nsCOMPtr<nsIStringBundle> bundle; 
      GetStringBundle(getter_AddRefs(bundle));
      if (bundle)
      { 
        nsAutoString numNewMsgsText;     
        numNewMsgsText.AppendInt(numNewMessages);

        const PRUnichar *formatStrings[] =
        {
          numNewMsgsText.get(),       
        };
       
        nsXPIDLString finalText; 
        if (numNewMessages == 1)
          bundle->FormatStringFromName(NS_LITERAL_STRING("biffNotification_message").get(), formatStrings, 1, getter_Copies(finalText));
        else
          bundle->FormatStringFromName(NS_LITERAL_STRING("biffNotification_messages").get(), formatStrings, 1, getter_Copies(finalText));

        // the alert message is special...we actually only want to show the first account with 
        // new mail in the alert. 
        if (animatedAlertText.IsEmpty()) // if we haven't filled in the animated alert text yet
          animatedAlertText = finalText;

        // only add this new string if it will fit without truncation....
        if (maxTooltipSize >= toolTipText.Length() + accountName.Length() + finalText.Length() + 2)
        {
    	    if (index > 0)
            toolTipText.Append(NS_LITERAL_STRING("\n").get());
          toolTipText.Append(accountName);
          toolTipText.AppendLiteral(" ");
		      toolTipText.Append(finalText);
        }
      } // if we got a bundle
    } // if we got a folder
  } // for each folder

  SetToolTipStringOnIconData(toolTipText.get());

  if (!mBiffIconVisible)
  {
    ShowAlertMessage(accountName, animatedAlertText.get(), "");
  }
  else
   GenericShellNotify( NIM_MODIFY);
}

// get the first top level folder which we know has new mail, then enumerate over all the subfolders
// looking for the first real folder with new mail. Return the folderURI for that folder.
nsresult nsMessengerWinIntegration::GetFirstFolderWithNewMail(char ** aFolderURI)
{
  nsresult rv;
  NS_ENSURE_TRUE(mFoldersWithNewMail, NS_ERROR_FAILURE); 

  nsCOMPtr<nsISupports> supports;
  nsCOMPtr<nsIMsgFolder> folder;
  nsCOMPtr<nsIWeakReference> weakReference;
  PRInt32 numNewMessages = 0;

  PRUint32 count = 0;
  mFoldersWithNewMail->Count(&count);

  if (!count)  // kick out if we don't have any folders with new mail
    return NS_OK;

  supports = getter_AddRefs(mFoldersWithNewMail->ElementAt(0));
  weakReference = do_QueryInterface(supports);
  folder = do_QueryReferent(weakReference);
  
  if (folder)
  {
    PRUint32 biffState = nsIMsgFolder::nsMsgBiffState_NoMail; 
    nsCOMPtr<nsIMsgFolder> msgFolder;
    // enumerate over the folders under this root folder till we find one with new mail....
    nsCOMPtr<nsISupportsArray> allFolders;
    NS_NewISupportsArray(getter_AddRefs(allFolders));
    rv = folder->ListDescendents(allFolders);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIEnumerator> enumerator;
    allFolders->Enumerate(getter_AddRefs(enumerator));
    if (enumerator)
    {
      nsCOMPtr<nsISupports> supports;
      nsresult more = enumerator->First();
      while (NS_SUCCEEDED(more))
      {
        rv = enumerator->CurrentItem(getter_AddRefs(supports));
        if (supports)
        {			
          msgFolder = do_QueryInterface(supports, &rv);
          if (msgFolder)
          {
            numNewMessages = 0;   
            msgFolder->GetNumNewMessages(PR_FALSE, &numNewMessages);
            if (numNewMessages)
              break; // kick out of the while loop
            more = enumerator->Next();
          }
        } // if we have a folder
      }  // if we have more potential folders to enumerate
    }  // if enumerator
    
    if (msgFolder)
      msgFolder->GetURI(aFolderURI);
  }

  return NS_OK;
}

void nsMessengerWinIntegration::DestroyBiffIcon()
{
  GenericShellNotify(NIM_DELETE); 
  // Don't call DestroyIcon().  see http://bugzilla.mozilla.org/show_bug.cgi?id=134745
}

PRUint32 nsMessengerWinIntegration::GetToolTipSize()
{
  if (mUseWideCharBiffIcon)
    return (sizeof(mWideBiffIconData.szTip)/sizeof(mWideBiffIconData.szTip[0]));
  else
    return (sizeof(mAsciiBiffIconData.szTip));
}

void nsMessengerWinIntegration::SetToolTipStringOnIconData(const PRUnichar * aToolTipString)
{
  if (!aToolTipString) return;

  PRUint32 toolTipBufSize = GetToolTipSize();
  
  if (mUseWideCharBiffIcon)
  {
    ::wcsncpy( mWideBiffIconData.szTip, aToolTipString, toolTipBufSize);
    if (wcslen(aToolTipString) >= toolTipBufSize)
      mWideBiffIconData.szTip[toolTipBufSize - 1] = 0;
  }
  else
  {
    nsCString asciiToolTip;
    asciiToolTip.AssignWithConversion(aToolTipString);
    ::strncpy( mAsciiBiffIconData.szTip, asciiToolTip.get(), GetToolTipSize() );
    if (asciiToolTip.Length() >= toolTipBufSize)
      mAsciiBiffIconData.szTip[toolTipBufSize - 1] = 0;
  }
}

void nsMessengerWinIntegration::GenericShellNotify(DWORD aMessage)
{
  if (mUseWideCharBiffIcon)
  {
    BOOL res = mShellNotifyWideChar( aMessage, &mWideBiffIconData );
    if (!res)
      RevertToNonUnicodeShellAPI(); // oops we don't really implement the unicode shell apis...fall back.
    else
      return; 
  }
  
  ::Shell_NotifyIcon( aMessage, &mAsciiBiffIconData ); 
}

// some flavors of windows define ShellNotifyW but when you actually try to use it,
// they return an error. In this case, we'll have a routine which converts us over to the 
// default ASCII version. 
void nsMessengerWinIntegration::RevertToNonUnicodeShellAPI()
{
  mUseWideCharBiffIcon = PR_FALSE;

  // now initialize the ascii shell notify struct
  InitializeBiffStatusIcon();

  // now we need to copy over any left over tool tip strings
  if (mWideBiffIconData.szTip)
  {
    const PRUnichar * oldTooltipString = mWideBiffIconData.szTip;
    SetToolTipStringOnIconData(oldTooltipString);
  }
}

NS_IMETHODIMP
nsMessengerWinIntegration::OnItemPropertyFlagChanged(nsISupports *item, nsIAtom *property, PRUint32 oldFlag, PRUint32 newFlag)
{
  nsresult rv = NS_OK;

  if (!mBiffIconInitialized)
    InitializeBiffStatusIcon(); 
    
  // if we got new mail show a icon in the system tray
	if (mBiffStateAtom == property && mFoldersWithNewMail)
	{
    nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(item);
    NS_ENSURE_TRUE(folder, NS_OK);

		if (newFlag == nsIMsgFolder::nsMsgBiffState_NewMail) 
    {
      // if the icon is not already visible, only show a system tray icon iff 
      // we are performing biff (as opposed to the user getting new mail)
      if (!mBiffIconVisible)
      {
        PRBool performingBiff = PR_FALSE;
        nsCOMPtr<nsIMsgIncomingServer> server;
        folder->GetServer(getter_AddRefs(server));
        if (server)
          server->GetPerformingBiff(&performingBiff);
        if (!performingBiff) 
          return NS_OK; // kick out right now...
      }
      nsCOMPtr<nsIWeakReference> weakFolder = do_GetWeakReference(folder); 

      // remove the element if it is already in the array....
      PRUint32 count = 0;
      PRUint32 index = 0; 
      mFoldersWithNewMail->Count(&count);
      nsCOMPtr<nsISupports> supports;
      nsCOMPtr<nsIMsgFolder> oldFolder;
      nsCOMPtr<nsIWeakReference> weakReference;
      for (index = 0; index < count; index++)
      {
        supports = getter_AddRefs(mFoldersWithNewMail->ElementAt(index));
        weakReference = do_QueryInterface(supports);
        oldFolder = do_QueryReferent(weakReference);
        if (oldFolder == folder) // if they point to the same folder
          break;
        oldFolder = nsnull;
      }

      if (oldFolder)
        mFoldersWithNewMail->ReplaceElementAt(weakFolder, index);
      else
        mFoldersWithNewMail->AppendElement(weakFolder);
      // now regenerate the tooltip
      FillToolTipInfo();    
    }
    else if (newFlag == nsIMsgFolder::nsMsgBiffState_NoMail)
    {
      // we are always going to remove the icon whenever we get our first no mail
      // notification. 
      
      // avoid a race condition where we are told to remove the icon before we've actually
      // added it to the system tray. This happens when the user reads a new message before
      // the animated alert has gone away.
      if (mAlertInProgress)
        mSuppressBiffIcon = PR_TRUE;

      mFoldersWithNewMail->Clear(); 
      if (mBiffIconVisible) 
      {
        mBiffIconVisible = PR_FALSE;
        GenericShellNotify(NIM_DELETE); 
      }
    }
  } // if the biff property changed
  
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerWinIntegration::OnItemAdded(nsIRDFResource *, nsISupports *)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerWinIntegration::OnItemBoolPropertyChanged(nsIRDFResource *aItem,
                                                         nsIAtom *aProperty,
                                                         PRBool aOldValue,
                                                         PRBool aNewValue)
{
  if (aProperty == mDefaultServerAtom) {
    nsresult rv;

    // this property changes multiple times
    // on account deletion or when the user changes their
    // default account.  ResetCurrent() will set
    // mInboxURI to null, so we use that
    // to prevent us from attempting to remove 
    // something from the registry that has already been removed
    if (mInboxURI && mEmail) {
      rv = RemoveCurrentFromRegistry();
      NS_ENSURE_SUCCESS(rv,rv);
    }

    // reset so we'll go get the new default server next time
    rv = ResetCurrent();
    NS_ENSURE_SUCCESS(rv,rv);

    rv = UpdateUnreadCount();
    NS_ENSURE_SUCCESS(rv,rv);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerWinIntegration::OnItemEvent(nsIMsgFolder *, nsIAtom *)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerWinIntegration::OnItemIntPropertyChanged(nsIRDFResource *aItem, nsIAtom *aProperty, PRInt32 aOldValue, PRInt32 aNewValue)
{
  nsresult rv;

  if (!mStoreUnreadCounts) return NS_OK; // don't do anything here if we aren't storing unread counts...

  if (aProperty == mTotalUnreadMessagesAtom) {
    const char *itemURI = nsnull;
    rv = aItem->GetValueConst(&itemURI);
    NS_ENSURE_SUCCESS(rv,rv);

    // Today, if the user brings up the mail app and gets his/her mail and read 
    // couple of messages and quit the app before the first timer action is 
    // triggered, app will not get the opportunity to update the registry 
    // latest unread count thus displaying essentially the previous sessions's data. 
    // That is not desirable. So, we can avoid that situation by updating the 
    // registry first time the total unread count is changed. That will update 
    // the registry to reflect the first user action that alters unread mail count.
    // As the user reads some of the messages, the latest unread mail count 
    // is kept track of. Now, if the user quits before the first timer is triggered,
    // the registry is updated one last time via UpdateRegistryWithCurrent() as we will
    // have all information needed available to do so.
    if (mFirstTimeFolderUnreadCountChanged) {
      mFirstTimeFolderUnreadCountChanged = PR_FALSE;

      rv = UpdateUnreadCount();
      NS_ENSURE_SUCCESS(rv,rv);
    }

    if (itemURI && mInboxURI && !nsCRT::strcmp(itemURI, mInboxURI)) {
      mCurrentUnreadCount = aNewValue;
    }
  }
  return NS_OK;
}

void 
nsMessengerWinIntegration::OnUnreadCountUpdateTimer(nsITimer *timer, void *osIntegration)
{
  nsMessengerWinIntegration *winIntegration = (nsMessengerWinIntegration*)osIntegration;

  nsresult rv = winIntegration->UpdateUnreadCount();
  NS_ASSERTION(NS_SUCCEEDED(rv), "updating unread count failed");
}

nsresult
nsMessengerWinIntegration::RemoveCurrentFromRegistry()
{
  if (!mStoreUnreadCounts) return NS_OK; // don't do anything here if we aren't storing unread counts...

  // If Windows XP, open the registry and get rid of old account registry entries
  // If there is a email prefix, get it and use it to build the registry key.
  // Otherwise, just the email address will be the registry key.
  nsAutoString currentUnreadMailCountKey;
  if (!mEmailPrefix.IsEmpty()) {
    currentUnreadMailCountKey.Assign(mEmailPrefix);
    currentUnreadMailCountKey.AppendWithConversion(mEmail);
  }
  else {
    currentUnreadMailCountKey.AssignWithConversion(mEmail);
  }


  WCHAR registryUnreadMailCountKey[_MAX_PATH] = {0};
  // Enumerate through registry entries to delete the key matching 
  // currentUnreadMailCountKey
  int index = 0;
  while (SUCCEEDED(mSHEnumerateUnreadMailAccounts(HKEY_CURRENT_USER, 
                                                  index, 
                                                  registryUnreadMailCountKey, 
                                                  sizeof(registryUnreadMailCountKey))))
  {
    if (wcscmp(registryUnreadMailCountKey, currentUnreadMailCountKey.get())==0) {
      nsAutoString deleteKey;
      deleteKey.Assign(NS_LITERAL_STRING(UNREADMAILNODEKEY).get());
      deleteKey.Append(currentUnreadMailCountKey.get());

      if (!deleteKey.IsEmpty()) {
        // delete this key and berak out of the loop
        RegDeleteKey(HKEY_CURRENT_USER, 
                     NS_ConvertUCS2toUTF8(deleteKey).get());
        break;
      }
      else {
        index++;
      }
    }
    else {
      index++;
    }
  }
  return NS_OK;
}

nsresult 
nsMessengerWinIntegration::UpdateRegistryWithCurrent()
{
  if (!mStoreUnreadCounts) return NS_OK; // don't do anything here if we aren't storing unread counts...

  if (!mInboxURI || !mEmail) 
    return NS_OK;

  // only update the registry if the count has changed
  // and if the unread count is valid
  if ((mCurrentUnreadCount < 0) || (mCurrentUnreadCount == mLastUnreadCountWrittenToRegistry)) {
    return NS_OK;
  }

  // commandliner has to be built in the form of statement
  // which can be open the mailer app to the default user account
  // For given profile 'foo', commandliner will be built as 
  // ""<absolute path to application>" -p foo -mail" where absolute
  // path to application is extracted from mAppName
  nsAutoString commandLinerForAppLaunch;
  commandLinerForAppLaunch.Assign(NS_LITERAL_STRING(DOUBLE_QUOTE).get());
  commandLinerForAppLaunch.Append(mAppName.get());
  commandLinerForAppLaunch.Append(NS_LITERAL_STRING(DOUBLE_QUOTE).get());
  commandLinerForAppLaunch.Append(NS_LITERAL_STRING(PROFILE_COMMANDLINE_ARG).get());
  commandLinerForAppLaunch.Append(mProfileName.get());
  commandLinerForAppLaunch.Append(NS_LITERAL_STRING(MAIL_COMMANDLINE_ARG).get());

  if (!commandLinerForAppLaunch.IsEmpty())
  {
    nsAutoString pBuffer;

    if (!mEmailPrefix.IsEmpty()) {
      pBuffer.Assign(mEmailPrefix.get());					
      pBuffer.AppendWithConversion(mEmail);
    }
    else {
      pBuffer.AssignWithConversion(mEmail);
    }

    // Write the info into the registry
    HRESULT hr = mSHSetUnreadMailCount(pBuffer.get(), 
                                       mCurrentUnreadCount, 
                                       commandLinerForAppLaunch.get());
  }

  // do this last
  mLastUnreadCountWrittenToRegistry = mCurrentUnreadCount;

  return NS_OK;
}

nsresult 
nsMessengerWinIntegration::SetupInbox()
{
  nsresult rv;
  if (!mStoreUnreadCounts) return NS_OK; // don't do anything here if we aren't storing unread counts...

  // get default account
  nsCOMPtr <nsIMsgAccountManager> accountManager = 
    do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv); 
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr <nsIMsgAccount> account;
  rv = accountManager->GetDefaultAccount(getter_AddRefs(account));
  if (NS_FAILED(rv)) {
    // this can happen if we launch mail on a new profile
    // we don't have a default account yet
    mDefaultAccountMightHaveAnInbox = PR_FALSE;
    return NS_OK;
  }

  // get incoming server
  nsCOMPtr <nsIMsgIncomingServer> server;
  rv = account->GetIncomingServer(getter_AddRefs(server));
  NS_ENSURE_SUCCESS(rv,rv);

  nsXPIDLCString type;
  rv = server->GetType(getter_Copies(type));
  NS_ENSURE_SUCCESS(rv,rv);

  // we only care about imap and pop3
  if (!(nsCRT::strcmp(type.get(), "imap")) ||
      !(nsCRT::strcmp(type.get(), "pop3"))) {
    // imap and pop3 account should have an Inbox
    mDefaultAccountMightHaveAnInbox = PR_TRUE;

    // get the redirector type, use it to get the prefix
    // todo remove this extra copy
    nsXPIDLCString redirectorType;
    server->GetRedirectorType(getter_Copies(redirectorType));

    if (redirectorType) {
      nsCAutoString providerPrefixPref;
      providerPrefixPref.Assign("mail.");
      providerPrefixPref.Append(redirectorType);
      providerPrefixPref.Append(".unreadMailCountRegistryKeyPrefix");

      // get pref service
      nsCOMPtr<nsIPref> prefService;
      prefService = do_GetService(NS_PREF_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv,rv);

      nsXPIDLString prefixValue;
      prefService->CopyUnicharPref(providerPrefixPref.get(), getter_Copies(prefixValue));
      if (prefixValue.get())
        mEmailPrefix.Assign(prefixValue);
      else
        mEmailPrefix.Truncate(0);
    }
    else {
        mEmailPrefix.Truncate(0);
    }

    // Get user's email address
    nsCOMPtr<nsIMsgIdentity> identity;
    rv = account->GetDefaultIdentity(getter_AddRefs(identity));
    NS_ENSURE_SUCCESS(rv,rv);
 
    if (!identity)
      return NS_ERROR_FAILURE;
 
    rv = identity->GetEmail(&mEmail);
    NS_ENSURE_SUCCESS(rv,rv);
    
    nsCOMPtr<nsIMsgFolder> rootMsgFolder;
    rv = server->GetRootMsgFolder(getter_AddRefs(rootMsgFolder));
    NS_ENSURE_SUCCESS(rv,rv);
 
    if (!rootMsgFolder)
      return NS_ERROR_FAILURE;
 
    PRUint32 numFolders = 0;
    nsCOMPtr <nsIMsgFolder> inboxFolder;
    rv = rootMsgFolder->GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX, 1, &numFolders, getter_AddRefs(inboxFolder));
    NS_ENSURE_SUCCESS(rv,rv);
 
    if (!inboxFolder)
     return NS_ERROR_FAILURE;
 
    rv = inboxFolder->GetURI(&mInboxURI);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = inboxFolder->GetNumUnread(PR_FALSE, &mCurrentUnreadCount);
    NS_ENSURE_SUCCESS(rv,rv);
  }
  else {
    // the default account is valid, but it's not something 
    // that we expect to have an inbox.  (local folders, news accounts)
    // set this flag to avoid calling SetupInbox() every time
    // the timer goes off.
    mDefaultAccountMightHaveAnInbox = PR_FALSE;
  }

  return NS_OK;
}

nsresult
nsMessengerWinIntegration::UpdateUnreadCount()
{
  nsresult rv;
  if (!mStoreUnreadCounts) return NS_OK; // don't do anything here if we aren't storing unread counts...

  if (mDefaultAccountMightHaveAnInbox && !mInboxURI) {
    rv = SetupInbox();
    NS_ENSURE_SUCCESS(rv,rv);
  }
  
  rv = UpdateRegistryWithCurrent();
  NS_ENSURE_SUCCESS(rv,rv);

  return NS_OK;
}

nsresult
nsMessengerWinIntegration::SetupUnreadCountUpdateTimer()
{
  if (!mStoreUnreadCounts) return NS_OK; // don't do anything here if we aren't storing unread counts...
  PRUint32 timeInMSUint32 = (PRUint32) mIntervalTime;
  if (mUnreadCountUpdateTimer) {
    mUnreadCountUpdateTimer->Cancel();
  }

  mUnreadCountUpdateTimer = do_CreateInstance("@mozilla.org/timer;1");
  mUnreadCountUpdateTimer->InitWithFuncCallback(OnUnreadCountUpdateTimer, (void*)this, timeInMSUint32, 
                                                nsITimer::TYPE_REPEATING_SLACK);

  return NS_OK;
}


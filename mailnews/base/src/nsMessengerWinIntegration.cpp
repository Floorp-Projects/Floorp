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
 *   Howard Chu <hyc@symas.com>
 *   Jens Bannmann <jens.b@web.de>
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
#include "nsAppDirectoryServiceDefs.h"
#include "nsIDirectoryService.h"
#include "nsIWindowWatcher.h"
#include "nsIWindowMediator.h"
#include "nsIDOMWindowInternal.h"
#include "nsPIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIBaseWindow.h"
#include "nsIWidget.h"
#include "nsWidgetsCID.h"
#include "nsILookAndFeel.h"

#include "nsIMessengerWindowService.h"
#include "prprf.h"
#include "nsIWeakReference.h"
#include "nsIStringBundle.h"
#include "nsIAlertsService.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsISupportsPrimitives.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsNativeCharsetUtils.h"

// XXX test for this as long as there are still non-xul-app suite builds
#ifdef MOZ_XUL_APP
#include "nsToolkitCompsCID.h" 
#define PROFILE_COMMANDLINE_ARG " -profile "
#else
#include "nsIProfile.h"
#define PROFILE_COMMANDLINE_ARG " -p "
#endif

#define XP_SHSetUnreadMailCounts "SHSetUnreadMailCountW"
#define XP_SHEnumerateUnreadMailAccounts "SHEnumerateUnreadMailAccountsW"
#define ShellNotifyWideVersion "Shell_NotifyIconW"
#define NOTIFICATIONCLASSNAME "MailBiffNotificationMessageWindow"
#define UNREADMAILNODEKEY "Software\\Microsoft\\Windows\\CurrentVersion\\UnreadMail\\"
#define SHELL32_DLL NS_LITERAL_CSTRING("shell32.dll")
#define DOUBLE_QUOTE "\""
#define MAIL_COMMANDLINE_ARG " -mail"
#define IDI_MAILBIFF 101
#define UNREAD_UPDATE_INTERVAL	(20 * 1000)	// 20 seconds
#define ALERT_CHROME_URL "chrome://messenger/content/newmailalert.xul"
#define NEW_MAIL_ALERT_ICON "chrome://messenger/skin/icons/new-mail-alert.png"
#define SHOW_ALERT_PREF     "mail.biff.show_alert"
#define SHOW_TRAY_ICON_PREF "mail.biff.show_tray_icon"

// since we are including windows.h in this file, undefine get user name....
#ifdef GetUserName
#undef GetUserName
#endif

// begin shameless copying from nsNativeAppSupportWin
HWND hwndForDOMWindow( nsISupports *window ) 
{
  nsCOMPtr<nsPIDOMWindow> win( do_QueryInterface(window) );
  if ( !win )
      return 0;
  
  nsCOMPtr<nsIBaseWindow> ppBaseWindow =
      do_QueryInterface( win->GetDocShell() );
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
  nsresult rv;
  nsCOMPtr<nsIMsgMailSession> mailSession ( do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return;

  nsCOMPtr<nsIMsgWindow> topMostMsgWindow;
  rv = mailSession->GetTopmostMsgWindow(getter_AddRefs(topMostMsgWindow));
  if (topMostMsgWindow)
  {
    if (aFolderUri)
    {
      nsCOMPtr<nsIMsgWindowCommands> windowCommands;
      topMostMsgWindow->GetWindowCommands(getter_AddRefs(windowCommands));
      if (windowCommands)
        windowCommands->SelectFolder(aFolderUri);
    }
    nsCOMPtr<nsIDOMWindowInternal> domWindow;
    topMostMsgWindow->GetDomWindow(getter_AddRefs(domWindow));
    if (domWindow)
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

static void CALLBACK delayedSingleClick(HWND msgWindow, UINT msg, INT_PTR idEvent, DWORD dwTime)
{
  ::KillTimer(msgWindow, idEvent);

#ifdef MOZ_THUNDERBIRD
  // single clicks on the biff icon should re-open the alert notification
  nsresult rv = NS_OK;
  nsCOMPtr<nsIMessengerOSIntegration> integrationService = 
    do_GetService(NS_MESSENGEROSINTEGRATION_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv))
  {
    // we know we are dealing with the windows integration object    
    nsMessengerWinIntegration * winIntegrationService = NS_STATIC_CAST(nsMessengerWinIntegration*, 
                                                                       NS_STATIC_CAST(nsIMessengerOSIntegration*, integrationService.get()));
    winIntegrationService->ShowNewAlertNotification(PR_TRUE);
  }
#endif
}

// Window proc.
static long CALLBACK MessageWindowProc( HWND msgWindow, UINT msg, WPARAM wp, LPARAM lp ) 
{
  if (msg == WM_USER)
  {
    if (lp == WM_LBUTTONDOWN)
    {
      // the only way to tell a single left click 
      // from a double left click is to fire a timer which gets cleared if we end up getting
      // a WM_LBUTTONDBLK event.
      ::SetTimer(msgWindow, 1, GetDoubleClickTime(), (TIMERPROC) delayedSingleClick);
    } 
    else if (lp == WM_LBUTTONDBLCLK)
    {
      ::KillTimer(msgWindow, 1);
      NS_NAMED_LITERAL_STRING(mailName, "mail:3pane");
      openMailWindow(mailName.get(), nsnull);
    }
  }

  return TRUE;
}

static HWND msgWindow;

// Create: Register class and create window.
static nsresult Create() 
{
  if (msgWindow)
    return NS_OK;

  WNDCLASS classStruct = { 0,                          // style
                           &MessageWindowProc,         // lpfnWndProc
                           0,                          // cbClsExtra
                           0,                          // cbWndExtra
                           0,                          // hInstance
                           0,                          // hIcon
                           0,                          // hCursor
                           0,                          // hbrBackground
                           0,                          // lpszMenuName
                           NOTIFICATIONCLASSNAME };    // lpszClassName

  // Register the window class.
  NS_ENSURE_TRUE( ::RegisterClass( &classStruct ), NS_ERROR_FAILURE );
  // Create the window.
  NS_ENSURE_TRUE( msgWindow = ::CreateWindow( NOTIFICATIONCLASSNAME,
                                              0,          // title
                                              WS_CAPTION, // style
                                              0,0,0,0,    // x, y, cx, cy
                                              0,          // parent
                                              0,          // menu
                                              0,          // instance
                                              0 ),        // create struct
                  NS_ERROR_FAILURE );
  return NS_OK;
}


nsMessengerWinIntegration::nsMessengerWinIntegration()
{
  mDefaultServerAtom = do_GetAtom("DefaultServer");
  mTotalUnreadMessagesAtom = do_GetAtom("TotalUnreadMessages");

  mUnreadTimerActive = PR_FALSE;
  mInboxURI = nsnull;
  mEmail = nsnull;
  mStoreUnreadCounts = PR_FALSE; 

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
   NS_INTERFACE_MAP_ENTRY(nsIObserver)
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

NOTIFYICONDATA nsMessengerWinIntegration::sNativeBiffIconData = { sizeof(NOTIFYICONDATA),
                                                    0,
                                                    2,
                                                    NIF_ICON | NIF_MESSAGE | NIF_TIP,
                                                    WM_USER,
                                                    0,
                                                    0 };

NOTIFYICONDATAW nsMessengerWinIntegration::sWideBiffIconData = { sizeof(NOTIFYICONDATAW),
                                                    0,
                                                    2,
                                                    NIF_ICON | NIF_MESSAGE | NIF_TIP,
                                                    WM_USER,
                                                    0,
                                                    0 };

#ifdef MOZ_STATIC_BUILD
#define MAIL_DLL_NAME NULL
#else
#ifdef MOZ_STATIC_MAIL_BUILD
#define MAIL_DLL_NAME "mail.dll"
#else
#define MAIL_DLL_NAME "msgbase.dll"
#endif
#endif

void nsMessengerWinIntegration::InitializeBiffStatusIcon()
{
  // initialize our biff status bar icon 
  Create();

  if (mUseWideCharBiffIcon)
  {
    sWideBiffIconData.hWnd = (HWND) msgWindow;
    sWideBiffIconData.hIcon =  ::LoadIcon( ::GetModuleHandle( MAIL_DLL_NAME ), MAKEINTRESOURCE(IDI_MAILBIFF) );
    sWideBiffIconData.szTip[0] = 0;
  }
  else
  {
    sNativeBiffIconData.hWnd = (HWND) msgWindow;
    sNativeBiffIconData.hIcon =  ::LoadIcon( ::GetModuleHandle( MAIL_DLL_NAME ), MAKEINTRESOURCE(IDI_MAILBIFF) );
    sNativeBiffIconData.szTip[0] = 0;
  }

  mBiffIconInitialized = PR_TRUE;
}

nsresult
nsMessengerWinIntegration::Init()
{
  nsresult rv;

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

  // get process addresses for the unread mail count functions
  if (hModule) {
    mSHSetUnreadMailCount = (fnSHSetUnreadMailCount)GetProcAddress(hModule, XP_SHSetUnreadMailCounts);
    mSHEnumerateUnreadMailAccounts = (fnSHEnumerateUnreadMailAccounts)GetProcAddress(hModule, XP_SHEnumerateUnreadMailAccounts);
    mShellNotifyWideChar = (fnShellNotifyW)GetProcAddress(hModule, ShellNotifyWideVersion);
    if (mShellNotifyWideChar)
       mUseWideCharBiffIcon = PR_TRUE; // this version of the shell supports I18N friendly ShellNotify routines.
  }

  // if failed to get either of the process addresses, this is not XP platform
  // so we aren't storing unread counts
  if (mSHSetUnreadMailCount && mSHEnumerateUnreadMailAccounts)
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
// XXX test for this as long as there are still non-xul-app suite builds
#ifdef MOZ_XUL_APP
    // get current profile path for the commandliner
    nsCOMPtr<nsIFile> profilePath;
    rv = directoryService->Get(NS_APP_USER_PROFILE_50_DIR, 
                               NS_GET_IID(nsIFile), 
                               getter_AddRefs(profilePath));
    NS_ENSURE_SUCCESS(rv,rv);

    rv = profilePath->GetPath(mProfilePath);
    NS_ENSURE_SUCCESS(rv, rv);
#else
    // get current profile name to fill in commandliner. 
    nsCOMPtr<nsIProfile> profileService = do_GetService(NS_PROFILE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    profileService->GetCurrentProfile(getter_Copies(mProfileName));
#endif

    // get application path 
    char appPath[_MAX_PATH] = {0};
    GetModuleFileName(nsnull, appPath, sizeof(appPath));
    WCHAR wideFormatAppPath[_MAX_PATH*2] = {0};
    MultiByteToWideChar(CP_ACP, 0, appPath, strlen(appPath), wideFormatAppPath, _MAX_PATH*2);
    mAppName.Assign((PRUnichar *)wideFormatAppPath);

    rv = ResetCurrent();
    NS_ENSURE_SUCCESS(rv,rv);
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

#ifndef MOZ_THUNDERBIRD
nsresult nsMessengerWinIntegration::ShowAlertMessage(const PRUnichar * aAlertTitle, const PRUnichar * aAlertText, const char * aFolderURI)
{
  nsresult rv;
  
  // if we are already in the process of showing an alert, don't try to show another....
  if (mAlertInProgress) 
    return NS_OK; 

  nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  PRBool showAlert = PR_TRUE;
  
  if (prefBranch)
    prefBranch->GetBoolPref(SHOW_ALERT_PREF, &showAlert);
  
  if (showAlert)
  {
    nsCOMPtr<nsIAlertsService> alertsService (do_GetService(NS_ALERTSERVICE_CONTRACTID, &rv));
    if (NS_SUCCEEDED(rv))
    {
      rv = alertsService->ShowAlertNotification(NS_LITERAL_STRING(NEW_MAIL_ALERT_ICON), nsDependentString(aAlertTitle),
                                                nsDependentString(aAlertText), PR_TRUE, 
                                                NS_ConvertASCIItoUTF16(aFolderURI), this); 
      mAlertInProgress = PR_TRUE;
    }
  }

  if (!showAlert || NS_FAILED(rv)) // go straight to showing the system tray icon.
    AlertFinished();

  return rv;
}
#else
// Opening Thunderbird's new mail alert notification window
// aUserInitiated --> true if we are opening the alert notification in response to a user action
//                    like clicking on the biff icon
nsresult nsMessengerWinIntegration::ShowNewAlertNotification(PRBool aUserInitiated)
{
  nsresult rv;

  // if we are already in the process of showing an alert, don't try to show another....
  if (mAlertInProgress) 
    return NS_OK;
  
  nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  PRBool showAlert = PR_TRUE;
  
  if (prefBranch)
    prefBranch->GetBoolPref(SHOW_ALERT_PREF, &showAlert);
  
  if (showAlert)
  {
    nsCOMPtr<nsISupportsArray> argsArray;
    rv = NS_NewISupportsArray(getter_AddRefs(argsArray));
    NS_ENSURE_SUCCESS(rv, rv);

    // pass in the array of folders with unread messages
    nsCOMPtr<nsISupportsInterfacePointer> ifptr = do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    ifptr->SetData(mFoldersWithNewMail);
    ifptr->SetDataIID(&NS_GET_IID(nsISupportsArray));
    rv = argsArray->AppendElement(ifptr);
    NS_ENSURE_SUCCESS(rv, rv);

    // pass in the observer
    ifptr = do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr <nsISupports> supports = do_QueryInterface(NS_STATIC_CAST(nsIMessengerOSIntegration*, this));
    ifptr->SetData(supports);
    ifptr->SetDataIID(&NS_GET_IID(nsIObserver));
    rv = argsArray->AppendElement(ifptr);
    NS_ENSURE_SUCCESS(rv, rv);
    
    // pass in the animation flag
    nsCOMPtr<nsISupportsPRBool> scriptableUserInitiated (do_CreateInstance(NS_SUPPORTS_PRBOOL_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    scriptableUserInitiated->SetData(aUserInitiated);
    rv = argsArray->AppendElement(scriptableUserInitiated);
    NS_ENSURE_SUCCESS(rv, rv);

    // pass in the alert origin
    nsCOMPtr<nsISupportsPRUint8> scriptableOrigin (do_CreateInstance(NS_SUPPORTS_PRUINT8_CONTRACTID));
    NS_ENSURE_TRUE(scriptableOrigin, NS_ERROR_FAILURE);
    scriptableOrigin->SetData(0);
    nsCOMPtr<nsILookAndFeel> lookAndFeel = do_GetService("@mozilla.org/widget/lookandfeel;1");
    if (lookAndFeel)
    {
      PRInt32 origin;
      lookAndFeel->GetMetric(nsILookAndFeel::eMetric_AlertNotificationOrigin,
                             origin);
      if (origin && origin >= 0 && origin <= 7)
        scriptableOrigin->SetData(origin);
    }
    rv = argsArray->AppendElement(scriptableOrigin);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
    nsCOMPtr<nsIDOMWindow> newWindow;
    rv = wwatch->OpenWindow(0, ALERT_CHROME_URL, "_blank",
                "chrome,dialog=yes,titlebar=no,popup=yes", argsArray,
                 getter_AddRefs(newWindow));

    mAlertInProgress = PR_TRUE;
  }

  // if the user has turned off the mail alert, or  openWindow generated an error, 
  // then go straight to the system tray.
  if (!showAlert || NS_FAILED(rv)) 
    AlertFinished();

  return rv;
}
#endif

nsresult nsMessengerWinIntegration::AlertFinished()
{
  // okay, we are done showing the alert
  // now put an icon in the system tray, if allowed
  PRBool showTrayIcon = !mSuppressBiffIcon;
  if (showTrayIcon)
  {
    nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
    if (prefBranch)
      prefBranch->GetBoolPref(SHOW_TRAY_ICON_PREF, &showTrayIcon);
  }
  if (showTrayIcon)
  {
    GenericShellNotify(NIM_ADD);
    mBiffIconVisible = PR_TRUE;
  }
  mSuppressBiffIcon = PR_FALSE;
  mAlertInProgress = PR_FALSE;
  return NS_OK;
}

nsresult nsMessengerWinIntegration::AlertClicked()
{
#ifdef MOZ_THUNDERBIRD
  nsresult rv;
  nsCOMPtr<nsIMsgMailSession> mailSession = do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);
  nsCOMPtr<nsIMsgWindow> topMostMsgWindow;
  rv = mailSession->GetTopmostMsgWindow(getter_AddRefs(topMostMsgWindow));
  if (topMostMsgWindow)
  {
    nsCOMPtr<nsIDOMWindowInternal> domWindow;
    rv = topMostMsgWindow->GetDomWindow(getter_AddRefs(domWindow));
    NS_ENSURE_SUCCESS(rv, rv);

    activateWindow(domWindow);
  }
#else
  // make sure we don't insert the icon in the system tray since the user clicked on the alert.
  mSuppressBiffIcon = PR_TRUE;

  nsXPIDLCString folderURI;
  GetFirstFolderWithNewMail(getter_Copies(folderURI));

  openMailWindow(NS_LITERAL_STRING("mail:3pane").get(), folderURI);
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerWinIntegration::Observe(nsISupports* aSubject, const char* aTopic, const PRUnichar* aData)
{
  if (strcmp(aTopic, "alertfinished") == 0)
      return AlertFinished();

  if (strcmp(aTopic, "alertclickcallback") == 0)
      return AlertClicked();

  return NS_OK;
}

void nsMessengerWinIntegration::FillToolTipInfo()
{
  // iterate over all the folders in mFoldersWithNewMail
  nsXPIDLString accountName;
  nsXPIDLCString hostName; 
  nsAutoString toolTipText;
  nsAutoString animatedAlertText;
  nsCOMPtr<nsIMsgFolder> folder;
  nsCOMPtr<nsIWeakReference> weakReference;
  PRInt32 numNewMessages = 0;

  PRUint32 count = 0;
  mFoldersWithNewMail->Count(&count);
  PRUint32 maxTooltipSize = GetToolTipSize();

  for (PRUint32 index = 0; index < count; index++)
  {
    weakReference = do_QueryElementAt(mFoldersWithNewMail, index);
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
#ifndef MOZ_THUNDERBIRD
    ShowAlertMessage(accountName, animatedAlertText.get(), "");
#else
    ShowNewAlertNotification(PR_FALSE);
#endif
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

  nsCOMPtr<nsIMsgFolder> folder;
  nsCOMPtr<nsIWeakReference> weakReference;
  PRInt32 numNewMessages = 0;

  PRUint32 count = 0;
  mFoldersWithNewMail->Count(&count);

  if (!count)  // kick out if we don't have any folders with new mail
    return NS_OK;

  weakReference = do_QueryElementAt(mFoldersWithNewMail, 0);
  folder = do_QueryReferent(weakReference);
  
  if (folder)
  {
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
    return (sizeof(sWideBiffIconData.szTip)/sizeof(sWideBiffIconData.szTip[0]));
  else
    return (sizeof(sNativeBiffIconData.szTip));
}

void nsMessengerWinIntegration::SetToolTipStringOnIconData(const PRUnichar * aToolTipString)
{
  if (!aToolTipString) return;

  PRUint32 toolTipBufSize = GetToolTipSize();
  
  if (mUseWideCharBiffIcon)
  {
    ::wcsncpy( sWideBiffIconData.szTip, aToolTipString, toolTipBufSize);
    if (wcslen(aToolTipString) >= toolTipBufSize)
      sWideBiffIconData.szTip[toolTipBufSize - 1] = 0;
  }
  else
  {
    nsCAutoString nativeToolTipString;
    NS_CopyUnicodeToNative(nsDependentString(aToolTipString),
                           nativeToolTipString);
    ::strncpy(sNativeBiffIconData.szTip,
              nativeToolTipString.get(), GetToolTipSize());
    if (nativeToolTipString.Length() >= toolTipBufSize)
      sNativeBiffIconData.szTip[toolTipBufSize - 1] = 0;
  }
}

void nsMessengerWinIntegration::GenericShellNotify(DWORD aMessage)
{
  if (mUseWideCharBiffIcon)
  {
    BOOL res = mShellNotifyWideChar( aMessage, &sWideBiffIconData );
    if (!res)
      RevertToNonUnicodeShellAPI(); // oops we don't really implement the unicode shell apis...fall back.
    else
      return; 
  }
  
  ::Shell_NotifyIcon( aMessage, &sNativeBiffIconData ); 
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
  if (sWideBiffIconData.szTip)
  {
    const PRUnichar * oldTooltipString = sWideBiffIconData.szTip;
    SetToolTipStringOnIconData(oldTooltipString);
  }
}

NS_IMETHODIMP
nsMessengerWinIntegration::OnItemPropertyFlagChanged(nsIMsgDBHdr *item, nsIAtom *property, PRUint32 oldFlag, PRUint32 newFlag)
{
  nsresult rv = NS_OK;
  
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
  // if we got new mail show a icon in the system tray
  if (mBiffStateAtom == aProperty && mFoldersWithNewMail)
  {
    if (!mBiffIconInitialized)
      InitializeBiffStatusIcon(); 

    nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(aItem);
    NS_ENSURE_TRUE(folder, NS_OK);

    if (aNewValue == nsIMsgFolder::nsMsgBiffState_NewMail) 
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

      if (mFoldersWithNewMail->IndexOf(weakFolder) == -1)
        mFoldersWithNewMail->AppendElement(weakFolder);
      // now regenerate the tooltip
      FillToolTipInfo();    
    }
    else if (aNewValue == nsIMsgFolder::nsMsgBiffState_NoMail)
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

  if (!mStoreUnreadCounts) return NS_OK; // don't do anything here if we aren't storing unread counts...

  if (aProperty == mTotalUnreadMessagesAtom) {
    const char *itemURI = nsnull;
    nsresult rv;
    rv = aItem->GetValueConst(&itemURI);
    NS_ENSURE_SUCCESS(rv,rv);

    if (itemURI && mInboxURI && !strcmp(itemURI, mInboxURI)) {
      mCurrentUnreadCount = aNewValue;
    }

    // If the timer isn't running yet, then we immediately update the
    // registry and then start a one-shot timer. If the Unread counter
    // has toggled zero / nonzero, we also update immediately.
    // Otherwise, if the timer is running, defer the update. This means
    // that all counter updates that occur within the timer interval are
    // batched into a single registry update, to avoid hitting the
    // registry too frequently. We also do a final update on shutdown,
    // regardless of the timer.
    if (!mUnreadTimerActive ||
         (!mCurrentUnreadCount && mLastUnreadCountWrittenToRegistry) ||
         (mCurrentUnreadCount && mLastUnreadCountWrittenToRegistry < 1)) {
      rv = UpdateUnreadCount();
      NS_ENSURE_SUCCESS(rv,rv);
      // If timer wasn't running, start it.
      if (!mUnreadTimerActive)
        rv = SetupUnreadCountUpdateTimer();
    }
  }
  return NS_OK;
}

void 
nsMessengerWinIntegration::OnUnreadCountUpdateTimer(nsITimer *timer, void *osIntegration)
{
  nsMessengerWinIntegration *winIntegration = (nsMessengerWinIntegration*)osIntegration;

  winIntegration->mUnreadTimerActive = PR_FALSE;
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
                     NS_ConvertUTF16toUTF8(deleteKey).get());
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
  commandLinerForAppLaunch.Assign(NS_LITERAL_STRING(DOUBLE_QUOTE));
  commandLinerForAppLaunch.Append(mAppName);
  commandLinerForAppLaunch.Append(NS_LITERAL_STRING(DOUBLE_QUOTE));
  commandLinerForAppLaunch.Append(NS_LITERAL_STRING(PROFILE_COMMANDLINE_ARG));
// XXX test for this as long as there are still non-xul-app suite builds
#ifdef MOZ_XUL_APP
  commandLinerForAppLaunch.Append(NS_LITERAL_STRING(DOUBLE_QUOTE));
  commandLinerForAppLaunch.Append(mProfilePath);
  commandLinerForAppLaunch.Append(NS_LITERAL_STRING(DOUBLE_QUOTE));
#else
  commandLinerForAppLaunch.Append(mProfileName);
#endif
  commandLinerForAppLaunch.Append(NS_LITERAL_STRING(MAIL_COMMANDLINE_ARG));

  if (!commandLinerForAppLaunch.IsEmpty())
  {
    nsAutoString pBuffer;

    if (!mEmailPrefix.IsEmpty()) {
      pBuffer.Assign(mEmailPrefix);
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
      nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
      NS_ENSURE_SUCCESS(rv,rv);

      nsCOMPtr<nsISupportsString> tmp;
      rv = prefBranch->GetComplexValue(providerPrefixPref.get(), NS_GET_IID(nsISupportsString),
                                       getter_AddRefs(tmp));

      if (NS_SUCCEEDED(rv))
        tmp->GetData(mEmailPrefix);
      else
        mEmailPrefix.Truncate();
    }
    else {
      mEmailPrefix.Truncate();
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
  mUnreadTimerActive = PR_TRUE;
  if (mUnreadCountUpdateTimer) {
    mUnreadCountUpdateTimer->Cancel();
  }
  else
  {
    mUnreadCountUpdateTimer = do_CreateInstance("@mozilla.org/timer;1");
  }

  mUnreadCountUpdateTimer->InitWithFuncCallback(OnUnreadCountUpdateTimer,
    (void *)this, UNREAD_UPDATE_INTERVAL, nsITimer::TYPE_ONE_SHOT);

  return NS_OK;
}


/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "nsMessengerUnixIntegration.h"
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

#include "nsIMessengerWindowService.h"
#include "prprf.h"
#include "nsIWeakReference.h"
#include "nsIStringBundle.h"
#include "nsIAlertsService.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsISupportsPrimitives.h"

#include "nsNativeCharsetUtils.h"

#ifdef MOZ_XUL_APP
#include "nsToolkitCompsCID.h" 
#endif

#define ALERT_CHROME_URL "chrome://messenger/content/newmailalert.xul"
#define NEW_MAIL_ALERT_ICON "chrome://messenger/skin/icons/new-mail-alert.png"
#define SHOW_ALERT_PREF "mail.biff.show_alert"

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
    domWindow->Focus();
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

nsMessengerUnixIntegration::nsMessengerUnixIntegration()
{
  mBiffStateAtom = do_GetAtom("BiffState");
  mAlertInProgress = PR_FALSE;
  NS_NewISupportsArray(getter_AddRefs(mFoldersWithNewMail));
}

NS_IMPL_ISUPPORTS3(nsMessengerUnixIntegration, nsIFolderListener, nsIObserver, nsIMessengerOSIntegration)

nsresult
nsMessengerUnixIntegration::Init()
{
  nsresult rv;

  nsCOMPtr<nsIMsgMailSession> mailSession = do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = mailSession->AddFolderListener(this, nsIFolderListener::intPropertyChanged);
  NS_ENSURE_SUCCESS(rv,rv);

  return NS_OK;
}

NS_IMETHODIMP
nsMessengerUnixIntegration::OnItemPropertyChanged(nsIRDFResource *, nsIAtom *, char const *, char const *)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerUnixIntegration::OnItemUnicharPropertyChanged(nsIRDFResource *, nsIAtom *, const PRUnichar *, const PRUnichar *)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerUnixIntegration::OnItemRemoved(nsIRDFResource *, nsISupports *)
{
  return NS_OK;
}

nsresult nsMessengerUnixIntegration::GetStringBundle(nsIStringBundle **aBundle)
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
nsresult nsMessengerUnixIntegration::ShowAlertMessage(const PRUnichar * aAlertTitle, const PRUnichar * aAlertText, const char * aFolderURI)
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
nsresult nsMessengerUnixIntegration::ShowNewAlertNotification(PRBool aUserInitiated)
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
    argsArray->AppendElement(ifptr);

    // pass in the observer
    ifptr = do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr <nsISupports> supports = do_QueryInterface(NS_STATIC_CAST(nsIMessengerOSIntegration*, this));
    ifptr->SetData(supports);
    ifptr->SetDataIID(&NS_GET_IID(nsIObserver));
    argsArray->AppendElement(ifptr);

    // pass in the animation flag
    nsCOMPtr<nsISupportsPRBool> scriptableUserInitiated (do_CreateInstance(NS_SUPPORTS_PRBOOL_CONTRACTID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    scriptableUserInitiated->SetData(aUserInitiated);
    argsArray->AppendElement(scriptableUserInitiated);

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

nsresult nsMessengerUnixIntegration::AlertFinished()
{
  mAlertInProgress = PR_FALSE;
  return NS_OK;
}

nsresult nsMessengerUnixIntegration::AlertClicked()
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

    domWindow->Focus();
  }
#else
  nsXPIDLCString folderURI;
  GetFirstFolderWithNewMail(getter_Copies(folderURI));

  openMailWindow(NS_LITERAL_STRING("mail:3pane").get(), folderURI);
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerUnixIntegration::Observe(nsISupports* aSubject, const char* aTopic, const PRUnichar* aData)
{
  if (strcmp(aTopic, "alertfinished") == 0)
      return AlertFinished();

  if (strcmp(aTopic, "alertclickcallback") == 0)
      return AlertClicked();

  return NS_OK;
}

void nsMessengerUnixIntegration::FillToolTipInfo()
{
  nsCOMPtr<nsIWeakReference> weakReference = do_QueryElementAt(mFoldersWithNewMail, 0);
  nsCOMPtr<nsIMsgFolder> folder = do_QueryReferent(weakReference);
  if (folder)
  {
    nsXPIDLString accountName;
    folder->GetPrettiestName(getter_Copies(accountName));

    nsCOMPtr<nsIStringBundle> bundle; 
    GetStringBundle(getter_AddRefs(bundle));
    if (bundle)
    { 
      PRInt32 numNewMessages = 0;   
      folder->GetNumNewMessages(PR_TRUE, &numNewMessages);
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

#ifndef MOZ_THUNDERBIRD
      ShowAlertMessage(accountName, finalText, "");
#else
      ShowNewAlertNotification(PR_FALSE);
#endif
    } // if we got a bundle
  } // if we got a folder
}

// get the first top level folder which we know has new mail, then enumerate over all the subfolders
// looking for the first real folder with new mail. Return the folderURI for that folder.
nsresult nsMessengerUnixIntegration::GetFirstFolderWithNewMail(char ** aFolderURI)
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

NS_IMETHODIMP
nsMessengerUnixIntegration::OnItemPropertyFlagChanged(nsIMsgDBHdr *item, nsIAtom *property, PRUint32 oldFlag, PRUint32 newFlag)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerUnixIntegration::OnItemAdded(nsIRDFResource *, nsISupports *)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerUnixIntegration::OnItemBoolPropertyChanged(nsIRDFResource *aItem,
                                                         nsIAtom *aProperty,
                                                         PRBool aOldValue,
                                                         PRBool aNewValue)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerUnixIntegration::OnItemEvent(nsIMsgFolder *, nsIAtom *)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerUnixIntegration::OnItemIntPropertyChanged(nsIRDFResource *aItem, nsIAtom *aProperty, PRInt32 aOldValue, PRInt32 aNewValue)
{
  // if we got new mail show a icon in the system tray
  if (mBiffStateAtom == aProperty && mFoldersWithNewMail)
  {
    nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(aItem);
    NS_ENSURE_TRUE(folder, NS_OK);

    if (aNewValue == nsIMsgFolder::nsMsgBiffState_NewMail) 
    {
      // only show a system tray icon iff we are performing biff
      // (as opposed to the user getting new mail)
      PRBool performingBiff = PR_FALSE;
      nsCOMPtr<nsIMsgIncomingServer> server;
      folder->GetServer(getter_AddRefs(server));
      if (server)
        server->GetPerformingBiff(&performingBiff);
      if (!performingBiff) 
        return NS_OK; // kick out right now...

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
      
      mFoldersWithNewMail->Clear(); 
    }
  } // if the biff property changed
  return NS_OK;
}

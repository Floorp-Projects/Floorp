/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Alec Flett <alecf@netscape.com>
 *   Scott Putterman <putterman@netscape.com>
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "nsMessengerBootstrap.h"
#include "nsCOMPtr.h"

#include "nsDOMCID.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgMailSession.h"
#include "nsIMsgFolderCache.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIDOMWindow.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIWindowWatcher.h"
#include "nsString.h"
#include "nsIURI.h"
#include "nsIDialogParamBlock.h"
#ifdef MOZ_XUL_APP
#include "nsICommandLine.h"
#include "nsILocalFile.h"
#include "nsNetUtil.h"
#include "nsIFileURL.h"
#endif

NS_IMPL_THREADSAFE_ADDREF(nsMessengerBootstrap)
NS_IMPL_THREADSAFE_RELEASE(nsMessengerBootstrap)

NS_IMPL_QUERY_INTERFACE2(nsMessengerBootstrap,
                         ICOMMANDLINEHANDLER,
                         nsIMessengerWindowService)

nsMessengerBootstrap::nsMessengerBootstrap()
{
}

nsMessengerBootstrap::~nsMessengerBootstrap()
{
}

#ifdef MOZ_XUL_APP
NS_IMETHODIMP
nsMessengerBootstrap::Handle(nsICommandLine* aCmdLine)
{
  nsresult rv;

  nsCOMPtr<nsIWindowWatcher> wwatch (do_GetService(NS_WINDOWWATCHER_CONTRACTID));
  NS_ENSURE_TRUE(wwatch, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMWindow> opened;

#ifndef MOZ_SUITE
  PRBool found;
  rv = aCmdLine->HandleFlag(NS_LITERAL_STRING("options"), PR_FALSE, &found);
  if (NS_SUCCEEDED(rv) && found) {
    wwatch->OpenWindow(nsnull, "chrome://messenger/content/preferences/preferences.xul", "_blank",
                      "chrome,dialog=no,all", nsnull, getter_AddRefs(opened));
    aCmdLine->SetPreventDefault(PR_TRUE);
  }
#endif
  
  nsAutoString mailUrl; // -mail or -mail <some url> 
  PRBool flag = PR_FALSE;
  rv = aCmdLine->HandleFlagWithParam(NS_LITERAL_STRING("mail"), PR_FALSE, mailUrl);
  if (NS_SUCCEEDED(rv))
    flag = !mailUrl.IsVoid();
  else 
    aCmdLine->HandleFlag(NS_LITERAL_STRING("mail"), PR_FALSE, &flag);
  if (flag)
  {
    nsCOMPtr<nsISupportsArray> argsArray = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // create scriptable versions of our strings that we can store in our nsISupportsArray....
    if (!mailUrl.IsEmpty())
    {
      nsCOMPtr<nsISupportsString> scriptableURL (do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
      NS_ENSURE_TRUE(scriptableURL, NS_ERROR_FAILURE);
  
      scriptableURL->SetData((mailUrl));
      argsArray->AppendElement(scriptableURL);
    }

    wwatch->OpenWindow(nsnull, "chrome://messenger/content/", "_blank",
                       "chrome,extrachrome,menubar,resizable,scrollbars,status,toolbar,dialog=no", argsArray, getter_AddRefs(opened));
    aCmdLine->SetPreventDefault(PR_TRUE);
    return NS_OK;
  } 

#ifndef MOZ_SUITE
  PRInt32 numArgs;
  aCmdLine->GetLength(&numArgs);
  if (numArgs > 0)
  {
    nsAutoString arg;
    aCmdLine->GetArgument(0, arg);
    if (StringEndsWith(arg, NS_LITERAL_STRING(".eml")))
    {
      nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
      NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);
      rv = file->InitWithPath(arg);
      NS_ENSURE_SUCCESS(rv, rv);
      // should we check that the file exists, or looks like a mail message?

      nsCOMPtr<nsIURI> uri;
      NS_NewFileURI(getter_AddRefs(uri), file);
      nsCOMPtr<nsIFileURL> fileURL(do_QueryInterface(uri));
      NS_ENSURE_TRUE(fileURL, NS_ERROR_FAILURE);

      // create scriptable versions of our strings that we can store in our nsISupportsArray....
      nsCOMPtr<nsISupportsString> scriptableURL (do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
      NS_ENSURE_TRUE(scriptableURL, NS_ERROR_FAILURE);

      fileURL->SetQuery(NS_LITERAL_CSTRING("?type=application/x-message-display"));

      wwatch->OpenWindow(nsnull, "chrome://messenger/content/messageWindow.xul", "_blank",
                         "chrome,extrachrome,menubar,resizable,scrollbars,status,toolbar,dialog=no", fileURL, getter_AddRefs(opened));
      aCmdLine->SetPreventDefault(PR_TRUE);
    }
    return NS_OK;

  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsMessengerBootstrap::GetHelpInfo(nsACString& aResult)
{
  aResult.Assign(
    "  -mail                Open the mail folder view.\n"
#ifndef MOZ_SUITE
    "  -options             Open the options dialog.\n"
#endif
  );

  return NS_OK;
}

#else
CMDLINEHANDLER3_IMPL(nsMessengerBootstrap,"-mail","general.startup.mail","Start with mail.",NS_MAILSTARTUPHANDLER_CONTRACTID,"Mail Cmd Line Handler",PR_TRUE,"", PR_TRUE)

NS_IMETHODIMP nsMessengerBootstrap::GetChromeUrlForTask(char **aChromeUrlForTask) 
{ 
  if (!aChromeUrlForTask) return NS_ERROR_FAILURE; 
  nsCOMPtr<nsIPrefBranch> pPrefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (pPrefBranch)
  {
    nsresult rv;
    PRInt32 layout;
    rv = pPrefBranch->GetIntPref("mail.pane_config", &layout);		
    if(NS_SUCCEEDED(rv))
    {
      if(layout == 0)
        *aChromeUrlForTask = PL_strdup("chrome://messenger/content/messenger.xul");
      else
        *aChromeUrlForTask = PL_strdup("chrome://messenger/content/mail3PaneWindowVertLayout.xul");
      
      return NS_OK;
      
    }	
  }
  *aChromeUrlForTask = PL_strdup("chrome://messenger/content/messenger.xul"); 

  return NS_OK; 
}
#endif

NS_IMETHODIMP nsMessengerBootstrap::OpenMessengerWindowWithUri(const char *windowType, const char * aFolderURI, nsMsgKey aMessageKey)
{
  nsresult rv;

#ifdef MOZ_XUL_APP
  NS_NAMED_LITERAL_CSTRING(chromeurl, "chrome://messenger/content/");
#else
  nsXPIDLCString chromeurl;
  rv = GetChromeUrlForTask(getter_Copies(chromeurl));
  if (NS_FAILED(rv)) return rv;
#endif

  nsCOMPtr<nsISupportsArray> argsArray;
  rv = NS_NewISupportsArray(getter_AddRefs(argsArray));
  NS_ENSURE_SUCCESS(rv, rv);

  // create scriptable versions of our strings that we can store in our nsISupportsArray....
  if (aFolderURI)
  {
    nsCOMPtr<nsISupportsCString> scriptableFolderURI (do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID));
    NS_ENSURE_TRUE(scriptableFolderURI, NS_ERROR_FAILURE);

    scriptableFolderURI->SetData(nsDependentCString(aFolderURI));
    argsArray->AppendElement(scriptableFolderURI);

    nsCOMPtr<nsISupportsPRUint32> scriptableMessageKey (do_CreateInstance(NS_SUPPORTS_PRUINT32_CONTRACTID));
    NS_ENSURE_TRUE(scriptableMessageKey, NS_ERROR_FAILURE);
    scriptableMessageKey->SetData(aMessageKey);
    argsArray->AppendElement(scriptableMessageKey);
  }
  
  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // we need to use the "mailnews.reuse_thread_window2" pref
	// to determine if we should open a new window, or use an existing one.
  nsCOMPtr<nsIDOMWindow> newWindow;
  rv = wwatch->OpenWindow(0, chromeurl.get(), "_blank",
                 "chrome,extrachrome,menubar,resizable,scrollbars,status,toolbar,dialog=no",
                 argsArray,
                 getter_AddRefs(newWindow));

  return NS_OK;
}

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMessengerBootstrap.h"
#include "nsCOMPtr.h"

#include "nsDOMCID.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgMailSession.h"
#include "nsIMsgFolderCache.h"
#include "nsIPref.h"
#include "nsIDOMWindow.h"
#include "nsISupportsPrimitives.h"
#include "nsIWindowWatcher.h"
#include "nsString.h"
#include "nsIURI.h"
#include "nsIDialogParamBlock.h"
static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID); 
static NS_DEFINE_CID(kMsgAccountManagerCID, NS_MSGACCOUNTMANAGER_CID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

NS_IMPL_THREADSAFE_ADDREF(nsMessengerBootstrap);
NS_IMPL_THREADSAFE_RELEASE(nsMessengerBootstrap);
NS_IMPL_QUERY_INTERFACE2(nsMessengerBootstrap, nsICmdLineHandler, nsIMessengerWindowService);

nsMessengerBootstrap::nsMessengerBootstrap()
{
  NS_INIT_REFCNT();
}

nsMessengerBootstrap::~nsMessengerBootstrap()
{
}

CMDLINEHANDLER3_IMPL(nsMessengerBootstrap,"-mail","general.startup.mail","Start with mail.",NS_MAILSTARTUPHANDLER_CONTRACTID,"Mail Cmd Line Handler",PR_FALSE,"", PR_TRUE)

NS_IMETHODIMP nsMessengerBootstrap::GetChromeUrlForTask(char **aChromeUrlForTask) 
{ 
  if (!aChromeUrlForTask) return NS_ERROR_FAILURE; 
  nsresult rv;
  nsCOMPtr<nsIPref> prefService(do_GetService(kPrefServiceCID, &rv));
  if (NS_SUCCEEDED(rv))
  {
    PRInt32 layout;
    rv = prefService->GetIntPref("mail.pane_config", &layout);		
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

// Utility function to open a messenger window and pass an argument string to it.
static nsresult openWindow( const PRUnichar *chrome, const PRUnichar *uri, nsMsgKey key ) {

  nsresult rv;
  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIDialogParamBlock> ioParamBlock(do_CreateInstance("@mozilla.org/embedcomp/dialogparam;1", &rv));
  if (NS_FAILED(rv)) 
    return rv;
  ioParamBlock->SetString(0, uri);
  ioParamBlock->SetInt(0, key);
  nsCOMPtr <nsISupports> supports = do_QueryInterface(ioParamBlock);
  nsCOMPtr<nsIDOMWindow> newWindow;
  return wwatch->OpenWindow(0, NS_ConvertUCS2toUTF8(chrome).get(), "_blank",
                 "chrome,dialog=no,all", supports,
                 getter_AddRefs(newWindow));
}

NS_IMETHODIMP nsMessengerBootstrap::OpenMessengerWindowWithUri(const char *windowType, const char *uri)
{
  nsresult rv = NS_OK;
  
  nsXPIDLCString chromeurl;
 
  rv = GetChromeUrlForTask(getter_Copies(chromeurl));
  NS_ENSURE_SUCCESS(rv, rv);
  
  // we need to use the "mailnews.reuse_thread_window2" pref
  // to determine if we should open a new window, or use an existing one.
  return openWindow(NS_ConvertUTF8toUCS2(chromeurl).get(), uri ? 
                    NS_ConvertUTF8toUCS2(uri).get() : nsnull, nsMsgKey_None);
}

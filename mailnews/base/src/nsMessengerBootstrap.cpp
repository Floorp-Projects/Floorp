/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsMessengerBootstrap.h"
#include "nsCOMPtr.h"

#include "nsDOMCID.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgMailSession.h"
#include "nsIMsgFolderCache.h"
#include "nsIPref.h"

static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID); 
static NS_DEFINE_CID(kMsgAccountManagerCID, NS_MSGACCOUNTMANAGER_CID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

NS_IMPL_THREADSAFE_ADDREF(nsMessengerBootstrap);
NS_IMPL_THREADSAFE_RELEASE(nsMessengerBootstrap);
NS_IMPL_QUERY_INTERFACE2(nsMessengerBootstrap, nsIAppShellComponent, nsICmdLineHandler);

nsMessengerBootstrap::nsMessengerBootstrap()
{
  NS_INIT_REFCNT();
}

nsMessengerBootstrap::~nsMessengerBootstrap()
{
}

nsresult
nsMessengerBootstrap::Initialize(nsIAppShellService*,
                                 nsICmdLineService*)
{
#if 0
    // not needed?
	nsresult rv;

    nsCOMPtr<nsISupports> bootstrapper;
    rv = this->QueryInterface(kISupportsIID, getter_AddRefs(bootstrapper));
    if (NS_SUCCEEDED(rv) && bootstrapper) {
      rv = nsServiceManager::RegisterService(NS_MESSENGERBOOTSTRAP_PROGID, bootstrapper);
    }
	return rv;
#else
    return NS_OK;
#endif
}

nsresult
nsMessengerBootstrap::Shutdown()
{

	return NS_OK;
}


CMDLINEHANDLER3_IMPL(nsMessengerBootstrap,"-mail","general.startup.mail","Start with mail.",NS_MAILSTARTUPHANDLER_PROGID,"Mail Cmd Line Handler",PR_FALSE,"", PR_TRUE)

NS_IMETHODIMP nsMessengerBootstrap::GetChromeUrlForTask(char **aChromeUrlForTask) 
{ 
    if (!aChromeUrlForTask) return NS_ERROR_FAILURE; 
	nsresult rv;
	NS_WITH_SERVICE(nsIPref, prefService, kPrefServiceCID, &rv);
	if (NS_SUCCEEDED(rv))
	{
		PRInt32 layout;
		rv = prefService->GetIntPref("mail.pane_config", &layout);		
		if(NS_SUCCEEDED(rv))
		{
			if(layout == 0)
				*aChromeUrlForTask = PL_strdup("chrome://messenger/content/");
			else
				*aChromeUrlForTask = PL_strdup("chrome://messenger/content/mail3PaneWindowVertLayout.xul");

			return NS_OK;

		}	
	}
	*aChromeUrlForTask = PL_strdup("chrome://messenger/content/"); 
    return NS_OK; 
}


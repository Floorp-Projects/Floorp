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
 *   Alec Flett <alecf@netscape.com>
 *   Scott Putterman <putterman@netscape.com>
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsMessengerBootstrap.h"
#include "nsCOMPtr.h"

#include "nsDOMCID.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgMailSession.h"
#include "nsIMsgFolderCache.h"
#include "nsIPref.h"
#include "nsIDOMWindowInternal.h"
#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"
#include "nsIURI.h"

static NS_DEFINE_CID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID); 
static NS_DEFINE_CID(kMsgAccountManagerCID, NS_MSGACCOUNTMANAGER_CID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

NS_IMPL_THREADSAFE_ADDREF(nsMessengerBootstrap);
NS_IMPL_THREADSAFE_RELEASE(nsMessengerBootstrap);
NS_IMPL_QUERY_INTERFACE3(nsMessengerBootstrap, nsIAppShellComponent, nsICmdLineHandler, nsIMessengerWindowService);

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
    return NS_OK;
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
static nsresult openWindow( const PRUnichar *chrome, const PRUnichar *args ) {
    nsCOMPtr<nsIDOMWindowInternal> hiddenWindow;
    JSContext *jsContext;
    nsresult rv;
    NS_WITH_SERVICE( nsIAppShellService, appShell, kAppShellServiceCID, &rv )
    if ( NS_SUCCEEDED( rv ) ) {
        rv = appShell->GetHiddenWindowAndJSContext( getter_AddRefs( hiddenWindow ),
                                                    &jsContext );
        if ( NS_SUCCEEDED( rv ) ) {
            // Set up arguments for "window.openDialog"
            void *stackPtr;
            jsval *argv = JS_PushArguments( jsContext,
                                            &stackPtr,
                                            "WssW",
                                            chrome,
                                            "_blank",
                                            "chrome,dialog=no,all",
                                            args );
            if ( argv ) {
                nsCOMPtr<nsIDOMWindowInternal> newWindow;
                rv = hiddenWindow->OpenDialog( jsContext,
                                               argv,
                                               4,
                                               getter_AddRefs( newWindow ) );
                JS_PopArguments( jsContext, stackPtr );
            }
        }
    }
    return rv;
}

NS_IMETHODIMP nsMessengerBootstrap::OpenMessengerWindowWithUri(nsIURI *aURI)
{
	nsresult rv;

	nsXPIDLCString args;
	nsXPIDLCString chromeurl;

	if (!aURI) return NS_ERROR_FAILURE;

	rv = aURI->GetSpec(getter_Copies(args));
	if (NS_FAILED(rv)) return rv;

	rv = GetChromeUrlForTask(getter_Copies(chromeurl));
	if (NS_FAILED(rv)) return rv;

	// we need to use the "mailnews.reuse_thread_window2" pref
	// to determine if we should open a new window, or use an existing one.
	rv = openWindow(NS_ConvertASCIItoUCS2(chromeurl).GetUnicode(),NS_ConvertASCIItoUCS2(args).GetUnicode());
	if (NS_FAILED(rv)) return rv;

	return NS_OK;
}

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
 * The Original Code is Mozilla
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Krishna Mohan Khandrika (kkhandrika@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#define MAPI_STARTUP_ARG       "/MAPIStartUp"

#include <mapidefs.h>

#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsISupports.h"
#include "nsIPromptService.h"
#include "nsAppShellCIDs.h"
#include "nsIAppShellService.h"
#include "nsINativeAppSupport.h"
#include "nsICmdLineService.h"
#include "nsIProfileInternal.h"
#include "nsIMsgAccountManager.h"
#include "nsIDOMWindowInternal.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsMsgBaseCID.h"
#include "nsIStringBundle.h"
#include "nsString.h"

#include "nsIMsgCompFields.h"
#include "nsIMsgComposeParams.h"
#include "nsIMsgCompose.h"
#include "nsMsgCompCID.h"
#include "nsIMsgSend.h"
#include "nsIProxyObjectManager.h"
#include "nsIMsgComposeService.h"
#include "nsProxiedService.h"
#include "nsEscape.h"

#include "msgMapiDefs.h"
#include "msgMapi.h"
#include "msgMapiHook.h"
#include "msgMapiSupport.h"
#include "msgMapiMain.h"


static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kCmdLineServiceCID, NS_COMMANDLINE_SERVICE_CID);
static NS_DEFINE_CID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_CID(kAccountMgrServiceCID, NS_MSGACCOUNTMANAGER_CID);
static NS_DEFINE_CID(kMsgSendCID, NS_MSGSEND_CID);
static NS_DEFINE_CID(kCompServiceCID, NS_MSGCOMPOSESERVICE_CID);

#define MAPI_PROPERTIES_CHROME  "chrome://messenger/locale/mapi.properties"

PRBool nsMapiHook::isMapiService = PR_FALSE;

PRBool nsMapiHook::Initialize()
{
    PRBool bResult = PR_FALSE;
    nsresult rv;
    nsCOMPtr<nsINativeAppSupport> native;

    nsCOMPtr<nsICmdLineService> cmdLineArgs(do_GetService(kCmdLineServiceCID, &rv));
    if (NS_FAILED(rv)) return PR_FALSE;

    nsCOMPtr<nsIAppShellService> appShell (do_GetService( "@mozilla.org/appshell/appShellService;1", &rv));
    if (NS_FAILED(rv)) return PR_FALSE;

    rv = appShell->GetNativeAppSupport( getter_AddRefs( native ));
    if (NS_FAILED(rv)) return PR_FALSE;

    rv = native->EnsureProfile(cmdLineArgs);
    if (NS_SUCCEEDED(rv))
    {
        bResult = PR_TRUE;
        PRBool serverMode = PR_FALSE;
        native->GetIsServerMode(&serverMode);
        if (serverMode == PR_FALSE)
        {
            native->SetIsServerMode( PR_TRUE );
            isMapiService = PR_TRUE;
        }
    }

    return bResult;
}

void nsMapiHook::CleanUp()
{
      nsresult rv;
      char arg[] = MAPI_STARTUP_ARG;
      char *mapiStartup = nsnull;

      nsCOMPtr<nsICmdLineService> cmdLineArgs(do_GetService(kCmdLineServiceCID, &rv));
      if (NS_FAILED(rv)) return;

      rv = cmdLineArgs->GetCmdLineValue(arg, &mapiStartup);
      if (NS_SUCCEEDED(rv) && (mapiStartup != nsnull || isMapiService == PR_TRUE))
      {
          nsCOMPtr<nsIAppShellService> appShell(do_GetService( "@mozilla.org/appshell/appShellService;1", &rv));
          if (NS_FAILED(rv)) return;

          nsCOMPtr<nsINativeAppSupport> native;
          rv = appShell->GetNativeAppSupport( getter_AddRefs( native ));
          if (native && NS_SUCCEEDED(rv))
          {
              native->SetIsServerMode( PR_FALSE );
              // This closes app if there are no top-level windows.
              appShell->UnregisterTopLevelWindow( 0 );
          }
      }
}

PRBool nsMapiHook::DisplayLoginDialog(PRBool aLogin, PRUnichar **aUsername, \
                      PRUnichar **aPassword)
{
    nsresult rv;
    PRBool btnResult = PR_FALSE;

    nsCOMPtr<nsIAppShellService> appShell(do_GetService( "@mozilla.org/appshell/appShellService;1", &rv));
    if (NS_FAILED(rv) || !appShell) return PR_FALSE;
   
    nsCOMPtr<nsIPromptService> dlgService(do_GetService("@mozilla.org/embedcomp/prompt-service;1", &rv));
    if (NS_SUCCEEDED(rv) && dlgService)
    {
        nsCOMPtr<nsIStringBundleService> bundleService(do_GetService(kStringBundleServiceCID, &rv));
        if (NS_FAILED(rv) || !bundleService) return PR_FALSE;

        nsCOMPtr<nsIStringBundle> bundle;
        rv = bundleService->CreateBundle(MAPI_PROPERTIES_CHROME, getter_AddRefs(bundle));
        if (NS_FAILED(rv) || !bundle) return PR_FALSE;

        nsIDOMWindowInternal *hiddenWindow;
        rv = appShell->GetHiddenDOMWindow(&hiddenWindow);
        if (NS_FAILED(rv) || !hiddenWindow) return PR_FALSE;

        nsCOMPtr<nsIStringBundle> brandBundle;
        rv = bundleService->CreateBundle(
                        "chrome://global/locale/brand.properties",
                        getter_AddRefs(brandBundle));
        if (NS_FAILED(rv)) return PR_FALSE;

        nsXPIDLString brandName;
        rv = brandBundle->GetStringFromName(
                           NS_LITERAL_STRING("brandShortName").get(),
                           getter_Copies(brandName));
        if (NS_FAILED(rv)) return PR_FALSE;
        
        nsXPIDLString loginTitle;
        const PRUnichar *brandStrings[] = { brandName.get() };
        NS_NAMED_LITERAL_STRING(loginTitlePropertyTag, "loginTitle");
        const PRUnichar *dTitlePropertyTag = loginTitlePropertyTag.get();
        rv = bundle->FormatStringFromName(dTitlePropertyTag, brandStrings, 1,
                                          getter_Copies(loginTitle));
        if (NS_FAILED(rv)) return PR_FALSE;
        
        if (aLogin)
        {
            nsXPIDLString loginText;
            rv = bundle->GetStringFromName(NS_LITERAL_STRING("loginTextwithName").get(),
                                           getter_Copies(loginText));
            if (NS_FAILED(rv) || !loginText) return PR_FALSE;



            rv = dlgService->PromptUsernameAndPassword(hiddenWindow, loginTitle,
                                                       loginText, aUsername, aPassword,
                                                       nsnull, PR_FALSE, &btnResult);
        }
        else
        {
            //nsString loginString; 
            nsXPIDLString loginText;
            const PRUnichar *userNameStrings[] = { *aUsername };

            NS_NAMED_LITERAL_STRING(loginTextPropertyTag, "loginText");
            const PRUnichar *dpropertyTag = loginTextPropertyTag.get();
            rv = bundle->FormatStringFromName(dpropertyTag, userNameStrings, 1,
                                              getter_Copies(loginText));
            if (NS_FAILED(rv)) return PR_FALSE;

            rv = dlgService->PromptPassword(hiddenWindow, loginTitle, loginText,
                                            aPassword, nsnull, PR_FALSE, &btnResult);
        }
    }            

    return btnResult;
}

PRBool nsMapiHook::VerifyUserName(const PRUnichar *aUsername, char **aIdKey)
{
    nsresult rv;

    if (aUsername == nsnull)
        return PR_FALSE;

    nsCOMPtr<nsIMsgAccountManager> accountManager(do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return PR_FALSE;
    nsCOMPtr<nsISupportsArray> identities;
    rv = accountManager->GetAllIdentities(getter_AddRefs(identities));
    if (NS_FAILED(rv)) return PR_FALSE;
    PRUint32 numIndentities;
    identities->Count(&numIndentities);

    for (PRUint32 i = 0; i < numIndentities; i++)
    {
        // convert supports->Identity
        nsCOMPtr<nsISupports> thisSupports;
        rv = identities->GetElementAt(i, getter_AddRefs(thisSupports));
        if (NS_FAILED(rv)) continue;
        nsCOMPtr<nsIMsgIdentity> thisIdentity(do_QueryInterface(thisSupports, &rv));
        if (NS_SUCCEEDED(rv) && thisIdentity)
        {
            nsXPIDLCString email;
            rv = thisIdentity->GetEmail(getter_Copies(email));
            if (NS_FAILED(rv)) continue;

            // get the username from the email and compare with the username
            nsCAutoString aEmail(email.get());
            PRInt32 index = aEmail.FindChar('@');
            if (index != -1)
                aEmail.Truncate(index);

            if (nsDependentString(aUsername) == NS_ConvertASCIItoUCS2(aEmail))  // == overloaded
                return NS_SUCCEEDED(thisIdentity->GetKey(aIdKey));
        }
    }

    return PR_FALSE;
}



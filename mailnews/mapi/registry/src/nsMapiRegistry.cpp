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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Srilatha Moturi <srilatha@netscape.com>
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
 
#include "nsIServiceManager.h"
#include "nsMapiRegistry.h"
#include "nsIGenericFactory.h"
#include "nsXPIDLString.h"
#include "nsIPromptService.h"
#include "nsIStringBundle.h"
#include "nsIProxyObjectManager.h"
#include "nsProxiedService.h"

#include "nsMapiRegistryUtils.h" 
#include "nsString.h"

static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

nsresult ShowMapiErrorDialog();

/** Implementation of the nsIMapiRegistry interface.
 *  Use standard implementation of nsISupports stuff.
 */
NS_IMPL_ISUPPORTS1(nsMapiRegistry, nsIMapiRegistry);

nsMapiRegistry::nsMapiRegistry() {
    NS_INIT_ISUPPORTS();
    m_ShowDialog = !verifyRestrictedAccess();
    m_DefaultMailClient = IsDefaultMailClient();
}

nsMapiRegistry::~nsMapiRegistry() {
}

NS_IMETHODIMP
nsMapiRegistry::GetIsDefaultMailClient(PRBool * retval) {
    *retval = m_DefaultMailClient;
    return NS_OK;
}

NS_IMETHODIMP
nsMapiRegistry::GetShowDialog(PRBool * retval) {
    *retval = m_ShowDialog;
    return NS_OK;
}

NS_IMETHODIMP
nsMapiRegistry::SetDefaultMailClient() {
    nsresult rv = setDefaultMailClient();
    if (NS_SUCCEEDED(rv))
        m_DefaultMailClient = PR_TRUE;
    else
       rv = ShowMapiErrorDialog();
    return NS_OK;
}

NS_IMETHODIMP
nsMapiRegistry::UnsetDefaultMailClient() {
    nsresult rv = unsetDefaultMailClient();
    if (NS_SUCCEEDED(rv))
        m_DefaultMailClient = PR_FALSE;
    else
        ShowMapiErrorDialog();
    return NS_OK;
}

/** This will bring up the dialog box only once per session and 
 * only if the current app is not default Mail Client.
 * This also checks the registry if the registry key
 * showMapiDialog is set
 */
NS_IMETHODIMP
nsMapiRegistry::ShowMailIntegrationDialog() {
    nsresult rv;
    if (!m_ShowDialog || !getShowDialog()) return NS_OK;
    nsCOMPtr<nsIPromptService> promptService(do_GetService(
                  "@mozilla.org/embedcomp/prompt-service;1", &rv));
    if (NS_SUCCEEDED(rv) && promptService)
    {
        nsCOMPtr<nsIStringBundleService> bundleService(do_GetService(
                                         kStringBundleServiceCID, &rv));
        if (NS_FAILED(rv) || !bundleService) return NS_ERROR_FAILURE;

        nsCOMPtr<nsIStringBundle> bundle;
        rv = bundleService->CreateBundle(
                        "chrome://messenger/locale/mapi.properties",
                        getter_AddRefs(bundle));
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

        nsCOMPtr<nsIStringBundle> brandBundle;
        rv = bundleService->CreateBundle(
                        "chrome://global/locale/brand.properties",
                        getter_AddRefs(brandBundle));
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

        nsXPIDLString brandName;
        rv = brandBundle->GetStringFromName(
                           NS_LITERAL_STRING("brandShortName").get(),
                           getter_Copies(brandName));
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

        nsXPIDLString dialogTitle;
        const PRUnichar *brandStrings[] = { brandName.get() };
        NS_NAMED_LITERAL_STRING(dialogTitlePropertyTag, "dialogTitle");
        const PRUnichar *dTitlepropertyTag = dialogTitlePropertyTag.get();
        rv = bundle->FormatStringFromName(dTitlepropertyTag,
                                          brandStrings, 1,
                                          getter_Copies(dialogTitle));
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
        
        nsXPIDLString dialogText;
        NS_NAMED_LITERAL_STRING(dialogTextPropertyTag, "dialogText");
        const PRUnichar *dpropertyTag = dialogTextPropertyTag.get();
        rv = bundle->FormatStringFromName(dpropertyTag,
                                          brandStrings, 1,
                                          getter_Copies(dialogText));
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

        nsXPIDLString checkboxText;
        rv = bundle->GetStringFromName(
                           NS_LITERAL_STRING("checkboxText").get(),
                           getter_Copies(checkboxText));
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

        PRBool checkValue = PR_FALSE;
        PRInt32 buttonPressed = 0;
        rv = promptService->ConfirmEx(nsnull,
                                      dialogTitle,
                                      dialogText.get(),
                                      (nsIPromptService::BUTTON_TITLE_YES * 
                                      nsIPromptService::BUTTON_POS_0) +
                                      (nsIPromptService::BUTTON_TITLE_NO * 
                                      nsIPromptService::BUTTON_POS_1),
                                      nsnull,
                                      nsnull,
                                      nsnull,
                                      checkboxText,
                                      &checkValue,
                                      &buttonPressed);
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
        rv = SetRegistryKey(HKEY_LOCAL_MACHINE, "Software\\Mozilla\\Desktop", 
                                "showMapiDialog", (checkValue) ? "0" : "1");
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

        m_ShowDialog = PR_FALSE;
        if (!buttonPressed)
            rv = SetDefaultMailClient();
    }
    return rv;
}

nsresult ShowMapiErrorDialog() {
    nsresult rv;
    nsCOMPtr<nsIPromptService> promptService(do_GetService(
                  "@mozilla.org/embedcomp/prompt-service;1", &rv));
    if (NS_SUCCEEDED(rv) && promptService)
    {
        nsCOMPtr<nsIStringBundleService> bundleService(do_GetService(
                                         kStringBundleServiceCID, &rv));
        if (NS_FAILED(rv) || !bundleService) return NS_ERROR_FAILURE;

        nsCOMPtr<nsIStringBundle> bundle;
        rv = bundleService->CreateBundle(
                        "chrome://messenger/locale/mapi.properties",
                        getter_AddRefs(bundle));
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

        nsCOMPtr<nsIStringBundle> brandBundle;
        rv = bundleService->CreateBundle(
                        "chrome://global/locale/brand.properties",
                        getter_AddRefs(brandBundle));
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

        nsXPIDLString brandName;
        rv = brandBundle->GetStringFromName(
                           NS_LITERAL_STRING("brandShortName").get(),
                           getter_Copies(brandName));
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

        nsXPIDLString dialogTitle;
        const PRUnichar *brandStrings[] = { brandName.get() };
        NS_NAMED_LITERAL_STRING(dialogTitlePropertyTag, "errorMessageTitle");
        const PRUnichar *dTitlepropertyTag = dialogTitlePropertyTag.get();
        rv = bundle->FormatStringFromName(dTitlepropertyTag,
                                          brandStrings, 1,
                                          getter_Copies(dialogTitle));
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
        nsXPIDLString dialogText;
        NS_NAMED_LITERAL_STRING(dialogTextPropertyTag, "errorMessageText");
        const PRUnichar *dpropertyTag = dialogTextPropertyTag.get();
        rv = bundle->FormatStringFromName(dpropertyTag,
                                          brandStrings, 1,
                                          getter_Copies(dialogText));
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
        rv = promptService->Alert(nsnull, dialogTitle,
                                  dialogText);
    }
    return rv;
}


NS_GENERIC_FACTORY_CONSTRUCTOR(nsMapiRegistry);

// The list of components we register
static nsModuleComponentInfo components[] = 
{
  {
    NS_IMAPIREGISTRY_CLASSNAME, 
    NS_IMAPIREGISTRY_CID,
    NS_IMAPIWINHOOK_CONTRACTID, 
    nsMapiRegistryConstructor
  }
};

NS_IMPL_NSGETMODULE(nsMapiRegistryModule, components);


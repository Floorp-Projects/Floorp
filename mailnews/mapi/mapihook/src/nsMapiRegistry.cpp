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
 *   Srilatha Moturi <srilatha@netscape.com>
 *   Rajiv Dayal <rdayal@netscape.com>
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
 
#include "nsIServiceManager.h"
#include "nsXPIDLString.h"
#include "nsIPromptService.h"
#include "nsIProxyObjectManager.h"
#include "nsProxiedService.h"

#include "nsMapiRegistryUtils.h" 
#include "nsMapiRegistry.h"

#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

/** Implementation of the nsIMapiRegistry interface.
 *  Use standard implementation of nsISupports stuff.
 */
NS_IMPL_ISUPPORTS1(nsMapiRegistry, nsIMapiRegistry)

nsMapiRegistry::nsMapiRegistry() {
    m_DefaultMailClient = m_registryUtils.IsDefaultMailClient();
    // m_ShowDialog should be initialized to false 
    // if we are the default mail client.
    m_ShowDialog = !m_registryUtils.verifyRestrictedAccess() && !m_DefaultMailClient;
}

nsMapiRegistry::~nsMapiRegistry() {
}

NS_IMETHODIMP
nsMapiRegistry::GetIsDefaultMailClient(PRBool * retval) {
    // we need to get the value from registry everytime
    // because the registry settings can be changed from
    // other mail applications.
    *retval = m_registryUtils.IsDefaultMailClient();
    return NS_OK;
}

NS_IMETHODIMP
nsMapiRegistry::GetShowDialog(PRBool * retval) {
    *retval = m_ShowDialog;
    return NS_OK;
}


NS_IMETHODIMP
nsMapiRegistry::SetIsDefaultMailClient(PRBool aIsDefaultMailClient) 
{
    nsresult rv = NS_OK ;

    if (aIsDefaultMailClient)
    {
        rv = m_registryUtils.setDefaultMailClient();
        if (NS_SUCCEEDED(rv))
            m_DefaultMailClient = PR_TRUE;
        else
            m_registryUtils.ShowMapiErrorDialog();
    }
    else
    {
        rv = m_registryUtils.unsetDefaultMailClient();
        if (NS_SUCCEEDED(rv))
            m_DefaultMailClient = PR_FALSE;
        else
            m_registryUtils.ShowMapiErrorDialog();
    }

    return rv ;
}

/** This will bring up the dialog box only once per session and 
 * only if the current app is not default Mail Client.
 * This also checks the registry if the registry key
 * showMapiDialog is set
 */
NS_IMETHODIMP
nsMapiRegistry::ShowMailIntegrationDialog(nsIDOMWindow *aParentWindow) {
    if (!m_ShowDialog || !m_registryUtils.getShowDialog() ||
        m_registryUtils.IsDefaultMailClient()) {
      return NS_OK;
    }

    nsresult rv;
    nsCOMPtr<nsIPromptService> promptService(do_GetService(
                  "@mozilla.org/embedcomp/prompt-service;1", &rv));
    if (NS_SUCCEEDED(rv) && promptService)
    {
        nsCOMPtr<nsIStringBundle> bundle;
        rv = m_registryUtils.MakeMapiStringBundle (getter_AddRefs (bundle)) ;
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

        nsXPIDLString dialogTitle;
        const PRUnichar *brandStrings[] = { m_registryUtils.brandName() };
        NS_NAMED_LITERAL_STRING(dialogTitlePropertyTag, "dialogTitle");
        rv = bundle->FormatStringFromName(dialogTitlePropertyTag.get(),
                                          brandStrings, 1,
                                          getter_Copies(dialogTitle));
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
        
        nsXPIDLString dialogText;
        NS_NAMED_LITERAL_STRING(dialogTextPropertyTag, "dialogText");
        rv = bundle->FormatStringFromName(dialogTextPropertyTag.get(),
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
        rv = promptService->ConfirmEx(aParentWindow,
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
        rv = m_registryUtils.SetRegistryKey(HKEY_LOCAL_MACHINE, "Software\\Mozilla\\Desktop", 
                                "showMapiDialog", (checkValue) ? "0" : "1");
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

        m_ShowDialog = PR_FALSE;
        if (!buttonPressed)
            rv = SetIsDefaultMailClient(PR_TRUE);
    }
    return rv;
}


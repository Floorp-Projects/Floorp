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
 * The Original Code is cookie manager code.
 *
 * The Initial Developer of the Original Code is
 * Michiel van Leeuwen (mvl@exedo.nl).
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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


#include "nsCookiePromptService.h"
#include "nsICookie.h"
#include "nsICookieAcceptDialog.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIWindowWatcher.h"
#include "nsIServiceManager.h"
#include "nsString.h"
#include "nsIDialogParamBlock.h"
#include "nsIMutableArray.h"

/****************************************************************
 ************************ nsCookiePromptService *****************
 ****************************************************************/

NS_IMPL_ISUPPORTS1(nsCookiePromptService, nsICookiePromptService)

nsCookiePromptService::nsCookiePromptService() {
}

nsCookiePromptService::~nsCookiePromptService() {
}

NS_IMETHODIMP
nsCookiePromptService::CookieDialog(nsIDOMWindow *aParent,
                                    nsICookie *aCookie,
                                    const nsACString &aHostname,
                                    PRInt32 aCookiesFromHost,
                                    PRBool aChangingCookie,
                                    PRBool *aRememberDecision,
                                    PRInt32 *aAccept)
{
  nsresult rv;

  nsCOMPtr<nsIDialogParamBlock> block = do_CreateInstance(NS_DIALOGPARAMBLOCK_CONTRACTID,&rv);
  if (NS_FAILED(rv)) return rv;

  block->SetInt(nsICookieAcceptDialog::ACCEPT_COOKIE, 1);
  block->SetString(nsICookieAcceptDialog::HOSTNAME, NS_ConvertUTF8toUTF16(aHostname).get());
  block->SetInt(nsICookieAcceptDialog::COOKIESFROMHOST, aCookiesFromHost);
  block->SetInt(nsICookieAcceptDialog::CHANGINGCOOKIE, aChangingCookie ? 1 : 0);
  
  nsCOMPtr<nsIMutableArray> objects =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = objects->AppendElement(aCookie, PR_FALSE);
  if (NS_FAILED(rv)) return rv;

  block->SetObjects(objects);

  nsCOMPtr<nsIWindowWatcher> wwatcher = do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsISupports> arguments = do_QueryInterface(block);
  nsCOMPtr<nsIDOMWindow> dialog;

  nsCOMPtr<nsIDOMWindow> parent(aParent);
  if (!parent) // if no parent provided, consult the window watcher:
    wwatcher->GetActiveWindow(getter_AddRefs(parent));

  if (parent) {
    nsCOMPtr<nsPIDOMWindow> privateParent(do_QueryInterface(parent));
    if (privateParent)
      privateParent = privateParent->GetPrivateRoot();
    parent = do_QueryInterface(privateParent);
  }

  // The cookie dialog will be modal for the root chrome window rather than the
  // tab containing the permission-requesting page.  This removes confusion
  // about which monitor is displaying the dialog (see bug 470356), but also
  // avoids unwanted tab switches (see bug 405239).
  rv = wwatcher->OpenWindow(parent, "chrome://cookie/content/cookieAcceptDialog.xul", "_blank",
                            "centerscreen,chrome,modal,titlebar", arguments,
                            getter_AddRefs(dialog));

  if (NS_FAILED(rv)) return rv;

  // get back output parameters
  PRInt32 tempValue;
  block->GetInt(nsICookieAcceptDialog::ACCEPT_COOKIE, &tempValue);
  *aAccept = tempValue;
  
  // GetInt returns a PRInt32; we need to sanitize it into PRBool
  block->GetInt(nsICookieAcceptDialog::REMEMBER_DECISION, &tempValue);
  *aRememberDecision = (tempValue == 1);

  return rv;
}


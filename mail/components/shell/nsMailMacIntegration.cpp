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
 * The Original Code is the Mozilla GNOME integration code.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Asaf Romano <mozilla.mano@sent.com>
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

#include "nsMailMacIntegration.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIStringBundle.h"
#include "nsIPromptService.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsEmbedCID.h"

// These Launch Services functions are undocumented. We're using them since
// they're the only way to set the default opener for URLs
extern "C" {
  // Returns the CFURL for application currently set as the default opener for
  // the given URL scheme. appURL must be released by the caller.
  extern OSStatus _LSCopyDefaultSchemeHandlerURL(CFStringRef scheme,
                                                 CFURLRef *appURL);
  extern OSStatus _LSSetDefaultSchemeHandlerURL(CFStringRef scheme,
                                                CFURLRef appURL);
  extern OSStatus _LSSaveAndRefresh(void);
}

NS_IMPL_ISUPPORTS1(nsMailMacIntegration, nsIMapiRegistry)

nsMailMacIntegration::nsMailMacIntegration()
{
  PRBool isDefault;
  GetIsDefaultMailClient(&isDefault);
  mShowMailDialog = !isDefault;
  GetIsDefaultNewsClient(&isDefault);
  mShowNewsDialog = !isDefault;
  GetIsDefaultFeedClient(&isDefault);
  mShowFeedDialog = !isDefault;
}

nsresult
nsMailMacIntegration::IsDefaultHandlerForProtocol(CFStringRef aScheme,
                                                  PRBool *aIsDefault)
{
  // Since neither Launch Services nor Internet Config actually differ between 
  // bundles which have the same bundle identifier (That is, if we set our
  // URL of our bundle as the default handler for the given protocol,
  // Launch Service might return the URL of another thunderbird bundle as the
  // defualt handler for that protocol), we are comparing the identifiers of the
  // bundles rather than their URLs.

  CFStringRef tbirdID = ::CFBundleGetIdentifier(CFBundleGetMainBundle());
  if (!tbirdID) {
    // CFBundleGetIdentifier is expected to return NULL only if the specified
    // bundle doesn't have a bundle identifier in its dictionary. In this case,
    // that means a failure, since our bundle does have an identifier.
    return NS_ERROR_FAILURE;
  }

  ::CFRetain(tbirdID);

  // Get the default handler URL of the given protocol
  CFURLRef defaultHandlerURL;
  OSStatus err = ::_LSCopyDefaultSchemeHandlerURL(aScheme,
                                                  &defaultHandlerURL);

  nsresult rv = NS_ERROR_FAILURE;
  if (err == noErr) {
    // Get a reference to the bundle (based on its URL)
    CFBundleRef defaultHandlerBundle = ::CFBundleCreate(NULL, 
                                                        defaultHandlerURL);
    if (defaultHandlerBundle) {
      CFStringRef defaultHandlerID =
        ::CFBundleGetIdentifier(defaultHandlerBundle);
      if (defaultHandlerID) {
        ::CFRetain(defaultHandlerID);
        // and compare it to our bundle identifier
        *aIsDefault = ::CFStringCompare(tbirdID, defaultHandlerID, 0)
                       == kCFCompareEqualTo;
        ::CFRelease(defaultHandlerID);
      }
      else {
        // If the bundle doesn't have an identifier in its info property list,
        // it's not our bundle.
        *aIsDefault = PR_FALSE;
      }

      ::CFRelease(defaultHandlerBundle);
      rv = NS_OK;
    }

    ::CFRelease(defaultHandlerURL);
  }
  else {
    // If |_LSCopyDefaultSchemeHandlerURL| failed, there's no default
    // handler for the given protocol
    *aIsDefault = PR_FALSE;
    rv = NS_OK;
  }

  ::CFRelease(tbirdID);
  return rv;
}

nsresult
nsMailMacIntegration::SetAsDefaultHandlerForProtocol(CFStringRef aScheme)
{
  CFURLRef tbirdURL = ::CFBundleCopyBundleURL(CFBundleGetMainBundle());

  ::_LSSetDefaultSchemeHandlerURL(aScheme, tbirdURL);
  ::_LSSaveAndRefresh();
  ::CFRelease(tbirdURL);

  return NS_OK;
}

NS_IMETHODIMP
nsMailMacIntegration::GetIsDefaultMailClient(PRBool *aIsDefault)
{
  return IsDefaultHandlerForProtocol(CFSTR("mailto"), aIsDefault);
}

NS_IMETHODIMP
nsMailMacIntegration::SetIsDefaultMailClient(PRBool aIsDefault)
{
  NS_ASSERTION(!aIsDefault, "Should never be called with aIsDefault=false");
  return SetAsDefaultHandlerForProtocol(CFSTR("mailto"));
}

NS_IMETHODIMP
nsMailMacIntegration::GetIsDefaultNewsClient(PRBool *aIsDefault)
{
  return IsDefaultHandlerForProtocol(CFSTR("news"), aIsDefault);
}

NS_IMETHODIMP
nsMailMacIntegration::SetIsDefaultNewsClient(PRBool aIsDefault)
{
  NS_ASSERTION(!aIsDefault, "Should never be called with aIsDefault=false");
  return SetAsDefaultHandlerForProtocol(CFSTR("news"));
}

NS_IMETHODIMP
nsMailMacIntegration::GetIsDefaultFeedClient(PRBool *aIsDefault)
{
  return IsDefaultHandlerForProtocol(CFSTR("feed"), aIsDefault);
}

NS_IMETHODIMP
nsMailMacIntegration::SetIsDefaultFeedClient(PRBool aIsDefault)
{
  NS_ASSERTION(!aIsDefault, "Should never be called with aIsDefault=false");
  return SetAsDefaultHandlerForProtocol(CFSTR("feed"));
}

NS_IMETHODIMP
nsMailMacIntegration::GetShowDialog(PRBool *aShow)
{
  *aShow = (mShowMailDialog || mShowNewsDialog || mShowFeedDialog);
  return NS_OK;
}

NS_IMETHODIMP
nsMailMacIntegration::ShowMailIntegrationDialog(nsIDOMWindow* aParentWindow)
{
  nsCOMPtr<nsIPrefService> pref = do_GetService(NS_PREFSERVICE_CONTRACTID);
  nsCOMPtr<nsIPrefBranch> branch;
  pref->GetBranch("mail.", getter_AddRefs(branch));

  PRBool showMailDialog, showNewsDialog, showFeedDialog;
  branch->GetBoolPref("checkDefaultMail", &showMailDialog);
  branch->GetBoolPref("checkDefaultNews", &showNewsDialog);
  branch->GetBoolPref("checkDefaultFeed", &showFeedDialog);

  if (!((mShowMailDialog && showMailDialog) ||
        (mShowNewsDialog && showNewsDialog) ||
        (mShowFeedDialog && showFeedDialog)))
    return NS_OK;

  nsCOMPtr<nsIStringBundleService> bundleService =
    do_GetService(NS_STRINGBUNDLE_CONTRACTID);
  NS_ENSURE_TRUE(bundleService, NS_ERROR_FAILURE);

  nsCOMPtr<nsIStringBundle> brandBundle;
  bundleService->CreateBundle("chrome://branding/locale/brand.properties",
                              getter_AddRefs(brandBundle));
  NS_ENSURE_TRUE(brandBundle, NS_ERROR_FAILURE);

  nsXPIDLString brandShortName;
  brandBundle->GetStringFromName(NS_LITERAL_STRING("brandShortName").get(),
                                 getter_Copies(brandShortName));

  nsCOMPtr<nsIStringBundle> mapiBundle;
  bundleService->CreateBundle("chrome://messenger-mapi/locale/mapi.properties",
                              getter_AddRefs(mapiBundle));
  NS_ENSURE_TRUE(mapiBundle, NS_ERROR_FAILURE);

  nsXPIDLString dialogTitle;
  const PRUnichar *brandStrings[] = { brandShortName.get() };

  nsresult rv =
    mapiBundle->FormatStringFromName(NS_LITERAL_STRING("dialogTitle").get(),
                                     brandStrings, 1,
                                     getter_Copies(dialogTitle));

  NS_ENSURE_SUCCESS(rv, rv);

  nsXPIDLString checkboxText;
  rv = mapiBundle->GetStringFromName(NS_LITERAL_STRING("checkboxText").get(),
                                     getter_Copies(checkboxText));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPromptService> promptService =
    do_GetService(NS_PROMPTSERVICE_CONTRACTID);
  NS_ENSURE_TRUE(promptService, NS_ERROR_FAILURE);

  if (mShowMailDialog && showMailDialog) {
    nsXPIDLString dialogText;
    rv = mapiBundle->FormatStringFromName(NS_LITERAL_STRING("dialogText").get(),
                                          brandStrings, 1,
                                          getter_Copies(dialogText));

    PRBool checkValue = PR_FALSE;
    PRInt32 buttonPressed = 0;
    rv = promptService->ConfirmEx(aParentWindow, dialogTitle, dialogText.get(),
                                  (nsIPromptService::BUTTON_TITLE_YES *
                                   nsIPromptService::BUTTON_POS_0) +
                                  (nsIPromptService::BUTTON_TITLE_NO *
                                   nsIPromptService::BUTTON_POS_1),
                                  nsnull, nsnull, nsnull,
                                  checkboxText, &checkValue, &buttonPressed);
    NS_ENSURE_SUCCESS(rv, rv);
    if (checkValue)
      branch->SetBoolPref("checkDefaultMail", PR_FALSE);
    mShowMailDialog = PR_FALSE;

    if (buttonPressed == 0) {
      rv = SetIsDefaultMailClient(PR_TRUE);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  if (mShowNewsDialog && showNewsDialog) {
    nsXPIDLString dialogText;
    rv = mapiBundle->FormatStringFromName(
                           NS_LITERAL_STRING("newsDialogText").get(),
                           brandStrings, 1, getter_Copies(dialogText));

    PRBool checkValue = PR_FALSE;
    PRInt32 buttonPressed = 0;
    rv = promptService->ConfirmEx(aParentWindow, dialogTitle, dialogText.get(),
                                  (nsIPromptService::BUTTON_TITLE_YES *
                                   nsIPromptService::BUTTON_POS_0) +
                                  (nsIPromptService::BUTTON_TITLE_NO *
                                   nsIPromptService::BUTTON_POS_1),
                                  nsnull, nsnull, nsnull,
                                  checkboxText, &checkValue, &buttonPressed);
    NS_ENSURE_SUCCESS(rv, rv);
    if (checkValue)
      branch->SetBoolPref("checkDefaultNews", PR_FALSE);
    mShowNewsDialog = PR_FALSE;

    if (buttonPressed == 0)
      rv = SetIsDefaultNewsClient(PR_TRUE);
  }

  if (mShowFeedDialog && showFeedDialog) {
    nsXPIDLString dialogText;
    rv = mapiBundle->FormatStringFromName(
                           NS_LITERAL_STRING("feedDialogText").get(),
                           brandStrings, 1, getter_Copies(dialogText));

    PRBool checkValue = PR_FALSE;
    PRInt32 buttonPressed = 0;
    rv = promptService->ConfirmEx(aParentWindow, dialogTitle, dialogText.get(),
                                  (nsIPromptService::BUTTON_TITLE_YES *
                                   nsIPromptService::BUTTON_POS_0) +
                                  (nsIPromptService::BUTTON_TITLE_NO *
                                   nsIPromptService::BUTTON_POS_1),
                                  nsnull, nsnull, nsnull,
                                  checkboxText, &checkValue, &buttonPressed);
    NS_ENSURE_SUCCESS(rv, rv);
    if (checkValue)
      branch->SetBoolPref("checkDefaultFeed", PR_FALSE);
    mShowMailDialog = PR_FALSE;

    if (buttonPressed == 0) {
      rv = SetIsDefaultFeedClient(PR_TRUE);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return rv;
}

NS_IMETHODIMP
nsMailMacIntegration::RegisterMailAndNewsClient()
{
  // Be gentle, do nothing. The OS will do it for us on the first launch
  // of Thunderbird (based on entries from the bundle's info property list).
  return NS_OK;
}

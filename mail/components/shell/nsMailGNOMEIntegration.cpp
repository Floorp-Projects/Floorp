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
 *  Brian Ryner <bryner@brianryner.com>
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

#include "nsMailGNOMEIntegration.h"
#include "nsIGenericFactory.h"
#include "nsIGConfService.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "prenv.h"
#include "nsICmdLineService.h"
#include "nsIStringBundle.h"
#include "nsIPromptService.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

#include <glib.h>
#include <limits.h>
#include <stdlib.h>

static const char* const sMailProtocols[] = {
  "mailto"
};

static const char* const sNewsProtocols[] = {
  "news",
  "snews",
  "nntp"
};

nsresult
nsMailGNOMEIntegration::Init()
{
  // GConf _must_ be available, or we do not allow CreateInstance to succeed.

  nsCOMPtr<nsIGConfService> gconf = do_GetService(NS_GCONFSERVICE_CONTRACTID);

  if (!gconf)
    return NS_ERROR_NOT_AVAILABLE;

  // Check G_BROKEN_FILENAMES.  If it's set, then filenames in glib use
  // the locale encoding.  If it's not set, they use UTF-8.
  mUseLocaleFilenames = PR_GetEnv("G_BROKEN_FILENAMES") != nsnull;

  // Get the path we were launched from.
  nsCOMPtr<nsICmdLineService> cmdService =
    do_GetService("@mozilla.org/appshell/commandLineService;1");
  if (!cmdService)
    return NS_ERROR_NOT_AVAILABLE;

  nsXPIDLCString programName;
  cmdService->GetProgramName(getter_Copies(programName));

  // Make sure we have an absolute pathname.
  if (programName[0] != '/') {
    // First search PATH if we were just launched as 'thunderbird-bin'.
    // If we were launched as './thunderbird-bin', this will just return
    // the original string.

    gchar *appPath = g_find_program_in_path(programName.get());

    // Now resolve it.
    char resolvedPath[PATH_MAX] = "";
    if (realpath(appPath, resolvedPath)) {
      mAppPath.Assign(resolvedPath);
    }

    g_free(appPath);
  } else {
    mAppPath.Assign(programName);
  }

  // strip "-bin" off of the binary name
  if (StringEndsWith(mAppPath, NS_LITERAL_CSTRING("-bin")))
    mAppPath.SetLength(mAppPath.Length() - 4);

  PRBool isDefault;
  nsMailGNOMEIntegration::GetIsDefaultMailClient(&isDefault);
  mShowMailDialog = !isDefault;

  nsMailGNOMEIntegration::GetIsDefaultNewsClient(&isDefault);
  mShowNewsDialog = !isDefault;

  return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsMailGNOMEIntegration, nsIMapiRegistry)

PRBool
nsMailGNOMEIntegration::KeyMatchesAppName(const char *aKeyValue) const
{
  gchar *commandPath;
  if (mUseLocaleFilenames) {
    gchar *nativePath = g_filename_from_utf8(aKeyValue, -1, NULL, NULL, NULL);
    if (!nativePath) {
      NS_ERROR("Error converting path to filesystem encoding");
      return PR_FALSE;
    }

    commandPath = g_find_program_in_path(nativePath);
    g_free(nativePath);
  } else {
    commandPath = g_find_program_in_path(aKeyValue);
  }

  if (!commandPath)
    return PR_FALSE;

  PRBool matches = mAppPath.Equals(commandPath);
  g_free(commandPath);
  return matches;
}

nsresult
nsMailGNOMEIntegration::CheckDefault(const char* const *aProtocols,
                                     unsigned int aLength, PRBool *aIsDefault)
{
  *aIsDefault = PR_FALSE;
  nsCOMPtr<nsIGConfService> gconf = do_GetService(NS_GCONFSERVICE_CONTRACTID);

  PRBool enabled;
  nsCAutoString handler;

  for (unsigned int i = 0; i < aLength; ++i) {
    handler.Truncate();
    nsresult rv = gconf->GetAppForProtocol(nsDependentCString(aProtocols[i]),
                                           &enabled, handler);
    NS_ENSURE_SUCCESS(rv, rv);

    // The string will be something of the form: [/path/to/]app "%s"
    // We want to remove all of the parameters and get just the binary name.

    gint argc;
    gchar **argv;

    if (g_shell_parse_argv(handler.get(), &argc, &argv, NULL) && argc > 0) {
      handler.Assign(argv[0]);
      g_strfreev(argv);
    } else {
      return NS_ERROR_FAILURE;
    }

    if (!KeyMatchesAppName(handler.get()) || !enabled)
      return NS_OK; // the handler is disabled or set to another app
  }

  *aIsDefault = PR_TRUE;
  return NS_OK;
}

nsresult
nsMailGNOMEIntegration::MakeDefault(const char* const *aProtocols,
                                    unsigned int aLength)
{
  nsCOMPtr<nsIGConfService> gconf = do_GetService(NS_GCONFSERVICE_CONTRACTID);
  nsCAutoString appKeyValue(mAppPath + NS_LITERAL_CSTRING(" \"%s\""));

  for (unsigned int i = 0; i < aLength; ++i) {
    nsresult rv = gconf->SetAppForProtocol(nsDependentCString(aProtocols[i]),
                                           appKeyValue);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMailGNOMEIntegration::GetIsDefaultMailClient(PRBool *aIsDefault)
{
  return CheckDefault(sMailProtocols, NS_ARRAY_LENGTH(sMailProtocols),
                      aIsDefault);
}

NS_IMETHODIMP
nsMailGNOMEIntegration::SetIsDefaultMailClient(PRBool aIsDefault)
{
  NS_ASSERTION(!aIsDefault, "Should never be called with aIsDefault=false");
  return MakeDefault(sMailProtocols, NS_ARRAY_LENGTH(sMailProtocols));
}

NS_IMETHODIMP
nsMailGNOMEIntegration::GetIsDefaultNewsClient(PRBool *aIsDefault)
{
  return CheckDefault(sNewsProtocols, NS_ARRAY_LENGTH(sNewsProtocols),
                      aIsDefault);
}

NS_IMETHODIMP
nsMailGNOMEIntegration::SetIsDefaultNewsClient(PRBool aIsDefault)
{
  NS_ASSERTION(!aIsDefault, "Should never be called with aIsDefault=false");
  return MakeDefault(sNewsProtocols, NS_ARRAY_LENGTH(sNewsProtocols));
}

NS_IMETHODIMP
nsMailGNOMEIntegration::GetShowDialog(PRBool *aShow)
{
  *aShow = (mShowMailDialog || mShowNewsDialog);
  return NS_OK;
}

NS_IMETHODIMP
nsMailGNOMEIntegration::ShowMailIntegrationDialog(nsIDOMWindow* aParentWindow)
{
  nsCOMPtr<nsIPrefService> pref = do_GetService(NS_PREFSERVICE_CONTRACTID);
  nsCOMPtr<nsIPrefBranch> branch;
  pref->GetBranch("", getter_AddRefs(branch));

  PRBool showMailDialog, showNewsDialog;
  branch->GetBoolPref("mail.checkDefaultMail", &showMailDialog);
  branch->GetBoolPref("mail.checkDefaultNews", &showNewsDialog);

  if (!((mShowMailDialog && showMailDialog) ||
        (mShowNewsDialog && showNewsDialog)))
    return NS_OK;

  nsCOMPtr<nsIStringBundleService> bundleService =
    do_GetService(NS_STRINGBUNDLE_CONTRACTID);
  NS_ENSURE_TRUE(bundleService, NS_ERROR_FAILURE);

  nsCOMPtr<nsIStringBundle> brandBundle;
  bundleService->CreateBundle("chrome://global/locale/brand.properties",
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
    do_GetService("@mozilla.org/embedcomp/prompt-service;1");
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
      branch->SetBoolPref("mail.checkDefaultMail", PR_FALSE);
    mShowMailDialog = PR_FALSE;

    if (buttonPressed == 0) {
      rv = nsMailGNOMEIntegration::SetIsDefaultMailClient(PR_TRUE);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  if (mShowNewsDialog && showNewsDialog) {
    nsXPIDLString dialogText;
    rv = mapiBundle->FormatStringFromName(NS_LITERAL_STRING("newsDialogText").get(),
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
      branch->SetBoolPref("mail.checkDefaultNews", PR_FALSE);
    mShowNewsDialog = PR_FALSE;

    if (buttonPressed == 0)
      rv = nsMailGNOMEIntegration::SetIsDefaultNewsClient(PR_TRUE);
  }

  return rv;
}

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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@netscape.com>
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

#import "nsAlertController.h"
#import "CHBrowserService.h"
#import "SecurityDialogs.h"
#import "MainController.h"

#include "nsString.h"
#include "nsIPrefBranch.h"
#include "nsIPrompt.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIServiceManagerUtils.h"

SecurityDialogs::SecurityDialogs()
{
  NS_INIT_ISUPPORTS();
}

SecurityDialogs::~SecurityDialogs()
{
}

NS_IMPL_ISUPPORTS2(SecurityDialogs, nsIBadCertListener, nsISecurityWarningDialogs)

// nsIBadCertListener implementation
/* boolean confirmUnknownIssuer (in nsIInterfaceRequestor socketInfo,
                                 in nsIX509Cert cert,
                                 out short certAddType); */
NS_IMETHODIMP
SecurityDialogs::ConfirmUnknownIssuer(nsIInterfaceRequestor *socketInfo,
                                      nsIX509Cert *cert, PRInt16 *outAddType,
                                      PRBool *_retval)
{
  *_retval = PR_TRUE;
  *outAddType = ADD_TRUSTED_FOR_SESSION;

  nsAlertController* controller = CHBrowserService::GetAlertController();
  if (!controller)
    return NS_ERROR_FAILURE;

  // HACK: there is no way to get which window this is for from the API. The
  // security team in mozilla just cheats and assumes the frontmost window so
  // that's what we'll do. Yes, it's wrong. Yes, it's skanky. Oh well.
  *outAddType = (PRBool)[controller unknownCert:[(MainController*)[NSApp delegate] getFrontmostBrowserWindow]];
  switch ( *outAddType ) {
  case nsIBadCertListener::ADD_TRUSTED_FOR_SESSION:
  case nsIBadCertListener::ADD_TRUSTED_PERMANENTLY:
    *_retval = PR_TRUE;
    break;
  default:
    *_retval = PR_FALSE;
  }

  return NS_OK;
}

/* boolean confirmMismatchDomain (in nsIInterfaceRequestor socketInfo,
                                  in nsAUTF8String targetURL,
                                  in nsIX509Cert cert); */
NS_IMETHODIMP
SecurityDialogs::ConfirmMismatchDomain(nsIInterfaceRequestor *socketInfo,
                                       const nsACString& targetURL,
                                       nsIX509Cert *cert, PRBool *_retval)
{
  nsAlertController* controller = CHBrowserService::GetAlertController();
  if (!controller)
    return NS_ERROR_FAILURE;

  // HACK: there is no way to get which window this is for from the API. The
  // security team in mozilla just cheats and assumes the frontmost window so
  // that's what we'll do. Yes, it's wrong. Yes, it's skanky. Oh well.
  *_retval = (PRBool)[controller badCert:[(MainController*)[NSApp delegate] getFrontmostBrowserWindow]];

  return NS_OK;
}


/* boolean confirmCertExpired (in nsIInterfaceRequestor socketInfo,
                               in nsIX509Cert cert); */
NS_IMETHODIMP
SecurityDialogs::ConfirmCertExpired(nsIInterfaceRequestor *socketInfo,
                                    nsIX509Cert *cert, PRBool *_retval)
{
  nsAlertController* controller = CHBrowserService::GetAlertController();
  if (!controller)
    return NS_ERROR_FAILURE;

  // HACK: there is no way to get which window this is for from the API. The
  // security team in mozilla just cheats and assumes the frontmost window so
  // that's what we'll do. Yes, it's wrong. Yes, it's skanky. Oh well.
  *_retval = (PRBool)[controller expiredCert:[(MainController*)[NSApp delegate] getFrontmostBrowserWindow]];

  return NS_OK;
}

NS_IMETHODIMP
SecurityDialogs::NotifyCrlNextupdate(nsIInterfaceRequestor *socketInfo,
                                     const nsACString& targetURL,
                                     nsIX509Cert *cert)
{
  // what does this do!?
  return NS_OK;
}


// nsISecurityWarningDialogs implementation
#define ENTER_SITE_PREF      "security.warn_entering_secure"
#define WEAK_SITE_PREF       "security.warn_entering_weak"
#define LEAVE_SITE_PREF      "security.warn_leaving_secure"
                                     
#define MIXEDCONTENT_PREF    "security.warn_viewing_mixed"
#define INSECURE_SUBMIT_PREF "security.warn_submit_insecure"

NS_IMETHODIMP
SecurityDialogs::ConfirmEnteringSecure(nsIInterfaceRequestor *ctx,
                                       PRBool *canceled)
{
  // I don't think any user cares they're entering a secure site.
  #if 0
  rv = AlertDialog(ctx, ENTER_SITE_PREF,
                   NS_LITERAL_STRING("EnterSecureMessage").get(),
                   NS_LITERAL_STRING("EnterSecureShowAgain").get());
  #endif

  *canceled = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
SecurityDialogs::ConfirmEnteringWeak(nsIInterfaceRequestor *ctx,
                                     PRBool *canceled)
{
  *canceled = PR_FALSE;
  return AlertDialog(ctx, WEAK_SITE_PREF,
                     NS_LITERAL_STRING("WeakSecureMessage").get(),
                     NS_LITERAL_STRING("WeakSecureShowAgain").get());
}

NS_IMETHODIMP
SecurityDialogs::ConfirmLeavingSecure(nsIInterfaceRequestor *ctx,
                                      PRBool *canceled)
{
  *canceled = PR_FALSE;
  return AlertDialog(ctx, LEAVE_SITE_PREF,
                     NS_LITERAL_STRING("LeaveSecureMessage").get(),
                     NS_LITERAL_STRING("LeaveSecureShowAgain").get());
}


NS_IMETHODIMP
SecurityDialogs::ConfirmMixedMode(nsIInterfaceRequestor *ctx, PRBool *canceled)
{
  *canceled = PR_FALSE;
  return AlertDialog(ctx, MIXEDCONTENT_PREF,
                     NS_LITERAL_STRING("MixedContentMessage").get(),
                     NS_LITERAL_STRING("MixedContentShowAgain").get());
}


NS_IMETHODIMP
SecurityDialogs::ConfirmPostToInsecure(nsIInterfaceRequestor *ctx,
                                       PRBool* _result)
{
  // no user cares about this. the first thing they do is turn it off.
  *_result = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
SecurityDialogs::ConfirmPostToInsecureFromSecure(nsIInterfaceRequestor *ctx,
                                                 PRBool* _result)
{
  nsAlertController* controller = CHBrowserService::GetAlertController();
  if (!controller)
    return NS_ERROR_FAILURE;

  // HACK: there is no way to get which window this is for from the API. The
  // security team in mozilla just cheats and assumes the frontmost window so
  // that's what we'll do. Yes, it's wrong. Yes, it's skanky. Oh well.
  *_result = (PRBool)[controller postToInsecureFromSecure:[(MainController*)[NSApp delegate] getFrontmostBrowserWindow]];

  return NS_OK;
}


// Private helper functions
nsresult
SecurityDialogs::EnsureSecurityStringBundle()
{
  if (!mSecurityStringBundle) {
    #define STRING_BUNDLE_URL "chrome://communicator/locale/security.properties"
    nsCOMPtr<nsIStringBundleService> service = do_GetService(NS_STRINGBUNDLE_CONTRACTID);
    if ( service ) {
          nsresult rv = service->CreateBundle(STRING_BUNDLE_URL, getter_AddRefs(mSecurityStringBundle));
          if (NS_FAILED(rv)) return rv;
    }
  }
  return NS_OK;
}


nsresult
SecurityDialogs::AlertDialog(nsIInterfaceRequestor *ctx, const char *prefName,
                             const PRUnichar *dialogMessageName,
                             const PRUnichar *showAgainName)
{
  nsresult rv = NS_OK;

  // Get user's preference for this alert
  nsCOMPtr<nsIPrefBranch> pref;
  PRBool prefValue = PR_TRUE;
  if ( prefName ) {
    pref = do_GetService("@mozilla.org/preferences-service;1");
    if ( pref )
      pref->GetBoolPref(prefName, &prefValue);

    // Stop if alert is not requested
    if (!prefValue) return NS_OK;
  }

  if ( NS_FAILED(rv = EnsureSecurityStringBundle()) )
    return rv;

  // Get Prompt to use
  nsCOMPtr<nsIPrompt> prompt = do_GetInterface(ctx);
  if (!prompt) return NS_ERROR_FAILURE;

  // Get messages strings from localization file
  nsXPIDLString windowTitle, message, dontShowAgain;
  mSecurityStringBundle->GetStringFromName(NS_LITERAL_STRING("Title").get(),
                                           getter_Copies(windowTitle));
  mSecurityStringBundle->GetStringFromName(dialogMessageName,
                                           getter_Copies(message));
  if ( prefName )
    mSecurityStringBundle->GetStringFromName(showAgainName,
                                             getter_Copies(dontShowAgain));
  if (!windowTitle.get() || !message.get()) return NS_ERROR_FAILURE;

  if ( prefName )
    rv = prompt->AlertCheck(windowTitle, message, dontShowAgain, &prefValue);
  else
    rv = prompt->AlertCheck(windowTitle, message, nil, nil);
  if (NS_FAILED(rv)) return rv;

  if (prefName && !prefValue)
    pref->SetBoolPref(prefName, PR_FALSE);

  return rv;
}

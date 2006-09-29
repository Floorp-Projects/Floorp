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
 *   Scott MacGregor <mscott@mozilla.org>
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

NS_IMPL_ISUPPORTS1(nsMailMacIntegration, nsIShellService)

nsMailMacIntegration::nsMailMacIntegration(): mCheckedThisSession(PR_FALSE)
{}

NS_IMETHODIMP
nsMailMacIntegration::IsDefaultClient(PRBool aStartupCheck, PRUint16 aApps, PRBool * aIsDefaultClient)
{
  *aIsDefaultClient = PR_TRUE;
  if (aApps & nsIShellService::MAIL)
    *aIsDefaultClient &= isDefaultHandlerForProtocol(CFSTR("mailto"));
  if (aApps & nsIShellService::NEWS)
    *aIsDefaultClient &= isDefaultHandlerForProtocol(CFSTR("news"));
  if (aApps & nsIShellService::RSS)
    *aIsDefaultClient &= isDefaultHandlerForProtocol(CFSTR("feed"));
  
  // if this is the first mail window, maintain internal state that we've
  // checked this session (so that subsequent window opens don't show the 
  // default client dialog.
  
  if (aStartupCheck)
    mCheckedThisSession = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsMailMacIntegration::SetDefaultClient(PRBool aForAllUsers, PRUint16 aApps)
{
  nsresult rv = NS_OK;
  if (aApps & nsIShellService::MAIL)
    rv |= setAsDefaultHandlerForProtocol(CFSTR("mailto"));
  if (aApps & nsIShellService::RSS)
    rv |= setAsDefaultHandlerForProtocol(CFSTR("news"));
  if (aApps & nsIShellService::RSS)
    rv |= setAsDefaultHandlerForProtocol(CFSTR("feed"));
  
  return rv;	
}

NS_IMETHODIMP
nsMailMacIntegration::GetShouldCheckDefaultClient(PRBool* aResult)
{
  if (mCheckedThisSession) 
  {
    *aResult = PR_FALSE;
    return NS_OK;
  }

  nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
  return prefs->GetBoolPref("mail.shell.checkDefaultClient", aResult);
}

NS_IMETHODIMP
nsMailMacIntegration::SetShouldCheckDefaultClient(PRBool aShouldCheck)
{
  nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
  return prefs->SetBoolPref("mail.shell.checkDefaultClient", aShouldCheck);
}

PRBool
nsMailMacIntegration::isDefaultHandlerForProtocol(CFStringRef aScheme)
{
  PRBool isDefault = PR_FALSE;
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
    return isDefault;
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
        isDefault = ::CFStringCompare(tbirdID, defaultHandlerID, 0)
                       == kCFCompareEqualTo;
        ::CFRelease(defaultHandlerID);
      }
      else {
        // If the bundle doesn't have an identifier in its info property list,
        // it's not our bundle.
        isDefault = PR_FALSE;
      }

      ::CFRelease(defaultHandlerBundle);
    }

    ::CFRelease(defaultHandlerURL);
  }
  else {
    // If |_LSCopyDefaultSchemeHandlerURL| failed, there's no default
    // handler for the given protocol
    isDefault = PR_FALSE;
  }

  ::CFRelease(tbirdID);
  return isDefault;
}

nsresult
nsMailMacIntegration::setAsDefaultHandlerForProtocol(CFStringRef aScheme)
{
  CFURLRef tbirdURL = ::CFBundleCopyBundleURL(CFBundleGetMainBundle());

  ::_LSSetDefaultSchemeHandlerURL(aScheme, tbirdURL);
  ::_LSSaveAndRefresh();
  ::CFRelease(tbirdURL);

  return NS_OK;
}




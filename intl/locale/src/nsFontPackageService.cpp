/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Frank Yung-Fong Tang <ftang@netscape.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsFontPackageService.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIFontEnumerator.h" 
#include "plstr.h"

enum {
  eInit = 0,
  eDownload,
  eInstalled
};

static PRInt8 mJAState = eInit;
static PRInt8 mKOState = eInit;
static PRInt8 mZHTWState = eInit;
static PRInt8 mZHCNState = eInit;

NS_IMPL_THREADSAFE_ISUPPORTS2(nsFontPackageService, 
   nsIFontPackageService,
   nsIFontPackageProxy);

/* from nsIFontPackageSercice.h */
nsFontPackageService::nsFontPackageService() : mHandler(nsnull)
{
  /* member initializers and constructor code */
}

nsFontPackageService::~nsFontPackageService()
{
  /* destructor code */
}

/* void SetHandler (in nsIFontPackageHandler aHandler); */
NS_IMETHODIMP nsFontPackageService::SetHandler(nsIFontPackageHandler *aHandler)
{
    mHandler = aHandler;
    return NS_OK;
}

/* void FontPackageHandled (in boolean aSuccess, in boolean aReloadPages, in string aFontPackID); */
NS_IMETHODIMP nsFontPackageService::FontPackageHandled(PRBool aSuccess, PRBool aReloadPages, const char *aFontPackID)
{
  if (strcmp(aFontPackID, "lang:ja") == 0) {
    mJAState = (aSuccess) ? eInstalled : eInit;
  }
  else if (strcmp(aFontPackID, "lang:ko") == 0) {
    mKOState = (aSuccess) ? eInstalled : eInit;
  }
  else if (strcmp(aFontPackID, "lang:zh-TW") == 0) {
    mZHTWState = (aSuccess) ? eInstalled : eInit;
  }
  else if (strcmp(aFontPackID, "lang:zh-CN") == 0) {
    mZHCNState = (aSuccess) ? eInstalled : eInit;
  }

  if (*aFontPackID == '\0' && (!aSuccess)) {
    // if aFontPackID is empty and failed , then reset to eInit.
    mJAState = mKOState = mZHTWState = mZHCNState = eInit;
  }

  return NS_OK;
}

nsresult nsFontPackageService::CallDownload(const char *aFontPackID, PRInt8 aInState, PRInt8 *aOutState)
{
  nsresult rv = NS_OK;

  if (aInState == eInit)  {
    nsCOMPtr<nsIFontEnumerator> fontEnum = do_GetService("@mozilla.org/gfx/fontenumerator;1", &rv);
    if (NS_SUCCEEDED(rv)) { 
      PRBool have = PR_FALSE;

      // aFontPackID="lang:xxxx", strip "lang:" and get the actual langID
      NS_ASSERTION((strncmp(aFontPackID, "lang:", 5) == 0), "Invalid FontPackID format.");
      const char *langID = &aFontPackID[5]; 
      rv = fontEnum->HaveFontFor(langID, &have);
      if (NS_SUCCEEDED(rv)) { 
        if (!have)  {
          *aOutState = eDownload;
          rv = mHandler->NeedFontPackage(aFontPackID);
          if (rv == NS_ERROR_ABORT) {
            *aOutState = eInit;
            rv = NS_OK;
          }
        }
        else  {
          *aOutState = eInstalled;
        }
      }
    }
  }

  return rv;
}

/* void NeedFontPackage (in string aFontPackID); */
NS_IMETHODIMP nsFontPackageService::NeedFontPackage(const char *aFontPackID)
{
  nsresult rv = NS_OK;
  if (!mHandler) {
    // create default handler
    mHandler = do_CreateInstance("@mozilla.org/locale/default-font-package-handler;1", &rv);
    if (NS_FAILED(rv)) return rv;
  }

  if (strcmp(aFontPackID, "lang:ja") == 0) {
    rv = CallDownload(aFontPackID, mJAState, &mJAState);
  }
  else if (strcmp(aFontPackID, "lang:ko") == 0) {
    rv = CallDownload(aFontPackID, mKOState, &mKOState);
  }
  else if (strcmp(aFontPackID, "lang:zh-TW") == 0) {
    rv = CallDownload(aFontPackID, mZHTWState, &mZHTWState);
  }
  else if (strcmp(aFontPackID, "lang:zh-CN") == 0) {
    rv = CallDownload(aFontPackID, mZHCNState, &mZHCNState);
  }

  return rv;
}

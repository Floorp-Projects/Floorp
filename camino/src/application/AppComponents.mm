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

#import <Cocoa/Cocoa.h>

#import "SecurityDialogs.h"
#import "CocoaPromptService.h"
#import "KeychainService.h"
#import "nsDownloadListener.h"
#import "ProgressDlgController.h"

#include "nsIGenericFactory.h"

// {0ffd3880-7a1a-11d6-a384-975d1d5f86fc}
#define NS_SECURITYDIALOGS_CID \
  {0x0ffd3880, 0x7a1a, 0x11d6,{0xa3, 0x84, 0x97, 0x5d, 0x1d, 0x5f, 0x86, 0xfc}}

#define NS_PROMPTSERVICE_CID \
  {0xa2112d6a, 0x0e28, 0x421f, {0xb4, 0x6a, 0x25, 0xc0, 0xb3, 0x8, 0xcb, 0xd0}}

#define NS_KEYCHAINPROMPT_CID                    \
    { 0x64997e60, 0x17fe, 0x11d4, {0x8c, 0xee, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3}}

NS_GENERIC_FACTORY_CONSTRUCTOR(SecurityDialogs);
NS_GENERIC_FACTORY_CONSTRUCTOR(CocoaPromptService);
NS_GENERIC_FACTORY_CONSTRUCTOR(KeychainPrompt);
//NS_GENERIC_FACTORY_CONSTRUCTOR(nsDownloadListener);

static NS_IMETHODIMP
nsDownloadListenerConstructor(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  *aResult = NULL;
  if (aOuter)
      return NS_ERROR_NO_AGGREGATION;

  nsDownloadListener* inst;
  NS_NEWXPCOM(inst, nsDownloadListener);
  if (!inst)
      return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(inst);
  inst->SetDisplayFactory([ProgressDlgController sharedDownloadController]);
  nsresult rv = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);
  return rv;
}


// used by MainController to register the components in which we want to override
// with the Gecko embed layer.

static const nsModuleComponentInfo gAppComponents[] = {
  {
    "Security Dialogs",
    NS_SECURITYDIALOGS_CID,
    NS_BADCERTLISTENER_CONTRACTID,
    SecurityDialogsConstructor
  },
  {
    "Security Dialogs",
    NS_SECURITYDIALOGS_CID,
    NS_SECURITYWARNINGDIALOGS_CONTRACTID,
    SecurityDialogsConstructor
  },
  {
    "Prompt Service",
    NS_PROMPTSERVICE_CID,
    "@mozilla.org/embedcomp/prompt-service;1",
    CocoaPromptServiceConstructor
  },
  {
    "Keychain Prompt",
    NS_KEYCHAINPROMPT_CID,
    "@mozilla.org/wallet/single-sign-on-prompt;1",
    KeychainPromptConstructor
  },
  {
    "Download",
    NS_DOWNLOAD_CID,
    NS_DOWNLOAD_CONTRACTID,
    nsDownloadListenerConstructor
  }
};


const nsModuleComponentInfo* GetAppComponents ( unsigned int * outNumComponents )
{
  *outNumComponents = sizeof(gAppComponents) / sizeof(nsModuleComponentInfo);
  return gAppComponents;
}

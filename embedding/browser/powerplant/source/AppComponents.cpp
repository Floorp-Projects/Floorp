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
 *  Brian Ryner <bryner@brianryner.com>
 *  Conrad Carlen <ccarlen@netscape.com>
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

#include "AppComponents.h"
#include "PromptService.h"
#include "UDownload.h"

#define NS_PROMPTSERVICE_CID \
  {0xa2112d6a, 0x0e28, 0x421f, {0xb4, 0x6a, 0x25, 0xc0, 0xb3, 0x8, 0xcb, 0xd0}}
#define NS_HELPERAPPLAUNCHERDIALOG_CID \
      {0xf68578eb, 0x6ec2, 0x4169, {0xae, 0x19, 0x8c, 0x62, 0x43, 0xf0, 0xab, 0xe1}}

NS_GENERIC_FACTORY_CONSTRUCTOR(CPromptService)
NS_GENERIC_FACTORY_CONSTRUCTOR(CDownload)
NS_GENERIC_FACTORY_CONSTRUCTOR(CHelperAppLauncherDialog)

static const nsModuleComponentInfo components[] = {
  {
    "Prompt Service",
    NS_PROMPTSERVICE_CID,
    "@mozilla.org/embedcomp/prompt-service;1",
    CPromptServiceConstructor
  },
  {
    "Download",
    NS_DOWNLOAD_CID,
    NS_DOWNLOAD_CONTRACTID,
    CDownloadConstructor
  },
  {
    NS_IHELPERAPPLAUNCHERDLG_CLASSNAME,
    NS_HELPERAPPLAUNCHERDIALOG_CID,
    NS_IHELPERAPPLAUNCHERDLG_CONTRACTID,
    CHelperAppLauncherDialogConstructor
  }
};

const nsModuleComponentInfo* GetAppModuleComponentInfo(int* outNumComponents)
{
  *outNumComponents = sizeof(components) / sizeof(components[0]);
  return components;
}


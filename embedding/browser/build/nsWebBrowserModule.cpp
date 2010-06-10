/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
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

#include "mozilla/ModuleUtils.h"
#include "nsIServiceManager.h"
#include "nsXPIDLString.h"

#include "nsEmbedCID.h"

#include "nsWebBrowser.h"
#include "nsCommandHandler.h"
#include "nsWebBrowserContentPolicy.h"

// Factory Constructors

NS_GENERIC_FACTORY_CONSTRUCTOR(nsWebBrowser)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsWebBrowserContentPolicy)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCommandHandler)

NS_DEFINE_NAMED_CID(NS_WEBBROWSER_CID);
NS_DEFINE_NAMED_CID(NS_COMMANDHANDLER_CID);
NS_DEFINE_NAMED_CID(NS_WEBBROWSERCONTENTPOLICY_CID);

static const mozilla::Module::CIDEntry kWebBrowserCIDs[] = {
    { &kNS_WEBBROWSER_CID, false, NULL, nsWebBrowserConstructor },
    { &kNS_COMMANDHANDLER_CID, false, NULL, nsCommandHandlerConstructor },
    { &kNS_WEBBROWSERCONTENTPOLICY_CID, false, NULL, nsWebBrowserContentPolicyConstructor },
    { NULL }
};

static const mozilla::Module::ContractIDEntry kWebBrowserContracts[] = {
    { NS_WEBBROWSER_CONTRACTID, &kNS_WEBBROWSER_CID },
    { NS_COMMANDHANDLER_CONTRACTID, &kNS_COMMANDHANDLER_CID },
    { NS_WEBBROWSERCONTENTPOLICY_CONTRACTID, &kNS_WEBBROWSERCONTENTPOLICY_CID },
    { NULL }
};

static const mozilla::Module::CategoryEntry kWebBrowserCategories[] = {
    { "content-policy", NS_WEBBROWSERCONTENTPOLICY_CONTRACTID, NS_WEBBROWSERCONTENTPOLICY_CONTRACTID },
    { NULL }
};

static const mozilla::Module kWebBrowserModule = {
    mozilla::Module::kVersion,
    kWebBrowserCIDs,
    kWebBrowserContracts,
    kWebBrowserCategories
};

NSMODULE_DEFN(Browser_Embedding_Module) = &kWebBrowserModule;




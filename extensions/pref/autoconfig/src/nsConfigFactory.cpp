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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mitesh Shah <mitesh@netscape.com>
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
#include "nsAutoConfig.h"
#include "nsReadConfig.h"
#include "nsIAppStartupNotifier.h"
#if defined(MOZ_LDAP_XPCOM)
#include "nsLDAPSyncQuery.h"
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsAutoConfig, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsReadConfig, Init)
#if defined(MOZ_LDAP_XPCOM)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLDAPSyncQuery)
#endif

NS_DEFINE_NAMED_CID(NS_AUTOCONFIG_CID);
NS_DEFINE_NAMED_CID(NS_READCONFIG_CID);
#if defined(MOZ_LDAP_XPCOM)
NS_DEFINE_NAMED_CID(NS_LDAPSYNCQUERY_CID);
#endif

static const mozilla::Module::CIDEntry kAutoConfigCIDs[] = {
  { &kNS_AUTOCONFIG_CID, false, NULL, nsAutoConfigConstructor },
  { &kNS_READCONFIG_CID, false, NULL, nsReadConfigConstructor },
#if defined(MOZ_LDAP_XPCOM)
  { &kNS_LDAPSYNCQUERY_CID, false, NULL, nsLDAPSyncQueryConstructor },
#endif
  { NULL }
};

static const mozilla::Module::ContractIDEntry kAutoConfigContracts[] = {
  { NS_AUTOCONFIG_CONTRACTID, &kNS_AUTOCONFIG_CID },
  { NS_READCONFIG_CONTRACTID, &kNS_READCONFIG_CID },
#if defined(MOZ_LDAP_XPCOM)
  { "@mozilla.org/ldapsyncquery;1", &kNS_LDAPSYNCQUERY_CID },
#endif
  { NULL }
};

static const mozilla::Module::CategoryEntry kAutoConfigCategories[] = {
  { "pref-config-startup", "ReadConfig Module", NS_READCONFIG_CONTRACTID },
  { NULL }
};

static const mozilla::Module kAutoConfigModule = {
  mozilla::Module::kVersion,
  kAutoConfigCIDs,
  kAutoConfigContracts,
  kAutoConfigCategories
};

NSMODULE_DEFN(nsAutoConfigModule) = &kAutoConfigModule;

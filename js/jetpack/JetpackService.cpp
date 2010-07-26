/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
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
 * The Original Code is Mozilla Firefox.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation <http://www.mozilla.org>.
 * Portions created by the Initial Developer are Copyright (C) 2010
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

#include "base/basictypes.h"
#include "mozilla/jetpack/JetpackService.h"

#include "mozilla/jetpack/JetpackParent.h"
#include "nsIJetpack.h"

#include "mozilla/ModuleUtils.h"

#include "nsIXPConnect.h"

namespace mozilla {
namespace jetpack {

NS_IMPL_ISUPPORTS1(JetpackService,
                   nsIJetpackService)

NS_IMETHODIMP
JetpackService::CreateJetpack(nsIJetpack** aResult)
{
  nsresult rv;
  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAXPCNativeCallContext* ncc = NULL;
  rv = xpc->GetCurrentNativeCallContext(&ncc);
  NS_ENSURE_SUCCESS(rv, rv);

  JSContext* cx;
  rv = ncc->GetJSContext(&cx);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<JetpackParent> j = new JetpackParent(cx);
  *aResult = j.forget().get();

  return NS_OK;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(JetpackService)

} // namespace jetpack
} // namespace mozilla

#define JETPACKSERVICE_CID \
{ 0x4cf18fcd, 0x4247, 0x4388, \
  { 0xb1, 0x88, 0xb0, 0x72, 0x2a, 0xc0, 0x52, 0x21 } }

NS_DEFINE_NAMED_CID(JETPACKSERVICE_CID);

static const mozilla::Module::CIDEntry kJetpackCIDs[] = {
  { &kJETPACKSERVICE_CID, false, NULL, mozilla::jetpack::JetpackServiceConstructor },
  { NULL }
};

static const mozilla::Module::ContractIDEntry kJetpackContracts[] = {
  { "@mozilla.org/jetpack/service;1", &kJETPACKSERVICE_CID },
  { NULL }
};

static const mozilla::Module kJetpackModule = {
  mozilla::Module::kVersion,
  kJetpackCIDs,
  kJetpackContracts
};

NSMODULE_DEFN(jetpack) = &kJetpackModule;


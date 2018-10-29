/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsCRT.h"
#include "nsIAuthModule.h"

//-----------------------------------------------------------------------------
#include "nsAuthGSSAPI.h"
#if defined( USE_SSPI )
#include "nsAuthSSPI.h"
#else
#include "nsAuthSambaNTLM.h"
#endif
#include "nsAuthSASL.h"
#include "nsNTLMAuthModule.h"
#include "nsNSSComponent.h"

static const mozilla::Module::CIDEntry kAuthCIDs[] = {
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kAuthContracts[] = {
  { nullptr }
};

// static
already_AddRefed<nsIAuthModule>
nsIAuthModule::CreateInstance(const char* aType)
{
  nsCOMPtr<nsIAuthModule> auth;
  if (!nsCRT::strcmp(aType, "kerb-gss")) {
    auth = new nsAuthGSSAPI(PACKAGE_TYPE_KERBEROS);
  } else if (!nsCRT::strcmp(aType, "negotiate-gss")) {
    auth = new nsAuthGSSAPI(PACKAGE_TYPE_NEGOTIATE);
#if defined( USE_SSPI )
  } else if (!nsCRT::strcmp(aType, "negotiate-sspi")) {
    auth = new nsAuthSSPI();
  } else if (!nsCRT::strcmp(aType, "kerb-sspi")) {
    auth = new nsAuthSSPI(PACKAGE_TYPE_KERBEROS);
  } else if (!nsCRT::strcmp(aType, "sys-ntlm")) {
    auth = new nsAuthSSPI(PACKAGE_TYPE_NTLM);
#elif !defined(XP_MACOSX)
  } else if (!nsCRT::strcmp(aType, "sys-ntlm")) {
    RefPtr<nsAuthSambaNTLM> sambaAuth = new nsAuthSambaNTLM();

    nsresult rv = sambaAuth->SpawnNTLMAuthHelper();
    if (NS_FAILED(rv)) {
      // Failure here probably means that cached credentials were not available
      return nullptr;
    }

    auth = sambaAuth.forget();
#endif
  } else if (!nsCRT::strcmp(aType, "sasl-gssapi")) {
    auth = new nsAuthSASL();
  } else if (!nsCRT::strcmp(aType, "ntlm") &&
    XRE_IsParentProcess() &&
    EnsureNSSInitializedChromeOrContent()) {
    RefPtr<nsNTLMAuthModule> ntlmAuth = new nsNTLMAuthModule();

    nsresult rv = ntlmAuth->InitTest();
    if (NS_FAILED(rv)) {
      return nullptr;
    }

    auth = ntlmAuth.forget();
  } else {
    return nullptr;
  }

  return auth.forget();
}

//-----------------------------------------------------------------------------
mozilla::LazyLogModule gNegotiateLog("negotiateauth");

// setup nspr logging ...
static nsresult
InitNegotiateAuth()
{
  return NS_OK;
}

static void
DestroyNegotiateAuth()
{
  nsAuthGSSAPI::Shutdown();
}

static const mozilla::Module kAuthModule = {
  mozilla::Module::kVersion,
  kAuthCIDs,
  kAuthContracts,
  nullptr,
  nullptr,
  InitNegotiateAuth,
  DestroyNegotiateAuth
};

NSMODULE_DEFN(nsAuthModule) = &kAuthModule;

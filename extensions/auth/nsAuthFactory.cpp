/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsAuth.h"

//-----------------------------------------------------------------------------

#define NS_HTTPNEGOTIATEAUTH_CID                   \
{ /* 75c80fd0-accb-432c-af59-ec60668c3990 */       \
  0x75c80fd0,                                      \
  0xaccb,                                          \
  0x432c,                                          \
  {0xaf, 0x59, 0xec, 0x60, 0x66, 0x8c, 0x39, 0x90} \
}

#include "nsHttpNegotiateAuth.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHttpNegotiateAuth)
//-----------------------------------------------------------------------------

#define NS_NEGOTIATEAUTH_CID                       \
{ /* 96ec4163-efc8-407a-8735-007fb26be4e8 */       \
  0x96ec4163,                                      \
  0xefc8,                                          \
  0x407a,                                          \
  {0x87, 0x35, 0x00, 0x7f, 0xb2, 0x6b, 0xe4, 0xe8} \
}
#define NS_GSSAUTH_CID                             \
{ /* dc8e21a0-03e4-11da-8cd6-0800200c9a66 */       \
  0xdc8e21a0,                                      \
  0x03e4,                                          \
  0x11da,                                          \
  {0x8c, 0xd6, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66} \
}

#include "nsAuthGSSAPI.h"

#if defined( USE_SSPI )
#include "nsAuthSSPI.h"

static nsresult
nsSysNTLMAuthConstructor(nsISupports *outer, REFNSIID iid, void **result)
{
  if (outer)
    return NS_ERROR_NO_AGGREGATION;

  nsAuthSSPI *auth = new nsAuthSSPI(PACKAGE_TYPE_NTLM);
  if (!auth)
    return NS_ERROR_OUT_OF_MEMORY;
  
  NS_ADDREF(auth);
  nsresult rv = auth->QueryInterface(iid, result);
  NS_RELEASE(auth);
  return rv;
}

static nsresult
nsKerbSSPIAuthConstructor(nsISupports *outer, REFNSIID iid, void **result)
{
  if (outer)
    return NS_ERROR_NO_AGGREGATION;

  nsAuthSSPI *auth = new nsAuthSSPI(PACKAGE_TYPE_KERBEROS);
  if (!auth)
    return NS_ERROR_OUT_OF_MEMORY;
  
  NS_ADDREF(auth);
  nsresult rv = auth->QueryInterface(iid, result);
  NS_RELEASE(auth);
  return rv;
}

#define NS_SYSNTLMAUTH_CID                         \
{ /* dc195987-6e9a-47bc-b1fd-ab895d398833 */       \
  0xdc195987,                                      \
  0x6e9a,                                          \
  0x47bc,                                          \
  {0xb1, 0xfd, 0xab, 0x89, 0x5d, 0x39, 0x88, 0x33} \
}

#define NS_NEGOTIATEAUTHSSPI_CID                   \
{ /* 78d3b0c0-0241-11da-8cd6-0800200c9a66 */       \
  0x78d3b0c0,                                      \
  0x0241,                                          \
  0x11da,                                          \
  {0x8c, 0xd6, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66} \
}

#define NS_KERBAUTHSSPI_CID                        \
{ /* 8c3a0e20-03e5-11da-8cd6-0800200c9a66 */       \
  0x8c3a0e20,                                      \
  0x03e5,                                          \
  0x11da,                                          \
  {0x8c, 0xd6, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66} \
}

#else

#define NS_SAMBANTLMAUTH_CID                       \
{ /* bc54f001-6eb0-4e32-9f49-7e064d8e70ef */       \
  0xbc54f001,                                      \
  0x6eb0,                                          \
  0x4e32,                                          \
  {0x9f, 0x49, 0x7e, 0x06, 0x4d, 0x8e, 0x70, 0xef} \
}

#include "nsAuthSambaNTLM.h"
static nsresult
nsSambaNTLMAuthConstructor(nsISupports *outer, REFNSIID iid, void **result)
{
  if (outer)
    return NS_ERROR_NO_AGGREGATION;

  RefPtr<nsAuthSambaNTLM> auth = new nsAuthSambaNTLM();
  if (!auth)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = auth->SpawnNTLMAuthHelper();
  if (NS_FAILED(rv)) {
    // Failure here probably means that cached credentials were not available
    return rv;
  }

  return auth->QueryInterface(iid, result);
}

#endif

static nsresult
nsKerbGSSAPIAuthConstructor(nsISupports *outer, REFNSIID iid, void **result)
{
  if (outer)
    return NS_ERROR_NO_AGGREGATION;

  nsAuthGSSAPI *auth = new nsAuthGSSAPI(PACKAGE_TYPE_KERBEROS);
  if (!auth)
    return NS_ERROR_OUT_OF_MEMORY;
  
  NS_ADDREF(auth);
  nsresult rv = auth->QueryInterface(iid, result);
  NS_RELEASE(auth);
  return rv;
}

static nsresult
nsGSSAPIAuthConstructor(nsISupports *outer, REFNSIID iid, void **result)
{
  if (outer)
    return NS_ERROR_NO_AGGREGATION;

  nsAuthGSSAPI *auth = new nsAuthGSSAPI(PACKAGE_TYPE_NEGOTIATE);
  if (!auth)
    return NS_ERROR_OUT_OF_MEMORY;
  
  NS_ADDREF(auth);
  nsresult rv = auth->QueryInterface(iid, result);
  NS_RELEASE(auth);
  return rv;
}


#if defined( USE_SSPI )
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAuthSSPI)
#endif

#define NS_AUTHSASL_CID                            \
{ /* 815e42e0-72cc-480f-934b-148e33c228a6 */       \
  0x815e42e0,                                      \
  0x72cc,                                          \
  0x480f,                                          \
  {0x93, 0x4b, 0x14, 0x8e, 0x33, 0xc2, 0x28, 0xa6} \
}

#include "nsAuthSASL.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAuthSASL)

NS_DEFINE_NAMED_CID(NS_GSSAUTH_CID);
NS_DEFINE_NAMED_CID(NS_NEGOTIATEAUTH_CID);
#if defined( USE_SSPI )
NS_DEFINE_NAMED_CID(NS_NEGOTIATEAUTHSSPI_CID);
NS_DEFINE_NAMED_CID(NS_KERBAUTHSSPI_CID);
NS_DEFINE_NAMED_CID(NS_SYSNTLMAUTH_CID);
#else
NS_DEFINE_NAMED_CID(NS_SAMBANTLMAUTH_CID);
#endif
NS_DEFINE_NAMED_CID(NS_HTTPNEGOTIATEAUTH_CID);
NS_DEFINE_NAMED_CID(NS_AUTHSASL_CID);


static const mozilla::Module::CIDEntry kAuthCIDs[] = {
  { &kNS_GSSAUTH_CID, false, nullptr, nsKerbGSSAPIAuthConstructor },
  { &kNS_NEGOTIATEAUTH_CID, false, nullptr, nsGSSAPIAuthConstructor },
#if defined( USE_SSPI )
  { &kNS_NEGOTIATEAUTHSSPI_CID, false, nullptr, nsAuthSSPIConstructor },
  { &kNS_KERBAUTHSSPI_CID, false, nullptr, nsKerbSSPIAuthConstructor },
  { &kNS_SYSNTLMAUTH_CID, false, nullptr, nsSysNTLMAuthConstructor },
#else
  { &kNS_SAMBANTLMAUTH_CID, false, nullptr, nsSambaNTLMAuthConstructor },
#endif
  { &kNS_HTTPNEGOTIATEAUTH_CID, false, nullptr, nsHttpNegotiateAuthConstructor },
  { &kNS_AUTHSASL_CID, false, nullptr, nsAuthSASLConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kAuthContracts[] = {
  { NS_AUTH_MODULE_CONTRACTID_PREFIX "kerb-gss", &kNS_GSSAUTH_CID },
  { NS_AUTH_MODULE_CONTRACTID_PREFIX "negotiate-gss", &kNS_NEGOTIATEAUTH_CID },
#if defined( USE_SSPI )
  { NS_AUTH_MODULE_CONTRACTID_PREFIX "negotiate-sspi", &kNS_NEGOTIATEAUTHSSPI_CID },
  { NS_AUTH_MODULE_CONTRACTID_PREFIX "kerb-sspi", &kNS_KERBAUTHSSPI_CID },
  { NS_AUTH_MODULE_CONTRACTID_PREFIX "sys-ntlm", &kNS_SYSNTLMAUTH_CID },
#else
  { NS_AUTH_MODULE_CONTRACTID_PREFIX "sys-ntlm", &kNS_SAMBANTLMAUTH_CID },
#endif
  { NS_HTTP_AUTHENTICATOR_CONTRACTID_PREFIX "negotiate", &kNS_HTTPNEGOTIATEAUTH_CID },
  { NS_AUTH_MODULE_CONTRACTID_PREFIX "sasl-gssapi", &kNS_AUTHSASL_CID },
  { nullptr }
};

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

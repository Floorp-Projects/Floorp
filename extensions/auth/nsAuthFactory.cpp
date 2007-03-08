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
 * The Original Code is the Negotiateauth
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
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

#include "nsIGenericFactory.h"
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

static NS_METHOD
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

static NS_METHOD
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
static NS_METHOD
nsSambaNTLMAuthConstructor(nsISupports *outer, REFNSIID iid, void **result)
{
  if (outer)
    return NS_ERROR_NO_AGGREGATION;

  nsCOMPtr<nsAuthSambaNTLM> auth = new nsAuthSambaNTLM();
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

static NS_METHOD
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

static NS_METHOD
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

//-----------------------------------------------------------------------------

static nsModuleComponentInfo components[] = {
  { "nsAuthKerbGSS", 
    NS_GSSAUTH_CID,
    NS_AUTH_MODULE_CONTRACTID_PREFIX "kerb-gss",
    nsKerbGSSAPIAuthConstructor
  },
  { "nsAuthNegoGSSAPI", 
    NS_NEGOTIATEAUTH_CID,
    NS_AUTH_MODULE_CONTRACTID_PREFIX "negotiate-gss",
    nsGSSAPIAuthConstructor
  },
#if defined( USE_SSPI )
  { "nsAuthNegoSSPI", 
    NS_NEGOTIATEAUTHSSPI_CID,
    NS_AUTH_MODULE_CONTRACTID_PREFIX "negotiate-sspi",
    nsAuthSSPIConstructor
  },
  { "nsAuthKerbSSPI", 
    NS_KERBAUTHSSPI_CID,
    NS_AUTH_MODULE_CONTRACTID_PREFIX "kerb-sspi",
    nsKerbSSPIAuthConstructor
  },
  { "nsAuthSYSNTLM", 
    NS_SYSNTLMAUTH_CID,
    NS_AUTH_MODULE_CONTRACTID_PREFIX "sys-ntlm",
    nsSysNTLMAuthConstructor
  },
#else
  { "nsAuthSambaNTLM", 
    NS_SAMBANTLMAUTH_CID,
    NS_AUTH_MODULE_CONTRACTID_PREFIX "sys-ntlm",
    nsSambaNTLMAuthConstructor
  },
#endif
  { "nsHttpNegotiateAuth", 
    NS_HTTPNEGOTIATEAUTH_CID,
    NS_HTTP_AUTHENTICATOR_CONTRACTID_PREFIX "negotiate",
    nsHttpNegotiateAuthConstructor
  },
  { "nsAuthSASL",
    NS_AUTHSASL_CID,
    NS_AUTH_MODULE_CONTRACTID_PREFIX "sasl-gssapi",
    nsAuthSASLConstructor
  }
};

//-----------------------------------------------------------------------------
#if defined( PR_LOGGING )
PRLogModuleInfo *gNegotiateLog;

// setup nspr logging ...
PR_STATIC_CALLBACK(nsresult)
InitNegotiateAuth(nsIModule *self)
{
  gNegotiateLog = PR_NewLogModule("negotiateauth");
  return NS_OK;
}
#else
#define InitNegotiateAuth nsnull
#endif

NS_IMPL_NSGETMODULE_WITH_CTOR(nsAuthModule, components, InitNegotiateAuth)

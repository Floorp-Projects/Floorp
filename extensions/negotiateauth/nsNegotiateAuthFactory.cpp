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
#include "nsNegotiateAuth.h"

//-----------------------------------------------------------------------------

#define NS_HTTPNEGOTIATEAUTH_CID \
{ /* 75c80fd0-accb-432c-af59-ec60668c3990 */         \
    0x75c80fd0,                                      \
    0xaccb,                                          \
    0x432c,                                          \
    {0xaf, 0x59, 0xec, 0x60, 0x66, 0x8c, 0x39, 0x90} \
}

#include "nsHttpNegotiateAuth.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHttpNegotiateAuth)

//-----------------------------------------------------------------------------

#define NS_NEGOTIATEAUTH_CID                         \
{ /* 96ec4163-efc8-407a-8735-007fb26be4e8 */         \
    0x96ec4163,                                      \
    0xefc8,                                          \
    0x407a,                                          \
    {0x87, 0x35, 0x00, 0x7f, 0xb2, 0x6b, 0xe4, 0xe8} \
}

#if defined( USE_GSSAPI )
#include "nsNegotiateAuthGSSAPI.h"
#elif defined( USE_SSPI )
#include "nsNegotiateAuthSSPI.h"
#else
#error "missing implementation"
#endif
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNegotiateAuth)

//-----------------------------------------------------------------------------

static nsModuleComponentInfo components[] = {
  { "nsNegotiateAuth", 
    NS_NEGOTIATEAUTH_CID,
    NS_AUTH_MODULE_CONTRACTID_PREFIX "negotiate",
    nsNegotiateAuthConstructor
  },
  { "nsHttpNegotiateAuth", 
    NS_HTTPNEGOTIATEAUTH_CID,
    NS_HTTP_AUTHENTICATOR_CONTRACTID_PREFIX "negotiate",
    nsHttpNegotiateAuthConstructor
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

NS_IMPL_NSGETMODULE_WITH_CTOR(nsNegotiateAuthModule, components, InitNegotiateAuth)

/* vim:set ts=4 sw=4 sts=4 et cindent: */
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
 * The Initial Developer of the Original Code is Daniel Kouril.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Kouril <kouril@ics.muni.cz> (original author)
 *   Wyllys Ingersoll <wyllys.ingersoll@sun.com>
 *   Christopher Nebergall <cneberg@sandia.gov>
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

//
// HTTP Negotiate Authentication Support Module
//
// Described by IETF Internet draft: draft-brezak-kerberos-http-00.txt
// (formerly draft-brezak-spnego-http-04.txt)
//
// Also described here:
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/dnsecure/html/http-sso-1.asp
//

#include <string.h>
#include <stdlib.h>

#include "nsNegotiateAuth.h"
#include "nsHttpNegotiateAuth.h"

#include "nsIHttpChannel.h"
#include "nsIAuthModule.h"
#include "nsIServiceManager.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsNetCID.h"
#include "plbase64.h"
#include "plstr.h"
#include "prprf.h"
#include "prlog.h"
#include "prmem.h"

//-----------------------------------------------------------------------------

static const char kNegotiate[] = "Negotiate";
static const char kNegotiateAuthTrustedURIs[] = "network.negotiate-auth.trusted-uris";
static const char kNegotiateAuthDelegationURIs[] = "network.negotiate-auth.delegation-uris";

#define kNegotiateLen  (sizeof(kNegotiate)-1)

//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpNegotiateAuth::GetAuthFlags(PRUint32 *flags)
{
    //
    // Negotiate Auth creds should not be reused across multiple requests.
    // Only perform the negotiation when it is explicitly requested by the
    // server.  Thus, do *NOT* use the "REUSABLE_CREDENTIALS" flag here.
    //
    // CONNECTION_BASED is specified instead of REQUEST_BASED since we need
    // to complete a sequence of transactions with the server over the same
    // connection.
    //
    *flags = CONNECTION_BASED | IDENTITY_IGNORED; 
    return NS_OK;
}

//
// Always set *identityInvalid == FALSE here.  This 
// will prevent the browser from popping up the authentication
// prompt window.  Because GSSAPI does not have an API
// for fetching initial credentials (ex: A Kerberos TGT),
// there is no correct way to get the users credentials.
// 
NS_IMETHODIMP
nsHttpNegotiateAuth::ChallengeReceived(nsIHttpChannel *httpChannel,
                                       const char *challenge,
                                       PRBool isProxyAuth,
                                       nsISupports **sessionState,
                                       nsISupports **continuationState,
                                       PRBool *identityInvalid)
{
    nsIAuthModule *module = (nsIAuthModule *) *continuationState;

    *identityInvalid = PR_FALSE;
    if (module)
        return NS_OK;

    // proxy auth not supported
    if (isProxyAuth)
        return NS_ERROR_ABORT;

    nsresult rv;

    nsCOMPtr<nsIURI> uri;
    rv = httpChannel->GetURI(getter_AddRefs(uri));
    if (NS_FAILED(rv))
        return rv;

    PRBool allowed = TestPref(uri, kNegotiateAuthTrustedURIs);
    if (!allowed) {
        LOG(("nsHttpNegotiateAuth::ChallengeReceived URI blocked\n"));
        return NS_ERROR_ABORT;
    }

    PRUint32 req_flags = nsIAuthModule::REQ_DEFAULT;

    PRBool delegation = TestPref(uri, kNegotiateAuthDelegationURIs);
    if (delegation) {
        LOG(("  using REQ_DELEGATE\n"));
        req_flags |= nsIAuthModule::REQ_DELEGATE;
    }

    nsCAutoString service;
    rv = uri->GetAsciiHost(service);
    if (NS_FAILED(rv))
        return rv;

    LOG(("  hostname = %s\n", service.get()));

    //
    // The correct service name for IIS servers is "HTTP/f.q.d.n", so
    // construct the proper service name for passing to "gss_import_name".
    //
    // TODO: Possibly make this a configurable service name for use
    // with non-standard servers that use stuff like "khttp/f.q.d.n" 
    // instead.
    //
    service.Insert("HTTP@", 0);

    rv = CallCreateInstance(NS_AUTH_MODULE_CONTRACTID_PREFIX "negotiate", &module);
    if (NS_FAILED(rv))
        return rv;

    rv = module->Init(service.get(), req_flags, nsnull, nsnull, nsnull);
    if (NS_FAILED(rv)) {
        NS_RELEASE(module);
        return rv;
    }

    *continuationState = module;
    return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsHttpNegotiateAuth, nsIHttpAuthenticator)
   
//
// GenerateCredentials
//
// This routine is responsible for creating the correct authentication
// blob to pass to the server that requested "Negotiate" authentication.
//
NS_IMETHODIMP
nsHttpNegotiateAuth::GenerateCredentials(nsIHttpChannel *httpChannel,
                                         const char *challenge,
                                         PRBool isProxyAuth,
                                         const PRUnichar *domain,
                                         const PRUnichar *username,
                                         const PRUnichar *password,
                                         nsISupports **sessionState,
                                         nsISupports **continuationState,
                                         char **creds)
{
    // ChallengeReceived must have been called previously.
    nsIAuthModule *module = (nsIAuthModule *) *continuationState;
    NS_ENSURE_TRUE(module, NS_ERROR_NOT_INITIALIZED);

    LOG(("nsHttpNegotiateAuth::GenerateCredentials() [challenge=%s]\n", challenge));

    NS_ASSERTION(creds, "null param");

#ifdef DEBUG
    PRBool isGssapiAuth =
        !PL_strncasecmp(challenge, kNegotiate, kNegotiateLen);
    NS_ENSURE_TRUE(isGssapiAuth, NS_ERROR_UNEXPECTED);
#endif

    //
    // If the "Negotiate:" header had some data associated with it,
    // that data should be used as the input to this call.  This may
    // be a continuation of an earlier call because GSSAPI authentication
    // often takes multiple round-trips to complete depending on the
    // context flags given.  We want to use MUTUAL_AUTHENTICATION which
    // generally *does* require multiple round-trips.  Don't assume
    // auth can be completed in just 1 call.
    //
    unsigned int len = strlen(challenge);

    void *inToken, *outToken;
    PRUint32 inTokenLen, outTokenLen;

    if (len > kNegotiateLen) {
        challenge += kNegotiateLen;
        while (*challenge == ' ')
            challenge++;
        len = strlen(challenge);

        inTokenLen = (len * 3)/4;
        inToken = malloc(inTokenLen);
        if (!inToken)
            return (NS_ERROR_OUT_OF_MEMORY);

        // strip off any padding (see bug 230351)
        while (challenge[len - 1] == '=')
            len--;

        //
        // Decode the response that followed the "Negotiate" token
        //
        if (PL_Base64Decode(challenge, len, (char *) inToken) == NULL) {
            free(inToken);
            return(NS_ERROR_UNEXPECTED);
        }
    }
    else {
        //
        // Initializing, don't use an input token.
        //
        inToken = nsnull;
        inTokenLen = 0;
    }

    nsresult rv = module->GetNextToken(inToken, inTokenLen, &outToken, &outTokenLen);

    free(inToken);

    if (NS_FAILED(rv))
        return rv;

    if (outTokenLen == 0) {
        LOG(("  No output token to send, exiting"));
        return NS_ERROR_FAILURE;
    }

    //
    // base64 encode the output token.
    //
    char *encoded_token = PL_Base64Encode((char *)outToken, outTokenLen, nsnull);

    nsMemory::Free(outToken);

    if (!encoded_token)
        return NS_ERROR_OUT_OF_MEMORY;

    LOG(("  Sending a token of length %d\n", outTokenLen));

    // allocate a buffer sizeof("Negotiate" + " " + b64output_token + "\0")
    *creds = (char *) nsMemory::Alloc(kNegotiateLen + 1 + strlen(encoded_token) + 1);
    if (NS_UNLIKELY(!*creds))
        rv = NS_ERROR_OUT_OF_MEMORY;
    else
        sprintf(*creds, "%s %s", kNegotiate, encoded_token);

    PR_Free(encoded_token);
    return rv;
}

PRBool
nsHttpNegotiateAuth::TestPref(nsIURI *uri, const char *pref)
{
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (!prefs)
        return PR_FALSE;

    nsCAutoString scheme, host;
    PRInt32 port;

    if (NS_FAILED(uri->GetScheme(scheme)))
        return PR_FALSE;
    if (NS_FAILED(uri->GetAsciiHost(host)))
        return PR_FALSE;
    if (NS_FAILED(uri->GetPort(&port)))
        return PR_FALSE;

    char *hostList;
    if (NS_FAILED(prefs->GetCharPref(pref, &hostList)) || !hostList)
        return PR_FALSE;

    // pseudo-BNF
    // ----------
    //
    // url-list       base-url ( base-url "," LWS )*
    // base-url       ( scheme-part | host-part | scheme-part host-part )
    // scheme-part    scheme "://"
    // host-part      host [":" port]
    //
    // for example:
    //   "https://, http://office.foo.com"
    //

    char *start = hostList, *end;
    for (;;) {
        // skip past any whitespace
        while (*start == ' ' || *start == '\t')
            ++start;
        end = strchr(start, ',');
        if (!end)
            end = start + strlen(start);
        if (start == end)
            break;
        if (MatchesBaseURI(scheme, host, port, start, end))
            return PR_TRUE;
        if (*end == '\0')
            break;
        start = end + 1;
    }
    
    nsMemory::Free(hostList);
    return PR_FALSE;
}

PRBool
nsHttpNegotiateAuth::MatchesBaseURI(const nsCSubstring &matchScheme,
                                    const nsCSubstring &matchHost,
                                    PRInt32             matchPort,
                                    const char         *baseStart,
                                    const char         *baseEnd)
{
    // check if scheme://host:port matches baseURI

    // parse the base URI
    const char *hostStart, *schemeEnd = strstr(baseStart, "://");
    if (schemeEnd) {
        // the given scheme must match the parsed scheme exactly
        if (!matchScheme.Equals(Substring(baseStart, schemeEnd)))
            return PR_FALSE;
        hostStart = schemeEnd + 3;
    }
    else
        hostStart = baseStart;

    // XXX this does not work for IPv6-literals
    const char *hostEnd = strchr(hostStart, ':');
    if (hostEnd && hostEnd <= baseEnd) {
        // the given port must match the parsed port exactly
        int port = atoi(hostEnd + 1);
        if (matchPort != (PRInt32) port)
            return PR_FALSE;
    }
    else
        hostEnd = baseEnd;


    // if we didn't parse out a host, then assume we got a match.
    if (hostStart == hostEnd)
        return PR_TRUE;

    PRUint32 hostLen = hostEnd - hostStart;

    // matchHost must either equal host or be a subdomain of host
    if (matchHost.Length() < hostLen)
        return PR_FALSE;

    const char *end = matchHost.EndReading();
    if (PL_strncasecmp(end - hostLen, hostStart, hostLen) == 0) {
        // if matchHost ends with host from the base URI, then make sure it is
        // either an exact match, or prefixed with a dot.  we don't want
        // "foobar.com" to match "bar.com"
        if (matchHost.Length() == hostLen ||
            *(end - hostLen) == '.' ||
            *(end - hostLen - 1) == '.')
            return PR_TRUE;
    }

    return PR_FALSE;
}

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
// GSSAPI Authentication Support Module
//
// Described by IETF Internet draft: draft-brezak-kerberos-http-00.txt
// (formerly draft-brezak-spnego-http-04.txt)
//
// Also described here:
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/dnsecure/html/http-sso-1.asp
//

// this #define must run before prlog.h is included
#ifdef MOZ_LOGGING
#define FORCE_PR_LOG 1
#endif

#include <string.h>
#include <stdlib.h>

#if defined(HAVE_GSSAPI_H)
#include <gssapi.h>
#elif defined(HAVE_GSSAPI_GSSAPI_H)
#include <gssapi/gssapi.h>
#endif

#if defined(HAVE_GSSAPI_GSSAPI_GENERIC_H)
#include <gssapi/gssapi_generic.h> 
#endif

#include "nsIHttpChannel.h"
#include "nsISupportsUtils.h"
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

#include "nsHttpGssapiAuth.h"

//-----------------------------------------------------------------------------

#ifdef PR_LOGGING
//
// in order to do logging, the following environment variables need to be set:
// 
//      set NSPR_LOG_MODULES=negotiateauth:4
//      set NSPR_LOG_FILE=negotiateauth.log
//
static PRLogModuleInfo* gNegotiateLog;

#define LOG(args) PR_LOG(gNegotiateLog, PR_LOG_DEBUG, args)

// Generate proper GSSAPI error messages from the major and
// minor status codes.
void
LogGssError(OM_uint32 maj_stat, OM_uint32 min_stat, const char *prefix)
{
    OM_uint32 new_stat;
    OM_uint32 msg_ctx = 0;
    gss_buffer_desc status1_string;
    gss_buffer_desc status2_string;
    OM_uint32 ret;
    nsCAutoString error(prefix);

    error += ": ";
    do {
        ret = gss_display_status (&new_stat,
                                  maj_stat,
                                  GSS_C_GSS_CODE,
                                  GSS_C_NULL_OID,
                                  &msg_ctx,
                                  &status1_string);
        error += (const char *) status1_string.value;
        error += '\n';
        ret = gss_display_status (&new_stat,
                                  min_stat,
                                  GSS_C_MECH_CODE,
                                  GSS_C_NULL_OID,
                                  &msg_ctx,
                                  &status2_string);
        error += (const char *) status2_string.value;
        error += '\n';
    } while (!GSS_ERROR(ret) && msg_ctx != 0);

    LOG(("%s\n", error.get()));
}

#else /* PR_LOGGING */

#define LOG(args)
#define LogGssError(x,y,z)

#endif /* PR_LOGGING */

//-----------------------------------------------------------------------------

static const char kNegotiate[] = "Negotiate";
static const char kNegotiateAuthTrustedURIs[] = "network.negotiate-auth.trusted-uris";
static const char kNegotiateAuthDelegationURIs[] = "network.negotiate-auth.delegation-uris";

#define kNegotiateLen  (sizeof(kNegotiate)-1)

class nsGssapiContinuationState : public nsISupports
{
public:
    NS_DECL_ISUPPORTS

    nsGssapiContinuationState();
    ~nsGssapiContinuationState() { Reset(); }

    void Reset();

    gss_OID GetOID() { return mech_oid; }

    gss_ctx_id_t mCtx;
private:
    gss_OID mech_oid;
};

nsGssapiContinuationState::nsGssapiContinuationState()
{
    OM_uint32 minstat, majstat;
    gss_OID_set mech_set;
    gss_OID item;
    unsigned int i;
    static gss_OID_desc gss_krb5_mech_oid_desc =
        {9, (void *) "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02"};
    static gss_OID_desc gss_spnego_mech_oid_desc =
        {6, (void *) "\x2b\x06\x01\x05\x05\x02"};

    mCtx = GSS_C_NO_CONTEXT;
    mech_oid = &gss_krb5_mech_oid_desc;

    int mech_found = 0;
    //
    // Now, look at the list of supported mechanisms,
    // if SPNEGO is found, then use it.
    // Otherwise, set the desired mechanism to
    // GSS_C_NO_OID and let the system try to use
    // the default mechanism.
    //
    // Using Kerberos directly (instead of negotiating
    // with SPNEGO) may work in some cases depending
    // on how smart the server side is.
    //
    majstat = gss_indicate_mechs(&minstat, &mech_set);
    if (GSS_ERROR(majstat))
        return;

    for (i=0; i<mech_set->count && !mech_found; i++) {
        item = &mech_set->elements[i];    
        if (item->length == gss_spnego_mech_oid_desc.length &&
            !memcmp(item->elements, gss_spnego_mech_oid_desc.elements,
            item->length)) {
            mech_found = 1;
            mech_oid = &gss_spnego_mech_oid_desc;
            break;
        }
    }
    gss_release_oid_set(&minstat, &mech_set);
}

void
nsGssapiContinuationState::Reset()
{
    if (mCtx != GSS_C_NO_CONTEXT) {
        OM_uint32 minor_status;
        gss_delete_sec_context(&minor_status, &mCtx, GSS_C_NO_BUFFER);
    }
    mCtx = GSS_C_NO_CONTEXT;
}

NS_IMPL_ISUPPORTS0(nsGssapiContinuationState)

//-----------------------------------------------------------------------------

nsHttpGssapiAuth::nsHttpGssapiAuth()
{
#ifdef PR_LOGGING
    if (!gNegotiateLog)
        gNegotiateLog = PR_NewLogModule("negotiateauth");
#endif
}

NS_IMETHODIMP
nsHttpGssapiAuth::GetAuthFlags(PRUint32 *flags)
{
    //
    // GSSAPI creds should not be reused across multiple requests.
    // Only perform the negotiation when it is explicitly requested
    // by the server.  Thus, do *NOT* use the "REUSABLE_CREDENTIALS"
    // flag here.
    //
    // CONNECTION_BASED is specified instead of REQUEST_BASED since
    // we need to complete a sequence of transactions with the server
    // over the same connection.
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
nsHttpGssapiAuth::ChallengeReceived(nsIHttpChannel *httpChannel,
                                    const char *challenge,
                                    PRBool isProxyAuth,
                                    nsISupports **sessionState,
                                    nsISupports **continuationState,
                                    PRBool *identityInvalid)
{
    nsGssapiContinuationState *state = (nsGssapiContinuationState *) *continuationState;

    *identityInvalid = PR_FALSE;

    // proxy auth not supported
    if (isProxyAuth)
        return NS_ERROR_ABORT;

    PRBool allowed = TestPref(httpChannel, kNegotiateAuthTrustedURIs);
    if (!allowed) {
        LOG(("nsHttpNegotiateAuth::ChallengeReceived URI blocked\n"));
        return NS_ERROR_ABORT;
    }

    //
    // Use this opportunity to instantiate the state object
    // that gets used later when we generate the credentials.
    //

    if (!state) {
        state = new nsGssapiContinuationState();
        if (!state)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(*continuationState = state);
    }

    return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsHttpGssapiAuth, nsIHttpAuthenticator)
   
//
// GenerateCredentials
//
// This routine is responsible for creating the correct authentication
// blob to pass to the server that requested "Negotiate" authentication.
//
NS_IMETHODIMP
nsHttpGssapiAuth::GenerateCredentials(nsIHttpChannel *httpChannel,
                                     const char *challenge,
                                     PRBool isProxyAuth,
                                     const PRUnichar *domain,
                                     const PRUnichar *username,
                                     const PRUnichar *password,
                                     nsISupports **sessionState,
                                     nsISupports **continuationState,
                                     char **creds)
{
    OM_uint32 major_status, minor_status;
    OM_uint32 req_flags = GSS_C_MUTUAL_FLAG;
    gss_buffer_desc input_token = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc output_token = GSS_C_EMPTY_BUFFER;
    gss_buffer_t  in_token_ptr = GSS_C_NO_BUFFER;
    gss_name_t server;
    nsGssapiContinuationState *state = (nsGssapiContinuationState *) *continuationState;

    nsCOMPtr<nsIURI> uri;
    nsresult rv;
    nsCString service;

    LOG(("nsHttpGssapiAuth::GenerateCredentials() [challenge=%s]\n", challenge));

    NS_ASSERTION(creds, "null param");

    PRBool isGssapiAuth =
        !PL_strncasecmp(challenge, kNegotiate, kNegotiateLen);
    NS_ENSURE_TRUE(isGssapiAuth, NS_ERROR_UNEXPECTED);

    // proxy auth not supported
    if (isProxyAuth)
        return NS_ERROR_ABORT;

    PRBool delegation = TestPref(httpChannel, kNegotiateAuthDelegationURIs);
    if (delegation) {
        LOG(("  using GSS_C_DELEG_FLAG\n"));
        req_flags |= GSS_C_DELEG_FLAG;
    }

    rv = httpChannel->GetURI(getter_AddRefs(uri));
    if (NS_FAILED(rv)) return rv;

    rv = uri->GetAsciiHost(service);
    if (NS_FAILED(rv)) return rv;

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

    input_token.value = (void *)service.get();
    input_token.length = service.Length() + 1;

    major_status = gss_import_name(&minor_status,
                                   &input_token,
#ifdef HAVE_GSS_C_NT_HOSTBASED_SERVICE
                                   GSS_C_NT_HOSTBASED_SERVICE,
#else
                                   gss_nt_service_name,
#endif
                                   &server);
    input_token.value = NULL;
    input_token.length = 0;
    if (GSS_ERROR(major_status)) {
        LogGssError(major_status, minor_status, "gss_import_name() failed");
        return NS_ERROR_FAILURE;
    }

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

    if (len > kNegotiateLen) {
        challenge += kNegotiateLen;
        while (*challenge == ' ') challenge++;
        len = strlen(challenge);

        input_token.length = (len * 3)/4;
        input_token.value = malloc(input_token.length);
        if (!input_token.value)
            return (NS_ERROR_OUT_OF_MEMORY);

        //
        // Decode the response that followed the "Negotiate" token
        //
        if (PL_Base64Decode(challenge, len, (char *) input_token.value) == NULL) {
            free(input_token.value);
            return(NS_ERROR_UNEXPECTED);
        }
        in_token_ptr = &input_token;
    }
    else {
        //
        // Starting over, clear out any existing context and don't
        // use an input token.
        //
        state->Reset();
        in_token_ptr = GSS_C_NO_BUFFER;
    }

    major_status = gss_init_sec_context(&minor_status,
                                        GSS_C_NO_CREDENTIAL,
                                        &state->mCtx,
                                        server,
                                        state->GetOID(),
                                        req_flags,
                                        GSS_C_INDEFINITE,
                                        GSS_C_NO_CHANNEL_BINDINGS,
                                        in_token_ptr,
                                        nsnull,
                                        &output_token,
                                        nsnull,
                                        nsnull);

    if (GSS_ERROR(major_status)) {
        LogGssError(major_status, minor_status, "gss_init_sec_context() failed");
        gss_release_name(&minor_status, &server);
        state->Reset();
        if (input_token.length > 0 && input_token.value != NULL)
            gss_release_buffer(&minor_status, &input_token);
        return NS_ERROR_FAILURE;
    }
    if (major_status == GSS_S_COMPLETE) {
        //
        // We are done with this authentication, reset the context. 
        //
        state->Reset();
    }
    else if (major_status == GSS_S_CONTINUE_NEEDED) {
        //
        // The important thing is that we do NOT reset the
        // context here because it will be needed on the
        // next call.
        //
    } 

    // We don't need the input token data anymore.
    if (input_token.length > 0 && input_token.value != NULL)
        gss_release_buffer(&minor_status, &input_token);

    if (output_token.length == 0) {
        LOG(("  No GSS output token to send, exiting"));
        gss_release_name(&minor_status, &server);
        return NS_ERROR_FAILURE;
    }

    //
    // The token output from the gss_init_sec_context call is
    // encoded and used as the Authentication response for the
    // server.
    //
    char *encoded_token = PL_Base64Encode((char *)output_token.value,
                                          output_token.length,
                                          nsnull);
    if (!encoded_token) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        goto end;
    }

    LOG(("  Sending a token of length %d\n", output_token.length));

    // allocate a buffer sizeof("Negotiate" + " " + b64output_token + "\0")
    *creds = (char *) nsMemory::Alloc (kNegotiateLen + 1 + strlen(encoded_token) + 1);
    if (!(*creds)) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        goto end;
    }

    sprintf(*creds, "%s %s", kNegotiate, encoded_token);
    rv = NS_OK;

end:
    if (encoded_token)
        PR_Free(encoded_token);

    gss_release_buffer(&minor_status, &output_token);
    gss_release_name(&minor_status, &server);

    LOG(("  returning the call"));
    return rv;
}

PRBool
nsHttpGssapiAuth::TestPref(nsIHttpChannel *httpChannel, const char *pref)
{
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (!prefs)
        return PR_FALSE;

    nsCOMPtr<nsIURI> uri;
    httpChannel->GetURI(getter_AddRefs(uri));
    if (!uri)
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
nsHttpGssapiAuth::MatchesBaseURI(const nsCSubstring &matchScheme,
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
    if (hostEnd) {
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

    // now, allow host to be a subdomain of matchHost
    if (matchHost.Length() > hostLen)
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

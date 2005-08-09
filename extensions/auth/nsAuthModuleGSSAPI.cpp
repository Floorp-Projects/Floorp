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

#include "nsNegotiateAuth.h"
#include "nsNegotiateAuthGSSAPI.h"

#ifdef XP_MACOSX
#include <Kerberos/Kerberos.h>
#endif

//-----------------------------------------------------------------------------

#if defined( PR_LOGGING )

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

#define LogGssError(x,y,z)

#endif /* PR_LOGGING */

//-----------------------------------------------------------------------------

nsNegotiateAuth::nsNegotiateAuth()
    : mServiceFlags(REQ_DEFAULT)
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
    mMechOID = &gss_krb5_mech_oid_desc;

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

    for (i=0; i<mech_set->count; i++) {
        item = &mech_set->elements[i];    
        if (item->length == gss_spnego_mech_oid_desc.length &&
            !memcmp(item->elements, gss_spnego_mech_oid_desc.elements,
            item->length)) {
            // ok, we found it
            mMechOID = &gss_spnego_mech_oid_desc;
            break;
        }
    }
    gss_release_oid_set(&minstat, &mech_set);
}

void
nsNegotiateAuth::Reset()
{
    if (mCtx != GSS_C_NO_CONTEXT) {
        OM_uint32 minor_status;
        gss_delete_sec_context(&minor_status, &mCtx, GSS_C_NO_BUFFER);
    }
    mCtx = GSS_C_NO_CONTEXT;
}

NS_IMPL_ISUPPORTS1(nsNegotiateAuth, nsIAuthModule)

NS_IMETHODIMP
nsNegotiateAuth::Init(const char *serviceName,
                   PRUint32    serviceFlags,
                   const PRUnichar *domain,
                   const PRUnichar *username,
                   const PRUnichar *password)
{
    // we don't expect to be passed any user credentials
    NS_ASSERTION(!domain && !username && !password, "unexpected credentials");

    // it's critial that the caller supply a service name to be used
    NS_ENSURE_TRUE(serviceName && *serviceName, NS_ERROR_INVALID_ARG);

    mServiceName = serviceName;
    mServiceFlags = serviceFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsNegotiateAuth::GetNextToken(const void *inToken,
                           PRUint32    inTokenLen,
                           void      **outToken,
                           PRUint32   *outTokenLen)
{
    OM_uint32 major_status, minor_status;
    OM_uint32 req_flags = 0;
    gss_buffer_desc input_token = GSS_C_EMPTY_BUFFER;
    gss_buffer_desc output_token = GSS_C_EMPTY_BUFFER;
    gss_buffer_t  in_token_ptr = GSS_C_NO_BUFFER;
    gss_name_t server;

    LOG(("entering nsNegotiateAuth::GetNextToken()\n"));

    if (mServiceFlags & REQ_DELEGATE)
        req_flags |= GSS_C_DELEG_FLAG;

    input_token.value = (void *)mServiceName.get();
    input_token.length = mServiceName.Length() + 1;

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

    if (inToken) {
        input_token.length = inTokenLen;
        input_token.value = (void *) inToken;
        in_token_ptr = &input_token;
    }
    else if (mCtx != GSS_C_NO_CONTEXT) {
        // If there is no input token, then we are starting a new
        // authentication sequence.  If we have already initialized our
        // security context, then we're in trouble because it means that the
        // first sequence failed.  We need to bail or else we might end up in
        // an infinite loop.
        LOG(("Cannot restart authentication sequence!"));
        return NS_ERROR_UNEXPECTED; 
    }

#if defined(XP_MACOSX)
    // Suppress Kerberos prompts to get credentials.  See bug 240643.
    KLBoolean found;
    if (KLCacheHasValidTickets(NULL, kerberosVersion_V5, &found, NULL, NULL) != klNoErr || !found)
    {
        major_status = GSS_S_FAILURE;
        minor_status = 0;
    }
    else
#endif /* XP_MACOSX */
    major_status = gss_init_sec_context(&minor_status,
                                        GSS_C_NO_CREDENTIAL,
                                        &mCtx,
                                        server,
                                        mMechOID,
                                        req_flags,
                                        GSS_C_INDEFINITE,
                                        GSS_C_NO_CHANNEL_BINDINGS,
                                        in_token_ptr,
                                        nsnull,
                                        &output_token,
                                        nsnull,
                                        nsnull);

    nsresult rv;
    if (GSS_ERROR(major_status)) {
        LogGssError(major_status, minor_status, "gss_init_sec_context() failed");
        Reset();
        rv = NS_ERROR_FAILURE;
        goto end;
    }
    if (major_status == GSS_S_COMPLETE) {
        //
        // We are done with this authentication, reset the context. 
        //
        Reset();
    }
    else if (major_status == GSS_S_CONTINUE_NEEDED) {
        //
        // The important thing is that we do NOT reset the
        // context here because it will be needed on the
        // next call.
        //
    } 

    if (output_token.length == 0) {
        LOG(("  No GSS output token to send, exiting"));
        rv = NS_ERROR_FAILURE;
        goto end;
    }

    *outTokenLen = output_token.length;
    *outToken = nsMemory::Clone(output_token.value, output_token.length);

    gss_release_buffer(&minor_status, &output_token);
    rv = NS_OK;

end:
    gss_release_name(&minor_status, &server);

    LOG(("  leaving nsNegotiateAuth::GetNextToken [rv=%x]", rv));
    return rv;
}

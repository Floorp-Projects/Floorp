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
 * The Original Code is the SSPI NegotiateAuth Module
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

//
// Negotiate Authentication Support Module
//
// Described by IETF Internet draft: draft-brezak-kerberos-http-00.txt
// (formerly draft-brezak-spnego-http-04.txt)
//
// Also described here:
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/dnsecure/html/http-sso-1.asp
//

#include "nsNegotiateAuth.h"
#include "nsNegotiateAuthSSPI.h"
#include "nsIServiceManager.h"
#include "nsIDNSService.h"
#include "nsIDNSRecord.h"
#include "nsNetCID.h"
#include "nsCOMPtr.h"

//-----------------------------------------------------------------------------

#ifdef DEBUG
#define CASE_(_x) case _x: return # _x;
static const char *MapErrorCode(int rc)
{
    switch (rc) {
    CASE_(SEC_E_OK)
    CASE_(SEC_I_CONTINUE_NEEDED)
    CASE_(SEC_I_COMPLETE_NEEDED)
    CASE_(SEC_I_COMPLETE_AND_CONTINUE)
    CASE_(SEC_E_INCOMPLETE_MESSAGE)
    CASE_(SEC_I_INCOMPLETE_CREDENTIALS)
    CASE_(SEC_E_INVALID_HANDLE)
    CASE_(SEC_E_TARGET_UNKNOWN)
    CASE_(SEC_E_LOGON_DENIED)
    CASE_(SEC_E_INTERNAL_ERROR)
    CASE_(SEC_E_NO_CREDENTIALS)
    CASE_(SEC_E_NO_AUTHENTICATING_AUTHORITY)
    CASE_(SEC_E_INSUFFICIENT_MEMORY)
    CASE_(SEC_E_INVALID_TOKEN)
    }
    return "<unknown>";
}
#else
#define MapErrorCode(_rc) ""
#endif

//-----------------------------------------------------------------------------

static HINSTANCE                 sspi_lib; 
static ULONG                     sspi_maxTokenLen;
static PSecurityFunctionTable    sspi;

static nsresult
InitSSPI()
{
    PSecurityFunctionTable (*initFun)(void);

    sspi_lib = LoadLibrary("secur32.dll");
    if (!sspi_lib) {
        sspi_lib = LoadLibrary("security.dll");
        if (!sspi_lib) {
            LOG(("SSPI library not found"));
            return NS_ERROR_UNEXPECTED;
        }
    }

    initFun = (PSecurityFunctionTable (*)(void))
            GetProcAddress(sspi_lib, "InitSecurityInterfaceA");
    if (!initFun) {
        LOG(("InitSecurityInterfaceA not found"));
        return NS_ERROR_UNEXPECTED;
    }

    sspi = initFun();
    if (!sspi) {
        LOG(("InitSecurityInterfaceA failed"));
        return NS_ERROR_UNEXPECTED;
    }

    PSecPkgInfo pinfo;
    int rc = (sspi->QuerySecurityPackageInfo)("Negotiate", &pinfo);
    if (rc != SEC_E_OK) {
        LOG(("Negotiate package not found"));
        return NS_ERROR_UNEXPECTED;
    }
    sspi_maxTokenLen = pinfo->cbMaxToken;

    return NS_OK;
}

//-----------------------------------------------------------------------------

static nsresult
MakeSN(const char *principal, nsCString &result)
{
    nsresult rv;

    nsCAutoString buf(principal);

    // The service name looks like "protocol@hostname", we need to map
    // this to a value that SSPI expects.  To be consistent with IE, we
    // need to map '@' to '/' and canonicalize the hostname.
    PRInt32 index = buf.FindChar('@');
    if (index == kNotFound)
        return NS_ERROR_UNEXPECTED;
    
    nsCOMPtr<nsIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv))
        return rv;

    // This should always hit our DNS cache
    nsCOMPtr<nsIDNSRecord> record;
    rv = dns->Resolve(Substring(buf, index + 1), PR_FALSE,
                      getter_AddRefs(record));
    if (NS_FAILED(rv))
        return rv;

    nsCAutoString cname;
    rv = record->GetCanonicalName(cname);
    if (NS_SUCCEEDED(rv)) {
        result = StringHead(buf, index) + NS_LITERAL_CSTRING("/") + cname;
        LOG(("Using SPN of [%s]\n", result.get()));
    }
    return rv;
}

//-----------------------------------------------------------------------------

nsNegotiateAuth::nsNegotiateAuth()
    : mServiceFlags(REQ_DEFAULT)
{
    memset(&mCred, 0, sizeof(mCred));
    memset(&mCtxt, 0, sizeof(mCtxt));
}

nsNegotiateAuth::~nsNegotiateAuth()
{
    Reset();

    if (mCred.dwLower || mCred.dwUpper) {
#ifdef __MINGW32__
        (sspi->FreeCredentialsHandle)(&mCred);
#else
        (sspi->FreeCredentialHandle)(&mCred);
#endif
        memset(&mCred, 0, sizeof(mCred));
    }
}

void
nsNegotiateAuth::Reset()
{
    if (mCtxt.dwLower || mCtxt.dwUpper) {
        (sspi->DeleteSecurityContext)(&mCtxt);
        memset(&mCtxt, 0, sizeof(mCtxt));
    }
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

    nsresult rv;

    // XXX lazy initialization like this assumes that we are single threaded
    if (!sspi) {
        rv = InitSSPI();
        if (NS_FAILED(rv))
            return rv;
    }

    rv = MakeSN(serviceName, mServiceName);
    if (NS_FAILED(rv))
        return rv;
    mServiceFlags = serviceFlags;

    TimeStamp useBefore;
    SECURITY_STATUS rc;

    rc = (sspi->AcquireCredentialsHandle)(NULL,
                                          "Negotiate",
                                          SECPKG_CRED_OUTBOUND,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL,
                                          &mCred,
                                          &useBefore);
    if (rc != SEC_E_OK)
        return NS_ERROR_UNEXPECTED;

    return NS_OK;
}

NS_IMETHODIMP
nsNegotiateAuth::GetNextToken(const void *inToken,
                              PRUint32    inTokenLen,
                              void      **outToken,
                              PRUint32   *outTokenLen)
{
    SECURITY_STATUS rc;

    DWORD ctxAttr, ctxReq = 0;
    CtxtHandle *ctxIn;
    SecBufferDesc ibd, obd;
    SecBuffer ib, ob;

    LOG(("entering nsNegotiateAuth::GetNextToken()\n"));

    if (mServiceFlags & REQ_DELEGATE)
        ctxReq |= ISC_REQ_DELEGATE;
    if (mServiceFlags & REQ_MUTUAL_AUTH)
        ctxReq |= ISC_REQ_MUTUAL_AUTH;

    if (inToken) {
        ib.BufferType = SECBUFFER_TOKEN;
        ib.cbBuffer = inTokenLen;
        ib.pvBuffer = (void *) inToken;
        ibd.ulVersion = SECBUFFER_VERSION;
        ibd.cBuffers = 1;
        ibd.pBuffers = &ib;
        ctxIn = &mCtxt;
    }
    else {
        // If there is no input token, then we are starting a new
        // authentication sequence.  If we have already initialized our
        // security context, then we're in trouble because it means that the
        // first sequence failed.  We need to bail or else we might end up in
        // an infinite loop.
        if (mCtxt.dwLower || mCtxt.dwUpper) {
            LOG(("Cannot restart authentication sequence!"));
            return NS_ERROR_UNEXPECTED;
        }

        ctxIn = NULL;
    }

    obd.ulVersion = SECBUFFER_VERSION;
    obd.cBuffers = 1;
    obd.pBuffers = &ob;
    ob.BufferType = SECBUFFER_TOKEN;
    ob.cbBuffer = sspi_maxTokenLen;
    ob.pvBuffer = nsMemory::Alloc(ob.cbBuffer);
    if (!ob.pvBuffer)
        return NS_ERROR_OUT_OF_MEMORY;
    memset(ob.pvBuffer, 0, ob.cbBuffer);

    rc = (sspi->InitializeSecurityContext)(&mCred,
                                           ctxIn,
                                           (SEC_CHAR *) mServiceName.get(),
                                           ctxReq,
                                           0,
                                           SECURITY_NATIVE_DREP,
                                           inToken ? &ibd : NULL,
                                           0,
                                           &mCtxt,
                                           &obd,
                                           &ctxAttr,
                                           NULL);
    if (rc == SEC_I_CONTINUE_NEEDED || rc == SEC_E_OK) {
        *outToken = ob.pvBuffer;
        *outTokenLen = ob.cbBuffer;
        return NS_OK;
    }

    LOG(("InitializeSecurityContext failed [rc=%d:%s]\n", rc, MapErrorCode(rc)));
    Reset();
    nsMemory::Free(ob.pvBuffer);
    return NS_ERROR_FAILURE;
}

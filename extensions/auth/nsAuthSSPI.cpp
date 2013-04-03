/* vim:set ts=4 sw=4 sts=4 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// Negotiate Authentication Support Module
//
// Described by IETF Internet draft: draft-brezak-kerberos-http-00.txt
// (formerly draft-brezak-spnego-http-04.txt)
//
// Also described here:
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/dnsecure/html/http-sso-1.asp
//

#include "nsAuthSSPI.h"
#include "nsIServiceManager.h"
#include "nsIDNSService.h"
#include "nsIDNSRecord.h"
#include "nsNetCID.h"
#include "nsCOMPtr.h"
#include "nsICryptoHash.h"

#include <windows.h>

#define SEC_SUCCESS(Status) ((Status) >= 0)

#ifndef KERB_WRAP_NO_ENCRYPT
#define KERB_WRAP_NO_ENCRYPT 0x80000001
#endif

#ifndef SECBUFFER_PADDING
#define SECBUFFER_PADDING 9
#endif

#ifndef SECBUFFER_STREAM
#define SECBUFFER_STREAM 10
#endif

//-----------------------------------------------------------------------------

static const PRUnichar *const pTypeName [] = {
    L"Kerberos",
    L"Negotiate",
    L"NTLM"
};

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

static PSecurityFunctionTableW   sspi;

static nsresult
InitSSPI()
{
    LOG(("  InitSSPI\n"));

    sspi = InitSecurityInterfaceW();
    if (!sspi) {
        LOG(("InitSecurityInterfaceW failed"));
        return NS_ERROR_UNEXPECTED;
    }

    return NS_OK;
}

//-----------------------------------------------------------------------------

static nsresult
MakeSN(const char *principal, nsCString &result)
{
    nsresult rv;

    nsAutoCString buf(principal);

    // The service name looks like "protocol@hostname", we need to map
    // this to a value that SSPI expects.  To be consistent with IE, we
    // need to map '@' to '/' and canonicalize the hostname.
    int32_t index = buf.FindChar('@');
    if (index == kNotFound)
        return NS_ERROR_UNEXPECTED;
    
    nsCOMPtr<nsIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv))
        return rv;

    // This could be expensive if our DNS cache cannot satisfy the request.
    // However, we should have at least hit the OS resolver once prior to
    // reaching this code, so provided the OS resolver has this information
    // cached, we should not have to worry about blocking on this function call
    // for very long.  NOTE: because we ask for the canonical hostname, we
    // might end up requiring extra network activity in cases where the OS
    // resolver might not have enough information to satisfy the request from
    // its cache.  This is not an issue in versions of Windows up to WinXP.
    nsCOMPtr<nsIDNSRecord> record;
    rv = dns->Resolve(Substring(buf, index + 1),
                      nsIDNSService::RESOLVE_CANONICAL_NAME,
                      getter_AddRefs(record));
    if (NS_FAILED(rv))
        return rv;

    nsAutoCString cname;
    rv = record->GetCanonicalName(cname);
    if (NS_SUCCEEDED(rv)) {
        result = StringHead(buf, index) + NS_LITERAL_CSTRING("/") + cname;
        LOG(("Using SPN of [%s]\n", result.get()));
    }
    return rv;
}

//-----------------------------------------------------------------------------

nsAuthSSPI::nsAuthSSPI(pType package)
    : mServiceFlags(REQ_DEFAULT)
    , mMaxTokenLen(0)
    , mPackage(package)
    , mCertDERData(nullptr)
    , mCertDERLength(0)
{
    memset(&mCred, 0, sizeof(mCred));
    memset(&mCtxt, 0, sizeof(mCtxt));
}

nsAuthSSPI::~nsAuthSSPI()
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
nsAuthSSPI::Reset()
{
    mIsFirst = true;

    if (mCertDERData){
        nsMemory::Free(mCertDERData);
        mCertDERData = nullptr;
        mCertDERLength = 0;   
    }

    if (mCtxt.dwLower || mCtxt.dwUpper) {
        (sspi->DeleteSecurityContext)(&mCtxt);
        memset(&mCtxt, 0, sizeof(mCtxt));
    }
}

NS_IMPL_ISUPPORTS1(nsAuthSSPI, nsIAuthModule)

NS_IMETHODIMP
nsAuthSSPI::Init(const char *serviceName,
                 uint32_t    serviceFlags,
                 const PRUnichar *domain,
                 const PRUnichar *username,
                 const PRUnichar *password)
{
    LOG(("  nsAuthSSPI::Init\n"));

    mIsFirst = true;
    mCertDERLength = 0;
    mCertDERData = nullptr;

    // The caller must supply a service name to be used. (For why we now require
    // a service name for NTLM, see bug 487872.)
    NS_ENSURE_TRUE(serviceName && *serviceName, NS_ERROR_INVALID_ARG);

    nsresult rv;

    // XXX lazy initialization like this assumes that we are single threaded
    if (!sspi) {
        rv = InitSSPI();
        if (NS_FAILED(rv))
            return rv;
    }
    SEC_WCHAR *package;

    package = (SEC_WCHAR *) pTypeName[(int)mPackage];

    if (mPackage == PACKAGE_TYPE_NTLM) {
        // (bug 535193) For NTLM, just use the uri host, do not do canonical host lookups.
        // The incoming serviceName is in the format: "protocol@hostname", SSPI expects
        // "<service class>/<hostname>", so swap the '@' for a '/'.
        mServiceName.Assign(serviceName);
        int32_t index = mServiceName.FindChar('@');
        if (index == kNotFound)
            return NS_ERROR_UNEXPECTED;
        mServiceName.Replace(index, 1, '/');
    }
    else {
        // Kerberos requires the canonical host, MakeSN takes care of this through a
        // DNS lookup.
        rv = MakeSN(serviceName, mServiceName);
        if (NS_FAILED(rv))
            return rv;
    }

    mServiceFlags = serviceFlags;

    SECURITY_STATUS rc;

    PSecPkgInfoW pinfo;
    rc = (sspi->QuerySecurityPackageInfoW)(package, &pinfo);
    if (rc != SEC_E_OK) {
        LOG(("%s package not found\n", package));
        return NS_ERROR_UNEXPECTED;
    }
    mMaxTokenLen = pinfo->cbMaxToken;
    (sspi->FreeContextBuffer)(pinfo);

    TimeStamp useBefore;

    SEC_WINNT_AUTH_IDENTITY_W ai;
    SEC_WINNT_AUTH_IDENTITY_W *pai = nullptr;
    
    // domain, username, and password will be null if nsHttpNTLMAuth's ChallengeReceived
    // returns false for identityInvalid. Use default credentials in this case by passing
    // null for pai.
    if (username && password) {
        // Keep a copy of these strings for the duration
        mUsername.Assign(username);
        mPassword.Assign(password);
        mDomain.Assign(domain);
        ai.Domain = reinterpret_cast<unsigned short*>(mDomain.BeginWriting());
        ai.DomainLength = mDomain.Length();
        ai.User = reinterpret_cast<unsigned short*>(mUsername.BeginWriting());
        ai.UserLength = mUsername.Length();
        ai.Password = reinterpret_cast<unsigned short*>(mPassword.BeginWriting());
        ai.PasswordLength = mPassword.Length();
        ai.Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;
        pai = &ai;
    }

    rc = (sspi->AcquireCredentialsHandleW)(nullptr,
                                           package,
                                           SECPKG_CRED_OUTBOUND,
                                           nullptr,
                                           pai,
                                           nullptr,
                                           nullptr,
                                           &mCred,
                                           &useBefore);
    if (rc != SEC_E_OK)
        return NS_ERROR_UNEXPECTED;
    LOG(("AcquireCredentialsHandle() succeeded.\n"));
    return NS_OK;
}

// The arguments inToken and inTokenLen are used to pass in the server
// certificate (when available) in the first call of the function. The
// second time these arguments hold an input token. 
NS_IMETHODIMP
nsAuthSSPI::GetNextToken(const void *inToken,
                         uint32_t    inTokenLen,
                         void      **outToken,
                         uint32_t   *outTokenLen)
{
    // String for end-point bindings.
    const char end_point[] = "tls-server-end-point:"; 
    const int end_point_length = sizeof(end_point) - 1;
    const int hash_size = 32;  // Size of a SHA256 hash.
    const int cbt_size = hash_size + end_point_length;
	
    SECURITY_STATUS rc;
    TimeStamp ignored;

    DWORD ctxAttr, ctxReq = 0;
    CtxtHandle *ctxIn;
    SecBufferDesc ibd, obd;
    // Optional second input buffer for the CBT (Channel Binding Token)
    SecBuffer ib[2], ob;
    // Pointer to the block of memory that stores the CBT
    char* sspi_cbt = nullptr;
    SEC_CHANNEL_BINDINGS pendpoint_binding;

    LOG(("entering nsAuthSSPI::GetNextToken()\n"));

    if (!mCred.dwLower && !mCred.dwUpper) {
        LOG(("nsAuthSSPI::GetNextToken(), not initialized. exiting."));
        return NS_ERROR_NOT_INITIALIZED;
    }

    if (mServiceFlags & REQ_DELEGATE)
        ctxReq |= ISC_REQ_DELEGATE;
    if (mServiceFlags & REQ_MUTUAL_AUTH)
        ctxReq |= ISC_REQ_MUTUAL_AUTH;

    if (inToken) {
        if (mIsFirst) {
            // First time if it comes with a token,
            // the token represents the server certificate.
            mIsFirst = false;
            mCertDERLength = inTokenLen;
            mCertDERData = nsMemory::Alloc(inTokenLen);
            if (!mCertDERData)
                return NS_ERROR_OUT_OF_MEMORY;
            memcpy(mCertDERData, inToken, inTokenLen);

            // We are starting a new authentication sequence.  
            // If we have already initialized our
            // security context, then we're in trouble because it means that the
            // first sequence failed.  We need to bail or else we might end up in
            // an infinite loop.
            if (mCtxt.dwLower || mCtxt.dwUpper) {
                LOG(("Cannot restart authentication sequence!"));
                return NS_ERROR_UNEXPECTED;
            }
            ctxIn = nullptr;
            // The certificate needs to be erased before being passed 
            // to InitializeSecurityContextW().
            inToken = nullptr;
            inTokenLen = 0;
        } else {
            ibd.ulVersion = SECBUFFER_VERSION;
            ibd.cBuffers = 0;
            ibd.pBuffers = ib;
            
            // If we have stored a certificate, the Channel Binding Token
            // needs to be generated and sent in the first input buffer.
            if (mCertDERLength > 0) {
                // First we create a proper Endpoint Binding structure. 
                pendpoint_binding.dwInitiatorAddrType = 0;
                pendpoint_binding.cbInitiatorLength = 0;
                pendpoint_binding.dwInitiatorOffset = 0;
                pendpoint_binding.dwAcceptorAddrType = 0;
                pendpoint_binding.cbAcceptorLength = 0;
                pendpoint_binding.dwAcceptorOffset = 0;
                pendpoint_binding.cbApplicationDataLength = cbt_size;
                pendpoint_binding.dwApplicationDataOffset = 
                                            sizeof(SEC_CHANNEL_BINDINGS);

                // Then add it to the array of sec buffers accordingly.
                ib[ibd.cBuffers].BufferType = SECBUFFER_CHANNEL_BINDINGS;
                ib[ibd.cBuffers].cbBuffer =
                        pendpoint_binding.cbApplicationDataLength
                        + pendpoint_binding.dwApplicationDataOffset;
          
                sspi_cbt = (char *) nsMemory::Alloc(ib[ibd.cBuffers].cbBuffer);
                if (!sspi_cbt){
                    return NS_ERROR_OUT_OF_MEMORY;
                }

                // Helper to write in the memory block that stores the CBT
                char* sspi_cbt_ptr = sspi_cbt;
          
                ib[ibd.cBuffers].pvBuffer = sspi_cbt;
                ibd.cBuffers++;

                memcpy(sspi_cbt_ptr, &pendpoint_binding,
                       pendpoint_binding.dwApplicationDataOffset);
                sspi_cbt_ptr += pendpoint_binding.dwApplicationDataOffset;

                memcpy(sspi_cbt_ptr, end_point, end_point_length);
                sspi_cbt_ptr += end_point_length;
          
                // Start hashing. We are always doing SHA256, but depending
                // on the certificate, a different alogirthm might be needed.
                nsAutoCString hashString;

                nsresult rv;
                nsCOMPtr<nsICryptoHash> crypto;
                crypto = do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &rv);
                if (NS_SUCCEEDED(rv))
                    rv = crypto->Init(nsICryptoHash::SHA256);
                if (NS_SUCCEEDED(rv))
                    rv = crypto->Update((unsigned char*)mCertDERData, mCertDERLength);
                if (NS_SUCCEEDED(rv))
                    rv = crypto->Finish(false, hashString);
                if (NS_FAILED(rv)) {
                    nsMemory::Free(mCertDERData);
                    mCertDERData = nullptr;
                    mCertDERLength = 0;
                    nsMemory::Free(sspi_cbt);
                    return rv;
                }
          
                // Once the hash has been computed, we store it in memory right
                // after the Endpoint structure and the "tls-server-end-point:"
                // char array.
                memcpy(sspi_cbt_ptr, hashString.get(), hash_size);
          
                // Free memory used to store the server certificate
                nsMemory::Free(mCertDERData);
                mCertDERData = nullptr;
                mCertDERLength = 0;
            } // End of CBT computation.

            // We always need this SECBUFFER.
            ib[ibd.cBuffers].BufferType = SECBUFFER_TOKEN;
            ib[ibd.cBuffers].cbBuffer = inTokenLen;
            ib[ibd.cBuffers].pvBuffer = (void *) inToken;
            ibd.cBuffers++;
            ctxIn = &mCtxt;
        }
    } else { // First time and without a token (no server certificate)
        // We are starting a new authentication sequence.  If we have already 
        // initialized our security context, then we're in trouble because it 
        // means that the first sequence failed.  We need to bail or else we 
        // might end up in an infinite loop.
        if (mCtxt.dwLower || mCtxt.dwUpper || mCertDERData || mCertDERLength) {
            LOG(("Cannot restart authentication sequence!"));
            return NS_ERROR_UNEXPECTED;
        }
        ctxIn = nullptr;
        mIsFirst = false;
    }

    obd.ulVersion = SECBUFFER_VERSION;
    obd.cBuffers = 1;
    obd.pBuffers = &ob;
    ob.BufferType = SECBUFFER_TOKEN;
    ob.cbBuffer = mMaxTokenLen;
    ob.pvBuffer = nsMemory::Alloc(ob.cbBuffer);
    if (!ob.pvBuffer){
        if (sspi_cbt)
            nsMemory::Free(sspi_cbt);
        return NS_ERROR_OUT_OF_MEMORY;
    }
    memset(ob.pvBuffer, 0, ob.cbBuffer);

    NS_ConvertUTF8toUTF16 wSN(mServiceName);
    SEC_WCHAR *sn = (SEC_WCHAR *) wSN.get();

    rc = (sspi->InitializeSecurityContextW)(&mCred,
                                            ctxIn,
                                            sn,
                                            ctxReq,
                                            0,
                                            SECURITY_NATIVE_DREP,
                                            inToken ? &ibd : nullptr,
                                            0,
                                            &mCtxt,
                                            &obd,
                                            &ctxAttr,
                                            &ignored);
    if (rc == SEC_I_CONTINUE_NEEDED || rc == SEC_E_OK) {

#ifdef PR_LOGGING
        if (rc == SEC_E_OK)
            LOG(("InitializeSecurityContext: succeeded.\n"));
        else
            LOG(("InitializeSecurityContext: continue.\n"));
#endif
        if (sspi_cbt)
            nsMemory::Free(sspi_cbt);
            
        if (!ob.cbBuffer) {
            nsMemory::Free(ob.pvBuffer);
            ob.pvBuffer = nullptr;
        }
        *outToken = ob.pvBuffer;
        *outTokenLen = ob.cbBuffer;

        if (rc == SEC_E_OK)
            return NS_SUCCESS_AUTH_FINISHED;

        return NS_OK;
    }

    LOG(("InitializeSecurityContext failed [rc=%d:%s]\n", rc, MapErrorCode(rc)));
    Reset();
    nsMemory::Free(ob.pvBuffer);
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsAuthSSPI::Unwrap(const void *inToken,
                   uint32_t    inTokenLen,
                   void      **outToken,
                   uint32_t   *outTokenLen)
{
    SECURITY_STATUS rc;
    SecBufferDesc ibd;
    SecBuffer ib[2];

    ibd.cBuffers = 2;
    ibd.pBuffers = ib;
    ibd.ulVersion = SECBUFFER_VERSION; 

    // SSPI Buf
    ib[0].BufferType = SECBUFFER_STREAM;
    ib[0].cbBuffer = inTokenLen;
    ib[0].pvBuffer = nsMemory::Alloc(ib[0].cbBuffer);
    if (!ib[0].pvBuffer)
        return NS_ERROR_OUT_OF_MEMORY;
    
    memcpy(ib[0].pvBuffer, inToken, inTokenLen);

    // app data
    ib[1].BufferType = SECBUFFER_DATA;
    ib[1].cbBuffer = 0;
    ib[1].pvBuffer = nullptr;

    rc = (sspi->DecryptMessage)(
                                &mCtxt,
                                &ibd,
                                0, // no sequence numbers
                                nullptr
                                );

    if (SEC_SUCCESS(rc)) {
        // check if ib[1].pvBuffer is really just ib[0].pvBuffer, in which
        // case we can let the caller free it. Otherwise, we need to
        // clone it, and free the original
        if (ib[0].pvBuffer == ib[1].pvBuffer) {
            *outToken = ib[1].pvBuffer;
        }
        else {
            *outToken = nsMemory::Clone(ib[1].pvBuffer, ib[1].cbBuffer);
            nsMemory::Free(ib[0].pvBuffer);
            if (!*outToken)
                return NS_ERROR_OUT_OF_MEMORY;
        }
        *outTokenLen = ib[1].cbBuffer;
    }
    else
        nsMemory::Free(ib[0].pvBuffer);

    if (!SEC_SUCCESS(rc))
        return NS_ERROR_FAILURE;

    return NS_OK;
}

// utility class used to free memory on exit
class secBuffers
{
public:

    SecBuffer ib[3];

    secBuffers() { memset(&ib, 0, sizeof(ib)); }

    ~secBuffers() 
    {
        if (ib[0].pvBuffer)
            nsMemory::Free(ib[0].pvBuffer);

        if (ib[1].pvBuffer)
            nsMemory::Free(ib[1].pvBuffer);

        if (ib[2].pvBuffer)
            nsMemory::Free(ib[2].pvBuffer);
    }
};

NS_IMETHODIMP
nsAuthSSPI::Wrap(const void *inToken,
                 uint32_t    inTokenLen,
                 bool        confidential,
                 void      **outToken,
                 uint32_t   *outTokenLen)
{
    SECURITY_STATUS rc;

    SecBufferDesc ibd;
    secBuffers bufs;
    SecPkgContext_Sizes sizes;

    rc = (sspi->QueryContextAttributesW)(
         &mCtxt,
         SECPKG_ATTR_SIZES,
         &sizes);

    if (!SEC_SUCCESS(rc))  
        return NS_ERROR_FAILURE;
    
    ibd.cBuffers = 3;
    ibd.pBuffers = bufs.ib;
    ibd.ulVersion = SECBUFFER_VERSION;
    
    // SSPI
    bufs.ib[0].cbBuffer = sizes.cbSecurityTrailer;
    bufs.ib[0].BufferType = SECBUFFER_TOKEN;
    bufs.ib[0].pvBuffer = nsMemory::Alloc(sizes.cbSecurityTrailer);

    if (!bufs.ib[0].pvBuffer)
        return NS_ERROR_OUT_OF_MEMORY;

    // APP Data
    bufs.ib[1].BufferType = SECBUFFER_DATA;
    bufs.ib[1].pvBuffer = nsMemory::Alloc(inTokenLen);
    bufs.ib[1].cbBuffer = inTokenLen;
    
    if (!bufs.ib[1].pvBuffer)
        return NS_ERROR_OUT_OF_MEMORY;

    memcpy(bufs.ib[1].pvBuffer, inToken, inTokenLen);

    // SSPI
    bufs.ib[2].BufferType = SECBUFFER_PADDING;
    bufs.ib[2].cbBuffer = sizes.cbBlockSize;
    bufs.ib[2].pvBuffer = nsMemory::Alloc(bufs.ib[2].cbBuffer);

    if (!bufs.ib[2].pvBuffer)
        return NS_ERROR_OUT_OF_MEMORY;

    rc = (sspi->EncryptMessage)(&mCtxt,
          confidential ? 0 : KERB_WRAP_NO_ENCRYPT,
         &ibd, 0);

    if (SEC_SUCCESS(rc)) {
        int len  = bufs.ib[0].cbBuffer + bufs.ib[1].cbBuffer + bufs.ib[2].cbBuffer;
        char *p = (char *) nsMemory::Alloc(len);

        if (!p)
            return NS_ERROR_OUT_OF_MEMORY;
				
        *outToken = (void *) p;
        *outTokenLen = len;

        memcpy(p, bufs.ib[0].pvBuffer, bufs.ib[0].cbBuffer);
        p += bufs.ib[0].cbBuffer;

        memcpy(p,bufs.ib[1].pvBuffer, bufs.ib[1].cbBuffer);
        p += bufs.ib[1].cbBuffer;

        memcpy(p,bufs.ib[2].pvBuffer, bufs.ib[2].cbBuffer);
        
        return NS_OK;
    }

    return NS_ERROR_FAILURE;
}

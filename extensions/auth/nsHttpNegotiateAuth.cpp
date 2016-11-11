/* vim:set ts=4 sw=4 sts=4 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

#include "nsAuth.h"
#include "nsHttpNegotiateAuth.h"

#include "nsIHttpAuthenticableChannel.h"
#include "nsIProxiedChannel.h"
#include "nsIAuthModule.h"
#include "nsIServiceManager.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIProxyInfo.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsNetCID.h"
#include "plbase64.h"
#include "plstr.h"
#include "prprf.h"
#include "mozilla/Logging.h"
#include "prmem.h"
#include "prnetdb.h"
#include "mozilla/Likely.h"
#include "mozilla/Sprintf.h"
#include "nsIChannel.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "nsIHttpAuthenticatorCallback.h"
#include "mozilla/Mutex.h"
#include "nsICancelable.h"

//-----------------------------------------------------------------------------

static const char kNegotiate[] = "Negotiate";
static const char kNegotiateAuthTrustedURIs[] = "network.negotiate-auth.trusted-uris";
static const char kNegotiateAuthDelegationURIs[] = "network.negotiate-auth.delegation-uris";
static const char kNegotiateAuthAllowProxies[] = "network.negotiate-auth.allow-proxies";
static const char kNegotiateAuthAllowNonFqdn[] = "network.negotiate-auth.allow-non-fqdn";
static const char kNegotiateAuthSSPI[] = "network.auth.use-sspi";

#define kNegotiateLen  (sizeof(kNegotiate)-1)
#define DEFAULT_THREAD_TIMEOUT_MS 30000

//-----------------------------------------------------------------------------

// Return false when the channel comes from a Private browsing window.
static bool
TestNotInPBMode(nsIHttpAuthenticableChannel *authChannel)
{
    nsCOMPtr<nsIChannel> bareChannel = do_QueryInterface(authChannel);
    MOZ_ASSERT(bareChannel);

    if (!NS_UsePrivateBrowsing(bareChannel)) {
        return true;
    }

    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (!prefs) {
        return true;
    }

    // When the "Never remember history" option is set, all channels are
    // set PB mode flag, but here we want to make an exception, users
    // want their credentials go out.
    bool dontRememberHistory;
    if (NS_SUCCEEDED(prefs->GetBoolPref("browser.privatebrowsing.autostart",
                                        &dontRememberHistory)) &&
        dontRememberHistory) {
        return true;
    }

    return false;
}

NS_IMETHODIMP
nsHttpNegotiateAuth::GetAuthFlags(uint32_t *flags)
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
nsHttpNegotiateAuth::ChallengeReceived(nsIHttpAuthenticableChannel *authChannel,
                                       const char *challenge,
                                       bool isProxyAuth,
                                       nsISupports **sessionState,
                                       nsISupports **continuationState,
                                       bool *identityInvalid)
{
    nsIAuthModule *module = (nsIAuthModule *) *continuationState;

    *identityInvalid = false;
    if (module)
        return NS_OK;

    nsresult rv;

    nsCOMPtr<nsIURI> uri;
    rv = authChannel->GetURI(getter_AddRefs(uri));
    if (NS_FAILED(rv))
        return rv;

    uint32_t req_flags = nsIAuthModule::REQ_DEFAULT;
    nsAutoCString service;

    if (isProxyAuth) {
        if (!TestBoolPref(kNegotiateAuthAllowProxies)) {
            LOG(("nsHttpNegotiateAuth::ChallengeReceived proxy auth blocked\n"));
            return NS_ERROR_ABORT;
        }

        req_flags |= nsIAuthModule::REQ_PROXY_AUTH;
        nsCOMPtr<nsIProxyInfo> proxyInfo;
        authChannel->GetProxyInfo(getter_AddRefs(proxyInfo));
        NS_ENSURE_STATE(proxyInfo);

        proxyInfo->GetHost(service);
    }
    else {
        bool allowed = TestNotInPBMode(authChannel) &&
                       (TestNonFqdn(uri) ||
                       TestPref(uri, kNegotiateAuthTrustedURIs));
        if (!allowed) {
            LOG(("nsHttpNegotiateAuth::ChallengeReceived URI blocked\n"));
            return NS_ERROR_ABORT;
        }

        bool delegation = TestPref(uri, kNegotiateAuthDelegationURIs);
        if (delegation) {
            LOG(("  using REQ_DELEGATE\n"));
            req_flags |= nsIAuthModule::REQ_DELEGATE;
        }

        rv = uri->GetAsciiHost(service);
        if (NS_FAILED(rv))
            return rv;
    }

    LOG(("  service = %s\n", service.get()));

    //
    // The correct service name for IIS servers is "HTTP/f.q.d.n", so
    // construct the proper service name for passing to "gss_import_name".
    //
    // TODO: Possibly make this a configurable service name for use
    // with non-standard servers that use stuff like "khttp/f.q.d.n" 
    // instead.
    //
    service.Insert("HTTP@", 0);

    const char *contractID;
    if (TestBoolPref(kNegotiateAuthSSPI)) {
	   LOG(("  using negotiate-sspi\n"));
	   contractID = NS_AUTH_MODULE_CONTRACTID_PREFIX "negotiate-sspi";
    }
    else {
	   LOG(("  using negotiate-gss\n"));
	   contractID = NS_AUTH_MODULE_CONTRACTID_PREFIX "negotiate-gss";
    }

    rv = CallCreateInstance(contractID, &module);

    if (NS_FAILED(rv)) {
        LOG(("  Failed to load Negotiate Module \n"));
        return rv;
    }

    rv = module->Init(service.get(), req_flags, nullptr, nullptr, nullptr);

    if (NS_FAILED(rv)) {
        NS_RELEASE(module);
        return rv;
    }

    *continuationState = module;
    return NS_OK;
}

NS_IMPL_ISUPPORTS(nsHttpNegotiateAuth, nsIHttpAuthenticator)

namespace {

//
// GetNextTokenCompleteEvent
//
// This event is fired on main thread when async call of
// nsHttpNegotiateAuth::GenerateCredentials is finished. During the Run()
// method the nsIHttpAuthenticatorCallback::OnCredsAvailable is called with
// obtained credentials, flags and NS_OK when successful, otherwise 
// NS_ERROR_FAILURE is returned as a result of failed operation.
//
class GetNextTokenCompleteEvent final : public nsIRunnable,
                                        public nsICancelable
{
    virtual ~GetNextTokenCompleteEvent()
    {
        if (mCreds) {
            free(mCreds);
        }
    };

public:
    NS_DECL_THREADSAFE_ISUPPORTS

    explicit GetNextTokenCompleteEvent(nsIHttpAuthenticatorCallback* aCallback)
        : mCallback(aCallback)
        , mCreds(nullptr)
        , mCancelled(false)
    {
    }

    NS_IMETHODIMP DispatchSuccess(char *aCreds,
                                  uint32_t aFlags,
                                  already_AddRefed<nsISupports> aSessionState,
                                  already_AddRefed<nsISupports> aContinuationState)
    {
        // Called from worker thread
        MOZ_ASSERT(!NS_IsMainThread());

        mCreds = aCreds;
        mFlags = aFlags;
        mResult = NS_OK;
        mSessionState = aSessionState;
        mContinuationState = aContinuationState;
        return NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL);
    }

    NS_IMETHODIMP DispatchError(already_AddRefed<nsISupports> aSessionState,
                                already_AddRefed<nsISupports> aContinuationState)
    {
        // Called from worker thread
        MOZ_ASSERT(!NS_IsMainThread());

        mResult = NS_ERROR_FAILURE;
        mSessionState = aSessionState;
        mContinuationState = aContinuationState;
        return NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL);
    }

    NS_IMETHODIMP Run() override
    {
        // Runs on main thread
        MOZ_ASSERT(NS_IsMainThread());

        if (!mCancelled) {
            nsCOMPtr<nsIHttpAuthenticatorCallback> callback;
            callback.swap(mCallback);
            callback->OnCredsGenerated(mCreds, mFlags, mResult, mSessionState, mContinuationState);
        }
        return NS_OK;
    }

    NS_IMETHODIMP Cancel(nsresult aReason) override
    {
        // Supposed to be called from main thread
        MOZ_ASSERT(NS_IsMainThread());

        mCancelled = true;
        return NS_OK;
    }

private:
    nsCOMPtr<nsIHttpAuthenticatorCallback> mCallback;
    char *mCreds; // This class owns it, freed in destructor
    uint32_t mFlags;
    nsresult mResult;
    bool mCancelled;
    nsCOMPtr<nsISupports> mSessionState;
    nsCOMPtr<nsISupports> mContinuationState;
};

NS_IMPL_ISUPPORTS(GetNextTokenCompleteEvent, nsIRunnable, nsICancelable)

//
// GetNextTokenRunnable
//
// This runnable is created by GenerateCredentialsAsync and it runs
// in nsHttpNegotiateAuth::mNegotiateThread and calling GenerateCredentials.
//
class GetNextTokenRunnable final : public mozilla::Runnable
{
    virtual ~GetNextTokenRunnable() {}
    public:
        GetNextTokenRunnable(nsIHttpAuthenticableChannel *authChannel,
                             const char *challenge,
                             bool isProxyAuth,
                             const char16_t *domain,
                             const char16_t *username,
                             const char16_t *password,
                             nsISupports *sessionState,
                             nsISupports *continuationState,
                             GetNextTokenCompleteEvent *aCompleteEvent
                             )
            : mAuthChannel(authChannel)
            , mChallenge(challenge)
            , mIsProxyAuth(isProxyAuth)
            , mDomain(domain)
            , mUsername(username)
            , mPassword(password)
            , mSessionState(sessionState)
            , mContinuationState(continuationState)
            , mCompleteEvent(aCompleteEvent)
        {
        }

        NS_IMETHODIMP Run() override
        {
            // Runs on worker thread
            MOZ_ASSERT(!NS_IsMainThread());

            char *creds;
            uint32_t flags;
            nsresult rv = ObtainCredentialsAndFlags(&creds, &flags);

            // Passing session and continuation state this way to not touch
            // referencing of the object that may not be thread safe.
            // Not having a thread safe referencing doesn't mean the object
            // cannot be used on multiple threads (one example is nsAuthSSPI.)
            // This ensures state objects will be destroyed on the main thread
            // when not changed by GenerateCredentials.
            if (NS_FAILED(rv)) {
                return mCompleteEvent->DispatchError(mSessionState.forget(),
                                                     mContinuationState.forget());
            }

            return mCompleteEvent->DispatchSuccess(creds, flags,
                                                   mSessionState.forget(),
                                                   mContinuationState.forget());
        }

        NS_IMETHODIMP ObtainCredentialsAndFlags(char **aCreds, uint32_t *aFlags)
        {
            nsresult rv;

            // Use negotiate service to call GenerateCredentials outside of main thread
            nsAutoCString contractId;
            contractId.Assign(NS_HTTP_AUTHENTICATOR_CONTRACTID_PREFIX);
            contractId.Append("negotiate");
            nsCOMPtr<nsIHttpAuthenticator> authenticator =
              do_GetService(contractId.get(), &rv);
            NS_ENSURE_SUCCESS(rv, rv);

            nsISupports *sessionState = mSessionState;
            nsISupports *continuationState = mContinuationState;
            // The continuationState is for the sake of completeness propagated
            // to the caller (despite it is not changed in any GenerateCredentials
            // implementation).
            //
            // The only implementation that use sessionState is the
            // nsHttpDigestAuth::GenerateCredentials. Since there's no reason
            // to implement nsHttpDigestAuth::GenerateCredentialsAsync
            // because digest auth does not block the main thread, we won't
            // propagate changes to sessionState to the caller because of
            // the change is too complicated on the caller side.
            //
            // Should any of the session or continuation states change inside
            // this method, they must be threadsafe.
            rv = authenticator->GenerateCredentials(mAuthChannel,
                                                    mChallenge.get(),
                                                    mIsProxyAuth,
                                                    mDomain.get(),
                                                    mUsername.get(),
                                                    mPassword.get(),
                                                    &sessionState,
                                                    &continuationState,
                                                    aFlags,
                                                    aCreds);
            if (mSessionState != sessionState) {
                mSessionState = sessionState;
            }
            if (mContinuationState != continuationState) {
                mContinuationState = continuationState;
            }
            return rv;
        }
    private:
        nsCOMPtr<nsIHttpAuthenticableChannel> mAuthChannel;
        nsCString mChallenge;
        bool mIsProxyAuth;
        nsString mDomain;
        nsString mUsername;
        nsString mPassword;
        nsCOMPtr<nsISupports> mSessionState;
        nsCOMPtr<nsISupports> mContinuationState;
        RefPtr<GetNextTokenCompleteEvent> mCompleteEvent;
};

} // anonymous namespace

NS_IMETHODIMP
nsHttpNegotiateAuth::GenerateCredentialsAsync(nsIHttpAuthenticableChannel *authChannel,
                                              nsIHttpAuthenticatorCallback* aCallback,
                                              const char *challenge,
                                              bool isProxyAuth,
                                              const char16_t *domain,
                                              const char16_t *username,
                                              const char16_t *password,
                                              nsISupports *sessionState,
                                              nsISupports *continuationState,
                                              nsICancelable **aCancelable)
{
   NS_ENSURE_ARG(aCallback);
   NS_ENSURE_ARG_POINTER(aCancelable);

   RefPtr<GetNextTokenCompleteEvent> cancelEvent =
       new GetNextTokenCompleteEvent(aCallback);


   nsCOMPtr<nsIRunnable> getNextTokenRunnable =
       new GetNextTokenRunnable(authChannel,
                                challenge,
                                isProxyAuth,
                                domain,
                                username,
                                password,
                                sessionState,
                                continuationState,
                                cancelEvent);
   cancelEvent.forget(aCancelable);

   nsresult rv;
   if (!mNegotiateThread) {
       mNegotiateThread =
           new mozilla::LazyIdleThread(DEFAULT_THREAD_TIMEOUT_MS,
                                       NS_LITERAL_CSTRING("NegotiateAuth"));
       NS_ENSURE_TRUE(mNegotiateThread, NS_ERROR_OUT_OF_MEMORY);
   }
   rv = mNegotiateThread->Dispatch(getNextTokenRunnable, NS_DISPATCH_NORMAL);
   NS_ENSURE_SUCCESS(rv, rv);

   return NS_OK;
}

//
// GenerateCredentials
//
// This routine is responsible for creating the correct authentication
// blob to pass to the server that requested "Negotiate" authentication.
//
NS_IMETHODIMP
nsHttpNegotiateAuth::GenerateCredentials(nsIHttpAuthenticableChannel *authChannel,
                                         const char *challenge,
                                         bool isProxyAuth,
                                         const char16_t *domain,
                                         const char16_t *username,
                                         const char16_t *password,
                                         nsISupports **sessionState,
                                         nsISupports **continuationState,
                                         uint32_t *flags,
                                         char **creds)
{
    // ChallengeReceived must have been called previously.
    nsIAuthModule *module = (nsIAuthModule *) *continuationState;
    NS_ENSURE_TRUE(module, NS_ERROR_NOT_INITIALIZED);

    *flags = USING_INTERNAL_IDENTITY;

    LOG(("nsHttpNegotiateAuth::GenerateCredentials() [challenge=%s]\n", challenge));

    NS_ASSERTION(creds, "null param");

#ifdef DEBUG
    bool isGssapiAuth =
        !PL_strncasecmp(challenge, kNegotiate, kNegotiateLen);
    NS_ASSERTION(isGssapiAuth, "Unexpected challenge");
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
    uint32_t inTokenLen, outTokenLen;

    if (len > kNegotiateLen) {
        challenge += kNegotiateLen;
        while (*challenge == ' ')
            challenge++;
        len = strlen(challenge);

        // strip off any padding (see bug 230351)
        while (challenge[len - 1] == '=')
            len--;

        inTokenLen = (len * 3)/4;
        inToken = malloc(inTokenLen);
        if (!inToken)
            return (NS_ERROR_OUT_OF_MEMORY);

        //
        // Decode the response that followed the "Negotiate" token
        //
        if (PL_Base64Decode(challenge, len, (char *) inToken) == nullptr) {
            free(inToken);
            return(NS_ERROR_UNEXPECTED);
        }
    }
    else {
        //
        // Initializing, don't use an input token.
        //
        inToken = nullptr;
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
    char *encoded_token = PL_Base64Encode((char *)outToken, outTokenLen, nullptr);

    free(outToken);

    if (!encoded_token)
        return NS_ERROR_OUT_OF_MEMORY;

    LOG(("  Sending a token of length %d\n", outTokenLen));

    // allocate a buffer sizeof("Negotiate" + " " + b64output_token + "\0")
    const int bufsize = kNegotiateLen + 1 + strlen(encoded_token) + 1;
    *creds = (char *) moz_xmalloc(bufsize);
    if (MOZ_UNLIKELY(!*creds))
        rv = NS_ERROR_OUT_OF_MEMORY;
    else
        snprintf(*creds, bufsize, "%s %s", kNegotiate, encoded_token);

    PR_Free(encoded_token);
    return rv;
}

bool
nsHttpNegotiateAuth::TestBoolPref(const char *pref)
{
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (!prefs)
        return false;

    bool val;
    nsresult rv = prefs->GetBoolPref(pref, &val);
    if (NS_FAILED(rv))
        return false;

    return val;
}

bool
nsHttpNegotiateAuth::TestNonFqdn(nsIURI *uri)
{
    nsAutoCString host;
    PRNetAddr addr;

    if (!TestBoolPref(kNegotiateAuthAllowNonFqdn))
        return false;

    if (NS_FAILED(uri->GetAsciiHost(host)))
        return false;

    // return true if host does not contain a dot and is not an ip address
    return !host.IsEmpty() && !host.Contains('.') &&
           PR_StringToNetAddr(host.BeginReading(), &addr) != PR_SUCCESS;
}

bool
nsHttpNegotiateAuth::TestPref(nsIURI *uri, const char *pref)
{
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (!prefs)
        return false;

    nsAutoCString scheme, host;
    int32_t port;

    if (NS_FAILED(uri->GetScheme(scheme)))
        return false;
    if (NS_FAILED(uri->GetAsciiHost(host)))
        return false;
    if (NS_FAILED(uri->GetPort(&port)))
        return false;

    char *hostList;
    if (NS_FAILED(prefs->GetCharPref(pref, &hostList)) || !hostList)
        return false;

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
            return true;
        if (*end == '\0')
            break;
        start = end + 1;
    }
    
    free(hostList);
    return false;
}

bool
nsHttpNegotiateAuth::MatchesBaseURI(const nsCSubstring &matchScheme,
                                    const nsCSubstring &matchHost,
                                    int32_t             matchPort,
                                    const char         *baseStart,
                                    const char         *baseEnd)
{
    // check if scheme://host:port matches baseURI

    // parse the base URI
    const char *hostStart, *schemeEnd = strstr(baseStart, "://");
    if (schemeEnd) {
        // the given scheme must match the parsed scheme exactly
        if (!matchScheme.Equals(Substring(baseStart, schemeEnd)))
            return false;
        hostStart = schemeEnd + 3;
    }
    else
        hostStart = baseStart;

    // XXX this does not work for IPv6-literals
    const char *hostEnd = strchr(hostStart, ':');
    if (hostEnd && hostEnd < baseEnd) {
        // the given port must match the parsed port exactly
        int port = atoi(hostEnd + 1);
        if (matchPort != (int32_t) port)
            return false;
    }
    else
        hostEnd = baseEnd;


    // if we didn't parse out a host, then assume we got a match.
    if (hostStart == hostEnd)
        return true;

    uint32_t hostLen = hostEnd - hostStart;

    // matchHost must either equal host or be a subdomain of host
    if (matchHost.Length() < hostLen)
        return false;

    const char *end = matchHost.EndReading();
    if (PL_strncasecmp(end - hostLen, hostStart, hostLen) == 0) {
        // if matchHost ends with host from the base URI, then make sure it is
        // either an exact match, or prefixed with a dot.  we don't want
        // "foobar.com" to match "bar.com"
        if (matchHost.Length() == hostLen ||
            *(end - hostLen) == '.' ||
            *(end - hostLen - 1) == '.')
            return true;
    }

    return false;
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozJSSubScriptLoader.h"
#include "mozJSComponentLoader.h"
#include "mozJSLoaderUtils.h"

#include "nsIURI.h"
#include "nsIIOService.h"
#include "nsIChannel.h"
#include "nsIInputStream.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsIFileURL.h"
#include "nsScriptLoader.h"
#include "nsIScriptSecurityManager.h"
#include "nsThreadUtils.h"

#include "jsapi.h"
#include "jsfriendapi.h"
#include "nsJSPrincipals.h"
#include "xpcprivate.h" // For xpc::OptionsBase
#include "jswrapper.h"

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/scache/StartupCache.h"
#include "mozilla/scache/StartupCacheUtils.h"
#include "mozilla/unused.h"
#include "nsContentUtils.h"
#include "nsStringGlue.h"
#include "nsCycleCollectionParticipant.h"

using namespace mozilla::scache;
using namespace JS;
using namespace xpc;
using namespace mozilla;
using namespace mozilla::dom;

class MOZ_STACK_CLASS LoadSubScriptOptions : public OptionsBase {
public:
    explicit LoadSubScriptOptions(JSContext* cx = xpc_GetSafeJSContext(),
                                  JSObject* options = nullptr)
        : OptionsBase(cx, options)
        , target(cx)
        , charset(NullString())
        , ignoreCache(false)
        , async(false)
    { }

    virtual bool Parse() {
      return ParseObject("target", &target) &&
             ParseString("charset", charset) &&
             ParseBoolean("ignoreCache", &ignoreCache) &&
             ParseBoolean("async", &async);
    }

    RootedObject target;
    nsString charset;
    bool ignoreCache;
    bool async;
};


/* load() error msgs, XXX localize? */
#define LOAD_ERROR_NOSERVICE "Error creating IO Service."
#define LOAD_ERROR_NOURI "Error creating URI (invalid URL scheme?)"
#define LOAD_ERROR_NOSCHEME "Failed to get URI scheme.  This is bad."
#define LOAD_ERROR_URI_NOT_LOCAL "Trying to load a non-local URI."
#define LOAD_ERROR_NOSTREAM  "Error opening input stream (invalid filename?)"
#define LOAD_ERROR_NOCONTENT "ContentLength not available (not a local URL?)"
#define LOAD_ERROR_BADCHARSET "Error converting to specified charset"
#define LOAD_ERROR_BADREAD   "File Read Error."
#define LOAD_ERROR_READUNDERFLOW "File Read Error (underflow.)"
#define LOAD_ERROR_NOPRINCIPALS "Failed to get principals."
#define LOAD_ERROR_NOSPEC "Failed to get URI spec.  This is bad."
#define LOAD_ERROR_CONTENTTOOBIG "ContentLength is too large"

mozJSSubScriptLoader::mozJSSubScriptLoader() : mSystemPrincipal(nullptr)
{
}

mozJSSubScriptLoader::~mozJSSubScriptLoader()
{
    /* empty */
}

NS_IMPL_ISUPPORTS(mozJSSubScriptLoader, mozIJSSubScriptLoader)

static nsresult
ReportError(JSContext* cx, const char* msg)
{
    RootedValue exn(cx, JS::StringValue(JS_NewStringCopyZ(cx, msg)));
    JS_SetPendingException(cx, exn);
    return NS_OK;
}

static nsresult
ReportError(JSContext* cx, const char* origMsg, nsIURI* uri)
{
    if (!uri)
        return ReportError(cx, origMsg);

    nsAutoCString spec;
    nsresult rv = uri->GetSpec(spec);
    if (NS_FAILED(rv))
        spec.Assign("(unknown)");

    nsAutoCString msg(origMsg);
    msg.Append(": ");
    msg.Append(spec);
    return ReportError(cx, msg.get());
}

nsresult
PrepareScript(nsIURI* uri,
              JSContext* cx,
              RootedObject& targetObj,
              const char* uriStr,
              const nsAString& charset,
              const char* buf,
              int64_t len,
              bool reuseGlobal,
              MutableHandleScript script,
              MutableHandleFunction function)
{
    JS::CompileOptions options(cx);
    options.setFileAndLine(uriStr, 1)
           .setVersion(JSVERSION_LATEST);
    if (!charset.IsVoid()) {
        char16_t* scriptBuf = nullptr;
        size_t scriptLength = 0;

        nsresult rv =
            nsScriptLoader::ConvertToUTF16(nullptr, reinterpret_cast<const uint8_t*>(buf), len,
                                           charset, nullptr, scriptBuf, scriptLength);

        JS::SourceBufferHolder srcBuf(scriptBuf, scriptLength,
                                      JS::SourceBufferHolder::GiveOwnership);

        if (NS_FAILED(rv)) {
            return ReportError(cx, LOAD_ERROR_BADCHARSET, uri);
        }

        if (!reuseGlobal) {
            if (JS_IsGlobalObject(targetObj))
                JS::Compile(cx, options, srcBuf, script);
            else
                JS::CompileForNonSyntacticScope(cx, options, srcBuf, script);
        } else {
            AutoObjectVector scopeChain(cx);
            if (!JS_IsGlobalObject(targetObj) &&
                !scopeChain.append(targetObj)) {
                return NS_ERROR_OUT_OF_MEMORY;
            }
            // XXXbz do we really not care if the compile fails???
            JS::CompileFunction(cx, scopeChain, options, nullptr, 0, nullptr,
                                srcBuf, function);
        }
    } else {
        // We only use lazy source when no special encoding is specified because
        // the lazy source loader doesn't know the encoding.
        if (!reuseGlobal) {
            options.setSourceIsLazy(true);
            if (JS_IsGlobalObject(targetObj))
                JS::Compile(cx, options, buf, len, script);
            else
                JS::CompileForNonSyntacticScope(cx, options, buf, len, script);
        } else {
            AutoObjectVector scopeChain(cx);
            if (!JS_IsGlobalObject(targetObj) &&
                !scopeChain.append(targetObj)) {
                return NS_ERROR_OUT_OF_MEMORY;
            }
            // XXXbz do we really not care if the compile fails???
            JS::CompileFunction(cx, scopeChain, options, nullptr, 0, nullptr,
                                buf, len, function);
        }
    }
    return NS_OK;
}

nsresult
EvalScript(JSContext* cx,
           RootedObject& target_obj,
           MutableHandleValue retval,
           nsIURI* uri,
           bool cache,
           RootedScript& script,
           RootedFunction& function)
{
    if (function) {
        script = JS_GetFunctionScript(cx, function);
    }

    bool ok = false;
    if (function) {
        ok = JS_CallFunction(cx, target_obj, function, JS::HandleValueArray::empty(),
                             retval);
    } else {
        if (JS_IsGlobalObject(target_obj)) {
            ok = JS_ExecuteScript(cx, script, retval);
        } else {
            JS::AutoObjectVector scopeChain(cx);
            ok = scopeChain.append(target_obj) &&
                 JS_ExecuteScript(cx, scopeChain, script, retval);
        }
    }

    if (ok) {
        JSAutoCompartment rac(cx, target_obj);
        if (!JS_WrapValue(cx, retval))
            return NS_ERROR_UNEXPECTED;
    }

    nsAutoCString cachePath;
    JSVersion version = JS_GetVersion(cx);
    cachePath.AppendPrintf("jssubloader/%d", version);
    PathifyURI(uri, cachePath);

    nsCOMPtr<nsIScriptSecurityManager> secman =
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
    if (!secman)
        return NS_OK;

    nsCOMPtr<nsIPrincipal> principal;
    nsresult rv = secman->GetSystemPrincipal(getter_AddRefs(principal));
    if (NS_FAILED(rv) || !principal)
        return rv;

    if (cache && ok && !!script) {
        WriteCachedScript(StartupCache::GetSingleton(),
                          cachePath, cx, principal, script);
    }
    return NS_OK;
}

class AsyncScriptLoader : public nsIIncrementalStreamLoaderObserver
{
public:
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_NSIINCREMENTALSTREAMLOADEROBSERVER

    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(AsyncScriptLoader)

    AsyncScriptLoader(nsIChannel* aChannel, bool aReuseGlobal,
                      JSObject* aTargetObj, const nsAString& aCharset,
                      bool aCache, Promise* aPromise)
        : mChannel(aChannel)
        , mTargetObj(aTargetObj)
        , mPromise(aPromise)
        , mCharset(aCharset)
        , mReuseGlobal(aReuseGlobal)
        , mCache(aCache)
    {
        // Needed for the cycle collector to manage mTargetObj.
        mozilla::HoldJSObjects(this);
    }

private:
    virtual ~AsyncScriptLoader() {
        mozilla::DropJSObjects(this);
    }

    RefPtr<nsIChannel>      mChannel;
    Heap<JSObject*>           mTargetObj;
    RefPtr<Promise>         mPromise;
    nsString                  mCharset;
    bool                      mReuseGlobal;
    bool                      mCache;
};

NS_IMPL_CYCLE_COLLECTION_CLASS(AsyncScriptLoader)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AsyncScriptLoader)
  NS_INTERFACE_MAP_ENTRY(nsIIncrementalStreamLoaderObserver)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(AsyncScriptLoader)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPromise)
  tmp->mTargetObj = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(AsyncScriptLoader)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPromise)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(AsyncScriptLoader)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mTargetObj)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(AsyncScriptLoader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AsyncScriptLoader)

class MOZ_STACK_CLASS AutoRejectPromise
{
  public:
    AutoRejectPromise(JSContext* cx,
                      Promise* aPromise,
                      nsIGlobalObject* aGlobalObject)
      : mCx(cx)
      , mPromise(aPromise)
      , mGlobalObject(aGlobalObject) {}

    ~AutoRejectPromise() {
      if (mPromise) {
        JS::Rooted<JS::Value> undefined(mCx, JS::UndefinedValue());
        mPromise->MaybeReject(mCx, undefined);
      }
    }

    void ResolvePromise(HandleValue aResolveValue) {
      mPromise->MaybeResolve(aResolveValue);
      mPromise = nullptr;
    }

  private:
    JSContext*                mCx;
    RefPtr<Promise>         mPromise;
    nsCOMPtr<nsIGlobalObject> mGlobalObject;
};

NS_IMETHODIMP
AsyncScriptLoader::OnStreamComplete(nsIIncrementalStreamLoader* aLoader,
                                    nsISupports* aContext,
                                    nsresult aStatus,
                                    uint32_t aLength,
                                    const uint8_t* aBuf)
{
    nsCOMPtr<nsIURI> uri;
    mChannel->GetURI(getter_AddRefs(uri));

    nsCOMPtr<nsIGlobalObject> globalObject = xpc::NativeGlobal(mTargetObj);
    AutoEntryScript aes(globalObject, "async loadSubScript");
    JSContext* cx = aes.cx();
    AutoRejectPromise autoPromise(cx, mPromise, globalObject);

    if (NS_FAILED(aStatus)) {
        ReportError(cx, "Unable to load script.", uri);
    }
    // Just notify that we are done with this load.
    NS_ENSURE_SUCCESS(aStatus, NS_OK);

    if (aLength == 0) {
        return ReportError(cx, LOAD_ERROR_NOCONTENT, uri);
    }

    if (aLength > INT32_MAX) {
        return ReportError(cx, LOAD_ERROR_CONTENTTOOBIG, uri);
    }

    RootedFunction function(cx);
    RootedScript script(cx);
    nsAutoCString spec;
    uri->GetSpec(spec);

    RootedObject target_obj(cx, mTargetObj);

    nsresult rv = PrepareScript(uri, cx, target_obj, spec.get(),
                                mCharset,
                                reinterpret_cast<const char*>(aBuf), aLength,
                                mReuseGlobal, &script, &function);
    if (NS_FAILED(rv)) {
        return rv;
    }

    JS::Rooted<JS::Value> retval(cx);
    rv = EvalScript(cx, target_obj, &retval, uri, mCache, script, function);

    if (NS_SUCCEEDED(rv)) {
        autoPromise.ResolvePromise(retval);
    }

    return rv;
}

nsresult
mozJSSubScriptLoader::ReadScriptAsync(nsIURI* uri, JSObject* targetObjArg,
                                      const nsAString& charset,
                                      nsIIOService* serv, bool reuseGlobal,
                                      bool cache, MutableHandleValue retval)
{
    RootedObject target_obj(nsContentUtils::RootingCx(), targetObjArg);

    nsCOMPtr<nsIGlobalObject> globalObject = xpc::NativeGlobal(target_obj);
    ErrorResult result;

    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(globalObject))) {
      return NS_ERROR_UNEXPECTED;
    }

    RefPtr<Promise> promise = Promise::Create(globalObject, result);
    if (result.Failed()) {
      promise = nullptr;
    }

    DebugOnly<bool> asJS = ToJSValue(jsapi.cx(), promise, retval);
    MOZ_ASSERT(asJS, "Should not fail to convert the promise to a JS value");

    // We create a channel and call SetContentType, to avoid expensive MIME type
    // lookups (bug 632490).
    nsCOMPtr<nsIChannel> channel;
    nsresult rv;
    rv = NS_NewChannel(getter_AddRefs(channel),
                       uri,
                       nsContentUtils::GetSystemPrincipal(),
                       nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                       nsIContentPolicy::TYPE_OTHER,
                       nullptr,  // aLoadGroup
                       nullptr,  // aCallbacks
                       nsIRequest::LOAD_NORMAL,
                       serv);

    if (!NS_SUCCEEDED(rv)) {
        return rv;
    }

    channel->SetContentType(NS_LITERAL_CSTRING("application/javascript"));

    RefPtr<AsyncScriptLoader> loadObserver =
        new AsyncScriptLoader(channel,
                              reuseGlobal,
                              target_obj,
                              charset,
                              cache,
                              promise);

    nsCOMPtr<nsIIncrementalStreamLoader> loader;
    rv = NS_NewIncrementalStreamLoader(getter_AddRefs(loader), loadObserver);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIStreamListener> listener = loader.get();
    return channel->AsyncOpen2(listener);
}

nsresult
mozJSSubScriptLoader::ReadScript(nsIURI* uri, JSContext* cx, JSObject* targetObjArg,
                                 const nsAString& charset, const char* uriStr,
                                 nsIIOService* serv, nsIPrincipal* principal,
                                 bool reuseGlobal, JS::MutableHandleScript script,
                                 JS::MutableHandleFunction function)
{
    script.set(nullptr);
    function.set(nullptr);

    RootedObject target_obj(cx, targetObjArg);

    // We create a channel and call SetContentType, to avoid expensive MIME type
    // lookups (bug 632490).
    nsCOMPtr<nsIChannel> chan;
    nsCOMPtr<nsIInputStream> instream;
    nsresult rv;
    rv = NS_NewChannel(getter_AddRefs(chan),
                       uri,
                       nsContentUtils::GetSystemPrincipal(),
                       nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                       nsIContentPolicy::TYPE_OTHER,
                       nullptr,  // aLoadGroup
                       nullptr,  // aCallbacks
                       nsIRequest::LOAD_NORMAL,
                       serv);

    if (NS_SUCCEEDED(rv)) {
        chan->SetContentType(NS_LITERAL_CSTRING("application/javascript"));
        rv = chan->Open2(getter_AddRefs(instream));
    }

    if (NS_FAILED(rv)) {
        return ReportError(cx, LOAD_ERROR_NOSTREAM, uri);
    }

    int64_t len = -1;

    rv = chan->GetContentLength(&len);
    if (NS_FAILED(rv) || len == -1) {
        return ReportError(cx, LOAD_ERROR_NOCONTENT, uri);
    }

    if (len > INT32_MAX) {
        return ReportError(cx, LOAD_ERROR_CONTENTTOOBIG, uri);
    }

    nsCString buf;
    rv = NS_ReadInputStreamToString(instream, buf, len);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = PrepareScript(uri, cx, target_obj, uriStr, charset,
                       buf.get(), len,
                       reuseGlobal,
                       script, function);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

NS_IMETHODIMP
mozJSSubScriptLoader::LoadSubScript(const nsAString& url,
                                    HandleValue target,
                                    const nsAString& charset,
                                    JSContext* cx,
                                    MutableHandleValue retval)
{
    /*
     * Loads a local url and evals it into the current cx
     * Synchronous (an async version would be cool too.)
     *   url: The url to load.  Must be local so that it can be loaded
     *        synchronously.
     *   target_obj: Optional object to eval the script onto (defaults to context
     *               global)
     *   charset: Optional character set to use for reading
     *   returns: Whatever jsval the script pointed to by the url returns.
     * Should ONLY (O N L Y !) be called from JavaScript code.
     */
    LoadSubScriptOptions options(cx);
    options.charset = charset;
    options.target = target.isObject() ? &target.toObject() : nullptr;
    return DoLoadSubScriptWithOptions(url, options, cx, retval);
}


NS_IMETHODIMP
mozJSSubScriptLoader::LoadSubScriptWithOptions(const nsAString& url,
                                               HandleValue optionsVal,
                                               JSContext* cx,
                                               MutableHandleValue retval)
{
    if (!optionsVal.isObject())
        return NS_ERROR_INVALID_ARG;
    LoadSubScriptOptions options(cx, &optionsVal.toObject());
    if (!options.Parse())
        return NS_ERROR_INVALID_ARG;
    return DoLoadSubScriptWithOptions(url, options, cx, retval);
}

nsresult
mozJSSubScriptLoader::DoLoadSubScriptWithOptions(const nsAString& url,
                                                 LoadSubScriptOptions& options,
                                                 JSContext* cx,
                                                 MutableHandleValue retval)
{
    nsresult rv = NS_OK;

    /* set the system principal if it's not here already */
    if (!mSystemPrincipal) {
        nsCOMPtr<nsIScriptSecurityManager> secman =
            do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
        if (!secman)
            return NS_OK;

        rv = secman->GetSystemPrincipal(getter_AddRefs(mSystemPrincipal));
        if (NS_FAILED(rv) || !mSystemPrincipal)
            return rv;
    }

    RootedObject targetObj(cx);
    mozJSComponentLoader* loader = mozJSComponentLoader::Get();
    rv = loader->FindTargetObject(cx, &targetObj);
    NS_ENSURE_SUCCESS(rv, rv);

    // We base reusingGlobal off of what the loader told us, but we may not
    // actually be using that object.
    bool reusingGlobal = !JS_IsGlobalObject(targetObj);

    if (options.target)
        targetObj = options.target;

    // Remember an object out of the calling compartment so that we
    // can properly wrap the result later.
    nsCOMPtr<nsIPrincipal> principal = mSystemPrincipal;
    RootedObject result_obj(cx, targetObj);
    targetObj = JS_FindCompilationScope(cx, targetObj);
    if (!targetObj)
        return NS_ERROR_FAILURE;

    if (targetObj != result_obj)
        principal = GetObjectPrincipal(targetObj);

    JSAutoCompartment ac(cx, targetObj);

    /* load up the url.  From here on, failures are reflected as ``custom''
     * js exceptions */
    nsCOMPtr<nsIURI> uri;
    nsAutoCString uriStr;
    nsAutoCString scheme;

    // Figure out who's calling us
    JS::AutoFilename filename;
    if (!JS::DescribeScriptedCaller(cx, &filename)) {
        // No scripted frame means we don't know who's calling, bail.
        return NS_ERROR_FAILURE;
    }

    // Suppress caching if we're compiling as content.
    StartupCache* cache = (principal == mSystemPrincipal)
                          ? StartupCache::GetSingleton()
                          : nullptr;
    nsCOMPtr<nsIIOService> serv = do_GetService(NS_IOSERVICE_CONTRACTID);
    if (!serv) {
        return ReportError(cx, LOAD_ERROR_NOSERVICE);
    }

    // Make sure to explicitly create the URI, since we'll need the
    // canonicalized spec.
    rv = NS_NewURI(getter_AddRefs(uri), NS_LossyConvertUTF16toASCII(url).get(), nullptr, serv);
    if (NS_FAILED(rv)) {
        return ReportError(cx, LOAD_ERROR_NOURI);
    }

    rv = uri->GetSpec(uriStr);
    if (NS_FAILED(rv)) {
        return ReportError(cx, LOAD_ERROR_NOSPEC);
    }

    rv = uri->GetScheme(scheme);
    if (NS_FAILED(rv)) {
        return ReportError(cx, LOAD_ERROR_NOSCHEME, uri);
    }

    if (!scheme.EqualsLiteral("chrome") && !scheme.EqualsLiteral("app")) {
        // This might be a URI to a local file, though!
        nsCOMPtr<nsIURI> innerURI = NS_GetInnermostURI(uri);
        nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(innerURI);
        if (!fileURL) {
            return ReportError(cx, LOAD_ERROR_URI_NOT_LOCAL, uri);
        }

        // For file URIs prepend the filename with the filename of the
        // calling script, and " -> ". See bug 418356.
        nsAutoCString tmp(filename.get());
        tmp.AppendLiteral(" -> ");
        tmp.Append(uriStr);

        uriStr = tmp;
    }

    JSVersion version = JS_GetVersion(cx);
    nsAutoCString cachePath;
    cachePath.AppendPrintf("jssubloader/%d", version);
    PathifyURI(uri, cachePath);

    RootedFunction function(cx);
    RootedScript script(cx);
    if (cache && !options.ignoreCache)
        rv = ReadCachedScript(cache, cachePath, cx, mSystemPrincipal, &script);

    // If we are doing an async load, trigger it and bail out.
    if (!script && options.async) {
        return ReadScriptAsync(uri, targetObj, options.charset, serv,
                               reusingGlobal, !!cache, retval);
    }

    if (!script) {
        rv = ReadScript(uri, cx, targetObj, options.charset,
                        static_cast<const char*>(uriStr.get()), serv,
                        principal, reusingGlobal, &script, &function);
    } else {
        cache = nullptr;
    }

    if (NS_FAILED(rv) || (!script && !function))
        return rv;

    return EvalScript(cx, targetObj, retval, uri, !!cache, script, function);
}

/**
  * Let us compile scripts from a URI off the main thread.
  */

class ScriptPrecompiler : public nsIIncrementalStreamLoaderObserver
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINCREMENTALSTREAMLOADEROBSERVER

    ScriptPrecompiler(nsIObserver* aObserver,
                      nsIPrincipal* aPrincipal,
                      nsIChannel* aChannel)
        : mObserver(aObserver)
        , mPrincipal(aPrincipal)
        , mChannel(aChannel)
        , mScriptBuf(nullptr)
        , mScriptLength(0)
    {}

    static void OffThreadCallback(void* aToken, void* aData);

    /* Sends the "done" notification back. Main thread only. */
    void SendObserverNotification();

private:
    virtual ~ScriptPrecompiler()
    {
      if (mScriptBuf) {
        js_free(mScriptBuf);
      }
    }

    RefPtr<nsIObserver> mObserver;
    RefPtr<nsIPrincipal> mPrincipal;
    RefPtr<nsIChannel> mChannel;
    char16_t* mScriptBuf;
    size_t mScriptLength;
};

NS_IMPL_ISUPPORTS(ScriptPrecompiler, nsIIncrementalStreamLoaderObserver);

class NotifyPrecompilationCompleteRunnable : public nsRunnable
{
public:
    NS_DECL_NSIRUNNABLE

    explicit NotifyPrecompilationCompleteRunnable(ScriptPrecompiler* aPrecompiler)
        : mPrecompiler(aPrecompiler)
        , mToken(nullptr)
    {}

    void SetToken(void* aToken) {
        MOZ_ASSERT(aToken && !mToken);
        mToken = aToken;
    }

protected:
    RefPtr<ScriptPrecompiler> mPrecompiler;
    void* mToken;
};

/* RAII helper class to send observer notifications */
class AutoSendObserverNotification {
public:
    explicit AutoSendObserverNotification(ScriptPrecompiler* aPrecompiler)
        : mPrecompiler(aPrecompiler)
    {}

    ~AutoSendObserverNotification() {
        if (mPrecompiler) {
            mPrecompiler->SendObserverNotification();
        }
    }

    void Disarm() {
        mPrecompiler = nullptr;
    }

private:
    ScriptPrecompiler* mPrecompiler;
};

NS_IMETHODIMP
NotifyPrecompilationCompleteRunnable::Run(void)
{
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mPrecompiler);

    AutoSendObserverNotification notifier(mPrecompiler);

    if (mToken) {
        JSRuntime* rt = XPCJSRuntime::Get()->Runtime();
        NS_ENSURE_TRUE(rt, NS_ERROR_FAILURE);
        JS::FinishOffThreadScript(nullptr, rt, mToken);
    }

    return NS_OK;
}

NS_IMETHODIMP
ScriptPrecompiler::OnStreamComplete(nsIIncrementalStreamLoader* aLoader,
                                    nsISupports* aContext,
                                    nsresult aStatus,
                                    uint32_t aLength,
                                    const uint8_t* aString)
{
    AutoSendObserverNotification notifier(this);

    // Just notify that we are done with this load.
    NS_ENSURE_SUCCESS(aStatus, NS_OK);

    // Convert data to char16_t* and prepare to call CompileOffThread.
    nsAutoString hintCharset;
    nsresult rv =
        nsScriptLoader::ConvertToUTF16(mChannel, aString, aLength,
                                       hintCharset, nullptr,
                                       mScriptBuf, mScriptLength);

    NS_ENSURE_SUCCESS(rv, NS_OK);

    // Our goal is to cache persistently the compiled script and to avoid quota
    // checks. Since the caching mechanism decide the persistence type based on
    // the principal, we create a new global with the app's principal.
    // We then enter its compartment to compile with its principal.
    AutoSafeJSContext cx;
    RootedValue v(cx);
    SandboxOptions sandboxOptions;
    sandboxOptions.sandboxName.AssignASCII("asm.js precompilation");
    sandboxOptions.invisibleToDebugger = true;
    sandboxOptions.discardSource = true;
    rv = CreateSandboxObject(cx, &v, mPrincipal, sandboxOptions);
    NS_ENSURE_SUCCESS(rv, NS_OK);

    JSAutoCompartment ac(cx, js::UncheckedUnwrap(&v.toObject()));

    JS::CompileOptions options(cx, JSVERSION_DEFAULT);
    options.forceAsync = true;
    options.installedFile = true;

    nsCOMPtr<nsIURI> uri;
    mChannel->GetURI(getter_AddRefs(uri));
    nsAutoCString spec;
    uri->GetSpec(spec);
    options.setFile(spec.get());

    if (!JS::CanCompileOffThread(cx, options, mScriptLength)) {
        NS_WARNING("Can't compile script off thread!");
        return NS_OK;
    }

    RefPtr<NotifyPrecompilationCompleteRunnable> runnable =
        new NotifyPrecompilationCompleteRunnable(this);

    if (!JS::CompileOffThread(cx, options,
                              mScriptBuf, mScriptLength,
                              OffThreadCallback,
                              static_cast<void*>(runnable))) {
        NS_WARNING("Failed to compile script off thread!");
        return NS_OK;
    }

    Unused << runnable.forget();
    notifier.Disarm();

    return NS_OK;
}

/* static */
void
ScriptPrecompiler::OffThreadCallback(void* aToken, void* aData)
{
    RefPtr<NotifyPrecompilationCompleteRunnable> runnable =
        dont_AddRef(static_cast<NotifyPrecompilationCompleteRunnable*>(aData));
    runnable->SetToken(aToken);

    NS_DispatchToMainThread(runnable);
}

void
ScriptPrecompiler::SendObserverNotification()
{
    MOZ_ASSERT(mChannel && mObserver);
    MOZ_ASSERT(NS_IsMainThread());

    nsCOMPtr<nsIURI> uri;
    mChannel->GetURI(getter_AddRefs(uri));
    mObserver->Observe(uri, "script-precompiled", nullptr);
}

NS_IMETHODIMP
mozJSSubScriptLoader::PrecompileScript(nsIURI* aURI,
                                       nsIPrincipal* aPrincipal,
                                       nsIObserver* aObserver)
{
    nsCOMPtr<nsIChannel> channel;
    nsresult rv = NS_NewChannel(getter_AddRefs(channel),
                                aURI,
                                nsContentUtils::GetSystemPrincipal(),
                                nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                                nsIContentPolicy::TYPE_OTHER);

    NS_ENSURE_SUCCESS(rv, rv);

    RefPtr<ScriptPrecompiler> loadObserver =
        new ScriptPrecompiler(aObserver, aPrincipal, channel);

    nsCOMPtr<nsIIncrementalStreamLoader> loader;
    rv = NS_NewIncrementalStreamLoader(getter_AddRefs(loader), loadObserver);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIStreamListener> listener = loader.get();
    rv = channel->AsyncOpen2(listener);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

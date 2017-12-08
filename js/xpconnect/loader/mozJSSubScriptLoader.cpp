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

#include "jsapi.h"
#include "jsfriendapi.h"
#include "xpcprivate.h" // For xpc::OptionsBase
#include "jswrapper.h"

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/ScriptLoader.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/ScriptPreloader.h"
#include "mozilla/scache/StartupCache.h"
#include "mozilla/scache/StartupCacheUtils.h"
#include "mozilla/Unused.h"
#include "nsContentUtils.h"
#include "nsString.h"
#include "nsCycleCollectionParticipant.h"
#include "GeckoProfiler.h"

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
        , charset(VoidString())
        , ignoreCache(false)
        , async(false)
        , wantReturnValue(false)
    { }

    virtual bool Parse() {
      return ParseObject("target", &target) &&
             ParseString("charset", charset) &&
             ParseBoolean("ignoreCache", &ignoreCache) &&
             ParseBoolean("async", &async) &&
             ParseBoolean("wantReturnValue", &wantReturnValue);
    }

    RootedObject target;
    nsString charset;
    bool ignoreCache;
    bool async;
    bool wantReturnValue;
};


/* load() error msgs, XXX localize? */
#define LOAD_ERROR_NOSERVICE "Error creating IO Service."
#define LOAD_ERROR_NOURI "Error creating URI (invalid URL scheme?)"
#define LOAD_ERROR_NOSCHEME "Failed to get URI scheme.  This is bad."
#define LOAD_ERROR_URI_NOT_LOCAL "Trying to load a non-local URI."
#define LOAD_ERROR_NOSTREAM  "Error opening input stream (invalid filename?)"
#define LOAD_ERROR_NOCONTENT "ContentLength not available (not a local URL?)"
#define LOAD_ERROR_BADCHARSET "Error converting to specified charset"
#define LOAD_ERROR_NOSPEC "Failed to get URI spec.  This is bad."
#define LOAD_ERROR_CONTENTTOOBIG "ContentLength is too large"

mozJSSubScriptLoader::mozJSSubScriptLoader()
{
}

mozJSSubScriptLoader::~mozJSSubScriptLoader()
{
}

NS_IMPL_ISUPPORTS(mozJSSubScriptLoader, mozIJSSubScriptLoader)

#define JSSUB_CACHE_PREFIX(aType) "jssubloader/" aType

static void
SubscriptCachePath(JSContext* cx, nsIURI* uri, JS::HandleObject targetObj, nsACString& cachePath)
{
    // StartupCache must distinguish between non-syntactic vs global when
    // computing the cache key.
    bool hasNonSyntacticScope = !JS_IsGlobalObject(targetObj);
    cachePath.Assign(hasNonSyntacticScope ? JSSUB_CACHE_PREFIX("non-syntactic")
                                          : JSSUB_CACHE_PREFIX("global"));
    PathifyURI(uri, cachePath);
}

static void
ReportError(JSContext* cx, const nsACString& msg)
{
    NS_ConvertUTF8toUTF16 ucMsg(msg);

    RootedValue exn(cx);
    if (xpc::NonVoidStringToJsval(cx, ucMsg, &exn)) {
        JS_SetPendingException(cx, exn);
    }
}

static void
ReportError(JSContext* cx, const char* origMsg, nsIURI* uri)
{
    if (!uri) {
        ReportError(cx, nsDependentCString(origMsg));
        return;
    }

    nsAutoCString spec;
    nsresult rv = uri->GetSpec(spec);
    if (NS_FAILED(rv))
        spec.AssignLiteral("(unknown)");

    nsAutoCString msg(origMsg);
    msg.AppendLiteral(": ");
    msg.Append(spec);
    ReportError(cx, msg);
}

static bool
PrepareScript(nsIURI* uri,
              JSContext* cx,
              HandleObject targetObj,
              const char* uriStr,
              const nsAString& charset,
              const char* buf,
              int64_t len,
              bool wantReturnValue,
              MutableHandleScript script)
{
    JS::CompileOptions options(cx);
    options.setFileAndLine(uriStr, 1)
           .setNoScriptRval(!wantReturnValue);
    if (!charset.IsVoid()) {
        char16_t* scriptBuf = nullptr;
        size_t scriptLength = 0;

        nsresult rv =
            ScriptLoader::ConvertToUTF16(nullptr, reinterpret_cast<const uint8_t*>(buf), len,
                                         charset, nullptr, scriptBuf, scriptLength);

        JS::SourceBufferHolder srcBuf(scriptBuf, scriptLength,
                                      JS::SourceBufferHolder::GiveOwnership);

        if (NS_FAILED(rv)) {
            ReportError(cx, LOAD_ERROR_BADCHARSET, uri);
            return false;
        }

        if (JS_IsGlobalObject(targetObj)) {
            return JS::Compile(cx, options, srcBuf, script);
        }
        return JS::CompileForNonSyntacticScope(cx, options, srcBuf, script);
    }
    // We only use lazy source when no special encoding is specified because
    // the lazy source loader doesn't know the encoding.
    options.setSourceIsLazy(true);
    if (JS_IsGlobalObject(targetObj)) {
        return JS::Compile(cx, options, buf, len, script);
    }
    return JS::CompileForNonSyntacticScope(cx, options, buf, len, script);
}

static bool
EvalScript(JSContext* cx,
           HandleObject targetObj,
           HandleObject loadScope,
           MutableHandleValue retval,
           nsIURI* uri,
           bool startupCache,
           bool preloadCache,
           MutableHandleScript script)
{
    if (JS_IsGlobalObject(targetObj)) {
        if (!JS::CloneAndExecuteScript(cx, script, retval)) {
            return false;
        }
    } else if (js::IsJSMEnvironment(targetObj)) {
        if (!ExecuteInJSMEnvironment(cx, script, targetObj)) {
            return false;
        }
        retval.setUndefined();
    } else {
        JS::AutoObjectVector envChain(cx);
        if (!envChain.append(targetObj)) {
            return false;
        }
        if (!loadScope) {
            // A null loadScope means we are cross-compartment. In this case, we
            // should check the target isn't in the JSM loader shared-global or
            // we will contaiminate all JSMs in the compartment.
            //
            // NOTE: If loadScope is already a shared-global JSM, we can't
            // determine which JSM the target belongs to and have to assume it
            // is in our JSM.
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
            JSObject* targetGlobal = js::GetGlobalForObjectCrossCompartment(targetObj);
            MOZ_DIAGNOSTIC_ASSERT(!mozJSComponentLoader::Get()->IsLoaderGlobal(targetGlobal),
                                  "Don't load subscript into target in a shared-global JSM");
#endif
            if (!JS::CloneAndExecuteScript(cx, envChain, script, retval)) {
                return false;
            }
        } else if (JS_IsGlobalObject(loadScope)) {
            if (!JS::CloneAndExecuteScript(cx, envChain, script, retval)) {
                return false;
            }
        } else {
            MOZ_ASSERT(js::IsJSMEnvironment(loadScope));
            if (!js::ExecuteInJSMEnvironment(cx, script, loadScope, envChain)) {
                return false;
            }
            retval.setUndefined();
        }
    }

    JSAutoCompartment rac(cx, targetObj);
    if (!JS_WrapValue(cx, retval)) {
        return false;
    }

    if (script && (startupCache || preloadCache)) {
        nsAutoCString cachePath;
        SubscriptCachePath(cx, uri, targetObj, cachePath);

        nsCString uriStr;
        if (preloadCache && NS_SUCCEEDED(uri->GetSpec(uriStr))) {
            // Note that, when called during startup, this will keep the
            // original JSScript object alive for an indefinite amount of time.
            // This has the side-effect of keeping the global that the script
            // was compiled for alive, too.
            //
            // For most startups, the global in question will be the
            // CompilationScope, since we pre-compile any scripts that were
            // needed during the last startup in that scope. But for startups
            // when a non-cached script is used (e.g., after add-on
            // installation), this may be a Sandbox global, which may be
            // nuked but held alive by the JSScript.
            //
            // In general, this isn't a problem, since add-on Sandboxes which
            // use the script preloader are not destroyed until add-on shutdown,
            // and when add-ons are uninstalled or upgraded, the preloader cache
            // is immediately flushed after shutdown. But it's possible to
            // disable and reenable an add-on without uninstalling it, leading
            // to cached scripts being held alive, and tied to nuked Sandbox
            // globals. Given the unusual circumstances required to trigger
            // this, it's not a major concern. But it should be kept in mind.
            ScriptPreloader::GetSingleton().NoteScript(uriStr, cachePath, script);
        }

        if (startupCache) {
            JSAutoCompartment ac(cx, script);
            WriteCachedScript(StartupCache::GetSingleton(), cachePath, cx, script);
        }
    }

    return true;
}

class AsyncScriptLoader : public nsIIncrementalStreamLoaderObserver
{
public:
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_NSIINCREMENTALSTREAMLOADEROBSERVER

    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(AsyncScriptLoader)

    AsyncScriptLoader(nsIChannel* aChannel, bool aWantReturnValue,
                      JSObject* aTargetObj, JSObject* aLoadScope,
                      const nsAString& aCharset, bool aCache,
                      Promise* aPromise)
        : mChannel(aChannel)
        , mTargetObj(aTargetObj)
        , mLoadScope(aLoadScope)
        , mPromise(aPromise)
        , mCharset(aCharset)
        , mWantReturnValue(aWantReturnValue)
        , mCache(aCache)
    {
        // Needed for the cycle collector to manage mTargetObj.
        mozilla::HoldJSObjects(this);
    }

private:
    virtual ~AsyncScriptLoader() {
        mozilla::DropJSObjects(this);
    }

    RefPtr<nsIChannel> mChannel;
    Heap<JSObject*> mTargetObj;
    Heap<JSObject*> mLoadScope;
    RefPtr<Promise> mPromise;
    nsString mCharset;
    bool mWantReturnValue;
    bool mCache;
};

NS_IMPL_CYCLE_COLLECTION_CLASS(AsyncScriptLoader)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AsyncScriptLoader)
  NS_INTERFACE_MAP_ENTRY(nsIIncrementalStreamLoaderObserver)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(AsyncScriptLoader)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPromise)
  tmp->mTargetObj = nullptr;
  tmp->mLoadScope = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(AsyncScriptLoader)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPromise)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(AsyncScriptLoader)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mTargetObj)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mLoadScope)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(AsyncScriptLoader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AsyncScriptLoader)

class MOZ_STACK_CLASS AutoRejectPromise
{
public:
    AutoRejectPromise(AutoEntryScript& aAutoEntryScript,
                      Promise* aPromise,
                      nsIGlobalObject* aGlobalObject)
        : mAutoEntryScript(aAutoEntryScript)
        , mPromise(aPromise)
        , mGlobalObject(aGlobalObject) {}

    ~AutoRejectPromise() {
        if (mPromise) {
            JSContext* cx = mAutoEntryScript.cx();
            RootedValue rejectionValue(cx, JS::UndefinedValue());
            if (mAutoEntryScript.HasException()) {
                Unused << mAutoEntryScript.PeekException(&rejectionValue);
            }
            mPromise->MaybeReject(cx, rejectionValue);
        }
    }

    void ResolvePromise(HandleValue aResolveValue) {
        mPromise->MaybeResolve(aResolveValue);
        mPromise = nullptr;
    }

private:
    AutoEntryScript& mAutoEntryScript;
    RefPtr<Promise> mPromise;
    nsCOMPtr<nsIGlobalObject> mGlobalObject;
};

NS_IMETHODIMP
AsyncScriptLoader::OnIncrementalData(nsIIncrementalStreamLoader* aLoader,
                                     nsISupports* aContext,
                                     uint32_t aDataLength,
                                     const uint8_t* aData,
                                     uint32_t *aConsumedData)
{
    return NS_OK;
}

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
    AutoRejectPromise autoPromise(aes, mPromise, globalObject);
    JSContext* cx = aes.cx();

    if (NS_FAILED(aStatus)) {
        ReportError(cx, "Unable to load script.", uri);
    }
    // Just notify that we are done with this load.
    NS_ENSURE_SUCCESS(aStatus, NS_OK);

    if (aLength == 0) {
        ReportError(cx, LOAD_ERROR_NOCONTENT, uri);
        return NS_OK;
    }

    if (aLength > INT32_MAX) {
        ReportError(cx, LOAD_ERROR_CONTENTTOOBIG, uri);
        return NS_OK;
    }

    RootedScript script(cx);
    nsAutoCString spec;
    nsresult rv = uri->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);

    RootedObject targetObj(cx, mTargetObj);
    RootedObject loadScope(cx, mLoadScope);

    if (!PrepareScript(uri, cx, targetObj, spec.get(), mCharset,
                       reinterpret_cast<const char*>(aBuf), aLength,
                       mWantReturnValue, &script))
    {
        return NS_OK;
    }

    JS::Rooted<JS::Value> retval(cx);
    if (EvalScript(cx, targetObj, loadScope, &retval, uri, mCache,
                   mCache && !mWantReturnValue,
                   &script)) {
        autoPromise.ResolvePromise(retval);
    }

    return NS_OK;
}

nsresult
mozJSSubScriptLoader::ReadScriptAsync(nsIURI* uri,
                                      HandleObject targetObj,
                                      HandleObject loadScope,
                                      const nsAString& charset,
                                      nsIIOService* serv,
                                      bool wantReturnValue,
                                      bool cache,
                                      MutableHandleValue retval)
{
    nsCOMPtr<nsIGlobalObject> globalObject = xpc::NativeGlobal(targetObj);
    ErrorResult result;

    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(globalObject))) {
        return NS_ERROR_UNEXPECTED;
    }

    RefPtr<Promise> promise = Promise::Create(globalObject, result);
    if (result.Failed()) {
        return result.StealNSResult();
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
                              wantReturnValue,
                              targetObj,
                              loadScope,
                              charset,
                              cache,
                              promise);

    nsCOMPtr<nsIIncrementalStreamLoader> loader;
    rv = NS_NewIncrementalStreamLoader(getter_AddRefs(loader), loadObserver);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIStreamListener> listener = loader.get();
    return channel->AsyncOpen2(listener);
}

bool
mozJSSubScriptLoader::ReadScript(nsIURI* uri,
                                 JSContext* cx,
                                 HandleObject targetObj,
                                 const nsAString& charset,
                                 const char* uriStr,
                                 nsIIOService* serv,
                                 bool wantReturnValue,
                                 MutableHandleScript script)
{
    script.set(nullptr);

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
        ReportError(cx, LOAD_ERROR_NOSTREAM, uri);
        return false;
    }

    int64_t len = -1;

    rv = chan->GetContentLength(&len);
    if (NS_FAILED(rv) || len == -1) {
        ReportError(cx, LOAD_ERROR_NOCONTENT, uri);
        return false;
    }

    if (len > INT32_MAX) {
        ReportError(cx, LOAD_ERROR_CONTENTTOOBIG, uri);
        return false;
    }

    nsCString buf;
    rv = NS_ReadInputStreamToString(instream, buf, len);
    NS_ENSURE_SUCCESS(rv, false);

    return PrepareScript(uri, cx, targetObj, uriStr, charset,
                         buf.get(), len, wantReturnValue,
                         script);
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
     *   targetObj: Optional object to eval the script onto (defaults to context
     *              global)
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
    RootedObject targetObj(cx);
    RootedObject loadScope(cx);
    mozJSComponentLoader* loader = mozJSComponentLoader::Get();
    loader->FindTargetObject(cx, &loadScope);

    if (options.target)
        targetObj = options.target;
    else
        targetObj = loadScope;

    targetObj = JS_FindCompilationScope(cx, targetObj);
    if (!targetObj || !loadScope)
        return NS_ERROR_FAILURE;

    if (js::GetObjectCompartment(loadScope) != js::GetObjectCompartment(targetObj))
        loadScope = nullptr;

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

    JSAutoCompartment ac(cx, targetObj);

    nsCOMPtr<nsIIOService> serv = do_GetService(NS_IOSERVICE_CONTRACTID);
    if (!serv) {
        ReportError(cx, NS_LITERAL_CSTRING(LOAD_ERROR_NOSERVICE));
        return NS_OK;
    }

    NS_LossyConvertUTF16toASCII asciiUrl(url);
    AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING(
        "mozJSSubScriptLoader::DoLoadSubScriptWithOptions", OTHER, asciiUrl);

    // Make sure to explicitly create the URI, since we'll need the
    // canonicalized spec.
    rv = NS_NewURI(getter_AddRefs(uri), asciiUrl.get(), nullptr, serv);
    if (NS_FAILED(rv)) {
        ReportError(cx, NS_LITERAL_CSTRING(LOAD_ERROR_NOURI));
        return NS_OK;
    }

    rv = uri->GetSpec(uriStr);
    if (NS_FAILED(rv)) {
        ReportError(cx, NS_LITERAL_CSTRING(LOAD_ERROR_NOSPEC));
        return NS_OK;
    }

    rv = uri->GetScheme(scheme);
    if (NS_FAILED(rv)) {
        ReportError(cx, LOAD_ERROR_NOSCHEME, uri);
        return NS_OK;
    }

    if (!scheme.EqualsLiteral("chrome") && !scheme.EqualsLiteral("app") &&
        !scheme.EqualsLiteral("blob")) {
        // This might be a URI to a local file, though!
        nsCOMPtr<nsIURI> innerURI = NS_GetInnermostURI(uri);
        nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(innerURI);
        if (!fileURL) {
            ReportError(cx, LOAD_ERROR_URI_NOT_LOCAL, uri);
            return NS_OK;
        }

        // For file URIs prepend the filename with the filename of the
        // calling script, and " -> ". See bug 418356.
        nsAutoCString tmp(filename.get());
        tmp.AppendLiteral(" -> ");
        tmp.Append(uriStr);

        uriStr = tmp;
    }

    // Suppress caching if we're compiling as content or if we're loading a
    // blob: URI.
    bool ignoreCache = options.ignoreCache
        || !GetObjectPrincipal(targetObj)->GetIsSystemPrincipal()
        || scheme.EqualsLiteral("blob");
    StartupCache* cache = ignoreCache ? nullptr : StartupCache::GetSingleton();

    nsAutoCString cachePath;
    SubscriptCachePath(cx, uri, targetObj, cachePath);

    RootedScript script(cx);
    if (!options.ignoreCache) {
        if (!options.wantReturnValue)
            script = ScriptPreloader::GetSingleton().GetCachedScript(cx, cachePath);
        if (!script && cache)
            rv = ReadCachedScript(cache, cachePath, cx, &script);
        if (NS_FAILED(rv) || !script) {
            // ReadCachedScript may have set a pending exception.
            JS_ClearPendingException(cx);
        }
    }

    // If we are doing an async load, trigger it and bail out.
    if (!script && options.async) {
        return ReadScriptAsync(uri, targetObj, loadScope, options.charset, serv,
                               options.wantReturnValue, !!cache, retval);
    }

    if (script) {
        // |script| came from the cache, so don't bother writing it
        // |back there.
        cache = nullptr;
    } else if (!ReadScript(uri, cx, targetObj, options.charset,
                        static_cast<const char*>(uriStr.get()), serv,
                        options.wantReturnValue, &script)) {
        return NS_OK;
    }

    Unused << EvalScript(cx, targetObj, loadScope, retval, uri, !!cache,
                         !ignoreCache && !options.wantReturnValue,
                         &script);
    return NS_OK;
}

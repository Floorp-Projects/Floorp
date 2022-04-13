/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozJSSubScriptLoader.h"
#include "js/experimental/JSStencil.h"
#include "mozJSComponentLoader.h"
#include "mozJSLoaderUtils.h"

#include "nsIURI.h"
#include "nsIIOService.h"
#include "nsIChannel.h"
#include "nsIInputStream.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"

#include "jsapi.h"
#include "jsfriendapi.h"
#include "xpcprivate.h"                   // xpc::OptionsBase
#include "js/CompilationAndEvaluation.h"  // JS::Compile
#include "js/CompileOptions.h"  // JS::ReadOnlyCompileOptions, JS::DecodeOptions
#include "js/friend/JSMEnvironment.h"  // JS::ExecuteInJSMEnvironment, JS::IsJSMEnvironment
#include "js/SourceText.h"             // JS::Source{Ownership,Text}
#include "js/Wrapper.h"

#include "mozilla/ContentPrincipal.h"
#include "mozilla/dom/ScriptLoader.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/ScriptPreloader.h"
#include "mozilla/SystemPrincipal.h"
#include "mozilla/scache/StartupCache.h"
#include "mozilla/scache/StartupCacheUtils.h"
#include "mozilla/Unused.h"
#include "mozilla/Utf8.h"  // mozilla::Utf8Unit
#include "nsContentUtils.h"
#include "nsString.h"

using namespace mozilla::scache;
using namespace JS;
using namespace xpc;
using namespace mozilla;
using namespace mozilla::dom;

class MOZ_STACK_CLASS LoadSubScriptOptions : public OptionsBase {
 public:
  explicit LoadSubScriptOptions(JSContext* cx = xpc_GetSafeJSContext(),
                                JSObject* options = nullptr)
      : OptionsBase(cx, options),
        target(cx),
        ignoreCache(false),
        wantReturnValue(false) {}

  virtual bool Parse() override {
    return ParseObject("target", &target) &&
           ParseBoolean("ignoreCache", &ignoreCache) &&
           ParseBoolean("wantReturnValue", &wantReturnValue);
  }

  RootedObject target;
  bool ignoreCache;
  bool wantReturnValue;
};

/* load() error msgs, XXX localize? */
#define LOAD_ERROR_NOSERVICE "Error creating IO Service."
#define LOAD_ERROR_NOURI "Error creating URI (invalid URL scheme?)"
#define LOAD_ERROR_NOSCHEME "Failed to get URI scheme.  This is bad."
#define LOAD_ERROR_URI_NOT_LOCAL "Trying to load a non-local URI."
#define LOAD_ERROR_NOSTREAM "Error opening input stream (invalid filename?)"
#define LOAD_ERROR_NOCONTENT "ContentLength not available (not a local URL?)"
#define LOAD_ERROR_BADCHARSET "Error converting to specified charset"
#define LOAD_ERROR_NOSPEC "Failed to get URI spec.  This is bad."
#define LOAD_ERROR_CONTENTTOOBIG "ContentLength is too large"

mozJSSubScriptLoader::mozJSSubScriptLoader() = default;

mozJSSubScriptLoader::~mozJSSubScriptLoader() = default;

NS_IMPL_ISUPPORTS(mozJSSubScriptLoader, mozIJSSubScriptLoader)

#define JSSUB_CACHE_PREFIX(aScopeType, aCompilationTarget) \
  "jssubloader/" aScopeType "/" aCompilationTarget

static void SubscriptCachePath(JSContext* cx, nsIURI* uri,
                               JS::HandleObject targetObj,
                               nsACString& cachePath) {
  // StartupCache must distinguish between non-syntactic vs global when
  // computing the cache key.
  if (!JS_IsGlobalObject(targetObj)) {
    PathifyURI(JSSUB_CACHE_PREFIX("non-syntactic", "script"), uri, cachePath);
  } else {
    PathifyURI(JSSUB_CACHE_PREFIX("global", "script"), uri, cachePath);
  }
}

static void ReportError(JSContext* cx, const nsACString& msg) {
  NS_ConvertUTF8toUTF16 ucMsg(msg);

  RootedValue exn(cx);
  if (xpc::NonVoidStringToJsval(cx, ucMsg, &exn)) {
    JS_SetPendingException(cx, exn);
  }
}

static void ReportError(JSContext* cx, const char* origMsg, nsIURI* uri) {
  if (!uri) {
    ReportError(cx, nsDependentCString(origMsg));
    return;
  }

  nsAutoCString spec;
  nsresult rv = uri->GetSpec(spec);
  if (NS_FAILED(rv)) {
    spec.AssignLiteral("(unknown)");
  }

  nsAutoCString msg(origMsg);
  msg.AppendLiteral(": ");
  msg.Append(spec);
  ReportError(cx, msg);
}

static bool EvalStencil(JSContext* cx, HandleObject targetObj,
                        HandleObject loadScope, MutableHandleValue retval,
                        nsIURI* uri, bool storeIntoStartupCache,
                        bool storeIntoPreloadCache, JS::Stencil* stencil) {
  MOZ_ASSERT(!js::IsWrapper(targetObj));

  JS::InstantiateOptions options;
  JS::RootedScript script(cx,
                          JS::InstantiateGlobalStencil(cx, options, stencil));
  if (!script) {
    return false;
  }

  if (JS_IsGlobalObject(targetObj)) {
    if (!JS_ExecuteScript(cx, script, retval)) {
      return false;
    }
  } else if (JS::IsJSMEnvironment(targetObj)) {
    if (!JS::ExecuteInJSMEnvironment(cx, script, targetObj)) {
      return false;
    }
    retval.setUndefined();
  } else {
    JS::RootedObjectVector envChain(cx);
    if (!envChain.append(targetObj)) {
      return false;
    }
    if (!loadScope) {
      // A null loadScope means we are cross-realm. In this case, we should
      // check the target isn't in the JSM loader shared-global or we will
      // contaminate all JSMs in the realm.
      //
      // NOTE: If loadScope is already a shared-global JSM, we can't
      // determine which JSM the target belongs to and have to assume it
      // is in our JSM.
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
      JSObject* targetGlobal = JS::GetNonCCWObjectGlobal(targetObj);
      MOZ_DIAGNOSTIC_ASSERT(
          !mozJSComponentLoader::Get()->IsLoaderGlobal(targetGlobal),
          "Don't load subscript into target in a shared-global JSM");
#endif
      if (!JS_ExecuteScript(cx, envChain, script, retval)) {
        return false;
      }
    } else if (JS_IsGlobalObject(loadScope)) {
      if (!JS_ExecuteScript(cx, envChain, script, retval)) {
        return false;
      }
    } else {
      MOZ_ASSERT(JS::IsJSMEnvironment(loadScope));
      if (!JS::ExecuteInJSMEnvironment(cx, script, loadScope, envChain)) {
        return false;
      }
      retval.setUndefined();
    }
  }

  JSAutoRealm rar(cx, targetObj);
  if (!JS_WrapValue(cx, retval)) {
    return false;
  }

  if (script && (storeIntoStartupCache || storeIntoPreloadCache)) {
    nsAutoCString cachePath;
    SubscriptCachePath(cx, uri, targetObj, cachePath);

    nsCString uriStr;
    if (storeIntoPreloadCache && NS_SUCCEEDED(uri->GetSpec(uriStr))) {
      ScriptPreloader::GetSingleton().NoteStencil(uriStr, cachePath, stencil);
    }

    if (storeIntoStartupCache) {
      JSAutoRealm ar(cx, script);
      WriteCachedStencil(StartupCache::GetSingleton(), cachePath, cx, stencil);
    }
  }

  return true;
}

bool mozJSSubScriptLoader::ReadStencil(
    JS::Stencil** stencilOut, nsIURI* uri, JSContext* cx,
    const JS::ReadOnlyCompileOptions& options, nsIIOService* serv,
    bool useCompilationScope) {
  // We create a channel and call SetContentType, to avoid expensive MIME type
  // lookups (bug 632490).
  nsCOMPtr<nsIChannel> chan;
  nsCOMPtr<nsIInputStream> instream;
  nsresult rv;
  rv = NS_NewChannel(getter_AddRefs(chan), uri,
                     nsContentUtils::GetSystemPrincipal(),
                     nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
                     nsIContentPolicy::TYPE_OTHER,
                     nullptr,  // nsICookieJarSettings
                     nullptr,  // PerformanceStorage
                     nullptr,  // aLoadGroup
                     nullptr,  // aCallbacks
                     nsIRequest::LOAD_NORMAL, serv);

  if (NS_SUCCEEDED(rv)) {
    chan->SetContentType("application/javascript"_ns);
    rv = chan->Open(getter_AddRefs(instream));
  }

  if (NS_FAILED(rv)) {
    ReportError(cx, LOAD_ERROR_NOSTREAM, uri);
    return false;
  }

  int64_t len = -1;

  rv = chan->GetContentLength(&len);
  if (NS_FAILED(rv)) {
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

  if (len < 0) {
    len = buf.Length();
  }

  Maybe<JSAutoRealm> ar;

  // Note that when using the ScriptPreloader cache with loadSubScript, there
  // will be a side-effect of keeping the global that the script was compiled
  // for alive. See note above in EvalScript().
  //
  // This will compile the script in XPConnect compilation scope. When the
  // script is evaluated, it will be cloned into the target scope to be
  // executed, avoiding leaks on the first session when we don't have a
  // startup cache.
  if (useCompilationScope) {
    ar.emplace(cx, xpc::CompilationScope());
  }

  JS::SourceText<Utf8Unit> srcBuf;
  if (!srcBuf.init(cx, buf.get(), len, JS::SourceOwnership::Borrowed)) {
    return false;
  }

  RefPtr<JS::Stencil> stencil =
      JS::CompileGlobalScriptToStencil(cx, options, srcBuf);
  stencil.forget(stencilOut);
  return *stencilOut;
}

NS_IMETHODIMP
mozJSSubScriptLoader::LoadSubScript(const nsAString& url, HandleValue target,
                                    JSContext* cx, MutableHandleValue retval) {
  /*
   * Loads a local url, referring to UTF-8-encoded data, and evals it into the
   * current cx.  Synchronous. ChromeUtils.compileScript() should be used for
   * async loads.
   *   url: The url to load.  Must be local so that it can be loaded
   *        synchronously.
   *   targetObj: Optional object to eval the script onto (defaults to context
   *              global)
   *   returns: Whatever jsval the script pointed to by the url returns.
   * Should ONLY (O N L Y !) be called from JavaScript code.
   */
  LoadSubScriptOptions options(cx);
  options.target = target.isObject() ? &target.toObject() : nullptr;
  return DoLoadSubScriptWithOptions(url, options, cx, retval);
}

NS_IMETHODIMP
mozJSSubScriptLoader::LoadSubScriptWithOptions(const nsAString& url,
                                               HandleValue optionsVal,
                                               JSContext* cx,
                                               MutableHandleValue retval) {
  if (!optionsVal.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }

  LoadSubScriptOptions options(cx, &optionsVal.toObject());
  if (!options.Parse()) {
    return NS_ERROR_INVALID_ARG;
  }

  return DoLoadSubScriptWithOptions(url, options, cx, retval);
}

nsresult mozJSSubScriptLoader::DoLoadSubScriptWithOptions(
    const nsAString& url, LoadSubScriptOptions& options, JSContext* cx,
    MutableHandleValue retval) {
  nsresult rv = NS_OK;
  RootedObject targetObj(cx);
  RootedObject loadScope(cx);
  mozJSComponentLoader* loader = mozJSComponentLoader::Get();
  loader->FindTargetObject(cx, &loadScope);

  if (options.target) {
    targetObj = options.target;
  } else {
    targetObj = loadScope;
  }

  targetObj = JS_FindCompilationScope(cx, targetObj);
  if (!targetObj || !loadScope) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(!js::IsWrapper(targetObj), "JS_FindCompilationScope must unwrap");

  if (js::GetNonCCWObjectRealm(loadScope) !=
      js::GetNonCCWObjectRealm(targetObj)) {
    loadScope = nullptr;
  }

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

  JSAutoRealm ar(cx, targetObj);

  nsCOMPtr<nsIIOService> serv = do_GetService(NS_IOSERVICE_CONTRACTID);
  if (!serv) {
    ReportError(cx, nsLiteralCString(LOAD_ERROR_NOSERVICE));
    return NS_OK;
  }

  NS_LossyConvertUTF16toASCII asciiUrl(url);
  AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING_NONSENSITIVE(
      "mozJSSubScriptLoader::DoLoadSubScriptWithOptions", OTHER, asciiUrl);
  AUTO_PROFILER_MARKER_TEXT("SubScript", JS,
                            MarkerOptions(MarkerStack::Capture(),
                                          MarkerInnerWindowIdFromJSContext(cx)),
                            asciiUrl);

  // Make sure to explicitly create the URI, since we'll need the
  // canonicalized spec.
  rv = NS_NewURI(getter_AddRefs(uri), asciiUrl);
  if (NS_FAILED(rv)) {
    ReportError(cx, nsLiteralCString(LOAD_ERROR_NOURI));
    return NS_OK;
  }

  rv = uri->GetSpec(uriStr);
  if (NS_FAILED(rv)) {
    ReportError(cx, nsLiteralCString(LOAD_ERROR_NOSPEC));
    return NS_OK;
  }

  rv = uri->GetScheme(scheme);
  if (NS_FAILED(rv)) {
    ReportError(cx, LOAD_ERROR_NOSCHEME, uri);
    return NS_OK;
  }

  // Suppress caching if we're compiling as content or if we're loading a
  // blob: URI.
  bool useCompilationScope = false;
  auto* principal = BasePrincipal::Cast(GetObjectPrincipal(targetObj));
  bool isSystem = principal->Is<SystemPrincipal>();
  if (!isSystem && principal->Is<ContentPrincipal>()) {
    nsAutoCString scheme;
    principal->GetScheme(scheme);

    // We want to enable caching for scripts with Activity Stream's
    // codebase URLs.
    if (scheme.EqualsLiteral("about")) {
      nsAutoCString filePath;
      principal->GetFilePath(filePath);

      useCompilationScope = filePath.EqualsLiteral("home") ||
                            filePath.EqualsLiteral("newtab") ||
                            filePath.EqualsLiteral("welcome");
      isSystem = true;
    }
  }
  bool ignoreCache =
      options.ignoreCache || !isSystem || scheme.EqualsLiteral("blob");

  StartupCache* cache = ignoreCache ? nullptr : StartupCache::GetSingleton();

  nsAutoCString cachePath;
  SubscriptCachePath(cx, uri, targetObj, cachePath);

  JS::DecodeOptions decodeOptions;
  ScriptPreloader::FillDecodeOptionsForCachedStencil(decodeOptions);

  RefPtr<JS::Stencil> stencil;
  if (!options.ignoreCache) {
    if (!options.wantReturnValue) {
      // NOTE: If we need the return value, we cannot use ScriptPreloader.
      stencil = ScriptPreloader::GetSingleton().GetCachedStencil(
          cx, decodeOptions, cachePath);
    }
    if (!stencil && cache) {
      rv = ReadCachedStencil(cache, cachePath, cx, decodeOptions,
                             getter_AddRefs(stencil));
      if (NS_FAILED(rv) || !stencil) {
        JS_ClearPendingException(cx);
      }
    }
  }

  bool storeIntoStartupCache = false;
  if (!stencil) {
    // Store into startup cache only when the script isn't come from any cache.
    storeIntoStartupCache = cache;

    JS::CompileOptions compileOptions(cx);
    ScriptPreloader::FillCompileOptionsForCachedStencil(compileOptions);
    compileOptions.setFileAndLine(uriStr.get(), 1);
    compileOptions.setNonSyntacticScope(!JS_IsGlobalObject(targetObj));

    if (options.wantReturnValue) {
      compileOptions.setNoScriptRval(false);
    }

    if (!ReadStencil(getter_AddRefs(stencil), uri, cx, compileOptions, serv,
                     useCompilationScope)) {
      return NS_OK;
    }

#ifdef DEBUG
    // The above shouldn't touch any options for instantiation.
    JS::InstantiateOptions instantiateOptions(compileOptions);
    instantiateOptions.assertDefault();
#endif
  }

  // As a policy choice, we don't store scripts that want return values
  // into the preload cache.
  bool storeIntoPreloadCache = !ignoreCache && !options.wantReturnValue;

  Unused << EvalStencil(cx, targetObj, loadScope, retval, uri,
                        storeIntoStartupCache, storeIntoPreloadCache, stencil);
  return NS_OK;
}

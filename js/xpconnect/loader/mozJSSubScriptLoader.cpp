/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

#include "jsapi.h"
#include "jsfriendapi.h"
#include "xpcprivate.h"                   // xpc::OptionsBase
#include "js/CompilationAndEvaluation.h"  // JS::Compile{,ForNonSyntacticScope}
#include "js/SourceText.h"                // JS::Source{Ownership,Text}
#include "js/Wrapper.h"

#include "mozilla/ContentPrincipal.h"
#include "mozilla/dom/ScriptLoader.h"
#include "mozilla/ScriptPreloader.h"
#include "mozilla/SystemPrincipal.h"
#include "mozilla/scache/StartupCache.h"
#include "mozilla/scache/StartupCacheUtils.h"
#include "mozilla/Unused.h"
#include "mozilla/Utf8.h"  // mozilla::Utf8Unit
#include "nsContentUtils.h"
#include "nsString.h"
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
      : OptionsBase(cx, options),
        target(cx),
        ignoreCache(false),
        useCompilationScope(false),
        wantReturnValue(false) {}

  virtual bool Parse() override {
    return ParseObject("target", &target) &&
           ParseBoolean("ignoreCache", &ignoreCache) &&
           ParseBoolean("useCompilationScope", &useCompilationScope) &&
           ParseBoolean("wantReturnValue", &wantReturnValue);
  }

  RootedObject target;
  bool ignoreCache;
  bool useCompilationScope;
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

#define JSSUB_CACHE_PREFIX(aType) "jssubloader/" aType

static void SubscriptCachePath(JSContext* cx, nsIURI* uri,
                               JS::HandleObject targetObj,
                               nsACString& cachePath) {
  // StartupCache must distinguish between non-syntactic vs global when
  // computing the cache key.
  if (!JS_IsGlobalObject(targetObj)) {
    cachePath.AssignLiteral(JSSUB_CACHE_PREFIX("non-syntactic"));
  } else {
    cachePath.AssignLiteral(JSSUB_CACHE_PREFIX("global"));
  }
  PathifyURI(uri, cachePath);
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

static JSScript* PrepareScript(nsIURI* uri, JSContext* cx,
                               bool wantGlobalScript, const char* uriStr,
                               const char* buf, int64_t len,
                               bool wantReturnValue) {
  JS::CompileOptions options(cx);
  options.setFileAndLine(uriStr, 1).setNoScriptRval(!wantReturnValue);

  // This presumes that no one else might be compiling a script for this
  // (URL, syntactic-or-not) key *not* using UTF-8.  Seeing as JS source can
  // only be compiled as UTF-8 or UTF-16 now -- there isn't a JSAPI function to
  // compile Latin-1 now -- this presumption seems relatively safe.
  //
  // This also presumes that lazy parsing is disabled, for the sake of the
  // startup cache.  If lazy parsing is ever enabled for pertinent scripts that
  // pass through here, we may need to disable lazy source for them.
  options.setSourceIsLazy(true);

  JS::SourceText<Utf8Unit> srcBuf;
  if (!srcBuf.init(cx, buf, len, JS::SourceOwnership::Borrowed)) {
    return nullptr;
  }

  if (wantGlobalScript) {
    return JS::Compile(cx, options, srcBuf);
  }
  return JS::CompileForNonSyntacticScope(cx, options, srcBuf);
}

static bool EvalScript(JSContext* cx, HandleObject targetObj,
                       HandleObject loadScope, MutableHandleValue retval,
                       nsIURI* uri, bool startupCache, bool preloadCache,
                       MutableHandleScript script) {
  MOZ_ASSERT(!js::IsWrapper(targetObj));

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

  JSAutoRealm rar(cx, targetObj);
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
      // nuked but held alive by the JSScript. We can avoid this problem
      // by using a different scope when compiling the script. See
      // useCompilationScope in ReadScript().
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
      JSAutoRealm ar(cx, script);
      WriteCachedScript(StartupCache::GetSingleton(), cachePath, cx, script);
    }
  }

  return true;
}

bool mozJSSubScriptLoader::ReadScript(JS::MutableHandle<JSScript*> script,
                                      nsIURI* uri, JSContext* cx,
                                      HandleObject targetObj,
                                      const char* uriStr, nsIIOService* serv,
                                      bool wantReturnValue,
                                      bool useCompilationScope) {
  // We create a channel and call SetContentType, to avoid expensive MIME type
  // lookups (bug 632490).
  nsCOMPtr<nsIChannel> chan;
  nsCOMPtr<nsIInputStream> instream;
  nsresult rv;
  rv = NS_NewChannel(getter_AddRefs(chan), uri,
                     nsContentUtils::GetSystemPrincipal(),
                     nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                     nsIContentPolicy::TYPE_OTHER,
                     nullptr,  // nsICookieJarSettings
                     nullptr,  // PerformanceStorage
                     nullptr,  // aLoadGroup
                     nullptr,  // aCallbacks
                     nsIRequest::LOAD_NORMAL, serv);

  if (NS_SUCCEEDED(rv)) {
    chan->SetContentType(NS_LITERAL_CSTRING("application/javascript"));
    rv = chan->Open(getter_AddRefs(instream));
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

  JSScript* ret = PrepareScript(uri, cx, JS_IsGlobalObject(targetObj), uriStr,
                                buf.get(), len, wantReturnValue);
  if (!ret) {
    return false;
  }

  script.set(ret);
  return true;
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
    ReportError(cx, NS_LITERAL_CSTRING(LOAD_ERROR_NOSERVICE));
    return NS_OK;
  }

  NS_LossyConvertUTF16toASCII asciiUrl(url);
  AUTO_PROFILER_TEXT_MARKER_CAUSE("SubScript", asciiUrl, JS, Nothing(),
                                  profiler_get_backtrace());
  AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING_NONSENSITIVE(
      "mozJSSubScriptLoader::DoLoadSubScriptWithOptions", OTHER, asciiUrl);

  // Make sure to explicitly create the URI, since we'll need the
  // canonicalized spec.
  rv = NS_NewURI(getter_AddRefs(uri), asciiUrl);
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

  // Suppress caching if we're compiling as content or if we're loading a
  // blob: URI.
  bool useCompilationScope = false;
  auto* principal = BasePrincipal::Cast(GetObjectPrincipal(targetObj));
  bool isSystem = principal->Is<SystemPrincipal>();
  if (!isSystem && principal->Is<ContentPrincipal>()) {
    auto* content = principal->As<ContentPrincipal>();

    nsAutoCString scheme;
    content->mURI->GetScheme(scheme);

    // We want to enable caching for scripts with Activity Stream's
    // codebase URLs.
    if (scheme.EqualsLiteral("about")) {
      nsAutoCString filePath;
      content->mURI->GetFilePath(filePath);

      useCompilationScope = filePath.EqualsLiteral("home") ||
                            filePath.EqualsLiteral("newtab") ||
                            filePath.EqualsLiteral("welcome");
      isSystem = true;
    }
  }
  if (options.useCompilationScope) {
    useCompilationScope = true;
  }
  bool ignoreCache =
      options.ignoreCache || !isSystem || scheme.EqualsLiteral("blob");

  StartupCache* cache = ignoreCache ? nullptr : StartupCache::GetSingleton();

  nsAutoCString cachePath;
  SubscriptCachePath(cx, uri, targetObj, cachePath);

  RootedScript script(cx);
  if (!options.ignoreCache) {
    if (!options.wantReturnValue) {
      script = ScriptPreloader::GetSingleton().GetCachedScript(cx, cachePath);
    }
    if (!script && cache) {
      rv = ReadCachedScript(cache, cachePath, cx, &script);
    }
    if (NS_FAILED(rv) || !script) {
      // ReadCachedScript may have set a pending exception.
      JS_ClearPendingException(cx);
    }
  }

  if (script) {
    // |script| came from the cache, so don't bother writing it
    // |back there.
    cache = nullptr;
  } else {
    if (!ReadScript(&script, uri, cx, targetObj,
                    static_cast<const char*>(uriStr.get()), serv,
                    options.wantReturnValue, useCompilationScope)) {
      return NS_OK;
    }
  }

  Unused << EvalScript(cx, targetObj, loadScope, retval, uri, !!cache,
                       !ignoreCache && !options.wantReturnValue, &script);
  return NS_OK;
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromeUtils.h"

#include "js/CharacterEncoding.h"
#include "js/SavedFrameAPI.h"
#include "jsfriendapi.h"
#include "WrapperFactory.h"

#include "mozilla/Base64.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/IntentionalCrash.h"
#include "mozilla/PerformanceMetricsCollector.h"
#include "mozilla/PerfStats.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProcInfo.h"
#include "mozilla/RDDProcessManager.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/IdleDeadline.h"
#include "mozilla/dom/JSWindowActorService.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ReportingHeader.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/dom/WindowBinding.h"  // For IdleRequestCallback/Options
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "mozilla/net/UrlClassifierFeatureFactory.h"
#include "mozilla/net/SocketProcessHost.h"
#include "IOActivityMonitor.h"
#include "nsIOService.h"
#include "nsThreadUtils.h"
#include "mozJSComponentLoader.h"
#include "GeckoProfiler.h"
#include "nsIException.h"

namespace mozilla {
namespace dom {

/* static */
void ChromeUtils::NondeterministicGetWeakMapKeys(
    GlobalObject& aGlobal, JS::Handle<JS::Value> aMap,
    JS::MutableHandle<JS::Value> aRetval, ErrorResult& aRv) {
  if (!aMap.isObject()) {
    aRetval.setUndefined();
  } else {
    JSContext* cx = aGlobal.Context();
    JS::Rooted<JSObject*> objRet(cx);
    JS::Rooted<JSObject*> mapObj(cx, &aMap.toObject());
    if (!JS_NondeterministicGetWeakMapKeys(cx, mapObj, &objRet)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    } else {
      aRetval.set(objRet ? JS::ObjectValue(*objRet) : JS::UndefinedValue());
    }
  }
}

/* static */
void ChromeUtils::NondeterministicGetWeakSetKeys(
    GlobalObject& aGlobal, JS::Handle<JS::Value> aSet,
    JS::MutableHandle<JS::Value> aRetval, ErrorResult& aRv) {
  if (!aSet.isObject()) {
    aRetval.setUndefined();
  } else {
    JSContext* cx = aGlobal.Context();
    JS::Rooted<JSObject*> objRet(cx);
    JS::Rooted<JSObject*> setObj(cx, &aSet.toObject());
    if (!JS_NondeterministicGetWeakSetKeys(cx, setObj, &objRet)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    } else {
      aRetval.set(objRet ? JS::ObjectValue(*objRet) : JS::UndefinedValue());
    }
  }
}

/* static */
void ChromeUtils::Base64URLEncode(GlobalObject& aGlobal,
                                  const ArrayBufferViewOrArrayBuffer& aSource,
                                  const Base64URLEncodeOptions& aOptions,
                                  nsACString& aResult, ErrorResult& aRv) {
  size_t length = 0;
  uint8_t* data = nullptr;
  if (aSource.IsArrayBuffer()) {
    const ArrayBuffer& buffer = aSource.GetAsArrayBuffer();
    buffer.ComputeLengthAndData();
    length = buffer.Length();
    data = buffer.Data();
  } else if (aSource.IsArrayBufferView()) {
    const ArrayBufferView& view = aSource.GetAsArrayBufferView();
    view.ComputeLengthAndData();
    length = view.Length();
    data = view.Data();
  } else {
    MOZ_CRASH("Uninitialized union: expected buffer or view");
  }

  auto paddingPolicy = aOptions.mPad ? Base64URLEncodePaddingPolicy::Include
                                     : Base64URLEncodePaddingPolicy::Omit;
  nsresult rv = mozilla::Base64URLEncode(length, data, paddingPolicy, aResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aResult.Truncate();
    aRv.Throw(rv);
  }
}

/* static */
void ChromeUtils::Base64URLDecode(GlobalObject& aGlobal,
                                  const nsACString& aString,
                                  const Base64URLDecodeOptions& aOptions,
                                  JS::MutableHandle<JSObject*> aRetval,
                                  ErrorResult& aRv) {
  Base64URLDecodePaddingPolicy paddingPolicy;
  switch (aOptions.mPadding) {
    case Base64URLDecodePadding::Require:
      paddingPolicy = Base64URLDecodePaddingPolicy::Require;
      break;

    case Base64URLDecodePadding::Ignore:
      paddingPolicy = Base64URLDecodePaddingPolicy::Ignore;
      break;

    case Base64URLDecodePadding::Reject:
      paddingPolicy = Base64URLDecodePaddingPolicy::Reject;
      break;

    default:
      aRv.Throw(NS_ERROR_INVALID_ARG);
      return;
  }
  FallibleTArray<uint8_t> data;
  nsresult rv = mozilla::Base64URLDecode(aString, paddingPolicy, data);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  JS::Rooted<JSObject*> buffer(
      aGlobal.Context(),
      ArrayBuffer::Create(aGlobal.Context(), data.Length(), data.Elements()));
  if (NS_WARN_IF(!buffer)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  aRetval.set(buffer);
}

/* static */
void ChromeUtils::ReleaseAssert(GlobalObject& aGlobal, bool aCondition,
                                const nsAString& aMessage) {
  // If the condition didn't fail, which is the likely case, immediately return.
  if (MOZ_LIKELY(aCondition)) {
    return;
  }

  // Extract the current stack from the JS runtime to embed in the crash reason.
  nsAutoString filename;
  uint32_t lineNo = 0;

  if (nsCOMPtr<nsIStackFrame> location = GetCurrentJSStack(1)) {
    location->GetFilename(aGlobal.Context(), filename);
    lineNo = location->GetLineNumber(aGlobal.Context());
  } else {
    filename.Assign(NS_LITERAL_STRING("<unknown>"));
  }

  // Convert to utf-8 for adding as the MozCrashReason.
  NS_ConvertUTF16toUTF8 filenameUtf8(filename);
  NS_ConvertUTF16toUTF8 messageUtf8(aMessage);

  // Actually crash.
  MOZ_CRASH_UNSAFE_PRINTF("Failed ChromeUtils.releaseAssert(\"%s\") @ %s:%u",
                          messageUtf8.get(), filenameUtf8.get(), lineNo);
}

/* static */
void ChromeUtils::WaiveXrays(GlobalObject& aGlobal, JS::HandleValue aVal,
                             JS::MutableHandleValue aRetval, ErrorResult& aRv) {
  JS::RootedValue value(aGlobal.Context(), aVal);
  if (!xpc::WrapperFactory::WaiveXrayAndWrap(aGlobal.Context(), &value)) {
    aRv.NoteJSContextException(aGlobal.Context());
  } else {
    aRetval.set(value);
  }
}

/* static */
void ChromeUtils::UnwaiveXrays(GlobalObject& aGlobal, JS::HandleValue aVal,
                               JS::MutableHandleValue aRetval,
                               ErrorResult& aRv) {
  if (!aVal.isObject()) {
    aRetval.set(aVal);
    return;
  }

  JS::RootedObject obj(aGlobal.Context(),
                       js::UncheckedUnwrap(&aVal.toObject()));
  if (!JS_WrapObject(aGlobal.Context(), &obj)) {
    aRv.NoteJSContextException(aGlobal.Context());
  } else {
    aRetval.setObject(*obj);
  }
}

/* static */
void ChromeUtils::GetClassName(GlobalObject& aGlobal, JS::HandleObject aObj,
                               bool aUnwrap, nsAString& aRetval) {
  JS::RootedObject obj(aGlobal.Context(), aObj);
  if (aUnwrap) {
    obj = js::UncheckedUnwrap(obj, /* stopAtWindowProxy = */ false);
  }

  aRetval =
      NS_ConvertUTF8toUTF16(nsDependentCString(js::GetObjectClass(obj)->name));
}

/* static */
void ChromeUtils::ShallowClone(GlobalObject& aGlobal, JS::HandleObject aObj,
                               JS::HandleObject aTarget,
                               JS::MutableHandleObject aRetval,
                               ErrorResult& aRv) {
  JSContext* cx = aGlobal.Context();

  auto cleanup = MakeScopeExit([&]() { aRv.NoteJSContextException(cx); });

  JS::Rooted<JS::IdVector> ids(cx, JS::IdVector(cx));
  JS::RootedVector<JS::Value> values(cx);
  JS::RootedVector<jsid> valuesIds(cx);

  {
    // cx represents our current Realm, so it makes sense to use it for the
    // CheckedUnwrapDynamic call.  We do want CheckedUnwrapDynamic, in case
    // someone is shallow-cloning a Window.
    JS::RootedObject obj(cx, js::CheckedUnwrapDynamic(aObj, cx));
    if (!obj) {
      js::ReportAccessDenied(cx);
      return;
    }

    if (js::IsScriptedProxy(obj)) {
      JS_ReportErrorASCII(cx, "Shallow cloning a proxy object is not allowed");
      return;
    }

    JSAutoRealm ar(cx, obj);

    if (!JS_Enumerate(cx, obj, &ids) || !values.reserve(ids.length()) ||
        !valuesIds.reserve(ids.length())) {
      return;
    }

    JS::Rooted<JS::PropertyDescriptor> desc(cx);
    JS::RootedId id(cx);
    for (jsid idVal : ids) {
      id = idVal;
      if (!JS_GetOwnPropertyDescriptorById(cx, obj, id, &desc)) {
        continue;
      }
      if (desc.setter() || desc.getter()) {
        continue;
      }
      valuesIds.infallibleAppend(id);
      values.infallibleAppend(desc.value());
    }
  }

  JS::RootedObject obj(cx);
  {
    Maybe<JSAutoRealm> ar;
    if (aTarget) {
      // Our target could be anything, so we want CheckedUnwrapDynamic here.
      // "cx" represents the current Realm when we were called from bindings, so
      // we can just use that.
      JS::RootedObject target(cx, js::CheckedUnwrapDynamic(aTarget, cx));
      if (!target) {
        js::ReportAccessDenied(cx);
        return;
      }
      ar.emplace(cx, target);
    }

    obj = JS_NewPlainObject(cx);
    if (!obj) {
      return;
    }

    JS::RootedValue value(cx);
    JS::RootedId id(cx);
    for (uint32_t i = 0; i < valuesIds.length(); i++) {
      id = valuesIds[i];
      value = values[i];

      JS_MarkCrossZoneId(cx, id);
      if (!JS_WrapValue(cx, &value) ||
          !JS_SetPropertyById(cx, obj, id, value)) {
        return;
      }
    }
  }

  if (aTarget && !JS_WrapObject(cx, &obj)) {
    return;
  }

  cleanup.release();
  aRetval.set(obj);
}

namespace {
class IdleDispatchRunnable final : public IdleRunnable,
                                   public nsITimerCallback {
 public:
  NS_DECL_ISUPPORTS_INHERITED

  IdleDispatchRunnable(nsIGlobalObject* aParent, IdleRequestCallback& aCallback)
      : IdleRunnable("ChromeUtils::IdleDispatch"),
        mCallback(&aCallback),
        mParent(aParent) {}

  // MOZ_CAN_RUN_SCRIPT_BOUNDARY until Runnable::Run is MOZ_CAN_RUN_SCRIPT.
  // See bug 1535398.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHOD Run() override {
    if (mCallback) {
      CancelTimer();

      auto deadline = mDeadline - TimeStamp::ProcessCreation();

      ErrorResult rv;
      RefPtr<IdleDeadline> idleDeadline =
          new IdleDeadline(mParent, mTimedOut, deadline.ToMilliseconds());

      RefPtr<IdleRequestCallback> callback(mCallback.forget());
      MOZ_ASSERT(!mCallback);
      callback->Call(*idleDeadline, "ChromeUtils::IdleDispatch handler");
      mParent = nullptr;
    }
    return NS_OK;
  }

  void SetDeadline(TimeStamp aDeadline) override { mDeadline = aDeadline; }

  NS_IMETHOD Notify(nsITimer* aTimer) override {
    mTimedOut = true;
    SetDeadline(TimeStamp::Now());
    return Run();
  }

  void SetTimer(uint32_t aDelay, nsIEventTarget* aTarget) override {
    MOZ_ASSERT(aTarget);
    MOZ_ASSERT(!mTimer);
    NS_NewTimerWithCallback(getter_AddRefs(mTimer), this, aDelay,
                            nsITimer::TYPE_ONE_SHOT, aTarget);
  }

 protected:
  virtual ~IdleDispatchRunnable() { CancelTimer(); }

 private:
  void CancelTimer() {
    if (mTimer) {
      mTimer->Cancel();
      mTimer = nullptr;
    }
  }

  RefPtr<IdleRequestCallback> mCallback;
  nsCOMPtr<nsIGlobalObject> mParent;

  nsCOMPtr<nsITimer> mTimer;

  TimeStamp mDeadline{};
  bool mTimedOut = false;
};

NS_IMPL_ISUPPORTS_INHERITED(IdleDispatchRunnable, IdleRunnable,
                            nsITimerCallback)
}  // anonymous namespace

/* static */
void ChromeUtils::IdleDispatch(const GlobalObject& aGlobal,
                               IdleRequestCallback& aCallback,
                               const IdleRequestOptions& aOptions,
                               ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  MOZ_ASSERT(global);

  auto runnable = MakeRefPtr<IdleDispatchRunnable>(global, aCallback);

  if (aOptions.mTimeout.WasPassed()) {
    aRv = NS_DispatchToCurrentThreadQueue(
        runnable.forget(), aOptions.mTimeout.Value(), EventQueuePriority::Idle);
  } else {
    aRv = NS_DispatchToCurrentThreadQueue(runnable.forget(),
                                          EventQueuePriority::Idle);
  }
}

/* static */
void ChromeUtils::Import(const GlobalObject& aGlobal,
                         const nsAString& aResourceURI,
                         const Optional<JS::Handle<JSObject*>>& aTargetObj,
                         JS::MutableHandle<JSObject*> aRetval,
                         ErrorResult& aRv) {
  RefPtr<mozJSComponentLoader> moduleloader = mozJSComponentLoader::Get();
  MOZ_ASSERT(moduleloader);

  NS_ConvertUTF16toUTF8 registryLocation(aResourceURI);

  AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING("ChromeUtils::Import", OTHER,
                                        registryLocation);

  JSContext* cx = aGlobal.Context();

  bool ignoreExports = aTargetObj.WasPassed() && !aTargetObj.Value();

  JS::RootedObject global(cx);
  JS::RootedObject exports(cx);
  nsresult rv = moduleloader->Import(cx, registryLocation, &global, &exports,
                                     ignoreExports);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  // Import() on the component loader can return NS_OK while leaving an
  // exception on the JSContext.  Check for that case.
  if (JS_IsExceptionPending(cx)) {
    aRv.NoteJSContextException(cx);
    return;
  }

  if (ignoreExports) {
    // Since we're ignoring exported symbols, return the module global rather
    // than an exports object.
    //
    // Note: This behavior is deprecated, since it is incompatible with ES6
    // module semantics, which don't include any such global object.
    if (!JS_WrapObject(cx, &global)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
    aRetval.set(global);
    return;
  }

  if (aTargetObj.WasPassed()) {
    if (!JS_AssignObject(cx, aTargetObj.Value(), exports)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
  }

  if (!JS_WrapObject(cx, &exports)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
  aRetval.set(exports);
}

namespace module_getter {
static const size_t SLOT_ID = 0;
static const size_t SLOT_URI = 1;

static bool ExtractArgs(JSContext* aCx, JS::CallArgs& aArgs,
                        JS::MutableHandle<JSObject*> aCallee,
                        JS::MutableHandle<JSObject*> aThisObj,
                        JS::MutableHandle<jsid> aId) {
  aCallee.set(&aArgs.callee());

  JS::Handle<JS::Value> thisv = aArgs.thisv();
  if (!thisv.isObject()) {
    JS_ReportErrorASCII(aCx, "Invalid target object");
    return false;
  }

  aThisObj.set(&thisv.toObject());

  JS::Rooted<JS::Value> id(aCx,
                           js::GetFunctionNativeReserved(aCallee, SLOT_ID));
  MOZ_ALWAYS_TRUE(JS_ValueToId(aCx, id, aId));
  return true;
}

static bool ModuleGetter(JSContext* aCx, unsigned aArgc, JS::Value* aVp) {
  JS::CallArgs args = JS::CallArgsFromVp(aArgc, aVp);

  JS::Rooted<JSObject*> callee(aCx);
  JS::Rooted<JSObject*> thisObj(aCx);
  JS::Rooted<jsid> id(aCx);
  if (!ExtractArgs(aCx, args, &callee, &thisObj, &id)) {
    return false;
  }

  JS::Rooted<JSString*> moduleURI(
      aCx, js::GetFunctionNativeReserved(callee, SLOT_URI).toString());
  JS::UniqueChars bytes = JS_EncodeStringToUTF8(aCx, moduleURI);
  if (!bytes) {
    return false;
  }
  nsDependentCString uri(bytes.get());

  RefPtr<mozJSComponentLoader> moduleloader = mozJSComponentLoader::Get();
  MOZ_ASSERT(moduleloader);

  JS::Rooted<JSObject*> moduleGlobal(aCx);
  JS::Rooted<JSObject*> moduleExports(aCx);
  nsresult rv = moduleloader->Import(aCx, uri, &moduleGlobal, &moduleExports);
  if (NS_FAILED(rv)) {
    Throw(aCx, rv);
    return false;
  }

  JS::RootedValue value(aCx);
  if (!JS_GetPropertyById(aCx, moduleExports, id, &value)) {
    return false;
  }

  if (!JS_DefinePropertyById(aCx, thisObj, id, value, JSPROP_ENUMERATE)) {
    return false;
  }

  args.rval().set(value);
  return true;
}

static bool ModuleSetter(JSContext* aCx, unsigned aArgc, JS::Value* aVp) {
  JS::CallArgs args = JS::CallArgsFromVp(aArgc, aVp);

  JS::Rooted<JSObject*> callee(aCx);
  JS::Rooted<JSObject*> thisObj(aCx);
  JS::Rooted<jsid> id(aCx);
  if (!ExtractArgs(aCx, args, &callee, &thisObj, &id)) {
    return false;
  }

  return JS_DefinePropertyById(aCx, thisObj, id, args.get(0), JSPROP_ENUMERATE);
}

static bool DefineGetter(JSContext* aCx, JS::Handle<JSObject*> aTarget,
                         const nsAString& aId, const nsAString& aResourceURI) {
  JS::RootedValue uri(aCx);
  JS::RootedValue idValue(aCx);
  JS::Rooted<jsid> id(aCx);
  if (!xpc::NonVoidStringToJsval(aCx, aResourceURI, &uri) ||
      !xpc::NonVoidStringToJsval(aCx, aId, &idValue) ||
      !JS_ValueToId(aCx, idValue, &id)) {
    return false;
  }
  idValue = js::IdToValue(id);

  JS::Rooted<JSObject*> getter(
      aCx, JS_GetFunctionObject(
               js::NewFunctionByIdWithReserved(aCx, ModuleGetter, 0, 0, id)));

  JS::Rooted<JSObject*> setter(
      aCx, JS_GetFunctionObject(
               js::NewFunctionByIdWithReserved(aCx, ModuleSetter, 0, 0, id)));

  if (!getter || !setter) {
    JS_ReportOutOfMemory(aCx);
    return false;
  }

  js::SetFunctionNativeReserved(getter, SLOT_ID, idValue);
  js::SetFunctionNativeReserved(setter, SLOT_ID, idValue);

  js::SetFunctionNativeReserved(getter, SLOT_URI, uri);

  return JS_DefinePropertyById(
      aCx, aTarget, id, getter, setter,
      JSPROP_GETTER | JSPROP_SETTER | JSPROP_ENUMERATE);
}
}  // namespace module_getter

/* static */
void ChromeUtils::DefineModuleGetter(const GlobalObject& global,
                                     JS::Handle<JSObject*> target,
                                     const nsAString& id,
                                     const nsAString& resourceURI,
                                     ErrorResult& aRv) {
  if (!module_getter::DefineGetter(global.Context(), target, id, resourceURI)) {
    aRv.NoteJSContextException(global.Context());
  }
}

/* static */
void ChromeUtils::OriginAttributesToSuffix(
    dom::GlobalObject& aGlobal, const dom::OriginAttributesDictionary& aAttrs,
    nsCString& aSuffix)

{
  OriginAttributes attrs(aAttrs);
  attrs.CreateSuffix(aSuffix);
}

/* static */
bool ChromeUtils::OriginAttributesMatchPattern(
    dom::GlobalObject& aGlobal, const dom::OriginAttributesDictionary& aAttrs,
    const dom::OriginAttributesPatternDictionary& aPattern) {
  OriginAttributes attrs(aAttrs);
  OriginAttributesPattern pattern(aPattern);
  return pattern.Matches(attrs);
}

/* static */
void ChromeUtils::CreateOriginAttributesFromOrigin(
    dom::GlobalObject& aGlobal, const nsAString& aOrigin,
    dom::OriginAttributesDictionary& aAttrs, ErrorResult& aRv) {
  OriginAttributes attrs;
  nsAutoCString suffix;
  if (!attrs.PopulateFromOrigin(NS_ConvertUTF16toUTF8(aOrigin), suffix)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
  aAttrs = attrs;
}

/* static */
void ChromeUtils::FillNonDefaultOriginAttributes(
    dom::GlobalObject& aGlobal, const dom::OriginAttributesDictionary& aAttrs,
    dom::OriginAttributesDictionary& aNewAttrs) {
  aNewAttrs = aAttrs;
}

/* static */
bool ChromeUtils::IsOriginAttributesEqual(
    dom::GlobalObject& aGlobal, const dom::OriginAttributesDictionary& aA,
    const dom::OriginAttributesDictionary& aB) {
  return IsOriginAttributesEqual(aA, aB);
}

/* static */
bool ChromeUtils::IsOriginAttributesEqual(
    const dom::OriginAttributesDictionary& aA,
    const dom::OriginAttributesDictionary& aB) {
  return aA == aB;
}

#ifdef NIGHTLY_BUILD
/* static */
void ChromeUtils::GetRecentJSDevError(GlobalObject& aGlobal,
                                      JS::MutableHandleValue aRetval,
                                      ErrorResult& aRv) {
  aRetval.setUndefined();
  auto runtime = CycleCollectedJSRuntime::Get();
  MOZ_ASSERT(runtime);

  auto cx = aGlobal.Context();
  if (!runtime->GetRecentDevError(cx, aRetval)) {
    aRv.NoteJSContextException(cx);
    return;
  }
}

/* static */
void ChromeUtils::ClearRecentJSDevError(GlobalObject&) {
  auto runtime = CycleCollectedJSRuntime::Get();
  MOZ_ASSERT(runtime);

  runtime->ClearRecentDevError();
}
#endif  // NIGHTLY_BUILD

/* static */
already_AddRefed<Promise> ChromeUtils::RequestProcInfo(GlobalObject& aGlobal,
                                                       ErrorResult& aRv) {
  // This function will use IPDL to enable threads info on macOS
  // see https://bugzilla.mozilla.org/show_bug.cgi?id=1529023
  if (!XRE_IsParentProcess()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  // Creating a JS promise
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  MOZ_ASSERT(global);
  RefPtr<Promise> domPromise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  MOZ_ASSERT(domPromise);

  base::ProcessId parentPid = base::GetCurrentProcId();
  RefPtr<nsISerialEventTarget> target =
      global->EventTargetFor(TaskCategory::Performance);

  // Getting the parent proc info
  mozilla::GetProcInfo(parentPid, 0, mozilla::ProcType::Browser)
      ->Then(
          target, __func__,
          [target, domPromise, parentPid](ProcInfo aParentInfo) {
            // Get a list of ContentParent
            nsTArray<ContentParent*> contentParents;
            ContentParent::GetAll(contentParents);
            nsTArray<RefPtr<ProcInfoPromise>> promises;
            mozilla::ipc::GeckoChildProcessHost::GetAll(
                [&promises, contentParents](
                    mozilla::ipc::GeckoChildProcessHost* aGeckoProcess) {
                  if (!aGeckoProcess->GetChildProcessHandle()) {
                    return;
                  }

                  base::ProcessId childPid =
                      base::GetProcId(aGeckoProcess->GetChildProcessHandle());
                  int32_t childId = 0;
                  mozilla::ProcType type;
                  switch (aGeckoProcess->GetProcessType()) {
                    case GeckoProcessType::GeckoProcessType_Content: {
                      ContentParent* contentParent = nullptr;
                      // This loop can become slow as we get more processes in
                      // Fission, so might need some refactoring in the future.
                      for (ContentParent* parent : contentParents) {
                        // find the match
                        if (parent->Process() == aGeckoProcess) {
                          contentParent = parent;
                          break;
                        }
                      }
                      if (!contentParent) {
                        return;
                      }
                      // Converting the Content Type into a ProcType
                      nsAutoString processType;
                      processType.Assign(contentParent->GetRemoteType());
                      if (processType.EqualsLiteral(DEFAULT_REMOTE_TYPE)) {
                        type = mozilla::ProcType::Web;
                      } else if (processType.EqualsLiteral(FILE_REMOTE_TYPE)) {
                        type = mozilla::ProcType::File;
                      } else if (processType.EqualsLiteral(
                                     EXTENSION_REMOTE_TYPE)) {
                        type = mozilla::ProcType::Extension;
                      } else if (processType.EqualsLiteral(
                                     PRIVILEGEDABOUT_REMOTE_TYPE)) {
                        type = mozilla::ProcType::PrivilegedAbout;
                      } else if (processType.EqualsLiteral(
                                     LARGE_ALLOCATION_REMOTE_TYPE)) {
                        type = mozilla::ProcType::WebLargeAllocation;
                      }
                      childId = contentParent->ChildID();
                      break;
                    }
                    case GeckoProcessType::GeckoProcessType_Default:
                      type = mozilla::ProcType::Browser;
                      break;
                    case GeckoProcessType::GeckoProcessType_Plugin:
                      type = mozilla::ProcType::Plugin;
                      break;
                    case GeckoProcessType::GeckoProcessType_GMPlugin:
                      type = mozilla::ProcType::GMPlugin;
                      break;
                    case GeckoProcessType::GeckoProcessType_GPU:
                      type = mozilla::ProcType::GPU;
                      break;
                    case GeckoProcessType::GeckoProcessType_VR:
                      type = mozilla::ProcType::VR;
                      break;
                    case GeckoProcessType::GeckoProcessType_RDD:
                      type = mozilla::ProcType::RDD;
                      break;
                    case GeckoProcessType::GeckoProcessType_Socket:
                      type = mozilla::ProcType::Socket;
                      break;
                    case GeckoProcessType::GeckoProcessType_RemoteSandboxBroker:
                      type = mozilla::ProcType::RemoteSandboxBroker;
                      break;
                    default:
                      type = mozilla::ProcType::Unknown;
                  }

                  promises.AppendElement(
#ifdef XP_MACOSX
                      mozilla::GetProcInfo(childPid, childId, type,
                                           aGeckoProcess->GetChildTask())
#else
                      mozilla::GetProcInfo(childPid, childId, type)
#endif
                  );
                });

            auto ProcInfoResolver =
                [domPromise, parentPid, parentInfo = aParentInfo](
                    const nsTArray<ProcInfo>& aChildrenInfo) {
                  mozilla::dom::ParentProcInfoDictionary procInfo;
                  // parent, basic info.
                  procInfo.mPid = parentPid;
                  procInfo.mFilename.Assign(parentInfo.filename);
                  procInfo.mType = mozilla::dom::ProcType::Browser;
                  procInfo.mVirtualMemorySize = parentInfo.virtualMemorySize;
                  procInfo.mResidentSetSize = parentInfo.residentSetSize;
                  procInfo.mCpuUser = parentInfo.cpuUser;
                  procInfo.mCpuKernel = parentInfo.cpuKernel;

                  // parent, threads info.
                  mozilla::dom::Sequence<mozilla::dom::ThreadInfoDictionary>
                      threads;
                  for (const ThreadInfo& entry : parentInfo.threads) {
                    ThreadInfoDictionary* thread =
                        threads.AppendElement(fallible);
                    if (NS_WARN_IF(!thread)) {
                      domPromise->MaybeReject(NS_ERROR_OUT_OF_MEMORY);
                      return;
                    }
                    thread->mCpuUser = entry.cpuUser;
                    thread->mCpuKernel = entry.cpuKernel;
                    thread->mTid = entry.tid;
                  }
                  procInfo.mThreads = threads;

                  mozilla::dom::Sequence<mozilla::dom::ChildProcInfoDictionary>
                      children;
                  for (const ProcInfo& info : aChildrenInfo) {
                    ChildProcInfoDictionary* childProcInfo =
                        children.AppendElement(fallible);
                    if (NS_WARN_IF(!childProcInfo)) {
                      domPromise->MaybeReject(NS_ERROR_OUT_OF_MEMORY);
                      return;
                    }
                    // Basic info.
                    childProcInfo->mChildID = info.childId;
                    childProcInfo->mType = static_cast<ProcType>(info.type);
                    childProcInfo->mPid = info.pid;
                    childProcInfo->mFilename.Assign(info.filename);
                    childProcInfo->mVirtualMemorySize = info.virtualMemorySize;
                    childProcInfo->mResidentSetSize = info.residentSetSize;
                    childProcInfo->mCpuUser = info.cpuUser;
                    childProcInfo->mCpuKernel = info.cpuKernel;

                    // Threads info.
                    mozilla::dom::Sequence<mozilla::dom::ThreadInfoDictionary>
                        threads;
                    for (const ThreadInfo& entry : info.threads) {
                      ThreadInfoDictionary* thread =
                          threads.AppendElement(fallible);
                      if (NS_WARN_IF(!thread)) {
                        domPromise->MaybeReject(NS_ERROR_OUT_OF_MEMORY);
                        return;
                      }
                      thread->mCpuUser = entry.cpuUser;
                      thread->mCpuKernel = entry.cpuKernel;
                      thread->mTid = entry.tid;
                      thread->mName.Assign(entry.name);
                    }
                    childProcInfo->mThreads = threads;
                  }
                  procInfo.mChildren = children;
                  domPromise->MaybeResolve(procInfo);
                };  // end of ProcInfoResolver

            ProcInfoPromise::All(target, promises)
                ->Then(target, __func__, std::move(ProcInfoResolver),
                       [domPromise](const nsresult aResult) {
                         domPromise->MaybeReject(aResult);
                       });  // end of ProcInfoPromise::All
          },
          [domPromise](nsresult aRv) {
            domPromise->MaybeReject(aRv);
          });  // end of mozilla::GetProcInfo

  // sending back the promise instance
  return domPromise.forget();
}

/* static */
already_AddRefed<Promise> ChromeUtils::RequestPerformanceMetrics(
    GlobalObject& aGlobal, ErrorResult& aRv) {
  MOZ_ASSERT(XRE_IsParentProcess());

  // Creating a JS promise
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  MOZ_ASSERT(global);
  RefPtr<Promise> domPromise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  MOZ_ASSERT(domPromise);
  RefPtr<nsISerialEventTarget> target =
      global->EventTargetFor(TaskCategory::Performance);

  // requesting metrics, that will be returned into the promise
  PerformanceMetricsCollector::RequestMetrics()->Then(
      target, __func__,
      [domPromise,
       target](nsTArray<dom::PerformanceInfoDictionary>&& aResults) {
        domPromise->MaybeResolve(std::move(aResults));
      },
      [domPromise](const nsresult& aRv) { domPromise->MaybeReject(aRv); });

  // sending back the promise instance
  return domPromise.forget();
}

void ChromeUtils::SetPerfStatsCollectionMask(GlobalObject& aGlobal,
                                             uint64_t aMask) {
  PerfStats::SetCollectionMask(static_cast<PerfStats::MetricMask>(aMask));
}

already_AddRefed<Promise> ChromeUtils::CollectPerfStats(GlobalObject& aGlobal,
                                                        ErrorResult& aRv) {
  // Creating a JS promise
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  MOZ_ASSERT(global);

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<PerfStats::PerfStatsPromise> extPromise =
      PerfStats::CollectPerfStatsJSON();

  extPromise->Then(
      GetCurrentThreadSerialEventTarget(), __func__,
      [promise](const nsCString& aResult) {
        promise->MaybeResolve(NS_ConvertUTF8toUTF16(aResult));
      },
      [promise](bool aValue) { promise->MaybeReject(NS_ERROR_FAILURE); });

  return promise.forget();
}

constexpr auto kSkipSelfHosted = JS::SavedFrameSelfHosted::Exclude;

/* static */
void ChromeUtils::GetCallerLocation(const GlobalObject& aGlobal,
                                    nsIPrincipal* aPrincipal,
                                    JS::MutableHandle<JSObject*> aRetval) {
  JSContext* cx = aGlobal.Context();

  auto* principals = nsJSPrincipals::get(aPrincipal);

  JS::StackCapture captureMode(JS::FirstSubsumedFrame(cx, principals));

  JS::RootedObject frame(cx);
  if (!JS::CaptureCurrentStack(cx, &frame, std::move(captureMode))) {
    JS_ClearPendingException(cx);
    aRetval.set(nullptr);
    return;
  }

  // FirstSubsumedFrame gets us a stack which stops at the first principal which
  // is subsumed by the given principal. That means that we may have a lot of
  // privileged frames that we don't care about at the top of the stack, though.
  // We need to filter those out to get the frame we actually want.
  aRetval.set(
      js::GetFirstSubsumedSavedFrame(cx, principals, frame, kSkipSelfHosted));
}

/* static */
void ChromeUtils::CreateError(const GlobalObject& aGlobal,
                              const nsAString& aMessage,
                              JS::Handle<JSObject*> aStack,
                              JS::MutableHandle<JSObject*> aRetVal,
                              ErrorResult& aRv) {
  if (aStack && !JS::IsMaybeWrappedSavedFrame(aStack)) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  JSContext* cx = aGlobal.Context();

  auto cleanup = MakeScopeExit([&]() { aRv.NoteJSContextException(cx); });

  JS::RootedObject retVal(cx);
  {
    JS::RootedString fileName(cx, JS_GetEmptyString(cx));
    uint32_t line = 0;
    uint32_t column = 0;

    Maybe<JSAutoRealm> ar;
    JS::RootedObject stack(cx);
    if (aStack) {
      stack = UncheckedUnwrap(aStack);
      ar.emplace(cx, stack);

      JSPrincipals* principals =
          JS::GetRealmPrincipals(js::GetContextRealm(cx));
      if (JS::GetSavedFrameLine(cx, principals, stack, &line) !=
              JS::SavedFrameResult::Ok ||
          JS::GetSavedFrameColumn(cx, principals, stack, &column) !=
              JS::SavedFrameResult::Ok ||
          JS::GetSavedFrameSource(cx, principals, stack, &fileName) !=
              JS::SavedFrameResult::Ok) {
        return;
      }
    }

    JS::RootedString message(cx);
    {
      JS::RootedValue msgVal(cx);
      if (!xpc::NonVoidStringToJsval(cx, aMessage, &msgVal)) {
        return;
      }
      message = msgVal.toString();
    }

    JS::Rooted<JS::Value> err(cx);
    if (!JS::CreateError(cx, JSEXN_ERR, stack, fileName, line, column, nullptr,
                         message, &err)) {
      return;
    }

    MOZ_ASSERT(err.isObject());
    retVal = &err.toObject();
  }

  if (aStack && !JS_WrapObject(cx, &retVal)) {
    return;
  }

  cleanup.release();
  aRetVal.set(retVal);
}

/* static */
already_AddRefed<Promise> ChromeUtils::RequestIOActivity(GlobalObject& aGlobal,
                                                         ErrorResult& aRv) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(Preferences::GetBool(IO_ACTIVITY_ENABLED_PREF, false));
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  MOZ_ASSERT(global);
  RefPtr<Promise> domPromise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  MOZ_ASSERT(domPromise);
  mozilla::net::IOActivityMonitor::RequestActivities(domPromise);
  return domPromise.forget();
}

/* static */
bool ChromeUtils::HasReportingHeaderForOrigin(GlobalObject& global,
                                              const nsAString& aOrigin,
                                              ErrorResult& aRv) {
  if (!XRE_IsParentProcess()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  return ReportingHeader::HasReportingHeaderForOrigin(
      NS_ConvertUTF16toUTF8(aOrigin));
}

/* static */
PopupBlockerState ChromeUtils::GetPopupControlState(GlobalObject& aGlobal) {
  switch (PopupBlocker::GetPopupControlState()) {
    case PopupBlocker::PopupControlState::openAllowed:
      return PopupBlockerState::OpenAllowed;

    case PopupBlocker::PopupControlState::openControlled:
      return PopupBlockerState::OpenControlled;

    case PopupBlocker::PopupControlState::openBlocked:
      return PopupBlockerState::OpenBlocked;

    case PopupBlocker::PopupControlState::openAbused:
      return PopupBlockerState::OpenAbused;

    case PopupBlocker::PopupControlState::openOverridden:
      return PopupBlockerState::OpenOverridden;

    default:
      MOZ_CRASH(
          "PopupBlocker::PopupControlState and PopupBlockerState are out of "
          "sync");
  }
}

/* static */
bool ChromeUtils::IsPopupTokenUnused(GlobalObject& aGlobal) {
  return PopupBlocker::IsPopupOpeningTokenUnused();
}

/* static */
double ChromeUtils::LastExternalProtocolIframeAllowed(GlobalObject& aGlobal) {
  TimeStamp when = PopupBlocker::WhenLastExternalProtocolIframeAllowed();
  if (when.IsNull()) {
    return 0;
  }

  TimeDuration duration = TimeStamp::Now() - when;
  return duration.ToMilliseconds();
}

/* static */
void ChromeUtils::ResetLastExternalProtocolIframeAllowed(
    GlobalObject& aGlobal) {
  PopupBlocker::ResetLastExternalProtocolIframeAllowed();
}

/* static */
void ChromeUtils::RegisterWindowActor(const GlobalObject& aGlobal,
                                      const nsAString& aName,
                                      const WindowActorOptions& aOptions,
                                      ErrorResult& aRv) {
  MOZ_ASSERT(XRE_IsParentProcess());

  RefPtr<JSWindowActorService> service = JSWindowActorService::GetSingleton();
  service->RegisterWindowActor(aName, aOptions, aRv);
}

/* static */
void ChromeUtils::UnregisterWindowActor(const GlobalObject& aGlobal,
                                        const nsAString& aName) {
  MOZ_ASSERT(XRE_IsParentProcess());

  RefPtr<JSWindowActorService> service = JSWindowActorService::GetSingleton();
  service->UnregisterWindowActor(aName);
}

/* static */
bool ChromeUtils::IsClassifierBlockingErrorCode(GlobalObject& aGlobal,
                                                uint32_t aError) {
  return net::UrlClassifierFeatureFactory::IsClassifierBlockingErrorCode(
      static_cast<nsresult>(aError));
}

/* static */
void ChromeUtils::PrivateNoteIntentionalCrash(const GlobalObject& aGlobal,
                                              ErrorResult& aError) {
  if (XRE_IsContentProcess()) {
    NoteIntentionalCrash("tab");
    return;
  }
  aError.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

}  // namespace dom
}  // namespace mozilla

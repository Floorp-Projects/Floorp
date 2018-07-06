/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromeUtils.h"

#include "jsfriendapi.h"
#include "WrapperFactory.h"

#include "mozilla/Base64.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/PerformanceUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/IdleDeadline.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/dom/WindowBinding.h" // For IdleRequestCallback/Options
#include "IOActivityMonitor.h"
#include "nsThreadUtils.h"
#include "mozJSComponentLoader.h"
#include "GeckoProfiler.h"

namespace mozilla {
namespace dom {

/* static */ void
ChromeUtils::NondeterministicGetWeakMapKeys(GlobalObject& aGlobal,
                                            JS::Handle<JS::Value> aMap,
                                            JS::MutableHandle<JS::Value> aRetval,
                                            ErrorResult& aRv)
{
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

/* static */ void
ChromeUtils::NondeterministicGetWeakSetKeys(GlobalObject& aGlobal,
                                            JS::Handle<JS::Value> aSet,
                                            JS::MutableHandle<JS::Value> aRetval,
                                            ErrorResult& aRv)
{
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

/* static */ void
ChromeUtils::Base64URLEncode(GlobalObject& aGlobal,
                             const ArrayBufferViewOrArrayBuffer& aSource,
                             const Base64URLEncodeOptions& aOptions,
                             nsACString& aResult,
                             ErrorResult& aRv)
{
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

  auto paddingPolicy = aOptions.mPad ? Base64URLEncodePaddingPolicy::Include :
                                       Base64URLEncodePaddingPolicy::Omit;
  nsresult rv = mozilla::Base64URLEncode(length, data, paddingPolicy, aResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aResult.Truncate();
    aRv.Throw(rv);
  }
}

/* static */ void
ChromeUtils::Base64URLDecode(GlobalObject& aGlobal,
                             const nsACString& aString,
                             const Base64URLDecodeOptions& aOptions,
                             JS::MutableHandle<JSObject*> aRetval,
                             ErrorResult& aRv)
{
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

  JS::Rooted<JSObject*> buffer(aGlobal.Context(),
                               ArrayBuffer::Create(aGlobal.Context(),
                                                   data.Length(),
                                                   data.Elements()));
  if (NS_WARN_IF(!buffer)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  aRetval.set(buffer);
}

/* static */ void
ChromeUtils::WaiveXrays(GlobalObject& aGlobal,
                        JS::HandleValue aVal,
                        JS::MutableHandleValue aRetval,
                        ErrorResult& aRv)
{
  JS::RootedValue value(aGlobal.Context(), aVal);
  if (!xpc::WrapperFactory::WaiveXrayAndWrap(aGlobal.Context(), &value)) {
    aRv.NoteJSContextException(aGlobal.Context());
  } else {
    aRetval.set(value);
  }
}

/* static */ void
ChromeUtils::UnwaiveXrays(GlobalObject& aGlobal,
                          JS::HandleValue aVal,
                          JS::MutableHandleValue aRetval,
                          ErrorResult& aRv)
{
  if (!aVal.isObject()) {
      aRetval.set(aVal);
      return;
  }

  JS::RootedObject obj(aGlobal.Context(), js::UncheckedUnwrap(&aVal.toObject()));
  if (!JS_WrapObject(aGlobal.Context(), &obj)) {
    aRv.NoteJSContextException(aGlobal.Context());
  } else {
    aRetval.setObject(*obj);
  }
}

/* static */ void
ChromeUtils::GetClassName(GlobalObject& aGlobal,
                          JS::HandleObject aObj,
                          bool aUnwrap,
                          nsAString& aRetval)
{
  JS::RootedObject obj(aGlobal.Context(), aObj);
  if (aUnwrap) {
    obj = js::UncheckedUnwrap(obj, /* stopAtWindowProxy = */ false);
  }

  aRetval = NS_ConvertUTF8toUTF16(nsDependentCString(js::GetObjectClass(obj)->name));
}

/* static */ void
ChromeUtils::ShallowClone(GlobalObject& aGlobal,
                          JS::HandleObject aObj,
                          JS::HandleObject aTarget,
                          JS::MutableHandleObject aRetval,
                          ErrorResult& aRv)
{
  JSContext* cx = aGlobal.Context();

  auto cleanup = MakeScopeExit([&] () {
    aRv.NoteJSContextException(cx);
  });

  JS::Rooted<JS::IdVector> ids(cx, JS::IdVector(cx));
  JS::AutoValueVector values(cx);

  {
    JS::RootedObject obj(cx, js::CheckedUnwrap(aObj));
    if (!obj) {
      js::ReportAccessDenied(cx);
      return;
    }

    if (js::IsScriptedProxy(obj)) {
      JS_ReportErrorASCII(cx, "Shallow cloning a proxy object is not allowed");
      return;
    }

    JSAutoRealm ar(cx, obj);

    if (!JS_Enumerate(cx, obj, &ids) ||
        !values.reserve(ids.length())) {
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
      values.infallibleAppend(desc.value());
    }
  }

  JS::RootedObject obj(cx);
  {
    Maybe<JSAutoRealm> ar;
    if (aTarget) {
      JS::RootedObject target(cx, js::CheckedUnwrap(aTarget));
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
    for (uint32_t i = 0; i < ids.length(); i++) {
      id = ids[i];
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
  class IdleDispatchRunnable final : public IdleRunnable
                                   , public nsITimerCallback
  {
  public:
    NS_DECL_ISUPPORTS_INHERITED

    IdleDispatchRunnable(nsIGlobalObject* aParent,
                         IdleRequestCallback& aCallback)
      : IdleRunnable("ChromeUtils::IdleDispatch")
      , mCallback(&aCallback)
      , mParent(aParent)
    {}

    NS_IMETHOD Run() override
    {
      if (mCallback) {
        CancelTimer();

        auto deadline = mDeadline - TimeStamp::ProcessCreation();

        ErrorResult rv;
        RefPtr<IdleDeadline> idleDeadline =
          new IdleDeadline(mParent, mTimedOut, deadline.ToMilliseconds());

        mCallback->Call(*idleDeadline, rv, "ChromeUtils::IdleDispatch handler");
        mCallback = nullptr;
        mParent = nullptr;

        rv.SuppressException();
        return rv.StealNSResult();
      }
      return NS_OK;
    }

    void SetDeadline(TimeStamp aDeadline) override
    {
      mDeadline = aDeadline;
    }

    NS_IMETHOD Notify(nsITimer* aTimer) override
    {
      mTimedOut = true;
      SetDeadline(TimeStamp::Now());
      return Run();
    }

    void SetTimer(uint32_t aDelay, nsIEventTarget* aTarget) override
    {
      MOZ_ASSERT(aTarget);
      MOZ_ASSERT(!mTimer);
      NS_NewTimerWithCallback(getter_AddRefs(mTimer),
                              this, aDelay, nsITimer::TYPE_ONE_SHOT,
                              aTarget);
    }

  protected:
    virtual ~IdleDispatchRunnable()
    {
      CancelTimer();
    }

  private:
    void CancelTimer()
    {
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

  NS_IMPL_ISUPPORTS_INHERITED(IdleDispatchRunnable, IdleRunnable, nsITimerCallback)
} // anonymous namespace

/* static */ void
ChromeUtils::IdleDispatch(const GlobalObject& aGlobal,
                          IdleRequestCallback& aCallback,
                          const IdleRequestOptions& aOptions,
                          ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  MOZ_ASSERT(global);

  auto runnable = MakeRefPtr<IdleDispatchRunnable>(global, aCallback);

  if (aOptions.mTimeout.WasPassed()) {
    aRv = NS_IdleDispatchToCurrentThread(runnable.forget(), aOptions.mTimeout.Value());
  } else {
    aRv = NS_IdleDispatchToCurrentThread(runnable.forget());
  }
}

/* static */ void
ChromeUtils::Import(const GlobalObject& aGlobal,
                    const nsAString& aResourceURI,
                    const Optional<JS::Handle<JSObject*>>& aTargetObj,
                    JS::MutableHandle<JSObject*> aRetval,
                    ErrorResult& aRv)
{

  RefPtr<mozJSComponentLoader> moduleloader = mozJSComponentLoader::Get();
  MOZ_ASSERT(moduleloader);

  NS_ConvertUTF16toUTF8 registryLocation(aResourceURI);

  AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING(
    "ChromeUtils::Import", OTHER, registryLocation);

  JSContext* cx = aGlobal.Context();
  JS::Rooted<JS::Value> targetObj(cx);
  uint8_t optionalArgc;
  if (aTargetObj.WasPassed()) {
    targetObj.setObjectOrNull(aTargetObj.Value());
    optionalArgc = 1;
  } else {
    targetObj.setUndefined();
    optionalArgc = 0;
  }

  JS::Rooted<JS::Value> retval(cx);
  nsresult rv = moduleloader->ImportInto(registryLocation, targetObj, cx,
                                         optionalArgc, &retval);
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

  // Now we better have an object.
  MOZ_ASSERT(retval.isObject());
  aRetval.set(&retval.toObject());
}

namespace module_getter {
  static const size_t SLOT_ID = 0;
  static const size_t SLOT_URI = 1;

  static bool
  ExtractArgs(JSContext* aCx, JS::CallArgs& aArgs,
              JS::MutableHandle<JSObject*> aCallee,
              JS::MutableHandle<JSObject*> aThisObj,
              JS::MutableHandle<jsid> aId)
  {
    aCallee.set(&aArgs.callee());

    JS::Handle<JS::Value> thisv = aArgs.thisv();
    if (!thisv.isObject()) {
      JS_ReportErrorASCII(aCx, "Invalid target object");
      return false;
    }

    aThisObj.set(&thisv.toObject());

    JS::Rooted<JS::Value> id(aCx, js::GetFunctionNativeReserved(aCallee, SLOT_ID));
    MOZ_ALWAYS_TRUE(JS_ValueToId(aCx, id, aId));
    return true;
  }

  static bool
  ModuleGetter(JSContext* aCx, unsigned aArgc, JS::Value* aVp)
  {
    JS::CallArgs args = JS::CallArgsFromVp(aArgc, aVp);

    JS::Rooted<JSObject*> callee(aCx);
    JS::Rooted<JSObject*> thisObj(aCx);
    JS::Rooted<jsid> id(aCx);
    if (!ExtractArgs(aCx, args, &callee, &thisObj, &id)) {
      return false;
    }

    JS::Rooted<JSString*> moduleURI(
      aCx, js::GetFunctionNativeReserved(callee, SLOT_URI).toString());
    JSAutoByteString bytes;
    if (!bytes.encodeUtf8(aCx, moduleURI)) {
      return false;
    }
    nsDependentCString uri(bytes.ptr());

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
    {
      JSAutoRealm ar(aCx, moduleExports);

      if (!JS_GetPropertyById(aCx, moduleExports, id, &value)) {
        return false;
      }
    }

    if (!JS_WrapValue(aCx, &value) ||
        !JS_DefinePropertyById(aCx, thisObj, id, value,
                               JSPROP_ENUMERATE)) {
      return false;
    }

    args.rval().set(value);
    return true;
  }

  static bool
  ModuleSetter(JSContext* aCx, unsigned aArgc, JS::Value* aVp)
  {
    JS::CallArgs args = JS::CallArgsFromVp(aArgc, aVp);

    JS::Rooted<JSObject*> callee(aCx);
    JS::Rooted<JSObject*> thisObj(aCx);
    JS::Rooted<jsid> id(aCx);
    if (!ExtractArgs(aCx, args, &callee, &thisObj, &id)) {
      return false;
    }

    return JS_DefinePropertyById(aCx, thisObj, id, args.get(0),
                                 JSPROP_ENUMERATE);
  }

  static bool
  DefineGetter(JSContext* aCx,
               JS::Handle<JSObject*> aTarget,
               const nsAString& aId,
               const nsAString& aResourceURI)
  {
    JS::RootedValue uri(aCx);
    JS::RootedValue idValue(aCx);
    JS::Rooted<jsid> id(aCx);
    if (!xpc::NonVoidStringToJsval(aCx, aResourceURI, &uri) ||
        !xpc::NonVoidStringToJsval(aCx, aId, &idValue) ||
        !JS_ValueToId(aCx, idValue, &id)) {
      return false;
    }
    idValue = js::IdToValue(id);


    JS::Rooted<JSObject*> getter(aCx, JS_GetFunctionObject(
      js::NewFunctionByIdWithReserved(aCx, ModuleGetter, 0, 0, id)));

    JS::Rooted<JSObject*> setter(aCx, JS_GetFunctionObject(
      js::NewFunctionByIdWithReserved(aCx, ModuleSetter, 0, 0, id)));

    if (!getter || !setter) {
      JS_ReportOutOfMemory(aCx);
      return false;
    }

    js::SetFunctionNativeReserved(getter, SLOT_ID, idValue);
    js::SetFunctionNativeReserved(setter, SLOT_ID, idValue);

    js::SetFunctionNativeReserved(getter, SLOT_URI, uri);

    return JS_DefinePropertyById(aCx, aTarget, id, getter, setter,
                                 JSPROP_GETTER | JSPROP_SETTER | JSPROP_ENUMERATE);
  }
} // namespace module_getter

/* static */ void
ChromeUtils::DefineModuleGetter(const GlobalObject& global,
                                JS::Handle<JSObject*> target,
                                const nsAString& id,
                                const nsAString& resourceURI,
                                ErrorResult& aRv)
{
  if (!module_getter::DefineGetter(global.Context(), target, id, resourceURI)) {
    aRv.NoteJSContextException(global.Context());
  }
}

/* static */ void
ChromeUtils::OriginAttributesToSuffix(dom::GlobalObject& aGlobal,
                                      const dom::OriginAttributesDictionary& aAttrs,
                                      nsCString& aSuffix)

{
  OriginAttributes attrs(aAttrs);
  attrs.CreateSuffix(aSuffix);
}

/* static */ bool
ChromeUtils::OriginAttributesMatchPattern(dom::GlobalObject& aGlobal,
                                          const dom::OriginAttributesDictionary& aAttrs,
                                          const dom::OriginAttributesPatternDictionary& aPattern)
{
  OriginAttributes attrs(aAttrs);
  OriginAttributesPattern pattern(aPattern);
  return pattern.Matches(attrs);
}

/* static */ void
ChromeUtils::CreateOriginAttributesFromOrigin(dom::GlobalObject& aGlobal,
                                       const nsAString& aOrigin,
                                       dom::OriginAttributesDictionary& aAttrs,
                                       ErrorResult& aRv)
{
  OriginAttributes attrs;
  nsAutoCString suffix;
  if (!attrs.PopulateFromOrigin(NS_ConvertUTF16toUTF8(aOrigin), suffix)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
  aAttrs = attrs;
}

/* static */ void
ChromeUtils::FillNonDefaultOriginAttributes(dom::GlobalObject& aGlobal,
                                 const dom::OriginAttributesDictionary& aAttrs,
                                 dom::OriginAttributesDictionary& aNewAttrs)
{
  aNewAttrs = aAttrs;
}


/* static */ bool
ChromeUtils::IsOriginAttributesEqual(dom::GlobalObject& aGlobal,
                                     const dom::OriginAttributesDictionary& aA,
                                     const dom::OriginAttributesDictionary& aB)
{
  return IsOriginAttributesEqual(aA, aB);
}

/* static */ bool
ChromeUtils::IsOriginAttributesEqual(const dom::OriginAttributesDictionary& aA,
                                     const dom::OriginAttributesDictionary& aB)
{
  return aA.mAppId == aB.mAppId &&
         aA.mInIsolatedMozBrowser == aB.mInIsolatedMozBrowser &&
         aA.mUserContextId == aB.mUserContextId &&
         aA.mPrivateBrowsingId == aB.mPrivateBrowsingId;
}

#ifdef NIGHTLY_BUILD
/* static */ void
ChromeUtils::GetRecentJSDevError(GlobalObject& aGlobal,
                                JS::MutableHandleValue aRetval,
                                ErrorResult& aRv)
{
  aRetval.setUndefined();
  auto runtime = CycleCollectedJSRuntime::Get();
  MOZ_ASSERT(runtime);

  auto cx = aGlobal.Context();
  if (!runtime->GetRecentDevError(cx, aRetval)) {
    aRv.NoteJSContextException(cx);
    return;
  }
}

/* static */ void
ChromeUtils::ClearRecentJSDevError(GlobalObject&)
{
  auto runtime = CycleCollectedJSRuntime::Get();
  MOZ_ASSERT(runtime);

  runtime->ClearRecentDevError();
}
#endif // NIGHTLY_BUILD

/* static */ void
ChromeUtils::RequestPerformanceMetrics(GlobalObject&)
{
  MOZ_ASSERT(XRE_IsParentProcess());

  // calling all content processes via IPDL (async)
  nsTArray<ContentParent*> children;
  ContentParent::GetAll(children);
  for (uint32_t i = 0; i < children.Length(); i++) {
    mozilla::Unused << children[i]->SendRequestPerformanceMetrics();
  }


  // collecting the current process counters and notifying them
  nsTArray<PerformanceInfo> info;
  CollectPerformanceInfo(info);
  SystemGroup::Dispatch(TaskCategory::Performance,
    NS_NewRunnableFunction(
      "RequestPerformanceMetrics",
      [info]() { mozilla::Unused << NS_WARN_IF(NS_FAILED(NotifyPerformanceInfo(info))); }
    )
  );

}

constexpr auto kSkipSelfHosted = JS::SavedFrameSelfHosted::Exclude;

/* static */ void
ChromeUtils::GetCallerLocation(const GlobalObject& aGlobal, nsIPrincipal* aPrincipal,
                               JS::MutableHandle<JSObject*> aRetval)
{
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
  aRetval.set(js::GetFirstSubsumedSavedFrame(cx, principals, frame, kSkipSelfHosted));
}

/* static */ void
ChromeUtils::CreateError(const GlobalObject& aGlobal, const nsAString& aMessage,
                         JS::Handle<JSObject*> aStack,
                         JS::MutableHandle<JSObject*> aRetVal, ErrorResult& aRv)
{
  if (aStack && !JS::IsSavedFrame(aStack)) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  JSContext* cx = aGlobal.Context();

  auto cleanup = MakeScopeExit([&]() {
    aRv.NoteJSContextException(cx);
  });

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

      if (JS::GetSavedFrameLine(cx, stack, &line) != JS::SavedFrameResult::Ok ||
          JS::GetSavedFrameColumn(cx, stack, &column) != JS::SavedFrameResult::Ok ||
          JS::GetSavedFrameSource(cx, stack, &fileName) != JS::SavedFrameResult::Ok) {
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
    if (!JS::CreateError(cx, JSEXN_ERR, stack,
                         fileName, line, column,
                         nullptr, message, &err)) {
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

/* static */ already_AddRefed<Promise>
ChromeUtils::RequestIOActivity(GlobalObject& aGlobal, ErrorResult& aRv)
{
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

} // namespace dom
} // namespace mozilla

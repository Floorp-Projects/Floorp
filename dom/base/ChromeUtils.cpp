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
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/IdleDeadline.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/dom/WindowBinding.h" // For IdleRequestCallback/Options
#include "nsThreadUtils.h"

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

    JSAutoCompartment ac(cx, obj);

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
    Maybe<JSAutoCompartment> ac;
    if (aTarget) {
      JS::RootedObject target(cx, js::CheckedUnwrap(aTarget));
      if (!target) {
        js::ReportAccessDenied(cx);
        return;
      }
      ac.emplace(cx, target);
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

} // namespace dom
} // namespace mozilla

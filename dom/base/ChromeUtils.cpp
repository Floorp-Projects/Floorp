/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromeUtils.h"

#include "mozilla/Base64.h"
#include "mozilla/BasePrincipal.h"

namespace mozilla {
namespace dom {

/* static */ void
ThreadSafeChromeUtils::NondeterministicGetWeakMapKeys(GlobalObject& aGlobal,
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
ThreadSafeChromeUtils::NondeterministicGetWeakSetKeys(GlobalObject& aGlobal,
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
ThreadSafeChromeUtils::Base64URLEncode(GlobalObject& aGlobal,
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
ThreadSafeChromeUtils::Base64URLDecode(GlobalObject& aGlobal,
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
ChromeUtils::OriginAttributesToSuffix(dom::GlobalObject& aGlobal,
                                      const dom::OriginAttributesDictionary& aAttrs,
                                      nsCString& aSuffix)

{
  GenericOriginAttributes attrs(aAttrs);
  attrs.CreateSuffix(aSuffix);
}

/* static */ bool
ChromeUtils::OriginAttributesMatchPattern(dom::GlobalObject& aGlobal,
                                          const dom::OriginAttributesDictionary& aAttrs,
                                          const dom::OriginAttributesPatternDictionary& aPattern)
{
  GenericOriginAttributes attrs(aAttrs);
  OriginAttributesPattern pattern(aPattern);
  return pattern.Matches(attrs);
}

/* static */ void
ChromeUtils::CreateOriginAttributesFromOrigin(dom::GlobalObject& aGlobal,
                                       const nsAString& aOrigin,
                                       dom::OriginAttributesDictionary& aAttrs,
                                       ErrorResult& aRv)
{
  GenericOriginAttributes attrs;
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
  return aA.mAddonId == aB.mAddonId &&
         aA.mAppId == aB.mAppId &&
         aA.mInIsolatedMozBrowser == aB.mInIsolatedMozBrowser &&
         aA.mSignedPkg == aB.mSignedPkg &&
         aA.mUserContextId == aB.mUserContextId &&
         aA.mPrivateBrowsingId == aB.mPrivateBrowsingId;
}

} // namespace dom
} // namespace mozilla

/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromeUtils.h"

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

} // namespace dom
} // namespace mozilla

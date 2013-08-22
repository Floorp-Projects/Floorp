/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Date.h"

#include "jsapi.h" // for JS_ObjectIsDate, JS_NewDateObjectMsec
#include "jsfriendapi.h" // for js_DateGetMsecSinceEpoch
#include "js/RootingAPI.h" // for Rooted, MutableHandle
#include "js/Value.h" // for Value
#include "jswrapper.h" // for CheckedUnwrap
#include "mozilla/FloatingPoint.h" // for IsNaN, UnspecifiedNaN

namespace mozilla {
namespace dom {

Date::Date()
  : mMsecSinceEpoch(UnspecifiedNaN())
{
}

bool
Date::IsUndefined() const
{
  return IsNaN(mMsecSinceEpoch);
}

bool
Date::SetTimeStamp(JSContext* aCx, JSObject* aObject)
{
  JS::Rooted<JSObject*> obj(aCx, aObject);
  MOZ_ASSERT(JS_ObjectIsDate(aCx, obj));

  obj = js::CheckedUnwrap(obj);
  // This really sucks: even if JS_ObjectIsDate, CheckedUnwrap can _still_ fail.
  if (!obj) {
    return false;
  }

  mMsecSinceEpoch = js_DateGetMsecSinceEpoch(obj);
  return true;
}

bool
Date::ToDateObject(JSContext* aCx, JS::MutableHandle<JS::Value> aRval) const
{
  JSObject* obj = JS_NewDateObjectMsec(aCx, mMsecSinceEpoch);
  if (!obj) {
    return false;
  }

  aRval.setObject(*obj);
  return true;
}

} // namespace dom
} // namespace mozilla

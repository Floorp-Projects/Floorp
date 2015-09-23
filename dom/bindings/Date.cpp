/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Date.h"

#include "jsapi.h" // for JS_ObjectIsDate
#include "jsfriendapi.h" // for DateGetMsecSinceEpoch
#include "js/Date.h" // for JS::NewDateObject, JS::ClippedTime, JS::TimeClip
#include "js/RootingAPI.h" // for Rooted, MutableHandle
#include "js/Value.h" // for Value
#include "mozilla/FloatingPoint.h" // for IsNaN, UnspecifiedNaN

namespace mozilla {
namespace dom {

bool
Date::SetTimeStamp(JSContext* aCx, JSObject* aObject)
{
  JS::Rooted<JSObject*> obj(aCx, aObject);

  double msecs;
  if (!js::DateGetMsecSinceEpoch(aCx, obj, &msecs)) {
    return false;
  }

  JS::ClippedTime time = JS::TimeClip(msecs);
  MOZ_ASSERT(NumbersAreIdentical(msecs, time.toDouble()));

  mMsecSinceEpoch = time;
  return true;
}

bool
Date::ToDateObject(JSContext* aCx, JS::MutableHandle<JS::Value> aRval) const
{
  JSObject* obj = JS::NewDateObject(aCx, mMsecSinceEpoch);
  if (!obj) {
    return false;
  }

  aRval.setObject(*obj);
  return true;
}

} // namespace dom
} // namespace mozilla

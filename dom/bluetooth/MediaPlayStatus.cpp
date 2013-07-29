/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/ */

#include "BluetoothCommon.h"
#include "MediaPlayStatus.h"

#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "nsJSUtils.h"
#include "nsThreadUtils.h"

using namespace mozilla;
USING_BLUETOOTH_NAMESPACE

MediaPlayStatus::MediaPlayStatus() : mDuration(-1)
                                     , mPosition(-1)
{
}

nsresult
MediaPlayStatus::Init(JSContext* aCx, const jsval* aVal)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!aCx || !aVal) {
    return NS_OK;
  }

  if (!aVal->isObject()) {
    return aVal->isNullOrUndefined() ? NS_OK : NS_ERROR_TYPE_ERR;
  }

  JS::RootedObject obj(aCx, &aVal->toObject());
  nsCxPusher pusher;
  pusher.Push(aCx);
  JSAutoCompartment ac(aCx, obj);

  JS::Value value;
  NS_ENSURE_STATE(JS_GetProperty(aCx, obj, "mDuration", &value));
  if (JSVAL_IS_INT(value)) {
    NS_ENSURE_STATE(JS_ValueToInt64(aCx, value, &mDuration));
  }

  NS_ENSURE_STATE(JS_GetProperty(aCx, obj, "mPlayStatus", &value));
  if (JSVAL_IS_STRING(value)) {
    nsDependentJSString jsString;
    NS_ENSURE_STATE(JSVAL_IS_STRING(value));
    NS_ENSURE_STATE(jsString.init(aCx, value.toString()));
    mPlayStatus = jsString;
  }

  NS_ENSURE_STATE(JS_GetProperty(aCx, obj, "mPosition", &value));
  if (JSVAL_IS_INT(value)) {
    NS_ENSURE_STATE(JS_ValueToInt64(aCx, value, &mPosition));
  }

  return NS_OK;
}


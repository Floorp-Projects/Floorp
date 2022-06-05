/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentFrameMessageManager.h"
#include "js/RootingAPI.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/ScriptSettings.h"

using namespace mozilla;
using namespace mozilla::dom;

JSObject* ContentFrameMessageManager::GetOrCreateWrapper() {
  JS::Rooted<JS::Value> val(RootingCx());
  {
    // Scope to run ~AutoJSAPI before working with a raw JSObject*.
    AutoJSAPI jsapi;
    jsapi.Init();

    if (!GetOrCreateDOMReflectorNoWrap(jsapi.cx(), this, &val)) {
      return nullptr;
    }
  }
  MOZ_ASSERT(val.isObject());
  return &val.toObject();
}

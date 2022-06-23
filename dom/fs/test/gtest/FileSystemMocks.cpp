/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemMocks.h"

#include "jsapi.h"
#include "nsISupports.h"
#include "js/RootingAPI.h"

namespace mozilla::dom::fs::test {

nsIGlobalObject* GetGlobal() {
  AutoJSAPI jsapi;
  DebugOnly<bool> ok = jsapi.Init(xpc::PrivilegedJunkScope());
  MOZ_ASSERT(ok);

  JSContext* cx = jsapi.cx();
  mozilla::dom::GlobalObject globalObject(cx, JS::CurrentGlobalOrNull(cx));
  nsCOMPtr<nsIGlobalObject> global =
      do_QueryInterface(globalObject.GetAsSupports());
  MOZ_ASSERT(global);

  return global.get();
}

}  // namespace mozilla::dom::fs::test

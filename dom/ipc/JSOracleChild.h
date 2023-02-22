/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_JSOracleChild
#define mozilla_dom_JSOracleChild

#include "mozilla/dom/PJSOracleChild.h"

#include "js/CharacterEncoding.h"
#include "js/HeapAPI.h"
#include "js/Initialization.h"
#include "jsapi.h"
#include "js/CompilationAndEvaluation.h"
#include "js/Context.h"

namespace mozilla::ipc {
class UtilityProcessParent;
}

namespace mozilla::dom {
struct JSContextHolder {
  JSContextHolder() {
    MOZ_RELEASE_ASSERT(JS_IsInitialized(),
                       "UtilityProcessChild::Init should have JS initialized");

    mCx = JS_NewContext(JS::DefaultHeapMaxBytes);
    if (!mCx) {
      MOZ_CRASH("Failed to create JS Context");
      return;
    }

    if (!JS::InitSelfHostedCode(mCx)) {
      MOZ_CRASH("Failed to initialize the runtime's self-hosted code");
      return;
    }

    static JSClass jsValidatorGlobalClass = {
        "JSValidatorGlobal", JSCLASS_GLOBAL_FLAGS, &JS::DefaultGlobalClassOps};

    JS::Rooted<JSObject*> global(
        mCx, JS_NewGlobalObject(mCx, &jsValidatorGlobalClass, nullptr,
                                JS::FireOnNewGlobalHook, JS::RealmOptions()));

    if (!global) {
      MOZ_CRASH("Failed to create the global");
      return;
    }

    mGlobal.init(mCx, global);
  }

  ~JSContextHolder() {
    if (mCx) {
      JS_DestroyContext(mCx);
    }
  }

  static void MaybeInit();

  JSContext* mCx;
  JS::PersistentRooted<JSObject*> mGlobal;
};

class PJSValidatorChild;

class JSOracleChild final : public PJSOracleChild {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(JSOracleChild, override);

  already_AddRefed<PJSValidatorChild> AllocPJSValidatorChild();

  void Start(Endpoint<PJSOracleChild>&& aEndpoint);

  static struct JSContext* JSContext();
  static class JSObject* JSObject();

 private:
  ~JSOracleChild() = default;

  static JSOracleChild* GetSingleton();
};
}  // namespace mozilla::dom

#endif  // defined(mozilla_dom_JSOracleChild)

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_JSOracleChild
#define mozilla_dom_JSOracleChild

#include "mozilla/dom/PJSOracleChild.h"

#include "js/experimental/JSStencil.h"
#include "js/experimental/CompileScript.h"
#include "js/Initialization.h"
#include "jsapi.h"

namespace mozilla::ipc {
class UtilityProcessParent;
}

namespace mozilla::dom {
struct JSFrontendContextHolder {
  JSFrontendContextHolder() {
    MOZ_RELEASE_ASSERT(JS_IsInitialized(),
                       "UtilityProcessChild::Init should have JS initialized");

    mFc = JS::NewFrontendContext();
    if (!mFc) {
      MOZ_CRASH("Failed to create JS FrontendContext");
      return;
    }

    // See the comment in XPCJSContext::Initialize.
    const size_t kDefaultStackQuota = 128 * sizeof(size_t) * 1024;

    JS::SetNativeStackQuota(mFc, kDefaultStackQuota);
  }

  ~JSFrontendContextHolder() {
    if (mFc) {
      JS::DestroyFrontendContext(mFc);
    }
  }

  static void MaybeInit();

  JS::FrontendContext* mFc;
};

class PJSValidatorChild;

class JSOracleChild final : public PJSOracleChild {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(JSOracleChild, override);

  already_AddRefed<PJSValidatorChild> AllocPJSValidatorChild();

  void Start(Endpoint<PJSOracleChild>&& aEndpoint);

  static JS::FrontendContext* JSFrontendContext();

 private:
  ~JSOracleChild() = default;

  static JSOracleChild* GetSingleton();
};
}  // namespace mozilla::dom

#endif  // defined(mozilla_dom_JSOracleChild)

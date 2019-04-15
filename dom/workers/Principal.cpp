/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Principal.h"

#include "jsapi.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/StructuredCloneTags.h"

namespace mozilla {
namespace dom {

struct WorkerPrincipal final : public JSPrincipals {
  bool write(JSContext* aCx, JSStructuredCloneWriter* aWriter) override {
    return JS_WriteUint32Pair(aWriter, SCTAG_DOM_WORKER_PRINCIPAL, 0);
  }
};

JSPrincipals* GetWorkerPrincipal() {
  static WorkerPrincipal sPrincipal;

  /*
   * To make sure the the principals refcount is initialized to one, atomically
   * increment it on every pass though this function. If we discover this wasn't
   * the first time, decrement it again. This avoids the need for
   * synchronization.
   */
  int32_t prevRefcount = sPrincipal.refcount++;
  if (prevRefcount > 0) {
    --sPrincipal.refcount;
  } else {
#ifdef DEBUG
    sPrincipal.debugToken = workerinternals::kJSPrincipalsDebugToken;
#endif
  }

  return &sPrincipal;
}

void DestroyWorkerPrincipals(JSPrincipals* aPrincipals) {
  MOZ_ASSERT_UNREACHABLE(
      "Worker principals refcount should never fall below one");
}

}  // namespace dom
}  // namespace mozilla

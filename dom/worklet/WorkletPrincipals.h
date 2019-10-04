/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_WORKLET_WORKLETPRINCIPALS_H_
#define DOM_WORKLET_WORKLETPRINCIPALS_H_

#include "js/Principals.h"
#include "mozilla/RefPtr.h"

namespace mozilla {

class WorkletImpl;

namespace dom {

struct MOZ_HEAP_CLASS WorkletPrincipals final : public JSPrincipals {
  // A new WorkletPrincipals has refcount zero.
  explicit WorkletPrincipals(WorkletImpl* aWorkletImpl);

  bool write(JSContext* aCx, JSStructuredCloneWriter* aWriter) override;

  bool isSystemOrAddonPrincipal() override;

  // Callback for JS_InitDestroyPrincipalsCallback()
  static void Destroy(JSPrincipals* aPrincipals);

  static const uint32_t kJSPrincipalsDebugToken = 0x7e2df9f4;

 private:
  ~WorkletPrincipals();
  RefPtr<WorkletImpl> mWorkletImpl;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_WORKLET_WORKLETPRINCIPALS_H_

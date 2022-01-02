/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_principal_h__
#define mozilla_dom_workers_principal_h__

#include "js/Principals.h"

namespace mozilla {
namespace dom {

struct MOZ_HEAP_CLASS WorkerPrincipal final : public JSPrincipals {
  explicit WorkerPrincipal(bool aIsSystemOrAddonPrincipal);

  bool write(JSContext* aCx, JSStructuredCloneWriter* aWriter) override;

  // We don't distinguish between System or Addon because the only use
  // case for this right now doesn't need to. When you need to distinguish
  // add a second bool.
  bool isSystemOrAddonPrincipal() override;

  // Callback for JS_InitDestroyPrincipalsCallback()
  static void Destroy(JSPrincipals* aPrincipals);

 private:
  ~WorkerPrincipal();
  bool mIsSystemOrAddonPrincipal;
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_workers_principal_h__ */

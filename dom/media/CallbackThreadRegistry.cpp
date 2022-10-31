/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CallbackThreadRegistry.h"
#include "mozilla/ClearOnShutdown.h"

namespace mozilla {
CallbackThreadRegistry::CallbackThreadRegistry()
    : mThreadIds("CallbackThreadRegistry::mThreadIds") {}

struct CallbackThreadRegistrySingleton {
  CallbackThreadRegistrySingleton()
      : mRegistry(MakeUnique<CallbackThreadRegistry>()) {
    NS_DispatchToMainThread(
        NS_NewRunnableFunction(__func__, [registry = &mRegistry] {
          const auto phase = ShutdownPhase::XPCOMShutdownFinal;
          MOZ_DIAGNOSTIC_ASSERT(!PastShutdownPhase(phase));
          ClearOnShutdown(registry, phase);
        }));
  }

  UniquePtr<CallbackThreadRegistry> mRegistry;
};

/* static */
CallbackThreadRegistry* CallbackThreadRegistry::Get() {
  static CallbackThreadRegistrySingleton sSingleton;
  return sSingleton.mRegistry.get();
}

}  // namespace mozilla

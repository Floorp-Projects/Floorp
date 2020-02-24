/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ChildProcessChannelListener.h"

namespace mozilla {
namespace dom {

static StaticRefPtr<ChildProcessChannelListener> sCPCLSingleton;

void ChildProcessChannelListener::RegisterCallback(uint64_t aIdentifier,
                                                   Callback&& aCallback) {
  if (auto args = mChannelArgs.GetAndRemove(aIdentifier)) {
    aCallback(args->mLoadState, std::move(args->mRedirects), args->mTiming);
  } else {
    mCallbacks.Put(aIdentifier, std::move(aCallback));
  }
}

void ChildProcessChannelListener::OnChannelReady(
    nsDocShellLoadState* aLoadState, uint64_t aIdentifier,
    nsTArray<net::DocumentChannelRedirect>&& aRedirects,
    nsDOMNavigationTiming* aTiming) {
  if (auto callback = mCallbacks.GetAndRemove(aIdentifier)) {
    (*callback)(aLoadState, std::move(aRedirects), aTiming);
  } else {
    mChannelArgs.Put(aIdentifier, {aLoadState, std::move(aRedirects), aTiming});
  }
}

already_AddRefed<ChildProcessChannelListener>
ChildProcessChannelListener::GetSingleton() {
  if (!sCPCLSingleton) {
    sCPCLSingleton = new ChildProcessChannelListener();
    ClearOnShutdown(&sCPCLSingleton);
  }
  RefPtr<ChildProcessChannelListener> cpcl = sCPCLSingleton;
  return cpcl.forget();
}

}  // namespace dom
}  // namespace mozilla

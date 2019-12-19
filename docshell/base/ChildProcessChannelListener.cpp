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
    aCallback(args->mChannel, std::move(args->mRedirects),
              args->mLoadStateLoadFlags);
  } else {
    mCallbacks.Put(aIdentifier, std::move(aCallback));
  }
}

void ChildProcessChannelListener::OnChannelReady(
    nsIChannel* aChannel, uint64_t aIdentifier,
    nsTArray<net::DocumentChannelRedirect>&& aRedirects,
    uint32_t aLoadStateLoadFlags) {
  if (auto callback = mCallbacks.GetAndRemove(aIdentifier)) {
    (*callback)(aChannel, std::move(aRedirects), aLoadStateLoadFlags);
  } else {
    mChannelArgs.Put(aIdentifier,
                     {aChannel, std::move(aRedirects), aLoadStateLoadFlags});
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

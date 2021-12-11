/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ChildProcessChannelListener_h
#define mozilla_dom_ChildProcessChannelListener_h

#include <functional>

#include "mozilla/extensions/StreamFilterParent.h"
#include "mozilla/net/NeckoChannelParams.h"
#include "nsDOMNavigationTiming.h"
#include "nsTHashMap.h"
#include "nsIChannel.h"
#include "mozilla/ipc/BackgroundUtils.h"

namespace mozilla {
namespace dom {

class ChildProcessChannelListener final {
  NS_INLINE_DECL_REFCOUNTING(ChildProcessChannelListener)

  using Endpoint = mozilla::ipc::Endpoint<extensions::PStreamFilterParent>;
  using Resolver = std::function<void(const nsresult&)>;
  using Callback = std::function<nsresult(
      nsDocShellLoadState*, nsTArray<Endpoint>&&, nsDOMNavigationTiming*)>;

  void RegisterCallback(uint64_t aIdentifier, Callback&& aCallback);

  void OnChannelReady(nsDocShellLoadState* aLoadState, uint64_t aIdentifier,
                      nsTArray<Endpoint>&& aStreamFilterEndpoints,
                      nsDOMNavigationTiming* aTiming, Resolver&& aResolver);

  static already_AddRefed<ChildProcessChannelListener> GetSingleton();

 private:
  ChildProcessChannelListener() = default;
  ~ChildProcessChannelListener();
  struct CallbackArgs {
    RefPtr<nsDocShellLoadState> mLoadState;
    nsTArray<Endpoint> mStreamFilterEndpoints;
    RefPtr<nsDOMNavigationTiming> mTiming;
    Resolver mResolver;
  };

  // TODO Backtrack.
  nsTHashMap<nsUint64HashKey, Callback> mCallbacks;
  nsTHashMap<nsUint64HashKey, CallbackArgs> mChannelArgs;
};

}  // namespace dom
}  // namespace mozilla

#endif  // !defined(mozilla_dom_ChildProcessChannelListener_h)

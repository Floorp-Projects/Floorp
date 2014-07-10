/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NetworkWorker_h
#define NetworkWorker_h

#include "mozilla/dom/NetworkOptionsBinding.h"
#include "mozilla/ipc/Netd.h"
#include "nsINetworkWorker.h"
#include "nsCOMPtr.h"
#include "nsThread.h"

namespace mozilla {

class NetworkWorker MOZ_FINAL : public nsINetworkWorker
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSINETWORKWORKER

  static already_AddRefed<NetworkWorker> FactoryCreate();

  void DispatchNetworkResult(const mozilla::dom::NetworkResultOptions& aOptions);

private:
  NetworkWorker();
  ~NetworkWorker();

  static void NotifyResult(mozilla::dom::NetworkResultOptions& aResult);

  nsCOMPtr<nsINetworkEventListener> mListener;
};

} // namespace mozilla

#endif  // NetworkWorker_h

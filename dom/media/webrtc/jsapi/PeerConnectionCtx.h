/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef peerconnectionctx_h___h__
#define peerconnectionctx_h___h__

#include <map>
#include <string>

#include "MediaTransportHandler.h"  // Mostly for IceLogPromise
#include "mozIGeckoMediaPluginService.h"
#include "mozilla/Attributes.h"
#include "mozilla/StaticPtr.h"
#include "nsIRunnable.h"
#include "PeerConnectionImpl.h"

namespace mozilla {
class PeerConnectionCtxObserver;

namespace dom {
class WebrtcGlobalInformation;
}

// A class to hold some of the singleton objects we need:
// * The global PeerConnectionImpl table and its associated lock.
// * Stats report objects for PCs that are gone
// * GMP related state
class PeerConnectionCtx {
 public:
  static nsresult InitializeGlobal(nsIThread* mainThread);
  static PeerConnectionCtx* GetInstance();
  static bool isActive();
  static void Destroy();

  bool isReady() {
    // If mGMPService is not set, we aren't using GMP.
    if (mGMPService) {
      return mGMPReady;
    }
    return true;
  }

  void queueJSEPOperation(nsIRunnable* aJSEPOperation);
  void onGMPReady();

  bool gmpHasH264();

  static void UpdateNetworkState(bool online);

  RefPtr<MediaTransportHandler> GetTransportHandler() const {
    return mTransportHandler;
  }

  // WebrtcGlobalInformation uses this; we put it here so we don't need to
  // create another shutdown observer class.
  mozilla::dom::Sequence<mozilla::dom::RTCStatsReportInternal>
      mStatsForClosedPeerConnections;

  void RemovePeerConnection(const std::string& aKey);
  void AddPeerConnection(const std::string& aKey,
                         PeerConnectionImpl* aPeerConnection);
  PeerConnectionImpl* GetPeerConnection(const std::string& aKey) const;
  template <typename Function>
  void ForEachPeerConnection(Function&& aFunction) const;

 private:
  std::map<const std::string, PeerConnectionImpl*> mPeerConnections;

  PeerConnectionCtx()
      : mGMPReady(false),
        mTransportHandler(
            MediaTransportHandler::Create(GetMainThreadSerialEventTarget())) {}

  // This is a singleton, so don't copy construct it, etc.
  PeerConnectionCtx(const PeerConnectionCtx& other) = delete;
  void operator=(const PeerConnectionCtx& other) = delete;
  virtual ~PeerConnectionCtx();

  nsresult Initialize();
  nsresult Cleanup();

  void initGMP();

  static void EverySecondTelemetryCallback_m(nsITimer* timer, void*);

  nsCOMPtr<nsITimer> mTelemetryTimer;

 private:
  void DeliverStats(UniquePtr<dom::RTCStatsReportInternal>&& aReport);

  std::map<nsString, UniquePtr<dom::RTCStatsReportInternal>> mLastReports;
  // We cannot form offers/answers properly until the Gecko Media Plugin stuff
  // has been initted, which is a complicated mess of thread dispatches,
  // including sync dispatches to main. So, we need to be able to queue up
  // offer creation (or SetRemote, when we're the answerer) until all of this is
  // ready to go, since blocking on this init is just begging for deadlock.
  nsCOMPtr<mozIGeckoMediaPluginService> mGMPService;
  bool mGMPReady;
  nsTArray<nsCOMPtr<nsIRunnable>> mQueuedJSEPOperations;

  // Not initted, just for ICE logging stuff
  RefPtr<MediaTransportHandler> mTransportHandler;

  static PeerConnectionCtx* gInstance;

 public:
  static nsIThread* gMainThread;
  static mozilla::StaticRefPtr<mozilla::PeerConnectionCtxObserver>
      gPeerConnectionCtxObserver;
};

}  // namespace mozilla

#endif

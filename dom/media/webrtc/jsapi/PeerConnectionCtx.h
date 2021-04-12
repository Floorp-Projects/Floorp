/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef peerconnectionctx_h___h__
#define peerconnectionctx_h___h__

#include <map>
#include <string>

#include "api/scoped_refptr.h"
#include "call/audio_state.h"
#include "MediaTransportHandler.h"  // Mostly for IceLogPromise
#include "mozIGeckoMediaPluginService.h"
#include "mozilla/Attributes.h"
#include "mozilla/StaticPtr.h"
#include "nsIRunnable.h"
#include "PeerConnectionImpl.h"

namespace webrtc {
class AudioDecoderFactory;
class SharedModuleThread;
class WebRtcKeyValueConfig;
}  // namespace webrtc

namespace mozilla {
class PeerConnectionCtxObserver;

namespace dom {
class WebrtcGlobalInformation;
}

/**
 * Refcounted class containing state shared across all PeerConnections and all
 * Call instances. Managed by PeerConnectionCtx, and kept around while there are
 * registered peer connections.
 */
class SharedWebrtcState {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SharedWebrtcState)

  SharedWebrtcState(RefPtr<AbstractThread> aCallWorkerThread,
                    webrtc::AudioState::Config&& aAudioStateConfig,
                    RefPtr<webrtc::AudioDecoderFactory> aAudioDecoderFactory);

  webrtc::SharedModuleThread* GetModuleThread();

  // A global Call worker thread shared between all Call instances. Implements
  // AbstractThread for running tasks that call into a Call instance through its
  // webrtc::TaskQueue member, and for using AbstractThread-specific higher
  // order constructs like StateMirroring.
  const RefPtr<AbstractThread> mCallWorkerThread;

  // AudioState config containing dummy implementations of the audio stack,
  // since we use our own audio stack instead. Shared across all Call instances.
  const webrtc::AudioState::Config mAudioStateConfig;

  // AudioDecoderFactory instance shared between calls, to limit the number of
  // instances in large calls.
  const RefPtr<webrtc::AudioDecoderFactory> mAudioDecoderFactory;

 private:
  virtual ~SharedWebrtcState();

  // SharedModuleThread used for processing in all Call instances.
  // Only accessed on the global Call worker task queue. Set on first
  // GetModuleThread(), Unset on the last external Release.
  rtc::scoped_refptr<webrtc::SharedModuleThread> mModuleThread;
};

// A class to hold some of the singleton objects we need:
// * The global PeerConnectionImpl table and its associated lock.
// * Stats report objects for PCs that are gone
// * GMP related state
// * Upstream webrtc state shared across all Calls (processing thread)
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

  SharedWebrtcState* GetSharedWebrtcState() const;

  webrtc::WebRtcKeyValueConfig* GetTrials() const { return mTrials.get(); }

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

  // State used by libwebrtc that needs to be shared across all PeerConnections
  // and all Call instances. Set while there is at least one peer connection
  // registered.
  RefPtr<SharedWebrtcState> mSharedWebrtcState;

  // Trials instance shared between calls, to limit the number of instances in
  // large calls. Note that this has to outlive the Call instances owned by
  // PeerConnections, and as such is not unset prior to the ctx dtor.
  UniquePtr<webrtc::WebRtcKeyValueConfig> mTrials;

  static PeerConnectionCtx* gInstance;

 public:
  static nsIThread* gMainThread;
  static mozilla::StaticRefPtr<mozilla::PeerConnectionCtxObserver>
      gPeerConnectionCtxObserver;
};

}  // namespace mozilla

#endif

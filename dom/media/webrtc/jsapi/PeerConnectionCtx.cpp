/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PeerConnectionCtx.h"

#include "WebrtcGlobalStatsHistory.h"
#include "api/audio/audio_mixer.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "call/audio_state.h"
#include "common/browser_logging/CSFLog.h"
#include "common/browser_logging/WebRtcLog.h"
#include "gmp-video-decode.h"  // GMP_API_VIDEO_DECODER
#include "gmp-video-encode.h"  // GMP_API_VIDEO_ENCODER
#include "libwebrtcglue/CallWorkerThread.h"
#include "modules/audio_device/include/fake_audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/include/aec_dump.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/RTCStatsReportBinding.h"
#include "nsCRTGlue.h"
#include "nsIIOService.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"  // do_GetService
#include "PeerConnectionImpl.h"
#include "transport/runnable_utils.h"
#include "WebrtcGlobalChild.h"
#include "WebrtcGlobalInformation.h"

static const char* pccLogTag = "PeerConnectionCtx";
#ifdef LOGTAG
#  undef LOGTAG
#endif
#define LOGTAG pccLogTag

using namespace webrtc;

namespace {
class DummyAudioMixer : public AudioMixer {
 public:
  bool AddSource(Source*) override { return true; }
  void RemoveSource(Source*) override {}
  void Mix(size_t, AudioFrame*) override { MOZ_CRASH("Unexpected call"); }
};

class DummyAudioProcessing : public AudioProcessing {
 public:
  int Initialize() override {
    MOZ_CRASH("Unexpected call");
    return kNoError;
  }
  int Initialize(const ProcessingConfig&) override { return Initialize(); }
  void ApplyConfig(const Config&) override { MOZ_CRASH("Unexpected call"); }
  int proc_sample_rate_hz() const override {
    MOZ_CRASH("Unexpected call");
    return 0;
  }
  int proc_split_sample_rate_hz() const override {
    MOZ_CRASH("Unexpected call");
    return 0;
  }
  size_t num_input_channels() const override {
    MOZ_CRASH("Unexpected call");
    return 0;
  }
  size_t num_proc_channels() const override {
    MOZ_CRASH("Unexpected call");
    return 0;
  }
  size_t num_output_channels() const override {
    MOZ_CRASH("Unexpected call");
    return 0;
  }
  size_t num_reverse_channels() const override {
    MOZ_CRASH("Unexpected call");
    return 0;
  }
  void set_output_will_be_muted(bool) override { MOZ_CRASH("Unexpected call"); }
  void SetRuntimeSetting(RuntimeSetting) override {
    MOZ_CRASH("Unexpected call");
  }
  bool PostRuntimeSetting(RuntimeSetting setting) override { return false; }
  int ProcessStream(const int16_t* const, const StreamConfig&,
                    const StreamConfig&, int16_t* const) override {
    MOZ_CRASH("Unexpected call");
    return kNoError;
  }
  int ProcessStream(const float* const*, const StreamConfig&,
                    const StreamConfig&, float* const*) override {
    MOZ_CRASH("Unexpected call");
    return kNoError;
  }
  int ProcessReverseStream(const int16_t* const, const StreamConfig&,
                           const StreamConfig&, int16_t* const) override {
    MOZ_CRASH("Unexpected call");
    return kNoError;
  }
  int ProcessReverseStream(const float* const*, const StreamConfig&,
                           const StreamConfig&, float* const*) override {
    MOZ_CRASH("Unexpected call");
    return kNoError;
  }
  int AnalyzeReverseStream(const float* const*, const StreamConfig&) override {
    MOZ_CRASH("Unexpected call");
    return kNoError;
  }
  bool GetLinearAecOutput(
      rtc::ArrayView<std::array<float, 160>>) const override {
    MOZ_CRASH("Unexpected call");
    return false;
  }
  void set_stream_analog_level(int) override { MOZ_CRASH("Unexpected call"); }
  int recommended_stream_analog_level() const override {
    MOZ_CRASH("Unexpected call");
    return -1;
  }
  int set_stream_delay_ms(int) override {
    MOZ_CRASH("Unexpected call");
    return kNoError;
  }
  int stream_delay_ms() const override {
    MOZ_CRASH("Unexpected call");
    return 0;
  }
  void set_stream_key_pressed(bool) override { MOZ_CRASH("Unexpected call"); }
  bool CreateAndAttachAecDump(absl::string_view, int64_t,
                              rtc::TaskQueue*) override {
    MOZ_CRASH("Unexpected call");
    return false;
  }
  bool CreateAndAttachAecDump(FILE*, int64_t, rtc::TaskQueue*) override {
    MOZ_CRASH("Unexpected call");
    return false;
  }
  void AttachAecDump(std::unique_ptr<AecDump>) override {
    MOZ_CRASH("Unexpected call");
  }
  void DetachAecDump() override { MOZ_CRASH("Unexpected call"); }
  AudioProcessingStats GetStatistics() override {
    return AudioProcessingStats();
  }
  AudioProcessingStats GetStatistics(bool) override { return GetStatistics(); }
  AudioProcessing::Config GetConfig() const override {
    MOZ_CRASH("Unexpected call");
    return Config();
  }
};
}  // namespace

namespace mozilla {

using namespace dom;

SharedWebrtcState::SharedWebrtcState(
    RefPtr<AbstractThread> aCallWorkerThread,
    webrtc::AudioState::Config&& aAudioStateConfig,
    RefPtr<webrtc::AudioDecoderFactory> aAudioDecoderFactory,
    UniquePtr<webrtc::FieldTrialsView> aTrials)
    : mCallWorkerThread(std::move(aCallWorkerThread)),
      mAudioStateConfig(std::move(aAudioStateConfig)),
      mAudioDecoderFactory(std::move(aAudioDecoderFactory)),
      mTrials(std::move(aTrials)) {}

SharedWebrtcState::~SharedWebrtcState() = default;

class PeerConnectionCtxObserver : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS

  PeerConnectionCtxObserver() {}

  void Init() {
    nsCOMPtr<nsIObserverService> observerService =
        services::GetObserverService();
    if (!observerService) return;

    nsresult rv = NS_OK;

    rv = observerService->AddObserver(this, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID,
                                      false);
    MOZ_ALWAYS_SUCCEEDS(rv);
    rv = observerService->AddObserver(this, NS_IOSERVICE_OFFLINE_STATUS_TOPIC,
                                      false);
    MOZ_ALWAYS_SUCCEEDS(rv);
    (void)rv;
  }

  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) override {
    if (strcmp(aTopic, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID) == 0) {
      CSFLogDebug(LOGTAG, "Shutting down PeerConnectionCtx");
      PeerConnectionCtx::Destroy();

      nsCOMPtr<nsIObserverService> observerService =
          services::GetObserverService();
      if (!observerService) return NS_ERROR_FAILURE;

      nsresult rv = observerService->RemoveObserver(
          this, NS_IOSERVICE_OFFLINE_STATUS_TOPIC);
      MOZ_ALWAYS_SUCCEEDS(rv);
      rv = observerService->RemoveObserver(this,
                                           NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID);
      MOZ_ALWAYS_SUCCEEDS(rv);

      // Make sure we're not deleted while still inside ::Observe()
      RefPtr<PeerConnectionCtxObserver> kungFuDeathGrip(this);
      PeerConnectionCtx::gPeerConnectionCtxObserver = nullptr;
    }
    if (strcmp(aTopic, NS_IOSERVICE_OFFLINE_STATUS_TOPIC) == 0) {
      if (NS_strcmp(aData, u"" NS_IOSERVICE_OFFLINE) == 0) {
        CSFLogDebug(LOGTAG, "Updating network state to offline");
        PeerConnectionCtx::UpdateNetworkState(false);
      } else if (NS_strcmp(aData, u"" NS_IOSERVICE_ONLINE) == 0) {
        CSFLogDebug(LOGTAG, "Updating network state to online");
        PeerConnectionCtx::UpdateNetworkState(true);
      } else {
        CSFLogDebug(LOGTAG, "Received unsupported network state event");
        MOZ_CRASH();
      }
    }
    return NS_OK;
  }

 private:
  virtual ~PeerConnectionCtxObserver() {
    nsCOMPtr<nsIObserverService> observerService =
        services::GetObserverService();
    if (observerService) {
      observerService->RemoveObserver(this, NS_IOSERVICE_OFFLINE_STATUS_TOPIC);
      observerService->RemoveObserver(this, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID);
    }
  }
};

NS_IMPL_ISUPPORTS(PeerConnectionCtxObserver, nsIObserver);

PeerConnectionCtx* PeerConnectionCtx::gInstance;
StaticRefPtr<PeerConnectionCtxObserver>
    PeerConnectionCtx::gPeerConnectionCtxObserver;

nsresult PeerConnectionCtx::InitializeGlobal() {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult res;

  if (!gInstance) {
    CSFLogDebug(LOGTAG, "Creating PeerConnectionCtx");
    PeerConnectionCtx* ctx = new PeerConnectionCtx();

    res = ctx->Initialize();
    PR_ASSERT(NS_SUCCEEDED(res));
    if (!NS_SUCCEEDED(res)) return res;

    gInstance = ctx;

    if (!PeerConnectionCtx::gPeerConnectionCtxObserver) {
      PeerConnectionCtx::gPeerConnectionCtxObserver =
          new PeerConnectionCtxObserver();
      PeerConnectionCtx::gPeerConnectionCtxObserver->Init();
    }
  }

  return NS_OK;
}

PeerConnectionCtx* PeerConnectionCtx::GetInstance() {
  MOZ_ASSERT(gInstance);
  return gInstance;
}

bool PeerConnectionCtx::isActive() { return gInstance; }

void PeerConnectionCtx::Destroy() {
  CSFLogDebug(LOGTAG, "%s", __FUNCTION__);

  if (gInstance) {
    // Null out gInstance first, so PeerConnectionImpl doesn't try to use it
    // in Cleanup.
    auto* instance = gInstance;
    gInstance = nullptr;
    instance->Cleanup();
    delete instance;
  }
}

template <typename T>
static void RecordCommonRtpTelemetry(const T& list, const T& lastList,
                                     const bool isRemote) {
  using namespace Telemetry;
  for (const auto& s : list) {
    const bool isAudio = s.mKind.Find(u"audio") != -1;
    if (s.mPacketsLost.WasPassed() && s.mPacketsReceived.WasPassed()) {
      if (const uint64_t total =
              s.mPacketsLost.Value() + s.mPacketsReceived.Value()) {
        HistogramID id =
            isRemote ? (isAudio ? WEBRTC_AUDIO_QUALITY_OUTBOUND_PACKETLOSS_RATE
                                : WEBRTC_VIDEO_QUALITY_OUTBOUND_PACKETLOSS_RATE)
                     : (isAudio ? WEBRTC_AUDIO_QUALITY_INBOUND_PACKETLOSS_RATE
                                : WEBRTC_VIDEO_QUALITY_INBOUND_PACKETLOSS_RATE);
        // Because this is an integer and we would like some extra precision
        // the unit is per mille (1/1000) instead of percent (1/100).
        Accumulate(id, (s.mPacketsLost.Value() * 1000) / total);
      }
    }
    if (s.mJitter.WasPassed()) {
      HistogramID id = isRemote
                           ? (isAudio ? WEBRTC_AUDIO_QUALITY_OUTBOUND_JITTER
                                      : WEBRTC_VIDEO_QUALITY_OUTBOUND_JITTER)
                           : (isAudio ? WEBRTC_AUDIO_QUALITY_INBOUND_JITTER
                                      : WEBRTC_VIDEO_QUALITY_INBOUND_JITTER);
      Accumulate(id, s.mJitter.Value() * 1000);
    }
  }
}

// Telemetry reporting every second after start of first call.
// The threading model around the media pipelines is weird:
// - The pipelines are containers,
// - containers that are only safe on main thread, with members only safe on
//   STS,
// - hence the there and back again approach.

void PeerConnectionCtx::DeliverStats(
    UniquePtr<dom::RTCStatsReportInternal>&& aReport) {
  using namespace Telemetry;

  // First, get reports from a second ago, if any, for calculations below
  UniquePtr<dom::RTCStatsReportInternal> lastReport;
  {
    auto i = mLastReports.find(aReport->mPcid);
    if (i != mLastReports.end()) {
      lastReport = std::move(i->second);
    } else {
      lastReport = MakeUnique<dom::RTCStatsReportInternal>();
    }
  }
  // Record Telemetery
  RecordCommonRtpTelemetry(aReport->mInboundRtpStreamStats,
                           lastReport->mInboundRtpStreamStats, false);
  // Record bandwidth telemetry
  for (const auto& s : aReport->mInboundRtpStreamStats) {
    if (s.mBytesReceived.WasPassed()) {
      const bool isAudio = s.mKind.Find(u"audio") != -1;
      for (const auto& lastS : lastReport->mInboundRtpStreamStats) {
        if (lastS.mId == s.mId) {
          int32_t deltaMs = s.mTimestamp.Value() - lastS.mTimestamp.Value();
          // In theory we're called every second, so delta *should* be in that
          // range. Small deltas could cause errors due to division
          if (deltaMs < 500 || deltaMs > 60000 ||
              !lastS.mBytesReceived.WasPassed()) {
            break;
          }
          HistogramID id = isAudio
                               ? WEBRTC_AUDIO_QUALITY_INBOUND_BANDWIDTH_KBITS
                               : WEBRTC_VIDEO_QUALITY_INBOUND_BANDWIDTH_KBITS;
          // We could accumulate values until enough time has passed
          // and then Accumulate() but this isn't that important
          Accumulate(
              id,
              ((s.mBytesReceived.Value() - lastS.mBytesReceived.Value()) * 8) /
                  deltaMs);
          break;
        }
      }
    }
  }
  RecordCommonRtpTelemetry(aReport->mRemoteInboundRtpStreamStats,
                           lastReport->mRemoteInboundRtpStreamStats, true);
  for (const auto& s : aReport->mRemoteInboundRtpStreamStats) {
    if (s.mRoundTripTime.WasPassed()) {
      const bool isAudio = s.mKind.Find(u"audio") != -1;
      HistogramID id = isAudio ? WEBRTC_AUDIO_QUALITY_OUTBOUND_RTT
                               : WEBRTC_VIDEO_QUALITY_OUTBOUND_RTT;
      Accumulate(id, s.mRoundTripTime.Value() * 1000);
    }
  }

  mLastReports[aReport->mPcid] = std::move(aReport);
}

void PeerConnectionCtx::EverySecondTelemetryCallback_m(nsITimer* timer,
                                                       void* closure) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(PeerConnectionCtx::isActive());

  for (auto& idAndPc : GetInstance()->mPeerConnections) {
    if (!idAndPc.second->IsClosed()) {
      idAndPc.second->GetStats(nullptr, true)
          ->Then(
              GetMainThreadSerialEventTarget(), __func__,
              [=](UniquePtr<dom::RTCStatsReportInternal>&& aReport) {
                if (PeerConnectionCtx::isActive()) {
                  PeerConnectionCtx::GetInstance()->DeliverStats(
                      std::move(aReport));
                }
              },
              [=](nsresult aError) {});
      idAndPc.second->CollectConduitTelemetryData();
    }
  }
}

void PeerConnectionCtx::UpdateNetworkState(bool online) {
  auto ctx = GetInstance();
  if (ctx->mPeerConnections.empty()) {
    return;
  }
  for (auto pc : ctx->mPeerConnections) {
    pc.second->UpdateNetworkState(online);
  }
}

SharedWebrtcState* PeerConnectionCtx::GetSharedWebrtcState() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mSharedWebrtcState;
}

void PeerConnectionCtx::RemovePeerConnection(const std::string& aKey) {
  MOZ_ASSERT(NS_IsMainThread());
  auto it = mPeerConnections.find(aKey);
  if (it != mPeerConnections.end()) {
    if (it->second->GetFinalStats() && !it->second->LongTermStatsIsDisabled()) {
      WebrtcGlobalInformation::StashStats(*(it->second->GetFinalStats()));
    }
    nsAutoString pcId = NS_ConvertASCIItoUTF16(it->second->GetName().c_str());
    if (XRE_IsContentProcess()) {
      if (auto* child = WebrtcGlobalChild::Get(); child) {
        auto pcId = NS_ConvertASCIItoUTF16(it->second->GetName().c_str());
        child->SendPeerConnectionFinalStats(*(it->second->GetFinalStats()));
        child->SendPeerConnectionDestroyed(pcId);
      }
    } else {
      using Update = WebrtcGlobalInformation::PcTrackingUpdate;
      auto update = Update::Remove(pcId);
      auto finalStats =
          MakeUnique<RTCStatsReportInternal>(*(it->second->GetFinalStats()));
      WebrtcGlobalStatsHistory::Record(std::move(finalStats));
      WebrtcGlobalInformation::PeerConnectionTracking(update);
    }

    mPeerConnections.erase(it);
    if (mPeerConnections.empty()) {
      mSharedWebrtcState = nullptr;
      StopTelemetryTimer();
    }
  }
}

void PeerConnectionCtx::AddPeerConnection(const std::string& aKey,
                                          PeerConnectionImpl* aPeerConnection) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mPeerConnections.count(aKey) == 0,
             "PeerConnection with this key should not already exist");
  if (mPeerConnections.empty()) {
    AudioState::Config audioStateConfig;
    audioStateConfig.audio_mixer = new rtc::RefCountedObject<DummyAudioMixer>();
    AudioProcessingBuilder audio_processing_builder;
    audioStateConfig.audio_processing =
        new rtc::RefCountedObject<DummyAudioProcessing>();
    audioStateConfig.audio_device_module =
        new rtc::RefCountedObject<FakeAudioDeviceModule>();

    SharedThreadPoolWebRtcTaskQueueFactory taskQueueFactory;
    constexpr bool supportTailDispatch = true;
    // Note the NonBlocking DeletionPolicy!
    // This task queue is passed into libwebrtc as a raw pointer.
    // WebrtcCallWrapper guarantees that it outlives its webrtc::Call instance.
    // Outside of libwebrtc we must use ref-counting to either the
    // WebrtcCallWrapper or to the CallWorkerThread to keep it alive.
    auto callWorkerThread =
        WrapUnique(taskQueueFactory
                       .CreateTaskQueueWrapper<DeletionPolicy::NonBlocking>(
                           "CallWorker", supportTailDispatch,
                           webrtc::TaskQueueFactory::Priority::NORMAL,
                           MediaThreadType::WEBRTC_CALL_THREAD)
                       .release());

    UniquePtr<webrtc::FieldTrialsView> trials =
        WrapUnique(new NoTrialsConfig());

    mSharedWebrtcState = MakeAndAddRef<SharedWebrtcState>(
        new CallWorkerThread(std::move(callWorkerThread)),
        std::move(audioStateConfig),
        already_AddRefed(CreateBuiltinAudioDecoderFactory().release()),
        std::move(trials));
    StartTelemetryTimer();
  }
  auto pcId = NS_ConvertASCIItoUTF16(aPeerConnection->GetName().c_str());
  if (XRE_IsContentProcess()) {
    if (auto* child = WebrtcGlobalChild::Get(); child) {
      child->SendPeerConnectionCreated(
          pcId, aPeerConnection->LongTermStatsIsDisabled());
    }
  } else {
    using Update = WebrtcGlobalInformation::PcTrackingUpdate;
    auto update = Update::Add(pcId, aPeerConnection->LongTermStatsIsDisabled());
    WebrtcGlobalInformation::PeerConnectionTracking(update);
  }
  mPeerConnections[aKey] = aPeerConnection;
}

PeerConnectionImpl* PeerConnectionCtx::GetPeerConnection(
    const std::string& aKey) const {
  MOZ_ASSERT(NS_IsMainThread());
  auto iterator = mPeerConnections.find(aKey);
  if (iterator == mPeerConnections.end()) {
    return nullptr;
  }
  return iterator->second;
}

void PeerConnectionCtx::ClearClosedStats() {
  for (auto& [id, pc] : mPeerConnections) {
    Unused << id;
    if (pc->IsClosed()) {
      // Rare case
      pc->DisableLongTermStats();
    }
  }
}

PeerConnectionCtx::PeerConnectionCtx()
    : mGMPReady(false),
      mLogHandle(EnsureWebrtcLogging()),
      mTransportHandler(
          MediaTransportHandler::Create(GetMainThreadSerialEventTarget())) {}

nsresult PeerConnectionCtx::Initialize() {
  MOZ_ASSERT(NS_IsMainThread());
  initGMP();
  SdpRidAttributeList::kMaxRidLength =
      webrtc::BaseRtpStringExtension::kMaxValueSizeBytes;

  if (XRE_IsContentProcess()) {
    WebrtcGlobalChild::Get();
  }

  return NS_OK;
}

nsresult PeerConnectionCtx::StartTelemetryTimer() {
  return NS_NewTimerWithFuncCallback(getter_AddRefs(mTelemetryTimer),
                                     EverySecondTelemetryCallback_m, this, 1000,
                                     nsITimer::TYPE_REPEATING_PRECISE_CAN_SKIP,
                                     "EverySecondTelemetryCallback_m");
}

void PeerConnectionCtx::StopTelemetryTimer() {
  if (mTelemetryTimer) {
    mTelemetryTimer->Cancel();
    mTelemetryTimer = nullptr;
  }
}

static void GMPReady_m() {
  if (PeerConnectionCtx::isActive()) {
    PeerConnectionCtx::GetInstance()->onGMPReady();
  }
};

static void GMPReady() {
  GetMainThreadSerialEventTarget()->Dispatch(WrapRunnableNM(&GMPReady_m),
                                             NS_DISPATCH_NORMAL);
};

void PeerConnectionCtx::initGMP() {
  mGMPService = do_GetService("@mozilla.org/gecko-media-plugin-service;1");

  if (!mGMPService) {
    CSFLogError(LOGTAG, "%s failed to get the gecko-media-plugin-service",
                __FUNCTION__);
    return;
  }

  nsCOMPtr<nsIThread> thread;
  nsresult rv = mGMPService->GetThread(getter_AddRefs(thread));

  if (NS_FAILED(rv)) {
    mGMPService = nullptr;
    CSFLogError(LOGTAG,
                "%s failed to get the gecko-media-plugin thread, err=%u",
                __FUNCTION__, static_cast<unsigned>(rv));
    return;
  }

  // presumes that all GMP dir scans have been queued for the GMPThread
  thread->Dispatch(WrapRunnableNM(&GMPReady), NS_DISPATCH_NORMAL);
}

nsresult PeerConnectionCtx::Cleanup() {
  CSFLogDebug(LOGTAG, "%s", __FUNCTION__);
  MOZ_ASSERT(NS_IsMainThread());

  mQueuedJSEPOperations.Clear();
  mGMPService = nullptr;
  mTransportHandler = nullptr;
  for (auto& [id, pc] : mPeerConnections) {
    (void)id;
    pc->Close();
  }
  mPeerConnections.clear();
  mSharedWebrtcState = nullptr;
  return NS_OK;
}

void PeerConnectionCtx::queueJSEPOperation(nsIRunnable* aOperation) {
  mQueuedJSEPOperations.AppendElement(aOperation);
}

void PeerConnectionCtx::onGMPReady() {
  mGMPReady = true;
  for (size_t i = 0; i < mQueuedJSEPOperations.Length(); ++i) {
    mQueuedJSEPOperations[i]->Run();
  }
  mQueuedJSEPOperations.Clear();
}

bool PeerConnectionCtx::gmpHasH264() {
  if (!mGMPService) {
    return false;
  }

  // XXX I'd prefer if this was all known ahead of time...

  AutoTArray<nsCString, 1> tags;
  tags.AppendElement("h264"_ns);

  bool has_gmp;
  nsresult rv;
  rv = mGMPService->HasPluginForAPI(nsLiteralCString(GMP_API_VIDEO_ENCODER),
                                    tags, &has_gmp);
  if (NS_FAILED(rv) || !has_gmp) {
    return false;
  }

  rv = mGMPService->HasPluginForAPI(nsLiteralCString(GMP_API_VIDEO_DECODER),
                                    tags, &has_gmp);
  if (NS_FAILED(rv) || !has_gmp) {
    return false;
  }

  return true;
}

}  // namespace mozilla

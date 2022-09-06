/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdlib>
#include <cerrno>
#include <deque>
#include <set>
#include <sstream>
#include <vector>

#include "common/browser_logging/CSFLog.h"
#include "base/histogram.h"
#include "common/time_profiling/timecard.h"

#include "jsapi.h"
#include "nspr.h"
#include "nss.h"
#include "pk11pub.h"

#include "nsNetCID.h"
#include "nsIIDNService.h"
#include "nsILoadContext.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsProxyRelease.h"
#include "prtime.h"

#include "libwebrtcglue/AudioConduit.h"
#include "libwebrtcglue/VideoConduit.h"
#include "libwebrtcglue/WebrtcCallWrapper.h"
#include "MediaTrackGraph.h"
#include "transport/runnable_utils.h"
#include "IPeerConnection.h"
#include "PeerConnectionCtx.h"
#include "PeerConnectionImpl.h"
#include "RemoteTrackSource.h"
#include "nsDOMDataChannelDeclarations.h"
#include "transport/dtlsidentity.h"
#include "sdp/SdpAttribute.h"

#include "jsep/JsepTrack.h"
#include "jsep/JsepSession.h"
#include "jsep/JsepSessionImpl.h"

#include "transportbridge/MediaPipeline.h"
#include "jsapi/RTCRtpReceiver.h"
#include "jsapi/RTCRtpSender.h"

#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Sprintf.h"

#ifdef XP_WIN
// We need to undef the MS macro for Document::CreateEvent
#  ifdef CreateEvent
#    undef CreateEvent
#  endif
#endif  // XP_WIN

#include "mozilla/dom/Document.h"
#include "nsGlobalWindow.h"
#include "nsDOMDataChannel.h"
#include "mozilla/dom/Location.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Preferences.h"
#include "mozilla/PublicSSL.h"
#include "nsXULAppAPI.h"
#include "nsContentUtils.h"
#include "nsDOMJSUtils.h"
#include "nsPrintfCString.h"
#include "nsURLHelper.h"
#include "nsNetUtil.h"
#include "js/ArrayBuffer.h"    // JS::NewArrayBufferWithContents
#include "js/GCAnnotations.h"  // JS_HAZ_ROOTED
#include "js/RootingAPI.h"     // JS::{{,Mutable}Handle,Rooted}
#include "mozilla/PeerIdentity.h"
#include "mozilla/dom/RTCCertificate.h"
#include "mozilla/dom/RTCRtpReceiverBinding.h"
#include "mozilla/dom/RTCRtpSenderBinding.h"
#include "mozilla/dom/RTCStatsReportBinding.h"
#include "mozilla/dom/RTCPeerConnectionBinding.h"
#include "mozilla/dom/PeerConnectionImplBinding.h"
#include "mozilla/dom/RTCDataChannelBinding.h"
#include "mozilla/dom/PluginCrashedEvent.h"
#include "MediaStreamTrack.h"
#include "AudioStreamTrack.h"
#include "VideoStreamTrack.h"
#include "nsIScriptGlobalObject.h"
#include "DOMMediaStream.h"
#include "WebrtcGlobalInformation.h"
#include "mozilla/dom/Event.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/net/DataChannelProtocol.h"
#include "MediaManager.h"

#include "transport/nr_socket_proxy_config.h"
#include "RTCDtlsTransport.h"
#include "jsep/JsepTransport.h"

#include "nsILoadInfo.h"
#include "nsIProxyInfo.h"
#include "nsIPrincipal.h"
#include "mozilla/LoadInfo.h"
#include "nsIProxiedChannel.h"

#include "mozilla/dom/BrowserChild.h"
#include "mozilla/net/WebrtcProxyConfig.h"

#ifdef XP_WIN
// We need to undef the MS macro again in case the windows include file
// got imported after we included mozilla/dom/Document.h
#  ifdef CreateEvent
#    undef CreateEvent
#  endif
#endif  // XP_WIN

#include "MediaSegment.h"

#ifdef USE_FAKE_PCOBSERVER
#  include "FakePCObserver.h"
#else
#  include "mozilla/dom/PeerConnectionObserverBinding.h"
#endif
#include "mozilla/dom/PeerConnectionObserverEnumsBinding.h"

#define ICE_PARSING \
  "In RTCConfiguration passed to RTCPeerConnection constructor"

using namespace mozilla;
using namespace mozilla::dom;

typedef PCObserverString ObString;

static const char* pciLogTag = "PeerConnectionImpl";
#ifdef LOGTAG
#  undef LOGTAG
#endif
#define LOGTAG pciLogTag

static mozilla::LazyLogModule logModuleInfo("signaling");

// Getting exceptions back down from PCObserver is generally not harmful.
namespace {
// This is a terrible hack.  The problem is that SuppressException is not
// inline, and we link this file without libxul in some cases (e.g. for our test
// setup).  So we can't use ErrorResult or IgnoredErrorResult because those call
// SuppressException...  And we can't use FastErrorResult because we can't
// include BindingUtils.h, because our linking is completely broken. Use
// BaseErrorResult directly.  Please do not let me see _anyone_ doing this
// without really careful review from someone who knows what they are doing.
class JSErrorResult : public binding_danger::TErrorResult<
                          binding_danger::JustAssertCleanupPolicy> {
 public:
  ~JSErrorResult() { SuppressException(); }
} JS_HAZ_ROOTED;

// The WrapRunnable() macros copy passed-in args and passes them to the function
// later on the other thread. ErrorResult cannot be passed like this because it
// disallows copy-semantics.
//
// This WrappableJSErrorResult hack solves this by not actually copying the
// ErrorResult, but creating a new one instead, which works because we don't
// care about the result.
//
// Since this is for JS-calls, these can only be dispatched to the main thread.

class WrappableJSErrorResult {
 public:
  WrappableJSErrorResult() : mRv(MakeUnique<JSErrorResult>()), isCopy(false) {}
  WrappableJSErrorResult(const WrappableJSErrorResult& other)
      : mRv(MakeUnique<JSErrorResult>()), isCopy(true) {}
  ~WrappableJSErrorResult() {
    if (isCopy) {
      MOZ_ASSERT(NS_IsMainThread());
    }
  }
  operator ErrorResult&() { return *mRv; }

 private:
  mozilla::UniquePtr<JSErrorResult> mRv;
  bool isCopy;
} JS_HAZ_ROOTED;

}  // namespace

static nsresult InitNSSInContent() {
  NS_ENSURE_TRUE(NS_IsMainThread(), NS_ERROR_NOT_SAME_THREAD);

  if (!XRE_IsContentProcess()) {
    MOZ_ASSERT_UNREACHABLE("Must be called in content process");
    return NS_ERROR_FAILURE;
  }

  static bool nssStarted = false;
  if (nssStarted) {
    return NS_OK;
  }

  if (NSS_NoDB_Init(nullptr) != SECSuccess) {
    CSFLogError(LOGTAG, "NSS_NoDB_Init failed.");
    return NS_ERROR_FAILURE;
  }

  if (NS_FAILED(mozilla::psm::InitializeCipherSuite())) {
    CSFLogError(LOGTAG, "Fail to set up nss cipher suite.");
    return NS_ERROR_FAILURE;
  }

  mozilla::psm::DisableMD5();

  nssStarted = true;

  return NS_OK;
}

namespace mozilla {
class DataChannel;
}

namespace mozilla {

void PeerConnectionAutoTimer::RegisterConnection() { mRefCnt++; }

void PeerConnectionAutoTimer::UnregisterConnection(bool aContainedAV) {
  MOZ_ASSERT(mRefCnt);
  mRefCnt--;
  mUsedAV |= aContainedAV;
  if (mRefCnt == 0) {
    if (mUsedAV) {
      Telemetry::Accumulate(
          Telemetry::WEBRTC_AV_CALL_DURATION,
          static_cast<uint32_t>((TimeStamp::Now() - mStart).ToSeconds()));
    }
    Telemetry::Accumulate(
        Telemetry::WEBRTC_CALL_DURATION,
        static_cast<uint32_t>((TimeStamp::Now() - mStart).ToSeconds()));
  }
}

bool PeerConnectionAutoTimer::IsStopped() { return mRefCnt == 0; }

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(PeerConnectionImpl)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(PeerConnectionImpl)
  tmp->Close();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPCObserver, mWindow, mCertificate,
                                  mSTSThread, mReceiveStreams, mOperations,
                                  mTransceivers, mKungFuDeathGrip)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(PeerConnectionImpl)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPCObserver, mWindow, mCertificate,
                                    mSTSThread, mReceiveStreams, mOperations,
                                    mTransceivers, mKungFuDeathGrip)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(PeerConnectionImpl)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PeerConnectionImpl)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PeerConnectionImpl)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

already_AddRefed<PeerConnectionImpl> PeerConnectionImpl::Constructor(
    const dom::GlobalObject& aGlobal) {
  RefPtr<PeerConnectionImpl> pc = new PeerConnectionImpl(&aGlobal);

  CSFLogDebug(LOGTAG, "Created PeerConnection: %p", pc.get());

  return pc.forget();
}

JSObject* PeerConnectionImpl::WrapObject(JSContext* aCx,
                                         JS::Handle<JSObject*> aGivenProto) {
  return PeerConnectionImpl_Binding::Wrap(aCx, this, aGivenProto);
}

nsPIDOMWindowInner* PeerConnectionImpl::GetParentObject() const {
  return mWindow;
}

bool PCUuidGenerator::Generate(std::string* idp) {
  nsresult rv;

  if (!mGenerator) {
    mGenerator = do_GetService("@mozilla.org/uuid-generator;1", &rv);
    if (NS_FAILED(rv)) {
      return false;
    }
    if (!mGenerator) {
      return false;
    }
  }

  nsID id;
  rv = mGenerator->GenerateUUIDInPlace(&id);
  if (NS_FAILED(rv)) {
    return false;
  }
  char buffer[NSID_LENGTH];
  id.ToProvidedString(buffer);
  idp->assign(buffer);

  return true;
}

bool IsPrivateBrowsing(nsPIDOMWindowInner* aWindow) {
  if (!aWindow) {
    return false;
  }

  Document* doc = aWindow->GetExtantDoc();
  if (!doc) {
    return false;
  }

  nsILoadContext* loadContext = doc->GetLoadContext();
  return loadContext && loadContext->UsePrivateBrowsing();
}

PeerConnectionImpl::PeerConnectionImpl(const GlobalObject* aGlobal)
    : mTimeCard(MOZ_LOG_TEST(logModuleInfo, LogLevel::Error) ? create_timecard()
                                                             : nullptr),
      mJsConfiguration(),
      mSignalingState(RTCSignalingState::Stable),
      mIceConnectionState(RTCIceConnectionState::New),
      mIceGatheringState(RTCIceGatheringState::New),
      mWindow(do_QueryInterface(aGlobal ? aGlobal->GetAsSupports() : nullptr)),
      mCertificate(nullptr),
      mSTSThread(nullptr),
      mForceIceTcp(false),
      mTransportHandler(nullptr),
      mUuidGen(MakeUnique<PCUuidGenerator>()),
      mIceRestartCount(0),
      mIceRollbackCount(0),
      mHaveConfiguredCodecs(false),
      mTrickle(true)  // TODO(ekr@rtfm.com): Use pref
      ,
      mPrivateWindow(false),
      mActiveOnWindow(false),
      mTimestampMaker(mWindow),
      mIdGenerator(new RTCStatsIdGenerator()),
      listenPort(0),
      connectPort(0),
      connectStr(nullptr) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT_IF(aGlobal, mWindow);
  mKungFuDeathGrip = this;
  if (aGlobal) {
    if (IsPrivateBrowsing(mWindow)) {
      mPrivateWindow = true;
    }
    mWindow->AddPeerConnection();
    mActiveOnWindow = true;

    mRtxIsAllowed =
        !HostnameInPref("media.peerconnection.video.use_rtx.blocklist",
                        mWindow->GetDocumentURI());
  }
  CSFLogInfo(LOGTAG, "%s: PeerConnectionImpl constructor for %s", __FUNCTION__,
             mHandle.c_str());
  STAMP_TIMECARD(mTimeCard, "Constructor Completed");
  mForceIceTcp =
      Preferences::GetBool("media.peerconnection.ice.force_ice_tcp", false);
  memset(mMaxReceiving, 0, sizeof(mMaxReceiving));
  memset(mMaxSending, 0, sizeof(mMaxSending));
  mJsConfiguration.mCertificatesProvided = false;
  mJsConfiguration.mPeerIdentityProvided = false;
}

PeerConnectionImpl::~PeerConnectionImpl() {
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(!mTransportHandler,
             "PeerConnection should either be closed, or not initted in the "
             "first place.");

  if (mTimeCard) {
    STAMP_TIMECARD(mTimeCard, "Destructor Invoked");
    STAMP_TIMECARD(mTimeCard, mHandle.c_str());
    print_timecard(mTimeCard);
    destroy_timecard(mTimeCard);
    mTimeCard = nullptr;
  }

  CSFLogInfo(LOGTAG, "%s: PeerConnectionImpl destructor invoked for %s",
             __FUNCTION__, mHandle.c_str());
}

nsresult PeerConnectionImpl::Initialize(PeerConnectionObserver& aObserver,
                                        nsGlobalWindowInner* aWindow) {
  nsresult res;

  MOZ_ASSERT(NS_IsMainThread());

  mPCObserver = &aObserver;

  // Find the STS thread

  mSTSThread = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &res);
  MOZ_ASSERT(mSTSThread);

  // We do callback handling on STS instead of main to avoid media jank.
  // Someday, we may have a dedicated thread for this.
  mTransportHandler = MediaTransportHandler::Create(mSTSThread);
  if (mPrivateWindow) {
    mTransportHandler->EnterPrivateMode();
  }

  // Initialize NSS if we are in content process. For chrome process, NSS should
  // already been initialized.
  if (XRE_IsParentProcess()) {
    // This code interferes with the C++ unit test startup code.
    nsCOMPtr<nsISupports> nssDummy = do_GetService("@mozilla.org/psm;1", &res);
    NS_ENSURE_SUCCESS(res, res);
  } else {
    NS_ENSURE_SUCCESS(res = InitNSSInContent(), res);
  }

  // Currently no standalone unit tests for DataChannel,
  // which is the user of mWindow
  MOZ_ASSERT(aWindow);
  mWindow = aWindow;
  NS_ENSURE_STATE(mWindow);

  PRTime timestamp = PR_Now();
  // Ok if we truncate this.
  char temp[128];

  nsAutoCString locationCStr;

  RefPtr<Location> location = mWindow->Location();
  nsAutoString locationAStr;
  res = location->ToString(locationAStr);
  NS_ENSURE_SUCCESS(res, res);

  CopyUTF16toUTF8(locationAStr, locationCStr);

  if (!mUuidGen->Generate(&mHandle)) {
    MOZ_CRASH();
    return NS_ERROR_UNEXPECTED;
  }

  SprintfLiteral(temp, "%s %" PRIu64 " (id=%" PRIu64 " url=%s)",
                 mHandle.c_str(), static_cast<uint64_t>(timestamp),
                 static_cast<uint64_t>(mWindow ? mWindow->WindowID() : 0),
                 locationCStr.get() ? locationCStr.get() : "NULL");

  mName = temp;

  STAMP_TIMECARD(mTimeCard, "Initializing PC Ctx");
  res = PeerConnectionCtx::InitializeGlobal();
  NS_ENSURE_SUCCESS(res, res);

  mTransportHandler->CreateIceCtx("PC:" + GetName());

  mJsepSession =
      MakeUnique<JsepSessionImpl>(mName, MakeUnique<PCUuidGenerator>());
  mJsepSession->SetRtxIsAllowed(mRtxIsAllowed);

  res = mJsepSession->Init();
  if (NS_FAILED(res)) {
    CSFLogError(LOGTAG, "%s: Couldn't init JSEP Session, res=%u", __FUNCTION__,
                static_cast<unsigned>(res));
    return res;
  }

  if (XRE_IsContentProcess()) {
    mStunAddrsRequest =
        new net::StunAddrsRequestChild(new StunAddrsHandler(this));
  }

  // Initialize the media object.
  mForceProxy = ShouldForceProxy();

  // setup the stun local addresses IPC async call
  InitLocalAddrs();

  mSignalHandler = MakeUnique<SignalHandler>(this, mTransportHandler.get());

  PeerConnectionCtx::GetInstance()->AddPeerConnection(mHandle, this);

  return NS_OK;
}

void PeerConnectionImpl::Initialize(PeerConnectionObserver& aObserver,
                                    nsGlobalWindowInner& aWindow,
                                    ErrorResult& rv) {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult res = Initialize(aObserver, &aWindow);
  if (NS_FAILED(res)) {
    rv.Throw(res);
    return;
  }
}

void PeerConnectionImpl::SetCertificate(
    mozilla::dom::RTCCertificate& aCertificate) {
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(!mCertificate, "This can only be called once");
  mCertificate = &aCertificate;

  std::vector<uint8_t> fingerprint;
  nsresult rv =
      CalculateFingerprint(DtlsIdentity::DEFAULT_HASH_ALGORITHM, &fingerprint);
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "%s: Couldn't calculate fingerprint, rv=%u",
                __FUNCTION__, static_cast<unsigned>(rv));
    mCertificate = nullptr;
    return;
  }
  rv = mJsepSession->AddDtlsFingerprint(DtlsIdentity::DEFAULT_HASH_ALGORITHM,
                                        fingerprint);
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "%s: Couldn't set DTLS credentials, rv=%u",
                __FUNCTION__, static_cast<unsigned>(rv));
    mCertificate = nullptr;
  }
}

const RefPtr<mozilla::dom::RTCCertificate>& PeerConnectionImpl::Certificate()
    const {
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  return mCertificate;
}

RefPtr<DtlsIdentity> PeerConnectionImpl::Identity() const {
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(mCertificate);
  return mCertificate->CreateDtlsIdentity();
}

class CompareCodecPriority {
 public:
  void SetPreferredCodec(int32_t preferredCodec) {
    // This pref really ought to be a string, preferably something like
    // "H264" or "VP8" instead of a payload type.
    // Bug 1101259.
    std::ostringstream os;
    os << preferredCodec;
    mPreferredCodec = os.str();
  }

  bool operator()(const UniquePtr<JsepCodecDescription>& lhs,
                  const UniquePtr<JsepCodecDescription>& rhs) const {
    if (!mPreferredCodec.empty() && lhs->mDefaultPt == mPreferredCodec &&
        rhs->mDefaultPt != mPreferredCodec) {
      return true;
    }

    if (lhs->mStronglyPreferred && !rhs->mStronglyPreferred) {
      return true;
    }

    return false;
  }

 private:
  std::string mPreferredCodec;
};

class ConfigureCodec {
 public:
  explicit ConfigureCodec(nsCOMPtr<nsIPrefBranch>& branch)
      : mHardwareH264Enabled(false),
        mSoftwareH264Enabled(false),
        mH264Enabled(false),
        mVP9Enabled(true),
        mVP9Preferred(false),
        mH264Level(13),   // minimum suggested for WebRTC spec
        mH264MaxBr(0),    // Unlimited
        mH264MaxMbps(0),  // Unlimited
        mVP8MaxFs(0),
        mVP8MaxFr(0),
        mUseTmmbr(false),
        mUseRemb(false),
        mUseTransportCC(false),
        mUseAudioFec(false),
        mRedUlpfecEnabled(false),
        mDtmfEnabled(false) {
    mSoftwareH264Enabled = PeerConnectionCtx::GetInstance()->gmpHasH264();

    if (WebrtcVideoConduit::HasH264Hardware()) {
      Telemetry::Accumulate(Telemetry::WEBRTC_HAS_H264_HARDWARE, true);
      branch->GetBoolPref("media.webrtc.hw.h264.enabled",
                          &mHardwareH264Enabled);
    }

    mH264Enabled = mHardwareH264Enabled || mSoftwareH264Enabled;
    Telemetry::Accumulate(Telemetry::WEBRTC_SOFTWARE_H264_ENABLED,
                          mSoftwareH264Enabled);
    Telemetry::Accumulate(Telemetry::WEBRTC_HARDWARE_H264_ENABLED,
                          mHardwareH264Enabled);
    Telemetry::Accumulate(Telemetry::WEBRTC_H264_ENABLED, mH264Enabled);

    branch->GetIntPref("media.navigator.video.h264.level", &mH264Level);
    mH264Level &= 0xFF;

    branch->GetIntPref("media.navigator.video.h264.max_br", &mH264MaxBr);

    branch->GetIntPref("media.navigator.video.h264.max_mbps", &mH264MaxMbps);

    branch->GetBoolPref("media.peerconnection.video.vp9_enabled", &mVP9Enabled);

    branch->GetBoolPref("media.peerconnection.video.vp9_preferred",
                        &mVP9Preferred);

    branch->GetIntPref("media.navigator.video.max_fs", &mVP8MaxFs);
    if (mVP8MaxFs <= 0) {
      mVP8MaxFs = 12288;  // We must specify something other than 0
    }

    branch->GetIntPref("media.navigator.video.max_fr", &mVP8MaxFr);
    if (mVP8MaxFr <= 0) {
      mVP8MaxFr = 60;  // We must specify something other than 0
    }

    // TMMBR is enabled from a pref in about:config
    branch->GetBoolPref("media.navigator.video.use_tmmbr", &mUseTmmbr);

    // REMB is enabled by default, but can be disabled from about:config
    branch->GetBoolPref("media.navigator.video.use_remb", &mUseRemb);

    branch->GetBoolPref("media.navigator.video.use_transport_cc",
                        &mUseTransportCC);

    branch->GetBoolPref("media.navigator.audio.use_fec", &mUseAudioFec);

    branch->GetBoolPref("media.navigator.video.red_ulpfec_enabled",
                        &mRedUlpfecEnabled);

    // media.peerconnection.dtmf.enabled controls both sdp generation for
    // DTMF support as well as DTMF exposure to DOM
    branch->GetBoolPref("media.peerconnection.dtmf.enabled", &mDtmfEnabled);
  }

  void operator()(UniquePtr<JsepCodecDescription>& codec) const {
    switch (codec->Type()) {
      case SdpMediaSection::kAudio: {
        JsepAudioCodecDescription& audioCodec =
            static_cast<JsepAudioCodecDescription&>(*codec);
        if (audioCodec.mName == "opus") {
          audioCodec.mFECEnabled = mUseAudioFec;
        } else if (audioCodec.mName == "telephone-event") {
          audioCodec.mEnabled = mDtmfEnabled;
        }
      } break;
      case SdpMediaSection::kVideo: {
        JsepVideoCodecDescription& videoCodec =
            static_cast<JsepVideoCodecDescription&>(*codec);

        if (videoCodec.mName == "H264") {
          // Override level
          videoCodec.mProfileLevelId &= 0xFFFF00;
          videoCodec.mProfileLevelId |= mH264Level;

          videoCodec.mConstraints.maxBr = mH264MaxBr;

          videoCodec.mConstraints.maxMbps = mH264MaxMbps;

          // Might disable it, but we set up other params anyway
          videoCodec.mEnabled = mH264Enabled;

          if (videoCodec.mPacketizationMode == 0 && !mSoftwareH264Enabled) {
            // We're assuming packetization mode 0 is unsupported by
            // hardware.
            videoCodec.mEnabled = false;
          }

          if (mHardwareH264Enabled) {
            videoCodec.mStronglyPreferred = true;
          }
        } else if (videoCodec.mName == "red") {
          videoCodec.mEnabled = mRedUlpfecEnabled;
        } else if (videoCodec.mName == "ulpfec") {
          videoCodec.mEnabled = mRedUlpfecEnabled;
        } else if (videoCodec.mName == "VP8" || videoCodec.mName == "VP9") {
          if (videoCodec.mName == "VP9") {
            if (!mVP9Enabled) {
              videoCodec.mEnabled = false;
              break;
            }
            if (mVP9Preferred) {
              videoCodec.mStronglyPreferred = true;
            }
          }
          videoCodec.mConstraints.maxFs = mVP8MaxFs;
          videoCodec.mConstraints.maxFps = Some(mVP8MaxFr);
        }

        if (mUseTmmbr) {
          videoCodec.EnableTmmbr();
        }
        if (mUseRemb) {
          videoCodec.EnableRemb();
        }
        if (mUseTransportCC) {
          videoCodec.EnableTransportCC();
        }
      } break;
      case SdpMediaSection::kText:
      case SdpMediaSection::kApplication:
      case SdpMediaSection::kMessage: {
      }  // Nothing to configure for these.
    }
  }

 private:
  bool mHardwareH264Enabled;
  bool mSoftwareH264Enabled;
  bool mH264Enabled;
  bool mVP9Enabled;
  bool mVP9Preferred;
  int32_t mH264Level;
  int32_t mH264MaxBr;
  int32_t mH264MaxMbps;
  int32_t mVP8MaxFs;
  int32_t mVP8MaxFr;
  bool mUseTmmbr;
  bool mUseRemb;
  bool mUseTransportCC;
  bool mUseAudioFec;
  bool mRedUlpfecEnabled;
  bool mDtmfEnabled;
};

class ConfigureRedCodec {
 public:
  explicit ConfigureRedCodec(nsCOMPtr<nsIPrefBranch>& branch,
                             std::vector<uint8_t>* redundantEncodings)
      : mRedundantEncodings(redundantEncodings) {
    // if we wanted to override or modify which encodings are considered
    // for redundant encodings, we'd probably want to handle it here by
    // checking prefs modifying the operator() code below
  }

  void operator()(UniquePtr<JsepCodecDescription>& codec) const {
    if (codec->Type() == SdpMediaSection::kVideo && !codec->mEnabled) {
      uint8_t pt = (uint8_t)strtoul(codec->mDefaultPt.c_str(), nullptr, 10);
      // don't search for the codec payload type unless we have a valid
      // conversion (non-zero)
      if (pt != 0) {
        std::vector<uint8_t>::iterator it = std::find(
            mRedundantEncodings->begin(), mRedundantEncodings->end(), pt);
        if (it != mRedundantEncodings->end()) {
          mRedundantEncodings->erase(it);
        }
      }
    }
  }

 private:
  std::vector<uint8_t>* mRedundantEncodings;
};

nsresult PeerConnectionImpl::ConfigureJsepSessionCodecs() {
  nsresult res;
  nsCOMPtr<nsIPrefService> prefs =
      do_GetService("@mozilla.org/preferences-service;1", &res);

  if (NS_FAILED(res)) {
    CSFLogError(LOGTAG, "%s: Couldn't get prefs service, res=%u", __FUNCTION__,
                static_cast<unsigned>(res));
    return res;
  }

  nsCOMPtr<nsIPrefBranch> branch = do_QueryInterface(prefs);
  if (!branch) {
    CSFLogError(LOGTAG, "%s: Couldn't get prefs branch", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  ConfigureCodec configurer(branch);
  mJsepSession->ForEachCodec(configurer);

  // if red codec is enabled, configure it for the other enabled codecs
  for (auto& codec : mJsepSession->Codecs()) {
    if (codec->mName == "red" && codec->mEnabled) {
      JsepVideoCodecDescription* redCodec =
          static_cast<JsepVideoCodecDescription*>(codec.get());
      ConfigureRedCodec configureRed(branch, &(redCodec->mRedundantEncodings));
      mJsepSession->ForEachCodec(configureRed);
      break;
    }
  }

  // We use this to sort the list of codecs once everything is configured
  CompareCodecPriority comparator;

  // Sort by priority
  int32_t preferredCodec = 0;
  branch->GetIntPref("media.navigator.video.preferred_codec", &preferredCodec);

  if (preferredCodec) {
    comparator.SetPreferredCodec(preferredCodec);
  }

  mJsepSession->SortCodecs(comparator);
  return NS_OK;
}

// Data channels won't work without a window, so in order for the C++ unit
// tests to work (it doesn't have a window available) we ifdef the following
// two implementations.
//
// Note: 'media.peerconnection.sctp.force_maximum_message_size' changes
// behaviour triggered by these parameters.
NS_IMETHODIMP
PeerConnectionImpl::EnsureDataConnection(uint16_t aLocalPort,
                                         uint16_t aNumstreams,
                                         uint32_t aMaxMessageSize,
                                         bool aMMSSet) {
  PC_AUTO_ENTER_API_CALL(false);

  if (mDataConnection) {
    CSFLogDebug(LOGTAG, "%s DataConnection already connected", __FUNCTION__);
    mDataConnection->SetMaxMessageSize(aMMSSet, aMaxMessageSize);
    return NS_OK;
  }

  nsCOMPtr<nsISerialEventTarget> target =
      mWindow ? mWindow->EventTargetFor(TaskCategory::Other) : nullptr;
  Maybe<uint64_t> mms = aMMSSet ? Some(aMaxMessageSize) : Nothing();
  if (auto res = DataChannelConnection::Create(this, target, mTransportHandler,
                                               aLocalPort, aNumstreams, mms)) {
    mDataConnection = res.value();
    CSFLogDebug(LOGTAG, "%s DataChannelConnection %p attached to %s",
                __FUNCTION__, (void*)mDataConnection.get(), mHandle.c_str());
    return NS_OK;
  }
  CSFLogError(LOGTAG, "%s DataConnection Create Failed", __FUNCTION__);
  return NS_ERROR_FAILURE;
}

nsresult PeerConnectionImpl::GetDatachannelParameters(
    uint32_t* channels, uint16_t* localport, uint16_t* remoteport,
    uint32_t* remotemaxmessagesize, bool* mmsset, std::string* transportId,
    bool* client) const {
  // Clear, just in case we fail.
  *channels = 0;
  *localport = 0;
  *remoteport = 0;
  *remotemaxmessagesize = 0;
  *mmsset = false;
  transportId->clear();

  RefPtr<JsepTransceiver> datachannelTransceiver;
  for (const auto& [id, transceiver] : mJsepSession->GetTransceivers()) {
    (void)id;  // Lame, but no better way to do this right now.
    if ((transceiver->GetMediaType() == SdpMediaSection::kApplication) &&
        transceiver->mSendTrack.GetNegotiatedDetails()) {
      datachannelTransceiver = transceiver;
      break;
    }
  }

  if (!datachannelTransceiver ||
      !datachannelTransceiver->mTransport.mComponents) {
    return NS_ERROR_FAILURE;
  }

  // This will release assert if there is no such index, and that's ok
  const JsepTrackEncoding& encoding =
      datachannelTransceiver->mSendTrack.GetNegotiatedDetails()->GetEncoding(0);

  if (NS_WARN_IF(encoding.GetCodecs().empty())) {
    CSFLogError(LOGTAG,
                "%s: Negotiated m=application with no codec. "
                "This is likely to be broken.",
                __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  for (const auto& codec : encoding.GetCodecs()) {
    if (codec->Type() != SdpMediaSection::kApplication) {
      CSFLogError(LOGTAG,
                  "%s: Codec type for m=application was %u, this "
                  "is a bug.",
                  __FUNCTION__, static_cast<unsigned>(codec->Type()));
      MOZ_ASSERT(false, "Codec for m=application was not \"application\"");
      return NS_ERROR_FAILURE;
    }

    if (codec->mName != "webrtc-datachannel") {
      CSFLogWarn(LOGTAG,
                 "%s: Codec for m=application was not "
                 "webrtc-datachannel (was instead %s). ",
                 __FUNCTION__, codec->mName.c_str());
      continue;
    }

    if (codec->mChannels) {
      *channels = codec->mChannels;
    } else {
      *channels = WEBRTC_DATACHANNEL_STREAMS_DEFAULT;
    }
    const JsepApplicationCodecDescription* appCodec =
        static_cast<const JsepApplicationCodecDescription*>(codec.get());
    *localport = appCodec->mLocalPort;
    *remoteport = appCodec->mRemotePort;
    *remotemaxmessagesize = appCodec->mRemoteMaxMessageSize;
    *mmsset = appCodec->mRemoteMMSSet;
    MOZ_ASSERT(!datachannelTransceiver->mTransport.mTransportId.empty());
    *transportId = datachannelTransceiver->mTransport.mTransportId;
    *client = datachannelTransceiver->mTransport.mDtls->GetRole() ==
              JsepDtlsTransport::kJsepDtlsClient;
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult PeerConnectionImpl::AddRtpTransceiverToJsepSession(
    RefPtr<JsepTransceiver>& transceiver) {
  nsresult res = ConfigureJsepSessionCodecs();
  if (NS_FAILED(res)) {
    CSFLogError(LOGTAG, "Failed to configure codecs");
    return res;
  }

  res = mJsepSession->AddTransceiver(transceiver);

  if (NS_FAILED(res)) {
    std::string errorString = mJsepSession->GetLastError();
    CSFLogError(LOGTAG, "%s (%s) : pc = %s, error = %s", __FUNCTION__,
                transceiver->GetMediaType() == SdpMediaSection::kAudio
                    ? "audio"
                    : "video",
                mHandle.c_str(), errorString.c_str());
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

static Maybe<SdpMediaSection::MediaType> ToSdpMediaType(
    const nsAString& aKind) {
  if (aKind.EqualsASCII("audio")) {
    return Some(SdpMediaSection::MediaType::kAudio);
  } else if (aKind.EqualsASCII("video")) {
    return Some(SdpMediaSection::MediaType::kVideo);
  }
  return Nothing();
}

already_AddRefed<RTCRtpTransceiver> PeerConnectionImpl::AddTransceiver(
    const dom::RTCRtpTransceiverInit& aInit, const nsAString& aKind,
    dom::MediaStreamTrack* aSendTrack, ErrorResult& aRv) {
  Maybe<SdpMediaSection::MediaType> type = ToSdpMediaType(aKind);
  if (NS_WARN_IF(!type.isSome())) {
    MOZ_ASSERT(false, "Invalid media kind");
    aRv = NS_ERROR_INVALID_ARG;
    return nullptr;
  }

  RefPtr<JsepTransceiver> jsepTransceiver = new JsepTransceiver(*type);

  RefPtr<RTCRtpTransceiver> transceiver =
      CreateTransceiver(jsepTransceiver, aInit, aSendTrack, aRv);

  if (aRv.Failed()) {
    // Would be nice if we could peek at the rv without stealing it, so we
    // could log...
    CSFLogError(LOGTAG, "%s: failed", __FUNCTION__);
    return nullptr;
  }

  jsepTransceiver->SetRtxIsAllowed(mRtxIsAllowed);

  // Do this last, since it is not possible to roll back.
  nsresult rv = AddRtpTransceiverToJsepSession(jsepTransceiver);
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "%s: AddRtpTransceiverToJsepSession failed, res=%u",
                __FUNCTION__, static_cast<unsigned>(rv));
    aRv = rv;
    return nullptr;
  }

  mTransceivers.AppendElement(transceiver);
  return transceiver.forget();
}

bool PeerConnectionImpl::CheckNegotiationNeeded() {
  MOZ_ASSERT(mSignalingState == RTCSignalingState::Stable);
  return !mLocalIceCredentialsToReplace.empty() ||
         mJsepSession->CheckNegotiationNeeded();
}

bool PeerConnectionImpl::CreatedSender(const dom::RTCRtpSender& aSender) const {
  return aSender.IsMyPc(this);
}

nsresult PeerConnectionImpl::InitializeDataChannel() {
  PC_AUTO_ENTER_API_CALL(false);
  CSFLogDebug(LOGTAG, "%s", __FUNCTION__);

  uint32_t channels = 0;
  uint16_t localport = 0;
  uint16_t remoteport = 0;
  uint32_t remotemaxmessagesize = 0;
  bool mmsset = false;
  std::string transportId;
  bool client = false;
  nsresult rv = GetDatachannelParameters(&channels, &localport, &remoteport,
                                         &remotemaxmessagesize, &mmsset,
                                         &transportId, &client);

  if (NS_FAILED(rv)) {
    CSFLogDebug(LOGTAG, "%s: We did not negotiate datachannel", __FUNCTION__);
    return NS_OK;
  }

  if (channels > MAX_NUM_STREAMS) {
    channels = MAX_NUM_STREAMS;
  }

  rv = EnsureDataConnection(localport, channels, remotemaxmessagesize, mmsset);
  if (NS_SUCCEEDED(rv)) {
    if (mDataConnection->ConnectToTransport(transportId, client, localport,
                                            remoteport)) {
      return NS_OK;
    }
    // If we inited the DataConnection, call Destroy() before releasing it
    mDataConnection->Destroy();
  }
  mDataConnection = nullptr;
  return NS_ERROR_FAILURE;
}

already_AddRefed<nsDOMDataChannel> PeerConnectionImpl::CreateDataChannel(
    const nsAString& aLabel, const nsAString& aProtocol, uint16_t aType,
    bool ordered, uint16_t aMaxTime, uint16_t aMaxNum, bool aExternalNegotiated,
    uint16_t aStream, ErrorResult& rv) {
  RefPtr<nsDOMDataChannel> result;
  rv = CreateDataChannel(aLabel, aProtocol, aType, ordered, aMaxTime, aMaxNum,
                         aExternalNegotiated, aStream, getter_AddRefs(result));
  return result.forget();
}

NS_IMETHODIMP
PeerConnectionImpl::CreateDataChannel(
    const nsAString& aLabel, const nsAString& aProtocol, uint16_t aType,
    bool ordered, uint16_t aMaxTime, uint16_t aMaxNum, bool aExternalNegotiated,
    uint16_t aStream, nsDOMDataChannel** aRetval) {
  PC_AUTO_ENTER_API_CALL(false);
  MOZ_ASSERT(aRetval);

  RefPtr<DataChannel> dataChannel;
  DataChannelConnection::Type theType =
      static_cast<DataChannelConnection::Type>(aType);

  nsresult rv = EnsureDataConnection(
      WEBRTC_DATACHANNEL_PORT_DEFAULT, WEBRTC_DATACHANNEL_STREAMS_DEFAULT,
      WEBRTC_DATACHANNEL_MAX_MESSAGE_SIZE_REMOTE_DEFAULT, false);
  if (NS_FAILED(rv)) {
    return rv;
  }
  dataChannel = mDataConnection->Open(
      NS_ConvertUTF16toUTF8(aLabel), NS_ConvertUTF16toUTF8(aProtocol), theType,
      ordered,
      aType == DataChannelConnection::PARTIAL_RELIABLE_REXMIT
          ? aMaxNum
          : (aType == DataChannelConnection::PARTIAL_RELIABLE_TIMED ? aMaxTime
                                                                    : 0),
      nullptr, nullptr, aExternalNegotiated, aStream);
  NS_ENSURE_TRUE(dataChannel, NS_ERROR_NOT_AVAILABLE);

  CSFLogDebug(LOGTAG, "%s: making DOMDataChannel", __FUNCTION__);

  RefPtr<JsepTransceiver> dcTransceiver;
  for (auto& [id, transceiver] : mJsepSession->GetTransceivers()) {
    (void)id;  // Lame, but no better way to do this right now.
    if (transceiver->GetMediaType() == SdpMediaSection::kApplication) {
      dcTransceiver = transceiver;
      break;
    }
  }

  if (!dcTransceiver) {
    dcTransceiver =
        new JsepTransceiver(SdpMediaSection::MediaType::kApplication);
    mJsepSession->AddTransceiver(dcTransceiver);
  }

  dcTransceiver->RestartDatachannelTransceiver();

  RefPtr<nsDOMDataChannel> retval;
  rv = NS_NewDOMDataChannel(dataChannel.forget(), mWindow,
                            getter_AddRefs(retval));
  if (NS_FAILED(rv)) {
    return rv;
  }
  retval.forget(aRetval);
  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION(PeerConnectionImpl::Operation, mPromise, mPc)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PeerConnectionImpl::Operation)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END
NS_IMPL_CYCLE_COLLECTING_ADDREF(PeerConnectionImpl::Operation)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PeerConnectionImpl::Operation)

PeerConnectionImpl::Operation::Operation(PeerConnectionImpl* aPc,
                                         ErrorResult& aError)
    : mPromise(aPc->MakePromise(aError)), mPc(aPc) {}

PeerConnectionImpl::Operation::~Operation() = default;

void PeerConnectionImpl::Operation::Call(ErrorResult& aError) {
  RefPtr<dom::Promise> opPromise = CallImpl(aError);
  if (aError.Failed()) {
    return;
  }
  // Upon fulfillment or rejection of the promise returned by the operation,
  // run the following steps:
  // (NOTE: mPromise is p from https://w3c.github.io/webrtc-pc/#dfn-chain,
  // and CallImpl() is what returns the promise for the operation itself)
  opPromise->AppendNativeHandler(this);
}

void PeerConnectionImpl::Operation::ResolvedCallback(
    JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult& aRv) {
  // If connection.[[IsClosed]] is true, abort these steps.
  // (the spec wants p to never settle in this event)
  if (!mPc->IsClosed()) {
    // If the promise returned by operation was fulfilled with a
    // value, fulfill p with that value.
    mPromise->MaybeResolveWithClone(aCx, aValue);
    // Upon fulfillment or rejection of p, execute the following
    // steps:
    // (Static analysis forces us to use a temporary)
    RefPtr<PeerConnectionImpl> pc = mPc;
    pc->RunNextOperation(aRv);
  }
}

void PeerConnectionImpl::Operation::RejectedCallback(
    JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult& aRv) {
  // If connection.[[IsClosed]] is true, abort these steps.
  // (the spec wants p to never settle in this event)
  if (!mPc->IsClosed()) {
    // If the promise returned by operation was rejected with a
    // value, reject p with that value.
    mPromise->MaybeRejectWithClone(aCx, aValue);
    // Upon fulfillment or rejection of p, execute the following
    // steps:
    // (Static analysis forces us to use a temporary)
    RefPtr<PeerConnectionImpl> pc = mPc;
    pc->RunNextOperation(aRv);
  }
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(PeerConnectionImpl::JSOperation,
                                   PeerConnectionImpl::Operation, mOperation)

NS_IMPL_ADDREF_INHERITED(PeerConnectionImpl::JSOperation,
                         PeerConnectionImpl::Operation)
NS_IMPL_RELEASE_INHERITED(PeerConnectionImpl::JSOperation,
                          PeerConnectionImpl::Operation)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PeerConnectionImpl::JSOperation)
NS_INTERFACE_MAP_END_INHERITING(PeerConnectionImpl::Operation)

PeerConnectionImpl::JSOperation::JSOperation(PeerConnectionImpl* aPc,
                                             dom::ChainedOperation& aOp,
                                             ErrorResult& aError)
    : Operation(aPc, aError), mOperation(&aOp) {}

RefPtr<dom::Promise> PeerConnectionImpl::JSOperation::CallImpl(
    ErrorResult& aError) {
  // Static analysis will not let us call this without a temporary :(
  RefPtr<dom::ChainedOperation> op = mOperation;
  return op->Call(aError);
}

already_AddRefed<dom::Promise> PeerConnectionImpl::Chain(
    dom::ChainedOperation& aOperation, ErrorResult& aError) {
  MOZ_RELEASE_ASSERT(!mChainingOperation);
  mChainingOperation = true;
  RefPtr<Operation> operation = new JSOperation(this, aOperation, aError);
  if (aError.Failed()) {
    return nullptr;
  }
  RefPtr<Promise> promise = Chain(operation, aError);
  if (aError.Failed()) {
    return nullptr;
  }
  mChainingOperation = false;
  return promise.forget();
}

// This is kinda complicated, but it is what the spec requires us to do. The
// core of what makes this complicated is the requirement that |aOperation| be
// run _immediately_ (without any Promise.Then!) if the operations chain is
// empty.
already_AddRefed<dom::Promise> PeerConnectionImpl::Chain(
    const RefPtr<Operation>& aOperation, ErrorResult& aError) {
  // If connection.[[IsClosed]] is true, return a promise rejected with a newly
  // created InvalidStateError.
  if (IsClosed()) {
    CSFLogDebug(LOGTAG, "%s:%d: Peer connection is closed", __FILE__, __LINE__);
    RefPtr<dom::Promise> error = MakePromise(aError);
    if (aError.Failed()) {
      return nullptr;
    }
    error->MaybeRejectWithInvalidStateError("Peer connection is closed");
    return error.forget();
  }

  // Append operation to [[Operations]].
  mOperations.AppendElement(aOperation);

  // If the length of [[Operations]] is exactly 1, execute operation.
  if (mOperations.Length() == 1) {
    aOperation->Call(aError);
    if (aError.Failed()) {
      return nullptr;
    }
  }

  // This is the promise p from https://w3c.github.io/webrtc-pc/#dfn-chain
  return do_AddRef(aOperation->GetPromise());
}

void PeerConnectionImpl::RunNextOperation(ErrorResult& aError) {
  // If connection.[[IsClosed]] is true, abort these steps.
  if (IsClosed()) {
    return;
  }

  // Remove the first element of [[Operations]].
  mOperations.RemoveElementAt(0);

  // If [[Operations]] is non-empty, execute the operation represented by the
  // first element of [[Operations]], and abort these steps.
  if (mOperations.Length()) {
    // Cannot call without a temporary :(
    RefPtr<Operation> op = mOperations[0];
    op->Call(aError);
    return;
  }

  // If connection.[[UpdateNegotiationNeededFlagOnEmptyChain]] is false, abort
  // these steps.
  if (!mUpdateNegotiationNeededFlagOnEmptyChain) {
    return;
  }

  // Set connection.[[UpdateNegotiationNeededFlagOnEmptyChain]] to false.
  mUpdateNegotiationNeededFlagOnEmptyChain = false;
  // Update the negotiation-needed flag for connection.
  UpdateNegotiationNeeded();
}

already_AddRefed<dom::Promise> PeerConnectionImpl::MakePromise(
    ErrorResult& aError) const {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mWindow);
  return dom::Promise::Create(global, aError);
}

void PeerConnectionImpl::UpdateNegotiationNeeded() {
  // If the length of connection.[[Operations]] is not 0, then set
  // connection.[[UpdateNegotiationNeededFlagOnEmptyChain]] to true, and abort
  // these steps.
  if (mOperations.Length() != 0) {
    mUpdateNegotiationNeededFlagOnEmptyChain = true;
    return;
  }

  // Queue a task to run the following steps:
  GetMainThreadEventTarget()->Dispatch(NS_NewRunnableFunction(
      __func__, [this, self = RefPtr<PeerConnectionImpl>(this)] {
        // If connection.[[IsClosed]] is true, abort these steps.
        if (IsClosed()) {
          return;
        }
        // If the length of connection.[[Operations]] is not 0, then set
        // connection.[[UpdateNegotiationNeededFlagOnEmptyChain]] to true, and
        // abort these steps.
        if (mOperations.Length()) {
          mUpdateNegotiationNeededFlagOnEmptyChain = true;
          return;
        }
        // If connection's signaling state is not "stable", abort these steps.
        if (mSignalingState != RTCSignalingState::Stable) {
          return;
        }
        // If the result of checking if negotiation is needed is false, clear
        // the negotiation-needed flag by setting
        // connection.[[NegotiationNeeded]] to false, and abort these steps.
        if (!CheckNegotiationNeeded()) {
          mNegotiationNeeded = false;
          return;
        }

        // If connection.[[NegotiationNeeded]] is already true, abort these
        // steps.
        if (mNegotiationNeeded) {
          return;
        }

        // Set connection.[[NegotiationNeeded]] to true.
        mNegotiationNeeded = true;

        // Fire an event named negotiationneeded at connection.
        ErrorResult rv;
        mPCObserver->FireNegotiationNeededEvent(rv);
      }));
}

void PeerConnectionImpl::NotifyDataChannel(
    already_AddRefed<DataChannel> aChannel) {
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  RefPtr<DataChannel> channel(aChannel);
  MOZ_ASSERT(channel);
  CSFLogDebug(LOGTAG, "%s: channel: %p", __FUNCTION__, channel.get());

  RefPtr<nsDOMDataChannel> domchannel;
  nsresult rv = NS_NewDOMDataChannel(channel.forget(), mWindow,
                                     getter_AddRefs(domchannel));
  NS_ENSURE_SUCCESS_VOID(rv);

  JSErrorResult jrv;
  mPCObserver->NotifyDataChannel(*domchannel, jrv);
}

NS_IMETHODIMP
PeerConnectionImpl::CreateOffer(const RTCOfferOptions& aOptions) {
  JsepOfferOptions options;
  // convert the RTCOfferOptions to JsepOfferOptions
  if (aOptions.mOfferToReceiveAudio.WasPassed()) {
    options.mOfferToReceiveAudio =
        mozilla::Some(size_t(aOptions.mOfferToReceiveAudio.Value()));
  }

  if (aOptions.mOfferToReceiveVideo.WasPassed()) {
    options.mOfferToReceiveVideo =
        mozilla::Some(size_t(aOptions.mOfferToReceiveVideo.Value()));
  }

  options.mIceRestart = mozilla::Some(aOptions.mIceRestart ||
                                      !mLocalIceCredentialsToReplace.empty());

  return CreateOffer(options);
}

static void DeferredCreateOffer(const std::string& aPcHandle,
                                const JsepOfferOptions& aOptions) {
  PeerConnectionWrapper wrapper(aPcHandle);

  if (wrapper.impl()) {
    if (!PeerConnectionCtx::GetInstance()->isReady()) {
      MOZ_CRASH(
          "Why is DeferredCreateOffer being executed when the "
          "PeerConnectionCtx isn't ready?");
    }
    wrapper.impl()->CreateOffer(aOptions);
  }
}

// Have to use unique_ptr because webidl enums are generated without a
// copy c'tor.
static std::unique_ptr<dom::PCErrorData> buildJSErrorData(
    const JsepSession::Result& aResult, const std::string& aMessage) {
  std::unique_ptr<dom::PCErrorData> result(new dom::PCErrorData);
  result->mName = *aResult.mError;
  result->mMessage = NS_ConvertASCIItoUTF16(aMessage.c_str());
  return result;
}

// Used by unit tests and the IDL CreateOffer.
NS_IMETHODIMP
PeerConnectionImpl::CreateOffer(const JsepOfferOptions& aOptions) {
  PC_AUTO_ENTER_API_CALL(true);

  if (!PeerConnectionCtx::GetInstance()->isReady()) {
    // Uh oh. We're not ready yet. Enqueue this operation.
    PeerConnectionCtx::GetInstance()->queueJSEPOperation(
        WrapRunnableNM(DeferredCreateOffer, mHandle, aOptions));
    STAMP_TIMECARD(mTimeCard, "Deferring CreateOffer (not ready)");
    return NS_OK;
  }

  CSFLogDebug(LOGTAG, "CreateOffer()");

  nsresult nrv = ConfigureJsepSessionCodecs();
  if (NS_FAILED(nrv)) {
    CSFLogError(LOGTAG, "Failed to configure codecs");
    return nrv;
  }

  STAMP_TIMECARD(mTimeCard, "Create Offer");

  GetMainThreadEventTarget()->Dispatch(NS_NewRunnableFunction(
      __func__, [this, self = RefPtr<PeerConnectionImpl>(this), aOptions] {
        std::string offer;

        JsepSession::Result result =
            mJsepSession->CreateOffer(aOptions, &offer);
        JSErrorResult rv;
        if (result.mError.isSome()) {
          std::string errorString = mJsepSession->GetLastError();

          CSFLogError(LOGTAG, "%s: pc = %s, error = %s", __FUNCTION__,
                      mHandle.c_str(), errorString.c_str());

          mPCObserver->OnCreateOfferError(
              *buildJSErrorData(result, errorString), rv);
        } else {
          mPCObserver->OnCreateOfferSuccess(ObString(offer.c_str()), rv);
        }
      }));

  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::CreateAnswer() {
  PC_AUTO_ENTER_API_CALL(true);

  CSFLogDebug(LOGTAG, "CreateAnswer()");

  STAMP_TIMECARD(mTimeCard, "Create Answer");
  // TODO(bug 1098015): Once RTCAnswerOptions is standardized, we'll need to
  // add it as a param to CreateAnswer, and convert it here.
  JsepAnswerOptions options;

  GetMainThreadEventTarget()->Dispatch(NS_NewRunnableFunction(
      __func__, [this, self = RefPtr<PeerConnectionImpl>(this), options] {
        std::string answer;

        JsepSession::Result result =
            mJsepSession->CreateAnswer(options, &answer);
        JSErrorResult rv;
        if (result.mError.isSome()) {
          std::string errorString = mJsepSession->GetLastError();

          CSFLogError(LOGTAG, "%s: pc = %s, error = %s", __FUNCTION__,
                      mHandle.c_str(), errorString.c_str());

          mPCObserver->OnCreateAnswerError(
              *buildJSErrorData(result, errorString), rv);
        } else {
          mPCObserver->OnCreateAnswerSuccess(ObString(answer.c_str()), rv);
        }
      }));

  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::SetLocalDescription(int32_t aAction, const char* aSDP) {
  PC_AUTO_ENTER_API_CALL(true);

  if (!aSDP) {
    CSFLogError(LOGTAG, "%s - aSDP is NULL", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  JSErrorResult rv;
  STAMP_TIMECARD(mTimeCard, "Set Local Description");

  if (AnyLocalTrackHasPeerIdentity()) {
    mPrivacyRequested = Some(true);
  }

  mozilla::dom::RTCSdpHistoryEntryInternal sdpEntry;
  sdpEntry.mIsLocal = true;
  sdpEntry.mTimestamp = mTimestampMaker.GetNow();
  sdpEntry.mSdp = NS_ConvertASCIItoUTF16(aSDP);
  auto appendHistory = [&]() {
    if (!mSdpHistory.AppendElement(sdpEntry, fallible)) {
      mozalloc_handle_oom(0);
    }
  };

  mLocalRequestedSDP = aSDP;

  bool wasRestartingIce = mJsepSession->IsIceRestarting();
  JsepSdpType sdpType;
  switch (aAction) {
    case IPeerConnection::kActionOffer:
      sdpType = mozilla::kJsepSdpOffer;
      break;
    case IPeerConnection::kActionAnswer:
      sdpType = mozilla::kJsepSdpAnswer;
      break;
    case IPeerConnection::kActionPRAnswer:
      sdpType = mozilla::kJsepSdpPranswer;
      break;
    case IPeerConnection::kActionRollback:
      sdpType = mozilla::kJsepSdpRollback;
      break;
    default:
      MOZ_ASSERT(false);
      appendHistory();
      return NS_ERROR_FAILURE;
  }
  JsepSession::Result result =
      mJsepSession->SetLocalDescription(sdpType, mLocalRequestedSDP);
  if (result.mError.isSome()) {
    std::string errorString = mJsepSession->GetLastError();
    CSFLogError(LOGTAG, "%s: pc = %s, error = %s", __FUNCTION__,
                mHandle.c_str(), errorString.c_str());
    mPCObserver->OnSetDescriptionError(*buildJSErrorData(result, errorString),
                                       rv);
    sdpEntry.mErrors = GetLastSdpParsingErrors();
  } else {
    if (wasRestartingIce) {
      RecordIceRestartStatistics(sdpType);
    }

    OnSetDescriptionSuccess(sdpType, false);
  }

  appendHistory();
  return NS_OK;
}

static void DeferredSetRemote(const std::string& aPcHandle, int32_t aAction,
                              const std::string& aSdp) {
  PeerConnectionWrapper wrapper(aPcHandle);

  if (wrapper.impl()) {
    if (!PeerConnectionCtx::GetInstance()->isReady()) {
      MOZ_CRASH(
          "Why is DeferredSetRemote being executed when the "
          "PeerConnectionCtx isn't ready?");
    }
    wrapper.impl()->SetRemoteDescription(aAction, aSdp.c_str());
  }
}

NS_IMETHODIMP
PeerConnectionImpl::SetRemoteDescription(int32_t action, const char* aSDP) {
  PC_AUTO_ENTER_API_CALL(true);

  if (!aSDP) {
    CSFLogError(LOGTAG, "%s - aSDP is NULL", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  JSErrorResult jrv;
  if (action == IPeerConnection::kActionOffer) {
    if (!PeerConnectionCtx::GetInstance()->isReady()) {
      // Uh oh. We're not ready yet. Enqueue this operation. (This must be a
      // remote offer, or else we would not have gotten this far)
      PeerConnectionCtx::GetInstance()->queueJSEPOperation(WrapRunnableNM(
          DeferredSetRemote, mHandle, action, std::string(aSDP)));
      STAMP_TIMECARD(mTimeCard, "Deferring SetRemote (not ready)");
      return NS_OK;
    }

    nsresult nrv = ConfigureJsepSessionCodecs();
    if (NS_FAILED(nrv)) {
      CSFLogError(LOGTAG, "Failed to configure codecs");
      return nrv;
    }
  }

  STAMP_TIMECARD(mTimeCard, "Set Remote Description");

  mozilla::dom::RTCSdpHistoryEntryInternal sdpEntry;
  sdpEntry.mIsLocal = false;
  sdpEntry.mTimestamp = mTimestampMaker.GetNow();
  sdpEntry.mSdp = NS_ConvertASCIItoUTF16(aSDP);
  auto appendHistory = [&]() {
    if (!mSdpHistory.AppendElement(sdpEntry, fallible)) {
      mozalloc_handle_oom(0);
    }
  };

  mRemoteRequestedSDP = aSDP;
  bool wasRestartingIce = mJsepSession->IsIceRestarting();
  JsepSdpType sdpType;
  switch (action) {
    case IPeerConnection::kActionOffer:
      sdpType = mozilla::kJsepSdpOffer;
      break;
    case IPeerConnection::kActionAnswer:
      sdpType = mozilla::kJsepSdpAnswer;
      break;
    case IPeerConnection::kActionPRAnswer:
      sdpType = mozilla::kJsepSdpPranswer;
      break;
    case IPeerConnection::kActionRollback:
      sdpType = mozilla::kJsepSdpRollback;
      break;
    default:
      MOZ_ASSERT(false);
      return NS_ERROR_FAILURE;
  }

  auto originalTransceivers = mJsepSession->GetTransceivers();
  JsepSession::Result result =
      mJsepSession->SetRemoteDescription(sdpType, mRemoteRequestedSDP);
  if (result.mError.isSome()) {
    std::string errorString = mJsepSession->GetLastError();
    sdpEntry.mErrors = GetLastSdpParsingErrors();
    CSFLogError(LOGTAG, "%s: pc = %s, error = %s", __FUNCTION__,
                mHandle.c_str(), errorString.c_str());
    mPCObserver->OnSetDescriptionError(*buildJSErrorData(result, errorString),
                                       jrv);
  } else {
    for (const auto& [id, jsepTransceiver] : mJsepSession->GetTransceivers()) {
      if (jsepTransceiver->GetMediaType() ==
          SdpMediaSection::MediaType::kApplication) {
        continue;
      }

      if (originalTransceivers.count(id)) {
        continue;
      }

      dom::RTCRtpTransceiverInit init;
      init.mDirection = RTCRtpTransceiverDirection::Recvonly;
      RefPtr<RTCRtpTransceiver> transceiver =
          CreateTransceiver(jsepTransceiver, init, nullptr, jrv);
      if (jrv.Failed()) {
        appendHistory();
        return NS_ERROR_FAILURE;
      }
      mTransceivers.AppendElement(transceiver);
    }

    if (wasRestartingIce) {
      RecordIceRestartStatistics(sdpType);
    }

    OnSetDescriptionSuccess(sdpType, true);
  }

  appendHistory();
  return NS_OK;
}

already_AddRefed<dom::Promise> PeerConnectionImpl::GetStats(
    MediaStreamTrack* aSelector) {
  if (!mWindow) {
    MOZ_CRASH("Cannot create a promise without a window!");
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mWindow);
  ErrorResult rv;
  RefPtr<Promise> promise = Promise::Create(global, rv);
  if (NS_WARN_IF(rv.Failed())) {
    MOZ_CRASH("Failed to create a promise!");
  }

  if (!IsClosed()) {
    GetStats(aSelector, false)
        ->Then(
            GetMainThreadSerialEventTarget(), __func__,
            [promise, window = mWindow](
                UniquePtr<dom::RTCStatsReportInternal>&& aReport) {
              RefPtr<RTCStatsReport> report(new RTCStatsReport(window));
              report->Incorporate(*aReport);
              promise->MaybeResolve(std::move(report));
            },
            [promise, window = mWindow](nsresult aError) {
              RefPtr<RTCStatsReport> report(new RTCStatsReport(window));
              promise->MaybeResolve(std::move(report));
            });
  } else {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
  }

  return promise.forget();
}

void PeerConnectionImpl::GetRemoteStreams(
    nsTArray<RefPtr<DOMMediaStream>>& aStreamsOut) const {
  aStreamsOut = mReceiveStreams.Clone();
}

NS_IMETHODIMP
PeerConnectionImpl::AddIceCandidate(
    const char* aCandidate, const char* aMid, const char* aUfrag,
    const dom::Nullable<unsigned short>& aLevel) {
  PC_AUTO_ENTER_API_CALL(true);

  if (mForceIceTcp &&
      std::string::npos != std::string(aCandidate).find(" UDP ")) {
    CSFLogError(LOGTAG, "Blocking remote UDP candidate: %s", aCandidate);
    return NS_OK;
  }

  STAMP_TIMECARD(mTimeCard, "Add Ice Candidate");

  CSFLogDebug(LOGTAG, "AddIceCandidate: %s %s", aCandidate, aUfrag);

  std::string transportId;
  Maybe<unsigned short> level;
  if (!aLevel.IsNull()) {
    level = Some(aLevel.Value());
  }
  JsepSession::Result result = mJsepSession->AddRemoteIceCandidate(
      aCandidate, aMid, level, aUfrag, &transportId);

  if (!result.mError.isSome()) {
    // We do not bother the MediaTransportHandler about this before
    // offer/answer concludes.  Once offer/answer concludes, we will extract
    // these candidates from the remote SDP.
    if (mSignalingState == RTCSignalingState::Stable && !transportId.empty()) {
      AddIceCandidate(aCandidate, transportId, aUfrag);
      mRawTrickledCandidates.push_back(aCandidate);
    }
    // Spec says we queue a task for these updates
    GetMainThreadEventTarget()->Dispatch(NS_NewRunnableFunction(
        __func__, [this, self = RefPtr<PeerConnectionImpl>(this)] {
          if (IsClosed()) {
            return;
          }
          mPendingRemoteDescription =
              mJsepSession->GetRemoteDescription(kJsepDescriptionPending);
          mCurrentRemoteDescription =
              mJsepSession->GetRemoteDescription(kJsepDescriptionCurrent);
          JSErrorResult rv;
          mPCObserver->OnAddIceCandidateSuccess(rv);
        }));
  } else {
    std::string errorString = mJsepSession->GetLastError();

    CSFLogError(LOGTAG,
                "Failed to incorporate remote candidate into SDP:"
                " res = %u, candidate = %s, level = %i, error = %s",
                static_cast<unsigned>(*result.mError), aCandidate,
                level.valueOr(-1), errorString.c_str());

    GetMainThreadEventTarget()->Dispatch(NS_NewRunnableFunction(
        __func__,
        [this, self = RefPtr<PeerConnectionImpl>(this), errorString, result] {
          if (IsClosed()) {
            return;
          }
          JSErrorResult rv;
          mPCObserver->OnAddIceCandidateError(
              *buildJSErrorData(result, errorString), rv);
        }));
  }

  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::CloseStreams() {
  PC_AUTO_ENTER_API_CALL(false);

  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::SetPeerIdentity(const nsAString& aPeerIdentity) {
  PC_AUTO_ENTER_API_CALL(true);
  MOZ_ASSERT(!aPeerIdentity.IsEmpty());

  // once set, this can't be changed
  if (mPeerIdentity) {
    if (!mPeerIdentity->Equals(aPeerIdentity)) {
      return NS_ERROR_FAILURE;
    }
  } else {
    mPeerIdentity = new PeerIdentity(aPeerIdentity);
    Document* doc = mWindow->GetExtantDoc();
    if (!doc) {
      CSFLogInfo(LOGTAG, "Can't update principal on streams; document gone");
      return NS_ERROR_FAILURE;
    }
    for (const auto& transceiver : mTransceivers) {
      transceiver->Sender()->GetPipeline()->UpdateSinkIdentity(
          doc->NodePrincipal(), mPeerIdentity);
    }
  }
  return NS_OK;
}

nsresult PeerConnectionImpl::OnAlpnNegotiated(bool aPrivacyRequested) {
  PC_AUTO_ENTER_API_CALL(false);
  if (mPrivacyRequested.isSome()) {
    MOZ_DIAGNOSTIC_ASSERT(*mPrivacyRequested == aPrivacyRequested);
    return NS_OK;
  }

  mPrivacyRequested = Some(aPrivacyRequested);
  return NS_OK;
}

void PeerConnectionImpl::OnMediaError(const std::string& aError) {
  CSFLogError(LOGTAG, "Encountered media error! %s", aError.c_str());
  // TODO: Let content know about this somehow.
}

void PeerConnectionImpl::DumpPacket_m(size_t level, dom::mozPacketDumpType type,
                                      bool sending,
                                      UniquePtr<uint8_t[]>& packet,
                                      size_t size) {
  if (IsClosed()) {
    return;
  }

  // TODO: Is this efficient? Should we try grabbing our JS ctx from somewhere
  // else?
  AutoJSAPI jsapi;
  if (!jsapi.Init(mWindow)) {
    return;
  }

  JS::Rooted<JSObject*> jsobj(
      jsapi.cx(),
      JS::NewArrayBufferWithContents(jsapi.cx(), size, packet.release()));

  RootedSpiderMonkeyInterface<ArrayBuffer> arrayBuffer(jsapi.cx());
  if (!arrayBuffer.Init(jsobj)) {
    return;
  }

  JSErrorResult jrv;
  mPCObserver->OnPacket(level, type, sending, arrayBuffer, jrv);
}

bool PeerConnectionImpl::HostnameInPref(const char* aPref, nsIURI* aDocURI) {
  auto HostInDomain = [](const nsCString& aHost, const nsCString& aPattern) {
    int32_t patternOffset = 0;
    int32_t hostOffset = 0;

    // Act on '*.' wildcard in the left-most position in a domain pattern.
    if (StringBeginsWith(aPattern, nsCString("*."))) {
      patternOffset = 2;

      // Ignore the lowest level sub-domain for the hostname.
      hostOffset = aHost.FindChar('.') + 1;

      if (hostOffset <= 1) {
        // Reject a match between a wildcard and a TLD or '.foo' form.
        return false;
      }
    }

    nsDependentCString hostRoot(aHost, hostOffset);
    return hostRoot.EqualsIgnoreCase(aPattern.BeginReading() + patternOffset);
  };

  if (!aDocURI) {
    return false;
  }

  nsCString hostName;
  aDocURI->GetAsciiHost(hostName);  // normalize UTF8 to ASCII equivalent
  nsCString domainList;
  nsresult nr = Preferences::GetCString(aPref, domainList);

  if (NS_FAILED(nr)) {
    return false;
  }

  domainList.StripWhitespace();

  if (domainList.IsEmpty() || hostName.IsEmpty()) {
    return false;
  }

  // Get UTF8 to ASCII domain name normalization service
  nsresult rv;
  nsCOMPtr<nsIIDNService> idnService =
      do_GetService("@mozilla.org/network/idn-service;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  // Test each domain name in the comma separated list
  // after converting from UTF8 to ASCII. Each domain
  // must match exactly or have a single leading '*.' wildcard.
  for (const nsACString& each : domainList.Split(',')) {
    nsCString domainName;
    rv = idnService->ConvertUTF8toACE(each, domainName);
    if (NS_SUCCEEDED(rv)) {
      if (HostInDomain(hostName, domainName)) {
        return true;
      }
    } else {
      NS_WARNING("Failed to convert UTF-8 host to ASCII");
    }
  }

  return false;
}

nsresult PeerConnectionImpl::EnablePacketDump(unsigned long level,
                                              dom::mozPacketDumpType type,
                                              bool sending) {
  return GetPacketDumper()->EnablePacketDump(level, type, sending);
}

nsresult PeerConnectionImpl::DisablePacketDump(unsigned long level,
                                               dom::mozPacketDumpType type,
                                               bool sending) {
  return GetPacketDumper()->DisablePacketDump(level, type, sending);
}

void PeerConnectionImpl::StampTimecard(const char* aEvent) {
  MOZ_ASSERT(NS_IsMainThread());
  STAMP_TIMECARD(mTimeCard, aEvent);
}

nsresult PeerConnectionImpl::CalculateFingerprint(
    const std::string& algorithm, std::vector<uint8_t>* fingerprint) const {
  DtlsDigest digest(algorithm);

  MOZ_ASSERT(fingerprint);
  const UniqueCERTCertificate& cert = mCertificate->Certificate();
  nsresult rv = DtlsIdentity::ComputeFingerprint(cert, &digest);
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "Unable to calculate certificate fingerprint, rv=%u",
                static_cast<unsigned>(rv));
    return rv;
  }
  *fingerprint = digest.value_;
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::GetFingerprint(char** fingerprint) {
  MOZ_ASSERT(fingerprint);
  MOZ_ASSERT(mCertificate);
  std::vector<uint8_t> fp;
  nsresult rv = CalculateFingerprint(DtlsIdentity::DEFAULT_HASH_ALGORITHM, &fp);
  NS_ENSURE_SUCCESS(rv, rv);
  std::ostringstream os;
  os << DtlsIdentity::DEFAULT_HASH_ALGORITHM << ' '
     << SdpFingerprintAttributeList::FormatFingerprint(fp);
  std::string fpStr = os.str();

  char* tmp = new char[fpStr.size() + 1];
  std::copy(fpStr.begin(), fpStr.end(), tmp);
  tmp[fpStr.size()] = '\0';

  *fingerprint = tmp;
  return NS_OK;
}

void PeerConnectionImpl::GetCurrentLocalDescription(nsAString& aSDP) const {
  aSDP = NS_ConvertASCIItoUTF16(mCurrentLocalDescription.c_str());
}

void PeerConnectionImpl::GetPendingLocalDescription(nsAString& aSDP) const {
  aSDP = NS_ConvertASCIItoUTF16(mPendingLocalDescription.c_str());
}

void PeerConnectionImpl::GetCurrentRemoteDescription(nsAString& aSDP) const {
  aSDP = NS_ConvertASCIItoUTF16(mCurrentRemoteDescription.c_str());
}

void PeerConnectionImpl::GetPendingRemoteDescription(nsAString& aSDP) const {
  aSDP = NS_ConvertASCIItoUTF16(mPendingRemoteDescription.c_str());
}

dom::Nullable<bool> PeerConnectionImpl::GetCurrentOfferer() const {
  dom::Nullable<bool> result;
  if (mCurrentOfferer.isSome()) {
    result.SetValue(*mCurrentOfferer);
  }
  return result;
}

dom::Nullable<bool> PeerConnectionImpl::GetPendingOfferer() const {
  dom::Nullable<bool> result;
  if (mPendingOfferer.isSome()) {
    result.SetValue(*mPendingOfferer);
  }
  return result;
}

NS_IMETHODIMP
PeerConnectionImpl::SignalingState(RTCSignalingState* aState) {
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(aState);

  *aState = mSignalingState;
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::IceConnectionState(RTCIceConnectionState* aState) {
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(aState);

  *aState = mIceConnectionState;
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::IceGatheringState(RTCIceGatheringState* aState) {
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(aState);

  *aState = mIceGatheringState;
  return NS_OK;
}

nsresult PeerConnectionImpl::CheckApiState(bool assert_ice_ready) const {
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(mTrickle || !assert_ice_ready ||
             (mIceGatheringState == RTCIceGatheringState::Complete));

  if (IsClosed()) {
    CSFLogError(LOGTAG, "%s: called API while closed", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::Close() {
  CSFLogDebug(LOGTAG, "%s: for %s", __FUNCTION__, mHandle.c_str());
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  if (IsClosed()) {
    return NS_OK;
  }

  STAMP_TIMECARD(mTimeCard, "Close");

  // When ICE completes, we record a bunch of statistics that outlive the
  // PeerConnection. This includes a call to GetStats, as well as some
  // telemetry. We do this at the end of the call because we want to make sure
  // we've waited for all trickle ICE candidates to come in; this can happen
  // well after we've transitioned to connected. As a bonus, this allows us to
  // detect race conditions where a stats dispatch happens right as the PC
  // closes.
  if (!mPrivateWindow) {
    WebrtcGlobalInformation::StoreLongTermICEStatistics(*this);
  }
  RecordEndOfCallTelemetry();

  CSFLogInfo(LOGTAG,
             "%s: Closing PeerConnectionImpl %s; "
             "ending call",
             __FUNCTION__, mHandle.c_str());
  if (mJsepSession) {
    mJsepSession->Close();
  }
  if (mDataConnection) {
    CSFLogInfo(LOGTAG, "%s: Destroying DataChannelConnection %p for %s",
               __FUNCTION__, (void*)mDataConnection.get(), mHandle.c_str());
    mDataConnection->Destroy();
    mDataConnection =
        nullptr;  // it may not go away until the runnables are dead
  }

  if (mStunAddrsRequest) {
    for (const auto& hostname : mRegisteredMDNSHostnames) {
      mStunAddrsRequest->SendUnregisterMDNSHostname(
          nsCString(hostname.c_str()));
    }
    mRegisteredMDNSHostnames.clear();
    mStunAddrsRequest->Cancel();
    mStunAddrsRequest = nullptr;
  }

  for (auto& transceiver : mTransceivers) {
    // transceivers are garbage-collected, so we need to poke them to perform
    // cleanup right now so the appropriate events fire.
    transceiver->Shutdown_m();
  }

  mQueuedIceCtxOperations.clear();

  mOperations.Clear();

  // Uncount this connection as active on the inner window upon close.
  if (mWindow && mActiveOnWindow) {
    mWindow->RemovePeerConnection();
    mActiveOnWindow = false;
  }

  mSignalingState = RTCSignalingState::Closed;

  if (!mTransportHandler) {
    // We were never initialized, apparently.
    return NS_OK;
  }

  // Clear any resources held by libwebrtc through our Call instance.
  RefPtr<GenericPromise> promise;
  if (mCall) {
    // Make sure the compiler does not get confused and try to acquire a
    // reference to this thread _after_ we null out mCall.
    auto callThread = mCall->mCallThread;
    promise = InvokeAsync(callThread, __func__, [call = std::move(mCall)]() {
      call->Destroy();
      return GenericPromise::CreateAndResolve(
          true, "PCImpl->WebRtcCallWrapper::Destroy");
    });
  } else {
    promise = GenericPromise::CreateAndResolve(true, __func__);
  }

  // We do this after the call is destroyed, to allow things like RTCP BYE to
  // make it out on the wire before we shut the MediaTransportHandler down.
  // Before we can tear down the MediaTransportHandler, we also need to unhook
  // from sigslot, which is accomplished by destroying mSignalHandler.
  MOZ_RELEASE_ASSERT(mSTSThread);
  promise
      ->Then(
          mSTSThread, __func__,
          [signalHandler = std::move(mSignalHandler)]() mutable {
            CSFLogDebug(
                LOGTAG,
                "Destroying PeerConnectionImpl::SignalHandler on STS thread");
            return GenericPromise::CreateAndResolve(
                true, "PeerConnectionImpl::~SignalHandler");
          })
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [this, self = RefPtr<PeerConnectionImpl>(this)]() mutable {
            CSFLogDebug(LOGTAG, "PCImpl->mTransportHandler::RemoveTransports");
            mTransportHandler->RemoveTransportsExcept(std::set<std::string>());
            if (mPrivateWindow) {
              mTransportHandler->ExitPrivateMode();
            }
            mTransportHandler = nullptr;
            if (PeerConnectionCtx::isActive()) {
              // If we're shutting down xpcom, this Instance will be unset
              // before calling Close() on all remaining PCs, to avoid
              // reentrancy.
              PeerConnectionCtx::GetInstance()->RemovePeerConnection(mHandle);
            }
          });

  return NS_OK;
}

nsresult PeerConnectionImpl::SetConfiguration(
    const RTCConfiguration& aConfiguration) {
  nsresult rv = mTransportHandler->SetIceConfig(
      aConfiguration.mIceServers, aConfiguration.mIceTransportPolicy);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  JsepBundlePolicy bundlePolicy;
  switch (aConfiguration.mBundlePolicy) {
    case dom::RTCBundlePolicy::Balanced:
      bundlePolicy = kBundleBalanced;
      break;
    case dom::RTCBundlePolicy::Max_compat:
      bundlePolicy = kBundleMaxCompat;
      break;
    case dom::RTCBundlePolicy::Max_bundle:
      bundlePolicy = kBundleMaxBundle;
      break;
    default:
      MOZ_CRASH();
  }

  rv = mJsepSession->SetBundlePolicy(bundlePolicy);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    CSFLogError(LOGTAG, "%s: Couldn't set bundle policy, res=%u, error=%s",
                __FUNCTION__, static_cast<unsigned>(rv),
                mJsepSession->GetLastError().c_str());
    return rv;
  }

  if (!aConfiguration.mPeerIdentity.IsEmpty()) {
    mPeerIdentity = new PeerIdentity(aConfiguration.mPeerIdentity);
    mPrivacyRequested = Some(true);
  }

  // Store the configuration for about:webrtc
  StoreConfigurationForAboutWebrtc(aConfiguration);
  return NS_OK;
}

void PeerConnectionImpl::RestartIce() {
  RestartIceNoRenegotiationNeeded();
  // Update the negotiation-needed flag for connection.
  UpdateNegotiationNeeded();
}

// webrtc-pc does not specify any situations where this is done, but the JSEP
// spec does, in some situations due to setConfiguration.
void PeerConnectionImpl::RestartIceNoRenegotiationNeeded() {
  // Empty connection.[[LocalIceCredentialsToReplace]], and populate it with
  // all ICE credentials (ice-ufrag and ice-pwd as defined in section 15.4 of
  // [RFC5245]) found in connection.[[CurrentLocalDescription]], as well as all
  // ICE credentials found in connection.[[PendingLocalDescription]].
  mLocalIceCredentialsToReplace = mJsepSession->GetLocalIceCredentials();
}

bool PeerConnectionImpl::PluginCrash(uint32_t aPluginID,
                                     const nsAString& aPluginName) {
  // fire an event to the DOM window if this is "ours"
  if (!AnyCodecHasPluginID(aPluginID)) {
    return false;
  }

  CSFLogError(LOGTAG, "%s: Our plugin %llu crashed", __FUNCTION__,
              static_cast<unsigned long long>(aPluginID));

  RefPtr<Document> doc = mWindow->GetExtantDoc();
  if (!doc) {
    NS_WARNING("Couldn't get document for PluginCrashed event!");
    return true;
  }

  PluginCrashedEventInit init;
  init.mPluginID = aPluginID;
  init.mPluginName = aPluginName;
  init.mSubmittedCrashReport = false;
  init.mGmpPlugin = true;
  init.mBubbles = true;
  init.mCancelable = true;

  RefPtr<PluginCrashedEvent> event =
      PluginCrashedEvent::Constructor(doc, u"PluginCrashed"_ns, init);

  event->SetTrusted(true);
  event->WidgetEventPtr()->mFlags.mOnlyChromeDispatch = true;

  nsCOMPtr<nsPIDOMWindowInner> window = mWindow;
  EventDispatcher::DispatchDOMEvent(window, nullptr, event, nullptr, nullptr);

  return true;
}

void PeerConnectionImpl::RecordEndOfCallTelemetry() {
  if (!mCallTelemStarted) {
    return;
  }
  MOZ_RELEASE_ASSERT(!mCallTelemEnded, "Don't end telemetry twice");
  MOZ_RELEASE_ASSERT(mJsepSession,
                     "Call telemetry only starts after jsep session start");
  MOZ_RELEASE_ASSERT(mJsepSession->GetNegotiations() > 0,
                     "Call telemetry only starts after first connection");

  // Bitmask used for WEBRTC/LOOP_CALL_TYPE telemetry reporting
  static const uint32_t kAudioTypeMask = 1;
  static const uint32_t kVideoTypeMask = 2;
  static const uint32_t kDataChannelTypeMask = 4;

  // Report end-of-call Telemetry
  Telemetry::Accumulate(Telemetry::WEBRTC_RENEGOTIATIONS,
                        mJsepSession->GetNegotiations() - 1);
  Telemetry::Accumulate(Telemetry::WEBRTC_MAX_VIDEO_SEND_TRACK,
                        mMaxSending[SdpMediaSection::MediaType::kVideo]);
  Telemetry::Accumulate(Telemetry::WEBRTC_MAX_VIDEO_RECEIVE_TRACK,
                        mMaxReceiving[SdpMediaSection::MediaType::kVideo]);
  Telemetry::Accumulate(Telemetry::WEBRTC_MAX_AUDIO_SEND_TRACK,
                        mMaxSending[SdpMediaSection::MediaType::kAudio]);
  Telemetry::Accumulate(Telemetry::WEBRTC_MAX_AUDIO_RECEIVE_TRACK,
                        mMaxReceiving[SdpMediaSection::MediaType::kAudio]);
  // DataChannels appear in both Sending and Receiving
  Telemetry::Accumulate(Telemetry::WEBRTC_DATACHANNEL_NEGOTIATED,
                        mMaxSending[SdpMediaSection::MediaType::kApplication]);
  // Enumerated/bitmask: 1 = Audio, 2 = Video, 4 = DataChannel
  // A/V = 3, A/V/D = 7, etc
  uint32_t type = 0;
  if (mMaxSending[SdpMediaSection::MediaType::kAudio] ||
      mMaxReceiving[SdpMediaSection::MediaType::kAudio]) {
    type = kAudioTypeMask;
  }
  if (mMaxSending[SdpMediaSection::MediaType::kVideo] ||
      mMaxReceiving[SdpMediaSection::MediaType::kVideo]) {
    type |= kVideoTypeMask;
  }
  if (mMaxSending[SdpMediaSection::MediaType::kApplication]) {
    type |= kDataChannelTypeMask;
  }
  Telemetry::Accumulate(Telemetry::WEBRTC_CALL_TYPE, type);

  MOZ_RELEASE_ASSERT(mWindow);
  auto found = sCallDurationTimers.find(mWindow->WindowID());
  if (found != sCallDurationTimers.end()) {
    found->second.UnregisterConnection((type & kAudioTypeMask) ||
                                       (type & kVideoTypeMask));
    if (found->second.IsStopped()) {
      sCallDurationTimers.erase(found);
    }
  }
  mCallTelemEnded = true;
}

DOMMediaStream* PeerConnectionImpl::GetReceiveStream(
    const std::string& aId) const {
  nsString wanted = NS_ConvertASCIItoUTF16(aId.c_str());
  for (auto& stream : mReceiveStreams) {
    nsString id;
    stream->GetId(id);
    if (id == wanted) {
      return stream;
    }
  }
  return nullptr;
}

DOMMediaStream* PeerConnectionImpl::CreateReceiveStream(
    const std::string& aId) {
  mReceiveStreams.AppendElement(new DOMMediaStream(mWindow));
  mReceiveStreams.LastElement()->AssignId(NS_ConvertASCIItoUTF16(aId.c_str()));
  return mReceiveStreams.LastElement();
}

void PeerConnectionImpl::OnSetDescriptionSuccess(JsepSdpType sdpType,
                                                 bool remote) {
  // Spec says we queue a task for all the stuff that ends up back in JS
  auto newSignalingState = GetSignalingState();

  GetMainThreadEventTarget()->Dispatch(NS_NewRunnableFunction(
      __func__, [this, self = RefPtr<PeerConnectionImpl>(this),
                 newSignalingState, sdpType, remote] {
        if (IsClosed()) {
          return;
        }

        // Section 4.4.1.5 Set the RTCSessionDescription:
        if (sdpType == mozilla::kJsepSdpRollback) {
          // - step 4.5.10, type is rollback
          RollbackRTCDtlsTransports();
        } else if (!(remote && sdpType == mozilla::kJsepSdpOffer)) {
          // - step 4.5.9 type is not rollback
          // - step 4.5.9.1 when remote is false
          // - step 4.5.9.2.13 when remote is true, type answer or pranswer
          // More simply: not rollback, and not for remote offers.
          bool markAsStable = sdpType == kJsepSdpOffer &&
                              mSignalingState == RTCSignalingState::Stable;
          UpdateRTCDtlsTransports(markAsStable);
        }

        for (auto& transceiver : mTransceivers) {
          transceiver->SyncWithJsep();
        }

        mPendingRemoteDescription =
            mJsepSession->GetRemoteDescription(kJsepDescriptionPending);
        mCurrentRemoteDescription =
            mJsepSession->GetRemoteDescription(kJsepDescriptionCurrent);
        mPendingLocalDescription =
            mJsepSession->GetLocalDescription(kJsepDescriptionPending);
        mCurrentLocalDescription =
            mJsepSession->GetLocalDescription(kJsepDescriptionCurrent);
        mPendingOfferer = mJsepSession->IsPendingOfferer();
        mCurrentOfferer = mJsepSession->IsCurrentOfferer();

        if (sdpType == mozilla::kJsepSdpAnswer) {
          std::set<std::pair<std::string, std::string>> iceCredentials =
              mJsepSession->GetLocalIceCredentials();
          std::vector<std::pair<std::string, std::string>>
              iceCredentialsNotReplaced;
          std::set_intersection(mLocalIceCredentialsToReplace.begin(),
                                mLocalIceCredentialsToReplace.end(),
                                iceCredentials.begin(), iceCredentials.end(),
                                std::back_inserter(iceCredentialsNotReplaced));

          if (iceCredentialsNotReplaced.empty()) {
            mLocalIceCredentialsToReplace.clear();
          }
        }

        if (newSignalingState == RTCSignalingState::Stable) {
          mNegotiationNeeded = false;
          UpdateNegotiationNeeded();
        }

        JSErrorResult jrv;
        if (newSignalingState != mSignalingState) {
          mSignalingState = newSignalingState;
          mPCObserver->OnStateChange(PCObserverStateType::SignalingState, jrv);
        }

        if (remote) {
          dom::RTCRtpReceiver::StreamAssociationChanges changes;
          for (const auto& transceiver : mTransceivers) {
            transceiver->Receiver()->UpdateStreams(&changes);
          }

          for (const auto& track : changes.mTracksToMute) {
            // This sets the muted state for track and all its clones.
            static_cast<RemoteTrackSource&>(track->GetSource()).SetMuted(true);
          }

          for (const auto& association : changes.mStreamAssociationsRemoved) {
            RefPtr<DOMMediaStream> stream =
                GetReceiveStream(association.mStreamId);
            if (stream && stream->HasTrack(*association.mTrack)) {
              stream->RemoveTrackInternal(association.mTrack);
            }
          }

          // TODO(Bug 1241291): For legacy event, remove eventually
          std::vector<RefPtr<DOMMediaStream>> newStreams;

          for (const auto& association : changes.mStreamAssociationsAdded) {
            RefPtr<DOMMediaStream> stream =
                GetReceiveStream(association.mStreamId);
            if (!stream) {
              stream = CreateReceiveStream(association.mStreamId);
              newStreams.push_back(stream);
            }

            if (!stream->HasTrack(*association.mTrack)) {
              stream->AddTrackInternal(association.mTrack);
            }
          }

          // Make sure to wait until after we've calculated track changes before
          // doing this.
          for (size_t i = 0; i < mTransceivers.Length();) {
            auto& transceiver = mTransceivers[i];
            if (transceiver->ShouldRemove()) {
              // TODO: Can we make Shutdown_m and StopImpl the same thing?
              mTransceivers[i]->Shutdown_m();
              mTransceivers.RemoveElementAt(i);
            } else {
              ++i;
            }
          }

          for (const auto& trackEvent : changes.mTrackEvents) {
            dom::Sequence<OwningNonNull<DOMMediaStream>> streams;
            for (const auto& id : trackEvent.mStreamIds) {
              RefPtr<DOMMediaStream> stream = GetReceiveStream(id);
              if (!stream) {
                MOZ_ASSERT(false);
                continue;
              }
              if (!streams.AppendElement(*stream, fallible)) {
                // XXX(Bug 1632090) Instead of extending the array 1-by-1 (which
                // might involve multiple reallocations) and potentially
                // crashing here, SetCapacity could be called outside the loop
                // once.
                mozalloc_handle_oom(0);
              }
            }
            mPCObserver->FireTrackEvent(*trackEvent.mReceiver, streams, jrv);
          }

          // TODO(Bug 1241291): Legacy event, remove eventually
          for (const auto& stream : newStreams) {
            mPCObserver->FireStreamEvent(*stream, jrv);
          }
        }

        mPCObserver->OnSetDescriptionSuccess(jrv);
      }));

  // We do this after queueing the above task, to ensure that ICE state
  // changes don't start happening before sRD finishes.

  // Did we just apply a local description?
  if (!remote) {
    // We'd like to handle this in PeerConnectionImpl::UpdateNetworkState.
    // Unfortunately, if the WiFi switch happens quickly, we never see
    // that state change.  We need to detect the ice restart here and
    // reset the PeerConnectionImpl's stun addresses so they are
    // regathered when PeerConnectionImpl::GatherIfReady is called.
    if (mJsepSession->IsIceRestarting()) {
      ResetStunAddrsForIceRestart();
    }
    EnsureTransports(*mJsepSession);
  }

  if (mJsepSession->GetState() != kJsepStateStable) {
    return;  // The rest of this stuff is done only when offer/answer is done
  }

  // If we're rolling back a local offer, we might need to remove some
  // transports, and stomp some MediaPipeline setup, but nothing further
  // needs to be done.
  UpdateTransports(*mJsepSession, mForceIceTcp);
  if (NS_FAILED(UpdateMediaPipelines())) {
    CSFLogError(LOGTAG, "Error Updating MediaPipelines");
    NS_ASSERTION(false,
                 "Error Updating MediaPipelines in OnSetDescriptionSuccess()");
    // XXX what now?  Not much we can do but keep going, without major
    // restructuring
  }

  if (sdpType != kJsepSdpRollback) {
    InitializeDataChannel();
    StartIceChecks(*mJsepSession);
  }

  // Telemetry: record info on the current state of streams/renegotiations/etc
  // Note: this code gets run on rollbacks as well!

  // Update the max channels used with each direction for each type
  uint16_t receiving[SdpMediaSection::kMediaTypes];
  uint16_t sending[SdpMediaSection::kMediaTypes];
  mJsepSession->CountTracksAndDatachannels(receiving, sending);
  for (size_t i = 0; i < SdpMediaSection::kMediaTypes; i++) {
    if (mMaxReceiving[i] < receiving[i]) {
      mMaxReceiving[i] = receiving[i];
    }
    if (mMaxSending[i] < sending[i]) {
      mMaxSending[i] = sending[i];
    }
  }
}

RTCSignalingState PeerConnectionImpl::GetSignalingState() const {
  switch (mJsepSession->GetState()) {
    case kJsepStateStable:
      return RTCSignalingState::Stable;
      break;
    case kJsepStateHaveLocalOffer:
      return RTCSignalingState::Have_local_offer;
      break;
    case kJsepStateHaveRemoteOffer:
      return RTCSignalingState::Have_remote_offer;
      break;
    case kJsepStateHaveLocalPranswer:
      return RTCSignalingState::Have_local_pranswer;
      break;
    case kJsepStateHaveRemotePranswer:
      return RTCSignalingState::Have_remote_pranswer;
      break;
    case kJsepStateClosed:
      return RTCSignalingState::Closed;
      break;
  }
  MOZ_CRASH("Invalid JSEP state");
}

bool PeerConnectionImpl::IsClosed() const {
  return mSignalingState == RTCSignalingState::Closed;
}

PeerConnectionWrapper::PeerConnectionWrapper(const std::string& handle)
    : impl_(nullptr) {
  if (PeerConnectionCtx::isActive()) {
    impl_ = PeerConnectionCtx::GetInstance()->GetPeerConnection(handle);
  }
}

const RefPtr<MediaTransportHandler> PeerConnectionImpl::GetTransportHandler()
    const {
  return mTransportHandler;
}

const std::string& PeerConnectionImpl::GetHandle() { return mHandle; }

const std::string& PeerConnectionImpl::GetName() {
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  return mName;
}

void PeerConnectionImpl::CandidateReady(const std::string& candidate,
                                        const std::string& transportId,
                                        const std::string& ufrag) {
  STAMP_TIMECARD(mTimeCard, "Ice Candidate gathered");
  PC_AUTO_ENTER_API_CALL_VOID_RETURN(false);

  if (mForceIceTcp && std::string::npos != candidate.find(" UDP ")) {
    CSFLogWarn(LOGTAG, "Blocking local UDP candidate: %s", candidate.c_str());
    STAMP_TIMECARD(mTimeCard, "UDP Ice Candidate blocked");
    return;
  }

  // One of the very few places we still use level; required by the JSEP API
  uint16_t level = 0;
  std::string mid;
  bool skipped = false;
  nsresult res = mJsepSession->AddLocalIceCandidate(
      candidate, transportId, ufrag, &level, &mid, &skipped);

  if (NS_FAILED(res)) {
    std::string errorString = mJsepSession->GetLastError();

    STAMP_TIMECARD(mTimeCard, "Local Ice Candidate invalid");
    CSFLogError(LOGTAG,
                "Failed to incorporate local candidate into SDP:"
                " res = %u, candidate = %s, transport-id = %s,"
                " error = %s",
                static_cast<unsigned>(res), candidate.c_str(),
                transportId.c_str(), errorString.c_str());
    return;
  }

  if (skipped) {
    STAMP_TIMECARD(mTimeCard, "Local Ice Candidate skipped");
    CSFLogInfo(LOGTAG,
               "Skipped adding local candidate %s (transport-id %s) "
               "to SDP, this typically happens because the m-section "
               "is bundled, which means it doesn't make sense for it "
               "to have its own transport-related attributes.",
               candidate.c_str(), transportId.c_str());
    return;
  }

  mPendingLocalDescription =
      mJsepSession->GetLocalDescription(kJsepDescriptionPending);
  mCurrentLocalDescription =
      mJsepSession->GetLocalDescription(kJsepDescriptionCurrent);
  CSFLogInfo(LOGTAG, "Passing local candidate to content: %s",
             candidate.c_str());
  SendLocalIceCandidateToContent(level, mid, candidate, ufrag);
}

void PeerConnectionImpl::SendLocalIceCandidateToContent(
    uint16_t level, const std::string& mid, const std::string& candidate,
    const std::string& ufrag) {
  STAMP_TIMECARD(mTimeCard, "Send Ice Candidate to content");
  JSErrorResult rv;
  mPCObserver->OnIceCandidate(level, ObString(mid.c_str()),
                              ObString(candidate.c_str()),
                              ObString(ufrag.c_str()), rv);
}

void PeerConnectionImpl::IceConnectionStateChange(
    dom::RTCIceConnectionState domState) {
  PC_AUTO_ENTER_API_CALL_VOID_RETURN(false);

  CSFLogDebug(LOGTAG, "%s: %d", __FUNCTION__, static_cast<int>(domState));

  if (domState == mIceConnectionState) {
    // no work to be done since the states are the same.
    // this can happen during ICE rollback situations.
    return;
  }

  mIceConnectionState = domState;

  // Would be nice if we had a means of converting one of these dom enums
  // to a string that wasn't almost as much text as this switch statement...
  switch (mIceConnectionState) {
    case RTCIceConnectionState::New:
      STAMP_TIMECARD(mTimeCard, "Ice state: new");
      break;
    case RTCIceConnectionState::Checking:
      // For telemetry
      mIceStartTime = TimeStamp::Now();
      STAMP_TIMECARD(mTimeCard, "Ice state: checking");
      break;
    case RTCIceConnectionState::Connected:
      STAMP_TIMECARD(mTimeCard, "Ice state: connected");
      StartCallTelem();
      break;
    case RTCIceConnectionState::Completed:
      STAMP_TIMECARD(mTimeCard, "Ice state: completed");
      break;
    case RTCIceConnectionState::Failed:
      STAMP_TIMECARD(mTimeCard, "Ice state: failed");
      break;
    case RTCIceConnectionState::Disconnected:
      STAMP_TIMECARD(mTimeCard, "Ice state: disconnected");
      break;
    case RTCIceConnectionState::Closed:
      STAMP_TIMECARD(mTimeCard, "Ice state: closed");
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected mIceConnectionState!");
  }

  WrappableJSErrorResult rv;
  mPCObserver->OnStateChange(PCObserverStateType::IceConnectionState, rv);
}

void PeerConnectionImpl::OnCandidateFound(const std::string& aTransportId,
                                          const CandidateInfo& aCandidateInfo) {
  if (mStunAddrsRequest && !aCandidateInfo.mMDNSAddress.empty()) {
    MOZ_ASSERT(!aCandidateInfo.mActualAddress.empty());

    if (mCanRegisterMDNSHostnamesDirectly) {
      auto itor = mRegisteredMDNSHostnames.find(aCandidateInfo.mMDNSAddress);

      // We'll see the address twice if we're generating both UDP and TCP
      // candidates.
      if (itor == mRegisteredMDNSHostnames.end()) {
        mRegisteredMDNSHostnames.insert(aCandidateInfo.mMDNSAddress);
        mStunAddrsRequest->SendRegisterMDNSHostname(
            nsCString(aCandidateInfo.mMDNSAddress.c_str()),
            nsCString(aCandidateInfo.mActualAddress.c_str()));
      }
    } else {
      mMDNSHostnamesToRegister.emplace(aCandidateInfo.mMDNSAddress,
                                       aCandidateInfo.mActualAddress);
    }
  }

  if (!aCandidateInfo.mDefaultHostRtp.empty()) {
    UpdateDefaultCandidate(aCandidateInfo.mDefaultHostRtp,
                           aCandidateInfo.mDefaultPortRtp,
                           aCandidateInfo.mDefaultHostRtcp,
                           aCandidateInfo.mDefaultPortRtcp, aTransportId);
  }
  CandidateReady(aCandidateInfo.mCandidate, aTransportId,
                 aCandidateInfo.mUfrag);
}

void PeerConnectionImpl::IceGatheringStateChange(
    dom::RTCIceGatheringState state) {
  PC_AUTO_ENTER_API_CALL_VOID_RETURN(false);

  CSFLogDebug(LOGTAG, "%s %d", __FUNCTION__, static_cast<int>(state));
  if (mIceGatheringState == state) {
    return;
  }

  mIceGatheringState = state;

  // Would be nice if we had a means of converting one of these dom enums
  // to a string that wasn't almost as much text as this switch statement...
  switch (mIceGatheringState) {
    case RTCIceGatheringState::New:
      STAMP_TIMECARD(mTimeCard, "Ice gathering state: new");
      break;
    case RTCIceGatheringState::Gathering:
      STAMP_TIMECARD(mTimeCard, "Ice gathering state: gathering");
      break;
    case RTCIceGatheringState::Complete:
      STAMP_TIMECARD(mTimeCard, "Ice gathering state: complete");
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected mIceGatheringState!");
  }

  JSErrorResult rv;
  mPCObserver->OnStateChange(PCObserverStateType::IceGatheringState, rv);
}

void PeerConnectionImpl::UpdateDefaultCandidate(
    const std::string& defaultAddr, uint16_t defaultPort,
    const std::string& defaultRtcpAddr, uint16_t defaultRtcpPort,
    const std::string& transportId) {
  CSFLogDebug(LOGTAG, "%s", __FUNCTION__);
  mJsepSession->UpdateDefaultCandidate(
      defaultAddr, defaultPort, defaultRtcpAddr, defaultRtcpPort, transportId);
}

static UniquePtr<dom::RTCStatsCollection> GetDataChannelStats_s(
    const RefPtr<DataChannelConnection>& aDataConnection,
    const DOMHighResTimeStamp aTimestamp) {
  UniquePtr<dom::RTCStatsCollection> report(new dom::RTCStatsCollection);
  if (aDataConnection) {
    aDataConnection->AppendStatsToReport(report, aTimestamp);
  }
  return report;
}

RefPtr<dom::RTCStatsPromise> PeerConnectionImpl::GetDataChannelStats(
    const RefPtr<DataChannelConnection>& aDataChannelConnection,
    const DOMHighResTimeStamp aTimestamp) {
  // Gather stats from DataChannels
  return InvokeAsync(
      GetMainThreadSerialEventTarget(), __func__,
      [aDataChannelConnection, aTimestamp]() {
        return dom::RTCStatsPromise::CreateAndResolve(
            GetDataChannelStats_s(aDataChannelConnection, aTimestamp),
            __func__);
      });
}

void PeerConnectionImpl::CollectConduitTelemetryData() {
  MOZ_ASSERT(NS_IsMainThread());

  nsTArray<RefPtr<VideoSessionConduit>> conduits;
  for (const auto& transceiver : mTransceivers) {
    if (RefPtr<MediaSessionConduit> conduit = transceiver->GetConduit()) {
      conduit->AsVideoSessionConduit().apply(
          [&](const auto& aVideo) { conduits.AppendElement(aVideo); });
    }
  }

  if (!conduits.IsEmpty() && mCall) {
    mCall->mCallThread->Dispatch(
        NS_NewRunnableFunction(__func__, [conduits = std::move(conduits)] {
          for (const auto& conduit : conduits) {
            conduit->CollectTelemetryData();
          }
        }));
  }
}

nsTArray<dom::RTCCodecStats> PeerConnectionImpl::GetCodecStats(
    DOMHighResTimeStamp aNow) {
  MOZ_ASSERT(NS_IsMainThread());
  nsTArray<dom::RTCCodecStats> result;

  struct CodecComparator {
    bool operator()(const JsepCodecDescription* aA,
                    const JsepCodecDescription* aB) const {
      return aA->StatsId() < aB->StatsId();
    }
  };

  // transportId -> codec; per direction (whether the codecType
  // shall be "encode", "decode" or absent (if a codec exists in both maps for a
  // transport)). These do the bookkeeping to ensure codec stats get coalesced
  // to transport level.
  std::map<std::string, std::set<JsepCodecDescription*, CodecComparator>>
      sendCodecMap;
  std::map<std::string, std::set<JsepCodecDescription*, CodecComparator>>
      recvCodecMap;

  // Find all JsepCodecDescription instances we want to turn into codec stats.
  for (const auto& transceiver : mTransceivers) {
    auto sendCodecs = transceiver->GetNegotiatedSendCodecs();
    auto recvCodecs = transceiver->GetNegotiatedRecvCodecs();

    const std::string transportId = transceiver->GetTransportId();
    // This ensures both codec maps have the same size.
    auto& sendMap = sendCodecMap[transportId];
    auto& recvMap = recvCodecMap[transportId];

    sendCodecs.apply([&](const auto& aCodecs) {
      for (const auto& codec : aCodecs) {
        sendMap.insert(codec.get());
      }
    });
    recvCodecs.apply([&](const auto& aCodecs) {
      for (const auto& codec : aCodecs) {
        recvMap.insert(codec.get());
      }
    });
  }

  auto createCodecStat = [&](const JsepCodecDescription* aCodec,
                             const nsString& aTransportId,
                             Maybe<RTCCodecType> aCodecType) {
    uint16_t pt;
    {
      DebugOnly<bool> rv = aCodec->GetPtAsInt(&pt);
      MOZ_ASSERT(rv);
    }
    nsString mimeType;
    mimeType.AppendPrintf(
        "%s/%s", aCodec->Type() == SdpMediaSection::kVideo ? "video" : "audio",
        aCodec->mName.c_str());
    nsString id = aTransportId;
    id.Append(u"_");
    id.Append(aCodec->StatsId());

    dom::RTCCodecStats codec;
    codec.mId.Construct(std::move(id));
    codec.mTimestamp.Construct(aNow);
    codec.mType.Construct(RTCStatsType::Codec);
    codec.mPayloadType = pt;
    if (aCodecType) {
      codec.mCodecType.Construct(*aCodecType);
    }
    codec.mTransportId = aTransportId;
    codec.mMimeType = std::move(mimeType);
    codec.mClockRate.Construct(aCodec->mClock);
    if (aCodec->Type() == SdpMediaSection::MediaType::kAudio) {
      codec.mChannels.Construct(aCodec->mChannels);
    }
    if (aCodec->mSdpFmtpLine) {
      codec.mSdpFmtpLine.Construct(
          NS_ConvertUTF8toUTF16(aCodec->mSdpFmtpLine->c_str()));
    }

    result.AppendElement(std::move(codec));
  };

  // Create codec stats for the gathered codec descriptions, sorted primarily
  // by transportId, secondarily by payload type (from StatsId()).
  for (const auto& [transportId, sendCodecs] : sendCodecMap) {
    const auto& recvCodecs = recvCodecMap[transportId];
    const nsString tid = NS_ConvertASCIItoUTF16(transportId);
    AutoTArray<JsepCodecDescription*, 16> bidirectionalCodecs;
    AutoTArray<JsepCodecDescription*, 16> unidirectionalCodecs;
    std::set_intersection(sendCodecs.cbegin(), sendCodecs.cend(),
                          recvCodecs.cbegin(), recvCodecs.cend(),
                          MakeBackInserter(bidirectionalCodecs),
                          CodecComparator());
    std::set_symmetric_difference(sendCodecs.cbegin(), sendCodecs.cend(),
                                  recvCodecs.cbegin(), recvCodecs.cend(),
                                  MakeBackInserter(unidirectionalCodecs),
                                  CodecComparator());
    for (const auto* codec : bidirectionalCodecs) {
      createCodecStat(codec, tid, Nothing());
    }
    for (const auto* codec : unidirectionalCodecs) {
      createCodecStat(
          codec, tid,
          Some(codec->mDirection == sdp::kSend ? RTCCodecType::Encode
                                               : RTCCodecType::Decode));
    }
  }

  return result;
}

RefPtr<dom::RTCStatsReportPromise> PeerConnectionImpl::GetStats(
    dom::MediaStreamTrack* aSelector, bool aInternalStats) {
  MOZ_ASSERT(NS_IsMainThread());
  nsTArray<RefPtr<dom::RTCStatsPromise>> promises;
  DOMHighResTimeStamp now = mTimestampMaker.GetNow();

  nsTArray<dom::RTCCodecStats> codecStats = GetCodecStats(now);

  nsTArray<
      std::tuple<RTCRtpTransceiver*, RefPtr<RTCStatsPromise::AllPromiseType>>>
      transceiverStatsPromises;
  for (const auto& transceiver : mTransceivers) {
    const bool sendSelected = transceiver->Sender()->HasTrack(aSelector);
    const bool recvSelected = transceiver->Receiver()->HasTrack(aSelector);
    if (!sendSelected && !recvSelected) {
      continue;
    }
    nsTArray<RefPtr<RTCStatsPromise>> rtpStreamPromises;
    // Get all rtp stream stats for the given selector. Then filter away any
    // codec stat not related to the selector, and assign codec ids to the
    // stream stats.
    if (sendSelected) {
      rtpStreamPromises.AppendElements(
          transceiver->Sender()->GetStatsInternal());
    }
    if (recvSelected) {
      // Right now, returns two promises; one for RTP/RTCP stats, and
      // another for ICE stats.
      rtpStreamPromises.AppendElements(
          transceiver->Receiver()->GetStatsInternal());
    }
    transceiverStatsPromises.AppendElement(
        std::make_tuple(transceiver.get(),
                        RTCStatsPromise::All(GetMainThreadSerialEventTarget(),
                                             rtpStreamPromises)));
  }

  promises.AppendElement(RTCRtpTransceiver::ApplyCodecStats(
      std::move(codecStats), std::move(transceiverStatsPromises)));

  // TODO(bug 1616937): We need to move this is RTCRtpSender, to make
  // getStats on those objects work properly. It might be worth optimizing
  // the null selector case, so we don't end up with bunches of copies of
  // the same transport information in the final report.
  if (aSelector) {
    std::string transportId = GetTransportIdMatchingSendTrack(*aSelector);
    if (!transportId.empty()) {
      promises.AppendElement(mTransportHandler->GetIceStats(transportId, now));
    }
  } else {
    promises.AppendElement(mTransportHandler->GetIceStats("", now));
  }

  promises.AppendElement(GetDataChannelStats(mDataConnection, now));

  // This is what we're going to return; all the stuff in |promises| will be
  // accumulated here.
  UniquePtr<dom::RTCStatsReportInternal> report(
      new dom::RTCStatsReportInternal);
  report->mPcid = NS_ConvertASCIItoUTF16(mName.c_str());
  if (mWindow && mWindow->GetBrowsingContext()) {
    report->mBrowserId = mWindow->GetBrowsingContext()->BrowserId();
  }
  report->mConfiguration.Construct(mJsConfiguration);
  // TODO(bug 1589416): We need to do better here.
  if (!mIceStartTime.IsNull()) {
    report->mCallDurationMs.Construct(
        (TimeStamp::Now() - mIceStartTime).ToMilliseconds());
  }
  report->mIceRestarts = mIceRestartCount;
  report->mIceRollbacks = mIceRollbackCount;
  report->mClosed = false;
  report->mTimestamp = now;

  if (aInternalStats && mJsepSession) {
    for (const auto& candidate : mRawTrickledCandidates) {
      if (!report->mRawRemoteCandidates.AppendElement(
              NS_ConvertASCIItoUTF16(candidate.c_str()), fallible)) {
        // XXX(Bug 1632090) Instead of extending the array 1-by-1 (which might
        // involve multiple reallocations) and potentially crashing here,
        // SetCapacity could be called outside the loop once.
        mozalloc_handle_oom(0);
      }
    }

    if (mJsepSession) {
      // TODO we probably should report Current and Pending SDPs here
      // separately. Plus the raw SDP we got from JS (mLocalRequestedSDP).
      // And if it's the offer or answer would also be nice.
      std::string localDescription =
          mJsepSession->GetLocalDescription(kJsepDescriptionPendingOrCurrent);
      std::string remoteDescription =
          mJsepSession->GetRemoteDescription(kJsepDescriptionPendingOrCurrent);
      report->mLocalSdp.Construct(
          NS_ConvertASCIItoUTF16(localDescription.c_str()));
      report->mRemoteSdp.Construct(
          NS_ConvertASCIItoUTF16(remoteDescription.c_str()));
      if (!report->mSdpHistory.AppendElements(mSdpHistory, fallible)) {
        mozalloc_handle_oom(0);
      }
      if (mJsepSession->IsPendingOfferer().isSome()) {
        report->mOfferer.Construct(*mJsepSession->IsPendingOfferer());
      } else if (mJsepSession->IsCurrentOfferer().isSome()) {
        report->mOfferer.Construct(*mJsepSession->IsCurrentOfferer());
      } else {
        // Silly.
        report->mOfferer.Construct(false);
      }
    }
  }

  return dom::RTCStatsPromise::All(GetMainThreadSerialEventTarget(), promises)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [report = std::move(report), idGen = mIdGenerator](
              nsTArray<UniquePtr<dom::RTCStatsCollection>> aStats) mutable {
            idGen->RewriteIds(std::move(aStats), report.get());
            return dom::RTCStatsReportPromise::CreateAndResolve(
                std::move(report), __func__);
          },
          [](nsresult rv) {
            return dom::RTCStatsReportPromise::CreateAndReject(rv, __func__);
          });
}

void PeerConnectionImpl::RecordIceRestartStatistics(JsepSdpType type) {
  switch (type) {
    case mozilla::kJsepSdpOffer:
    case mozilla::kJsepSdpPranswer:
      break;
    case mozilla::kJsepSdpAnswer:
      ++mIceRestartCount;
      break;
    case mozilla::kJsepSdpRollback:
      ++mIceRollbackCount;
      break;
  }
}

void PeerConnectionImpl::StoreConfigurationForAboutWebrtc(
    const dom::RTCConfiguration& aConfig) {
  // This will only be called once, when the PeerConnection is initially
  // configured, at least until setConfiguration is implemented
  // see https://bugzilla.mozilla.org/show_bug.cgi?id=1253706
  // @TODO bug 1739451 call this from setConfiguration
  mJsConfiguration.mIceServers.Clear();
  for (const auto& server : aConfig.mIceServers) {
    RTCIceServerInternal internal;
    internal.mCredentialProvided = server.mCredential.WasPassed();
    internal.mUserNameProvided = server.mUsername.WasPassed();
    if (server.mUrl.WasPassed()) {
      if (!internal.mUrls.AppendElement(server.mUrl.Value(), fallible)) {
        mozalloc_handle_oom(0);
      }
    }
    if (server.mUrls.WasPassed()) {
      for (const auto& url : server.mUrls.Value().GetAsStringSequence()) {
        if (!internal.mUrls.AppendElement(url, fallible)) {
          mozalloc_handle_oom(0);
        }
      }
    }
    if (!mJsConfiguration.mIceServers.AppendElement(internal, fallible)) {
      mozalloc_handle_oom(0);
    }
  }
  mJsConfiguration.mSdpSemantics.Reset();
  if (aConfig.mSdpSemantics.WasPassed()) {
    mJsConfiguration.mSdpSemantics.Construct(aConfig.mSdpSemantics.Value());
  }

  mJsConfiguration.mIceTransportPolicy.Reset();
  mJsConfiguration.mIceTransportPolicy.Construct(aConfig.mIceTransportPolicy);
  mJsConfiguration.mBundlePolicy.Reset();
  mJsConfiguration.mBundlePolicy.Construct(aConfig.mBundlePolicy);
  mJsConfiguration.mPeerIdentityProvided = !aConfig.mPeerIdentity.IsEmpty();
  mJsConfiguration.mCertificatesProvided = !aConfig.mCertificates.Length();
}

dom::Sequence<dom::RTCSdpParsingErrorInternal>
PeerConnectionImpl::GetLastSdpParsingErrors() const {
  const auto& sdpErrors = mJsepSession->GetLastSdpParsingErrors();
  dom::Sequence<dom::RTCSdpParsingErrorInternal> domErrors;
  if (!domErrors.SetCapacity(domErrors.Length(), fallible)) {
    mozalloc_handle_oom(0);
  }
  for (const auto& error : sdpErrors) {
    mozilla::dom::RTCSdpParsingErrorInternal internal;
    internal.mLineNumber = error.first;
    if (!AppendASCIItoUTF16(MakeStringSpan(error.second.c_str()),
                            internal.mError, fallible)) {
      mozalloc_handle_oom(0);
    }
    if (!domErrors.AppendElement(std::move(internal), fallible)) {
      mozalloc_handle_oom(0);
    }
  }
  return domErrors;
}

// Telemetry for when calls start
void PeerConnectionImpl::StartCallTelem() {
  if (mCallTelemStarted) {
    return;
  }
  MOZ_RELEASE_ASSERT(mWindow);
  uint64_t windowId = mWindow->WindowID();
  auto found = sCallDurationTimers.find(windowId);
  if (found == sCallDurationTimers.end()) {
    found =
        sCallDurationTimers.emplace(windowId, PeerConnectionAutoTimer()).first;
  }
  found->second.RegisterConnection();
  mCallTelemStarted = true;

  // Increment session call counter
  // If we want to track Loop calls independently here, we need two
  // histograms.
  //
  // NOTE: As of bug 1654248 landing we are no longer counting renegotiations
  // as separate calls. Expect numbers to drop compared to
  // WEBRTC_CALL_COUNT_2.
  Telemetry::Accumulate(Telemetry::WEBRTC_CALL_COUNT_3, 1);
}

void PeerConnectionImpl::StunAddrsHandler::OnMDNSQueryComplete(
    const nsCString& hostname, const Maybe<nsCString>& address) {
  MOZ_ASSERT(NS_IsMainThread());
  PeerConnectionWrapper pcw(mPcHandle);
  if (!pcw.impl()) {
    return;
  }
  auto itor = pcw.impl()->mQueriedMDNSHostnames.find(hostname.BeginReading());
  if (itor != pcw.impl()->mQueriedMDNSHostnames.end()) {
    if (address) {
      for (auto& cand : itor->second) {
        // Replace obfuscated address with actual address
        std::string obfuscatedAddr = cand.mTokenizedCandidate[4];
        cand.mTokenizedCandidate[4] = address->BeginReading();
        std::ostringstream o;
        for (size_t i = 0; i < cand.mTokenizedCandidate.size(); ++i) {
          o << cand.mTokenizedCandidate[i];
          if (i + 1 != cand.mTokenizedCandidate.size()) {
            o << " ";
          }
        }
        std::string mungedCandidate = o.str();
        pcw.impl()->StampTimecard("Done looking up mDNS name");
        pcw.impl()->mTransportHandler->AddIceCandidate(
            cand.mTransportId, mungedCandidate, cand.mUfrag, obfuscatedAddr);
      }
    } else {
      pcw.impl()->StampTimecard("Failed looking up mDNS name");
    }
    pcw.impl()->mQueriedMDNSHostnames.erase(itor);
  }
}

void PeerConnectionImpl::StunAddrsHandler::OnStunAddrsAvailable(
    const mozilla::net::NrIceStunAddrArray& addrs) {
  CSFLogInfo(LOGTAG, "%s: receiving (%d) stun addrs", __FUNCTION__,
             (int)addrs.Length());
  PeerConnectionWrapper pcw(mPcHandle);
  if (!pcw.impl()) {
    return;
  }
  pcw.impl()->mStunAddrs = addrs.Clone();
  pcw.impl()->mLocalAddrsRequestState = STUN_ADDR_REQUEST_COMPLETE;
  pcw.impl()->FlushIceCtxOperationQueueIfReady();
  // If parent process returns 0 STUN addresses, change ICE connection
  // state to failed.
  if (!pcw.impl()->mStunAddrs.Length()) {
    pcw.impl()->IceConnectionStateChange(dom::RTCIceConnectionState::Failed);
  }
}

void PeerConnectionImpl::InitLocalAddrs() {
  if (mLocalAddrsRequestState == STUN_ADDR_REQUEST_PENDING) {
    return;
  }
  if (mStunAddrsRequest) {
    mLocalAddrsRequestState = STUN_ADDR_REQUEST_PENDING;
    mStunAddrsRequest->SendGetStunAddrs();
  } else {
    mLocalAddrsRequestState = STUN_ADDR_REQUEST_COMPLETE;
  }
}

bool PeerConnectionImpl::ShouldForceProxy() const {
  if (Preferences::GetBool("media.peerconnection.ice.proxy_only", false)) {
    return true;
  }

  if (!Preferences::GetBool(
          "media.peerconnection.ice.proxy_only_if_behind_proxy", false)) {
    return false;
  }

  // Ok, we're supposed to be proxy_only, but only if a proxy is configured.
  // Let's just see if the document was loaded via a proxy.

  nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal = GetChannel();
  if (!httpChannelInternal) {
    return false;
  }

  nsCOMPtr<nsIProxiedChannel> proxiedChannel =
      do_QueryInterface(httpChannelInternal);
  if (!proxiedChannel) {
    return false;
  }

  nsCOMPtr<nsIProxyInfo> proxyInfo;
  proxiedChannel->GetProxyInfo(getter_AddRefs(proxyInfo));
  if (!proxyInfo) {
    return false;
  }

  nsCString proxyType;
  proxyInfo->GetType(proxyType);

  return !proxyType.IsEmpty() && !proxyType.EqualsLiteral("direct");
}

void PeerConnectionImpl::EnsureTransports(const JsepSession& aSession) {
  for (const auto& [id, transceiver] : aSession.GetTransceivers()) {
    (void)id;  // Lame, but no better way to do this right now.
    if (transceiver->HasOwnTransport()) {
      mTransportHandler->EnsureProvisionalTransport(
          transceiver->mTransport.mTransportId,
          transceiver->mTransport.mLocalUfrag,
          transceiver->mTransport.mLocalPwd,
          transceiver->mTransport.mComponents);
    }
  }

  GatherIfReady();
}

void PeerConnectionImpl::UpdateRTCDtlsTransports(bool aMarkAsStable) {
  for (auto& transceiver : mTransceivers) {
    std::string transportId = transceiver->GetTransportId();
    if (transportId.empty()) {
      continue;
    }
    if (!mTransportIdToRTCDtlsTransport.count(transportId)) {
      mTransportIdToRTCDtlsTransport.emplace(
          transportId, new RTCDtlsTransport(transceiver->GetParentObject()));
    }

    transceiver->SetDtlsTransport(mTransportIdToRTCDtlsTransport[transportId],
                                  aMarkAsStable);
  }
}

void PeerConnectionImpl::RollbackRTCDtlsTransports() {
  for (auto& transceiver : mTransceivers) {
    transceiver->RollbackToStableDtlsTransport();
  }
}

void PeerConnectionImpl::RemoveRTCDtlsTransportsExcept(
    const std::set<std::string>& aTransportIds) {
  for (auto iter = mTransportIdToRTCDtlsTransport.begin();
       iter != mTransportIdToRTCDtlsTransport.end();) {
    if (!aTransportIds.count(iter->first)) {
      iter = mTransportIdToRTCDtlsTransport.erase(iter);
    } else {
      ++iter;
    }
  }
}

nsresult PeerConnectionImpl::UpdateTransports(const JsepSession& aSession,
                                              const bool forceIceTcp) {
  std::set<std::string> finalTransports;
  for (const auto& [id, transceiver] : aSession.GetTransceivers()) {
    (void)id;  // Lame, but no better way to do this right now.
    if (transceiver->HasOwnTransport()) {
      finalTransports.insert(transceiver->mTransport.mTransportId);
      UpdateTransport(*transceiver, forceIceTcp);
    }
  }

  // clean up the unused RTCDtlsTransports
  RemoveRTCDtlsTransportsExcept(finalTransports);

  mTransportHandler->RemoveTransportsExcept(finalTransports);

  for (const auto& transceiverImpl : mTransceivers) {
    transceiverImpl->UpdateTransport();
  }

  return NS_OK;
}

void PeerConnectionImpl::UpdateTransport(const JsepTransceiver& aTransceiver,
                                         bool aForceIceTcp) {
  std::string ufrag;
  std::string pwd;
  std::vector<std::string> candidates;
  size_t components = 0;

  const JsepTransport& transport = aTransceiver.mTransport;
  unsigned level = aTransceiver.GetLevel();

  CSFLogDebug(LOGTAG, "ACTIVATING TRANSPORT! - PC %s: level=%u components=%u",
              mHandle.c_str(), (unsigned)level,
              (unsigned)transport.mComponents);

  ufrag = transport.mIce->GetUfrag();
  pwd = transport.mIce->GetPassword();
  candidates = transport.mIce->GetCandidates();
  components = transport.mComponents;
  if (aForceIceTcp) {
    candidates.erase(
        std::remove_if(candidates.begin(), candidates.end(),
                       [](const std::string& s) {
                         return s.find(" UDP ") != std::string::npos ||
                                s.find(" udp ") != std::string::npos;
                       }),
        candidates.end());
  }

  nsTArray<uint8_t> keyDer;
  nsTArray<uint8_t> certDer;
  nsresult rv = Identity()->Serialize(&keyDer, &certDer);
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "%s: Failed to serialize DTLS identity: %d",
                __FUNCTION__, (int)rv);
    return;
  }

  DtlsDigestList digests;
  for (const auto& fingerprint :
       transport.mDtls->GetFingerprints().mFingerprints) {
    std::ostringstream ss;
    ss << fingerprint.hashFunc;
    digests.emplace_back(ss.str(), fingerprint.fingerprint);
  }

  mTransportHandler->ActivateTransport(
      transport.mTransportId, transport.mLocalUfrag, transport.mLocalPwd,
      components, ufrag, pwd, keyDer, certDer, Identity()->auth_type(),
      transport.mDtls->GetRole() == JsepDtlsTransport::kJsepDtlsClient, digests,
      PrivacyRequested());

  for (auto& candidate : candidates) {
    AddIceCandidate("candidate:" + candidate, transport.mTransportId, ufrag);
  }
}

nsresult PeerConnectionImpl::UpdateMediaPipelines() {
  for (RefPtr<RTCRtpTransceiver>& transceiver : mTransceivers) {
    transceiver->ResetSync();
  }

  for (RefPtr<RTCRtpTransceiver>& transceiver : mTransceivers) {
    if (!transceiver->IsVideo()) {
      nsresult rv = transceiver->SyncWithMatchingVideoConduits(mTransceivers);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }

    nsresult rv = transceiver->UpdateConduit();
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return NS_OK;
}

void PeerConnectionImpl::StartIceChecks(const JsepSession& aSession) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mCanRegisterMDNSHostnamesDirectly) {
    for (auto& pair : mMDNSHostnamesToRegister) {
      mRegisteredMDNSHostnames.insert(pair.first);
      mStunAddrsRequest->SendRegisterMDNSHostname(
          nsCString(pair.first.c_str()), nsCString(pair.second.c_str()));
    }
    mMDNSHostnamesToRegister.clear();
    mCanRegisterMDNSHostnamesDirectly = true;
  }

  std::vector<std::string> attributes;
  if (aSession.RemoteIsIceLite()) {
    attributes.push_back("ice-lite");
  }

  if (!aSession.GetIceOptions().empty()) {
    attributes.push_back("ice-options:");
    for (const auto& option : aSession.GetIceOptions()) {
      attributes.back() += option + ' ';
    }
  }

  nsCOMPtr<nsIRunnable> runnable(
      WrapRunnable(mTransportHandler, &MediaTransportHandler::StartIceChecks,
                   aSession.IsIceControlling(), attributes));

  PerformOrEnqueueIceCtxOperation(runnable);
}

bool PeerConnectionImpl::GetPrefDefaultAddressOnly() const {
  MOZ_ASSERT(NS_IsMainThread());

  uint64_t winId = mWindow->WindowID();

  bool default_address_only = Preferences::GetBool(
      "media.peerconnection.ice.default_address_only", false);
  default_address_only |=
      !MediaManager::Get()->IsActivelyCapturingOrHasAPermission(winId);
  return default_address_only;
}

bool PeerConnectionImpl::GetPrefObfuscateHostAddresses() const {
  MOZ_ASSERT(NS_IsMainThread());

  uint64_t winId = mWindow->WindowID();

  bool obfuscate_host_addresses = Preferences::GetBool(
      "media.peerconnection.ice.obfuscate_host_addresses", false);
  obfuscate_host_addresses &=
      !MediaManager::Get()->IsActivelyCapturingOrHasAPermission(winId);
  obfuscate_host_addresses &= !PeerConnectionImpl::HostnameInPref(
      "media.peerconnection.ice.obfuscate_host_addresses.blocklist",
      mWindow->GetDocumentURI());
  obfuscate_host_addresses &= XRE_IsContentProcess();

  return obfuscate_host_addresses;
}

PeerConnectionImpl::SignalHandler::SignalHandler(PeerConnectionImpl* aPc,
                                                 MediaTransportHandler* aSource)
    : mHandle(aPc->GetHandle()),
      mSource(aSource),
      mSTSThread(aPc->GetSTSThread()) {
  ConnectSignals();
}

PeerConnectionImpl::SignalHandler::~SignalHandler() {
  ASSERT_ON_THREAD(mSTSThread);
}

void PeerConnectionImpl::SignalHandler::ConnectSignals() {
  mSource->SignalGatheringStateChange.connect(
      this, &PeerConnectionImpl::SignalHandler::IceGatheringStateChange_s);
  mSource->SignalConnectionStateChange.connect(
      this, &PeerConnectionImpl::SignalHandler::IceConnectionStateChange_s);
  mSource->SignalCandidate.connect(
      this, &PeerConnectionImpl::SignalHandler::OnCandidateFound_s);
  mSource->SignalAlpnNegotiated.connect(
      this, &PeerConnectionImpl::SignalHandler::AlpnNegotiated_s);
}

void PeerConnectionImpl::AddIceCandidate(const std::string& aCandidate,
                                         const std::string& aTransportId,
                                         const std::string& aUfrag) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aTransportId.empty());

  bool obfuscate_host_addresses = Preferences::GetBool(
      "media.peerconnection.ice.obfuscate_host_addresses", false);

  if (obfuscate_host_addresses && !RelayOnly()) {
    std::vector<std::string> tokens;
    TokenizeCandidate(aCandidate, tokens);

    if (tokens.size() > 4) {
      std::string addr = tokens[4];

      // Check for address ending with .local
      size_t nPeriods = std::count(addr.begin(), addr.end(), '.');
      size_t dotLocalLength = 6;  // length of ".local"

      if (nPeriods == 1 &&
          addr.rfind(".local") + dotLocalLength == addr.length()) {
        if (mStunAddrsRequest) {
          PendingIceCandidate cand;
          cand.mTokenizedCandidate = std::move(tokens);
          cand.mTransportId = aTransportId;
          cand.mUfrag = aUfrag;
          mQueriedMDNSHostnames[addr].push_back(cand);

          GetMainThreadEventTarget()->Dispatch(NS_NewRunnableFunction(
              "PeerConnectionImpl::SendQueryMDNSHostname",
              [self = RefPtr<PeerConnectionImpl>(this), addr]() mutable {
                if (self->mStunAddrsRequest) {
                  self->StampTimecard("Look up mDNS name");
                  self->mStunAddrsRequest->SendQueryMDNSHostname(
                      nsCString(nsAutoCString(addr.c_str())));
                }
                NS_ReleaseOnMainThread(
                    "PeerConnectionImpl::SendQueryMDNSHostname", self.forget());
              }));
        }
        // TODO: Bug 1535690, we don't want to tell the ICE context that remote
        // trickle is done if we are waiting to resolve a mDNS candidate.
        return;
      }
    }
  }

  mTransportHandler->AddIceCandidate(aTransportId, aCandidate, aUfrag, "");
}

void PeerConnectionImpl::UpdateNetworkState(bool online) {
  if (mTransportHandler) {
    mTransportHandler->UpdateNetworkState(online);
  }
}

void PeerConnectionImpl::FlushIceCtxOperationQueueIfReady() {
  MOZ_ASSERT(NS_IsMainThread());

  if (IsIceCtxReady()) {
    for (auto& queuedIceCtxOperation : mQueuedIceCtxOperations) {
      queuedIceCtxOperation->Run();
    }
    mQueuedIceCtxOperations.clear();
  }
}

void PeerConnectionImpl::PerformOrEnqueueIceCtxOperation(
    nsIRunnable* runnable) {
  MOZ_ASSERT(NS_IsMainThread());

  if (IsIceCtxReady()) {
    runnable->Run();
  } else {
    mQueuedIceCtxOperations.push_back(runnable);
  }
}

void PeerConnectionImpl::GatherIfReady() {
  MOZ_ASSERT(NS_IsMainThread());

  // Init local addrs here so that if we re-gather after an ICE restart
  // resulting from changing WiFi networks, we get new local addrs.
  // Otherwise, we would reuse the addrs from the original WiFi network
  // and the ICE restart will fail.
  if (!mStunAddrs.Length()) {
    InitLocalAddrs();
  }

  // If we had previously queued gathering or ICE start, unqueue them
  mQueuedIceCtxOperations.clear();
  nsCOMPtr<nsIRunnable> runnable(WrapRunnable(
      RefPtr<PeerConnectionImpl>(this), &PeerConnectionImpl::EnsureIceGathering,
      GetPrefDefaultAddressOnly(), GetPrefObfuscateHostAddresses()));

  PerformOrEnqueueIceCtxOperation(runnable);
}

already_AddRefed<nsIHttpChannelInternal> PeerConnectionImpl::GetChannel()
    const {
  Document* doc = mWindow->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    NS_WARNING("Unable to get document from window");
    return nullptr;
  }

  if (!doc->GetDocumentURI()->SchemeIs("file")) {
    nsIChannel* channel = doc->GetChannel();
    if (!channel) {
      NS_WARNING("Unable to get channel from document");
      return nullptr;
    }

    nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal =
        do_QueryInterface(channel);
    if (NS_WARN_IF(!httpChannelInternal)) {
      CSFLogInfo(LOGTAG, "%s: Document does not have an HTTP channel",
                 __FUNCTION__);
      return nullptr;
    }
    return httpChannelInternal.forget();
  }
  return nullptr;
}

nsresult PeerConnectionImpl::SetTargetForDefaultLocalAddressLookup() {
  nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal = GetChannel();
  if (!httpChannelInternal) {
    return NS_OK;
  }

  nsCString remoteIp;
  nsresult rv = httpChannelInternal->GetRemoteAddress(remoteIp);
  if (NS_FAILED(rv) || remoteIp.IsEmpty()) {
    CSFLogError(LOGTAG, "%s: Failed to get remote IP address: %d", __FUNCTION__,
                (int)rv);
    return rv;
  }

  int32_t remotePort;
  rv = httpChannelInternal->GetRemotePort(&remotePort);
  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "%s: Failed to get remote port number: %d",
                __FUNCTION__, (int)rv);
    return rv;
  }

  mTransportHandler->SetTargetForDefaultLocalAddressLookup(remoteIp.get(),
                                                           remotePort);

  return NS_OK;
}

void PeerConnectionImpl::EnsureIceGathering(bool aDefaultRouteOnly,
                                            bool aObfuscateHostAddresses) {
  auto proxyConfig = GetProxyConfig();
  if (proxyConfig) {
    // Note that this could check if PrivacyRequested() is set on the PC and
    // remove "webrtc" from the ALPN list.  But that would only work if the PC
    // was constructed with a peerIdentity constraint, not when isolated
    // streams are added.  If we ever need to signal to the proxy that the
    // media is isolated, then we would need to restructure this code.
    mTransportHandler->SetProxyConfig(std::move(*proxyConfig));
  }

  if (!mTargetForDefaultLocalAddressLookupIsSet) {
    nsresult rv = SetTargetForDefaultLocalAddressLookup();
    if (NS_FAILED(rv)) {
      NS_WARNING("Unable to set target for default local address lookup");
    }
    mTargetForDefaultLocalAddressLookupIsSet = true;
  }

  // Make sure we don't call StartIceGathering if we're in e10s mode
  // and we received no STUN addresses from the parent process.  In the
  // absence of previously provided STUN addresses, StartIceGathering will
  // attempt to gather them (as in non-e10s mode), and this will cause a
  // sandboxing exception in e10s mode.
  if (!mStunAddrs.Length() && XRE_IsContentProcess()) {
    CSFLogInfo(LOGTAG, "%s: No STUN addresses returned from parent process",
               __FUNCTION__);
    return;
  }

  mTransportHandler->StartIceGathering(aDefaultRouteOnly,
                                       aObfuscateHostAddresses, mStunAddrs);
}

already_AddRefed<dom::RTCRtpTransceiver> PeerConnectionImpl::CreateTransceiver(
    JsepTransceiver* aJsepTransceiver, const RTCRtpTransceiverInit& aInit,
    dom::MediaStreamTrack* aSendTrack, ErrorResult& aRv) {
  PeerConnectionCtx* ctx = PeerConnectionCtx::GetInstance();
  if (!mCall) {
    mCall = WebrtcCallWrapper::Create(
        GetTimestampMaker(),
        media::ShutdownBlockingTicket::Create(
            u"WebrtcCallWrapper shutdown blocker"_ns,
            NS_LITERAL_STRING_FROM_CSTRING(__FILE__), __LINE__),
        ctx->GetSharedWebrtcState());
  }

  RefPtr<RTCRtpTransceiver> transceiver = new RTCRtpTransceiver(
      mWindow, PrivacyNeeded(), this, mTransportHandler, aJsepTransceiver,
      mSTSThread.get(), aSendTrack, mCall.get(), mIdGenerator);

  transceiver->Init(aInit, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (aSendTrack) {
    // implement checking for peerIdentity (where failure == black/silence)
    Document* doc = mWindow->GetExtantDoc();
    if (doc) {
      transceiver->Sender()->GetPipeline()->UpdateSinkIdentity(
          doc->NodePrincipal(), GetPeerIdentity());
    } else {
      MOZ_CRASH();
      aRv = NS_ERROR_FAILURE;
      return nullptr;  // Don't remove this till we know it's safe.
    }
  }

  return transceiver.forget();
}

std::string PeerConnectionImpl::GetTransportIdMatchingSendTrack(
    const dom::MediaStreamTrack& aTrack) const {
  for (const RefPtr<RTCRtpTransceiver>& transceiver : mTransceivers) {
    if (transceiver->Sender()->HasTrack(&aTrack)) {
      return transceiver->GetTransportId();
    }
  }
  return std::string();
}

void PeerConnectionImpl::SignalHandler::IceGatheringStateChange_s(
    dom::RTCIceGatheringState aState) {
  ASSERT_ON_THREAD(mSTSThread);

  GetMainThreadEventTarget()->Dispatch(
      NS_NewRunnableFunction(__func__,
                             [handle = mHandle, aState] {
                               PeerConnectionWrapper wrapper(handle);
                               if (wrapper.impl()) {
                                 wrapper.impl()->IceGatheringStateChange(
                                     aState);
                               }
                             }),
      NS_DISPATCH_NORMAL);
}

void PeerConnectionImpl::SignalHandler::IceConnectionStateChange_s(
    dom::RTCIceConnectionState aState) {
  ASSERT_ON_THREAD(mSTSThread);

  GetMainThreadEventTarget()->Dispatch(
      NS_NewRunnableFunction(__func__,
                             [handle = mHandle, aState] {
                               PeerConnectionWrapper wrapper(handle);
                               if (wrapper.impl()) {
                                 wrapper.impl()->IceConnectionStateChange(
                                     aState);
                               }
                             }),
      NS_DISPATCH_NORMAL);
}

void PeerConnectionImpl::SignalHandler::OnCandidateFound_s(
    const std::string& aTransportId, const CandidateInfo& aCandidateInfo) {
  ASSERT_ON_THREAD(mSTSThread);
  CSFLogDebug(LOGTAG, "%s: %s", __FUNCTION__, aTransportId.c_str());

  MOZ_ASSERT(!aCandidateInfo.mUfrag.empty());

  GetMainThreadEventTarget()->Dispatch(
      NS_NewRunnableFunction(__func__,
                             [handle = mHandle, aTransportId, aCandidateInfo] {
                               PeerConnectionWrapper wrapper(handle);
                               if (wrapper.impl()) {
                                 wrapper.impl()->OnCandidateFound(
                                     aTransportId, aCandidateInfo);
                               }
                             }),
      NS_DISPATCH_NORMAL);
}

void PeerConnectionImpl::SignalHandler::AlpnNegotiated_s(
    const std::string& aAlpn, bool aPrivacyRequested) {
  MOZ_DIAGNOSTIC_ASSERT((aAlpn == "c-webrtc") == aPrivacyRequested);
  GetMainThreadEventTarget()->Dispatch(
      NS_NewRunnableFunction(__func__,
                             [handle = mHandle, aPrivacyRequested] {
                               PeerConnectionWrapper wrapper(handle);
                               if (wrapper.impl()) {
                                 wrapper.impl()->OnAlpnNegotiated(
                                     aPrivacyRequested);
                               }
                             }),
      NS_DISPATCH_NORMAL);
}

/**
 * Tells you if any local track is isolated to a specific peer identity.
 * Obviously, we want all the tracks to be isolated equally so that they can
 * all be sent or not.  We check once when we are setting a local description
 * and that determines if we flip the "privacy requested" bit on.  Once the bit
 * is on, all media originating from this peer connection is isolated.
 *
 * @returns true if any track has a peerIdentity set on it
 */
bool PeerConnectionImpl::AnyLocalTrackHasPeerIdentity() const {
  MOZ_ASSERT(NS_IsMainThread());

  for (const RefPtr<RTCRtpTransceiver>& transceiver : mTransceivers) {
    if (transceiver->Sender()->GetTrack() &&
        transceiver->Sender()->GetTrack()->GetPeerIdentity()) {
      return true;
    }
  }
  return false;
}

bool PeerConnectionImpl::AnyCodecHasPluginID(uint64_t aPluginID) {
  for (RefPtr<RTCRtpTransceiver>& transceiver : mTransceivers) {
    if (transceiver->ConduitHasPluginID(aPluginID)) {
      return true;
    }
  }
  return false;
}

std::unique_ptr<NrSocketProxyConfig> PeerConnectionImpl::GetProxyConfig()
    const {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mForceProxy &&
      Preferences::GetBool("media.peerconnection.disable_http_proxy", false)) {
    return nullptr;
  }

  nsCString alpn = "webrtc,c-webrtc"_ns;
  auto* browserChild = BrowserChild::GetFrom(mWindow);
  if (!browserChild) {
    // Android doesn't have browser child apparently...
    return nullptr;
  }

  Document* doc = mWindow->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    NS_WARNING("Unable to get document from window");
    return nullptr;
  }

  TabId id = browserChild->GetTabId();
  nsCOMPtr<nsILoadInfo> loadInfo =
      new net::LoadInfo(doc->NodePrincipal(), doc->NodePrincipal(), doc, 0,
                        nsIContentPolicy::TYPE_INVALID);

  Maybe<net::LoadInfoArgs> loadInfoArgs;
  MOZ_ALWAYS_SUCCEEDS(
      mozilla::ipc::LoadInfoToLoadInfoArgs(loadInfo, &loadInfoArgs));
  return std::unique_ptr<NrSocketProxyConfig>(new NrSocketProxyConfig(
      net::WebrtcProxyConfig(id, alpn, *loadInfoArgs, mForceProxy)));
}

std::map<uint64_t, PeerConnectionAutoTimer>
    PeerConnectionImpl::sCallDurationTimers;
}  // namespace mozilla

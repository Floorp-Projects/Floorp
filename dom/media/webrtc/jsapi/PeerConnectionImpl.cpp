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
#include "PeerConnectionMedia.h"
#include "RemoteTrackSource.h"
#include "nsDOMDataChannelDeclarations.h"
#include "transport/dtlsidentity.h"
#include "sdp/SdpAttribute.h"

#include "jsep/JsepTrack.h"
#include "jsep/JsepSession.h"
#include "jsep/JsepSessionImpl.h"

#include "transportbridge/MediaPipeline.h"
#include "jsapi/RTCRtpReceiver.h"

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

NS_IMPL_ISUPPORTS0(PeerConnectionImpl)

already_AddRefed<PeerConnectionImpl> PeerConnectionImpl::Constructor(
    const dom::GlobalObject& aGlobal) {
  RefPtr<PeerConnectionImpl> pc = new PeerConnectionImpl(&aGlobal);

  CSFLogDebug(LOGTAG, "Created PeerConnection: %p", pc.get());

  return pc.forget();
}

bool PeerConnectionImpl::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto,
                                    JS::MutableHandle<JSObject*> aReflector) {
  return PeerConnectionImpl_Binding::Wrap(aCx, this, aGivenProto, aReflector);
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
      mMedia(nullptr),
      mTransportHandler(nullptr),
      mUuidGen(MakeUnique<PCUuidGenerator>()),
      mIceRestartCount(0),
      mIceRollbackCount(0),
      mHaveConfiguredCodecs(false),
      mTrickle(true)  // TODO(ekr@rtfm.com): Use pref
      ,
      mPrivateWindow(false),
      mActiveOnWindow(false),
      mPacketDumpEnabled(false),
      mPacketDumpFlagsMutex("Packet dump flags mutex"),
      mTimestampMaker(mWindow),
      mIdGenerator(new RTCStatsIdGenerator()),
      listenPort(0),
      connectPort(0),
      connectStr(nullptr) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT_IF(aGlobal, mWindow);
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
}

PeerConnectionImpl::~PeerConnectionImpl() {
  if (mTimeCard) {
    STAMP_TIMECARD(mTimeCard, "Destructor Invoked");
    print_timecard(mTimeCard);
    destroy_timecard(mTimeCard);
    mTimeCard = nullptr;
  }

  MOZ_ASSERT(NS_IsMainThread());

  if (mWindow && mActiveOnWindow) {
    mWindow->RemovePeerConnection();
    // No code is supposed to observe the assignment below, but
    // hopefully it makes looking at this object in a debugger
    // make more sense.
    mActiveOnWindow = false;
  }

  if (mPrivateWindow && mTransportHandler) {
    mTransportHandler->ExitPrivateMode();
  }
  if (PeerConnectionCtx::isActive()) {
    PeerConnectionCtx::GetInstance()->RemovePeerConnection(mHandle);
  } else {
    CSFLogError(LOGTAG, "PeerConnectionCtx is already gone. Ignoring...");
  }

  CSFLogInfo(LOGTAG, "%s: PeerConnectionImpl destructor invoked for %s",
             __FUNCTION__, mHandle.c_str());

  // Since this and Initialize() occur on MainThread, they can't both be
  // running at once

  // Right now, we delete PeerConnectionCtx at XPCOM shutdown only, but we
  // probably want to shut it down more aggressively to save memory.  We
  // could shut down here when there are no uses.  It might be more optimal
  // to release off a timer (and XPCOM Shutdown) to avoid churn
  ShutdownMedia();
}

nsresult PeerConnectionImpl::Initialize(PeerConnectionObserver& aObserver,
                                        nsGlobalWindowInner* aWindow,
                                        const RTCConfiguration& aConfiguration,
                                        nsISupports* aThread) {
  nsresult res;

  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aThread);
  if (!mThread) {
    mThread = do_QueryInterface(aThread);
    MOZ_ASSERT(mThread);
  }
  CheckThread();

  // Store the configuration for about:webrtc
  StoreConfigurationForAboutWebrtc(aConfiguration);

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
  res = PeerConnectionCtx::InitializeGlobal(mThread);
  NS_ENSURE_SUCCESS(res, res);

  nsTArray<dom::RTCIceServer> iceServers;
  if (aConfiguration.mIceServers.WasPassed()) {
    iceServers = aConfiguration.mIceServers.Value();
  }

  res = mTransportHandler->CreateIceCtx("PC:" + GetName(), iceServers,
                                        aConfiguration.mIceTransportPolicy);
  if (NS_FAILED(res)) {
    CSFLogError(LOGTAG, "%s: Failed to init mtransport", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  mJsepSession =
      MakeUnique<JsepSessionImpl>(mName, MakeUnique<PCUuidGenerator>());
  mJsepSession->SetRtxIsAllowed(mRtxIsAllowed);

  res = mJsepSession->Init();
  if (NS_FAILED(res)) {
    CSFLogError(LOGTAG, "%s: Couldn't init JSEP Session, res=%u", __FUNCTION__,
                static_cast<unsigned>(res));
    return res;
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

  res = mJsepSession->SetBundlePolicy(bundlePolicy);
  if (NS_FAILED(res)) {
    CSFLogError(LOGTAG, "%s: Couldn't set bundle policy, res=%u, error=%s",
                __FUNCTION__, static_cast<unsigned>(res),
                mJsepSession->GetLastError().c_str());
    return res;
  }

  mMedia = new PeerConnectionMedia(this);

  // Initialize the media object.
  res = mMedia->Init();
  if (NS_FAILED(res)) {
    CSFLogError(LOGTAG, "%s: Couldn't initialize media object", __FUNCTION__);
    ShutdownMedia();
    return res;
  }

  PeerConnectionCtx::GetInstance()->AddPeerConnection(mHandle, this);

  return NS_OK;
}

void PeerConnectionImpl::Initialize(PeerConnectionObserver& aObserver,
                                    nsGlobalWindowInner& aWindow,
                                    const RTCConfiguration& aConfiguration,
                                    nsISupports* aThread, ErrorResult& rv) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aThread);
  mThread = do_QueryInterface(aThread);

  nsresult res = Initialize(aObserver, &aWindow, aConfiguration, aThread);
  if (NS_FAILED(res)) {
    rv.Throw(res);
    return;
  }

  if (!aConfiguration.mPeerIdentity.IsEmpty()) {
    mPeerIdentity = new PeerIdentity(aConfiguration.mPeerIdentity);
    mPrivacyRequested = Some(true);
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
      branch->GetBoolPref("media.webrtc.hw.h264.enabled",
                          &mHardwareH264Enabled);
    }

    mH264Enabled = mHardwareH264Enabled || mSoftwareH264Enabled;

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
    switch (codec->mType) {
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
          videoCodec.mConstraints.maxFps = mVP8MaxFr;
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
    if (codec->mType == SdpMediaSection::kVideo && codec->mEnabled == false) {
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
    if (codec->mType != SdpMediaSection::kApplication) {
      CSFLogError(LOGTAG,
                  "%s: Codec type for m=application was %u, this "
                  "is a bug.",
                  __FUNCTION__, static_cast<unsigned>(codec->mType));
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

already_AddRefed<TransceiverImpl> PeerConnectionImpl::CreateTransceiverImpl(
    JsepTransceiver* aJsepTransceiver, dom::MediaStreamTrack* aSendTrack,
    ErrorResult& aRv) {
  // TODO: Maybe this should be done in PeerConnectionMedia?
  if (aSendTrack) {
    aSendTrack->AddPrincipalChangeObserver(this);
  }

  PeerConnectionCtx* ctx = PeerConnectionCtx::GetInstance();
  RefPtr<TransceiverImpl> transceiverImpl;
  aRv = mMedia->AddTransceiver(aJsepTransceiver, aSendTrack,
                               ctx->GetSharedWebrtcState(), &transceiverImpl);

  return transceiverImpl.forget();
}

already_AddRefed<TransceiverImpl> PeerConnectionImpl::CreateTransceiverImpl(
    const nsAString& aKind, dom::MediaStreamTrack* aSendTrack,
    ErrorResult& jrv) {
  SdpMediaSection::MediaType type;
  if (aKind.EqualsASCII("audio")) {
    type = SdpMediaSection::MediaType::kAudio;
  } else if (aKind.EqualsASCII("video")) {
    type = SdpMediaSection::MediaType::kVideo;
  } else {
    MOZ_ASSERT(false);
    jrv = NS_ERROR_INVALID_ARG;
    return nullptr;
  }

  RefPtr<JsepTransceiver> jsepTransceiver = new JsepTransceiver(type);

  RefPtr<TransceiverImpl> transceiverImpl =
      CreateTransceiverImpl(jsepTransceiver, aSendTrack, jrv);

  if (jrv.Failed()) {
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
    jrv = rv;
    return nullptr;
  }

  return transceiverImpl.forget();
}

bool PeerConnectionImpl::CheckNegotiationNeeded(ErrorResult& rv) {
  MOZ_ASSERT(mSignalingState == RTCSignalingState::Stable);
  return mJsepSession->CheckNegotiationNeeded();
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

  options.mIceRestart = mozilla::Some(aOptions.mIceRestart);

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

  mThread->Dispatch(NS_NewRunnableFunction(
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

  mThread->Dispatch(NS_NewRunnableFunction(
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

  if (mMedia->AnyLocalTrackHasPeerIdentity()) {
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

      // New audio or video transceiver, need to tell JS about it.
      RefPtr<TransceiverImpl> transceiverImpl =
          CreateTransceiverImpl(jsepTransceiver, nullptr, jrv);
      if (jrv.Failed()) {
        appendHistory();
        return NS_ERROR_FAILURE;
      }

      const JsepTrack& receiving(jsepTransceiver->mRecvTrack);
      CSFLogInfo(LOGTAG, "%s: pc = %s, asking JS to create transceiver",
                 __FUNCTION__, mHandle.c_str());
      switch (receiving.GetMediaType()) {
        case SdpMediaSection::MediaType::kAudio:
          mPCObserver->OnTransceiverNeeded(NS_ConvertASCIItoUTF16("audio"),
                                           *transceiverImpl, jrv);
          break;
        case SdpMediaSection::MediaType::kVideo:
          mPCObserver->OnTransceiverNeeded(NS_ConvertASCIItoUTF16("video"),
                                           *transceiverImpl, jrv);
          break;
        default:
          MOZ_RELEASE_ASSERT(false);
      }

      if (jrv.Failed()) {
        nsresult rv = jrv.StealNSResult();
        CSFLogError(LOGTAG,
                    "%s: pc = %s, OnTransceiverNeeded failed. "
                    "This should never happen. rv = %d",
                    __FUNCTION__, mHandle.c_str(), static_cast<int>(rv));
        MOZ_CRASH();
        return NS_ERROR_FAILURE;
      }
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
  if (NS_FAILED(CheckApiState(false))) {
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mWindow);
  ErrorResult rv;
  RefPtr<Promise> promise = Promise::Create(global, rv);
  if (NS_WARN_IF(rv.Failed())) {
    rv.StealNSResult();
    return nullptr;
  }

  GetStats(aSelector, false)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [promise,
           window = mWindow](UniquePtr<dom::RTCStatsReportInternal>&& aReport) {
            RefPtr<RTCStatsReport> report(new RTCStatsReport(window));
            report->Incorporate(*aReport);
            promise->MaybeResolve(std::move(report));
          },
          [promise, window = mWindow](nsresult aError) {
            RefPtr<RTCStatsReport> report(new RTCStatsReport(window));
            promise->MaybeResolve(std::move(report));
          });

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
    // We do not bother PCMedia about this before offer/answer concludes.
    // Once offer/answer concludes, PCMedia will extract these candidates from
    // the remote SDP.
    if (mSignalingState == RTCSignalingState::Stable && !transportId.empty()) {
      mMedia->AddIceCandidate(aCandidate, transportId, aUfrag);
      mRawTrickledCandidates.push_back(aCandidate);
    }
    // Spec says we queue a task for these updates
    mThread->Dispatch(NS_NewRunnableFunction(
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

    mThread->Dispatch(NS_NewRunnableFunction(
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

void PeerConnectionImpl::UpdateNetworkState(bool online) {
  if (!mMedia) {
    return;
  }
  mMedia->UpdateNetworkState(online);
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
    Document* doc = GetWindow()->GetExtantDoc();
    if (!doc) {
      CSFLogInfo(LOGTAG, "Can't update principal on streams; document gone");
      return NS_ERROR_FAILURE;
    }
    MediaStreamTrack* allTracks = nullptr;
    mMedia->UpdateSinkIdentity_m(allTracks, doc->NodePrincipal(),
                                 mPeerIdentity);
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

void PeerConnectionImpl::PrincipalChanged(MediaStreamTrack* aTrack) {
  Document* doc = GetWindow()->GetExtantDoc();
  if (doc) {
    mMedia->UpdateSinkIdentity_m(aTrack, doc->NodePrincipal(), mPeerIdentity);
  } else {
    CSFLogInfo(LOGTAG, "Can't update sink principal; document gone");
  }
}

void PeerConnectionImpl::OnMediaError(const std::string& aError) {
  CSFLogError(LOGTAG, "Encountered media error! %s", aError.c_str());
  // TODO: Let content know about this somehow.
}

bool PeerConnectionImpl::ShouldDumpPacket(size_t level,
                                          dom::mozPacketDumpType type,
                                          bool sending) const {
  if (!mPacketDumpEnabled) {
    return false;
  }

  MutexAutoLock lock(mPacketDumpFlagsMutex);

  const std::vector<unsigned>* packetDumpFlags;

  if (sending) {
    packetDumpFlags = &mSendPacketDumpFlags;
  } else {
    packetDumpFlags = &mRecvPacketDumpFlags;
  }

  if (level < packetDumpFlags->size()) {
    unsigned flag = 1 << (unsigned)type;
    return flag & packetDumpFlags->at(level);
  }

  return false;
}

void PeerConnectionImpl::DumpPacket_m(size_t level, dom::mozPacketDumpType type,
                                      bool sending,
                                      UniquePtr<uint8_t[]>& packet,
                                      size_t size) {
  if (IsClosed()) {
    return;
  }

  if (!ShouldDumpPacket(level, type, sending)) {
    return;
  }

  // TODO: Is this efficient? Should we try grabbing our JS ctx from somewhere
  // else?
  AutoJSAPI jsapi;
  if (!jsapi.Init(GetWindow())) {
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
  mPacketDumpEnabled = true;
  std::vector<unsigned>* packetDumpFlags;
  if (sending) {
    packetDumpFlags = &mSendPacketDumpFlags;
  } else {
    packetDumpFlags = &mRecvPacketDumpFlags;
  }

  unsigned flag = 1 << (unsigned)type;

  MutexAutoLock lock(mPacketDumpFlagsMutex);
  if (level >= packetDumpFlags->size()) {
    packetDumpFlags->resize(level + 1);
  }

  (*packetDumpFlags)[level] |= flag;
  return NS_OK;
}

nsresult PeerConnectionImpl::DisablePacketDump(unsigned long level,
                                               dom::mozPacketDumpType type,
                                               bool sending) {
  std::vector<unsigned>* packetDumpFlags;
  if (sending) {
    packetDumpFlags = &mSendPacketDumpFlags;
  } else {
    packetDumpFlags = &mRecvPacketDumpFlags;
  }

  unsigned flag = 1 << (unsigned)type;

  MutexAutoLock lock(mPacketDumpFlagsMutex);
  if (level < packetDumpFlags->size()) {
    (*packetDumpFlags)[level] &= ~flag;
  }

  return NS_OK;
}

void PeerConnectionImpl::StampTimecard(const char* aEvent) {
  MOZ_ASSERT(NS_IsMainThread());
  STAMP_TIMECARD(mTimeCard, aEvent);
}

NS_IMETHODIMP
PeerConnectionImpl::ReplaceTrackNoRenegotiation(TransceiverImpl& aTransceiver,
                                                MediaStreamTrack* aWithTrack) {
  PC_AUTO_ENTER_API_CALL(true);

  RefPtr<dom::MediaStreamTrack> oldSendTrack(aTransceiver.GetSendTrack());
  if (oldSendTrack) {
    oldSendTrack->RemovePrincipalChangeObserver(this);
  }

  nsresult rv = aTransceiver.UpdateSendTrack(aWithTrack);

  if (NS_FAILED(rv)) {
    CSFLogError(LOGTAG, "Failed to update transceiver: %d",
                static_cast<int>(rv));
    return rv;
  }

  if (aWithTrack) {
    aWithTrack->AddPrincipalChangeObserver(this);
    PrincipalChanged(aWithTrack);
  }

  if (aTransceiver.IsVideo()) {
    // We update the media pipelines here so we can apply different codec
    // settings for different sources (e.g. screensharing as opposed to camera.)
    Maybe<MediaSourceEnum> oldType;
    Maybe<MediaSourceEnum> newType;
    if (oldSendTrack) {
      oldType = Some(oldSendTrack->GetSource().GetMediaSource());
    }
    if (aWithTrack) {
      newType = Some(aWithTrack->GetSource().GetMediaSource());
    }
    if (oldType != newType) {
      if (NS_WARN_IF(NS_FAILED(rv = aTransceiver.UpdateConduit()))) {
        CSFLogError(LOGTAG, "Error Updating VideoConduit");
        return rv;
      }
    }
  } else if (!oldSendTrack != !aWithTrack) {
    if (NS_WARN_IF(NS_FAILED(rv = aTransceiver.UpdateConduit()))) {
      CSFLogError(LOGTAG, "Error Updating AudioConduit");
      return rv;
    }
  }

  return NS_OK;
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
  if (!mMedia) {
    CSFLogError(LOGTAG, "%s: called API with disposed mMedia", __FUNCTION__);
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

  CloseInt();
  // Uncount this connection as active on the inner window upon close.
  if (mWindow && mActiveOnWindow) {
    mWindow->RemovePeerConnection();
    mActiveOnWindow = false;
  }

  return NS_OK;
}

bool PeerConnectionImpl::PluginCrash(uint32_t aPluginID,
                                     const nsAString& aPluginName) {
  // fire an event to the DOM window if this is "ours"
  bool result = mMedia ? mMedia->AnyCodecHasPluginID(aPluginID) : false;
  if (!result) {
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

  EventDispatcher::DispatchDOMEvent(mWindow, nullptr, event, nullptr, nullptr);

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

nsresult PeerConnectionImpl::CloseInt() {
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  mSignalingState = RTCSignalingState::Closed;

  // We do this at the end of the call because we want to make sure we've waited
  // for all trickle ICE candidates to come in; this can happen well after we've
  // transitioned to connected. As a bonus, this allows us to detect race
  // conditions where a stats dispatch happens right as the PC closes.
  if (!mPrivateWindow) {
    RecordLongtermICEStatistics();
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
  ShutdownMedia();

  // DataConnection will need to stay alive until all threads/runnables exit

  return NS_OK;
}

void PeerConnectionImpl::ShutdownMedia() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mMedia) return;

  // before we destroy references to local tracks, detach from them
  for (RefPtr<TransceiverImpl>& transceiver : mMedia->GetTransceivers()) {
    RefPtr<dom::MediaStreamTrack> track = transceiver->GetSendTrack();
    if (track) {
      track->RemovePrincipalChangeObserver(this);
    }
  }

  mMedia->Shutdown();
  mMedia = nullptr;
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

  mThread->Dispatch(NS_NewRunnableFunction(
      __func__, [this, self = RefPtr<PeerConnectionImpl>(this),
                 newSignalingState, sdpType, remote] {
        if (IsClosed()) {
          return;
        }

        if (HasMedia()) {
          // Section 4.4.1.5 Set the RTCSessionDescription:
          if (sdpType == mozilla::kJsepSdpRollback) {
            // - step 4.5.10, type is rollback
            mMedia->RollbackRTCDtlsTransports();
          } else if (!(remote && sdpType == mozilla::kJsepSdpOffer)) {
            // - step 4.5.9 type is not rollback
            // - step 4.5.9.1 when remote is false
            // - step 4.5.9.2.13 when remote is true, type answer or pranswer
            // More simply: not rollback, and not for remote offers.
            bool markAsStable = sdpType == kJsepSdpOffer &&
                                mSignalingState == RTCSignalingState::Stable;
            mMedia->UpdateRTCDtlsTransports(markAsStable);
          }
        }

        JSErrorResult jrv;
        mPCObserver->SyncTransceivers(jrv);
        if (NS_WARN_IF(jrv.Failed())) {
          return;
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
        if (newSignalingState != mSignalingState) {
          mSignalingState = newSignalingState;
          mPCObserver->OnStateChange(PCObserverStateType::SignalingState, jrv);
        }

        // TODO: Spec says that we should do this even if JS closes the PC
        // during the signalingstatechange event. We'd need to refactor a
        // little here to make this possible.
        if (remote && HasMedia()) {
          dom::RTCRtpReceiver::StreamAssociationChanges changes;
          for (const auto& transceiver : mMedia->GetTransceivers()) {
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
    // We'd like to handle this in PeerConnectionMedia::UpdateNetworkState.
    // Unfortunately, if the WiFi switch happens quickly, we never see
    // that state change.  We need to detect the ice restart here and
    // reset the PeerConnectionMedia's stun addresses so they are
    // regathered when PeerConnectionMedia::GatherIfReady is called.
    if (mJsepSession->IsIceRestarting()) {
      mMedia->ResetStunAddrsForIceRestart();
    }
    mMedia->EnsureTransports(*mJsepSession);
  }

  if (mJsepSession->GetState() != kJsepStateStable) {
    return;  // The rest of this stuff is done only when offer/answer is done
  }

  // If we're rolling back a local offer, we might need to remove some
  // transports, and stomp some MediaPipeline setup, but nothing further
  // needs to be done.
  mMedia->UpdateTransports(*mJsepSession, mForceIceTcp);
  if (NS_FAILED(mMedia->UpdateMediaPipelines())) {
    CSFLogError(LOGTAG, "Error Updating MediaPipelines");
    NS_ASSERTION(false,
                 "Error Updating MediaPipelines in OnSetDescriptionSuccess()");
    // XXX what now?  Not much we can do but keep going, without major
    // restructuring
  }

  if (sdpType != kJsepSdpRollback) {
    InitializeDataChannel();
    mMedia->StartIceChecks(*mJsepSession);
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

bool PeerConnectionImpl::HasMedia() const { return mMedia; }

PeerConnectionWrapper::PeerConnectionWrapper(const std::string& handle)
    : impl_(nullptr) {
  PeerConnectionImpl* impl =
      PeerConnectionCtx::GetInstance()->GetPeerConnection(handle);

  if (!PeerConnectionCtx::isActive() || !impl) {
    return;
  }

  if (!impl->media()) return;

  impl_ = impl;
}

const RefPtr<MediaTransportHandler> PeerConnectionImpl::GetTransportHandler()
    const {
  return mTransportHandler;
}

const std::string& PeerConnectionImpl::GetHandle() {
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  return mHandle;
}

const std::string& PeerConnectionImpl::GetName() {
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  return mName;
}

void PeerConnectionImpl::CandidateReady(const std::string& candidate,
                                        const std::string& transportId,
                                        const std::string& ufrag) {
  PC_AUTO_ENTER_API_CALL_VOID_RETURN(false);

  if (mForceIceTcp && std::string::npos != candidate.find(" UDP ")) {
    CSFLogWarn(LOGTAG, "Blocking local UDP candidate: %s", candidate.c_str());
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

    CSFLogError(LOGTAG,
                "Failed to incorporate local candidate into SDP:"
                " res = %u, candidate = %s, transport-id = %s,"
                " error = %s",
                static_cast<unsigned>(res), candidate.c_str(),
                transportId.c_str(), errorString.c_str());
    return;
  }

  if (skipped) {
    CSFLogDebug(LOGTAG,
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
  CSFLogDebug(LOGTAG, "Passing local candidate to content: %s",
              candidate.c_str());
  SendLocalIceCandidateToContent(level, mid, candidate, ufrag);
}

void PeerConnectionImpl::SendLocalIceCandidateToContent(
    uint16_t level, const std::string& mid, const std::string& candidate,
    const std::string& ufrag) {
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

  // Uncount this connection as active on the inner window upon close.
  if (mWindow && mActiveOnWindow &&
      mIceConnectionState == RTCIceConnectionState::Closed) {
    mWindow->RemovePeerConnection();
    mActiveOnWindow = false;
  }

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

// TODO(bug 1616937): Move this to RTCRtpSender.
nsTArray<RefPtr<dom::RTCStatsPromise>> PeerConnectionImpl::GetSenderStats(
    const RefPtr<MediaPipelineTransmit>& aPipeline) {
  MOZ_ASSERT(NS_IsMainThread());
  nsTArray<RefPtr<RTCStatsPromise>> promises(2);

  nsAutoString trackName;
  if (auto track = aPipeline->GetTrack()) {
    track->GetId(trackName);
  }

  {
    // Add bandwidth estimation stats
    promises.AppendElement(InvokeAsync(
        aPipeline->mCallThread, __func__,
        [conduit = aPipeline->mConduit, trackName]() mutable {
          auto report = MakeUnique<dom::RTCStatsCollection>();
          Maybe<webrtc::Call::Stats> stats = conduit->GetCallStats();
          stats.apply([&](const auto aStats) {
            dom::RTCBandwidthEstimationInternal bw;
            bw.mTrackIdentifier = trackName;
            bw.mSendBandwidthBps.Construct(aStats.send_bandwidth_bps / 8);
            bw.mMaxPaddingBps.Construct(aStats.max_padding_bitrate_bps / 8);
            bw.mReceiveBandwidthBps.Construct(aStats.recv_bandwidth_bps / 8);
            bw.mPacerDelayMs.Construct(aStats.pacer_delay_ms);
            if (aStats.rtt_ms >= 0) {
              bw.mRttMs.Construct(aStats.rtt_ms);
            }
            if (!report->mBandwidthEstimations.AppendElement(std::move(bw),
                                                             fallible)) {
              mozalloc_handle_oom(0);
            }
          });
          return RTCStatsPromise::CreateAndResolve(std::move(report), __func__);
        }));
  }

  promises.AppendElement(
      InvokeAsync(aPipeline->mCallThread, __func__, [aPipeline] {
        auto report = MakeUnique<dom::RTCStatsCollection>();
        auto asAudio = aPipeline->mConduit->AsAudioSessionConduit();
        auto asVideo = aPipeline->mConduit->AsVideoSessionConduit();

        nsString kind = asVideo.isNothing() ? u"audio"_ns : u"video"_ns;
        nsString idstr = kind + u"_"_ns;
        idstr.AppendInt(static_cast<uint32_t>(aPipeline->Level()));

        for (uint32_t ssrc : aPipeline->mConduit->GetLocalSSRCs()) {
          nsString localId = u"outbound_rtp_"_ns + idstr + u"_"_ns;
          localId.AppendInt(ssrc);
          nsString remoteId;
          Maybe<uint16_t> base_seq =
              aPipeline->mConduit->RtpSendBaseSeqFor(ssrc);

          auto constructCommonRemoteInboundRtpStats =
              [&](RTCRemoteInboundRtpStreamStats& aRemote,
                  const webrtc::ReportBlockData& aRtcpData) {
                remoteId = u"outbound_rtcp_"_ns + idstr + u"_"_ns;
                remoteId.AppendInt(ssrc);
                aRemote.mTimestamp.Construct(
                    aPipeline->GetTimestampMaker().ConvertNtpToDomTime(
                        webrtc::Timestamp::Micros(
                            aRtcpData.report_block_timestamp_utc_us()) +
                        webrtc::TimeDelta::Seconds(webrtc::kNtpJan1970)));
                aRemote.mId.Construct(remoteId);
                aRemote.mType.Construct(RTCStatsType::Remote_inbound_rtp);
                aRemote.mSsrc.Construct(ssrc);
                aRemote.mMediaType.Construct(
                    kind);  // mediaType is the old name for kind.
                aRemote.mKind.Construct(kind);
                aRemote.mLocalId.Construct(localId);
                if (base_seq) {
                  if (aRtcpData.report_block()
                          .extended_highest_sequence_number < *base_seq) {
                    aRemote.mPacketsReceived.Construct(0);
                  } else {
                    aRemote.mPacketsReceived.Construct(
                        aRtcpData.report_block()
                            .extended_highest_sequence_number -
                        aRtcpData.report_block().packets_lost - *base_seq + 1);
                  }
                }
              };

          auto constructCommonOutboundRtpStats =
              [&](RTCOutboundRtpStreamStats& aLocal) {
                aLocal.mSsrc.Construct(ssrc);
                aLocal.mTimestamp.Construct(
                    aPipeline->GetTimestampMaker().GetNow());
                aLocal.mId.Construct(localId);
                aLocal.mType.Construct(RTCStatsType::Outbound_rtp);
                aLocal.mMediaType.Construct(
                    kind);  // mediaType is the old name for kind.
                aLocal.mKind.Construct(kind);
                if (remoteId.Length()) {
                  aLocal.mRemoteId.Construct(remoteId);
                }
              };

          asAudio.apply([&](auto& aConduit) {
            Maybe<webrtc::AudioSendStream::Stats> audioStats =
                aConduit->GetSenderStats();
            if (audioStats.isNothing()) {
              return;
            }

            // First, fill in remote stat with rtcp receiver data, if present.
            // ReceiverReports have less information than SenderReports, so fill
            // in what we can.
            Maybe<webrtc::ReportBlockData> reportBlockData;
            {
              if (const auto remoteSsrc = aConduit->GetRemoteSSRC();
                  remoteSsrc) {
                for (auto& data : audioStats->report_block_datas) {
                  if (data.report_block().source_ssrc == ssrc &&
                      data.report_block().sender_ssrc == *remoteSsrc) {
                    reportBlockData.emplace(data);
                    break;
                  }
                }
              }
            }
            reportBlockData.apply([&](auto& aReportBlockData) {
              RTCRemoteInboundRtpStreamStats remote;
              constructCommonRemoteInboundRtpStats(remote, aReportBlockData);
              if (audioStats->jitter_ms >= 0) {
                remote.mJitter.Construct(audioStats->jitter_ms / 1000.0);
              }
              if (audioStats->packets_lost >= 0) {
                remote.mPacketsLost.Construct(audioStats->packets_lost);
              }
              if (audioStats->rtt_ms >= 0) {
                remote.mRoundTripTime.Construct(
                    static_cast<double>(audioStats->rtt_ms) / 1000.0);
              }
              /*
               * Potential new stats that are now available upstream.
              remote.mFractionLost.Construct(audioStats->fraction_lost);
              remote.mTotalRoundTripTime.Construct(
                  double(aReportBlockData.sum_rtt_ms()) / 1000);
              remote.mRoundTripTimeMeasurements.Construct(
                  aReportBlockData.num_rtts());
               */
              if (!report->mRemoteInboundRtpStreamStats.AppendElement(
                      std::move(remote), fallible)) {
                mozalloc_handle_oom(0);
              }
            });

            // Then, fill in local side (with cross-link to remote only if
            // present)
            RTCOutboundRtpStreamStats local;
            constructCommonOutboundRtpStats(local);
            local.mPacketsSent.Construct(audioStats->packets_sent);
            local.mBytesSent.Construct(audioStats->payload_bytes_sent);
            local.mNackCount.Construct(
                audioStats->rtcp_packet_type_counts.nack_packets);
            /*
             * Potential new stats that are now available upstream.
            local.mHeaderBytesSent.Construct(
                audioStats->header_and_padding_bytes_sent);
            local.mRetransmittedPacketsSent.Construct(
                audioStats->retransmitted_packets_sent);
            local.mRetransmittedBytesSent.Construct(
                audioStats->retransmitted_bytes_sent);
            local.mTargetBitrate.Construct(audioStats->target_bitrate_bps);
             */
            if (!report->mOutboundRtpStreamStats.AppendElement(std::move(local),
                                                               fallible)) {
              mozalloc_handle_oom(0);
            }
          });

          asVideo.apply([&](auto& aConduit) {
            Maybe<webrtc::VideoSendStream::Stats> videoStats =
                aConduit->GetSenderStats();
            if (videoStats.isNothing()) {
              return;
            }

            Maybe<webrtc::VideoSendStream::StreamStats> streamStats;
            auto kv = videoStats->substreams.find(ssrc);
            if (kv != videoStats->substreams.end()) {
              streamStats = Some(kv->second);
            }

            // First, fill in remote stat with rtcp receiver data, if present.
            // ReceiverReports have less information than SenderReports, so fill
            // in what we can.
            if (streamStats && streamStats->report_block_data) {
              const webrtc::ReportBlockData& rtcpReportData =
                  *streamStats->report_block_data;
              RTCRemoteInboundRtpStreamStats remote;
              remote.mJitter.Construct(
                  static_cast<double>(streamStats->rtcp_stats.jitter) /
                  webrtc::kVideoPayloadTypeFrequency);
              remote.mPacketsLost.Construct(
                  streamStats->rtcp_stats.packets_lost);
              if (rtcpReportData.has_rtt()) {
                remote.mRoundTripTime.Construct(
                    static_cast<double>(rtcpReportData.last_rtt_ms()) / 1000.0);
              }
              constructCommonRemoteInboundRtpStats(remote, rtcpReportData);
              /*
               * Potential new stats that are now available upstream.
              remote.mTotalRoundTripTime.Construct(
                  streamStats->report_block_data->sum_rtt_ms() / 1000.0);
              remote.mFractionLost.Construct(
                  static_cast<float>(streamStats->rtcp_stats.fraction_lost) /
                  (1 << 8));
              remote.mRoundTripTimeMeasurements.Construct(
                  streamStats->report_block_data.num_rtts());
               */
              if (!report->mRemoteInboundRtpStreamStats.AppendElement(
                      std::move(remote), fallible)) {
                mozalloc_handle_oom(0);
              }
            }

            // Then, fill in local side (with cross-link to remote only if
            // present)
            RTCOutboundRtpStreamStats local;
            constructCommonOutboundRtpStats(local);
            streamStats.apply([&](auto& aStreamStats) {
              local.mPacketsSent.Construct(
                  aStreamStats.rtp_stats.transmitted.packets);
              local.mBytesSent.Construct(
                  aStreamStats.rtp_stats.transmitted.payload_bytes);
              local.mNackCount.Construct(
                  aStreamStats.rtcp_packet_type_counts.nack_packets);
              local.mFirCount.Construct(
                  aStreamStats.rtcp_packet_type_counts.fir_packets);
              local.mPliCount.Construct(
                  aStreamStats.rtcp_packet_type_counts.pli_packets);
              local.mFramesEncoded.Construct(aStreamStats.frames_encoded);
              if (aStreamStats.qp_sum) {
                local.mQpSum.Construct(*aStreamStats.qp_sum);
              }
            });
            /*
             * Potential new stats that are now available upstream.
            local.mHeaderBytesSent.Construct(
                aStreamStats.rtp_stats.transmitted.header_bytes +
                aStreamStats.rtp_stats.transmitted.padding_bytes);
            local.mRetransmittedPacketsSent.Construct(
                aStreamStats.rtp_stats.retransmitted.packets);
            local.mRetransmittedBytesSent.Construct(
                aStreamStats.rtp_stats.retransmitted.payload_bytes);
            local.mTargetBitrate.Construct(videoStats->target_media_bitrate_bps);
            local.mTotalEncodedBytesTarget.Construct(
                videoStats->total_encoded_bytes_target);
             */
            if (!report->mOutboundRtpStreamStats.AppendElement(std::move(local),
                                                               fallible)) {
              mozalloc_handle_oom(0);
            }
          });
        }
        return RTCStatsPromise::CreateAndResolve(std::move(report), __func__);
      }));

  return promises;
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

  if (!mMedia) {
    return;
  }

  nsTArray<RefPtr<VideoSessionConduit>> conduits;
  for (const auto& transceiver : mMedia->GetTransceivers()) {
    if (RefPtr<MediaSessionConduit> conduit = transceiver->GetConduit()) {
      conduit->AsVideoSessionConduit().apply(
          [&](const auto& aVideo) { conduits.AppendElement(aVideo); });
    }
  }

  if (!conduits.IsEmpty()) {
    mMedia->mCall->mCallThread->Dispatch(
        NS_NewRunnableFunction(__func__, [conduits = std::move(conduits)] {
          for (const auto& conduit : conduits) {
            conduit->CollectTelemetryData();
          }
        }));
  }
}

template <class T>
void AssignWithOpaqueIds(dom::Sequence<T>& aSource, dom::Sequence<T>& aDest,
                         RefPtr<RTCStatsIdGenerator>& aGenerator) {
  for (auto& stat : aSource) {
    stat.mId.Value() = aGenerator->Id(stat.mId.Value());
  }
  if (!aDest.AppendElements(aSource, fallible)) {
    mozalloc_handle_oom(0);
  }
}

template <class T>
void RewriteRemoteIds(dom::Sequence<T>& aList,
                      RefPtr<RTCStatsIdGenerator>& aGenerator) {
  for (auto& stat : aList) {
    if (stat.mRemoteId.WasPassed()) {
      stat.mRemoteId.Value() = aGenerator->Id(stat.mRemoteId.Value());
    }
  }
}

template <class T>
void RewriteLocalIds(dom::Sequence<T>& aList,
                     RefPtr<RTCStatsIdGenerator>& aGenerator) {
  for (auto& stat : aList) {
    if (stat.mLocalId.WasPassed()) {
      stat.mLocalId.Value() = aGenerator->Id(stat.mLocalId.Value());
    }
  }
}

RefPtr<dom::RTCStatsReportPromise> PeerConnectionImpl::GetStats(
    dom::MediaStreamTrack* aSelector, bool aInternalStats) {
  MOZ_ASSERT(NS_IsMainThread());
  nsTArray<RefPtr<dom::RTCStatsPromise>> promises;
  DOMHighResTimeStamp now = mTimestampMaker.GetNow();

  if (mMedia) {
    nsTArray<RefPtr<MediaPipelineTransmit>> sendPipelines;
    // Gather up pipelines from mMedia so they may be inspected on STS
    // TODO(bug 1616937): Use RTCRtpSender for these instead.
    mMedia->GetTransmitPipelinesMatching(aSelector, &sendPipelines);
    if (!sendPipelines.Length()) {
      CSFLogError(LOGTAG, "%s: Found no pipelines matching selector.",
                  __FUNCTION__);
    }

    for (const auto& pipeline : sendPipelines) {
      promises.AppendElements(GetSenderStats(pipeline));
    }

    for (const auto& transceiver : mMedia->GetTransceivers()) {
      if (transceiver->Receiver()->HasTrack(aSelector)) {
        // Right now, returns two promises; one for RTP/RTCP stats, and
        // another for ICE stats.
        promises.AppendElements(transceiver->Receiver()->GetStatsInternal());
      }
    }

    // TODO(bug 1616937): We need to move this is RTCRtpSender, to make
    // getStats on those objects work properly. It might be worth optimizing
    // the null selector case, so we don't end up with bunches of copies of
    // the same transport information in the final report.
    if (aSelector) {
      std::string transportId =
          mMedia->GetTransportIdMatchingSendTrack(*aSelector);
      if (!transportId.empty()) {
        promises.AppendElement(
            mTransportHandler->GetIceStats(transportId, now));
      }
    } else {
      promises.AppendElement(mTransportHandler->GetIceStats("", now));
    }

    promises.AppendElement(GetDataChannelStats(mDataConnection, now));
  }

  // This is what we're going to return; all the stuff in |promises| will be
  // accumulated here.
  UniquePtr<dom::RTCStatsReportInternal> report(
      new dom::RTCStatsReportInternal);
  report->mPcid = NS_ConvertASCIItoUTF16(mName.c_str());
  report->mBrowserId = mWindow->GetBrowsingContext()->BrowserId();
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

  return dom::RTCStatsPromise::All(mThread, promises)
      ->Then(
          mThread, __func__,
          [report = std::move(report), idGen = mIdGenerator](
              const nsTArray<UniquePtr<dom::RTCStatsCollection>>&
                  aStats) mutable {
            // Rewrite an Optional id
            auto rewriteId = [&idGen](Optional<nsString>& id) {
              if (id.WasPassed()) {
                id.Value() = idGen->Id(id.Value());
              }
            };

            // Involves a lot of copying, since webidl dictionaries don't have
            // move semantics. Oh well.
            for (const auto& stats : aStats) {
              for (auto& stat : stats->mIceCandidatePairStats) {
                rewriteId(stat.mLocalCandidateId);
                rewriteId(stat.mRemoteCandidateId);
              };
              AssignWithOpaqueIds(stats->mIceCandidatePairStats,
                                  report->mIceCandidatePairStats, idGen);

              AssignWithOpaqueIds(stats->mIceCandidateStats,
                                  report->mIceCandidateStats, idGen);

              RewriteRemoteIds(stats->mInboundRtpStreamStats, idGen);
              AssignWithOpaqueIds(stats->mInboundRtpStreamStats,
                                  report->mInboundRtpStreamStats, idGen);

              RewriteRemoteIds(stats->mOutboundRtpStreamStats, idGen);
              AssignWithOpaqueIds(stats->mOutboundRtpStreamStats,
                                  report->mOutboundRtpStreamStats, idGen);

              RewriteLocalIds(stats->mRemoteInboundRtpStreamStats, idGen);
              AssignWithOpaqueIds(stats->mRemoteInboundRtpStreamStats,
                                  report->mRemoteInboundRtpStreamStats, idGen);

              RewriteLocalIds(stats->mRemoteOutboundRtpStreamStats, idGen);
              AssignWithOpaqueIds(stats->mRemoteOutboundRtpStreamStats,
                                  report->mRemoteOutboundRtpStreamStats, idGen);

              AssignWithOpaqueIds(stats->mRtpContributingSourceStats,
                                  report->mRtpContributingSourceStats, idGen);
              AssignWithOpaqueIds(stats->mTrickledIceCandidateStats,
                                  report->mTrickledIceCandidateStats, idGen);
              AssignWithOpaqueIds(stats->mDataChannelStats,
                                  report->mDataChannelStats, idGen);
              if (!report->mRawLocalCandidates.AppendElements(
                      stats->mRawLocalCandidates, fallible) ||
                  !report->mRawRemoteCandidates.AppendElements(
                      stats->mRawRemoteCandidates, fallible) ||
                  !report->mVideoFrameHistories.AppendElements(
                      stats->mVideoFrameHistories, fallible) ||
                  !report->mBandwidthEstimations.AppendElements(
                      stats->mBandwidthEstimations, fallible)) {
                // XXX(Bug 1632090) Instead of extending the array 1-by-1
                // (which might involve multiple reallocations) and
                // potentially crashing here, SetCapacity could be called
                // outside the loop once.
                mozalloc_handle_oom(0);
              }
            }
            return dom::RTCStatsReportPromise::CreateAndResolve(
                std::move(report), __func__);
          },
          [](nsresult rv) {
            return dom::RTCStatsReportPromise::CreateAndReject(rv, __func__);
          });
}

void PeerConnectionImpl::RecordLongtermICEStatistics() {
  WebrtcGlobalInformation::StoreLongTermICEStatistics(*this);
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
  if (aConfig.mIceServers.WasPassed()) {
    for (const auto& server : aConfig.mIceServers.Value()) {
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
  }
  if (aConfig.mSdpSemantics.WasPassed()) {
    mJsConfiguration.mSdpSemantics.Construct(aConfig.mSdpSemantics.Value());
  }

  mJsConfiguration.mIceTransportPolicy.Construct(aConfig.mIceTransportPolicy);
  mJsConfiguration.mBundlePolicy.Construct(aConfig.mBundlePolicy);
  mJsConfiguration.mPeerIdentityProvided = !aConfig.mPeerIdentity.IsEmpty();
  mJsConfiguration.mCertificatesProvided =
      aConfig.mCertificates.WasPassed() &&
      !aConfig.mCertificates.Value().Length();
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

std::map<uint64_t, PeerConnectionAutoTimer>
    PeerConnectionImpl::sCallDurationTimers;
}  // namespace mozilla

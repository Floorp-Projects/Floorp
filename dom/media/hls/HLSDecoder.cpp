/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HLSDecoder.h"
#include "AndroidBridge.h"
#include "base/process_util.h"
#include "DecoderTraits.h"
#include "HLSDemuxer.h"
#include "HLSUtils.h"
#include "JavaBuiltins.h"
#include "MediaContainerType.h"
#include "MediaDecoderStateMachine.h"
#include "MediaFormatReader.h"
#include "MediaShutdownManager.h"
#include "mozilla/java/GeckoHLSResourceWrapperNatives.h"
#include "nsContentUtils.h"
#include "nsIChannel.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/StaticPrefs_media.h"

namespace mozilla {

class HLSResourceCallbacksSupport
    : public java::GeckoHLSResourceWrapper::Callbacks::Natives<
          HLSResourceCallbacksSupport> {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(HLSResourceCallbacksSupport)
 public:
  typedef java::GeckoHLSResourceWrapper::Callbacks::Natives<
      HLSResourceCallbacksSupport>
      NativeCallbacks;
  using NativeCallbacks::AttachNative;
  using NativeCallbacks::DisposeNative;

  explicit HLSResourceCallbacksSupport(HLSDecoder* aResource);
  void Detach();
  void OnLoad(jni::String::Param aUrl);
  void OnDataArrived();
  void OnError(int aErrorCode);

 private:
  ~HLSResourceCallbacksSupport() {}
  Mutex mMutex MOZ_UNANNOTATED;
  HLSDecoder* mDecoder;
};

HLSResourceCallbacksSupport::HLSResourceCallbacksSupport(HLSDecoder* aDecoder)
    : mMutex("HLSResourceCallbacksSupport"), mDecoder(aDecoder) {
  MOZ_ASSERT(mDecoder);
}

void HLSResourceCallbacksSupport::Detach() {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);
  mDecoder = nullptr;
}

void HLSResourceCallbacksSupport::OnLoad(jni::String::Param aUrl) {
  MutexAutoLock lock(mMutex);
  if (!mDecoder) {
    return;
  }
  RefPtr<HLSResourceCallbacksSupport> self = this;
  jni::String::GlobalRef url = std::move(aUrl);
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "HLSResourceCallbacksSupport::OnLoad", [self, url]() -> void {
        if (self->mDecoder) {
          self->mDecoder->NotifyLoad(url->ToCString());
        }
      }));
}

void HLSResourceCallbacksSupport::OnDataArrived() {
  HLS_DEBUG("HLSResourceCallbacksSupport", "OnDataArrived.");
  MutexAutoLock lock(mMutex);
  if (!mDecoder) {
    return;
  }
  RefPtr<HLSResourceCallbacksSupport> self = this;
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "HLSResourceCallbacksSupport::OnDataArrived", [self]() -> void {
        if (self->mDecoder) {
          self->mDecoder->NotifyDataArrived();
        }
      }));
}

void HLSResourceCallbacksSupport::OnError(int aErrorCode) {
  HLS_DEBUG("HLSResourceCallbacksSupport", "onError(%d)", aErrorCode);
  MutexAutoLock lock(mMutex);
  if (!mDecoder) {
    return;
  }
  RefPtr<HLSResourceCallbacksSupport> self = this;
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "HLSResourceCallbacksSupport::OnError", [self]() -> void {
        if (self->mDecoder) {
          // Since HLS source should be from the Internet, we treat all resource
          // errors from GeckoHlsPlayer as network errors.
          self->mDecoder->NetworkError(
              MediaResult(NS_ERROR_FAILURE, "HLS error"));
        }
      }));
}

size_t HLSDecoder::sAllocatedInstances = 0;

// static
RefPtr<HLSDecoder> HLSDecoder::Create(MediaDecoderInit& aInit) {
  MOZ_ASSERT(NS_IsMainThread());

  return sAllocatedInstances < StaticPrefs::media_hls_max_allocations()
             ? new HLSDecoder(aInit)
             : nullptr;
}

HLSDecoder::HLSDecoder(MediaDecoderInit& aInit) : MediaDecoder(aInit) {
  MOZ_ASSERT(NS_IsMainThread());
  sAllocatedInstances++;
  HLS_DEBUG("HLSDecoder", "HLSDecoder(): allocated=%zu", sAllocatedInstances);
}

HLSDecoder::~HLSDecoder() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(sAllocatedInstances > 0);
  sAllocatedInstances--;
  HLS_DEBUG("HLSDecoder", "~HLSDecoder(): allocated=%zu", sAllocatedInstances);
}

MediaDecoderStateMachineBase* HLSDecoder::CreateStateMachine(
    bool aDisableExternalEngine) {
  MOZ_ASSERT(NS_IsMainThread());

  MediaFormatReaderInit init;
  init.mVideoFrameContainer = GetVideoFrameContainer();
  init.mKnowsCompositor = GetCompositor();
  init.mCrashHelper = GetOwner()->CreateGMPCrashHelper();
  init.mFrameStats = mFrameStats;
  init.mMediaDecoderOwnerID = mOwner;
  static Atomic<uint32_t> sTrackingIdCounter(0);
  init.mTrackingId =
      Some(TrackingId(TrackingId::Source::HLSDecoder, sTrackingIdCounter++,
                      TrackingId::TrackAcrossProcesses::Yes));
  mReader = new MediaFormatReader(
      init, new HLSDemuxer(mHLSResourceWrapper->GetPlayerId()));

  return new MediaDecoderStateMachine(this, mReader);
}

bool HLSDecoder::IsEnabled() { return StaticPrefs::media_hls_enabled(); }

bool HLSDecoder::IsSupportedType(const MediaContainerType& aContainerType) {
  return IsEnabled() && DecoderTraits::IsHttpLiveStreamingType(aContainerType);
}

nsresult HLSDecoder::Load(nsIChannel* aChannel) {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(mURI));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mChannel = aChannel;
  nsCString spec;
  Unused << mURI->GetSpec(spec);
  ;
  HLSResourceCallbacksSupport::Init();
  mJavaCallbacks = java::GeckoHLSResourceWrapper::Callbacks::New();
  mCallbackSupport = new HLSResourceCallbacksSupport(this);
  HLSResourceCallbacksSupport::AttachNative(mJavaCallbacks, mCallbackSupport);
  mHLSResourceWrapper = java::GeckoHLSResourceWrapper::Create(
      NS_ConvertUTF8toUTF16(spec), mJavaCallbacks);
  MOZ_ASSERT(mHLSResourceWrapper);

  rv = MediaShutdownManager::Instance().Register(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return CreateAndInitStateMachine(false);
}

void HLSDecoder::AddSizeOfResources(ResourceSizes* aSizes) {
  MOZ_ASSERT(NS_IsMainThread());
  // TODO: track JAVA wrappers.
}

already_AddRefed<nsIPrincipal> HLSDecoder::GetCurrentPrincipal() {
  MOZ_ASSERT(NS_IsMainThread());
  return do_AddRef(mContentPrincipal);
}

bool HLSDecoder::HadCrossOriginRedirects() {
  MOZ_ASSERT(NS_IsMainThread());
  // Bug 1478843
  return false;
}

void HLSDecoder::Play() {
  MOZ_ASSERT(NS_IsMainThread());
  HLS_DEBUG("HLSDecoder", "MediaElement called Play");
  mHLSResourceWrapper->Play();
  return MediaDecoder::Play();
}

void HLSDecoder::Pause() {
  MOZ_ASSERT(NS_IsMainThread());
  HLS_DEBUG("HLSDecoder", "MediaElement called Pause");
  mHLSResourceWrapper->Pause();
  return MediaDecoder::Pause();
}

void HLSDecoder::Suspend() {
  MOZ_ASSERT(NS_IsMainThread());
  HLS_DEBUG("HLSDecoder", "Should suspend the resource fetching.");
  mHLSResourceWrapper->Suspend();
}

void HLSDecoder::Resume() {
  MOZ_ASSERT(NS_IsMainThread());
  HLS_DEBUG("HLSDecoder", "Should resume the resource fetching.");
  mHLSResourceWrapper->Resume();
}

void HLSDecoder::Shutdown() {
  HLS_DEBUG("HLSDecoder", "Shutdown");
  if (mCallbackSupport) {
    mCallbackSupport->Detach();
  }
  if (mHLSResourceWrapper) {
    mHLSResourceWrapper->Destroy();
    mHLSResourceWrapper = nullptr;
  }
  if (mJavaCallbacks) {
    HLSResourceCallbacksSupport::DisposeNative(mJavaCallbacks);
    mJavaCallbacks = nullptr;
  }
  MediaDecoder::Shutdown();
}

void HLSDecoder::NotifyDataArrived() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  NotifyReaderDataArrived();
  GetOwner()->DownloadProgressed();
}

void HLSDecoder::NotifyLoad(nsCString aMediaUrl) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  UpdateCurrentPrincipal(aMediaUrl);
}

// Should be called when the decoder loads media from a URL to ensure the
// principal of the media element is appropriately set for CORS.
void HLSDecoder::UpdateCurrentPrincipal(nsCString aMediaUrl) {
  nsCOMPtr<nsIPrincipal> principal = GetContentPrincipal(aMediaUrl);
  MOZ_DIAGNOSTIC_ASSERT(principal);

  // Check the subsumption of old and new principals. Should be either
  // equal or disjoint.
  if (!mContentPrincipal) {
    mContentPrincipal = principal;
  } else if (principal->Equals(mContentPrincipal)) {
    return;
  } else if (!principal->Subsumes(mContentPrincipal) &&
             !mContentPrincipal->Subsumes(principal)) {
    // Principals are disjoint -- no access.
    mContentPrincipal = NullPrincipal::Create(OriginAttributes());
  } else {
    MOZ_DIAGNOSTIC_ASSERT(false, "non-equal principals should be disjoint");
    mContentPrincipal = nullptr;
  }
  MediaDecoder::NotifyPrincipalChanged();
}

already_AddRefed<nsIPrincipal> HLSDecoder::GetContentPrincipal(
    nsCString aMediaUrl) {
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aMediaUrl.Data());
  NS_ENSURE_SUCCESS(rv, nullptr);
  RefPtr<dom::HTMLMediaElement> element = GetOwner()->GetMediaElement();
  NS_ENSURE_SUCCESS(rv, nullptr);
  nsSecurityFlags securityFlags =
      element->ShouldCheckAllowOrigin()
          ? nsILoadInfo::SEC_REQUIRE_CORS_INHERITS_SEC_CONTEXT
          : nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_INHERITS_SEC_CONTEXT;
  if (element->GetCORSMode() == CORS_USE_CREDENTIALS) {
    securityFlags |= nsILoadInfo::SEC_COOKIES_INCLUDE;
  }
  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), uri,
                     static_cast<dom::Element*>(element), securityFlags,
                     nsIContentPolicy::TYPE_INTERNAL_VIDEO);
  NS_ENSURE_SUCCESS(rv, nullptr);
  nsCOMPtr<nsIPrincipal> principal;
  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  if (!secMan) {
    return nullptr;
  }
  secMan->GetChannelResultPrincipal(channel, getter_AddRefs(principal));
  return principal.forget();
}

}  // namespace mozilla

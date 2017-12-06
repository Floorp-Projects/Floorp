/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "HLSDecoder.h"
#include "AndroidBridge.h"
#include "DecoderTraits.h"
#include "GeneratedJNINatives.h"
#include "GeneratedJNIWrappers.h"
#include "HLSDemuxer.h"
#include "HLSUtils.h"
#include "MediaContainerType.h"
#include "MediaDecoderStateMachine.h"
#include "MediaFormatReader.h"
#include "MediaPrefs.h"
#include "MediaShutdownManager.h"
#include "nsContentUtils.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"

using namespace mozilla::java;

namespace mozilla {

class HLSResourceCallbacksSupport
  : public GeckoHLSResourceWrapper::Callbacks::Natives<HLSResourceCallbacksSupport>
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(HLSResourceCallbacksSupport)
public:
  typedef GeckoHLSResourceWrapper::Callbacks::Natives<HLSResourceCallbacksSupport> NativeCallbacks;
  using NativeCallbacks::DisposeNative;
  using NativeCallbacks::AttachNative;

  HLSResourceCallbacksSupport(HLSDecoder* aResource);
  void Detach();
  void OnDataArrived();
  void OnError(int aErrorCode);

private:
  ~HLSResourceCallbacksSupport() {}
  Mutex mMutex;
  HLSDecoder* mDecoder;
};

HLSResourceCallbacksSupport::HLSResourceCallbacksSupport(HLSDecoder* aDecoder)
  : mMutex("HLSResourceCallbacksSupport")
  , mDecoder(aDecoder)
{
  MOZ_ASSERT(mDecoder);
}

void
HLSResourceCallbacksSupport::Detach()
{
  MutexAutoLock lock(mMutex);
  mDecoder = nullptr;
}

void
HLSResourceCallbacksSupport::OnDataArrived()
{
  HLS_DEBUG("HLSResourceCallbacksSupport", "OnDataArrived.");
  MutexAutoLock lock(mMutex);
  if (!mDecoder) { return; }
  RefPtr<HLSResourceCallbacksSupport> self = this;
  NS_DispatchToMainThread(
    NS_NewRunnableFunction(
      "HLSResourceCallbacksSupport::OnDataArrived",
      [self]() -> void {
        MutexAutoLock lock(self->mMutex);
        if (self->mDecoder) {
          self->mDecoder->NotifyDataArrived();
        }
      }
    )
  );
}

void
HLSResourceCallbacksSupport::OnError(int aErrorCode)
{
  HLS_DEBUG("HLSResourceCallbacksSupport", "onError(%d)", aErrorCode);
  MutexAutoLock lock(mMutex);
  if (!mDecoder) { return; }
  RefPtr<HLSResourceCallbacksSupport> self = this;
  NS_DispatchToMainThread(
    NS_NewRunnableFunction(
      "HLSResourceCallbacksSupport::OnDataArrived",
      [self]() -> void {
        MutexAutoLock lock(self->mMutex);
        if (self->mDecoder) {
          // Since HLS source should be from the Internet, we treat all resource errors
          // from GeckoHlsPlayer as network errors.
          self->mDecoder->NetworkError(
            MediaResult(NS_ERROR_FAILURE, "HLS error"));
        }
      }
    )
  );
}

HLSDecoder::HLSDecoder(MediaDecoderInit& aInit)
  : MediaDecoder(aInit)
{
}

MediaDecoderStateMachine*
HLSDecoder::CreateStateMachine()
{
  MOZ_ASSERT(NS_IsMainThread());

  MediaFormatReaderInit init;
  init.mVideoFrameContainer = GetVideoFrameContainer();
  init.mKnowsCompositor = GetCompositor();
  init.mCrashHelper = GetOwner()->CreateGMPCrashHelper();
  init.mFrameStats = mFrameStats;
  init.mMediaDecoderOwnerID = mOwner;
  mReader =
    new MediaFormatReader(init, new HLSDemuxer(mHLSResourceWrapper->GetPlayerId()));

  return new MediaDecoderStateMachine(this, mReader);
}

bool
HLSDecoder::IsEnabled()
{
  return MediaPrefs::HLSEnabled() && (jni::GetAPIVersion() >= 16);
}

bool
HLSDecoder::IsSupportedType(const MediaContainerType& aContainerType)
{
  return IsEnabled() &&
         DecoderTraits::IsHttpLiveStreamingType(aContainerType);
}

nsresult
HLSDecoder::Load(nsIChannel* aChannel)
{
  MOZ_ASSERT(NS_IsMainThread());
  AbstractThread::AutoEnter context(AbstractMainThread());

  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(mURI));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mChannel = aChannel;
  nsCString spec;
  Unused << mURI->GetSpec(spec);;
  HLSResourceCallbacksSupport::Init();
  mJavaCallbacks = GeckoHLSResourceWrapper::Callbacks::New();
  mCallbackSupport = new HLSResourceCallbacksSupport(this);
  HLSResourceCallbacksSupport::AttachNative(mJavaCallbacks, mCallbackSupport);
  mHLSResourceWrapper = java::GeckoHLSResourceWrapper::Create(NS_ConvertUTF8toUTF16(spec),
                                                              mJavaCallbacks);
  MOZ_ASSERT(mHLSResourceWrapper);

  rv = MediaShutdownManager::Instance().Register(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  SetStateMachine(CreateStateMachine());
  NS_ENSURE_TRUE(GetStateMachine(), NS_ERROR_FAILURE);

  GetStateMachine()->DispatchIsLiveStream(false);

  return InitializeStateMachine();
}

void
HLSDecoder::AddSizeOfResources(ResourceSizes* aSizes)
{
  MOZ_ASSERT(NS_IsMainThread());
  // TODO: track JAVA wrappers.
}

already_AddRefed<nsIPrincipal>
HLSDecoder::GetCurrentPrincipal()
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIPrincipal> principal;
  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  if (!secMan || !mChannel) {
    return nullptr;
  }
  secMan->GetChannelResultPrincipal(mChannel, getter_AddRefs(principal));
  return principal.forget();
}

nsresult
HLSDecoder::Play()
{
  MOZ_ASSERT(NS_IsMainThread());
  HLS_DEBUG("HLSDecoder", "MediaElement called Play");
  mHLSResourceWrapper->Play();
  return MediaDecoder::Play();
}

void
HLSDecoder::Pause()
{
  MOZ_ASSERT(NS_IsMainThread());
  HLS_DEBUG("HLSDecoder", "MediaElement called Pause");
  mHLSResourceWrapper->Pause();
  return MediaDecoder::Pause();
}

void
HLSDecoder::Suspend()
{
  MOZ_ASSERT(NS_IsMainThread());
  HLS_DEBUG("HLSDecoder", "Should suspend the resource fetching.");
  mHLSResourceWrapper->Suspend();
}

void
HLSDecoder::Resume()
{
  MOZ_ASSERT(NS_IsMainThread());
  HLS_DEBUG("HLSDecoder", "Should resume the resource fetching.");
  mHLSResourceWrapper->Resume();
}

void
HLSDecoder::Shutdown()
{
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

void
HLSDecoder::NotifyDataArrived()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!IsShutdown());
  AbstractThread::AutoEnter context(AbstractMainThread());
  NotifyReaderDataArrived();
  GetOwner()->DownloadProgressed();
}

} // namespace mozilla

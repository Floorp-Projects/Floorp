/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedDecoderManager.h"

namespace mozilla {

class SharedDecoderCallback : public MediaDataDecoderCallback
{
public:
  explicit SharedDecoderCallback(SharedDecoderManager* aManager)
    : mManager(aManager)
  {
  }

  virtual void Output(MediaData* aData) override
  {
    if (mManager->mActiveCallback) {
      AssertHaveActiveProxy();
      mManager->mActiveCallback->Output(aData);
    }
  }
  virtual void Error() override
  {
    if (mManager->mActiveCallback) {
      AssertHaveActiveProxy();
      mManager->mActiveCallback->Error();
    }
  }
  virtual void InputExhausted() override
  {
    if (mManager->mActiveCallback) {
      AssertHaveActiveProxy();
      mManager->mActiveCallback->InputExhausted();
    }
  }
  virtual void DrainComplete() override
  {
    if (mManager->mActiveCallback) {
      AssertHaveActiveProxy();
      mManager->DrainComplete();
    }
  }
  virtual void ReleaseMediaResources() override
  {
    if (mManager->mActiveCallback) {
      AssertHaveActiveProxy();
      mManager->mActiveCallback->ReleaseMediaResources();
    }
  }
  virtual bool OnReaderTaskQueue() override
  {
    MOZ_ASSERT(mManager->mActiveCallback);
    return mManager->mActiveCallback->OnReaderTaskQueue();
  }

private:
  void AssertHaveActiveProxy() {
#ifdef MOZ_FFMPEG // bug 1161895
    NS_WARN_IF_FALSE(mManager->mActiveProxy, "callback not active proxy");
#else
    MOZ_DIAGNOSTIC_ASSERT(mManager->mActiveProxy);
#endif
  }

  SharedDecoderManager* mManager;
};

SharedDecoderManager::SharedDecoderManager()
  : mTaskQueue(new FlushableTaskQueue(GetMediaThreadPool(MediaThreadType::PLATFORM_DECODER)))
  , mActiveProxy(nullptr)
  , mActiveCallback(nullptr)
  , mInit(false)
  , mWaitForInternalDrain(false)
  , mMonitor("SharedDecoderManager")
  , mDecoderReleasedResources(false)
{
  MOZ_ASSERT(NS_IsMainThread()); // taskqueue must be created on main thread.
  mCallback = new SharedDecoderCallback(this);
}

SharedDecoderManager::~SharedDecoderManager()
{
}

already_AddRefed<MediaDataDecoder>
SharedDecoderManager::CreateVideoDecoder(
  PlatformDecoderModule* aPDM,
  const VideoInfo& aConfig,
  layers::LayersBackend aLayersBackend,
  layers::ImageContainer* aImageContainer,
  FlushableTaskQueue* aVideoTaskQueue,
  MediaDataDecoderCallback* aCallback)
{
  if (!mDecoder) {
    mLayersBackend = aLayersBackend;
    mImageContainer = aImageContainer;
    // We use the manager's task queue for the decoder, rather than the one
    // passed in, so that none of the objects sharing the decoder can shutdown
    // the task queue while we're potentially still using it for a *different*
    // object also sharing the decoder.
    mDecoder =
      aPDM->CreateDecoder(aConfig,
                          mTaskQueue,
                          mCallback,
                          mLayersBackend,
                          mImageContainer);
    if (!mDecoder) {
      mPDM = nullptr;
      return nullptr;
    }

    mPDM = aPDM;
  }

  nsRefPtr<SharedDecoderProxy> proxy(new SharedDecoderProxy(this, aCallback));
  return proxy.forget();
}

void
SharedDecoderManager::DisableHardwareAcceleration()
{
  MOZ_ASSERT(mPDM);
  mPDM->DisableHardwareAcceleration();
}

bool
SharedDecoderManager::Recreate(const VideoInfo& aConfig)
{
  mDecoder->Flush();
  mDecoder->Shutdown();
  mDecoder = mPDM->CreateDecoder(aConfig,
                                 mTaskQueue,
                                 mCallback,
                                 mLayersBackend,
                                 mImageContainer);
  if (!mDecoder) {
    return false;
  }
  mInit = false;
  return true;
}

void
SharedDecoderManager::Select(SharedDecoderProxy* aProxy)
{
  if (mActiveProxy == aProxy) {
    return;
  }
  SetIdle(mActiveProxy);

  mActiveProxy = aProxy;
  mActiveCallback = aProxy->mCallback;
}

void
SharedDecoderManager::SetIdle(MediaDataDecoder* aProxy)
{
  if (aProxy && mActiveProxy == aProxy) {
    {
      MonitorAutoLock mon(mMonitor);
      mWaitForInternalDrain = true;
      // We don't want to hold the lock while calling Drain() as some
      // platform implementations call DrainComplete() immediately.
    }
    nsresult rv = mActiveProxy->Drain();
    if (NS_SUCCEEDED(rv)) {
      MonitorAutoLock mon(mMonitor);
      while (mWaitForInternalDrain) {
        mon.Wait();
      }
    }
    mActiveProxy->Flush();
    mActiveProxy = nullptr;
  }
}

nsRefPtr<MediaDataDecoder::InitPromise>
SharedDecoderManager::InitDecoder()
{
  if (!mInit && mDecoder) {
    MOZ_ASSERT(mCallback->OnReaderTaskQueue());

    nsRefPtr<SharedDecoderManager> self = this;
    nsRefPtr<MediaDataDecoder::InitPromise> p = mDecoderInitPromise.Ensure(__func__);

    mDecoderInitPromiseRequest.Begin(
      mDecoder->Init()->Then(AbstractThread::GetCurrent()->AsTaskQueue(), __func__,
        [self] (TrackInfo::TrackType aType) -> void {
          self->mDecoderInitPromiseRequest.Complete();
          self->mInit = true;
          self->mDecoderInitPromise.ResolveIfExists(aType, __func__);
        },
        [self] (MediaDataDecoder::DecoderFailureReason aReason) -> void {
          self->mDecoderInitPromiseRequest.Complete();
          self->mDecoderInitPromise.RejectIfExists(aReason, __func__);
        }));
    return p;
  }

  return MediaDataDecoder::InitPromise::CreateAndResolve(TrackInfo::kVideoTrack, __func__);
}

void
SharedDecoderManager::DrainComplete()
{
  {
    MonitorAutoLock mon(mMonitor);
    if (mWaitForInternalDrain) {
      mWaitForInternalDrain = false;
      mon.NotifyAll();
      return;
    }
  }
  mActiveCallback->DrainComplete();
}

void
SharedDecoderManager::Shutdown()
{
  // Shutdown() should have been called on any proxies.
  MOZ_ASSERT(!mActiveProxy);

  if (mDecoder) {
    mDecoder->Shutdown();
    mDecoder = nullptr;
  }
  mPDM = nullptr;
  if (mTaskQueue) {
    mTaskQueue->BeginShutdown();
    mTaskQueue->AwaitShutdownAndIdle();
    mTaskQueue = nullptr;
  }
  mDecoderInitPromiseRequest.DisconnectIfExists();
}

SharedDecoderProxy::SharedDecoderProxy(SharedDecoderManager* aManager,
                                       MediaDataDecoderCallback* aCallback)
  : mManager(aManager)
  , mCallback(aCallback)
{
}

SharedDecoderProxy::~SharedDecoderProxy()
{
  Shutdown();
}

nsRefPtr<MediaDataDecoder::InitPromise>
SharedDecoderProxy::Init()
{
  if (mManager->mActiveProxy != this) {
    mManager->Select(this);
  }

  return mManager->InitDecoder();
}

nsresult
SharedDecoderProxy::Input(MediaRawData* aSample)
{
  if (mManager->mActiveProxy != this) {
    mManager->Select(this);
  }
  return mManager->mDecoder->Input(aSample);
}

nsresult
SharedDecoderProxy::Flush()
{
  if (mManager->mActiveProxy == this) {
    return mManager->mDecoder->Flush();
  }
  return NS_OK;
}

nsresult
SharedDecoderProxy::Drain()
{
  if (mManager->mActiveProxy == this) {
    return mManager->mDecoder->Drain();
  } else {
    mCallback->DrainComplete();
    return NS_OK;
  }
}

nsresult
SharedDecoderProxy::Shutdown()
{
  mManager->SetIdle(this);
  return NS_OK;
}

bool
SharedDecoderProxy::IsHardwareAccelerated(nsACString& aFailureReason) const
{
  return mManager->mDecoder->IsHardwareAccelerated(aFailureReason);
}

} // namespace mozilla

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
  : mTaskQueue(new FlushableMediaTaskQueue(GetMediaThreadPool(MediaThreadType::PLATFORM_DECODER)))
  , mActiveProxy(nullptr)
  , mActiveCallback(nullptr)
  , mWaitForInternalDrain(false)
  , mMonitor("SharedDecoderProxy")
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
  FlushableMediaTaskQueue* aVideoTaskQueue,
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
    nsresult rv = mDecoder->Init();
    if (NS_FAILED(rv)) {
      mDecoder = nullptr;
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
  nsresult rv = mDecoder->Init();
  return rv == NS_OK;
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
    MonitorAutoLock mon(mMonitor);
    mWaitForInternalDrain = true;
    nsresult rv;
    {
      // We don't want to hold the lock while calling Drain() has some
      // platform implementations call DrainComplete() immediately.
      MonitorAutoUnlock mon(mMonitor);
      rv = mActiveProxy->Drain();
    }
    if (NS_SUCCEEDED(rv)) {
      while (mWaitForInternalDrain) {
        mon.Wait();
      }
    }
    mActiveProxy->Flush();
    mActiveProxy = nullptr;
  }
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

nsresult
SharedDecoderProxy::Init()
{
  return NS_OK;
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
SharedDecoderProxy::IsHardwareAccelerated() const
{
  return mManager->mDecoder->IsHardwareAccelerated();
}

}

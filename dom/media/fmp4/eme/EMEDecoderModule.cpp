/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EMEDecoderModule.h"
#include "EMEAudioDecoder.h"
#include "EMEVideoDecoder.h"
#include "MediaDataDecoderProxy.h"
#include "mozIGeckoMediaPluginService.h"
#include "mozilla/CDMProxy.h"
#include "mozilla/unused.h"
#include "nsServiceManagerUtils.h"
#include "MediaInfo.h"

namespace mozilla {

class EMEDecryptor : public MediaDataDecoder {

public:

  EMEDecryptor(MediaDataDecoder* aDecoder,
               MediaDataDecoderCallback* aCallback,
               CDMProxy* aProxy,
               MediaTaskQueue* aDecodeTaskQueue)
    : mDecoder(aDecoder)
    , mCallback(aCallback)
    , mTaskQueue(aDecodeTaskQueue)
    , mProxy(aProxy)
    , mSamplesWaitingForKey(new SamplesWaitingForKey(this, mTaskQueue, mProxy))
    , mIsShutdown(false)
  {
  }

  virtual nsresult Init() override {
    MOZ_ASSERT(!mIsShutdown);
    return mDecoder->Init();
  }

  class DeliverDecrypted : public DecryptionClient {
  public:
    explicit DeliverDecrypted(EMEDecryptor* aDecryptor)
      : mDecryptor(aDecryptor)
    { }
    virtual void Decrypted(GMPErr aResult, MediaRawData* aSample) override {
      mDecryptor->Decrypted(aResult, aSample);
      mDecryptor = nullptr;
    }
  private:
    nsRefPtr<EMEDecryptor> mDecryptor;
  };

  virtual nsresult Input(MediaRawData* aSample) override {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
    MOZ_ASSERT(!mIsShutdown);
    if (mSamplesWaitingForKey->WaitIfKeyNotUsable(aSample)) {
      return NS_OK;
    }

    nsAutoPtr<MediaRawDataWriter> writer(aSample->CreateWriter());
    mProxy->GetSessionIdsForKeyId(aSample->mCrypto.mKeyId,
                                  writer->mCrypto.mSessionIds);

    mProxy->Decrypt(aSample, new DeliverDecrypted(this), mTaskQueue);
    return NS_OK;
  }

  void Decrypted(GMPErr aResult, MediaRawData* aSample) {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
    if (mIsShutdown) {
      NS_WARNING("EME decrypted sample arrived after shutdown");
      return;
    }
    if (aResult == GMPNoKeyErr) {
      // Key became unusable after we sent the sample to CDM to decrypt.
      // Call Input() again, so that the sample is enqueued for decryption
      // if the key becomes usable again.
      Input(aSample);
    } else if (GMP_FAILED(aResult)) {
      if (mCallback) {
        mCallback->Error();
      }
      MOZ_ASSERT(!aSample);
    } else {
      MOZ_ASSERT(!mIsShutdown);
      nsresult rv = mDecoder->Input(aSample);
      unused << NS_WARN_IF(NS_FAILED(rv));
    }
  }

  virtual nsresult Flush() override {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
    MOZ_ASSERT(!mIsShutdown);
    nsresult rv = mDecoder->Flush();
    unused << NS_WARN_IF(NS_FAILED(rv));
    mSamplesWaitingForKey->Flush();
    return rv;
  }

  virtual nsresult Drain() override {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
    MOZ_ASSERT(!mIsShutdown);
    nsresult rv = mDecoder->Drain();
    unused << NS_WARN_IF(NS_FAILED(rv));
    return rv;
  }

  virtual nsresult Shutdown() override {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
    MOZ_ASSERT(!mIsShutdown);
    mIsShutdown = true;
    nsresult rv = mDecoder->Shutdown();
    unused << NS_WARN_IF(NS_FAILED(rv));
    mSamplesWaitingForKey->BreakCycles();
    mSamplesWaitingForKey = nullptr;
    mDecoder = nullptr;
    mProxy = nullptr;
    mCallback = nullptr;
    return rv;
  }

private:

  nsRefPtr<MediaDataDecoder> mDecoder;
  MediaDataDecoderCallback* mCallback;
  nsRefPtr<MediaTaskQueue> mTaskQueue;
  nsRefPtr<CDMProxy> mProxy;
  nsRefPtr<SamplesWaitingForKey> mSamplesWaitingForKey;
  bool mIsShutdown;
};

class EMEMediaDataDecoderProxy : public MediaDataDecoderProxy {
public:
  EMEMediaDataDecoderProxy(nsIThread* aProxyThread, MediaDataDecoderCallback* aCallback, CDMProxy* aProxy, FlushableMediaTaskQueue* aTaskQueue)
   : MediaDataDecoderProxy(aProxyThread, aCallback)
   , mSamplesWaitingForKey(new SamplesWaitingForKey(this, aTaskQueue, aProxy))
   , mProxy(aProxy)
  {
  }

  virtual nsresult Input(MediaRawData* aSample) override;
  virtual nsresult Shutdown() override;

private:
  nsRefPtr<SamplesWaitingForKey> mSamplesWaitingForKey;
  nsRefPtr<CDMProxy> mProxy;
};

nsresult
EMEMediaDataDecoderProxy::Input(MediaRawData* aSample)
{
  if (mSamplesWaitingForKey->WaitIfKeyNotUsable(aSample)) {
    return NS_OK;
  }

  nsAutoPtr<MediaRawDataWriter> writer(aSample->CreateWriter());
  mProxy->GetSessionIdsForKeyId(aSample->mCrypto.mKeyId,
                                writer->mCrypto.mSessionIds);

  return MediaDataDecoderProxy::Input(aSample);
}

nsresult
EMEMediaDataDecoderProxy::Shutdown()
{
  nsresult rv = MediaDataDecoderProxy::Shutdown();

  mSamplesWaitingForKey->BreakCycles();
  mSamplesWaitingForKey = nullptr;
  mProxy = nullptr;

  return rv;
}

EMEDecoderModule::EMEDecoderModule(CDMProxy* aProxy,
                                   PlatformDecoderModule* aPDM,
                                   bool aCDMDecodesAudio,
                                   bool aCDMDecodesVideo)
  : mProxy(aProxy)
  , mPDM(aPDM)
  , mCDMDecodesAudio(aCDMDecodesAudio)
  , mCDMDecodesVideo(aCDMDecodesVideo)
{
}

EMEDecoderModule::~EMEDecoderModule()
{
}

static already_AddRefed<MediaDataDecoderProxy>
CreateDecoderWrapper(MediaDataDecoderCallback* aCallback, CDMProxy* aProxy, FlushableMediaTaskQueue* aTaskQueue)
{
  nsCOMPtr<mozIGeckoMediaPluginService> gmpService = do_GetService("@mozilla.org/gecko-media-plugin-service;1");
  if (!gmpService) {
    return nullptr;
  }

  nsCOMPtr<nsIThread> thread;
  nsresult rv = gmpService->GetThread(getter_AddRefs(thread));
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  nsRefPtr<MediaDataDecoderProxy> decoder(new EMEMediaDataDecoderProxy(thread, aCallback, aProxy, aTaskQueue));
  return decoder.forget();
}

already_AddRefed<MediaDataDecoder>
EMEDecoderModule::CreateVideoDecoder(const VideoInfo& aConfig,
                                     layers::LayersBackend aLayersBackend,
                                     layers::ImageContainer* aImageContainer,
                                     FlushableMediaTaskQueue* aVideoTaskQueue,
                                     MediaDataDecoderCallback* aCallback)
{
  if (mCDMDecodesVideo && aConfig.mCrypto.mValid) {
    nsRefPtr<MediaDataDecoderProxy> wrapper = CreateDecoderWrapper(aCallback, mProxy, aVideoTaskQueue);
    wrapper->SetProxyTarget(new EMEVideoDecoder(mProxy,
                                                aConfig,
                                                aLayersBackend,
                                                aImageContainer,
                                                aVideoTaskQueue,
                                                wrapper->Callback()));
    return wrapper.forget();
  }

  nsRefPtr<MediaDataDecoder> decoder(
    mPDM->CreateDecoder(aConfig,
                        aVideoTaskQueue,
                        aCallback,
                        aLayersBackend,
                        aImageContainer));
  if (!decoder) {
    return nullptr;
  }

  if (!aConfig.mCrypto.mValid) {
    return decoder.forget();
  }

  nsRefPtr<MediaDataDecoder> emeDecoder(new EMEDecryptor(decoder,
                                                         aCallback,
                                                         mProxy,
                                                         MediaTaskQueue::GetCurrentQueue()));
  return emeDecoder.forget();
}

already_AddRefed<MediaDataDecoder>
EMEDecoderModule::CreateAudioDecoder(const AudioInfo& aConfig,
                                     FlushableMediaTaskQueue* aAudioTaskQueue,
                                     MediaDataDecoderCallback* aCallback)
{
  if (mCDMDecodesAudio && aConfig.mCrypto.mValid) {
    nsRefPtr<MediaDataDecoderProxy> wrapper = CreateDecoderWrapper(aCallback, mProxy, aAudioTaskQueue);
    wrapper->SetProxyTarget(new EMEAudioDecoder(mProxy,
                                                aConfig,
                                                aAudioTaskQueue,
                                                wrapper->Callback()));
    return wrapper.forget();
  }

  nsRefPtr<MediaDataDecoder> decoder(
    mPDM->CreateDecoder(aConfig, aAudioTaskQueue, aCallback));
  if (!decoder) {
    return nullptr;
  }

  if (!aConfig.mCrypto.mValid) {
    return decoder.forget();
  }

  nsRefPtr<MediaDataDecoder> emeDecoder(new EMEDecryptor(decoder,
                                                         aCallback,
                                                         mProxy,
                                                         MediaTaskQueue::GetCurrentQueue()));
  return emeDecoder.forget();
}

PlatformDecoderModule::ConversionRequired
EMEDecoderModule::DecoderNeedsConversion(const TrackInfo& aConfig) const
{
  if (aConfig.IsVideo()) {
    return kNeedAVCC;
  } else {
    return kNeedNone;
  }
}

} // namespace mozilla

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
#include "mp4_demuxer/DecoderData.h"

namespace mozilla {

class EMEDecryptor : public MediaDataDecoder {

public:

  EMEDecryptor(MediaDataDecoder* aDecoder,
               MediaDataDecoderCallback* aCallback,
               CDMProxy* aProxy)
    : mDecoder(aDecoder)
    , mCallback(aCallback)
    , mTaskQueue(CreateFlushableMediaDecodeTaskQueue())
    , mProxy(aProxy)
    , mSamplesWaitingForKey(new SamplesWaitingForKey(this, mTaskQueue, mProxy))
#ifdef DEBUG
    , mIsShutdown(false)
#endif
  {
  }

  virtual nsresult Init() override {
    MOZ_ASSERT(!mIsShutdown);
    nsresult rv = mTaskQueue->SyncDispatch(
      NS_NewRunnableMethod(mDecoder, &MediaDataDecoder::Init));
    unused << NS_WARN_IF(NS_FAILED(rv));
    return rv;
  }

  class DeliverDecrypted : public DecryptionClient {
  public:
    DeliverDecrypted(EMEDecryptor* aDecryptor, FlushableMediaTaskQueue* aTaskQueue)
      : mDecryptor(aDecryptor)
      , mTaskQueue(aTaskQueue)
    {}
    virtual void Decrypted(GMPErr aResult,
                           MediaRawData* aSample) override {
      if (aResult == GMPNoKeyErr) {
        RefPtr<nsIRunnable> task;
        task =  NS_NewRunnableMethodWithArg<nsRefPtr<MediaRawData>>(
          mDecryptor,
          &EMEDecryptor::Input,
          nsRefPtr<MediaRawData>(aSample));
        mTaskQueue->Dispatch(task.forget());
      } else if (GMP_FAILED(aResult)) {
        if (mDecryptor->mCallback) {
          mDecryptor->mCallback->Error();
        }
        MOZ_ASSERT(!aSample);
      } else {
        RefPtr<nsIRunnable> task;
        task = NS_NewRunnableMethodWithArg<nsRefPtr<MediaRawData>>(
          mDecryptor,
          &EMEDecryptor::Decrypted,
          nsRefPtr<MediaRawData>(aSample));
        mTaskQueue->Dispatch(task.forget());
      }
      mTaskQueue = nullptr;
      mDecryptor = nullptr;
    }
  private:
    nsRefPtr<EMEDecryptor> mDecryptor;
    nsRefPtr<FlushableMediaTaskQueue> mTaskQueue;
  };

  virtual nsresult Input(MediaRawData* aSample) override {
    MOZ_ASSERT(!mIsShutdown);
    // We run the PDM on its own task queue. We can't run it on the decode
    // task queue, because that calls into Input() in a loop and waits until
    // output is delivered. We need to defer some Input() calls while we wait
    // for keys to become usable, and once they do we need to dispatch an event
    // to run the PDM on the same task queue, but since the decode task queue
    // is waiting in MP4Reader::Decode() for output our task would never run.
    // So we dispatch tasks to make all calls into the wrapped decoder.
    if (mSamplesWaitingForKey->WaitIfKeyNotUsable(aSample)) {
      return NS_OK;
    }

    nsAutoPtr<MediaRawDataWriter> writer(aSample->CreateWriter());
    mProxy->GetSessionIdsForKeyId(aSample->mCrypto.mKeyId,
                                  writer->mCrypto.mSessionIds);

    mProxy->Decrypt(aSample, new DeliverDecrypted(this, mTaskQueue));
    return NS_OK;
  }

  void Decrypted(MediaRawData* aSample) {
    MOZ_ASSERT(!mIsShutdown);
    nsresult rv = mTaskQueue->Dispatch(
      NS_NewRunnableMethodWithArg<nsRefPtr<MediaRawData>>(
        mDecoder,
        &MediaDataDecoder::Input,
        nsRefPtr<MediaRawData>(aSample)));
    unused << NS_WARN_IF(NS_FAILED(rv));
  }

  virtual nsresult Flush() override {
    MOZ_ASSERT(!mIsShutdown);
    nsresult rv = mTaskQueue->SyncDispatch(
      NS_NewRunnableMethod(
        mDecoder,
        &MediaDataDecoder::Flush));
    unused << NS_WARN_IF(NS_FAILED(rv));
    mSamplesWaitingForKey->Flush();
    return rv;
  }

  virtual nsresult Drain() override {
    MOZ_ASSERT(!mIsShutdown);
    nsresult rv = mTaskQueue->Dispatch(
      NS_NewRunnableMethod(
        mDecoder,
        &MediaDataDecoder::Drain));
    unused << NS_WARN_IF(NS_FAILED(rv));
    return rv;
  }

  virtual nsresult Shutdown() override {
    MOZ_ASSERT(!mIsShutdown);
#ifdef DEBUG
    mIsShutdown = true;
#endif
    nsresult rv = mTaskQueue->SyncDispatch(
      NS_NewRunnableMethod(
        mDecoder,
        &MediaDataDecoder::Shutdown));
    unused << NS_WARN_IF(NS_FAILED(rv));
    mSamplesWaitingForKey->BreakCycles();
    mSamplesWaitingForKey = nullptr;
    mDecoder = nullptr;
    mTaskQueue->BeginShutdown();
    mTaskQueue->AwaitShutdownAndIdle();
    mTaskQueue = nullptr;
    mProxy = nullptr;
    mCallback = nullptr;
    return rv;
  }

private:

  nsRefPtr<MediaDataDecoder> mDecoder;
  MediaDataDecoderCallback* mCallback;
  nsRefPtr<FlushableMediaTaskQueue> mTaskQueue;
  nsRefPtr<CDMProxy> mProxy;
  nsRefPtr<SamplesWaitingForKey> mSamplesWaitingForKey;
#ifdef DEBUG
  bool mIsShutdown;
#endif
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
EMEDecoderModule::CreateVideoDecoder(const VideoDecoderConfig& aConfig,
                                     layers::LayersBackend aLayersBackend,
                                     layers::ImageContainer* aImageContainer,
                                     FlushableMediaTaskQueue* aVideoTaskQueue,
                                     MediaDataDecoderCallback* aCallback)
{
  if (mCDMDecodesVideo && aConfig.crypto.mValid) {
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

  if (!aConfig.crypto.mValid) {
    return decoder.forget();
  }

  nsRefPtr<MediaDataDecoder> emeDecoder(new EMEDecryptor(decoder,
                                                         aCallback,
                                                         mProxy));
  return emeDecoder.forget();
}

already_AddRefed<MediaDataDecoder>
EMEDecoderModule::CreateAudioDecoder(const AudioDecoderConfig& aConfig,
                                     FlushableMediaTaskQueue* aAudioTaskQueue,
                                     MediaDataDecoderCallback* aCallback)
{
  if (mCDMDecodesAudio && aConfig.crypto.mValid) {
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

  if (!aConfig.crypto.mValid) {
    return decoder.forget();
  }

  nsRefPtr<MediaDataDecoder> emeDecoder(new EMEDecryptor(decoder,
                                                         aCallback,
                                                         mProxy));
  return emeDecoder.forget();
}

PlatformDecoderModule::ConversionRequired
EMEDecoderModule::DecoderNeedsConversion(const mp4_demuxer::TrackConfig& aConfig) const
{
  if (aConfig.IsVideoConfig()) {
    return kNeedAVCC;
  } else {
    return kNeedNone;
  }
}

} // namespace mozilla

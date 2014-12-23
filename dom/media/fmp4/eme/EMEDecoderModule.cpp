/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EMEDecoderModule.h"
#include "mozIGeckoMediaPluginService.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "ImageContainer.h"
#include "prsystem.h"
#include "mp4_demuxer/DecoderData.h"
#include "gfx2DGlue.h"
#include "nsContentUtils.h"
#include "mozilla/CDMProxy.h"
#include "mozilla/EMELog.h"
#include "MediaTaskQueue.h"
#include "SharedThreadPool.h"
#include "mozilla/EMELog.h"
#include "EMEH264Decoder.h"
#include "EMEAudioDecoder.h"
#include "mozilla/unused.h"
#include "SamplesWaitingForKey.h"
#include <string>

namespace mozilla {

class EMEDecryptor : public MediaDataDecoder {
  typedef mp4_demuxer::MP4Sample MP4Sample;

public:

  EMEDecryptor(MediaDataDecoder* aDecoder,
               MediaDataDecoderCallback* aCallback,
               CDMProxy* aProxy)
    : mDecoder(aDecoder)
    , mCallback(aCallback)
    , mTaskQueue(CreateMediaDecodeTaskQueue())
    , mProxy(aProxy)
    , mSamplesWaitingForKey(new SamplesWaitingForKey(this, mTaskQueue, mProxy))
#ifdef DEBUG
    , mIsShutdown(false)
#endif
  {
  }

  virtual nsresult Init() MOZ_OVERRIDE {
    MOZ_ASSERT(!mIsShutdown);
    nsresult rv = mTaskQueue->SyncDispatch(
      NS_NewRunnableMethod(mDecoder, &MediaDataDecoder::Init));
    unused << NS_WARN_IF(NS_FAILED(rv));
    return rv;
  }

  class DeliverDecrypted : public DecryptionClient {
  public:
    DeliverDecrypted(EMEDecryptor* aDecryptor, MediaTaskQueue* aTaskQueue)
      : mDecryptor(aDecryptor)
      , mTaskQueue(aTaskQueue)
    {}
    virtual void Decrypted(GMPErr aResult,
                           mp4_demuxer::MP4Sample* aSample) MOZ_OVERRIDE {
      if (aResult == GMPNoKeyErr) {
        RefPtr<nsIRunnable> task;
        task = NS_NewRunnableMethodWithArg<MP4Sample*>(mDecryptor,
                                                       &EMEDecryptor::Input,
                                                       aSample);
        mTaskQueue->Dispatch(task.forget());
      } else if (GMP_FAILED(aResult)) {
        if (mDecryptor->mCallback) {
          mDecryptor->mCallback->Error();
        }
        MOZ_ASSERT(!aSample);
      } else {
        RefPtr<nsIRunnable> task;
        task = NS_NewRunnableMethodWithArg<MP4Sample*>(mDecryptor,
                                                       &EMEDecryptor::Decrypted,
                                                       aSample);
        mTaskQueue->Dispatch(task.forget());
      }
      mTaskQueue = nullptr;
      mDecryptor = nullptr;
    }
  private:
    nsRefPtr<EMEDecryptor> mDecryptor;
    nsRefPtr<MediaTaskQueue> mTaskQueue;
  };

  virtual nsresult Input(MP4Sample* aSample) MOZ_OVERRIDE {
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

    mProxy->Decrypt(aSample, new DeliverDecrypted(this, mTaskQueue));
    return NS_OK;
  }

  void Decrypted(mp4_demuxer::MP4Sample* aSample) {
    MOZ_ASSERT(!mIsShutdown);
    nsresult rv = mTaskQueue->Dispatch(
      NS_NewRunnableMethodWithArg<mp4_demuxer::MP4Sample*>(
        mDecoder,
        &MediaDataDecoder::Input,
        aSample));
    unused << NS_WARN_IF(NS_FAILED(rv));
  }

  virtual nsresult Flush() MOZ_OVERRIDE {
    MOZ_ASSERT(!mIsShutdown);
    nsresult rv = mTaskQueue->SyncDispatch(
      NS_NewRunnableMethod(
        mDecoder,
        &MediaDataDecoder::Flush));
    unused << NS_WARN_IF(NS_FAILED(rv));
    mSamplesWaitingForKey->Flush();
    return rv;
  }

  virtual nsresult Drain() MOZ_OVERRIDE {
    MOZ_ASSERT(!mIsShutdown);
    nsresult rv = mTaskQueue->Dispatch(
      NS_NewRunnableMethod(
        mDecoder,
        &MediaDataDecoder::Drain));
    unused << NS_WARN_IF(NS_FAILED(rv));
    return rv;
  }

  virtual nsresult Shutdown() MOZ_OVERRIDE {
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
  nsRefPtr<MediaTaskQueue> mTaskQueue;
  nsRefPtr<CDMProxy> mProxy;
  nsRefPtr<SamplesWaitingForKey> mSamplesWaitingForKey;
#ifdef DEBUG
  bool mIsShutdown;
#endif
};

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

nsresult
EMEDecoderModule::Shutdown()
{
  if (mPDM) {
    return mPDM->Shutdown();
  }
  return NS_OK;
}

already_AddRefed<MediaDataDecoder>
EMEDecoderModule::CreateVideoDecoder(const VideoDecoderConfig& aConfig,
                                     layers::LayersBackend aLayersBackend,
                                     layers::ImageContainer* aImageContainer,
                                     MediaTaskQueue* aVideoTaskQueue,
                                     MediaDataDecoderCallback* aCallback)
{
  if (mCDMDecodesVideo && aConfig.crypto.valid) {
    nsRefPtr<MediaDataDecoder> decoder(new EMEH264Decoder(mProxy,
                                                          aConfig,
                                                          aLayersBackend,
                                                          aImageContainer,
                                                          aVideoTaskQueue,
                                                          aCallback));
    return decoder.forget();
  }

  nsRefPtr<MediaDataDecoder> decoder(mPDM->CreateVideoDecoder(aConfig,
                                                              aLayersBackend,
                                                              aImageContainer,
                                                              aVideoTaskQueue,
                                                              aCallback));
  if (!decoder) {
    return nullptr;
  }

  if (!aConfig.crypto.valid) {
    return decoder.forget();
  }

  nsRefPtr<MediaDataDecoder> emeDecoder(new EMEDecryptor(decoder,
                                                         aCallback,
                                                         mProxy));
  return emeDecoder.forget();
}

already_AddRefed<MediaDataDecoder>
EMEDecoderModule::CreateAudioDecoder(const AudioDecoderConfig& aConfig,
                                     MediaTaskQueue* aAudioTaskQueue,
                                     MediaDataDecoderCallback* aCallback)
{
  if (mCDMDecodesAudio && aConfig.crypto.valid) {
    nsRefPtr<MediaDataDecoder> decoder(new EMEAudioDecoder(mProxy,
                                                           aConfig,
                                                           aAudioTaskQueue,
                                                           aCallback));
    return decoder.forget();
  }

  nsRefPtr<MediaDataDecoder> decoder(mPDM->CreateAudioDecoder(aConfig,
                                                              aAudioTaskQueue,
                                                              aCallback));
  if (!decoder) {
    return nullptr;
  }

  if (!aConfig.crypto.valid) {
    return decoder.forget();
  }

  nsRefPtr<MediaDataDecoder> emeDecoder(new EMEDecryptor(decoder,
                                                         aCallback,
                                                         mProxy));
  return emeDecoder.forget();
}

bool
EMEDecoderModule::DecoderNeedsAVCC(const mp4_demuxer::VideoDecoderConfig& aConfig)
{
  return mCDMDecodesVideo && aConfig.crypto.valid;
}

} // namespace mozilla

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
#include <string>

namespace mozilla {

class EMEDecryptor : public MediaDataDecoder {
  typedef mp4_demuxer::MP4Sample MP4Sample;

public:

  EMEDecryptor(MediaDataDecoder* aDecoder,
               MediaDataDecoderCallback* aCallback,
               MediaTaskQueue* aTaskQueue,
               CDMProxy* aProxy)
    : mDecoder(aDecoder)
    , mCallback(aCallback)
    , mTaskQueue(aTaskQueue)
    , mProxy(aProxy)
  {
  }

  virtual nsresult Init() MOZ_OVERRIDE {
    return mTaskQueue->SyncDispatch(
      NS_NewRunnableMethod(mDecoder,
      &MediaDataDecoder::Init));
  }

  class RedeliverEncryptedInput : public nsRunnable {
  public:
    RedeliverEncryptedInput(EMEDecryptor* aDecryptor,
                            MediaTaskQueue* aTaskQueue,
                            MP4Sample* aSample)
      : mDecryptor(aDecryptor)
      , mTaskQueue(aTaskQueue)
      , mSample(aSample)
    {}

    NS_IMETHOD Run() {
      RefPtr<nsIRunnable> task;
      task = NS_NewRunnableMethodWithArg<MP4Sample*>(mDecryptor,
                                                     &EMEDecryptor::Input,
                                                     mSample.forget());
      mTaskQueue->Dispatch(task.forget());
      mTaskQueue = nullptr;
      mDecryptor = nullptr;
      return NS_OK;
    }

  private:
    nsRefPtr<EMEDecryptor> mDecryptor;
    nsRefPtr<MediaTaskQueue> mTaskQueue;
    nsAutoPtr<MP4Sample> mSample;
  };

  class DeliverDecrypted : public DecryptionClient {
  public:
    DeliverDecrypted(EMEDecryptor* aDecryptor, MediaTaskQueue* aTaskQueue)
      : mDecryptor(aDecryptor)
      , mTaskQueue(aTaskQueue)
    {}
    virtual void Decrypted(nsresult aResult,
                           mp4_demuxer::MP4Sample* aSample) MOZ_OVERRIDE {
      if (NS_FAILED(aResult)) {
        mDecryptor->mCallback->Error();
        delete aSample;
      } else {
        RefPtr<nsIRunnable> task;
        task = NS_NewRunnableMethodWithArg<MP4Sample*>(mDecryptor,
                                                       &EMEDecryptor::Decrypted,
                                                       aSample);
        mTaskQueue->Dispatch(task.forget());
        mTaskQueue = nullptr;
        mDecryptor = nullptr;
      }
    }
  private:
    nsRefPtr<EMEDecryptor> mDecryptor;
    nsRefPtr<MediaTaskQueue> mTaskQueue;
  };

  virtual nsresult Input(MP4Sample* aSample) MOZ_OVERRIDE {
    // We run the PDM on its own task queue. We can't run it on the decode
    // task queue, because that calls into Input() in a loop and waits until
    // output is delivered. We need to defer some Input() calls while we wait
    // for keys to become usable, and once they do we need to dispatch an event
    // to run the PDM on the same task queue, but since the decode task queue
    // is waiting in MP4Reader::Decode() for output our task would never run.
    // So we dispatch tasks to make all calls into the wrapped decoder.
    {
      CDMCaps::AutoLock caps(mProxy->Capabilites());
      if (!caps.IsKeyUsable(aSample->crypto.key)) {
        EME_LOG("Encountered a non-usable key, waiting");
        nsRefPtr<nsIRunnable> task(new RedeliverEncryptedInput(this,
                                                               mTaskQueue,
                                                               aSample));
        caps.CallWhenKeyUsable(aSample->crypto.key, task);
        return NS_OK;
      }
    }
    mProxy->Decrypt(aSample, new DeliverDecrypted(this, mTaskQueue));
    return NS_OK;
  }

  void Decrypted(mp4_demuxer::MP4Sample* aSample) {
    mTaskQueue->Dispatch(
      NS_NewRunnableMethodWithArg<mp4_demuxer::MP4Sample*>(
        mDecoder,
        &MediaDataDecoder::Input,
        aSample));
  }

  virtual nsresult Flush() MOZ_OVERRIDE {
    mTaskQueue->SyncDispatch(
      NS_NewRunnableMethod(
        mDecoder,
        &MediaDataDecoder::Flush));
    return NS_OK;
  }

  virtual nsresult Drain() MOZ_OVERRIDE {
    mTaskQueue->Dispatch(
      NS_NewRunnableMethod(
        mDecoder,
        &MediaDataDecoder::Drain));
    return NS_OK;
  }

  virtual nsresult Shutdown() MOZ_OVERRIDE {
    mTaskQueue->SyncDispatch(
      NS_NewRunnableMethod(
        mDecoder,
        &MediaDataDecoder::Shutdown));
    mDecoder = nullptr;
    mTaskQueue->Shutdown();
    mTaskQueue = nullptr;
    mProxy = nullptr;
    return NS_OK;
  }

private:

  nsRefPtr<MediaDataDecoder> mDecoder;
  MediaDataDecoderCallback* mCallback;
  nsRefPtr<MediaTaskQueue> mTaskQueue;
  nsRefPtr<CDMProxy> mProxy;
};

EMEDecoderModule::EMEDecoderModule(CDMProxy* aProxy,
                                   PlatformDecoderModule* aPDM,
                                   bool aCDMDecodesAudio,
                                   bool aCDMDecodesVideo,
                                   already_AddRefed<MediaTaskQueue> aTaskQueue)
  : mProxy(aProxy)
  , mPDM(aPDM)
  , mTaskQueue(aTaskQueue)
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
  mTaskQueue->Shutdown();
  return NS_OK;
}

already_AddRefed<MediaDataDecoder>
EMEDecoderModule::CreateH264Decoder(const VideoDecoderConfig& aConfig,
                                    layers::LayersBackend aLayersBackend,
                                    layers::ImageContainer* aImageContainer,
                                    MediaTaskQueue* aVideoTaskQueue,
                                    MediaDataDecoderCallback* aCallback)
{
  if (mCDMDecodesVideo) {
    nsRefPtr<MediaDataDecoder> decoder(new EMEH264Decoder(mProxy,
                                                          aConfig,
                                                          aLayersBackend,
                                                          aImageContainer,
                                                          aVideoTaskQueue,
                                                          aCallback));
    return decoder.forget();
  }

  nsRefPtr<MediaDataDecoder> decoder(mPDM->CreateH264Decoder(aConfig,
                                                             aLayersBackend,
                                                             aImageContainer,
                                                             aVideoTaskQueue,
                                                             aCallback));
  if (!decoder) {
    return nullptr;
  }

  nsRefPtr<MediaDataDecoder> emeDecoder(new EMEDecryptor(decoder,
                                                         aCallback,
                                                         mTaskQueue,
                                                         mProxy));
  return emeDecoder.forget();
}

already_AddRefed<MediaDataDecoder>
EMEDecoderModule::CreateAudioDecoder(const AudioDecoderConfig& aConfig,
                                     MediaTaskQueue* aAudioTaskQueue,
                                     MediaDataDecoderCallback* aCallback)
{
  if (mCDMDecodesAudio) {
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

  nsRefPtr<MediaDataDecoder> emeDecoder(new EMEDecryptor(decoder,
                                                         aCallback,
                                                         mTaskQueue,
                                                         mProxy));
  return emeDecoder.forget();
}

} // namespace mozilla

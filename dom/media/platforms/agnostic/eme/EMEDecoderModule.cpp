/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EMEDecoderModule.h"
#include "EMEVideoDecoder.h"
#include "GMPDecoderModule.h"
#include "GMPService.h"
#include "MP4Decoder.h"
#include "MediaInfo.h"
#include "MediaPrefs.h"
#include "PDMFactory.h"
#include "gmp-decryption.h"
#include "mozIGeckoMediaPluginService.h"
#include "mozilla/CDMProxy.h"
#include "mozilla/EMEUtils.h"
#include "mozilla/Unused.h"
#include "nsAutoPtr.h"
#include "nsClassHashtable.h"
#include "nsServiceManagerUtils.h"
#include "DecryptThroughputLimit.h"

namespace mozilla {

typedef MozPromiseRequestHolder<DecryptPromise> DecryptPromiseRequestHolder;
extern already_AddRefed<PlatformDecoderModule> CreateBlankDecoderModule();

class EMEDecryptor : public MediaDataDecoder
{
public:
  EMEDecryptor(MediaDataDecoder* aDecoder, CDMProxy* aProxy,
               TaskQueue* aDecodeTaskQueue, TrackInfo::TrackType aType,
               MediaEventProducer<TrackInfo::TrackType>* aOnWaitingForKey)
    : mDecoder(aDecoder)
    , mTaskQueue(aDecodeTaskQueue)
    , mProxy(aProxy)
    , mSamplesWaitingForKey(
        new SamplesWaitingForKey(mProxy, aType, aOnWaitingForKey))
    , mThroughputLimiter(aDecodeTaskQueue)
    , mIsShutdown(false)
  {
  }

  RefPtr<InitPromise> Init() override
  {
    MOZ_ASSERT(!mIsShutdown);
    return mDecoder->Init();
  }

  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override
  {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
    MOZ_RELEASE_ASSERT(mDecrypts.Count() == 0,
                       "Can only process one sample at a time");
    RefPtr<DecodePromise> p = mDecodePromise.Ensure(__func__);

    RefPtr<EMEDecryptor> self = this;
    mSamplesWaitingForKey->WaitIfKeyNotUsable(aSample)
      ->Then(mTaskQueue, __func__,
             [self, this](RefPtr<MediaRawData> aSample) {
               mKeyRequest.Complete();
               ThrottleDecode(aSample);
             },
             [self, this]() {
               mKeyRequest.Complete();
             })
      ->Track(mKeyRequest);

    return p;
  }

  void ThrottleDecode(MediaRawData* aSample)
  {
    RefPtr<EMEDecryptor> self = this;
    mThroughputLimiter.Throttle(aSample)
      ->Then(mTaskQueue, __func__,
             [self, this] (RefPtr<MediaRawData> aSample) {
               mThrottleRequest.Complete();
               AttemptDecode(aSample);
             },
             [self, this]() {
                mThrottleRequest.Complete();
             })
      ->Track(mThrottleRequest);
  }

  void AttemptDecode(MediaRawData* aSample)
  {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
    if (mIsShutdown) {
      NS_WARNING("EME encrypted sample arrived after shutdown");
      mDecodePromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
      return;
    }

    nsAutoPtr<MediaRawDataWriter> writer(aSample->CreateWriter());
    mProxy->GetSessionIdsForKeyId(aSample->mCrypto.mKeyId,
                                  writer->mCrypto.mSessionIds);

    mDecrypts.Put(aSample, new DecryptPromiseRequestHolder());
    mProxy->Decrypt(aSample)
      ->Then(mTaskQueue, __func__, this,
            &EMEDecryptor::Decrypted,
            &EMEDecryptor::Decrypted)
      ->Track(*mDecrypts.Get(aSample));
  }

  void Decrypted(const DecryptResult& aDecrypted)
  {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
    MOZ_ASSERT(aDecrypted.mSample);

    nsAutoPtr<DecryptPromiseRequestHolder> holder;
    mDecrypts.Remove(aDecrypted.mSample, &holder);
    if (holder) {
      holder->Complete();
    } else {
      // Decryption is not in the list of decrypt operations waiting
      // for a result. It must have been flushed or drained. Ignore result.
      return;
    }

    if (mIsShutdown) {
      NS_WARNING("EME decrypted sample arrived after shutdown");
      return;
    }

    if (aDecrypted.mStatus == eme::NoKeyErr) {
      // Key became unusable after we sent the sample to CDM to decrypt.
      // Call Decode() again, so that the sample is enqueued for decryption
      // if the key becomes usable again.
      AttemptDecode(aDecrypted.mSample);
    } else if (aDecrypted.mStatus != eme::Ok) {
      mDecodePromise.RejectIfExists(
        MediaResult(
          NS_ERROR_DOM_MEDIA_FATAL_ERR,
          RESULT_DETAIL("decrypted.mStatus=%u", uint32_t(aDecrypted.mStatus))),
        __func__);
    } else {
      MOZ_ASSERT(!mIsShutdown);
      // The sample is no longer encrypted, so clear its crypto metadata.
      UniquePtr<MediaRawDataWriter> writer(aDecrypted.mSample->CreateWriter());
      writer->mCrypto = CryptoSample();
      RefPtr<EMEDecryptor> self = this;
      mDecoder->Decode(aDecrypted.mSample)
        ->Then(mTaskQueue, __func__,
               [self, this](const DecodedData& aResults) {
                 mDecodeRequest.Complete();
                 mDecodePromise.ResolveIfExists(aResults, __func__);
               },
               [self, this](const MediaResult& aError) {
                 mDecodeRequest.Complete();
                 mDecodePromise.RejectIfExists(aError, __func__);
               })
        ->Track(mDecodeRequest);
    }
  }

  RefPtr<FlushPromise> Flush() override
  {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
    MOZ_ASSERT(!mIsShutdown);
    mKeyRequest.DisconnectIfExists();
    mThrottleRequest.DisconnectIfExists();
    mDecodeRequest.DisconnectIfExists();
    mDecodePromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
    mThroughputLimiter.Flush();
    for (auto iter = mDecrypts.Iter(); !iter.Done(); iter.Next()) {
      nsAutoPtr<DecryptPromiseRequestHolder>& holder = iter.Data();
      holder->DisconnectIfExists();
      iter.Remove();
    }
    RefPtr<SamplesWaitingForKey> k = mSamplesWaitingForKey;
    return mDecoder->Flush()->Then(
      mTaskQueue, __func__,
      [k]() {
        k->Flush();
        return FlushPromise::CreateAndResolve(true, __func__);
      });
  }

  RefPtr<DecodePromise> Drain() override
  {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
    MOZ_ASSERT(!mIsShutdown);
    MOZ_ASSERT(mDecodePromise.IsEmpty() && !mDecodeRequest.Exists(),
               "Must wait for decoding to complete");
    for (auto iter = mDecrypts.Iter(); !iter.Done(); iter.Next()) {
      nsAutoPtr<DecryptPromiseRequestHolder>& holder = iter.Data();
      holder->DisconnectIfExists();
      iter.Remove();
    }
    return mDecoder->Drain();
  }

  RefPtr<ShutdownPromise> Shutdown() override
  {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
    MOZ_ASSERT(!mIsShutdown);
    mIsShutdown = true;
    mSamplesWaitingForKey = nullptr;
    RefPtr<MediaDataDecoder> decoder = mDecoder.forget();
    mProxy = nullptr;
    return decoder->Shutdown();
  }

  const char* GetDescriptionName() const override
  {
    return mDecoder->GetDescriptionName();
  }

  ConversionRequired NeedsConversion() const override
  {
    return mDecoder->NeedsConversion();
  }

private:
  RefPtr<MediaDataDecoder> mDecoder;
  RefPtr<TaskQueue> mTaskQueue;
  RefPtr<CDMProxy> mProxy;
  nsClassHashtable<nsRefPtrHashKey<MediaRawData>, DecryptPromiseRequestHolder>
    mDecrypts;
  RefPtr<SamplesWaitingForKey> mSamplesWaitingForKey;
  MozPromiseRequestHolder<SamplesWaitingForKey::WaitForKeyPromise> mKeyRequest;
  DecryptThroughputLimit mThroughputLimiter;
  MozPromiseRequestHolder<DecryptThroughputLimit::ThrottlePromise> mThrottleRequest;
  MozPromiseHolder<DecodePromise> mDecodePromise;
  MozPromiseHolder<DecodePromise> mDrainPromise;
  MozPromiseHolder<FlushPromise> mFlushPromise;
  MozPromiseRequestHolder<DecodePromise> mDecodeRequest;

  bool mIsShutdown;
};

EMEMediaDataDecoderProxy::EMEMediaDataDecoderProxy(
  already_AddRefed<AbstractThread> aProxyThread,
  CDMProxy* aProxy,
  const CreateDecoderParams& aParams)
  : MediaDataDecoderProxy(Move(aProxyThread))
  , mTaskQueue(AbstractThread::GetCurrent()->AsTaskQueue())
  , mSamplesWaitingForKey(
      new SamplesWaitingForKey(aProxy,
                               aParams.mType,
                               aParams.mOnWaitingForKeyEvent))
  , mProxy(aProxy)
{
}

EMEMediaDataDecoderProxy::EMEMediaDataDecoderProxy(
  const CreateDecoderParams& aParams,
  already_AddRefed<MediaDataDecoder> aProxyDecoder,
  CDMProxy* aProxy)
  : MediaDataDecoderProxy(Move(aProxyDecoder))
  , mTaskQueue(AbstractThread::GetCurrent()->AsTaskQueue())
  , mSamplesWaitingForKey(
      new SamplesWaitingForKey(aProxy,
                               aParams.mType,
                               aParams.mOnWaitingForKeyEvent))
  , mProxy(aProxy)
{
}

RefPtr<MediaDataDecoder::DecodePromise>
EMEMediaDataDecoderProxy::Decode(MediaRawData* aSample)
{
  RefPtr<DecodePromise> p = mDecodePromise.Ensure(__func__);

  RefPtr<EMEMediaDataDecoderProxy> self = this;
  mSamplesWaitingForKey->WaitIfKeyNotUsable(aSample)
    ->Then(mTaskQueue, __func__,
           [self, this](RefPtr<MediaRawData> aSample) {
             mKeyRequest.Complete();

             nsAutoPtr<MediaRawDataWriter> writer(aSample->CreateWriter());
             mProxy->GetSessionIdsForKeyId(aSample->mCrypto.mKeyId,
                                           writer->mCrypto.mSessionIds);
             MediaDataDecoderProxy::Decode(aSample)
               ->Then(mTaskQueue, __func__,
                      [self, this](const DecodedData& aResults) {
                        mDecodeRequest.Complete();
                        mDecodePromise.Resolve(aResults, __func__);
                      },
                      [self, this](const MediaResult& aError) {
                        mDecodeRequest.Complete();
                        mDecodePromise.Reject(aError, __func__);
                      })
               ->Track(mDecodeRequest);
           },
           [self, this]() {
             mKeyRequest.Complete();
             MOZ_CRASH("Should never get here");
           })
    ->Track(mKeyRequest);

  return p;
}

RefPtr<MediaDataDecoder::FlushPromise>
EMEMediaDataDecoderProxy::Flush()
{
  mKeyRequest.DisconnectIfExists();
  mDecodeRequest.DisconnectIfExists();
  mDecodePromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  return MediaDataDecoderProxy::Flush();
}

RefPtr<ShutdownPromise>
EMEMediaDataDecoderProxy::Shutdown()
{
  mSamplesWaitingForKey = nullptr;
  mProxy = nullptr;
  return MediaDataDecoderProxy::Shutdown();
}

EMEDecoderModule::EMEDecoderModule(CDMProxy* aProxy, PDMFactory* aPDM)
  : mProxy(aProxy)
  , mPDM(aPDM)
{
}

EMEDecoderModule::~EMEDecoderModule()
{
}

static already_AddRefed<MediaDataDecoderProxy>
CreateDecoderWrapper(CDMProxy* aProxy, const CreateDecoderParams& aParams)
{
  RefPtr<gmp::GeckoMediaPluginService> s(
    gmp::GeckoMediaPluginService::GetGeckoMediaPluginService());
  if (!s) {
    return nullptr;
  }
  RefPtr<AbstractThread> thread(s->GetAbstractGMPThread());
  if (!thread) {
    return nullptr;
  }
  RefPtr<MediaDataDecoderProxy> decoder(new EMEMediaDataDecoderProxy(
    thread.forget(), aProxy, aParams));
  return decoder.forget();
}

already_AddRefed<MediaDataDecoder>
EMEDecoderModule::CreateVideoDecoder(const CreateDecoderParams& aParams)
{
  MOZ_ASSERT(aParams.mConfig.mCrypto.mValid);

  if (MediaPrefs::EMEBlankVideo()) {
    EME_LOG("EMEDecoderModule::CreateVideoDecoder() creating a blank decoder.");
    RefPtr<PlatformDecoderModule> m(CreateBlankDecoderModule());
    return m->CreateVideoDecoder(aParams);
  }

  if (SupportsMimeType(aParams.mConfig.mMimeType, nullptr)) {
    // GMP decodes. Assume that means it can decrypt too.
    RefPtr<MediaDataDecoderProxy> wrapper =
      CreateDecoderWrapper(mProxy, aParams);
    auto params = GMPVideoDecoderParams(aParams);
    if (MediaPrefs::EMEChromiumAPIEnabled()) {
      wrapper->SetProxyTarget(new ChromiumCDMVideoDecoder(params, mProxy));
    } else {
      wrapper->SetProxyTarget(new EMEVideoDecoder(mProxy, params));
    }
    return wrapper.forget();
  }

  MOZ_ASSERT(mPDM);
  RefPtr<MediaDataDecoder> decoder(mPDM->CreateDecoder(aParams));
  if (!decoder) {
    return nullptr;
  }

  RefPtr<MediaDataDecoder> emeDecoder(new EMEDecryptor(
    decoder, mProxy, AbstractThread::GetCurrent()->AsTaskQueue(),
    aParams.mType, aParams.mOnWaitingForKeyEvent));
  return emeDecoder.forget();
}

already_AddRefed<MediaDataDecoder>
EMEDecoderModule::CreateAudioDecoder(const CreateDecoderParams& aParams)
{
  MOZ_ASSERT(aParams.mConfig.mCrypto.mValid);

  // We don't support using the GMP to decode audio.
  MOZ_ASSERT(!SupportsMimeType(aParams.mConfig.mMimeType, nullptr));
  MOZ_ASSERT(mPDM);

  if (MediaPrefs::EMEBlankAudio()) {
    EME_LOG("EMEDecoderModule::CreateAudioDecoder() creating a blank decoder.");
    RefPtr<PlatformDecoderModule> m(CreateBlankDecoderModule());
    return m->CreateAudioDecoder(aParams);
  }

  RefPtr<MediaDataDecoder> decoder(mPDM->CreateDecoder(aParams));
  if (!decoder) {
    return nullptr;
  }

  RefPtr<MediaDataDecoder> emeDecoder(new EMEDecryptor(
    decoder, mProxy, AbstractThread::GetCurrent()->AsTaskQueue(),
    aParams.mType, aParams.mOnWaitingForKeyEvent));
  return emeDecoder.forget();
}

bool
EMEDecoderModule::SupportsMimeType(const nsACString& aMimeType,
                                   DecoderDoctorDiagnostics* aDiagnostics) const
{
  Maybe<nsCString> gmp;
  gmp.emplace(NS_ConvertUTF16toUTF8(mProxy->KeySystem()));
  return GMPDecoderModule::SupportsMimeType(aMimeType, gmp);
}

} // namespace mozilla

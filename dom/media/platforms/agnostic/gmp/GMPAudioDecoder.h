/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(GMPAudioDecoder_h_)
#define GMPAudioDecoder_h_

#include "GMPAudioDecoderProxy.h"
#include "MediaDataDecoderProxy.h"
#include "PlatformDecoderModule.h"
#include "mozIGeckoMediaPluginService.h"
#include "nsAutoPtr.h"

namespace mozilla {

class AudioCallbackAdapter : public GMPAudioDecoderCallbackProxy {
public:
  explicit AudioCallbackAdapter(MediaDataDecoderCallbackProxy* aCallback)
   : mCallback(aCallback)
   , mLastStreamOffset(0)
   , mAudioFrameSum(0)
   , mAudioFrameOffset(0)
   , mMustRecaptureAudioPosition(true)
  {}

  MediaDataDecoderCallbackProxy* Callback() const { return mCallback; }

  // GMPAudioDecoderCallbackProxy
  void Decoded(const nsTArray<int16_t>& aPCM, uint64_t aTimeStamp, uint32_t aChannels, uint32_t aRate) override;
  void InputDataExhausted() override;
  void DrainComplete() override;
  void ResetComplete() override;
  void Error(GMPErr aErr) override;
  void Terminated() override;

  void SetLastStreamOffset(int64_t aStreamOffset) {
    mLastStreamOffset = aStreamOffset;
  }

private:
  MediaDataDecoderCallbackProxy* mCallback;
  int64_t mLastStreamOffset;

  int64_t mAudioFrameSum;
  int64_t mAudioFrameOffset;
  bool mMustRecaptureAudioPosition;
};

struct GMPAudioDecoderParams {
  explicit GMPAudioDecoderParams(const CreateDecoderParams& aParams);
  GMPAudioDecoderParams& WithCallback(MediaDataDecoderProxy* aWrapper);
  GMPAudioDecoderParams& WithAdapter(AudioCallbackAdapter* aAdapter);

  const AudioInfo& mConfig;
  TaskQueue* mTaskQueue;
  MediaDataDecoderCallbackProxy* mCallback;
  AudioCallbackAdapter* mAdapter;
  RefPtr<GMPCrashHelper> mCrashHelper;
};

class GMPAudioDecoder : public MediaDataDecoder {
public:
  explicit GMPAudioDecoder(const GMPAudioDecoderParams& aParams);

  RefPtr<InitPromise> Init() override;
  nsresult Input(MediaRawData* aSample) override;
  nsresult Flush() override;
  nsresult Drain() override;
  nsresult Shutdown() override;
  const char* GetDescriptionName() const override
  {
    return "GMP audio decoder";
  }

protected:
  virtual void InitTags(nsTArray<nsCString>& aTags);
  virtual nsCString GetNodeId();

private:

  class GMPInitDoneCallback : public GetGMPAudioDecoderCallback
  {
  public:
    explicit GMPInitDoneCallback(GMPAudioDecoder* aDecoder)
      : mDecoder(aDecoder)
    {
    }

    void Done(GMPAudioDecoderProxy* aGMP) override
    {
      mDecoder->GMPInitDone(aGMP);
    }

  private:
    RefPtr<GMPAudioDecoder> mDecoder;
  };
  void GMPInitDone(GMPAudioDecoderProxy* aGMP);

  const AudioInfo mConfig;
  MediaDataDecoderCallbackProxy* mCallback;
  nsCOMPtr<mozIGeckoMediaPluginService> mMPS;
  GMPAudioDecoderProxy* mGMP;
  nsAutoPtr<AudioCallbackAdapter> mAdapter;
  MozPromiseHolder<InitPromise> mInitPromise;
  RefPtr<GMPCrashHelper> mCrashHelper;
};

} // namespace mozilla

#endif // GMPAudioDecoder_h_

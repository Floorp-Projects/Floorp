/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORMS_ANDROID_ANDROIDDATAENCODER_H_
#define DOM_MEDIA_PLATFORMS_ANDROID_ANDROIDDATAENCODER_H_

#include "MediaData.h"
#include "PlatformEncoderModule.h"
#include "TimeUnits.h"

#include "JavaCallbacksSupport.h"

#include "mozilla/Maybe.h"
#include "mozilla/Monitor.h"

namespace mozilla {

template <typename ConfigType>
class AndroidDataEncoder final : public MediaDataEncoder {
 public:
  AndroidDataEncoder(const ConfigType& aConfig, RefPtr<TaskQueue> aTaskQueue)
      : mConfig(aConfig), mTaskQueue(aTaskQueue) {
    MOZ_ASSERT(mConfig.mSize.width > 0 && mConfig.mSize.height > 0);
    MOZ_ASSERT(mTaskQueue);
  }
  RefPtr<InitPromise> Init() override;
  RefPtr<EncodePromise> Encode(const MediaData* aSample) override;
  RefPtr<EncodePromise> Drain() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  RefPtr<GenericPromise> SetBitrate(const Rate aBitsPerSec) override;

  nsCString GetDescriptionName() const override { return "Android Encoder"_ns; }

 private:
  class CallbacksSupport final : public JavaCallbacksSupport {
   public:
    explicit CallbacksSupport(AndroidDataEncoder* aEncoder)
        : mEncoder(aEncoder) {}

    void HandleInput(int64_t aTimestamp, bool aProcessed) override;
    void HandleOutput(java::Sample::Param aSample,
                      java::SampleBuffer::Param aBuffer) override;
    void HandleOutputFormatChanged(
        java::sdk::MediaFormat::Param aFormat) override;
    void HandleError(const MediaResult& aError) override;

   private:
    AndroidDataEncoder* mEncoder;
  };
  friend class CallbacksSupport;

  // Methods only called on mTaskQueue.
  RefPtr<InitPromise> ProcessInit();
  RefPtr<EncodePromise> ProcessEncode(RefPtr<const MediaData> aSample);
  RefPtr<EncodePromise> ProcessDrain();
  RefPtr<ShutdownPromise> ProcessShutdown();
  void ProcessInput();
  void ProcessOutput(java::Sample::GlobalRef&& aSample,
                     java::SampleBuffer::GlobalRef&& aBuffer);
  RefPtr<MediaRawData> GetOutputData(java::SampleBuffer::Param aBuffer,
                                     const int32_t aOffset, const int32_t aSize,
                                     const bool aIsKeyFrame);
  void Error(const MediaResult& aError);

  void AssertOnTaskQueue() const {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  }

  const ConfigType mConfig;

  RefPtr<TaskQueue> mTaskQueue;

  // Can be accessed on any thread, but only written on during init.
  bool mIsHardwareAccelerated = false;

  java::CodecProxy::GlobalRef mJavaEncoder;
  java::CodecProxy::NativeCallbacks::GlobalRef mJavaCallbacks;
  java::sdk::MediaFormat::GlobalRef mFormat;
  // Preallocated Java object used as a reusable storage for input buffer
  // information. Contents must be changed only on mTaskQueue.
  java::sdk::BufferInfo::GlobalRef mInputBufferInfo;

  MozPromiseHolder<EncodePromise> mDrainPromise;

  // Accessed on mTaskqueue only.
  RefPtr<MediaByteBuffer> mYUVBuffer;
  EncodedData mEncodedData;
  // SPS/PPS NALUs for realtime usage, avcC otherwise.
  RefPtr<MediaByteBuffer> mConfigData;

  enum class DrainState { DRAINED, DRAINABLE, DRAINING };
  DrainState mDrainState;

  Maybe<MediaResult> mError;
};

}  // namespace mozilla

#endif

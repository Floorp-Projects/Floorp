/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaCodecDataDecoder_h_
#define MediaCodecDataDecoder_h_

#include "AndroidDecoderModule.h"

#include "MediaCodec.h"
#include "SurfaceTexture.h"
#include "TimeUnits.h"
#include "mozilla/Monitor.h"
#include "mozilla/Maybe.h"

#include <deque>

namespace mozilla {

typedef std::deque<RefPtr<MediaRawData>> SampleQueue;

class MediaCodecDataDecoder : public MediaDataDecoder {
public:
  static MediaDataDecoder* CreateAudioDecoder(const AudioInfo& aConfig,
                                              java::sdk::MediaFormat::Param aFormat,
                                              MediaDataDecoderCallback* aCallback,
                                              const nsString& aDrmStubId,
                                              CDMProxy* aProxy,
                                              TaskQueue* aTaskQueue);

  static MediaDataDecoder* CreateVideoDecoder(const VideoInfo& aConfig,
                                              java::sdk::MediaFormat::Param aFormat,
                                              MediaDataDecoderCallback* aCallback,
                                              layers::ImageContainer* aImageContainer,
                                              const nsString& aDrmStubId,
                                              CDMProxy* aProxy,
                                              TaskQueue* aTaskQueue);

  virtual ~MediaCodecDataDecoder();

  RefPtr<MediaDataDecoder::InitPromise> Init() override;
  void Flush() override;
  void Drain() override;
  void Shutdown() override;
  void Input(MediaRawData* aSample) override;
  const char* GetDescriptionName() const override
  {
    return "Android MediaCodec decoder";
  }

protected:
  enum class ModuleState : uint8_t {
    kDecoding = 0,
    kFlushing,
    kDrainQueue,
    kDrainDecoder,
    kDrainWaitEOS,
    kStopping,
    kShutdown
  };

  friend class AndroidDecoderModule;

  MediaCodecDataDecoder(MediaData::Type aType,
                        const nsACString& aMimeType,
                        java::sdk::MediaFormat::Param aFormat,
                        MediaDataDecoderCallback* aCallback,
                        const nsString& aDrmStubId);

  static const char* ModuleStateStr(ModuleState aState);

  virtual nsresult InitDecoder(java::sdk::Surface::Param aSurface);

  virtual nsresult Output(java::sdk::BufferInfo::Param aInfo, void* aBuffer,
      java::sdk::MediaFormat::Param aFormat, const media::TimeUnit& aDuration)
  {
    return NS_OK;
  }

  virtual nsresult PostOutput(java::sdk::BufferInfo::Param aInfo,
      java::sdk::MediaFormat::Param aFormat, const media::TimeUnit& aDuration)
  {
    return NS_OK;
  }

  virtual void Cleanup() {};

  nsresult ResetInputBuffers();
  nsresult ResetOutputBuffers();

  nsresult GetInputBuffer(JNIEnv* env, int index, jni::Object::LocalRef* buffer);
  bool WaitForInput();
  already_AddRefed<MediaRawData> PeekNextSample();
  nsresult QueueSample(const MediaRawData* aSample);
  nsresult QueueEOS();
  void HandleEOS(int32_t aOutputStatus);
  Maybe<media::TimeUnit> GetOutputDuration();
  nsresult ProcessOutput(java::sdk::BufferInfo::Param aInfo,
                         java::sdk::MediaFormat::Param aFormat,
                         int32_t aStatus);
  // Sets decoder state and returns whether the new state has become effective.
  bool SetState(ModuleState aState);
  void DecoderLoop();

  virtual void ClearQueue();

  MediaData::Type mType;

  nsAutoCString mMimeType;
  java::sdk::MediaFormat::GlobalRef mFormat;

  MediaDataDecoderCallback* mCallback;

  java::sdk::MediaCodec::GlobalRef mDecoder;

  jni::ObjectArray::GlobalRef mInputBuffers;
  jni::ObjectArray::GlobalRef mOutputBuffers;

  nsCOMPtr<nsIThread> mThread;

  // Only these members are protected by mMonitor.
  Monitor mMonitor;

  ModuleState mState;

  SampleQueue mQueue;
  // Durations are stored in microseconds.
  std::deque<media::TimeUnit> mDurations;

  nsString mDrmStubId;
};

} // namespace mozilla

#endif

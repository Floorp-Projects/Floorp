/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AndroidDecoderModule_h_
#define AndroidDecoderModule_h_

#include "PlatformDecoderModule.h"

#include "MediaCodec.h"
#include "SurfaceTexture.h"
#include "TimeUnits.h"
#include "mozilla/Monitor.h"
#include "mozilla/Maybe.h"

#include <deque>

namespace mozilla {

typedef std::deque<RefPtr<MediaRawData>> SampleQueue;

class AndroidDecoderModule : public PlatformDecoderModule {
public:
  already_AddRefed<MediaDataDecoder>
  CreateVideoDecoder(const VideoInfo& aConfig,
                     layers::LayersBackend aLayersBackend,
                     layers::ImageContainer* aImageContainer,
                     FlushableTaskQueue* aVideoTaskQueue,
                     MediaDataDecoderCallback* aCallback,
                     DecoderDoctorDiagnostics* aDiagnostics) override;

  already_AddRefed<MediaDataDecoder>
  CreateAudioDecoder(const AudioInfo& aConfig,
                     FlushableTaskQueue* aAudioTaskQueue,
                     MediaDataDecoderCallback* aCallback,
                     DecoderDoctorDiagnostics* aDiagnostics) override;


  AndroidDecoderModule() {}
  virtual ~AndroidDecoderModule() {}

  bool SupportsMimeType(const nsACString& aMimeType,
                        DecoderDoctorDiagnostics* aDiagnostics) const override;

  ConversionRequired
  DecoderNeedsConversion(const TrackInfo& aConfig) const override;
};

class MediaCodecDataDecoder : public MediaDataDecoder {
public:

  MediaCodecDataDecoder(MediaData::Type aType,
                        const nsACString& aMimeType,
                        widget::sdk::MediaFormat::Param aFormat,
                        MediaDataDecoderCallback* aCallback);

  virtual ~MediaCodecDataDecoder();

  RefPtr<MediaDataDecoder::InitPromise> Init() override;
  nsresult Flush() override;
  nsresult Drain() override;
  nsresult Shutdown() override;
  nsresult Input(MediaRawData* aSample) override;
  const char* GetDescriptionName() const override
  {
    return "android decoder";
  }

protected:
  enum ModuleState {
    kDecoding = 0,
    kFlushing,
    kDrainQueue,
    kDrainDecoder,
    kDrainWaitEOS,
    kStopping,
    kShutdown
  };

  friend class AndroidDecoderModule;

  static const char* ModuleStateStr(ModuleState aState);

  virtual nsresult InitDecoder(widget::sdk::Surface::Param aSurface);

  virtual nsresult Output(widget::sdk::BufferInfo::Param aInfo, void* aBuffer,
      widget::sdk::MediaFormat::Param aFormat, const media::TimeUnit& aDuration)
  {
    return NS_OK;
  }

  virtual nsresult PostOutput(widget::sdk::BufferInfo::Param aInfo,
      widget::sdk::MediaFormat::Param aFormat, const media::TimeUnit& aDuration)
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
  nsresult ProcessOutput(widget::sdk::BufferInfo::Param aInfo,
                         widget::sdk::MediaFormat::Param aFormat,
                         int32_t aStatus);
  ModuleState State() const;
  // Sets decoder state and returns whether the new state has become effective.
  bool State(ModuleState aState);
  void DecoderLoop();

  virtual void ClearQueue();

  MediaData::Type mType;

  nsAutoCString mMimeType;
  widget::sdk::MediaFormat::GlobalRef mFormat;

  MediaDataDecoderCallback* mCallback;

  widget::sdk::MediaCodec::GlobalRef mDecoder;

  jni::ObjectArray::GlobalRef mInputBuffers;
  jni::ObjectArray::GlobalRef mOutputBuffers;

  nsCOMPtr<nsIThread> mThread;

  // Only these members are protected by mMonitor.
  Monitor mMonitor;

  ModuleState mState;

  SampleQueue mQueue;
  // Durations are stored in microseconds.
  std::deque<media::TimeUnit> mDurations;
};

} // namespace mozilla

#endif

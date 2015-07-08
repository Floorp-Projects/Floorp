/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AndroidDecoderModule_h_
#define AndroidDecoderModule_h_

#include "PlatformDecoderModule.h"
#include "AndroidSurfaceTexture.h"

#include "MediaCodec.h"
#include "TimeUnits.h"
#include "mozilla/Monitor.h"

#include <queue>

namespace mozilla {

typedef std::queue<nsRefPtr<MediaRawData>> SampleQueue;

class AndroidDecoderModule : public PlatformDecoderModule {
public:
  virtual already_AddRefed<MediaDataDecoder>
  CreateVideoDecoder(const VideoInfo& aConfig,
                     layers::LayersBackend aLayersBackend,
                     layers::ImageContainer* aImageContainer,
                     FlushableMediaTaskQueue* aVideoTaskQueue,
                     MediaDataDecoderCallback* aCallback) override;

  virtual already_AddRefed<MediaDataDecoder>
  CreateAudioDecoder(const AudioInfo& aConfig,
                     FlushableMediaTaskQueue* aAudioTaskQueue,
                     MediaDataDecoderCallback* aCallback) override;


  AndroidDecoderModule() {}
  virtual ~AndroidDecoderModule() {}

  virtual bool SupportsMimeType(const nsACString& aMimeType) override;

  virtual ConversionRequired
  DecoderNeedsConversion(const TrackInfo& aConfig) const override;
};

class MediaCodecDataDecoder : public MediaDataDecoder {
public:

  MediaCodecDataDecoder(MediaData::Type aType,
                        const nsACString& aMimeType,
                        widget::sdk::MediaFormat::Param aFormat,
                        MediaDataDecoderCallback* aCallback);

  virtual ~MediaCodecDataDecoder();

  virtual nsresult Init() override;
  virtual nsresult Flush() override;
  virtual nsresult Drain() override;
  virtual nsresult Shutdown() override;
  virtual nsresult Input(MediaRawData* aSample);

protected:
  friend class AndroidDecoderModule;

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
  bool mFlushing;
  bool mDraining;
  bool mStopping;

  SampleQueue mQueue;
  // Durations are stored in microseconds.
  std::queue<media::TimeUnit> mDurations;

  virtual nsresult InitDecoder(widget::sdk::Surface::Param aSurface);

  virtual nsresult Output(widget::sdk::BufferInfo::Param aInfo, void* aBuffer, widget::sdk::MediaFormat::Param aFormat, const media::TimeUnit& aDuration) { return NS_OK; }
  virtual nsresult PostOutput(widget::sdk::BufferInfo::Param aInfo, widget::sdk::MediaFormat::Param aFormat, const media::TimeUnit& aDuration) { return NS_OK; }
  virtual void Cleanup() {};

  nsresult ResetInputBuffers();
  nsresult ResetOutputBuffers();

  void DecoderLoop();
  nsresult GetInputBuffer(JNIEnv* env, int index, jni::Object::LocalRef* buffer);
  virtual void ClearQueue();
};

} // namwspace mozilla

#endif

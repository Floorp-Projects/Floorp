/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AndroidDecoderModule_h_
#define AndroidDecoderModule_h_

#include "PlatformDecoderModule.h"
#include "AndroidJavaWrappers.h"
#include "AndroidSurfaceTexture.h"

#include "GeneratedSDKWrappers.h"
#include "mozilla/Monitor.h"

#include <queue>

namespace mozilla {

typedef std::queue<mp4_demuxer::MP4Sample*> SampleQueue;

namespace widget {
namespace android {
  class MediaCodec;
  class MediaFormat;
  class ByteBuffer;
}
}

class MediaCodecDataDecoder;

class AndroidDecoderModule : public PlatformDecoderModule {
public:
  virtual nsresult Shutdown() MOZ_OVERRIDE;

  virtual already_AddRefed<MediaDataDecoder>
  CreateH264Decoder(const mp4_demuxer::VideoDecoderConfig& aConfig,
                    layers::LayersBackend aLayersBackend,
                    layers::ImageContainer* aImageContainer,
                    MediaTaskQueue* aVideoTaskQueue,
                    MediaDataDecoderCallback* aCallback) MOZ_OVERRIDE;

  virtual already_AddRefed<MediaDataDecoder>
  CreateAudioDecoder(const mp4_demuxer::AudioDecoderConfig& aConfig,
                     MediaTaskQueue* aAudioTaskQueue,
                     MediaDataDecoderCallback* aCallback) MOZ_OVERRIDE;


  AndroidDecoderModule() {}
  virtual ~AndroidDecoderModule() {}

  virtual bool SupportsAudioMimeType(const char* aMimeType) MOZ_OVERRIDE;
};

class MediaCodecDataDecoder : public MediaDataDecoder {
public:

  MediaCodecDataDecoder(MediaData::Type aType,
                        const char* aMimeType,
                        mozilla::widget::android::MediaFormat* aFormat,
                        MediaDataDecoderCallback* aCallback);

  virtual ~MediaCodecDataDecoder();

  virtual nsresult Init() MOZ_OVERRIDE;
  virtual nsresult Flush() MOZ_OVERRIDE;
  virtual nsresult Drain() MOZ_OVERRIDE;
  virtual nsresult Shutdown() MOZ_OVERRIDE;
  virtual nsresult Input(mp4_demuxer::MP4Sample* aSample);

protected:
  friend class AndroidDecoderModule;

  MediaData::Type mType;

  nsAutoPtr<char> mMimeType;
  nsAutoPtr<mozilla::widget::android::MediaFormat> mFormat;

  MediaDataDecoderCallback* mCallback;

  nsAutoPtr<mozilla::widget::android::MediaCodec> mDecoder;

  jobjectArray mInputBuffers;
  jobjectArray mOutputBuffers;

  nsCOMPtr<nsIThread> mThread;

  // Only these members are protected by mMonitor.
  Monitor mMonitor;
  bool mDraining;
  bool mStopping;

  SampleQueue mQueue;
  std::queue<Microseconds> mDurations;

  virtual nsresult InitDecoder(jobject aSurface = nullptr);

  virtual nsresult Output(mozilla::widget::android::BufferInfo* aInfo, void* aBuffer, mozilla::widget::android::MediaFormat* aFormat, Microseconds aDuration) { return NS_OK; }
  virtual nsresult PostOutput(mozilla::widget::android::BufferInfo* aInfo, mozilla::widget::android::MediaFormat* aFormat, Microseconds aDuration) { return NS_OK; }

  void ResetInputBuffers();
  void ResetOutputBuffers();

  void DecoderLoop();
  virtual void ClearQueue();
};

} // namwspace mozilla

#endif

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AndroidDecoderModule_h_
#define AndroidDecoderModule_h_

#include "PlatformDecoderModule.h"
#include "AndroidSurfaceTexture.h"

#include "MediaCodec.h"
#include "mozilla/Monitor.h"

#include <queue>

namespace mozilla {

typedef std::queue<mp4_demuxer::MP4Sample*> SampleQueue;

class AndroidDecoderModule : public PlatformDecoderModule {
public:
  virtual nsresult Shutdown() MOZ_OVERRIDE;

  virtual already_AddRefed<MediaDataDecoder>
  CreateVideoDecoder(const mp4_demuxer::VideoDecoderConfig& aConfig,
                     layers::LayersBackend aLayersBackend,
                     layers::ImageContainer* aImageContainer,
                     FlushableMediaTaskQueue* aVideoTaskQueue,
                     MediaDataDecoderCallback* aCallback) MOZ_OVERRIDE;

  virtual already_AddRefed<MediaDataDecoder>
  CreateAudioDecoder(const mp4_demuxer::AudioDecoderConfig& aConfig,
                     FlushableMediaTaskQueue* aAudioTaskQueue,
                     MediaDataDecoderCallback* aCallback) MOZ_OVERRIDE;


  AndroidDecoderModule() {}
  virtual ~AndroidDecoderModule() {}

  virtual bool SupportsAudioMimeType(const char* aMimeType) MOZ_OVERRIDE;
};

class MediaCodecDataDecoder : public MediaDataDecoder {
public:

  MediaCodecDataDecoder(MediaData::Type aType,
                        const char* aMimeType,
                        widget::sdk::MediaFormat::Param aFormat,
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
  std::queue<Microseconds> mDurations;

  virtual nsresult InitDecoder(widget::sdk::Surface::Param aSurface);

  virtual nsresult Output(widget::sdk::BufferInfo::Param aInfo, void* aBuffer, widget::sdk::MediaFormat::Param aFormat, Microseconds aDuration) { return NS_OK; }
  virtual nsresult PostOutput(widget::sdk::BufferInfo::Param aInfo, widget::sdk::MediaFormat::Param aFormat, Microseconds aDuration) { return NS_OK; }
  virtual void Cleanup() {};

  nsresult ResetInputBuffers();
  nsresult ResetOutputBuffers();

  void DecoderLoop();
  nsresult GetInputBuffer(JNIEnv* env, int index, jni::Object::LocalRef* buffer);
  virtual void ClearQueue();
};

} // namwspace mozilla

#endif

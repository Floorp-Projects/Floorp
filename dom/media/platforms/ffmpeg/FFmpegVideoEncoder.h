/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGVIDEOENCODER_H_
#define DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGVIDEOENCODER_H_

#include "PlatformEncoderModule.h"
#include "FFmpegLibWrapper.h"
#include "mozilla/ThreadSafety.h"

// This must be the last header included
#include "FFmpegLibs.h"

namespace mozilla {

template <int V>
AVCodecID GetFFmpegEncoderCodecId(CodecType aCodec);

template <>
AVCodecID GetFFmpegEncoderCodecId<LIBAV_VER>(CodecType aCodec);

template <int V>
class FFmpegVideoEncoder : public MediaDataEncoder {};

// TODO: Bug 1860925: FFmpegDataEncoder
template <>
class FFmpegVideoEncoder<LIBAV_VER> final : public MediaDataEncoder {
 public:
  FFmpegVideoEncoder(const FFmpegLibWrapper* aLib, AVCodecID aCodecID,
                     const RefPtr<TaskQueue>& aTaskQueue,
                     const EncoderConfig& aConfig);

  /* MediaDataEncoder Methods */
  // All methods run on the task queue, except for GetDescriptionName.
  RefPtr<InitPromise> Init() override;
  RefPtr<EncodePromise> Encode(const MediaData* aSample) override;
  RefPtr<ReconfigurationPromise> Reconfigure(
      const RefPtr<const EncoderConfigurationChangeList>& aConfigurationChanges)
      override;
  RefPtr<EncodePromise> Drain() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  RefPtr<GenericPromise> SetBitrate(uint32_t aBitRate) override;
  nsCString GetDescriptionName() const override;

 private:
  ~FFmpegVideoEncoder() = default;

  // Methods only called on mTaskQueue.
  RefPtr<InitPromise> ProcessInit();
  RefPtr<EncodePromise> ProcessEncode(RefPtr<const MediaData> aSample);
  RefPtr<ReconfigurationPromise> ProcessReconfigure(
      const RefPtr<const EncoderConfigurationChangeList>&
          aConfigurationChanges);
  RefPtr<EncodePromise> ProcessDrain();
  void ProcessShutdown();
  MediaResult InitInternal();
  void ShutdownInternal();
  // TODO: Share these with FFmpegDataDecoder.
  int OpenCodecContext(const AVCodec* aCodec, AVDictionary** aOptions)
      MOZ_EXCLUDES(sMutex);
  void CloseCodecContext() MOZ_EXCLUDES(sMutex);
  bool PrepareFrame();
  void DestroyFrame();
#if LIBAVCODEC_VERSION_MAJOR >= 58
  RefPtr<EncodePromise> EncodeWithModernAPIs(RefPtr<const VideoData> aSample);
  RefPtr<EncodePromise> DrainWithModernAPIs();
#endif
  RefPtr<MediaRawData> ToMediaRawData(AVPacket* aPacket);
  void ForceEnablingFFmpegDebugLogs();

  // This refers to a static FFmpegLibWrapper, so raw pointer is adequate.
  const FFmpegLibWrapper* mLib;
  const AVCodecID mCodecID;
  const RefPtr<TaskQueue> mTaskQueue;

  // set in constructor, modified when parameters change
  EncoderConfig mConfig;

  // mTaskQueue only.
  AVCodecContext* mCodecContext;
  AVFrame* mFrame;

  // Provide critical-section for open/close mCodecContext.
  // TODO: Merge this with FFmpegDataDecoder's one.
  static StaticMutex sMutex;
};

}  // namespace mozilla

#endif /* DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGVIDEOENCODER_H_ */

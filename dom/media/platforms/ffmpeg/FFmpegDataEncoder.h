/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGDATAENCODER_H_
#define DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGDATAENCODER_H_

#include "FFmpegLibWrapper.h"
#include "PlatformEncoderModule.h"
#include "SimpleMap.h"
#include "mozilla/ThreadSafety.h"

// This must be the last header included
#include "FFmpegLibs.h"

namespace mozilla {

template <int V>
AVCodecID GetFFmpegEncoderCodecId(CodecType aCodec);

template <>
AVCodecID GetFFmpegEncoderCodecId<LIBAV_VER>(CodecType aCodec);

template <int V>
class FFmpegDataEncoder : public MediaDataEncoder {};

template <>
class FFmpegDataEncoder<LIBAV_VER> : public MediaDataEncoder {
  using DurationMap = SimpleMap<int64_t, int64_t, ThreadSafePolicy>;

 public:
  static AVCodec* FindEncoderWithPreference(const FFmpegLibWrapper* aLib,
                                            AVCodecID aCodecId);

  FFmpegDataEncoder(const FFmpegLibWrapper* aLib, AVCodecID aCodecID,
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

 protected:
  // Methods only called on mTaskQueue.
  RefPtr<InitPromise> ProcessInit();
  RefPtr<EncodePromise> ProcessEncode(RefPtr<const MediaData> aSample);
  RefPtr<ReconfigurationPromise> ProcessReconfigure(
      const RefPtr<const EncoderConfigurationChangeList>&
          aConfigurationChanges);
  RefPtr<EncodePromise> ProcessDrain();
  RefPtr<ShutdownPromise> ProcessShutdown();
  // Initialize the audio or video-specific members of an encoder instance.
  virtual nsresult InitSpecific() = 0;
  // nullptr in case of failure. This is to be called by the
  // audio/video-specific InitInternal methods in the sub-class, and initializes
  // the common members.
  AVCodec* InitCommon();
  MediaResult FinishInitCommon(AVCodec* aCodec);
  void ShutdownInternal();
  int OpenCodecContext(const AVCodec* aCodec, AVDictionary** aOptions)
      MOZ_EXCLUDES(sMutex);
  void CloseCodecContext() MOZ_EXCLUDES(sMutex);
  bool PrepareFrame();
  void DestroyFrame();
#if LIBAVCODEC_VERSION_MAJOR >= 58
  virtual Result<EncodedData, nsresult> EncodeInputWithModernAPIs(
      RefPtr<const MediaData> aSample) = 0;
  Result<EncodedData, nsresult> EncodeWithModernAPIs();
  virtual Result<EncodedData, nsresult> DrainWithModernAPIs();
#endif
  // Convert an AVPacket to a MediaRawData. This can return nullptr if a packet
  // has been processed by the encoder, but is not to be returned to the caller,
  // because DTX is enabled.
  virtual RefPtr<MediaRawData> ToMediaRawData(AVPacket* aPacket) = 0;
  RefPtr<MediaRawData> ToMediaRawDataCommon(AVPacket* aPacket);
  virtual Result<already_AddRefed<MediaByteBuffer>, nsresult> GetExtraData(
      AVPacket* aPacket) = 0;
  void ForceEnablingFFmpegDebugLogs();

  // This refers to a static FFmpegLibWrapper, so raw pointer is adequate.
  const FFmpegLibWrapper* mLib;
  const AVCodecID mCodecID;
  const RefPtr<TaskQueue> mTaskQueue;

  // set in constructor, modified when parameters change
  EncoderConfig mConfig;

  // mTaskQueue only.
  nsCString mCodecName;
  AVCodecContext* mCodecContext;
  AVFrame* mFrame;
  DurationMap mDurationMap;

  // Provide critical-section for open/close mCodecContext.
  static StaticMutex sMutex;
  const bool mVideoCodec;
};

}  // namespace mozilla

#endif /* DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGDATAENCODER_H_ */

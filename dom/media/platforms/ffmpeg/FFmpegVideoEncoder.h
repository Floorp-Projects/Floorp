/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGVIDEOENCODER_H_
#define DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGVIDEOENCODER_H_

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
class FFmpegVideoEncoder : public MediaDataEncoder {};

// TODO: Bug 1860925: FFmpegDataEncoder
template <>
class FFmpegVideoEncoder<LIBAV_VER> final : public MediaDataEncoder {
  using DurationMap = SimpleMap<int64_t>;

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
      const RefPtr<const EncoderConfigurationChangeList> aConfigurationChanges);
  RefPtr<EncodePromise> ProcessDrain();
  RefPtr<ShutdownPromise> ProcessShutdown();
  MediaResult InitInternal();
  void ShutdownInternal();
  // TODO: Share these with FFmpegDataDecoder.
  int OpenCodecContext(const AVCodec* aCodec, AVDictionary** aOptions)
      MOZ_EXCLUDES(sMutex);
  void CloseCodecContext() MOZ_EXCLUDES(sMutex);
  bool PrepareFrame();
  void DestroyFrame();
  bool ScaleInputFrame();
#if LIBAVCODEC_VERSION_MAJOR >= 58
  RefPtr<EncodePromise> EncodeWithModernAPIs(RefPtr<const VideoData> aSample);
  RefPtr<EncodePromise> DrainWithModernAPIs();
#endif
  RefPtr<MediaRawData> ToMediaRawData(AVPacket* aPacket);
  Result<already_AddRefed<MediaByteBuffer>, nsresult> GetExtraData(
      AVPacket* aPacket);
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

  struct SVCInfo {
    explicit SVCInfo(nsTArray<uint8_t>&& aTemporalLayerIds)
        : mTemporalLayerIds(std::move(aTemporalLayerIds)), mNextIndex(0) {}
    const nsTArray<uint8_t> mTemporalLayerIds;
    size_t mNextIndex;
    // Return the current temporal layer id and update the next.
    uint8_t UpdateTemporalLayerId();
  };
  Maybe<SVCInfo> mSVCInfo;

  // Provide critical-section for open/close mCodecContext.
  // TODO: Merge this with FFmpegDataDecoder's one.
  static StaticMutex sMutex;
};

}  // namespace mozilla

#endif /* DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGVIDEOENCODER_H_ */

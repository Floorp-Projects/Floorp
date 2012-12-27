/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(WMFReader_h_)
#define WMFReader_h_

#include "WMF.h"
#include "MediaDecoderReader.h"

namespace mozilla {

class WMFByteStream;

// Decoder backend for reading H.264/AAC in MP4/M4A and MP3 audio files,
// using Windows Media Foundation.
class WMFReader : public MediaDecoderReader
{
public:
  WMFReader(MediaDecoder* aDecoder);

  virtual ~WMFReader();

  nsresult Init(MediaDecoderReader* aCloneDonor) MOZ_OVERRIDE;

  bool DecodeAudioData() MOZ_OVERRIDE;
  bool DecodeVideoFrame(bool &aKeyframeSkip,
                        int64_t aTimeThreshold) MOZ_OVERRIDE;

  bool HasAudio() MOZ_OVERRIDE;
  bool HasVideo() MOZ_OVERRIDE;

  nsresult ReadMetadata(VideoInfo* aInfo,
                        MetadataTags** aTags) MOZ_OVERRIDE;

  nsresult Seek(int64_t aTime,
                int64_t aStartTime,
                int64_t aEndTime,
                int64_t aCurrentTime) MOZ_OVERRIDE;

  nsresult GetBuffered(nsTimeRanges* aBuffered,
                       int64_t aStartTime) MOZ_OVERRIDE;

  void OnDecodeThreadStart() MOZ_OVERRIDE;
  void OnDecodeThreadFinish() MOZ_OVERRIDE;

private:

  void ConfigureAudioDecoder();
  void ConfigureVideoDecoder();

  RefPtr<IMFSourceReader> mSourceReader;
  RefPtr<WMFByteStream> mByteStream;

  uint32_t mAudioChannels;
  uint32_t mAudioBytesPerSample;
  uint32_t mAudioRate;

  uint32_t mVideoWidth;
  uint32_t mVideoHeight;
  uint32_t mVideoStride;

  bool mHasAudio;
  bool mHasVideo;
  bool mCanSeek;
};

} // namespace mozilla

#endif

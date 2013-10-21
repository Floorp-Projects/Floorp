/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(WMFReader_h_)
#define WMFReader_h_

#include "WMF.h"
#include "MediaDecoderReader.h"
#include "nsAutoPtr.h"
#include "mozilla/RefPtr.h"
#include "nsRect.h"

namespace mozilla {

class WMFByteStream;
class WMFSourceReaderCallback;
class DXVA2Manager;

namespace dom {
class TimeRanges;
}

// Decoder backend for reading H.264/AAC in MP4/M4A, and MP3 files using
// Windows Media Foundation.
class WMFReader : public MediaDecoderReader
{
public:
  WMFReader(AbstractMediaDecoder* aDecoder);

  virtual ~WMFReader();

  nsresult Init(MediaDecoderReader* aCloneDonor) MOZ_OVERRIDE;

  bool DecodeAudioData() MOZ_OVERRIDE;
  bool DecodeVideoFrame(bool &aKeyframeSkip,
                        int64_t aTimeThreshold) MOZ_OVERRIDE;

  bool HasAudio() MOZ_OVERRIDE;
  bool HasVideo() MOZ_OVERRIDE;

  nsresult ReadMetadata(MediaInfo* aInfo,
                        MetadataTags** aTags) MOZ_OVERRIDE;

  nsresult Seek(int64_t aTime,
                int64_t aStartTime,
                int64_t aEndTime,
                int64_t aCurrentTime) MOZ_OVERRIDE;

  void OnDecodeThreadStart() MOZ_OVERRIDE;
  void OnDecodeThreadFinish() MOZ_OVERRIDE;

private:

  HRESULT ConfigureAudioDecoder();
  HRESULT ConfigureVideoDecoder();
  HRESULT ConfigureVideoFrameGeometry(IMFMediaType* aMediaType);
  void GetSupportedAudioCodecs(const GUID** aCodecs, uint32_t* aNumCodecs);

  HRESULT CreateBasicVideoFrame(IMFSample* aSample,
                                int64_t aTimestampUsecs,
                                int64_t aDurationUsecs,
                                int64_t aOffsetBytes,
                                VideoData** aOutVideoData);

  HRESULT CreateD3DVideoFrame(IMFSample* aSample,
                              int64_t aTimestampUsecs,
                              int64_t aDurationUsecs,
                              int64_t aOffsetBytes,
                              VideoData** aOutVideoData);

  // Attempt to initialize DXVA. Returns true on success.
  bool InitializeDXVA();  

  RefPtr<IMFSourceReader> mSourceReader;
  RefPtr<WMFByteStream> mByteStream;
  RefPtr<WMFSourceReaderCallback> mSourceReaderCallback;
  nsAutoPtr<DXVA2Manager> mDXVA2Manager;

  // Region inside the video frame that makes up the picture. Pixels outside
  // of this region should not be rendered.
  nsIntRect mPictureRegion;

  uint32_t mAudioChannels;
  uint32_t mAudioBytesPerSample;
  uint32_t mAudioRate;

  uint32_t mVideoWidth;
  uint32_t mVideoHeight;
  uint32_t mVideoStride;

  // The offset, in audio frames, at which playback started since the
  // last discontinuity.
  int64_t mAudioFrameOffset;
  // The number of audio frames that we've played since the last
  // discontinuity.
  int64_t mAudioFrameSum;
  // True if we need to re-initialize mAudioFrameOffset and mAudioFrameSum
  // from the next audio packet we decode. This happens after a seek, since
  // WMF doesn't mark a stream as having a discontinuity after a seek(0).
  bool mMustRecaptureAudioPosition;

  bool mHasAudio;
  bool mHasVideo;
  bool mUseHwAccel;

  // We can't call WMFDecoder::IsMP3Supported() on non-main threads, since it
  // checks a pref, so we cache its value in mIsMP3Enabled and use that on
  // the decode thread.
  const bool mIsMP3Enabled;

  bool mCOMInitialized;
};

} // namespace mozilla

#endif

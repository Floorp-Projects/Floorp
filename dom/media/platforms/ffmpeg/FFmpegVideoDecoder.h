/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFmpegVideoDecoder_h__
#define __FFmpegVideoDecoder_h__

#include "FFmpegLibWrapper.h"
#include "FFmpegDataDecoder.h"
#include "mozilla/Pair.h"
#include "nsTArray.h"

namespace mozilla
{

template <int V>
class FFmpegVideoDecoder : public FFmpegDataDecoder<V>
{
};

template <>
class FFmpegVideoDecoder<LIBAV_VER> : public FFmpegDataDecoder<LIBAV_VER>
{
  typedef mozilla::layers::Image Image;
  typedef mozilla::layers::ImageContainer ImageContainer;

  enum DecodeResult {
    DECODE_FRAME,
    DECODE_NO_FRAME,
    DECODE_ERROR
  };

public:
  FFmpegVideoDecoder(FFmpegLibWrapper* aLib, FlushableTaskQueue* aTaskQueue,
                     MediaDataDecoderCallback* aCallback,
                     const VideoInfo& aConfig,
                     ImageContainer* aImageContainer);
  virtual ~FFmpegVideoDecoder();

  RefPtr<InitPromise> Init() override;
  nsresult Input(MediaRawData* aSample) override;
  void ProcessDrain() override;
  void ProcessFlush() override;
  void InitCodecContext() override;
  const char* GetDescriptionName() const override
  {
#ifdef USING_MOZFFVPX
    return "ffvpx video decoder";
#else
    return "ffmpeg video decoder";
#endif
  }
  static AVCodecID GetCodecId(const nsACString& aMimeType);

private:
  void ProcessDecode(MediaRawData* aSample);
  DecodeResult DoDecode(MediaRawData* aSample);
  DecodeResult DoDecode(MediaRawData* aSample, uint8_t* aData, int aSize);
  void DoDrain();
  void OutputDelayedFrames();

  /**
   * This method allocates a buffer for FFmpeg's decoder, wrapped in an Image.
   * Currently it only supports Planar YUV420, which appears to be the only
   * non-hardware accelerated image format that FFmpeg's H264 decoder is
   * capable of outputting.
   */
  int AllocateYUV420PVideoBuffer(AVCodecContext* aCodecContext,
                                 AVFrame* aFrame);

  RefPtr<ImageContainer> mImageContainer;
  VideoInfo mInfo;

  // Parser used for VP8 and VP9 decoding.
  AVCodecParserContext* mCodecParser;

  class PtsCorrectionContext {
  public:
    PtsCorrectionContext();
    int64_t GuessCorrectPts(int64_t aPts, int64_t aDts);
    void Reset();
    int64_t LastDts() const { return mLastDts; }

  private:
    int64_t mNumFaultyPts; /// Number of incorrect PTS values so far
    int64_t mNumFaultyDts; /// Number of incorrect DTS values so far
    int64_t mLastPts;       /// PTS of the last frame
    int64_t mLastDts;       /// DTS of the last frame
  };

  PtsCorrectionContext mPtsContext;

  class DurationMap {
  public:
    typedef Pair<int64_t, int64_t> DurationElement;

    // Insert Key and Duration pair at the end of our map.
    void Insert(int64_t aKey, int64_t aDuration)
    {
      mMap.AppendElement(MakePair(aKey, aDuration));
    }
    // Sets aDuration matching aKey and remove it from the map if found.
    // The element returned is the first one found.
    // Returns true if found, false otherwise.
    bool Find(int64_t aKey, int64_t& aDuration)
    {
      for (uint32_t i = 0; i < mMap.Length(); i++) {
        DurationElement& element = mMap[i];
        if (element.first() == aKey) {
          aDuration = element.second();
          mMap.RemoveElementAt(i);
          return true;
        }
      }
      return false;
    }
    // Remove all elements of the map.
    void Clear()
    {
      mMap.Clear();
    }

  private:
    AutoTArray<DurationElement, 16> mMap;
  };

  DurationMap mDurationMap;
};

} // namespace mozilla

#endif // __FFmpegVideoDecoder_h__

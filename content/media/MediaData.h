/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(MediaData_h)
#define MediaData_h

#include "nsSize.h"
#include "mozilla/gfx/Rect.h"
#include "nsRect.h"
#include "AudioSampleFormat.h"
#include "nsIMemoryReporter.h"
#include "SharedBuffer.h"

namespace mozilla {

namespace layers {
class Image;
class ImageContainer;
}

// Container that holds media samples.
class MediaData {
public:

  enum Type {
    AUDIO_SAMPLES = 0,
    VIDEO_FRAME = 1
  };

  MediaData(Type aType,
            int64_t aOffset,
            int64_t aTimestamp,
            int64_t aDuration)
    : mType(aType)
    , mOffset(aOffset)
    , mTime(aTimestamp)
    , mDuration(aDuration)
    , mDiscontinuity(false)
  {}

  virtual ~MediaData() {}

  // Type of contained data.
  const Type mType;

  // Approximate byte offset where this data was demuxed from its media.
  const int64_t mOffset;

  // Start time of sample, in microseconds.
  const int64_t mTime;

  // Duration of sample, in microseconds.
  const int64_t mDuration;

  // True if this is the first sample after a gap or discontinuity in
  // the stream. This is true for the first sample in a stream after a seek.
  bool mDiscontinuity;

  int64_t GetEndTime() const { return mTime + mDuration; }

};

// Holds chunk a decoded audio frames.
class AudioData : public MediaData {
public:

  AudioData(int64_t aOffset,
            int64_t aTime,
            int64_t aDuration,
            uint32_t aFrames,
            AudioDataValue* aData,
            uint32_t aChannels)
    : MediaData(AUDIO_SAMPLES, aOffset, aTime, aDuration)
    , mFrames(aFrames)
    , mChannels(aChannels)
    , mAudioData(aData)
  {
    MOZ_COUNT_CTOR(AudioData);
  }

  ~AudioData()
  {
    MOZ_COUNT_DTOR(AudioData);
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

  // If mAudioBuffer is null, creates it from mAudioData.
  void EnsureAudioBuffer();

  const uint32_t mFrames;
  const uint32_t mChannels;
  // At least one of mAudioBuffer/mAudioData must be non-null.
  // mChannels channels, each with mFrames frames
  nsRefPtr<SharedBuffer> mAudioBuffer;
  // mFrames frames, each with mChannels values
  nsAutoArrayPtr<AudioDataValue> mAudioData;
};

namespace layers {
class TextureClient;
class PlanarYCbCrImage;
}

class VideoInfo;

// Holds a decoded video frame, in YCbCr format. These are queued in the reader.
class VideoData : public MediaData {
public:
  typedef gfx::IntRect IntRect;
  typedef gfx::IntSize IntSize;
  typedef layers::ImageContainer ImageContainer;
  typedef layers::Image Image;
  typedef layers::PlanarYCbCrImage PlanarYCbCrImage;

  // YCbCr data obtained from decoding the video. The index's are:
  //   0 = Y
  //   1 = Cb
  //   2 = Cr
  struct YCbCrBuffer {
    struct Plane {
      uint8_t* mData;
      uint32_t mWidth;
      uint32_t mHeight;
      uint32_t mStride;
      uint32_t mOffset;
      uint32_t mSkip;
    };

    Plane mPlanes[3];
  };

  // Constructs a VideoData object. If aImage is nullptr, creates a new Image
  // holding a copy of the YCbCr data passed in aBuffer. If aImage is not
  // nullptr, it's stored as the underlying video image and aBuffer is assumed
  // to point to memory within aImage so no copy is made. aTimecode is a codec
  // specific number representing the timestamp of the frame of video data.
  // Returns nsnull if an error occurs. This may indicate that memory couldn't
  // be allocated to create the VideoData object, or it may indicate some
  // problem with the input data (e.g. negative stride).
  static VideoData* Create(VideoInfo& aInfo,
                           ImageContainer* aContainer,
                           Image* aImage,
                           int64_t aOffset,
                           int64_t aTime,
                           int64_t aDuration,
                           const YCbCrBuffer &aBuffer,
                           bool aKeyframe,
                           int64_t aTimecode,
                           const IntRect& aPicture);

  // Variant that always makes a copy of aBuffer
  static VideoData* Create(VideoInfo& aInfo,
                           ImageContainer* aContainer,
                           int64_t aOffset,
                           int64_t aTime,
                           int64_t aDuration,
                           const YCbCrBuffer &aBuffer,
                           bool aKeyframe,
                           int64_t aTimecode,
                           const IntRect& aPicture);

  // Variant to create a VideoData instance given an existing aImage
  static VideoData* Create(VideoInfo& aInfo,
                           Image* aImage,
                           int64_t aOffset,
                           int64_t aTime,
                           int64_t aDuration,
                           const YCbCrBuffer &aBuffer,
                           bool aKeyframe,
                           int64_t aTimecode,
                           const IntRect& aPicture);

  static VideoData* Create(VideoInfo& aInfo,
                           ImageContainer* aContainer,
                           int64_t aOffset,
                           int64_t aTime,
                           int64_t aDuration,
                           layers::TextureClient* aBuffer,
                           bool aKeyframe,
                           int64_t aTimecode,
                           const IntRect& aPicture);

  static VideoData* CreateFromImage(VideoInfo& aInfo,
                                    ImageContainer* aContainer,
                                    int64_t aOffset,
                                    int64_t aTime,
                                    int64_t aDuration,
                                    const nsRefPtr<Image>& aImage,
                                    bool aKeyframe,
                                    int64_t aTimecode,
                                    const IntRect& aPicture);

  // Creates a new VideoData identical to aOther, but with a different
  // specified duration. All data from aOther is copied into the new
  // VideoData. The new VideoData's mImage field holds a reference to
  // aOther's mImage, i.e. the Image is not copied. This function is useful
  // in reader backends that can't determine the duration of a VideoData
  // until the next frame is decoded, i.e. it's a way to change the const
  // duration field on a VideoData.
  static VideoData* ShallowCopyUpdateDuration(VideoData* aOther,
                                              int64_t aDuration);

  // Creates a new VideoData identical to aOther, but with a different
  // specified timestamp. All data from aOther is copied into the new
  // VideoData, as ShallowCopyUpdateDuration() does.
  static VideoData* ShallowCopyUpdateTimestamp(VideoData* aOther,
                                               int64_t aTimestamp);

  // Creates a new VideoData identical to aOther, but with a different
  // specified timestamp and duration. All data from aOther is copied
  // into the new VideoData, as ShallowCopyUpdateDuration() does.
  static VideoData* ShallowCopyUpdateTimestampAndDuration(VideoData* aOther,
                                                          int64_t aTimestamp,
                                                          int64_t aDuration);

  // Initialize PlanarYCbCrImage. Only When aCopyData is true,
  // video data is copied to PlanarYCbCrImage.
  static void SetVideoDataToImage(PlanarYCbCrImage* aVideoImage,
                                  VideoInfo& aInfo,
                                  const YCbCrBuffer &aBuffer,
                                  const IntRect& aPicture,
                                  bool aCopyData);

  // Constructs a duplicate VideoData object. This intrinsically tells the
  // player that it does not need to update the displayed frame when this
  // frame is played; this frame is identical to the previous.
  static VideoData* CreateDuplicate(int64_t aOffset,
                                    int64_t aTime,
                                    int64_t aDuration,
                                    int64_t aTimecode)
  {
    return new VideoData(aOffset, aTime, aDuration, aTimecode);
  }

  ~VideoData();

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

  // Dimensions at which to display the video frame. The picture region
  // will be scaled to this size. This is should be the picture region's
  // dimensions scaled with respect to its aspect ratio.
  const IntSize mDisplay;

  // Codec specific internal time code. For Ogg based codecs this is the
  // granulepos.
  const int64_t mTimecode;

  // This frame's image.
  nsRefPtr<Image> mImage;

  // When true, denotes that this frame is identical to the frame that
  // came before; it's a duplicate. mBuffer will be empty.
  const bool mDuplicate;
  const bool mKeyframe;

public:
  VideoData(int64_t aOffset,
            int64_t aTime,
            int64_t aDuration,
            int64_t aTimecode);

  VideoData(int64_t aOffset,
            int64_t aTime,
            int64_t aDuration,
            bool aKeyframe,
            int64_t aTimecode,
            IntSize aDisplay);

};

} // namespace mozilla

#endif // MediaData_h

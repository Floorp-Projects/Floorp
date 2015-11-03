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
#include "mozilla/RefPtr.h"
#include "nsTArray.h"

namespace mozilla {

namespace layers {
class Image;
class ImageContainer;
} // namespace layers

class MediaByteBuffer;
class SharedTrackInfo;

// Container that holds media samples.
class MediaData {
public:

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaData)

  enum Type {
    AUDIO_DATA = 0,
    VIDEO_DATA,
    RAW_DATA
  };

  MediaData(Type aType,
            int64_t aOffset,
            int64_t aTimestamp,
            int64_t aDuration,
            uint32_t aFrames)
    : mType(aType)
    , mOffset(aOffset)
    , mTime(aTimestamp)
    , mTimecode(aTimestamp)
    , mDuration(aDuration)
    , mFrames(aFrames)
    , mKeyframe(false)
    , mDiscontinuity(false)
  {}

  // Type of contained data.
  const Type mType;

  // Approximate byte offset where this data was demuxed from its media.
  int64_t mOffset;

  // Start time of sample, in microseconds.
  int64_t mTime;

  // Codec specific internal time code. For Ogg based codecs this is the
  // granulepos.
  int64_t mTimecode;

  // Duration of sample, in microseconds.
  int64_t mDuration;

  // Amount of frames for contained data.
  const uint32_t mFrames;

  bool mKeyframe;

  // True if this is the first sample after a gap or discontinuity in
  // the stream. This is true for the first sample in a stream after a seek.
  bool mDiscontinuity;

  int64_t GetEndTime() const { return mTime + mDuration; }

  bool AdjustForStartTime(int64_t aStartTime)
  {
    mTime = mTime - aStartTime;
    return mTime >= 0;
  }

  template <typename ReturnType>
  const ReturnType* As() const
  {
    MOZ_ASSERT(this->mType == ReturnType::sType);
    return static_cast<const ReturnType*>(this);
  }

  template <typename ReturnType>
  ReturnType* As()
  {
    MOZ_ASSERT(this->mType == ReturnType::sType);
    return static_cast<ReturnType*>(this);
  }

protected:
  MediaData(Type aType, uint32_t aFrames)
    : mType(aType)
    , mOffset(0)
    , mTime(0)
    , mTimecode(0)
    , mDuration(0)
    , mFrames(aFrames)
    , mKeyframe(false)
    , mDiscontinuity(false)
  {}

  virtual ~MediaData() {}

};

// Holds chunk a decoded audio frames.
class AudioData : public MediaData {
public:

  AudioData(int64_t aOffset,
            int64_t aTime,
            int64_t aDuration,
            uint32_t aFrames,
            AudioDataValue* aData,
            uint32_t aChannels,
            uint32_t aRate)
    : MediaData(sType, aOffset, aTime, aDuration, aFrames)
    , mChannels(aChannels)
    , mRate(aRate)
    , mAudioData(aData) {}

  static const Type sType = AUDIO_DATA;
  static const char* sTypeName;

  // Creates a new AudioData identical to aOther, but with a different
  // specified timestamp and duration. All data from aOther is copied
  // into the new AudioData but the audio data which is transferred.
  // After such call, the original aOther is unusable.
  static already_AddRefed<AudioData>
  TransferAndUpdateTimestampAndDuration(AudioData* aOther,
                                        int64_t aTimestamp,
                                        int64_t aDuration);

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

  // If mAudioBuffer is null, creates it from mAudioData.
  void EnsureAudioBuffer();

  const uint32_t mChannels;
  const uint32_t mRate;
  // At least one of mAudioBuffer/mAudioData must be non-null.
  // mChannels channels, each with mFrames frames
  RefPtr<SharedBuffer> mAudioBuffer;
  // mFrames frames, each with mChannels values
  nsAutoArrayPtr<AudioDataValue> mAudioData;

protected:
  ~AudioData() {}
};

namespace layers {
class TextureClient;
class PlanarYCbCrImage;
} // namespace layers

class VideoInfo;

// Holds a decoded video frame, in YCbCr format. These are queued in the reader.
class VideoData : public MediaData {
public:
  typedef gfx::IntRect IntRect;
  typedef gfx::IntSize IntSize;
  typedef layers::ImageContainer ImageContainer;
  typedef layers::Image Image;
  typedef layers::PlanarYCbCrImage PlanarYCbCrImage;

  static const Type sType = VIDEO_DATA;
  static const char* sTypeName;

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
  static already_AddRefed<VideoData> Create(const VideoInfo& aInfo,
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
  static already_AddRefed<VideoData> Create(const VideoInfo& aInfo,
                                            ImageContainer* aContainer,
                                            int64_t aOffset,
                                            int64_t aTime,
                                            int64_t aDuration,
                                            const YCbCrBuffer &aBuffer,
                                            bool aKeyframe,
                                            int64_t aTimecode,
                                            const IntRect& aPicture);

  // Variant to create a VideoData instance given an existing aImage
  static already_AddRefed<VideoData> Create(const VideoInfo& aInfo,
                                            Image* aImage,
                                            int64_t aOffset,
                                            int64_t aTime,
                                            int64_t aDuration,
                                            const YCbCrBuffer &aBuffer,
                                            bool aKeyframe,
                                            int64_t aTimecode,
                                            const IntRect& aPicture);

  static already_AddRefed<VideoData> Create(const VideoInfo& aInfo,
                                            ImageContainer* aContainer,
                                            int64_t aOffset,
                                            int64_t aTime,
                                            int64_t aDuration,
                                            layers::TextureClient* aBuffer,
                                            bool aKeyframe,
                                            int64_t aTimecode,
                                            const IntRect& aPicture);

  static already_AddRefed<VideoData> CreateFromImage(const VideoInfo& aInfo,
                                                     ImageContainer* aContainer,
                                                     int64_t aOffset,
                                                     int64_t aTime,
                                                     int64_t aDuration,
                                                     const RefPtr<Image>& aImage,
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
  static already_AddRefed<VideoData> ShallowCopyUpdateDuration(const VideoData* aOther,
                                                               int64_t aDuration);

  // Creates a new VideoData identical to aOther, but with a different
  // specified timestamp. All data from aOther is copied into the new
  // VideoData, as ShallowCopyUpdateDuration() does.
  static already_AddRefed<VideoData> ShallowCopyUpdateTimestamp(const VideoData* aOther,
                                                                int64_t aTimestamp);

  // Creates a new VideoData identical to aOther, but with a different
  // specified timestamp and duration. All data from aOther is copied
  // into the new VideoData, as ShallowCopyUpdateDuration() does.
  static already_AddRefed<VideoData>
  ShallowCopyUpdateTimestampAndDuration(const VideoData* aOther, int64_t aTimestamp,
                                        int64_t aDuration);

  // Initialize PlanarYCbCrImage. Only When aCopyData is true,
  // video data is copied to PlanarYCbCrImage.
  static bool SetVideoDataToImage(PlanarYCbCrImage* aVideoImage,
                                  const VideoInfo& aInfo,
                                  const YCbCrBuffer &aBuffer,
                                  const IntRect& aPicture,
                                  bool aCopyData);

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

  // Dimensions at which to display the video frame. The picture region
  // will be scaled to this size. This is should be the picture region's
  // dimensions scaled with respect to its aspect ratio.
  const IntSize mDisplay;

  // This frame's image.
  RefPtr<Image> mImage;

  int32_t mFrameID;

  bool mSentToCompositor;

  VideoData(int64_t aOffset,
            int64_t aTime,
            int64_t aDuration,
            bool aKeyframe,
            int64_t aTimecode,
            IntSize aDisplay,
            uint32_t aFrameID);

protected:
  ~VideoData();
};

class CryptoTrack
{
public:
  CryptoTrack() : mValid(false) {}
  bool mValid;
  int32_t mMode;
  int32_t mIVSize;
  nsTArray<uint8_t> mKeyId;
};

class CryptoSample : public CryptoTrack
{
public:
  nsTArray<uint16_t> mPlainSizes;
  nsTArray<uint32_t> mEncryptedSizes;
  nsTArray<uint8_t> mIV;
  nsTArray<nsCString> mSessionIds;
};


// MediaRawData is a MediaData container used to store demuxed, still compressed
// samples.
// Use MediaRawData::CreateWriter() to obtain a MediaRawDataWriter object that
// provides methods to modify and manipulate the data.
// Memory allocations are fallibles. Methods return a boolean indicating if
// memory allocations were successful. Return values should always be checked.
// MediaRawData::mData will be nullptr if no memory has been allocated or if
// an error occurred during construction.
// Existing data is only ever modified if new memory allocation has succeeded
// and preserved if not.
//
// The memory referenced by mData will always be 32 bytes aligned and the
// underlying buffer will always have a size such that 32 bytes blocks can be
// used to read the content, regardless of the mSize value. Buffer is zeroed
// on creation.
//
// Typical usage: create new MediaRawData; create the associated
// MediaRawDataWriter, call SetSize() to allocate memory, write to mData,
// up to mSize bytes.

class MediaRawData;

class MediaRawDataWriter
{
public:
  // Pointer to data or null if not-yet allocated
  uint8_t* Data();
  // Writeable size of buffer.
  size_t Size();
  // Writeable reference to MediaRawData::mCryptoInternal
  CryptoSample& mCrypto;

  // Data manipulation methods. mData and mSize may be updated accordingly.

  // Set size of buffer, allocating memory as required.
  // If size is increased, new buffer area is filled with 0.
  bool SetSize(size_t aSize);
  // Add aData at the beginning of buffer.
  bool Prepend(const uint8_t* aData, size_t aSize);
  // Replace current content with aData.
  bool Replace(const uint8_t* aData, size_t aSize);
  // Clear the memory buffer. Will set target mData and mSize to 0.
  void Clear();

private:
  friend class MediaRawData;
  explicit MediaRawDataWriter(MediaRawData* aMediaRawData);
  bool EnsureSize(size_t aSize);
  MediaRawData* mTarget;
};

class MediaRawData : public MediaData {
public:
  MediaRawData();
  MediaRawData(const uint8_t* aData, size_t mSize);

  // Pointer to data or null if not-yet allocated
  const uint8_t* Data() const { return mData; }
  // Size of buffer.
  size_t Size() const { return mSize; }
  size_t ComputedSizeOfIncludingThis() const
  {
    return sizeof(*this) + mCapacity;
  }

  const CryptoSample& mCrypto;
  RefPtr<MediaByteBuffer> mExtraData;

  RefPtr<SharedTrackInfo> mTrackInfo;

  // Return a deep copy or nullptr if out of memory.
  virtual already_AddRefed<MediaRawData> Clone() const;
  // Create a MediaRawDataWriter for this MediaRawData. The caller must
  // delete the writer once done. The writer is not thread-safe.
  virtual MediaRawDataWriter* CreateWriter();
  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

protected:
  ~MediaRawData();

private:
  friend class MediaRawDataWriter;
  // Ensure that the backend buffer can hold aSize data. Will update mData.
  // Will enforce that the start of allocated data is always 32 bytes
  // aligned and that it has sufficient end padding to allow for 32 bytes block
  // read as required by some data decoders.
  // Returns false if memory couldn't be allocated.
  bool EnsureCapacity(size_t aSize);
  uint8_t* mData;
  size_t mSize;
  nsAutoArrayPtr<uint8_t> mBuffer;
  uint32_t mCapacity;
  CryptoSample mCryptoInternal;
  MediaRawData(const MediaRawData&); // Not implemented
};

  // MediaByteBuffer is a ref counted infallible TArray.
class MediaByteBuffer : public nsTArray<uint8_t> {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaByteBuffer);
  MediaByteBuffer() = default;
  explicit MediaByteBuffer(size_t aCapacity) : nsTArray<uint8_t>(aCapacity) {}

private:
  ~MediaByteBuffer() {}
};

} // namespace mozilla

#endif // MediaData_h

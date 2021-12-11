/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(MediaData_h)
#  define MediaData_h

#  include "AudioConfig.h"
#  include "AudioSampleFormat.h"
#  include "ImageTypes.h"
#  include "SharedBuffer.h"
#  include "TimeUnits.h"
#  include "mozilla/CheckedInt.h"
#  include "mozilla/Maybe.h"
#  include "mozilla/PodOperations.h"
#  include "mozilla/RefPtr.h"
#  include "mozilla/Span.h"
#  include "mozilla/UniquePtr.h"
#  include "mozilla/UniquePtrExtensions.h"
#  include "mozilla/gfx/Rect.h"
#  include "nsString.h"
#  include "nsTArray.h"

namespace mozilla {

namespace layers {
class Image;
class ImageContainer;
class KnowsCompositor;
}  // namespace layers

class MediaByteBuffer;
class TrackInfoSharedPtr;

// AlignedBuffer:
// Memory allocations are fallibles. Methods return a boolean indicating if
// memory allocations were successful. Return values should always be checked.
// AlignedBuffer::mData will be nullptr if no memory has been allocated or if
// an error occurred during construction.
// Existing data is only ever modified if new memory allocation has succeeded
// and preserved if not.
//
// The memory referenced by mData will always be Alignment bytes aligned and the
// underlying buffer will always have a size such that Alignment bytes blocks
// can be used to read the content, regardless of the mSize value. Buffer is
// zeroed on creation, elements are not individually constructed.
// An Alignment value of 0 means that the data isn't aligned.
//
// Type must be trivially copyable.
//
// AlignedBuffer can typically be used in place of UniquePtr<Type[]> however
// care must be taken as all memory allocations are fallible.
// Example:
// auto buffer = MakeUniqueFallible<float[]>(samples)
// becomes: AlignedFloatBuffer buffer(samples)
//
// auto buffer = MakeUnique<float[]>(samples)
// becomes:
// AlignedFloatBuffer buffer(samples);
// if (!buffer) { return NS_ERROR_OUT_OF_MEMORY; }

template <typename Type, int Alignment = 32>
class AlignedBuffer {
 public:
  AlignedBuffer()
      : mData(nullptr), mLength(0), mBuffer(nullptr), mCapacity(0) {}

  explicit AlignedBuffer(size_t aLength)
      : mData(nullptr), mLength(0), mBuffer(nullptr), mCapacity(0) {
    if (EnsureCapacity(aLength)) {
      mLength = aLength;
    }
  }

  AlignedBuffer(const Type* aData, size_t aLength) : AlignedBuffer(aLength) {
    if (!mData) {
      return;
    }
    PodCopy(mData, aData, aLength);
  }

  AlignedBuffer(const AlignedBuffer& aOther)
      : AlignedBuffer(aOther.Data(), aOther.Length()) {}

  AlignedBuffer(AlignedBuffer&& aOther)
      : mData(aOther.mData),
        mLength(aOther.mLength),
        mBuffer(std::move(aOther.mBuffer)),
        mCapacity(aOther.mCapacity) {
    aOther.mData = nullptr;
    aOther.mLength = 0;
    aOther.mCapacity = 0;
  }

  AlignedBuffer& operator=(AlignedBuffer&& aOther) {
    this->~AlignedBuffer();
    new (this) AlignedBuffer(std::move(aOther));
    return *this;
  }

  Type* Data() const { return mData; }
  size_t Length() const { return mLength; }
  size_t Size() const { return mLength * sizeof(Type); }
  Type& operator[](size_t aIndex) {
    MOZ_ASSERT(aIndex < mLength);
    return mData[aIndex];
  }
  const Type& operator[](size_t aIndex) const {
    MOZ_ASSERT(aIndex < mLength);
    return mData[aIndex];
  }
  // Set length of buffer, allocating memory as required.
  // If length is increased, new buffer area is filled with 0.
  bool SetLength(size_t aLength) {
    if (aLength > mLength && !EnsureCapacity(aLength)) {
      return false;
    }
    mLength = aLength;
    return true;
  }
  // Add aData at the beginning of buffer.
  bool Prepend(const Type* aData, size_t aLength) {
    if (!EnsureCapacity(aLength + mLength)) {
      return false;
    }

    // Shift the data to the right by aLength to leave room for the new data.
    PodMove(mData + aLength, mData, mLength);
    PodCopy(mData, aData, aLength);

    mLength += aLength;
    return true;
  }
  // Add aData at the end of buffer.
  bool Append(const Type* aData, size_t aLength) {
    if (!EnsureCapacity(aLength + mLength)) {
      return false;
    }

    PodCopy(mData + mLength, aData, aLength);

    mLength += aLength;
    return true;
  }
  // Replace current content with aData.
  bool Replace(const Type* aData, size_t aLength) {
    // If aLength is smaller than our current length, we leave the buffer as is,
    // only adjusting the reported length.
    if (!EnsureCapacity(aLength)) {
      return false;
    }

    PodCopy(mData, aData, aLength);
    mLength = aLength;
    return true;
  }
  // Clear the memory buffer. Will set target mData and mLength to 0.
  void Clear() {
    mLength = 0;
    mData = nullptr;
  }

  // Methods for reporting memory.
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
    size_t size = aMallocSizeOf(this);
    size += aMallocSizeOf(mBuffer.get());
    return size;
  }
  // AlignedBuffer is typically allocated on the stack. As such, you likely
  // want to use SizeOfExcludingThis
  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(mBuffer.get());
  }
  size_t ComputedSizeOfExcludingThis() const { return mCapacity; }

  // For backward compatibility with UniquePtr<Type[]>
  Type* get() const { return mData; }
  explicit operator bool() const { return mData != nullptr; }

  // Size in bytes of extra space allocated for padding.
  static size_t AlignmentPaddingSize() { return AlignmentOffset() * 2; }

  void PopFront(size_t aCount) {
    MOZ_DIAGNOSTIC_ASSERT(mLength >= aCount, "Popping too many elements.");
    PodMove(mData, mData + aCount, mLength - aCount);
    mLength -= aCount;
  }

  void PopBack(size_t aCount) {
    MOZ_DIAGNOSTIC_ASSERT(mLength >= aCount, "Popping too many elements.");
    mLength -= aCount;
  }

 private:
  static size_t AlignmentOffset() { return Alignment ? Alignment - 1 : 0; }

  // Ensure that the backend buffer can hold aLength data. Will update mData.
  // Will enforce that the start of allocated data is always Alignment bytes
  // aligned and that it has sufficient end padding to allow for Alignment bytes
  // block read as required by some data decoders.
  // Returns false if memory couldn't be allocated.
  bool EnsureCapacity(size_t aLength) {
    if (!aLength) {
      // No need to allocate a buffer yet.
      return true;
    }
    const CheckedInt<size_t> sizeNeeded =
        CheckedInt<size_t>(aLength) * sizeof(Type) + AlignmentPaddingSize();

    if (!sizeNeeded.isValid() || sizeNeeded.value() >= INT32_MAX) {
      // overflow or over an acceptable size.
      return false;
    }
    if (mData && mCapacity >= sizeNeeded.value()) {
      return true;
    }
    auto newBuffer = MakeUniqueFallible<uint8_t[]>(sizeNeeded.value());
    if (!newBuffer) {
      return false;
    }

    // Find alignment address.
    const uintptr_t alignmask = AlignmentOffset();
    Type* newData = reinterpret_cast<Type*>(
        (reinterpret_cast<uintptr_t>(newBuffer.get()) + alignmask) &
        ~alignmask);
    MOZ_ASSERT(uintptr_t(newData) % (AlignmentOffset() + 1) == 0);

    MOZ_ASSERT(!mLength || mData);

    PodZero(newData + mLength, aLength - mLength);
    if (mLength) {
      PodCopy(newData, mData, mLength);
    }

    mBuffer = std::move(newBuffer);
    mCapacity = sizeNeeded.value();
    mData = newData;

    return true;
  }
  Type* mData;
  size_t mLength;  // number of elements
  UniquePtr<uint8_t[]> mBuffer;
  size_t mCapacity;  // in bytes
};

typedef AlignedBuffer<uint8_t> AlignedByteBuffer;
typedef AlignedBuffer<float> AlignedFloatBuffer;
typedef AlignedBuffer<int16_t> AlignedShortBuffer;
typedef AlignedBuffer<AudioDataValue> AlignedAudioBuffer;

// Container that holds media samples.
class MediaData {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaData)

  enum class Type { AUDIO_DATA = 0, VIDEO_DATA, RAW_DATA, NULL_DATA };
  static const char* TypeToStr(Type aType) {
    switch (aType) {
      case Type::AUDIO_DATA:
        return "AUDIO_DATA";
      case Type::VIDEO_DATA:
        return "VIDEO_DATA";
      case Type::RAW_DATA:
        return "RAW_DATA";
      case Type::NULL_DATA:
        return "NULL_DATA";
      default:
        MOZ_CRASH("bad value");
    }
  }

  MediaData(Type aType, int64_t aOffset, const media::TimeUnit& aTimestamp,
            const media::TimeUnit& aDuration)
      : mType(aType),
        mOffset(aOffset),
        mTime(aTimestamp),
        mTimecode(aTimestamp),
        mDuration(aDuration),
        mKeyframe(false) {}

  // Type of contained data.
  const Type mType;

  // Approximate byte offset where this data was demuxed from its media.
  int64_t mOffset;

  // Start time of sample.
  media::TimeUnit mTime;

  // Codec specific internal time code. For Ogg based codecs this is the
  // granulepos.
  media::TimeUnit mTimecode;

  // Duration of sample, in microseconds.
  media::TimeUnit mDuration;

  bool mKeyframe;

  media::TimeUnit GetEndTime() const { return mTime + mDuration; }

  media::TimeUnit GetEndTimecode() const { return mTimecode + mDuration; }

  bool HasValidTime() const {
    return mTime.IsValid() && mTimecode.IsValid() && mDuration.IsValid() &&
           GetEndTime().IsValid() && GetEndTimecode().IsValid();
  }

  template <typename ReturnType>
  const ReturnType* As() const {
    MOZ_ASSERT(this->mType == ReturnType::sType);
    return static_cast<const ReturnType*>(this);
  }

  template <typename ReturnType>
  ReturnType* As() {
    MOZ_ASSERT(this->mType == ReturnType::sType);
    return static_cast<ReturnType*>(this);
  }

 protected:
  explicit MediaData(Type aType) : mType(aType), mOffset(0), mKeyframe(false) {}

  virtual ~MediaData() = default;
};

// NullData is for decoder generating a sample which doesn't need to be
// rendered.
class NullData : public MediaData {
 public:
  NullData(int64_t aOffset, const media::TimeUnit& aTime,
           const media::TimeUnit& aDuration)
      : MediaData(Type::NULL_DATA, aOffset, aTime, aDuration) {}

  static const Type sType = Type::NULL_DATA;
};

// Holds chunk a decoded audio frames.
class AudioData : public MediaData {
 public:
  AudioData(int64_t aOffset, const media::TimeUnit& aTime,
            AlignedAudioBuffer&& aData, uint32_t aChannels, uint32_t aRate,
            uint32_t aChannelMap = AudioConfig::ChannelLayout::UNKNOWN_MAP);

  static const Type sType = Type::AUDIO_DATA;
  static const char* sTypeName;

  // Access the buffer as a Span.
  Span<AudioDataValue> Data() const;

  // Amount of frames for contained data.
  uint32_t Frames() const { return mFrames; }

  // Trim the audio buffer such that its apparent content fits within the aTrim
  // interval. The actual data isn't removed from the buffer and a followup call
  // to SetTrimWindow could restore the content. mDuration, mTime and mFrames
  // will be adjusted accordingly.
  // Warning: rounding may occurs, in which case the new start time of the audio
  // sample may still be lesser than aTrim.mStart.
  bool SetTrimWindow(const media::TimeInterval& aTrim);

  // Get the internal audio buffer to be moved. After this call the original
  // AudioData will be emptied and can't be used again.
  AlignedAudioBuffer MoveableData();

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

  // If mAudioBuffer is null, creates it from mAudioData.
  void EnsureAudioBuffer();

  // Return true if the adjusted time is valid. Caller should handle error when
  // the result is invalid.
  bool AdjustForStartTime(const media::TimeUnit& aStartTime);

  // This method is used to adjust the original start time, which would change
  //  `mTime` and `mOriginalTime` together, and should only be used for data
  // which hasn't been trimmed before.
  void SetOriginalStartTime(const media::TimeUnit& aStartTime);

  const uint32_t mChannels;
  // The AudioConfig::ChannelLayout map. Channels are ordered as per SMPTE
  // definition. A value of UNKNOWN_MAP indicates unknown layout.
  // ChannelMap is an unsigned bitmap compatible with Windows' WAVE and FFmpeg
  // channel map.
  const AudioConfig::ChannelLayout::ChannelMap mChannelMap;
  const uint32_t mRate;

  // At least one of mAudioBuffer/mAudioData must be non-null.
  // mChannels channels, each with mFrames frames
  RefPtr<SharedBuffer> mAudioBuffer;

 protected:
  ~AudioData() = default;

 private:
  friend class ArrayOfRemoteAudioData;
  AudioDataValue* GetAdjustedData() const;
  media::TimeUnit mOriginalTime;
  // mFrames frames, each with mChannels values
  AlignedAudioBuffer mAudioData;
  Maybe<media::TimeInterval> mTrimWindow;
  // Amount of frames for contained data.
  uint32_t mFrames;
  size_t mDataOffset = 0;
};

namespace layers {
class TextureClient;
class PlanarYCbCrImage;
}  // namespace layers

class VideoInfo;

// Holds a decoded video frame, in YCbCr format. These are queued in the reader.
class VideoData : public MediaData {
 public:
  typedef gfx::IntRect IntRect;
  typedef gfx::IntSize IntSize;
  typedef gfx::ColorDepth ColorDepth;
  typedef gfx::ColorRange ColorRange;
  typedef gfx::YUVColorSpace YUVColorSpace;
  typedef layers::ImageContainer ImageContainer;
  typedef layers::Image Image;
  typedef layers::PlanarYCbCrImage PlanarYCbCrImage;

  static const Type sType = Type::VIDEO_DATA;
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
      uint32_t mSkip;
    };

    Plane mPlanes[3];
    YUVColorSpace mYUVColorSpace = YUVColorSpace::Identity;
    ColorDepth mColorDepth = ColorDepth::COLOR_8;
    ColorRange mColorRange = ColorRange::LIMITED;
  };

  // Constructs a VideoData object. If aImage is nullptr, creates a new Image
  // holding a copy of the YCbCr data passed in aBuffer. If aImage is not
  // nullptr, it's stored as the underlying video image and aBuffer is assumed
  // to point to memory within aImage so no copy is made. aTimecode is a codec
  // specific number representing the timestamp of the frame of video data.
  // Returns nsnull if an error occurs. This may indicate that memory couldn't
  // be allocated to create the VideoData object, or it may indicate some
  // problem with the input data (e.g. negative stride).

  // Creates a new VideoData containing a deep copy of aBuffer. May use
  // aContainer to allocate an Image to hold the copied data.
  static already_AddRefed<VideoData> CreateAndCopyData(
      const VideoInfo& aInfo, ImageContainer* aContainer, int64_t aOffset,
      const media::TimeUnit& aTime, const media::TimeUnit& aDuration,
      const YCbCrBuffer& aBuffer, bool aKeyframe,
      const media::TimeUnit& aTimecode, const IntRect& aPicture,
      layers::KnowsCompositor* aAllocator);

  static already_AddRefed<VideoData> CreateAndCopyData(
      const VideoInfo& aInfo, ImageContainer* aContainer, int64_t aOffset,
      const media::TimeUnit& aTime, const media::TimeUnit& aDuration,
      const YCbCrBuffer& aBuffer, const YCbCrBuffer::Plane& aAlphaPlane,
      bool aKeyframe, const media::TimeUnit& aTimecode,
      const IntRect& aPicture);

  static already_AddRefed<VideoData> CreateFromImage(
      const IntSize& aDisplay, int64_t aOffset, const media::TimeUnit& aTime,
      const media::TimeUnit& aDuration, const RefPtr<Image>& aImage,
      bool aKeyframe, const media::TimeUnit& aTimecode);

  // Initialize PlanarYCbCrImage. Only When aCopyData is true,
  // video data is copied to PlanarYCbCrImage.
  static bool SetVideoDataToImage(PlanarYCbCrImage* aVideoImage,
                                  const VideoInfo& aInfo,
                                  const YCbCrBuffer& aBuffer,
                                  const IntRect& aPicture, bool aCopyData);

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

  // Dimensions at which to display the video frame. The picture region
  // will be scaled to this size. This is should be the picture region's
  // dimensions scaled with respect to its aspect ratio.
  const IntSize mDisplay;

  // This frame's image.
  RefPtr<Image> mImage;

  int32_t mFrameID;

  VideoData(int64_t aOffset, const media::TimeUnit& aTime,
            const media::TimeUnit& aDuration, bool aKeyframe,
            const media::TimeUnit& aTimecode, IntSize aDisplay,
            uint32_t aFrameID);

  void MarkSentToCompositor() { mSentToCompositor = true; }
  bool IsSentToCompositor() { return mSentToCompositor; }

  void UpdateDuration(const media::TimeUnit& aDuration);
  void UpdateTimestamp(const media::TimeUnit& aTimestamp);

  // Return true if the adjusted time is valid. Caller should handle error when
  // the result is invalid.
  bool AdjustForStartTime(const media::TimeUnit& aStartTime);

  void SetNextKeyFrameTime(const media::TimeUnit& aTime) {
    mNextKeyFrameTime = aTime;
  }

  const media::TimeUnit& NextKeyFrameTime() const { return mNextKeyFrameTime; }

 protected:
  ~VideoData();

  bool mSentToCompositor;
  media::TimeUnit mNextKeyFrameTime;
};

enum class CryptoScheme : uint8_t {
  None,
  Cenc,
  Cbcs,
};

class CryptoTrack {
 public:
  CryptoTrack()
      : mCryptoScheme(CryptoScheme::None),
        mIVSize(0),
        mCryptByteBlock(0),
        mSkipByteBlock(0) {}
  CryptoScheme mCryptoScheme;
  int32_t mIVSize;
  CopyableTArray<uint8_t> mKeyId;
  uint8_t mCryptByteBlock;
  uint8_t mSkipByteBlock;
  CopyableTArray<uint8_t> mConstantIV;

  bool IsEncrypted() const { return mCryptoScheme != CryptoScheme::None; }
};

class CryptoSample : public CryptoTrack {
 public:
  // The num clear bytes in each subsample. The nth element in the array is the
  // number of clear bytes at the start of the nth subsample.
  // Clear sizes are stored as uint16_t in containers per ISO/IEC
  // 23001-7, but we store them as uint32_t for 2 reasons
  // - The Widevine CDM accepts clear sizes as uint32_t.
  // - When converting samples to Annex B we modify the clear sizes and
  //   clear sizes near UINT16_MAX can overflow if stored in a uint16_t.
  CopyableTArray<uint32_t> mPlainSizes;
  // The num encrypted bytes in each subsample. The nth element in the array is
  // the number of encrypted bytes at the start of the nth subsample.
  CopyableTArray<uint32_t> mEncryptedSizes;
  CopyableTArray<uint8_t> mIV;
  CopyableTArray<CopyableTArray<uint8_t>> mInitDatas;
  nsString mInitDataType;
};

// MediaRawData is a MediaData container used to store demuxed, still compressed
// samples.
// Use MediaRawData::CreateWriter() to obtain a MediaRawDataWriter object that
// provides methods to modify and manipulate the data.
// Memory allocations are fallible. Methods return a boolean indicating if
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

class MediaRawDataWriter {
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
  [[nodiscard]] bool SetSize(size_t aSize);
  // Add aData at the beginning of buffer.
  [[nodiscard]] bool Prepend(const uint8_t* aData, size_t aSize);
  [[nodiscard]] bool Append(const uint8_t* aData, size_t aSize);
  // Replace current content with aData.
  [[nodiscard]] bool Replace(const uint8_t* aData, size_t aSize);
  // Clear the memory buffer. Will set target mData and mSize to 0.
  void Clear();
  // Remove aSize bytes from the front of the sample.
  void PopFront(size_t aSize);

 private:
  friend class MediaRawData;
  explicit MediaRawDataWriter(MediaRawData* aMediaRawData);
  [[nodiscard]] bool EnsureSize(size_t aSize);
  MediaRawData* mTarget;
};

class MediaRawData final : public MediaData {
 public:
  MediaRawData();
  MediaRawData(const uint8_t* aData, size_t aSize);
  MediaRawData(const uint8_t* aData, size_t aSize, const uint8_t* aAlphaData,
               size_t aAlphaSize);
  explicit MediaRawData(AlignedByteBuffer&& aData);
  MediaRawData(AlignedByteBuffer&& aData, AlignedByteBuffer&& aAlphaData);

  // Pointer to data or null if not-yet allocated
  const uint8_t* Data() const { return mBuffer.Data(); }
  // Pointer to alpha data or null if not-yet allocated
  const uint8_t* AlphaData() const { return mAlphaBuffer.Data(); }
  // Size of buffer.
  size_t Size() const { return mBuffer.Length(); }
  size_t AlphaSize() const { return mAlphaBuffer.Length(); }
  size_t ComputedSizeOfIncludingThis() const {
    return sizeof(*this) + mBuffer.ComputedSizeOfExcludingThis() +
           mAlphaBuffer.ComputedSizeOfExcludingThis();
  }
  // Access the buffer as a Span.
  operator Span<const uint8_t>() { return Span{Data(), Size()}; }

  const CryptoSample& mCrypto;
  RefPtr<MediaByteBuffer> mExtraData;

  // Used by the Vorbis decoder and Ogg demuxer.
  // Indicates that this is the last packet of the stream.
  bool mEOS = false;

  // Indicate to the audio decoder that mDiscardPadding frames should be
  // trimmed.
  uint32_t mDiscardPadding = 0;

  RefPtr<TrackInfoSharedPtr> mTrackInfo;

  // May contain the original start time and duration of the frames.
  // mOriginalPresentationWindow.mStart would always be less or equal to mTime
  // and mOriginalPresentationWindow.mEnd equal or greater to mTime + mDuration.
  // This is used when the sample should get cropped so that its content will
  // actually start on mTime and go for mDuration. If this interval is set, then
  // the decoder should crop the content accordingly.
  Maybe<media::TimeInterval> mOriginalPresentationWindow;

  // Return a deep copy or nullptr if out of memory.
  already_AddRefed<MediaRawData> Clone() const;
  // Create a MediaRawDataWriter for this MediaRawData. The writer is not
  // thread-safe.
  UniquePtr<MediaRawDataWriter> CreateWriter();
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;

 protected:
  ~MediaRawData();

 private:
  friend class MediaRawDataWriter;
  friend class ArrayOfRemoteMediaRawData;
  AlignedByteBuffer mBuffer;
  AlignedByteBuffer mAlphaBuffer;
  CryptoSample mCryptoInternal;
  MediaRawData(const MediaRawData&);  // Not implemented
};

// MediaByteBuffer is a ref counted infallible TArray.
class MediaByteBuffer : public nsTArray<uint8_t> {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaByteBuffer);
  MediaByteBuffer() = default;
  explicit MediaByteBuffer(size_t aCapacity) : nsTArray<uint8_t>(aCapacity) {}

 private:
  ~MediaByteBuffer() = default;
};

}  // namespace mozilla

#endif  // MediaData_h

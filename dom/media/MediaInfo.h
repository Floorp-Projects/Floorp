/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(MediaInfo_h)
#define MediaInfo_h

#include "mozilla/UniquePtr.h"
#include "nsRect.h"
#include "mozilla/RefPtr.h"
#include "nsSize.h"
#include "nsString.h"
#include "nsTArray.h"
#include "ImageTypes.h"
#include "MediaData.h"
#include "StreamTracks.h" // for TrackID
#include "TimeUnits.h"

namespace mozilla {

class AudioInfo;
class VideoInfo;
class TextInfo;

class MetadataTag {
public:
  MetadataTag(const nsACString& aKey,
              const nsACString& aValue)
    : mKey(aKey)
    , mValue(aValue)
  {}
  nsCString mKey;
  nsCString mValue;
};

  // Maximum channel number we can currently handle (7.1)
#define MAX_AUDIO_CHANNELS 8

class TrackInfo {
public:
  enum TrackType {
    kUndefinedTrack,
    kAudioTrack,
    kVideoTrack,
    kTextTrack
  };
  TrackInfo(TrackType aType,
            const nsAString& aId,
            const nsAString& aKind,
            const nsAString& aLabel,
            const nsAString& aLanguage,
            bool aEnabled,
            TrackID aTrackId)
    : mId(aId)
    , mKind(aKind)
    , mLabel(aLabel)
    , mLanguage(aLanguage)
    , mEnabled(aEnabled)
    , mTrackId(aTrackId)
    , mDuration(0)
    , mMediaTime(0)
    , mIsRenderedExternally(false)
    , mType(aType)
  {
    MOZ_COUNT_CTOR(TrackInfo);
  }

  // Only used for backward compatibility. Do not use in new code.
  void Init(const nsAString& aId,
            const nsAString& aKind,
            const nsAString& aLabel,
            const nsAString& aLanguage,
            bool aEnabled)
  {
    mId = aId;
    mKind = aKind;
    mLabel = aLabel;
    mLanguage = aLanguage;
    mEnabled = aEnabled;
  }

  // Fields common with MediaTrack object.
  nsString mId;
  nsString mKind;
  nsString mLabel;
  nsString mLanguage;
  bool mEnabled;

  TrackID mTrackId;

  nsCString mMimeType;
  int64_t mDuration;
  int64_t mMediaTime;
  CryptoTrack mCrypto;

  nsTArray<MetadataTag> mTags;

  // True if the track is gonna be (decrypted)/decoded and
  // rendered directly by non-gecko components.
  bool mIsRenderedExternally;

  virtual AudioInfo* GetAsAudioInfo()
  {
    return nullptr;
  }
  virtual VideoInfo* GetAsVideoInfo()
  {
    return nullptr;
  }
  virtual TextInfo* GetAsTextInfo()
  {
    return nullptr;
  }
  virtual const AudioInfo* GetAsAudioInfo() const
  {
    return nullptr;
  }
  virtual const VideoInfo* GetAsVideoInfo() const
  {
    return nullptr;
  }
  virtual const TextInfo* GetAsTextInfo() const
  {
    return nullptr;
  }

  bool IsAudio() const
  {
    return !!GetAsAudioInfo();
  }
  bool IsVideo() const
  {
    return !!GetAsVideoInfo();
  }
  bool IsText() const
  {
    return !!GetAsTextInfo();
  }
  TrackType GetType() const
  {
    return mType;
  }

  bool virtual IsValid() const = 0;

  virtual UniquePtr<TrackInfo> Clone() const = 0;

  virtual ~TrackInfo()
  {
    MOZ_COUNT_DTOR(TrackInfo);
  }

protected:
  TrackInfo(const TrackInfo& aOther)
  {
    mId = aOther.mId;
    mKind = aOther.mKind;
    mLabel = aOther.mLabel;
    mLanguage = aOther.mLanguage;
    mEnabled = aOther.mEnabled;
    mTrackId = aOther.mTrackId;
    mMimeType = aOther.mMimeType;
    mDuration = aOther.mDuration;
    mMediaTime = aOther.mMediaTime;
    mCrypto = aOther.mCrypto;
    mIsRenderedExternally = aOther.mIsRenderedExternally;
    mType = aOther.mType;
    mTags = aOther.mTags;
    MOZ_COUNT_CTOR(TrackInfo);
  }

private:
  TrackType mType;
};

// Stores info relevant to presenting media frames.
class VideoInfo : public TrackInfo {
public:
  enum Rotation {
    kDegree_0 = 0,
    kDegree_90 = 90,
    kDegree_180 = 180,
    kDegree_270 = 270,
  };
  VideoInfo()
    : VideoInfo(-1, -1)
  {
  }

  explicit VideoInfo(int32_t aWidth, int32_t aHeight)
    : VideoInfo(nsIntSize(aWidth, aHeight))
  {
  }

  explicit VideoInfo(const nsIntSize& aSize)
    : TrackInfo(kVideoTrack, NS_LITERAL_STRING("2"), NS_LITERAL_STRING("main"),
                EmptyString(), EmptyString(), true, 2)
    , mDisplay(aSize)
    , mStereoMode(StereoMode::MONO)
    , mImage(aSize)
    , mCodecSpecificConfig(new MediaByteBuffer)
    , mExtraData(new MediaByteBuffer)
    , mRotation(kDegree_0)
    , mImageRect(nsIntRect(nsIntPoint(), aSize))
  {
  }

  VideoInfo(const VideoInfo& aOther)
    : TrackInfo(aOther)
    , mDisplay(aOther.mDisplay)
    , mStereoMode(aOther.mStereoMode)
    , mImage(aOther.mImage)
    , mCodecSpecificConfig(aOther.mCodecSpecificConfig)
    , mExtraData(aOther.mExtraData)
    , mRotation(aOther.mRotation)
    , mImageRect(aOther.mImageRect)
  {
  }

  bool IsValid() const override
  {
    return mDisplay.width > 0 && mDisplay.height > 0;
  }

  VideoInfo* GetAsVideoInfo() override
  {
    return this;
  }

  const VideoInfo* GetAsVideoInfo() const override
  {
    return this;
  }

  UniquePtr<TrackInfo> Clone() const override
  {
    return MakeUnique<VideoInfo>(*this);
  }

  nsIntRect ImageRect() const
  {
    if (mImageRect.width < 0 || mImageRect.height < 0) {
      return nsIntRect(0, 0, mImage.width, mImage.height);
    }
    return mImageRect;
  }

  void SetImageRect(const nsIntRect& aRect)
  {
    mImageRect = aRect;
  }

  // Returned the crop rectangle scaled to aWidth/aHeight size relative to
  // mImage size.
  // If aWidth and aHeight are identical to the original mImage.width/mImage.height
  // then the scaling ratio will be 1.
  // This is used for when the frame size is different from what the container
  // reports. This is legal in WebM, and we will preserve the ratio of the crop
  // rectangle as it was reported relative to the picture size reported by the
  // container.
  nsIntRect ScaledImageRect(int64_t aWidth, int64_t aHeight) const
  {
    if ((aWidth == mImage.width && aHeight == mImage.height) ||
        !mImage.width || !mImage.height) {
      return ImageRect();
    }
    nsIntRect imageRect = ImageRect();
    imageRect.x = (imageRect.x * aWidth) / mImage.width;
    imageRect.y = (imageRect.y * aHeight) / mImage.height;
    imageRect.width = (aWidth * imageRect.width) / mImage.width;
    imageRect.height = (aHeight * imageRect.height) / mImage.height;
    return imageRect;
  }

  Rotation ToSupportedRotation(int32_t aDegree)
  {
    switch (aDegree) {
      case 90:
        return kDegree_90;
      case 180:
        return kDegree_180;
      case 270:
        return kDegree_270;
      default:
        NS_WARNING_ASSERTION(aDegree == 0, "Invalid rotation degree, ignored");
        return kDegree_0;
    }
  }

  // Size in pixels at which the video is rendered. This is after it has
  // been scaled by its aspect ratio.
  nsIntSize mDisplay;

  // Indicates the frame layout for single track stereo videos.
  StereoMode mStereoMode;

  // Size of the decoded video's image.
  nsIntSize mImage;

  RefPtr<MediaByteBuffer> mCodecSpecificConfig;
  RefPtr<MediaByteBuffer> mExtraData;

  // Describing how many degrees video frames should be rotated in clock-wise to
  // get correct view.
  Rotation mRotation;

private:
  // mImage may be cropped; currently only used with the WebM container.
  // A negative width or height indicate that no cropping is to occur.
  nsIntRect mImageRect;
};

class AudioInfo : public TrackInfo {
public:
  AudioInfo()
    : TrackInfo(kAudioTrack, NS_LITERAL_STRING("1"), NS_LITERAL_STRING("main"),
                EmptyString(), EmptyString(), true, 1)
    , mRate(0)
    , mChannels(0)
    , mBitDepth(0)
    , mProfile(0)
    , mExtendedProfile(0)
    , mCodecSpecificConfig(new MediaByteBuffer)
    , mExtraData(new MediaByteBuffer)
  {
  }

  AudioInfo(const AudioInfo& aOther)
    : TrackInfo(aOther)
    , mRate(aOther.mRate)
    , mChannels(aOther.mChannels)
    , mBitDepth(aOther.mBitDepth)
    , mProfile(aOther.mProfile)
    , mExtendedProfile(aOther.mExtendedProfile)
    , mCodecSpecificConfig(aOther.mCodecSpecificConfig)
    , mExtraData(aOther.mExtraData)
  {
  }

  static const uint32_t MAX_RATE = 640000;

  bool IsValid() const override
  {
    return mChannels > 0 && mChannels <= MAX_AUDIO_CHANNELS
           && mRate > 0 && mRate <= MAX_RATE;
  }

  AudioInfo* GetAsAudioInfo() override
  {
    return this;
  }

  const AudioInfo* GetAsAudioInfo() const override
  {
    return this;
  }

  UniquePtr<TrackInfo> Clone() const override
  {
    return MakeUnique<AudioInfo>(*this);
  }

  // Sample rate.
  uint32_t mRate;

  // Number of audio channels.
  uint32_t mChannels;

  // Bits per sample.
  uint32_t mBitDepth;

  // Codec profile.
  int8_t mProfile;

  // Extended codec profile.
  int8_t mExtendedProfile;

  RefPtr<MediaByteBuffer> mCodecSpecificConfig;
  RefPtr<MediaByteBuffer> mExtraData;

};

class EncryptionInfo {
public:
  EncryptionInfo()
    : mEncrypted(false)
  {
  }

  struct InitData {
    template<typename AInitDatas>
    InitData(const nsAString& aType, AInitDatas&& aInitData)
      : mType(aType)
      , mInitData(Forward<AInitDatas>(aInitData))
    {
    }

    // Encryption type to be passed to JS. Usually `cenc'.
    nsString mType;

    // Encryption data.
    nsTArray<uint8_t> mInitData;
  };
  typedef nsTArray<InitData> InitDatas;

  // True if the stream has encryption metadata
  bool IsEncrypted() const
  {
    return mEncrypted;
  }

  template<typename AInitDatas>
  void AddInitData(const nsAString& aType, AInitDatas&& aInitData)
  {
    mInitDatas.AppendElement(InitData(aType, Forward<AInitDatas>(aInitData)));
    mEncrypted = true;
  }

  void AddInitData(const EncryptionInfo& aInfo)
  {
    mInitDatas.AppendElements(aInfo.mInitDatas);
    mEncrypted = !!mInitDatas.Length();
  }

  // One 'InitData' per encrypted buffer.
  InitDatas mInitDatas;
private:
  bool mEncrypted;
};

class MediaInfo {
public:
  bool HasVideo() const
  {
    return mVideo.IsValid();
  }

  void EnableVideo()
  {
    if (HasVideo()) {
      return;
    }
    // Set dummy values so that HasVideo() will return true;
    // See VideoInfo::IsValid()
    mVideo.mDisplay = nsIntSize(1, 1);
  }

  bool HasAudio() const
  {
    return mAudio.IsValid();
  }

  void EnableAudio()
  {
    if (HasAudio()) {
      return;
    }
    // Set dummy values so that HasAudio() will return true;
    // See AudioInfo::IsValid()
    mAudio.mChannels = 2;
    mAudio.mRate = 44100;
  }

  bool IsEncrypted() const
  {
    return (HasAudio() && mAudio.mCrypto.mValid) ||
           (HasVideo() && mVideo.mCrypto.mValid);
  }

  bool HasValidMedia() const
  {
    return HasVideo() || HasAudio();
  }

  void AssertValid() const
  {
    NS_ASSERTION(!HasAudio() || mAudio.mTrackId != TRACK_INVALID,
                 "Audio track ID must be valid");
    NS_ASSERTION(!HasVideo() || mVideo.mTrackId != TRACK_INVALID,
                 "Audio track ID must be valid");
    NS_ASSERTION(!HasAudio() || !HasVideo() ||
                 mAudio.mTrackId != mVideo.mTrackId,
                 "Duplicate track IDs");
  }

  // TODO: Store VideoInfo and AudioIndo in arrays to support multi-tracks.
  VideoInfo mVideo;
  AudioInfo mAudio;

  // If the metadata includes a duration, we store it here.
  media::NullableTimeUnit mMetadataDuration;

  // The Ogg reader tries to kinda-sorta compute the duration by seeking to the
  // end and determining the timestamp of the last frame. This isn't useful as
  // a duration until we know the start time, so we need to track it separately.
  media::NullableTimeUnit mUnadjustedMetadataEndTime;

  // True if the media is seekable (i.e. supports random access).
  bool mMediaSeekable = true;

  // True if the media is only seekable within its buffered ranges.
  bool mMediaSeekableOnlyInBufferedRanges = false;

  EncryptionInfo mCrypto;
};

class SharedTrackInfo {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SharedTrackInfo)
public:
  SharedTrackInfo(const TrackInfo& aOriginal, uint32_t aStreamID)
    : mInfo(aOriginal.Clone())
    , mStreamSourceID(aStreamID)
    , mMimeType(mInfo->mMimeType)
  {
  }

  uint32_t GetID() const
  {
    return mStreamSourceID;
  }

  const TrackInfo* operator*() const
  {
    return mInfo.get();
  }

  const TrackInfo* operator->() const
  {
    MOZ_ASSERT(mInfo.get(), "dereferencing a UniquePtr containing nullptr");
    return mInfo.get();
  }

  const AudioInfo* GetAsAudioInfo() const
  {
    return mInfo ? mInfo->GetAsAudioInfo() : nullptr;
  }

  const VideoInfo* GetAsVideoInfo() const
  {
    return mInfo ? mInfo->GetAsVideoInfo() : nullptr;
  }

  const TextInfo* GetAsTextInfo() const
  {
    return mInfo ? mInfo->GetAsTextInfo() : nullptr;
  }

private:
  ~SharedTrackInfo() {};
  UniquePtr<TrackInfo> mInfo;
  // A unique ID, guaranteed to change when changing streams.
  uint32_t mStreamSourceID;

public:
  const nsCString& mMimeType;
};

class AudioConfig {
public:
  enum Channel {
    CHANNEL_INVALID = -1,
    CHANNEL_MONO = 0,
    CHANNEL_LEFT,
    CHANNEL_RIGHT,
    CHANNEL_CENTER,
    CHANNEL_LS,
    CHANNEL_RS,
    CHANNEL_RLS,
    CHANNEL_RCENTER,
    CHANNEL_RRS,
    CHANNEL_LFE,
  };

  class ChannelLayout {
  public:
    ChannelLayout()
      : mChannelMap(0)
      , mValid(false)
    {}
    explicit ChannelLayout(uint32_t aChannels)
      : ChannelLayout(aChannels, SMPTEDefault(aChannels))
    {}
    ChannelLayout(uint32_t aChannels, const Channel* aConfig)
      : ChannelLayout()
    {
      if (!aConfig) {
        mValid = false;
        return;
      }
      mChannels.AppendElements(aConfig, aChannels);
      UpdateChannelMap();
    }
    bool operator==(const ChannelLayout& aOther) const
    {
      return mChannels == aOther.mChannels;
    }
    bool operator!=(const ChannelLayout& aOther) const
    {
      return mChannels != aOther.mChannels;
    }
    const Channel& operator[](uint32_t aIndex) const
    {
      return mChannels[aIndex];
    }
    uint32_t Count() const
    {
      return mChannels.Length();
    }
    uint32_t Map() const
    {
      return mChannelMap;
    }
    // Calculate the mapping table from the current layout to aOther such that
    // one can easily go from one layout to the other by doing:
    // out[channel] = in[map[channel]].
    // Returns true if the reordering is possible or false otherwise.
    // If true, then aMap, if set, will be updated to contain the mapping table
    // allowing conversion from the current layout to aOther.
    // If aMap is nullptr, then MappingTable can be used to simply determine if
    // the current layout can be easily reordered to aOther.
    // aMap must be an array of size MAX_AUDIO_CHANNELS.
    bool MappingTable(const ChannelLayout& aOther, uint8_t* aMap = nullptr) const;
    bool IsValid() const {
      return mValid;
    }
    bool HasChannel(Channel aChannel) const
    {
      return mChannelMap & (1 << aChannel);
    }
  private:
    void UpdateChannelMap();
    const Channel* SMPTEDefault(uint32_t aChannels) const;
    AutoTArray<Channel, MAX_AUDIO_CHANNELS> mChannels;
    uint32_t mChannelMap;
    bool mValid;
  };

  enum SampleFormat {
    FORMAT_NONE = 0,
    FORMAT_U8,
    FORMAT_S16,
    FORMAT_S24LSB,
    FORMAT_S24,
    FORMAT_S32,
    FORMAT_FLT,
#if defined(MOZ_SAMPLE_TYPE_FLOAT32)
    FORMAT_DEFAULT = FORMAT_FLT
#elif defined(MOZ_SAMPLE_TYPE_S16)
    FORMAT_DEFAULT = FORMAT_S16
#else
#error "Not supported audio type"
#endif
  };

  AudioConfig(const ChannelLayout& aChannelLayout, uint32_t aRate,
              AudioConfig::SampleFormat aFormat = FORMAT_DEFAULT,
              bool aInterleaved = true);
  // Will create a channel configuration from default SMPTE ordering.
  AudioConfig(uint32_t aChannels, uint32_t aRate,
              AudioConfig::SampleFormat aFormat = FORMAT_DEFAULT,
              bool aInterleaved = true);

  const ChannelLayout& Layout() const
  {
    return mChannelLayout;
  }
  uint32_t Channels() const
  {
    if (!mChannelLayout.IsValid()) {
      return mChannels;
    }
    return mChannelLayout.Count();
  }
  uint32_t Rate() const
  {
    return mRate;
  }
  SampleFormat Format() const
  {
    return mFormat;
  }
  bool Interleaved() const
  {
    return mInterleaved;
  }
  bool operator==(const AudioConfig& aOther) const
  {
    return mChannelLayout == aOther.mChannelLayout &&
      mRate == aOther.mRate && mFormat == aOther.mFormat &&
      mInterleaved == aOther.mInterleaved;
  }
  bool operator!=(const AudioConfig& aOther) const
  {
    return !(*this == aOther);
  }

  bool IsValid() const
  {
    return mChannelLayout.IsValid() && Format() != FORMAT_NONE && Rate() > 0;
  }

  static const char* FormatToString(SampleFormat aFormat);
  static uint32_t SampleSize(SampleFormat aFormat);
  static uint32_t FormatToBits(SampleFormat aFormat);

private:
  // Channels configuration.
  ChannelLayout mChannelLayout;

  // Channel count.
  uint32_t mChannels;

  // Sample rate.
  uint32_t mRate;

  // Sample format.
  SampleFormat mFormat;

  bool mInterleaved;
};

} // namespace mozilla

#endif // MediaInfo_h

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
#include "StreamBuffer.h" // for TrackID
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
  VideoInfo()
    : VideoInfo(-1, -1)
  {
  }

  VideoInfo(int32_t aWidth, int32_t aHeight)
    : TrackInfo(kVideoTrack, NS_LITERAL_STRING("2"), NS_LITERAL_STRING("main"),
                EmptyString(), EmptyString(), true, 2)
    , mDisplay(nsIntSize(aWidth, aHeight))
    , mStereoMode(StereoMode::MONO)
    , mImage(nsIntSize(aWidth, aHeight))
    , mCodecSpecificConfig(new MediaByteBuffer)
    , mExtraData(new MediaByteBuffer)
    , mImageRect(nsIntRect(0, 0, aWidth, aHeight))
  {
  }

  VideoInfo(const VideoInfo& aOther)
    : TrackInfo(aOther)
    , mDisplay(aOther.mDisplay)
    , mStereoMode(aOther.mStereoMode)
    , mImage(aOther.mImage)
    , mCodecSpecificConfig(aOther.mCodecSpecificConfig)
    , mExtraData(aOther.mExtraData)
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
    if (aWidth == mImage.width && aHeight == mImage.height) {
      return ImageRect();
    }
    nsIntRect imageRect = ImageRect();
    imageRect.x = (imageRect.x * aWidth) / mImage.width;
    imageRect.y = (imageRect.y * aHeight) / mImage.height;
    imageRect.width = (aWidth * imageRect.width) / mImage.width;
    imageRect.height = (aHeight * imageRect.height) / mImage.height;
    return imageRect;
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

  bool IsValid() const override
  {
    return mChannels > 0 && mRate > 0;
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
    return mCrypto.IsEncrypted();
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

} // namespace mozilla

#endif // MediaInfo_h

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(MediaInfo_h)
#define MediaInfo_h

#include "nsSize.h"
#include "nsRect.h"
#include "ImageTypes.h"
#include "nsString.h"
#include "nsTArray.h"
#include "StreamBuffer.h" // for TrackID

namespace mozilla {

struct TrackInfo {
  void Init(const nsAString& aId,
            const nsAString& aKind,
            const nsAString& aLabel,
            const nsAString& aLanguage,
            bool aEnabled,
            TrackID aOutputId = TRACK_INVALID)
  {
    mId = aId;
    mKind = aKind;
    mLabel = aLabel;
    mLanguage = aLanguage;
    mEnabled = aEnabled;
    mOutputId = aOutputId;
  }

  nsString mId;
  nsString mKind;
  nsString mLabel;
  nsString mLanguage;
  bool mEnabled;
  TrackID mOutputId;
};

// Stores info relevant to presenting media frames.
class VideoInfo {
private:
  void Init(int32_t aWidth, int32_t aHeight, bool aHasVideo)
  {
    mDisplay = nsIntSize(aWidth, aHeight);
    mStereoMode = StereoMode::MONO;
    mHasVideo = aHasVideo;

    // TODO: TrackInfo should be initialized by its specific codec decoder.
    // This following call should be removed once we have that implemented.
    mTrackInfo.Init(NS_LITERAL_STRING("2"), NS_LITERAL_STRING("main"),
                    EmptyString(), EmptyString(), true, 2);
  }

public:
  VideoInfo()
  {
    Init(0, 0, false);
  }

  VideoInfo(int32_t aWidth, int32_t aHeight)
  {
    Init(aWidth, aHeight, true);
  }

  // Size in pixels at which the video is rendered. This is after it has
  // been scaled by its aspect ratio.
  nsIntSize mDisplay;

  // Indicates the frame layout for single track stereo videos.
  StereoMode mStereoMode;

  // True if we have an active video bitstream.
  bool mHasVideo;

  TrackInfo mTrackInfo;
};

class AudioInfo {
public:
  AudioInfo()
    : mRate(44100)
    , mChannels(2)
    , mHasAudio(false)
  {
    // TODO: TrackInfo should be initialized by its specific codec decoder.
    // This following call should be removed once we have that implemented.
    mTrackInfo.Init(NS_LITERAL_STRING("1"), NS_LITERAL_STRING("main"),
                    EmptyString(), EmptyString(), true, 1);
  }

  // Sample rate.
  uint32_t mRate;

  // Number of audio channels.
  uint32_t mChannels;

  // True if we have an active audio bitstream.
  bool mHasAudio;

  TrackInfo mTrackInfo;
};

class EncryptionInfo {
public:
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
    return !mInitDatas.IsEmpty();
  }

  template<typename AInitDatas>
  void AddInitData(const nsAString& aType, AInitDatas&& aInitData)
  {
    mInitDatas.AppendElement(InitData(aType, Forward<AInitDatas>(aInitData)));
  }

  void AddInitData(const EncryptionInfo& aInfo)
  {
    mInitDatas.AppendElements(aInfo.mInitDatas);
  }

  // One 'InitData' per encrypted buffer.
  InitDatas mInitDatas;
};

class MediaInfo {
public:
  bool HasVideo() const
  {
    return mVideo.mHasVideo;
  }

  bool HasAudio() const
  {
    return mAudio.mHasAudio;
  }

  bool IsEncrypted() const
  {
    return mCrypto.IsEncrypted();
  }

  bool HasValidMedia() const
  {
    return HasVideo() || HasAudio();
  }

  // TODO: Store VideoInfo and AudioIndo in arrays to support multi-tracks.
  VideoInfo mVideo;
  AudioInfo mAudio;

  EncryptionInfo mCrypto;
};

} // namespace mozilla

#endif // MediaInfo_h

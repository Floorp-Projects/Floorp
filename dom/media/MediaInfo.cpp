/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaInfo.h"
#include "MediaData.h"

namespace mozilla {

const char* TrackTypeToStr(TrackInfo::TrackType aTrack) {
  switch (aTrack) {
    case TrackInfo::kUndefinedTrack:
      return "Undefined";
    case TrackInfo::kAudioTrack:
      return "Audio";
    case TrackInfo::kVideoTrack:
      return "Video";
    case TrackInfo::kTextTrack:
      return "Text";
    default:
      return "Unknown";
  }
}

bool TrackInfo::IsEqualTo(const TrackInfo& rhs) const {
  return (
      mId == rhs.mId && mKind == rhs.mKind && mLabel == rhs.mLabel &&
      mLanguage == rhs.mLanguage && mEnabled == rhs.mEnabled &&
      mTrackId == rhs.mTrackId && mMimeType == rhs.mMimeType &&
      mDuration == rhs.mDuration && mMediaTime == rhs.mMediaTime &&
      mCrypto.mCryptoScheme == rhs.mCrypto.mCryptoScheme &&
      mCrypto.mIVSize == rhs.mCrypto.mIVSize &&
      mCrypto.mKeyId == rhs.mCrypto.mKeyId &&
      mCrypto.mCryptByteBlock == rhs.mCrypto.mCryptByteBlock &&
      mCrypto.mSkipByteBlock == rhs.mCrypto.mSkipByteBlock &&
      mCrypto.mConstantIV == rhs.mCrypto.mConstantIV && mTags == rhs.mTags &&
      mIsRenderedExternally == rhs.mIsRenderedExternally && mType == rhs.mType);
}

nsCString TrackInfo::ToString() const {
  nsCString rv;

  rv.AppendPrintf(
      "TrackInfo: id:%s kind:%s label:%s language:%s enabled:%s trackid: %d "
      "mimetype:%s duration:%s media time:%s crypto:%s rendered externaly: %s "
      "type:%s",
      NS_ConvertUTF16toUTF8(mId).get(), NS_ConvertUTF16toUTF8(mKind).get(),
      NS_ConvertUTF16toUTF8(mLabel).get(),
      NS_ConvertUTF16toUTF8(mLanguage).get(), mEnabled ? "true" : "false",
      mTrackId, mMimeType.get(), mDuration.ToString().get(),
      mMediaTime.ToString().get(), CryptoSchemeToString(mCrypto.mCryptoScheme),
      mIsRenderedExternally ? "true" : "false", TrackTypeToStr(mType));

  if (!mTags.IsEmpty()) {
    rv.AppendPrintf("\n");
    for (const auto& tag : mTags) {
      rv.AppendPrintf("%s:%s", tag.mKey.get(), tag.mValue.get());
    }
  }

  return rv;
}

bool VideoInfo::operator==(const VideoInfo& rhs) const {
  return (TrackInfo::IsEqualTo(rhs) && mDisplay == rhs.mDisplay &&
          mStereoMode == rhs.mStereoMode && mImage == rhs.mImage &&
          *mCodecSpecificConfig == *rhs.mCodecSpecificConfig &&
          *mExtraData == *rhs.mExtraData && mRotation == rhs.mRotation &&
          mColorDepth == rhs.mColorDepth && mImageRect == rhs.mImageRect &&
          mAlphaPresent == rhs.mAlphaPresent);
}

bool AudioInfo::operator==(const AudioInfo& rhs) const {
  return (TrackInfo::IsEqualTo(rhs) && mRate == rhs.mRate &&
          mChannels == rhs.mChannels && mChannelMap == rhs.mChannelMap &&
          mBitDepth == rhs.mBitDepth && mProfile == rhs.mProfile &&
          mExtendedProfile == rhs.mExtendedProfile &&
          mCodecSpecificConfig == rhs.mCodecSpecificConfig);
}

nsCString AudioInfo::ToString() const {
  nsCString rv;

  rv.AppendPrintf(
      "AudioInfo: %" PRIu32 "Hz, %" PRIu32 "ch (%s) %" PRIu32
      "-bits profile: %" PRIu8 " extended profile: %" PRIu8 ", %s extradata",
      mRate, mChannels,
      AudioConfig::ChannelLayout::ChannelMapToString(mChannelMap).get(),
      mBitDepth, mProfile, mExtendedProfile,
      mCodecSpecificConfig.is<NoCodecSpecificData>() ? "no" : "with");

  return rv;
}

}  // namespace mozilla

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaInfo.h"

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

}  // namespace mozilla

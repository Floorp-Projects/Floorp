/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaEnginePrefs_h
#define MediaEnginePrefs_h

#include <stdint.h>
#include <string.h>

namespace mozilla {

/**
 * Video source and friends.
 */
class MediaEnginePrefs {
 public:
  static const int DEFAULT_VIDEO_FPS = 30;
  static const int DEFAULT_43_VIDEO_WIDTH = 640;
  static const int DEFAULT_43_VIDEO_HEIGHT = 480;
  static const int DEFAULT_169_VIDEO_WIDTH = 1280;
  static const int DEFAULT_169_VIDEO_HEIGHT = 720;

  MediaEnginePrefs()
      : mWidth(0),
        mHeight(0),
        mFPS(0),
        mFreq(0),
        mAecOn(false),
        mUseAecMobile(false),
        mAgcOn(false),
        mHPFOn(false),
        mNoiseOn(false),
        mTransientOn(false),
        mAgc2Forced(false),
        mAgc(0),
        mNoise(0),
        mChannels(0) {}

  int32_t mWidth;
  int32_t mHeight;
  int32_t mFPS;
  int32_t mFreq;  // for test tones (fake:true)
  bool mAecOn;
  bool mUseAecMobile;
  bool mAgcOn;
  bool mHPFOn;
  bool mNoiseOn;
  bool mTransientOn;
  bool mAgc2Forced;
  int32_t mAgc;
  int32_t mNoise;
  int32_t mChannels;

  bool operator==(const MediaEnginePrefs& aRhs) {
    return memcmp(this, &aRhs, sizeof(MediaEnginePrefs)) == 0;
  };

  // mWidth and/or mHeight may be zero (=adaptive default), so use functions.

  int32_t GetWidth(bool aHD = false) const {
    return mWidth ? mWidth
                  : (mHeight ? (mHeight * GetDefWidth(aHD)) / GetDefHeight(aHD)
                             : GetDefWidth(aHD));
  }

  int32_t GetHeight(bool aHD = false) const {
    return mHeight ? mHeight
                   : (mWidth ? (mWidth * GetDefHeight(aHD)) / GetDefWidth(aHD)
                             : GetDefHeight(aHD));
  }

 private:
  static int32_t GetDefWidth(bool aHD = false) {
    // It'd be nice if we could use the ternary operator here, but we can't
    // because of bug 1002729.
    if (aHD) {
      return DEFAULT_169_VIDEO_WIDTH;
    }

    return DEFAULT_43_VIDEO_WIDTH;
  }

  static int32_t GetDefHeight(bool aHD = false) {
    // It'd be nice if we could use the ternary operator here, but we can't
    // because of bug 1002729.
    if (aHD) {
      return DEFAULT_169_VIDEO_HEIGHT;
    }

    return DEFAULT_43_VIDEO_HEIGHT;
  }
};

}  // namespace mozilla

#endif  // MediaEnginePrefs_h

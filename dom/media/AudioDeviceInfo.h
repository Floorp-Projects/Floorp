/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_AudioDeviceInfo_H
#define MOZILLA_AudioDeviceInfo_H

#include "nsIAudioDeviceInfo.h"
#include "nsString.h"

// This is mapped to the cubeb_device_info.
class AudioDeviceInfo final : public nsIAudioDeviceInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIAUDIODEVICEINFO

  explicit AudioDeviceInfo(const nsAString& aName,
                           const nsAString& aGroupId,
                           const nsAString& aVendor,
                           uint16_t aType,
                           uint16_t aState,
                           uint16_t aPreferred,
                           uint16_t aSupportedFormat,
                           uint16_t aDefaultFormat,
                           uint32_t aMaxChannels,
                           uint32_t aDefaultRate,
                           uint32_t aMaxRate,
                           uint32_t aMinRate,
                           uint32_t aMaxLatency,
                           uint32_t aMinLatency);

private:
  virtual ~AudioDeviceInfo() = default;

  nsString mName;
  nsString mGroupId;
  nsString mVendor;
  uint16_t mType;
  uint16_t mState;
  uint16_t mPreferred;
  uint16_t mSupportedFormat;
  uint16_t mDefaultFormat;
  uint32_t mMaxChannels;
  uint32_t mDefaultRate;
  uint32_t mMaxRate;
  uint32_t mMinRate;
  uint32_t mMaxLatency;
  uint32_t mMinLatency;
};

#endif // MOZILLA_AudioDeviceInfo_H

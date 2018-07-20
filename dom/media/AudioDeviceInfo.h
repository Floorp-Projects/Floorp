/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_AudioDeviceInfo_H
#define MOZILLA_AudioDeviceInfo_H

#include "nsIAudioDeviceInfo.h"
#include "CubebUtils.h"
#include "nsString.h"
#include "mozilla/Maybe.h"

// This is mapped to the cubeb_device_info.
class AudioDeviceInfo final : public nsIAudioDeviceInfo
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIAUDIODEVICEINFO

  using AudioDeviceID = mozilla::CubebUtils::AudioDeviceID;

  AudioDeviceInfo(const AudioDeviceID aID,
                  const nsAString& aName,
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

  explicit AudioDeviceInfo(cubeb_device_info* aInfo);
  // It is possible to not know the device identifier here. It depends on which
  // ctor this instance has been constructed with.
  mozilla::Maybe<AudioDeviceID> GetDeviceID();
  const nsString& FriendlyName();
  uint32_t MaxChannels();
  uint32_t Type();
  uint32_t State();
private:
  virtual ~AudioDeviceInfo() = default;

  const AudioDeviceID mDeviceId;
  const nsString mName;
  const nsString mGroupId;
  const nsString mVendor;
  const uint16_t mType;
  const uint16_t mState;
  const uint16_t mPreferred;
  const uint16_t mSupportedFormat;
  const uint16_t mDefaultFormat;
  const uint32_t mMaxChannels;
  const uint32_t mDefaultRate;
  const uint32_t mMaxRate;
  const uint32_t mMinRate;
  const uint32_t mMaxLatency;
  const uint32_t mMinLatency;
};

#endif // MOZILLA_AudioDeviceInfo_H

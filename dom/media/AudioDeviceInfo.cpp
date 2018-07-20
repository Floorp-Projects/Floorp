/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioDeviceInfo.h"

NS_IMPL_ISUPPORTS(AudioDeviceInfo, nsIAudioDeviceInfo)

using namespace mozilla;
using namespace mozilla::CubebUtils;

AudioDeviceInfo::AudioDeviceInfo(cubeb_device_info* aInfo)
:AudioDeviceInfo(aInfo->devid,
                 NS_ConvertUTF8toUTF16(aInfo->friendly_name),
                 NS_ConvertUTF8toUTF16(aInfo->group_id),
                 NS_ConvertUTF8toUTF16(aInfo->vendor_name),
                 aInfo->type,
                 aInfo->state,
                 aInfo->preferred,
                 aInfo->format,
                 aInfo->default_format,
                 aInfo->max_channels,
                 aInfo->default_rate,
                 aInfo->max_rate,
                 aInfo->min_rate,
                 aInfo->latency_lo,
                 aInfo->latency_hi)
{
}


AudioDeviceInfo::AudioDeviceInfo(AudioDeviceID aID,
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
                                 uint32_t aMinLatency)
  : mDeviceId(aID)
  , mName(aName)
  , mGroupId(aGroupId)
  , mVendor(aVendor)
  , mType(aType)
  , mState(aState)
  , mPreferred(aPreferred)
  , mSupportedFormat(aSupportedFormat)
  , mDefaultFormat(aDefaultFormat)
  , mMaxChannels(aMaxChannels)
  , mDefaultRate(aDefaultRate)
  , mMaxRate(aMaxRate)
  , mMinRate(aMinRate)
  , mMaxLatency(aMaxLatency)
  , mMinLatency(aMinLatency)
{
  MOZ_ASSERT(mType == TYPE_UNKNOWN ||
             mType == TYPE_INPUT ||
             mType == TYPE_OUTPUT, "Wrong type");
  MOZ_ASSERT(mState == STATE_DISABLED ||
             mState == STATE_UNPLUGGED ||
             mState == STATE_ENABLED, "Wrong state");
  MOZ_ASSERT(mPreferred == PREF_NONE ||
             mPreferred == PREF_ALL ||
             mPreferred & (PREF_MULTIMEDIA | PREF_VOICE | PREF_NOTIFICATION),
             "Wrong preferred value");
  MOZ_ASSERT(mSupportedFormat & (FMT_S16LE | FMT_S16BE | FMT_F32LE | FMT_F32BE),
             "Wrong supported format");
  MOZ_ASSERT(mDefaultFormat == FMT_S16LE ||
             mDefaultFormat == FMT_S16BE ||
             mDefaultFormat == FMT_F32LE ||
             mDefaultFormat == FMT_F32BE, "Wrong default format");
}

Maybe<AudioDeviceID>
AudioDeviceInfo::GetDeviceID()
{
  if (mDeviceId) {
    return Some(mDeviceId);
  }
  return Nothing();
}

const nsString& AudioDeviceInfo::FriendlyName()
{
  return mName;
}
uint32_t AudioDeviceInfo::MaxChannels()
{
  return mMaxChannels;
}
uint32_t AudioDeviceInfo::Type()
{
  return mType;
}
uint32_t AudioDeviceInfo::State()
{
  return mState;
}

/* readonly attribute DOMString name; */
NS_IMETHODIMP
AudioDeviceInfo::GetName(nsAString& aName)
{
  aName = mName;
  return NS_OK;
}

/* readonly attribute DOMString groupId; */
NS_IMETHODIMP
AudioDeviceInfo::GetGroupId(nsAString& aGroupId)
{
  aGroupId = mGroupId;
  return NS_OK;
}

/* readonly attribute DOMString vendor; */
NS_IMETHODIMP
AudioDeviceInfo::GetVendor(nsAString& aVendor)
{
  aVendor = mVendor;
  return NS_OK;
}

/* readonly attribute unsigned short type; */
NS_IMETHODIMP
AudioDeviceInfo::GetType(uint16_t* aType)
{
  *aType = mType;
  return NS_OK;
}

/* readonly attribute unsigned short state; */
NS_IMETHODIMP
AudioDeviceInfo::GetState(uint16_t* aState)
{
  *aState = mState;
  return NS_OK;
}

/* readonly attribute unsigned short preferred; */
NS_IMETHODIMP
AudioDeviceInfo::GetPreferred(uint16_t* aPreferred)
{
  *aPreferred = mPreferred;
  return NS_OK;
}

/* readonly attribute unsigned short supportedFormat; */
NS_IMETHODIMP
AudioDeviceInfo::GetSupportedFormat(uint16_t* aSupportedFormat)
{
  *aSupportedFormat = mSupportedFormat;
  return NS_OK;
}

/* readonly attribute unsigned short defaultFormat; */
NS_IMETHODIMP
AudioDeviceInfo::GetDefaultFormat(uint16_t* aDefaultFormat)
{
  *aDefaultFormat = mDefaultFormat;
  return NS_OK;
}

/* readonly attribute unsigned long maxChannels; */
NS_IMETHODIMP
AudioDeviceInfo::GetMaxChannels(uint32_t* aMaxChannels)
{
  *aMaxChannels = mMaxChannels;
  return NS_OK;
}

/* readonly attribute unsigned long defaultRate; */
NS_IMETHODIMP
AudioDeviceInfo::GetDefaultRate(uint32_t* aDefaultRate)
{
  *aDefaultRate = mDefaultRate;
  return NS_OK;
}

/* readonly attribute unsigned long maxRate; */
NS_IMETHODIMP
AudioDeviceInfo::GetMaxRate(uint32_t* aMaxRate)
{
  *aMaxRate = mMaxRate;
  return NS_OK;
}

/* readonly attribute unsigned long minRate; */
NS_IMETHODIMP
AudioDeviceInfo::GetMinRate(uint32_t* aMinRate)
{
  *aMinRate = mMinRate;
  return NS_OK;
}

/* readonly attribute unsigned long maxLatency; */
NS_IMETHODIMP
AudioDeviceInfo::GetMaxLatency(uint32_t* aMaxLatency)
{
  *aMaxLatency = mMaxLatency;
  return NS_OK;
}

/* readonly attribute unsigned long minLatency; */
NS_IMETHODIMP
AudioDeviceInfo::GetMinLatency(uint32_t* aMinLatency)
{
  *aMinLatency = mMinLatency;
  return NS_OK;
}

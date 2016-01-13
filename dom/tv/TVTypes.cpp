/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TVUtils.h"
#include "nsCRTGlue.h"
#include "TVTypes.h"

namespace mozilla {
namespace dom {

/*
 * Implementation of TVTunerData
 */

NS_IMPL_ISUPPORTS(TVTunerData, nsITVTunerData)

TVTunerData::TVTunerData()
  : mSupportedSourceTypes(nullptr)
  , mCount(0)
  , mStreamType(0)
{
}

TVTunerData::~TVTunerData()
{
  if (mSupportedSourceTypes) {
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(mCount, mSupportedSourceTypes);
  }
}

/* virtual */ NS_IMETHODIMP
TVTunerData::GetId(nsAString& aId)
{
  aId = mId;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVTunerData::SetId(const nsAString& aId)
{
  if (aId.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  mId = aId;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVTunerData::GetStreamType(uint16_t* aStreamType)
{
  *aStreamType = mStreamType;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVTunerData::SetStreamType(const uint16_t aStreamType)
{
  mStreamType = aStreamType;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVTunerData::GetSupportedSourceTypes(uint32_t* aCount,
                                     char*** aSourceTypes)
{
  *aCount = mCount;

  char** sourceTypes = (mCount > 0) ?
                       static_cast<char **>(moz_xmalloc(mCount * sizeof(char*))) :
                       nullptr;
  for (uint32_t i = 0; i < mCount; i++) {
    sourceTypes[i] = NS_strdup(mSupportedSourceTypes[i]);
  }

  *aSourceTypes = sourceTypes;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVTunerData::SetSupportedSourceTypes(uint32_t aCount,
                                     const char** aSourceTypes)
{
  if (aCount == 0) {
    return NS_ERROR_INVALID_ARG;
  }
  NS_ENSURE_ARG_POINTER(aSourceTypes);

  for (uint32_t i = 0; i < aCount; i++) {
    if (TVSourceType::EndGuard_ == ToTVSourceType(aSourceTypes[i])) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  if (mSupportedSourceTypes) {
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(mCount, mSupportedSourceTypes);
  }

  mCount = aCount;

  mSupportedSourceTypes = (mCount > 0) ?
                          static_cast<char **>(moz_xmalloc(mCount * sizeof(char*))) :
                          nullptr;
  for (uint32_t i = 0; i < mCount; i++) {
    mSupportedSourceTypes[i] = NS_strdup(aSourceTypes[i]);
  }

  return NS_OK;
}


/*
 * Implementation of TVChannelData
 */

NS_IMPL_ISUPPORTS(TVChannelData, nsITVChannelData)

TVChannelData::TVChannelData()
  : mIsEmergency(false)
  , mIsFree(false)
{
}

TVChannelData::~TVChannelData()
{
}

/* virtual */ NS_IMETHODIMP
TVChannelData::GetNetworkId(nsAString& aNetworkId)
{
  aNetworkId = mNetworkId;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVChannelData::SetNetworkId(const nsAString& aNetworkId)
{
  if (aNetworkId.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  mNetworkId = aNetworkId;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVChannelData::GetTransportStreamId(nsAString& aTransportStreamId)
{
  aTransportStreamId = mTransportStreamId;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVChannelData::SetTransportStreamId(const nsAString& aTransportStreamId)
{
  if (aTransportStreamId.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  mTransportStreamId = aTransportStreamId;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVChannelData::GetServiceId(nsAString& aServiceId)
{
  aServiceId = mServiceId;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVChannelData::SetServiceId(const nsAString& aServiceId)
{
  if (aServiceId.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  mServiceId = aServiceId;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVChannelData::GetType(nsAString& aType)
{
  aType = mType;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVChannelData::SetType(const nsAString& aType)
{
  if (aType.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  if (TVChannelType::EndGuard_ == ToTVChannelType(aType)) {
    return NS_ERROR_INVALID_ARG;
  }

  mType = aType;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVChannelData::GetNumber(nsAString& aNumber)
{
  aNumber = mNumber;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVChannelData::SetNumber(const nsAString& aNumber)
{
  if (aNumber.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  mNumber = aNumber;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVChannelData::GetName(nsAString& aName)
{
  aName = mName;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVChannelData::SetName(const nsAString& aName)
{
  if (aName.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  mName = aName;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVChannelData::GetIsEmergency(bool *aIsEmergency)
{
  *aIsEmergency = mIsEmergency;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVChannelData::SetIsEmergency(bool aIsEmergency)
{
  mIsEmergency = aIsEmergency;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVChannelData::GetIsFree(bool *aIsFree)
{
  *aIsFree = mIsFree;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVChannelData::SetIsFree(bool aIsFree)
{
  mIsFree = aIsFree;
  return NS_OK;
}


/*
 * Implementation of TVProgramData
 */

NS_IMPL_ISUPPORTS(TVProgramData, nsITVProgramData)

TVProgramData::TVProgramData()
  : mStartTime(0)
  , mDuration(0)
  , mAudioLanguages(nullptr)
  , mAudioLanguageCount(0)
  , mSubtitleLanguages(nullptr)
  , mSubtitleLanguageCount(0)
{
}

TVProgramData::~TVProgramData()
{
  if (mAudioLanguages) {
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(mAudioLanguageCount, mAudioLanguages);
  }
  if (mSubtitleLanguages) {
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(mSubtitleLanguageCount, mSubtitleLanguages);
  }
}

/* virtual */ NS_IMETHODIMP
TVProgramData::GetEventId(nsAString& aEventId)
{
  aEventId = mEventId;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVProgramData::SetEventId(const nsAString& aEventId)
{
  if (aEventId.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  mEventId = aEventId;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVProgramData::GetTitle(nsAString& aTitle)
{
  aTitle = mTitle;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVProgramData::SetTitle(const nsAString& aTitle)
{
  if (aTitle.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  mTitle = aTitle;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVProgramData::GetStartTime(uint64_t *aStartTime)
{
  *aStartTime = mStartTime;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVProgramData::SetStartTime(uint64_t aStartTime)
{
  mStartTime = aStartTime;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVProgramData::GetDuration(uint64_t *aDuration)
{
  *aDuration = mDuration;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVProgramData::SetDuration(uint64_t aDuration)
{
  mDuration = aDuration;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVProgramData::GetDescription(nsAString& aDescription)
{
  aDescription = mDescription;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVProgramData::SetDescription(const nsAString& aDescription)
{
  mDescription = aDescription;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVProgramData::GetRating(nsAString& aRating)
{
  aRating = mRating;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVProgramData::SetRating(const nsAString& aRating)
{
  mRating = aRating;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVProgramData::GetAudioLanguages(uint32_t* aCount,
                                 char*** aLanguages)
{
  *aCount = mAudioLanguageCount;

  char** languages = (mAudioLanguageCount > 0) ?
                     static_cast<char **>(moz_xmalloc(mAudioLanguageCount * sizeof(char*))) :
                     nullptr;
  for (uint32_t i = 0; i < mAudioLanguageCount; i++) {
    languages[i] = NS_strdup(mAudioLanguages[i]);
  }

  *aLanguages = languages;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVProgramData::SetAudioLanguages(uint32_t aCount,
                                 const char** aLanguages)
{
  if (aCount > 0) {
    NS_ENSURE_ARG_POINTER(aLanguages);
  }

  if (mAudioLanguages) {
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(mAudioLanguageCount, mAudioLanguages);
  }

  mAudioLanguageCount = aCount;

  mAudioLanguages = (mAudioLanguageCount > 0) ?
                    static_cast<char **>(moz_xmalloc(mAudioLanguageCount * sizeof(char*))) :
                    nullptr;
  for (uint32_t i = 0; i < mAudioLanguageCount; i++) {
    mAudioLanguages[i] = NS_strdup(aLanguages[i]);
  }

  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVProgramData::GetSubtitleLanguages(uint32_t* aCount,
                                    char*** aLanguages)
{
  *aCount = mSubtitleLanguageCount;

  char** languages = (mSubtitleLanguageCount > 0) ?
                     static_cast<char **>(moz_xmalloc(mSubtitleLanguageCount * sizeof(char*))) :
                     nullptr;
  for (uint32_t i = 0; i < mSubtitleLanguageCount; i++) {
    languages[i] = NS_strdup(mSubtitleLanguages[i]);
  }

  *aLanguages = languages;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVProgramData::SetSubtitleLanguages(uint32_t aCount,
                                    const char** aLanguages)
{
  if (aCount > 0) {
    NS_ENSURE_ARG_POINTER(aLanguages);
  }

  if (mSubtitleLanguages) {
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(mSubtitleLanguageCount, mSubtitleLanguages);
  }

  mSubtitleLanguageCount = aCount;

  mSubtitleLanguages = (mSubtitleLanguageCount > 0) ?
                       static_cast<char **>(moz_xmalloc(mSubtitleLanguageCount * sizeof(char*))) :
                       nullptr;
  for (uint32_t i = 0; i < mSubtitleLanguageCount; i++) {
    mSubtitleLanguages[i] = NS_strdup(aLanguages[i]);
  }

  return NS_OK;
}

} // namespace dom
} // namespace mozilla

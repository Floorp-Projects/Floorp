/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TVTypes_h
#define mozilla_dom_TVTypes_h

#include "nsITVService.h"

namespace mozilla {
namespace dom {

class TVTunerData final : public nsITVTunerData
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITVTUNERDATA

  TVTunerData();

private:
  ~TVTunerData();

  nsString mId;
  char** mSupportedSourceTypes;
  uint32_t mCount;
  uint16_t mStreamType;
};

class TVChannelData final : public nsITVChannelData
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITVCHANNELDATA

  TVChannelData();

private:
  ~TVChannelData();

  nsString mNetworkId;
  nsString mTransportStreamId;
  nsString mServiceId;
  nsString mType;
  nsString mNumber;
  nsString mName;
  bool mIsEmergency;
  bool mIsFree;
};

class TVProgramData final : public nsITVProgramData
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITVPROGRAMDATA

  TVProgramData();

private:
  ~TVProgramData();

  nsString mEventId;
  nsString mTitle;
  uint64_t mStartTime;
  uint64_t mDuration;
  nsString mDescription;
  nsString mRating;
  char** mAudioLanguages;
  uint32_t mAudioLanguageCount;
  char** mSubtitleLanguages;
  uint32_t mSubtitleLanguageCount;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TVTypes_h

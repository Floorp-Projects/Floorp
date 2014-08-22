/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TVChannel.h"
#include "mozilla/dom/TVProgramBinding.h"
#include "nsITVService.h"
#include "TVProgram.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TVProgram, mOwner, mChannel)

NS_IMPL_CYCLE_COLLECTING_ADDREF(TVProgram)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TVProgram)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TVProgram)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

TVProgram::TVProgram(nsISupports* aOwner,
                     TVChannel* aChannel,
                     nsITVProgramData* aData)
  : mOwner(aOwner)
  , mChannel(aChannel)
{
  MOZ_ASSERT(mChannel);
  MOZ_ASSERT(aData);

  aData->GetEventId(mEventId);
  aData->GetTitle(mTitle);
  aData->GetStartTime(&mStartTime);
  aData->GetDuration(&mDuration);
  aData->GetDescription(mDescription);
  aData->GetRating(mRating);

  uint32_t count;
  char** languages;
  aData->GetAudioLanguages(&count, &languages);
  SetLanguages(count, languages, mAudioLanguages);
  aData->GetSubtitleLanguages(&count, &languages);
  SetLanguages(count, languages, mSubtitleLanguages);
}

TVProgram::~TVProgram()
{
}

/* virtual */ JSObject*
TVProgram::WrapObject(JSContext* aCx)
{
  return TVProgramBinding::Wrap(aCx, this);
}

void
TVProgram::GetAudioLanguages(nsTArray<nsString>& aLanguages) const
{
  aLanguages = mAudioLanguages;
}

void
TVProgram::GetSubtitleLanguages(nsTArray<nsString>& aLanguages) const
{
  aLanguages = mSubtitleLanguages;
}

void
TVProgram::GetEventId(nsAString& aEventId) const
{
  aEventId = mEventId;
}

already_AddRefed<TVChannel>
TVProgram::Channel() const
{
  nsRefPtr<TVChannel> channel = mChannel;
  return channel.forget();
}

void
TVProgram::GetTitle(nsAString& aTitle) const
{
  aTitle = mTitle;
}

uint64_t
TVProgram::StartTime() const
{
  return mStartTime;
}

uint64_t
TVProgram::Duration() const
{
  return mDuration;
}

void
TVProgram::GetDescription(nsAString& aDescription) const
{
  aDescription = mDescription;
}

void
TVProgram::GetRating(nsAString& aRating) const
{
  aRating = mRating;
}

void
TVProgram::SetLanguages(uint32_t aCount,
                        char** aLanguages,
                        nsTArray<nsString>& aLanguageList)
{
  for (uint32_t i = 0; i < aCount; i++) {
    nsString language;
    language.AssignASCII(aLanguages[i]);
    aLanguageList.AppendElement(language);
  }
  NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(aCount, aLanguages);
}

} // namespace dom
} // namespace mozilla

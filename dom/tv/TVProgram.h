/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TVProgram_h
#define mozilla_dom_TVProgram_h

#include "nsWrapperCache.h"

class nsITVProgramData;

namespace mozilla {
namespace dom {

class TVChannel;

class TVProgram MOZ_FINAL : public nsISupports
                          , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(TVProgram)

  TVProgram(nsISupports* aOwner,
            TVChannel* aChannel,
            nsITVProgramData* aData);

  // WebIDL (internal functions)

  nsISupports* GetParentObject() const
  {
    return mOwner;
  }

  virtual JSObject* WrapObject(JSContext *aCx) MOZ_OVERRIDE;

  // WebIDL (public APIs)

  void GetAudioLanguages(nsTArray<nsString>& aLanguages) const;

  void GetSubtitleLanguages(nsTArray<nsString>& aLanguages) const;

  void GetEventId(nsAString& aEventId) const;

  already_AddRefed<TVChannel> Channel() const;

  void GetTitle(nsAString& aTitle) const;

  uint64_t StartTime() const;

  uint64_t Duration() const;

  void GetDescription(nsAString& aDescription) const;

  void GetRating(nsAString& aRating) const;

private:
  ~TVProgram();

  void SetLanguages(uint32_t aCount,
                    char** aLanguages,
                    nsTArray<nsString>& aLanguageList);

  nsCOMPtr<nsISupports> mOwner;
  nsRefPtr<TVChannel> mChannel;
  nsString mEventId;
  nsString mTitle;
  uint64_t mStartTime;
  uint64_t mDuration;
  nsString mDescription;
  nsString mRating;
  bool mIsInterrupting;
  nsTArray<nsString> mAudioLanguages;
  nsTArray<nsString> mSubtitleLanguages;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TVProgram_h__

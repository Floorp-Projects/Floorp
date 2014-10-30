/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FakeTVService_h
#define mozilla_dom_FakeTVService_h

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsITVService.h"
#include "nsTArray.h"

#define FAKE_TV_SERVICE_CONTRACTID \
  "@mozilla.org/tv/faketvservice;1"
#define FAKE_TV_SERVICE_CID \
  { 0x60fb3c53, 0x017f, 0x4340, { 0x91, 0x1b, 0xd5, 0x5c, 0x31, 0x28, 0x88, 0xb6 } }

class nsITimer;
class nsITVTunerData;
class nsITVChannelData;
class nsITVProgramData;

namespace mozilla {
namespace dom {

class FakeTVService MOZ_FINAL : public nsITVService
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(FakeTVService)
  NS_DECL_NSITVSERVICE

  FakeTVService();

private:
  ~FakeTVService();

  void Init();

  void Shutdown();

  bool IsAllowed(const nsAString& aTunerId,
                 const nsAString& aSourceType);

  already_AddRefed<nsITVTunerData> MockTuner(const nsAString& aId,
                                             uint32_t aSupportedSourceTypeCount,
                                             const char** aSupportedSourceTypes);

  already_AddRefed<nsITVChannelData> MockChannel(const nsAString& aNetworkId,
                                                 const nsAString& aTransportStreamId,
                                                 const nsAString& aServiceId,
                                                 const nsAString& aType,
                                                 const nsAString& aNumber,
                                                 const nsAString& aName,
                                                 bool aIsEmergency,
                                                 bool aIsFree);

  already_AddRefed<nsITVProgramData> MockProgram(const nsAString& aEventId,
                                                 const nsAString& aTitle,
                                                 uint64_t aStartTime,
                                                 uint64_t aDuration,
                                                 const nsAString& aDescription,
                                                 const nsAString& aRating,
                                                 uint32_t aAudioLanguageCount,
                                                 const char** aAudioLanguages,
                                                 uint32_t aSubtitleLanguageCount,
                                                 const char** aSubtitleLanguages);

  nsCOMPtr<nsITVSourceListener> mSourceListener;

  // The real implementation may want to use more efficient data structures.
  nsTArray<nsCOMPtr<nsITVTunerData>> mTuners;
  nsTArray<nsCOMPtr<nsITVChannelData>> mChannels;
  nsTArray<nsCOMPtr<nsITVProgramData>> mPrograms;
  nsCOMPtr<nsITimer> mEITBroadcastedTimer;
  nsCOMPtr<nsITimer> mScanCompleteTimer;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FakeTVService_h

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TVIPCHelper.h"
#include "nsITVService.h"
#include "mozilla/dom/PTVTypes.h"

namespace mozilla {
namespace dom {

/*
 * TVTunerData Pack/Unpack.
 */
void
PackTVIPCTunerData(nsITVTunerData* aTunerData, TVIPCTunerData& aIPCTunerData)
{
  if (!aTunerData) {
    return;
  }

  aTunerData->GetId(aIPCTunerData.id());
  aTunerData->GetStreamType(&aIPCTunerData.streamType());
  aTunerData->GetSupportedSourceTypesByString(aIPCTunerData.sourceTypes());
  aIPCTunerData.sourceTypeCount() = aIPCTunerData.sourceTypes().Length();
}

void
UnpackTVIPCTunerData(nsITVTunerData* aTunerData,
                     const TVIPCTunerData& aIPCTunerData)
{
  if (!aTunerData) {
    return;
  }

  aTunerData->SetId(aIPCTunerData.id());
  aTunerData->SetStreamType(aIPCTunerData.streamType());
  aTunerData->SetSupportedSourceTypesByString(aIPCTunerData.sourceTypes());
}

/*
 * TVChannelData Pack/Unpack.
 */
void
PackTVIPCChannelData(nsITVChannelData* aChannelData,
                     TVIPCChannelData& aIPCChannelData)
{
  if (!aChannelData) {
    return;
  }

  nsString networkId;
  aChannelData->GetNetworkId(networkId);
  aIPCChannelData.networkId() = NS_ConvertUTF16toUTF8(networkId);

  nsString transportStreamId;
  aChannelData->GetTransportStreamId(transportStreamId);
  aIPCChannelData.transportStreamId() =
    NS_ConvertUTF16toUTF8(transportStreamId);

  nsString serviceId;
  aChannelData->GetServiceId(serviceId);
  aIPCChannelData.serviceId() = NS_ConvertUTF16toUTF8(serviceId);

  nsString type;
  aChannelData->GetType(type);
  aIPCChannelData.type() = NS_ConvertUTF16toUTF8(type);

  nsString number;
  aChannelData->GetNumber(number);
  aIPCChannelData.number() = NS_ConvertUTF16toUTF8(number);

  nsString name;
  aChannelData->GetName(name);
  aIPCChannelData.name() = NS_ConvertUTF16toUTF8(name);

  aChannelData->GetIsEmergency(&aIPCChannelData.isEmergency());
  aChannelData->GetIsFree(&aIPCChannelData.isFree());
}

void
UnpackTVIPCChannelData(nsITVChannelData* aChannelData,
                       const TVIPCChannelData& aIPCChannelData)
{
  if (!aChannelData) {
    return;
  }

  aChannelData->SetNetworkId(
    NS_ConvertUTF8toUTF16(aIPCChannelData.networkId()));
  aChannelData->SetTransportStreamId(
    NS_ConvertUTF8toUTF16(aIPCChannelData.transportStreamId()));
  aChannelData->SetServiceId(
    NS_ConvertUTF8toUTF16(aIPCChannelData.serviceId()));
  aChannelData->SetType(NS_ConvertUTF8toUTF16(aIPCChannelData.type()));
  aChannelData->SetNumber(NS_ConvertUTF8toUTF16(aIPCChannelData.number()));
  aChannelData->SetName(NS_ConvertUTF8toUTF16(aIPCChannelData.name()));
  aChannelData->SetIsEmergency(aIPCChannelData.isEmergency());
  aChannelData->SetIsFree(aIPCChannelData.isFree());
}

/*
 * TVProgramData Pack/Unpack.
 */
void
PackTVIPCProgramData(nsITVProgramData* aProgramData,
                     TVIPCProgramData& aIPCProgramData)
{
  if (!aProgramData) {
    return;
  }

  nsString eventId;
  aProgramData->GetEventId(eventId);
  aIPCProgramData.eventId() = NS_ConvertUTF16toUTF8(eventId);

  nsString title;
  aProgramData->GetTitle(title);
  aIPCProgramData.title() = NS_ConvertUTF16toUTF8(title);

  aProgramData->GetStartTime(&aIPCProgramData.startTime());

  aProgramData->GetDuration(&aIPCProgramData.duration());

  nsString description;
  aProgramData->GetDescription(description);
  aIPCProgramData.description() = NS_ConvertUTF16toUTF8(description);

  nsString rating;
  aProgramData->GetRating(rating);
  aIPCProgramData.rating() = NS_ConvertUTF16toUTF8(rating);

  aProgramData->GetAudioLanguagesByString(aIPCProgramData.audioLanguages());
  aIPCProgramData.audioLanguageCount() =
    aIPCProgramData.audioLanguages().Length();

  aProgramData->GetSubtitleLanguagesByString(
    aIPCProgramData.subtitleLanguages());
  aIPCProgramData.subtitleLanguageCount() =
    aIPCProgramData.subtitleLanguages().Length();
}

void
UnpackTVIPCProgramData(nsITVProgramData* aProgramData,
                       const TVIPCProgramData& aIPCProgramData)
{
  if (!aProgramData) {
    return;
  }

  aProgramData->SetEventId(NS_ConvertUTF8toUTF16(aIPCProgramData.eventId()));
  aProgramData->SetTitle(NS_ConvertUTF8toUTF16(aIPCProgramData.title()));
  aProgramData->SetStartTime(aIPCProgramData.startTime());
  aProgramData->SetDuration(aIPCProgramData.duration());
  aProgramData->SetDescription(
    NS_ConvertUTF8toUTF16(aIPCProgramData.description()));
  aProgramData->SetRating(NS_ConvertUTF8toUTF16(aIPCProgramData.rating()));
  aProgramData->SetAudioLanguagesByString(aIPCProgramData.audioLanguages());
  aProgramData->SetSubtitleLanguagesByString(
    aIPCProgramData.subtitleLanguages());
}

/**
 * nsITVGonkNativeHandleData Serialize/De-serialize.
 */
void
PackTVIPCGonkNativeHandleData(nsITVGonkNativeHandleData* aNativeHandleData,
                              TVIPCGonkNativeHandleData& aIPCNativeHandleData)
{
#ifdef MOZ_WIDGET_GONK
  if (!aNativeHandleData) {
    return;
  }

  aNativeHandleData->GetHandle(aIPCNativeHandleData.handle());
#else
  aIPCNativeHandleData.handle() = null_t();
#endif
}

void
UnpackTVIPCGonkNativeHandleData(
  nsITVGonkNativeHandleData* aNativeHandleData,
  const TVIPCGonkNativeHandleData& aIPCNativeHandleData)
{
#ifdef MOZ_WIDGET_GONK
  if (!aNativeHandleData) {
    return;
  }

   aNativeHandleData->SetHandle(
     const_cast<StreamHandle&>(aIPCNativeHandleData.handle()));
#endif
}

} // namespace dom
} // namespace mozilla

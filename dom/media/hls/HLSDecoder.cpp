/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HLSDecoder.h"
#include "AndroidBridge.h"
#include "DecoderTraits.h"
#include "HLSDemuxer.h"
#include "HLSResource.h"
#include "HLSUtils.h"
#include "MediaContainerType.h"
#include "MediaDecoderStateMachine.h"
#include "MediaFormatReader.h"
#include "MediaPrefs.h"

namespace mozilla {

MediaDecoderStateMachine*
HLSDecoder::CreateStateMachine()
{
  MOZ_ASSERT(NS_IsMainThread());

  MediaResource* resource = GetResource();
  MOZ_ASSERT(resource);
  auto resourceWrapper = static_cast<HLSResource*>(resource)->GetResourceWrapper();
  MOZ_ASSERT(resourceWrapper);
  mReader =
    new MediaFormatReader(this,
                          new HLSDemuxer(resourceWrapper->GetPlayerId()),
                          GetVideoFrameContainer());

  return new MediaDecoderStateMachine(this, mReader);
}

MediaDecoder*
HLSDecoder::Clone(MediaDecoderInit& aInit)
{
  if (!IsEnabled()) {
    return nullptr;
  }
  return new HLSDecoder(aInit);
}

bool
HLSDecoder::IsEnabled()
{
  return MediaPrefs::HLSEnabled() && (jni::GetAPIVersion() >= 16);
}

bool
HLSDecoder::IsSupportedType(const MediaContainerType& aContainerType)
{
  return IsEnabled() &&
         DecoderTraits::IsHttpLiveStreamingType(aContainerType);
}

} // namespace mozilla

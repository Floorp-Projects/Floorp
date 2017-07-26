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
#include "MediaShutdownManager.h"
#include "nsNetUtil.h"

namespace mozilla {

void
HLSDecoder::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  auto resource = static_cast<HLSResource*>(mResource.get());
  if (resource) {
    resource->Detach();
  }
  MediaDecoder::Shutdown();
}

MediaDecoderStateMachine*
HLSDecoder::CreateStateMachine()
{
  MOZ_ASSERT(NS_IsMainThread());

  MediaResource* resource = GetResource();
  MOZ_ASSERT(resource);
  auto resourceWrapper = static_cast<HLSResource*>(resource)->GetResourceWrapper();
  MOZ_ASSERT(resourceWrapper);
  MediaFormatReaderInit init;
  init.mVideoFrameContainer = GetVideoFrameContainer();
  init.mKnowsCompositor = GetCompositor();
  init.mCrashHelper = GetOwner()->CreateGMPCrashHelper();
  init.mFrameStats = mFrameStats;
  mReader =
    new MediaFormatReader(init, new HLSDemuxer(resourceWrapper->GetPlayerId()));

  return new MediaDecoderStateMachine(this, mReader);
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

nsresult
HLSDecoder::Load(nsIChannel* aChannel,
                 bool aIsPrivateBrowsing,
                 nsIStreamListener**)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mResource);

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mResource = new HLSResource(this, aChannel, uri);

  rv = MediaShutdownManager::Instance().Register(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  SetStateMachine(CreateStateMachine());
  NS_ENSURE_TRUE(GetStateMachine(), NS_ERROR_FAILURE);

  return InitializeStateMachine();
}

nsresult
HLSDecoder::Play()
{
  MOZ_ASSERT(NS_IsMainThread());
  HLS_DEBUG("HLSDecoder", "MediaElement called Play");
  auto resourceWrapper =
        static_cast<HLSResource*>(GetResource())->GetResourceWrapper();
  resourceWrapper->Play();
  return MediaDecoder::Play();
}

void
HLSDecoder::Pause()
{
  MOZ_ASSERT(NS_IsMainThread());
  HLS_DEBUG("HLSDecoder", "MediaElement called Pause");
  auto resourceWrapper =
      static_cast<HLSResource*>(GetResource())->GetResourceWrapper();
  resourceWrapper->Pause();
  return MediaDecoder::Pause();
}

} // namespace mozilla

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RtspMediaResource.h"
#include "RtspOmxDecoder.h"
#include "RtspOmxReader.h"
#include "MediaDecoderStateMachine.h"

namespace mozilla {

MediaDecoder* RtspOmxDecoder::Clone()
{
  return new RtspOmxDecoder();
}

MediaDecoderStateMachine*
RtspOmxDecoder::CreateStateMachine()
{
  return new MediaDecoderStateMachine(this,
                                      new RtspOmxReader(this),
                                      mResource->IsRealTime());
}

void
RtspOmxDecoder::ChangeState(PlayState aState)
{
  MOZ_ASSERT(NS_IsMainThread());

  MediaDecoder::ChangeState(aState);

  // Notify RTSP controller if the play state is ended.
  // This is necessary for RTSP controller to transit its own state.
  if (mPlayState == PLAY_STATE_ENDED) {
    nsRefPtr<RtspMediaResource> resource = mResource->GetRtspPointer();
    if (resource) {
      nsIStreamingProtocolController* controller =
        resource->GetMediaStreamController();
      if (controller) {
        controller->PlaybackEnded();
      }
    }
  }
}

} // namespace mozilla


/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RtspMediaResource.h"
#include "RtspOmxDecoder.h"
#include "RtspOmxReader.h"
#include "MediaOmxStateMachine.h"

namespace mozilla {

MediaDecoder* RtspOmxDecoder::Clone()
{
  return new RtspOmxDecoder();
}

MediaDecoderStateMachine* RtspOmxDecoder::CreateStateMachine()
{
  return new MediaOmxStateMachine(this, new RtspOmxReader(this),
                                  mResource->IsRealTime());
}

void RtspOmxDecoder::ApplyStateToStateMachine(PlayState aState)
{
  MOZ_ASSERT(NS_IsMainThread());
  GetReentrantMonitor().AssertCurrentThreadIn();

  MediaDecoder::ApplyStateToStateMachine(aState);


  // Send play/pause commands here through the nsIStreamingProtocolController
  // except seek command. We need to clear the decoded/un-decoded buffer data
  // before sending seek command. So the seek calling path to controller is:
  // mDecoderStateMachine::Seek-> RtspOmxReader::Seek-> RtspResource::SeekTime->
  // controller->Seek(). RtspOmxReader::Seek will clear the decoded buffer and
  // the RtspResource::SeekTime will clear the un-decoded buffer.

  RtspMediaResource* rtspResource = mResource->GetRtspPointer();
  MOZ_ASSERT(rtspResource);

  nsIStreamingProtocolController* controller =
    rtspResource->GetMediaStreamController();
  if (mDecoderStateMachine) {
    switch (aState) {
      case PLAY_STATE_PLAYING:
        if (controller) {
          controller->Play();
        }
        break;
      case PLAY_STATE_PAUSED:
        if (controller) {
          controller->Pause();
        }
        break;
      default:
        /* No action needed */
        break;
    }
  }
}

} // namespace mozilla


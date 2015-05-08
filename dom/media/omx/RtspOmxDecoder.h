/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(RtspOmxDecoder_h_)
#define RtspOmxDecoder_h_

#include "base/basictypes.h"
#include "MediaDecoder.h"

namespace mozilla {

/* RtspOmxDecoder is a subclass of MediaDecoder but not a subclass of
 * MediaOmxDecoder. Because the MediaOmxDecoder doesn't extend any functionality
 * for MediaDecoder.
 * It creates the RtspOmxReader for the MediaDecoderStateMachine and override
 * the ApplyStateToStateMachine to send rtsp play/pause command to rtsp server.
 *
 * */
class RtspOmxDecoder : public MediaDecoder
{
public:
  RtspOmxDecoder()
  : MediaDecoder() {
    MOZ_COUNT_CTOR(RtspOmxDecoder);
  }

  ~RtspOmxDecoder() {
    MOZ_COUNT_DTOR(RtspOmxDecoder);
  }

  virtual MediaDecoder* Clone() override final;
  virtual MediaDecoderStateMachine* CreateStateMachine() override final;
  virtual void ChangeState(PlayState aState) override final;
};

} // namespace mozilla

#endif

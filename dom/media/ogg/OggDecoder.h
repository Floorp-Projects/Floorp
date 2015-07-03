/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(OggDecoder_h_)
#define OggDecoder_h_

#include "MediaDecoder.h"

namespace mozilla {

class OggDecoder : public MediaDecoder
{
public:
  OggDecoder()
    : mShutdownBitMonitor("mShutdownBitMonitor")
    , mShutdownBit(false)
  {}

  virtual MediaDecoder* Clone() override {
    if (!IsOggEnabled()) {
      return nullptr;
    }
    return new OggDecoder();
  }
  virtual MediaDecoderStateMachine* CreateStateMachine() override;

  // For yucky legacy reasons, the ogg decoder needs to do a cross-thread read
  // to check for shutdown while it hogs its own task queue. We don't want to
  // protect the general state with a lock, so we make a special copy and a
  // special-purpose lock. This method may be called on any thread.
  bool IsOggDecoderShutdown() override
  {
    MonitorAutoLock lock(mShutdownBitMonitor);
    return mShutdownBit;
  }

protected:
  void ShutdownBitChanged() override
  {
    MonitorAutoLock lock(mShutdownBitMonitor);
    mShutdownBit = mStateMachineIsShutdown;
  }

  Monitor mShutdownBitMonitor;
  bool mShutdownBit;
};

} // namespace mozilla

#endif

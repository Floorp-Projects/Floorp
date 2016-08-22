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
  explicit OggDecoder(MediaDecoderOwner* aOwner)
    : MediaDecoder(aOwner)
    , mShutdownBitMonitor("mShutdownBitMonitor")
    , mShutdownBit(false)
  {}

  MediaDecoder* Clone(MediaDecoderOwner* aOwner) override {
    if (!IsOggEnabled()) {
      return nullptr;
    }
    return new OggDecoder(aOwner);
  }
  MediaDecoderStateMachine* CreateStateMachine() override;

  // For yucky legacy reasons, the ogg decoder needs to do a cross-thread read
  // to check for shutdown while it hogs its own task queue. We don't want to
  // protect the general state with a lock, so we make a special copy and a
  // special-purpose lock. This method may be called on any thread.
  bool IsOggDecoderShutdown() override
  {
    MonitorAutoLock lock(mShutdownBitMonitor);
    return mShutdownBit;
  }

  // Returns true if aMIMEType is a type that we think we can render with the
  // a platform decoder backend. If aCodecs is non emtpy, it is filled
  // with a comma-delimited list of codecs to check support for.
  static bool CanHandleMediaType(const nsACString& aMIMETypeExcludingCodecs,
                                 const nsAString& aCodecs);

  static bool IsEnabled();

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

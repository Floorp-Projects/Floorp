/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(OggCodecStore_h_)
#define OggCodecStore_h_

#include <ogg/ogg.h>

#include "OggCodecState.h"
#include "VideoUtils.h"
#include "mozilla/Monitor.h"

namespace mozilla {

// Thread safe container to store the codec information and the serial for each
// streams.
class OggCodecStore
{
  public:
    OggCodecStore();
    void Add(uint32_t serial, OggCodecState* codecState);
    bool Contains(uint32_t serial);
    OggCodecState* Get(uint32_t serial);
    bool IsKnownStream(uint32_t aSerial);

  private:
    // Maps Ogg serialnos to OggStreams.
    nsClassHashtable<nsUint32HashKey, OggCodecState> mCodecStates;

    // Protects the |mCodecStates| and the |mKnownStreams| members.
    Monitor mMonitor;
};

} // namespace mozilla

#endif

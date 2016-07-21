/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "OggCodecStore.h"

namespace mozilla {

OggCodecStore::OggCodecStore()
: mMonitor("CodecStore")
{
}

void OggCodecStore::Add(uint32_t serial, OggCodecState* codecState)
{
  MonitorAutoLock mon(mMonitor);
  mCodecStates.Put(serial, codecState);
}

bool OggCodecStore::Contains(uint32_t serial)
{
  MonitorAutoLock mon(mMonitor);
  return mCodecStates.Get(serial, nullptr);
}

OggCodecState* OggCodecStore::Get(uint32_t serial)
{
  MonitorAutoLock mon(mMonitor);
  return mCodecStates.Get(serial);
}

} // namespace mozilla


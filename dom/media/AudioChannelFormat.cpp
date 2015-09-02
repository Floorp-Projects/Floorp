/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioChannelFormat.h"

#include <algorithm>

namespace mozilla {

uint32_t
GetAudioChannelsSuperset(uint32_t aChannels1, uint32_t aChannels2)
{
  return std::max(aChannels1, aChannels2);
}

} // namespace mozilla

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaInfo.h"

namespace mozilla {

typedef AudioConfig::ChannelLayout ChannelLayout;

/**
 * AudioConfig::ChannelLayout
 */

/*
 SMPTE channel layout (also known as wave order)
 DUAL-MONO      L   R
 DUAL-MONO-LFE  L   R   LFE
 MONO           M
 MONO-LFE       M   LFE
 STEREO         L   R
 STEREO-LFE     L   R   LFE
 3F             L   R   C
 3F-LFE         L   R   C    LFE
 2F1            L   R   S
 2F1-LFE        L   R   LFE  S
 3F1            L   R   C    S
 3F1-LFE        L   R   C    LFE S
 2F2            L   R   LS   RS
 2F2-LFE        L   R   LFE  LS   RS
 3F2            L   R   C    LS   RS
 3F2-LFE        L   R   C    LFE  LS   RS
 3F3R-LFE       L   R   C    LFE  BC   LS   RS
 3F4-LFE        L   R   C    LFE  Rls  Rrs  LS   RS
*/

void
AudioConfig::ChannelLayout::UpdateChannelMap()
{
  mChannelMap = 0;
  mValid = mChannels.Length() <= MAX_AUDIO_CHANNELS;
  for (size_t i = 0; i < mChannels.Length() && i <= MAX_AUDIO_CHANNELS; i++) {
    uint32_t mask = 1 << mChannels[i];
    if (mChannels[i] == CHANNEL_INVALID || (mChannelMap & mask)) {
      mValid = false;
    }
    mChannelMap |= mask;
  }
}

/* static */ const AudioConfig::Channel*
AudioConfig::ChannelLayout::SMPTEDefault(uint32_t aChannels) const
{
  switch (aChannels) {
    case 1: // MONO
    {
      static const Channel config[] = { CHANNEL_MONO };
      return config;
    }
    case 2: // STEREO
    {
      static const Channel config[] = { CHANNEL_LEFT, CHANNEL_RIGHT };
      return config;
    }
    case 3: // 3F
    {
      static const Channel config[] = { CHANNEL_LEFT, CHANNEL_RIGHT, CHANNEL_CENTER };
      return config;
    }
    case 4: // 2F2
    {
      static const Channel config[] = { CHANNEL_LEFT, CHANNEL_RIGHT, CHANNEL_LS, CHANNEL_RS };
      return config;
    }
    case 5: // 3F2
    {
      static const Channel config[] = { CHANNEL_LEFT, CHANNEL_RIGHT, CHANNEL_CENTER, CHANNEL_LS, CHANNEL_RS };
      return config;
    }
    case 6: // 3F2-LFE
    {
      static const Channel config[] = { CHANNEL_LEFT, CHANNEL_RIGHT, CHANNEL_CENTER, CHANNEL_LFE, CHANNEL_LS, CHANNEL_RS };
      return config;
    }
    case 7: // 3F3R-LFE
    {
      static const Channel config[] = { CHANNEL_LEFT, CHANNEL_RIGHT, CHANNEL_CENTER, CHANNEL_LFE, CHANNEL_RCENTER, CHANNEL_LS, CHANNEL_RS };
      return config;
    }
    case 8: // 3F4-LFE
    {
      static const Channel config[] = { CHANNEL_LEFT, CHANNEL_RIGHT, CHANNEL_CENTER, CHANNEL_LFE, CHANNEL_RLS, CHANNEL_RRS, CHANNEL_LS, CHANNEL_RS };
      return config;
    }
    default:
      return nullptr;
  }
}

bool
AudioConfig::ChannelLayout::MappingTable(const ChannelLayout& aOther,
                                         uint8_t* aMap) const
{
  if (!IsValid() || !aOther.IsValid() ||
      Map() != aOther.Map()) {
    return false;
  }
  if (!aMap) {
    return true;
  }
  for (uint32_t i = 0; i < Count(); i++) {
    for (uint32_t j = 0; j < Count(); j++) {
      if (aOther[j] == mChannels[i]) {
        aMap[j] = i;
        break;
      }
    }
  }
  return true;
}

/**
 * AudioConfig::ChannelConfig
 */

/* static */ const char*
AudioConfig::FormatToString(AudioConfig::SampleFormat aFormat)
{
  switch (aFormat) {
    case FORMAT_U8:     return "unsigned 8 bit";
    case FORMAT_S16:    return "signed 16 bit";
    case FORMAT_S24:    return "signed 24 bit MSB";
    case FORMAT_S24LSB: return "signed 24 bit LSB";
    case FORMAT_S32:    return "signed 32 bit";
    case FORMAT_FLT:    return "32 bit floating point";
    case FORMAT_NONE:   return "none";
    default:            return "unknown";
  }
}
/* static */ uint32_t
AudioConfig::SampleSize(AudioConfig::SampleFormat aFormat)
{
  switch (aFormat) {
    case FORMAT_U8:     return 1;
    case FORMAT_S16:    return 2;
    case FORMAT_S24:    MOZ_FALLTHROUGH;
    case FORMAT_S24LSB: MOZ_FALLTHROUGH;
    case FORMAT_S32:    MOZ_FALLTHROUGH;
    case FORMAT_FLT:    return 4;
    case FORMAT_NONE:
    default:            return 0;
  }
}

/* static */ uint32_t
AudioConfig::FormatToBits(AudioConfig::SampleFormat aFormat)
{
  switch (aFormat) {
    case FORMAT_U8:     return 8;
    case FORMAT_S16:    return 16;
    case FORMAT_S24LSB: MOZ_FALLTHROUGH;
    case FORMAT_S24:    return 24;
    case FORMAT_S32:    MOZ_FALLTHROUGH;
    case FORMAT_FLT:    return 32;
    case FORMAT_NONE:   MOZ_FALLTHROUGH;
    default:            return 0;
  }
}

AudioConfig::AudioConfig(const ChannelLayout& aChannelLayout, uint32_t aRate,
                         AudioConfig::SampleFormat aFormat, bool aInterleaved)
  : mChannelLayout(aChannelLayout)
  , mChannels(aChannelLayout.Count())
  , mRate(aRate)
  , mFormat(aFormat)
  , mInterleaved(aInterleaved)
{}

AudioConfig::AudioConfig(uint32_t aChannels, uint32_t aRate,
                         AudioConfig::SampleFormat aFormat, bool aInterleaved)
  : mChannelLayout(aChannels)
  , mChannels(aChannels)
  , mRate(aRate)
  , mFormat(aFormat)
  , mInterleaved(aInterleaved)
{}

} // namespace mozilla

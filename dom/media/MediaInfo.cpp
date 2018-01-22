/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaInfo.h"

namespace mozilla {

const char*
TrackTypeToStr(TrackInfo::TrackType aTrack)
{
  switch (aTrack) {
  case TrackInfo::kUndefinedTrack:
    return "Undefined";
  case TrackInfo::kAudioTrack:
    return "Audio";
  case TrackInfo::kVideoTrack:
    return "Video";
  case TrackInfo::kTextTrack:
    return "Text";
  default:
    return "Unknown";
  }
}

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

ChannelLayout ChannelLayout::LMONO{ AudioConfig::CHANNEL_MONO };
ChannelLayout ChannelLayout::LMONO_LFE{ AudioConfig::CHANNEL_MONO,
                                        AudioConfig::CHANNEL_LFE };
ChannelLayout ChannelLayout::LSTEREO{ AudioConfig::CHANNEL_LEFT,
                                      AudioConfig::CHANNEL_RIGHT };
ChannelLayout ChannelLayout::LSTEREO_LFE{ AudioConfig::CHANNEL_LEFT,
                                          AudioConfig::CHANNEL_RIGHT,
                                          AudioConfig::CHANNEL_LFE };
ChannelLayout ChannelLayout::L3F{ AudioConfig::CHANNEL_LEFT,
                                  AudioConfig::CHANNEL_RIGHT,
                                  AudioConfig::CHANNEL_CENTER };
ChannelLayout ChannelLayout::L3F_LFE{ AudioConfig::CHANNEL_LEFT,
                                      AudioConfig::CHANNEL_RIGHT,
                                      AudioConfig::CHANNEL_CENTER,
                                      AudioConfig::CHANNEL_LFE };
ChannelLayout ChannelLayout::L2F1{ AudioConfig::CHANNEL_LEFT,
                                   AudioConfig::CHANNEL_RIGHT,
                                   AudioConfig::CHANNEL_RCENTER };
ChannelLayout ChannelLayout::L2F1_LFE{ AudioConfig::CHANNEL_LEFT,
                                       AudioConfig::CHANNEL_RIGHT,
                                       AudioConfig::CHANNEL_LFE,
                                       AudioConfig::CHANNEL_RCENTER };
ChannelLayout ChannelLayout::L3F1{ AudioConfig::CHANNEL_LEFT,
                                   AudioConfig::CHANNEL_RIGHT,
                                   AudioConfig::CHANNEL_CENTER,
                                   AudioConfig::CHANNEL_RCENTER };
ChannelLayout ChannelLayout::L3F1_LFE{ AudioConfig::CHANNEL_LEFT,
                                       AudioConfig::CHANNEL_RIGHT,
                                       AudioConfig::CHANNEL_CENTER,
                                       AudioConfig::CHANNEL_LFE,
                                       AudioConfig::CHANNEL_RCENTER };
ChannelLayout ChannelLayout::L2F2{ AudioConfig::CHANNEL_LEFT,
                                   AudioConfig::CHANNEL_RIGHT,
                                   AudioConfig::CHANNEL_LS,
                                   AudioConfig::CHANNEL_RS };
ChannelLayout ChannelLayout::L2F2_LFE{ AudioConfig::CHANNEL_LEFT,
                                       AudioConfig::CHANNEL_RIGHT,
                                       AudioConfig::CHANNEL_LFE,
                                       AudioConfig::CHANNEL_LS,
                                       AudioConfig::CHANNEL_RS };
ChannelLayout ChannelLayout::L3F2{ AudioConfig::CHANNEL_LEFT,
                                   AudioConfig::CHANNEL_RIGHT,
                                   AudioConfig::CHANNEL_CENTER,
                                   AudioConfig::CHANNEL_LS,
                                   AudioConfig::CHANNEL_RS };
ChannelLayout ChannelLayout::L3F2_LFE{
  AudioConfig::CHANNEL_LEFT,   AudioConfig::CHANNEL_RIGHT,
  AudioConfig::CHANNEL_CENTER, AudioConfig::CHANNEL_LFE,
  AudioConfig::CHANNEL_LS,     AudioConfig::CHANNEL_RS
};
ChannelLayout ChannelLayout::L3F3R_LFE{
  AudioConfig::CHANNEL_LEFT,    AudioConfig::CHANNEL_RIGHT,
  AudioConfig::CHANNEL_CENTER,  AudioConfig::CHANNEL_LFE,
  AudioConfig::CHANNEL_RCENTER, AudioConfig::CHANNEL_LS,
  AudioConfig::CHANNEL_RS
};
ChannelLayout ChannelLayout::L3F4_LFE{
  AudioConfig::CHANNEL_LEFT,   AudioConfig::CHANNEL_RIGHT,
  AudioConfig::CHANNEL_CENTER, AudioConfig::CHANNEL_LFE,
  AudioConfig::CHANNEL_RLS,    AudioConfig::CHANNEL_RRS,
  AudioConfig::CHANNEL_LS,     AudioConfig::CHANNEL_RS
};

void
AudioConfig::ChannelLayout::UpdateChannelMap()
{
  mValid = mChannels.Length() <= MAX_AUDIO_CHANNELS;
  mChannelMap = 0;
  if (mValid) {
    mChannelMap = Map();
    mValid = mChannelMap > 0;
  }
}

uint32_t
AudioConfig::ChannelLayout::Map() const
{
  if (mChannelMap) {
    return mChannelMap;
  }
  uint32_t map = 0;
  for (size_t i = 0; i < mChannels.Length() && i <= MAX_AUDIO_CHANNELS; i++) {
    uint32_t mask = 1 << mChannels[i];
    if (mChannels[i] == CHANNEL_INVALID || (mChannelMap & mask)) {
      // Invalid configuration.
      return 0;
    }
    map |= mask;
  }
  return map;
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

/* static */ const AudioConfig::ChannelLayout&
AudioConfig::ChannelLayout::SMPTEDefault(
  const ChannelLayout& aChannelLayout)
{
  MOZ_ASSERT(LMONO_MAP == LMONO.Map());
  MOZ_ASSERT(LMONO_LFE_MAP == LMONO_LFE.Map());
  MOZ_ASSERT(LSTEREO_MAP == LSTEREO.Map());
  MOZ_ASSERT(LSTEREO_LFE_MAP == LSTEREO_LFE.Map());
  MOZ_ASSERT(L3F_MAP == L3F.Map());
  MOZ_ASSERT(L3F_LFE_MAP == L3F_LFE.Map());
  MOZ_ASSERT(L2F1_MAP == L2F1.Map());
  MOZ_ASSERT(L2F1_LFE_MAP == L2F1_LFE.Map());
  MOZ_ASSERT(L3F1_MAP == L3F1.Map());
  MOZ_ASSERT(L3F1_LFE_MAP == L3F1_LFE.Map());
  MOZ_ASSERT(L2F2_MAP == L2F2.Map());
  MOZ_ASSERT(L2F2_LFE_MAP == L2F2_LFE.Map());
  MOZ_ASSERT(L3F2_MAP == L3F2.Map());
  MOZ_ASSERT(L3F2_LFE_MAP == L3F2_LFE.Map());
  MOZ_ASSERT(L3F3R_LFE_MAP == L3F3R_LFE.Map());
  MOZ_ASSERT(L3F4_LFE_MAP == L3F4_LFE.Map());

  if (!aChannelLayout.IsValid()) {
    return aChannelLayout;
  }
  const uint32_t map = aChannelLayout.Map();
  switch (map) {
    case LMONO_MAP: return LMONO;
    case LMONO_LFE_MAP: return LMONO_LFE;
    case LSTEREO_MAP: return LSTEREO;
    case LSTEREO_LFE_MAP : return LSTEREO_LFE;
    case L3F_MAP: return L3F;
    case L3F_LFE_MAP: return L3F_LFE;
    case L2F1_MAP: return L2F1;
    case L2F1_LFE_MAP: return L2F1_LFE;
    case L3F1_MAP: return L3F1;
    case L3F1_LFE_MAP: return L3F1_LFE;
    case L2F2_MAP: return L2F2;
    case L2F2_LFE_MAP: return L2F2_LFE;
    case L3F2_MAP: return L3F2;
    case L3F2_LFE_MAP: return L3F2_LFE;
    case L3F3R_LFE_MAP: return L3F3R_LFE;
    case L3F4_LFE_MAP: return L3F4_LFE;
    default:
      // unknown return identical.
      return aChannelLayout;
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

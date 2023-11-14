/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioConfig.h"
#include "nsString.h"
#include <array>

namespace mozilla {

using ChannelLayout = AudioConfig::ChannelLayout;

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

void AudioConfig::ChannelLayout::UpdateChannelMap() {
  mValid = mChannels.Length() <= MAX_CHANNELS;
  mChannelMap = UNKNOWN_MAP;
  if (mValid) {
    mChannelMap = Map();
    mValid = mChannelMap > 0;
  }
}

auto AudioConfig::ChannelLayout::Map() const -> ChannelMap {
  if (mChannelMap != UNKNOWN_MAP) {
    return mChannelMap;
  }
  if (mChannels.Length() > MAX_CHANNELS) {
    return UNKNOWN_MAP;
  }
  ChannelMap map = UNKNOWN_MAP;
  for (size_t i = 0; i < mChannels.Length(); i++) {
    if (uint32_t(mChannels[i]) > sizeof(ChannelMap) * 8) {
      return UNKNOWN_MAP;
    }
    ChannelMap mask = 1 << mChannels[i];
    if (mChannels[i] == CHANNEL_INVALID || (mChannelMap & mask)) {
      // Invalid configuration.
      return UNKNOWN_MAP;
    }
    map |= mask;
  }
  return map;
}

const AudioConfig::Channel*
AudioConfig::ChannelLayout::DefaultLayoutForChannels(uint32_t aChannels) const {
  switch (aChannels) {
    case 1:  // MONO
    {
      static const Channel config[] = {CHANNEL_FRONT_CENTER};
      return config;
    }
    case 2:  // STEREO
    {
      static const Channel config[] = {CHANNEL_FRONT_LEFT, CHANNEL_FRONT_RIGHT};
      return config;
    }
    case 3:  // 3F
    {
      static const Channel config[] = {CHANNEL_FRONT_LEFT, CHANNEL_FRONT_RIGHT,
                                       CHANNEL_FRONT_CENTER};
      return config;
    }
    case 4:  // QUAD
    {
      static const Channel config[] = {CHANNEL_FRONT_LEFT, CHANNEL_FRONT_RIGHT,
                                       CHANNEL_BACK_LEFT, CHANNEL_BACK_RIGHT};
      return config;
    }
    case 5:  // 3F2
    {
      static const Channel config[] = {CHANNEL_FRONT_LEFT, CHANNEL_FRONT_RIGHT,
                                       CHANNEL_FRONT_CENTER, CHANNEL_SIDE_LEFT,
                                       CHANNEL_SIDE_RIGHT};
      return config;
    }
    case 6:  // 3F2-LFE
    {
      static const Channel config[] = {
          CHANNEL_FRONT_LEFT, CHANNEL_FRONT_RIGHT, CHANNEL_FRONT_CENTER,
          CHANNEL_LFE,        CHANNEL_SIDE_LEFT,   CHANNEL_SIDE_RIGHT};
      return config;
    }
    case 7:  // 3F3R-LFE
    {
      static const Channel config[] = {
          CHANNEL_FRONT_LEFT, CHANNEL_FRONT_RIGHT, CHANNEL_FRONT_CENTER,
          CHANNEL_LFE,        CHANNEL_BACK_CENTER, CHANNEL_SIDE_LEFT,
          CHANNEL_SIDE_RIGHT};
      return config;
    }
    case 8:  // 3F4-LFE
    {
      static const Channel config[] = {
          CHANNEL_FRONT_LEFT, CHANNEL_FRONT_RIGHT, CHANNEL_FRONT_CENTER,
          CHANNEL_LFE,        CHANNEL_BACK_LEFT,   CHANNEL_BACK_RIGHT,
          CHANNEL_SIDE_LEFT,  CHANNEL_SIDE_RIGHT};
      return config;
    }
    default:
      return nullptr;
  }
}

/* static */ AudioConfig::ChannelLayout
AudioConfig::ChannelLayout::SMPTEDefault(const ChannelLayout& aChannelLayout) {
  if (!aChannelLayout.IsValid()) {
    return aChannelLayout;
  }
  return SMPTEDefault(aChannelLayout.Map());
}

/* static */
ChannelLayout AudioConfig::ChannelLayout::SMPTEDefault(ChannelMap aMap) {
  // First handle the most common cases.
  switch (aMap) {
    case LMONO_MAP:
      return ChannelLayout{CHANNEL_FRONT_CENTER};
    case LSTEREO_MAP:
      return ChannelLayout{CHANNEL_FRONT_LEFT, CHANNEL_FRONT_RIGHT};
    case L3F_MAP:
      return ChannelLayout{CHANNEL_FRONT_LEFT, CHANNEL_FRONT_RIGHT,
                           CHANNEL_FRONT_CENTER};
    case L3F_LFE_MAP:
      return ChannelLayout{CHANNEL_FRONT_LEFT, CHANNEL_FRONT_RIGHT,
                           CHANNEL_FRONT_CENTER, CHANNEL_LFE};
    case L2F1_MAP:
      return ChannelLayout{CHANNEL_FRONT_LEFT, CHANNEL_FRONT_RIGHT,
                           CHANNEL_BACK_CENTER};
    case L2F1_LFE_MAP:
      return ChannelLayout{CHANNEL_FRONT_LEFT, CHANNEL_FRONT_RIGHT, CHANNEL_LFE,
                           CHANNEL_BACK_CENTER};
    case L3F1_MAP:
      return ChannelLayout{CHANNEL_FRONT_LEFT, CHANNEL_FRONT_RIGHT,
                           CHANNEL_FRONT_CENTER, CHANNEL_BACK_CENTER};
    case L3F1_LFE_MAP:
      return ChannelLayout{CHANNEL_FRONT_LEFT, CHANNEL_FRONT_RIGHT,
                           CHANNEL_FRONT_CENTER, CHANNEL_LFE,
                           CHANNEL_BACK_CENTER};
    case L2F2_MAP:
      return ChannelLayout{CHANNEL_FRONT_LEFT, CHANNEL_FRONT_RIGHT,
                           CHANNEL_SIDE_LEFT, CHANNEL_SIDE_RIGHT};
    case L2F2_LFE_MAP:
      return ChannelLayout{CHANNEL_FRONT_LEFT, CHANNEL_FRONT_RIGHT, CHANNEL_LFE,
                           CHANNEL_SIDE_LEFT, CHANNEL_SIDE_RIGHT};
    case LQUAD_MAP:
      return ChannelLayout{CHANNEL_FRONT_LEFT, CHANNEL_FRONT_RIGHT,
                           CHANNEL_BACK_LEFT, CHANNEL_BACK_RIGHT};
    case LQUAD_LFE_MAP:
      return ChannelLayout{CHANNEL_FRONT_LEFT, CHANNEL_FRONT_RIGHT, CHANNEL_LFE,
                           CHANNEL_BACK_LEFT, CHANNEL_BACK_RIGHT};
    case L3F2_MAP:
      return ChannelLayout{CHANNEL_FRONT_LEFT, CHANNEL_FRONT_RIGHT,
                           CHANNEL_FRONT_CENTER, CHANNEL_SIDE_LEFT,
                           CHANNEL_SIDE_RIGHT};
    case L3F2_LFE_MAP:
      return ChannelLayout{CHANNEL_FRONT_LEFT,   CHANNEL_FRONT_RIGHT,
                           CHANNEL_FRONT_CENTER, CHANNEL_LFE,
                           CHANNEL_SIDE_LEFT,    CHANNEL_SIDE_RIGHT};
    case L3F2_BACK_MAP:
      return ChannelLayout{CHANNEL_FRONT_LEFT, CHANNEL_FRONT_RIGHT,
                           CHANNEL_FRONT_CENTER, CHANNEL_BACK_LEFT,
                           CHANNEL_BACK_RIGHT};
    case L3F2_BACK_LFE_MAP:
      return ChannelLayout{CHANNEL_FRONT_LEFT,   CHANNEL_FRONT_RIGHT,
                           CHANNEL_FRONT_CENTER, CHANNEL_LFE,
                           CHANNEL_BACK_LEFT,    CHANNEL_BACK_RIGHT};
    case L3F3R_LFE_MAP:
      return ChannelLayout{CHANNEL_FRONT_LEFT,   CHANNEL_FRONT_RIGHT,
                           CHANNEL_FRONT_CENTER, CHANNEL_LFE,
                           CHANNEL_BACK_CENTER,  CHANNEL_SIDE_LEFT,
                           CHANNEL_SIDE_RIGHT};
    case L3F4_LFE_MAP:
      return ChannelLayout{CHANNEL_FRONT_LEFT,   CHANNEL_FRONT_RIGHT,
                           CHANNEL_FRONT_CENTER, CHANNEL_LFE,
                           CHANNEL_BACK_LEFT,    CHANNEL_BACK_RIGHT,
                           CHANNEL_SIDE_LEFT,    CHANNEL_SIDE_RIGHT};
    default:
      break;
  }

  static_assert(MAX_CHANNELS <= sizeof(ChannelMap) * 8,
                "Must be able to fit channels on bit mask");
  AutoTArray<Channel, MAX_CHANNELS> layout;
  uint32_t channels = 0;

  uint32_t i = 0;
  while (aMap) {
    if (aMap & 1) {
      channels++;
      if (channels > MAX_CHANNELS) {
        return ChannelLayout();
      }
      layout.AppendElement(static_cast<Channel>(i));
    }
    aMap >>= 1;
    i++;
  }
  return ChannelLayout(channels, layout.Elements());
}

nsCString AudioConfig::ChannelLayout::ChannelMapToString(
    const ChannelMap aChannelMap) {
  nsCString rv;

  constexpr const std::array CHANNEL_NAME = {"Front left",
                                             "Front right",
                                             "Front center",
                                             "Low frequency",
                                             "Back left",
                                             "Back right",
                                             "Front left of center",
                                             "Front right of center",
                                             "Back center",
                                             "Side left",
                                             "Side right",
                                             "Top center",
                                             "Top front left",
                                             "Top front center",
                                             "Top front right",
                                             "Top back left",
                                             "Top back center",
                                             "Top back right"};

  rv.AppendPrintf("0x%08x", aChannelMap);
  rv.Append("[");
  bool empty = true;
  for (size_t i = 0; i < CHANNEL_NAME.size(); i++) {
    if (aChannelMap & (1 << i)) {
      if (!empty) {
        rv.Append("|");
      }
      empty = false;
      rv.Append(CHANNEL_NAME[i]);
    }
  }
  rv.Append("]");

  return rv;
}

bool AudioConfig::ChannelLayout::MappingTable(const ChannelLayout& aOther,
                                              nsTArray<uint8_t>* aMap) const {
  if (!IsValid() || !aOther.IsValid() || Map() != aOther.Map()) {
    if (aMap) {
      aMap->SetLength(0);
    }
    return false;
  }
  if (!aMap) {
    return true;
  }
  aMap->SetLength(Count());
  for (uint32_t i = 0; i < Count(); i++) {
    for (uint32_t j = 0; j < Count(); j++) {
      if (aOther[j] == mChannels[i]) {
        (*aMap)[j] = i;
        break;
      }
    }
  }
  return true;
}

/**
 * AudioConfig::ChannelConfig
 */

/* static */ const char* AudioConfig::FormatToString(
    AudioConfig::SampleFormat aFormat) {
  switch (aFormat) {
    case FORMAT_U8:
      return "unsigned 8 bit";
    case FORMAT_S16:
      return "signed 16 bit";
    case FORMAT_S24:
      return "signed 24 bit MSB";
    case FORMAT_S24LSB:
      return "signed 24 bit LSB";
    case FORMAT_S32:
      return "signed 32 bit";
    case FORMAT_FLT:
      return "32 bit floating point";
    case FORMAT_NONE:
      return "none";
    default:
      return "unknown";
  }
}
/* static */
uint32_t AudioConfig::SampleSize(AudioConfig::SampleFormat aFormat) {
  switch (aFormat) {
    case FORMAT_U8:
      return 1;
    case FORMAT_S16:
      return 2;
    case FORMAT_S24:
      [[fallthrough]];
    case FORMAT_S24LSB:
      [[fallthrough]];
    case FORMAT_S32:
      [[fallthrough]];
    case FORMAT_FLT:
      return 4;
    case FORMAT_NONE:
    default:
      return 0;
  }
}

/* static */
uint32_t AudioConfig::FormatToBits(AudioConfig::SampleFormat aFormat) {
  switch (aFormat) {
    case FORMAT_U8:
      return 8;
    case FORMAT_S16:
      return 16;
    case FORMAT_S24LSB:
      [[fallthrough]];
    case FORMAT_S24:
      return 24;
    case FORMAT_S32:
      [[fallthrough]];
    case FORMAT_FLT:
      return 32;
    case FORMAT_NONE:
      [[fallthrough]];
    default:
      return 0;
  }
}

AudioConfig::AudioConfig(const ChannelLayout& aChannelLayout, uint32_t aRate,
                         AudioConfig::SampleFormat aFormat, bool aInterleaved)
    : mChannelLayout(aChannelLayout),
      mChannels(aChannelLayout.Count()),
      mRate(aRate),
      mFormat(aFormat),
      mInterleaved(aInterleaved) {}

AudioConfig::AudioConfig(const ChannelLayout& aChannelLayout,
                         uint32_t aChannels, uint32_t aRate,
                         AudioConfig::SampleFormat aFormat, bool aInterleaved)
    : mChannelLayout(aChannelLayout),
      mChannels(aChannels),
      mRate(aRate),
      mFormat(aFormat),
      mInterleaved(aInterleaved) {}

AudioConfig::AudioConfig(uint32_t aChannels, uint32_t aRate,
                         AudioConfig::SampleFormat aFormat, bool aInterleaved)
    : mChannelLayout(aChannels),
      mChannels(aChannels),
      mRate(aRate),
      mFormat(aFormat),
      mInterleaved(aInterleaved) {}

}  // namespace mozilla

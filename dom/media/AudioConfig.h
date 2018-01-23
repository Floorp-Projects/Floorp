/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(AudioLayout_h)
#define AudioLayout_h

#include <initializer_list>
#include <cstdint>
#include "nsTArray.h"

namespace mozilla {

  // Maximum channel number we can currently handle (7.1)
#define MAX_AUDIO_CHANNELS 8

class AudioConfig
{
public:
  // Channel definition is conveniently defined to be in the same order as
  // WAVEFORMAT && SMPTE, even though this is unused for now.
  enum Channel
  {
    CHANNEL_INVALID = -1,
    CHANNEL_FRONT_LEFT = 0,
    CHANNEL_FRONT_RIGHT,
    CHANNEL_FRONT_CENTER,
    CHANNEL_LFE,
    CHANNEL_BACK_LEFT,
    CHANNEL_BACK_RIGHT,
    CHANNEL_FRONT_LEFT_OF_CENTER,
    CHANNEL_FRONT_RIGHT_OF_CENTER,
    CHANNEL_BACK_CENTER,
    CHANNEL_SIDE_LEFT,
    CHANNEL_SIDE_RIGHT,
    // From WAVEFORMAT definition.
    CHANNEL_TOP_CENTER,
    CHANNEL_TOP_FRONT_LEFT,
    CHANNEL_TOP_FRONT_CENTER,
    CHANNEL_TOP_FRONT_RIGHT,
    CHANNEL_TOP_BACK_LEFT,
    CHANNEL_TOP_BACK_CENTER,
    CHANNEL_TOP_BACK_RIGHT
  };

  class ChannelLayout
  {
  public:
    typedef uint32_t ChannelMap;

    ChannelLayout() : mChannelMap(0), mValid(false) { }
    explicit ChannelLayout(uint32_t aChannels)
      : ChannelLayout(aChannels, DefaultLayoutForChannels(aChannels))
    {
    }
    ChannelLayout(uint32_t aChannels, const Channel* aConfig)
      : ChannelLayout()
    {
      if (aChannels == 0 || !aConfig) {
        mValid = false;
        return;
      }
      mChannels.AppendElements(aConfig, aChannels);
      UpdateChannelMap();
    }
    explicit ChannelLayout(std::initializer_list<Channel> aChannelList)
      : ChannelLayout(aChannelList.size(), aChannelList.begin())
    {
    }
    bool operator==(const ChannelLayout& aOther) const
    {
      return mChannels == aOther.mChannels;
    }
    bool operator!=(const ChannelLayout& aOther) const
    {
      return mChannels != aOther.mChannels;
    }
    const Channel& operator[](uint32_t aIndex) const
    {
      return mChannels[aIndex];
    }
    uint32_t Count() const
    {
      return mChannels.Length();
    }
    ChannelMap Map() const;

    // Calculate the mapping table from the current layout to aOther such that
    // one can easily go from one layout to the other by doing:
    // out[channel] = in[map[channel]].
    // Returns true if the reordering is possible or false otherwise.
    // If true, then aMap, if set, will be updated to contain the mapping table
    // allowing conversion from the current layout to aOther.
    // If aMap is nullptr, then MappingTable can be used to simply determine if
    // the current layout can be easily reordered to aOther.
    // aMap must be an array of size MAX_AUDIO_CHANNELS.
    bool MappingTable(const ChannelLayout& aOther, uint8_t* aMap = nullptr) const;
    bool IsValid() const { return mValid; }
    bool HasChannel(Channel aChannel) const
    {
      return mChannelMap & (1 << aChannel);
    }

    static ChannelLayout SMPTEDefault(
      const ChannelLayout& aChannelLayout);
    static ChannelLayout SMPTEDefault(ChannelMap aMap);

    static constexpr ChannelMap UNKNOWN_MAP = 0;

    // Common channel layout definitions.
    static ChannelLayout LMONO;
    static constexpr ChannelMap LMONO_MAP = 1 << CHANNEL_FRONT_CENTER;
    static ChannelLayout LMONO_LFE;
    static constexpr ChannelMap LMONO_LFE_MAP =
      1 << CHANNEL_FRONT_CENTER | 1 << CHANNEL_LFE;
    static ChannelLayout LSTEREO;
    static constexpr ChannelMap LSTEREO_MAP =
      1 << CHANNEL_FRONT_LEFT | 1 << CHANNEL_FRONT_RIGHT;
    static ChannelLayout LSTEREO_LFE;
    static constexpr ChannelMap LSTEREO_LFE_MAP =
      1 << CHANNEL_FRONT_LEFT | 1 << CHANNEL_FRONT_RIGHT | 1 << CHANNEL_LFE;
    static ChannelLayout L3F;
    static constexpr ChannelMap L3F_MAP = 1 << CHANNEL_FRONT_LEFT |
                                        1 << CHANNEL_FRONT_RIGHT |
                                        1 << CHANNEL_FRONT_CENTER;
    static ChannelLayout L3F_LFE;
    static constexpr ChannelMap L3F_LFE_MAP =
      1 << CHANNEL_FRONT_LEFT | 1 << CHANNEL_FRONT_RIGHT |
      1 << CHANNEL_FRONT_CENTER | 1 << CHANNEL_LFE;
    static ChannelLayout L2F1;
    static constexpr ChannelMap L2F1_MAP = 1 << CHANNEL_FRONT_LEFT |
                                         1 << CHANNEL_FRONT_RIGHT |
                                         1 << CHANNEL_BACK_CENTER;
    static ChannelLayout L2F1_LFE;
    static constexpr ChannelMap L2F1_LFE_MAP =
      1 << CHANNEL_FRONT_LEFT | 1 << CHANNEL_FRONT_RIGHT | 1 << CHANNEL_LFE |
      1 << CHANNEL_BACK_CENTER;
    static ChannelLayout L3F1;
    static constexpr ChannelMap L3F1_MAP =
      1 << CHANNEL_FRONT_LEFT | 1 << CHANNEL_FRONT_RIGHT |
      1 << CHANNEL_FRONT_CENTER | 1 << CHANNEL_BACK_CENTER;
    static ChannelLayout LSURROUND; // Same as 3F1
    static constexpr ChannelMap LSURROUND_MAP = L3F1_MAP;
    static ChannelLayout L3F1_LFE;
    static constexpr ChannelMap L3F1_LFE_MAP =
      1 << CHANNEL_FRONT_LEFT | 1 << CHANNEL_FRONT_RIGHT |
      1 << CHANNEL_FRONT_CENTER | 1 << CHANNEL_LFE | 1 << CHANNEL_BACK_CENTER;
    static ChannelLayout L2F2;
    static constexpr ChannelMap L2F2_MAP =
      1 << CHANNEL_FRONT_LEFT | 1 << CHANNEL_FRONT_RIGHT |
      1 << CHANNEL_SIDE_LEFT | 1 << CHANNEL_SIDE_RIGHT;
    static ChannelLayout L2F2_LFE;
    static constexpr ChannelMap L2F2_LFE_MAP =
      1 << CHANNEL_FRONT_LEFT | 1 << CHANNEL_FRONT_RIGHT | 1 << CHANNEL_LFE |
      1 << CHANNEL_SIDE_LEFT | 1 << CHANNEL_SIDE_RIGHT;
    static ChannelLayout LQUAD;
    static constexpr ChannelMap LQUAD_MAP =
      1 << CHANNEL_FRONT_LEFT | 1 << CHANNEL_FRONT_RIGHT |
      1 << CHANNEL_BACK_LEFT | 1 << CHANNEL_BACK_RIGHT;
    static ChannelLayout LQUAD_LFE;
    static constexpr ChannelMap LQUAD_MAP_LFE =
      1 << CHANNEL_FRONT_LEFT | 1 << CHANNEL_FRONT_RIGHT | 1 << CHANNEL_LFE |
      1 << CHANNEL_BACK_LEFT | 1 << CHANNEL_BACK_RIGHT;
    static ChannelLayout L3F2;
    static constexpr ChannelMap L3F2_MAP =
      1 << CHANNEL_FRONT_LEFT | 1 << CHANNEL_FRONT_RIGHT |
      1 << CHANNEL_FRONT_CENTER | 1 << CHANNEL_SIDE_LEFT |
      1 << CHANNEL_SIDE_RIGHT;
    static ChannelLayout L3F2_LFE;
    static constexpr ChannelMap L3F2_LFE_MAP =
      1 << CHANNEL_FRONT_LEFT | 1 << CHANNEL_FRONT_RIGHT |
      1 << CHANNEL_FRONT_CENTER | 1 << CHANNEL_LFE | 1 << CHANNEL_SIDE_LEFT |
      1 << CHANNEL_SIDE_RIGHT;
    // 3F2_LFE Alias
    static ChannelLayout L5POINT1_SURROUND;
    static constexpr ChannelMap L5POINT1_SURROUND_MAP = L3F2_LFE_MAP;
    static ChannelLayout L3F3R_LFE;
    static constexpr ChannelMap L3F3R_LFE_MAP =
      1 << CHANNEL_FRONT_LEFT | 1 << CHANNEL_FRONT_RIGHT |
      1 << CHANNEL_FRONT_CENTER | 1 << CHANNEL_LFE | 1 << CHANNEL_BACK_CENTER |
      1 << CHANNEL_SIDE_LEFT | 1 << CHANNEL_SIDE_RIGHT;
    static ChannelLayout L3F4_LFE;
    static constexpr ChannelMap L3F4_LFE_MAP =
      1 << CHANNEL_FRONT_LEFT | 1 << CHANNEL_FRONT_RIGHT |
      1 << CHANNEL_FRONT_CENTER | 1 << CHANNEL_LFE | 1 << CHANNEL_BACK_LEFT |
      1 << CHANNEL_BACK_RIGHT | 1 << CHANNEL_SIDE_LEFT |
      1 << CHANNEL_SIDE_RIGHT;
    // 3F4_LFE Alias
    static ChannelLayout L7POINT1_SURROUND;
    static constexpr ChannelMap L7POINT1_SURROUND_MAP = L3F4_LFE_MAP;

  private:
    void UpdateChannelMap();
    const Channel* DefaultLayoutForChannels(uint32_t aChannels) const;
    AutoTArray<Channel, MAX_AUDIO_CHANNELS> mChannels;
    ChannelMap mChannelMap;
    bool mValid;
  };

  enum SampleFormat
  {
    FORMAT_NONE = 0,
    FORMAT_U8,
    FORMAT_S16,
    FORMAT_S24LSB,
    FORMAT_S24,
    FORMAT_S32,
    FORMAT_FLT,
#if defined(MOZ_SAMPLE_TYPE_FLOAT32)
    FORMAT_DEFAULT = FORMAT_FLT
#elif defined(MOZ_SAMPLE_TYPE_S16)
    FORMAT_DEFAULT = FORMAT_S16
#else
#error "Not supported audio type"
#endif
  };

  AudioConfig(const ChannelLayout& aChannelLayout,
              uint32_t aRate,
              AudioConfig::SampleFormat aFormat = FORMAT_DEFAULT,
              bool aInterleaved = true);
  // Will create a channel configuration from default SMPTE ordering.
  AudioConfig(uint32_t aChannels,
              uint32_t aRate,
              AudioConfig::SampleFormat aFormat = FORMAT_DEFAULT,
              bool aInterleaved = true);

  const ChannelLayout& Layout() const { return mChannelLayout; }
  uint32_t Channels() const
  {
    if (!mChannelLayout.IsValid()) {
      return mChannels;
    }
    return mChannelLayout.Count();
  }
  uint32_t Rate() const { return mRate; }
  SampleFormat Format() const { return mFormat; }
  bool Interleaved() const { return mInterleaved; }
  bool operator==(const AudioConfig& aOther) const
  {
    return mChannelLayout == aOther.mChannelLayout && mRate == aOther.mRate &&
           mFormat == aOther.mFormat && mInterleaved == aOther.mInterleaved;
  }
  bool operator!=(const AudioConfig& aOther) const
  {
    return !(*this == aOther);
  }

  bool IsValid() const
  {
    return mChannelLayout.IsValid() && Format() != FORMAT_NONE && Rate() > 0;
  }

  static const char* FormatToString(SampleFormat aFormat);
  static uint32_t SampleSize(SampleFormat aFormat);
  static uint32_t FormatToBits(SampleFormat aFormat);

private:
  // Channels configuration.
  ChannelLayout mChannelLayout;

  // Channel count.
  uint32_t mChannels;

  // Sample rate.
  uint32_t mRate;

  // Sample format.
  SampleFormat mFormat;

  bool mInterleaved;
};

} // namespace mozilla

#endif // AudioLayout_h

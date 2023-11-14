/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(AudioLayout_h)
#  define AudioLayout_h

#  include <cstdint>
#  include <initializer_list>
#  include "mozilla/MathAlgorithms.h"
#  include "nsTArray.h"
#  include "cubeb/cubeb.h"

namespace mozilla {

class AudioConfig {
 public:
  // Channel definition is conveniently defined to be in the same order as
  // WAVEFORMAT && SMPTE, even though this is unused for now.
  enum Channel {
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

  class ChannelLayout {
   public:
    // The maximum number of channels a channel map can represent.
    static constexpr uint32_t MAX_CHANNELS = 32;

    using ChannelMap = uint32_t;

    ChannelLayout() : mChannelMap(UNKNOWN_MAP), mValid(false) {}
    explicit ChannelLayout(uint32_t aChannels)
        : ChannelLayout(aChannels, DefaultLayoutForChannels(aChannels)) {}
    ChannelLayout(uint32_t aChannels, const Channel* aConfig)
        : ChannelLayout() {
      if (aChannels == 0 || !aConfig) {
        return;
      }
      mChannels.AppendElements(aConfig, aChannels);
      UpdateChannelMap();
    }
    explicit ChannelLayout(std::initializer_list<Channel> aChannelList)
        : ChannelLayout(aChannelList.size(), aChannelList.begin()) {}
    bool operator==(const ChannelLayout& aOther) const {
      return mChannels == aOther.mChannels;
    }
    bool operator!=(const ChannelLayout& aOther) const {
      return mChannels != aOther.mChannels;
    }
    const Channel& operator[](uint32_t aIndex) const {
      MOZ_ASSERT(mChannels.Length() > aIndex);
      return mChannels[aIndex];
    }
    uint32_t Count() const { return mChannels.Length(); }
    ChannelMap Map() const;

    // Calculate the mapping table from the current layout to aOther such that
    // one can easily go from one layout to the other by doing:
    // out[channel] = in[map[channel]].
    // Returns true if the reordering is possible or false otherwise.
    // If true, then aMap, if set, will be updated to contain the mapping table
    // allowing conversion from the current layout to aOther.
    // If aMap is empty, then MappingTable can be used to simply determine if
    // the current layout can be easily reordered to aOther.
    bool MappingTable(const ChannelLayout& aOther,
                      nsTArray<uint8_t>* aMap = nullptr) const;
    bool IsValid() const { return mValid; }
    bool HasChannel(Channel aChannel) const {
      return mChannelMap & (1 << aChannel);
    }
    // Return the number of channels found in this ChannelMap.
    static uint32_t Channels(ChannelMap aMap) {
      static_assert(sizeof(ChannelMap) == sizeof(uint32_t),
                    "Must adjust ChannelMap type");
      return CountPopulation32(aMap);
    }

    static ChannelLayout SMPTEDefault(const ChannelLayout& aChannelLayout);
    static ChannelLayout SMPTEDefault(ChannelMap aMap);
    // Convert a channel map to a human readable string for debugging purposes.
    static nsCString ChannelMapToString(const ChannelMap aChannelMap);

    static constexpr ChannelMap UNKNOWN_MAP = 0;

    // Common channel layout definitions.
    static constexpr ChannelMap LMONO_MAP = 1 << CHANNEL_FRONT_CENTER;
    static constexpr ChannelMap LMONO_LFE_MAP =
        1 << CHANNEL_FRONT_CENTER | 1 << CHANNEL_LFE;
    static constexpr ChannelMap LSTEREO_MAP =
        1 << CHANNEL_FRONT_LEFT | 1 << CHANNEL_FRONT_RIGHT;
    static constexpr ChannelMap LSTEREO_LFE_MAP =
        1 << CHANNEL_FRONT_LEFT | 1 << CHANNEL_FRONT_RIGHT | 1 << CHANNEL_LFE;
    static constexpr ChannelMap L3F_MAP = 1 << CHANNEL_FRONT_LEFT |
                                          1 << CHANNEL_FRONT_RIGHT |
                                          1 << CHANNEL_FRONT_CENTER;
    static constexpr ChannelMap L3F_LFE_MAP =
        1 << CHANNEL_FRONT_LEFT | 1 << CHANNEL_FRONT_RIGHT |
        1 << CHANNEL_FRONT_CENTER | 1 << CHANNEL_LFE;
    static constexpr ChannelMap L2F1_MAP = 1 << CHANNEL_FRONT_LEFT |
                                           1 << CHANNEL_FRONT_RIGHT |
                                           1 << CHANNEL_BACK_CENTER;
    static constexpr ChannelMap L2F1_LFE_MAP =
        1 << CHANNEL_FRONT_LEFT | 1 << CHANNEL_FRONT_RIGHT | 1 << CHANNEL_LFE |
        1 << CHANNEL_BACK_CENTER;
    static constexpr ChannelMap L3F1_MAP =
        1 << CHANNEL_FRONT_LEFT | 1 << CHANNEL_FRONT_RIGHT |
        1 << CHANNEL_FRONT_CENTER | 1 << CHANNEL_BACK_CENTER;
    static constexpr ChannelMap LSURROUND_MAP = L3F1_MAP;
    static constexpr ChannelMap L3F1_LFE_MAP =
        1 << CHANNEL_FRONT_LEFT | 1 << CHANNEL_FRONT_RIGHT |
        1 << CHANNEL_FRONT_CENTER | 1 << CHANNEL_LFE | 1 << CHANNEL_BACK_CENTER;
    static constexpr ChannelMap L2F2_MAP =
        1 << CHANNEL_FRONT_LEFT | 1 << CHANNEL_FRONT_RIGHT |
        1 << CHANNEL_SIDE_LEFT | 1 << CHANNEL_SIDE_RIGHT;
    static constexpr ChannelMap L2F2_LFE_MAP =
        1 << CHANNEL_FRONT_LEFT | 1 << CHANNEL_FRONT_RIGHT | 1 << CHANNEL_LFE |
        1 << CHANNEL_SIDE_LEFT | 1 << CHANNEL_SIDE_RIGHT;
    static constexpr ChannelMap LQUAD_MAP =
        1 << CHANNEL_FRONT_LEFT | 1 << CHANNEL_FRONT_RIGHT |
        1 << CHANNEL_BACK_LEFT | 1 << CHANNEL_BACK_RIGHT;
    static constexpr ChannelMap LQUAD_LFE_MAP =
        1 << CHANNEL_FRONT_LEFT | 1 << CHANNEL_FRONT_RIGHT | 1 << CHANNEL_LFE |
        1 << CHANNEL_BACK_LEFT | 1 << CHANNEL_BACK_RIGHT;
    static constexpr ChannelMap L3F2_MAP =
        1 << CHANNEL_FRONT_LEFT | 1 << CHANNEL_FRONT_RIGHT |
        1 << CHANNEL_FRONT_CENTER | 1 << CHANNEL_SIDE_LEFT |
        1 << CHANNEL_SIDE_RIGHT;
    static constexpr ChannelMap L3F2_LFE_MAP =
        1 << CHANNEL_FRONT_LEFT | 1 << CHANNEL_FRONT_RIGHT |
        1 << CHANNEL_FRONT_CENTER | 1 << CHANNEL_LFE | 1 << CHANNEL_SIDE_LEFT |
        1 << CHANNEL_SIDE_RIGHT;
    // 3F2_LFE Alias
    static constexpr ChannelMap L5POINT1_SURROUND_MAP = L3F2_LFE_MAP;
    static constexpr ChannelMap L3F2_BACK_MAP =
        1 << CHANNEL_FRONT_LEFT | 1 << CHANNEL_FRONT_RIGHT |
        1 << CHANNEL_FRONT_CENTER | 1 << CHANNEL_BACK_LEFT |
        1 << CHANNEL_BACK_RIGHT;
    static constexpr ChannelMap L3F2_BACK_LFE_MAP =
        L3F2_BACK_MAP | 1 << CHANNEL_LFE;
    static constexpr ChannelMap L3F3R_LFE_MAP =
        1 << CHANNEL_FRONT_LEFT | 1 << CHANNEL_FRONT_RIGHT |
        1 << CHANNEL_FRONT_CENTER | 1 << CHANNEL_LFE |
        1 << CHANNEL_BACK_CENTER | 1 << CHANNEL_SIDE_LEFT |
        1 << CHANNEL_SIDE_RIGHT;
    static ChannelLayout L3F4_LFE;
    static constexpr ChannelMap L3F4_LFE_MAP =
        1 << CHANNEL_FRONT_LEFT | 1 << CHANNEL_FRONT_RIGHT |
        1 << CHANNEL_FRONT_CENTER | 1 << CHANNEL_LFE | 1 << CHANNEL_BACK_LEFT |
        1 << CHANNEL_BACK_RIGHT | 1 << CHANNEL_SIDE_LEFT |
        1 << CHANNEL_SIDE_RIGHT;
    // 3F4_LFE Alias
    static ChannelLayout L7POINT1_SURROUND;
    static constexpr ChannelMap L7POINT1_SURROUND_MAP = L3F4_LFE_MAP;

    // Statically check that we can static_cast a Gecko ChannelLayout to a
    // cubeb_channel_layout.
    static_assert(CUBEB_LAYOUT_UNDEFINED == UNKNOWN_MAP);
    static_assert(CUBEB_LAYOUT_MONO == LMONO_MAP);
    static_assert(CUBEB_LAYOUT_MONO_LFE == LMONO_LFE_MAP);
    static_assert(CUBEB_LAYOUT_STEREO == LSTEREO_MAP);
    static_assert(CUBEB_LAYOUT_STEREO_LFE == LSTEREO_LFE_MAP);
    static_assert(CUBEB_LAYOUT_3F == L3F_MAP);
    static_assert(CUBEB_LAYOUT_3F_LFE == L3F_LFE_MAP);
    static_assert(CUBEB_LAYOUT_2F1 == L2F1_MAP);
    static_assert(CUBEB_LAYOUT_2F1_LFE == L2F1_LFE_MAP);
    static_assert(CUBEB_LAYOUT_3F1 == L3F1_MAP);
    static_assert(CUBEB_LAYOUT_3F1_LFE == L3F1_LFE_MAP);
    static_assert(CUBEB_LAYOUT_2F2 == L2F2_MAP);
    static_assert(CUBEB_LAYOUT_3F2_LFE == L3F2_LFE_MAP);
    static_assert(CUBEB_LAYOUT_QUAD == LQUAD_MAP);
    static_assert(CUBEB_LAYOUT_QUAD_LFE == LQUAD_LFE_MAP);
    static_assert(CUBEB_LAYOUT_3F2 == L3F2_MAP);
    static_assert(CUBEB_LAYOUT_3F2_LFE == L3F2_LFE_MAP);
    static_assert(CUBEB_LAYOUT_3F2_BACK == L3F2_BACK_MAP);
    static_assert(CUBEB_LAYOUT_3F2_LFE_BACK == L3F2_BACK_LFE_MAP);
    static_assert(CUBEB_LAYOUT_3F3R_LFE == L3F3R_LFE_MAP);
    static_assert(CUBEB_LAYOUT_3F4_LFE == L3F4_LFE_MAP);

   private:
    void UpdateChannelMap();
    const Channel* DefaultLayoutForChannels(uint32_t aChannels) const;
    CopyableAutoTArray<Channel, MAX_CHANNELS> mChannels;
    ChannelMap mChannelMap;
    bool mValid;
  };

  enum SampleFormat {
    FORMAT_NONE = 0,
    FORMAT_U8,
    FORMAT_S16,
    FORMAT_S24LSB,
    FORMAT_S24,
    FORMAT_S32,
    FORMAT_FLT,
    FORMAT_DEFAULT = FORMAT_FLT
  };

  AudioConfig(const ChannelLayout& aChannelLayout, uint32_t aRate,
              AudioConfig::SampleFormat aFormat = FORMAT_DEFAULT,
              bool aInterleaved = true);
  AudioConfig(const ChannelLayout& aChannelLayout, uint32_t aChannels,
              uint32_t aRate,
              AudioConfig::SampleFormat aFormat = FORMAT_DEFAULT,
              bool aInterleaved = true);
  // Will create a channel configuration from default SMPTE ordering.
  AudioConfig(uint32_t aChannels, uint32_t aRate,
              AudioConfig::SampleFormat aFormat = FORMAT_DEFAULT,
              bool aInterleaved = true);

  const ChannelLayout& Layout() const { return mChannelLayout; }
  uint32_t Channels() const {
    if (!mChannelLayout.IsValid()) {
      return mChannels;
    }
    return mChannelLayout.Count();
  }
  uint32_t Rate() const { return mRate; }
  SampleFormat Format() const { return mFormat; }
  bool Interleaved() const { return mInterleaved; }
  bool operator==(const AudioConfig& aOther) const {
    return mChannelLayout == aOther.mChannelLayout && mRate == aOther.mRate &&
           mFormat == aOther.mFormat && mInterleaved == aOther.mInterleaved;
  }
  bool operator!=(const AudioConfig& aOther) const {
    return !(*this == aOther);
  }

  bool IsValid() const {
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

}  // namespace mozilla

#endif  // AudioLayout_h

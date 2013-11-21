// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mp4_demuxer/channel_layout.h"

#include "mp4_demuxer/basictypes.h"

namespace mp4_demuxer {

static const int kLayoutToChannels[] = {
    0,   // CHANNEL_LAYOUT_NONE
    0,   // CHANNEL_LAYOUT_UNSUPPORTED
    1,   // CHANNEL_LAYOUT_MONO
    2,   // CHANNEL_LAYOUT_STEREO
    3,   // CHANNEL_LAYOUT_2_1
    3,   // CHANNEL_LAYOUT_SURROUND
    4,   // CHANNEL_LAYOUT_4_0
    4,   // CHANNEL_LAYOUT_2_2
    4,   // CHANNEL_LAYOUT_QUAD
    5,   // CHANNEL_LAYOUT_5_0
    6,   // CHANNEL_LAYOUT_5_1
    5,   // CHANNEL_LAYOUT_5_0_BACK
    6,   // CHANNEL_LAYOUT_5_1_BACK
    7,   // CHANNEL_LAYOUT_7_0
    8,   // CHANNEL_LAYOUT_7_1
    8,   // CHANNEL_LAYOUT_7_1_WIDE
    2,   // CHANNEL_LAYOUT_STEREO_DOWNMIX
    3,   // CHANNEL_LAYOUT_2POINT1
    4,   // CHANNEL_LAYOUT_3_1
    5,   // CHANNEL_LAYOUT_4_1
    6,   // CHANNEL_LAYOUT_6_0
    6,   // CHANNEL_LAYOUT_6_0_FRONT
    6,   // CHANNEL_LAYOUT_HEXAGONAL
    7,   // CHANNEL_LAYOUT_6_1
    7,   // CHANNEL_LAYOUT_6_1_BACK
    7,   // CHANNEL_LAYOUT_6_1_FRONT
    7,   // CHANNEL_LAYOUT_7_0_FRONT
    8,   // CHANNEL_LAYOUT_7_1_WIDE_BACK
    8,   // CHANNEL_LAYOUT_OCTAGONAL
    0,   // CHANNEL_LAYOUT_DISCRETE
};

// The channel orderings for each layout as specified by FFmpeg.  Each value
// represents the index of each channel in each layout.  Values of -1 mean the
// channel at that index is not used for that layout.For example, the left side
// surround sound channel in FFmpeg's 5.1 layout is in the 5th position (because
// the order is L, R, C, LFE, LS, RS), so
// kChannelOrderings[CHANNEL_LAYOUT_5POINT1][SIDE_LEFT] = 4;
static const int kChannelOrderings[CHANNEL_LAYOUT_MAX][CHANNELS_MAX] = {
    // FL | FR | FC | LFE | BL | BR | FLofC | FRofC | BC | SL | SR

    // CHANNEL_LAYOUT_NONE
    {  -1 , -1 , -1 , -1  , -1 , -1 , -1    , -1    , -1 , -1 , -1 },

    // CHANNEL_LAYOUT_UNSUPPORTED
    {  -1 , -1 , -1 , -1  , -1 , -1 , -1    , -1    , -1 , -1 , -1 },

    // CHANNEL_LAYOUT_MONO
    {  -1 , -1 , 0  , -1  , -1 , -1 , -1    , -1    , -1 , -1 , -1 },

    // CHANNEL_LAYOUT_STEREO
    {  0  , 1  , -1 , -1  , -1 , -1 , -1    , -1    , -1 , -1 , -1 },

    // CHANNEL_LAYOUT_2_1
    {  0  , 1  , -1 , -1  , -1 , -1 , -1    , -1    , 2  , -1 , -1 },

    // CHANNEL_LAYOUT_SURROUND
    {  0  , 1  , 2  , -1  , -1 , -1 , -1    , -1    , -1 , -1 , -1 },

    // CHANNEL_LAYOUT_4_0
    {  0  , 1  , 2  , -1  , -1 , -1 , -1    , -1    , 3  , -1 , -1 },

    // CHANNEL_LAYOUT_2_2
    {  0  , 1  , -1 , -1  , -1 , -1 , -1    , -1    , -1 , 2  ,  3 },

    // CHANNEL_LAYOUT_QUAD
    {  0  , 1  , -1 , -1  , 2  , 3  , -1    , -1    , -1 , -1 , -1 },

    // CHANNEL_LAYOUT_5_0
    {  0  , 1  , 2  , -1  , -1 , -1 , -1    , -1    , -1 , 3  ,  4 },

    // CHANNEL_LAYOUT_5_1
    {  0  , 1  , 2  , 3   , -1 , -1 , -1    , -1    , -1 , 4  ,  5 },

    // FL | FR | FC | LFE | BL | BR | FLofC | FRofC | BC | SL | SR

    // CHANNEL_LAYOUT_5_0_BACK
    {  0  , 1  , 2  , -1  , 3  , 4  , -1    , -1    , -1 , -1 , -1 },

    // CHANNEL_LAYOUT_5_1_BACK
    {  0  , 1  , 2  , 3   , 4  , 5  , -1    , -1    , -1 , -1 , -1 },

    // CHANNEL_LAYOUT_7_0
    {  0  , 1  , 2  , -1  , 5  , 6  , -1    , -1    , -1 , 3  ,  4 },

    // CHANNEL_LAYOUT_7_1
    {  0  , 1  , 2  , 3   , 6  , 7  , -1    , -1    , -1 , 4  ,  5 },

    // CHANNEL_LAYOUT_7_1_WIDE
    {  0  , 1  , 2  , 3   , -1 , -1 , 6     , 7     , -1 , 4  ,  5 },

    // CHANNEL_LAYOUT_STEREO_DOWNMIX
    {  0  , 1  , -1 , -1  , -1 , -1 , -1    , -1    , -1 , -1 , -1 },

    // CHANNEL_LAYOUT_2POINT1
    {  0  , 1  , -1 ,  2  , -1 , -1 , -1    , -1    , -1 , -1 , -1 },

    // CHANNEL_LAYOUT_3_1
    {  0  , 1  ,  2 ,  3  , -1 , -1 , -1    , -1    , -1 , -1 , -1 },

    // CHANNEL_LAYOUT_4_1
    {  0  , 1  ,  2 ,  4  , -1 , -1 , -1    , -1    ,  3 , -1 , -1 },

    // CHANNEL_LAYOUT_6_0
    {  0  , 1  , 2  , -1  , -1 , -1 , -1    , -1    ,  5 , 3  ,  4 },

    // CHANNEL_LAYOUT_6_0_FRONT
    {  0  , 1  , -1 , -1  , -1 , -1 ,  4    ,  5    , -1 , 2  ,  3 },

    // FL | FR | FC | LFE | BL | BR | FLofC | FRofC | BC | SL | SR

    // CHANNEL_LAYOUT_HEXAGONAL
    {  0  , 1  , 2  , -1  , 3  , 4  , -1    , -1    ,  5 , -1 , -1 },

    // CHANNEL_LAYOUT_6_1
    {  0  , 1  , 2  , 3   , -1 , -1 , -1    , -1    ,  6 , 4  ,  5 },

    // CHANNEL_LAYOUT_6_1_BACK
    {  0  , 1  , 2  , 3   , 4  , 5  , -1    , -1    ,  6 , -1 , -1 },

    // CHANNEL_LAYOUT_6_1_FRONT
    {  0  , 1  , -1 , 6   , -1 , -1 , 4     , 5     , -1 , 2  ,  3 },

    // CHANNEL_LAYOUT_7_0_FRONT
    {  0  , 1  , 2  , -1  , -1 , -1 , 5     , 6     , -1 , 3  ,  4 },

    // CHANNEL_LAYOUT_7_1_WIDE_BACK
    {  0  , 1  , 2  , 3   , 4  , 5  , 6     , 7     , -1 , -1 , -1 },

    // CHANNEL_LAYOUT_OCTAGONAL
    {  0  , 1  , 2  , -1  , 5  , 6  , -1    , -1    ,  7 , 3  ,  4 },

    // CHANNEL_LAYOUT_DISCRETE
    {  -1 , -1 , -1 , -1  , -1 , -1 , -1    , -1    , -1 , -1 , -1 },

    // FL | FR | FC | LFE | BL | BR | FLofC | FRofC | BC | SL | SR
};

int ChannelLayoutToChannelCount(ChannelLayout layout) {
  DCHECK_LT(static_cast<size_t>(layout), arraysize(kLayoutToChannels));
  return kLayoutToChannels[layout];
}

// Converts a channel count into a channel layout.
ChannelLayout GuessChannelLayout(int channels) {
  switch (channels) {
    case 1:
      return CHANNEL_LAYOUT_MONO;
    case 2:
      return CHANNEL_LAYOUT_STEREO;
    case 3:
      return CHANNEL_LAYOUT_SURROUND;
    case 4:
      return CHANNEL_LAYOUT_QUAD;
    case 5:
      return CHANNEL_LAYOUT_5_0;
    case 6:
      return CHANNEL_LAYOUT_5_1;
    case 7:
      return CHANNEL_LAYOUT_6_1;
    case 8:
      return CHANNEL_LAYOUT_7_1;
    default:
      DMX_LOG("Unsupported channel count: %d\n", channels);
  }
  return CHANNEL_LAYOUT_UNSUPPORTED;
}

int ChannelOrder(ChannelLayout layout, Channels channel) {
  DCHECK_LT(static_cast<size_t>(layout), arraysize(kChannelOrderings));
  DCHECK_LT(static_cast<size_t>(channel), arraysize(kChannelOrderings[0]));
  return kChannelOrderings[layout][channel];
}

}  // namespace mp4_demuxer

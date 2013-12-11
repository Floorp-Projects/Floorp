// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CHANNEL_LAYOUT_H_
#define MEDIA_BASE_CHANNEL_LAYOUT_H_


namespace mp4_demuxer {

// Enumerates the various representations of the ordering of audio channels.
// Logged to UMA, so never reuse a value, always add new/greater ones!
enum ChannelLayout {
  CHANNEL_LAYOUT_NONE = 0,
  CHANNEL_LAYOUT_UNSUPPORTED = 1,

  // Front C
  CHANNEL_LAYOUT_MONO = 2,

  // Front L, Front R
  CHANNEL_LAYOUT_STEREO = 3,

  // Front L, Front R, Back C
  CHANNEL_LAYOUT_2_1 = 4,

  // Front L, Front R, Front C
  CHANNEL_LAYOUT_SURROUND = 5,

  // Front L, Front R, Front C, Back C
  CHANNEL_LAYOUT_4_0 = 6,

  // Front L, Front R, Side L, Side R
  CHANNEL_LAYOUT_2_2 = 7,

  // Front L, Front R, Back L, Back R
  CHANNEL_LAYOUT_QUAD = 8,

  // Front L, Front R, Front C, Side L, Side R
  CHANNEL_LAYOUT_5_0 = 9,

  // Front L, Front R, Front C, Side L, Side R, LFE
  CHANNEL_LAYOUT_5_1 = 10,

  // Front L, Front R, Front C, Back L, Back R
  CHANNEL_LAYOUT_5_0_BACK = 11,

  // Front L, Front R, Front C, Back L, Back R, LFE
  CHANNEL_LAYOUT_5_1_BACK = 12,

  // Front L, Front R, Front C, Side L, Side R, Back L, Back R
  CHANNEL_LAYOUT_7_0 = 13,

  // Front L, Front R, Front C, Side L, Side R, LFE, Back L, Back R
  CHANNEL_LAYOUT_7_1 = 14,

  // Front L, Front R, Front C, Side L, Side R, LFE, Front LofC, Front RofC
  CHANNEL_LAYOUT_7_1_WIDE = 15,

  // Stereo L, Stereo R
  CHANNEL_LAYOUT_STEREO_DOWNMIX = 16,

  // Stereo L, Stereo R, LFE
  CHANNEL_LAYOUT_2POINT1 = 17,

  // Stereo L, Stereo R, Front C, LFE
  CHANNEL_LAYOUT_3_1 = 18,

  // Stereo L, Stereo R, Front C, Rear C, LFE
  CHANNEL_LAYOUT_4_1 = 19,

  // Stereo L, Stereo R, Front C, Side L, Side R, Back C
  CHANNEL_LAYOUT_6_0 = 20,

  // Stereo L, Stereo R, Side L, Side R, Front LofC, Front RofC
  CHANNEL_LAYOUT_6_0_FRONT = 21,

  // Stereo L, Stereo R, Side L, Side R, Front C, Rear C.
  CHANNEL_LAYOUT_HEXAGONAL = 22,

  // Stereo L, Stereo R, Side L, Side R, Front C, Rear Center, LFE
  CHANNEL_LAYOUT_6_1 = 23,

  // Stereo L, Stereo R, Back L, Back R, Front C, Rear Center, LFE
  CHANNEL_LAYOUT_6_1_BACK = 24,

  // Stereo L, Stereo R, Side L, Side R, Front LofC, Front RofC, LFE
  CHANNEL_LAYOUT_6_1_FRONT = 25,

  // Front L, Front R, Front C, Side L, Side R, Front LofC, Front RofC
  CHANNEL_LAYOUT_7_0_FRONT = 26,

  // Front L, Front R, Front C, Back L, Back R, LFE, Front LofC, Front RofC
  CHANNEL_LAYOUT_7_1_WIDE_BACK = 27,

  // Front L, Front R, Front C, Side L, Side R, Rear C, Back L, Back R.
  CHANNEL_LAYOUT_OCTAGONAL = 28,

  // Channels are not explicitly mapped to speakers.
  CHANNEL_LAYOUT_DISCRETE = 29,

  // Total number of layouts.
  CHANNEL_LAYOUT_MAX  // Must always be last!
};

enum Channels {
  LEFT = 0,
  RIGHT,
  CENTER,
  LFE,
  BACK_LEFT,
  BACK_RIGHT,
  LEFT_OF_CENTER,
  RIGHT_OF_CENTER,
  BACK_CENTER,
  SIDE_LEFT,
  SIDE_RIGHT,
  CHANNELS_MAX
};

// Returns the expected channel position in an interleaved stream.  Values of -1
// mean the channel at that index is not used for that layout.  Values range
// from 0 to CHANNELS_MAX - 1.
int ChannelOrder(ChannelLayout layout, Channels channel);

// Returns the number of channels in a given ChannelLayout.
int ChannelLayoutToChannelCount(ChannelLayout layout);

// Given the number of channels, return the best layout,
// or return CHANNEL_LAYOUT_UNSUPPORTED if there is no good match.
ChannelLayout GuessChannelLayout(int channels);

}  // namespace mp4_demuxer

#endif  // MEDIA_BASE_CHANNEL_LAYOUT_H_

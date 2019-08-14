/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "OpusParser.h"
#include <algorithm>

using namespace mozilla;

TEST(OpusParser, Mapping2)
{
  uint8_t validChannels[] = {1,   3,   4,   6,   9,   11,  16,  18,  25,  27,
                             36,  38,  49,  51,  64,  66,  81,  83,  100, 102,
                             121, 123, 144, 146, 169, 171, 196, 198, 225, 227};
  for (uint8_t channels = 0; channels < 255; channels++) {
    bool found = OpusParser::IsValidMapping2ChannelsCount(channels);
    bool foundTable =
        std::find(std::begin(validChannels), std::end(validChannels),
                  channels) != std::end(validChannels);
    EXPECT_EQ(found, foundTable);
  }
}

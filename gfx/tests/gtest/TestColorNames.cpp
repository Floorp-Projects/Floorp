/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include <string.h>
#include "nsColor.h"
#include "nsColorNames.h"
#include "prprf.h"
#include "nsString.h"
#include "mozilla/Util.h"

// define an array of all color names
#define GFX_COLOR(_name, _value) #_name,
static const char* const kColorNames[] = {
#include "nsColorNameList.h"
};
#undef GFX_COLOR

// define an array of all color name values
#define GFX_COLOR(_name, _value) _value,
static const nscolor kColors[] = {
#include "nsColorNameList.h"
};
#undef GFX_COLOR

using namespace mozilla;

static const char* kJunkNames[] = {
  nullptr,
  "",
  "123",
  "backgroundz",
  "zzzzzz",
  "#@$&@#*@*$@$#"
};

static
void RunColorTests() {
  nscolor rgb;
  // First make sure we can find all of the tags that are supposed to
  // be in the table. Futz with the case to make sure any case will
  // work

  for (uint32_t index = 0 ; index < ArrayLength(kColorNames); index++) {
    // Lookup color by name and make sure it has the right id
    nsCString tagName(kColorNames[index]);

    // Check that color lookup by name gets the right rgb value
    ASSERT_TRUE(NS_ColorNameToRGB(NS_ConvertASCIItoUTF16(tagName), &rgb)) <<
      "can't find '" << tagName.get() << "'";
    ASSERT_TRUE((rgb == kColors[index])) <<
      "failed at index " << index << " out of " << ArrayLength(kColorNames);

    // fiddle with the case to make sure we can still find it
    tagName.SetCharAt(tagName.CharAt(0) - 32, 0);
    ASSERT_TRUE(NS_ColorNameToRGB(NS_ConvertASCIItoUTF16(tagName), &rgb)) <<
      "can't find '" << tagName.get() << "'";
    ASSERT_TRUE((rgb == kColors[index])) <<
      "failed at index " << index << " out of " << ArrayLength(kColorNames);

    // Check that parsing an RGB value in hex gets the right values
    uint8_t r = NS_GET_R(rgb);
    uint8_t g = NS_GET_G(rgb);
    uint8_t b = NS_GET_B(rgb);
    uint8_t a = NS_GET_A(rgb);
    if (a != UINT8_MAX) {
      // NS_HexToRGB() can not handle a color with alpha channel
      rgb = NS_RGB(r, g, b);
    }
    char cbuf[50];
    PR_snprintf(cbuf, sizeof(cbuf), "%02x%02x%02x", r, g, b);
    nscolor hexrgb;
    ASSERT_TRUE(NS_HexToRGB(NS_ConvertASCIItoUTF16(cbuf), &hexrgb)) <<
      "hex conversion to color of '" << cbuf << "'";
    ASSERT_TRUE(hexrgb == rgb);
  }
}

static
void RunJunkColorTests() {
  nscolor rgb;
  // Now make sure we don't find some garbage
  for (uint32_t i = 0; i < ArrayLength(kJunkNames); i++) {
    nsCString tag(kJunkNames[i]);
    ASSERT_FALSE(NS_ColorNameToRGB(NS_ConvertASCIItoUTF16(tag), &rgb)) <<
      "Failed at junk color " << kJunkNames[i];
  }
}

TEST(Gfx, ColorNames) {
  RunColorTests();
}

TEST(Gfx, JunkColorNames) {
  RunJunkColorTests();
}

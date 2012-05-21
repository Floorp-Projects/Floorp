/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestHarness.h"

#include <string.h>
#include "nsColor.h"
#include "nsColorNames.h"
#include "prprf.h"
#include "nsString.h"

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

static const char* kJunkNames[] = {
  nsnull,
  "",
  "123",
  "backgroundz",
  "zzzzzz",
  "#@$&@#*@*$@$#"
};

int main(int argc, char** argv)
{
  ScopedXPCOM xpcom("TestColorNames");
  if (xpcom.failed())
    return 1;

  nscolor rgb;
  int rv = 0;

  // First make sure we can find all of the tags that are supposed to
  // be in the table. Futz with the case to make sure any case will
  // work

  for (PRUint32 index = 0 ; index < ArrayLength(kColorNames); index++) {
    // Lookup color by name and make sure it has the right id
    nsCString tagName(kColorNames[index]);

    // Check that color lookup by name gets the right rgb value
    if (!NS_ColorNameToRGB(NS_ConvertASCIItoUTF16(tagName), &rgb)) {
      fail("can't find '%s'", tagName.get());
      rv = 1;
    }
    if (rgb != kColors[index]) {
      fail("name='%s' ColorNameToRGB=%x kColors[%d]=%08x",
           tagName.get(), rgb, index, kColors[index]);
      rv = 1;
    }

    // fiddle with the case to make sure we can still find it
    tagName.SetCharAt(tagName.CharAt(0) - 32, 0);
    if (!NS_ColorNameToRGB(NS_ConvertASCIItoUTF16(tagName), &rgb)) {
      fail("can't find '%s'", tagName.get());
      rv = 1;
    }
    if (rgb != kColors[index]) {
      fail("name='%s' ColorNameToRGB=%x kColors[%d]=%08x",
           tagName.get(), rgb, index, kColors[index]);
      rv = 1;
    }

    // Check that parsing an RGB value in hex gets the right values
    PRUint8 r = NS_GET_R(rgb);
    PRUint8 g = NS_GET_G(rgb);
    PRUint8 b = NS_GET_B(rgb);
    PRUint8 a = NS_GET_A(rgb);
    if (a != PR_UINT8_MAX) {
      // NS_HexToRGB() can not handle a color with alpha channel
      rgb = NS_RGB(r, g, b);
    }
    char cbuf[50];
    PR_snprintf(cbuf, sizeof(cbuf), "%02x%02x%02x", r, g, b);
    nscolor hexrgb;
    if (!NS_HexToRGB(NS_ConvertASCIItoUTF16(cbuf), &hexrgb)) {
      fail("hex conversion to color of '%s'", cbuf);
      rv = 1;
    }
    if (hexrgb != rgb) {
      fail("rgb=%x hexrgb=%x", rgb, hexrgb);
      rv = 1;
    }
  }

  // Now make sure we don't find some garbage
  for (PRUint32 i = 0; i < ArrayLength(kJunkNames); i++) {
    nsCString tag(kJunkNames[i]);
    if (NS_ColorNameToRGB(NS_ConvertASCIItoUTF16(tag), &rgb)) {
      fail("found '%s'", kJunkNames[i] ? kJunkNames[i] : "(null)");
      rv = 1;
    }
  }

  if (rv == 0)
    passed("TestColorNames");
  return rv;
}

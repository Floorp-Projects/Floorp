/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"         // for ArrayLength
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "mozilla/MathAlgorithms.h"

#include "nsColor.h"
#include <sys/types.h>                  // for int32_t
#include "nsColorNames.h"               // for nsColorNames
#include "nsDebug.h"                    // for NS_ASSERTION, etc
#include "nsStaticNameTable.h"
#include "nsString.h"                   // for nsAutoCString, nsString, etc
#include "nscore.h"                     // for nsAString, etc

using namespace mozilla;

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

#define eColorName_COUNT (ArrayLength(kColorNames))
#define eColorName_UNKNOWN (-1)

static nsStaticCaseInsensitiveNameTable* gColorTable = nullptr;

void nsColorNames::AddRefTable(void) 
{
  NS_ASSERTION(!gColorTable, "pre existing array!");
  if (!gColorTable) {
    gColorTable =
      new nsStaticCaseInsensitiveNameTable(kColorNames, eColorName_COUNT);
  }
}

void nsColorNames::ReleaseTable(void)
{
  if (gColorTable) {
    delete gColorTable;
    gColorTable = nullptr;
  }
}

static int ComponentValue(const char16_t* aColorSpec, int aLen, int color, int dpc)
{
  int component = 0;
  int index = (color * dpc);
  if (2 < dpc) {
    dpc = 2;
  }
  while (--dpc >= 0) {
    char16_t ch = ((index < aLen) ? aColorSpec[index++] : '0');
    if (('0' <= ch) && (ch <= '9')) {
      component = (component * 16) + (ch - '0');
    } else if ((('a' <= ch) && (ch <= 'f')) || 
               (('A' <= ch) && (ch <= 'F'))) {
      // "ch&7" handles lower and uppercase hex alphabetics
      component = (component * 16) + (ch & 7) + 9;
    }
    else {  // not a hex digit, treat it like 0
      component = (component * 16);
    }
  }
  return component;
}

bool
NS_HexToRGBA(const nsAString& aColorSpec, nsHexColorType aType,
             nscolor* aResult)
{
  const char16_t* buffer = aColorSpec.BeginReading();

  int nameLen = aColorSpec.Length();
  bool hasAlpha = false;
  if (nameLen != 3 && nameLen != 6) {
    if ((nameLen != 4 && nameLen != 8) || aType == nsHexColorType::NoAlpha) {
      // Improperly formatted color value
      return false;
    }
    hasAlpha = true;
  }

  // Make sure the digits are legal
  for (int i = 0; i < nameLen; i++) {
    char16_t ch = buffer[i];
    if (((ch >= '0') && (ch <= '9')) ||
        ((ch >= 'a') && (ch <= 'f')) ||
        ((ch >= 'A') && (ch <= 'F'))) {
      // Legal character
      continue;
    }
    // Whoops. Illegal character.
    return false;
  }

  // Convert the ascii to binary
  int dpc = ((nameLen <= 4) ? 1 : 2);
  // Translate components from hex to binary
  int r = ComponentValue(buffer, nameLen, 0, dpc);
  int g = ComponentValue(buffer, nameLen, 1, dpc);
  int b = ComponentValue(buffer, nameLen, 2, dpc);
  int a;
  if (hasAlpha) {
    a = ComponentValue(buffer, nameLen, 3, dpc);
  } else {
    a = (dpc == 1) ? 0xf : 0xff;
  }
  if (dpc == 1) {
    // Scale single digit component to an 8 bit value. Replicate the
    // single digit to compute the new value.
    r = (r << 4) | r;
    g = (g << 4) | g;
    b = (b << 4) | b;
    a = (a << 4) | a;
  }
  NS_ASSERTION((r >= 0) && (r <= 255), "bad r");
  NS_ASSERTION((g >= 0) && (g <= 255), "bad g");
  NS_ASSERTION((b >= 0) && (b <= 255), "bad b");
  NS_ASSERTION((a >= 0) && (a <= 255), "bad a");
  *aResult = NS_RGBA(r, g, b, a);
  return true;
}

// This implements part of the algorithm for legacy behavior described in
// http://www.whatwg.org/specs/web-apps/current-work/complete/common-microsyntaxes.html#rules-for-parsing-a-legacy-color-value
bool NS_LooseHexToRGB(const nsString& aColorSpec, nscolor* aResult)
{
  if (aColorSpec.EqualsLiteral("transparent")) {
    return false;
  }

  int nameLen = aColorSpec.Length();
  const char16_t* colorSpec = aColorSpec.get();
  if (nameLen > 128) {
    nameLen = 128;
  }

  if ('#' == colorSpec[0]) {
    ++colorSpec;
    --nameLen;
  }

  // digits per component
  int dpc = (nameLen + 2) / 3;
  int newdpc = dpc;

  // Use only the rightmost 8 characters of each component.
  if (newdpc > 8) {
    nameLen -= newdpc - 8;
    colorSpec += newdpc - 8;
    newdpc = 8;
  }

  // And then keep trimming characters at the left until we'd trim one
  // that would leave a nonzero value, but not past 2 characters per
  // component.
  while (newdpc > 2) {
    bool haveNonzero = false;
    for (int c = 0; c < 3; ++c) {
      MOZ_ASSERT(c * dpc < nameLen,
                 "should not pass end of string while newdpc > 2");
      char16_t ch = colorSpec[c * dpc];
      if (('1' <= ch && ch <= '9') ||
          ('A' <= ch && ch <= 'F') ||
          ('a' <= ch && ch <= 'f')) {
        haveNonzero = true;
        break;
      }
    }
    if (haveNonzero) {
      break;
    }
    --newdpc;
    --nameLen;
    ++colorSpec;
  }

  // Translate components from hex to binary
  int r = ComponentValue(colorSpec, nameLen, 0, dpc);
  int g = ComponentValue(colorSpec, nameLen, 1, dpc);
  int b = ComponentValue(colorSpec, nameLen, 2, dpc);
  NS_ASSERTION((r >= 0) && (r <= 255), "bad r");
  NS_ASSERTION((g >= 0) && (g <= 255), "bad g");
  NS_ASSERTION((b >= 0) && (b <= 255), "bad b");

  *aResult = NS_RGB(r, g, b);
  return true;
}

bool NS_ColorNameToRGB(const nsAString& aColorName, nscolor* aResult)
{
  if (!gColorTable) return false;

  int32_t id = gColorTable->Lookup(aColorName);
  if (eColorName_UNKNOWN < id) {
    NS_ASSERTION(uint32_t(id) < eColorName_COUNT,
                 "gColorTable->Lookup messed up");
    if (aResult) {
      *aResult = kColors[id];
    }
    return true;
  }
  return false;
}

// Macro to blend two colors
//
// equivalent to target = (bg*(255-fgalpha) + fg*fgalpha)/255
#define MOZ_BLEND(target, bg, fg, fgalpha)       \
  FAST_DIVIDE_BY_255(target, (bg)*(255-fgalpha) + (fg)*(fgalpha))

nscolor
NS_ComposeColors(nscolor aBG, nscolor aFG)
{
  // This function uses colors that are non premultiplied alpha.
  int r, g, b, a;

  int bgAlpha = NS_GET_A(aBG);
  int fgAlpha = NS_GET_A(aFG);

  // Compute the final alpha of the blended color
  // a = fgAlpha + bgAlpha*(255 - fgAlpha)/255;
  FAST_DIVIDE_BY_255(a, bgAlpha*(255-fgAlpha));
  a = fgAlpha + a;
  int blendAlpha;
  if (a == 0) {
    // In this case the blended color is totally trasparent,
    // we preserve the color information of the foreground color.
    blendAlpha = 255;
  } else {
    blendAlpha = (fgAlpha*255)/a;
  }
  MOZ_BLEND(r, NS_GET_R(aBG), NS_GET_R(aFG), blendAlpha);
  MOZ_BLEND(g, NS_GET_G(aBG), NS_GET_G(aFG), blendAlpha);
  MOZ_BLEND(b, NS_GET_B(aBG), NS_GET_B(aFG), blendAlpha);

  return NS_RGBA(r, g, b, a);
}

// Functions to convert from HSL color space to RGB color space.
// This is the algorithm described in the CSS3 specification

// helper
static float
HSL_HueToRGB(float m1, float m2, float h)
{
  if (h < 0.0f)
    h += 1.0f;
  if (h > 1.0f)
    h -= 1.0f;
  if (h < (float)(1.0/6.0))
    return m1 + (m2 - m1)*h*6.0f;
  if (h < (float)(1.0/2.0))
    return m2;
  if (h < (float)(2.0/3.0))
    return m1 + (m2 - m1)*((float)(2.0/3.0) - h)*6.0f;
  return m1;      
}

// The float parameters are all expected to be in the range 0-1
nscolor
NS_HSL2RGB(float h, float s, float l)
{
  uint8_t r, g, b;
  float m1, m2;
  if (l <= 0.5f) {
    m2 = l*(s+1);
  } else {
    m2 = l + s - l*s;
  }
  m1 = l*2 - m2;
  // We round, not floor, because that's how we handle
  // percentage RGB values.
  r = ClampColor(255 * HSL_HueToRGB(m1, m2, h + 1.0f/3.0f));
  g = ClampColor(255 * HSL_HueToRGB(m1, m2, h));
  b = ClampColor(255 * HSL_HueToRGB(m1, m2, h - 1.0f/3.0f));
  return NS_RGB(r, g, b);  
}

const char*
NS_RGBToColorName(nscolor aColor)
{
  for (size_t idx = 0; idx < ArrayLength(kColors); ++idx) {
    if (kColors[idx] == aColor) {
      return kColorNames[idx];
    }
  }

  return nullptr;
}

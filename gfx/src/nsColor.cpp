/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"  // for ArrayLength
#include "mozilla/mozalloc.h"    // for operator delete, etc
#include "mozilla/MathAlgorithms.h"

#include "nsColor.h"
#include <sys/types.h>  // for int32_t
#include "nsDebug.h"    // for NS_ASSERTION, etc
#include "nsStaticNameTable.h"
#include "nsString.h"  // for nsAutoCString, nsString, etc
#include "nscore.h"    // for nsAString, etc
#include "prtypes.h"   // for PR_BEGIN_MACRO, etc

using namespace mozilla;

static int ComponentValue(const char16_t* aColorSpec, int aLen, int color,
                          int dpc) {
  int component = 0;
  int index = (color * dpc);
  if (2 < dpc) {
    dpc = 2;
  }
  while (--dpc >= 0) {
    char16_t ch = ((index < aLen) ? aColorSpec[index++] : '0');
    if (('0' <= ch) && (ch <= '9')) {
      component = (component * 16) + (ch - '0');
    } else if ((('a' <= ch) && (ch <= 'f')) || (('A' <= ch) && (ch <= 'F'))) {
      // "ch&7" handles lower and uppercase hex alphabetics
      component = (component * 16) + (ch & 7) + 9;
    } else {  // not a hex digit, treat it like 0
      component = (component * 16);
    }
  }
  return component;
}

bool NS_HexToRGBA(const nsAString& aColorSpec, nsHexColorType aType,
                  nscolor* aResult) {
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
    if (((ch >= '0') && (ch <= '9')) || ((ch >= 'a') && (ch <= 'f')) ||
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
bool NS_LooseHexToRGB(const nsString& aColorSpec, nscolor* aResult) {
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
      if (('1' <= ch && ch <= '9') || ('A' <= ch && ch <= 'F') ||
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

// Fast approximate division by 255. It has the property that
// for all 0 <= n <= 255*255, FAST_DIVIDE_BY_255(n) == n/255.
// But it only uses two adds and two shifts instead of an
// integer division (which is expensive on many processors).
//
// equivalent to target=v/255
#define FAST_DIVIDE_BY_255(target, v)        \
  PR_BEGIN_MACRO                             \
  unsigned tmp_ = v;                         \
  target = ((tmp_ << 8) + tmp_ + 255) >> 16; \
  PR_END_MACRO

// Macro to blend two colors
//
// equivalent to target = (bg*(255-fgalpha) + fg*fgalpha)/255
#define MOZ_BLEND(target, bg, fg, fgalpha) \
  FAST_DIVIDE_BY_255(target, (bg) * (255 - fgalpha) + (fg) * (fgalpha))

nscolor NS_ComposeColors(nscolor aBG, nscolor aFG) {
  // This function uses colors that are non premultiplied alpha.
  int r, g, b, a;

  int bgAlpha = NS_GET_A(aBG);
  int fgAlpha = NS_GET_A(aFG);

  // Compute the final alpha of the blended color
  // a = fgAlpha + bgAlpha*(255 - fgAlpha)/255;
  FAST_DIVIDE_BY_255(a, bgAlpha * (255 - fgAlpha));
  a = fgAlpha + a;
  int blendAlpha;
  if (a == 0) {
    // In this case the blended color is totally trasparent,
    // we preserve the color information of the foreground color.
    blendAlpha = 255;
  } else {
    blendAlpha = (fgAlpha * 255) / a;
  }
  MOZ_BLEND(r, NS_GET_R(aBG), NS_GET_R(aFG), blendAlpha);
  MOZ_BLEND(g, NS_GET_G(aBG), NS_GET_G(aFG), blendAlpha);
  MOZ_BLEND(b, NS_GET_B(aBG), NS_GET_B(aFG), blendAlpha);

  return NS_RGBA(r, g, b, a);
}

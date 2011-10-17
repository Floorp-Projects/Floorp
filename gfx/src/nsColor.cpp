/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozilla/Util.h"

#include "plstr.h"
#include "nsColor.h"
#include "nsColorNames.h"
#include "nsString.h"
#include "nscore.h"
#include "nsCoord.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include <math.h>
#include "prprf.h"
#include "nsStaticNameTable.h"

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

static nsStaticCaseInsensitiveNameTable* gColorTable = nsnull;

void nsColorNames::AddRefTable(void) 
{
  NS_ASSERTION(!gColorTable, "pre existing array!");
  if (!gColorTable) {
    gColorTable = new nsStaticCaseInsensitiveNameTable();
    if (gColorTable) {
#ifdef DEBUG
    {
      // let's verify the table...
      for (PRUint32 index = 0; index < eColorName_COUNT; ++index) {
        nsCAutoString temp1(kColorNames[index]);
        nsCAutoString temp2(kColorNames[index]);
        ToLowerCase(temp1);
        NS_ASSERTION(temp1.Equals(temp2), "upper case char in table");
      }
    }
#endif      
      gColorTable->Init(kColorNames, eColorName_COUNT); 
    }
  }
}

void nsColorNames::ReleaseTable(void)
{
  if (gColorTable) {
    delete gColorTable;
    gColorTable = nsnull;
  }
}

static int ComponentValue(const PRUnichar* aColorSpec, int aLen, int color, int dpc)
{
  int component = 0;
  int index = (color * dpc);
  if (2 < dpc) {
    dpc = 2;
  }
  while (--dpc >= 0) {
    PRUnichar ch = ((index < aLen) ? aColorSpec[index++] : '0');
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

NS_GFX_(bool) NS_HexToRGB(const nsString& aColorSpec,
                                       nscolor* aResult)
{
  const PRUnichar* buffer = aColorSpec.get();

  int nameLen = aColorSpec.Length();
  if ((nameLen == 3) || (nameLen == 6)) {
    // Make sure the digits are legal
    for (int i = 0; i < nameLen; i++) {
      PRUnichar ch = buffer[i];
      if (((ch >= '0') && (ch <= '9')) ||
          ((ch >= 'a') && (ch <= 'f')) ||
          ((ch >= 'A') && (ch <= 'F'))) {
        // Legal character
        continue;
      }
      // Whoops. Illegal character.
      return PR_FALSE;
    }

    // Convert the ascii to binary
    int dpc = ((3 == nameLen) ? 1 : 2);
    // Translate components from hex to binary
    int r = ComponentValue(buffer, nameLen, 0, dpc);
    int g = ComponentValue(buffer, nameLen, 1, dpc);
    int b = ComponentValue(buffer, nameLen, 2, dpc);
    if (dpc == 1) {
      // Scale single digit component to an 8 bit value. Replicate the
      // single digit to compute the new value.
      r = (r << 4) | r;
      g = (g << 4) | g;
      b = (b << 4) | b;
    }
    NS_ASSERTION((r >= 0) && (r <= 255), "bad r");
    NS_ASSERTION((g >= 0) && (g <= 255), "bad g");
    NS_ASSERTION((b >= 0) && (b <= 255), "bad b");
    *aResult = NS_RGB(r, g, b);
    return PR_TRUE;
  }

  // Improperly formatted color value
  return PR_FALSE;
}

// This implements part of the algorithm for legacy behavior described in
// http://www.whatwg.org/specs/web-apps/current-work/complete/common-microsyntaxes.html#rules-for-parsing-a-legacy-color-value
NS_GFX_(bool) NS_LooseHexToRGB(const nsString& aColorSpec, nscolor* aResult)
{
  if (aColorSpec.EqualsLiteral("transparent")) {
    return PR_FALSE;
  }

  int nameLen = aColorSpec.Length();
  const PRUnichar* colorSpec = aColorSpec.get();
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
      NS_ABORT_IF_FALSE(c * dpc < nameLen,
                        "should not pass end of string while newdpc > 2");
      PRUnichar ch = colorSpec[c * dpc];
      if (('1' <= ch && ch <= '9') ||
          ('A' <= ch && ch <= 'F') ||
          ('a' <= ch && ch <= 'f')) {
        haveNonzero = PR_TRUE;
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
  return PR_TRUE;
}

NS_GFX_(bool) NS_ColorNameToRGB(const nsAString& aColorName, nscolor* aResult)
{
  if (!gColorTable) return PR_FALSE;

  PRInt32 id = gColorTable->Lookup(aColorName);
  if (eColorName_UNKNOWN < id) {
    NS_ASSERTION(PRUint32(id) < eColorName_COUNT,
                 "gColorTable->Lookup messed up");
    if (aResult) {
      *aResult = kColors[id];
    }
    return PR_TRUE;
  }
  return PR_FALSE;
}

// Macro to blend two colors
//
// equivalent to target = (bg*(255-fgalpha) + fg*fgalpha)/255
#define MOZ_BLEND(target, bg, fg, fgalpha)       \
  FAST_DIVIDE_BY_255(target, (bg)*(255-fgalpha) + (fg)*(fgalpha))

NS_GFX_(nscolor)
NS_ComposeColors(nscolor aBG, nscolor aFG)
{
  // This function uses colors that are non premultiplied alpha.
  PRIntn r, g, b, a;

  PRIntn bgAlpha = NS_GET_A(aBG);
  PRIntn fgAlpha = NS_GET_A(aFG);

  // Compute the final alpha of the blended color
  // a = fgAlpha + bgAlpha*(255 - fgAlpha)/255;
  FAST_DIVIDE_BY_255(a, bgAlpha*(255-fgAlpha));
  a = fgAlpha + a;
  PRIntn blendAlpha;
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
NS_GFX_(nscolor)
NS_HSL2RGB(float h, float s, float l)
{
  PRUint8 r, g, b;
  float m1, m2;
  if (l <= 0.5f) {
    m2 = l*(s+1);
  } else {
    m2 = l + s - l*s;
  }
  m1 = l*2 - m2;
  r = PRUint8(255 * HSL_HueToRGB(m1, m2, h + 1.0f/3.0f));
  g = PRUint8(255 * HSL_HueToRGB(m1, m2, h));
  b = PRUint8(255 * HSL_HueToRGB(m1, m2, h - 1.0f/3.0f));
  return NS_RGB(r, g, b);  
}

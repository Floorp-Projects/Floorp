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

#include "plstr.h"
#include "nsColor.h"
#include "nsColorNames.h"
#include "nsString.h"
#include "nscore.h"
#include "nsCoord.h"
#include "nsUnitConversion.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIScreen.h"
#include "nsIScreenManager.h"
#include <math.h>
#include "prprf.h"

static int ComponentValue(const char* aColorSpec, int aLen, int color, int dpc)
{
  int component = 0;
  int index = (color * dpc);
  if (2 < dpc) {
    dpc = 2;
  }
  while (--dpc >= 0) {
    char ch = ((index < aLen) ? aColorSpec[index++] : '0');
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

extern "C" NS_GFX_(PRBool) NS_HexToRGB(const nsString& aColorSpec,
                                       nscolor* aResult)
{
  // XXXldb nsStackString<10>
  NS_LossyConvertUCS2toASCII bufferStr(aColorSpec);
  return NS_ASCIIHexToRGB(bufferStr, aResult);
}

extern "C" NS_GFX_(PRBool) NS_ASCIIHexToRGB(const nsCString& aColorSpec,
                                            nscolor* aResult)
{
  const char* buffer = aColorSpec.get();

  int nameLen = aColorSpec.Length();
  if ((nameLen == 3) || (nameLen == 6)) {
    // Make sure the digits are legal
    for (int i = 0; i < nameLen; i++) {
      char ch = buffer[i];
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
    if (nsnull != aResult) {
      *aResult = NS_RGB(r, g, b);
    }
    return PR_TRUE;
  }

  // Improperly formatted color value
  return PR_FALSE;
}

// compatible with legacy Nav behavior
extern "C" NS_GFX_(PRBool) NS_LooseHexToRGB(const nsString& aColorSpec, nscolor* aResult)
{
  // XXXldb nsStackString<30>
  NS_LossyConvertUCS2toASCII buffer(aColorSpec);

  int nameLen = buffer.Length();
  const char* colorSpec = buffer.get();
  if ('#' == colorSpec[0]) {
    ++colorSpec;
    --nameLen;
  }

  if (3 < nameLen) {
    // Convert the ascii to binary
    int dpc = (nameLen / 3) + (((nameLen % 3) != 0) ? 1 : 0);
    if (4 < dpc) {
      dpc = 4;
    }

    // Translate components from hex to binary
    int r = ComponentValue(colorSpec, nameLen, 0, dpc);
    int g = ComponentValue(colorSpec, nameLen, 1, dpc);
    int b = ComponentValue(colorSpec, nameLen, 2, dpc);
    NS_ASSERTION((r >= 0) && (r <= 255), "bad r");
    NS_ASSERTION((g >= 0) && (g <= 255), "bad g");
    NS_ASSERTION((b >= 0) && (b <= 255), "bad b");
    if (nsnull != aResult) {
      *aResult = NS_RGB(r, g, b);
    }
  }
  else {
    if (nsnull != aResult) {
      *aResult = NS_RGB(0, 0, 0);
    }
  }
  return PR_TRUE;
}

extern "C" NS_GFX_(void) NS_RGBToHex(nscolor aColor, nsAString& aResult)
{
  char buf[10];
  PR_snprintf(buf, sizeof(buf), "#%02x%02x%02x",
              NS_GET_R(aColor), NS_GET_G(aColor), NS_GET_B(aColor));
  CopyASCIItoUTF16(buf, aResult);
}

extern "C" NS_GFX_(void) NS_RGBToASCIIHex(nscolor aColor,
                                          nsAFlatCString& aResult)
{
  aResult.SetLength(7);
  char *buf = aResult.BeginWriting();
  PR_snprintf(buf, 8, "#%02x%02x%02x",
              NS_GET_R(aColor), NS_GET_G(aColor), NS_GET_B(aColor));
}

extern "C" NS_GFX_(PRBool) NS_ColorNameToRGB(const nsAString& aColorName, nscolor* aResult)
{
  nsColorName id = nsColorNames::LookupName(aColorName);
  if (eColorName_UNKNOWN < id) {
    NS_ASSERTION(id < eColorName_COUNT, "LookupName mess up");
    if (nsnull != aResult) {
      *aResult = nsColorNames::kColors[id];
    }
    return PR_TRUE;
  }
  return PR_FALSE;
}

extern "C" NS_GFX_(nscolor) NS_BrightenColor(nscolor inColor)
{
  PRIntn r, g, b, max, over;

  r = NS_GET_R(inColor);
  g = NS_GET_G(inColor);
  b = NS_GET_B(inColor);

  //10% of max color increase across the board
  r += 25;
  g += 25;
  b += 25;

  //figure out which color is largest
  if (r > g)
  {
    if (b > r)
      max = b;
    else
      max = r;
  }
  else
  {
    if (b > g)
      max = b;
    else
      max = g;
  }

  //if we overflowed on this max color, increase
  //other components by the overflow amount
  if (max > 255)
  {
    over = max - 255;

    if (max == r)
    {
      g += over;
      b += over;
    }
    else if (max == g)
    {
      r += over;
      b += over;
    }
    else
    {
      r += over;
      g += over;
    }
  }

  //clamp
  if (r > 255)
    r = 255;
  if (g > 255)
    g = 255;
  if (b > 255)
    b = 255;

  return NS_RGBA(r, g, b, NS_GET_A(inColor));
}

extern "C" NS_GFX_(nscolor) NS_DarkenColor(nscolor inColor)
{
  PRIntn r, g, b, max;

  r = NS_GET_R(inColor);
  g = NS_GET_G(inColor);
  b = NS_GET_B(inColor);

  //10% of max color decrease across the board
  r -= 25;
  g -= 25;
  b -= 25;

  //figure out which color is largest
  if (r > g)
  {
    if (b > r)
      max = b;
    else
      max = r;
  }
  else
  {
    if (b > g)
      max = b;
    else
      max = g;
  }

  //if we underflowed on this max color, decrease
  //other components by the underflow amount
  if (max < 0)
  {
    if (max == r)
    {
      g += max;
      b += max;
    }
    else if (max == g)
    {
      r += max;
      b += max;
    }
    else
    {
      r += max;
      g += max;
    }
  }

  //clamp
  if (r < 0)
    r = 0;
  if (g < 0)
    g = 0;
  if (b < 0)
    b = 0;

  return NS_RGBA(r, g, b, NS_GET_A(inColor));
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
extern "C" NS_GFX_(nscolor)
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

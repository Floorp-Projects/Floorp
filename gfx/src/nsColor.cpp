/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "plstr.h"
#include "nsColor.h"
#include "nsColorNames.h"
#include "nsString.h"

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

extern "C" NS_GFX_(PRBool) NS_HexToRGB(const nsString& aColorSpec, nscolor* aResult)
{
  // XXXldb nsStackString<10>
  NS_LossyConvertUCS2toASCII bufferStr(aColorSpec);
  const char* buffer = bufferStr.get();

  int nameLen = bufferStr.Length();
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

// Weird color computing code stolen from winfe which was stolen
// from the xfe which was written originally by Eric Bina. So there.

#define RED_LUMINOSITY        30
#define GREEN_LUMINOSITY      59
#define BLUE_LUMINOSITY       11
#define INTENSITY_FACTOR      25
#define LIGHT_FACTOR          0
#define LUMINOSITY_FACTOR     75

#define MAX_COLOR             255
#define COLOR_DARK_THRESHOLD  51
#define COLOR_LIGHT_THRESHOLD 204

#define COLOR_LITE_BS_FACTOR 45
#define COLOR_LITE_TS_FACTOR 70

#define COLOR_DARK_BS_FACTOR 30
#define COLOR_DARK_TS_FACTOR 50

#define LIGHT_GRAY NS_RGB(192, 192, 192)
#define DARK_GRAY  NS_RGB(128, 128, 128)
#define WHITE      NS_RGB(255, 255, 255)
#define BLACK      NS_RGB(0, 0, 0)

#define MAX_BRIGHTNESS  254
#define MAX_DARKNESS     0
 
extern "C" NS_GFX_(void) NS_Get3DColors(nscolor aResult[2], nscolor aColor)
{
  int rb = NS_GET_R(aColor);
  int gb = NS_GET_G(aColor);
  int bb = NS_GET_B(aColor);
  int intensity = (rb + gb + bb) / 3;
  int luminosity =
    ((RED_LUMINOSITY * rb) / 100) +
    ((GREEN_LUMINOSITY * gb) / 100) +
    ((BLUE_LUMINOSITY * bb) / 100);
  int brightness = ((intensity * INTENSITY_FACTOR) +
                    (luminosity * LUMINOSITY_FACTOR)) / 100;
  int f0, f1;
  if (brightness < COLOR_DARK_THRESHOLD) {
    f0 = COLOR_DARK_BS_FACTOR;
    f1 = COLOR_DARK_TS_FACTOR;
  } else if (brightness > COLOR_LIGHT_THRESHOLD) {
    f0 = COLOR_LITE_BS_FACTOR;
    f1 = COLOR_LITE_TS_FACTOR;
  } else {
    f0 = COLOR_DARK_BS_FACTOR +
      (brightness *
       (COLOR_LITE_BS_FACTOR - COLOR_DARK_BS_FACTOR) / MAX_COLOR);
    f1 = COLOR_DARK_TS_FACTOR +
      (brightness *
       (COLOR_LITE_TS_FACTOR - COLOR_DARK_TS_FACTOR) / MAX_COLOR);
  }

  int r = rb - (f0 * rb / 100);
  int g = gb - (f0 * gb / 100);
  int b = bb - (f0 * bb / 100);
  aResult[0] = NS_RGB(r, g, b);
  if ((r == rb) && (g == gb) && (b == bb)) {
    aResult[0] = (aColor == LIGHT_GRAY) ? WHITE : LIGHT_GRAY;
  }

  r = rb + (f1 * (MAX_COLOR - rb) / 100);
  if (r > 255) r = 255;
  g = gb + (f1 * (MAX_COLOR - gb) / 100);
  if (g > 255) g = 255;
  b = bb + (f1 * (MAX_COLOR - bb) / 100);
  if (b > 255) b = 255;
  aResult[1] = NS_RGB(r, g, b);
  if ((r == rb) && (g == gb) && (b == bb)) {
    aResult[1] = (aColor == DARK_GRAY) ? BLACK : DARK_GRAY;
  }
}

extern "C" NS_GFX_(void) NS_GetSpecial3DColors(nscolor aResult[2],
											   nscolor aBackgroundColor,
											   nscolor aBorderColor)
{

  PRUint8 f0, f1;
  PRUint8 r, g, b;

  PRUint8 rb = NS_GET_R(aBorderColor);
  PRUint8 gb = NS_GET_G(aBorderColor);
  PRUint8 bb = NS_GET_B(aBorderColor);

  // This needs to be optimized.
  // Calculating background brightness again and again is 
  // a waste of time!!!. Just calculate it only once.
  // .....somehow!!!

  PRUint8 red = NS_GET_R(aBackgroundColor);
  PRUint8 green = NS_GET_G(aBackgroundColor);
  PRUint8 blue = NS_GET_B(aBackgroundColor);
  
  PRUint8 elementBrightness = NS_GetBrightness(rb,gb,bb);
  PRUint8 backgroundBrightness = NS_GetBrightness(red, green, blue);


  if (backgroundBrightness < COLOR_DARK_THRESHOLD) {
    f0 = COLOR_DARK_BS_FACTOR;
    f1 = COLOR_DARK_TS_FACTOR;
	if(elementBrightness == MAX_DARKNESS)
	{
       rb = NS_GET_R(DARK_GRAY);
       gb = NS_GET_G(DARK_GRAY);
       bb = NS_GET_B(DARK_GRAY);
	}
  }else if (backgroundBrightness > COLOR_LIGHT_THRESHOLD) {
    f0 = COLOR_LITE_BS_FACTOR;
    f1 = COLOR_LITE_TS_FACTOR;
	if(elementBrightness == MAX_BRIGHTNESS)
	{
       rb = NS_GET_R(LIGHT_GRAY);
       gb = NS_GET_G(LIGHT_GRAY);
       bb = NS_GET_B(LIGHT_GRAY);
	}
  }else {
    f0 = COLOR_DARK_BS_FACTOR +
      (backgroundBrightness *
       (COLOR_LITE_BS_FACTOR - COLOR_DARK_BS_FACTOR) / MAX_COLOR);
    f1 = COLOR_DARK_TS_FACTOR +
      (backgroundBrightness *
       (COLOR_LITE_TS_FACTOR - COLOR_DARK_TS_FACTOR) / MAX_COLOR);
  }
  
  
  r = rb - (f0 * rb / 100);
  g = gb - (f0 * gb / 100);
  b = bb - (f0 * bb / 100);
  aResult[0] = NS_RGB(r, g, b);

  r = rb + (f1 * (MAX_COLOR - rb) / 100);
  if (r > 255) r = 255;
  g = gb + (f1 * (MAX_COLOR - gb) / 100);
  if (g > 255) g = 255;
  b = bb + (f1 * (MAX_COLOR - bb) / 100);
  if (b > 255) b = 255;
  aResult[1] = NS_RGB(r, g, b);
 
}

extern "C" NS_GFX_(int) NS_GetBrightness(PRUint8 aRed, PRUint8 aGreen, PRUint8 aBlue)
{

  PRUint8 intensity = (aRed + aGreen + aBlue) / 3;

  PRUint8 luminosity =
    ((RED_LUMINOSITY * aRed) / 100) +
    ((GREEN_LUMINOSITY * aGreen) / 100) +
    ((BLUE_LUMINOSITY * aBlue) / 100);
 
  return ((intensity * INTENSITY_FACTOR) +
          (luminosity * LUMINOSITY_FACTOR)) / 100;
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

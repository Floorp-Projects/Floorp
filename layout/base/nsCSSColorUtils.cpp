/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* functions that manipulate colors */

#include "nsCSSColorUtils.h"
#include "nsDebug.h"
#include <math.h>

// Weird color computing code stolen from winfe which was stolen
// from the xfe which was written originally by Eric Bina. So there.

#define RED_LUMINOSITY        299
#define GREEN_LUMINOSITY      587
#define BLUE_LUMINOSITY       114
#define INTENSITY_FACTOR      25
#define LUMINOSITY_FACTOR     75

#define MAX_COLOR             255
#define COLOR_DARK_THRESHOLD  51
#define COLOR_LIGHT_THRESHOLD 204

#define COLOR_LITE_BS_FACTOR 45
#define COLOR_LITE_TS_FACTOR 70

#define COLOR_DARK_BS_FACTOR 30
#define COLOR_DARK_TS_FACTOR 50

#define LIGHT_GRAY NS_RGB(192, 192, 192)
#define DARK_GRAY  NS_RGB(96, 96, 96)

#define MAX_BRIGHTNESS  254
#define MAX_DARKNESS     0

void NS_GetSpecial3DColors(nscolor aResult[2],
                           nscolor aBackgroundColor,
                           nscolor aBorderColor)
{

  uint8_t f0, f1;
  uint8_t r, g, b;

  uint8_t rb = NS_GET_R(aBorderColor);
  uint8_t gb = NS_GET_G(aBorderColor);
  uint8_t bb = NS_GET_B(aBorderColor);

  uint8_t a = NS_GET_A(aBorderColor);

  // This needs to be optimized.
  // Calculating background brightness again and again is
  // a waste of time!!!. Just calculate it only once.
  // .....somehow!!!

  uint8_t red = NS_GET_R(aBackgroundColor);
  uint8_t green = NS_GET_G(aBackgroundColor);
  uint8_t blue = NS_GET_B(aBackgroundColor);

  uint8_t elementBrightness = NS_GetBrightness(rb,gb,bb);
  uint8_t backgroundBrightness = NS_GetBrightness(red, green, blue);


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
  aResult[0] = NS_RGBA(r, g, b, a);

  r = rb + (f1 * (MAX_COLOR - rb) / 100);
  g = gb + (f1 * (MAX_COLOR - gb) / 100);
  b = bb + (f1 * (MAX_COLOR - bb) / 100);
  aResult[1] = NS_RGBA(r, g, b, a);
}

int NS_GetBrightness(uint8_t aRed, uint8_t aGreen, uint8_t aBlue)
{

  uint8_t intensity = (aRed + aGreen + aBlue) / 3;

  uint8_t luminosity = NS_GetLuminosity(NS_RGB(aRed, aGreen, aBlue)) / 1000;

  return ((intensity * INTENSITY_FACTOR) +
          (luminosity * LUMINOSITY_FACTOR)) / 100;
}

int32_t NS_GetLuminosity(nscolor aColor)
{
  // When aColor is not opaque, the perceived luminosity will depend
  // on what color(s) aColor is ultimately drawn on top of, which we
  // do not know.
  NS_ASSERTION(NS_GET_A(aColor) == 255,
               "impossible to compute luminosity of a non-opaque color");

  return (NS_GET_R(aColor) * RED_LUMINOSITY +
          NS_GET_G(aColor) * GREEN_LUMINOSITY +
          NS_GET_B(aColor) * BLUE_LUMINOSITY);
}

// Function to convert RGB color space into the HSV colorspace
// Hue is the primary color defined from 0 to 359 degrees
// Saturation is defined from 0 to 255.  The higher the number.. the deeper
// the color Value is the brightness of the color. 0 is black, 255 is white.
void NS_RGB2HSV(nscolor aColor, uint16_t &aHue, uint16_t &aSat,
                uint16_t &aValue, uint8_t &aAlpha)
{
  uint8_t r, g, b;
  int16_t delta, min, max, r1, b1, g1;
  float   hue;

  r = NS_GET_R(aColor);
  g = NS_GET_G(aColor);
  b = NS_GET_B(aColor);

  if (r>g) {
    max = r;
    min = g;
  } else {
    max = g;
    min = r;
  }

  if (b>max) {
    max = b;
  }
  if (b<min) {
    min = b;
  }

  // value or brightness will always be the max of all the colors(RGB)
  aValue = max;
  delta = max-min;
  aSat = (max!=0)?((delta*255)/max):0;
  r1 = r;
  b1 = b;
  g1 = g;

  if (aSat==0) {
    hue = 1000;
  } else {
    if(r==max){
      hue=(float)(g1-b1)/(float)delta;
    } else if (g1==max) {
      hue= 2.0f+(float)(b1-r1)/(float)delta;
    } else {
      hue = 4.0f+(float)(r1-g1)/(float)delta;
    }
  }

  if(hue<999) {
    hue*=60;
    if(hue<0){
      hue+=360;
    }
  } else {
    hue=0;
  }

  aHue = (uint16_t)hue;

  aAlpha = NS_GET_A(aColor);
}

// Function to convert HSV color space into the RGB colorspace
// Hue is the primary color defined from 0 to 359 degrees
// Saturation is defined from 0 to 255.  The higher the number.. the deeper
// the color Value is the brightness of the color. 0 is black, 255 is white.
void NS_HSV2RGB(nscolor &aColor, uint16_t aHue, uint16_t aSat, uint16_t aValue,
                uint8_t aAlpha)
{
  uint16_t  r = 0, g = 0, b = 0;
  uint16_t  i, p, q, t;
  double    h, f, percent;

  if ( aSat == 0 ){
    // achromatic color, no hue is defined
    r = aValue;
    g = aValue;
    b = aValue;
  } else {
    // hue in in degrees around the color wheel defined from
    // 0 to 360 degrees.
    if (aHue >= 360) {
      aHue = 0;
    }

    // we break the color wheel into 6 areas.. these
    // areas define how the saturation and value define the color.
    // reds behave differently than the blues
    h = (double)aHue / 60.0;
    i = (uint16_t) floor(h);
    f = h-(double)i;
    percent = ((double)aValue/255.0);   // this needs to be a value from 0 to 1, so a percentage
                                        // can be calculated of the saturation.
    p = (uint16_t)(percent*(255-aSat));
    q = (uint16_t)(percent*(255-(aSat*f)));
    t = (uint16_t)(percent*(255-(aSat*(1.0-f))));

    // i is guaranteed to never be larger than 5.
    switch(i){
      case 0: r = aValue; g = t; b = p;break;
      case 1: r = q; g = aValue; b = p;break;
      case 2: r = p; g = aValue; b = t;break;
      case 3: r = p; g = q; b = aValue;break;
      case 4: r = t; g = p; b = aValue;break;
      case 5: r = aValue; g = p; b = q;break;
    }
  }
  aColor = NS_RGBA(r, g, b, aAlpha);
}

#undef RED_LUMINOSITY
#undef GREEN_LUMINOSITY
#undef BLUE_LUMINOSITY
#undef INTENSITY_FACTOR
#undef LUMINOSITY_FACTOR

#undef MAX_COLOR
#undef COLOR_DARK_THRESHOLD
#undef COLOR_LIGHT_THRESHOLD

#undef COLOR_LITE_BS_FACTOR
#undef COLOR_LITE_TS_FACTOR

#undef COLOR_DARK_BS_FACTOR
#undef COLOR_DARK_TS_FACTOR

#undef LIGHT_GRAY
#undef DARK_GRAY

#undef MAX_BRIGHTNESS
#undef MAX_DARKNESS

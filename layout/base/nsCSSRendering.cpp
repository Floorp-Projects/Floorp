/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsCSSRendering.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIImage.h"
#include "nsIFrame.h"
#include "nsPoint.h"
#include "nsRect.h"
#include "nsIViewManager.h"
#include "nsIPresShell.h"
#include "nsIFrameImageLoader.h"
#include "nsGlobalVariables.h"

#define BORDER_FULL    0        //entire side
#define BORDER_INSIDE  1        //inside half
#define BORDER_OUTSIDE 2        //outside half

//thickness of dashed line relative to dotted line
#define DOT_LENGTH  1           //square
#define DASH_LENGTH 3           //3 times longer than dot



// Weird color computing code stolen from winfe which was stolen
// from the xfe which was written originally by Eric Bina. So there.

const int nsCSSRendering::RED_LUMINOSITY = 30;
const int nsCSSRendering::GREEN_LUMINOSITY = 59;
const int nsCSSRendering::BLUE_LUMINOSITY = 11;
const int nsCSSRendering::INTENSITY_FACTOR = 25;
const int nsCSSRendering::LIGHT_FACTOR = 0;
const int nsCSSRendering::LUMINOSITY_FACTOR = 75;
const int nsCSSRendering::MAX_COLOR = 255;
const int nsCSSRendering::COLOR_DARK_THRESHOLD = 51;
const int nsCSSRendering::COLOR_LIGHT_THRESHOLD = 204;
 

void nsCSSRendering::Get3DColors(nscolor aResult[2], nscolor aColor)
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
    f0 = 30;
    f1 = 50;
  } else if (brightness > COLOR_LIGHT_THRESHOLD) {
    f0 = 45;
    f1 = 50;
  } else {
    f0 = 30 + (brightness * (45 - 30) / MAX_COLOR);
    f1 = f0;
  }
  int r = rb - (f0 * rb / 100);
  int g = gb - (f0 * gb / 100);
  int b = bb - (f0 * bb / 100);
  aResult[0] = NS_RGB(r, g, b);
  r = rb + (f1 * (MAX_COLOR - rb) / 100);
  if (r > 255) r = 255;
  g = gb + (f1 * (MAX_COLOR - gb) / 100);
  if (g > 255) g = 255;
  b = bb + (f1 * (MAX_COLOR - bb) / 100);
  if (b > 255) b = 255;
  aResult[1] = NS_RGB(r, g, b);
}


/**
 * Special method to brighten a Color and have it shift to white when
 * fully saturated.
 */
nscolor nsCSSRendering::Brighten(nscolor inColor)
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

/**
 * Special method to darken a Color and have it shift to black when
 * darkest component underflows
 */
nscolor nsCSSRendering::Darken(nscolor inColor)
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

/**
 * Make a bevel color
 */
nscolor nsCSSRendering::MakeBevelColor(PRIntn whichSide, PRUint8 style,
                                       nscolor baseColor,
                                       PRBool printing)
{

  PRBool blackLines = nsGlobalVariables::Instance()->GetBlackLines();
  nscolor colors[2];
  nscolor theColor;

  // Get the background color that applies to this HR
  if (printing && blackLines)
  {
    colors[0] = NS_RGB(0,0,0);
    colors[1] = colors[0];
  }
  else
  {
    // Given a background color and a border color
    // calculate the color used for the shading
    Get3DColors(colors, baseColor);
  }
 

  if ((style == NS_STYLE_BORDER_STYLE_OUTSET) ||
      (style == NS_STYLE_BORDER_STYLE_RIDGE)) {
    // Flip colors for these two border style
    switch (whichSide) {
    case NS_SIDE_BOTTOM: whichSide = NS_SIDE_TOP;    break;
    case NS_SIDE_RIGHT:  whichSide = NS_SIDE_LEFT;   break;
    case NS_SIDE_TOP:    whichSide = NS_SIDE_BOTTOM; break;
    case NS_SIDE_LEFT:   whichSide = NS_SIDE_RIGHT;  break;
    }
  }

  switch (whichSide) {
  case NS_SIDE_BOTTOM:
    theColor = colors[1];
    break;
  case NS_SIDE_RIGHT:
    theColor = colors[1];
    break;
  case NS_SIDE_TOP:
    theColor = colors[0];
    break;
  case NS_SIDE_LEFT:
    theColor = colors[0];
    break;
  }
  return theColor;
}

// Maximum poly points in any of the polygons we generate below
#define MAX_POLY_POINTS 4

// a nifty helper function to create a polygon representing a
// particular side of a border. This helps localize code for figuring
// mitered edges. It is mainly used by the solid, inset, and outset
// styles.
//
// If the side can be represented as a line segment (because the thickness
// is one pixel), then a line with two endpoints is returned
PRIntn nsCSSRendering::MakeSide(nsPoint aPoints[],
                                nsIRenderingContext& aContext,
                                PRIntn whichSide,
                                const nsRect& outside, const nsRect& inside,
                                PRIntn borderPart, float borderFrac,
                                nscoord twipsPerPixel)
{
  float borderRest = 1.0f - borderFrac;

  // XXX QQQ We really should decide to do a bevel based on whether there
  // is a side adjacent or not. This could let you join borders across
  // block elements (paragraphs).

  PRIntn np = 0;
  nscoord thickness;

  // Base our thickness check on the segment being less than a pixel and 1/2
  twipsPerPixel += twipsPerPixel >> 2;

  switch (whichSide) {
  case NS_SIDE_TOP:
    if (borderPart == BORDER_FULL) {
      thickness = inside.y - outside.y;

      aPoints[np++].MoveTo(outside.XMost(), outside.y);
      if (thickness >= twipsPerPixel) {
        aPoints[np++].MoveTo(inside.XMost(), inside.y);
        aPoints[np++].MoveTo(inside.x, inside.y);
      }
      aPoints[np++].MoveTo(outside.x, outside.y);
    } else if (borderPart == BORDER_INSIDE) {
      aPoints[np++].MoveTo(nscoord(outside.x * borderFrac +
                                   inside.x * borderRest),
                           nscoord(outside.y * borderFrac +
                                   inside.y * borderRest));
      aPoints[np++].MoveTo(nscoord(outside.XMost() * borderFrac +
                                   inside.XMost() * borderRest),
                           nscoord(outside.y * borderFrac +
                                   inside.y * borderRest));
      aPoints[np++].MoveTo(inside.XMost(), inside.y);
      aPoints[np++].MoveTo(inside.x, inside.y);
    } else {
      aPoints[np++].MoveTo(outside.x, outside.y);
      aPoints[np++].MoveTo(outside.XMost(), outside.y);
      aPoints[np++].MoveTo(nscoord(inside.XMost() * borderFrac +
                                   outside.XMost() * borderRest),
                           nscoord(inside.y * borderFrac +
                                   outside.y * borderRest));
      aPoints[np++].MoveTo(nscoord(inside.x * borderFrac +
                                   outside.x * borderRest),
                           nscoord(inside.y * borderFrac +
                                   outside.y * borderRest));
    }
    break;

  case NS_SIDE_LEFT:
    if (borderPart == BORDER_FULL) {
      thickness = inside.x - outside.x;

      aPoints[np++].MoveTo(outside.x, outside.y);
      if (thickness >= twipsPerPixel) {
        aPoints[np++].MoveTo(inside.x, inside.y);
        aPoints[np++].MoveTo(inside.x, inside.YMost());
      }
      aPoints[np++].MoveTo(outside.x, outside.YMost());
    } else if (borderPart == BORDER_INSIDE) {
      aPoints[np++].MoveTo(nscoord(outside.x * borderFrac +
                                   inside.x * borderRest),
                           nscoord(outside.y * borderFrac +
                                   inside.y * borderRest));
      aPoints[np++].MoveTo(inside.x,  inside.y);
      aPoints[np++].MoveTo(inside.x,  inside.YMost());
      aPoints[np++].MoveTo(nscoord(outside.x * borderFrac +
                                   inside.x * borderRest),
                           nscoord(outside.YMost() * borderFrac +
                                   inside.YMost() * borderRest));
    } else {
      aPoints[np++].MoveTo(outside.x, outside.y);
      aPoints[np++].MoveTo(nscoord(inside.x * borderFrac +
                                   outside.x * borderRest),
                           nscoord(inside.y * borderFrac +
                                   outside.y * borderRest));
      aPoints[np++].MoveTo(nscoord(inside.x * borderFrac +
                                   outside.x * borderRest),
                           nscoord(inside.YMost() * borderFrac +
                                   outside.YMost() * borderRest));
      aPoints[np++].MoveTo(outside.x, outside.YMost());
    }
    break;

  case NS_SIDE_BOTTOM:
    if (borderPart == BORDER_FULL) {
      thickness = outside.YMost() - inside.YMost();

      aPoints[np++].MoveTo(outside.x, outside.YMost());
      if (thickness >= twipsPerPixel) {
        aPoints[np++].MoveTo(inside.x, inside.YMost());
        aPoints[np++].MoveTo(inside.XMost(), inside.YMost());
      }
      aPoints[np++].MoveTo(outside.XMost(), outside.YMost());
    } else if (borderPart == BORDER_INSIDE) {
      aPoints[np++].MoveTo(nscoord(outside.x * borderFrac +
                                   inside.x * borderRest),
                           nscoord(outside.YMost() * borderFrac +
                                   inside.YMost() * borderRest));
      aPoints[np++].MoveTo(inside.x, inside.YMost());
      aPoints[np++].MoveTo(inside.XMost(), inside.YMost());
      aPoints[np++].MoveTo(nscoord(outside.XMost() * borderFrac +
                                   inside.XMost() * borderRest),
                           nscoord(outside.YMost() * borderFrac +
                                   inside.YMost() * borderRest));
    } else {
      aPoints[np++].MoveTo(outside.x, outside.YMost());
      aPoints[np++].MoveTo(nscoord(inside.x * borderFrac +
                                   outside.x * borderRest),
                           nscoord(inside.YMost() * borderFrac +
                                   outside.YMost() * borderRest));
      aPoints[np++].MoveTo(nscoord(inside.XMost() * borderFrac +
                                   outside.XMost() * borderRest),
                           nscoord(inside.YMost() * borderFrac +
                                   outside.YMost() * borderRest));
      aPoints[np++].MoveTo(outside.XMost(), outside.YMost());
    }
    break;

  case NS_SIDE_RIGHT:
    if (borderPart == BORDER_FULL) {
      thickness = outside.XMost() - inside.XMost();

      aPoints[np++].MoveTo(outside.XMost(), outside.YMost());
      aPoints[np++].MoveTo(outside.XMost(), outside.y);
      if (thickness >= twipsPerPixel) {
        aPoints[np++].MoveTo(inside.XMost(), inside.y);
        aPoints[np++].MoveTo(inside.XMost(), inside.YMost());
      }
    } else if (borderPart == BORDER_INSIDE) {
      aPoints[np++].MoveTo(inside.XMost(), inside.y);
      aPoints[np++].MoveTo(nscoord(outside.XMost() * borderFrac +
                                   inside.XMost() * borderRest),
                           nscoord(outside.y * borderFrac +
                                   inside.y * borderRest));
      aPoints[np++].MoveTo(nscoord(outside.XMost() * borderFrac +
                                   inside.XMost() * borderRest),
                           nscoord(outside.YMost() * borderFrac +
                                   inside.YMost() * borderRest));
      aPoints[np++].MoveTo(inside.XMost(),  inside.YMost());
    } else {
      aPoints[np++].MoveTo(nscoord(inside.XMost() * borderFrac +
                                   outside.XMost() * borderRest),
                           nscoord(inside.y * borderFrac +
                                   outside.y * borderRest));
      aPoints[np++].MoveTo(outside.XMost(), outside.y);
      aPoints[np++].MoveTo(outside.XMost(), outside.YMost());
      aPoints[np++].MoveTo(nscoord(inside.XMost() * borderFrac +
                                   outside.XMost() * borderRest),
                           nscoord(inside.YMost() * borderFrac +
                                   outside.YMost() * borderRest));
    }
    break;
  }
  return np;
}

void nsCSSRendering::DrawSide(nsIRenderingContext& aContext,
                              PRIntn whichSide,
                              const PRUint8 borderStyles[],
                              const nscolor borderColors[],
                              const nsRect& borderOutside,
                              const nsRect& borderInside,
                              PRBool printing,
                              nscoord twipsPerPixel)
{
  nsPoint theSide[MAX_POLY_POINTS];
  nscolor theColor = borderColors[whichSide];
  PRUint8 theStyle = borderStyles[whichSide];
  PRInt32 np;
  switch (theStyle) {
  case NS_STYLE_BORDER_STYLE_NONE:
  case NS_STYLE_BORDER_STYLE_BLANK:
    return;

  case NS_STYLE_BORDER_STYLE_DOTTED:    //handled a special case elsewhere
  case NS_STYLE_BORDER_STYLE_DASHED:    //handled a special case elsewhere
    break; // That was easy...

  case NS_STYLE_BORDER_STYLE_GROOVE:
  case NS_STYLE_BORDER_STYLE_RIDGE:
    np = MakeSide (theSide, aContext, whichSide, borderOutside, borderInside,
                   BORDER_INSIDE, 0.5f, twipsPerPixel);
    aContext.SetColor ( MakeBevelColor (whichSide, theStyle, theColor, printing));
    if (2 == np) {
      aContext.DrawLine (theSide[0].x, theSide[0].y, theSide[1].x, theSide[1].y);
    } else {
      aContext.FillPolygon (theSide, np);
    }
    np = MakeSide (theSide, aContext, whichSide, borderOutside, borderInside,
                   BORDER_OUTSIDE, 0.5f, twipsPerPixel);
    aContext.SetColor ( MakeBevelColor (whichSide,
       (theStyle == NS_STYLE_BORDER_STYLE_RIDGE)
          ? NS_STYLE_BORDER_STYLE_GROOVE
          : NS_STYLE_BORDER_STYLE_RIDGE, theColor,printing));
    if (2 == np) {
      aContext.DrawLine (theSide[0].x, theSide[0].y, theSide[1].x, theSide[1].y);
    } else {
      aContext.FillPolygon (theSide, np);
    }
    break;

  case NS_STYLE_BORDER_STYLE_SOLID:
    np = MakeSide (theSide, aContext, whichSide, borderOutside, borderInside,
                   BORDER_FULL, 1.0f, twipsPerPixel);
    aContext.SetColor (borderColors[whichSide]);
    if (2 == np) {
      aContext.DrawLine (theSide[0].x, theSide[0].y, theSide[1].x, theSide[1].y);
    } else {
      aContext.FillPolygon (theSide, np);
    }
    break;

  case NS_STYLE_BORDER_STYLE_DOUBLE:
    np = MakeSide (theSide, aContext, whichSide, borderOutside, borderInside,
                   BORDER_INSIDE, 0.333333f, twipsPerPixel);
    aContext.SetColor (borderColors[whichSide]);
    if (2 == np) {
      aContext.DrawLine (theSide[0].x, theSide[0].y, theSide[1].x, theSide[1].y);
    } else {
      aContext.FillPolygon (theSide, np);
    }
    np = MakeSide (theSide, aContext, whichSide, borderOutside, borderInside,
                   BORDER_OUTSIDE, 0.333333f, twipsPerPixel);
    if (2 == np) {
      aContext.DrawLine (theSide[0].x, theSide[0].y, theSide[1].x, theSide[1].y);
    } else {
      aContext.FillPolygon (theSide, np);
    }
    break;

  case NS_STYLE_BORDER_STYLE_OUTSET:
  case NS_STYLE_BORDER_STYLE_INSET:
    np = MakeSide (theSide, aContext, whichSide, borderOutside, borderInside,
                   BORDER_FULL, 1.0f, twipsPerPixel);
    aContext.SetColor ( MakeBevelColor (whichSide, theStyle, theColor,printing));
    if (2 == np) {
      aContext.DrawLine (theSide[0].x, theSide[0].y, theSide[1].x, theSide[1].y);
    } else {
      aContext.FillPolygon (theSide, np);
    }
    break;
  }
}

/**
 * Draw a dotted/dashed sides of a box
 */
//XXX dashes which span more than two edges are not handled properly MMP
void nsCSSRendering::DrawDashedSides(PRIntn startSide,
                                     nsIRenderingContext& aContext,
                                     const PRUint8 borderStyles[],
                                     const nscolor borderColors[],
                                     const nsRect& borderOutside,
                                     const nsRect& borderInside,
                                     PRIntn aSkipSides)
{
  PRIntn dashLength;
  nsRect dashRect, firstRect, currRect;

  PRBool bSolid = PR_TRUE;
  float over = 0.0f;
  PRUint8 style = borderStyles[startSide];
  PRBool skippedSide = PR_FALSE;
  for (PRIntn whichSide = startSide; whichSide < 4; whichSide++) {
    PRUint8 prevStyle = style;
    style = borderStyles[whichSide];
    if ((1<<whichSide) & aSkipSides) {
      // Skipped side
      skippedSide = PR_TRUE;
      continue;
    }
    if ((style == NS_STYLE_BORDER_STYLE_DASHED) ||
        (style == NS_STYLE_BORDER_STYLE_DOTTED))
    {
      if ((style != prevStyle) || skippedSide) {
        //style discontinuity
        over = 0.0f;
        bSolid = PR_TRUE;
      }

      // XXX units for dash & dot?
      if (style == NS_STYLE_BORDER_STYLE_DASHED) {
        dashLength = DASH_LENGTH;
      } else {
        dashLength = DOT_LENGTH;
      }

      aContext.SetColor(borderColors[whichSide]);
      switch (whichSide) {
      case NS_SIDE_LEFT:
        //XXX need to properly handle wrap around from last edge to first edge
        //(this is the first edge) MMP
        dashRect.width = borderInside.x - borderOutside.x;
        dashRect.height = nscoord(dashRect.width * dashLength);
        dashRect.x = borderOutside.x;
        dashRect.y = borderInside.YMost() - dashRect.height;

        if (over > 0.0f) {
          firstRect.x = dashRect.x;
          firstRect.width = dashRect.width;
          firstRect.height = nscoord(dashRect.height * over);
          firstRect.y = dashRect.y + (dashRect.height - firstRect.height);
          over = 0.0f;
          currRect = firstRect;
        } else {
          currRect = dashRect;
        }

        while (currRect.YMost() > borderInside.y) {
          //clip if necessary
          if (currRect.y < borderInside.y) {
            over = float(borderInside.y - dashRect.y) /
              float(dashRect.height);
            currRect.height = currRect.height - (borderInside.y - currRect.y);
            currRect.y = borderInside.y;
          }

          //draw if necessary
          if (bSolid) {
            aContext.FillRect(currRect);
          }

          //setup for next iteration
          if (over == 0.0f) {
            bSolid = PRBool(!bSolid);
          }
          dashRect.y = dashRect.y - currRect.height;
          currRect = dashRect;
        }
        break;

      case NS_SIDE_TOP:
        //if we are continuing a solid rect, fill in the corner first
        if (bSolid) {
          aContext.FillRect(borderOutside.x, borderOutside.y,
                            borderInside.x - borderOutside.x,
                            borderInside.y - borderOutside.y);
        }

        dashRect.height = borderInside.y - borderOutside.y;
        dashRect.width = dashRect.height * dashLength;
        dashRect.x = borderInside.x;
        dashRect.y = borderOutside.y;

        if (over > 0.0f) {
          firstRect.x = dashRect.x;
          firstRect.y = dashRect.y;
          firstRect.width = nscoord(dashRect.width * over);
          firstRect.height = dashRect.height;
          over = 0.0f;
          currRect = firstRect;
        } else {
          currRect = dashRect;
        }

        while (currRect.x < borderInside.XMost()) {
          //clip if necessary
          if (currRect.XMost() > borderInside.XMost()) {
            over = float(dashRect.XMost() - borderInside.XMost()) /
              float(dashRect.width);
            currRect.width = currRect.width -
              (currRect.XMost() - borderInside.XMost());
          }

          //draw if necessary
          if (bSolid) {
            aContext.FillRect(currRect);
          }

          //setup for next iteration
          if (over == 0.0f) {
            bSolid = PRBool(!bSolid);
          }
          dashRect.x = dashRect.x + currRect.width;
          currRect = dashRect;
        }
        break;

      case NS_SIDE_RIGHT:
        //if we are continuing a solid rect, fill in the corner first
        if (bSolid) {
          aContext.FillRect(borderInside.XMost(), borderOutside.y,
                            borderOutside.XMost() - borderInside.XMost(),
                            borderInside.y - borderOutside.y);
        }

        dashRect.width = borderOutside.XMost() - borderInside.XMost();
        dashRect.height = nscoord(dashRect.width * dashLength);
        dashRect.x = borderInside.XMost();
        dashRect.y = borderInside.y;

        if (over > 0.0f) {
          firstRect.x = dashRect.x;
          firstRect.y = dashRect.y;
          firstRect.width = dashRect.width;
          firstRect.height = nscoord(dashRect.height * over);
          over = 0.0f;
          currRect = firstRect;
        } else {
          currRect = dashRect;
        }

        while (currRect.y < borderInside.YMost()) {
          //clip if necessary
          if (currRect.YMost() > borderInside.YMost()) {
            over = float(dashRect.YMost() - borderInside.YMost()) /
              float(dashRect.height);
            currRect.height = currRect.height -
              (currRect.YMost() - borderInside.YMost());
          }

          //draw if necessary
          if (bSolid) {
            aContext.FillRect(currRect);
          }

          //setup for next iteration
          if (over == 0.0f) {
            bSolid = PRBool(!bSolid);
          }
          dashRect.y = dashRect.y + currRect.height;
          currRect = dashRect;
        }
        break;

      case NS_SIDE_BOTTOM:
        //if we are continuing a solid rect, fill in the corner first
        if (bSolid) {
          aContext.FillRect(borderInside.XMost(), borderInside.YMost(),
                            borderOutside.XMost() - borderInside.XMost(),
                            borderOutside.YMost() - borderInside.YMost());
        }

        dashRect.height = borderOutside.YMost() - borderInside.YMost();
        dashRect.width = nscoord(dashRect.height * dashLength);
        dashRect.x = borderInside.XMost() - dashRect.width;
        dashRect.y = borderInside.YMost();

        if (over > 0.0f) {
          firstRect.y = dashRect.y;
          firstRect.width = nscoord(dashRect.width * over);
          firstRect.height = dashRect.height;
          firstRect.x = dashRect.x + (dashRect.width - firstRect.width);
          over = 0.0f;
          currRect = firstRect;
        } else {
          currRect = dashRect;
        }

        while (currRect.XMost() > borderInside.x) {
          //clip if necessary
          if (currRect.x < borderInside.x) {
            over = float(borderInside.x - dashRect.x) / float(dashRect.width);
            currRect.width = currRect.width - (borderInside.x - currRect.x);
            currRect.x = borderInside.x;
          }

          //draw if necessary
          if (bSolid) {
            aContext.FillRect(currRect);
          }

          //setup for next iteration
          if (over == 0.0f) {
            bSolid = PRBool(!bSolid);
          }
          dashRect.x = dashRect.x - currRect.width;
          currRect = dashRect;
        }
        break;
      }
    }
    skippedSide = PR_FALSE;
  }
}

// XXX improve this to constrain rendering to the damaged area
void nsCSSRendering::PaintBorder(nsIPresContext& aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 nsIFrame* aForFrame,
                                 const nsRect& aDirtyRect,
                                 const nsRect& aBounds,
                                 const nsStyleSpacing& aStyle,
                                 PRIntn aSkipSides)
{
  PRIntn    cnt;
  nsMargin  border;
  PRBool    printing = nsGlobalVariables::Instance()->GetPrinting(&aPresContext);

  aStyle.CalcBorderFor(aForFrame, border);
  if ((0 == border.left) && (0 == border.right) &&
      (0 == border.top) && (0 == border.bottom)) {
    // Empty border area
    return;
  }


  nsRect inside(0, 0, aBounds.width, aBounds.height);
  nsRect outside(inside);
  outside.Deflate(border);

  //see if any sides are dotted or dashed
  for (cnt = 0; cnt < 4; cnt++) {
    if ((aStyle.mBorderStyle[cnt] == NS_STYLE_BORDER_STYLE_DOTTED) ||
        (aStyle.mBorderStyle[cnt] == NS_STYLE_BORDER_STYLE_DASHED)) {
      break;
    }
  }
  if (cnt < 4) {
    // Draw the dashed/dotted lines first
    DrawDashedSides(cnt, aRenderingContext, aStyle.mBorderStyle,
                    aStyle.mBorderColor, inside, outside,
                    aSkipSides);
  }

  // Draw all the other sides
  nscoord twipsPerPixel = (nscoord)aPresContext.GetPixelsToTwips();

  if (0 == (aSkipSides & (1<<NS_SIDE_TOP))) {
    DrawSide(aRenderingContext, NS_SIDE_TOP, aStyle.mBorderStyle,
             aStyle.mBorderColor, inside, outside, printing, twipsPerPixel);
  }
  if (0 == (aSkipSides & (1<<NS_SIDE_LEFT))) {
    DrawSide(aRenderingContext, NS_SIDE_LEFT, aStyle.mBorderStyle, 
             aStyle.mBorderColor, inside, outside, printing, twipsPerPixel);
  }
  if (0 == (aSkipSides & (1<<NS_SIDE_BOTTOM))) {
    DrawSide(aRenderingContext, NS_SIDE_BOTTOM, aStyle.mBorderStyle,
             aStyle.mBorderColor, inside, outside, printing, twipsPerPixel);
  }
  if (0 == (aSkipSides & (1<<NS_SIDE_RIGHT))) {
    DrawSide(aRenderingContext, NS_SIDE_RIGHT, aStyle.mBorderStyle,
             aStyle.mBorderColor, inside, outside, printing, twipsPerPixel);
  }
}

//----------------------------------------------------------------------

// XXX improve this to constrain rendering to the damaged area
void nsCSSRendering::PaintBackground(nsIPresContext& aPresContext,
                                     nsIRenderingContext& aRenderingContext,
                                     nsIFrame* aForFrame,
                                     const nsRect& aDirtyRect,
                                     const nsRect& aBounds,
                                     const nsStyleColor& aColor)
{
  if (0 < aColor.mBackgroundImage.Length()) {
    // Lookup the image
    nsSize imageSize;
    nsIImage* image = nsnull;
    nsIFrameImageLoader* loader = nsnull;
    nsresult rv = aPresContext.LoadImage(aColor.mBackgroundImage,
                                         aForFrame, PR_FALSE, loader);
    if ((NS_OK != rv) || (nsnull == loader) ||
        (loader->GetImage(image), (nsnull == image))) {
      NS_IF_RELEASE(loader);
      // Redraw will happen later
      if (0 == (aColor.mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT)) {
        aRenderingContext.SetColor(aColor.mBackgroundColor);
        aRenderingContext.FillRect(0, 0, aBounds.width, aBounds.height);
      }
      return;
    }
    loader->GetSize(imageSize);
    NS_RELEASE(loader);

#if XXX
    // XXX enable this code as soon as nsIImage can support it
    if (image->NeedsBlend()) {
      if (0 == (aColor.mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT)) {
        aRenderingContext.SetColor(aColor.mBackgroundColor);
        aRenderingContext.FillRect(0, 0, aBounds.width, aBounds.height);
      }
    }
#endif

    // Convert image dimensions into nscoord's
    float p2t = aPresContext.GetPixelsToTwips();
    nscoord tileWidth = nscoord(p2t * imageSize.width);
    nscoord tileHeight = nscoord(p2t * imageSize.height);

    PRIntn repeat = aColor.mBackgroundRepeat;
    PRIntn xcount, ycount;
    switch (aColor.mBackgroundRepeat) {
    case NS_STYLE_BG_REPEAT_OFF:
    default:
      xcount = 0;
      ycount = 0;
      break;
    case NS_STYLE_BG_REPEAT_X:
      xcount = (tileWidth  == 0) ? 0 : (PRIntn) (aBounds.width / tileWidth);
      ycount = 0;
      break;
    case NS_STYLE_BG_REPEAT_Y:
      xcount = 0;
      ycount = (tileHeight == 0) ? 0 : (PRIntn) (aBounds.height / tileHeight);
      break;
    case NS_STYLE_BG_REPEAT_XY:
      xcount = (tileWidth  == 0) ? 0 : (PRIntn) (aBounds.width / tileWidth);
      ycount = (tileHeight == 0) ? 0 : (PRIntn) (aBounds.height / tileHeight);
      break;
    }

    // Tile the background
    nscoord xpos = 0, ypos = 0;
    nscoord xpos0 = 0;
#if XXX
    // XXX support offset positioning
    PRIntn xPos = aColor.mBackgroundXPosition;
    PRIntn yPos = aColor.mBackgroundXPosition;
#endif
    aRenderingContext.PushState();
    aRenderingContext.SetClipRect(aDirtyRect, nsClipCombine_kIntersect);
    PRIntn x, y;
    for (y = 0;  y <= ycount;  ++y, ypos += tileHeight) {
      for (x = 0, xpos = xpos0;  x <= xcount;  ++x, xpos += tileWidth) {
        aRenderingContext.DrawImage(image, xpos, ypos);
      }
    }
    aRenderingContext.PopState();
  } else {
    if (0 == (aColor.mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT)) {
      // XXX This step can be avoided if we have an image and it doesn't
      // have any transparent pixels and the image is tiled in both
      // the x and the y
      aRenderingContext.SetColor(aColor.mBackgroundColor);
      aRenderingContext.FillRect(0, 0, aBounds.width, aBounds.height);
    }
  }
}

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
#include "nsIStyleContext.h"
#include "nsGlobalVariables.h"

#define BORDER_FULL    0        //entire side
#define BORDER_INSIDE  1        //inside half
#define BORDER_OUTSIDE 2        //outside half

//thickness of dashed line relative to dotted line
#define DOT_LENGTH  1           //square
#define DASH_LENGTH 3           //3 times longer than dot


// Draw a line, skipping that portion which crosses aGap. aGap defines a rectangle gap
// This services fieldset legends and only works for coords defining horizontal lines.
void nsCSSRendering::DrawLine (nsIRenderingContext& aContext, 
                               nscoord aX1, nscoord aY1, nscoord aX2, nscoord aY2,
                               nsRect* aGap)
{
  if (nsnull == aGap) {
    aContext.DrawLine(aX1, aY1, aX2, aY2);
  } else {
    nscoord x1 = (aX1 < aX2) ? aX1 : aX2;
    nscoord x2 = (aX1 < aX2) ? aX2 : aX1;
    nsPoint gapUpperRight(aGap->x + aGap->width, aGap->y);
    nsPoint gapLowerRight(aGap->x + aGap->width, aGap->y + aGap->height);
    if ((aGap->y <= aY1) && (gapLowerRight.y >= aY2)) {
      if ((aGap->x > x1) && (aGap->x < x2)) {
        aContext.DrawLine(x1, aY1, aGap->x, aY1);
      } 
      if ((gapLowerRight.x > x1) && (gapLowerRight.x < x2)) {
        aContext.DrawLine(gapUpperRight.x, aY2, x2, aY2);
      } 
    } else {
      aContext.DrawLine(aX1, aY1, aX2, aY2);
    }
  }
}

// Fill a polygon, skipping that portion which crosses aGap. aGap defines a rectangle gap
// This services fieldset legends and only works for points defining a horizontal rectangle 
void nsCSSRendering::FillPolygon (nsIRenderingContext& aContext, 
                                  const nsPoint aPoints[],
                                  PRInt32 aNumPoints,
                                  nsRect* aGap)
{
  if (nsnull == aGap) {
    aContext.FillPolygon(aPoints, aNumPoints);
  } else if (4 == aNumPoints) {
    nsPoint gapUpperRight(aGap->x + aGap->width, aGap->y);
    nsPoint gapLowerRight(aGap->x + aGap->width, aGap->y + aGap->height);

    // sort the 4 points by x
    nsPoint points[4];
    for (PRInt32 pX = 0; pX < 4; pX++) {
      points[pX] = aPoints[pX];
    }
    for (PRInt32 i = 0; i < 3; i++) {
      for (PRInt32 j = i+1; j < 4; j++) { 
        if (points[j].x < points[i].x) {
          nsPoint swap = points[i];
          points[i] = points[j];
          points[j] = swap;
        }
      }
    }

    nsPoint upperLeft  = (points[0].y <= points[1].y) ? points[0] : points[1];
    nsPoint lowerLeft  = (points[0].y <= points[1].y) ? points[1] : points[0];
    nsPoint upperRight = (points[2].y <= points[3].y) ? points[2] : points[3];
    nsPoint lowerRight = (points[2].y <= points[3].y) ? points[3] : points[2];


    if ((aGap->y <= upperLeft.y) && (gapLowerRight.y >= lowerRight.y)) {
      if ((aGap->x > upperLeft.x) && (aGap->x < upperRight.x)) {
        nsPoint leftRect[4];
        leftRect[0] = upperLeft;
        leftRect[1] = nsPoint(aGap->x, upperLeft.y);
        leftRect[2] = nsPoint(aGap->x, lowerLeft.y);
        leftRect[3] = lowerLeft;
        aContext.FillPolygon(leftRect, 4);
      } 
      if ((gapUpperRight.x > upperLeft.x) && (gapUpperRight.x < upperRight.x)) {
        nsPoint rightRect[4];
        rightRect[0] = nsPoint(gapUpperRight.x, upperRight.y);
        rightRect[1] = upperRight;
        rightRect[2] = lowerRight;
        rightRect[3] = nsPoint(gapLowerRight.x, lowerRight.y);
        aContext.FillPolygon(rightRect, 4);
      } 
    } else {
      aContext.FillPolygon(aPoints, aNumPoints);
    }      
  }
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
    NS_Get3DColors(colors, baseColor);
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

      aPoints[np++].MoveTo(outside.x, outside.y);
      aPoints[np++].MoveTo(outside.XMost(), outside.y);
      if (thickness >= twipsPerPixel) {
        aPoints[np++].MoveTo(inside.XMost(), inside.y);
        aPoints[np++].MoveTo(inside.x, inside.y);
      }
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

      if (thickness >= twipsPerPixel) {
        aPoints[np++].MoveTo(outside.x, outside.YMost());
        aPoints[np++].MoveTo(inside.x, inside.YMost());
        aPoints[np++].MoveTo(inside.XMost(), inside.YMost());
        aPoints[np++].MoveTo(outside.XMost(), outside.YMost());
      } else {
        aPoints[np++].MoveTo(outside.x, inside.YMost());
        aPoints[np++].MoveTo(outside.XMost(), inside.YMost());
      }

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

      if (thickness >= twipsPerPixel) {
        aPoints[np++].MoveTo(outside.XMost(), outside.YMost());
        aPoints[np++].MoveTo(outside.XMost(), outside.y);
      }
      aPoints[np++].MoveTo(inside.XMost(), inside.y);
      aPoints[np++].MoveTo(inside.XMost(), inside.YMost());
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
                              const PRUint8 borderStyle,  
                              const nscolor borderColor,  
                              const nsRect& borderOutside,
                              const nsRect& borderInside,
                              PRBool printing,
                              nscoord twipsPerPixel,
                              nsRect* aGap)
{
  nsPoint theSide[MAX_POLY_POINTS];
  nscolor theColor = borderColor; 
  PRUint8 theStyle = borderStyle; 
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
    aContext.SetColor ( MakeBevelColor (whichSide, 
                                        ((theStyle == NS_STYLE_BORDER_STYLE_RIDGE) ?
                                         NS_STYLE_BORDER_STYLE_GROOVE :
                                         NS_STYLE_BORDER_STYLE_RIDGE), 
                                        theColor, printing));
    if (2 == np) {
      //aContext.DrawLine (theSide[0].x, theSide[0].y, theSide[1].x, theSide[1].y);
      DrawLine (aContext, theSide[0].x, theSide[0].y, theSide[1].x, theSide[1].y, aGap);
    } else {
      //aContext.FillPolygon (theSide, np);
      FillPolygon (aContext, theSide, np, aGap);
    }
    np = MakeSide (theSide, aContext, whichSide, borderOutside, borderInside,
                   BORDER_OUTSIDE, 0.5f, twipsPerPixel);
    aContext.SetColor ( MakeBevelColor (whichSide, theStyle, theColor,printing));
    if (2 == np) {
      //aContext.DrawLine (theSide[0].x, theSide[0].y, theSide[1].x, theSide[1].y);
      DrawLine (aContext, theSide[0].x, theSide[0].y, theSide[1].x, theSide[1].y, aGap);
    } else {
      //aContext.FillPolygon (theSide, np);
      FillPolygon (aContext, theSide, np, aGap);
    }
    break;

  case NS_STYLE_BORDER_STYLE_SOLID:
    np = MakeSide (theSide, aContext, whichSide, borderOutside, borderInside,
                   BORDER_FULL, 1.0f, twipsPerPixel);
    aContext.SetColor (borderColor);  
    if (2 == np) {
      //aContext.DrawLine (theSide[0].x, theSide[0].y, theSide[1].x, theSide[1].y);
      DrawLine (aContext, theSide[0].x, theSide[0].y, theSide[1].x, theSide[1].y, aGap);
    } else {
      //aContext.FillPolygon (theSide, np);
      FillPolygon (aContext, theSide, np, aGap);
    }
    break;

  case NS_STYLE_BORDER_STYLE_DOUBLE:
    np = MakeSide (theSide, aContext, whichSide, borderOutside, borderInside,
                   BORDER_INSIDE, 0.333333f, twipsPerPixel);
    aContext.SetColor (borderColor);
    if (2 == np) {
      //aContext.DrawLine (theSide[0].x, theSide[0].y, theSide[1].x, theSide[1].y);
      DrawLine (aContext, theSide[0].x, theSide[0].y, theSide[1].x, theSide[1].y, aGap);
    } else {
      //aContext.FillPolygon (theSide, np);
      FillPolygon (aContext, theSide, np, aGap);
   }
    np = MakeSide (theSide, aContext, whichSide, borderOutside, borderInside,
                   BORDER_OUTSIDE, 0.333333f, twipsPerPixel);
    if (2 == np) {
      //aContext.DrawLine (theSide[0].x, theSide[0].y, theSide[1].x, theSide[1].y);
      DrawLine (aContext, theSide[0].x, theSide[0].y, theSide[1].x, theSide[1].y, aGap);
    } else {
      //aContext.FillPolygon (theSide, np);
      FillPolygon (aContext, theSide, np, aGap);
    }
    break;

  case NS_STYLE_BORDER_STYLE_OUTSET:
  case NS_STYLE_BORDER_STYLE_INSET:
    np = MakeSide (theSide, aContext, whichSide, borderOutside, borderInside,
                   BORDER_FULL, 1.0f, twipsPerPixel);
    aContext.SetColor ( MakeBevelColor (whichSide, theStyle, theColor,printing));
    if (2 == np) {
      //aContext.DrawLine (theSide[0].x, theSide[0].y, theSide[1].x, theSide[1].y);
      DrawLine (aContext, theSide[0].x, theSide[0].y, theSide[1].x, theSide[1].y, aGap);
    } else {
      //aContext.FillPolygon (theSide, np);
      FillPolygon (aContext, theSide, np, aGap);
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
                                     PRIntn aSkipSides,
                                     nsRect* aGap)
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


void nsCSSRendering::DrawDashedSides(PRIntn startSide,
                                     nsIRenderingContext& aContext,
                                     const nsStyleSpacing& aSpacing,
                                     const nsRect& borderOutside,
                                     const nsRect& borderInside,
                                     PRIntn aSkipSides,
                                     nsRect* aGap)
{
  PRIntn dashLength;
  nsRect dashRect, firstRect, currRect;

  PRBool bSolid = PR_TRUE;
  float over = 0.0f;
  PRUint8 style = aSpacing.GetBorderStyle(startSide);  
  PRBool skippedSide = PR_FALSE;
  for (PRIntn whichSide = startSide; whichSide < 4; whichSide++) {
    PRUint8 prevStyle = style;
    style = aSpacing.GetBorderStyle(whichSide);  
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

      aContext.SetColor(aSpacing.GetBorderColor(whichSide));  
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
                                 PRIntn aSkipSides,
                                 nsRect* aGap)
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

  // Turn off rendering for all of the zero sized sides
  if (0 == border.top) aSkipSides |= (1 << NS_SIDE_TOP);
  if (0 == border.right) aSkipSides |= (1 << NS_SIDE_RIGHT);
  if (0 == border.bottom) aSkipSides |= (1 << NS_SIDE_BOTTOM);
  if (0 == border.left) aSkipSides |= (1 << NS_SIDE_LEFT);

  nsRect inside(aBounds);
  nsRect outside(inside);
  outside.Deflate(border);
 
  //see if any sides are dotted or dashed
  for (cnt = 0; cnt < 4; cnt++) {
    if ((aStyle.GetBorderStyle(cnt) == NS_STYLE_BORDER_STYLE_DOTTED) || 
        (aStyle.GetBorderStyle(cnt) == NS_STYLE_BORDER_STYLE_DASHED))  {
      break;
    }
  }
  if (cnt < 4) {
    DrawDashedSides(cnt, aRenderingContext,aStyle,
                    inside, outside, aSkipSides, aGap);
  }

  // Draw all the other sides
  nscoord twipsPerPixel = (nscoord)aPresContext.GetPixelsToTwips();
  if (0 == (aSkipSides & (1<<NS_SIDE_TOP))) {
    DrawSide(aRenderingContext, NS_SIDE_TOP,
             aStyle.GetBorderStyle(NS_SIDE_TOP),
             aStyle.GetBorderColor(NS_SIDE_TOP),
             inside, outside, printing, twipsPerPixel, aGap);
  }
  if (0 == (aSkipSides & (1<<NS_SIDE_LEFT))) {
    DrawSide(aRenderingContext, NS_SIDE_LEFT,
             aStyle.GetBorderStyle(NS_SIDE_LEFT), 
             aStyle.GetBorderColor(NS_SIDE_LEFT),
             inside, outside, printing, twipsPerPixel, aGap);
  }
  if (0 == (aSkipSides & (1<<NS_SIDE_BOTTOM))) {
    DrawSide(aRenderingContext, NS_SIDE_BOTTOM,
             aStyle.GetBorderStyle(NS_SIDE_BOTTOM),
             aStyle.GetBorderColor(NS_SIDE_BOTTOM),
             inside, outside, printing, twipsPerPixel, aGap);
  }
  if (0 == (aSkipSides & (1<<NS_SIDE_RIGHT))) {
    DrawSide(aRenderingContext, NS_SIDE_RIGHT,
             aStyle.GetBorderStyle(NS_SIDE_RIGHT),
             aStyle.GetBorderColor(NS_SIDE_RIGHT),
             inside, outside, printing, twipsPerPixel, aGap);
  }
}

// XXX improve this to constrain rendering to the damaged area
void nsCSSRendering::PaintBorderEdges(nsIPresContext& aPresContext,
                                      nsIRenderingContext& aRenderingContext,
                                      nsIFrame* aForFrame,
                                      const nsRect& aDirtyRect,
                                      const nsRect& aBounds,
                                      nsBorderEdges * aBorderEdges,
                                      PRIntn aSkipSides,
                                      nsRect* aGap)
{
  //PRIntn    cnt;
  PRBool    printing = nsGlobalVariables::Instance()->GetPrinting(&aPresContext);
  if (nsnull==aBorderEdges) {  // Empty border segments
    return;
  }

  // Turn off rendering for all of the zero sized sides
  if (0 == aBorderEdges->mMaxBorderWidth.top) 
    aSkipSides |= (1 << NS_SIDE_TOP);
  if (0 == aBorderEdges->mMaxBorderWidth.right) 
    aSkipSides |= (1 << NS_SIDE_RIGHT);
  if (0 == aBorderEdges->mMaxBorderWidth.bottom) 
    aSkipSides |= (1 << NS_SIDE_BOTTOM);
  if (0 == aBorderEdges->mMaxBorderWidth.left) 
    aSkipSides |= (1 << NS_SIDE_LEFT);

  nsRect inside(aBounds);
  nsRect outside(inside);
  outside.Deflate(aBorderEdges->mMaxBorderWidth);

/* XXX ignoring dotted and dashed for now */
#if 0
  //see if any sides are dotted or dashed
  for (cnt = 0; cnt < 4; cnt++) {
    if ((aStyle.GetBorderStyle(cnt) == NS_STYLE_BORDER_STYLE_DOTTED) || 
        (aStyle.GetBorderStyle(cnt) == NS_STYLE_BORDER_STYLE_DASHED))  {
      break;
    }
  }
  if (cnt < 4) {
    DrawDashedSides(cnt, aRenderingContext,aStyle,
                    inside, outside, aSkipSides, aGap);
  }
#endif    //XXX

  // Draw all the other sides
  nscoord twipsPerPixel = (nscoord)aPresContext.GetPixelsToTwips();
  if (0 == (aSkipSides & (1<<NS_SIDE_TOP))) {
    PRInt32 segmentCount = aBorderEdges->mEdges[NS_SIDE_TOP].Count();
    PRInt32 i;
    for (i=0; i<segmentCount; i++)
    {
      nsBorderEdge * borderEdge =  (nsBorderEdge *)(aBorderEdges->mEdges[NS_SIDE_TOP].ElementAt(i));
      DrawSide(aRenderingContext, NS_SIDE_TOP,
               borderEdge->mStyle,
               borderEdge->mColor,
               inside, outside, printing, twipsPerPixel, aGap);
    }
  }
  if (0 == (aSkipSides & (1<<NS_SIDE_LEFT))) {
    PRInt32 segmentCount = aBorderEdges->mEdges[NS_SIDE_LEFT].Count();
    PRInt32 i;
    for (i=0; i<segmentCount; i++)
    {
      nsBorderEdge * borderEdge =  (nsBorderEdge *)(aBorderEdges->mEdges[NS_SIDE_LEFT].ElementAt(i));
      DrawSide(aRenderingContext, NS_SIDE_LEFT,
               borderEdge->mStyle,
               borderEdge->mColor,
               inside, outside, printing, twipsPerPixel, aGap);
    }
  }
  if (0 == (aSkipSides & (1<<NS_SIDE_BOTTOM))) {
    PRInt32 segmentCount = aBorderEdges->mEdges[NS_SIDE_BOTTOM].Count();
    PRInt32 i;
    for (i=0; i<segmentCount; i++)
    {
      nsBorderEdge * borderEdge =  (nsBorderEdge *)(aBorderEdges->mEdges[NS_SIDE_BOTTOM].ElementAt(i));
      DrawSide(aRenderingContext, NS_SIDE_LEFT,
               borderEdge->mStyle,
               borderEdge->mColor,
               inside, outside, printing, twipsPerPixel, aGap);
    }
  }
  if (0 == (aSkipSides & (1<<NS_SIDE_RIGHT))) {
    PRInt32 segmentCount = aBorderEdges->mEdges[NS_SIDE_RIGHT].Count();
    PRInt32 i;
    for (i=0; i<segmentCount; i++)
    {
      nsBorderEdge * borderEdge =  (nsBorderEdge *)(aBorderEdges->mEdges[NS_SIDE_RIGHT].ElementAt(i));
      DrawSide(aRenderingContext, NS_SIDE_LEFT,
               borderEdge->mStyle,
               borderEdge->mColor,
               inside, outside, printing, twipsPerPixel, aGap);
    }
  }
}


//----------------------------------------------------------------------

static void
ComputeBackgroundAnchorPoint(const nsStyleColor& aColor,
                             const nsRect& aBounds,
                             nscoord aTileWidth, nscoord aTileHeight,
                             nsPoint& aResult)
{
  nscoord x;
  if (NS_STYLE_BG_X_POSITION_LENGTH & aColor.mBackgroundFlags) {
    x = aColor.mBackgroundXPosition;
  }
  else {
    nscoord t = aColor.mBackgroundXPosition;
    if (0 == (NS_STYLE_BG_X_POSITION_PERCENT & aColor.mBackgroundFlags)) {
      // XXX map enum to pct here
      t = 0;
    }
    float pct = float(t) / 100.0f;
    nscoord tilePos = nscoord(pct * aTileWidth);
    nscoord boxPos = nscoord(pct * aBounds.width);
    x = boxPos - tilePos;
  }
  if (NS_STYLE_BG_REPEAT_X & aColor.mBackgroundRepeat) {
    // When we are tiling in the x direction the loop will run from
    // the left edge of the box to the right edge of the box. We need
    // to adjust the starting coordinate to lie within the band being
    // rendered.
    if (x < 0) {
      x = -x;
      if (x < 0) {
        // Some joker gave us max-negative-integer.
        x = 0;
      }
      x %= aTileWidth;
      x = -x;
    }
    else {
      x %= aTileWidth;
      x = x - aTileWidth;
    }
  }
  aResult.x = x;

  nscoord y;
  if (NS_STYLE_BG_Y_POSITION_LENGTH & aColor.mBackgroundFlags) {
    y = aColor.mBackgroundYPosition;
  }
  else {
    nscoord t = aColor.mBackgroundYPosition;
    if (0 == (NS_STYLE_BG_Y_POSITION_PERCENT & aColor.mBackgroundFlags)) {
      // XXX map enum to pct here
      t = 0;
    }
    float pct = float(t) / 100.0f;
    nscoord tilePos = nscoord(pct * aTileHeight);
    nscoord boxPos = nscoord(pct * aBounds.height);
    y = boxPos - tilePos;
  }
  if (NS_STYLE_BG_REPEAT_Y & aColor.mBackgroundRepeat) {
    // When we are tiling in the y direction the loop will run from
    // the top edge of the box to the bottom edge of the box. We need
    // to adjust the starting coordinate to lie within the band being
    // rendered.
    if (y < 0) {
      y = -y;
      if (y < 0) {
        // Some joker gave us max-negative-integer.
        y = 0;
      }
      y %= aTileHeight;
      y = -y;
    }
    else {
      y %= aTileHeight;
      y = y - aTileHeight;
    }
  }
  aResult.y = y;
}

void
nsCSSRendering::PaintBackground(nsIPresContext& aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                nsIFrame* aForFrame,
                                const nsRect& aDirtyRect,
                                const nsRect& aBounds,
                                const nsStyleColor& aColor,
                                nscoord aDX,
                                nscoord aDY)
{
  if (0 < aColor.mBackgroundImage.Length()) {
    // Lookup the image
    nsSize imageSize;
    nsIImage* image = nsnull;
    nsIFrameImageLoader* loader = nsnull;
    PRBool transparentBG = NS_STYLE_BG_COLOR_TRANSPARENT ==
      (aColor.mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT);
    nsresult rv = aPresContext.StartLoadImage(aColor.mBackgroundImage,
                                              transparentBG
                                              ? nsnull
                                              : &aColor.mBackgroundColor,
                                              aForFrame, nsnull,
                                              PR_FALSE, loader);
    if ((NS_OK != rv) || (nsnull == loader) ||
        (loader->GetImage(image), (nsnull == image))) {
      NS_IF_RELEASE(loader);
      // Redraw will happen later
      if (!transparentBG) {
        aRenderingContext.SetColor(aColor.mBackgroundColor);
        aRenderingContext.FillRect(aBounds);
      }
      return;
    }
    loader->GetSize(imageSize);
    NS_RELEASE(loader);

    PRBool needBackgroundColor = PR_FALSE;
#if XXX
    // XXX enable this code as soon as nsIImage can support it
    if (image->NeedsBlend()) {
      needBackgroundColor = PR_TRUE;
    }
#endif

    // Convert image dimensions into nscoord's
    float p2t;
    aPresContext.GetScaledPixelsToTwips(p2t);
    nscoord tileWidth = NSIntPixelsToTwips(imageSize.width, p2t);
    nscoord tileHeight = NSIntPixelsToTwips(imageSize.height, p2t);
    if ((tileWidth == 0) || (tileHeight == 0)) {
      return;
    }

    // Based on the repeat setting, compute how many tiles we should
    // lay down for each axis. The value computed is the maximum based
    // on the dirty rect before accounting for the background-position.
    PRIntn repeat = aColor.mBackgroundRepeat;
    nscoord xDistance, yDistance;
    switch (aColor.mBackgroundRepeat) {
    case NS_STYLE_BG_REPEAT_OFF:
    default:
      xDistance = tileWidth;
      yDistance = tileHeight;
      needBackgroundColor = PR_TRUE;
      break;
    case NS_STYLE_BG_REPEAT_X:
      xDistance = aDirtyRect.width;
      yDistance = tileHeight;
      needBackgroundColor = PR_TRUE;
      break;
    case NS_STYLE_BG_REPEAT_Y:
      xDistance = tileWidth;
      yDistance = aDirtyRect.height;
      needBackgroundColor = PR_TRUE;
      break;
    case NS_STYLE_BG_REPEAT_XY:
      xDistance = aDirtyRect.width;
      yDistance = aDirtyRect.height;
      break;
    }
    if (needBackgroundColor) {
      aRenderingContext.SetColor(aColor.mBackgroundColor);
      aRenderingContext.FillRect(aBounds);
    }

    // Compute the anchor point, relative to the bounding box
    // (aBounds) where the background image rendering should
    // begin. When tiling, the anchor coordinate values will be
    // negative offsets from aBounds origin.
    nsPoint anchor;
    ComputeBackgroundAnchorPoint(aColor, aBounds,
                                 tileWidth, tileHeight, anchor);

    // Setup clipping so that rendering doesn't leak out of the dirty rect
    PRBool clipState;
    aRenderingContext.PushState();
    aRenderingContext.SetClipRect(aDirtyRect, nsClipCombine_kIntersect,
                                  clipState);

    // Compute the x and y starting points and limits for tiling
    nscoord x0, x1;
    if (NS_STYLE_BG_REPEAT_X & aColor.mBackgroundRepeat) {
      // When tiling in the x direction, adjust the starting position
      // of the tile to account for the aDirtyRect.x. When tiling in
      // x, the anchor.x value will be a negative value used to adjust
      // the starting coordinate.
      x0 = (aDirtyRect.x / tileWidth) * tileWidth + anchor.x;
      x1 = x0 + xDistance + tileWidth;
      if (0 != anchor.x) {
        x1 += tileWidth;
      }
    }
    else {
      // When tiling is off in x, anchor.x is relative to aBounds.x
      x0 = aBounds.x + anchor.x;
      x1 = x0 + tileWidth;
    }

    nscoord y0, y1;
    if (NS_STYLE_BG_REPEAT_Y & aColor.mBackgroundRepeat) {
      // When tiling in the y direction, adjust the starting position
      // of the tile to account for the aDirtyRect.y. When tiling in
      // y, the anchor.y value will be a negative value used to adjust
      // the starting coordinate.
      y0 = (aDirtyRect.y / tileHeight) * tileHeight + anchor.y;
      y1 = y0 + yDistance + tileHeight;
      if (0 != anchor.y) {
        y1 += tileHeight;
      }
    }
    else {
      // When tiling is off in y, anchor.y is relative to aBounds.y
      y0 = aBounds.y + anchor.y;
      y1 = y0 + tileHeight;
    }

    // Tile the image in x and y
    nscoord x, y;
    for (y = y0; y < y1; y += tileHeight) {
      for (x = x0; x < x1; x += tileWidth) {
        aRenderingContext.DrawImage(image, x, y, tileWidth, tileHeight);
      }
    }

    // Restore clipping
    aRenderingContext.PopState(clipState);
  } else {
    if (0 == (aColor.mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT)) {
      // XXX This step can be avoided if we have an image and it doesn't
      // have any transparent pixels, and the image is tiled in both
      // the x and the y
      aRenderingContext.SetColor(aColor.mBackgroundColor);
      aRenderingContext.FillRect(aBounds);
    }
  }
}

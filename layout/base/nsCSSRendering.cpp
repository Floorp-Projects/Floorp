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
#include "nsStyleUtil.h"

#define BORDER_FULL    0        //entire side
#define BORDER_INSIDE  1        //inside half
#define BORDER_OUTSIDE 2        //outside half

//thickness of dashed line relative to dotted line
#define DOT_LENGTH  1           //square
#define DASH_LENGTH 3           //3 times longer than dot


/** The following classes are used by CSSRendering for the rounded rect implementation */
#define MAXPATHSIZE 12
#define MAXPOLYPATHSIZE 1000

enum ePathTypes{
  eOutside =0,
  eInside,
  eCalc,
  eCalcRev
};

static void GetPath(nsPoint aPoints[],nsPoint aPolyPath[],PRInt32 *aCurIndex,ePathTypes  aPathType,PRInt32 &aC1Index,float aFrac=0);


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
                                       nscolor aBackgroundColor,
                                       nscolor aBorderColor,
                                       PRBool aSpecialCase)
{

  nscolor colors[2];
  nscolor theColor;

  // Given a background color and a border color
  // calculate the color used for the shading
  if(aSpecialCase)
    NS_GetSpecial3DColors(colors, aBackgroundColor, aBorderColor);
  else
    NS_Get3DColors(colors, aBackgroundColor);
 

  if ((style == NS_STYLE_BORDER_STYLE_BG_OUTSET) ||
	  (style == NS_STYLE_BORDER_STYLE_OUTSET) ||
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
  default:
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
                              const nscolor aBackgroundColor,
                              const nsRect& borderOutside,
                              const nsRect& borderInside,
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
										 aBackgroundColor, theColor, 
										 PR_TRUE));
    if (2 == np) {
      //aContext.DrawLine (theSide[0].x, theSide[0].y, theSide[1].x, theSide[1].y);
      DrawLine (aContext, theSide[0].x, theSide[0].y, theSide[1].x, theSide[1].y, aGap);
    } else {
      //aContext.FillPolygon (theSide, np);
      FillPolygon (aContext, theSide, np, aGap);
    }
    np = MakeSide (theSide, aContext, whichSide, borderOutside, borderInside,
                   BORDER_OUTSIDE, 0.5f, twipsPerPixel);
    aContext.SetColor ( MakeBevelColor (whichSide, theStyle, aBackgroundColor, 
		                                theColor, PR_TRUE));
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

  case NS_STYLE_BORDER_STYLE_BG_OUTSET:
  case NS_STYLE_BORDER_STYLE_BG_INSET:
    np = MakeSide (theSide, aContext, whichSide, borderOutside, borderInside,
                   BORDER_FULL, 1.0f, twipsPerPixel);
    aContext.SetColor ( MakeBevelColor (whichSide, theStyle, aBackgroundColor,
		                                 theColor, PR_FALSE));
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
    aContext.SetColor ( MakeBevelColor (whichSide, theStyle, aBackgroundColor, 
		                                theColor, PR_TRUE));
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

      nscolor sideColor;
      if (! aSpacing.GetBorderColor(whichSide, sideColor)) {
        continue; // side is transparent
      }
      aContext.SetColor(sideColor);  
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

/* draw the portions of the border described in aBorderEdges that are dashed.
 * a border has 4 edges.  Each edge has 1 or more segments. 
 * "inside edges" are drawn differently than "outside edges" so the shared edges will match up.
 * in the case of table collapsing borders, the table edge is the "outside" edge and
 * cell edges are always "inside" edges (so adjacent cells have 2 shared "inside" edges.)
 * There is a case for each of the four sides.  Only the left side is well documented.  The others
 * are very similar.
 */
// XXX: doesn't do corners or junctions well at all.  Just uses logic stolen 
//      from DrawDashedSides which is insufficient
void nsCSSRendering::DrawDashedSegments(nsIRenderingContext& aContext,
                                        const nsRect& aBounds,
                                        nsBorderEdges * aBorderEdges,
                                        PRIntn aSkipSides,
                                        nsRect* aGap)
{
  PRIntn dashLength;
  nsRect dashRect, currRect;

  PRBool bSolid = PR_TRUE;
  float over = 0.0f;
  PRBool skippedSide = PR_FALSE;
  PRIntn whichSide=0;
  // do this just to set up initial condition for loop
  // "segment" is the current portion of the edge we are computing
  nsBorderEdge * segment =  (nsBorderEdge *)(aBorderEdges->mEdges[whichSide].ElementAt(0));
  PRUint8 style = segment->mStyle;  
  for ( ; whichSide < 4; whichSide++) 
  {
    if ((1<<whichSide) & aSkipSides) {
      // Skipped side
      skippedSide = PR_TRUE;
      continue;
    }
    nscoord x=0;  nscoord y=0;
    PRInt32 i;
    PRInt32 segmentCount = aBorderEdges->mEdges[whichSide].Count();
    nsBorderEdges * neighborBorderEdges=nsnull;
    PRIntn neighborEdgeCount=0; // keeps track of which inside neighbor is shared with an outside segment
    for (i=0; i<segmentCount; i++)
    {
      bSolid=PR_TRUE;
      over = 0.0f;
      segment =  (nsBorderEdge *)(aBorderEdges->mEdges[whichSide].ElementAt(i));
      style = segment->mStyle;

      // XXX units for dash & dot?
      if (style == NS_STYLE_BORDER_STYLE_DASHED) {
        dashLength = DASH_LENGTH;
      } else {
        dashLength = DOT_LENGTH;
      }

      aContext.SetColor(segment->mColor);  
      switch (whichSide) {
      case NS_SIDE_LEFT:
      { // draw left segment i
        nsBorderEdge * topEdge =  (nsBorderEdge *)(aBorderEdges->mEdges[NS_SIDE_TOP].ElementAt(0));
        if (0==y)
        { // y is the offset to the top of this segment.  0 means its the topmost left segment
          y = aBorderEdges->mMaxBorderWidth.top - topEdge->mWidth;
          if (PR_TRUE==aBorderEdges->mOutsideEdge)
            y += topEdge->mWidth;
        }
        // the x offset is the x position offset by the max width of the left edge minus this segment's width
        x = aBounds.x + (aBorderEdges->mMaxBorderWidth.left - segment->mWidth);
        nscoord height = segment->mLength;
        // the space between borderOutside and borderInside inclusive is the segment.
        nsRect borderOutside(x, y, aBounds.width, height);
        y += segment->mLength;  // keep track of the y offset for the next segment
        if ((style == NS_STYLE_BORDER_STYLE_DASHED) ||
            (style == NS_STYLE_BORDER_STYLE_DOTTED))
        {
          nsRect borderInside(borderOutside);
          nsMargin outsideMargin(segment->mWidth, 0, 0, 0);
          borderInside.Deflate(outsideMargin);
          nscoord totalLength = segment->mLength; // the computed length of this segment
          // outside edges need info from their inside neighbor.  The following code keeps track
          // of which segment of the inside neighbor's shared edge we should use for this outside segment
          if (PR_TRUE==aBorderEdges->mOutsideEdge)
          {
            if (segment->mInsideNeighbor == neighborBorderEdges)
            {
              neighborEdgeCount++;
            }
            else 
            {
              neighborBorderEdges = segment->mInsideNeighbor;
              neighborEdgeCount=0;
            }
            nsBorderEdge * neighborLeft = (nsBorderEdge *)(segment->mInsideNeighbor->mEdges[NS_SIDE_LEFT].ElementAt(neighborEdgeCount));
            totalLength = neighborLeft->mLength;
          }
          dashRect.width = borderInside.x - borderOutside.x;
          dashRect.height = nscoord(dashRect.width * dashLength);
          dashRect.x = borderOutside.x;
          dashRect.y = borderOutside.y + (totalLength/2) - dashRect.height;
          if ((PR_TRUE==aBorderEdges->mOutsideEdge) && (0!=i))
            dashRect.y -= topEdge->mWidth;  // account for the topmost left edge corner with the leftmost top edge
          if (0)
          {
            printf("  L: totalLength = %d, borderOutside.y = %d, midpoint %d, dashRect.y = %d\n", 
            totalLength, borderOutside.y, borderOutside.y +(totalLength/2), dashRect.y); 
          }
          currRect = dashRect;

          // we draw the segment in 2 halves to get the inside and outside edges to line up on the
          // centerline of the shared edge.

          // draw the top half
          while (currRect.YMost() > borderInside.y) {
            //clip if necessary
            if (currRect.y < borderInside.y) {
              over = float(borderInside.y - dashRect.y) /
                float(dashRect.height);
              currRect.height = currRect.height - (borderInside.y - currRect.y);
              currRect.y = borderInside.y;
            }

            //draw if necessary
            if (0)
            {
              printf("DASHED LEFT: xywh in loop currRect = %d %d %d %d %s\n", 
                   currRect.x, currRect.y, currRect.width, currRect.height, bSolid?"TRUE":"FALSE");
            }
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

          // draw the bottom half
          dashRect.y = borderOutside.y + (totalLength/2) + dashRect.height;
          if ((PR_TRUE==aBorderEdges->mOutsideEdge) && (0!=i))
            dashRect.y -= topEdge->mWidth;
          currRect = dashRect;
          bSolid=PR_TRUE;
          over = 0.0f;
          while (currRect.YMost() < borderInside.YMost()) {
            //clip if necessary
            if (currRect.y < borderInside.y) {
              over = float(borderInside.y - dashRect.y) /
                float(dashRect.height);
              currRect.height = currRect.height - (borderInside.y - currRect.y);
              currRect.y = borderInside.y;
            }

            //draw if necessary
            if (0)
            {
              printf("DASHED LEFT: xywh in loop currRect = %d %d %d %d %s\n", 
                   currRect.x, currRect.y, currRect.width, currRect.height, bSolid?"TRUE":"FALSE");
            }
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
        }
      }
      break;

      case NS_SIDE_TOP:
      { // draw top segment i
        if (0==x)
        {
          nsBorderEdge * leftEdge =  (nsBorderEdge *)(aBorderEdges->mEdges[NS_SIDE_LEFT].ElementAt(0));
          x = aBorderEdges->mMaxBorderWidth.left - leftEdge->mWidth;
        }
        y = aBounds.y;
        if (PR_TRUE==aBorderEdges->mOutsideEdge) // segments of the outside edge are bottom-aligned
          y += aBorderEdges->mMaxBorderWidth.top - segment->mWidth;
        nsRect borderOutside(x, y, segment->mLength, aBounds.height);
        x += segment->mLength;
        if ((style == NS_STYLE_BORDER_STYLE_DASHED) ||
            (style == NS_STYLE_BORDER_STYLE_DOTTED))
        {
          nsRect borderInside(borderOutside);
          nsBorderEdge * neighbor;
          if (PR_TRUE==aBorderEdges->mOutsideEdge)
            neighbor = (nsBorderEdge *)(segment->mInsideNeighbor->mEdges[NS_SIDE_LEFT].ElementAt(0));
          else
            neighbor = (nsBorderEdge *)(aBorderEdges->mEdges[NS_SIDE_LEFT].ElementAt(0));
          nsMargin outsideMargin(neighbor->mWidth, segment->mWidth, 0, segment->mWidth);
          borderInside.Deflate(outsideMargin);
          nscoord firstRectWidth = 0;
          if (PR_TRUE==aBorderEdges->mOutsideEdge && 0==i)
          {
            firstRectWidth = borderInside.x - borderOutside.x;
            aContext.FillRect(borderOutside.x, borderOutside.y,
                              firstRectWidth,
                              borderInside.y - borderOutside.y);
          }

          dashRect.height = borderInside.y - borderOutside.y;
          dashRect.width = dashRect.height * dashLength;
          dashRect.x = borderOutside.x + firstRectWidth;
          dashRect.y = borderOutside.y;
          currRect = dashRect;

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
        }
      }
      break;

      case NS_SIDE_RIGHT:
      { // draw right segment i
        nsBorderEdge * topEdge =  (nsBorderEdge *)
            (aBorderEdges->mEdges[NS_SIDE_TOP].ElementAt(aBorderEdges->mEdges[NS_SIDE_TOP].Count()-1));
        if (0==y)
        {
          y = aBorderEdges->mMaxBorderWidth.top - topEdge->mWidth;
          if (PR_TRUE==aBorderEdges->mOutsideEdge)
            y += topEdge->mWidth;
        }
        nscoord width;
        if (PR_TRUE==aBorderEdges->mOutsideEdge)
        {
          width = aBounds.width - aBorderEdges->mMaxBorderWidth.right;
          width += segment->mWidth;
        }
	      else
        {
          width = aBounds.width;
        }
        nscoord height = segment->mLength;
        nsRect borderOutside(aBounds.x, y, width, height);
        y += segment->mLength;
        if ((style == NS_STYLE_BORDER_STYLE_DASHED) ||
            (style == NS_STYLE_BORDER_STYLE_DOTTED))
        {
          nsRect borderInside(borderOutside);
          nsMargin outsideMargin(segment->mWidth, 0, (segment->mWidth), 0);
          borderInside.Deflate(outsideMargin);
          nscoord totalLength = segment->mLength;
          if (PR_TRUE==aBorderEdges->mOutsideEdge)
          {
            if (segment->mInsideNeighbor == neighborBorderEdges)
            {
              neighborEdgeCount++;
            }
            else 
            {
              neighborBorderEdges = segment->mInsideNeighbor;
              neighborEdgeCount=0;
            }
			      nsBorderEdge * neighborRight = (nsBorderEdge *)(segment->mInsideNeighbor->mEdges[NS_SIDE_RIGHT].ElementAt(neighborEdgeCount));
            totalLength = neighborRight->mLength;
          }
          dashRect.width = borderOutside.XMost() - borderInside.XMost();
          dashRect.height = nscoord(dashRect.width * dashLength);
          dashRect.x = borderInside.XMost();
          dashRect.y = borderOutside.y + (totalLength/2) - dashRect.height;
          if ((PR_TRUE==aBorderEdges->mOutsideEdge) && (0!=i))
            dashRect.y -= topEdge->mWidth;
		      currRect = dashRect;

          // draw the top half
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

          // draw the bottom half
          dashRect.y = borderOutside.y + (totalLength/2) + dashRect.height;
          if ((PR_TRUE==aBorderEdges->mOutsideEdge) && (0!=i))
            dashRect.y -= topEdge->mWidth;
          currRect = dashRect;
          bSolid=PR_TRUE;
          over = 0.0f;
          while (currRect.YMost() < borderInside.YMost()) {
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
            dashRect.y = dashRect.y + currRect.height;
            currRect = dashRect;
          }

        }
      }
      break;

      case NS_SIDE_BOTTOM:
      {  // draw bottom segment i
        if (0==x)
        {
          nsBorderEdge * leftEdge =  (nsBorderEdge *)
            (aBorderEdges->mEdges[NS_SIDE_LEFT].ElementAt(aBorderEdges->mEdges[NS_SIDE_LEFT].Count()-1));
          x = aBorderEdges->mMaxBorderWidth.left - leftEdge->mWidth;
        }
        y = aBounds.y;
        if (PR_TRUE==aBorderEdges->mOutsideEdge) // segments of the outside edge are top-aligned
          y -= aBorderEdges->mMaxBorderWidth.bottom - segment->mWidth;
        nsRect borderOutside(x, y, segment->mLength, aBounds.height);
        x += segment->mLength;
        if ((style == NS_STYLE_BORDER_STYLE_DASHED) ||
            (style == NS_STYLE_BORDER_STYLE_DOTTED))
        {
          nsRect borderInside(borderOutside);
          nsBorderEdge * neighbor;
          if (PR_TRUE==aBorderEdges->mOutsideEdge)
            neighbor = (nsBorderEdge *)(segment->mInsideNeighbor->mEdges[NS_SIDE_LEFT].ElementAt(0));
          else
            neighbor = (nsBorderEdge *)(aBorderEdges->mEdges[NS_SIDE_LEFT].ElementAt(0));
          nsMargin outsideMargin(neighbor->mWidth, segment->mWidth, 0, segment->mWidth);
          borderInside.Deflate(outsideMargin);
          nscoord firstRectWidth = 0;
          if (PR_TRUE==aBorderEdges->mOutsideEdge  &&  0==i)
          {
            firstRectWidth = borderInside.x - borderOutside.x;
            aContext.FillRect(borderOutside.x, borderInside.YMost(),
                              firstRectWidth,
                              borderOutside.YMost() - borderInside.YMost());
          }

          dashRect.height = borderOutside.YMost() - borderInside.YMost();
          dashRect.width = nscoord(dashRect.height * dashLength);
          dashRect.x = borderOutside.x + firstRectWidth;
          dashRect.y = borderInside.YMost();
          currRect = dashRect;

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
        }
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
                                 const nsRect& aBorderArea,
                                 const nsStyleSpacing& aBorderStyle,
                                 nsIStyleContext* aStyleContext,
                                 PRIntn aSkipSides,
                                 nsRect* aGap)
{
  PRIntn    cnt;
  nsMargin  border;
  const nsStyleColor* bgColor = nsStyleUtil::FindNonTransparentBackground(aStyleContext); 
  PRInt16       theRadius;
  nsStyleCoord  borderRadius;

  aBorderStyle.CalcBorderFor(aForFrame, border);
  if ((0 == border.left) && (0 == border.right) &&
      (0 == border.top) && (0 == border.bottom)) {
    // Empty border area
    return;
  }

    // get the radius for our border
  borderRadius = aBorderStyle.mBorderRadius;
  theRadius = 0;
  switch (borderRadius.GetUnit() ) {
    case eStyleUnit_Inherit:
      break;
    case eStyleUnit_Percent:
      break;
    case eStyleUnit_Coord:
      theRadius = borderRadius.GetCoordValue();
      break;
  }

  // rounded version of the border
  if (theRadius > 0){
    PaintRoundedBorder(aPresContext,aRenderingContext,aForFrame,aDirtyRect,aBorderArea,aBorderStyle,aStyleContext,aSkipSides,theRadius,aGap);
    return;
  }

  // Turn off rendering for all of the zero sized sides
  if (0 == border.top) aSkipSides |= (1 << NS_SIDE_TOP);
  if (0 == border.right) aSkipSides |= (1 << NS_SIDE_RIGHT);
  if (0 == border.bottom) aSkipSides |= (1 << NS_SIDE_BOTTOM);
  if (0 == border.left) aSkipSides |= (1 << NS_SIDE_LEFT);

  nsRect inside(aBorderArea);
  nsRect outside(inside);
  outside.Deflate(border);
 
  //see if any sides are dotted or dashed
  for (cnt = 0; cnt < 4; cnt++) {
    if ((aBorderStyle.GetBorderStyle(cnt) == NS_STYLE_BORDER_STYLE_DOTTED) || 
        (aBorderStyle.GetBorderStyle(cnt) == NS_STYLE_BORDER_STYLE_DASHED))  {
      break;
    }
  }
  if (cnt < 4) {
    DrawDashedSides(cnt, aRenderingContext,aBorderStyle,
                    inside, outside, aSkipSides, aGap);
  }

  // Draw all the other sides

  /* XXX something is misnamed here!!!! */
  nscoord twipsPerPixel;/* XXX */
  float p2t;/* XXX */
  aPresContext.GetPixelsToTwips(&p2t);/* XXX */
  twipsPerPixel = (nscoord) p2t;/* XXX */

  nscolor sideColor;
  if (0 == (aSkipSides & (1<<NS_SIDE_TOP))) {
    if (aBorderStyle.GetBorderColor(NS_SIDE_TOP, sideColor)) {
      DrawSide(aRenderingContext, NS_SIDE_TOP,
               aBorderStyle.GetBorderStyle(NS_SIDE_TOP),
               sideColor,
               bgColor->mBackgroundColor, inside,outside, 
               twipsPerPixel, aGap);
    }
  }
  if (0 == (aSkipSides & (1<<NS_SIDE_LEFT))) {
    if (aBorderStyle.GetBorderColor(NS_SIDE_LEFT, sideColor)) {
      DrawSide(aRenderingContext, NS_SIDE_LEFT,
               aBorderStyle.GetBorderStyle(NS_SIDE_LEFT), 
               sideColor,
               bgColor->mBackgroundColor,inside, outside,
               twipsPerPixel, aGap);
    }
  }
  if (0 == (aSkipSides & (1<<NS_SIDE_BOTTOM))) {
    if (aBorderStyle.GetBorderColor(NS_SIDE_BOTTOM, sideColor)) {
      DrawSide(aRenderingContext, NS_SIDE_BOTTOM,
               aBorderStyle.GetBorderStyle(NS_SIDE_BOTTOM),
               sideColor,
			   bgColor->mBackgroundColor,inside, outside,
			   twipsPerPixel, aGap);
    }
  }
  if (0 == (aSkipSides & (1<<NS_SIDE_RIGHT))) {
    if (aBorderStyle.GetBorderColor(NS_SIDE_RIGHT, sideColor)) {
      DrawSide(aRenderingContext, NS_SIDE_RIGHT,
               aBorderStyle.GetBorderStyle(NS_SIDE_RIGHT),
               sideColor,
      			   bgColor->mBackgroundColor,inside, outside,
			         twipsPerPixel, aGap);
    }
  }
}

/* draw the edges of the border described in aBorderEdges one segment at a time.
 * a border has 4 edges.  Each edge has 1 or more segments. 
 * "inside edges" are drawn differently than "outside edges" so the shared edges will match up.
 * in the case of table collapsing borders, the table edge is the "outside" edge and
 * cell edges are always "inside" edges (so adjacent cells have 2 shared "inside" edges.)
 * dashed segments are drawn by DrawDashedSegments().
 */
// XXX: doesn't do corners or junctions well at all.  Just uses logic stolen 
//      from PaintBorder which is insufficient

void nsCSSRendering::PaintBorderEdges(nsIPresContext& aPresContext,
                                      nsIRenderingContext& aRenderingContext,
                                      nsIFrame* aForFrame,
                                      const nsRect& aDirtyRect,
                                      const nsRect& aBorderArea,
                                      nsBorderEdges * aBorderEdges,
                                      nsIStyleContext* aStyleContext,
                                      PRIntn aSkipSides,
                                      nsRect* aGap)
{
  const nsStyleColor* bgColor = nsStyleUtil::FindNonTransparentBackground(aStyleContext); 
  
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

  // Draw any dashed or dotted segments separately
  DrawDashedSegments(aRenderingContext, aBorderArea, aBorderEdges, aSkipSides, aGap);

  // Draw all the other sides
  nscoord twipsPerPixel;
  float p2t;
  aPresContext.GetPixelsToTwips(&p2t);
  twipsPerPixel = (nscoord) p2t;/* XXX huh!*/

  if (0 == (aSkipSides & (1<<NS_SIDE_TOP))) {
    PRInt32 segmentCount = aBorderEdges->mEdges[NS_SIDE_TOP].Count();
    PRInt32 i;
    nsBorderEdge * leftEdge =  (nsBorderEdge *)(aBorderEdges->mEdges[NS_SIDE_LEFT].ElementAt(0));
    nscoord x = aBorderEdges->mMaxBorderWidth.left - leftEdge->mWidth;
    for (i=0; i<segmentCount; i++)
    {
      nsBorderEdge * borderEdge =  (nsBorderEdge *)(aBorderEdges->mEdges[NS_SIDE_TOP].ElementAt(i));
      nscoord y = aBorderArea.y;
      if (PR_TRUE==aBorderEdges->mOutsideEdge) // segments of the outside edge are bottom-aligned
        y += aBorderEdges->mMaxBorderWidth.top - borderEdge->mWidth;
      nsRect inside(x, y, borderEdge->mLength, aBorderArea.height);
      x += borderEdge->mLength;
      nsRect outside(inside);
      nsMargin outsideMargin(0, borderEdge->mWidth, 0, 0);
      outside.Deflate(outsideMargin);
      DrawSide(aRenderingContext, NS_SIDE_TOP,
               borderEdge->mStyle,
               borderEdge->mColor,
               bgColor->mBackgroundColor,
               inside, outside,
               twipsPerPixel, aGap);
    }
  }
  if (0 == (aSkipSides & (1<<NS_SIDE_LEFT))) {
    PRInt32 segmentCount = aBorderEdges->mEdges[NS_SIDE_LEFT].Count();
    PRInt32 i;
    nsBorderEdge * topEdge =  (nsBorderEdge *)(aBorderEdges->mEdges[NS_SIDE_TOP].ElementAt(0));
    nscoord y = aBorderEdges->mMaxBorderWidth.top - topEdge->mWidth;
    for (i=0; i<segmentCount; i++)
    {
      nsBorderEdge * borderEdge =  (nsBorderEdge *)(aBorderEdges->mEdges[NS_SIDE_LEFT].ElementAt(i));
      nscoord x = aBorderArea.x + (aBorderEdges->mMaxBorderWidth.left - borderEdge->mWidth);
      nsRect inside(x, y, aBorderArea.width, borderEdge->mLength);
      y += borderEdge->mLength;
      nsRect outside(inside);
      nsMargin outsideMargin(borderEdge->mWidth, 0, 0, 0);
      outside.Deflate(outsideMargin);
      DrawSide(aRenderingContext, NS_SIDE_LEFT,
               borderEdge->mStyle,
               borderEdge->mColor,
               bgColor->mBackgroundColor,
               inside, outside, 
               twipsPerPixel, aGap);
    }
  }
  if (0 == (aSkipSides & (1<<NS_SIDE_BOTTOM))) {
    PRInt32 segmentCount = aBorderEdges->mEdges[NS_SIDE_BOTTOM].Count();
    PRInt32 i;
    nsBorderEdge * leftEdge =  (nsBorderEdge *)
      (aBorderEdges->mEdges[NS_SIDE_LEFT].ElementAt(aBorderEdges->mEdges[NS_SIDE_LEFT].Count()-1));
    nscoord x = aBorderEdges->mMaxBorderWidth.left - leftEdge->mWidth;
    for (i=0; i<segmentCount; i++)
    {
      nsBorderEdge * borderEdge =  (nsBorderEdge *)(aBorderEdges->mEdges[NS_SIDE_BOTTOM].ElementAt(i));
      nscoord y = aBorderArea.y;
      if (PR_TRUE==aBorderEdges->mOutsideEdge) // segments of the outside edge are top-aligned
        y -= (aBorderEdges->mMaxBorderWidth.bottom - borderEdge->mWidth);
      nsRect inside(x, y, borderEdge->mLength, aBorderArea.height);
      x += borderEdge->mLength;
      nsRect outside(inside);
      nsMargin outsideMargin(0, 0, 0, borderEdge->mWidth);
      outside.Deflate(outsideMargin);
      DrawSide(aRenderingContext, NS_SIDE_BOTTOM,
               borderEdge->mStyle,
               borderEdge->mColor,
               bgColor->mBackgroundColor,
               inside, outside,
               twipsPerPixel, aGap);
    }
  }
  if (0 == (aSkipSides & (1<<NS_SIDE_RIGHT))) {
    PRInt32 segmentCount = aBorderEdges->mEdges[NS_SIDE_RIGHT].Count();
    PRInt32 i;
    nsBorderEdge * topEdge =  (nsBorderEdge *)
      (aBorderEdges->mEdges[NS_SIDE_TOP].ElementAt(aBorderEdges->mEdges[NS_SIDE_TOP].Count()-1));
    nscoord y = aBorderEdges->mMaxBorderWidth.top - topEdge->mWidth;
    for (i=0; i<segmentCount; i++)
    {
      nsBorderEdge * borderEdge =  (nsBorderEdge *)(aBorderEdges->mEdges[NS_SIDE_RIGHT].ElementAt(i));
      nscoord width;
      if (PR_TRUE==aBorderEdges->mOutsideEdge)
      {
        width = aBorderArea.width - aBorderEdges->mMaxBorderWidth.right;
        width += borderEdge->mWidth;
      }
	    else
      {
        width = aBorderArea.width;
      }
      nsRect inside(aBorderArea.x, y, width, borderEdge->mLength);
      y += borderEdge->mLength;
      nsRect outside(inside);
      nsMargin outsideMargin(0, 0, (borderEdge->mWidth), 0);
      outside.Deflate(outsideMargin);
      DrawSide(aRenderingContext, NS_SIDE_RIGHT,
               borderEdge->mStyle,
               borderEdge->mColor,
               bgColor->mBackgroundColor,
               inside, outside,
               twipsPerPixel, aGap);
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
                                const nsRect& aBorderArea,
                                const nsStyleColor& aColor,
                                const nsStyleSpacing& aSpacing,
                                nscoord aDX,
                                nscoord aDY)
{
nsMargin      border;
PRInt16       theRadius;
nsStyleCoord  borderRadius;


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
                                              nsnull,
                                              aForFrame, nsnull, nsnull,
                                              &loader);
    if ((NS_OK != rv) || (nsnull == loader) ||
        (loader->GetImage(&image), (nsnull == image))) {
      NS_IF_RELEASE(loader);
      // Redraw will happen later
      if (!transparentBG) {
        aRenderingContext.SetColor(aColor.mBackgroundColor);
        aRenderingContext.FillRect(aBorderArea);
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

    nscoord tileWidth = imageSize.width;
    nscoord tileHeight = imageSize.height;
    if ((tileWidth == 0) || (tileHeight == 0)) {
      return;
    }

    // Background images are tiled over the 'content' and 'padding' areas
    // only (not the 'border' area)
    nsRect    paddingArea(aBorderArea);
    nsMargin  border;

    aSpacing.GetBorder(border);
    paddingArea.Deflate(border);

    // The actual dirty rect is the intersection of the padding area and the
    // dirty rect we were given
    nsRect  dirtyRect;

    if (!dirtyRect.IntersectRect(paddingArea, aDirtyRect)) {
      // Nothing to paint
      return;
    }

    // Based on the repeat setting, compute how many tiles we should
    // lay down for each axis. The value computed is the maximum based
    // on the dirty rect before accounting for the background-position.
    PRIntn repeat = aColor.mBackgroundRepeat;
    nscoord xDistance, yDistance;
    switch (repeat) {
    case NS_STYLE_BG_REPEAT_OFF:
    default:
      xDistance = tileWidth;
      yDistance = tileHeight;
      needBackgroundColor = PR_TRUE;
      break;
    case NS_STYLE_BG_REPEAT_X:
      xDistance = dirtyRect.width;
      yDistance = tileHeight;
      needBackgroundColor = PR_TRUE;
      break;
    case NS_STYLE_BG_REPEAT_Y:
      xDistance = tileWidth;
      yDistance = dirtyRect.height;
      needBackgroundColor = PR_TRUE;
      break;
    case NS_STYLE_BG_REPEAT_XY:
      xDistance = dirtyRect.width;
      yDistance = dirtyRect.height;
      break;
    }

    // The background color is rendered over the 'border' 'padding' and
    // 'content' areas
    if (needBackgroundColor) {
      aRenderingContext.SetColor(aColor.mBackgroundColor);
      aRenderingContext.FillRect(aBorderArea);
    }

    // Compute the anchor point, relative to the padding area where the
    // background image rendering should begin. When tiling, the anchor
    // coordinate values will be negative offsets from the padding area
    nsPoint anchor;
    ComputeBackgroundAnchorPoint(aColor, paddingArea,
                                 tileWidth, tileHeight, anchor);

    // Setup clipping so that rendering doesn't leak out of the computed
    // dirty rect
    PRBool clipState;
    aRenderingContext.PushState();
    aRenderingContext.SetClipRect(dirtyRect, nsClipCombine_kIntersect,
                                  clipState);

    // Compute the x and y starting points and limits for tiling
    nscoord x0, x1;
    if (NS_STYLE_BG_REPEAT_X & repeat) {
      // When tiling in the x direction, adjust the starting position of the
      // tile to account for dirtyRect.x. When tiling in x, the anchor.x value
      // will be a negative value used to adjust the starting coordinate.
      x0 = (dirtyRect.x / tileWidth) * tileWidth + anchor.x;
      x1 = x0 + xDistance + tileWidth;
      if (0 != anchor.x) {
        x1 += tileWidth;
      }
    }
    else {
      // When tiling is off in x, anchor.x is relative to padding area
      x0 = paddingArea.x + anchor.x;
      x1 = x0 + tileWidth;
    }

    nscoord y0, y1;
    if (NS_STYLE_BG_REPEAT_Y & repeat) {
      // When tiling in the y direction, adjust the starting position of the
      // tile to account for dirtyRect.y. When tiling in y, the anchor.y value
      // will be a negative value used to adjust the starting coordinate.
      y0 = (dirtyRect.y / tileHeight) * tileHeight + anchor.y;
      y1 = y0 + yDistance + tileHeight;
      if (0 != anchor.y) {
        y1 += tileHeight;
      }
    }
    else {
      // When tiling is off in y, anchor.y is relative to padding area
      y0 = paddingArea.y + anchor.y;
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
    // See if there's a background color specified. The background color
    // is rendered over the 'border' 'padding' and 'content' areas
    if (0 == (aColor.mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT)) {
      // XXX This step can be avoided if we have an image and it doesn't
      // have any transparent pixels, and the image is tiled in both
      // the x and the y

      // check to see if we have a radius
      borderRadius = aSpacing.mBorderRadius;
      theRadius = 0;
      switch (borderRadius.GetUnit() ) {
        case eStyleUnit_Inherit:
          break;
        case eStyleUnit_Percent:
          break;
        case eStyleUnit_Coord:
          theRadius = borderRadius.GetCoordValue();
          break;
      }

      // rounded version of the border
      if (theRadius > 0){
        PaintRoundedBackground(aPresContext,aRenderingContext,aForFrame,aDirtyRect,aBorderArea,aColor,aSpacing,aDX,aDY,theRadius);
        return;
      }


      aRenderingContext.SetColor(aColor.mBackgroundColor);
      aRenderingContext.FillRect(aBorderArea);
    }
  }
}

static void  AntiAliasPoly(nsIRenderingContext& aRenderingContext,nsPoint aPoints[],PRInt32 aStartIndex,PRInt32 curIndex,PRInt8 aSide,PRInt8 aCorner);


/** ---------------------------------------------------
 *  See documentation in nsCSSRendering.h
 *	@update 3/26/99 dwc
 */
void
nsCSSRendering::PaintRoundedBackground(nsIPresContext& aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                nsIFrame* aForFrame,
                                const nsRect& aDirtyRect,
                                const nsRect& aBorderArea,
                                const nsStyleColor& aColor,
                                const nsStyleSpacing& aSpacing,
                                nscoord aDX,
                                nscoord aDY,
                                PRInt16 aTheRadius)
{
RoundedRect   outerPath;
QBCurve       cr1,cr2,cr3,cr4;
QBCurve       UL,UR,LL,LR;
PRInt32       curIndex,c1Index;
nsPoint       anc1,con,anc2;
nsPoint       thePath[MAXPATHSIZE];
nsPoint       polyPath[MAXPOLYPATHSIZE];
PRInt16       np;
nsMargin      border;
nsStyleCoord  borderRadius;

  aRenderingContext.SetColor(aColor.mBackgroundColor);

  // set the rounded rect up, and let'er rip
  outerPath.Set(aBorderArea.x,aBorderArea.y,aBorderArea.width,aBorderArea.height,aTheRadius);
  outerPath.GetRoundedBorders(UL,UR,LL,LR);

  // BUILD THE ENTIRE OUTSIDE PATH
  // TOP LINE ----------------------------------------------------------------
  UL.MidPointDivide(&cr1,&cr2);
  UR.MidPointDivide(&cr3,&cr4);
  np=0;
  thePath[np++].MoveTo(cr2.mAnc1.x,cr2.mAnc1.y);
  thePath[np++].MoveTo(cr2.mCon.x, cr2.mCon.y);
  thePath[np++].MoveTo(cr2.mAnc2.x, cr2.mAnc2.y);
  thePath[np++].MoveTo(cr3.mAnc1.x, cr3.mAnc1.y);
  thePath[np++].MoveTo(cr3.mCon.x, cr3.mCon.y);
  thePath[np++].MoveTo(cr3.mAnc2.x, cr3.mAnc2.y);

  polyPath[0].x = thePath[0].x;
  polyPath[0].y = thePath[0].y;
  curIndex = 1;
  GetPath(thePath,polyPath,&curIndex,eOutside,c1Index);

  // RIGHT LINE ----------------------------------------------------------------
  LR.MidPointDivide(&cr2,&cr3);
  np=0;
  thePath[np++].MoveTo(cr4.mAnc1.x,cr4.mAnc1.y);
  thePath[np++].MoveTo(cr4.mCon.x, cr4.mCon.y);
  thePath[np++].MoveTo(cr4.mAnc2.x, cr4.mAnc2.y);
  thePath[np++].MoveTo(cr2.mAnc1.x, cr2.mAnc1.y);
  thePath[np++].MoveTo(cr2.mCon.x, cr2.mCon.y);
  thePath[np++].MoveTo(cr2.mAnc2.x, cr2.mAnc2.y);
  GetPath(thePath,polyPath,&curIndex,eOutside,c1Index);

  // BOTTOM LINE ----------------------------------------------------------------
  LL.MidPointDivide(&cr2,&cr4);
  np=0;
  thePath[np++].MoveTo(cr3.mAnc1.x,cr3.mAnc1.y);
  thePath[np++].MoveTo(cr3.mCon.x, cr3.mCon.y);
  thePath[np++].MoveTo(cr3.mAnc2.x, cr3.mAnc2.y);
  thePath[np++].MoveTo(cr2.mAnc1.x, cr2.mAnc1.y);
  thePath[np++].MoveTo(cr2.mCon.x, cr2.mCon.y);
  thePath[np++].MoveTo(cr2.mAnc2.x, cr2.mAnc2.y);
  GetPath(thePath,polyPath,&curIndex,eOutside,c1Index);

  // LEFT LINE ----------------------------------------------------------------
  np=0;
  thePath[np++].MoveTo(cr4.mAnc1.x,cr4.mAnc1.y);
  thePath[np++].MoveTo(cr4.mCon.x, cr4.mCon.y);
  thePath[np++].MoveTo(cr4.mAnc2.x, cr4.mAnc2.y);
  thePath[np++].MoveTo(cr1.mAnc1.x, cr1.mAnc1.y);
  thePath[np++].MoveTo(cr1.mCon.x, cr1.mCon.y);
  thePath[np++].MoveTo(cr1.mAnc2.x, cr1.mAnc2.y);
  GetPath(thePath,polyPath,&curIndex,eOutside,c1Index);

  aRenderingContext.FillPolygon(polyPath,curIndex); 

}


/** ---------------------------------------------------
 *  See documentation in nsCSSRendering.h
 *	@update 3/26/99 dwc
 */
void 
nsCSSRendering::PaintRoundedBorder(nsIPresContext& aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 nsIFrame* aForFrame,
                                 const nsRect& aDirtyRect,
                                 const nsRect& aBorderArea,
                                 const nsStyleSpacing& aBorderStyle,
                                 nsIStyleContext* aStyleContext,
                                 PRIntn aSkipSides,
                                 PRInt16 aBorderRadius,
                                 nsRect* aGap)
{
RoundedRect   outerPath;
QBCurve       UL,LL,UR,LR;
QBCurve       IUL,ILL,IUR,ILR;
QBCurve       cr1,cr2,cr3,cr4;
QBCurve       Icr1,Icr2,Icr3,Icr4;
nsPoint       anc1,con,anc2;
nsPoint       thePath[MAXPATHSIZE];
PRInt16       np;
nsMargin      border;
nscoord       twipsPerPixel;
float         p2t;

  aBorderStyle.CalcBorderFor(aForFrame, border);
  if ((0 == border.left) && (0 == border.right) &&
      (0 == border.top) && (0 == border.bottom)) {
    return;
  }

  // needed for our border thickness
  aPresContext.GetPixelsToTwips(&p2t);
  twipsPerPixel = (nscoord) p2t;

  // Base our thickness check on the segment being less than a pixel and 1/2
  twipsPerPixel += twipsPerPixel >> 2;

  // set the rounded rect up, and let'er rip
  outerPath.Set(aBorderArea.x,aBorderArea.y,aBorderArea.width,aBorderArea.height,aBorderRadius);
  outerPath.GetRoundedBorders(UL,UR,LL,LR);
  outerPath.CalcInsetCurves(IUL,IUR,ILL,ILR,border);

  // TOP LINE -- construct and divide the curves first, then put together our top and bottom paths
  if(0==border.top)
    return;
  // construct and divide the curves needed
  UL.MidPointDivide(&cr1,&cr2);
  UR.MidPointDivide(&cr3,&cr4);
  IUL.MidPointDivide(&Icr1,&Icr2);
  IUR.MidPointDivide(&Icr3,&Icr4);

  // the outer part of the path
  np=0;
  thePath[np++].MoveTo(cr2.mAnc1.x,cr2.mAnc1.y);
  thePath[np++].MoveTo(cr2.mCon.x, cr2.mCon.y);
  thePath[np++].MoveTo(cr2.mAnc2.x, cr2.mAnc2.y);
  thePath[np++].MoveTo(cr3.mAnc1.x, cr3.mAnc1.y);
  thePath[np++].MoveTo(cr3.mCon.x, cr3.mCon.y);
  thePath[np++].MoveTo(cr3.mAnc2.x, cr3.mAnc2.y);
 
  thePath[np++].MoveTo(Icr3.mAnc2.x,Icr3.mAnc2.y);
  thePath[np++].MoveTo(Icr3.mCon.x, Icr3.mCon.y);
  thePath[np++].MoveTo(Icr3.mAnc1.x, Icr3.mAnc1.y);
  thePath[np++].MoveTo(Icr2.mAnc2.x, Icr2.mAnc2.y);
  thePath[np++].MoveTo(Icr2.mCon.x, Icr2.mCon.y);
  thePath[np++].MoveTo(Icr2.mAnc1.x, Icr2.mAnc1.y);

  RenderSide(thePath,aRenderingContext,aBorderStyle,aStyleContext,NS_SIDE_TOP,border,twipsPerPixel);

  // RIGHT  LINE ----------------------------------------------------------------
  if(0==border.right)
    return;
  LR.MidPointDivide(&cr2,&cr3);
  ILR.MidPointDivide(&Icr2,&Icr3);
  np=0;
  thePath[np++].MoveTo(cr4.mAnc1.x,cr4.mAnc1.y);
  thePath[np++].MoveTo(cr4.mCon.x, cr4.mCon.y);
  thePath[np++].MoveTo(cr4.mAnc2.x,cr4.mAnc2.y);
  thePath[np++].MoveTo(cr2.mAnc1.x,cr2.mAnc1.y);
  thePath[np++].MoveTo(cr2.mCon.x, cr2.mCon.y);
  thePath[np++].MoveTo(cr2.mAnc2.x,cr2.mAnc2.y);

  thePath[np++].MoveTo(Icr2.mAnc2.x,Icr2.mAnc2.y);
  thePath[np++].MoveTo(Icr2.mCon.x, Icr2.mCon.y);
  thePath[np++].MoveTo(Icr2.mAnc1.x,Icr2.mAnc1.y);
  thePath[np++].MoveTo(Icr4.mAnc2.x,Icr4.mAnc2.y);
  thePath[np++].MoveTo(Icr4.mCon.x, Icr4.mCon.y);
  thePath[np++].MoveTo(Icr4.mAnc1.x,Icr4.mAnc1.y);

  RenderSide(thePath,aRenderingContext,aBorderStyle,aStyleContext,NS_SIDE_RIGHT,border,twipsPerPixel);

  // bottom line ----------------------------------------------------------------
  if(0==border.bottom)
    return;
  LL.MidPointDivide(&cr2,&cr4);
  ILL.MidPointDivide(&Icr2,&Icr4);
  np=0;
  thePath[np++].MoveTo(cr3.mAnc1.x,cr3.mAnc1.y);
  thePath[np++].MoveTo(cr3.mCon.x, cr3.mCon.y);
  thePath[np++].MoveTo(cr3.mAnc2.x, cr3.mAnc2.y);
  thePath[np++].MoveTo(cr2.mAnc1.x, cr2.mAnc1.y);
  thePath[np++].MoveTo(cr2.mCon.x, cr2.mCon.y);
  thePath[np++].MoveTo(cr2.mAnc2.x, cr2.mAnc2.y);

  thePath[np++].MoveTo(Icr2.mAnc2.x,Icr2.mAnc2.y);
  thePath[np++].MoveTo(Icr2.mCon.x, Icr2.mCon.y);
  thePath[np++].MoveTo(Icr2.mAnc1.x, Icr2.mAnc1.y);
  thePath[np++].MoveTo(Icr3.mAnc2.x, Icr3.mAnc2.y);
  thePath[np++].MoveTo(Icr3.mCon.x, Icr3.mCon.y);
  thePath[np++].MoveTo(Icr3.mAnc1.x, Icr3.mAnc1.y);
  RenderSide(thePath,aRenderingContext,aBorderStyle,aStyleContext,NS_SIDE_BOTTOM,border,twipsPerPixel);

  // left line ----------------------------------------------------------------
  if(0==border.left)
    return;
  np=0;
  thePath[np++].MoveTo(cr4.mAnc1.x,cr4.mAnc1.y);
  thePath[np++].MoveTo(cr4.mCon.x, cr4.mCon.y);
  thePath[np++].MoveTo(cr4.mAnc2.x, cr4.mAnc2.y);
  thePath[np++].MoveTo(cr1.mAnc1.x, cr1.mAnc1.y);
  thePath[np++].MoveTo(cr1.mCon.x, cr1.mCon.y);
  thePath[np++].MoveTo(cr1.mAnc2.x, cr1.mAnc2.y);


  thePath[np++].MoveTo(Icr1.mAnc2.x,Icr1.mAnc2.y);
  thePath[np++].MoveTo(Icr1.mCon.x, Icr1.mCon.y);
  thePath[np++].MoveTo(Icr1.mAnc1.x, Icr1.mAnc1.y);
  thePath[np++].MoveTo(Icr4.mAnc2.x, Icr4.mAnc2.y);
  thePath[np++].MoveTo(Icr4.mCon.x, Icr4.mCon.y);
  thePath[np++].MoveTo(Icr4.mAnc1.x, Icr4.mAnc1.y);

  RenderSide(thePath,aRenderingContext,aBorderStyle,aStyleContext,NS_SIDE_LEFT,border,twipsPerPixel);
}


/** ---------------------------------------------------
 *  See documentation in nsCSSRendering.h
 *	@update 3/26/99 dwc
 */
void 
nsCSSRendering::RenderSide(nsPoint aPoints[],nsIRenderingContext& aRenderingContext,
                        const nsStyleSpacing& aBorderStyle,nsIStyleContext* aStyleContext,
                        PRUint8 aSide,nsMargin  &aBorThick,nscoord aTwipsPerPixel)
{
QBCurve   thecurve,cr1,cr2,cr3,cr4;
nscolor   sideColor;
nsPoint   thePath[MAXPATHSIZE];
nsPoint   polypath[MAXPOLYPATHSIZE];
PRInt32   curIndex,c1Index,c2Index,junk;
PRInt8    border_Style;
PRInt16   r,g,b;

  // set the style information
  aBorderStyle.GetBorderColor(aSide,sideColor);
  aRenderingContext.SetColor ( sideColor );

  // if the border is thin, just draw it 
  if (aBorThick.top<aTwipsPerPixel) {
    // NOTHING FANCY JUST DRAW OUR OUTSIDE BORDER
    thecurve.SetPoints(aPoints[0].x,aPoints[0].y,aPoints[1].x,aPoints[1].y,aPoints[2].x,aPoints[2].y);
    thecurve.SubDivide((nsIRenderingContext*)&aRenderingContext,0,0);
    aRenderingContext.DrawLine(aPoints[2].x,aPoints[2].y,aPoints[3].x,aPoints[3].y);
    thecurve.SetPoints(aPoints[3].x,aPoints[3].y,aPoints[4].x,aPoints[4].y,aPoints[5].x,aPoints[5].y);
    thecurve.SubDivide((nsIRenderingContext*)&aRenderingContext,0,0);
  } else {
    
    border_Style = aBorderStyle.GetBorderStyle(aSide);
    switch (border_Style){
      case NS_STYLE_BORDER_STYLE_OUTSET:
      case NS_STYLE_BORDER_STYLE_INSET:
        {
        const nsStyleColor* bgColor = nsStyleUtil::FindNonTransparentBackground(aStyleContext);
        aBorderStyle.GetBorderColor(aSide,sideColor);
        aRenderingContext.SetColor ( MakeBevelColor (aSide, border_Style, bgColor->mBackgroundColor,sideColor, PR_TRUE));
        }
      case NS_STYLE_BORDER_STYLE_SOLID:
        polypath[0].x = aPoints[0].x;
        polypath[0].y = aPoints[0].y;
        curIndex = 1;
        GetPath(aPoints,polypath,&curIndex,eOutside,c1Index);
        c2Index = curIndex;
        polypath[curIndex].x = aPoints[6].x;
        polypath[curIndex].y = aPoints[6].y;
        curIndex++;
        GetPath(aPoints,polypath,&curIndex,eInside,junk);
        polypath[curIndex].x = aPoints[0].x;
        polypath[curIndex].y = aPoints[0].y;
        curIndex++;
        aRenderingContext.FillPolygon(polypath,curIndex);

        // anti-alias this
        r = NS_GET_R(sideColor);
        g = NS_GET_G(sideColor);
        b = NS_GET_B(sideColor);

        r += (255-r)>>1;
        g += (255-g)>>1;
        b += (255-b)>>1;
        
        sideColor = NS_RGB(r,g,b);

        aRenderingContext.SetColor(sideColor);
        AntiAliasPoly(aRenderingContext,polypath,0,c1Index,aSide,1);
        AntiAliasPoly(aRenderingContext,polypath,c1Index+1,c2Index,aSide,2);

       break;
      case NS_STYLE_BORDER_STYLE_DOUBLE:
        polypath[0].x = aPoints[0].x;
        polypath[0].y = aPoints[0].y;
        curIndex = 1;
        GetPath(aPoints,polypath,&curIndex,eOutside,c1Index);
        aRenderingContext.DrawPolyline(polypath,curIndex);
        polypath[0].x = aPoints[6].x;
        polypath[0].y = aPoints[6].y;
        curIndex = 1;
        GetPath(aPoints,polypath,&curIndex,eInside,c1Index);
        aRenderingContext.DrawPolyline(polypath,curIndex);
        break;
      case NS_STYLE_BORDER_STYLE_NONE:
      case NS_STYLE_BORDER_STYLE_BLANK:
        break;
      case NS_STYLE_BORDER_STYLE_DOTTED:
      case NS_STYLE_BORDER_STYLE_DASHED:
        break;
      case NS_STYLE_BORDER_STYLE_RIDGE:
      case NS_STYLE_BORDER_STYLE_GROOVE:
        {
        const nsStyleColor* bgColor = nsStyleUtil::FindNonTransparentBackground(aStyleContext);
        aBorderStyle.GetBorderColor(aSide,sideColor);
        aRenderingContext.SetColor ( MakeBevelColor (aSide, border_Style, bgColor->mBackgroundColor,sideColor, PR_TRUE));

        polypath[0].x = aPoints[0].x;
        polypath[0].y = aPoints[0].y;
        curIndex = 1;
        GetPath(aPoints,polypath,&curIndex,eOutside,c1Index);
        polypath[curIndex].x = (aPoints[5].x + aPoints[6].x)>>1;
        polypath[curIndex].y = (aPoints[5].y + aPoints[6].y)>>1;
        curIndex++;
        GetPath(aPoints,polypath,&curIndex,eCalcRev,c1Index,.5);
        polypath[curIndex].x = aPoints[0].x;
        polypath[curIndex].y = aPoints[0].y;
        curIndex++;
        aRenderingContext.FillPolygon(polypath,curIndex);

        aRenderingContext.SetColor ( MakeBevelColor (aSide, 
                                                ((border_Style == NS_STYLE_BORDER_STYLE_RIDGE) ?
                                                NS_STYLE_BORDER_STYLE_GROOVE :
                                                NS_STYLE_BORDER_STYLE_RIDGE), 
                                                bgColor->mBackgroundColor,sideColor, PR_TRUE));
       
        polypath[0].x = (aPoints[0].x + aPoints[11].x)>>1;
        polypath[0].y = (aPoints[0].y + aPoints[11].y)>>1;
        curIndex = 1;
        GetPath(aPoints,polypath,&curIndex,eCalc,c1Index,.5);
        polypath[curIndex].x = aPoints[6].x ;
        polypath[curIndex].y = aPoints[6].y;
        curIndex++;
        GetPath(aPoints,polypath,&curIndex,eInside,c1Index);
        polypath[curIndex].x = aPoints[0].x;
        polypath[curIndex].y = aPoints[0].y;
        curIndex++;
        aRenderingContext.FillPolygon(polypath,curIndex);
        }
        break;
      default:
        break;
    }
  }
}

/** ---------------------------------------------------
 *  AntiAlias the polygon
 *	@update 4/13/99 dwc
 */
static void
AntiAliasPoly(nsIRenderingContext& aRenderingContext,nsPoint aPoints[],PRInt32 aStartIndex,PRInt32 aCurIndex,PRInt8 aSide,PRInt8 aCorner)
{
PRInt32 i;
PRInt32 x0,y0,x1,y1,offsetx,offsety;

  offsetx = offsety = 0;
  switch (aSide) {
    case NS_SIDE_TOP:
      if( aCorner == 1) {
        offsetx = 0;
      }else{
        offsetx = -10;
      }  
    break;
    case NS_SIDE_LEFT:
      if( aCorner == 1) {
        offsetx = 0;
      }else{
        offsetx = 0;
      }
    break;
    case NS_SIDE_RIGHT:
      if( aCorner == 1) {
        offsetx = -10;
      }else{
        offsetx = -10;
      }
    break;
    case NS_SIDE_BOTTOM:
      if( aCorner == 1) {
        offsety = -10;
      }else{
        offsety = -10;
      }
    break;
  } 


  for(i=aStartIndex+1;i<aCurIndex;i++) {
    x0 = aPoints[i-1].x+offsetx;
    y0 = aPoints[i-1].y+offsety;
    x1 = aPoints[i].x+offsetx;
    y1 = aPoints[i].y+offsety;
    aRenderingContext.DrawLine(x0,y0,x1,y1); 
  }

}


/** ---------------------------------------------------
 *  See documentation in nsCSSRendering.h
 *	@update 3/26/99 dwc
 */
void 
RoundedRect::CalcInsetCurves(QBCurve &aULCurve,QBCurve &aURCurve,QBCurve &aLLCurve,QBCurve &aLRCurve,nsMargin &aBorder)
{
PRInt32   nLeft,nTop,nRight,nBottom;

  nLeft = mOuterLeft+aBorder.left;
  nTop = mOuterTop+aBorder.top;
  nRight = mOuterRight-aBorder.right;
  nBottom = mOuterBottom-aBorder.bottom;

  if(nLeft > nRight){
    nLeft = nRight;
  }

  if(nTop > nBottom){
    nTop = nBottom;
  }

  // set the passed in curves to the rounded borders of the rectangle
  aULCurve.SetPoints(nLeft,mInnerTop,nLeft,nTop,mInnerLeft,nTop);
  aURCurve.SetPoints(mInnerRight,nTop,nRight,nTop,nRight,mInnerTop);
  aLRCurve.SetPoints(nRight,mInnerBottom,nRight,nBottom,mInnerRight,nBottom);
  aLLCurve.SetPoints(mInnerLeft,nBottom,nLeft,nBottom,nLeft,mInnerBottom);

}

/** ---------------------------------------------------
 *  See documentation in nsCSSRendering.h
 *	@update 4/13/99 dwc
 */
void 
RoundedRect::Set(nscoord aLeft,nscoord aTop,PRInt32  aWidth,PRInt32 aHeight,PRInt16 aRadius)
{
PRInt32 width;

  width = aLeft+aWidth;

  if( aRadius > (aWidth>>1) )
    mRoundness = aWidth>>1;
  else
    mRoundness = aRadius;

  if( mRoundness > (aHeight>>1) )
    mRoundness = aHeight>>1;

  // important coordinates that the path hits
  mOuterLeft = aLeft;
  mOuterRight = aLeft + aWidth;
  mOuterTop = aTop;
  mOuterBottom = aTop+aHeight;
  mInnerLeft = mOuterLeft + mRoundness;
  mInnerRight = mOuterRight - mRoundness;
  mInnerTop = mOuterTop + mRoundness;
  mInnerBottom = mOuterBottom - mRoundness;


}

/** ---------------------------------------------------
 *  See documentation in nsCSSRendering.h
 *	@update 4/13/99 dwc
 */
void 
RoundedRect::GetRoundedBorders(QBCurve &aULCurve,QBCurve &aURCurve,QBCurve &aLLCurve,QBCurve &aLRCurve)
{

  // set the passed in curves to the rounded borders of the rectangle
  aULCurve.SetPoints(mOuterLeft,mInnerTop,mOuterLeft,mOuterTop,mInnerLeft,mOuterTop);
  aURCurve.SetPoints(mInnerRight,mOuterTop,mOuterRight,mOuterTop,mOuterRight,mInnerTop);
  aLRCurve.SetPoints(mOuterRight,mInnerBottom,mOuterRight,mOuterBottom,mInnerRight,mOuterBottom);
  aLLCurve.SetPoints(mInnerLeft,mOuterBottom,mOuterLeft,mOuterBottom,mOuterLeft,mInnerBottom);
}

/** ---------------------------------------------------
 *  Given a qbezier path, convert it into a polygon path
 *	@update 3/26/99 dwc
 *  @param aPoints -- an array of points to use for the path
 *  @param aPolyPath -- an array of points containing the flattened polygon to use
 *  @param aCurIndex -- the index that points to the last element of the array
 *  @param aPathType -- what kind of path that should be returned
 *  @param aFrac -- the inset amount for a eCalc type path
 */
static void 
GetPath(nsPoint aPoints[],nsPoint aPolyPath[],PRInt32 *aCurIndex,ePathTypes  aPathType,PRInt32 &aC1Index,float aFrac)
{
QBCurve thecurve;

  switch (aPathType) {
    case eOutside:
      thecurve.SetPoints(aPoints[0].x,aPoints[0].y,aPoints[1].x,aPoints[1].y,aPoints[2].x,aPoints[2].y);
      thecurve.SubDivide(nsnull,aPolyPath,aCurIndex);
      aC1Index = *aCurIndex;
      aPolyPath[*aCurIndex].x = aPoints[3].x;
      aPolyPath[*aCurIndex].y = aPoints[3].y;
      (*aCurIndex)++;
      thecurve.SetPoints(aPoints[3].x,aPoints[3].y,aPoints[4].x,aPoints[4].y,aPoints[5].x,aPoints[5].y);
      thecurve.SubDivide(nsnull,aPolyPath,aCurIndex);
      break;
    case eInside:
      thecurve.SetPoints(aPoints[6].x,aPoints[6].y,aPoints[7].x,aPoints[7].y,aPoints[8].x,aPoints[8].y);
      thecurve.SubDivide(nsnull,aPolyPath,aCurIndex);
      aPolyPath[*aCurIndex].x = aPoints[9].x;
      aPolyPath[*aCurIndex].y = aPoints[9].y;
      (*aCurIndex)++;
      thecurve.SetPoints(aPoints[9].x,aPoints[9].y,aPoints[10].x,aPoints[10].y,aPoints[11].x,aPoints[11].y);
      thecurve.SubDivide(nsnull,aPolyPath,aCurIndex);
     break;
    case eCalc:
      thecurve.SetPoints( (aPoints[0].x+aPoints[11].x)>>1,(aPoints[0].y+aPoints[11].y)>>1,
                          (aPoints[1].x+aPoints[10].x)>>1,(aPoints[1].y+aPoints[10].y)>>1,
                          (aPoints[2].x+aPoints[9].x)>>1,(aPoints[2].y+aPoints[9].y)>>1);
      thecurve.SubDivide(nsnull,aPolyPath,aCurIndex);
      aPolyPath[*aCurIndex].x = (aPoints[3].x+aPoints[8].x)>>1;
      aPolyPath[*aCurIndex].y = (aPoints[3].y+aPoints[8].y)>>1;
      (*aCurIndex)++;
      thecurve.SetPoints( (aPoints[3].x+aPoints[8].x)>>1,(aPoints[3].y+aPoints[8].y)>>1,
                          (aPoints[4].x+aPoints[7].x)>>1,(aPoints[4].y+aPoints[7].y)>>1,
                          (aPoints[5].x+aPoints[6].x)>>1,(aPoints[5].y+aPoints[6].y)>>1);
      thecurve.SubDivide(nsnull,aPolyPath,aCurIndex);
      break;
    case eCalcRev:
      thecurve.SetPoints( (aPoints[5].x+aPoints[6].x)>>1,(aPoints[5].y+aPoints[6].y)>>1,
                          (aPoints[4].x+aPoints[7].x)>>1,(aPoints[4].y+aPoints[7].y)>>1,
                          (aPoints[3].x+aPoints[8].x)>>1,(aPoints[3].y+aPoints[8].y)>>1);
      thecurve.SubDivide(nsnull,aPolyPath,aCurIndex);
      aPolyPath[*aCurIndex].x = (aPoints[2].x+aPoints[9].x)>>1;
      aPolyPath[*aCurIndex].y = (aPoints[2].y+aPoints[9].y)>>1;
      (*aCurIndex)++;
      thecurve.SetPoints( (aPoints[2].x+aPoints[9].x)>>1,(aPoints[2].y+aPoints[9].y)>>1,
                          (aPoints[1].x+aPoints[10].x)>>1,(aPoints[1].y+aPoints[10].y)>>1,
                          (aPoints[0].x+aPoints[11].x)>>1,(aPoints[0].y+aPoints[11].y)>>1);
      thecurve.SubDivide(nsnull,aPolyPath,aCurIndex);
      break;
  } 
}

/** ---------------------------------------------------
 *  See documentation in nsCSSRendering.h
 *	@update 4/13/99 dwc
 */
void 
QBCurve::SubDivide(nsIRenderingContext *aRenderingContext,nsPoint aPointArray[],PRInt32 *aCurIndex)
{
QBCurve		curve1,curve2;
PRInt16		fx,fy,smag;

  // divide the curve into 2 pieces
	MidPointDivide(&curve1,&curve2);
	
	fx = (PRInt16)abs(curve1.mAnc2.x - this->mCon.x);
	fy = (PRInt16)abs(curve1.mAnc2.y - this->mCon.y);

	smag = fx+fy-(PR_MIN(fx,fy)>>1);
  //smag = fx*fx + fy*fy;
 
	if (smag>2){
		// split the curve again
    curve1.SubDivide(aRenderingContext,aPointArray,aCurIndex);
    curve2.SubDivide(aRenderingContext,aPointArray,aCurIndex);
	}else{
    if(aPointArray ) {
      // save the points for further processing
      aPointArray[*aCurIndex].x = curve1.mAnc2.x;
      aPointArray[*aCurIndex].y = curve1.mAnc2.y;
      (*aCurIndex)++;
      aPointArray[*aCurIndex].x = curve2.mAnc2.x;
      aPointArray[*aCurIndex].y = curve2.mAnc2.y;
      (*aCurIndex)++;
    }else{
		  // draw the curve 
      aRenderingContext->DrawLine(curve1.mAnc1.x,curve1.mAnc1.y,curve1.mAnc2.x,curve1.mAnc2.y);
      aRenderingContext->DrawLine(curve1.mAnc2.x,curve1.mAnc2.y,curve2.mAnc2.x,curve2.mAnc2.y);
    }
	}
}

/** ---------------------------------------------------
 *  See documentation in nsCSSRendering.h
 *	@update 4/13/99 dwc
 */
void 
QBCurve::MidPointDivide(QBCurve *A,QBCurve *B)
{
nsPoint	a1,control1,control2;

  // do the math (averages inline)
  control1.x = (PRInt32)((mAnc1.x + mCon.x)>>1);
	control1.y = (PRInt32)((mAnc1.y + mCon.y)>>1);

  control2.x = (PRInt32)((mAnc2.x + mCon.x)>>1);
	control2.y = (PRInt32)((mAnc2.y + mCon.y)>>1);

  a1.x = (PRInt32)((control1.x + control2.x)>>1);
	a1.y = (PRInt32)((control1.y + control2.y)>>1);

  // put the math into our 2 new curves
  A->mAnc1 = this->mAnc1;
  A->mCon = control1;
  A->mAnc2 = a1;
  B->mAnc1 = a1;
  B->mCon = control2;
  B->mAnc2 = this->mAnc2;
}

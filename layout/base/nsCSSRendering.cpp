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
#include "nsCSSRendering.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIImage.h"
#include "nsIFrame.h"
#include "nsPoint.h"
#include "nsRect.h"
#include "nsIViewManager.h"
#include "nsIPresShell.h"
#include "nsIStyleContext.h"
#include "nsStyleUtil.h"
#include "nsIScrollableView.h"
#include "nsLayoutAtoms.h"
#include "nsIDrawingSurface.h"
#include "nsTransform2D.h"
#include "nsIDeviceContext.h"
#include "nsIContent.h"
#include "nsHTMLAtoms.h"
#include "nsIDocument.h"
#include "nsIScrollableFrame.h"
#include "imgIRequest.h"
#include "imgIContainer.h"

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

static void GetPath(nsFloatPoint aPoints[],nsPoint aPolyPath[],PRInt32 *aCurIndex,ePathTypes  aPathType,PRInt32 &aC1Index,float aFrac=0);
static void TileImage(nsIRenderingContext& aRC,nsDrawingSurface  aDS,nsRect &aSrcRect,PRInt16 aWidth,PRInt16 aHeight);
static PRBool GetBGColorForHTMLElement(nsIPresContext *aPresContext,const nsStyleColor *&aBGColor);
static nsresult GetFrameForBackgroundUpdate(nsIPresContext *aPresContext,nsIFrame *aFrame, nsIFrame **aBGFrame);

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
                                PRIntn aSkipSides,
                                PRIntn borderPart, float borderFrac,
                                nscoord twipsPerPixel)
{
  float borderRest = 1.0f - borderFrac;

  PRIntn np = 0;
  nscoord thickness, outsideEdge, insideEdge, outsideTL, insideTL, outsideBR,
    insideBR;

  // Initialize the following six nscoord's:
  // outsideEdge, insideEdge, outsideTL, insideTL, outsideBR, insideBR
  // so that outsideEdge is the x or y of the outside edge, etc., and
  // outsideTR is the y or x at the top or right end, etc., e.g.:
  //
  // outsideEdge ---  ----------------------------------------
  //                  \                                      /
  //                   \                                    /
  //                    \                                  /
  // insideEdge -------  ----------------------------------
  //                 |   |                                |   |
  //         outsideTL   insideTL                  insideBR   outsideBR       
  //
  // if we don't want the bevel, we'll get rid of it later by setting
  // outsideXX to insideXX

  switch (whichSide) {
  case NS_SIDE_TOP:
    // the TL points are the left end; the BR points are the right end
    outsideEdge = outside.y;
    insideEdge = inside.y;
    outsideTL = outside.x;
    insideTL = inside.x;
    insideBR = inside.XMost();
    outsideBR = outside.XMost();
    break;

  case NS_SIDE_BOTTOM:
    // the TL points are the left end; the BR points are the right end
    outsideEdge = outside.YMost();
    insideEdge = inside.YMost();
    outsideTL = outside.x;
    insideTL = inside.x;
    insideBR = inside.XMost();
    outsideBR = outside.XMost();
    break;

  case NS_SIDE_LEFT:
    // the TL points are the top end; the BR points are the bottom end
    outsideEdge = outside.x;
    insideEdge = inside.x;
    outsideTL = outside.y;
    insideTL = inside.y;
    insideBR = inside.YMost();
    outsideBR = outside.YMost();
    break;

  case NS_SIDE_RIGHT:
    // the TL points are the top end; the BR points are the bottom end
    outsideEdge = outside.XMost();
    insideEdge = inside.XMost();
    outsideTL = outside.y;
    insideTL = inside.y;
    insideBR = inside.YMost();
    outsideBR = outside.YMost();
    break;
  }

  // Don't draw the bevels if an adjacent side is skipped

  if ( (whichSide == NS_SIDE_TOP) || (whichSide == NS_SIDE_BOTTOM) ) {
    // a top or bottom side
    if ((1<<NS_SIDE_LEFT) & aSkipSides) {
      insideTL = outsideTL;
    }
    if ((1<<NS_SIDE_RIGHT) & aSkipSides) {
      insideBR = outsideBR;
    }
  } else {
    // a right or left side
    if ((1<<NS_SIDE_TOP) & aSkipSides) {
      insideTL = outsideTL;
    }
    if ((1<<NS_SIDE_BOTTOM) & aSkipSides) {
      insideBR = outsideBR;
    }
  }

  // move things around when only drawing part of the border

  if (borderPart == BORDER_INSIDE) {
    outsideEdge = nscoord(outsideEdge * borderFrac + insideEdge * borderRest);
    outsideTL = nscoord(outsideTL * borderFrac + insideTL * borderRest);
    outsideBR = nscoord(outsideBR * borderFrac + insideBR * borderRest);
  } else if (borderPart == BORDER_OUTSIDE ) {
    insideEdge = nscoord(insideEdge * borderFrac + outsideEdge * borderRest);
    insideTL = nscoord(insideTL * borderFrac + outsideTL * borderRest);
    insideBR = nscoord(insideBR * borderFrac + outsideBR * borderRest);
  }

  // Base our thickness check on the segment being less than a pixel and 1/2
  twipsPerPixel += twipsPerPixel >> 2;

  // find the thickness of the piece being drawn
  if ((whichSide == NS_SIDE_TOP) || (whichSide == NS_SIDE_LEFT)) {
    thickness = insideEdge - outsideEdge;
  } else {
    thickness = outsideEdge - insideEdge;
  }

  // if returning a line, do it along inside edge for bottom or right borders
  // so that it's in the same place as it would be with polygons (why?)
  // XXX The previous version of the code shortened the right border too.
  if ( !((thickness >= twipsPerPixel) || (borderPart != BORDER_FULL)) &&
       ((whichSide == NS_SIDE_BOTTOM) || (whichSide == NS_SIDE_RIGHT))) {
    outsideEdge = insideEdge;
    }

  // return the appropriate line or trapezoid
  if ((whichSide == NS_SIDE_TOP) || (whichSide == NS_SIDE_BOTTOM)) {
    // top and bottom borders
    aPoints[np++].MoveTo(outsideTL,outsideEdge);
    aPoints[np++].MoveTo(outsideBR,outsideEdge);
    // XXX Making this condition only (thickness >= twipsPerPixel) will
    // improve double borders and some cases of groove/ridge,
    //  but will cause problems with table borders.  See last and third
    // from last tests in test4.htm
    // Doing it this way emulates the old behavior.  It might be worth
    // fixing.
    if ((thickness >= twipsPerPixel) || (borderPart != BORDER_FULL) ) {
      aPoints[np++].MoveTo(insideBR,insideEdge);
      aPoints[np++].MoveTo(insideTL,insideEdge);
    }
  } else {
    // right and left borders
    // XXX Ditto above
    if ((thickness >= twipsPerPixel) || (borderPart != BORDER_FULL) )  {
      aPoints[np++].MoveTo(insideEdge,insideBR);
      aPoints[np++].MoveTo(insideEdge,insideTL);
    }
    aPoints[np++].MoveTo(outsideEdge,outsideTL);
    aPoints[np++].MoveTo(outsideEdge,outsideBR);
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
                              PRIntn aSkipSides,
                              nscoord twipsPerPixel,
                              nsRect* aGap)
{
  nsPoint theSide[MAX_POLY_POINTS];
  nscolor theColor = borderColor; 
  PRUint8 theStyle = borderStyle; 
  PRInt32 np;
  switch (theStyle) {
  case NS_STYLE_BORDER_STYLE_NONE:
  case NS_STYLE_BORDER_STYLE_HIDDEN:
  case NS_STYLE_BORDER_STYLE_BLANK:
    return;

  case NS_STYLE_BORDER_STYLE_DOTTED:    //handled a special case elsewhere
  case NS_STYLE_BORDER_STYLE_DASHED:    //handled a special case elsewhere
    break; // That was easy...

  case NS_STYLE_BORDER_STYLE_GROOVE:
  case NS_STYLE_BORDER_STYLE_RIDGE:
    np = MakeSide (theSide, aContext, whichSide, borderOutside, borderInside, aSkipSides,
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
    np = MakeSide (theSide, aContext, whichSide, borderOutside, borderInside,aSkipSides,
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
    np = MakeSide (theSide, aContext, whichSide, borderOutside, borderInside,aSkipSides,
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
    np = MakeSide (theSide, aContext, whichSide, borderOutside, borderInside,aSkipSides,
                   BORDER_INSIDE, 0.333333f, twipsPerPixel);
    aContext.SetColor (borderColor);
    if (2 == np) {
      //aContext.DrawLine (theSide[0].x, theSide[0].y, theSide[1].x, theSide[1].y);
      DrawLine (aContext, theSide[0].x, theSide[0].y, theSide[1].x, theSide[1].y, aGap);
    } else {
      //aContext.FillPolygon (theSide, np);
      FillPolygon (aContext, theSide, np, aGap);
   }
    np = MakeSide (theSide, aContext, whichSide, borderOutside, borderInside,aSkipSides,
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
    np = MakeSide (theSide, aContext, whichSide, borderOutside, borderInside,aSkipSides,
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
	np = MakeSide (theSide, aContext, whichSide, borderOutside, borderInside,aSkipSides,
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
                                     const nsRect& aDirtyRect,
                                     const PRUint8 borderStyles[],  
                                     const nscolor borderColors[],  
                                     const nsRect& borderOutside,
                                     const nsRect& borderInside,
                                     PRIntn aSkipSides,
                                     nsRect* aGap)
{
PRIntn  dashLength;
nsRect  dashRect, firstRect, currRect;
PRBool  bSolid = PR_TRUE;
float   over = 0.0f;
PRUint8 style = borderStyles[startSide];  
PRBool  skippedSide = PR_FALSE;
nscoord xstart,xwidth,ystart,ywidth;

  // find out were x and y start
  if(aDirtyRect.x > borderInside.x) {
    xstart = aDirtyRect.x;
  } else {
    xstart = borderInside.x;
  }

  if(aDirtyRect.y > borderInside.y) {
    ystart = aDirtyRect.y;
  } else {
    ystart = aDirtyRect.y;
  }

  // find the x and y width
  xwidth = aDirtyRect.XMost();
  ywidth = aDirtyRect.YMost();

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

/** ---------------------------------------------------
 *  See documentation in nsCSSRendering.h
 *	@update 10/22/99 dwc
 */
void nsCSSRendering::DrawDashedSides(PRIntn startSide,
                                     nsIRenderingContext& aContext,
                                     const nsRect& aDirtyRect,
                                     const nsStyleColor* aColorStyle,
                                     const nsStyleBorder* aBorderStyle,  
                                     const nsStyleOutline* aOutlineStyle,  
                                     PRBool aDoOutline,
                                     const nsRect& borderOutside,
                                     const nsRect& borderInside,
                                     PRIntn aSkipSides,
                                     nsRect* aGap)
{

PRIntn  dashLength;
nsRect  dashRect, currRect;
nscoord xstart,xwidth,ystart,ywidth,temp,temp1,adjust;
PRBool  bSolid = PR_TRUE;
float   over = 0.0f;
PRBool  skippedSide = PR_FALSE;

  NS_ASSERTION((aDoOutline && aOutlineStyle) || (!aDoOutline && aBorderStyle), "null params not allowed");
  PRUint8 style = aDoOutline
                  ? aOutlineStyle->GetOutlineStyle()
                  : aBorderStyle->GetBorderStyle(startSide);  

  // find out were x and y start
  if(aDirtyRect.x > borderInside.x) {
    xstart = aDirtyRect.x;
  } else {
    xstart = borderInside.x;
  }

  if(aDirtyRect.y > borderInside.y) {
    ystart = aDirtyRect.y;
  } else {
    ystart = borderInside.y;
  }


  // find the x and y width
  xwidth = aDirtyRect.XMost();
  ywidth = aDirtyRect.YMost();


  for (PRIntn whichSide = startSide; whichSide < 4; whichSide++) {
    PRUint8 prevStyle = style;
    style = aDoOutline
              ? aOutlineStyle->GetOutlineStyle()
              : aBorderStyle->GetBorderStyle(whichSide);  
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

      if (style == NS_STYLE_BORDER_STYLE_DASHED) {
        dashLength = DASH_LENGTH;
      } else {
        dashLength = DOT_LENGTH;
      }

      nscolor sideColor;
      if (aDoOutline) {
        aOutlineStyle->GetOutlineColor(sideColor);
      } else {
        PRBool transparent; 
        PRBool foreground;
        aBorderStyle->GetBorderColor(whichSide, sideColor, transparent, foreground);
        if (foreground)
          sideColor = aColorStyle->mColor;
        if (transparent)
          continue; // side is transparent
      }
      aContext.SetColor(sideColor);  
      switch (whichSide) {
      case NS_SIDE_RIGHT:
      case NS_SIDE_LEFT:
        bSolid = PR_FALSE;
        
        // This is our dot or dash..
        if(whichSide==NS_SIDE_LEFT){ 
          dashRect.width = borderInside.x - borderOutside.x;
        } else {
          dashRect.width = borderOutside.XMost() - borderInside.XMost();
        }
        if( dashRect.width >0 ) {
          dashRect.height = dashRect.width * dashLength;
          dashRect.y = borderOutside.y;

          if(whichSide == NS_SIDE_RIGHT){
            dashRect.x = borderInside.XMost();
          } else {
            dashRect.x = borderOutside.x;
          }

          temp = borderOutside.YMost();
          temp1 = temp/dashRect.height;

          currRect = dashRect;

          if((temp1%2)==0){
            adjust = (dashRect.height-(temp%dashRect.height))/2; // adjust back
            // draw in the left and right
            aContext.FillRect(dashRect.x, borderOutside.y,dashRect.width, dashRect.height-adjust);
            aContext.FillRect(dashRect.x,(borderOutside.YMost()-(dashRect.height-adjust)),dashRect.width, dashRect.height-adjust);
            currRect.y += (dashRect.height-adjust);
            temp = temp-= (dashRect.height-adjust);
          } else {
            adjust = (temp%dashRect.width)/2;                   // adjust a tad longer
            // draw in the left and right
            aContext.FillRect(dashRect.x, borderOutside.y,dashRect.width, dashRect.height+adjust);
            aContext.FillRect(dashRect.x,(borderOutside.YMost()-(dashRect.height+adjust)),dashRect.width, dashRect.height+adjust);
            currRect.y += (dashRect.height+adjust);
            temp = temp-= (dashRect.height+adjust);
          }
        
          if( temp > ywidth)
            temp = ywidth;

          // get the currRect's x into the view before we start
          if( currRect.y < aDirtyRect.y){
            temp1 = NSToCoordFloor((float)((aDirtyRect.y-currRect.y)/dashRect.height));
            currRect.y += temp1*dashRect.height;
            if((temp1%2)==1){
              bSolid = PR_TRUE;
            }
         }

          while(currRect.y<temp) {
            //draw if necessary
            if (bSolid) {
              aContext.FillRect(currRect);
            }

            bSolid = PRBool(!bSolid);
            currRect.y += dashRect.height;
          }
        }
        break;

      case NS_SIDE_BOTTOM:
      case NS_SIDE_TOP:
        bSolid = PR_FALSE;
        
        // This is our dot or dash..

        if(whichSide==NS_SIDE_TOP){ 
          dashRect.height = borderInside.y - borderOutside.y;
        } else {
          dashRect.height = borderOutside.YMost() - borderInside.YMost();
        }
        if( dashRect.height >0 ) {
          dashRect.width = dashRect.height * dashLength;
          dashRect.x = borderOutside.x;

          if(whichSide == NS_SIDE_BOTTOM){
            dashRect.y = borderInside.YMost();
          } else {
            dashRect.y = borderOutside.y;
          }

          temp = borderOutside.XMost();
          temp1 = temp/dashRect.width;

          currRect = dashRect;

          if((temp1%2)==0){
            adjust = (dashRect.width-(temp%dashRect.width))/2;     // even, adjust back
            // draw in the left and right
            aContext.FillRect(borderOutside.x,dashRect.y,dashRect.width-adjust,dashRect.height);
            aContext.FillRect((borderOutside.XMost()-(dashRect.width-adjust)),dashRect.y,dashRect.width-adjust,dashRect.height);
            currRect.x += (dashRect.width-adjust);
            temp = temp-= (dashRect.width-adjust);
          } else {
            adjust = (temp%dashRect.width)/2;
            // draw in the left and right
            aContext.FillRect(borderOutside.x,dashRect.y,dashRect.width+adjust,dashRect.height);
            aContext.FillRect((borderOutside.XMost()-(dashRect.width+adjust)),dashRect.y,dashRect.width+adjust,dashRect.height);
            currRect.x += (dashRect.width+adjust);
            temp = temp-= (dashRect.width+adjust);
          }
       

          if( temp > xwidth)
            temp = xwidth;

          // get the currRect's x into the view before we start
          if( currRect.x < aDirtyRect.x){
            temp1 = NSToCoordFloor((float)((aDirtyRect.x-currRect.x)/dashRect.width));
            currRect.x += temp1*dashRect.width;
            if((temp1%2)==1){
              bSolid = PR_TRUE;
            }
          }

          while(currRect.x<temp) {
            //draw if necessary
            if (bSolid) {
              aContext.FillRect(currRect);
            }

            bSolid = PRBool(!bSolid);
            currRect.x += dashRect.width;
          }
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

PRBool  bSolid = PR_TRUE;
float   over = 0.0f;
PRBool  skippedSide = PR_FALSE;
PRIntn  whichSide=0;


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
          // XXX Adding check to make sure segment->mInsideNeighbor is not null
          // so it will do the else part, at this point we are assuming this is an
          // ok thing to do (Bug 52130)
          if (PR_TRUE==aBorderEdges->mOutsideEdge && segment->mInsideNeighbor)
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



nscolor 
nsCSSRendering::TransformColor(nscolor  aMapColor,PRBool aNoBackGround)
{
PRInt32 brightness;
nscolor newcolor;

  newcolor = aMapColor;
  if (PR_TRUE == aNoBackGround){
    // convert the RBG to a brightness value (HSV scale)
    brightness = NS_GetBrightness(NS_GET_R(aMapColor),NS_GET_G(aMapColor),NS_GET_B(aMapColor));

    if(brightness > 64){
      newcolor = NS_RGB(64,64,64);
    }
  }

 return newcolor;
}




// method GetBGColorForHTMLElement
//
// Now here's a *fun* hack: Nav4 uses the BODY element's background color for the 
//                          background color on tables so we need to find that element's
//                          color and use it... Actually, we can use the HTML element as well.
//
// Traverse from PresContext to PresShell to Document to RootContent. The RootContent is
// then checked to ensure that it is the HTML or BODY element, and if it is, we get
// it's primary frame and from that the style context and from that the color to use.
//
PRBool GetBGColorForHTMLElement( nsIPresContext *aPresContext,
                                   const nsStyleBackground *&aBGColor )
{
  NS_ASSERTION(aPresContext, "null params not allowed");
  PRBool result = PR_FALSE; // assume we did not find the HTML element

  nsIPresShell* shell = nsnull;
  aPresContext->GetShell(&shell);
  if (shell) {
    nsIDocument *doc = nsnull;
    if (NS_SUCCEEDED(shell->GetDocument(&doc)) && doc) {
      nsIContent *pContent;
      if (NS_SUCCEEDED(doc->GetRootContent(&pContent)) && pContent) {
        // make sure that this is the HTML element
        nsIAtom *tag = nsnull;
        pContent->GetTag(tag);
        NS_ASSERTION(tag, "Tag could not be retrieved from root content element");
        if (tag) {
          if (tag == nsHTMLAtoms::html ||
              tag == nsHTMLAtoms::body) {
            // use this guy's color
            nsIFrame *pFrame = nsnull;
            if (NS_SUCCEEDED(shell->GetPrimaryFrameFor(pContent, &pFrame)) && pFrame) {
              nsIStyleContext *pContext = nsnull;
              pFrame->GetStyleContext(&pContext);
              if (pContext) {
                const nsStyleBackground* color = (const nsStyleBackground*)pContext->GetStyleData(eStyleStruct_Background);
                NS_ASSERTION(color,"ColorStyleData should not be null");
                if (0 == (color->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT)) {
                  aBGColor = color;
                  // set the reslt to TRUE to indicate we mapped the color
                  result = PR_TRUE;
                }
                NS_RELEASE(pContext);
              }// if context
            }// if frame
          }// if tag == html or body
#ifdef DEBUG
          else {
            printf( "Root Content is not HTML or BODY: cannot get bgColor of HTML or BODY\n");
          }
#endif
          NS_RELEASE(tag);
        }// if tag
        NS_RELEASE(pContent);
      }// if content
      NS_RELEASE(doc);
    }// if doc
    NS_RELEASE(shell);
  } // if shell

  return result;
}

// nethod GetFrameForBackgroundUpdate
//
// If the frame (aFrame) is the HTML or BODY frame then find the canvas frame and set the
// aBGFrame param to that. This is used when we need a frame to invalidate after an asynch
// image load for the background.
// 
// The check is a bit expensive, however until the canvas frame is somehow cached on the 
// body frame, or the root element, we need to walk the frames up until we find the canvas
//
nsresult GetFrameForBackgroundUpdate(nsIPresContext *aPresContext,nsIFrame *aFrame, nsIFrame **aBGFrame)
{
  NS_ASSERTION(aFrame && aBGFrame, "illegal null parameter");

  nsresult rv = NS_OK;

  if (aFrame && aBGFrame) {
    *aBGFrame = aFrame; // default to the frame passed in

    nsCOMPtr<nsIContent> pContent;
    aFrame->GetContent(getter_AddRefs(pContent));
    if (pContent) {
      // make sure that this is the HTML or BODY element
      nsCOMPtr<nsIAtom> tag;
      pContent->GetTag(*(getter_AddRefs(tag)));
      if (tag) {
        if (tag.get() == nsHTMLAtoms::html ||
            tag.get() == nsHTMLAtoms::body) {
          // the frame is the body frame, so we provide the canvas frame
          nsIFrame *pCanvasFrame = nsnull;
          aFrame->GetParent(&pCanvasFrame);
          while (pCanvasFrame) {
            nsCOMPtr<nsIAtom>  parentType;
            pCanvasFrame->GetFrameType(getter_AddRefs(parentType));
            if (parentType.get() == nsLayoutAtoms::canvasFrame) {
              *aBGFrame = pCanvasFrame;
              break;
            }
            pCanvasFrame->GetParent(&pCanvasFrame);
          }
        }// if tag == html or body
      }// if tag
    }
  } else {
    rv = NS_ERROR_NULL_POINTER;
  }
  return rv;
}

// helper macro to determine if the borderstyle 'a' is a MOZ-BG-XXX style
#define MOZ_BG_BORDER(a)\
((a==NS_STYLE_BORDER_STYLE_BG_INSET) || (a==NS_STYLE_BORDER_STYLE_BG_OUTSET))

static
PRBool GetBorderColor(const nsStyleColor* aColor, const nsStyleBorder& aBorder, PRUint8 aSide, nscolor& aColorVal)
{
  PRBool transparent;
  PRBool foreground;
  aBorder.GetBorderColor(aSide, aColorVal, transparent, foreground);
  if (foreground)
    aColorVal = aColor->mColor;

  return !transparent;
}

// XXX improve this to constrain rendering to the damaged area
void nsCSSRendering::PaintBorder(nsIPresContext* aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 nsIFrame* aForFrame,
                                 const nsRect& aDirtyRect,
                                 const nsRect& aBorderArea,
                                 const nsStyleBorder& aBorderStyle,
                                 nsIStyleContext* aStyleContext,
                                 PRIntn aSkipSides,
                                 nsRect* aGap,
                                 nscoord aHardBorderSize,
                                 PRBool  aShouldIgnoreRounded)
{
  PRIntn              cnt;
  nsMargin            border;
  nsStyleCoord        bordStyleRadius[4];
  PRInt16             borderRadii[4],i;
  float               percent;
  nsCompatibility     compatMode;
  aPresContext->GetCompatibilityMode(&compatMode);

  // Get our style context's color struct.
  const nsStyleColor* ourColor = (const nsStyleColor*)aStyleContext->GetStyleData(eStyleStruct_Color);

  // Get our style context's background struct.
  const nsStyleBackground* ourBG = (const nsStyleBackground*)aStyleContext->GetStyleData(eStyleStruct_Background);

  // in NavQuirks mode we want to use the parent's context as a starting point 
  // for determining the background color
  const nsStyleBackground* bgColor = 
    nsStyleUtil::FindNonTransparentBackground(aStyleContext, 
                                            compatMode == eCompatibility_NavQuirks ? PR_TRUE : PR_FALSE); 
  // mozBGColor is used instead of bgColor when the display type is BG_INSET or BG_OUTSET
  // AND, in quirk mode, it is set to the BODY element's background color instead of the nearest
  // ancestor's background color.
  const nsStyleBackground* mozBGColor = bgColor;

  // now check if we are in Quirks mode and have a border style of BG_INSET or OUTSET
  // - if so we use the bgColor from the HTML element instead of the nearest ancestor
  if (compatMode == eCompatibility_NavQuirks) {
    PRBool bNeedBodyBGColor = PR_FALSE;
    if (aStyleContext) {
      for (cnt=0; cnt<4;cnt++) {
        bNeedBodyBGColor = MOZ_BG_BORDER(aBorderStyle.GetBorderStyle(cnt));
        if (bNeedBodyBGColor) {
          break;
        }
      }
    }
    if (bNeedBodyBGColor) {
      GetBGColorForHTMLElement(aPresContext, mozBGColor);
    } 
  }

  if (aHardBorderSize > 0) {
    border.SizeTo(aHardBorderSize, aHardBorderSize, aHardBorderSize, aHardBorderSize);
  } else {
    aBorderStyle.CalcBorderFor(aForFrame, border);
  }
  if ((0 == border.left) && (0 == border.right) &&
      (0 == border.top) && (0 == border.bottom)) {
    // Empty border area
    return;
  }


  // get the radius for our border
  aBorderStyle.mBorderRadius.GetTop(bordStyleRadius[0]);      //topleft
  aBorderStyle.mBorderRadius.GetRight(bordStyleRadius[1]);    //topright
  aBorderStyle.mBorderRadius.GetBottom(bordStyleRadius[2]);   //bottomright
  aBorderStyle.mBorderRadius.GetLeft(bordStyleRadius[3]);     //bottomleft

  for(i=0;i<4;i++) {
    borderRadii[i] = 0;
    switch ( bordStyleRadius[i].GetUnit()) {
    case eStyleUnit_Inherit:
      break;
    case eStyleUnit_Percent:
      percent = bordStyleRadius[i].GetPercentValue();
      borderRadii[i] = (nscoord)(percent * aBorderArea.width);
      break;
    case eStyleUnit_Coord:
      borderRadii[i] = bordStyleRadius[i].GetCoordValue();
      break;
    default:
      break;
    }
  }

  // rounded version of the outline
  // check for any corner that is rounded
  for(i=0;i<4;i++){
    if(borderRadii[i] > 0){
      PaintRoundedBorder(aPresContext,aRenderingContext,aForFrame,aDirtyRect,aBorderArea,&aBorderStyle,nsnull,aStyleContext,aSkipSides,borderRadii,aGap,PR_FALSE);
      return;
    }
  }

  // Turn off rendering for all of the zero sized sides
  if (0 == border.top) aSkipSides |= (1 << NS_SIDE_TOP);
  if (0 == border.right) aSkipSides |= (1 << NS_SIDE_RIGHT);
  if (0 == border.bottom) aSkipSides |= (1 << NS_SIDE_BOTTOM);
  if (0 == border.left) aSkipSides |= (1 << NS_SIDE_LEFT);

  // get the inside and outside parts of the border
  nsRect outerRect(aBorderArea);
  nsRect innerRect(outerRect);
  innerRect.Deflate(border);

  if (border.left + border.right > aBorderArea.width) {
    innerRect.x = outerRect.x;
    innerRect.width = outerRect.width;
  }
  if (border.top + border.bottom > aBorderArea.height) {
    innerRect.y = outerRect.y;
    innerRect.height = outerRect.height;
  }



  // If the dirty rect is completely inside the border area (e.g., only the
  // content is being painted), then we can skip out now
  if (innerRect.Contains(aDirtyRect)) {
    return;
  }
 
  //see if any sides are dotted or dashed
  for (cnt = 0; cnt < 4; cnt++) {
    if ((aBorderStyle.GetBorderStyle(cnt) == NS_STYLE_BORDER_STYLE_DOTTED) || 
        (aBorderStyle.GetBorderStyle(cnt) == NS_STYLE_BORDER_STYLE_DASHED))  {
      break;
    }
  }
  if (cnt < 4) {
    DrawDashedSides(cnt, aRenderingContext,aDirtyRect, ourColor, &aBorderStyle,nsnull, PR_FALSE,
                    outerRect, innerRect, aSkipSides, aGap);
  }

  // Draw all the other sides

  /* Get our conversion values */
  nscoord twipsPerPixel;
  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);
  twipsPerPixel = NSIntPixelsToTwips(1,p2t);


  nscolor sideColor;
  if (0 == (aSkipSides & (1<<NS_SIDE_BOTTOM))) {
    if (GetBorderColor(ourColor, aBorderStyle, NS_SIDE_BOTTOM, sideColor)) {
      DrawSide(aRenderingContext, NS_SIDE_BOTTOM,
               aBorderStyle.GetBorderStyle(NS_SIDE_BOTTOM),
               sideColor,
               MOZ_BG_BORDER(aBorderStyle.GetBorderStyle(NS_SIDE_BOTTOM)) ? 
                mozBGColor->mBackgroundColor :
                bgColor->mBackgroundColor,
               outerRect,innerRect, aSkipSides,
               twipsPerPixel, aGap);
    }
  }
  if (0 == (aSkipSides & (1<<NS_SIDE_LEFT))) {
    if (GetBorderColor(ourColor, aBorderStyle, NS_SIDE_LEFT, sideColor)) {
      DrawSide(aRenderingContext, NS_SIDE_LEFT,
               aBorderStyle.GetBorderStyle(NS_SIDE_LEFT), 
               sideColor,
               MOZ_BG_BORDER(aBorderStyle.GetBorderStyle(NS_SIDE_LEFT)) ? 
                mozBGColor->mBackgroundColor :
                bgColor->mBackgroundColor,
               outerRect, innerRect,aSkipSides,
               twipsPerPixel, aGap);
    }
  }
  if (0 == (aSkipSides & (1<<NS_SIDE_TOP))) {
    if (GetBorderColor(ourColor, aBorderStyle, NS_SIDE_TOP, sideColor)) {
      DrawSide(aRenderingContext, NS_SIDE_TOP,
               aBorderStyle.GetBorderStyle(NS_SIDE_TOP),
               sideColor,
               MOZ_BG_BORDER(aBorderStyle.GetBorderStyle(NS_SIDE_TOP)) ? 
                mozBGColor->mBackgroundColor :
                bgColor->mBackgroundColor,
               outerRect, innerRect,aSkipSides,
               twipsPerPixel, aGap);
    }
  }
  if (0 == (aSkipSides & (1<<NS_SIDE_RIGHT))) {
    if (GetBorderColor(ourColor, aBorderStyle, NS_SIDE_RIGHT, sideColor)) {
      DrawSide(aRenderingContext, NS_SIDE_RIGHT,
               aBorderStyle.GetBorderStyle(NS_SIDE_RIGHT),
               sideColor,
               MOZ_BG_BORDER(aBorderStyle.GetBorderStyle(NS_SIDE_RIGHT)) ? 
                mozBGColor->mBackgroundColor :
                bgColor->mBackgroundColor,
               outerRect, innerRect,aSkipSides,
               twipsPerPixel, aGap);
    }
  }
}

// XXX improve this to constrain rendering to the damaged area
void nsCSSRendering::PaintOutline(nsIPresContext* aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 nsIFrame* aForFrame,
                                 const nsRect& aDirtyRect,
                                 const nsRect& aBorderArea,
                                 const nsStyleBorder& aBorderStyle,
                                 const nsStyleOutline& aOutlineStyle,
                                 nsIStyleContext* aStyleContext,
                                 PRIntn aSkipSides,
                                 nsRect* aGap)
{
nsStyleCoord        bordStyleRadius[4];
PRInt16             borderRadii[4],i;
float               percent;
const nsStyleBackground* bgColor = nsStyleUtil::FindNonTransparentBackground(aStyleContext);
nscoord width;

  // Get our style context's color struct.
  const nsStyleColor* ourColor = (const nsStyleColor*)aStyleContext->GetStyleData(eStyleStruct_Color);

  aOutlineStyle.GetOutlineWidth(width);

  if (0 == width) {
    // Empty outline
    return;
  }

  // get the radius for our border
  aOutlineStyle.mOutlineRadius.GetTop(bordStyleRadius[0]);      //topleft
  aOutlineStyle.mOutlineRadius.GetRight(bordStyleRadius[1]);    //topright
  aOutlineStyle.mOutlineRadius.GetBottom(bordStyleRadius[2]);   //bottomright
  aOutlineStyle.mOutlineRadius.GetLeft(bordStyleRadius[3]);     //bottomleft

  for(i=0;i<4;i++) {
    borderRadii[i] = 0;
    switch ( bordStyleRadius[i].GetUnit()) {
    case eStyleUnit_Inherit:
      break;
    case eStyleUnit_Percent:
      percent = bordStyleRadius[i].GetPercentValue();
      borderRadii[i] = (nscoord)(percent * aBorderArea.width);
      break;
    case eStyleUnit_Coord:
      borderRadii[i] = bordStyleRadius[i].GetCoordValue();
      break;
    default:
      break;
    }
  }


  // This if control whether the outline paints on the inside 
  // or outside of the frame
  // XXX This is temporary fix for nsbeta3+ Bug 48973
  // so we can use "mozoutline
#if 0 // outside
  nsRect inside(aBorderArea);
  nsRect outside(inside);
  inside.Inflate(width, width);

  nsRect clipRect(aBorderArea);
  clipRect.Inflate(width, width); // make clip extra big for now

#else // inside
  nsMargin borderWidth;
  aBorderStyle.GetBorder(borderWidth);

  nsRect outside(aBorderArea);
  outside.Deflate(borderWidth);
  nsRect inside(outside);
  inside.Deflate(width, width);

  nsRect clipRect(outside);
#endif

  PRBool clipState = PR_FALSE;
  aRenderingContext.PushState();
  aRenderingContext.SetClipRect(clipRect, nsClipCombine_kReplace, clipState);

  // rounded version of the border
  for(i=0;i<4;i++){
    if(borderRadii[i] > 0){
      PaintRoundedBorder(aPresContext,aRenderingContext,aForFrame,aDirtyRect,aBorderArea,nsnull,&aOutlineStyle,aStyleContext,aSkipSides,borderRadii,aGap,PR_TRUE);
      aRenderingContext.PopState(clipState);
      return;
    }
  }


  PRUint8 outlineStyle = aOutlineStyle.GetOutlineStyle();
  //see if any sides are dotted or dashed
  if ((outlineStyle == NS_STYLE_BORDER_STYLE_DOTTED) || 
      (outlineStyle == NS_STYLE_BORDER_STYLE_DASHED))  {
    DrawDashedSides(0, aRenderingContext, aDirtyRect, ourColor, nsnull, &aOutlineStyle, PR_TRUE,
                    outside, inside, aSkipSides, aGap);
    aRenderingContext.PopState(clipState);
    return;
  }

  // Draw all the other sides

  /* XXX something is misnamed here!!!! */
  nscoord twipsPerPixel;/* XXX */
  float p2t;/* XXX */
  aPresContext->GetPixelsToTwips(&p2t);/* XXX */
  twipsPerPixel = (nscoord) p2t;/* XXX */

  nscolor outlineColor;
  PRBool  canDraw,modeChanged=PR_FALSE;
 
  // see if the outline color is 'invert' or can invert.
  if (aOutlineStyle.GetOutlineInvert() && NS_SUCCEEDED(aRenderingContext.SetPenMode(nsPenMode_kInvert)) ) {
    canDraw = PR_TRUE;
    modeChanged=PR_TRUE;
  } else {
    canDraw = aOutlineStyle.GetOutlineColor(outlineColor);
  }

  if (PR_TRUE == canDraw) {
    DrawSide(aRenderingContext, NS_SIDE_BOTTOM,
             outlineStyle,
             outlineColor,
             bgColor->mBackgroundColor, outside, inside, aSkipSides,
             twipsPerPixel, aGap);

    DrawSide(aRenderingContext, NS_SIDE_LEFT,
             outlineStyle, 
             outlineColor,
             bgColor->mBackgroundColor,outside, inside,aSkipSides,
             twipsPerPixel, aGap);

    DrawSide(aRenderingContext, NS_SIDE_TOP,
             outlineStyle,
             outlineColor,
             bgColor->mBackgroundColor,outside, inside,aSkipSides,
             twipsPerPixel, aGap);

    DrawSide(aRenderingContext, NS_SIDE_RIGHT,
             outlineStyle,
             outlineColor,
             bgColor->mBackgroundColor,outside, inside,aSkipSides,
             twipsPerPixel, aGap);
    if(PR_TRUE == modeChanged ) {
      aRenderingContext.SetPenMode(nsPenMode_kNone);
    }  
  }
  // Restore clipping
  aRenderingContext.PopState(clipState);
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

void nsCSSRendering::PaintBorderEdges(nsIPresContext* aPresContext,
                                      nsIRenderingContext& aRenderingContext,
                                      nsIFrame* aForFrame,
                                      const nsRect& aDirtyRect,
                                      const nsRect& aBorderArea,
                                      nsBorderEdges * aBorderEdges,
                                      nsIStyleContext* aStyleContext,
                                      PRIntn aSkipSides,
                                      nsRect* aGap)
{
  const nsStyleBackground* bgColor = nsStyleUtil::FindNonTransparentBackground(aStyleContext);
  
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
  aPresContext->GetPixelsToTwips(&p2t);
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
               inside, outside,aSkipSides,
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
               inside, outside, aSkipSides,
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
               inside, outside,aSkipSides,
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
               inside, outside,aSkipSides,
               twipsPerPixel, aGap);
    }
  }
}


//----------------------------------------------------------------------

// Returns the anchor point to use for the background image. The
// anchor point is the (x, y) location where the first tile should
// be placed
//
// For repeated tiling, the anchor values are normalized wrt to the upper-left
// edge of the bounds, and are always in the range:
// -(aTileWidth - 1) <= anchor.x <= 0
// -(aTileHeight - 1) <= anchor.y <= 0
//
// i.e., they are either 0 or a negative number whose absolute value is
// less than the tile size in that dimension
//
// aRelativeBounds is the box to which the tiling position should be relative,
// aTilingBounds is the box in which the tiling will actually be done. They
// should be identical except when painting on the canvas, in which case the
// relative bounds should be the bounds of the root element's frame and the
// tiling bounds should be the bounds of the canvas frame.
static void
ComputeBackgroundAnchorPoint(const nsStyleBackground& aColor,
                             const nsRect& aRelativeBounds, const nsRect& aTilingBounds,
                             nscoord aTileWidth, nscoord aTileHeight,
                             nsPoint& aResult)
{
  nscoord x;
  if (NS_STYLE_BG_X_POSITION_LENGTH & aColor.mBackgroundFlags) {
    x = aColor.mBackgroundXPosition;
  }
  else {
    nscoord t = aColor.mBackgroundXPosition;
    float pct = float(t) / 100.0f;
    nscoord tilePos = nscoord(pct * aTileWidth);
    nscoord boxPos = nscoord(pct * aRelativeBounds.width);
    x = boxPos - tilePos;
  }
  x += aRelativeBounds.x - aTilingBounds.x;
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
    else if (x != 0) {
      x %= aTileWidth;
      if (x > 0) {
        x = x - aTileWidth;
      }
    }

    NS_POSTCONDITION((x >= -(aTileWidth - 1)) && (x <= 0), "bad computed anchor value");
  }
  aResult.x = x;

  nscoord y;
  if (NS_STYLE_BG_Y_POSITION_LENGTH & aColor.mBackgroundFlags) {
    y = aColor.mBackgroundYPosition;
  }
  else {
    nscoord t = aColor.mBackgroundYPosition;
    float pct = float(t) / 100.0f;
    nscoord tilePos = nscoord(pct * aTileHeight);
    nscoord boxPos = nscoord(pct * aRelativeBounds.height);
    y = boxPos - tilePos;
  }
  y += aRelativeBounds.y - aTilingBounds.y;
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
    else if (y != 0) {
      y %= aTileHeight;
      if (y > 0) {
        y = y - aTileHeight;
      }
    }
    
    NS_POSTCONDITION((y >= -(aTileHeight - 1)) && (y <= 0), "bad computed anchor value");
  }
  aResult.y = y;
}

// Returns the nearest scroll frame ancestor
static nsIFrame*
GetNearestScrollFrame(nsIFrame* aFrame)
{
  for (nsIFrame* f = aFrame; f; f->GetParent(&f)) {
    nsIAtom*  frameType;
    
    // Is it a scroll frame?
    f->GetFrameType(&frameType);
    if (nsLayoutAtoms::scrollFrame == frameType) {
      NS_RELEASE(frameType);
      return f;
    }

    NS_IF_RELEASE(frameType);
  }

  return nsnull;
}

void
nsCSSRendering::PaintBackground(nsIPresContext* aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                nsIFrame* aForFrame,
                                const nsRect& aDirtyRect,
                                const nsRect& aBorderArea,
                                const nsStyleBackground& aColor,
                                const nsStyleBorder& aBorder,
                                nscoord aDX,
                                nscoord aDY)
{
  NS_ASSERTION(aForFrame, "Frame is expected to be provided to PaintBackground");

  // consider it transparent if transparent is set, or if it is propagated to the parent
  PRBool        transparentBG = 
                  (NS_STYLE_BG_COLOR_TRANSPARENT == (aColor.mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT)) ||
                  (aColor.mBackgroundFlags & NS_STYLE_BG_PROPAGATED_TO_PARENT);
  float         percent;
  nsStyleCoord  bordStyleRadius[4];
  PRInt16       borderRadii[4],i;


  // if there is no background image, try a color.
  if (aColor.mBackgroundImage.IsEmpty()) {
    // See if there's a background color specified. The background color
    // is rendered over the 'border' 'padding' and 'content' areas
    if (!transparentBG) {
      // get the radius for our border
      aBorder.mBorderRadius.GetTop(bordStyleRadius[0]);      //topleft
      aBorder.mBorderRadius.GetRight(bordStyleRadius[1]);    //topright
      aBorder.mBorderRadius.GetBottom(bordStyleRadius[2]);   //bottomright
      aBorder.mBorderRadius.GetLeft(bordStyleRadius[3]);     //bottomleft

      for(i=0;i<4;i++) {
        borderRadii[i] = 0;
        switch ( bordStyleRadius[i].GetUnit()) {
          case eStyleUnit_Inherit:
            break;
          case eStyleUnit_Percent:
            percent = bordStyleRadius[i].GetPercentValue();
            borderRadii[i] = (nscoord)(percent * aBorderArea.width);
            break;
          case eStyleUnit_Coord:
            borderRadii[i] = bordStyleRadius[i].GetCoordValue();
            break;
          default:
            break;
        }
      }

      // rounded version of the border
      for(i=0;i<4;i++){
        if (borderRadii[i] > 0){
          PaintRoundedBackground(aPresContext,aRenderingContext,aForFrame,aDirtyRect,
                                 aBorderArea,aColor,aDX,aDY,borderRadii);
          return;
        }
      }

      aRenderingContext.SetColor(aColor.mBackgroundColor);
      aRenderingContext.FillRect(aBorderArea);
    }
  } else {
    // we have a background image


    // get the frame for the background image load to complete in
    // - this may be different than the frame we are rendering
    //   (as in the case of the canvas frame) 
    nsIFrame *pBGFrame = nsnull;
    GetFrameForBackgroundUpdate(aPresContext, aForFrame, &pBGFrame);
    NS_ASSERTION(pBGFrame, "Background Frame must be set by GetFrameForBackgroundUpdate");

    // Lookup the image
    nsCOMPtr<imgIRequest> req;
    nsresult rv = aPresContext->LoadImage(aColor.mBackgroundImage, pBGFrame, getter_AddRefs(req));

    PRUint32 status = imgIRequest::STATUS_ERROR;
    if (req)
      req->GetImageStatus(&status);

    if (NS_FAILED(rv) || !req || !(status & imgIRequest::STATUS_SIZE_AVAILABLE)) {
      if (!transparentBG) {
        // The background color is rendered over the 'border' 'padding' and
        // 'content' areas
        aRenderingContext.SetColor(aColor.mBackgroundColor);
        aRenderingContext.FillRect(aBorderArea);
      }
      return;
    }

    nsSize imageSize;
    nsCOMPtr<imgIContainer> image;
    req->GetImage(getter_AddRefs(image));

    image->GetWidth(&imageSize.width);
    image->GetHeight(&imageSize.height);

    float p2t;
    aPresContext->GetPixelsToTwips(&p2t);
    imageSize.width = NSIntPixelsToTwips(imageSize.width, p2t);
    imageSize.height = NSIntPixelsToTwips(imageSize.height, p2t);

    req = nsnull;

    // Background images are tiled over the 'content' and 'padding' areas
    // only (not the 'border' area)
    nsRect    paddingArea(aBorderArea);
    nsMargin  border;

    if (!aBorder.GetBorder(border)) {
      NS_NOTYETIMPLEMENTED("percentage border");
    }
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
    nscoord tileWidth = imageSize.width;
    nscoord tileHeight = imageSize.height;
    PRBool  needBackgroundColor = PR_TRUE;
    PRIntn  repeat = aColor.mBackgroundRepeat;
    nscoord xDistance, yDistance;
    PRBool needBackgroundOnContinuation = PR_FALSE; // set to true if repeat-y value is set

    switch (repeat) {
    case NS_STYLE_BG_REPEAT_OFF:
    default:
      xDistance = tileWidth;
      yDistance = tileHeight;
      break;
    case NS_STYLE_BG_REPEAT_X:
      xDistance = dirtyRect.width;
      yDistance = tileHeight;
      break;
    case NS_STYLE_BG_REPEAT_Y:
      xDistance = tileWidth;
      yDistance = dirtyRect.height;
      needBackgroundOnContinuation = PR_TRUE;
      break;
    case NS_STYLE_BG_REPEAT_XY:
      xDistance = dirtyRect.width;
      yDistance = dirtyRect.height;
      needBackgroundOnContinuation = PR_TRUE;
      // We need to render the background color if the image is transparent
      //needBackgroundColor = image->GetHasAlphaMask();
      break;
    }

    // The background color is rendered over the 'border' 'padding' and
    // 'content' areas
    if (!transparentBG && needBackgroundColor) {
      aRenderingContext.SetColor(aColor.mBackgroundColor);
      aRenderingContext.FillRect(aBorderArea);
    }

    // See if there's nothing left to do
    if ((tileWidth == 0) || (tileHeight == 0) || dirtyRect.IsEmpty()) {
      // Nothing to paint
      return;
    }

    // if the frame is a continuation frame, check if we need to draw the image for it
    // (continuation with no repeat setting in the Y direction do not get background images)
    if (aForFrame) {
      nsIFrame *prevInFlowFrame = nsnull;
      aForFrame->GetPrevInFlow(&prevInFlowFrame);
      if (prevInFlowFrame != nsnull) {
        if (!needBackgroundOnContinuation) {
          // the frame is a continuation, and we do not want the background image repeated
          // in the Y direction (needBackgroundOnContinuation == PR_FALSE) so just bail
          return;
        }
      }
    }


    // Compute the anchor point.
    //
    // When tiling, the anchor coordinate values will be negative offsets
    // from the padding area

    nsPoint anchor;
    if (NS_STYLE_BG_ATTACHMENT_FIXED == aColor.mBackgroundAttachment) {
      // If it's a fixed background attachment, then the image is placed 
      // relative to the nearest scrolling ancestor, or the viewport if
      // the frame doesn't have a scrolling ancestor
      nsIFrame* scrolledFrame = nsnull;
      nsIView* viewportView = nsnull;
      nsRect viewportArea;
        
      // get the nsIScrollableFrame interface from the scrollFrame
      nsIFrame* scrollFrame = GetNearestScrollFrame(aForFrame);
      if (scrollFrame) {
        nsCOMPtr<nsIScrollableFrame> scrollableFrame(do_QueryInterface(scrollFrame));
        if (scrollableFrame) {
          scrollableFrame->GetScrolledFrame(aPresContext, scrolledFrame);
          if (scrolledFrame) {
            scrolledFrame->GetRect(viewportArea);
            scrolledFrame->GetView(aPresContext, &viewportView);
          }
        }
      } 
      if (!scrolledFrame) {
        // The viewport isn't scrollable, so use the root frame's view
        nsCOMPtr<nsIPresShell> presShell;
        aPresContext->GetShell(getter_AddRefs(presShell));
        NS_ASSERTION(presShell, "no pres shell");
        
        nsIFrame* rootFrame;
        presShell->GetRootFrame(&rootFrame);
        NS_ASSERTION(rootFrame, "no root frame");

        rootFrame->GetView(aPresContext, &viewportView);
        NS_ASSERTION(viewportView, "no viewport view");
        viewportArea.x = 0;
        viewportArea.y = 0;
        viewportView->GetDimensions(&viewportArea.width, &viewportArea.height);
      }

      // Get the anchor point
      ComputeBackgroundAnchorPoint(aColor, viewportArea, viewportArea, tileWidth, tileHeight, anchor);

      // Convert the anchor point to aForFrame's coordinate space
      nsIView*  view;
      aForFrame->GetView(aPresContext, &view);
      if (!view) {
        nsPoint offset;
        aForFrame->GetOffsetFromView(aPresContext, offset, &view);
        anchor -= offset;
      }
      NS_ASSERTION(view, "expected a view");
      while (view && (view != viewportView)) {
        nscoord x, y;
        
        view->GetPosition(&x, &y);
        anchor.x -= x;
        anchor.y -= y;
        
        // Get the parent view until we reach the viewport view
        view->GetParent(view);
      }

      // Move the padding area so that we can use the same logic for both the 
      // fixed and scrolling cases
      paddingArea.x = 0;
      paddingArea.y = 0;
    } else { 
      nsCOMPtr<nsIAtom> frameType;
      aForFrame->GetFrameType(getter_AddRefs(frameType));
      if (frameType.get() == nsLayoutAtoms::canvasFrame) {
        // If the frame is the canvas, the image is placed relative to
        // the root element's (first) frame (see bug 46446)
        nsRect firstRootElementFrameArea;
        nsIFrame* firstRootElementFrame;
        aForFrame->FirstChild(aPresContext, nsnull, &firstRootElementFrame);
        NS_ASSERTION(firstRootElementFrame, "A canvas with a background "
          "image had no child frame, which is impossible according to CSS. "
          "Make sure there isn't a background image specified on the "
          "|:viewport| pseudo-element in |html.css|.");

        // temporary null check -- see bug 97226
        if (firstRootElementFrame) {
          firstRootElementFrame->GetRect(firstRootElementFrameArea);

          // Take the border out of the frame's rect
          const nsStyleBorder* borderStyle;
          firstRootElementFrame->GetStyleData(eStyleStruct_Border, (const nsStyleStruct*&)borderStyle);
          nsMargin border;
          borderStyle->GetBorder(border);
          firstRootElementFrameArea.Deflate(border);

          // Get the anchor point
          ComputeBackgroundAnchorPoint(aColor, firstRootElementFrameArea, paddingArea, tileWidth, tileHeight, anchor);
        } else {
          ComputeBackgroundAnchorPoint(aColor, paddingArea, paddingArea, tileWidth, tileHeight, anchor);
        }
      } else {
        // Otherwise, it is the normal case, and the background is
        // simply placed relative to the frame's padding area
        ComputeBackgroundAnchorPoint(aColor, paddingArea, paddingArea, tileWidth, tileHeight, anchor);
      }
    }


#if !defined(XP_UNIX) && !defined(XP_BEOS)
    // Setup clipping so that rendering doesn't leak out of the computed
    // dirty rect
    PRBool clipState;
    aRenderingContext.PushState();
    aRenderingContext.SetClipRect(dirtyRect, nsClipCombine_kIntersect,
                                  clipState);
#endif

    // Compute the x and y starting points and limits for tiling

    /* An Overview Of The Following Logic
                     
          A........ . . . . . . . . . . . . . .
          :   +---:-------.-------.-------.----  /|\
          :   |   :       .       .       .       |  nh 
          :.......: . . . x . . . . . . . . . .  \|/   
          .   |   .       .       .       .        
          .   |   .       .  ###########  .        
          . . . . . . . . . .#. . . . .#. . . .     
          .   |   .       .  ###########  .      /|\
          .   |   .       .       .       .       |  h
          . . | . . . . . . . . . . . . . z . .  \|/
          .   |   .       .       .       .    
          |<-----nw------>|       |<--w-->|

       ---- = the paddingArea edge. The padding is done relative to this
              area. Outside the padding is the border. If the background
              is positioned relative to the viewport ('fixed') then this
              is the viewport edge.

       .... = the primary tile.

       . .  = the other tiles.

       #### = the dirtyRect. This is the minimum region we want to cover.

          A = The anchor point. This is the point at which the tile should
              start. Always negative or zero.

          x = x0 and y0 in the code. The point at which tiling must start
              so that the fewest tiles are laid out while completly
              covering the dirtyRect area.

          z = x1 and y1 in the code. The point at which tiling must end so
              that the fewest tiles are laid out while completly covering
              the dirtyRect area.

          w = the width of the tile (tileWidth).

          h = the height of the tile (tileHeight).

          n = the number of whole tiles that fit between 'A' and 'x'.
              (the vertical n and the horizontal n are different)


       Therefore, 

          x0 = paddingArea.x + anchor.x + n * tileWidth;

       ...where n is an integer greater or equal to 0 fitting:

          n * tileWidth <= 
                      dirtyRect.x - (paddingArea.x + anchor.x) <= 
                                                             (n+1) * tileWidth

       ...i.e.,

          n <= (dirtyRect.x - (paddingArea.x + anchor.x)) / tileWidth < n + 1

       ...which, treating the division as an integer divide rounding down, gives:

          n = (dirtyRect.x - (paddingArea.x + anchor.x)) / tileWidth

       Substituting into the original expression for x0:

          x0 = paddingArea.x + anchor.x +
               ((dirtyRect.x - (paddingArea.x + anchor.x)) / tileWidth) *
               tileWidth;

       From this x1 is determined,

          x1 = x0 + m * tileWidth;

       ...where m is an integer greater than 0 fitting:

          (m - 1) * tileWidth <
                            dirtyRect.x + dirtyRect.width - x0 <=
                                                               m * tileWidth

       ...i.e.,

          m - 1 < (dirtyRect.x + dirtyRect.width - x0) / tileWidth <= m

       ...which, treating the division as an integer divide, and making it
          round up, gives:

          m = (dirtyRect.x + dirtyRect.width - x0 + tileWidth - 1) / tileWidth

       Substituting into the original expression for x1:

          x1 = x0 + ((dirtyRect.x + dirtyRect.width - x0 + tileWidth - 1) /
                     tileWidth) * tileWidth

       The vertical case is analogous. If the background is fixed, then 
       paddingArea.x and paddingArea.y are set to zero when finding the parent
       viewport, above.

    */

    // first do the horizontal case
    nscoord x0, x1;
    if (repeat & NS_STYLE_BG_REPEAT_X) {
      // When tiling in the x direction, adjust the starting position of the
      // tile to account for dirtyRect.x. When tiling in x, the anchor.x value
      // will be a negative value used to adjust the starting coordinate.
      x0 = paddingArea.x + anchor.x + ((dirtyRect.x - (paddingArea.x + anchor.x)) / tileWidth) * tileWidth;
      x1 = x0 + ((dirtyRect.x + dirtyRect.width - x0 + tileWidth - 1) / tileWidth) * tileWidth;
    }
    else {
      // For scrolling attachment, the anchor is relative to the padding area.
      // For fixed attachment, paddingArea.x is set to zero and the anchor is 
      // relative to the nearest scrolling ancestor (or the viewport).
      x0 = paddingArea.x + anchor.x;
      x1 = x0 + tileWidth;
    }

    // now do all that again with the vertical case
    nscoord y0, y1;
    if (repeat & NS_STYLE_BG_REPEAT_Y) {
      // When tiling in the y direction, adjust the starting position of the
      // tile to account for dirtyRect.y. When tiling in y, the anchor.y value
      // will be a negative value used to adjust the starting coordinate.
      y0 = paddingArea.y + anchor.y + ((dirtyRect.y - (paddingArea.y + anchor.y)) / tileHeight) * tileHeight;
      y1 = y0 + ((dirtyRect.y + dirtyRect.height - y0 + tileHeight - 1) / tileHeight) * tileHeight;
    }
    else {
      // For scrolling attachment, the anchor is relative to the padding area.
      // For fixed attachment, paddingArea.y is set to zero and the anchor is 
      // relative to the nearest scrolling ancestor (or the viewport).
      y0 = paddingArea.y + anchor.y;
      y1 = y0 + tileHeight;
    }

    // Take the intersection again to paint only the required area
    nsRect tileRect(x0,y0,(x1-x0),(y1-y0));
#ifdef XP_WIN
    PRInt32 xOffset = tileRect.x - x0,
            yOffset = tileRect.y - y0;
    aRenderingContext.DrawTile(image,xOffset,yOffset,&tileRect);
#else
    nsRect drawRect;

    if (drawRect.IntersectRect(tileRect, dirtyRect)) {
      PRInt32 xOffset = drawRect.x - x0,
              yOffset = drawRect.y - y0;
      aRenderingContext.DrawTile(image,xOffset,yOffset,&drawRect);
    }
#endif

#if !defined(XP_UNIX) && !defined(XP_BEOS)
    // Restore clipping
    aRenderingContext.PopState(clipState);
#endif

  }

}

/** ---------------------------------------------------
 *  A bit blitter to tile images to the background recursively
 *	@update 4/13/99 dwc
 *  @param aRC -- Rendering Context to render to
 *  @param aDS -- Target drawing surface for the rendering context
 *  @param aSrcRect -- Rectangle we are build with the image
 *  @param aHeight -- height of the tile
 *  @param aWidth -- width of the tile
 */
static void
TileImage(nsIRenderingContext& aRC,nsDrawingSurface  aDS,nsRect &aSrcRect,PRInt16 aWidth,PRInt16 aHeight)
{
nsRect  destRect;
PRInt32 flag = NS_COPYBITS_TO_BACK_BUFFER | NS_COPYBITS_XFORM_DEST_VALUES;
  
  if( aSrcRect.width < aWidth) {
    // width is less than double so double our source bitmap width
    destRect = aSrcRect;
    destRect.x += aSrcRect.width;
    aRC.CopyOffScreenBits(aDS,aSrcRect.x,aSrcRect.y,destRect,flag);
    aSrcRect.width*=2; 
    TileImage(aRC,aDS,aSrcRect,aWidth,aHeight);
  } else if (aSrcRect.height < aHeight) {
    // height is less than double so double our source bitmap height
    destRect = aSrcRect;
    destRect.y += aSrcRect.height;
    aRC.CopyOffScreenBits(aDS,aSrcRect.x,aSrcRect.y,destRect,flag);
    aSrcRect.height*=2;
    TileImage(aRC,aDS,aSrcRect,aWidth,aHeight);
  } 
}

/** ---------------------------------------------------
 *  See documentation in nsCSSRendering.h
 *	@update 3/26/99 dwc
 */
void
nsCSSRendering::PaintRoundedBackground(nsIPresContext* aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                nsIFrame* aForFrame,
                                const nsRect& aDirtyRect,
                                const nsRect& aBorderArea,
                                const nsStyleBackground& aColor,
                                nscoord aDX,
                                nscoord aDY,
                                PRInt16 aTheRadius[4])
{
RoundedRect   outerPath;
QBCurve       cr1,cr2,cr3,cr4;
QBCurve       UL,UR,LL,LR;
PRInt32       curIndex,c1Index;
nsFloatPoint  thePath[MAXPATHSIZE];
static nsPoint       polyPath[MAXPOLYPATHSIZE];
PRInt16       np;
nscoord       twipsPerPixel;
float         p2t;

  // needed for our border thickness
  aPresContext->GetPixelsToTwips(&p2t);
  twipsPerPixel = NSToCoordRound(p2t);

  aRenderingContext.SetColor(aColor.mBackgroundColor);

  // set the rounded rect up, and let'er rip
  outerPath.Set(aBorderArea.x,aBorderArea.y,aBorderArea.width,aBorderArea.height,aTheRadius,twipsPerPixel);
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

  polyPath[0].x = NSToCoordRound(thePath[0].x);
  polyPath[0].y = NSToCoordRound(thePath[0].y);
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
nsCSSRendering::PaintRoundedBorder(nsIPresContext* aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 nsIFrame* aForFrame,
                                 const nsRect& aDirtyRect,
                                 const nsRect& aBorderArea,
                                 const nsStyleBorder* aBorderStyle,
                                 const nsStyleOutline* aOutlineStyle,
                                 nsIStyleContext* aStyleContext,
                                 PRIntn aSkipSides,
                                 PRInt16 aBorderRadius[4],
                                 nsRect* aGap,
                                 PRBool aIsOutline)
{
  RoundedRect   outerPath;
  QBCurve       UL,LL,UR,LR;
  QBCurve       IUL,ILL,IUR,ILR;
  QBCurve       cr1,cr2,cr3,cr4;
  QBCurve       Icr1,Icr2,Icr3,Icr4;
  nsFloatPoint  thePath[MAXPATHSIZE];
  PRInt16       np;
  nsMargin      border;
  nscoord       twipsPerPixel,qtwips;
  float         p2t;

  NS_ASSERTION((aIsOutline && aOutlineStyle) || (!aIsOutline && aBorderStyle), "null params not allowed");
  if (!aIsOutline) {
    aBorderStyle->CalcBorderFor(aForFrame, border);
    if ((0 == border.left) && (0 == border.right) &&
        (0 == border.top) && (0 == border.bottom)) {
      return;
    }
  } else {
    nscoord width;
    if (!aOutlineStyle->GetOutlineWidth(width)) {
      return;
    }
    border.left   = width;
    border.right  = width;
    border.top    = width;
    border.bottom = width;
  }

  // needed for our border thickness
  aPresContext->GetPixelsToTwips(&p2t);
  twipsPerPixel = NSToCoordRound(p2t);

  // Base our thickness check on the segment being less than a pixel and 1/2
  qtwips = twipsPerPixel >> 2;
  //qtwips = twipsPerPixel;

  outerPath.Set(aBorderArea.x,aBorderArea.y,aBorderArea.width,aBorderArea.height,aBorderRadius,twipsPerPixel);
  outerPath.GetRoundedBorders(UL,UR,LL,LR);
  outerPath.CalcInsetCurves(IUL,IUR,ILL,ILR,border);

  // TOP LINE -- construct and divide the curves first, then put together our top and bottom paths
  UL.MidPointDivide(&cr1,&cr2);
  UR.MidPointDivide(&cr3,&cr4);
  IUL.MidPointDivide(&Icr1,&Icr2);
  IUR.MidPointDivide(&Icr3,&Icr4);
  if(0!=border.top){
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
    RenderSide(thePath,aRenderingContext,aBorderStyle,aOutlineStyle,aStyleContext,NS_SIDE_TOP,border,qtwips, aIsOutline);
  }
  // RIGHT  LINE ----------------------------------------------------------------
  LR.MidPointDivide(&cr2,&cr3);
  ILR.MidPointDivide(&Icr2,&Icr3);
  if(0!=border.right){
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
    RenderSide(thePath,aRenderingContext,aBorderStyle,aOutlineStyle,aStyleContext,NS_SIDE_RIGHT,border,qtwips, aIsOutline);
  }

  // bottom line ----------------------------------------------------------------
  LL.MidPointDivide(&cr2,&cr4);
  ILL.MidPointDivide(&Icr2,&Icr4);
  if(0!=border.bottom){
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
    RenderSide(thePath,aRenderingContext,aBorderStyle,aOutlineStyle,aStyleContext,NS_SIDE_BOTTOM,border,qtwips, aIsOutline);
  }
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

  RenderSide(thePath,aRenderingContext,aBorderStyle,aOutlineStyle,aStyleContext,NS_SIDE_LEFT,border,qtwips, aIsOutline);
}


/** ---------------------------------------------------
 *  See documentation in nsCSSRendering.h
 *	@update 3/26/99 dwc
 */
void 
nsCSSRendering::RenderSide(nsFloatPoint aPoints[],nsIRenderingContext& aRenderingContext,
                        const nsStyleBorder* aBorderStyle,const nsStyleOutline* aOutlineStyle,nsIStyleContext* aStyleContext,
                        PRUint8 aSide,nsMargin  &aBorThick,nscoord aTwipsPerPixel,
                        PRBool aIsOutline)
{
  QBCurve   thecurve;
  nscolor   sideColor = NS_RGB(0,0,0);
  static nsPoint   polypath[MAXPOLYPATHSIZE];
  PRInt32   curIndex,c1Index,c2Index,junk;
  PRInt8    border_Style;
  PRInt16   thickness;

  // Get our style context's color struct.
  const nsStyleColor* ourColor = (const nsStyleColor*)aStyleContext->GetStyleData(eStyleStruct_Color);

  NS_ASSERTION((aIsOutline && aOutlineStyle) || (!aIsOutline && aBorderStyle), "null params not allowed");
  // set the style information
  if (!aIsOutline) {
    GetBorderColor(ourColor, *aBorderStyle, aSide, sideColor);
  } else {
    aOutlineStyle->GetOutlineColor(sideColor);
  }
  aRenderingContext.SetColor ( sideColor );

  thickness = 0;
  switch(aSide){
    case  NS_SIDE_LEFT:
      thickness = aBorThick.left;
      break;
    case  NS_SIDE_TOP:
      thickness = aBorThick.top;
      break;
    case  NS_SIDE_RIGHT:
      thickness = aBorThick.right;
      break;
    case  NS_SIDE_BOTTOM:
      thickness = aBorThick.bottom;
      break;
  }

  // if the border is thin, just draw it 
  if (thickness<=aTwipsPerPixel) {
    // NOTHING FANCY JUST DRAW OUR OUTSIDE BORDER
    thecurve.SetPoints(aPoints[0].x,aPoints[0].y,aPoints[1].x,aPoints[1].y,aPoints[2].x,aPoints[2].y);
    thecurve.SubDivide((nsIRenderingContext*)&aRenderingContext,0,0);
    aRenderingContext.DrawLine((nscoord)aPoints[2].x,(nscoord)aPoints[2].y,(nscoord)aPoints[3].x,(nscoord)aPoints[3].y);
    thecurve.SetPoints(aPoints[3].x,aPoints[3].y,aPoints[4].x,aPoints[4].y,aPoints[5].x,aPoints[5].y);
    thecurve.SubDivide((nsIRenderingContext*)&aRenderingContext,0,0);
  } else {
    
    if (!aIsOutline) {
      border_Style = aBorderStyle->GetBorderStyle(aSide);
    } else {
      border_Style = aOutlineStyle->GetOutlineStyle();
    }
    switch (border_Style){
      case NS_STYLE_BORDER_STYLE_OUTSET:
      case NS_STYLE_BORDER_STYLE_INSET:
        {
        const nsStyleBackground* bgColor = nsStyleUtil::FindNonTransparentBackground(aStyleContext);
        aRenderingContext.SetColor ( MakeBevelColor (aSide, border_Style, bgColor->mBackgroundColor,sideColor, PR_TRUE));
        }
      case NS_STYLE_BORDER_STYLE_DOTTED:
      case NS_STYLE_BORDER_STYLE_DASHED:
        // break;   This is here until dotted and dashed are supported.  It is ok to have
        // dotted and dashed render in solid until this style is supported.  This code should
        // be moved when it is supported so that the above outset and inset will fall into the 
        // solid code below....
      case NS_STYLE_BORDER_STYLE_SOLID:
        polypath[0].x = NSToCoordRound(aPoints[0].x);
        polypath[0].y = NSToCoordRound(aPoints[0].y);
        curIndex = 1;
        GetPath(aPoints,polypath,&curIndex,eOutside,c1Index);
        c2Index = curIndex;
        polypath[curIndex].x = NSToCoordRound(aPoints[6].x);
        polypath[curIndex].y = NSToCoordRound(aPoints[6].y);
        curIndex++;
        GetPath(aPoints,polypath,&curIndex,eInside,junk);
        polypath[curIndex].x = NSToCoordRound(aPoints[0].x);
        polypath[curIndex].y = NSToCoordRound(aPoints[0].y);
        curIndex++;
        aRenderingContext.FillPolygon(polypath,curIndex);

       break;
      case NS_STYLE_BORDER_STYLE_DOUBLE:
        polypath[0].x = NSToCoordRound(aPoints[0].x);
        polypath[0].y = NSToCoordRound(aPoints[0].y);
        curIndex = 1;
        GetPath(aPoints,polypath,&curIndex,eOutside,c1Index);
        aRenderingContext.DrawPolyline(polypath,curIndex);
        polypath[0].x = NSToCoordRound(aPoints[6].x);
        polypath[0].y = NSToCoordRound(aPoints[6].y);
        curIndex = 1;
        GetPath(aPoints,polypath,&curIndex,eInside,c1Index);
        aRenderingContext.DrawPolyline(polypath,curIndex);
        break;
      case NS_STYLE_BORDER_STYLE_NONE:
      case NS_STYLE_BORDER_STYLE_HIDDEN:
      case NS_STYLE_BORDER_STYLE_BLANK:
        break;
      case NS_STYLE_BORDER_STYLE_RIDGE:
      case NS_STYLE_BORDER_STYLE_GROOVE:
        {
        const nsStyleBackground* bgColor = nsStyleUtil::FindNonTransparentBackground(aStyleContext);
        aRenderingContext.SetColor ( MakeBevelColor (aSide, border_Style, bgColor->mBackgroundColor,sideColor, PR_TRUE));

        polypath[0].x = NSToCoordRound(aPoints[0].x);
        polypath[0].y = NSToCoordRound(aPoints[0].y);
        curIndex = 1;
        GetPath(aPoints,polypath,&curIndex,eOutside,c1Index);
        polypath[curIndex].x = NSToCoordRound((aPoints[5].x + aPoints[6].x)/2.0f);
        polypath[curIndex].y = NSToCoordRound((aPoints[5].y + aPoints[6].y)/2.0f);
        curIndex++;
        GetPath(aPoints,polypath,&curIndex,eCalcRev,c1Index,.5);
        polypath[curIndex].x = NSToCoordRound(aPoints[0].x);
        polypath[curIndex].y = NSToCoordRound(aPoints[0].y);
        curIndex++;
        aRenderingContext.FillPolygon(polypath,curIndex);

        aRenderingContext.SetColor ( MakeBevelColor (aSide, 
                                                ((border_Style == NS_STYLE_BORDER_STYLE_RIDGE) ?
                                                NS_STYLE_BORDER_STYLE_GROOVE :
                                                NS_STYLE_BORDER_STYLE_RIDGE), 
                                                bgColor->mBackgroundColor,sideColor, PR_TRUE));
       
        polypath[0].x = NSToCoordRound((aPoints[0].x + aPoints[11].x)/2.0f);
        polypath[0].y = NSToCoordRound((aPoints[0].y + aPoints[11].y)/2.0f);
        curIndex = 1;
        GetPath(aPoints,polypath,&curIndex,eCalc,c1Index,.5);
        polypath[curIndex].x = NSToCoordRound(aPoints[6].x) ;
        polypath[curIndex].y = NSToCoordRound(aPoints[6].y);
        curIndex++;
        GetPath(aPoints,polypath,&curIndex,eInside,c1Index);
        polypath[curIndex].x = NSToCoordRound(aPoints[0].x);
        polypath[curIndex].y = NSToCoordRound(aPoints[0].y);
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
 *  See documentation in nsCSSRendering.h
 *	@update 3/26/99 dwc
 */
void 
RoundedRect::CalcInsetCurves(QBCurve &aULCurve,QBCurve &aURCurve,QBCurve &aLLCurve,QBCurve &aLRCurve,nsMargin &aBorder)
{
PRInt32   nLeft,nTop,nRight,nBottom;
PRInt32   tLeft,bLeft,tRight,bRight,lTop,rTop,lBottom,rBottom;
PRInt16   adjust=0;

  if(mDoRound)
    adjust = mRoundness[0]>>3;

  nLeft = mLeft + aBorder.left;
  tLeft = mLeft + mRoundness[0];
  bLeft = mLeft + mRoundness[3];

  if(tLeft < nLeft){
    tLeft = nLeft;
  }

  if(bLeft < nLeft){
    bLeft = nLeft;
  }

  nRight = mRight - aBorder.right;
  tRight = mRight - mRoundness[1];
  bRight = mRight - mRoundness[2];

  if(tRight > nRight){
    tRight = nRight;
  }

  if(bRight > nRight){
    bRight = nRight;
  }

  nTop = mTop + aBorder.top;
  lTop = mTop + mRoundness[0];
  rTop = mTop + mRoundness[1];

  if(lTop < nTop){
    lTop = nTop;
  }

  if(rTop < nTop){
    rTop = nTop;
  }

  nBottom = mBottom - aBorder.bottom;
  lBottom = mBottom - mRoundness[3];
  rBottom = mBottom - mRoundness[2];

  if(lBottom > nBottom){
    lBottom = nBottom;
  }

  if(rBottom > nBottom){
    rBottom = nBottom;
  }


  // set the passed in curves to the rounded borders of the rectangle
  aULCurve.SetPoints( (float)nLeft,(float)lTop,
                      (float)nLeft+adjust,(float)nTop+adjust,
                      (float)tLeft,(float)nTop);
  aURCurve.SetPoints( (float)tRight,(float)nTop,
                      (float)nRight-adjust,(float)nTop+adjust,
                      (float)nRight,(float)rTop);
  aLRCurve.SetPoints( (float)nRight,(float)rBottom,
                      (float)nRight-adjust,(float)nBottom-adjust,
                      (float)bRight,(float)nBottom);
  aLLCurve.SetPoints( (float)bLeft,(float)nBottom,
                      (float)nLeft+adjust,(float)nBottom-adjust,
                      (float)nLeft,(float)lBottom);

}

/** ---------------------------------------------------
 *  See documentation in nsCSSRendering.h
 *	@update 4/13/99 dwc
 */
void 
RoundedRect::Set(nscoord aLeft,nscoord aTop,PRInt32  aWidth,PRInt32 aHeight,PRInt16 aRadius[4],PRInt16 aNumTwipPerPix)
{
  nscoord x,y,width,height;
  int     i;

  // convert this rect to pixel boundaries
  x = (aLeft/aNumTwipPerPix)*aNumTwipPerPix;
  y = (aTop/aNumTwipPerPix)*aNumTwipPerPix;
  width = (aWidth/aNumTwipPerPix)*aNumTwipPerPix;
  height = (aHeight/aNumTwipPerPix)*aNumTwipPerPix;


  for(i=0;i<4;i++) {
    if( (aRadius[i]) > (aWidth>>1) ){
      mRoundness[i] = (aWidth>>1); 
    } else {
      mRoundness[i] = aRadius[i];
    }

    if( mRoundness[i] > (aHeight>>1) )
      mRoundness[i] = aHeight>>1;
  }


  // if we are drawing a circle
  mDoRound = PR_FALSE;
  if(aHeight==aWidth){
    PRBool doRound = PR_TRUE;
    for(i=0;i<4;i++){
      if(mRoundness[i]<(aWidth>>1)){
        doRound = PR_FALSE;
        break;
      }
    }

    if(doRound){
      mDoRound = PR_TRUE;
      for(i=0;i<4;i++){
        mRoundness[i] = aWidth>>1;
      }
    }
  }



  // important coordinates that the path hits
  mLeft = x;
  mTop = y;
  mRight = x+width;
  mBottom = y+height;

}

/** ---------------------------------------------------
 *  See documentation in nsCSSRendering.h
 *	@update 4/13/99 dwc
 */
void 
RoundedRect::GetRoundedBorders(QBCurve &aULCurve,QBCurve &aURCurve,QBCurve &aLLCurve,QBCurve &aLRCurve)
{

  PRInt16 adjust=0;

  if(mDoRound)
    adjust = mRoundness[0]>>3;

  // set the passed in curves to the rounded borders of the rectangle
  aULCurve.SetPoints( (float)mLeft,(float)mTop + mRoundness[0],
                      (float)mLeft+adjust,(float)mTop+adjust,
                      (float)mLeft+mRoundness[0],(float)mTop);
  aURCurve.SetPoints( (float)mRight - mRoundness[1],(float)mTop,
                      (float)mRight-adjust,(float)mTop+adjust,
                      (float)mRight,(float)mTop + mRoundness[1]);
  aLRCurve.SetPoints( (float)mRight,(float)mBottom - mRoundness[2],
                      (float)mRight-adjust,(float)mBottom-adjust,
                      (float)mRight - mRoundness[2],(float)mBottom);
  aLLCurve.SetPoints( (float)mLeft + mRoundness[3],(float)mBottom,
                      (float)mLeft+adjust,(float)mBottom-adjust,
                      (float)mLeft,(float)mBottom - mRoundness[3]);
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
GetPath(nsFloatPoint aPoints[],nsPoint aPolyPath[],PRInt32 *aCurIndex,ePathTypes  aPathType,PRInt32 &aC1Index,float aFrac)
{
  QBCurve thecurve;

  switch (aPathType) {
    case eOutside:
      thecurve.SetPoints(aPoints[0].x,aPoints[0].y,aPoints[1].x,aPoints[1].y,aPoints[2].x,aPoints[2].y);
      thecurve.SubDivide(nsnull,aPolyPath,aCurIndex);
      aC1Index = *aCurIndex;
      aPolyPath[*aCurIndex].x = (nscoord)aPoints[3].x;
      aPolyPath[*aCurIndex].y = (nscoord)aPoints[3].y;
      (*aCurIndex)++;
      thecurve.SetPoints(aPoints[3].x,aPoints[3].y,aPoints[4].x,aPoints[4].y,aPoints[5].x,aPoints[5].y);
      thecurve.SubDivide(nsnull,aPolyPath,aCurIndex);
      break;
    case eInside:
      thecurve.SetPoints(aPoints[6].x,aPoints[6].y,aPoints[7].x,aPoints[7].y,aPoints[8].x,aPoints[8].y);
      thecurve.SubDivide(nsnull,aPolyPath,aCurIndex);
      aPolyPath[*aCurIndex].x = (nscoord)aPoints[9].x;
      aPolyPath[*aCurIndex].y = (nscoord)aPoints[9].y;
      (*aCurIndex)++;
      thecurve.SetPoints(aPoints[9].x,aPoints[9].y,aPoints[10].x,aPoints[10].y,aPoints[11].x,aPoints[11].y);
      thecurve.SubDivide(nsnull,aPolyPath,aCurIndex);
     break;
    case eCalc:
      thecurve.SetPoints( (aPoints[0].x+aPoints[11].x)/2.0f,(aPoints[0].y+aPoints[11].y)/2.0f,
                          (aPoints[1].x+aPoints[10].x)/2.0f,(aPoints[1].y+aPoints[10].y)/2.0f,
                          (aPoints[2].x+aPoints[9].x)/2.0f,(aPoints[2].y+aPoints[9].y)/2.0f);
      thecurve.SubDivide(nsnull,aPolyPath,aCurIndex);
      aPolyPath[*aCurIndex].x = (nscoord)((aPoints[3].x+aPoints[8].x)/2.0f);
      aPolyPath[*aCurIndex].y = (nscoord)((aPoints[3].y+aPoints[8].y)/2.0f);
      (*aCurIndex)++;
      thecurve.SetPoints( (aPoints[3].x+aPoints[8].x)/2.0f,(aPoints[3].y+aPoints[8].y)/2.0f,
                          (aPoints[4].x+aPoints[7].x)/2.0f,(aPoints[4].y+aPoints[7].y)/2.0f,
                          (aPoints[5].x+aPoints[6].x)/2.0f,(aPoints[5].y+aPoints[6].y)/2.0f);
      thecurve.SubDivide(nsnull,aPolyPath,aCurIndex);
      break;
    case eCalcRev:
      thecurve.SetPoints( (aPoints[5].x+aPoints[6].x)/2.0f,(aPoints[5].y+aPoints[6].y)/2.0f,
                          (aPoints[4].x+aPoints[7].x)/2.0f,(aPoints[4].y+aPoints[7].y)/2.0f,
                          (aPoints[3].x+aPoints[8].x)/2.0f,(aPoints[3].y+aPoints[8].y)/2.0f);
      thecurve.SubDivide(nsnull,aPolyPath,aCurIndex);
      aPolyPath[*aCurIndex].x = (nscoord)((aPoints[2].x+aPoints[9].x)/2.0f);
      aPolyPath[*aCurIndex].y = (nscoord)((aPoints[2].y+aPoints[9].y)/2.0f);
      (*aCurIndex)++;
      thecurve.SetPoints( (aPoints[2].x+aPoints[9].x)/2.0f,(aPoints[2].y+aPoints[9].y)/2.0f,
                          (aPoints[1].x+aPoints[10].x)/2.0f,(aPoints[1].y+aPoints[10].y)/2.0f,
                          (aPoints[0].x+aPoints[11].x)/2.0f,(aPoints[0].y+aPoints[11].y)/2.0f);
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
QBCurve   curve1,curve2;
float     fx,fy,smag;

  // divide the curve into 2 pieces
	MidPointDivide(&curve1,&curve2);
	
	fx = (float)fabs(curve1.mAnc2.x - this->mCon.x);
	fy = (float)fabs(curve1.mAnc2.y - this->mCon.y);

	//smag = fx+fy-(PR_MIN(fx,fy)>>1);
  smag = fx*fx + fy*fy;
 
	if (smag>1){
		// split the curve again
    curve1.SubDivide(aRenderingContext,aPointArray,aCurIndex);
    curve2.SubDivide(aRenderingContext,aPointArray,aCurIndex);
	}else{
    if(aPointArray ) {
      // save the points for further processing
      aPointArray[*aCurIndex].x = (nscoord)curve1.mAnc2.x;
      aPointArray[*aCurIndex].y = (nscoord)curve1.mAnc2.y;
      (*aCurIndex)++;
      aPointArray[*aCurIndex].x = (nscoord)curve2.mAnc2.x;
      aPointArray[*aCurIndex].y = (nscoord)curve2.mAnc2.y;
      (*aCurIndex)++;
    }else{
		  // draw the curve 
      nsTransform2D *aTransform;
      aRenderingContext->GetCurrentTransform(aTransform);

      
      aRenderingContext->DrawLine((nscoord)curve1.mAnc1.x,(nscoord)curve1.mAnc1.y,(nscoord)curve1.mAnc2.x,(nscoord)curve1.mAnc2.y);
      aRenderingContext->DrawLine((nscoord)curve1.mAnc2.x,(nscoord)curve1.mAnc2.y,(nscoord)curve2.mAnc2.x,(nscoord)curve2.mAnc2.y);
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
float         c1x,c1y,c2x,c2y;
nsFloatPoint	a1;

  c1x = (mAnc1.x+mCon.x)/2.0f;
  c1y = (mAnc1.y+mCon.y)/2.0f;
  c2x = (mAnc2.x+mCon.x)/2.0f;
  c2y = (mAnc2.y+mCon.y)/2.0f;

  a1.x = (c1x + c2x)/2.0f;
	a1.y = (c1y + c2y)/2.0f;

  // put the math into our 2 new curves
  A->mAnc1 = this->mAnc1;
  A->mCon.x = c1x;
  A->mCon.y = c1y;
  A->mAnc2 = a1;
  B->mAnc1 = a1;
  B->mCon.x = c2x;
  B->mCon.y = c2y;
  B->mAnc2 = this->mAnc2;
}

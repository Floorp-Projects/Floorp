/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
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
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *   Takeshi Ichimaru <ayakawa.m@gmail.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation
 *   Michael Ventnor <m.ventnor@gmail.com>
 *   Rob Arnold <robarnold@mozilla.com>
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

/* utility functions for drawing borders and backgrounds */

#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIImage.h"
#include "nsIFrame.h"
#include "nsPoint.h"
#include "nsRect.h"
#include "nsIViewManager.h"
#include "nsIPresShell.h"
#include "nsFrameManager.h"
#include "nsStyleContext.h"
#include "nsGkAtoms.h"
#include "nsCSSAnonBoxes.h"
#include "nsTransform2D.h"
#include "nsIDeviceContext.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIScrollableFrame.h"
#include "imgIRequest.h"
#include "imgIContainer.h"
#include "gfxIImageFrame.h"
#include "nsCSSRendering.h"
#include "nsCSSColorUtils.h"
#include "nsITheme.h"
#include "nsThemeConstants.h"
#include "nsIServiceManager.h"
#include "nsIHTMLDocument.h"
#include "nsLayoutUtils.h"
#include "nsINameSpaceManager.h"
#include "nsBlockFrame.h"
#include "gfxContext.h"
#include "nsIInterfaceRequestorUtils.h"
#include "gfxPlatform.h"
#include "gfxImageSurface.h"
#include "nsStyleStructInlines.h"
#include "nsCSSFrameConstructor.h"

#include "nsCSSRenderingBorders.h"

// To avoid storing this data on nsInlineFrame (bloat) and to avoid
// recalculating this for each frame in a continuation (perf), hold
// a cache of various coordinate information that we need in order
// to paint inline backgrounds.
struct InlineBackgroundData
{
  InlineBackgroundData()
      : mFrame(nsnull), mBlockFrame(nsnull)
  {
  }

  ~InlineBackgroundData()
  {
  }

  void Reset()
  {
    mBoundingBox.SetRect(0,0,0,0);
    mContinuationPoint = mLineContinuationPoint = mUnbrokenWidth = 0;
    mFrame = mBlockFrame = nsnull;
  }

  nsRect GetContinuousRect(nsIFrame* aFrame)
  {
    SetFrame(aFrame);

    nscoord x;
    if (mBidiEnabled) {
      x = mLineContinuationPoint;

      // Scan continuations on the same line as aFrame and accumulate the widths
      // of frames that are to the left (if this is an LTR block) or right 
      // (if it's RTL) of the current one.
      PRBool isRtlBlock = (mBlockFrame->GetStyleVisibility()->mDirection ==
                           NS_STYLE_DIRECTION_RTL);      
      nscoord curOffset = aFrame->GetOffsetTo(mBlockFrame).x;

      nsIFrame* inlineFrame = aFrame->GetPrevContinuation();
      // If the continuation is fluid we know inlineFrame is not on the same line.
      // If it's not fluid, we need to test furhter to be sure.
      while (inlineFrame && !inlineFrame->GetNextInFlow() &&
             AreOnSameLine(aFrame, inlineFrame)) {
        nscoord frameXOffset = inlineFrame->GetOffsetTo(mBlockFrame).x;
        if(isRtlBlock == (frameXOffset >= curOffset)) {
          x += inlineFrame->GetSize().width;
        }
        inlineFrame = inlineFrame->GetPrevContinuation();
      }

      inlineFrame = aFrame->GetNextContinuation();
      while (inlineFrame && !inlineFrame->GetPrevInFlow() &&
             AreOnSameLine(aFrame, inlineFrame)) {
        nscoord frameXOffset = inlineFrame->GetOffsetTo(mBlockFrame).x;
        if(isRtlBlock == (frameXOffset >= curOffset)) {
          x += inlineFrame->GetSize().width;
        }
        inlineFrame = inlineFrame->GetNextContinuation();
      }
      if (isRtlBlock) {
        // aFrame itself is also to the right of its left edge, so add its width.
        x += aFrame->GetSize().width;
        // x is now the distance from the left edge of aFrame to the right edge
        // of the unbroken content. Change it to indicate the distance from the
        // left edge of the unbroken content to the left edge of aFrame.
        x = mUnbrokenWidth - x;
      }
    } else {
      x = mContinuationPoint;
    }

    // Assume background-origin: border and return a rect with offsets
    // relative to (0,0).  If we have a different background-origin,
    // then our rect should be deflated appropriately by our caller.
    return nsRect(-x, 0, mUnbrokenWidth, mFrame->GetSize().height);
  }

  nsRect GetBoundingRect(nsIFrame* aFrame)
  {
    SetFrame(aFrame);

    // Move the offsets relative to (0,0) which puts the bounding box into
    // our coordinate system rather than our parent's.  We do this by
    // moving it the back distance from us to the bounding box.
    // This also assumes background-origin: border, so our caller will
    // need to deflate us if needed.
    nsRect boundingBox(mBoundingBox);
    nsPoint point = mFrame->GetPosition();
    boundingBox.MoveBy(-point.x, -point.y);

    return boundingBox;
  }

protected:
  nsIFrame*     mFrame;
  nscoord       mContinuationPoint;
  nscoord       mUnbrokenWidth;
  nsRect        mBoundingBox;

  PRBool        mBidiEnabled;
  nsBlockFrame* mBlockFrame;
  nscoord       mLineContinuationPoint;
  
  void SetFrame(nsIFrame* aFrame)
  {
    NS_PRECONDITION(aFrame, "Need a frame");

    nsIFrame *prevContinuation = aFrame->GetPrevContinuation();

    if (!prevContinuation || mFrame != prevContinuation) {
      // Ok, we've got the wrong frame.  We have to start from scratch.
      Reset();
      Init(aFrame);
      return;
    }

    // Get our last frame's size and add its width to our continuation
    // point before we cache the new frame.
    mContinuationPoint += mFrame->GetSize().width;

    // If this a new line, update mLineContinuationPoint.
    if (mBidiEnabled &&
        (aFrame->GetPrevInFlow() || !AreOnSameLine(mFrame, aFrame))) {
       mLineContinuationPoint = mContinuationPoint;
    }
    
    mFrame = aFrame;
  }

  void Init(nsIFrame* aFrame)
  {    
    // Start with the previous flow frame as our continuation point
    // is the total of the widths of the previous frames.
    nsIFrame* inlineFrame = aFrame->GetPrevContinuation();

    while (inlineFrame) {
      nsRect rect = inlineFrame->GetRect();
      mContinuationPoint += rect.width;
      mUnbrokenWidth += rect.width;
      mBoundingBox.UnionRect(mBoundingBox, rect);
      inlineFrame = inlineFrame->GetPrevContinuation();
    }

    // Next add this frame and subsequent frames to the bounding box and
    // unbroken width.
    inlineFrame = aFrame;
    while (inlineFrame) {
      nsRect rect = inlineFrame->GetRect();
      mUnbrokenWidth += rect.width;
      mBoundingBox.UnionRect(mBoundingBox, rect);
      inlineFrame = inlineFrame->GetNextContinuation();
    }

    mFrame = aFrame;

    mBidiEnabled = aFrame->PresContext()->BidiEnabled();
    if (mBidiEnabled) {
      // Find the containing block frame
      nsIFrame* frame = aFrame;
      nsresult rv = NS_ERROR_FAILURE;
      while (frame &&
             frame->IsFrameOfType(nsIFrame::eLineParticipant) &&
             NS_FAILED(rv)) {
        frame = frame->GetParent();
        rv = frame->QueryInterface(kBlockFrameCID, (void**)&mBlockFrame);
      }
      NS_ASSERTION(NS_SUCCEEDED(rv) && mBlockFrame, "Cannot find containing block.");

      mLineContinuationPoint = mContinuationPoint;
    }
  }
  
  PRBool AreOnSameLine(nsIFrame* aFrame1, nsIFrame* aFrame2) {
    // Assumes that aFrame1 and aFrame2 are both decsendants of mBlockFrame.
    PRBool isValid1, isValid2;
    nsBlockInFlowLineIterator it1(mBlockFrame, aFrame1, &isValid1);
    nsBlockInFlowLineIterator it2(mBlockFrame, aFrame2, &isValid2);
    return isValid1 && isValid2 && it1.GetLine() == it2.GetLine();
  }
};

/* Local functions */
static void DrawBorderImage(nsPresContext* aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            nsIFrame* aForFrame,
                            const nsRect& aBorderArea,
                            const nsStyleBorder& aBorderStyle);

static void DrawBorderImageSide(gfxContext *aThebesContext,
                                nsIDeviceContext* aDeviceContext,
                                imgIContainer* aImage,
                                gfxRect& aDestRect,
                                gfxSize aInterSize,
                                gfxRect& aSourceRect,
                                PRUint8 aHFillType,
                                PRUint8 aVFillType);

static void PaintBackgroundColor(nsPresContext* aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 nsIFrame* aForFrame,
                                 const nsRect& aBgClipArea,
                                 const nsStyleBackground& aColor,
                                 const nsStyleBorder& aBorder,
                                 PRBool aCanPaintNonWhite);

static nscolor MakeBevelColor(PRIntn whichSide, PRUint8 style,
                              nscolor aBackgroundColor,
                              nscolor aBorderColor);

static gfxRect GetTextDecorationRectInternal(const gfxPoint& aPt,
                                             const gfxSize& aLineSize,
                                             const gfxFloat aAscent,
                                             const gfxFloat aOffset,
                                             const PRUint8 aDecoration,
                                             const PRUint8 aStyle);

/* Returns FALSE iff all returned aTwipsRadii == 0, TRUE otherwise */
static PRBool GetBorderRadiusTwips(const nsStyleCorners& aBorderRadius,
                                   const nscoord& aFrameWidth,
                                   nscoord aTwipsRadii[8]);

static InlineBackgroundData* gInlineBGData = nsnull;

// Initialize any static variables used by nsCSSRendering.
nsresult nsCSSRendering::Init()
{  
  NS_ASSERTION(!gInlineBGData, "Init called twice");
  gInlineBGData = new InlineBackgroundData();
  if (!gInlineBGData)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

// Clean up any global variables used by nsCSSRendering.
void nsCSSRendering::Shutdown()
{
  delete gInlineBGData;
  gInlineBGData = nsnull;
}

/**
 * Make a bevel color
 */
static nscolor
MakeBevelColor(PRIntn whichSide, PRUint8 style,
               nscolor aBackgroundColor, nscolor aBorderColor)
{

  nscolor colors[2];
  nscolor theColor;

  // Given a background color and a border color
  // calculate the color used for the shading
  NS_GetSpecial3DColors(colors, aBackgroundColor, aBorderColor);
 
  if ((style == NS_STYLE_BORDER_STYLE_OUTSET) ||
      (style == NS_STYLE_BORDER_STYLE_RIDGE)) {
    // Flip colors for these two border styles
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

//----------------------------------------------------------------------
// Thebes Border Rendering Code Start

// helper function to convert a nsRect to a gfxRect
static gfxRect
RectToGfxRect(const nsRect& rect, nscoord twipsPerPixel)
{
  return gfxRect(gfxFloat(rect.x) / twipsPerPixel,
                 gfxFloat(rect.y) / twipsPerPixel,
                 gfxFloat(rect.width) / twipsPerPixel,
                 gfxFloat(rect.height) / twipsPerPixel);
}

/*
 * Compute the float-pixel radii that should be used for drawing
 * this border/outline, given the various input bits.
 *
 * If a side is skipped via skipSides, its corners are forced to 0.
 * All corner radii are then adjusted so they do not require more
 * space than outerRect, according to the algorithm in css3-background.
 */
static void
ComputePixelRadii(const nscoord *aTwipsRadii,
                  const nsRect& outerRect,
                  PRIntn skipSides,
                  nscoord twipsPerPixel,
                  gfxCornerSizes *oBorderRadii)
{
  nscoord twipsRadii[8];
  memcpy(twipsRadii, aTwipsRadii, sizeof twipsRadii);

  if (skipSides & SIDE_BIT_TOP) {
    twipsRadii[NS_CORNER_TOP_LEFT_X] = 0;
    twipsRadii[NS_CORNER_TOP_LEFT_Y] = 0;
    twipsRadii[NS_CORNER_TOP_RIGHT_X] = 0;
    twipsRadii[NS_CORNER_TOP_RIGHT_Y] = 0;
  }

  if (skipSides & SIDE_BIT_RIGHT) {
    twipsRadii[NS_CORNER_TOP_RIGHT_X] = 0;
    twipsRadii[NS_CORNER_TOP_RIGHT_Y] = 0;
    twipsRadii[NS_CORNER_BOTTOM_RIGHT_X] = 0;
    twipsRadii[NS_CORNER_BOTTOM_RIGHT_Y] = 0;
  }

  if (skipSides & SIDE_BIT_BOTTOM) {
    twipsRadii[NS_CORNER_BOTTOM_RIGHT_X] = 0;
    twipsRadii[NS_CORNER_BOTTOM_RIGHT_Y] = 0;
    twipsRadii[NS_CORNER_BOTTOM_LEFT_X] = 0;
    twipsRadii[NS_CORNER_BOTTOM_LEFT_Y] = 0;
  }

  if (skipSides & SIDE_BIT_LEFT) {
    twipsRadii[NS_CORNER_BOTTOM_LEFT_X] = 0;
    twipsRadii[NS_CORNER_BOTTOM_LEFT_Y] = 0;
    twipsRadii[NS_CORNER_TOP_LEFT_X] = 0;
    twipsRadii[NS_CORNER_TOP_LEFT_Y] = 0;
  }

  gfxFloat radii[8];
  NS_FOR_CSS_HALF_CORNERS(corner)
    radii[corner] = twipsRadii[corner] / twipsPerPixel;

  // css3-background specifies this algorithm for reducing
  // corner radii when they are too big.
  gfxFloat maxWidth = outerRect.width / twipsPerPixel;
  gfxFloat maxHeight = outerRect.height / twipsPerPixel;
  gfxFloat f = 1.0f;
  NS_FOR_CSS_SIDES(side) {
    PRUint32 hc1 = NS_SIDE_TO_HALF_CORNER(side, PR_FALSE, PR_TRUE);
    PRUint32 hc2 = NS_SIDE_TO_HALF_CORNER(side, PR_TRUE, PR_TRUE);
    gfxFloat length = NS_SIDE_IS_VERTICAL(side) ? maxHeight : maxWidth;
    gfxFloat sum = radii[hc1] + radii[hc2];
    // avoid floating point division in the normal case
    if (length < sum)
      f = PR_MIN(f, length/sum);
  }
  if (f < 1.0) {
    NS_FOR_CSS_HALF_CORNERS(corner) {
      radii[corner] *= f;
    }
  }

  (*oBorderRadii)[C_TL] = gfxSize(radii[NS_CORNER_TOP_LEFT_X],
                                  radii[NS_CORNER_TOP_LEFT_Y]);
  (*oBorderRadii)[C_TR] = gfxSize(radii[NS_CORNER_TOP_RIGHT_X],
                                  radii[NS_CORNER_TOP_RIGHT_Y]);
  (*oBorderRadii)[C_BR] = gfxSize(radii[NS_CORNER_BOTTOM_RIGHT_X],
                                  radii[NS_CORNER_BOTTOM_RIGHT_Y]);
  (*oBorderRadii)[C_BL] = gfxSize(radii[NS_CORNER_BOTTOM_LEFT_X],
                                  radii[NS_CORNER_BOTTOM_LEFT_Y]);
}

void
nsCSSRendering::PaintBorder(nsPresContext* aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            nsIFrame* aForFrame,
                            const nsRect& aDirtyRect,
                            const nsRect& aBorderArea,
                            const nsStyleBorder& aBorderStyle,
                            nsStyleContext* aStyleContext,
                            PRIntn aSkipSides)
{
  nsMargin            border;
  nscoord             twipsRadii[8];
  nsCompatibility     compatMode = aPresContext->CompatibilityMode();

  SN("++ PaintBorder");

  // Check to see if we have an appearance defined.  If so, we let the theme
  // renderer draw the border.  DO not get the data from aForFrame, since the passed in style context
  // may be different!  Always use |aStyleContext|!
  const nsStyleDisplay* displayData = aStyleContext->GetStyleDisplay();
  if (displayData->mAppearance) {
    nsITheme *theme = aPresContext->GetTheme();
    if (theme && theme->ThemeSupportsWidget(aPresContext, aForFrame, displayData->mAppearance))
      return; // Let the theme handle it.
  }

  if (aBorderStyle.IsBorderImageLoaded()) {
    DrawBorderImage(aPresContext, aRenderingContext, aForFrame,
                    aBorderArea, aBorderStyle);
    return;
  }
  
  // Get our style context's color struct.
  const nsStyleColor* ourColor = aStyleContext->GetStyleColor();

  // in NavQuirks mode we want to use the parent's context as a starting point
  // for determining the background color
  const nsStyleBackground* bgColor = nsCSSRendering::FindNonTransparentBackground
    (aStyleContext, compatMode == eCompatibility_NavQuirks ? PR_TRUE : PR_FALSE);

  border = aBorderStyle.GetComputedBorder();
  if ((0 == border.left) && (0 == border.right) &&
      (0 == border.top) && (0 == border.bottom)) {
    // Empty border area
    return;
  }

  GetBorderRadiusTwips(aBorderStyle.mBorderRadius, aForFrame->GetSize().width,
                       twipsRadii);

  // Turn off rendering for all of the zero sized sides
  if (aSkipSides & SIDE_BIT_TOP) border.top = 0;
  if (aSkipSides & SIDE_BIT_RIGHT) border.right = 0;
  if (aSkipSides & SIDE_BIT_BOTTOM) border.bottom = 0;
  if (aSkipSides & SIDE_BIT_LEFT) border.left = 0;

  // get the inside and outside parts of the border
  nsRect outerRect(aBorderArea);

  SF(" outerRect: %d %d %d %d\n", outerRect.x, outerRect.y, outerRect.width, outerRect.height);

  // we can assume that we're already clipped to aDirtyRect -- I think? (!?)

  // Get our conversion values
  nscoord twipsPerPixel = aPresContext->DevPixelsToAppUnits(1);

  // convert outer and inner rects
  gfxRect oRect(RectToGfxRect(outerRect, twipsPerPixel));

  // convert the border widths
  gfxFloat borderWidths[4] = { border.top / twipsPerPixel,
                               border.right / twipsPerPixel,
                               border.bottom / twipsPerPixel,
                               border.left / twipsPerPixel };

  // convert the radii
  gfxCornerSizes borderRadii;
  ComputePixelRadii(twipsRadii, outerRect, aSkipSides, twipsPerPixel,
                    &borderRadii);

  PRUint8 borderStyles[4];
  nscolor borderColors[4];
  nsBorderColors *compositeColors[4];

  // pull out styles, colors, composite colors
  NS_FOR_CSS_SIDES (i) {
    PRBool foreground;
    borderStyles[i] = aBorderStyle.GetBorderStyle(i);
    aBorderStyle.GetBorderColor(i, borderColors[i], foreground);
    aBorderStyle.GetCompositeColors(i, &compositeColors[i]);

    if (foreground)
      borderColors[i] = ourColor->mColor;
  }

  SF(" borderStyles: %d %d %d %d\n", borderStyles[0], borderStyles[1], borderStyles[2], borderStyles[3]);

  // start drawing
  gfxContext *ctx = aRenderingContext.ThebesContext();

  ctx->Save();

#if 0
  // this will draw a transparent red backround underneath the oRect area
  ctx->Save();
  ctx->Rectangle(oRect);
  ctx->SetColor(gfxRGBA(1.0, 0.0, 0.0, 0.5));
  ctx->Fill();
  ctx->Restore();
#endif

  //SF ("borderRadii: %f %f %f %f\n", borderRadii[0], borderRadii[1], borderRadii[2], borderRadii[3]);

  nsCSSBorderRenderer br(twipsPerPixel,
                         ctx,
                         oRect,
                         borderStyles,
                         borderWidths,
                         borderRadii,
                         borderColors,
                         compositeColors,
                         aSkipSides,
                         bgColor->mBackgroundColor);
  br.DrawBorders();

  ctx->Restore();

  SN();
}

static nsRect
GetOutlineInnerRect(nsIFrame* aFrame)
{
  nsRect* savedOutlineInnerRect = static_cast<nsRect*>
    (aFrame->GetProperty(nsGkAtoms::outlineInnerRectProperty));
  if (savedOutlineInnerRect)
    return *savedOutlineInnerRect;
  return aFrame->GetOverflowRect();
}

void
nsCSSRendering::PaintOutline(nsPresContext* aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             nsIFrame* aForFrame,
                             const nsRect& aDirtyRect,
                             const nsRect& aBorderArea,
                             const nsStyleBorder& aBorderStyle,
                             const nsStyleOutline& aOutlineStyle,
                             nsStyleContext* aStyleContext)
{
  nscoord             twipsRadii[8];

  // Get our style context's color struct.
  const nsStyleColor* ourColor = aStyleContext->GetStyleColor();

  nscoord width;
  aOutlineStyle.GetOutlineWidth(width);

  if (width == 0) {
    // Empty outline
    return;
  }

  const nsStyleBackground* bgColor = nsCSSRendering::FindNonTransparentBackground
    (aStyleContext, PR_FALSE);

  // get the radius for our outline
  GetBorderRadiusTwips(aOutlineStyle.mOutlineRadius, aBorderArea.width,
                       twipsRadii);

  // When the outline property is set on :-moz-anonymous-block or
  // :-moz-anonyomus-positioned-block pseudo-elements, it inherited that
  // outline from the inline that was broken because it contained a
  // block.  In that case, we don't want a really wide outline if the
  // block inside the inline is narrow, so union the actual contents of
  // the anonymous blocks.
  nsIFrame *frameForArea = aForFrame;
  do {
    nsIAtom *pseudoType = frameForArea->GetStyleContext()->GetPseudoType();
    if (pseudoType != nsCSSAnonBoxes::mozAnonymousBlock &&
        pseudoType != nsCSSAnonBoxes::mozAnonymousPositionedBlock)
      break;
    // If we're done, we really want it and all its later siblings.
    frameForArea = frameForArea->GetFirstChild(nsnull);
    NS_ASSERTION(frameForArea, "anonymous block with no children?");
  } while (frameForArea);
  nsRect innerRect; // relative to aBorderArea.TopLeft()
  if (frameForArea == aForFrame) {
    innerRect = GetOutlineInnerRect(aForFrame);
  } else {
    for (; frameForArea; frameForArea = frameForArea->GetNextSibling()) {
      // The outline has already been included in aForFrame's overflow
      // area, but not in those of its descendants, so we have to
      // include it.  Otherwise we'll end up drawing the outline inside
      // the border.
      nsRect r(GetOutlineInnerRect(frameForArea) +
               frameForArea->GetOffsetTo(aForFrame));
      innerRect.UnionRect(innerRect, r);
    }
  }

  innerRect += aBorderArea.TopLeft();
  nscoord offset = aOutlineStyle.mOutlineOffset;
  innerRect.Inflate(offset, offset);
  // If the dirty rect is completely inside the border area (e.g., only the
  // content is being painted), then we can skip out now
  // XXX this isn't exactly true for rounded borders, where the inside curves may
  // encroach into the content area.  A safer calculation would be to
  // shorten insideRect by the radius one each side before performing this test.
  if (innerRect.Contains(aDirtyRect))
    return;

  nsRect outerRect = innerRect;
  outerRect.Inflate(width, width);

  // Get our conversion values
  nscoord twipsPerPixel = aPresContext->DevPixelsToAppUnits(1);

  // get the outer rectangles
  gfxRect oRect(RectToGfxRect(outerRect, twipsPerPixel));

  // convert the radii
  nsMargin outlineMargin(width, width, width, width);
  gfxCornerSizes outlineRadii;
  ComputePixelRadii(twipsRadii, outerRect, 0, twipsPerPixel,
                    &outlineRadii);

  PRUint8 outlineStyle = aOutlineStyle.GetOutlineStyle();
  PRUint8 outlineStyles[4] = { outlineStyle,
                               outlineStyle,
                               outlineStyle,
                               outlineStyle };

  nscolor outlineColor;
  // PR_FALSE means use the initial color; PR_TRUE means a color was
  // set.
  if (!aOutlineStyle.GetOutlineColor(outlineColor))
    outlineColor = ourColor->mColor;
  nscolor outlineColors[4] = { outlineColor,
                               outlineColor,
                               outlineColor,
                               outlineColor };

  // convert the border widths
  gfxFloat outlineWidths[4] = { width / twipsPerPixel,
                                width / twipsPerPixel,
                                width / twipsPerPixel,
                                width / twipsPerPixel };

  // start drawing
  gfxContext *ctx = aRenderingContext.ThebesContext();

  ctx->Save();

  nsCSSBorderRenderer br(twipsPerPixel,
                         ctx,
                         oRect,
                         outlineStyles,
                         outlineWidths,
                         outlineRadii,
                         outlineColors,
                         nsnull, 0,
                         bgColor->mBackgroundColor);
  br.DrawBorders();

  ctx->Restore();

  SN();
}

void
nsCSSRendering::PaintFocus(nsPresContext* aPresContext,
                           nsIRenderingContext& aRenderingContext,
                           const nsRect& aFocusRect,
                           nscolor aColor)
{
  nscoord oneCSSPixel = nsPresContext::CSSPixelsToAppUnits(1);
  nscoord oneDevPixel = aPresContext->DevPixelsToAppUnits(1);

  gfxRect focusRect(RectToGfxRect(aFocusRect, oneDevPixel));

  gfxCornerSizes focusRadii;
  {
    nscoord twipsRadii[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    ComputePixelRadii(twipsRadii, aFocusRect, 0, oneDevPixel, &focusRadii);
  }
  gfxFloat focusWidths[4] = { oneCSSPixel / oneDevPixel,
                              oneCSSPixel / oneDevPixel,
                              oneCSSPixel / oneDevPixel,
                              oneCSSPixel / oneDevPixel };

  PRUint8 focusStyles[4] = { NS_STYLE_BORDER_STYLE_DOTTED,
                             NS_STYLE_BORDER_STYLE_DOTTED,
                             NS_STYLE_BORDER_STYLE_DOTTED,
                             NS_STYLE_BORDER_STYLE_DOTTED };
  nscolor focusColors[4] = { aColor, aColor, aColor, aColor };

  gfxContext *ctx = aRenderingContext.ThebesContext();

  ctx->Save();

  // Because this renders a dotted border, the background color
  // should not be used.  Therefore, we provide a value that will
  // be blatantly wrong if it ever does get used.  (If this becomes
  // something that CSS can style, this function will then have access
  // to a style context and can use the same logic that PaintBorder
  // and PaintOutline do.)
  nsCSSBorderRenderer br(oneDevPixel,
                         ctx,
                         focusRect,
                         focusStyles,
                         focusWidths,
                         focusRadii,
                         focusColors,
                         nsnull, 0,
                         NS_RGB(255, 0, 0));
  br.DrawBorders();

  ctx->Restore();

  SN();
}

// Thebes Border Rendering Code End
//----------------------------------------------------------------------


//----------------------------------------------------------------------

/**
 * Computes the placement of a background image.
 *
 * @param aOriginBounds is the box to which the tiling position should be
 * relative
 * This should correspond to 'background-origin' for the frame,
 * except when painting on the canvas, in which case the origin bounds
 * should be the bounds of the root element's frame.
 * @param aTopLeft the top-left corner where an image tile should be drawn
 * @param aAnchorPoint a point which should be pixel-aligned by
 * nsLayoutUtils::DrawImage. This is the same as aTopLeft, unless CSS
 * specifies a percentage (including 'right' or 'bottom'), in which case
 * it's that percentage within of aOriginBounds. So 'right' would set
 * aAnchorPoint.x to aOriginBounds.XMost().
 * 
 * Points are returned relative to aOriginBounds.
 */
static void
ComputeBackgroundAnchorPoint(const nsStyleBackground& aColor,
                             const nsSize& aOriginBounds,
                             const nsSize& aImageSize,
                             nsPoint* aTopLeft,
                             nsPoint* aAnchorPoint)
{
  if (NS_STYLE_BG_X_POSITION_LENGTH & aColor.mBackgroundFlags) {
    aTopLeft->x = aAnchorPoint->x = aColor.mBackgroundXPosition.mCoord;
  }
  else if (NS_STYLE_BG_X_POSITION_PERCENT & aColor.mBackgroundFlags) {
    double percent = aColor.mBackgroundXPosition.mFloat;
    aAnchorPoint->x = NSToCoordRound(percent*aOriginBounds.width);
    aTopLeft->x = NSToCoordRound(percent*(aOriginBounds.width - aImageSize.width));
  }
  else {
    aTopLeft->x = aAnchorPoint->x = 0;
  }

  if (NS_STYLE_BG_Y_POSITION_LENGTH & aColor.mBackgroundFlags) {
    aTopLeft->y = aAnchorPoint->y = aColor.mBackgroundYPosition.mCoord;
  }
  else if (NS_STYLE_BG_Y_POSITION_PERCENT & aColor.mBackgroundFlags) {
    double percent = aColor.mBackgroundYPosition.mFloat;
    aAnchorPoint->y = NSToCoordRound(percent*aOriginBounds.height);
    aTopLeft->y = NSToCoordRound(percent*(aOriginBounds.height - aImageSize.height));
  }
  else {
    aTopLeft->y = aAnchorPoint->y = 0;
  }
}

const nsStyleBackground*
nsCSSRendering::FindNonTransparentBackground(nsStyleContext* aContext,
                                             PRBool aStartAtParent /*= PR_FALSE*/)
{
  NS_ASSERTION(aContext, "Cannot find NonTransparentBackground in a null context" );
  
  const nsStyleBackground* result = nsnull;
  nsStyleContext* context = nsnull;
  if (aStartAtParent) {
    context = aContext->GetParent();
  }
  if (!context) {
    context = aContext;
  }
  
  while (context) {
    result = context->GetStyleBackground();
    if (NS_GET_A(result->mBackgroundColor) > 0)
      break;

    context = context->GetParent();
  }
  return result;
}


/**
 * |FindBackground| finds the correct style data to use to paint the
 * background.  It is responsible for handling the following two
 * statements in section 14.2 of CSS2:
 *
 *   The background of the box generated by the root element covers the
 *   entire canvas.
 *
 *   For HTML documents, however, we recommend that authors specify the
 *   background for the BODY element rather than the HTML element. User
 *   agents should observe the following precedence rules to fill in the
 *   background: if the value of the 'background' property for the HTML
 *   element is different from 'transparent' then use it, else use the
 *   value of the 'background' property for the BODY element. If the
 *   resulting value is 'transparent', the rendering is undefined.
 *
 * Thus, in our implementation, it is responsible for ensuring that:
 *  + we paint the correct background on the |nsCanvasFrame|,
 *    |nsRootBoxFrame|, or |nsPageFrame|,
 *  + we don't paint the background on the root element, and
 *  + we don't paint the background on the BODY element in *some* cases,
 *    and for SGML-based HTML documents only.
 *
 * |FindBackground| returns true if a background should be painted, and
 * the resulting style context to use for the background information
 * will be filled in to |aBackground|.  It fills in a boolean indicating
 * whether the frame is the canvas frame to allow PaintBackground to
 * ensure that it always paints something non-transparent for the
 * canvas.
 */

// Returns true if aFrame is a canvas frame.
// We need to treat the viewport as canvas because, even though
// it does not actually paint a background, we need to get the right
// background style so we correctly detect transparent documents.
inline PRBool
IsCanvasFrame(nsIFrame *aFrame)
{
  nsIAtom* frameType = aFrame->GetType();
  return frameType == nsGkAtoms::canvasFrame ||
         frameType == nsGkAtoms::rootFrame ||
         frameType == nsGkAtoms::pageFrame ||
         frameType == nsGkAtoms::pageContentFrame ||
         frameType == nsGkAtoms::viewportFrame;
}

inline PRBool
FindCanvasBackground(nsIFrame* aForFrame, nsIFrame* aRootElementFrame,
                     const nsStyleBackground** aBackground)
{
  if (aRootElementFrame) {
    const nsStyleBackground* result = aRootElementFrame->GetStyleBackground();

    // Check if we need to do propagation from BODY rather than HTML.
    if (result->IsTransparent()) {
      nsIContent* content = aRootElementFrame->GetContent();
      // The root element content can't be null. We wouldn't know what
      // frame to create for aRootElementFrame.
      // Use |GetOwnerDoc| so it works during destruction.
      nsIDocument* document = content->GetOwnerDoc();
      nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(document);
      if (htmlDoc) {
        nsIContent* bodyContent = htmlDoc->GetBodyContentExternal();
        // We need to null check the body node (bug 118829) since
        // there are cases, thanks to the fix for bug 5569, where we
        // will reflow a document with no body.  In particular, if a
        // SCRIPT element in the head blocks the parser and then has a
        // SCRIPT that does "document.location.href = 'foo'", then
        // nsParser::Terminate will call |DidBuildModel| methods
        // through to the content sink, which will call |StartLayout|
        // and thus |InitialReflow| on the pres shell.  See bug 119351
        // for the ugly details.
        if (bodyContent) {
          nsIFrame *bodyFrame = aForFrame->PresContext()->GetPresShell()->
            GetPrimaryFrameFor(bodyContent);
          if (bodyFrame)
            result = bodyFrame->GetStyleBackground();
        }
      }
    }

    *aBackground = result;
  } else {
    // This should always give transparent, so we'll fill it in with the
    // default color if needed.  This seems to happen a bit while a page is
    // being loaded.
    *aBackground = aForFrame->GetStyleBackground();
  }
  
  return PR_TRUE;
}

inline PRBool
FindElementBackground(nsIFrame* aForFrame, nsIFrame* aRootElementFrame,
                      const nsStyleBackground** aBackground)
{
  if (aForFrame == aRootElementFrame) {
    // We must have propagated our background to the viewport or canvas. Abort.
    return PR_FALSE;
  }

  *aBackground = aForFrame->GetStyleBackground();

  // Return true unless the frame is for a BODY element whose background
  // was propagated to the viewport.

  nsIContent* content = aForFrame->GetContent();
  if (!content || content->Tag() != nsGkAtoms::body)
    return PR_TRUE; // not frame for a "body" element
  // It could be a non-HTML "body" element but that's OK, we'd fail the
  // bodyContent check below

  if (aForFrame->GetStyleContext()->GetPseudoType())
    return PR_TRUE; // A pseudo-element frame.

  // We should only look at the <html> background if we're in an HTML document
  nsIDocument* document = content->GetOwnerDoc();
  nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(document);
  if (!htmlDoc)
    return PR_TRUE;

  nsIContent* bodyContent = htmlDoc->GetBodyContentExternal();
  if (bodyContent != content)
    return PR_TRUE; // this wasn't the background that was propagated

  // This can be called even when there's no root element yet, during frame
  // construction, via nsLayoutUtils::FrameHasTransparency and
  // nsContainerFrame::SyncFrameViewProperties.
  if (!aRootElementFrame)
    return PR_TRUE;

  const nsStyleBackground* htmlBG = aRootElementFrame->GetStyleBackground();
  return !htmlBG->IsTransparent();
}

PRBool
nsCSSRendering::FindBackground(nsPresContext* aPresContext,
                               nsIFrame* aForFrame,
                               const nsStyleBackground** aBackground,
                               PRBool* aIsCanvas)
{
  nsIFrame* rootElementFrame =
    aPresContext->PresShell()->FrameConstructor()->GetRootElementStyleFrame();
  PRBool isCanvasFrame = IsCanvasFrame(aForFrame);
  *aIsCanvas = isCanvasFrame;
  return isCanvasFrame
      ? FindCanvasBackground(aForFrame, rootElementFrame, aBackground)
      : FindElementBackground(aForFrame, rootElementFrame, aBackground);
}

void
nsCSSRendering::DidPaint()
{
  gInlineBGData->Reset();
}

static PRBool
GetBorderRadiusTwips(const nsStyleCorners& aBorderRadius,
                     const nscoord& aFrameWidth, nscoord aTwipsRadii[8])
{
  PRBool result = PR_FALSE;
  
  // Convert percentage values
  NS_FOR_CSS_HALF_CORNERS(i) {
    const nsStyleCoord c = aBorderRadius.Get(i);

    switch (c.GetUnit()) {
      case eStyleUnit_Percent:
        aTwipsRadii[i] = (nscoord)(c.GetPercentValue() * aFrameWidth);
        break;

      case eStyleUnit_Coord:
        aTwipsRadii[i] = c.GetCoordValue();
        break;

      default:
        NS_NOTREACHED("GetBorderRadiusTwips: bad unit");
        aTwipsRadii[i] = 0;
        break;
    }

    if (aTwipsRadii[i])
      result = PR_TRUE;
  }
  return result;
}

void
nsCSSRendering::PaintBoxShadow(nsPresContext* aPresContext,
                               nsIRenderingContext& aRenderingContext,
                               nsIFrame* aForFrame,
                               const nsPoint& aForFramePt)
{
  nsMargin      borderValues;
  PRIntn        sidesToSkip;
  nsRect        frameRect;

  const nsStyleBorder* styleBorder = aForFrame->GetStyleBorder();
  borderValues = styleBorder->GetActualBorder();
  sidesToSkip = aForFrame->GetSkipSides();
  frameRect = nsRect(aForFramePt, aForFrame->GetSize());

  // Get any border radius, since box-shadow must also have rounded corners if the frame does
  nscoord twipsRadii[8];
  PRBool hasBorderRadius = GetBorderRadiusTwips(styleBorder->mBorderRadius,
                                                frameRect.width, twipsRadii);
  nscoord twipsPerPixel = aPresContext->DevPixelsToAppUnits(1);

  gfxCornerSizes borderRadii;
  ComputePixelRadii(twipsRadii, frameRect, sidesToSkip,
                    twipsPerPixel, &borderRadii);

  gfxRect frameGfxRect = RectToGfxRect(frameRect, twipsPerPixel);
  for (PRUint32 i = styleBorder->mBoxShadow->Length(); i > 0; --i) {
    nsCSSShadowItem* shadowItem = styleBorder->mBoxShadow->ShadowAt(i - 1);
    gfxRect shadowRect(frameRect.x, frameRect.y, frameRect.width, frameRect.height);
    shadowRect.MoveBy(gfxPoint(shadowItem->mXOffset, shadowItem->mYOffset));
    shadowRect.Outset(shadowItem->mSpread);

    gfxRect shadowRectPlusBlur = shadowRect;
    shadowRect.ScaleInverse(twipsPerPixel);
    shadowRect.RoundOut();

    // shadowRect won't include the blur, so make an extra rect here that includes the blur
    // for use in the even-odd rule below.
    nscoord blurRadius = shadowItem->mRadius;
    shadowRectPlusBlur.Outset(blurRadius);
    shadowRectPlusBlur.ScaleInverse(twipsPerPixel);
    shadowRectPlusBlur.RoundOut();

    gfxContext* renderContext = aRenderingContext.ThebesContext();
    nsRefPtr<gfxContext> shadowContext;
    nsContextBoxBlur blurringArea;

    // shadowRect has already been converted to device pixels, pass 1 as the appunits/pixel value
    blurRadius /= twipsPerPixel;
    shadowContext = blurringArea.Init(shadowRect, blurRadius, 1, renderContext);
    if (!shadowContext)
      continue;

    // Set the shadow color; if not specified, use the foreground color
    nscolor shadowColor;
    if (shadowItem->mHasColor)
      shadowColor = shadowItem->mColor;
    else
      shadowColor = aForFrame->GetStyleColor()->mColor;

    renderContext->Save();
    renderContext->SetColor(gfxRGBA(shadowColor));

    // Clip out the area of the actual frame so the shadow is not shown within
    // the frame
    renderContext->NewPath();
    renderContext->Rectangle(shadowRectPlusBlur);
    if (hasBorderRadius)
      renderContext->RoundedRectangle(frameGfxRect, borderRadii);
    else
      renderContext->Rectangle(frameGfxRect);
    renderContext->SetFillRule(gfxContext::FILL_RULE_EVEN_ODD);
    renderContext->Clip();

    // Draw the shape of the frame so it can be blurred. Recall how nsContextBoxBlur
    // doesn't make any temporary surfaces if blur is 0 and it just returns the original
    // surface? If we have no blur, we're painting this fill on the actual content surface
    // (renderContext == shadowContext) which is why we set up the color and clip
    // before doing this.
    shadowContext->NewPath();
    if (hasBorderRadius)
      shadowContext->RoundedRectangle(shadowRect, borderRadii);
    else
      shadowContext->Rectangle(shadowRect);
    shadowContext->Fill();

    blurringArea.DoPaint();
    renderContext->Restore();
  }
}

void
nsCSSRendering::PaintBackground(nsPresContext* aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                nsIFrame* aForFrame,
                                const nsRect& aDirtyRect,
                                const nsRect& aBorderArea,
                                PRBool aUsePrintSettings,
                                nsRect* aBGClipRect)
{
  NS_PRECONDITION(aForFrame,
                  "Frame is expected to be provided to PaintBackground");

  PRBool isCanvas;
  const nsStyleBackground *color;
  const nsStyleBorder* border = aForFrame->GetStyleBorder();

  if (!FindBackground(aPresContext, aForFrame, &color, &isCanvas)) {
    // we don't want to bail out of moz-appearance is set on a root
    // node. If it has a parent content node, bail because it's not
    // a root, other wise keep going in order to let the theme stuff
    // draw the background. The canvas really should be drawing the
    // bg, but there's no way to hook that up via css.
    if (!aForFrame->GetStyleDisplay()->mAppearance) {
      return;
    }

    nsIContent* content = aForFrame->GetContent();
    if (!content || content->GetParent()) {
      return;
    }
        
    color = aForFrame->GetStyleBackground();
  }
  if (!isCanvas) {
    PaintBackgroundWithSC(aPresContext, aRenderingContext, aForFrame,
                          aDirtyRect, aBorderArea, *color, *border,
                          aUsePrintSettings, aBGClipRect);
    return;
  }

  nsStyleBackground canvasColor(*color);

  nsIViewManager* vm = aPresContext->GetViewManager();

  if (NS_GET_A(canvasColor.mBackgroundColor) < 255) {
    // If the window is intended to be opaque, ensure that we always
    // paint an opaque color for its root element, in case there's no
    // background at all or a partly transparent image.
    nsIView* rView;
    vm->GetRootView(rView);
    if (!rView->GetParent() &&
        (!rView->HasWidget() ||
         rView->GetWidget()->GetTransparencyMode() == eTransparencyOpaque)) {
      nscolor backColor = aPresContext->DefaultBackgroundColor();
      NS_ASSERTION(NS_GET_A(backColor) == 255,
                   "default background color is not opaque");

      canvasColor.mBackgroundColor =
        NS_ComposeColors(backColor, canvasColor.mBackgroundColor);
    }
  }

  vm->SetDefaultBackgroundColor(canvasColor.mBackgroundColor);

  PaintBackgroundWithSC(aPresContext, aRenderingContext, aForFrame,
                        aDirtyRect, aBorderArea, canvasColor,
                        *border, aUsePrintSettings, aBGClipRect);
}

static PRBool
IsSolidBorderEdge(const nsStyleBorder& aBorder, PRUint32 aSide)
{
  if (aBorder.GetActualBorder().side(aSide) == 0)
    return PR_TRUE;
  if (aBorder.GetBorderStyle(aSide) != NS_STYLE_BORDER_STYLE_SOLID)
    return PR_FALSE;

  // If we're using a border image, assume it's not fully opaque,
  // because we may not even have the image loaded at this point, and
  // even if we did, checking whether the relevant tile is fully
  // opaque would be too much work.
  if (aBorder.GetBorderImage())
    return PR_FALSE;

  nscolor color;
  PRBool isForeground;
  aBorder.GetBorderColor(aSide, color, isForeground);

  // We don't know the foreground color here, so if it's being used
  // we must assume it might be transparent.
  if (isForeground)
    return PR_FALSE;

  return NS_GET_A(color) == 255;
}

/**
 * Returns true if all border edges are either missing or opaque.
 */
static PRBool
IsSolidBorder(const nsStyleBorder& aBorder)
{
  if (aBorder.mBorderColors ||
      nsLayoutUtils::HasNonZeroCorner(aBorder.mBorderRadius))
    return PR_FALSE;
  for (PRUint32 i = 0; i < 4; ++i) {
    if (!IsSolidBorderEdge(aBorder, i))
      return PR_FALSE;
  }
  return PR_TRUE;
}

void
nsCSSRendering::PaintBackgroundWithSC(nsPresContext* aPresContext,
                                      nsIRenderingContext& aRenderingContext,
                                      nsIFrame* aForFrame,
                                      const nsRect& aDirtyRect,
                                      const nsRect& aBorderArea,
                                      const nsStyleBackground& aColor,
                                      const nsStyleBorder& aBorder,
                                      PRBool aUsePrintSettings,
                                      nsRect* aBGClipRect)
{
  NS_PRECONDITION(aForFrame,
                  "Frame is expected to be provided to PaintBackground");

  PRBool canDrawBackgroundImage = PR_TRUE;
  PRBool canDrawBackgroundColor = PR_TRUE;

  if (aUsePrintSettings) {
    canDrawBackgroundImage = aPresContext->GetBackgroundImageDraw();
    canDrawBackgroundColor = aPresContext->GetBackgroundColorDraw();
  }

  // Check to see if we have an appearance defined.  If so, we let the theme
  // renderer draw the background and bail out.
  const nsStyleDisplay* displayData = aForFrame->GetStyleDisplay();
  if (displayData->mAppearance) {
    nsITheme *theme = aPresContext->GetTheme();
    if (theme && theme->ThemeSupportsWidget(aPresContext, aForFrame, displayData->mAppearance)) {
      nsRect dirty;
      dirty.IntersectRect(aDirtyRect, aBorderArea);
      theme->DrawWidgetBackground(&aRenderingContext, aForFrame, 
                                  displayData->mAppearance, aBorderArea, dirty);
      return;
    }
  }

  // Same coordinate space as aBorderArea
  nsRect bgClipArea;
  if (aBGClipRect) {
    bgClipArea = *aBGClipRect;
  }
  else {
    // The background is rendered over the 'background-clip' area.
    bgClipArea = aBorderArea;
    // If the border is solid, then clip the background to the padding-box
    // so that we don't draw unnecessary tiles.
    if (aColor.mBackgroundClip != NS_STYLE_BG_CLIP_BORDER ||
        IsSolidBorder(aBorder)) {
      nsMargin border = aForFrame->GetUsedBorder();
      aForFrame->ApplySkipSides(border);
      bgClipArea.Deflate(border);
    }
  }

  gfxContext *ctx = aRenderingContext.ThebesContext();

  // The actual dirty rect is the intersection of the 'background-clip'
  // area and the dirty rect we were given
  nsRect dirtyRect;
  if (!dirtyRect.IntersectRect(bgClipArea, aDirtyRect)) {
    // Nothing to paint
    return;
  }

  // if there is no background image or background images are turned off, try a color.
  if (!aColor.mBackgroundImage || !canDrawBackgroundImage) {
    PaintBackgroundColor(aPresContext, aRenderingContext, aForFrame, bgClipArea,
                         aColor, aBorder, canDrawBackgroundColor);
    return;
  }

  // We have a background image

  // Lookup the image
  imgIRequest *req = aPresContext->LoadImage(aColor.mBackgroundImage,
                                             aForFrame);

  PRUint32 status = imgIRequest::STATUS_ERROR;
  if (req)
    req->GetImageStatus(&status);

  if (!req || !(status & imgIRequest::STATUS_FRAME_COMPLETE) || !(status & imgIRequest::STATUS_SIZE_AVAILABLE)) {
    PaintBackgroundColor(aPresContext, aRenderingContext, aForFrame, bgClipArea,
                         aColor, aBorder, canDrawBackgroundColor);
    return;
  }

  nsCOMPtr<imgIContainer> image;
  req->GetImage(getter_AddRefs(image));

  nsSize imageSize;
  image->GetWidth(&imageSize.width);
  image->GetHeight(&imageSize.height);

  imageSize.width = nsPresContext::CSSPixelsToAppUnits(imageSize.width);
  imageSize.height = nsPresContext::CSSPixelsToAppUnits(imageSize.height);

  req = nsnull;

  // relative to aBorderArea
  nsRect bgOriginRect;

  nsIAtom* frameType = aForFrame->GetType();
  nsIFrame* geometryFrame = aForFrame;
  if (frameType == nsGkAtoms::inlineFrame ||
      frameType == nsGkAtoms::positionedInlineFrame) {
    switch (aColor.mBackgroundInlinePolicy) {
    case NS_STYLE_BG_INLINE_POLICY_EACH_BOX:
      bgOriginRect = nsRect(nsPoint(0,0), aBorderArea.Size());
      break;
    case NS_STYLE_BG_INLINE_POLICY_BOUNDING_BOX:
      bgOriginRect = gInlineBGData->GetBoundingRect(aForFrame);
      break;
    default:
      NS_ERROR("Unknown background-inline-policy value!  "
               "Please, teach me what to do.");
    case NS_STYLE_BG_INLINE_POLICY_CONTINUOUS:
      bgOriginRect = gInlineBGData->GetContinuousRect(aForFrame);
      break;
    }
  } else if (frameType == nsGkAtoms::canvasFrame) {
    geometryFrame = aForFrame->GetFirstChild(nsnull);
    NS_ASSERTION(geometryFrame, "A canvas with a background "
      "image had no child frame, which is impossible according to CSS. "
      "Make sure there isn't a background image specified on the "
      "|:viewport| pseudo-element in |html.css|.");
    bgOriginRect = geometryFrame->GetRect();
  } else {
    bgOriginRect = nsRect(nsPoint(0,0), aBorderArea.Size());
  }

  // Background images are tiled over the 'background-clip' area
  // but the origin of the tiling is based on the 'background-origin' area
  if (aColor.mBackgroundOrigin != NS_STYLE_BG_ORIGIN_BORDER) {
    nsMargin border = geometryFrame->GetUsedBorder();
    geometryFrame->ApplySkipSides(border);
    bgOriginRect.Deflate(border);
    if (aColor.mBackgroundOrigin != NS_STYLE_BG_ORIGIN_PADDING) {
      nsMargin padding = geometryFrame->GetUsedPadding();
      geometryFrame->ApplySkipSides(padding);
      bgOriginRect.Deflate(padding);
      NS_ASSERTION(aColor.mBackgroundOrigin == NS_STYLE_BG_ORIGIN_CONTENT,
                   "unknown background-origin value");
    }
  }

  PRBool  needBackgroundColor = NS_GET_A(aColor.mBackgroundColor) > 0;
  PRIntn  repeat = aColor.mBackgroundRepeat;

  switch (repeat) {
    case NS_STYLE_BG_REPEAT_X:
      break;
    case NS_STYLE_BG_REPEAT_Y:
      break;
    case NS_STYLE_BG_REPEAT_XY:
      if (needBackgroundColor) {
        // If the image is completely opaque, we do not need to paint the
        // background color
        nsCOMPtr<gfxIImageFrame> gfxImgFrame;
        image->GetCurrentFrame(getter_AddRefs(gfxImgFrame));
        if (gfxImgFrame) {
          gfxImgFrame->GetNeedsBackground(&needBackgroundColor);

          /* check for tiling of a image where frame smaller than container */
          nsSize iSize;
          image->GetWidth(&iSize.width);
          image->GetHeight(&iSize.height);
          nsRect iframeRect;
          gfxImgFrame->GetRect(iframeRect);
          if (iSize.width != iframeRect.width ||
              iSize.height != iframeRect.height) {
            needBackgroundColor = PR_TRUE;
          }
        }
      }
      break;
    case NS_STYLE_BG_REPEAT_OFF:
    default:
      NS_ASSERTION(repeat == NS_STYLE_BG_REPEAT_OFF, "unknown background-repeat value");
      break;
  }

  // The background color is rendered over the 'background-clip' area
  if (needBackgroundColor) {
    PaintBackgroundColor(aPresContext, aRenderingContext, aForFrame, bgClipArea,
                         aColor, aBorder, canDrawBackgroundColor);
  }

  // Compute the anchor point.
  //
  // relative to aBorderArea.TopLeft() (which is where the top-left
  // of aForFrame's border-box will be rendered)
  nsPoint imageTopLeft, anchor;
  if (NS_STYLE_BG_ATTACHMENT_FIXED == aColor.mBackgroundAttachment) {
    // If it's a fixed background attachment, then the image is placed
    // relative to the viewport, which is the area of the root frame
    // in a screen context or the page content frame in a print context.

    // Remember that we've drawn position-varying content in this prescontext
    aPresContext->SetRenderedPositionVaryingContent();

    nsIFrame* topFrame =
      aPresContext->PresShell()->FrameManager()->GetRootFrame();
    NS_ASSERTION(topFrame, "no root frame");
    nsIFrame* pageContentFrame = nsnull;
    if (aPresContext->IsPaginated()) {
      pageContentFrame =
        nsLayoutUtils::GetClosestFrameOfType(aForFrame, nsGkAtoms::pageContentFrame);
      if (pageContentFrame) {
        topFrame = pageContentFrame;
      }
      // else this is an embedded shell and its root frame is what we want
    }

    nsRect viewportArea(nsPoint(0, 0), topFrame->GetSize());

    if (!pageContentFrame) {
      // Subtract the size of scrollbars.
      nsIScrollableFrame* scrollableFrame =
        aPresContext->PresShell()->GetRootScrollFrameAsScrollable();
      if (scrollableFrame) {
        nsMargin scrollbars = scrollableFrame->GetActualScrollbarSizes();
        viewportArea.Deflate(scrollbars);
      }
    }
     
    // Get the anchor point, relative to the viewport.
    ComputeBackgroundAnchorPoint(aColor, viewportArea.Size(), imageSize,
                                 &imageTopLeft, &anchor);

    // Convert the anchor point from viewport coordinates to aForFrame
    // coordinates.
    nsPoint offset = viewportArea.TopLeft() - aForFrame->GetOffsetTo(topFrame);
    imageTopLeft += offset;
    anchor += offset;
  } else {
    ComputeBackgroundAnchorPoint(aColor, bgOriginRect.Size(), imageSize,
                                 &imageTopLeft, &anchor);
    imageTopLeft += bgOriginRect.TopLeft();
    anchor += bgOriginRect.TopLeft();
  }

  ctx->Save();

  nscoord borderRadii[8];
  PRBool haveRadius = GetBorderRadiusTwips(aBorder.mBorderRadius,
                                           aForFrame->GetSize().width,
                                           borderRadii);
  if (haveRadius) {
    nscoord appUnitsPerPixel = aPresContext->DevPixelsToAppUnits(1);
    gfxCornerSizes radii;
    ComputePixelRadii(borderRadii, bgClipArea,
                      aForFrame ? aForFrame->GetSkipSides() : 0,
                      appUnitsPerPixel, &radii);

    gfxRect oRect(RectToGfxRect(bgClipArea, appUnitsPerPixel));
    oRect.Round();
    oRect.Condition();

    ctx->NewPath();
    ctx->RoundedRectangle(oRect, radii);
    ctx->Clip();
  }

  nsRect destArea(imageTopLeft + aBorderArea.TopLeft(), imageSize);
  nsRect fillArea = destArea;
  if (repeat & NS_STYLE_BG_REPEAT_X) {
    fillArea.x = bgClipArea.x;
    fillArea.width = bgClipArea.width;
  }
  if (repeat & NS_STYLE_BG_REPEAT_Y) {
    fillArea.y = bgClipArea.y;
    fillArea.height = bgClipArea.height;
  }
  fillArea.IntersectRect(fillArea, bgClipArea);

  nsLayoutUtils::DrawImage(&aRenderingContext, image,
      destArea, fillArea, anchor + aBorderArea.TopLeft(), dirtyRect);

  ctx->Restore();
}

static void
DrawBorderImage(nsPresContext* aPresContext,
                nsIRenderingContext& aRenderingContext,
                nsIFrame* aForFrame, const nsRect& aBorderArea,
                const nsStyleBorder& aBorderStyle)
{
    float percent;
    nsStyleCoord borderImageSplit[4];
    PRInt32 borderImageSplitInt[4];
    nsMargin border;
    gfxFloat borderTop, borderRight, borderBottom, borderLeft;
    gfxFloat borderImageSplitGfx[4];

    border = aBorderStyle.GetActualBorder();
    if ((0 == border.left) && (0 == border.right) &&
        (0 == border.top) && (0 == border.bottom)) {
      // Empty border area
      return;
    }

    borderImageSplit[NS_SIDE_TOP] = aBorderStyle.mBorderImageSplit.GetTop();
    borderImageSplit[NS_SIDE_RIGHT] = aBorderStyle.mBorderImageSplit.GetRight();
    borderImageSplit[NS_SIDE_BOTTOM] = aBorderStyle.mBorderImageSplit.GetBottom();
    borderImageSplit[NS_SIDE_LEFT] = aBorderStyle.mBorderImageSplit.GetLeft();

    imgIRequest *req = aPresContext->LoadBorderImage(aBorderStyle.GetBorderImage(), aForFrame);

    nsCOMPtr<imgIContainer> image;
    req->GetImage(getter_AddRefs(image));

    nsSize imageSize;
    image->GetWidth(&imageSize.width);
    image->GetHeight(&imageSize.height);
    imageSize.width = nsPresContext::CSSPixelsToAppUnits(imageSize.width);
    imageSize.height = nsPresContext::CSSPixelsToAppUnits(imageSize.height);

    // convert percentage values
    NS_FOR_CSS_SIDES(side) {
      borderImageSplitInt[side] = 0;
      switch (borderImageSplit[side].GetUnit()) {
        case eStyleUnit_Percent:
          percent = borderImageSplit[side].GetPercentValue();
          if (side == NS_SIDE_TOP || side == NS_SIDE_BOTTOM)
            borderImageSplitInt[side] = (nscoord)(percent * imageSize.height);
          else
            borderImageSplitInt[side] = (nscoord)(percent * imageSize.width);
          break;
        case eStyleUnit_Integer:
          borderImageSplitInt[side] = nsPresContext::CSSPixelsToAppUnits(borderImageSplit[side].
                                          GetIntValue());
          break;
        case eStyleUnit_Factor:
          borderImageSplitInt[side] = nsPresContext::CSSPixelsToAppUnits(borderImageSplit[side].GetFactorValue());
          break;
        default:
          break;
      }
    }

    gfxContext *thebesCtx = aRenderingContext.ThebesContext();
    nsCOMPtr<nsIDeviceContext> dc;
    aRenderingContext.GetDeviceContext(*getter_AddRefs(dc));

    NS_FOR_CSS_SIDES(side) {
      borderImageSplitGfx[side] = nsPresContext::AppUnitsToFloatCSSPixels(borderImageSplitInt[side]);
    }

    borderTop = dc->AppUnitsToGfxUnits(border.top);
    borderRight = dc->AppUnitsToGfxUnits(border.right);
    borderBottom = dc->AppUnitsToGfxUnits(border.bottom);
    borderLeft = dc->AppUnitsToGfxUnits(border.left);

    gfxSize gfxImageSize;
    gfxImageSize.width = nsPresContext::AppUnitsToFloatCSSPixels(imageSize.width);
    gfxImageSize.height = nsPresContext::AppUnitsToFloatCSSPixels(imageSize.height);

    nsRect outerRect(aBorderArea);
    gfxRect rectToDraw,
            rectToDrawSource;

    gfxRect clipRect;
    clipRect.pos.x = dc->AppUnitsToGfxUnits(outerRect.x);
    clipRect.pos.y = dc->AppUnitsToGfxUnits(outerRect.y);
    clipRect.size.width = dc->AppUnitsToGfxUnits(outerRect.width);
    clipRect.size.height = dc->AppUnitsToGfxUnits(outerRect.height);
    if (thebesCtx->UserToDevicePixelSnapped(clipRect))
      clipRect = thebesCtx->DeviceToUser(clipRect);

    thebesCtx->Save();
    thebesCtx->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);

    gfxSize middleSize(clipRect.size.width - (borderLeft + borderRight),
                       clipRect.size.height - (borderTop + borderBottom));

    // middle size in source space
    gfxIntSize middleSizeSource(gfxImageSize.width - (borderImageSplitGfx[NS_SIDE_RIGHT] + borderImageSplitGfx[NS_SIDE_LEFT]), 
                                gfxImageSize.height - (borderImageSplitGfx[NS_SIDE_TOP] + borderImageSplitGfx[NS_SIDE_BOTTOM]));

    gfxSize interSizeTop, interSizeBottom, interSizeLeft, interSizeRight,
            interSizeMiddle;
    gfxFloat topScale = borderTop/borderImageSplitGfx[NS_SIDE_TOP];
    gfxFloat bottomScale = borderBottom/borderImageSplitGfx[NS_SIDE_BOTTOM];
    gfxFloat leftScale = borderLeft/borderImageSplitGfx[NS_SIDE_LEFT];
    gfxFloat rightScale = borderRight/borderImageSplitGfx[NS_SIDE_RIGHT];
    gfxFloat middleScaleH,
             middleScaleV;
    // TODO: check for nan and properly check for inf
    if (topScale != 0.0 && borderImageSplitGfx[NS_SIDE_TOP] != 0.0) {
      middleScaleH = topScale;
    } else if (bottomScale != 0.0 && borderImageSplitGfx[NS_SIDE_BOTTOM] != 0.0) {
      middleScaleH = bottomScale;
    } else {
      middleScaleH = 1.0;
    }

    if (leftScale != 0.0 && borderImageSplitGfx[NS_SIDE_LEFT] != 0.0) {
      middleScaleV = leftScale;
    } else if (rightScale != 0.0 && borderImageSplitGfx[NS_SIDE_RIGHT] != 0.0) {
      middleScaleV = rightScale;
    } else {
      middleScaleV = 1.0;
    }

    interSizeTop.height = borderTop;
    interSizeTop.width = middleSizeSource.width*topScale;

    interSizeBottom.height = borderBottom;
    interSizeBottom.width = middleSizeSource.width*bottomScale;

    interSizeLeft.width = borderLeft;
    interSizeLeft.height = middleSizeSource.height*leftScale;

    interSizeRight.width = borderRight;
    interSizeRight.height = middleSizeSource.height*rightScale;

    interSizeMiddle.width = middleSizeSource.width*middleScaleH;
    interSizeMiddle.height = middleSizeSource.height*middleScaleV;

    // draw top left corner
    rectToDraw = clipRect;
    rectToDraw.size.width = borderLeft;
    rectToDraw.size.height = borderTop;
    rectToDrawSource.pos.x = 0;
    rectToDrawSource.pos.y = 0;
    rectToDrawSource.size.width = borderImageSplitGfx[NS_SIDE_LEFT];
    rectToDrawSource.size.height = borderImageSplitGfx[NS_SIDE_TOP];
    DrawBorderImageSide(thebesCtx, dc, image,
                        rectToDraw, rectToDraw.size, rectToDrawSource,
                        NS_STYLE_BORDER_IMAGE_STRETCH, NS_STYLE_BORDER_IMAGE_STRETCH);

    // draw top
    rectToDraw = clipRect;
    rectToDraw.pos.x += borderLeft;
    rectToDraw.size.width = middleSize.width;
    rectToDraw.size.height = borderTop;
    rectToDrawSource.pos.x = borderImageSplitGfx[NS_SIDE_LEFT];
    rectToDrawSource.pos.y = 0;
    rectToDrawSource.size.width = middleSizeSource.width;
    rectToDrawSource.size.height = borderImageSplitGfx[NS_SIDE_TOP];
    DrawBorderImageSide(thebesCtx, dc, image,
                        rectToDraw, interSizeTop, rectToDrawSource, 
                        aBorderStyle.mBorderImageHFill, NS_STYLE_BORDER_IMAGE_STRETCH);
    
    // draw top right corner
    rectToDraw = clipRect;
    rectToDraw.pos.x += clipRect.size.width - borderRight;
    rectToDraw.size.width = borderRight;
    rectToDraw.size.height = borderTop;
    rectToDrawSource.pos.x = gfxImageSize.width - borderImageSplitGfx[NS_SIDE_RIGHT];
    rectToDrawSource.pos.y = 0;
    rectToDrawSource.size.width = borderImageSplitGfx[NS_SIDE_RIGHT];
    rectToDrawSource.size.height = borderImageSplitGfx[NS_SIDE_TOP];
    DrawBorderImageSide(thebesCtx, dc, image,
                        rectToDraw, rectToDraw.size, rectToDrawSource,
                        NS_STYLE_BORDER_IMAGE_STRETCH, NS_STYLE_BORDER_IMAGE_STRETCH);
    
    // draw right
    rectToDraw = clipRect;
    rectToDraw.pos.x += clipRect.size.width - borderRight;
    rectToDraw.pos.y += borderTop;
    rectToDraw.size.width = borderRight;
    rectToDraw.size.height = middleSize.height;
    rectToDrawSource.pos.x = gfxImageSize.width - borderImageSplitGfx[NS_SIDE_RIGHT];
    rectToDrawSource.pos.y = borderImageSplitGfx[NS_SIDE_TOP];
    rectToDrawSource.size.width = borderImageSplitGfx[NS_SIDE_RIGHT];
    rectToDrawSource.size.height = middleSizeSource.height;
    DrawBorderImageSide(thebesCtx, dc, image,
                        rectToDraw, interSizeRight, rectToDrawSource, 
                        NS_STYLE_BORDER_IMAGE_STRETCH, aBorderStyle.mBorderImageVFill);
    
    // draw bottom right corner
    rectToDraw = clipRect;
    rectToDraw.pos.x += clipRect.size.width - borderRight;
    rectToDraw.pos.y += clipRect.size.height - borderBottom;
    rectToDraw.size.width = borderRight;
    rectToDraw.size.height = borderBottom;
    rectToDrawSource.pos.x = gfxImageSize.width - borderImageSplitGfx[NS_SIDE_RIGHT];
    rectToDrawSource.pos.y = gfxImageSize.height - borderImageSplitGfx[NS_SIDE_BOTTOM];
    rectToDrawSource.size.width = borderImageSplitGfx[NS_SIDE_RIGHT];
    rectToDrawSource.size.height = borderImageSplitGfx[NS_SIDE_BOTTOM];
    DrawBorderImageSide(thebesCtx, dc, image,
                        rectToDraw, rectToDraw.size, rectToDrawSource,
                        NS_STYLE_BORDER_IMAGE_STRETCH, NS_STYLE_BORDER_IMAGE_STRETCH);
    
    // draw bottom
    rectToDraw = clipRect;
    rectToDraw.pos.x += borderLeft;
    rectToDraw.pos.y += clipRect.size.height - borderBottom;
    rectToDraw.size.width = middleSize.width;
    rectToDraw.size.height = borderBottom;
    rectToDrawSource.pos.x = borderImageSplitGfx[NS_SIDE_LEFT];
    rectToDrawSource.pos.y = gfxImageSize.height - borderImageSplitGfx[NS_SIDE_BOTTOM];
    rectToDrawSource.size.width = middleSizeSource.width;
    rectToDrawSource.size.height = borderImageSplitGfx[NS_SIDE_BOTTOM];
    DrawBorderImageSide(thebesCtx, dc, image,
                        rectToDraw, interSizeBottom, rectToDrawSource, 
                        aBorderStyle.mBorderImageHFill, NS_STYLE_BORDER_IMAGE_STRETCH);
    
    // draw bottom left corner
    rectToDraw = clipRect;
    rectToDraw.pos.y += clipRect.size.height - borderBottom;
    rectToDraw.size.width = borderLeft;
    rectToDraw.size.height = borderBottom;
    rectToDrawSource.pos.x = 0;
    rectToDrawSource.pos.y = gfxImageSize.height - borderImageSplitGfx[NS_SIDE_BOTTOM];
    rectToDrawSource.size.width = borderImageSplitGfx[NS_SIDE_LEFT];
    rectToDrawSource.size.height = borderImageSplitGfx[NS_SIDE_BOTTOM];
    DrawBorderImageSide(thebesCtx, dc, image,
                        rectToDraw, rectToDraw.size, rectToDrawSource,
                        NS_STYLE_BORDER_IMAGE_STRETCH, NS_STYLE_BORDER_IMAGE_STRETCH);
    
    // draw left
    rectToDraw = clipRect;
    rectToDraw.pos.y += borderTop;
    rectToDraw.size.width = borderLeft;
    rectToDraw.size.height = middleSize.height;
    rectToDrawSource.pos.x = 0;
    rectToDrawSource.pos.y = borderImageSplitGfx[NS_SIDE_TOP];
    rectToDrawSource.size.width = borderImageSplitGfx[NS_SIDE_LEFT];
    rectToDrawSource.size.height = middleSizeSource.height;
    DrawBorderImageSide(thebesCtx, dc, image,
                        rectToDraw, interSizeLeft, rectToDrawSource, 
                        NS_STYLE_BORDER_IMAGE_STRETCH, aBorderStyle.mBorderImageVFill);

    // Draw middle
    rectToDraw = clipRect;
    rectToDraw.pos.x += borderLeft;
    rectToDraw.pos.y += borderTop;
    rectToDraw.size.width = middleSize.width;
    rectToDraw.size.height = middleSize.height;
    rectToDrawSource.pos.x = borderImageSplitGfx[NS_SIDE_LEFT];
    rectToDrawSource.pos.y = borderImageSplitGfx[NS_SIDE_TOP];
    rectToDrawSource.size = middleSizeSource;
    DrawBorderImageSide(thebesCtx, dc, image,
                        rectToDraw, interSizeMiddle, rectToDrawSource,
                        aBorderStyle.mBorderImageHFill, aBorderStyle.mBorderImageVFill);

    thebesCtx->PopGroupToSource();
    thebesCtx->SetOperator(gfxContext::OPERATOR_OVER);
    thebesCtx->Paint();
    thebesCtx->Restore();
}

static void
DrawBorderImageSide(gfxContext *aThebesContext,
                    nsIDeviceContext* aDeviceContext,
                    imgIContainer* aImage,
                    gfxRect& aDestRect,
                    gfxSize aInterSize, // non-ref to allow aliasing
                    gfxRect& aSourceRect,
                    PRUint8 aHFillType,
                    PRUint8 aVFillType)
{
  if (aDestRect.size.width < 1.0 || aDestRect.size.height < 1.0 ||
      aSourceRect.size.width < 1.0 || aSourceRect.size.height < 1.0) {
    return;
  }

  gfxIntSize gfxSourceSize((PRInt32)aSourceRect.size.width,
                           (PRInt32)aSourceRect.size.height);

  // where the actual border ends up being rendered
  if (aThebesContext->UserToDevicePixelSnapped(aDestRect))
    aDestRect = aThebesContext->DeviceToUser(aDestRect);
  if (aThebesContext->UserToDevicePixelSnapped(aSourceRect))
    aSourceRect = aThebesContext->DeviceToUser(aSourceRect);

  if (aDestRect.size.height < 1.0 ||
     aDestRect.size.width < 1.0)
    return;

  if (aInterSize.width < 1.0 ||
     aInterSize.height < 1.0)
    return;

  // Surface will hold just the part of the source image specified by the aSourceRect
  // but at a different size
  nsRefPtr<gfxASurface> interSurface =
    gfxPlatform::GetPlatform()->CreateOffscreenSurface(
        gfxSourceSize, gfxASurface::ImageFormatARGB32);

  gfxMatrix srcMatrix;
  // Adjust the matrix scale for Step 1 of the spec
  srcMatrix.Scale(aSourceRect.size.width/aInterSize.width,
                  aSourceRect.size.height/aInterSize.height);
  {
    nsCOMPtr<gfxIImageFrame> frame;
    nsresult rv = aImage->GetCurrentFrame(getter_AddRefs(frame));
    if(NS_FAILED(rv))
      return;
    nsCOMPtr<nsIImage> image;
    image = do_GetInterface(frame);
    if(!image)
      return;

    // surface for the whole image
    nsRefPtr<gfxPattern> imagePattern;
    rv = image->GetPattern(getter_AddRefs(imagePattern));
    if(NS_FAILED(rv) || !imagePattern)
      return;

    gfxMatrix mat;
    mat.Translate(aSourceRect.pos);
    imagePattern->SetMatrix(mat);

    // Straightforward blit - no resizing
    nsRefPtr<gfxContext> srcCtx = new gfxContext(interSurface);
    srcCtx->SetPattern(imagePattern);
    srcCtx->SetOperator(gfxContext::OPERATOR_SOURCE);
    srcCtx->Paint();
    srcCtx = nsnull;

  }

  // offset to make the middle tile centered in the middle of the border
  gfxPoint renderOffset(0, 0);
  gfxSize rectSize(aDestRect.size);

  aThebesContext->Save();
  aThebesContext->Clip(aDestRect);

  gfxFloat hScale(1.0), vScale(1.0);

  nsRefPtr<gfxPattern> pattern = new gfxPattern(interSurface);
  pattern->SetExtend(gfxPattern::EXTEND_PAD_EDGE);
  switch (aHFillType) {
    case NS_STYLE_BORDER_IMAGE_REPEAT:
      renderOffset.x = (rectSize.width - aInterSize.width*NS_ceil(rectSize.width/aInterSize.width))*-0.5;
      aDestRect.pos.x -= renderOffset.x;
      pattern->SetExtend(gfxPattern::EXTEND_REPEAT);
      break;
    case NS_STYLE_BORDER_IMAGE_ROUND:
      hScale = aInterSize.width*(NS_ceil(aDestRect.size.width/aInterSize.width)/aDestRect.size.width);
      pattern->SetExtend(gfxPattern::EXTEND_REPEAT);
      break;
    case NS_STYLE_BORDER_IMAGE_STRETCH:
    default:
      hScale = aInterSize.width/aDestRect.size.width;
      break;
  }

  switch (aVFillType) {
    case NS_STYLE_BORDER_IMAGE_REPEAT:
      renderOffset.y = (rectSize.height - aInterSize.height*NS_ceil(rectSize.height/aInterSize.height))*-0.5;
      aDestRect.pos.y -= renderOffset.y;
      pattern->SetExtend(gfxPattern::EXTEND_REPEAT);
      break;
    case NS_STYLE_BORDER_IMAGE_ROUND:
      vScale = aInterSize.height*(NS_ceil(aDestRect.size.height/aInterSize.height)/aDestRect.size.height);
      pattern->SetExtend(gfxPattern::EXTEND_REPEAT);
      break;
    case NS_STYLE_BORDER_IMAGE_STRETCH:
    default:
      vScale = aInterSize.height/aDestRect.size.height;
      break;
  }

  // Adjust the matrix scale for Step 2 of the spec
  srcMatrix.Scale(hScale,vScale);
  pattern->SetMatrix(srcMatrix);

  // render
  aThebesContext->Translate(aDestRect.pos);
  aThebesContext->SetPattern(pattern);
  aThebesContext->NewPath();
  aThebesContext->Rectangle(gfxRect(renderOffset, rectSize));
  aThebesContext->SetOperator(gfxContext::OPERATOR_ADD);
  aThebesContext->Fill();
  aThebesContext->Restore();
}

static void
PaintBackgroundColor(nsPresContext* aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     nsIFrame* aForFrame,
                     const nsRect& aBgClipArea,
                     const nsStyleBackground& aColor,
                     const nsStyleBorder& aBorder,
                     PRBool aCanPaintNonWhite)
{
  // If we're only allowed to paint white, then don't bail out on transparent
  // color if we're not completely transparent.  See the corresponding check
  // for whether we're allowed to paint background images in
  // PaintBackgroundWithSC before the first call to PaintBackgroundColor.
  if (NS_GET_A(aColor.mBackgroundColor) == 0 &&
      (aCanPaintNonWhite || aColor.IsTransparent())) {
    // nothing to paint
    return;
  }

  nscolor color = aColor.mBackgroundColor;
  if (!aCanPaintNonWhite) {
    color = NS_RGB(255, 255, 255);
  }
  aRenderingContext.SetColor(color);

  if (!nsLayoutUtils::HasNonZeroCorner(aBorder.mBorderRadius)) {
    aRenderingContext.FillRect(aBgClipArea);
    return;
  }

  gfxContext *ctx = aRenderingContext.ThebesContext();

  // needed for our border thickness
  nscoord appUnitsPerPixel = aPresContext->AppUnitsPerDevPixel();

  nscoord borderRadii[8];
  GetBorderRadiusTwips(aBorder.mBorderRadius, aForFrame->GetSize().width,
                       borderRadii);

  // the bgClipArea is the outside
  gfxRect oRect(RectToGfxRect(aBgClipArea, appUnitsPerPixel));
  oRect.Round();
  oRect.Condition();
  if (oRect.IsEmpty())
    return;

  // convert the radii
  gfxCornerSizes radii;
  ComputePixelRadii(borderRadii, aBgClipArea,
                    aForFrame ? aForFrame->GetSkipSides() : 0,
                    appUnitsPerPixel, &radii);

  // Add 1.0 to any border radii; if we don't, the border and background
  // curves will combine to have fringing at the rounded corners.  Since
  // alpha is used for coverage, we have problems because the border and
  // background should have identical coverage, and the border should
  // overlay the background exactly.  The way to avoid this is by using
  // a supersampling scheme, but we don't have the mechanism in place to do
  // this.  So, this will do for now.
  for (int i = 0; i < 4; i++) {
    if (radii[i].width > 0.0)
      radii[i].width += 1.0;
    if (radii[i].height > 0.0)
      radii[i].height += 1.0;
  }

  ctx->NewPath();
  ctx->RoundedRectangle(oRect, radii);
  ctx->Fill();
}


// Begin table border-collapsing section
// These functions were written to not disrupt the normal ones and yet satisfy some additional requirements
// At some point, all functions should be unified to include the additional functionality that these provide

static nscoord
RoundIntToPixel(nscoord aValue, 
                nscoord aTwipsPerPixel,
                PRBool  aRoundDown = PR_FALSE)
{
  if (aTwipsPerPixel <= 0) 
    // We must be rendering to a device that has a resolution greater than Twips! 
    // In that case, aValue is as accurate as it's going to get.
    return aValue; 

  nscoord halfPixel = NSToCoordRound(aTwipsPerPixel / 2.0f);
  nscoord extra = aValue % aTwipsPerPixel;
  nscoord finalValue = (!aRoundDown && (extra >= halfPixel)) ? aValue + (aTwipsPerPixel - extra) : aValue - extra;
  return finalValue;
}

static nscoord
RoundFloatToPixel(float   aValue, 
                  nscoord aTwipsPerPixel,
                  PRBool  aRoundDown = PR_FALSE)
{
  return RoundIntToPixel(NSToCoordRound(aValue), aTwipsPerPixel, aRoundDown);
}

static void
SetPoly(const nsRect& aRect,
        nsPoint*      poly)
{
  poly[0].x = aRect.x;
  poly[0].y = aRect.y;
  poly[1].x = aRect.x + aRect.width;
  poly[1].y = aRect.y;
  poly[2].x = aRect.x + aRect.width;
  poly[2].y = aRect.y + aRect.height;
  poly[3].x = aRect.x;
  poly[3].y = aRect.y + aRect.height;
  poly[4].x = aRect.x;
  poly[4].y = aRect.y;
}
          
static void 
DrawSolidBorderSegment(nsIRenderingContext& aContext,
                       nsRect               aRect,
                       nscoord              aTwipsPerPixel,
                       PRUint8              aStartBevelSide = 0,
                       nscoord              aStartBevelOffset = 0,
                       PRUint8              aEndBevelSide = 0,
                       nscoord              aEndBevelOffset = 0)
{

  if ((aRect.width == aTwipsPerPixel) || (aRect.height == aTwipsPerPixel) ||
      ((0 == aStartBevelOffset) && (0 == aEndBevelOffset))) {
    // simple line or rectangle
    if ((NS_SIDE_TOP == aStartBevelSide) || (NS_SIDE_BOTTOM == aStartBevelSide)) {
      if (1 == aRect.height) 
        aContext.DrawLine(aRect.x, aRect.y, aRect.x, aRect.y + aRect.height); 
      else 
        aContext.FillRect(aRect);
    }
    else {
      if (1 == aRect.width) 
        aContext.DrawLine(aRect.x, aRect.y, aRect.x + aRect.width, aRect.y); 
      else 
        aContext.FillRect(aRect);
    }
  }
  else {
    // polygon with beveling
    nsPoint poly[5];
    SetPoly(aRect, poly);
    switch(aStartBevelSide) {
    case NS_SIDE_TOP:
      poly[0].x += aStartBevelOffset;
      poly[4].x = poly[0].x;
      break;
    case NS_SIDE_BOTTOM:
      poly[3].x += aStartBevelOffset;
      break;
    case NS_SIDE_RIGHT:
      poly[1].y += aStartBevelOffset;
      break;
    case NS_SIDE_LEFT:
      poly[0].y += aStartBevelOffset;
      poly[4].y = poly[0].y;
    }

    switch(aEndBevelSide) {
    case NS_SIDE_TOP:
      poly[1].x -= aEndBevelOffset;
      break;
    case NS_SIDE_BOTTOM:
      poly[2].x -= aEndBevelOffset;
      break;
    case NS_SIDE_RIGHT:
      poly[2].y -= aEndBevelOffset;
      break;
    case NS_SIDE_LEFT:
      poly[3].y -= aEndBevelOffset;
    }

    aContext.FillPolygon(poly, 5);
  }


}

static void
GetDashInfo(nscoord  aBorderLength,
            nscoord  aDashLength,
            nscoord  aTwipsPerPixel,
            PRInt32& aNumDashSpaces,
            nscoord& aStartDashLength,
            nscoord& aEndDashLength)
{
  aNumDashSpaces = 0;
  if (aStartDashLength + aDashLength + aEndDashLength >= aBorderLength) {
    aStartDashLength = aBorderLength;
    aEndDashLength = 0;
  }
  else {
    aNumDashSpaces = aBorderLength / (2 * aDashLength); // round down
    nscoord extra = aBorderLength - aStartDashLength - aEndDashLength - (((2 * aNumDashSpaces) - 1) * aDashLength);
    if (extra > 0) {
      nscoord half = RoundIntToPixel(extra / 2, aTwipsPerPixel);
      aStartDashLength += half;
      aEndDashLength += (extra - half);
    }
  }
}

void 
nsCSSRendering::DrawTableBorderSegment(nsIRenderingContext&     aContext,
                                       PRUint8                  aBorderStyle,  
                                       nscolor                  aBorderColor,
                                       const nsStyleBackground* aBGColor,
                                       const nsRect&            aBorder,
                                       PRInt32                  aAppUnitsPerCSSPixel,
                                       PRUint8                  aStartBevelSide,
                                       nscoord                  aStartBevelOffset,
                                       PRUint8                  aEndBevelSide,
                                       nscoord                  aEndBevelOffset)
{
  aContext.SetColor (aBorderColor); 

  PRBool horizontal = ((NS_SIDE_TOP == aStartBevelSide) || (NS_SIDE_BOTTOM == aStartBevelSide));
  nscoord twipsPerPixel = NSIntPixelsToAppUnits(1, aAppUnitsPerCSSPixel);
  PRUint8 ridgeGroove = NS_STYLE_BORDER_STYLE_RIDGE;

  if ((twipsPerPixel >= aBorder.width) || (twipsPerPixel >= aBorder.height) ||
      (NS_STYLE_BORDER_STYLE_DASHED == aBorderStyle) || (NS_STYLE_BORDER_STYLE_DOTTED == aBorderStyle)) {
    // no beveling for 1 pixel border, dash or dot
    aStartBevelOffset = 0;
    aEndBevelOffset = 0;
  }

  gfxContext *ctx = aContext.ThebesContext();
  gfxContext::AntialiasMode oldMode = ctx->CurrentAntialiasMode();
  ctx->SetAntialiasMode(gfxContext::MODE_ALIASED);

  switch (aBorderStyle) {
  case NS_STYLE_BORDER_STYLE_NONE:
  case NS_STYLE_BORDER_STYLE_HIDDEN:
    //NS_ASSERTION(PR_FALSE, "style of none or hidden");
    break;
  case NS_STYLE_BORDER_STYLE_DOTTED:
  case NS_STYLE_BORDER_STYLE_DASHED: 
    {
      nscoord dashLength = (NS_STYLE_BORDER_STYLE_DASHED == aBorderStyle) ? DASH_LENGTH : DOT_LENGTH;
      // make the dash length proportional to the border thickness
      dashLength *= (horizontal) ? aBorder.height : aBorder.width;
      // make the min dash length for the ends 1/2 the dash length
      nscoord minDashLength = (NS_STYLE_BORDER_STYLE_DASHED == aBorderStyle) 
                              ? RoundFloatToPixel(((float)dashLength) / 2.0f, twipsPerPixel) : dashLength;
      minDashLength = PR_MAX(minDashLength, twipsPerPixel);
      nscoord numDashSpaces = 0;
      nscoord startDashLength = minDashLength;
      nscoord endDashLength   = minDashLength;
      if (horizontal) {
        GetDashInfo(aBorder.width, dashLength, twipsPerPixel, numDashSpaces, startDashLength, endDashLength);
        nsRect rect(aBorder.x, aBorder.y, startDashLength, aBorder.height);
        DrawSolidBorderSegment(aContext, rect, twipsPerPixel);
        for (PRInt32 spaceX = 0; spaceX < numDashSpaces; spaceX++) {
          rect.x += rect.width + dashLength;
          rect.width = (spaceX == (numDashSpaces - 1)) ? endDashLength : dashLength;
          DrawSolidBorderSegment(aContext, rect, twipsPerPixel);
        }
      }
      else {
        GetDashInfo(aBorder.height, dashLength, twipsPerPixel, numDashSpaces, startDashLength, endDashLength);
        nsRect rect(aBorder.x, aBorder.y, aBorder.width, startDashLength);
        DrawSolidBorderSegment(aContext, rect, twipsPerPixel);
        for (PRInt32 spaceY = 0; spaceY < numDashSpaces; spaceY++) {
          rect.y += rect.height + dashLength;
          rect.height = (spaceY == (numDashSpaces - 1)) ? endDashLength : dashLength;
          DrawSolidBorderSegment(aContext, rect, twipsPerPixel);
        }
      }
    }
    break;                                  
  case NS_STYLE_BORDER_STYLE_GROOVE:
    ridgeGroove = NS_STYLE_BORDER_STYLE_GROOVE; // and fall through to ridge
  case NS_STYLE_BORDER_STYLE_RIDGE:
    if ((horizontal && (twipsPerPixel >= aBorder.height)) ||
        (!horizontal && (twipsPerPixel >= aBorder.width))) {
      // a one pixel border
      DrawSolidBorderSegment(aContext, aBorder, twipsPerPixel, aStartBevelSide, aStartBevelOffset,
                             aEndBevelSide, aEndBevelOffset);
    }
    else {
      nscoord startBevel = (aStartBevelOffset > 0) 
                            ? RoundFloatToPixel(0.5f * (float)aStartBevelOffset, twipsPerPixel, PR_TRUE) : 0;
      nscoord endBevel =   (aEndBevelOffset > 0) 
                            ? RoundFloatToPixel(0.5f * (float)aEndBevelOffset, twipsPerPixel, PR_TRUE) : 0;
      PRUint8 ridgeGrooveSide = (horizontal) ? NS_SIDE_TOP : NS_SIDE_LEFT;
      aContext.SetColor ( 
        MakeBevelColor(ridgeGrooveSide, ridgeGroove, aBGColor->mBackgroundColor, aBorderColor));
      nsRect rect(aBorder);
      nscoord half;
      if (horizontal) { // top, bottom
        half = RoundFloatToPixel(0.5f * (float)aBorder.height, twipsPerPixel);
        rect.height = half;
        if (NS_SIDE_TOP == aStartBevelSide) {
          rect.x += startBevel;
          rect.width -= startBevel;
        }
        if (NS_SIDE_TOP == aEndBevelSide) {
          rect.width -= endBevel;
        }
        DrawSolidBorderSegment(aContext, rect, twipsPerPixel, aStartBevelSide, 
                               startBevel, aEndBevelSide, endBevel);
      }
      else { // left, right
        half = RoundFloatToPixel(0.5f * (float)aBorder.width, twipsPerPixel);
        rect.width = half;
        if (NS_SIDE_LEFT == aStartBevelSide) {
          rect.y += startBevel;
          rect.height -= startBevel;
        }
        if (NS_SIDE_LEFT == aEndBevelSide) {
          rect.height -= endBevel;
        }
        DrawSolidBorderSegment(aContext, rect, twipsPerPixel, aStartBevelSide, 
                               startBevel, aEndBevelSide, endBevel);
      }

      rect = aBorder;
      ridgeGrooveSide = (NS_SIDE_TOP == ridgeGrooveSide) ? NS_SIDE_BOTTOM : NS_SIDE_RIGHT;
      aContext.SetColor ( 
        MakeBevelColor(ridgeGrooveSide, ridgeGroove, aBGColor->mBackgroundColor, aBorderColor));
      if (horizontal) {
        rect.y = rect.y + half;
        rect.height = aBorder.height - half;
        if (NS_SIDE_BOTTOM == aStartBevelSide) {
          rect.x += startBevel;
          rect.width -= startBevel;
        }
        if (NS_SIDE_BOTTOM == aEndBevelSide) {
          rect.width -= endBevel;
        }
        DrawSolidBorderSegment(aContext, rect, twipsPerPixel, aStartBevelSide, 
                               startBevel, aEndBevelSide, endBevel);
      }
      else {
        rect.x = rect.x + half;
        rect.width = aBorder.width - half;
        if (NS_SIDE_RIGHT == aStartBevelSide) {
          rect.y += aStartBevelOffset - startBevel;
          rect.height -= startBevel;
        }
        if (NS_SIDE_RIGHT == aEndBevelSide) {
          rect.height -= endBevel;
        }
        DrawSolidBorderSegment(aContext, rect, twipsPerPixel, aStartBevelSide, 
                               startBevel, aEndBevelSide, endBevel);
      }
    }
    break;
  case NS_STYLE_BORDER_STYLE_DOUBLE:
    if ((aBorder.width > 2) && (aBorder.height > 2)) {
      nscoord startBevel = (aStartBevelOffset > 0) 
                            ? RoundFloatToPixel(0.333333f * (float)aStartBevelOffset, twipsPerPixel) : 0;
      nscoord endBevel =   (aEndBevelOffset > 0) 
                            ? RoundFloatToPixel(0.333333f * (float)aEndBevelOffset, twipsPerPixel) : 0;
      if (horizontal) { // top, bottom
        nscoord thirdHeight = RoundFloatToPixel(0.333333f * (float)aBorder.height, twipsPerPixel);

        // draw the top line or rect
        nsRect topRect(aBorder.x, aBorder.y, aBorder.width, thirdHeight);
        if (NS_SIDE_TOP == aStartBevelSide) {
          topRect.x += aStartBevelOffset - startBevel;
          topRect.width -= aStartBevelOffset - startBevel;
        }
        if (NS_SIDE_TOP == aEndBevelSide) {
          topRect.width -= aEndBevelOffset - endBevel;
        }
        DrawSolidBorderSegment(aContext, topRect, twipsPerPixel, aStartBevelSide, 
                               startBevel, aEndBevelSide, endBevel);

        // draw the botom line or rect
        nscoord heightOffset = aBorder.height - thirdHeight; 
        nsRect bottomRect(aBorder.x, aBorder.y + heightOffset, aBorder.width, aBorder.height - heightOffset);
        if (NS_SIDE_BOTTOM == aStartBevelSide) {
          bottomRect.x += aStartBevelOffset - startBevel;
          bottomRect.width -= aStartBevelOffset - startBevel;
        }
        if (NS_SIDE_BOTTOM == aEndBevelSide) {
          bottomRect.width -= aEndBevelOffset - endBevel;
        }
        DrawSolidBorderSegment(aContext, bottomRect, twipsPerPixel, aStartBevelSide, 
                               startBevel, aEndBevelSide, endBevel);
      }
      else { // left, right
        nscoord thirdWidth = RoundFloatToPixel(0.333333f * (float)aBorder.width, twipsPerPixel);

        nsRect leftRect(aBorder.x, aBorder.y, thirdWidth, aBorder.height); 
        if (NS_SIDE_LEFT == aStartBevelSide) {
          leftRect.y += aStartBevelOffset - startBevel;
          leftRect.height -= aStartBevelOffset - startBevel;
        }
        if (NS_SIDE_LEFT == aEndBevelSide) {
          leftRect.height -= aEndBevelOffset - endBevel;
        }
        DrawSolidBorderSegment(aContext, leftRect, twipsPerPixel, aStartBevelSide,
                               startBevel, aEndBevelSide, endBevel);

        nscoord widthOffset = aBorder.width - thirdWidth; 
        nsRect rightRect(aBorder.x + widthOffset, aBorder.y, aBorder.width - widthOffset, aBorder.height);
        if (NS_SIDE_RIGHT == aStartBevelSide) {
          rightRect.y += aStartBevelOffset - startBevel;
          rightRect.height -= aStartBevelOffset - startBevel;
        }
        if (NS_SIDE_RIGHT == aEndBevelSide) {
          rightRect.height -= aEndBevelOffset - endBevel;
        }
        DrawSolidBorderSegment(aContext, rightRect, twipsPerPixel, aStartBevelSide,
                               startBevel, aEndBevelSide, endBevel);
      }
      break;
    }
    // else fall through to solid
  case NS_STYLE_BORDER_STYLE_SOLID:
    DrawSolidBorderSegment(aContext, aBorder, twipsPerPixel, aStartBevelSide, 
                           aStartBevelOffset, aEndBevelSide, aEndBevelOffset);
    break;
  case NS_STYLE_BORDER_STYLE_OUTSET:
  case NS_STYLE_BORDER_STYLE_INSET:
    NS_ASSERTION(PR_FALSE, "inset, outset should have been converted to groove, ridge");
    break;
  case NS_STYLE_BORDER_STYLE_AUTO:
    NS_ASSERTION(PR_FALSE, "Unexpected 'auto' table border");
    break;
  }

  ctx->SetAntialiasMode(oldMode);
}

// End table border-collapsing section

void
nsCSSRendering::PaintDecorationLine(gfxContext* aGfxContext,
                                    const nscolor aColor,
                                    const gfxPoint& aPt,
                                    const gfxSize& aLineSize,
                                    const gfxFloat aAscent,
                                    const gfxFloat aOffset,
                                    const PRUint8 aDecoration,
                                    const PRUint8 aStyle)
{
  gfxRect rect =
    GetTextDecorationRectInternal(aPt, aLineSize, aAscent, aOffset,
                                  aDecoration, aStyle);
  if (rect.IsEmpty())
    return;

  if (aDecoration != NS_STYLE_TEXT_DECORATION_UNDERLINE &&
      aDecoration != NS_STYLE_TEXT_DECORATION_OVERLINE &&
      aDecoration != NS_STYLE_TEXT_DECORATION_LINE_THROUGH)
  {
    NS_ERROR("Invalid decoration value!");
    return;
  }

  gfxFloat lineHeight = PR_MAX(NS_round(aLineSize.height), 1.0);
  PRBool contextIsSaved = PR_FALSE;

  gfxFloat oldLineWidth;
  nsRefPtr<gfxPattern> oldPattern;

  switch (aStyle) {
    case NS_STYLE_BORDER_STYLE_SOLID:
    case NS_STYLE_BORDER_STYLE_DOUBLE:
      oldLineWidth = aGfxContext->CurrentLineWidth();
      oldPattern = aGfxContext->GetPattern();
      break;
    case NS_STYLE_BORDER_STYLE_DASHED: {
      aGfxContext->Save();
      contextIsSaved = PR_TRUE;
      gfxFloat dashWidth = lineHeight * DOT_LENGTH * DASH_LENGTH;
      gfxFloat dash[2] = { dashWidth, dashWidth };
      aGfxContext->SetLineCap(gfxContext::LINE_CAP_BUTT);
      aGfxContext->SetDash(dash, 2, 0.0);
      break;
    }
    case NS_STYLE_BORDER_STYLE_DOTTED: {
      aGfxContext->Save();
      contextIsSaved = PR_TRUE;
      gfxFloat dashWidth = lineHeight * DOT_LENGTH;
      gfxFloat dash[2];
      if (lineHeight > 2.0) {
        dash[0] = 0.0;
        dash[1] = dashWidth * 2.0;
        aGfxContext->SetLineCap(gfxContext::LINE_CAP_ROUND);
      } else {
        dash[0] = dashWidth;
        dash[1] = dashWidth;
      }
      aGfxContext->SetDash(dash, 2, 0.0);
      break;
    }
    default:
      NS_ERROR("Invalid style value!");
      return;
  }

  // The y position should be set to the middle of the line.
  rect.pos.y += lineHeight / 2;

  aGfxContext->SetColor(gfxRGBA(aColor));
  aGfxContext->SetLineWidth(lineHeight);
  switch (aStyle) {
    case NS_STYLE_BORDER_STYLE_SOLID:
      aGfxContext->NewPath();
      aGfxContext->MoveTo(rect.TopLeft());
      aGfxContext->LineTo(rect.TopRight());
      aGfxContext->Stroke();
      break;
    case NS_STYLE_BORDER_STYLE_DOUBLE:
      aGfxContext->NewPath();
      aGfxContext->MoveTo(rect.TopLeft());
      aGfxContext->LineTo(rect.TopRight());
      rect.size.height -= lineHeight;
      aGfxContext->MoveTo(rect.BottomLeft());
      aGfxContext->LineTo(rect.BottomRight());
      aGfxContext->Stroke();
      break;
    case NS_STYLE_BORDER_STYLE_DOTTED:
    case NS_STYLE_BORDER_STYLE_DASHED:
      aGfxContext->NewPath();
      aGfxContext->MoveTo(rect.TopLeft());
      aGfxContext->LineTo(rect.TopRight());
      aGfxContext->Stroke();
      break;
    default:
      NS_ERROR("Invalid style value!");
      break;
  }

  if (contextIsSaved) {
    aGfxContext->Restore();
  } else {
    aGfxContext->SetPattern(oldPattern);
    aGfxContext->SetLineWidth(oldLineWidth);
  }
}

nsRect
nsCSSRendering::GetTextDecorationRect(nsPresContext* aPresContext,
                                      const gfxSize& aLineSize,
                                      const gfxFloat aAscent,
                                      const gfxFloat aOffset,
                                      const PRUint8 aDecoration,
                                      const PRUint8 aStyle)
{
  NS_ASSERTION(aPresContext, "aPresContext is null");

  gfxRect rect =
    GetTextDecorationRectInternal(gfxPoint(0, 0), aLineSize, aAscent, aOffset,
                                  aDecoration, aStyle);
  // The rect values are already rounded to nearest device pixels.
  nsRect r;
  r.x = aPresContext->GfxUnitsToAppUnits(rect.X());
  r.y = aPresContext->GfxUnitsToAppUnits(rect.Y());
  r.width = aPresContext->GfxUnitsToAppUnits(rect.Width());
  r.height = aPresContext->GfxUnitsToAppUnits(rect.Height());
  return r;
}

static gfxRect
GetTextDecorationRectInternal(const gfxPoint& aPt,
                              const gfxSize& aLineSize,
                              const gfxFloat aAscent,
                              const gfxFloat aOffset,
                              const PRUint8 aDecoration,
                              const PRUint8 aStyle)
{
  gfxRect r;
  r.pos.x = NS_floor(aPt.x + 0.5);
  r.size.width = NS_round(aLineSize.width);

  gfxFloat basesize = NS_round(aLineSize.height);
  basesize = PR_MAX(basesize, 1.0);
  r.size.height = basesize;
  if (aStyle == NS_STYLE_BORDER_STYLE_DOUBLE) {
    gfxFloat gap = NS_round(basesize / 2.0);
    gap = PR_MAX(gap, 1.0);
    r.size.height = basesize * 2.0 + gap;
  } else {
    r.size.height = basesize;
  }

  gfxFloat baseline = NS_floor(aPt.y + aAscent + 0.5);
  gfxFloat offset = 0;
  switch (aDecoration) {
    case NS_STYLE_TEXT_DECORATION_UNDERLINE:
      offset = aOffset;
      break;
    case NS_STYLE_TEXT_DECORATION_OVERLINE:
      offset = aOffset - basesize + r.Height();
      break;
    case NS_STYLE_TEXT_DECORATION_LINE_THROUGH: {
      gfxFloat extra = NS_floor(r.Height() / 2.0 + 0.5);
      extra = PR_MAX(extra, basesize);
      offset = aOffset - basesize + extra;
      break;
    }
    default:
      NS_ERROR("Invalid decoration value!");
  }
  r.pos.y = baseline - NS_floor(offset + 0.5);
  return r;
}

// -----
// nsContextBoxBlur
// -----
gfxContext*
nsContextBoxBlur::Init(const gfxRect& aRect, nscoord aBlurRadius,
                       PRInt32 aAppUnitsPerDevPixel,
                       gfxContext* aDestinationCtx)
{
  mDestinationCtx = aDestinationCtx;

  PRInt32 blurRadius = static_cast<PRInt32>(aBlurRadius / aAppUnitsPerDevPixel);

  // if not blurring, draw directly onto the destination device
  if (blurRadius <= 0) {
    mContext = aDestinationCtx;
    return mContext;
  }

  // Convert from app units to device pixels
  gfxRect rect = aRect;
  rect.ScaleInverse(aAppUnitsPerDevPixel);

  if (rect.IsEmpty()) {
    mContext = aDestinationCtx;
    return mContext;
  }

  mDestinationCtx = aDestinationCtx;

  mContext = blur.Init(rect, gfxIntSize(blurRadius, blurRadius));
  return mContext;
}

void
nsContextBoxBlur::DoPaint()
{
  if (mContext == mDestinationCtx)
    return;

  blur.Paint(mDestinationCtx);
}

gfxContext*
nsContextBoxBlur::GetContext()
{
  return mContext;
}

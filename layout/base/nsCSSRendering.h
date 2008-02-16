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

/* utility functions for drawing borders and backgrounds */

#ifndef nsCSSRendering_h___
#define nsCSSRendering_h___

#include "nsIRenderingContext.h"
#include "nsStyleConsts.h"
#include "gfxContext.h"
struct nsPoint;
class nsStyleContext;
class nsPresContext;

class nsCSSRendering {
public:
  /**
   * Initialize any static variables used by nsCSSRendering.
   */
  static nsresult Init();
  
  /**
   * Clean up any static variables used by nsCSSRendering.
   */
  static void Shutdown();
  
  /**
   * Render the border for an element using css rendering rules
   * for borders. aSkipSides is a bitmask of the sides to skip
   * when rendering. If 0 then no sides are skipped.
   *
   * Both aDirtyRect and aBorderArea are in the local coordinate space
   * of aForFrame
   */
  static void PaintBorder(nsPresContext* aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          nsIFrame* aForFrame,
                          const nsRect& aDirtyRect,
                          const nsRect& aBorderArea,
                          const nsStyleBorder& aBorderStyle,
                          nsStyleContext* aStyleContext,
                          PRIntn aSkipSides,
                          nsRect* aGap = 0,
                          nscoord aHardBorderSize = 0,
                          PRBool aShouldIgnoreRounded = PR_FALSE);

  /**
   * Render the outline for an element using css rendering rules
   * for borders. aSkipSides is a bitmask of the sides to skip
   * when rendering. If 0 then no sides are skipped.
   *
   * Both aDirtyRect and aBorderArea are in the local coordinate space
   * of aForFrame
   */
  static void PaintOutline(nsPresContext* aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          nsIFrame* aForFrame,
                          const nsRect& aDirtyRect,
                          const nsRect& aBorderArea,
                          const nsStyleBorder& aBorderStyle,
                          const nsStyleOutline& aOutlineStyle,
                          nsStyleContext* aStyleContext,
                          nsRect* aGap = 0);

  /**
   * Fill in an nsStyleBackground to be used to paint the background for
   * an element.  The nsStyleBackground should first be initialized
   * using the pres context.  This applies the rules for propagating
   * backgrounds between BODY, the root element, and the canvas.
   * @return PR_TRUE if there is some meaningful background.
   */
  static PRBool FindBackground(nsPresContext* aPresContext,
                               nsIFrame* aForFrame,
                               const nsStyleBackground** aBackground,
                               PRBool* aIsCanvas);
                               
  /**
   * Find a non-transparent background, for various table-related and
   * HR-related backwards-compatibility hacks.  Be very hesitant if
   * you're considering calling this function -- it's usually not what
   * you want.
   */
  static const nsStyleBackground*
  FindNonTransparentBackground(nsStyleContext* aContext,
                               PRBool aStartAtParent = PR_FALSE);

  /**
   * Render the background for an element using css rendering rules
   * for backgrounds.
   *
   * Both aDirtyRect and aBorderArea are in the local coordinate space
   * of aForFrame
   */
  static void PaintBackground(nsPresContext* aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              nsIFrame* aForFrame,
                              const nsRect& aDirtyRect,
                              const nsRect& aBorderArea,
                              const nsStyleBorder& aBorder,
                              const nsStylePadding& aPadding,
                              PRBool aUsePrintSettings,
                              nsRect* aBGClipRect = nsnull);

  /**
   * Same as |PaintBackground|, except using the provided style context
   * (which short-circuits the code that ensures that the root element's
   * background is drawn on the canvas.
   */
  static void PaintBackgroundWithSC(nsPresContext* aPresContext,
                                    nsIRenderingContext& aRenderingContext,
                                    nsIFrame* aForFrame,
                                    const nsRect& aDirtyRect,
                                    const nsRect& aBorderArea,
                                    const nsStyleBackground& aColor,
                                    const nsStyleBorder& aBorder,
                                    const nsStylePadding& aPadding,
                                    PRBool aUsePrintSettings = PR_FALSE,
                                    nsRect* aBGClipRect = nsnull);

  /**
   * Called by the presShell when painting is finished, so we can clear our
   * inline background data cache.
   */
  static void DidPaint();


  static void DrawDashedSides(PRIntn startSide,
                              nsIRenderingContext& aContext,
                              const nsRect& aDirtyRect,
                              const PRUint8 borderStyles[],
                              const nscolor borderColors[],    
                              const nsRect& borderOutside,
                              const nsRect& borderInside,
                              PRIntn aSkipSides,
                              nsRect* aGap);

  static void DrawDashedSides(PRIntn startSide,
                              nsIRenderingContext& aContext,
                              const nsRect& aDirtyRect,
                              const nsStyleColor* aColorStyle,
                              const nsStyleBorder* aBorderStyle,  
                              const nsStyleOutline* aOutlineStyle,  
                              PRBool aDoOutline,
                              const nsRect& borderOutside,
                              const nsRect& borderInside,
                              PRIntn aSkipSides,
                              nsRect* aGap);

  // Draw a border segment in the table collapsing border model without beveling corners
  static void DrawTableBorderSegment(nsIRenderingContext&     aContext,
                                     PRUint8                  aBorderStyle,  
                                     nscolor                  aBorderColor,
                                     const nsStyleBackground* aBGColor,
                                     const nsRect&            aBorderRect,
                                     PRInt32                  aAppUnitsPerCSSPixel,
                                     PRUint8                  aStartBevelSide = 0,
                                     nscoord                  aStartBevelOffset = 0,
                                     PRUint8                  aEndBevelSide = 0,
                                     nscoord                  aEndBevelOffset = 0);
  /**
   * transform a color to a color that will show up on a printer if needed
   * aMapColor - color to evaluate
   * aIsPrinter - Is this a printing device
   * return - the transformed color
   */
  static nscolor TransformColor(nscolor  aMapColor,PRBool aNoBackGround);

  /**
   * Function for painting the decoration lines for the text.
   * NOTE: aPt, aLineSize, aAscent and aOffset are non-rounded device pixels,
   *       not app units.
   *   input:
   *     @param aGfxContext
   *     @param aColor            the color of the decoration line
   *     @param aPt               the top/left edge of the text
   *     @param aLineSize         the width and the height of the decoration
   *                              line
   *     @param aAscent           the ascent of the text
   *     @param aOffset           the offset of the decoration line from
   *                              the baseline of the text (if the value is
   *                              positive, the line is lifted up)
   *     @param aDecoration       which line will be painted. The value can be
   *                              NS_STYLE_TEXT_DECORATION_UNDERLINE or
   *                              NS_STYLE_TEXT_DECORATION_OVERLINE or
   *                              NS_STYLE_TEXT_DECORATION_LINE_THROUGH.
   *     @param aStyle            the style of the decoration line. The value
   *                              can be NS_STYLE_BORDER_STYLE_SOLID or
   *                              NS_STYLE_BORDER_STYLE_DOTTED or
   *                              NS_STYLE_BORDER_STYLE_DASHED or
   *                              NS_STYLE_BORDER_STYLE_DOUBLE.
   *     @param aIsRTL            when the text is RTL, it is true.
   */
  static void PaintDecorationLine(gfxContext* aGfxContext,
                                  const nscolor aColor,
                                  const gfxPoint& aPt,
                                  const gfxSize& aLineSize,
                                  const gfxFloat aAscent,
                                  const gfxFloat aOffset,
                                  const PRUint8 aDecoration,
                                  const PRUint8 aStyle,
                                  const PRBool aIsRTL);

  /**
   * Function for getting the decoration line rect for the text.
   * NOTE: aLineSize, aAscent and aOffset are non-rounded device pixels,
   *       not app units.
   *   input:
   *     @param aPresContext
   *     @param aLineSize         the width and the height of the decoration
   *                              line
   *     @param aAscent           the ascent of the text
   *     @param aOffset           the offset of the decoration line from
   *                              the baseline of the text (if the value is
   *                              positive, the line is lifted up)
   *     @param aDecoration       which line will be painted. The value can be
   *                              NS_STYLE_TEXT_DECORATION_UNDERLINE or
   *                              NS_STYLE_TEXT_DECORATION_OVERLINE or
   *                              NS_STYLE_TEXT_DECORATION_LINE_THROUGH.
   *     @param aStyle            the style of the decoration line. The value
   *                              can be NS_STYLE_BORDER_STYLE_SOLID or
   *                              NS_STYLE_BORDER_STYLE_DOTTED or
   *                              NS_STYLE_BORDER_STYLE_DASHED or
   *                              NS_STYLE_BORDER_STYLE_DOUBLE.
   *   output:
   *     @return                  the decoration line rect for the input,
   *                              the each values are app units.
   */
  static nsRect GetTextDecorationRect(nsPresContext* aPresContext,
                                      const gfxSize& aLineSize,
                                      const gfxFloat aAscent,
                                      const gfxFloat aOffset,
                                      const PRUint8 aDecoration,
                                      const PRUint8 aStyle);

protected:

  static void PaintBackgroundColor(nsPresContext* aPresContext,
                                   nsIRenderingContext& aRenderingContext,
                                   nsIFrame* aForFrame,
                                   const nsRect& aBgClipArea,
                                   const nsStyleBackground& aColor,
                                   const nsStyleBorder& aBorder,
                                   const nsStylePadding& aPadding,
                                   PRBool aCanPaintNonWhite);

  static void PaintRoundedBackground(nsPresContext* aPresContext,
                                     nsIRenderingContext& aRenderingContext,
                                     nsIFrame* aForFrame,
                                     const nsRect& aBorderArea,
                                     const nsStyleBackground& aColor,
                                     const nsStyleBorder& aBorder,
                                     nscoord aTheRadius[4],
                                     PRBool aCanPaintNonWhite);

  static nscolor MakeBevelColor(PRIntn whichSide, PRUint8 style,
                                nscolor aBackgroundColor,
                                nscolor aBorderColor);

  static void DrawLine (nsIRenderingContext& aContext, 
                        nscoord aX1, nscoord aY1, nscoord aX2, nscoord aY2,
                        nsRect* aGap);

  static void FillPolygon (nsIRenderingContext& aContext, 
                           const nsPoint aPoints[],
                           PRInt32 aNumPoints,
                           nsRect* aGap);

  static gfxRect GetTextDecorationRectInternal(const gfxPoint& aPt,
                                               const gfxSize& aLineSize,
                                               const gfxFloat aAscent,
                                               const gfxFloat aOffset,
                                               const PRUint8 aDecoration,
                                               const PRUint8 aStyle);
};


#endif /* nsCSSRendering_h___ */

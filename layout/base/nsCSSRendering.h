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
#include "gfxBlur.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"

struct nsPoint;
class nsStyleContext;
class nsPresContext;

struct nsCSSRendering {
  /**
   * Initialize any static variables used by nsCSSRendering.
   */
  static nsresult Init();
  
  /**
   * Clean up any static variables used by nsCSSRendering.
   */
  static void Shutdown();
  
  static void PaintBoxShadowInner(nsPresContext* aPresContext,
                                  nsIRenderingContext& aRenderingContext,
                                  nsIFrame* aForFrame,
                                  const nsRect& aFrameArea,
                                  const nsRect& aDirtyRect);

  static void PaintBoxShadowOuter(nsPresContext* aPresContext,
                                  nsIRenderingContext& aRenderingContext,
                                  nsIFrame* aForFrame,
                                  const nsRect& aFrameArea,
                                  const nsRect& aDirtyRect);

  static void ComputePixelRadii(const nscoord *aAppUnitsRadii,
                                nscoord aAppUnitsPerPixel,
                                gfxCornerSizes *oBorderRadii);

  /**
   * Render the border for an element using css rendering rules
   * for borders. aSkipSides is a bitmask of the sides to skip
   * when rendering. If 0 then no sides are skipped.
   */
  static void PaintBorder(nsPresContext* aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          nsIFrame* aForFrame,
                          const nsRect& aDirtyRect,
                          const nsRect& aBorderArea,
                          nsStyleContext* aStyleContext,
                          PRIntn aSkipSides = 0);

  /**
   * Like PaintBorder, but taking an nsStyleBorder argument instead of
   * getting it from aStyleContext.
   */
  static void PaintBorderWithStyleBorder(nsPresContext* aPresContext,
                                         nsIRenderingContext& aRenderingContext,
                                         nsIFrame* aForFrame,
                                         const nsRect& aDirtyRect,
                                         const nsRect& aBorderArea,
                                         const nsStyleBorder& aBorderStyle,
                                         nsStyleContext* aStyleContext,
                                         PRIntn aSkipSides = 0);


  /**
   * Render the outline for an element using css rendering rules
   * for borders. aSkipSides is a bitmask of the sides to skip
   * when rendering. If 0 then no sides are skipped.
   */
  static void PaintOutline(nsPresContext* aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          nsIFrame* aForFrame,
                          const nsRect& aDirtyRect,
                          const nsRect& aBorderArea,
                          nsStyleContext* aStyleContext);

  /**
   * Render keyboard focus on an element.
   * |aFocusRect| is the outer rectangle of the focused element.
   * Uses a fixed style equivalent to "1px dotted |aColor|".
   * Not used for controls, because the native theme may differ.
   */
  static void PaintFocus(nsPresContext* aPresContext,
                         nsIRenderingContext& aRenderingContext,
                         const nsRect& aFocusRect,
                         nscolor aColor);

  /**
   * Render a gradient for an element.
   */
  static void PaintGradient(nsPresContext* aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            nsStyleGradient* aGradient,
                            const nsRect& aDirtyRect,
                            const nsRect& aOneCellArea,
                            const nsRect& aFillArea);

  /**
   * Find the frame whose background style should be used to draw the
   * canvas background. aForFrame must be the frame for the root element
   * whose background style should be used. This function will return
   * aForFrame unless the <body> background should be propagated, in
   * which case we return the frame associated with the <body>'s background.
   */
  static nsIFrame* FindBackgroundStyleFrame(nsIFrame* aForFrame);

  /**
   * @return PR_TRUE if |aFrame| is a canvas frame, in the CSS sense.
   */
  static PRBool IsCanvasFrame(nsIFrame* aFrame);

  /**
   * Fill in an aBackgroundSC to be used to paint the background
   * for an element.  This applies the rules for propagating
   * backgrounds between BODY, the root element, and the canvas.
   * @return PR_TRUE if there is some meaningful background.
   */
  static PRBool FindBackground(nsPresContext* aPresContext,
                               nsIFrame* aForFrame,
                               nsStyleContext** aBackgroundSC);

  /**
   * As FindBackground, but the passed-in frame is known to be a root frame
   * (returned from nsCSSFrameConstructor::GetRootElementStyleFrame())
   * and there is always some meaningful background returned.
   */
  static nsStyleContext* FindRootFrameBackground(nsIFrame* aForFrame);

  /**
   * Returns background style information for the canvas.
   *
   * @param aForFrame
   *   the frame used to represent the canvas, in the CSS sense (i.e.
   *   nsCSSRendering::IsCanvasFrame(aForFrame) must be true)
   * @param aRootElementFrame
   *   the frame representing the root element of the document
   * @param aBackground
   *   contains background style information for the canvas on return
   */
  static nsStyleContext*
  FindCanvasBackground(nsIFrame* aForFrame, nsIFrame* aRootElementFrame)
  {
    NS_ABORT_IF_FALSE(IsCanvasFrame(aForFrame), "not a canvas frame");
    if (aRootElementFrame)
      return FindRootFrameBackground(aRootElementFrame);

    // This should always give transparent, so we'll fill it in with the
    // default color if needed.  This seems to happen a bit while a page is
    // being loaded.
    return aForFrame->GetStyleContext();
  }

  /**
   * Find a frame which draws a non-transparent background,
   * for various table-related and HR-related backwards-compatibility hacks.
   * This function will also stop if it finds themed frame which might draw
   * background.
   *
   * Be very hesitant if you're considering calling this function -- it's
   * usually not what you want.
   */
  static nsIFrame*
  FindNonTransparentBackgroundFrame(nsIFrame* aFrame,
                                    PRBool aStartAtParent = PR_FALSE);

  /**
   * Determine the background color to draw taking into account print settings.
   */
  static nscolor
  DetermineBackgroundColor(nsPresContext* aPresContext,
                           nsStyleContext* aStyleContext,
                           nsIFrame* aFrame);

  /**
   * Render the background for an element using css rendering rules
   * for backgrounds.
   */
  enum {
    /**
     * When this flag is passed, the element's nsDisplayBorder will be
     * painted immediately on top of this background.
     */
    PAINTBG_WILL_PAINT_BORDER = 0x01,
    /**
     * When this flag is passed, images are synchronously decoded. */
    PAINTBG_SYNC_DECODE_IMAGES = 0x02
  };
  static void PaintBackground(nsPresContext* aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              nsIFrame* aForFrame,
                              const nsRect& aDirtyRect,
                              const nsRect& aBorderArea,
                              PRUint32 aFlags,
                              nsRect* aBGClipRect = nsnull);

  /**
   * Same as |PaintBackground|, except using the provided style structs.
   * This short-circuits the code that ensures that the root element's
   * background is drawn on the canvas.
   */
  static void PaintBackgroundWithSC(nsPresContext* aPresContext,
                                    nsIRenderingContext& aRenderingContext,
                                    nsIFrame* aForFrame,
                                    const nsRect& aDirtyRect,
                                    const nsRect& aBorderArea,
                                    nsStyleContext *aStyleContext,
                                    const nsStyleBorder& aBorder,
                                    PRUint32 aFlags,
                                    nsRect* aBGClipRect = nsnull);

  /**
   * Called by the presShell when painting is finished, so we can clear our
   * inline background data cache.
   */
  static void DidPaint();

  // Draw a border segment in the table collapsing border model without
  // beveling corners
  static void DrawTableBorderSegment(nsIRenderingContext& aContext,
                                     PRUint8              aBorderStyle,  
                                     nscolor              aBorderColor,
                                     const nsStyleBackground* aBGColor,
                                     const nsRect&        aBorderRect,
                                     PRInt32              aAppUnitsPerCSSPixel,
                                     PRUint8              aStartBevelSide = 0,
                                     nscoord              aStartBevelOffset = 0,
                                     PRUint8              aEndBevelSide = 0,
                                     nscoord              aEndBevelOffset = 0);

  enum {
    DECORATION_STYLE_NONE   = 0,
    DECORATION_STYLE_SOLID  = 1,
    DECORATION_STYLE_DOTTED = 2,
    DECORATION_STYLE_DASHED = 3,
    DECORATION_STYLE_DOUBLE = 4,
    DECORATION_STYLE_WAVY   = 5
  };

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
   *     @param aStyle            the style of the decoration line (See above
   *                              enum names).
   *     @param aDescentLimit     If aDescentLimit is zero or larger and the
   *                              underline overflows from the descent space,
   *                              the underline should be lifted up as far as
   *                              possible.  Note that this does not mean the
   *                              underline never overflows from this
   *                              limitation.  Because if the underline is
   *                              positioned to the baseline or upper, it causes
   *                              unreadability.  Note that if this is zero
   *                              or larger, the underline rect may be shrunken
   *                              if it's possible.  Therefore, this value is
   *                              used for strikeout line and overline too.
   */
  static void PaintDecorationLine(gfxContext* aGfxContext,
                                  const nscolor aColor,
                                  const gfxPoint& aPt,
                                  const gfxSize& aLineSize,
                                  const gfxFloat aAscent,
                                  const gfxFloat aOffset,
                                  const PRUint8 aDecoration,
                                  const PRUint8 aStyle,
                                  const gfxFloat aDescentLimit = -1.0);

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
   *     @param aStyle            the style of the decoration line (See above
   *                              enum names).
   *     @param aDescentLimit     If aDescentLimit is zero or larger and the
   *                              underline overflows from the descent space,
   *                              the underline should be lifted up as far as
   *                              possible.  Note that this does not mean the
   *                              underline never overflows from this
   *                              limitation.  Because if the underline is
   *                              positioned to the baseline or upper, it causes
   *                              unreadability.  Note that if this is zero
   *                              or larger, the underline rect may be shrunken
   *                              if it's possible.  Therefore, this value is
   *                              used for strikeout line and overline too.
   *   output:
   *     @return                  the decoration line rect for the input,
   *                              the each values are app units.
   */
  static nsRect GetTextDecorationRect(nsPresContext* aPresContext,
                                      const gfxSize& aLineSize,
                                      const gfxFloat aAscent,
                                      const gfxFloat aOffset,
                                      const PRUint8 aDecoration,
                                      const PRUint8 aStyle,
                                      const gfxFloat aDescentLimit = -1.0);

protected:
  static gfxRect GetTextDecorationRectInternal(const gfxPoint& aPt,
                                               const gfxSize& aLineSize,
                                               const gfxFloat aAscent,
                                               const gfxFloat aOffset,
                                               const PRUint8 aDecoration,
                                               const PRUint8 aStyle,
                                               const gfxFloat aDscentLimit);
};

/*
 * nsContextBoxBlur
 * Creates an 8-bit alpha channel context for callers to draw in, blurs the
 * contents of that context and applies it as a 1-color mask on a
 * different existing context. Uses gfxAlphaBoxBlur as its back end.
 *
 * You must call Init() first to create a suitable temporary surface to draw
 * on.  You must then draw any desired content onto the given context, then
 * call DoPaint() to apply the blurred content as a single-color mask. You
 * can only call Init() once, so objects cannot be reused.
 *
 * This is very useful for creating drop shadows or silhouettes.
 */
class nsContextBoxBlur {
public:
  enum {
    FORCE_MASK = 0x01
  };
  /**
   * Prepares a gfxContext to draw on. Do not call this twice; if you want
   * to get the gfxContext again use GetContext().
   *
   * @param aRect                The coordinates of the surface to create.
   *                             All coordinates must be in app units.
   *                             This must not include the blur radius, pass
   *                             it as the second parameter and everything
   *                             is taken care of.
   *
   * @param aBlurRadius          The blur radius in app units.
   *
   * @param aAppUnitsPerDevPixel The number of app units in a device pixel,
   *                             for conversion.  Most of the time you'll
   *                             pass this from the current PresContext if
   *                             available.
   *
   * @param aDestinationCtx      The graphics context to apply the blurred
   *                             mask to when you call DoPaint(). Make sure
   *                             it is not destroyed before you call
   *                             DoPaint(). To set the color of the
   *                             resulting blurred graphic mask, you must
   *                             set the color on this context before
   *                             calling Init().
   *
   * @param aDirtyRect           The absolute dirty rect in app units. Used to
   *                             optimize the temporary surface size and speed up blur.
   *
   * @param aSkipRect            An area in device pixels (NOT app units!) to avoid
   *                             blurring over, to prevent unnecessary work.
   *                             
   * @param aFlags               FORCE_MASK to ensure that the content drawn to the
   *                             returned gfxContext is used as a mask, and not
   *                             drawn directly to aDestinationCtx.
   *
   * @return            A blank 8-bit alpha-channel-only graphics context to
   *                    draw on, or null on error. Must not be freed. The
   *                    context has a device offset applied to it given by
   *                    aRect. This means you can use coordinates as if it
   *                    were at the desired position at aRect and you don't
   *                    need to worry about translating any coordinates to
   *                    draw on this temporary surface.
   *
   * If aBlurRadius is 0, the returned context is aDestinationCtx and
   * DoPaint() does nothing, because no blurring is required. Therefore, you
   * should prepare the destination context as if you were going to draw
   * directly on it instead of any temporary surface created in this class.
   */
  gfxContext* Init(const nsRect& aRect, nscoord aSpreadRadius,
                   nscoord aBlurRadius,
                   PRInt32 aAppUnitsPerDevPixel, gfxContext* aDestinationCtx,
                   const nsRect& aDirtyRect, const gfxRect* aSkipRect,
                   PRUint32 aFlags = 0);

  /**
   * Does the actual blurring/spreading. Users of this object *must*
   * have called Init() first, then have drawn whatever they want to be
   * blurred onto the internal gfxContext before calling this.
   */
  void DoEffects();
  
  /**
   * Does the actual blurring and mask applying. Users of this object *must*
   * have called Init() first, then have drawn whatever they want to be
   * blurred onto the internal gfxContext before calling this.
   */
  void DoPaint();

  /**
   * Gets the internal gfxContext at any time. Must not be freed. Avoid
   * calling this before calling Init() since the context would not be
   * constructed at that point.
   */
  gfxContext* GetContext();


  /**
   * Get the margin associated with the given blur radius, i.e., the
   * additional area that might be painted as a result of it.  (The
   * margin for a spread radius is itself, on all sides.)
   */
  static nsMargin GetBlurRadiusMargin(nscoord aBlurRadius,
                                      PRInt32 aAppUnitsPerDevPixel);

protected:
  gfxAlphaBoxBlur blur;
  nsRefPtr<gfxContext> mContext;
  gfxContext* mDestinationCtx;
  
};

#endif /* nsCSSRendering_h___ */

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

/* base class #2 for rendering objects that have child lists */

#ifndef nsHTMLContainerFrame_h___
#define nsHTMLContainerFrame_h___

#include "nsContainerFrame.h"
#include "gfxPoint.h"
class nsString;
class nsAbsoluteFrame;
class nsPlaceholderFrame;
struct nsStyleDisplay;
struct nsStylePosition;
struct nsHTMLReflowMetrics;
struct nsHTMLReflowState;
class nsLineBox;

// Some macros for container classes to do sanity checking on
// width/height/x/y values computed during reflow.
#ifdef DEBUG
#define CRAZY_W 500000

// 100000 lines, approximately. Assumes p2t is 15 and 15 pixels per line
#define CRAZY_H 22500000

#define CRAZY_WIDTH(_x) (((_x) < -CRAZY_W) || ((_x) > CRAZY_W))
#define CRAZY_HEIGHT(_y) (((_y) < -CRAZY_H) || ((_y) > CRAZY_H))
#endif

class nsDisplayTextDecoration;

// Base class for html container frames that provides common
// functionality.
class nsHTMLContainerFrame : public nsContainerFrame {
public:

  /**
   * Helper method to create next-in-flows if necessary. If aFrame
   * already has a next-in-flow then this method does
   * nothing. Otherwise, a new continuation frame is created and
   * linked into the flow. In addition, the new frame becomes the
   * next-sibling of aFrame. If aPlaceholderResult is not null and
   * aFrame is a float or positioned, then *aPlaceholderResult holds
   * a placeholder.
   */
  static nsresult CreateNextInFlow(nsPresContext* aPresContext,
                                   nsIFrame*       aOuterFrame,
                                   nsIFrame*       aFrame,
                                   nsIFrame*&      aNextInFlowResult);

  /**
   * Helper method to wrap views around frames. Used by containers
   * under special circumstances (can be used by leaf frames as well)
   */
  static nsresult CreateViewForFrame(nsIFrame* aFrame,
                                     PRBool aForce);

  static nsresult ReparentFrameView(nsPresContext* aPresContext,
                                    nsIFrame*       aChildFrame,
                                    nsIFrame*       aOldParentFrame,
                                    nsIFrame*       aNewParentFrame);

  static nsresult ReparentFrameViewList(nsPresContext* aPresContext,
                                        nsIFrame*       aChildFrameList,
                                        nsIFrame*       aOldParentFrame,
                                        nsIFrame*       aNewParentFrame);

  /**
   * Displays the standard border, background and outline for the frame
   * and calls DisplayTextDecorationsAndChildren. This is suitable for
   * inline frames or frames that behave like inlines.
   */
  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);
                              
  nsresult DisplayTextDecorations(nsDisplayListBuilder* aBuilder,
                                  nsDisplayList* aBelowTextDecorations,
                                  nsDisplayList* aAboveTextDecorations,
                                  nsLineBox* aLine);

protected:
  nsHTMLContainerFrame(nsStyleContext *aContext) : nsContainerFrame(aContext) {}

  /**
   * Displays the below-children decorations, then the children, then
   * the above-children decorations, with the decorations going in the
   * Content() list. This is suitable for inline elements and elements
   * that behave like inline elements (e.g. MathML containers).
   */
  nsresult DisplayTextDecorationsAndChildren(nsDisplayListBuilder* aBuilder, 
                                             const nsRect& aDirtyRect,
                                             const nsDisplayListSet& aLists);

  /**
   * Fetch the text decorations for this frame. 
   *  @param aIsBlock      whether |this| is a block frame or no.
   *  @param aDecorations  mask with all decorations. 
   *                         See bug 1777 and 20163 to understand how a
   *                         frame can end up with several decorations.
   *  @param aUnderColor   The color of underline if the appropriate bit 
   *                         in aDecoration is set. It is undefined otherwise.
   *  @param aOverColor    The color of overline if the appropriate bit 
   *                         in aDecoration is set. It is undefined otherwise.
   *  @param aStrikeColor  The color of strike-through if the appropriate bit 
   *                         in aDecoration is set. It is undefined otherwise.
   *  NOTE: This function assigns NS_STYLE_TEXT_DECORATION_NONE to
   *        aDecorations for text-less frames.  See bug 20163 for
   *        details.
   */
  void GetTextDecorations(nsPresContext* aPresContext, 
                          PRBool aIsBlock,
                          PRUint8& aDecorations, 
                          nscolor& aUnderColor, 
                          nscolor& aOverColor, 
                          nscolor& aStrikeColor);

  /** 
   * Function that does the actual drawing of the textdecoration. 
   *   input:
   *    @param aCtx               the Thebes graphics context to draw on
   *    @param aLine              the line, or nsnull if this is an inline frame
   *    @param aColor             the color of the text-decoration
   *    @param aAscent            ascent of the font from which the
   *                                text-decoration was derived. 
   *    @param aOffset            distance *above* baseline where the
   *                                text-decoration should be drawn,
   *                                i.e. negative offsets draws *below*
   *                                the baseline.
   *    @param aSize              the thickness of the line
   *    @param aDecoration        which line will be painted
   *                                i.e., NS_STYLE_TEXT_DECORATION_UNDERLINE or
   *                                      NS_STYLE_TEXT_DECORATION_OVERLINE or
   *                                      NS_STYLE_TEXT_DECORATION_LINE_THROUGH.
   */
  virtual void PaintTextDecorationLine(gfxContext* aCtx,
                                       const nsPoint& aPt,
                                       nsLineBox* aLine,
                                       nscolor aColor,
                                       gfxFloat aOffset,
                                       gfxFloat aAscent,
                                       gfxFloat aSize,
                                       const PRUint8 aDecoration);

  friend class nsDisplayTextDecoration;
  friend class nsDisplayTextShadow;
};

#endif /* nsHTMLContainerFrame_h___ */

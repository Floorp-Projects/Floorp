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
#include "nsDisplayList.h"
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
// NOTE: AppUnitsPerCSSPixel value hardwired here to remove the
// dependency on nsDeviceContext.h.  It doesn't matter if it's a
// little off.
#ifdef DEBUG
#define CRAZY_W (1000000*60)
#define CRAZY_H CRAZY_W

#define CRAZY_WIDTH(_x) (((_x) < -CRAZY_W) || ((_x) > CRAZY_W))
#define CRAZY_HEIGHT(_y) (((_y) < -CRAZY_H) || ((_y) > CRAZY_H))
#endif

// Base class for html container frames that provides common
// functionality.
class nsHTMLContainerFrame : public nsContainerFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  /**
   * Helper method to create next-in-flows if necessary. If aFrame
   * already has a next-in-flow then this method does
   * nothing. Otherwise, a new continuation frame is created and
   * linked into the flow. In addition, the new frame is inserted
   * into the principal child list after aFrame.
   * @note calling this method on a block frame is illegal. Use
   * nsBlockFrame::CreateContinuationFor() instead.
   * @param aNextInFlowResult will contain the next-in-flow
   *        <b>if and only if</b> one is created. If a next-in-flow already
   *        exists aNextInFlowResult is set to nsnull.
   * @return NS_OK if a next-in-flow already exists or is successfully created.
   */
  nsresult CreateNextInFlow(nsPresContext* aPresContext,
                            nsIFrame*       aFrame,
                            nsIFrame*&      aNextInFlowResult);

  /**
   * Displays the standard border, background and outline for the frame
   * and calls DisplayTextDecorationsAndChildren. This is suitable for
   * inline frames or frames that behave like inlines.
   */
  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

protected:
  nsHTMLContainerFrame(nsStyleContext *aContext) : nsContainerFrame(aContext) {}
};

#endif /* nsHTMLContainerFrame_h___ */

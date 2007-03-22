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

/*
 * rendering object for the point that anchors out-of-flow rendering
 * objects such as floats and absolutely positioned elements
 */

/*
 * Destruction of a placeholder and its out-of-flow must observe the
 * following constraints:
 *
 * - The mapping from the out-of-flow to the placeholder must be
 *   removed from the frame manager before the placeholder is destroyed.
 * - The mapping from the out-of-flow to the placeholder must be
 *   removed from the frame manager before the out-of-flow is destroyed.
 * - The placeholder must be removed from the frame tree, or have the
 *   mapping from it to its out-of-flow cleared, before the out-of-flow
 *   is destroyed (so that the placeholder will not point to a destroyed
 *   frame while it's in the frame tree).
 *
 * Therefore the safe order of teardown is to:
 *
 * 1)  Unregister the placeholder from the frame manager.
 * 2)  Destroy the placeholder
 * 3)  Destroy the out of flow
 *
 * In certain cases it may be possible to replace step (2) with:
 *
 * 2') Null out the mOutOfFlowFrame pointer in the placeholder
 *
 * and add
 *
 * 4) Destroy the placeholder
 *
 * but this is somewhat dangerous, since lots of code assumes that
 * placeholders point to something useful.
 */

#ifndef nsPlaceholderFrame_h___
#define nsPlaceholderFrame_h___

#include "nsSplittableFrame.h"
#include "nsGkAtoms.h"

nsIFrame* NS_NewPlaceholderFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

/**
 * Implementation of a frame that's used as a placeholder for a frame that
 * has been moved out of the flow
 */
class nsPlaceholderFrame : public nsSplittableFrame {
public:
  /**
   * Create a new placeholder frame
   */
  friend nsIFrame* NS_NewPlaceholderFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);
  nsPlaceholderFrame(nsStyleContext* aContext) : nsSplittableFrame(aContext) {}
  virtual ~nsPlaceholderFrame();

  // Get/Set the associated out of flow frame
  nsIFrame*  GetOutOfFlowFrame() const {return mOutOfFlowFrame;}
  void       SetOutOfFlowFrame(nsIFrame* aFrame) {mOutOfFlowFrame = aFrame;}

  // nsIHTMLReflow overrides
  // We need to override GetMinWidth and GetPrefWidth because XUL uses
  // placeholders not within lines.
  virtual nscoord GetMinWidth(nsIRenderingContext *aRenderingContext);
  virtual nscoord GetPrefWidth(nsIRenderingContext *aRenderingContext);
  virtual void AddInlineMinWidth(nsIRenderingContext *aRenderingContext,
                                 InlineMinWidthData *aData);
  virtual void AddInlinePrefWidth(nsIRenderingContext *aRenderingContext,
                                  InlinePrefWidthData *aData);
  NS_IMETHOD Reflow(nsPresContext* aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);

  virtual void Destroy();
  virtual nsSplittableType GetSplittableType() const;

  // nsIFrame overrides
#if defined(DEBUG) || (defined(MOZ_REFLOW_PERF_DSP) && defined(MOZ_REFLOW_PERF))
  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);
#endif // DEBUG || (MOZ_REFLOW_PERF_DSP && MOZ_REFLOW_PERF)
  
#ifdef DEBUG
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;
#endif // DEBUG

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::placeholderFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

  virtual PRBool IsEmpty() { return PR_TRUE; }
  virtual PRBool IsSelfEmpty() { return PR_TRUE; }

  virtual PRBool CanContinueTextRun() const;

#ifdef ACCESSIBILITY
  NS_IMETHOD  GetAccessible(nsIAccessible** aAccessible)
  {
    nsIFrame *realFrame = GetRealFrameForPlaceholder(this);
    return realFrame ? realFrame->GetAccessible(aAccessible) :
                       nsSplittableFrame::GetAccessible(aAccessible);
  }
#endif

  /**
   * @return the out-of-flow for aFrame if aFrame is a placeholder; otherwise
   * aFrame
   */
  static nsIFrame* GetRealFrameFor(nsIFrame* aFrame) {
    NS_PRECONDITION(aFrame, "Must have a frame to work with");
    if (aFrame->GetType() == nsGkAtoms::placeholderFrame) {
      return GetRealFrameForPlaceholder(aFrame);
    }
    return aFrame;
  }

  /**
   * @return the out-of-flow for aFrame, which is known to be a placeholder
   */
  static nsIFrame* GetRealFrameForPlaceholder(nsIFrame* aFrame) {
    NS_PRECONDITION(aFrame->GetType() == nsGkAtoms::placeholderFrame,
                    "Must have placeholder frame as input");
    nsIFrame* outOfFlow =
      NS_STATIC_CAST(nsPlaceholderFrame*, aFrame)->GetOutOfFlowFrame();
    NS_ASSERTION(outOfFlow, "Null out-of-flow for placeholder?");
    return outOfFlow;
  }

protected:
  nsIFrame* mOutOfFlowFrame;
};

#endif /* nsPlaceholderFrame_h___ */

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
 * Furthermore, some code assumes that placeholders point to something
 * useful, so placeholders without an associated out-of-flow should not
 * remain in the tree.
 *
 * The placeholder's Destroy() implementation handles the destruction of
 * the placeholder and its out-of-flow. To avoid crashes, frame removal
 * and destruction code that works with placeholders must not assume
 * that the placeholder points to its out-of-flow.
 */

#ifndef nsPlaceholderFrame_h___
#define nsPlaceholderFrame_h___

#include "nsFrame.h"
#include "nsGkAtoms.h"

nsIFrame* NS_NewPlaceholderFrame(nsIPresShell* aPresShell,
                                 nsStyleContext* aContext,
                                 nsFrameState aTypeBit);

// Frame state bits that are used to keep track of what this is a
// placeholder for.
#define PLACEHOLDER_FOR_FLOAT    NS_FRAME_STATE_BIT(20)
#define PLACEHOLDER_FOR_ABSPOS   NS_FRAME_STATE_BIT(21)
#define PLACEHOLDER_FOR_FIXEDPOS NS_FRAME_STATE_BIT(22)
#define PLACEHOLDER_FOR_POPUP    NS_FRAME_STATE_BIT(23)
#define PLACEHOLDER_TYPE_MASK    (PLACEHOLDER_FOR_FLOAT | \
                                  PLACEHOLDER_FOR_ABSPOS | \
                                  PLACEHOLDER_FOR_FIXEDPOS | \
                                  PLACEHOLDER_FOR_POPUP)

/**
 * Implementation of a frame that's used as a placeholder for a frame that
 * has been moved out of the flow
 */
class nsPlaceholderFrame : public nsFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  /**
   * Create a new placeholder frame.  aTypeBit must be one of the
   * PLACEHOLDER_FOR_* constants above.
   */
  friend nsIFrame* NS_NewPlaceholderFrame(nsIPresShell* aPresShell,
                                          nsStyleContext* aContext,
                                          nsFrameState aTypeBit);
  nsPlaceholderFrame(nsStyleContext* aContext, nsFrameState aTypeBit) :
    nsFrame(aContext)
  {
    NS_PRECONDITION(aTypeBit == PLACEHOLDER_FOR_FLOAT ||
                    aTypeBit == PLACEHOLDER_FOR_ABSPOS ||
                    aTypeBit == PLACEHOLDER_FOR_FIXEDPOS ||
                    aTypeBit == PLACEHOLDER_FOR_POPUP,
                    "Unexpected type bit");
    AddStateBits(aTypeBit);
  }
  virtual ~nsPlaceholderFrame();

  // Get/Set the associated out of flow frame
  nsIFrame*  GetOutOfFlowFrame() const {return mOutOfFlowFrame;}
  void       SetOutOfFlowFrame(nsIFrame* aFrame) {
               NS_ASSERTION(!aFrame || !aFrame->GetPrevContinuation(),
                            "OOF must be first continuation");
               mOutOfFlowFrame = aFrame;
             }

  // nsIHTMLReflow overrides
  // We need to override GetMinWidth and GetPrefWidth because XUL uses
  // placeholders not within lines.
  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext);
  virtual nscoord GetPrefWidth(nsRenderingContext *aRenderingContext);
  virtual void AddInlineMinWidth(nsRenderingContext *aRenderingContext,
                                 InlineMinWidthData *aData);
  virtual void AddInlinePrefWidth(nsRenderingContext *aRenderingContext,
                                  InlinePrefWidthData *aData);
  virtual nsSize GetMinSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetPrefSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetMaxSize(nsBoxLayoutState& aBoxLayoutState);
  NS_IMETHOD Reflow(nsPresContext* aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);

  virtual void DestroyFrom(nsIFrame* aDestructRoot);

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

  virtual bool IsEmpty() { return true; }
  virtual bool IsSelfEmpty() { return true; }

  virtual bool CanContinueTextRun() const;

#ifdef ACCESSIBILITY
  virtual already_AddRefed<nsAccessible> CreateAccessible()
  {
    nsIFrame* realFrame = GetRealFrameForPlaceholder(this);
    return realFrame ? realFrame->CreateAccessible() :
                       nsFrame::CreateAccessible();
  }
#endif

  virtual nsIFrame* GetParentStyleContextFrame();

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
      static_cast<nsPlaceholderFrame*>(aFrame)->GetOutOfFlowFrame();
    NS_ASSERTION(outOfFlow, "Null out-of-flow for placeholder?");
    return outOfFlow;
  }

protected:
  nsIFrame* mOutOfFlowFrame;
};

#endif /* nsPlaceholderFrame_h___ */

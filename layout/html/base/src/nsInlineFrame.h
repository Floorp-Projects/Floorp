/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#ifndef nsInlineFrame_h___
#define nsInlineFrame_h___

#include "nsHTMLContainerFrame.h"
#include "nsAbsoluteContainingBlock.h"
#include "nsLineLayout.h"

class nsAnonymousBlockFrame;

#define nsInlineFrameSuper nsHTMLContainerFrame

class nsInlineFrame : public nsInlineFrameSuper
{
public:
  friend nsresult NS_NewInlineFrame(nsIFrame*& aNewFrame);

  // nsISupports overrides
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  // nsIFrame overrides
  NS_IMETHOD SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom* aListName,
                                 nsIFrame* aChildList);
  NS_IMETHOD AppendFrames(nsIPresContext& aPresContext,
                          nsIPresShell& aPresShell,
                          nsIAtom* aListName,
                          nsIFrame* aFrameList);
  NS_IMETHOD InsertFrames(nsIPresContext& aPresContext,
                          nsIPresShell& aPresShell,
                          nsIAtom* aListName,
                          nsIFrame* aPrevFrame,
                          nsIFrame* aFrameList);
  NS_IMETHOD RemoveFrame(nsIPresContext& aPresContext,
                         nsIPresShell& aPresShell,
                         nsIAtom* aListName,
                         nsIFrame* aOldFrame);
  NS_IMETHOD DeleteFrame(nsIPresContext& aPresContext);
  NS_IMETHOD GetFrameName(nsString& aResult) const;
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;

  // nsIHTMLReflow overrides
  NS_IMETHOD Reflow(nsIPresContext& aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);
  NS_IMETHOD FindTextRuns(nsLineLayout& aLineLayout);
#if XXX_fix_me
  NS_IMETHOD AdjustFrameSize(nscoord aExtraSpace, nscoord& aUsedSpace);
  NS_IMETHOD TrimTrailingWhiteSpace(nsIPresContext& aPresContext,
                                    nsIRenderingContext& aRC,
                                    nscoord& aDeltaWidth);
#endif

protected:
  // Additional reflow state used during our reflow methods
  struct InlineReflowState {
    nsIFrame* mNextRCFrame;
    nsIFrame* mPrevFrame;
    nsInlineFrame* mNextInFlow;
  };

  // A helper class that knows how to take a list of frames and chop
  // it up into 3 sections.
  struct SectionData {
    SectionData(nsIFrame* aFrameList);

    PRBool SplitFrameList(nsFrameList& aSection1,
                          nsFrameList& aSection2,
                          nsFrameList& aSection3);

    PRBool HasABlock() const {
      return nsnull != firstBlock;
    }

    nsIFrame* firstBlock;
    nsIFrame* prevFirstBlock;
    nsIFrame* lastBlock;
    nsIFrame* firstFrame;
    nsIFrame* lastFrame;
  };

  nsInlineFrame();

  virtual PRIntn GetSkipSides() const;

  PRBool HaveAnonymousBlock() const {
    return mFrames.NotEmpty()
      ? nsLineLayout::TreatFrameAsBlock(mFrames.FirstChild())
      : PR_FALSE;
  }

  static nsIID kInlineFrameCID;

  static PRBool ParentIsInlineFrame(nsIFrame* aFrame, nsIFrame** aParent) {
    void* tmp;
    nsIFrame* parent;
    aFrame->GetParent(&parent);
    *aParent = parent;
    if (NS_SUCCEEDED(parent->QueryInterface(kInlineFrameCID, &tmp))) {
      return PR_TRUE;
    }
    return PR_FALSE;
  }

  nsAnonymousBlockFrame* FindPrevAnonymousBlock(nsInlineFrame** aBlockParent);

  nsAnonymousBlockFrame* FindAnonymousBlock(nsInlineFrame** aBlockParent);

  nsresult CreateAnonymousBlock(nsIPresContext& aPresContext,
                                nsIFrame* aFrameList,
                                nsIFrame** aResult);

  nsresult AppendFrames(nsIPresContext& aPresContext,
                        nsIPresShell& aPresShell,
                        nsIFrame* aFrameList,
                        PRBool aGenerateReflowCommands);

  nsresult InsertBlockFrames(nsIPresContext& aPresContext,
                             nsIPresShell& aPresShell,
                             nsIFrame* aPrevFrame,
                             nsIFrame* aFrameList);

  nsresult InsertInlineFrames(nsIPresContext& aPresContext,
                              nsIPresShell& aPresShell,
                              nsIFrame* aPrevFrame,
                              nsIFrame* aFrameList);

  nsresult ReflowInlineFrames(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              InlineReflowState& rs,
                              nsHTMLReflowMetrics& aMetrics,
                              nsReflowStatus& aStatus);

  nsresult ReflowInlineFrame(nsIPresContext* aPresContext,
                             const nsHTMLReflowState& aReflowState,
                             InlineReflowState& rs,
                             nsIFrame* aFrame,
                             nsReflowStatus& aStatus);

  virtual nsIFrame* PullInlineFrame(nsIPresContext* aPresContext,
                                    InlineReflowState& rs,
                                    PRBool* aIsComplete);

  virtual void PushFrames(nsIPresContext* aPresContext,
                          nsIFrame* aFromChild,
                          nsIFrame* aPrevSibling);

  virtual void DrainOverflow(nsIPresContext* aPresContext);

  nsIFrame* PullAnyFrame(nsIPresContext* aPresContext, InlineReflowState& rs);

  nsresult ReflowBlockFrame(nsIPresContext* aPresContext,
                            const nsHTMLReflowState& aReflowState,
                            InlineReflowState& rs,
                            nsHTMLReflowMetrics& aMetrics,
                            nsReflowStatus& aStatus);
};

//----------------------------------------------------------------------

// Variation on inline-frame used to manage lines for line layout in
// special situations.
class nsFirstLineFrame : public nsInlineFrame {
public:
  friend nsresult NS_NewFirstLineFrame(nsIFrame** aNewFrame);

  NS_IMETHOD GetFrameName(nsString& aResult) const;
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;
  NS_IMETHOD Reflow(nsIPresContext& aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);

  // AppendFrames/InsertFrames/RemoveFrame are implemented to forward
  // the method call to the parent frame.
  NS_IMETHOD AppendFrames(nsIPresContext& aPresContext,
                          nsIPresShell& aPresShell,
                          nsIAtom* aListName,
                          nsIFrame* aFrameList);
  NS_IMETHOD InsertFrames(nsIPresContext& aPresContext,
                          nsIPresShell& aPresShell,
                          nsIAtom* aListName,
                          nsIFrame* aPrevFrame,
                          nsIFrame* aFrameList);
  NS_IMETHOD RemoveFrame(nsIPresContext& aPresContext,
                         nsIPresShell& aPresShell,
                         nsIAtom* aListName,
                         nsIFrame* aOldFrame);

  // These methods are used by the parent frame to actually modify the
  // child frames of the line frame. These methods do not generate
  // reflow commands.
  nsresult AppendFrames2(nsIPresContext* aPresContext,
                         nsIFrame*       aFrameList);

  nsresult InsertFrames2(nsIPresContext* aPresContext,
                         nsIFrame*       aPrevFrame,
                         nsIFrame*       aFrameList);

  nsresult RemoveFrame2(nsIPresContext* aPresContext,
                        nsIFrame*       aOldFrame);

  void RemoveFramesFrom(nsIFrame* aFrame);

  void RemoveAllFrames() {
    mFrames.SetFrames(nsnull);
  }

protected:
  nsFirstLineFrame();

  virtual nsIFrame* PullInlineFrame(nsIPresContext* aPresContext,
                                    InlineReflowState& rs,
                                    PRBool* aIsComplete);

  virtual void DrainOverflow(nsIPresContext* aPresContext);
};

extern nsresult NS_NewFirstLineFrame(nsIFrame** aNewFrame);


//----------------------------------------------------------------------

// Derived class created for relatively positioned inline-level elements
// that acts as a containing block for child absolutely positioned
// elements

class nsPositionedInlineFrame : public nsInlineFrame
{
public:
  NS_IMETHOD DeleteFrame(nsIPresContext& aPresContext);

  NS_IMETHOD SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  NS_IMETHOD GetAdditionalChildListName(PRInt32   aIndex,
                                        nsIAtom** aListName) const;

  NS_IMETHOD FirstChild(nsIAtom* aListName, nsIFrame** aFirstChild) const;

  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
  
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;

protected:
  nsAbsoluteContainingBlock mAbsoluteContainer;
};

#endif /* nsInlineFrame_h___ */

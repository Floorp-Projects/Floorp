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
#include "nsBlockFrame.h"
#include "nsFrameList.h"
#include "nsBlockReflowContext.h"
#include "nsLineLayout.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLParts.h"
#include "nsCOMPtr.h"
#include "nsIStyleContext.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"

// XXX TODO:
// append/insert/remove floater testing

// Theory of operation:
// XXX write this

#ifdef DEBUG
#undef NOISY_ANON_BLOCK
#else
#undef NOISY_ANON_BLOCK
#endif

#define INLINE_FRAME_CID \
 { 0xa6cf90e0, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

static NS_DEFINE_IID(kInlineFrameCID, INLINE_FRAME_CID);

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

  nsresult ReflowInlineFrames(nsIPresContext& aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              InlineReflowState& rs,
                              nsHTMLReflowMetrics& aMetrics,
                              nsReflowStatus& aStatus);

  nsresult ReflowInlineFrame(nsIPresContext& aPresContext,
                             const nsHTMLReflowState& aReflowState,
                             InlineReflowState& rs,
                             nsIFrame* aFrame,
                             nsReflowStatus& aStatus);

  nsIFrame* PullInlineFrame(InlineReflowState& rs, PRBool* aIsComplete);

  nsIFrame* PullAnyFrame(InlineReflowState& rs);

  void PushFrames(nsIFrame* aFromChild, nsIFrame* aPrevSibling);

  void DrainOverflow();

  nsresult ReflowBlockFrame(nsIPresContext& aPresContext,
                            const nsHTMLReflowState& aReflowState,
                            InlineReflowState& rs,
                            nsHTMLReflowMetrics& aMetrics,
                            nsReflowStatus& aStatus);
};

//////////////////////////////////////////////////////////////////////
// SectionData implementation

nsInlineFrame::SectionData::SectionData(nsIFrame* aFrameList)
{
  firstBlock = nsnull;
  prevFirstBlock = nsnull;
  lastBlock = nsnull;
  lastFrame = nsnull;

  // Find the first and last block (if any!). When we exit the loop
  // lastFrame will be the last frame in aList.
  nsIFrame* frame = aFrameList;
  firstFrame = aFrameList;
  while (nsnull != frame) {
    if (nsLineLayout::TreatFrameAsBlock(frame)) {
      if (nsnull == firstBlock) {
        prevFirstBlock = lastFrame;
        firstBlock = frame;
        lastBlock = frame;
      }
      else {
        lastBlock = frame;
      }
    }
    lastFrame = frame;
    frame->GetNextSibling(&frame);
  }
}

PRBool
nsInlineFrame::SectionData::SplitFrameList(nsFrameList& aSection1,
                                           nsFrameList& aSection2,
                                           nsFrameList& aSection3)
{
  if (nsnull == firstBlock) {
    // There are no blocks
    return PR_FALSE;
  }

  // We have at least one block
  if (nsnull != prevFirstBlock) {
    // The first block is not the first frame in aList. Setup section1.
    prevFirstBlock->SetNextSibling(nsnull);
    aSection1.SetFrames(firstFrame);
  }
  aSection2.SetFrames(firstBlock);

  if (lastFrame != lastBlock) {
    // There are inline frames that follow the last block. Setup section3.
    nsIFrame* remainder;
    lastBlock->GetNextSibling(&remainder);
    lastBlock->SetNextSibling(nsnull);
    aSection3.SetFrames(remainder);
  }

  return PR_TRUE;
}

////////////////////////////////////////////////////////////////////////
// Basic nsInlineFrame methods

nsresult
NS_NewInlineFrame(nsIFrame*& aNewFrame)
{
  nsInlineFrame* it = new nsInlineFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aNewFrame = it;
  return NS_OK;
}

nsInlineFrame::nsInlineFrame()
{
}

NS_IMETHODIMP
nsInlineFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kInlineFrameCID)) {
    nsInlineFrame* tmp = this;
    *aInstancePtr = (void*) tmp;
    return NS_OK;
  }
  return nsInlineFrameSuper::QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP
nsInlineFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Inline", aResult);
}

NS_IMETHODIMP
nsInlineFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsHTMLAtoms::inlineFrame;
  NS_ADDREF(*aType);
  return NS_OK;
}

NS_IMETHODIMP
nsInlineFrame::DeleteFrame(nsIPresContext& aPresContext)
{
  mFrames.DeleteFrames(aPresContext);
  return nsInlineFrameSuper::DeleteFrame(aPresContext);
}

//////////////////////////////////////////////////////////////////////
// nsInlineFrame child management

// Find the first inline frame, looking backwards starting at "this",
// that contains an anonymous block. Return nsnull if an anonymous
// block is not found.
nsAnonymousBlockFrame*
nsInlineFrame::FindPrevAnonymousBlock(nsInlineFrame** aBlockParent)
{
  nsInlineFrame* prevInFlow = this;
  while (nsnull != prevInFlow) {
    // Scan the prev-in-flows frame list, looking for an anonymous
    // block frame.
    nsIFrame* frame = prevInFlow->mFrames.FirstChild();
    while (nsnull != frame) {
      if (nsLineLayout::TreatFrameAsBlock(frame)) {
        *aBlockParent = prevInFlow;
        return (nsAnonymousBlockFrame*) frame;
      }
      frame->GetNextSibling(&frame);
    }
    prevInFlow = (nsInlineFrame*) prevInFlow->mPrevInFlow;
  }
  return nsnull;
}

// Find the first inline frame, looking forwards starting at "this",
// that contains an anonymous block. Return nsnull if an anonymous
// block is not found.
nsAnonymousBlockFrame*
nsInlineFrame::FindAnonymousBlock(nsInlineFrame** aBlockParent)
{
  nsInlineFrame* nextInFlow = this;
  while (nsnull != nextInFlow) {
    // Scan the prev-in-flows frame list, looking for an anonymous
    // block frame.
    nsIFrame* frame = nextInFlow->mFrames.FirstChild();
    while (nsnull != frame) {
      if (nsLineLayout::TreatFrameAsBlock(frame)) {
        *aBlockParent = nextInFlow;
        return (nsAnonymousBlockFrame*) frame;
      }
      frame->GetNextSibling(&frame);
    }
    nextInFlow = (nsInlineFrame*) nextInFlow->mNextInFlow;
  }
  return nsnull;
}

nsresult
nsInlineFrame::CreateAnonymousBlock(nsIPresContext& aPresContext,
                                    nsIFrame* aInitialFrames,
                                    nsIFrame** aResult)
{
  nsIFrame* bf;
  nsresult rv = NS_NewAnonymousBlockFrame(bf);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIStyleContext> newSC;
    aPresContext.ResolvePseudoStyleContextFor(mContent, 
                                              nsHTMLAtoms::mozAnonymousBlock,
                                              mStyleContext,
                                              PR_FALSE,
                                              getter_AddRefs(newSC));
    rv = bf->Init(aPresContext, mContent, this, newSC, nsnull);
    if (NS_FAILED(rv)) {
      bf->DeleteFrame(aPresContext);
      delete bf;
    }
    else {
      // Set parent for the frames now that the anonymous block has
      // been created.
      nsIFrame* frame = aInitialFrames;
      while (nsnull != frame) {
        frame->SetParent(bf);
        frame->GetNextSibling(&frame);
      }
      rv = bf->SetInitialChildList(aPresContext, nsnull, aInitialFrames);
    }
    *aResult = bf;
  }
  return rv;
}

NS_IMETHODIMP
nsInlineFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                   nsIAtom* aListName,
                                   nsIFrame* aFrameList)
{
  if (nsnull != aListName) {
    return NS_ERROR_UNEXPECTED;
  }
  if (nsnull == aFrameList) {
    return NS_OK;
  }
  nsCOMPtr<nsIPresShell> shell;
  nsresult rv = aPresContext.GetShell(getter_AddRefs(shell));
  if (NS_SUCCEEDED(rv) && shell) {
    rv = AppendFrames(aPresContext, *shell, aFrameList, PR_FALSE);
  }
  return rv;
}

NS_IMETHODIMP
nsInlineFrame::AppendFrames(nsIPresContext& aPresContext,
                            nsIPresShell& aPresShell,
                            nsIAtom* aListName,
                            nsIFrame* aFrameList)
{
  if (nsnull != aListName) {
    return NS_ERROR_INVALID_ARG;
  }
  if (nsnull == aFrameList) {
    return NS_OK;
  }
  return AppendFrames(aPresContext, aPresShell, aFrameList, PR_TRUE);
}

nsresult
nsInlineFrame::AppendFrames(nsIPresContext& aPresContext,
                            nsIPresShell& aPresShell,
                            nsIFrame* aFrameList,
                            PRBool aGenerateReflowCommands)
{
  nsresult rv = NS_OK;
  SectionData sd(aFrameList);
  if (sd.HasABlock()) {
    nsFrameList section1, section2, section3;
    sd.SplitFrameList(section1, section2, section3);

    // There is at least one block in the new frames. See if there is
    // an anonymous block frame in a prev-in-flow of this frame.
    nsInlineFrame* prevInline;
    nsAnonymousBlockFrame* anonymousBlock;
    anonymousBlock = FindPrevAnonymousBlock(&prevInline);
    if (nsnull != anonymousBlock) {
      // One of the invariants of nsInlineFrame is that there will be
      // at most one anonymous block frame that reflows the
      // nsInlineFrame's block children (not counting any
      // continuations of the anonymous block frame).
      //
      // We maintain that invariant by noticing when a new block frame
      // enters the list of children and ensuring that all of the
      // frames from the first block to the last block are contained
      // by the anonymous block (or one of its continuations). This
      // can cause some inline frames between here and the inline that
      // contains anonymousBlock to have their children stuffed into
      // the anonymous block.

      // Build a list of the frames between the anonymous block and
      // this frame, stealing children from our continuation frames as
      // necessary.
      nsIFrame* inlineSiblings;
      anonymousBlock->GetNextSibling(&inlineSiblings);
      nsFrameList newBlockFrames;
      if (nsnull != inlineSiblings) {
        newBlockFrames.AppendFrames(anonymousBlock, inlineSiblings);
      }
      nsInlineFrame* tmp = (nsInlineFrame*) prevInline->mNextInFlow;
      while ((nsnull != tmp) && (this != tmp)) {
        newBlockFrames.AppendFrames(anonymousBlock, tmp->mFrames);
        tmp = (nsInlineFrame*) tmp->mNextInFlow;
      }

      // Now tack on all of this frame's child frames (unless this
      // frame is the frame that contains the anonymous block)
      if (this != prevInline) {
        newBlockFrames.AppendFrames(anonymousBlock, mFrames);
      }

      // And then append section1 and section2 frames.
      if (section1.NotEmpty()) {
        newBlockFrames.AppendFrames(anonymousBlock, section1);
      }
      newBlockFrames.AppendFrames(anonymousBlock, section2);

      // Finally, if there are any frames in section3 then they are
      // appended after the anonymous block. These frames will be
      // reflowed when they are pushed from the prevInline frame to a
      // next-in-flow after the anonymousBlock frame is reflowed.
      anonymousBlock->SetNextSibling(section3.FirstChild());

      // Now we can append the frames to the anonymous block and it
      // can generate a reflow command.
      rv = anonymousBlock->AppendFrames2(aPresContext, aPresShell, nsnull,
                                         newBlockFrames.FirstChild());
#ifdef NOISY_ANON_BLOCK
      printf("AppendFrames: case 1\n");
#endif
    }
    else {
      // There is no prior block frames that are the children of this
      // inline. Therefore, section 1 is appended to our frame list
      // and we wrap up the frames in section 2 with a new anonymous
      // block and the frames in section 3 get appended to the frame
      // list after the anonymous frame (so that they can be pushed to
      // a next-in-flow after this finishes reflowing its anonymous
      // block).
      nsIFrame* anonymousBlock;
      rv = CreateAnonymousBlock(aPresContext, section2.FirstChild(),
                                &anonymousBlock);
      if (NS_FAILED(rv)) {
        return rv;
      }
      if (section1.NotEmpty()) {
        mFrames.AppendFrames(nsnull, section1);
      }
      mFrames.AppendFrame(nsnull, anonymousBlock);
      if (section3.NotEmpty()) {
        mFrames.AppendFrames(nsnull, section3);
      }
#ifdef NOISY_ANON_BLOCK
      printf("AppendFrames: case 2\n");
#endif

      if (aGenerateReflowCommands) {
        // generate a reflow command for "this"
        nsIReflowCommand* reflowCmd = nsnull;
        rv = NS_NewHTMLReflowCommand(&reflowCmd, this,
                                     nsIReflowCommand::ReflowDirty);
        if (NS_SUCCEEDED(rv)) {
          aPresShell.AppendReflowCommand(reflowCmd);
          NS_RELEASE(reflowCmd);
        }
      }
    }
  }
  else {
    // The new frames contain no block frames
    mFrames.AppendFrames(this, aFrameList);
#ifdef NOISY_ANON_BLOCK
    printf("AppendFrames: case 3\n");
#endif

    if (aGenerateReflowCommands) {
      // generate a reflow command for "this"
      nsIReflowCommand* reflowCmd = nsnull;
      rv = NS_NewHTMLReflowCommand(&reflowCmd, this,
                                   nsIReflowCommand::ReflowDirty);
      if (NS_SUCCEEDED(rv)) {
        aPresShell.AppendReflowCommand(reflowCmd);
        NS_RELEASE(reflowCmd);
      }
    }
  }

  return rv;
}

NS_IMETHODIMP
nsInlineFrame::InsertFrames(nsIPresContext& aPresContext,
                            nsIPresShell& aPresShell,
                            nsIAtom* aListName,
                            nsIFrame* aPrevFrame,
                            nsIFrame* aFrameList)
{
  if (nsnull != aListName) {
    return NS_ERROR_INVALID_ARG;
  }
  if (nsnull == aFrameList) {
    return NS_OK;
  }

  nsresult rv = NS_OK;
  SectionData sd(aFrameList);
  if (sd.HasABlock()) {
    // Break insertion up into 3 pieces
    nsFrameList section1, section2, section3;
    sd.SplitFrameList(section1, section2, section3);

    nsIFrame* prevFrame = aPrevFrame;

    // First insert the inlines in section1 after prevFrame
    if (section1.NotEmpty()) {
      nsIFrame* newPrevFrame = section1.LastChild();
      rv = InsertInlineFrames(aPresContext, aPresShell, prevFrame,
                              section1.FirstChild());
      prevFrame = newPrevFrame;
    }

    // Next insert the frames in section2 after prevFrame
    if (NS_SUCCEEDED(rv)) {
      nsIFrame* newPrevFrame = section2.LastChild();
      rv = InsertBlockFrames(aPresContext, aPresShell, prevFrame,
                             section2.FirstChild());
      prevFrame = newPrevFrame;
    }

    // Finally, insert the frames in section3 after prevFrame
    if (NS_SUCCEEDED(rv) && section3.NotEmpty()) {
      rv = InsertInlineFrames(aPresContext, aPresShell, prevFrame,
                              section3.FirstChild());
    }
  }
  else {
    // Use simpler path when the insertion is only inline frames
    rv = InsertInlineFrames(aPresContext, aPresShell, aPrevFrame, aFrameList);
  }

  return rv;
}

nsresult
nsInlineFrame::InsertBlockFrames(nsIPresContext& aPresContext,
                                 nsIPresShell& aPresShell,
                                 nsIFrame* aPrevFrame,
                                 nsIFrame* aFrameList)
{
  nsresult rv = NS_OK;
  PRBool generateReflowCommand = PR_FALSE;
  nsIFrame* target = nsnull;

  if (nsnull == aPrevFrame) {
    // The block is being inserted at the head of all the child
    // frames.
    nsInlineFrame* flow;
    nsAnonymousBlockFrame* anonymousBlock = FindAnonymousBlock(&flow);
    if (nsnull == anonymousBlock) {
      // There are no anonymous blocks so create one and place the
      // frames into it.
      nsIFrame* anonymousBlock;
      rv = CreateAnonymousBlock(aPresContext, aFrameList, &anonymousBlock);
      if (NS_FAILED(rv)) {
        return rv;
      }
      mFrames.InsertFrames(this, nsnull, anonymousBlock);
      target = this;
      generateReflowCommand = PR_TRUE;
#ifdef NOISY_ANON_BLOCK
      printf("InsertBlockFrames: case 1\n");
#endif
    }
    else {
      // Take all of the frames before the anonymous block, plus the
      // frames in aFrameList and insert them into the anonymous
      // block.
      nsFrameList frames;
      frames.AppendFrames(anonymousBlock, aFrameList);
      nsInlineFrame* start = this;
      while (start != flow) {
        frames.AppendFrames(anonymousBlock, start->mFrames);
        start->GetNextInFlow((nsIFrame**) &start);
      }
      anonymousBlock->InsertFrames2(aPresContext, aPresShell, nsnull,
                                    nsnull, frames.FirstChild());
#ifdef NOISY_ANON_BLOCK
      printf("InsertBlockFrames: case 2\n");
#endif
    }
  }
  else {
    // First see if the insertion is inside the anonymous block
    nsIFrame* prevFrameParent;
    aPrevFrame->GetParent(&prevFrameParent);
    if (nsLineLayout::TreatFrameAsBlock(prevFrameParent)) {
      // The previous frame's parent is an anonymous block. This means
      // that the new block frames can be safely inserted there.
      nsIFrame* frame = aFrameList;
      while (nsnull != frame) {
        frame->SetParent(prevFrameParent);
        frame->GetNextSibling(&frame);
      }
      nsAnonymousBlockFrame* anonymousBlock;
      anonymousBlock = (nsAnonymousBlockFrame*) prevFrameParent;
      anonymousBlock->InsertFrames2(aPresContext, aPresShell, nsnull,
                                    aPrevFrame, aFrameList);
#ifdef NOISY_ANON_BLOCK
      printf("InsertBlockFrames: case 3\n");
#endif
    }
    else {
      // The previous frame's parent is an inline frame. First see if
      // there is an anonymous block before the insertion point.
      nsInlineFrame* flow = (nsInlineFrame*) prevFrameParent;
      nsAnonymousBlockFrame* anonymousBlock;
      anonymousBlock = flow->FindPrevAnonymousBlock(&flow);
      if (nsnull != anonymousBlock) {
        // We found an anonymous block before the insertion
        // point. Take all of the inline frames between the anonymous
        // block and prevFrameParent and strip them away from the
        // inline frames that contain them.
        nsFrameList frames;
        nsInlineFrame* start;
        flow->GetNextInFlow((nsIFrame**) &start);   // start after anon block
        while (start != prevFrameParent) {
          frames.AppendFrames(anonymousBlock, start->mFrames);
          start->GetNextInFlow((nsIFrame**) &start);
        }

        // Now append the frames just before and including aPrevFrame
        // to "frames".
        flow = (nsInlineFrame*) prevFrameParent;
        nsIFrame* remainder;
        flow->mFrames.Split(aPrevFrame, &remainder);
        frames.AppendFrames(anonymousBlock, flow->mFrames);
        flow->mFrames.SetFrames(remainder);
        generateReflowCommand = PR_TRUE;
        target = flow;

        // Finally, append the block frames to "frames" and then
        // append the list of frames to the anonymous block.
        frames.AppendFrames(anonymousBlock, aFrameList);
        anonymousBlock->AppendFrames2(aPresContext, aPresShell, nsnull,
                                      frames.FirstChild());
#ifdef NOISY_ANON_BLOCK
        printf("InsertBlockFrames: case 4\n");
#endif
      }
      else {
        // There is no anymous block before the insertion point. See
        // if there is one after the insertion point.
        flow = (nsInlineFrame*) prevFrameParent;
        anonymousBlock = flow->FindAnonymousBlock(&flow);
        if (nsnull != anonymousBlock) {
          // We found an anonymous block after the insertion
          // point. Seed the list of frames to put into the anonymous
          // block with the block frames being inserted.
          nsFrameList frames;
          frames.AppendFrames(anonymousBlock, aFrameList);
          nsInlineFrame* start = (nsInlineFrame*) prevFrameParent;

          // Take the frames after aPrevFrame and place them at the
          // end of the frames list.
          nsIFrame* remainder;
          start->mFrames.Split(aPrevFrame, &remainder);
          if (nsnull != remainder) {
            frames.AppendFrames(anonymousBlock, remainder);
          }
          generateReflowCommand = PR_TRUE;
          target = start;

          // Gather up all of the inline frames from all of the flow
          // between the insertion point and the anonymous block.
          start->GetNextInFlow((nsIFrame**) &start);
          while (start != flow) {
            frames.AppendFrames(anonymousBlock, start->mFrames);
            start->GetNextInFlow((nsIFrame**) &start);
          }

          // Now update the anonymous block
          anonymousBlock->InsertFrames2(aPresContext, aPresShell, nsnull,
                                        nsnull, frames.FirstChild());
#ifdef NOISY_ANON_BLOCK
          printf("InsertBlockFrames: case 5\n");
#endif
        }
        else {
          // There are no anonymous blocks so create one and place the
          // frames into it.
          nsIFrame* anonymousBlock;
          rv = CreateAnonymousBlock(aPresContext, aFrameList, &anonymousBlock);
          if (NS_FAILED(rv)) {
            return rv;
          }

          // Insert the frame into the correct parent (it will not be
          // this frame when aPrevFrame's parent != this)
          flow = (nsInlineFrame*) prevFrameParent;
          flow->mFrames.InsertFrames(flow, aPrevFrame, anonymousBlock);
          generateReflowCommand = PR_TRUE;
          target = flow;
#ifdef NOISY_ANON_BLOCK
          printf("InsertBlockFrames: case 6\n");
#endif
        }
      }
    }
  }

  if (generateReflowCommand) {
    // generate a reflow command for "this"
    nsIReflowCommand* reflowCmd = nsnull;
    rv = NS_NewHTMLReflowCommand(&reflowCmd, target,
                                 nsIReflowCommand::ReflowDirty);
    if (NS_SUCCEEDED(rv)) {
      aPresShell.AppendReflowCommand(reflowCmd);
      NS_RELEASE(reflowCmd);
    }
  }
  return rv;
}

nsresult
nsInlineFrame::InsertInlineFrames(nsIPresContext& aPresContext,
                                  nsIPresShell& aPresShell,
                                  nsIFrame* aPrevFrame,
                                  nsIFrame* aFrameList)
{
  nsresult rv = NS_OK;
  PRBool generateReflowCommand = PR_FALSE;
  nsIFrame* target = nsnull;

  if (nsnull == aPrevFrame) {
    // Insert the frames at the front of our list. Since the frames
    // are all inline frames we just place them there, even if our
    // current first frame is the anonymous block frame. The reflow
    // logic will properly push the anonymous block frame to a
    // next-in-flow.
    mFrames.InsertFrames(this, nsnull, aFrameList);
    generateReflowCommand = PR_TRUE;
    target = this;
#ifdef NOISY_ANON_BLOCK
    printf("InsertInlineFrames: case 1\n");
#endif
  }
  else {
    nsIFrame* prevFrameParent;
    aPrevFrame->GetParent(&prevFrameParent);
    if (nsLineLayout::TreatFrameAsBlock(prevFrameParent)) {
      nsAnonymousBlockFrame* anonymousBlock;
      anonymousBlock = (nsAnonymousBlockFrame*) prevFrameParent;

      // The previous frame's parent is an anonymous block (otherwise
      // its parent would be this frame). We must not place the inline
      // frames into the anonymous block if they should be in
      // section3.
      nsIFrame* nextSibling;
      aPrevFrame->GetNextSibling(&nextSibling);
      nsIFrame* anonymousBlockNextInFlow;
      prevFrameParent->GetNextInFlow(&anonymousBlockNextInFlow);
      if ((nsnull != nextSibling) || (nsnull != anonymousBlockNextInFlow)) {
        // Easy case: there are more frames following aPrevFrame which
        // means that this insertion lies in the anonymous block.
        nsIFrame* frame = aFrameList;
        while (nsnull != frame) {
          frame->SetParent(anonymousBlock);
          frame->GetNextSibling(&frame);
        }
        anonymousBlock->InsertFrames2(aPresContext, aPresShell, nsnull,
                                      aPrevFrame, aFrameList);
#ifdef NOISY_ANON_BLOCK
        printf("InsertInlineFrames: case 2\n");
#endif
      }
      else {
        // aPrevFrame is the last frame that should be in the
        // anonymous block.
        nsInlineFrame* anonymousBlockParent;
        anonymousBlock->GetParent((nsIFrame**)&anonymousBlockParent);

        // Place the inline frames after the anonymous block
        nsIFrame* frame = aFrameList;
        while (nsnull != frame) {
          frame->SetParent(anonymousBlockParent);
          frame->GetNextSibling(&frame);
        }
        anonymousBlockParent->mFrames.InsertFrames(nsnull, anonymousBlock,
                                                   aFrameList);
        generateReflowCommand = PR_TRUE;
        target = anonymousBlockParent;
#ifdef NOISY_ANON_BLOCK
        printf("InsertInlineFrames: case 3\n");
#endif
      }
    }
    else {
      // The previous frame's parent is an inline frame. Therefore
      // this is either a section1 or section3 insertion. Insert the
      // frames in the proper flow block (which will be aPrevFrame's
      // parent which is currently stored in anonymousBlock)
      nsInlineFrame* flow = (nsInlineFrame*) prevFrameParent;
      flow->mFrames.InsertFrames(flow, aPrevFrame, aFrameList);
      generateReflowCommand = PR_TRUE;
      target = flow;
#ifdef NOISY_ANON_BLOCK
      printf("InsertInlineFrames: case 4\n");
#endif
    }
  }

  if (generateReflowCommand) {
    // generate a reflow command for "this"
    nsIReflowCommand* reflowCmd = nsnull;
    rv = NS_NewHTMLReflowCommand(&reflowCmd, target,
                                 nsIReflowCommand::ReflowDirty);
    if (NS_SUCCEEDED(rv)) {
      aPresShell.AppendReflowCommand(reflowCmd);
      NS_RELEASE(reflowCmd);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsInlineFrame::RemoveFrame(nsIPresContext& aPresContext,
                           nsIPresShell& aPresShell,
                           nsIAtom* aListName,
                           nsIFrame* aOldFrame)
{
  if (nsnull != aListName) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv = NS_OK;
  PRBool generateReflowCommand = PR_FALSE;
  nsIFrame* target = nsnull;

  nsIFrame* oldFrameParent;
  if (ParentIsInlineFrame(aOldFrame, &oldFrameParent)) {
    // Loop and destroy the frame and all of its
    // continuations. Because the frame's parent is an inline frame we
    // know that any continuations will also be in an inline frame
    // parent.
    nsInlineFrame* parent = (nsInlineFrame*) oldFrameParent;
    while (nsnull != aOldFrame) {
      // If the frame being removed has zero size then don't bother
      // generating a reflow command.
      nsRect bbox;
      aOldFrame->GetRect(bbox);
      if ((0 == bbox.width) && (0 == bbox.height)) {
        // Don't bother generating a reflow command
      }
      else {
        generateReflowCommand = PR_TRUE;
        target = this;
      }

      // When the parent is an inline frame we have a simple task -
      // just remove the frame from its parents list and generate a
      // reflow command.
      nsIFrame* oldFrameNextInFlow;
      aOldFrame->GetNextInFlow(&oldFrameNextInFlow);
      nsSplittableType st;
      aOldFrame->IsSplittable(st);
      if (NS_FRAME_NOT_SPLITTABLE != st) {
        aOldFrame->RemoveFromFlow();
      }
      parent->mFrames.DeleteFrame(aPresContext, aOldFrame);
      aOldFrame = oldFrameNextInFlow;
      if (nsnull != aOldFrame) {
        aOldFrame->GetParent((nsIFrame**) &parent);
      }
    }
#ifdef NOISY_ANON_BLOCK
    printf("RemoveFrame: case 1\n");
#endif
  }
  else {
#ifdef DEBUG
    nsIFrame* oldFrameNextInFlow;
    aOldFrame->GetNextInFlow(&oldFrameNextInFlow);
    NS_ASSERTION(nsnull == oldFrameNextInFlow, "XXX: can't remove continued frames that are in anonymous blocks -- not yet implemented");
#endif

    nsIFrame* nextInFlow;
    nsIFrame* prevInFlow;

    // The parent is not an inline frame which means it is an
    // anonymous block frame.
    nsAnonymousBlockFrame*  anonymousBlock =
      (nsAnonymousBlockFrame*) oldFrameParent;

    // It is possible that we are about to remove the last child of
    // the anonymous block. In this case we remove the anonymous block.
    nsIFrame* kids;
    anonymousBlock->FirstChild(nsnull, &kids);
    nsFrameList blockKids(kids);
    if (1 == blockKids.GetLength()) {
      // Remove the anonymous block
      mFrames.DeleteFrame(aPresContext, anonymousBlock);
      generateReflowCommand = PR_TRUE;
      target = this;
#ifdef NOISY_ANON_BLOCK
      printf("RemoveFrame: case 2\n");
#endif
    }
    else {
      // If the frame being removed is a block frame then we may need to
      // do something fancy.
      if (nsLineLayout::TreatFrameAsBlock(aOldFrame)) {
        // It is possible that we are removing the first block in the
        // anonymous block or the last block. See if its so.
        anonymousBlock->GetPrevInFlow(&prevInFlow);
        nsIFrame* prevSib;
        if ((nsnull != prevInFlow) ||
            (nsnull != (prevSib = blockKids.GetPrevSiblingFor(aOldFrame)))) {
          // There is a block in the anonymous block prior to the
          // block that we are removing. See if we are removing the
          // last block in the anonymous block.
          anonymousBlock->GetNextInFlow(&nextInFlow);
          nsIFrame* nextSib;
          aOldFrame->GetNextSibling(&nextSib);
          if ((nsnull != nextInFlow) || (nsnull != nextSib)) {
            // There is a block in the anonymous block after the block
            // that we are removing. This means that we can let the
            // anonymous block remove the frame.
#ifdef NOISY_ANON_BLOCK
            printf("RemoveFrame: case 3\n");
#endif
            anonymousBlock->RemoveFrame2(aPresContext, aPresShell, aListName,
                                         aOldFrame);
          }
          else {
            // We are removing the last block. We must steal all of
            // the inline frames that preceed the block being removed
            // (up to the closest prior block) so that they can be
            // moved outside the anonymous block and into an inline
            // frame.

            // First get the last frame out of the picture; delete any
            // continuations it might have.
            nsInlineFrame* anonymousBlockParent;
            anonymousBlock->GetParent((nsIFrame**) &anonymousBlockParent);
            nsAnonymousBlockFrame* ab = anonymousBlock;
            anonymousBlock->RemoveFramesFrom(aOldFrame);
            aOldFrame->DeleteFrame(aPresContext);
            while (nsnull != nextInFlow) {
              nsIFrame* nextParent;
              nextInFlow->GetParent(&nextParent);
              if (nextParent != ab) {
                ab = (nsAnonymousBlockFrame*) nextParent;
              }
              ab->RemoveFirstFrame();
              nsIFrame* nextNextInFlow;
              nextInFlow->GetNextInFlow(&nextNextInFlow);
              nextInFlow->DeleteFrame(aPresContext);
              nextInFlow = nextNextInFlow;
            }

            // Any inline frames that are between the new last-block
            // inside the anonymous block and the block we just
            // removed need to be taken out of the anonymous block.
            nsFrameList inlines;
            while (nsnull != anonymousBlock) {
              // Find the first inline before the last block
              nsIFrame* kids;
              anonymousBlock->FirstChild(nsnull, &kids);
              if (nsnull != kids) {
                SectionData sd(kids);
                if (sd.HasABlock()) {
                  kids = sd.lastBlock;
                  kids->GetNextSibling(&kids);
                  if (nsnull != kids) {
                    // Take the frames that follow the last block
                    // (which are inline frames) and remove them from
                    // the anonymous block. Insert them into the
                    // inlines frame-list.
                    anonymousBlock->RemoveFramesFrom(kids);
                    inlines.InsertFrames(nsnull, nsnull, kids);
                  }
                }
                else {
                  // All of the frames are inline frames -- take them
                  // all away.
                  anonymousBlock->RemoveFramesFrom(kids);
                  inlines.InsertFrames(nsnull, nsnull, kids);
                }
              }
              anonymousBlock->GetPrevInFlow((nsIFrame**) &anonymousBlock);
            }

            // Now we have all of the inline frames that need to be
            // placed into an inline parent instead of the anonymous
            // block parent.
            if (inlines.NotEmpty()) {
              // Place the inline frames after the anonymous block
              // frame in the child list of the anonymousBlockParent.
              anonymousBlockParent->mFrames.AppendFrames(anonymousBlockParent,
                                                         inlines);
            }
            generateReflowCommand = PR_TRUE;
            target = anonymousBlockParent;
#ifdef NOISY_ANON_BLOCK
            printf("RemoveFrame: case 4\n");
#endif
          }
        }
        else {
          // We are removing the first block child of the anonymous
          // block.  We must gather all of the inline frames that
          // follow the block being removed from the anonymous block
          // so that they can be moved outside the anonymous block and
          // into an inline frame.

          // Take away the first frame from the anonymous block (which
          // is the frame we are trying to remove). Make sure we
          // remove aOldFrame's continuations if it has any...
          nsInlineFrame* anonymousBlockParent;
          anonymousBlock->GetParent((nsIFrame**) &anonymousBlockParent);
          anonymousBlock->RemoveFirstFrame();
          aOldFrame->GetNextInFlow(&nextInFlow);
          aOldFrame->DeleteFrame(aPresContext);
          while (nsnull != nextInFlow) {
            nsIFrame* nextParent;
            nextInFlow->GetParent(&nextParent);
            if (nextParent != anonymousBlock) {
              anonymousBlock = (nsAnonymousBlockFrame*) nextParent;
            }
            anonymousBlock->RemoveFirstFrame();
            nsIFrame* nextNextInFlow;
            nextInFlow->GetNextInFlow(&nextNextInFlow);
            nextInFlow->DeleteFrame(aPresContext);
            nextInFlow = nextNextInFlow;
          }

          // Gather up the inline frames that follow aOldFrame
          nsFrameList frames;
          PRBool done = PR_FALSE;
          while (!done && (nsnull != anonymousBlock)) {
            nsIFrame* kid;
            anonymousBlock->FirstChild(nsnull, &kid);
            while (nsnull != kid) {
              if (nsLineLayout::TreatFrameAsBlock(kid)) {
                done = PR_TRUE;
                break;
              }
              nsIFrame* next;
              kid->GetNextSibling(&next);
              anonymousBlock->RemoveFirstFrame();
              frames.AppendFrame(nsnull, kid);
              kid = next;
            }
            anonymousBlock->GetNextInFlow((nsIFrame**) &anonymousBlock);
          }

          if (frames.NotEmpty()) {
            // If the anonymousBlockParent has a prev-in-flow then
            // append the inline frames there, otherwise insert them
            // before the anonymousBlock.
            anonymousBlockParent->GetPrevInFlow(&prevInFlow);
            if (nsnull != prevInFlow) {
              anonymousBlockParent = (nsInlineFrame*) prevInFlow;
              anonymousBlockParent->mFrames.AppendFrames(anonymousBlockParent,
                                                         frames);
            }
            else {
              anonymousBlockParent->mFrames.InsertFrames(anonymousBlockParent,
                                                         nsnull,
                                                         frames);
            }
          }
          generateReflowCommand = PR_TRUE;
          target = anonymousBlockParent;
#ifdef NOISY_ANON_BLOCK
          printf("RemoveFrame: case 5\n");
#endif
        }
      }
      else {
        // We can let the anonymousBlock remove the frame directly
#ifdef NOISY_ANON_BLOCK
        printf("RemoveFrame: case 6\n");
#endif
        anonymousBlock->RemoveFrame2(aPresContext, aPresShell, aListName,
                                     aOldFrame);
      }
    }
  }

  if (generateReflowCommand) {
    // generate a reflow command for "this"
    nsIReflowCommand* reflowCmd = nsnull;
    rv = NS_NewHTMLReflowCommand(&reflowCmd, target,
                                 nsIReflowCommand::ReflowDirty);
    if (NS_SUCCEEDED(rv)) {
      aPresShell.AppendReflowCommand(reflowCmd);
      NS_RELEASE(reflowCmd);
    }
  }

  return NS_OK;
}

//////////////////////////////////////////////////////////////////////
// Reflow methods

NS_IMETHODIMP
nsInlineFrame::Reflow(nsIPresContext&          aPresContext,
                      nsHTMLReflowMetrics&     aMetrics,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus)
{
  if (nsnull == aReflowState.lineLayout) {
    return NS_ERROR_INVALID_ARG;
  }
  DrainOverflow();

  // Set our own reflow state (additional state above and beyond
  // aReflowState)
  InlineReflowState irs;
  irs.mPrevFrame = nsnull;
  irs.mNextInFlow = (nsInlineFrame*) mNextInFlow;
  if (eReflowReason_Incremental == aReflowState.reason) {
    // Peel off the next frame in the path if this is an incremental
    // reflow aimed at one of the children.
    nsIFrame* target;
    aReflowState.reflowCommand->GetTarget(target);
    if (this != target) {
      aReflowState.reflowCommand->GetNext(irs.mNextRCFrame);
    }
  }
  else {
    irs.mNextRCFrame = nsnull;
  }

  nsresult rv;
  if (mFrames.IsEmpty()) {
    // Try to pull over one frame before starting so that we know what
    // state we should be in.
    nsIFrame* frame = PullAnyFrame(irs);
    if (nsnull == frame) {
      // Nothing to pull, nothing to do...
      aStatus = NS_FRAME_COMPLETE;
      aMetrics.width = 0;
      aMetrics.height = 0;
      aMetrics.ascent = 0;
      aMetrics.descent = 0;
      aMetrics.mCarriedOutTopMargin = 0;
      aMetrics.mCarriedOutBottomMargin = 0;
      aMetrics.mCombinedArea.SetRect(0, 0, 0, 0);
      if (nsnull != aMetrics.maxElementSize) {
        aMetrics.maxElementSize->SizeTo(0, 0);
      }
      mState &= ~NS_FRAME_OUTSIDE_CHILDREN;
      return NS_OK;
    }
  }

  if (HaveAnonymousBlock()) {
    if (!aReflowState.lineLayout->LineIsEmpty()) {
      // This inline frame cannot be placed on the current line
      // because there already is an inline frame on this line (and we
      // contain an anonymous block).
      aStatus = NS_INLINE_LINE_BREAK_BEFORE();
      rv = NS_OK;
    }
    else {
      rv = ReflowBlockFrame(aPresContext, aReflowState, irs,
                            aMetrics, aStatus);

      // If the combined area of our children exceeds our bounding box
      // then set the NS_FRAME_OUTSIDE_CHILDREN flag, otherwise clear
      // it.
      if ((aMetrics.mCombinedArea.x < 0) ||
          (aMetrics.mCombinedArea.y < 0) ||
          (aMetrics.mCombinedArea.XMost() > aMetrics.width) ||
          (aMetrics.mCombinedArea.YMost() > aMetrics.height)) {
        mState |= NS_FRAME_OUTSIDE_CHILDREN;
      }
      else {
        mState &= ~NS_FRAME_OUTSIDE_CHILDREN;
      }
    }
  }
  else {
    rv = ReflowInlineFrames(aPresContext, aReflowState, irs,
                            aMetrics, aStatus);
    // Note: when we are reflowing inline frames the line layout code
    // will properly compute our NS_FRAME_OUTSIDE_CHILDREN state for
    // us.
  }

  return rv;
}

NS_IMETHODIMP
nsInlineFrame::FindTextRuns(nsLineLayout& aLineLayout)
{
  if (HaveAnonymousBlock()) {
    aLineLayout.EndTextRun();
  }
  else {
    nsIFrame* frame = mFrames.FirstChild();
    while (nsnull != frame) {
      nsIHTMLReflow* ihr;
      nsresult rv = frame->QueryInterface(kIHTMLReflowIID, (void**)&ihr);
      if (NS_SUCCEEDED(rv)) {
        ihr->FindTextRuns(aLineLayout);
      }
      frame->GetNextSibling(&frame);
    }
  }
  return NS_OK;
}

void
nsInlineFrame::DrainOverflow()
{
  PRBool changedFirstFrame = PR_FALSE;

  // Check for an overflow list with our prev-in-flow
  nsInlineFrame* prevInFlow = (nsInlineFrame*)mPrevInFlow;
  if (nsnull != prevInFlow) {
    if (prevInFlow->mOverflowFrames.NotEmpty()) {
      mFrames.InsertFrames(this, nsnull, prevInFlow->mOverflowFrames);
      changedFirstFrame = PR_TRUE;
    }
  }

  // It's also possible that we have an overflow list for ourselves
  if (mOverflowFrames.NotEmpty()) {
    NS_ASSERTION(mFrames.NotEmpty(), "overflow list w/o frames");
    mFrames.Join(nsnull, mOverflowFrames);
  }
}

nsresult
nsInlineFrame::ReflowInlineFrames(nsIPresContext& aPresContext,
                                  const nsHTMLReflowState& aReflowState,
                                  InlineReflowState& irs,
                                  nsHTMLReflowMetrics& aMetrics,
                                  nsReflowStatus& aStatus)
{
  nsresult rv = NS_OK;
  aStatus = NS_FRAME_COMPLETE;

  nsLineLayout* lineLayout = aReflowState.lineLayout;
  nscoord leftEdge = 0;
  if (nsnull == mPrevInFlow) {
    leftEdge = aReflowState.mComputedBorderPadding.left;
  }
  nscoord availableWidth = aReflowState.availableWidth;
  if (NS_UNCONSTRAINEDSIZE != availableWidth) {
    // Subtract off left and right border+padding from availableWidth
    availableWidth -= leftEdge;
    availableWidth -= aReflowState.mComputedBorderPadding.right;
  }
  lineLayout->BeginSpan(this, &aReflowState,
                        leftEdge, leftEdge + availableWidth);

  // First reflow our current children
  nsIFrame* frame = mFrames.FirstChild();
  PRBool done = PR_FALSE;
  while (nsnull != frame) {
    rv = ReflowInlineFrame(aPresContext, aReflowState, irs, frame, aStatus);
    if (NS_FAILED(rv) || (NS_FRAME_COMPLETE != aStatus)) {
      done = PR_TRUE;
      break;
    }
    irs.mPrevFrame = frame;
    frame->GetNextSibling(&frame);
  }

  // Attempt to pull frames from our next-in-flow until we can't
  if (!done && (nsnull != mNextInFlow)) {
    while (!done) {
      PRBool isComplete;
      frame = PullInlineFrame(irs, &isComplete);
      if (nsnull == frame) {
        if (!isComplete) {
          aStatus = NS_FRAME_NOT_COMPLETE;
        }
        break;
      }
      rv = ReflowInlineFrame(aPresContext, aReflowState, irs, frame, aStatus);
      if (NS_FAILED(rv) || (NS_FRAME_COMPLETE != aStatus)) {
        done = PR_TRUE;
        break;
      }
      irs.mPrevFrame = frame;
    }
  }
#ifdef DEBUG
  if (NS_FRAME_COMPLETE == aStatus) {
    // We can't be complete AND have overflow frames!
    NS_ASSERTION(mOverflowFrames.IsEmpty(), "whoops");
  }
#endif

  // If after reflowing our children they take up no area then make
  // sure that we don't either.
  nsSize size;
  lineLayout->EndSpan(this, size, aMetrics.maxElementSize);
  if ((0 == size.height) && (0 == size.width)) {
    aMetrics.width = 0;
    aMetrics.height = 0;
    aMetrics.ascent = 0;
    aMetrics.descent = 0;
    if (nsnull != aMetrics.maxElementSize) {
      aMetrics.maxElementSize->width = 0;
      aMetrics.maxElementSize->height = 0;
    }
  }
  else {
    // Compute final width
    aMetrics.width = size.width;
    if (nsnull == mPrevInFlow) {
      aMetrics.width += aReflowState.mComputedBorderPadding.left;
    }
    if (NS_FRAME_IS_COMPLETE(aStatus)) {
      aMetrics.width += aReflowState.mComputedBorderPadding.right;
    }

    // Compute final height. The height of our box is the sum of our
    // font size plus the top and bottom border and padding. The height
    // of children do not affect our height.
    //
    // Note 2: we use the actual font height for sizing our selves instead
    // of the computed font height. On systems where they disagree the
    // actual font height is more appropriate.
    const nsStyleFont* font;
    GetStyleData(eStyleStruct_Font, (const nsStyleStruct*&)font);
    aReflowState.rendContext->SetFont(font->mFont);
    nsIFontMetrics* fm;
    aReflowState.rendContext->GetFontMetrics(fm);
    fm->GetMaxAscent(aMetrics.ascent);
    fm->GetMaxDescent(aMetrics.descent);
    fm->GetHeight(aMetrics.height);
    aMetrics.ascent += aReflowState.mComputedBorderPadding.top;
    aMetrics.descent += aReflowState.mComputedBorderPadding.bottom;
    aMetrics.height += aReflowState.mComputedBorderPadding.top +
      aReflowState.mComputedBorderPadding.bottom;

#if defined(XP_UNIX) || defined(XP_PC)
    static PRBool useComputedHeight = PR_FALSE;
    static PRBool firstTime = 1;
    if (firstTime) {
      if (getenv("GECKO_USE_COMPUTED_HEIGHT")) {
        useComputedHeight = PR_TRUE;
      }
    }
    if (useComputedHeight) {
      // Special debug code that violates the above CSS2 spec
      // clarification. Why? So that we can predictably compute the values
      // for testing layout.
      nscoord computedHeight = aReflowState.mComputedBorderPadding.top +
        aReflowState.mComputedBorderPadding.bottom +
        font->mFont.size;
      if (computedHeight != aMetrics.height) {
        if (0 == (mState & 0x80000000)) {
          nsFrame::ListTag(stdout, this);
          printf(": using computedHeight %d instead of actual height %d\n",
                 computedHeight, aMetrics.height);
          mState |= 0x80000000;
        }
        aMetrics.height = computedHeight;
      }
    }
#endif

    NS_RELEASE(fm);
  }

  // For now our combined area is zero. The real value will be
  // computed during vertical alignment of the line we are on.
  aMetrics.mCombinedArea.x = 0;
  aMetrics.mCombinedArea.y = 0;
  aMetrics.mCombinedArea.width = aMetrics.width;
  aMetrics.mCombinedArea.height = aMetrics.height;

#ifdef NOISY_FINAL_SIZE
  ListTag(stdout);
  printf(": metrics=%d,%d ascent=%d descent=%d\n",
         aMetrics.width, aMetrics.height, aMetrics.ascent, aMetrics.descent);
#endif

  return rv;
}

nsresult
nsInlineFrame::ReflowInlineFrame(nsIPresContext& aPresContext,
                                 const nsHTMLReflowState& aReflowState,
                                 InlineReflowState& irs,
                                 nsIFrame* aFrame,
                                 nsReflowStatus& aStatus)
{
  // Make sure that we don't reflow a block frame when we run across
  // one. This can easily happen if this inline has a mixture of
  // frames (note that an anonymous block frame is used to wrap up the
  // direct block children of this inline therefore when we do run
  // across a block frame its an anonymous block).
  if (nsLineLayout::TreatFrameAsBlock(aFrame)) {
    NS_ASSERTION(aFrame != mFrames.FirstChild(), "bad anon-block status");
    PushFrames(aFrame, irs.mPrevFrame);
    aStatus = NS_INLINE_LINE_BREAK_AFTER(NS_FRAME_NOT_COMPLETE);
    return NS_OK;
  }

  nsLineLayout* lineLayout = aReflowState.lineLayout;
  nsresult rv = lineLayout->ReflowFrame(aFrame, &irs.mNextRCFrame, aStatus);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (NS_INLINE_IS_BREAK(aStatus)) {
    if (NS_INLINE_IS_BREAK_BEFORE(aStatus)) {
      if (aFrame != mFrames.FirstChild()) {
        // Change break-before status into break-after since we have
        // already placed at least one child frame. This preserves the
        // break-type so that it can be propogated upward.
        aStatus = NS_FRAME_NOT_COMPLETE |
          NS_INLINE_BREAK | NS_INLINE_BREAK_AFTER |
          (aStatus & NS_INLINE_BREAK_TYPE_MASK);
        PushFrames(aFrame, irs.mPrevFrame);
      }
      else {
        // Preserve reflow status when breaking-before our first child
        // and propogate it upward without modification.
      }
    }
    else {
      // Break-after
      if (NS_FRAME_IS_NOT_COMPLETE(aStatus)) {
        nsIFrame* newFrame;
        rv = CreateNextInFlow(aPresContext, this, aFrame, newFrame);
        if (NS_FAILED(rv)) {
          return rv;
        }
      }
      nsIFrame* nextFrame;
      aFrame->GetNextSibling(&nextFrame);
      if (nsnull != nextFrame) {
        aStatus |= NS_FRAME_NOT_COMPLETE;
        PushFrames(nextFrame, aFrame);
      }
      else if (nsnull != mNextInFlow) {
        // We must return an incomplete status if there are more child
        // frames remaining in a next-in-flow that follows this frame.
        nsInlineFrame* nextInFlow = (nsInlineFrame*) mNextInFlow;
        while (nsnull != nextInFlow) {
          if (nextInFlow->mFrames.NotEmpty()) {
            aStatus |= NS_FRAME_NOT_COMPLETE;
            break;
          }
          nextInFlow = (nsInlineFrame*) nextInFlow->mNextInFlow;
        }
      }
    }
  }
  else if (NS_FRAME_IS_NOT_COMPLETE(aStatus)) {
    nsIFrame* newFrame;
    rv = CreateNextInFlow(aPresContext, this, aFrame, newFrame);
    if (NS_FAILED(rv)) {
      return rv;
    }
    nsIFrame* nextFrame;
    aFrame->GetNextSibling(&nextFrame);
    if (nsnull != nextFrame) {
      PushFrames(nextFrame, aFrame);
    }
  }
  return rv;
}

nsIFrame*
nsInlineFrame::PullInlineFrame(InlineReflowState& irs, PRBool* aIsComplete)
{
  PRBool isComplete = PR_TRUE;

  nsIFrame* frame = nsnull;
  nsInlineFrame* nextInFlow = irs.mNextInFlow;
  while (nsnull != nextInFlow) {
    if (nextInFlow->HaveAnonymousBlock()) {
      isComplete = PR_FALSE;
      break;
    }
    frame = mFrames.PullFrame(this, irs.mPrevFrame, nextInFlow->mFrames);
    if (nsnull != frame) {
      isComplete = PR_FALSE;
      break;
    }
    nextInFlow = (nsInlineFrame*) nextInFlow->mNextInFlow;
    irs.mNextInFlow = nextInFlow;
  }

  *aIsComplete = isComplete;
  return frame;
}

nsIFrame*
nsInlineFrame::PullAnyFrame(InlineReflowState& irs)
{
  nsIFrame* frame = nsnull;
  nsInlineFrame* nextInFlow = irs.mNextInFlow;
  while (nsnull != nextInFlow) {
    frame = mFrames.PullFrame(this, irs.mPrevFrame, nextInFlow->mFrames);
    if (nsnull != frame) {
      break;
    }

    nextInFlow = (nsInlineFrame*) nextInFlow->mNextInFlow;
    irs.mNextInFlow = nextInFlow;
  }

  return frame;
}

void
nsInlineFrame::PushFrames(nsIFrame* aFromChild, nsIFrame* aPrevSibling)
{
  NS_PRECONDITION(nsnull != aFromChild, "null pointer");
  NS_PRECONDITION(nsnull != aPrevSibling, "pushing first child");
#ifdef DEBUG
  nsIFrame* prevNextSibling;
  aPrevSibling->GetNextSibling(&prevNextSibling);
  NS_PRECONDITION(prevNextSibling == aFromChild, "bad prev sibling");
#endif

  // Disconnect aFromChild from its previous sibling
  aPrevSibling->SetNextSibling(nsnull);

  // Add the frames to our overflow list (let our next in flow drain
  // our overflow list when it is ready)
  NS_ASSERTION(mOverflowFrames.IsEmpty(), "bad overflow list");
  mOverflowFrames.SetFrames(aFromChild);
}

nsresult
nsInlineFrame::ReflowBlockFrame(nsIPresContext& aPresContext,
                                const nsHTMLReflowState& aReflowState,
                                InlineReflowState& irs,
                                nsHTMLReflowMetrics& aMetrics,
                                nsReflowStatus& aStatus)
{
  nsIFrame* blockFrame = mFrames.FirstChild();

  // Compute available area
#if XXX_what_to_do
  nscoord x = aReflowState.mComputedBorderPadding.left;
  nscoord availableWidth = aReflowState.availableWidth;
  if (NS_UNCONSTRAINEDSIZE != availableWidth) {
    if (nsnull != mPrevInFlow) {
      x = 0;
      availableWidth -= aReflowState.mComputedBorderPadding.right;
    }
    else {
      availableWidth -= aReflowState.mComputedBorderPadding.left +
        aReflowState.mComputedBorderPadding.right;
    }
  }
  nscoord y = aReflowState.mComputedBorderPadding.top;
  nscoord availableHeight = aReflowState.availableHeight;
  if (NS_UNCONSTRAINEDSIZE != aReflowState.availableHeight) {
    availableHeight -= aReflowState.mComputedBorderPadding.top +
      aReflowState.mComputedBorderPadding.right;
  }
#else
  nscoord x = 0;
  nscoord availableWidth = aReflowState.availableWidth;
  nscoord y = 0;
  nscoord availableHeight = aReflowState.availableHeight;
#endif

  // Reflow the block frame
  nsBlockReflowContext bc(aPresContext, aReflowState,
                          nsnull != aMetrics.maxElementSize);
  bc.SetNextRCFrame(irs.mNextRCFrame);
  nsRect availSpace(x, y, availableWidth, availableHeight);
  PRBool isAdjacentWithTop = PR_FALSE;
  nsMargin computedOffsets;
  nsresult rv = bc.ReflowBlock(blockFrame, availSpace,
#ifdef SPECULATIVE_TOP_MARGIN
                               PR_FALSE, 0,
#endif
                               isAdjacentWithTop,
                               computedOffsets, aStatus);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (NS_INLINE_IS_BREAK_BEFORE(aStatus)) {
    // We need to break before placing the block so propogate that
    // status outward to our parent.
  }
  else {
    // Place the block (during placement we might discover that none
    // of it fits)
    nsRect bounds;
    PRBool anyFit = bc.PlaceBlock(isAdjacentWithTop,
#ifndef SPECULATIVE_TOP_MARGIN
                                  PR_FALSE, 0,
#endif
                                  computedOffsets,
                                  bounds, aMetrics.mCombinedArea);
    if (!anyFit) {
      // None of the block fit
      aStatus = NS_INLINE_LINE_BREAK_BEFORE();
    }
    else {
      nsReflowStatus complete = NS_FRAME_COMPLETE;
      if (NS_FRAME_IS_NOT_COMPLETE(aStatus)) {
        // When the block isn't complete create a continuation for it
        nsIFrame* newFrame;
        rv = CreateNextInFlow(aPresContext, this, blockFrame, newFrame);
        if (NS_FAILED(rv)) {
          return rv;
        }
        complete = NS_FRAME_NOT_COMPLETE;
      }

      // It's possible that the block frame is followed by one or more
      // inline frames. Make sure we reflect that in our reflow status
      // and push them to our next-in-flow.
      nsIFrame* nextFrame;
      blockFrame->GetNextSibling(&nextFrame);
      if (nsnull != nextFrame) {
        PushFrames(nextFrame, blockFrame);
        aStatus |= NS_FRAME_NOT_COMPLETE;
      }
      else if (NS_FRAME_IS_COMPLETE(aStatus)) {
        // When the block we reflowed is complete then we need to
        // check and see if there are other frames (inline frames)
        // following in our continuations so that we return the proper
        // reflow status.
        nsInlineFrame* nextInFlow = (nsInlineFrame*) mNextInFlow;
        while (nsnull != nextInFlow) {
          if (nextInFlow->mFrames.NotEmpty() ||
              nextInFlow->mOverflowFrames.NotEmpty()) {
            aStatus |= NS_FRAME_NOT_COMPLETE;
            break;
          }
          nextInFlow = (nsInlineFrame*) nextInFlow->mNextInFlow;
        }
      }

      // Map reflow status so that nothing ends up on the same line as
      // this frame.
      aStatus = NS_INLINE_LINE_BREAK_AFTER(complete);

      // What we do here is to fudge the size. <b>This</b> frame will
      // be 0,0 but will contain a single child (the anonymous block)
      // that is properly sized.
      aMetrics.width = 0;
      aMetrics.height = 0;
      aMetrics.ascent = 0;
      aMetrics.descent = 0;
      aMetrics.mCarriedOutTopMargin = bc.GetCollapsedTopMargin();
      aMetrics.mCarriedOutBottomMargin = bc.GetCarriedOutBottomMargin();
      if (nsnull != aMetrics.maxElementSize) {
        *aMetrics.maxElementSize = bc.GetMaxElementSize();
      }
    }
  }

  return NS_OK;
}

//////////////////////////////////////////////////////////////////////

PRIntn
nsInlineFrame::GetSkipSides() const
{
  PRIntn skip = 0;
  if (nsnull != mPrevInFlow) {
    skip |= 1 << NS_SIDE_LEFT;
  }
  if (nsnull != mNextInFlow) {
    skip |= 1 << NS_SIDE_RIGHT;
  }
  return skip;
}

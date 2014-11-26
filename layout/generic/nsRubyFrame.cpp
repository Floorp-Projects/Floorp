/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "display: ruby" */
#include "nsRubyFrame.h"
#include "nsLineLayout.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "WritingModes.h"
#include "nsRubyBaseContainerFrame.h"
#include "nsRubyTextContainerFrame.h"

using namespace mozilla;

//----------------------------------------------------------------------

// Frame class boilerplate
// =======================

NS_QUERYFRAME_HEAD(nsRubyFrame)
  NS_QUERYFRAME_ENTRY(nsRubyFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

NS_IMPL_FRAMEARENA_HELPERS(nsRubyFrame)

nsContainerFrame*
NS_NewRubyFrame(nsIPresShell* aPresShell,
                nsStyleContext* aContext)
{
  return new (aPresShell) nsRubyFrame(aContext);
}

//----------------------------------------------------------------------

// nsRubyFrame Method Implementations
// ==================================

nsIAtom*
nsRubyFrame::GetType() const
{
  return nsGkAtoms::rubyFrame;
}

/* virtual */ bool
nsRubyFrame::IsFrameOfType(uint32_t aFlags) const
{
  return nsContainerFrame::IsFrameOfType(aFlags &
    ~(nsIFrame::eLineParticipant));
}

#ifdef DEBUG_FRAME_DUMP
nsresult
nsRubyFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Ruby"), aResult);
}
#endif

class MOZ_STACK_CLASS TextContainerIterator
{
public:
  TextContainerIterator(nsRubyBaseContainerFrame* aBaseContainer);
  void Next();
  bool AtEnd() const { return !mFrame; }
  nsRubyTextContainerFrame* GetTextContainer() const
  {
    return static_cast<nsRubyTextContainerFrame*>(mFrame);
  }

private:
  nsIFrame* mFrame;
};

TextContainerIterator::TextContainerIterator(
    nsRubyBaseContainerFrame* aBaseContainer)
{
  mFrame = aBaseContainer;
  Next();
}

void
TextContainerIterator::Next()
{
  if (mFrame) {
    mFrame = mFrame->GetNextSibling();
    if (mFrame && mFrame->GetType() != nsGkAtoms::rubyTextContainerFrame) {
      mFrame = nullptr;
    }
  }
}

/**
 * This class is responsible for appending and clearing
 * text container list of the base container.
 */
class MOZ_STACK_CLASS AutoSetTextContainers
{
public:
  AutoSetTextContainers(nsRubyBaseContainerFrame* aBaseContainer);
  ~AutoSetTextContainers();

private:
  nsRubyBaseContainerFrame* mBaseContainer;
};

AutoSetTextContainers::AutoSetTextContainers(
    nsRubyBaseContainerFrame* aBaseContainer)
  : mBaseContainer(aBaseContainer)
{
#ifdef DEBUG
  aBaseContainer->AssertTextContainersEmpty();
#endif
  for (TextContainerIterator iter(aBaseContainer);
       !iter.AtEnd(); iter.Next()) {
    aBaseContainer->AppendTextContainer(iter.GetTextContainer());
  }
}

AutoSetTextContainers::~AutoSetTextContainers()
{
  mBaseContainer->ClearTextContainers();
}

/**
 * This enumerator enumerates each segment.
 */
class MOZ_STACK_CLASS SegmentEnumerator
{
public:
  SegmentEnumerator(nsRubyFrame* aRubyFrame);

  void Next();
  bool AtEnd() const { return !mBaseContainer; }

  nsRubyBaseContainerFrame* GetBaseContainer() const
  {
    return mBaseContainer;
  }

private:
  nsRubyBaseContainerFrame* mBaseContainer;
};

SegmentEnumerator::SegmentEnumerator(nsRubyFrame* aRubyFrame)
{
  nsIFrame* frame = aRubyFrame->GetFirstPrincipalChild();
  MOZ_ASSERT(!frame ||
             frame->GetType() == nsGkAtoms::rubyBaseContainerFrame);
  mBaseContainer = static_cast<nsRubyBaseContainerFrame*>(frame);
}

void
SegmentEnumerator::Next()
{
  MOZ_ASSERT(mBaseContainer);
  nsIFrame* frame = mBaseContainer->GetNextSibling();
  while (frame && frame->GetType() != nsGkAtoms::rubyBaseContainerFrame) {
    frame = frame->GetNextSibling();
  }
  mBaseContainer = static_cast<nsRubyBaseContainerFrame*>(frame);
}

/* virtual */ void
nsRubyFrame::AddInlineMinISize(nsRenderingContext *aRenderingContext,
                               nsIFrame::InlineMinISizeData *aData)
{
  nscoord max = 0;
  for (SegmentEnumerator e(this); !e.AtEnd(); e.Next()) {
    AutoSetTextContainers holder(e.GetBaseContainer());
    max = std::max(max, e.GetBaseContainer()->GetMinISize(aRenderingContext));
  }
  aData->currentLine += max;
}

/* virtual */ void
nsRubyFrame::AddInlinePrefISize(nsRenderingContext *aRenderingContext,
                                nsIFrame::InlinePrefISizeData *aData)
{
  nscoord sum = 0;
  for (SegmentEnumerator e(this); !e.AtEnd(); e.Next()) {
    AutoSetTextContainers holder(e.GetBaseContainer());
    sum += e.GetBaseContainer()->GetPrefISize(aRenderingContext);
  }
  aData->currentLine += sum;
}

/* virtual */ nscoord
nsRubyFrame::GetLogicalBaseline(WritingMode aWritingMode) const
{
  return mBaseline;
}

/* virtual */ bool
nsRubyFrame::CanContinueTextRun() const
{
  return true;
}

/* virtual */ void
nsRubyFrame::Reflow(nsPresContext* aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsRubyFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  
  if (!aReflowState.mLineLayout) {
    NS_ASSERTION(aReflowState.mLineLayout,
                 "No line layout provided to RubyFrame reflow method.");
    aStatus = NS_FRAME_COMPLETE;
    return;
  }

  // Begin the span for the ruby frame
  WritingMode frameWM = aReflowState.GetWritingMode();
  WritingMode lineWM = aReflowState.mLineLayout->GetWritingMode();
  LogicalMargin borderPadding = aReflowState.ComputedLogicalBorderPadding();
  nscoord availableISize = aReflowState.AvailableISize();
  NS_ASSERTION(availableISize != NS_UNCONSTRAINEDSIZE,
               "should no longer use available widths");
  // Subtract off inline axis border+padding from availableISize
  availableISize -= borderPadding.IStartEnd(frameWM);
  aReflowState.mLineLayout->BeginSpan(this, &aReflowState,
                                      borderPadding.IStart(frameWM),
                                      availableISize, &mBaseline);

  // FIXME: line breaking / continuations not yet implemented
  aStatus = NS_FRAME_COMPLETE;
  LogicalSize availSize(lineWM, aReflowState.AvailableISize(),
                        aReflowState.AvailableBSize());
  // The ruby base container for the current segment
  nsRubyBaseContainerFrame* segmentRBC = nullptr;
  nscoord annotationBSize = 0;
  for (nsFrameList::Enumerator e(mFrames); !e.AtEnd(); e.Next()) {
    nsIFrame* childFrame = e.get();
    if (childFrame->GetType() == nsGkAtoms::rubyBaseContainerFrame) {
      if (segmentRBC) {
        annotationBSize = 0;
      }

      // Figure out what all the text containers are for this segment (necessary
      // for reflow calculations)
      segmentRBC = do_QueryFrame(childFrame);
      nsFrameList::Enumerator segment(e);
      segment.Next();
      while (!segment.AtEnd() && (segment.get()->GetType() !=
             nsGkAtoms::rubyBaseContainerFrame)) {
        if (segment.get()->GetType() == nsGkAtoms::rubyTextContainerFrame) {
          segmentRBC->AppendTextContainer(segment.get());
        }
        segment.Next();
      }

      nsReflowStatus frameReflowStatus;
      nsHTMLReflowMetrics metrics(aReflowState, aDesiredSize.mFlags);
      nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                         childFrame, availSize);
      childReflowState.mLineLayout = aReflowState.mLineLayout;
      childFrame->Reflow(aPresContext, metrics, childReflowState,
                         frameReflowStatus);
      NS_ASSERTION(frameReflowStatus == NS_FRAME_COMPLETE,
                 "Ruby line breaking is not yet implemented");
      childFrame->SetSize(LogicalSize(lineWM, metrics.ISize(lineWM),
                                      metrics.BSize(lineWM)));
      FinishReflowChild(childFrame, aPresContext, metrics, &childReflowState, 0,
                        0, NS_FRAME_NO_MOVE_FRAME | NS_FRAME_NO_MOVE_VIEW);

    } else if (childFrame->GetType() == nsGkAtoms::rubyTextContainerFrame) {
      nsReflowStatus frameReflowStatus;
      nsHTMLReflowMetrics metrics(aReflowState, aDesiredSize.mFlags);
      nsHTMLReflowState childReflowState(aPresContext, aReflowState, childFrame,
                                         availSize);
      childReflowState.mLineLayout = aReflowState.mLineLayout;
      childFrame->Reflow(aPresContext, metrics, childReflowState,
                      frameReflowStatus);
      NS_ASSERTION(frameReflowStatus == NS_FRAME_COMPLETE,
                 "Ruby line breaking is not yet implemented");
      annotationBSize += metrics.BSize(lineWM);
      childFrame->SetSize(LogicalSize(lineWM, metrics.ISize(lineWM),
                                      metrics.BSize(lineWM)));
      // FIXME: This is a temporary calculation for finding the block coordinate
      // of the ruby text container. A better one replace it once it's been
      // spec'ed.
      nscoord baseContainerBCoord; 
      if (segmentRBC) {
        // Find the starting block coordinate of the ruby base container for
        // this segment. For now, the ruby text container will be placed so that
        // its bottom edge touches this coordinate.
        baseContainerBCoord = segmentRBC->
          GetLogicalPosition(this->GetParent()->GetLogicalSize().ISize(lineWM)).
          B(lineWM);
      } else {
        baseContainerBCoord = 0;
      }
      FinishReflowChild(childFrame, aPresContext, metrics, &childReflowState, 0,
                        baseContainerBCoord - metrics.BSize(lineWM), 0);
    } else {
      NS_NOTREACHED(
        "Unrecognized child type for ruby frame will not be reflowed."); 
    }
  }

  // Null the pointers between child frames.
  for (nsFrameList::Enumerator children(mFrames); !children.AtEnd();
       children.Next()) {
    nsRubyBaseContainerFrame* rbcFrame = do_QueryFrame(children.get());
    if (rbcFrame) {
      rbcFrame->ClearTextContainers();
    }
  }

  aDesiredSize.ISize(lineWM) = aReflowState.mLineLayout->EndSpan(this);
  nsLayoutUtils::SetBSizeFromFontMetrics(this, aDesiredSize, aReflowState,
                                         borderPadding, lineWM, frameWM);
}

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLContainerFrame_h___
#define nsMathMLContainerFrame_h___

#include "mozilla/Attributes.h"
#include "nsContainerFrame.h"
#include "nsBlockFrame.h"
#include "nsInlineFrame.h"
#include "nsMathMLOperators.h"
#include "nsMathMLFrame.h"
#include "mozilla/Likely.h"

/*
 * Base class for MathML container frames. It acts like an inferred 
 * mrow. By default, this frame uses its Reflow() method to lay its 
 * children horizontally and ensure that their baselines are aligned.
 * The Reflow() method relies upon Place() to position children.
 * By overloading Place() in derived classes, it is therefore possible
 * to position children in various customized ways.
 */

// Options for the preferred size at which to stretch our stretchy children 
#define STRETCH_CONSIDER_ACTUAL_SIZE    0x00000001 // just use our current size
#define STRETCH_CONSIDER_EMBELLISHMENTS 0x00000002 // size calculations include embellishments

class nsMathMLContainerFrame : public nsContainerFrame,
                               public nsMathMLFrame {
  friend class nsMathMLmfencedFrame;
public:
  explicit nsMathMLContainerFrame(nsStyleContext* aContext)
    : nsContainerFrame(aContext)
    , mIntrinsicWidth(NS_INTRINSIC_WIDTH_UNKNOWN)
  {}

  NS_DECL_QUERYFRAME_TARGET(nsMathMLContainerFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_ABSTRACT_FRAME(nsMathMLContainerFrame)

  // --------------------------------------------------------------------------
  // Overloaded nsMathMLFrame methods -- see documentation in nsIMathMLFrame.h

  NS_IMETHOD
  Stretch(nsRenderingContext& aRenderingContext,
          nsStretchDirection   aStretchDirection,
          nsBoundingMetrics&   aContainerSize,
          nsHTMLReflowMetrics& aDesiredStretchSize) override;

  NS_IMETHOD
  UpdatePresentationDataFromChildAt(int32_t         aFirstIndex,
                                    int32_t         aLastIndex,
                                    uint32_t        aFlagsValues,
                                    uint32_t        aFlagsToUpdate) override
  {
    PropagatePresentationDataFromChildAt(this, aFirstIndex, aLastIndex,
      aFlagsValues, aFlagsToUpdate);
    return NS_OK;
  }
  
  // helper to set the "increment script level" flag on the element belonging
  // to a child frame given by aChildIndex.
  // When this flag is set, the style system will increment the scriptlevel
  // for the child element. This is needed for situations where the style system
  // cannot itself determine the scriptlevel (mfrac, munder, mover, munderover).
  // This should be called during reflow. We set the flag and if it changed,
  // we request appropriate restyling and also queue a post-reflow callback
  // to ensure that restyle and reflow happens immediately after the current
  // reflow.
  void
  SetIncrementScriptLevel(int32_t aChildIndex, bool aIncrement);

  // --------------------------------------------------------------------------
  // Overloaded nsContainerFrame methods -- see documentation in nsIFrame.h

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return !(aFlags & nsIFrame::eLineParticipant) &&
      nsContainerFrame::IsFrameOfType(aFlags &
              ~(nsIFrame::eMathML | nsIFrame::eExcludesIgnorableWhitespace));
  }

  virtual void
  AppendFrames(ChildListID     aListID,
               nsFrameList&    aFrameList) override;

  virtual void
  InsertFrames(ChildListID     aListID,
               nsIFrame*       aPrevFrame,
               nsFrameList&    aFrameList) override;

  virtual void
  RemoveFrame(ChildListID     aListID,
              nsIFrame*       aOldFrame) override;

  /**
   * Both GetMinISize and GetPrefISize use the intrinsic width metrics
   * returned by GetIntrinsicMetrics, including ink overflow.
   */
  virtual nscoord GetMinISize(nsRenderingContext* aRenderingContext) override;
  virtual nscoord GetPrefISize(nsRenderingContext* aRenderingContext) override;

  /**
   * Return the intrinsic horizontal metrics of the frame's content area.
   */
  virtual void
  GetIntrinsicISizeMetrics(nsRenderingContext* aRenderingContext,
                           nsHTMLReflowMetrics& aDesiredSize);

  virtual void
  Reflow(nsPresContext*          aPresContext,
         nsHTMLReflowMetrics&     aDesiredSize,
         const nsHTMLReflowState& aReflowState,
         nsReflowStatus&          aStatus) override;

  virtual void DidReflow(nsPresContext*           aPresContext,
            const nsHTMLReflowState*  aReflowState,
            nsDidReflowStatus         aStatus) override

  {
    mPresentationData.flags &= ~NS_MATHML_STRETCH_DONE;
    return nsContainerFrame::DidReflow(aPresContext, aReflowState, aStatus);
  }

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  virtual bool UpdateOverflow() override;

  virtual void MarkIntrinsicISizesDirty() override;

  // Notification when an attribute is changed. The MathML module uses the
  // following paradigm:
  //
  // 1. If the MathML frame class doesn't have any cached automatic data that
  //    depends on the attribute: we just reflow (e.g., this happens with <msub>,
  //    <msup>, <mmultiscripts>, etc). This is the default behavior implemented
  //    by this base class.
  //
  // 2. If the MathML frame class has cached automatic data that depends on
  //    the attribute:
  //    2a. If the automatic data to update resides only within the descendants,
  //        we just re-layout them using ReLayoutChildren(this);
  //        (e.g., this happens with <ms>).
  //    2b. If the automatic data to update affects us in some way, we ask our parent
  //        to re-layout its children using ReLayoutChildren(mParent);
  //        Therefore, there is an overhead here in that our siblings are re-laid
  //        too (e.g., this happens with <munder>, <mover>, <munderover>). 
  virtual nsresult
  AttributeChanged(int32_t         aNameSpaceID,
                   nsIAtom*        aAttribute,
                   int32_t         aModType) override;

  // helper function to apply mirroring to a horizontal coordinate, if needed.
  nscoord
  MirrorIfRTL(nscoord aParentWidth, nscoord aChildWidth, nscoord aChildLeading)
  {
    return (StyleVisibility()->mDirection ?
            aParentWidth - aChildWidth - aChildLeading : aChildLeading);
  }

  // --------------------------------------------------------------------------
  // Additional methods 

protected:
  /* Place :
   * This method is used to measure or position child frames and other
   * elements.  It may be called any number of times with aPlaceOrigin
   * false to measure, and the final call of the Reflow process before
   * returning from Reflow() or Stretch() will have aPlaceOrigin true
   * to position the elements.
   *
   * IMPORTANT: This method uses GetReflowAndBoundingMetricsFor() which must
   * have been set up with SaveReflowAndBoundingMetricsFor().
   *
   * The Place() method will use this information to compute the desired size
   * of the frame.
   *
   * @param aPlaceOrigin [in]
   *        If aPlaceOrigin is false, compute your desired size using the
   *        information from GetReflowAndBoundingMetricsFor.  However, child
   *        frames or other elements should not be repositioned.
   *
   *        If aPlaceOrigin is true, reflow is finished. You should position
   *        all your children, and return your desired size. You should now
   *        use FinishReflowChild() on your children to complete post-reflow
   *        operations.
   *
   * @param aDesiredSize [out] parameter where you should return your desired
   *        size and your ascent/descent info. Compute your desired size using
   *        the information from GetReflowAndBoundingMetricsFor, and include
   *        any space you want for border/padding in the desired size you
   *        return.
   */
  virtual nsresult
  Place(nsRenderingContext& aRenderingContext,
        bool                 aPlaceOrigin,
        nsHTMLReflowMetrics& aDesiredSize);

  // MeasureForWidth:
  //
  // A method used by nsMathMLContainerFrame::GetIntrinsicISize to get the
  // width that a particular Place method desires.  For most frames, this will
  // just call the object's Place method.  However <msqrt> and <menclose> use
  // nsMathMLContainerFrame::GetIntrinsicISize to measure the child frames as
  // if in an <mrow>, and so their frames implement MeasureForWidth to use
  // nsMathMLContainerFrame::Place.
  virtual nsresult
  MeasureForWidth(nsRenderingContext& aRenderingContext,
                  nsHTMLReflowMetrics& aDesiredSize);


  // helper to re-sync the automatic data in our children and notify our parent to
  // reflow us when changes (e.g., append/insert/remove) happen in our child list
  virtual nsresult
  ChildListChanged(int32_t aModType);

  // helper to get the preferred size that a container frame should use to fire
  // the stretch on its stretchy child frames.
  void
  GetPreferredStretchSize(nsRenderingContext& aRenderingContext,
                          uint32_t             aOptions,
                          nsStretchDirection   aStretchDirection,
                          nsBoundingMetrics&   aPreferredStretchSize);

  // helper used by mstyle, mphantom, mpadded and mrow in their implementation
  // of TransmitAutomaticData() to determine whether they are space-like.
  nsresult
  TransmitAutomaticDataForMrowLikeElement();

public:
  // error handlers to provide a visual feedback to the user when an error
  // (typically invalid markup) was encountered during reflow.
  nsresult
  ReflowError(nsRenderingContext& aRenderingContext,
              nsHTMLReflowMetrics& aDesiredSize);
  /*
   * Helper to call ReportErrorToConsole for parse errors involving 
   * attribute/value pairs.
   * @param aAttribute The attribute for which the parse error occured.
   * @param aValue The value for which the parse error occured.
   */
  nsresult
  ReportParseError(const char16_t*           aAttribute,
                   const char16_t*           aValue);

  /*
   * Helper to call ReportErrorToConsole when certain tags
   * have more than the expected amount of children.
   */
  nsresult
  ReportChildCountError();

  /*
   * Helper to call ReportErrorToConsole when certain tags have
   * invalid child tags
   * @param aChildTag The tag which is forbidden in this context
   */
  nsresult
  ReportInvalidChildError(nsIAtom* aChildTag);

  /*
   * Helper to call ReportToConsole when an error occurs.
   * @param aParams see nsContentUtils::ReportToConsole
   */
  nsresult
  ReportErrorToConsole(const char*       aErrorMsgId,
                       const char16_t** aParams = nullptr,
                       uint32_t          aParamCount = 0);

  // helper method to reflow a child frame. We are inline frames, and we don't
  // know our positions until reflow is finished. That's why we ask the
  // base method not to worry about our position.
  void
  ReflowChild(nsIFrame*                aKidFrame,
              nsPresContext*          aPresContext,
              nsHTMLReflowMetrics&     aDesiredSize,
              const nsHTMLReflowState& aReflowState,
              nsReflowStatus&          aStatus);

protected:
  // helper to add the inter-spacing when <math> is the immediate parent.
  // Since we don't (yet) handle the root <math> element ourselves, we need to
  // take special care of the inter-frame spacing on elements for which <math>
  // is the direct xml parent. This function will be repeatedly called from
  // left to right on the childframes of <math>, and by so doing it will
  // emulate the spacing that would have been done by a <mrow> container.
  // e.g., it fixes <math> <mi>f</mi> <mo>q</mo> <mi>f</mi> <mo>I</mo> </math>
  virtual nscoord
  FixInterFrameSpacing(nsHTMLReflowMetrics& aDesiredSize);

  // helper method to complete the post-reflow hook and ensure that embellished
  // operators don't terminate their Reflow without receiving a Stretch command.
  virtual nsresult
  FinalizeReflow(nsRenderingContext& aRenderingContext,
                 nsHTMLReflowMetrics& aDesiredSize);

  // Record metrics of a child frame for recovery through the following method
  static void
  SaveReflowAndBoundingMetricsFor(nsIFrame*                  aFrame,
                                  const nsHTMLReflowMetrics& aReflowMetrics,
                                  const nsBoundingMetrics&   aBoundingMetrics);

  // helper method to facilitate getting the reflow and bounding metrics of a
  // child frame.  The argument aMathMLFrameType, when non null, will return
  // the 'type' of the frame, which is used to determine the inter-frame
  // spacing.
  // IMPORTANT: This function is only meant to be called in Place() methods as
  // the information is available only when set up with the above method
  // during Reflow/Stretch() and GetPrefISize().
  static void
  GetReflowAndBoundingMetricsFor(nsIFrame*            aFrame,
                                 nsHTMLReflowMetrics& aReflowMetrics,
                                 nsBoundingMetrics&   aBoundingMetrics,
                                 eMathMLFrameType*    aMathMLFrameType = nullptr);

  // helper method to clear metrics saved with
  // SaveReflowAndBoundingMetricsFor() from all child frames.
  void ClearSavedChildMetrics();

  // helper to let the update of presentation data pass through
  // a subtree that may contain non-MathML container frames
  static void
  PropagatePresentationDataFor(nsIFrame*       aFrame,
                               uint32_t        aFlagsValues,
                               uint32_t        aFlagsToUpdate);

public:
  static void
  PropagatePresentationDataFromChildAt(nsIFrame*       aParentFrame,
                                       int32_t         aFirstChildIndex,
                                       int32_t         aLastChildIndex,
                                       uint32_t        aFlagsValues,
                                       uint32_t        aFlagsToUpdate);

  // Sets flags on aFrame and all descendant frames
  static void
  PropagateFrameFlagFor(nsIFrame* aFrame,
                        nsFrameState aFlags);

  // helper to let the rebuild of automatic data (presentation data
  // and embellishement data) walk through a subtree that may contain
  // non-MathML container frames. Note that this method re-builds the
  // automatic data in the children -- not in aParentFrame itself (except
  // for those particular operations that the parent frame may do in its
  // TransmitAutomaticData()). The reason it works this way is because
  // a container frame knows what it wants for its children, whereas children
  // have no clue who their parent is. For example, it is <mfrac> who knows
  // that its children have to be in scriptsizes, and has to transmit this
  // information to them. Hence, when changes occur in a child frame, the child
  // has to request the re-build from its parent. Unfortunately, the extra cost
  // for this is that it will re-sync in the siblings of the child as well.
  static void
  RebuildAutomaticDataForChildren(nsIFrame* aParentFrame);

  // helper to blow away the automatic data cached in a frame's subtree and
  // re-layout its subtree to reflect changes that may have happen. In the
  // event where aParentFrame isn't a MathML frame, it will first walk up to
  // the ancestor that is a MathML frame, and re-layout from there -- this is
  // to guarantee that automatic data will be rebuilt properly. Note that this
  // method re-builds the automatic data in the children -- not in the parent
  // frame itself (except for those particular operations that the parent frame
  // may do do its TransmitAutomaticData()). @see RebuildAutomaticDataForChildren
  //
  // aBits are the bits to pass to FrameNeedsReflow() when we call it.
  static nsresult
  ReLayoutChildren(nsIFrame* aParentFrame);

protected:
  // Helper method which positions child frames as an <mrow> on given baseline
  // y = aBaseline starting from x = aOffsetX, calling FinishReflowChild()
  // on the frames.
  void
  PositionRowChildFrames(nscoord aOffsetX, nscoord aBaseline);

  // A variant on FinishAndStoreOverflow() that uses the union of child
  // overflows, the frame bounds, and mBoundingMetrics to set and store the
  // overflow.
  void GatherAndStoreOverflow(nsHTMLReflowMetrics* aMetrics);

  /**
   * Call DidReflow() if the NS_FRAME_IN_REFLOW frame bit is set on aFirst and
   * all its next siblings up to, but not including, aStop.
   * aStop == nullptr meaning all next siblings with the bit set.
   * The method does nothing if aFirst == nullptr.
   */
  static void DidReflowChildren(nsIFrame* aFirst, nsIFrame* aStop = nullptr);

  /**
   * Recompute mIntrinsicWidth if it's not already up to date.
   */
  void UpdateIntrinsicWidth(nsRenderingContext* aRenderingContext);

  nscoord mIntrinsicWidth;

private:
  class RowChildFrameIterator;
  friend class RowChildFrameIterator;
};


// --------------------------------------------------------------------------
// Currently, to benefit from line-breaking inside the <math> element, <math> is
// simply mapping to nsBlockFrame or nsInlineFrame.
// A separate implemention needs to provide:
// 1) line-breaking
// 2) proper inter-frame spacing
// 3) firing of Stretch() (in which case FinalizeReflow() would have to be cleaned)
// Issues: If/when mathml becomes a pluggable component, the separation will be needed.
class nsMathMLmathBlockFrame : public nsBlockFrame {
public:
  NS_DECL_QUERYFRAME_TARGET(nsMathMLmathBlockFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  friend nsContainerFrame* NS_NewMathMLmathBlockFrame(nsIPresShell* aPresShell,
          nsStyleContext* aContext, nsFrameState aFlags);

  // beware, mFrames is not set by nsBlockFrame
  // cannot use mFrames{.FirstChild()|.etc} since the block code doesn't set mFrames
  virtual void
  SetInitialChildList(ChildListID     aListID,
                      nsFrameList&    aChildList) override
  {
    NS_ASSERTION(aListID == kPrincipalList, "unexpected frame list");
    nsBlockFrame::SetInitialChildList(aListID, aChildList);
    // re-resolve our subtree to set any mathml-expected data
    nsMathMLContainerFrame::RebuildAutomaticDataForChildren(this);
  }

  virtual void
  AppendFrames(ChildListID     aListID,
               nsFrameList&    aFrameList) override
  {
    NS_ASSERTION(aListID == kPrincipalList || aListID == kNoReflowPrincipalList,
                 "unexpected frame list");
    nsBlockFrame::AppendFrames(aListID, aFrameList);
    if (MOZ_LIKELY(aListID == kPrincipalList))
      nsMathMLContainerFrame::ReLayoutChildren(this);
  }

  virtual void
  InsertFrames(ChildListID     aListID,
               nsIFrame*       aPrevFrame,
               nsFrameList&    aFrameList) override
  {
    NS_ASSERTION(aListID == kPrincipalList || aListID == kNoReflowPrincipalList,
                 "unexpected frame list");
    nsBlockFrame::InsertFrames(aListID, aPrevFrame, aFrameList);
    if (MOZ_LIKELY(aListID == kPrincipalList))
      nsMathMLContainerFrame::ReLayoutChildren(this);
  }

  virtual void
  RemoveFrame(ChildListID     aListID,
              nsIFrame*       aOldFrame) override
  {
    NS_ASSERTION(aListID == kPrincipalList || aListID == kNoReflowPrincipalList,
                 "unexpected frame list");
    nsBlockFrame::RemoveFrame(aListID, aOldFrame);
    if (MOZ_LIKELY(aListID == kPrincipalList))
      nsMathMLContainerFrame::ReLayoutChildren(this);
  }

  virtual bool IsFrameOfType(uint32_t aFlags) const override {
    return nsBlockFrame::IsFrameOfType(aFlags &
              ~(nsIFrame::eMathML | nsIFrame::eExcludesIgnorableWhitespace));
  }

  // See nsIMathMLFrame.h
  bool IsMrowLike() {
    return mFrames.FirstChild() != mFrames.LastChild() ||
           !mFrames.FirstChild();
  }

protected:
  explicit nsMathMLmathBlockFrame(nsStyleContext* aContext) : nsBlockFrame(aContext) {
    // We should always have a float manager.  Not that things can really try
    // to float out of us anyway, but we need one for line layout.
    AddStateBits(NS_BLOCK_FLOAT_MGR);
  }
  virtual ~nsMathMLmathBlockFrame() {}
};

// --------------

class nsMathMLmathInlineFrame : public nsInlineFrame,
                                public nsMathMLFrame {
public:
  NS_DECL_QUERYFRAME_TARGET(nsMathMLmathInlineFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  friend nsContainerFrame* NS_NewMathMLmathInlineFrame(nsIPresShell* aPresShell,
                                                       nsStyleContext* aContext);

  virtual void
  SetInitialChildList(ChildListID     aListID,
                      nsFrameList&    aChildList) override
  {
    NS_ASSERTION(aListID == kPrincipalList, "unexpected frame list");
    nsInlineFrame::SetInitialChildList(aListID, aChildList);
    // re-resolve our subtree to set any mathml-expected data
    nsMathMLContainerFrame::RebuildAutomaticDataForChildren(this);
  }

  virtual void
  AppendFrames(ChildListID     aListID,
               nsFrameList&    aFrameList) override
  {
    NS_ASSERTION(aListID == kPrincipalList || aListID == kNoReflowPrincipalList,
                 "unexpected frame list");
    nsInlineFrame::AppendFrames(aListID, aFrameList);
    if (MOZ_LIKELY(aListID == kPrincipalList))
      nsMathMLContainerFrame::ReLayoutChildren(this);
  }

  virtual void
  InsertFrames(ChildListID     aListID,
               nsIFrame*       aPrevFrame,
               nsFrameList&    aFrameList) override
  {
    NS_ASSERTION(aListID == kPrincipalList || aListID == kNoReflowPrincipalList,
                 "unexpected frame list");
    nsInlineFrame::InsertFrames(aListID, aPrevFrame, aFrameList);
    if (MOZ_LIKELY(aListID == kPrincipalList))
      nsMathMLContainerFrame::ReLayoutChildren(this);
  }

  virtual void
  RemoveFrame(ChildListID     aListID,
              nsIFrame*       aOldFrame) override
  {
    NS_ASSERTION(aListID == kPrincipalList || aListID == kNoReflowPrincipalList,
                 "unexpected frame list");
    nsInlineFrame::RemoveFrame(aListID, aOldFrame);
    if (MOZ_LIKELY(aListID == kPrincipalList))
      nsMathMLContainerFrame::ReLayoutChildren(this);
  }

  virtual bool IsFrameOfType(uint32_t aFlags) const override {
      return nsInlineFrame::IsFrameOfType(aFlags &
                ~(nsIFrame::eMathML | nsIFrame::eExcludesIgnorableWhitespace));
  }

  bool
  IsMrowLike() override {
    return mFrames.FirstChild() != mFrames.LastChild() ||
           !mFrames.FirstChild();
  }

protected:
  explicit nsMathMLmathInlineFrame(nsStyleContext* aContext) : nsInlineFrame(aContext) {}
  virtual ~nsMathMLmathInlineFrame() {}
};

#endif /* nsMathMLContainerFrame_h___ */

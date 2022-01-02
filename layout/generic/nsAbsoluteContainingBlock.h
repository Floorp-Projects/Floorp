/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * code for managing absolutely positioned children of a rendering
 * object that is a containing block for them
 */

#ifndef nsAbsoluteContainingBlock_h___
#define nsAbsoluteContainingBlock_h___

#include "nsFrameList.h"
#include "nsIFrame.h"
#include "mozilla/TypedEnumBits.h"

class nsContainerFrame;
class nsPresContext;

/**
 * This class contains the logic for being an absolute containing block.  This
 * class is used within viewport frames (for frames representing content with
 * fixed position) and blocks (for frames representing absolutely positioned
 * content), since each set of frames is absolutely positioned with respect to
 * its parent.
 *
 * There is no principal child list, just a named child list which contains
 * the absolutely positioned frames (kAbsoluteList or kFixedList).
 *
 * All functions include as the first argument the frame that is delegating
 * the request.
 */
class nsAbsoluteContainingBlock {
  using ReflowInput = mozilla::ReflowInput;

 public:
  typedef nsIFrame::ChildListID ChildListID;

  explicit nsAbsoluteContainingBlock(ChildListID aChildListID)
#ifdef DEBUG
      : mChildListID(aChildListID)
#endif
  {
    MOZ_ASSERT(mChildListID == nsIFrame::kAbsoluteList ||
                   mChildListID == nsIFrame::kFixedList,
               "should either represent position:fixed or absolute content");
  }

  const nsFrameList& GetChildList() const { return mAbsoluteFrames; }
  void AppendChildList(nsTArray<nsIFrame::ChildList>* aLists,
                       ChildListID aListID) const {
    NS_ASSERTION(aListID == mChildListID, "wrong list ID");
    GetChildList().AppendIfNonempty(aLists, aListID);
  }

  void SetInitialChildList(nsIFrame* aDelegatingFrame, ChildListID aListID,
                           nsFrameList& aChildList);
  void AppendFrames(nsIFrame* aDelegatingFrame, ChildListID aListID,
                    nsFrameList& aFrameList);
  void InsertFrames(nsIFrame* aDelegatingFrame, ChildListID aListID,
                    nsIFrame* aPrevFrame, nsFrameList& aFrameList);
  void RemoveFrame(nsIFrame* aDelegatingFrame, ChildListID aListID,
                   nsIFrame* aOldFrame);

  enum class AbsPosReflowFlags {
    ConstrainHeight = 0x1,
    CBWidthChanged = 0x2,
    CBHeightChanged = 0x4,
    CBWidthAndHeightChanged = CBWidthChanged | CBHeightChanged,
    IsGridContainerCB = 0x8,
  };

  /**
   * Called by the delegating frame after it has done its reflow first. This
   * function will reflow any absolutely positioned child frames that need to
   * be reflowed, e.g., because the absolutely positioned child frame has
   * 'auto' for an offset, or a percentage based width or height.
   *
   * @param aOverflowAreas, if non-null, is unioned with (in the local
   * coordinate space) the overflow areas of the absolutely positioned
   * children.
   *
   * @param aReflowStatus is assumed to be already-initialized, e.g. with the
   * status of the delegating frame's main reflow. This function merges in the
   * statuses of the absolutely positioned children's reflows.
   *
   * @param aFlags zero or more AbsPosReflowFlags
   */
  void Reflow(nsContainerFrame* aDelegatingFrame, nsPresContext* aPresContext,
              const ReflowInput& aReflowInput, nsReflowStatus& aReflowStatus,
              const nsRect& aContainingBlock, AbsPosReflowFlags aFlags,
              mozilla::OverflowAreas* aOverflowAreas);

  using PostDestroyData = nsIFrame::PostDestroyData;
  void DestroyFrames(nsIFrame* aDelegatingFrame, nsIFrame* aDestructRoot,
                     PostDestroyData& aPostDestroyData);

  bool HasAbsoluteFrames() const { return mAbsoluteFrames.NotEmpty(); }

  /**
   * Mark our size-dependent absolute frames with NS_FRAME_HAS_DIRTY_CHILDREN
   * so that we'll make sure to reflow them.
   */
  void MarkSizeDependentFramesDirty();

  /**
   * Mark all our absolute frames with NS_FRAME_IS_DIRTY.
   */
  void MarkAllFramesDirty();

 protected:
  /**
   * Returns true if the position of aFrame depends on the position of
   * its placeholder or if the position or size of aFrame depends on a
   * containing block dimension that changed.
   */
  bool FrameDependsOnContainer(nsIFrame* aFrame, bool aCBWidthChanged,
                               bool aCBHeightChanged);

  /**
   * After an abspos child's size is known, this method can be used to
   * resolve size-dependent values in the ComputedLogicalOffsets on its
   * reflow input. (This may involve resolving the inline dimension of
   * aLogicalCBSize, too; hence, that variable is an in/outparam.)
   *
   * aKidSize, aMargin, aOffsets, and aLogicalCBSize are all expected to be
   * represented in terms of the absolute containing block's writing-mode.
   */
  void ResolveSizeDependentOffsets(nsPresContext* aPresContext,
                                   ReflowInput& aKidReflowInput,
                                   const mozilla::LogicalSize& aKidSize,
                                   const mozilla::LogicalMargin& aMargin,
                                   mozilla::LogicalMargin* aOffsets,
                                   mozilla::LogicalSize* aLogicalCBSize);

  /**
   * For frames that have intrinsic block sizes, since we want to use the
   * frame's actual instrinsic block-size, we don't compute margins in
   * InitAbsoluteConstraints because the block-size isn't computed yet. This
   * method computes the margins for them after layout.
   * aMargin and aOffsets are both outparams (though we only touch aOffsets if
   * the position is overconstrained)
   */
  void ResolveAutoMarginsAfterLayout(ReflowInput& aKidReflowInput,
                                     const mozilla::LogicalSize* aLogicalCBSize,
                                     const mozilla::LogicalSize& aKidSize,
                                     mozilla::LogicalMargin& aMargin,
                                     mozilla::LogicalMargin& aOffsets);

  void ReflowAbsoluteFrame(nsIFrame* aDelegatingFrame,
                           nsPresContext* aPresContext,
                           const ReflowInput& aReflowInput,
                           const nsRect& aContainingBlockRect,
                           AbsPosReflowFlags aFlags, nsIFrame* aKidFrame,
                           nsReflowStatus& aStatus,
                           mozilla::OverflowAreas* aOverflowAreas);

  /**
   * Mark our absolute frames dirty.
   * @param aMarkAllDirty if true, all will be marked with NS_FRAME_IS_DIRTY.
   * Otherwise, the size-dependant ones will be marked with
   * NS_FRAME_HAS_DIRTY_CHILDREN.
   */
  void DoMarkFramesDirty(bool aMarkAllDirty);

 protected:
  nsFrameList mAbsoluteFrames;  // additional named child list

#ifdef DEBUG
  ChildListID const mChildListID;  // kFixedList or kAbsoluteList
#endif
};

namespace mozilla {
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(
    nsAbsoluteContainingBlock::AbsPosReflowFlags)
}
#endif /* nsnsAbsoluteContainingBlock_h___ */

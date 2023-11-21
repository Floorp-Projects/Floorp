/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsILineIterator_h___
#define nsILineIterator_h___

#include "nscore.h"
#include "nsINode.h"
#include "nsRect.h"
#include "mozilla/Attributes.h"
#include "mozilla/Result.h"
#include "mozilla/WritingModes.h"

class nsIFrame;

/**
 * Line iterator API.
 *
 * Lines are numbered from 0 to N, where 0 is the top line and N is
 * the bottom line.
 *
 * Obtain this interface from frames via nsIFrame::GetLineIterator.
 * This iterator belongs to the frame from which it was obtained, and should
 * not be deleted by the caller.
 * Note that any modification of the frame will invalidate the iterator!
 * Users must get a new iterator any time the target may have been touched.
 */
class nsILineIterator {
 protected:
  ~nsILineIterator() = default;

 public:
  /**
   * The number of lines in the block
   */
  virtual int32_t GetNumLines() const = 0;

  /**
   * Returns whether our lines are rtl.
   */
  virtual bool IsLineIteratorFlowRTL() = 0;

  struct LineInfo {
    /** The first frame on the line. */
    nsIFrame* mFirstFrameOnLine = nullptr;
    /** The numbers of frames on the line. */
    int32_t mNumFramesOnLine = 0;
    /**
     * The bounding box of the line (which is based on the in-flow position of
     * the frames on the line; if a frame was moved because of relative
     * positioning then its coordinates may be outside the line bounds)
     */
    nsRect mLineBounds;
    /** Whether the line is wrapped at the end */
    bool mIsWrapped = false;

    /**
     * Return last frame of the line if there is no enough siblings of
     * mFirstFrameOnLine.
     * Otherwise, nullptr including in the unexpected error cases.
     */
    nsIFrame* GetLastFrameOnLine() const;
  };

  // Return miscellaneous information about a line.
  virtual mozilla::Result<LineInfo, nsresult> GetLine(int32_t aLineNumber) = 0;

  /**
   * Given a frame that's a child of the block, find which line its on
   * and return that line index, as long as it's at least as big as
   * aStartLine.  Returns -1 if the frame cannot be found on lines
   * starting with aStartLine.
   */
  virtual int32_t FindLineContaining(nsIFrame* aFrame,
                                     int32_t aStartLine = 0) = 0;

  // Given a line number and a coordinate, find the frame on the line
  // that is nearest to aPos along the inline axis. (The block-axis coord
  // of aPos is irrelevant.)
  // The aPosIsBeforeFirstFrame and aPosIsAfterLastFrame flags are updated
  // appropriately.
  NS_IMETHOD FindFrameAt(int32_t aLineNumber, nsPoint aPos,
                         nsIFrame** aFrameFound, bool* aPosIsBeforeFirstFrame,
                         bool* aPosIsAfterLastFrame) = 0;

  // Check whether visual and logical order of frames within a line are
  // identical.
  //  If not, return the first and last visual frames
  NS_IMETHOD CheckLineOrder(int32_t aLine, bool* aIsReordered,
                            nsIFrame** aFirstVisual,
                            nsIFrame** aLastVisual) = 0;
};

namespace mozilla {

// Helper struct for FindFrameAt.
struct LineFrameFinder {
  LineFrameFinder(const nsPoint& aPos, const nsSize& aContainerSize,
                  WritingMode aWM, bool aIsReversed)
      : mPos(aWM, aPos, aContainerSize),
        mContainerSize(aContainerSize),
        mWM(aWM),
        mIsReversed(aIsReversed) {}

  void Scan(nsIFrame*);
  void Finish(nsIFrame**, bool* aPosIsBeforeFirstFrame,
              bool* aPosIsAfterLastFrame);

  const LogicalPoint mPos;
  const nsSize mContainerSize;
  const WritingMode mWM;
  const bool mIsReversed;

  bool IsDone() const { return mDone; }

 private:
  bool mDone = false;
  nsIFrame* mFirstFrame = nullptr;
  nsIFrame* mClosestFromStart = nullptr;
  nsIFrame* mClosestFromEnd = nullptr;
};

}  // namespace mozilla

/**
 * Helper intended to be used in a scope where we're using an nsILineIterator
 * and want to verify that no DOM mutations (which would invalidate the
 * iterator) occur while we're using it.
 */
class MOZ_STACK_CLASS AutoAssertNoDomMutations final {
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  nsMutationGuard mGuard;
#endif
 public:
  ~AutoAssertNoDomMutations() { MOZ_DIAGNOSTIC_ASSERT(!mGuard.Mutated(0)); }
};

#endif /* nsILineIterator_h___ */

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsILineIterator_h___
#define nsILineIterator_h___

#include "nscore.h"
#include "nsRect.h"
#include "mozilla/Attributes.h"
#include "mozilla/Result.h"

class nsIFrame;

/**
 * Line iterator API.
 *
 * Lines are numbered from 0 to N, where 0 is the top line and N is
 * the bottom line.
 *
 * Obtain this interface from frames via nsIFrame::GetLineIterator.
 * When you are finished using the iterator, call DisposeLineIterator()
 * to destroy the iterator if appropriate.
 */
class nsILineIterator {
 protected:
  ~nsILineIterator() = default;

 public:
  virtual void DisposeLineIterator() = 0;

  /**
   * The number of lines in the block
   */
  virtual int32_t GetNumLines() const = 0;

  /**
   * The prevailing direction of lines.
   *
   * @return true if the CSS direction property for the block is
   *         "rtl", otherwise false
   *
   *XXX after bug 924851 change this to return a UBiDiDirection
   */
  virtual bool GetDirection() = 0;

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
    bool mIsWrapped;
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

class nsAutoLineIterator {
 public:
  nsAutoLineIterator() : mRawPtr(nullptr) {}
  MOZ_IMPLICIT nsAutoLineIterator(nsILineIterator* i) : mRawPtr(i) {}

  ~nsAutoLineIterator() {
    if (mRawPtr) mRawPtr->DisposeLineIterator();
  }

  operator const nsILineIterator*() const { return mRawPtr; }
  operator nsILineIterator*() { return mRawPtr; }
  const nsILineIterator* operator->() const { return mRawPtr; }
  nsILineIterator* operator->() { return mRawPtr; }

  nsILineIterator* operator=(nsILineIterator* i) {
    if (i == mRawPtr) return i;

    if (mRawPtr) mRawPtr->DisposeLineIterator();

    mRawPtr = i;
    return i;
  }

 private:
  nsILineIterator* mRawPtr;
};

#endif /* nsILineIterator_h___ */

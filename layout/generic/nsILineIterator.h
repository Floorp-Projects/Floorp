/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsILineIterator_h___
#define nsILineIterator_h___

#include "nscore.h"
#include "nsCoord.h"

class nsIFrame;
struct nsRect;

// Line Flags (see GetLine below)

// This bit is set when the line is wrapping up a block frame. When
// clear, it means that the line contains inline elements.
#define NS_LINE_FLAG_IS_BLOCK           0x1

// This bit is set when the line ends in some sort of break.
#define NS_LINE_FLAG_ENDS_IN_BREAK      0x4

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
class nsILineIterator
{
protected:
  ~nsILineIterator() { }

public:
  virtual void DisposeLineIterator() = 0;

  /**
   * The number of lines in the block
   */
  virtual PRInt32 GetNumLines() = 0;

  /**
   * The prevailing direction of lines.
   *
   * @return true if the CSS direction property for the block is
   *         "rtl", otherwise false
   */
  virtual bool GetDirection() = 0;

  // Return structural information about a line. aFirstFrameOnLine is
  // the first frame on the line and aNumFramesOnLine is the number of
  // frames that are on the line. If the line-number is invalid then
  // aFirstFrameOnLine will be nsnull and aNumFramesOnLine will be
  // zero.
  //
  // For valid line numbers, aLineBounds is set to the bounding box of
  // the line (which is based on the in-flow position of the frames on
  // the line; if a frame was moved because of relative positioning
  // then its coordinates may be outside the line bounds).
  //
  // In addition, aLineFlags will contain flag information about the
  // line.
  NS_IMETHOD GetLine(PRInt32 aLineNumber,
                     nsIFrame** aFirstFrameOnLine,
                     PRInt32* aNumFramesOnLine,
                     nsRect& aLineBounds,
                     PRUint32* aLineFlags) = 0;

  /**
   * Given a frame that's a child of the block, find which line its on
   * and return that line index, as long as it's at least as big as
   * aStartLine.  Returns -1 if the frame cannot be found on lines
   * starting with aStartLine.
   */
  virtual PRInt32 FindLineContaining(nsIFrame* aFrame,
                                     PRInt32 aStartLine = 0) = 0;

  // Given a line number and an X coordinate, find the frame on the
  // line that is nearest to the X coordinate. The
  // aXIsBeforeFirstFrame and aXIsAfterLastFrame flags are updated
  // appropriately.
  NS_IMETHOD FindFrameAt(PRInt32 aLineNumber,
                         nscoord aX,
                         nsIFrame** aFrameFound,
                         bool* aXIsBeforeFirstFrame,
                         bool* aXIsAfterLastFrame) = 0;

  // Give the line iterator implementor a chance todo something more complicated than
  // nsIFrame::GetNextSibling()
  NS_IMETHOD GetNextSiblingOnLine(nsIFrame*& aFrame, PRInt32 aLineNumber) = 0;

#ifdef IBMBIDI
  // Check whether visual and logical order of frames within a line are identical.
  //  If not, return the first and last visual frames
  NS_IMETHOD CheckLineOrder(PRInt32                  aLine,
                            bool                     *aIsReordered,
                            nsIFrame                 **aFirstVisual,
                            nsIFrame                 **aLastVisual) = 0;
#endif
};

class nsAutoLineIterator
{
public:
  nsAutoLineIterator() : mRawPtr(nsnull) { }
  nsAutoLineIterator(nsILineIterator *i) : mRawPtr(i) { }

  ~nsAutoLineIterator() {
    if (mRawPtr)
      mRawPtr->DisposeLineIterator();
  }

  operator nsILineIterator*() { return mRawPtr; }
  nsILineIterator* operator->() { return mRawPtr; }

  nsILineIterator* operator=(nsILineIterator* i) {
    if (i == mRawPtr)
      return i;

    if (mRawPtr)
      mRawPtr->DisposeLineIterator();

    mRawPtr = i;
    return i;
  }

private:
  nsILineIterator* mRawPtr;
};

#endif /* nsILineIterator_h___ */

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
 * The Original Code is Mozilla Communicator client code.
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
   * @return PR_TRUE if the CSS direction property for the block is
   *         "rtl", otherwise PR_FALSE
   */
  virtual PRBool GetDirection() = 0;

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
   * and return that line index. Returns -1 if the frame cannot be found.
   */
  virtual PRInt32 FindLineContaining(nsIFrame* aFrame) = 0;

  // Given a line number and an X coordinate, find the frame on the
  // line that is nearest to the X coordinate. The
  // aXIsBeforeFirstFrame and aXIsAfterLastFrame flags are updated
  // appropriately.
  NS_IMETHOD FindFrameAt(PRInt32 aLineNumber,
                         nscoord aX,
                         nsIFrame** aFrameFound,
                         PRBool* aXIsBeforeFirstFrame,
                         PRBool* aXIsAfterLastFrame) = 0;

  // Give the line iterator implementor a chance todo something more complicated than
  // nsIFrame::GetNextSibling()
  NS_IMETHOD GetNextSiblingOnLine(nsIFrame*& aFrame, PRInt32 aLineNumber) = 0;

#ifdef IBMBIDI
  // Check whether visual and logical order of frames within a line are identical.
  //  If not, return the first and last visual frames
  NS_IMETHOD CheckLineOrder(PRInt32                  aLine,
                            PRBool                   *aIsReordered,
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

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsHTMLReflowMetrics_h___
#define nsHTMLReflowMetrics_h___

#include <stdio.h>
#include "nsISupports.h"
#include "nsMargin.h"
#include "nsRect.h"
// for MOZ_MATHML
#include "nsIRenderingContext.h" //to get struct nsBoundingMetrics

struct nsSize;

//----------------------------------------------------------------------

// Option flags
#define NS_REFLOW_CALC_MAX_WIDTH         0x0001
#ifdef MOZ_MATHML
#define NS_REFLOW_CALC_BOUNDING_METRICS  0x0002
#endif

/**
 * An nsCollapsingMargin represents a vertical collapsing margin between
 * blocks as described in section 8.3.1 of CSS2,
 * <URL: http://www.w3.org/TR/REC-CSS2/box.html#collapsing-margins >.
 *
 * All adjacent vertical margins collapse, and the resulting margin is
 * the sum of the largest positive margin included and the smallest (most
 * negative) negative margin included.
 */
struct nsCollapsingMargin {
  private:
    nscoord mMostPos;  // the largest positive margin included
    nscoord mMostNeg;  // the smallest negative margin included

  public:
    nsCollapsingMargin()
        : mMostPos(0),
          mMostNeg(0)
      {
      }

    nsCollapsingMargin(const nsCollapsingMargin& aOther)
        : mMostPos(aOther.mMostPos),
          mMostNeg(aOther.mMostNeg)
      {
      }

    nsCollapsingMargin& operator=(const nsCollapsingMargin& aOther)
      {
        mMostPos = aOther.mMostPos;
        mMostNeg = aOther.mMostNeg;
        return *this;
      }

    void Include(nscoord aCoord)
      {
        if (aCoord > mMostPos)
          mMostPos = aCoord;
        else if (aCoord < mMostNeg)
          mMostNeg = aCoord;
      }

    void Zero()
      {
        mMostPos = 0;
        mMostNeg = 0;
      }

    PRBool IsZero() const
      {
        return (mMostPos == 0) && (mMostNeg == 0);
      }

    nscoord get() const
      {
        return mMostPos + mMostNeg;
      }
};

/**
 * Reflow metrics used to return the frame's desired size and alignment
 * information.
 *
 * @see #Reflow()
 */
struct nsHTMLReflowMetrics {
  nscoord width, height;        // [OUT] desired width and height
  nscoord ascent, descent;      // [OUT] ascent and descent information

  // Set this to null if you don't need to compute the max element size
  nsSize* maxElementSize;       // [OUT]

  // Used for incremental reflow. If the NS_REFLOW_CALC_MAX_WIDTH flag is set,
  // then the caller is requesting that you update and return your maximum width
  nscoord mMaximumWidth;        // [OUT]

#ifdef MOZ_MATHML
  // Metrics that _exactly_ enclose the text to allow precise MathML placements.
  // If the NS_REFLOW_CALC_BOUNDING_METRICS flag is set, then the caller is 
  // requesting that you also compute additional details about your inner
  // bounding box and italic correction. For example, the bounding box of
  // msup is the smallest rectangle that _exactly_ encloses both the text
  // of the base and the text of the superscript.
  nsBoundingMetrics mBoundingMetrics;  // [OUT]
#endif

  // Carried out bottom margin values. This is the collapsed
  // (generational) bottom margin value.
  nsCollapsingMargin mCarriedOutBottomMargin;
  
  // For frames that have content that overflow their content area
  // (NS_FRAME_OUTSIDE_CHILDREN) this rectangle represents the total area
  // of the frame including visible overflow, i.e., don't include overflowing
  // content that is hidden.
  // The rect is in the local coordinate space of the frame, and should be at
  // least as big as the desired size. If there is no content that overflows,
  // then the overflow area is identical to the desired size and should be
  // {0, 0, mWidth, mHeight}.
  nsRect mOverflowArea;

  PRUint32 mFlags;
 
  // used by tables to optimize common cases
  PRBool mNothingChanged;

  nsHTMLReflowMetrics(nsSize* aMaxElementSize, PRUint32 aFlags = 0) {
    maxElementSize = aMaxElementSize;
    mMaximumWidth = 0;
    mFlags = aFlags;
    mOverflowArea.x = 0;
    mOverflowArea.y = 0;
    mOverflowArea.width = 0;
    mOverflowArea.height = 0;
    mNothingChanged = PR_FALSE;
#ifdef MOZ_MATHML
    mBoundingMetrics.Clear();
#endif

    // XXX These are OUT parameters and so they shouldn't have to be
    // initialized, but there are some bad frame classes that aren't
    // properly setting them when returning from Reflow()...
    width = height = 0;
    ascent = descent = 0;
  }
  
  void AddBorderPaddingToMaxElementSize(const nsMargin& aBorderPadding) {
    maxElementSize->width += aBorderPadding.left + aBorderPadding.right;
    maxElementSize->height += aBorderPadding.top + aBorderPadding.bottom;
  }

  nsHTMLReflowMetrics& operator=(const nsHTMLReflowMetrics& aOther)
  {
    if (maxElementSize && aOther.maxElementSize) {
      maxElementSize->width = aOther.maxElementSize->width;
      maxElementSize->height = aOther.maxElementSize->height;
    }
    mMaximumWidth = aOther.mMaximumWidth;
    mFlags = aOther.mFlags;
    mCarriedOutBottomMargin = aOther.mCarriedOutBottomMargin;
    mOverflowArea.x = aOther.mOverflowArea.x;
    mOverflowArea.y = aOther.mOverflowArea.y;
    mOverflowArea.width = aOther.mOverflowArea.width;
    mOverflowArea.height = aOther.mOverflowArea.height;
    mNothingChanged = aOther.mNothingChanged;
#ifdef MOZ_MATHML
    mBoundingMetrics = aOther.mBoundingMetrics;
#endif

    width = aOther.width;
    height = aOther.height;
    ascent = aOther.ascent;
    descent = aOther.descent;
    return *this;
  }

};

#endif /* nsHTMLReflowMetrics_h___ */

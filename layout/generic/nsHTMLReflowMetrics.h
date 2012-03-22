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

/* struct containing the output from nsIFrame::Reflow */

#ifndef nsHTMLReflowMetrics_h___
#define nsHTMLReflowMetrics_h___

#include <stdio.h>
#include "nsISupports.h"
#include "nsMargin.h"
#include "nsRect.h"
#include "nsBoundingMetrics.h"

//----------------------------------------------------------------------

// Option flags
#define NS_REFLOW_CALC_BOUNDING_METRICS  0x0001

/**
 * When we store overflow areas as an array of scrollable and visual
 * overflow, we use these indices.
 *
 * eOverflowType_LENGTH is needed (for gcc 4.5.*, at least) to ensure
 * that 2 is a valid value of nsOverflowType for use in
 * NS_FOR_FRAME_OVERFLOW_TYPES.
 */
enum nsOverflowType { eVisualOverflow, eScrollableOverflow,
                      eOverflowType_LENGTH };

#define NS_FOR_FRAME_OVERFLOW_TYPES(var_)                                     \
  for (nsOverflowType var_ = nsOverflowType(0); var_ < 2;                     \
       var_ = nsOverflowType(var_ + 1))

struct nsOverflowAreas {
private:
  nsRect mRects[2];
public:
  nsRect& Overflow(size_t aIndex) {
    NS_ASSERTION(aIndex < 2, "index out of range");
    return mRects[aIndex];
  }
  const nsRect& Overflow(size_t aIndex) const {
    NS_ASSERTION(aIndex < 2, "index out of range");
    return mRects[aIndex];
  }

  nsRect& VisualOverflow() { return mRects[eVisualOverflow]; }
  const nsRect& VisualOverflow() const { return mRects[eVisualOverflow]; }

  nsRect& ScrollableOverflow() { return mRects[eScrollableOverflow]; }
  const nsRect& ScrollableOverflow() const { return mRects[eScrollableOverflow]; }

  nsOverflowAreas() {
    // default-initializes to zero due to nsRect's default constructor
  }

  nsOverflowAreas(const nsRect& aVisualOverflow,
                  const nsRect& aScrollableOverflow)
  {
    mRects[eVisualOverflow] = aVisualOverflow;
    mRects[eScrollableOverflow] = aScrollableOverflow;
  }

  nsOverflowAreas(const nsOverflowAreas& aOther) {
    *this = aOther;
  }

  nsOverflowAreas& operator=(const nsOverflowAreas& aOther) {
    mRects[0] = aOther.mRects[0];
    mRects[1] = aOther.mRects[1];
    return *this;
  }

  bool operator==(const nsOverflowAreas& aOther) const {
    // Scrollable overflow is a point-set rectangle and visual overflow
    // is a pixel-set rectangle.
    return VisualOverflow().IsEqualInterior(aOther.VisualOverflow()) &&
           ScrollableOverflow().IsEqualEdges(aOther.ScrollableOverflow());
  }

  bool operator!=(const nsOverflowAreas& aOther) const {
    return !(*this == aOther);
  }

  nsOverflowAreas operator+(const nsPoint& aPoint) const {
    nsOverflowAreas result(*this);
    result += aPoint;
    return result;
  }

  nsOverflowAreas& operator+=(const nsPoint& aPoint) {
    mRects[0] += aPoint;
    mRects[1] += aPoint;
    return *this;
  }

  void Clear() {
    mRects[0].SetRect(0, 0, 0, 0);
    mRects[1].SetRect(0, 0, 0, 0);
  }

  // Mutates |this| by unioning both overflow areas with |aOther|.
  void UnionWith(const nsOverflowAreas& aOther);

  // Mutates |this| by unioning both overflow areas with |aRect|.
  void UnionAllWith(const nsRect& aRect);

  // Mutates |this| by setting both overflow areas to |aRect|.
  void SetAllTo(const nsRect& aRect);
};

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

    bool operator==(const nsCollapsingMargin& aOther)
      {
        return mMostPos == aOther.mMostPos &&
          mMostNeg == aOther.mMostNeg;
      }

    bool operator!=(const nsCollapsingMargin& aOther)
      {
        return !(*this == aOther);
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

    void Include(const nsCollapsingMargin& aOther)
      {
        if (aOther.mMostPos > mMostPos)
          mMostPos = aOther.mMostPos;
        if (aOther.mMostNeg < mMostNeg)
          mMostNeg = aOther.mMostNeg;
      }

    void Zero()
      {
        mMostPos = 0;
        mMostNeg = 0;
      }

    bool IsZero() const
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
  nscoord width, height;    // [OUT] desired width and height (border-box)
  nscoord ascent;           // [OUT] baseline (from top), or ASK_FOR_BASELINE

  PRUint32 mFlags;

  enum { ASK_FOR_BASELINE = nscoord_MAX };

  // Metrics that _exactly_ enclose the text to allow precise MathML placements.
  // If the NS_REFLOW_CALC_BOUNDING_METRICS flag is set, then the caller is 
  // requesting that you also compute additional details about your inner
  // bounding box and italic correction. For example, the bounding box of
  // msup is the smallest rectangle that _exactly_ encloses both the text
  // of the base and the text of the superscript.
  nsBoundingMetrics mBoundingMetrics;  // [OUT]

  // Carried out bottom margin values. This is the collapsed
  // (generational) bottom margin value.
  nsCollapsingMargin mCarriedOutBottomMargin;

  // For frames that have content that overflow their content area
  // (HasOverflowAreas() is true) these rectangles represent the total
  // area of the frame including visible overflow, i.e., don't include
  // overflowing content that is hidden.  The rects are in the local
  // coordinate space of the frame, and should be at least as big as the
  // desired size. If there is no content that overflows, then the
  // overflow area is identical to the desired size and should be {0, 0,
  // width, height}.
  nsOverflowAreas mOverflowAreas;

  nsRect& VisualOverflow()
    { return mOverflowAreas.VisualOverflow(); }
  const nsRect& VisualOverflow() const
    { return mOverflowAreas.VisualOverflow(); }
  nsRect& ScrollableOverflow()
    { return mOverflowAreas.ScrollableOverflow(); }
  const nsRect& ScrollableOverflow() const
    { return mOverflowAreas.ScrollableOverflow(); }

  // Set all of mOverflowAreas to (0, 0, width, height).
  void SetOverflowAreasToDesiredBounds();

  // Union all of mOverflowAreas with (0, 0, width, height).
  void UnionOverflowAreasWithDesiredBounds();

  // XXXldb Should |aFlags| generally be passed from parent to child?
  // Some places do it, and some don't.  |aFlags| should perhaps go away
  // entirely.
  // XXX width/height/ascent are OUT parameters and so they shouldn't
  // have to be initialized, but there are some bad frame classes that
  // aren't properly setting them when returning from Reflow()...
  nsHTMLReflowMetrics(PRUint32 aFlags = 0)
    : width(0), height(0), ascent(ASK_FOR_BASELINE), mFlags(aFlags)
  {}
};

#endif /* nsHTMLReflowMetrics_h___ */

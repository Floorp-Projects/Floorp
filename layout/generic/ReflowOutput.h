/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* struct containing the output from nsIFrame::Reflow */

#ifndef mozilla_ReflowOutput_h
#define mozilla_ReflowOutput_h

#include "mozilla/WritingModes.h"
#include "nsBoundingMetrics.h"
#include "nsRect.h"

//----------------------------------------------------------------------

namespace mozilla {
struct ReflowInput;
} // namespace mozilla

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

namespace mozilla {

/**
 * Reflow metrics used to return the frame's desired size and alignment
 * information.
 *
 * @see #Reflow()
 */
class ReflowOutput {
public:
  // XXXldb Should |aFlags| generally be passed from parent to child?
  // Some places do it, and some don't.  |aFlags| should perhaps go away
  // entirely.
  // XXX width/height/ascent are OUT parameters and so they shouldn't
  // have to be initialized, but there are some bad frame classes that
  // aren't properly setting them when returning from Reflow()...
  explicit ReflowOutput(mozilla::WritingMode aWritingMode, uint32_t aFlags = 0)
    : mISize(0)
    , mBSize(0)
    , mBlockStartAscent(ASK_FOR_BASELINE)
    , mFlags(aFlags)
    , mWritingMode(aWritingMode)
  {}

  explicit ReflowOutput(const ReflowInput& aState, uint32_t aFlags = 0);

  // ISize and BSize are logical-coordinate dimensions:
  // ISize is the size in the writing mode's inline direction (which equates to
  // width in horizontal writing modes, height in vertical ones), and BSize is
  // the size in the block-progression direction.
  nscoord ISize(mozilla::WritingMode aWritingMode) const {
    NS_ASSERTION(!aWritingMode.IsOrthogonalTo(mWritingMode),
                 "mismatched writing mode");
    return mISize;
  }
  nscoord BSize(mozilla::WritingMode aWritingMode) const {
    NS_ASSERTION(!aWritingMode.IsOrthogonalTo(mWritingMode),
                 "mismatched writing mode");
    return mBSize;
  }
  mozilla::LogicalSize Size(mozilla::WritingMode aWritingMode) const {
    NS_ASSERTION(!aWritingMode.IsOrthogonalTo(mWritingMode),
                 "mismatched writing mode");
    return mozilla::LogicalSize(aWritingMode, mISize, mBSize);
  }

  nscoord& ISize(mozilla::WritingMode aWritingMode) {
    NS_ASSERTION(!aWritingMode.IsOrthogonalTo(mWritingMode),
                 "mismatched writing mode");
    return mISize;
  }
  nscoord& BSize(mozilla::WritingMode aWritingMode) {
    NS_ASSERTION(!aWritingMode.IsOrthogonalTo(mWritingMode),
                 "mismatched writing mode");
    return mBSize;
  }

  // Set inline and block size from a LogicalSize, converting to our
  // writing mode as necessary.
  void SetSize(mozilla::WritingMode aWM, mozilla::LogicalSize aSize)
  {
    mozilla::LogicalSize convertedSize = aSize.ConvertTo(mWritingMode, aWM);
    mBSize = convertedSize.BSize(mWritingMode);
    mISize = convertedSize.ISize(mWritingMode);
  }

  // Set both inline and block size to zero -- no need for a writing mode!
  void ClearSize()
  {
    mISize = mBSize = 0;
  }

  // Width and Height are physical dimensions, independent of writing mode.
  // Accessing these is slightly more expensive than accessing the logical
  // dimensions (once vertical writing mode support is enabled); as far as
  // possible, client code should work purely with logical dimensions.
  nscoord Width() const { return mWritingMode.IsVertical() ? mBSize : mISize; }
  nscoord Height() const { return mWritingMode.IsVertical() ? mISize : mBSize; }

  // It's only meaningful to consider "ascent" on the block-start side of the
  // frame, so no need to pass a writing mode argument
  nscoord BlockStartAscent() const
  {
    return mBlockStartAscent;
  }

  nscoord& Width() { return mWritingMode.IsVertical() ? mBSize : mISize; }
  nscoord& Height() { return mWritingMode.IsVertical() ? mISize : mBSize; }

  nsSize PhysicalSize()
  {
    return Size(mWritingMode).GetPhysicalSize(mWritingMode);
  }

  void SetBlockStartAscent(nscoord aAscent)
  {
    mBlockStartAscent = aAscent;
  }

  enum { ASK_FOR_BASELINE = nscoord_MAX };

  // Metrics that _exactly_ enclose the text to allow precise MathML placements.
  // If the NS_REFLOW_CALC_BOUNDING_METRICS flag is set, then the caller is
  // requesting that you also compute additional details about your inner
  // bounding box and italic correction. For example, the bounding box of
  // msup is the smallest rectangle that _exactly_ encloses both the text
  // of the base and the text of the superscript.
  nsBoundingMetrics mBoundingMetrics;  // [OUT]

  // Carried out block-end margin values. This is the collapsed
  // (generational) block-end margin value.
  nsCollapsingMargin mCarriedOutBEndMargin;

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

  mozilla::WritingMode GetWritingMode() const { return mWritingMode; }

private:
  nscoord mISize, mBSize; // [OUT] desired width and height (border-box)
  nscoord mBlockStartAscent; // [OUT] baseline (in Block direction), or ASK_FOR_BASELINE

public:
  uint32_t mFlags;

private:
  mozilla::WritingMode mWritingMode;
};

} // mozilla namespace

#endif // mozilla_ReflowOutput_h

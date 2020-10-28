/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
}  // namespace mozilla

/**
 * When we store overflow areas as an array of scrollable and visual
 * overflow, we use these indices.
 *
 * eOverflowType_LENGTH is needed (for gcc 4.5.*, at least) to ensure
 * that 2 is a valid value of nsOverflowType for use in
 * NS_FOR_FRAME_OVERFLOW_TYPES.
 */
enum nsOverflowType { eInkOverflow, eScrollableOverflow, eOverflowType_LENGTH };

#define NS_FOR_FRAME_OVERFLOW_TYPES(var_)                 \
  for (nsOverflowType var_ = nsOverflowType(0); var_ < 2; \
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

  nsRect& InkOverflow() { return mRects[eInkOverflow]; }
  const nsRect& InkOverflow() const { return mRects[eInkOverflow]; }

  nsRect& ScrollableOverflow() { return mRects[eScrollableOverflow]; }
  const nsRect& ScrollableOverflow() const {
    return mRects[eScrollableOverflow];
  }

  nsOverflowAreas() {
    // default-initializes to zero due to nsRect's default constructor
  }

  nsOverflowAreas(const nsRect& aInkOverflow,
                  const nsRect& aScrollableOverflow) {
    mRects[eInkOverflow] = aInkOverflow;
    mRects[eScrollableOverflow] = aScrollableOverflow;
  }

  nsOverflowAreas(const nsOverflowAreas& aOther) { *this = aOther; }

  nsOverflowAreas& operator=(const nsOverflowAreas& aOther) {
    mRects[0] = aOther.mRects[0];
    mRects[1] = aOther.mRects[1];
    return *this;
  }

  bool operator==(const nsOverflowAreas& aOther) const {
    // Scrollable overflow is a point-set rectangle and ink overflow
    // is a pixel-set rectangle.
    return InkOverflow().IsEqualInterior(aOther.InkOverflow()) &&
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
  nsCollapsingMargin() : mMostPos(0), mMostNeg(0) {}

  nsCollapsingMargin(const nsCollapsingMargin& aOther) = default;

  bool operator==(const nsCollapsingMargin& aOther) {
    return mMostPos == aOther.mMostPos && mMostNeg == aOther.mMostNeg;
  }

  bool operator!=(const nsCollapsingMargin& aOther) {
    return !(*this == aOther);
  }

  nsCollapsingMargin& operator=(const nsCollapsingMargin& aOther) = default;

  void Include(nscoord aCoord) {
    if (aCoord > mMostPos)
      mMostPos = aCoord;
    else if (aCoord < mMostNeg)
      mMostNeg = aCoord;
  }

  void Include(const nsCollapsingMargin& aOther) {
    if (aOther.mMostPos > mMostPos) mMostPos = aOther.mMostPos;
    if (aOther.mMostNeg < mMostNeg) mMostNeg = aOther.mMostNeg;
  }

  void Zero() {
    mMostPos = 0;
    mMostNeg = 0;
  }

  bool IsZero() const { return (mMostPos == 0) && (mMostNeg == 0); }

  nscoord get() const { return mMostPos + mMostNeg; }
};

namespace mozilla {

/**
 * ReflowOutput is initialized by a parent frame as a parameter passing to
 * Reflow() to allow a child frame to return its desired size and alignment
 * information.
 *
 * ReflowOutput's constructor usually takes a parent frame's WritingMode (or
 * ReflowInput) because it is more convenient for the parent frame to use the
 * stored Size() after reflowing the child frame. However, it can actually
 * accept any WritingMode (or ReflowInput) because SetSize() knows how to
 * convert a size in any writing mode to the stored writing mode.
 *
 * @see nsIFrame::Reflow() for more information.
 */
class ReflowOutput {
 public:
  explicit ReflowOutput(mozilla::WritingMode aWritingMode)
      : mSize(aWritingMode), mWritingMode(aWritingMode) {}

  // A convenient constructor to get WritingMode in ReflowInput.
  explicit ReflowOutput(const ReflowInput& aReflowInput);

  nscoord ISize(mozilla::WritingMode aWritingMode) const {
    return mSize.ISize(aWritingMode);
  }
  nscoord BSize(mozilla::WritingMode aWritingMode) const {
    return mSize.BSize(aWritingMode);
  }
  mozilla::LogicalSize Size(mozilla::WritingMode aWritingMode) const {
    return mSize.ConvertTo(aWritingMode, mWritingMode);
  }

  nscoord& ISize(mozilla::WritingMode aWritingMode) {
    return mSize.ISize(aWritingMode);
  }
  nscoord& BSize(mozilla::WritingMode aWritingMode) {
    return mSize.BSize(aWritingMode);
  }

  // Set inline and block size from a LogicalSize, converting to our
  // writing mode as necessary.
  void SetSize(mozilla::WritingMode aWM, mozilla::LogicalSize aSize) {
    mSize = aSize.ConvertTo(mWritingMode, aWM);
  }

  // Set both inline and block size to zero -- no need for a writing mode!
  void ClearSize() { mSize.SizeTo(mWritingMode, 0, 0); }

  // Width and Height are physical dimensions, independent of writing mode.
  // Accessing these is slightly more expensive than accessing the logical
  // dimensions; as far as possible, client code should work purely with logical
  // dimensions.
  nscoord Width() const { return mSize.Width(mWritingMode); }
  nscoord Height() const { return mSize.Height(mWritingMode); }
  nscoord& Width() {
    return mWritingMode.IsVertical() ? mSize.BSize(mWritingMode)
                                     : mSize.ISize(mWritingMode);
  }
  nscoord& Height() {
    return mWritingMode.IsVertical() ? mSize.ISize(mWritingMode)
                                     : mSize.BSize(mWritingMode);
  }

  nsSize PhysicalSize() const { return mSize.GetPhysicalSize(mWritingMode); }

  // It's only meaningful to consider "ascent" on the block-start side of the
  // frame, so no need to pass a writing mode argument
  enum { ASK_FOR_BASELINE = nscoord_MAX };
  nscoord BlockStartAscent() const { return mBlockStartAscent; }
  void SetBlockStartAscent(nscoord aAscent) { mBlockStartAscent = aAscent; }

  // Metrics that _exactly_ enclose the text to allow precise MathML placements.
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

  nsRect& InkOverflow() { return mOverflowAreas.InkOverflow(); }
  const nsRect& InkOverflow() const { return mOverflowAreas.InkOverflow(); }
  nsRect& ScrollableOverflow() { return mOverflowAreas.ScrollableOverflow(); }
  const nsRect& ScrollableOverflow() const {
    return mOverflowAreas.ScrollableOverflow();
  }

  // Set all of mOverflowAreas to (0, 0, width, height).
  void SetOverflowAreasToDesiredBounds();

  // Union all of mOverflowAreas with (0, 0, width, height).
  void UnionOverflowAreasWithDesiredBounds();

  mozilla::WritingMode GetWritingMode() const { return mWritingMode; }

 private:
  // Desired size of a frame's border-box.
  LogicalSize mSize;

  // Baseline (in block direction), or the default value ASK_FOR_BASELINE.
  nscoord mBlockStartAscent = ASK_FOR_BASELINE;

  mozilla::WritingMode mWritingMode;
};

}  // namespace mozilla

#endif  // mozilla_ReflowOutput_h

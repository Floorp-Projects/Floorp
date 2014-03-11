/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WritingModes_h_
#define WritingModes_h_

#include "nsRect.h"
#include "nsStyleStruct.h"

// If WRITING_MODE_VERTICAL_ENABLED is defined, we will attempt to support
// the vertical writing-mode values; if it is not defined, then
// WritingMode.IsVertical() will be hard-coded to return false, allowing
// many conditional branches to be optimized away while we're in the process
// of transitioning layout to use writing-mode and logical directions, but
// not yet ready to ship vertical support.

/* #define WRITING_MODE_VERTICAL_ENABLED 1 */

// It is the caller's responsibility to operate on logical-coordinate objects
// with matched writing modes. Failure to do so will be a runtime bug; the
// compiler can't catch it, but in debug mode, we'll throw an assertion.
// NOTE that in non-debug builds, a writing mode mismatch error will NOT be
// detected, yet the results will be nonsense (and may lead to further layout
// failures). Therefore, it is important to test (and fuzz-test) writing-mode
// support using debug builds.

// Methods in logical-coordinate classes that take another logical-coordinate
// object as a parameter should call CHECK_WRITING_MODE on it to verify that
// the writing modes match.
// (In some cases, there are internal (private) methods that don't do this;
// such methods should only be used by other methods that have already checked
// the writing modes.)

#define CHECK_WRITING_MODE(param) \
   NS_ASSERTION(param == mWritingMode, "writing-mode mismatch")

/**
 * mozilla::WritingMode is an immutable class representing a
 * writing mode.
 *
 * It efficiently stores the writing mode and can rapidly compute
 * interesting things about it for use in layout.
 *
 * Writing modes are computed from the CSS 'direction',
 * 'writing-mode', and 'text-orientation' properties.
 * See CSS3 Writing Modes for more information
 *   http://www.w3.org/TR/css3-writing-modes/
 */

namespace mozilla {

class WritingMode {
public:
  /**
   * Absolute inline flow direction
   */
  enum InlineDir {
    eInlineLTR = 0x00, // text flows horizontally left to right
    eInlineRTL = 0x02, // text flows horizontally right to left
    eInlineTTB = 0x01, // text flows vertically top to bottom
    eInlineBTT = 0x03, // text flows vertically bottom to top
  };

  /**
   * Absolute block flow direction
   */
  enum BlockDir {
    eBlockTB = 0x00, // horizontal lines stack top to bottom
    eBlockRL = 0x01, // vertical lines stack right to left
    eBlockLR = 0x05, // vertical lines stack left to right
  };

  /**
   * Line-relative (bidi-relative) inline flow direction
   */
  enum BidiDir {
    eBidiLTR = 0x00, // inline flow matches bidi LTR text
    eBidiRTL = 0x10, // inline flow matches bidi RTL text
  };

  /**
   * Unknown writing mode (should never actually be stored or used anywhere).
   */
  enum {
    eUnknownWritingMode = 0xff
  };

  /**
   * Return the absolute inline flow direction as an InlineDir
   */
  InlineDir GetInlineDir() const { return InlineDir(mWritingMode & eInlineMask); }

  /**
   * Return the absolute block flow direction as a BlockDir
   */
  BlockDir GetBlockDir() const { return BlockDir(mWritingMode & eBlockMask); }

  /**
   * Return the line-relative inline flow direction as a BidiDir
   */
  BidiDir GetBidiDir() const { return BidiDir(mWritingMode & eBidiMask); }

  /**
   * Return true if LTR. (Convenience method)
   */
  bool IsBidiLTR() const { return eBidiLTR == (mWritingMode & eBidiMask); }

  /**
   * True if vertical-mode block direction is LR (convenience method).
   */
  bool IsVerticalLR() const { return eBlockLR == (mWritingMode & eBlockMask); }

  /**
   * True if vertical writing mode, i.e. when
   * writing-mode: vertical-lr | vertical-rl.
   */
#ifdef WRITING_MODE_VERTICAL_ENABLED
  bool IsVertical() const { return !!(mWritingMode & eOrientationMask); }
#else
  bool IsVertical() const { return false; }
#endif

  /**
   * True if line-over/line-under are inverted from block-start/block-end.
   * This is true when
   *   - writing-mode is vertical-rl && text-orientation is sideways-left
   *   - writing-mode is vertical-lr && text-orientation is not sideways-left
   */
#ifdef WRITING_MODE_VERTICAL_ENABLED
  bool IsLineInverted() const { return !!(mWritingMode & eLineOrientMask); }
#else
  bool IsLineInverted() const { return false; }
#endif

  /**
   * Block-axis flow-relative to line-relative factor.
   * May be used as a multiplication factor for block-axis coordinates
   * to convert between flow- and line-relative coordinate systems (e.g.
   * positioning an over- or under-line decoration).
   */
  int FlowRelativeToLineRelativeFactor() const
  {
    return IsLineInverted() ? -1 : 1;
  }

  /**
   * Default constructor gives us a horizontal, LTR writing mode.
   * XXX We will probably eliminate this and require explicit initialization
   *     in all cases once transition is complete.
   */
  WritingMode()
    : mWritingMode(0)
  { }

  /**
   * Construct writing mode based on a style context
   */
  WritingMode(const nsStyleVisibility* aStyleVisibility)
  {
    NS_ASSERTION(aStyleVisibility, "we need an nsStyleVisibility here");

#ifdef WRITING_MODE_VERTICAL_ENABLED
    switch (aStyleVisibility->mWritingMode) {
      case NS_STYLE_WRITING_MODE_HORIZONTAL_TB:
        mWritingMode = 0;
        break;

      case NS_STYLE_WRITING_MODE_VERTICAL_LR:
        mWritingMode = eBlockFlowMask |
                       eLineOrientMask | //XXX needs update when text-orientation added
                       eOrientationMask;
        break;

      case NS_STYLE_WRITING_MODE_VERTICAL_RL:
        mWritingMode = eOrientationMask;
        break;

      default:
        NS_NOTREACHED("unknown writing mode!");
        mWritingMode = 0;
        break;
    }
#else
    mWritingMode = 0;
#endif

    if (NS_STYLE_DIRECTION_RTL == aStyleVisibility->mDirection) {
      mWritingMode |= eInlineFlowMask | //XXX needs update when text-orientation added
                      eBidiMask;
    }
  }

  // For unicode-bidi: plaintext, reset the direction of the writing mode from
  // the bidi paragraph level of the content

  //XXX change uint8_t to UBiDiLevel after bug 924851
  void SetDirectionFromBidiLevel(uint8_t level)
  {
    if (level & 1) {
      // odd level, set RTL
      mWritingMode |= eBidiMask;
    } else {
      // even level, set LTR
      mWritingMode &= ~eBidiMask;
    }
  }

  /**
   * Compare two WritingModes for equality.
   */
  bool operator==(const WritingMode& aOther) const
  {
    return mWritingMode == aOther.mWritingMode;
  }

private:
  friend class LogicalPoint;
  friend class LogicalSize;
  friend class LogicalMargin;
  friend class LogicalRect;

  /**
   * Return a WritingMode representing an unknown value.
   */
  static inline WritingMode Unknown()
  {
    return WritingMode(eUnknownWritingMode);
  }

  /**
   * Constructing a WritingMode with an arbitrary value is a private operation
   * currently only used by the Unknown() static method.
   */
  WritingMode(uint8_t aValue)
    : mWritingMode(aValue)
  { }

  uint8_t mWritingMode;

  enum Masks {
    // Masks for our bits; true chosen as opposite of commonest case
    eOrientationMask = 0x01, // true means vertical text
    eInlineFlowMask  = 0x02, // true means absolute RTL/BTT (against physical coords)
    eBlockFlowMask   = 0x04, // true means vertical-LR (or horizontal-BT if added)
    eLineOrientMask  = 0x08, // true means over != block-start
    eBidiMask        = 0x10, // true means line-relative RTL (bidi RTL)
    // Note: We have one excess bit of info; WritingMode can pack into 4 bits.
    // But since we have space, we're caching interesting things for fast access.

    // Masks for output enums
    eInlineMask = 0x03,
    eBlockMask  = 0x05
  };
};


/**
 * Logical-coordinate classes:
 *
 * There are three sets of coordinate space:
 *   - physical (top, left, bottom, right)
 *       relative to graphics coord system
 *   - flow-relative (block-start, inline-start, block-end, inline-end)
 *       relative to block/inline flow directions
 *   - line-relative (line-over, line-left, line-under, line-right)
 *       relative to glyph orientation / inline bidi directions
 * See CSS3 Writing Modes for more information
 *   http://www.w3.org/TR/css3-writing-modes/#abstract-box
 *
 * For shorthand, B represents the block-axis
 *                I represents the inline-axis
 *
 * The flow-relative geometric classes store coords in flow-relative space.
 * They use a private ns{Point,Size,Rect,Margin} member to store the actual
 * coordinate values, but reinterpret them as logical instead of physical.
 * This allows us to easily perform calculations in logical space (provided
 * writing modes of the operands match), by simply mapping to nsPoint (etc)
 * methods.
 *
 * Physical-coordinate accessors/setters are responsible to translate these
 * internal logical values as necessary.
 *
 * In DEBUG builds, the logical types store their WritingMode and check
 * that the same WritingMode is passed whenever callers ask them to do a
 * writing-mode-dependent operation. Non-DEBUG builds do NOT check this,
 * to avoid the overhead of storing WritingMode fields.
 *
 * Open question: do we need a different set optimized for line-relative
 * math, for use in nsLineLayout and the like? Or is multiplying values
 * by FlowRelativeToLineRelativeFactor() enough?
 */

/**
 * Flow-relative point
 */
class LogicalPoint {
public:
  LogicalPoint(WritingMode aWritingMode)
    :
#ifdef DEBUG
      mWritingMode(aWritingMode),
#endif
      mPoint(0, 0)
  { }

  // Construct from a writing mode and individual coordinates (which MUST be
  // values in that writing mode, NOT physical coordinates!)
  LogicalPoint(WritingMode aWritingMode, nscoord aI, nscoord aB)
    :
#ifdef DEBUG
      mWritingMode(aWritingMode),
#endif
      mPoint(aI, aB)
  { }

  // Construct from a writing mode and a physical point, within a given
  // containing rectangle's width (defining the conversion between LTR
  // and RTL coordinates).
  LogicalPoint(WritingMode aWritingMode,
               const nsPoint& aPoint,
               nscoord aContainerWidth)
#ifdef DEBUG
    : mWritingMode(aWritingMode)
#endif
  {
    if (aWritingMode.IsVertical()) {
      I() = aPoint.y;
      B() = aWritingMode.IsVerticalLR() ? aPoint.x : aContainerWidth - aPoint.x;
    } else {
      I() = aWritingMode.IsBidiLTR() ? aPoint.x : aContainerWidth - aPoint.x;
      B() = aPoint.y;
    }
  }

  /**
   * Read-only (const) access to the coordinates, in both logical
   * and physical terms.
   */
  nscoord I(WritingMode aWritingMode) const // inline-axis
  {
    CHECK_WRITING_MODE(aWritingMode);
    return mPoint.x;
  }
  nscoord B(WritingMode aWritingMode) const // block-axis
  {
    CHECK_WRITING_MODE(aWritingMode);
    return mPoint.y;
  }

  nscoord X(WritingMode aWritingMode, nscoord aContainerWidth) const
  {
    CHECK_WRITING_MODE(aWritingMode);
    if (aWritingMode.IsVertical()) {
      return aWritingMode.IsVerticalLR() ? B() : aContainerWidth - B();
    } else {
      return aWritingMode.IsBidiLTR() ? I() : aContainerWidth - I();
    }
  }
  nscoord Y(WritingMode aWritingMode) const
  {
    CHECK_WRITING_MODE(aWritingMode);
    return aWritingMode.IsVertical() ? I() : B();
  }

  /**
   * These non-const accessors return a reference (lvalue) that can be
   * assigned to by callers.
   */
  nscoord& I(WritingMode aWritingMode) // inline-axis
  {
    CHECK_WRITING_MODE(aWritingMode);
    return mPoint.x;
  }
  nscoord& B(WritingMode aWritingMode) // block-axis
  {
    CHECK_WRITING_MODE(aWritingMode);
    return mPoint.y;
  }

  /**
   * Setters for the physical coordinates.
   */
  void SetX(WritingMode aWritingMode, nscoord aX, nscoord aContainerWidth)
  {
    CHECK_WRITING_MODE(aWritingMode);
    if (aWritingMode.IsVertical()) {
      B() = aWritingMode.IsVerticalLR() ? aX : aContainerWidth - aX;
    } else {
      I() = aWritingMode.IsBidiLTR() ? aX : aContainerWidth - aX;
    }
  }
  void SetY(WritingMode aWritingMode, nscoord aY)
  {
    CHECK_WRITING_MODE(aWritingMode);
    if (aWritingMode.IsVertical()) {
      B() = aY;
    } else {
      I() = aY;
    }
  }

  /**
   * Return a physical point corresponding to our logical coordinates,
   * converted according to our writing mode.
   */
  nsPoint GetPhysicalPoint(WritingMode aWritingMode,
                           nscoord aContainerWidth) const
  {
    CHECK_WRITING_MODE(aWritingMode);
    if (aWritingMode.IsVertical()) {
      return nsPoint(aWritingMode.IsVerticalLR() ? B() : aContainerWidth - B(),
                     I());
    } else {
      return nsPoint(aWritingMode.IsBidiLTR() ? I() : aContainerWidth - I(),
                     B());
    }
  }

  /**
   * Return the equivalent point in a different writing mode.
   */
  LogicalPoint ConvertTo(WritingMode aToMode, WritingMode aFromMode,
                         nscoord aContainerWidth) const
  {
    CHECK_WRITING_MODE(aFromMode);
    return aToMode == aFromMode ?
      *this : LogicalPoint(aToMode,
                           GetPhysicalPoint(aFromMode, aContainerWidth),
                           aContainerWidth);
  }

  LogicalPoint operator+(const LogicalPoint& aOther) const
  {
    CHECK_WRITING_MODE(aOther.GetWritingMode());
    // In non-debug builds, LogicalPoint does not store the WritingMode,
    // so the first parameter here (which will always be eUnknownWritingMode)
    // is ignored.
    return LogicalPoint(GetWritingMode(),
                        mPoint.x + aOther.mPoint.x,
                        mPoint.y + aOther.mPoint.y);
  }

private:
  friend class LogicalRect;

  /**
   * NOTE that in non-DEBUG builds, GetWritingMode() always returns
   * eUnknownWritingMode, as the current mode is not stored in the logical-
   * geometry classes. Therefore, this method is private; it is used ONLY
   * by the DEBUG-mode checking macros in this class and its friends;
   * other code is not allowed to ask a logical point for its writing mode,
   * as this info will simply not be available in non-DEBUG builds.
   *
   * Also, in non-DEBUG builds, CHECK_WRITING_MODE does nothing, and the
   * WritingMode parameter to logical methods will generally be optimized
   * away altogether.
   */
#ifdef DEBUG
  WritingMode GetWritingMode() const { return mWritingMode; }
#else
  WritingMode GetWritingMode() const { return WritingMode::Unknown(); }
#endif

  // We don't allow construction of a LogicalPoint with no writing mode.
  LogicalPoint() MOZ_DELETE;

  // Accessors that don't take or check a WritingMode value.
  // These are for internal use only; they are called by methods that have
  // themselves already checked the WritingMode passed by the caller.
  nscoord I() const // inline-axis
  {
    return mPoint.x;
  }
  nscoord B() const // block-axis
  {
    return mPoint.y;
  }

  nscoord& I() // inline-axis
  {
    return mPoint.x;
  }
  nscoord& B() // block-axis
  {
    return mPoint.y;
  }

  WritingMode mWritingMode;

  // We use an nsPoint to hold the coordinates, but reinterpret its .x and .y
  // fields as the inline and block directions. Hence, this is not exposed
  // directly, but only through accessors that will map them according to the
  // writing mode.
  nsPoint mPoint;
};

/**
 * Flow-relative size
 */
class LogicalSize {
public:
  LogicalSize(WritingMode aWritingMode)
    :
#ifdef DEBUG
      mWritingMode(aWritingMode),
#endif
      mSize(0, 0)
  { }

  LogicalSize(WritingMode aWritingMode, nscoord aISize, nscoord aBSize)
    :
#ifdef DEBUG
      mWritingMode(aWritingMode),
#endif
      mSize(aISize, aBSize)
  { }

  LogicalSize(WritingMode aWritingMode, const nsSize& aPhysicalSize)
#ifdef DEBUG
    : mWritingMode(aWritingMode)
#endif
  {
    if (aWritingMode.IsVertical()) {
      ISize() = aPhysicalSize.height;
      BSize() = aPhysicalSize.width;
    } else {
      ISize() = aPhysicalSize.width;
      BSize() = aPhysicalSize.height;
    }
  }

  /**
   * Dimensions in logical and physical terms
   */
  nscoord ISize(WritingMode aWritingMode) const // inline-size
  {
    CHECK_WRITING_MODE(aWritingMode);
    return mSize.width;
  }
  nscoord BSize(WritingMode aWritingMode) const // block-size
  {
    CHECK_WRITING_MODE(aWritingMode);
    return mSize.height;
  }

  nscoord Width(WritingMode aWritingMode) const
  {
    CHECK_WRITING_MODE(aWritingMode);
    return aWritingMode.IsVertical() ? BSize() : ISize();
  }
  nscoord Height(WritingMode aWritingMode) const
  {
    CHECK_WRITING_MODE(aWritingMode);
    return aWritingMode.IsVertical() ? ISize() : BSize();
  }

  /**
   * Writable references to the logical dimensions
   */
  nscoord& ISize(WritingMode aWritingMode) // inline-size
  {
    CHECK_WRITING_MODE(aWritingMode);
    return mSize.width;
  }
  nscoord& BSize(WritingMode aWritingMode) // block-size
  {
    CHECK_WRITING_MODE(aWritingMode);
    return mSize.height;
  }

  /**
   * Setters for the physical dimensions
   */
  void SetWidth(WritingMode aWritingMode, nscoord aWidth)
  {
    CHECK_WRITING_MODE(aWritingMode);
    if (aWritingMode.IsVertical()) {
      BSize() = aWidth;
    } else {
      ISize() = aWidth;
    }
  }
  void SetHeight(WritingMode aWritingMode, nscoord aHeight)
  {
    CHECK_WRITING_MODE(aWritingMode);
    if (aWritingMode.IsVertical()) {
      ISize() = aHeight;
    } else {
      BSize() = aHeight;
    }
  }

  /**
   * Return an nsSize containing our physical dimensions
   */
  nsSize GetPhysicalSize(WritingMode aWritingMode) const
  {
    CHECK_WRITING_MODE(aWritingMode);
    return aWritingMode.IsVertical() ?
      nsSize(BSize(), ISize()) : nsSize(ISize(), BSize());
  }

  /**
   * Return a LogicalSize representing this size in a different writing mode
   */
  LogicalSize ConvertTo(WritingMode aToMode, WritingMode aFromMode) const
  {
    CHECK_WRITING_MODE(aFromMode);
    return aToMode == aFromMode ?
      *this : LogicalSize(aToMode, GetPhysicalSize(aFromMode));
  }

private:
  friend class LogicalRect;

  LogicalSize() MOZ_DELETE;

#ifdef DEBUG
  WritingMode GetWritingMode() const { return mWritingMode; }
#else
  WritingMode GetWritingMode() const { return WritingMode::Unknown(); }
#endif

  nscoord ISize() const // inline-size
  {
    return mSize.width;
  }
  nscoord BSize() const // block-size
  {
    return mSize.height;
  }

  nscoord& ISize() // inline-size
  {
    return mSize.width;
  }
  nscoord& BSize() // block-size
  {
    return mSize.height;
  }

  WritingMode mWritingMode;
  nsSize      mSize;
};

/**
 * Flow-relative margin
 */
class LogicalMargin {
public:
  LogicalMargin(WritingMode aWritingMode)
    :
#ifdef DEBUG
      mWritingMode(aWritingMode),
#endif
      mMargin(0, 0, 0, 0)
  { }

  LogicalMargin(WritingMode aWritingMode,
                nscoord aBStart, nscoord aIEnd,
                nscoord aBEnd, nscoord aIStart)
    :
#ifdef DEBUG
      mWritingMode(aWritingMode),
#endif
      mMargin(aBStart, aIEnd, aBEnd, aIStart)
  { }

  LogicalMargin(WritingMode aWritingMode, const nsMargin& aPhysicalMargin)
#ifdef DEBUG
    : mWritingMode(aWritingMode)
#endif
  {
    if (aWritingMode.IsVertical()) {
      if (aWritingMode.IsVerticalLR()) {
        mMargin.top = aPhysicalMargin.left;
        mMargin.bottom = aPhysicalMargin.right;
      } else {
        mMargin.top = aPhysicalMargin.right;
        mMargin.bottom = aPhysicalMargin.left;
      }
      if (aWritingMode.IsBidiLTR()) {
        mMargin.left = aPhysicalMargin.top;
        mMargin.right = aPhysicalMargin.bottom;
      } else {
        mMargin.left = aPhysicalMargin.bottom;
        mMargin.right = aPhysicalMargin.top;
      }
    } else {
      mMargin.top = aPhysicalMargin.top;
      mMargin.bottom = aPhysicalMargin.bottom;
      if (aWritingMode.IsBidiLTR()) {
        mMargin.left = aPhysicalMargin.left;
        mMargin.right = aPhysicalMargin.right;
      } else {
        mMargin.left = aPhysicalMargin.right;
        mMargin.right = aPhysicalMargin.left;
      }
    }
  }

  nscoord IStart(WritingMode aWritingMode) const // inline-start margin
  {
    CHECK_WRITING_MODE(aWritingMode);
    return mMargin.left;
  }
  nscoord IEnd(WritingMode aWritingMode) const // inline-end margin
  {
    CHECK_WRITING_MODE(aWritingMode);
    return mMargin.right;
  }
  nscoord BStart(WritingMode aWritingMode) const // block-start margin
  {
    CHECK_WRITING_MODE(aWritingMode);
    return mMargin.top;
  }
  nscoord BEnd(WritingMode aWritingMode) const // block-end margin
  {
    CHECK_WRITING_MODE(aWritingMode);
    return mMargin.bottom;
  }

  nscoord& IStart(WritingMode aWritingMode) // inline-start margin
  {
    CHECK_WRITING_MODE(aWritingMode);
    return mMargin.left;
  }
  nscoord& IEnd(WritingMode aWritingMode) // inline-end margin
  {
    CHECK_WRITING_MODE(aWritingMode);
    return mMargin.right;
  }
  nscoord& BStart(WritingMode aWritingMode) // block-start margin
  {
    CHECK_WRITING_MODE(aWritingMode);
    return mMargin.top;
  }
  nscoord& BEnd(WritingMode aWritingMode) // block-end margin
  {
    CHECK_WRITING_MODE(aWritingMode);
    return mMargin.bottom;
  }

  nscoord IStartEnd(WritingMode aWritingMode) const // inline margins
  {
    CHECK_WRITING_MODE(aWritingMode);
    return mMargin.LeftRight();
  }
  nscoord BStartEnd(WritingMode aWritingMode) const // block margins
  {
    CHECK_WRITING_MODE(aWritingMode);
    return mMargin.TopBottom();
  }

  /**
   * Accessors for physical margins, using our writing mode to convert from
   * logical values.
   */
  nscoord Top(WritingMode aWritingMode) const
  {
    CHECK_WRITING_MODE(aWritingMode);
    return aWritingMode.IsVertical() ?
      (aWritingMode.IsBidiLTR() ? IStart() : IEnd()) : BStart();
  }

  nscoord Bottom(WritingMode aWritingMode) const
  {
    CHECK_WRITING_MODE(aWritingMode);
    return aWritingMode.IsVertical() ?
      (aWritingMode.IsBidiLTR() ? IEnd() : IStart()) : BEnd();
  }

  nscoord Left(WritingMode aWritingMode) const
  {
    CHECK_WRITING_MODE(aWritingMode);
    return aWritingMode.IsVertical() ?
      (aWritingMode.IsVerticalLR() ? BStart() : BEnd()) :
      (aWritingMode.IsBidiLTR() ? IStart() : IEnd());
  }

  nscoord Right(WritingMode aWritingMode) const
  {
    CHECK_WRITING_MODE(aWritingMode);
    return aWritingMode.IsVertical() ?
      (aWritingMode.IsVerticalLR() ? BEnd() : BStart()) :
      (aWritingMode.IsBidiLTR() ? IEnd() : IStart());
  }

  nscoord LeftRight(WritingMode aWritingMode) const
  {
    CHECK_WRITING_MODE(aWritingMode);
    return aWritingMode.IsVertical() ? BStartEnd() : IStartEnd();
  }

  nscoord TopBottom(WritingMode aWritingMode) const
  {
    CHECK_WRITING_MODE(aWritingMode);
    return aWritingMode.IsVertical() ? IStartEnd() : BStartEnd();
  }

  void SizeTo(WritingMode aWritingMode,
              nscoord aBStart, nscoord aIEnd, nscoord aBEnd, nscoord aIStart)
  {
    CHECK_WRITING_MODE(aWritingMode);
    mMargin.SizeTo(aBStart, aIEnd, aBEnd, aIStart);
  }

  /**
   * Return an nsMargin containing our physical coordinates
   */
  nsMargin GetPhysicalMargin(WritingMode aWritingMode) const
  {
    CHECK_WRITING_MODE(aWritingMode);
    return aWritingMode.IsVertical() ?
      (aWritingMode.IsVerticalLR() ?
        nsMargin(IStart(), BEnd(), IEnd(), BStart()) :
        nsMargin(IStart(), BStart(), IEnd(), BEnd())) :
      (aWritingMode.IsBidiLTR() ?
        nsMargin(BStart(), IEnd(), BEnd(), IStart()) :
        nsMargin(BStart(), IStart(), BEnd(), IEnd()));
  }

  /**
   * Return a LogicalMargin representing this margin in a different
   * writing mode
   */
  LogicalMargin ConvertTo(WritingMode aToMode, WritingMode aFromMode) const
  {
    CHECK_WRITING_MODE(aFromMode);
    return aToMode == aFromMode ?
      *this : LogicalMargin(aToMode, GetPhysicalMargin(aFromMode));
  }

  bool IsEmpty() const
  {
    return (mMargin.left == 0 && mMargin.top == 0 &&
            mMargin.right == 0 && mMargin.bottom == 0);
  }

private:
  friend class LogicalRect;

  LogicalMargin() MOZ_DELETE;

#ifdef DEBUG
  WritingMode GetWritingMode() const { return mWritingMode; }
#else
  WritingMode GetWritingMode() const { return WritingMode::Unknown(); }
#endif

  nscoord IStart() const // inline-start margin
  {
    return mMargin.left;
  }
  nscoord IEnd() const // inline-end margin
  {
    return mMargin.right;
  }
  nscoord BStart() const // block-start margin
  {
    return mMargin.top;
  }
  nscoord BEnd() const // block-end margin
  {
    return mMargin.bottom;
  }

  nscoord& IStart() // inline-start margin
  {
    return mMargin.left;
  }
  nscoord& IEnd() // inline-end margin
  {
    return mMargin.right;
  }
  nscoord& BStart() // block-start margin
  {
    return mMargin.top;
  }
  nscoord& BEnd() // block-end margin
  {
    return mMargin.bottom;
  }

  nscoord IStartEnd() const // inline margins
  {
    return mMargin.LeftRight();
  }
  nscoord BStartEnd() const // block margins
  {
    return mMargin.TopBottom();
  }

  WritingMode mWritingMode;
  nsMargin    mMargin;
};

/**
 * Flow-relative rectangle
 */
class LogicalRect {
public:
  LogicalRect(WritingMode aWritingMode)
    :
#ifdef DEBUG
      mWritingMode(aWritingMode),
#endif
      mRect(0, 0, 0, 0)
  { }

  LogicalRect(WritingMode aWritingMode,
              nscoord aIStart, nscoord aBStart,
              nscoord aISize, nscoord aBSize)
    :
#ifdef DEBUG
      mWritingMode(aWritingMode),
#endif
      mRect(aIStart, aBStart, aISize, aBSize)
  { }

  LogicalRect(WritingMode aWritingMode,
              const LogicalPoint& aOrigin,
              const LogicalSize& aSize)
    : 
#ifdef DEBUG
      mWritingMode(aWritingMode),
#endif
      mRect(aOrigin.mPoint, aSize.mSize)
  {
    CHECK_WRITING_MODE(aOrigin.GetWritingMode());
    CHECK_WRITING_MODE(aSize.GetWritingMode());
  }

  LogicalRect(WritingMode aWritingMode,
              const nsRect& aRect,
              nscoord aContainerWidth)
#ifdef DEBUG
    : mWritingMode(aWritingMode)
#endif
  {
    if (aWritingMode.IsVertical()) {
      if (aWritingMode.IsVerticalLR()) {
        mRect.y = aRect.x;
      } else {
        mRect.y = aContainerWidth - aRect.XMost();
      }
      mRect.height = aRect.width;
      mRect.x = aRect.y;
      mRect.width = aRect.height;
    } else {
      if (aWritingMode.IsBidiLTR()) {
        mRect.x = aRect.x;
      } else {
        mRect.x = aContainerWidth - aRect.XMost();
      }
      mRect.width = aRect.width;
      mRect.y = aRect.y;
      mRect.height = aRect.height;
    }
  }

  /**
   * Inline- and block-dimension geometry.
   */
  nscoord IStart(WritingMode aWritingMode) const // inline-start edge
  {
    CHECK_WRITING_MODE(aWritingMode);
    return mRect.X();
  }
  nscoord IEnd(WritingMode aWritingMode) const // inline-end edge
  {
    CHECK_WRITING_MODE(aWritingMode);
    return mRect.XMost();
  }
  nscoord ISize(WritingMode aWritingMode) const // inline-size
  {
    CHECK_WRITING_MODE(aWritingMode);
    return mRect.Width();
  }

  nscoord BStart(WritingMode aWritingMode) const // block-start edge
  {
    CHECK_WRITING_MODE(aWritingMode);
    return mRect.Y();
  }
  nscoord BEnd(WritingMode aWritingMode) const // block-end edge
  {
    CHECK_WRITING_MODE(aWritingMode);
    return mRect.YMost();
  }
  nscoord BSize(WritingMode aWritingMode) const // block-size
  {
    CHECK_WRITING_MODE(aWritingMode);
    return mRect.Height();
  }

  /**
   * Writable (reference) accessors are only available for the basic logical
   * fields (Start and Size), not derivatives like End.
   */
  nscoord& IStart(WritingMode aWritingMode) // inline-start edge
  {
    CHECK_WRITING_MODE(aWritingMode);
    return mRect.x;
  }
  nscoord& ISize(WritingMode aWritingMode) // inline-size
  {
    CHECK_WRITING_MODE(aWritingMode);
    return mRect.width;
  }
  nscoord& BStart(WritingMode aWritingMode) // block-start edge
  {
    CHECK_WRITING_MODE(aWritingMode);
    return mRect.y;
  }
  nscoord& BSize(WritingMode aWritingMode) // block-size
  {
    CHECK_WRITING_MODE(aWritingMode);
    return mRect.height;
  }

  /**
   * Physical coordinates of the rect.
   */
  nscoord X(WritingMode aWritingMode, nscoord aContainerWidth) const
  {
    CHECK_WRITING_MODE(aWritingMode);
    if (aWritingMode.IsVertical()) {
      return aWritingMode.IsVerticalLR() ?
             mRect.Y() : aContainerWidth - mRect.YMost();
    } else {
      return aWritingMode.IsBidiLTR() ?
             mRect.X() : aContainerWidth - mRect.XMost();
    }
  }

  void SetX(WritingMode aWritingMode, nscoord aX, nscoord aContainerWidth)
  {
    CHECK_WRITING_MODE(aWritingMode);
    if (aWritingMode.IsVertical()) {
      if (aWritingMode.IsVerticalLR()) {
        BStart() = aX;
      } else {
        BStart() = aContainerWidth - aX - BSize();
      }
    } else {
      if (aWritingMode.IsBidiLTR()) {
        IStart() = aX;
      } else {
        IStart() = aContainerWidth - aX - ISize();
      }
    }
  }

  nscoord Y(WritingMode aWritingMode) const
  {
    CHECK_WRITING_MODE(aWritingMode);
    return aWritingMode.IsVertical() ? mRect.X() : mRect.Y();
  }

  void SetY(WritingMode aWritingMode, nscoord aY)
  {
    CHECK_WRITING_MODE(aWritingMode);
    if (aWritingMode.IsVertical()) {
      IStart() = aY;
    } else {
      BStart() = aY;
    }
  }

  nscoord Width(WritingMode aWritingMode) const
  {
    CHECK_WRITING_MODE(aWritingMode);
    return aWritingMode.IsVertical() ? mRect.Height() : mRect.Width();
  }

  // When setting the Width of a rect, we keep its physical X-coord fixed
  // and modify XMax. This means that in the RTL case, we'll be moving
  // the IStart, so that IEnd remains constant.
  void SetWidth(WritingMode aWritingMode, nscoord aWidth)
  {
    CHECK_WRITING_MODE(aWritingMode);
    if (aWritingMode.IsVertical()) {
      if (!aWritingMode.IsVerticalLR()) {
        BStart() = BStart() + BSize() - aWidth;
      }
      BSize() = aWidth;
    } else {
      if (!aWritingMode.IsBidiLTR()) {
        IStart() = IStart() + ISize() - aWidth;
      }
      ISize() = aWidth;
    }
  }

  nscoord Height(WritingMode aWritingMode) const
  {
    CHECK_WRITING_MODE(aWritingMode);
    return aWritingMode.IsVertical() ? mRect.Width() : mRect.Height();
  }

  void SetHeight(WritingMode aWritingMode, nscoord aHeight)
  {
    CHECK_WRITING_MODE(aWritingMode);
    if (aWritingMode.IsVertical()) {
      ISize() = aHeight;
    } else {
      BSize() = aHeight;
    }
  }

  nscoord XMost(WritingMode aWritingMode, nscoord aContainerWidth) const
  {
    CHECK_WRITING_MODE(aWritingMode);
    if (aWritingMode.IsVertical()) {
      return aWritingMode.IsVerticalLR() ?
             mRect.YMost() : aContainerWidth - mRect.Y();
    } else {
      return aWritingMode.IsBidiLTR() ?
             mRect.XMost() : aContainerWidth - mRect.X();
    }
  }

  nscoord YMost(WritingMode aWritingMode) const
  {
    CHECK_WRITING_MODE(aWritingMode);
    return aWritingMode.IsVertical() ? mRect.XMost() : mRect.YMost();
  }

  bool IsEmpty() const
  {
    return (mRect.x == 0 && mRect.y == 0 &&
            mRect.width == 0 && mRect.height == 0);
  }

  bool IsZeroSize() const
  {
    return (mRect.width == 0 && mRect.height == 0);
  }

/* XXX are these correct?
  nscoord ILeft(WritingMode aWritingMode) const
  {
    CHECK_WRITING_MODE(aWritingMode);
    return aWritingMode.IsBidiLTR() ? IStart() : IEnd();
  }
  nscoord IRight(WritingMode aWritingMode) const
  {
    CHECK_WRITING_MODE(aWritingMode);
    return aWritingMode.IsBidiLTR() ? IEnd() : IStart();
  }
*/

  LogicalPoint Origin(WritingMode aWritingMode) const
  {
    CHECK_WRITING_MODE(aWritingMode);
    return LogicalPoint(aWritingMode, IStart(), BStart());
  }
  LogicalSize Size(WritingMode aWritingMode) const
  {
    CHECK_WRITING_MODE(aWritingMode);
    return LogicalSize(aWritingMode, ISize(), BSize());
  }

  LogicalRect operator+(const LogicalPoint& aPoint) const
  {
    CHECK_WRITING_MODE(aPoint.GetWritingMode());
    return LogicalRect(GetWritingMode(),
                       IStart() + aPoint.I(), BStart() + aPoint.B(),
                       ISize(), BSize());
  }

  LogicalRect& operator+=(const LogicalPoint& aPoint)
  {
    CHECK_WRITING_MODE(aPoint.GetWritingMode());
    mRect += aPoint.mPoint;
    return *this;
  }

  LogicalRect operator-(const LogicalPoint& aPoint) const
  {
    CHECK_WRITING_MODE(aPoint.GetWritingMode());
    return LogicalRect(GetWritingMode(),
                       IStart() - aPoint.I(), BStart() - aPoint.B(),
                       ISize(), BSize());
  }

  LogicalRect& operator-=(const LogicalPoint& aPoint)
  {
    CHECK_WRITING_MODE(aPoint.GetWritingMode());
    mRect -= aPoint.mPoint;
    return *this;
  }

  void MoveBy(WritingMode aWritingMode, const LogicalPoint& aDelta)
  {
    CHECK_WRITING_MODE(aWritingMode);
    CHECK_WRITING_MODE(aDelta.GetWritingMode());
    IStart() += aDelta.I();
    BStart() += aDelta.B();
  }

  void Inflate(nscoord aD) { mRect.Inflate(aD); }
  void Inflate(nscoord aDI, nscoord aDB) { mRect.Inflate(aDI, aDB); }
  void Inflate(WritingMode aWritingMode, const LogicalMargin& aMargin)
  {
    CHECK_WRITING_MODE(aWritingMode);
    CHECK_WRITING_MODE(aMargin.GetWritingMode());
    mRect.Inflate(aMargin.mMargin);
  }

  void Deflate(nscoord aD) { mRect.Deflate(aD); }
  void Deflate(nscoord aDI, nscoord aDB) { mRect.Deflate(aDI, aDB); }
  void Deflate(WritingMode aWritingMode, const LogicalMargin& aMargin)
  {
    CHECK_WRITING_MODE(aWritingMode);
    CHECK_WRITING_MODE(aMargin.GetWritingMode());
    mRect.Deflate(aMargin.mMargin);
  }

  /**
   * Return an nsRect containing our physical coordinates within the given
   * container width
   */
  nsRect GetPhysicalRect(WritingMode aWritingMode,
                         nscoord aContainerWidth) const
  {
    CHECK_WRITING_MODE(aWritingMode);
    if (aWritingMode.IsVertical()) {
      return nsRect(aWritingMode.IsVerticalLR() ?
                      BStart() : aContainerWidth - BEnd(),
                    IStart(), BSize(), ISize());
    } else {
      return nsRect(aWritingMode.IsBidiLTR() ?
                      IStart() : aContainerWidth - IEnd(),
                    BStart(), ISize(), BSize());
    }
  }

#if 0 // XXX this would require aContainerWidth as well
  /**
   * Return a LogicalRect representing this rect in a different writing mode
   */
  LogicalRect ConvertTo(WritingMode aToMode, WritingMode aFromMode) const
  {
    CHECK_WRITING_MODE(aFromMode);
    return aToMode == aFromMode ?
      *this : LogicalRect(aToMode, GetPhysicalRect(aFromMode));
  }
#endif

private:
  LogicalRect() MOZ_DELETE;

#ifdef DEBUG
  WritingMode GetWritingMode() const { return mWritingMode; }
#else
  WritingMode GetWritingMode() const { return WritingMode::Unknown(); }
#endif

  nscoord IStart() const // inline-start edge
  {
    return mRect.X();
  }
  nscoord IEnd() const // inline-end edge
  {
    return mRect.XMost();
  }
  nscoord ISize() const // inline-size
  {
    return mRect.Width();
  }

  nscoord BStart() const // block-start edge
  {
    return mRect.Y();
  }
  nscoord BEnd() const // block-end edge
  {
    return mRect.YMost();
  }
  nscoord BSize() const // block-size
  {
    return mRect.Height();
  }

  nscoord& IStart() // inline-start edge
  {
    return mRect.x;
  }
  nscoord& ISize() // inline-size
  {
    return mRect.width;
  }
  nscoord& BStart() // block-start edge
  {
    return mRect.y;
  }
  nscoord& BSize() // block-size
  {
    return mRect.height;
  }

  WritingMode mWritingMode;
  nsRect      mRect;
};

} // namespace mozilla

#endif // WritingModes_h_

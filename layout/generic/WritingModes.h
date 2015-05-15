/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WritingModes_h_
#define WritingModes_h_

#include "nsRect.h"
#include "nsStyleContext.h"
#include "nsBidiUtils.h"

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
   NS_ASSERTION(param == GetWritingMode(), "writing-mode mismatch")

namespace mozilla {

namespace widget {
struct IMENotification;
} // namespace widget

// Physical axis constants.
enum PhysicalAxis {
  eAxisVertical      = 0x0,
  eAxisHorizontal    = 0x1
};

inline bool IsInline(LogicalSide aSide) { return aSide & 0x2; }
inline bool IsBlock(LogicalSide aSide) { return !IsInline(aSide); }
inline bool IsEnd(LogicalSide aSide) { return aSide & 0x1; }
inline bool IsStart(LogicalSide aSide) { return !IsEnd(aSide); }

inline LogicalAxis GetAxis(LogicalSide aSide)
{
  return IsInline(aSide) ? eLogicalAxisInline : eLogicalAxisBlock;
}

inline LogicalEdge GetEdge(LogicalSide aSide)
{
  return IsEnd(aSide) ? eLogicalEdgeEnd : eLogicalEdgeStart;
}

inline LogicalEdge GetOppositeEdge(LogicalEdge aEdge)
{
  // This relies on the only two LogicalEdge enum values being 0 and 1.
  return LogicalEdge(1 - aEdge);
}

inline LogicalSide
MakeLogicalSide(LogicalAxis aAxis, LogicalEdge aEdge)
{
  return LogicalSide((aAxis << 1) | aEdge);
}

inline LogicalSide GetOppositeSide(LogicalSide aSide)
{
  return MakeLogicalSide(GetAxis(aSide), GetOppositeEdge(GetEdge(aSide)));
}

enum LogicalSideBits {
  eLogicalSideBitsNone   = 0,
  eLogicalSideBitsBStart = 1 << eLogicalSideBStart,
  eLogicalSideBitsBEnd   = 1 << eLogicalSideBEnd,
  eLogicalSideBitsIEnd   = 1 << eLogicalSideIEnd,
  eLogicalSideBitsIStart = 1 << eLogicalSideIStart,
  eLogicalSideBitsBBoth = eLogicalSideBitsBStart | eLogicalSideBitsBEnd,
  eLogicalSideBitsIBoth = eLogicalSideBitsIStart | eLogicalSideBitsIEnd,
  eLogicalSideBitsAll = eLogicalSideBitsBBoth | eLogicalSideBitsIBoth
};

enum LineRelativeDir {
  eLineRelativeDirOver  = eLogicalSideBStart,
  eLineRelativeDirUnder = eLogicalSideBEnd,
  eLineRelativeDirLeft  = eLogicalSideIStart,
  eLineRelativeDirRight = eLogicalSideIEnd
};

/**
 * LogicalSides represents a set of logical sides.
 */
struct LogicalSides final {
  LogicalSides() : mBits(0) {}
  explicit LogicalSides(LogicalSideBits aSideBits)
  {
    MOZ_ASSERT((aSideBits & ~eLogicalSideBitsAll) == 0, "illegal side bits");
    mBits = aSideBits;
  }
  bool IsEmpty() const { return mBits == 0; }
  bool BStart()  const { return mBits & eLogicalSideBitsBStart; }
  bool BEnd()    const { return mBits & eLogicalSideBitsBEnd; }
  bool IStart()  const { return mBits & eLogicalSideBitsIStart; }
  bool IEnd()    const { return mBits & eLogicalSideBitsIEnd; }
  bool Contains(LogicalSideBits aSideBits) const
  {
    MOZ_ASSERT((aSideBits & ~eLogicalSideBitsAll) == 0, "illegal side bits");
    return (mBits & aSideBits) == aSideBits;
  }
  LogicalSides operator|(LogicalSides aOther) const
  {
    return LogicalSides(LogicalSideBits(mBits | aOther.mBits));
  }
  LogicalSides operator|(LogicalSideBits aSideBits) const
  {
    return *this | LogicalSides(aSideBits);
  }
  LogicalSides& operator|=(LogicalSides aOther)
  {
    mBits |= aOther.mBits;
    return *this;
  }
  LogicalSides& operator|=(LogicalSideBits aSideBits)
  {
    return *this |= LogicalSides(aSideBits);
  }
  bool operator==(LogicalSides aOther) const
  {
    return mBits == aOther.mBits;
  }
  bool operator!=(LogicalSides aOther) const
  {
    return !(*this == aOther);
  }

private:
  uint8_t mBits;
};

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
  bool IsBidiLTR() const { return eBidiLTR == GetBidiDir(); }

  /**
   * True if vertical-mode block direction is LR (convenience method).
   */
  bool IsVerticalLR() const { return eBlockLR == GetBlockDir(); }

  /**
   * True if vertical-mode block direction is RL (convenience method).
   */
  bool IsVerticalRL() const { return eBlockRL == GetBlockDir(); }

  /**
   * True if vertical writing mode, i.e. when
   * writing-mode: vertical-lr | vertical-rl.
   */
  bool IsVertical() const { return !!(mWritingMode & eOrientationMask); }

  /**
   * True if line-over/line-under are inverted from block-start/block-end.
   * This is true when
   *   - writing-mode is vertical-rl && text-orientation is sideways-left
   *   - writing-mode is vertical-lr && text-orientation is not sideways-left
   */
  bool IsLineInverted() const { return !!(mWritingMode & eLineOrientMask); }

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
   * True if the text-orientation will force all text to be rendered sideways
   * in vertical lines, in which case we should prefer an alphabetic baseline;
   * otherwise, the default is centered.
   * Note that some glyph runs may be rendered sideways even if this is false,
   * due to text-orientation:mixed resolution, but in that case the dominant
   * baseline remains centered.
   */
  bool IsSideways() const { return !!(mWritingMode & eSidewaysMask); }

  static mozilla::PhysicalAxis PhysicalAxisForLogicalAxis(
                                              uint8_t aWritingModeValue,
                                              LogicalAxis aAxis)
  {
    // This relies on bit 0 of a writing-value mode indicating vertical
    // orientation and bit 0 of a LogicalAxis value indicating the inline axis,
    // so that it can correctly form mozilla::PhysicalAxis values using bit
    // manipulation.
    static_assert(NS_STYLE_WRITING_MODE_HORIZONTAL_TB == 0 &&
                  NS_STYLE_WRITING_MODE_VERTICAL_RL == 1 &&
                  NS_STYLE_WRITING_MODE_VERTICAL_LR == 3 &&
                  eLogicalAxisBlock == 0 &&
                  eLogicalAxisInline == 1 &&
                  eAxisVertical == 0 && 
                  eAxisHorizontal == 1,
                  "unexpected writing-mode, logical axis or physical axis "
                  "constant values");
    return mozilla::PhysicalAxis((aWritingModeValue ^ aAxis) & 0x1);
  }

  mozilla::PhysicalAxis PhysicalAxis(LogicalAxis aAxis) const
  {
    // This will set wm to either NS_STYLE_WRITING_MODE_HORIZONTAL_TB or
    // NS_STYLE_WRITING_MODE_VERTICAL_RL, and not the other two (real
    // and hypothetical) values.  But this is fine; we only need to
    // distinguish between vertical and horizontal in
    // PhysicalAxisForLogicalAxis.
    int wm = mWritingMode & eOrientationMask;
    return PhysicalAxisForLogicalAxis(wm, aAxis);
  }

  static mozilla::Side PhysicalSideForBlockAxis(uint8_t aWritingModeValue,
                                                LogicalEdge aEdge)
  {
    // indexes are NS_STYLE_WRITING_MODE_* values, which are the same as these
    // two-bit values:
    //   bit 0 = the eOrientationMask value
    //   bit 1 = the eBlockFlowMask value
    static const mozilla::css::Side kLogicalBlockSides[][2] = {
      { NS_SIDE_TOP,    NS_SIDE_BOTTOM },  // horizontal-tb
      { NS_SIDE_RIGHT,  NS_SIDE_LEFT   },  // vertical-rl
      { NS_SIDE_BOTTOM, NS_SIDE_TOP    },  // (horizontal-bt)
      { NS_SIDE_LEFT,   NS_SIDE_RIGHT  },  // vertical-lr
    };

    NS_ASSERTION(aWritingModeValue < 4, "invalid aWritingModeValue value");
    return kLogicalBlockSides[aWritingModeValue][aEdge];
  }

  mozilla::Side PhysicalSideForInlineAxis(LogicalEdge aEdge) const
  {
    // indexes are four-bit values:
    //   bit 0 = the eOrientationMask value
    //   bit 1 = the eInlineFlowMask value
    //   bit 2 = the eBlockFlowMask value
    //   bit 3 = the eLineOrientMask value
    static const mozilla::css::Side kLogicalInlineSides[][2] = {
      { NS_SIDE_LEFT,   NS_SIDE_RIGHT  },  // horizontal-tb                  ltr
      { NS_SIDE_TOP,    NS_SIDE_BOTTOM },  // vertical-rl                    ltr
      { NS_SIDE_RIGHT,  NS_SIDE_LEFT   },  // horizontal-tb                  rtl
      { NS_SIDE_BOTTOM, NS_SIDE_TOP    },  // vertical-rl                    rtl
      { NS_SIDE_RIGHT,  NS_SIDE_LEFT   },  // (horizontal-bt)  (inverted)    ltr
      { NS_SIDE_TOP,    NS_SIDE_BOTTOM },  // vertical-lr      sideways-left rtl
      { NS_SIDE_LEFT,   NS_SIDE_RIGHT  },  // (horizontal-bt)  (inverted)    rtl
      { NS_SIDE_BOTTOM, NS_SIDE_TOP    },  // vertical-lr      sideways-left ltr
      { NS_SIDE_LEFT,   NS_SIDE_RIGHT  },  // horizontal-tb    (inverted)    rtl
      { NS_SIDE_TOP,    NS_SIDE_BOTTOM },  // vertical-rl      sideways-left rtl
      { NS_SIDE_RIGHT,  NS_SIDE_LEFT   },  // horizontal-tb    (inverted)    ltr
      { NS_SIDE_BOTTOM, NS_SIDE_TOP    },  // vertical-rl      sideways-left ltr
      { NS_SIDE_LEFT,   NS_SIDE_RIGHT  },  // (horizontal-bt)                ltr
      { NS_SIDE_TOP,    NS_SIDE_BOTTOM },  // vertical-lr                    ltr
      { NS_SIDE_RIGHT,  NS_SIDE_LEFT   },  // (horizontal-bt)                rtl
      { NS_SIDE_BOTTOM, NS_SIDE_TOP    },  // vertical-lr                    rtl
    };

    // Inline axis sides depend on all three of writing-mode, text-orientation
    // and direction, which are encoded in the eOrientationMask,
    // eInlineFlowMask, eBlockFlowMask and eLineOrientMask bits.  Use these four
    // bits to index into kLogicalInlineSides.
    static_assert(eOrientationMask == 0x01 && eInlineFlowMask == 0x02 &&
                  eBlockFlowMask == 0x04 && eLineOrientMask == 0x08,
                  "unexpected mask values");
    int index = mWritingMode & 0x0F;
    return kLogicalInlineSides[index][aEdge];
  }

  /**
   * Returns the physical side corresponding to the specified logical side,
   * given the current writing mode.
   */
  mozilla::Side PhysicalSide(LogicalSide aSide) const
  {
    if (IsBlock(aSide)) {
      static_assert(eOrientationMask == 0x01 && eBlockFlowMask == 0x04,
                    "unexpected mask values");
      int wm = ((mWritingMode & eBlockFlowMask) >> 1) |
               (mWritingMode & eOrientationMask);
      return PhysicalSideForBlockAxis(wm, GetEdge(aSide));
    }

    return PhysicalSideForInlineAxis(GetEdge(aSide));
  }

  /**
   * Returns the logical side corresponding to the specified
   * line-relative direction, given the current writing mode.
   */
  LogicalSide LogicalSideForLineRelativeDir(LineRelativeDir aDir) const
  {
    auto side = static_cast<LogicalSide>(aDir);
    if (IsInline(side)) {
      return IsBidiLTR() ? side : GetOppositeSide(side);
    }
    return !IsLineInverted() ? side : GetOppositeSide(side);
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
  explicit WritingMode(nsStyleContext* aStyleContext)
  {
    NS_ASSERTION(aStyleContext, "we need an nsStyleContext here");

    const nsStyleVisibility* styleVisibility = aStyleContext->StyleVisibility();

    switch (styleVisibility->mWritingMode) {
      case NS_STYLE_WRITING_MODE_HORIZONTAL_TB:
        mWritingMode = 0;
        break;

      case NS_STYLE_WRITING_MODE_VERTICAL_LR:
      {
        mWritingMode = eBlockFlowMask |
                       eLineOrientMask |
                       eOrientationMask;
        uint8_t textOrientation = aStyleContext->StyleVisibility()->mTextOrientation;
#if 0 // not yet implemented
        if (textOrientation == NS_STYLE_TEXT_ORIENTATION_SIDEWAYS_LEFT) {
          mWritingMode &= ~eLineOrientMask;
        }
#endif
        if (textOrientation >= NS_STYLE_TEXT_ORIENTATION_SIDEWAYS_RIGHT) {
          mWritingMode |= eSidewaysMask;
        }
        break;
      }

      case NS_STYLE_WRITING_MODE_VERTICAL_RL:
      {
        mWritingMode = eOrientationMask;
        uint8_t textOrientation = aStyleContext->StyleVisibility()->mTextOrientation;
#if 0 // not yet implemented
        if (textOrientation == NS_STYLE_TEXT_ORIENTATION_SIDEWAYS_LEFT) {
          mWritingMode |= eLineOrientMask;
        }
#endif
        if (textOrientation >= NS_STYLE_TEXT_ORIENTATION_SIDEWAYS_RIGHT) {
          mWritingMode |= eSidewaysMask;
        }
        break;
      }

      default:
        NS_NOTREACHED("unknown writing mode!");
        mWritingMode = 0;
        break;
    }

    if (NS_STYLE_DIRECTION_RTL == styleVisibility->mDirection) {
      mWritingMode |= eInlineFlowMask | //XXX needs update when text-orientation added
                      eBidiMask;
    }
  }

  // For unicode-bidi: plaintext, reset the direction of the writing mode from
  // the bidi paragraph level of the content

  //XXX change uint8_t to UBiDiLevel after bug 924851
  void SetDirectionFromBidiLevel(uint8_t level)
  {
    if (IS_LEVEL_RTL(level)) {
      // set RTL
      mWritingMode |= eBidiMask;
    } else {
      // set LTR
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

  bool operator!=(const WritingMode& aOther) const
  {
    return mWritingMode != aOther.mWritingMode;
  }

  /**
   * Check whether two modes are orthogonal to each other.
   */
  bool IsOrthogonalTo(const WritingMode& aOther) const
  {
    return IsVertical() != aOther.IsVertical();
  }

private:
  friend class LogicalPoint;
  friend class LogicalSize;
  friend class LogicalMargin;
  friend class LogicalRect;

  friend struct IPC::ParamTraits<WritingMode>;
  // IMENotification cannot store this class directly since this has some
  // constructors.  Therefore, it stores mWritingMode and recreate the
  // instance from it.
  friend struct widget::IMENotification;

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
  explicit WritingMode(uint8_t aValue)
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

    eSidewaysMask    = 0x20, // true means text-orientation is sideways-*,
                             // which means we'll use alphabetic instead of
                             // centered default baseline for vertical text

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
  explicit LogicalPoint(WritingMode aWritingMode)
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

  bool operator==(const LogicalPoint& aOther) const
  {
    CHECK_WRITING_MODE(aOther.GetWritingMode());
    return mPoint == aOther.mPoint;
  }

  bool operator!=(const LogicalPoint& aOther) const
  {
    CHECK_WRITING_MODE(aOther.GetWritingMode());
    return mPoint != aOther.mPoint;
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

  LogicalPoint& operator+=(const LogicalPoint& aOther)
  {
    CHECK_WRITING_MODE(aOther.GetWritingMode());
    I() += aOther.I();
    B() += aOther.B();
    return *this;
  }

  LogicalPoint operator-(const LogicalPoint& aOther) const
  {
    CHECK_WRITING_MODE(aOther.GetWritingMode());
    // In non-debug builds, LogicalPoint does not store the WritingMode,
    // so the first parameter here (which will always be eUnknownWritingMode)
    // is ignored.
    return LogicalPoint(GetWritingMode(),
                        mPoint.x - aOther.mPoint.x,
                        mPoint.y - aOther.mPoint.y);
  }

  LogicalPoint& operator-=(const LogicalPoint& aOther)
  {
    CHECK_WRITING_MODE(aOther.GetWritingMode());
    I() -= aOther.I();
    B() -= aOther.B();
    return *this;
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
  LogicalPoint() = delete;

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

#ifdef DEBUG
  WritingMode mWritingMode;
#endif

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
  explicit LogicalSize(WritingMode aWritingMode)
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

  void SizeTo(WritingMode aWritingMode, nscoord aISize, nscoord aBSize)
  {
    CHECK_WRITING_MODE(aWritingMode);
    mSize.SizeTo(aISize, aBSize);
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
   * Writable references to the logical and physical dimensions
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

  nscoord& Width(WritingMode aWritingMode)
  {
    CHECK_WRITING_MODE(aWritingMode);
    return aWritingMode.IsVertical() ? BSize() : ISize();
  }
  nscoord& Height(WritingMode aWritingMode)
  {
    CHECK_WRITING_MODE(aWritingMode);
    return aWritingMode.IsVertical() ? ISize() : BSize();
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
#ifdef DEBUG
    // In DEBUG builds make sure to return a LogicalSize with the
    // expected writing mode
    CHECK_WRITING_MODE(aFromMode);
    return aToMode == aFromMode ?
      *this : LogicalSize(aToMode, GetPhysicalSize(aFromMode));
#else
    // optimization for non-DEBUG builds where LogicalSize doesn't store
    // the writing mode
    return (aToMode == aFromMode || !aToMode.IsOrthogonalTo(aFromMode))
             ? *this : LogicalSize(aToMode, BSize(), ISize());
#endif
  }

  /**
   * Test if a size is (0, 0).
   */
  bool IsAllZero() const
  {
    return ISize() == 0 && BSize() == 0;
  }

  /**
   * Various binary operators on LogicalSize. These are valid ONLY for operands
   * that share the same writing mode.
   */
  bool operator==(const LogicalSize& aOther) const
  {
    CHECK_WRITING_MODE(aOther.GetWritingMode());
    return mSize == aOther.mSize;
  }

  bool operator!=(const LogicalSize& aOther) const
  {
    CHECK_WRITING_MODE(aOther.GetWritingMode());
    return mSize != aOther.mSize;
  }

  LogicalSize operator+(const LogicalSize& aOther) const
  {
    CHECK_WRITING_MODE(aOther.GetWritingMode());
    return LogicalSize(GetWritingMode(), ISize() + aOther.ISize(),
                                         BSize() + aOther.BSize());
  }
  LogicalSize& operator+=(const LogicalSize& aOther)
  {
    CHECK_WRITING_MODE(aOther.GetWritingMode());
    ISize() += aOther.ISize();
    BSize() += aOther.BSize();
    return *this;
  }

  LogicalSize operator-(const LogicalSize& aOther) const
  {
    CHECK_WRITING_MODE(aOther.GetWritingMode());
    return LogicalSize(GetWritingMode(), ISize() - aOther.ISize(),
                                         BSize() - aOther.BSize());
  }
  LogicalSize& operator-=(const LogicalSize& aOther)
  {
    CHECK_WRITING_MODE(aOther.GetWritingMode());
    ISize() -= aOther.ISize();
    BSize() -= aOther.BSize();
    return *this;
  }

private:
  friend class LogicalRect;

  LogicalSize() = delete;

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

#ifdef DEBUG
  WritingMode mWritingMode;
#endif
  nsSize      mSize;
};

/**
 * Flow-relative margin
 */
class LogicalMargin {
public:
  explicit LogicalMargin(WritingMode aWritingMode)
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
   * Return a LogicalSize representing the total size of the inline-
   * and block-dimension margins.
   */
  LogicalSize Size(WritingMode aWritingMode) const
  {
    CHECK_WRITING_MODE(aWritingMode);
    return LogicalSize(aWritingMode, IStartEnd(), BStartEnd());
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

  nscoord& Top(WritingMode aWritingMode)
  {
    CHECK_WRITING_MODE(aWritingMode);
    return aWritingMode.IsVertical() ?
      (aWritingMode.IsBidiLTR() ? IStart() : IEnd()) : BStart();
  }

  nscoord& Bottom(WritingMode aWritingMode)
  {
    CHECK_WRITING_MODE(aWritingMode);
    return aWritingMode.IsVertical() ?
      (aWritingMode.IsBidiLTR() ? IEnd() : IStart()) : BEnd();
  }

  nscoord& Left(WritingMode aWritingMode)
  {
    CHECK_WRITING_MODE(aWritingMode);
    return aWritingMode.IsVertical() ?
      (aWritingMode.IsVerticalLR() ? BStart() : BEnd()) :
      (aWritingMode.IsBidiLTR() ? IStart() : IEnd());
  }

  nscoord& Right(WritingMode aWritingMode)
  {
    CHECK_WRITING_MODE(aWritingMode);
    return aWritingMode.IsVertical() ?
      (aWritingMode.IsVerticalLR() ? BEnd() : BStart()) :
      (aWritingMode.IsBidiLTR() ? IEnd() : IStart());
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

  void ApplySkipSides(LogicalSides aSkipSides)
  {
    if (aSkipSides.BStart()) {
      BStart() = 0;
    }
    if (aSkipSides.BEnd()) {
      BEnd() = 0;
    }
    if (aSkipSides.IStart()) {
      IStart() = 0;
    }
    if (aSkipSides.IEnd()) {
      IEnd() = 0;
    }
  }

  bool IsAllZero() const
  {
    return (mMargin.left == 0 && mMargin.top == 0 &&
            mMargin.right == 0 && mMargin.bottom == 0);
  }

  LogicalMargin operator+(const LogicalMargin& aMargin) const {
    CHECK_WRITING_MODE(aMargin.GetWritingMode());
    return LogicalMargin(GetWritingMode(),
                         BStart() + aMargin.BStart(),
                         IEnd() + aMargin.IEnd(),
                         BEnd() + aMargin.BEnd(),
                         IStart() + aMargin.IStart());
  }

  LogicalMargin operator+=(const LogicalMargin& aMargin)
  {
    CHECK_WRITING_MODE(aMargin.GetWritingMode());
    mMargin += aMargin.mMargin;
    return *this;
  }

  LogicalMargin operator-(const LogicalMargin& aMargin) const {
    CHECK_WRITING_MODE(aMargin.GetWritingMode());
    return LogicalMargin(GetWritingMode(),
                         BStart() - aMargin.BStart(),
                         IEnd() - aMargin.IEnd(),
                         BEnd() - aMargin.BEnd(),
                         IStart() - aMargin.IStart());
  }

private:
  friend class LogicalRect;

  LogicalMargin() = delete;

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

#ifdef DEBUG
  WritingMode mWritingMode;
#endif
  nsMargin    mMargin;
};

/**
 * Flow-relative rectangle
 */
class LogicalRect {
public:
  explicit LogicalRect(WritingMode aWritingMode)
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
   * Accessors for line-relative coordinates
   */
  nscoord LineLeft(WritingMode aWritingMode, nscoord aContainerWidth) const
  {
    CHECK_WRITING_MODE(aWritingMode);
    if (aWritingMode.IsVertical()) {
      return IStart(); // sideways-left will require aContainerHeight
    } else {
      return aWritingMode.IsBidiLTR() ? IStart()
                                      : aContainerWidth - IEnd();
    }
  }
  nscoord LineRight(WritingMode aWritingMode, nscoord aContainerWidth) const
  {
    CHECK_WRITING_MODE(aWritingMode);
    if (aWritingMode.IsVertical()) {
      return IEnd(); // sideways-left will require aContainerHeight
    } else {
      return aWritingMode.IsBidiLTR() ? IEnd()
                                      : aContainerWidth - IStart();
    }
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
    return mRect.IsEmpty();
  }

  bool IsAllZero() const
  {
    return (mRect.x == 0 && mRect.y == 0 &&
            mRect.width == 0 && mRect.height == 0);
  }

  bool IsZeroSize() const
  {
    return (mRect.width == 0 && mRect.height == 0);
  }

  void SetEmpty() { mRect.SetEmpty(); }

  bool IsEqualEdges(const LogicalRect aOther) const
  {
    CHECK_WRITING_MODE(aOther.GetWritingMode());
    return mRect.IsEqualEdges(aOther.mRect);
  }

  LogicalPoint Origin(WritingMode aWritingMode) const
  {
    CHECK_WRITING_MODE(aWritingMode);
    return LogicalPoint(aWritingMode, IStart(), BStart());
  }
  void SetOrigin(WritingMode aWritingMode, const LogicalPoint& aPoint)
  {
    IStart(aWritingMode) = aPoint.I(aWritingMode);
    BStart(aWritingMode) = aPoint.B(aWritingMode);
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

  /**
   * Return a LogicalRect representing this rect in a different writing mode
   */
  LogicalRect ConvertTo(WritingMode aToMode, WritingMode aFromMode,
                        nscoord aContainerWidth) const
  {
    CHECK_WRITING_MODE(aFromMode);
    return aToMode == aFromMode ?
      *this : LogicalRect(aToMode, GetPhysicalRect(aFromMode, aContainerWidth),
                          aContainerWidth);
  }

  /**
   * Set *this to be the rectangle containing the intersection of aRect1
   * and aRect2, return whether the intersection is non-empty.
   */
  bool IntersectRect(const LogicalRect& aRect1, const LogicalRect& aRect2)
  {
    CHECK_WRITING_MODE(aRect1.mWritingMode);
    CHECK_WRITING_MODE(aRect2.mWritingMode);
    return mRect.IntersectRect(aRect1.mRect, aRect2.mRect);
  }

private:
  LogicalRect() = delete;

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

#ifdef DEBUG
  WritingMode mWritingMode;
#endif
  nsRect      mRect;
};

} // namespace mozilla

// Definitions of inline methods for nsStyleSides, declared in nsStyleCoord.h
// but not defined there because they need WritingMode.
inline nsStyleUnit nsStyleSides::GetUnit(mozilla::WritingMode aWM,
                                         mozilla::LogicalSide aSide) const
{
  return GetUnit(aWM.PhysicalSide(aSide));
}

inline nsStyleUnit nsStyleSides::GetIStartUnit(mozilla::WritingMode aWM) const
{
  return GetUnit(aWM, mozilla::eLogicalSideIStart);
}

inline nsStyleUnit nsStyleSides::GetBStartUnit(mozilla::WritingMode aWM) const
{
  return GetUnit(aWM, mozilla::eLogicalSideBStart);
}

inline nsStyleUnit nsStyleSides::GetIEndUnit(mozilla::WritingMode aWM) const
{
  return GetUnit(aWM, mozilla::eLogicalSideIEnd);
}

inline nsStyleUnit nsStyleSides::GetBEndUnit(mozilla::WritingMode aWM) const
{
  return GetUnit(aWM, mozilla::eLogicalSideBEnd);
}

inline nsStyleCoord nsStyleSides::Get(mozilla::WritingMode aWM,
                                      mozilla::LogicalSide aSide) const
{
  return Get(aWM.PhysicalSide(aSide));
}

inline nsStyleCoord nsStyleSides::GetIStart(mozilla::WritingMode aWM) const
{
  return Get(aWM, mozilla::eLogicalSideIStart);
}

inline nsStyleCoord nsStyleSides::GetBStart(mozilla::WritingMode aWM) const
{
  return Get(aWM, mozilla::eLogicalSideBStart);
}

inline nsStyleCoord nsStyleSides::GetIEnd(mozilla::WritingMode aWM) const
{
  return Get(aWM, mozilla::eLogicalSideIEnd);
}

inline nsStyleCoord nsStyleSides::GetBEnd(mozilla::WritingMode aWM) const
{
  return Get(aWM, mozilla::eLogicalSideBEnd);
}

// Definitions of inline methods for nsStylePosition, declared in
// nsStyleStruct.h but not defined there because they need WritingMode.
inline nsStyleCoord& nsStylePosition::ISize(mozilla::WritingMode aWM)
{
  return aWM.IsVertical() ? mHeight : mWidth;
}
inline nsStyleCoord& nsStylePosition::MinISize(mozilla::WritingMode aWM)
{
  return aWM.IsVertical() ? mMinHeight : mMinWidth;
}
inline nsStyleCoord& nsStylePosition::MaxISize(mozilla::WritingMode aWM)
{
  return aWM.IsVertical() ? mMaxHeight : mMaxWidth;
}
inline nsStyleCoord& nsStylePosition::BSize(mozilla::WritingMode aWM)
{
  return aWM.IsVertical() ? mWidth : mHeight;
}
inline nsStyleCoord& nsStylePosition::MinBSize(mozilla::WritingMode aWM)
{
  return aWM.IsVertical() ? mMinWidth : mMinHeight;
}
inline nsStyleCoord& nsStylePosition::MaxBSize(mozilla::WritingMode aWM)
{
  return aWM.IsVertical() ? mMaxWidth : mMaxHeight;
}

inline const nsStyleCoord&
nsStylePosition::ISize(mozilla::WritingMode aWM) const
{
  return aWM.IsVertical() ? mHeight : mWidth;
}
inline const nsStyleCoord&
nsStylePosition::MinISize(mozilla::WritingMode aWM) const
{
  return aWM.IsVertical() ? mMinHeight : mMinWidth;
}
inline const nsStyleCoord&
nsStylePosition::MaxISize(mozilla::WritingMode aWM) const
{
  return aWM.IsVertical() ? mMaxHeight : mMaxWidth;
}
inline const nsStyleCoord&
nsStylePosition::BSize(mozilla::WritingMode aWM) const
{
  return aWM.IsVertical() ? mWidth : mHeight;
}
inline const nsStyleCoord&
nsStylePosition::MinBSize(mozilla::WritingMode aWM) const
{
  return aWM.IsVertical() ? mMinWidth : mMinHeight;
}
inline const nsStyleCoord&
nsStylePosition::MaxBSize(mozilla::WritingMode aWM) const
{
  return aWM.IsVertical() ? mMaxWidth : mMaxHeight;
}

inline bool
nsStylePosition::ISizeDependsOnContainer(mozilla::WritingMode aWM) const
{
  return aWM.IsVertical() ? HeightDependsOnContainer()
                          : WidthDependsOnContainer();
}
inline bool
nsStylePosition::MinISizeDependsOnContainer(mozilla::WritingMode aWM) const
{
  return aWM.IsVertical() ? MinHeightDependsOnContainer()
                          : MinWidthDependsOnContainer();
}
inline bool
nsStylePosition::MaxISizeDependsOnContainer(mozilla::WritingMode aWM) const
{
  return aWM.IsVertical() ? MaxHeightDependsOnContainer()
                          : MaxWidthDependsOnContainer();
}
inline bool
nsStylePosition::BSizeDependsOnContainer(mozilla::WritingMode aWM) const
{
  return aWM.IsVertical() ? WidthDependsOnContainer()
                          : HeightDependsOnContainer();
}
inline bool
nsStylePosition::MinBSizeDependsOnContainer(mozilla::WritingMode aWM) const
{
  return aWM.IsVertical() ? MinWidthDependsOnContainer()
                          : MinHeightDependsOnContainer();
}
inline bool
nsStylePosition::MaxBSizeDependsOnContainer(mozilla::WritingMode aWM) const
{
  return aWM.IsVertical() ? MaxWidthDependsOnContainer()
                          : MaxHeightDependsOnContainer();
}

#endif // WritingModes_h_

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_DottedCornerFinder_h_
#define mozilla_DottedCornerFinder_h_

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/BezierUtils.h"
#include "gfxRect.h"

namespace mozilla {

// Calculate C_i and r_i for each filled/unfilled circles in dotted corner.
// Returns circle with C_{2j} and r_{2j} where 0 < 2j < n.
//
//                            ____-----------+
//                      __----  *****     ###|
//                __----      *********  ####|
//           __---  #####    ***********#####|
//        _--     #########  *****+*****#####+ C_0
//      _-       ########### *** C_1****#####|
//     /         #####+#####  *********  ####|
//    /       .  ### C_2 ###    *****     ###|
//   |            #########       ____-------+
//   |     .        #####____-----
//  |              __----
//  |    .       /
// |           /
// |  *****   |
// | ******* |
// |*********|
// |****+****|
// | C_{n-1} |
// | ******* |
// |  *****  |
// |  #####  |
// | ####### |
// |#########|
// +----+----+
//     C_n

class DottedCornerFinder
{
  typedef mozilla::gfx::Bezier Bezier;
  typedef mozilla::gfx::Float Float;
  typedef mozilla::gfx::Point Point;
  typedef mozilla::gfx::Size Size;

public:
  struct Result
  {
    // Center point of dot and its radius.
    Point C;
    Float r;

    Result(const Point& aC, Float aR)
     : C(aC), r(aR)
    {}
  };

  //                        aBorderRadiusX
  //                       aCornerDim.width
  //                     |<----------------->|
  //                     |                   | v
  //                   --+-------------___---+--
  //                   ^ |         __--      | |
  //                   | |       _-          | | aR0
  //                   | |     /         aC0 +--
  //                   | |   /               | ^
  //                   | |  |                |
  //  aBorderRadiusY   | | |             __--+
  // aCornerDim.height | ||            _-
  //                   | ||           /
  //                   | |           /
  //                   | |          |
  //                   | |          |
  //                   | |         |
  //                   | |         |
  //                   v |    aCn  |
  //                   --+----+----+
  //                     |    |
  //                     |<-->|
  //                      aRn
  //
  // aCornerDim and (aBorderRadiusX, aBorderRadiusY) can be different when
  // aBorderRadiusX is smaller than aRn*2 or
  // aBorderRadiusY is smaller than aR0*2.
  //
  //                                        aCornerDim.width
  //                                      |<----------------->|
  //                                      |                   |
  //                                      | aBorderRadiusX    |
  //                                      |<--------->|       |
  //                                      |           |       |
  //                   -------------------+-------__--+-------+--
  //                   ^                ^ |     _-            | ^
  //                   |                | |   /               | |
  //                   |                | |  /                | |
  //                   | aBorderRadiusY | | |                 | | aR0
  //                   |                | ||                  | |
  //                   |                | |                   | |
  // aCornerDim.height |                v |                   | v
  //                   |                --+               aC0 +--
  //                   |                  |                   |
  //                   |                  |                   |
  //                   |                  |                   |
  //                   |                  |                   |
  //                   |                  |                   |
  //                   v                  |        aCn        |
  //                   -------------------+---------+---------+
  //                                      |         |
  //                                      |<------->|
  //                                          aRn
  DottedCornerFinder(const Bezier& aOuterBezier, const Bezier& aInnerBezier,
                     mozilla::css::Corner aCorner,
                     Float aBorderRadiusX, Float aBorderRadiusY,
                     const Point& aC0, Float aR0, const Point& aCn, Float aRn,
                     const Size& aCornerDim);

  bool HasMore(void) const;
  Result Next(void);

private:
  static const size_t MAX_LOOP = 32;

  // Bezier control points for the outer curve, the inner curve, and a curve
  // that center points of circles are on (center curve).
  //
  //               ___---+ outer curve
  //           __--      |
  //         _-          |
  //       /        __---+ center curve
  //     /      __--     |
  //    |     /          |
  //   |    /        __--+ inner curve
  //  |    |       _-
  //  |    |      /
  // |     |     /
  // |    |     |
  // |    |     |
  // |    |    |
  // |    |    |
  // |    |    |
  // +----+----+
  Bezier mOuterBezier;
  Bezier mInnerBezier;
  Bezier mCenterBezier;

  mozilla::css::Corner mCorner;

  // Sign of the normal vector used in radius calculation, flipped depends on
  // corner and start and end radii.
  Float mNormalSign;

  // Center points and raii for start and end circles, mR0 >= mRn.
  // mMaxR = max(mR0, mRn)
  //
  //                           v
  //               ___---+------
  //           __--     #|#    | mRn
  //         _-        ##|##   |
  //       /           ##+## ---
  //     /              mCn    ^
  //    |               #|#
  //   |             __--+
  //  |            _-
  //  |           /
  // |           /
  // |          |
  // |          |
  // |  #####  |
  // | ####### |
  // |#########|
  // +----+----+
  // |## mC0 ##|
  // | ####### |
  // |  #####  |
  // |    |
  // |<-->|
  //
  //  mR0
  //
  Point mC0;
  Point mCn;
  Float mR0;
  Float mRn;
  Float mMaxR;

  // Parameters for the center curve with perfect circle and the inner curve.
  //
  //               ___---+
  //           __--      |
  //         _-          |
  //       /        __---+
  //     /      __--     |
  //    |     /          |
  //   |    /        __--+--
  //  |    |       _-    | ^
  //  |    |      /      | |
  // |     |     /       | |
  // |    |     |        | |
  // |    |     |        | | mInnerHeight
  // |    |    |         | |
  // |    |    |         | |
  // |    |    |         | v
  // +----+----+---------+
  //      |    |         | mCurveOrigin
  //      |    |<------->|
  //      |  mInnerWidth |
  //      |              |
  //      |<------------>|
  //        mCenterCurveR
  //
  Point mCurveOrigin;
  Float mCenterCurveR;
  Float mInnerWidth;
  Float mInnerHeight;

  Point mLastC;
  Float mLastR;
  Float mLastT;

  // Overlap between two circles.
  // It uses arc length on PERFECT, SINGLE_CURVE_AND_RADIUS, and SINGLE_CURVE,
  // and direct distance on OTHER.
  Float mBestOverlap;

  // If one of border-widths is 0, do not calculate overlap, and draw circles
  // until it reaches the other side or exceeds mMaxCount.
  bool mHasZeroBorderWidth;
  bool mHasMore;

  // The maximum number of filled/unfilled circles.
  size_t mMaxCount;

  enum {
    //                      radius.width
    //                 |<----------------->|
    //                 |                   |
    //               --+-------------___---+----
    //               ^ |         __--     #|#  ^
    //               | |       _-        ##|## |
    //               | |     /           ##+## | top-width
    //               | |   /             ##|## |
    //               | |  |               #|#  v
    //               | | |             __--+----
    // radius.height | ||            _-
    //               | ||           /
    //               | |           /
    //               | |          |
    //               | |          |
    //               | |  #####  |
    //               | | ####### |
    //               v |#########|
    //               --+----+----+
    //                 |#########|
    //                 | ####### |
    //                 |  #####  |
    //                 |         |
    //                 |<------->|
    //                  left-width

    // * top-width == left-width
    // * radius.width == radius.height
    // * top-width < radius.width * 2
    //
    // All circles has same radii and are on single perfect circle's arc.
    // Overlap is known.
    //
    // Split the perfect circle's arc into 2n segments, each segment's length is
    // top-width * (1 - overlap).  Place each circle's center point C_i on each
    // end of the segment, each circle's radius r_i is top-width / 2
    //
    //                       #####
    //                      #######
    // perfect             #########
    // circle's          ___---+####
    // arc     ##### __--  ## C_0 ##
    //   |    #####_-       ###|###
    //   |   ####+####       ##|##
    //   |   ##/C_i ##         |
    //   |    |######          |
    //   |   | #####           |
    //   +->|                  |
    //     |                   |
    //   ##|##                 |
    //  ###|###                |
    // ####|####               |
    // ####+-------------------+
    // ## C_n ##
    //  #######
    //   #####
    PERFECT,

    // * top-width == left-width
    // * 0.5 < radius.width / radius.height < 2.0
    // * top-width < min(radius.width, radius.height) * 2
    //
    // All circles has same radii and are on single elliptic arc.
    // Overlap is known.
    //
    // Split the elliptic arc into 2n segments, each segment's length is
    // top-width * (1 - overlap).  Place each circle's center point C_i on each
    // end of the segment, each circle's radius r_i is top-width / 2
    //
    //                            #####
    //                           #######
    //             #####        #########
    //            #######   ____----+####
    // elliptic  ######__---    ## C_0 ##
    // arc       ##__+-###       ###|###
    //   |      / # C_i #         ##|##
    //   +--> /    #####            |
    //       |                      |
    //   ###|#                      |
    //  ###|###                     |
    // ####|####                    |
    // ####+------------------------+
    // ## C_n ##
    //  #######
    //   #####
    SINGLE_CURVE_AND_RADIUS,

    // * top-width != left-width
    // * 0 < min(top-width, left-width)
    // * 0.5 < radius.width / radius.height < 2.0
    // * max(top-width, left-width) < min(radius.width, radius.height) * 2
    //
    // All circles are on single elliptic arc.
    // Overlap is unknown.
    //
    // Place each circle's center point C_i on elliptic arc, each circle's
    // radius r_i is the distance between the center point and the inner curve.
    // The arc segment's length between C_i and C_{i-1} is
    // (r_i + r_{i-1}) * (1 - overlap).
    //
    //  outer curve
    //           /
    //          /
    //         /         / center curve
    //        / ####### /
    //       /##       /#
    //      +#        /  #
    //     /#        /    #
    //    / #   C_i /     #
    //   /  #      +      #  /
    //  /   #     /  \    # / inner curve
    //      #    /     \  #/
    //       #  /   r_i  \+
    //        #/       ##/
    //        / ####### /
    //                 /
    SINGLE_CURVE,

    // Other cases.
    // Circles are not on single elliptic arc.
    // Overlap are unknown.
    //
    // Place tangent point innerTangent on the inner curve and find circle's
    // center point C_i and radius r_i where the circle is also tangent to the
    // outer curve.
    // Distance between C_i and C_{i-1} is (r_i + r_{i-1}) * (1 - overlap).
    //
    //  outer curve
    //           /
    //          /
    //         /
    //        / #######
    //       /##       ##
    //      +#           #
    //     /# \           #
    //    / #    \        #
    //   /  #      +      #  /
    //  /   #   C_i  \    # / inner curve
    //      #          \  #/
    //       #      r_i  \+
    //        ##       ##/ innerTangent
    //          ####### /
    //                 /
    OTHER
  } mType;

  size_t mI;
  size_t mCount;

  // Determine mType from parameters.
  void DetermineType(Float aBorderRadiusX, Float aBorderRadiusY);

  // Reset calculation.
  void Reset(void);

  // Find radius for the given tangent point on the inner curve such that the
  // circle is also tangent to the outer curve.
  void FindPointAndRadius(Point& C, Float& r, const Point& innerTangent,
                          const Point& normal, Float t);

  // Find next dot.
  Float FindNext(Float overlap);

  // Find mBestOverlap for parameters.
  void FindBestOverlap(Float aMinR,
                       Float aMinBorderRadius, Float aMaxBorderRadius);

  // Fill corner with dots with given overlap, and return the number of dots
  // and last two dots's overlap.
  bool GetCountAndLastOverlap(Float aOverlap,
                              size_t* aCount, Float* aActualOverlap);
};

} // namespace mozilla

#endif /* mozilla_DottedCornerFinder_h_ */

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_DashedCornerFinder_h_
#define mozilla_DashedCornerFinder_h_

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/BezierUtils.h"

namespace mozilla {

// Calculate {OuterT_i, InnerT_i} for each 1 < i < n, that
//   (OuterL_i + InnerL_i) / 2 == dashLength * (W_i + W_{i-1}) / 2
// where
//   OuterP_i: OuterCurve(OuterT_i)
//   InnerP_i: InnerCurve(OuterT_i)
//   OuterL_i: Elliptic arc length between OuterP_i - OuterP_{i-1}
//   InnerL_i: Elliptic arc length between InnerP_i - InnerP_{i-1}
//   W_i = |OuterP_i - InnerP_i|
//   1.0 < dashLength < 3.0
//
//                                         OuterP_1          OuterP_0
//                                         _+__-----------+ OuterCurve
//                          OuterP_2 __---- |   OuterL_1  |
//                             __+---       |             |
//                        __---  | OuterL_2 |             |
//            OuterP_3 _--       |           | W_1        | W_0
//                   _+           |          |            |
//                  /  \       W_2 |         |            |
//                 /    \          |          | InnerL_1  |
//                |      \          | InnerL_2|____-------+ InnerCurve
//                |       \          |____----+          InnerP_0
//               |     .   \    __---+       InnerP_1
//               |          \ /     InnerP_2
//              |     .     /+ InnerP_3
//              |          |
//              |    .    |
//              |         |
//              |        |
//              |        |
// OuterP_{n-1} +--------+ InnerP_{n-1}
//              |        |
//              |        |
//              |        |
//              |        |
//              |        |
//     OuterP_n +--------+ InnerP_n
//
// Returns region with [OuterCurve((OuterT_{2j} + OuterT_{2j-1}) / 2),
//                      OuterCurve((OuterT_{2j} + OuterT_{2j-1}) / 2),
//                      InnerCurve((OuterT_{2j} + OuterT_{2j+1}) / 2),
//                      InnerCurve((OuterT_{2j} + OuterT_{2j+1}) / 2)],
// to start and end with half segment.
//
//                                     _+__----+------+ OuterCurve
//                               _+---- |      |######|
//                         __+---#|     |      |######|
//                    _+---##|####|     |      |######|
//                 _-- |#####|#####|     |      |#####|
//               _+     |#####|#####|    |      |#####|
//              /  \     |#####|####|    |      |#####|
//             /    \     |####|#####|    |     |#####|
//            |      \     |####|####|    |____-+-----+ InnerCurve
//            |       \     |####|____+---+
//           |     .   \   __+---+
//           |          \ /
//          |     .     /+
//          |          |
//          |    .    |
//          |         |
//          |        |
//          |        |
//          +--------+
//          |        |
//          |        |
//          +--------+
//          |########|
//          |########|
//          +--------+

class DashedCornerFinder
{
  typedef mozilla::gfx::Bezier Bezier;
  typedef mozilla::gfx::Float Float;
  typedef mozilla::gfx::Point Point;
  typedef mozilla::gfx::Size Size;

public:
  struct Result
  {
    // Control points for the outer curve and the inner curve.
    //
    //   outerSectionBezier
    //        |
    //        v     _+ 3
    //        ___---#|
    //  0 +---#######|
    //    |###########|
    //     |###########|
    //      |##########|
    //       |##########|
    //        |#########|
    //         |#####____+ 3
    //        0 +----
    //              ^
    //              |
    //   innerSectionBezier
    Bezier outerSectionBezier;
    Bezier innerSectionBezier;

    Result(const Bezier& aOuterSectionBezier,
           const Bezier& aInnerSectionBezier)
     : outerSectionBezier(aOuterSectionBezier),
       innerSectionBezier(aInnerSectionBezier)
    {}
  };

  //                       aCornerDim.width
  //                     |<----------------->|
  //                     |                   |
  //                   --+-------------___---+--
  //                   ^ |         __--      | ^
  //                   | |       _-          | |
  //                   | |     /             | | aBorderWidthH
  //                   | |   /               | |
  //                   | |  |                | v
  //                   | | |             __--+--
  // aCornerDim.height | ||            _-
  //                   | ||           /
  //                   | |           /
  //                   | |          |
  //                   | |          |
  //                   | |         |
  //                   | |         |
  //                   v |         |
  //                   --+---------+
  //                     |         |
  //                     |<------->|
  //                     aBorderWidthV
  DashedCornerFinder(const Bezier& aOuterBezier, const Bezier& aInnerBezier,
                     Float aBorderWidthH, Float aBorderWidthV,
                     const Size& aCornerDim);

  bool HasMore(void) const;
  Result Next(void);

private:
  static const size_t MAX_LOOP = 32;

  // Bezier control points for the outer curve and the inner curve.
  //
  //               ___---+ outer curve
  //           __--      |
  //         _-          |
  //       /             |
  //     /               |
  //    |                |
  //   |             __--+ inner curve
  //  |            _-
  //  |           /
  // |           /
  // |          |
  // |          |
  // |         |
  // |         |
  // |         |
  // +---------+
  Bezier mOuterBezier;
  Bezier mInnerBezier;

  Point mLastOuterP;
  Point mLastInnerP;
  Float mLastOuterT;
  Float mLastInnerT;

  // Length for each segment, ratio of the border width at that point.
  Float mBestDashLength;

  // If one of border-widths is 0, do not calculate mBestDashLength, and draw
  // segments until it reaches the other side or exceeds mMaxCount.
  bool mHasZeroBorderWidth;
  bool mHasMore;

  // The maximum number of segments.
  size_t mMaxCount;

  enum {
    //                      radius.width
    //                 |<----------------->|
    //                 |                   |
    //               --+-------------___---+--
    //               ^ |         __--      | ^
    //               | |       _-          | |
    //               | |     /             + | top-width
    //               | |   /               | |
    //               | |  |                | v
    //               | | |             __--+--
    // radius.height | ||            _-
    //               | ||           /
    //               | |           /
    //               | |          |
    //               | |          |
    //               | |         |
    //               | |         |
    //               v |         |
    //               --+----+----+
    //                 |         |
    //                 |<------->|
    //                  left-width

    // * top-width == left-width
    // * radius.width == radius.height
    // * top-width < radius.width * 2
    //
    // Split the perfect circle's arc into 2n segments, each segment's length is
    // top-width * dashLength. Then split the inner curve and the outer curve
    // with same angles.
    //
    //                        radius.width
    //                 |<---------------------->|
    //                 |                        | v
    //               --+------------------------+--
    //               ^ |                        | | top-width / 2
    //               | | perfect                | |
    //               | | circle's         ___---+--
    //               | | arc          __-+      | ^
    //               | |  |         _-   |      |
    // radius.height | |  |       +       |     +--
    //               | |  |     /  \      |     |
    //               | |  |    |     \     |    |
    //               | |  |   |       \     |   |
    //               | |  +->|          \   |   |
    //               | |    +---__       \   |  |
    //               | |    |     --__     \  | |
    //               | |    |         ---__ \ | |
    //               v |    |              --_\||
    //               --+----+----+--------------+
    //                 |    |    |
    //                 |<-->|    |
    //               left-width / 2
    PERFECT,

    // Other cases.
    //
    // Split the outer curve and the inner curve into 2n segments, each segment
    // satisfies following:
    //   (OuterL_i + InnerL_i) / 2 == dashLength * (W_i + W_{i-1}) / 2
    OTHER
  } mType;

  size_t mI;
  size_t mCount;

  // Determine mType from parameters.
  void DetermineType(Float aBorderWidthH, Float aBorderWidthV);

  // Reset calculation.
  void Reset(void);

  // Find next segment.
  Float FindNext(Float dashLength);

  // Find mBestDashLength for parameters.
  void FindBestDashLength(Float aMinBorderWidth, Float aMaxBorderWidth,
                          Float aMinBorderRadius, Float aMaxBorderRadius);

  // Fill corner with dashes with given dash length, and return the number of
  // segments and last segment's dash length.
  bool GetCountAndLastDashLength(Float aDashLength,
                                 size_t* aCount, Float* aActualDashLength);
};

} // namespace mozilla

#endif /* mozilla_DashedCornerFinder_h_ */

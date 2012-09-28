/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_PATHHELPERS_H_
#define MOZILLA_GFX_PATHHELPERS_H_

#include "2D.h"
#include "mozilla/Constants.h"

namespace mozilla {
namespace gfx {

template <typename T>
void ArcToBezier(T* aSink, const Point &aOrigin, float aRadius, float aStartAngle,
                 float aEndAngle, bool aAntiClockwise)
{
  Point startPoint(aOrigin.x + cos(aStartAngle) * aRadius,
                   aOrigin.y + sin(aStartAngle) * aRadius);

  aSink->LineTo(startPoint);

  // Clockwise we always sweep from the smaller to the larger angle, ccw
  // it's vice versa.
  if (!aAntiClockwise && (aEndAngle < aStartAngle)) {
    Float correction = Float(ceil((aStartAngle - aEndAngle) / (2.0f * M_PI)));
    aEndAngle += float(correction * 2.0f * M_PI);
  } else if (aAntiClockwise && (aStartAngle < aEndAngle)) {
    Float correction = (Float)ceil((aEndAngle - aStartAngle) / (2.0f * M_PI));
    aStartAngle += float(correction * 2.0f * M_PI);
  }

  // Sweeping more than 2 * pi is a full circle.
  if (!aAntiClockwise && (aEndAngle - aStartAngle > 2 * M_PI)) {
    aEndAngle = float(aStartAngle + 2.0f * M_PI);
  } else if (aAntiClockwise && (aStartAngle - aEndAngle > 2.0f * M_PI)) {
    aEndAngle = float(aStartAngle - 2.0f * M_PI);
  }

  // Calculate the total arc we're going to sweep.
  Float arcSweepLeft = fabs(aEndAngle - aStartAngle);

  Float sweepDirection = aAntiClockwise ? -1.0f : 1.0f;

  Float currentStartAngle = aStartAngle;

  while (arcSweepLeft > 0) {
    // We guarantee here the current point is the start point of the next
    // curve segment.
    Float currentEndAngle;

    if (arcSweepLeft > M_PI / 2.0f) {
      currentEndAngle = Float(currentStartAngle + M_PI / 2.0f * sweepDirection);
    } else {
      currentEndAngle = currentStartAngle + arcSweepLeft * sweepDirection;
    }

    Point currentStartPoint(aOrigin.x + cos(currentStartAngle) * aRadius,
                            aOrigin.y + sin(currentStartAngle) * aRadius);
    Point currentEndPoint(aOrigin.x + cos(currentEndAngle) * aRadius,
                          aOrigin.y + sin(currentEndAngle) * aRadius);

    // Calculate kappa constant for partial curve. The sign of angle in the
    // tangent will actually ensure this is negative for a counter clockwise
    // sweep, so changing signs later isn't needed.
    Float kappa = (4.0f / 3.0f) * tan((currentEndAngle - currentStartAngle) / 4.0f) * aRadius;

    Point tangentStart(-sin(currentStartAngle), cos(currentStartAngle));
    Point cp1 = currentStartPoint;
    cp1 += tangentStart * kappa;

    Point revTangentEnd(sin(currentEndAngle), -cos(currentEndAngle));
    Point cp2 = currentEndPoint;
    cp2 += revTangentEnd * kappa;

    aSink->BezierTo(cp1, cp2, currentEndPoint);

    arcSweepLeft -= Float(M_PI / 2.0f);
    currentStartAngle = currentEndAngle;
  }
}

}
}

#endif /* MOZILLA_GFX_PATHHELPERS_H_ */

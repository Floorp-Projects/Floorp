/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SMILKEYSPLINE_H_
#define NS_SMILKEYSPLINE_H_

/**
 * Utility class to provide scaling defined in a keySplines element.
 */
class nsSMILKeySpline
{
public:
  nsSMILKeySpline() { /* caller must call Init later */ }

  /**
   * Creates a new key spline control point description.
   *
   * aX1, etc. are the x1, y1, x2, y2 cubic Bezier control points as defined by
   * SMILANIM 3.2.3. They must each be in the range 0.0 <= x <= 1.0
   */
  nsSMILKeySpline(double aX1, double aY1,
                  double aX2, double aY2)
  {
    Init(aX1, aY1, aX2, aY2);
  }

  void Init(double aX1, double aY1,
            double aX2, double aY2);

  /**
   * Gets the output (y) value for an input (x).
   *
   * @param aX  The input x value. A floating-point number between 0 and
   *            1 (inclusive).
   */
  double GetSplineValue(double aX) const;

  void GetSplineDerivativeValues(double aX, double& aDX, double& aDY) const;

private:
  void
  CalcSampleValues();

  /**
   * Returns x(t) given t, x1, and x2, or y(t) given t, y1, and y2.
   */
  static double
  CalcBezier(double aT, double aA1, double aA2);

  /**
   * Returns dx/dt given t, x1, and x2, or dy/dt given t, y1, and y2.
   */
  static double
  GetSlope(double aT, double aA1, double aA2);

  double
  GetTForX(double aX) const;

  double
  NewtonRaphsonIterate(double aX, double aGuessT) const;

  double
  BinarySubdivide(double aX, double aA, double aB) const;

  static double
  A(double aA1, double aA2)
  {
    return 1.0 - 3.0 * aA2 + 3.0 * aA1;
  }

  static double
  B(double aA1, double aA2)
  {
    return 3.0 * aA2 - 6.0 * aA1;
  }

  static double
  C(double aA1)
  {
    return 3.0 * aA1;
  }

  double               mX1;
  double               mY1;
  double               mX2;
  double               mY2;

  enum { kSplineTableSize = 11 };
  double               mSampleValues[kSplineTableSize];

  static const double  kSampleStepSize;
};

#endif // NS_SMILKEYSPLINE_H_

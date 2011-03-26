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
 * The Original Code is the Mozilla SMIL module.
 *
 * The Initial Developer of the Original Code is Brian Birtles.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Birtles <birtles@gmail.com>
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

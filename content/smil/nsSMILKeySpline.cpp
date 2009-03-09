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

#include "nsSMILKeySpline.h"
#include <math.h>

#define NEWTON_ITERATIONS   4

const double nsSMILKeySpline::kSampleStepSize = 
                                        1.0 / double(kSplineTableSize - 1);

nsSMILKeySpline::nsSMILKeySpline(double aX1,
                                 double aY1,
                                 double aX2,
                                 double aY2)
: mX1(aX1),
  mY1(aY1),
  mX2(aX2),
  mY2(aY2)
{
  if (mX1 != mY1 || mX2 != mY2)
    CalcSampleValues();
}

double
nsSMILKeySpline::GetSplineValue(double aX) const
{
  if (mX1 == mY1 && mX2 == mY2)
    return aX;

  return CalcBezier(GetTForX(aX), mY1, mY2);
}

void
nsSMILKeySpline::CalcSampleValues()
{
  for (int i = 0; i < kSplineTableSize; ++i) {
    mSampleValues[i] = CalcBezier(double(i) * kSampleStepSize, mX1, mX2);
  }
}

/*static*/ double
nsSMILKeySpline::CalcBezier(double aT,
                            double aA1,
                            double aA2)
{
  return A(aA1, aA2) * pow(aT,3) + B(aA1, aA2)*aT*aT + C(aA1) * aT;
}

/*static*/ double
nsSMILKeySpline::GetSlope(double aT,
                          double aA1,
                          double aA2)
{
  double denom = (3.0 * A(aA1, aA2)*aT*aT + 2.0 * B(aA1, aA2) * aT + C(aA1)); 
  return (denom == 0.0) ? 0.0 : 1.0 / denom;
}

double
nsSMILKeySpline::GetTForX(double aX) const
{
  int i;

  // Get an initial guess.
  //
  // Note: This is better than just taking x as our initial guess as cases such
  // as where the control points are (1, 1), (0, 0) will take some 20 iterations
  // to converge to a good accuracy. By taking an initial guess in this way we
  // only need 3~4 iterations depending on the size of the table.
  for (i = 0; i < kSplineTableSize - 2 && mSampleValues[i] < aX; ++i);
  double currentT = 
    double(i) * kSampleStepSize + (aX - mSampleValues[i]) * kSampleStepSize;

  // Refine with Newton-Raphson iteration
  for (i = 0; i < NEWTON_ITERATIONS; ++i) {
    double currentX = CalcBezier(currentT, mX1, mX2);
    double currentSlope = GetSlope(currentT, mX1, mX2);

    if (currentSlope == 0.0)
      return currentT;

    currentT -= (currentX - aX) * currentSlope;
  }

  return currentT;
}

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/***** BEGIN LICENSE BLOCK *****
  Version: MPL 1.1/GPL 2.0/LGPL 2.1

  The contents of this file are subject to the Mozilla Public License Version 
  1.1 (the "License"); you may not use this file except in compliance with 
  the License. You may obtain a copy of the License at 
  http://www.mozilla.org/MPL/

  Software distributed under the License is distributed on an "AS IS" basis,
  WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
  for the specific language governing rights and limitations under the
  License.

  The Original Code is bayesian spam filter

  Portions created by the Initial Developer are Copyright (C) 2004
  the Initial Developer. All Rights Reserved.

  Contributor(s):
    Scott MacGregor <mscott@mozilla.org>
    tenthumbs@cybernex.net

  Alternatively, the contents of this file may be used under the terms of
  either of the GNU General Public License Version 2 or later (the "GPL"),
  or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
  in which case the provisions of the GPL or the LGPL are applicable instead
  of those above. If you wish to allow use of your version of this file only
  under the terms of either the GPL or the LGPL, and not to allow others to
  use your version of this file under the terms of the MPL, indicate your
  decision by deleting the provisions above and replace them with the notice
  and other provisions required by the GPL or the LGPL. If you do not delete
  the provisions above, a recipient may use your version of this file under
  the terms of any one of the MPL, the GPL or the LGPL.

***** END LICENSE BLOCK *****/

#ifndef nsIncompleteGamma_h__
#define nsIncompleteGamma_h__

/* An implementation of the incomplete gamma functions for real
   arguments. P is defined as

                          x
                         /
                  1      [     a - 1  - t
    P(a, x) =  --------  I    t      e    dt
               Gamma(a)  ]
                         /
                          0

   and

                          infinity
                         /
                  1      [     a - 1  - t
    Q(a, x) =  --------  I    t      e    dt
               Gamma(a)  ]
                         /
                          x

   so that P(a,x) + Q(a,x) = 1.

   Both a series expansion and a continued fraction exist.  This
   implementation uses the more efficient method based on the arguments.

   Either case involves calculating a multiplicative term:
      e^(-x)*x^a/Gamma(a).
   Here we calculate the log of this term. Most math libraries have a
   "lgamma" function but it is not re-entrant. Some libraries have a
   "lgamma_r" which is re-entrant. Use it if possible. I have included a
   simple replacement but it is certainly not as accurate.

   Relative errors are almost always < 1e-10 and usually < 1e-14. Very
   small and very large arguments cause trouble.

   The region where a < 0.5 and x < 0.5 has poor error properties and is
   not too stable. Get a better routine if you need results in this
   region.

   The error argument will be set negative if there is a domain error or
   positive for an internal calculation error, currently lack of
   convergence. A value is always returned, though.

 */

#include <math.h>
#include <float.h>

// the main routine
static double nsIncompleteGammaP (double a, double x, int *error);

// nsLnGamma(z): either a wrapper around lgamma_r or the internal function.
// C_m = B[2*m]/(2*m*(2*m-1)) where B is a Bernoulli number
static const double C_1 = 1.0 / 12.0;
static const double C_2 = -1.0 / 360.0;
static const double C_3 = 1.0 / 1260.0;
static const double C_4 = -1.0 / 1680.0;
static const double C_5 = 1.0 / 1188.0;
static const double C_6 = -691.0 / 360360.0;
static const double C_7 = 1.0 / 156.0;
static const double C_8 = -3617.0 / 122400.0;
static const double C_9 = 43867.0 / 244188.0;
static const double C_10 = -174611.0 / 125400.0;
static const double C_11 = 77683.0 / 5796.0;

// truncated asymptotic series in 1/z
static inline double lngamma_asymp (double z)
{
  double w, w2, sum;
  w = 1.0 / z;
  w2 = w * w;
  sum = w * (w2 * (w2 * (w2 * (w2 * (w2 * (w2 * (w2 * (w2 * (w2
        * (C_11 * w2 + C_10) + C_9) + C_8) + C_7) + C_6)
        + C_5) + C_4) + C_3) + C_2) + C_1);

  return sum;
}

struct fact_table_s
{
  double fact;
  double lnfact;
};

// for speed and accuracy
static const struct fact_table_s FactTable[] = {
    {1.000000000000000, 0.0000000000000000000000e+00},
    {1.000000000000000, 0.0000000000000000000000e+00},
    {2.000000000000000, 6.9314718055994530942869e-01},
    {6.000000000000000, 1.7917594692280550007892e+00},
    {24.00000000000000, 3.1780538303479456197550e+00},
    {120.0000000000000, 4.7874917427820459941458e+00},
    {720.0000000000000, 6.5792512120101009952602e+00},
    {5040.000000000000, 8.5251613610654142999881e+00},
    {40320.00000000000, 1.0604602902745250228925e+01},
    {362880.0000000000, 1.2801827480081469610995e+01},
    {3628800.000000000, 1.5104412573075515295248e+01},
    {39916800.00000000, 1.7502307845873885839769e+01},
    {479001600.0000000, 1.9987214495661886149228e+01},
    {6227020800.000000, 2.2552163853123422886104e+01},
    {87178291200.00000, 2.5191221182738681499610e+01},
    {1307674368000.000, 2.7899271383840891566988e+01},
    {20922789888000.00, 3.0671860106080672803835e+01},
    {355687428096000.0, 3.3505073450136888885825e+01},
    {6402373705728000., 3.6395445208033053576674e+01}
};
#define FactTableLength (int)(sizeof(FactTable)/sizeof(FactTable[0]))

// for speed
static const double ln_2pi_2 = 0.918938533204672741803;	// log(2*PI)/2

/* A simple lgamma function, not very robust.

   Valid for z_in > 0 ONLY.

   For z_in > 8 precision is quite good, relative errors < 1e-14 and
   usually better. For z_in < 8 relative errors increase but are usually
   < 1e-10. In two small regions, 1 +/- .001 and 2 +/- .001 errors
   increase quickly.
*/
static double nsLnGamma (double z_in, int *gsign)
{
  double scale, z, sum, result;
  *gsign = 1;

  int zi = (int) z_in;
  if (z_in == (double) zi)
  {
    if (0 < zi && zi <= FactTableLength)
	    return FactTable[zi - 1].lnfact;	// gamma(z) = (z-1)!
  }

  for (scale = 1.0, z = z_in; z < 8.0; ++z)
    scale *= z;
  
  sum = lngamma_asymp (z);
  result = (z - 0.5) * log (z) - z + ln_2pi_2 - log (scale);
  result += sum;
  return result;
}

// log( e^(-x)*x^a/Gamma(a) )
static inline double lnPQfactor (double a, double x)
{
  int gsign;				// ignored because a > 0
  return a * log (x) - x - nsLnGamma (a, &gsign);
}

static double Pseries (double a, double x, int *error)
{
  double sum, term;
  const double eps = 2.0 * DBL_EPSILON;
  const int imax = 5000;
  int i;

  sum = term = 1.0 / a;
  for (i = 1; i < imax; ++i)
  {
	  term *= x / (a + i);
	  sum += term;
	  if (fabs (term) < eps * fabs (sum))
      break;
  }

  if (i >= imax)
	  *error = 1;

  return sum;
}

static double Qcontfrac (double a, double x, int *error)
{
  double result, D, C, e, f, term;
  const double eps = 2.0 * DBL_EPSILON;
  const double small =
  DBL_EPSILON * DBL_EPSILON * DBL_EPSILON * DBL_EPSILON;
  const int imax = 5000;
  int i;

  // modified Lentz method
  f = x - a + 1.0;
  if (fabs (f) < small)
    f = small;
  C = f + 1.0 / small;
  D = 1.0 / f;
  result = D;
  for (i = 1; i < imax; ++i)
  {
    e = i * (a - i);
    f += 2.0;
    D = f + e * D;
    if (fabs (D) < small)
      D = small;
    D = 1.0 / D;
    C = f + e / C;
    if (fabs (C) < small)
      C = small;
    term = C * D;
    result *= term;
    if (fabs (term - 1.0) < eps)
      break;
  }

  if (i >= imax)
    *error = 1;
  return result;
}

static double nsIncompleteGammaP (double a, double x, int *error)
{
  double result, dom, ldom;
  //  domain errors. the return values are meaningless but have
  //  to return something. 
  *error = -1;
  if (a <= 0.0)
	  return 1.0;
  if (x < 0.0)
	  return 0.0;
  *error = 0;
  if (x == 0.0)
    return 0.0;

  ldom = lnPQfactor (a, x);
  dom = exp (ldom);
  // might need to adjust the crossover point
  if (a <= 0.5)
  {
    if (x < a + 1.0)
      result = dom * Pseries (a, x, error);
    else
      result = 1.0 - dom * Qcontfrac (a, x, error);
  }
  else
  {
    if (x < a)
      result = dom * Pseries (a, x, error);
	  else
	    result = 1.0 - dom * Qcontfrac (a, x, error);
  }

  // not clear if this can ever happen
  if (result > 1.0)
    result = 1.0;
  if (result < 0.0)
    result = 0.0;
  return result;
}

#endif


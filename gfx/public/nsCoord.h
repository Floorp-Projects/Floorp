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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef NSCOORD_H
#define NSCOORD_H

#include <math.h>

#include "nsDebug.h"

/*
 * Basic type used for the geometry classes.
 *
 * Normally all coordinates are maintained in the twips coordinate
 * space. A twip is 1/20th of a point, and there are 72 points per
 * inch. However, nscoords do appear in pixel space and other
 * coordinate spaces.
 *
 * Twips are used because they are a device-independent unit of measure. See
 * header file nsUnitConversion.h for many useful macros to convert between
 * different units of measure.
 */

// This controls whether we're using integers or floats for coordinates. We
// want to eventually use floats. If you change this, you need to manually
// change the definition of nscoord in gfx/idl/gfxtypes.idl.
//#define NS_COORD_IS_FLOAT

inline void VERIFY_COORD(nscoord aCoord) {
#ifdef NS_COORD_IS_FLOAT
  NS_ASSERTION(floorf(aCoord) == aCoord,
               "Coords cannot have fractions");
#endif
}

inline float NS_IEEEPositiveInfinity() {
  float f;
  *(PRUint32*)&f = 0x7F800000;
  return f;
}
inline PRBool NS_IEEEIsNan(float aF) {
  PRUint32 bits = *(PRUint32*)&aF;
  return (bits & 0x7F800000) == 0x7F800000 &&
    (bits & 0x007FFFFF) != 0;
}

#ifdef NS_COORD_IS_FLOAT
typedef float nscoord;
#define nscoord_MAX NS_IEEEPositiveInfinity()
#else
typedef PRInt32 nscoord;
#define nscoord_MAX nscoord(1 << 30)
#endif

#define nscoord_MIN (-nscoord_MAX)

inline nscoord NSCoordMultiply(nscoord aCoord, float aVal) {
  VERIFY_COORD(aCoord);
#ifdef NS_COORD_IS_FLOAT
  return floorf(aCoord*aVal);
#else
  return (PRInt32)(aCoord*aVal);
#endif
}

inline nscoord NSCoordMultiply(nscoord aCoord, PRInt32 aVal) {
  VERIFY_COORD(aCoord);
  return aCoord*aVal;
}

inline nscoord NSCoordDivide(nscoord aCoord, float aVal) {
  VERIFY_COORD(aCoord);
#ifdef NS_COORD_IS_FLOAT
  return floorf(aCoord/aVal);
#else
  return (PRInt32)(aCoord/aVal);
#endif
}

inline nscoord NSCoordDivide(nscoord aCoord, PRInt32 aVal) {
  VERIFY_COORD(aCoord);
#ifdef NS_COORD_IS_FLOAT
  return floorf(aCoord/aVal);
#else
  return aCoord/aVal;
#endif
}

/**
 * Convert an nscoord to a PRInt32. This *does not* do rounding because
 * coords are never fractional. They can be out of range, so this does
 * clamp out of bounds coord values to PR_INT32_MIN and PR_INT32_MAX.
 */
inline PRInt32 NSCoordToInt(nscoord aCoord) {
  VERIFY_COORD(aCoord);
#ifdef NS_COORD_IS_FLOAT
  NS_ASSERTION(!NS_IEEEIsNan(aCoord), "NaN encountered in int conversion");
  if (aCoord < -2147483648.0f) {
    // -2147483648 is the smallest 32-bit signed integer that can be
    // exactly represented as a float
    return PR_INT32_MIN;
  } else if (aCoord > 2147483520.0f) {
    // 2147483520 is the largest 32-bit signed integer that can be
    // exactly represented as an IEEE float
    return PR_INT32_MAX;
  } else {
    return (PRInt32)aCoord;
  }
#else
  return aCoord;
#endif
}

inline float NSCoordToFloat(nscoord aCoord) {
  VERIFY_COORD(aCoord);
#ifdef NS_COORD_IS_FLOAT
  NS_ASSERTION(!NS_IEEEIsNan(aCoord), "NaN encountered in float conversion");
#endif
  return aCoord;
}

#endif /* NSCOORD_H */

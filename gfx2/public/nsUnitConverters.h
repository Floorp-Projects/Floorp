/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-2000 Netscape Communications Corporation.
 * All Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#ifndef nsUnitConverters_h__
#define nsUnitConverters_h__

#include "gfxtypes.h"
#include <math.h>

/**
 * gfx unit conversion functions
 * @file nsUnitConverters.h
 */

/**
 * Unit Convertion Constants
 * @defgroup conversion_constants Unit conversion constants
 */

/**
 * ROUND_EXCLUSIVE_CONST_FLOAT
 * @ingroup conversion_constants
 * @note XXX this should be derived from platform FLOAT_MIN
 */
#define ROUND_EXCLUSIVE_CONST_FLOAT   0.4999999999999999

/**
 * ROUND_CONST_FLOAT
 * @ingroup conversion_constants
 */
#define ROUND_CONST_FLOAT             0.5

/**
 * CEIL_CONST_FLOAT
 * @ingroup conversion_constants
 * @note XXX this should be derived from platform FLOAT_MIN
 */
#define CEIL_CONST_FLOAT              0.9999999999999999


/**
 * gfx_coord rounding, floor, ceil functions
 * @defgroup gfx_coord_functions gfx_coord conversion functions
 */


/**
 * Return the absolute value of \a aValue
 * @ingroup gfx_coord_functions
 * @param aValue the value you wish to get the absolute value of
 * @return the absolute value
 */
inline gfx_coord GFXCoordAbs(gfx_coord aValue)
{
  return fabs(aValue);
}


/**
 * Round a gfx_coord
 * @ingroup gfx_coord_functions
 * @param aValue the value you wish to round
 * @return the rounded value
 */
inline gfx_coord GFXCoordRound(gfx_coord aValue)
{
  return (0.0f <= aValue) ? (aValue + ROUND_CONST_FLOAT) : (aValue - ROUND_CONST_FLOAT);
}

/**
 * Floor a gfx_coord
 * @ingroup gfx_coord_functions
 * @param aValue the value you wish to floor
 * @return the floored value
 */
inline gfx_coord GFXCoordFloor(gfx_coord aValue)
{
  return (0.0f <= aValue) ? aValue : (aValue - CEIL_CONST_FLOAT);
}

/**
 * Ceil a gfx_coord
 * @ingroup gfx_coord_functions
 * @param aValue the value you wish to ceil
 * @return the ceiled value
 */
inline gfx_coord GFXCoordCeil(gfx_coord aValue)
{
  return (0.0f <= aValue) ? (aValue + CEIL_CONST_FLOAT) : aValue;
}


/**
 * gfx_coord -> integer
 * @defgroup gfx_coord_to_int_functions gfx_coord to integer conversion functions
 */

/**
 * Round a gfx_coord to a 32bit integer
 * @ingroup gfx_coord_to_int_functions
 * @param aValue the value you wish to round
 * @return the rounded integer value
 */
inline PRInt32 GFXCoordToIntRound(gfx_coord aValue)
{
  return (0.0f <= aValue) ? PRInt32(aValue + ROUND_CONST_FLOAT) : PRInt32(aValue - ROUND_CONST_FLOAT);
}

/**
 * Floor a gfx_coord and convert to a 32bit integer
 * @ingroup gfx_coord_to_int_functions
 * @param aValue the value you wish to floor
 * @return the floored 32bit integer value
 */
inline PRInt32 GFXCoordToIntFloor(gfx_coord aValue)
{
  return (0.0f <= aValue) ? PRInt32(aValue) : PRInt32(aValue - CEIL_CONST_FLOAT);
}

/**
 * Ceil a gfx_coord and convert to a 32bit integer
 * @ingroup gfx_coord_to_int_functions
 * @param aValue the value you wish to ceil
 * @return the ceiled 32bit integer value
 */
inline PRInt32 GFXCoordToIntCeil(gfx_coord aValue)
{
  return (0.0f <= aValue) ? PRInt32(aValue + CEIL_CONST_FLOAT) : PRInt32(aValue);
}

/**
 * gfx_angle -> integer
 * @defgroup gfx_angle_to_int_functions gfx_angle to integer conversion functions
 */

/**
 * Round a gfx_angle to a 32bit integer
 * @ingroup gfx_angle_to_int_functions
 * @param aValue the value you wish to round
 * @return the rounded integer value
 */
inline PRInt32 GFXAngleToIntRound(gfx_angle aValue)
{
  return (0.0f <= aValue) ? PRInt32(aValue + ROUND_CONST_FLOAT) : PRInt32(aValue - ROUND_CONST_FLOAT);
}


/**
 * other... ?
 */

#if 0
inline gfx_coord NSToCoordRoundExclusive(float aValue)
{
  return ((0.0f <= aValue) ? nscoord(aValue + ROUND_EXCLUSIVE_CONST_FLOAT) :
                             nscoord(aValue - ROUND_EXCLUSIVE_CONST_FLOAT));
}

inline PRInt32 NSToIntRoundExclusive(float aValue)
{
  return ((0.0f <= aValue) ? PRInt32(aValue + ROUND_EXCLUSIVE_CONST_FLOAT) :
                             PRInt32(aValue - ROUND_EXCLUSIVE_CONST_FLOAT));
}

#endif


#endif

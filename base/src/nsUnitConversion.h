/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsUnitConversion_h__
#define nsUnitConversion_h__

#include "nscore.h"
#include <math.h>

/// Unit conversion macros
//@{
#define TWIPS_CONST 20
#define TWIPS_CONST_FLOAT 20.0f

#define NS_POINTS_TO_TWIPS_INT(x)     ((PRInt32)(TWIPS_CONST * (x)))
#define NS_TWIPS_TO_POINTS_INT(x)     ((PRInt32)((x) / TWIPS_CONST))

#define NS_POINTS_TO_TWIPS_FLOAT(x)   (TWIPS_CONST_FLOAT * (x))
#define NS_TWIPS_TO_POINTS_FLOAT(x)   ((x) / TWIPS_CONST_FLOAT)

#define NS_INCHES_TO_TWIPS(x)         (72.0f * TWIPS_CONST_FLOAT * (x))                  // 72 points per inch
#define NS_FEET_TO_TWIPS(x)           (72.0f * 12.0f * TWIPS_CONST_FLOAT * (x))
#define NS_MILES_TO_TWIPS(x)          (72.0f * 12.0f * 5280.0f * TWIPS_CONST_FLOAT * (x))

#define NS_MILLIMETERS_TO_TWIPS(x)    (72.0f * 0.03937f * TWIPS_CONST_FLOAT * (x))
#define NS_CENTIMETERS_TO_TWIPS(x)    (72.0f * 0.3937f * TWIPS_CONST_FLOAT * (x))
#define NS_METERS_TO_TWIPS(x)         (72.0f * 39.37f * TWIPS_CONST_FLOAT * (x))
#define NS_KILOMETERS_TO_TWIPS(x)     (72.0f * 39370.0f * TWIPS_CONST_FLOAT * (x))

#define NS_PICAS_TO_TWIPS(x)          (12.0f * TWIPS_CONST_FLOAT * (x))                   // 12 points per pica
#define NS_DIDOTS_TO_TWIPS(x)         ((16.0f / 15.0f) * TWIPS_CONST_FLOAT * (x))         // 15 didots per 16 points
#define NS_CICEROS_TO_TWIPS(x)        ((12.0f * 16.0f / 15.0f) * TWIPS_CONST_FLOAT * (x)) // 12 didots per cicero

#define NS_TWIPS_TO_INCHES(x)         ((1.0f / (72.0f * TWIPS_CONST_FLOAT)) * (x))
#define NS_TWIPS_TO_FEET(x)           ((1.0f / (72.0f * 12.0f * TWIPS_CONST_FLOAT)) * (x))
#define NS_TWIPS_TO_MILES(x)          ((1.0f / (72.0f * 12.0f * 5280.0f * TWIPS_CONST_FLOAT)) * (x))

#define NS_TWIPS_TO_MILLIMETERS(x)    ((1.0f / (72.0f * 0.03937f * TWIPS_CONST_FLOAT)) * (x))
#define NS_TWIPS_TO_CENTIMETERS(x)    ((1.0f / (72.0f * 0.3937f * TWIPS_CONST_FLOAT)) * (x))
#define NS_TWIPS_TO_METERS(x)         ((1.0f / (72.0f * 39.37f * TWIPS_CONST_FLOAT)) * (x))
#define NS_TWIPS_TO_KILOMETERS(x)     ((1.0f / (72.0f * 39370.0f * TWIPS_CONST_FLOAT)) * (x))

#define NS_TWIPS_TO_PICAS(x)          ((1.0f / (12.0f * TWIPS_CONST_FLOAT)) * (x))
#define NS_TWIPS_TO_DIDOTS(x)         ((1.0f / ((16.0f / 15.0f) * TWIPS_CONST_FLOAT)) * (x))
#define NS_TWIPS_TO_CICEROS(x)        ((1.0f / ((12.0f * 16.0f / 15.0f) * TWIPS_CONST_FLOAT)) * (x))
//@}


/// use these for all of your rounding needs...
//@{ 

#define NS_TO_INT_FLOOR(x)            ((PRInt32)floor(x))
#define NS_TO_INT_CEIL(x)             ((PRInt32)ceil(x))
#define NS_TO_INT_ROUND(x)            ((PRInt32)floor((x) + 0.5))
#define NS_TO_INT_ROUND_EXCLUSIVE(x)  ((PRInt32)floor((x) + 0.4999999999999999))
//@}
#endif

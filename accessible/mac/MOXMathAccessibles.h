/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "mozAccessible.h"

@interface MOXMathRootAccessible : mozAccessible

// overrides
- (id)moxMathRootRadicand;

// overrides
- (id)moxMathRootIndex;

@end

@interface MOXMathSquareRootAccessible : mozAccessible

// overrides
- (id)moxMathRootRadicand;

@end

@interface MOXMathFractionAccessible : mozAccessible

// overrides
- (id)moxMathFractionNumerator;

// overrides
- (id)moxMathFractionDenominator;

// overrides
- (NSNumber*)moxMathLineThickness;

@end

@interface MOXMathSubSupAccessible : mozAccessible

// overrides
- (id)moxMathBase;

// overrides
- (id)moxMathSubscript;

// overrides
- (id)moxMathSuperscript;

@end

@interface MOXMathUnderOverAccessible : mozAccessible

// overrides
- (id)moxMathBase;

// overrides
- (id)moxMathUnder;

// overrides
- (id)moxMathOver;

@end

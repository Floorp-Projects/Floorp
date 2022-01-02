/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "MOXMathAccessibles.h"

#import "MacUtils.h"

using namespace mozilla::a11y;

// XXX WebKit also defines the following attributes.
// See bugs 1176970 and 1176983.
// - NSAccessibilityMathFencedOpenAttribute @"AXMathFencedOpen"
// - NSAccessibilityMathFencedCloseAttribute @"AXMathFencedClose"
// - NSAccessibilityMathPrescriptsAttribute @"AXMathPrescripts"
// - NSAccessibilityMathPostscriptsAttribute @"AXMathPostscripts"

@implementation MOXMathRootAccessible

- (id)moxMathRootRadicand {
  return [self childAt:0];
}

- (id)moxMathRootIndex {
  return [self childAt:1];
}

@end

@implementation MOXMathSquareRootAccessible

- (id)moxMathRootRadicand {
  return [self childAt:0];
}

@end

@implementation MOXMathFractionAccessible

- (id)moxMathFractionNumerator {
  return [self childAt:0];
}

- (id)moxMathFractionDenominator {
  return [self childAt:1];
}

// Bug 1639745: This doesn't actually work.
- (NSNumber*)moxMathLineThickness {
  // WebKit sets line thickness to some logical value parsed in the
  // renderer object of the <mfrac> element. It's not clear whether the
  // exact value is relevant to assistive technologies. From a semantic
  // point of view, the only important point is to distinguish between
  // <mfrac> elements that have a fraction bar and those that do not.
  // Per the MathML 3 spec, the latter happens iff the linethickness
  // attribute is of the form [zero-float][optional-unit]. In that case we
  // set line thickness to zero and in the other cases we set it to one.
  if (NSString* thickness =
          utils::GetAccAttr(self, nsGkAtoms::linethickness_)) {
    NSNumberFormatter* formatter =
        [[[NSNumberFormatter alloc] init] autorelease];
    NSNumber* value = [formatter numberFromString:thickness];
    return [NSNumber numberWithBool:[value boolValue]];
  } else {
    return [NSNumber numberWithInteger:0];
  }
}

@end

@implementation MOXMathSubSupAccessible
- (id)moxMathBase {
  return [self childAt:0];
}

- (id)moxMathSubscript {
  if (mRole == roles::MATHML_SUP) {
    return nil;
  }

  return [self childAt:1];
}

- (id)moxMathSuperscript {
  if (mRole == roles::MATHML_SUB) {
    return nil;
  }

  return [self childAt:mRole == roles::MATHML_SUP ? 1 : 2];
}

@end

@implementation MOXMathUnderOverAccessible
- (id)moxMathBase {
  return [self childAt:0];
}

- (id)moxMathUnder {
  if (mRole == roles::MATHML_OVER) {
    return nil;
  }

  return [self childAt:1];
}

- (id)moxMathOver {
  if (mRole == roles::MATHML_UNDER) {
    return nil;
  }

  return [self childAt:mRole == roles::MATHML_OVER ? 1 : 2];
}
@end

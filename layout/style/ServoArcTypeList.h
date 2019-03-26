/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* a list of all Servo Arc<T> types used across bindings, for preprocessing */

// The first argument is the name of the Servo type used inside the Arc.
// This doesn't need to be accurate; it's only used to generate nice looking
// FFI function names.
//
// The second argument is the name of an opaque Gecko type that will
// correspond to the Servo type used inside the Arc.  The convention for the
// the name of the opaque Gecko type is "RawServo{Type}", where {Type} is
// the name of the Servo type or something close to it.
//
// See the comment at the top of ServoBindingTypes.h for how to use these.
//
// If you add an entry to this file, you should also add an impl_arc_ffi!()
// call to servo/components/style/gecko/arc_types.rs.

// clang-format off
// Needs to be a on single line
SERVO_ARC_TYPE(CssRules, ServoCssRules)
SERVO_ARC_TYPE(StyleSheetContents, RawServoStyleSheetContents)
SERVO_ARC_TYPE(DeclarationBlock, RawServoDeclarationBlock)
SERVO_ARC_TYPE(StyleRule, RawServoStyleRule)
SERVO_ARC_TYPE(ImportRule, RawServoImportRule)
SERVO_ARC_TYPE(AnimationValue, RawServoAnimationValue)
SERVO_ARC_TYPE(Keyframe, RawServoKeyframe)
SERVO_ARC_TYPE(KeyframesRule, RawServoKeyframesRule)
SERVO_ARC_TYPE(MediaList, RawServoMediaList)
SERVO_ARC_TYPE(MediaRule, RawServoMediaRule)
SERVO_ARC_TYPE(NamespaceRule, RawServoNamespaceRule)
SERVO_ARC_TYPE(PageRule, RawServoPageRule)
SERVO_ARC_TYPE(SupportsRule, RawServoSupportsRule)
SERVO_ARC_TYPE(DocumentRule, RawServoMozDocumentRule)
SERVO_ARC_TYPE(FontFeatureValuesRule, RawServoFontFeatureValuesRule)
SERVO_ARC_TYPE(FontFaceRule, RawServoFontFaceRule)
SERVO_ARC_TYPE(CounterStyleRule, RawServoCounterStyleRule)
SERVO_ARC_TYPE(CssUrlData, RawServoCssUrlData)
SERVO_ARC_TYPE(Quotes, RawServoQuotes)

// clang-format on

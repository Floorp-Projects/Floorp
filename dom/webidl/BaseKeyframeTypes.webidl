/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://drafts.csswg.org/web-animations/#the-compositeoperation-enumeration
 * https://drafts.csswg.org/web-animations/#dictdef-basepropertyindexedkeyframe
 * https://drafts.csswg.org/web-animations/#dictdef-basekeyframe
 * https://drafts.csswg.org/web-animations/#dictdef-basecomputedkeyframe
 *
 * Copyright © 2016 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

enum CompositeOperation { "replace", "add", "accumulate" };

// NOTE: The order of the values in this enum are important.
//
// We assume that CompositeOperation is a subset of CompositeOperationOrAuto so
// that we can cast between the two types (provided the value is not "auto").
//
// If that assumption ceases to hold we will need to update the conversion
// in KeyframeUtils::GetAnimationPropertiesFromKeyframes.
enum CompositeOperationOrAuto { "replace", "add", "accumulate", "auto" };

// The following dictionary types are not referred to by other .webidl files,
// but we use it for manual JS->IDL and IDL->JS conversions in KeyframeEffect's
// implementation.

[GenerateInit]
dictionary BasePropertyIndexedKeyframe {
  (double? or sequence<double?>) offset = [];
  (UTF8String or sequence<UTF8String>) easing = [];
  (CompositeOperationOrAuto or sequence<CompositeOperationOrAuto>) composite = [];
};

[GenerateInit]
dictionary BaseKeyframe {
  double? offset = null;
  UTF8String easing = "linear";
  CompositeOperationOrAuto composite = "auto";

  // Non-standard extensions

  // Member to allow testing when StyleAnimationValue::ComputeValues fails.
  //
  // Note that we currently only apply this to shorthand properties since
  // it's easier to annotate shorthand property values and because we have
  // only ever observed ComputeValues failing on shorthand values.
  //
  // Bug 1216844 should remove this member since after that bug is fixed we will
  // have a well-defined behavior to use when animation endpoints are not
  // available.
  [ChromeOnly] boolean simulateComputeValuesFailure = false;
};

[GenerateConversionToJS]
dictionary BaseComputedKeyframe : BaseKeyframe {
  double computedOffset;
};

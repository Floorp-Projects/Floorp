/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* a list of Gecko types used across bindings, for preprocessing */

// There are two macros:
//
//   GECKO_BORROWED_TYPE(gecko_type, ffi_type_name)
//   GECKO_BORROWED_MUT_TYPE(gecko_type, ffi_type_name)
//
// GECKO_BORROWED_TYPE will generated Borrowed and BorrowedOrNull types.
//
// GECKO_BORROWED_MUT_TYPE will generate those and the BorrowedMut &
// BorrowedMutOrNull types.
//
// The first argument is the Gecko C++ type name.  This must be a type
// that is in scope in ServoBindingTypes.h; forward declarations may need
// to be added there.
//
// The second argument is the name of a typedef that will be generated,
// equivalent to the actual C++ type.  This name will be used as the basis
// for the generated borrowed type names to be used in FFI signatures in
// C++ and Rust.  The convention for this name is "RawGecko{Type}", where
// {Type} is the name of the C++ type or something close to it.
//
// See the comment at the top of ServoBindingTypes.h for how to use these.

// clang-format off
// Needs to be a on single line
GECKO_BORROWED_TYPE(mozilla::dom::Element, RawGeckoElement)
GECKO_BORROWED_TYPE(nsIDocument, RawGeckoDocument)
GECKO_BORROWED_TYPE(nsINode, RawGeckoNode)
GECKO_BORROWED_TYPE(nsPresContext, RawGeckoPresContext)
GECKO_BORROWED_TYPE(nsXBLBinding, RawGeckoXBLBinding)
GECKO_BORROWED_TYPE_MUT(mozilla::AnimationPropertySegment, RawGeckoAnimationPropertySegment)
GECKO_BORROWED_TYPE_MUT(mozilla::ComputedTiming, RawGeckoComputedTiming)
GECKO_BORROWED_TYPE_MUT(mozilla::dom::StyleChildrenIterator, RawGeckoStyleChildrenIterator)
GECKO_BORROWED_TYPE_MUT(mozilla::GfxMatrix4x4, RawGeckoGfxMatrix4x4)
GECKO_BORROWED_TYPE_MUT(mozilla::URLExtraData, RawGeckoURLExtraData)
GECKO_BORROWED_TYPE_MUT(nsCSSPropertyIDSet, nsCSSPropertyIDSet)
GECKO_BORROWED_TYPE_MUT(nsCSSValue, nsCSSValue)
GECKO_BORROWED_TYPE_MUT(nsStyleAutoArray<mozilla::StyleAnimation>, RawGeckoStyleAnimationList)
GECKO_BORROWED_TYPE_MUT(nsTArray<const RawServoStyleRule*>, RawGeckoServoStyleRuleList)
GECKO_BORROWED_TYPE_MUT(nsTArray<mozilla::ComputedKeyframeValues>, RawGeckoComputedKeyframeValuesList)
GECKO_BORROWED_TYPE_MUT(nsTArray<mozilla::Keyframe>, RawGeckoKeyframeList)
GECKO_BORROWED_TYPE_MUT(nsTArray<mozilla::PropertyValuePair>, RawGeckoPropertyValuePairList)
GECKO_BORROWED_TYPE_MUT(nsTArray<nsCSSPropertyID>, RawGeckoCSSPropertyIDList)
GECKO_BORROWED_TYPE_MUT(nsTArray<nsFontFaceRuleContainer>, RawGeckoFontFaceRuleList)
GECKO_BORROWED_TYPE_MUT(nsTArray<RefPtr<RawServoAnimationValue>>, RawGeckoServoAnimationValueList)
GECKO_BORROWED_TYPE_MUT(nsTimingFunction, nsTimingFunction)
// clang-format on

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* FFI functions for Gecko to call into Servo */

#ifndef mozilla_ServoBindings_h
#define mozilla_ServoBindings_h

#include "mozilla/ServoStyleConsts.h"

// These functions are defined via macros in Rust, so cbindgen doesn't see them.

namespace mozilla {

// The clang we use on windows complains about returning StyleStrong<> and
// StyleOwned<>, since the template parameters are incomplete.
//
// But they only contain pointers so it is ok. Also, this warning hilariously
// doesn't exist in GCC.
//
// A solution for this is to explicitly instantiate the template, but duplicate
// instantiations are an error.
//
// https://github.com/eqrion/cbindgen/issues/402 tracks an improvement to
// cbindgen that would allow it to autogenerate the template instantiations on
// its own.
#pragma GCC diagnostic push
#ifdef __clang__
#  pragma GCC diagnostic ignored "-Wreturn-type-c-linkage"
#endif

extern "C" {

#define BASIC_RULE_FUNCS_WITHOUT_GETTER(type_)                            \
  void Servo_##type_##_Debug(const RawServo##type_*, nsACString* result); \
  void Servo_##type_##_GetCssText(const RawServo##type_*, nsACString* result);

#define BASIC_RULE_FUNCS(type_)                                         \
  StyleStrong<RawServo##type_##Rule> Servo_CssRules_Get##type_##RuleAt( \
      const ServoCssRules* rules, uint32_t index, uint32_t* line,       \
      uint32_t* column);                                                \
  void Servo_StyleSet_##type_##RuleChanged(                             \
      const RawServoStyleSet*, const RawServo##type_##Rule*,            \
      const StyleDomStyleSheet*, StyleRuleChangeKind);                  \
  BASIC_RULE_FUNCS_WITHOUT_GETTER(type_##Rule)

#define GROUP_RULE_FUNCS(type_)                            \
  BASIC_RULE_FUNCS(type_)                                  \
  StyleStrong<ServoCssRules> Servo_##type_##Rule_GetRules( \
      const RawServo##type_##Rule* rule);

BASIC_RULE_FUNCS(Style)
BASIC_RULE_FUNCS(Import)
BASIC_RULE_FUNCS_WITHOUT_GETTER(Keyframe)
BASIC_RULE_FUNCS(Keyframes)
GROUP_RULE_FUNCS(Media)
GROUP_RULE_FUNCS(MozDocument)
BASIC_RULE_FUNCS(Namespace)
BASIC_RULE_FUNCS(Page)
GROUP_RULE_FUNCS(Supports)
GROUP_RULE_FUNCS(LayerBlock)
BASIC_RULE_FUNCS(LayerStatement)
BASIC_RULE_FUNCS(FontFeatureValues)
BASIC_RULE_FUNCS(FontFace)
BASIC_RULE_FUNCS(CounterStyle)
BASIC_RULE_FUNCS(ScrollTimeline)
GROUP_RULE_FUNCS(Container)

#undef GROUP_RULE_FUNCS
#undef BASIC_RULE_FUNCS
#undef BASIC_RULE_FUNCS_WITHOUT_GETTER

#define BASIC_SERDE_FUNCS(type_)                                            \
  bool Servo_##type_##_Deserialize(mozilla::ipc::ByteBuf* input, type_* v); \
  bool Servo_##type_##_Serialize(const type_* v, mozilla::ipc::ByteBuf* output);

using RayFunction = StyleRayFunction<StyleAngle>;
BASIC_SERDE_FUNCS(LengthPercentage)
BASIC_SERDE_FUNCS(StyleRotate)
BASIC_SERDE_FUNCS(StyleScale)
BASIC_SERDE_FUNCS(StyleTranslate)
BASIC_SERDE_FUNCS(StyleTransform)
BASIC_SERDE_FUNCS(StyleOffsetPath)
BASIC_SERDE_FUNCS(StyleOffsetRotate)
BASIC_SERDE_FUNCS(StylePositionOrAuto)
BASIC_SERDE_FUNCS(StyleComputedTimingFunction)

#undef BASIC_SERDE_FUNCS

void Servo_CounterStyleRule_GetDescriptorCssText(
    const RawServoCounterStyleRule* rule, nsCSSCounterDesc desc,
    nsACString* result);

bool Servo_CounterStyleRule_SetDescriptor(const RawServoCounterStyleRule* rule,
                                          nsCSSCounterDesc desc,
                                          const nsACString* value);

}  // extern "C"

#pragma GCC diagnostic pop

}  // namespace mozilla

#endif  // mozilla_ServoBindings_h

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

#define BASIC_RULE_FUNCS_WITHOUT_GETTER_WITH_PREFIX(type_, prefix_)      \
  void Servo_##type_##_Debug(const mozilla::Style##prefix_##type_*,      \
                             nsACString* result);                        \
  void Servo_##type_##_GetCssText(const mozilla::Style##prefix_##type_*, \
                                  nsACString* result);

#define BASIC_RULE_FUNCS_WITHOUT_GETTER_LOCKED(type_) \
  BASIC_RULE_FUNCS_WITHOUT_GETTER_WITH_PREFIX(type_, Locked)
#define BASIC_RULE_FUNCS_WITHOUT_GETTER_UNLOCKED(type_) \
  BASIC_RULE_FUNCS_WITHOUT_GETTER_WITH_PREFIX(type_, )

#define BASIC_RULE_FUNCS_WITH_PREFIX(type_, prefix_)                        \
  StyleStrong<mozilla::Style##prefix_##type_##Rule>                         \
      Servo_CssRules_Get##type_##RuleAt(const StyleLockedCssRules* rules,   \
                                        uint32_t index, uint32_t* line,     \
                                        uint32_t* column);                  \
  void Servo_StyleSet_##type_##RuleChanged(                                 \
      const StylePerDocumentStyleData*, const Style##prefix_##type_##Rule*, \
      const StyleDomStyleSheet*, StyleRuleChangeKind);                      \
  BASIC_RULE_FUNCS_WITHOUT_GETTER_WITH_PREFIX(type_##Rule, prefix_)

#define BASIC_RULE_FUNCS_LOCKED(type_) \
  BASIC_RULE_FUNCS_WITH_PREFIX(type_, Locked)

#define BASIC_RULE_FUNCS_UNLOCKED(type_) BASIC_RULE_FUNCS_WITH_PREFIX(type_, )

#define GROUP_RULE_FUNCS_WITH_PREFIX(type_, prefix_)                      \
  BASIC_RULE_FUNCS_WITH_PREFIX(type_, prefix_)                            \
  StyleStrong<mozilla::StyleLockedCssRules> Servo_##type_##Rule_GetRules( \
      const mozilla::Style##prefix_##type_##Rule* rule);

#define GROUP_RULE_FUNCS_LOCKED(type_) \
  GROUP_RULE_FUNCS_WITH_PREFIX(type_, Locked)

#define GROUP_RULE_FUNCS_UNLOCKED(type_) GROUP_RULE_FUNCS_WITH_PREFIX(type_, )

BASIC_RULE_FUNCS_LOCKED(Style)
BASIC_RULE_FUNCS_LOCKED(Import)
BASIC_RULE_FUNCS_WITHOUT_GETTER_LOCKED(Keyframe)
BASIC_RULE_FUNCS_LOCKED(Keyframes)
GROUP_RULE_FUNCS_UNLOCKED(Media)
GROUP_RULE_FUNCS_UNLOCKED(Document)
BASIC_RULE_FUNCS_UNLOCKED(Namespace)
BASIC_RULE_FUNCS_LOCKED(Page)
BASIC_RULE_FUNCS_UNLOCKED(Property)
GROUP_RULE_FUNCS_UNLOCKED(Supports)
GROUP_RULE_FUNCS_UNLOCKED(LayerBlock)
BASIC_RULE_FUNCS_UNLOCKED(LayerStatement)
BASIC_RULE_FUNCS_UNLOCKED(FontFeatureValues)
BASIC_RULE_FUNCS_UNLOCKED(FontPaletteValues)
BASIC_RULE_FUNCS_LOCKED(FontFace)
BASIC_RULE_FUNCS_LOCKED(CounterStyle)
GROUP_RULE_FUNCS_UNLOCKED(Container)

#undef GROUP_RULE_FUNCS_LOCKED
#undef GROUP_RULE_FUNCS_UNLOCKED
#undef GROUP_RULE_FUNCS_WITH_PREFIX
#undef BASIC_RULE_FUNCS_LOCKED
#undef BASIC_RULE_FUNCS_UNLOCKED
#undef BASIC_RULE_FUNCS_WITH_PREFIX
#undef BASIC_RULE_FUNCS_WITHOUT_GETTER_LOCKED
#undef BASIC_RULE_FUNCS_WITHOUT_GETTER_UNLOCKED
#undef BASIC_RULE_FUNCS_WITHOUT_GETTER_WITH_PREFIX

#define BASIC_SERDE_FUNCS(type_)                                            \
  bool Servo_##type_##_Deserialize(mozilla::ipc::ByteBuf* input, type_* v); \
  bool Servo_##type_##_Serialize(const type_* v, mozilla::ipc::ByteBuf* output);

BASIC_SERDE_FUNCS(LengthPercentage)
BASIC_SERDE_FUNCS(StyleRotate)
BASIC_SERDE_FUNCS(StyleScale)
BASIC_SERDE_FUNCS(StyleTranslate)
BASIC_SERDE_FUNCS(StyleTransform)
BASIC_SERDE_FUNCS(StyleOffsetPath)
BASIC_SERDE_FUNCS(StyleOffsetRotate)
BASIC_SERDE_FUNCS(StylePositionOrAuto)
BASIC_SERDE_FUNCS(StyleOffsetPosition)
BASIC_SERDE_FUNCS(StyleComputedTimingFunction)

#undef BASIC_SERDE_FUNCS

void Servo_CounterStyleRule_GetDescriptorCssText(
    const StyleLockedCounterStyleRule* rule, nsCSSCounterDesc desc,
    nsACString* result);

bool Servo_CounterStyleRule_SetDescriptor(
    const StyleLockedCounterStyleRule* rule, nsCSSCounterDesc desc,
    const nsACString* value);

}  // extern "C"

#pragma GCC diagnostic pop

}  // namespace mozilla

#endif  // mozilla_ServoBindings_h

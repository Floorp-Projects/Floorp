/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Inlined methods for nsStyleContext. Will just redirect to
 * GeckoStyleContext methods when compiled without stylo, but will do
 * virtual dispatch (by checking which kind of container it is)
 * in stylo mode.
 */

#ifndef nsStyleContextInlines_h
#define nsStyleContextInlines_h

#include "nsStyleContext.h"
#include "mozilla/ServoStyleContext.h"
#include "mozilla/GeckoStyleContext.h"
#include "mozilla/ServoUtils.h"

MOZ_DEFINE_STYLO_METHODS(nsStyleContext,
                         mozilla::GeckoStyleContext,
                         mozilla::ServoStyleContext);

nsRuleNode*
nsStyleContext::RuleNode()
{
    MOZ_RELEASE_ASSERT(IsGecko());
    return AsGecko()->RuleNode();
}

ServoComputedValues*
nsStyleContext::ComputedValues()
{
    MOZ_RELEASE_ASSERT(IsServo());
    return AsServo()->ComputedValues();
}

#define STYLE_STRUCT(name_, checkdata_cb_)                      \
const nsStyle##name_ *                                          \
nsStyleContext::Style##name_() {                                \
  return DoGetStyle##name_<true>();                             \
}                                                               \
const nsStyle##name_ *                                          \
nsStyleContext::ThreadsafeStyle##name_() {                      \
  if (mozilla::ServoStyleSet::IsInServoTraversal()) {           \
    return Servo_GetStyle##name_(AsServo()->ComputedValues());  \
  }                                                             \
  return Style##name_();                                        \
}                                                               \
const nsStyle##name_ * nsStyleContext::PeekStyle##name_() {     \
  return DoGetStyle##name_<false>();                            \
}
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

// Helper functions for GetStyle* and PeekStyle*
#define STYLE_STRUCT_INHERITED(name_, checkdata_cb_)                \
template<bool aComputeData>                                         \
const nsStyle##name_ * nsStyleContext::DoGetStyle##name_() {        \
  if (auto gecko = GetAsGecko()) {                                  \
    const nsStyle##name_ * cachedData =                             \
      static_cast<nsStyle##name_*>(                                 \
        gecko->mCachedInheritedData                                 \
        .mStyleStructs[eStyleStruct_##name_]);                      \
    if (cachedData) /* Have it cached already, yay */               \
      return cachedData;                                            \
    if (!aComputeData) {                                            \
      /* We always cache inherited structs on the context when we */\
      /* compute them. */                                           \
      return nullptr;                                               \
    }                                                               \
    /* Have the rulenode deal */                                    \
    AUTO_CHECK_DEPENDENCY(eStyleStruct_##name_);                    \
    const nsStyle##name_ * newData =                                \
      gecko->RuleNode()->                                           \
        GetStyle##name_<aComputeData>(this->AsGecko(), mBits);      \
    /* always cache inherited data on the style context; the rule */\
    /* node set the bit in mBits for us if needed. */               \
    gecko->mCachedInheritedData                                     \
      .mStyleStructs[eStyleStruct_##name_] =                        \
      const_cast<nsStyle##name_ *>(newData);                        \
    return newData;                                                 \
  }                                                                 \
  auto servo = AsServo();                                           \
  /**                                                               \
   * Also (conservatively) set the owning bit in the parent style   \
   * context if we're a text node.                                  \
   *                                                                \
   * This causes the parent element's style context to cache any    \
   * inherited structs we request for a text node, which means we   \
   * don't have to compute change hints for the text node, as       \
   * handling the change on the parent element is sufficient.       \
   *                                                                \
   * Note, however, that we still need to request the style struct  \
   * of the text node itself, since we may run some fixups on it,   \
   * like for text-combine.                                         \
   *                                                                \
   * This model is sound because for the fixed-up values to change, \
   * other properties on the parent need to change too, and we'll   \
   * handle those change hints correctly.                           \
   *                                                                \
   * TODO(emilio): Perhaps we should remove those fixups and handle \
   * those in layout instead. Those fixups are kind of expensive    \
   * for style sharing, and computed style of text nodes is not     \
   * observable. If we do that, we could assert here that the       \
   * inherited structs of both are the same.                        \
   */                                                               \
  if (mPseudoTag == nsCSSAnonBoxes::mozText && aComputeData) {      \
    MOZ_ASSERT(mParent);                                            \
    mParent->AddStyleBit(NS_STYLE_INHERIT_BIT(name_));              \
  }                                                                 \
                                                                    \
  const bool needToCompute = !(mBits & NS_STYLE_INHERIT_BIT(name_));\
  if (!aComputeData && needToCompute) {                             \
    return nullptr;                                                 \
  }                                                                 \
                                                                    \
  const nsStyle##name_* data =                                      \
    Servo_GetStyle##name_(servo->ComputedValues());   \
  /* perform any remaining main thread work on the struct */        \
  if (needToCompute) {                                              \
    MOZ_ASSERT(NS_IsMainThread());                                  \
    MOZ_ASSERT(!mozilla::ServoStyleSet::IsInServoTraversal());      \
    const_cast<nsStyle##name_*>(data)->FinishStyle(PresContext());  \
    /* the ServoStyleContext owns the struct */                     \
    AddStyleBit(NS_STYLE_INHERIT_BIT(name_));                       \
  }                                                                 \
  return data;                                                      \
}

#define STYLE_STRUCT_RESET(name_, checkdata_cb_)                              \
template<bool aComputeData>                                                   \
const nsStyle##name_ * nsStyleContext::DoGetStyle##name_() {                  \
  if (auto gecko = GetAsGecko()) {                                            \
    if (gecko->mCachedResetData) {                                            \
      const nsStyle##name_ * cachedData =                                     \
        static_cast<nsStyle##name_*>(                                         \
          gecko->mCachedResetData->mStyleStructs[eStyleStruct_##name_]);      \
      if (cachedData) /* Have it cached already, yay */                       \
        return cachedData;                                                    \
    }                                                                         \
    /* Have the rulenode deal */                                              \
    AUTO_CHECK_DEPENDENCY(eStyleStruct_##name_);                              \
    return gecko->RuleNode()->GetStyle##name_<aComputeData>(this->AsGecko()); \
  }                                                                           \
  auto servo = AsServo();                                                     \
  const bool needToCompute = !(mBits & NS_STYLE_INHERIT_BIT(name_));          \
  if (!aComputeData && needToCompute) {                                       \
    return nullptr;                                                           \
  }                                                                           \
  const nsStyle##name_* data =                                                \
    Servo_GetStyle##name_(servo->ComputedValues());                           \
  /* perform any remaining main thread work on the struct */                  \
  if (needToCompute) {                                                        \
    const_cast<nsStyle##name_*>(data)->FinishStyle(PresContext());            \
    /* the ServoStyleContext owns the struct */                               \
    AddStyleBit(NS_STYLE_INHERIT_BIT(name_));                                 \
  }                                                                           \
  return data;                                                                \
}
#include "nsStyleStructList.h"
#undef STYLE_STRUCT_RESET
#undef STYLE_STRUCT_INHERITED


nsPresContext*
nsStyleContext::PresContext() const
{
    MOZ_STYLO_FORWARD(PresContext, ())
}

mozilla::GeckoStyleContext*
nsStyleContext::GetParent() const
{
  MOZ_ASSERT(IsGecko(),
             "This should be used only in Gecko-backed style system!");
  if (mParent) {
    return mParent->AsGecko();
  } else {
    return nullptr;
  }
}

bool
nsStyleContext::IsLinkContext() const
{
  return GetStyleIfVisited() && GetStyleIfVisited()->GetParent() == GetParent();
}

void
nsStyleContext::StartBackgroundImageLoads()
{
  // Just get our background struct; that should do the trick
  StyleBackground();
}

#endif // nsStyleContextInlines_h

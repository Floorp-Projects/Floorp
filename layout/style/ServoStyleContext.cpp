/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoStyleContext.h"

#include "nsCSSAnonBoxes.h"
#include "nsStyleConsts.h"
#include "nsStyleStruct.h"
#include "nsPresContext.h"
#include "nsCSSRuleProcessor.h"
#include "mozilla/dom/HTMLBodyElement.h"

#include "mozilla/ServoBindings.h"

namespace mozilla {

ServoStyleContext::ServoStyleContext(
    nsPresContext* aPresContext,
    nsAtom* aPseudoTag,
    CSSPseudoElementType aPseudoType,
    ServoComputedDataForgotten aComputedValues)
  : nsStyleContext(aPseudoTag, aPseudoType)
  , mPresContext(aPresContext)
  , mSource(aComputedValues)
{
  AddStyleBit(Servo_ComputedValues_GetStyleBits(this));
  MOZ_ASSERT(ComputedData());

  // No need to call ApplyStyleFixups here, since fixups are handled by Servo when
  // producing the ServoComputedData.
}

ServoStyleContext*
ServoStyleContext::GetCachedLazyPseudoStyle(CSSPseudoElementType aPseudo) const
{
  MOZ_ASSERT(aPseudo != CSSPseudoElementType::NotPseudo &&
             aPseudo != CSSPseudoElementType::InheritingAnonBox &&
             aPseudo != CSSPseudoElementType::NonInheritingAnonBox);
  MOZ_ASSERT(!IsLazilyCascadedPseudoElement(), "Lazy pseudos can't inherit lazy pseudos");

  if (nsCSSPseudoElements::PseudoElementSupportsUserActionState(aPseudo)) {
    return nullptr;
  }

  return mCachedInheritingStyles.Lookup(nsCSSPseudoElements::GetPseudoAtom(aPseudo));
}

} // namespace mozilla

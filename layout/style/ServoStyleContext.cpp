/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoStyleContext.h"

#include "nsStyleConsts.h"
#include "nsStyleStruct.h"
#include "nsPresContext.h"
#include "nsCSSRuleProcessor.h"
#include "mozilla/dom/HTMLBodyElement.h"

#include "mozilla/ServoBindings.h"

namespace mozilla {

ServoStyleContext::ServoStyleContext(
    nsStyleContext* aParent,
    nsPresContext* aPresContext,
    nsIAtom* aPseudoTag,
    CSSPseudoElementType aPseudoType,
    ServoComputedDataForgotten aComputedValues)
  : nsStyleContext(aParent, aPseudoTag, aPseudoType)
  , mPresContext(aPresContext)
  , mSource(aComputedValues)
{
  AddStyleBit(Servo_ComputedValues_GetStyleBits(this));
  FinishConstruction();

  // No need to call ApplyStyleFixups here, since fixups are handled by Servo when
  // producing the ServoComputedData.
}

} // namespace mozilla

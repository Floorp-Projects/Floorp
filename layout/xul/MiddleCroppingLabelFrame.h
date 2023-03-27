/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_MiddleCroppingLabelFrame_h
#define mozilla_MiddleCroppingLabelFrame_h

#include "MiddleCroppingBlockFrame.h"
namespace mozilla {

// A frame for a <xul:label> or <xul:description> with crop="center"
class MiddleCroppingLabelFrame final : public MiddleCroppingBlockFrame {
 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(MiddleCroppingLabelFrame)

  MiddleCroppingLabelFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : MiddleCroppingBlockFrame(aStyle, aPresContext, kClassID) {}

  void GetUncroppedValue(nsAString& aValue) override;
  nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                            int32_t aModType) override;
};

}  // namespace mozilla

#endif

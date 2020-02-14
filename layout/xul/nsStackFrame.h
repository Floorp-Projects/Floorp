/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**

  Eric D Vaughan
  A frame that can have multiple children. Only one child may be displayed at
one time. So the can be flipped though like a Stack of cards.

**/

#ifndef nsStackFrame_h___
#define nsStackFrame_h___

#include "mozilla/Attributes.h"
#include "nsBoxFrame.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

class nsStackFrame final : public nsBoxFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsStackFrame)

  friend nsIFrame* NS_NewStackFrame(mozilla::PresShell* aPresShell,
                                    ComputedStyle* aStyle);

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(NS_LITERAL_STRING("Stack"), aResult);
  }
#endif

  virtual void BuildDisplayListForChildren(
      nsDisplayListBuilder* aBuilder, const nsDisplayListSet& aLists) override;

 protected:
  explicit nsStackFrame(ComputedStyle* aStyle, nsPresContext* aPresContext);
};  // class nsStackFrame

#endif

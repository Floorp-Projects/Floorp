/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsResizerFrame_h___
#define nsResizerFrame_h___

#include "nsTitleBarFrame.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

class nsResizerFrame final : public nsTitleBarFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsResizerFrame)

  friend nsIFrame* NS_NewResizerFrame(mozilla::PresShell* aPresShell,
                                      ComputedStyle* aStyle);

  nsResizerFrame(ComputedStyle* aStyle, nsPresContext* aPresContext);

};  // class nsResizerFrame

#endif /* nsResizerFrame_h___ */

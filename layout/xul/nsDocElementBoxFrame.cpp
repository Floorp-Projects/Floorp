/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/PresShell.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/FromParser.h"
#include "mozilla/dom/NodeInfo.h"
#include "nsHTMLParts.h"
#include "nsContainerFrame.h"
#include "nsCSSRendering.h"
#include "nsPageFrame.h"
#include "nsStyleConsts.h"
#include "nsGkAtoms.h"
#include "nsBoxFrame.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"

using namespace mozilla;
using namespace mozilla::dom;

class nsDocElementBoxFrame final : public nsBoxFrame {
 public:
  friend nsIFrame* NS_NewBoxFrame(mozilla::PresShell* aPresShell,
                                  ComputedStyle* aStyle);

  explicit nsDocElementBoxFrame(ComputedStyle* aStyle,
                                nsPresContext* aPresContext)
      : nsBoxFrame(aStyle, aPresContext, kClassID, true) {}

  NS_DECL_FRAMEARENA_HELPERS(nsDocElementBoxFrame)

  bool IsFrameOfType(uint32_t aFlags) const override {
    // Override nsBoxFrame.
    if (aFlags & (nsIFrame::eReplacedContainsBlock | nsIFrame::eReplaced))
      return false;
    return nsBoxFrame::IsFrameOfType(aFlags);
  }

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override;
#endif
};

//----------------------------------------------------------------------

nsContainerFrame* NS_NewDocElementBoxFrame(PresShell* aPresShell,
                                           ComputedStyle* aStyle) {
  return new (aPresShell)
      nsDocElementBoxFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsDocElementBoxFrame)

#ifdef DEBUG_FRAME_DUMP
nsresult nsDocElementBoxFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(u"DocElementBox"_ns, aResult);
}
#endif

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// nsSplitterFrame
//

#ifndef nsSplitterFrame_h__
#define nsSplitterFrame_h__

#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"
#include "SimpleXULLeafFrame.h"

class nsSplitterFrameInner;

namespace mozilla {
class PresShell;
}  // namespace mozilla

nsIFrame* NS_NewSplitterFrame(mozilla::PresShell* aPresShell,
                              mozilla::ComputedStyle* aStyle);

class nsSplitterFrame final : public mozilla::SimpleXULLeafFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsSplitterFrame)

  explicit nsSplitterFrame(ComputedStyle* aStyle, nsPresContext* aPresContext);
  void Destroy(DestroyContext&) override;

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"SplitterFrame"_ns, aResult);
  }
#endif

  bool IsHorizontal() const { return mIsHorizontal; }

  // nsIFrame overrides
  nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                            int32_t aModType) override;

  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;

  NS_IMETHOD HandlePress(nsPresContext* aPresContext,
                         mozilla::WidgetGUIEvent* aEvent,
                         nsEventStatus* aEventStatus) override;

  NS_IMETHOD HandleMultiplePress(nsPresContext* aPresContext,
                                 mozilla::WidgetGUIEvent* aEvent,
                                 nsEventStatus* aEventStatus,
                                 bool aControlHeld) override;

  void Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
              const ReflowInput& aReflowInput,
              nsReflowStatus& aStatus) override;

  MOZ_CAN_RUN_SCRIPT
  NS_IMETHOD HandleDrag(nsPresContext* aPresContext,
                        mozilla::WidgetGUIEvent* aEvent,
                        nsEventStatus* aEventStatus) override;

  NS_IMETHOD HandleRelease(nsPresContext* aPresContext,
                           mozilla::WidgetGUIEvent* aEvent,
                           nsEventStatus* aEventStatus) override;

  nsresult HandleEvent(nsPresContext* aPresContext,
                       mozilla::WidgetGUIEvent* aEvent,
                       nsEventStatus* aEventStatus) override;

  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) override;

 private:
  friend class nsSplitterFrameInner;
  RefPtr<nsSplitterFrameInner> mInner;
  bool mIsHorizontal = false;
};  // class nsSplitterFrame

#endif

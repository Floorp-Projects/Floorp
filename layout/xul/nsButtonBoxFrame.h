/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsButtonBoxFrame_h___
#define nsButtonBoxFrame_h___

#include "mozilla/Attributes.h"
#include "nsIDOMEventListener.h"
#include "nsBoxFrame.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

class nsButtonBoxFrame : public nsBoxFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsButtonBoxFrame)

  friend nsIFrame* NS_NewButtonBoxFrame(mozilla::PresShell* aPresShell);

  nsButtonBoxFrame(ComputedStyle*, nsPresContext*, ClassID = kClassID);

  virtual void Init(nsIContent* aContent, nsContainerFrame* aParent,
                    nsIFrame* aPrevInFlow) override;

  virtual void BuildDisplayListForChildren(
      nsDisplayListBuilder* aBuilder, const nsDisplayListSet& aLists) override;

  virtual void DestroyFrom(nsIFrame* aDestructRoot,
                           PostDestroyData& aPostDestroyData) override;

  virtual nsresult HandleEvent(nsPresContext* aPresContext,
                               mozilla::WidgetGUIEvent* aEvent,
                               nsEventStatus* aEventStatus) override;

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  virtual void MouseClicked(mozilla::WidgetGUIEvent* aEvent);

  void Blurred();

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"ButtonBoxFrame"_ns, aResult);
  }
#endif

 private:
  class nsButtonBoxListener final : public nsIDOMEventListener {
   public:
    explicit nsButtonBoxListener(nsButtonBoxFrame* aButtonBoxFrame)
        : mButtonBoxFrame(aButtonBoxFrame) {}

    NS_DECL_NSIDOMEVENTLISTENER

    NS_DECL_ISUPPORTS

   private:
    friend class nsButtonBoxFrame;
    virtual ~nsButtonBoxListener() = default;
    nsButtonBoxFrame* mButtonBoxFrame;
  };

  RefPtr<nsButtonBoxListener> mButtonBoxListener;
  bool mIsHandlingKeyEvent;
};  // class nsButtonBoxFrame

#endif /* nsButtonBoxFrame_h___ */

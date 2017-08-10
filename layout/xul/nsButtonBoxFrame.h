/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsButtonBoxFrame_h___
#define nsButtonBoxFrame_h___

#include "mozilla/Attributes.h"
#include "nsIDOMEventListener.h"
#include "nsBoxFrame.h"

class nsButtonBoxFrame : public nsBoxFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS(nsButtonBoxFrame)

  friend nsIFrame* NS_NewButtonBoxFrame(nsIPresShell* aPresShell);

  explicit nsButtonBoxFrame(nsStyleContext* aContext, ClassID = kClassID);

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;

  virtual void BuildDisplayListForChildren(nsDisplayListBuilder*   aBuilder,
                                           const nsRect&           aDirtyRect,
                                           const nsDisplayListSet& aLists) override;

  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;

  virtual nsresult HandleEvent(nsPresContext* aPresContext,
                               mozilla::WidgetGUIEvent* aEvent,
                               nsEventStatus* aEventStatus) override;

  virtual void MouseClicked(mozilla::WidgetGUIEvent* aEvent)
  { DoMouseClick(aEvent, false); }

  void Blurred();

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(NS_LITERAL_STRING("ButtonBoxFrame"), aResult);
  }
#endif

  /**
   * Our implementation of MouseClicked.
   * @param aTrustEvent if true and aEvent as null, then assume the event was trusted
   */
  void DoMouseClick(mozilla::WidgetGUIEvent* aEvent, bool aTrustEvent);
  void UpdateMouseThrough() override { AddStateBits(NS_FRAME_MOUSE_THROUGH_NEVER); }

private:
  class nsButtonBoxListener final : public nsIDOMEventListener
  {
  public:
    explicit nsButtonBoxListener(nsButtonBoxFrame* aButtonBoxFrame) :
      mButtonBoxFrame(aButtonBoxFrame)
      { }

    NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) override;

    NS_DECL_ISUPPORTS

  private:
    friend class nsButtonBoxFrame;
    virtual ~nsButtonBoxListener() { }
    nsButtonBoxFrame* mButtonBoxFrame;
  };

  RefPtr<nsButtonBoxListener> mButtonBoxListener;
  bool mIsHandlingKeyEvent;
}; // class nsButtonBoxFrame

#endif /* nsButtonBoxFrame_h___ */

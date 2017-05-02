/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// nsScrollbarFrame
//

#ifndef nsScrollbarFrame_h__
#define nsScrollbarFrame_h__

#include "mozilla/Attributes.h"
#include "nsBoxFrame.h"

class nsIScrollbarMediator;

nsIFrame* NS_NewScrollbarFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

class nsScrollbarFrame final : public nsBoxFrame
{
public:
  explicit nsScrollbarFrame(nsStyleContext* aContext)
    : nsBoxFrame(aContext, mozilla::FrameType::Scrollbar)
    , mScrollbarMediator(nullptr)
  {}

  NS_DECL_QUERYFRAME_TARGET(nsScrollbarFrame)

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(NS_LITERAL_STRING("ScrollbarFrame"), aResult);
  }
#endif

  // nsIFrame overrides
  virtual nsresult AttributeChanged(int32_t aNameSpaceID,
                                    nsIAtom* aAttribute,
                                    int32_t aModType) override;

  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  NS_IMETHOD HandlePress(nsPresContext* aPresContext,
                         mozilla::WidgetGUIEvent* aEvent,
                         nsEventStatus* aEventStatus) override;

  NS_IMETHOD HandleMultiplePress(nsPresContext* aPresContext,
                                 mozilla::WidgetGUIEvent* aEvent,
                                 nsEventStatus* aEventStatus,
                                 bool aControlHeld) override;

  NS_IMETHOD HandleDrag(nsPresContext* aPresContext,
                        mozilla::WidgetGUIEvent* aEvent,
                        nsEventStatus* aEventStatus) override;

  NS_IMETHOD HandleRelease(nsPresContext* aPresContext,
                           mozilla::WidgetGUIEvent* aEvent,
                           nsEventStatus* aEventStatus) override;

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;

  virtual void Reflow(nsPresContext*           aPresContext,
                      ReflowOutput&     aDesiredSize,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus&          aStatus) override;

  void SetScrollbarMediatorContent(nsIContent* aMediator);
  nsIScrollbarMediator* GetScrollbarMediator();

  // nsBox methods

  /**
   * Treat scrollbars as clipping their children; overflowing children
   * will not be allowed to set an overflow rect on this
   * frame. This means that when the scroll code decides to hide a
   * scrollframe by setting its height or width to zero, that will
   * hide the children too.
   */
  virtual bool DoesClipChildren() override { return true; }

  virtual nsresult GetXULMargin(nsMargin& aMargin) override;

  /**
   * The following three methods set the value of mIncrement when a
   * scrollbar button is pressed.
   */
  void SetIncrementToLine(int32_t aDirection);
  void SetIncrementToPage(int32_t aDirection);
  void SetIncrementToWhole(int32_t aDirection);
  /**
   * MoveToNewPosition() adds mIncrement to the current position and
   * updates the curpos attribute.
   * @returns The new position after clamping, in CSS Pixels
   * @note This method might destroy the frame, pres shell, and other objects.
   */
  int32_t MoveToNewPosition();
  int32_t GetIncrement() { return mIncrement; }

protected:
  int32_t mIncrement; // Amount to scroll, in CSSPixels
  bool mSmoothScroll;

private:
  nsCOMPtr<nsIContent> mScrollbarMediator;
}; // class nsScrollbarFrame

#endif

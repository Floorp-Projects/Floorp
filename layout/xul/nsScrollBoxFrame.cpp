/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsButtonBoxFrame.h"
#include "nsITimer.h"
#include "nsRepeatService.h"
#include "mozilla/MouseEvents.h"
#include "nsIContent.h"

using namespace mozilla;

class nsAutoRepeatBoxFrame : public nsButtonBoxFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewAutoRepeatBoxFrame(nsIPresShell* aPresShell,
                                            nsStyleContext* aContext);

  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;

  virtual nsresult AttributeChanged(int32_t aNameSpaceID,
                                    nsIAtom* aAttribute,
                                    int32_t aModType) override;

  virtual nsresult HandleEvent(nsPresContext* aPresContext,
                               WidgetGUIEvent* aEvent,
                               nsEventStatus* aEventStatus) override;

  NS_IMETHOD HandlePress(nsPresContext* aPresContext,
                         WidgetGUIEvent* aEvent,
                         nsEventStatus* aEventStatus) override;

  NS_IMETHOD HandleRelease(nsPresContext* aPresContext,
                           WidgetGUIEvent* aEvent,
                           nsEventStatus* aEventStatus) override;

protected:
  explicit nsAutoRepeatBoxFrame(nsStyleContext* aContext):
    nsButtonBoxFrame(aContext) {}
  
  void StartRepeat() {
    if (IsActivatedOnHover()) {
      // No initial delay on hover.
      nsRepeatService::GetInstance()->Start(Notify, this, 0);
    } else {
      nsRepeatService::GetInstance()->Start(Notify, this);
    }
  }
  void StopRepeat() {
    nsRepeatService::GetInstance()->Stop(Notify, this);
  }
  void Notify();
  static void Notify(void* aData) {
    static_cast<nsAutoRepeatBoxFrame*>(aData)->Notify();
  }

  bool mTrustedEvent;
  
  bool IsActivatedOnHover();
};

nsIFrame*
NS_NewAutoRepeatBoxFrame (nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsAutoRepeatBoxFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsAutoRepeatBoxFrame)

nsresult
nsAutoRepeatBoxFrame::HandleEvent(nsPresContext* aPresContext,
                                  WidgetGUIEvent* aEvent,
                                  nsEventStatus* aEventStatus)
{  
  NS_ENSURE_ARG_POINTER(aEventStatus);
  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

  switch(aEvent->message)
  {
    // repeat mode may be "hover" for repeating while the mouse is hovering
    // over the element, otherwise repetition is done while the element is
    // active (pressed).
    case NS_MOUSE_ENTER_WIDGET:
    case NS_MOUSE_OVER:
      if (IsActivatedOnHover()) {
        StartRepeat();
        mTrustedEvent = aEvent->mFlags.mIsTrusted;
      }
      break;

    case NS_MOUSE_EXIT_WIDGET:
    case NS_MOUSE_OUT:
      // always stop on mouse exit
      StopRepeat();
      // Not really necessary but do this to be safe
      mTrustedEvent = false;
      break;

    case NS_MOUSE_CLICK: {
      WidgetMouseEvent* mouseEvent = aEvent->AsMouseEvent();
      if (mouseEvent->IsLeftClickEvent()) {
        // skip button frame handling to prevent click handling
        return nsBoxFrame::HandleEvent(aPresContext, mouseEvent, aEventStatus);
      }
      break;
    }
  }
     
  return nsButtonBoxFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}

NS_IMETHODIMP
nsAutoRepeatBoxFrame::HandlePress(nsPresContext* aPresContext,
                                  WidgetGUIEvent* aEvent,
                                  nsEventStatus* aEventStatus)
{
  if (!IsActivatedOnHover()) {
    StartRepeat();
    mTrustedEvent = aEvent->mFlags.mIsTrusted;
    DoMouseClick(aEvent, mTrustedEvent);
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsAutoRepeatBoxFrame::HandleRelease(nsPresContext* aPresContext,
                                    WidgetGUIEvent* aEvent,
                                    nsEventStatus* aEventStatus)
{
  if (!IsActivatedOnHover()) {
    StopRepeat();
  }
  return NS_OK;
}

nsresult
nsAutoRepeatBoxFrame::AttributeChanged(int32_t aNameSpaceID,
                                       nsIAtom* aAttribute,
                                       int32_t aModType)
{
  if (aAttribute == nsGkAtoms::type) {
    StopRepeat();
  }
  return NS_OK;
}

void
nsAutoRepeatBoxFrame::Notify()
{
  DoMouseClick(nullptr, mTrustedEvent);
}

void
nsAutoRepeatBoxFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  // Ensure our repeat service isn't going... it's possible that a scrollbar can disappear out
  // from under you while you're in the process of scrolling.
  StopRepeat();
  nsButtonBoxFrame::DestroyFrom(aDestructRoot);
}

bool
nsAutoRepeatBoxFrame::IsActivatedOnHover()
{
  return mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::repeat,
                               nsGkAtoms::hover, eCaseMatters);
}

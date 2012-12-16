/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsGUIEvent.h"
#include "nsButtonBoxFrame.h"
#include "nsITimer.h"
#include "nsRepeatService.h"

class nsAutoRepeatBoxFrame : public nsButtonBoxFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewAutoRepeatBoxFrame(nsIPresShell* aPresShell,
                                            nsStyleContext* aContext);

  virtual void DestroyFrom(nsIFrame* aDestructRoot);

  NS_IMETHOD AttributeChanged(int32_t aNameSpaceID,
                              nsIAtom* aAttribute,
                              int32_t aModType);

  NS_IMETHOD HandleEvent(nsPresContext* aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus* aEventStatus);

  NS_IMETHOD HandlePress(nsPresContext* aPresContext, 
                         nsGUIEvent*     aEvent,
                         nsEventStatus*  aEventStatus);

  NS_IMETHOD HandleRelease(nsPresContext* aPresContext, 
                           nsGUIEvent*     aEvent,
                           nsEventStatus*  aEventStatus);

protected:
  nsAutoRepeatBoxFrame(nsIPresShell* aPresShell, nsStyleContext* aContext):
    nsButtonBoxFrame(aPresShell, aContext) {}
  
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
  return new (aPresShell) nsAutoRepeatBoxFrame (aPresShell, aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsAutoRepeatBoxFrame)

NS_IMETHODIMP
nsAutoRepeatBoxFrame::HandleEvent(nsPresContext* aPresContext, 
                                      nsGUIEvent* aEvent,
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
    case NS_MOUSE_ENTER:
    case NS_MOUSE_ENTER_SYNTH:
      if (IsActivatedOnHover()) {
        StartRepeat();
        mTrustedEvent = aEvent->mFlags.mIsTrusted;
      }
      break;

    case NS_MOUSE_EXIT:
    case NS_MOUSE_EXIT_SYNTH:
      // always stop on mouse exit
      StopRepeat();
      // Not really necessary but do this to be safe
      mTrustedEvent = false;
      break;

    case NS_MOUSE_CLICK:
      if (NS_IS_MOUSE_LEFT_CLICK(aEvent)) {
        // skip button frame handling to prevent click handling
         return nsBoxFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
      }
      break;
  }
     
  return nsButtonBoxFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}

NS_IMETHODIMP
nsAutoRepeatBoxFrame::HandlePress(nsPresContext* aPresContext, 
                                  nsGUIEvent* aEvent,
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
                                    nsGUIEvent* aEvent,
                                    nsEventStatus* aEventStatus)
{
  if (!IsActivatedOnHover()) {
    StopRepeat();
  }
  return NS_OK;
}

NS_IMETHODIMP
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

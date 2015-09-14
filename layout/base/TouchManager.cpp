/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "TouchManager.h"
#include "nsPresShell.h"

bool TouchManager::gPreventMouseEvents = false;
nsRefPtrHashtable<nsUint32HashKey, dom::Touch>* TouchManager::gCaptureTouchList;

/*static*/ void
TouchManager::InitializeStatics()
{
  NS_ASSERTION(!gCaptureTouchList, "InitializeStatics called multiple times!");
  gCaptureTouchList = new nsRefPtrHashtable<nsUint32HashKey, dom::Touch>;
}

/*static*/ void
TouchManager::ReleaseStatics()
{
  NS_ASSERTION(gCaptureTouchList, "ReleaseStatics called without Initialize!");
  delete gCaptureTouchList;
  gCaptureTouchList = nullptr;
}

void
TouchManager::Init(PresShell* aPresShell, nsIDocument* aDocument)
{
  mPresShell = aPresShell;
  mDocument = aDocument;
}

void
TouchManager::Destroy()
{
  EvictTouches();
  mDocument = nullptr;
  mPresShell = nullptr;
}

static void
EvictTouchPoint(nsRefPtr<dom::Touch>& aTouch,
                nsIDocument* aLimitToDocument = nullptr)
{
  nsCOMPtr<nsINode> node(do_QueryInterface(aTouch->mTarget));
  if (node) {
    nsIDocument* doc = node->GetCurrentDoc();
    if (doc && (!aLimitToDocument || aLimitToDocument == doc)) {
      nsIPresShell* presShell = doc->GetShell();
      if (presShell) {
        nsIFrame* frame = presShell->GetRootFrame();
        if (frame) {
          nsPoint pt(aTouch->mRefPoint.x, aTouch->mRefPoint.y);
          nsCOMPtr<nsIWidget> widget = frame->GetView()->GetNearestWidget(&pt);
          if (widget) {
            WidgetTouchEvent event(true, eTouchEnd, widget);
            event.widget = widget;
            event.time = PR_IntervalNow();
            event.touches.AppendElement(aTouch);
            nsEventStatus status;
            widget->DispatchEvent(&event, status);
            return;
          }
        }
      }
    }
  }
  if (!node || !aLimitToDocument || node->OwnerDoc() == aLimitToDocument) {
    // We couldn't dispatch touchend. Remove the touch from gCaptureTouchList explicitly.
    TouchManager::gCaptureTouchList->Remove(aTouch->Identifier());
  }
}

static PLDHashOperator
AppendToTouchList(const uint32_t& aKey, nsRefPtr<dom::Touch>& aData, void *aTouchList)
{
  WidgetTouchEvent::TouchArray* touches =
    static_cast<WidgetTouchEvent::TouchArray*>(aTouchList);
  aData->mChanged = false;
  touches->AppendElement(aData);
  return PL_DHASH_NEXT;
}

void
TouchManager::EvictTouches()
{
  WidgetTouchEvent::AutoTouchArray touches;
  gCaptureTouchList->Enumerate(&AppendToTouchList, &touches);
  for (uint32_t i = 0; i < touches.Length(); ++i) {
    EvictTouchPoint(touches[i], mDocument);
  }
}

bool
TouchManager::PreHandleEvent(WidgetEvent* aEvent,
                             nsEventStatus* aStatus,
                             bool& aTouchIsNew,
                             bool& aIsHandlingUserInput,
                             nsCOMPtr<nsIContent>& aCurrentEventContent)
{
  switch (aEvent->mMessage) {
    case eTouchStart: {
      aIsHandlingUserInput = true;
      WidgetTouchEvent* touchEvent = aEvent->AsTouchEvent();
      // if there is only one touch in this touchstart event, assume that it is
      // the start of a new touch session and evict any old touches in the
      // queue
      if (touchEvent->touches.Length() == 1) {
        WidgetTouchEvent::AutoTouchArray touches;
        gCaptureTouchList->Enumerate(&AppendToTouchList, (void *)&touches);
        for (uint32_t i = 0; i < touches.Length(); ++i) {
          EvictTouchPoint(touches[i]);
        }
      }
      // Add any new touches to the queue
      for (uint32_t i = 0; i < touchEvent->touches.Length(); ++i) {
        dom::Touch* touch = touchEvent->touches[i];
        int32_t id = touch->Identifier();
        if (!gCaptureTouchList->Get(id, nullptr)) {
          // If it is not already in the queue, it is a new touch
          touch->mChanged = true;
        }
        touch->mMessage = aEvent->mMessage;
        gCaptureTouchList->Put(id, touch);
      }
      break;
    }
    case eTouchMove: {
      // Check for touches that changed. Mark them add to queue
      WidgetTouchEvent* touchEvent = aEvent->AsTouchEvent();
      WidgetTouchEvent::TouchArray& touches = touchEvent->touches;
      bool haveChanged = false;
      for (int32_t i = touches.Length(); i; ) {
        --i;
        dom::Touch* touch = touches[i];
        if (!touch) {
          continue;
        }
        int32_t id = touch->Identifier();
        touch->mMessage = aEvent->mMessage;

        nsRefPtr<dom::Touch> oldTouch = gCaptureTouchList->GetWeak(id);
        if (!oldTouch) {
          touches.RemoveElementAt(i);
          continue;
        }
        if (!touch->Equals(oldTouch)) {
          touch->mChanged = true;
          haveChanged = true;
        }

        nsCOMPtr<dom::EventTarget> targetPtr = oldTouch->mTarget;
        if (!targetPtr) {
          touches.RemoveElementAt(i);
          continue;
        }
        touch->SetTarget(targetPtr);

        gCaptureTouchList->Put(id, touch);
        // if we're moving from touchstart to touchmove for this touch
        // we allow preventDefault to prevent mouse events
        if (oldTouch->mMessage != touch->mMessage) {
          aTouchIsNew = true;
        }
      }
      // is nothing has changed, we should just return
      if (!haveChanged) {
        if (aTouchIsNew) {
          // however, if this is the first touchmove after a touchstart,
          // it is special in that preventDefault is allowed on it, so
          // we must dispatch it to content even if nothing changed. we
          // arbitrarily pick the first touch point to be the "changed"
          // touch because firing an event with no changed events doesn't
          // work.
          for (uint32_t i = 0; i < touchEvent->touches.Length(); ++i) {
            if (touchEvent->touches[i]) {
              touchEvent->touches[i]->mChanged = true;
              break;
            }
          }
        } else {
          if (gPreventMouseEvents) {
            *aStatus = nsEventStatus_eConsumeNoDefault;
          }
          return false;
        }
      }
      break;
    }
    case eTouchEnd:
      aIsHandlingUserInput = true;
      // Fall through to touchcancel code
    case NS_TOUCH_CANCEL: {
      // Remove the changed touches
      // need to make sure we only remove touches that are ending here
      WidgetTouchEvent* touchEvent = aEvent->AsTouchEvent();
      WidgetTouchEvent::TouchArray& touches = touchEvent->touches;
      for (uint32_t i = 0; i < touches.Length(); ++i) {
        dom::Touch* touch = touches[i];
        if (!touch) {
          continue;
        }
        touch->mMessage = aEvent->mMessage;
        touch->mChanged = true;

        int32_t id = touch->Identifier();
        nsRefPtr<dom::Touch> oldTouch = gCaptureTouchList->GetWeak(id);
        if (!oldTouch) {
          continue;
        }
        nsCOMPtr<EventTarget> targetPtr = oldTouch->mTarget;

        aCurrentEventContent = do_QueryInterface(targetPtr);
        touch->SetTarget(targetPtr);
        gCaptureTouchList->Remove(id);
      }
      // add any touches left in the touch list, but ensure changed=false
      gCaptureTouchList->Enumerate(&AppendToTouchList, (void *)&touches);
      break;
    }
    default:
      break;
  }
  return true;
}

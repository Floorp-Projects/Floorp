/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "TouchManager.h"

#include "gfxPrefs.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/PresShell.h"
#include "nsIFrame.h"
#include "nsView.h"
#include "PositionedEventTargeting.h"

using namespace mozilla::dom;

namespace mozilla {

nsDataHashtable<nsUint32HashKey, TouchManager::TouchInfo>* TouchManager::sCaptureTouchList;

/*static*/ void
TouchManager::InitializeStatics()
{
  NS_ASSERTION(!sCaptureTouchList, "InitializeStatics called multiple times!");
  sCaptureTouchList = new nsDataHashtable<nsUint32HashKey, TouchManager::TouchInfo>;
}

/*static*/ void
TouchManager::ReleaseStatics()
{
  NS_ASSERTION(sCaptureTouchList, "ReleaseStatics called without Initialize!");
  delete sCaptureTouchList;
  sCaptureTouchList = nullptr;
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

static nsIContent*
GetNonAnonymousAncestor(EventTarget* aTarget)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(aTarget));
  if (content && content->IsInNativeAnonymousSubtree()) {
    content = content->FindFirstNonChromeOnlyAccessContent();
  }
  return content;
}

/*static*/ void
TouchManager::EvictTouchPoint(RefPtr<Touch>& aTouch,
                              nsIDocument* aLimitToDocument)
{
  nsCOMPtr<nsINode> node(do_QueryInterface(aTouch->mTarget));
  if (node) {
    nsIDocument* doc = node->GetUncomposedDoc();
    if (doc && (!aLimitToDocument || aLimitToDocument == doc)) {
      nsIPresShell* presShell = doc->GetShell();
      if (presShell) {
        nsIFrame* frame = presShell->GetRootFrame();
        if (frame) {
          nsPoint pt(aTouch->mRefPoint.x, aTouch->mRefPoint.y);
          nsCOMPtr<nsIWidget> widget = frame->GetView()->GetNearestWidget(&pt);
          if (widget) {
            WidgetTouchEvent event(true, eTouchEnd, widget);
            event.mTime = PR_IntervalNow();
            event.mTouches.AppendElement(aTouch);
            nsEventStatus status;
            widget->DispatchEvent(&event, status);
          }
        }
      }
    }
  }
  if (!node || !aLimitToDocument || node->OwnerDoc() == aLimitToDocument) {
    sCaptureTouchList->Remove(aTouch->Identifier());
  }
}

/*static*/ void
TouchManager::AppendToTouchList(WidgetTouchEvent::TouchArray* aTouchList)
{
  for (auto iter = sCaptureTouchList->Iter();
       !iter.Done();
       iter.Next()) {
    RefPtr<Touch>& touch = iter.Data().mTouch;
    touch->mChanged = false;
    aTouchList->AppendElement(touch);
  }
}

void
TouchManager::EvictTouches()
{
  WidgetTouchEvent::AutoTouchArray touches;
  AppendToTouchList(&touches);
  for (uint32_t i = 0; i < touches.Length(); ++i) {
    EvictTouchPoint(touches[i], mDocument);
  }
}

/* static */ nsIFrame*
TouchManager::SetupTarget(WidgetTouchEvent* aEvent, nsIFrame* aFrame)
{
  MOZ_ASSERT(aEvent);

  if (!aEvent || aEvent->mMessage != eTouchStart) {
    // All touch events except for touchstart use a captured target.
    return aFrame;
  }

  uint32_t flags = 0;
  // Setting this flag will skip the scrollbars on the root frame from
  // participating in hit-testing, and we only want that to happen on
  // zoomable platforms (for now).
  if (gfxPrefs::APZAllowZooming()) {
    flags |= INPUT_IGNORE_ROOT_SCROLL_FRAME;
  }
  // if this is a continuing session, ensure that all these events are
  // in the same document by taking the target of the events already in
  // the capture list
  nsCOMPtr<nsIContent> anyTarget;
  if (aEvent->mTouches.Length() > 1) {
    anyTarget = TouchManager::GetAnyCapturedTouchTarget();
  }

  nsPoint eventPoint;
  nsIFrame* frame = aFrame;
  for (int32_t i = aEvent->mTouches.Length(); i; ) {
    --i;
    dom::Touch* touch = aEvent->mTouches[i];

    int32_t id = touch->Identifier();
    if (!TouchManager::HasCapturedTouch(id)) {
      // find the target for this touch
      eventPoint = nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent,
                                                        touch->mRefPoint,
                                                        aFrame);
      nsIFrame* target = FindFrameTargetedByInputEvent(aEvent,
                                                       aFrame,
                                                       eventPoint,
                                                       flags);
      if (target && !anyTarget) {
        target->GetContentForEvent(aEvent, getter_AddRefs(anyTarget));
        while (anyTarget && !anyTarget->IsElement()) {
          anyTarget = anyTarget->GetParent();
        }
        touch->SetTarget(anyTarget);
      } else {
        nsIFrame* newTargetFrame = nullptr;
        for (nsIFrame* f = target; f;
             f = nsLayoutUtils::GetParentOrPlaceholderForCrossDoc(f)) {
          if (f->PresContext()->Document() == anyTarget->OwnerDoc()) {
            newTargetFrame = f;
            break;
          }
          // We must be in a subdocument so jump directly to the root frame.
          // GetParentOrPlaceholderForCrossDoc gets called immediately to
          // jump up to the containing document.
          f = f->PresShell()->GetRootFrame();
        }

        // if we couldn't find a target frame in the same document as
        // anyTarget, remove the touch from the capture touch list, as
        // well as the event->mTouches array. touchmove events that aren't
        // in the captured touch list will be discarded
        if (!newTargetFrame) {
          aEvent->mTouches.RemoveElementAt(i);
        } else {
          target = newTargetFrame;
          nsCOMPtr<nsIContent> targetContent;
          target->GetContentForEvent(aEvent, getter_AddRefs(targetContent));
          while (targetContent && !targetContent->IsElement()) {
            targetContent = targetContent->GetParent();
          }
          touch->SetTarget(targetContent);
        }
      }
      if (target) {
        frame = target;
      }
    } else {
      // This touch is an old touch, we need to ensure that is not
      // marked as changed and set its target correctly
      touch->mChanged = false;

      RefPtr<dom::Touch> oldTouch = TouchManager::GetCapturedTouch(id);
      if (oldTouch) {
        touch->SetTarget(oldTouch->mTarget);
      }
    }
  }
  return FindFrameTargetedByInputEvent(aEvent, frame, eventPoint, flags);
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
      if (touchEvent->mTouches.Length() == 1) {
        WidgetTouchEvent::AutoTouchArray touches;
        AppendToTouchList(&touches);
        for (uint32_t i = 0; i < touches.Length(); ++i) {
          EvictTouchPoint(touches[i]);
        }
      }
      // Add any new touches to the queue
      for (uint32_t i = 0; i < touchEvent->mTouches.Length(); ++i) {
        Touch* touch = touchEvent->mTouches[i];
        int32_t id = touch->Identifier();
        if (!sCaptureTouchList->Get(id, nullptr)) {
          // If it is not already in the queue, it is a new touch
          touch->mChanged = true;
        }
        touch->mMessage = aEvent->mMessage;
        TouchInfo info = { touch, GetNonAnonymousAncestor(touch->mTarget),
                           true };
        sCaptureTouchList->Put(id, info);
      }
      break;
    }
    case eTouchMove: {
      // Check for touches that changed. Mark them add to queue
      WidgetTouchEvent* touchEvent = aEvent->AsTouchEvent();
      WidgetTouchEvent::TouchArray& touches = touchEvent->mTouches;
      bool haveChanged = false;
      for (int32_t i = touches.Length(); i; ) {
        --i;
        Touch* touch = touches[i];
        if (!touch) {
          continue;
        }
        int32_t id = touch->Identifier();
        touch->mMessage = aEvent->mMessage;

        TouchInfo info;
        if (!sCaptureTouchList->Get(id, &info)) {
          touches.RemoveElementAt(i);
          continue;
        }
        RefPtr<Touch> oldTouch = info.mTouch;
        if (!touch->Equals(oldTouch)) {
          touch->mChanged = true;
          haveChanged = true;
        }

        nsCOMPtr<EventTarget> targetPtr = oldTouch->mTarget;
        if (!targetPtr) {
          touches.RemoveElementAt(i);
          continue;
        }
        nsCOMPtr<nsINode> targetNode(do_QueryInterface(targetPtr));
        if (!targetNode->IsInComposedDoc()) {
          targetPtr = do_QueryInterface(info.mNonAnonymousTarget);
        }
        touch->SetTarget(targetPtr);

        info.mTouch = touch;
        // info.mNonAnonymousTarget is still valid from above
        sCaptureTouchList->Put(id, info);
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
          for (uint32_t i = 0; i < touchEvent->mTouches.Length(); ++i) {
            if (touchEvent->mTouches[i]) {
              touchEvent->mTouches[i]->mChanged = true;
              break;
            }
          }
        } else {
          return false;
        }
      }
      break;
    }
    case eTouchEnd:
      aIsHandlingUserInput = true;
      // Fall through to touchcancel code
      MOZ_FALLTHROUGH;
    case eTouchCancel: {
      // Remove the changed touches
      // need to make sure we only remove touches that are ending here
      WidgetTouchEvent* touchEvent = aEvent->AsTouchEvent();
      WidgetTouchEvent::TouchArray& touches = touchEvent->mTouches;
      for (uint32_t i = 0; i < touches.Length(); ++i) {
        Touch* touch = touches[i];
        if (!touch) {
          continue;
        }
        touch->mMessage = aEvent->mMessage;
        touch->mChanged = true;

        int32_t id = touch->Identifier();
        TouchInfo info;
        if (!sCaptureTouchList->Get(id, &info)) {
          continue;
        }
        nsCOMPtr<EventTarget> targetPtr = info.mTouch->mTarget;
        nsCOMPtr<nsINode> targetNode(do_QueryInterface(targetPtr));
        if (targetNode && !targetNode->IsInComposedDoc()) {
          targetPtr = do_QueryInterface(info.mNonAnonymousTarget);
        }

        aCurrentEventContent = do_QueryInterface(targetPtr);
        touch->SetTarget(targetPtr);
        sCaptureTouchList->Remove(id);
      }
      // add any touches left in the touch list, but ensure changed=false
      AppendToTouchList(&touches);
      break;
    }
    case eTouchPointerCancel: {
      // Don't generate pointer events by touch events after eTouchPointerCancel
      // is received.
      WidgetTouchEvent* touchEvent = aEvent->AsTouchEvent();
      WidgetTouchEvent::TouchArray& touches = touchEvent->mTouches;
      for (uint32_t i = 0; i < touches.Length(); ++i) {
        Touch* touch = touches[i];
        if (!touch) {
          continue;
        }
        int32_t id = touch->Identifier();
        TouchInfo info;
        if (!sCaptureTouchList->Get(id, &info)) {
          continue;
        }
        info.mConvertToPointer = false;
        sCaptureTouchList->Put(id, info);
      }
      break;
    }
    default:
      break;
  }
  return true;
}

/*static*/ already_AddRefed<nsIContent>
TouchManager::GetAnyCapturedTouchTarget()
{
  nsCOMPtr<nsIContent> result = nullptr;
  if (sCaptureTouchList->Count() == 0) {
    return result.forget();
  }
  for (auto iter = sCaptureTouchList->Iter(); !iter.Done(); iter.Next()) {
    RefPtr<Touch>& touch = iter.Data().mTouch;
    if (touch) {
      EventTarget* target = touch->GetTarget();
      if (target) {
        result = do_QueryInterface(target);
        break;
      }
    }
  }
  return result.forget();
}

/*static*/ bool
TouchManager::HasCapturedTouch(int32_t aId)
{
  return sCaptureTouchList->Contains(aId);
}

/*static*/ already_AddRefed<Touch>
TouchManager::GetCapturedTouch(int32_t aId)
{
  RefPtr<Touch> touch;
  TouchInfo info;
  if (sCaptureTouchList->Get(aId, &info)) {
    touch = info.mTouch;
  }
  return touch.forget();
}

/*static*/ bool
TouchManager::ShouldConvertTouchToPointer(const Touch* aTouch,
                                          const WidgetTouchEvent* aEvent)
{
  if (!aTouch || !aTouch->convertToPointer) {
    return false;
  }
  TouchInfo info;
  if (!sCaptureTouchList->Get(aTouch->Identifier(), &info)) {
    // This check runs before the TouchManager has the touch registered in its
    // touch list. It's because we dispatching pointer events before handling
    // touch events. So we convert eTouchStart to pointerdown even it's not
    // registered.
    // Check WidgetTouchEvent::mMessage because Touch::mMessage is assigned when
    // pre-handling touch events.
    return aEvent->mMessage == eTouchStart;
  }
  return info.mConvertToPointer;
}

} // namespace mozilla

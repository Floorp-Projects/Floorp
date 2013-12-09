/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "AccessCheck.h"
#include "ipc/IPCMessageUtils.h"
#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsDOMEvent.h"
#include "nsEventStateManager.h"
#include "nsIFrame.h"
#include "nsIContent.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/MutationEvent.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"
#include "nsContentUtils.h"
#include "nsJSEnvironment.h"
#include "mozilla/Preferences.h"
#include "nsLayoutUtils.h"
#include "nsIScrollableFrame.h"
#include "nsDOMEventTargetHelper.h"
#include "nsPIWindowRoot.h"
#include "nsGlobalWindow.h"
#include "nsDeviceContext.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla {
namespace dom {
namespace workers {
extern bool IsCurrentThreadRunningChromeWorker();
} // namespace workers
} // namespace dom
} // namespace mozilla

static char *sPopupAllowedEvents;


nsDOMEvent::nsDOMEvent(mozilla::dom::EventTarget* aOwner,
                       nsPresContext* aPresContext, WidgetEvent* aEvent)
{
  ConstructorInit(aOwner, aPresContext, aEvent);
}

nsDOMEvent::nsDOMEvent(nsPIDOMWindow* aParent)
{
  ConstructorInit(static_cast<nsGlobalWindow *>(aParent), nullptr, nullptr);
}

void
nsDOMEvent::ConstructorInit(mozilla::dom::EventTarget* aOwner,
                            nsPresContext* aPresContext, WidgetEvent* aEvent)
{
  SetIsDOMBinding();
  SetOwner(aOwner);
  mIsMainThreadEvent = mOwner || NS_IsMainThread();
  if (mIsMainThreadEvent) {
    nsJSContext::LikelyShortLivingObjectCreated();
  }

  mPrivateDataDuplicated = false;

  if (aEvent) {
    mEvent = aEvent;
    mEventIsInternal = false;
  }
  else {
    mEventIsInternal = true;
    /*
      A derived class might want to allocate its own type of aEvent
      (derived from WidgetEvent). To do this, it should take care to pass
      a non-nullptr aEvent to this ctor, e.g.:
      
        nsDOMFooEvent::nsDOMFooEvent(..., WidgetEvent* aEvent)
        : nsDOMEvent(..., aEvent ? aEvent : new nsFooEvent())
      
      Then, to override the mEventIsInternal assignments done by the
      base ctor, it should do this in its own ctor:

        nsDOMFooEvent::nsDOMFooEvent(..., WidgetEvent* aEvent)
        ...
        {
          ...
          if (aEvent) {
            mEventIsInternal = false;
          }
          else {
            mEventIsInternal = true;
          }
          ...
        }
     */
    mEvent = new WidgetEvent(false, 0);
    mEvent->time = PR_Now();
  }

  InitPresContextData(aPresContext);
}

void
nsDOMEvent::InitPresContextData(nsPresContext* aPresContext)
{
  mPresContext = aPresContext;
  // Get the explicit original target (if it's anonymous make it null)
  {
    nsCOMPtr<nsIContent> content = GetTargetFromFrame();
    mExplicitOriginalTarget = content;
    if (content && content->IsInAnonymousSubtree()) {
      mExplicitOriginalTarget = nullptr;
    }
  }
}

nsDOMEvent::~nsDOMEvent() 
{
  NS_ASSERT_OWNINGTHREAD(nsDOMEvent);

  if (mEventIsInternal && mEvent) {
    delete mEvent;
  }
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMEvent)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEvent)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMEvent)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMEvent)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMEvent)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDOMEvent)
  if (tmp->mEventIsInternal) {
    tmp->mEvent->target = nullptr;
    tmp->mEvent->currentTarget = nullptr;
    tmp->mEvent->originalTarget = nullptr;
    switch (tmp->mEvent->eventStructType) {
      case NS_MOUSE_EVENT:
      case NS_MOUSE_SCROLL_EVENT:
      case NS_WHEEL_EVENT:
      case NS_SIMPLE_GESTURE_EVENT:
      case NS_POINTER_EVENT:
        tmp->mEvent->AsMouseEventBase()->relatedTarget = nullptr;
        break;
      case NS_DRAG_EVENT: {
        WidgetDragEvent* dragEvent = tmp->mEvent->AsDragEvent();
        dragEvent->dataTransfer = nullptr;
        dragEvent->relatedTarget = nullptr;
        break;
      }
      case NS_CLIPBOARD_EVENT:
        tmp->mEvent->AsClipboardEvent()->clipboardData = nullptr;
        break;
      case NS_MUTATION_EVENT:
        tmp->mEvent->AsMutationEvent()->mRelatedNode = nullptr;
        break;
      case NS_FOCUS_EVENT:
        tmp->mEvent->AsFocusEvent()->relatedTarget = nullptr;
        break;
      default:
        break;
    }
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPresContext);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mExplicitOriginalTarget);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOwner);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsDOMEvent)
  if (tmp->mEventIsInternal) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEvent->target)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEvent->currentTarget)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEvent->originalTarget)
    switch (tmp->mEvent->eventStructType) {
      case NS_MOUSE_EVENT:
      case NS_MOUSE_SCROLL_EVENT:
      case NS_WHEEL_EVENT:
      case NS_SIMPLE_GESTURE_EVENT:
      case NS_POINTER_EVENT:
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mEvent->relatedTarget");
        cb.NoteXPCOMChild(tmp->mEvent->AsMouseEventBase()->relatedTarget);
        break;
      case NS_DRAG_EVENT: {
        WidgetDragEvent* dragEvent = tmp->mEvent->AsDragEvent();
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mEvent->dataTransfer");
        cb.NoteXPCOMChild(dragEvent->dataTransfer);
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mEvent->relatedTarget");
        cb.NoteXPCOMChild(dragEvent->relatedTarget);
        break;
      }
      case NS_CLIPBOARD_EVENT:
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mEvent->clipboardData");
        cb.NoteXPCOMChild(tmp->mEvent->AsClipboardEvent()->clipboardData);
        break;
      case NS_MUTATION_EVENT:
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mEvent->mRelatedNode");
        cb.NoteXPCOMChild(tmp->mEvent->AsMutationEvent()->mRelatedNode);
        break;
      case NS_FOCUS_EVENT:
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mEvent->relatedTarget");
        cb.NoteXPCOMChild(tmp->mEvent->AsFocusEvent()->relatedTarget);
        break;
      default:
        break;
    }
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPresContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mExplicitOriginalTarget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwner)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

bool
nsDOMEvent::IsChrome(JSContext* aCx) const
{
  return mIsMainThreadEvent ?
    xpc::AccessCheck::isChrome(js::GetContextCompartment(aCx)) :
    mozilla::dom::workers::IsCurrentThreadRunningChromeWorker();
}

// nsIDOMEventInterface
NS_METHOD nsDOMEvent::GetType(nsAString& aType)
{
  if (!mIsMainThreadEvent || !mEvent->typeString.IsEmpty()) {
    aType = mEvent->typeString;
    return NS_OK;
  }
  const char* name = GetEventName(mEvent->message);

  if (name) {
    CopyASCIItoUTF16(name, aType);
    return NS_OK;
  } else if (mEvent->message == NS_USER_DEFINED_EVENT && mEvent->userType) {
    aType = Substring(nsDependentAtomString(mEvent->userType), 2); // Remove "on"
    mEvent->typeString = aType;
    return NS_OK;
  }

  aType.Truncate();
  return NS_OK;
}

static EventTarget*
GetDOMEventTarget(nsIDOMEventTarget* aTarget)
{
  return aTarget ? aTarget->GetTargetForDOMEvent() : nullptr;
}

EventTarget*
nsDOMEvent::GetTarget() const
{
  return GetDOMEventTarget(mEvent->target);
}

NS_METHOD
nsDOMEvent::GetTarget(nsIDOMEventTarget** aTarget)
{
  NS_IF_ADDREF(*aTarget = GetTarget());
  return NS_OK;
}

EventTarget*
nsDOMEvent::GetCurrentTarget() const
{
  return GetDOMEventTarget(mEvent->currentTarget);
}

NS_IMETHODIMP
nsDOMEvent::GetCurrentTarget(nsIDOMEventTarget** aCurrentTarget)
{
  NS_IF_ADDREF(*aCurrentTarget = GetCurrentTarget());
  return NS_OK;
}

//
// Get the actual event target node (may have been retargeted for mouse events)
//
already_AddRefed<nsIContent>
nsDOMEvent::GetTargetFromFrame()
{
  if (!mPresContext) { return nullptr; }

  // Get the target frame (have to get the ESM first)
  nsIFrame* targetFrame = mPresContext->EventStateManager()->GetEventTarget();
  if (!targetFrame) { return nullptr; }

  // get the real content
  nsCOMPtr<nsIContent> realEventContent;
  targetFrame->GetContentForEvent(mEvent, getter_AddRefs(realEventContent));
  return realEventContent.forget();
}

EventTarget*
nsDOMEvent::GetExplicitOriginalTarget() const
{
  if (mExplicitOriginalTarget) {
    return mExplicitOriginalTarget;
  }
  return GetTarget();
}

NS_IMETHODIMP
nsDOMEvent::GetExplicitOriginalTarget(nsIDOMEventTarget** aRealEventTarget)
{
  NS_IF_ADDREF(*aRealEventTarget = GetExplicitOriginalTarget());
  return NS_OK;
}

EventTarget*
nsDOMEvent::GetOriginalTarget() const
{
  if (mEvent->originalTarget) {
    return GetDOMEventTarget(mEvent->originalTarget);
  }

  return GetTarget();
}

NS_IMETHODIMP
nsDOMEvent::GetOriginalTarget(nsIDOMEventTarget** aOriginalTarget)
{
  NS_IF_ADDREF(*aOriginalTarget = GetOriginalTarget());
  return NS_OK;
}

NS_IMETHODIMP_(void)
nsDOMEvent::SetTrusted(bool aTrusted)
{
  mEvent->mFlags.mIsTrusted = aTrusted;
}

bool
nsDOMEvent::Init(mozilla::dom::EventTarget* aGlobal)
{
  if (!mIsMainThreadEvent) {
    return nsContentUtils::ThreadsafeIsCallerChrome();
  }
  bool trusted = false;
  nsCOMPtr<nsPIDOMWindow> w = do_QueryInterface(aGlobal);
  if (w) {
    nsCOMPtr<nsIDocument> d = w->GetExtantDoc();
    if (d) {
      trusted = nsContentUtils::IsChromeDoc(d);
      nsIPresShell* s = d->GetShell();
      if (s) {
        InitPresContextData(s->GetPresContext());
      }
    }
  }
  return trusted;
}

//static
already_AddRefed<nsDOMEvent>
nsDOMEvent::Constructor(const mozilla::dom::GlobalObject& aGlobal,
                        const nsAString& aType,
                        const mozilla::dom::EventInit& aParam,
                        mozilla::ErrorResult& aRv)
{
  nsCOMPtr<mozilla::dom::EventTarget> t = do_QueryInterface(aGlobal.GetAsSupports());
  nsRefPtr<nsDOMEvent> e = new nsDOMEvent(t, nullptr, nullptr);
  bool trusted = e->Init(t);
  aRv = e->InitEvent(aType, aParam.mBubbles, aParam.mCancelable);
  e->SetTrusted(trusted);
  return e.forget();
}

uint16_t
nsDOMEvent::EventPhase() const
{
  // Note, remember to check that this works also
  // if or when Bug 235441 is fixed.
  if ((mEvent->currentTarget &&
       mEvent->currentTarget == mEvent->target) ||
       mEvent->mFlags.InTargetPhase()) {
    return nsIDOMEvent::AT_TARGET;
  }
  if (mEvent->mFlags.mInCapturePhase) {
    return nsIDOMEvent::CAPTURING_PHASE;
  }
  if (mEvent->mFlags.mInBubblingPhase) {
    return nsIDOMEvent::BUBBLING_PHASE;
  }
  return nsIDOMEvent::NONE;
}

NS_IMETHODIMP
nsDOMEvent::GetEventPhase(uint16_t* aEventPhase)
{
  *aEventPhase = EventPhase();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMEvent::GetBubbles(bool* aBubbles)
{
  *aBubbles = Bubbles();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMEvent::GetCancelable(bool* aCancelable)
{
  *aCancelable = Cancelable();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMEvent::GetTimeStamp(uint64_t* aTimeStamp)
{
  *aTimeStamp = TimeStamp();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMEvent::StopPropagation()
{
  mEvent->mFlags.mPropagationStopped = true;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMEvent::StopImmediatePropagation()
{
  mEvent->mFlags.mPropagationStopped = true;
  mEvent->mFlags.mImmediatePropagationStopped = true;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMEvent::GetIsTrusted(bool *aIsTrusted)
{
  *aIsTrusted = IsTrusted();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMEvent::PreventDefault()
{
  // This method is called only from C++ code which must handle default action
  // of this event.  So, pass true always.
  PreventDefaultInternal(true);
  return NS_OK;
}

void
nsDOMEvent::PreventDefault(JSContext* aCx)
{
  MOZ_ASSERT(aCx, "JS context must be specified");

  // Note that at handling default action, another event may be dispatched.
  // Then, JS in content mey be call preventDefault()
  // even in the event is in system event group.  Therefore, don't refer
  // mInSystemGroup here.
  PreventDefaultInternal(IsChrome(aCx));
}

void
nsDOMEvent::PreventDefaultInternal(bool aCalledByDefaultHandler)
{
  if (!mEvent->mFlags.mCancelable) {
    return;
  }

  mEvent->mFlags.mDefaultPrevented = true;

  // Note that even if preventDefault() has already been called by chrome,
  // a call of preventDefault() by content needs to overwrite
  // mDefaultPreventedByContent to true because in such case, defaultPrevented
  // must be true when web apps check it after they call preventDefault().
  if (!aCalledByDefaultHandler) {
    mEvent->mFlags.mDefaultPreventedByContent = true;
  }
}

void
nsDOMEvent::SetEventType(const nsAString& aEventTypeArg)
{
  if (mIsMainThreadEvent) {
    mEvent->userType =
      nsContentUtils::GetEventIdAndAtom(aEventTypeArg, mEvent->eventStructType,
                                        &(mEvent->message));
  } else {
    mEvent->userType = nullptr;
    mEvent->message = NS_USER_DEFINED_EVENT;
    mEvent->typeString = aEventTypeArg;
  }
}

NS_IMETHODIMP
nsDOMEvent::InitEvent(const nsAString& aEventTypeArg, bool aCanBubbleArg, bool aCancelableArg)
{
  // Make sure this event isn't already being dispatched.
  NS_ENSURE_TRUE(!mEvent->mFlags.mIsBeingDispatched, NS_OK);

  if (IsTrusted()) {
    // Ensure the caller is permitted to dispatch trusted DOM events.
    if (!nsContentUtils::ThreadsafeIsCallerChrome()) {
      SetTrusted(false);
    }
  }

  SetEventType(aEventTypeArg);

  mEvent->mFlags.mBubbles = aCanBubbleArg;
  mEvent->mFlags.mCancelable = aCancelableArg;

  mEvent->mFlags.mDefaultPrevented = false;

  // Clearing the old targets, so that the event is targeted correctly when
  // re-dispatching it.
  mEvent->target = nullptr;
  mEvent->originalTarget = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMEvent::DuplicatePrivateData()
{
  // FIXME! Simplify this method and make it somehow easily extendable,
  //        Bug 329127
  
  NS_ASSERTION(mEvent, "No WidgetEvent for nsDOMEvent duplication!");
  if (mEventIsInternal) {
    return NS_OK;
  }

  WidgetEvent* newEvent = nullptr;
  uint32_t msg = mEvent->message;

  switch (mEvent->eventStructType) {
    case NS_EVENT:
    {
      newEvent = new WidgetEvent(false, msg);
      newEvent->AssignEventData(*mEvent, true);
      break;
    }
    case NS_GUI_EVENT:
    {
      WidgetGUIEvent* oldGUIEvent = mEvent->AsGUIEvent();
      // Not copying widget, it is a weak reference.
      WidgetGUIEvent* guiEvent = new WidgetGUIEvent(false, msg, nullptr);
      guiEvent->AssignGUIEventData(*oldGUIEvent, true);
      newEvent = guiEvent;
      break;
    }
    case NS_INPUT_EVENT:
    {
      WidgetInputEvent* oldInputEvent = mEvent->AsInputEvent();
      WidgetInputEvent* inputEvent = new WidgetInputEvent(false, msg, nullptr);
      inputEvent->AssignInputEventData(*oldInputEvent, true);
      newEvent = inputEvent;
      break;
    }
    case NS_KEY_EVENT:
    {
      WidgetKeyboardEvent* oldKeyEvent = mEvent->AsKeyboardEvent();
      WidgetKeyboardEvent* keyEvent =
        new WidgetKeyboardEvent(false, msg, nullptr);
      keyEvent->AssignKeyEventData(*oldKeyEvent, true);
      newEvent = keyEvent;
      break;
    }
    case NS_MOUSE_EVENT:
    {
      WidgetMouseEvent* oldMouseEvent = mEvent->AsMouseEvent();
      WidgetMouseEvent* mouseEvent =
        new WidgetMouseEvent(false, msg, nullptr, oldMouseEvent->reason);
      mouseEvent->AssignMouseEventData(*oldMouseEvent, true);
      newEvent = mouseEvent;
      break;
    }
    case NS_DRAG_EVENT:
    {
      WidgetDragEvent* oldDragEvent = mEvent->AsDragEvent();
      WidgetDragEvent* dragEvent = new WidgetDragEvent(false, msg, nullptr);
      dragEvent->AssignDragEventData(*oldDragEvent, true);
      newEvent = dragEvent;
      break;
    }
    case NS_CLIPBOARD_EVENT:
    {
      InternalClipboardEvent* oldClipboardEvent = mEvent->AsClipboardEvent();
      InternalClipboardEvent* clipboardEvent =
        new InternalClipboardEvent(false, msg);
      clipboardEvent->AssignClipboardEventData(*oldClipboardEvent, true);
      newEvent = clipboardEvent;
      break;
    }
    case NS_SCRIPT_ERROR_EVENT:
    {
      InternalScriptErrorEvent* oldScriptErrorEvent =
        mEvent->AsScriptErrorEvent();
      InternalScriptErrorEvent* scriptErrorEvent =
        new InternalScriptErrorEvent(false, msg);
      scriptErrorEvent->AssignScriptErrorEventData(*oldScriptErrorEvent, true);
      newEvent = scriptErrorEvent;
      break;
    }
    case NS_TEXT_EVENT:
    {
      WidgetTextEvent* oldTextEvent = mEvent->AsTextEvent();
      WidgetTextEvent* textEvent = new WidgetTextEvent(false, msg, nullptr);
      textEvent->AssignTextEventData(*oldTextEvent, true);
      newEvent = textEvent;
      break;
    }
    case NS_COMPOSITION_EVENT:
    {
      WidgetCompositionEvent* compositionEvent =
        new WidgetCompositionEvent(false, msg, nullptr);
      WidgetCompositionEvent* oldCompositionEvent =
        mEvent->AsCompositionEvent();
      compositionEvent->AssignCompositionEventData(*oldCompositionEvent, true);
      newEvent = compositionEvent;
      break;
    }
    case NS_MOUSE_SCROLL_EVENT:
    {
      WidgetMouseScrollEvent* oldMouseScrollEvent =
        mEvent->AsMouseScrollEvent();
      WidgetMouseScrollEvent* mouseScrollEvent =
        new WidgetMouseScrollEvent(false, msg, nullptr);
      mouseScrollEvent->AssignMouseScrollEventData(*oldMouseScrollEvent, true);
      newEvent = mouseScrollEvent;
      break;
    }
    case NS_WHEEL_EVENT:
    {
      WidgetWheelEvent* oldWheelEvent = mEvent->AsWheelEvent();
      WidgetWheelEvent* wheelEvent = new WidgetWheelEvent(false, msg, nullptr);
      wheelEvent->AssignWheelEventData(*oldWheelEvent, true);
      newEvent = wheelEvent;
      break;
    }
    case NS_SCROLLPORT_EVENT:
    {
      InternalScrollPortEvent* oldScrollPortEvent = mEvent->AsScrollPortEvent();
      InternalScrollPortEvent* scrollPortEvent =
        new InternalScrollPortEvent(false, msg, nullptr);
      scrollPortEvent->AssignScrollPortEventData(*oldScrollPortEvent, true);
      newEvent = scrollPortEvent;
      break;
    }
    case NS_SCROLLAREA_EVENT:
    {
      InternalScrollAreaEvent* oldScrollAreaEvent = mEvent->AsScrollAreaEvent();
      InternalScrollAreaEvent* scrollAreaEvent = 
        new InternalScrollAreaEvent(false, msg, nullptr);
      scrollAreaEvent->AssignScrollAreaEventData(*oldScrollAreaEvent, true);
      newEvent = scrollAreaEvent;
      break;
    }
    case NS_MUTATION_EVENT:
    {
      InternalMutationEvent* mutationEvent =
        new InternalMutationEvent(false, msg);
      InternalMutationEvent* oldMutationEvent = mEvent->AsMutationEvent();
      mutationEvent->AssignMutationEventData(*oldMutationEvent, true);
      newEvent = mutationEvent;
      break;
    }
    case NS_FORM_EVENT:
    {
      InternalFormEvent* oldFormEvent = mEvent->AsFormEvent();
      InternalFormEvent* formEvent = new InternalFormEvent(false, msg);
      formEvent->AssignFormEventData(*oldFormEvent, true);
      newEvent = formEvent;
      break;
    }
    case NS_FOCUS_EVENT:
    {
      InternalFocusEvent* newFocusEvent = new InternalFocusEvent(false, msg);
      InternalFocusEvent* oldFocusEvent = mEvent->AsFocusEvent();
      newFocusEvent->AssignFocusEventData(*oldFocusEvent, true);
      newEvent = newFocusEvent;
      break;
    }
    case NS_COMMAND_EVENT:
    {
      WidgetCommandEvent* oldCommandEvent = mEvent->AsCommandEvent();
      WidgetCommandEvent* commandEvent =
        new WidgetCommandEvent(false, mEvent->userType,
                               oldCommandEvent->command, nullptr);
      commandEvent->AssignCommandEventData(*oldCommandEvent, true);
      newEvent = commandEvent;
      break;
    }
    case NS_UI_EVENT:
    {
      InternalUIEvent* oldUIEvent = mEvent->AsUIEvent();
      InternalUIEvent* uiEvent =
        new InternalUIEvent(false, msg, oldUIEvent->detail);
      uiEvent->AssignUIEventData(*oldUIEvent, true);
      newEvent = uiEvent;
      break;
    }
    case NS_SVGZOOM_EVENT:
    {
      WidgetGUIEvent* oldGUIEvent = mEvent->AsGUIEvent();
      WidgetGUIEvent* guiEvent = new WidgetGUIEvent(false, msg, nullptr);
      guiEvent->eventStructType = NS_SVGZOOM_EVENT;
      guiEvent->AssignGUIEventData(*oldGUIEvent, true);
      newEvent = guiEvent;
      break;
    }
    case NS_SMIL_TIME_EVENT:
    {
      InternalUIEvent* oldUIEvent = mEvent->AsUIEvent();
      InternalUIEvent* uiEvent = new InternalUIEvent(false, msg, 0);
      uiEvent->eventStructType = NS_SMIL_TIME_EVENT;
      uiEvent->AssignUIEventData(*oldUIEvent, true);
      newEvent = uiEvent;
      break;
    }
    case NS_SIMPLE_GESTURE_EVENT:
    {
      WidgetSimpleGestureEvent* oldSimpleGestureEvent =
        mEvent->AsSimpleGestureEvent();
      WidgetSimpleGestureEvent* simpleGestureEvent = 
        new WidgetSimpleGestureEvent(false, msg, nullptr, 0, 0.0);
      simpleGestureEvent->
        AssignSimpleGestureEventData(*oldSimpleGestureEvent, true);
      newEvent = simpleGestureEvent;
      break;
    }
    case NS_TRANSITION_EVENT:
    {
      InternalTransitionEvent* oldTransitionEvent = mEvent->AsTransitionEvent();
      InternalTransitionEvent* transitionEvent =
         new InternalTransitionEvent(false, msg,
                                     oldTransitionEvent->propertyName,
                                     oldTransitionEvent->elapsedTime,
                                     oldTransitionEvent->pseudoElement);
      transitionEvent->AssignTransitionEventData(*oldTransitionEvent, true);
      newEvent = transitionEvent;
      break;
    }
    case NS_ANIMATION_EVENT:
    {
      InternalAnimationEvent* oldAnimationEvent = mEvent->AsAnimationEvent();
      InternalAnimationEvent* animationEvent =
        new InternalAnimationEvent(false, msg,
                                   oldAnimationEvent->animationName,
                                   oldAnimationEvent->elapsedTime,
                                   oldAnimationEvent->pseudoElement);
      animationEvent->AssignAnimationEventData(*oldAnimationEvent, true);
      newEvent = animationEvent;
      break;
    }
    case NS_TOUCH_EVENT:
    {
      WidgetTouchEvent* oldTouchEvent = mEvent->AsTouchEvent();
      WidgetTouchEvent* touchEvent = new WidgetTouchEvent(false, oldTouchEvent);
      touchEvent->AssignTouchEventData(*oldTouchEvent, true);
      newEvent = touchEvent;
      break;
    }
    case NS_POINTER_EVENT:
    {
      WidgetPointerEvent* oldPointerEvent = mEvent->AsPointerEvent();
      WidgetPointerEvent* pointerEvent =
        new WidgetPointerEvent(false, msg, nullptr,
                               oldPointerEvent->pointerId,
                               oldPointerEvent->width,
                               oldPointerEvent->height,
                               oldPointerEvent->tiltX,
                               oldPointerEvent->tiltY,
                               oldPointerEvent->isPrimary);
      pointerEvent->buttons = oldPointerEvent->buttons;
      newEvent = pointerEvent;
      break;
    }
    default:
    {
      NS_WARNING("Unknown event type!!!");
      return NS_ERROR_FAILURE;
    }
  }

  newEvent->mFlags = mEvent->mFlags;

  mEvent = newEvent;
  mPresContext = nullptr;
  mEventIsInternal = true;
  mPrivateDataDuplicated = true;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMEvent::SetTarget(nsIDOMEventTarget* aTarget)
{
#ifdef DEBUG
  {
    nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(aTarget);

    NS_ASSERTION(!win || !win->IsInnerWindow(),
                 "Uh, inner window set as event target!");
  }
#endif

  mEvent->target = do_QueryInterface(aTarget);
  return NS_OK;
}

NS_IMETHODIMP_(bool)
nsDOMEvent::IsDispatchStopped()
{
  return mEvent->mFlags.mPropagationStopped;
}

NS_IMETHODIMP_(WidgetEvent*)
nsDOMEvent::GetInternalNSEvent()
{
  return mEvent;
}

NS_IMETHODIMP_(nsDOMEvent*)
nsDOMEvent::InternalDOMEvent()
{
  return this;
}

// return true if eventName is contained within events, delimited by
// spaces
static bool
PopupAllowedForEvent(const char *eventName)
{
  if (!sPopupAllowedEvents) {
    nsDOMEvent::PopupAllowedEventsChanged();

    if (!sPopupAllowedEvents) {
      return false;
    }
  }

  nsDependentCString events(sPopupAllowedEvents);

  nsAFlatCString::const_iterator start, end;
  nsAFlatCString::const_iterator startiter(events.BeginReading(start));
  events.EndReading(end);

  while (startiter != end) {
    nsAFlatCString::const_iterator enditer(end);

    if (!FindInReadable(nsDependentCString(eventName), startiter, enditer))
      return false;

    // the match is surrounded by spaces, or at a string boundary
    if ((startiter == start || *--startiter == ' ') &&
        (enditer == end || *enditer == ' ')) {
      return true;
    }

    // Move on and see if there are other matches. (The delimitation
    // requirement makes it pointless to begin the next search before
    // the end of the invalid match just found.)
    startiter = enditer;
  }

  return false;
}

// static
PopupControlState
nsDOMEvent::GetEventPopupControlState(WidgetEvent* aEvent)
{
  // generally if an event handler is running, new windows are disallowed.
  // check for exceptions:
  PopupControlState abuse = openAbused;

  switch(aEvent->eventStructType) {
  case NS_EVENT :
    // For these following events only allow popups if they're
    // triggered while handling user input. See
    // nsPresShell::HandleEventInternal() for details.
    if (nsEventStateManager::IsHandlingUserInput()) {
      switch(aEvent->message) {
      case NS_FORM_SELECTED :
        if (::PopupAllowedForEvent("select"))
          abuse = openControlled;
        break;
      case NS_FORM_CHANGE :
        if (::PopupAllowedForEvent("change"))
          abuse = openControlled;
        break;
      }
    }
    break;
  case NS_GUI_EVENT :
    // For this following event only allow popups if it's triggered
    // while handling user input. See
    // nsPresShell::HandleEventInternal() for details.
    if (nsEventStateManager::IsHandlingUserInput()) {
      switch(aEvent->message) {
      case NS_FORM_INPUT :
        if (::PopupAllowedForEvent("input"))
          abuse = openControlled;
        break;
      }
    }
    break;
  case NS_INPUT_EVENT :
    // For this following event only allow popups if it's triggered
    // while handling user input. See
    // nsPresShell::HandleEventInternal() for details.
    if (nsEventStateManager::IsHandlingUserInput()) {
      switch(aEvent->message) {
      case NS_FORM_CHANGE :
        if (::PopupAllowedForEvent("change"))
          abuse = openControlled;
        break;
      case NS_XUL_COMMAND:
        abuse = openControlled;
        break;
      }
    }
    break;
  case NS_KEY_EVENT :
    if (aEvent->mFlags.mIsTrusted) {
      uint32_t key = aEvent->AsKeyboardEvent()->keyCode;
      switch(aEvent->message) {
      case NS_KEY_PRESS :
        // return key on focused button. see note at NS_MOUSE_CLICK.
        if (key == nsIDOMKeyEvent::DOM_VK_RETURN)
          abuse = openAllowed;
        else if (::PopupAllowedForEvent("keypress"))
          abuse = openControlled;
        break;
      case NS_KEY_UP :
        // space key on focused button. see note at NS_MOUSE_CLICK.
        if (key == nsIDOMKeyEvent::DOM_VK_SPACE)
          abuse = openAllowed;
        else if (::PopupAllowedForEvent("keyup"))
          abuse = openControlled;
        break;
      case NS_KEY_DOWN :
        if (::PopupAllowedForEvent("keydown"))
          abuse = openControlled;
        break;
      }
    }
    break;
  case NS_TOUCH_EVENT :
    if (aEvent->mFlags.mIsTrusted) {
      switch (aEvent->message) {
      case NS_TOUCH_START :
        if (PopupAllowedForEvent("touchstart")) {
          abuse = openControlled;
        }
        break;
      case NS_TOUCH_END :
        if (PopupAllowedForEvent("touchend")) {
          abuse = openControlled;
        }
        break;
      }
    }
    break;
  case NS_MOUSE_EVENT :
    if (aEvent->mFlags.mIsTrusted &&
        aEvent->AsMouseEvent()->button == WidgetMouseEvent::eLeftButton) {
      switch(aEvent->message) {
      case NS_MOUSE_BUTTON_UP :
        if (::PopupAllowedForEvent("mouseup"))
          abuse = openControlled;
        break;
      case NS_MOUSE_BUTTON_DOWN :
        if (::PopupAllowedForEvent("mousedown"))
          abuse = openControlled;
        break;
      case NS_MOUSE_CLICK :
        /* Click events get special treatment because of their
           historical status as a more legitimate event handler. If
           click popups are enabled in the prefs, clear the popup
           status completely. */
        if (::PopupAllowedForEvent("click"))
          abuse = openAllowed;
        break;
      case NS_MOUSE_DOUBLECLICK :
        if (::PopupAllowedForEvent("dblclick"))
          abuse = openControlled;
        break;
      }
    }
    break;
  case NS_SCRIPT_ERROR_EVENT :
    switch(aEvent->message) {
    case NS_LOAD_ERROR :
      // Any error event will allow popups, if enabled in the pref.
      if (::PopupAllowedForEvent("error"))
        abuse = openControlled;
      break;
    }
    break;
  case NS_FORM_EVENT :
    // For these following events only allow popups if they're
    // triggered while handling user input. See
    // nsPresShell::HandleEventInternal() for details.
    if (nsEventStateManager::IsHandlingUserInput()) {
      switch(aEvent->message) {
      case NS_FORM_SUBMIT :
        if (::PopupAllowedForEvent("submit"))
          abuse = openControlled;
        break;
      case NS_FORM_RESET :
        if (::PopupAllowedForEvent("reset"))
          abuse = openControlled;
        break;
      }
    }
    break;
  default:
    break;
  }

  return abuse;
}

// static
void
nsDOMEvent::PopupAllowedEventsChanged()
{
  if (sPopupAllowedEvents) {
    nsMemory::Free(sPopupAllowedEvents);
  }

  nsAdoptingCString str = Preferences::GetCString("dom.popup_allowed_events");

  // We'll want to do this even if str is empty to avoid looking up
  // this pref all the time if it's not set.
  sPopupAllowedEvents = ToNewCString(str);
}

// static
void
nsDOMEvent::Shutdown()
{
  if (sPopupAllowedEvents) {
    nsMemory::Free(sPopupAllowedEvents);
  }
}

nsIntPoint
nsDOMEvent::GetScreenCoords(nsPresContext* aPresContext,
                            WidgetEvent* aEvent,
                            LayoutDeviceIntPoint aPoint)
{
  if (nsEventStateManager::sIsPointerLocked) {
    return nsEventStateManager::sLastScreenPoint;
  }

  if (!aEvent || 
       (aEvent->eventStructType != NS_MOUSE_EVENT &&
        aEvent->eventStructType != NS_MOUSE_SCROLL_EVENT &&
        aEvent->eventStructType != NS_WHEEL_EVENT &&
        aEvent->eventStructType != NS_POINTER_EVENT &&
        aEvent->eventStructType != NS_TOUCH_EVENT &&
        aEvent->eventStructType != NS_DRAG_EVENT &&
        aEvent->eventStructType != NS_SIMPLE_GESTURE_EVENT)) {
    return nsIntPoint(0, 0);
  }

  WidgetGUIEvent* guiEvent = aEvent->AsGUIEvent();
  if (!guiEvent->widget) {
    return LayoutDeviceIntPoint::ToUntyped(aPoint);
  }

  LayoutDeviceIntPoint offset = aPoint +
    LayoutDeviceIntPoint::FromUntyped(guiEvent->widget->WidgetToScreenOffset());
  nscoord factor = aPresContext->DeviceContext()->UnscaledAppUnitsPerDevPixel();
  return nsIntPoint(nsPresContext::AppUnitsToIntCSSPixels(offset.x * factor),
                    nsPresContext::AppUnitsToIntCSSPixels(offset.y * factor));
}

//static
CSSIntPoint
nsDOMEvent::GetPageCoords(nsPresContext* aPresContext,
                          WidgetEvent* aEvent,
                          LayoutDeviceIntPoint aPoint,
                          CSSIntPoint aDefaultPoint)
{
  CSSIntPoint pagePoint = nsDOMEvent::GetClientCoords(aPresContext,
                                                      aEvent,
                                                      aPoint,
                                                      aDefaultPoint);

  // If there is some scrolling, add scroll info to client point.
  if (aPresContext && aPresContext->GetPresShell()) {
    nsIPresShell* shell = aPresContext->GetPresShell();
    nsIScrollableFrame* scrollframe = shell->GetRootScrollFrameAsScrollable();
    if (scrollframe) {
      pagePoint += CSSIntPoint::FromAppUnitsRounded(scrollframe->GetScrollPosition());
    }
  }

  return pagePoint;
}

// static
CSSIntPoint
nsDOMEvent::GetClientCoords(nsPresContext* aPresContext,
                            WidgetEvent* aEvent,
                            LayoutDeviceIntPoint aPoint,
                            CSSIntPoint aDefaultPoint)
{
  if (nsEventStateManager::sIsPointerLocked) {
    return nsEventStateManager::sLastClientPoint;
  }

  if (!aEvent ||
      (aEvent->eventStructType != NS_MOUSE_EVENT &&
       aEvent->eventStructType != NS_MOUSE_SCROLL_EVENT &&
       aEvent->eventStructType != NS_WHEEL_EVENT &&
       aEvent->eventStructType != NS_TOUCH_EVENT &&
       aEvent->eventStructType != NS_DRAG_EVENT &&
       aEvent->eventStructType != NS_POINTER_EVENT &&
       aEvent->eventStructType != NS_SIMPLE_GESTURE_EVENT) ||
      !aPresContext ||
      !aEvent->AsGUIEvent()->widget) {
    return aDefaultPoint;
  }

  nsIPresShell* shell = aPresContext->GetPresShell();
  if (!shell) {
    return CSSIntPoint(0, 0);
  }

  nsIFrame* rootFrame = shell->GetRootFrame();
  if (!rootFrame) {
    return CSSIntPoint(0, 0);
  }
  nsPoint pt =
    nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent,
      LayoutDeviceIntPoint::ToUntyped(aPoint), rootFrame);

  return CSSIntPoint::FromAppUnitsRounded(pt);
}

// To be called ONLY by nsDOMEvent::GetType (which has the additional
// logic for handling user-defined events).
// static
const char* nsDOMEvent::GetEventName(uint32_t aEventType)
{
  switch(aEventType) {
#define ID_TO_EVENT(name_, _id, _type, _struct) \
  case _id: return #name_;
#include "nsEventNameList.h"
#undef ID_TO_EVENT
  default:
    break;
  }
  // XXXldb We can hit this case for WidgetEvent objects that we didn't
  // create and that are not user defined events since this function and
  // SetEventType are incomplete.  (But fixing that requires fixing the
  // arrays in nsEventListenerManager too, since the events for which
  // this is a problem generally *are* created by nsDOMEvent.)
  return nullptr;
}

bool
nsDOMEvent::DefaultPrevented(JSContext* aCx) const
{
  MOZ_ASSERT(aCx, "JS context must be specified");

  NS_ENSURE_TRUE(mEvent, false);

  // If preventDefault() has never been called, just return false.
  if (!mEvent->mFlags.mDefaultPrevented) {
    return false;
  }

  // If preventDefault() has been called by content, return true.  Otherwise,
  // i.e., preventDefault() has been called by chrome, return true only when
  // this is called by chrome.
  return mEvent->mFlags.mDefaultPreventedByContent || IsChrome(aCx);
}

bool
nsDOMEvent::GetPreventDefault() const
{
  if (mOwner) {
    if (nsIDocument* doc = mOwner->GetExtantDoc()) {
      doc->WarnOnceAbout(nsIDocument::eGetPreventDefault);
    }
  }
  // GetPreventDefault() is legacy and Gecko specific method.  Although,
  // the result should be same as defaultPrevented, we don't need to break
  // backward compatibility of legacy method.  Let's behave traditionally.
  return DefaultPrevented();
}

NS_IMETHODIMP
nsDOMEvent::GetPreventDefault(bool* aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = GetPreventDefault();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMEvent::GetDefaultPrevented(bool* aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  // This method must be called by only event handlers implemented by C++.
  // Then, the handlers must handle default action.  So, this method don't need
  // to check if preventDefault() has been called by content or chrome.
  *aReturn = DefaultPrevented();
  return NS_OK;
}

NS_IMETHODIMP_(void)
nsDOMEvent::Serialize(IPC::Message* aMsg, bool aSerializeInterfaceType)
{
  if (aSerializeInterfaceType) {
    IPC::WriteParam(aMsg, NS_LITERAL_STRING("event"));
  }

  nsString type;
  GetType(type);
  IPC::WriteParam(aMsg, type);

  IPC::WriteParam(aMsg, Bubbles());
  IPC::WriteParam(aMsg, Cancelable());
  IPC::WriteParam(aMsg, IsTrusted());

  // No timestamp serialization for now!
}

NS_IMETHODIMP_(bool)
nsDOMEvent::Deserialize(const IPC::Message* aMsg, void** aIter)
{
  nsString type;
  NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &type), false);

  bool bubbles = false;
  NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &bubbles), false);

  bool cancelable = false;
  NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &cancelable), false);

  bool trusted = false;
  NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &trusted), false);

  nsresult rv = InitEvent(type, bubbles, cancelable);
  NS_ENSURE_SUCCESS(rv, false);
  SetTrusted(trusted);

  return true;
}

NS_IMETHODIMP_(void)
nsDOMEvent::SetOwner(mozilla::dom::EventTarget* aOwner)
{
  mOwner = nullptr;

  if (!aOwner) {
    return;
  }

  nsCOMPtr<nsINode> n = do_QueryInterface(aOwner);
  if (n) {
    mOwner = do_QueryInterface(n->OwnerDoc()->GetScopeObject());
    return;
  }

  nsCOMPtr<nsPIDOMWindow> w = do_QueryInterface(aOwner);
  if (w) {
    if (w->IsOuterWindow()) {
      mOwner = w->GetCurrentInnerWindow();
    } else {
      mOwner.swap(w);
    }
    return;
  }

  nsCOMPtr<nsDOMEventTargetHelper> eth = do_QueryInterface(aOwner);
  if (eth) {
    mOwner = eth->GetOwner();
    return;
  }

#ifdef DEBUG
  nsCOMPtr<nsPIWindowRoot> root = do_QueryInterface(aOwner);
  MOZ_ASSERT(root, "Unexpected EventTarget!");
#endif
}

nsresult NS_NewDOMEvent(nsIDOMEvent** aInstancePtrResult,
                        mozilla::dom::EventTarget* aOwner,
                        nsPresContext* aPresContext,
                        WidgetEvent* aEvent) 
{
  nsRefPtr<nsDOMEvent> it =
    new nsDOMEvent(aOwner, aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}

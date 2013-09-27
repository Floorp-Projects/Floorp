/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

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

static char *sPopupAllowedEvents;


nsDOMEvent::nsDOMEvent(mozilla::dom::EventTarget* aOwner,
                       nsPresContext* aPresContext, nsEvent* aEvent)
{
  ConstructorInit(aOwner, aPresContext, aEvent);
}

nsDOMEvent::nsDOMEvent(nsPIDOMWindow* aParent)
{
  ConstructorInit(static_cast<nsGlobalWindow *>(aParent), nullptr, nullptr);
}

void
nsDOMEvent::ConstructorInit(mozilla::dom::EventTarget* aOwner,
                            nsPresContext* aPresContext, nsEvent* aEvent)
{
  SetIsDOMBinding();
  SetOwner(aOwner);
  mIsMainThreadEvent = mOwner || NS_IsMainThread();

  mPrivateDataDuplicated = false;

  if (aEvent) {
    mEvent = aEvent;
    mEventIsInternal = false;
  }
  else {
    mEventIsInternal = true;
    /*
      A derived class might want to allocate its own type of aEvent
      (derived from nsEvent). To do this, it should take care to pass
      a non-nullptr aEvent to this ctor, e.g.:
      
        nsDOMFooEvent::nsDOMFooEvent(..., nsEvent* aEvent)
        : nsDOMEvent(..., aEvent ? aEvent : new nsFooEvent())
      
      Then, to override the mEventIsInternal assignments done by the
      base ctor, it should do this in its own ctor:

        nsDOMFooEvent::nsDOMFooEvent(..., nsEvent* aEvent)
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
    mEvent = new nsEvent(false, 0);
    mEvent->time = PR_Now();
  }

  InitPresContextData(aPresContext);
  nsJSContext::LikelyShortLivingObjectCreated();
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
        static_cast<nsMouseEvent_base*>(tmp->mEvent)->relatedTarget = nullptr;
        break;
      case NS_DRAG_EVENT:
        static_cast<nsDragEvent*>(tmp->mEvent)->dataTransfer = nullptr;
        static_cast<nsMouseEvent_base*>(tmp->mEvent)->relatedTarget = nullptr;
        break;
      case NS_CLIPBOARD_EVENT:
        static_cast<InternalClipboardEvent*>(tmp->mEvent)->clipboardData =
          nullptr;
        break;
      case NS_MUTATION_EVENT:
        static_cast<InternalMutationEvent*>(tmp->mEvent)->mRelatedNode =
          nullptr;
        break;
      case NS_FOCUS_EVENT:
        static_cast<InternalFocusEvent*>(tmp->mEvent)->relatedTarget = nullptr;
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
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mEvent->relatedTarget");
        cb.NoteXPCOMChild(
          static_cast<nsMouseEvent_base*>(tmp->mEvent)->relatedTarget);
        break;
      case NS_DRAG_EVENT:
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mEvent->dataTransfer");
        cb.NoteXPCOMChild(
          static_cast<nsDragEvent*>(tmp->mEvent)->dataTransfer);
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mEvent->relatedTarget");
        cb.NoteXPCOMChild(
          static_cast<nsMouseEvent_base*>(tmp->mEvent)->relatedTarget);
        break;
      case NS_CLIPBOARD_EVENT:
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mEvent->clipboardData");
        cb.NoteXPCOMChild(
          static_cast<InternalClipboardEvent*>(tmp->mEvent)->clipboardData);
        break;
      case NS_MUTATION_EVENT:
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mEvent->mRelatedNode");
        cb.NoteXPCOMChild(
          static_cast<InternalMutationEvent*>(tmp->mEvent)->mRelatedNode);
        break;
      case NS_FOCUS_EVENT:
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mEvent->relatedTarget");
        cb.NoteXPCOMChild(
          static_cast<InternalFocusEvent*>(tmp->mEvent)->relatedTarget);
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
  if (mEvent->mFlags.mCancelable) {
    mEvent->mFlags.mDefaultPrevented = true;

    // Need to set an extra flag for drag events.
    if (mEvent->eventStructType == NS_DRAG_EVENT && IsTrusted()) {
      nsCOMPtr<nsINode> node = do_QueryInterface(mEvent->currentTarget);
      if (!node) {
        nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(mEvent->currentTarget);
        if (win) {
          node = win->GetExtantDoc();
        }
      }
      if (node && !nsContentUtils::IsChromeDoc(node->OwnerDoc())) {
        mEvent->mFlags.mDefaultPreventedByContent = true;
      }
    }
  }

  return NS_OK;
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
  
  NS_ASSERTION(mEvent, "No nsEvent for nsDOMEvent duplication!");
  if (mEventIsInternal) {
    return NS_OK;
  }

  nsEvent* newEvent = nullptr;
  uint32_t msg = mEvent->message;

  switch (mEvent->eventStructType) {
    case NS_EVENT:
    {
      newEvent = new nsEvent(false, msg);
      newEvent->AssignEventData(*mEvent, true);
      break;
    }
    case NS_GUI_EVENT:
    {
      nsGUIEvent* oldGUIEvent = static_cast<nsGUIEvent*>(mEvent);
      // Not copying widget, it is a weak reference.
      nsGUIEvent* guiEvent = new nsGUIEvent(false, msg, nullptr);
      guiEvent->AssignGUIEventData(*oldGUIEvent, true);
      newEvent = guiEvent;
      break;
    }
    case NS_INPUT_EVENT:
    {
      nsInputEvent* oldInputEvent = static_cast<nsInputEvent*>(mEvent);
      nsInputEvent* inputEvent = new nsInputEvent(false, msg, nullptr);
      inputEvent->AssignInputEventData(*oldInputEvent, true);
      newEvent = inputEvent;
      break;
    }
    case NS_KEY_EVENT:
    {
      nsKeyEvent* oldKeyEvent = static_cast<nsKeyEvent*>(mEvent);
      nsKeyEvent* keyEvent = new nsKeyEvent(false, msg, nullptr);
      keyEvent->AssignKeyEventData(*oldKeyEvent, true);
      newEvent = keyEvent;
      break;
    }
    case NS_MOUSE_EVENT:
    {
      nsMouseEvent* oldMouseEvent = static_cast<nsMouseEvent*>(mEvent);
      nsMouseEvent* mouseEvent =
        new nsMouseEvent(false, msg, nullptr, oldMouseEvent->reason);
      mouseEvent->AssignMouseEventData(*oldMouseEvent, true);
      newEvent = mouseEvent;
      break;
    }
    case NS_DRAG_EVENT:
    {
      nsDragEvent* oldDragEvent = static_cast<nsDragEvent*>(mEvent);
      nsDragEvent* dragEvent =
        new nsDragEvent(false, msg, nullptr);
      dragEvent->AssignDragEventData(*oldDragEvent, true);
      newEvent = dragEvent;
      break;
    }
    case NS_CLIPBOARD_EVENT:
    {
      InternalClipboardEvent* oldClipboardEvent =
        static_cast<InternalClipboardEvent*>(mEvent);
      InternalClipboardEvent* clipboardEvent =
        new InternalClipboardEvent(false, msg);
      clipboardEvent->AssignClipboardEventData(*oldClipboardEvent, true);
      newEvent = clipboardEvent;
      break;
    }
    case NS_SCRIPT_ERROR_EVENT:
    {
      nsScriptErrorEvent* oldScriptErrorEvent =
        static_cast<nsScriptErrorEvent*>(mEvent);
      nsScriptErrorEvent* scriptErrorEvent = new nsScriptErrorEvent(false, msg);
      scriptErrorEvent->AssignScriptErrorEventData(*oldScriptErrorEvent, true);
      newEvent = scriptErrorEvent;
      break;
    }
    case NS_TEXT_EVENT:
    {
      nsTextEvent* oldTextEvent = static_cast<nsTextEvent*>(mEvent);
      nsTextEvent* textEvent = new nsTextEvent(false, msg, nullptr);
      textEvent->AssignTextEventData(*oldTextEvent, true);
      newEvent = textEvent;
      break;
    }
    case NS_COMPOSITION_EVENT:
    {
      nsCompositionEvent* compositionEvent =
        new nsCompositionEvent(false, msg, nullptr);
      nsCompositionEvent* oldCompositionEvent =
        static_cast<nsCompositionEvent*>(mEvent);
      compositionEvent->AssignCompositionEventData(*oldCompositionEvent, true);
      newEvent = compositionEvent;
      break;
    }
    case NS_MOUSE_SCROLL_EVENT:
    {
      nsMouseScrollEvent* oldMouseScrollEvent =
        static_cast<nsMouseScrollEvent*>(mEvent);
      nsMouseScrollEvent* mouseScrollEvent =
        new nsMouseScrollEvent(false, msg, nullptr);
      mouseScrollEvent->AssignMouseScrollEventData(*oldMouseScrollEvent, true);
      newEvent = mouseScrollEvent;
      break;
    }
    case NS_WHEEL_EVENT:
    {
      WheelEvent* oldWheelEvent = static_cast<WheelEvent*>(mEvent);
      WheelEvent* wheelEvent = new WheelEvent(false, msg, nullptr);
      wheelEvent->AssignWheelEventData(*oldWheelEvent, true);
      newEvent = wheelEvent;
      break;
    }
    case NS_SCROLLPORT_EVENT:
    {
      nsScrollPortEvent* oldScrollPortEvent =
        static_cast<nsScrollPortEvent*>(mEvent);
      nsScrollPortEvent* scrollPortEvent =
        new nsScrollPortEvent(false, msg, nullptr);
      scrollPortEvent->AssignScrollPortEventData(*oldScrollPortEvent, true);
      newEvent = scrollPortEvent;
      break;
    }
    case NS_SCROLLAREA_EVENT:
    {
      nsScrollAreaEvent* oldScrollAreaEvent =
        static_cast<nsScrollAreaEvent*>(mEvent);
      nsScrollAreaEvent* scrollAreaEvent = 
        new nsScrollAreaEvent(false, msg, nullptr);
      scrollAreaEvent->AssignScrollAreaEventData(*oldScrollAreaEvent, true);
      newEvent = scrollAreaEvent;
      break;
    }
    case NS_MUTATION_EVENT:
    {
      InternalMutationEvent* mutationEvent =
        new InternalMutationEvent(false, msg);
      InternalMutationEvent* oldMutationEvent =
        static_cast<InternalMutationEvent*>(mEvent);
      mutationEvent->AssignMutationEventData(*oldMutationEvent, true);
      newEvent = mutationEvent;
      break;
    }
    case NS_FORM_EVENT:
    {
      nsFormEvent* oldFormEvent = static_cast<nsFormEvent*>(mEvent);
      nsFormEvent* formEvent = new nsFormEvent(false, msg);
      formEvent->AssignFormEventData(*oldFormEvent, true);
      newEvent = formEvent;
      break;
    }
    case NS_FOCUS_EVENT:
    {
      InternalFocusEvent* newFocusEvent = new InternalFocusEvent(false, msg);
      InternalFocusEvent* oldFocusEvent =
        static_cast<InternalFocusEvent*>(mEvent);
      newFocusEvent->AssignFocusEventData(*oldFocusEvent, true);
      newEvent = newFocusEvent;
      break;
    }
    case NS_COMMAND_EVENT:
    {
      WidgetCommandEvent* oldCommandEvent =
        static_cast<WidgetCommandEvent*>(mEvent);
      WidgetCommandEvent* commandEvent =
        new WidgetCommandEvent(false, mEvent->userType,
                               oldCommandEvent->command, nullptr);
      commandEvent->AssignCommandEventData(*oldCommandEvent, true);
      newEvent = commandEvent;
      break;
    }
    case NS_UI_EVENT:
    {
      nsUIEvent* oldUIEvent = static_cast<nsUIEvent*>(mEvent);
      nsUIEvent* uiEvent = new nsUIEvent(false, msg, oldUIEvent->detail);
      uiEvent->AssignUIEventData(*oldUIEvent, true);
      newEvent = uiEvent;
      break;
    }
    case NS_SVGZOOM_EVENT:
    {
      nsGUIEvent* oldGUIEvent = static_cast<nsGUIEvent*>(mEvent);
      nsGUIEvent* guiEvent = new nsGUIEvent(false, msg, nullptr);
      guiEvent->eventStructType = NS_SVGZOOM_EVENT;
      guiEvent->AssignGUIEventData(*oldGUIEvent, true);
      newEvent = guiEvent;
      break;
    }
    case NS_SMIL_TIME_EVENT:
    {
      nsUIEvent* oldUIEvent = static_cast<nsUIEvent*>(mEvent);
      nsUIEvent* uiEvent = new nsUIEvent(false, msg, 0);
      uiEvent->eventStructType = NS_SMIL_TIME_EVENT;
      uiEvent->AssignUIEventData(*oldUIEvent, true);
      newEvent = uiEvent;
      break;
    }
    case NS_SIMPLE_GESTURE_EVENT:
    {
      nsSimpleGestureEvent* oldSimpleGestureEvent =
        static_cast<nsSimpleGestureEvent*>(mEvent);
      nsSimpleGestureEvent* simpleGestureEvent = 
        new nsSimpleGestureEvent(false, msg, nullptr, 0, 0.0);
      simpleGestureEvent->
        AssignSimpleGestureEventData(*oldSimpleGestureEvent, true);
      newEvent = simpleGestureEvent;
      break;
    }
    case NS_TRANSITION_EVENT:
    {
      InternalTransitionEvent* oldTransitionEvent =
        static_cast<InternalTransitionEvent*>(mEvent);
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
      InternalAnimationEvent* oldAnimationEvent =
        static_cast<InternalAnimationEvent*>(mEvent);
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
      nsTouchEvent* oldTouchEvent = static_cast<nsTouchEvent*>(mEvent);
      nsTouchEvent* touchEvent = new nsTouchEvent(false, oldTouchEvent);
      touchEvent->AssignTouchEventData(*oldTouchEvent, true);
      newEvent = touchEvent;
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

NS_IMETHODIMP_(nsEvent*)
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
nsDOMEvent::GetEventPopupControlState(nsEvent *aEvent)
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
      uint32_t key = static_cast<nsKeyEvent *>(aEvent)->keyCode;
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
        static_cast<nsMouseEvent*>(aEvent)->button == nsMouseEvent::eLeftButton) {
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
                            nsEvent* aEvent,
                            LayoutDeviceIntPoint aPoint)
{
  if (nsEventStateManager::sIsPointerLocked) {
    return nsEventStateManager::sLastScreenPoint;
  }

  if (!aEvent || 
       (aEvent->eventStructType != NS_MOUSE_EVENT &&
        aEvent->eventStructType != NS_MOUSE_SCROLL_EVENT &&
        aEvent->eventStructType != NS_WHEEL_EVENT &&
        aEvent->eventStructType != NS_TOUCH_EVENT &&
        aEvent->eventStructType != NS_DRAG_EVENT &&
        aEvent->eventStructType != NS_SIMPLE_GESTURE_EVENT)) {
    return nsIntPoint(0, 0);
  }

  nsGUIEvent* guiEvent = static_cast<nsGUIEvent*>(aEvent);
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
                          nsEvent* aEvent,
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
                            nsEvent* aEvent,
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
       aEvent->eventStructType != NS_SIMPLE_GESTURE_EVENT) ||
      !aPresContext ||
      !static_cast<nsGUIEvent*>(aEvent)->widget) {
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
  // XXXldb We can hit this case for nsEvent objects that we didn't
  // create and that are not user defined events since this function and
  // SetEventType are incomplete.  (But fixing that requires fixing the
  // arrays in nsEventListenerManager too, since the events for which
  // this is a problem generally *are* created by nsDOMEvent.)
  return nullptr;
}

bool
nsDOMEvent::GetPreventDefault() const
{
  if (mOwner) {
    if (nsIDocument* doc = mOwner->GetExtantDoc()) {
      doc->WarnOnceAbout(nsIDocument::eGetPreventDefault);
    }
  }
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
                        nsEvent *aEvent) 
{
  nsRefPtr<nsDOMEvent> it =
    new nsDOMEvent(aOwner, aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}

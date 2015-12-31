/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessCheck.h"
#include "base/basictypes.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/InternalMutationEvent.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"
#include "nsContentUtils.h"
#include "nsCOMPtr.h"
#include "nsDeviceContext.h"
#include "nsError.h"
#include "nsGlobalWindow.h"
#include "nsIFrame.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIScrollableFrame.h"
#include "nsJSEnvironment.h"
#include "nsLayoutUtils.h"
#include "nsPerformance.h"
#include "nsPIWindowRoot.h"
#include "WorkerPrivate.h"

namespace mozilla {
namespace dom {

namespace workers {
extern bool IsCurrentThreadRunningChromeWorker();
} // namespace workers

static char *sPopupAllowedEvents;

static bool sReturnHighResTimeStamp = false;
static bool sReturnHighResTimeStampIsSet = false;

Event::Event(EventTarget* aOwner,
             nsPresContext* aPresContext,
             WidgetEvent* aEvent)
{
  ConstructorInit(aOwner, aPresContext, aEvent);
}

Event::Event(nsPIDOMWindow* aParent)
{
  ConstructorInit(static_cast<nsGlobalWindow *>(aParent), nullptr, nullptr);
}

void
Event::ConstructorInit(EventTarget* aOwner,
                       nsPresContext* aPresContext,
                       WidgetEvent* aEvent)
{
  SetOwner(aOwner);
  mIsMainThreadEvent = NS_IsMainThread();

  if (mIsMainThreadEvent && !sReturnHighResTimeStampIsSet) {
    Preferences::AddBoolVarCache(&sReturnHighResTimeStamp,
                                 "dom.event.highrestimestamp.enabled",
                                 sReturnHighResTimeStamp);
    sReturnHighResTimeStampIsSet = true;
  }

  mPrivateDataDuplicated = false;
  mWantsPopupControlCheck = false;

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
      
        FooEvent::FooEvent(..., WidgetEvent* aEvent)
          : Event(..., aEvent ? aEvent : new WidgetEvent())
      
      Then, to override the mEventIsInternal assignments done by the
      base ctor, it should do this in its own ctor:

        FooEvent::FooEvent(..., WidgetEvent* aEvent)
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
    mEvent = new WidgetEvent(false, eVoidEvent);
    mEvent->time = PR_Now();
  }

  InitPresContextData(aPresContext);
}

void
Event::InitPresContextData(nsPresContext* aPresContext)
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

Event::~Event() 
{
  NS_ASSERT_OWNINGTHREAD(Event);

  if (mEventIsInternal && mEvent) {
    delete mEvent;
  }
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Event)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEvent)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Event)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Event)

NS_IMPL_CYCLE_COLLECTION_CLASS(Event)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(Event)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Event)
  if (tmp->mEventIsInternal) {
    tmp->mEvent->target = nullptr;
    tmp->mEvent->currentTarget = nullptr;
    tmp->mEvent->originalTarget = nullptr;
    switch (tmp->mEvent->mClass) {
      case eMouseEventClass:
      case eMouseScrollEventClass:
      case eWheelEventClass:
      case eSimpleGestureEventClass:
      case ePointerEventClass:
        tmp->mEvent->AsMouseEventBase()->relatedTarget = nullptr;
        break;
      case eDragEventClass: {
        WidgetDragEvent* dragEvent = tmp->mEvent->AsDragEvent();
        dragEvent->dataTransfer = nullptr;
        dragEvent->relatedTarget = nullptr;
        break;
      }
      case eClipboardEventClass:
        tmp->mEvent->AsClipboardEvent()->clipboardData = nullptr;
        break;
      case eMutationEventClass:
        tmp->mEvent->AsMutationEvent()->mRelatedNode = nullptr;
        break;
      case eFocusEventClass:
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

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Event)
  if (tmp->mEventIsInternal) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEvent->target)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEvent->currentTarget)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEvent->originalTarget)
    switch (tmp->mEvent->mClass) {
      case eMouseEventClass:
      case eMouseScrollEventClass:
      case eWheelEventClass:
      case eSimpleGestureEventClass:
      case ePointerEventClass:
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mEvent->relatedTarget");
        cb.NoteXPCOMChild(tmp->mEvent->AsMouseEventBase()->relatedTarget);
        break;
      case eDragEventClass: {
        WidgetDragEvent* dragEvent = tmp->mEvent->AsDragEvent();
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mEvent->dataTransfer");
        cb.NoteXPCOMChild(dragEvent->dataTransfer);
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mEvent->relatedTarget");
        cb.NoteXPCOMChild(dragEvent->relatedTarget);
        break;
      }
      case eClipboardEventClass:
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mEvent->clipboardData");
        cb.NoteXPCOMChild(tmp->mEvent->AsClipboardEvent()->clipboardData);
        break;
      case eMutationEventClass:
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mEvent->mRelatedNode");
        cb.NoteXPCOMChild(tmp->mEvent->AsMutationEvent()->mRelatedNode);
        break;
      case eFocusEventClass:
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


JSObject*
Event::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return WrapObjectInternal(aCx, aGivenProto);
}

JSObject*
Event::WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return EventBinding::Wrap(aCx, this, aGivenProto);
}

bool
Event::IsChrome(JSContext* aCx) const
{
  return mIsMainThreadEvent ?
    xpc::AccessCheck::isChrome(js::GetContextCompartment(aCx)) :
    mozilla::dom::workers::IsCurrentThreadRunningChromeWorker();
}

// nsIDOMEventInterface
NS_METHOD
Event::GetType(nsAString& aType)
{
  if (!mIsMainThreadEvent || !mEvent->typeString.IsEmpty()) {
    aType = mEvent->typeString;
    return NS_OK;
  }
  const char* name = GetEventName(mEvent->mMessage);

  if (name) {
    CopyASCIItoUTF16(name, aType);
    return NS_OK;
  } else if (mEvent->mMessage == eUnidentifiedEvent && mEvent->userType) {
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
Event::GetTarget() const
{
  return GetDOMEventTarget(mEvent->target);
}

NS_METHOD
Event::GetTarget(nsIDOMEventTarget** aTarget)
{
  NS_IF_ADDREF(*aTarget = GetTarget());
  return NS_OK;
}

EventTarget*
Event::GetCurrentTarget() const
{
  return GetDOMEventTarget(mEvent->currentTarget);
}

NS_IMETHODIMP
Event::GetCurrentTarget(nsIDOMEventTarget** aCurrentTarget)
{
  NS_IF_ADDREF(*aCurrentTarget = GetCurrentTarget());
  return NS_OK;
}

//
// Get the actual event target node (may have been retargeted for mouse events)
//
already_AddRefed<nsIContent>
Event::GetTargetFromFrame()
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
Event::GetExplicitOriginalTarget() const
{
  if (mExplicitOriginalTarget) {
    return mExplicitOriginalTarget;
  }
  return GetTarget();
}

NS_IMETHODIMP
Event::GetExplicitOriginalTarget(nsIDOMEventTarget** aRealEventTarget)
{
  NS_IF_ADDREF(*aRealEventTarget = GetExplicitOriginalTarget());
  return NS_OK;
}

EventTarget*
Event::GetOriginalTarget() const
{
  if (mEvent->originalTarget) {
    return GetDOMEventTarget(mEvent->originalTarget);
  }

  return GetTarget();
}

NS_IMETHODIMP
Event::GetOriginalTarget(nsIDOMEventTarget** aOriginalTarget)
{
  NS_IF_ADDREF(*aOriginalTarget = GetOriginalTarget());
  return NS_OK;
}

EventTarget*
Event::GetComposedTarget() const
{
  EventTarget* et = GetOriginalTarget();
  nsCOMPtr<nsIContent> content = do_QueryInterface(et);
  if (!content) {
    return et;
  }
  nsIContent* nonChrome = content->FindFirstNonChromeOnlyAccessContent();
  return nonChrome ?
    static_cast<EventTarget*>(nonChrome) :
    static_cast<EventTarget*>(content->GetComposedDoc());
}

NS_IMETHODIMP_(void)
Event::SetTrusted(bool aTrusted)
{
  mEvent->mFlags.mIsTrusted = aTrusted;
}

bool
Event::Init(mozilla::dom::EventTarget* aGlobal)
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

// static
already_AddRefed<Event>
Event::Constructor(const GlobalObject& aGlobal,
                   const nsAString& aType,
                   const EventInit& aParam,
                   ErrorResult& aRv)
{
  nsCOMPtr<mozilla::dom::EventTarget> t = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<Event> e = new Event(t, nullptr, nullptr);
  bool trusted = e->Init(t);
  e->InitEvent(aType, aParam.mBubbles, aParam.mCancelable);
  e->SetTrusted(trusted);
  return e.forget();
}

uint16_t
Event::EventPhase() const
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
Event::GetEventPhase(uint16_t* aEventPhase)
{
  *aEventPhase = EventPhase();
  return NS_OK;
}

NS_IMETHODIMP
Event::GetBubbles(bool* aBubbles)
{
  *aBubbles = Bubbles();
  return NS_OK;
}

NS_IMETHODIMP
Event::GetCancelable(bool* aCancelable)
{
  *aCancelable = Cancelable();
  return NS_OK;
}

NS_IMETHODIMP
Event::GetTimeStamp(uint64_t* aTimeStamp)
{
  *aTimeStamp = mEvent->time;
  return NS_OK;
}

NS_IMETHODIMP
Event::StopPropagation()
{
  mEvent->mFlags.mPropagationStopped = true;
  return NS_OK;
}

NS_IMETHODIMP
Event::StopImmediatePropagation()
{
  mEvent->mFlags.mPropagationStopped = true;
  mEvent->mFlags.mImmediatePropagationStopped = true;
  return NS_OK;
}

NS_IMETHODIMP
Event::StopCrossProcessForwarding()
{
  mEvent->mFlags.mNoCrossProcessBoundaryForwarding = true;
  return NS_OK;
}

NS_IMETHODIMP
Event::GetIsTrusted(bool* aIsTrusted)
{
  *aIsTrusted = IsTrusted();
  return NS_OK;
}

NS_IMETHODIMP
Event::PreventDefault()
{
  // This method is called only from C++ code which must handle default action
  // of this event.  So, pass true always.
  PreventDefaultInternal(true);
  return NS_OK;
}

void
Event::PreventDefault(JSContext* aCx)
{
  MOZ_ASSERT(aCx, "JS context must be specified");

  // Note that at handling default action, another event may be dispatched.
  // Then, JS in content mey be call preventDefault()
  // even in the event is in system event group.  Therefore, don't refer
  // mInSystemGroup here.
  PreventDefaultInternal(IsChrome(aCx));
}

void
Event::PreventDefaultInternal(bool aCalledByDefaultHandler)
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
  } else {
    mEvent->mFlags.mDefaultPreventedByChrome = true;
  }

  if (!IsTrusted()) {
    return;
  }

  WidgetDragEvent* dragEvent = mEvent->AsDragEvent();
  if (!dragEvent) {
    return;
  }

  nsCOMPtr<nsINode> node = do_QueryInterface(mEvent->currentTarget);
  if (!node) {
    nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(mEvent->currentTarget);
    if (!win) {
      return;
    }
    node = win->GetExtantDoc();
  }
  if (!nsContentUtils::IsChromeDoc(node->OwnerDoc())) {
    dragEvent->mDefaultPreventedOnContent = true;
  }
}

void
Event::SetEventType(const nsAString& aEventTypeArg)
{
  if (mIsMainThreadEvent) {
    mEvent->typeString.Truncate();
    mEvent->userType =
      nsContentUtils::GetEventMessageAndAtom(aEventTypeArg, mEvent->mClass,
                                             &(mEvent->mMessage));
  } else {
    mEvent->userType = nullptr;
    mEvent->mMessage = eUnidentifiedEvent;
    mEvent->typeString = aEventTypeArg;
  }
}

void
Event::InitEvent(const nsAString& aEventTypeArg,
                 bool aCanBubbleArg,
                 bool aCancelableArg)
{
  // Make sure this event isn't already being dispatched.
  NS_ENSURE_TRUE_VOID(!mEvent->mFlags.mIsBeingDispatched);

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
  mEvent->mFlags.mDefaultPreventedByContent = false;
  mEvent->mFlags.mDefaultPreventedByChrome = false;

  // Clearing the old targets, so that the event is targeted correctly when
  // re-dispatching it.
  mEvent->target = nullptr;
  mEvent->originalTarget = nullptr;
}

NS_IMETHODIMP
Event::DuplicatePrivateData()
{
  NS_ASSERTION(mEvent, "No WidgetEvent for Event duplication!");
  if (mEventIsInternal) {
    return NS_OK;
  }

  mEvent = mEvent->Duplicate();
  mPresContext = nullptr;
  mEventIsInternal = true;
  mPrivateDataDuplicated = true;

  return NS_OK;
}

NS_IMETHODIMP
Event::SetTarget(nsIDOMEventTarget* aTarget)
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
Event::IsDispatchStopped()
{
  return mEvent->mFlags.mPropagationStopped;
}

NS_IMETHODIMP_(WidgetEvent*)
Event::GetInternalNSEvent()
{
  return mEvent;
}

NS_IMETHODIMP_(Event*)
Event::InternalDOMEvent()
{
  return this;
}

// return true if eventName is contained within events, delimited by
// spaces
static bool
PopupAllowedForEvent(const char *eventName)
{
  if (!sPopupAllowedEvents) {
    Event::PopupAllowedEventsChanged();

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
Event::GetEventPopupControlState(WidgetEvent* aEvent, nsIDOMEvent* aDOMEvent)
{
  // generally if an event handler is running, new windows are disallowed.
  // check for exceptions:
  PopupControlState abuse = openAbused;

  if (aDOMEvent && aDOMEvent->InternalDOMEvent()->GetWantsPopupControlCheck()) {
    nsAutoString type;
    aDOMEvent->GetType(type);
    if (PopupAllowedForEvent(NS_ConvertUTF16toUTF8(type).get())) {
      return openAllowed;
    }
  }

  switch(aEvent->mClass) {
  case eBasicEventClass:
    // For these following events only allow popups if they're
    // triggered while handling user input. See
    // nsPresShell::HandleEventInternal() for details.
    if (EventStateManager::IsHandlingUserInput()) {
      switch(aEvent->mMessage) {
      case eFormSelect:
        if (PopupAllowedForEvent("select")) {
          abuse = openControlled;
        }
        break;
      case eFormChange:
        if (PopupAllowedForEvent("change")) {
          abuse = openControlled;
        }
        break;
      default:
        break;
      }
    }
    break;
  case eEditorInputEventClass:
    // For this following event only allow popups if it's triggered
    // while handling user input. See
    // nsPresShell::HandleEventInternal() for details.
    if (EventStateManager::IsHandlingUserInput()) {
      switch(aEvent->mMessage) {
      case eEditorInput:
        if (PopupAllowedForEvent("input")) {
          abuse = openControlled;
        }
        break;
      default:
        break;
      }
    }
    break;
  case eInputEventClass:
    // For this following event only allow popups if it's triggered
    // while handling user input. See
    // nsPresShell::HandleEventInternal() for details.
    if (EventStateManager::IsHandlingUserInput()) {
      switch(aEvent->mMessage) {
      case eFormChange:
        if (PopupAllowedForEvent("change")) {
          abuse = openControlled;
        }
        break;
      case eXULCommand:
        abuse = openControlled;
        break;
      default:
        break;
      }
    }
    break;
  case eKeyboardEventClass:
    if (aEvent->mFlags.mIsTrusted) {
      uint32_t key = aEvent->AsKeyboardEvent()->keyCode;
      switch(aEvent->mMessage) {
      case eKeyPress:
        // return key on focused button. see note at eMouseClick.
        if (key == nsIDOMKeyEvent::DOM_VK_RETURN) {
          abuse = openAllowed;
        } else if (PopupAllowedForEvent("keypress")) {
          abuse = openControlled;
        }
        break;
      case eKeyUp:
        // space key on focused button. see note at eMouseClick.
        if (key == nsIDOMKeyEvent::DOM_VK_SPACE) {
          abuse = openAllowed;
        } else if (PopupAllowedForEvent("keyup")) {
          abuse = openControlled;
        }
        break;
      case eKeyDown:
        if (PopupAllowedForEvent("keydown")) {
          abuse = openControlled;
        }
        break;
      default:
        break;
      }
    }
    break;
  case eTouchEventClass:
    if (aEvent->mFlags.mIsTrusted) {
      switch (aEvent->mMessage) {
      case eTouchStart:
        if (PopupAllowedForEvent("touchstart")) {
          abuse = openControlled;
        }
        break;
      case eTouchEnd:
        if (PopupAllowedForEvent("touchend")) {
          abuse = openControlled;
        }
        break;
      default:
        break;
      }
    }
    break;
  case eMouseEventClass:
    if (aEvent->mFlags.mIsTrusted &&
        aEvent->AsMouseEvent()->button == WidgetMouseEvent::eLeftButton) {
      switch(aEvent->mMessage) {
      case eMouseUp:
        if (PopupAllowedForEvent("mouseup")) {
          abuse = openControlled;
        }
        break;
      case eMouseDown:
        if (PopupAllowedForEvent("mousedown")) {
          abuse = openControlled;
        }
        break;
      case eMouseClick:
        /* Click events get special treatment because of their
           historical status as a more legitimate event handler. If
           click popups are enabled in the prefs, clear the popup
           status completely. */
        if (PopupAllowedForEvent("click")) {
          abuse = openAllowed;
        }
        break;
      case eMouseDoubleClick:
        if (PopupAllowedForEvent("dblclick")) {
          abuse = openControlled;
        }
        break;
      default:
        break;
      }
    }
    break;
  case eFormEventClass:
    // For these following events only allow popups if they're
    // triggered while handling user input. See
    // nsPresShell::HandleEventInternal() for details.
    if (EventStateManager::IsHandlingUserInput()) {
      switch(aEvent->mMessage) {
      case eFormSubmit:
        if (PopupAllowedForEvent("submit")) {
          abuse = openControlled;
        }
        break;
      case eFormReset:
        if (PopupAllowedForEvent("reset")) {
          abuse = openControlled;
        }
        break;
      default:
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
Event::PopupAllowedEventsChanged()
{
  if (sPopupAllowedEvents) {
    free(sPopupAllowedEvents);
  }

  nsAdoptingCString str = Preferences::GetCString("dom.popup_allowed_events");

  // We'll want to do this even if str is empty to avoid looking up
  // this pref all the time if it's not set.
  sPopupAllowedEvents = ToNewCString(str);
}

// static
void
Event::Shutdown()
{
  if (sPopupAllowedEvents) {
    free(sPopupAllowedEvents);
  }
}

// static
CSSIntPoint
Event::GetScreenCoords(nsPresContext* aPresContext,
                       WidgetEvent* aEvent,
                       LayoutDeviceIntPoint aPoint)
{
  if (!nsContentUtils::LegacyIsCallerChromeOrNativeCode() &&
      nsContentUtils::ResistFingerprinting()) {
    // When resisting fingerprinting, return client coordinates instead.
    return GetClientCoords(aPresContext, aEvent, aPoint, CSSIntPoint(0, 0));
  }

  if (EventStateManager::sIsPointerLocked) {
    return EventStateManager::sLastScreenPoint;
  }

  if (!aEvent ||
       (aEvent->mClass != eMouseEventClass &&
        aEvent->mClass != eMouseScrollEventClass &&
        aEvent->mClass != eWheelEventClass &&
        aEvent->mClass != ePointerEventClass &&
        aEvent->mClass != eTouchEventClass &&
        aEvent->mClass != eDragEventClass &&
        aEvent->mClass != eSimpleGestureEventClass)) {
    return CSSIntPoint(0, 0);
  }

  // Doing a straight conversion from LayoutDeviceIntPoint to CSSIntPoint
  // seem incorrect, but it is needed to maintain legacy functionality.
  WidgetGUIEvent* guiEvent = aEvent->AsGUIEvent();
  if (!aPresContext || !(guiEvent && guiEvent->widget)) {
    return CSSIntPoint(aPoint.x, aPoint.y);
  }

  nsPoint pt =
    LayoutDevicePixel::ToAppUnits(aPoint, aPresContext->DeviceContext()->AppUnitsPerDevPixelAtUnitFullZoom());

  if (aPresContext->PresShell()) {
    pt = pt.RemoveResolution(aPresContext->PresShell()->GetCumulativeNonRootScaleResolution());
  }

  pt += LayoutDevicePixel::ToAppUnits(guiEvent->widget->WidgetToScreenOffset(),
                                      aPresContext->DeviceContext()->AppUnitsPerDevPixelAtUnitFullZoom());

  return CSSPixel::FromAppUnitsRounded(pt);
}

// static
CSSIntPoint
Event::GetPageCoords(nsPresContext* aPresContext,
                     WidgetEvent* aEvent,
                     LayoutDeviceIntPoint aPoint,
                     CSSIntPoint aDefaultPoint)
{
  CSSIntPoint pagePoint =
    Event::GetClientCoords(aPresContext, aEvent, aPoint, aDefaultPoint);

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
Event::GetClientCoords(nsPresContext* aPresContext,
                       WidgetEvent* aEvent,
                       LayoutDeviceIntPoint aPoint,
                       CSSIntPoint aDefaultPoint)
{
  if (EventStateManager::sIsPointerLocked) {
    return EventStateManager::sLastClientPoint;
  }

  if (!aEvent ||
      (aEvent->mClass != eMouseEventClass &&
       aEvent->mClass != eMouseScrollEventClass &&
       aEvent->mClass != eWheelEventClass &&
       aEvent->mClass != eTouchEventClass &&
       aEvent->mClass != eDragEventClass &&
       aEvent->mClass != ePointerEventClass &&
       aEvent->mClass != eSimpleGestureEventClass) ||
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
    nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, aPoint, rootFrame);

  return CSSIntPoint::FromAppUnitsRounded(pt);
}

// static
CSSIntPoint
Event::GetOffsetCoords(nsPresContext* aPresContext,
                       WidgetEvent* aEvent,
                       LayoutDeviceIntPoint aPoint,
                       CSSIntPoint aDefaultPoint)
{
  if (!aEvent->target) {
    return GetPageCoords(aPresContext, aEvent, aPoint, aDefaultPoint);
  }
  nsCOMPtr<nsIContent> content = do_QueryInterface(aEvent->target);
  if (!content || !aPresContext) {
    return CSSIntPoint(0, 0);
  }
  nsCOMPtr<nsIPresShell> shell = aPresContext->GetPresShell();
  if (!shell) {
    return CSSIntPoint(0, 0);
  }
  shell->FlushPendingNotifications(Flush_Layout);
  nsIFrame* frame = content->GetPrimaryFrame();
  if (!frame) {
    return CSSIntPoint(0, 0);
  }
  nsIFrame* rootFrame = shell->GetRootFrame();
  if (!rootFrame) {
    return CSSIntPoint(0, 0);
  }
  CSSIntPoint clientCoords =
    GetClientCoords(aPresContext, aEvent, aPoint, aDefaultPoint);
  nsPoint pt = CSSPixel::ToAppUnits(clientCoords);
  if (nsLayoutUtils::TransformPoint(rootFrame, frame, pt) ==
      nsLayoutUtils::TRANSFORM_SUCCEEDED) {
    pt -= frame->GetPaddingRectRelativeToSelf().TopLeft();
    return CSSPixel::FromAppUnitsRounded(pt);
  }
  return CSSIntPoint(0, 0);
}

// To be called ONLY by Event::GetType (which has the additional
// logic for handling user-defined events).
// static
const char*
Event::GetEventName(EventMessage aEventType)
{
  switch(aEventType) {
#define MESSAGE_TO_EVENT(name_, _message, _type, _struct) \
  case _message: return #name_;
#include "mozilla/EventNameList.h"
#undef MESSAGE_TO_EVENT
  default:
    break;
  }
  // XXXldb We can hit this case for WidgetEvent objects that we didn't
  // create and that are not user defined events since this function and
  // SetEventType are incomplete.  (But fixing that requires fixing the
  // arrays in nsEventListenerManager too, since the events for which
  // this is a problem generally *are* created by Event.)
  return nullptr;
}

bool
Event::DefaultPrevented(JSContext* aCx) const
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

double
Event::TimeStamp() const
{
  if (!sReturnHighResTimeStamp) {
    return static_cast<double>(mEvent->time);
  }

  if (mEvent->timeStamp.IsNull()) {
    return 0.0;
  }

  if (mIsMainThreadEvent) {
    if (NS_WARN_IF(!mOwner)) {
      return 0.0;
    }

    nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(mOwner);
    if (NS_WARN_IF(!win)) {
      return 0.0;
    }
    nsPerformance* perf = win->GetPerformance();
    if (NS_WARN_IF(!perf)) {
      return 0.0;
    }

    return perf->GetDOMTiming()->TimeStampToDOMHighRes(mEvent->timeStamp);
  }

  // For dedicated workers, we should make times relative to the navigation
  // start of the document that created the worker, which is the same as the
  // timebase for performance.now().
  workers::WorkerPrivate* workerPrivate =
    workers::GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);

  TimeDuration duration =
    mEvent->timeStamp - workerPrivate->CreationTimeStamp();
  return duration.ToMilliseconds();
}

bool
Event::GetPreventDefault() const
{
  nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(mOwner));
  if (win) {
    if (nsIDocument* doc = win->GetExtantDoc()) {
      doc->WarnOnceAbout(nsIDocument::eGetPreventDefault);
    }
  }
  // GetPreventDefault() is legacy and Gecko specific method.  Although,
  // the result should be same as defaultPrevented, we don't need to break
  // backward compatibility of legacy method.  Let's behave traditionally.
  return DefaultPrevented();
}

NS_IMETHODIMP
Event::GetPreventDefault(bool* aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = GetPreventDefault();
  return NS_OK;
}

NS_IMETHODIMP
Event::GetDefaultPrevented(bool* aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  // This method must be called by only event handlers implemented by C++.
  // Then, the handlers must handle default action.  So, this method don't need
  // to check if preventDefault() has been called by content or chrome.
  *aReturn = DefaultPrevented();
  return NS_OK;
}

NS_IMETHODIMP_(void)
Event::Serialize(IPC::Message* aMsg, bool aSerializeInterfaceType)
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
Event::Deserialize(const IPC::Message* aMsg, void** aIter)
{
  nsString type;
  NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &type), false);

  bool bubbles = false;
  NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &bubbles), false);

  bool cancelable = false;
  NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &cancelable), false);

  bool trusted = false;
  NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &trusted), false);

  InitEvent(type, bubbles, cancelable);
  SetTrusted(trusted);

  return true;
}

NS_IMETHODIMP_(void)
Event::SetOwner(mozilla::dom::EventTarget* aOwner)
{
  mOwner = nullptr;

  if (!aOwner) {
    return;
  }

  nsCOMPtr<nsINode> n = do_QueryInterface(aOwner);
  if (n) {
    mOwner = n->OwnerDoc()->GetScopeObject();
    return;
  }

  nsCOMPtr<nsPIDOMWindow> w = do_QueryInterface(aOwner);
  if (w) {
    if (w->IsOuterWindow()) {
      mOwner = do_QueryInterface(w->GetCurrentInnerWindow());
    } else {
      mOwner = do_QueryInterface(w);
    }
    return;
  }

  nsCOMPtr<DOMEventTargetHelper> eth = do_QueryInterface(aOwner);
  if (eth) {
    mOwner = eth->GetParentObject();
    return;
  }

#ifdef DEBUG
  nsCOMPtr<nsPIWindowRoot> root = do_QueryInterface(aOwner);
  MOZ_ASSERT(root, "Unexpected EventTarget!");
#endif
}

// static
nsIContent*
Event::GetShadowRelatedTarget(nsIContent* aCurrentTarget,
                              nsIContent* aRelatedTarget)
{
  if (!aCurrentTarget || !aRelatedTarget) {
    return nullptr;
  }

  // Walk up the ancestor node trees of the related target until
  // we encounter the node tree of the current target in order
  // to find the adjusted related target. Walking up the tree may
  // not find a common ancestor node tree if the related target is in
  // an ancestor tree, but in that case it does not need to be adjusted.
  ShadowRoot* currentTargetShadow = aCurrentTarget->GetContainingShadow();
  if (!currentTargetShadow) {
    return nullptr;
  }

  nsIContent* relatedTarget = aCurrentTarget;
  while (relatedTarget) {
    ShadowRoot* ancestorShadow = relatedTarget->GetContainingShadow();
    if (currentTargetShadow == ancestorShadow) {
      return relatedTarget;
    }

    // Didn't find the ancestor tree, thus related target does not have to
    // adjusted.
    if (!ancestorShadow) {
      return nullptr;
    }

    relatedTarget = ancestorShadow->GetHost();
  }

  return nullptr;
}

} // namespace dom
} // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<Event>
NS_NewDOMEvent(EventTarget* aOwner,
               nsPresContext* aPresContext,
               WidgetEvent* aEvent) 
{
  RefPtr<Event> it = new Event(aOwner, aPresContext, aEvent);
  return it.forget();
}

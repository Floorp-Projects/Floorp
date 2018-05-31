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
#include "mozilla/EventDispatcher.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/InternalMutationEvent.h"
#include "mozilla/dom/Performance.h"
#include "mozilla/dom/WorkerPrivate.h"
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
#include "nsIContentInlines.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIScrollableFrame.h"
#include "nsJSEnvironment.h"
#include "nsLayoutUtils.h"
#include "nsPIWindowRoot.h"
#include "nsRFPService.h"

namespace mozilla {
namespace dom {

static char *sPopupAllowedEvents;

static bool sReturnHighResTimeStamp = false;
static bool sReturnHighResTimeStampIsSet = false;

Event::Event(EventTarget* aOwner,
             nsPresContext* aPresContext,
             WidgetEvent* aEvent)
{
  ConstructorInit(aOwner, aPresContext, aEvent);
}

Event::Event(nsPIDOMWindowInner* aParent)
{
  ConstructorInit(nsGlobalWindowInner::Cast(aParent), nullptr, nullptr);
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
    mEvent->mTime = PR_Now();
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
  NS_INTERFACE_MAP_ENTRY(Event)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Event)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Event)

NS_IMPL_CYCLE_COLLECTION_CLASS(Event)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(Event)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Event)
  if (tmp->mEventIsInternal) {
    tmp->mEvent->mTarget = nullptr;
    tmp->mEvent->mCurrentTarget = nullptr;
    tmp->mEvent->mOriginalTarget = nullptr;
    tmp->mEvent->mRelatedTarget = nullptr;
    tmp->mEvent->mOriginalRelatedTarget = nullptr;
    switch (tmp->mEvent->mClass) {
      case eDragEventClass: {
        WidgetDragEvent* dragEvent = tmp->mEvent->AsDragEvent();
        dragEvent->mDataTransfer = nullptr;
        break;
      }
      case eClipboardEventClass:
        tmp->mEvent->AsClipboardEvent()->mClipboardData = nullptr;
        break;
      case eMutationEventClass:
        tmp->mEvent->AsMutationEvent()->mRelatedNode = nullptr;
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
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEvent->mTarget)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEvent->mCurrentTarget)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEvent->mOriginalTarget)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEvent->mRelatedTarget)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEvent->mOriginalRelatedTarget);
    switch (tmp->mEvent->mClass) {
      case eDragEventClass: {
        WidgetDragEvent* dragEvent = tmp->mEvent->AsDragEvent();
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mEvent->mDataTransfer");
        cb.NoteXPCOMChild(dragEvent->mDataTransfer);
        break;
      }
      case eClipboardEventClass:
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mEvent->mClipboardData");
        cb.NoteXPCOMChild(tmp->mEvent->AsClipboardEvent()->mClipboardData);
        break;
      case eMutationEventClass:
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mEvent->mRelatedNode");
        cb.NoteXPCOMChild(tmp->mEvent->AsMutationEvent()->mRelatedNode);
        break;
      default:
        break;
    }
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPresContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mExplicitOriginalTarget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwner)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END


JSObject*
Event::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return WrapObjectInternal(aCx, aGivenProto);
}

JSObject*
Event::WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return Event_Binding::Wrap(aCx, this, aGivenProto);
}

void
Event::GetType(nsAString& aType) const
{
  if (!mIsMainThreadEvent) {
    aType = mEvent->mSpecifiedEventTypeString;
    return;
  }
  GetWidgetEventType(mEvent, aType);
}

EventTarget*
Event::GetTarget() const
{
  return mEvent->GetDOMEventTarget();
}

EventTarget*
Event::GetCurrentTarget() const
{
  return mEvent->GetCurrentDOMEventTarget();
}

void
Event::ComposedPath(nsTArray<RefPtr<EventTarget>>& aPath)
{
  EventDispatcher::GetComposedPathFor(mEvent, aPath);
}

//
// Get the actual event target node (may have been retargeted for mouse events)
//
already_AddRefed<nsIContent>
Event::GetTargetFromFrame()
{
  if (!mPresContext) { return nullptr; }

  // Get the mTarget frame (have to get the ESM first)
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

EventTarget*
Event::GetOriginalTarget() const
{
  return mEvent->GetOriginalDOMEventTarget();
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

void
Event::SetTrusted(bool aTrusted)
{
  mEvent->mFlags.mIsTrusted = aTrusted;
}

bool
Event::Init(mozilla::dom::EventTarget* aGlobal)
{
  if (!mIsMainThreadEvent) {
    return IsCurrentThreadRunningChromeWorker();
  }
  bool trusted = false;
  nsCOMPtr<nsPIDOMWindowInner> w = do_QueryInterface(aGlobal);
  if (w) {
    nsCOMPtr<nsIDocument> d = w->GetExtantDoc();
    if (d) {
      trusted = nsContentUtils::IsChromeDoc(d);
      nsPresContext* presContext = d->GetPresContext();
      if (presContext) {
        InitPresContextData(presContext);
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
  return Constructor(t, aType, aParam);
}

// static
already_AddRefed<Event>
Event::Constructor(EventTarget* aEventTarget,
                   const nsAString& aType,
                   const EventInit& aParam)
{
  RefPtr<Event> e = new Event(aEventTarget, nullptr, nullptr);
  bool trusted = e->Init(aEventTarget);
  e->InitEvent(aType, aParam.mBubbles, aParam.mCancelable);
  e->SetTrusted(trusted);
  e->SetComposed(aParam.mComposed);
  return e.forget();
}

uint16_t
Event::EventPhase() const
{
  // Note, remember to check that this works also
  // if or when Bug 235441 is fixed.
  if ((mEvent->mCurrentTarget &&
       mEvent->mCurrentTarget == mEvent->mTarget) ||
       mEvent->mFlags.InTargetPhase()) {
    return Event_Binding::AT_TARGET;
  }
  if (mEvent->mFlags.mInCapturePhase) {
    return Event_Binding::CAPTURING_PHASE;
  }
  if (mEvent->mFlags.mInBubblingPhase) {
    return Event_Binding::BUBBLING_PHASE;
  }
  return Event_Binding::NONE;
}

void
Event::StopPropagation()
{
  mEvent->StopPropagation();
}

void
Event::StopImmediatePropagation()
{
  mEvent->StopImmediatePropagation();
}

void
Event::StopCrossProcessForwarding()
{
  mEvent->StopCrossProcessForwarding();
}

void
Event::PreventDefault()
{
  // This method is called only from C++ code which must handle default action
  // of this event.  So, pass true always.
  PreventDefaultInternal(true);
}

void
Event::PreventDefault(JSContext* aCx, CallerType aCallerType)
{
  // Note that at handling default action, another event may be dispatched.
  // Then, JS in content mey be call preventDefault()
  // even in the event is in system event group.  Therefore, don't refer
  // mInSystemGroup here.
  nsIPrincipal* principal = mIsMainThreadEvent ?
                              nsContentUtils::SubjectPrincipal(aCx) : nullptr;

  PreventDefaultInternal(aCallerType == CallerType::System, principal);
}

void
Event::PreventDefaultInternal(bool aCalledByDefaultHandler,
                              nsIPrincipal* aPrincipal)
{
  if (!mEvent->mFlags.mCancelable) {
    return;
  }
  if (mEvent->mFlags.mInPassiveListener) {
    nsCOMPtr<nsPIDOMWindowInner> win(do_QueryInterface(mOwner));
    if (win) {
      if (nsIDocument* doc = win->GetExtantDoc()) {
        nsString type;
        GetType(type);
        const char16_t* params[] = { type.get() };
        doc->WarnOnceAbout(nsIDocument::ePreventDefaultFromPassiveListener,
          false, params, ArrayLength(params));
      }
    }
    return;
  }

  mEvent->PreventDefault(aCalledByDefaultHandler, aPrincipal);

  if (!IsTrusted()) {
    return;
  }

  WidgetDragEvent* dragEvent = mEvent->AsDragEvent();
  if (!dragEvent) {
    return;
  }

  nsCOMPtr<nsINode> node = do_QueryInterface(mEvent->mCurrentTarget);
  if (!node) {
    nsCOMPtr<nsPIDOMWindowOuter> win =
      do_QueryInterface(mEvent->mCurrentTarget);
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
    mEvent->mSpecifiedEventTypeString.Truncate();
    mEvent->mSpecifiedEventType =
      nsContentUtils::GetEventMessageAndAtom(aEventTypeArg, mEvent->mClass,
                                             &(mEvent->mMessage));
    mEvent->SetDefaultComposed();
  } else {
    mEvent->mSpecifiedEventType = nullptr;
    mEvent->mMessage = eUnidentifiedEvent;
    mEvent->mSpecifiedEventTypeString = aEventTypeArg;
    mEvent->SetComposed(aEventTypeArg);
  }
  mEvent->SetDefaultComposedInNativeAnonymousContent();
}

already_AddRefed<EventTarget>
Event::EnsureWebAccessibleRelatedTarget(EventTarget* aRelatedTarget)
{
  nsCOMPtr<EventTarget> relatedTarget = aRelatedTarget;
  if (relatedTarget) {
    nsCOMPtr<nsIContent> content = do_QueryInterface(relatedTarget);

    if (content && content->ChromeOnlyAccess() &&
        !nsContentUtils::CanAccessNativeAnon()) {
      content = content->FindFirstNonChromeOnlyAccessContent();
      relatedTarget = do_QueryInterface(content);
    }

    if (relatedTarget) {
      relatedTarget = relatedTarget->GetTargetForDOMEvent();
    }
  }
  return relatedTarget.forget();
}

void
Event::InitEvent(const nsAString& aEventTypeArg,
                 mozilla::CanBubble aCanBubbleArg,
                 mozilla::Cancelable aCancelableArg)
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

  mEvent->mFlags.mBubbles = aCanBubbleArg == CanBubble::eYes;
  mEvent->mFlags.mCancelable = aCancelableArg == Cancelable::eYes;

  mEvent->mFlags.mDefaultPrevented = false;
  mEvent->mFlags.mDefaultPreventedByContent = false;
  mEvent->mFlags.mDefaultPreventedByChrome = false;
  mEvent->mFlags.mPropagationStopped = false;
  mEvent->mFlags.mImmediatePropagationStopped = false;

  // Clearing the old targets, so that the event is targeted correctly when
  // re-dispatching it.
  mEvent->mTarget = nullptr;
  mEvent->mOriginalTarget = nullptr;
}

void
Event::DuplicatePrivateData()
{
  NS_ASSERTION(mEvent, "No WidgetEvent for Event duplication!");
  if (mEventIsInternal) {
    return;
  }

  mEvent = mEvent->Duplicate();
  mPresContext = nullptr;
  mEventIsInternal = true;
  mPrivateDataDuplicated = true;
}

void
Event::SetTarget(EventTarget* aTarget)
{
  mEvent->mTarget = aTarget;
}

bool
Event::IsDispatchStopped()
{
  return mEvent->PropagationStopped();
}

WidgetEvent*
Event::WidgetEventPtr()
{
  return mEvent;
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

  nsCString::const_iterator start, end;
  nsCString::const_iterator startiter(events.BeginReading(start));
  events.EndReading(end);

  while (startiter != end) {
    nsCString::const_iterator enditer(end);

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
Event::GetEventPopupControlState(WidgetEvent* aEvent, Event* aDOMEvent)
{
  // generally if an event handler is running, new windows are disallowed.
  // check for exceptions:
  PopupControlState abuse = openAbused;

  if (aDOMEvent && aDOMEvent->GetWantsPopupControlCheck()) {
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
      abuse = openBlocked;
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
      abuse = openBlocked;
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
      abuse = openBlocked;
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
    if (aEvent->IsTrusted()) {
      abuse = openBlocked;
      uint32_t key = aEvent->AsKeyboardEvent()->mKeyCode;
      switch(aEvent->mMessage) {
      case eKeyPress:
        // return key on focused button. see note at eMouseClick.
        if (key == NS_VK_RETURN) {
          abuse = openAllowed;
        } else if (PopupAllowedForEvent("keypress")) {
          abuse = openControlled;
        }
        break;
      case eKeyUp:
        // space key on focused button. see note at eMouseClick.
        if (key == NS_VK_SPACE) {
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
    if (aEvent->IsTrusted()) {
      abuse = openBlocked;
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
    if (aEvent->IsTrusted() &&
        aEvent->AsMouseEvent()->button == WidgetMouseEvent::eLeftButton) {
      abuse = openBlocked;
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
  case ePointerEventClass:
    if (aEvent->IsTrusted() &&
        aEvent->AsPointerEvent()->button == WidgetMouseEvent::eLeftButton) {
      switch(aEvent->mMessage) {
      case ePointerUp:
        if (PopupAllowedForEvent("pointerup")) {
          abuse = openControlled;
        }
        break;
      case ePointerDown:
        if (PopupAllowedForEvent("pointerdown")) {
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
      abuse = openBlocked;
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

  nsAutoCString str;
  Preferences::GetCString("dom.popup_allowed_events", str);

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
  if (!aPresContext || !(guiEvent && guiEvent->mWidget)) {
    return CSSIntPoint(aPoint.x, aPoint.y);
  }

  nsPoint pt =
    LayoutDevicePixel::ToAppUnits(aPoint, aPresContext->DeviceContext()->AppUnitsPerDevPixelAtUnitFullZoom());

  if (nsIPresShell* ps = aPresContext->GetPresShell()) {
    pt = pt.RemoveResolution(nsLayoutUtils::GetCurrentAPZResolutionScale(ps));
  }

  pt += LayoutDevicePixel::ToAppUnits(guiEvent->mWidget->WidgetToScreenOffset(),
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
      !aEvent->AsGUIEvent()->mWidget) {
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
  if (!aEvent->mTarget) {
    return GetPageCoords(aPresContext, aEvent, aPoint, aDefaultPoint);
  }
  nsCOMPtr<nsIContent> content = do_QueryInterface(aEvent->mTarget);
  if (!content || !aPresContext) {
    return CSSIntPoint(0, 0);
  }
  nsCOMPtr<nsIPresShell> shell = aPresContext->GetPresShell();
  if (!shell) {
    return CSSIntPoint(0, 0);
  }
  shell->FlushPendingNotifications(FlushType::Layout);
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
Event::DefaultPrevented(CallerType aCallerType) const
{
  NS_ENSURE_TRUE(mEvent, false);

  // If preventDefault() has never been called, just return false.
  if (!mEvent->DefaultPrevented()) {
    return false;
  }

  // If preventDefault() has been called by content, return true.  Otherwise,
  // i.e., preventDefault() has been called by chrome, return true only when
  // this is called by chrome.
  return mEvent->DefaultPreventedByContent() ||
         aCallerType == CallerType::System;
}

double
Event::TimeStamp()
{
  if (!sReturnHighResTimeStamp) {
    // In the situation where you have set a very old, not-very-supported
    // non-default preference, we will always reduce the precision,
    // regardless of system principal or not.
    // The timestamp is absolute, so we supply a zero context mix-in.
    double ret = static_cast<double>(mEvent->mTime);
    return nsRFPService::ReduceTimePrecisionAsMSecs(ret, 0);
  }

  if (mEvent->mTimeStamp.IsNull()) {
    return 0.0;
  }

  if (mIsMainThreadEvent) {
    if (NS_WARN_IF(!mOwner)) {
      return 0.0;
    }

    nsCOMPtr<nsPIDOMWindowInner> win = do_QueryInterface(mOwner);
    if (NS_WARN_IF(!win)) {
      return 0.0;
    }

    Performance* perf = win->GetPerformance();
    if (NS_WARN_IF(!perf)) {
      return 0.0;
    }

    double ret = perf->GetDOMTiming()->TimeStampToDOMHighRes(mEvent->mTimeStamp);
    MOZ_ASSERT(mOwner->PrincipalOrNull());
    if (nsContentUtils::IsSystemPrincipal(mOwner->PrincipalOrNull()))
      return ret;

    return nsRFPService::ReduceTimePrecisionAsMSecs(ret,
      perf->GetRandomTimelineSeed());
  }

  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);

  double ret = workerPrivate->TimeStampToDOMHighRes(mEvent->mTimeStamp);
  if (workerPrivate->UsesSystemPrincipal())
    return ret;

  return nsRFPService::ReduceTimePrecisionAsMSecs(ret,
    workerPrivate->GetRandomTimelineSeed());
}

void
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
  IPC::WriteParam(aMsg, Composed());

  // No timestamp serialization for now!
}

bool
Event::Deserialize(const IPC::Message* aMsg, PickleIterator* aIter)
{
  nsString type;
  NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &type), false);

  bool bubbles = false;
  NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &bubbles), false);

  bool cancelable = false;
  NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &cancelable), false);

  bool trusted = false;
  NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &trusted), false);

  bool composed = false;
  NS_ENSURE_TRUE(IPC::ReadParam(aMsg, aIter, &composed), false);

  InitEvent(type, bubbles, cancelable);
  SetTrusted(trusted);
  SetComposed(composed);

  return true;
}

void
Event::SetOwner(EventTarget* aOwner)
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

  nsCOMPtr<nsPIDOMWindowInner> w = do_QueryInterface(aOwner);
  if (w) {
    mOwner = do_QueryInterface(w);
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

void
Event::GetWidgetEventType(WidgetEvent* aEvent, nsAString& aType)
{
  if (!aEvent->mSpecifiedEventTypeString.IsEmpty()) {
    aType = aEvent->mSpecifiedEventTypeString;
    return;
  }

  const char* name = GetEventName(aEvent->mMessage);

  if (name) {
    CopyASCIItoUTF16(name, aType);
    return;
  } else if (aEvent->mMessage == eUnidentifiedEvent &&
             aEvent->mSpecifiedEventType) {
    // Remove "on"
    aType = Substring(nsDependentAtomString(aEvent->mSpecifiedEventType), 2);
    aEvent->mSpecifiedEventTypeString = aType;
    return;
  }

  aType.Truncate();
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

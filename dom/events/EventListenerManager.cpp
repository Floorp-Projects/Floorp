/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Microsoft's API Name hackery sucks
#undef CreateEvent

#include "mozilla/AddonPathService.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventListenerManager.h"
#ifdef MOZ_B2G
#include "mozilla/Hal.h"
#endif // #ifdef MOZ_B2G
#include "mozilla/HalSensor.h"
#include "mozilla/InternalMutationEvent.h"
#include "mozilla/JSEventHandler.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/TimelineConsumers.h"
#include "mozilla/EventTimelineMarker.h"

#include "EventListenerService.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsDocShell.h"
#include "nsDOMCID.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsHtml5Atoms.h"
#include "nsIContent.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIDocument.h"
#include "nsIDOMEventListener.h"
#include "nsIScriptGlobalObject.h"
#include "nsISupports.h"
#include "nsIXPConnect.h"
#include "nsJSUtils.h"
#include "nsNameSpaceManager.h"
#include "nsPIDOMWindow.h"
#include "nsSandboxFlags.h"
#include "xpcpublic.h"

namespace mozilla {

using namespace dom;
using namespace hal;

#define EVENT_TYPE_EQUALS(ls, message, userType, typeString, allEvents) \
  ((ls->mEventMessage == message &&                                     \
    (ls->mEventMessage != NS_USER_DEFINED_EVENT ||                      \
    (mIsMainThreadELM && ls->mTypeAtom == userType) ||                  \
    (!mIsMainThreadELM && ls->mTypeString.Equals(typeString)))) ||      \
   (allEvents && ls->mAllEvents))

static const uint32_t kAllMutationBits =
  NS_EVENT_BITS_MUTATION_SUBTREEMODIFIED |
  NS_EVENT_BITS_MUTATION_NODEINSERTED |
  NS_EVENT_BITS_MUTATION_NODEREMOVED |
  NS_EVENT_BITS_MUTATION_NODEREMOVEDFROMDOCUMENT |
  NS_EVENT_BITS_MUTATION_NODEINSERTEDINTODOCUMENT |
  NS_EVENT_BITS_MUTATION_ATTRMODIFIED |
  NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED;

static uint32_t
MutationBitForEventType(EventMessage aEventType)
{
  switch (aEventType) {
    case eLegacySubtreeModified:
      return NS_EVENT_BITS_MUTATION_SUBTREEMODIFIED;
    case eLegacyNodeInserted:
      return NS_EVENT_BITS_MUTATION_NODEINSERTED;
    case eLegacyNodeRemoved:
      return NS_EVENT_BITS_MUTATION_NODEREMOVED;
    case eLegacyNodeRemovedFromDocument:
      return NS_EVENT_BITS_MUTATION_NODEREMOVEDFROMDOCUMENT;
    case eLegacyNodeInsertedIntoDocument:
      return NS_EVENT_BITS_MUTATION_NODEINSERTEDINTODOCUMENT;
    case eLegacyAttrModified:
      return NS_EVENT_BITS_MUTATION_ATTRMODIFIED;
    case eLegacyCharacterDataModified:
      return NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED;
    default:
      break;
  }
  return 0;
}

uint32_t EventListenerManager::sMainThreadCreatedCount = 0;

EventListenerManagerBase::EventListenerManagerBase()
  : mNoListenerForEvent(eVoidEvent)
  , mMayHavePaintEventListener(false)
  , mMayHaveMutationListeners(false)
  , mMayHaveCapturingListeners(false)
  , mMayHaveSystemGroupListeners(false)
  , mMayHaveTouchEventListener(false)
  , mMayHaveMouseEnterLeaveEventListener(false)
  , mMayHavePointerEnterLeaveEventListener(false)
  , mMayHaveKeyEventListener(false)
  , mMayHaveInputOrCompositionEventListener(false)
  , mClearingListeners(false)
  , mIsMainThreadELM(NS_IsMainThread())
{
  static_assert(sizeof(EventListenerManagerBase) == sizeof(uint32_t),
                "Keep the size of EventListenerManagerBase size compact!");
}

EventListenerManager::EventListenerManager(EventTarget* aTarget)
  : EventListenerManagerBase()
  , mTarget(aTarget)
{
  NS_ASSERTION(aTarget, "unexpected null pointer");

  if (mIsMainThreadELM) {
    ++sMainThreadCreatedCount;
  }
}

EventListenerManager::~EventListenerManager()
{
  // If your code fails this assertion, a possible reason is that
  // a class did not call our Disconnect() manually. Note that
  // this class can have Disconnect called in one of two ways:
  // if it is part of a cycle, then in Unlink() (such a cycle
  // would be with one of the listeners, not mTarget which is weak).
  // If not part of a cycle, then Disconnect must be called manually,
  // typically from the destructor of the owner class (mTarget).
  // XXX azakai: Is there any reason to not just call Disconnect
  //             from right here, if not previously called?
  NS_ASSERTION(!mTarget, "didn't call Disconnect");
  RemoveAllListeners();
}

void
EventListenerManager::RemoveAllListeners()
{
  if (mClearingListeners) {
    return;
  }
  mClearingListeners = true;
  mListeners.Clear();
  mClearingListeners = false;
}

void
EventListenerManager::Shutdown()
{
  Event::Shutdown();
}

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(EventListenerManager, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(EventListenerManager, Release)

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            EventListenerManager::Listener& aField,
                            const char* aName,
                            unsigned aFlags)
{
  if (MOZ_UNLIKELY(aCallback.WantDebugInfo())) {
    nsAutoCString name;
    name.AppendASCII(aName);
    if (aField.mTypeAtom) {
      name.AppendASCII(" event=");
      name.Append(nsAtomCString(aField.mTypeAtom));
      name.AppendASCII(" listenerType=");
      name.AppendInt(aField.mListenerType);
      name.AppendASCII(" ");
    }
    CycleCollectionNoteChild(aCallback, aField.mListener.GetISupports(), name.get(),
                             aFlags);
  } else {
    CycleCollectionNoteChild(aCallback, aField.mListener.GetISupports(), aName,
                             aFlags);
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(EventListenerManager)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(EventListenerManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mListeners)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(EventListenerManager)
  tmp->Disconnect();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END


nsPIDOMWindow*
EventListenerManager::GetInnerWindowForTarget()
{
  nsCOMPtr<nsINode> node = do_QueryInterface(mTarget);
  if (node) {
    // XXX sXBL/XBL2 issue -- do we really want the owner here?  What
    // if that's the XBL document?
    return node->OwnerDoc()->GetInnerWindow();
  }

  nsCOMPtr<nsPIDOMWindow> window = GetTargetAsInnerWindow();
  return window;
}

already_AddRefed<nsPIDOMWindow>
EventListenerManager::GetTargetAsInnerWindow() const
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(mTarget);
  if (!window) {
    return nullptr;
  }

  NS_ASSERTION(window->IsInnerWindow(), "Target should not be an outer window");
  return window.forget();
}

void
EventListenerManager::AddEventListenerInternal(
                        const EventListenerHolder& aListenerHolder,
                        EventMessage aEventMessage,
                        nsIAtom* aTypeAtom,
                        const nsAString& aTypeString,
                        const EventListenerFlags& aFlags,
                        bool aHandler,
                        bool aAllEvents)
{
  MOZ_ASSERT(// Main thread
             (NS_IsMainThread() && aEventMessage && aTypeAtom) ||
             // non-main-thread
             (!NS_IsMainThread() && aEventMessage && !aTypeString.IsEmpty()) ||
             aAllEvents, "Missing type"); // all-events listener

  if (!aListenerHolder || mClearingListeners) {
    return;
  }

  // Since there is no public API to call us with an EventListenerHolder, we
  // know that there's an EventListenerHolder on the stack holding a strong ref
  // to the listener.

  Listener* listener;
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; i++) {
    listener = &mListeners.ElementAt(i);
    // mListener == aListenerHolder is the last one, since it can be a bit slow.
    if (listener->mListenerIsHandler == aHandler &&
        listener->mFlags == aFlags &&
        EVENT_TYPE_EQUALS(listener, aEventMessage, aTypeAtom, aTypeString,
                          aAllEvents) &&
        listener->mListener == aListenerHolder) {
      return;
    }
  }

  mNoListenerForEvent = eVoidEvent;
  mNoListenerForEventAtom = nullptr;

  listener = aAllEvents ? mListeners.InsertElementAt(0) :
                          mListeners.AppendElement();
  listener->mListener = aListenerHolder;
  listener->mEventMessage = aEventMessage;
  listener->mTypeString = aTypeString;
  listener->mTypeAtom = aTypeAtom;
  listener->mFlags = aFlags;
  listener->mListenerIsHandler = aHandler;
  listener->mHandlerIsString = false;
  listener->mAllEvents = aAllEvents;

  // Detect the type of event listener.
  nsCOMPtr<nsIXPConnectWrappedJS> wjs;
  if (aFlags.mListenerIsJSListener) {
    MOZ_ASSERT(!aListenerHolder.HasWebIDLCallback());
    listener->mListenerType = Listener::eJSEventListener;
  } else if (aListenerHolder.HasWebIDLCallback()) {
    listener->mListenerType = Listener::eWebIDLListener;
  } else if ((wjs = do_QueryInterface(aListenerHolder.GetXPCOMCallback()))) {
    listener->mListenerType = Listener::eWrappedJSListener;
  } else {
    listener->mListenerType = Listener::eNativeListener;
  }


  if (aFlags.mInSystemGroup) {
    mMayHaveSystemGroupListeners = true;
  }
  if (aFlags.mCapture) {
    mMayHaveCapturingListeners = true;
  }

  if (aEventMessage == NS_AFTERPAINT) {
    mMayHavePaintEventListener = true;
    nsPIDOMWindow* window = GetInnerWindowForTarget();
    if (window) {
      window->SetHasPaintEventListeners();
    }
  } else if (aEventMessage >= eLegacyMutationEventFirst &&
             aEventMessage <= eLegacyMutationEventLast) {
    // For mutation listeners, we need to update the global bit on the DOM window.
    // Otherwise we won't actually fire the mutation event.
    mMayHaveMutationListeners = true;
    // Go from our target to the nearest enclosing DOM window.
    nsPIDOMWindow* window = GetInnerWindowForTarget();
    if (window) {
      nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
      if (doc) {
        doc->WarnOnceAbout(nsIDocument::eMutationEvent);
      }
      // If aEventMessage is eLegacySubtreeModified, we need to listen all
      // mutations. nsContentUtils::HasMutationListeners relies on this.
      window->SetMutationListeners(
        (aEventMessage == eLegacySubtreeModified) ?
          kAllMutationBits : MutationBitForEventType(aEventMessage));
    }
  } else if (aTypeAtom == nsGkAtoms::ondeviceorientation) {
    EnableDevice(NS_DEVICE_ORIENTATION);
  } else if (aTypeAtom == nsGkAtoms::ondeviceproximity || aTypeAtom == nsGkAtoms::onuserproximity) {
    EnableDevice(NS_DEVICE_PROXIMITY);
  } else if (aTypeAtom == nsGkAtoms::ondevicelight) {
    EnableDevice(NS_DEVICE_LIGHT);
  } else if (aTypeAtom == nsGkAtoms::ondevicemotion) {
    EnableDevice(NS_DEVICE_MOTION);
#ifdef MOZ_B2G
  } else if (aTypeAtom == nsGkAtoms::onmoztimechange) {
    nsCOMPtr<nsPIDOMWindow> window = GetTargetAsInnerWindow();
    if (window) {
      window->EnableTimeChangeNotifications();
    }
  } else if (aTypeAtom == nsGkAtoms::onmoznetworkupload) {
    nsCOMPtr<nsPIDOMWindow> window = GetTargetAsInnerWindow();
    if (window) {
      window->EnableNetworkEvent(NS_NETWORK_UPLOAD_EVENT);
    }
  } else if (aTypeAtom == nsGkAtoms::onmoznetworkdownload) {
    nsCOMPtr<nsPIDOMWindow> window = GetTargetAsInnerWindow();
    if (window) {
      window->EnableNetworkEvent(NS_NETWORK_DOWNLOAD_EVENT);
    }
#endif // MOZ_B2G
  } else if (aTypeAtom == nsGkAtoms::ontouchstart ||
             aTypeAtom == nsGkAtoms::ontouchend ||
             aTypeAtom == nsGkAtoms::ontouchmove ||
             aTypeAtom == nsGkAtoms::ontouchcancel) {
    mMayHaveTouchEventListener = true;
    nsPIDOMWindow* window = GetInnerWindowForTarget();
    // we don't want touchevent listeners added by scrollbars to flip this flag
    // so we ignore listeners created with system event flag
    if (window && !aFlags.mInSystemGroup) {
      window->SetHasTouchEventListeners();
    }
  } else if (aEventMessage >= ePointerEventFirst &&
             aEventMessage <= ePointerEventLast) {
    nsPIDOMWindow* window = GetInnerWindowForTarget();
    if (aTypeAtom == nsGkAtoms::onpointerenter ||
        aTypeAtom == nsGkAtoms::onpointerleave) {
      mMayHavePointerEnterLeaveEventListener = true;
      if (window) {
#ifdef DEBUG
        nsCOMPtr<nsIDocument> d = window->GetExtantDoc();
        NS_WARN_IF_FALSE(!nsContentUtils::IsChromeDoc(d),
                         "Please do not use pointerenter/leave events in chrome. "
                         "They are slower than pointerover/out!");
#endif
        window->SetHasPointerEnterLeaveEventListeners();
      }
    }
  } else if (aTypeAtom == nsGkAtoms::onmouseenter ||
             aTypeAtom == nsGkAtoms::onmouseleave) {
    mMayHaveMouseEnterLeaveEventListener = true;
    nsPIDOMWindow* window = GetInnerWindowForTarget();
    if (window) {
#ifdef DEBUG
      nsCOMPtr<nsIDocument> d = window->GetExtantDoc();
      NS_WARN_IF_FALSE(!nsContentUtils::IsChromeDoc(d),
                       "Please do not use mouseenter/leave events in chrome. "
                       "They are slower than mouseover/out!");
#endif
      window->SetHasMouseEnterLeaveEventListeners();
    }
#ifdef MOZ_GAMEPAD
  } else if (aEventMessage >= NS_GAMEPAD_START &&
             aEventMessage <= NS_GAMEPAD_END) {
    nsPIDOMWindow* window = GetInnerWindowForTarget();
    if (window) {
      window->SetHasGamepadEventListener();
    }
#endif
  } else if (aTypeAtom == nsGkAtoms::onkeydown ||
             aTypeAtom == nsGkAtoms::onkeypress ||
             aTypeAtom == nsGkAtoms::onkeyup) {
    if (!aFlags.mInSystemGroup) {
      mMayHaveKeyEventListener = true;
    }
  } else if (aTypeAtom == nsGkAtoms::oncompositionend ||
             aTypeAtom == nsGkAtoms::oncompositionstart ||
             aTypeAtom == nsGkAtoms::oncompositionupdate ||
             aTypeAtom == nsGkAtoms::oninput) {
    if (!aFlags.mInSystemGroup) {
      mMayHaveInputOrCompositionEventListener = true;
    }
  }

  if (aTypeAtom && mTarget) {
    mTarget->EventListenerAdded(aTypeAtom);
  }

  if (mIsMainThreadELM && mTarget) {
    EventListenerService::NotifyAboutMainThreadListenerChange(mTarget,
                                                              aTypeAtom);
  }
}

bool
EventListenerManager::IsDeviceType(EventMessage aEventMessage)
{
  switch (aEventMessage) {
    case NS_DEVICE_ORIENTATION:
    case NS_DEVICE_MOTION:
    case NS_DEVICE_LIGHT:
    case NS_DEVICE_PROXIMITY:
    case NS_USER_PROXIMITY:
      return true;
    default:
      break;
  }
  return false;
}

void
EventListenerManager::EnableDevice(EventMessage aEventMessage)
{
  nsCOMPtr<nsPIDOMWindow> window = GetTargetAsInnerWindow();
  if (!window) {
    return;
  }

  switch (aEventMessage) {
    case NS_DEVICE_ORIENTATION:
      window->EnableDeviceSensor(SENSOR_ORIENTATION);
      break;
    case NS_DEVICE_PROXIMITY:
    case NS_USER_PROXIMITY:
      window->EnableDeviceSensor(SENSOR_PROXIMITY);
      break;
    case NS_DEVICE_LIGHT:
      window->EnableDeviceSensor(SENSOR_LIGHT);
      break;
    case NS_DEVICE_MOTION:
      window->EnableDeviceSensor(SENSOR_ACCELERATION);
      window->EnableDeviceSensor(SENSOR_LINEAR_ACCELERATION);
      window->EnableDeviceSensor(SENSOR_GYROSCOPE);
      break;
    default:
      NS_WARNING("Enabling an unknown device sensor.");
      break;
  }
}

void
EventListenerManager::DisableDevice(EventMessage aEventMessage)
{
  nsCOMPtr<nsPIDOMWindow> window = GetTargetAsInnerWindow();
  if (!window) {
    return;
  }

  switch (aEventMessage) {
    case NS_DEVICE_ORIENTATION:
      window->DisableDeviceSensor(SENSOR_ORIENTATION);
      break;
    case NS_DEVICE_MOTION:
      window->DisableDeviceSensor(SENSOR_ACCELERATION);
      window->DisableDeviceSensor(SENSOR_LINEAR_ACCELERATION);
      window->DisableDeviceSensor(SENSOR_GYROSCOPE);
      break;
    case NS_DEVICE_PROXIMITY:
    case NS_USER_PROXIMITY:
      window->DisableDeviceSensor(SENSOR_PROXIMITY);
      break;
    case NS_DEVICE_LIGHT:
      window->DisableDeviceSensor(SENSOR_LIGHT);
      break;
    default:
      NS_WARNING("Disabling an unknown device sensor.");
      break;
  }
}

void
EventListenerManager::RemoveEventListenerInternal(
                        const EventListenerHolder& aListenerHolder,
                        EventMessage aEventMessage,
                        nsIAtom* aUserType,
                        const nsAString& aTypeString,
                        const EventListenerFlags& aFlags,
                        bool aAllEvents)
{
  if (!aListenerHolder || !aEventMessage || mClearingListeners) {
    return;
  }

  Listener* listener;

  uint32_t count = mListeners.Length();
  uint32_t typeCount = 0;
  bool deviceType = IsDeviceType(aEventMessage);
#ifdef MOZ_B2G
  bool timeChangeEvent = (aEventMessage == NS_MOZ_TIME_CHANGE_EVENT);
  bool networkEvent = (aEventMessage == NS_NETWORK_UPLOAD_EVENT ||
                       aEventMessage == NS_NETWORK_DOWNLOAD_EVENT);
#endif // MOZ_B2G

  for (uint32_t i = 0; i < count; ++i) {
    listener = &mListeners.ElementAt(i);
    if (EVENT_TYPE_EQUALS(listener, aEventMessage, aUserType, aTypeString,
                          aAllEvents)) {
      ++typeCount;
      if (listener->mListener == aListenerHolder &&
          listener->mFlags.EqualsIgnoringTrustness(aFlags)) {
        nsRefPtr<EventListenerManager> kungFuDeathGrip(this);
        mListeners.RemoveElementAt(i);
        --count;
        mNoListenerForEvent = eVoidEvent;
        mNoListenerForEventAtom = nullptr;
        if (mTarget && aUserType) {
          mTarget->EventListenerRemoved(aUserType);
        }
        if (mIsMainThreadELM && mTarget) {
          EventListenerService::NotifyAboutMainThreadListenerChange(mTarget,
                                                                    aUserType);
        }

        if (!deviceType
#ifdef MOZ_B2G
            && !timeChangeEvent && !networkEvent
#endif // MOZ_B2G
            ) {
          return;
        }
        --typeCount;
      }
    }
  }

  if (!aAllEvents && deviceType && typeCount == 0) {
    DisableDevice(aEventMessage);
#ifdef MOZ_B2G
  } else if (timeChangeEvent && typeCount == 0) {
    nsCOMPtr<nsPIDOMWindow> window = GetTargetAsInnerWindow();
    if (window) {
      window->DisableTimeChangeNotifications();
    }
  } else if (!aAllEvents && networkEvent && typeCount == 0) {
    nsCOMPtr<nsPIDOMWindow> window = GetTargetAsInnerWindow();
    if (window) {
      window->DisableNetworkEvent(aEventMessage);
    }
#endif // MOZ_B2G
  }
}

bool
EventListenerManager::ListenerCanHandle(Listener* aListener,
                                        WidgetEvent* aEvent)
{
  // This is slightly different from EVENT_TYPE_EQUALS in that it returns
  // true even when aEvent->mMessage == NS_USER_DEFINED_EVENT and
  // aListener=>mEventMessage != NS_USER_DEFINED_EVENT as long as the atoms are
  // the same
  if (aListener->mAllEvents) {
    return true;
  }
  if (aEvent->mMessage == NS_USER_DEFINED_EVENT) {
    if (mIsMainThreadELM) {
      return aListener->mTypeAtom == aEvent->userType;
    }
    return aListener->mTypeString.Equals(aEvent->typeString);
  }
  MOZ_ASSERT(mIsMainThreadELM);
  return aListener->mEventMessage == aEvent->mMessage;
}

void
EventListenerManager::AddEventListenerByType(
                        const EventListenerHolder& aListenerHolder,
                        const nsAString& aType,
                        const EventListenerFlags& aFlags)
{
  nsCOMPtr<nsIAtom> atom =
    mIsMainThreadELM ? do_GetAtom(NS_LITERAL_STRING("on") + aType) : nullptr;
  EventMessage message = nsContentUtils::GetEventMessage(atom);
  AddEventListenerInternal(aListenerHolder, message, atom, aType, aFlags);
}

void
EventListenerManager::RemoveEventListenerByType(
                        const EventListenerHolder& aListenerHolder,
                        const nsAString& aType,
                        const EventListenerFlags& aFlags)
{
  nsCOMPtr<nsIAtom> atom =
    mIsMainThreadELM ? do_GetAtom(NS_LITERAL_STRING("on") + aType) : nullptr;
  EventMessage message = nsContentUtils::GetEventMessage(atom);
  RemoveEventListenerInternal(aListenerHolder, message, atom, aType, aFlags);
}

EventListenerManager::Listener*
EventListenerManager::FindEventHandler(EventMessage aEventMessage,
                                       nsIAtom* aTypeAtom,
                                       const nsAString& aTypeString)
{
  // Run through the listeners for this type and see if a script
  // listener is registered
  Listener* listener;
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; ++i) {
    listener = &mListeners.ElementAt(i);
    if (listener->mListenerIsHandler &&
        EVENT_TYPE_EQUALS(listener, aEventMessage, aTypeAtom, aTypeString,
                          false)) {
      return listener;
    }
  }
  return nullptr;
}

EventListenerManager::Listener*
EventListenerManager::SetEventHandlerInternal(
                        nsIAtom* aName,
                        const nsAString& aTypeString,
                        const TypedEventHandler& aTypedHandler,
                        bool aPermitUntrustedEvents)
{
  MOZ_ASSERT(aName || !aTypeString.IsEmpty());

  EventMessage eventMessage = nsContentUtils::GetEventMessage(aName);
  Listener* listener = FindEventHandler(eventMessage, aName, aTypeString);

  if (!listener) {
    // If we didn't find a script listener or no listeners existed
    // create and add a new one.
    EventListenerFlags flags;
    flags.mListenerIsJSListener = true;

    nsCOMPtr<JSEventHandler> jsEventHandler;
    NS_NewJSEventHandler(mTarget, aName,
                         aTypedHandler, getter_AddRefs(jsEventHandler));
    EventListenerHolder listenerHolder(jsEventHandler);
    AddEventListenerInternal(listenerHolder, eventMessage, aName, aTypeString,
                             flags, true);

    listener = FindEventHandler(eventMessage, aName, aTypeString);
  } else {
    JSEventHandler* jsEventHandler = listener->GetJSEventHandler();
    MOZ_ASSERT(jsEventHandler,
               "How can we have an event handler with no JSEventHandler?");

    bool same = jsEventHandler->GetTypedEventHandler() == aTypedHandler;
    // Possibly the same listener, but update still the context and scope.
    jsEventHandler->SetHandler(aTypedHandler);
    if (mTarget && !same && aName) {
      mTarget->EventListenerRemoved(aName);
      mTarget->EventListenerAdded(aName);
    }
    if (mIsMainThreadELM && mTarget) {
      EventListenerService::NotifyAboutMainThreadListenerChange(mTarget, aName);
    }
  }

  // Set flag to indicate possible need for compilation later
  listener->mHandlerIsString = !aTypedHandler.HasEventHandler();
  if (aPermitUntrustedEvents) {
    listener->mFlags.mAllowUntrustedEvents = true;
  }

  return listener;
}

nsresult
EventListenerManager::SetEventHandler(nsIAtom* aName,
                                      const nsAString& aBody,
                                      bool aDeferCompilation,
                                      bool aPermitUntrustedEvents,
                                      Element* aElement)
{
  nsCOMPtr<nsIDocument> doc;
  nsCOMPtr<nsIScriptGlobalObject> global =
    GetScriptGlobalAndDocument(getter_AddRefs(doc));

  if (!global) {
    // This can happen; for example this document might have been
    // loaded as data.
    return NS_OK;
  }

#ifdef DEBUG
  nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(global);
  if (win) {
    MOZ_ASSERT(win->IsInnerWindow(), "We should not have an outer window here!");
  }
#endif

  nsresult rv = NS_OK;
  // return early preventing the event listener from being added
  // 'doc' is fetched above
  if (doc) {
    // Don't allow adding an event listener if the document is sandboxed
    // without 'allow-scripts'.
    if (doc->GetSandboxFlags() & SANDBOXED_SCRIPTS) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }

    nsCOMPtr<nsIContentSecurityPolicy> csp;
    rv = doc->NodePrincipal()->GetCsp(getter_AddRefs(csp));
    NS_ENSURE_SUCCESS(rv, rv);

    if (csp) {
      bool inlineOK = true;
      bool reportViolations = false;
      rv = csp->GetAllowsInlineScript(&reportViolations, &inlineOK);
      NS_ENSURE_SUCCESS(rv, rv);

      if (reportViolations) {
        // gather information to log with violation report
        nsIURI* uri = doc->GetDocumentURI();
        nsAutoCString asciiSpec;
        if (uri)
          uri->GetAsciiSpec(asciiSpec);
        nsAutoString scriptSample, attr, tagName(NS_LITERAL_STRING("UNKNOWN"));
        aName->ToString(attr);
        nsCOMPtr<nsIDOMNode> domNode(do_QueryInterface(mTarget));
        if (domNode)
          domNode->GetNodeName(tagName);
        // build a "script sample" based on what we know about this element
        scriptSample.Assign(attr);
        scriptSample.AppendLiteral(" attribute on ");
        scriptSample.Append(tagName);
        scriptSample.AppendLiteral(" element");
        csp->LogViolationDetails(nsIContentSecurityPolicy::VIOLATION_TYPE_INLINE_SCRIPT,
                                 NS_ConvertUTF8toUTF16(asciiSpec),
                                 scriptSample,
                                 0,
                                 EmptyString(),
                                 EmptyString());
      }

      // return early if CSP wants us to block inline scripts
      if (!inlineOK) {
        return NS_OK;
      }
    }
  }

  // This might be the first reference to this language in the global
  // We must init the language before we attempt to fetch its context.
  if (NS_FAILED(global->EnsureScriptEnvironment())) {
    NS_WARNING("Failed to setup script environment for this language");
    // but fall through and let the inevitable failure below handle it.
  }

  nsIScriptContext* context = global->GetScriptContext();
  NS_ENSURE_TRUE(context, NS_ERROR_FAILURE);
  NS_ENSURE_STATE(global->GetGlobalJSObject());

  Listener* listener = SetEventHandlerInternal(aName,
                                               EmptyString(),
                                               TypedEventHandler(),
                                               aPermitUntrustedEvents);

  if (!aDeferCompilation) {
    return CompileEventHandlerInternal(listener, &aBody, aElement);
  }

  return NS_OK;
}

void
EventListenerManager::RemoveEventHandler(nsIAtom* aName,
                                         const nsAString& aTypeString)
{
  if (mClearingListeners) {
    return;
  }

  EventMessage eventMessage = nsContentUtils::GetEventMessage(aName);
  Listener* listener = FindEventHandler(eventMessage, aName, aTypeString);

  if (listener) {
    mListeners.RemoveElementAt(uint32_t(listener - &mListeners.ElementAt(0)));
    mNoListenerForEvent = eVoidEvent;
    mNoListenerForEventAtom = nullptr;
    if (mTarget && aName) {
      mTarget->EventListenerRemoved(aName);
    }
    if (mIsMainThreadELM && mTarget) {
      EventListenerService::NotifyAboutMainThreadListenerChange(mTarget, aName);
    }
  }
}

nsresult
EventListenerManager::CompileEventHandlerInternal(Listener* aListener,
                                                  const nsAString* aBody,
                                                  Element* aElement)
{
  MOZ_ASSERT(aListener->GetJSEventHandler());
  MOZ_ASSERT(aListener->mHandlerIsString, "Why are we compiling a non-string JS listener?");
  JSEventHandler* jsEventHandler = aListener->GetJSEventHandler();
  MOZ_ASSERT(!jsEventHandler->GetTypedEventHandler().HasEventHandler(),
             "What is there to compile?");

  nsresult result = NS_OK;
  nsCOMPtr<nsIDocument> doc;
  nsCOMPtr<nsIScriptGlobalObject> global =
    GetScriptGlobalAndDocument(getter_AddRefs(doc));
  NS_ENSURE_STATE(global);

  // Activate JSAPI, and make sure that exceptions are reported on the right
  // Window.
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(global))) {
    return NS_ERROR_UNEXPECTED;
  }
  jsapi.TakeOwnershipOfErrorReporting();
  JSContext* cx = jsapi.cx();

  nsCOMPtr<nsIAtom> typeAtom = aListener->mTypeAtom;
  nsIAtom* attrName = typeAtom;

  // Flag us as not a string so we don't keep trying to compile strings which
  // can't be compiled.
  aListener->mHandlerIsString = false;

  // mTarget may not be an Element if it's a window and we're
  // getting an inline event listener forwarded from <html:body> or
  // <html:frameset> or <xul:window> or the like.
  // XXX I don't like that we have to reference content from
  // here. The alternative is to store the event handler string on
  // the JSEventHandler itself, and that still doesn't address
  // the arg names issue.
  nsCOMPtr<Element> element = do_QueryInterface(mTarget);
  MOZ_ASSERT(element || aBody, "Where will we get our body?");
  nsAutoString handlerBody;
  const nsAString* body = aBody;
  if (!aBody) {
    if (aListener->mTypeAtom == nsGkAtoms::onSVGLoad) {
      attrName = nsGkAtoms::onload;
    } else if (aListener->mTypeAtom == nsGkAtoms::onSVGUnload) {
      attrName = nsGkAtoms::onunload;
    } else if (aListener->mTypeAtom == nsGkAtoms::onSVGResize) {
      attrName = nsGkAtoms::onresize;
    } else if (aListener->mTypeAtom == nsGkAtoms::onSVGScroll) {
      attrName = nsGkAtoms::onscroll;
    } else if (aListener->mTypeAtom == nsGkAtoms::onSVGZoom) {
      attrName = nsGkAtoms::onzoom;
    } else if (aListener->mTypeAtom == nsGkAtoms::onbeginEvent) {
      attrName = nsGkAtoms::onbegin;
    } else if (aListener->mTypeAtom == nsGkAtoms::onrepeatEvent) {
      attrName = nsGkAtoms::onrepeat;
    } else if (aListener->mTypeAtom == nsGkAtoms::onendEvent) {
      attrName = nsGkAtoms::onend;
    }

    element->GetAttr(kNameSpaceID_None, attrName, handlerBody);
    body = &handlerBody;
    aElement = element;
  }
  aListener = nullptr;

  uint32_t lineNo = 0;
  nsAutoCString url (NS_LITERAL_CSTRING("-moz-evil:lying-event-listener"));
  MOZ_ASSERT(body);
  MOZ_ASSERT(aElement);
  nsIURI *uri = aElement->OwnerDoc()->GetDocumentURI();
  if (uri) {
    uri->GetSpec(url);
    lineNo = 1;
  }

  nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(mTarget);
  uint32_t argCount;
  const char **argNames;
  nsContentUtils::GetEventArgNames(aElement->GetNameSpaceID(),
                                   typeAtom, win,
                                   &argCount, &argNames);

  JSAddonId *addonId = MapURIToAddonID(uri);

  // Wrap the event target, so that we can use it as the scope for the event
  // handler. Note that mTarget is different from aElement in the <body> case,
  // where mTarget is a Window.
  //
  // The wrapScope doesn't really matter here, because the target will create
  // its reflector in the proper scope, and then we'll enter that compartment.
  JS::Rooted<JSObject*> wrapScope(cx, global->GetGlobalJSObject());
  JS::Rooted<JS::Value> v(cx);
  {
    JSAutoCompartment ac(cx, wrapScope);
    nsresult rv = nsContentUtils::WrapNative(cx, mTarget, &v,
                                             /* aAllowWrapping = */ false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (addonId) {
    JS::Rooted<JSObject*> vObj(cx, &v.toObject());
    JS::Rooted<JSObject*> addonScope(cx, xpc::GetAddonScope(cx, vObj, addonId));
    if (!addonScope) {
      return NS_ERROR_FAILURE;
    }
    JSAutoCompartment ac(cx, addonScope);

    // Wrap our event target into the addon scope, since that's where we want to
    // do all our work.
    if (!JS_WrapValue(cx, &v)) {
      return NS_ERROR_FAILURE;
    }
  }
  JS::Rooted<JSObject*> target(cx, &v.toObject());
  JSAutoCompartment ac(cx, target);

  // Now that we've entered the compartment we actually care about, create our
  // scope chain.  Note that we start with |element|, not aElement, because
  // mTarget is different from aElement in the <body> case, where mTarget is a
  // Window, and in that case we do not want the scope chain to include the body
  // or the document.
  JS::AutoObjectVector scopeChain(cx);
  if (!nsJSUtils::GetScopeChainForElement(cx, element, scopeChain)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsDependentAtomString str(attrName);
  // Most of our names are short enough that we don't even have to malloc
  // the JS string stuff, so don't worry about playing games with
  // refcounting XPCOM stringbuffers.
  JS::Rooted<JSString*> jsStr(cx, JS_NewUCStringCopyN(cx,
                                                      str.BeginReading(),
                                                      str.Length()));
  NS_ENSURE_TRUE(jsStr, NS_ERROR_OUT_OF_MEMORY);

  // Get the reflector for |aElement|, so that we can pass to setElement.
  if (NS_WARN_IF(!GetOrCreateDOMReflector(cx, aElement, &v))) {
    return NS_ERROR_FAILURE;
  }
  JS::CompileOptions options(cx);
  options.setIntroductionType("eventHandler")
         .setFileAndLine(url.get(), lineNo)
         .setVersion(JSVERSION_DEFAULT)
         .setElement(&v.toObject())
         .setElementAttributeName(jsStr);

  JS::Rooted<JSObject*> handler(cx);
  result = nsJSUtils::CompileFunction(jsapi, scopeChain, options,
                                      nsAtomCString(typeAtom),
                                      argCount, argNames, *body, handler.address());
  NS_ENSURE_SUCCESS(result, result);
  NS_ENSURE_TRUE(handler, NS_ERROR_FAILURE);

  if (jsEventHandler->EventName() == nsGkAtoms::onerror && win) {
    nsRefPtr<OnErrorEventHandlerNonNull> handlerCallback =
      new OnErrorEventHandlerNonNull(nullptr, handler, /* aIncumbentGlobal = */ nullptr);
    jsEventHandler->SetHandler(handlerCallback);
  } else if (jsEventHandler->EventName() == nsGkAtoms::onbeforeunload && win) {
    nsRefPtr<OnBeforeUnloadEventHandlerNonNull> handlerCallback =
      new OnBeforeUnloadEventHandlerNonNull(nullptr, handler, /* aIncumbentGlobal = */ nullptr);
    jsEventHandler->SetHandler(handlerCallback);
  } else {
    nsRefPtr<EventHandlerNonNull> handlerCallback =
      new EventHandlerNonNull(nullptr, handler, /* aIncumbentGlobal = */ nullptr);
    jsEventHandler->SetHandler(handlerCallback);
  }

  return result;
}

nsresult
EventListenerManager::HandleEventSubType(Listener* aListener,
                                         nsIDOMEvent* aDOMEvent,
                                         EventTarget* aCurrentTarget)
{
  nsresult result = NS_OK;
  EventListenerHolder listenerHolder(aListener->mListener);  // strong ref

  // If this is a script handler and we haven't yet
  // compiled the event handler itself
  if ((aListener->mListenerType == Listener::eJSEventListener) &&
      aListener->mHandlerIsString) {
    result = CompileEventHandlerInternal(aListener, nullptr, nullptr);
    aListener = nullptr;
  }

  if (NS_SUCCEEDED(result)) {
    if (mIsMainThreadELM) {
      nsContentUtils::EnterMicroTask();
    }
    // nsIDOMEvent::currentTarget is set in EventDispatcher.
    if (listenerHolder.HasWebIDLCallback()) {
      ErrorResult rv;
      listenerHolder.GetWebIDLCallback()->
        HandleEvent(aCurrentTarget, *(aDOMEvent->InternalDOMEvent()), rv);
      result = rv.StealNSResult();
    } else {
      result = listenerHolder.GetXPCOMCallback()->HandleEvent(aDOMEvent);
    }
    if (mIsMainThreadELM) {
      nsContentUtils::LeaveMicroTask();
    }
  }

  return result;
}

nsIDocShell*
EventListenerManager::GetDocShellForTarget()
{
  nsCOMPtr<nsINode> node(do_QueryInterface(mTarget));
  nsIDocument* doc = nullptr;
  nsIDocShell* docShell = nullptr;

  if (node) {
    doc = node->OwnerDoc();
    if (!doc->GetDocShell()) {
      bool ignore;
      nsCOMPtr<nsPIDOMWindow> window =
        do_QueryInterface(doc->GetScriptHandlingObject(ignore));
      if (window) {
        doc = window->GetExtantDoc();
      }
    }
  } else {
    nsCOMPtr<nsPIDOMWindow> window = GetTargetAsInnerWindow();
    if (window) {
      doc = window->GetExtantDoc();
    }
  }

  if (!doc) {
    nsCOMPtr<DOMEventTargetHelper> helper(do_QueryInterface(mTarget));
    if (helper) {
      nsPIDOMWindow* window = helper->GetOwner();
      if (window) {
        doc = window->GetExtantDoc();
      }
    }
  }

  if (doc) {
    docShell = doc->GetDocShell();
  }

  return docShell;
}

/**
* Causes a check for event listeners and processing by them if they exist.
* @param an event listener
*/

void
EventListenerManager::HandleEventInternal(nsPresContext* aPresContext,
                                          WidgetEvent* aEvent,
                                          nsIDOMEvent** aDOMEvent,
                                          EventTarget* aCurrentTarget,
                                          nsEventStatus* aEventStatus)
{
  //Set the value of the internal PreventDefault flag properly based on aEventStatus
  if (*aEventStatus == nsEventStatus_eConsumeNoDefault) {
    aEvent->mFlags.mDefaultPrevented = true;
  }

  nsAutoTObserverArray<Listener, 2>::EndLimitedIterator iter(mListeners);
  Maybe<nsAutoPopupStatePusher> popupStatePusher;
  if (mIsMainThreadELM) {
    popupStatePusher.emplace(Event::GetEventPopupControlState(aEvent, *aDOMEvent));
  }

  bool hasListener = false;
  while (iter.HasMore()) {
    if (aEvent->mFlags.mImmediatePropagationStopped) {
      break;
    }
    Listener* listener = &iter.GetNext();
    // Check that the phase is same in event and event listener.
    // Handle only trusted events, except when listener permits untrusted events.
    if (ListenerCanHandle(listener, aEvent)) {
      hasListener = true;
      if (listener->IsListening(aEvent) &&
          (aEvent->mFlags.mIsTrusted ||
           listener->mFlags.mAllowUntrustedEvents)) {
        if (!*aDOMEvent) {
          // This is tiny bit slow, but happens only once per event.
          nsCOMPtr<EventTarget> et =
            do_QueryInterface(aEvent->originalTarget);
          nsRefPtr<Event> event = EventDispatcher::CreateEvent(et, aPresContext,
                                                               aEvent,
                                                               EmptyString());
          event.forget(aDOMEvent);
        }
        if (*aDOMEvent) {
          if (!aEvent->currentTarget) {
            aEvent->currentTarget = aCurrentTarget->GetTargetForDOMEvent();
            if (!aEvent->currentTarget) {
              break;
            }
          }

          // Maybe add a marker to the docshell's timeline, but only
          // bother with all the logic if some docshell is recording.
          nsCOMPtr<nsIDocShell> docShell;
          bool isTimelineRecording = false;
          if (mIsMainThreadELM &&
              !TimelineConsumers::IsEmpty() &&
              listener->mListenerType != Listener::eNativeListener) {
            docShell = GetDocShellForTarget();
            if (docShell) {
              docShell->GetRecordProfileTimelineMarkers(&isTimelineRecording);
            }
            if (isTimelineRecording) {
              nsDocShell* ds = static_cast<nsDocShell*>(docShell.get());
              nsAutoString typeStr;
              (*aDOMEvent)->GetType(typeStr);
              uint16_t phase;
              (*aDOMEvent)->GetEventPhase(&phase);
              UniquePtr<TimelineMarker> marker = MakeUnique<EventTimelineMarker>(
                typeStr, phase, MarkerTracingType::START);
              TimelineConsumers::AddMarkerForDocShell(ds, Move(marker));
            }
          }

          if (NS_FAILED(HandleEventSubType(listener, *aDOMEvent,
                                           aCurrentTarget))) {
            aEvent->mFlags.mExceptionHasBeenRisen = true;
          }

          if (isTimelineRecording) {
            nsDocShell* ds = static_cast<nsDocShell*>(docShell.get());
            TimelineConsumers::AddMarkerForDocShell(ds, "DOMEvent", MarkerTracingType::END);
          }
        }
      }
    }
  }

  aEvent->currentTarget = nullptr;

  if (mIsMainThreadELM && !hasListener) {
    mNoListenerForEvent = aEvent->mMessage;
    mNoListenerForEventAtom = aEvent->userType;
  }

  if (aEvent->mFlags.mDefaultPrevented) {
    *aEventStatus = nsEventStatus_eConsumeNoDefault;
  }
}

void
EventListenerManager::Disconnect()
{
  mTarget = nullptr;
  RemoveAllListeners();
}

void
EventListenerManager::AddEventListener(
                        const nsAString& aType,
                        const EventListenerHolder& aListenerHolder,
                        bool aUseCapture,
                        bool aWantsUntrusted)
{
  EventListenerFlags flags;
  flags.mCapture = aUseCapture;
  flags.mAllowUntrustedEvents = aWantsUntrusted;
  return AddEventListenerByType(aListenerHolder, aType, flags);
}

void
EventListenerManager::RemoveEventListener(
                        const nsAString& aType,
                        const EventListenerHolder& aListenerHolder,
                        bool aUseCapture)
{
  EventListenerFlags flags;
  flags.mCapture = aUseCapture;
  RemoveEventListenerByType(aListenerHolder, aType, flags);
}

void
EventListenerManager::AddListenerForAllEvents(nsIDOMEventListener* aDOMListener,
                                              bool aUseCapture,
                                              bool aWantsUntrusted,
                                              bool aSystemEventGroup)
{
  EventListenerFlags flags;
  flags.mCapture = aUseCapture;
  flags.mAllowUntrustedEvents = aWantsUntrusted;
  flags.mInSystemGroup = aSystemEventGroup;
  EventListenerHolder listenerHolder(aDOMListener);
  AddEventListenerInternal(listenerHolder, eAllEvents, nullptr, EmptyString(),
                           flags, false, true);
}

void
EventListenerManager::RemoveListenerForAllEvents(
                        nsIDOMEventListener* aDOMListener,
                        bool aUseCapture,
                        bool aSystemEventGroup)
{
  EventListenerFlags flags;
  flags.mCapture = aUseCapture;
  flags.mInSystemGroup = aSystemEventGroup;
  EventListenerHolder listenerHolder(aDOMListener);
  RemoveEventListenerInternal(listenerHolder, eAllEvents, nullptr,
                              EmptyString(), flags, true);
}

bool
EventListenerManager::HasMutationListeners()
{
  if (mMayHaveMutationListeners) {
    uint32_t count = mListeners.Length();
    for (uint32_t i = 0; i < count; ++i) {
      Listener* listener = &mListeners.ElementAt(i);
      if (listener->mEventMessage >= eLegacyMutationEventFirst &&
          listener->mEventMessage <= eLegacyMutationEventLast) {
        return true;
      }
    }
  }

  return false;
}

uint32_t
EventListenerManager::MutationListenerBits()
{
  uint32_t bits = 0;
  if (mMayHaveMutationListeners) {
    uint32_t count = mListeners.Length();
    for (uint32_t i = 0; i < count; ++i) {
      Listener* listener = &mListeners.ElementAt(i);
      if (listener->mEventMessage >= eLegacyMutationEventFirst &&
          listener->mEventMessage <= eLegacyMutationEventLast) {
        if (listener->mEventMessage == eLegacySubtreeModified) {
          return kAllMutationBits;
        }
        bits |= MutationBitForEventType(listener->mEventMessage);
      }
    }
  }
  return bits;
}

bool
EventListenerManager::HasListenersFor(const nsAString& aEventName)
{
  if (mIsMainThreadELM) {
    nsCOMPtr<nsIAtom> atom = do_GetAtom(NS_LITERAL_STRING("on") + aEventName);
    return HasListenersFor(atom);
  }

  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; ++i) {
    Listener* listener = &mListeners.ElementAt(i);
    if (listener->mTypeString == aEventName) {
      return true;
    }
  }
  return false;
}

bool
EventListenerManager::HasListenersFor(nsIAtom* aEventNameWithOn)
{
#ifdef DEBUG
  nsAutoString name;
  aEventNameWithOn->ToString(name);
#endif
  NS_ASSERTION(StringBeginsWith(name, NS_LITERAL_STRING("on")),
               "Event name does not start with 'on'");
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; ++i) {
    Listener* listener = &mListeners.ElementAt(i);
    if (listener->mTypeAtom == aEventNameWithOn) {
      return true;
    }
  }
  return false;
}

bool
EventListenerManager::HasListeners()
{
  return !mListeners.IsEmpty();
}

nsresult
EventListenerManager::GetListenerInfo(nsCOMArray<nsIEventListenerInfo>* aList)
{
  nsCOMPtr<EventTarget> target = do_QueryInterface(mTarget);
  NS_ENSURE_STATE(target);
  aList->Clear();
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; ++i) {
    const Listener& listener = mListeners.ElementAt(i);
    // If this is a script handler and we haven't yet
    // compiled the event handler itself go ahead and compile it
    if (listener.mListenerType == Listener::eJSEventListener &&
        listener.mHandlerIsString) {
      CompileEventHandlerInternal(const_cast<Listener*>(&listener), nullptr,
                                  nullptr);
    }
    nsAutoString eventType;
    if (listener.mAllEvents) {
      eventType.SetIsVoid(true);
    } else {
      eventType.Assign(Substring(nsDependentAtomString(listener.mTypeAtom), 2));
    }
    // EventListenerInfo is defined in XPCOM, so we have to go ahead
    // and convert to an XPCOM callback here...
    nsRefPtr<EventListenerInfo> info =
      new EventListenerInfo(eventType, listener.mListener.ToXPCOMCallback(),
                            listener.mFlags.mCapture,
                            listener.mFlags.mAllowUntrustedEvents,
                            listener.mFlags.mInSystemGroup);
    aList->AppendObject(info);
  }
  return NS_OK;
}

bool
EventListenerManager::HasUnloadListeners()
{
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; ++i) {
    Listener* listener = &mListeners.ElementAt(i);
    if (listener->mEventMessage == eUnload ||
        listener->mEventMessage == eBeforeUnload) {
      return true;
    }
  }
  return false;
}

void
EventListenerManager::SetEventHandler(nsIAtom* aEventName,
                                      const nsAString& aTypeString,
                                      EventHandlerNonNull* aHandler)
{
  if (!aHandler) {
    RemoveEventHandler(aEventName, aTypeString);
    return;
  }

  // Untrusted events are always permitted for non-chrome script
  // handlers.
  SetEventHandlerInternal(aEventName, aTypeString, TypedEventHandler(aHandler),
                          !mIsMainThreadELM ||
                          !nsContentUtils::IsCallerChrome());
}

void
EventListenerManager::SetEventHandler(OnErrorEventHandlerNonNull* aHandler)
{
  if (mIsMainThreadELM) {
    if (!aHandler) {
      RemoveEventHandler(nsGkAtoms::onerror, EmptyString());
      return;
    }

    // Untrusted events are always permitted for non-chrome script
    // handlers.
    SetEventHandlerInternal(nsGkAtoms::onerror, EmptyString(),
                            TypedEventHandler(aHandler),
                            !nsContentUtils::IsCallerChrome());
  } else {
    if (!aHandler) {
      RemoveEventHandler(nullptr, NS_LITERAL_STRING("error"));
      return;
    }

    // Untrusted events are always permitted.
    SetEventHandlerInternal(nullptr, NS_LITERAL_STRING("error"),
                            TypedEventHandler(aHandler), true);
  }
}

void
EventListenerManager::SetEventHandler(
                        OnBeforeUnloadEventHandlerNonNull* aHandler)
{
  if (!aHandler) {
    RemoveEventHandler(nsGkAtoms::onbeforeunload, EmptyString());
    return;
  }

  // Untrusted events are always permitted for non-chrome script
  // handlers.
  SetEventHandlerInternal(nsGkAtoms::onbeforeunload, EmptyString(),
                          TypedEventHandler(aHandler),
                          !mIsMainThreadELM ||
                          !nsContentUtils::IsCallerChrome());
}

const TypedEventHandler*
EventListenerManager::GetTypedEventHandler(nsIAtom* aEventName,
                                           const nsAString& aTypeString)
{
  EventMessage eventMessage = nsContentUtils::GetEventMessage(aEventName);
  Listener* listener = FindEventHandler(eventMessage, aEventName, aTypeString);

  if (!listener) {
    return nullptr;
  }

  JSEventHandler* jsEventHandler = listener->GetJSEventHandler();

  if (listener->mHandlerIsString) {
    CompileEventHandlerInternal(listener, nullptr, nullptr);
  }

  const TypedEventHandler& typedHandler =
    jsEventHandler->GetTypedEventHandler();
  return typedHandler.HasEventHandler() ? &typedHandler : nullptr;
}

size_t
EventListenerManager::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  n += mListeners.ShallowSizeOfExcludingThis(aMallocSizeOf);
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; ++i) {
    JSEventHandler* jsEventHandler =
      mListeners.ElementAt(i).GetJSEventHandler();
    if (jsEventHandler) {
      n += jsEventHandler->SizeOfIncludingThis(aMallocSizeOf);
    }
  }
  return n;
}

void
EventListenerManager::MarkForCC()
{
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; ++i) {
    const Listener& listener = mListeners.ElementAt(i);
    JSEventHandler* jsEventHandler = listener.GetJSEventHandler();
    if (jsEventHandler) {
      const TypedEventHandler& typedHandler =
        jsEventHandler->GetTypedEventHandler();
      if (typedHandler.HasEventHandler()) {
        typedHandler.Ptr()->MarkForCC();
      }
    } else if (listener.mListenerType == Listener::eWrappedJSListener) {
      xpc_TryUnmarkWrappedGrayObject(listener.mListener.GetXPCOMCallback());
    } else if (listener.mListenerType == Listener::eWebIDLListener) {
      listener.mListener.GetWebIDLCallback()->MarkForCC();
    }
  }
  if (mRefCnt.IsPurple()) {
    mRefCnt.RemovePurple();
  }
}

void
EventListenerManager::TraceListeners(JSTracer* aTrc)
{
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; ++i) {
    const Listener& listener = mListeners.ElementAt(i);
    JSEventHandler* jsEventHandler = listener.GetJSEventHandler();
    if (jsEventHandler) {
      const TypedEventHandler& typedHandler =
        jsEventHandler->GetTypedEventHandler();
      if (typedHandler.HasEventHandler()) {
        mozilla::TraceScriptHolder(typedHandler.Ptr(), aTrc);
      }
    } else if (listener.mListenerType == Listener::eWebIDLListener) {
      mozilla::TraceScriptHolder(listener.mListener.GetWebIDLCallback(), aTrc);
    }
    // We might have eWrappedJSListener, but that is the legacy type for
    // JS implemented event listeners, and trickier to handle here.
  }
}

already_AddRefed<nsIScriptGlobalObject>
EventListenerManager::GetScriptGlobalAndDocument(nsIDocument** aDoc)
{
  nsCOMPtr<nsINode> node(do_QueryInterface(mTarget));
  nsCOMPtr<nsIDocument> doc;
  nsCOMPtr<nsIScriptGlobalObject> global;
  if (node) {
    // Try to get context from doc
    // XXX sXBL/XBL2 issue -- do we really want the owner here?  What
    // if that's the XBL document?
    doc = node->OwnerDoc();
    if (doc->IsLoadedAsData()) {
      return nullptr;
    }

    // We want to allow compiling an event handler even in an unloaded
    // document, so use GetScopeObject here, not GetScriptHandlingObject.
    global = do_QueryInterface(doc->GetScopeObject());
  } else {
    nsCOMPtr<nsPIDOMWindow> win = GetTargetAsInnerWindow();
    if (win) {
      doc = win->GetExtantDoc();
      global = do_QueryInterface(win);
    } else {
      global = do_QueryInterface(mTarget);
    }
  }

  doc.forget(aDoc);
  return global.forget();
}

} // namespace mozilla

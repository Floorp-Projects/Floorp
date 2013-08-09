/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Hal.h"
#include "mozilla/HalSensor.h"

// Microsoft's API Name hackery sucks
#undef CreateEvent

#include "nsISupports.h"
#include "nsGUIEvent.h"
#include "nsDOMEvent.h"
#include "nsEventListenerManager.h"
#include "nsCaret.h"
#include "nsIDOMEventListener.h"
#include "nsITextControlFrame.h"
#include "nsGkAtoms.h"
#include "nsPIDOMWindow.h"
#include "nsIJSEventListener.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptRuntime.h"
#include "nsLayoutUtils.h"
#include "nsINameSpaceManager.h"
#include "nsIContent.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/Element.h"
#include "nsIFrame.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIScriptSecurityManager.h"
#include "nsError.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsMutationEvent.h"
#include "nsIXPConnect.h"
#include "nsDOMCID.h"
#include "nsFocusManager.h"
#include "nsIDOMElement.h"
#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "nsJSUtils.h"
#include "nsContentCID.h"
#include "nsEventDispatcher.h"
#include "nsDOMJSUtils.h"
#include "nsDataHashtable.h"
#include "nsCOMArray.h"
#include "nsEventListenerService.h"
#include "nsIContentSecurityPolicy.h"
#include "nsJSEnvironment.h"
#include "xpcpublic.h"
#include "nsSandboxFlags.h"
#include "mozilla/dom/time/TimeChangeObserver.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::hal;

#define EVENT_TYPE_EQUALS(ls, type, userType, allEvents) \
  ((ls->mEventType == type && \
   (ls->mEventType != NS_USER_DEFINED_EVENT || ls->mTypeAtom == userType)) || \
   (allEvents && ls->mAllEvents))

static NS_DEFINE_CID(kDOMScriptObjectFactoryCID,
                     NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

static const uint32_t kAllMutationBits =
  NS_EVENT_BITS_MUTATION_SUBTREEMODIFIED |
  NS_EVENT_BITS_MUTATION_NODEINSERTED |
  NS_EVENT_BITS_MUTATION_NODEREMOVED |
  NS_EVENT_BITS_MUTATION_NODEREMOVEDFROMDOCUMENT |
  NS_EVENT_BITS_MUTATION_NODEINSERTEDINTODOCUMENT |
  NS_EVENT_BITS_MUTATION_ATTRMODIFIED |
  NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED;

static uint32_t
MutationBitForEventType(uint32_t aEventType)
{
  switch (aEventType) {
    case NS_MUTATION_SUBTREEMODIFIED:
      return NS_EVENT_BITS_MUTATION_SUBTREEMODIFIED;
    case NS_MUTATION_NODEINSERTED:
      return NS_EVENT_BITS_MUTATION_NODEINSERTED;
    case NS_MUTATION_NODEREMOVED:
      return NS_EVENT_BITS_MUTATION_NODEREMOVED;
    case NS_MUTATION_NODEREMOVEDFROMDOCUMENT:
      return NS_EVENT_BITS_MUTATION_NODEREMOVEDFROMDOCUMENT;
    case NS_MUTATION_NODEINSERTEDINTODOCUMENT:
      return NS_EVENT_BITS_MUTATION_NODEINSERTEDINTODOCUMENT;
    case NS_MUTATION_ATTRMODIFIED:
      return NS_EVENT_BITS_MUTATION_ATTRMODIFIED;
    case NS_MUTATION_CHARACTERDATAMODIFIED:
      return NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED;
    default:
      break;
  }
  return 0;
}

uint32_t nsEventListenerManager::sMainThreadCreatedCount = 0;

nsEventListenerManager::nsEventListenerManager(EventTarget* aTarget) :
  mMayHavePaintEventListener(false),
  mMayHaveMutationListeners(false),
  mMayHaveCapturingListeners(false),
  mMayHaveSystemGroupListeners(false),
  mMayHaveAudioAvailableEventListener(false),
  mMayHaveTouchEventListener(false),
  mMayHaveMouseEnterLeaveEventListener(false),
  mClearingListeners(false),
  mNoListenerForEvent(0),
  mTarget(aTarget)
{
  NS_ASSERTION(aTarget, "unexpected null pointer");

  if (NS_IsMainThread()) {
    ++sMainThreadCreatedCount;
  }
}

nsEventListenerManager::~nsEventListenerManager() 
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
nsEventListenerManager::RemoveAllListeners()
{
  if (mClearingListeners) {
    return;
  }
  mClearingListeners = true;
  mListeners.Clear();
  mClearingListeners = false;
}

void
nsEventListenerManager::Shutdown()
{
  nsDOMEvent::Shutdown();
}

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsEventListenerManager, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsEventListenerManager, Release)

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            nsListenerStruct& aField,
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

NS_IMPL_CYCLE_COLLECTION_CLASS(nsEventListenerManager)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsEventListenerManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mListeners)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsEventListenerManager)
  tmp->Disconnect();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END


nsPIDOMWindow*
nsEventListenerManager::GetInnerWindowForTarget()
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
nsEventListenerManager::GetTargetAsInnerWindow() const
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(mTarget);
  if (!window) {
    return nullptr;
  }

  NS_ASSERTION(window->IsInnerWindow(), "Target should not be an outer window");
  return window.forget();
}

void
nsEventListenerManager::AddEventListenerInternal(
                          const EventListenerHolder& aListener,
                          uint32_t aType,
                          nsIAtom* aTypeAtom,
                          const EventListenerFlags& aFlags,
                          bool aHandler,
                          bool aAllEvents)
{
  NS_ABORT_IF_FALSE((aType && aTypeAtom) || aAllEvents, "Missing type");

  if (!aListener || mClearingListeners) {
    return;
  }

  // Since there is no public API to call us with an EventListenerHolder, we
  // know that there's an EventListenerHolder on the stack holding a strong ref
  // to the listener.

  nsListenerStruct* ls;
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; i++) {
    ls = &mListeners.ElementAt(i);
    if (ls->mListener == aListener &&
        ls->mListenerIsHandler == aHandler &&
        ls->mFlags == aFlags &&
        EVENT_TYPE_EQUALS(ls, aType, aTypeAtom, aAllEvents)) {
      return;
    }
  }

  mNoListenerForEvent = NS_EVENT_TYPE_NULL;
  mNoListenerForEventAtom = nullptr;

  ls = aAllEvents ? mListeners.InsertElementAt(0) : mListeners.AppendElement();
  ls->mListener = aListener;
  ls->mEventType = aType;
  ls->mTypeAtom = aTypeAtom;
  ls->mFlags = aFlags;
  ls->mListenerIsHandler = aHandler;
  ls->mHandlerIsString = false;
  ls->mAllEvents = aAllEvents;

  // Detect the type of event listener.
  nsCOMPtr<nsIXPConnectWrappedJS> wjs;
  if (aFlags.mListenerIsJSListener) {
    MOZ_ASSERT(!aListener.HasWebIDLCallback());
    ls->mListenerType = eJSEventListener;
  } else if (aListener.HasWebIDLCallback()) {
    ls->mListenerType = eWebIDLListener;
  } else if ((wjs = do_QueryInterface(aListener.GetXPCOMCallback()))) {
    ls->mListenerType = eWrappedJSListener;
  } else {
    ls->mListenerType = eNativeListener;
  }


  if (aFlags.mInSystemGroup) {
    mMayHaveSystemGroupListeners = true;
  }
  if (aFlags.mCapture) {
    mMayHaveCapturingListeners = true;
  }

  if (aType == NS_AFTERPAINT) {
    mMayHavePaintEventListener = true;
    nsPIDOMWindow* window = GetInnerWindowForTarget();
    if (window) {
      window->SetHasPaintEventListeners();
    }
  } else if (aType == NS_MOZAUDIOAVAILABLE) {
    mMayHaveAudioAvailableEventListener = true;
    nsPIDOMWindow* window = GetInnerWindowForTarget();
    if (window) {
      window->SetHasAudioAvailableEventListeners();
    }
  } else if (aType >= NS_MUTATION_START && aType <= NS_MUTATION_END) {
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
      // If aType is NS_MUTATION_SUBTREEMODIFIED, we need to listen all
      // mutations. nsContentUtils::HasMutationListeners relies on this.
      window->SetMutationListeners((aType == NS_MUTATION_SUBTREEMODIFIED) ?
                                   kAllMutationBits :
                                   MutationBitForEventType(aType));
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
             aTypeAtom == nsGkAtoms::ontouchenter ||
             aTypeAtom == nsGkAtoms::ontouchleave ||
             aTypeAtom == nsGkAtoms::ontouchcancel) {
    mMayHaveTouchEventListener = true;
    nsPIDOMWindow* window = GetInnerWindowForTarget();
    // we don't want touchevent listeners added by scrollbars to flip this flag
    // so we ignore listeners created with system event flag
    if (window && !aFlags.mInSystemGroup) {
      window->SetHasTouchEventListeners();
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
  } else if (aType >= NS_GAMEPAD_START &&
             aType <= NS_GAMEPAD_END) {
    nsPIDOMWindow* window = GetInnerWindowForTarget();
    if (window) {
      window->SetHasGamepadEventListener();
    }
#endif
  }
  if (aTypeAtom && mTarget) {
    mTarget->EventListenerAdded(aTypeAtom);
  }
}

bool
nsEventListenerManager::IsDeviceType(uint32_t aType)
{
  switch (aType) {
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
nsEventListenerManager::EnableDevice(uint32_t aType)
{
  nsCOMPtr<nsPIDOMWindow> window = GetTargetAsInnerWindow();
  if (!window) {
    return;
  }

  switch (aType) {
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
nsEventListenerManager::DisableDevice(uint32_t aType)
{
  nsCOMPtr<nsPIDOMWindow> window = GetTargetAsInnerWindow();
  if (!window) {
    return;
  }

  switch (aType) {
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
nsEventListenerManager::RemoveEventListenerInternal(
                          const EventListenerHolder& aListener, 
                          uint32_t aType,
                          nsIAtom* aUserType,
                          const EventListenerFlags& aFlags,
                          bool aAllEvents)
{
  if (!aListener || !aType || mClearingListeners) {
    return;
  }

  nsListenerStruct* ls;

  uint32_t count = mListeners.Length();
  uint32_t typeCount = 0;
  bool deviceType = IsDeviceType(aType);
#ifdef MOZ_B2G
  bool timeChangeEvent = (aType == NS_MOZ_TIME_CHANGE_EVENT);
  bool networkEvent = (aType == NS_NETWORK_UPLOAD_EVENT ||
                       aType == NS_NETWORK_DOWNLOAD_EVENT);
#endif // MOZ_B2G

  for (uint32_t i = 0; i < count; ++i) {
    ls = &mListeners.ElementAt(i);
    if (EVENT_TYPE_EQUALS(ls, aType, aUserType, aAllEvents)) {
      ++typeCount;
      if (ls->mListener == aListener &&
          ls->mFlags.EqualsIgnoringTrustness(aFlags)) {
        nsRefPtr<nsEventListenerManager> kungFuDeathGrip = this;
        mListeners.RemoveElementAt(i);
        --count;
        mNoListenerForEvent = NS_EVENT_TYPE_NULL;
        mNoListenerForEventAtom = nullptr;
        if (mTarget && aUserType) {
          mTarget->EventListenerRemoved(aUserType);
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
    DisableDevice(aType);
#ifdef MOZ_B2G
  } else if (timeChangeEvent && typeCount == 0) {
    nsCOMPtr<nsPIDOMWindow> window = GetTargetAsInnerWindow();
    if (window) {
      window->DisableTimeChangeNotifications();
    }
  } else if (!aAllEvents && networkEvent && typeCount == 0) {
    nsCOMPtr<nsPIDOMWindow> window = GetTargetAsInnerWindow();
    if (window) {
      window->DisableNetworkEvent(aType);
    }
#endif // MOZ_B2G
  }
}

static inline bool
ListenerCanHandle(nsListenerStruct* aLs, nsEvent* aEvent)
{
  // This is slightly different from EVENT_TYPE_EQUALS in that it returns
  // true even when aEvent->message == NS_USER_DEFINED_EVENT and
  // aLs=>mEventType != NS_USER_DEFINED_EVENT as long as the atoms are the same
  return (aEvent->message == NS_USER_DEFINED_EVENT ?
    (aLs->mTypeAtom == aEvent->userType) :
    (aLs->mEventType == aEvent->message)) || aLs->mAllEvents;
}

void
nsEventListenerManager::AddEventListenerByType(const EventListenerHolder& aListener, 
                                               const nsAString& aType,
                                               const EventListenerFlags& aFlags)
{
  nsCOMPtr<nsIAtom> atom = do_GetAtom(NS_LITERAL_STRING("on") + aType);
  uint32_t type = nsContentUtils::GetEventId(atom);
  AddEventListenerInternal(aListener, type, atom, aFlags);
}

void
nsEventListenerManager::RemoveEventListenerByType(
                          const EventListenerHolder& aListener,
                          const nsAString& aType,
                          const EventListenerFlags& aFlags)
{
  nsCOMPtr<nsIAtom> atom = do_GetAtom(NS_LITERAL_STRING("on") + aType);
  uint32_t type = nsContentUtils::GetEventId(atom);
  RemoveEventListenerInternal(aListener, type, atom, aFlags);
}

nsListenerStruct*
nsEventListenerManager::FindEventHandler(uint32_t aEventType,
                                         nsIAtom* aTypeAtom)
{
  // Run through the listeners for this type and see if a script
  // listener is registered
  nsListenerStruct *ls;
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; ++i) {
    ls = &mListeners.ElementAt(i);
    if (ls->mListenerIsHandler &&
        EVENT_TYPE_EQUALS(ls, aEventType, aTypeAtom, false)) {
      return ls;
    }
  }
  return nullptr;
}

nsresult
nsEventListenerManager::SetEventHandlerInternal(nsIScriptContext *aContext,
                                                JS::Handle<JSObject*> aScopeObject,
                                                nsIAtom* aName,
                                                const nsEventHandler& aHandler,
                                                bool aPermitUntrustedEvents,
                                                nsListenerStruct **aListenerStruct)
{
  NS_ASSERTION((aContext && aScopeObject) || aHandler.HasEventHandler(),
               "Must have one or the other!");

  nsresult rv = NS_OK;
  uint32_t eventType = nsContentUtils::GetEventId(aName);
  nsListenerStruct* ls = FindEventHandler(eventType, aName);

  if (!ls) {
    // If we didn't find a script listener or no listeners existed
    // create and add a new one.
    EventListenerFlags flags;
    flags.mListenerIsJSListener = true;

    nsCOMPtr<nsIJSEventListener> scriptListener;
    rv = NS_NewJSEventListener(aContext, aScopeObject, mTarget, aName,
                               aHandler, getter_AddRefs(scriptListener));

    if (NS_SUCCEEDED(rv)) {
      EventListenerHolder holder(scriptListener);
      AddEventListenerInternal(holder, eventType, aName, flags, true);

      ls = FindEventHandler(eventType, aName);
    }
  } else {
    nsIJSEventListener* scriptListener = ls->GetJSListener();
    MOZ_ASSERT(scriptListener,
               "How can we have an event handler with no nsIJSEventListener?");

    bool same = scriptListener->GetHandler() == aHandler;
    // Possibly the same listener, but update still the context and scope.
    scriptListener->SetHandler(aHandler, aContext, aScopeObject);
    if (mTarget && !same) {
      mTarget->EventListenerRemoved(aName);
      mTarget->EventListenerAdded(aName);
    }
  }

  if (NS_SUCCEEDED(rv) && ls) {
    // Set flag to indicate possible need for compilation later
    ls->mHandlerIsString = !aHandler.HasEventHandler();
    if (aPermitUntrustedEvents) {
      ls->mFlags.mAllowUntrustedEvents = true;
    }

    *aListenerStruct = ls;
  } else {
    *aListenerStruct = nullptr;
  }

  return rv;
}

nsresult
nsEventListenerManager::SetEventHandler(nsIAtom *aName,
                                        const nsAString& aBody,
                                        uint32_t aLanguage,
                                        bool aDeferCompilation,
                                        bool aPermitUntrustedEvents)
{
  NS_PRECONDITION(aLanguage != nsIProgrammingLanguage::UNKNOWN,
                  "Must know the language for the script event listener");

  // |aPermitUntrustedEvents| is set to False for chrome - events
  // *generated* from an unknown source are not allowed.
  // However, for script languages with no 'sandbox', we want to reject
  // such scripts based on the source of their code, not just the source
  // of the event.
  if (aPermitUntrustedEvents && 
      aLanguage != nsIProgrammingLanguage::JAVASCRIPT) {
    NS_WARNING("Discarding non-JS event listener from untrusted source");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsINode> node(do_QueryInterface(mTarget));

  nsCOMPtr<nsIDocument> doc;

  nsCOMPtr<nsIScriptGlobalObject> global;

  if (node) {
    // Try to get context from doc
    // XXX sXBL/XBL2 issue -- do we really want the owner here?  What
    // if that's the XBL document?
    doc = node->OwnerDoc();
    MOZ_ASSERT(!doc->IsLoadedAsData(), "Should not get in here at all");

    // We want to allow compiling an event handler even in an unloaded
    // document, so use GetScopeObject here, not GetScriptHandlingObject.
    global = do_QueryInterface(doc->GetScopeObject());
  } else {
    nsCOMPtr<nsPIDOMWindow> win = GetTargetAsInnerWindow();
    if (win) {
      nsCOMPtr<nsIDOMDocument> domdoc;
      win->GetDocument(getter_AddRefs(domdoc));
      doc = do_QueryInterface(domdoc);
      global = do_QueryInterface(win);
    } else {
      global = do_QueryInterface(mTarget);
    }
  }

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
                                 0);
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

  JSAutoRequest ar(context->GetNativeContext());
  JS::Rooted<JSObject*> scope(context->GetNativeContext(),
                              global->GetGlobalJSObject());

  nsListenerStruct *ls;
  rv = SetEventHandlerInternal(context, scope, aName, nsEventHandler(),
                               aPermitUntrustedEvents, &ls);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aDeferCompilation) {
    return CompileEventHandlerInternal(ls, true, &aBody);
  }

  return NS_OK;
}

void
nsEventListenerManager::RemoveEventHandler(nsIAtom* aName)
{
  if (mClearingListeners) {
    return;
  }

  uint32_t eventType = nsContentUtils::GetEventId(aName);
  nsListenerStruct* ls = FindEventHandler(eventType, aName);

  if (ls) {
    mListeners.RemoveElementAt(uint32_t(ls - &mListeners.ElementAt(0)));
    mNoListenerForEvent = NS_EVENT_TYPE_NULL;
    mNoListenerForEventAtom = nullptr;
    if (mTarget) {
      mTarget->EventListenerRemoved(aName);
    }
  }
}

nsresult
nsEventListenerManager::CompileEventHandlerInternal(nsListenerStruct *aListenerStruct,
                                                    bool aNeedsCxPush,
                                                    const nsAString* aBody)
{
  NS_PRECONDITION(aListenerStruct->GetJSListener(),
                  "Why do we not have a JS listener?");
  NS_PRECONDITION(aListenerStruct->mHandlerIsString,
                  "Why are we compiling a non-string JS listener?");

  nsresult result = NS_OK;

  nsIJSEventListener *listener = aListenerStruct->GetJSListener();
  NS_ASSERTION(!listener->GetHandler().HasEventHandler(),
               "What is there to compile?");

  nsIScriptContext *context = listener->GetEventContext();
  JSContext *cx = context->GetNativeContext();
  JS::Rooted<JSObject*> handler(cx);

  nsCOMPtr<nsPIDOMWindow> win; // Will end up non-null if mTarget is a window

  nsCxPusher pusher;
  if (aNeedsCxPush) {
    pusher.Push(cx);
  }

  if (aListenerStruct->mHandlerIsString) {
    // OK, we didn't find an existing compiled event handler.  Flag us
    // as not a string so we don't keep trying to compile strings
    // which can't be compiled
    aListenerStruct->mHandlerIsString = false;

    // mTarget may not be an nsIContent if it's a window and we're
    // getting an inline event listener forwarded from <html:body> or
    // <html:frameset> or <xul:window> or the like.
    // XXX I don't like that we have to reference content from
    // here. The alternative is to store the event handler string on
    // the nsIJSEventListener itself, and that still doesn't address
    // the arg names issue.
    nsCOMPtr<nsIContent> content = do_QueryInterface(mTarget);
    nsAutoString handlerBody;
    const nsAString* body = aBody;
    if (content && !aBody) {
      nsIAtom* attrName = aListenerStruct->mTypeAtom;
      if (aListenerStruct->mTypeAtom == nsGkAtoms::onSVGLoad)
        attrName = nsGkAtoms::onload;
      else if (aListenerStruct->mTypeAtom == nsGkAtoms::onSVGUnload)
        attrName = nsGkAtoms::onunload;
      else if (aListenerStruct->mTypeAtom == nsGkAtoms::onSVGAbort)
        attrName = nsGkAtoms::onabort;
      else if (aListenerStruct->mTypeAtom == nsGkAtoms::onSVGError)
        attrName = nsGkAtoms::onerror;
      else if (aListenerStruct->mTypeAtom == nsGkAtoms::onSVGResize)
        attrName = nsGkAtoms::onresize;
      else if (aListenerStruct->mTypeAtom == nsGkAtoms::onSVGScroll)
        attrName = nsGkAtoms::onscroll;
      else if (aListenerStruct->mTypeAtom == nsGkAtoms::onSVGZoom)
        attrName = nsGkAtoms::onzoom;
      else if (aListenerStruct->mTypeAtom == nsGkAtoms::onbeginEvent)
        attrName = nsGkAtoms::onbegin;
      else if (aListenerStruct->mTypeAtom == nsGkAtoms::onrepeatEvent)
        attrName = nsGkAtoms::onrepeat;
      else if (aListenerStruct->mTypeAtom == nsGkAtoms::onendEvent)
        attrName = nsGkAtoms::onend;

      content->GetAttr(kNameSpaceID_None, attrName, handlerBody);
      body = &handlerBody;
    }

    uint32_t lineNo = 0;
    nsAutoCString url (NS_LITERAL_CSTRING("-moz-evil:lying-event-listener"));
    nsCOMPtr<nsIDocument> doc;
    if (content) {
      doc = content->OwnerDoc();
    } else {
      win = do_QueryInterface(mTarget);
      if (win) {
        doc = win->GetExtantDoc();
      }
    }

    if (doc) {
      nsIURI *uri = doc->GetDocumentURI();
      if (uri) {
        uri->GetSpec(url);
        lineNo = 1;
      }
    }

    uint32_t argCount;
    const char **argNames;
    // If no content, then just use kNameSpaceID_None for the
    // namespace ID.  In practice, it doesn't matter since SVG is
    // the only thing with weird arg names and SVG doesn't map event
    // listeners to the window.
    nsContentUtils::GetEventArgNames(content ?
                                       content->GetNameSpaceID() :
                                       kNameSpaceID_None,
                                     aListenerStruct->mTypeAtom,
                                     &argCount, &argNames);

    JSAutoCompartment ac(cx, context->GetNativeGlobal());
    JS::CompileOptions options(cx);
    options.setFileAndLine(url.get(), lineNo)
           .setVersion(SCRIPTVERSION_DEFAULT);

    JS::Rooted<JSObject*> handlerFun(cx);
    result = nsJSUtils::CompileFunction(cx, JS::NullPtr(), options,
                                        nsAtomCString(aListenerStruct->mTypeAtom),
                                        argCount, argNames, *body, handlerFun.address());
    NS_ENSURE_SUCCESS(result, result);
    handler = handlerFun;
    NS_ENSURE_TRUE(handler, NS_ERROR_FAILURE);
  }

  if (handler) {
    // Bind it
    JS::Rooted<JSObject*> boundHandler(cx);
    JS::Rooted<JSObject*> scope(cx, listener->GetEventScope());
    context->BindCompiledEventHandler(mTarget, scope, handler, &boundHandler);
    if (listener->EventName() == nsGkAtoms::onerror && win) {
      nsRefPtr<OnErrorEventHandlerNonNull> handlerCallback =
        new OnErrorEventHandlerNonNull(boundHandler);
      listener->SetHandler(handlerCallback);
    } else if (listener->EventName() == nsGkAtoms::onbeforeunload && win) {
      nsRefPtr<BeforeUnloadEventHandlerNonNull> handlerCallback =
        new BeforeUnloadEventHandlerNonNull(boundHandler);
      listener->SetHandler(handlerCallback);
    } else {
      nsRefPtr<EventHandlerNonNull> handlerCallback =
        new EventHandlerNonNull(boundHandler);
      listener->SetHandler(handlerCallback);
    }
  }

  return result;
}

nsresult
nsEventListenerManager::HandleEventSubType(nsListenerStruct* aListenerStruct,
                                           const EventListenerHolder& aListener,
                                           nsIDOMEvent* aDOMEvent,
                                           EventTarget* aCurrentTarget,
                                           nsCxPusher* aPusher)
{
  nsresult result = NS_OK;

  // If this is a script handler and we haven't yet
  // compiled the event handler itself
  if ((aListenerStruct->mListenerType == eJSEventListener) &&
      aListenerStruct->mHandlerIsString) {
    nsIJSEventListener *jslistener = aListenerStruct->GetJSListener();
    result = CompileEventHandlerInternal(aListenerStruct,
                                         jslistener->GetEventContext() !=
                                           aPusher->GetCurrentScriptContext(),
                                         nullptr);
  }

  if (NS_SUCCEEDED(result)) {
    nsAutoMicroTask mt;
    // nsIDOMEvent::currentTarget is set in nsEventDispatcher.
    if (aListener.HasWebIDLCallback()) {
      ErrorResult rv;
      aListener.GetWebIDLCallback()->
        HandleEvent(aCurrentTarget, *(aDOMEvent->InternalDOMEvent()), rv);
      result = rv.ErrorCode();
    } else {
      result = aListener.GetXPCOMCallback()->HandleEvent(aDOMEvent);
    }
  }

  return result;
}

/**
* Causes a check for event listeners and processing by them if they exist.
* @param an event listener
*/

void
nsEventListenerManager::HandleEventInternal(nsPresContext* aPresContext,
                                            nsEvent* aEvent,
                                            nsIDOMEvent** aDOMEvent,
                                            EventTarget* aCurrentTarget,
                                            nsEventStatus* aEventStatus,
                                            nsCxPusher* aPusher)
{
  //Set the value of the internal PreventDefault flag properly based on aEventStatus
  if (*aEventStatus == nsEventStatus_eConsumeNoDefault) {
    aEvent->mFlags.mDefaultPrevented = true;
  }

  nsAutoTObserverArray<nsListenerStruct, 2>::EndLimitedIterator iter(mListeners);
  nsAutoPopupStatePusher popupStatePusher(nsDOMEvent::GetEventPopupControlState(aEvent));
  bool hasListener = false;
  while (iter.HasMore()) {
    if (aEvent->mFlags.mImmediatePropagationStopped) {
      break;
    }
    nsListenerStruct* ls = &iter.GetNext();
    // Check that the phase is same in event and event listener.
    // Handle only trusted events, except when listener permits untrusted events.
    if (ListenerCanHandle(ls, aEvent)) {
      hasListener = true;
      if (ls->IsListening(aEvent) &&
          (aEvent->mFlags.mIsTrusted || ls->mFlags.mAllowUntrustedEvents)) {
        if (!*aDOMEvent) {
          // This is tiny bit slow, but happens only once per event.
          nsCOMPtr<mozilla::dom::EventTarget> et =
            do_QueryInterface(aEvent->originalTarget);
          nsEventDispatcher::CreateEvent(et, aPresContext,
                                         aEvent, EmptyString(), aDOMEvent);
        }
        if (*aDOMEvent) {
          if (!aEvent->currentTarget) {
            aEvent->currentTarget = aCurrentTarget->GetTargetForDOMEvent();
            if (!aEvent->currentTarget) {
              break;
            }
          }

          // Push the appropriate context. Note that we explicitly don't push a
          // context in the case that the listener is non-scripted, in which case
          // it's the native code's responsibility to push a context if it ever
          // enters JS. Ideally we'd do things this way for all scripted callbacks,
          // but that would involve a lot of changes and context pushing is going
          // away soon anyhow.
          //
          // NB: Since we're looping here, the no-RePush() case needs to actually be
          // a Pop(), otherwise we might end up with whatever was pushed in a
          // previous iteration.
          if (ls->mListenerType == eNativeListener) {
            aPusher->Pop();
          } else if (!aPusher->RePush(aCurrentTarget)) {
            continue;
          }

          EventListenerHolder kungFuDeathGrip(ls->mListener);
          if (NS_FAILED(HandleEventSubType(ls, ls->mListener, *aDOMEvent,
                                           aCurrentTarget, aPusher))) {
            aEvent->mFlags.mExceptionHasBeenRisen = true;
          }
        }
      }
    }
  }

  aEvent->currentTarget = nullptr;

  if (!hasListener) {
    mNoListenerForEvent = aEvent->message;
    mNoListenerForEventAtom = aEvent->userType;
  }

  if (aEvent->mFlags.mDefaultPrevented) {
    *aEventStatus = nsEventStatus_eConsumeNoDefault;
  }
}

void
nsEventListenerManager::Disconnect()
{
  mTarget = nullptr;
  RemoveAllListeners();
}

void
nsEventListenerManager::AddEventListener(const nsAString& aType,
                                         const EventListenerHolder& aListener,
                                         bool aUseCapture,
                                         bool aWantsUntrusted)
{
  EventListenerFlags flags;
  flags.mCapture = aUseCapture;
  flags.mAllowUntrustedEvents = aWantsUntrusted;
  return AddEventListenerByType(aListener, aType, flags);
}

void
nsEventListenerManager::RemoveEventListener(const nsAString& aType, 
                                            const EventListenerHolder& aListener, 
                                            bool aUseCapture)
{
  EventListenerFlags flags;
  flags.mCapture = aUseCapture;
  RemoveEventListenerByType(aListener, aType, flags);
}

void
nsEventListenerManager::AddListenerForAllEvents(nsIDOMEventListener* aListener,
                                                bool aUseCapture,
                                                bool aWantsUntrusted,
                                                bool aSystemEventGroup)
{
  EventListenerFlags flags;
  flags.mCapture = aUseCapture;
  flags.mAllowUntrustedEvents = aWantsUntrusted;
  flags.mInSystemGroup = aSystemEventGroup;
  EventListenerHolder holder(aListener);
  AddEventListenerInternal(holder, NS_EVENT_TYPE_ALL, nullptr, flags,
                           false, true);
}

void
nsEventListenerManager::RemoveListenerForAllEvents(nsIDOMEventListener* aListener, 
                                                   bool aUseCapture,
                                                   bool aSystemEventGroup)
{
  EventListenerFlags flags;
  flags.mCapture = aUseCapture;
  flags.mInSystemGroup = aSystemEventGroup;
  EventListenerHolder holder(aListener);
  RemoveEventListenerInternal(holder, NS_EVENT_TYPE_ALL, nullptr, flags,
                              true);
}

bool
nsEventListenerManager::HasMutationListeners()
{
  if (mMayHaveMutationListeners) {
    uint32_t count = mListeners.Length();
    for (uint32_t i = 0; i < count; ++i) {
      nsListenerStruct* ls = &mListeners.ElementAt(i);
      if (ls->mEventType >= NS_MUTATION_START &&
          ls->mEventType <= NS_MUTATION_END) {
        return true;
      }
    }
  }

  return false;
}

uint32_t
nsEventListenerManager::MutationListenerBits()
{
  uint32_t bits = 0;
  if (mMayHaveMutationListeners) {
    uint32_t count = mListeners.Length();
    for (uint32_t i = 0; i < count; ++i) {
      nsListenerStruct* ls = &mListeners.ElementAt(i);
      if (ls->mEventType >= NS_MUTATION_START &&
          ls->mEventType <= NS_MUTATION_END) {
        if (ls->mEventType == NS_MUTATION_SUBTREEMODIFIED) {
          return kAllMutationBits;
        }
        bits |= MutationBitForEventType(ls->mEventType);
      }
    }
  }
  return bits;
}

bool
nsEventListenerManager::HasListenersFor(const nsAString& aEventName)
{
  nsCOMPtr<nsIAtom> atom = do_GetAtom(NS_LITERAL_STRING("on") + aEventName);
  return HasListenersFor(atom);
}

bool
nsEventListenerManager::HasListenersFor(nsIAtom* aEventNameWithOn)
{
#ifdef DEBUG
  nsAutoString name;
  aEventNameWithOn->ToString(name);
#endif
  NS_ASSERTION(StringBeginsWith(name, NS_LITERAL_STRING("on")),
               "Event name does not start with 'on'");
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; ++i) {
    nsListenerStruct* ls = &mListeners.ElementAt(i);
    if (ls->mTypeAtom == aEventNameWithOn) {
      return true;
    }
  }
  return false;
}

bool
nsEventListenerManager::HasListeners()
{
  return !mListeners.IsEmpty();
}

nsresult
nsEventListenerManager::GetListenerInfo(nsCOMArray<nsIEventListenerInfo>* aList)
{
  nsCOMPtr<EventTarget> target = do_QueryInterface(mTarget);
  NS_ENSURE_STATE(target);
  aList->Clear();
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; ++i) {
    const nsListenerStruct& ls = mListeners.ElementAt(i);
    // If this is a script handler and we haven't yet
    // compiled the event handler itself go ahead and compile it
    if ((ls.mListenerType == eJSEventListener) && ls.mHandlerIsString) {
      CompileEventHandlerInternal(const_cast<nsListenerStruct*>(&ls),
                                  true, nullptr);
    }
    nsAutoString eventType;
    if (ls.mAllEvents) {
      eventType.SetIsVoid(true);
    } else {
      eventType.Assign(Substring(nsDependentAtomString(ls.mTypeAtom), 2));
    }
    // EventListenerInfo is defined in XPCOM, so we have to go ahead
    // and convert to an XPCOM callback here...
    nsRefPtr<nsEventListenerInfo> info =
      new nsEventListenerInfo(eventType, ls.mListener.ToXPCOMCallback(),
                              ls.mFlags.mCapture,
                              ls.mFlags.mAllowUntrustedEvents,
                              ls.mFlags.mInSystemGroup);
    aList->AppendObject(info);
  }
  return NS_OK;
}

bool
nsEventListenerManager::HasUnloadListeners()
{
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; ++i) {
    nsListenerStruct* ls = &mListeners.ElementAt(i);
    if (ls->mEventType == NS_PAGE_UNLOAD ||
        ls->mEventType == NS_BEFORE_PAGE_UNLOAD) {
      return true;
    }
  }
  return false;
}

nsresult
nsEventListenerManager::SetEventHandler(nsIAtom* aEventName,
                                        EventHandlerNonNull* aHandler)
{
  if (!aHandler) {
    RemoveEventHandler(aEventName);
    return NS_OK;
  }

  // Untrusted events are always permitted for non-chrome script
  // handlers.
  nsListenerStruct *ignored;
  return SetEventHandlerInternal(nullptr, JS::NullPtr(), aEventName,
                                 nsEventHandler(aHandler),
                                 !nsContentUtils::IsCallerChrome(), &ignored);
}

nsresult
nsEventListenerManager::SetEventHandler(OnErrorEventHandlerNonNull* aHandler)
{
  if (!aHandler) {
    RemoveEventHandler(nsGkAtoms::onerror);
    return NS_OK;
  }

  // Untrusted events are always permitted for non-chrome script
  // handlers.
  nsListenerStruct *ignored;
  return SetEventHandlerInternal(nullptr, JS::NullPtr(), nsGkAtoms::onerror,
                                 nsEventHandler(aHandler),
                                 !nsContentUtils::IsCallerChrome(), &ignored);
}

nsresult
nsEventListenerManager::SetEventHandler(BeforeUnloadEventHandlerNonNull* aHandler)
{
  if (!aHandler) {
    RemoveEventHandler(nsGkAtoms::onbeforeunload);
    return NS_OK;
  }

  // Untrusted events are always permitted for non-chrome script
  // handlers.
  nsListenerStruct *ignored;
  return SetEventHandlerInternal(nullptr, JS::NullPtr(), nsGkAtoms::onbeforeunload,
                                 nsEventHandler(aHandler),
                                 !nsContentUtils::IsCallerChrome(), &ignored);
}

const nsEventHandler*
nsEventListenerManager::GetEventHandlerInternal(nsIAtom *aEventName)
{
  uint32_t eventType = nsContentUtils::GetEventId(aEventName);
  nsListenerStruct* ls = FindEventHandler(eventType, aEventName);

  if (!ls) {
    return nullptr;
  }

  nsIJSEventListener *listener = ls->GetJSListener();
    
  if (ls->mHandlerIsString) {
    CompileEventHandlerInternal(ls, true, nullptr);
  }

  const nsEventHandler& handler = listener->GetHandler();
  if (handler.HasEventHandler()) {
    return &handler;
  }

  return nullptr;
}

size_t
nsEventListenerManager::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf)
  const
{
  size_t n = aMallocSizeOf(this);
  n += mListeners.SizeOfExcludingThis(aMallocSizeOf);
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; ++i) {
    nsIJSEventListener* jsl = mListeners.ElementAt(i).GetJSListener();
    if (jsl) {
      n += jsl->SizeOfIncludingThis(aMallocSizeOf);
    }
  }
  return n;
}

void
nsEventListenerManager::MarkForCC()
{
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; ++i) {
    const nsListenerStruct& ls = mListeners.ElementAt(i);
    nsIJSEventListener* jsl = ls.GetJSListener();
    if (jsl) {
      if (jsl->GetHandler().HasEventHandler()) {
        xpc_UnmarkGrayObject(jsl->GetHandler().Ptr()->Callable());
      }
      xpc_UnmarkGrayObject(jsl->GetEventScope());
    } else if (ls.mListenerType == eWrappedJSListener) {
      xpc_TryUnmarkWrappedGrayObject(ls.mListener.GetXPCOMCallback());
    } else if (ls.mListenerType == eWebIDLListener) {
      // Callback() unmarks gray
      ls.mListener.GetWebIDLCallback()->Callback();
    }
  }
  if (mRefCnt.IsPurple()) {
    mRefCnt.RemovePurple();
  }
}

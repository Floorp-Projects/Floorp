/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsXBLPrototypeHandler.h"
#include "nsXBLWindowKeyHandler.h"
#include "nsIContent.h"
#include "nsAtom.h"
#include "nsIDOMKeyEvent.h"
#include "nsXBLService.h"
#include "nsIServiceManager.h"
#include "nsGkAtoms.h"
#include "nsXBLDocumentInfo.h"
#include "nsIDOMElement.h"
#include "nsFocusManager.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsContentUtils.h"
#include "nsXBLPrototypeBinding.h"
#include "nsPIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIDOMDocument.h"
#include "nsISelectionController.h"
#include "nsIPresShell.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/Move.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/layers/KeyboardMap.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;

class nsXBLSpecialDocInfo : public nsIObserver
{
public:
  RefPtr<nsXBLDocumentInfo> mHTMLBindings;
  RefPtr<nsXBLDocumentInfo> mUserHTMLBindings;

  static const char sHTMLBindingStr[];
  static const char sUserHTMLBindingStr[];

  bool mInitialized;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  void LoadDocInfo();
  void GetAllHandlers(const char* aType,
                      nsXBLPrototypeHandler** handler,
                      nsXBLPrototypeHandler** userHandler);
  void GetHandlers(nsXBLDocumentInfo* aInfo,
                   const nsACString& aRef,
                   nsXBLPrototypeHandler** aResult);

  nsXBLSpecialDocInfo() : mInitialized(false) {}

protected:
  virtual ~nsXBLSpecialDocInfo() {}

};

const char nsXBLSpecialDocInfo::sHTMLBindingStr[] =
  "chrome://global/content/platformHTMLBindings.xml";

NS_IMPL_ISUPPORTS(nsXBLSpecialDocInfo, nsIObserver)

NS_IMETHODIMP
nsXBLSpecialDocInfo::Observe(nsISupports* aSubject,
                             const char* aTopic,
                             const char16_t* aData)
{
  MOZ_ASSERT(!strcmp(aTopic, "xpcom-shutdown"), "wrong topic");

  // On shutdown, clear our fields to avoid an extra cycle collection.
  mHTMLBindings = nullptr;
  mUserHTMLBindings = nullptr;
  mInitialized = false;
  nsContentUtils::UnregisterShutdownObserver(this);

  return NS_OK;
}

void nsXBLSpecialDocInfo::LoadDocInfo()
{
  if (mInitialized)
    return;
  mInitialized = true;
  nsContentUtils::RegisterShutdownObserver(this);

  nsXBLService* xblService = nsXBLService::GetInstance();
  if (!xblService)
    return;

  // Obtain the platform doc info
  nsCOMPtr<nsIURI> bindingURI;
  NS_NewURI(getter_AddRefs(bindingURI), sHTMLBindingStr);
  if (!bindingURI) {
    return;
  }
  xblService->LoadBindingDocumentInfo(nullptr, nullptr,
                                      bindingURI,
                                      nullptr,
                                      true,
                                      getter_AddRefs(mHTMLBindings));
}

//
// GetHandlers
//
//
void
nsXBLSpecialDocInfo::GetHandlers(nsXBLDocumentInfo* aInfo,
                                 const nsACString& aRef,
                                 nsXBLPrototypeHandler** aResult)
{
  nsXBLPrototypeBinding* binding = aInfo->GetPrototypeBinding(aRef);

  NS_ASSERTION(binding, "No binding found for the XBL window key handler.");
  if (!binding)
    return;

  *aResult = binding->GetPrototypeHandlers();
}

void
nsXBLSpecialDocInfo::GetAllHandlers(const char* aType,
                                    nsXBLPrototypeHandler** aHandler,
                                    nsXBLPrototypeHandler** aUserHandler)
{
  if (mUserHTMLBindings) {
    nsAutoCString type(aType);
    type.AppendLiteral("User");
    GetHandlers(mUserHTMLBindings, type, aUserHandler);
  }
  if (mHTMLBindings) {
    GetHandlers(mHTMLBindings, nsDependentCString(aType), aHandler);
  }
}

// Init statics
static StaticRefPtr<nsXBLSpecialDocInfo> sXBLSpecialDocInfo;
uint32_t nsXBLWindowKeyHandler::sRefCnt = 0;

/* static */ void
nsXBLWindowKeyHandler::EnsureSpecialDocInfo()
{
  if (!sXBLSpecialDocInfo) {
    sXBLSpecialDocInfo = new nsXBLSpecialDocInfo();
  }
  sXBLSpecialDocInfo->LoadDocInfo();
}

nsXBLWindowKeyHandler::nsXBLWindowKeyHandler(nsIDOMElement* aElement,
                                             EventTarget* aTarget)
  : mTarget(aTarget),
    mHandler(nullptr),
    mUserHandler(nullptr)
{
  mWeakPtrForElement = do_GetWeakReference(aElement);
  ++sRefCnt;
}

nsXBLWindowKeyHandler::~nsXBLWindowKeyHandler()
{
  // If mWeakPtrForElement is non-null, we created a prototype handler.
  if (mWeakPtrForElement)
    delete mHandler;

  --sRefCnt;
  if (!sRefCnt) {
    sXBLSpecialDocInfo = nullptr;
  }
}

NS_IMPL_ISUPPORTS(nsXBLWindowKeyHandler,
                  nsIDOMEventListener)

static void
BuildHandlerChain(nsIContent* aContent, nsXBLPrototypeHandler** aResult)
{
  *aResult = nullptr;

  // Since we chain each handler onto the next handler,
  // we'll enumerate them here in reverse so that when we
  // walk the chain they'll come out in the original order
  for (nsIContent* key = aContent->GetLastChild();
       key;
       key = key->GetPreviousSibling()) {
    if (!key->NodeInfo()->Equals(nsGkAtoms::key, kNameSpaceID_XUL)) {
      continue;
    }

    Element* keyElement = key->AsElement();
    // Check whether the key element has empty value at key/char attribute.
    // Such element is used by localizers for alternative shortcut key
    // definition on the locale. See bug 426501.
    nsAutoString valKey, valCharCode, valKeyCode;
    bool attrExists =
      keyElement->GetAttr(kNameSpaceID_None, nsGkAtoms::key, valKey) ||
      keyElement->GetAttr(kNameSpaceID_None, nsGkAtoms::charcode, valCharCode) ||
      keyElement->GetAttr(kNameSpaceID_None, nsGkAtoms::keycode, valKeyCode);
    if (attrExists &&
        valKey.IsEmpty() && valCharCode.IsEmpty() && valKeyCode.IsEmpty())
      continue;

    // reserved="pref" is the default for <key> elements.
    XBLReservedKey reserved = XBLReservedKey_Unset;
    if (keyElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::reserved,
                                nsGkAtoms::_true, eCaseMatters)) {
      reserved = XBLReservedKey_True;
    } else if (keyElement->AttrValueIs(kNameSpaceID_None,
                                       nsGkAtoms::reserved,
                                       nsGkAtoms::_false, eCaseMatters)) {
      reserved = XBLReservedKey_False;
    }

    nsXBLPrototypeHandler* handler =
      new nsXBLPrototypeHandler(keyElement, reserved);

    handler->SetNextHandler(*aResult);
    *aResult = handler;
  }
}

//
// EnsureHandlers
//
// Lazily load the XBL handlers. Overridden to handle being attached
// to a particular element rather than the document
//
nsresult
nsXBLWindowKeyHandler::EnsureHandlers()
{
  nsCOMPtr<Element> el = GetElement();
  NS_ENSURE_STATE(!mWeakPtrForElement || el);
  if (el) {
    // We are actually a XUL <keyset>.
    if (mHandler)
      return NS_OK;

    nsCOMPtr<nsIContent> content(do_QueryInterface(el));
    BuildHandlerChain(content, &mHandler);
  } else { // We are an XBL file of handlers.
    EnsureSpecialDocInfo();

    // Now determine which handlers we should be using.
    if (IsHTMLEditableFieldFocused()) {
      sXBLSpecialDocInfo->GetAllHandlers("editor", &mHandler, &mUserHandler);
    }
    else {
      sXBLSpecialDocInfo->GetAllHandlers("browser", &mHandler, &mUserHandler);
    }
  }

  return NS_OK;
}

nsresult
nsXBLWindowKeyHandler::WalkHandlers(nsIDOMKeyEvent* aKeyEvent, nsAtom* aEventType)
{
  bool prevent;
  aKeyEvent->AsEvent()->GetDefaultPrevented(&prevent);
  if (prevent)
    return NS_OK;

  bool trustedEvent = false;
  // Don't process the event if it was not dispatched from a trusted source
  aKeyEvent->AsEvent()->GetIsTrusted(&trustedEvent);

  if (!trustedEvent)
    return NS_OK;

  nsresult rv = EnsureHandlers();
  NS_ENSURE_SUCCESS(rv, rv);

  bool isDisabled;
  nsCOMPtr<Element> el = GetElement(&isDisabled);
  if (!el) {
    if (mUserHandler) {
      WalkHandlersInternal(aKeyEvent, aEventType, mUserHandler, true);
      aKeyEvent->AsEvent()->GetDefaultPrevented(&prevent);
      if (prevent)
        return NS_OK; // Handled by the user bindings. Our work here is done.
    }
  }

  // skip keysets that are disabled
  if (el && isDisabled) {
    return NS_OK;
  }

  WalkHandlersInternal(aKeyEvent, aEventType, mHandler, true);

  return NS_OK;
}

void
nsXBLWindowKeyHandler::InstallKeyboardEventListenersTo(
                         EventListenerManager* aEventListenerManager)
{
  // For marking each keyboard event as if it's reserved by chrome,
  // nsXBLWindowKeyHandlers need to listen each keyboard events before
  // web contents.
  aEventListenerManager->AddEventListenerByType(
                           this, NS_LITERAL_STRING("keydown"),
                           TrustedEventsAtCapture());
  aEventListenerManager->AddEventListenerByType(
                           this, NS_LITERAL_STRING("keyup"),
                           TrustedEventsAtCapture());
  aEventListenerManager->AddEventListenerByType(
                           this, NS_LITERAL_STRING("keypress"),
                           TrustedEventsAtCapture());
  aEventListenerManager->AddEventListenerByType(
                           this, NS_LITERAL_STRING("mozkeydownonplugin"),
                           TrustedEventsAtCapture());
  aEventListenerManager->AddEventListenerByType(
                           this, NS_LITERAL_STRING("mozkeyuponplugin"),
                           TrustedEventsAtCapture());

  // For reducing the IPC cost, preventing to dispatch reserved keyboard
  // events into the content process.
  aEventListenerManager->AddEventListenerByType(
                           this, NS_LITERAL_STRING("keydown"),
                           TrustedEventsAtSystemGroupCapture());
  aEventListenerManager->AddEventListenerByType(
                           this, NS_LITERAL_STRING("keyup"),
                           TrustedEventsAtSystemGroupCapture());
  aEventListenerManager->AddEventListenerByType(
                           this, NS_LITERAL_STRING("keypress"),
                           TrustedEventsAtSystemGroupCapture());
  aEventListenerManager->AddEventListenerByType(
                           this, NS_LITERAL_STRING("mozkeydownonplugin"),
                           TrustedEventsAtSystemGroupCapture());
  aEventListenerManager->AddEventListenerByType(
                           this, NS_LITERAL_STRING("mozkeyuponplugin"),
                           TrustedEventsAtSystemGroupCapture());

  // Handle keyboard events in bubbling phase of the system event group.
  aEventListenerManager->AddEventListenerByType(
                           this, NS_LITERAL_STRING("keydown"),
                           TrustedEventsAtSystemGroupBubble());
  aEventListenerManager->AddEventListenerByType(
                           this, NS_LITERAL_STRING("keyup"),
                           TrustedEventsAtSystemGroupBubble());
  aEventListenerManager->AddEventListenerByType(
                           this, NS_LITERAL_STRING("keypress"),
                           TrustedEventsAtSystemGroupBubble());
  aEventListenerManager->AddEventListenerByType(
                           this, NS_LITERAL_STRING("mozkeydownonplugin"),
                           TrustedEventsAtSystemGroupBubble());
  aEventListenerManager->AddEventListenerByType(
                           this, NS_LITERAL_STRING("mozkeyuponplugin"),
                           TrustedEventsAtSystemGroupBubble());
}

void
nsXBLWindowKeyHandler::RemoveKeyboardEventListenersFrom(
                         EventListenerManager* aEventListenerManager)
{
  aEventListenerManager->RemoveEventListenerByType(
                           this, NS_LITERAL_STRING("keydown"),
                           TrustedEventsAtCapture());
  aEventListenerManager->RemoveEventListenerByType(
                           this, NS_LITERAL_STRING("keyup"),
                           TrustedEventsAtCapture());
  aEventListenerManager->RemoveEventListenerByType(
                           this, NS_LITERAL_STRING("keypress"),
                           TrustedEventsAtCapture());
  aEventListenerManager->RemoveEventListenerByType(
                           this, NS_LITERAL_STRING("mozkeydownonplugin"),
                           TrustedEventsAtCapture());
  aEventListenerManager->RemoveEventListenerByType(
                           this, NS_LITERAL_STRING("mozkeyuponplugin"),
                           TrustedEventsAtCapture());

  aEventListenerManager->RemoveEventListenerByType(
                           this, NS_LITERAL_STRING("keydown"),
                           TrustedEventsAtSystemGroupCapture());
  aEventListenerManager->RemoveEventListenerByType(
                           this, NS_LITERAL_STRING("keyup"),
                           TrustedEventsAtSystemGroupCapture());
  aEventListenerManager->RemoveEventListenerByType(
                           this, NS_LITERAL_STRING("keypress"),
                           TrustedEventsAtSystemGroupCapture());
  aEventListenerManager->RemoveEventListenerByType(
                           this, NS_LITERAL_STRING("mozkeydownonplugin"),
                           TrustedEventsAtSystemGroupCapture());
  aEventListenerManager->RemoveEventListenerByType(
                           this, NS_LITERAL_STRING("mozkeyuponplugin"),
                           TrustedEventsAtSystemGroupCapture());

  aEventListenerManager->RemoveEventListenerByType(
                           this, NS_LITERAL_STRING("keydown"),
                           TrustedEventsAtSystemGroupBubble());
  aEventListenerManager->RemoveEventListenerByType(
                           this, NS_LITERAL_STRING("keyup"),
                           TrustedEventsAtSystemGroupBubble());
  aEventListenerManager->RemoveEventListenerByType(
                           this, NS_LITERAL_STRING("keypress"),
                           TrustedEventsAtSystemGroupBubble());
  aEventListenerManager->RemoveEventListenerByType(
                           this, NS_LITERAL_STRING("mozkeydownonplugin"),
                           TrustedEventsAtSystemGroupBubble());
  aEventListenerManager->RemoveEventListenerByType(
                           this, NS_LITERAL_STRING("mozkeyuponplugin"),
                           TrustedEventsAtSystemGroupBubble());
}

/* static */ KeyboardMap
nsXBLWindowKeyHandler::CollectKeyboardShortcuts()
{
  // Load the XBL handlers
  EnsureSpecialDocInfo();

  nsXBLPrototypeHandler* handlers = nullptr;
  nsXBLPrototypeHandler* userHandlers = nullptr;
  sXBLSpecialDocInfo->GetAllHandlers("browser", &handlers, &userHandlers);

  // Convert the handlers into keyboard shortcuts, using an AutoTArray with
  // the maximum amount of shortcuts used on any platform to minimize allocations
  AutoTArray<KeyboardShortcut, 48> shortcuts;

  // Append keyboard shortcuts for hardcoded actions like tab
  KeyboardShortcut::AppendHardcodedShortcuts(shortcuts);

  for (nsXBLPrototypeHandler* handler = handlers;
       handler;
       handler = handler->GetNextHandler()) {
    KeyboardShortcut shortcut;
    if (handler->TryConvertToKeyboardShortcut(&shortcut)) {
      shortcuts.AppendElement(shortcut);
    }
  }

  for (nsXBLPrototypeHandler* handler = userHandlers;
       handler;
       handler = handler->GetNextHandler()) {
    KeyboardShortcut shortcut;
    if (handler->TryConvertToKeyboardShortcut(&shortcut)) {
      shortcuts.AppendElement(shortcut);
    }
  }

  return KeyboardMap(mozilla::Move(shortcuts));
}

nsAtom*
nsXBLWindowKeyHandler::ConvertEventToDOMEventType(
                         const WidgetKeyboardEvent& aWidgetKeyboardEvent) const
{
  if (aWidgetKeyboardEvent.IsKeyDownOrKeyDownOnPlugin()) {
    return nsGkAtoms::keydown;
  }
  if (aWidgetKeyboardEvent.IsKeyUpOrKeyUpOnPlugin()) {
    return nsGkAtoms::keyup;
  }
  if (aWidgetKeyboardEvent.mMessage == eKeyPress) {
    return nsGkAtoms::keypress;
  }
  MOZ_ASSERT_UNREACHABLE(
    "All event messages which this instance listens to should be handled");
  return nullptr;
}

NS_IMETHODIMP
nsXBLWindowKeyHandler::HandleEvent(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMKeyEvent> keyEvent(do_QueryInterface(aEvent));
  NS_ENSURE_TRUE(keyEvent, NS_ERROR_INVALID_ARG);

  uint16_t eventPhase;
  aEvent->GetEventPhase(&eventPhase);
  if (eventPhase == nsIDOMEvent::CAPTURING_PHASE) {
    if (aEvent->WidgetEventPtr()->mFlags.mInSystemGroup) {
      HandleEventOnCaptureInSystemEventGroup(keyEvent);
    } else {
      HandleEventOnCaptureInDefaultEventGroup(keyEvent);
    }
    return NS_OK;
  }

  WidgetKeyboardEvent* widgetKeyboardEvent =
    aEvent->WidgetEventPtr()->AsKeyboardEvent();
  if (widgetKeyboardEvent->IsKeyEventOnPlugin()) {
    // key events on plugin shouldn't execute shortcut key handlers which are
    // not reserved.
    if (!widgetKeyboardEvent->IsReservedByChrome()) {
      return NS_OK;
    }

    // If the event is untrusted event or was already consumed, do nothing.
    if (!widgetKeyboardEvent->IsTrusted() ||
        widgetKeyboardEvent->DefaultPrevented()) {
      return NS_OK;
    }

    // XXX Don't check isReserved here because even if the handler in this
    //     instance isn't reserved but another instance reserves the key
    //     combination, it will be executed when the event is normal keyboard
    //     events...
    bool isReserved = false;
    if (!HasHandlerForEvent(keyEvent, &isReserved)) {
      return NS_OK;
    }
  }

  // If this event was handled by APZ then don't do the default action, and
  // preventDefault to prevent any other listeners from handling the event.
  if (widgetKeyboardEvent->mFlags.mHandledByAPZ) {
    aEvent->PreventDefault();
    return NS_OK;
  }

  RefPtr<nsAtom> eventTypeAtom =
    ConvertEventToDOMEventType(*widgetKeyboardEvent);
  return WalkHandlers(keyEvent, eventTypeAtom);
}

void
nsXBLWindowKeyHandler::HandleEventOnCaptureInDefaultEventGroup(
                         nsIDOMKeyEvent* aEvent)
{
  WidgetKeyboardEvent* widgetKeyboardEvent =
    aEvent->AsEvent()->WidgetEventPtr()->AsKeyboardEvent();

  if (widgetKeyboardEvent->IsReservedByChrome()) {
    return;
  }

  bool isReserved = false;
  if (HasHandlerForEvent(aEvent, &isReserved) && isReserved) {
    widgetKeyboardEvent->MarkAsReservedByChrome();
  }
}

void
nsXBLWindowKeyHandler::HandleEventOnCaptureInSystemEventGroup(
                         nsIDOMKeyEvent* aEvent)
{
  WidgetKeyboardEvent* widgetEvent =
    aEvent->AsEvent()->WidgetEventPtr()->AsKeyboardEvent();

  // If the event won't be sent to remote process, this listener needs to do
  // nothing.
  if (widgetEvent->mFlags.mOnlySystemGroupDispatchInContent ||
      !widgetEvent->WillBeSentToRemoteProcess()) {
    return;
  }

  if (!HasHandlerForEvent(aEvent)) {
    return;
  }

  // If this event wasn't marked as IsCrossProcessForwardingStopped,
  // yet, it means it wasn't processed by content. We'll not call any
  // of the handlers at this moment, and will wait the reply event.
  // So, stop immediate propagation in this event first, then, mark it as
  // waiting reply from remote process.  Finally, when this process receives
  // a reply from the remote process, it should be dispatched into this
  // DOM tree again.
  widgetEvent->StopImmediatePropagation();
  widgetEvent->MarkAsWaitingReplyFromRemoteProcess();
}

bool
nsXBLWindowKeyHandler::IsHTMLEditableFieldFocused()
{
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm)
    return false;

  nsCOMPtr<mozIDOMWindowProxy> focusedWindow;
  fm->GetFocusedWindow(getter_AddRefs(focusedWindow));
  if (!focusedWindow)
    return false;

  auto* piwin = nsPIDOMWindowOuter::From(focusedWindow);
  nsIDocShell *docShell = piwin->GetDocShell();
  if (!docShell) {
    return false;
  }

  RefPtr<HTMLEditor> htmlEditor = docShell->GetHTMLEditor();
  if (!htmlEditor) {
    return false;
  }

  nsCOMPtr<nsIDocument> doc = htmlEditor->GetDocument();
  if (doc->HasFlag(NODE_IS_EDITABLE)) {
    // Don't need to perform any checks in designMode documents.
    return true;
  }

  nsCOMPtr<nsIDOMElement> focusedElement;
  fm->GetFocusedElement(getter_AddRefs(focusedElement));
  nsCOMPtr<nsINode> focusedNode = do_QueryInterface(focusedElement);
  if (focusedNode) {
    // If there is a focused element, make sure it's in the active editing host.
    // Note that GetActiveEditingHost finds the current editing host based on
    // the document's selection.  Even though the document selection is usually
    // collapsed to where the focus is, but the page may modify the selection
    // without our knowledge, in which case this check will do something useful.
    nsCOMPtr<Element> activeEditingHost = htmlEditor->GetActiveEditingHost();
    if (!activeEditingHost) {
      return false;
    }
    return nsContentUtils::ContentIsDescendantOf(focusedNode, activeEditingHost);
  }

  return false;
}

//
// WalkHandlersInternal and WalkHandlersAndExecute
//
// Given a particular DOM event and a pointer to the first handler in the list,
// scan through the list to find something to handle the event. If aExecute = true,
// the handler will be executed; otherwise just return an answer telling if a handler
// for that event was found.
//
bool
nsXBLWindowKeyHandler::WalkHandlersInternal(nsIDOMKeyEvent* aKeyEvent,
                                            nsAtom* aEventType,
                                            nsXBLPrototypeHandler* aHandler,
                                            bool aExecute,
                                            bool* aOutReservedForChrome)
{
  WidgetKeyboardEvent* nativeKeyboardEvent =
    aKeyEvent->AsEvent()->WidgetEventPtr()->AsKeyboardEvent();
  MOZ_ASSERT(nativeKeyboardEvent);

  AutoShortcutKeyCandidateArray shortcutKeys;
  nativeKeyboardEvent->GetShortcutKeyCandidates(shortcutKeys);

  if (shortcutKeys.IsEmpty()) {
    return WalkHandlersAndExecute(aKeyEvent, aEventType, aHandler,
                                  0, IgnoreModifierState(),
                                  aExecute, aOutReservedForChrome);
  }

  for (uint32_t i = 0; i < shortcutKeys.Length(); ++i) {
    ShortcutKeyCandidate& key = shortcutKeys[i];
    IgnoreModifierState ignoreModifierState;
    ignoreModifierState.mShift = key.mIgnoreShift;
    if (WalkHandlersAndExecute(aKeyEvent, aEventType, aHandler,
                               key.mCharCode, ignoreModifierState,
                               aExecute, aOutReservedForChrome)) {
      return true;
    }
  }
  return false;
}

bool
nsXBLWindowKeyHandler::WalkHandlersAndExecute(
                         nsIDOMKeyEvent* aKeyEvent,
                         nsAtom* aEventType,
                         nsXBLPrototypeHandler* aFirstHandler,
                         uint32_t aCharCode,
                         const IgnoreModifierState& aIgnoreModifierState,
                         bool aExecute,
                         bool* aOutReservedForChrome)
{
  if (aOutReservedForChrome) {
    *aOutReservedForChrome = false;
  }

  WidgetKeyboardEvent* widgetKeyboardEvent =
    aKeyEvent->AsEvent()->WidgetEventPtr()->AsKeyboardEvent();
  if (NS_WARN_IF(!widgetKeyboardEvent)) {
    return false;
  }

  // Try all of the handlers until we find one that matches the event.
  for (nsXBLPrototypeHandler* handler = aFirstHandler;
       handler;
       handler = handler->GetNextHandler()) {
    bool stopped = aKeyEvent->AsEvent()->IsDispatchStopped();
    if (stopped) {
      // The event is finished, don't execute any more handlers
      return false;
    }

    if (aExecute) {
      // If the event is eKeyDownOnPlugin, it should execute either keydown
      // handler or keypress handler because eKeyDownOnPlugin events are
      // never followed by keypress events.
      if (widgetKeyboardEvent->mMessage == eKeyDownOnPlugin) {
        if (!handler->EventTypeEquals(nsGkAtoms::keydown) &&
            !handler->EventTypeEquals(nsGkAtoms::keypress)) {
          continue;
        }
      // The other event types should exactly be matched with the handler's
      // event type.
      } else if (!handler->EventTypeEquals(aEventType)) {
        continue;
      }
    } else {
      if (handler->EventTypeEquals(nsGkAtoms::keypress)) {
        // If the handler is a keypress event handler, we also need to check
        // if coming keydown event is a preceding event of reserved key
        // combination because if default action of a keydown event is
        // prevented, following keypress event won't be fired.  However, if
        // following keypress event is reserved, we shouldn't allow web
        // contents to prevent the default of the preceding keydown event.
        if (aEventType != nsGkAtoms::keydown &&
            aEventType != nsGkAtoms::keypress) {
          continue;
        }
      } else if (!handler->EventTypeEquals(aEventType)) {
        // Otherwise, aEventType should exactly be matched.
        continue;
      }
    }

    // Check if the keyboard event *may* execute the handler.
    if (!handler->KeyEventMatched(aKeyEvent, aCharCode, aIgnoreModifierState)) {
      continue;  // try the next one
    }

    // Before executing this handler, check that it's not disabled,
    // and that it has something to do (oncommand of the <key> or its
    // <command> is non-empty).
    nsCOMPtr<Element> commandElement;
    if (!GetElementForHandler(handler, getter_AddRefs(commandElement))) {
      continue;
    }

    if (commandElement) {
      if (aExecute && !IsExecutableElement(commandElement)) {
        continue;
      }
    }

    if (!aExecute) {
      if (handler->EventTypeEquals(aEventType)) {
        if (aOutReservedForChrome) {
          *aOutReservedForChrome = IsReservedKey(widgetKeyboardEvent, handler);
        }

        return true;
      }

      // If the command is reserved and the event is keydown, check also if
      // the handler is for keypress because if following keypress event is
      // reserved, we shouldn't dispatch the event into web contents.
      if (aEventType == nsGkAtoms::keydown &&
          handler->EventTypeEquals(nsGkAtoms::keypress)) {
        if (IsReservedKey(widgetKeyboardEvent, handler)) {
          if (aOutReservedForChrome) {
            *aOutReservedForChrome = true;
          }

          return true;
        }
      }
      // Otherwise, we've not found a handler for the event yet.
      continue;
    }

    // This should only be assigned when aExecute is false.
    MOZ_ASSERT(!aOutReservedForChrome);

    // If it's not reserved and the event is a key event on a plugin,
    // the handler shouldn't be executed.
    if (widgetKeyboardEvent->IsKeyEventOnPlugin() &&
        !IsReservedKey(widgetKeyboardEvent, handler)) {
      return false;
    }

    nsCOMPtr<EventTarget> target;
    nsCOMPtr<Element> chromeHandlerElement = GetElement();
    if (chromeHandlerElement) {
      // XXX commandElement may be nullptr...
      target = commandElement;
    } else {
      target = mTarget;
    }

    // XXX Do we execute only one handler even if the handler neither stops
    //     propagation nor prevents default of the event?
    nsresult rv = handler->ExecuteHandler(target, aKeyEvent->AsEvent());
    if (NS_SUCCEEDED(rv)) {
      return true;
    }
  }

#ifdef XP_WIN
  // Windows native applications ignore Windows-Logo key state when checking
  // shortcut keys even if the key is pressed.  Therefore, if there is no
  // shortcut key which exactly matches current modifier state, we should
  // retry to look for a shortcut key without the Windows-Logo key press.
  if (!aIgnoreModifierState.mOS && widgetKeyboardEvent->IsOS()) {
    IgnoreModifierState ignoreModifierState(aIgnoreModifierState);
    ignoreModifierState.mOS = true;
    return WalkHandlersAndExecute(aKeyEvent, aEventType, aFirstHandler,
                                  aCharCode, ignoreModifierState, aExecute);
  }
#endif

  return false;
}

bool
nsXBLWindowKeyHandler::IsReservedKey(WidgetKeyboardEvent* aKeyEvent,
                                     nsXBLPrototypeHandler* aHandler)
{
  XBLReservedKey reserved = aHandler->GetIsReserved();
  // reserved="true" means that the key is always reserved. reserved="false"
  // means that the key is never reserved. Otherwise, we check site-specific
  // permissions.
  if (reserved == XBLReservedKey_False) {
    return false;
  }

  if (reserved == XBLReservedKey_True) {
    return true;
  }

  return nsContentUtils::ShouldBlockReservedKeys(aKeyEvent);
}

bool
nsXBLWindowKeyHandler::HasHandlerForEvent(nsIDOMKeyEvent* aEvent,
                                          bool* aOutReservedForChrome)
{
  WidgetKeyboardEvent* widgetKeyboardEvent =
    aEvent->AsEvent()->WidgetEventPtr()->AsKeyboardEvent();
  if (NS_WARN_IF(!widgetKeyboardEvent) || !widgetKeyboardEvent->IsTrusted()) {
    return false;
  }

  nsresult rv = EnsureHandlers();
  NS_ENSURE_SUCCESS(rv, false);

  bool isDisabled;
  nsCOMPtr<Element> el = GetElement(&isDisabled);
  if (el && isDisabled) {
    return false;
  }

  RefPtr<nsAtom> eventTypeAtom =
    ConvertEventToDOMEventType(*widgetKeyboardEvent);
  return WalkHandlersInternal(aEvent, eventTypeAtom, mHandler, false,
                              aOutReservedForChrome);
}

already_AddRefed<Element>
nsXBLWindowKeyHandler::GetElement(bool* aIsDisabled)
{
  nsCOMPtr<Element> element = do_QueryReferent(mWeakPtrForElement);
  if (element && aIsDisabled) {
    *aIsDisabled = element->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled,
                                        nsGkAtoms::_true, eCaseMatters);
  }
  return element.forget();
}

bool
nsXBLWindowKeyHandler::GetElementForHandler(nsXBLPrototypeHandler* aHandler,
                                            Element** aElementForHandler)
{
  MOZ_ASSERT(aElementForHandler);
  *aElementForHandler = nullptr;

  RefPtr<Element> keyContent = aHandler->GetHandlerElement();
  if (!keyContent) {
    return true; // XXX Even though no key element?
  }

  nsCOMPtr<Element> chromeHandlerElement = GetElement();
  if (!chromeHandlerElement) {
    NS_WARNING_ASSERTION(keyContent->IsInUncomposedDoc(), "uncomposed");
    nsCOMPtr<Element> keyElement = do_QueryInterface(keyContent);
    keyElement.swap(*aElementForHandler);
    return true;
  }

  // We are in a XUL doc.  Obtain our command attribute.
  nsAutoString command;
  keyContent->GetAttr(kNameSpaceID_None, nsGkAtoms::command, command);
  if (command.IsEmpty()) {
    // There is no command element associated with the key element.
    NS_WARNING_ASSERTION(keyContent->IsInUncomposedDoc(), "uncomposed");
    nsCOMPtr<Element> keyElement = do_QueryInterface(keyContent);
    keyElement.swap(*aElementForHandler);
    return true;
  }

  // XXX Shouldn't we check this earlier?
  nsIDocument* doc = keyContent->GetUncomposedDoc();
  if (NS_WARN_IF(!doc)) {
    return false;
  }

  nsCOMPtr<Element> commandElement =
    do_QueryInterface(doc->GetElementById(command));
  if (!commandElement) {
    NS_ERROR("A XUL <key> is observing a command that doesn't exist. "
             "Unable to execute key binding!");
    return false;
  }

  commandElement.swap(*aElementForHandler);
  return true;
}

bool
nsXBLWindowKeyHandler::IsExecutableElement(Element* aElement) const
{
  if (!aElement) {
    return false;
  }

  nsAutoString value;
  aElement->GetAttribute(NS_LITERAL_STRING("disabled"), value);
  if (value.EqualsLiteral("true")) {
    return false;
  }

  aElement->GetAttribute(NS_LITERAL_STRING("oncommand"), value);
  if (value.IsEmpty()) {
    return false;
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////////

already_AddRefed<nsXBLWindowKeyHandler>
NS_NewXBLWindowKeyHandler(nsIDOMElement* aElement, EventTarget* aTarget)
{
  RefPtr<nsXBLWindowKeyHandler> result =
    new nsXBLWindowKeyHandler(aElement, aTarget);
  return result.forget();
}

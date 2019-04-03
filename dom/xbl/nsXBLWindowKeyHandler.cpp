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
#include "nsXBLService.h"
#include "nsIServiceManager.h"
#include "nsGkAtoms.h"
#include "nsXBLDocumentInfo.h"
#include "nsFocusManager.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsContentUtils.h"
#include "nsXBLPrototypeBinding.h"
#include "nsPIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsISelectionController.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/Move.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventBinding.h"
#include "mozilla/dom/KeyboardEvent.h"
#include "mozilla/layers/KeyboardMap.h"
#include "mozilla/ShortcutKeys.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;

nsXBLWindowKeyHandler::nsXBLWindowKeyHandler(Element* aElement,
                                             EventTarget* aTarget)
    : mTarget(aTarget), mHandler(nullptr) {
  mWeakPtrForElement = do_GetWeakReference(aElement);
}

nsXBLWindowKeyHandler::~nsXBLWindowKeyHandler() {
  // If mWeakPtrForElement is non-null, we created a prototype handler.
  if (mWeakPtrForElement) delete mHandler;
}

NS_IMPL_ISUPPORTS(nsXBLWindowKeyHandler, nsIDOMEventListener)

static void BuildHandlerChain(nsIContent* aContent,
                              nsXBLPrototypeHandler** aResult) {
  *aResult = nullptr;

  // Since we chain each handler onto the next handler,
  // we'll enumerate them here in reverse so that when we
  // walk the chain they'll come out in the original order
  for (nsIContent* key = aContent->GetLastChild(); key;
       key = key->GetPreviousSibling()) {
    if (!key->NodeInfo()->Equals(nsGkAtoms::key, kNameSpaceID_XUL)) {
      continue;
    }

    Element* keyElement = key->AsElement();
    // Check whether the key element has empty value at key/char attribute.
    // Such element is used by localizers for alternative shortcut key
    // definition on the locale. See bug 426501.
    nsAutoString valKey, valCharCode, valKeyCode;
    // Hopefully at least one of the attributes is set:
    keyElement->GetAttr(kNameSpaceID_None, nsGkAtoms::key, valKey) ||
        keyElement->GetAttr(kNameSpaceID_None, nsGkAtoms::charcode,
                            valCharCode) ||
        keyElement->GetAttr(kNameSpaceID_None, nsGkAtoms::keycode, valKeyCode);
    // If not, ignore this key element.
    if (valKey.IsEmpty() && valCharCode.IsEmpty() && valKeyCode.IsEmpty()) {
      continue;
    }

    // reserved="pref" is the default for <key> elements.
    XBLReservedKey reserved = XBLReservedKey_Unset;
    if (keyElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::reserved,
                                nsGkAtoms::_true, eCaseMatters)) {
      reserved = XBLReservedKey_True;
    } else if (keyElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::reserved,
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
nsresult nsXBLWindowKeyHandler::EnsureHandlers() {
  nsCOMPtr<Element> el = GetElement();
  NS_ENSURE_STATE(!mWeakPtrForElement || el);
  if (el) {
    // We are actually a XUL <keyset>.
    if (mHandler) return NS_OK;

    BuildHandlerChain(el, &mHandler);
  } else {  // We are an XBL file of handlers.
    // Now determine which handlers we should be using.
    if (IsHTMLEditableFieldFocused()) {
      mHandler = ShortcutKeys::GetHandlers(HandlerType::eEditor);
    } else {
      mHandler = ShortcutKeys::GetHandlers(HandlerType::eBrowser);
    }
  }

  return NS_OK;
}

nsresult nsXBLWindowKeyHandler::WalkHandlers(KeyboardEvent* aKeyEvent) {
  if (aKeyEvent->DefaultPrevented()) {
    return NS_OK;
  }

  // Don't process the event if it was not dispatched from a trusted source
  if (!aKeyEvent->IsTrusted()) {
    return NS_OK;
  }

  nsresult rv = EnsureHandlers();
  NS_ENSURE_SUCCESS(rv, rv);

  bool isDisabled;
  nsCOMPtr<Element> el = GetElement(&isDisabled);

  // skip keysets that are disabled
  if (el && isDisabled) {
    return NS_OK;
  }

  WalkHandlersInternal(aKeyEvent, true);

  return NS_OK;
}

void nsXBLWindowKeyHandler::InstallKeyboardEventListenersTo(
    EventListenerManager* aEventListenerManager) {
  // For marking each keyboard event as if it's reserved by chrome,
  // nsXBLWindowKeyHandlers need to listen each keyboard events before
  // web contents.
  aEventListenerManager->AddEventListenerByType(
      this, NS_LITERAL_STRING("keydown"), TrustedEventsAtCapture());
  aEventListenerManager->AddEventListenerByType(
      this, NS_LITERAL_STRING("keyup"), TrustedEventsAtCapture());
  aEventListenerManager->AddEventListenerByType(
      this, NS_LITERAL_STRING("keypress"), TrustedEventsAtCapture());
  aEventListenerManager->AddEventListenerByType(
      this, NS_LITERAL_STRING("mozkeydownonplugin"), TrustedEventsAtCapture());
  aEventListenerManager->AddEventListenerByType(
      this, NS_LITERAL_STRING("mozkeyuponplugin"), TrustedEventsAtCapture());

  // For reducing the IPC cost, preventing to dispatch reserved keyboard
  // events into the content process.
  aEventListenerManager->AddEventListenerByType(
      this, NS_LITERAL_STRING("keydown"), TrustedEventsAtSystemGroupCapture());
  aEventListenerManager->AddEventListenerByType(
      this, NS_LITERAL_STRING("keyup"), TrustedEventsAtSystemGroupCapture());
  aEventListenerManager->AddEventListenerByType(
      this, NS_LITERAL_STRING("keypress"), TrustedEventsAtSystemGroupCapture());
  aEventListenerManager->AddEventListenerByType(
      this, NS_LITERAL_STRING("mozkeydownonplugin"),
      TrustedEventsAtSystemGroupCapture());
  aEventListenerManager->AddEventListenerByType(
      this, NS_LITERAL_STRING("mozkeyuponplugin"),
      TrustedEventsAtSystemGroupCapture());

  // Handle keyboard events in bubbling phase of the system event group.
  aEventListenerManager->AddEventListenerByType(
      this, NS_LITERAL_STRING("keydown"), TrustedEventsAtSystemGroupBubble());
  aEventListenerManager->AddEventListenerByType(
      this, NS_LITERAL_STRING("keyup"), TrustedEventsAtSystemGroupBubble());
  aEventListenerManager->AddEventListenerByType(
      this, NS_LITERAL_STRING("keypress"), TrustedEventsAtSystemGroupBubble());
  // mozaccesskeynotfound event is fired when modifiers of keypress event
  // matches with modifier of content access key but it's not consumed by
  // remote content.
  aEventListenerManager->AddEventListenerByType(
      this, NS_LITERAL_STRING("mozaccesskeynotfound"),
      TrustedEventsAtSystemGroupBubble());
  aEventListenerManager->AddEventListenerByType(
      this, NS_LITERAL_STRING("mozkeydownonplugin"),
      TrustedEventsAtSystemGroupBubble());
  aEventListenerManager->AddEventListenerByType(
      this, NS_LITERAL_STRING("mozkeyuponplugin"),
      TrustedEventsAtSystemGroupBubble());
}

void nsXBLWindowKeyHandler::RemoveKeyboardEventListenersFrom(
    EventListenerManager* aEventListenerManager) {
  aEventListenerManager->RemoveEventListenerByType(
      this, NS_LITERAL_STRING("keydown"), TrustedEventsAtCapture());
  aEventListenerManager->RemoveEventListenerByType(
      this, NS_LITERAL_STRING("keyup"), TrustedEventsAtCapture());
  aEventListenerManager->RemoveEventListenerByType(
      this, NS_LITERAL_STRING("keypress"), TrustedEventsAtCapture());
  aEventListenerManager->RemoveEventListenerByType(
      this, NS_LITERAL_STRING("mozkeydownonplugin"), TrustedEventsAtCapture());
  aEventListenerManager->RemoveEventListenerByType(
      this, NS_LITERAL_STRING("mozkeyuponplugin"), TrustedEventsAtCapture());

  aEventListenerManager->RemoveEventListenerByType(
      this, NS_LITERAL_STRING("keydown"), TrustedEventsAtSystemGroupCapture());
  aEventListenerManager->RemoveEventListenerByType(
      this, NS_LITERAL_STRING("keyup"), TrustedEventsAtSystemGroupCapture());
  aEventListenerManager->RemoveEventListenerByType(
      this, NS_LITERAL_STRING("keypress"), TrustedEventsAtSystemGroupCapture());
  aEventListenerManager->RemoveEventListenerByType(
      this, NS_LITERAL_STRING("mozkeydownonplugin"),
      TrustedEventsAtSystemGroupCapture());
  aEventListenerManager->RemoveEventListenerByType(
      this, NS_LITERAL_STRING("mozkeyuponplugin"),
      TrustedEventsAtSystemGroupCapture());

  aEventListenerManager->RemoveEventListenerByType(
      this, NS_LITERAL_STRING("keydown"), TrustedEventsAtSystemGroupBubble());
  aEventListenerManager->RemoveEventListenerByType(
      this, NS_LITERAL_STRING("keyup"), TrustedEventsAtSystemGroupBubble());
  aEventListenerManager->RemoveEventListenerByType(
      this, NS_LITERAL_STRING("keypress"), TrustedEventsAtSystemGroupBubble());
  aEventListenerManager->RemoveEventListenerByType(
      this, NS_LITERAL_STRING("mozaccesskeynotfound"),
      TrustedEventsAtSystemGroupBubble());
  aEventListenerManager->RemoveEventListenerByType(
      this, NS_LITERAL_STRING("mozkeydownonplugin"),
      TrustedEventsAtSystemGroupBubble());
  aEventListenerManager->RemoveEventListenerByType(
      this, NS_LITERAL_STRING("mozkeyuponplugin"),
      TrustedEventsAtSystemGroupBubble());
}

/* static */
KeyboardMap nsXBLWindowKeyHandler::CollectKeyboardShortcuts() {
  nsXBLPrototypeHandler* handlers =
      ShortcutKeys::GetHandlers(HandlerType::eBrowser);

  // Convert the handlers into keyboard shortcuts, using an AutoTArray with
  // the maximum amount of shortcuts used on any platform to minimize
  // allocations
  AutoTArray<KeyboardShortcut, 48> shortcuts;

  // Append keyboard shortcuts for hardcoded actions like tab
  KeyboardShortcut::AppendHardcodedShortcuts(shortcuts);

  for (nsXBLPrototypeHandler* handler = handlers; handler;
       handler = handler->GetNextHandler()) {
    KeyboardShortcut shortcut;
    if (handler->TryConvertToKeyboardShortcut(&shortcut)) {
      shortcuts.AppendElement(shortcut);
    }
  }

  return KeyboardMap(std::move(shortcuts));
}

NS_IMETHODIMP
nsXBLWindowKeyHandler::HandleEvent(Event* aEvent) {
  RefPtr<KeyboardEvent> keyEvent = aEvent->AsKeyboardEvent();
  NS_ENSURE_TRUE(keyEvent, NS_ERROR_INVALID_ARG);

  if (aEvent->EventPhase() == Event_Binding::CAPTURING_PHASE) {
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

  return WalkHandlers(keyEvent);
}

void nsXBLWindowKeyHandler::HandleEventOnCaptureInDefaultEventGroup(
    KeyboardEvent* aEvent) {
  WidgetKeyboardEvent* widgetKeyboardEvent =
      aEvent->WidgetEventPtr()->AsKeyboardEvent();

  if (widgetKeyboardEvent->IsReservedByChrome()) {
    return;
  }

  bool isReserved = false;
  if (HasHandlerForEvent(aEvent, &isReserved) && isReserved) {
    widgetKeyboardEvent->MarkAsReservedByChrome();
  }
}

void nsXBLWindowKeyHandler::HandleEventOnCaptureInSystemEventGroup(
    KeyboardEvent* aEvent) {
  WidgetKeyboardEvent* widgetEvent =
      aEvent->WidgetEventPtr()->AsKeyboardEvent();

  // If the event won't be sent to remote process, this listener needs to do
  // nothing.  Note that even if mOnlySystemGroupDispatchInContent is true,
  // we need to send the event to remote process and check reply event
  // before matching it with registered shortcut keys because event listeners
  // in the system event group may want to handle the event before registered
  // shortcut key handlers.
  if (!widgetEvent->WillBeSentToRemoteProcess()) {
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

bool nsXBLWindowKeyHandler::IsHTMLEditableFieldFocused() {
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm) return false;

  nsCOMPtr<mozIDOMWindowProxy> focusedWindow;
  fm->GetFocusedWindow(getter_AddRefs(focusedWindow));
  if (!focusedWindow) return false;

  auto* piwin = nsPIDOMWindowOuter::From(focusedWindow);
  nsIDocShell* docShell = piwin->GetDocShell();
  if (!docShell) {
    return false;
  }

  RefPtr<HTMLEditor> htmlEditor = docShell->GetHTMLEditor();
  if (!htmlEditor) {
    return false;
  }

  nsCOMPtr<Document> doc = htmlEditor->GetDocument();
  if (doc->HasFlag(NODE_IS_EDITABLE)) {
    // Don't need to perform any checks in designMode documents.
    return true;
  }

  nsINode* focusedNode = fm->GetFocusedElement();
  if (focusedNode && focusedNode->IsElement()) {
    // If there is a focused element, make sure it's in the active editing host.
    // Note that GetActiveEditingHost finds the current editing host based on
    // the document's selection.  Even though the document selection is usually
    // collapsed to where the focus is, but the page may modify the selection
    // without our knowledge, in which case this check will do something useful.
    nsCOMPtr<Element> activeEditingHost = htmlEditor->GetActiveEditingHost();
    if (!activeEditingHost) {
      return false;
    }
    return nsContentUtils::ContentIsDescendantOf(focusedNode,
                                                 activeEditingHost);
  }

  return false;
}

//
// WalkHandlersInternal and WalkHandlersAndExecute
//
// Given a particular DOM event and a pointer to the first handler in the list,
// scan through the list to find something to handle the event. If aExecute =
// true, the handler will be executed; otherwise just return an answer telling
// if a handler for that event was found.
//
bool nsXBLWindowKeyHandler::WalkHandlersInternal(KeyboardEvent* aKeyEvent,
                                                 bool aExecute,
                                                 bool* aOutReservedForChrome) {
  WidgetKeyboardEvent* nativeKeyboardEvent =
      aKeyEvent->WidgetEventPtr()->AsKeyboardEvent();
  MOZ_ASSERT(nativeKeyboardEvent);

  AutoShortcutKeyCandidateArray shortcutKeys;
  nativeKeyboardEvent->GetShortcutKeyCandidates(shortcutKeys);

  if (shortcutKeys.IsEmpty()) {
    return WalkHandlersAndExecute(aKeyEvent, 0, IgnoreModifierState(), aExecute,
                                  aOutReservedForChrome);
  }

  for (uint32_t i = 0; i < shortcutKeys.Length(); ++i) {
    ShortcutKeyCandidate& key = shortcutKeys[i];
    IgnoreModifierState ignoreModifierState;
    ignoreModifierState.mShift = key.mIgnoreShift;
    if (WalkHandlersAndExecute(aKeyEvent, key.mCharCode, ignoreModifierState,
                               aExecute, aOutReservedForChrome)) {
      return true;
    }
  }
  return false;
}

bool nsXBLWindowKeyHandler::WalkHandlersAndExecute(
    KeyboardEvent* aKeyEvent, uint32_t aCharCode,
    const IgnoreModifierState& aIgnoreModifierState, bool aExecute,
    bool* aOutReservedForChrome) {
  if (aOutReservedForChrome) {
    *aOutReservedForChrome = false;
  }

  WidgetKeyboardEvent* widgetKeyboardEvent =
      aKeyEvent->WidgetEventPtr()->AsKeyboardEvent();
  if (NS_WARN_IF(!widgetKeyboardEvent)) {
    return false;
  }

  nsAtom* eventType =
      ShortcutKeys::ConvertEventToDOMEventType(widgetKeyboardEvent);

  // Try all of the handlers until we find one that matches the event.
  for (nsXBLPrototypeHandler* handler = mHandler; handler;
       handler = handler->GetNextHandler()) {
    bool stopped = aKeyEvent->IsDispatchStopped();
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
      } else if (!handler->EventTypeEquals(eventType)) {
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
        if (eventType != nsGkAtoms::keydown &&
            eventType != nsGkAtoms::keypress) {
          continue;
        }
      } else if (!handler->EventTypeEquals(eventType)) {
        // Otherwise, eventType should exactly be matched.
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
      if (handler->EventTypeEquals(eventType)) {
        if (aOutReservedForChrome) {
          *aOutReservedForChrome = IsReservedKey(widgetKeyboardEvent, handler);
        }

        return true;
      }

      // If the command is reserved and the event is keydown, check also if
      // the handler is for keypress because if following keypress event is
      // reserved, we shouldn't dispatch the event into web contents.
      if (eventType == nsGkAtoms::keydown &&
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
    nsresult rv = handler->ExecuteHandler(target, aKeyEvent);
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
    return WalkHandlersAndExecute(aKeyEvent, aCharCode, ignoreModifierState,
                                  aExecute);
  }
#endif

  return false;
}

bool nsXBLWindowKeyHandler::IsReservedKey(WidgetKeyboardEvent* aKeyEvent,
                                          nsXBLPrototypeHandler* aHandler) {
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

bool nsXBLWindowKeyHandler::HasHandlerForEvent(KeyboardEvent* aEvent,
                                               bool* aOutReservedForChrome) {
  WidgetKeyboardEvent* widgetKeyboardEvent =
      aEvent->WidgetEventPtr()->AsKeyboardEvent();
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

  return WalkHandlersInternal(aEvent, false, aOutReservedForChrome);
}

already_AddRefed<Element> nsXBLWindowKeyHandler::GetElement(bool* aIsDisabled) {
  nsCOMPtr<Element> element = do_QueryReferent(mWeakPtrForElement);
  if (element && aIsDisabled) {
    *aIsDisabled = element->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled,
                                        nsGkAtoms::_true, eCaseMatters);
  }
  return element.forget();
}

bool nsXBLWindowKeyHandler::GetElementForHandler(
    nsXBLPrototypeHandler* aHandler, Element** aElementForHandler) {
  MOZ_ASSERT(aElementForHandler);
  *aElementForHandler = nullptr;

  RefPtr<Element> keyElement = aHandler->GetHandlerElement();
  if (!keyElement) {
    return true;  // XXX Even though no key element?
  }

  nsCOMPtr<Element> chromeHandlerElement = GetElement();
  if (!chromeHandlerElement) {
    NS_WARNING_ASSERTION(keyElement->IsInUncomposedDoc(), "uncomposed");
    keyElement.swap(*aElementForHandler);
    return true;
  }

  // We are in a XUL doc.  Obtain our command attribute.
  nsAutoString command;
  keyElement->GetAttr(kNameSpaceID_None, nsGkAtoms::command, command);
  if (command.IsEmpty()) {
    // There is no command element associated with the key element.
    NS_WARNING_ASSERTION(keyElement->IsInUncomposedDoc(), "uncomposed");
    keyElement.swap(*aElementForHandler);
    return true;
  }

  // XXX Shouldn't we check this earlier?
  Document* doc = keyElement->GetUncomposedDoc();
  if (NS_WARN_IF(!doc)) {
    return false;
  }

  nsCOMPtr<Element> commandElement = doc->GetElementById(command);
  if (!commandElement) {
    NS_ERROR(
        "A XUL <key> is observing a command that doesn't exist. "
        "Unable to execute key binding!");
    return false;
  }

  commandElement.swap(*aElementForHandler);
  return true;
}

bool nsXBLWindowKeyHandler::IsExecutableElement(Element* aElement) const {
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

already_AddRefed<nsXBLWindowKeyHandler> NS_NewXBLWindowKeyHandler(
    Element* aElement, EventTarget* aTarget) {
  RefPtr<nsXBLWindowKeyHandler> result =
      new nsXBLWindowKeyHandler(aElement, aTarget);
  return result.forget();
}

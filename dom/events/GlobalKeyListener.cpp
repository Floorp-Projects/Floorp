/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GlobalKeyListener.h"
#include "EventTarget.h"

#include <utility>

#include "mozilla/EventListenerManager.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/KeyEventHandler.h"
#include "mozilla/Preferences.h"
#include "mozilla/ShortcutKeys.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventBinding.h"
#include "mozilla/dom/KeyboardEvent.h"
#include "nsAtom.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsFocusManager.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsIContentInlines.h"
#include "nsIDocShell.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"

namespace mozilla {

using namespace mozilla::layers;

GlobalKeyListener::GlobalKeyListener(dom::EventTarget* aTarget)
    : mTarget(aTarget), mHandler(nullptr) {}

NS_IMPL_ISUPPORTS(GlobalKeyListener, nsIDOMEventListener)

static void BuildHandlerChain(nsIContent* aContent, KeyEventHandler** aResult) {
  *aResult = nullptr;

  // Since we chain each handler onto the next handler,
  // we'll enumerate them here in reverse so that when we
  // walk the chain they'll come out in the original order
  for (nsIContent* key = aContent->GetLastChild(); key;
       key = key->GetPreviousSibling()) {
    if (!key->NodeInfo()->Equals(nsGkAtoms::key, kNameSpaceID_XUL)) {
      continue;
    }

    dom::Element* keyElement = key->AsElement();
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
    ReservedKey reserved = ReservedKey_Unset;
    if (keyElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::reserved,
                                nsGkAtoms::_true, eCaseMatters)) {
      reserved = ReservedKey_True;
    } else if (keyElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::reserved,
                                       nsGkAtoms::_false, eCaseMatters)) {
      reserved = ReservedKey_False;
    }

    KeyEventHandler* handler = new KeyEventHandler(keyElement, reserved);

    handler->SetNextHandler(*aResult);
    *aResult = handler;
  }
}

void GlobalKeyListener::WalkHandlers(dom::KeyboardEvent* aKeyEvent) {
  if (aKeyEvent->DefaultPrevented()) {
    return;
  }

  // Don't process the event if it was not dispatched from a trusted source
  if (!aKeyEvent->IsTrusted()) {
    return;
  }

  EnsureHandlers();

  // skip keysets that are disabled
  if (IsDisabled()) {
    return;
  }

  WalkHandlersInternal(aKeyEvent, true);
}

void GlobalKeyListener::InstallKeyboardEventListenersTo(
    EventListenerManager* aEventListenerManager) {
  // For marking each keyboard event as if it's reserved by chrome,
  // GlobalKeyListeners need to listen each keyboard events before
  // web contents.
  aEventListenerManager->AddEventListenerByType(this, u"keydown"_ns,
                                                TrustedEventsAtCapture());
  aEventListenerManager->AddEventListenerByType(this, u"keyup"_ns,
                                                TrustedEventsAtCapture());
  aEventListenerManager->AddEventListenerByType(this, u"keypress"_ns,
                                                TrustedEventsAtCapture());

  // For reducing the IPC cost, preventing to dispatch reserved keyboard
  // events into the content process.
  aEventListenerManager->AddEventListenerByType(
      this, u"keydown"_ns, TrustedEventsAtSystemGroupCapture());
  aEventListenerManager->AddEventListenerByType(
      this, u"keyup"_ns, TrustedEventsAtSystemGroupCapture());
  aEventListenerManager->AddEventListenerByType(
      this, u"keypress"_ns, TrustedEventsAtSystemGroupCapture());

  // Handle keyboard events in bubbling phase of the system event group.
  aEventListenerManager->AddEventListenerByType(
      this, u"keydown"_ns, TrustedEventsAtSystemGroupBubble());
  aEventListenerManager->AddEventListenerByType(
      this, u"keyup"_ns, TrustedEventsAtSystemGroupBubble());
  aEventListenerManager->AddEventListenerByType(
      this, u"keypress"_ns, TrustedEventsAtSystemGroupBubble());
  // mozaccesskeynotfound event is fired when modifiers of keypress event
  // matches with modifier of content access key but it's not consumed by
  // remote content.
  aEventListenerManager->AddEventListenerByType(
      this, u"mozaccesskeynotfound"_ns, TrustedEventsAtSystemGroupBubble());
}

void GlobalKeyListener::RemoveKeyboardEventListenersFrom(
    EventListenerManager* aEventListenerManager) {
  aEventListenerManager->RemoveEventListenerByType(this, u"keydown"_ns,
                                                   TrustedEventsAtCapture());
  aEventListenerManager->RemoveEventListenerByType(this, u"keyup"_ns,
                                                   TrustedEventsAtCapture());
  aEventListenerManager->RemoveEventListenerByType(this, u"keypress"_ns,
                                                   TrustedEventsAtCapture());

  aEventListenerManager->RemoveEventListenerByType(
      this, u"keydown"_ns, TrustedEventsAtSystemGroupCapture());
  aEventListenerManager->RemoveEventListenerByType(
      this, u"keyup"_ns, TrustedEventsAtSystemGroupCapture());
  aEventListenerManager->RemoveEventListenerByType(
      this, u"keypress"_ns, TrustedEventsAtSystemGroupCapture());

  aEventListenerManager->RemoveEventListenerByType(
      this, u"keydown"_ns, TrustedEventsAtSystemGroupBubble());
  aEventListenerManager->RemoveEventListenerByType(
      this, u"keyup"_ns, TrustedEventsAtSystemGroupBubble());
  aEventListenerManager->RemoveEventListenerByType(
      this, u"keypress"_ns, TrustedEventsAtSystemGroupBubble());
  aEventListenerManager->RemoveEventListenerByType(
      this, u"mozaccesskeynotfound"_ns, TrustedEventsAtSystemGroupBubble());
}

NS_IMETHODIMP
GlobalKeyListener::HandleEvent(dom::Event* aEvent) {
  RefPtr<dom::KeyboardEvent> keyEvent = aEvent->AsKeyboardEvent();
  NS_ENSURE_TRUE(keyEvent, NS_ERROR_INVALID_ARG);

  if (aEvent->EventPhase() == dom::Event_Binding::CAPTURING_PHASE) {
    if (aEvent->WidgetEventPtr()->mFlags.mInSystemGroup) {
      HandleEventOnCaptureInSystemEventGroup(keyEvent);
    } else {
      HandleEventOnCaptureInDefaultEventGroup(keyEvent);
    }
    return NS_OK;
  }

  // If this event was handled by APZ then don't do the default action, and
  // preventDefault to prevent any other listeners from handling the event.
  if (aEvent->WidgetEventPtr()->mFlags.mHandledByAPZ) {
    aEvent->PreventDefault();
    return NS_OK;
  }

  WalkHandlers(keyEvent);
  return NS_OK;
}

void GlobalKeyListener::HandleEventOnCaptureInDefaultEventGroup(
    dom::KeyboardEvent* aEvent) {
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

void GlobalKeyListener::HandleEventOnCaptureInSystemEventGroup(
    dom::KeyboardEvent* aEvent) {
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

//
// WalkHandlersInternal and WalkHandlersAndExecute
//
// Given a particular DOM event and a pointer to the first handler in the list,
// scan through the list to find something to handle the event. If aExecute =
// true, the handler will be executed; otherwise just return an answer telling
// if a handler for that event was found.
//
bool GlobalKeyListener::WalkHandlersInternal(dom::KeyboardEvent* aKeyEvent,
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

  for (unsigned long i = 0; i < shortcutKeys.Length(); ++i) {
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

bool GlobalKeyListener::WalkHandlersAndExecute(
    dom::KeyboardEvent* aKeyEvent, uint32_t aCharCode,
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
  for (KeyEventHandler* handler = mHandler; handler;
       handler = handler->GetNextHandler()) {
    bool stopped = aKeyEvent->IsDispatchStopped();
    if (stopped) {
      // The event is finished, don't execute any more handlers
      return false;
    }

    if (aExecute) {
      if (!handler->EventTypeEquals(eventType)) {
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
    if (!CanHandle(handler, aExecute)) {
      continue;
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

    nsCOMPtr<dom::EventTarget> target = GetHandlerTarget(handler);

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

bool GlobalKeyListener::IsReservedKey(WidgetKeyboardEvent* aKeyEvent,
                                      KeyEventHandler* aHandler) {
  ReservedKey reserved = aHandler->GetIsReserved();
  // reserved="true" means that the key is always reserved. reserved="false"
  // means that the key is never reserved. Otherwise, we check site-specific
  // permissions.
  if (reserved == ReservedKey_False) {
    return false;
  }

  if (reserved == ReservedKey_True) {
    return true;
  }

  return nsContentUtils::ShouldBlockReservedKeys(aKeyEvent);
}

bool GlobalKeyListener::HasHandlerForEvent(dom::KeyboardEvent* aEvent,
                                           bool* aOutReservedForChrome) {
  WidgetKeyboardEvent* widgetKeyboardEvent =
      aEvent->WidgetEventPtr()->AsKeyboardEvent();
  if (NS_WARN_IF(!widgetKeyboardEvent) || !widgetKeyboardEvent->IsTrusted()) {
    return false;
  }

  EnsureHandlers();

  if (IsDisabled()) {
    return false;
  }

  return WalkHandlersInternal(aEvent, false, aOutReservedForChrome);
}

//
// AttachGlobalKeyHandler
//
// Creates a new key handler and prepares to listen to key events on the given
// event receiver (either a document or an content node). If the receiver is
// content, then extra work needs to be done to hook it up to the document (XXX
// WHY??)
//
void XULKeySetGlobalKeyListener::AttachKeyHandler(
    dom::Element* aElementTarget) {
  // Only attach if we're really in a document
  nsCOMPtr<dom::Document> doc = aElementTarget->GetUncomposedDoc();
  if (!doc) {
    return;
  }

  EventListenerManager* manager = doc->GetOrCreateListenerManager();
  if (!manager) {
    return;
  }

  // the listener already exists, so skip this
  if (aElementTarget->GetProperty(nsGkAtoms::listener)) {
    return;
  }

  // Create the key handler
  RefPtr<XULKeySetGlobalKeyListener> handler =
      new XULKeySetGlobalKeyListener(aElementTarget, doc);

  handler->InstallKeyboardEventListenersTo(manager);

  aElementTarget->SetProperty(nsGkAtoms::listener, handler.forget().take(),
                              nsPropertyTable::SupportsDtorFunc, true);
}

//
// DetachGlobalKeyHandler
//
// Removes a key handler added by AttachKeyHandler.
//
void XULKeySetGlobalKeyListener::DetachKeyHandler(
    dom::Element* aElementTarget) {
  // Only attach if we're really in a document
  nsCOMPtr<dom::Document> doc = aElementTarget->GetUncomposedDoc();
  if (!doc) {
    return;
  }

  EventListenerManager* manager = doc->GetOrCreateListenerManager();
  if (!manager) {
    return;
  }

  nsIDOMEventListener* handler = static_cast<nsIDOMEventListener*>(
      aElementTarget->GetProperty(nsGkAtoms::listener));
  if (!handler) {
    return;
  }

  static_cast<XULKeySetGlobalKeyListener*>(handler)
      ->RemoveKeyboardEventListenersFrom(manager);
  aElementTarget->RemoveProperty(nsGkAtoms::listener);
}

XULKeySetGlobalKeyListener::XULKeySetGlobalKeyListener(
    dom::Element* aElement, dom::EventTarget* aTarget)
    : GlobalKeyListener(aTarget) {
  mWeakPtrForElement = do_GetWeakReference(aElement);
}

dom::Element* XULKeySetGlobalKeyListener::GetElement(bool* aIsDisabled) const {
  RefPtr<dom::Element> element = do_QueryReferent(mWeakPtrForElement);
  if (element && aIsDisabled) {
    *aIsDisabled = element->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled,
                                        nsGkAtoms::_true, eCaseMatters);
  }
  return element.get();
}

XULKeySetGlobalKeyListener::~XULKeySetGlobalKeyListener() {
  if (mWeakPtrForElement) {
    delete mHandler;
  }
}

void XULKeySetGlobalKeyListener::EnsureHandlers() {
  if (mHandler) {
    return;
  }

  dom::Element* element = GetElement();
  if (!element) {
    return;
  }

  BuildHandlerChain(element, &mHandler);
}

bool XULKeySetGlobalKeyListener::IsDisabled() const {
  bool isDisabled;
  dom::Element* element = GetElement(&isDisabled);
  return element && isDisabled;
}

bool XULKeySetGlobalKeyListener::GetElementForHandler(
    KeyEventHandler* aHandler, dom::Element** aElementForHandler) const {
  MOZ_ASSERT(aElementForHandler);
  *aElementForHandler = nullptr;

  RefPtr<dom::Element> keyElement = aHandler->GetHandlerElement();
  if (!keyElement) {
    // This should only be the case where the <key> element that generated the
    // handler has been destroyed. Not sure why we return true here...
    return true;
  }

  nsCOMPtr<dom::Element> chromeHandlerElement = GetElement();
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
  dom::Document* doc = keyElement->GetUncomposedDoc();
  if (NS_WARN_IF(!doc)) {
    return false;
  }

  nsCOMPtr<dom::Element> commandElement = doc->GetElementById(command);
  if (!commandElement) {
    NS_ERROR(
        "A XUL <key> is observing a command that doesn't exist. "
        "Unable to execute key binding!");
    return false;
  }

  commandElement.swap(*aElementForHandler);
  return true;
}

bool XULKeySetGlobalKeyListener::IsExecutableElement(
    dom::Element* aElement) const {
  if (!aElement) {
    return false;
  }

  nsAutoString value;
  aElement->GetAttr(nsGkAtoms::disabled, value);
  if (value.EqualsLiteral("true")) {
    return false;
  }

  aElement->GetAttr(nsGkAtoms::oncommand, value);
  return !value.IsEmpty();
}

already_AddRefed<dom::EventTarget> XULKeySetGlobalKeyListener::GetHandlerTarget(
    KeyEventHandler* aHandler) {
  nsCOMPtr<dom::Element> commandElement;
  if (!GetElementForHandler(aHandler, getter_AddRefs(commandElement))) {
    return nullptr;
  }

  return commandElement.forget();
}

bool XULKeySetGlobalKeyListener::CanHandle(KeyEventHandler* aHandler,
                                           bool aWillExecute) const {
  nsCOMPtr<dom::Element> commandElement;
  if (!GetElementForHandler(aHandler, getter_AddRefs(commandElement))) {
    return false;
  }

  // The only case where commandElement can be null here is where the <key>
  // element for the handler is already destroyed. I'm not sure why we continue
  // in this case.
  if (!commandElement) {
    return true;
  }

  // If we're not actually going to execute here bypass the execution check.
  return !aWillExecute || IsExecutableElement(commandElement);
}

/* static */
layers::KeyboardMap RootWindowGlobalKeyListener::CollectKeyboardShortcuts() {
  KeyEventHandler* handlers = ShortcutKeys::GetHandlers(HandlerType::eBrowser);

  // Convert the handlers into keyboard shortcuts, using an AutoTArray with
  // the maximum amount of shortcuts used on any platform to minimize
  // allocations
  AutoTArray<KeyboardShortcut, 48> shortcuts;

  // Append keyboard shortcuts for hardcoded actions like tab
  KeyboardShortcut::AppendHardcodedShortcuts(shortcuts);

  for (KeyEventHandler* handler = handlers; handler;
       handler = handler->GetNextHandler()) {
    KeyboardShortcut shortcut;
    if (handler->TryConvertToKeyboardShortcut(&shortcut)) {
      shortcuts.AppendElement(shortcut);
    }
  }

  return layers::KeyboardMap(std::move(shortcuts));
}

//
// AttachGlobalKeyHandler
//
// Creates a new key handler and prepares to listen to key events on the given
// event receiver (either a document or an content node). If the receiver is
// content, then extra work needs to be done to hook it up to the document (XXX
// WHY??)
//
void RootWindowGlobalKeyListener::AttachKeyHandler(dom::EventTarget* aTarget) {
  EventListenerManager* manager = aTarget->GetOrCreateListenerManager();
  if (!manager) {
    return;
  }

  // Create the key handler
  RefPtr<RootWindowGlobalKeyListener> handler =
      new RootWindowGlobalKeyListener(aTarget);

  // This registers handler with the manager so the manager will keep handler
  // alive past this point.
  handler->InstallKeyboardEventListenersTo(manager);
}

RootWindowGlobalKeyListener::RootWindowGlobalKeyListener(
    dom::EventTarget* aTarget)
    : GlobalKeyListener(aTarget) {}

/* static */
bool RootWindowGlobalKeyListener::IsHTMLEditorFocused() {
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm) {
    return false;
  }

  nsCOMPtr<mozIDOMWindowProxy> focusedWindow;
  fm->GetFocusedWindow(getter_AddRefs(focusedWindow));
  if (!focusedWindow) {
    return false;
  }

  auto* piwin = nsPIDOMWindowOuter::From(focusedWindow);
  nsIDocShell* docShell = piwin->GetDocShell();
  if (!docShell) {
    return false;
  }

  HTMLEditor* htmlEditor = docShell->GetHTMLEditor();
  if (!htmlEditor) {
    return false;
  }

  if (htmlEditor->IsInDesignMode()) {
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
    nsCOMPtr<dom::Element> activeEditingHost =
        htmlEditor->GetActiveEditingHost();
    if (!activeEditingHost) {
      return false;
    }
    return focusedNode->IsInclusiveDescendantOf(activeEditingHost);
  }

  return false;
}

void RootWindowGlobalKeyListener::EnsureHandlers() {
  if (IsHTMLEditorFocused()) {
    mHandler = ShortcutKeys::GetHandlers(HandlerType::eEditor);
  } else {
    mHandler = ShortcutKeys::GetHandlers(HandlerType::eBrowser);
  }
}

}  // namespace mozilla

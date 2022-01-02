/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nsCOMPtr.h"
#include "nsQueryObject.h"
#include "KeyEventHandler.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsGlobalWindowCommands.h"
#include "nsIContent.h"
#include "nsAtom.h"
#include "nsNameSpaceManager.h"
#include "mozilla/dom/Document.h"
#include "nsIController.h"
#include "nsIControllers.h"
#include "nsXULElement.h"
#include "nsFocusManager.h"
#include "nsIFormControl.h"
#include "nsPIDOMWindow.h"
#include "nsPIWindowRoot.h"
#include "nsIScriptError.h"
#include "nsIWeakReferenceUtils.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsGkAtoms.h"
#include "nsDOMCID.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"
#include "nsJSUtils.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/JSEventHandler.h"
#include "mozilla/Preferences.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventHandlerBinding.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/HTMLTextAreaElement.h"
#include "mozilla/dom/KeyboardEvent.h"
#include "mozilla/dom/KeyboardEventBinding.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/layers/KeyboardMap.h"
#include "xpcpublic.h"

namespace mozilla {

using namespace mozilla::layers;

uint32_t KeyEventHandler::gRefCnt = 0;

int32_t KeyEventHandler::kMenuAccessKey = -1;

const int32_t KeyEventHandler::cShift = (1 << 0);
const int32_t KeyEventHandler::cAlt = (1 << 1);
const int32_t KeyEventHandler::cControl = (1 << 2);
const int32_t KeyEventHandler::cMeta = (1 << 3);
const int32_t KeyEventHandler::cOS = (1 << 4);

const int32_t KeyEventHandler::cShiftMask = (1 << 5);
const int32_t KeyEventHandler::cAltMask = (1 << 6);
const int32_t KeyEventHandler::cControlMask = (1 << 7);
const int32_t KeyEventHandler::cMetaMask = (1 << 8);
const int32_t KeyEventHandler::cOSMask = (1 << 9);

const int32_t KeyEventHandler::cAllModifiers =
    cShiftMask | cAltMask | cControlMask | cMetaMask | cOSMask;

KeyEventHandler::KeyEventHandler(dom::Element* aHandlerElement,
                                 ReservedKey aReserved)
    : mHandlerElement(nullptr),
      mIsXULKey(true),
      mReserved(aReserved),
      mNextHandler(nullptr) {
  Init();

  // Make sure our prototype is initialized.
  ConstructPrototype(aHandlerElement);
}

KeyEventHandler::KeyEventHandler(ShortcutKeyData* aKeyData)
    : mCommand(nullptr),
      mIsXULKey(false),
      mReserved(ReservedKey_False),
      mNextHandler(nullptr) {
  Init();

  ConstructPrototype(nullptr, aKeyData->event, aKeyData->command,
                     aKeyData->keycode, aKeyData->key, aKeyData->modifiers);
}

KeyEventHandler::~KeyEventHandler() {
  --gRefCnt;
  if (mIsXULKey) {
    NS_IF_RELEASE(mHandlerElement);
  } else if (mCommand) {
    free(mCommand);
  }

  // We own the next handler in the chain, so delete it now.
  NS_CONTENT_DELETE_LIST_MEMBER(KeyEventHandler, this, mNextHandler);
}

void KeyEventHandler::GetCommand(nsAString& aCommand) const {
  MOZ_ASSERT(aCommand.IsEmpty());
  if (mIsXULKey) {
    MOZ_ASSERT_UNREACHABLE("Not yet implemented");
    return;
  }
  if (mCommand) {
    aCommand.Assign(mCommand);
  }
}

bool KeyEventHandler::TryConvertToKeyboardShortcut(
    KeyboardShortcut* aOut) const {
  // Convert the event type
  KeyboardInput::KeyboardEventType eventType;

  if (mEventName == nsGkAtoms::keydown) {
    eventType = KeyboardInput::KEY_DOWN;
  } else if (mEventName == nsGkAtoms::keypress) {
    eventType = KeyboardInput::KEY_PRESS;
  } else if (mEventName == nsGkAtoms::keyup) {
    eventType = KeyboardInput::KEY_UP;
  } else {
    return false;
  }

  // Convert the modifiers
  Modifiers modifiersMask = GetModifiersMask();
  Modifiers modifiers = GetModifiers();

  // Mask away any bits that won't be compared
  modifiers &= modifiersMask;

  // Convert the keyCode or charCode
  uint32_t keyCode;
  uint32_t charCode;

  if (mMisc) {
    keyCode = 0;
    charCode = static_cast<uint32_t>(mDetail);
  } else {
    keyCode = static_cast<uint32_t>(mDetail);
    charCode = 0;
  }

  NS_LossyConvertUTF16toASCII commandText(mCommand);
  KeyboardScrollAction action;
  if (!nsGlobalWindowCommands::FindScrollCommand(commandText.get(), &action)) {
    // This action doesn't represent a scroll so we need to create a dispatch
    // to content keyboard shortcut so APZ handles this command correctly
    *aOut = KeyboardShortcut(eventType, keyCode, charCode, modifiers,
                             modifiersMask);
    return true;
  }

  // This prototype is a command which represents a scroll action, so create
  // a keyboard shortcut to handle it
  *aOut = KeyboardShortcut(eventType, keyCode, charCode, modifiers,
                           modifiersMask, action);
  return true;
}

already_AddRefed<dom::Element> KeyEventHandler::GetHandlerElement() {
  if (mIsXULKey) {
    nsCOMPtr<dom::Element> element = do_QueryReferent(mHandlerElement);
    return element.forget();
  }

  return nullptr;
}

/////////////////////////////////////////////////////////////////////////////
// Get the menu access key from prefs.
// XXX Eventually pick up using CSS3 key-equivalent property or somesuch
void KeyEventHandler::InitAccessKeys() {
  if (kMenuAccessKey >= 0) {
    return;
  }

  // Compiled-in defaults, in case we can't get the pref --
  // mac doesn't have menu shortcuts, other platforms use alt.
#ifdef XP_MACOSX
  kMenuAccessKey = 0;
#else
  kMenuAccessKey = dom::KeyboardEvent_Binding::DOM_VK_ALT;
#endif

  // Get the menu access key value from prefs, overriding the default:
  kMenuAccessKey = Preferences::GetInt("ui.key.menuAccessKey", kMenuAccessKey);
}

nsresult KeyEventHandler::ExecuteHandler(dom::EventTarget* aTarget,
                                         dom::Event* aEvent) {
  // In both cases the union should be defined.
  if (!mHandlerElement) {
    return NS_ERROR_FAILURE;
  }

  // XUL handlers and commands shouldn't be triggered by non-trusted
  // events.
  if (!aEvent->IsTrusted()) {
    return NS_OK;
  }

  if (mIsXULKey) {
    return DispatchXULKeyCommand(aEvent);
  }

  return DispatchXBLCommand(aTarget, aEvent);
}

nsresult KeyEventHandler::DispatchXBLCommand(dom::EventTarget* aTarget,
                                             dom::Event* aEvent) {
  // This is a special-case optimization to make command handling fast.
  // It isn't really a part of XBL, but it helps speed things up.

  if (aEvent) {
    // See if preventDefault has been set.  If so, don't execute.
    if (aEvent->DefaultPrevented()) {
      return NS_OK;
    }
    bool dispatchStopped = aEvent->IsDispatchStopped();
    if (dispatchStopped) {
      return NS_OK;
    }
  }

  // Instead of executing JS, let's get the controller for the bound
  // element and call doCommand on it.
  nsCOMPtr<nsIController> controller;

  nsCOMPtr<nsPIDOMWindowOuter> privateWindow;
  nsCOMPtr<nsPIWindowRoot> windowRoot =
      nsPIWindowRoot::FromEventTargetOrNull(aTarget);
  if (windowRoot) {
    privateWindow = windowRoot->GetWindow();
  } else {
    privateWindow = nsPIDOMWindowOuter::FromEventTargetOrNull(aTarget);
    if (!privateWindow) {
      nsCOMPtr<dom::Document> doc;
      // XXXbz sXBL/XBL2 issue -- this should be the "scope doc" or
      // something... whatever we use when wrapping DOM nodes
      // normally.  It's not clear that the owner doc is the right
      // thing.
      if (nsIContent* content = nsIContent::FromEventTargetOrNull(aTarget)) {
        doc = content->OwnerDoc();
      }

      if (!doc) {
        if (nsINode* node = nsINode::FromEventTargetOrNull(aTarget)) {
          if (node->IsDocument()) {
            doc = node->AsDocument();
          }
        }
      }

      if (!doc) {
        return NS_ERROR_FAILURE;
      }

      privateWindow = doc->GetWindow();
      if (!privateWindow) {
        return NS_ERROR_FAILURE;
      }
    }

    windowRoot = privateWindow->GetTopWindowRoot();
  }

  NS_LossyConvertUTF16toASCII command(mCommand);
  if (windowRoot) {
    // If user tries to do something, user must try to do it in visible window.
    // So, let's retrieve controller of visible window.
    windowRoot->GetControllerForCommand(command.get(), true,
                                        getter_AddRefs(controller));
  } else {
    controller =
        GetController(aTarget);  // We're attached to the receiver possibly.
  }

  // We are the default action for this command.
  // Stop any other default action from executing.
  aEvent->PreventDefault();

  if (mEventName == nsGkAtoms::keypress &&
      mDetail == dom::KeyboardEvent_Binding::DOM_VK_SPACE && mMisc == 1) {
    // get the focused element so that we can pageDown only at
    // certain times.

    nsCOMPtr<nsPIDOMWindowOuter> windowToCheck;
    if (windowRoot) {
      windowToCheck = windowRoot->GetWindow();
    } else {
      windowToCheck = privateWindow->GetPrivateRoot();
    }

    nsCOMPtr<nsIContent> focusedContent;
    if (windowToCheck) {
      nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
      focusedContent = nsFocusManager::GetFocusedDescendant(
          windowToCheck, nsFocusManager::eIncludeAllDescendants,
          getter_AddRefs(focusedWindow));
    }

    // If the focus is in an editable region, don't scroll.
    if (focusedContent && focusedContent->IsEditable()) {
      return NS_OK;
    }

    // If the focus is in a form control, don't scroll.
    for (nsIContent* c = focusedContent; c; c = c->GetParent()) {
      nsCOMPtr<nsIFormControl> formControl = do_QueryInterface(c);
      if (formControl) {
        return NS_OK;
      }
    }
  }

  if (controller) {
    controller->DoCommand(command.get());
  }

  return NS_OK;
}

nsresult KeyEventHandler::DispatchXULKeyCommand(dom::Event* aEvent) {
  nsCOMPtr<dom::Element> handlerElement = GetHandlerElement();
  NS_ENSURE_STATE(handlerElement);
  if (handlerElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled,
                                  nsGkAtoms::_true, eCaseMatters)) {
    // Don't dispatch command events for disabled keys.
    return NS_OK;
  }

  aEvent->PreventDefault();

  // Copy the modifiers from the key event.
  RefPtr<dom::KeyboardEvent> domKeyboardEvent = aEvent->AsKeyboardEvent();
  if (!domKeyboardEvent) {
    NS_ERROR("Trying to execute a key handler for a non-key event!");
    return NS_ERROR_FAILURE;
  }

  // XXX We should use mozilla::Modifiers for supporting all modifiers.

  bool isAlt = domKeyboardEvent->AltKey();
  bool isControl = domKeyboardEvent->CtrlKey();
  bool isShift = domKeyboardEvent->ShiftKey();
  bool isMeta = domKeyboardEvent->MetaKey();

  nsContentUtils::DispatchXULCommand(handlerElement, true, nullptr, nullptr,
                                     isControl, isAlt, isShift, isMeta);
  return NS_OK;
}

Modifiers KeyEventHandler::GetModifiers() const {
  Modifiers modifiers = 0;

  if (mKeyMask & cMeta) {
    modifiers |= MODIFIER_META;
  }
  if (mKeyMask & cOS) {
    modifiers |= MODIFIER_OS;
  }
  if (mKeyMask & cShift) {
    modifiers |= MODIFIER_SHIFT;
  }
  if (mKeyMask & cAlt) {
    modifiers |= MODIFIER_ALT;
  }
  if (mKeyMask & cControl) {
    modifiers |= MODIFIER_CONTROL;
  }

  return modifiers;
}

Modifiers KeyEventHandler::GetModifiersMask() const {
  Modifiers modifiersMask = 0;

  if (mKeyMask & cMetaMask) {
    modifiersMask |= MODIFIER_META;
  }
  if (mKeyMask & cOSMask) {
    modifiersMask |= MODIFIER_OS;
  }
  if (mKeyMask & cShiftMask) {
    modifiersMask |= MODIFIER_SHIFT;
  }
  if (mKeyMask & cAltMask) {
    modifiersMask |= MODIFIER_ALT;
  }
  if (mKeyMask & cControlMask) {
    modifiersMask |= MODIFIER_CONTROL;
  }

  return modifiersMask;
}

already_AddRefed<nsIController> KeyEventHandler::GetController(
    dom::EventTarget* aTarget) {
  if (!aTarget) {
    return nullptr;
  }

  // XXX Fix this so there's a generic interface that describes controllers,
  // This code should have no special knowledge of what objects might have
  // controllers.
  nsCOMPtr<nsIControllers> controllers;

  if (nsIContent* targetContent = nsIContent::FromEventTarget(aTarget)) {
    RefPtr<nsXULElement> xulElement = nsXULElement::FromNode(targetContent);
    if (xulElement) {
      controllers = xulElement->GetControllers(IgnoreErrors());
    }

    if (!controllers) {
      dom::HTMLTextAreaElement* htmlTextArea =
          dom::HTMLTextAreaElement::FromNode(targetContent);
      if (htmlTextArea) {
        htmlTextArea->GetControllers(getter_AddRefs(controllers));
      }
    }

    if (!controllers) {
      dom::HTMLInputElement* htmlInputElement =
          dom::HTMLInputElement::FromNode(targetContent);
      if (htmlInputElement) {
        htmlInputElement->GetControllers(getter_AddRefs(controllers));
      }
    }
  }

  if (!controllers) {
    if (nsCOMPtr<nsPIDOMWindowOuter> domWindow =
            nsPIDOMWindowOuter::FromEventTarget(aTarget)) {
      domWindow->GetControllers(getter_AddRefs(controllers));
    }
  }

  // Return the first controller.
  // XXX This code should be checking the command name and using supportscommand
  // and iscommandenabled.
  nsCOMPtr<nsIController> controller;
  if (controllers) {
    controllers->GetControllerAt(0, getter_AddRefs(controller));
  }

  return controller.forget();
}

bool KeyEventHandler::KeyEventMatched(
    dom::KeyboardEvent* aDomKeyboardEvent, uint32_t aCharCode,
    const IgnoreModifierState& aIgnoreModifierState) {
  if (mDetail != -1) {
    // Get the keycode or charcode of the key event.
    uint32_t code;

    if (mMisc) {
      if (aCharCode) {
        code = aCharCode;
      } else {
        code = aDomKeyboardEvent->CharCode();
      }
      if (IS_IN_BMP(code)) {
        code = ToLowerCase(char16_t(code));
      }
    } else {
      code = aDomKeyboardEvent->KeyCode();
    }

    if (code != static_cast<uint32_t>(mDetail)) {
      return false;
    }
  }

  return ModifiersMatchMask(aDomKeyboardEvent, aIgnoreModifierState);
}

struct keyCodeData {
  const char* str;
  uint16_t strlength;
  uint16_t keycode;
};

// All of these must be uppercase, since the function below does
// case-insensitive comparison by converting to uppercase.
// XXX: be sure to check this periodically for new symbol additions!
static const keyCodeData gKeyCodes[] = {

#define NS_DEFINE_VK(aDOMKeyName, aDOMKeyCode) \
  {#aDOMKeyName, sizeof(#aDOMKeyName) - 1, aDOMKeyCode},
#include "mozilla/VirtualKeyCodeList.h"
#undef NS_DEFINE_VK

    {nullptr, 0, 0}};

int32_t KeyEventHandler::GetMatchingKeyCode(const nsAString& aKeyName) {
  nsAutoCString keyName;
  LossyCopyUTF16toASCII(aKeyName, keyName);
  ToUpperCase(keyName);  // We want case-insensitive comparison with data
                         // stored as uppercase.

  uint32_t keyNameLength = keyName.Length();
  const char* keyNameStr = keyName.get();
  for (unsigned long i = 0; i < ArrayLength(gKeyCodes) - 1; ++i) {
    if (keyNameLength == gKeyCodes[i].strlength &&
        !nsCRT::strcmp(gKeyCodes[i].str, keyNameStr)) {
      return gKeyCodes[i].keycode;
    }
  }

  return 0;
}

int32_t KeyEventHandler::KeyToMask(int32_t key) {
  switch (key) {
    case dom::KeyboardEvent_Binding::DOM_VK_META:
      return cMeta | cMetaMask;

    case dom::KeyboardEvent_Binding::DOM_VK_WIN:
      return cOS | cOSMask;

    case dom::KeyboardEvent_Binding::DOM_VK_ALT:
      return cAlt | cAltMask;

    case dom::KeyboardEvent_Binding::DOM_VK_CONTROL:
    default:
      return cControl | cControlMask;
  }
}

// static
int32_t KeyEventHandler::AccelKeyMask() {
  switch (WidgetInputEvent::AccelModifier()) {
    case MODIFIER_ALT:
      return KeyToMask(dom::KeyboardEvent_Binding::DOM_VK_ALT);
    case MODIFIER_CONTROL:
      return KeyToMask(dom::KeyboardEvent_Binding::DOM_VK_CONTROL);
    case MODIFIER_META:
      return KeyToMask(dom::KeyboardEvent_Binding::DOM_VK_META);
    case MODIFIER_OS:
      return KeyToMask(dom::KeyboardEvent_Binding::DOM_VK_WIN);
    default:
      MOZ_CRASH("Handle the new result of WidgetInputEvent::AccelModifier()");
      return 0;
  }
}

void KeyEventHandler::GetEventType(nsAString& aEvent) {
  nsCOMPtr<dom::Element> handlerElement = GetHandlerElement();
  if (!handlerElement) {
    aEvent.Truncate();
    return;
  }
  handlerElement->GetAttr(kNameSpaceID_None, nsGkAtoms::event, aEvent);

  if (aEvent.IsEmpty() && mIsXULKey) {
    // If no type is specified for a XUL <key> element, let's assume that we're
    // "keypress".
    aEvent.AssignLiteral("keypress");
  }
}

void KeyEventHandler::ConstructPrototype(dom::Element* aKeyElement,
                                         const char16_t* aEvent,
                                         const char16_t* aCommand,
                                         const char16_t* aKeyCode,
                                         const char16_t* aCharCode,
                                         const char16_t* aModifiers) {
  mDetail = -1;
  mMisc = 0;
  mKeyMask = 0;
  nsAutoString modifiers;

  if (mIsXULKey) {
    nsWeakPtr weak = do_GetWeakReference(aKeyElement);
    if (!weak) {
      return;
    }
    weak.swap(mHandlerElement);

    nsAutoString event;
    GetEventType(event);
    if (event.IsEmpty()) {
      return;
    }
    mEventName = NS_Atomize(event);

    aKeyElement->GetAttr(kNameSpaceID_None, nsGkAtoms::modifiers, modifiers);
  } else {
    mCommand = ToNewUnicode(nsDependentString(aCommand));
    mEventName = NS_Atomize(aEvent);
    modifiers = aModifiers;
  }

  BuildModifiers(modifiers);

  nsAutoString key(aCharCode);
  if (key.IsEmpty()) {
    if (mIsXULKey) {
      aKeyElement->GetAttr(kNameSpaceID_None, nsGkAtoms::key, key);
      if (key.IsEmpty()) {
        aKeyElement->GetAttr(kNameSpaceID_None, nsGkAtoms::charcode, key);
      }
    }
  }

  if (!key.IsEmpty()) {
    if (mKeyMask == 0) {
      mKeyMask = cAllModifiers;
    }
    ToLowerCase(key);

    // We have a charcode.
    mMisc = 1;
    mDetail = key[0];
    const uint8_t GTK2Modifiers = cShift | cControl | cShiftMask | cControlMask;
    if (mIsXULKey && (mKeyMask & GTK2Modifiers) == GTK2Modifiers &&
        modifiers.First() != char16_t(',') &&
        (mDetail == 'u' || mDetail == 'U')) {
      ReportKeyConflict(key.get(), modifiers.get(), aKeyElement,
                        "GTK2Conflict2");
    }
    const uint8_t WinModifiers = cControl | cAlt | cControlMask | cAltMask;
    if (mIsXULKey && (mKeyMask & WinModifiers) == WinModifiers &&
        modifiers.First() != char16_t(',') &&
        (('A' <= mDetail && mDetail <= 'Z') ||
         ('a' <= mDetail && mDetail <= 'z'))) {
      ReportKeyConflict(key.get(), modifiers.get(), aKeyElement,
                        "WinConflict2");
    }
  } else {
    key.Assign(aKeyCode);
    if (mIsXULKey) {
      aKeyElement->GetAttr(kNameSpaceID_None, nsGkAtoms::keycode, key);
    }

    if (!key.IsEmpty()) {
      if (mKeyMask == 0) {
        mKeyMask = cAllModifiers;
      }
      mDetail = GetMatchingKeyCode(key);
    }
  }
}

void KeyEventHandler::BuildModifiers(nsAString& aModifiers) {
  if (!aModifiers.IsEmpty()) {
    mKeyMask = cAllModifiers;
    char* str = ToNewCString(aModifiers);
    char* newStr;
    char* token = nsCRT::strtok(str, ", \t", &newStr);
    while (token != nullptr) {
      if (strcmp(token, "shift") == 0) {
        mKeyMask |= cShift | cShiftMask;
      } else if (strcmp(token, "alt") == 0) {
        mKeyMask |= cAlt | cAltMask;
      } else if (strcmp(token, "meta") == 0) {
        mKeyMask |= cMeta | cMetaMask;
      } else if (strcmp(token, "os") == 0) {
        mKeyMask |= cOS | cOSMask;
      } else if (strcmp(token, "control") == 0) {
        mKeyMask |= cControl | cControlMask;
      } else if (strcmp(token, "accel") == 0) {
        mKeyMask |= AccelKeyMask();
      } else if (strcmp(token, "access") == 0) {
        mKeyMask |= KeyToMask(kMenuAccessKey);
      } else if (strcmp(token, "any") == 0) {
        mKeyMask &= ~(mKeyMask << 5);
      }

      token = nsCRT::strtok(newStr, ", \t", &newStr);
    }

    free(str);
  }
}

void KeyEventHandler::ReportKeyConflict(const char16_t* aKey,
                                        const char16_t* aModifiers,
                                        dom::Element* aKeyElement,
                                        const char* aMessageName) {
  nsCOMPtr<dom::Document> doc = aKeyElement->OwnerDoc();

  nsAutoString id;
  aKeyElement->GetAttr(kNameSpaceID_None, nsGkAtoms::id, id);
  AutoTArray<nsString, 3> params;
  params.AppendElement(aKey);
  params.AppendElement(aModifiers);
  params.AppendElement(id);
  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  "Key dom::Event Handler"_ns, doc,
                                  nsContentUtils::eDOM_PROPERTIES, aMessageName,
                                  params, nullptr, u""_ns, 0);
}

bool KeyEventHandler::ModifiersMatchMask(
    dom::UIEvent* aEvent, const IgnoreModifierState& aIgnoreModifierState) {
  WidgetInputEvent* inputEvent = aEvent->WidgetEventPtr()->AsInputEvent();
  NS_ENSURE_TRUE(inputEvent, false);

  if (mKeyMask & cMetaMask) {
    if (inputEvent->IsMeta() != ((mKeyMask & cMeta) != 0)) {
      return false;
    }
  }

  if ((mKeyMask & cOSMask) && !aIgnoreModifierState.mOS) {
    if (inputEvent->IsOS() != ((mKeyMask & cOS) != 0)) {
      return false;
    }
  }

  if (mKeyMask & cShiftMask && !aIgnoreModifierState.mShift) {
    if (inputEvent->IsShift() != ((mKeyMask & cShift) != 0)) {
      return false;
    }
  }

  if (mKeyMask & cAltMask) {
    if (inputEvent->IsAlt() != ((mKeyMask & cAlt) != 0)) {
      return false;
    }
  }

  if (mKeyMask & cControlMask) {
    if (inputEvent->IsControl() != ((mKeyMask & cControl) != 0)) {
      return false;
    }
  }

  return true;
}

size_t KeyEventHandler::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  size_t n = 0;
  for (const KeyEventHandler* handler = this; handler;
       handler = handler->mNextHandler) {
    n += aMallocSizeOf(handler);
    if (!mIsXULKey) {
      n += aMallocSizeOf(handler->mCommand);
    }
  }
  return n;
}

}  // namespace mozilla

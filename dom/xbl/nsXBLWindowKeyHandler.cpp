/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsXBLPrototypeHandler.h"
#include "nsXBLWindowKeyHandler.h"
#include "nsIContent.h"
#include "nsIAtom.h"
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
#include "nsIPresShell.h"
#include "mozilla/EventStateManager.h"
#include "nsISelectionController.h"
#include "mozilla/Preferences.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "nsIEditor.h"
#include "nsIHTMLEditor.h"
#include "nsIDOMDocument.h"

using namespace mozilla;
using namespace mozilla::dom;

class nsXBLSpecialDocInfo : public nsIObserver
{
public:
  nsRefPtr<nsXBLDocumentInfo> mHTMLBindings;
  nsRefPtr<nsXBLDocumentInfo> mUserHTMLBindings;

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

  const nsAdoptingCString& userHTMLBindingStr =
    Preferences::GetCString("dom.userHTMLBindings.uri");
  if (!userHTMLBindingStr.IsEmpty()) {
    NS_NewURI(getter_AddRefs(bindingURI), userHTMLBindingStr);
    if (!bindingURI) {
      return;
    }

    xblService->LoadBindingDocumentInfo(nullptr, nullptr,
                                        bindingURI,
                                        nullptr,
                                        true, 
                                        getter_AddRefs(mUserHTMLBindings));
  }
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
nsXBLSpecialDocInfo* nsXBLWindowKeyHandler::sXBLSpecialDocInfo = nullptr;
uint32_t nsXBLWindowKeyHandler::sRefCnt = 0;

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
    NS_IF_RELEASE(sXBLSpecialDocInfo);
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

    if (key->NodeInfo()->Equals(nsGkAtoms::key, kNameSpaceID_XUL)) {
      // Check whether the key element has empty value at key/char attribute.
      // Such element is used by localizers for alternative shortcut key
      // definition on the locale. See bug 426501.
      nsAutoString valKey, valCharCode, valKeyCode;
      bool attrExists =
        key->GetAttr(kNameSpaceID_None, nsGkAtoms::key, valKey) ||
        key->GetAttr(kNameSpaceID_None, nsGkAtoms::charcode, valCharCode) ||
        key->GetAttr(kNameSpaceID_None, nsGkAtoms::keycode, valKeyCode);
      if (attrExists &&
          valKey.IsEmpty() && valCharCode.IsEmpty() && valKeyCode.IsEmpty())
        continue;

      nsXBLPrototypeHandler* handler = new nsXBLPrototypeHandler(key);

      handler->SetNextHandler(*aResult);
      *aResult = handler;
    }
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
    if (!sXBLSpecialDocInfo) {
      sXBLSpecialDocInfo = new nsXBLSpecialDocInfo();
      NS_ADDREF(sXBLSpecialDocInfo);
    }
    sXBLSpecialDocInfo->LoadDocInfo();

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
nsXBLWindowKeyHandler::WalkHandlers(nsIDOMKeyEvent* aKeyEvent, nsIAtom* aEventType)
{
  bool prevent;
  aKeyEvent->GetDefaultPrevented(&prevent);
  if (prevent)
    return NS_OK;

  bool trustedEvent = false;
  // Don't process the event if it was not dispatched from a trusted source
  aKeyEvent->GetIsTrusted(&trustedEvent);

  if (!trustedEvent)
    return NS_OK;

  nsresult rv = EnsureHandlers();
  NS_ENSURE_SUCCESS(rv, rv);

  bool isDisabled;
  nsCOMPtr<Element> el = GetElement(&isDisabled);
  if (!el) {
    if (mUserHandler) {
      WalkHandlersInternal(aKeyEvent, aEventType, mUserHandler, true);
      aKeyEvent->GetDefaultPrevented(&prevent);
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

NS_IMETHODIMP
nsXBLWindowKeyHandler::HandleEvent(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMKeyEvent> keyEvent(do_QueryInterface(aEvent));
  NS_ENSURE_TRUE(keyEvent, NS_ERROR_INVALID_ARG);

  uint16_t eventPhase;
  aEvent->GetEventPhase(&eventPhase);
  if (eventPhase == nsIDOMEvent::CAPTURING_PHASE) {
    HandleEventOnCapture(keyEvent);
    return NS_OK;
  }

  nsAutoString eventType;
  aEvent->GetType(eventType);
  nsCOMPtr<nsIAtom> eventTypeAtom = do_GetAtom(eventType);
  NS_ENSURE_TRUE(eventTypeAtom, NS_ERROR_OUT_OF_MEMORY);

  return WalkHandlers(keyEvent, eventTypeAtom);
}

void
nsXBLWindowKeyHandler::HandleEventOnCapture(nsIDOMKeyEvent* aEvent)
{
  WidgetKeyboardEvent* widgetEvent =
    aEvent->GetInternalNSEvent()->AsKeyboardEvent();

  if (widgetEvent->mFlags.mNoCrossProcessBoundaryForwarding) {
    return;
  }

  nsCOMPtr<mozilla::dom::Element> originalTarget =
    do_QueryInterface(aEvent->GetInternalNSEvent()->originalTarget);
  if (!EventStateManager::IsRemoteTarget(originalTarget)) {
    return;
  }

  bool aReservedForChrome = false;
  if (!HasHandlerForEvent(aEvent, &aReservedForChrome)) {
    return;
  }

  if (aReservedForChrome) {
    // For reserved commands (such as Open New Tab), we don't to wait for
    // the content to answer (so mWantReplyFromContentProcess remains false),
    // neither to give a chance for content to override its behavior.
    widgetEvent->mFlags.mNoCrossProcessBoundaryForwarding = true;
  } else {
    // Inform the child process that this is a event that we want a reply
    // from.
    widgetEvent->mFlags.mWantReplyFromContentProcess = true;

    // If this event hadn't been marked as mNoCrossProcessBoundaryForwarding
    // yet, it means it wasn't processed by content. We'll not call any
    // of the handlers at this moment, and will wait for the event to be
    // redispatched with mNoCrossProcessBoundaryForwarding = 1 to process it.
    aEvent->StopPropagation();
  }
}

//
// EventMatched
//
// See if the given handler cares about this particular key event
//
bool
nsXBLWindowKeyHandler::EventMatched(
                         nsXBLPrototypeHandler* aHandler,
                         nsIAtom* aEventType,
                         nsIDOMKeyEvent* aEvent,
                         uint32_t aCharCode,
                         const IgnoreModifierState& aIgnoreModifierState)
{
  return aHandler->KeyEventMatched(aEventType, aEvent, aCharCode,
                                   aIgnoreModifierState);
}

bool
nsXBLWindowKeyHandler::IsHTMLEditableFieldFocused()
{
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm)
    return false;

  nsCOMPtr<nsIDOMWindow> focusedWindow;
  fm->GetFocusedWindow(getter_AddRefs(focusedWindow));
  if (!focusedWindow)
    return false;

  nsCOMPtr<nsPIDOMWindow> piwin(do_QueryInterface(focusedWindow));
  nsIDocShell *docShell = piwin->GetDocShell();
  if (!docShell) {
    return false;
  }

  nsCOMPtr<nsIEditor> editor;
  docShell->GetEditor(getter_AddRefs(editor));
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(editor);
  if (!htmlEditor) {
    return false;
  }

  nsCOMPtr<nsIDOMDocument> domDocument;
  editor->GetDocument(getter_AddRefs(domDocument));
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDocument);
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
                                            nsIAtom* aEventType, 
                                            nsXBLPrototypeHandler* aHandler,
                                            bool aExecute,
                                            bool* aOutReservedForChrome)
{
  nsAutoTArray<nsShortcutCandidate, 10> accessKeys;
  nsContentUtils::GetAccelKeyCandidates(aKeyEvent, accessKeys);

  if (accessKeys.IsEmpty()) {
    return WalkHandlersAndExecute(aKeyEvent, aEventType, aHandler,
                                  0, IgnoreModifierState(),
                                  aExecute, aOutReservedForChrome);
  }

  for (uint32_t i = 0; i < accessKeys.Length(); ++i) {
    nsShortcutCandidate &key = accessKeys[i];
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
                         nsIAtom* aEventType,
                         nsXBLPrototypeHandler* aHandler,
                         uint32_t aCharCode,
                         const IgnoreModifierState& aIgnoreModifierState,
                         bool aExecute,
                         bool* aOutReservedForChrome)
{
  nsresult rv;

  // Try all of the handlers until we find one that matches the event.
  for (nsXBLPrototypeHandler *currHandler = aHandler; currHandler;
       currHandler = currHandler->GetNextHandler()) {
    bool stopped = aKeyEvent->IsDispatchStopped();
    if (stopped) {
      // The event is finished, don't execute any more handlers
      return false;
    }

    if (!EventMatched(currHandler, aEventType, aKeyEvent,
                      aCharCode, aIgnoreModifierState)) {
      continue;  // try the next one
    }

    // Before executing this handler, check that it's not disabled,
    // and that it has something to do (oncommand of the <key> or its
    // <command> is non-empty).
    nsCOMPtr<nsIContent> elt = currHandler->GetHandlerElement();
    nsCOMPtr<Element> commandElt;

    // See if we're in a XUL doc.
    nsCOMPtr<Element> el = GetElement();
    if (el && elt) {
      // We are.  Obtain our command attribute.
      nsAutoString command;
      elt->GetAttr(kNameSpaceID_None, nsGkAtoms::command, command);
      if (!command.IsEmpty()) {
        // Locate the command element in question.  Note that we
        // know "elt" is in a doc if we're dealing with it here.
        NS_ASSERTION(elt->IsInDoc(), "elt must be in document");
        nsIDocument *doc = elt->GetCurrentDoc();
        if (doc)
          commandElt = do_QueryInterface(doc->GetElementById(command));

        if (!commandElt) {
          NS_ERROR("A XUL <key> is observing a command that doesn't exist. Unable to execute key binding!");
          continue;
        }
      }
    }

    if (!commandElt) {
      commandElt = do_QueryInterface(elt);
    }

    if (commandElt) {
      nsAutoString value;
      commandElt->GetAttribute(NS_LITERAL_STRING("disabled"), value);
      if (value.EqualsLiteral("true")) {
        continue;  // this handler is disabled, try the next one
      }

      // Check that there is an oncommand handler
      commandElt->GetAttribute(NS_LITERAL_STRING("oncommand"), value);
      if (value.IsEmpty()) {
        continue;  // nothing to do
      }

      if (aOutReservedForChrome) {
        // The caller wants to know if this is a reserved command
        commandElt->GetAttribute(NS_LITERAL_STRING("reserved"), value);
        *aOutReservedForChrome = value.EqualsLiteral("true");
      }
    }

    nsCOMPtr<EventTarget> piTarget;
    nsCOMPtr<Element> element = GetElement();
    if (element) {
      piTarget = commandElt;
    } else {
      piTarget = mTarget;
    }

    if (!aExecute) {
      return true;
    }

    rv = currHandler->ExecuteHandler(piTarget, aKeyEvent);
    if (NS_SUCCEEDED(rv)) {
      return true;
    }
  }

#ifdef XP_WIN
  // Windows native applications ignore Windows-Logo key state when checking
  // shortcut keys even if the key is pressed.  Therefore, if there is no
  // shortcut key which exactly matches current modifier state, we should
  // retry to look for a shortcut key without the Windows-Logo key press.
  if (!aIgnoreModifierState.mOS) {
    WidgetKeyboardEvent* keyEvent =
      aKeyEvent->GetInternalNSEvent()->AsKeyboardEvent();
    if (keyEvent && keyEvent->IsOS()) {
      IgnoreModifierState ignoreModifierState(aIgnoreModifierState);
      ignoreModifierState.mOS = true;
      return WalkHandlersAndExecute(aKeyEvent, aEventType, aHandler, aCharCode,
                                    ignoreModifierState, aExecute);
    }
  }
#endif

  return false;
}

bool
nsXBLWindowKeyHandler::HasHandlerForEvent(nsIDOMKeyEvent* aEvent,
                                          bool* aOutReservedForChrome)
{
  if (!aEvent->InternalDOMEvent()->IsTrusted()) {
    return false;
  }

  nsresult rv = EnsureHandlers();
  NS_ENSURE_SUCCESS(rv, false);

  bool isDisabled;
  nsCOMPtr<Element> el = GetElement(&isDisabled);
  if (el && isDisabled) {
    return false;
  }

  nsAutoString eventType;
  aEvent->GetType(eventType);
  nsCOMPtr<nsIAtom> eventTypeAtom = do_GetAtom(eventType);
  NS_ENSURE_TRUE(eventTypeAtom, false);

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

///////////////////////////////////////////////////////////////////////////////////

already_AddRefed<nsXBLWindowKeyHandler>
NS_NewXBLWindowKeyHandler(nsIDOMElement* aElement, EventTarget* aTarget)
{
  nsRefPtr<nsXBLWindowKeyHandler> result =
    new nsXBLWindowKeyHandler(aElement, aTarget);
  return result.forget();
}

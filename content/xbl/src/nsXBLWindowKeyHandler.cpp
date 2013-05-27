/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsINativeKeyBindings.h"
#include "nsIController.h"
#include "nsIControllers.h"
#include "nsFocusManager.h"
#include "nsPIWindowRoot.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsContentUtils.h"
#include "nsXBLPrototypeBinding.h"
#include "nsPIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIPresShell.h"
#include "nsISelectionController.h"
#include "nsGUIEvent.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/Element.h"
#include "nsEventStateManager.h"

using namespace mozilla;
using namespace mozilla::dom;

static nsINativeKeyBindings *sNativeEditorBindings = nullptr;

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

  virtual ~nsXBLSpecialDocInfo() {}

};

const char nsXBLSpecialDocInfo::sHTMLBindingStr[] =
  "chrome://global/content/platformHTMLBindings.xml";

NS_IMPL_ISUPPORTS1(nsXBLSpecialDocInfo, nsIObserver)

NS_IMETHODIMP
nsXBLSpecialDocInfo::Observe(nsISupports* aSubject,
                             const char* aTopic,
                             const PRUnichar* aData)
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
    type.Append("User");
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

NS_IMPL_ISUPPORTS1(nsXBLWindowKeyHandler,
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

      if (!handler)
        return;

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
nsXBLWindowKeyHandler::EnsureHandlers(bool *aIsEditor)
{
  nsCOMPtr<Element> el = GetElement();
  NS_ENSURE_STATE(!mWeakPtrForElement || el);
  if (el) {
    // We are actually a XUL <keyset>.
    if (aIsEditor)
      *aIsEditor = false;

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
    bool isEditor = IsEditor();
    if (isEditor) {
      sXBLSpecialDocInfo->GetAllHandlers("editor", &mHandler, &mUserHandler);
    }
    else {
      sXBLSpecialDocInfo->GetAllHandlers("browser", &mHandler, &mUserHandler);
    }

    if (aIsEditor)
      *aIsEditor = isEditor;
  }

  return NS_OK;
}

static nsINativeKeyBindings*
GetEditorKeyBindings()
{
  static bool noBindings = false;
  if (!sNativeEditorBindings && !noBindings) {
    CallGetService(NS_NATIVEKEYBINDINGS_CONTRACTID_PREFIX "editor",
                   &sNativeEditorBindings);

    if (!sNativeEditorBindings) {
      noBindings = true;
    }
  }

  return sNativeEditorBindings;
}

static void
DoCommandCallback(const char *aCommand, void *aData)
{
  nsIControllers *controllers = static_cast<nsIControllers*>(aData);
  if (controllers) {
    nsCOMPtr<nsIController> controller;
    controllers->GetControllerForCommand(aCommand, getter_AddRefs(controller));
    if (controller) {
      controller->DoCommand(aCommand);
    }
  }
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

  bool isEditor;
  nsresult rv = EnsureHandlers(&isEditor);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<Element> el = GetElement();
  if (!el) {
    if (mUserHandler) {
      WalkHandlersInternal(aKeyEvent, aEventType, mUserHandler);
      aKeyEvent->GetDefaultPrevented(&prevent);
      if (prevent)
        return NS_OK; // Handled by the user bindings. Our work here is done.
    }
  }

  nsCOMPtr<nsIContent> content = do_QueryInterface(el);
  // skip keysets that are disabled
  if (content && content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::disabled,
                                      nsGkAtoms::_true, eCaseMatters)) {
    return NS_OK;
  }

  WalkHandlersInternal(aKeyEvent, aEventType, mHandler);

  if (isEditor && GetEditorKeyBindings()) {
    nsNativeKeyEvent nativeEvent;
    // get the DOM window we're attached to
    nsCOMPtr<nsIControllers> controllers;
    nsCOMPtr<nsPIWindowRoot> root = do_QueryInterface(mTarget);
    if (root) {
      root->GetControllers(getter_AddRefs(controllers));
    }

    bool handled = false;
    if (aEventType == nsGkAtoms::keypress) {
      if (nsContentUtils::DOMEventToNativeKeyEvent(aKeyEvent, &nativeEvent, true))
        handled = sNativeEditorBindings->KeyPress(nativeEvent,
                                                  DoCommandCallback, controllers);
    } else if (aEventType == nsGkAtoms::keyup) {
      if (nsContentUtils::DOMEventToNativeKeyEvent(aKeyEvent, &nativeEvent, false))
        handled = sNativeEditorBindings->KeyUp(nativeEvent,
                                               DoCommandCallback, controllers);
    } else {
      NS_ASSERTION(aEventType == nsGkAtoms::keydown, "unknown key event type");
      if (nsContentUtils::DOMEventToNativeKeyEvent(aKeyEvent, &nativeEvent, false))
        handled = sNativeEditorBindings->KeyDown(nativeEvent,
                                                 DoCommandCallback, controllers);
    }

    if (handled)
      aKeyEvent->PreventDefault();

  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsXBLWindowKeyHandler::HandleEvent(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMKeyEvent> keyEvent(do_QueryInterface(aEvent));
  NS_ENSURE_TRUE(keyEvent, NS_ERROR_INVALID_ARG);

  nsAutoString eventType;
  aEvent->GetType(eventType);
  nsCOMPtr<nsIAtom> eventTypeAtom = do_GetAtom(eventType);
  NS_ENSURE_TRUE(eventTypeAtom, NS_ERROR_OUT_OF_MEMORY);

  if (!mWeakPtrForElement) {
    nsCOMPtr<mozilla::dom::Element> originalTarget =
      do_QueryInterface(aEvent->GetInternalNSEvent()->originalTarget);
    if (nsEventStateManager::IsRemoteTarget(originalTarget)) {
      return NS_OK;
    }
  }

  return WalkHandlers(keyEvent, eventTypeAtom);
}

//
// EventMatched
//
// See if the given handler cares about this particular key event
//
bool
nsXBLWindowKeyHandler::EventMatched(nsXBLPrototypeHandler* inHandler,
                                    nsIAtom* inEventType,
                                    nsIDOMKeyEvent* inEvent,
                                    uint32_t aCharCode, bool aIgnoreShiftKey)
{
  return inHandler->KeyEventMatched(inEventType, inEvent, aCharCode,
                                    aIgnoreShiftKey);
}

/* static */ void
nsXBLWindowKeyHandler::ShutDown()
{
  NS_IF_RELEASE(sNativeEditorBindings);
}

//
// IsEditor
//
// Determine if the document we're working with is Editor or Browser
//
bool
nsXBLWindowKeyHandler::IsEditor()
{
  // XXXndeakin even though this is only used for key events which should be
  // going to the focused frame anyway, this doesn't seem like the right way
  // to determine if something is an editor.
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm)
    return false;

  nsCOMPtr<nsIDOMWindow> focusedWindow;
  fm->GetFocusedWindow(getter_AddRefs(focusedWindow));
  if (!focusedWindow)
    return false;

  nsCOMPtr<nsPIDOMWindow> piwin(do_QueryInterface(focusedWindow));
  nsIDocShell *docShell = piwin->GetDocShell();
  nsCOMPtr<nsIPresShell> presShell;
  if (docShell)
    presShell = docShell->GetPresShell();

  if (presShell) {
    return presShell->GetSelectionFlags() == nsISelectionDisplay::DISPLAY_ALL;
  }

  return false;
}

//
// WalkHandlersInternal and WalkHandlersAndExecute
//
// Given a particular DOM event and a pointer to the first handler in the list,
// scan through the list to find something to handle the event and then make it
// so.
//
nsresult
nsXBLWindowKeyHandler::WalkHandlersInternal(nsIDOMKeyEvent* aKeyEvent,
                                            nsIAtom* aEventType, 
                                            nsXBLPrototypeHandler* aHandler)
{
  nsAutoTArray<nsShortcutCandidate, 10> accessKeys;
  nsContentUtils::GetAccelKeyCandidates(aKeyEvent, accessKeys);

  if (accessKeys.IsEmpty()) {
    WalkHandlersAndExecute(aKeyEvent, aEventType, aHandler, 0, false);
    return NS_OK;
  }

  for (uint32_t i = 0; i < accessKeys.Length(); ++i) {
    nsShortcutCandidate &key = accessKeys[i];
    if (WalkHandlersAndExecute(aKeyEvent, aEventType, aHandler,
                               key.mCharCode, key.mIgnoreShift))
      return NS_OK;
  }
  return NS_OK;
}

bool
nsXBLWindowKeyHandler::WalkHandlersAndExecute(nsIDOMKeyEvent* aKeyEvent,
                                              nsIAtom* aEventType,
                                              nsXBLPrototypeHandler* aHandler,
                                              uint32_t aCharCode,
                                              bool aIgnoreShiftKey)
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
                      aCharCode, aIgnoreShiftKey))
      continue;  // try the next one

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
    }

    nsCOMPtr<EventTarget> piTarget;
    nsCOMPtr<Element> element = GetElement();
    if (element) {
      piTarget = commandElt;
    } else {
      piTarget = mTarget;
    }

    rv = currHandler->ExecuteHandler(piTarget, aKeyEvent);
    if (NS_SUCCEEDED(rv)) {
      return true;
    }
  }

  return false;
}

already_AddRefed<Element>
nsXBLWindowKeyHandler::GetElement()
{
  nsCOMPtr<Element> element = do_QueryReferent(mWeakPtrForElement);
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

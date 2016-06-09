/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TabParent.h"

#include "nsFocusManager.h"

#include "AccessibleCaretEventHub.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsGkAtoms.h"
#include "nsContentUtils.h"
#include "nsIDocument.h"
#include "nsIEditor.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMChromeWindow.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIDOMRange.h"
#include "nsIHTMLDocument.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIFormControl.h"
#include "nsLayoutUtils.h"
#include "nsIPresShell.h"
#include "nsFrameTraversal.h"
#include "nsIWebNavigation.h"
#include "nsCaret.h"
#include "nsIBaseWindow.h"
#include "nsIXULWindow.h"
#include "nsViewManager.h"
#include "nsFrameSelection.h"
#include "mozilla/dom/Selection.h"
#include "nsXULPopupManager.h"
#include "nsMenuPopupFrame.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIPrincipal.h"
#include "nsIObserverService.h"
#include "nsIObjectFrame.h"
#include "nsBindingManager.h"
#include "nsStyleCoord.h"
#include "TabChild.h"
#include "nsFrameLoader.h"
#include "nsNumberControlFrame.h"

#include "mozilla/ContentEvents.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/EventStates.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/unused.h"
#include <algorithm>

#ifdef MOZ_XUL
#include "nsIDOMXULTextboxElement.h"
#include "nsIDOMXULMenuListElement.h"
#endif

#ifdef ACCESSIBILITY
#include "nsAccessibilityService.h"
#endif

#ifndef XP_MACOSX
#include "nsIScriptError.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::widget;

// Two types of focus pr logging are available:
//   'Focus' for normal focus manager calls
//   'FocusNavigation' for tab and document navigation
LazyLogModule gFocusLog("Focus");
LazyLogModule gFocusNavigationLog("FocusNavigation");

#define LOGFOCUS(args) MOZ_LOG(gFocusLog, mozilla::LogLevel::Debug, args)
#define LOGFOCUSNAVIGATION(args) MOZ_LOG(gFocusNavigationLog, mozilla::LogLevel::Debug, args)

#define LOGTAG(log, format, content)                            \
  if (MOZ_LOG_TEST(log, LogLevel::Debug)) {                         \
    nsAutoCString tag(NS_LITERAL_CSTRING("(none)"));            \
    if (content) {                                              \
      content->NodeInfo()->NameAtom()->ToUTF8String(tag);       \
    }                                                           \
    MOZ_LOG(log, LogLevel::Debug, (format, tag.get()));             \
  }

#define LOGCONTENT(format, content) LOGTAG(gFocusLog, format, content)
#define LOGCONTENTNAVIGATION(format, content) LOGTAG(gFocusNavigationLog, format, content)

struct nsDelayedBlurOrFocusEvent
{
  nsDelayedBlurOrFocusEvent(EventMessage aEventMessage,
                            nsIPresShell* aPresShell,
                            nsIDocument* aDocument,
                            EventTarget* aTarget,
                            EventTarget* aRelatedTarget)
    : mPresShell(aPresShell)
    , mDocument(aDocument)
    , mTarget(aTarget)
    , mEventMessage(aEventMessage)
    , mRelatedTarget(aRelatedTarget)
 {
 }

  nsDelayedBlurOrFocusEvent(const nsDelayedBlurOrFocusEvent& aOther)
    : mPresShell(aOther.mPresShell)
    , mDocument(aOther.mDocument)
    , mTarget(aOther.mTarget)
    , mEventMessage(aOther.mEventMessage)
  {
  }

  nsCOMPtr<nsIPresShell> mPresShell;
  nsCOMPtr<nsIDocument> mDocument;
  nsCOMPtr<EventTarget> mTarget;
  EventMessage mEventMessage;
  nsCOMPtr<EventTarget> mRelatedTarget;
};

inline void ImplCycleCollectionUnlink(nsDelayedBlurOrFocusEvent& aField)
{
  aField.mPresShell = nullptr;
  aField.mDocument = nullptr;
  aField.mTarget = nullptr;
  aField.mRelatedTarget = nullptr;
}

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            nsDelayedBlurOrFocusEvent& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  CycleCollectionNoteChild(aCallback, aField.mPresShell.get(), aName, aFlags);
  CycleCollectionNoteChild(aCallback, aField.mDocument.get(), aName, aFlags);
  CycleCollectionNoteChild(aCallback, aField.mTarget.get(), aName, aFlags);
  CycleCollectionNoteChild(aCallback, aField.mRelatedTarget.get(), aName, aFlags);
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsFocusManager)
  NS_INTERFACE_MAP_ENTRY(nsIFocusManager)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIFocusManager)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsFocusManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsFocusManager)

NS_IMPL_CYCLE_COLLECTION(nsFocusManager,
                         mActiveWindow,
                         mFocusedWindow,
                         mFocusedContent,
                         mFirstBlurEvent,
                         mFirstFocusEvent,
                         mWindowBeingLowered,
                         mDelayedBlurFocusEvents,
                         mMouseButtonEventHandlingDocument)

nsFocusManager* nsFocusManager::sInstance = nullptr;
bool nsFocusManager::sMouseFocusesFormControl = false;
bool nsFocusManager::sTestMode = false;

static const char* kObservedPrefs[] = {
  "accessibility.browsewithcaret",
  "accessibility.tabfocus_applies_to_xul",
  "accessibility.mouse_focuses_formcontrol",
  "focusmanager.testmode",
  nullptr
};

nsFocusManager::nsFocusManager()
{ }

nsFocusManager::~nsFocusManager()
{
  Preferences::RemoveObservers(this, kObservedPrefs);

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, "xpcom-shutdown");
  }
}

// static
nsresult
nsFocusManager::Init()
{
  nsFocusManager* fm = new nsFocusManager();
  NS_ADDREF(fm);
  sInstance = fm;

  nsIContent::sTabFocusModelAppliesToXUL =
    Preferences::GetBool("accessibility.tabfocus_applies_to_xul",
                         nsIContent::sTabFocusModelAppliesToXUL);

  sMouseFocusesFormControl =
    Preferences::GetBool("accessibility.mouse_focuses_formcontrol", false);

  sTestMode = Preferences::GetBool("focusmanager.testmode", false);

  Preferences::AddWeakObservers(fm, kObservedPrefs);

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(fm, "xpcom-shutdown", true);
  }

  return NS_OK;
}

// static
void
nsFocusManager::Shutdown()
{
  NS_IF_RELEASE(sInstance);
}

NS_IMETHODIMP
nsFocusManager::Observe(nsISupports *aSubject,
                        const char *aTopic,
                        const char16_t *aData)
{
  if (!nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    nsDependentString data(aData);
    if (data.EqualsLiteral("accessibility.browsewithcaret")) {
      UpdateCaretForCaretBrowsingMode();
    }
    else if (data.EqualsLiteral("accessibility.tabfocus_applies_to_xul")) {
      nsIContent::sTabFocusModelAppliesToXUL =
        Preferences::GetBool("accessibility.tabfocus_applies_to_xul",
                             nsIContent::sTabFocusModelAppliesToXUL);
    }
    else if (data.EqualsLiteral("accessibility.mouse_focuses_formcontrol")) {
      sMouseFocusesFormControl =
        Preferences::GetBool("accessibility.mouse_focuses_formcontrol",
                             false);
    }
    else if (data.EqualsLiteral("focusmanager.testmode")) {
      sTestMode = Preferences::GetBool("focusmanager.testmode", false);
    }
  } else if (!nsCRT::strcmp(aTopic, "xpcom-shutdown")) {
    mActiveWindow = nullptr;
    mFocusedWindow = nullptr;
    mFocusedContent = nullptr;
    mFirstBlurEvent = nullptr;
    mFirstFocusEvent = nullptr;
    mWindowBeingLowered = nullptr;
    mDelayedBlurFocusEvents.Clear();
    mMouseButtonEventHandlingDocument = nullptr;
  }

  return NS_OK;
}

// given a frame content node, retrieve the nsIDOMWindow displayed in it 
static nsPIDOMWindowOuter*
GetContentWindow(nsIContent* aContent)
{
  nsIDocument* doc = aContent->GetComposedDoc();
  if (doc) {
    nsIDocument* subdoc = doc->GetSubDocumentFor(aContent);
    if (subdoc)
      return subdoc->GetWindow();
  }

  return nullptr;
}

// get the current window for the given content node 
static nsPIDOMWindowOuter*
GetCurrentWindow(nsIContent* aContent)
{
  nsIDocument* doc = aContent->GetComposedDoc();
  return doc ? doc->GetWindow() : nullptr;
}

// static
nsIContent*
nsFocusManager::GetFocusedDescendant(nsPIDOMWindowOuter* aWindow, bool aDeep,
                                     nsPIDOMWindowOuter** aFocusedWindow)
{
  NS_ENSURE_TRUE(aWindow, nullptr);

  *aFocusedWindow = nullptr;

  nsIContent* currentContent = nullptr;
  nsPIDOMWindowOuter* window = aWindow;
  while (window) {
    *aFocusedWindow = window;
    currentContent = window->GetFocusedNode();
    if (!currentContent || !aDeep)
      break;

    window = GetContentWindow(currentContent);
  }

  NS_IF_ADDREF(*aFocusedWindow);

  return currentContent;
}

// static
nsIContent*
nsFocusManager::GetRedirectedFocus(nsIContent* aContent)
{
  // For input number, redirect focus to our anonymous text control.
  if (aContent->IsHTMLElement(nsGkAtoms::input)) {
    bool typeIsNumber =
      static_cast<dom::HTMLInputElement*>(aContent)->GetType() ==
        NS_FORM_INPUT_NUMBER;

    if (typeIsNumber) {
      nsNumberControlFrame* numberControlFrame =
        do_QueryFrame(aContent->GetPrimaryFrame());

      if (numberControlFrame) {
        HTMLInputElement* textControl =
          numberControlFrame->GetAnonTextControl();
        return static_cast<nsIContent*>(textControl);
      }
    }
  }

#ifdef MOZ_XUL
  if (aContent->IsXULElement()) {
    nsCOMPtr<nsIDOMNode> inputField;

    nsCOMPtr<nsIDOMXULTextBoxElement> textbox = do_QueryInterface(aContent);
    if (textbox) {
      textbox->GetInputField(getter_AddRefs(inputField));
    }
    else {
      nsCOMPtr<nsIDOMXULMenuListElement> menulist = do_QueryInterface(aContent);
      if (menulist) {
        menulist->GetInputField(getter_AddRefs(inputField));
      }
      else if (aContent->IsXULElement(nsGkAtoms::scale)) {
        nsCOMPtr<nsIDocument> doc = aContent->GetComposedDoc();
        if (!doc)
          return nullptr;

        nsINodeList* children = doc->BindingManager()->GetAnonymousNodesFor(aContent);
        if (children) {
          nsIContent* child = children->Item(0);
          if (child && child->IsXULElement(nsGkAtoms::slider))
            return child;
        }
      }
    }

    if (inputField) {
      nsCOMPtr<nsIContent> retval = do_QueryInterface(inputField);
      return retval;
    }
  }
#endif

  return nullptr;
}

// static
InputContextAction::Cause
nsFocusManager::GetFocusMoveActionCause(uint32_t aFlags)
{
  if (aFlags & nsIFocusManager::FLAG_BYTOUCH) {
    return InputContextAction::CAUSE_TOUCH;
  } else if (aFlags & nsIFocusManager::FLAG_BYMOUSE) {
    return InputContextAction::CAUSE_MOUSE;
  } else if (aFlags & nsIFocusManager::FLAG_BYKEY) {
    return InputContextAction::CAUSE_KEY;
  }
  return InputContextAction::CAUSE_UNKNOWN;
}

NS_IMETHODIMP
nsFocusManager::GetActiveWindow(mozIDOMWindowProxy** aWindow)
{
  NS_IF_ADDREF(*aWindow = mActiveWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::SetActiveWindow(mozIDOMWindowProxy* aWindow)
{
  NS_ENSURE_STATE(aWindow);
  
  // only top-level windows can be made active
  nsCOMPtr<nsPIDOMWindowOuter> piWindow = nsPIDOMWindowOuter::From(aWindow);
  NS_ENSURE_TRUE(piWindow == piWindow->GetPrivateRoot(), NS_ERROR_INVALID_ARG);

  RaiseWindow(piWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::GetFocusedWindow(mozIDOMWindowProxy** aFocusedWindow)
{
  NS_IF_ADDREF(*aFocusedWindow = mFocusedWindow);
  return NS_OK;
}

NS_IMETHODIMP nsFocusManager::SetFocusedWindow(mozIDOMWindowProxy* aWindowToFocus)
{
  LOGFOCUS(("<<SetFocusedWindow begin>>"));

  nsCOMPtr<nsPIDOMWindowOuter> windowToFocus = nsPIDOMWindowOuter::From(aWindowToFocus);
  NS_ENSURE_TRUE(windowToFocus, NS_ERROR_FAILURE);

  windowToFocus = windowToFocus->GetOuterWindow();

  nsCOMPtr<Element> frameElement = windowToFocus->GetFrameElementInternal();
  if (frameElement) {
    // pass false for aFocusChanged so that the caret does not get updated
    // and scrolling does not occur.
    SetFocusInner(frameElement, 0, false, true);
  }
  else {
    // this is a top-level window. If the window has a child frame focused,
    // clear the focus. Otherwise, focus should already be in this frame, or
    // already cleared. This ensures that focus will be in this frame and not
    // in a child.
    nsIContent* content = windowToFocus->GetFocusedNode();
    if (content) {
      if (nsCOMPtr<nsPIDOMWindowOuter> childWindow = GetContentWindow(content))
        ClearFocus(windowToFocus);
    }
  }

  nsCOMPtr<nsPIDOMWindowOuter> rootWindow = windowToFocus->GetPrivateRoot();
  if (rootWindow)
    RaiseWindow(rootWindow);

  LOGFOCUS(("<<SetFocusedWindow end>>"));

  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::GetFocusedElement(nsIDOMElement** aFocusedElement)
{
  if (mFocusedContent)
    CallQueryInterface(mFocusedContent, aFocusedElement);
  else
    *aFocusedElement = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::GetLastFocusMethod(mozIDOMWindowProxy* aWindow, uint32_t* aLastFocusMethod)
{
  // the focus method is stored on the inner window
  nsCOMPtr<nsPIDOMWindowOuter> window;
  if (aWindow) {
    window = nsPIDOMWindowOuter::From(aWindow);
  }
  if (!window)
    window = mFocusedWindow;

  *aLastFocusMethod = window ? window->GetFocusMethod() : 0;

  NS_ASSERTION((*aLastFocusMethod & FOCUSMETHOD_MASK) == *aLastFocusMethod,
               "invalid focus method");
  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::SetFocus(nsIDOMElement* aElement, uint32_t aFlags)
{
  LOGFOCUS(("<<SetFocus begin>>"));

  nsCOMPtr<nsIContent> newFocus = do_QueryInterface(aElement);
  NS_ENSURE_ARG(newFocus);

  SetFocusInner(newFocus, aFlags, true, true);

  LOGFOCUS(("<<SetFocus end>>"));

  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::ElementIsFocusable(nsIDOMElement* aElement, uint32_t aFlags,
                                   bool* aIsFocusable)
{
  NS_ENSURE_TRUE(aElement, NS_ERROR_INVALID_ARG);

  nsCOMPtr<nsIContent> aContent = do_QueryInterface(aElement);

  *aIsFocusable = CheckIfFocusable(aContent, aFlags) != nullptr;

  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::MoveFocus(mozIDOMWindowProxy* aWindow, nsIDOMElement* aStartElement,
                          uint32_t aType, uint32_t aFlags, nsIDOMElement** aElement)
{
  *aElement = nullptr;

  LOGFOCUS(("<<MoveFocus begin Type: %d Flags: %x>>", aType, aFlags));

  if (MOZ_LOG_TEST(gFocusLog, LogLevel::Debug) && mFocusedWindow) {
    nsIDocument* doc = mFocusedWindow->GetExtantDoc();
    if (doc && doc->GetDocumentURI()) {
      nsAutoCString spec;
      doc->GetDocumentURI()->GetSpec(spec);
      LOGFOCUS((" Focused Window: %p %s", mFocusedWindow.get(), spec.get()));
    }
  }

  LOGCONTENT("  Current Focus: %s", mFocusedContent.get());

  // use FLAG_BYMOVEFOCUS when switching focus with MoveFocus unless one of
  // the other focus methods is already set, or we're just moving to the root
  // or caret position.
  if (aType != MOVEFOCUS_ROOT && aType != MOVEFOCUS_CARET &&
      (aFlags & FOCUSMETHOD_MASK) == 0) {
    aFlags |= FLAG_BYMOVEFOCUS;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window;
  nsCOMPtr<nsIContent> startContent;
  if (aStartElement) {
    startContent = do_QueryInterface(aStartElement);
    NS_ENSURE_TRUE(startContent, NS_ERROR_INVALID_ARG);

    window = GetCurrentWindow(startContent);
  }
  else {
    window = aWindow ? nsPIDOMWindowOuter::From(aWindow) : mFocusedWindow.get();
    NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);
  }

  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  bool noParentTraversal = aFlags & FLAG_NOPARENTFRAME;
  nsCOMPtr<nsIContent> newFocus;
  nsresult rv = DetermineElementToMoveFocus(window, startContent, aType, noParentTraversal,
                                            getter_AddRefs(newFocus));
  if (rv == NS_SUCCESS_DOM_NO_OPERATION) {
    return NS_OK;
  }

  NS_ENSURE_SUCCESS(rv, rv);

  LOGCONTENTNAVIGATION("Element to be focused: %s", newFocus.get());

  if (newFocus) {
    // for caret movement, pass false for the aFocusChanged argument,
    // otherwise the caret will end up moving to the focus position. This
    // would be a problem because the caret would move to the beginning of the
    // focused link making it impossible to navigate the caret over a link.
    SetFocusInner(newFocus, aFlags, aType != MOVEFOCUS_CARET, true);
    CallQueryInterface(newFocus, aElement);
  }
  else if (aType == MOVEFOCUS_ROOT || aType == MOVEFOCUS_CARET) {
    // no content was found, so clear the focus for these two types.
    ClearFocus(window);
  }

  LOGFOCUS(("<<MoveFocus end>>"));

  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::ClearFocus(mozIDOMWindowProxy* aWindow)
{
  LOGFOCUS(("<<ClearFocus begin>>"));

  // if the window to clear is the focused window or an ancestor of the
  // focused window, then blur the existing focused content. Otherwise, the
  // focus is somewhere else so just update the current node.
  NS_ENSURE_TRUE(aWindow, NS_ERROR_INVALID_ARG);
  nsCOMPtr<nsPIDOMWindowOuter> window = nsPIDOMWindowOuter::From(aWindow);

  if (IsSameOrAncestor(window, mFocusedWindow)) {
    bool isAncestor = (window != mFocusedWindow);
    if (Blur(window, nullptr, isAncestor, true)) {
      // if we are clearing the focus on an ancestor of the focused window,
      // the ancestor will become the new focused window, so focus it
      if (isAncestor)
        Focus(window, nullptr, 0, true, false, false, true);
    }
  }
  else {
    window->SetFocusedNode(nullptr);
  }

  LOGFOCUS(("<<ClearFocus end>>"));

  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::GetFocusedElementForWindow(mozIDOMWindowProxy* aWindow,
                                           bool aDeep,
                                           mozIDOMWindowProxy** aFocusedWindow,
                                           nsIDOMElement** aElement)
{
  *aElement = nullptr;
  if (aFocusedWindow)
    *aFocusedWindow = nullptr;

  NS_ENSURE_TRUE(aWindow, NS_ERROR_INVALID_ARG);
  nsCOMPtr<nsPIDOMWindowOuter> window = nsPIDOMWindowOuter::From(aWindow);

  nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
  nsCOMPtr<nsIContent> focusedContent =
    GetFocusedDescendant(window, aDeep, getter_AddRefs(focusedWindow));
  if (focusedContent)
    CallQueryInterface(focusedContent, aElement);

  if (aFocusedWindow)
    NS_IF_ADDREF(*aFocusedWindow = focusedWindow);

  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::MoveCaretToFocus(mozIDOMWindowProxy* aWindow)
{
  nsCOMPtr<nsIWebNavigation> webnav = do_GetInterface(aWindow);
  nsCOMPtr<nsIDocShellTreeItem> dsti = do_QueryInterface(webnav);
  if (dsti) {
    if (dsti->ItemType() != nsIDocShellTreeItem::typeChrome) {
      nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(dsti);
      NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

      // don't move the caret for editable documents
      bool isEditable;
      docShell->GetEditable(&isEditable);
      if (isEditable)
        return NS_OK;

      nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell();
      NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

      nsCOMPtr<nsPIDOMWindowOuter> window = nsPIDOMWindowOuter::From(aWindow);
      nsCOMPtr<nsIContent> content = window->GetFocusedNode();
      if (content)
        MoveCaretToFocus(presShell, content);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::WindowRaised(mozIDOMWindowProxy* aWindow)
{
  NS_ENSURE_TRUE(aWindow, NS_ERROR_INVALID_ARG);
  nsCOMPtr<nsPIDOMWindowOuter> window = nsPIDOMWindowOuter::From(aWindow);

  if (MOZ_LOG_TEST(gFocusLog, LogLevel::Debug)) {
    LOGFOCUS(("Window %p Raised [Currently: %p %p]", aWindow, mActiveWindow.get(), mFocusedWindow.get()));
    nsAutoCString spec;
    nsIDocument* doc = window->GetExtantDoc();
    if (doc && doc->GetDocumentURI()) {
      doc->GetDocumentURI()->GetSpec(spec);
      LOGFOCUS(("  Raised Window: %p %s", aWindow, spec.get()));
    }
    if (mActiveWindow) {
      doc = mActiveWindow->GetExtantDoc();
      if (doc && doc->GetDocumentURI()) {
        doc->GetDocumentURI()->GetSpec(spec);
        LOGFOCUS(("  Active Window: %p %s", mActiveWindow.get(), spec.get()));
      }
    }
  }

  if (mActiveWindow == window) {
    // The window is already active, so there is no need to focus anything,
    // but make sure that the right widget is focused. This is a special case
    // for Windows because when restoring a minimized window, a second
    // activation will occur and the top-level widget could be focused instead
    // of the child we want. We solve this by calling SetFocus to ensure that
    // what the focus manager thinks should be the current widget is actually
    // focused.
    EnsureCurrentWidgetFocused();
    return NS_OK;
  }

  // lower the existing window, if any. This shouldn't happen usually.
  if (mActiveWindow)
    WindowLowered(mActiveWindow);

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem = window->GetDocShell();
  // If there's no docShellAsItem, this window must have been closed,
  // in that case there is no tree owner.
  NS_ENSURE_TRUE(docShellAsItem, NS_OK);

  // set this as the active window
  mActiveWindow = window;

  // ensure that the window is enabled and visible
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  docShellAsItem->GetTreeOwner(getter_AddRefs(treeOwner));
  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(treeOwner);
  if (baseWindow) {
    bool isEnabled = true;
    if (NS_SUCCEEDED(baseWindow->GetEnabled(&isEnabled)) && !isEnabled) {
      return NS_ERROR_FAILURE;
    }

    if (!sTestMode) {
      baseWindow->SetVisibility(true);
    }
  }

  // If this is a parent or single process window, send the activate event.
  // Events for child process windows will be sent when ParentActivated
  // is called.
  if (XRE_IsParentProcess()) {
    ActivateOrDeactivate(window, true);
  }

  // retrieve the last focused element within the window that was raised
  nsCOMPtr<nsPIDOMWindowOuter> currentWindow;
  nsCOMPtr<nsIContent> currentFocus =
    GetFocusedDescendant(window, true, getter_AddRefs(currentWindow));

  NS_ASSERTION(currentWindow, "window raised with no window current");
  if (!currentWindow)
    return NS_OK;

  // If there is no nsIXULWindow, then this is an embedded or child process window.
  // Pass false for aWindowRaised so that commands get updated.
  nsCOMPtr<nsIXULWindow> xulWin(do_GetInterface(baseWindow));
  Focus(currentWindow, currentFocus, 0, true, false, xulWin != nullptr, true);

  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::WindowLowered(mozIDOMWindowProxy* aWindow)
{
  NS_ENSURE_TRUE(aWindow, NS_ERROR_INVALID_ARG);
  nsCOMPtr<nsPIDOMWindowOuter> window = nsPIDOMWindowOuter::From(aWindow);

  if (MOZ_LOG_TEST(gFocusLog, LogLevel::Debug)) {
    LOGFOCUS(("Window %p Lowered [Currently: %p %p]", aWindow, mActiveWindow.get(), mFocusedWindow.get()));
    nsAutoCString spec;
    nsIDocument* doc = window->GetExtantDoc();
    if (doc && doc->GetDocumentURI()) {
      doc->GetDocumentURI()->GetSpec(spec);
      LOGFOCUS(("  Lowered Window: %s", spec.get()));
    }
    if (mActiveWindow) {
      doc = mActiveWindow->GetExtantDoc();
      if (doc && doc->GetDocumentURI()) {
        doc->GetDocumentURI()->GetSpec(spec);
        LOGFOCUS(("  Active Window: %s", spec.get()));
      }
    }
  }

  if (mActiveWindow != window)
    return NS_OK;

  // clear the mouse capture as the active window has changed
  nsIPresShell::SetCapturingContent(nullptr, 0);

  // In addition, reset the drag state to ensure that we are no longer in
  // drag-select mode.
  if (mFocusedWindow) {
    nsCOMPtr<nsIDocShell> docShell = mFocusedWindow->GetDocShell();
    if (docShell) {
      nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell();
      if (presShell) {
        RefPtr<nsFrameSelection> frameSelection = presShell->FrameSelection();
        frameSelection->SetDragState(false);
      }
    }
  }

  // If this is a parent or single process window, send the deactivate event.
  // Events for child process windows will be sent when ParentActivated
  // is called.
  if (XRE_IsParentProcess()) {
    ActivateOrDeactivate(window, false);
  }

  // keep track of the window being lowered, so that attempts to raise the
  // window can be prevented until we return. Otherwise, focus can get into
  // an unusual state.
  mWindowBeingLowered = mActiveWindow;
  mActiveWindow = nullptr;

  if (mFocusedWindow)
    Blur(nullptr, nullptr, true, true);

  mWindowBeingLowered = nullptr;

  return NS_OK;
}

nsresult
nsFocusManager::ContentRemoved(nsIDocument* aDocument, nsIContent* aContent)
{
  NS_ENSURE_ARG(aDocument);
  NS_ENSURE_ARG(aContent);

  nsPIDOMWindowOuter *window = aDocument->GetWindow();
  if (!window)
    return NS_OK;

  // if the content is currently focused in the window, or is an ancestor
  // of the currently focused element, reset the focus within that window.
  nsIContent* content = window->GetFocusedNode();
  if (content && nsContentUtils::ContentIsDescendantOf(content, aContent)) {
    bool shouldShowFocusRing = window->ShouldShowFocusRing();
    window->SetFocusedNode(nullptr);

    // if this window is currently focused, clear the global focused
    // element as well, but don't fire any events.
    if (window == mFocusedWindow) {
      mFocusedContent = nullptr;
    } else {
      // Check if the node that was focused is an iframe or similar by looking
      // if it has a subdocument. This would indicate that this focused iframe
      // and its descendants will be going away. We will need to move the
      // focus somewhere else, so just clear the focus in the toplevel window
      // so that no element is focused.
      nsIDocument* subdoc = aDocument->GetSubDocumentFor(content);
      if (subdoc) {
        nsCOMPtr<nsIDocShell> docShell = subdoc->GetDocShell();
        if (docShell) {
          nsCOMPtr<nsPIDOMWindowOuter> childWindow = docShell->GetWindow();
          if (childWindow && IsSameOrAncestor(childWindow, mFocusedWindow)) {
            ClearFocus(mActiveWindow);
          }
        }
      }
    }

    // Notify the editor in case we removed its ancestor limiter.
    if (content->IsEditable()) {
      nsCOMPtr<nsIDocShell> docShell = aDocument->GetDocShell();
      if (docShell) {
        nsCOMPtr<nsIEditor> editor;
        docShell->GetEditor(getter_AddRefs(editor));
        if (editor) {
          nsCOMPtr<nsISelection> s;
          editor->GetSelection(getter_AddRefs(s));
          nsCOMPtr<nsISelectionPrivate> selection = do_QueryInterface(s);
          if (selection) {
            nsCOMPtr<nsIContent> limiter;
            selection->GetAncestorLimiter(getter_AddRefs(limiter));
            if (limiter == content) {
              editor->FinalizeSelection();
            }
          }
        }
      }
    }

    NotifyFocusStateChange(content, shouldShowFocusRing, false);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::WindowShown(mozIDOMWindowProxy* aWindow, bool aNeedsFocus)
{
  NS_ENSURE_TRUE(aWindow, NS_ERROR_INVALID_ARG);
  nsCOMPtr<nsPIDOMWindowOuter> window = nsPIDOMWindowOuter::From(aWindow);

  if (MOZ_LOG_TEST(gFocusLog, LogLevel::Debug)) {
    LOGFOCUS(("Window %p Shown [Currently: %p %p]", window.get(), mActiveWindow.get(), mFocusedWindow.get()));
    nsAutoCString spec;
    nsIDocument* doc = window->GetExtantDoc();
    if (doc && doc->GetDocumentURI()) {
      doc->GetDocumentURI()->GetSpec(spec);
      LOGFOCUS(("Shown Window: %s", spec.get()));
    }

    if (mFocusedWindow) {
      doc = mFocusedWindow->GetExtantDoc();
      if (doc && doc->GetDocumentURI()) {
        doc->GetDocumentURI()->GetSpec(spec);
        LOGFOCUS((" Focused Window: %s", spec.get()));
      }
    }
  }

  if (nsIDocShell* docShell = window->GetDocShell()) {
    if (nsCOMPtr<nsITabChild> child = docShell->GetTabChild()) {
      bool active = static_cast<TabChild*>(child.get())->ParentIsActive();
      ActivateOrDeactivate(window, active);
    }
  }

  if (mFocusedWindow != window)
    return NS_OK;

  if (aNeedsFocus) {
    nsCOMPtr<nsPIDOMWindowOuter> currentWindow;
    nsCOMPtr<nsIContent> currentFocus =
      GetFocusedDescendant(window, true, getter_AddRefs(currentWindow));
    if (currentWindow)
      Focus(currentWindow, currentFocus, 0, true, false, false, true);
  }
  else {
    // Sometimes, an element in a window can be focused before the window is
    // visible, which would mean that the widget may not be properly focused.
    // When the window becomes visible, make sure the right widget is focused.
    EnsureCurrentWidgetFocused();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::WindowHidden(mozIDOMWindowProxy* aWindow)
{
  // if there is no window or it is not the same or an ancestor of the
  // currently focused window, just return, as the current focus will not
  // be affected.

  NS_ENSURE_TRUE(aWindow, NS_ERROR_INVALID_ARG);
  nsCOMPtr<nsPIDOMWindowOuter> window = nsPIDOMWindowOuter::From(aWindow);

  if (MOZ_LOG_TEST(gFocusLog, LogLevel::Debug)) {
    LOGFOCUS(("Window %p Hidden [Currently: %p %p]", window.get(), mActiveWindow.get(), mFocusedWindow.get()));
    nsAutoCString spec;
    nsIDocument* doc = window->GetExtantDoc();
    if (doc && doc->GetDocumentURI()) {
      doc->GetDocumentURI()->GetSpec(spec);
      LOGFOCUS(("  Hide Window: %s", spec.get()));
    }

    if (mFocusedWindow) {
      doc = mFocusedWindow->GetExtantDoc();
      if (doc && doc->GetDocumentURI()) {
        doc->GetDocumentURI()->GetSpec(spec);
        LOGFOCUS(("  Focused Window: %s", spec.get()));
      }
    }

    if (mActiveWindow) {
      doc = mActiveWindow->GetExtantDoc();
      if (doc && doc->GetDocumentURI()) {
        doc->GetDocumentURI()->GetSpec(spec);
        LOGFOCUS(("  Active Window: %s", spec.get()));
      }
    }
  }

  if (!IsSameOrAncestor(window, mFocusedWindow))
    return NS_OK;

  // at this point, we know that the window being hidden is either the focused
  // window, or an ancestor of the focused window. Either way, the focus is no
  // longer valid, so it needs to be updated.

  nsCOMPtr<nsIContent> oldFocusedContent = mFocusedContent.forget();

  nsCOMPtr<nsIDocShell> focusedDocShell = mFocusedWindow->GetDocShell();
  nsCOMPtr<nsIPresShell> presShell = focusedDocShell->GetPresShell();

  if (oldFocusedContent && oldFocusedContent->IsInComposedDoc()) {
    NotifyFocusStateChange(oldFocusedContent,
                           mFocusedWindow->ShouldShowFocusRing(),
                           false);
    window->UpdateCommands(NS_LITERAL_STRING("focus"), nullptr, 0);

    if (presShell) {
      SendFocusOrBlurEvent(eBlur, presShell,
                           oldFocusedContent->GetComposedDoc(),
                           oldFocusedContent, 1, false);
    }
  }

  nsPresContext* focusedPresContext =
    presShell ? presShell->GetPresContext() : nullptr;
  IMEStateManager::OnChangeFocus(focusedPresContext, nullptr,
                                 GetFocusMoveActionCause(0));
  if (presShell) {
    SetCaretVisible(presShell, false, nullptr);
  }

  // if the docshell being hidden is being destroyed, then we want to move
  // focus somewhere else. Call ClearFocus on the toplevel window, which
  // will have the effect of clearing the focus and moving the focused window
  // to the toplevel window. But if the window isn't being destroyed, we are
  // likely just loading a new document in it, so we want to maintain the
  // focused window so that the new document gets properly focused.
  nsCOMPtr<nsIDocShell> docShellBeingHidden = window->GetDocShell();
  bool beingDestroyed = !docShellBeingHidden;
  if (docShellBeingHidden) {
    docShellBeingHidden->IsBeingDestroyed(&beingDestroyed);
  }
  if (beingDestroyed) {
    // There is usually no need to do anything if a toplevel window is going
    // away, as we assume that WindowLowered will be called. However, this may
    // not happen if nsIAppStartup::eForceQuit is used to quit, and can cause
    // a leak. So if the active window is being destroyed, call WindowLowered
    // directly.
    NS_ASSERTION(mFocusedWindow->IsOuterWindow(), "outer window expected");
    if (mActiveWindow == mFocusedWindow || mActiveWindow == window)
      WindowLowered(mActiveWindow);
    else
      ClearFocus(mActiveWindow);
    return NS_OK;
  }

  // if the window being hidden is an ancestor of the focused window, adjust
  // the focused window so that it points to the one being hidden. This
  // ensures that the focused window isn't in a chain of frames that doesn't
  // exist any more.
  if (window != mFocusedWindow) {
    nsCOMPtr<nsIDocShellTreeItem> dsti =
      mFocusedWindow ? mFocusedWindow->GetDocShell() : nullptr;
    if (dsti) {
      nsCOMPtr<nsIDocShellTreeItem> parentDsti;
      dsti->GetParent(getter_AddRefs(parentDsti));
      if (parentDsti) {
        if (nsCOMPtr<nsPIDOMWindowOuter> parentWindow = parentDsti->GetWindow())
          parentWindow->SetFocusedNode(nullptr);
      }
    }

    SetFocusedWindowInternal(window);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::FireDelayedEvents(nsIDocument* aDocument)
{
  NS_ENSURE_ARG(aDocument);

  // fire any delayed focus and blur events in the same order that they were added
  for (uint32_t i = 0; i < mDelayedBlurFocusEvents.Length(); i++) {
    if (mDelayedBlurFocusEvents[i].mDocument == aDocument) {
      if (!aDocument->GetInnerWindow() ||
          !aDocument->GetInnerWindow()->IsCurrentInnerWindow()) {
        // If the document was navigated away from or is defunct, don't bother
        // firing events on it. Note the symmetry between this condition and
        // the similar one in nsDocument.cpp:FireOrClearDelayedEvents.
        mDelayedBlurFocusEvents.RemoveElementAt(i);
        --i;
      } else if (!aDocument->EventHandlingSuppressed()) {
        EventMessage message = mDelayedBlurFocusEvents[i].mEventMessage;
        nsCOMPtr<EventTarget> target = mDelayedBlurFocusEvents[i].mTarget;
        nsCOMPtr<nsIPresShell> presShell = mDelayedBlurFocusEvents[i].mPresShell;
        nsCOMPtr<EventTarget> relatedTarget = mDelayedBlurFocusEvents[i].mRelatedTarget;
        mDelayedBlurFocusEvents.RemoveElementAt(i);
        SendFocusOrBlurEvent(message, presShell, aDocument, target, 0,
                             false, false, relatedTarget);
        --i;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::FocusPlugin(nsIContent* aContent)
{
  NS_ENSURE_ARG(aContent);
  SetFocusInner(aContent, 0, true, false);
  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::ParentActivated(mozIDOMWindowProxy* aWindow, bool aActive)
{
  nsCOMPtr<nsPIDOMWindowOuter> window = nsPIDOMWindowOuter::From(aWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_INVALID_ARG);

  ActivateOrDeactivate(window, aActive);
  return NS_OK;
}

/* static */
void
nsFocusManager::NotifyFocusStateChange(nsIContent* aContent,
                                       bool aWindowShouldShowFocusRing,
                                       bool aGettingFocus)
{
  if (!aContent->IsElement()) {
    return;
  }
  EventStates eventState = NS_EVENT_STATE_FOCUS;
  if (aWindowShouldShowFocusRing) {
    eventState |= NS_EVENT_STATE_FOCUSRING;
  }
  if (aGettingFocus) {
    aContent->AsElement()->AddStates(eventState);
  } else {
    aContent->AsElement()->RemoveStates(eventState);
  }
}

// static
void
nsFocusManager::EnsureCurrentWidgetFocused()
{
  if (!mFocusedWindow || sTestMode)
    return;

  // get the main child widget for the focused window and ensure that the
  // platform knows that this widget is focused.
  nsCOMPtr<nsIDocShell> docShell = mFocusedWindow->GetDocShell();
  if (docShell) {
    nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell();
    if (presShell) {
      nsViewManager* vm = presShell->GetViewManager();
      if (vm) {
        nsCOMPtr<nsIWidget> widget;
        vm->GetRootWidget(getter_AddRefs(widget));
        if (widget)
          widget->SetFocus(false);
      }
    }
  }
}

bool
ActivateOrDeactivateChild(TabParent* aParent, void* aArg)
{
  bool active = static_cast<bool>(aArg);
  Unused << aParent->SendParentActivated(active);
  return false;
}

void
nsFocusManager::ActivateOrDeactivate(nsPIDOMWindowOuter* aWindow, bool aActive)
{
  if (!aWindow) {
    return;
  }

  // Inform the DOM window that it has activated or deactivated, so that
  // the active attribute is updated on the window.
  aWindow->ActivateOrDeactivate(aActive);

  // Send the activate event.
  if (aWindow->GetExtantDoc()) {
    nsContentUtils::DispatchEventOnlyToChrome(aWindow->GetExtantDoc(),
                                              aWindow->GetCurrentInnerWindow(),
                                              aActive ?
                                                NS_LITERAL_STRING("activate") :
                                                NS_LITERAL_STRING("deactivate"),
                                              true, true, nullptr);
  }

  // Look for any remote child frames, iterate over them and send the activation notification.
  nsContentUtils::CallOnAllRemoteChildren(aWindow, ActivateOrDeactivateChild,
                                          (void *)aActive);
}

void
nsFocusManager::SetFocusInner(nsIContent* aNewContent, int32_t aFlags,
                              bool aFocusChanged, bool aAdjustWidget)
{
  // if the element is not focusable, just return and leave the focus as is
  nsCOMPtr<nsIContent> contentToFocus = CheckIfFocusable(aNewContent, aFlags);
  if (!contentToFocus)
    return;

  // check if the element to focus is a frame (iframe) containing a child
  // document. Frames are never directly focused; instead focusing a frame
  // means focus what is inside the frame. To do this, the descendant content
  // within the frame is retrieved and that will be focused instead.
  nsCOMPtr<nsPIDOMWindowOuter> newWindow;
  nsCOMPtr<nsPIDOMWindowOuter> subWindow = GetContentWindow(contentToFocus);
  if (subWindow) {
    contentToFocus = GetFocusedDescendant(subWindow, true, getter_AddRefs(newWindow));
    // since a window is being refocused, clear aFocusChanged so that the
    // caret position isn't updated.
    aFocusChanged = false;
  }

  // unless it was set above, retrieve the window for the element to focus
  if (!newWindow)
    newWindow = GetCurrentWindow(contentToFocus);

  // if the element is already focused, just return. Note that this happens
  // after the frame check above so that we compare the element that will be
  // focused rather than the frame it is in.
  if (!newWindow || (newWindow == mFocusedWindow && contentToFocus == mFocusedContent))
    return;

  // don't allow focus to be placed in docshells or descendants of docshells
  // that are being destroyed. Also, ensure that the page hasn't been
  // unloaded. The prevents content from being refocused during an unload event.
  nsCOMPtr<nsIDocShell> newDocShell = newWindow->GetDocShell();
  nsCOMPtr<nsIDocShell> docShell = newDocShell;
  while (docShell) {
    bool inUnload;
    docShell->GetIsInUnload(&inUnload);
    if (inUnload)
      return;

    bool beingDestroyed;
    docShell->IsBeingDestroyed(&beingDestroyed);
    if (beingDestroyed)
      return;

    nsCOMPtr<nsIDocShellTreeItem> parentDsti;
    docShell->GetParent(getter_AddRefs(parentDsti));
    docShell = do_QueryInterface(parentDsti);
  }

  // if the new element is in the same window as the currently focused element 
  bool isElementInFocusedWindow = (mFocusedWindow == newWindow);

  if (!isElementInFocusedWindow && mFocusedWindow && newWindow &&
      nsContentUtils::IsHandlingKeyBoardEvent()) {
    nsCOMPtr<nsIScriptObjectPrincipal> focused =
      do_QueryInterface(mFocusedWindow);
    nsCOMPtr<nsIScriptObjectPrincipal> newFocus =
      do_QueryInterface(newWindow);
    nsIPrincipal* focusedPrincipal = focused->GetPrincipal();
    nsIPrincipal* newPrincipal = newFocus->GetPrincipal();
    if (!focusedPrincipal || !newPrincipal) {
      return;
    }
    bool subsumes = false;
    focusedPrincipal->Subsumes(newPrincipal, &subsumes);
    if (!subsumes && !nsContentUtils::LegacyIsCallerChromeOrNativeCode()) {
      NS_WARNING("Not allowed to focus the new window!");
      return;
    }
  }

  // to check if the new element is in the active window, compare the
  // new root docshell for the new element with the active window's docshell.
  bool isElementInActiveWindow = false;

  nsCOMPtr<nsIDocShellTreeItem> dsti = newWindow->GetDocShell();
  nsCOMPtr<nsPIDOMWindowOuter> newRootWindow;
  if (dsti) {
    nsCOMPtr<nsIDocShellTreeItem> root;
    dsti->GetRootTreeItem(getter_AddRefs(root));
    newRootWindow = root ? root->GetWindow() : nullptr;

    isElementInActiveWindow = (mActiveWindow && newRootWindow == mActiveWindow);
  }

  // Exit fullscreen if we're focusing a windowed plugin on a non-MacOSX
  // system. We don't control event dispatch to windowed plugins on non-MacOSX,
  // so we can't display the "Press ESC to leave fullscreen mode" warning on
  // key input if a windowed plugin is focused, so just exit fullscreen
  // to guard against phishing.
#ifndef XP_MACOSX
  if (contentToFocus &&
      nsContentUtils::
        GetRootDocument(contentToFocus->OwnerDoc())->GetFullscreenElement() &&
      nsContentUtils::HasPluginWithUncontrolledEventDispatch(contentToFocus)) {
    nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                    NS_LITERAL_CSTRING("DOM"),
                                    contentToFocus->OwnerDoc(),
                                    nsContentUtils::eDOM_PROPERTIES,
                                    "FocusedWindowedPluginWhileFullscreen");
    nsIDocument::AsyncExitFullscreen(contentToFocus->OwnerDoc());
  }
#endif

  // if the FLAG_NOSWITCHFRAME flag is used, only allow the focus to be
  // shifted away from the current element if the new shell to focus is
  // the same or an ancestor shell of the currently focused shell.
  bool allowFrameSwitch = !(aFlags & FLAG_NOSWITCHFRAME) ||
                            IsSameOrAncestor(newWindow, mFocusedWindow);

  // if the element is in the active window, frame switching is allowed and
  // the content is in a visible window, fire blur and focus events.
  bool sendFocusEvent =
    isElementInActiveWindow && allowFrameSwitch && IsWindowVisible(newWindow);

  // When the following conditions are true:
  //  * an element has focus
  //  * isn't called by trusted event (i.e., called by untrusted event or by js)
  //  * the focus is moved to another document's element
  // we need to check the permission.
  if (sendFocusEvent && mFocusedContent && !nsContentUtils::LegacyIsCallerNativeCode() &&
      mFocusedContent->OwnerDoc() != aNewContent->OwnerDoc()) {
    // If the caller cannot access the current focused node, the caller should
    // not be able to steal focus from it. E.g., When the current focused node
    // is in chrome, any web contents should not be able to steal the focus.
    nsCOMPtr<nsIDOMNode> domNode(do_QueryInterface(mFocusedContent));
    sendFocusEvent = nsContentUtils::CanCallerAccess(domNode);
    if (!sendFocusEvent && mMouseButtonEventHandlingDocument) {
      // However, while mouse button event is handling, the handling document's
      // script should be able to steal focus.
      domNode = do_QueryInterface(mMouseButtonEventHandlingDocument);
      sendFocusEvent = nsContentUtils::CanCallerAccess(domNode);
    }
  }

  LOGCONTENT("Shift Focus: %s", contentToFocus.get());
  LOGFOCUS((" Flags: %x Current Window: %p New Window: %p Current Element: %p",
           aFlags, mFocusedWindow.get(), newWindow.get(), mFocusedContent.get()));
  LOGFOCUS((" In Active Window: %d In Focused Window: %d SendFocus: %d",
           isElementInActiveWindow, isElementInFocusedWindow, sendFocusEvent));

  if (sendFocusEvent) {
    nsCOMPtr<nsIContent> oldFocusedContent = mFocusedContent;
    // return if blurring fails or the focus changes during the blur
    if (mFocusedWindow) {
      // if the focus is being moved to another element in the same document,
      // or to a descendant, pass the existing window to Blur so that the
      // current node in the existing window is cleared. If moving to a
      // window elsewhere, we want to maintain the current node in the
      // window but still blur it.
      bool currentIsSameOrAncestor = IsSameOrAncestor(mFocusedWindow, newWindow);
      // find the common ancestor of the currently focused window and the new
      // window. The ancestor will need to have its currently focused node
      // cleared once the document has been blurred. Otherwise, we'll be in a
      // state where a document is blurred yet the chain of windows above it
      // still points to that document.
      // For instance, in the following frame tree:
      //   A
      //  B C
      //  D
      // D is focused and we want to focus C. Once D has been blurred, we need
      // to clear out the focus in A, otherwise A would still maintain that B
      // was focused, and B that D was focused.
      nsCOMPtr<nsPIDOMWindowOuter> commonAncestor;
      if (!isElementInFocusedWindow)
        commonAncestor = GetCommonAncestor(newWindow, mFocusedWindow);

      if (!Blur(currentIsSameOrAncestor ? mFocusedWindow.get() : nullptr,
                commonAncestor, !isElementInFocusedWindow, aAdjustWidget,
                contentToFocus))
        return;
    }

    Focus(newWindow, contentToFocus, aFlags, !isElementInFocusedWindow,
          aFocusChanged, false, aAdjustWidget, oldFocusedContent);
  }
  else {
    // otherwise, for inactive windows and when the caller cannot steal the
    // focus, update the node in the window, and  raise the window if desired.
    if (allowFrameSwitch)
      AdjustWindowFocus(newWindow, true);

    // set the focus node and method as needed
    uint32_t focusMethod = aFocusChanged ? aFlags & FOCUSMETHODANDRING_MASK :
                           newWindow->GetFocusMethod() | (aFlags & FLAG_SHOWRING);
    newWindow->SetFocusedNode(contentToFocus, focusMethod);
    if (aFocusChanged) {
      nsCOMPtr<nsIDocShell> docShell = newWindow->GetDocShell();

      nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell();
      if (presShell && presShell->DidInitialize())
        ScrollIntoView(presShell, contentToFocus, aFlags);
    }

    // update the commands even when inactive so that the attributes for that
    // window are up to date.
    if (allowFrameSwitch)
      newWindow->UpdateCommands(NS_LITERAL_STRING("focus"), nullptr, 0);

    if (aFlags & FLAG_RAISE)
      RaiseWindow(newRootWindow);
  }
}

bool
nsFocusManager::IsSameOrAncestor(nsPIDOMWindowOuter* aPossibleAncestor,
                                 nsPIDOMWindowOuter* aWindow)
{
  if (!aWindow || !aPossibleAncestor) {
    return false;
  }

  nsCOMPtr<nsIDocShellTreeItem> ancestordsti = aPossibleAncestor->GetDocShell();
  nsCOMPtr<nsIDocShellTreeItem> dsti = aWindow->GetDocShell();
  while (dsti) {
    if (dsti == ancestordsti)
      return true;
    nsCOMPtr<nsIDocShellTreeItem> parentDsti;
    dsti->GetParent(getter_AddRefs(parentDsti));
    dsti.swap(parentDsti);
  }

  return false;
}

already_AddRefed<nsPIDOMWindowOuter>
nsFocusManager::GetCommonAncestor(nsPIDOMWindowOuter* aWindow1,
                                  nsPIDOMWindowOuter* aWindow2)
{
  NS_ENSURE_TRUE(aWindow1 && aWindow2, nullptr);

  nsCOMPtr<nsIDocShellTreeItem> dsti1 = aWindow1->GetDocShell();
  NS_ENSURE_TRUE(dsti1, nullptr);

  nsCOMPtr<nsIDocShellTreeItem> dsti2 = aWindow2->GetDocShell();
  NS_ENSURE_TRUE(dsti2, nullptr);

  AutoTArray<nsIDocShellTreeItem*, 30> parents1, parents2;
  do {
    parents1.AppendElement(dsti1);
    nsCOMPtr<nsIDocShellTreeItem> parentDsti1;
    dsti1->GetParent(getter_AddRefs(parentDsti1));
    dsti1.swap(parentDsti1);
  } while (dsti1);
  do {
    parents2.AppendElement(dsti2);
    nsCOMPtr<nsIDocShellTreeItem> parentDsti2;
    dsti2->GetParent(getter_AddRefs(parentDsti2));
    dsti2.swap(parentDsti2);
  } while (dsti2);

  uint32_t pos1 = parents1.Length();
  uint32_t pos2 = parents2.Length();
  nsIDocShellTreeItem* parent = nullptr;
  uint32_t len;
  for (len = std::min(pos1, pos2); len > 0; --len) {
    nsIDocShellTreeItem* child1 = parents1.ElementAt(--pos1);
    nsIDocShellTreeItem* child2 = parents2.ElementAt(--pos2);
    if (child1 != child2) {
      break;
    }
    parent = child1;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = parent ? parent->GetWindow() : nullptr;
  return window.forget();
}

void
nsFocusManager::AdjustWindowFocus(nsPIDOMWindowOuter* aWindow,
                                  bool aCheckPermission)
{
  bool isVisible = IsWindowVisible(aWindow);

  nsCOMPtr<nsPIDOMWindowOuter> window(aWindow);
  while (window) {
    // get the containing <iframe> or equivalent element so that it can be
    // focused below.
    nsCOMPtr<Element> frameElement = window->GetFrameElementInternal();

    nsCOMPtr<nsIDocShellTreeItem> dsti = window->GetDocShell();
    if (!dsti) 
      return;
    nsCOMPtr<nsIDocShellTreeItem> parentDsti;
    dsti->GetParent(getter_AddRefs(parentDsti));
    if (!parentDsti) {
      return;
    }

    window = parentDsti->GetWindow();
    if (window) {
      // if the parent window is visible but aWindow was not, then we have
      // likely moved up and out from a hidden tab to the browser window, or a
      // similar such arrangement. Stop adjusting the current nodes.
      if (IsWindowVisible(window) != isVisible)
        break;

      // When aCheckPermission is true, we should check whether the caller can
      // access the window or not.  If it cannot access, we should stop the
      // adjusting.
      if (aCheckPermission && !nsContentUtils::LegacyIsCallerNativeCode() &&
          !nsContentUtils::CanCallerAccess(window->GetCurrentInnerWindow())) {
        break;
      }

      window->SetFocusedNode(frameElement);
    }
  }
}

bool
nsFocusManager::IsWindowVisible(nsPIDOMWindowOuter* aWindow)
{
  if (!aWindow || aWindow->IsFrozen())
    return false;

  // Check if the inner window is frozen as well. This can happen when a focus change
  // occurs while restoring a previous page.
  nsPIDOMWindowInner* innerWindow = aWindow->GetCurrentInnerWindow();
  if (!innerWindow || innerWindow->IsFrozen())
    return false;

  nsCOMPtr<nsIDocShell> docShell = aWindow->GetDocShell();
  nsCOMPtr<nsIBaseWindow> baseWin(do_QueryInterface(docShell));
  if (!baseWin)
    return false;

  bool visible = false;
  baseWin->GetVisibility(&visible);
  return visible;
}

bool
nsFocusManager::IsNonFocusableRoot(nsIContent* aContent)
{
  NS_PRECONDITION(aContent, "aContent must not be NULL");
  NS_PRECONDITION(aContent->IsInComposedDoc(), "aContent must be in a document");

  // If aContent is in designMode, the root element is not focusable.
  // NOTE: in designMode, most elements are not focusable, just the document is
  //       focusable.
  // Also, if aContent is not editable but it isn't in designMode, it's not
  // focusable.
  // And in userfocusignored context nothing is focusable.
  nsIDocument* doc = aContent->GetComposedDoc();
  NS_ASSERTION(doc, "aContent must have current document");
  return aContent == doc->GetRootElement() &&
           (doc->HasFlag(NODE_IS_EDITABLE) || !aContent->IsEditable() ||
            nsContentUtils::IsUserFocusIgnored(aContent));
}

nsIContent*
nsFocusManager::CheckIfFocusable(nsIContent* aContent, uint32_t aFlags)
{
  if (!aContent)
    return nullptr;

  // this is a special case for some XUL elements or input number, where an
  // anonymous child is actually focusable and not the element itself.
  nsIContent* redirectedFocus = GetRedirectedFocus(aContent);
  if (redirectedFocus)
    return CheckIfFocusable(redirectedFocus, aFlags);

  nsCOMPtr<nsIDocument> doc = aContent->GetComposedDoc();
  // can't focus elements that are not in documents
  if (!doc) {
    LOGCONTENT("Cannot focus %s because content not in document", aContent)
    return nullptr;
  }

  // Make sure that our frames are up to date
  doc->FlushPendingNotifications(Flush_Layout);

  nsIPresShell *shell = doc->GetShell();
  if (!shell)
    return nullptr;

  // the root content can always be focused,
  // except in userfocusignored context.
  if (aContent == doc->GetRootElement())
    return nsContentUtils::IsUserFocusIgnored(aContent) ? nullptr : aContent;

  // cannot focus content in print preview mode. Only the root can be focused.
  nsPresContext* presContext = shell->GetPresContext();
  if (presContext && presContext->Type() == nsPresContext::eContext_PrintPreview) {
    LOGCONTENT("Cannot focus %s while in print preview", aContent)
    return nullptr;
  }

  nsIFrame* frame = aContent->GetPrimaryFrame();
  if (!frame) {
    LOGCONTENT("Cannot focus %s as it has no frame", aContent)
    return nullptr;
  }

  if (aContent->IsHTMLElement(nsGkAtoms::area)) {
    // HTML areas do not have their own frame, and the img frame we get from
    // GetPrimaryFrame() is not relevant as to whether it is focusable or
    // not, so we have to do all the relevant checks manually for them.
    return frame->IsVisibleConsideringAncestors() &&
           aContent->IsFocusable() ? aContent : nullptr;
  }

  // if this is a child frame content node, check if it is visible and
  // call the content node's IsFocusable method instead of the frame's
  // IsFocusable method. This skips checking the style system and ensures that
  // offscreen browsers can still be focused.
  nsIDocument* subdoc = doc->GetSubDocumentFor(aContent);
  if (subdoc && IsWindowVisible(subdoc->GetWindow())) {
    const nsStyleUserInterface* ui = frame->StyleUserInterface();
    int32_t tabIndex = (ui->mUserFocus == NS_STYLE_USER_FOCUS_IGNORE ||
                        ui->mUserFocus == NS_STYLE_USER_FOCUS_NONE) ? -1 : 0;
    return aContent->IsFocusable(&tabIndex, aFlags & FLAG_BYMOUSE) ? aContent : nullptr;
  }
  
  return frame->IsFocusable(nullptr, aFlags & FLAG_BYMOUSE) ? aContent : nullptr;
}

bool
nsFocusManager::Blur(nsPIDOMWindowOuter* aWindowToClear,
                     nsPIDOMWindowOuter* aAncestorWindowToFocus,
                     bool aIsLeavingDocument,
                     bool aAdjustWidgets,
                     nsIContent* aContentToFocus)
{
  LOGFOCUS(("<<Blur begin>>"));

  // hold a reference to the focused content, which may be null
  nsCOMPtr<nsIContent> content = mFocusedContent;
  if (content) {
    if (!content->IsInComposedDoc()) {
      mFocusedContent = nullptr;
      return true;
    }
    if (content == mFirstBlurEvent)
      return true;
  }

  // hold a reference to the focused window
  nsCOMPtr<nsPIDOMWindowOuter> window = mFocusedWindow;
  if (!window) {
    mFocusedContent = nullptr;
    return true;
  }

  nsCOMPtr<nsIDocShell> docShell = window->GetDocShell();
  if (!docShell) {
    mFocusedContent = nullptr;
    return true;
  }

  // Keep a ref to presShell since dispatching the DOM event may cause
  // the document to be destroyed.
  nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell();
  if (!presShell) {
    mFocusedContent = nullptr;
    return true;
  }

  bool clearFirstBlurEvent = false;
  if (!mFirstBlurEvent) {
    mFirstBlurEvent = content;
    clearFirstBlurEvent = true;
  }

  nsPresContext* focusedPresContext =
    mActiveWindow ? presShell->GetPresContext() : nullptr;
  IMEStateManager::OnChangeFocus(focusedPresContext, nullptr,
                                 GetFocusMoveActionCause(0));

  // now adjust the actual focus, by clearing the fields in the focus manager
  // and in the window.
  mFocusedContent = nullptr;
  bool shouldShowFocusRing = window->ShouldShowFocusRing();
  if (aWindowToClear)
    aWindowToClear->SetFocusedNode(nullptr);

  LOGCONTENT("Element %s has been blurred", content.get());

  // Don't fire blur event on the root content which isn't editable.
  bool sendBlurEvent =
    content && content->IsInComposedDoc() && !IsNonFocusableRoot(content);
  if (content) {
    if (sendBlurEvent) {
      NotifyFocusStateChange(content, shouldShowFocusRing, false);
    }

    // if an object/plug-in/remote browser is being blurred, move the system focus
    // to the parent window, otherwise events will still get fired at the plugin.
    // But don't do this if we are blurring due to the window being lowered,
    // otherwise, the parent window can get raised again.
    if (mActiveWindow) {
      nsIFrame* contentFrame = content->GetPrimaryFrame();
      nsIObjectFrame* objectFrame = do_QueryFrame(contentFrame);
      if (aAdjustWidgets && objectFrame && !sTestMode) {
        if (XRE_IsContentProcess()) {
          // set focus to the top level window via the chrome process.
          nsCOMPtr<nsITabChild> tabChild = docShell->GetTabChild();
          if (tabChild) {
            static_cast<TabChild*>(tabChild.get())->SendDispatchFocusToTopLevelWindow();
          }
        } else {
          // note that the presshell's widget is being retrieved here, not the one
          // for the object frame.
          nsViewManager* vm = presShell->GetViewManager();
          if (vm) {
            nsCOMPtr<nsIWidget> widget;
            vm->GetRootWidget(getter_AddRefs(widget));
            if (widget) {
              // set focus to the top level window but don't raise it.
              widget->SetFocus(false);
            }
          }
        }
      }
    }

      // if the object being blurred is a remote browser, deactivate remote content
    if (TabParent* remote = TabParent::GetFrom(content)) {
      remote->Deactivate();
      LOGFOCUS(("Remote browser deactivated"));
    }
  }

  bool result = true;
  if (sendBlurEvent) {
    // if there is an active window, update commands. If there isn't an active
    // window, then this was a blur caused by the active window being lowered,
    // so there is no need to update the commands
    if (mActiveWindow)
      window->UpdateCommands(NS_LITERAL_STRING("focus"), nullptr, 0);

    SendFocusOrBlurEvent(eBlur, presShell,
                         content->GetComposedDoc(), content, 1,
                         false, false, aContentToFocus);
  }

  // if we are leaving the document or the window was lowered, make the caret
  // invisible.
  if (aIsLeavingDocument || !mActiveWindow) {
    SetCaretVisible(presShell, false, nullptr);
  }

  RefPtr<AccessibleCaretEventHub> eventHub = presShell->GetAccessibleCaretEventHub();
  if (eventHub) {
    eventHub->NotifyBlur(aIsLeavingDocument || !mActiveWindow);
  }

  // at this point, it is expected that this window will be still be
  // focused, but the focused content will be null, as it was cleared before
  // the event. If this isn't the case, then something else was focused during
  // the blur event above and we should just return. However, if
  // aIsLeavingDocument is set, a new document is desired, so make sure to
  // blur the document and window.
  if (mFocusedWindow != window ||
      (mFocusedContent != nullptr && !aIsLeavingDocument)) {
    result = false;
  }
  else if (aIsLeavingDocument) {
    window->TakeFocus(false, 0);

    // clear the focus so that the ancestor frame hierarchy is in the correct
    // state. Pass true because aAncestorWindowToFocus is thought to be
    // focused at this point.
    if (aAncestorWindowToFocus)
      aAncestorWindowToFocus->SetFocusedNode(nullptr, 0, true);

    SetFocusedWindowInternal(nullptr);
    mFocusedContent = nullptr;

    // pass 1 for the focus method when calling SendFocusOrBlurEvent just so
    // that the check is made for suppressed documents. Check to ensure that
    // the document isn't null in case someone closed it during the blur above
    nsIDocument* doc = window->GetExtantDoc();
    if (doc)
      SendFocusOrBlurEvent(eBlur, presShell, doc, doc, 1, false);
    if (mFocusedWindow == nullptr)
      SendFocusOrBlurEvent(eBlur, presShell, doc,
                           window->GetCurrentInnerWindow(), 1, false);

    // check if a different window was focused
    result = (mFocusedWindow == nullptr && mActiveWindow);
  }
  else if (mActiveWindow) {
    // Otherwise, the blur of the element without blurring the document
    // occurred normally. Call UpdateCaret to redisplay the caret at the right
    // location within the document. This is needed to ensure that the caret
    // used for caret browsing is made visible again when an input field is
    // blurred.
    UpdateCaret(false, true, nullptr);
  }

  if (clearFirstBlurEvent)
    mFirstBlurEvent = nullptr;

  return result;
}

void
nsFocusManager::Focus(nsPIDOMWindowOuter* aWindow,
                      nsIContent* aContent,
                      uint32_t aFlags,
                      bool aIsNewDocument,
                      bool aFocusChanged,
                      bool aWindowRaised,
                      bool aAdjustWidgets,
                      nsIContent* aContentLostFocus)
{
  LOGFOCUS(("<<Focus begin>>"));

  if (!aWindow)
    return;

  if (aContent && (aContent == mFirstFocusEvent || aContent == mFirstBlurEvent))
    return;

  // Keep a reference to the presShell since dispatching the DOM event may
  // cause the document to be destroyed.
  nsCOMPtr<nsIDocShell> docShell = aWindow->GetDocShell();
  if (!docShell)
    return;

  nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell();
  if (!presShell)
    return;

  // If the focus actually changed, set the focus method (mouse, keyboard, etc).
  // Otherwise, just get the current focus method and use that. This ensures
  // that the method is set during the document and window focus events.
  uint32_t focusMethod = aFocusChanged ? aFlags & FOCUSMETHODANDRING_MASK :
                         aWindow->GetFocusMethod() | (aFlags & FLAG_SHOWRING);

  if (!IsWindowVisible(aWindow)) {
    // if the window isn't visible, for instance because it is a hidden tab,
    // update the current focus and scroll it into view but don't do anything else
    if (CheckIfFocusable(aContent, aFlags)) {
      aWindow->SetFocusedNode(aContent, focusMethod);
      if (aFocusChanged)
        ScrollIntoView(presShell, aContent, aFlags);
    }
    return;
  }

  bool clearFirstFocusEvent = false;
  if (!mFirstFocusEvent) {
    mFirstFocusEvent = aContent;
    clearFirstFocusEvent = true;
  }

  LOGCONTENT("Element %s has been focused", aContent);

  if (MOZ_LOG_TEST(gFocusLog, LogLevel::Debug)) {
    nsIDocument* docm = aWindow->GetExtantDoc();
    if (docm) {
      LOGCONTENT(" from %s", docm->GetRootElement());
    }
    LOGFOCUS((" [Newdoc: %d FocusChanged: %d Raised: %d Flags: %x]",
             aIsNewDocument, aFocusChanged, aWindowRaised, aFlags));
  }

  if (aIsNewDocument) {
    // if this is a new document, update the parent chain of frames so that
    // focus can be traversed from the top level down to the newly focused
    // window.
    AdjustWindowFocus(aWindow, false);
  }

  // indicate that the window has taken focus.
  if (aWindow->TakeFocus(true, focusMethod))
    aIsNewDocument = true;

  SetFocusedWindowInternal(aWindow);

  // Update the system focus by focusing the root widget.  But avoid this
  // if 1) aAdjustWidgets is false or 2) aContent is a plugin that has its
  // own widget and is either already focused or is about to be focused.
  nsCOMPtr<nsIWidget> objectFrameWidget;
  if (aContent) {
    nsIFrame* contentFrame = aContent->GetPrimaryFrame();
    nsIObjectFrame* objectFrame = do_QueryFrame(contentFrame);
    if (objectFrame)
      objectFrameWidget = objectFrame->GetWidget();
  }
  if (aAdjustWidgets && !objectFrameWidget && !sTestMode) {
    nsViewManager* vm = presShell->GetViewManager();
    if (vm) {
      nsCOMPtr<nsIWidget> widget;
      vm->GetRootWidget(getter_AddRefs(widget));
      if (widget)
        widget->SetFocus(false);
    }
  }

  // if switching to a new document, first fire the focus event on the
  // document and then the window.
  if (aIsNewDocument) {
    nsIDocument* doc = aWindow->GetExtantDoc();
    // The focus change should be notified to IMEStateManager from here if
    // the focused content is a designMode editor since any content won't
    // receive focus event.
    if (doc && doc->HasFlag(NODE_IS_EDITABLE)) {
      IMEStateManager::OnChangeFocus(presShell->GetPresContext(), nullptr,
                                     GetFocusMoveActionCause(aFlags));
    }
    if (doc)
      SendFocusOrBlurEvent(eFocus, presShell, doc,
                           doc, aFlags & FOCUSMETHOD_MASK, aWindowRaised);
    if (mFocusedWindow == aWindow && mFocusedContent == nullptr)
      SendFocusOrBlurEvent(eFocus, presShell, doc,
                           aWindow->GetCurrentInnerWindow(),
                           aFlags & FOCUSMETHOD_MASK, aWindowRaised);
  }

  // check to ensure that the element is still focusable, and that nothing
  // else was focused during the events above.
  if (CheckIfFocusable(aContent, aFlags) &&
      mFocusedWindow == aWindow && mFocusedContent == nullptr) {
    mFocusedContent = aContent;

    nsIContent* focusedNode = aWindow->GetFocusedNode();
    bool isRefocus = focusedNode && focusedNode->IsEqualNode(aContent);

    aWindow->SetFocusedNode(aContent, focusMethod);

    bool sendFocusEvent =
      aContent && aContent->IsInComposedDoc() && !IsNonFocusableRoot(aContent);
    nsPresContext* presContext = presShell->GetPresContext();
    if (sendFocusEvent) {
      // if the focused element changed, scroll it into view
      if (aFocusChanged)
        ScrollIntoView(presShell, aContent, aFlags);

      NotifyFocusStateChange(aContent, aWindow->ShouldShowFocusRing(), true);

      // if this is an object/plug-in/remote browser, focus its widget.  Note that we might
      // no longer be in the same document, due to the events we fired above when
      // aIsNewDocument.
      if (presShell->GetDocument() == aContent->GetComposedDoc()) {
        if (aAdjustWidgets && objectFrameWidget && !sTestMode)
          objectFrameWidget->SetFocus(false);

        // if the object being focused is a remote browser, activate remote content
        if (TabParent* remote = TabParent::GetFrom(aContent)) {
          remote->Activate();
          LOGFOCUS(("Remote browser activated"));
        }
      }

      IMEStateManager::OnChangeFocus(presContext, aContent,
                                     GetFocusMoveActionCause(aFlags));

      // as long as this focus wasn't because a window was raised, update the
      // commands
      // XXXndeakin P2 someone could adjust the focus during the update
      if (!aWindowRaised)
        aWindow->UpdateCommands(NS_LITERAL_STRING("focus"), nullptr, 0);

      SendFocusOrBlurEvent(eFocus, presShell,
                           aContent->GetComposedDoc(),
                           aContent, aFlags & FOCUSMETHOD_MASK,
                           aWindowRaised, isRefocus, aContentLostFocus);
    } else {
      IMEStateManager::OnChangeFocus(presContext, nullptr,
                                     GetFocusMoveActionCause(aFlags));
      if (!aWindowRaised) {
        aWindow->UpdateCommands(NS_LITERAL_STRING("focus"), nullptr, 0);
      }
    }
  }
  else {
    // If the window focus event (fired above when aIsNewDocument) caused
    // the plugin not to be focusable, update the system focus by focusing
    // the root widget.
    if (aAdjustWidgets && objectFrameWidget &&
        mFocusedWindow == aWindow && mFocusedContent == nullptr &&
        !sTestMode) {
      nsViewManager* vm = presShell->GetViewManager();
      if (vm) {
        nsCOMPtr<nsIWidget> widget;
        vm->GetRootWidget(getter_AddRefs(widget));
        if (widget)
          widget->SetFocus(false);
      }
    }

    if (!mFocusedContent) {
      // When there is no focused content, IMEStateManager needs to adjust IME
      // enabled state with the document.
      nsPresContext* presContext = presShell->GetPresContext();
      IMEStateManager::OnChangeFocus(presContext, nullptr,
                                     GetFocusMoveActionCause(aFlags));
    }

    if (!aWindowRaised)
      aWindow->UpdateCommands(NS_LITERAL_STRING("focus"), nullptr, 0);
  }

  // update the caret visibility and position to match the newly focused
  // element. However, don't update the position if this was a focus due to a
  // mouse click as the selection code would already have moved the caret as
  // needed. If this is a different document than was focused before, also
  // update the caret's visibility. If this is the same document, the caret
  // visibility should be the same as before so there is no need to update it.
  if (mFocusedContent == aContent)
    UpdateCaret(aFocusChanged && !(aFlags & FLAG_BYMOUSE), aIsNewDocument,
                mFocusedContent);

  if (clearFirstFocusEvent)
    mFirstFocusEvent = nullptr;
}

class FocusBlurEvent : public Runnable
{
public:
  FocusBlurEvent(nsISupports* aTarget, EventMessage aEventMessage,
                 nsPresContext* aContext, bool aWindowRaised,
                 bool aIsRefocus, EventTarget* aRelatedTarget)
    : mTarget(aTarget)
    , mContext(aContext)
    , mEventMessage(aEventMessage)
    , mWindowRaised(aWindowRaised)
    , mIsRefocus(aIsRefocus)
    , mRelatedTarget(aRelatedTarget)
  {
  }

  NS_IMETHOD Run()
  {
    InternalFocusEvent event(true, mEventMessage);
    event.mFlags.mBubbles = false;
    event.mFlags.mCancelable = false;
    event.mFromRaise = mWindowRaised;
    event.mIsRefocus = mIsRefocus;
    event.mRelatedTarget = mRelatedTarget;
    return EventDispatcher::Dispatch(mTarget, mContext, &event);
  }

  nsCOMPtr<nsISupports>   mTarget;
  RefPtr<nsPresContext> mContext;
  EventMessage            mEventMessage;
  bool                    mWindowRaised;
  bool                    mIsRefocus;
  nsCOMPtr<EventTarget>   mRelatedTarget;
};

static nsIDocument*
GetDocumentHelper(EventTarget* aTarget)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aTarget);
  if (!node) {
    nsCOMPtr<nsPIDOMWindowInner> win = do_QueryInterface(aTarget);
    return win ? win->GetExtantDoc() : nullptr;
  }

  return node->OwnerDoc();
}

void
nsFocusManager::SendFocusOrBlurEvent(EventMessage aEventMessage,
                                     nsIPresShell* aPresShell,
                                     nsIDocument* aDocument,
                                     nsISupports* aTarget,
                                     uint32_t aFocusMethod,
                                     bool aWindowRaised,
                                     bool aIsRefocus,
                                     EventTarget* aRelatedTarget)
{
  NS_ASSERTION(aEventMessage == eFocus || aEventMessage == eBlur,
               "Wrong event type for SendFocusOrBlurEvent");

  nsCOMPtr<EventTarget> eventTarget = do_QueryInterface(aTarget);
  nsCOMPtr<nsIDocument> eventTargetDoc = GetDocumentHelper(eventTarget);
  nsCOMPtr<nsIDocument> relatedTargetDoc = GetDocumentHelper(aRelatedTarget);

  // set aRelatedTarget to null if it's not in the same document as eventTarget
  if (eventTargetDoc != relatedTargetDoc) {
    aRelatedTarget = nullptr;
  }

  bool dontDispatchEvent =
    eventTargetDoc && nsContentUtils::IsUserFocusIgnored(eventTargetDoc);

  // for focus events, if this event was from a mouse or key and event
  // handling on the document is suppressed, queue the event and fire it
  // later. For blur events, a non-zero value would be set for aFocusMethod.
  if (aFocusMethod && !dontDispatchEvent &&
      aDocument && aDocument->EventHandlingSuppressed()) {
    // aFlags is always 0 when aWindowRaised is true so this won't be called
    // on a window raise.
    NS_ASSERTION(!aWindowRaised, "aWindowRaised should not be set");

    for (uint32_t i = mDelayedBlurFocusEvents.Length(); i > 0; --i) {
      // if this event was already queued, remove it and append it to the end
      if (mDelayedBlurFocusEvents[i - 1].mEventMessage == aEventMessage &&
          mDelayedBlurFocusEvents[i - 1].mPresShell == aPresShell &&
          mDelayedBlurFocusEvents[i - 1].mDocument == aDocument &&
          mDelayedBlurFocusEvents[i - 1].mTarget == eventTarget &&
          mDelayedBlurFocusEvents[i - 1].mRelatedTarget == aRelatedTarget) {
        mDelayedBlurFocusEvents.RemoveElementAt(i - 1);
      }
    }

    mDelayedBlurFocusEvents.AppendElement(
      nsDelayedBlurOrFocusEvent(aEventMessage, aPresShell,
                                aDocument, eventTarget, aRelatedTarget));
    return;
  }

#ifdef ACCESSIBILITY
  nsAccessibilityService* accService = GetAccService();
  if (accService) {
    if (aEventMessage == eFocus) {
      accService->NotifyOfDOMFocus(aTarget);
    } else {
      accService->NotifyOfDOMBlur(aTarget);
    }
  }
#endif

  if (!dontDispatchEvent) {
    nsContentUtils::AddScriptRunner(
      new FocusBlurEvent(aTarget, aEventMessage, aPresShell->GetPresContext(),
                         aWindowRaised, aIsRefocus, aRelatedTarget));
  }
}

void
nsFocusManager::ScrollIntoView(nsIPresShell* aPresShell,
                               nsIContent* aContent,
                               uint32_t aFlags)
{
  // if the noscroll flag isn't set, scroll the newly focused element into view
  if (!(aFlags & FLAG_NOSCROLL))
    aPresShell->ScrollContentIntoView(aContent,
                                      nsIPresShell::ScrollAxis(
                                        nsIPresShell::SCROLL_MINIMUM,
                                        nsIPresShell::SCROLL_IF_NOT_VISIBLE),
                                      nsIPresShell::ScrollAxis(
                                        nsIPresShell::SCROLL_MINIMUM,
                                        nsIPresShell::SCROLL_IF_NOT_VISIBLE),
                                      nsIPresShell::SCROLL_OVERFLOW_HIDDEN);
}


void
nsFocusManager::RaiseWindow(nsPIDOMWindowOuter* aWindow)
{
  // don't raise windows that are already raised or are in the process of
  // being lowered
  if (!aWindow || aWindow == mActiveWindow || aWindow == mWindowBeingLowered)
    return;

  if (sTestMode) {
    // In test mode, emulate the existing window being lowered and the new
    // window being raised.
    if (mActiveWindow)
      WindowLowered(mActiveWindow);
    WindowRaised(aWindow);
    return;
  }

#if defined(XP_WIN)
  // Windows would rather we focus the child widget, otherwise, the toplevel
  // widget will always end up being focused. Fortunately, focusing the child
  // widget will also have the effect of raising the window this widget is in.
  // But on other platforms, we can just focus the toplevel widget to raise
  // the window.
  nsCOMPtr<nsPIDOMWindowOuter> childWindow;
  GetFocusedDescendant(aWindow, true, getter_AddRefs(childWindow));
  if (!childWindow)
    childWindow = aWindow;

  nsCOMPtr<nsIDocShell> docShell = aWindow->GetDocShell();
  if (!docShell)
    return;

  nsCOMPtr<nsIPresShell> presShell = docShell->GetPresShell();
  if (!presShell)
    return;

  nsViewManager* vm = presShell->GetViewManager();
  if (vm) {
    nsCOMPtr<nsIWidget> widget;
    vm->GetRootWidget(getter_AddRefs(widget));
    if (widget)
      widget->SetFocus(true);
  }
#else
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin =
    do_QueryInterface(aWindow->GetDocShell());
  if (treeOwnerAsWin) {
    nsCOMPtr<nsIWidget> widget;
    treeOwnerAsWin->GetMainWidget(getter_AddRefs(widget));
    if (widget)
      widget->SetFocus(true);
  }
#endif
}

void
nsFocusManager::UpdateCaretForCaretBrowsingMode()
{
  UpdateCaret(false, true, mFocusedContent);
}

void
nsFocusManager::UpdateCaret(bool aMoveCaretToFocus,
                            bool aUpdateVisibility,
                            nsIContent* aContent)
{
  LOGFOCUS(("Update Caret: %d %d", aMoveCaretToFocus, aUpdateVisibility));

  if (!mFocusedWindow)
    return;

  // this is called when a document is focused or when the caretbrowsing
  // preference is changed
  nsCOMPtr<nsIDocShell> focusedDocShell = mFocusedWindow->GetDocShell();
  nsCOMPtr<nsIDocShellTreeItem> dsti = do_QueryInterface(focusedDocShell);
  if (!dsti)
    return;

  if (dsti->ItemType() == nsIDocShellTreeItem::typeChrome) {
    return;  // Never browse with caret in chrome
  }

  bool browseWithCaret =
    Preferences::GetBool("accessibility.browsewithcaret");

  nsCOMPtr<nsIPresShell> presShell = focusedDocShell->GetPresShell();
  if (!presShell)
    return;

  // If this is an editable document which isn't contentEditable, or a
  // contentEditable document and the node to focus is contentEditable,
  // return, so that we don't mess with caret visibility.
  bool isEditable = false;
  focusedDocShell->GetEditable(&isEditable);

  if (isEditable) {
    nsCOMPtr<nsIHTMLDocument> doc =
      do_QueryInterface(presShell->GetDocument());

    bool isContentEditableDoc =
      doc && doc->GetEditingState() == nsIHTMLDocument::eContentEditable;

    bool isFocusEditable =
      aContent && aContent->HasFlag(NODE_IS_EDITABLE);
    if (!isContentEditableDoc || isFocusEditable)
      return;
  }

  if (!isEditable && aMoveCaretToFocus)
    MoveCaretToFocus(presShell, aContent);

  if (!aUpdateVisibility)
    return;

  // XXXndeakin this doesn't seem right. It should be checking for this only
  // on the nearest ancestor frame which is a chrome frame. But this is
  // what the existing code does, so just leave it for now.
  if (!browseWithCaret) {
    nsCOMPtr<Element> docElement =
      mFocusedWindow->GetFrameElementInternal();
    if (docElement)
      browseWithCaret = docElement->AttrValueIs(kNameSpaceID_None,
                                                nsGkAtoms::showcaret,
                                                NS_LITERAL_STRING("true"),
                                                eCaseMatters);
  }

  SetCaretVisible(presShell, browseWithCaret, aContent);
}

void
nsFocusManager::MoveCaretToFocus(nsIPresShell* aPresShell, nsIContent* aContent)
{
  // domDoc is a document interface we can create a range with
  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(aPresShell->GetDocument());
  if (domDoc) {
    RefPtr<nsFrameSelection> frameSelection = aPresShell->FrameSelection();
    nsCOMPtr<nsISelection> domSelection = frameSelection->
      GetSelection(nsISelectionController::SELECTION_NORMAL);
    if (domSelection) {
      nsCOMPtr<nsIDOMNode> currentFocusNode(do_QueryInterface(aContent));
      // First clear the selection. This way, if there is no currently focused
      // content, the selection will just be cleared.
      domSelection->RemoveAllRanges();
      if (currentFocusNode) {
        nsCOMPtr<nsIDOMRange> newRange;
        nsresult rv = domDoc->CreateRange(getter_AddRefs(newRange));
        if (NS_SUCCEEDED(rv)) {
          // Set the range to the start of the currently focused node
          // Make sure it's collapsed
          newRange->SelectNodeContents(currentFocusNode);
          nsCOMPtr<nsIDOMNode> firstChild;
          currentFocusNode->GetFirstChild(getter_AddRefs(firstChild));
          if (!firstChild ||
              aContent->IsNodeOfType(nsINode::eHTML_FORM_CONTROL)) {
            // If current focus node is a leaf, set range to before the
            // node by using the parent as a container.
            // This prevents it from appearing as selected.
            newRange->SetStartBefore(currentFocusNode);
            newRange->SetEndBefore(currentFocusNode);
          }
          domSelection->AddRange(newRange);
          domSelection->CollapseToStart();
        }
      }
    }
  }
}

nsresult
nsFocusManager::SetCaretVisible(nsIPresShell* aPresShell,
                                bool aVisible,
                                nsIContent* aContent)
{
  // When browsing with caret, make sure caret is visible after new focus
  // Return early if there is no caret. This can happen for the testcase
  // for bug 308025 where a window is closed in a blur handler.
  RefPtr<nsCaret> caret = aPresShell->GetCaret();
  if (!caret)
    return NS_OK;

  bool caretVisible = caret->IsVisible();
  if (!aVisible && !caretVisible)
    return NS_OK;

  RefPtr<nsFrameSelection> frameSelection;
  if (aContent) {
    NS_ASSERTION(aContent->GetComposedDoc() == aPresShell->GetDocument(),
                 "Wrong document?");
    nsIFrame *focusFrame = aContent->GetPrimaryFrame();
    if (focusFrame)
      frameSelection = focusFrame->GetFrameSelection();
  }

  RefPtr<nsFrameSelection> docFrameSelection = aPresShell->FrameSelection();

  if (docFrameSelection && caret &&
     (frameSelection == docFrameSelection || !aContent)) {
    nsISelection* domSelection = docFrameSelection->
      GetSelection(nsISelectionController::SELECTION_NORMAL);
    if (domSelection) {
      nsCOMPtr<nsISelectionController> selCon(do_QueryInterface(aPresShell));
      if (!selCon) {
        return NS_ERROR_FAILURE;
      }
      // First, hide the caret to prevent attempting to show it in SetCaretDOMSelection
      selCon->SetCaretEnabled(false);

      // Caret must blink on non-editable elements
      caret->SetIgnoreUserModify(true);
      // Tell the caret which selection to use
      caret->SetSelection(domSelection);

      // In content, we need to set the caret. The only special case is edit
      // fields, which have a different frame selection from the document.
      // They will take care of making the caret visible themselves.

      selCon->SetCaretReadOnly(false);
      selCon->SetCaretEnabled(aVisible);
    }
  }

  return NS_OK;
}

nsresult
nsFocusManager::GetSelectionLocation(nsIDocument* aDocument,
                                     nsIPresShell* aPresShell,
                                     nsIContent **aStartContent,
                                     nsIContent **aEndContent)
{
  *aStartContent = *aEndContent = nullptr;
  nsresult rv = NS_ERROR_FAILURE;

  nsPresContext* presContext = aPresShell->GetPresContext();
  NS_ASSERTION(presContext, "mPresContent is null!!");

  RefPtr<nsFrameSelection> frameSelection = aPresShell->FrameSelection();

  nsCOMPtr<nsISelection> domSelection;
  if (frameSelection) {
    domSelection = frameSelection->
      GetSelection(nsISelectionController::SELECTION_NORMAL);
  }

  nsCOMPtr<nsIDOMNode> startNode, endNode;
  bool isCollapsed = false;
  nsCOMPtr<nsIContent> startContent, endContent;
  int32_t startOffset = 0;
  if (domSelection) {
    domSelection->GetIsCollapsed(&isCollapsed);
    nsCOMPtr<nsIDOMRange> domRange;
    rv = domSelection->GetRangeAt(0, getter_AddRefs(domRange));
    if (domRange) {
      domRange->GetStartContainer(getter_AddRefs(startNode));
      domRange->GetEndContainer(getter_AddRefs(endNode));
      domRange->GetStartOffset(&startOffset);

      nsIContent *childContent = nullptr;

      startContent = do_QueryInterface(startNode);
      if (startContent && startContent->IsElement()) {
        NS_ASSERTION(startOffset >= 0, "Start offset cannot be negative");  
        childContent = startContent->GetChildAt(startOffset);
        if (childContent) {
          startContent = childContent;
        }
      }

      endContent = do_QueryInterface(endNode);
      if (endContent && endContent->IsElement()) {
        int32_t endOffset = 0;
        domRange->GetEndOffset(&endOffset);
        NS_ASSERTION(endOffset >= 0, "End offset cannot be negative");
        childContent = endContent->GetChildAt(endOffset);
        if (childContent) {
          endContent = childContent;
        }
      }
    }
  }
  else {
    rv = NS_ERROR_INVALID_ARG;
  }

  nsIFrame *startFrame = nullptr;
  if (startContent) {
    startFrame = startContent->GetPrimaryFrame();
    if (isCollapsed) {
      // Next check to see if our caret is at the very end of a node
      // If so, the caret is actually sitting in front of the next
      // logical frame's primary node - so for this case we need to
      // change caretContent to that node.

      if (startContent->NodeType() == nsIDOMNode::TEXT_NODE) {
        nsAutoString nodeValue;
        startContent->AppendTextTo(nodeValue);

        bool isFormControl =
          startContent->IsNodeOfType(nsINode::eHTML_FORM_CONTROL);

        if (nodeValue.Length() == (uint32_t)startOffset && !isFormControl &&
            startContent != aDocument->GetRootElement()) {
          // Yes, indeed we were at the end of the last node
          nsCOMPtr<nsIFrameEnumerator> frameTraversal;
          nsresult rv = NS_NewFrameTraversal(getter_AddRefs(frameTraversal),
                                             presContext, startFrame,
                                             eLeaf,
                                             false, // aVisual
                                             false, // aLockInScrollView
                                             true,  // aFollowOOFs
                                             false  // aSkipPopupChecks
                                             );
          NS_ENSURE_SUCCESS(rv, rv);

          nsIFrame *newCaretFrame = nullptr;
          nsCOMPtr<nsIContent> newCaretContent = startContent;
          bool endOfSelectionInStartNode(startContent == endContent);
          do {
            // Continue getting the next frame until the primary content for the frame
            // we are on changes - we don't want to be stuck in the same place
            frameTraversal->Next();
            newCaretFrame = static_cast<nsIFrame*>(frameTraversal->CurrentItem());
            if (nullptr == newCaretFrame)
              break;
            newCaretContent = newCaretFrame->GetContent();
          } while (!newCaretContent || newCaretContent == startContent);

          if (newCaretFrame && newCaretContent) {
            // If the caret is exactly at the same position of the new frame,
            // then we can use the newCaretFrame and newCaretContent for our position
            nsRect caretRect;
            nsIFrame *frame = nsCaret::GetGeometry(domSelection, &caretRect);
            if (frame) {
              nsPoint caretWidgetOffset;
              nsIWidget *widget = frame->GetNearestWidget(caretWidgetOffset);
              caretRect.MoveBy(caretWidgetOffset);
              nsPoint newCaretOffset;
              nsIWidget *newCaretWidget = newCaretFrame->GetNearestWidget(newCaretOffset);
              if (widget == newCaretWidget && caretRect.y == newCaretOffset.y &&
                  caretRect.x == newCaretOffset.x) {
                // The caret is at the start of the new element.
                startFrame = newCaretFrame;
                startContent = newCaretContent;
                if (endOfSelectionInStartNode) {
                  endContent = newCaretContent; // Ensure end of selection is not before start
                }
              }
            }
          }
        }
      }
    }
  }

  *aStartContent = startContent;
  *aEndContent = endContent;
  NS_IF_ADDREF(*aStartContent);
  NS_IF_ADDREF(*aEndContent);

  return rv;
}

nsresult
nsFocusManager::DetermineElementToMoveFocus(nsPIDOMWindowOuter* aWindow,
                                            nsIContent* aStartContent,
                                            int32_t aType, bool aNoParentTraversal,
                                            nsIContent** aNextContent)
{
  *aNextContent = nullptr;

  // True if we are navigating by document (F6/Shift+F6) or false if we are
  // navigating by element (Tab/Shift+Tab).
  bool forDocumentNavigation = false;

  // This is used for document navigation only. It will be set to true if we
  // start navigating from a starting point. If this starting point is near the
  // end of the document (for example, an element on a statusbar), and there
  // are no child documents or panels before the end of the document, then we
  // will need to ensure that we don't consider the root chrome window when we
  // loop around and instead find the next child document/panel, as focus is
  // already in that window. This flag will be cleared once we navigate into
  // another document.
  bool mayFocusRoot = (aStartContent != nullptr);

  nsCOMPtr<nsIContent> startContent = aStartContent;
  if (!startContent && aType != MOVEFOCUS_CARET) {
    if (aType == MOVEFOCUS_FORWARDDOC || aType == MOVEFOCUS_BACKWARDDOC) {
      // When moving between documents, make sure to get the right
      // starting content in a descendant.
      nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
      startContent = GetFocusedDescendant(aWindow, true, getter_AddRefs(focusedWindow));
    }
    else if (aType != MOVEFOCUS_LASTDOC) {
      // Otherwise, start at the focused node. If MOVEFOCUS_LASTDOC is used,
      // then we are document-navigating backwards from chrome to the content
      // process, and we don't want to use this so that we start from the end
      // of the document.
      startContent = aWindow->GetFocusedNode();
    }
  }

  nsCOMPtr<nsIDocument> doc;
  if (startContent)
    doc = startContent->GetComposedDoc();
  else
    doc = aWindow->GetExtantDoc();
  if (!doc)
    return NS_OK;

  LookAndFeel::GetInt(LookAndFeel::eIntID_TabFocusModel,
                      &nsIContent::sTabFocusModel);

  // These types are for document navigation using F6.
  if (aType == MOVEFOCUS_FORWARDDOC || aType == MOVEFOCUS_BACKWARDDOC ||
      aType == MOVEFOCUS_FIRSTDOC || aType == MOVEFOCUS_LASTDOC) {
    forDocumentNavigation = true;
  }

  // If moving to the root or first document, find the root element and return.
  if (aType == MOVEFOCUS_ROOT || aType == MOVEFOCUS_FIRSTDOC) {
    NS_IF_ADDREF(*aNextContent = GetRootForFocus(aWindow, doc, false, false));
    if (!*aNextContent && aType == MOVEFOCUS_FIRSTDOC) {
      // When looking for the first document, if the root wasn't focusable,
      // find the next focusable document.
      aType = MOVEFOCUS_FORWARDDOC;
    } else {
      return NS_OK;
    }
  }

  nsIContent* rootContent = doc->GetRootElement();
  NS_ENSURE_TRUE(rootContent, NS_OK);

  nsIPresShell *presShell = doc->GetShell();
  NS_ENSURE_TRUE(presShell, NS_OK);

  if (aType == MOVEFOCUS_FIRST) {
    if (!aStartContent)
      startContent = rootContent;
    return GetNextTabbableContent(presShell, startContent,
                                  nullptr, startContent,
                                  true, 1, false, false, aNextContent);
  }
  if (aType == MOVEFOCUS_LAST) {
    if (!aStartContent)
      startContent = rootContent;
    return GetNextTabbableContent(presShell, startContent,
                                  nullptr, startContent,
                                  false, 0, false, false, aNextContent);
  }

  bool forward = (aType == MOVEFOCUS_FORWARD ||
                  aType == MOVEFOCUS_FORWARDDOC ||
                  aType == MOVEFOCUS_CARET);
  bool doNavigation = true;
  bool ignoreTabIndex = false;
  // when a popup is open, we want to ensure that tab navigation occurs only
  // within the most recently opened panel. If a popup is open, its frame will
  // be stored in popupFrame.
  nsIFrame* popupFrame = nullptr;

  int32_t tabIndex = forward ? 1 : 0;
  if (startContent) {
    nsIFrame* frame = startContent->GetPrimaryFrame();
    if (startContent->IsHTMLElement(nsGkAtoms::area))
      startContent->IsFocusable(&tabIndex);
    else if (frame)
      frame->IsFocusable(&tabIndex, 0);
    else
      startContent->IsFocusable(&tabIndex);

    // if the current element isn't tabbable, ignore the tabindex and just
    // look for the next element. The root content won't have a tabindex
    // so just treat this as the beginning of the tab order.
    if (tabIndex < 0) {
      tabIndex = 1;
      if (startContent != rootContent)
        ignoreTabIndex = true;
    }

    // check if the focus is currently inside a popup. Elements such as the
    // autocomplete widget use the noautofocus attribute to allow the focus to
    // remain outside the popup when it is opened.
    if (frame) {
      popupFrame = nsLayoutUtils::GetClosestFrameOfType(frame,
                                                        nsGkAtoms::menuPopupFrame);
    }

    if (popupFrame && !forDocumentNavigation) {
      // Don't navigate outside of a popup, so pretend that the
      // root content is the popup itself
      rootContent = popupFrame->GetContent();
      NS_ASSERTION(rootContent, "Popup frame doesn't have a content node");
    }
    else if (!forward) {
      // If focus moves backward and when current focused node is root
      // content or <body> element which is editable by contenteditable
      // attribute, focus should move to its parent document.
      if (startContent == rootContent) {
        doNavigation = false;
      } else {
        nsIDocument* doc = startContent->GetComposedDoc();
        if (startContent ==
              nsLayoutUtils::GetEditableRootContentByContentEditable(doc)) {
          doNavigation = false;
        }
      }
    }
  }
  else {
#ifdef MOZ_XUL
    if (aType != MOVEFOCUS_CARET) {
      // if there is no focus, yet a panel is open, focus the first item in
      // the panel
      nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
      if (pm)
        popupFrame = pm->GetTopPopup(ePopupTypePanel);
    }
#endif
    if (popupFrame) {
      // When there is a popup open, and no starting content, start the search
      // at the topmost popup.
      startContent = popupFrame->GetContent();
      NS_ASSERTION(startContent, "Popup frame doesn't have a content node");
      // Unless we are searching for documents, set the root content to the
      // popup as well, so that we don't tab-navigate outside the popup.
      // When navigating by documents, we start at the popup but can navigate
      // outside of it to look for other panels and documents.
      if (!forDocumentNavigation) {
        rootContent = startContent;
      }

      doc = startContent ? startContent->GetComposedDoc() : nullptr;
    }
    else {
      // Otherwise, for content shells, start from the location of the caret.
      nsCOMPtr<nsIDocShell> docShell = aWindow->GetDocShell();
      if (docShell && docShell->ItemType() != nsIDocShellTreeItem::typeChrome) {
        nsCOMPtr<nsIContent> endSelectionContent;
        GetSelectionLocation(doc, presShell,
                             getter_AddRefs(startContent),
                             getter_AddRefs(endSelectionContent));
        // If the selection is on the rootContent, then there is no selection
        if (startContent == rootContent) {
          startContent = nullptr;
        }

        if (aType == MOVEFOCUS_CARET) {
          // GetFocusInSelection finds a focusable link near the caret.
          // If there is no start content though, don't do this to avoid
          // focusing something unexpected.
          if (startContent) {
            GetFocusInSelection(aWindow, startContent,
                                endSelectionContent, aNextContent);
          }
          return NS_OK;
        }

        if (startContent) {
          // when starting from a selection, we always want to find the next or
          // previous element in the document. So the tabindex on elements
          // should be ignored.
          ignoreTabIndex = true;
        }
      }

      if (!startContent) {
        // otherwise, just use the root content as the starting point
        startContent = rootContent;
        NS_ENSURE_TRUE(startContent, NS_OK);
      }
    }
  }

  // Check if the starting content is the same as the content assigned to the
  // retargetdocumentfocus attribute. Is so, we don't want to start searching
  // from there but instead from the beginning of the document. Otherwise, the
  // content that appears before the retargetdocumentfocus element will never
  // get checked as it will be skipped when the focus is retargetted to it.
  if (forDocumentNavigation && doc->IsXULDocument()) {
    nsAutoString retarget;

    if (rootContent->GetAttr(kNameSpaceID_None,
                             nsGkAtoms::retargetdocumentfocus, retarget)) {
      nsIContent* retargetElement = doc->GetElementById(retarget);
      // The common case here is the urlbar where focus is on the anonymous
      // input inside the textbox, but the retargetdocumentfocus attribute
      // refers to the textbox. The Contains check will return false and the
      // ContentIsDescendantOf check will return true in this case.
      if (retargetElement && (retargetElement == startContent ||
                              (!retargetElement->Contains(startContent) &&
                              nsContentUtils::ContentIsDescendantOf(startContent, retargetElement)))) {
        startContent = rootContent;
      }
    }
  }

  NS_ASSERTION(startContent, "starting content not set");

  // keep a reference to the starting content. If we find that again, it means
  // we've iterated around completely and we don't want to adjust the focus.
  // The skipOriginalContentCheck will be set to true only for the first time
  // GetNextTabbableContent is called. This ensures that we don't break out
  // when nothing is focused to start with. Specifically,
  // GetNextTabbableContent first checks the root content -- which happens to
  // be the same as the start content -- when nothing is focused and tabbing
  // forward. Without skipOriginalContentCheck set to true, we'd end up
  // returning right away and focusing nothing. Luckily, GetNextTabbableContent
  // will never wrap around on its own, and can only return the original
  // content when it is called a second time or later.
  bool skipOriginalContentCheck = true;
  nsIContent* originalStartContent = startContent;

  LOGCONTENTNAVIGATION("Focus Navigation Start Content %s", startContent.get());
  LOGFOCUSNAVIGATION(("  Forward: %d Tabindex: %d Ignore: %d DocNav: %d",
                      forward, tabIndex, ignoreTabIndex, forDocumentNavigation));

  while (doc) {
    if (doNavigation) {
      nsCOMPtr<nsIContent> nextFocus;
      nsresult rv = GetNextTabbableContent(presShell, rootContent,
                                           skipOriginalContentCheck ? nullptr : originalStartContent,
                                           startContent, forward,
                                           tabIndex, ignoreTabIndex,
                                           forDocumentNavigation,
                                           getter_AddRefs(nextFocus));
      NS_ENSURE_SUCCESS(rv, rv);
      if (rv == NS_SUCCESS_DOM_NO_OPERATION) {
        // Navigation was redirected to a child process, so just return.
        return NS_OK;
      }

      // found a content node to focus.
      if (nextFocus) {
        LOGCONTENTNAVIGATION("Next Content: %s", nextFocus.get());

        // as long as the found node was not the same as the starting node,
        // set it as the return value. For document navigation, we can return
        // the same element in case there is only one content node that could
        // be returned, for example, in a child process document. 
        if (nextFocus != originalStartContent || forDocumentNavigation) {
          nextFocus.forget(aNextContent);
        }
        return NS_OK;
      }

      if (popupFrame && !forDocumentNavigation) {
        // in a popup, so start again from the beginning of the popup. However,
        // if we already started at the beginning, then there isn't anything to
        // focus, so just return
        if (startContent != rootContent) {
          startContent = rootContent;
          tabIndex = forward ? 1 : 0;
          continue;
        }
        return NS_OK;
      }
    }

    doNavigation = true;
    skipOriginalContentCheck = forDocumentNavigation;
    ignoreTabIndex = false;

    if (aNoParentTraversal) {
      if (startContent == rootContent)
        return NS_OK;

      startContent = rootContent;
      tabIndex = forward ? 1 : 0;
      continue;
    }

    // Reached the beginning or end of the document. Next, navigate up to the
    // parent document and try again.
    nsCOMPtr<nsPIDOMWindowOuter> piWindow = doc->GetWindow();
    NS_ENSURE_TRUE(piWindow, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDocShell> docShell = piWindow->GetDocShell();
    NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

    // Get the frame element this window is inside and, from that, get the
    // parent document and presshell. If there is no enclosing frame element,
    // then this is a top-level, embedded or remote window.
    startContent = piWindow->GetFrameElementInternal();
    if (startContent) {
      doc = startContent->GetComposedDoc();
      NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

      rootContent = doc->GetRootElement();
      presShell = doc->GetShell();

      // We can focus the root element now that we have moved to another document.
      mayFocusRoot = true;

      nsIFrame* frame = startContent->GetPrimaryFrame();
      if (!frame) {
        return NS_OK;
      }

      frame->IsFocusable(&tabIndex, 0);
      if (tabIndex < 0) {
        tabIndex = 1;
        ignoreTabIndex = true;
      }

      // if the frame is inside a popup, make sure to scan only within the
      // popup. This handles the situation of tabbing amongst elements
      // inside an iframe which is itself inside a popup. Otherwise,
      // navigation would move outside the popup when tabbing outside the
      // iframe.
      if (!forDocumentNavigation) {
        popupFrame = nsLayoutUtils::GetClosestFrameOfType(frame,
                                                          nsGkAtoms::menuPopupFrame);
        if (popupFrame) {
          rootContent = popupFrame->GetContent();
          NS_ASSERTION(rootContent, "Popup frame doesn't have a content node");
        }
      }
    }
    else {
      // There is no parent, so call the tree owner. This will tell the
      // embedder or parent process that it should take the focus.
      bool tookFocus;
      docShell->TabToTreeOwner(forward, forDocumentNavigation, &tookFocus);
      // If the tree owner took the focus, blur the current content.
      if (tookFocus) {
        nsCOMPtr<nsPIDOMWindowOuter> window = docShell->GetWindow();
        if (window->GetFocusedNode() == mFocusedContent)
          Blur(mFocusedWindow, nullptr, true, true);
        else
          window->SetFocusedNode(nullptr);
        return NS_OK;
      }

      // If we have reached the end of the top-level document, focus the
      // first element in the top-level document. This should always happen
      // when navigating by document forwards but when navigating backwards,
      // only do this if we started in another document or within a popup frame.
      // If the focus started in this window outside a popup however, we should
      // continue by looping around to the end again.
      if (forDocumentNavigation && (forward || mayFocusRoot || popupFrame)) {
        // HTML content documents can have their root element focused (a focus
        // ring appears around the entire content area frame). This root
        // appears in the tab order before all of the elements in the document.
        // Chrome documents however cannot be focused directly, so instead we
        // focus the first focusable element within the window.
        // For example, the urlbar.
        nsIContent* root = GetRootForFocus(piWindow, doc, true, true);
        return FocusFirst(root, aNextContent);
      }

      // Once we have hit the top-level and have iterated to the end again, we
      // just want to break out next time we hit this spot to prevent infinite
      // iteration.
      mayFocusRoot = true;

      // reset the tab index and start again from the beginning or end
      startContent = rootContent;
      tabIndex = forward ? 1 : 0;
    }

    // wrapped all the way around and didn't find anything to move the focus
    // to, so just break out
    if (startContent == originalStartContent)
      break;
  }

  return NS_OK;
}

nsresult
nsFocusManager::GetNextTabbableContent(nsIPresShell* aPresShell,
                                       nsIContent* aRootContent,
                                       nsIContent* aOriginalStartContent,
                                       nsIContent* aStartContent,
                                       bool aForward,
                                       int32_t aCurrentTabIndex,
                                       bool aIgnoreTabIndex,
                                       bool aForDocumentNavigation,
                                       nsIContent** aResultContent)
{
  *aResultContent = nullptr;

  nsCOMPtr<nsIContent> startContent = aStartContent;
  if (!startContent)
    return NS_OK;

  LOGCONTENTNAVIGATION("GetNextTabbable: %s", aStartContent);
  LOGFOCUSNAVIGATION(("  tabindex: %d", aCurrentTabIndex));

  nsPresContext* presContext = aPresShell->GetPresContext();

  bool getNextFrame = true;
  nsCOMPtr<nsIContent> iterStartContent = aStartContent;
  while (1) {
    nsIFrame* startFrame = iterStartContent->GetPrimaryFrame();
    // if there is no frame, look for another content node that has a frame
    if (!startFrame) {
      // if the root content doesn't have a frame, just return
      if (iterStartContent == aRootContent)
        return NS_OK;

      // look for the next or previous content node in tree order
      iterStartContent = aForward ? iterStartContent->GetNextNode() : iterStartContent->GetPreviousContent();
      // we've already skipped over the initial focused content, so we
      // don't want to traverse frames.
      getNextFrame = false;
      if (iterStartContent)
        continue;

      // otherwise, as a last attempt, just look at the root content
      iterStartContent = aRootContent;
      continue;
    }

    // For tab navigation, pass false for aSkipPopupChecks so that we don't
    // iterate into or out of a popup. For document naviation pass true to
    // ignore these boundaries.
    nsCOMPtr<nsIFrameEnumerator> frameTraversal;
    nsresult rv = NS_NewFrameTraversal(getter_AddRefs(frameTraversal),
                                       presContext, startFrame,
                                       ePreOrder,
                                       false, // aVisual
                                       false, // aLockInScrollView
                                       true,  // aFollowOOFs
                                       aForDocumentNavigation  // aSkipPopupChecks
                                       );
    NS_ENSURE_SUCCESS(rv, rv);

    if (iterStartContent == aRootContent) {
      if (!aForward) {
        frameTraversal->Last();
      } else if (aRootContent->IsFocusable()) {
        frameTraversal->Next();
      }
    }
    else if (getNextFrame &&
             (!iterStartContent ||
              !iterStartContent->IsHTMLElement(nsGkAtoms::area))) {
      // Need to do special check in case we're in an imagemap which has multiple
      // content nodes per frame, so don't skip over the starting frame.
      if (aForward)
        frameTraversal->Next();
      else
        frameTraversal->Prev();
    }

    // Walk frames to find something tabbable matching mCurrentTabIndex
    nsIFrame* frame = static_cast<nsIFrame*>(frameTraversal->CurrentItem());
    while (frame) {
      nsIContent* currentContent = frame->GetContent();

      // For document navigation, check if this element is an open panel. Since
      // panels aren't focusable (tabIndex would be -1), we'll just assume that
      // for document navigation, the tabIndex is 0.
      if (aForDocumentNavigation && currentContent && (aCurrentTabIndex == 0) &&
          currentContent->IsXULElement(nsGkAtoms::panel)) {
        nsMenuPopupFrame* popupFrame = do_QueryFrame(frame);
        // Check if the panel is open. Closed panels are ignored since you can't
        // focus anything in them.
        if (popupFrame && popupFrame->IsOpen()) {
          // When moving backward, skip the popup we started in otherwise it
          // will be selected again.
          bool validPopup = true;
          if (!aForward) {
            nsIContent* content = aStartContent;
            while (content) {
              if (content == currentContent) {
                validPopup = false;
                break;
              }

              content = content->GetParent();
            }
          }

          if (validPopup) {
            // Since a panel isn't focusable itself, find the first focusable
            // content within the popup. If there isn't any focusable content
            // in the popup, skip this popup and continue iterating through the
            // frames. We pass the panel itself (currentContent) as the starting
            // and root content, so that we only find content within the panel.
            // Note also that we pass false for aForDocumentNavigation since we
            // want to locate the first content, not the first document.
            rv = GetNextTabbableContent(aPresShell, currentContent,
                                        nullptr, currentContent,
                                        true, 1, false, false,
                                        aResultContent);
            if (NS_SUCCEEDED(rv) && *aResultContent) {
              return rv;
            }
          }
        }
      }

      // TabIndex not set defaults to 0 for form elements, anchors and other
      // elements that are normally focusable. Tabindex defaults to -1
      // for elements that are not normally focusable.
      // The returned computed tabindex from IsFocusable() is as follows:
      //          < 0 not tabbable at all
      //          == 0 in normal tab order (last after positive tabindexed items)
      //          > 0 can be tabbed to in the order specified by this value
      int32_t tabIndex;
      frame->IsFocusable(&tabIndex, 0);

      LOGCONTENTNAVIGATION("Next Tabbable %s:", frame->GetContent());
      LOGFOCUSNAVIGATION(("  with tabindex: %d expected: %d", tabIndex, aCurrentTabIndex));

      if (tabIndex >= 0) {
        NS_ASSERTION(currentContent, "IsFocusable set a tabindex for a frame with no content");
        if (!aForDocumentNavigation &&
            currentContent->IsHTMLElement(nsGkAtoms::img) &&
            currentContent->HasAttr(kNameSpaceID_None, nsGkAtoms::usemap)) {
          // This is an image with a map. Image map areas are not traversed by
          // nsIFrameTraversal so look for the next or previous area element.
          nsIContent *areaContent =
            GetNextTabbableMapArea(aForward, aCurrentTabIndex,
                                   currentContent, iterStartContent);
          if (areaContent) {
            NS_ADDREF(*aResultContent = areaContent);
            return NS_OK;
          }
        }
        else if (aIgnoreTabIndex || aCurrentTabIndex == tabIndex) {
          // break out if we've wrapped around to the start again.
          if (aOriginalStartContent && currentContent == aOriginalStartContent) {
            NS_ADDREF(*aResultContent = currentContent);
            return NS_OK;
          }

          // If this is a remote child browser, call NavigateDocument to have
          // the child process continue the navigation. Return a special error
          // code to have the caller return early. If the child ends up not
          // being focusable in some way, the child process will call back
          // into document navigation again by calling MoveFocus.
          TabParent* remote = TabParent::GetFrom(currentContent);
          if (remote) {
            remote->NavigateByKey(aForward, aForDocumentNavigation);
            return NS_SUCCESS_DOM_NO_OPERATION;
          }

          // Next, for document navigation, check if this a non-remote child document.
          bool checkSubDocument = true;
          if (aForDocumentNavigation) {
            nsIContent* docRoot = GetRootForChildDocument(currentContent);
            if (docRoot) {
              // If GetRootForChildDocument returned something then call
              // FocusFirst to find the root or first element to focus within
              // the child document. If this is a frameset though, skip this and
              // fall through to the checkSubDocument block below to iterate into
              // the frameset's frames and locate the first focusable frame.
              if (!docRoot->IsHTMLElement(nsGkAtoms::frameset)) {
                return FocusFirst(docRoot, aResultContent);
              }
            } else {
              // Set checkSubDocument to false, as this was neither a frame
              // type element or a child document that was focusable.
              checkSubDocument = false;
            }
          }

          if (checkSubDocument) {
            // found a node with a matching tab index. Check if it is a child
            // frame. If so, navigate into the child frame instead.
            nsIDocument* doc = currentContent->GetComposedDoc();
            NS_ASSERTION(doc, "content not in document");
            nsIDocument* subdoc = doc->GetSubDocumentFor(currentContent);
            if (subdoc && !subdoc->EventHandlingSuppressed()) {
              if (aForward) {
                // when tabbing forward into a frame, return the root
                // frame so that the canvas becomes focused.
                nsCOMPtr<nsPIDOMWindowOuter> subframe = subdoc->GetWindow();
                if (subframe) {
                  *aResultContent = GetRootForFocus(subframe, subdoc, false, true);
                  if (*aResultContent) {
                    NS_ADDREF(*aResultContent);
                    return NS_OK;
                  }
                }
              }
              Element* rootElement = subdoc->GetRootElement();
              nsIPresShell* subShell = subdoc->GetShell();
              if (rootElement && subShell) {
                rv = GetNextTabbableContent(subShell, rootElement,
                                            aOriginalStartContent, rootElement,
                                            aForward, (aForward ? 1 : 0),
                                            false, aForDocumentNavigation, aResultContent);
                NS_ENSURE_SUCCESS(rv, rv);
                if (*aResultContent)
                  return NS_OK;
              }
            }
            // otherwise, use this as the next content node to tab to, unless
            // this was the element we started on. This would happen for
            // instance on an element with child frames, where frame navigation
            // could return the original element again. In that case, just skip
            // it. Also, if the next content node is the root content, then
            // return it. This latter case would happen only if someone made a
            // popup focusable.
            // Also, when going backwards, check to ensure that the focus
            // wouldn't be redirected. Otherwise, for example, when an input in
            // a textbox is focused, the enclosing textbox would be found and
            // the same inner input would be returned again.
            else if (currentContent == aRootContent ||
                     (currentContent != startContent &&
                      (aForward || !GetRedirectedFocus(currentContent)))) {
              NS_ADDREF(*aResultContent = currentContent);
              return NS_OK;
            }
          }
        }
      }
      else if (aOriginalStartContent && currentContent == aOriginalStartContent) {
        // not focusable, so return if we have wrapped around to the original
        // content. This is necessary in case the original starting content was
        // not focusable.
        NS_ADDREF(*aResultContent = currentContent);
        return NS_OK;
      }

      // Move to the next or previous frame, but ignore continuation frames
      // since only the first frame should be involved in focusability.
      // Otherwise, a loop will occur in the following example:
      //   <span tabindex="1">...<a/><a/>...</span>
      // where the text wraps onto multiple lines. Tabbing from the second
      // link can find one of the span's continuation frames between the link
      // and the end of the span, and the span would end up getting focused
      // again.
      do {
        if (aForward)
          frameTraversal->Next();
        else
          frameTraversal->Prev();
        frame = static_cast<nsIFrame*>(frameTraversal->CurrentItem());
      } while (frame && frame->GetPrevContinuation());
    }

    // If already at lowest priority tab (0), end search completely.
    // A bit counterintuitive but true, tabindex order goes 1, 2, ... 32767, 0
    if (aCurrentTabIndex == (aForward ? 0 : 1)) {
      // if going backwards, the canvas should be focused once the beginning
      // has been reached, so get the root element.
      if (!aForward) {
        nsCOMPtr<nsPIDOMWindowOuter> window = GetCurrentWindow(aRootContent);
        NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

        nsCOMPtr<nsIContent> docRoot =
          GetRootForFocus(window, aRootContent->GetComposedDoc(), false, true);
        FocusFirst(docRoot, aResultContent);
      }
      break;
    }

    // continue looking for next highest priority tabindex
    aCurrentTabIndex = GetNextTabIndex(aRootContent, aCurrentTabIndex, aForward);
    startContent = iterStartContent = aRootContent;
  }

  return NS_OK;
}

nsIContent*
nsFocusManager::GetNextTabbableMapArea(bool aForward,
                                       int32_t aCurrentTabIndex,
                                       nsIContent* aImageContent,
                                       nsIContent* aStartContent)
{
  nsAutoString useMap;
  aImageContent->GetAttr(kNameSpaceID_None, nsGkAtoms::usemap, useMap);

  nsCOMPtr<nsIDocument> doc = aImageContent->GetComposedDoc();
  if (doc) {
    nsCOMPtr<nsIContent> mapContent = doc->FindImageMap(useMap);
    if (!mapContent)
      return nullptr;
    uint32_t count = mapContent->GetChildCount();
    // First see if the the start content is in this map

    int32_t index = mapContent->IndexOf(aStartContent);
    int32_t tabIndex;
    if (index < 0 || (aStartContent->IsFocusable(&tabIndex) &&
                      tabIndex != aCurrentTabIndex)) {
      // If aStartContent is in this map we must start iterating past it.
      // We skip the case where aStartContent has tabindex == aStartContent
      // since the next tab ordered element might be before it
      // (or after for backwards) in the child list.
      index = aForward ? -1 : (int32_t)count;
    }

    // GetChildAt will return nullptr if our index < 0 or index >= count
    nsCOMPtr<nsIContent> areaContent;
    while ((areaContent = mapContent->GetChildAt(aForward ? ++index : --index)) != nullptr) {
      if (areaContent->IsFocusable(&tabIndex) && tabIndex == aCurrentTabIndex) {
        return areaContent;
      }
    }
  }

  return nullptr;
}

int32_t
nsFocusManager::GetNextTabIndex(nsIContent* aParent,
                                int32_t aCurrentTabIndex,
                                bool aForward)
{
  int32_t tabIndex, childTabIndex;

  if (aForward) {
    tabIndex = 0;
    for (nsIContent* child = aParent->GetFirstChild();
         child;
         child = child->GetNextSibling()) {
      childTabIndex = GetNextTabIndex(child, aCurrentTabIndex, aForward);
      if (childTabIndex > aCurrentTabIndex && childTabIndex != tabIndex) {
        tabIndex = (tabIndex == 0 || childTabIndex < tabIndex) ? childTabIndex : tabIndex;
      }

      nsAutoString tabIndexStr;
      child->GetAttr(kNameSpaceID_None, nsGkAtoms::tabindex, tabIndexStr);
      nsresult ec;
      int32_t val = tabIndexStr.ToInteger(&ec);
      if (NS_SUCCEEDED (ec) && val > aCurrentTabIndex && val != tabIndex) {
        tabIndex = (tabIndex == 0 || val < tabIndex) ? val : tabIndex;
      }
    }
  }
  else { /* !aForward */
    tabIndex = 1;
    for (nsIContent* child = aParent->GetFirstChild();
         child;
         child = child->GetNextSibling()) {
      childTabIndex = GetNextTabIndex(child, aCurrentTabIndex, aForward);
      if ((aCurrentTabIndex == 0 && childTabIndex > tabIndex) ||
          (childTabIndex < aCurrentTabIndex && childTabIndex > tabIndex)) {
        tabIndex = childTabIndex;
      }

      nsAutoString tabIndexStr;
      child->GetAttr(kNameSpaceID_None, nsGkAtoms::tabindex, tabIndexStr);
      nsresult ec;
      int32_t val = tabIndexStr.ToInteger(&ec);
      if (NS_SUCCEEDED (ec)) {
        if ((aCurrentTabIndex == 0 && val > tabIndex) ||
            (val < aCurrentTabIndex && val > tabIndex) ) {
          tabIndex = val;
        }
      }
    }
  }

  return tabIndex;
}

nsresult
nsFocusManager::FocusFirst(nsIContent* aRootContent, nsIContent** aNextContent)
{
  if (!aRootContent) {
    return NS_OK;
  }

  nsIDocument* doc = aRootContent->GetComposedDoc();
  if (doc) {
    if (doc->IsXULDocument()) {
      // If the redirectdocumentfocus attribute is set, redirect the focus to a
      // specific element. This is primarily used to retarget the focus to the
      // urlbar during document navigation.
      nsAutoString retarget;

      if (aRootContent->GetAttr(kNameSpaceID_None,
                               nsGkAtoms::retargetdocumentfocus, retarget)) {
        nsCOMPtr<nsIContent> retargetElement =
          CheckIfFocusable(doc->GetElementById(retarget), 0);
        if (retargetElement) {
          retargetElement.forget(aNextContent);
          return NS_OK;
        }
      }
    }

    nsCOMPtr<nsIDocShell> docShell = doc->GetDocShell();
    if (docShell->ItemType() == nsIDocShellTreeItem::typeChrome) {
      // If the found content is in a chrome shell, navigate forward one
      // tabbable item so that the first item is focused. Note that we
      // always go forward and not back here.
      nsIPresShell* presShell = doc->GetShell();
      if (presShell) {
        return GetNextTabbableContent(presShell, aRootContent,
                                      nullptr, aRootContent,
                                      true, 1, false, false,
                                      aNextContent);
      }
    }
  }

  NS_ADDREF(*aNextContent = aRootContent);
  return NS_OK;
}

nsIContent*
nsFocusManager::GetRootForFocus(nsPIDOMWindowOuter* aWindow,
                                nsIDocument* aDocument,
                                bool aForDocumentNavigation,
                                bool aCheckVisibility)
{
  if (!aForDocumentNavigation) {
    nsCOMPtr<nsIDocShell> docShell = aWindow->GetDocShell();
    if (docShell->ItemType() == nsIDocShellTreeItem::typeChrome) {
      return nullptr;
    }
  }

  if (aCheckVisibility && !IsWindowVisible(aWindow))
    return nullptr;

  // If the body is contenteditable, use the editor's root element rather than
  // the actual root element.
  nsCOMPtr<nsIContent> rootElement =
    nsLayoutUtils::GetEditableRootContentByContentEditable(aDocument);
  if (!rootElement || !rootElement->GetPrimaryFrame()) {
    rootElement = aDocument->GetRootElement();
    if (!rootElement) {
      return nullptr;
    }
  }

  if (aCheckVisibility && !rootElement->GetPrimaryFrame()) {
    return nullptr;
  }

  // Finally, check if this is a frameset
  nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(aDocument);
  if (htmlDoc) {
    nsIContent* htmlChild = aDocument->GetHtmlChildElement(nsGkAtoms::frameset);
    if (htmlChild) {
      // In document navigation mode, return the frameset so that navigation
      // descends into the child frames.
      return aForDocumentNavigation ? htmlChild : nullptr;
    }
  }

  return rootElement;
}

nsIContent*
nsFocusManager::GetRootForChildDocument(nsIContent* aContent)
{
  // Check for elements that represent child documents, that is, browsers,
  // editors or frames from a frameset. We don't include iframes since we
  // consider them to be an integral part of the same window or page.
  if (!aContent ||
      !(aContent->IsXULElement(nsGkAtoms::browser) ||
        aContent->IsXULElement(nsGkAtoms::editor) ||
        aContent->IsHTMLElement(nsGkAtoms::frame))) {
    return nullptr;
  }

  nsIDocument* doc = aContent->GetComposedDoc();
  if (!doc) {
    return nullptr;
  }

  nsIDocument* subdoc = doc->GetSubDocumentFor(aContent);
  if (!subdoc || subdoc->EventHandlingSuppressed()) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = subdoc->GetWindow();
  return GetRootForFocus(window, subdoc, true, true);
}

void
nsFocusManager::GetFocusInSelection(nsPIDOMWindowOuter* aWindow,
                                    nsIContent* aStartSelection,
                                    nsIContent* aEndSelection,
                                    nsIContent** aFocusedContent)
{
  *aFocusedContent = nullptr;

  nsCOMPtr<nsIContent> testContent = aStartSelection;
  nsCOMPtr<nsIContent> nextTestContent = aEndSelection;

  nsCOMPtr<nsIContent> currentFocus = aWindow->GetFocusedNode();

  // We now have the correct start node in selectionContent!
  // Search for focusable elements, starting with selectionContent

  // Method #1: Keep going up while we look - an ancestor might be focusable
  // We could end the loop earlier, such as when we're no longer
  // in the same frame, by comparing selectionContent->GetPrimaryFrame()
  // with a variable holding the starting selectionContent
  while (testContent) {
    // Keep testing while selectionContent is equal to something,
    // eventually we'll run out of ancestors

    nsCOMPtr<nsIURI> uri;
    if (testContent == currentFocus ||
        testContent->IsLink(getter_AddRefs(uri))) {
      testContent.forget(aFocusedContent);
      return;
    }

    // Get the parent
    testContent = testContent->GetParent();

    if (!testContent) {
      // We run this loop again, checking the ancestor chain of the selection's end point
      testContent = nextTestContent;
      nextTestContent = nullptr;
    }
  }

  // We couldn't find an anchor that was an ancestor of the selection start
  // Method #2: look for anchor in selection's primary range (depth first search)

  // Turn into nodes so that we can use GetNextSibling() and GetFirstChild()
  nsCOMPtr<nsIDOMNode> selectionNode(do_QueryInterface(aStartSelection));
  nsCOMPtr<nsIDOMNode> endSelectionNode(do_QueryInterface(aEndSelection));
  nsCOMPtr<nsIDOMNode> testNode;

  do {
    testContent = do_QueryInterface(selectionNode);

    // We're looking for any focusable link that could be part of the
    // main document's selection.
    nsCOMPtr<nsIURI> uri;
    if (testContent == currentFocus ||
        testContent->IsLink(getter_AddRefs(uri))) {
      testContent.forget(aFocusedContent);
      return;
    }

    selectionNode->GetFirstChild(getter_AddRefs(testNode));
    if (testNode) {
      selectionNode = testNode;
      continue;
    }

    if (selectionNode == endSelectionNode)
      break;
    selectionNode->GetNextSibling(getter_AddRefs(testNode));
    if (testNode) {
      selectionNode = testNode;
      continue;
    }

    do {
      selectionNode->GetParentNode(getter_AddRefs(testNode));
      if (!testNode || testNode == endSelectionNode) {
        selectionNode = nullptr;
        break;
      }
      testNode->GetNextSibling(getter_AddRefs(selectionNode));
      if (selectionNode)
        break;
      selectionNode = testNode;
    } while (true);
  }
  while (selectionNode && selectionNode != endSelectionNode);
}

class PointerUnlocker : public Runnable
{
public:
  PointerUnlocker()
  {
    MOZ_ASSERT(!PointerUnlocker::sActiveUnlocker);
    PointerUnlocker::sActiveUnlocker = this;
  }

  ~PointerUnlocker()
  {
    if (PointerUnlocker::sActiveUnlocker == this) {
      PointerUnlocker::sActiveUnlocker = nullptr;
    }
  }

  NS_IMETHOD Run()
  {
    if (PointerUnlocker::sActiveUnlocker == this) {
      PointerUnlocker::sActiveUnlocker = nullptr;
    }
    NS_ENSURE_STATE(nsFocusManager::GetFocusManager());
    nsPIDOMWindowOuter* focused =
      nsFocusManager::GetFocusManager()->GetFocusedWindow();
    nsCOMPtr<nsIDocument> pointerLockedDoc =
      do_QueryReferent(EventStateManager::sPointerLockedDoc);
    if (pointerLockedDoc &&
        !nsContentUtils::IsInPointerLockContext(focused)) {
      nsIDocument::UnlockPointer();
    }
    return NS_OK;
  }

  static PointerUnlocker* sActiveUnlocker;
};

PointerUnlocker*
PointerUnlocker::sActiveUnlocker = nullptr;

void
nsFocusManager::SetFocusedWindowInternal(nsPIDOMWindowOuter* aWindow)
{
  if (!PointerUnlocker::sActiveUnlocker &&
      nsContentUtils::IsInPointerLockContext(mFocusedWindow) &&
      !nsContentUtils::IsInPointerLockContext(aWindow)) {
    nsCOMPtr<nsIRunnable> runnable = new PointerUnlocker();
    NS_DispatchToCurrentThread(runnable);
  }
  mFocusedWindow = aWindow;
}

void
nsFocusManager::MarkUncollectableForCCGeneration(uint32_t aGeneration)
{
  if (!sInstance) {
    return;
  }

  if (sInstance->mActiveWindow) {
    sInstance->mActiveWindow->
      MarkUncollectableForCCGeneration(aGeneration);
  }
  if (sInstance->mFocusedWindow) {
    sInstance->mFocusedWindow->
      MarkUncollectableForCCGeneration(aGeneration);
  }
  if (sInstance->mWindowBeingLowered) {
    sInstance->mWindowBeingLowered->
      MarkUncollectableForCCGeneration(aGeneration);
  }
  if (sInstance->mFocusedContent) {
    sInstance->mFocusedContent->OwnerDoc()->
      MarkUncollectableForCCGeneration(aGeneration);
  }
  if (sInstance->mFirstBlurEvent) {
    sInstance->mFirstBlurEvent->OwnerDoc()->
      MarkUncollectableForCCGeneration(aGeneration);
  }
  if (sInstance->mFirstFocusEvent) {
    sInstance->mFirstFocusEvent->OwnerDoc()->
      MarkUncollectableForCCGeneration(aGeneration);
  }
  if (sInstance->mMouseButtonEventHandlingDocument) {
    sInstance->mMouseButtonEventHandlingDocument->
      MarkUncollectableForCCGeneration(aGeneration);
  }
}

nsresult
NS_NewFocusManager(nsIFocusManager** aResult)
{
  NS_IF_ADDREF(*aResult = nsFocusManager::GetFocusManager());
  return NS_OK;
}

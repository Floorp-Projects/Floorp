/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BrowserParent.h"

#include "nsFocusManager.h"

#include "ChildIterator.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsGkAtoms.h"
#include "nsGlobalWindow.h"
#include "nsContentUtils.h"
#include "ContentParent.h"
#include "nsPIDOMWindow.h"
#include "nsIContentInlines.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIFormControl.h"
#include "nsLayoutUtils.h"
#include "nsFrameTraversal.h"
#include "nsIWebNavigation.h"
#include "nsCaret.h"
#include "nsIBaseWindow.h"
#include "nsIAppWindow.h"
#include "nsTextControlFrame.h"
#include "nsViewManager.h"
#include "nsFrameSelection.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Selection.h"
#include "nsXULPopupManager.h"
#include "nsMenuPopupFrame.h"
#include "nsIScriptError.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIPrincipal.h"
#include "nsIObserverService.h"
#include "BrowserChild.h"
#include "nsFrameLoader.h"
#include "nsHTMLDocument.h"
#include "nsNetUtil.h"
#include "nsRange.h"
#include "nsFrameLoaderOwner.h"
#include "nsQueryObject.h"

#include "mozilla/AccessibleCaretEventHub.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ElementBinding.h"
#include "mozilla/dom/HTMLImageElement.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/HTMLSlotElement.h"
#include "mozilla/dom/BrowserBridgeChild.h"
#include "mozilla/dom/Text.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/EventStates.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/PointerLockManager.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/Services.h"
#include "mozilla/Unused.h"
#include "mozilla/StaticPrefs_full_screen_api.h"
#include <algorithm>

#ifdef MOZ_XUL
#  include "nsIDOMXULMenuListElement.h"
#endif

#ifdef ACCESSIBILITY
#  include "nsAccessibilityService.h"
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
#define LOGFOCUSNAVIGATION(args) \
  MOZ_LOG(gFocusNavigationLog, mozilla::LogLevel::Debug, args)

#define LOGTAG(log, format, content)                      \
  if (MOZ_LOG_TEST(log, LogLevel::Debug)) {               \
    nsAutoCString tag("(none)"_ns);                       \
    if (content) {                                        \
      content->NodeInfo()->NameAtom()->ToUTF8String(tag); \
    }                                                     \
    MOZ_LOG(log, LogLevel::Debug, (format, tag.get()));   \
  }

#define LOGCONTENT(format, content) LOGTAG(gFocusLog, format, content)
#define LOGCONTENTNAVIGATION(format, content) \
  LOGTAG(gFocusNavigationLog, format, content)

struct nsDelayedBlurOrFocusEvent {
  nsDelayedBlurOrFocusEvent(EventMessage aEventMessage, PresShell* aPresShell,
                            Document* aDocument, EventTarget* aTarget,
                            EventTarget* aRelatedTarget)
      : mPresShell(aPresShell),
        mDocument(aDocument),
        mTarget(aTarget),
        mEventMessage(aEventMessage),
        mRelatedTarget(aRelatedTarget) {}

  nsDelayedBlurOrFocusEvent(const nsDelayedBlurOrFocusEvent& aOther)
      : mPresShell(aOther.mPresShell),
        mDocument(aOther.mDocument),
        mTarget(aOther.mTarget),
        mEventMessage(aOther.mEventMessage) {}

  RefPtr<PresShell> mPresShell;
  nsCOMPtr<Document> mDocument;
  nsCOMPtr<EventTarget> mTarget;
  EventMessage mEventMessage;
  nsCOMPtr<EventTarget> mRelatedTarget;
};

inline void ImplCycleCollectionUnlink(nsDelayedBlurOrFocusEvent& aField) {
  aField.mPresShell = nullptr;
  aField.mDocument = nullptr;
  aField.mTarget = nullptr;
  aField.mRelatedTarget = nullptr;
}

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    nsDelayedBlurOrFocusEvent& aField, const char* aName, uint32_t aFlags = 0) {
  CycleCollectionNoteChild(
      aCallback, static_cast<nsIDocumentObserver*>(aField.mPresShell.get()),
      aName, aFlags);
  CycleCollectionNoteChild(aCallback, aField.mDocument.get(), aName, aFlags);
  CycleCollectionNoteChild(aCallback, aField.mTarget.get(), aName, aFlags);
  CycleCollectionNoteChild(aCallback, aField.mRelatedTarget.get(), aName,
                           aFlags);
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsFocusManager)
  NS_INTERFACE_MAP_ENTRY(nsIFocusManager)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIFocusManager)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsFocusManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsFocusManager)

NS_IMPL_CYCLE_COLLECTION_WEAK(nsFocusManager, mActiveWindow,
                              mActiveBrowsingContextInContent,
                              mActiveBrowsingContextInChrome, mFocusedWindow,
                              mFocusedBrowsingContextInContent,
                              mFocusedBrowsingContextInChrome, mFocusedElement,
                              mFirstBlurEvent, mFirstFocusEvent,
                              mWindowBeingLowered, mDelayedBlurFocusEvents)

StaticRefPtr<nsFocusManager> nsFocusManager::sInstance;
bool nsFocusManager::sTestMode = false;
uint64_t nsFocusManager::sFocusActionCounter = 0;

static const char* kObservedPrefs[] = {"accessibility.browsewithcaret",
                                       "accessibility.tabfocus_applies_to_xul",
                                       "focusmanager.testmode", nullptr};

nsFocusManager::nsFocusManager()
    : mActionIdForActiveBrowsingContextInContent(0),
      mActionIdForActiveBrowsingContextInChrome(0),
      mActionIdForFocusedBrowsingContextInContent(0),
      mActionIdForFocusedBrowsingContextInChrome(0),
      mActiveBrowsingContextInContentSetFromOtherProcess(false),
      mEventHandlingNeedsFlush(false) {}

nsFocusManager::~nsFocusManager() {
  Preferences::UnregisterCallbacks(nsFocusManager::PrefChanged, kObservedPrefs,
                                   this);

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, "xpcom-shutdown");
  }
}

// static
nsresult nsFocusManager::Init() {
  sInstance = new nsFocusManager();

  nsIContent::sTabFocusModelAppliesToXUL =
      Preferences::GetBool("accessibility.tabfocus_applies_to_xul",
                           nsIContent::sTabFocusModelAppliesToXUL);

  sTestMode = Preferences::GetBool("focusmanager.testmode", false);

  Preferences::RegisterCallbacks(nsFocusManager::PrefChanged, kObservedPrefs,
                                 sInstance.get());

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(sInstance, "xpcom-shutdown", true);
  }

  return NS_OK;
}

// static
void nsFocusManager::Shutdown() { sInstance = nullptr; }

// static
void nsFocusManager::PrefChanged(const char* aPref, void* aSelf) {
  static_cast<nsFocusManager*>(aSelf)->PrefChanged(aPref);
}

void nsFocusManager::PrefChanged(const char* aPref) {
  nsDependentCString pref(aPref);
  if (pref.EqualsLiteral("accessibility.browsewithcaret")) {
    UpdateCaretForCaretBrowsingMode();
  } else if (pref.EqualsLiteral("accessibility.tabfocus_applies_to_xul")) {
    nsIContent::sTabFocusModelAppliesToXUL =
        Preferences::GetBool("accessibility.tabfocus_applies_to_xul",
                             nsIContent::sTabFocusModelAppliesToXUL);
  } else if (pref.EqualsLiteral("focusmanager.testmode")) {
    sTestMode = Preferences::GetBool("focusmanager.testmode", false);
  }
}

NS_IMETHODIMP
nsFocusManager::Observe(nsISupports* aSubject, const char* aTopic,
                        const char16_t* aData) {
  if (!nsCRT::strcmp(aTopic, "xpcom-shutdown")) {
    mActiveWindow = nullptr;
    mActiveBrowsingContextInContent = nullptr;
    mActionIdForActiveBrowsingContextInContent = 0;
    mActionIdForFocusedBrowsingContextInContent = 0;
    mActiveBrowsingContextInChrome = nullptr;
    mActionIdForActiveBrowsingContextInChrome = 0;
    mActionIdForFocusedBrowsingContextInChrome = 0;
    mFocusedWindow = nullptr;
    mFocusedBrowsingContextInContent = nullptr;
    mFocusedBrowsingContextInChrome = nullptr;
    mFocusedElement = nullptr;
    mFirstBlurEvent = nullptr;
    mFirstFocusEvent = nullptr;
    mWindowBeingLowered = nullptr;
    mDelayedBlurFocusEvents.Clear();
  }

  return NS_OK;
}

static bool ActionIdComparableAndLower(uint64_t aActionId,
                                       uint64_t aReference) {
  MOZ_ASSERT(aActionId, "Uninitialized action id");
  auto [actionProc, actionId] =
      nsContentUtils::SplitProcessSpecificId(aActionId);
  auto [refProc, refId] = nsContentUtils::SplitProcessSpecificId(aReference);
  return actionProc == refProc && actionId < refId;
}

// given a frame content node, retrieve the nsIDOMWindow displayed in it
static nsPIDOMWindowOuter* GetContentWindow(nsIContent* aContent) {
  Document* doc = aContent->GetComposedDoc();
  if (doc) {
    Document* subdoc = doc->GetSubDocumentFor(aContent);
    if (subdoc) {
      return subdoc->GetWindow();
    }
  }

  return nullptr;
}

bool nsFocusManager::IsFocused(nsIContent* aContent) {
  if (!aContent || !mFocusedElement) {
    return false;
  }
  return aContent == mFocusedElement;
}

bool nsFocusManager::IsTestMode() { return sTestMode; }

bool nsFocusManager::IsInActiveWindow(BrowsingContext* aBC) const {
  RefPtr<BrowsingContext> top = aBC->Top();
  if (XRE_IsParentProcess()) {
    top = top->Canonical()->TopCrossChromeBoundary();
  }
  return IsSameOrAncestor(top, GetActiveBrowsingContext());
}

// get the current window for the given content node
static nsPIDOMWindowOuter* GetCurrentWindow(nsIContent* aContent) {
  Document* doc = aContent->GetComposedDoc();
  return doc ? doc->GetWindow() : nullptr;
}

// static
Element* nsFocusManager::GetFocusedDescendant(
    nsPIDOMWindowOuter* aWindow, SearchRange aSearchRange,
    nsPIDOMWindowOuter** aFocusedWindow) {
  NS_ENSURE_TRUE(aWindow, nullptr);

  *aFocusedWindow = nullptr;

  Element* currentElement = nullptr;
  nsPIDOMWindowOuter* window = aWindow;
  for (;;) {
    *aFocusedWindow = window;
    currentElement = window->GetFocusedElement();
    if (!currentElement || aSearchRange == eOnlyCurrentWindow) {
      break;
    }

    window = GetContentWindow(currentElement);
    if (!window) {
      break;
    }

    if (aSearchRange == eIncludeAllDescendants) {
      continue;
    }

    MOZ_ASSERT(aSearchRange == eIncludeVisibleDescendants);

    // If the child window doesn't have PresShell, it means the window is
    // invisible.
    nsIDocShell* docShell = window->GetDocShell();
    if (!docShell) {
      break;
    }
    if (!docShell->GetPresShell()) {
      break;
    }
  }

  NS_IF_ADDREF(*aFocusedWindow);

  return currentElement;
}

// static
Element* nsFocusManager::GetRedirectedFocus(nsIContent* aContent) {
#ifdef MOZ_XUL
  if (aContent->IsXULElement()) {
    nsCOMPtr<nsIDOMXULMenuListElement> menulist =
        aContent->AsElement()->AsXULMenuList();
    if (menulist) {
      RefPtr<Element> inputField;
      menulist->GetInputField(getter_AddRefs(inputField));
      return inputField;
    }
  }
#endif

  return nullptr;
}

// static
InputContextAction::Cause nsFocusManager::GetFocusMoveActionCause(
    uint32_t aFlags) {
  if (aFlags & nsIFocusManager::FLAG_BYTOUCH) {
    return InputContextAction::CAUSE_TOUCH;
  } else if (aFlags & nsIFocusManager::FLAG_BYMOUSE) {
    return InputContextAction::CAUSE_MOUSE;
  } else if (aFlags & nsIFocusManager::FLAG_BYKEY) {
    return InputContextAction::CAUSE_KEY;
  } else if (aFlags & nsIFocusManager::FLAG_BYLONGPRESS) {
    return InputContextAction::CAUSE_LONGPRESS;
  }
  return InputContextAction::CAUSE_UNKNOWN;
}

NS_IMETHODIMP
nsFocusManager::GetActiveWindow(mozIDOMWindowProxy** aWindow) {
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Must not be called outside the parent process.");
  NS_IF_ADDREF(*aWindow = mActiveWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::GetActiveBrowsingContext(BrowsingContext** aBrowsingContext) {
  NS_IF_ADDREF(*aBrowsingContext = GetActiveBrowsingContext());
  return NS_OK;
}

void nsFocusManager::FocusWindow(nsPIDOMWindowOuter* aWindow,
                                 CallerType aCallerType) {
  if (sInstance) {
    sInstance->SetFocusedWindowWithCallerType(
        aWindow, aCallerType, sInstance->GenerateFocusActionId());
  }
}

NS_IMETHODIMP
nsFocusManager::GetFocusedWindow(mozIDOMWindowProxy** aFocusedWindow) {
  NS_IF_ADDREF(*aFocusedWindow = mFocusedWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::GetFocusedContentBrowsingContext(
    BrowsingContext** aBrowsingContext) {
  MOZ_DIAGNOSTIC_ASSERT(
      XRE_IsParentProcess(),
      "We only have use cases for this in the parent process");
  NS_IF_ADDREF(*aBrowsingContext = GetFocusedBrowsingContextInChrome());
  return NS_OK;
}

nsresult nsFocusManager::SetFocusedWindowWithCallerType(
    mozIDOMWindowProxy* aWindowToFocus, CallerType aCallerType,
    uint64_t aActionId) {
  LOGFOCUS(("<<SetFocusedWindow begin actionid: %" PRIu64 ">>", aActionId));

  nsCOMPtr<nsPIDOMWindowOuter> windowToFocus =
      nsPIDOMWindowOuter::From(aWindowToFocus);
  NS_ENSURE_TRUE(windowToFocus, NS_ERROR_FAILURE);

  nsCOMPtr<Element> frameElement = windowToFocus->GetFrameElementInternal();
  if (frameElement) {
    // pass false for aFocusChanged so that the caret does not get updated
    // and scrolling does not occur.
    SetFocusInner(frameElement, 0, false, true, aActionId);
  } else {
    // this is a top-level window. If the window has a child frame focused,
    // clear the focus. Otherwise, focus should already be in this frame, or
    // already cleared. This ensures that focus will be in this frame and not
    // in a child.
    nsIContent* content = windowToFocus->GetFocusedElement();
    if (content) {
      if (nsCOMPtr<nsPIDOMWindowOuter> childWindow = GetContentWindow(content))
        ClearFocus(windowToFocus);
    }
  }

  nsCOMPtr<nsPIDOMWindowOuter> rootWindow = windowToFocus->GetPrivateRoot();
  if (rootWindow) {
    RaiseWindow(rootWindow, aCallerType, aActionId);
  }

  LOGFOCUS(("<<SetFocusedWindow end actionid: %" PRIu64 ">>", aActionId));

  return NS_OK;
}

NS_IMETHODIMP nsFocusManager::SetFocusedWindow(
    mozIDOMWindowProxy* aWindowToFocus) {
  return SetFocusedWindowWithCallerType(aWindowToFocus, CallerType::System,
                                        GenerateFocusActionId());
}

NS_IMETHODIMP
nsFocusManager::GetFocusedElement(Element** aFocusedElement) {
  RefPtr<Element> focusedElement = mFocusedElement;
  focusedElement.forget(aFocusedElement);
  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::GetLastFocusMethod(mozIDOMWindowProxy* aWindow,
                                   uint32_t* aLastFocusMethod) {
  // the focus method is stored on the inner window
  nsCOMPtr<nsPIDOMWindowOuter> window;
  if (aWindow) {
    window = nsPIDOMWindowOuter::From(aWindow);
  }
  if (!window) {
    window = mFocusedWindow;
  }

  *aLastFocusMethod = window ? window->GetFocusMethod() : 0;

  NS_ASSERTION((*aLastFocusMethod & METHOD_MASK) == *aLastFocusMethod,
               "invalid focus method");
  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::SetFocus(Element* aElement, uint32_t aFlags) {
  LOGFOCUS(("<<SetFocus begin>>"));

  NS_ENSURE_ARG(aElement);

  SetFocusInner(aElement, aFlags, true, true, GenerateFocusActionId());

  LOGFOCUS(("<<SetFocus end>>"));

  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::ElementIsFocusable(Element* aElement, uint32_t aFlags,
                                   bool* aIsFocusable) {
  NS_ENSURE_TRUE(aElement, NS_ERROR_INVALID_ARG);

  *aIsFocusable = FlushAndCheckIfFocusable(aElement, aFlags) != nullptr;

  return NS_OK;
}

MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHODIMP
nsFocusManager::MoveFocus(mozIDOMWindowProxy* aWindow, Element* aStartElement,
                          uint32_t aType, uint32_t aFlags, Element** aElement) {
  *aElement = nullptr;

  LOGFOCUS(("<<MoveFocus begin Type: %d Flags: %x>>", aType, aFlags));

  if (MOZ_LOG_TEST(gFocusLog, LogLevel::Debug) && mFocusedWindow) {
    Document* doc = mFocusedWindow->GetExtantDoc();
    if (doc && doc->GetDocumentURI()) {
      LOGFOCUS((" Focused Window: %p %s", mFocusedWindow.get(),
                doc->GetDocumentURI()->GetSpecOrDefault().get()));
    }
  }

  LOGCONTENT("  Current Focus: %s", mFocusedElement.get());

  // use FLAG_BYMOVEFOCUS when switching focus with MoveFocus unless one of
  // the other focus methods is already set, or we're just moving to the root
  // or caret position.
  if (aType != MOVEFOCUS_ROOT && aType != MOVEFOCUS_CARET &&
      (aFlags & METHOD_MASK) == 0) {
    aFlags |= FLAG_BYMOVEFOCUS;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window;
  if (aStartElement) {
    window = GetCurrentWindow(aStartElement);
  } else {
    window = aWindow ? nsPIDOMWindowOuter::From(aWindow) : mFocusedWindow.get();
  }

  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  bool noParentTraversal = aFlags & FLAG_NOPARENTFRAME;
  nsCOMPtr<nsIContent> newFocus;
  nsresult rv = DetermineElementToMoveFocus(window, aStartElement, aType,
                                            noParentTraversal, true,
                                            getter_AddRefs(newFocus));
  if (rv == NS_SUCCESS_DOM_NO_OPERATION) {
    return NS_OK;
  }

  NS_ENSURE_SUCCESS(rv, rv);

  LOGCONTENTNAVIGATION("Element to be focused: %s", newFocus.get());

  if (newFocus && newFocus->IsElement()) {
    // for caret movement, pass false for the aFocusChanged argument,
    // otherwise the caret will end up moving to the focus position. This
    // would be a problem because the caret would move to the beginning of the
    // focused link making it impossible to navigate the caret over a link.
    SetFocusInner(MOZ_KnownLive(newFocus->AsElement()), aFlags,
                  aType != MOVEFOCUS_CARET, true, GenerateFocusActionId());
    *aElement = do_AddRef(newFocus->AsElement()).take();
  } else if (aType == MOVEFOCUS_ROOT || aType == MOVEFOCUS_CARET) {
    // no content was found, so clear the focus for these two types.
    ClearFocus(window);
  }

  LOGFOCUS(("<<MoveFocus end>>"));

  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::ClearFocus(mozIDOMWindowProxy* aWindow) {
  LOGFOCUS(("<<ClearFocus begin>>"));

  // if the window to clear is the focused window or an ancestor of the
  // focused window, then blur the existing focused content. Otherwise, the
  // focus is somewhere else so just update the current node.
  NS_ENSURE_TRUE(aWindow, NS_ERROR_INVALID_ARG);
  nsCOMPtr<nsPIDOMWindowOuter> window = nsPIDOMWindowOuter::From(aWindow);

  if (IsSameOrAncestor(window, GetFocusedBrowsingContext())) {
    BrowsingContext* bc = window->GetBrowsingContext();
    bool isAncestor = (GetFocusedBrowsingContext() != bc);
    uint64_t actionId = GenerateFocusActionId();
    if (Blur(bc, nullptr, isAncestor, true, actionId)) {
      // if we are clearing the focus on an ancestor of the focused window,
      // the ancestor will become the new focused window, so focus it
      if (isAncestor) {
        Focus(window, nullptr, 0, true, false, false, true, actionId);
      }
    }
  } else {
    window->SetFocusedElement(nullptr);
  }

  LOGFOCUS(("<<ClearFocus end>>"));

  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::GetFocusedElementForWindow(mozIDOMWindowProxy* aWindow,
                                           bool aDeep,
                                           mozIDOMWindowProxy** aFocusedWindow,
                                           Element** aElement) {
  *aElement = nullptr;
  if (aFocusedWindow) {
    *aFocusedWindow = nullptr;
  }

  NS_ENSURE_TRUE(aWindow, NS_ERROR_INVALID_ARG);
  nsCOMPtr<nsPIDOMWindowOuter> window = nsPIDOMWindowOuter::From(aWindow);

  nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
  RefPtr<Element> focusedElement =
      GetFocusedDescendant(window,
                           aDeep ? nsFocusManager::eIncludeAllDescendants
                                 : nsFocusManager::eOnlyCurrentWindow,
                           getter_AddRefs(focusedWindow));

  focusedElement.forget(aElement);

  if (aFocusedWindow) {
    NS_IF_ADDREF(*aFocusedWindow = focusedWindow);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::MoveCaretToFocus(mozIDOMWindowProxy* aWindow) {
  nsCOMPtr<nsIWebNavigation> webnav = do_GetInterface(aWindow);
  nsCOMPtr<nsIDocShellTreeItem> dsti = do_QueryInterface(webnav);
  if (dsti) {
    if (dsti->ItemType() != nsIDocShellTreeItem::typeChrome) {
      nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(dsti);
      NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

      // don't move the caret for editable documents
      bool isEditable;
      docShell->GetEditable(&isEditable);
      if (isEditable) {
        return NS_OK;
      }

      RefPtr<PresShell> presShell = docShell->GetPresShell();
      NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

      nsCOMPtr<nsPIDOMWindowOuter> window = nsPIDOMWindowOuter::From(aWindow);
      nsCOMPtr<nsIContent> content = window->GetFocusedElement();
      if (content) {
        MoveCaretToFocus(presShell, content);
      }
    }
  }

  return NS_OK;
}

void nsFocusManager::WindowRaised(mozIDOMWindowProxy* aWindow,
                                  uint64_t aActionId) {
  if (!aWindow) {
    return;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = nsPIDOMWindowOuter::From(aWindow);
  BrowsingContext* bc = window->GetBrowsingContext();

  if (MOZ_LOG_TEST(gFocusLog, LogLevel::Debug)) {
    LOGFOCUS(("Window %p Raised [Currently: %p %p] actionid: %" PRIu64, aWindow,
              mActiveWindow.get(), mFocusedWindow.get(), aActionId));
    Document* doc = window->GetExtantDoc();
    if (doc && doc->GetDocumentURI()) {
      LOGFOCUS(("  Raised Window: %p %s", aWindow,
                doc->GetDocumentURI()->GetSpecOrDefault().get()));
    }
    if (mActiveWindow) {
      doc = mActiveWindow->GetExtantDoc();
      if (doc && doc->GetDocumentURI()) {
        LOGFOCUS(("  Active Window: %p %s", mActiveWindow.get(),
                  doc->GetDocumentURI()->GetSpecOrDefault().get()));
      }
    }
  }

  if (XRE_IsParentProcess()) {
    if (mActiveWindow == window) {
      // The window is already active, so there is no need to focus anything,
      // but make sure that the right widget is focused. This is a special case
      // for Windows because when restoring a minimized window, a second
      // activation will occur and the top-level widget could be focused instead
      // of the child we want. We solve this by calling SetFocus to ensure that
      // what the focus manager thinks should be the current widget is actually
      // focused.
      EnsureCurrentWidgetFocused(CallerType::System);
      return;
    }

    // lower the existing window, if any. This shouldn't happen usually.
    if (mActiveWindow) {
      WindowLowered(mActiveWindow, aActionId);
    }
  } else if (bc->IsTop()) {
    BrowsingContext* active = GetActiveBrowsingContext();
    if (active == bc && !mActiveBrowsingContextInContentSetFromOtherProcess) {
      // EnsureCurrentWidgetFocused() should not be necessary with
      // PuppetWidget.
      return;
    }

    if (active && active != bc) {
      if (active->IsInProcess()) {
        WindowLowered(active->GetDOMWindow(), aActionId);
      }
      // No else, because trying to lower other-process windows
      // from here can result in the BrowsingContext no longer
      // existing in the parent process by the time it deserializes
      // the IPC message.
    }
  }

  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem = window->GetDocShell();
  // If there's no docShellAsItem, this window must have been closed,
  // in that case there is no tree owner.
  if (!docShellAsItem) {
    return;
  }

  // set this as the active window
  if (XRE_IsParentProcess()) {
    mActiveWindow = window;
  } else if (bc->IsTop()) {
    SetActiveBrowsingContextInContent(bc, aActionId);
  }

  // ensure that the window is enabled and visible
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  docShellAsItem->GetTreeOwner(getter_AddRefs(treeOwner));
  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(treeOwner);
  if (baseWindow) {
    bool isEnabled = true;
    if (NS_SUCCEEDED(baseWindow->GetEnabled(&isEnabled)) && !isEnabled) {
      return;
    }

    baseWindow->SetVisibility(true);
  }

  if (XRE_IsParentProcess()) {
    // Unsetting top-level focus upon lowering was inhibited to accommodate
    // ATOK, so we need to do it here.
    BrowserParent::UnsetTopLevelWebFocusAll();
    ActivateOrDeactivate(window, true);
  }

  // retrieve the last focused element within the window that was raised
  nsCOMPtr<nsPIDOMWindowOuter> currentWindow;
  RefPtr<Element> currentFocus = GetFocusedDescendant(
      window, eIncludeAllDescendants, getter_AddRefs(currentWindow));

  NS_ASSERTION(currentWindow, "window raised with no window current");
  if (!currentWindow) {
    return;
  }

  nsCOMPtr<nsIAppWindow> appWin(do_GetInterface(baseWindow));
  // We use mFocusedWindow here is basically for the case that iframe navigate
  // from a.com to b.com for example, so it ends up being loaded in a different
  // process after Fission, but
  // currentWindow->GetBrowsingContext() == GetFocusedBrowsingContext() would
  // still be true because focused browsing context is synced, and we won't
  // fire a focus event while focusing if we use it as condition.
  Focus(currentWindow, currentFocus, 0, currentWindow != mFocusedWindow, false,
        appWin != nullptr, true, aActionId);
}

void nsFocusManager::WindowLowered(mozIDOMWindowProxy* aWindow,
                                   uint64_t aActionId) {
  if (!aWindow) {
    return;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = nsPIDOMWindowOuter::From(aWindow);

  if (MOZ_LOG_TEST(gFocusLog, LogLevel::Debug)) {
    LOGFOCUS(("Window %p Lowered [Currently: %p %p]", aWindow,
              mActiveWindow.get(), mFocusedWindow.get()));
    Document* doc = window->GetExtantDoc();
    if (doc && doc->GetDocumentURI()) {
      LOGFOCUS(("  Lowered Window: %s",
                doc->GetDocumentURI()->GetSpecOrDefault().get()));
    }
    if (mActiveWindow) {
      doc = mActiveWindow->GetExtantDoc();
      if (doc && doc->GetDocumentURI()) {
        LOGFOCUS(("  Active Window: %s",
                  doc->GetDocumentURI()->GetSpecOrDefault().get()));
      }
    }
  }

  if (XRE_IsParentProcess()) {
    if (mActiveWindow != window) {
      return;
    }
  } else {
    BrowsingContext* bc = window->GetBrowsingContext();
    BrowsingContext* active = GetActiveBrowsingContext();
    if (active != bc->Top()) {
      return;
    }
  }

  // clear the mouse capture as the active window has changed
  PresShell::ReleaseCapturingContent();

  // In addition, reset the drag state to ensure that we are no longer in
  // drag-select mode.
  if (mFocusedWindow) {
    nsCOMPtr<nsIDocShell> docShell = mFocusedWindow->GetDocShell();
    if (docShell) {
      if (PresShell* presShell = docShell->GetPresShell()) {
        RefPtr<nsFrameSelection> frameSelection = presShell->FrameSelection();
        frameSelection->SetDragState(false);
      }
    }
  }

  if (XRE_IsParentProcess()) {
    ActivateOrDeactivate(window, false);
  }

  // keep track of the window being lowered, so that attempts to raise the
  // window can be prevented until we return. Otherwise, focus can get into
  // an unusual state.
  mWindowBeingLowered = window;
  if (XRE_IsParentProcess()) {
    mActiveWindow = nullptr;
  } else {
    BrowsingContext* bc = window->GetBrowsingContext();
    if (bc == bc->Top()) {
      SetActiveBrowsingContextInContent(nullptr, aActionId);
    }
  }

  if (mFocusedWindow) {
    Blur(nullptr, nullptr, true, true, aActionId);
  }

  mWindowBeingLowered = nullptr;
}

nsresult nsFocusManager::ContentRemoved(Document* aDocument,
                                        nsIContent* aContent) {
  NS_ENSURE_ARG(aDocument);
  NS_ENSURE_ARG(aContent);

  nsPIDOMWindowOuter* window = aDocument->GetWindow();
  if (!window) {
    return NS_OK;
  }

  // if the content is currently focused in the window, or is an
  // shadow-including inclusive ancestor of the currently focused element,
  // reset the focus within that window.
  Element* content = window->GetFocusedElement();
  if (content &&
      nsContentUtils::ContentIsHostIncludingDescendantOf(content, aContent)) {
    window->SetFocusedElement(nullptr);

    // if this window is currently focused, clear the global focused
    // element as well, but don't fire any events.
    if (window->GetBrowsingContext() == GetFocusedBrowsingContext()) {
      mFocusedElement = nullptr;
    } else {
      // Check if the node that was focused is an iframe or similar by looking
      // if it has a subdocument. This would indicate that this focused iframe
      // and its descendants will be going away. We will need to move the
      // focus somewhere else, so just clear the focus in the toplevel window
      // so that no element is focused.
      //
      // The Fission case is handled in FlushAndCheckIfFocusable().
      Document* subdoc = aDocument->GetSubDocumentFor(content);
      if (subdoc) {
        nsCOMPtr<nsIDocShell> docShell = subdoc->GetDocShell();
        if (docShell) {
          nsCOMPtr<nsPIDOMWindowOuter> childWindow = docShell->GetWindow();
          if (childWindow &&
              IsSameOrAncestor(childWindow, GetFocusedBrowsingContext())) {
            if (XRE_IsParentProcess()) {
              ClearFocus(mActiveWindow);
            } else {
              BrowsingContext* active = GetActiveBrowsingContext();
              if (active) {
                if (active->IsInProcess()) {
                  ClearFocus(active->GetDOMWindow());
                } else {
                  mozilla::dom::ContentChild* contentChild =
                      mozilla::dom::ContentChild::GetSingleton();
                  MOZ_ASSERT(contentChild);
                  contentChild->SendClearFocus(active);
                }
              }  // no else, because ClearFocus does nothing with nullptr
            }
          }
        }
      }
    }

    // Notify the editor in case we removed its ancestor limiter.
    if (content->IsEditable()) {
      nsCOMPtr<nsIDocShell> docShell = aDocument->GetDocShell();
      if (docShell) {
        RefPtr<HTMLEditor> htmlEditor = docShell->GetHTMLEditor();
        if (htmlEditor) {
          RefPtr<Selection> selection = htmlEditor->GetSelection();
          if (selection && selection->GetFrameSelection() &&
              content == selection->GetFrameSelection()->GetAncestorLimiter()) {
            htmlEditor->FinalizeSelection();
          }
        }
      }
    }

    NotifyFocusStateChange(content, nullptr, 0, /* aGettingFocus = */ false,
                           false);
  }

  return NS_OK;
}

void nsFocusManager::WindowShown(mozIDOMWindowProxy* aWindow,
                                 bool aNeedsFocus) {
  if (!aWindow) {
    return;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = nsPIDOMWindowOuter::From(aWindow);

  if (MOZ_LOG_TEST(gFocusLog, LogLevel::Debug)) {
    LOGFOCUS(("Window %p Shown [Currently: %p %p]", window.get(),
              mActiveWindow.get(), mFocusedWindow.get()));
    Document* doc = window->GetExtantDoc();
    if (doc && doc->GetDocumentURI()) {
      LOGFOCUS(("Shown Window: %s",
                doc->GetDocumentURI()->GetSpecOrDefault().get()));
    }

    if (mFocusedWindow) {
      doc = mFocusedWindow->GetExtantDoc();
      if (doc && doc->GetDocumentURI()) {
        LOGFOCUS((" Focused Window: %s",
                  doc->GetDocumentURI()->GetSpecOrDefault().get()));
      }
    }
  }

  if (XRE_IsParentProcess()) {
    if (BrowsingContext* bc = window->GetBrowsingContext()) {
      if (bc->IsTop()) {
        bc->SetIsActiveBrowserWindow(bc->GetIsActiveBrowserWindow());
      }
    }
  }

  if (XRE_IsParentProcess()) {
    if (mFocusedWindow != window) {
      return;
    }
  } else {
    BrowsingContext* bc = window->GetBrowsingContext();
    if (!bc || mFocusedBrowsingContextInContent != bc) {
      return;
    }
    // Sync the window for a newly-created OOP iframe
    // Set actionId to zero to signify that it should be ignored.
    SetFocusedWindowInternal(window, 0, false);
  }

  if (aNeedsFocus) {
    nsCOMPtr<nsPIDOMWindowOuter> currentWindow;
    RefPtr<Element> currentFocus = GetFocusedDescendant(
        window, eIncludeAllDescendants, getter_AddRefs(currentWindow));

    if (currentWindow) {
      Focus(currentWindow, currentFocus, 0, true, false, false, true,
            GenerateFocusActionId());
    }
  } else {
    // Sometimes, an element in a window can be focused before the window is
    // visible, which would mean that the widget may not be properly focused.
    // When the window becomes visible, make sure the right widget is focused.
    EnsureCurrentWidgetFocused(CallerType::System);
  }
}

void nsFocusManager::WindowHidden(mozIDOMWindowProxy* aWindow,
                                  uint64_t aActionId) {
  // if there is no window or it is not the same or an ancestor of the
  // currently focused window, just return, as the current focus will not
  // be affected.

  if (!aWindow) {
    return;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = nsPIDOMWindowOuter::From(aWindow);

  if (MOZ_LOG_TEST(gFocusLog, LogLevel::Debug)) {
    LOGFOCUS(("Window %p Hidden [Currently: %p %p] actionid: %" PRIu64,
              window.get(), mActiveWindow.get(), mFocusedWindow.get(),
              aActionId));
    nsAutoCString spec;
    Document* doc = window->GetExtantDoc();
    if (doc && doc->GetDocumentURI()) {
      LOGFOCUS(("  Hide Window: %s",
                doc->GetDocumentURI()->GetSpecOrDefault().get()));
    }

    if (mFocusedWindow) {
      doc = mFocusedWindow->GetExtantDoc();
      if (doc && doc->GetDocumentURI()) {
        LOGFOCUS(("  Focused Window: %s",
                  doc->GetDocumentURI()->GetSpecOrDefault().get()));
      }
    }

    if (mActiveWindow) {
      doc = mActiveWindow->GetExtantDoc();
      if (doc && doc->GetDocumentURI()) {
        LOGFOCUS(("  Active Window: %s",
                  doc->GetDocumentURI()->GetSpecOrDefault().get()));
      }
    }
  }

  if (!IsSameOrAncestor(window, mFocusedWindow)) {
    return;
  }

  // at this point, we know that the window being hidden is either the focused
  // window, or an ancestor of the focused window. Either way, the focus is no
  // longer valid, so it needs to be updated.

  RefPtr<Element> oldFocusedElement = std::move(mFocusedElement);

  nsCOMPtr<nsIDocShell> focusedDocShell = mFocusedWindow->GetDocShell();
  if (!focusedDocShell) {
    return;
  }

  RefPtr<PresShell> presShell = focusedDocShell->GetPresShell();

  if (oldFocusedElement && oldFocusedElement->IsInComposedDoc()) {
    NotifyFocusStateChange(oldFocusedElement, nullptr, 0, false, false);
    window->UpdateCommands(u"focus"_ns, nullptr, 0);

    if (presShell) {
      SendFocusOrBlurEvent(eBlur, presShell,
                           oldFocusedElement->GetComposedDoc(),
                           oldFocusedElement, false);
    }
  }

  nsPresContext* focusedPresContext =
      presShell ? presShell->GetPresContext() : nullptr;
  IMEStateManager::OnChangeFocus(focusedPresContext, nullptr,
                                 GetFocusMoveActionCause(0));
  if (presShell) {
    SetCaretVisible(presShell, false, nullptr);
  }

  // If a window is being "hidden" because its BrowsingContext is changing
  // remoteness, we don't want to handle docshell destruction by moving focus.
  // Instead, the focused browsing context should stay the way it is (so that
  // the newly "shown" window in the other process knows to take focus) and
  // we should just null out the process-local field.
  nsCOMPtr<nsIDocShell> docShellBeingHidden = window->GetDocShell();
  // Check if we're currently hiding a non-remote nsDocShell due to its
  // BrowsingContext navigating to become remote. Normally, when a focused
  // subframe is hidden, focus is moved to the frame element, but focus should
  // stay with the BrowsingContext when performing a process switch. We don't
  // need to consider process switches where the hiding docshell is already
  // remote (ie. GetEmbedderElement is nullptr), as shifting remoteness to the
  // frame element is handled elsewhere.
  if (nsDocShell::Cast(docShellBeingHidden)->WillChangeProcess() &&
      docShellBeingHidden->GetBrowsingContext()->GetEmbedderElement()) {
    if (mFocusedWindow != window) {
      // The window being hidden is an ancestor of the focused window.
#ifdef DEBUG
      BrowsingContext* ancestor = window->GetBrowsingContext();
      BrowsingContext* bc = mFocusedWindow->GetBrowsingContext();
      for (;;) {
        if (!bc) {
          MOZ_ASSERT(false, "Should have found ancestor");
        }
        bc = bc->GetParent();
        if (ancestor == bc) {
          break;
        }
      }
#endif
      // This call adjusts the focused browsing context and window.
      // The latter gets nulled out immediately below.
      SetFocusedWindowInternal(window, aActionId);
    }
    mFocusedWindow = nullptr;
    window->SetFocusedElement(nullptr);
    return;
  }

  // if the docshell being hidden is being destroyed, then we want to move
  // focus somewhere else. Call ClearFocus on the toplevel window, which
  // will have the effect of clearing the focus and moving the focused window
  // to the toplevel window. But if the window isn't being destroyed, we are
  // likely just loading a new document in it, so we want to maintain the
  // focused window so that the new document gets properly focused.
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

    if (XRE_IsParentProcess()) {
      if (mActiveWindow == mFocusedWindow || mActiveWindow == window) {
        WindowLowered(mActiveWindow, aActionId);
      } else {
        ClearFocus(mActiveWindow);
      }
    } else {
      BrowsingContext* active = GetActiveBrowsingContext();
      if (active) {
        nsPIDOMWindowOuter* activeWindow = active->GetDOMWindow();
        if (activeWindow) {
          if ((mFocusedWindow &&
               mFocusedWindow->GetBrowsingContext() == active) ||
              (window->GetBrowsingContext() == active)) {
            WindowLowered(activeWindow, aActionId);
          } else {
            ClearFocus(activeWindow);
          }
        }  // else do nothing when an out-of-process iframe is torn down
      }
    }
    return;
  }

  if (!XRE_IsParentProcess() &&
      mActiveBrowsingContextInContent ==
          docShellBeingHidden->GetBrowsingContext() &&
      mActiveBrowsingContextInContent->GetIsInBFCache()) {
    SetActiveBrowsingContextInContent(nullptr, aActionId);
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
      dsti->GetInProcessParent(getter_AddRefs(parentDsti));
      if (parentDsti) {
        if (nsCOMPtr<nsPIDOMWindowOuter> parentWindow =
                parentDsti->GetWindow()) {
          parentWindow->SetFocusedElement(nullptr);
        }
      }
    }

    SetFocusedWindowInternal(window, aActionId);
  }
}

void nsFocusManager::FireDelayedEvents(Document* aDocument) {
  MOZ_ASSERT(aDocument);

  // fire any delayed focus and blur events in the same order that they were
  // added
  for (uint32_t i = 0; i < mDelayedBlurFocusEvents.Length(); i++) {
    if (mDelayedBlurFocusEvents[i].mDocument == aDocument) {
      if (!aDocument->GetInnerWindow() ||
          !aDocument->GetInnerWindow()->IsCurrentInnerWindow()) {
        // If the document was navigated away from or is defunct, don't bother
        // firing events on it. Note the symmetry between this condition and
        // the similar one in Document.cpp:FireOrClearDelayedEvents.
        mDelayedBlurFocusEvents.RemoveElementAt(i);
        --i;
      } else if (!aDocument->EventHandlingSuppressed()) {
        EventMessage message = mDelayedBlurFocusEvents[i].mEventMessage;
        nsCOMPtr<EventTarget> target = mDelayedBlurFocusEvents[i].mTarget;
        RefPtr<PresShell> presShell = mDelayedBlurFocusEvents[i].mPresShell;
        nsCOMPtr<EventTarget> relatedTarget =
            mDelayedBlurFocusEvents[i].mRelatedTarget;
        mDelayedBlurFocusEvents.RemoveElementAt(i);

        FireFocusOrBlurEvent(message, presShell, target, false, false,
                             relatedTarget);
        --i;
      }
    }
  }
}

void nsFocusManager::WasNuked(nsPIDOMWindowOuter* aWindow) {
  MOZ_ASSERT(aWindow, "Expected non-null window.");
  MOZ_ASSERT(aWindow != mActiveWindow,
             "How come we're nuking a window that's still active?");
  if (aWindow == mFocusedWindow) {
    mFocusedWindow = nullptr;
    SetFocusedBrowsingContext(nullptr, GenerateFocusActionId());
    mFocusedElement = nullptr;
  }
}

nsresult nsFocusManager::FocusPlugin(Element* aPlugin) {
  NS_ENSURE_ARG(aPlugin);
  SetFocusInner(aPlugin, 0, true, false, GenerateFocusActionId());
  return NS_OK;
}

nsFocusManager::BlurredElementInfo::BlurredElementInfo(Element& aElement)
    : mElement(aElement) {}

nsFocusManager::BlurredElementInfo::~BlurredElementInfo() = default;

// https://drafts.csswg.org/selectors-4/#the-focus-visible-pseudo
static bool ShouldMatchFocusVisible(nsPIDOMWindowOuter* aWindow,
                                    const Element& aElement,
                                    int32_t aFocusFlags) {
  // If we were explicitly requested to show the ring, do it.
  if (aFocusFlags & nsIFocusManager::FLAG_SHOWRING) {
    return true;
  }

  if (aWindow->ShouldShowFocusRing()) {
    // The window decision also trumps any other heuristic.
    return true;
  }

  // Any element which supports keyboard input (such as an input element, or any
  // other element which may trigger a virtual keyboard to be shown on focus if
  // a physical keyboard is not present) should always match :focus-visible when
  // focused.
  {
    if (aElement.IsHTMLElement(nsGkAtoms::textarea) || aElement.IsEditable()) {
      return true;
    }

    if (auto* input = HTMLInputElement::FromNode(aElement)) {
      if (input->IsSingleLineTextControl()) {
        return true;
      }
    }
  }

  if (aFocusFlags & nsIFocusManager::FLAG_NOSHOWRING) {
    return false;
  }

  switch (nsFocusManager::GetFocusMoveActionCause(aFocusFlags)) {
    case InputContextAction::CAUSE_KEY:
      // If the user interacts with the page via the keyboard, the currently
      // focused element should match :focus-visible (i.e. keyboard usage may
      // change whether this pseudo-class matches even if it doesn't affect
      // :focus).
      return true;
    case InputContextAction::CAUSE_UNKNOWN:
      // We render outlines if the last "known" focus method was by key or there
      // was no previous known focus method, otherwise we don't.
      return aWindow->UnknownFocusMethodShouldShowOutline();
    case InputContextAction::CAUSE_MOUSE:
    case InputContextAction::CAUSE_TOUCH:
    case InputContextAction::CAUSE_LONGPRESS:
      // If the user interacts with the page via a pointing device, such that
      // the focus is moved to a new element which does not support user input,
      // the newly focused element should not match :focus-visible.
      return false;
    case InputContextAction::CAUSE_UNKNOWN_CHROME:
    case InputContextAction::CAUSE_UNKNOWN_DURING_KEYBOARD_INPUT:
    case InputContextAction::CAUSE_UNKNOWN_DURING_NON_KEYBOARD_INPUT:
      // TODO(emilio): We could return some of these though, looking at
      // UserActivation. We may want to suppress focus rings for unknown /
      // programatic focus if the user is interacting with the page but not
      // during keyboard input, or such.
      MOZ_ASSERT_UNREACHABLE(
          "These don't get returned by GetFocusMoveActionCause");
      break;
  }
  return false;
}

/* static */
void nsFocusManager::NotifyFocusStateChange(Element* aElement,
                                            Element* aElementToFocus,
                                            int32_t aFlags, bool aGettingFocus,
                                            bool aShouldShowFocusRing) {
  MOZ_ASSERT_IF(aElementToFocus, !aGettingFocus);
  nsIContent* commonAncestor = nullptr;
  if (aElementToFocus) {
    commonAncestor = nsContentUtils::GetCommonFlattenedTreeAncestor(
        aElement, aElementToFocus);
  }

  if (aGettingFocus) {
    EventStates eventStateToAdd = NS_EVENT_STATE_FOCUS;
    if (aShouldShowFocusRing) {
      eventStateToAdd |= NS_EVENT_STATE_FOCUSRING;
    }
    aElement->AddStates(eventStateToAdd);

    for (nsIContent* host = aElement->GetContainingShadowHost(); host;
         host = host->GetContainingShadowHost()) {
      host->AsElement()->AddStates(NS_EVENT_STATE_FOCUS);
    }
  } else {
    EventStates eventStateToRemove =
        NS_EVENT_STATE_FOCUS | NS_EVENT_STATE_FOCUSRING;
    aElement->RemoveStates(eventStateToRemove);

    for (nsIContent* host = aElement->GetContainingShadowHost(); host;
         host = host->GetContainingShadowHost()) {
      host->AsElement()->RemoveStates(NS_EVENT_STATE_FOCUS);
    }
  }

  for (nsIContent* content = aElement; content && content != commonAncestor;
       content = content->GetFlattenedTreeParent()) {
    Element* element = Element::FromNode(content);
    if (!element) {
      continue;
    }

    if (aGettingFocus) {
      if (element->State().HasState(NS_EVENT_STATE_FOCUS_WITHIN)) {
        break;
      }
      element->AddStates(NS_EVENT_STATE_FOCUS_WITHIN);
    } else {
      element->RemoveStates(NS_EVENT_STATE_FOCUS_WITHIN);
    }
  }
}

// static
void nsFocusManager::EnsureCurrentWidgetFocused(CallerType aCallerType) {
  if (!mFocusedWindow || sTestMode) return;

  // get the main child widget for the focused window and ensure that the
  // platform knows that this widget is focused.
  nsCOMPtr<nsIDocShell> docShell = mFocusedWindow->GetDocShell();
  if (!docShell) {
    return;
  }
  RefPtr<PresShell> presShell = docShell->GetPresShell();
  if (!presShell) {
    return;
  }
  nsViewManager* vm = presShell->GetViewManager();
  if (!vm) {
    return;
  }
  nsCOMPtr<nsIWidget> widget = vm->GetRootWidget();
  if (!widget) {
    return;
  }
  widget->SetFocus(nsIWidget::Raise::No, aCallerType);
}

void nsFocusManager::ActivateOrDeactivate(nsPIDOMWindowOuter* aWindow,
                                          bool aActive) {
  MOZ_ASSERT(XRE_IsParentProcess());
  if (!aWindow) {
    return;
  }

  if (BrowsingContext* bc = aWindow->GetBrowsingContext()) {
    MOZ_ASSERT(bc->IsTop());

    RefPtr<CanonicalBrowsingContext> chromeTop =
        bc->Canonical()->TopCrossChromeBoundary();
    MOZ_ASSERT(bc == chromeTop);

    chromeTop->SetIsActiveBrowserWindow(aActive);
    chromeTop->CallOnAllTopDescendants(
        [aActive](CanonicalBrowsingContext* aBrowsingContext) -> CallState {
          aBrowsingContext->SetIsActiveBrowserWindow(aActive);
          return CallState::Continue;
        });
  }

  if (aWindow->GetExtantDoc()) {
    nsContentUtils::DispatchEventOnlyToChrome(
        aWindow->GetExtantDoc(), aWindow->GetCurrentInnerWindow(),
        aActive ? u"activate"_ns : u"deactivate"_ns, CanBubble::eYes,
        Cancelable::eYes, nullptr);
  }
}

// Retrieves innerWindowId of the window of the last focused element to
// log a warning to the website console.
void LogWarningFullscreenWindowRaise(Element* aElement) {
  nsCOMPtr<nsFrameLoaderOwner> frameLoaderOwner(do_QueryInterface(aElement));
  NS_ENSURE_TRUE_VOID(frameLoaderOwner);

  RefPtr<nsFrameLoader> frameLoader = frameLoaderOwner->GetFrameLoader();
  NS_ENSURE_TRUE_VOID(frameLoaderOwner);

  RefPtr<BrowsingContext> browsingContext = frameLoader->GetBrowsingContext();
  NS_ENSURE_TRUE_VOID(browsingContext);

  WindowGlobalParent* windowGlobalParent =
      browsingContext->Canonical()->GetCurrentWindowGlobal();
  NS_ENSURE_TRUE_VOID(windowGlobalParent);

  // Log to console
  nsAutoString localizedMsg;
  nsTArray<nsString> params;
  nsresult rv = nsContentUtils::FormatLocalizedString(
      nsContentUtils::eDOM_PROPERTIES, "FullscreenExitWindowFocus", params,
      localizedMsg);

  NS_ENSURE_SUCCESS_VOID(rv);

  Unused << nsContentUtils::ReportToConsoleByWindowID(
      localizedMsg, nsIScriptError::warningFlag, "DOM"_ns,
      windowGlobalParent->InnerWindowId(),
      windowGlobalParent->GetDocumentURI());
}

void nsFocusManager::SetFocusInner(Element* aNewContent, int32_t aFlags,
                                   bool aFocusChanged, bool aAdjustWidget,
                                   uint64_t aActionId) {
  // if the element is not focusable, just return and leave the focus as is
  RefPtr<Element> elementToFocus =
      FlushAndCheckIfFocusable(aNewContent, aFlags);
  if (!elementToFocus) {
    return;
  }

  RefPtr<BrowsingContext> focusedBrowsingContext = GetFocusedBrowsingContext();

  // check if the element to focus is a frame (iframe) containing a child
  // document. Frames are never directly focused; instead focusing a frame
  // means focus what is inside the frame. To do this, the descendant content
  // within the frame is retrieved and that will be focused instead.
  nsCOMPtr<nsPIDOMWindowOuter> newWindow;
  nsCOMPtr<nsPIDOMWindowOuter> subWindow = GetContentWindow(elementToFocus);
  if (subWindow) {
    elementToFocus = GetFocusedDescendant(subWindow, eIncludeAllDescendants,
                                          getter_AddRefs(newWindow));

    // since a window is being refocused, clear aFocusChanged so that the
    // caret position isn't updated.
    aFocusChanged = false;
  }

  // unless it was set above, retrieve the window for the element to focus
  if (!newWindow) {
    newWindow = GetCurrentWindow(elementToFocus);
  }

  BrowsingContext* newBrowsingContext = nullptr;
  if (newWindow) {
    newBrowsingContext = newWindow->GetBrowsingContext();
  }

  // if the element is already focused, just return. Note that this happens
  // after the frame check above so that we compare the element that will be
  // focused rather than the frame it is in.
  if (!newWindow || (newBrowsingContext == GetFocusedBrowsingContext() &&
                     elementToFocus == mFocusedElement)) {
    return;
  }

  MOZ_ASSERT(newBrowsingContext);

  BrowsingContext* browsingContextToFocus = newBrowsingContext;
  if (RefPtr<nsFrameLoaderOwner> flo = do_QueryObject(elementToFocus)) {
    // Only look at pre-existing browsing contexts. If this function is
    // called during reflow, calling GetBrowsingContext() could cause frame
    // loader initialization at a time when it isn't safe.
    if (BrowsingContext* bc = flo->GetExtantBrowsingContext()) {
      // If focus is already in the subtree rooted at bc, return early
      // to match the single-process focus semantics. Otherwise, we'd
      // blur and immediately refocus whatever is focused.
      BrowsingContext* walk = focusedBrowsingContext;
      while (walk) {
        if (walk == bc) {
          return;
        }
        walk = walk->GetParent();
      }
      browsingContextToFocus = bc;
    }
  }

  // don't allow focus to be placed in docshells or descendants of docshells
  // that are being destroyed. Also, ensure that the page hasn't been
  // unloaded. The prevents content from being refocused during an unload event.
  nsCOMPtr<nsIDocShell> newDocShell = newWindow->GetDocShell();
  nsCOMPtr<nsIDocShell> docShell = newDocShell;
  while (docShell) {
    bool inUnload;
    docShell->GetIsInUnload(&inUnload);
    if (inUnload) {
      return;
    }

    bool beingDestroyed;
    docShell->IsBeingDestroyed(&beingDestroyed);
    if (beingDestroyed) {
      return;
    }

    BrowsingContext* bc = docShell->GetBrowsingContext();

    nsCOMPtr<nsIDocShellTreeItem> parentDsti;
    docShell->GetInProcessParent(getter_AddRefs(parentDsti));
    docShell = do_QueryInterface(parentDsti);
    if (!docShell && !XRE_IsParentProcess()) {
      // We don't have an in-process parent, but let's see if we have
      // an in-process ancestor or if an out-of-process ancestor
      // is discarded.
      do {
        bc = bc->GetParent();
        if (bc && bc->IsDiscarded()) {
          return;
        }
      } while (bc && !bc->IsInProcess());
      if (bc) {
        docShell = bc->GetDocShell();
      } else {
        docShell = nullptr;
      }
    }
  }

  bool focusMovesToDifferentBC =
      (focusedBrowsingContext != browsingContextToFocus);

  if (focusedBrowsingContext && focusMovesToDifferentBC &&
      nsContentUtils::IsHandlingKeyBoardEvent() &&
      !nsContentUtils::LegacyIsCallerChromeOrNativeCode()) {
    MOZ_ASSERT(browsingContextToFocus,
               "BrowsingContext to focus should be non-null.");

    nsIPrincipal* focusedPrincipal = nullptr;
    nsIPrincipal* newPrincipal = nullptr;

    if (XRE_IsParentProcess()) {
      if (WindowGlobalParent* focusedWindowGlobalParent =
              focusedBrowsingContext->Canonical()->GetCurrentWindowGlobal()) {
        focusedPrincipal = focusedWindowGlobalParent->DocumentPrincipal();
      }

      if (WindowGlobalParent* newWindowGlobalParent =
              browsingContextToFocus->Canonical()->GetCurrentWindowGlobal()) {
        newPrincipal = newWindowGlobalParent->DocumentPrincipal();
      }
    } else if (focusedBrowsingContext->IsInProcess() &&
               browsingContextToFocus->IsInProcess()) {
      nsCOMPtr<nsIScriptObjectPrincipal> focused =
          do_QueryInterface(focusedBrowsingContext->GetDOMWindow());
      nsCOMPtr<nsIScriptObjectPrincipal> newFocus =
          do_QueryInterface(browsingContextToFocus->GetDOMWindow());
      MOZ_ASSERT(focused && newFocus,
                 "BrowsingContext should always have a window here.");
      focusedPrincipal = focused->GetPrincipal();
      newPrincipal = newFocus->GetPrincipal();
    }

    if (!focusedPrincipal || !newPrincipal) {
      return;
    }

    if (!focusedPrincipal->Subsumes(newPrincipal)) {
      NS_WARNING("Not allowed to focus the new window!");
      return;
    }
  }

  // to check if the new element is in the active window, compare the
  // new root docshell for the new element with the active window's docshell.
  RefPtr<BrowsingContext> newRootBrowsingContext = nullptr;
  bool isElementInActiveWindow = false;
  if (XRE_IsParentProcess()) {
    nsCOMPtr<nsPIDOMWindowOuter> newRootWindow = nullptr;
    nsCOMPtr<nsIDocShellTreeItem> dsti = newWindow->GetDocShell();
    if (dsti) {
      nsCOMPtr<nsIDocShellTreeItem> root;
      dsti->GetInProcessRootTreeItem(getter_AddRefs(root));
      newRootWindow = root ? root->GetWindow() : nullptr;

      isElementInActiveWindow =
          (mActiveWindow && newRootWindow == mActiveWindow);
    }
    if (newRootWindow) {
      newRootBrowsingContext = newRootWindow->GetBrowsingContext();
    }
  } else {
    // XXX This is wrong for `<iframe mozbrowser>` and for XUL
    // `<browser remote="true">`. See:
    // https://searchfox.org/mozilla-central/rev/8a63fc190b39ed6951abb4aef4a56487a43962bc/dom/base/nsFrameLoader.cpp#229-232
    newRootBrowsingContext = newBrowsingContext->Top();
    // to check if the new element is in the active window, compare the
    // new root docshell for the new element with the active window's docshell.
    isElementInActiveWindow =
        (GetActiveBrowsingContext() == newRootBrowsingContext);
  }

  // Exit fullscreen if a website focuses another window
  if (StaticPrefs::full_screen_api_exit_on_windowRaise() &&
      !isElementInActiveWindow && (aFlags & FLAG_RAISE) &&
      (aFlags & FLAG_NONSYSTEMCALLER)) {
    if (XRE_IsParentProcess()) {
      if (Document* doc = mActiveWindow ? mActiveWindow->GetDoc() : nullptr) {
        if (doc->GetFullscreenElement()) {
          LogWarningFullscreenWindowRaise(mFocusedElement);
          Document::AsyncExitFullscreen(doc);
        }
      }
    } else {
      BrowsingContext* activeBrowsingContext = GetActiveBrowsingContext();
      if (activeBrowsingContext) {
        nsIDocShell* shell = activeBrowsingContext->GetDocShell();
        if (shell) {
          Document* doc = shell->GetDocument();
          if (doc && doc->GetFullscreenElement()) {
            Document::AsyncExitFullscreen(doc);
          }
        } else {
          mozilla::dom::ContentChild* contentChild =
              mozilla::dom::ContentChild::GetSingleton();
          MOZ_ASSERT(contentChild);
          contentChild->SendMaybeExitFullscreen(activeBrowsingContext);
        }
      }
    }
  }

  // Exit fullscreen if we're focusing a windowed plugin on a non-MacOSX
  // system. We don't control event dispatch to windowed plugins on non-MacOSX,
  // so we can't display the "Press ESC to leave fullscreen mode" warning on
  // key input if a windowed plugin is focused, so just exit fullscreen
  // to guard against phishing.
#ifndef XP_MACOSX
  if (elementToFocus &&
      nsContentUtils::GetInProcessSubtreeRootDocument(
          elementToFocus->OwnerDoc())
          ->GetFullscreenElement() &&
      nsContentUtils::HasPluginWithUncontrolledEventDispatch(elementToFocus)) {
    nsContentUtils::ReportToConsole(nsIScriptError::warningFlag, "DOM"_ns,
                                    elementToFocus->OwnerDoc(),
                                    nsContentUtils::eDOM_PROPERTIES,
                                    "FocusedWindowedPluginWhileFullscreen");
    Document::AsyncExitFullscreen(elementToFocus->OwnerDoc());
  }
#endif

  // if the FLAG_NOSWITCHFRAME flag is used, only allow the focus to be
  // shifted away from the current element if the new shell to focus is
  // the same or an ancestor shell of the currently focused shell.
  bool allowFrameSwitch = !(aFlags & FLAG_NOSWITCHFRAME) ||
                          IsSameOrAncestor(newWindow, focusedBrowsingContext);

  // if the element is in the active window, frame switching is allowed and
  // the content is in a visible window, fire blur and focus events.
  bool sendFocusEvent =
      isElementInActiveWindow && allowFrameSwitch && IsWindowVisible(newWindow);

  // Don't allow to steal the focus from chrome nodes if the caller cannot
  // access them.
  if (sendFocusEvent && mFocusedElement &&
      mFocusedElement->OwnerDoc() != aNewContent->OwnerDoc() &&
      mFocusedElement->NodePrincipal()->IsSystemPrincipal() &&
      !nsContentUtils::LegacyIsCallerNativeCode() &&
      !nsContentUtils::CanCallerAccess(mFocusedElement)) {
    sendFocusEvent = false;
  }

  LOGCONTENT("Shift Focus: %s", elementToFocus.get());
  LOGFOCUS((" Flags: %x Current Window: %p New Window: %p Current Element: %p",
            aFlags, mFocusedWindow.get(), newWindow.get(),
            mFocusedElement.get()));
  LOGFOCUS(
      (" In Active Window: %d Moves to different BrowsingContext: %d "
       "SendFocus: %d actionid: %" PRIu64,
       isElementInActiveWindow, focusMovesToDifferentBC, sendFocusEvent,
       aActionId));

  if (sendFocusEvent) {
    Maybe<BlurredElementInfo> blurredInfo;
    if (mFocusedElement) {
      blurredInfo.emplace(*mFocusedElement);
    }
    // return if blurring fails or the focus changes during the blur
    if (focusedBrowsingContext) {
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
      RefPtr<BrowsingContext> commonAncestor;
      if (focusMovesToDifferentBC) {
        commonAncestor = GetCommonAncestor(newWindow, focusedBrowsingContext);
      }

      bool needToClearFocusedElement = false;
      if (focusedBrowsingContext->IsChrome()) {
        // Always reset focused element if focus is currently in chrome window.
        needToClearFocusedElement = true;
      } else {
        // Only reset focused element if focus moves within the same top-level
        // content window.
        if (focusedBrowsingContext->Top() == browsingContextToFocus->Top()) {
          // XXX for the case that we try to focus an
          // already-focused-remote-frame, we would still send blur and focus
          // IPC to it, but they will not generate blur or focus event, we don't
          // want to reset activeElement on the remote frame.
          needToClearFocusedElement = (focusMovesToDifferentBC ||
                                       focusedBrowsingContext->IsInProcess());
        }
      }

      if (!Blur(needToClearFocusedElement ? focusedBrowsingContext.get()
                                          : nullptr,
                commonAncestor ? commonAncestor.get() : nullptr,
                focusMovesToDifferentBC, aAdjustWidget, aActionId,
                elementToFocus)) {
        return;
      }
    }

    Focus(newWindow, elementToFocus, aFlags, focusMovesToDifferentBC,
          aFocusChanged, false, aAdjustWidget, aActionId, blurredInfo);
  } else {
    // otherwise, for inactive windows and when the caller cannot steal the
    // focus, update the node in the window, and  raise the window if desired.
    if (allowFrameSwitch) {
      AdjustWindowFocus(newBrowsingContext, true, IsWindowVisible(newWindow),
                        aActionId);
    }

    // set the focus node and method as needed
    uint32_t focusMethod =
        aFocusChanged ? aFlags & METHODANDRING_MASK
                      : newWindow->GetFocusMethod() |
                            (aFlags & (FLAG_SHOWRING | FLAG_NOSHOWRING));
    newWindow->SetFocusedElement(elementToFocus, focusMethod);
    if (aFocusChanged) {
      nsCOMPtr<nsIDocShell> docShell = newWindow->GetDocShell();

      RefPtr<PresShell> presShell = docShell->GetPresShell();
      if (presShell && presShell->DidInitialize()) {
        ScrollIntoView(presShell, elementToFocus, aFlags);
      }
    }

    // update the commands even when inactive so that the attributes for that
    // window are up to date.
    if (allowFrameSwitch) {
      newWindow->UpdateCommands(u"focus"_ns, nullptr, 0);
    }

    if (aFlags & FLAG_RAISE) {
      if (newRootBrowsingContext) {
        if (XRE_IsParentProcess() || newRootBrowsingContext->IsInProcess()) {
          RaiseWindow(newRootBrowsingContext->GetDOMWindow(),
                      aFlags & FLAG_NONSYSTEMCALLER ? CallerType::NonSystem
                                                    : CallerType::System,
                      aActionId);
        } else {
          mozilla::dom::ContentChild* contentChild =
              mozilla::dom::ContentChild::GetSingleton();
          MOZ_ASSERT(contentChild);
          contentChild->SendRaiseWindow(newRootBrowsingContext,
                                        aFlags & FLAG_NONSYSTEMCALLER
                                            ? CallerType::NonSystem
                                            : CallerType::System,
                                        aActionId);
        }
      }
    }
  }
}

static already_AddRefed<BrowsingContext> GetParentIgnoreChromeBoundary(
    BrowsingContext* aBC) {
  // Chrome BrowsingContexts are only available in the parent process, so if
  // we're in a content process, we only worry about the context tree.
  if (XRE_IsParentProcess()) {
    return aBC->Canonical()->GetParentCrossChromeBoundary();
  }
  return do_AddRef(aBC->GetParent());
}

bool nsFocusManager::IsSameOrAncestor(BrowsingContext* aPossibleAncestor,
                                      BrowsingContext* aContext) const {
  if (!aPossibleAncestor) {
    return false;
  }

  for (RefPtr<BrowsingContext> bc = aContext; bc;
       bc = GetParentIgnoreChromeBoundary(bc)) {
    if (bc == aPossibleAncestor) {
      return true;
    }
  }

  return false;
}

bool nsFocusManager::IsSameOrAncestor(nsPIDOMWindowOuter* aPossibleAncestor,
                                      nsPIDOMWindowOuter* aWindow) const {
  if (aWindow && aPossibleAncestor) {
    return IsSameOrAncestor(aPossibleAncestor->GetBrowsingContext(),
                            aWindow->GetBrowsingContext());
  }
  return false;
}

bool nsFocusManager::IsSameOrAncestor(nsPIDOMWindowOuter* aPossibleAncestor,
                                      BrowsingContext* aContext) const {
  if (aPossibleAncestor) {
    return IsSameOrAncestor(aPossibleAncestor->GetBrowsingContext(), aContext);
  }
  return false;
}

bool nsFocusManager::IsSameOrAncestor(BrowsingContext* aPossibleAncestor,
                                      nsPIDOMWindowOuter* aWindow) const {
  if (aWindow) {
    return IsSameOrAncestor(aPossibleAncestor, aWindow->GetBrowsingContext());
  }
  return false;
}

mozilla::dom::BrowsingContext* nsFocusManager::GetCommonAncestor(
    nsPIDOMWindowOuter* aWindow, mozilla::dom::BrowsingContext* aContext) {
  NS_ENSURE_TRUE(aWindow && aContext, nullptr);

  if (XRE_IsParentProcess()) {
    nsCOMPtr<nsIDocShellTreeItem> dsti1 = aWindow->GetDocShell();
    NS_ENSURE_TRUE(dsti1, nullptr);

    nsCOMPtr<nsIDocShellTreeItem> dsti2 = aContext->GetDocShell();
    NS_ENSURE_TRUE(dsti2, nullptr);

    AutoTArray<nsIDocShellTreeItem*, 30> parents1, parents2;
    do {
      parents1.AppendElement(dsti1);
      nsCOMPtr<nsIDocShellTreeItem> parentDsti1;
      dsti1->GetInProcessParent(getter_AddRefs(parentDsti1));
      dsti1.swap(parentDsti1);
    } while (dsti1);
    do {
      parents2.AppendElement(dsti2);
      nsCOMPtr<nsIDocShellTreeItem> parentDsti2;
      dsti2->GetInProcessParent(getter_AddRefs(parentDsti2));
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

    return parent ? parent->GetBrowsingContext() : nullptr;
  }

  BrowsingContext* bc1 = aWindow->GetBrowsingContext();
  NS_ENSURE_TRUE(bc1, nullptr);

  BrowsingContext* bc2 = aContext;

  AutoTArray<BrowsingContext*, 30> parents1, parents2;
  do {
    parents1.AppendElement(bc1);
    bc1 = bc1->GetParent();
  } while (bc1);
  do {
    parents2.AppendElement(bc2);
    bc2 = bc2->GetParent();
  } while (bc2);

  uint32_t pos1 = parents1.Length();
  uint32_t pos2 = parents2.Length();
  BrowsingContext* parent = nullptr;
  uint32_t len;
  for (len = std::min(pos1, pos2); len > 0; --len) {
    BrowsingContext* child1 = parents1.ElementAt(--pos1);
    BrowsingContext* child2 = parents2.ElementAt(--pos2);
    if (child1 != child2) {
      break;
    }
    parent = child1;
  }

  return parent;
}

bool nsFocusManager::AdjustInProcessWindowFocus(
    BrowsingContext* aBrowsingContext, bool aCheckPermission, bool aIsVisible,
    uint64_t aActionId) {
  BrowsingContext* bc = aBrowsingContext;
  bool needToNotifyOtherProcess = false;
  while (bc) {
    // get the containing <iframe> or equivalent element so that it can be
    // focused below.
    nsCOMPtr<Element> frameElement = bc->GetEmbedderElement();
    BrowsingContext* parent = bc->GetParent();
    if (!parent && XRE_IsParentProcess()) {
      CanonicalBrowsingContext* canonical = bc->Canonical();
      RefPtr<WindowGlobalParent> embedder =
          canonical->GetEmbedderWindowGlobal();
      if (embedder) {
        parent = embedder->BrowsingContext();
      }
    }
    bc = parent;
    if (!bc) {
      break;
    }
    if (!frameElement && XRE_IsContentProcess()) {
      needToNotifyOtherProcess = true;
      continue;
    }

    nsCOMPtr<nsPIDOMWindowOuter> window = bc->GetDOMWindow();
    MOZ_ASSERT(window);
    // if the parent window is visible but the original window was not, then we
    // have likely moved up and out from a hidden tab to the browser window, or
    // a similar such arrangement. Stop adjusting the current nodes.
    if (IsWindowVisible(window) != aIsVisible) {
      break;
    }

    // When aCheckPermission is true, we should check whether the caller can
    // access the window or not.  If it cannot access, we should stop the
    // adjusting.
    if (aCheckPermission && !nsContentUtils::LegacyIsCallerNativeCode() &&
        !nsContentUtils::CanCallerAccess(window->GetCurrentInnerWindow())) {
      break;
    }

    if (frameElement != window->GetFocusedElement()) {
      window->SetFocusedElement(frameElement);

      RefPtr<nsFrameLoaderOwner> loaderOwner = do_QueryObject(frameElement);
      MOZ_ASSERT(loaderOwner);
      RefPtr<nsFrameLoader> loader = loaderOwner->GetFrameLoader();
      if (loader && loader->IsRemoteFrame() &&
          GetFocusedBrowsingContext() == bc) {
        Blur(nullptr, nullptr, true, true, aActionId);
      }
    }
  }
  return needToNotifyOtherProcess;
}

void nsFocusManager::AdjustWindowFocus(BrowsingContext* aBrowsingContext,
                                       bool aCheckPermission, bool aIsVisible,
                                       uint64_t aActionId) {
  if (AdjustInProcessWindowFocus(aBrowsingContext, aCheckPermission, aIsVisible,
                                 aActionId)) {
    // Some ancestors of aBrowsingContext isn't in this process, so notify other
    // processes to adjust their focused element.
    mozilla::dom::ContentChild* contentChild =
        mozilla::dom::ContentChild::GetSingleton();
    MOZ_ASSERT(contentChild);
    contentChild->SendAdjustWindowFocus(aBrowsingContext, aIsVisible,
                                        aActionId);
  }
}

bool nsFocusManager::IsWindowVisible(nsPIDOMWindowOuter* aWindow) {
  if (!aWindow || aWindow->IsFrozen()) {
    return false;
  }

  // Check if the inner window is frozen as well. This can happen when a focus
  // change occurs while restoring a previous page.
  nsPIDOMWindowInner* innerWindow = aWindow->GetCurrentInnerWindow();
  if (!innerWindow || innerWindow->IsFrozen()) {
    return false;
  }

  nsCOMPtr<nsIDocShell> docShell = aWindow->GetDocShell();
  nsCOMPtr<nsIBaseWindow> baseWin(do_QueryInterface(docShell));
  if (!baseWin) {
    return false;
  }

  bool visible = false;
  baseWin->GetVisibility(&visible);
  return visible;
}

bool nsFocusManager::IsNonFocusableRoot(nsIContent* aContent) {
  MOZ_ASSERT(aContent, "aContent must not be NULL");
  MOZ_ASSERT(aContent->IsInComposedDoc(), "aContent must be in a document");

  // If the uncomposed document of aContent is in designMode, the root element
  // is not focusable.
  // NOTE: Most elements whose uncomposed document is in design mode are not
  //       focusable, just the document is focusable.  However, if it's in a
  //       shadow tree, it may be focus able even if the shadow host is in
  //       design mode.
  // Also, if aContent is not editable and it's not in designMode, it's not
  // focusable.
  // And in userfocusignored context nothing is focusable.
  Document* doc = aContent->GetComposedDoc();
  NS_ASSERTION(doc, "aContent must have current document");
  return aContent == doc->GetRootElement() &&
         (aContent->IsInDesignMode() || !aContent->IsEditable());
}

Element* nsFocusManager::FlushAndCheckIfFocusable(Element* aElement,
                                                  uint32_t aFlags) {
  if (!aElement) {
    return nullptr;
  }

  nsCOMPtr<Document> doc = aElement->GetComposedDoc();
  // can't focus elements that are not in documents
  if (!doc) {
    LOGCONTENT("Cannot focus %s because content not in document", aElement)
    return nullptr;
  }

  // Make sure that our frames are up to date while ensuring the presshell is
  // also initialized in case we come from a script calling focus() early.
  mEventHandlingNeedsFlush = false;
  doc->FlushPendingNotifications(FlushType::EnsurePresShellInitAndFrames);

  // this is a special case for some XUL elements or input number, where an
  // anonymous child is actually focusable and not the element itself.
  if (RefPtr<Element> redirectedFocus = GetRedirectedFocus(aElement)) {
    return FlushAndCheckIfFocusable(redirectedFocus, aFlags);
  }

  PresShell* presShell = doc->GetPresShell();
  if (!presShell) {
    return nullptr;
  }

  // the root content can always be focused,
  // except in userfocusignored context.
  if (aElement == doc->GetRootElement()) {
    return aElement;
  }

  nsIFrame* frame = aElement->GetPrimaryFrame();
  if (!frame) {
    LOGCONTENT("Cannot focus %s as it has no frame", aElement)
    return nullptr;
  }

  if (aElement->IsHTMLElement(nsGkAtoms::area)) {
    // HTML areas do not have their own frame, and the img frame we get from
    // GetPrimaryFrame() is not relevant as to whether it is focusable or
    // not, so we have to do all the relevant checks manually for them.
    return frame->IsVisibleConsideringAncestors() && aElement->IsFocusable()
               ? aElement
               : nullptr;
  }

  // If this is an iframe that doesn't have an in-process subdocument, it is
  // either an OOP iframe or an in-process iframe without lazy about:blank
  // creation having taken place. In the OOP case, treat the frame as
  // focusable for consistency with Chrome. In the in-process case, create
  // the initial about:blank for in-process BrowsingContexts in order to
  // have the `GetSubDocumentFor` call after this block return something.
  if (RefPtr<nsFrameLoaderOwner> flo = do_QueryObject(aElement)) {
    // dom/webauthn/tests/browser/browser_abort_visibility.js fails without
    // the exclusion of XUL.
    if (aElement->NodeInfo()->NamespaceID() != kNameSpaceID_XUL) {
      // Only look at pre-existing browsing contexts. If this function is
      // called during reflow, calling GetBrowsingContext() could cause frame
      // loader initialization at a time when it isn't safe.
      if (BrowsingContext* bc = flo->GetExtantBrowsingContext()) {
        // This call may create a contentViewer-created about:blank.
        // That's intentional, so we can move focus there.
        Document* subdoc = bc->GetDocument();
        if (!subdoc) {
          return aElement;
        }
        nsIPrincipal* framerPrincipal = doc->GetPrincipal();
        nsIPrincipal* frameePrincipal = subdoc->GetPrincipal();
        if (framerPrincipal && frameePrincipal &&
            !framerPrincipal->Equals(frameePrincipal)) {
          // Assume focusability of different-origin iframes even in the
          // in-process case for consistency with the OOP case.
          // This is likely already the case anyway, but in case not,
          // this makes it explicitly so.
          return aElement;
        }
      }
    }
  }

  if (frame->IsFocusable(aFlags & FLAG_BYMOUSE)) {
    return aElement;
  }

  if (ShadowRoot* root = aElement->GetShadowRoot()) {
    if (root->DelegatesFocus()) {
      // If focus target's shadow root is a shadow-including inclusive ancestor
      // of the currently focused area of a top-level browsing context's DOM
      // anchor, then return null.
      //
      // Note that the spec still uses the host, see
      // https://github.com/whatwg/html/issues/7207
      if (nsPIDOMWindowInner* innerWindow =
              aElement->OwnerDoc()->GetInnerWindow()) {
        BrowsingContext* bc = innerWindow->GetBrowsingContext();
        if (bc && bc->IsTop()) {
          if (Element* focusedElement = innerWindow->GetFocusedElement()) {
            if (focusedElement->IsShadowIncludingInclusiveDescendantOf(root)) {
              return nullptr;
            }
          }
        }
      }

      if (Element* firstFocusable =
              root->GetFirstFocusable(aFlags & FLAG_BYMOUSE)) {
        return firstFocusable;
      }
    }
  }

  return nullptr;
}

bool nsFocusManager::Blur(BrowsingContext* aBrowsingContextToClear,
                          BrowsingContext* aAncestorBrowsingContextToFocus,
                          bool aIsLeavingDocument, bool aAdjustWidget,
                          uint64_t aActionId, Element* aElementToFocus) {
  if (XRE_IsParentProcess()) {
    return BlurImpl(aBrowsingContextToClear, aAncestorBrowsingContextToFocus,
                    aIsLeavingDocument, aAdjustWidget, aElementToFocus,
                    aActionId);
  }
  mozilla::dom::ContentChild* contentChild =
      mozilla::dom::ContentChild::GetSingleton();
  MOZ_ASSERT(contentChild);
  bool windowToClearHandled = false;
  bool ancestorWindowToFocusHandled = false;

  RefPtr<BrowsingContext> focusedBrowsingContext = GetFocusedBrowsingContext();
  if (focusedBrowsingContext && focusedBrowsingContext->IsDiscarded()) {
    focusedBrowsingContext = nullptr;
  }
  if (!focusedBrowsingContext) {
    mFocusedElement = nullptr;
    return true;
  }
  if (aBrowsingContextToClear && aBrowsingContextToClear->IsDiscarded()) {
    aBrowsingContextToClear = nullptr;
  }
  if (aAncestorBrowsingContextToFocus &&
      aAncestorBrowsingContextToFocus->IsDiscarded()) {
    aAncestorBrowsingContextToFocus = nullptr;
  }
  // XXX should more early returns from BlurImpl be hoisted here to avoid
  // processing aBrowsingContextToClear and aAncestorBrowsingContextToFocus in
  // other processes when BlurImpl returns early in this process? Or should the
  // IPC messages for those be sent by BlurImpl itself, in which case they could
  // arrive late?
  if (focusedBrowsingContext->IsInProcess()) {
    if (aBrowsingContextToClear && !aBrowsingContextToClear->IsInProcess()) {
      MOZ_RELEASE_ASSERT(!(aAncestorBrowsingContextToFocus &&
                           !aAncestorBrowsingContextToFocus->IsInProcess()),
                         "Both aBrowsingContextToClear and "
                         "aAncestorBrowsingContextToFocus are "
                         "out-of-process.");
      contentChild->SendSetFocusedElement(aBrowsingContextToClear, false);
    }
    if (aAncestorBrowsingContextToFocus &&
        !aAncestorBrowsingContextToFocus->IsInProcess()) {
      contentChild->SendSetFocusedElement(aAncestorBrowsingContextToFocus,
                                          true);
    }
    return BlurImpl(aBrowsingContextToClear, aAncestorBrowsingContextToFocus,
                    aIsLeavingDocument, aAdjustWidget, aElementToFocus,
                    aActionId);
  }
  if (aBrowsingContextToClear && aBrowsingContextToClear->IsInProcess()) {
    nsPIDOMWindowOuter* windowToClear = aBrowsingContextToClear->GetDOMWindow();
    MOZ_ASSERT(windowToClear);
    windowToClear->SetFocusedElement(nullptr);
    windowToClearHandled = true;
  }
  if (aAncestorBrowsingContextToFocus &&
      aAncestorBrowsingContextToFocus->IsInProcess()) {
    nsPIDOMWindowOuter* ancestorWindowToFocus =
        aAncestorBrowsingContextToFocus->GetDOMWindow();
    MOZ_ASSERT(ancestorWindowToFocus);
    ancestorWindowToFocus->SetFocusedElement(nullptr, 0, true);
    ancestorWindowToFocusHandled = true;
  }
  // The expectation is that the blurring would eventually result in an IPC
  // message doing this anyway, but this doesn't happen if the focus is in OOP
  // iframe which won't try to bounce an IPC message to its parent frame.
  SetFocusedWindowInternal(nullptr, aActionId);
  contentChild->SendBlurToParent(
      focusedBrowsingContext, aBrowsingContextToClear,
      aAncestorBrowsingContextToFocus, aIsLeavingDocument, aAdjustWidget,
      windowToClearHandled, ancestorWindowToFocusHandled, aActionId);
  return true;
}

void nsFocusManager::BlurFromOtherProcess(
    mozilla::dom::BrowsingContext* aFocusedBrowsingContext,
    mozilla::dom::BrowsingContext* aBrowsingContextToClear,
    mozilla::dom::BrowsingContext* aAncestorBrowsingContextToFocus,
    bool aIsLeavingDocument, bool aAdjustWidget, uint64_t aActionId) {
  if (aFocusedBrowsingContext != GetFocusedBrowsingContext()) {
    return;
  }
  BlurImpl(aBrowsingContextToClear, aAncestorBrowsingContextToFocus,
           aIsLeavingDocument, aAdjustWidget, nullptr, aActionId);
}

bool nsFocusManager::BlurImpl(BrowsingContext* aBrowsingContextToClear,
                              BrowsingContext* aAncestorBrowsingContextToFocus,
                              bool aIsLeavingDocument, bool aAdjustWidget,
                              Element* aElementToFocus, uint64_t aActionId) {
  LOGFOCUS(("<<Blur begin actionid: %" PRIu64 ">>", aActionId));

  // hold a reference to the focused content, which may be null
  RefPtr<Element> element = mFocusedElement;
  if (element) {
    if (!element->IsInComposedDoc()) {
      mFocusedElement = nullptr;
      return true;
    }
    if (element == mFirstBlurEvent) {
      return true;
    }
  }

  RefPtr<BrowsingContext> focusedBrowsingContext = GetFocusedBrowsingContext();
  // hold a reference to the focused window
  nsCOMPtr<nsPIDOMWindowOuter> window;
  if (focusedBrowsingContext) {
    window = focusedBrowsingContext->GetDOMWindow();
  }
  if (!window) {
    mFocusedElement = nullptr;
    return true;
  }

  nsCOMPtr<nsIDocShell> docShell = window->GetDocShell();
  if (!docShell) {
    if (XRE_IsContentProcess() &&
        ActionIdComparableAndLower(
            aActionId, mActionIdForFocusedBrowsingContextInContent)) {
      // Unclear if this ever happens.
      LOGFOCUS(
          ("Ignored an attempt to null out focused BrowsingContext when "
           "docShell is null due to a stale action id %" PRIu64 ".",
           aActionId));
      return true;
    }

    mFocusedWindow = nullptr;
    // Setting focused BrowsingContext to nullptr to avoid leaking in print
    // preview.
    SetFocusedBrowsingContext(nullptr, aActionId);
    mFocusedElement = nullptr;
    return true;
  }

  // Keep a ref to presShell since dispatching the DOM event may cause
  // the document to be destroyed.
  RefPtr<PresShell> presShell = docShell->GetPresShell();
  if (!presShell) {
    if (XRE_IsContentProcess() &&
        ActionIdComparableAndLower(
            aActionId, mActionIdForFocusedBrowsingContextInContent)) {
      // Unclear if this ever happens.
      LOGFOCUS(
          ("Ignored an attempt to null out focused BrowsingContext when "
           "presShell is null due to a stale action id %" PRIu64 ".",
           aActionId));
      return true;
    }
    mFocusedElement = nullptr;
    mFocusedWindow = nullptr;
    // Setting focused BrowsingContext to nullptr to avoid leaking in print
    // preview.
    SetFocusedBrowsingContext(nullptr, aActionId);
    return true;
  }

  Maybe<AutoRestore<RefPtr<Element>>> ar;
  if (!mFirstBlurEvent) {
    ar.emplace(mFirstBlurEvent);
    mFirstBlurEvent = element;
  }

  nsPresContext* focusedPresContext =
      GetActiveBrowsingContext() ? presShell->GetPresContext() : nullptr;
  IMEStateManager::OnChangeFocus(focusedPresContext, nullptr,
                                 GetFocusMoveActionCause(0));

  // now adjust the actual focus, by clearing the fields in the focus manager
  // and in the window.
  mFocusedElement = nullptr;
  if (aBrowsingContextToClear) {
    nsPIDOMWindowOuter* windowToClear = aBrowsingContextToClear->GetDOMWindow();
    if (windowToClear) {
      windowToClear->SetFocusedElement(nullptr);
    }
  }

  LOGCONTENT("Element %s has been blurred", element.get());

  // Don't fire blur event on the root content which isn't editable.
  bool sendBlurEvent =
      element && element->IsInComposedDoc() && !IsNonFocusableRoot(element);
  if (element) {
    if (sendBlurEvent) {
      NotifyFocusStateChange(element, aElementToFocus, 0, false, false);
    }

    bool windowBeingLowered = !aBrowsingContextToClear &&
                              !aAncestorBrowsingContextToFocus &&
                              aIsLeavingDocument && aAdjustWidget;
    // if the object being blurred is a remote browser, deactivate remote
    // content
    if (BrowserParent* remote = BrowserParent::GetFrom(element)) {
      MOZ_ASSERT(XRE_IsParentProcess());
      // Let's deactivate all remote browsers.
      BrowsingContext* topLevelBrowsingContext = remote->GetBrowsingContext();
      topLevelBrowsingContext->PreOrderWalk([&](BrowsingContext* aContext) {
        if (WindowGlobalParent* windowGlobalParent =
                aContext->Canonical()->GetCurrentWindowGlobal()) {
          if (RefPtr<BrowserParent> browserParent =
                  windowGlobalParent->GetBrowserParent()) {
            browserParent->Deactivate(windowBeingLowered, aActionId);
            LOGFOCUS(
                ("%s remote browser deactivated %p, %d, actionid: %" PRIu64,
                 aContext == topLevelBrowsingContext ? "Top-level"
                                                     : "OOP iframe",
                 browserParent.get(), windowBeingLowered, aActionId));
          }
        }
      });
    }

    // Same as above but for out-of-process iframes
    if (BrowserBridgeChild* bbc = BrowserBridgeChild::GetFrom(element)) {
      bbc->Deactivate(windowBeingLowered, aActionId);
      LOGFOCUS(("Out-of-process iframe deactivated %p, %d, actionid: %" PRIu64,
                bbc, windowBeingLowered, aActionId));
    }
  }

  bool result = true;
  if (sendBlurEvent) {
    // if there is an active window, update commands. If there isn't an active
    // window, then this was a blur caused by the active window being lowered,
    // so there is no need to update the commands
    if (GetActiveBrowsingContext()) {
      window->UpdateCommands(u"focus"_ns, nullptr, 0);
    }

    SendFocusOrBlurEvent(eBlur, presShell, element->GetComposedDoc(), element,
                         false, false, aElementToFocus);
  }

  // if we are leaving the document or the window was lowered, make the caret
  // invisible.
  if (aIsLeavingDocument || !GetActiveBrowsingContext()) {
    SetCaretVisible(presShell, false, nullptr);
  }

  RefPtr<AccessibleCaretEventHub> eventHub =
      presShell->GetAccessibleCaretEventHub();
  if (eventHub) {
    eventHub->NotifyBlur(aIsLeavingDocument || !GetActiveBrowsingContext());
  }

  // at this point, it is expected that this window will be still be
  // focused, but the focused element will be null, as it was cleared before
  // the event. If this isn't the case, then something else was focused during
  // the blur event above and we should just return. However, if
  // aIsLeavingDocument is set, a new document is desired, so make sure to
  // blur the document and window.
  if (GetFocusedBrowsingContext() != window->GetBrowsingContext() ||
      (mFocusedElement != nullptr && !aIsLeavingDocument)) {
    result = false;
  } else if (aIsLeavingDocument) {
    window->TakeFocus(false, 0);

    // clear the focus so that the ancestor frame hierarchy is in the correct
    // state. Pass true because aAncestorBrowsingContextToFocus is thought to be
    // focused at this point.
    if (aAncestorBrowsingContextToFocus) {
      nsPIDOMWindowOuter* ancestorWindowToFocus =
          aAncestorBrowsingContextToFocus->GetDOMWindow();
      if (ancestorWindowToFocus) {
        ancestorWindowToFocus->SetFocusedElement(nullptr, 0, true);
      }
    }

    SetFocusedWindowInternal(nullptr, aActionId);
    mFocusedElement = nullptr;

    Document* doc = window->GetExtantDoc();
    if (doc) {
      SendFocusOrBlurEvent(eBlur, presShell, doc, ToSupports(doc), false);
    }
    if (!GetFocusedBrowsingContext()) {
      SendFocusOrBlurEvent(eBlur, presShell, doc,
                           window->GetCurrentInnerWindow(), false);
    }

    // check if a different window was focused
    result = (!GetFocusedBrowsingContext() && GetActiveBrowsingContext());
  } else if (GetActiveBrowsingContext()) {
    // Otherwise, the blur of the element without blurring the document
    // occurred normally. Call UpdateCaret to redisplay the caret at the right
    // location within the document. This is needed to ensure that the caret
    // used for caret browsing is made visible again when an input field is
    // blurred.
    UpdateCaret(false, true, nullptr);
  }

  return result;
}

void nsFocusManager::ActivateRemoteFrameIfNeeded(Element& aElement,
                                                 uint64_t aActionId) {
  if (BrowserParent* remote = BrowserParent::GetFrom(&aElement)) {
    remote->Activate(aActionId);
    LOGFOCUS(
        ("Remote browser activated %p, actionid: %" PRIu64, remote, aActionId));
  }

  // Same as above but for out-of-process iframes
  if (BrowserBridgeChild* bbc = BrowserBridgeChild::GetFrom(&aElement)) {
    bbc->Activate(aActionId);
    LOGFOCUS(("Out-of-process iframe activated %p, actionid: %" PRIu64, bbc,
              aActionId));
  }
}

void nsFocusManager::Focus(
    nsPIDOMWindowOuter* aWindow, Element* aElement, uint32_t aFlags,
    bool aIsNewDocument, bool aFocusChanged, bool aWindowRaised,
    bool aAdjustWidget, uint64_t aActionId,
    const Maybe<BlurredElementInfo>& aBlurredElementInfo) {
  LOGFOCUS(("<<Focus begin actionid: %" PRIu64 ">>", aActionId));

  if (!aWindow) {
    return;
  }

  if (aElement &&
      (aElement == mFirstFocusEvent || aElement == mFirstBlurEvent)) {
    return;
  }

  // Keep a reference to the presShell since dispatching the DOM event may
  // cause the document to be destroyed.
  nsCOMPtr<nsIDocShell> docShell = aWindow->GetDocShell();
  if (!docShell) {
    return;
  }

  RefPtr<PresShell> presShell = docShell->GetPresShell();
  if (!presShell) {
    return;
  }

  bool focusInOtherContentProcess = false;
  // Keep mochitest-browser-chrome harness happy by ignoring
  // focusInOtherContentProcess in the chrome process, because the harness
  // expects that.
  if (!XRE_IsParentProcess()) {
    if (RefPtr<nsFrameLoaderOwner> flo = do_QueryObject(aElement)) {
      // Only look at pre-existing browsing contexts. If this function is
      // called during reflow, calling GetBrowsingContext() could cause frame
      // loader initialization at a time when it isn't safe.
      if (BrowsingContext* bc = flo->GetExtantBrowsingContext()) {
        focusInOtherContentProcess = !bc->IsInProcess();
      }
    }

    if (ActionIdComparableAndLower(
            aActionId, mActionIdForFocusedBrowsingContextInContent)) {
      // Unclear if this ever happens.
      LOGFOCUS(
          ("Ignored an attempt to focus an element due to stale action id "
           "%" PRIu64 ".",
           aActionId));
      return;
    }
  }

  // If the focus actually changed, set the focus method (mouse, keyboard, etc).
  // Otherwise, just get the current focus method and use that. This ensures
  // that the method is set during the document and window focus events.
  uint32_t focusMethod = aFocusChanged
                             ? aFlags & METHODANDRING_MASK
                             : aWindow->GetFocusMethod() |
                                   (aFlags & (FLAG_SHOWRING | FLAG_NOSHOWRING));

  if (!IsWindowVisible(aWindow)) {
    // if the window isn't visible, for instance because it is a hidden tab,
    // update the current focus and scroll it into view but don't do anything
    // else
    if (FlushAndCheckIfFocusable(aElement, aFlags)) {
      aWindow->SetFocusedElement(aElement, focusMethod);
      if (aFocusChanged) {
        ScrollIntoView(presShell, aElement, aFlags);
      }
    }
    return;
  }

  Maybe<AutoRestore<RefPtr<Element>>> ar;
  if (!mFirstFocusEvent) {
    ar.emplace(mFirstFocusEvent);
    mFirstFocusEvent = aElement;
  }

  LOGCONTENT("Element %s has been focused", aElement);

  if (MOZ_LOG_TEST(gFocusLog, LogLevel::Debug)) {
    Document* docm = aWindow->GetExtantDoc();
    if (docm) {
      LOGCONTENT(" from %s", docm->GetRootElement());
    }
    LOGFOCUS(
        (" [Newdoc: %d FocusChanged: %d Raised: %d Flags: %x actionid: %" PRIu64
         "]",
         aIsNewDocument, aFocusChanged, aWindowRaised, aFlags, aActionId));
  }

  if (aIsNewDocument) {
    // if this is a new document, update the parent chain of frames so that
    // focus can be traversed from the top level down to the newly focused
    // window.
    AdjustWindowFocus(aWindow->GetBrowsingContext(), false,
                      IsWindowVisible(aWindow), aActionId);
  }

  // indicate that the window has taken focus.
  if (aWindow->TakeFocus(true, focusMethod)) {
    aIsNewDocument = true;
  }

  SetFocusedWindowInternal(aWindow, aActionId);

  if (aAdjustWidget && !sTestMode) {
    if (nsViewManager* vm = presShell->GetViewManager()) {
      nsCOMPtr<nsIWidget> widget = vm->GetRootWidget();
      if (widget)
        widget->SetFocus(nsIWidget::Raise::No, aFlags & FLAG_NONSYSTEMCALLER
                                                   ? CallerType::NonSystem
                                                   : CallerType::System);
    }
  }

  // if switching to a new document, first fire the focus event on the
  // document and then the window.
  if (aIsNewDocument) {
    Document* doc = aWindow->GetExtantDoc();
    // The focus change should be notified to IMEStateManager from here if:
    // * the focused element is in design mode or
    // * nobody gets focus and the document is in design mode
    // since any element whose uncomposed document is in design mode won't
    // receive focus event.
    if (doc && ((aElement && aElement->IsInDesignMode()) ||
                (!aElement && doc->IsInDesignMode()))) {
      IMEStateManager::OnChangeFocus(presShell->GetPresContext(), nullptr,
                                     GetFocusMoveActionCause(aFlags));
    }
    if (doc && !focusInOtherContentProcess) {
      SendFocusOrBlurEvent(eFocus, presShell, doc, ToSupports(doc),
                           aWindowRaised);
    }
    if (GetFocusedBrowsingContext() == aWindow->GetBrowsingContext() &&
        !mFocusedElement && !focusInOtherContentProcess) {
      SendFocusOrBlurEvent(eFocus, presShell, doc,
                           aWindow->GetCurrentInnerWindow(), aWindowRaised);
    }
  }

  // check to ensure that the element is still focusable, and that nothing
  // else was focused during the events above.
  if (FlushAndCheckIfFocusable(aElement, aFlags) &&
      GetFocusedBrowsingContext() == aWindow->GetBrowsingContext() &&
      mFocusedElement == nullptr) {
    mFocusedElement = aElement;

    nsIContent* focusedNode = aWindow->GetFocusedElement();
    const bool sendFocusEvent = aElement && aElement->IsInComposedDoc() &&
                                !IsNonFocusableRoot(aElement);
    const bool isRefocus = focusedNode && focusedNode == aElement;
    const bool shouldShowFocusRing =
        sendFocusEvent && ShouldMatchFocusVisible(aWindow, *aElement, aFlags);

    aWindow->SetFocusedElement(aElement, focusMethod, false);

    // if the focused element changed, scroll it into view
    if (aElement && aFocusChanged) {
      ScrollIntoView(presShell, aElement, aFlags);
    }
    nsPresContext* presContext = presShell->GetPresContext();
    if (sendFocusEvent) {
      NotifyFocusStateChange(aElement, nullptr, aFlags,
                             /* aGettingFocus = */ true, shouldShowFocusRing);

      // If this is a remote browser, focus its widget and activate remote
      // content.  Note that we might no longer be in the same document,
      // due to the events we fired above when aIsNewDocument.
      if (presShell->GetDocument() == aElement->GetComposedDoc()) {
        ActivateRemoteFrameIfNeeded(*aElement, aActionId);
      }

      IMEStateManager::OnChangeFocus(presContext, aElement,
                                     GetFocusMoveActionCause(aFlags));

      // as long as this focus wasn't because a window was raised, update the
      // commands
      // XXXndeakin P2 someone could adjust the focus during the update
      if (!aWindowRaised) {
        aWindow->UpdateCommands(u"focus"_ns, nullptr, 0);
      }

      if (!focusInOtherContentProcess) {
        SendFocusOrBlurEvent(eFocus, presShell, aElement->GetComposedDoc(),
                             aElement, aWindowRaised, isRefocus,
                             aBlurredElementInfo
                                 ? aBlurredElementInfo->mElement.get()
                                 : nullptr);
      }
    } else {
      IMEStateManager::OnChangeFocus(presContext, nullptr,
                                     GetFocusMoveActionCause(aFlags));
      if (!aWindowRaised) {
        aWindow->UpdateCommands(u"focus"_ns, nullptr, 0);
      }
    }
  } else {
    if (!mFocusedElement) {
      // When there is no focused element, IMEStateManager needs to adjust IME
      // enabled state with the document.
      nsPresContext* presContext = presShell->GetPresContext();
      IMEStateManager::OnChangeFocus(presContext, nullptr,
                                     GetFocusMoveActionCause(aFlags));
    }

    if (!aWindowRaised) {
      aWindow->UpdateCommands(u"focus"_ns, nullptr, 0);
    }
  }

  // update the caret visibility and position to match the newly focused
  // element. However, don't update the position if this was a focus due to a
  // mouse click as the selection code would already have moved the caret as
  // needed. If this is a different document than was focused before, also
  // update the caret's visibility. If this is the same document, the caret
  // visibility should be the same as before so there is no need to update it.
  if (mFocusedElement == aElement)
    UpdateCaret(aFocusChanged && !(aFlags & FLAG_BYMOUSE), aIsNewDocument,
                mFocusedElement);
}

class FocusBlurEvent : public Runnable {
 public:
  FocusBlurEvent(nsISupports* aTarget, EventMessage aEventMessage,
                 nsPresContext* aContext, bool aWindowRaised, bool aIsRefocus,
                 EventTarget* aRelatedTarget)
      : mozilla::Runnable("FocusBlurEvent"),
        mTarget(aTarget),
        mContext(aContext),
        mEventMessage(aEventMessage),
        mWindowRaised(aWindowRaised),
        mIsRefocus(aIsRefocus),
        mRelatedTarget(aRelatedTarget) {}

  NS_IMETHOD Run() override {
    InternalFocusEvent event(true, mEventMessage);
    event.mFlags.mBubbles = false;
    event.mFlags.mCancelable = false;
    event.mFromRaise = mWindowRaised;
    event.mIsRefocus = mIsRefocus;
    event.mRelatedTarget = mRelatedTarget;
    return EventDispatcher::Dispatch(mTarget, mContext, &event);
  }

  nsCOMPtr<nsISupports> mTarget;
  RefPtr<nsPresContext> mContext;
  EventMessage mEventMessage;
  bool mWindowRaised;
  bool mIsRefocus;
  nsCOMPtr<EventTarget> mRelatedTarget;
};

class FocusInOutEvent : public Runnable {
 public:
  FocusInOutEvent(nsISupports* aTarget, EventMessage aEventMessage,
                  nsPresContext* aContext,
                  nsPIDOMWindowOuter* aOriginalFocusedWindow,
                  nsIContent* aOriginalFocusedContent,
                  EventTarget* aRelatedTarget)
      : mozilla::Runnable("FocusInOutEvent"),
        mTarget(aTarget),
        mContext(aContext),
        mEventMessage(aEventMessage),
        mOriginalFocusedWindow(aOriginalFocusedWindow),
        mOriginalFocusedContent(aOriginalFocusedContent),
        mRelatedTarget(aRelatedTarget) {}

  NS_IMETHOD Run() override {
    nsCOMPtr<nsIContent> originalWindowFocus =
        mOriginalFocusedWindow ? mOriginalFocusedWindow->GetFocusedElement()
                               : nullptr;
    // Blink does not check that focus is the same after blur, but WebKit does.
    // Opt to follow Blink's behavior (see bug 687787).
    if (mEventMessage == eFocusOut ||
        originalWindowFocus == mOriginalFocusedContent) {
      InternalFocusEvent event(true, mEventMessage);
      event.mFlags.mBubbles = true;
      event.mFlags.mCancelable = false;
      event.mRelatedTarget = mRelatedTarget;
      return EventDispatcher::Dispatch(mTarget, mContext, &event);
    }
    return NS_OK;
  }

  nsCOMPtr<nsISupports> mTarget;
  RefPtr<nsPresContext> mContext;
  EventMessage mEventMessage;
  nsCOMPtr<nsPIDOMWindowOuter> mOriginalFocusedWindow;
  nsCOMPtr<nsIContent> mOriginalFocusedContent;
  nsCOMPtr<EventTarget> mRelatedTarget;
};

static Document* GetDocumentHelper(EventTarget* aTarget) {
  if (!aTarget) {
    return nullptr;
  }
  if (const nsINode* node = nsINode::FromEventTarget(aTarget)) {
    return node->OwnerDoc();
  }
  nsPIDOMWindowInner* win = nsPIDOMWindowInner::FromEventTarget(aTarget);
  return win ? win->GetExtantDoc() : nullptr;
}

void nsFocusManager::FireFocusInOrOutEvent(
    EventMessage aEventMessage, PresShell* aPresShell, nsISupports* aTarget,
    nsPIDOMWindowOuter* aCurrentFocusedWindow,
    nsIContent* aCurrentFocusedContent, EventTarget* aRelatedTarget) {
  NS_ASSERTION(aEventMessage == eFocusIn || aEventMessage == eFocusOut,
               "Wrong event type for FireFocusInOrOutEvent");

  nsContentUtils::AddScriptRunner(new FocusInOutEvent(
      aTarget, aEventMessage, aPresShell->GetPresContext(),
      aCurrentFocusedWindow, aCurrentFocusedContent, aRelatedTarget));
}

void nsFocusManager::SendFocusOrBlurEvent(EventMessage aEventMessage,
                                          PresShell* aPresShell,
                                          Document* aDocument,
                                          nsISupports* aTarget,
                                          bool aWindowRaised, bool aIsRefocus,
                                          EventTarget* aRelatedTarget) {
  NS_ASSERTION(aEventMessage == eFocus || aEventMessage == eBlur,
               "Wrong event type for SendFocusOrBlurEvent");

  nsCOMPtr<EventTarget> eventTarget = do_QueryInterface(aTarget);
  nsCOMPtr<Document> eventTargetDoc = GetDocumentHelper(eventTarget);
  nsCOMPtr<Document> relatedTargetDoc = GetDocumentHelper(aRelatedTarget);

  // set aRelatedTarget to null if it's not in the same document as eventTarget
  if (eventTargetDoc != relatedTargetDoc) {
    aRelatedTarget = nullptr;
  }

  if (aDocument && aDocument->EventHandlingSuppressed()) {
    // if this event was already queued, remove it and append it to the end
    mDelayedBlurFocusEvents.RemoveElementsBy([&](const auto& event) {
      return event.mEventMessage == aEventMessage &&
             event.mPresShell == aPresShell && event.mDocument == aDocument &&
             event.mTarget == eventTarget &&
             event.mRelatedTarget == aRelatedTarget;
    });

    mDelayedBlurFocusEvents.EmplaceBack(aEventMessage, aPresShell, aDocument,
                                        eventTarget, aRelatedTarget);
    return;
  }

  // If mDelayedBlurFocusEvents queue is not empty, check if there are events
  // that belongs to this doc, if yes, fire them first.
  if (aDocument && !aDocument->EventHandlingSuppressed() &&
      mDelayedBlurFocusEvents.Length()) {
    FireDelayedEvents(aDocument);
  }

  FireFocusOrBlurEvent(aEventMessage, aPresShell, aTarget, aWindowRaised,
                       aIsRefocus, aRelatedTarget);
}

void nsFocusManager::FireFocusOrBlurEvent(EventMessage aEventMessage,
                                          PresShell* aPresShell,
                                          nsISupports* aTarget,
                                          bool aWindowRaised, bool aIsRefocus,
                                          EventTarget* aRelatedTarget) {
  nsCOMPtr<nsPIDOMWindowOuter> currentWindow = mFocusedWindow;
  nsCOMPtr<nsPIDOMWindowInner> targetWindow = do_QueryInterface(aTarget);
  nsCOMPtr<Document> targetDocument = do_QueryInterface(aTarget);
  nsCOMPtr<nsIContent> currentFocusedContent =
      currentWindow ? currentWindow->GetFocusedElement() : nullptr;

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

  nsContentUtils::AddScriptRunner(
      new FocusBlurEvent(aTarget, aEventMessage, aPresShell->GetPresContext(),
                         aWindowRaised, aIsRefocus, aRelatedTarget));

  // Check that the target is not a window or document before firing
  // focusin/focusout. Other browsers do not fire focusin/focusout on window,
  // despite being required in the spec, so follow their behavior.
  //
  // As for document, we should not even fire focus/blur, but until then, we
  // need this check. targetDocument should be removed once bug 1228802 is
  // resolved.
  if (!targetWindow && !targetDocument) {
    EventMessage focusInOrOutMessage =
        aEventMessage == eFocus ? eFocusIn : eFocusOut;
    FireFocusInOrOutEvent(focusInOrOutMessage, aPresShell, aTarget,
                          currentWindow, currentFocusedContent, aRelatedTarget);
  }
}

void nsFocusManager::ScrollIntoView(PresShell* aPresShell, nsIContent* aContent,
                                    uint32_t aFlags) {
  if (aFlags & FLAG_NOSCROLL) {
    return;
  }
  // If the noscroll flag isn't set, scroll the newly focused element into view.
  aPresShell->ScrollContentIntoView(
      aContent, ScrollAxis(kScrollMinimum, WhenToScroll::IfNotVisible),
      ScrollAxis(kScrollMinimum, WhenToScroll::IfNotVisible),
      ScrollFlags::ScrollOverflowHidden);
  // Scroll the input / textarea selection into view, unless focused with the
  // mouse, see bug 572649.
  if (aFlags & FLAG_BYMOUSE) {
    return;
  }
  // ScrollContentIntoView flushes layout, so no need to flush again here.
  if (nsTextControlFrame* tf = do_QueryFrame(aContent->GetPrimaryFrame())) {
    tf->ScrollSelectionIntoViewAsync(nsTextControlFrame::ScrollAncestors::Yes);
  }
}

void nsFocusManager::RaiseWindow(nsPIDOMWindowOuter* aWindow,
                                 CallerType aCallerType, uint64_t aActionId) {
  // don't raise windows that are already raised or are in the process of
  // being lowered

  if (!aWindow || aWindow == mWindowBeingLowered) {
    return;
  }

  if (XRE_IsParentProcess()) {
    if (aWindow == mActiveWindow) {
      return;
    }
  } else {
    BrowsingContext* bc = aWindow->GetBrowsingContext();
    // TODO: Deeper OOP frame hierarchies are
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1661227
    if (bc == GetActiveBrowsingContext()) {
      return;
    }
    if (bc == GetFocusedBrowsingContext()) {
      return;
    }
  }

  if (sTestMode) {
    // In test mode, emulate raising the window. WindowRaised takes
    // care of lowering the present active window. This happens in
    // a separate runnable to avoid touching multiple windows in
    // the current runnable.

    nsCOMPtr<nsPIDOMWindowOuter> window(aWindow);
    RefPtr<nsFocusManager> self(this);
    NS_DispatchToCurrentThread(NS_NewRunnableFunction(
        "nsFocusManager::RaiseWindow", [self, window]() -> void {
          self->WindowRaised(window, GenerateFocusActionId());
        }));
    return;
  }

  if (XRE_IsContentProcess()) {
    BrowsingContext* bc = aWindow->GetBrowsingContext();
    if (!bc->IsTop()) {
      // Assume the raise below will succeed and run the raising synchronously
      // in this process to make the focus event that is observable in this
      // process fire in the right order relative to mouseup when we are here
      // thanks to a mousedown.
      WindowRaised(aWindow, aActionId);
    }
  }

#if defined(XP_WIN)
  // Windows would rather we focus the child widget, otherwise, the toplevel
  // widget will always end up being focused. Fortunately, focusing the child
  // widget will also have the effect of raising the window this widget is in.
  // But on other platforms, we can just focus the toplevel widget to raise
  // the window.
  nsCOMPtr<nsPIDOMWindowOuter> childWindow;
  GetFocusedDescendant(aWindow, eIncludeAllDescendants,
                       getter_AddRefs(childWindow));
  if (!childWindow) {
    childWindow = aWindow;
  }

  nsCOMPtr<nsIDocShell> docShell = aWindow->GetDocShell();
  if (!docShell) {
    return;
  }

  PresShell* presShell = docShell->GetPresShell();
  if (!presShell) {
    return;
  }

  if (nsViewManager* vm = presShell->GetViewManager()) {
    nsCOMPtr<nsIWidget> widget = vm->GetRootWidget();
    if (widget) {
      widget->SetFocus(nsIWidget::Raise::Yes, aCallerType);
    }
  }
#else
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin =
      do_QueryInterface(aWindow->GetDocShell());
  if (treeOwnerAsWin) {
    nsCOMPtr<nsIWidget> widget;
    treeOwnerAsWin->GetMainWidget(getter_AddRefs(widget));
    if (widget) {
      widget->SetFocus(nsIWidget::Raise::Yes, aCallerType);
    }
  }
#endif
}

void nsFocusManager::UpdateCaretForCaretBrowsingMode() {
  UpdateCaret(false, true, mFocusedElement);
}

void nsFocusManager::UpdateCaret(bool aMoveCaretToFocus, bool aUpdateVisibility,
                                 nsIContent* aContent) {
  LOGFOCUS(("Update Caret: %d %d", aMoveCaretToFocus, aUpdateVisibility));

  if (!mFocusedWindow) {
    return;
  }

  // this is called when a document is focused or when the caretbrowsing
  // preference is changed
  nsCOMPtr<nsIDocShell> focusedDocShell = mFocusedWindow->GetDocShell();
  if (!focusedDocShell) {
    return;
  }

  if (focusedDocShell->ItemType() == nsIDocShellTreeItem::typeChrome) {
    return;  // Never browse with caret in chrome
  }

  bool browseWithCaret = Preferences::GetBool("accessibility.browsewithcaret");

  RefPtr<PresShell> presShell = focusedDocShell->GetPresShell();
  if (!presShell) {
    return;
  }

  // If this is an editable document which isn't contentEditable, or a
  // contentEditable document and the node to focus is contentEditable,
  // return, so that we don't mess with caret visibility.
  bool isEditable = false;
  focusedDocShell->GetEditable(&isEditable);

  if (isEditable) {
    Document* doc = presShell->GetDocument();

    bool isContentEditableDoc =
        doc &&
        doc->GetEditingState() == Document::EditingState::eContentEditable;

    bool isFocusEditable = aContent && aContent->HasFlag(NODE_IS_EDITABLE);
    if (!isContentEditableDoc || isFocusEditable) {
      return;
    }
  }

  if (!isEditable && aMoveCaretToFocus) {
    MoveCaretToFocus(presShell, aContent);
  }

  // The above MoveCaretToFocus call may run scripts which
  // may clear mFocusWindow
  if (!mFocusedWindow) {
    return;
  }

  if (!aUpdateVisibility) {
    return;
  }

  // XXXndeakin this doesn't seem right. It should be checking for this only
  // on the nearest ancestor frame which is a chrome frame. But this is
  // what the existing code does, so just leave it for now.
  if (!browseWithCaret) {
    nsCOMPtr<Element> docElement = mFocusedWindow->GetFrameElementInternal();
    if (docElement)
      browseWithCaret = docElement->AttrValueIs(
          kNameSpaceID_None, nsGkAtoms::showcaret, u"true"_ns, eCaseMatters);
  }

  SetCaretVisible(presShell, browseWithCaret, aContent);
}

void nsFocusManager::MoveCaretToFocus(PresShell* aPresShell,
                                      nsIContent* aContent) {
  nsCOMPtr<Document> doc = aPresShell->GetDocument();
  if (doc) {
    RefPtr<nsFrameSelection> frameSelection = aPresShell->FrameSelection();
    RefPtr<Selection> domSelection =
        frameSelection->GetSelection(SelectionType::eNormal);
    if (domSelection) {
      // First clear the selection. This way, if there is no currently focused
      // content, the selection will just be cleared.
      domSelection->RemoveAllRanges(IgnoreErrors());
      if (aContent) {
        ErrorResult rv;
        RefPtr<nsRange> newRange = doc->CreateRange(rv);
        if (NS_WARN_IF(rv.Failed())) {
          rv.SuppressException();
          return;
        }

        // Set the range to the start of the currently focused node
        // Make sure it's collapsed
        newRange->SelectNodeContents(*aContent, IgnoreErrors());

        if (!aContent->GetFirstChild() ||
            aContent->IsNodeOfType(nsINode::eHTML_FORM_CONTROL)) {
          // If current focus node is a leaf, set range to before the
          // node by using the parent as a container.
          // This prevents it from appearing as selected.
          newRange->SetStartBefore(*aContent, IgnoreErrors());
          newRange->SetEndBefore(*aContent, IgnoreErrors());
        }
        domSelection->AddRangeAndSelectFramesAndNotifyListeners(*newRange,
                                                                IgnoreErrors());
        domSelection->CollapseToStart(IgnoreErrors());
      }
    }
  }
}

nsresult nsFocusManager::SetCaretVisible(PresShell* aPresShell, bool aVisible,
                                         nsIContent* aContent) {
  // When browsing with caret, make sure caret is visible after new focus
  // Return early if there is no caret. This can happen for the testcase
  // for bug 308025 where a window is closed in a blur handler.
  RefPtr<nsCaret> caret = aPresShell->GetCaret();
  if (!caret) {
    return NS_OK;
  }

  bool caretVisible = caret->IsVisible();
  if (!aVisible && !caretVisible) {
    return NS_OK;
  }

  RefPtr<nsFrameSelection> frameSelection;
  if (aContent) {
    NS_ASSERTION(aContent->GetComposedDoc() == aPresShell->GetDocument(),
                 "Wrong document?");
    nsIFrame* focusFrame = aContent->GetPrimaryFrame();
    if (focusFrame) {
      frameSelection = focusFrame->GetFrameSelection();
    }
  }

  RefPtr<nsFrameSelection> docFrameSelection = aPresShell->FrameSelection();

  if (docFrameSelection && caret &&
      (frameSelection == docFrameSelection || !aContent)) {
    Selection* domSelection =
        docFrameSelection->GetSelection(SelectionType::eNormal);
    if (domSelection) {
      // First, hide the caret to prevent attempting to show it in
      // SetCaretDOMSelection
      aPresShell->SetCaretEnabled(false);

      // Caret must blink on non-editable elements
      caret->SetIgnoreUserModify(true);
      // Tell the caret which selection to use
      caret->SetSelection(domSelection);

      // In content, we need to set the caret. The only special case is edit
      // fields, which have a different frame selection from the document.
      // They will take care of making the caret visible themselves.

      aPresShell->SetCaretReadOnly(false);
      aPresShell->SetCaretEnabled(aVisible);
    }
  }

  return NS_OK;
}

nsresult nsFocusManager::GetSelectionLocation(Document* aDocument,
                                              PresShell* aPresShell,
                                              nsIContent** aStartContent,
                                              nsIContent** aEndContent) {
  *aStartContent = *aEndContent = nullptr;
  nsPresContext* presContext = aPresShell->GetPresContext();
  NS_ASSERTION(presContext, "mPresContent is null!!");

  RefPtr<nsFrameSelection> frameSelection = aPresShell->FrameSelection();

  RefPtr<Selection> domSelection;
  if (frameSelection) {
    domSelection = frameSelection->GetSelection(SelectionType::eNormal);
  }

  bool isCollapsed = false;
  nsCOMPtr<nsIContent> startContent, endContent;
  uint32_t startOffset = 0;
  if (domSelection) {
    isCollapsed = domSelection->IsCollapsed();
    RefPtr<const nsRange> domRange = domSelection->GetRangeAt(0);
    if (domRange) {
      nsCOMPtr<nsINode> startNode = domRange->GetStartContainer();
      nsCOMPtr<nsINode> endNode = domRange->GetEndContainer();
      startOffset = domRange->StartOffset();

      nsIContent* childContent = nullptr;

      startContent = do_QueryInterface(startNode);
      if (startContent && startContent->IsElement()) {
        childContent = startContent->GetChildAt_Deprecated(startOffset);
        if (childContent) {
          startContent = childContent;
        }
      }

      endContent = do_QueryInterface(endNode);
      if (endContent && endContent->IsElement()) {
        uint32_t endOffset = domRange->EndOffset();
        childContent = endContent->GetChildAt_Deprecated(endOffset);
        if (childContent) {
          endContent = childContent;
        }
      }
    }
  } else {
    return NS_ERROR_INVALID_ARG;
  }

  nsIFrame* startFrame = nullptr;
  if (startContent) {
    startFrame = startContent->GetPrimaryFrame();
    if (isCollapsed) {
      // Next check to see if our caret is at the very end of a node
      // If so, the caret is actually sitting in front of the next
      // logical frame's primary node - so for this case we need to
      // change caretContent to that node.

      if (startContent->NodeType() == nsINode::TEXT_NODE) {
        nsAutoString nodeValue;
        startContent->GetAsText()->AppendTextTo(nodeValue);

        bool isFormControl =
            startContent->IsNodeOfType(nsINode::eHTML_FORM_CONTROL);

        if (nodeValue.Length() == startOffset && !isFormControl &&
            startContent != aDocument->GetRootElement()) {
          // Yes, indeed we were at the end of the last node
          nsCOMPtr<nsIFrameEnumerator> frameTraversal;
          nsresult rv = NS_NewFrameTraversal(getter_AddRefs(frameTraversal),
                                             presContext, startFrame, eLeaf,
                                             false,  // aVisual
                                             false,  // aLockInScrollView
                                             true,   // aFollowOOFs
                                             false   // aSkipPopupChecks
          );
          NS_ENSURE_SUCCESS(rv, rv);

          nsIFrame* newCaretFrame = nullptr;
          nsCOMPtr<nsIContent> newCaretContent = startContent;
          bool endOfSelectionInStartNode(startContent == endContent);
          do {
            // Continue getting the next frame until the primary content for the
            // frame we are on changes - we don't want to be stuck in the same
            // place
            frameTraversal->Next();
            newCaretFrame =
                static_cast<nsIFrame*>(frameTraversal->CurrentItem());
            if (nullptr == newCaretFrame) break;
            newCaretContent = newCaretFrame->GetContent();
          } while (!newCaretContent || newCaretContent == startContent);

          if (newCaretFrame && newCaretContent) {
            // If the caret is exactly at the same position of the new frame,
            // then we can use the newCaretFrame and newCaretContent for our
            // position
            nsRect caretRect;
            nsIFrame* frame = nsCaret::GetGeometry(domSelection, &caretRect);
            if (frame) {
              nsPoint caretWidgetOffset;
              nsIWidget* widget = frame->GetNearestWidget(caretWidgetOffset);
              caretRect.MoveBy(caretWidgetOffset);
              nsPoint newCaretOffset;
              nsIWidget* newCaretWidget =
                  newCaretFrame->GetNearestWidget(newCaretOffset);
              if (widget == newCaretWidget && caretRect.y == newCaretOffset.y &&
                  caretRect.x == newCaretOffset.x) {
                // The caret is at the start of the new element.
                startFrame = newCaretFrame;
                startContent = newCaretContent;
                if (endOfSelectionInStartNode) {
                  endContent = newCaretContent;  // Ensure end of selection is
                                                 // not before start
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

  return NS_OK;
}

nsresult nsFocusManager::DetermineElementToMoveFocus(
    nsPIDOMWindowOuter* aWindow, nsIContent* aStartContent, int32_t aType,
    bool aNoParentTraversal, bool aNavigateByKey, nsIContent** aNextContent) {
  *aNextContent = nullptr;

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
      startContent = GetFocusedDescendant(aWindow, eIncludeAllDescendants,
                                          getter_AddRefs(focusedWindow));
    } else if (aType != MOVEFOCUS_LASTDOC) {
      // Otherwise, start at the focused node. If MOVEFOCUS_LASTDOC is used,
      // then we are document-navigating backwards from chrome to the content
      // process, and we don't want to use this so that we start from the end
      // of the document.
      startContent = aWindow->GetFocusedElement();
    }
  }

  nsCOMPtr<Document> doc;
  if (startContent)
    doc = startContent->GetComposedDoc();
  else
    doc = aWindow->GetExtantDoc();
  if (!doc) return NS_OK;

  LookAndFeel::GetInt(LookAndFeel::IntID::TabFocusModel,
                      &nsIContent::sTabFocusModel);

  // True if we are navigating by document (F6/Shift+F6) or false if we are
  // navigating by element (Tab/Shift+Tab).
  const bool forDocumentNavigation =
      aType == MOVEFOCUS_FORWARDDOC || aType == MOVEFOCUS_BACKWARDDOC ||
      aType == MOVEFOCUS_FIRSTDOC || aType == MOVEFOCUS_LASTDOC;

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

  Element* rootContent = doc->GetRootElement();
  NS_ENSURE_TRUE(rootContent, NS_OK);

  PresShell* presShell = doc->GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_OK);

  if (aType == MOVEFOCUS_FIRST) {
    if (!aStartContent) {
      startContent = rootContent;
    }
    return GetNextTabbableContent(presShell, startContent, nullptr,
                                  startContent, true, 1, false, false,
                                  aNavigateByKey, aNextContent);
  }
  if (aType == MOVEFOCUS_LAST) {
    if (!aStartContent) {
      startContent = rootContent;
    }
    return GetNextTabbableContent(presShell, startContent, nullptr,
                                  startContent, false, 0, false, false,
                                  aNavigateByKey, aNextContent);
  }

  bool forward = (aType == MOVEFOCUS_FORWARD || aType == MOVEFOCUS_FORWARDDOC ||
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
    if (startContent->IsHTMLElement(nsGkAtoms::area)) {
      startContent->IsFocusable(&tabIndex);
    } else if (frame) {
      tabIndex = frame->IsFocusable().mTabIndex;
    } else {
      startContent->IsFocusable(&tabIndex);
    }

    // if the current element isn't tabbable, ignore the tabindex and just
    // look for the next element. The root content won't have a tabindex
    // so just treat this as the beginning of the tab order.
    if (tabIndex < 0) {
      tabIndex = 1;
      if (startContent != rootContent) {
        ignoreTabIndex = true;
      }
    }

    // check if the focus is currently inside a popup. Elements such as the
    // autocomplete widget use the noautofocus attribute to allow the focus to
    // remain outside the popup when it is opened.
    if (frame) {
      popupFrame = nsLayoutUtils::GetClosestFrameOfType(
          frame, LayoutFrameType::MenuPopup);
    }

    if (popupFrame && !forDocumentNavigation) {
      // Don't navigate outside of a popup, so pretend that the
      // root content is the popup itself
      rootContent = popupFrame->GetContent()->AsElement();
      NS_ASSERTION(rootContent, "Popup frame doesn't have a content node");
    } else if (!forward) {
      // If focus moves backward and when current focused node is root
      // content or <body> element which is editable by contenteditable
      // attribute, focus should move to its parent document.
      if (startContent == rootContent) {
        doNavigation = false;
      } else {
        Document* doc = startContent->GetComposedDoc();
        if (startContent ==
            nsLayoutUtils::GetEditableRootContentByContentEditable(doc)) {
          doNavigation = false;
        }
      }
    }
  } else {
#ifdef MOZ_XUL
    if (aType != MOVEFOCUS_CARET) {
      // if there is no focus, yet a panel is open, focus the first item in
      // the panel
      nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
      if (pm) {
        popupFrame = pm->GetTopPopup(ePopupTypePanel);
      }
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
        rootContent = startContent->AsElement();
      }

      doc = startContent ? startContent->GetComposedDoc() : nullptr;
    } else {
      // Otherwise, for content shells, start from the location of the caret.
      nsCOMPtr<nsIDocShell> docShell = aWindow->GetDocShell();
      if (docShell && docShell->ItemType() != nsIDocShellTreeItem::typeChrome) {
        nsCOMPtr<nsIContent> endSelectionContent;
        GetSelectionLocation(doc, presShell, getter_AddRefs(startContent),
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
            GetFocusInSelection(aWindow, startContent, endSelectionContent,
                                aNextContent);
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
  if (forDocumentNavigation && nsContentUtils::IsChromeDoc(doc)) {
    nsAutoString retarget;

    if (rootContent->GetAttr(kNameSpaceID_None,
                             nsGkAtoms::retargetdocumentfocus, retarget)) {
      nsIContent* retargetElement = doc->GetElementById(retarget);
      // The common case here is the urlbar where focus is on the anonymous
      // input inside the textbox, but the retargetdocumentfocus attribute
      // refers to the textbox. The Contains check will return false and the
      // IsInclusiveDescendantOf check will return true in this case.
      if (retargetElement &&
          (retargetElement == startContent ||
           (!retargetElement->Contains(startContent) &&
            startContent->IsInclusiveDescendantOf(retargetElement)))) {
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
                      forward, tabIndex, ignoreTabIndex,
                      forDocumentNavigation));

  while (doc) {
    if (doNavigation) {
      nsCOMPtr<nsIContent> nextFocus;
      nsresult rv = GetNextTabbableContent(
          presShell, rootContent,
          skipOriginalContentCheck ? nullptr : originalStartContent,
          startContent, forward, tabIndex, ignoreTabIndex,
          forDocumentNavigation, aNavigateByKey, getter_AddRefs(nextFocus));
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
      if (startContent == rootContent) {
        return NS_OK;
      }

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
      presShell = doc->GetPresShell();

      // We can focus the root element now that we have moved to another
      // document.
      mayFocusRoot = true;

      nsIFrame* frame = startContent->GetPrimaryFrame();
      if (!frame) {
        return NS_OK;
      }

      tabIndex = frame->IsFocusable().mTabIndex;
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
        popupFrame = nsLayoutUtils::GetClosestFrameOfType(
            frame, LayoutFrameType::MenuPopup);
        if (popupFrame) {
          rootContent = popupFrame->GetContent()->AsElement();
          NS_ASSERTION(rootContent, "Popup frame doesn't have a content node");
        }
      }
    } else {
      if (aNavigateByKey) {
        // There is no parent, so call the tree owner. This will tell the
        // embedder or parent process that it should take the focus.
        bool tookFocus;
        docShell->TabToTreeOwner(forward, forDocumentNavigation, &tookFocus);
        // If the tree owner took the focus, blur the current element.
        if (tookFocus) {
          if (GetFocusedBrowsingContext() &&
              GetFocusedBrowsingContext()->IsInProcess()) {
            Blur(GetFocusedBrowsingContext(), nullptr, true, true,
                 GenerateFocusActionId());
          } else {
            nsCOMPtr<nsPIDOMWindowOuter> window = docShell->GetWindow();
            window->SetFocusedElement(nullptr);
          }
          return NS_OK;
        }
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
        Element* root = GetRootForFocus(piWindow, doc, true, true);
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
    if (startContent == originalStartContent) {
      break;
    }
  }

  return NS_OK;
}

uint32_t nsFocusManager::ProgrammaticFocusFlags(const FocusOptions& aOptions) {
  uint32_t flags = FLAG_BYJS;
  if (aOptions.mPreventScroll) {
    flags |= FLAG_NOSCROLL;
  }
  if (aOptions.mPreventFocusRing) {
    flags |= FLAG_NOSHOWRING;
  }
  if (UserActivation::IsHandlingKeyboardInput()) {
    flags |= FLAG_BYKEY;
  }
  // TODO: We could do a similar thing if we're handling mouse input, but that
  // changes focusability of some elements so may be more risky.
  return flags;
}

static bool IsHostOrSlot(const nsIContent* aContent) {
  return aContent && (aContent->GetShadowRoot() ||
                      aContent->IsHTMLElement(nsGkAtoms::slot));
}

// Helper class to iterate contents in scope by traversing flattened tree
// in tree order
class MOZ_STACK_CLASS ScopedContentTraversal {
 public:
  ScopedContentTraversal(nsIContent* aStartContent, nsIContent* aOwner)
      : mCurrent(aStartContent), mOwner(aOwner) {
    MOZ_ASSERT(aStartContent);
  }

  void Next();
  void Prev();

  void Reset() { SetCurrent(mOwner); }

  nsIContent* GetCurrent() const { return mCurrent; }

 private:
  void SetCurrent(nsIContent* aContent) { mCurrent = aContent; }

  nsIContent* mCurrent;
  nsIContent* mOwner;
};

void ScopedContentTraversal::Next() {
  MOZ_ASSERT(mCurrent);

  // Get mCurrent's first child if it's in the same scope.
  if (!IsHostOrSlot(mCurrent) || mCurrent == mOwner) {
    StyleChildrenIterator iter(mCurrent);
    nsIContent* child = iter.GetNextChild();
    if (child) {
      SetCurrent(child);
      return;
    }
  }

  // If mOwner has no children, END traversal
  if (mCurrent == mOwner) {
    SetCurrent(nullptr);
    return;
  }

  nsIContent* current = mCurrent;
  while (1) {
    // Create parent's iterator and move to current
    nsIContent* parent = current->GetFlattenedTreeParent();
    StyleChildrenIterator parentIter(parent);
    parentIter.Seek(current);

    // Get next sibling of current
    if (nsIContent* next = parentIter.GetNextChild()) {
      SetCurrent(next);
      return;
    }

    // If no next sibling and parent is mOwner, END traversal
    if (parent == mOwner) {
      SetCurrent(nullptr);
      return;
    }

    current = parent;
  }
}

void ScopedContentTraversal::Prev() {
  MOZ_ASSERT(mCurrent);

  nsIContent* parent;
  nsIContent* last;
  if (mCurrent == mOwner) {
    // Get last child of mOwner
    StyleChildrenIterator ownerIter(mOwner, false /* aStartAtBeginning */);
    last = ownerIter.GetPreviousChild();

    parent = last;
  } else {
    // Create parent's iterator and move to mCurrent
    parent = mCurrent->GetFlattenedTreeParent();
    StyleChildrenIterator parentIter(parent);
    parentIter.Seek(mCurrent);

    // Get previous sibling
    last = parentIter.GetPreviousChild();
  }

  while (last) {
    parent = last;
    if (IsHostOrSlot(parent)) {
      // Skip contents in other scopes
      break;
    }

    // Find last child
    StyleChildrenIterator iter(parent, false /* aStartAtBeginning */);
    last = iter.GetPreviousChild();
  }

  // If parent is mOwner and no previous sibling remains, END traversal
  SetCurrent(parent == mOwner ? nullptr : parent);
}

/**
 * Returns scope owner of aContent.
 * A scope owner is either a shadow host, or slot.
 */
static nsIContent* FindScopeOwner(nsIContent* aContent) {
  nsIContent* currentContent = aContent;
  while (currentContent) {
    nsIContent* parent = currentContent->GetFlattenedTreeParent();

    // Shadow host / Slot
    if (IsHostOrSlot(parent)) {
      return parent;
    }

    currentContent = parent;
  }

  return nullptr;
}

/**
 * Host and Slot elements need to be handled as if they had tabindex 0 even
 * when they don't have the attribute. This is a helper method to get the
 * right value for focus navigation. If aIsFocusable is passed, it is set to
 * true if the element itself is focusable.
 */
static int32_t HostOrSlotTabIndexValue(const nsIContent* aContent,
                                       bool* aIsFocusable = nullptr) {
  MOZ_ASSERT(IsHostOrSlot(aContent));

  if (aIsFocusable) {
    nsIFrame* frame = aContent->GetPrimaryFrame();
    *aIsFocusable = frame && frame->IsFocusable().mTabIndex >= 0;
  }

  const nsAttrValue* attrVal =
      aContent->AsElement()->GetParsedAttr(nsGkAtoms::tabindex);
  if (!attrVal) {
    return 0;
  }

  if (attrVal->Type() == nsAttrValue::eInteger) {
    return attrVal->GetIntegerValue();
  }

  return -1;
}

nsIContent* nsFocusManager::GetNextTabbableContentInScope(
    nsIContent* aOwner, nsIContent* aStartContent,
    nsIContent* aOriginalStartContent, bool aForward, int32_t aCurrentTabIndex,
    bool aIgnoreTabIndex, bool aForDocumentNavigation, bool aNavigateByKey,
    bool aSkipOwner) {
  MOZ_ASSERT(IsHostOrSlot(aOwner), "Scope owner should be host or slot");

  if (!aSkipOwner && (aForward && aOwner == aStartContent)) {
    if (nsIFrame* frame = aOwner->GetPrimaryFrame()) {
      auto focusable = frame->IsFocusable();
      if (focusable && focusable.mTabIndex >= 0) {
        return aOwner;
      }
    }
  }

  //
  // Iterate contents in scope
  //
  ScopedContentTraversal contentTraversal(aStartContent, aOwner);
  nsCOMPtr<nsIContent> iterContent;
  nsIContent* firstNonChromeOnly =
      aStartContent->IsInNativeAnonymousSubtree()
          ? aStartContent->FindFirstNonChromeOnlyAccessContent()
          : nullptr;
  while (1) {
    // Iterate tab index to find corresponding contents in scope

    while (1) {
      // Iterate remaining contents in scope to find next content to focus

      // Get next content
      aForward ? contentTraversal.Next() : contentTraversal.Prev();
      iterContent = contentTraversal.GetCurrent();

      if (firstNonChromeOnly && firstNonChromeOnly == iterContent) {
        // We just broke out from the native anonymous content, so move
        // to the previous/next node of the native anonymous owner.
        if (aForward) {
          contentTraversal.Next();
        } else {
          contentTraversal.Prev();
        }
        iterContent = contentTraversal.GetCurrent();
      }
      if (!iterContent) {
        // Reach the end
        break;
      }

      int32_t tabIndex = 0;
      if (iterContent->IsInNativeAnonymousSubtree() &&
          iterContent->GetPrimaryFrame()) {
        tabIndex = iterContent->GetPrimaryFrame()->IsFocusable().mTabIndex;
      } else if (IsHostOrSlot(iterContent)) {
        tabIndex = HostOrSlotTabIndexValue(iterContent);
      } else {
        nsIFrame* frame = iterContent->GetPrimaryFrame();
        if (!frame) {
          continue;
        }
        tabIndex = frame->IsFocusable().mTabIndex;
      }
      if (tabIndex < 0 || !(aIgnoreTabIndex || tabIndex == aCurrentTabIndex)) {
        continue;
      }

      if (!IsHostOrSlot(iterContent)) {
        nsCOMPtr<nsIContent> elementInFrame;
        bool checkSubDocument = true;
        if (aForDocumentNavigation &&
            TryDocumentNavigation(iterContent, &checkSubDocument,
                                  getter_AddRefs(elementInFrame))) {
          return elementInFrame;
        }
        if (!checkSubDocument) {
          continue;
        }

        if (TryToMoveFocusToSubDocument(iterContent, aOriginalStartContent,
                                        aForward, aForDocumentNavigation,
                                        aNavigateByKey,
                                        getter_AddRefs(elementInFrame))) {
          return elementInFrame;
        }

        // Found content to focus
        return iterContent;
      }

      // Search in scope owned by iterContent
      nsIContent* contentToFocus = GetNextTabbableContentInScope(
          iterContent, iterContent, aOriginalStartContent, aForward,
          aForward ? 1 : 0, aIgnoreTabIndex, aForDocumentNavigation,
          aNavigateByKey, false /* aSkipOwner */);
      if (contentToFocus) {
        return contentToFocus;
      }
    };

    // If already at lowest priority tab (0), end search completely.
    // A bit counterintuitive but true, tabindex order goes 1, 2, ... 32767, 0
    if (aCurrentTabIndex == (aForward ? 0 : 1)) {
      break;
    }

    // We've been just trying to find some focusable element, and haven't, so
    // bail out.
    if (aIgnoreTabIndex) {
      break;
    }

    // Continue looking for next highest priority tabindex
    aCurrentTabIndex = GetNextTabIndex(aOwner, aCurrentTabIndex, aForward);
    contentTraversal.Reset();
  }

  // Return scope owner at last for backward navigation if its tabindex
  // is non-negative
  if (!aSkipOwner && !aForward) {
    if (nsIFrame* frame = aOwner->GetPrimaryFrame()) {
      auto focusable = frame->IsFocusable();
      if (focusable && focusable.mTabIndex >= 0) {
        return aOwner;
      }
    }
  }

  return nullptr;
}

nsIContent* nsFocusManager::GetNextTabbableContentInAncestorScopes(
    nsIContent* aStartOwner, nsIContent** aStartContent,
    nsIContent* aOriginalStartContent, bool aForward, int32_t* aCurrentTabIndex,
    bool aIgnoreTabIndex, bool aForDocumentNavigation, bool aNavigateByKey) {
  MOZ_ASSERT(aStartOwner == FindScopeOwner(*aStartContent),
             "aStartOWner should be the scope owner of aStartContent");
  MOZ_ASSERT(IsHostOrSlot(aStartOwner), "scope owner should be host or slot");

  nsIContent* owner = aStartOwner;
  nsIContent* startContent = *aStartContent;
  while (IsHostOrSlot(owner)) {
    int32_t tabIndex = 0;
    if (IsHostOrSlot(startContent)) {
      tabIndex = HostOrSlotTabIndexValue(startContent);
    } else if (nsIFrame* frame = startContent->GetPrimaryFrame()) {
      tabIndex = frame->IsFocusable().mTabIndex;
    } else {
      startContent->IsFocusable(&tabIndex);
    }
    nsIContent* contentToFocus = GetNextTabbableContentInScope(
        owner, startContent, aOriginalStartContent, aForward, tabIndex,
        aIgnoreTabIndex, aForDocumentNavigation, aNavigateByKey,
        false /* aSkipOwner */);
    if (contentToFocus) {
      return contentToFocus;
    }

    startContent = owner;
    owner = FindScopeOwner(startContent);
  }

  // If not found in shadow DOM, search from the top level shadow host in light
  // DOM
  *aStartContent = startContent;
  *aCurrentTabIndex = HostOrSlotTabIndexValue(startContent);

  return nullptr;
}

static nsIContent* GetTopLevelScopeOwner(nsIContent* aContent) {
  nsIContent* topLevelScopeOwner = nullptr;
  while (aContent) {
    if (HTMLSlotElement* slot = aContent->GetAssignedSlot()) {
      aContent = slot;
      topLevelScopeOwner = aContent;
    } else if (ShadowRoot* shadowRoot = aContent->GetContainingShadow()) {
      aContent = shadowRoot->Host();
      topLevelScopeOwner = aContent;
    } else {
      aContent = aContent->GetParent();
      if (aContent && HTMLSlotElement::FromNode(aContent)) {
        topLevelScopeOwner = aContent;
      }
    }
  }

  return topLevelScopeOwner;
}

nsresult nsFocusManager::GetNextTabbableContent(
    PresShell* aPresShell, nsIContent* aRootContent,
    nsIContent* aOriginalStartContent, nsIContent* aStartContent, bool aForward,
    int32_t aCurrentTabIndex, bool aIgnoreTabIndex, bool aForDocumentNavigation,
    bool aNavigateByKey, nsIContent** aResultContent) {
  *aResultContent = nullptr;

  if (!aStartContent) {
    return NS_OK;
  }

  nsIContent* startContent = aStartContent;
  nsIContent* currentTopLevelScopeOwner = GetTopLevelScopeOwner(startContent);

  LOGCONTENTNAVIGATION("GetNextTabbable: %s", startContent);
  LOGFOCUSNAVIGATION(("  tabindex: %d", aCurrentTabIndex));

  // If startContent is a shadow host or slot in forward navigation,
  // search in scope owned by startContent
  if (aForward && IsHostOrSlot(startContent)) {
    nsIContent* contentToFocus = GetNextTabbableContentInScope(
        startContent, startContent, aOriginalStartContent, aForward,
        aForward ? 1 : 0, aIgnoreTabIndex, aForDocumentNavigation,
        aNavigateByKey, true /* aSkipOwner */);
    if (contentToFocus) {
      NS_ADDREF(*aResultContent = contentToFocus);
      return NS_OK;
    }
  }

  // If startContent is in a scope owned by Shadow DOM search from scope
  // including startContent
  if (nsIContent* owner = FindScopeOwner(startContent)) {
    nsIContent* contentToFocus = GetNextTabbableContentInAncestorScopes(
        owner, &startContent, aOriginalStartContent, aForward,
        &aCurrentTabIndex, aIgnoreTabIndex, aForDocumentNavigation,
        aNavigateByKey);
    if (contentToFocus) {
      NS_ADDREF(*aResultContent = contentToFocus);
      return NS_OK;
    }
  }

  // If we reach here, it means no next tabbable content in shadow DOM.
  // We need to continue searching in light DOM, starting at the top level
  // shadow host in light DOM (updated startContent) and its tabindex
  // (updated aCurrentTabIndex).
  MOZ_ASSERT(!FindScopeOwner(startContent),
             "startContent should not be owned by Shadow DOM at this point");

  nsPresContext* presContext = aPresShell->GetPresContext();

  bool getNextFrame = true;
  nsCOMPtr<nsIContent> iterStartContent = startContent;
  nsIContent* topLevelScopeStartContent = startContent;
  // Iterate tab index to find corresponding contents
  while (1) {
    nsIFrame* frame = iterStartContent->GetPrimaryFrame();
    // if there is no frame, look for another content node that has a frame
    while (!frame) {
      // if the root content doesn't have a frame, just return
      if (iterStartContent == aRootContent) {
        return NS_OK;
      }

      // look for the next or previous content node in tree order
      iterStartContent = aForward ? iterStartContent->GetNextNode()
                                  : iterStartContent->GetPreviousContent();
      if (!iterStartContent) {
        break;
      }

      frame = iterStartContent->GetPrimaryFrame();
      // Host without frame, enter its scope.
      if (!frame && iterStartContent->GetShadowRoot()) {
        int32_t tabIndex = HostOrSlotTabIndexValue(iterStartContent);
        if (tabIndex >= 0 &&
            (aIgnoreTabIndex || aCurrentTabIndex == tabIndex)) {
          nsIContent* contentToFocus = GetNextTabbableContentInScope(
              iterStartContent, iterStartContent, aOriginalStartContent,
              aForward, aForward ? 1 : 0, aIgnoreTabIndex,
              aForDocumentNavigation, aNavigateByKey, true /* aSkipOwner */);
          if (contentToFocus) {
            NS_ADDREF(*aResultContent = contentToFocus);
            return NS_OK;
          }
        }
      }
      // we've already skipped over the initial focused content, so we
      // don't want to traverse frames.
      getNextFrame = false;
    }

    nsCOMPtr<nsIFrameEnumerator> frameTraversal;
    if (frame) {
      // For tab navigation, pass false for aSkipPopupChecks so that we don't
      // iterate into or out of a popup. For document naviation pass true to
      // ignore these boundaries.
      nsresult rv = NS_NewFrameTraversal(
          getter_AddRefs(frameTraversal), presContext, frame, ePreOrder,
          false,                  // aVisual
          false,                  // aLockInScrollView
          true,                   // aFollowOOFs
          aForDocumentNavigation  // aSkipPopupChecks
      );
      NS_ENSURE_SUCCESS(rv, rv);

      if (iterStartContent == aRootContent) {
        if (!aForward) {
          frameTraversal->Last();
        } else if (aRootContent->IsFocusable()) {
          frameTraversal->Next();
        }
        frame = frameTraversal->CurrentItem();
      } else if (getNextFrame &&
                 (!iterStartContent ||
                  !iterStartContent->IsHTMLElement(nsGkAtoms::area))) {
        // Need to do special check in case we're in an imagemap which has
        // multiple content nodes per frame, so don't skip over the starting
        // frame.
        frame = frameTraversal->Traverse(aForward);
      }
    }

    nsIContent* oldTopLevelScopeOwner = nullptr;
    // Walk frames to find something tabbable matching aCurrentTabIndex
    while (frame) {
      // Try to find the topmost scope owner, since we want to skip the node
      // that is not owned by document in frame traversal.
      nsIContent* currentContent = frame->GetContent();
      if (currentTopLevelScopeOwner) {
        oldTopLevelScopeOwner = currentTopLevelScopeOwner;
      }
      currentTopLevelScopeOwner = GetTopLevelScopeOwner(currentContent);
      if (currentTopLevelScopeOwner &&
          currentTopLevelScopeOwner == oldTopLevelScopeOwner) {
        // We're within non-document scope, continue.
        do {
          if (aForward) {
            frameTraversal->Next();
          } else {
            frameTraversal->Prev();
          }
          frame = static_cast<nsIFrame*>(frameTraversal->CurrentItem());
          // For the usage of GetPrevContinuation, see the comment
          // at the end of while (frame) loop.
        } while (frame && frame->GetPrevContinuation());
        continue;
      }

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
            nsIContent* content = topLevelScopeStartContent;
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
            nsresult rv = GetNextTabbableContent(
                aPresShell, currentContent, nullptr, currentContent, true, 1,
                false, false, aNavigateByKey, aResultContent);
            if (NS_SUCCEEDED(rv) && *aResultContent) {
              return rv;
            }
          }
        }
      }

      // As of now, 2018/04/12, sequential focus navigation is still
      // in the obsolete Shadow DOM specification.
      // http://w3c.github.io/webcomponents/spec/shadow/#sequential-focus-navigation
      // "if ELEMENT is focusable, a shadow host, or a slot element,
      //  append ELEMENT to NAVIGATION-ORDER."
      // and later in "For each element ELEMENT in NAVIGATION-ORDER: "
      // hosts and slots are handled before other elements.
      if (currentTopLevelScopeOwner) {
        bool focusableHostSlot;
        int32_t tabIndex = HostOrSlotTabIndexValue(currentTopLevelScopeOwner,
                                                   &focusableHostSlot);
        // Host or slot itself isn't focusable or going backwards, enter its
        // scope.
        if ((!aForward || !focusableHostSlot) && tabIndex >= 0 &&
            (aIgnoreTabIndex || aCurrentTabIndex == tabIndex)) {
          nsIContent* contentToFocus = GetNextTabbableContentInScope(
              currentTopLevelScopeOwner, currentTopLevelScopeOwner,
              aOriginalStartContent, aForward, aForward ? 1 : 0,
              aIgnoreTabIndex, aForDocumentNavigation, aNavigateByKey,
              true /* aSkipOwner */);
          if (contentToFocus) {
            NS_ADDREF(*aResultContent = contentToFocus);
            return NS_OK;
          }
          // If we've wrapped around already, then carry on.
          if (aOriginalStartContent &&
              currentTopLevelScopeOwner ==
                  GetTopLevelScopeOwner(aOriginalStartContent)) {
            // FIXME: Shouldn't this return null instead?  aOriginalStartContent
            // isn't focusable after all.
            NS_ADDREF(*aResultContent = aOriginalStartContent);
            return NS_OK;
          }
        }
        // There is no next tabbable content in currentTopLevelScopeOwner's
        // scope. We should continue the loop in order to skip all contents that
        // is in currentTopLevelScopeOwner's scope.
        continue;
      }

      MOZ_ASSERT(!GetTopLevelScopeOwner(currentContent),
                 "currentContent should be in top-level-scope at this point");

      // TabIndex not set defaults to 0 for form elements, anchors and other
      // elements that are normally focusable. Tabindex defaults to -1
      // for elements that are not normally focusable.
      // The returned computed tabindex from IsFocusable() is as follows:
      // clang-format off
      //          < 0 not tabbable at all
      //          == 0 in normal tab order (last after positive tabindexed items)
      //          > 0 can be tabbed to in the order specified by this value
      // clang-format on
      int32_t tabIndex = frame->IsFocusable().mTabIndex;

      LOGCONTENTNAVIGATION("Next Tabbable %s:", frame->GetContent());
      LOGFOCUSNAVIGATION(
          ("  with tabindex: %d expected: %d", tabIndex, aCurrentTabIndex));

      if (tabIndex >= 0) {
        NS_ASSERTION(currentContent,
                     "IsFocusable set a tabindex for a frame with no content");
        if (!aForDocumentNavigation &&
            currentContent->IsHTMLElement(nsGkAtoms::img) &&
            currentContent->AsElement()->HasAttr(kNameSpaceID_None,
                                                 nsGkAtoms::usemap)) {
          // This is an image with a map. Image map areas are not traversed by
          // nsIFrameTraversal so look for the next or previous area element.
          nsIContent* areaContent = GetNextTabbableMapArea(
              aForward, aCurrentTabIndex, currentContent->AsElement(),
              iterStartContent);
          if (areaContent) {
            NS_ADDREF(*aResultContent = areaContent);
            return NS_OK;
          }
        } else if (aIgnoreTabIndex || aCurrentTabIndex == tabIndex) {
          // break out if we've wrapped around to the start again.
          if (aOriginalStartContent &&
              currentContent == aOriginalStartContent) {
            NS_ADDREF(*aResultContent = currentContent);
            return NS_OK;
          }

          // If this is a remote child browser, call NavigateDocument to have
          // the child process continue the navigation. Return a special error
          // code to have the caller return early. If the child ends up not
          // being focusable in some way, the child process will call back
          // into document navigation again by calling MoveFocus.
          if (BrowserParent* remote = BrowserParent::GetFrom(currentContent)) {
            if (aNavigateByKey) {
              remote->NavigateByKey(aForward, aForDocumentNavigation);
              return NS_SUCCESS_DOM_NO_OPERATION;
            }
            return NS_OK;
          }

          // Same as above but for out-of-process iframes
          if (auto* bbc = BrowserBridgeChild::GetFrom(currentContent)) {
            if (aNavigateByKey) {
              bbc->NavigateByKey(aForward, aForDocumentNavigation);
              return NS_SUCCESS_DOM_NO_OPERATION;
            }
            return NS_OK;
          }

          // Next, for document navigation, check if this a non-remote child
          // document.
          bool checkSubDocument = true;
          if (aForDocumentNavigation &&
              TryDocumentNavigation(currentContent, &checkSubDocument,
                                    aResultContent)) {
            return NS_OK;
          }

          if (checkSubDocument) {
            // found a node with a matching tab index. Check if it is a child
            // frame. If so, navigate into the child frame instead.
            if (TryToMoveFocusToSubDocument(
                    currentContent, aOriginalStartContent, aForward,
                    aForDocumentNavigation, aNavigateByKey, aResultContent)) {
              MOZ_ASSERT(*aResultContent);
              return NS_OK;
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
      } else if (aOriginalStartContent &&
                 currentContent == aOriginalStartContent) {
        // not focusable, so return if we have wrapped around to the original
        // content. This is necessary in case the original starting content was
        // not focusable.
        //
        // FIXME: Shouldn't this return null instead? currentContent isn't
        // focusable after all.
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
        if (aForward) {
          frameTraversal->Next();
        } else {
          frameTraversal->Prev();
        }
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

        RefPtr<Element> docRoot = GetRootForFocus(
            window, aRootContent->GetComposedDoc(), false, true);
        FocusFirst(docRoot, aResultContent);
      }
      break;
    }

    // continue looking for next highest priority tabindex
    aCurrentTabIndex =
        GetNextTabIndex(aRootContent, aCurrentTabIndex, aForward);
    startContent = iterStartContent = aRootContent;
    currentTopLevelScopeOwner = GetTopLevelScopeOwner(startContent);
  }

  return NS_OK;
}

bool nsFocusManager::TryDocumentNavigation(nsIContent* aCurrentContent,
                                           bool* aCheckSubDocument,
                                           nsIContent** aResultContent) {
  *aCheckSubDocument = true;
  if (Element* docRoot = GetRootForChildDocument(aCurrentContent)) {
    // If GetRootForChildDocument returned something then call
    // FocusFirst to find the root or first element to focus within
    // the child document. If this is a frameset though, skip this and
    // fall through to normal tab navigation to iterate into
    // the frameset's frames and locate the first focusable frame.
    if (!docRoot->IsHTMLElement(nsGkAtoms::frameset)) {
      *aCheckSubDocument = false;
      Unused << FocusFirst(docRoot, aResultContent);
      return *aResultContent != nullptr;
    }
  } else {
    // Set aCheckSubDocument to false, as this was neither a frame
    // type element or a child document that was focusable.
    *aCheckSubDocument = false;
  }

  return false;
}

bool nsFocusManager::TryToMoveFocusToSubDocument(
    nsIContent* aCurrentContent, nsIContent* aOriginalStartContent,
    bool aForward, bool aForDocumentNavigation, bool aNavigateByKey,
    nsIContent** aResultContent) {
  Document* doc = aCurrentContent->GetComposedDoc();
  NS_ASSERTION(doc, "content not in document");
  Document* subdoc = doc->GetSubDocumentFor(aCurrentContent);
  if (subdoc && !subdoc->EventHandlingSuppressed()) {
    if (aForward) {
      // When tabbing forward into a frame, return the root
      // frame so that the canvas becomes focused.
      if (nsCOMPtr<nsPIDOMWindowOuter> subframe = subdoc->GetWindow()) {
        *aResultContent = GetRootForFocus(subframe, subdoc, false, true);
        if (*aResultContent) {
          NS_ADDREF(*aResultContent);
          return true;
        }
      }
    }
    Element* rootElement = subdoc->GetRootElement();
    PresShell* subPresShell = subdoc->GetPresShell();
    if (rootElement && subPresShell) {
      nsresult rv = GetNextTabbableContent(
          subPresShell, rootElement, aOriginalStartContent, rootElement,
          aForward, (aForward ? 1 : 0), false, aForDocumentNavigation,
          aNavigateByKey, aResultContent);
      NS_ENSURE_SUCCESS(rv, false);
      if (*aResultContent) {
        return true;
      }
    }
  }
  return false;
}

nsIContent* nsFocusManager::GetNextTabbableMapArea(bool aForward,
                                                   int32_t aCurrentTabIndex,
                                                   Element* aImageContent,
                                                   nsIContent* aStartContent) {
  if (aImageContent->IsInComposedDoc()) {
    HTMLImageElement* imgElement = HTMLImageElement::FromNode(aImageContent);
    // The caller should check the element type, so we can assert here.
    MOZ_ASSERT(imgElement);

    nsCOMPtr<nsIContent> mapContent = imgElement->FindImageMap();
    if (!mapContent) {
      return nullptr;
    }
    uint32_t count = mapContent->GetChildCount();
    // First see if the the start content is in this map

    int32_t index = mapContent->ComputeIndexOf(aStartContent);
    int32_t tabIndex;
    if (index < 0 || (aStartContent->IsFocusable(&tabIndex) &&
                      tabIndex != aCurrentTabIndex)) {
      // If aStartContent is in this map we must start iterating past it.
      // We skip the case where aStartContent has tabindex == aStartContent
      // since the next tab ordered element might be before it
      // (or after for backwards) in the child list.
      index = aForward ? -1 : (int32_t)count;
    }

    // GetChildAt_Deprecated will return nullptr if our index < 0 or index >=
    // count
    nsCOMPtr<nsIContent> areaContent;
    while ((areaContent = mapContent->GetChildAt_Deprecated(
                aForward ? ++index : --index)) != nullptr) {
      if (areaContent->IsFocusable(&tabIndex) && tabIndex == aCurrentTabIndex) {
        return areaContent;
      }
    }
  }

  return nullptr;
}

int32_t nsFocusManager::GetNextTabIndex(nsIContent* aParent,
                                        int32_t aCurrentTabIndex,
                                        bool aForward) {
  int32_t tabIndex, childTabIndex;
  StyleChildrenIterator iter(aParent);

  if (aForward) {
    tabIndex = 0;
    for (nsIContent* child = iter.GetNextChild(); child;
         child = iter.GetNextChild()) {
      // Skip child's descendants if child is a shadow host or slot, as they are
      // in the focus navigation scope owned by child's shadow root
      if (!IsHostOrSlot(child)) {
        childTabIndex = GetNextTabIndex(child, aCurrentTabIndex, aForward);
        if (childTabIndex > aCurrentTabIndex && childTabIndex != tabIndex) {
          tabIndex = (tabIndex == 0 || childTabIndex < tabIndex) ? childTabIndex
                                                                 : tabIndex;
        }
      }

      nsAutoString tabIndexStr;
      if (child->IsElement()) {
        child->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::tabindex,
                                    tabIndexStr);
      }
      nsresult ec;
      int32_t val = tabIndexStr.ToInteger(&ec);
      if (NS_SUCCEEDED(ec) && val > aCurrentTabIndex && val != tabIndex) {
        tabIndex = (tabIndex == 0 || val < tabIndex) ? val : tabIndex;
      }
    }
  } else { /* !aForward */
    tabIndex = 1;
    for (nsIContent* child = iter.GetNextChild(); child;
         child = iter.GetNextChild()) {
      // Skip child's descendants if child is a shadow host or slot, as they are
      // in the focus navigation scope owned by child's shadow root
      if (!IsHostOrSlot(child)) {
        childTabIndex = GetNextTabIndex(child, aCurrentTabIndex, aForward);
        if ((aCurrentTabIndex == 0 && childTabIndex > tabIndex) ||
            (childTabIndex < aCurrentTabIndex && childTabIndex > tabIndex)) {
          tabIndex = childTabIndex;
        }
      }

      nsAutoString tabIndexStr;
      if (child->IsElement()) {
        child->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::tabindex,
                                    tabIndexStr);
      }
      nsresult ec;
      int32_t val = tabIndexStr.ToInteger(&ec);
      if (NS_SUCCEEDED(ec)) {
        if ((aCurrentTabIndex == 0 && val > tabIndex) ||
            (val < aCurrentTabIndex && val > tabIndex)) {
          tabIndex = val;
        }
      }
    }
  }

  return tabIndex;
}

nsresult nsFocusManager::FocusFirst(Element* aRootElement,
                                    nsIContent** aNextContent) {
  if (!aRootElement) {
    return NS_OK;
  }

  Document* doc = aRootElement->GetComposedDoc();
  if (doc) {
    if (nsContentUtils::IsChromeDoc(doc)) {
      // If the redirectdocumentfocus attribute is set, redirect the focus to a
      // specific element. This is primarily used to retarget the focus to the
      // urlbar during document navigation.
      nsAutoString retarget;

      if (aRootElement->GetAttr(kNameSpaceID_None,
                                nsGkAtoms::retargetdocumentfocus, retarget)) {
        nsCOMPtr<Element> element = doc->GetElementById(retarget);
        nsCOMPtr<nsIContent> retargetElement =
            FlushAndCheckIfFocusable(element, 0);
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
      PresShell* presShell = doc->GetPresShell();
      if (presShell) {
        return GetNextTabbableContent(presShell, aRootElement, nullptr,
                                      aRootElement, true, 1, false, false, true,
                                      aNextContent);
      }
    }
  }

  NS_ADDREF(*aNextContent = aRootElement);
  return NS_OK;
}

Element* nsFocusManager::GetRootForFocus(nsPIDOMWindowOuter* aWindow,
                                         Document* aDocument,
                                         bool aForDocumentNavigation,
                                         bool aCheckVisibility) {
  if (!aForDocumentNavigation) {
    nsCOMPtr<nsIDocShell> docShell = aWindow->GetDocShell();
    if (docShell->ItemType() == nsIDocShellTreeItem::typeChrome) {
      return nullptr;
    }
  }

  if (aCheckVisibility && !IsWindowVisible(aWindow)) return nullptr;

  // If the body is contenteditable, use the editor's root element rather than
  // the actual root element.
  RefPtr<Element> rootElement =
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
  if (aDocument && aDocument->IsHTMLOrXHTML()) {
    Element* htmlChild = aDocument->GetHtmlChildElement(nsGkAtoms::frameset);
    if (htmlChild) {
      // In document navigation mode, return the frameset so that navigation
      // descends into the child frames.
      return aForDocumentNavigation ? htmlChild : nullptr;
    }
  }

  return rootElement;
}

Element* nsFocusManager::GetRootForChildDocument(nsIContent* aContent) {
  // Check for elements that represent child documents, that is, browsers,
  // editors or frames from a frameset. We don't include iframes since we
  // consider them to be an integral part of the same window or page.
  if (!aContent || !(aContent->IsXULElement(nsGkAtoms::browser) ||
                     aContent->IsXULElement(nsGkAtoms::editor) ||
                     aContent->IsHTMLElement(nsGkAtoms::frame))) {
    return nullptr;
  }

  Document* doc = aContent->GetComposedDoc();
  if (!doc) {
    return nullptr;
  }

  Document* subdoc = doc->GetSubDocumentFor(aContent);
  if (!subdoc || subdoc->EventHandlingSuppressed()) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = subdoc->GetWindow();
  return GetRootForFocus(window, subdoc, true, true);
}

void nsFocusManager::GetFocusInSelection(nsPIDOMWindowOuter* aWindow,
                                         nsIContent* aStartSelection,
                                         nsIContent* aEndSelection,
                                         nsIContent** aFocusedContent) {
  *aFocusedContent = nullptr;

  nsCOMPtr<nsIContent> testContent = aStartSelection;
  nsCOMPtr<nsIContent> nextTestContent = aEndSelection;

  nsCOMPtr<nsIContent> currentFocus = aWindow->GetFocusedElement();

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
      // We run this loop again, checking the ancestor chain of the selection's
      // end point
      testContent = nextTestContent;
      nextTestContent = nullptr;
    }
  }

  // We couldn't find an anchor that was an ancestor of the selection start
  // Method #2: look for anchor in selection's primary range (depth first
  // search)

  nsCOMPtr<nsIContent> selectionNode = aStartSelection;
  nsCOMPtr<nsIContent> endSelectionNode = aEndSelection;
  nsCOMPtr<nsIContent> testNode;

  do {
    testContent = selectionNode;

    // We're looking for any focusable link that could be part of the
    // main document's selection.
    nsCOMPtr<nsIURI> uri;
    if (testContent == currentFocus ||
        testContent->IsLink(getter_AddRefs(uri))) {
      testContent.forget(aFocusedContent);
      return;
    }

    nsIContent* testNode = selectionNode->GetFirstChild();
    if (testNode) {
      selectionNode = testNode;
      continue;
    }

    if (selectionNode == endSelectionNode) {
      break;
    }
    testNode = selectionNode->GetNextSibling();
    if (testNode) {
      selectionNode = testNode;
      continue;
    }

    do {
      // GetParent is OK here, instead of GetParentNode, because the only case
      // where the latter returns something different from the former is when
      // GetParentNode is the document.  But in that case we would simply get
      // null for selectionNode when setting it to testNode->GetNextSibling()
      // (because a document has no next sibling).  And then the next iteration
      // of this loop would get null for GetParentNode anyway, and break out of
      // all the loops.
      testNode = selectionNode->GetParent();
      if (!testNode || testNode == endSelectionNode) {
        selectionNode = nullptr;
        break;
      }
      selectionNode = testNode->GetNextSibling();
      if (selectionNode) {
        break;
      }
      selectionNode = testNode;
    } while (true);
  } while (selectionNode && selectionNode != endSelectionNode);
}

static void MaybeUnlockPointer(BrowsingContext* aCurrentFocusedContext) {
  if (!PointerLockManager::IsInLockContext(aCurrentFocusedContext)) {
    PointerLockManager::Unlock();
  }
}

class PointerUnlocker : public Runnable {
 public:
  PointerUnlocker() : mozilla::Runnable("PointerUnlocker") {
    MOZ_ASSERT(XRE_IsParentProcess());
    MOZ_ASSERT(!PointerUnlocker::sActiveUnlocker);
    PointerUnlocker::sActiveUnlocker = this;
  }

  ~PointerUnlocker() {
    if (PointerUnlocker::sActiveUnlocker == this) {
      PointerUnlocker::sActiveUnlocker = nullptr;
    }
  }

  NS_IMETHOD Run() override {
    if (PointerUnlocker::sActiveUnlocker == this) {
      PointerUnlocker::sActiveUnlocker = nullptr;
    }
    NS_ENSURE_STATE(nsFocusManager::GetFocusManager());
    nsPIDOMWindowOuter* focused =
        nsFocusManager::GetFocusManager()->GetFocusedWindow();
    MaybeUnlockPointer(focused ? focused->GetBrowsingContext() : nullptr);
    return NS_OK;
  }

  static PointerUnlocker* sActiveUnlocker;
};

PointerUnlocker* PointerUnlocker::sActiveUnlocker = nullptr;

void nsFocusManager::SetFocusedBrowsingContext(BrowsingContext* aContext,
                                               uint64_t aActionId) {
  if (XRE_IsParentProcess()) {
    return;
  }
  MOZ_ASSERT(!ActionIdComparableAndLower(
      aActionId, mActionIdForFocusedBrowsingContextInContent));
  mFocusedBrowsingContextInContent = aContext;
  mActionIdForFocusedBrowsingContextInContent = aActionId;
  if (aContext) {
    // We don't send the unset but instead expect the set from
    // elsewhere to take care of it. XXX Is that bad?
    MOZ_ASSERT(aContext->IsInProcess());
    mozilla::dom::ContentChild* contentChild =
        mozilla::dom::ContentChild::GetSingleton();
    MOZ_ASSERT(contentChild);
    contentChild->SendSetFocusedBrowsingContext(aContext, aActionId);
  }
}

void nsFocusManager::SetFocusedBrowsingContextFromOtherProcess(
    BrowsingContext* aContext, uint64_t aActionId) {
  MOZ_ASSERT(!XRE_IsParentProcess());
  MOZ_ASSERT(aContext);
  if (ActionIdComparableAndLower(aActionId,
                                 mActionIdForFocusedBrowsingContextInContent)) {
    // Unclear if this ever happens.
    LOGFOCUS(
        ("Ignored an attempt to set an in-process BrowsingContext [%p] as "
         "focused from another process due to stale action id %" PRIu64 ".",
         aContext, aActionId));
    return;
  }
  if (aContext->IsInProcess()) {
    // This message has been in transit for long enough that
    // the process association of aContext has changed since
    // the other content process sent the message, because
    // an iframe in that process became an out-of-process
    // iframe while the IPC broadcast that we're receiving
    // was in-flight. Let's just ignore this.
    LOGFOCUS(
        ("Ignored an attempt to set an in-process BrowsingContext [%p] as "
         "focused from another process, actionid: %" PRIu64 ".",
         aContext, aActionId));
    return;
  }
  mFocusedBrowsingContextInContent = aContext;
  mActionIdForFocusedBrowsingContextInContent = aActionId;
  mFocusedElement = nullptr;
  mFocusedWindow = nullptr;
}

bool nsFocusManager::SetFocusedBrowsingContextInChrome(
    mozilla::dom::BrowsingContext* aContext, uint64_t aActionId) {
  MOZ_ASSERT(aActionId);
  if (ProcessPendingFocusedBrowsingContextActionId(aActionId)) {
    MOZ_DIAGNOSTIC_ASSERT(!ActionIdComparableAndLower(
        aActionId, mActionIdForFocusedBrowsingContextInChrome));
    mFocusedBrowsingContextInChrome = aContext;
    mActionIdForFocusedBrowsingContextInChrome = aActionId;
    return true;
  }
  return false;
}

BrowsingContext* nsFocusManager::GetFocusedBrowsingContextInChrome() {
  return mFocusedBrowsingContextInChrome;
}

void nsFocusManager::BrowsingContextDetached(BrowsingContext* aContext) {
  if (mFocusedBrowsingContextInChrome == aContext) {
    mFocusedBrowsingContextInChrome = nullptr;
    // Deliberately not adjusting the corresponding action id, because
    // we don't want changes from the past to take effect.
  }
  if (mActiveBrowsingContextInChrome == aContext) {
    mActiveBrowsingContextInChrome = nullptr;
    // Deliberately not adjusting the corresponding action id, because
    // we don't want changes from the past to take effect.
  }
}

void nsFocusManager::SetActiveBrowsingContextInContent(
    mozilla::dom::BrowsingContext* aContext, uint64_t aActionId) {
  MOZ_ASSERT(!XRE_IsParentProcess());
  MOZ_ASSERT(!aContext || aContext->IsInProcess());
  mozilla::dom::ContentChild* contentChild =
      mozilla::dom::ContentChild::GetSingleton();
  MOZ_ASSERT(contentChild);

  if (ActionIdComparableAndLower(aActionId,
                                 mActionIdForActiveBrowsingContextInContent)) {
    LOGFOCUS(
        ("Ignored an attempt to set an in-process BrowsingContext [%p] as "
         "the active browsing context due to a stale action id %" PRIu64 ".",
         aContext, aActionId));
    return;
  }

  if (aContext != mActiveBrowsingContextInContent) {
    if (aContext) {
      contentChild->SendSetActiveBrowsingContext(aContext, aActionId);
    } else if (mActiveBrowsingContextInContent) {
      // We want to sync this over only if this isn't happening
      // due to the active BrowsingContext switching processes,
      // in which case the BrowserChild has already marked itself
      // as destroying.
      nsPIDOMWindowOuter* outer =
          mActiveBrowsingContextInContent->GetDOMWindow();
      if (outer) {
        nsPIDOMWindowInner* inner = outer->GetCurrentInnerWindow();
        if (inner) {
          WindowGlobalChild* globalChild = inner->GetWindowGlobalChild();
          if (globalChild) {
            RefPtr<BrowserChild> browserChild = globalChild->GetBrowserChild();
            if (browserChild && !browserChild->IsDestroyed()) {
              contentChild->SendUnsetActiveBrowsingContext(
                  mActiveBrowsingContextInContent, aActionId);
            }
          }
        }
      }
    }
  }
  mActiveBrowsingContextInContentSetFromOtherProcess = false;
  mActiveBrowsingContextInContent = aContext;
  mActionIdForActiveBrowsingContextInContent = aActionId;
  MaybeUnlockPointer(aContext);
}

void nsFocusManager::SetActiveBrowsingContextFromOtherProcess(
    BrowsingContext* aContext, uint64_t aActionId) {
  MOZ_ASSERT(!XRE_IsParentProcess());
  MOZ_ASSERT(aContext);
  if (ActionIdComparableAndLower(aActionId,
                                 mActionIdForActiveBrowsingContextInContent)) {
    LOGFOCUS(
        ("Ignored an attempt to set active BrowsingContext [%p] from "
         "another process due to a stale action id %" PRIu64 ".",
         aContext, aActionId));
    return;
  }
  if (aContext->IsInProcess()) {
    // This message has been in transit for long enough that
    // the process association of aContext has changed since
    // the other content process sent the message, because
    // an iframe in that process became an out-of-process
    // iframe while the IPC broadcast that we're receiving
    // was in-flight. Let's just ignore this.
    LOGFOCUS(
        ("Ignored an attempt to set an in-process BrowsingContext [%p] as "
         "active from another process. actionid: %" PRIu64,
         aContext, aActionId));
    return;
  }
  mActiveBrowsingContextInContentSetFromOtherProcess = true;
  mActiveBrowsingContextInContent = aContext;
  mActionIdForActiveBrowsingContextInContent = aActionId;
  MaybeUnlockPointer(aContext);
}

void nsFocusManager::UnsetActiveBrowsingContextFromOtherProcess(
    BrowsingContext* aContext, uint64_t aActionId) {
  MOZ_ASSERT(!XRE_IsParentProcess());
  MOZ_ASSERT(aContext);
  if (ActionIdComparableAndLower(aActionId,
                                 mActionIdForActiveBrowsingContextInContent)) {
    LOGFOCUS(
        ("Ignored an attempt to unset the active BrowsingContext [%p] from "
         "another process due to stale action id: %" PRIu64 ".",
         aContext, aActionId));
    return;
  }
  if (mActiveBrowsingContextInContent == aContext) {
    mActiveBrowsingContextInContent = nullptr;
    mActionIdForActiveBrowsingContextInContent = aActionId;
    MaybeUnlockPointer(nullptr);
  } else {
    LOGFOCUS(
        ("Ignored an attempt to unset the active BrowsingContext [%p] from "
         "another process. actionid: %" PRIu64,
         aContext, aActionId));
  }
}

void nsFocusManager::ReviseActiveBrowsingContext(
    uint64_t aOldActionId, mozilla::dom::BrowsingContext* aContext,
    uint64_t aNewActionId) {
  MOZ_ASSERT(XRE_IsContentProcess());
  if (mActionIdForActiveBrowsingContextInContent == aOldActionId) {
    LOGFOCUS(("Revising the active BrowsingContext [%p]. old actionid: %" PRIu64
              ", new "
              "actionid: %" PRIu64,
              aContext, aOldActionId, aNewActionId));
    mActiveBrowsingContextInContent = aContext;
    mActionIdForActiveBrowsingContextInContent = aNewActionId;
  } else {
    LOGFOCUS(
        ("Ignored a stale attempt to revise the active BrowsingContext [%p]. "
         "old actionid: %" PRIu64 ", new actionid: %" PRIu64,
         aContext, aOldActionId, aNewActionId));
  }
}

void nsFocusManager::ReviseFocusedBrowsingContext(
    uint64_t aOldActionId, mozilla::dom::BrowsingContext* aContext,
    uint64_t aNewActionId) {
  MOZ_ASSERT(XRE_IsContentProcess());
  if (mActionIdForFocusedBrowsingContextInContent == aOldActionId) {
    LOGFOCUS(
        ("Revising the focused BrowsingContext [%p]. old actionid: %" PRIu64
         ", new "
         "actionid: %" PRIu64,
         aContext, aOldActionId, aNewActionId));
    mFocusedBrowsingContextInContent = aContext;
    mActionIdForFocusedBrowsingContextInContent = aNewActionId;
    mFocusedElement = nullptr;
  } else {
    LOGFOCUS(
        ("Ignored a stale attempt to revise the focused BrowsingContext [%p]. "
         "old actionid: %" PRIu64 ", new actionid: %" PRIu64,
         aContext, aOldActionId, aNewActionId));
  }
}

bool nsFocusManager::SetActiveBrowsingContextInChrome(
    mozilla::dom::BrowsingContext* aContext, uint64_t aActionId) {
  MOZ_ASSERT(aActionId);
  if (ProcessPendingActiveBrowsingContextActionId(aActionId, aContext)) {
    MOZ_DIAGNOSTIC_ASSERT(!ActionIdComparableAndLower(
        aActionId, mActionIdForActiveBrowsingContextInChrome));
    mActiveBrowsingContextInChrome = aContext;
    mActionIdForActiveBrowsingContextInChrome = aActionId;
    return true;
  }
  return false;
}

uint64_t nsFocusManager::GetActionIdForActiveBrowsingContextInChrome() const {
  return mActionIdForActiveBrowsingContextInChrome;
}

uint64_t nsFocusManager::GetActionIdForFocusedBrowsingContextInChrome() const {
  return mActionIdForFocusedBrowsingContextInChrome;
}

BrowsingContext* nsFocusManager::GetActiveBrowsingContextInChrome() {
  return mActiveBrowsingContextInChrome;
}

void nsFocusManager::InsertNewFocusActionId(uint64_t aActionId) {
  LOGFOCUS(("InsertNewFocusActionId %" PRIu64, aActionId));
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(!mPendingActiveBrowsingContextActions.Contains(aActionId));
  mPendingActiveBrowsingContextActions.AppendElement(aActionId);
  MOZ_ASSERT(!mPendingFocusedBrowsingContextActions.Contains(aActionId));
  mPendingFocusedBrowsingContextActions.AppendElement(aActionId);
}

static void RemoveContentInitiatedActionsUntil(
    nsTArray<uint64_t>& aPendingActions,
    nsTArray<uint64_t>::index_type aUntil) {
  nsTArray<uint64_t>::index_type i = 0;
  while (i < aUntil) {
    auto [actionProc, actionId] =
        nsContentUtils::SplitProcessSpecificId(aPendingActions[i]);
    Unused << actionId;
    if (actionProc) {
      aPendingActions.RemoveElementAt(i);
      --aUntil;
      continue;
    }
    ++i;
  }
}

bool nsFocusManager::ProcessPendingActiveBrowsingContextActionId(
    uint64_t aActionId, bool aSettingToNonNull) {
  MOZ_ASSERT(XRE_IsParentProcess());
  auto index = mPendingActiveBrowsingContextActions.IndexOf(aActionId);
  if (index == nsTArray<uint64_t>::NoIndex) {
    return false;
  }
  // When aSettingToNonNull is true, we need to remove one more
  // element to remove the action id itself in addition to
  // removing the older ones.
  if (aSettingToNonNull) {
    index++;
  }
  auto [actionProc, actionId] =
      nsContentUtils::SplitProcessSpecificId(aActionId);
  Unused << actionId;
  if (actionProc) {
    // Action from content: We allow parent-initiated actions
    // to take precedence over content-initiated ones, so we
    // remove only prior content-initiated actions.
    RemoveContentInitiatedActionsUntil(mPendingActiveBrowsingContextActions,
                                       index);
  } else {
    // Action from chrome
    mPendingActiveBrowsingContextActions.RemoveElementsAt(0, index);
  }
  return true;
}

bool nsFocusManager::ProcessPendingFocusedBrowsingContextActionId(
    uint64_t aActionId) {
  MOZ_ASSERT(XRE_IsParentProcess());
  auto index = mPendingFocusedBrowsingContextActions.IndexOf(aActionId);
  if (index == nsTArray<uint64_t>::NoIndex) {
    return false;
  }

  auto [actionProc, actionId] =
      nsContentUtils::SplitProcessSpecificId(aActionId);
  Unused << actionId;
  if (actionProc) {
    // Action from content: We allow parent-initiated actions
    // to take precedence over content-initiated ones, so we
    // remove only prior content-initiated actions.
    RemoveContentInitiatedActionsUntil(mPendingFocusedBrowsingContextActions,
                                       index);
  } else {
    // Action from chrome
    mPendingFocusedBrowsingContextActions.RemoveElementsAt(0, index);
  }
  return true;
}

// static
uint64_t nsFocusManager::GenerateFocusActionId() {
  uint64_t id =
      nsContentUtils::GenerateProcessSpecificId(++sFocusActionCounter);
  if (XRE_IsParentProcess()) {
    nsFocusManager* fm = GetFocusManager();
    if (fm) {
      fm->InsertNewFocusActionId(id);
    }
  } else {
    mozilla::dom::ContentChild* contentChild =
        mozilla::dom::ContentChild::GetSingleton();
    MOZ_ASSERT(contentChild);
    contentChild->SendInsertNewFocusActionId(id);
  }
  LOGFOCUS(("GenerateFocusActionId %" PRIu64, id));
  return id;
}

static bool IsInPointerLockContext(nsPIDOMWindowOuter* aWin) {
  return PointerLockManager::IsInLockContext(aWin ? aWin->GetBrowsingContext()
                                                  : nullptr);
}

void nsFocusManager::SetFocusedWindowInternal(nsPIDOMWindowOuter* aWindow,
                                              uint64_t aActionId,
                                              bool aSyncBrowsingContext) {
  if (XRE_IsParentProcess() && !PointerUnlocker::sActiveUnlocker &&
      IsInPointerLockContext(mFocusedWindow) &&
      !IsInPointerLockContext(aWindow)) {
    nsCOMPtr<nsIRunnable> runnable = new PointerUnlocker();
    NS_DispatchToCurrentThread(runnable);
  }

  // Update the last focus time on any affected documents
  if (aWindow && aWindow != mFocusedWindow) {
    const TimeStamp now(TimeStamp::Now());
    for (Document* doc = aWindow->GetExtantDoc(); doc;
         doc = doc->GetInProcessParentDocument()) {
      doc->SetLastFocusTime(now);
    }
  }

  // This function may be called with zero action id to indicate that the
  // action id should be ignored.
  if (XRE_IsContentProcess() && aActionId &&
      ActionIdComparableAndLower(aActionId,
                                 mActionIdForFocusedBrowsingContextInContent)) {
    // Unclear if this ever happens.
    LOGFOCUS(
        ("Ignored an attempt to set an in-process BrowsingContext as "
         "focused due to stale action id %" PRIu64 ".",
         aActionId));
    return;
  }

  mFocusedWindow = aWindow;
  BrowsingContext* bc = aWindow ? aWindow->GetBrowsingContext() : nullptr;
  if (aSyncBrowsingContext) {
    MOZ_ASSERT(aActionId,
               "aActionId must not be zero if aSyncBrowsingContext is true");
    SetFocusedBrowsingContext(bc, aActionId);
  } else if (XRE_IsContentProcess()) {
    MOZ_ASSERT(mFocusedBrowsingContextInContent == bc,
               "Not syncing BrowsingContext even when different.");
  }
}

void nsFocusManager::NotifyOfReFocus(nsIContent& aContent) {
  nsPIDOMWindowOuter* window = GetCurrentWindow(&aContent);
  if (!window || window != mFocusedWindow) {
    return;
  }
  if (!aContent.IsInComposedDoc() || IsNonFocusableRoot(&aContent)) {
    return;
  }
  nsIDocShell* docShell = window->GetDocShell();
  if (!docShell) {
    return;
  }
  RefPtr<PresShell> presShell = docShell->GetPresShell();
  if (!presShell) {
    return;
  }
  nsPresContext* presContext = presShell->GetPresContext();
  if (!presContext) {
    return;
  }
  IMEStateManager::OnReFocus(presContext, aContent);
}

void nsFocusManager::MarkUncollectableForCCGeneration(uint32_t aGeneration) {
  if (!sInstance) {
    return;
  }

  if (sInstance->mActiveWindow) {
    sInstance->mActiveWindow->MarkUncollectableForCCGeneration(aGeneration);
  }
  if (sInstance->mFocusedWindow) {
    sInstance->mFocusedWindow->MarkUncollectableForCCGeneration(aGeneration);
  }
  if (sInstance->mWindowBeingLowered) {
    sInstance->mWindowBeingLowered->MarkUncollectableForCCGeneration(
        aGeneration);
  }
  if (sInstance->mFocusedElement) {
    sInstance->mFocusedElement->OwnerDoc()->MarkUncollectableForCCGeneration(
        aGeneration);
  }
  if (sInstance->mFirstBlurEvent) {
    sInstance->mFirstBlurEvent->OwnerDoc()->MarkUncollectableForCCGeneration(
        aGeneration);
  }
  if (sInstance->mFirstFocusEvent) {
    sInstance->mFirstFocusEvent->OwnerDoc()->MarkUncollectableForCCGeneration(
        aGeneration);
  }
}

bool nsFocusManager::CanSkipFocus(nsIContent* aContent) {
  if (!aContent || nsContentUtils::IsChromeDoc(aContent->OwnerDoc())) {
    return false;
  }

  if (mFocusedElement == aContent) {
    return true;
  }

  nsIDocShell* ds = aContent->OwnerDoc()->GetDocShell();
  if (!ds) {
    return true;
  }

  if (XRE_IsParentProcess()) {
    nsCOMPtr<nsIDocShellTreeItem> root;
    ds->GetInProcessRootTreeItem(getter_AddRefs(root));
    nsCOMPtr<nsPIDOMWindowOuter> newRootWindow =
        root ? root->GetWindow() : nullptr;
    if (mActiveWindow != newRootWindow) {
      nsPIDOMWindowOuter* outerWindow = aContent->OwnerDoc()->GetWindow();
      if (outerWindow && outerWindow->GetFocusedElement() == aContent) {
        return true;
      }
    }
  } else {
    BrowsingContext* bc = aContent->OwnerDoc()->GetBrowsingContext();
    BrowsingContext* top = bc ? bc->Top() : nullptr;
    if (GetActiveBrowsingContext() != top) {
      nsPIDOMWindowOuter* outerWindow = aContent->OwnerDoc()->GetWindow();
      if (outerWindow && outerWindow->GetFocusedElement() == aContent) {
        return true;
      }
    }
  }

  return false;
}

nsresult NS_NewFocusManager(nsIFocusManager** aResult) {
  NS_IF_ADDREF(*aResult = nsFocusManager::GetFocusManager());
  return NS_OK;
}

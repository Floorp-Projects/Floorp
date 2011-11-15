/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozila.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozilla/dom/TabParent.h"

#include "nsFocusManager.h"

#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIServiceManager.h"
#include "nsIEnumerator.h"
#include "nsGkAtoms.h"
#include "nsContentUtils.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMElement.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMHTMLFrameElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLMapElement.h"
#include "nsIDOMHTMLLegendElement.h"
#include "nsIDOMDocument.h"
#include "nsIDOMRange.h"
#include "nsIHTMLDocument.h"
#include "nsIFormControlFrame.h"
#include "nsGenericHTMLElement.h"
#include "nsIDocShell.h"
#include "nsIEditorDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsLayoutUtils.h"
#include "nsIPresShell.h"
#include "nsIContentViewer.h"
#include "nsFrameTraversal.h"
#include "nsObjectFrame.h"
#include "nsEventDispatcher.h"
#include "nsEventStateManager.h"
#include "nsIMEStateManager.h"
#include "nsIWebNavigation.h"
#include "nsCaret.h"
#include "nsIWidget.h"
#include "nsIBaseWindow.h"
#include "nsIViewManager.h"
#include "nsFrameSelection.h"
#include "nsXULPopupManager.h"
#include "nsIDOMNodeFilter.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIPrincipal.h"
#include "mozilla/dom/Element.h"
#include "mozAutoDocUpdate.h"
#include "mozilla/Preferences.h"
#include "mozilla/LookAndFeel.h"

#ifdef MOZ_XUL
#include "nsIDOMXULTextboxElement.h"
#include "nsIDOMXULMenuListElement.h"
#endif

#ifdef ACCESSIBILITY
#include "nsAccessibilityService.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;

//#define DEBUG_FOCUS 1
//#define DEBUG_FOCUS_NAVIGATION 1
#define PRINTTAGF(format, content)                     \
  {                                                    \
    nsAutoString tag(NS_LITERAL_STRING("(none)"));     \
    if (content)                                       \
      content->Tag()->ToString(tag);                   \
    printf(format, NS_ConvertUTF16toUTF8(tag).get());  \
  }

struct nsDelayedBlurOrFocusEvent
{
  nsDelayedBlurOrFocusEvent(PRUint32 aType,
                            nsIPresShell* aPresShell,
                            nsIDocument* aDocument,
                            nsIDOMEventTarget* aTarget)
   : mType(aType),
     mPresShell(aPresShell),
     mDocument(aDocument),
     mTarget(aTarget) { }

  nsDelayedBlurOrFocusEvent(const nsDelayedBlurOrFocusEvent& aOther)
   : mType(aOther.mType),
     mPresShell(aOther.mPresShell),
     mDocument(aOther.mDocument),
     mTarget(aOther.mTarget) { }

  PRUint32 mType;
  nsCOMPtr<nsIPresShell> mPresShell;
  nsCOMPtr<nsIDocument> mDocument;
  nsCOMPtr<nsIDOMEventTarget> mTarget;
};

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsFocusManager)
  NS_INTERFACE_MAP_ENTRY(nsIFocusManager)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIFocusManager)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsFocusManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsFocusManager)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsFocusManager)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsFocusManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mActiveWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mFocusedWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mFocusedContent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mFirstBlurEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mFirstFocusEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mWindowBeingLowered)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsFocusManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mActiveWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mFocusedWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mFocusedContent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mFirstBlurEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mFirstFocusEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mWindowBeingLowered)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

nsFocusManager* nsFocusManager::sInstance = nsnull;
bool nsFocusManager::sMouseFocusesFormControl = false;

static const char* kObservedPrefs[] = {
  "accessibility.browsewithcaret",
  "accessibility.tabfocus_applies_to_xul",
  "accessibility.mouse_focuses_formcontrol",
  NULL
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
  NS_ENSURE_TRUE(fm, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(fm);
  sInstance = fm;

  nsIContent::sTabFocusModelAppliesToXUL =
    Preferences::GetBool("accessibility.tabfocus_applies_to_xul",
                         nsIContent::sTabFocusModelAppliesToXUL);

  sMouseFocusesFormControl =
    Preferences::GetBool("accessibility.mouse_focuses_formcontrol", false);

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
                        const PRUnichar *aData)
{
  if (!nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    nsDependentString data(aData);
    if (data.EqualsLiteral("accessibility.browsewithcaret")) {
      UpdateCaret(false, true, mFocusedContent);
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
  } else if (!nsCRT::strcmp(aTopic, "xpcom-shutdown")) {
    mActiveWindow = nsnull;
    mFocusedWindow = nsnull;
    mFocusedContent = nsnull;
    mFirstBlurEvent = nsnull;
    mFirstFocusEvent = nsnull;
    mWindowBeingLowered = nsnull;
    mDelayedBlurFocusEvents.Clear();
    mMouseDownEventHandlingDocument = nsnull;
  }

  return NS_OK;
}

// given a frame content node, retrieve the nsIDOMWindow displayed in it 
static nsPIDOMWindow*
GetContentWindow(nsIContent* aContent)
{
  nsIDocument* doc = aContent->GetCurrentDoc();
  if (doc) {
    nsIDocument* subdoc = doc->GetSubDocumentFor(aContent);
    if (subdoc)
      return subdoc->GetWindow();
  }

  return nsnull;
}

// get the current window for the given content node 
static nsPIDOMWindow*
GetCurrentWindow(nsIContent* aContent)
{
  nsIDocument *doc = aContent->GetCurrentDoc();
  return doc ? doc->GetWindow() : nsnull;
}

// static
nsIContent*
nsFocusManager::GetFocusedDescendant(nsPIDOMWindow* aWindow, bool aDeep,
                                     nsPIDOMWindow** aFocusedWindow)
{
  NS_ENSURE_TRUE(aWindow, nsnull);

  *aFocusedWindow = nsnull;

  nsIContent* currentContent = nsnull;
  nsPIDOMWindow* window = aWindow->GetOuterWindow();
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
#ifdef MOZ_XUL
  if (aContent->IsXUL()) {
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
      else if (aContent->Tag() == nsGkAtoms::scale) {
        nsCOMPtr<nsIDocument> doc = aContent->GetCurrentDoc();
        if (!doc)
          return nsnull;

        nsINodeList* children = doc->BindingManager()->GetXBLChildNodesFor(aContent);
        if (children) {
          nsIContent* child = children->GetNodeAt(0);
          if (child && child->Tag() == nsGkAtoms::slider)
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

  return nsnull;
}

// static
PRUint32
nsFocusManager::GetFocusMoveReason(PRUint32 aFlags)
{
  PRUint32 reason = IMEContext::FOCUS_MOVED_UNKNOWN;
  if (aFlags & nsIFocusManager::FLAG_BYMOUSE) {
    reason = IMEContext::FOCUS_MOVED_BY_MOUSE;
  } else if (aFlags & nsIFocusManager::FLAG_BYKEY) {
    reason = IMEContext::FOCUS_MOVED_BY_KEY;
  } else if (aFlags & nsIFocusManager::FLAG_BYMOVEFOCUS) {
    reason = IMEContext::FOCUS_MOVED_BY_MOVEFOCUS;
  }

  return reason;
}

NS_IMETHODIMP
nsFocusManager::GetActiveWindow(nsIDOMWindow** aWindow)
{
  NS_IF_ADDREF(*aWindow = mActiveWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::SetActiveWindow(nsIDOMWindow* aWindow)
{
  // only top-level windows can be made active
  nsCOMPtr<nsPIDOMWindow> piWindow = do_QueryInterface(aWindow);
  if (piWindow)
      piWindow = piWindow->GetOuterWindow();

  NS_ENSURE_TRUE(piWindow && (piWindow == piWindow->GetPrivateRoot()),
                 NS_ERROR_INVALID_ARG);

  RaiseWindow(piWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::GetFocusedWindow(nsIDOMWindow** aFocusedWindow)
{
  NS_IF_ADDREF(*aFocusedWindow = mFocusedWindow);
  return NS_OK;
}

NS_IMETHODIMP nsFocusManager::SetFocusedWindow(nsIDOMWindow* aWindowToFocus)
{
#ifdef DEBUG_FOCUS
  printf("<<SetFocusedWindow begin>>\n");
#endif

  nsCOMPtr<nsPIDOMWindow> windowToFocus(do_QueryInterface(aWindowToFocus));
  NS_ENSURE_TRUE(windowToFocus, NS_ERROR_FAILURE);

  windowToFocus = windowToFocus->GetOuterWindow();

  nsCOMPtr<nsIContent> frameContent =
    do_QueryInterface(windowToFocus->GetFrameElementInternal());
  if (frameContent) {
    // pass false for aFocusChanged so that the caret does not get updated
    // and scrolling does not occur.
    SetFocusInner(frameContent, 0, false, true);
  }
  else {
    // this is a top-level window. If the window has a child frame focused,
    // clear the focus. Otherwise, focus should already be in this frame, or
    // already cleared. This ensures that focus will be in this frame and not
    // in a child.
    nsIContent* content = windowToFocus->GetFocusedNode();
    if (content) {
      nsCOMPtr<nsIDOMWindow> childWindow = GetContentWindow(content);
      if (childWindow)
        ClearFocus(windowToFocus);
    }
  }

  nsCOMPtr<nsPIDOMWindow> rootWindow = windowToFocus->GetPrivateRoot();
  if (rootWindow)
    RaiseWindow(rootWindow);

#ifdef DEBUG_FOCUS
  printf("<<SetFocusedWindow end>>\n");
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::GetFocusedElement(nsIDOMElement** aFocusedElement)
{
  if (mFocusedContent)
    CallQueryInterface(mFocusedContent, aFocusedElement);
  else
    *aFocusedElement = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::GetLastFocusMethod(nsIDOMWindow* aWindow, PRUint32* aLastFocusMethod)
{
  // the focus method is stored on the inner window
  nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(aWindow));
  if (window)
    window = window->GetCurrentInnerWindow();
  if (!window)
    window = mFocusedWindow;

  *aLastFocusMethod = window ? window->GetFocusMethod() : 0;

  NS_ASSERTION((*aLastFocusMethod & FOCUSMETHOD_MASK) == *aLastFocusMethod,
               "invalid focus method");
  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::SetFocus(nsIDOMElement* aElement, PRUint32 aFlags)
{
#ifdef DEBUG_FOCUS
  printf("<<SetFocus>>\n");
#endif

  nsCOMPtr<nsIContent> newFocus = do_QueryInterface(aElement);
  NS_ENSURE_ARG(newFocus);

  SetFocusInner(newFocus, aFlags, true, true);

  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::ElementIsFocusable(nsIDOMElement* aElement, PRUint32 aFlags,
                                   bool* aIsFocusable)
{
  NS_ENSURE_TRUE(aElement, NS_ERROR_INVALID_ARG);

  nsCOMPtr<nsIContent> aContent = do_QueryInterface(aElement);

  *aIsFocusable = CheckIfFocusable(aContent, aFlags) != nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::MoveFocus(nsIDOMWindow* aWindow, nsIDOMElement* aStartElement,
                          PRUint32 aType, PRUint32 aFlags, nsIDOMElement** aElement)
{
  *aElement = nsnull;

#ifdef DEBUG_FOCUS
  printf("<<MoveFocus Type: %d Flags: %x>>\n<<", aType, aFlags);

  nsCOMPtr<nsPIDOMWindow> focusedWindow = mFocusedWindow;
  if (focusedWindow) {
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(focusedWindow->GetExtantDocument());
    if (doc) {
      nsCAutoString spec;
      doc->GetDocumentURI()->GetSpec(spec);
      printf(" [%p] Focused Window: %s", mFocusedWindow.get(), spec.get());
    }
  }
  PRINTTAGF(">> $[[%s]]\n", mFocusedContent);
#endif

  // use FLAG_BYMOVEFOCUS when switching focus with MoveFocus unless one of
  // the other focus methods is already set, or we're just moving to the root
  // or caret position.
  if (aType != MOVEFOCUS_ROOT && aType != MOVEFOCUS_CARET &&
      (aFlags & FOCUSMETHOD_MASK) == 0) {
    aFlags |= FLAG_BYMOVEFOCUS;
  }

  nsCOMPtr<nsPIDOMWindow> window;
  nsCOMPtr<nsIContent> startContent;
  if (aStartElement) {
    startContent = do_QueryInterface(aStartElement);
    NS_ENSURE_TRUE(startContent, NS_ERROR_INVALID_ARG);

    window = GetCurrentWindow(startContent);
  }
  else {
    window = aWindow ? do_QueryInterface(aWindow) : mFocusedWindow;
    NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);
    window = window->GetOuterWindow();
  }

  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsCOMPtr<nsIContent> newFocus;
  nsresult rv = DetermineElementToMoveFocus(window, startContent, aType,
                                            getter_AddRefs(newFocus));
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG_FOCUS_NAVIGATION
  PRINTTAGF("-> Element to be focused: %s\n", newFocus);
#endif

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

#ifdef DEBUG_FOCUS
  printf("<<MoveFocus end>>\n");
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::ClearFocus(nsIDOMWindow* aWindow)
{
#ifdef DEBUG_FOCUS
  printf("<<ClearFocus begin>>\n");
#endif

  // if the window to clear is the focused window or an ancestor of the
  // focused window, then blur the existing focused content. Otherwise, the
  // focus is somewhere else so just update the current node.
  nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(aWindow));
  NS_ENSURE_TRUE(window, NS_ERROR_INVALID_ARG);

  window = window->GetOuterWindow();
  NS_ENSURE_TRUE(window, NS_ERROR_INVALID_ARG);

  if (IsSameOrAncestor(window, mFocusedWindow)) {
    bool isAncestor = (window != mFocusedWindow);
    if (Blur(window, nsnull, isAncestor, true)) {
      // if we are clearing the focus on an ancestor of the focused window,
      // the ancestor will become the new focused window, so focus it
      if (isAncestor)
        Focus(window, nsnull, 0, true, false, false, true);
    }
  }
  else {
    window->SetFocusedNode(nsnull);
  }

#ifdef DEBUG_FOCUS
  printf("<<ClearFocus end>>\n");
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::GetFocusedElementForWindow(nsIDOMWindow* aWindow,
                                           bool aDeep,
                                           nsIDOMWindow** aFocusedWindow,
                                           nsIDOMElement** aElement)
{
  *aElement = nsnull;
  if (aFocusedWindow)
    *aFocusedWindow = nsnull;

  nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(aWindow));
  NS_ENSURE_TRUE(window, NS_ERROR_INVALID_ARG);

  window = window->GetOuterWindow();
  NS_ENSURE_TRUE(window, NS_ERROR_INVALID_ARG);

  nsCOMPtr<nsPIDOMWindow> focusedWindow;
  nsCOMPtr<nsIContent> focusedContent =
    GetFocusedDescendant(window, aDeep, getter_AddRefs(focusedWindow));
  if (focusedContent)
    CallQueryInterface(focusedContent, aElement);

  if (aFocusedWindow)
    NS_IF_ADDREF(*aFocusedWindow = focusedWindow);

  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::MoveCaretToFocus(nsIDOMWindow* aWindow)
{
  PRInt32 itemType = nsIDocShellTreeItem::typeChrome;

  nsCOMPtr<nsIWebNavigation> webnav = do_GetInterface(aWindow);
  nsCOMPtr<nsIDocShellTreeItem> dsti = do_QueryInterface(webnav);
  if (dsti) {
    dsti->GetItemType(&itemType);
    if (itemType != nsIDocShellTreeItem::typeChrome) {
      // don't move the caret for editable documents
      nsCOMPtr<nsIEditorDocShell> editorDocShell(do_QueryInterface(dsti));
      if (editorDocShell) {
        bool isEditable;
        editorDocShell->GetEditable(&isEditable);
        if (isEditable)
          return NS_OK;
      }

      nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(dsti);
      NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

      nsCOMPtr<nsIPresShell> presShell;
      docShell->GetPresShell(getter_AddRefs(presShell));
      NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

      nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(aWindow));
      nsCOMPtr<nsIContent> content = window->GetFocusedNode();
      if (content)
        MoveCaretToFocus(presShell, content);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::WindowRaised(nsIDOMWindow* aWindow)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aWindow);
  NS_ENSURE_TRUE(window && window->IsOuterWindow(), NS_ERROR_INVALID_ARG);

#ifdef DEBUG_FOCUS
  printf("Window %p Raised [Currently: %p %p] <<", aWindow, mActiveWindow.get(), mFocusedWindow.get());
  nsCAutoString spec;
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(window->GetExtantDocument());
  if (doc) {
    doc->GetDocumentURI()->GetSpec(spec);
    printf("[%p] Raised Window: %s", aWindow, spec.get());
  }
  if (mActiveWindow) {
    doc = do_QueryInterface(mActiveWindow->GetExtantDocument());
    if (doc) {
      doc->GetDocumentURI()->GetSpec(spec);
      printf(" [%p] Active Window: %s", mActiveWindow.get(), spec.get());
    }
  }
  printf(">>\n");
#endif

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

  nsCOMPtr<nsIWebNavigation> webnav(do_GetInterface(aWindow));
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(webnav));
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

    baseWindow->SetVisibility(true);
  }

  // inform the DOM window that it has activated, so that the active attribute
  // is updated on the window
  window->ActivateOrDeactivate(true);

  // send activate event
  nsCOMPtr<nsIDocument> document = do_QueryInterface(window->GetExtantDocument());
  nsContentUtils::DispatchTrustedEvent(document,
                                       window,
                                       NS_LITERAL_STRING("activate"),
                                       true, true, nsnull);

  // retrieve the last focused element within the window that was raised
  nsCOMPtr<nsPIDOMWindow> currentWindow;
  nsCOMPtr<nsIContent> currentFocus =
    GetFocusedDescendant(window, true, getter_AddRefs(currentWindow));

  NS_ASSERTION(currentWindow, "window raised with no window current");
  if (!currentWindow)
    return NS_OK;

  nsCOMPtr<nsIDocShell> currentDocShell = currentWindow->GetDocShell();

  nsCOMPtr<nsIPresShell> presShell;
  currentDocShell->GetPresShell(getter_AddRefs(presShell));
  if (presShell) {
    // disable selection mousedown state on activation
    // XXXndeakin P3 not sure if this is necessary, but it doesn't hurt
    nsRefPtr<nsFrameSelection> frameSelection = presShell->FrameSelection();
    frameSelection->SetMouseDownState(false);
  }

  Focus(currentWindow, currentFocus, 0, true, false, true, true);

  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::WindowLowered(nsIDOMWindow* aWindow)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aWindow);
  NS_ENSURE_TRUE(window && window->IsOuterWindow(), NS_ERROR_INVALID_ARG);

#ifdef DEBUG_FOCUS
  printf("Window %p Lowered [Currently: %p %p] <<", aWindow, mActiveWindow.get(), mFocusedWindow.get());
  nsCAutoString spec;
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(window->GetExtantDocument());
  if (doc) {
    doc->GetDocumentURI()->GetSpec(spec);
    printf("[%p] Lowered Window: %s", aWindow, spec.get());
  }
  if (mActiveWindow) {
    doc = do_QueryInterface(mActiveWindow->GetExtantDocument());
    if (doc) {
      doc->GetDocumentURI()->GetSpec(spec);
      printf(" [%p] Active Window: %s", mActiveWindow.get(), spec.get());
    }
  }
  printf(">>\n");
#endif

  if (mActiveWindow != window)
    return NS_OK;

  // clear the mouse capture as the active window has changed
  nsIPresShell::SetCapturingContent(nsnull, 0);

  // inform the DOM window that it has deactivated, so that the active
  // attribute is updated on the window
  window->ActivateOrDeactivate(false);

  // send deactivate event
  nsCOMPtr<nsIDocument> document = do_QueryInterface(window->GetExtantDocument());
  nsContentUtils::DispatchTrustedEvent(document,
                                       window,
                                       NS_LITERAL_STRING("deactivate"),
                                       true, true, nsnull);

  // keep track of the window being lowered, so that attempts to raise the
  // window can be prevented until we return. Otherwise, focus can get into
  // an unusual state.
  mWindowBeingLowered = mActiveWindow;
  mActiveWindow = nsnull;

  if (mFocusedWindow)
    Blur(nsnull, nsnull, true, true);

  mWindowBeingLowered = nsnull;

  return NS_OK;
}

nsresult
nsFocusManager::ContentRemoved(nsIDocument* aDocument, nsIContent* aContent)
{
  NS_ENSURE_ARG(aDocument);
  NS_ENSURE_ARG(aContent);

  nsPIDOMWindow *window = aDocument->GetWindow();
  if (!window)
    return NS_OK;

  // if the content is currently focused in the window, or is an ancestor
  // of the currently focused element, reset the focus within that window.
  nsIContent* content = window->GetFocusedNode();
  if (content && nsContentUtils::ContentIsDescendantOf(content, aContent)) {
    bool shouldShowFocusRing = window->ShouldShowFocusRing();
    window->SetFocusedNode(nsnull);

    nsCOMPtr<nsIDocShell> docShell = window->GetDocShell();
    if (docShell) {
      nsCOMPtr<nsIPresShell> presShell;
      docShell->GetPresShell(getter_AddRefs(presShell));
      nsIMEStateManager::OnRemoveContent(presShell->GetPresContext(), content);
    }

    // if this window is currently focused, clear the global focused
    // element as well, but don't fire any events.
    if (window == mFocusedWindow) {
      mFocusedContent = nsnull;
    }
    else {
      // Check if the node that was focused is an iframe or similar by looking
      // if it has a subdocument. This would indicate that this focused iframe
      // and its descendants will be going away. We will need to move the
      // focus somewhere else, so just clear the focus in the toplevel window
      // so that no element is focused.
      nsIDocument* subdoc = aDocument->GetSubDocumentFor(content);
      if (subdoc) {
        nsCOMPtr<nsISupports> container = subdoc->GetContainer();
        nsCOMPtr<nsPIDOMWindow> childWindow = do_GetInterface(container);
        if (childWindow && IsSameOrAncestor(childWindow, mFocusedWindow)) {
          ClearFocus(mActiveWindow);
        }
      }
    }

    NotifyFocusStateChange(aContent, shouldShowFocusRing, false);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::WindowShown(nsIDOMWindow* aWindow, bool aNeedsFocus)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_INVALID_ARG);

  window = window->GetOuterWindow();

#ifdef DEBUG_FOCUS
  printf("Window %p Shown [Currently: %p %p] <<", window.get(), mActiveWindow.get(), mFocusedWindow.get());
  nsCAutoString spec;
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(window->GetExtantDocument());
  if (doc) {
    doc->GetDocumentURI()->GetSpec(spec);
    printf("Shown Window: %s", spec.get());
  }

  if (mFocusedWindow) {
    doc = do_QueryInterface(mFocusedWindow->GetExtantDocument());
    if (doc) {
      doc->GetDocumentURI()->GetSpec(spec);
      printf(" Focused Window: %s", spec.get());
    }
  }
  printf(">>\n");
#endif

  if (mFocusedWindow != window)
    return NS_OK;

  if (aNeedsFocus) {
    nsCOMPtr<nsPIDOMWindow> currentWindow;
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
nsFocusManager::WindowHidden(nsIDOMWindow* aWindow)
{
  // if there is no window or it is not the same or an ancestor of the
  // currently focused window, just return, as the current focus will not
  // be affected.

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_INVALID_ARG);

  window = window->GetOuterWindow();

#ifdef DEBUG_FOCUS
  printf("Window %p Hidden [Currently: %p %p] <<", window.get(), mActiveWindow.get(), mFocusedWindow.get());
  nsCAutoString spec;
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(window->GetExtantDocument());
  if (doc) {
    doc->GetDocumentURI()->GetSpec(spec);
    printf("Hide Window: %s", spec.get());
  }

  if (mFocusedWindow) {
    doc = do_QueryInterface(mFocusedWindow->GetExtantDocument());
    if (doc) {
      doc->GetDocumentURI()->GetSpec(spec);
      printf(" Focused Window: %s", spec.get());
    }
  }

  if (mActiveWindow) {
    doc = do_QueryInterface(mActiveWindow->GetExtantDocument());
    if (doc) {
      doc->GetDocumentURI()->GetSpec(spec);
      printf(" Active Window: %s", spec.get());
    }
  }
  printf(">>\n");
#endif

  if (!IsSameOrAncestor(window, mFocusedWindow))
    return NS_OK;

  // at this point, we know that the window being hidden is either the focused
  // window, or an ancestor of the focused window. Either way, the focus is no
  // longer valid, so it needs to be updated.

  nsIContent* oldFocusedContent = mFocusedContent;
  mFocusedContent = nsnull;

  if (oldFocusedContent && oldFocusedContent->IsInDoc()) {
    NotifyFocusStateChange(oldFocusedContent,
                           mFocusedWindow->ShouldShowFocusRing(),
                           false);
  }

  nsCOMPtr<nsIDocShell> focusedDocShell = mFocusedWindow->GetDocShell();
  nsCOMPtr<nsIPresShell> presShell;
  focusedDocShell->GetPresShell(getter_AddRefs(presShell));

  nsIMEStateManager::OnTextStateBlur(nsnull, nsnull);
  if (presShell) {
    nsIMEStateManager::OnChangeFocus(presShell->GetPresContext(), nsnull, IMEContext::FOCUS_REMOVED);
    SetCaretVisible(presShell, false, nsnull);
  }

  // if the docshell being hidden is being destroyed, then we want to move
  // focus somewhere else. Call ClearFocus on the toplevel window, which
  // will have the effect of clearing the focus and moving the focused window
  // to the toplevel window. But if the window isn't being destroyed, we are
  // likely just loading a new document in it, so we want to maintain the
  // focused window so that the new document gets properly focused.
  bool beingDestroyed;
  nsCOMPtr<nsIDocShell> docShellBeingHidden = window->GetDocShell();
  docShellBeingHidden->IsBeingDestroyed(&beingDestroyed);
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
    nsCOMPtr<nsIWebNavigation> webnav(do_GetInterface(mFocusedWindow));
    nsCOMPtr<nsIDocShellTreeItem> dsti = do_QueryInterface(webnav);
    if (dsti) {
      nsCOMPtr<nsIDocShellTreeItem> parentDsti;
      dsti->GetParent(getter_AddRefs(parentDsti));
      nsCOMPtr<nsPIDOMWindow> parentWindow = do_GetInterface(parentDsti);
      if (parentWindow)
        parentWindow->SetFocusedNode(nsnull);
    }

    mFocusedWindow = window;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFocusManager::FireDelayedEvents(nsIDocument* aDocument)
{
  NS_ENSURE_ARG(aDocument);

  // fire any delayed focus and blur events in the same order that they were added
  for (PRUint32 i = 0; i < mDelayedBlurFocusEvents.Length(); i++)
  {
    if (mDelayedBlurFocusEvents[i].mDocument == aDocument &&
        !aDocument->EventHandlingSuppressed()) {
      PRUint32 type = mDelayedBlurFocusEvents[i].mType;
      nsCOMPtr<nsIDOMEventTarget> target = mDelayedBlurFocusEvents[i].mTarget;
      nsCOMPtr<nsIPresShell> presShell = mDelayedBlurFocusEvents[i].mPresShell;
      mDelayedBlurFocusEvents.RemoveElementAt(i);
      SendFocusOrBlurEvent(type, presShell, aDocument, target, 0, false);
      --i;
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

/* static */
void
nsFocusManager::NotifyFocusStateChange(nsIContent* aContent,
                                       bool aWindowShouldShowFocusRing,
                                       bool aGettingFocus)
{
  if (!aContent->IsElement()) {
    return;
  }
  nsEventStates eventState = NS_EVENT_STATE_FOCUS;
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
  if (!mFocusedWindow)
    return;

  // get the main child widget for the focused window and ensure that the
  // platform knows that this widget is focused.
  nsCOMPtr<nsIDocShell> docShell = mFocusedWindow->GetDocShell();
  if (docShell) {
    nsCOMPtr<nsIPresShell> presShell;
    docShell->GetPresShell(getter_AddRefs(presShell));
    if (presShell) {
      nsIViewManager* vm = presShell->GetViewManager();
      if (vm) {
        nsCOMPtr<nsIWidget> widget;
        vm->GetRootWidget(getter_AddRefs(widget));
        if (widget)
          widget->SetFocus(false);
      }
    }
  }
}

void
nsFocusManager::SetFocusInner(nsIContent* aNewContent, PRInt32 aFlags,
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
  nsCOMPtr<nsPIDOMWindow> newWindow;
  nsCOMPtr<nsPIDOMWindow> subWindow = GetContentWindow(contentToFocus);
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

    nsCOMPtr<nsIDocShellTreeItem> dsti = do_QueryInterface(docShell);
    nsCOMPtr<nsIDocShellTreeItem> parentDsti;
    dsti->GetParent(getter_AddRefs(parentDsti));
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
    if (!subsumes && !nsContentUtils::IsCallerTrustedForWrite()) {
      NS_WARNING("Not allowed to focus the new window!");
      return;
    }
  }

  // to check if the new element is in the active window, compare the
  // new root docshell for the new element with the active window's docshell.
  bool isElementInActiveWindow = false;

  nsCOMPtr<nsIWebNavigation> webnav = do_GetInterface(newWindow);
  nsCOMPtr<nsIDocShellTreeItem> dsti = do_QueryInterface(webnav);
  nsCOMPtr<nsPIDOMWindow> newRootWindow;
  if (dsti) {
    nsCOMPtr<nsIDocShellTreeItem> root;
    dsti->GetRootTreeItem(getter_AddRefs(root));
    newRootWindow = do_GetInterface(root);

    isElementInActiveWindow = (mActiveWindow && newRootWindow == mActiveWindow);
  }

#ifdef DEBUG_FOCUS
  PRINTTAGF("Shift Focus: %s", contentToFocus);
  printf(" Flags: %x Current Window: %p New Window: %p Current Element: %p",
         aFlags, mFocusedWindow.get(), newWindow.get(), mFocusedContent.get());
  printf(" In Active Window: %d In Focused Window: %d\n",
         isElementInActiveWindow, isElementInFocusedWindow);
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
  if (sendFocusEvent && mFocusedContent &&
      mFocusedContent->OwnerDoc() != aNewContent->OwnerDoc()) {
    // If the caller cannot access the current focused node, the caller should
    // not be able to steal focus from it. E.g., When the current focused node
    // is in chrome, any web contents should not be able to steal the focus.
    nsCOMPtr<nsIDOMNode> domNode(do_QueryInterface(mFocusedContent));
    sendFocusEvent = nsContentUtils::CanCallerAccess(domNode);
    if (!sendFocusEvent && mMouseDownEventHandlingDocument) {
      // However, while mouse down event is handling, the handling document's
      // script should be able to steal focus.
      domNode = do_QueryInterface(mMouseDownEventHandlingDocument);
      sendFocusEvent = nsContentUtils::CanCallerAccess(domNode);
    }
  }

  if (sendFocusEvent) {
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
      nsCOMPtr<nsPIDOMWindow> commonAncestor;
      if (!isElementInFocusedWindow)
        commonAncestor = GetCommonAncestor(newWindow, mFocusedWindow);

      if (!Blur(currentIsSameOrAncestor ? mFocusedWindow.get() : nsnull,
                commonAncestor, !isElementInFocusedWindow, aAdjustWidget))
        return;
    }

    Focus(newWindow, contentToFocus, aFlags, !isElementInFocusedWindow,
          aFocusChanged, false, aAdjustWidget);
  }
  else {
    // otherwise, for inactive windows and when the caller cannot steal the
    // focus, update the node in the window, and  raise the window if desired.
    if (allowFrameSwitch)
      AdjustWindowFocus(newWindow, true);

    // set the focus node and method as needed
    PRUint32 focusMethod = aFocusChanged ? aFlags & FOCUSMETHODANDRING_MASK :
                           newWindow->GetFocusMethod() | (aFlags & FLAG_SHOWRING);
    newWindow->SetFocusedNode(contentToFocus, focusMethod);
    if (aFocusChanged) {
      nsCOMPtr<nsIDocShell> docShell = newWindow->GetDocShell();

      nsCOMPtr<nsIPresShell> presShell;
      docShell->GetPresShell(getter_AddRefs(presShell));
      if (presShell)
        ScrollIntoView(presShell, contentToFocus, aFlags);
    }

    // update the commands even when inactive so that the attributes for that
    // window are up to date.
    if (allowFrameSwitch)
      newWindow->UpdateCommands(NS_LITERAL_STRING("focus"));

    if (aFlags & FLAG_RAISE)
      RaiseWindow(newRootWindow);
  }
}

bool
nsFocusManager::IsSameOrAncestor(nsPIDOMWindow* aPossibleAncestor,
                                 nsPIDOMWindow* aWindow)
{
  nsCOMPtr<nsIWebNavigation> awebnav(do_GetInterface(aPossibleAncestor));
  nsCOMPtr<nsIDocShellTreeItem> ancestordsti = do_QueryInterface(awebnav);

  nsCOMPtr<nsIWebNavigation> fwebnav(do_GetInterface(aWindow));
  nsCOMPtr<nsIDocShellTreeItem> dsti = do_QueryInterface(fwebnav);
  while (dsti) {
    if (dsti == ancestordsti)
      return true;
    nsCOMPtr<nsIDocShellTreeItem> parentDsti;
    dsti->GetParent(getter_AddRefs(parentDsti));
    dsti.swap(parentDsti);
  }

  return false;
}

already_AddRefed<nsPIDOMWindow>
nsFocusManager::GetCommonAncestor(nsPIDOMWindow* aWindow1,
                                  nsPIDOMWindow* aWindow2)
{
  nsCOMPtr<nsIWebNavigation> webnav(do_GetInterface(aWindow1));
  nsCOMPtr<nsIDocShellTreeItem> dsti1 = do_QueryInterface(webnav);
  NS_ENSURE_TRUE(dsti1, nsnull);

  webnav = do_GetInterface(aWindow2);
  nsCOMPtr<nsIDocShellTreeItem> dsti2 = do_QueryInterface(webnav);
  NS_ENSURE_TRUE(dsti2, nsnull);

  nsAutoTArray<nsIDocShellTreeItem*, 30> parents1, parents2;
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

  PRUint32 pos1 = parents1.Length();
  PRUint32 pos2 = parents2.Length();
  nsIDocShellTreeItem* parent = nsnull;
  PRUint32 len;
  for (len = NS_MIN(pos1, pos2); len > 0; --len) {
    nsIDocShellTreeItem* child1 = parents1.ElementAt(--pos1);
    nsIDocShellTreeItem* child2 = parents2.ElementAt(--pos2);
    if (child1 != child2) {
      break;
    }
    parent = child1;
  }

  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(parent);
  return window.forget();
}

void
nsFocusManager::AdjustWindowFocus(nsPIDOMWindow* aWindow,
                                  bool aCheckPermission)
{
  bool isVisible = IsWindowVisible(aWindow);

  nsCOMPtr<nsPIDOMWindow> window(aWindow);
  while (window) {
    // get the containing <iframe> or equivalent element so that it can be
    // focused below.
    nsCOMPtr<nsIContent> frameContent =
      do_QueryInterface(window->GetFrameElementInternal());

    nsCOMPtr<nsIWebNavigation> webnav(do_GetInterface(window));
    nsCOMPtr<nsIDocShellTreeItem> dsti = do_QueryInterface(webnav);
    if (!dsti) 
      return;
    nsCOMPtr<nsIDocShellTreeItem> parentDsti;
    dsti->GetParent(getter_AddRefs(parentDsti));

    window = do_GetInterface(parentDsti);
    if (window) {
      // if the parent window is visible but aWindow was not, then we have
      // likely moved up and out from a hidden tab to the browser window, or a
      // similar such arrangement. Stop adjusting the current nodes.
      if (IsWindowVisible(window) != isVisible)
        break;

      // When aCheckPermission is true, we should check whether the caller can
      // access the window or not.  If it cannot access, we should stop the
      // adjusting.
      if (aCheckPermission && !nsContentUtils::CanCallerAccess(window))
        break;

      window->SetFocusedNode(frameContent);
    }
  }
}

bool
nsFocusManager::IsWindowVisible(nsPIDOMWindow* aWindow)
{
  if (!aWindow)
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
  NS_PRECONDITION(aContent->IsInDoc(), "aContent must be in a document");

  // If aContent is in designMode, the root element is not focusable.
  // NOTE: in designMode, most elements are not focusable, just the document is
  //       focusable.
  // Also, if aContent is not editable but it isn't in designMode, it's not
  // focusable.
  nsIDocument* doc = aContent->GetCurrentDoc();
  NS_ASSERTION(doc, "aContent must have current document");
  return aContent == doc->GetRootElement() &&
           (doc->HasFlag(NODE_IS_EDITABLE) || !aContent->IsEditable());
}

nsIContent*
nsFocusManager::CheckIfFocusable(nsIContent* aContent, PRUint32 aFlags)
{
  if (!aContent)
    return nsnull;

  // this is a special case for some XUL elements where an anonymous child is
  // actually focusable and not the element itself.
  nsIContent* redirectedFocus = GetRedirectedFocus(aContent);
  if (redirectedFocus)
    return CheckIfFocusable(redirectedFocus, aFlags);

  nsCOMPtr<nsIDocument> doc = aContent->GetCurrentDoc();
  // can't focus elements that are not in documents
  if (!doc)
    return nsnull;

  // Make sure that our frames are up to date
  doc->FlushPendingNotifications(Flush_Layout);

  nsIPresShell *shell = doc->GetShell();
  if (!shell)
    return nsnull;

  // the root content can always be focused
  if (aContent == doc->GetRootElement())
    return aContent;

  // cannot focus content in print preview mode. Only the root can be focused.
  nsPresContext* presContext = shell->GetPresContext();
  if (presContext && presContext->Type() == nsPresContext::eContext_PrintPreview)
    return nsnull;

  nsIFrame* frame = aContent->GetPrimaryFrame();
  if (!frame)
    return nsnull;

  if (aContent->Tag() == nsGkAtoms::area && aContent->IsHTML()) {
    // HTML areas do not have their own frame, and the img frame we get from
    // GetPrimaryFrame() is not relevant as to whether it is focusable or
    // not, so we have to do all the relevant checks manually for them.
    return frame->IsVisibleConsideringAncestors() &&
           aContent->IsFocusable() ? aContent : nsnull;
  }

  // if this is a child frame content node, check if it is visible and
  // call the content node's IsFocusable method instead of the frame's
  // IsFocusable method. This skips checking the style system and ensures that
  // offscreen browsers can still be focused.
  nsIDocument* subdoc = doc->GetSubDocumentFor(aContent);
  if (subdoc && IsWindowVisible(subdoc->GetWindow())) {
    const nsStyleUserInterface* ui = frame->GetStyleUserInterface();
    PRInt32 tabIndex = (ui->mUserFocus == NS_STYLE_USER_FOCUS_IGNORE ||
                        ui->mUserFocus == NS_STYLE_USER_FOCUS_NONE) ? -1 : 0;
    return aContent->IsFocusable(&tabIndex, aFlags & FLAG_BYMOUSE) ? aContent : nsnull;
  }
  
  return frame->IsFocusable(nsnull, aFlags & FLAG_BYMOUSE) ? aContent : nsnull;
}

bool
nsFocusManager::Blur(nsPIDOMWindow* aWindowToClear,
                     nsPIDOMWindow* aAncestorWindowToFocus,
                     bool aIsLeavingDocument,
                     bool aAdjustWidgets)
{
  // hold a reference to the focused content, which may be null
  nsCOMPtr<nsIContent> content = mFocusedContent;
  if (content) {
    if (!content->IsInDoc()) {
      mFocusedContent = nsnull;
      return true;
    }
    if (content == mFirstBlurEvent)
      return true;
  }

  // hold a reference to the focused window
  nsCOMPtr<nsPIDOMWindow> window = mFocusedWindow;
  if (!window) {
    mFocusedContent = nsnull;
    return true;
  }

  nsCOMPtr<nsIDocShell> docShell = window->GetDocShell();
  if (!docShell) {
    mFocusedContent = nsnull;
    return true;
  }

  // Keep a ref to presShell since dispatching the DOM event may cause
  // the document to be destroyed.
  nsCOMPtr<nsIPresShell> presShell;
  docShell->GetPresShell(getter_AddRefs(presShell));
  if (!presShell) {
    mFocusedContent = nsnull;
    return true;
  }

  bool clearFirstBlurEvent = false;
  if (!mFirstBlurEvent) {
    mFirstBlurEvent = content;
    clearFirstBlurEvent = true;
  }

  // if there is still an active window, adjust the IME state.
  // This has to happen before the focus is cleared below, otherwise, the IME
  // compositionend event won't get fired at the element being blurred.
  nsIMEStateManager::OnTextStateBlur(nsnull, nsnull);
  if (mActiveWindow)
    nsIMEStateManager::OnChangeFocus(presShell->GetPresContext(), nsnull, IMEContext::FOCUS_REMOVED);

  // now adjust the actual focus, by clearing the fields in the focus manager
  // and in the window.
  mFocusedContent = nsnull;
  bool shouldShowFocusRing = window->ShouldShowFocusRing();
  if (aWindowToClear)
    aWindowToClear->SetFocusedNode(nsnull);

#ifdef DEBUG_FOCUS
  PRINTTAGF("**Element %s has been blurred\n", content);
#endif

  // Don't fire blur event on the root content which isn't editable.
  bool sendBlurEvent =
    content && content->IsInDoc() && !IsNonFocusableRoot(content);
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
      if (aAdjustWidgets && objectFrame) {
        // note that the presshell's widget is being retrieved here, not the one
        // for the object frame.
        nsIViewManager* vm = presShell->GetViewManager();
        if (vm) {
          nsCOMPtr<nsIWidget> widget;
          vm->GetRootWidget(getter_AddRefs(widget));
          if (widget)
            widget->SetFocus(false);
        }
      }

      // if the object being blurred is a remote browser, deactivate remote content
      TabParent* remote = GetRemoteForContent(content);
      if (remote) {
        remote->Deactivate();
  #ifdef DEBUG_FOCUS
      printf("*Remote browser deactivated\n");
  #endif
      }
    }
  }

  bool result = true;
  if (sendBlurEvent) {
    // if there is an active window, update commands. If there isn't an active
    // window, then this was a blur caused by the active window being lowered,
    // so there is no need to update the commands
    if (mActiveWindow)
      window->UpdateCommands(NS_LITERAL_STRING("focus"));

    SendFocusOrBlurEvent(NS_BLUR_CONTENT, presShell,
                         content->GetCurrentDoc(), content, 1, false);
  }

  // if we are leaving the document or the window was lowered, make the caret
  // invisible.
  if (aIsLeavingDocument || !mActiveWindow)
    SetCaretVisible(presShell, false, nsnull);

  // at this point, it is expected that this window will be still be
  // focused, but the focused content will be null, as it was cleared before
  // the event. If this isn't the case, then something else was focused during
  // the blur event above and we should just return. However, if
  // aIsLeavingDocument is set, a new document is desired, so make sure to
  // blur the document and window.
  if (mFocusedWindow != window ||
      (mFocusedContent != nsnull && !aIsLeavingDocument)) {
    result = false;
  }
  else if (aIsLeavingDocument) {
    window->TakeFocus(false, 0);

    // clear the focus so that the ancestor frame hierarchy is in the correct
    // state. Pass true because aAncestorWindowToFocus is thought to be
    // focused at this point.
    if (aAncestorWindowToFocus)
      aAncestorWindowToFocus->SetFocusedNode(nsnull, 0, true);

    mFocusedWindow = nsnull;
    mFocusedContent = nsnull;

    // pass 1 for the focus method when calling SendFocusOrBlurEvent just so
    // that the check is made for suppressed documents. Check to ensure that
    // the document isn't null in case someone closed it during the blur above
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(window->GetExtantDocument());
    if (doc)
      SendFocusOrBlurEvent(NS_BLUR_CONTENT, presShell, doc, doc, 1, false);
    if (mFocusedWindow == nsnull)
      SendFocusOrBlurEvent(NS_BLUR_CONTENT, presShell, doc, window, 1, false);

    // check if a different window was focused
    result = (mFocusedWindow == nsnull && mActiveWindow);
  }
  else if (mActiveWindow) {
    // Otherwise, the blur of the element without blurring the document
    // occurred normally. Call UpdateCaret to redisplay the caret at the right
    // location within the document. This is needed to ensure that the caret
    // used for caret browsing is made visible again when an input field is
    // blurred.
    UpdateCaret(false, true, nsnull);
  }

  if (clearFirstBlurEvent)
    mFirstBlurEvent = nsnull;

  return result;
}

void
nsFocusManager::Focus(nsPIDOMWindow* aWindow,
                      nsIContent* aContent,
                      PRUint32 aFlags,
                      bool aIsNewDocument,
                      bool aFocusChanged,
                      bool aWindowRaised,
                      bool aAdjustWidgets)
{
  if (!aWindow)
    return;

  if (aContent && (aContent == mFirstFocusEvent || aContent == mFirstBlurEvent))
    return;

  // Keep a reference to the presShell since dispatching the DOM event may
  // cause the document to be destroyed.
  nsCOMPtr<nsIDocShell> docShell = aWindow->GetDocShell();
  if (!docShell)
    return;

  nsCOMPtr<nsIPresShell> presShell;
  docShell->GetPresShell(getter_AddRefs(presShell));
  if (!presShell)
    return;

  // If the focus actually changed, set the focus method (mouse, keyboard, etc).
  // Otherwise, just get the current focus method and use that. This ensures
  // that the method is set during the document and window focus events.
  PRUint32 focusMethod = aFocusChanged ? aFlags & FOCUSMETHODANDRING_MASK :
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

#ifdef DEBUG_FOCUS
  PRINTTAGF("**Element %s has been focused", aContent);
  nsCOMPtr<nsIDocument> docm = do_QueryInterface(aWindow->GetExtantDocument());
  if (docm)
    PRINTTAGF(" from %s", docm->GetRootElement());
  printf(" [Newdoc: %d FocusChanged: %d Raised: %d Flags: %x]\n",
         aIsNewDocument, aFocusChanged, aWindowRaised, aFlags);
#endif

  if (aIsNewDocument) {
    // if this is a new document, update the parent chain of frames so that
    // focus can be traversed from the top level down to the newly focused
    // window.
    AdjustWindowFocus(aWindow, false);

    // Update the window touch registration to reflect the state of
    // the new document that got focus
    aWindow->UpdateTouchState();
  }

  // indicate that the window has taken focus.
  if (aWindow->TakeFocus(true, focusMethod))
    aIsNewDocument = true;

  mFocusedWindow = aWindow;

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
  if (aAdjustWidgets && !objectFrameWidget) {
    nsIViewManager* vm = presShell->GetViewManager();
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
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(aWindow->GetExtantDocument());
    if (doc)
      SendFocusOrBlurEvent(NS_FOCUS_CONTENT, presShell, doc,
                           doc, aFlags & FOCUSMETHOD_MASK, aWindowRaised);
    if (mFocusedWindow == aWindow && mFocusedContent == nsnull)
      SendFocusOrBlurEvent(NS_FOCUS_CONTENT, presShell, doc,
                           aWindow, aFlags & FOCUSMETHOD_MASK, aWindowRaised);
  }

  // check to ensure that the element is still focusable, and that nothing
  // else was focused during the events above.
  if (CheckIfFocusable(aContent, aFlags) &&
      mFocusedWindow == aWindow && mFocusedContent == nsnull) {
    mFocusedContent = aContent;

    nsIContent* focusedNode = aWindow->GetFocusedNode();
    bool isRefocus = focusedNode && focusedNode->IsEqualTo(aContent);

    aWindow->SetFocusedNode(aContent, focusMethod);

    bool sendFocusEvent =
      aContent && aContent->IsInDoc() && !IsNonFocusableRoot(aContent);
    nsPresContext* presContext = presShell->GetPresContext();
    if (sendFocusEvent) {
      // if the focused element changed, scroll it into view
      if (aFocusChanged)
        ScrollIntoView(presShell, aContent, aFlags);

      NotifyFocusStateChange(aContent, aWindow->ShouldShowFocusRing(), true);

      // if this is an object/plug-in/remote browser, focus its widget.  Note that we might
      // no longer be in the same document, due to the events we fired above when
      // aIsNewDocument.
      if (presShell->GetDocument() == aContent->GetDocument()) {
        if (aAdjustWidgets && objectFrameWidget)
          objectFrameWidget->SetFocus(false);

        // if the object being focused is a remote browser, activate remote content
        TabParent* remote = GetRemoteForContent(aContent);
        if (remote) {
          remote->Activate();
#ifdef DEBUG_FOCUS
          printf("*Remote browser activated\n");
#endif
        }
      }

      PRUint32 reason = GetFocusMoveReason(aFlags);
      nsIMEStateManager::OnChangeFocus(presContext, aContent, reason);

      // as long as this focus wasn't because a window was raised, update the
      // commands
      // XXXndeakin P2 someone could adjust the focus during the update
      if (!aWindowRaised)
        aWindow->UpdateCommands(NS_LITERAL_STRING("focus"));

      SendFocusOrBlurEvent(NS_FOCUS_CONTENT, presShell,
                           aContent->GetCurrentDoc(),
                           aContent, aFlags & FOCUSMETHOD_MASK,
                           aWindowRaised, isRefocus);

      nsIMEStateManager::OnTextStateFocus(presContext, aContent);
    } else {
      nsIMEStateManager::OnTextStateBlur(presContext, nsnull);
      nsIMEStateManager::OnChangeFocus(presContext, nsnull, IMEContext::FOCUS_REMOVED);
      if (!aWindowRaised) {
        aWindow->UpdateCommands(NS_LITERAL_STRING("focus"));
      }
    }
  }
  else {
    // If the window focus event (fired above when aIsNewDocument) caused
    // the plugin not to be focusable, update the system focus by focusing
    // the root widget.
    if (aAdjustWidgets && objectFrameWidget &&
        mFocusedWindow == aWindow && mFocusedContent == nsnull) {
      nsIViewManager* vm = presShell->GetViewManager();
      if (vm) {
        nsCOMPtr<nsIWidget> widget;
        vm->GetRootWidget(getter_AddRefs(widget));
        if (widget)
          widget->SetFocus(false);
      }
    }

    nsPresContext* presContext = presShell->GetPresContext();
    nsIMEStateManager::OnTextStateBlur(presContext, nsnull);
    nsIMEStateManager::OnChangeFocus(presContext, nsnull, IMEContext::FOCUS_REMOVED);

    if (!aWindowRaised)
      aWindow->UpdateCommands(NS_LITERAL_STRING("focus"));
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
    mFirstFocusEvent = nsnull;
}

class FocusBlurEvent : public nsRunnable
{
public:
  FocusBlurEvent(nsISupports* aTarget, PRUint32 aType,
                 nsPresContext* aContext, bool aWindowRaised,
                 bool aIsRefocus)
  : mTarget(aTarget), mType(aType), mContext(aContext),
    mWindowRaised(aWindowRaised), mIsRefocus(aIsRefocus) {}

  NS_IMETHOD Run()
  {
    nsFocusEvent event(true, mType);
    event.flags |= NS_EVENT_FLAG_CANT_BUBBLE;
    event.fromRaise = mWindowRaised;
    event.isRefocus = mIsRefocus;
    return nsEventDispatcher::Dispatch(mTarget, mContext, &event);
  }

  nsCOMPtr<nsISupports>   mTarget;
  PRUint32                mType;
  nsRefPtr<nsPresContext> mContext;
  bool                    mWindowRaised;
  bool                    mIsRefocus;
};

void
nsFocusManager::SendFocusOrBlurEvent(PRUint32 aType,
                                     nsIPresShell* aPresShell,
                                     nsIDocument* aDocument,
                                     nsISupports* aTarget,
                                     PRUint32 aFocusMethod,
                                     bool aWindowRaised,
                                     bool aIsRefocus)
{
  NS_ASSERTION(aType == NS_FOCUS_CONTENT || aType == NS_BLUR_CONTENT,
               "Wrong event type for SendFocusOrBlurEvent");

  nsCOMPtr<nsIDOMEventTarget> eventTarget = do_QueryInterface(aTarget);

  // for focus events, if this event was from a mouse or key and event
  // handling on the document is suppressed, queue the event and fire it
  // later. For blur events, a non-zero value would be set for aFocusMethod.
  if (aFocusMethod && aDocument && aDocument->EventHandlingSuppressed()) {
    // aFlags is always 0 when aWindowRaised is true so this won't be called
    // on a window raise.
    NS_ASSERTION(!aWindowRaised, "aWindowRaised should not be set");

    for (PRUint32 i = mDelayedBlurFocusEvents.Length(); i > 0; --i) {
      // if this event was already queued, remove it and append it to the end
      if (mDelayedBlurFocusEvents[i - 1].mType == aType &&
          mDelayedBlurFocusEvents[i - 1].mPresShell == aPresShell &&
          mDelayedBlurFocusEvents[i - 1].mDocument == aDocument &&
          mDelayedBlurFocusEvents[i - 1].mTarget == eventTarget) {
        mDelayedBlurFocusEvents.RemoveElementAt(i - 1);
      }
    }

    mDelayedBlurFocusEvents.AppendElement(
      nsDelayedBlurOrFocusEvent(aType, aPresShell, aDocument, eventTarget));
    return;
  }

#ifdef ACCESSIBILITY
  nsAccessibilityService* accService = GetAccService();
  if (accService) {
    if (aType == NS_FOCUS_CONTENT)
      accService->NotifyOfDOMFocus(aTarget);
    else
      accService->NotifyOfDOMBlur(aTarget);
  }
#endif

  nsContentUtils::AddScriptRunner(
    new FocusBlurEvent(aTarget, aType, aPresShell->GetPresContext(),
                       aWindowRaised, aIsRefocus));
}

void
nsFocusManager::ScrollIntoView(nsIPresShell* aPresShell,
                               nsIContent* aContent,
                               PRUint32 aFlags)
{
  // if the noscroll flag isn't set, scroll the newly focused element into view
  if (!(aFlags & FLAG_NOSCROLL))
    aPresShell->ScrollContentIntoView(aContent,
                                      NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE,
                                      NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE,
                                      nsIPresShell::SCROLL_OVERFLOW_HIDDEN);
}


void
nsFocusManager::RaiseWindow(nsPIDOMWindow* aWindow)
{
  // don't raise windows that are already raised or are in the process of
  // being lowered
  if (!aWindow || aWindow == mActiveWindow || aWindow == mWindowBeingLowered)
    return;

#if defined(XP_WIN) || defined(XP_OS2)
  // Windows would rather we focus the child widget, otherwise, the toplevel
  // widget will always end up being focused. Fortunately, focusing the child
  // widget will also have the effect of raising the window this widget is in.
  // But on other platforms, we can just focus the toplevel widget to raise
  // the window.
  nsCOMPtr<nsPIDOMWindow> childWindow;
  GetFocusedDescendant(aWindow, true, getter_AddRefs(childWindow));
  if (!childWindow)
    childWindow = aWindow;

  nsCOMPtr<nsIDocShell> docShell = aWindow->GetDocShell();
  if (!docShell)
    return;

  nsCOMPtr<nsIPresShell> presShell;
  docShell->GetPresShell(getter_AddRefs(presShell));
  if (!presShell)
    return;

  nsIViewManager* vm = presShell->GetViewManager();
  if (vm) {
    nsCOMPtr<nsIWidget> widget;
    vm->GetRootWidget(getter_AddRefs(widget));
    if (widget)
      widget->SetFocus(true);
  }
#else
  nsCOMPtr<nsIWebNavigation> webnav = do_GetInterface(aWindow);
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin = do_QueryInterface(webnav);
  if (treeOwnerAsWin) {
    nsCOMPtr<nsIWidget> widget;
    treeOwnerAsWin->GetMainWidget(getter_AddRefs(widget));
    if (widget)
      widget->SetFocus(true);
  }
#endif
}

void
nsFocusManager::UpdateCaret(bool aMoveCaretToFocus,
                            bool aUpdateVisibility,
                            nsIContent* aContent)
{
#ifdef DEBUG_FOCUS
  printf("Update Caret: %d %d\n", aMoveCaretToFocus, aUpdateVisibility);
#endif

  if (!mFocusedWindow)
    return;

  // this is called when a document is focused or when the caretbrowsing
  // preference is changed
  nsCOMPtr<nsIDocShell> focusedDocShell = mFocusedWindow->GetDocShell();
  nsCOMPtr<nsIDocShellTreeItem> dsti = do_QueryInterface(focusedDocShell);
  if (!dsti)
    return;

  PRInt32 itemType;
  dsti->GetItemType(&itemType);
  if (itemType == nsIDocShellTreeItem::typeChrome)
    return;  // Never browse with caret in chrome

  bool browseWithCaret =
    Preferences::GetBool("accessibility.browsewithcaret");

  nsCOMPtr<nsIPresShell> presShell;
  focusedDocShell->GetPresShell(getter_AddRefs(presShell));
  if (!presShell)
    return;

  // If this is an editable document which isn't contentEditable, or a
  // contentEditable document and the node to focus is contentEditable,
  // return, so that we don't mess with caret visibility.
  bool isEditable = false;
  nsCOMPtr<nsIEditorDocShell> editorDocShell(do_QueryInterface(dsti));
  if (editorDocShell) {
    editorDocShell->GetEditable(&isEditable);

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
  }

  if (!isEditable && aMoveCaretToFocus)
    MoveCaretToFocus(presShell, aContent);

  if (!aUpdateVisibility)
    return;

  // XXXndeakin this doesn't seem right. It should be checking for this only
  // on the nearest ancestor frame which is a chrome frame. But this is
  // what the existing code does, so just leave it for now.
  if (!browseWithCaret) {
    nsCOMPtr<nsIContent> docContent =
      do_QueryInterface(mFocusedWindow->GetFrameElementInternal());
    if (docContent)
      browseWithCaret = docContent->AttrValueIs(kNameSpaceID_None,
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
    nsRefPtr<nsFrameSelection> frameSelection = aPresShell->FrameSelection();
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
  nsRefPtr<nsCaret> caret = aPresShell->GetCaret();
  if (!caret)
    return NS_OK;

  bool caretVisible = false;
  caret->GetCaretVisible(&caretVisible);
  if (!aVisible && !caretVisible)
    return NS_OK;

  nsRefPtr<nsFrameSelection> frameSelection;
  if (aContent) {
    NS_ASSERTION(aContent->GetDocument() == aPresShell->GetDocument(),
                 "Wrong document?");
    nsIFrame *focusFrame = aContent->GetPrimaryFrame();
    if (focusFrame)
      frameSelection = focusFrame->GetFrameSelection();
  }

  nsRefPtr<nsFrameSelection> docFrameSelection = aPresShell->FrameSelection();

  if (docFrameSelection && caret &&
     (frameSelection == docFrameSelection || !aContent)) {
    nsISelection* domSelection = docFrameSelection->
      GetSelection(nsISelectionController::SELECTION_NORMAL);
    if (domSelection) {
      // First, hide the caret to prevent attempting to show it in SetCaretDOMSelection
      caret->SetCaretVisible(false);

      // Tell the caret which selection to use
      caret->SetCaretDOMSelection(domSelection);

      // In content, we need to set the caret. The only special case is edit
      // fields, which have a different frame selection from the document.
      // They will take care of making the caret visible themselves.

      nsCOMPtr<nsISelectionController> selCon(do_QueryInterface(aPresShell));
      if (!selCon)
        return NS_ERROR_FAILURE;

      selCon->SetCaretEnabled(aVisible);
      caret->SetCaretVisible(aVisible);
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
  *aStartContent = *aEndContent = nsnull;
  nsresult rv = NS_ERROR_FAILURE;

  nsPresContext* presContext = aPresShell->GetPresContext();
  NS_ASSERTION(presContext, "mPresContent is null!!");

  nsRefPtr<nsFrameSelection> frameSelection = aPresShell->FrameSelection();

  nsCOMPtr<nsISelection> domSelection;
  if (frameSelection) {
    domSelection = frameSelection->
      GetSelection(nsISelectionController::SELECTION_NORMAL);
  }

  nsCOMPtr<nsIDOMNode> startNode, endNode;
  bool isCollapsed = false;
  nsCOMPtr<nsIContent> startContent, endContent;
  PRInt32 startOffset = 0;
  if (domSelection) {
    domSelection->GetIsCollapsed(&isCollapsed);
    nsCOMPtr<nsIDOMRange> domRange;
    rv = domSelection->GetRangeAt(0, getter_AddRefs(domRange));
    if (domRange) {
      domRange->GetStartContainer(getter_AddRefs(startNode));
      domRange->GetEndContainer(getter_AddRefs(endNode));
      domRange->GetStartOffset(&startOffset);

      nsIContent *childContent = nsnull;

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
        PRInt32 endOffset = 0;
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

  nsIFrame *startFrame = nsnull;
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

        if (nodeValue.Length() == (PRUint32)startOffset && !isFormControl &&
            startContent != aDocument->GetRootElement()) {
          // Yes, indeed we were at the end of the last node
          nsCOMPtr<nsIFrameEnumerator> frameTraversal;
          nsresult rv = NS_NewFrameTraversal(getter_AddRefs(frameTraversal),
                                             presContext, startFrame,
                                             eLeaf,
                                             false, // aVisual
                                             false, // aLockInScrollView
                                             true      // aFollowOOFs
                                             );
          NS_ENSURE_SUCCESS(rv, rv);

          nsIFrame *newCaretFrame = nsnull;
          nsCOMPtr<nsIContent> newCaretContent = startContent;
          bool endOfSelectionInStartNode(startContent == endContent);
          do {
            // Continue getting the next frame until the primary content for the frame
            // we are on changes - we don't want to be stuck in the same place
            frameTraversal->Next();
            newCaretFrame = static_cast<nsIFrame*>(frameTraversal->CurrentItem());
            if (nsnull == newCaretFrame)
              break;
            newCaretContent = newCaretFrame->GetContent();            
          } while (!newCaretContent || newCaretContent == startContent);

          if (newCaretFrame && newCaretContent) {
            // If the caret is exactly at the same position of the new frame,
            // then we can use the newCaretFrame and newCaretContent for our position
            nsRefPtr<nsCaret> caret = aPresShell->GetCaret();
            nsRect caretRect;
            nsIFrame *frame = caret->GetGeometry(domSelection, &caretRect);
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
nsFocusManager::DetermineElementToMoveFocus(nsPIDOMWindow* aWindow,
                                            nsIContent* aStartContent,
                                            PRInt32 aType,
                                            nsIContent** aNextContent)
{
  *aNextContent = nsnull;

  nsCOMPtr<nsIDocShell> docShell = aWindow->GetDocShell();
  if (!docShell)
    return NS_OK;

  nsCOMPtr<nsIContent> startContent = aStartContent;
  if (!startContent && aType != MOVEFOCUS_CARET)
    startContent = aWindow->GetFocusedNode();

  nsCOMPtr<nsIDocument> doc;
  if (startContent)
    doc = startContent->GetCurrentDoc();
  else
    doc = do_QueryInterface(aWindow->GetExtantDocument());
  if (!doc)
    return NS_OK;

  LookAndFeel::GetInt(LookAndFeel::eIntID_TabFocusModel,
                      &nsIContent::sTabFocusModel);

  if (aType == MOVEFOCUS_ROOT) {
    NS_IF_ADDREF(*aNextContent = GetRootForFocus(aWindow, doc, false, false));
    return NS_OK;
  }
  if (aType == MOVEFOCUS_FORWARDDOC) {
    NS_IF_ADDREF(*aNextContent = GetNextTabbableDocument(true));
    return NS_OK;
  }
  if (aType == MOVEFOCUS_BACKWARDDOC) {
    NS_IF_ADDREF(*aNextContent = GetNextTabbableDocument(false));
    return NS_OK;
  }
  
  nsIContent* rootContent = doc->GetRootElement();
  NS_ENSURE_TRUE(rootContent, NS_OK);

  nsIPresShell *presShell = doc->GetShell();
  NS_ENSURE_TRUE(presShell, NS_OK);

  if (aType == MOVEFOCUS_FIRST) {
    if (!aStartContent)
      startContent = rootContent;
    return GetNextTabbableContent(presShell, startContent,
                                  nsnull, startContent,
                                  true, 1, false, aNextContent);
  }
  if (aType == MOVEFOCUS_LAST) {
    if (!aStartContent)
      startContent = rootContent;
    return GetNextTabbableContent(presShell, startContent,
                                  nsnull, startContent,
                                  false, 0, false, aNextContent);
  }

  bool forward = (aType == MOVEFOCUS_FORWARD || aType == MOVEFOCUS_CARET);
  bool doNavigation = true;
  bool ignoreTabIndex = false;
  // when a popup is open, we want to ensure that tab navigation occurs only
  // within the most recently opened panel. If a popup is open, its frame will
  // be stored in popupFrame.
  nsIFrame* popupFrame = nsnull;

  PRInt32 tabIndex = forward ? 1 : 0;
  if (startContent) {
    nsIFrame* frame = startContent->GetPrimaryFrame();
    if (startContent->Tag() == nsGkAtoms::area &&
        startContent->IsHTML())
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

    if (popupFrame) {
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
        nsIDocument* doc = startContent->GetCurrentDoc();
        if (startContent ==
              nsLayoutUtils::GetEditableRootContentByContentEditable(doc)) {
          doNavigation = false;
        }
      }
    }
  }
  else {
#ifdef MOZ_XUL
    // if there is no focus, yet a panel is open, focus the first item in
    // the panel
    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (pm)
      popupFrame = pm->GetTopPopup(ePopupTypePanel);
#endif
    if (popupFrame) {
      rootContent = popupFrame->GetContent();
      NS_ASSERTION(rootContent, "Popup frame doesn't have a content node");
      startContent = rootContent;
    }
    else {
      // Otherwise, for content shells, start from the location of the caret.
      PRInt32 itemType;
      nsCOMPtr<nsIDocShellTreeItem> shellItem = do_QueryInterface(docShell);
      shellItem->GetItemType(&itemType);
      if (itemType != nsIDocShellTreeItem::typeChrome) {
        nsCOMPtr<nsIContent> endSelectionContent;
        GetSelectionLocation(doc, presShell,
                             getter_AddRefs(startContent),
                             getter_AddRefs(endSelectionContent));
        // If the selection is on the rootContent, then there is no selection
        if (startContent == rootContent) {
          startContent = nsnull;
        }
        else if (startContent && startContent->HasFlag(NODE_IS_EDITABLE)) {
          // Don't start from the selection if the selection is in a
          // contentEditable region.
          nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(doc);
          if (htmlDoc &&
              htmlDoc->GetEditingState() == nsIHTMLDocument::eContentEditable)
            startContent = nsnull;
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

#ifdef DEBUG_FOCUS_NAVIGATION
  PRINTTAGF("Focus Navigation Start Content %s\n", startContent);
  printf("[Tabindex: %d Ignore: %d]", tabIndex, ignoreTabIndex);
#endif

  while (doc) {
    if (doNavigation) {
      nsCOMPtr<nsIContent> nextFocus;
      nsresult rv = GetNextTabbableContent(presShell, rootContent,
                                           skipOriginalContentCheck ? nsnull : originalStartContent,
                                           startContent, forward,
                                           tabIndex, ignoreTabIndex,
                                           getter_AddRefs(nextFocus));
      NS_ENSURE_SUCCESS(rv, rv);

      // found a content node to focus.
      if (nextFocus) {
#ifdef DEBUG_FOCUS_NAVIGATION
        PRINTTAGF("Next Content: %s\n", nextFocus);
#endif
        // as long as the found node was not the same as the starting node,
        // set it as the return value.
        if (nextFocus != originalStartContent)
          NS_ADDREF(*aNextContent = nextFocus);
        return NS_OK;
      }

      if (popupFrame) {
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
    skipOriginalContentCheck = false;
    ignoreTabIndex = false;

    // reached the beginning or end of the document. Traverse up to the parent
    // document and try again.
    nsCOMPtr<nsIDocShellTreeItem> dsti = do_QueryInterface(docShell);

    nsCOMPtr<nsIDocShellTreeItem> docShellParent;
    dsti->GetParent(getter_AddRefs(docShellParent));
    if (docShellParent) {
      // move up to the parent shell and try again from there.

      // first, get the frame element this window is inside.
      nsCOMPtr<nsPIDOMWindow> piWindow = do_GetInterface(docShell);
      NS_ENSURE_TRUE(piWindow, NS_ERROR_FAILURE);

      // Next, retrieve the parent docshell, document and presshell.
      docShell = do_QueryInterface(docShellParent);
      NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

      nsCOMPtr<nsPIDOMWindow> piParentWindow = do_GetInterface(docShellParent);
      NS_ENSURE_TRUE(piParentWindow, NS_ERROR_FAILURE);
      doc = do_QueryInterface(piParentWindow->GetExtantDocument());
      NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

      presShell = doc->GetShell();

      rootContent = doc->GetRootElement();
      startContent = do_QueryInterface(piWindow->GetFrameElementInternal());
      if (startContent) {
        nsIFrame* frame = startContent->GetPrimaryFrame();
        if (!frame)
          return NS_OK;

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
        popupFrame = nsLayoutUtils::GetClosestFrameOfType(frame,
                                                          nsGkAtoms::menuPopupFrame);
        if (popupFrame) {
          rootContent = popupFrame->GetContent();
          NS_ASSERTION(rootContent, "Popup frame doesn't have a content node");
        }
      }
      else {
        startContent = rootContent;
        tabIndex = forward ? 1 : 0;
      }
    }
    else {
      // no parent, so call the tree owner. This will tell the embedder that
      // it should take the focus.
      bool tookFocus;
      docShell->TabToTreeOwner(forward, &tookFocus);
      // if the tree owner, took the focus, blur the current content
      if (tookFocus) {
        nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(docShell);
        if (window->GetFocusedNode() == mFocusedContent)
          Blur(mFocusedWindow, nsnull, true, true);
        else
          window->SetFocusedNode(nsnull);
        return NS_OK;
      }

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
                                       PRInt32 aCurrentTabIndex,
                                       bool aIgnoreTabIndex,
                                       nsIContent** aResultContent)
{
  *aResultContent = nsnull;

  nsCOMPtr<nsIContent> startContent = aStartContent;
  if (!startContent)
    return NS_OK;

#ifdef DEBUG_FOCUS_NAVIGATION
  PRINTTAGF("GetNextTabbable: %s", aStartContent);
  printf(" tabindex: %d\n", aCurrentTabIndex);
#endif

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

    nsCOMPtr<nsIFrameEnumerator> frameTraversal;
    nsresult rv = NS_NewFrameTraversal(getter_AddRefs(frameTraversal),
                                       presContext, startFrame,
                                       ePreOrder,
                                       false, // aVisual
                                       false, // aLockInScrollView
                                       true      // aFollowOOFs
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
             (!iterStartContent || iterStartContent->Tag() != nsGkAtoms::area ||
              !iterStartContent->IsHTML())) {
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
      // TabIndex not set defaults to 0 for form elements, anchors and other
      // elements that are normally focusable. Tabindex defaults to -1
      // for elements that are not normally focusable.
      // The returned computed tabindex from IsFocusable() is as follows:
      //          < 0 not tabbable at all
      //          == 0 in normal tab order (last after positive tabindexed items)
      //          > 0 can be tabbed to in the order specified by this value

      PRInt32 tabIndex;
      frame->IsFocusable(&tabIndex, 0);

#ifdef DEBUG_FOCUS_NAVIGATION
      if (frame->GetContent()) {
        PRINTTAGF("Next Tabbable %s:", frame->GetContent());
        printf(" with tabindex: %d expected: %d\n", tabIndex, aCurrentTabIndex);
      }
#endif

      nsIContent* currentContent = frame->GetContent();
      if (tabIndex >= 0) {
        NS_ASSERTION(currentContent, "IsFocusable set a tabindex for a frame with no content");
        if (currentContent->Tag() == nsGkAtoms::img &&
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

          // found a node with a matching tab index. Check if it is a child
          // frame. If so, navigate into the child frame instead.
          nsIDocument* doc = currentContent->GetCurrentDoc();
          NS_ASSERTION(doc, "content not in document");
          nsIDocument* subdoc = doc->GetSubDocumentFor(currentContent);
          if (subdoc) {
            if (!subdoc->EventHandlingSuppressed()) {
              if (aForward) {
                // when tabbing forward into a frame, return the root
                // frame so that the canvas becomes focused.
                nsCOMPtr<nsPIDOMWindow> subframe = subdoc->GetWindow();
                if (subframe) {
                  // If the subframe body is editable by contenteditable,
                  // we should set the editor's root element rather than the
                  // actual root element.  Otherwise, we should set the focus
                  // to the root content.
                  *aResultContent =
                    nsLayoutUtils::GetEditableRootContentByContentEditable(subdoc);
                  if (!*aResultContent ||
                      !((*aResultContent)->GetPrimaryFrame())) {
                    *aResultContent =
                      GetRootForFocus(subframe, subdoc, false, true);
                  }
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
                                            false, aResultContent);
                NS_ENSURE_SUCCESS(rv, rv);
                if (*aResultContent)
                  return NS_OK;
              }
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
      // has been reached.
      if (!aForward) {
        nsCOMPtr<nsPIDOMWindow> window = GetCurrentWindow(aRootContent);
        NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);
        NS_IF_ADDREF(*aResultContent =
                     GetRootForFocus(window, aRootContent->GetCurrentDoc(), false, true));
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
                                       PRInt32 aCurrentTabIndex,
                                       nsIContent* aImageContent,
                                       nsIContent* aStartContent)
{
  nsAutoString useMap;
  aImageContent->GetAttr(kNameSpaceID_None, nsGkAtoms::usemap, useMap);

  nsCOMPtr<nsIDocument> doc = aImageContent->GetDocument();
  if (doc) {
    nsCOMPtr<nsIContent> mapContent = doc->FindImageMap(useMap);
    if (!mapContent)
      return nsnull;
    PRUint32 count = mapContent->GetChildCount();
    // First see if the the start content is in this map

    PRInt32 index = mapContent->IndexOf(aStartContent);
    PRInt32 tabIndex;
    if (index < 0 || (aStartContent->IsFocusable(&tabIndex) &&
                      tabIndex != aCurrentTabIndex)) {
      // If aStartContent is in this map we must start iterating past it.
      // We skip the case where aStartContent has tabindex == aStartContent
      // since the next tab ordered element might be before it
      // (or after for backwards) in the child list.
      index = aForward ? -1 : (PRInt32)count;
    }

    // GetChildAt will return nsnull if our index < 0 or index >= count
    nsCOMPtr<nsIContent> areaContent;
    while ((areaContent = mapContent->GetChildAt(aForward ? ++index : --index)) != nsnull) {
      if (areaContent->IsFocusable(&tabIndex) && tabIndex == aCurrentTabIndex) {
        return areaContent;
      }
    }
  }

  return nsnull;
}

PRInt32
nsFocusManager::GetNextTabIndex(nsIContent* aParent,
                                PRInt32 aCurrentTabIndex,
                                bool aForward)
{
  PRInt32 tabIndex, childTabIndex;
  nsIContent *child;

  PRUint32 count = aParent->GetChildCount();

  if (aForward) {
    tabIndex = 0;
    for (PRUint32 index = 0; index < count; index++) {
      child = aParent->GetChildAt(index);
      childTabIndex = GetNextTabIndex(child, aCurrentTabIndex, aForward);
      if (childTabIndex > aCurrentTabIndex && childTabIndex != tabIndex) {
        tabIndex = (tabIndex == 0 || childTabIndex < tabIndex) ? childTabIndex : tabIndex;
      }

      nsAutoString tabIndexStr;
      child->GetAttr(kNameSpaceID_None, nsGkAtoms::tabindex, tabIndexStr);
      PRInt32 ec, val = tabIndexStr.ToInteger(&ec);
      if (NS_SUCCEEDED (ec) && val > aCurrentTabIndex && val != tabIndex) {
        tabIndex = (tabIndex == 0 || val < tabIndex) ? val : tabIndex;
      }
    }
  }
  else { /* !aForward */
    tabIndex = 1;
    for (PRUint32 index = 0; index < count; index++) {
      child = aParent->GetChildAt(index);
      childTabIndex = GetNextTabIndex(child, aCurrentTabIndex, aForward);
      if ((aCurrentTabIndex == 0 && childTabIndex > tabIndex) ||
          (childTabIndex < aCurrentTabIndex && childTabIndex > tabIndex)) {
        tabIndex = childTabIndex;
      }

      nsAutoString tabIndexStr;
      child->GetAttr(kNameSpaceID_None, nsGkAtoms::tabindex, tabIndexStr);
      PRInt32 ec, val = tabIndexStr.ToInteger(&ec);
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

nsIContent*
nsFocusManager::GetRootForFocus(nsPIDOMWindow* aWindow,
                                nsIDocument* aDocument,
                                bool aIsForDocNavigation,
                                bool aCheckVisibility)
{
  // the root element's canvas may be focused as long as the document is in a
  // a non-chrome shell and does not contain a frameset.
  if (aIsForDocNavigation) {
    nsCOMPtr<nsIContent> docContent =
      do_QueryInterface(aWindow->GetFrameElementInternal());
    // document navigation skips iframes and frames that are specifically non-focusable
    if (docContent) {
      if (docContent->Tag() == nsGkAtoms::iframe)
        return nsnull;

      nsIFrame* frame = docContent->GetPrimaryFrame();
      if (!frame || !frame->IsFocusable(nsnull, 0))
        return nsnull;
    }
  }
  else  {
    PRInt32 itemType;
    nsCOMPtr<nsIDocShellTreeItem> shellItem = do_QueryInterface(aWindow->GetDocShell());
    shellItem->GetItemType(&itemType);

    if (itemType == nsIDocShellTreeItem::typeChrome)
      return nsnull;
  }

  if (aCheckVisibility && !IsWindowVisible(aWindow))
    return nsnull;

  Element *rootElement = aDocument->GetRootElement();
  if (rootElement) {
    if (aCheckVisibility && !rootElement->GetPrimaryFrame()) {
      return nsnull;
    }

    // Finally, check if this is a frameset
    nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(aDocument);
    if (htmlDoc) {
      PRUint32 childCount = rootElement->GetChildCount();
      for (PRUint32 i = 0; i < childCount; ++i) {
        nsIContent *childContent = rootElement->GetChildAt(i);
        nsINodeInfo *ni = childContent->NodeInfo();
        if (childContent->IsHTML() &&
            ni->Equals(nsGkAtoms::frameset))
          return nsnull;
      }
    }
  }

  return rootElement;
}

TabParent*
nsFocusManager::GetRemoteForContent(nsIContent* aContent) {
  if (!aContent ||
      (aContent->Tag() != nsGkAtoms::browser &&
       aContent->Tag() != nsGkAtoms::iframe) ||
      !aContent->IsXUL() ||
      !aContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::Remote,
                             nsGkAtoms::_true, eIgnoreCase))
    return nsnull;

  nsCOMPtr<nsIFrameLoaderOwner> loaderOwner = do_QueryInterface(aContent);
  if (!loaderOwner)
    return nsnull;

  nsRefPtr<nsFrameLoader> frameLoader = loaderOwner->GetFrameLoader();
  if (!frameLoader)
    return nsnull;

  PBrowserParent* remoteBrowser = frameLoader->GetRemoteBrowser();
  TabParent* remote = static_cast<TabParent*>(remoteBrowser);
  return remote;
}

void
nsFocusManager::GetLastDocShell(nsIDocShellTreeItem* aItem,
                                nsIDocShellTreeItem** aResult)
{
  *aResult = nsnull;

  nsCOMPtr<nsIDocShellTreeItem> curItem = aItem;
  while (curItem) {
    PRInt32 childCount = 0;
    curItem->GetChildCount(&childCount);
    if (!childCount) {
      *aResult = curItem;
      NS_ADDREF(*aResult);
      return;
    }

    
    curItem->GetChildAt(childCount - 1, getter_AddRefs(curItem));
  }
}

void
nsFocusManager::GetNextDocShell(nsIDocShellTreeItem* aItem,
                                nsIDocShellTreeItem** aResult)
{
  *aResult = nsnull;

  PRInt32 childCount = 0;
  aItem->GetChildCount(&childCount);
  if (childCount) {
    aItem->GetChildAt(0, aResult);
    if (*aResult)
      return;
  }

  nsCOMPtr<nsIDocShellTreeItem> curItem = aItem;
  while (curItem) {
    nsCOMPtr<nsIDocShellTreeItem> parentItem;
    curItem->GetParent(getter_AddRefs(parentItem));
    if (!parentItem)
      return;

    // Note that we avoid using GetChildOffset() here because docshell
    // child offsets can't be trusted to be correct. bug 162283.
    nsCOMPtr<nsIDocShellTreeItem> iterItem;
    childCount = 0;
    parentItem->GetChildCount(&childCount);
    for (PRInt32 index = 0; index < childCount; ++index) {
      parentItem->GetChildAt(index, getter_AddRefs(iterItem));
      if (iterItem == curItem) {
        ++index;
        if (index < childCount) {
          parentItem->GetChildAt(index, aResult);
          if (*aResult)
            return;
        }
        break;
      }
    }

    curItem = parentItem;
  }
}

void
nsFocusManager::GetPreviousDocShell(nsIDocShellTreeItem* aItem,
                                    nsIDocShellTreeItem** aResult)
{
  *aResult = nsnull;

  nsCOMPtr<nsIDocShellTreeItem> parentItem;
  aItem->GetParent(getter_AddRefs(parentItem));
  if (!parentItem)
    return;

  // Note that we avoid using GetChildOffset() here because docshell
  // child offsets can't be trusted to be correct. bug 162283.
  PRInt32 childCount = 0;
  parentItem->GetChildCount(&childCount);
  nsCOMPtr<nsIDocShellTreeItem> prevItem, iterItem;
  for (PRInt32 index = 0; index < childCount; ++index) {
    parentItem->GetChildAt(index, getter_AddRefs(iterItem));
    if (iterItem == aItem)
      break;
    prevItem = iterItem;
  }

  if (prevItem)
    GetLastDocShell(prevItem, aResult);
  else
    NS_ADDREF(*aResult = parentItem);
}

nsIContent*
nsFocusManager::GetNextTabbableDocument(bool aForward)
{
  nsCOMPtr<nsIDocShellTreeItem> startItem;
  if (mFocusedWindow) {
    startItem = do_QueryInterface(mFocusedWindow->GetDocShell());
  }
  else {
    nsCOMPtr<nsIWebNavigation> webnav = do_GetInterface(mActiveWindow);
    startItem = do_QueryInterface(webnav);
  }
  if (!startItem)
    return nsnull;

  // perform a depth first search (preorder) of the docshell tree
  // looking for an HTML Frame or a chrome document
  nsIContent* content = nsnull;
  nsCOMPtr<nsIDocShellTreeItem> curItem = startItem;
  nsCOMPtr<nsIDocShellTreeItem> nextItem;
  do {
    if (aForward) {
      GetNextDocShell(curItem, getter_AddRefs(nextItem));
      if (!nextItem) {
        // wrap around to the beginning, which is the top of the tree
        startItem->GetRootTreeItem(getter_AddRefs(nextItem));
      }
    }
    else {
      GetPreviousDocShell(curItem, getter_AddRefs(nextItem));
      if (!nextItem) {
        // wrap around to the end, which is the last item in the tree
        nsCOMPtr<nsIDocShellTreeItem> rootItem;
        startItem->GetRootTreeItem(getter_AddRefs(rootItem));
        GetLastDocShell(rootItem, getter_AddRefs(nextItem));
      }
    }

    curItem = nextItem;
    nsCOMPtr<nsPIDOMWindow> nextFrame = do_GetInterface(nextItem);
    if (!nextFrame)
      return nsnull;

    nsCOMPtr<nsIDocument> doc = do_QueryInterface(nextFrame->GetExtantDocument());
    if (doc && !doc->EventHandlingSuppressed()) {
      content = GetRootForFocus(nextFrame, doc, true, true);
      if (content && !GetRootForFocus(nextFrame, doc, false, false)) {
        // if the found content is in a chrome shell or a frameset, navigate
        // forward one tabbable item so that the first item is focused. Note
        // that we always go forward and not back here.
        nsCOMPtr<nsIContent> nextFocus;
        Element* rootElement = doc->GetRootElement();
        nsIPresShell* presShell = doc->GetShell();
        if (presShell) {
          nsresult rv = GetNextTabbableContent(presShell, rootElement,
                                               nsnull, rootElement,
                                               true, 1, false,
                                               getter_AddRefs(nextFocus));
          return NS_SUCCEEDED(rv) ? nextFocus.get() : nsnull;
        }
      }
    }
  } while (!content);

  return content;
}

void
nsFocusManager::GetFocusInSelection(nsPIDOMWindow* aWindow,
                                    nsIContent* aStartSelection,
                                    nsIContent* aEndSelection,
                                    nsIContent** aFocusedContent)
{
  *aFocusedContent = nsnull;

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
      NS_ADDREF(*aFocusedContent = testContent);
      return;
    }

    // Get the parent
    testContent = testContent->GetParent();

    if (!testContent) {
      // We run this loop again, checking the ancestor chain of the selection's end point
      testContent = nextTestContent;
      nextTestContent = nsnull;
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
      NS_ADDREF(*aFocusedContent = testContent);
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
        selectionNode = nsnull;
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

nsresult
NS_NewFocusManager(nsIFocusManager** aResult)
{
  NS_IF_ADDREF(*aResult = nsFocusManager::GetFocusManager());
  return NS_OK;
}

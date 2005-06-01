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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: Aaron Leventhal (aaronl@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsDocAccessible.h"
#include "nsAccessibleEventData.h"
#include "nsIAccessibilityService.h"
#include "nsArray.h"
#include "nsICommandManager.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocument.h"
#include "nsIDOMAttr.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMEvent.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentType.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMNSHTMLDocument.h"
#include "nsIDOMMutationEvent.h"
#include "nsIDOMWindow.h"
#include "nsIEditingSession.h"
#include "nsIFrame.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsINameSpaceManager.h"
#include "nsIObserverService.h"
#include "nsIPlaintextEditor.h"
#include "nsIPresShell.h"
#include "nsIScriptGlobalObject.h"
#include "nsIServiceManager.h"
#include "nsIScrollableView.h"
#include "nsIURI.h"
#include "nsIWebNavigation.h"
#ifdef MOZ_XUL
#include "nsIXULDocument.h"
#endif

//=============================//
// nsDocAccessible  //
//=============================//

//-----------------------------------------------------
// construction 
//-----------------------------------------------------
nsDocAccessible::nsDocAccessible(nsIDOMNode *aDOMNode, nsIWeakReference* aShell):
  nsBlockAccessible(aDOMNode, aShell), mWnd(nsnull),
  mEditor(nsnull), mScrollPositionChangedTicks(0)
{
  // Because of the way document loading happens, the new nsIWidget is created before
  // the old one is removed. Since it creates the nsDocAccessible, for a brief moment 
  // there can be 2 nsDocAccessible's for the content area, although for 2 different
  // pres shells.

  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mWeakShell));
  if (shell) {
    mDocument = shell->GetDocument();
    nsIViewManager* vm = shell->GetViewManager();
    if (vm) {
      nsCOMPtr<nsIWidget> widget;
      vm->GetWidget(getter_AddRefs(widget));
      if (widget) {
        mWnd = widget->GetNativeData(NS_NATIVE_WINDOW);
      }
    }
  }
  
  PutCacheEntry(gGlobalDocAccessibleCache, mWeakShell, this);

  // XXX aaronl should we use an algorithm for the initial cache size?
  mAccessNodeCache.Init(kDefaultCacheSize);
}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
nsDocAccessible::~nsDocAccessible()
{
}

NS_INTERFACE_MAP_BEGIN(nsDocAccessible)
  NS_INTERFACE_MAP_ENTRY(nsIAccessibleDocument)
  NS_INTERFACE_MAP_ENTRY(nsPIAccessibleDocument)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMutationListener)
  NS_INTERFACE_MAP_ENTRY(nsIScrollPositionListener)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAccessibleDocument)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END_INHERITING(nsBlockAccessible)

NS_IMPL_ADDREF_INHERITED(nsDocAccessible, nsBlockAccessible)
NS_IMPL_RELEASE_INHERITED(nsDocAccessible, nsBlockAccessible)

NS_IMETHODIMP nsDocAccessible::GetName(nsAString& aName) 
{ 
  nsresult rv = NS_OK;
  aName.Truncate();
  if (mRoleMapEntry) {
    rv = nsAccessible::GetName(aName);
  }
  if (aName.IsEmpty()) {
    rv = GetTitle(aName);
  }

  return rv;
}

NS_IMETHODIMP nsDocAccessible::GetRole(PRUint32 *aRole)
{
  *aRole = ROLE_PANE; // Fall back

  nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem =
    GetDocShellTreeItemFor(mDOMNode);
  if (docShellTreeItem) {
    nsCOMPtr<nsIDocShellTreeItem> sameTypeRoot;
    docShellTreeItem->GetSameTypeRootTreeItem(getter_AddRefs(sameTypeRoot));
    if (sameTypeRoot == docShellTreeItem) {
      // Root of content or chrome tree
      PRInt32 itemType;
      docShellTreeItem->GetItemType(&itemType);
      if (itemType == nsIDocShellTreeItem::typeChrome) {
        *aRole = ROLE_APPLICATION;
      }
      else if (itemType == nsIDocShellTreeItem::typeContent) {
        *aRole = ROLE_DOCUMENT;
      }
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::GetValue(nsAString& aValue)
{
  return GetURL(aValue);
}

NS_IMETHODIMP nsDocAccessible::GetState(PRUint32 *aState)
{
  if (!mDOMNode) {
    return NS_ERROR_FAILURE;
  }
  nsAccessible::GetState(aState);
  *aState |= STATE_FOCUSABLE;

  nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem =
    GetDocShellTreeItemFor(mDOMNode);
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(docShellTreeItem));
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  PRUint32 busyFlags;
  docShell->GetBusyFlags(&busyFlags);
  if (busyFlags != nsIDocShell::BUSY_FLAGS_NONE) {
    *aState |= STATE_BUSY;
  }

  // Is it visible?
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mWeakShell));
  nsCOMPtr<nsIWidget> widget;
  if (shell) {
    nsIViewManager* vm = shell->GetViewManager();
    if (vm) {
      vm->GetWidget(getter_AddRefs(widget));
    }
  }
  PRBool isVisible = (widget != nsnull);
  while (widget && isVisible) {
    widget->IsVisible(isVisible);
    widget = widget->GetParent();
  }
  if (!isVisible) {
    *aState |= STATE_INVISIBLE;
  }

  PRBool isEditable;
  GetIsEditable(&isEditable);

  if (!isEditable) {
    // Use STATE_READONLY when we're not in an editor pane
    *aState |= STATE_READONLY; 
  }
  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::GetFocusedChild(nsIAccessible **aFocusedChild) 
{
  if (!gLastFocusedNode) {
    *aFocusedChild = nsnull;
    return NS_OK;
  }

  nsCOMPtr<nsIAccessibilityService> accService =
    do_GetService("@mozilla.org/accessibilityService;1");
  return accService->GetAccessibleInWeakShell(gLastFocusedNode, mWeakShell, 
                                              aFocusedChild);
}

// ------- nsIAccessibleDocument Methods (5) ---------------

NS_IMETHODIMP nsDocAccessible::GetURL(nsAString& aURL)
{ 
  if (!mDocument) {
    return NS_ERROR_FAILURE; // Document has been shut down
  }
  nsCOMPtr<nsISupports> container = mDocument->GetContainer();
  nsCOMPtr<nsIWebNavigation> webNav(do_GetInterface(container));
  nsCAutoString theURL;
  if (webNav) {
    nsCOMPtr<nsIURI> pURI;
    webNav->GetCurrentURI(getter_AddRefs(pURI));
    if (pURI) 
      pURI->GetSpec(theURL);
  }
  CopyUTF8toUTF16(theURL, aURL);
  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::GetTitle(nsAString& aTitle)
{
  if (mDocument) {
    aTitle = mDocument->GetDocumentTitle();
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocAccessible::GetMimeType(nsAString& aMimeType)
{
  nsCOMPtr<nsIDOMNSDocument> domnsDocument(do_QueryInterface(mDocument));
  if (domnsDocument) {
    return domnsDocument->GetContentType(aMimeType);
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocAccessible::GetDocType(nsAString& aDocType)
{
  nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(mDocument));
  nsCOMPtr<nsIDOMDocumentType> docType;

#ifdef MOZ_XUL
  nsCOMPtr<nsIXULDocument> xulDoc(do_QueryInterface(mDocument));
  if (xulDoc) {
    aDocType.AssignLiteral("window"); // doctype not implemented for XUL at time of writing - causes assertion
    return NS_OK;
  } else
#endif
  if (domDoc && NS_SUCCEEDED(domDoc->GetDoctype(getter_AddRefs(docType))) && docType) {
    return docType->GetName(aDocType);
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocAccessible::GetNameSpaceURIForID(PRInt16 aNameSpaceID, nsAString& aNameSpaceURI)
{
  if (mDocument) {
    nsCOMPtr<nsINameSpaceManager> nameSpaceManager =
        do_GetService(NS_NAMESPACEMANAGER_CONTRACTID);
    if (nameSpaceManager) 
      return nameSpaceManager->GetNameSpaceURI(aNameSpaceID, aNameSpaceURI);
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocAccessible::GetCaretAccessible(nsIAccessible **aCaretAccessible)
{
  // We only have a caret accessible on the root document
  *aCaretAccessible = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::GetWindowHandle(void **aWindow)
{
  *aWindow = mWnd;
  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::GetWindow(nsIDOMWindow **aDOMWin)
{
  *aDOMWin = nsnull;
  if (!mDocument) {
    return NS_ERROR_FAILURE;  // Accessible is Shutdown()
  }
  nsCOMPtr<nsIDOMWindow> domWindow(do_QueryInterface(mDocument->GetScriptGlobalObject()));

  if (!domWindow)
    return NS_ERROR_FAILURE;  // No DOM Window

  NS_ADDREF(*aDOMWin = domWindow);

  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::GetDocument(nsIDOMDocument **aDOMDoc)
{
  nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(mDocument));
  *aDOMDoc = domDoc;

  if (domDoc) {
    NS_ADDREF(*aDOMDoc);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

void nsDocAccessible::CheckForEditor()
{
  if (mEditor) {
    return;  // Already have editor, don't need to check
  }
  if (!mDocument) {
    return;  // No document -- we've been shut down
  }
  nsCOMPtr<nsIDOMWindow> domWindow(do_QueryInterface(mDocument->GetScriptGlobalObject()));
  if (!domWindow)
    return;  // No DOM Window

  nsCOMPtr<nsISupports> container = mDocument->GetContainer();
  nsCOMPtr<nsIEditingSession> editingSession(do_GetInterface(container));
  if (!editingSession)
    return; // No editing session interface

  editingSession->GetEditorForWindow(domWindow, getter_AddRefs(mEditor));
  if (mEditor) {
    // State readonly is now clear
#ifdef MOZ_ACCESSIBILITY_ATK
    AtkStateChange stateData;
    stateData.enable = PR_TRUE;
    stateData.state = STATE_READONLY; // Will be translated to ATK_STATE_EDITABLE
    FireToolkitEvent(nsIAccessibleEvent::EVENT_STATE_CHANGE, this, &stateData);
#else
    FireToolkitEvent(nsIAccessibleEvent::EVENT_STATE_CHANGE, this, nsnull);
#endif
  }
}

NS_IMETHODIMP nsDocAccessible::GetIsEditable(PRBool *aIsEditable)
{
  *aIsEditable = PR_FALSE;
  if (mEditor) {
    PRUint32 flags;
    mEditor->GetFlags(&flags);
    *aIsEditable = (flags & nsIPlaintextEditor::eEditorReadonlyMask) == 0;
  }
  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::GetCachedAccessNode(void *aUniqueID, nsIAccessNode **aAccessNode)
{
  GetCacheEntry(mAccessNodeCache, aUniqueID, aAccessNode); // Addrefs for us
  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::CacheAccessNode(void *aUniqueID, nsIAccessNode *aAccessNode)
{
  PutCacheEntry(mAccessNodeCache, aUniqueID, aAccessNode);
  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::Init()
{
  // Hook up our new accessible with our parent
  if (!mParent) {
    nsIDocument *parentDoc = mDocument->GetParentDocument();
    if (parentDoc) {
      nsIContent *ownerContent = parentDoc->FindContentForSubDocument(mDocument);
      nsCOMPtr<nsIDOMNode> ownerNode(do_QueryInterface(ownerContent));
      if (ownerNode) {
        nsCOMPtr<nsIAccessibilityService> accService = 
          do_GetService("@mozilla.org/accessibilityService;1");
        if (accService) {
          // XXX aaronl: ideally we would traverse the presshell chain
          // Since there's no easy way to do that, we cheat and use
          // the document hierarchy. GetAccessibleFor() is bad because
          // it doesn't support our concept of multiple presshells per doc.
          // It should be changed to use GetAccessibleInWeakShell()
          nsCOMPtr<nsIAccessible> accParent;
          accService->GetAccessibleFor(ownerNode, getter_AddRefs(accParent));
          nsCOMPtr<nsPIAccessible> privateParent(do_QueryInterface(accParent));
          if (privateParent) {
            SetParent(accParent);
            privateParent->SetFirstChild(this);
          }
        }
      }
    }
  }
  AddEventListeners();

  nsresult rv = nsBlockAccessible::Init();

  if (mRoleMapEntry && mRoleMapEntry->role != ROLE_DIALOG &&
      mRoleMapEntry->role != ROLE_APPLICATION &&
      mRoleMapEntry->role != ROLE_DOCUMENT) {
    // Document accessible can only have certain roles
    // This was set in nsAccessible::Init() based on xhtml2:role attribute
    mRoleMapEntry->role = nsnull;
  }

  return rv;
}  


NS_IMETHODIMP nsDocAccessible::Destroy()
{
  gGlobalDocAccessibleCache.Remove(NS_STATIC_CAST(void*, mWeakShell));
  return Shutdown();
}

NS_IMETHODIMP nsDocAccessible::Shutdown()
{
  if (!mWeakShell) {
    return NS_OK;  // Already shutdown
  }

  RemoveEventListeners();

  mWeakShell = nsnull;  // Avoid reentrancy

  mEditor = nsnull;

  if (mFireEventTimer) {
    mFireEventTimer->Cancel();
    mFireEventTimer = nsnull;
  }
  mEventsToFire.Clear();

  ClearCache(mAccessNodeCache);

  mDocument = nsnull;

  return nsBlockAccessible::Shutdown();
}

nsIFrame* nsDocAccessible::GetFrame()
{
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mWeakShell));

  nsIFrame* root = nsnull;
  if (shell) 
    root = shell->GetRootFrame();

  return root;
}

void nsDocAccessible::GetBoundsRect(nsRect& aBounds, nsIFrame** aRelativeFrame)
{
  *aRelativeFrame = GetFrame();

  nsIDocument *document = mDocument;
  nsIDocument *parentDoc = nsnull;

  while (document) {
    nsIPresShell *presShell = document->GetShellAt(0);
    if (!presShell) {
      return;
    }
    nsIViewManager* vm = presShell->GetViewManager();

    nsIScrollableView* scrollableView = nsnull;
    if (vm)
      vm->GetRootScrollableView(&scrollableView);

    nsRect viewBounds(0, 0, 0, 0);
    if (scrollableView) {
      viewBounds = scrollableView->View()->GetBounds();
    }
    else {
      nsIView *view;
      vm->GetRootView(view);
      if (view) {
        viewBounds = view->GetBounds();
      }
    }

    if (parentDoc) {  // After first time thru loop
      aBounds.IntersectRect(viewBounds, aBounds);
    }
    else {  // First time through loop
      aBounds = viewBounds;
    }

    document = parentDoc = document->GetParentDocument();
  }
}


nsresult nsDocAccessible::AddEventListeners()
{
  // 1) Set up scroll position listener
  // 2) Set up web progress listener - we need to know 
  //    when page loading is finished
  //    That way we can send the STATE_CHANGE events for 
  //    the MSAA root "pane" object (ROLE_PANE),
  //    and change the STATE_BUSY bit flag
  //    Do this only for top level content documents

  nsCOMPtr<nsIPresShell> presShell(GetPresShell());
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

  nsCOMPtr<nsISupports> container = mDocument->GetContainer();
  nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem(do_QueryInterface(container));
  NS_ENSURE_TRUE(docShellTreeItem, NS_ERROR_FAILURE);

  // Make sure we're a content docshell
  // We don't want to listen to chrome progress
  PRInt32 itemType;
  docShellTreeItem->GetItemType(&itemType);

  PRBool isContent = (itemType == nsIDocShellTreeItem::typeContent);

  if (isContent) {
    CheckForEditor();
  
    if (!mEditor) {
      // We're not an editor yet, but we might become one
      nsCOMPtr<nsICommandManager> commandManager = do_GetInterface(docShellTreeItem);
      if (commandManager) {
        commandManager->AddCommandObserver(this, "obs_documentCreated");
      }
    }
  }

  // add ourself as a mutation event listener 
  // (this slows down mozilla about 3%, but only used when accessibility APIs active)
  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(mDocument));
  NS_ASSERTION(target, "No dom event target for document");
  nsresult rv = target->AddEventListener(NS_LITERAL_STRING("DOMAttrModified"), 
    this, PR_TRUE);
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");
  rv = target->AddEventListener(NS_LITERAL_STRING("DOMSubtreeModified"), 
    this, PR_TRUE);
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");
  rv = target->AddEventListener(NS_LITERAL_STRING("DOMNodeInserted"), 
    this, PR_TRUE);
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");
  rv = target->AddEventListener(NS_LITERAL_STRING("DOMNodeRemoved"), 
    this, PR_TRUE);
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");
  // These aren't implemented yet, and we're not using them
  // rv = target->AddEventListener(NS_LITERAL_STRING("DOMNodeInsertedIntoDocument"), 
  //  this, PR_TRUE);
  // NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");
  // rv = target->AddEventListener(NS_LITERAL_STRING("DOMNodeRemovedFromDocument"), 
  //  this, PR_TRUE);
  // NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");

  return rv;
}

nsresult nsDocAccessible::RemoveEventListeners()
{
  // Remove listeners associated with content documents
  // Remove scroll position listener
  RemoveScrollListener();

  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(mDocument));
  NS_ASSERTION(target, "No dom event target for document");
  target->RemoveEventListener(NS_LITERAL_STRING("DOMAttrModified"), this, PR_TRUE);
  target->RemoveEventListener(NS_LITERAL_STRING("DOMSubtreeModified"), this, PR_TRUE);
  target->RemoveEventListener(NS_LITERAL_STRING("DOMNodeInserted"), this, PR_TRUE);
  target->RemoveEventListener(NS_LITERAL_STRING("DOMNodeRemoved"), this, PR_TRUE);
  // target->RemoveEventListener(NS_LITERAL_STRING("DOMNodeInsertedIntoDocument"), this, PR_TRUE);
  // target->RemoveEventListener(NS_LITERAL_STRING("DOMNodeRemovedFromDocument"), this, PR_TRUE);

  if (mScrollWatchTimer) {
    mScrollWatchTimer->Cancel();
    mScrollWatchTimer = nsnull;
  }

  nsCOMPtr<nsISupports> container = mDocument->GetContainer();
  nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem(do_QueryInterface(container));
  NS_ENSURE_TRUE(docShellTreeItem, NS_ERROR_FAILURE);

  PRInt32 itemType;
  docShellTreeItem->GetItemType(&itemType);
  if (itemType == nsIDocShellTreeItem::typeContent) {
    nsCOMPtr<nsICommandManager> commandManager = do_GetInterface(docShellTreeItem);
    if (commandManager) {
      commandManager->RemoveCommandObserver(this, "obs_documentCreated");
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::FireAnchorJumpEvent()
{
  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::FireDocLoadingEvent(PRBool aIsFinished)
{
  if (!mDocument || !mWeakShell) {
    return NS_OK;  // Document has been shut down
  }

  if (aIsFinished) {
    // Need to wait until scrollable view is available
    AddScrollListener();
  }

  return NS_OK;
}

void nsDocAccessible::ScrollTimerCallback(nsITimer *aTimer, void *aClosure)
{
  nsDocAccessible *docAcc = NS_REINTERPRET_CAST(nsDocAccessible*, aClosure);

  if (docAcc && docAcc->mScrollPositionChangedTicks && 
      ++docAcc->mScrollPositionChangedTicks > 2) {
    // Whenever scroll position changes, mScrollPositionChangeTicks gets reset to 1
    // We only want to fire accessibilty scroll event when scrolling stops or pauses
    // Therefore, we wait for no scroll events to occur between 2 ticks of this timer
    // That indicates a pause in scrolling, so we fire the accessibilty scroll event
    docAcc->FireToolkitEvent(nsIAccessibleEvent::EVENT_SCROLLINGEND, docAcc, nsnull);
    docAcc->mScrollPositionChangedTicks = 0;
    if (docAcc->mScrollWatchTimer) {
      docAcc->mScrollWatchTimer->Cancel();
      docAcc->mScrollWatchTimer = nsnull;
    }
  }
}

void nsDocAccessible::AddScrollListener()
{
  nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mWeakShell));

  nsIViewManager* vm = nsnull;
  if (presShell)
    vm = presShell->GetViewManager();

  nsIScrollableView* scrollableView = nsnull;
  if (vm)
    vm->GetRootScrollableView(&scrollableView);

  if (scrollableView)
    scrollableView->AddScrollPositionListener(this);
}

void nsDocAccessible::RemoveScrollListener()
{
  nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mWeakShell));

  nsIViewManager* vm = nsnull;
  if (presShell)
    vm = presShell->GetViewManager();

  nsIScrollableView* scrollableView = nsnull;
  if (vm)
    vm->GetRootScrollableView(&scrollableView);

  if (scrollableView)
    scrollableView->RemoveScrollPositionListener(this);
}

NS_IMETHODIMP nsDocAccessible::ScrollPositionWillChange(nsIScrollableView *aView, nscoord aX, nscoord aY)
{
  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::ScrollPositionDidChange(nsIScrollableView *aScrollableView, nscoord aX, nscoord aY)
{
  // Start new timer, if the timer cycles at least 1 full cycle without more scroll position changes, 
  // then the ::Notify() method will fire the accessibility event for scroll position changes
  const PRUint32 kScrollPosCheckWait = 50;
  if (mScrollWatchTimer) {
    mScrollWatchTimer->SetDelay(kScrollPosCheckWait);  // Create new timer, to avoid leaks
  }
  else {
    mScrollWatchTimer = do_CreateInstance("@mozilla.org/timer;1");
    if (mScrollWatchTimer) {
      mScrollWatchTimer->InitWithFuncCallback(ScrollTimerCallback, this,
                                              kScrollPosCheckWait, 
                                              nsITimer::TYPE_REPEATING_SLACK);
    }
  }
  mScrollPositionChangedTicks = 1;
  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::Observe(nsISupports *aSubject, const char *aTopic,
                                       const PRUnichar *aData)
{
  if (!nsCRT::strcmp(aTopic,"obs_documentCreated")) {
    CheckForEditor();
    NS_ASSERTION(mEditor, "Should have editor if we see obs_documentCreated");
  }

  return NS_OK;
}

// ---------- Mutation event listeners ------------
NS_IMETHODIMP nsDocAccessible::NodeInserted(nsIDOMEvent* aEvent)
{
  HandleMutationEvent(aEvent, nsIAccessibleEvent::EVENT_SHOW);

  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::NodeRemoved(nsIDOMEvent* aEvent)
{
  // The related node for the event will be the parent of the removed node or subtree

  HandleMutationEvent(aEvent, nsIAccessibleEvent::EVENT_HIDE);

  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::SubtreeModified(nsIDOMEvent* aEvent)
{
  HandleMutationEvent(aEvent, nsIAccessibleEvent::EVENT_REORDER);
 
  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::AttrModified(nsIDOMEvent* aMutationEvent)
{
  // XXX todo
  // We still need to handle special HTML cases here
  // For example, if an <img>'s usemap attribute is modified
  // Otherwise it may just be a state change, for example an object changing
  // its visibility

  nsCOMPtr<nsIPresShell> shell = GetPresShell();
  if (!shell) {
    return NS_OK; // Document has been shut down
  }

  nsCOMPtr<nsIDOMMutationEvent> mutationEvent(do_QueryInterface(aMutationEvent));
  NS_ASSERTION(mutationEvent, "Not a mutation event!");
  nsCOMPtr<nsIDOMEventTarget> domEventTarget;
  mutationEvent->GetTarget(getter_AddRefs(domEventTarget));
  nsCOMPtr<nsIDOMNode> targetNode(do_QueryInterface(domEventTarget));
  NS_ASSERTION(targetNode, "No node for attr modified");
  if (!targetNode) {
    return NS_OK;
  }

  nsCOMPtr<nsIAccessibilityService> accService = 
    do_GetService("@mozilla.org/accessibilityService;1");
  NS_ASSERTION(accService, "How can we be here if no accessibility service?");

  nsCOMPtr<nsIAccessible> changedAccessible;
  accService->GetAccessibleInShell(targetNode, shell,
                                   getter_AddRefs(changedAccessible));
  if (!changedAccessible) {
    return NS_OK; // The attribute change did not occur on a node exposed for accessibility
  }

  nsCOMPtr<nsIDOMNode> attributeNode;
  mutationEvent->GetRelatedNode(getter_AddRefs(attributeNode));
  nsCOMPtr<nsIDOMAttr> domAttr(do_QueryInterface(attributeNode));
  NS_ASSERTION(domAttr, "No related attribute node for DOMAttrModified event");

  nsAutoString attrName;
  mutationEvent->GetAttrName(attrName);

  PRUint32 eventType = 0;
  if (attrName.EqualsLiteral("checked") || attrName.EqualsLiteral("expanded")) {
    nsCOMPtr<nsIContent> targetContent(do_QueryInterface(targetNode));
    if (targetContent->IsContentOfType(nsIContent::eXUL)) {
      // XXX Should we remove XUL's CheckboxStateChanged event and just utilize this instead?
      return NS_OK; // XUL utilizes CheckboxStateChanged event for this. Don't fire double event
    }
    eventType = nsIAccessibleEvent::EVENT_STATE_CHANGE;
  }
  else if (attrName.EqualsLiteral("readonly") ||
           attrName.EqualsLiteral("disabled") ||
           attrName.EqualsLiteral("required") ||
           attrName.EqualsLiteral("invalid")) {
    eventType = nsIAccessibleEvent::EVENT_STATE_CHANGE;
  }
  else if (attrName.EqualsLiteral("valuenow")) {
    eventType = nsIAccessibleEvent::EVENT_VALUE_CHANGE;
  }
  else if (attrName.EqualsLiteral("selected")) {
    nsCOMPtr<nsIContent> targetContent(do_QueryInterface(targetNode));
    if (targetContent->IsContentOfType(nsIContent::eXUL)) {
      return NS_OK; // XUL fires special events for selection
    }
    // XXX Do we need to differentiate between different kinds of selection events
    // such as selection, selection add, selection remove?
    nsAutoString attrValue;
    mutationEvent->GetNewValue(attrValue);
    if (!attrValue.IsEmpty() && !attrValue.EqualsLiteral("false")) {
      eventType = nsIAccessibleEvent::EVENT_SELECTION;
    }
  }

  // Fire after short timer, because we need to wait for
  // DOM attribute to actually change. Otherwise, assistive technology 
  // will retrieve the wrong state/value/selection info.
  if (eventType) {
    PRBool isTimerStarted = PR_TRUE;
    if (mEventsToFire.Count() == 0) {
      if (!mFireEventTimer) {
        // Do not yet have a timer going for firing another event.
        mFireEventTimer = do_CreateInstance("@mozilla.org/timer;1");
        NS_ENSURE_TRUE(mFireEventTimer, NS_ERROR_OUT_OF_MEMORY);
      }
      isTimerStarted = PR_FALSE;
    }
    // XXX Add related data for ATK support.
    // For example, state change event should provide what state has changed,
    // as well as the old and new value.
    nsCOMPtr<nsIAccessibleEvent> event =
      new nsAccessibleEventData(eventType, changedAccessible, this, nsnull);
    NS_ENSURE_TRUE(event, NS_ERROR_OUT_OF_MEMORY);
    mEventsToFire.AppendObject(event);
    if (!isTimerStarted) {
      // This is be the first delayed event in queue, start timer
      // so that event gets fired via FlushEventsCallback
      mFireEventTimer->InitWithFuncCallback(FlushEventsCallback,
                                            NS_STATIC_CAST(nsPIAccessibleDocument*, this),
                                            0, nsITimer::TYPE_ONE_SHOT);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::FlushPendingEvents()
{
  PRUint32 length = mEventsToFire.Count();
  NS_ASSERTION(length, "How did we get here without events to fire?");
  PRUint32 index;
  for (index = 0; index < length; index ++) {
    nsIAccessibleEvent *accessibleEvent = mEventsToFire[index];
    NS_ASSERTION(accessibleEvent, "Array item is not an accessible event");
    nsCOMPtr<nsIAccessible> accessible;
    accessibleEvent->GetAccessible(getter_AddRefs(accessible));
    PRUint32 eventType;
    accessibleEvent->GetEventType(&eventType);
    FireToolkitEvent(eventType, accessible, nsnull);
  }
  mEventsToFire.Clear(); // Clear out array
  return NS_OK;
}

void nsDocAccessible::FlushEventsCallback(nsITimer *aTimer, void *aClosure)
{
  nsPIAccessibleDocument *accessibleDoc = NS_STATIC_CAST(nsPIAccessibleDocument*, aClosure);
  NS_ASSERTION(accessibleDoc, "How did we get here without an accessible document?");
  accessibleDoc->FlushPendingEvents();
}

NS_IMETHODIMP nsDocAccessible::NodeRemovedFromDocument(nsIDOMEvent* aMutationEvent)
{
  // Not implemented yet, see bug 74220
  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::NodeInsertedIntoDocument(nsIDOMEvent* aMutationEvent)
{

  // Not implemented yet, see bug 74219
  // This is different from NodeInserted() in that it's fired when
  // a node is inserted into a document, but isn't necessarily mean that
  // it's becoming part of the DOM tree.
  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::HandleEvent(nsIDOMEvent* aEvent)
{
  NS_NOTREACHED("Should be handled by specific methods like NodeInserted, etc.");
  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::CharacterDataModified(nsIDOMEvent* aMutationEvent)
{
  return NS_OK;
}

void nsDocAccessible::ShutdownNodes(nsIDOMNode *aStartNode)
{
  nsCOMPtr<nsIDOMNode> iterNode(aStartNode), nextNode;
  nsCOMPtr<nsIAccessNode> accessNode;

  do {
    GetCachedAccessNode(iterNode, getter_AddRefs(accessNode));
    if (accessNode) {
      // Accessibles that implement their own subtrees,
      // like html combo boxes and xul trees must shutdown all of their own
      // children when they override Shutdown()

      // Don't shutdown our doc object!
      if (accessNode != NS_STATIC_CAST(nsIAccessNode*, this)) { 
        void *uniqueID;
        accessNode->GetUniqueID(&uniqueID);
        nsCOMPtr<nsPIAccessNode> privateAccessNode(do_QueryInterface(accessNode));
        privateAccessNode->Shutdown();
        // Remove from hash table as well
        mAccessNodeCache.Remove(uniqueID);
      }
    }

    iterNode->GetFirstChild(getter_AddRefs(nextNode));
    if (nextNode) {
      iterNode = nextNode;
      continue;
    }

    if (iterNode == aStartNode)
      break;
    iterNode->GetNextSibling(getter_AddRefs(nextNode));
    if (nextNode) {
      iterNode = nextNode;
      continue;
    }

    do {
      iterNode->GetParentNode(getter_AddRefs(nextNode));
      if (!nextNode || nextNode == aStartNode) {
        return;
      }
      nextNode->GetNextSibling(getter_AddRefs(iterNode));
      if (iterNode)
        break;
      iterNode = nextNode;
    } while (PR_TRUE);
  }
  while (iterNode && iterNode != aStartNode);
}

NS_IMETHODIMP nsDocAccessible::InvalidateCacheSubtree(nsIDOMNode *aStartNode,
                                                      PRUint32 aChangeEventType)
{
  // Invalidate cache subtree
  // We have to check for accessibles for each dom node by traversing DOM tree
  // instead of just the accessible tree, although that would be faster
  // Otherwise we might miss the nsAccessNode's that are not nsAccessible's.

  if (!aStartNode)
    return NS_ERROR_FAILURE;

  // Shutdown nsIAccessNode's or nsIAccessibles for any DOM nodes in this subtree
  ShutdownNodes(aStartNode);

  // We need to get an accessible for the mutation event's target node
  // If there is no accessible for that node, we need to keep moving up the parent
  // chain so there is some accessible.
  // We will use this accessible to fire the accessible mutation event.
  // We're guaranteed success, because we will eventually end up at the doc accessible,
  // and there is always one of those.

  nsCOMPtr<nsIAccessible> accessible;
  GetAccessibleInParentChain(aStartNode, getter_AddRefs(accessible));
  nsCOMPtr<nsPIAccessible> privateAccessible(do_QueryInterface(accessible));
  if (privateAccessible) {
    privateAccessible->InvalidateChildren();
  }

  // Fire an event so the assistive technology knows the objects it is holding onto
  // in this part of the subtree are no longer useful and should be released.
  // However, they still won't crash if the AT tries to use them, because a stub of the
  // object still exists as long as it is refcounted, even from outside of Gecko.
  privateAccessible->FireToolkitEvent(aChangeEventType, accessible, nsnull);

  return NS_OK;
}

NS_IMETHODIMP 
nsDocAccessible::GetAccessibleInParentChain(nsIDOMNode *aNode, 
                                            nsIAccessible **aAccessible)
{
  nsCOMPtr<nsIAccessibilityService> accService = 
    do_GetService("@mozilla.org/accessibilityService;1");
  NS_ASSERTION(accService,
               "No accessibility service while in code created by service");

  nsCOMPtr<nsIDOMNode> currentNode(aNode), parentNode;

  while (NS_FAILED(accService->GetAccessibleInWeakShell(currentNode, mWeakShell, 
                                                        aAccessible))) {
    currentNode->GetParentNode(getter_AddRefs(parentNode));
    NS_ASSERTION(parentNode, "Impossible! Crawled up parent chain without "
                             "finding accessible. There should have at least "
                             "been a document accessible at the root.");
    if (!parentNode) {
      // XXX Todo We need to figure out why this is happening.
      // For now, return safely.
      return NS_ERROR_FAILURE;
    }
    currentNode = parentNode;
  }

  return NS_OK;
}

void nsDocAccessible::HandleMutationEvent(nsIDOMEvent *aEvent, PRUint32 aChangeEventType)
{
  NS_ASSERTION(aChangeEventType == nsIAccessibleEvent::EVENT_REORDER ||
               aChangeEventType == nsIAccessibleEvent::EVENT_SHOW ||
               aChangeEventType == nsIAccessibleEvent::EVENT_HIDE,
               "Incorrect aChangeEventType passed in");


  nsCOMPtr<nsIDOMMutationEvent> mutationEvent(do_QueryInterface(aEvent));
  NS_ASSERTION(mutationEvent, "Not a mutation event!");

  nsCOMPtr<nsIDOMNode> subTreeToInvalidate;
  mutationEvent->GetRelatedNode(getter_AddRefs(subTreeToInvalidate));
  NS_ASSERTION(subTreeToInvalidate, 
               "No old sub tree being replaced in DOMSubtreeModified");
  nsCOMPtr<nsIAccessibleDocument> docAccessible =
    GetDocAccessibleFor(subTreeToInvalidate);
  nsCOMPtr<nsPIAccessibleDocument> privateDocAccessible =
    do_QueryInterface(docAccessible);
  if (privateDocAccessible) {
    privateDocAccessible->InvalidateCacheSubtree(subTreeToInvalidate,
                                                 aChangeEventType);
  }
}

NS_IMETHODIMP nsDocAccessible::FireToolkitEvent(PRUint32 aEvent, nsIAccessible* aAccessible, void* aData)
{
  nsCOMPtr<nsIObserverService> obsService =
    do_GetService("@mozilla.org/observer-service;1");
  if (!obsService) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIAccessibleEvent> accEvent = new nsAccessibleEventData(aEvent, aAccessible, this, aData);
  NS_ENSURE_TRUE(accEvent, NS_ERROR_OUT_OF_MEMORY);

  return obsService->NotifyObservers(accEvent, NS_ACCESSIBLE_EVENT_TOPIC, nsnull);
}


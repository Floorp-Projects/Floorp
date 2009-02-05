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

#include "nsRootAccessible.h"
#include "nsAccessibilityAtoms.h"
#include "nsAccessibleEventData.h"
#include "nsIAccessibilityService.h"
#include "nsIMutableArray.h"
#include "nsICommandManager.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocument.h"
#include "nsIDOMAttr.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentType.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMNSHTMLDocument.h"
#include "nsIDOMMutationEvent.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMXULPopupElement.h"
#include "nsIEditingSession.h"
#include "nsIEventStateManager.h"
#include "nsIFrame.h"
#include "nsHTMLSelectAccessible.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsINameSpaceManager.h"
#include "nsIPresShell.h"
#include "nsIServiceManager.h"
#include "nsIScrollableView.h"
#include "nsIViewManager.h"
#include "nsIView.h"
#include "nsUnicharUtils.h"
#include "nsIURI.h"
#include "nsIWebNavigation.h"
#include "nsIFocusController.h"
#ifdef MOZ_XUL
#include "nsIXULDocument.h"
#endif

//=============================//
// nsDocAccessible  //
//=============================//

PRUint32 nsDocAccessible::gLastFocusedAccessiblesState = 0;
nsIAtom *nsDocAccessible::gLastFocusedFrameType = nsnull;

//-----------------------------------------------------
// construction
//-----------------------------------------------------
nsDocAccessible::nsDocAccessible(nsIDOMNode *aDOMNode, nsIWeakReference* aShell):
  nsHyperTextAccessibleWrap(aDOMNode, aShell), mWnd(nsnull),
  mScrollPositionChangedTicks(0), mIsContentLoaded(PR_FALSE),
  mIsLoadCompleteFired(PR_FALSE), mInFlushPendingEvents(PR_FALSE)
{
  // For GTK+ native window, we do nothing here.
  if (!mDOMNode)
    return;

  // Because of the way document loading happens, the new nsIWidget is created before
  // the old one is removed. Since it creates the nsDocAccessible, for a brief moment
  // there can be 2 nsDocAccessible's for the content area, although for 2 different
  // pres shells.

  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mWeakShell));
  if (shell) {
    // Find mDocument
    mDocument = shell->GetDocument();
    
    // Find mWnd
    nsIViewManager* vm = shell->GetViewManager();
    if (vm) {
      nsCOMPtr<nsIWidget> widget;
      vm->GetWidget(getter_AddRefs(widget));
      if (widget) {
        mWnd = widget->GetNativeData(NS_NATIVE_WINDOW);
      }
    }
  }

  // XXX aaronl should we use an algorithm for the initial cache size?
  mAccessNodeCache.Init(kDefaultCacheSize);

  nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem =
    nsCoreUtils::GetDocShellTreeItemFor(mDOMNode);
  nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(docShellTreeItem);
  if (docShell) {
    PRUint32 busyFlags;
    docShell->GetBusyFlags(&busyFlags);
    if (busyFlags == nsIDocShell::BUSY_FLAGS_NONE) {
      mIsContentLoaded = PR_TRUE;                                               
    }
  }
}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
nsDocAccessible::~nsDocAccessible()
{
}

////////////////////////////////////////////////////////////////////////////////
// nsDocAccessible. nsISupports

static PLDHashOperator
ElementTraverser(const void *aKey, nsIAccessNode *aAccessNode,
                 void *aUserArg)
{
  nsCycleCollectionTraversalCallback *cb = 
    static_cast<nsCycleCollectionTraversalCallback*>(aUserArg);

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*cb, "mAccessNodeCache entry");
  cb->NoteXPCOMChild(aAccessNode);
  return PL_DHASH_NEXT;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDocAccessible)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDocAccessible, nsAccessible)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mEventsToFire)
  tmp->mAccessNodeCache.EnumerateRead(ElementTraverser, &cb); 
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDocAccessible, nsAccessible)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMARRAY(mEventsToFire)
  tmp->ClearCache(tmp->mAccessNodeCache);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDocAccessible)
  NS_INTERFACE_MAP_ENTRY(nsIAccessibleDocument)
  NS_INTERFACE_MAP_ENTRY(nsPIAccessibleDocument)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentObserver)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY(nsIScrollPositionListener)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAccessibleDocument)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END_INHERITING(nsHyperTextAccessible)

NS_IMPL_ADDREF_INHERITED(nsDocAccessible, nsHyperTextAccessible)
NS_IMPL_RELEASE_INHERITED(nsDocAccessible, nsHyperTextAccessible)

NS_IMETHODIMP
nsDocAccessible::GetName(nsAString& aName)
{
  nsresult rv = NS_OK;
  aName.Truncate();
  if (mParent) {
    rv = mParent->GetName(aName); // Allow owning iframe to override the name
  }
  if (aName.IsEmpty()) {
    // Allow name via aria-labelledby or title attribute
    rv = nsAccessible::GetName(aName);
  }
  if (aName.IsEmpty()) {
    rv = GetTitle(aName);   // Try title element
  }
  if (aName.IsEmpty()) {   // Last resort: use URL
    rv = GetURL(aName);
  }

  return rv;
}

NS_IMETHODIMP nsDocAccessible::GetRole(PRUint32 *aRole)
{
  *aRole = nsIAccessibleRole::ROLE_PANE; // Fall back

  nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem =
    nsCoreUtils::GetDocShellTreeItemFor(mDOMNode);
  if (docShellTreeItem) {
    nsCOMPtr<nsIDocShellTreeItem> sameTypeRoot;
    docShellTreeItem->GetSameTypeRootTreeItem(getter_AddRefs(sameTypeRoot));
    PRInt32 itemType;
    docShellTreeItem->GetItemType(&itemType);
    if (sameTypeRoot == docShellTreeItem) {
      // Root of content or chrome tree
      if (itemType == nsIDocShellTreeItem::typeChrome) {
        *aRole = nsIAccessibleRole::ROLE_CHROME_WINDOW;
      }
      else if (itemType == nsIDocShellTreeItem::typeContent) {
#ifdef MOZ_XUL
        nsCOMPtr<nsIXULDocument> xulDoc(do_QueryInterface(mDocument));
        if (xulDoc) {
          *aRole = nsIAccessibleRole::ROLE_APPLICATION;
        } else {
          *aRole = nsIAccessibleRole::ROLE_DOCUMENT;
        }
#else
        *aRole = nsIAccessibleRole::ROLE_DOCUMENT;
#endif
      }
    }
    else if (itemType == nsIDocShellTreeItem::typeContent) {
      *aRole = nsIAccessibleRole::ROLE_DOCUMENT;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::SetRoleMapEntry(nsRoleMapEntry* aRoleMapEntry)
{
  NS_ENSURE_STATE(mDocument);

  mRoleMapEntry = aRoleMapEntry;

  // Allow use of ARIA role from outer to override
  nsIDocument *parentDoc = mDocument->GetParentDocument();
  NS_ENSURE_TRUE(parentDoc, NS_ERROR_FAILURE);
  nsIContent *ownerContent = parentDoc->FindContentForSubDocument(mDocument);
  nsCOMPtr<nsIDOMNode> ownerNode(do_QueryInterface(ownerContent));
  if (ownerNode) {
    nsRoleMapEntry *roleMapEntry = nsAccUtils::GetRoleMapEntry(ownerNode);
    if (roleMapEntry)
      mRoleMapEntry = roleMapEntry; // Override
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsDocAccessible::GetDescription(nsAString& aDescription)
{
  if (mParent)
    mParent->GetDescription(aDescription);

  if (aDescription.IsEmpty()) {
    nsAutoString description;
    GetTextFromRelationID(nsAccessibilityAtoms::aria_describedby, description);
    aDescription = description;
  }

  return NS_OK;
}

nsresult
nsDocAccessible::GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState)
{
  nsresult rv = nsAccessible::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

#ifdef MOZ_XUL
  nsCOMPtr<nsIXULDocument> xulDoc(do_QueryInterface(mDocument));
  if (!xulDoc)
#endif
  {
    // XXX Need to invent better check to see if doc is focusable,
    // which it should be if it is scrollable. A XUL document could be focusable.
    // See bug 376803.
    *aState |= nsIAccessibleStates::STATE_FOCUSABLE;
    if (gLastFocusedNode == mDOMNode) {
      *aState |= nsIAccessibleStates::STATE_FOCUSED;
    }
  }

  if (!mIsContentLoaded) {
    *aState |= nsIAccessibleStates::STATE_BUSY;
    if (aExtraState) {
      *aExtraState |= nsIAccessibleStates::EXT_STATE_STALE;
    }
  }
 
  nsIFrame* frame = GetFrame();
  while (frame != nsnull && !frame->HasView()) {
    frame = frame->GetParent();
  }
 
  if (frame == nsnull ||
      !CheckVisibilityInParentChain(mDocument, frame->GetViewExternal())) {
    *aState |= nsIAccessibleStates::STATE_INVISIBLE |
               nsIAccessibleStates::STATE_OFFSCREEN;
  }

  nsCOMPtr<nsIEditor> editor;
  GetAssociatedEditor(getter_AddRefs(editor));
  if (!editor) {
    *aState |= nsIAccessibleStates::STATE_READONLY;
  }
  else if (aExtraState) {
    *aExtraState |= nsIAccessibleStates::EXT_STATE_EDITABLE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocAccessible::GetARIAState(PRUint32 *aState)
{
  // Combine with states from outer doc
  NS_ENSURE_ARG_POINTER(aState);
  nsresult rv = nsAccessible::GetARIAState(aState);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsPIAccessible> privateParentAccessible = do_QueryInterface(mParent);
  if (privateParentAccessible)  // Allow iframe/frame etc. to have final state override via ARIA
    return privateParentAccessible->GetARIAState(aState);

  return rv;
}

NS_IMETHODIMP
nsDocAccessible::GetAttributes(nsIPersistentProperties **aAttributes)
{
  nsAccessible::GetAttributes(aAttributes);
  if (mParent) {
    mParent->GetAttributes(aAttributes); // Add parent attributes (override inner)
  }
  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::GetFocusedChild(nsIAccessible **aFocusedChild)
{
  if (!gLastFocusedNode) {
    *aFocusedChild = nsnull;
    return NS_OK;
  }

  // Return an accessible for the current global focus, which does not have to
  // be contained within the current document.
  nsCOMPtr<nsIAccessibilityService> accService =
    do_GetService("@mozilla.org/accessibilityService;1");
  return accService->GetAccessibleFor(gLastFocusedNode, aFocusedChild);
}

NS_IMETHODIMP nsDocAccessible::TakeFocus()
{
  NS_ENSURE_TRUE(mDocument, NS_ERROR_FAILURE);
  PRUint32 state;
  GetStateInternal(&state, nsnull);
  if (0 == (state & nsIAccessibleStates::STATE_FOCUSABLE)) {
    return NS_ERROR_FAILURE; // Not focusable
  }

  nsCOMPtr<nsIDocShellTreeItem> treeItem =
    nsCoreUtils::GetDocShellTreeItemFor(mDOMNode);
  nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(treeItem);
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  nsCOMPtr<nsIPresShell> shell(GetPresShell());
  if (!shell) {
    NS_WARNING("Was not shutdown properly via InvalidateCacheSubtree()");
    return NS_ERROR_FAILURE;
  }
  nsIEventStateManager *esm = shell->GetPresContext()->EventStateManager();
  NS_ENSURE_TRUE(esm, NS_ERROR_FAILURE);

  // Focus the document
  nsresult rv = docShell->SetHasFocus(PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  // Clear out any existing focus state
  return esm->SetContentState(nsnull, NS_EVENT_STATE_FOCUS);
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
  nsCOMPtr<nsIDOMNSDocument> domnsDocument(do_QueryInterface(mDocument));
  if (domnsDocument) {
    return domnsDocument->GetTitle(aTitle);
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
    return docType->GetPublicId(aDocType);
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
  *aDOMWin = mDocument->GetWindow();

  if (!*aDOMWin)
    return NS_ERROR_FAILURE;  // No DOM Window

  NS_ADDREF(*aDOMWin);

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

NS_IMETHODIMP nsDocAccessible::GetAssociatedEditor(nsIEditor **aEditor)
{
  NS_ENSURE_ARG_POINTER(aEditor);
  *aEditor = nsnull;

  if (!mDocument)
    return NS_ERROR_FAILURE;

  // Check if document is editable (designMode="on" case). Otherwise check if
  // the html:body (for HTML document case) or document element is editable.
  if (!mDocument->HasFlag(NODE_IS_EDITABLE)) {
    nsCOMPtr<nsIDOMNode> DOMDocument(do_QueryInterface(mDocument));
    nsCOMPtr<nsIDOMElement> DOMElement =
      nsCoreUtils::GetDOMElementFor(DOMDocument);
    nsCOMPtr<nsIContent> content(do_QueryInterface(DOMElement));

    if (!content || !content->HasFlag(NODE_IS_EDITABLE))
      return NS_OK;
  }

  nsCOMPtr<nsISupports> container = mDocument->GetContainer();
  nsCOMPtr<nsIEditingSession> editingSession(do_GetInterface(container));
  if (!editingSession)
    return NS_OK; // No editing session interface

  nsCOMPtr<nsIEditor> editor;
  editingSession->GetEditorForWindow(mDocument->GetWindow(), getter_AddRefs(editor));
  if (!editor) {
    return NS_OK;
  }
  PRBool isEditable;
  editor->GetIsDocumentEditable(&isEditable);
  if (isEditable) {
    NS_ADDREF(*aEditor = editor);
  }
  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::GetCachedAccessNode(void *aUniqueID, nsIAccessNode **aAccessNode)
{
  GetCacheEntry(mAccessNodeCache, aUniqueID, aAccessNode); // Addrefs for us
#ifdef DEBUG_A11Y
  // All cached accessible nodes should be in the parent
  // It will assert if not all the children were created
  // when they were first cached, and no invalidation
  // ever corrected parent accessible's child cache.
  nsCOMPtr<nsIAccessible> accessible = do_QueryInterface(*aAccessNode);
  nsCOMPtr<nsPIAccessible> privateAccessible = do_QueryInterface(accessible);
  if (privateAccessible) {
    nsCOMPtr<nsIAccessible> parent;
    privateAccessible->GetCachedParent(getter_AddRefs(parent));
    nsCOMPtr<nsPIAccessible> privateParent(do_QueryInterface(parent));
    if (privateParent) {
      privateParent->TestChildCache(accessible);
    }
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsDocAccessible::CacheAccessNode(void *aUniqueID, nsIAccessNode *aAccessNode)
{
  // If there is an access node for the given unique ID then let's shutdown it.
  // The unique ID may be presented in the cache if originally we created
  // access node object and then we want to create accessible object when
  // DOM node is changed.
  nsCOMPtr<nsIAccessNode> accessNode;
  GetCacheEntry(mAccessNodeCache, aUniqueID, getter_AddRefs(accessNode));
  if (accessNode) {
    nsRefPtr<nsAccessNode> accNode = nsAccUtils::QueryAccessNode(accessNode);
    accNode->Shutdown();
  }

  PutCacheEntry(mAccessNodeCache, aUniqueID, aAccessNode);
  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::GetParent(nsIAccessible **aParent)
{
  // Hook up our new accessible with our parent
  *aParent = nsnull;
  NS_ENSURE_TRUE(mDocument, NS_ERROR_FAILURE);
  if (!mParent) {
    nsIDocument *parentDoc = mDocument->GetParentDocument();
    NS_ENSURE_TRUE(parentDoc, NS_ERROR_FAILURE);
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
        accService->GetAccessibleFor(ownerNode, getter_AddRefs(mParent));
      }
    }
  }
  return mParent ? nsAccessible::GetParent(aParent) : NS_ERROR_FAILURE;
}

nsresult
nsDocAccessible::Init()
{
  PutCacheEntry(gGlobalDocAccessibleCache, mDocument, this);

  AddEventListeners();

  nsCOMPtr<nsIAccessible> parentAccessible;  // Ensure outer doc mParent accessible
  GetParent(getter_AddRefs(parentAccessible));

  return nsHyperTextAccessibleWrap::Init();
}

nsresult
nsDocAccessible::Shutdown()
{
  if (!mWeakShell) {
    return NS_OK;  // Already shutdown
  }

  nsCOMPtr<nsIDocShellTreeItem> treeItem =
    nsCoreUtils::GetDocShellTreeItemFor(mDOMNode);
  ShutdownChildDocuments(treeItem);

  RemoveEventListeners();

  mWeakShell = nsnull;  // Avoid reentrancy

  ClearCache(mAccessNodeCache);

  nsCOMPtr<nsIDocument> kungFuDeathGripDoc = mDocument;
  mDocument = nsnull;

  nsHyperTextAccessibleWrap::Shutdown();

  if (mFireEventTimer) {
    // Doc being shut down before events fired,
    mFireEventTimer->Cancel();
    mFireEventTimer = nsnull;
    if (mEventsToFire.Count() > 0 ) {
      mEventsToFire.Clear();
      // Make sure we release the kung fu death grip which is always
      // there when there are still events left to be fired
      // If FlushPendingEvents() is in call stack,
      // kung fu death grip will be released there.
      if (!mInFlushPendingEvents)
        NS_RELEASE_THIS();
    }
  }

  // Remove from the cache after other parts of Shutdown(), so that Shutdown() procedures
  // can find the doc or root accessible in the cache if they need it.
  // We don't do this during ShutdownAccessibility() because that is already clearing the cache
  if (!gIsShuttingDownApp)
    gGlobalDocAccessibleCache.Remove(static_cast<void*>(kungFuDeathGripDoc));

  return NS_OK;
}

void nsDocAccessible::ShutdownChildDocuments(nsIDocShellTreeItem *aStart)
{
  nsCOMPtr<nsIDocShellTreeNode> treeNode(do_QueryInterface(aStart));
  if (treeNode) {
    PRInt32 subDocuments;
    treeNode->GetChildCount(&subDocuments);
    for (PRInt32 count = 0; count < subDocuments; count ++) {
      nsCOMPtr<nsIDocShellTreeItem> treeItemChild;
      treeNode->GetChildAt(count, getter_AddRefs(treeItemChild));
      NS_ASSERTION(treeItemChild, "No tree item when there should be");
      if (!treeItemChild) {
        continue;
      }
      nsCOMPtr<nsIAccessibleDocument> docAccessible =
        GetDocAccessibleFor(treeItemChild);
      if (docAccessible) {
        nsRefPtr<nsAccessNode> docAccNode =
          nsAccUtils::QueryAccessNode(docAccessible);
        docAccNode->Shutdown();
      }
    }
  }
}

nsIFrame*
nsDocAccessible::GetFrame()
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
    nsIPresShell *presShell = document->GetPrimaryShell();
    if (!presShell) {
      return;
    }
    nsIViewManager* vm = presShell->GetViewManager();
    if (!vm) {
      return;
    }

    nsIScrollableView* scrollableView = nsnull;
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
  // 2) Check for editor and listen for changes to editor

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
    // We're not an editor yet, but we might become one
    nsCOMPtr<nsICommandManager> commandManager = do_GetInterface(docShellTreeItem);
    if (commandManager) {
      commandManager->AddCommandObserver(this, "obs_documentCreated");
    }
  }

  nsCOMPtr<nsIDocShellTreeItem> rootTreeItem;
  docShellTreeItem->GetRootTreeItem(getter_AddRefs(rootTreeItem));
  if (rootTreeItem) {
    nsCOMPtr<nsIAccessibleDocument> rootAccDoc =
      GetDocAccessibleFor(rootTreeItem, PR_TRUE); // Ensure root accessible is created;
    nsRefPtr<nsRootAccessible> rootAccessible = GetRootAccessible(); // Then get it as ref ptr
    NS_ENSURE_TRUE(rootAccessible, NS_ERROR_FAILURE);
    nsRefPtr<nsCaretAccessible> caretAccessible = rootAccessible->GetCaretAccessible();
    if (caretAccessible) {
      caretAccessible->AddDocSelectionListener(presShell);
    }
  }

  // add document observer
  mDocument->AddObserver(this);
  return NS_OK;
}

nsresult nsDocAccessible::RemoveEventListeners()
{
  // Remove listeners associated with content documents
  // Remove scroll position listener
  RemoveScrollListener();

  // Remove document observer
  mDocument->RemoveObserver(this);

  if (mScrollWatchTimer) {
    mScrollWatchTimer->Cancel();
    mScrollWatchTimer = nsnull;
    NS_RELEASE_THIS(); // Kung fu death grip
  }

  nsRefPtr<nsRootAccessible> rootAccessible(GetRootAccessible());
  if (rootAccessible) {
    nsRefPtr<nsCaretAccessible> caretAccessible = rootAccessible->GetCaretAccessible();
    if (caretAccessible) {
      // Don't use GetPresShell() which can call Shutdown() if it sees dead pres shell
      nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mWeakShell));
      caretAccessible->RemoveDocSelectionListener(presShell);
    }
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
  if (!mIsContentLoaded || !mDocument) {
    return NS_OK;
  }
  nsCOMPtr<nsISupports> container = mDocument->GetContainer();
  nsCOMPtr<nsIWebNavigation> webNav(do_GetInterface(container));
  nsCAutoString theURL;
  if (webNav) {
    nsCOMPtr<nsIURI> pURI;
    webNav->GetCurrentURI(getter_AddRefs(pURI));
    if (pURI) {
      pURI->GetSpec(theURL);
    }
  }
  static nsCAutoString lastAnchor;
  const char kHash = '#';
  nsCAutoString currentAnchor;
  PRInt32 hasPosition = theURL.FindChar(kHash);
  if (hasPosition > 0 && hasPosition < (PRInt32)theURL.Length() - 1) {
    mIsAnchor = PR_TRUE;
    currentAnchor.Assign(Substring(theURL,
                                   hasPosition+1, 
                                   (PRInt32)theURL.Length()-hasPosition-1));
  }

  if (currentAnchor.Equals(lastAnchor)) {
    mIsAnchorJumped = PR_FALSE;
  } else {
    mIsAnchorJumped = PR_TRUE;
    lastAnchor.Assign(currentAnchor);
  }

  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::FireDocLoadEvents(PRUint32 aEventType)
{
  if (!mDocument || !mWeakShell) {
    return NS_OK;  // Document has been shut down
  }

  PRBool isFinished = 
             (aEventType == nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_COMPLETE ||
              aEventType == nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_STOPPED);

  mIsContentLoaded = isFinished;
  if (isFinished) {
    if (mIsLoadCompleteFired)
      return NS_OK;
    mIsLoadCompleteFired = PR_TRUE;
  }

  nsCOMPtr<nsIDocShellTreeItem> treeItem =
    nsCoreUtils::GetDocShellTreeItemFor(mDOMNode);
  if (!treeItem) {
    return NS_OK;
  }
  nsCOMPtr<nsIDocShellTreeItem> sameTypeRoot;
  treeItem->GetSameTypeRootTreeItem(getter_AddRefs(sameTypeRoot));

  if (isFinished) {
    // Need to wait until scrollable view is available
    AddScrollListener();
    nsCOMPtr<nsIAccessible> parent(nsAccessible::GetParent());
    nsCOMPtr<nsPIAccessible> privateAccessible(do_QueryInterface(parent));
    if (privateAccessible) {
      // Make the parent forget about the old document as a child
      privateAccessible->InvalidateChildren();
    }
    if (sameTypeRoot != treeItem) {
      // Fire show/hide events to indicate frame/iframe content is new, rather than
      // doc load event which causes screen readers to act is if entire page is reloaded
      InvalidateCacheSubtree(nsnull, nsIAccessibleEvent::EVENT_DOM_SIGNIFICANT_CHANGE);
    }
    // Fire STATE_CHANGE event for doc load finish if focus is in same doc tree
    if (gLastFocusedNode) {
      nsCOMPtr<nsIDocShellTreeItem> focusedTreeItem =
        nsCoreUtils::GetDocShellTreeItemFor(gLastFocusedNode);
      if (focusedTreeItem) {
        nsCOMPtr<nsIDocShellTreeItem> sameTypeRootOfFocus;
        focusedTreeItem->GetSameTypeRootTreeItem(getter_AddRefs(sameTypeRootOfFocus));
        if (sameTypeRoot == sameTypeRootOfFocus) {
          nsCOMPtr<nsIAccessibleStateChangeEvent> accEvent =
            new nsAccStateChangeEvent(this, nsIAccessibleStates::STATE_BUSY, PR_FALSE, PR_FALSE);
          FireAccessibleEvent(accEvent);
          FireAnchorJumpEvent();
        }
      }
    }
  }

  if (sameTypeRoot == treeItem) {
    // Not a frame or iframe
    if (!isFinished) {
      // Fire state change event to set STATE_BUSY when document is loading. For
      // example, Window-Eyes expects to get it.
      nsCOMPtr<nsIAccessibleStateChangeEvent> accEvent =
        new nsAccStateChangeEvent(this, nsIAccessibleStates::STATE_BUSY,
                                  PR_FALSE, PR_TRUE);
      FireAccessibleEvent(accEvent);
    }

    nsAccUtils::FireAccEvent(aEventType, this);
  }
  return NS_OK;
}

void nsDocAccessible::ScrollTimerCallback(nsITimer *aTimer, void *aClosure)
{
  nsDocAccessible *docAcc = reinterpret_cast<nsDocAccessible*>(aClosure);

  if (docAcc && docAcc->mScrollPositionChangedTicks &&
      ++docAcc->mScrollPositionChangedTicks > 2) {
    // Whenever scroll position changes, mScrollPositionChangeTicks gets reset to 1
    // We only want to fire accessibilty scroll event when scrolling stops or pauses
    // Therefore, we wait for no scroll events to occur between 2 ticks of this timer
    // That indicates a pause in scrolling, so we fire the accessibilty scroll event
    nsAccUtils::FireAccEvent(nsIAccessibleEvent::EVENT_SCROLLING_END, docAcc);

    docAcc->mScrollPositionChangedTicks = 0;
    if (docAcc->mScrollWatchTimer) {
      docAcc->mScrollWatchTimer->Cancel();
      docAcc->mScrollWatchTimer = nsnull;
      NS_RELEASE(docAcc); // Release kung fu death grip
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
      NS_ADDREF_THIS(); // Kung fu death grip
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
    // State editable will now be set, readonly is now clear
    nsCOMPtr<nsIAccessibleStateChangeEvent> event =
      new nsAccStateChangeEvent(this, nsIAccessibleStates::EXT_STATE_EDITABLE,
                                PR_TRUE, PR_TRUE);
    FireAccessibleEvent(event);
  }

  return NS_OK;
}

  ///////////////////////////////////////////////////////////////////////
// nsIDocumentObserver

NS_IMPL_NSIDOCUMENTOBSERVER_CORE_STUB(nsDocAccessible)
NS_IMPL_NSIDOCUMENTOBSERVER_LOAD_STUB(nsDocAccessible)
NS_IMPL_NSIDOCUMENTOBSERVER_STYLE_STUB(nsDocAccessible)

void
nsDocAccessible::AttributeChanged(nsIDocument *aDocument, nsIContent* aContent,
                                  PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                                  PRInt32 aModType, PRUint32 aStateMask)
{
  AttributeChangedImpl(aContent, aNameSpaceID, aAttribute);

  // If it was the focused node, cache the new state
  nsCOMPtr<nsIDOMNode> targetNode = do_QueryInterface(aContent);
  if (targetNode == gLastFocusedNode) {
    nsCOMPtr<nsIAccessible> focusedAccessible;
    GetAccService()->GetAccessibleFor(targetNode, getter_AddRefs(focusedAccessible));
    if (focusedAccessible) {
      gLastFocusedAccessiblesState = nsAccUtils::State(focusedAccessible);
    }
  }
}


void
nsDocAccessible::AttributeChangedImpl(nsIContent* aContent, PRInt32 aNameSpaceID, nsIAtom* aAttribute)
{
  // Fire accessible event after short timer, because we need to wait for
  // DOM attribute & resulting layout to actually change. Otherwise,
  // assistive technology will retrieve the wrong state/value/selection info.

  // XXX todo
  // We still need to handle special HTML cases here
  // For example, if an <img>'s usemap attribute is modified
  // Otherwise it may just be a state change, for example an object changing
  // its visibility
  // 
  // XXX todo: report aria state changes for "undefined" literal value changes
  // filed as bug 472142
  //
  // XXX todo:  invalidate accessible when aria state changes affect exposed role
  // filed as bug 472143
  
  nsCOMPtr<nsISupports> container = mDocument->GetContainer();
  nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(container);
  if (!docShell) {
    return;
  }

  PRUint32 busyFlags;
  docShell->GetBusyFlags(&busyFlags);
  if (busyFlags) {
    return; // Still loading, ignore setting of initial attributes
  }

  nsCOMPtr<nsIPresShell> shell = GetPresShell();
  if (!shell) {
    return; // Document has been shut down
  }

  nsCOMPtr<nsIDOMNode> targetNode(do_QueryInterface(aContent));
  NS_ASSERTION(targetNode, "No node for attr modified");
  if (!targetNode || !nsAccUtils::IsNodeRelevant(targetNode))
    return;

  // Since we're in synchronous code, we can store whether the current attribute
  // change is from user input or not. If the attribute change causes an asynchronous
  // layout change, that event can use the last known user input state
  nsAccEvent::PrepareForEvent(targetNode);

  // Universal boolean properties that don't require a role.
  if (aAttribute == nsAccessibilityAtoms::disabled ||
      aAttribute == nsAccessibilityAtoms::aria_disabled) {
    // Fire the state change whether disabled attribute is
    // set for XUL, HTML or ARIA namespace.
    // Checking the namespace would not seem to gain us anything, because
    // disabled really is going to mean the same thing in any namespace.
    // We use the attribute instead of the disabled state bit because
    // ARIA's aria-disabled does not affect the disabled state bit
    nsCOMPtr<nsIAccessibleStateChangeEvent> enabledChangeEvent =
      new nsAccStateChangeEvent(targetNode,
                                nsIAccessibleStates::EXT_STATE_ENABLED,
                                PR_TRUE);
    FireDelayedAccessibleEvent(enabledChangeEvent);
    nsCOMPtr<nsIAccessibleStateChangeEvent> sensitiveChangeEvent =
      new nsAccStateChangeEvent(targetNode,
                                nsIAccessibleStates::EXT_STATE_SENSITIVE,
                                PR_TRUE);
    FireDelayedAccessibleEvent(sensitiveChangeEvent);
    return;
  }

  // Check for namespaced ARIA attribute
  if (aNameSpaceID == kNameSpaceID_None) {
    // Check for hyphenated aria-foo property?
    const char* attributeName;
    aAttribute->GetUTF8String(&attributeName);
    if (!PL_strncmp("aria-", attributeName, 5)) {
      ARIAAttributeChanged(aContent, aAttribute);
    }
  }

  if (aAttribute == nsAccessibilityAtoms::role ||
      aAttribute == nsAccessibilityAtoms::href ||
      aAttribute == nsAccessibilityAtoms::onclick ||
      aAttribute == nsAccessibilityAtoms::aria_droppable) {
    // Not worth the expense to ensure which namespace these are in
    // It doesn't kill use to recreate the accessible even if the attribute was used
    // in the wrong namespace or an element that doesn't support it
    InvalidateCacheSubtree(aContent, nsIAccessibleEvent::EVENT_DOM_SIGNIFICANT_CHANGE);
    return;
  }
  
  if (aAttribute == nsAccessibilityAtoms::alt ||
      aAttribute == nsAccessibilityAtoms::title) {
    FireDelayedToolkitEvent(nsIAccessibleEvent::EVENT_NAME_CHANGE,
                            targetNode);
    return;
  }

  if (aAttribute == nsAccessibilityAtoms::selected ||
      aAttribute == nsAccessibilityAtoms::aria_selected) {
    // ARIA or XUL selection
    nsCOMPtr<nsIAccessible> multiSelect =
      nsAccUtils::GetMultiSelectFor(targetNode);
    // Multi selects use selection_add and selection_remove
    // Single select widgets just mirror event_selection for
    // whatever gets event_focus, which is done in
    // nsRootAccessible::FireAccessibleFocusEvent()
    // So right here we make sure only to deal with multi selects
    if (multiSelect) {
      // Need to find the right event to use here, SELECTION_WITHIN would
      // seem right but we had started using it for something else
      nsCOMPtr<nsIAccessNode> multiSelectAccessNode =
        do_QueryInterface(multiSelect);
      nsCOMPtr<nsIDOMNode> multiSelectDOMNode;
      multiSelectAccessNode->GetDOMNode(getter_AddRefs(multiSelectDOMNode));
      NS_ASSERTION(multiSelectDOMNode, "A new accessible without a DOM node!");
      FireDelayedToolkitEvent(nsIAccessibleEvent::EVENT_SELECTION_WITHIN,
                              multiSelectDOMNode,
                              nsAccEvent::eAllowDupes);

      static nsIContent::AttrValuesArray strings[] =
        {&nsAccessibilityAtoms::_empty, &nsAccessibilityAtoms::_false, nsnull};
      if (aContent->FindAttrValueIn(kNameSpaceID_None, aAttribute,
                                    strings, eCaseMatters) >= 0) {
        FireDelayedToolkitEvent(nsIAccessibleEvent::EVENT_SELECTION_REMOVE,
                                targetNode);
        return;
      }

      FireDelayedToolkitEvent(nsIAccessibleEvent::EVENT_SELECTION_ADD,
                                                  targetNode);
    }
  }

  if (aAttribute == nsAccessibilityAtoms::contenteditable) {
    nsCOMPtr<nsIAccessibleStateChangeEvent> editableChangeEvent =
      new nsAccStateChangeEvent(targetNode,
                                nsIAccessibleStates::EXT_STATE_EDITABLE,
                                PR_TRUE);
    FireDelayedAccessibleEvent(editableChangeEvent);
    return;
  }
}

void
nsDocAccessible::ARIAAttributeChanged(nsIContent* aContent, nsIAtom* aAttribute)
{
  nsCOMPtr<nsIDOMNode> targetNode(do_QueryInterface(aContent));
  if (!targetNode)
    return;

  if (aAttribute == nsAccessibilityAtoms::aria_required) {
    nsCOMPtr<nsIAccessibleStateChangeEvent> event =
      new nsAccStateChangeEvent(targetNode,
                                nsIAccessibleStates::STATE_REQUIRED,
                                PR_FALSE);
    FireDelayedAccessibleEvent(event);
    return;
  }

  if (aAttribute == nsAccessibilityAtoms::aria_invalid) {
    nsCOMPtr<nsIAccessibleStateChangeEvent> event =
      new nsAccStateChangeEvent(targetNode,
                                nsIAccessibleStates::STATE_INVALID,
                                PR_FALSE);
    FireDelayedAccessibleEvent(event);
    return;
  }

  if (aAttribute == nsAccessibilityAtoms::aria_activedescendant) {
    // The activedescendant universal property redirects accessible focus events
    // to the element with the id that activedescendant points to
    nsCOMPtr<nsIDOMNode> currentFocus = GetCurrentFocus();
    if (SameCOMIdentity(nsCoreUtils::GetRoleContent(currentFocus), targetNode)) {
      nsRefPtr<nsRootAccessible> rootAcc = GetRootAccessible();
      if (rootAcc)
        rootAcc->FireAccessibleFocusEvent(nsnull, currentFocus, nsnull, PR_TRUE);
    }
    return;
  }

  if (!aContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::role)) {
    // We don't care about these other ARIA attribute changes unless there is
    // an ARIA role set for the element
    // XXX: we should check the role map to see if the changed property is
    // relevant for that particular role.
    return;
  }

  // The following ARIA attributes only take affect when dynamic content role is present
  if (aAttribute == nsAccessibilityAtoms::aria_checked ||
      aAttribute == nsAccessibilityAtoms::aria_pressed) {
    const PRUint32 kState = (aAttribute == nsAccessibilityAtoms::aria_checked) ?
                            nsIAccessibleStates::STATE_CHECKED : 
                            nsIAccessibleStates::STATE_PRESSED;
    nsCOMPtr<nsIAccessibleStateChangeEvent> event =
      new nsAccStateChangeEvent(targetNode, kState, PR_FALSE);
    FireDelayedAccessibleEvent(event);
    if (targetNode == gLastFocusedNode) {
      // State changes for MIXED state currently only supported for focused item, because
      // otherwise we would need access to the old attribute value in this listener.
      // This is because we don't know if the previous value of aria-checked or aria-pressed was "mixed"
      // without caching that info.
      nsCOMPtr<nsIAccessible> accessible;
      event->GetAccessible(getter_AddRefs(accessible));
      if (accessible) {
        PRBool wasMixed = (gLastFocusedAccessiblesState & nsIAccessibleStates::STATE_MIXED) != 0;
        PRBool isMixed  =
          (nsAccUtils::State(accessible) & nsIAccessibleStates::STATE_MIXED) != 0;
        if (wasMixed != isMixed) {
          nsCOMPtr<nsIAccessibleStateChangeEvent> event =
            new nsAccStateChangeEvent(targetNode,
                                      nsIAccessibleStates::STATE_MIXED,
                                      PR_FALSE, isMixed);
          FireDelayedAccessibleEvent(event);
        }
      }
    }
    return;
  }

  if (aAttribute == nsAccessibilityAtoms::aria_expanded) {
    nsCOMPtr<nsIAccessibleStateChangeEvent> event =
      new nsAccStateChangeEvent(targetNode,
                                nsIAccessibleStates::STATE_EXPANDED,
                                PR_FALSE);
    FireDelayedAccessibleEvent(event);
    return;
  }

  if (aAttribute == nsAccessibilityAtoms::aria_readonly) {
    nsCOMPtr<nsIAccessibleStateChangeEvent> event =
      new nsAccStateChangeEvent(targetNode,
                                nsIAccessibleStates::STATE_READONLY,
                                PR_FALSE);
    FireDelayedAccessibleEvent(event);
    return;
  }

  if (aAttribute == nsAccessibilityAtoms::aria_valuenow) {
    FireDelayedToolkitEvent(nsIAccessibleEvent::EVENT_VALUE_CHANGE,
                            targetNode);
    return;
  }

  if (aAttribute == nsAccessibilityAtoms::aria_multiselectable &&
      aContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::role)) {
    // This affects whether the accessible supports nsIAccessibleSelectable.
    // COM says we cannot change what interfaces are supported on-the-fly,
    // so invalidate this object. A new one will be created on demand.
    InvalidateCacheSubtree(aContent, nsIAccessibleEvent::EVENT_DOM_SIGNIFICANT_CHANGE);
  }
}

void nsDocAccessible::ContentAppended(nsIDocument *aDocument,
                                      nsIContent* aContainer,
                                      PRInt32 aNewIndexInContainer)
{
  if ((!mIsContentLoaded || !mDocument) && mAccessNodeCache.Count() <= 1) {
    // See comments in nsDocAccessible::InvalidateCacheSubtree
    InvalidateChildren();
    return;
  }

  PRUint32 childCount = aContainer->GetChildCount();
  for (PRUint32 index = aNewIndexInContainer; index < childCount; index ++) {
    nsCOMPtr<nsIContent> child(aContainer->GetChildAt(index));
    // InvalidateCacheSubtree will not fire the EVENT_SHOW for the new node
    // unless an accessible can be created for the passed in node, which it
    // can't do unless the node is visible. The right thing happens there so
    // no need for an extra visibility check here.
    InvalidateCacheSubtree(child, nsIAccessibleEvent::EVENT_DOM_CREATE);
  }
}

void nsDocAccessible::ContentStatesChanged(nsIDocument* aDocument,
                                           nsIContent* aContent1,
                                           nsIContent* aContent2,
                                           PRInt32 aStateMask)
{
  if (0 == (aStateMask & NS_EVENT_STATE_CHECKED)) {
    return;
  }

  nsHTMLSelectOptionAccessible::SelectionChangedIfOption(aContent1);
  nsHTMLSelectOptionAccessible::SelectionChangedIfOption(aContent2);
}

void nsDocAccessible::CharacterDataWillChange(nsIDocument *aDocument,
                                              nsIContent* aContent,
                                              CharacterDataChangeInfo* aInfo)
{
  FireTextChangeEventForText(aContent, aInfo, PR_FALSE);
}

void nsDocAccessible::CharacterDataChanged(nsIDocument *aDocument,
                                           nsIContent* aContent,
                                           CharacterDataChangeInfo* aInfo)
{
  FireTextChangeEventForText(aContent, aInfo, PR_TRUE);
}

void
nsDocAccessible::ContentInserted(nsIDocument *aDocument, nsIContent* aContainer,
                                 nsIContent* aChild, PRInt32 aIndexInContainer)
{
  // InvalidateCacheSubtree will not fire the EVENT_SHOW for the new node
  // unless an accessible can be created for the passed in node, which it
  // can't do unless the node is visible. The right thing happens there so
  // no need for an extra visibility check here.
  InvalidateCacheSubtree(aChild, nsIAccessibleEvent::EVENT_DOM_CREATE);
}

void
nsDocAccessible::ContentRemoved(nsIDocument *aDocument, nsIContent* aContainer,
                                nsIContent* aChild, PRInt32 aIndexInContainer)
{
  // Invalidate the subtree of the removed element.
  // InvalidateCacheSubtree(aChild, nsIAccessibleEvent::EVENT_DOM_DESTROY);
  // This is no longer needed, we get our notifications directly from content
  // *before* the frame for the content is destroyed, or any other side effects occur.
  // That allows us to correctly calculate the TEXT_REMOVED event if there is one.
}

void
nsDocAccessible::ParentChainChanged(nsIContent *aContent)
{
}

void
nsDocAccessible::FireValueChangeForTextFields(nsIAccessible *aPossibleTextFieldAccessible)
{
  if (nsAccUtils::Role(aPossibleTextFieldAccessible) != nsIAccessibleRole::ROLE_ENTRY)
    return;

  // Dependent value change event for text changes in textfields
  nsCOMPtr<nsIAccessibleEvent> valueChangeEvent =
    new nsAccEvent(nsIAccessibleEvent::EVENT_VALUE_CHANGE, aPossibleTextFieldAccessible,
                   PR_FALSE, nsAccEvent::eRemoveDupes);
  FireDelayedAccessibleEvent(valueChangeEvent );
}

void
nsDocAccessible::FireTextChangeEventForText(nsIContent *aContent,
                                            CharacterDataChangeInfo* aInfo,
                                            PRBool aIsInserted)
{
  if (!mIsContentLoaded || !mDocument) {
    return;
  }

  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(aContent));
  if (!node)
    return;

  nsCOMPtr<nsIAccessible> accessible;
  nsresult rv = GetAccessibleInParentChain(node, PR_TRUE, getter_AddRefs(accessible));
  if (NS_FAILED(rv) || !accessible)
    return;

  nsRefPtr<nsHyperTextAccessible> textAccessible;
  rv = accessible->QueryInterface(NS_GET_IID(nsHyperTextAccessible),
                                  getter_AddRefs(textAccessible));
  if (NS_FAILED(rv) || !textAccessible)
    return;

  PRInt32 start = aInfo->mChangeStart;

  PRInt32 offset = 0;
  rv = textAccessible->DOMPointToHypertextOffset(node, start, &offset);
  if (NS_FAILED(rv))
    return;

  PRInt32 length = aIsInserted ?
    aInfo->mReplaceLength: // text has been added
    aInfo->mChangeEnd - start; // text has been removed

  if (length > 0) {
    nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mWeakShell));
    if (!shell)
      return;

    PRUint32 renderedStartOffset, renderedEndOffset;
    nsIFrame* frame = shell->GetPrimaryFrameFor(aContent);
    if (!frame)
      return;

    rv = textAccessible->ContentToRenderedOffset(frame, start,
                                                 &renderedStartOffset);
    if (NS_FAILED(rv))
      return;

    rv = textAccessible->ContentToRenderedOffset(frame, start + length,
                                                 &renderedEndOffset);
    if (NS_FAILED(rv))
      return;

    nsCOMPtr<nsIAccessibleTextChangeEvent> event =
      new nsAccTextChangeEvent(accessible, offset,
                               renderedEndOffset - renderedStartOffset,
                               aIsInserted, PR_FALSE);
    textAccessible->FireAccessibleEvent(event);

    FireValueChangeForTextFields(accessible);
  }
}

already_AddRefed<nsIAccessibleTextChangeEvent>
nsDocAccessible::CreateTextChangeEventForNode(nsIAccessible *aContainerAccessible,
                                              nsIDOMNode *aChangeNode,
                                              nsIAccessible *aAccessibleForChangeNode,
                                              PRBool aIsInserting,
                                              PRBool aIsAsynch)
{
  nsRefPtr<nsHyperTextAccessible> textAccessible;
  aContainerAccessible->QueryInterface(NS_GET_IID(nsHyperTextAccessible),
                                       getter_AddRefs(textAccessible));
  if (!textAccessible) {
    return nsnull;
  }

  PRInt32 offset;
  PRInt32 length = 0;
  nsCOMPtr<nsIAccessible> changeAccessible;
  nsresult rv = textAccessible->DOMPointToHypertextOffset(aChangeNode, -1, &offset,
                                                          getter_AddRefs(changeAccessible));
  NS_ENSURE_SUCCESS(rv, nsnull);

  if (!aAccessibleForChangeNode) {
    // A span-level object or something else without an accessible is being removed, where
    // it has no accessible but it has descendant content which is aggregated as text
    // into the parent hypertext.
    // In this case, accessibleToBeRemoved may just be the first
    // accessible that is removed, which affects the text in the hypertext container
    if (!changeAccessible) {
      return nsnull; // No descendant content that represents any text in the hypertext parent
    }
    nsCOMPtr<nsIAccessible> child = changeAccessible;
    while (PR_TRUE) {
      nsCOMPtr<nsIAccessNode> childAccessNode =
        do_QueryInterface(changeAccessible);
      nsCOMPtr<nsIDOMNode> childNode;
      childAccessNode->GetDOMNode(getter_AddRefs(childNode));
      if (!nsCoreUtils::IsAncestorOf(aChangeNode, childNode)) {
        break;  // We only want accessibles with DOM nodes as children of this node
      }
      length += nsAccUtils::TextLength(child);
      child->GetNextSibling(getter_AddRefs(changeAccessible));
      if (!changeAccessible) {
        break;
      }
      child.swap(changeAccessible);
    }
  }
  else {
    NS_ASSERTION(!changeAccessible || changeAccessible == aAccessibleForChangeNode,
                 "Hypertext is reporting a different accessible for this node");

    length = nsAccUtils::TextLength(aAccessibleForChangeNode);
    if (nsAccUtils::Role(aAccessibleForChangeNode) == nsIAccessibleRole::ROLE_WHITESPACE) {  // newline
      // Don't fire event for the first html:br in an editor.
      nsCOMPtr<nsIEditor> editor;
      textAccessible->GetAssociatedEditor(getter_AddRefs(editor));
      if (editor) {
        PRBool isEmpty = PR_FALSE;
        editor->GetDocumentIsEmpty(&isEmpty);
        if (isEmpty) {
          return nsnull;
        }
      }
    }
  }

  if (length <= 0) {
    return nsnull;
  }

  nsIAccessibleTextChangeEvent *event =
    new nsAccTextChangeEvent(aContainerAccessible, offset, length, aIsInserting, aIsAsynch);
  NS_IF_ADDREF(event);

  return event;
}
  
nsresult nsDocAccessible::FireDelayedToolkitEvent(PRUint32 aEvent,
                                                  nsIDOMNode *aDOMNode,
                                                  nsAccEvent::EEventRule aAllowDupes,
                                                  PRBool aIsAsynch)
{
  nsCOMPtr<nsIAccessibleEvent> event =
    new nsAccEvent(aEvent, aDOMNode, aIsAsynch, aAllowDupes);
  NS_ENSURE_TRUE(event, NS_ERROR_OUT_OF_MEMORY);

  return FireDelayedAccessibleEvent(event);
}

nsresult
nsDocAccessible::FireDelayedAccessibleEvent(nsIAccessibleEvent *aEvent)
{
  NS_ENSURE_TRUE(aEvent, NS_ERROR_FAILURE);

  if (!mFireEventTimer) {
    // Do not yet have a timer going for firing another event.
    mFireEventTimer = do_CreateInstance("@mozilla.org/timer;1");
    NS_ENSURE_TRUE(mFireEventTimer, NS_ERROR_OUT_OF_MEMORY);
  }

  mEventsToFire.AppendObject(aEvent);
  if (mEventsToFire.Count() == 1) {
    // This is be the first delayed event in queue, start timer
    // so that event gets fired via FlushEventsCallback
    NS_ADDREF_THIS(); // Kung fu death grip to prevent crash in callback
    mFireEventTimer->InitWithFuncCallback(FlushEventsCallback,
                                          static_cast<nsPIAccessibleDocument*>(this),
                                          0, nsITimer::TYPE_ONE_SHOT);
  }

  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::FlushPendingEvents()
{
  mInFlushPendingEvents = PR_TRUE;
  PRUint32 length = mEventsToFire.Count();
  NS_ASSERTION(length, "How did we get here without events to fire?");
  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  if (!presShell)
    length = 0; // The doc is now shut down, don't fire events in it anymore
  else
    nsAccEvent::ApplyEventRules(mEventsToFire);
  
  for (PRUint32 index = 0; index < length; index ++) {
    nsCOMPtr<nsIAccessibleEvent> accessibleEvent(
      do_QueryInterface(mEventsToFire[index]));

    if (nsAccEvent::EventRule(accessibleEvent) == nsAccEvent::eDoNotEmit)
      continue;

    nsCOMPtr<nsIAccessible> accessible;
    accessibleEvent->GetAccessible(getter_AddRefs(accessible));
    nsCOMPtr<nsIDOMNode> domNode;
    accessibleEvent->GetDOMNode(getter_AddRefs(domNode));
    PRUint32 eventType = nsAccEvent::EventType(accessibleEvent);
    PRBool isFromUserInput = nsAccEvent::IsFromUserInput(accessibleEvent);

    if (domNode == gLastFocusedNode &&
        (eventType == nsIAccessibleEvent::EVENT_ASYNCH_HIDE || 
        eventType == nsIAccessibleEvent::EVENT_ASYNCH_SHOW)) {
      // If frame type didn't change for this event, then we don't actually need to invalidate
      // However, we only keep track of the old frame type for the focus, where it's very
      // important not to destroy and recreate the accessible for minor style changes,
      // such as a:focus { overflow: scroll; }
      nsCOMPtr<nsIContent> focusContent(do_QueryInterface(domNode));
      if (focusContent) {
        nsIFrame *focusFrame = presShell->GetRealPrimaryFrameFor(focusContent);
        nsIAtom *newFrameType =
          (focusFrame && focusFrame->GetStyleVisibility()->IsVisible()) ?
          focusFrame->GetType() : nsnull;

        if (newFrameType == gLastFocusedFrameType) {
          // Don't need to invalidate this current accessible, but can
          // just invalidate the children instead
          FireShowHideEvents(domNode, PR_TRUE, eventType, PR_FALSE, isFromUserInput); 
          continue;
        }
        gLastFocusedFrameType = newFrameType;
      }
    }

    if (eventType == nsIAccessibleEvent::EVENT_DOM_CREATE || 
        eventType == nsIAccessibleEvent::EVENT_ASYNCH_SHOW) {

      nsCOMPtr<nsIAccessible> containerAccessible;
      if (accessible)
        accessible->GetParent(getter_AddRefs(containerAccessible));

      if (!containerAccessible) {
        GetAccessibleInParentChain(domNode, PR_TRUE,
                                   getter_AddRefs(containerAccessible));
        if (!containerAccessible)
          containerAccessible = this;
      }

      if (eventType == nsIAccessibleEvent::EVENT_ASYNCH_SHOW) {
        // For asynch show, delayed invalidatation of parent's children
        nsCOMPtr<nsPIAccessible> privateContainerAccessible =
          do_QueryInterface(containerAccessible);
        if (privateContainerAccessible)
          privateContainerAccessible->InvalidateChildren();

        // Some show events in the subtree may have been removed to 
        // avoid firing redundant events. But, we still need to make sure any
        // accessibles parenting those shown nodes lose their child references.
        InvalidateChildrenInSubtree(domNode);
      }

      // Also fire text changes if the node being created could affect the text in an nsIAccessibleText parent.
      // When a node is being made visible or is inserted, the text in an ancestor hyper text will gain characters
      // At this point we now have the frame and accessible for this node if there is one. That is why we
      // wait to fire this here, instead of in InvalidateCacheSubtree(), where we wouldn't be able to calculate
      // the offset, length and text for the text change.
      if (domNode && domNode != mDOMNode) {
        nsCOMPtr<nsIAccessibleTextChangeEvent> textChangeEvent =
          CreateTextChangeEventForNode(containerAccessible, domNode, accessible, PR_TRUE, PR_TRUE);
        if (textChangeEvent) {
          nsAccEvent::PrepareForEvent(textChangeEvent, isFromUserInput);
          // XXX Queue them up and merge the text change events
          // XXX We need a way to ignore SplitNode and JoinNode() when they
          // do not affect the text within the hypertext
          FireAccessibleEvent(textChangeEvent);
        }
      }

      // Fire show/create events for this node or first accessible descendants of it
      FireShowHideEvents(domNode, PR_FALSE, eventType, PR_FALSE, isFromUserInput); 
      continue;
    }

    if (accessible) {
      if (eventType == nsIAccessibleEvent::EVENT_INTERNAL_LOAD) {
        nsCOMPtr<nsPIAccessibleDocument> docAccessible =
          do_QueryInterface(accessible);
        NS_ASSERTION(docAccessible, "No doc accessible for doc load event");
        if (docAccessible) {
          docAccessible->FireDocLoadEvents(nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_COMPLETE);
        }
      }
      else if (eventType == nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED) {
        nsCOMPtr<nsIAccessibleText> accessibleText = do_QueryInterface(accessible);
        PRInt32 caretOffset;
        if (accessibleText && NS_SUCCEEDED(accessibleText->GetCaretOffset(&caretOffset))) {
#ifdef DEBUG_A11Y
          PRUnichar chAtOffset;
          accessibleText->GetCharacterAtOffset(caretOffset, &chAtOffset);
          printf("\nCaret moved to %d with char %c", caretOffset, chAtOffset);
#endif
#ifdef DEBUG_CARET
          // Test caret line # -- fire an EVENT_ALERT on the focused node so we can watch the
          // line-number object attribute on it
          nsCOMPtr<nsIAccessible> accForFocus;
          GetAccService()->GetAccessibleFor(gLastFocusedNode, getter_AddRefs(accForFocus));
          nsAccUtils::FireAccEvent(nsIAccessibleEvent::EVENT_ALERT, accForFocus);
#endif
          nsCOMPtr<nsIAccessibleCaretMoveEvent> caretMoveEvent =
            new nsAccCaretMoveEvent(accessible, caretOffset);
          if (!caretMoveEvent)
            break; // Out of memory, break out to release kung fu death grip

          FireAccessibleEvent(caretMoveEvent);

          PRInt32 selectionCount;
          accessibleText->GetSelectionCount(&selectionCount);
          if (selectionCount) {  // There's a selection so fire selection change as well
            nsAccUtils::FireAccEvent(nsIAccessibleEvent::EVENT_TEXT_SELECTION_CHANGED,
                                     accessible, PR_TRUE);
          }
        } 
      }
      else if (eventType == nsIAccessibleEvent::EVENT_REORDER) {
        // Fire reorder event if it's unconditional (see InvalidateCacheSubtree
        // method) or if changed node (that is the reason of this reorder event)
        // is accessible or has accessible children.
        nsAccReorderEvent* reorderEvent = nsnull;
        CallQueryInterface(accessibleEvent, &reorderEvent);
        if (reorderEvent->IsUnconditionalEvent() ||
            reorderEvent->HasAccessibleInReasonSubtree()) {
          nsAccEvent::PrepareForEvent(accessibleEvent);
          FireAccessibleEvent(accessibleEvent);
        }
      }
      else {
        // The input state was previously stored with the nsIAccessibleEvent,
        // so use that state now when firing the event
        nsAccEvent::PrepareForEvent(accessibleEvent);
        FireAccessibleEvent(accessibleEvent);
        // Post event processing
        if (eventType == nsIAccessibleEvent::EVENT_ASYNCH_HIDE ||
            eventType == nsIAccessibleEvent::EVENT_DOM_DESTROY) {
          // Shutdown nsIAccessNode's or nsIAccessibles for any DOM nodes in this subtree
          nsCOMPtr<nsIDOMNode> hidingNode;
          accessibleEvent->GetDOMNode(getter_AddRefs(hidingNode));
          if (hidingNode) {
            RefreshNodes(hidingNode); // Will this bite us with asynch events
          }
        }
      }
    }
  }
  mEventsToFire.Clear(); // Clear out array
  NS_RELEASE_THIS(); // Release kung fu death grip

  // After a flood of events, reset so that user input flag is off
  nsAccEvent::ResetLastInputState();

  mInFlushPendingEvents = PR_FALSE;
  return NS_OK;
}

void nsDocAccessible::FlushEventsCallback(nsITimer *aTimer, void *aClosure)
{
  nsPIAccessibleDocument *accessibleDoc = static_cast<nsPIAccessibleDocument*>(aClosure);
  NS_ASSERTION(accessibleDoc, "How did we get here without an accessible document?");
  if (accessibleDoc) {
    // A lot of crashes were happening here, so now we're reffing the doc
    // now until the events are flushed
    accessibleDoc->FlushPendingEvents();
  }
}

void nsDocAccessible::InvalidateChildrenInSubtree(nsIDOMNode *aStartNode)
{
  nsCOMPtr<nsIAccessNode> accessNode;
  GetCachedAccessNode(aStartNode, getter_AddRefs(accessNode));
  nsCOMPtr<nsPIAccessible> accessible(do_QueryInterface(accessNode));
  if (accessible)
    accessible->InvalidateChildren();

  // Invalidate accessible children in the DOM subtree 
  nsCOMPtr<nsINode> node = do_QueryInterface(aStartNode);
  PRInt32 index, numChildren = node->GetChildCount();
  for (index = 0; index < numChildren; index ++) {
    nsCOMPtr<nsIDOMNode> childNode = do_QueryInterface(node->GetChildAt(index));
    if (childNode)
      InvalidateChildrenInSubtree(childNode);
  }
}

void nsDocAccessible::RefreshNodes(nsIDOMNode *aStartNode)
{
  if (mAccessNodeCache.Count() <= 1) {
    return; // All we have is a doc accessible. There is nothing to invalidate, quit early
  }

  nsCOMPtr<nsIAccessNode> accessNode;
  GetCachedAccessNode(aStartNode, getter_AddRefs(accessNode));

  // Shut down accessible subtree, which may have been created for
  // anonymous content subtree
  nsCOMPtr<nsIAccessible> accessible(do_QueryInterface(accessNode));
  if (accessible) {
    // Fire menupopup end if a menu goes away
    PRUint32 role = nsAccUtils::Role(accessible);
    if (role == nsIAccessibleRole::ROLE_MENUPOPUP) {
      nsCOMPtr<nsIDOMNode> domNode;
      accessNode->GetDOMNode(getter_AddRefs(domNode));
      nsCOMPtr<nsIDOMXULPopupElement> popup(do_QueryInterface(domNode));
      if (!popup) {
        // Popup elements already fire these via DOMMenuInactive
        // handling in nsRootAccessible::HandleEvent
        nsAccUtils::FireAccEvent(nsIAccessibleEvent::EVENT_MENUPOPUP_END,
                                 accessible);
      }
    }
    nsCOMPtr<nsPIAccessible> privateAccessible = do_QueryInterface(accessible);
    NS_ASSERTION(privateAccessible, "No nsPIAccessible for nsIAccessible");

    nsCOMPtr<nsIAccessible> childAccessible;
    // we only need to shutdown the accessibles here if one of them has been created
    privateAccessible->GetCachedFirstChild(getter_AddRefs(childAccessible));
    if (childAccessible) {
      nsCOMPtr<nsIArray> children;
      // use GetChildren() to fetch children at one time, instead of using
      // GetNextSibling(), because after we shutdown the first child,
      // mNextSibling will be set null.
      accessible->GetChildren(getter_AddRefs(children));
      PRUint32 childCount =0;
      if (children)
        children->GetLength(&childCount);
      nsCOMPtr<nsIDOMNode> possibleAnonNode;
      for (PRUint32 index = 0; index < childCount; index++) {
        nsCOMPtr<nsIAccessNode> childAccessNode;
        children->QueryElementAt(index, NS_GET_IID(nsIAccessNode),
                                 getter_AddRefs(childAccessNode));
        childAccessNode->GetDOMNode(getter_AddRefs(possibleAnonNode));
        nsCOMPtr<nsIContent> iterContent = do_QueryInterface(possibleAnonNode);
        if (iterContent && iterContent->IsInAnonymousSubtree()) {
          // IsInAnonymousSubtree() check is a perf win -- make sure we don't
          // shut down the same subtree twice since we'll reach non-anon content via
          // DOM traversal later in this method
          RefreshNodes(possibleAnonNode);
        }
      }
    }
  }

  // Shutdown ordinary content subtree as well -- there may be
  // access node children which are not full accessible objects
  nsCOMPtr<nsIDOMNode> nextNode, iterNode;
  aStartNode->GetFirstChild(getter_AddRefs(nextNode));
  while (nextNode) {
    nextNode.swap(iterNode);
    RefreshNodes(iterNode);
    iterNode->GetNextSibling(getter_AddRefs(nextNode));
  }

  if (!accessNode)
    return;

  if (accessNode == this) {
    // Don't shutdown our doc object -- this may just be from the finished loading.
    // We will completely shut it down when the pagehide event is received
    // However, we must invalidate the doc accessible's children in order to be sure
    // all pointers to them are correct
    InvalidateChildren();
    return;
  }

  // Shut down the actual accessible or access node
  void *uniqueID;
  accessNode->GetUniqueID(&uniqueID);
  nsRefPtr<nsAccessNode> accNode = nsAccUtils::QueryAccessNode(accessNode);
  accNode->Shutdown();

  // Remove from hash table as well
  mAccessNodeCache.Remove(uniqueID);
}

NS_IMETHODIMP nsDocAccessible::InvalidateCacheSubtree(nsIContent *aChild,
                                                      PRUint32 aChangeEventType)
{
  PRBool isHiding = 
    aChangeEventType == nsIAccessibleEvent::EVENT_ASYNCH_HIDE ||
    aChangeEventType == nsIAccessibleEvent::EVENT_DOM_DESTROY;

  PRBool isShowing = 
    aChangeEventType == nsIAccessibleEvent::EVENT_ASYNCH_SHOW ||
    aChangeEventType == nsIAccessibleEvent::EVENT_DOM_CREATE;

  PRBool isChanging = 
    aChangeEventType == nsIAccessibleEvent::EVENT_DOM_SIGNIFICANT_CHANGE ||
    aChangeEventType == nsIAccessibleEvent::EVENT_ASYNCH_SIGNIFICANT_CHANGE;

  NS_ASSERTION(isChanging || isHiding || isShowing,
               "Incorrect aChangeEventType passed in");

  PRBool isAsynch = 
    aChangeEventType == nsIAccessibleEvent::EVENT_ASYNCH_HIDE ||
    aChangeEventType == nsIAccessibleEvent::EVENT_ASYNCH_SHOW ||
    aChangeEventType == nsIAccessibleEvent::EVENT_ASYNCH_SIGNIFICANT_CHANGE;

  // Invalidate cache subtree
  // We have to check for accessibles for each dom node by traversing DOM tree
  // instead of just the accessible tree, although that would be faster
  // Otherwise we might miss the nsAccessNode's that are not nsAccessible's.

  NS_ENSURE_TRUE(mDOMNode, NS_ERROR_FAILURE);
  nsCOMPtr<nsIDOMNode> childNode = aChild ? do_QueryInterface(aChild) : mDOMNode;

  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);
  
  if (!mIsContentLoaded) {
    // Still loading document
    if (mAccessNodeCache.Count() <= 1) {
      // Still loading and no accessibles has yet been created other than this
      // doc accessible. In this case we optimize
      // by not firing SHOW/HIDE/REORDER events for every document mutation
      // caused by page load, since AT is not going to want to grab the
      // document and listen to these changes until after the page is first loaded
      // Leave early, and ensure mAccChildCount stays uninitialized instead of 0,
      // which it is if anyone asks for its children right now.
      return InvalidateChildren();
    }
    nsIEventStateManager *esm = presShell->GetPresContext()->EventStateManager();
    NS_ENSURE_TRUE(esm, NS_ERROR_FAILURE);
    if (!esm->IsHandlingUserInputExternal()) {
      // Changes during page load, but not caused by user input
      // Just invalidate accessible hierarchy and return,
      // otherwise the page load time slows down way too much
      nsCOMPtr<nsIAccessible> containerAccessible;
      GetAccessibleInParentChain(childNode, PR_FALSE, getter_AddRefs(containerAccessible));
      if (!containerAccessible) {
        containerAccessible = this;
      }
      nsCOMPtr<nsPIAccessible> privateContainer = do_QueryInterface(containerAccessible);
      return privateContainer->InvalidateChildren();
    }     
    // else: user input, so we must fall through and for full handling,
    // e.g. fire the mutation events. Note: user input could cause DOM_CREATE
    // during page load if user typed into an input field or contentEditable area
  }

  // Update last change state information
  nsCOMPtr<nsIAccessNode> childAccessNode;
  GetCachedAccessNode(childNode, getter_AddRefs(childAccessNode));
  nsCOMPtr<nsIAccessible> childAccessible = do_QueryInterface(childAccessNode);

#ifdef DEBUG_A11Y
  nsAutoString localName;
  childNode->GetLocalName(localName);
  const char *hasAccessible = childAccessible ? " (acc)" : "";
  if (aChangeEventType == nsIAccessibleEvent::EVENT_ASYNCH_HIDE) {
    printf("[Hide %s %s]\n", NS_ConvertUTF16toUTF8(localName).get(), hasAccessible);
  }
  else if (aChangeEventType == nsIAccessibleEvent::EVENT_ASYNCH_SHOW) {
    printf("[Show %s %s]\n", NS_ConvertUTF16toUTF8(localName).get(), hasAccessible);
  }
  else if (aChangeEventType == nsIAccessibleEvent::EVENT_ASYNCH_SIGNIFICANT_CHANGE) {
    printf("[Layout change %s %s]\n", NS_ConvertUTF16toUTF8(localName).get(), hasAccessible);
  }
  else if (aChangeEventType == nsIAccessibleEvent::EVENT_DOM_CREATE) {
    printf("[Create %s %s]\n", NS_ConvertUTF16toUTF8(localName).get(), hasAccessible);
  }
  else if (aChangeEventType == nsIAccessibleEvent::EVENT_DOM_DESTROY) {
    printf("[Destroy  %s %s]\n", NS_ConvertUTF16toUTF8(localName).get(), hasAccessible);
  }
  else if (aChangeEventType == nsIAccessibleEvent::EVENT_DOM_SIGNIFICANT_CHANGE) {
    printf("[Type change %s %s]\n", NS_ConvertUTF16toUTF8(localName).get(), hasAccessible);
  }
#endif

  nsCOMPtr<nsIAccessible> containerAccessible;
  GetAccessibleInParentChain(childNode, PR_TRUE, getter_AddRefs(containerAccessible));
  if (!containerAccessible) {
    containerAccessible = this;
  }

  if (!isShowing) {
    // Fire EVENT_ASYNCH_HIDE or EVENT_DOM_DESTROY
    if (isHiding) {
      nsCOMPtr<nsIContent> content(do_QueryInterface(childNode));
      if (content) {
        nsIFrame *frame = presShell->GetPrimaryFrameFor(content);
        if (frame) {
          nsIFrame *frameParent = frame->GetParent();
          if (!frameParent || !frameParent->GetStyleVisibility()->IsVisible()) {
            // Ancestor already hidden or being hidden at the same time:
            // don't process redundant hide event
            // This often happens when visibility is cleared for node,
            // which hides an entire subtree -- we get notified for each
            // node in the subtree and need to collate the hide events ourselves.
            return NS_OK;
          }
        }
      }
    }

    PRUint32 removalEventType = isAsynch ? nsIAccessibleEvent::EVENT_ASYNCH_HIDE :
                                           nsIAccessibleEvent::EVENT_DOM_DESTROY;

    // Fire an event if the accessible existed for node being hidden, otherwise
    // for the first line accessible descendants. Fire before the accessible(s) away.
    nsresult rv = FireShowHideEvents(childNode, PR_FALSE, removalEventType, PR_TRUE, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
    if (childNode != mDOMNode) { // Fire text change unless the node being removed is for this doc
      // When a node is hidden or removed, the text in an ancestor hyper text will lose characters
      // At this point we still have the frame and accessible for this node if there was one
      // XXX Collate events when a range is deleted
      // XXX We need a way to ignore SplitNode and JoinNode() when they
      // do not affect the text within the hypertext
      nsCOMPtr<nsIAccessibleTextChangeEvent> textChangeEvent =
        CreateTextChangeEventForNode(containerAccessible, childNode, childAccessible,
                                     PR_FALSE, isAsynch);
      if (textChangeEvent) {
        FireAccessibleEvent(textChangeEvent);
      }
    }
  }

  // We need to get an accessible for the mutation event's container node
  // If there is no accessible for that node, we need to keep moving up the parent
  // chain so there is some accessible.
  // We will use this accessible to fire the accessible mutation event.
  // We're guaranteed success, because we will eventually end up at the doc accessible,
  // and there is always one of those.

  if (aChild && !isHiding) {
    if (!isAsynch) {
      // DOM already updated with new objects -- invalidate parent's children now
      // For asynch we must wait until layout updates before we invalidate the children
      nsCOMPtr<nsPIAccessible> privateContainerAccessible =
        do_QueryInterface(containerAccessible);
      if (privateContainerAccessible) {
        privateContainerAccessible->InvalidateChildren();
      }
    }
    // Fire EVENT_SHOW, EVENT_MENUPOPUP_START for newly visible content.
    // Fire after a short timer, because we want to make sure the view has been
    // updated to make this accessible content visible. If we don't wait,
    // the assistive technology may receive the event and then retrieve
    // nsIAccessibleStates::STATE_INVISIBLE for the event's accessible object.
    PRUint32 additionEvent = isAsynch ? nsIAccessibleEvent::EVENT_ASYNCH_SHOW :
                                        nsIAccessibleEvent::EVENT_DOM_CREATE;
    FireDelayedToolkitEvent(additionEvent, childNode,
                            nsAccEvent::eCoalesceFromSameSubtree,
                            isAsynch);

    // Check to see change occured in an ARIA menu, and fire
    // an EVENT_MENUPOPUP_START if it did.
    nsRoleMapEntry *roleMapEntry = nsAccUtils::GetRoleMapEntry(childNode);
    if (roleMapEntry && roleMapEntry->role == nsIAccessibleRole::ROLE_MENUPOPUP) {
      FireDelayedToolkitEvent(nsIAccessibleEvent::EVENT_MENUPOPUP_START,
                              childNode, nsAccEvent::eRemoveDupes,
                              isAsynch);
    }

    // Check to see if change occured inside an alert, and fire an EVENT_ALERT if it did
    nsIContent *ancestor = aChild;
    while (PR_TRUE) {
      if (roleMapEntry && roleMapEntry->role == nsIAccessibleRole::ROLE_ALERT) {
        nsCOMPtr<nsIDOMNode> alertNode(do_QueryInterface(ancestor));
        FireDelayedToolkitEvent(nsIAccessibleEvent::EVENT_ALERT, alertNode,
                                nsAccEvent::eRemoveDupes, isAsynch);
        break;
      }
      ancestor = ancestor->GetParent();
      nsCOMPtr<nsIDOMNode> ancestorNode = do_QueryInterface(ancestor);
      if (!ancestorNode) {
        break;
      }
      roleMapEntry = nsAccUtils::GetRoleMapEntry(ancestorNode);
    }
  }

  FireValueChangeForTextFields(containerAccessible);

  // Fire an event so the MSAA clients know the children have changed. Also
  // the event is used internally by MSAA part.

  // We need to fire a delayed reorder event for the accessible parent of the
  // changed node. We fire an unconditional reorder event if the changed node or
  // one of its children is already accessible. In the case of show events, the
  // accessible object might not be created yet for an otherwise accessible
  // changed node (because its frame might not be constructed yet). In this case
  // we fire a conditional reorder event, so that we will later check whether
  // the changed node is accessible or has accessible children.
  // Filtering/coalescing of these events happens during the queue flush.

  PRBool isUnconditionalEvent = childAccessible ||
    aChild && nsAccUtils::HasAccessibleChildren(childNode);

  nsCOMPtr<nsIAccessibleEvent> reorderEvent =
    new nsAccReorderEvent(containerAccessible, isAsynch,
                          isUnconditionalEvent,
                          aChild ? childNode.get() : nsnull);
  NS_ENSURE_TRUE(reorderEvent, NS_ERROR_OUT_OF_MEMORY);

  FireDelayedAccessibleEvent(reorderEvent);

  return NS_OK;
}

NS_IMETHODIMP
nsDocAccessible::GetAccessibleInParentChain(nsIDOMNode *aNode,
                                            PRBool aCanCreate,
                                            nsIAccessible **aAccessible)
{
  // Find accessible in parent chain of DOM nodes, or return null
  *aAccessible = nsnull;
  nsCOMPtr<nsIDOMNode> currentNode(aNode), parentNode;
  nsCOMPtr<nsIAccessNode> accessNode;

  nsIAccessibilityService *accService = GetAccService();
  NS_ENSURE_TRUE(accService, NS_ERROR_FAILURE);

  do {
    currentNode->GetParentNode(getter_AddRefs(parentNode));
    currentNode = parentNode;
    if (!currentNode) {
      NS_ADDREF_THIS();
      *aAccessible = this;
      break;
    }

    nsCOMPtr<nsIDOMNode> relevantNode;
    if (NS_SUCCEEDED(accService->GetRelevantContentNodeFor(currentNode, getter_AddRefs(relevantNode))) && relevantNode) {
      currentNode = relevantNode;
    }
    if (aCanCreate) {
      accService->GetAccessibleInWeakShell(currentNode, mWeakShell, aAccessible);
    }
    else { // Only return cached accessibles, don't create anything
      nsCOMPtr<nsIAccessNode> accessNode;
      GetCachedAccessNode(currentNode, getter_AddRefs(accessNode)); // AddRefs
      if (accessNode) {
        CallQueryInterface(accessNode, aAccessible); // AddRefs
      }
    }
  } while (!*aAccessible);

  return NS_OK;
}

nsresult
nsDocAccessible::FireShowHideEvents(nsIDOMNode *aDOMNode, PRBool aAvoidOnThisNode, PRUint32 aEventType,
                                    PRBool aDelay, PRBool aForceIsFromUserInput)
{
  NS_ENSURE_ARG(aDOMNode);

  nsCOMPtr<nsIAccessible> accessible;
  if (!aAvoidOnThisNode) {
    if (aEventType == nsIAccessibleEvent::EVENT_ASYNCH_HIDE ||
        aEventType == nsIAccessibleEvent::EVENT_DOM_DESTROY) {
      // Don't allow creation for accessibles when nodes going away
      nsCOMPtr<nsIAccessNode> accessNode;
      GetCachedAccessNode(aDOMNode, getter_AddRefs(accessNode));
      accessible = do_QueryInterface(accessNode);
    } else {
      // Allow creation of new accessibles for show events
      GetAccService()->GetAttachedAccessibleFor(aDOMNode,
                                                getter_AddRefs(accessible));
    }
  }

  if (accessible) {
    // Found an accessible, so fire the show/hide on it and don't
    // look further into this subtree
    PRBool isAsynch = aEventType == nsIAccessibleEvent::EVENT_ASYNCH_HIDE ||
                      aEventType == nsIAccessibleEvent::EVENT_ASYNCH_SHOW;

    nsCOMPtr<nsIAccessibleEvent> event =
      new nsAccEvent(aEventType, accessible, isAsynch,
                     nsAccEvent::eCoalesceFromSameSubtree);
    NS_ENSURE_TRUE(event, NS_ERROR_OUT_OF_MEMORY);
    if (aForceIsFromUserInput) {
      nsAccEvent::PrepareForEvent(event, aForceIsFromUserInput);
    }
    if (aDelay) {
      return FireDelayedAccessibleEvent(event);
    }
    return FireAccessibleEvent(event);
  }

  // Could not find accessible to show hide yet, so fire on any
  // accessible descendants in this subtree
  nsCOMPtr<nsINode> node(do_QueryInterface(aDOMNode));
  PRUint32 count = node->GetChildCount();
  for (PRUint32 index = 0; index < count; index++) {
    nsCOMPtr<nsIDOMNode> childNode = do_QueryInterface(node->GetChildAt(index));
    nsresult rv = FireShowHideEvents(childNode, PR_FALSE, aEventType,
                                     aDelay, aForceIsFromUserInput);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

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

//-----------------------------------------------------
// construction
//-----------------------------------------------------
nsDocAccessible::nsDocAccessible(nsIDOMNode *aDOMNode, nsIWeakReference* aShell):
  nsHyperTextAccessibleWrap(aDOMNode, aShell), mWnd(nsnull),
  mScrollPositionChangedTicks(0), mIsContentLoaded(PR_FALSE),
  mAriaPropTypes(eCheckNamespaced)
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
    // Find mAriaPropTypes: the initial type of ARIA properties that should be checked for
    if (!mDocument) {
      NS_WARNING("No document!");
      return;
    }
    
    nsCOMPtr<nsIDOMNSHTMLDocument> htmlDoc(do_QueryInterface(mDocument));
    if (htmlDoc) {
      nsAutoString mimeType;
      GetMimeType(mimeType);
      mAriaPropTypes = eCheckHyphenated;
      if (! mimeType.EqualsLiteral("text/html")) {
        mAriaPropTypes |= eCheckNamespaced;
      }
    }
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
    nsAccUtils::GetDocShellTreeItemFor(mDOMNode);
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

NS_INTERFACE_MAP_BEGIN(nsDocAccessible)
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
  if (aName.IsEmpty() && mParent) {
    rv = mParent->GetName(aName);
  }

  return rv;
}

NS_IMETHODIMP nsDocAccessible::GetRole(PRUint32 *aRole)
{
  *aRole = nsIAccessibleRole::ROLE_PANE; // Fall back

  nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem =
    nsAccUtils::GetDocShellTreeItemFor(mDOMNode);
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

NS_IMETHODIMP nsDocAccessible::GetValue(nsAString& aValue)
{
  return GetURL(aValue);
}

NS_IMETHODIMP 
nsDocAccessible::GetDescription(nsAString& aDescription)
{
  nsAutoString description;
  GetTextFromRelationID(eAria_describedby, description);
  aDescription = description;
  return NS_OK;
}

NS_IMETHODIMP
nsDocAccessible::GetState(PRUint32 *aState, PRUint32 *aExtraState)
{
  // nsAccessible::GetState() always fail for document accessible.
  nsAccessible::GetState(aState, aExtraState);
  if (!mDOMNode)
    return NS_OK;

#ifdef MOZ_XUL
  nsCOMPtr<nsIXULDocument> xulDoc(do_QueryInterface(mDocument));
  if (!xulDoc)
#endif
  {
    // XXX Need to invent better check to see if doc is focusable,
    // which it should be if it is scrollable. A XUL document could be focusable.
    // See bug 376803.
    *aState |= nsIAccessibleStates::STATE_FOCUSABLE;
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
 
  if (frame != nsnull) {
    if (!CheckVisibilityInParentChain(mDocument, frame->GetViewExternal())) {
      *aState |= nsIAccessibleStates::STATE_INVISIBLE;
    }
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
  GetState(&state, nsnull);
  if (0 == (state & nsIAccessibleStates::STATE_FOCUSABLE)) {
    return NS_ERROR_FAILURE; // Not focusable
  }

  nsCOMPtr<nsIDocShellTreeItem> treeItem =
    nsAccUtils::GetDocShellTreeItemFor(mDOMNode);
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
  NS_ENSURE_TRUE(mDocument, NS_ERROR_FAILURE);

  if (!mDocument->HasFlag(NODE_IS_EDITABLE)) {
    return NS_OK; // Document not editable
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

NS_IMETHODIMP nsDocAccessible::CacheAccessNode(void *aUniqueID, nsIAccessNode *aAccessNode)
{
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

NS_IMETHODIMP
nsDocAccessible::GetAttributes(nsIPersistentProperties **aAttributes)
{
  NS_ENSURE_ARG_POINTER(aAttributes);
  *aAttributes = nsnull;

  return mDOMNode ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsDocAccessible::Init()
{
  PutCacheEntry(gGlobalDocAccessibleCache, mWeakShell, this);

  AddEventListeners();

  nsresult rv = nsHyperTextAccessibleWrap::Init();

  if (mRoleMapEntry && mRoleMapEntry->role != nsIAccessibleRole::ROLE_DIALOG &&
      mRoleMapEntry->role != nsIAccessibleRole::ROLE_APPLICATION &&
      mRoleMapEntry->role != nsIAccessibleRole::ROLE_ALERT &&
      mRoleMapEntry->role != nsIAccessibleRole::ROLE_DOCUMENT) {
    // Document accessible can only have certain roles
    // This was set in nsAccessible::Init() based on dynamic role attribute
    mRoleMapEntry = nsnull; // role attribute is not valid for a document
  }

  return rv;
}


NS_IMETHODIMP nsDocAccessible::Destroy()
{
  if (mWeakShell) {
    gGlobalDocAccessibleCache.Remove(static_cast<void*>(mWeakShell));
  }
  return Shutdown();
}

NS_IMETHODIMP nsDocAccessible::Shutdown()
{
  if (!mWeakShell) {
    return NS_OK;  // Already shutdown
  }

  nsCOMPtr<nsIDocShellTreeItem> treeItem =
    nsAccUtils::GetDocShellTreeItemFor(mDOMNode);
  ShutdownChildDocuments(treeItem);

  if (mDocLoadTimer) {
    mDocLoadTimer->Cancel();
    mDocLoadTimer = nsnull;
  }

  RemoveEventListeners();

  mWeakShell = nsnull;  // Avoid reentrancy

  if (mFireEventTimer) {
    mFireEventTimer->Cancel();
    mFireEventTimer = nsnull;
  }
  mEventsToFire.Clear();

  ClearCache(mAccessNodeCache);

  mDocument = nsnull;

  return nsHyperTextAccessibleWrap::Shutdown();
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
      nsCOMPtr<nsPIAccessNode> accessNode = do_QueryInterface(docAccessible);
      if (accessNode) {
        accessNode->Shutdown();
      }
    }
  }
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
      nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(mDocument);
      caretAccessible->AddDocSelectionListener(domDoc);
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
  }

  nsRefPtr<nsRootAccessible> rootAccessible(GetRootAccessible());
  if (rootAccessible) {
    nsRefPtr<nsCaretAccessible> caretAccessible = rootAccessible->GetCaretAccessible();
    if (caretAccessible) {
      nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(mDocument);
      caretAccessible->RemoveDocSelectionListener(domDoc);
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

  if (mIsContentLoaded == isFinished) {
    return NS_OK;
  }
  mIsContentLoaded = isFinished;

  if (isFinished) {
    // Need to wait until scrollable view is available
    AddScrollListener();
    nsCOMPtr<nsIAccessible> parent(nsAccessible::GetParent());
    nsCOMPtr<nsPIAccessible> privateAccessible(do_QueryInterface(parent));
    if (privateAccessible) {
      // Make the parent forget about the old document as a child
      privateAccessible->InvalidateChildren();
    }
    // Use short timer before firing state change event for finished doc,
    // because the window is made visible asynchronously
    if (!mDocLoadTimer) {
      mDocLoadTimer = do_CreateInstance("@mozilla.org/timer;1");
    }
    if (mDocLoadTimer) {
      mDocLoadTimer->InitWithFuncCallback(DocLoadCallback, this, 0,
                                          nsITimer::TYPE_ONE_SHOT);
    }
  } else {
    nsCOMPtr<nsIDocShellTreeItem> treeItem =
      nsAccUtils::GetDocShellTreeItemFor(mDOMNode);
    if (!treeItem) {
      return NS_OK;
    }

    nsCOMPtr<nsIDocShellTreeItem> sameTypeRoot;
    treeItem->GetSameTypeRootTreeItem(getter_AddRefs(sameTypeRoot));
    if (sameTypeRoot != treeItem) {
      return NS_OK; 
    }

    // Loading document: fire EVENT_STATE_CHANGE to set STATE_BUSY
    nsCOMPtr<nsIAccessibleStateChangeEvent> accEvent =
      new nsAccStateChangeEvent(this, nsIAccessibleStates::STATE_BUSY,
                                PR_FALSE, PR_TRUE);
    FireAccessibleEvent(accEvent);
  }

  nsAccUtils::FireAccEvent(aEventType, this);
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
    // State editable will now be set, readonly is now clear
    nsCOMPtr<nsIAccessibleStateChangeEvent> event =
      new nsAccStateChangeEvent(this, nsIAccessibleStates::EXT_STATE_EDITABLE,
                                PR_TRUE, PR_TRUE);
    FireAccessibleEvent(event);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocAccessible::GetAriaPropTypes(PRUint32 *aAriaPropTypes) 
{
  *aAriaPropTypes = mAriaPropTypes;
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
      gLastFocusedAccessiblesState = State(focusedAccessible);
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

  nsCOMPtr<nsISupports> container = mDocument->GetContainer();
  nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(container);
  if (!docShell) {
    return;
  }
  if (aNameSpaceID == kNameSpaceID_WAIProperties) {
    // Using setAttributeNS() in HTML to set namespaced ARIA properties.
    // From this point forward, check namespaced properties, which
    // take precedence over hyphenated properties, since in text/html
    // that can only be set dynamically.
    mAriaPropTypes |= eCheckNamespaced;
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
  if (!targetNode || !IsNodeRelevant(targetNode)) {
    return;
  }

  // Since we're in synchronous code, we can store whether the current attribute
  // change is from user input or not. If the attribute change causes an asynchronous
  // layout change, that event can use the last known user input state
  nsAccEvent::PrepareForEvent(targetNode);

  // Universal boolean properties that don't require a role.
  if (aAttribute == nsAccessibilityAtoms::disabled ||
      (aAttribute == nsAccessibilityAtoms::aria_disabled && (mAriaPropTypes & eCheckHyphenated))) {
    // Fire the state change whether disabled attribute is
    // set for XUL, HTML or ARIA namespace.
    // Checking the namespace would not seem to gain us anything, because
    // disabled really is going to mean the same thing in any namespace.
    // We use the attribute instead of the disabled state bit because
    // ARIA's aaa:disabled does not affect the disabled state bit
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
  nsCOMPtr<nsIAtom> ariaAttribute;
  if (aNameSpaceID == kNameSpaceID_WAIProperties) {
    ariaAttribute = aAttribute;
  }
  else if (mAriaPropTypes & eCheckHyphenated && aNameSpaceID == kNameSpaceID_None) {
    // Check for hyphenated aria-foo property?
    const char* attributeName;
    aAttribute->GetUTF8String(&attributeName);
    if (!PL_strncmp("aria-", attributeName, 5)) {
      // Convert to WAI property atom attribute
      ariaAttribute = do_GetAtom(attributeName + 5);
    }
  }
  if (ariaAttribute) {  // We have an ARIA attribute
    ARIAAttributeChanged(aContent, ariaAttribute);
    return;
  }

  if (aAttribute == nsAccessibilityAtoms::role ||
      aAttribute == nsAccessibilityAtoms::href ||
      aAttribute == nsAccessibilityAtoms::onclick ||
      aAttribute == nsAccessibilityAtoms::droppable) {
    // Not worth the expense to ensure which namespace these are in
    // It doesn't kill use to recreate the accessible even if the attribute was used
    // in the wrong namespace or an element that doesn't support it
    InvalidateCacheSubtree(aContent, nsIAccessibleEvent::EVENT_DOM_SIGNIFICANT_CHANGE);
    return;
  }

  if (aAttribute == nsAccessibilityAtoms::selected) {
    // DHTML or XUL selection
    nsCOMPtr<nsIAccessible> multiSelect = GetMultiSelectFor(targetNode);
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
                              multiSelectDOMNode, nsnull, eAllowDupes);

      static nsIContent::AttrValuesArray strings[] =
        {&nsAccessibilityAtoms::_empty, &nsAccessibilityAtoms::_false, nsnull};
      if (aContent->FindAttrValueIn(kNameSpaceID_None,
                                    nsAccessibilityAtoms::selected,
                                    strings, eCaseMatters) !=
          nsIContent::ATTR_VALUE_NO_MATCH) {

        FireDelayedToolkitEvent(nsIAccessibleEvent::EVENT_SELECTION_REMOVE,
                                targetNode, nsnull);
        return;
      }

      FireDelayedToolkitEvent(nsIAccessibleEvent::EVENT_SELECTION_ADD,
                                                  targetNode, nsnull);
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

  if (aAttribute == nsAccessibilityAtoms::required) {
    nsCOMPtr<nsIAccessibleStateChangeEvent> event =
      new nsAccStateChangeEvent(targetNode,
                                nsIAccessibleStates::STATE_REQUIRED,
                                PR_FALSE);
    FireDelayedAccessibleEvent(event);
    return;
  }

  if (aAttribute == nsAccessibilityAtoms::invalid) {
    nsCOMPtr<nsIAccessibleStateChangeEvent> event =
      new nsAccStateChangeEvent(targetNode,
                                nsIAccessibleStates::STATE_INVALID,
                                PR_FALSE);
    FireDelayedAccessibleEvent(event);
    return;
  }

  if (aAttribute == nsAccessibilityAtoms::activedescendant) {
    // The activedescendant universal property redirects accessible focus events
    // to the element with the id that activedescendant points to
    nsCOMPtr<nsIDOMNode> currentFocus = GetCurrentFocus();
    if (currentFocus == targetNode) {
      nsRefPtr<nsRootAccessible> rootAcc = GetRootAccessible();
      if (rootAcc)
        rootAcc->FireAccessibleFocusEvent(nsnull, currentFocus, nsnull, PR_TRUE);
    }
    return;
  }

  if (!HasRoleAttribute(aContent)) {
    // We don't care about these other ARIA attribute changes unless there is
    // an ARIA role set for the element
    // XXX: we should check the role map to see if the changed property is
    // relevant for that particular role.
    return;
  }

  if (aAttribute == nsAccessibilityAtoms::checked ||
      aAttribute == nsAccessibilityAtoms::pressed) {
    const PRUint32 kState = (aAttribute == nsAccessibilityAtoms::checked) ?
                            nsIAccessibleStates::STATE_CHECKED : 
                            nsIAccessibleStates::STATE_PRESSED;
    nsCOMPtr<nsIAccessibleStateChangeEvent> event =
      new nsAccStateChangeEvent(targetNode, kState, PR_FALSE);
    FireDelayedAccessibleEvent(event);
    if (targetNode == gLastFocusedNode) {
      // State changes for MIXED state currently only supported for focused item, because
      // otherwise we would need access to the old attribute value in this listener.
      // This is because we don't know if the previous value of aaa:checked or aaa:pressed was "mixed"
      // without caching that info.
      nsCOMPtr<nsIAccessible> accessible;
      event->GetAccessible(getter_AddRefs(accessible));
      if (accessible) {
        PRBool wasMixed = (gLastFocusedAccessiblesState & nsIAccessibleStates::STATE_MIXED) != 0;
        PRBool isMixed  = (State(accessible) & nsIAccessibleStates::STATE_MIXED) != 0;
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

  if (aAttribute == nsAccessibilityAtoms::expanded) {
    nsCOMPtr<nsIAccessibleStateChangeEvent> event =
      new nsAccStateChangeEvent(targetNode,
                                nsIAccessibleStates::STATE_EXPANDED,
                                PR_FALSE);
    FireDelayedAccessibleEvent(event);
    return;
  }

  if (aAttribute == nsAccessibilityAtoms::readonly) {
    nsCOMPtr<nsIAccessibleStateChangeEvent> event =
      new nsAccStateChangeEvent(targetNode,
                                nsIAccessibleStates::STATE_READONLY,
                                PR_FALSE);
    FireDelayedAccessibleEvent(event);
    return;
  }

  if (aAttribute == nsAccessibilityAtoms::valuenow) {
    FireDelayedToolkitEvent(nsIAccessibleEvent::EVENT_VALUE_CHANGE,
                            targetNode, nsnull);
    return;
  }

  if (aAttribute == nsAccessibilityAtoms::multiselectable) {
    // This affects whether the accessible supports nsIAccessibleSelectable.
    // COM says we cannot change what interfaces are supported on-the-fly,
    // so invalidate this object. A new one will be created on demand.
    if (HasRoleAttribute(aContent)) {
      // The multiselectable and other waistate attributes only take affect
      // when dynamic content role is present
      InvalidateCacheSubtree(aContent, nsIAccessibleEvent::EVENT_DOM_SIGNIFICANT_CHANGE);
    }
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
      if (!nsAccUtils::IsAncestorOf(aChangeNode, childNode)) {
        break;  // We only want accessibles with DOM nodes as children of this node
      }
      length += TextLength(child);
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
    length = TextLength(aAccessibleForChangeNode);
    if (Role(aAccessibleForChangeNode) == nsIAccessibleRole::ROLE_WHITESPACE) {  // newline
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
                                                  void *aData,
                                                  EDupeEventRule aAllowDupes,
                                                  PRBool aIsAsynch)
{
  nsCOMPtr<nsIAccessibleEvent> event =
    new nsAccEvent(aEvent, aDOMNode, aData, PR_TRUE);
  NS_ENSURE_TRUE(event, NS_ERROR_OUT_OF_MEMORY);

  return FireDelayedAccessibleEvent(event, aAllowDupes, aIsAsynch);
}

nsresult
nsDocAccessible::FireDelayedAccessibleEvent(nsIAccessibleEvent *aEvent,
                                            EDupeEventRule aAllowDupes,
                                            PRBool aIsAsynch)
{
  NS_ENSURE_TRUE(aEvent, NS_ERROR_FAILURE);

  PRBool isTimerStarted = PR_TRUE;
  PRInt32 numQueuedEvents = mEventsToFire.Count();
  if (!mFireEventTimer) {
    // Do not yet have a timer going for firing another event.
    mFireEventTimer = do_CreateInstance("@mozilla.org/timer;1");
    NS_ENSURE_TRUE(mFireEventTimer, NS_ERROR_OUT_OF_MEMORY);
  }

  PRUint32 newEventType;
  aEvent->GetEventType(&newEventType);

  nsCOMPtr<nsIDOMNode> newEventDOMNode;
  aEvent->GetDOMNode(getter_AddRefs(newEventDOMNode));

  if (!aIsAsynch) {
    // If already asynchronous don't call PrepareFromEvent() -- it
    // should only be called while ESM still knows if the event occurred
    // originally because of user input
    nsAccEvent::PrepareForEvent(newEventDOMNode);
  }

  if (numQueuedEvents == 0) {
    isTimerStarted = PR_FALSE;
  } else if (aAllowDupes == eCoalesceFromSameSubtree) {
    // Especially for mutation events, we will define a duplicate event
    // as one on the same node or on a descendant node.
    // This prevents a flood of events when a subtree is changed.
    for (PRInt32 index = 0; index < numQueuedEvents; index ++) {
      nsIAccessibleEvent *accessibleEvent = mEventsToFire[index];
      NS_ASSERTION(accessibleEvent, "Array item is not an accessible event");
      if (!accessibleEvent) {
        continue;
      }
      PRUint32 eventType;
      accessibleEvent->GetEventType(&eventType);
      if (eventType == newEventType) {
        nsCOMPtr<nsIDOMNode> domNode;
        accessibleEvent->GetDOMNode(getter_AddRefs(domNode));
        if (newEventDOMNode == domNode || nsAccUtils::IsAncestorOf(newEventDOMNode, domNode)) {
          mEventsToFire.RemoveObjectAt(index);
          // The other event is the same type, but in a descendant of this
          // event, so remove that one. The umbrella event in the ancestor
          // is already enough
          -- index;
          -- numQueuedEvents;
        }
        else if (nsAccUtils::IsAncestorOf(domNode, newEventDOMNode)) {
          // There is a better SHOW/HIDE event (it's in an ancestor)
          return NS_OK;
        }
      }    
    }
  } else if (aAllowDupes == eRemoveDupes) {
    // Check for repeat events. If a redundant event exists remove
    // original and put the new event at the end of the queue
    // so it is fired after the others
    for (PRInt32 index = 0; index < numQueuedEvents; index ++) {
      nsIAccessibleEvent *accessibleEvent = mEventsToFire[index];
      NS_ASSERTION(accessibleEvent, "Array item is not an accessible event");
      if (!accessibleEvent) {
        continue;
      }
      PRUint32 eventType;
      accessibleEvent->GetEventType(&eventType);
      if (eventType == newEventType) {
        nsCOMPtr<nsIDOMNode> domNode;
        accessibleEvent->GetDOMNode(getter_AddRefs(domNode));
        if (domNode == newEventDOMNode) {
          mEventsToFire.RemoveObjectAt(index);
          -- index;
          -- numQueuedEvents;
        }
      }
    }
  }

  mEventsToFire.AppendObject(aEvent);
  if (!isTimerStarted) {
    // This is be the first delayed event in queue, start timer
    // so that event gets fired via FlushEventsCallback
    mFireEventTimer->InitWithFuncCallback(FlushEventsCallback,
                                          static_cast<nsPIAccessibleDocument*>(this),
                                          0, nsITimer::TYPE_ONE_SHOT);
  }

  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::FlushPendingEvents()
{
  PRUint32 length = mEventsToFire.Count();
  NS_ASSERTION(length, "How did we get here without events to fire?");
  PRUint32 index;
  for (index = 0; index < length; index ++) {
    nsCOMPtr<nsIAccessibleEvent> accessibleEvent(
      do_QueryInterface(mEventsToFire[index]));
    NS_ASSERTION(accessibleEvent, "Array item is not an accessible event");

    nsCOMPtr<nsIAccessible> accessible;
    accessibleEvent->GetAccessible(getter_AddRefs(accessible));

    PRUint32 eventType;
    accessibleEvent->GetEventType(&eventType);
    if (eventType == nsIAccessibleEvent::EVENT_DOM_CREATE || 
        eventType == nsIAccessibleEvent::EVENT_ASYNCH_SHOW) {
      nsCOMPtr<nsIAccessible> containerAccessible;
      if (accessible) {
        accessible->GetParent(getter_AddRefs(containerAccessible));
        nsCOMPtr<nsPIAccessible> privateContainerAccessible =
          do_QueryInterface(containerAccessible);
        if (privateContainerAccessible)
          privateContainerAccessible->InvalidateChildren();
      }

      // Also fire text changes if the node being created could affect the text in an nsIAccessibleText parent.
      // When a node is being made visible or is inserted, the text in an ancestor hyper text will gain characters
      // At this point we now have the frame and accessible for this node if there is one. That is why we
      // wait to fire this here, instead of in InvalidateCacheSubtree(), where we wouldn't be able to calculate
      // the offset, length and text for the text change.
      nsCOMPtr<nsIDOMNode> domNode;
      accessibleEvent->GetDOMNode(getter_AddRefs(domNode));
      PRBool isFromUserInput;
      accessibleEvent->GetIsFromUserInput(&isFromUserInput);
      if (domNode && domNode != mDOMNode) {
        if (!containerAccessible)
          GetAccessibleInParentChain(domNode, PR_TRUE,
                                     getter_AddRefs(containerAccessible));

        nsCOMPtr<nsIAccessibleTextChangeEvent> textChangeEvent =
          CreateTextChangeEventForNode(containerAccessible, domNode, accessible, PR_TRUE, PR_TRUE);
        if (textChangeEvent) {
          nsCOMPtr<nsIDOMNode> hyperTextNode;
          textChangeEvent->GetDOMNode(getter_AddRefs(hyperTextNode));
          nsAccEvent::PrepareForEvent(hyperTextNode, isFromUserInput);
          // XXX Queue them up and merge the text change events
          // XXX We need a way to ignore SplitNode and JoinNode() when they
          // do not affect the text within the hypertext
          FireAccessibleEvent(textChangeEvent);
        }
      }

      // Fire show/create events for this node or first accessible descendants of it
      FireShowHideEvents(domNode, eventType, PR_FALSE, isFromUserInput); 
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
          nsCOMPtr<nsIAccessibleCaretMoveEvent> caretMoveEvent =
            new nsAccCaretMoveEvent(accessible, caretOffset);
          NS_ENSURE_TRUE(caretMoveEvent, NS_ERROR_OUT_OF_MEMORY);

          FireAccessibleEvent(caretMoveEvent);

          PRInt32 selectionCount;
          accessibleText->GetSelectionCount(&selectionCount);
          if (selectionCount) {  // There's a selection so fire selection change as well
            nsAccUtils::FireAccEvent(nsIAccessibleEvent::EVENT_TEXT_SELECTION_CHANGED,
                                     accessible, PR_TRUE);
          }
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
          // Invalidate children
          nsCOMPtr<nsIAccessible> containerAccessible;
          accessible->GetParent(getter_AddRefs(containerAccessible));
          nsCOMPtr<nsPIAccessible> privateContainerAccessible =
            do_QueryInterface(containerAccessible);
          if (privateContainerAccessible) {
            privateContainerAccessible->InvalidateChildren();
          }
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
  return NS_OK;
}

void nsDocAccessible::FlushEventsCallback(nsITimer *aTimer, void *aClosure)
{
  nsPIAccessibleDocument *accessibleDoc = static_cast<nsPIAccessibleDocument*>(aClosure);
  NS_ASSERTION(accessibleDoc, "How did we get here without an accessible document?");
  accessibleDoc->FlushPendingEvents();
}

void nsDocAccessible::RefreshNodes(nsIDOMNode *aStartNode)
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
      if (accessNode != static_cast<nsIAccessNode*>(this)) {

        nsCOMPtr<nsIAccessible> accessible(do_QueryInterface(accessNode));
        if (accessible) {
          // Fire menupopupend events for menu popups that go away
          PRUint32 role = Role(accessible);
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
        }

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
    if (aChangeEventType == nsIAccessibleEvent::EVENT_DOM_CREATE) {
      nsCOMPtr<nsIPresShell> presShell = GetPresShell();
      NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);
      nsIEventStateManager *esm = presShell->GetPresContext()->EventStateManager();
      NS_ENSURE_TRUE(esm, NS_ERROR_FAILURE);
      if (!esm->IsHandlingUserInputExternal()) {
        // Adding content during page load, but not caused by user input
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
  }

  // Update last change state information
  nsCOMPtr<nsIAccessNode> childAccessNode;
  GetCachedAccessNode(childNode, getter_AddRefs(childAccessNode));
  nsCOMPtr<nsIAccessible> childAccessible = do_QueryInterface(childAccessNode);
  if (!childAccessible && !isHiding) {
    // If not about to hide it, make sure there's an accessible so we can fire an
    // event for it
    GetAccService()->GetAttachedAccessibleFor(childNode,
                                              getter_AddRefs(childAccessible));
  }

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
    nsCOMPtr<nsIContent> content(do_QueryInterface(childNode));
    if (isHiding) {
      nsCOMPtr<nsIPresShell> presShell = GetPresShell();
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
    nsresult rv = FireShowHideEvents(childNode, removalEventType, PR_TRUE, PR_FALSE);
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
    // Fire EVENT_SHOW, EVENT_MENUPOPUP_START for newly visible content.
    // Fire after a short timer, because we want to make sure the view has been
    // updated to make this accessible content visible. If we don't wait,
    // the assistive technology may receive the event and then retrieve
    // nsIAccessibleStates::STATE_INVISIBLE for the event's accessible object.
    PRUint32 additionEvent = isAsynch ? nsIAccessibleEvent::EVENT_ASYNCH_SHOW :
                                        nsIAccessibleEvent::EVENT_DOM_CREATE;
    FireDelayedToolkitEvent(additionEvent, childNode, nsnull,
                            eCoalesceFromSameSubtree, isAsynch);

    // Check to see change occured in an ARIA menu, and fire an EVENT_MENUPOPUP_START if it did
    if (ARIARoleEquals(aChild, "menu")) {
      FireDelayedToolkitEvent(nsIAccessibleEvent::EVENT_MENUPOPUP_START,
                              childNode, nsnull, eAllowDupes, isAsynch);
    }

    // Check to see if change occured inside an alert, and fire an EVENT_ALERT if it did
    nsIContent *ancestor = aChild;
    while (ancestor) {
      if (ARIARoleEquals(ancestor, "alert")) {
        nsCOMPtr<nsIDOMNode> alertNode(do_QueryInterface(ancestor));
        FireDelayedToolkitEvent(nsIAccessibleEvent::EVENT_ALERT, alertNode, nsnull,
                                eRemoveDupes, isAsynch);
        break;
      }
      ancestor = ancestor->GetParent();
    }
  }

  if (!isShowing) {
    // Fire an event so the assistive technology knows the children have changed
    // This is only used by older MSAA clients. Newer ones should derive this
    // from SHOW and HIDE so that they don't fetch extra objects
    if (childAccessible) {
      nsCOMPtr<nsIAccessibleEvent> reorderEvent =
        new nsAccEvent(nsIAccessibleEvent::EVENT_REORDER, containerAccessible, nsnull, PR_TRUE);
      NS_ENSURE_TRUE(reorderEvent, NS_ERROR_OUT_OF_MEMORY);
      FireDelayedAccessibleEvent(reorderEvent, eCoalesceFromSameSubtree, isAsynch);
    }
  }

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
nsDocAccessible::FireShowHideEvents(nsIDOMNode *aDOMNode, PRUint32 aEventType,
                                    PRBool aDelay, PRBool aForceIsFromUserInput)
{
  NS_ENSURE_ARG(aDOMNode);

  nsCOMPtr<nsIAccessible> accessible;
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

  if (accessible) {
    // Found an accessible, so fire the show/hide on it and don't
    // look further into this subtree
    PRBool isAsynch = aEventType == nsIAccessibleEvent::EVENT_ASYNCH_HIDE ||
                      aEventType == nsIAccessibleEvent::EVENT_ASYNCH_SHOW;

    nsCOMPtr<nsIAccessibleEvent> event =
      new nsAccEvent(aEventType, accessible, nsnull, isAsynch);
    NS_ENSURE_TRUE(event, NS_ERROR_OUT_OF_MEMORY);
    if (aForceIsFromUserInput) {
      nsAccEvent::PrepareForEvent(aDOMNode, aForceIsFromUserInput);
    }
    if (aDelay) {
      return FireDelayedAccessibleEvent(event, eCoalesceFromSameSubtree, isAsynch);
    }
    return FireAccessibleEvent(event);
  }

  // Could not find accessible to show hide yet, so fire on any
  // accessible descendants in this subtree
  nsCOMPtr<nsIContent> content(do_QueryInterface(aDOMNode));
  PRUint32 count = content->GetChildCount();
  for (PRUint32 index = 0; index < count; index++) {
    nsCOMPtr<nsIDOMNode> childNode = do_QueryInterface(content->GetChildAt(index));
    nsresult rv = FireShowHideEvents(childNode, aEventType,
                                     aDelay, aForceIsFromUserInput);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

void nsDocAccessible::DocLoadCallback(nsITimer *aTimer, void *aClosure)
{
  // Doc has finished loading, fire "load finished" event
  // By using short timer we can wait make the window visible, 
  // which it does asynchronously. This avoids confusing the screen reader with a
  // hidden window. Waiting also allows us to see of the document has focus,
  // which is important because we only fire doc loaded events for focused documents.

  nsDocAccessible *docAcc =
    reinterpret_cast<nsDocAccessible*>(aClosure);
  if (!docAcc) {
    return;
  }

  // Fire doc finished event
  nsCOMPtr<nsIDOMNode> docDomNode;
  docAcc->GetDOMNode(getter_AddRefs(docDomNode));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(docDomNode));
  if (doc) {
    nsCOMPtr<nsISupports> container = doc->GetContainer();
    nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem = do_QueryInterface(container);
    if (!docShellTreeItem) {
      return;
    }
    nsCOMPtr<nsIDocShellTreeItem> sameTypeRoot;
    docShellTreeItem->GetSameTypeRootTreeItem(getter_AddRefs(sameTypeRoot));
    if (sameTypeRoot != docShellTreeItem) {
      // A frame or iframe has finished loading new content
      docAcc->InvalidateCacheSubtree(nsnull, nsIAccessibleEvent::EVENT_DOM_SIGNIFICANT_CHANGE);
      return;
    }

    // Fire STATE_CHANGE event for doc load finish if focus is in same doc tree
    if (gLastFocusedNode) {
      nsCOMPtr<nsIDocShellTreeItem> focusedTreeItem =
        nsAccUtils::GetDocShellTreeItemFor(gLastFocusedNode);
      if (focusedTreeItem) {
        nsCOMPtr<nsIDocShellTreeItem> sameTypeRootOfFocus;
        focusedTreeItem->GetSameTypeRootTreeItem(getter_AddRefs(sameTypeRootOfFocus));
        if (sameTypeRoot == sameTypeRootOfFocus) {
          nsCOMPtr<nsIAccessibleStateChangeEvent> accEvent =
            new nsAccStateChangeEvent(docAcc, nsIAccessibleStates::STATE_BUSY,
                                      PR_FALSE, PR_FALSE);
          docAcc->FireAccessibleEvent(accEvent);
          docAcc->FireAnchorJumpEvent();
        }
      }
    }
  }
}


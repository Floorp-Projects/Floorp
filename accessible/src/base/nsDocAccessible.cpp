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

#include "nsAccCache.h"
#include "nsAccessibilityAtoms.h"
#include "nsAccessibilityService.h"
#include "nsAccTreeWalker.h"
#include "nsAccUtils.h"
#include "nsRootAccessible.h"
#include "nsTextEquivUtils.h"

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
#include "nsIDOMXULDocument.h"
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
#include "nsIViewManager.h"
#include "nsIScrollableFrame.h"
#include "nsUnicharUtils.h"
#include "nsIURI.h"
#include "nsIWebNavigation.h"
#include "nsFocusManager.h"
#include "mozilla/dom/Element.h"
#ifdef MOZ_XUL
#include "nsIXULDocument.h"
#endif

namespace dom = mozilla::dom;

////////////////////////////////////////////////////////////////////////////////
// Static member initialization

PRUint32 nsDocAccessible::gLastFocusedAccessiblesState = 0;


////////////////////////////////////////////////////////////////////////////////
// Constructor/desctructor

nsDocAccessible::
  nsDocAccessible(nsIDocument *aDocument, nsIContent *aRootContent,
                  nsIWeakReference *aShell) :
  nsHyperTextAccessibleWrap(aRootContent, aShell),
  mDocument(aDocument), mScrollPositionChangedTicks(0), mIsLoaded(PR_FALSE)
{
  // XXX aaronl should we use an algorithm for the initial cache size?
  mAccessibleCache.Init(kDefaultCacheSize);
  mNodeToAccessibleMap.Init(kDefaultCacheSize);

  // For GTK+ native window, we do nothing here.
  if (!mDocument)
    return;

  // nsAccDocManager creates document accessible when scrollable frame is
  // available already, it should be safe time to add scroll listener.
  AddScrollListener();
}

nsDocAccessible::~nsDocAccessible()
{
}


////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDocAccessible)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDocAccessible, nsAccessible)
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mEventQueue");
  cb.NoteXPCOMChild(tmp->mEventQueue.get());

  PRUint32 i, length = tmp->mChildDocuments.Length();
  for (i = 0; i < length; ++i) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mChildDocuments[i]");
    cb.NoteXPCOMChild(static_cast<nsIAccessible*>(tmp->mChildDocuments[i].get()));
  }

  CycleCollectorTraverseCache(tmp->mAccessibleCache, &cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDocAccessible, nsAccessible)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mEventQueue)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSTARRAY(mChildDocuments)
  tmp->mNodeToAccessibleMap.Clear();
  ClearCache(tmp->mAccessibleCache);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDocAccessible)
  NS_INTERFACE_MAP_STATIC_AMBIGUOUS(nsDocAccessible)
  NS_INTERFACE_MAP_ENTRY(nsIAccessibleDocument)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentObserver)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAccessibleDocument)
    foundInterface = 0;

  nsresult status;
  if (!foundInterface) {
    // HTML document accessible must inherit from nsHyperTextAccessible to get
    // support text interfaces. XUL document accessible doesn't need this.
    // However at some point we may push <body> to implement the interfaces and
    // return nsDocAccessible to inherit from nsAccessibleWrap.

    nsCOMPtr<nsIDOMXULDocument> xulDoc(do_QueryInterface(mDocument));
    if (xulDoc)
      status = nsAccessible::QueryInterface(aIID, (void**)&foundInterface);
    else
      status = nsHyperTextAccessible::QueryInterface(aIID,
                                                     (void**)&foundInterface);
  } else {
    NS_ADDREF(foundInterface);
    status = NS_OK;
  }

  *aInstancePtr = foundInterface;
  return status;
}

NS_IMPL_ADDREF_INHERITED(nsDocAccessible, nsHyperTextAccessible)
NS_IMPL_RELEASE_INHERITED(nsDocAccessible, nsHyperTextAccessible)


////////////////////////////////////////////////////////////////////////////////
// nsIAccessible

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

// nsAccessible public method
PRUint32
nsDocAccessible::NativeRole()
{
  nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem =
    nsCoreUtils::GetDocShellTreeItemFor(mDocument);
  if (docShellTreeItem) {
    nsCOMPtr<nsIDocShellTreeItem> sameTypeRoot;
    docShellTreeItem->GetSameTypeRootTreeItem(getter_AddRefs(sameTypeRoot));
    PRInt32 itemType;
    docShellTreeItem->GetItemType(&itemType);
    if (sameTypeRoot == docShellTreeItem) {
      // Root of content or chrome tree
      if (itemType == nsIDocShellTreeItem::typeChrome)
        return nsIAccessibleRole::ROLE_CHROME_WINDOW;

      if (itemType == nsIDocShellTreeItem::typeContent) {
#ifdef MOZ_XUL
        nsCOMPtr<nsIXULDocument> xulDoc(do_QueryInterface(mDocument));
        if (xulDoc)
          return nsIAccessibleRole::ROLE_APPLICATION;
#endif
        return nsIAccessibleRole::ROLE_DOCUMENT;
      }
    }
    else if (itemType == nsIDocShellTreeItem::typeContent) {
      return nsIAccessibleRole::ROLE_DOCUMENT;
    }
  }

  return nsIAccessibleRole::ROLE_PANE; // Fall back;
}

// nsAccessible public method
void
nsDocAccessible::SetRoleMapEntry(nsRoleMapEntry* aRoleMapEntry)
{
  NS_ASSERTION(mDocument, "No document during initialization!");
  if (!mDocument)
    return;

  mRoleMapEntry = aRoleMapEntry;

  nsIDocument *parentDoc = mDocument->GetParentDocument();
  if (!parentDoc)
    return; // No parent document for the root document

  // Allow use of ARIA role from outer to override
  nsIContent *ownerContent = parentDoc->FindContentForSubDocument(mDocument);
  if (ownerContent) {
    nsRoleMapEntry *roleMapEntry = nsAccUtils::GetRoleMapEntry(ownerContent);
    if (roleMapEntry)
      mRoleMapEntry = roleMapEntry; // Override
  }
}

NS_IMETHODIMP 
nsDocAccessible::GetDescription(nsAString& aDescription)
{
  if (mParent)
    mParent->GetDescription(aDescription);

  if (aDescription.IsEmpty()) {
    nsAutoString description;
    nsTextEquivUtils::
      GetTextEquivFromIDRefs(this, nsAccessibilityAtoms::aria_describedby,
                             description);
    aDescription = description;
  }

  return NS_OK;
}

// nsAccessible public method
nsresult
nsDocAccessible::GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState)
{
  *aState = 0;

  if (IsDefunct()) {
    if (aExtraState)
      *aExtraState = nsIAccessibleStates::EXT_STATE_DEFUNCT;

    return NS_OK_DEFUNCT_OBJECT;
  }

  if (aExtraState) {
    // The root content of the document might be removed so that mContent is
    // out of date.
    *aExtraState = (mContent->GetCurrentDoc() == mDocument) ?
      0 : nsIAccessibleStates::EXT_STATE_STALE;
  }

#ifdef MOZ_XUL
  nsCOMPtr<nsIXULDocument> xulDoc(do_QueryInterface(mDocument));
  if (!xulDoc)
#endif
  {
    // XXX Need to invent better check to see if doc is focusable,
    // which it should be if it is scrollable. A XUL document could be focusable.
    // See bug 376803.
    *aState |= nsIAccessibleStates::STATE_FOCUSABLE;
    if (gLastFocusedNode == mDocument)
      *aState |= nsIAccessibleStates::STATE_FOCUSED;
  }

  if (nsCoreUtils::IsDocumentBusy(mDocument)) {
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

// nsAccessible public method
nsresult
nsDocAccessible::GetARIAState(PRUint32 *aState, PRUint32 *aExtraState)
{
  // Combine with states from outer doc
  NS_ENSURE_ARG_POINTER(aState);
  nsresult rv = nsAccessible::GetARIAState(aState, aExtraState);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mParent)  // Allow iframe/frame etc. to have final state override via ARIA
    return mParent->GetARIAState(aState, aExtraState);

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
  // XXXndeakin P3 accessibility shouldn't be caching the focus
  if (!gLastFocusedNode) {
    *aFocusedChild = nsnull;
    return NS_OK;
  }

  // Return an accessible for the current global focus, which does not have to
  // be contained within the current document.
  NS_IF_ADDREF(*aFocusedChild = GetAccService()->GetAccessible(gLastFocusedNode));
  return NS_OK;
}

NS_IMETHODIMP nsDocAccessible::TakeFocus()
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  PRUint32 state;
  GetStateInternal(&state, nsnull);
  if (0 == (state & nsIAccessibleStates::STATE_FOCUSABLE)) {
    return NS_ERROR_FAILURE; // Not focusable
  }

  // Focus the document.
  nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
  NS_ENSURE_STATE(fm);

  nsCOMPtr<nsIDOMElement> newFocus;
  return fm->MoveFocus(mDocument->GetWindow(), nsnull,
                       nsIFocusManager::MOVEFOCUS_ROOT, 0,
                       getter_AddRefs(newFocus));
}


////////////////////////////////////////////////////////////////////////////////
// nsIAccessibleDocument

NS_IMETHODIMP nsDocAccessible::GetURL(nsAString& aURL)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

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
  NS_ENSURE_ARG_POINTER(aWindow);
  *aWindow = GetNativeWindow();
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

NS_IMETHODIMP
nsDocAccessible::GetDOMDocument(nsIDOMDocument **aDOMDocument)
{
  NS_ENSURE_ARG_POINTER(aDOMDocument);
  *aDOMDocument = nsnull;

  if (mDocument)
    CallQueryInterface(mDocument, aDOMDocument);

  return NS_OK;
}

NS_IMETHODIMP
nsDocAccessible::GetParentDocument(nsIAccessibleDocument** aDocument)
{
  NS_ENSURE_ARG_POINTER(aDocument);
  *aDocument = nsnull;

  if (!IsDefunct())
    NS_IF_ADDREF(*aDocument = ParentDocument());

  return NS_OK;
}

NS_IMETHODIMP
nsDocAccessible::GetChildDocumentCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  if (!IsDefunct())
    *aCount = ChildDocumentCount();

  return NS_OK;
}

NS_IMETHODIMP
nsDocAccessible::GetChildDocumentAt(PRUint32 aIndex,
                                    nsIAccessibleDocument** aDocument)
{
  NS_ENSURE_ARG_POINTER(aDocument);
  *aDocument = nsnull;

  if (IsDefunct())
    return NS_OK;

  NS_IF_ADDREF(*aDocument = GetChildDocumentAt(aIndex));
  return *aDocument ? NS_OK : NS_ERROR_INVALID_ARG;
}

// nsIAccessibleHyperText method
NS_IMETHODIMP nsDocAccessible::GetAssociatedEditor(nsIEditor **aEditor)
{
  NS_ENSURE_ARG_POINTER(aEditor);
  *aEditor = nsnull;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // Check if document is editable (designMode="on" case). Otherwise check if
  // the html:body (for HTML document case) or document element is editable.
  if (!mDocument->HasFlag(NODE_IS_EDITABLE) &&
      !mContent->HasFlag(NODE_IS_EDITABLE))
    return NS_OK;

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

// nsDocAccessible public method
nsAccessible *
nsDocAccessible::GetCachedAccessible(nsINode *aNode)
{
  nsAccessible* accessible = mNodeToAccessibleMap.Get(aNode);

  // No accessible in the cache, check if the given ID is unique ID of this
  // document accessible.
  if (!accessible) {
    if (GetNode() != aNode)
      return nsnull;

    accessible = this;
  }

#ifdef DEBUG
  // All cached accessible nodes should be in the parent
  // It will assert if not all the children were created
  // when they were first cached, and no invalidation
  // ever corrected parent accessible's child cache.
  nsAccessible* parent(accessible->GetCachedParent());
  if (parent)
    parent->TestChildCache(accessible);
#endif

  return accessible;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessNode

PRBool
nsDocAccessible::Init()
{
  NS_LOG_ACCDOCCREATE_FOR("document initialize", mDocument, this)

  // Initialize event queue.
  mEventQueue = new nsAccEventQueue(this);
  if (!mEventQueue)
    return PR_FALSE;

  AddEventListeners();

  nsDocAccessible* parentDocument = mParent->GetDocAccessible();
  if (parentDocument)
    parentDocument->AppendChildDocument(this);

  // Fire reorder event to notify new accessible document has been created and
  // attached to the tree.
  nsRefPtr<AccEvent> reorderEvent =
    new AccEvent(nsIAccessibleEvent::EVENT_REORDER, mParent, eAutoDetect,
                 AccEvent::eCoalesceFromSameSubtree);
  if (reorderEvent) {
    FireDelayedAccessibleEvent(reorderEvent);
    return PR_TRUE;
  }

  return PR_FALSE;
}

void
nsDocAccessible::Shutdown()
{
  if (!mWeakShell) // already shutdown
    return;

  NS_LOG_ACCDOCDESTROY_FOR("document shutdown", mDocument, this)

  if (mEventQueue) {
    mEventQueue->Shutdown();
    mEventQueue = nsnull;
  }

  RemoveEventListeners();

  if (mParent) {
    nsDocAccessible* parentDocument = mParent->GetDocAccessible();
    if (parentDocument)
      parentDocument->RemoveChildDocument(this);

    mParent->RemoveChild(this);
  }

  // Walk the array backwards because child documents remove themselves from the
  // array as they are shutdown.
  PRInt32 childDocCount = mChildDocuments.Length();
  for (PRInt32 idx = childDocCount - 1; idx >= 0; idx--)
    mChildDocuments[idx]->Shutdown();

  mChildDocuments.Clear();

  mWeakShell = nsnull;  // Avoid reentrancy

  mNodeToAccessibleMap.Clear();
  ClearCache(mAccessibleCache);

  nsCOMPtr<nsIDocument> kungFuDeathGripDoc = mDocument;
  mDocument = nsnull;

  nsHyperTextAccessibleWrap::Shutdown();

  GetAccService()->NotifyOfDocumentShutdown(kungFuDeathGripDoc);
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

PRBool
nsDocAccessible::IsDefunct()
{
  return nsHyperTextAccessibleWrap::IsDefunct() || !mDocument;
}

// nsDocAccessible protected member
void nsDocAccessible::GetBoundsRect(nsRect& aBounds, nsIFrame** aRelativeFrame)
{
  *aRelativeFrame = GetFrame();

  nsIDocument *document = mDocument;
  nsIDocument *parentDoc = nsnull;

  while (document) {
    nsIPresShell *presShell = document->GetShell();
    if (!presShell) {
      return;
    }

    nsRect scrollPort;
    nsIScrollableFrame* sf = presShell->GetRootScrollFrameAsScrollableExternal();
    if (sf) {
      scrollPort = sf->GetScrollPortRect();
    } else {
      nsIFrame* rootFrame = presShell->GetRootFrame();
      if (!rootFrame) {
        return;
      }
      scrollPort = rootFrame->GetRect();
    }

    if (parentDoc) {  // After first time thru loop
      // XXXroc bogus code! scrollPort is relative to the viewport of
      // this document, but we're intersecting rectangles derived from
      // multiple documents and assuming they're all in the same coordinate
      // system. See bug 514117.
      aBounds.IntersectRect(scrollPort, aBounds);
    }
    else {  // First time through loop
      aBounds = scrollPort;
    }

    document = parentDoc = document->GetParentDocument();
  }
}

// nsDocAccessible protected member
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
    nsRefPtr<nsRootAccessible> rootAccessible = GetRootAccessible();
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

// nsDocAccessible protected member
nsresult nsDocAccessible::RemoveEventListeners()
{
  // Remove listeners associated with content documents
  // Remove scroll position listener
  RemoveScrollListener();

  NS_ASSERTION(mDocument, "No document during removal of listeners.");

  if (mDocument) {
    mDocument->RemoveObserver(this);

    nsCOMPtr<nsISupports> container = mDocument->GetContainer();
    nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem(do_QueryInterface(container));
    NS_ASSERTION(docShellTreeItem, "doc should support nsIDocShellTreeItem.");

    if (docShellTreeItem) {
      PRInt32 itemType;
      docShellTreeItem->GetItemType(&itemType);
      if (itemType == nsIDocShellTreeItem::typeContent) {
        nsCOMPtr<nsICommandManager> commandManager = do_GetInterface(docShellTreeItem);
        if (commandManager) {
          commandManager->RemoveCommandObserver(this, "obs_documentCreated");
        }
      }
    }
  }

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
    nsEventShell::FireEvent(nsIAccessibleEvent::EVENT_SCROLLING_END, docAcc);

    docAcc->mScrollPositionChangedTicks = 0;
    if (docAcc->mScrollWatchTimer) {
      docAcc->mScrollWatchTimer->Cancel();
      docAcc->mScrollWatchTimer = nsnull;
      NS_RELEASE(docAcc); // Release kung fu death grip
    }
  }
}

// nsDocAccessible protected member
void nsDocAccessible::AddScrollListener()
{
  nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mWeakShell));
  if (!presShell)
    return;

  nsIScrollableFrame* sf = presShell->GetRootScrollFrameAsScrollableExternal();
  if (sf) {
    sf->AddScrollPositionListener(this);
    NS_LOG_ACCDOCCREATE_TEXT("add scroll listener")
  }
}

// nsDocAccessible protected member
void nsDocAccessible::RemoveScrollListener()
{
  nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mWeakShell));
  if (!presShell)
    return;
 
  nsIScrollableFrame* sf = presShell->GetRootScrollFrameAsScrollableExternal();
  if (sf) {
    sf->RemoveScrollPositionListener(this);
  }
}

////////////////////////////////////////////////////////////////////////////////
// nsIScrollPositionListener

void nsDocAccessible::ScrollPositionDidChange(nscoord aX, nscoord aY)
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
}

////////////////////////////////////////////////////////////////////////////////
// nsIObserver

NS_IMETHODIMP nsDocAccessible::Observe(nsISupports *aSubject, const char *aTopic,
                                       const PRUnichar *aData)
{
  if (!nsCRT::strcmp(aTopic,"obs_documentCreated")) {    
    // State editable will now be set, readonly is now clear
    // Normally we only fire delayed events created from the node, not an
    // accessible object. See the AccStateChangeEvent constructor for details
    // about this exceptional case.
    nsRefPtr<AccEvent> event =
      new AccStateChangeEvent(this, nsIAccessibleStates::EXT_STATE_EDITABLE,
                              PR_TRUE, PR_TRUE);
    FireDelayedAccessibleEvent(event);
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIDocumentObserver

NS_IMPL_NSIDOCUMENTOBSERVER_CORE_STUB(nsDocAccessible)
NS_IMPL_NSIDOCUMENTOBSERVER_LOAD_STUB(nsDocAccessible)
NS_IMPL_NSIDOCUMENTOBSERVER_STYLE_STUB(nsDocAccessible)

void
nsDocAccessible::AttributeWillChange(nsIDocument *aDocument,
                                     dom::Element* aElement,
                                     PRInt32 aNameSpaceID,
                                     nsIAtom* aAttribute, PRInt32 aModType)
{
  // XXX TODO: bugs 381599 467143 472142 472143
  // Here we will want to cache whatever state we are potentially interested in,
  // such as the existence of aria-pressed for button (so we know if we need to
  // newly expose it as a toggle button) etc.
}

void
nsDocAccessible::AttributeChanged(nsIDocument *aDocument,
                                  dom::Element* aElement,
                                  PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                                  PRInt32 aModType)
{
  AttributeChangedImpl(aElement, aNameSpaceID, aAttribute);

  // If it was the focused node, cache the new state
  if (aElement == gLastFocusedNode) {
    nsAccessible *focusedAccessible = GetAccService()->GetAccessible(aElement);
    if (focusedAccessible)
      gLastFocusedAccessiblesState = nsAccUtils::State(focusedAccessible);
  }
}

// nsDocAccessible protected member
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

  if (!IsContentLoaded())
    return; // Still loading, ignore setting of initial attributes

  nsCOMPtr<nsIPresShell> shell = GetPresShell();
  if (!shell) {
    return; // Document has been shut down
  }

  NS_ASSERTION(aContent, "No node for attr modified");

  // Universal boolean properties that don't require a role. Fire the state
  // change when disabled or aria-disabled attribute is set.
  if (aAttribute == nsAccessibilityAtoms::disabled ||
      aAttribute == nsAccessibilityAtoms::aria_disabled) {

    // Note. Checking the XUL or HTML namespace would not seem to gain us
    // anything, because disabled attribute really is going to mean the same
    // thing in any namespace.

    // Note. We use the attribute instead of the disabled state bit because
    // ARIA's aria-disabled does not affect the disabled state bit.

    nsRefPtr<AccEvent> enabledChangeEvent =
      new AccStateChangeEvent(aContent,
                              nsIAccessibleStates::EXT_STATE_ENABLED,
                              PR_TRUE);

    FireDelayedAccessibleEvent(enabledChangeEvent);

    nsRefPtr<AccEvent> sensitiveChangeEvent =
      new AccStateChangeEvent(aContent,
                              nsIAccessibleStates::EXT_STATE_SENSITIVE,
                              PR_TRUE);

    FireDelayedAccessibleEvent(sensitiveChangeEvent);
    return;
  }

  // Check for namespaced ARIA attribute
  if (aNameSpaceID == kNameSpaceID_None) {
    // Check for hyphenated aria-foo property?
    if (StringBeginsWith(nsDependentAtomString(aAttribute),
                         NS_LITERAL_STRING("aria-"))) {
      ARIAAttributeChanged(aContent, aAttribute);
    }
  }

  if (aAttribute == nsAccessibilityAtoms::role ||
      aAttribute == nsAccessibilityAtoms::href ||
      aAttribute == nsAccessibilityAtoms::onclick) {
    // Not worth the expense to ensure which namespace these are in
    // It doesn't kill use to recreate the accessible even if the attribute was used
    // in the wrong namespace or an element that doesn't support it
    RecreateAccessible(aContent);
    return;
  }
  
  if (aAttribute == nsAccessibilityAtoms::alt ||
      aAttribute == nsAccessibilityAtoms::title ||
      aAttribute == nsAccessibilityAtoms::aria_label ||
      aAttribute == nsAccessibilityAtoms::aria_labelledby) {
    FireDelayedAccessibleEvent(nsIAccessibleEvent::EVENT_NAME_CHANGE,
                               aContent);
    return;
  }

  if (aAttribute == nsAccessibilityAtoms::selected ||
      aAttribute == nsAccessibilityAtoms::aria_selected) {
    // ARIA or XUL selection

    nsAccessible *multiSelect =
      nsAccUtils::GetMultiSelectableContainer(aContent);
    // Multi selects use selection_add and selection_remove
    // Single select widgets just mirror event_selection for
    // whatever gets event_focus, which is done in
    // nsRootAccessible::FireAccessibleFocusEvent()
    // So right here we make sure only to deal with multi selects
    if (multiSelect) {
      // Need to find the right event to use here, SELECTION_WITHIN would
      // seem right but we had started using it for something else
      FireDelayedAccessibleEvent(nsIAccessibleEvent::EVENT_SELECTION_WITHIN,
                                 multiSelect->GetNode(),
                                 AccEvent::eAllowDupes);

      static nsIContent::AttrValuesArray strings[] =
        {&nsAccessibilityAtoms::_empty, &nsAccessibilityAtoms::_false, nsnull};
      if (aContent->FindAttrValueIn(kNameSpaceID_None, aAttribute,
                                    strings, eCaseMatters) >= 0) {
        FireDelayedAccessibleEvent(nsIAccessibleEvent::EVENT_SELECTION_REMOVE,
                                   aContent);
        return;
      }

      FireDelayedAccessibleEvent(nsIAccessibleEvent::EVENT_SELECTION_ADD,
                                 aContent);
    }
  }

  if (aAttribute == nsAccessibilityAtoms::contenteditable) {
    nsRefPtr<AccEvent> editableChangeEvent =
      new AccStateChangeEvent(aContent,
                              nsIAccessibleStates::EXT_STATE_EDITABLE,
                              PR_TRUE);
    FireDelayedAccessibleEvent(editableChangeEvent);
    return;
  }
}

// nsDocAccessible protected member
void
nsDocAccessible::ARIAAttributeChanged(nsIContent* aContent, nsIAtom* aAttribute)
{
  // Note: For universal/global ARIA states and properties we don't care if
  // there is an ARIA role present or not.

  if (aAttribute == nsAccessibilityAtoms::aria_required) {
    nsRefPtr<AccEvent> event =
      new AccStateChangeEvent(aContent, nsIAccessibleStates::STATE_REQUIRED,
                              PR_FALSE);
    FireDelayedAccessibleEvent(event);
    return;
  }

  if (aAttribute == nsAccessibilityAtoms::aria_invalid) {
    nsRefPtr<AccEvent> event =
      new AccStateChangeEvent(aContent, nsIAccessibleStates::STATE_INVALID,
                              PR_FALSE);
    FireDelayedAccessibleEvent(event);
    return;
  }

  if (aAttribute == nsAccessibilityAtoms::aria_activedescendant) {
    // The activedescendant universal property redirects accessible focus events
    // to the element with the id that activedescendant points to
    nsCOMPtr<nsINode> focusedNode = GetCurrentFocus();
    if (nsCoreUtils::GetRoleContent(focusedNode) == aContent) {
      nsRefPtr<nsRootAccessible> rootAcc = GetRootAccessible();
      if (rootAcc) {
        rootAcc->FireAccessibleFocusEvent(nsnull, focusedNode, nsnull, PR_TRUE);
      }
    }
    return;
  }

  // For aria drag and drop changes we fire a generic attribute change event;
  // at least until native API comes up with a more meaningful event.
  if (aAttribute == nsAccessibilityAtoms::aria_grabbed ||
      aAttribute == nsAccessibilityAtoms::aria_dropeffect) {
    FireDelayedAccessibleEvent(nsIAccessibleEvent::EVENT_OBJECT_ATTRIBUTE_CHANGED,
                               aContent);
  }

  // We treat aria-expanded as a global ARIA state for historical reasons
  if (aAttribute == nsAccessibilityAtoms::aria_expanded) {
    nsRefPtr<AccEvent> event =
      new AccStateChangeEvent(aContent, nsIAccessibleStates::STATE_EXPANDED,
                              PR_FALSE);
    FireDelayedAccessibleEvent(event);
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
    nsRefPtr<AccEvent> event =
      new AccStateChangeEvent(aContent, kState, PR_FALSE);
    FireDelayedAccessibleEvent(event);
    if (aContent == gLastFocusedNode) {
      // State changes for MIXED state currently only supported for focused item, because
      // otherwise we would need access to the old attribute value in this listener.
      // This is because we don't know if the previous value of aria-checked or aria-pressed was "mixed"
      // without caching that info.
      nsAccessible *accessible = event->GetAccessible();
      if (accessible) {
        PRBool wasMixed = (gLastFocusedAccessiblesState & nsIAccessibleStates::STATE_MIXED) != 0;
        PRBool isMixed  =
          (nsAccUtils::State(accessible) & nsIAccessibleStates::STATE_MIXED) != 0;
        if (wasMixed != isMixed) {
          nsRefPtr<AccEvent> event =
            new AccStateChangeEvent(aContent, nsIAccessibleStates::STATE_MIXED,
                                    PR_FALSE, isMixed);
          FireDelayedAccessibleEvent(event);
        }
      }
    }
    return;
  }

  if (aAttribute == nsAccessibilityAtoms::aria_readonly) {
    nsRefPtr<AccEvent> event =
      new AccStateChangeEvent(aContent, nsIAccessibleStates::STATE_READONLY,
                              PR_FALSE);
    FireDelayedAccessibleEvent(event);
    return;
  }

  // Fire value change event whenever aria-valuetext is changed, or
  // when aria-valuenow is changed and aria-valuetext is empty
  if (aAttribute == nsAccessibilityAtoms::aria_valuetext ||      
      (aAttribute == nsAccessibilityAtoms::aria_valuenow &&
       (!aContent->HasAttr(kNameSpaceID_None,
           nsAccessibilityAtoms::aria_valuetext) ||
        aContent->AttrValueIs(kNameSpaceID_None,
            nsAccessibilityAtoms::aria_valuetext, nsAccessibilityAtoms::_empty,
            eCaseMatters)))) {
    FireDelayedAccessibleEvent(nsIAccessibleEvent::EVENT_VALUE_CHANGE,
                               aContent);
    return;
  }

  if (aAttribute == nsAccessibilityAtoms::aria_multiselectable &&
      aContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::role)) {
    // This affects whether the accessible supports SelectAccessible.
    // COM says we cannot change what interfaces are supported on-the-fly,
    // so invalidate this object. A new one will be created on demand.
    RecreateAccessible(aContent);
    return;
  }
}

void nsDocAccessible::ContentAppended(nsIDocument *aDocument,
                                      nsIContent* aContainer,
                                      nsIContent* aFirstNewContent,
                                      PRInt32 /* unused */)
{
}

void nsDocAccessible::ContentStatesChanged(nsIDocument* aDocument,
                                           nsIContent* aContent1,
                                           nsIContent* aContent2,
                                           nsEventStates aStateMask)
{
  if (aStateMask.HasState(NS_EVENT_STATE_CHECKED)) {
    nsHTMLSelectOptionAccessible::SelectionChangedIfOption(aContent1);
    nsHTMLSelectOptionAccessible::SelectionChangedIfOption(aContent2);
  }

  if (aStateMask.HasState(NS_EVENT_STATE_INVALID)) {
    nsRefPtr<AccEvent> event =
      new AccStateChangeEvent(aContent1, nsIAccessibleStates::STATE_INVALID,
                              PR_FALSE, PR_TRUE);
    FireDelayedAccessibleEvent(event);
   }
}

void nsDocAccessible::DocumentStatesChanged(nsIDocument* aDocument,
                                            nsEventStates aStateMask)
{
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
                                 nsIContent* aChild, PRInt32 /* unused */)
{
}

void
nsDocAccessible::ContentRemoved(nsIDocument *aDocument, nsIContent* aContainer,
                                nsIContent* aChild, PRInt32 /* unused */,
                                nsIContent* aPreviousSibling)
{
}

void
nsDocAccessible::ParentChainChanged(nsIContent *aContent)
{
}


////////////////////////////////////////////////////////////////////////////////
// nsAccessible

#ifdef DEBUG_ACCDOCMGR
nsresult
nsDocAccessible::HandleAccEvent(AccEvent* aAccEvent)
{
  NS_LOG_ACCDOCLOAD_HANDLEEVENT(aAccEvent)

  return nsHyperTextAccessible::HandleAccEvent(aAccEvent);

}
#endif

////////////////////////////////////////////////////////////////////////////////
// Public members

void*
nsDocAccessible::GetNativeWindow() const
{
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mWeakShell));
  nsIViewManager* vm = shell->GetViewManager();
  if (vm) {
    nsCOMPtr<nsIWidget> widget;
    vm->GetRootWidget(getter_AddRefs(widget));
    if (widget)
      return widget->GetNativeData(NS_NATIVE_WINDOW);
  }
  return nsnull;
}

nsAccessible*
nsDocAccessible::GetCachedAccessibleByUniqueIDInSubtree(void* aUniqueID)
{
  nsAccessible* child = GetCachedAccessibleByUniqueID(aUniqueID);
  if (child)
    return child;

  PRUint32 childDocCount = mChildDocuments.Length();
  for (PRUint32 childDocIdx= 0; childDocIdx < childDocCount; childDocIdx++) {
    nsDocAccessible* childDocument = mChildDocuments.ElementAt(childDocIdx);
    child = childDocument->GetCachedAccessibleByUniqueIDInSubtree(aUniqueID);
    if (child)
      return child;
  }

  return nsnull;
}

bool
nsDocAccessible::BindToDocument(nsAccessible* aAccessible,
                                nsRoleMapEntry* aRoleMapEntry)
{
  if (!aAccessible)
    return false;

  // Put into DOM node cache.
  if (aAccessible->IsPrimaryForNode() &&
      !mNodeToAccessibleMap.Put(aAccessible->GetNode(), aAccessible))
    return false;

  // Put into unique ID cache.
  if (!mAccessibleCache.Put(aAccessible->UniqueID(), aAccessible)) {
    if (aAccessible->IsPrimaryForNode())
      mNodeToAccessibleMap.Remove(aAccessible->GetNode());

    return false;
  }

  // Initialize the accessible.
  if (!aAccessible->Init()) {
    NS_ERROR("Failed to initialize an accessible!");

    UnbindFromDocument(aAccessible);
    return false;
  }

  aAccessible->SetRoleMapEntry(aRoleMapEntry);
  return true;
}

void
nsDocAccessible::UnbindFromDocument(nsAccessible* aAccessible)
{
  // Remove an accessible from node-to-accessible map if it exists there.
  if (aAccessible->IsPrimaryForNode() &&
      mNodeToAccessibleMap.Get(aAccessible->GetNode()) == aAccessible)
    mNodeToAccessibleMap.Remove(aAccessible->GetNode());

#ifdef DEBUG
  NS_ASSERTION(mAccessibleCache.GetWeak(aAccessible->UniqueID()),
               "Unbinding the unbound accessible!");
#endif

  void* uniqueID = aAccessible->UniqueID();
  aAccessible->Shutdown();
  mAccessibleCache.Remove(uniqueID);
}

void
nsDocAccessible::UpdateTree(nsIContent* aContainerNode,
                            nsIContent* aStartNode,
                            nsIContent* aEndNode,
                            PRBool aIsInsert)
{
  // Content change notification mostly are async, thus we can't detect whether
  // these actions are from user. This information is used to fire or do not
  // fire events to avoid events that are generated because of document loading.
  // Since this information may be not correct then we need to fire some events
  // regardless the document loading state.

  // Update the whole tree of this document accessible when the container is
  // null (document element is inserted or removed).

  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  nsIEventStateManager* esm = presShell->GetPresContext()->EventStateManager();
  PRBool fireAllEvents = PR_TRUE;//IsContentLoaded() || esm->IsHandlingUserInputExternal();

  // XXX: bug 608887 reconsider accessible tree update logic because
  // 1) elements appended outside the HTML body don't get accessibles;
  // 2) the document having elements that should be accessible may function
  // without body.
  nsAccessible* container = nsnull;
  if (aIsInsert) {
    container = aContainerNode ?
      GetAccService()->GetAccessibleOrContainer(aContainerNode, mWeakShell) :
      this;

    // The document children were changed; the root content might be affected.
    if (container == this) {
      nsIContent* rootContent = nsCoreUtils::GetRoleContent(mDocument);

      // No root content (for example HTML document element was inserted but no
      // body). Nothing to update.
      if (!rootContent)
        return;

      // New root content has been inserted, update it and update the tree.
      if (rootContent != mContent)
        mContent = rootContent;
    }

    // XXX: Invalidate parent-child relations for container accessible and its
    // children because there's no good way to find insertion point of new child
    // accessibles into accessible tree. We need to invalidate children even
    // there's no inserted accessibles in the end because accessible children
    // are created while parent recaches child accessibles.
    container->InvalidateChildren();

  } else {
    // Don't create new accessibles on content removal.
    container = aContainerNode ?
      GetAccService()->GetCachedAccessibleOrContainer(aContainerNode) :
      this;
  }

  EIsFromUserInput fromUserInput = esm->IsHandlingUserInputExternal() ?
    eFromUserInput : eNoUserInput;

  // Update the accessible tree in the case of content removal and fire events
  // if allowed.
  PRUint32 updateFlags =
    UpdateTreeInternal(container, aStartNode, aEndNode,
                       aIsInsert, fireAllEvents, fromUserInput);

  // Content insertion/removal is not cause of accessible tree change.
  if (updateFlags == eNoAccessible)
    return;

  // Check to see if change occurred inside an alert, and fire an EVENT_ALERT
  // if it did.
  if (aIsInsert && !(updateFlags & eAlertAccessible)) {
    // XXX: tree traversal is perf issue, accessible should know if they are
    // children of alert accessible to avoid this.
    nsAccessible* ancestor = container;
    while (ancestor) {
      const nsRoleMapEntry* roleMapEntry = ancestor->GetRoleMapEntry();
      if (roleMapEntry && roleMapEntry->role == nsIAccessibleRole::ROLE_ALERT) {
        FireDelayedAccessibleEvent(nsIAccessibleEvent::EVENT_ALERT,
                                   ancestor->GetNode(), AccEvent::eRemoveDupes,
                                   fromUserInput);
        break;
      }

      // Don't climb above this document.
      if (ancestor == this)
        break;

      ancestor = ancestor->GetParent();
    }
  }

  // Fire nether value change nor reorder events if action is not from user
  // input and document is loading. We are notified about changes in editor
  // synchronously, so from user input flag is correct for value change events.
  if (!fireAllEvents)
    return;

  // Fire value change event.
  if (container->Role() == nsIAccessibleRole::ROLE_ENTRY) {
    nsRefPtr<AccEvent> valueChangeEvent =
      new AccEvent(nsIAccessibleEvent::EVENT_VALUE_CHANGE, container,
                   fromUserInput, AccEvent::eRemoveDupes);
    FireDelayedAccessibleEvent(valueChangeEvent);
  }

  // Fire reorder event so the MSAA clients know the children have changed. Also
  // the event is used internally by MSAA part.
  nsRefPtr<AccEvent> reorderEvent =
    new AccEvent(nsIAccessibleEvent::EVENT_REORDER, container->GetNode(),
                 fromUserInput, AccEvent::eCoalesceFromSameSubtree);
  if (reorderEvent)
    FireDelayedAccessibleEvent(reorderEvent);
}

void
nsDocAccessible::RecreateAccessible(nsINode* aNode)
{
  // XXX: we shouldn't recreate whole accessible subtree that happens when
  // hide event is handled, instead we should subclass hide and show events
  // to handle them separately and implement their coalescence with normal hide
  // and show events.

  nsAccessible* parent = nsnull;

  // Fire hide event for old accessible.
  nsAccessible* oldAccessible =
    GetAccService()->GetAccessibleInWeakShell(aNode, mWeakShell);
  if (oldAccessible) {
    parent = oldAccessible->GetParent();

    nsRefPtr<AccEvent> hideEvent = new AccHideEvent(oldAccessible, aNode,
                                                    eAutoDetect);
    if (hideEvent)
      FireDelayedAccessibleEvent(hideEvent);

    // Unbind old accessible from tree.
    parent->RemoveChild(oldAccessible);

    if (oldAccessible->IsPrimaryForNode() &&
        mNodeToAccessibleMap.Get(oldAccessible->GetNode()) == oldAccessible)
      mNodeToAccessibleMap.Remove(oldAccessible->GetNode());

  } else {
    parent = GetAccService()->GetContainerAccessible(aNode, mWeakShell);
  }

  // Get new accessible and fire show event.
  parent->InvalidateChildren();

  nsAccessible* newAccessible =
    GetAccService()->GetAccessibleInWeakShell(aNode, mWeakShell);
  if (newAccessible) {
    nsRefPtr<AccEvent> showEvent = new AccShowEvent(newAccessible, aNode,
                                                    eAutoDetect);
    if (showEvent)
      FireDelayedAccessibleEvent(showEvent);
  }

  // Fire reorder event.
  if (oldAccessible || newAccessible) {
    nsRefPtr<AccEvent> reorderEvent =
      new AccEvent(nsIAccessibleEvent::EVENT_REORDER, parent->GetNode(),
                   eAutoDetect, AccEvent::eCoalesceFromSameSubtree);

    if (reorderEvent)
      FireDelayedAccessibleEvent(reorderEvent);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Protected members

void
nsDocAccessible::FireValueChangeForTextFields(nsAccessible *aAccessible)
{
  if (aAccessible->Role() != nsIAccessibleRole::ROLE_ENTRY)
    return;

  // Dependent value change event for text changes in textfields
  nsRefPtr<AccEvent> valueChangeEvent =
    new AccEvent(nsIAccessibleEvent::EVENT_VALUE_CHANGE, aAccessible,
                 eAutoDetect, AccEvent::eRemoveDupes);
  FireDelayedAccessibleEvent(valueChangeEvent);
}

void
nsDocAccessible::FireTextChangeEventForText(nsIContent *aContent,
                                            CharacterDataChangeInfo* aInfo,
                                            PRBool aIsInserted)
{
  if (!IsContentLoaded())
    return;

  PRInt32 contentOffset = aInfo->mChangeStart;
  PRUint32 contentLength = aIsInserted ?
    aInfo->mReplaceLength: // text has been added
    aInfo->mChangeEnd - contentOffset; // text has been removed

  if (contentLength == 0)
    return;

  nsAccessible *accessible = GetAccService()->GetAccessible(aContent);
  if (!accessible)
    return;

  nsRefPtr<nsHyperTextAccessible> textAccessible =
    do_QueryObject(accessible->GetParent());
  if (!textAccessible)
    return;

  // Get offset within hypertext accessible and invalidate cached offsets after
  // this child accessible.
  PRInt32 offset = textAccessible->GetChildOffset(accessible, PR_TRUE);

  // Get added or removed text.
  nsIFrame* frame = aContent->GetPrimaryFrame();
  if (!frame)
    return;

  PRUint32 textOffset = 0;
  nsresult rv = textAccessible->ContentToRenderedOffset(frame, contentOffset,
                                                        &textOffset);
  if (NS_FAILED(rv))
    return;

  nsAutoString text;
  rv = accessible->AppendTextTo(text, textOffset, contentLength);
  if (NS_FAILED(rv))
    return;

  if (text.IsEmpty())
    return;

  // Normally we only fire delayed events created from the node, not an
  // accessible object. See the AccTextChangeEvent constructor for details
  // about this exceptional case.
  nsRefPtr<AccEvent> event =
    new AccTextChangeEvent(textAccessible, offset + textOffset, text,
                          aIsInserted);
  FireDelayedAccessibleEvent(event);

  FireValueChangeForTextFields(textAccessible);
}

// nsDocAccessible public member
nsresult
nsDocAccessible::FireDelayedAccessibleEvent(PRUint32 aEventType, nsINode *aNode,
                                            AccEvent::EEventRule aAllowDupes,
                                            EIsFromUserInput aIsFromUserInput)
{
  nsRefPtr<AccEvent> event =
    new AccEvent(aEventType, aNode, aIsFromUserInput, aAllowDupes);
  NS_ENSURE_TRUE(event, NS_ERROR_OUT_OF_MEMORY);

  return FireDelayedAccessibleEvent(event);
}

// nsDocAccessible public member
nsresult
nsDocAccessible::FireDelayedAccessibleEvent(AccEvent* aEvent)
{
  NS_ENSURE_ARG(aEvent);
  NS_LOG_ACCDOCLOAD_FIREEVENT(aEvent)

  if (mEventQueue)
    mEventQueue->Push(aEvent);

  return NS_OK;
}

// nsDocAccessible public member
void
nsDocAccessible::ProcessPendingEvent(AccEvent* aEvent)
{
  nsAccessible* accessible = aEvent->GetAccessible();
  if (!accessible)
    return;

  PRUint32 eventType = aEvent->GetEventType();

  if (eventType == nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED) {
    nsCOMPtr<nsIAccessibleText> accessibleText = do_QueryObject(accessible);
    PRInt32 caretOffset;
    if (accessibleText &&
        NS_SUCCEEDED(accessibleText->GetCaretOffset(&caretOffset))) {
#ifdef DEBUG_A11Y
      PRUnichar chAtOffset;
      accessibleText->GetCharacterAtOffset(caretOffset, &chAtOffset);
      printf("\nCaret moved to %d with char %c", caretOffset, chAtOffset);
#endif
#ifdef DEBUG_CARET
      // Test caret line # -- fire an EVENT_ALERT on the focused node so we can watch the
      // line-number object attribute on it
      nsAccessible* focusedAcc =
        GetAccService()->GetAccessible(gLastFocusedNode);
      nsEventShell::FireEvent(nsIAccessibleEvent::EVENT_ALERT, focusedAcc);
#endif
      nsRefPtr<AccEvent> caretMoveEvent =
          new AccCaretMoveEvent(accessible, caretOffset);
      if (!caretMoveEvent)
        return;

      nsEventShell::FireEvent(caretMoveEvent);

      PRInt32 selectionCount;
      accessibleText->GetSelectionCount(&selectionCount);
      if (selectionCount) {  // There's a selection so fire selection change as well
        nsEventShell::FireEvent(nsIAccessibleEvent::EVENT_TEXT_SELECTION_CHANGED,
                                accessible);
      }
    }
  }
  else {
    nsEventShell::FireEvent(aEvent);

    // Post event processing
    if (eventType == nsIAccessibleEvent::EVENT_HIDE)
      ShutdownChildrenInSubtree(accessible);
  }
}

PRUint32
nsDocAccessible::UpdateTreeInternal(nsAccessible* aContainer,
                                    nsIContent* aStartNode,
                                    nsIContent* aEndNode,
                                    PRBool aIsInsert,
                                    PRBool aFireAllEvents,
                                    EIsFromUserInput aFromUserInput)
{
  PRUint32 updateFlags = eNoAccessible;
  for (nsIContent* node = aStartNode; node != aEndNode;
       node = node->GetNextSibling()) {

    // Tree update triggers for content insertion even if no content was
    // inserted actually, check if the given content has a frame to discard
    // this case early.
    if (aIsInsert && !node->GetPrimaryFrame())
      continue;

    nsAccessible* accessible = aIsInsert ?
      GetAccService()->GetAccessibleInWeakShell(node, mWeakShell) :
      GetCachedAccessible(node);

    if (!accessible) {
      updateFlags |= UpdateTreeInternal(aContainer, node->GetFirstChild(),
                                        nsnull, aIsInsert, aFireAllEvents,
                                        aFromUserInput);
      continue;
    }

    updateFlags |= eAccessible;

    // Fire show/hide event.
    if (aFireAllEvents) {
      nsRefPtr<AccEvent> event;
      if (aIsInsert)
        event = new AccShowEvent(accessible, node, aFromUserInput);
      else
        event = new AccHideEvent(accessible, node, aFromUserInput);

      if (event)
        FireDelayedAccessibleEvent(event);
    }

    if (aIsInsert) {
      const nsRoleMapEntry* roleMapEntry = accessible->GetRoleMapEntry();
      if (roleMapEntry) {
        if (roleMapEntry->role == nsIAccessibleRole::ROLE_MENUPOPUP) {
          // Fire EVENT_MENUPOPUP_START if ARIA menu appears.
          FireDelayedAccessibleEvent(nsIAccessibleEvent::EVENT_MENUPOPUP_START,
                                     node, AccEvent::eRemoveDupes, aFromUserInput);

        } else if (roleMapEntry->role == nsIAccessibleRole::ROLE_ALERT) {
          // Fire EVENT_ALERT if ARIA alert appears.
          updateFlags = eAlertAccessible;
          FireDelayedAccessibleEvent(nsIAccessibleEvent::EVENT_ALERT, node,
                                     AccEvent::eRemoveDupes, aFromUserInput);
        }
      }

      // If focused node has been shown then it means its frame was recreated
      // while it's focused. Fire focus event on new focused accessible. If
      // the queue contains focus event for this node then it's suppressed by
      // this one.
      if (node == gLastFocusedNode) {
        FireDelayedAccessibleEvent(nsIAccessibleEvent::EVENT_FOCUS,
                                   node, AccEvent::eCoalesceFromSameDocument,
                                   aFromUserInput);
      }
    } else {
      // Update the tree for content removal.
      aContainer->RemoveChild(accessible);
      UncacheChildrenInSubtree(accessible);
    }
  }

  return updateFlags;
}

void
nsDocAccessible::UncacheChildrenInSubtree(nsAccessible* aRoot)
{
  PRUint32 count = aRoot->GetCachedChildCount();
  for (PRUint32 idx = 0; idx < count; idx++)
    UncacheChildrenInSubtree(aRoot->GetCachedChildAt(idx));

  if (aRoot->IsPrimaryForNode() &&
      mNodeToAccessibleMap.Get(aRoot->GetNode()) == aRoot)
    mNodeToAccessibleMap.Remove(aRoot->GetNode());
}

void
nsDocAccessible::ShutdownChildrenInSubtree(nsAccessible* aAccessible)
{
  // Traverse through children and shutdown them before this accessible. When
  // child gets shutdown then it removes itself from children array of its
  //parent. Use jdx index to process the cases if child is not attached to the
  // parent and as result doesn't remove itself from its children.
  PRUint32 count = aAccessible->GetCachedChildCount();
  for (PRUint32 idx = 0, jdx = 0; idx < count; idx++) {
    nsAccessible* child = aAccessible->GetCachedChildAt(jdx);
    if (!child->IsBoundToParent()) {
      NS_ERROR("Parent refers to a child, child doesn't refer to parent!");
      jdx++;
    }

    ShutdownChildrenInSubtree(child);
  }

  UnbindFromDocument(aAccessible);
}


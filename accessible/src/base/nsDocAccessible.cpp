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
nsIAtom *nsDocAccessible::gLastFocusedFrameType = nsnull;


////////////////////////////////////////////////////////////////////////////////
// Constructor/desctructor

nsDocAccessible::
  nsDocAccessible(nsIDocument *aDocument, nsIContent *aRootContent,
                  nsIWeakReference *aShell) :
  nsHyperTextAccessibleWrap(aRootContent, aShell), mWnd(nsnull),
  mDocument(aDocument), mScrollPositionChangedTicks(0), mIsLoaded(PR_FALSE)
{
  // XXX aaronl should we use an algorithm for the initial cache size?
  mAccessibleCache.Init(kDefaultCacheSize);

  // For GTK+ native window, we do nothing here.
  if (!mDocument)
    return;

  // Initialize mWnd
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mWeakShell));
  nsIViewManager* vm = shell->GetViewManager();
  if (vm) {
    nsCOMPtr<nsIWidget> widget;
    vm->GetRootWidget(getter_AddRefs(widget));
    if (widget) {
      mWnd = widget->GetNativeData(NS_NATIVE_WINDOW);
    }
  }

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

  if (aExtraState)
    *aExtraState = 0;

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
nsDocAccessible::GetCachedAccessible(void *aUniqueID)
{
  nsAccessible* accessible = mAccessibleCache.GetWeak(aUniqueID);

  // No accessible in the cache, check if the given ID is unique ID of this
  // document accessible.
  if (!accessible) {
    void* thisUniqueID = nsnull;
    GetUniqueID(&thisUniqueID);
    if (thisUniqueID != aUniqueID)
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

// nsDocAccessible public method
PRBool
nsDocAccessible::CacheAccessible(void *aUniqueID, nsAccessible *aAccessible)
{
  // If there is already an accessible with the given unique ID, shut it down
  // because the DOM node has changed.
  nsAccessible *accessible = mAccessibleCache.GetWeak(aUniqueID);
  NS_ASSERTION(!accessible,
               "Caching new accessible for the DOM node while the old one is alive");

  if (accessible)
    accessible->Shutdown();

  return mAccessibleCache.Put(aUniqueID, aAccessible);
}

// nsDocAccessible public method
void
nsDocAccessible::RemoveAccessNodeFromCache(nsAccessible *aAccessible)
{
  if (!aAccessible)
    return;

  void *uniqueID = nsnull;
  aAccessible->GetUniqueID(&uniqueID);
  mAccessibleCache.Remove(uniqueID);
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
    new AccReorderEvent(mParent, PR_FALSE, PR_TRUE, mDocument);
  if (!reorderEvent)
    return PR_FALSE;

  FireDelayedAccessibleEvent(reorderEvent);
  return PR_TRUE;
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

  mChildDocuments.Clear();

  mWeakShell = nsnull;  // Avoid reentrancy

  ClearCache(mAccessibleCache);

  nsCOMPtr<nsIDocument> kungFuDeathGripDoc = mDocument;
  mDocument = nsnull;

  nsHyperTextAccessibleWrap::Shutdown();
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
    InvalidateCacheSubtree(aContent,
                           nsIAccessibilityService::NODE_SIGNIFICANT_CHANGE);
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
    InvalidateCacheSubtree(aContent,
                           nsIAccessibilityService::NODE_SIGNIFICANT_CHANGE);
    return;
  }
}

void nsDocAccessible::ContentAppended(nsIDocument *aDocument,
                                      nsIContent* aContainer,
                                      nsIContent* aFirstNewContent,
                                      PRInt32 /* unused */)
{
  if (!IsContentLoaded() && mAccessibleCache.Count() <= 1) {
    // See comments in nsDocAccessible::InvalidateCacheSubtree
    InvalidateChildren();
    return;
  }

  // Does this need to be a strong ref?  If so, why?
  for (nsIContent* cur = aFirstNewContent; cur; cur = cur->GetNextSibling()) {
    // InvalidateCacheSubtree will not fire the EVENT_SHOW for the new node
    // unless an accessible can be created for the passed in node, which it
    // can't do unless the node is visible. The right thing happens there so
    // no need for an extra visibility check here.
    InvalidateCacheSubtree(cur, nsIAccessibilityService::NODE_APPEND);
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

void nsDocAccessible::DocumentStatesChanged(nsIDocument* aDocument,
                                            PRInt32 aStateMask)
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
  // InvalidateCacheSubtree will not fire the EVENT_SHOW for the new node
  // unless an accessible can be created for the passed in node, which it
  // can't do unless the node is visible. The right thing happens there so
  // no need for an extra visibility check here.
  InvalidateCacheSubtree(aChild, nsIAccessibilityService::NODE_APPEND);
}

void
nsDocAccessible::ContentRemoved(nsIDocument *aDocument, nsIContent* aContainer,
                                nsIContent* aChild, PRInt32 /* unused */,
                                nsIContent* aPreviousSibling)
{
  // It's no needed to invalidate the subtree of the removed element,
  // because we get notifications directly from content (see
  // nsGenericElement::doRemoveChildAt) *before* the frame for the content is
  // destroyed, or any other side effects occur . That allows us to correctly
  // calculate the TEXT_REMOVED event if there is one and coalesce events from
  // the same subtree.
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

nsAccessible*
nsDocAccessible::GetCachedAccessibleInSubtree(void* aUniqueID)
{
  nsAccessible* child = GetCachedAccessible(aUniqueID);
  if (child)
    return child;

  PRUint32 childDocCount = mChildDocuments.Length();
  for (PRUint32 childDocIdx= 0; childDocIdx < childDocCount; childDocIdx++) {
    nsDocAccessible* childDocument = mChildDocuments.ElementAt(childDocIdx);
    child = childDocument->GetCachedAccessibleInSubtree(aUniqueID);
    if (child)
      return child;
  }

  return nsnull;
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
                 PR_FALSE, eAutoDetect, AccEvent::eRemoveDupes);
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
                           aIsInserted, PR_FALSE);
  FireDelayedAccessibleEvent(event);

  FireValueChangeForTextFields(textAccessible);
}

already_AddRefed<AccEvent>
nsDocAccessible::CreateTextChangeEventForNode(nsAccessible *aContainerAccessible,
                                              nsIContent *aChangeNode,
                                              nsAccessible *aChangeChild,
                                              PRBool aIsInserting,
                                              PRBool aIsAsynch,
                                              EIsFromUserInput aIsFromUserInput)
{
  nsRefPtr<nsHyperTextAccessible> textAccessible =
    do_QueryObject(aContainerAccessible);
  if (!textAccessible) {
    return nsnull;
  }

  nsAutoString text;
  PRInt32 offset = 0;
  if (aChangeChild) {
    // Don't fire event for the first html:br in an editor.
    if (aChangeChild->Role() == nsIAccessibleRole::ROLE_WHITESPACE) {
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

    offset = textAccessible->GetChildOffset(aChangeChild);
    aChangeChild->AppendTextTo(text, 0, PR_UINT32_MAX);

  } else {
    // A span-level object or something else without an accessible is being
    // added, where it has no accessible but it has descendant content which is
    // aggregated as text into the parent hypertext. In this case, changed text
    // is compounded from all accessible contained in changed node.
    nsAccTreeWalker walker(mWeakShell, aChangeNode,
                           GetAllowsAnonChildAccessibles());
    nsRefPtr<nsAccessible> child = walker.GetNextChild();

    // No descendant content that represents any text in the hypertext parent.
    if (!child)
      return nsnull;

    offset = textAccessible->GetChildOffset(child);
    child->AppendTextTo(text, 0, PR_UINT32_MAX);

    nsINode* containerNode = textAccessible->GetNode();
    PRInt32 childCount = textAccessible->GetChildCount();
    PRInt32 childIdx = child->GetIndexInParent();

    for (PRInt32 idx = childIdx + 1; idx < childCount; idx++) {
      nsAccessible* nextChild = textAccessible->GetChildAt(idx);
      // We only want accessibles with DOM nodes as children of this node.
      if (!nsCoreUtils::IsAncestorOf(aChangeNode, nextChild->GetNode(),
                                     containerNode))
        break;

      nextChild->AppendTextTo(text, 0, PR_UINT32_MAX);
    }
  }

  if (text.IsEmpty())
    return nsnull;

  AccEvent* event = new AccTextChangeEvent(aContainerAccessible, offset, text,
                                           aIsInserting, aIsAsynch,
                                           aIsFromUserInput);
  NS_IF_ADDREF(event);

  return event;
}

// nsDocAccessible public member
nsresult
nsDocAccessible::FireDelayedAccessibleEvent(PRUint32 aEventType, nsINode *aNode,
                                            AccEvent::EEventRule aAllowDupes,
                                            PRBool aIsAsynch,
                                            EIsFromUserInput aIsFromUserInput)
{
  nsRefPtr<AccEvent> event =
    new AccEvent(aEventType, aNode, aIsAsynch, aIsFromUserInput, aAllowDupes);
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

void
nsDocAccessible::ProcessPendingEvent(AccEvent* aEvent)
{  
  nsAccessible *accessible = aEvent->GetAccessible();
  nsINode *node = aEvent->GetNode();

  PRUint32 eventType = aEvent->GetEventType();
  EIsFromUserInput isFromUserInput =
    aEvent->IsFromUserInput() ? eFromUserInput : eNoUserInput;

  PRBool isAsync = aEvent->IsAsync();

  if (node == gLastFocusedNode && isAsync &&
      (eventType == nsIAccessibleEvent::EVENT_SHOW ||
       eventType == nsIAccessibleEvent::EVENT_HIDE)) {
    // If frame type didn't change for this event, then we don't actually need to invalidate
    // However, we only keep track of the old frame type for the focus, where it's very
    // important not to destroy and recreate the accessible for minor style changes,
    // such as a:focus { overflow: scroll; }
    nsCOMPtr<nsIContent> focusContent(do_QueryInterface(node));
    if (focusContent) {
      nsIFrame *focusFrame = focusContent->GetPrimaryFrame();
      nsIAtom *newFrameType =
        (focusFrame && focusFrame->GetStyleVisibility()->IsVisible()) ?
        focusFrame->GetType() : nsnull;

      if (newFrameType == gLastFocusedFrameType) {
        // Don't need to invalidate this current accessible, but can
        // just invalidate the children instead
        FireShowHideEvents(node, PR_TRUE, eventType, eNormalEvent,
                           isAsync, isFromUserInput);
        return;
      }
      gLastFocusedFrameType = newFrameType;
    }
  }

  if (eventType == nsIAccessibleEvent::EVENT_SHOW) {

    nsAccessible* containerAccessible = nsnull;
    if (accessible) {
      containerAccessible = accessible->GetParent();
    } else {
      nsCOMPtr<nsIWeakReference> weakShell(nsCoreUtils::GetWeakShellFor(node));
      containerAccessible = GetAccService()->GetContainerAccessible(node,
                                                                    weakShell);
    }

    if (!containerAccessible)
      containerAccessible = this;

    if (isAsync) {
      // For asynch show, delayed invalidatation of parent's children
      containerAccessible->InvalidateChildren();

      // Some show events in the subtree may have been removed to 
      // avoid firing redundant events. But, we still need to make sure any
      // accessibles parenting those shown nodes lose their child references.
      InvalidateChildrenInSubtree(node);
    }

    // Also fire text changes if the node being created could affect the text in an nsIAccessibleText parent.
    // When a node is being made visible or is inserted, the text in an ancestor hyper text will gain characters
    // At this point we now have the frame and accessible for this node if there is one. That is why we
    // wait to fire this here, instead of in InvalidateCacheSubtree(), where we wouldn't be able to calculate
    // the offset, length and text for the text change.
    if (node && node != mDocument) {
      nsCOMPtr<nsIContent> content(do_QueryInterface(node));
      nsRefPtr<AccEvent> textChangeEvent =
        CreateTextChangeEventForNode(containerAccessible, content, accessible,
                                     PR_TRUE, PR_TRUE, isFromUserInput);
      if (textChangeEvent) {
        // XXX Queue them up and merge the text change events
        // XXX We need a way to ignore SplitNode and JoinNode() when they
        // do not affect the text within the hypertext
        nsEventShell::FireEvent(textChangeEvent);
      }
    }

    // Fire show/create events for this node or first accessible descendants of it
    FireShowHideEvents(node, PR_FALSE, eventType, eNormalEvent, isAsync,
                       isFromUserInput); 
    return;
  }

  if (accessible) {
    if (eventType == nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED) {
      nsCOMPtr<nsIAccessibleText> accessibleText = do_QueryObject(accessible);
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
        nsAccessible *focusedAcc =
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
                                  accessible, PR_TRUE);
        }
      } 
    }
    else if (eventType == nsIAccessibleEvent::EVENT_REORDER) {
      // Fire reorder event if it's unconditional (see InvalidateCacheSubtree
      // method) or if changed node (that is the reason of this reorder event)
      // is accessible or has accessible children.
      AccReorderEvent* reorderEvent = downcast_accEvent(aEvent);
      if (reorderEvent->IsUnconditionalEvent() ||
          reorderEvent->HasAccessibleInReasonSubtree()) {
        nsEventShell::FireEvent(aEvent);
      }
    }
    else {
      nsEventShell::FireEvent(aEvent);

      // Post event processing
      if (eventType == nsIAccessibleEvent::EVENT_HIDE && node) {
        // Shutdown nsIAccessNode's or nsIAccessibles for any DOM nodes in
        // this subtree.
        // XXX: Will this bite us with asynch events.
        RefreshNodes(node);
      }
    }
  }
}

void
nsDocAccessible::InvalidateChildrenInSubtree(nsINode *aStartNode)
{
  nsAccessible *accessible = GetCachedAccessible(aStartNode);
  if (accessible)
    accessible->InvalidateChildren();

  // Invalidate accessible children in the DOM subtree 
  PRInt32 index, numChildren = aStartNode->GetChildCount();
  for (index = 0; index < numChildren; index ++) {
    nsINode *childNode = aStartNode->GetChildAt(index);
    InvalidateChildrenInSubtree(childNode);
  }
}

void
nsDocAccessible::RefreshNodes(nsINode *aStartNode)
{
  if (mAccessibleCache.Count() <= 1) {
    return; // All we have is a doc accessible. There is nothing to invalidate, quit early
  }

  // Shut down accessible subtree, which may have been created for anonymous
  // content subtree.
  nsAccessible *accessible = GetCachedAccessible(aStartNode);
  if (accessible) {
    // Fire menupopup end if a menu goes away
    if (accessible->Role() == nsIAccessibleRole::ROLE_MENUPOPUP) {
      nsCOMPtr<nsIDOMXULPopupElement> popup(do_QueryInterface(aStartNode));
      if (!popup) {
        // Popup elements already fire these via DOMMenuInactive
        // handling in nsRootAccessible::HandleEvent
        nsEventShell::FireEvent(nsIAccessibleEvent::EVENT_MENUPOPUP_END,
                                accessible);
      }
    }

    // We only need to shutdown the accessibles here if one of them has been
    // created.
    if (accessible->GetCachedChildCount() > 0) {
      nsCOMPtr<nsIArray> children;
      // use GetChildren() to fetch all children at once, because after shutdown
      // the child references are cleared.
      accessible->GetChildren(getter_AddRefs(children));
      PRUint32 childCount =0;
      if (children)
        children->GetLength(&childCount);
      nsINode *possibleAnonNode = nsnull;
      for (PRUint32 index = 0; index < childCount; index++) {
        nsRefPtr<nsAccessNode> childAccessNode;
        children->QueryElementAt(index, NS_GET_IID(nsAccessNode),
                                 getter_AddRefs(childAccessNode));
        possibleAnonNode = childAccessNode->GetNode();
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
  PRUint32 childCount = aStartNode->GetChildCount();
  for (PRUint32 childIdx = 0; childIdx < childCount; childIdx++) {
    nsIContent *childContent = aStartNode->GetChildAt(childIdx);
    RefreshNodes(childContent);
  }

  if (!accessible)
    return;

  if (accessible == this) {
    // Don't shutdown our doc object -- this may just be from the finished loading.
    // We will completely shut it down when the pagehide event is received
    // However, we must invalidate the doc accessible's children in order to be sure
    // all pointers to them are correct
    InvalidateChildren();
    return;
  }

  // Shut down the actual accessible or access node
  void *uniqueID;
  accessible->GetUniqueID(&uniqueID);
  accessible->Shutdown();

  // Remove from hash table as well
  mAccessibleCache.Remove(uniqueID);
}

// nsDocAccessible public member
void
nsDocAccessible::InvalidateCacheSubtree(nsIContent *aChild,
                                        PRUint32 aChangeType)
{
  PRBool isHiding =
    aChangeType == nsIAccessibilityService::FRAME_HIDE ||
    aChangeType == nsIAccessibilityService::NODE_REMOVE;

  PRBool isShowing =
    aChangeType == nsIAccessibilityService::FRAME_SHOW ||
    aChangeType == nsIAccessibilityService::NODE_APPEND;

#ifdef DEBUG
  PRBool isChanging =
    aChangeType == nsIAccessibilityService::NODE_SIGNIFICANT_CHANGE ||
    aChangeType == nsIAccessibilityService::FRAME_SIGNIFICANT_CHANGE;
#endif

  NS_ASSERTION(isChanging || isHiding || isShowing,
               "Incorrect aChangeEventType passed in");

  PRBool isAsynch =
    aChangeType == nsIAccessibilityService::FRAME_HIDE ||
    aChangeType == nsIAccessibilityService::FRAME_SHOW ||
    aChangeType == nsIAccessibilityService::FRAME_SIGNIFICANT_CHANGE;

  // Invalidate cache subtree
  // We have to check for accessibles for each dom node by traversing DOM tree
  // instead of just the accessible tree, although that would be faster
  // Otherwise we might miss the nsAccessNode's that are not nsAccessible's.

  NS_ENSURE_TRUE(mDocument,);

  nsINode *childNode = aChild;
  if (!childNode)
    childNode = mDocument;

  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  NS_ENSURE_TRUE(presShell,);
  
  if (!IsContentLoaded()) {
    // Still loading document
    if (mAccessibleCache.Count() <= 1) {
      // Still loading and no accessibles has yet been created other than this
      // doc accessible. In this case we optimize
      // by not firing SHOW/HIDE/REORDER events for every document mutation
      // caused by page load, since AT is not going to want to grab the
      // document and listen to these changes until after the page is first loaded
      // Leave early, and ensure mAccChildCount stays uninitialized instead of 0,
      // which it is if anyone asks for its children right now.
      InvalidateChildren();
      return;
    }

    nsIEventStateManager *esm = presShell->GetPresContext()->EventStateManager();
    NS_ENSURE_TRUE(esm,);

    if (!esm->IsHandlingUserInputExternal()) {
      // Changes during page load, but not caused by user input
      // Just invalidate accessible hierarchy and return,
      // otherwise the page load time slows down way too much
      nsAccessible *containerAccessible =
        GetAccService()->GetCachedContainerAccessible(childNode);
      if (!containerAccessible) {
        containerAccessible = this;
      }

      containerAccessible->InvalidateChildren();
      return;
    }     
    // else: user input, so we must fall through and for full handling,
    // e.g. fire the mutation events. Note: user input could cause DOM_CREATE
    // during page load if user typed into an input field or contentEditable area
  }

  // Update last change state information
  nsAccessible *childAccessible = GetCachedAccessible(childNode);

#ifdef DEBUG_A11Y
  nsAutoString localName;
  if (aChild)
    aChild->NodeInfo()->GetName(localName);
  const char *hasAccessible = childAccessible ? " (acc)" : "";
  if (aChangeType == nsIAccessibilityService::FRAME_HIDE)
    printf("[Hide %s %s]\n", NS_ConvertUTF16toUTF8(localName).get(), hasAccessible);
  else if (aChangeType == nsIAccessibilityService::FRAME_SHOW)
    printf("[Show %s %s]\n", NS_ConvertUTF16toUTF8(localName).get(), hasAccessible);
  else if (aChangeType == nsIAccessibilityService::FRAME_SIGNIFICANT_CHANGE)
    printf("[Layout change %s %s]\n", NS_ConvertUTF16toUTF8(localName).get(), hasAccessible);
  else if (aChangeType == nsIAccessibilityService::NODE_APPEND)
    printf("[Create %s %s]\n", NS_ConvertUTF16toUTF8(localName).get(), hasAccessible);
  else if (aChangeType == nsIAccessibilityService::NODE_REMOVE)
    printf("[Destroy  %s %s]\n", NS_ConvertUTF16toUTF8(localName).get(), hasAccessible);
  else if (aChangeType == nsIAccessibilityService::NODE_SIGNIFICANT_CHANGE)
    printf("[Type change %s %s]\n", NS_ConvertUTF16toUTF8(localName).get(), hasAccessible);
#endif

  nsAccessible *containerAccessible =
    GetAccService()->GetCachedContainerAccessible(childNode);
  if (!containerAccessible) {
    containerAccessible = this;
  }

  if (!isShowing) {
    // Fire EVENT_HIDE.
    if (isHiding) {
      if (aChild) {
        nsIFrame *frame = aChild->GetPrimaryFrame();
        if (frame) {
          nsIFrame *frameParent = frame->GetParent();
          if (!frameParent || !frameParent->GetStyleVisibility()->IsVisible()) {
            // Ancestor already hidden or being hidden at the same time:
            // don't process redundant hide event
            // This often happens when visibility is cleared for node,
            // which hides an entire subtree -- we get notified for each
            // node in the subtree and need to collate the hide events ourselves.
            return;
          }
        }
      }
    }

    // Fire an event if the accessible existed for node being hidden, otherwise
    // for the first line accessible descendants. Fire before the accessible(s)
    // away.
    nsresult rv = FireShowHideEvents(childNode, PR_FALSE,
                                     nsIAccessibleEvent::EVENT_HIDE,
                                     eDelayedEvent, isAsynch);
    if (NS_FAILED(rv))
      return;
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
      containerAccessible->InvalidateChildren();
    }

    // Fire EVENT_SHOW, EVENT_MENUPOPUP_START for newly visible content.

    // Fire after a short timer, because we want to make sure the view has been
    // updated to make this accessible content visible. If we don't wait,
    // the assistive technology may receive the event and then retrieve
    // nsIAccessibleStates::STATE_INVISIBLE for the event's accessible object.

    FireDelayedAccessibleEvent(nsIAccessibleEvent::EVENT_SHOW, childNode,
                               AccEvent::eCoalesceFromSameSubtree,
                               isAsynch);

    // Check to see change occurred in an ARIA menu, and fire
    // an EVENT_MENUPOPUP_START if it did.
    nsRoleMapEntry *roleMapEntry = nsAccUtils::GetRoleMapEntry(childNode);
    if (roleMapEntry && roleMapEntry->role == nsIAccessibleRole::ROLE_MENUPOPUP) {
      FireDelayedAccessibleEvent(nsIAccessibleEvent::EVENT_MENUPOPUP_START,
                                 childNode, AccEvent::eRemoveDupes,
                                 isAsynch);
    }

    // Check to see if change occurred inside an alert, and fire an EVENT_ALERT if it did
    nsIContent *ancestor = aChild;
    while (PR_TRUE) {
      if (roleMapEntry && roleMapEntry->role == nsIAccessibleRole::ROLE_ALERT) {
        FireDelayedAccessibleEvent(nsIAccessibleEvent::EVENT_ALERT, ancestor,
                                   AccEvent::eRemoveDupes, isAsynch);
        break;
      }
      ancestor = ancestor->GetParent();
      if (!ancestor)
        break;

      roleMapEntry = nsAccUtils::GetRoleMapEntry(ancestor);
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

  nsRefPtr<AccEvent> reorderEvent =
    new AccReorderEvent(containerAccessible, isAsynch, isUnconditionalEvent,
                        aChild ? aChild : nsnull);
  NS_ENSURE_TRUE(reorderEvent,);

  FireDelayedAccessibleEvent(reorderEvent);
}

nsresult
nsDocAccessible::FireShowHideEvents(nsINode *aNode,
                                    PRBool aAvoidOnThisNode,
                                    PRUint32 aEventType,
                                    EEventFiringType aDelayedOrNormal,
                                    PRBool aIsAsyncChange,
                                    EIsFromUserInput aIsFromUserInput)
{
  NS_ENSURE_ARG(aNode);

  nsAccessible *accessible = nsnull;
  if (!aAvoidOnThisNode) {
    if (aEventType == nsIAccessibleEvent::EVENT_HIDE) {
      // Don't allow creation for accessibles when nodes going away
      accessible = GetCachedAccessible(aNode);
    } else {
      // Allow creation of new accessibles for show events
      accessible = GetAccService()->GetAccessible(aNode);
    }
  }

  if (accessible) {
    // Found an accessible, so fire the show/hide on it and don't look further
    // into this subtree.
    nsRefPtr<AccEvent> event;
    if (aDelayedOrNormal == eDelayedEvent &&
        aEventType == nsIAccessibleEvent::EVENT_HIDE) {
      // Use AccHideEvent for delayed hide events to coalesce text change events
      // caused by these hide events.
      event = new AccHideEvent(accessible, accessible->GetNode(),
                               aIsAsyncChange, aIsFromUserInput);

    } else {
      event = new AccEvent(aEventType, accessible, aIsAsyncChange,
                           aIsFromUserInput,
                           AccEvent::eCoalesceFromSameSubtree);
    }
    NS_ENSURE_TRUE(event, NS_ERROR_OUT_OF_MEMORY);

    if (aDelayedOrNormal == eDelayedEvent)
      return FireDelayedAccessibleEvent(event);

    nsEventShell::FireEvent(event);
    return NS_OK;
  }

  // Could not find accessible to show hide yet, so fire on any
  // accessible descendants in this subtree
  PRUint32 count = aNode->GetChildCount();
  for (PRUint32 index = 0; index < count; index++) {
    nsINode *childNode = aNode->GetChildAt(index);
    nsresult rv = FireShowHideEvents(childNode, PR_FALSE, aEventType,
                                     aDelayedOrNormal, aIsAsyncChange,
                                     aIsFromUserInput);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

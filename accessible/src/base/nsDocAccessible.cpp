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

#include "AccIterator.h"
#include "States.h"
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
#include "nsIDOMXULDocument.h"
#include "nsIDOMMutationEvent.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMXULPopupElement.h"
#include "nsIEditingSession.h"
#include "nsEventStateManager.h"
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
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// Static member initialization

static nsIAtom** kRelationAttrs[] =
{
  &nsAccessibilityAtoms::aria_labelledby,
  &nsAccessibilityAtoms::aria_describedby,
  &nsAccessibilityAtoms::aria_owns,
  &nsAccessibilityAtoms::aria_controls,
  &nsAccessibilityAtoms::aria_flowto,
  &nsAccessibilityAtoms::_for,
  &nsAccessibilityAtoms::control
};

static const PRUint32 kRelationAttrsLen = NS_ARRAY_LENGTH(kRelationAttrs);

////////////////////////////////////////////////////////////////////////////////
// Constructor/desctructor

nsDocAccessible::
  nsDocAccessible(nsIDocument *aDocument, nsIContent *aRootContent,
                  nsIWeakReference *aShell) :
  nsHyperTextAccessibleWrap(aRootContent, aShell),
  mDocument(aDocument), mScrollPositionChangedTicks(0),
  mLoadState(eTreeConstructionPending), mLoadEventType(0)
{
  mFlags |= eDocAccessible;

  mDependentIDsHash.Init();
  // XXX aaronl should we use an algorithm for the initial cache size?
  mAccessibleCache.Init(kDefaultCacheSize);
  mNodeToAccessibleMap.Init(kDefaultCacheSize);

  // If this is a XUL Document, it should not implement nsHyperText
  if (mDocument && mDocument->IsXUL())
    mFlags &= ~eHyperTextAccessible;

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
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NATIVE_MEMBER(mNotificationController,
                                                  NotificationController)

  PRUint32 i, length = tmp->mChildDocuments.Length();
  for (i = 0; i < length; ++i) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mChildDocuments[i],
                                                         nsIAccessible)
  }

  CycleCollectorTraverseCache(tmp->mAccessibleCache, &cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDocAccessible, nsAccessible)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mNotificationController)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSTARRAY(mChildDocuments)
  tmp->mDependentIDsHash.Clear();
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

    status = IsHyperText() ? 
      nsHyperTextAccessible::QueryInterface(aIID,
                                            (void**)&foundInterface) :
      nsAccessible::QueryInterface(aIID, (void**)&foundInterface);
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

void
nsDocAccessible::Description(nsString& aDescription)
{
  if (mParent)
    mParent->Description(aDescription);

  if (aDescription.IsEmpty())
    nsTextEquivUtils::
      GetTextEquivFromIDRefs(this, nsAccessibilityAtoms::aria_describedby,
                             aDescription);
}

// nsAccessible public method
PRUint64
nsDocAccessible::NativeState()
{
  // The root content of the document might be removed so that mContent is
  // out of date.
  PRUint64 state = (mContent->GetCurrentDoc() == mDocument) ?
    0 : states::STALE;

#ifdef MOZ_XUL
  nsCOMPtr<nsIXULDocument> xulDoc(do_QueryInterface(mDocument));
  if (!xulDoc)
#endif
  {
    // XXX Need to invent better check to see if doc is focusable,
    // which it should be if it is scrollable. A XUL document could be focusable.
    // See bug 376803.
    state |= states::FOCUSABLE;
    if (gLastFocusedNode == mDocument)
      state |= states::FOCUSED;
  }

  // Expose stale state until the document is ready (DOM is loaded and tree is
  // constructed).
  if (!HasLoadState(eReady))
    state |= states::STALE;

  // Expose state busy until the document and all its subdocuments is completely
  // loaded.
  if (!HasLoadState(eCompletelyLoaded))
    state |= states::BUSY;

  nsIFrame* frame = GetFrame();
  if (!frame || !nsCoreUtils::CheckVisibilityInParentChain(frame)) {
    state |= states::INVISIBLE | states::OFFSCREEN;
  }

  nsCOMPtr<nsIEditor> editor;
  GetAssociatedEditor(getter_AddRefs(editor));
  state |= editor ? states::EDITABLE : states::READONLY;

  return state;
}

// nsAccessible public method
void
nsDocAccessible::ApplyARIAState(PRUint64* aState)
{
  // Combine with states from outer doc
  // 
  nsAccessible::ApplyARIAState(aState);

  // Allow iframe/frame etc. to have final state override via ARIA
  if (mParent)
    mParent->ApplyARIAState(aState);

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

nsAccessible*
nsDocAccessible::FocusedChild()
{
  // XXXndeakin P3 accessibility shouldn't be caching the focus

  // Return an accessible for the current global focus, which does not have to
  // be contained within the current document.
  return gLastFocusedNode ? GetAccService()->GetAccessible(gLastFocusedNode) :
    nsnull;
}

NS_IMETHODIMP nsDocAccessible::TakeFocus()
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  PRUint64 state = NativeState();
  if (0 == (state & states::FOCUSABLE)) {
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

NS_IMETHODIMP
nsDocAccessible::GetTitle(nsAString& aTitle)
{
  nsCOMPtr<nsIDOMDocument> domDocument = do_QueryInterface(mDocument);
  if (!domDocument) {
    return NS_ERROR_FAILURE;
  }
  return domDocument->GetTitle(aTitle);
}

NS_IMETHODIMP
nsDocAccessible::GetMimeType(nsAString& aMimeType)
{
  nsCOMPtr<nsIDOMDocument> domDocument = do_QueryInterface(mDocument);
  if (!domDocument) {
    return NS_ERROR_FAILURE;
  }
  return domDocument->GetContentType(aMimeType);
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
nsAccessible*
nsDocAccessible::GetAccessible(nsINode* aNode) const
{
  nsAccessible* accessible = mNodeToAccessibleMap.Get(aNode);

  // No accessible in the cache, check if the given ID is unique ID of this
  // document accessible.
  if (!accessible) {
    if (GetNode() != aNode)
      return nsnull;

    accessible = const_cast<nsDocAccessible*>(this);
  }

#ifdef DEBUG
  // All cached accessible nodes should be in the parent
  // It will assert if not all the children were created
  // when they were first cached, and no invalidation
  // ever corrected parent accessible's child cache.
  nsAccessible* parent = accessible->Parent();
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

  // Initialize notification controller.
  nsCOMPtr<nsIPresShell> shell(GetPresShell());
  mNotificationController = new NotificationController(this, shell);
  if (!mNotificationController)
    return PR_FALSE;

  // Mark the document accessible as loaded if its DOM document was loaded at
  // this point (this can happen because a11y is started late or DOM document
  // having no container was loaded.
  if (mDocument->GetReadyStateEnum() == nsIDocument::READYSTATE_COMPLETE)
    mLoadState |= eDOMLoaded;

  AddEventListeners();
  return PR_TRUE;
}

void
nsDocAccessible::Shutdown()
{
  if (!mWeakShell) // already shutdown
    return;

  NS_LOG_ACCDOCDESTROY_FOR("document shutdown", mDocument, this)

  if (mNotificationController) {
    mNotificationController->Shutdown();
    mNotificationController = nsnull;
  }

  RemoveEventListeners();

  // Mark the document as shutdown before AT is notified about the document
  // removal from its container (valid for root documents on ATK).
  nsCOMPtr<nsIDocument> kungFuDeathGripDoc = mDocument;
  mDocument = nsnull;

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

  mDependentIDsHash.Clear();
  mNodeToAccessibleMap.Clear();
  ClearCache(mAccessibleCache);

  nsHyperTextAccessibleWrap::Shutdown();

  GetAccService()->NotifyOfDocumentShutdown(kungFuDeathGripDoc);
}

nsIFrame*
nsDocAccessible::GetFrame() const
{
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mWeakShell));

  nsIFrame* root = nsnull;
  if (shell)
    root = shell->GetRootFrame();

  return root;
}

bool
nsDocAccessible::IsDefunct() const
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
    nsRootAccessible* rootAccessible = RootAccessible();
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

  nsRootAccessible* rootAccessible = RootAccessible();
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
      new AccStateChangeEvent(this, states::EDITABLE, PR_TRUE);
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
  nsAccessible* accessible = GetAccessible(aElement);
  if (!accessible) {
    if (aElement != mContent)
      return;

    accessible = this;
  }

  // Update dependent IDs cache. Take care of elements that are accessible
  // because dependent IDs cache doesn't contain IDs from non accessible
  // elements.
  if (aModType != nsIDOMMutationEvent::ADDITION)
    RemoveDependentIDsFor(accessible, aAttribute);

  // Store the ARIA attribute old value so that it can be used after
  // attribute change. Note, we assume there's no nested ARIA attribute
  // changes. If this happens then we should end up with keeping a stack of
  // old values.

  // XXX TODO: bugs 472142, 472143.
  // Here we will want to cache whatever attribute values we are interested
  // in, such as the existence of aria-pressed for button (so we know if we
  // need to newly expose it as a toggle button) etc.
  if (aAttribute == nsAccessibilityAtoms::aria_checked ||
      aAttribute == nsAccessibilityAtoms::aria_pressed) {
    mARIAAttrOldValue = (aModType != nsIDOMMutationEvent::ADDITION) ?
      nsAccUtils::GetARIAToken(aElement, aAttribute) : nsnull;
  }
}

void
nsDocAccessible::AttributeChanged(nsIDocument *aDocument,
                                  dom::Element* aElement,
                                  PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                                  PRInt32 aModType)
{
  NS_ASSERTION(!IsDefunct(),
               "Attribute changed called on defunct document accessible!");

  // Proceed even if the element is not accessible because element may become
  // accessible if it gets certain attribute.
  if (UpdateAccessibleOnAttrChange(aElement, aAttribute))
    return;

  // Ignore attribute change if the element doesn't have an accessible (at all
  // or still) iff the element is not a root content of this document accessible
  // (which is treated as attribute change on this document accessible).
  // Note: we don't bail if all the content hasn't finished loading because
  // these attributes are changing for a loaded part of the content.
  nsAccessible* accessible = GetAccessible(aElement);
  if (!accessible) {
    if (mContent != aElement)
      return;

    accessible = this;
  }

  // Fire accessible events iff there's an accessible, otherwise we consider
  // the accessible state wasn't changed, i.e. its state is initial state.
  AttributeChangedImpl(aElement, aNameSpaceID, aAttribute);

  // Update dependent IDs cache. Take care of accessible elements because no
  // accessible element means either the element is not accessible at all or
  // its accessible will be created later. It doesn't make sense to keep
  // dependent IDs for non accessible elements. For the second case we'll update
  // dependent IDs cache when its accessible is created.
  if (aModType == nsIDOMMutationEvent::MODIFICATION ||
      aModType == nsIDOMMutationEvent::ADDITION) {
    AddDependentIDsFor(accessible, aAttribute);
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
      new AccStateChangeEvent(aContent, states::ENABLED);

    FireDelayedAccessibleEvent(enabledChangeEvent);

    nsRefPtr<AccEvent> sensitiveChangeEvent =
      new AccStateChangeEvent(aContent, states::SENSITIVE);

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

  if (aAttribute == nsAccessibilityAtoms::alt ||
      aAttribute == nsAccessibilityAtoms::title ||
      aAttribute == nsAccessibilityAtoms::aria_label ||
      aAttribute == nsAccessibilityAtoms::aria_labelledby) {
    FireDelayedAccessibleEvent(nsIAccessibleEvent::EVENT_NAME_CHANGE,
                               aContent);
    return;
  }

  if (aAttribute == nsAccessibilityAtoms::aria_busy) {
    PRBool isOn = aContent->AttrValueIs(aNameSpaceID, aAttribute,
                                        nsAccessibilityAtoms::_true, eCaseMatters);
    nsRefPtr<AccEvent> event = new AccStateChangeEvent(aContent, states::BUSY, isOn);
    FireDelayedAccessibleEvent(event);
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
      new AccStateChangeEvent(aContent, states::EDITABLE);
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
      new AccStateChangeEvent(aContent, states::REQUIRED);
    FireDelayedAccessibleEvent(event);
    return;
  }

  if (aAttribute == nsAccessibilityAtoms::aria_invalid) {
    nsRefPtr<AccEvent> event =
      new AccStateChangeEvent(aContent, states::INVALID);
    FireDelayedAccessibleEvent(event);
    return;
  }

  if (aAttribute == nsAccessibilityAtoms::aria_activedescendant) {
    // The activedescendant universal property redirects accessible focus events
    // to the element with the id that activedescendant points to
    nsCOMPtr<nsINode> focusedNode = GetCurrentFocus();
    if (nsCoreUtils::GetRoleContent(focusedNode) == aContent) {
      nsAccessible* focusedAcc = GetAccService()->GetAccessible(focusedNode);
      nsRootAccessible* rootAcc = RootAccessible();
      if (rootAcc && focusedAcc) {
        rootAcc->FireAccessibleFocusEvent(focusedAcc, nsnull, PR_TRUE);
      }
    }
    return;
  }

  // For aria drag and drop changes we fire a generic attribute change event;
  // at least until native API comes up with a more meaningful event.
  if (aAttribute == nsAccessibilityAtoms::aria_grabbed ||
      aAttribute == nsAccessibilityAtoms::aria_dropeffect ||
      aAttribute == nsAccessibilityAtoms::aria_hidden ||
      aAttribute == nsAccessibilityAtoms::aria_sort) {
    FireDelayedAccessibleEvent(nsIAccessibleEvent::EVENT_OBJECT_ATTRIBUTE_CHANGED,
                               aContent);
  }

  // We treat aria-expanded as a global ARIA state for historical reasons
  if (aAttribute == nsAccessibilityAtoms::aria_expanded) {
    nsRefPtr<AccEvent> event =
      new AccStateChangeEvent(aContent, states::EXPANDED);
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
                            states::CHECKED : states::PRESSED;
    nsRefPtr<AccEvent> event = new AccStateChangeEvent(aContent, kState);
    FireDelayedAccessibleEvent(event);

    nsAccessible* accessible = event->GetAccessible();
    if (accessible) {
      bool wasMixed = (mARIAAttrOldValue == nsAccessibilityAtoms::mixed);
      bool isMixed = aContent->AttrValueIs(kNameSpaceID_None, aAttribute,
                                           nsAccessibilityAtoms::mixed, eCaseMatters);
      if (isMixed != wasMixed) {
        nsRefPtr<AccEvent> event =
          new AccStateChangeEvent(aContent, states::MIXED, isMixed);
        FireDelayedAccessibleEvent(event);
      }
    }
    return;
  }

  if (aAttribute == nsAccessibilityAtoms::aria_readonly) {
    nsRefPtr<AccEvent> event =
      new AccStateChangeEvent(aContent, states::READONLY);
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
}

void nsDocAccessible::ContentAppended(nsIDocument *aDocument,
                                      nsIContent* aContainer,
                                      nsIContent* aFirstNewContent,
                                      PRInt32 /* unused */)
{
}

void nsDocAccessible::ContentStateChanged(nsIDocument* aDocument,
                                          nsIContent* aContent,
                                          nsEventStates aStateMask)
{
  if (aStateMask.HasState(NS_EVENT_STATE_CHECKED)) {
    nsHTMLSelectOptionAccessible::SelectionChangedIfOption(aContent);
  }

  if (aStateMask.HasState(NS_EVENT_STATE_INVALID)) {
    nsRefPtr<AccEvent> event =
      new AccStateChangeEvent(aContent, states::INVALID, PR_TRUE);
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
}

void nsDocAccessible::CharacterDataChanged(nsIDocument *aDocument,
                                           nsIContent* aContent,
                                           CharacterDataChangeInfo* aInfo)
{
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
nsDocAccessible::GetAccessibleByUniqueIDInSubtree(void* aUniqueID)
{
  nsAccessible* child = GetAccessibleByUniqueID(aUniqueID);
  if (child)
    return child;

  PRUint32 childDocCount = mChildDocuments.Length();
  for (PRUint32 childDocIdx= 0; childDocIdx < childDocCount; childDocIdx++) {
    nsDocAccessible* childDocument = mChildDocuments.ElementAt(childDocIdx);
    child = childDocument->GetAccessibleByUniqueIDInSubtree(aUniqueID);
    if (child)
      return child;
  }

  return nsnull;
}

nsAccessible*
nsDocAccessible::GetAccessibleOrContainer(nsINode* aNode)
{
  if (!aNode || !aNode->IsInDoc())
    return nsnull;

  nsINode* currNode = aNode;
  nsAccessible* accessible = nsnull;
  while (!(accessible = GetAccessible(currNode)) &&
         (currNode = currNode->GetNodeParent()));

  return accessible;
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
  if (aAccessible->IsElement())
    AddDependentIDsFor(aAccessible);

  return true;
}

void
nsDocAccessible::UnbindFromDocument(nsAccessible* aAccessible)
{
  NS_ASSERTION(mAccessibleCache.GetWeak(aAccessible->UniqueID()),
               "Unbinding the unbound accessible!");

  // Remove an accessible from node-to-accessible map if it exists there.
  if (aAccessible->IsPrimaryForNode() &&
      mNodeToAccessibleMap.Get(aAccessible->GetNode()) == aAccessible)
    mNodeToAccessibleMap.Remove(aAccessible->GetNode());

  void* uniqueID = aAccessible->UniqueID();

  NS_ASSERTION(!aAccessible->IsDefunct(), "Shutdown the shutdown accessible!");
  aAccessible->Shutdown();

  mAccessibleCache.Remove(uniqueID);
}

void
nsDocAccessible::ContentInserted(nsIContent* aContainerNode,
                                 nsIContent* aStartChildNode,
                                 nsIContent* aEndChildNode)
{
  // Ignore content insertions until we constructed accessible tree. Otherwise
  // schedule tree update on content insertion after layout.
  if (mNotificationController && HasLoadState(eTreeConstructed)) {
    // Update the whole tree of this document accessible when the container is
    // null (document element is inserted or removed).
    nsAccessible* container = aContainerNode ?
      GetAccessibleOrContainer(aContainerNode) : this;

    mNotificationController->ScheduleContentInsertion(container,
                                                      aStartChildNode,
                                                      aEndChildNode);
  }
}

void
nsDocAccessible::ContentRemoved(nsIContent* aContainerNode,
                                nsIContent* aChildNode)
{
  // Update the whole tree of this document accessible when the container is
  // null (document element is removed).
  nsAccessible* container = aContainerNode ?
    GetAccessibleOrContainer(aContainerNode) : this;

  UpdateTree(container, aChildNode, false);
}

void
nsDocAccessible::RecreateAccessible(nsIContent* aContent)
{
  // XXX: we shouldn't recreate whole accessible subtree, instead we should
  // subclass hide and show events to handle them separately and implement their
  // coalescence with normal hide and show events. Note, in this case they
  // should be coalesced with normal show/hide events.

  // Check if the node is in accessible document.
  nsAccessible* container = GetContainerAccessible(aContent);
  if (container) {
    // Remove and reinsert.
    UpdateTree(container, aContent, false);
    container->UpdateChildren();
    UpdateTree(container, aContent, true);
  }
}

void
nsDocAccessible::ProcessInvalidationList()
{
  // Invalidate children of container accessible for each element in
  // invalidation list. Allow invalidation list insertions while container
  // children are recached.
  for (PRUint32 idx = 0; idx < mInvalidationList.Length(); idx++) {
    nsIContent* content = mInvalidationList[idx];
    nsAccessible* accessible = GetAccessible(content);
    if (!accessible) {
      nsAccessible* container = GetContainerAccessible(content);
      if (container) {
        container->UpdateChildren();
        accessible = GetAccessible(content);
      }
    }

    // Make sure the subtree is created.
    if (accessible)
      CacheChildrenInSubtree(accessible);
  }

  mInvalidationList.Clear();
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessible protected

void
nsDocAccessible::CacheChildren()
{
  // Search for accessible children starting from the document element since
  // some web pages tend to insert elements under it rather than document body.
  nsAccTreeWalker walker(mWeakShell, mDocument->GetRootElement(),
                         GetAllowsAnonChildAccessibles());

  nsAccessible* child = nsnull;
  while ((child = walker.NextChild()) && AppendChild(child));
}

////////////////////////////////////////////////////////////////////////////////
// Protected members

void
nsDocAccessible::NotifyOfLoading(bool aIsReloading)
{
  // Mark the document accessible as loading, if it stays alive then we'll mark
  // it as loaded when we receive proper notification.
  mLoadState &= ~eDOMLoaded;

  if (!IsLoadEventTarget())
    return;

  if (aIsReloading) {
    // Fire reload and state busy events on existing document accessible while
    // event from user input flag can be calculated properly and accessible
    // is alive. When new document gets loaded then this one is destroyed.
    nsRefPtr<AccEvent> reloadEvent =
      new AccEvent(nsIAccessibleEvent::EVENT_DOCUMENT_RELOAD, this);
    nsEventShell::FireEvent(reloadEvent);
  }

  // Fire state busy change event. Use delayed event since we don't care
  // actually if event isn't delivered when the document goes away like a shot.
  nsRefPtr<AccEvent> stateEvent =
    new AccStateChangeEvent(mDocument, states::BUSY, PR_TRUE);
  FireDelayedAccessibleEvent(stateEvent);
}

void
nsDocAccessible::DoInitialUpdate()
{
  mLoadState |= eTreeConstructed;

  // The content element may be changed before the initial update and then we
  // miss the notification (since content tree change notifications are ignored
  // prior to initial update). Make sure the content element is valid.
  nsIContent* contentElm = nsCoreUtils::GetRoleContent(mDocument);
  if (contentElm && mContent != contentElm)
    mContent = contentElm;

  // Build initial tree.
  CacheChildrenInSubtree(this);

  // Fire reorder event after the document tree is constructed. Note, since
  // this reorder event is processed by parent document then events targeted to
  // this document may be fired prior to this reorder event. If this is
  // a problem then consider to keep event processing per tab document.
  if (!IsRoot()) {
    nsRefPtr<AccEvent> reorderEvent =
      new AccEvent(nsIAccessibleEvent::EVENT_REORDER, Parent(), eAutoDetect,
                   AccEvent::eCoalesceFromSameSubtree);
    ParentDocument()->FireDelayedAccessibleEvent(reorderEvent);
  }
}

void
nsDocAccessible::ProcessLoad()
{
  mLoadState |= eCompletelyLoaded;

  // Do not fire document complete/stop events for root chrome document
  // accessibles and for frame/iframe documents because
  // a) screen readers start working on focus event in the case of root chrome
  // documents
  // b) document load event on sub documents causes screen readers to act is if
  // entire page is reloaded.
  if (!IsLoadEventTarget())
    return;

  // Fire complete/load stopped if the load event type is given.
  if (mLoadEventType) {
    nsRefPtr<AccEvent> loadEvent = new AccEvent(mLoadEventType, this);
    nsEventShell::FireEvent(loadEvent);

    mLoadEventType = 0;
  }

  // Fire busy state change event.
  nsRefPtr<AccEvent> stateEvent =
    new AccStateChangeEvent(this, states::BUSY, PR_FALSE);
  nsEventShell::FireEvent(stateEvent);
}

void
nsDocAccessible::AddDependentIDsFor(nsAccessible* aRelProvider,
                                    nsIAtom* aRelAttr)
{
  for (PRUint32 idx = 0; idx < kRelationAttrsLen; idx++) {
    nsIAtom* relAttr = *kRelationAttrs[idx];
    if (aRelAttr && aRelAttr != relAttr)
      continue;

    if (relAttr == nsAccessibilityAtoms::_for) {
      if (!aRelProvider->GetContent()->IsHTML() ||
          aRelProvider->GetContent()->Tag() != nsAccessibilityAtoms::label &&
          aRelProvider->GetContent()->Tag() != nsAccessibilityAtoms::output)
        continue;

    } else if (relAttr == nsAccessibilityAtoms::control) {
      if (!aRelProvider->GetContent()->IsXUL() ||
          aRelProvider->GetContent()->Tag() != nsAccessibilityAtoms::label &&
          aRelProvider->GetContent()->Tag() != nsAccessibilityAtoms::description)
        continue;
    }

    IDRefsIterator iter(aRelProvider->GetContent(), relAttr);
    while (true) {
      const nsDependentSubstring id = iter.NextID();
      if (id.IsEmpty())
        break;

      AttrRelProviderArray* providers = mDependentIDsHash.Get(id);
      if (!providers) {
        providers = new AttrRelProviderArray();
        if (providers) {
          if (!mDependentIDsHash.Put(id, providers)) {
            delete providers;
            providers = nsnull;
          }
        }
      }

      if (providers) {
        AttrRelProvider* provider =
          new AttrRelProvider(relAttr, aRelProvider->GetContent());
        if (provider) {
          providers->AppendElement(provider);

          // We've got here during the children caching. If the referenced
          // content is not accessible then store it to pend its container
          // children invalidation (this happens immediately after the caching
          // is finished).
          nsIContent* dependentContent = iter.GetElem(id);
          if (dependentContent && !HasAccessible(dependentContent)) {
            mInvalidationList.AppendElement(dependentContent);
          }
        }
      }
    }

    // If the relation attribute is given then we don't have anything else to
    // check.
    if (aRelAttr)
      break;
  }
}

void
nsDocAccessible::RemoveDependentIDsFor(nsAccessible* aRelProvider,
                                       nsIAtom* aRelAttr)
{
  for (PRUint32 idx = 0; idx < kRelationAttrsLen; idx++) {
    nsIAtom* relAttr = *kRelationAttrs[idx];
    if (aRelAttr && aRelAttr != *kRelationAttrs[idx])
      continue;

    IDRefsIterator iter(aRelProvider->GetContent(), relAttr);
    while (true) {
      const nsDependentSubstring id = iter.NextID();
      if (id.IsEmpty())
        break;

      AttrRelProviderArray* providers = mDependentIDsHash.Get(id);
      if (providers) {
        for (PRUint32 jdx = 0; jdx < providers->Length(); ) {
          AttrRelProvider* provider = (*providers)[jdx];
          if (provider->mRelAttr == relAttr &&
              provider->mContent == aRelProvider->GetContent())
            providers->RemoveElement(provider);
          else
            jdx++;
        }
        if (providers->Length() == 0)
          mDependentIDsHash.Remove(id);
      }
    }

    // If the relation attribute is given then we don't have anything else to
    // check.
    if (aRelAttr)
      break;
  }
}

bool
nsDocAccessible::UpdateAccessibleOnAttrChange(dom::Element* aElement,
                                              nsIAtom* aAttribute)
{
  if (aAttribute == nsAccessibilityAtoms::role) {
    // It is common for js libraries to set the role on the body element after
    // the document has loaded. In this case we just update the role map entry.
    if (mContent == aElement) {
      SetRoleMapEntry(nsAccUtils::GetRoleMapEntry(aElement));
      return true;
    }

    // Recreate the accessible when role is changed because we might require a
    // different accessible class for the new role or the accessible may expose
    // a different sets of interfaces (COM restriction).
    HandleNotification<nsDocAccessible, nsIContent>
      (this, &nsDocAccessible::RecreateAccessible, aElement);

    return true;
  }

  if (aAttribute == nsAccessibilityAtoms::href ||
      aAttribute == nsAccessibilityAtoms::onclick) {
    // Not worth the expense to ensure which namespace these are in. It doesn't
    // kill use to recreate the accessible even if the attribute was used in
    // the wrong namespace or an element that doesn't support it.

    // Recreate accessible asynchronously to allow the content to handle
    // the attribute change.
    mNotificationController->ScheduleNotification<nsDocAccessible, nsIContent>
      (this, &nsDocAccessible::RecreateAccessible, aElement);

    return true;
  }

  if (aAttribute == nsAccessibilityAtoms::aria_multiselectable &&
      aElement->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::role)) {
    // This affects whether the accessible supports SelectAccessible.
    // COM says we cannot change what interfaces are supported on-the-fly,
    // so invalidate this object. A new one will be created on demand.
    HandleNotification<nsDocAccessible, nsIContent>
      (this, &nsDocAccessible::RecreateAccessible, aElement);

    return true;
  }

  return false;
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

  if (mNotificationController)
    mNotificationController->QueueEvent(aEvent);

  return NS_OK;
}

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

void
nsDocAccessible::ProcessAnchorJump(nsIContent* aTargetNode)
{
  // If the jump target is not accessible then fire an event for nearest
  // accessible in parent chain.
  nsAccessible* target = GetAccessibleOrContainer(aTargetNode);
  if (!target)
    return;

  // XXX: bug 625699, note in some cases the node could go away before we flush
  // the queue, for example if the node becomes inaccessible, or is removed from
  // the DOM.
  FireDelayedAccessibleEvent(nsIAccessibleEvent::EVENT_SCROLLING_START,
                             target->GetNode());
}

void
nsDocAccessible::ProcessContentInserted(nsAccessible* aContainer,
                                        const nsTArray<nsCOMPtr<nsIContent> >* aInsertedContent)
{
  // Process the notification if the container accessible is still in tree.
  if (!HasAccessible(aContainer->GetNode()))
    return;

  if (aContainer == this) {
    // If new root content has been inserted then update it.
    nsIContent* rootContent = nsCoreUtils::GetRoleContent(mDocument);
    if (rootContent && rootContent != mContent)
      mContent = rootContent;

    // Continue to update the tree even if we don't have root content.
    // For example, elements may be inserted under the document element while
    // there is no HTML body element.
  }

  // XXX: Invalidate parent-child relations for container accessible and its
  // children because there's no good way to find insertion point of new child
  // accessibles into accessible tree. We need to invalidate children even
  // there's no inserted accessibles in the end because accessible children
  // are created while parent recaches child accessibles.
  aContainer->UpdateChildren();

  // The container might be changed, for example, because of the subsequent
  // overlapping content insertion (i.e. other content was inserted between this
  // inserted content and its container or the content was reinserted into
  // different container of unrelated part of tree). These cases result in
  // double processing, however generated events are coalesced and we don't
  // harm an AT.
  // Theoretically the element might be not in tree at all at this point what
  // means there's no container.
  for (PRUint32 idx = 0; idx < aInsertedContent->Length(); idx++) {
    nsAccessible* directContainer =
      GetContainerAccessible(aInsertedContent->ElementAt(idx));
    if (directContainer)
      UpdateTree(directContainer, aInsertedContent->ElementAt(idx), true);
  }
}

void
nsDocAccessible::UpdateTree(nsAccessible* aContainer, nsIContent* aChildNode,
                            bool aIsInsert)
{
  PRUint32 updateFlags = eNoAccessible;

  // If child node is not accessible then look for its accessible children.
  nsAccessible* child = GetAccessible(aChildNode);
  if (child) {
    updateFlags |= UpdateTreeInternal(child, aIsInsert);

  } else {
    nsAccTreeWalker walker(mWeakShell, aChildNode,
                           aContainer->GetAllowsAnonChildAccessibles(), true);

    while ((child = walker.NextChild()))
      updateFlags |= UpdateTreeInternal(child, aIsInsert);
  }

  // Content insertion/removal is not cause of accessible tree change.
  if (updateFlags == eNoAccessible)
    return;

  // Check to see if change occurred inside an alert, and fire an EVENT_ALERT
  // if it did.
  if (aIsInsert && !(updateFlags & eAlertAccessible)) {
    // XXX: tree traversal is perf issue, accessible should know if they are
    // children of alert accessible to avoid this.
    nsAccessible* ancestor = aContainer;
    while (ancestor) {
      if (ancestor->ARIARole() == nsIAccessibleRole::ROLE_ALERT) {
        FireDelayedAccessibleEvent(nsIAccessibleEvent::EVENT_ALERT,
                                   ancestor->GetNode());
        break;
      }

      // Don't climb above this document.
      if (ancestor == this)
        break;

      ancestor = ancestor->Parent();
    }
  }

  // Fire value change event.
  if (aContainer->Role() == nsIAccessibleRole::ROLE_ENTRY) {
    FireDelayedAccessibleEvent(nsIAccessibleEvent::EVENT_VALUE_CHANGE,
                               aContainer->GetNode());
  }

  // Fire reorder event so the MSAA clients know the children have changed. Also
  // the event is used internally by MSAA layer.
  nsRefPtr<AccEvent> reorderEvent =
    new AccEvent(nsIAccessibleEvent::EVENT_REORDER, aContainer->GetNode(),
                 eAutoDetect, AccEvent::eCoalesceFromSameSubtree);
  if (reorderEvent)
    FireDelayedAccessibleEvent(reorderEvent);
}

PRUint32
nsDocAccessible::UpdateTreeInternal(nsAccessible* aChild, bool aIsInsert)
{
  PRUint32 updateFlags = eAccessible;

  nsINode* node = aChild->GetNode();
  if (aIsInsert) {
    // Create accessible tree for shown accessible.
    CacheChildrenInSubtree(aChild);

  } else {
    // Fire menupopup end event before hide event if a menu goes away.

    // XXX: We don't look into children of hidden subtree to find hiding
    // menupopup (as we did prior bug 570275) because we don't do that when
    // menu is showing (and that's impossible until bug 606924 is fixed).
    // Nevertheless we should do this at least because layout coalesces
    // the changes before our processing and we may miss some menupopup
    // events. Now we just want to be consistent in content insertion/removal
    // handling.
    if (aChild->ARIARole() == nsIAccessibleRole::ROLE_MENUPOPUP) {
      nsRefPtr<AccEvent> event =
        new AccEvent(nsIAccessibleEvent::EVENT_MENUPOPUP_END, aChild);

      if (event)
        FireDelayedAccessibleEvent(event);
    }
  }

  // Fire show/hide event.
  nsRefPtr<AccEvent> event;
  if (aIsInsert)
    event = new AccShowEvent(aChild, node);
  else
    event = new AccHideEvent(aChild, node);

  if (event)
    FireDelayedAccessibleEvent(event);

  if (aIsInsert) {
    PRUint32 ariaRole = aChild->ARIARole();
    if (ariaRole == nsIAccessibleRole::ROLE_MENUPOPUP) {
      // Fire EVENT_MENUPOPUP_START if ARIA menu appears.
      FireDelayedAccessibleEvent(nsIAccessibleEvent::EVENT_MENUPOPUP_START,
                                 node, AccEvent::eRemoveDupes);

    } else if (ariaRole == nsIAccessibleRole::ROLE_ALERT) {
      // Fire EVENT_ALERT if ARIA alert appears.
      updateFlags = eAlertAccessible;
      FireDelayedAccessibleEvent(nsIAccessibleEvent::EVENT_ALERT, node,
                                 AccEvent::eRemoveDupes);
    }

    // If focused node has been shown then it means its frame was recreated
    // while it's focused. Fire focus event on new focused accessible. If
    // the queue contains focus event for this node then it's suppressed by
    // this one.
    if (node == gLastFocusedNode) {
      FireDelayedAccessibleEvent(nsIAccessibleEvent::EVENT_FOCUS,
                                 node, AccEvent::eCoalesceFromSameDocument);
    }
  } else {
    // Update the tree for content removal.
    // The accessible parent may differ from container accessible if
    // the parent doesn't have own DOM node like list accessible for HTML
    // selects.
    nsAccessible* parent = aChild->Parent();
    NS_ASSERTION(parent, "No accessible parent?!");
    if (parent)
      parent->RemoveChild(aChild);

    UncacheChildrenInSubtree(aChild);
  }

  return updateFlags;
}

void
nsDocAccessible::CacheChildrenInSubtree(nsAccessible* aRoot)
{
  aRoot->EnsureChildren();

  // Make sure we create accessible tree defined in DOM only, i.e. if accessible
  // provides specific tree (like XUL trees) then tree creation is handled by
  // this accessible.
  PRUint32 count = aRoot->ContentChildCount();
  for (PRUint32 idx = 0; idx < count; idx++) {
    nsAccessible* child = aRoot->ContentChildAt(idx);
    NS_ASSERTION(child, "Illicit tree change while tree is created!");
    // Don't cross document boundaries.
    if (child && child->IsContent())
      CacheChildrenInSubtree(child);
  }
}

void
nsDocAccessible::UncacheChildrenInSubtree(nsAccessible* aRoot)
{
  if (aRoot->IsElement())
    RemoveDependentIDsFor(aRoot);

  PRUint32 count = aRoot->ContentChildCount();
  for (PRUint32 idx = 0; idx < count; idx++)
    UncacheChildrenInSubtree(aRoot->ContentChildAt(idx));

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
  PRUint32 count = aAccessible->ContentChildCount();
  for (PRUint32 idx = 0, jdx = 0; idx < count; idx++) {
    nsAccessible* child = aAccessible->ContentChildAt(jdx);
    if (!child->IsBoundToParent()) {
      NS_ERROR("Parent refers to a child, child doesn't refer to parent!");
      jdx++;
    }

    ShutdownChildrenInSubtree(child);
  }

  UnbindFromDocument(aAccessible);
}

bool
nsDocAccessible::IsLoadEventTarget() const
{
  nsCOMPtr<nsISupports> container = mDocument->GetContainer();
  nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem =
    do_QueryInterface(container);
  NS_ASSERTION(docShellTreeItem, "No document shell for document!");

  nsCOMPtr<nsIDocShellTreeItem> parentTreeItem;
  docShellTreeItem->GetParent(getter_AddRefs(parentTreeItem));

  // It's not a root document.
  if (parentTreeItem) {
    nsCOMPtr<nsIDocShellTreeItem> sameTypeRoot;
    docShellTreeItem->GetSameTypeRootTreeItem(getter_AddRefs(sameTypeRoot));

    // It's not a sub document, i.e. a frame or iframe.
    return (sameTypeRoot == docShellTreeItem);
  }

  // It's not chrome root document.
  PRInt32 contentType;
  docShellTreeItem->GetItemType(&contentType);
  return (contentType == nsIDocShellTreeItem::typeContent);
}


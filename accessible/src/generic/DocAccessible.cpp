/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Accessible-inl.h"
#include "AccIterator.h"
#include "DocAccessible-inl.h"
#include "nsAccCache.h"
#include "nsAccessiblePivot.h"
#include "nsAccUtils.h"
#include "nsEventShell.h"
#include "nsTextEquivUtils.h"
#include "Role.h"
#include "RootAccessible.h"
#include "TreeWalker.h"

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
#include "mozilla/Assertions.h"
#include "mozilla/dom/Element.h"

#ifdef MOZ_XUL
#include "nsIXULDocument.h"
#endif

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// Static member initialization

static nsIAtom** kRelationAttrs[] =
{
  &nsGkAtoms::aria_labelledby,
  &nsGkAtoms::aria_describedby,
  &nsGkAtoms::aria_owns,
  &nsGkAtoms::aria_controls,
  &nsGkAtoms::aria_flowto,
  &nsGkAtoms::_for,
  &nsGkAtoms::control
};

static const uint32_t kRelationAttrsLen = NS_ARRAY_LENGTH(kRelationAttrs);

////////////////////////////////////////////////////////////////////////////////
// Constructor/desctructor

DocAccessible::
  DocAccessible(nsIDocument* aDocument, nsIContent* aRootContent,
                  nsIPresShell* aPresShell) :
  HyperTextAccessibleWrap(aRootContent, this),
  mDocumentNode(aDocument), mScrollPositionChangedTicks(0),
  mLoadState(eTreeConstructionPending), mLoadEventType(0),
  mVirtualCursor(nullptr),
  mPresShell(aPresShell)
{
  mGenericTypes |= eDocument;
  mStateFlags |= eNotNodeMapEntry;

  MOZ_ASSERT(mPresShell, "should have been given a pres shell");
  mPresShell->SetDocAccessible(this);

  mDependentIDsHash.Init();
  // XXX aaronl should we use an algorithm for the initial cache size?
  mAccessibleCache.Init(kDefaultCacheSize);
  mNodeToAccessibleMap.Init(kDefaultCacheSize);

  // If this is a XUL Document, it should not implement nsHyperText
  if (mDocumentNode && mDocumentNode->IsXUL())
    mGenericTypes &= ~eHyperText;

  // For GTK+ native window, we do nothing here.
  if (!mDocumentNode)
    return;

  // DocManager creates document accessible when scrollable frame is
  // available already, it should be safe time to add scroll listener.
  AddScrollListener();

  // We provide a virtual cursor if this is a root doc or if it's a tab doc.
  mIsCursorable = (!(mDocumentNode->GetParentDocument()) ||
                   nsCoreUtils::IsTabDocument(mDocumentNode));
}

DocAccessible::~DocAccessible()
{
  NS_ASSERTION(!mPresShell, "LastRelease was never called!?!");
}


////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_CYCLE_COLLECTION_CLASS(DocAccessible)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(DocAccessible, Accessible)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocumentNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNotificationController)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mVirtualCursor)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChildDocuments)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAccessibleCache)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(DocAccessible, Accessible)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocumentNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mNotificationController)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mVirtualCursor)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mChildDocuments)
  tmp->mDependentIDsHash.Clear();
  tmp->mNodeToAccessibleMap.Clear();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAccessibleCache)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(DocAccessible)
  NS_INTERFACE_MAP_ENTRY(nsIAccessibleDocument)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentObserver)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIAccessiblePivotObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAccessibleDocument)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIAccessibleCursorable,
                                     mIsCursorable)
    foundInterface = 0;

  nsresult status;
  if (!foundInterface) {
    // HTML document accessible must inherit from HyperTextAccessible to get
    // support text interfaces. XUL document accessible doesn't need this.
    // However at some point we may push <body> to implement the interfaces and
    // return DocAccessible to inherit from AccessibleWrap.

    status = IsHyperText() ? 
      HyperTextAccessible::QueryInterface(aIID, (void**)&foundInterface) :
      Accessible::QueryInterface(aIID, (void**)&foundInterface);
  } else {
    NS_ADDREF(foundInterface);
    status = NS_OK;
  }

  *aInstancePtr = foundInterface;
  return status;
}

NS_IMPL_ADDREF_INHERITED(DocAccessible, HyperTextAccessible)
NS_IMPL_RELEASE_INHERITED(DocAccessible, HyperTextAccessible)

////////////////////////////////////////////////////////////////////////////////
// nsIAccessible

ENameValueFlag
DocAccessible::Name(nsString& aName)
{
  aName.Truncate();

  if (mParent) {
    mParent->Name(aName); // Allow owning iframe to override the name
  }
  if (aName.IsEmpty()) {
    // Allow name via aria-labelledby or title attribute
    Accessible::Name(aName);
  }
  if (aName.IsEmpty()) {
    GetTitle(aName);   // Try title element
  }
  if (aName.IsEmpty()) {   // Last resort: use URL
    GetURL(aName);
  }
 
  return eNameOK;
}

// Accessible public method
role
DocAccessible::NativeRole()
{
  nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem =
    nsCoreUtils::GetDocShellTreeItemFor(mDocumentNode);
  if (docShellTreeItem) {
    nsCOMPtr<nsIDocShellTreeItem> sameTypeRoot;
    docShellTreeItem->GetSameTypeRootTreeItem(getter_AddRefs(sameTypeRoot));
    int32_t itemType;
    docShellTreeItem->GetItemType(&itemType);
    if (sameTypeRoot == docShellTreeItem) {
      // Root of content or chrome tree
      if (itemType == nsIDocShellTreeItem::typeChrome)
        return roles::CHROME_WINDOW;

      if (itemType == nsIDocShellTreeItem::typeContent) {
#ifdef MOZ_XUL
        nsCOMPtr<nsIXULDocument> xulDoc(do_QueryInterface(mDocumentNode));
        if (xulDoc)
          return roles::APPLICATION;
#endif
        return roles::DOCUMENT;
      }
    }
    else if (itemType == nsIDocShellTreeItem::typeContent) {
      return roles::DOCUMENT;
    }
  }

  return roles::PANE; // Fall back;
}

void
DocAccessible::Description(nsString& aDescription)
{
  if (mParent)
    mParent->Description(aDescription);

  if (HasOwnContent() && aDescription.IsEmpty()) {
    nsTextEquivUtils::
      GetTextEquivFromIDRefs(this, nsGkAtoms::aria_describedby,
                             aDescription);
  }
}

// Accessible public method
uint64_t
DocAccessible::NativeState()
{
  // Document is always focusable.
  uint64_t state = states::FOCUSABLE; // keep in sync with NativeInteractiveState() impl
  if (FocusMgr()->IsFocused(this))
    state |= states::FOCUSED;

  // Expose stale state until the document is ready (DOM is loaded and tree is
  // constructed).
  if (!HasLoadState(eReady))
    state |= states::STALE;

  // Expose state busy until the document and all its subdocuments is completely
  // loaded.
  if (!HasLoadState(eCompletelyLoaded))
    state |= states::BUSY;

  nsIFrame* frame = GetFrame();
  if (!frame ||
      !frame->IsVisibleConsideringAncestors(nsIFrame::VISIBILITY_CROSS_CHROME_CONTENT_BOUNDARY)) {
    state |= states::INVISIBLE | states::OFFSCREEN;
  }

  nsCOMPtr<nsIEditor> editor = GetEditor();
  state |= editor ? states::EDITABLE : states::READONLY;

  return state;
}

uint64_t
DocAccessible::NativeInteractiveState() const
{
  // Document is always focusable.
  return states::FOCUSABLE;
}

bool
DocAccessible::NativelyUnavailable() const
{
  return false;
}

// Accessible public method
void
DocAccessible::ApplyARIAState(uint64_t* aState) const
{
  // Grab states from content element.
  if (mContent)
    Accessible::ApplyARIAState(aState);

  // Allow iframe/frame etc. to have final state override via ARIA.
  if (mParent)
    mParent->ApplyARIAState(aState);
}

already_AddRefed<nsIPersistentProperties>
DocAccessible::Attributes()
{
  nsCOMPtr<nsIPersistentProperties> attributes =
    HyperTextAccessibleWrap::Attributes();

  // No attributes if document is not attached to the tree or if it's a root
  // document.
  if (!mParent || IsRoot())
    return attributes.forget();

  // Override ARIA object attributes from outerdoc.
  aria::AttrIterator attribIter(mParent->GetContent());
  nsAutoString name, value, unused;
  while(attribIter.Next(name, value))
    attributes->SetStringProperty(NS_ConvertUTF16toUTF8(name), value, unused);

  return attributes.forget();
}

Accessible*
DocAccessible::FocusedChild()
{
  // Return an accessible for the current global focus, which does not have to
  // be contained within the current document.
  return FocusMgr()->FocusedAccessible();
}

NS_IMETHODIMP
DocAccessible::TakeFocus()
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  // Focus the document.
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_STATE(fm);

  nsCOMPtr<nsIDOMElement> newFocus;
  return fm->MoveFocus(mDocumentNode->GetWindow(), nullptr,
                       nsIFocusManager::MOVEFOCUS_ROOT, 0,
                       getter_AddRefs(newFocus));
}


////////////////////////////////////////////////////////////////////////////////
// nsIAccessibleDocument

NS_IMETHODIMP
DocAccessible::GetURL(nsAString& aURL)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsISupports> container = mDocumentNode->GetContainer();
  nsCOMPtr<nsIWebNavigation> webNav(do_GetInterface(container));
  nsAutoCString theURL;
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
DocAccessible::GetTitle(nsAString& aTitle)
{
  nsCOMPtr<nsIDOMDocument> domDocument = do_QueryInterface(mDocumentNode);
  if (!domDocument) {
    return NS_ERROR_FAILURE;
  }
  return domDocument->GetTitle(aTitle);
}

NS_IMETHODIMP
DocAccessible::GetMimeType(nsAString& aMimeType)
{
  nsCOMPtr<nsIDOMDocument> domDocument = do_QueryInterface(mDocumentNode);
  if (!domDocument) {
    return NS_ERROR_FAILURE;
  }
  return domDocument->GetContentType(aMimeType);
}

NS_IMETHODIMP
DocAccessible::GetDocType(nsAString& aDocType)
{
  nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(mDocumentNode));
  nsCOMPtr<nsIDOMDocumentType> docType;

#ifdef MOZ_XUL
  nsCOMPtr<nsIXULDocument> xulDoc(do_QueryInterface(mDocumentNode));
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

NS_IMETHODIMP
DocAccessible::GetNameSpaceURIForID(int16_t aNameSpaceID, nsAString& aNameSpaceURI)
{
  if (mDocumentNode) {
    nsCOMPtr<nsINameSpaceManager> nameSpaceManager =
        do_GetService(NS_NAMESPACEMANAGER_CONTRACTID);
    if (nameSpaceManager)
      return nameSpaceManager->GetNameSpaceURI(aNameSpaceID, aNameSpaceURI);
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
DocAccessible::GetWindowHandle(void** aWindow)
{
  NS_ENSURE_ARG_POINTER(aWindow);
  *aWindow = GetNativeWindow();
  return NS_OK;
}

NS_IMETHODIMP
DocAccessible::GetWindow(nsIDOMWindow** aDOMWin)
{
  *aDOMWin = nullptr;
  if (!mDocumentNode) {
    return NS_ERROR_FAILURE;  // Accessible is Shutdown()
  }
  *aDOMWin = mDocumentNode->GetWindow();

  if (!*aDOMWin)
    return NS_ERROR_FAILURE;  // No DOM Window

  NS_ADDREF(*aDOMWin);

  return NS_OK;
}

NS_IMETHODIMP
DocAccessible::GetDOMDocument(nsIDOMDocument** aDOMDocument)
{
  NS_ENSURE_ARG_POINTER(aDOMDocument);
  *aDOMDocument = nullptr;

  if (mDocumentNode)
    CallQueryInterface(mDocumentNode, aDOMDocument);

  return NS_OK;
}

NS_IMETHODIMP
DocAccessible::GetParentDocument(nsIAccessibleDocument** aDocument)
{
  NS_ENSURE_ARG_POINTER(aDocument);
  *aDocument = nullptr;

  if (!IsDefunct())
    NS_IF_ADDREF(*aDocument = ParentDocument());

  return NS_OK;
}

NS_IMETHODIMP
DocAccessible::GetChildDocumentCount(uint32_t* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  *aCount = 0;

  if (!IsDefunct())
    *aCount = ChildDocumentCount();

  return NS_OK;
}

NS_IMETHODIMP
DocAccessible::GetChildDocumentAt(uint32_t aIndex,
                                  nsIAccessibleDocument** aDocument)
{
  NS_ENSURE_ARG_POINTER(aDocument);
  *aDocument = nullptr;

  if (IsDefunct())
    return NS_OK;

  NS_IF_ADDREF(*aDocument = GetChildDocumentAt(aIndex));
  return *aDocument ? NS_OK : NS_ERROR_INVALID_ARG;
}

// nsIAccessibleVirtualCursor method
NS_IMETHODIMP
DocAccessible::GetVirtualCursor(nsIAccessiblePivot** aVirtualCursor)
{
  NS_ENSURE_ARG_POINTER(aVirtualCursor);
  *aVirtualCursor = nullptr;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  NS_ENSURE_TRUE(mIsCursorable, NS_ERROR_NOT_IMPLEMENTED);

  if (!mVirtualCursor) {
    mVirtualCursor = new nsAccessiblePivot(this);
    mVirtualCursor->AddObserver(this);
  }

  NS_ADDREF(*aVirtualCursor = mVirtualCursor);
  return NS_OK;
}

// HyperTextAccessible method
already_AddRefed<nsIEditor>
DocAccessible::GetEditor() const
{
  // Check if document is editable (designMode="on" case). Otherwise check if
  // the html:body (for HTML document case) or document element is editable.
  if (!mDocumentNode->HasFlag(NODE_IS_EDITABLE) &&
      (!mContent || !mContent->HasFlag(NODE_IS_EDITABLE)))
    return nullptr;

  nsCOMPtr<nsISupports> container = mDocumentNode->GetContainer();
  nsCOMPtr<nsIEditingSession> editingSession(do_GetInterface(container));
  if (!editingSession)
    return nullptr; // No editing session interface

  nsCOMPtr<nsIEditor> editor;
  editingSession->GetEditorForWindow(mDocumentNode->GetWindow(), getter_AddRefs(editor));
  if (!editor)
    return nullptr;

  bool isEditable = false;
  editor->GetIsDocumentEditable(&isEditable);
  if (isEditable)
    return editor.forget();

  return nullptr;
}

// DocAccessible public method
Accessible*
DocAccessible::GetAccessible(nsINode* aNode) const
{
  Accessible* accessible = mNodeToAccessibleMap.Get(aNode);

  // No accessible in the cache, check if the given ID is unique ID of this
  // document accessible.
  if (!accessible) {
    if (GetNode() != aNode)
      return nullptr;

    accessible = const_cast<DocAccessible*>(this);
  }

#ifdef DEBUG
  // All cached accessible nodes should be in the parent
  // It will assert if not all the children were created
  // when they were first cached, and no invalidation
  // ever corrected parent accessible's child cache.
  Accessible* parent = accessible->Parent();
  if (parent)
    parent->TestChildCache(accessible);
#endif

  return accessible;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessNode

void
DocAccessible::Init()
{
#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eDocCreate))
    logging::DocCreate("document initialize", mDocumentNode, this);
#endif

  // Initialize notification controller.
  mNotificationController = new NotificationController(this, mPresShell);

  // Mark the document accessible as loaded if its DOM document was loaded at
  // this point (this can happen because a11y is started late or DOM document
  // having no container was loaded.
  if (mDocumentNode->GetReadyStateEnum() == nsIDocument::READYSTATE_COMPLETE)
    mLoadState |= eDOMLoaded;

  AddEventListeners();
}

void
DocAccessible::Shutdown()
{
  if (!mPresShell) // already shutdown
    return;

#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eDocDestroy))
    logging::DocDestroy("document shutdown", mDocumentNode, this);
#endif

  mPresShell->SetDocAccessible(nullptr);

  if (mNotificationController) {
    mNotificationController->Shutdown();
    mNotificationController = nullptr;
  }

  RemoveEventListeners();

  // Mark the document as shutdown before AT is notified about the document
  // removal from its container (valid for root documents on ATK and due to
  // some reason for MSAA, refer to bug 757392 for details).
  mStateFlags |= eIsDefunct;
  nsCOMPtr<nsIDocument> kungFuDeathGripDoc = mDocumentNode;
  mDocumentNode = nullptr;

  if (mParent) {
    DocAccessible* parentDocument = mParent->Document();
    if (parentDocument)
      parentDocument->RemoveChildDocument(this);

    mParent->RemoveChild(this);
  }

  // Walk the array backwards because child documents remove themselves from the
  // array as they are shutdown.
  int32_t childDocCount = mChildDocuments.Length();
  for (int32_t idx = childDocCount - 1; idx >= 0; idx--)
    mChildDocuments[idx]->Shutdown();

  mChildDocuments.Clear();

  if (mVirtualCursor) {
    mVirtualCursor->RemoveObserver(this);
    mVirtualCursor = nullptr;
  }

  mPresShell = nullptr;  // Avoid reentrancy

  mDependentIDsHash.Clear();
  mNodeToAccessibleMap.Clear();
  ClearCache(mAccessibleCache);

  HyperTextAccessibleWrap::Shutdown();

  GetAccService()->NotifyOfDocumentShutdown(kungFuDeathGripDoc);
}

nsIFrame*
DocAccessible::GetFrame() const
{
  nsIFrame* root = nullptr;
  if (mPresShell)
    root = mPresShell->GetRootFrame();

  return root;
}

// DocAccessible protected member
void
DocAccessible::GetBoundsRect(nsRect& aBounds, nsIFrame** aRelativeFrame)
{
  *aRelativeFrame = GetFrame();

  nsIDocument *document = mDocumentNode;
  nsIDocument *parentDoc = nullptr;

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

// DocAccessible protected member
nsresult
DocAccessible::AddEventListeners()
{
  nsCOMPtr<nsISupports> container = mDocumentNode->GetContainer();
  nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem(do_QueryInterface(container));

  // We want to add a command observer only if the document is content and has
  // an editor.
  int32_t itemType;
  docShellTreeItem->GetItemType(&itemType);
  if (itemType == nsIDocShellTreeItem::typeContent) {
    nsCOMPtr<nsICommandManager> commandManager = do_GetInterface(docShellTreeItem);
    if (commandManager)
      commandManager->AddCommandObserver(this, "obs_documentCreated");
  }

  a11y::RootAccessible* rootAccessible = RootAccessible();
  NS_ENSURE_TRUE(rootAccessible, NS_ERROR_FAILURE);
  nsRefPtr<nsCaretAccessible> caretAccessible = rootAccessible->GetCaretAccessible();
  if (caretAccessible)
    caretAccessible->AddDocSelectionListener(mPresShell);

  // Add document observer.
  mDocumentNode->AddObserver(this);
  return NS_OK;
}

// DocAccessible protected member
nsresult
DocAccessible::RemoveEventListeners()
{
  // Remove listeners associated with content documents
  // Remove scroll position listener
  RemoveScrollListener();

  NS_ASSERTION(mDocumentNode, "No document during removal of listeners.");

  if (mDocumentNode) {
    mDocumentNode->RemoveObserver(this);

    nsCOMPtr<nsISupports> container = mDocumentNode->GetContainer();
    nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem(do_QueryInterface(container));
    NS_ASSERTION(docShellTreeItem, "doc should support nsIDocShellTreeItem.");

    if (docShellTreeItem) {
      int32_t itemType;
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
    mScrollWatchTimer = nullptr;
    NS_RELEASE_THIS(); // Kung fu death grip
  }

  a11y::RootAccessible* rootAccessible = RootAccessible();
  if (rootAccessible) {
    nsRefPtr<nsCaretAccessible> caretAccessible = rootAccessible->GetCaretAccessible();
    if (caretAccessible)
      caretAccessible->RemoveDocSelectionListener(mPresShell);
  }

  return NS_OK;
}

void
DocAccessible::ScrollTimerCallback(nsITimer* aTimer, void* aClosure)
{
  DocAccessible* docAcc = reinterpret_cast<DocAccessible*>(aClosure);

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
      docAcc->mScrollWatchTimer = nullptr;
      NS_RELEASE(docAcc); // Release kung fu death grip
    }
  }
}

// DocAccessible protected member
void
DocAccessible::AddScrollListener()
{
  if (!mPresShell)
    return;

  nsIScrollableFrame* sf = mPresShell->GetRootScrollFrameAsScrollableExternal();
  if (sf) {
    sf->AddScrollPositionListener(this);
#ifdef A11Y_LOG
    if (logging::IsEnabled(logging::eDocCreate))
      logging::Text("add scroll listener");
#endif
  }
}

// DocAccessible protected member
void
DocAccessible::RemoveScrollListener()
{
  if (!mPresShell)
    return;
 
  nsIScrollableFrame* sf = mPresShell->GetRootScrollFrameAsScrollableExternal();
  if (sf) {
    sf->RemoveScrollPositionListener(this);
  }
}

////////////////////////////////////////////////////////////////////////////////
// nsIScrollPositionListener

void
DocAccessible::ScrollPositionDidChange(nscoord aX, nscoord aY)
{
  // Start new timer, if the timer cycles at least 1 full cycle without more scroll position changes,
  // then the ::Notify() method will fire the accessibility event for scroll position changes
  const uint32_t kScrollPosCheckWait = 50;
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

NS_IMETHODIMP
DocAccessible::Observe(nsISupports* aSubject, const char* aTopic,
                       const PRUnichar* aData)
{
  if (!nsCRT::strcmp(aTopic,"obs_documentCreated")) {    
    // State editable will now be set, readonly is now clear
    // Normally we only fire delayed events created from the node, not an
    // accessible object. See the AccStateChangeEvent constructor for details
    // about this exceptional case.
    nsRefPtr<AccEvent> event =
      new AccStateChangeEvent(this, states::EDITABLE, true);
    FireDelayedEvent(event);
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessiblePivotObserver

NS_IMETHODIMP
DocAccessible::OnPivotChanged(nsIAccessiblePivot* aPivot,
                              nsIAccessible* aOldAccessible,
                              int32_t aOldStart, int32_t aOldEnd,
                              PivotMoveReason aReason)
{
  nsRefPtr<AccEvent> event = new AccVCChangeEvent(this, aOldAccessible,
                                                  aOldStart, aOldEnd,
                                                  aReason);
  nsEventShell::FireEvent(event);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIDocumentObserver

NS_IMPL_NSIDOCUMENTOBSERVER_CORE_STUB(DocAccessible)
NS_IMPL_NSIDOCUMENTOBSERVER_LOAD_STUB(DocAccessible)
NS_IMPL_NSIDOCUMENTOBSERVER_STYLE_STUB(DocAccessible)

void
DocAccessible::AttributeWillChange(nsIDocument* aDocument,
                                   dom::Element* aElement,
                                   int32_t aNameSpaceID,
                                   nsIAtom* aAttribute, int32_t aModType)
{
  Accessible* accessible = GetAccessible(aElement);
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
  if (aAttribute == nsGkAtoms::aria_checked ||
      aAttribute == nsGkAtoms::aria_pressed) {
    mARIAAttrOldValue = (aModType != nsIDOMMutationEvent::ADDITION) ?
      nsAccUtils::GetARIAToken(aElement, aAttribute) : nullptr;
  }
}

void
DocAccessible::AttributeChanged(nsIDocument* aDocument,
                                dom::Element* aElement,
                                int32_t aNameSpaceID, nsIAtom* aAttribute,
                                int32_t aModType)
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
  Accessible* accessible = GetAccessible(aElement);
  if (!accessible) {
    if (mContent != aElement)
      return;

    accessible = this;
  }

  // Fire accessible events iff there's an accessible, otherwise we consider
  // the accessible state wasn't changed, i.e. its state is initial state.
  AttributeChangedImpl(accessible, aNameSpaceID, aAttribute);

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

// DocAccessible protected member
void
DocAccessible::AttributeChangedImpl(Accessible* aAccessible,
                                    int32_t aNameSpaceID, nsIAtom* aAttribute)
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
  if (aAttribute == nsGkAtoms::disabled ||
      aAttribute == nsGkAtoms::aria_disabled) {

    // Note. Checking the XUL or HTML namespace would not seem to gain us
    // anything, because disabled attribute really is going to mean the same
    // thing in any namespace.

    // Note. We use the attribute instead of the disabled state bit because
    // ARIA's aria-disabled does not affect the disabled state bit.

    nsRefPtr<AccEvent> enabledChangeEvent =
      new AccStateChangeEvent(aAccessible, states::ENABLED);
    FireDelayedEvent(enabledChangeEvent);

    nsRefPtr<AccEvent> sensitiveChangeEvent =
      new AccStateChangeEvent(aAccessible, states::SENSITIVE);
    FireDelayedEvent(sensitiveChangeEvent);
    return;
  }

  // Check for namespaced ARIA attribute
  if (aNameSpaceID == kNameSpaceID_None) {
    // Check for hyphenated aria-foo property?
    if (StringBeginsWith(nsDependentAtomString(aAttribute),
                         NS_LITERAL_STRING("aria-"))) {
      ARIAAttributeChanged(aAccessible, aAttribute);
    }
  }

  if (aAttribute == nsGkAtoms::alt ||
      aAttribute == nsGkAtoms::title ||
      aAttribute == nsGkAtoms::aria_label ||
      aAttribute == nsGkAtoms::aria_labelledby) {
    FireDelayedEvent(nsIAccessibleEvent::EVENT_NAME_CHANGE, aAccessible);
    return;
  }

  if (aAttribute == nsGkAtoms::aria_busy) {
    bool isOn = aAccessible->GetContent()->
      AttrValueIs(aNameSpaceID, aAttribute, nsGkAtoms::_true, eCaseMatters);
    nsRefPtr<AccEvent> event =
      new AccStateChangeEvent(aAccessible, states::BUSY, isOn);
    FireDelayedEvent(event);
    return;
  }

  // ARIA or XUL selection
  if ((aAccessible->GetContent()->IsXUL() && aAttribute == nsGkAtoms::selected) ||
      aAttribute == nsGkAtoms::aria_selected) {
    Accessible* widget =
      nsAccUtils::GetSelectableContainer(aAccessible, aAccessible->State());
    if (widget) {
      nsIContent* elm = aAccessible->GetContent();
      AccSelChangeEvent::SelChangeType selChangeType =
        elm->AttrValueIs(aNameSpaceID, aAttribute, nsGkAtoms::_true, eCaseMatters) ?
          AccSelChangeEvent::eSelectionAdd : AccSelChangeEvent::eSelectionRemove;

      nsRefPtr<AccEvent> event =
        new AccSelChangeEvent(widget, aAccessible, selChangeType);
      FireDelayedEvent(event);
    }

    return;
  }

  if (aAttribute == nsGkAtoms::contenteditable) {
    nsRefPtr<AccEvent> editableChangeEvent =
      new AccStateChangeEvent(aAccessible, states::EDITABLE);
    FireDelayedEvent(editableChangeEvent);
    return;
  }

  if (aAttribute == nsGkAtoms::value) {
    if (aAccessible->IsProgress())
      FireDelayedEvent(nsIAccessibleEvent::EVENT_VALUE_CHANGE, aAccessible);
  }
}

// DocAccessible protected member
void
DocAccessible::ARIAAttributeChanged(Accessible* aAccessible, nsIAtom* aAttribute)
{
  // Note: For universal/global ARIA states and properties we don't care if
  // there is an ARIA role present or not.

  if (aAttribute == nsGkAtoms::aria_required) {
    nsRefPtr<AccEvent> event =
      new AccStateChangeEvent(aAccessible, states::REQUIRED);
    FireDelayedEvent(event);
    return;
  }

  if (aAttribute == nsGkAtoms::aria_invalid) {
    nsRefPtr<AccEvent> event =
      new AccStateChangeEvent(aAccessible, states::INVALID);
    FireDelayedEvent(event);
    return;
  }

  // The activedescendant universal property redirects accessible focus events
  // to the element with the id that activedescendant points to. Make sure
  // the tree up to date before processing.
  if (aAttribute == nsGkAtoms::aria_activedescendant) {
    mNotificationController->HandleNotification<DocAccessible, Accessible>
      (this, &DocAccessible::ARIAActiveDescendantChanged, aAccessible);

    return;
  }

  // We treat aria-expanded as a global ARIA state for historical reasons
  if (aAttribute == nsGkAtoms::aria_expanded) {
    nsRefPtr<AccEvent> event =
      new AccStateChangeEvent(aAccessible, states::EXPANDED);
    FireDelayedEvent(event);
    return;
  }

  // For aria attributes like drag and drop changes we fire a generic attribute
  // change event; at least until native API comes up with a more meaningful event.
  uint8_t attrFlags = nsAccUtils::GetAttributeCharacteristics(aAttribute);
  if (!(attrFlags & ATTR_BYPASSOBJ))
    FireDelayedEvent(nsIAccessibleEvent::EVENT_OBJECT_ATTRIBUTE_CHANGED,
                     aAccessible);

  nsIContent* elm = aAccessible->GetContent();
  if (!elm->HasAttr(kNameSpaceID_None, nsGkAtoms::role)) {
    // We don't care about these other ARIA attribute changes unless there is
    // an ARIA role set for the element
    // XXX: we should check the role map to see if the changed property is
    // relevant for that particular role.
    return;
  }

  // The following ARIA attributes only take affect when dynamic content role is present
  if (aAttribute == nsGkAtoms::aria_checked ||
      aAttribute == nsGkAtoms::aria_pressed) {
    const uint64_t kState = (aAttribute == nsGkAtoms::aria_checked) ?
                            states::CHECKED : states::PRESSED;
    nsRefPtr<AccEvent> event = new AccStateChangeEvent(aAccessible, kState);
    FireDelayedEvent(event);

    bool wasMixed = (mARIAAttrOldValue == nsGkAtoms::mixed);
    bool isMixed = elm->AttrValueIs(kNameSpaceID_None, aAttribute,
                                    nsGkAtoms::mixed, eCaseMatters);
    if (isMixed != wasMixed) {
      nsRefPtr<AccEvent> event =
        new AccStateChangeEvent(aAccessible, states::MIXED, isMixed);
      FireDelayedEvent(event);
    }
    return;
  }

  if (aAttribute == nsGkAtoms::aria_readonly) {
    nsRefPtr<AccEvent> event =
      new AccStateChangeEvent(aAccessible, states::READONLY);
    FireDelayedEvent(event);
    return;
  }

  // Fire value change event whenever aria-valuetext is changed, or
  // when aria-valuenow is changed and aria-valuetext is empty
  if (aAttribute == nsGkAtoms::aria_valuetext ||
      (aAttribute == nsGkAtoms::aria_valuenow &&
       (!elm->HasAttr(kNameSpaceID_None, nsGkAtoms::aria_valuetext) ||
        elm->AttrValueIs(kNameSpaceID_None, nsGkAtoms::aria_valuetext,
                         nsGkAtoms::_empty, eCaseMatters)))) {
    FireDelayedEvent(nsIAccessibleEvent::EVENT_VALUE_CHANGE, aAccessible);
    return;
  }
}

void
DocAccessible::ARIAActiveDescendantChanged(Accessible* aAccessible)
{
  nsIContent* elm = aAccessible->GetContent();
  if (elm && aAccessible->IsActiveWidget()) {
    nsAutoString id;
    if (elm->GetAttr(kNameSpaceID_None, nsGkAtoms::aria_activedescendant, id)) {
      dom::Element* activeDescendantElm = elm->OwnerDoc()->GetElementById(id);
      if (activeDescendantElm) {
        Accessible* activeDescendant = GetAccessible(activeDescendantElm);
        if (activeDescendant) {
          FocusMgr()->ActiveItemChanged(activeDescendant, false);
#ifdef A11Y_LOG
          if (logging::IsEnabled(logging::eFocus))
            logging::ActiveItemChangeCausedBy("ARIA activedescedant changed",
                                              activeDescendant);
#endif
        }
      }
    }
  }
}

void
DocAccessible::ContentAppended(nsIDocument* aDocument,
                               nsIContent* aContainer,
                               nsIContent* aFirstNewContent,
                               int32_t /* unused */)
{
}

void
DocAccessible::ContentStateChanged(nsIDocument* aDocument,
                                   nsIContent* aContent,
                                   nsEventStates aStateMask)
{
  Accessible* accessible = GetAccessible(aContent);
  if (!accessible)
    return;

  if (aStateMask.HasState(NS_EVENT_STATE_CHECKED)) {
    Accessible* widget = accessible->ContainerWidget();
    if (widget && widget->IsSelect()) {
      AccSelChangeEvent::SelChangeType selChangeType =
        aContent->AsElement()->State().HasState(NS_EVENT_STATE_CHECKED) ?
          AccSelChangeEvent::eSelectionAdd : AccSelChangeEvent::eSelectionRemove;
      nsRefPtr<AccEvent> event =
        new AccSelChangeEvent(widget, accessible, selChangeType);
      FireDelayedEvent(event);
    }
  }

  if (aStateMask.HasState(NS_EVENT_STATE_INVALID)) {
    nsRefPtr<AccEvent> event =
      new AccStateChangeEvent(accessible, states::INVALID, true);
    FireDelayedEvent(event);
  }

  if (aStateMask.HasState(NS_EVENT_STATE_VISITED)) {
    nsRefPtr<AccEvent> event =
      new AccStateChangeEvent(accessible, states::TRAVERSED, true);
    FireDelayedEvent(event);
  }
}

void
DocAccessible::DocumentStatesChanged(nsIDocument* aDocument,
                                     nsEventStates aStateMask)
{
}

void
DocAccessible::CharacterDataWillChange(nsIDocument* aDocument,
                                       nsIContent* aContent,
                                       CharacterDataChangeInfo* aInfo)
{
}

void
DocAccessible::CharacterDataChanged(nsIDocument* aDocument,
                                    nsIContent* aContent,
                                    CharacterDataChangeInfo* aInfo)
{
}

void
DocAccessible::ContentInserted(nsIDocument* aDocument, nsIContent* aContainer,
                               nsIContent* aChild, int32_t /* unused */)
{
}

void
DocAccessible::ContentRemoved(nsIDocument* aDocument, nsIContent* aContainer,
                              nsIContent* aChild, int32_t /* unused */,
                              nsIContent* aPreviousSibling)
{
}

void
DocAccessible::ParentChainChanged(nsIContent* aContent)
{
}


////////////////////////////////////////////////////////////////////////////////
// Accessible

#ifdef A11Y_LOG
nsresult
DocAccessible::HandleAccEvent(AccEvent* aEvent)
{
  if (logging::IsEnabled(logging::eDocLoad))
    logging::DocLoadEventHandled(aEvent);

  return HyperTextAccessible::HandleAccEvent(aEvent);
}
#endif

////////////////////////////////////////////////////////////////////////////////
// Public members

void*
DocAccessible::GetNativeWindow() const
{
  if (!mPresShell)
    return nullptr;

  nsIViewManager* vm = mPresShell->GetViewManager();
  if (!vm)
    return nullptr;

  nsCOMPtr<nsIWidget> widget;
  vm->GetRootWidget(getter_AddRefs(widget));
  if (widget)
    return widget->GetNativeData(NS_NATIVE_WINDOW);

  return nullptr;
}

Accessible*
DocAccessible::GetAccessibleByUniqueIDInSubtree(void* aUniqueID)
{
  Accessible* child = GetAccessibleByUniqueID(aUniqueID);
  if (child)
    return child;

  uint32_t childDocCount = mChildDocuments.Length();
  for (uint32_t childDocIdx= 0; childDocIdx < childDocCount; childDocIdx++) {
    DocAccessible* childDocument = mChildDocuments.ElementAt(childDocIdx);
    child = childDocument->GetAccessibleByUniqueIDInSubtree(aUniqueID);
    if (child)
      return child;
  }

  return nullptr;
}

Accessible*
DocAccessible::GetAccessibleOrContainer(nsINode* aNode)
{
  if (!aNode || !aNode->IsInDoc())
    return nullptr;

  nsINode* currNode = aNode;
  Accessible* accessible = nullptr;
  while (!(accessible = GetAccessible(currNode)) &&
         (currNode = currNode->GetParentNode()));

  return accessible;
}

bool
DocAccessible::BindToDocument(Accessible* aAccessible,
                              nsRoleMapEntry* aRoleMapEntry)
{
  if (!aAccessible)
    return false;

  // Put into DOM node cache.
  if (aAccessible->IsNodeMapEntry())
    mNodeToAccessibleMap.Put(aAccessible->GetNode(), aAccessible);

  // Put into unique ID cache.
  mAccessibleCache.Put(aAccessible->UniqueID(), aAccessible);

  aAccessible->SetRoleMapEntry(aRoleMapEntry);
  if (aAccessible->IsElement())
    AddDependentIDsFor(aAccessible);

  return true;
}

void
DocAccessible::UnbindFromDocument(Accessible* aAccessible)
{
  NS_ASSERTION(mAccessibleCache.GetWeak(aAccessible->UniqueID()),
               "Unbinding the unbound accessible!");

  // Fire focus event on accessible having DOM focus if active item was removed
  // from the tree.
  if (FocusMgr()->IsActiveItem(aAccessible)) {
    FocusMgr()->ActiveItemChanged(nullptr);
#ifdef A11Y_LOG
          if (logging::IsEnabled(logging::eFocus))
            logging::ActiveItemChangeCausedBy("tree shutdown", aAccessible);
#endif
  }

  // Remove an accessible from node-to-accessible map if it exists there.
  if (aAccessible->IsNodeMapEntry() &&
      mNodeToAccessibleMap.Get(aAccessible->GetNode()) == aAccessible)
    mNodeToAccessibleMap.Remove(aAccessible->GetNode());

  void* uniqueID = aAccessible->UniqueID();

  NS_ASSERTION(!aAccessible->IsDefunct(), "Shutdown the shutdown accessible!");
  aAccessible->Shutdown();

  mAccessibleCache.Remove(uniqueID);
}

void
DocAccessible::ContentInserted(nsIContent* aContainerNode,
                               nsIContent* aStartChildNode,
                               nsIContent* aEndChildNode)
{
  // Ignore content insertions until we constructed accessible tree. Otherwise
  // schedule tree update on content insertion after layout.
  if (mNotificationController && HasLoadState(eTreeConstructed)) {
    // Update the whole tree of this document accessible when the container is
    // null (document element is inserted or removed).
    Accessible* container = aContainerNode ?
      GetAccessibleOrContainer(aContainerNode) : this;

    mNotificationController->ScheduleContentInsertion(container,
                                                      aStartChildNode,
                                                      aEndChildNode);
  }
}

void
DocAccessible::ContentRemoved(nsIContent* aContainerNode,
                              nsIContent* aChildNode)
{
  // Update the whole tree of this document accessible when the container is
  // null (document element is removed).
  Accessible* container = aContainerNode ?
    GetAccessibleOrContainer(aContainerNode) : this;

  UpdateTree(container, aChildNode, false);
}

void
DocAccessible::RecreateAccessible(nsIContent* aContent)
{
#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eTree)) {
    logging::MsgBegin("TREE", "accessible recreated");
    logging::Node("content", aContent);
    logging::MsgEnd();
  }
#endif

  // XXX: we shouldn't recreate whole accessible subtree, instead we should
  // subclass hide and show events to handle them separately and implement their
  // coalescence with normal hide and show events. Note, in this case they
  // should be coalesced with normal show/hide events.

  ContentRemoved(aContent->GetParent(), aContent);
  ContentInserted(aContent->GetParent(), aContent, aContent->GetNextSibling());
}

void
DocAccessible::ProcessInvalidationList()
{
  // Invalidate children of container accessible for each element in
  // invalidation list. Allow invalidation list insertions while container
  // children are recached.
  for (uint32_t idx = 0; idx < mInvalidationList.Length(); idx++) {
    nsIContent* content = mInvalidationList[idx];
    Accessible* accessible = GetAccessible(content);
    if (!accessible) {
      Accessible* container = GetContainerAccessible(content);
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
// Accessible protected

void
DocAccessible::CacheChildren()
{
  // Search for accessible children starting from the document element since
  // some web pages tend to insert elements under it rather than document body.
  TreeWalker walker(this, mDocumentNode->GetRootElement());

  Accessible* child = nullptr;
  while ((child = walker.NextChild()) && AppendChild(child));
}

////////////////////////////////////////////////////////////////////////////////
// Protected members

void
DocAccessible::NotifyOfLoading(bool aIsReloading)
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
    new AccStateChangeEvent(this, states::BUSY, true);
  FireDelayedEvent(stateEvent);
}

void
DocAccessible::DoInitialUpdate()
{
  mLoadState |= eTreeConstructed;

  // The content element may be changed before the initial update and then we
  // miss the notification (since content tree change notifications are ignored
  // prior to initial update). Make sure the content element is valid.
  nsIContent* contentElm = nsCoreUtils::GetRoleContent(mDocumentNode);
  if (mContent != contentElm)
    mContent = contentElm;

  // Build initial tree.
  CacheChildrenInSubtree(this);

  // Fire reorder event after the document tree is constructed. Note, since
  // this reorder event is processed by parent document then events targeted to
  // this document may be fired prior to this reorder event. If this is
  // a problem then consider to keep event processing per tab document.
  if (!IsRoot()) {
    nsRefPtr<AccReorderEvent> reorderEvent = new AccReorderEvent(Parent());
    ParentDocument()->FireDelayedEvent(reorderEvent);
  }
}

void
DocAccessible::ProcessLoad()
{
  mLoadState |= eCompletelyLoaded;

#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eDocLoad))
    logging::DocCompleteLoad(this, IsLoadEventTarget());
#endif

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
    new AccStateChangeEvent(this, states::BUSY, false);
  nsEventShell::FireEvent(stateEvent);
}

void
DocAccessible::AddDependentIDsFor(Accessible* aRelProvider,
                                  nsIAtom* aRelAttr)
{
  for (uint32_t idx = 0; idx < kRelationAttrsLen; idx++) {
    nsIAtom* relAttr = *kRelationAttrs[idx];
    if (aRelAttr && aRelAttr != relAttr)
      continue;

    if (relAttr == nsGkAtoms::_for) {
      if (!aRelProvider->GetContent()->IsHTML() ||
          (aRelProvider->GetContent()->Tag() != nsGkAtoms::label &&
           aRelProvider->GetContent()->Tag() != nsGkAtoms::output))
        continue;

    } else if (relAttr == nsGkAtoms::control) {
      if (!aRelProvider->GetContent()->IsXUL() ||
          (aRelProvider->GetContent()->Tag() != nsGkAtoms::label &&
           aRelProvider->GetContent()->Tag() != nsGkAtoms::description))
        continue;
    }

    IDRefsIterator iter(this, aRelProvider->GetContent(), relAttr);
    while (true) {
      const nsDependentSubstring id = iter.NextID();
      if (id.IsEmpty())
        break;

      AttrRelProviderArray* providers = mDependentIDsHash.Get(id);
      if (!providers) {
        providers = new AttrRelProviderArray();
        if (providers) {
          mDependentIDsHash.Put(id, providers);
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
DocAccessible::RemoveDependentIDsFor(Accessible* aRelProvider,
                                     nsIAtom* aRelAttr)
{
  for (uint32_t idx = 0; idx < kRelationAttrsLen; idx++) {
    nsIAtom* relAttr = *kRelationAttrs[idx];
    if (aRelAttr && aRelAttr != *kRelationAttrs[idx])
      continue;

    IDRefsIterator iter(this, aRelProvider->GetContent(), relAttr);
    while (true) {
      const nsDependentSubstring id = iter.NextID();
      if (id.IsEmpty())
        break;

      AttrRelProviderArray* providers = mDependentIDsHash.Get(id);
      if (providers) {
        for (uint32_t jdx = 0; jdx < providers->Length(); ) {
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
DocAccessible::UpdateAccessibleOnAttrChange(dom::Element* aElement,
                                            nsIAtom* aAttribute)
{
  if (aAttribute == nsGkAtoms::role) {
    // It is common for js libraries to set the role on the body element after
    // the document has loaded. In this case we just update the role map entry.
    if (mContent == aElement) {
      SetRoleMapEntry(aria::GetRoleMap(aElement));
      return true;
    }

    // Recreate the accessible when role is changed because we might require a
    // different accessible class for the new role or the accessible may expose
    // a different sets of interfaces (COM restriction).
    RecreateAccessible(aElement);

    return true;
  }

  if (aAttribute == nsGkAtoms::href ||
      aAttribute == nsGkAtoms::onclick) {
    // Not worth the expense to ensure which namespace these are in. It doesn't
    // kill use to recreate the accessible even if the attribute was used in
    // the wrong namespace or an element that doesn't support it.

    // Make sure the accessible is recreated asynchronously to allow the content
    // to handle the attribute change.
    RecreateAccessible(aElement);
    return true;
  }

  if (aAttribute == nsGkAtoms::aria_multiselectable &&
      aElement->HasAttr(kNameSpaceID_None, nsGkAtoms::role)) {
    // This affects whether the accessible supports SelectAccessible.
    // COM says we cannot change what interfaces are supported on-the-fly,
    // so invalidate this object. A new one will be created on demand.
    RecreateAccessible(aElement);

    return true;
  }

  return false;
}

void
DocAccessible::ProcessContentInserted(Accessible* aContainer,
                                      const nsTArray<nsCOMPtr<nsIContent> >* aInsertedContent)
{
  // Process insertions if the container accessible is still in tree.
  if (!HasAccessible(aContainer->GetNode()))
    return;

  bool containerNotUpdated = true;

  for (uint32_t idx = 0; idx < aInsertedContent->Length(); idx++) {
    // The container might be changed, for example, because of the subsequent
    // overlapping content insertion (i.e. other content was inserted between
    // this inserted content and its container or the content was reinserted
    // into different container of unrelated part of tree). To avoid a double
    // processing of the content insertion ignore this insertion notification.
    // Note, the inserted content might be not in tree at all at this point what
    // means there's no container. Ignore the insertion too.

    Accessible* presentContainer =
      GetContainerAccessible(aInsertedContent->ElementAt(idx));
    if (presentContainer != aContainer)
      continue;

    if (containerNotUpdated) {
      containerNotUpdated = false;

      if (aContainer == this) {
        // If new root content has been inserted then update it.
        nsIContent* rootContent = nsCoreUtils::GetRoleContent(mDocumentNode);
        if (rootContent != mContent)
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
    }

    UpdateTree(aContainer, aInsertedContent->ElementAt(idx), true);
  }
}

void
DocAccessible::UpdateTree(Accessible* aContainer, nsIContent* aChildNode,
                          bool aIsInsert)
{
  uint32_t updateFlags = eNoAccessible;

  // If child node is not accessible then look for its accessible children.
  Accessible* child = GetAccessible(aChildNode);
#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eTree)) {
    logging::MsgBegin("TREE", "process content %s",
                      (aIsInsert ? "insertion" : "removal"));
    logging::Node("container", aContainer->GetNode());
    logging::Node("child", aChildNode);
    if (child)
      logging::Address("child", child);
    else
      logging::MsgEntry("child accessible: null");

    logging::MsgEnd();
  }
#endif

  nsRefPtr<AccReorderEvent> reorderEvent = new AccReorderEvent(aContainer);

  if (child) {
    updateFlags |= UpdateTreeInternal(child, aIsInsert, reorderEvent);

    // XXX: since select change insertion point of option contained by optgroup
    // then we need to have special processing for them (bug 690417).
    if (!aIsInsert && aChildNode->IsHTML(nsGkAtoms::optgroup) &&
        aContainer->GetContent()->IsHTML(nsGkAtoms::select)) {
      for (nsIContent* optContent = aChildNode->GetFirstChild(); optContent;
           optContent = optContent->GetNextSibling()) {
        if (optContent->IsHTML(nsGkAtoms::option)) {
          Accessible* option = GetAccessible(optContent);
          if (option) {
            NS_ASSERTION(option->Parent() == aContainer,
                         "Not expected hierarchy on HTML select!");
            if (option->Parent() == aContainer)
              updateFlags |= UpdateTreeInternal(option, aIsInsert, reorderEvent);
          }
        }
      }
    }
  } else {
    TreeWalker walker(aContainer, aChildNode, true);

    while ((child = walker.NextChild()))
      updateFlags |= UpdateTreeInternal(child, aIsInsert, reorderEvent);
  }

  // Content insertion/removal is not cause of accessible tree change.
  if (updateFlags == eNoAccessible)
    return;

  // Check to see if change occurred inside an alert, and fire an EVENT_ALERT
  // if it did.
  if (aIsInsert && !(updateFlags & eAlertAccessible)) {
    // XXX: tree traversal is perf issue, accessible should know if they are
    // children of alert accessible to avoid this.
    Accessible* ancestor = aContainer;
    while (ancestor) {
      if (ancestor->ARIARole() == roles::ALERT) {
        FireDelayedEvent(nsIAccessibleEvent::EVENT_ALERT, ancestor);
        break;
      }

      // Don't climb above this document.
      if (ancestor == this)
        break;

      ancestor = ancestor->Parent();
    }
  }

  MaybeNotifyOfValueChange(aContainer);

  // Fire reorder event so the MSAA clients know the children have changed. Also
  // the event is used internally by MSAA layer.
  FireDelayedEvent(reorderEvent);
}

uint32_t
DocAccessible::UpdateTreeInternal(Accessible* aChild, bool aIsInsert,
                                  AccReorderEvent* aReorderEvent)
{
  uint32_t updateFlags = eAccessible;

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
    if (aChild->ARIARole() == roles::MENUPOPUP)
      FireDelayedEvent(nsIAccessibleEvent::EVENT_MENUPOPUP_END, aChild);
  }

  // Fire show/hide event.
  nsRefPtr<AccMutationEvent> event;
  if (aIsInsert)
    event = new AccShowEvent(aChild, node);
  else
    event = new AccHideEvent(aChild, node);

  FireDelayedEvent(event);
  aReorderEvent->AddSubMutationEvent(event);

  if (aIsInsert) {
    roles::Role ariaRole = aChild->ARIARole();
    if (ariaRole == roles::MENUPOPUP) {
      // Fire EVENT_MENUPOPUP_START if ARIA menu appears.
      FireDelayedEvent(nsIAccessibleEvent::EVENT_MENUPOPUP_START, aChild);

    } else if (ariaRole == roles::ALERT) {
      // Fire EVENT_ALERT if ARIA alert appears.
      updateFlags = eAlertAccessible;
      FireDelayedEvent(nsIAccessibleEvent::EVENT_ALERT, aChild);
    }

    // If focused node has been shown then it means its frame was recreated
    // while it's focused. Fire focus event on new focused accessible. If
    // the queue contains focus event for this node then it's suppressed by
    // this one.
    // XXX: do we really want to send focus to focused DOM node not taking into
    // account active item?
    if (FocusMgr()->IsFocused(aChild))
      FocusMgr()->DispatchFocusEvent(this, aChild);

  } else {
    // Update the tree for content removal.
    // The accessible parent may differ from container accessible if
    // the parent doesn't have own DOM node like list accessible for HTML
    // selects.
    Accessible* parent = aChild->Parent();
    NS_ASSERTION(parent, "No accessible parent?!");
    if (parent)
      parent->RemoveChild(aChild);

    UncacheChildrenInSubtree(aChild);
  }

  return updateFlags;
}

void
DocAccessible::CacheChildrenInSubtree(Accessible* aRoot)
{
  aRoot->EnsureChildren();

  // Make sure we create accessible tree defined in DOM only, i.e. if accessible
  // provides specific tree (like XUL trees) then tree creation is handled by
  // this accessible.
  uint32_t count = aRoot->ContentChildCount();
  for (uint32_t idx = 0; idx < count; idx++) {
    Accessible* child = aRoot->ContentChildAt(idx);
    NS_ASSERTION(child, "Illicit tree change while tree is created!");
    // Don't cross document boundaries.
    if (child && child->IsContent())
      CacheChildrenInSubtree(child);
  }

  // Fire document load complete on ARIA documents.
  // XXX: we should delay an event if the ARIA document has aria-busy.
  if (aRoot->HasARIARole() && !aRoot->IsDoc()) {
    a11y::role role = aRoot->ARIARole();
    if (role == roles::DIALOG || role == roles::DOCUMENT)
      FireDelayedEvent(nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_COMPLETE, aRoot);
  }
}

void
DocAccessible::UncacheChildrenInSubtree(Accessible* aRoot)
{
  aRoot->mStateFlags |= eIsNotInDocument;

  if (aRoot->IsElement())
    RemoveDependentIDsFor(aRoot);

  uint32_t count = aRoot->ContentChildCount();
  for (uint32_t idx = 0; idx < count; idx++)
    UncacheChildrenInSubtree(aRoot->ContentChildAt(idx));

  if (aRoot->IsNodeMapEntry() &&
      mNodeToAccessibleMap.Get(aRoot->GetNode()) == aRoot)
    mNodeToAccessibleMap.Remove(aRoot->GetNode());
}

void
DocAccessible::ShutdownChildrenInSubtree(Accessible* aAccessible)
{
  // Traverse through children and shutdown them before this accessible. When
  // child gets shutdown then it removes itself from children array of its
  //parent. Use jdx index to process the cases if child is not attached to the
  // parent and as result doesn't remove itself from its children.
  uint32_t count = aAccessible->ContentChildCount();
  for (uint32_t idx = 0, jdx = 0; idx < count; idx++) {
    Accessible* child = aAccessible->ContentChildAt(jdx);
    if (!child->IsBoundToParent()) {
      NS_ERROR("Parent refers to a child, child doesn't refer to parent!");
      jdx++;
    }

    ShutdownChildrenInSubtree(child);
  }

  UnbindFromDocument(aAccessible);
}

bool
DocAccessible::IsLoadEventTarget() const
{
  nsCOMPtr<nsISupports> container = mDocumentNode->GetContainer();
  nsCOMPtr<nsIDocShellTreeItem> treeItem = do_QueryInterface(container);
  NS_ASSERTION(treeItem, "No document shell for document!");

  nsCOMPtr<nsIDocShellTreeItem> parentTreeItem;
  treeItem->GetParent(getter_AddRefs(parentTreeItem));

  // Not a root document.
  if (parentTreeItem) {
    // Return true if it's either:
    // a) tab document;
    nsCOMPtr<nsIDocShellTreeItem> rootTreeItem;
    treeItem->GetRootTreeItem(getter_AddRefs(rootTreeItem));
    if (parentTreeItem == rootTreeItem)
      return true;

    // b) frame/iframe document and its parent document is not in loading state
    // Note: we can get notifications while document is loading (and thus
    // while there's no parent document yet).
    DocAccessible* parentDoc = ParentDocument();
    return parentDoc && parentDoc->HasLoadState(eCompletelyLoaded);
  }

  // It's content (not chrome) root document.
  int32_t contentType;
  treeItem->GetItemType(&contentType);
  return (contentType == nsIDocShellTreeItem::typeContent);
}


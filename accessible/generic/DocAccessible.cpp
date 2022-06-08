/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LocalAccessible-inl.h"
#include "AccIterator.h"
#include "AccAttributes.h"
#include "CachedTableAccessible.h"
#include "DocAccessible-inl.h"
#include "DocAccessibleChild.h"
#include "HTMLImageMapAccessible.h"
#include "nsAccCache.h"
#include "nsAccessiblePivot.h"
#include "nsAccUtils.h"
#include "nsDeckFrame.h"
#include "nsEventShell.h"
#include "nsLayoutUtils.h"
#include "nsTextEquivUtils.h"
#include "Pivot.h"
#include "Role.h"
#include "RootAccessible.h"
#include "TreeWalker.h"
#include "xpcAccessibleDocument.h"

#include "nsCommandManager.h"
#include "nsContentUtils.h"
#include "nsIDocShell.h"
#include "mozilla/dom/Document.h"
#include "nsPIDOMWindow.h"
#include "nsIContentInlines.h"
#include "nsIEditingSession.h"
#include "nsIFrame.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsImageFrame.h"
#include "nsViewManager.h"
#include "nsIScrollableFrame.h"
#include "nsUnicharUtils.h"
#include "nsIURI.h"
#include "nsIWebNavigation.h"
#include "nsFocusManager.h"
#include "nsTHashSet.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/EditorBase.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_accessibility.h"
#include "mozilla/dom/AncestorIterator.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/DocumentType.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLSelectElement.h"
#include "mozilla/dom/MutationEventBinding.h"
#include "mozilla/dom/UserActivation.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// Static member initialization

static nsStaticAtom* const kRelationAttrs[] = {nsGkAtoms::aria_labelledby,
                                               nsGkAtoms::aria_describedby,
                                               nsGkAtoms::aria_details,
                                               nsGkAtoms::aria_owns,
                                               nsGkAtoms::aria_controls,
                                               nsGkAtoms::aria_flowto,
                                               nsGkAtoms::aria_errormessage,
                                               nsGkAtoms::_for,
                                               nsGkAtoms::control};

static const uint32_t kRelationAttrsLen = ArrayLength(kRelationAttrs);

////////////////////////////////////////////////////////////////////////////////
// Constructor/desctructor

DocAccessible::DocAccessible(dom::Document* aDocument,
                             PresShell* aPresShell)
    :  // XXX don't pass a document to the LocalAccessible constructor so that
       // we don't set mDoc until our vtable is fully setup.  If we set mDoc
       // before setting up the vtable we will call LocalAccessible::AddRef()
       // but not the overrides of it for subclasses.  It is important to call
       // those overrides to avoid confusing leak checking machinary.
      HyperTextAccessibleWrap(nullptr, nullptr),
      // XXX aaronl should we use an algorithm for the initial cache size?
      mAccessibleCache(kDefaultCacheLength),
      mNodeToAccessibleMap(kDefaultCacheLength),
      mDocumentNode(aDocument),
      mLoadState(eTreeConstructionPending),
      mDocFlags(0),
      mLoadEventType(0),
      mPrevStateBits(0),
      mVirtualCursor(nullptr),
      mPresShell(aPresShell),
      mIPCDoc(nullptr) {
  mGenericTypes |= eDocument;
  mStateFlags |= eNotNodeMapEntry;
  mDoc = this;

  MOZ_ASSERT(mPresShell, "should have been given a pres shell");
  mPresShell->SetDocAccessible(this);
}

DocAccessible::~DocAccessible() {
  NS_ASSERTION(!mPresShell, "LastRelease was never called!?!");
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_CYCLE_COLLECTION_CLASS(DocAccessible)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(DocAccessible,
                                                  LocalAccessible)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNotificationController)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mVirtualCursor)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChildDocuments)
  for (const auto& hashEntry : tmp->mDependentIDsHashes.Values()) {
    for (const auto& providers : hashEntry->Values()) {
      for (int32_t provIdx = providers->Length() - 1; provIdx >= 0; provIdx--) {
        NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(
            cb, "content of dependent ids hash entry of document accessible");

        const auto& provider = (*providers)[provIdx];
        cb.NoteXPCOMChild(provider->mContent);
      }
    }
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAccessibleCache)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAnchorJumpElm)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mInvalidationList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPendingUpdates)
  for (const auto& ar : tmp->mARIAOwnsHash.Values()) {
    for (uint32_t i = 0; i < ar->Length(); i++) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mARIAOwnsHash entry item");
      cb.NoteXPCOMChild(ar->ElementAt(i));
    }
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(DocAccessible, LocalAccessible)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mNotificationController)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mVirtualCursor)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mChildDocuments)
  tmp->mDependentIDsHashes.Clear();
  tmp->mNodeToAccessibleMap.Clear();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAccessibleCache)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAnchorJumpElm)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mInvalidationList)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPendingUpdates)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_REFERENCE
  tmp->mARIAOwnsHash.Clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DocAccessible)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentObserver)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIAccessiblePivotObserver)
NS_INTERFACE_MAP_END_INHERITING(HyperTextAccessible)

NS_IMPL_ADDREF_INHERITED(DocAccessible, HyperTextAccessible)
NS_IMPL_RELEASE_INHERITED(DocAccessible, HyperTextAccessible)

////////////////////////////////////////////////////////////////////////////////
// nsIAccessible

ENameValueFlag DocAccessible::Name(nsString& aName) const {
  aName.Truncate();

  if (mParent) {
    mParent->Name(aName);  // Allow owning iframe to override the name
  }
  if (aName.IsEmpty()) {
    // Allow name via aria-labelledby or title attribute
    LocalAccessible::Name(aName);
  }
  if (aName.IsEmpty()) {
    Title(aName);  // Try title element
  }
  if (aName.IsEmpty()) {  // Last resort: use URL
    URL(aName);
  }

  return eNameOK;
}

// LocalAccessible public method
role DocAccessible::NativeRole() const {
  nsCOMPtr<nsIDocShell> docShell = nsCoreUtils::GetDocShellFor(mDocumentNode);
  if (docShell) {
    nsCOMPtr<nsIDocShellTreeItem> sameTypeRoot;
    docShell->GetInProcessSameTypeRootTreeItem(getter_AddRefs(sameTypeRoot));
    int32_t itemType = docShell->ItemType();
    if (sameTypeRoot == docShell) {
      // Root of content or chrome tree
      if (itemType == nsIDocShellTreeItem::typeChrome) {
        return roles::CHROME_WINDOW;
      }

      if (itemType == nsIDocShellTreeItem::typeContent) {
        return roles::DOCUMENT;
      }
    } else if (itemType == nsIDocShellTreeItem::typeContent) {
      return roles::DOCUMENT;
    }
  }

  return roles::PANE;  // Fall back;
}

void DocAccessible::Description(nsString& aDescription) const {
  if (mParent) mParent->Description(aDescription);

  if (HasOwnContent() && aDescription.IsEmpty()) {
    nsTextEquivUtils::GetTextEquivFromIDRefs(this, nsGkAtoms::aria_describedby,
                                             aDescription);
  }
}

// LocalAccessible public method
uint64_t DocAccessible::NativeState() const {
  // Document is always focusable.
  uint64_t state =
      states::FOCUSABLE;  // keep in sync with NativeInteractiveState() impl
  if (FocusMgr()->IsFocused(this)) state |= states::FOCUSED;

  // Expose stale state until the document is ready (DOM is loaded and tree is
  // constructed).
  if (!HasLoadState(eReady)) state |= states::STALE;

  // Expose state busy until the document and all its subdocuments is completely
  // loaded.
  if (!HasLoadState(eCompletelyLoaded)) state |= states::BUSY;

  nsIFrame* frame = GetFrame();
  if (!frame || !frame->IsVisibleConsideringAncestors(
                    nsIFrame::VISIBILITY_CROSS_CHROME_CONTENT_BOUNDARY)) {
    state |= states::INVISIBLE | states::OFFSCREEN;
  }

  RefPtr<EditorBase> editorBase = GetEditor();
  state |= editorBase ? states::EDITABLE : states::READONLY;

  return state;
}

uint64_t DocAccessible::NativeInteractiveState() const {
  // Document is always focusable.
  return states::FOCUSABLE;
}

bool DocAccessible::NativelyUnavailable() const { return false; }

// LocalAccessible public method
void DocAccessible::ApplyARIAState(uint64_t* aState) const {
  // Grab states from content element.
  if (mContent) LocalAccessible::ApplyARIAState(aState);

  // Allow iframe/frame etc. to have final state override via ARIA.
  if (mParent) mParent->ApplyARIAState(aState);
}

already_AddRefed<AccAttributes> DocAccessible::Attributes() {
  RefPtr<AccAttributes> attributes = HyperTextAccessibleWrap::Attributes();

  // No attributes if document is not attached to the tree or if it's a root
  // document.
  if (!mParent || IsRoot()) return attributes.forget();

  // Override ARIA object attributes from outerdoc.
  aria::AttrIterator attribIter(mParent->GetContent());
  while (attribIter.Next()) {
    nsString value;
    attribIter.AttrValue(value);
    attributes->SetAttribute(attribIter.AttrName(), std::move(value));
  }

  return attributes.forget();
}

LocalAccessible* DocAccessible::FocusedChild() {
  // Return an accessible for the current global focus, which does not have to
  // be contained within the current document.
  return FocusMgr()->FocusedAccessible();
}

void DocAccessible::TakeFocus() const {
  // Focus the document.
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  RefPtr<dom::Element> newFocus;
  dom::AutoHandlingUserInputStatePusher inputStatePusher(true);
  fm->MoveFocus(mDocumentNode->GetWindow(), nullptr,
                nsFocusManager::MOVEFOCUS_ROOT, 0, getter_AddRefs(newFocus));
}

// HyperTextAccessible method
already_AddRefed<EditorBase> DocAccessible::GetEditor() const {
  // Check if document is editable (designMode="on" case). Otherwise check if
  // the html:body (for HTML document case) or document element is editable.
  if (!mDocumentNode->IsInDesignMode() &&
      (!mContent || !mContent->HasFlag(NODE_IS_EDITABLE))) {
    return nullptr;
  }

  nsCOMPtr<nsIDocShell> docShell = mDocumentNode->GetDocShell();
  if (!docShell) {
    return nullptr;
  }

  nsCOMPtr<nsIEditingSession> editingSession;
  docShell->GetEditingSession(getter_AddRefs(editingSession));
  if (!editingSession) return nullptr;  // No editing session interface

  RefPtr<HTMLEditor> htmlEditor =
      editingSession->GetHTMLEditorForWindow(mDocumentNode->GetWindow());
  if (!htmlEditor) {
    return nullptr;
  }

  bool isEditable = false;
  htmlEditor->GetIsDocumentEditable(&isEditable);
  if (isEditable) {
    return htmlEditor.forget();
  }

  return nullptr;
}

// DocAccessible public method

void DocAccessible::URL(nsAString& aURL) const {
  nsCOMPtr<nsISupports> container = mDocumentNode->GetContainer();
  nsCOMPtr<nsIWebNavigation> webNav(do_GetInterface(container));
  nsAutoCString theURL;
  if (webNav) {
    nsCOMPtr<nsIURI> pURI;
    webNav->GetCurrentURI(getter_AddRefs(pURI));
    if (pURI) pURI->GetSpec(theURL);
  }
  CopyUTF8toUTF16(theURL, aURL);
}

void DocAccessible::Title(nsString& aTitle) const {
  mDocumentNode->GetTitle(aTitle);
}

void DocAccessible::MimeType(nsAString& aType) const {
  mDocumentNode->GetContentType(aType);
}

void DocAccessible::DocType(nsAString& aType) const {
  dom::DocumentType* docType = mDocumentNode->GetDoctype();
  if (docType) docType->GetPublicId(aType);
}

void DocAccessible::QueueCacheUpdate(LocalAccessible* aAcc,
                                     uint64_t aNewDomain) {
  uint64_t& domain = mQueuedCacheUpdates.LookupOrInsert(aAcc, 0);
  domain |= aNewDomain;

  Controller()->ScheduleProcessing();
}

////////////////////////////////////////////////////////////////////////////////
// LocalAccessible

void DocAccessible::Init() {
#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eDocCreate)) {
    logging::DocCreate("document initialize", mDocumentNode, this);
  }
#endif

  // Initialize notification controller.
  mNotificationController = new NotificationController(this, mPresShell);

  // Mark the DocAccessible as loaded if its DOM document is already loaded at
  // this point. This can happen for one of three reasons:
  // 1. A11y was started late.
  // 2. DOM loading for a document (probably an in-process iframe) completed
  // before its Accessible container was created.
  // 3. The PresShell for the document was created after DOM loading completed.
  // In that case, we tried to create the DocAccessible when DOM loading
  // completed, but we can't create a DocAccessible without a PresShell, so
  // this failed. The DocAccessible was subsequently created due to a layout
  // notification.
  if (mDocumentNode->GetReadyStateEnum() ==
      dom::Document::READYSTATE_COMPLETE) {
    mLoadState |= eDOMLoaded;
    // If this happened due to reasons 1 or 2, it isn't *necessary* to fire a
    // doc load complete event. If it happened due to reason 3, we need to fire
    // doc load complete because clients (especially tests) might be waiting
    // for the document to load using this event. We can't distinguish why this
    // happened at this point, so just fire it regardless. It won't do any
    // harm even if it isn't necessary. We set mLoadEventType here and it will
    // be fired in ProcessLoad as usual.
    mLoadEventType = nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_COMPLETE;
  } else if (mDocumentNode->IsInitialDocument()) {
    // The initial about:blank document will never finish loading, so we can
    // immediately mark it loaded to avoid waiting for its load.
    mLoadState |= eDOMLoaded;
  }

  AddEventListeners();
}

void DocAccessible::Shutdown() {
  if (!mPresShell) {  // already shutdown
    return;
  }

#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eDocDestroy)) {
    logging::DocDestroy("document shutdown", mDocumentNode, this);
  }
#endif

  // Mark the document as shutdown before AT is notified about the document
  // removal from its container (valid for root documents on ATK and due to
  // some reason for MSAA, refer to bug 757392 for details).
  mStateFlags |= eIsDefunct;

  if (mNotificationController) {
    mNotificationController->Shutdown();
    mNotificationController = nullptr;
  }

  RemoveEventListeners();

  // mParent->RemoveChild clears mParent, but we need to know whether we were a
  // child later, so use a flag.
  const bool isChild = !!mParent;
  if (mParent) {
    DocAccessible* parentDocument = mParent->Document();
    if (parentDocument) parentDocument->RemoveChildDocument(this);

    mParent->RemoveChild(this);
    MOZ_ASSERT(!mParent, "Parent has to be null!");
  }

  mPresShell->SetDocAccessible(nullptr);
  mPresShell = nullptr;  // Avoid reentrancy

  // Walk the array backwards because child documents remove themselves from the
  // array as they are shutdown.
  int32_t childDocCount = mChildDocuments.Length();
  for (int32_t idx = childDocCount - 1; idx >= 0; idx--) {
    mChildDocuments[idx]->Shutdown();
  }

  mChildDocuments.Clear();
  // mQueuedCacheUpdates can contain a reference to this document (ex. if the
  // doc is scrollable and we're sending a scroll position update). Clear the
  // map here to avoid creating ref cycles.
  mQueuedCacheUpdates.Clear();

  // XXX thinking about ordering?
  if (mIPCDoc) {
    MOZ_ASSERT(IPCAccessibilityActive());
    mIPCDoc->Shutdown();
    MOZ_ASSERT(!mIPCDoc);
  }

  if (mVirtualCursor) {
    mVirtualCursor->RemoveObserver(this);
    mVirtualCursor = nullptr;
  }

  mDependentIDsHashes.Clear();
  mNodeToAccessibleMap.Clear();

  mAnchorJumpElm = nullptr;
  mInvalidationList.Clear();
  mPendingUpdates.Clear();

  for (auto iter = mAccessibleCache.Iter(); !iter.Done(); iter.Next()) {
    LocalAccessible* accessible = iter.Data();
    MOZ_ASSERT(accessible);
    if (accessible && !accessible->IsDefunct()) {
      // Unlink parent to avoid its cleaning overhead in shutdown.
      accessible->mParent = nullptr;
      accessible->Shutdown();
    }
    iter.Remove();
  }

  HyperTextAccessibleWrap::Shutdown();

  MOZ_ASSERT(GetAccService());
  GetAccService()->NotifyOfDocumentShutdown(
      this, mDocumentNode,
      // Make sure we don't shut down AccService while a parent document is
      // still shutting down. The parent will allow service shutdown when it
      // reaches this point.
      /* aAllowServiceShutdown */ !isChild);
  mDocumentNode = nullptr;
}

nsIFrame* DocAccessible::GetFrame() const {
  nsIFrame* root = nullptr;
  if (mPresShell) {
    root = mPresShell->GetRootFrame();
  }

  return root;
}

nsINode* DocAccessible::GetNode() const { return mDocumentNode; }

// DocAccessible protected member
nsRect DocAccessible::RelativeBounds(nsIFrame** aRelativeFrame) const {
  *aRelativeFrame = GetFrame();

  dom::Document* document = mDocumentNode;
  dom::Document* parentDoc = nullptr;

  nsRect bounds;
  while (document) {
    PresShell* presShell = document->GetPresShell();
    if (!presShell) {
      return nsRect();
    }

    nsRect scrollPort;
    nsIScrollableFrame* sf = presShell->GetRootScrollFrameAsScrollable();
    if (sf) {
      scrollPort = sf->GetScrollPortRect();
    } else {
      nsIFrame* rootFrame = presShell->GetRootFrame();
      if (!rootFrame) return nsRect();

      scrollPort = rootFrame->GetRect();
    }

    if (parentDoc) {  // After first time thru loop
      // XXXroc bogus code! scrollPort is relative to the viewport of
      // this document, but we're intersecting rectangles derived from
      // multiple documents and assuming they're all in the same coordinate
      // system. See bug 514117.
      bounds.IntersectRect(scrollPort, bounds);
    } else {  // First time through loop
      bounds = scrollPort;
    }

    document = parentDoc = document->GetInProcessParentDocument();
  }

  return bounds;
}

// DocAccessible protected member
nsresult DocAccessible::AddEventListeners() {
  SelectionMgr()->AddDocSelectionListener(mPresShell);

  // Add document observer.
  mDocumentNode->AddObserver(this);
  return NS_OK;
}

// DocAccessible protected member
nsresult DocAccessible::RemoveEventListeners() {
  // Remove listeners associated with content documents
  NS_ASSERTION(mDocumentNode, "No document during removal of listeners.");

  if (mDocumentNode) {
    mDocumentNode->RemoveObserver(this);
  }

  if (mScrollWatchTimer) {
    mScrollWatchTimer->Cancel();
    mScrollWatchTimer = nullptr;
    NS_RELEASE_THIS();  // Kung fu death grip
  }

  SelectionMgr()->RemoveDocSelectionListener(mPresShell);
  return NS_OK;
}

void DocAccessible::ScrollTimerCallback(nsITimer* aTimer, void* aClosure) {
  DocAccessible* docAcc = reinterpret_cast<DocAccessible*>(aClosure);

  if (docAcc) {
    // Dispatch a scroll-end for all entries in table. They have not
    // been scrolled in at least `kScrollEventInterval`.
    for (auto iter = docAcc->mLastScrollingDispatch.Iter(); !iter.Done();
         iter.Next()) {
      docAcc->DispatchScrollingEvent(iter.Key(),
                                     nsIAccessibleEvent::EVENT_SCROLLING_END);
      iter.Remove();
    }

    if (docAcc->mScrollWatchTimer) {
      docAcc->mScrollWatchTimer = nullptr;
      NS_RELEASE(docAcc);  // Release kung fu death grip
    }
  }
}

void DocAccessible::HandleScroll(nsINode* aTarget) {
  // Regardless of our scroll timer, we need to send a cache update
  // to ensure the next Bounds() query accurately reflects our position
  // after scrolling.
  if (LocalAccessible* scrollTarget = GetAccessible(aTarget)) {
    QueueCacheUpdate(scrollTarget, CacheDomain::ScrollPosition);
  }

  const uint32_t kScrollEventInterval = 100;
  // If we haven't dispatched a scrolling event for a target in at least
  // kScrollEventInterval milliseconds, dispatch one now.
  mLastScrollingDispatch.WithEntryHandle(aTarget, [&](auto&& lastDispatch) {
    const TimeStamp now = TimeStamp::Now();

    if (!lastDispatch ||
        (now - lastDispatch.Data()).ToMilliseconds() >= kScrollEventInterval) {
      // We can't fire events on a document whose tree isn't constructed yet.
      if (HasLoadState(eTreeConstructed)) {
        DispatchScrollingEvent(aTarget, nsIAccessibleEvent::EVENT_SCROLLING);
      }
      lastDispatch.InsertOrUpdate(now);
    }
  });

  // If timer callback is still pending, push it 100ms into the future.
  // When scrolling ends and we don't fire this callback anymore, the
  // timer callback will fire and dispatch an EVENT_SCROLLING_END.
  if (mScrollWatchTimer) {
    mScrollWatchTimer->SetDelay(kScrollEventInterval);
  } else {
    NS_NewTimerWithFuncCallback(getter_AddRefs(mScrollWatchTimer),
                                ScrollTimerCallback, this, kScrollEventInterval,
                                nsITimer::TYPE_ONE_SHOT,
                                "a11y::DocAccessible::ScrollPositionDidChange");
    if (mScrollWatchTimer) {
      NS_ADDREF_THIS();  // Kung fu death grip
    }
  }
}

std::pair<nsPoint, nsRect> DocAccessible::ComputeScrollData(
    LocalAccessible* aAcc) {
  nsPoint scrollPoint;
  nsRect scrollRange;

  if (nsIFrame* frame = aAcc->GetFrame()) {
    nsIScrollableFrame* sf = aAcc == this
                                 ? mPresShell->GetRootScrollFrameAsScrollable()
                                 : frame->GetScrollTargetFrame();

    // If there is no scrollable frame, it's likely a scroll in a popup, like
    // <select>. Return a scroll offset and range of 0. The scroll info
    // is currently only used on Android, and popups are rendered natively
    // there.
    if (sf) {
      scrollPoint = sf->GetScrollPosition() * mPresShell->GetResolution();
      scrollRange = sf->GetScrollRange();
      scrollRange.ScaleRoundOut(mPresShell->GetResolution());
    }
  }

  return {scrollPoint, scrollRange};
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessiblePivotObserver

NS_IMETHODIMP
DocAccessible::OnPivotChanged(nsIAccessiblePivot* aPivot,
                              nsIAccessible* aOldAccessible, int32_t aOldStart,
                              int32_t aOldEnd, nsIAccessible* aNewAccessible,
                              int32_t aNewStart, int32_t aNewEnd,
                              PivotMoveReason aReason,
                              TextBoundaryType aBoundaryType,
                              bool aIsFromUserInput) {
  RefPtr<AccEvent> event = new AccVCChangeEvent(
      this, (aOldAccessible ? aOldAccessible->ToInternalAccessible() : nullptr),
      aOldStart, aOldEnd,
      (aNewAccessible ? aNewAccessible->ToInternalAccessible() : nullptr),
      aNewStart, aNewEnd, aReason, aBoundaryType,
      aIsFromUserInput ? eFromUserInput : eNoUserInput);
  nsEventShell::FireEvent(event);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIDocumentObserver

NS_IMPL_NSIDOCUMENTOBSERVER_CORE_STUB(DocAccessible)
NS_IMPL_NSIDOCUMENTOBSERVER_LOAD_STUB(DocAccessible)

void DocAccessible::AttributeWillChange(dom::Element* aElement,
                                        int32_t aNameSpaceID,
                                        nsAtom* aAttribute, int32_t aModType) {
  LocalAccessible* accessible = GetAccessible(aElement);
  if (!accessible) {
    if (aElement != mContent) return;

    accessible = this;
  }

  // Update dependent IDs cache. Take care of elements that are accessible
  // because dependent IDs cache doesn't contain IDs from non accessible
  // elements.
  if (aModType != dom::MutationEvent_Binding::ADDITION) {
    RemoveDependentIDsFor(accessible, aAttribute);
  }

  if (aAttribute == nsGkAtoms::id) {
    if (accessible->IsActiveDescendant()) {
      RefPtr<AccEvent> event =
          new AccStateChangeEvent(accessible, states::ACTIVE, false);
      FireDelayedEvent(event);
    }

    RelocateARIAOwnedIfNeeded(aElement);
  }

  if (aAttribute == nsGkAtoms::aria_activedescendant) {
    if (LocalAccessible* activeDescendant = accessible->CurrentItem()) {
      RefPtr<AccEvent> event =
          new AccStateChangeEvent(activeDescendant, states::ACTIVE, false);
      FireDelayedEvent(event);
    }
  }

  // If attribute affects accessible's state, store the old state so we can
  // later compare it against the state of the accessible after the attribute
  // change.
  if (accessible->AttributeChangesState(aAttribute)) {
    mPrevStateBits = accessible->State();
  } else {
    mPrevStateBits = 0;
  }
}

void DocAccessible::NativeAnonymousChildListChange(nsIContent* aContent,
                                                   bool aIsRemove) {
  if (aIsRemove) {
#ifdef A11Y_LOG
    if (logging::IsEnabled(logging::eTree)) {
      logging::MsgBegin("TREE", "Anonymous content removed; doc: %p", this);
      logging::Node("node", aContent);
      logging::MsgEnd();
    }
#endif

    ContentRemoved(aContent);
  }
}

void DocAccessible::AttributeChanged(dom::Element* aElement,
                                     int32_t aNameSpaceID, nsAtom* aAttribute,
                                     int32_t aModType,
                                     const nsAttrValue* aOldValue) {
  NS_ASSERTION(!IsDefunct(),
               "Attribute changed called on defunct document accessible!");

  // Proceed even if the element is not accessible because element may become
  // accessible if it gets certain attribute.
  if (UpdateAccessibleOnAttrChange(aElement, aAttribute)) return;

  // Update the accessible tree on aria-hidden change. Make sure to not create
  // a tree under aria-hidden='true'.
  if (aAttribute == nsGkAtoms::aria_hidden) {
    if (aria::HasDefinedARIAHidden(aElement)) {
      ContentRemoved(aElement);
    } else {
      ContentInserted(aElement, aElement->GetNextSibling());
    }
    return;
  }

  // Ignore attribute change if the element doesn't have an accessible (at all
  // or still) if the element is not a root content of this document accessible
  // (which is treated as attribute change on this document accessible).
  // Note: we don't bail if all the content hasn't finished loading because
  // these attributes are changing for a loaded part of the content.
  LocalAccessible* accessible = GetAccessible(aElement);
  if (!accessible) {
    if (mContent != aElement) return;

    accessible = this;
  }

  MOZ_ASSERT(accessible->IsBoundToParent() || accessible->IsDoc(),
             "DOM attribute change on an accessible detached from the tree");

  if (aAttribute == nsGkAtoms::id) {
    dom::Element* elm = accessible->Elm();
    RelocateARIAOwnedIfNeeded(elm);
    ARIAActiveDescendantIDMaybeMoved(accessible);
    accessible->SendCache(CacheDomain::DOMNodeID, CacheUpdateType::Update);
  }

  // The activedescendant universal property redirects accessible focus events
  // to the element with the id that activedescendant points to. Make sure
  // the tree up to date before processing. In other words, when a node has just
  // been inserted, the tree won't be up to date yet, so we must always schedule
  // an async notification so that a newly inserted node will be present in
  // the tree.
  if (aAttribute == nsGkAtoms::aria_activedescendant) {
    mNotificationController
        ->ScheduleNotification<DocAccessible, LocalAccessible>(
            this, &DocAccessible::ARIAActiveDescendantChanged, accessible);
    return;
  }

  // Defer to accessible any needed actions like changing states or emiting
  // events.
  accessible->DOMAttributeChanged(aNameSpaceID, aAttribute, aModType, aOldValue,
                                  mPrevStateBits);

  // Update dependent IDs cache. We handle elements with accessibles.
  // If the accessible or element with the ID doesn't exist yet the cache will
  // be updated when they are added.
  if (aModType == dom::MutationEvent_Binding::MODIFICATION ||
      aModType == dom::MutationEvent_Binding::ADDITION) {
    AddDependentIDsFor(accessible, aAttribute);
  }
}

void DocAccessible::ARIAActiveDescendantChanged(LocalAccessible* aAccessible) {
  if (dom::Element* elm = aAccessible->Elm()) {
    nsAutoString id;
    if (elm->GetAttr(kNameSpaceID_None, nsGkAtoms::aria_activedescendant, id)) {
      dom::Element* activeDescendantElm = IDRefsIterator::GetElem(elm, id);
      if (activeDescendantElm) {
        LocalAccessible* activeDescendant = GetAccessible(activeDescendantElm);
        if (activeDescendant) {
          RefPtr<AccEvent> event =
              new AccStateChangeEvent(activeDescendant, states::ACTIVE, true);
          FireDelayedEvent(event);
          if (aAccessible->IsActiveWidget()) {
            FocusMgr()->ActiveItemChanged(activeDescendant, false);
#ifdef A11Y_LOG
            if (logging::IsEnabled(logging::eFocus)) {
              logging::ActiveItemChangeCausedBy("ARIA activedescedant changed",
                                                activeDescendant);
            }
#endif
          }
          return;
        }
      }
    }

    // aria-activedescendant was cleared or changed to a non-existent node.
    // Move focus back to the element itself.
    FocusMgr()->ActiveItemChanged(aAccessible, false);
#ifdef A11Y_LOG
    if (logging::IsEnabled(logging::eFocus)) {
      logging::ActiveItemChangeCausedBy("ARIA activedescedant cleared",
                                        aAccessible);
    }
#endif
  }
}

void DocAccessible::ContentAppended(nsIContent* aFirstNewContent) {
  MaybeHandleChangeToHiddenNameOrDescription(aFirstNewContent);
}

void DocAccessible::ElementStateChanged(dom::Document* aDocument,
                                        dom::Element* aElement,
                                        dom::ElementState aStateMask) {
  if (aStateMask.HasState(dom::ElementState::READWRITE) &&
      aElement == mDocumentNode->GetRootElement()) {
    // This handles changes to designMode. contentEditable is handled by
    // LocalAccessible::AttributeChangesState and
    // LocalAccessible::DOMAttributeChanged.
    const bool isEditable =
        aElement->State().HasState(dom::ElementState::READWRITE);
    RefPtr<AccEvent> event =
        new AccStateChangeEvent(this, states::EDITABLE, isEditable);
    FireDelayedEvent(event);
    event = new AccStateChangeEvent(this, states::READONLY, !isEditable);
    FireDelayedEvent(event);
  }

  LocalAccessible* accessible = GetAccessible(aElement);
  if (!accessible) return;

  if (aStateMask.HasState(dom::ElementState::CHECKED)) {
    LocalAccessible* widget = accessible->ContainerWidget();
    if (widget && widget->IsSelect()) {
      AccSelChangeEvent::SelChangeType selChangeType =
          aElement->State().HasState(dom::ElementState::CHECKED)
              ? AccSelChangeEvent::eSelectionAdd
              : AccSelChangeEvent::eSelectionRemove;
      RefPtr<AccEvent> event =
          new AccSelChangeEvent(widget, accessible, selChangeType);
      FireDelayedEvent(event);
      return;
    }

    RefPtr<AccEvent> event = new AccStateChangeEvent(
        accessible, states::CHECKED,
        aElement->State().HasState(dom::ElementState::CHECKED));
    FireDelayedEvent(event);
  }

  if (aStateMask.HasState(dom::ElementState::INVALID)) {
    RefPtr<AccEvent> event =
        new AccStateChangeEvent(accessible, states::INVALID, true);
    FireDelayedEvent(event);
  }

  if (aStateMask.HasState(dom::ElementState::REQUIRED)) {
    RefPtr<AccEvent> event =
        new AccStateChangeEvent(accessible, states::REQUIRED);
    FireDelayedEvent(event);
  }

  if (aStateMask.HasState(dom::ElementState::VISITED)) {
    RefPtr<AccEvent> event =
        new AccStateChangeEvent(accessible, states::TRAVERSED, true);
    FireDelayedEvent(event);
  }

  // We only expose dom::ElementState::DEFAULT on buttons, but we can get
  // notifications for other controls like checkboxes.
  if (aStateMask.HasState(dom::ElementState::DEFAULT) &&
      accessible->IsButton()) {
    RefPtr<AccEvent> event =
        new AccStateChangeEvent(accessible, states::DEFAULT);
    FireDelayedEvent(event);
  }
}

void DocAccessible::CharacterDataWillChange(nsIContent* aContent,
                                            const CharacterDataChangeInfo&) {}

void DocAccessible::CharacterDataChanged(nsIContent* aContent,
                                         const CharacterDataChangeInfo&) {}

void DocAccessible::ContentInserted(nsIContent* aChild) {
  MaybeHandleChangeToHiddenNameOrDescription(aChild);
}

void DocAccessible::ContentRemoved(nsIContent* aChildNode,
                                   nsIContent* aPreviousSiblingNode) {
#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eTree)) {
    logging::MsgBegin("TREE", "DOM content removed; doc: %p", this);
    logging::Node("container node", aChildNode->GetParent());
    logging::Node("content node", aChildNode);
    logging::MsgEnd();
  }
#endif
  // This one and content removal notification from layout may result in
  // double processing of same subtrees. If it pops up in profiling, then
  // consider reusing a document node cache to reject these notifications early.
  ContentRemoved(aChildNode);
}

void DocAccessible::ParentChainChanged(nsIContent* aContent) {}

////////////////////////////////////////////////////////////////////////////////
// LocalAccessible

#ifdef A11Y_LOG
nsresult DocAccessible::HandleAccEvent(AccEvent* aEvent) {
  if (logging::IsEnabled(logging::eDocLoad)) {
    logging::DocLoadEventHandled(aEvent);
  }

  return HyperTextAccessible::HandleAccEvent(aEvent);
}
#endif

////////////////////////////////////////////////////////////////////////////////
// Public members

nsPresContext* DocAccessible::PresContext() const {
  return mPresShell->GetPresContext();
}

void* DocAccessible::GetNativeWindow() const {
  if (!mPresShell) {
    return nullptr;
  }

  nsViewManager* vm = mPresShell->GetViewManager();
  if (!vm) return nullptr;

  nsCOMPtr<nsIWidget> widget = vm->GetRootWidget();
  if (widget) return widget->GetNativeData(NS_NATIVE_WINDOW);

  return nullptr;
}

LocalAccessible* DocAccessible::GetAccessibleByUniqueIDInSubtree(
    void* aUniqueID) {
  LocalAccessible* child = GetAccessibleByUniqueID(aUniqueID);
  if (child) return child;

  uint32_t childDocCount = mChildDocuments.Length();
  for (uint32_t childDocIdx = 0; childDocIdx < childDocCount; childDocIdx++) {
    DocAccessible* childDocument = mChildDocuments.ElementAt(childDocIdx);
    child = childDocument->GetAccessibleByUniqueIDInSubtree(aUniqueID);
    if (child) return child;
  }

  return nullptr;
}

LocalAccessible* DocAccessible::GetAccessibleOrContainer(
    nsINode* aNode, bool aNoContainerIfPruned) const {
  if (!aNode || !aNode->GetComposedDoc()) {
    return nullptr;
  }

  nsINode* start = aNode;
  if (auto* shadowRoot = dom::ShadowRoot::FromNode(aNode)) {
    // This can happen, for example, when called within
    // SelectionManager::ProcessSelectionChanged due to focusing a direct
    // child of a shadow root.
    // GetFlattenedTreeParent works on children of a shadow root, but not the
    // shadow root itself.
    start = shadowRoot->GetHost();
    if (!start) {
      return nullptr;
    }
  }

  for (nsINode* currNode : dom::InclusiveFlatTreeAncestors(*start)) {
    // No container if is inside of aria-hidden subtree.
    if (aNoContainerIfPruned && currNode->IsElement() &&
        aria::HasDefinedARIAHidden(currNode->AsElement())) {
      return nullptr;
    }

    // Check if node is in an unselected deck panel
    if (aNoContainerIfPruned && currNode->IsXULElement()) {
      if (nsIFrame* frame = currNode->AsContent()->GetPrimaryFrame()) {
        nsDeckFrame* deckFrame = do_QueryFrame(frame->GetParent());
        if (deckFrame && deckFrame->GetSelectedBox() != frame) {
          // If deck is not a <tabpanels>, return null
          nsIContent* parentFrameContent = deckFrame->GetContent();
          if (!parentFrameContent ||
              !parentFrameContent->IsXULElement(nsGkAtoms::tabpanels)) {
            return nullptr;
          }
        }
      }
    }

    // Check if node is in zero-sized map
    if (aNoContainerIfPruned && currNode->IsHTMLElement(nsGkAtoms::map)) {
      if (nsIFrame* frame = currNode->AsContent()->GetPrimaryFrame()) {
        if (nsLayoutUtils::GetAllInFlowRectsUnion(frame, frame->GetParent())
                .IsEmpty()) {
          return nullptr;
        }
      }
    }

    if (LocalAccessible* accessible = GetAccessible(currNode)) {
      return accessible;
    }
  }

  return nullptr;
}

LocalAccessible* DocAccessible::GetContainerAccessible(nsINode* aNode) const {
  return aNode ? GetAccessibleOrContainer(aNode->GetFlattenedTreeParentNode())
               : nullptr;
}

LocalAccessible* DocAccessible::GetAccessibleOrDescendant(
    nsINode* aNode) const {
  LocalAccessible* acc = GetAccessible(aNode);
  if (acc) return acc;

  if (aNode == mContent || aNode == mDocumentNode->GetRootElement()) {
    // If the node is the doc's body or root element, return the doc accessible.
    return const_cast<DocAccessible*>(this);
  }

  acc = GetContainerAccessible(aNode);
  if (acc) {
    TreeWalker walker(acc, aNode->AsContent(),
                      TreeWalker::eWalkCache | TreeWalker::eScoped);
    return walker.Next();
  }

  return nullptr;
}

void DocAccessible::BindToDocument(LocalAccessible* aAccessible,
                                   const nsRoleMapEntry* aRoleMapEntry) {
  // Put into DOM node cache.
  if (aAccessible->IsNodeMapEntry()) {
    mNodeToAccessibleMap.InsertOrUpdate(aAccessible->GetNode(), aAccessible);
  }

  // Put into unique ID cache.
  mAccessibleCache.InsertOrUpdate(aAccessible->UniqueID(), RefPtr{aAccessible});

  aAccessible->SetRoleMapEntry(aRoleMapEntry);

  if (aAccessible->HasOwnContent()) {
    AddDependentIDsFor(aAccessible);

    nsIContent* content = aAccessible->GetContent();
    if (content->IsElement() && content->AsElement()->HasAttr(
                                    kNameSpaceID_None, nsGkAtoms::aria_owns)) {
      mNotificationController->ScheduleRelocation(aAccessible);
    }
  }

  if (mIPCDoc) {
    mInsertedAccessibles.EnsureInserted(aAccessible);
  }
}

void DocAccessible::UnbindFromDocument(LocalAccessible* aAccessible) {
  NS_ASSERTION(mAccessibleCache.GetWeak(aAccessible->UniqueID()),
               "Unbinding the unbound accessible!");

  // Fire focus event on accessible having DOM focus if last focus was removed
  // from the tree.
  if (FocusMgr()->WasLastFocused(aAccessible)) {
    FocusMgr()->ActiveItemChanged(nullptr);
#ifdef A11Y_LOG
    if (logging::IsEnabled(logging::eFocus)) {
      logging::ActiveItemChangeCausedBy("tree shutdown", aAccessible);
    }
#endif
  }

  // Remove an accessible from node-to-accessible map if it exists there.
  if (aAccessible->IsNodeMapEntry() &&
      mNodeToAccessibleMap.Get(aAccessible->GetNode()) == aAccessible) {
    mNodeToAccessibleMap.Remove(aAccessible->GetNode());
  }

  aAccessible->mStateFlags |= eIsNotInDocument;

  // Update XPCOM part.
  xpcAccessibleDocument* xpcDoc = GetAccService()->GetCachedXPCDocument(this);
  if (xpcDoc) xpcDoc->NotifyOfShutdown(aAccessible);

  void* uniqueID = aAccessible->UniqueID();

  NS_ASSERTION(!aAccessible->IsDefunct(), "Shutdown the shutdown accessible!");
  aAccessible->Shutdown();

  mAccessibleCache.Remove(uniqueID);
}

void DocAccessible::ContentInserted(nsIContent* aStartChildNode,
                                    nsIContent* aEndChildNode) {
  // Ignore content insertions until we constructed accessible tree. Otherwise
  // schedule tree update on content insertion after layout.
  if (!mNotificationController || !HasLoadState(eTreeConstructed)) {
    return;
  }

  // The frame constructor guarantees that only ranges with the same parent
  // arrive here in presence of dynamic changes to the page, see
  // nsCSSFrameConstructor::IssueSingleInsertNotifications' callers.
  nsINode* parent = aStartChildNode->GetFlattenedTreeParentNode();
  if (!parent) {
    return;
  }

  LocalAccessible* container = AccessibleOrTrueContainer(parent);
  if (!container) {
    return;
  }

  AutoTArray<nsCOMPtr<nsIContent>, 10> list;
  for (nsIContent* node = aStartChildNode; node != aEndChildNode;
       node = node->GetNextSibling()) {
    MOZ_ASSERT(parent == node->GetFlattenedTreeParentNode());
    if (PruneOrInsertSubtree(node)) {
      list.AppendElement(node);
    }
  }

  mNotificationController->ScheduleContentInsertion(container, list);
}

void DocAccessible::ScheduleTreeUpdate(nsIContent* aContent) {
  if (mPendingUpdates.Contains(aContent)) {
    return;
  }
  mPendingUpdates.AppendElement(aContent);
  mNotificationController->ScheduleProcessing();
}

void DocAccessible::ProcessPendingUpdates() {
  auto updates = std::move(mPendingUpdates);
  for (auto update : updates) {
    if (update->GetComposedDoc() != mDocumentNode) {
      continue;
    }
    // The pruning logic will take care of avoiding unnecessary notifications.
    ContentInserted(update, update->GetNextSibling());
  }
}

bool DocAccessible::PruneOrInsertSubtree(nsIContent* aRoot) {
  bool insert = false;

  // In the case that we are, or are in, a shadow host, we need to assure
  // some accessibles are removed if they are not rendered anymore.
  nsIContent* shadowHost =
      aRoot->GetShadowRoot() ? aRoot : aRoot->GetContainingShadowHost();
  if (shadowHost) {
    // Check all explicit children in the host, if they are not slotted
    // then remove their accessibles and subtrees.
    for (nsIContent* childNode = shadowHost->GetFirstChild(); childNode;
         childNode = childNode->GetNextSibling()) {
      if (!childNode->GetPrimaryFrame() &&
          !nsCoreUtils::CanCreateAccessibleWithoutFrame(childNode)) {
        ContentRemoved(childNode);
      }
    }

    // If this is a slot, check to see if its fallback content is rendered,
    // if not - remove it.
    if (aRoot->IsHTMLElement(nsGkAtoms::slot)) {
      for (nsIContent* childNode = aRoot->GetFirstChild(); childNode;
           childNode = childNode->GetNextSibling()) {
        if (!childNode->GetPrimaryFrame() &&
            !nsCoreUtils::CanCreateAccessibleWithoutFrame(childNode)) {
          ContentRemoved(childNode);
        }
      }
    }
  }

  // If we already have an accessible, check if we need to remove it, recreate
  // it, or keep it in place.
  LocalAccessible* acc = GetAccessible(aRoot);
  if (acc) {
    MOZ_ASSERT(aRoot == acc->GetContent(),
               "LocalAccessible has differing content!");
#ifdef A11Y_LOG
    if (logging::IsEnabled(logging::eTree)) {
      logging::MsgBegin(
          "TREE", "inserted content already has accessible; doc: %p", this);
      logging::Node("content node", aRoot);
      logging::AccessibleInfo("accessible node", acc);
      logging::MsgEnd();
    }
#endif

    nsIFrame* frame = acc->GetFrame();
    if (frame) {
      acc->MaybeQueueCacheUpdateForStyleChanges();
    }

    // LocalAccessible has no frame and it's not display:contents. Remove it.
    // As well as removing the a11y subtree, we must also remove Accessibles
    // for DOM descendants, since some of these might be relocated Accessibles
    // and their DOM nodes are now hidden as well.
    if (!frame && !nsCoreUtils::CanCreateAccessibleWithoutFrame(aRoot)) {
      ContentRemoved(aRoot);
      return false;
    }

    // If it's a XULLabel it was probably reframed because a `value` attribute
    // was added. The accessible creates its text leaf upon construction, so we
    // need to recreate. Remove it, and schedule for reconstruction.
    if (acc->IsXULLabel()) {
      ContentRemoved(acc);
      return true;
    }

    // It is a broken image that is being reframed because it either got
    // or lost an `alt` tag that would rerender this node as text.
    if (frame && (acc->IsImage() != (frame->AccessibleType() == eImageType))) {
      ContentRemoved(aRoot);
      return true;
    }

    // If the frame is an OuterDoc frame but this isn't an OuterDocAccessible,
    // we need to recreate the LocalAccessible. This can happen for embed or
    // object elements if their embedded content changes to be web content.
    if (frame && !acc->IsOuterDoc() &&
        frame->AccessibleType() == eOuterDocType) {
      ContentRemoved(aRoot);
      return true;
    }

    // If the content is focused, and is being re-framed, reset the selection
    // listener for the node because the previous selection listener is on the
    // old frame.
    if (aRoot->IsElement() && FocusMgr()->HasDOMFocus(aRoot)) {
      SelectionMgr()->SetControlSelectionListener(aRoot->AsElement());
    }

    // If the accessible is a table, or table part, its layout table
    // status may have changed. We need to invalidate the associated
    // cache, which listens for the following event.
    if (acc->IsTable() || acc->IsTableRow() || acc->IsTableCell()) {
      FireDelayedEvent(nsIAccessibleEvent::EVENT_TABLE_STYLING_CHANGED, acc);
      LocalAccessible* table;
      if (acc->IsTable()) {
        table = acc;
      } else {
        for (table = acc->LocalParent(); table; table = table->LocalParent()) {
          if (table->IsTable()) {
            break;
          }
        }
      }
      if (table && table->IsTable()) {
        QueueCacheUpdate(acc, CacheDomain::Table);
      }
    }

    // The accessible can be reparented or reordered in its parent.
    // We schedule it for reinsertion. For example, a slotted element
    // can change its slot attribute to a different slot.
    insert = true;

    // If the frame is invisible, remove it.
    // Normally, layout sends explicit a11y notifications for visibility
    // changes (see SendA11yNotifications in RestyleManager). However, if a
    // visibility change also reconstructs the frame, we must handle it here.
    if (frame && !frame->StyleVisibility()->IsVisible()) {
      ContentRemoved(aRoot);
      // There might be visible descendants, so we want to walk the subtree.
      // However, we know we don't want to reinsert this node, so we set insert
      // to false.
      insert = false;
    }
  } else {
    // If there is no current accessible, and the node has a frame, or is
    // display:contents, schedule it for insertion.
    if (aRoot->GetPrimaryFrame() ||
        nsCoreUtils::CanCreateAccessibleWithoutFrame(aRoot)) {
      // This may be a new subtree, the insertion process will recurse through
      // its descendants.
      if (!GetAccessibleOrDescendant(aRoot)) {
        return true;
      }

      // Content is not an accessible, but has accessible descendants.
      // We schedule this container for insertion strictly for the case where it
      // itself now needs an accessible. We will still need to recurse into the
      // descendant content to prune accessibles, and in all likelyness to
      // insert accessibles since accessible insertions will likeley get missed
      // in an existing subtree.
      insert = true;
    }
  }

  if (LocalAccessible* container = AccessibleOrTrueContainer(aRoot)) {
    AutoTArray<nsCOMPtr<nsIContent>, 10> list;
    dom::AllChildrenIterator iter =
        dom::AllChildrenIterator(aRoot, nsIContent::eAllChildren, true);
    while (nsIContent* childNode = iter.GetNextChild()) {
      if (PruneOrInsertSubtree(childNode)) {
        list.AppendElement(childNode);
      }
    }

    if (!list.IsEmpty()) {
      mNotificationController->ScheduleContentInsertion(container, list);
    }
  }

  return insert;
}

void DocAccessible::RecreateAccessible(nsIContent* aContent) {
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
  ContentRemoved(aContent);
  ContentInserted(aContent, aContent->GetNextSibling());
}

void DocAccessible::ProcessInvalidationList() {
  // Invalidate children of container accessible for each element in
  // invalidation list. Allow invalidation list insertions while container
  // children are recached.
  for (uint32_t idx = 0; idx < mInvalidationList.Length(); idx++) {
    nsIContent* content = mInvalidationList[idx];
    if (!HasAccessible(content) && content->HasID()) {
      LocalAccessible* container = GetContainerAccessible(content);
      if (container) {
        // Check if the node is a target of aria-owns, and if so, don't process
        // it here and let DoARIAOwnsRelocation process it.
        AttrRelProviders* list = GetRelProviders(
            content->AsElement(), nsDependentAtomString(content->GetID()));
        bool shouldProcess = !!list;
        if (shouldProcess) {
          for (uint32_t idx = 0; idx < list->Length(); idx++) {
            if (list->ElementAt(idx)->mRelAttr == nsGkAtoms::aria_owns) {
              shouldProcess = false;
              break;
            }
          }

          if (shouldProcess) {
            ProcessContentInserted(container, content);
          }
        }
      }
    }
  }

  mInvalidationList.Clear();
}

void DocAccessible::ProcessQueuedCacheUpdates() {
  if (!StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return;
  }

  nsTArray<CacheData> data;
  for (auto iter = mQueuedCacheUpdates.Iter(); !iter.Done(); iter.Next()) {
    LocalAccessible* acc = iter.Key();
    uint64_t domain = iter.UserData();
    if (acc->IsInDocument() && !acc->IsDefunct()) {
      RefPtr<AccAttributes> fields =
          acc->BundleFieldsForCache(domain, CacheUpdateType::Update);

      if (fields->Count()) {
        data.AppendElement(CacheData(
            acc->IsDoc() ? 0 : reinterpret_cast<uint64_t>(acc->UniqueID()),
            fields));
      }
    }
  }

  mQueuedCacheUpdates.Clear();

  if (data.Length()) {
    IPCDoc()->SendCache(CacheUpdateType::Update, data, true);
  }
}

void DocAccessible::SendAccessiblesWillMove() {
  if (!mIPCDoc) {
    return;
  }
  nsTArray<uint64_t> ids;
  for (LocalAccessible* acc : mMovedAccessibles) {
    // If acc is defunct or not in a document, it was removed after it was
    // moved.
    if (!acc->IsDefunct() && acc->IsInDocument()) {
      ids.AppendElement(reinterpret_cast<uintptr_t>(acc->UniqueID()));
    }
  }
  if (!ids.IsEmpty()) {
    mIPCDoc->SendAccessiblesWillMove(ids);
  }
}

LocalAccessible* DocAccessible::GetAccessibleEvenIfNotInMap(
    nsINode* aNode) const {
  if (!aNode->IsContent() ||
      !aNode->AsContent()->IsHTMLElement(nsGkAtoms::area)) {
    return GetAccessible(aNode);
  }

  // XXX Bug 135040, incorrect when multiple images use the same map.
  nsIFrame* frame = aNode->AsContent()->GetPrimaryFrame();
  nsImageFrame* imageFrame = do_QueryFrame(frame);
  if (imageFrame) {
    LocalAccessible* parent = GetAccessible(imageFrame->GetContent());
    if (parent) {
      LocalAccessible* area =
          parent->AsImageMap()->GetChildAccessibleFor(aNode);
      if (area) return area;

      return nullptr;
    }
  }

  return GetAccessible(aNode);
}

////////////////////////////////////////////////////////////////////////////////
// Protected members

void DocAccessible::NotifyOfLoading(bool aIsReloading) {
  // Mark the document accessible as loading, if it stays alive then we'll mark
  // it as loaded when we receive proper notification.
  mLoadState &= ~eDOMLoaded;

  if (!IsLoadEventTarget()) return;

  if (aIsReloading && !mLoadEventType &&
      // We can't fire events on a document whose tree isn't constructed yet.
      HasLoadState(eTreeConstructed)) {
    // Fire reload and state busy events on existing document accessible while
    // event from user input flag can be calculated properly and accessible
    // is alive. When new document gets loaded then this one is destroyed.
    RefPtr<AccEvent> reloadEvent =
        new AccEvent(nsIAccessibleEvent::EVENT_DOCUMENT_RELOAD, this);
    nsEventShell::FireEvent(reloadEvent);
  }

  // Fire state busy change event. Use delayed event since we don't care
  // actually if event isn't delivered when the document goes away like a shot.
  RefPtr<AccEvent> stateEvent =
      new AccStateChangeEvent(this, states::BUSY, true);
  FireDelayedEvent(stateEvent);
}

void DocAccessible::DoInitialUpdate() {
  if (nsCoreUtils::IsTopLevelContentDocInProcess(mDocumentNode)) {
    mDocFlags |= eTopLevelContentDocInProcess;
    if (IPCAccessibilityActive()) {
      nsIDocShell* docShell = mDocumentNode->GetDocShell();
      if (RefPtr<dom::BrowserChild> browserChild =
              dom::BrowserChild::GetFrom(docShell)) {
        // In content processes, top level content documents are always
        // RootAccessibles.
        MOZ_ASSERT(IsRoot());
        DocAccessibleChild* ipcDoc = IPCDoc();
        if (ipcDoc) {
          browserChild->SetTopLevelDocAccessibleChild(ipcDoc);
        } else {
          ipcDoc = new DocAccessibleChild(this, browserChild);
          SetIPCDoc(ipcDoc);
          // Subsequent initialization might depend on being able to get the
          // top level DocAccessibleChild, so set that as early as possible.
          browserChild->SetTopLevelDocAccessibleChild(ipcDoc);

#if defined(XP_WIN)
          IAccessibleHolder holder;
          int32_t childID;
          if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
            childID = 0;
          } else {
            holder = CreateHolderFromAccessible(WrapNotNull(this));
            MOZ_ASSERT(!holder.IsNull());
            childID = MsaaAccessible::GetChildIDFor(this);
          }
#else
          int32_t holder = 0, childID = 0;
#endif
          browserChild->SendPDocAccessibleConstructor(
              ipcDoc, nullptr, 0, mDocumentNode->GetBrowsingContext(), childID,
              holder);
#if !defined(XP_WIN)
          ipcDoc->SendPDocAccessiblePlatformExtConstructor();
#endif
        }
#if !defined(XP_WIN)
        // It's safe for us to mark top level documents as constructed in the
        // parent process without receiving an explicit message, since we can
        // never get queries for this document or descendants before parent
        // process construction is complete.
        ipcDoc->SetConstructedInParentProcess();
#endif
      }
    }
  }

  mLoadState |= eTreeConstructed;

  // Set up a root element and ARIA role mapping.
  UpdateRootElIfNeeded();

  // Build initial tree.
  CacheChildrenInSubtree(this);
#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eVerbose)) {
    logging::Tree("TREE", "Initial subtree", this);
  }
  if (logging::IsEnabled(logging::eTreeSize)) {
    logging::TreeSize("TREE SIZE", "Initial subtree", this);
  }
#endif

  // Fire reorder event after the document tree is constructed. Note, since
  // this reorder event is processed by parent document then events targeted to
  // this document may be fired prior to this reorder event. If this is
  // a problem then consider to keep event processing per tab document.
  if (!IsRoot()) {
    RefPtr<AccReorderEvent> reorderEvent = new AccReorderEvent(LocalParent());
    ParentDocument()->FireDelayedEvent(reorderEvent);
  }

  if (IPCAccessibilityActive()) {
    DocAccessibleChild* ipcDoc = IPCDoc();
    MOZ_ASSERT(ipcDoc);
    if (ipcDoc) {
      if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
        // If we're caching, we should send an initial update for this document
        // and its attributes. Each acc contained in this doc will have its
        // initial update sent in `InsertIntoIpcTree`.
        SendCache(CacheDomain::All, CacheUpdateType::Initial);
      }

      for (auto idx = 0U; idx < mChildren.Length(); idx++) {
        ipcDoc->InsertIntoIpcTree(this, mChildren.ElementAt(idx), idx, true);
      }
    }
  }
}

void DocAccessible::ProcessLoad() {
  mLoadState |= eCompletelyLoaded;

#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eDocLoad)) {
    logging::DocCompleteLoad(this, IsLoadEventTarget());
  }
#endif

  // Do not fire document complete/stop events for root chrome document
  // accessibles and for frame/iframe documents because
  // a) screen readers start working on focus event in the case of root chrome
  // documents
  // b) document load event on sub documents causes screen readers to act is if
  // entire page is reloaded.
  if (!IsLoadEventTarget()) return;

  // Fire complete/load stopped if the load event type is given.
  if (mLoadEventType) {
    RefPtr<AccEvent> loadEvent = new AccEvent(mLoadEventType, this);
    FireDelayedEvent(loadEvent);

    mLoadEventType = 0;
  }

  // Fire busy state change event.
  RefPtr<AccEvent> stateEvent =
      new AccStateChangeEvent(this, states::BUSY, false);
  FireDelayedEvent(stateEvent);
}

void DocAccessible::AddDependentIDsFor(LocalAccessible* aRelProvider,
                                       nsAtom* aRelAttr) {
  dom::Element* relProviderEl = aRelProvider->Elm();
  if (!relProviderEl) return;

  for (uint32_t idx = 0; idx < kRelationAttrsLen; idx++) {
    nsStaticAtom* relAttr = kRelationAttrs[idx];
    if (aRelAttr && aRelAttr != relAttr) continue;

    if (relAttr == nsGkAtoms::_for) {
      if (!relProviderEl->IsAnyOfHTMLElements(nsGkAtoms::label,
                                              nsGkAtoms::output)) {
        continue;
      }

    } else if (relAttr == nsGkAtoms::control) {
      if (!relProviderEl->IsAnyOfXULElements(nsGkAtoms::label,
                                             nsGkAtoms::description)) {
        continue;
      }
    }

    IDRefsIterator iter(this, relProviderEl, relAttr);
    while (true) {
      const nsDependentSubstring id = iter.NextID();
      if (id.IsEmpty()) break;

      AttrRelProviders* providers = GetOrCreateRelProviders(relProviderEl, id);
      if (providers) {
        AttrRelProvider* provider = new AttrRelProvider(relAttr, relProviderEl);
        if (provider) {
          providers->AppendElement(provider);

          // We've got here during the children caching. If the referenced
          // content is not accessible then store it to pend its container
          // children invalidation (this happens immediately after the caching
          // is finished).
          nsIContent* dependentContent = iter.GetElem(id);
          if (dependentContent) {
            if (!HasAccessible(dependentContent)) {
              mInvalidationList.AppendElement(dependentContent);
            }
          }
        }
      }
    }

    // If the relation attribute is given then we don't have anything else to
    // check.
    if (aRelAttr) break;
  }

  // Make sure to schedule the tree update if needed.
  mNotificationController->ScheduleProcessing();
}

void DocAccessible::RemoveDependentIDsFor(LocalAccessible* aRelProvider,
                                          nsAtom* aRelAttr) {
  dom::Element* relProviderElm = aRelProvider->Elm();
  if (!relProviderElm) return;

  for (uint32_t idx = 0; idx < kRelationAttrsLen; idx++) {
    nsStaticAtom* relAttr = kRelationAttrs[idx];
    if (aRelAttr && aRelAttr != kRelationAttrs[idx]) continue;

    IDRefsIterator iter(this, relProviderElm, relAttr);
    while (true) {
      const nsDependentSubstring id = iter.NextID();
      if (id.IsEmpty()) break;

      AttrRelProviders* providers = GetRelProviders(relProviderElm, id);
      if (providers) {
        providers->RemoveElementsBy(
            [relAttr, relProviderElm](const auto& provider) {
              return provider->mRelAttr == relAttr &&
                     provider->mContent == relProviderElm;
            });

        RemoveRelProvidersIfEmpty(relProviderElm, id);
      }
    }

    // If the relation attribute is given then we don't have anything else to
    // check.
    if (aRelAttr) break;
  }
}

bool DocAccessible::UpdateAccessibleOnAttrChange(dom::Element* aElement,
                                                 nsAtom* aAttribute) {
  if (aAttribute == nsGkAtoms::role) {
    // It is common for js libraries to set the role on the body element after
    // the document has loaded. In this case we just update the role map entry.
    if (mContent == aElement) {
      SetRoleMapEntryForDoc(aElement);
      if (mIPCDoc) {
        mIPCDoc->SendRoleChangedEvent(Role());
      }

      return true;
    }

    // Recreate the accessible when role is changed because we might require a
    // different accessible class for the new role or the accessible may expose
    // a different sets of interfaces (COM restriction).
    RecreateAccessible(aElement);

    return true;
  }

  if (aAttribute == nsGkAtoms::multiple) {
    if (dom::HTMLSelectElement* select =
            dom::HTMLSelectElement::FromNode(aElement)) {
      if (select->Size() <= 1) {
        // Adding the 'multiple' attribute to a select that has a size of 1
        // creates a listbox as opposed to a combobox with a popup combobox
        // list. Removing the attribute does the opposite.
        RecreateAccessible(aElement);
        return true;
      }
    }
  }

  if (aAttribute == nsGkAtoms::size &&
      aElement->IsHTMLElement(nsGkAtoms::select)) {
    // Changing the size of a select element can potentially change it from a
    // combobox button to a listbox with different underlying implementations.
    RecreateAccessible(aElement);
    return true;
  }

  if (aAttribute == nsGkAtoms::type) {
    // If the input[type] changes, we should recreate the accessible.
    RecreateAccessible(aElement);
    return true;
  }

  return false;
}

void DocAccessible::UpdateRootElIfNeeded() {
  dom::Element* rootEl = mDocumentNode->GetBodyElement();
  if (!rootEl) {
    rootEl = mDocumentNode->GetRootElement();
  }
  if (rootEl != mContent) {
    mContent = rootEl;
    SetRoleMapEntryForDoc(rootEl);
    if (mIPCDoc) {
      mIPCDoc->SendRoleChangedEvent(Role());
    }
  }
}

/**
 * Content insertion helper.
 */
class InsertIterator final {
 public:
  InsertIterator(LocalAccessible* aContext,
                 const nsTArray<nsCOMPtr<nsIContent>>* aNodes)
      : mChild(nullptr),
        mChildBefore(nullptr),
        mWalker(aContext),
        mNodes(aNodes),
        mNodesIdx(0) {
    MOZ_ASSERT(aContext, "No context");
    MOZ_ASSERT(aNodes, "No nodes to search for accessible elements");
    MOZ_COUNT_CTOR(InsertIterator);
  }
  MOZ_COUNTED_DTOR(InsertIterator)

  LocalAccessible* Context() const { return mWalker.Context(); }
  LocalAccessible* Child() const { return mChild; }
  LocalAccessible* ChildBefore() const { return mChildBefore; }
  DocAccessible* Document() const { return mWalker.Document(); }

  /**
   * Iterates to a next accessible within the inserted content.
   */
  bool Next();

  void Rejected() {
    mChild = nullptr;
    mChildBefore = nullptr;
  }

 private:
  LocalAccessible* mChild;
  LocalAccessible* mChildBefore;
  TreeWalker mWalker;

  const nsTArray<nsCOMPtr<nsIContent>>* mNodes;
  nsTHashSet<nsPtrHashKey<const nsIContent>> mProcessedNodes;
  uint32_t mNodesIdx;
};

bool InsertIterator::Next() {
  if (mNodesIdx > 0) {
    // If we already processed the first node in the mNodes list,
    // check if we can just use the walker to get its next sibling.
    LocalAccessible* nextChild = mWalker.Next();
    if (nextChild) {
      mChildBefore = mChild;
      mChild = nextChild;
      return true;
    }
  }

  while (mNodesIdx < mNodes->Length()) {
    nsIContent* node = mNodes->ElementAt(mNodesIdx++);
    // Check to see if we already processed this node with this iterator.
    // this can happen if we get two redundant insertions in the case of a
    // text and frame insertion.
    if (!mProcessedNodes.EnsureInserted(node)) {
      continue;
    }

    LocalAccessible* container = Document()->AccessibleOrTrueContainer(
        node->GetFlattenedTreeParentNode(), true);
    // Ignore nodes that are not contained by the container anymore.
    // The container might be changed, for example, because of the subsequent
    // overlapping content insertion (i.e. other content was inserted between
    // this inserted content and its container or the content was reinserted
    // into different container of unrelated part of tree). To avoid a double
    // processing of the content insertion ignore this insertion notification.
    // Note, the inserted content might be not in tree at all at this point
    // what means there's no container. Ignore the insertion too.
    if (container != Context()) {
      continue;
    }

    // HTML comboboxes have no-content list accessible as an intermediate
    // containing all options.
    if (container->IsHTMLCombobox()) {
      container = container->LocalFirstChild();
    }

    if (!container->IsAcceptableChild(node)) {
      continue;
    }

#ifdef A11Y_LOG
    logging::TreeInfo("traversing an inserted node", logging::eVerbose,
                      "container", container, "node", node);
#endif

    nsIContent* prevNode = mChild ? mChild->GetContent() : nullptr;
    if (prevNode && prevNode->GetNextSibling() == node) {
      // If inserted nodes are siblings then just move the walker next.
      LocalAccessible* nextChild = mWalker.Scope(node);
      if (nextChild) {
        mChildBefore = mChild;
        mChild = nextChild;
        return true;
      }
    } else {
      // Otherwise use a new walker to find this node in the container's
      // subtree, and retrieve its preceding sibling.
      TreeWalker finder(container);
      if (finder.Seek(node)) {
        mChild = mWalker.Scope(node);
        if (mChild) {
          MOZ_ASSERT(!mChild->IsRelocated(), "child cannot be aria owned");
          mChildBefore = finder.Prev();
          return true;
        }
      }
    }
  }

  return false;
}

void DocAccessible::ProcessContentInserted(
    LocalAccessible* aContainer, const nsTArray<nsCOMPtr<nsIContent>>* aNodes) {
  // Process insertions if the container accessible is still in tree.
  if (!aContainer->IsInDocument()) {
    return;
  }

  // If new root content has been inserted then update it.
  if (aContainer == this) {
    UpdateRootElIfNeeded();
  }

  InsertIterator iter(aContainer, aNodes);
  if (!iter.Next()) {
    return;
  }

#ifdef A11Y_LOG
  logging::TreeInfo("children before insertion", logging::eVerbose, aContainer);
#endif

  TreeMutation mt(aContainer);
  do {
    LocalAccessible* parent = iter.Child()->LocalParent();
    if (parent) {
      LocalAccessible* previousSibling = iter.ChildBefore();
      if (parent != aContainer ||
          iter.Child()->LocalPrevSibling() != previousSibling) {
        if (previousSibling && previousSibling->LocalParent() != aContainer) {
          // previousSibling hasn't been moved into aContainer yet.
          // previousSibling should be later in the insertion list, so the tree
          // will get adjusted when we process it later.
          MOZ_DIAGNOSTIC_ASSERT(parent == aContainer,
                                "Child moving to new parent, but previous "
                                "sibling in wrong parent");
          continue;
        }
#ifdef A11Y_LOG
        logging::TreeInfo("relocating accessible", 0, "old parent", parent,
                          "new parent", aContainer, "child", iter.Child(),
                          nullptr);
#endif
        MoveChild(iter.Child(), aContainer,
                  previousSibling ? previousSibling->IndexInParent() + 1 : 0);
      }
      continue;
    }

    if (aContainer->InsertAfter(iter.Child(), iter.ChildBefore())) {
#ifdef A11Y_LOG
      logging::TreeInfo("accessible was inserted", 0, "container", aContainer,
                        "child", iter.Child(), nullptr);
#endif

      CreateSubtree(iter.Child());
      mt.AfterInsertion(iter.Child());
      continue;
    }

    MOZ_ASSERT_UNREACHABLE("accessible was rejected");
    iter.Rejected();
  } while (iter.Next());

  mt.Done();

#ifdef A11Y_LOG
  logging::TreeInfo("children after insertion", logging::eVerbose, aContainer);
#endif

  FireEventsOnInsertion(aContainer);
}

void DocAccessible::ProcessContentInserted(LocalAccessible* aContainer,
                                           nsIContent* aNode) {
  if (!aContainer->IsInDocument()) {
    return;
  }

#ifdef A11Y_LOG
  logging::TreeInfo("children before insertion", logging::eVerbose, aContainer);
#endif

#ifdef A11Y_LOG
  logging::TreeInfo("traversing an inserted node", logging::eVerbose,
                    "container", aContainer, "node", aNode);
#endif

  TreeWalker walker(aContainer);
  if (aContainer->IsAcceptableChild(aNode) && walker.Seek(aNode)) {
    LocalAccessible* child = GetAccessible(aNode);
    if (!child) {
      child = GetAccService()->CreateAccessible(aNode, aContainer);
    }

    if (child) {
      TreeMutation mt(aContainer);
      if (!aContainer->InsertAfter(child, walker.Prev())) {
        return;
      }
      CreateSubtree(child);
      mt.AfterInsertion(child);
      mt.Done();

      FireEventsOnInsertion(aContainer);
    }
  }

#ifdef A11Y_LOG
  logging::TreeInfo("children after insertion", logging::eVerbose, aContainer);
#endif
}

void DocAccessible::FireEventsOnInsertion(LocalAccessible* aContainer) {
  // Check to see if change occurred inside an alert, and fire an EVENT_ALERT
  // if it did.
  if (aContainer->IsAlert() || aContainer->IsInsideAlert()) {
    LocalAccessible* ancestor = aContainer;
    do {
      if (ancestor->IsAlert()) {
        FireDelayedEvent(nsIAccessibleEvent::EVENT_ALERT, ancestor);
        break;
      }
    } while ((ancestor = ancestor->LocalParent()));
  }
}

void DocAccessible::ContentRemoved(LocalAccessible* aChild) {
  LocalAccessible* parent = aChild->LocalParent();
  MOZ_DIAGNOSTIC_ASSERT(parent, "Unattached accessible from tree");

#ifdef A11Y_LOG
  logging::TreeInfo("process content removal", 0, "container", parent, "child",
                    aChild, nullptr);
#endif

  // XXX: event coalescence may kill us
  RefPtr<LocalAccessible> kungFuDeathGripChild(aChild);

  TreeMutation mt(parent);
  mt.BeforeRemoval(aChild);

  if (aChild->IsDefunct()) {
    MOZ_ASSERT_UNREACHABLE("Event coalescence killed the accessible");
    mt.Done();
    return;
  }

  MOZ_DIAGNOSTIC_ASSERT(aChild->LocalParent(), "Alive but unparented #1");

  if (aChild->IsRelocated()) {
    nsTArray<RefPtr<LocalAccessible>>* owned = mARIAOwnsHash.Get(parent);
    MOZ_ASSERT(owned, "IsRelocated flag is out of sync with mARIAOwnsHash");
    owned->RemoveElement(aChild);
    if (owned->Length() == 0) {
      mARIAOwnsHash.Remove(parent);
    }
  }
  MOZ_DIAGNOSTIC_ASSERT(aChild->LocalParent(), "Unparented #2");
  UncacheChildrenInSubtree(aChild);
  parent->RemoveChild(aChild);

  mt.Done();
}

void DocAccessible::ContentRemoved(nsIContent* aContentNode) {
  // If child node is not accessible then look for its accessible children.
  LocalAccessible* acc = GetAccessible(aContentNode);
  if (acc) {
    ContentRemoved(acc);
  }

  dom::AllChildrenIterator iter =
      dom::AllChildrenIterator(aContentNode, nsIContent::eAllChildren, true);
  while (nsIContent* childNode = iter.GetNextChild()) {
    ContentRemoved(childNode);
  }

  // If this node has a shadow root, remove its explicit children too.
  // The host node may be removed after the shadow root was attached, and
  // before we asynchronously prune the light DOM and construct the shadow DOM.
  // If this is a case where the node does not have its own accessible, we will
  // not recurse into its current children, so we need to use an
  // ExplicitChildIterator in order to get its accessible children in the light
  // DOM, since they are not accessible anymore via AllChildrenIterator.
  if (aContentNode->GetShadowRoot()) {
    for (nsIContent* childNode = aContentNode->GetFirstChild(); childNode;
         childNode = childNode->GetNextSibling()) {
      ContentRemoved(childNode);
    }
  }
}

bool DocAccessible::RelocateARIAOwnedIfNeeded(nsIContent* aElement) {
  if (!aElement->HasID()) return false;

  AttrRelProviders* list = GetRelProviders(
      aElement->AsElement(), nsDependentAtomString(aElement->GetID()));
  if (list) {
    for (uint32_t idx = 0; idx < list->Length(); idx++) {
      if (list->ElementAt(idx)->mRelAttr == nsGkAtoms::aria_owns) {
        LocalAccessible* owner = GetAccessible(list->ElementAt(idx)->mContent);
        if (owner) {
          mNotificationController->ScheduleRelocation(owner);
          return true;
        }
      }
    }
  }

  return false;
}

void DocAccessible::DoARIAOwnsRelocation(LocalAccessible* aOwner) {
  MOZ_ASSERT(aOwner, "aOwner must be a valid pointer");
  MOZ_ASSERT(aOwner->Elm(), "aOwner->Elm() must be a valid pointer");

#ifdef A11Y_LOG
  logging::TreeInfo("aria owns relocation", logging::eVerbose, aOwner);
#endif

  nsTArray<RefPtr<LocalAccessible>>* owned =
      mARIAOwnsHash.GetOrInsertNew(aOwner);

  IDRefsIterator iter(this, aOwner->Elm(), nsGkAtoms::aria_owns);
  uint32_t idx = 0;
  while (nsIContent* childEl = iter.NextElem()) {
    LocalAccessible* child = GetAccessible(childEl);
    auto insertIdx = aOwner->ChildCount() - owned->Length() + idx;

    // Make an attempt to create an accessible if it wasn't created yet.
    if (!child) {
      // An owned child cannot be an ancestor of the owner.
      bool ok = true;
      bool check = true;
      for (LocalAccessible* parent = aOwner; parent && !parent->IsDoc();
           parent = parent->LocalParent()) {
        if (check) {
          if (parent->Elm()->IsInclusiveDescendantOf(childEl)) {
            ok = false;
            break;
          }
        }
        // We need to do the DOM descendant check again whenever the DOM
        // lineage changes. If parent is relocated, that means the next
        // ancestor will have a different DOM lineage.
        check = parent->IsRelocated();
      }
      if (!ok) {
        continue;
      }

      if (aOwner->IsAcceptableChild(childEl)) {
        child = GetAccService()->CreateAccessible(childEl, aOwner);
        if (child) {
          TreeMutation imut(aOwner);
          aOwner->InsertChildAt(insertIdx, child);
          imut.AfterInsertion(child);
          imut.Done();

          child->SetRelocated(true);
          owned->InsertElementAt(idx, child);
          idx++;

          // Create subtree before adjusting the insertion index, since subtree
          // creation may alter children in the container.
          CreateSubtree(child);
          FireEventsOnInsertion(aOwner);
        }
      }
      continue;
    }

#ifdef A11Y_LOG
    logging::TreeInfo("aria owns traversal", logging::eVerbose, "candidate",
                      child, nullptr);
#endif

    if (owned->IndexOf(child) < idx) {
      continue;  // ignore second entry of same ID
    }

    // Same child on same position, no change.
    if (child->LocalParent() == aOwner) {
      int32_t indexInParent = child->IndexInParent();

      // The child is being placed in its current index,
      // eg. aria-owns='id1 id2 id3' is changed to aria-owns='id3 id2 id1'.
      if (indexInParent == static_cast<int32_t>(insertIdx)) {
        MOZ_ASSERT(child->IsRelocated(),
                   "A child, having an index in parent from aria ownded "
                   "indices range, has to be aria owned");
        MOZ_ASSERT(owned->ElementAt(idx) == child,
                   "Unexpected child in ARIA owned array");
        idx++;
        continue;
      }

      // The child is being inserted directly after its current index,
      // resulting in a no-move case. This will happen when a parent aria-owns
      // its last ordinal child:
      // <ul aria-owns='id2'><li id='id1'></li><li id='id2'></li></ul>
      if (indexInParent == static_cast<int32_t>(insertIdx) - 1) {
        MOZ_ASSERT(!child->IsRelocated(),
                   "Child should be in its ordinal position");
        child->SetRelocated(true);
        owned->InsertElementAt(idx, child);
        idx++;
        continue;
      }
    }

    MOZ_ASSERT(owned->SafeElementAt(idx) != child, "Already in place!");

    // A new child is found, check for loops.
    if (child->LocalParent() != aOwner) {
      // Child is aria-owned by another container, skip.
      if (child->IsRelocated()) {
        continue;
      }

      LocalAccessible* parent = aOwner;
      while (parent && parent != child && !parent->IsDoc()) {
        parent = parent->LocalParent();
      }
      // A referred child cannot be a parent of the owner.
      if (parent == child) {
        continue;
      }
    }

    if (MoveChild(child, aOwner, insertIdx)) {
      child->SetRelocated(true);
      MOZ_ASSERT(owned == mARIAOwnsHash.Get(aOwner));
      owned = mARIAOwnsHash.GetOrInsertNew(aOwner);
      owned->InsertElementAt(idx, child);
      idx++;
    }
  }

  // Put back children that are not seized anymore.
  PutChildrenBack(owned, idx);
  if (owned->Length() == 0) {
    mARIAOwnsHash.Remove(aOwner);
  }
}

void DocAccessible::PutChildrenBack(
    nsTArray<RefPtr<LocalAccessible>>* aChildren, uint32_t aStartIdx) {
  MOZ_ASSERT(aStartIdx <= aChildren->Length(), "Wrong removal index");

  for (auto idx = aStartIdx; idx < aChildren->Length(); idx++) {
    LocalAccessible* child = aChildren->ElementAt(idx);
    if (!child->IsInDocument()) {
      continue;
    }

    // Remove the child from the owner
    LocalAccessible* owner = child->LocalParent();
    if (!owner) {
      NS_ERROR("Cannot put the child back. No parent, a broken tree.");
      continue;
    }

#ifdef A11Y_LOG
    logging::TreeInfo("aria owns put child back", 0, "old parent", owner,
                      "child", child, nullptr);
#endif

    // Unset relocated flag to find an insertion point for the child.
    child->SetRelocated(false);

    nsIContent* content = child->GetContent();
    int32_t idxInParent = -1;
    LocalAccessible* origContainer =
        AccessibleOrTrueContainer(content->GetFlattenedTreeParentNode());
    if (origContainer) {
      TreeWalker walker(origContainer);
      if (walker.Seek(content)) {
        LocalAccessible* prevChild = walker.Prev();
        if (prevChild) {
          idxInParent = prevChild->IndexInParent() + 1;
          MOZ_DIAGNOSTIC_ASSERT(origContainer == prevChild->LocalParent(),
                                "Broken tree");
          origContainer = prevChild->LocalParent();
        } else {
          idxInParent = 0;
        }
      }
    }

    // The child may have already be in its ordinal place for 2 reasons:
    // 1. It was the last ordinal child, and the first aria-owned child.
    //    given:      <ul id="list" aria-owns="b"><li id="a"></li><li
    //    id="b"></li></ul> after load: $("list").setAttribute("aria-owns", "");
    // 2. The preceding adopted children were just reclaimed, eg:
    //    given:      <ul id="list"><li id="b"></li></ul>
    //    after load: $("list").setAttribute("aria-owns", "a b");
    //    later:      $("list").setAttribute("aria-owns", "");
    if (origContainer != owner || child->IndexInParent() != idxInParent) {
      DebugOnly<bool> moved = MoveChild(child, origContainer, idxInParent);
      MOZ_ASSERT(moved, "Failed to put child back.");
    } else {
      MOZ_ASSERT(!child->LocalPrevSibling() ||
                     !child->LocalPrevSibling()->IsRelocated(),
                 "No relocated child should appear before this one");
      MOZ_ASSERT(!child->LocalNextSibling() ||
                     child->LocalNextSibling()->IsRelocated(),
                 "No ordinal child should appear after this one");
    }
  }

  aChildren->RemoveLastElements(aChildren->Length() - aStartIdx);
}

void DocAccessible::TrackMovedAccessible(LocalAccessible* aAcc) {
  // If an Accessible is inserted and moved during the same tick, don't track
  // it as a move because it hasn't been shown yet.
  if (!mInsertedAccessibles.Contains(aAcc)) {
    mMovedAccessibles.EnsureInserted(aAcc);
  }
  // When we move an Accessible, we're also moving its descendants.
  for (uint32_t c = 0, count = aAcc->ContentChildCount(); c < count; ++c) {
    TrackMovedAccessible(aAcc->ContentChildAt(c));
  }
}

bool DocAccessible::MoveChild(LocalAccessible* aChild,
                              LocalAccessible* aNewParent,
                              int32_t aIdxInParent) {
  MOZ_ASSERT(aChild, "No child");
  MOZ_ASSERT(aChild->LocalParent(), "No parent");
  // We can't guarantee MoveChild works correctly for accessibilities storing
  // children outside mChildren.
  MOZ_ASSERT(
      aIdxInParent <= static_cast<int32_t>(aNewParent->mChildren.Length()),
      "Wrong insertion point for a moving child");

  LocalAccessible* curParent = aChild->LocalParent();

  if (!aNewParent->IsAcceptableChild(aChild->GetContent())) {
    return false;
  }

#ifdef A11Y_LOG
  logging::TreeInfo("move child", 0, "old parent", curParent, "new parent",
                    aNewParent, "child", aChild, nullptr);
#endif

  // Forget aria-owns info in case of ARIA owned element. The caller is expected
  // to update it if needed.
  if (aChild->IsRelocated()) {
    aChild->SetRelocated(false);
    nsTArray<RefPtr<LocalAccessible>>* owned = mARIAOwnsHash.Get(curParent);
    MOZ_ASSERT(owned, "IsRelocated flag is out of sync with mARIAOwnsHash");
    owned->RemoveElement(aChild);
    if (owned->Length() == 0) {
      mARIAOwnsHash.Remove(curParent);
    }
  }

  NotificationController::MoveGuard mguard(mNotificationController);

  if (curParent == aNewParent) {
    MOZ_ASSERT(aChild->IndexInParent() != aIdxInParent, "No move case");
    curParent->RelocateChild(aIdxInParent, aChild);
    if (mIPCDoc) {
      TrackMovedAccessible(aChild);
    }

#ifdef A11Y_LOG
    logging::TreeInfo("move child: parent tree after", logging::eVerbose,
                      curParent);
#endif
    return true;
  }

  // If the child cannot be re-inserted into the tree, then make sure to remove
  // it from its present parent and then shutdown it.
  bool hasInsertionPoint =
      (aIdxInParent >= 0) &&
      (aIdxInParent <= static_cast<int32_t>(aNewParent->mChildren.Length()));

  TreeMutation rmut(curParent);
  rmut.BeforeRemoval(aChild, hasInsertionPoint && TreeMutation::kNoShutdown);
  curParent->RemoveChild(aChild);
  rmut.Done();

  // No insertion point for the child.
  if (!hasInsertionPoint) {
    return true;
  }

  TreeMutation imut(aNewParent);
  aNewParent->InsertChildAt(aIdxInParent, aChild);
  if (mIPCDoc) {
    TrackMovedAccessible(aChild);
  }
  imut.AfterInsertion(aChild);
  imut.Done();

#ifdef A11Y_LOG
  logging::TreeInfo("move child: old parent tree after", logging::eVerbose,
                    curParent);
  logging::TreeInfo("move child: new parent tree after", logging::eVerbose,
                    aNewParent);
#endif

  return true;
}

void DocAccessible::CacheChildrenInSubtree(LocalAccessible* aRoot,
                                           LocalAccessible** aFocusedAcc) {
  // If the accessible is focused then report a focus event after all related
  // mutation events.
  if (aFocusedAcc && !*aFocusedAcc &&
      FocusMgr()->HasDOMFocus(aRoot->GetContent())) {
    *aFocusedAcc = aRoot;
  }

  LocalAccessible* root =
      aRoot->IsHTMLCombobox() ? aRoot->LocalFirstChild() : aRoot;
  if (root->KidsFromDOM()) {
    TreeMutation mt(root, TreeMutation::kNoEvents);
    TreeWalker walker(root);
    while (LocalAccessible* child = walker.Next()) {
      if (child->IsBoundToParent()) {
        MoveChild(child, root, root->mChildren.Length());
        continue;
      }

      root->AppendChild(child);
      mt.AfterInsertion(child);

      CacheChildrenInSubtree(child, aFocusedAcc);
    }
    mt.Done();
  }

  // Fire events for ARIA elements.
  if (!aRoot->HasARIARole()) {
    return;
  }

  // XXX: we should delay document load complete event if the ARIA document
  // has aria-busy.
  roles::Role role = aRoot->ARIARole();
  if (!aRoot->IsDoc() &&
      (role == roles::DIALOG || role == roles::NON_NATIVE_DOCUMENT)) {
    FireDelayedEvent(nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_COMPLETE, aRoot);
  }
}

void DocAccessible::UncacheChildrenInSubtree(LocalAccessible* aRoot) {
  aRoot->mStateFlags |= eIsNotInDocument;
  RemoveDependentIDsFor(aRoot);

  // The parent of the removed subtree is about to be cleared, so we must do
  // this here rather than in LocalAccessible::UnbindFromParent because we need
  // the ancestry for this to work.
  if (StaticPrefs::accessibility_cache_enabled_AtStartup() &&
      (aRoot->IsTable() || aRoot->IsTableCell())) {
    CachedTableAccessible::Invalidate(aRoot);
  }

  nsTArray<RefPtr<LocalAccessible>>* owned = mARIAOwnsHash.Get(aRoot);
  uint32_t count = aRoot->ContentChildCount();
  for (uint32_t idx = 0; idx < count; idx++) {
    LocalAccessible* child = aRoot->ContentChildAt(idx);

    if (child->IsRelocated()) {
      MOZ_ASSERT(owned, "IsRelocated flag is out of sync with mARIAOwnsHash");
      owned->RemoveElement(child);
      if (owned->Length() == 0) {
        mARIAOwnsHash.Remove(aRoot);
        owned = nullptr;
      }
    }

    // Removing this accessible from the document doesn't mean anything about
    // accessibles for subdocuments, so skip removing those from the tree.
    if (!child->IsDoc()) {
      UncacheChildrenInSubtree(child);
    }
  }

  if (aRoot->IsNodeMapEntry() &&
      mNodeToAccessibleMap.Get(aRoot->GetNode()) == aRoot) {
    mNodeToAccessibleMap.Remove(aRoot->GetNode());
  }
}

void DocAccessible::ShutdownChildrenInSubtree(LocalAccessible* aAccessible) {
  // Traverse through children and shutdown them before this accessible. When
  // child gets shutdown then it removes itself from children array of its
  // parent. Use jdx index to process the cases if child is not attached to the
  // parent and as result doesn't remove itself from its children.
  uint32_t count = aAccessible->ContentChildCount();
  for (uint32_t idx = 0, jdx = 0; idx < count; idx++) {
    LocalAccessible* child = aAccessible->ContentChildAt(jdx);
    if (!child->IsBoundToParent()) {
      NS_ERROR("Parent refers to a child, child doesn't refer to parent!");
      jdx++;
    }

    // Don't cross document boundaries. The outerdoc shutdown takes care about
    // its subdocument.
    if (!child->IsDoc()) ShutdownChildrenInSubtree(child);
  }

  UnbindFromDocument(aAccessible);
}

bool DocAccessible::IsLoadEventTarget() const {
  nsCOMPtr<nsIDocShellTreeItem> treeItem = mDocumentNode->GetDocShell();
  if (!treeItem) {
    return false;
  }

  nsCOMPtr<nsIDocShellTreeItem> parentTreeItem;
  treeItem->GetInProcessParent(getter_AddRefs(parentTreeItem));

  // Not a root document.
  if (parentTreeItem) {
    // Return true if it's either:
    // a) tab document;
    nsCOMPtr<nsIDocShellTreeItem> rootTreeItem;
    treeItem->GetInProcessRootTreeItem(getter_AddRefs(rootTreeItem));
    if (parentTreeItem == rootTreeItem) return true;

    // b) frame/iframe document and its parent document is not in loading state
    // Note: we can get notifications while document is loading (and thus
    // while there's no parent document yet).
    DocAccessible* parentDoc = ParentDocument();
    return parentDoc && parentDoc->HasLoadState(eCompletelyLoaded);
  }

  // It's content (not chrome) root document.
  return (treeItem->ItemType() == nsIDocShellTreeItem::typeContent);
}

void DocAccessible::SetIPCDoc(DocAccessibleChild* aIPCDoc) {
  MOZ_ASSERT(!mIPCDoc || !aIPCDoc, "Clobbering an attached IPCDoc!");
  mIPCDoc = aIPCDoc;
}

void DocAccessible::DispatchScrollingEvent(nsINode* aTarget,
                                           uint32_t aEventType) {
  LocalAccessible* acc = GetAccessible(aTarget);
  if (!acc) {
    return;
  }

  nsIFrame* frame = acc->GetFrame();
  if (!frame) {
    // Although the accessible had a frame at scroll time, it may now be gone
    // because of display: contents.
    return;
  }

  auto [scrollPoint, scrollRange] = ComputeScrollData(acc);

  int32_t appUnitsPerDevPixel =
      mPresShell->GetPresContext()->AppUnitsPerDevPixel();

  LayoutDeviceIntPoint scrollPointDP = LayoutDevicePoint::FromAppUnitsToNearest(
      scrollPoint, appUnitsPerDevPixel);
  LayoutDeviceIntRect scrollRangeDP =
      LayoutDeviceRect::FromAppUnitsToNearest(scrollRange, appUnitsPerDevPixel);

  RefPtr<AccEvent> event =
      new AccScrollingEvent(aEventType, acc, scrollPointDP.x, scrollPointDP.y,
                            scrollRangeDP.width, scrollRangeDP.height);
  nsEventShell::FireEvent(event);
}

void DocAccessible::ARIAActiveDescendantIDMaybeMoved(
    LocalAccessible* aAccessible) {
  LocalAccessible* widget = nullptr;
  if (aAccessible->IsActiveDescendant(&widget) && widget) {
    // The active descendant might have just been inserted and may not be in the
    // tree yet. Therefore, schedule this async to ensure the tree is up to
    // date.
    mNotificationController
        ->ScheduleNotification<DocAccessible, LocalAccessible>(
            this, &DocAccessible::ARIAActiveDescendantChanged, widget);
  }
}

void DocAccessible::SetRoleMapEntryForDoc(dom::Element* aElement) {
  const nsRoleMapEntry* entry = aria::GetRoleMap(aElement);
  if (!entry || entry->role == roles::APPLICATION ||
      entry->role == roles::DIALOG ||
      // Role alert isn't valid on the body element according to the ARIA spec,
      // but it's useful for our UI; e.g. the WebRTC sharing indicator.
      (entry->role == roles::ALERT && !mDocumentNode->IsContentDocument())) {
    SetRoleMapEntry(entry);
    return;
  }
  // No other ARIA roles are valid on body elements.
  SetRoleMapEntry(nullptr);
}

LocalAccessible* DocAccessible::GetAccessible(nsINode* aNode) const {
  return aNode == mDocumentNode ? const_cast<DocAccessible*>(this)
                                : mNodeToAccessibleMap.Get(aNode);
}

bool DocAccessible::HasPrimaryAction() const {
  if (HyperTextAccessible::HasPrimaryAction()) {
    return true;
  }
  // mContent is normally the body, but there might be a click listener on the
  // root.
  dom::Element* root = mDocumentNode->GetRootElement();
  if (mContent != root) {
    return nsCoreUtils::HasClickListener(root);
  }
  return false;
}

void DocAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName) {
  aName.Truncate();
  if (aIndex != 0) {
    return;
  }
  if (HasPrimaryAction()) {
    aName.AssignLiteral("click");
  }
}

void DocAccessible::MaybeHandleChangeToHiddenNameOrDescription(
    nsIContent* aChild) {
  if (!HasLoadState(eTreeConstructed)) {
    return;
  }
  for (nsIContent* content = aChild; content; content = content->GetParent()) {
    if (HasAccessible(content)) {
      // This node isn't hidden. Events for name/description dependents will be
      // fired elsewhere.
      break;
    }
    nsAtom* id = content->GetID();
    if (!id) {
      continue;
    }
    auto* providers =
        GetRelProviders(content->AsElement(), nsDependentAtomString(id));
    if (!providers) {
      continue;
    }
    for (auto& provider : *providers) {
      if (provider->mRelAttr != nsGkAtoms::aria_labelledby &&
          provider->mRelAttr != nsGkAtoms::aria_describedby) {
        continue;
      }
      LocalAccessible* dependentAcc = GetAccessible(provider->mContent);
      if (!dependentAcc) {
        continue;
      }
      FireDelayedEvent(provider->mRelAttr == nsGkAtoms::aria_labelledby
                           ? nsIAccessibleEvent::EVENT_NAME_CHANGE
                           : nsIAccessibleEvent::EVENT_DESCRIPTION_CHANGE,
                       dependentAcc);
    }
  }
}

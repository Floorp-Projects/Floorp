/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Accessible-inl.h"
#include "AccIterator.h"
#include "DocAccessible-inl.h"
#include "DocAccessibleChild.h"
#include "HTMLImageMapAccessible.h"
#include "nsAccCache.h"
#include "nsAccessiblePivot.h"
#include "nsAccUtils.h"
#include "nsEventShell.h"
#include "nsTextEquivUtils.h"
#include "Role.h"
#include "RootAccessible.h"
#include "TreeWalker.h"
#include "xpcAccessibleDocument.h"

#include "nsIMutableArray.h"
#include "nsICommandManager.h"
#include "nsIDocShell.h"
#include "nsIDocument.h"
#include "nsIDOMAttr.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMDocument.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMMutationEvent.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMXULPopupElement.h"
#include "nsIEditingSession.h"
#include "nsIFrame.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsImageFrame.h"
#include "nsIPersistentProperties2.h"
#include "nsIPresShell.h"
#include "nsIServiceManager.h"
#include "nsViewManager.h"
#include "nsIScrollableFrame.h"
#include "nsUnicharUtils.h"
#include "nsIURI.h"
#include "nsIWebNavigation.h"
#include "nsFocusManager.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/EventStates.h"
#include "mozilla/dom/DocumentType.h"
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

static const uint32_t kRelationAttrsLen = ArrayLength(kRelationAttrs);

////////////////////////////////////////////////////////////////////////////////
// Constructor/desctructor

DocAccessible::
  DocAccessible(nsIDocument* aDocument, nsIPresShell* aPresShell) :
  HyperTextAccessibleWrap(nullptr, this),
  // XXX aaronl should we use an algorithm for the initial cache size?
  mAccessibleCache(kDefaultCacheLength),
  mNodeToAccessibleMap(kDefaultCacheLength),
  mDocumentNode(aDocument),
  mScrollPositionChangedTicks(0),
  mLoadState(eTreeConstructionPending), mDocFlags(0), mLoadEventType(0),
  mVirtualCursor(nullptr),
  mPresShell(aPresShell), mIPCDoc(nullptr)
{
  mGenericTypes |= eDocument;
  mStateFlags |= eNotNodeMapEntry;

  MOZ_ASSERT(mPresShell, "should have been given a pres shell");
  mPresShell->SetDocAccessible(this);

  // If this is a XUL Document, it should not implement nsHyperText
  if (mDocumentNode && mDocumentNode->IsXULDocument())
    mGenericTypes &= ~eHyperText;
}

DocAccessible::~DocAccessible()
{
  NS_ASSERTION(!mPresShell, "LastRelease was never called!?!");
}


////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_CYCLE_COLLECTION_CLASS(DocAccessible)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(DocAccessible, Accessible)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNotificationController)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mVirtualCursor)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChildDocuments)
  for (auto iter = tmp->mDependentIDsHash.Iter(); !iter.Done(); iter.Next()) {
    AttrRelProviderArray* providers = iter.UserData();

    for (int32_t jdx = providers->Length() - 1; jdx >= 0; jdx--) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(
        cb, "content of dependent ids hash entry of document accessible");

      AttrRelProvider* provider = (*providers)[jdx];
      cb.NoteXPCOMChild(provider->mContent);

      NS_ASSERTION(provider->mContent->IsInUncomposedDoc(),
                   "Referred content is not in document!");
    }
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAccessibleCache)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAnchorJumpElm)
  for (auto it = tmp->mARIAOwnsHash.ConstIter(); !it.Done(); it.Next()) {
    nsTArray<RefPtr<Accessible> >* ar = it.UserData();
    for (uint32_t i = 0; i < ar->Length(); i++) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb,
                                         "mARIAOwnsHash entry item");
      cb.NoteXPCOMChild(ar->ElementAt(i));
    }
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(DocAccessible, Accessible)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mNotificationController)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mVirtualCursor)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mChildDocuments)
  tmp->mDependentIDsHash.Clear();
  tmp->mNodeToAccessibleMap.Clear();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAccessibleCache)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAnchorJumpElm)
  tmp->mARIAOwnsHash.Clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(DocAccessible)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentObserver)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIAccessiblePivotObserver)
NS_INTERFACE_MAP_END_INHERITING(HyperTextAccessible)

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
    Title(aName); // Try title element
  }
  if (aName.IsEmpty()) {   // Last resort: use URL
    URL(aName);
  }

  return eNameOK;
}

// Accessible public method
role
DocAccessible::NativeRole()
{
  nsCOMPtr<nsIDocShell> docShell = nsCoreUtils::GetDocShellFor(mDocumentNode);
  if (docShell) {
    nsCOMPtr<nsIDocShellTreeItem> sameTypeRoot;
    docShell->GetSameTypeRootTreeItem(getter_AddRefs(sameTypeRoot));
    int32_t itemType = docShell->ItemType();
    if (sameTypeRoot == docShell) {
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

void
DocAccessible::TakeFocus()
{
  // Focus the document.
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  nsCOMPtr<nsIDOMElement> newFocus;
  fm->MoveFocus(mDocumentNode->GetWindow(), nullptr,
                nsFocusManager::MOVEFOCUS_ROOT, 0, getter_AddRefs(newFocus));
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

  nsCOMPtr<nsIDocShell> docShell = mDocumentNode->GetDocShell();
  if (!docShell) {
    return nullptr;
  }

  nsCOMPtr<nsIEditingSession> editingSession;
  docShell->GetEditingSession(getter_AddRefs(editingSession));
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

void
DocAccessible::URL(nsAString& aURL) const
{
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
}

void
DocAccessible::DocType(nsAString& aType) const
{
#ifdef MOZ_XUL
  nsCOMPtr<nsIXULDocument> xulDoc(do_QueryInterface(mDocumentNode));
  if (xulDoc) {
    aType.AssignLiteral("window"); // doctype not implemented for XUL at time of writing - causes assertion
    return;
  }
#endif
  dom::DocumentType* docType = mDocumentNode->GetDoctype();
  if (docType)
    docType->GetPublicId(aType);
}

////////////////////////////////////////////////////////////////////////////////
// Accessible

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

  // Mark the document as shutdown before AT is notified about the document
  // removal from its container (valid for root documents on ATK and due to
  // some reason for MSAA, refer to bug 757392 for details).
  mStateFlags |= eIsDefunct;

  if (mNotificationController) {
    mNotificationController->Shutdown();
    mNotificationController = nullptr;
  }

  RemoveEventListeners();

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

  mPresShell->SetDocAccessible(nullptr);
  mPresShell = nullptr;  // Avoid reentrancy

  mDependentIDsHash.Clear();
  mNodeToAccessibleMap.Clear();
  ClearCache(mAccessibleCache);

  HyperTextAccessibleWrap::Shutdown();

  GetAccService()->NotifyOfDocumentShutdown(this, kungFuDeathGripDoc);
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
nsRect
DocAccessible::RelativeBounds(nsIFrame** aRelativeFrame) const
{
  *aRelativeFrame = GetFrame();

  nsIDocument *document = mDocumentNode;
  nsIDocument *parentDoc = nullptr;

  nsRect bounds;
  while (document) {
    nsIPresShell *presShell = document->GetShell();
    if (!presShell)
      return nsRect();

    nsRect scrollPort;
    nsIScrollableFrame* sf = presShell->GetRootScrollFrameAsScrollableExternal();
    if (sf) {
      scrollPort = sf->GetScrollPortRect();
    } else {
      nsIFrame* rootFrame = presShell->GetRootFrame();
      if (!rootFrame)
        return nsRect();

      scrollPort = rootFrame->GetRect();
    }

    if (parentDoc) {  // After first time thru loop
      // XXXroc bogus code! scrollPort is relative to the viewport of
      // this document, but we're intersecting rectangles derived from
      // multiple documents and assuming they're all in the same coordinate
      // system. See bug 514117.
      bounds.IntersectRect(scrollPort, bounds);
    }
    else {  // First time through loop
      bounds = scrollPort;
    }

    document = parentDoc = document->GetParentDocument();
  }

  return bounds;
}

// DocAccessible protected member
nsresult
DocAccessible::AddEventListeners()
{
  nsCOMPtr<nsIDocShell> docShell(mDocumentNode->GetDocShell());

  // We want to add a command observer only if the document is content and has
  // an editor.
  if (docShell->ItemType() == nsIDocShellTreeItem::typeContent) {
    nsCOMPtr<nsICommandManager> commandManager = docShell->GetCommandManager();
    if (commandManager)
      commandManager->AddCommandObserver(this, "obs_documentCreated");
  }

  SelectionMgr()->AddDocSelectionListener(mPresShell);

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

    nsCOMPtr<nsIDocShell> docShell(mDocumentNode->GetDocShell());
    NS_ASSERTION(docShell, "doc should support nsIDocShellTreeItem.");

    if (docShell) {
      if (docShell->ItemType() == nsIDocShellTreeItem::typeContent) {
        nsCOMPtr<nsICommandManager> commandManager = docShell->GetCommandManager();
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

  SelectionMgr()->RemoveDocSelectionListener(mPresShell);
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
                       const char16_t* aData)
{
  if (!nsCRT::strcmp(aTopic,"obs_documentCreated")) {    
    // State editable will now be set, readonly is now clear
    // Normally we only fire delayed events created from the node, not an
    // accessible object. See the AccStateChangeEvent constructor for details
    // about this exceptional case.
    RefPtr<AccEvent> event =
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
                              PivotMoveReason aReason,
                              bool aIsFromUserInput)
{
  RefPtr<AccEvent> event =
    new AccVCChangeEvent(
      this, (aOldAccessible ? aOldAccessible->ToInternalAccessible() : nullptr),
      aOldStart, aOldEnd, aReason,
      aIsFromUserInput ? eFromUserInput : eNoUserInput);
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
                                   nsIAtom* aAttribute, int32_t aModType,
                                   const nsAttrValue* aNewValue)
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

  if (aAttribute == nsGkAtoms::id) {
    RelocateARIAOwnedIfNeeded(aElement);
  }

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
    return;
  }

  if (aAttribute == nsGkAtoms::aria_disabled ||
      aAttribute == nsGkAtoms::disabled)
    mStateBitWasOn = accessible->Unavailable();
}

void
DocAccessible::NativeAnonymousChildListChange(nsIDocument* aDocument,
                                              nsIContent* aContent,
                                              bool aIsRemove)
{
}

void
DocAccessible::AttributeChanged(nsIDocument* aDocument,
                                dom::Element* aElement,
                                int32_t aNameSpaceID, nsIAtom* aAttribute,
                                int32_t aModType,
                                const nsAttrValue* aOldValue)
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

  if (!accessible->IsBoundToParent()) {
    MOZ_ASSERT_UNREACHABLE("DOM attribute change on accessible detached from tree");
    return;
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
  // Note. Checking the XUL or HTML namespace would not seem to gain us
  // anything, because disabled attribute really is going to mean the same
  // thing in any namespace.
  // Note. We use the attribute instead of the disabled state bit because
  // ARIA's aria-disabled does not affect the disabled state bit.
  if (aAttribute == nsGkAtoms::disabled ||
      aAttribute == nsGkAtoms::aria_disabled) {
    // Do nothing if state wasn't changed (like @aria-disabled was removed but
    // @disabled is still presented).
    if (aAccessible->Unavailable() == mStateBitWasOn)
      return;

    RefPtr<AccEvent> enabledChangeEvent =
      new AccStateChangeEvent(aAccessible, states::ENABLED, mStateBitWasOn);
    FireDelayedEvent(enabledChangeEvent);

    RefPtr<AccEvent> sensitiveChangeEvent =
      new AccStateChangeEvent(aAccessible, states::SENSITIVE, mStateBitWasOn);
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

  // Fire name change and description change events. XXX: it's not complete and
  // dupes the code logic of accessible name and description calculation, we do
  // that for performance reasons.
  if (aAttribute == nsGkAtoms::aria_label) {
    FireDelayedEvent(nsIAccessibleEvent::EVENT_NAME_CHANGE, aAccessible);
    return;
  }

  if (aAttribute == nsGkAtoms::aria_describedby) {
    FireDelayedEvent(nsIAccessibleEvent::EVENT_DESCRIPTION_CHANGE, aAccessible);
    return;
  }

  nsIContent* elm = aAccessible->GetContent();
  if (aAttribute == nsGkAtoms::aria_labelledby &&
      !elm->HasAttr(kNameSpaceID_None, nsGkAtoms::aria_label)) {
    FireDelayedEvent(nsIAccessibleEvent::EVENT_NAME_CHANGE, aAccessible);
    return;
  }

  if (aAttribute == nsGkAtoms::alt &&
      !elm->HasAttr(kNameSpaceID_None, nsGkAtoms::aria_label) &&
      !elm->HasAttr(kNameSpaceID_None, nsGkAtoms::aria_labelledby)) {
    FireDelayedEvent(nsIAccessibleEvent::EVENT_NAME_CHANGE, aAccessible);
    return;
  }

  if (aAttribute == nsGkAtoms::title) {
    if (!elm->HasAttr(kNameSpaceID_None, nsGkAtoms::aria_label) &&
        !elm->HasAttr(kNameSpaceID_None, nsGkAtoms::aria_labelledby) &&
        !elm->HasAttr(kNameSpaceID_None, nsGkAtoms::alt)) {
      FireDelayedEvent(nsIAccessibleEvent::EVENT_NAME_CHANGE, aAccessible);
      return;
    }

    if (!elm->HasAttr(kNameSpaceID_None, nsGkAtoms::aria_describedby))
      FireDelayedEvent(nsIAccessibleEvent::EVENT_DESCRIPTION_CHANGE, aAccessible);

    return;
  }

  if (aAttribute == nsGkAtoms::aria_busy) {
    bool isOn = elm->AttrValueIs(aNameSpaceID, aAttribute, nsGkAtoms::_true,
                                 eCaseMatters);
    RefPtr<AccEvent> event =
      new AccStateChangeEvent(aAccessible, states::BUSY, isOn);
    FireDelayedEvent(event);
    return;
  }

  if (aAttribute == nsGkAtoms::id) {
    RelocateARIAOwnedIfNeeded(elm);
  }

  // ARIA or XUL selection
  if ((aAccessible->GetContent()->IsXULElement() &&
       aAttribute == nsGkAtoms::selected) ||
      aAttribute == nsGkAtoms::aria_selected) {
    Accessible* widget =
      nsAccUtils::GetSelectableContainer(aAccessible, aAccessible->State());
    if (widget) {
      AccSelChangeEvent::SelChangeType selChangeType =
        elm->AttrValueIs(aNameSpaceID, aAttribute, nsGkAtoms::_true, eCaseMatters) ?
          AccSelChangeEvent::eSelectionAdd : AccSelChangeEvent::eSelectionRemove;

      RefPtr<AccEvent> event =
        new AccSelChangeEvent(widget, aAccessible, selChangeType);
      FireDelayedEvent(event);
    }

    return;
  }

  if (aAttribute == nsGkAtoms::contenteditable) {
    RefPtr<AccEvent> editableChangeEvent =
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
    RefPtr<AccEvent> event =
      new AccStateChangeEvent(aAccessible, states::REQUIRED);
    FireDelayedEvent(event);
    return;
  }

  if (aAttribute == nsGkAtoms::aria_invalid) {
    RefPtr<AccEvent> event =
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
    RefPtr<AccEvent> event =
      new AccStateChangeEvent(aAccessible, states::EXPANDED);
    FireDelayedEvent(event);
    return;
  }

  // For aria attributes like drag and drop changes we fire a generic attribute
  // change event; at least until native API comes up with a more meaningful event.
  uint8_t attrFlags = aria::AttrCharacteristicsFor(aAttribute);
  if (!(attrFlags & ATTR_BYPASSOBJ)) {
    RefPtr<AccEvent> event =
      new AccObjectAttrChangedEvent(aAccessible, aAttribute);
    FireDelayedEvent(event);
  }

  nsIContent* elm = aAccessible->GetContent();

  // Update aria-hidden flag for the whole subtree iff aria-hidden is changed
  // on the root, i.e. ignore any affiliated aria-hidden changes in the subtree
  // of top aria-hidden.
  if (aAttribute == nsGkAtoms::aria_hidden) {
    bool isDefined = aria::HasDefinedARIAHidden(elm);
    if (isDefined != aAccessible->IsARIAHidden() &&
        !aAccessible->Parent()->IsARIAHidden()) {
      aAccessible->SetARIAHidden(isDefined);

      RefPtr<AccEvent> event =
        new AccObjectAttrChangedEvent(aAccessible, aAttribute);
      FireDelayedEvent(event);
    }
    return;
  }

  if (aAttribute == nsGkAtoms::aria_checked ||
      (aAccessible->IsButton() &&
       aAttribute == nsGkAtoms::aria_pressed)) {
    const uint64_t kState = (aAttribute == nsGkAtoms::aria_checked) ?
                            states::CHECKED : states::PRESSED;
    RefPtr<AccEvent> event = new AccStateChangeEvent(aAccessible, kState);
    FireDelayedEvent(event);

    bool wasMixed = (mARIAAttrOldValue == nsGkAtoms::mixed);
    bool isMixed = elm->AttrValueIs(kNameSpaceID_None, aAttribute,
                                    nsGkAtoms::mixed, eCaseMatters);
    if (isMixed != wasMixed) {
      RefPtr<AccEvent> event =
        new AccStateChangeEvent(aAccessible, states::MIXED, isMixed);
      FireDelayedEvent(event);
    }
    return;
  }

  if (aAttribute == nsGkAtoms::aria_readonly) {
    RefPtr<AccEvent> event =
      new AccStateChangeEvent(aAccessible, states::READONLY);
    FireDelayedEvent(event);
    return;
  }

  // Fire text value change event whenever aria-valuetext is changed.
  if (aAttribute == nsGkAtoms::aria_valuetext) {
    FireDelayedEvent(nsIAccessibleEvent::EVENT_TEXT_VALUE_CHANGE, aAccessible);
    return;
  }

  // Fire numeric value change event when aria-valuenow is changed and
  // aria-valuetext is empty
  if (aAttribute == nsGkAtoms::aria_valuenow &&
      (!elm->HasAttr(kNameSpaceID_None, nsGkAtoms::aria_valuetext) ||
       elm->AttrValueIs(kNameSpaceID_None, nsGkAtoms::aria_valuetext,
                        nsGkAtoms::_empty, eCaseMatters))) {
    FireDelayedEvent(nsIAccessibleEvent::EVENT_VALUE_CHANGE, aAccessible);
    return;
  }

  if (aAttribute == nsGkAtoms::aria_owns) {
    mNotificationController->ScheduleRelocation(aAccessible);
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
                                   EventStates aStateMask)
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
      RefPtr<AccEvent> event =
        new AccSelChangeEvent(widget, accessible, selChangeType);
      FireDelayedEvent(event);
      return;
    }

    RefPtr<AccEvent> event =
      new AccStateChangeEvent(accessible, states::CHECKED,
                              aContent->AsElement()->State().HasState(NS_EVENT_STATE_CHECKED));
    FireDelayedEvent(event);
  }

  if (aStateMask.HasState(NS_EVENT_STATE_INVALID)) {
    RefPtr<AccEvent> event =
      new AccStateChangeEvent(accessible, states::INVALID, true);
    FireDelayedEvent(event);
  }

  if (aStateMask.HasState(NS_EVENT_STATE_VISITED)) {
    RefPtr<AccEvent> event =
      new AccStateChangeEvent(accessible, states::TRAVERSED, true);
    FireDelayedEvent(event);
  }
}

void
DocAccessible::DocumentStatesChanged(nsIDocument* aDocument,
                                     EventStates aStateMask)
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
DocAccessible::ContentRemoved(nsIDocument* aDocument,
                              nsIContent* aContainerNode,
                              nsIContent* aChildNode, int32_t /* unused */,
                              nsIContent* aPreviousSiblingNode)
{
#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eTree)) {
    logging::MsgBegin("TREE", "DOM content removed; doc: %p", this);
    logging::Node("container node", aContainerNode);
    logging::Node("content node", aChildNode);
    logging::MsgEnd();
  }
#endif
  // This one and content removal notification from layout may result in
  // double processing of same subtrees. If it pops up in profiling, then
  // consider reusing a document node cache to reject these notifications early.
  Accessible* container = GetAccessibleOrContainer(aContainerNode);
  if (container) {
    UpdateTreeOnRemoval(container, aChildNode);
  }
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

  nsViewManager* vm = mPresShell->GetViewManager();
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
DocAccessible::GetAccessibleOrContainer(nsINode* aNode) const
{
  if (!aNode || !aNode->GetComposedDoc())
    return nullptr;

  nsINode* currNode = aNode;
  Accessible* accessible = nullptr;
  while (!(accessible = GetAccessible(currNode))) {
    nsINode* parent = nullptr;

    // If this is a content node, try to get a flattened parent content node.
    // This will smartly skip from the shadow root to the host element,
    // over parentless document fragment
    if (currNode->IsContent())
      parent = currNode->AsContent()->GetFlattenedTreeParent();

    // Fallback to just get parent node, in case there is no parent content
    // node. Or current node is not a content node.
    if (!parent)
      parent = currNode->GetParentNode();

    if (!(currNode = parent)) break;
  }

  return accessible;
}

Accessible*
DocAccessible::GetAccessibleOrDescendant(nsINode* aNode) const
{
  Accessible* acc = GetAccessible(aNode);
  if (acc)
    return acc;

  acc = GetContainerAccessible(aNode);
  if (acc) {
    uint32_t childCnt = acc->ChildCount();
    for (uint32_t idx = 0; idx < childCnt; idx++) {
      Accessible* child = acc->GetChildAt(idx);
      for (nsIContent* elm = child->GetContent();
           elm && elm != acc->GetContent();
           elm = elm->GetFlattenedTreeParent()) {
        if (elm == aNode)
          return child;
      }
    }
  }

  return nullptr;
}

void
DocAccessible::BindToDocument(Accessible* aAccessible,
                              const nsRoleMapEntry* aRoleMapEntry)
{
  // Put into DOM node cache.
  if (aAccessible->IsNodeMapEntry())
    mNodeToAccessibleMap.Put(aAccessible->GetNode(), aAccessible);

  // Put into unique ID cache.
  mAccessibleCache.Put(aAccessible->UniqueID(), aAccessible);

  aAccessible->SetRoleMapEntry(aRoleMapEntry);

  AddDependentIDsFor(aAccessible);

  if (aAccessible->HasOwnContent()) {
    nsIContent* el = aAccessible->GetContent();
    if (el->HasAttr(kNameSpaceID_None, nsGkAtoms::aria_owns)) {
      mNotificationController->ScheduleRelocation(aAccessible);
    }
  }
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

  // Update XPCOM part.
  xpcAccessibleDocument* xpcDoc = GetAccService()->GetCachedXPCDocument(this);
  if (xpcDoc)
    xpcDoc->NotifyOfShutdown(aAccessible);

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
      AccessibleOrTrueContainer(aContainerNode) : this;
    if (container) {
      // Ignore notification if the container node is no longer in the DOM tree.
      mNotificationController->ScheduleContentInsertion(container,
                                                        aStartChildNode,
                                                        aEndChildNode);
    }
  }
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

  nsIContent* parent = aContent->GetFlattenedTreeParent();
  ContentRemoved(parent, aContent);
  ContentInserted(parent, aContent, aContent->GetNextSibling());
}

void
DocAccessible::ProcessInvalidationList()
{
  // Invalidate children of container accessible for each element in
  // invalidation list. Allow invalidation list insertions while container
  // children are recached.
  for (uint32_t idx = 0; idx < mInvalidationList.Length(); idx++) {
    nsIContent* content = mInvalidationList[idx];
    if (!HasAccessible(content)) {
      Accessible* container = GetContainerAccessible(content);
      if (container) {
        ProcessContentInserted(container, content);
      }
    }
  }

  mInvalidationList.Clear();
}

Accessible*
DocAccessible::GetAccessibleEvenIfNotInMap(nsINode* aNode) const
{
if (!aNode->IsContent() || !aNode->AsContent()->IsHTMLElement(nsGkAtoms::area))
    return GetAccessible(aNode);

  // XXX Bug 135040, incorrect when multiple images use the same map.
  nsIFrame* frame = aNode->AsContent()->GetPrimaryFrame();
  nsImageFrame* imageFrame = do_QueryFrame(frame);
  if (imageFrame) {
    Accessible* parent = GetAccessible(imageFrame->GetContent());
    if (parent) {
      Accessible* area =
        parent->AsImageMap()->GetChildAccessibleFor(aNode);
      if (area)
        return area;

      return nullptr;
    }
  }

  return GetAccessible(aNode);
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

void
DocAccessible::DoInitialUpdate()
{
  if (nsCoreUtils::IsTabDocument(mDocumentNode))
    mDocFlags |= eTabDocument;

  mLoadState |= eTreeConstructed;

  // Set up a root element and ARIA role mapping.
  UpdateRootElIfNeeded();

  // Build initial tree.
  CacheChildrenInSubtree(this);

  // Fire reorder event after the document tree is constructed. Note, since
  // this reorder event is processed by parent document then events targeted to
  // this document may be fired prior to this reorder event. If this is
  // a problem then consider to keep event processing per tab document.
  if (!IsRoot()) {
    RefPtr<AccReorderEvent> reorderEvent = new AccReorderEvent(Parent());
    ParentDocument()->FireDelayedEvent(reorderEvent);
  }

  uint32_t childCount = ChildCount();
  for (uint32_t i = 0; i < childCount; i++) {
    Accessible* child = GetChildAt(i);
    RefPtr<AccShowEvent> event = new AccShowEvent(child);
  FireDelayedEvent(event);
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
    RefPtr<AccEvent> loadEvent = new AccEvent(mLoadEventType, this);
    FireDelayedEvent(loadEvent);

    mLoadEventType = 0;
  }

  // Fire busy state change event.
  RefPtr<AccEvent> stateEvent =
    new AccStateChangeEvent(this, states::BUSY, false);
  FireDelayedEvent(stateEvent);
}

void
DocAccessible::AddDependentIDsFor(Accessible* aRelProvider, nsIAtom* aRelAttr)
{
  dom::Element* relProviderEl = aRelProvider->Elm();
  if (!relProviderEl)
    return;

  for (uint32_t idx = 0; idx < kRelationAttrsLen; idx++) {
    nsIAtom* relAttr = *kRelationAttrs[idx];
    if (aRelAttr && aRelAttr != relAttr)
      continue;

    if (relAttr == nsGkAtoms::_for) {
      if (!relProviderEl->IsAnyOfHTMLElements(nsGkAtoms::label,
                                               nsGkAtoms::output))
        continue;

    } else if (relAttr == nsGkAtoms::control) {
      if (!relProviderEl->IsAnyOfXULElements(nsGkAtoms::label,
                                              nsGkAtoms::description))
        continue;
    }

    IDRefsIterator iter(this, relProviderEl, relAttr);
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
          new AttrRelProvider(relAttr, relProviderEl);
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
    if (aRelAttr)
      break;
  }

  // Make sure to schedule the tree update if needed.
  mNotificationController->ScheduleProcessing();
}

void
DocAccessible::RemoveDependentIDsFor(Accessible* aRelProvider,
                                     nsIAtom* aRelAttr)
{
  dom::Element* relProviderElm = aRelProvider->Elm();
  if (!relProviderElm)
    return;

  for (uint32_t idx = 0; idx < kRelationAttrsLen; idx++) {
    nsIAtom* relAttr = *kRelationAttrs[idx];
    if (aRelAttr && aRelAttr != *kRelationAttrs[idx])
      continue;

    IDRefsIterator iter(this, relProviderElm, relAttr);
    while (true) {
      const nsDependentSubstring id = iter.NextID();
      if (id.IsEmpty())
        break;

      AttrRelProviderArray* providers = mDependentIDsHash.Get(id);
      if (providers) {
        for (uint32_t jdx = 0; jdx < providers->Length(); ) {
          AttrRelProvider* provider = (*providers)[jdx];
          if (provider->mRelAttr == relAttr &&
              provider->mContent == relProviderElm)
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

  if (aAttribute == nsGkAtoms::href) {
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
DocAccessible::UpdateRootElIfNeeded()
{
  dom::Element* rootEl = mDocumentNode->GetBodyElement();
  if (!rootEl) {
    rootEl = mDocumentNode->GetRootElement();
  }
  if (rootEl != mContent) {
    mContent = rootEl;
    SetRoleMapEntry(aria::GetRoleMap(rootEl));
    if (mIPCDoc) {
      mIPCDoc->SendRoleChangedEvent(Role());
    }
  }
}

/**
 * Content insertion helper.
 */
class InsertIterator final
{
public:
  InsertIterator(Accessible* aContext,
                 const nsTArray<nsCOMPtr<nsIContent> >* aNodes) :
    mChild(nullptr), mChildBefore(nullptr), mWalker(aContext),
    mStopNode(nullptr), mNodes(aNodes), mNodesIdx(0)
  {
    MOZ_ASSERT(aContext, "No context");
    MOZ_ASSERT(aNodes, "No nodes to search for accessible elements");
    MOZ_COUNT_CTOR(InsertIterator);
  }
  ~InsertIterator() { MOZ_COUNT_DTOR(InsertIterator); }

  Accessible* Context() const { return mWalker.Context(); }
  Accessible* Child() const { return mChild; }
  Accessible* ChildBefore() const { return mChildBefore; }
  DocAccessible* Document() const { return mWalker.Document(); }

  /**
   * Iterates to a next accessible within the inserted content.
   */
  bool Next();

  void Rejected()
  {
    mChild = nullptr;
    mChildBefore = nullptr;
  }

private:
  Accessible* mChild;
  Accessible* mChildBefore;
  TreeWalker mWalker;
  nsIContent* mStopNode;

  const nsTArray<nsCOMPtr<nsIContent> >* mNodes;
  uint32_t mNodesIdx;
};

bool
InsertIterator::Next()
{
  if (mNodesIdx > 0) {
    Accessible* nextChild = mWalker.Next(mStopNode);
    if (nextChild) {
      mChildBefore = mChild;
      mChild = nextChild;
      return true;
    }
  }

  while (mNodesIdx < mNodes->Length()) {
    // Ignore nodes that are not contained by the container anymore.

    // The container might be changed, for example, because of the subsequent
    // overlapping content insertion (i.e. other content was inserted between
    // this inserted content and its container or the content was reinserted
    // into different container of unrelated part of tree). To avoid a double
    // processing of the content insertion ignore this insertion notification.
    // Note, the inserted content might be not in tree at all at this point
    // what means there's no container. Ignore the insertion too.
    nsIContent* prevNode = mNodes->SafeElementAt(mNodesIdx - 1);
    nsIContent* node = mNodes->ElementAt(mNodesIdx++);
    Accessible* container = Document()->AccessibleOrTrueContainer(node);
    if (container != Context()) {
      continue;
    }

    // HTML comboboxes have no-content list accessible as an intermediate
    // containing all options.
    if (container->IsHTMLCombobox()) {
      container = container->FirstChild();
    }

    if (!container->IsAcceptableChild(node)) {
      continue;
    }

#ifdef A11Y_LOG
    logging::TreeInfo("traversing an inserted node", logging::eVerbose,
                      "container", container, "node", node);
#endif

    // If inserted nodes are siblings then just move the walker next.
    if (prevNode && prevNode->GetNextSibling() == node) {
      mStopNode = node;
      Accessible* nextChild = mWalker.Next(mStopNode);
      if (nextChild) {
        mChildBefore = mChild;
        mChild = nextChild;
        return true;
      }
    }
    else if (mWalker.Seek(node)) {
      mStopNode = node;
      mChildBefore = mWalker.Prev();
      mChild = mWalker.Next(mStopNode);
      if (mChild) {
        return true;
      }
    }
  }

  return false;
}

void
DocAccessible::ProcessContentInserted(Accessible* aContainer,
                                      const nsTArray<nsCOMPtr<nsIContent> >* aNodes)
{
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
  logging::TreeInfo("children before insertion", logging::eVerbose,
                    aContainer);
#endif

  TreeMutation mt(aContainer);
  do {
    Accessible* parent = iter.Child()->Parent();
    if (parent) {
      if (parent != aContainer) {
#ifdef A11Y_LOG
        logging::TreeInfo("stealing accessible", 0,
                          "old parent", parent, "new parent",
                          aContainer, "child", iter.Child(), nullptr);
#endif
        MOZ_ASSERT_UNREACHABLE("stealing accessible");
        continue;
      }

#ifdef A11Y_LOG
      logging::TreeInfo("binding to same parent", logging::eVerbose,
                        "parent", aContainer, "child", iter.Child(), nullptr);
#endif
      continue;
    }

    if (aContainer->InsertAfter(iter.Child(), iter.ChildBefore())) {
#ifdef A11Y_LOG
      logging::TreeInfo("accessible was inserted", 0,
                        "container", aContainer, "child", iter.Child(), nullptr);
#endif

      mt.AfterInsertion(iter.Child());
      CreateSubtree(iter.Child());
      continue;
    }

    MOZ_ASSERT_UNREACHABLE("accessible was rejected");
    iter.Rejected();
  } while (iter.Next());

  mt.Done();

#ifdef A11Y_LOG
  logging::TreeInfo("children after insertion", logging::eVerbose,
                    aContainer);
#endif

  FireEventsOnInsertion(aContainer);
}

void
DocAccessible::ProcessContentInserted(Accessible* aContainer, nsIContent* aNode)
{
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
    Accessible* child = GetAccessible(aNode);
    if (!child) {
      child = GetAccService()->CreateAccessible(aNode, aContainer);
    }

    if (child) {
      TreeMutation mt(aContainer);
      if (!aContainer->InsertAfter(child, walker.Prev())) {
        return;
      }
      mt.AfterInsertion(child);
      mt.Done();

      CreateSubtree(child);
      FireEventsOnInsertion(aContainer);
    }
  }

#ifdef A11Y_LOG
  logging::TreeInfo("children after insertion", logging::eVerbose, aContainer);
#endif
}

void
DocAccessible::FireEventsOnInsertion(Accessible* aContainer)
{
  // Check to see if change occurred inside an alert, and fire an EVENT_ALERT
  // if it did.
  if (aContainer->IsAlert() || aContainer->IsInsideAlert()) {
    Accessible* ancestor = aContainer;
    do {
      if (ancestor->IsAlert()) {
        FireDelayedEvent(nsIAccessibleEvent::EVENT_ALERT, ancestor);
        break;
      }
    }
    while ((ancestor = ancestor->Parent()));
  }
}

void
DocAccessible::UpdateTreeOnRemoval(Accessible* aContainer, nsIContent* aChildNode)
{
  // If child node is not accessible then look for its accessible children.
  Accessible* child = GetAccessible(aChildNode);
#ifdef A11Y_LOG
  logging::TreeInfo("process content removal", 0,
                    "container", aContainer, "child", aChildNode);
#endif

  TreeMutation mt(aContainer);
  if (child) {
    mt.BeforeRemoval(child);
    MOZ_ASSERT(aContainer == child->Parent(), "Wrong parent");
    aContainer->RemoveChild(child);
    UncacheChildrenInSubtree(child);
    mt.Done();
    return;
  }

  TreeWalker walker(aContainer, aChildNode, TreeWalker::eWalkCache);
  while (Accessible* child = walker.Next()) {
    mt.BeforeRemoval(child);
    MOZ_ASSERT(aContainer == child->Parent(), "Wrong parent");
    aContainer->RemoveChild(child);
    UncacheChildrenInSubtree(child);
  }
  mt.Done();
}

bool
DocAccessible::RelocateARIAOwnedIfNeeded(nsIContent* aElement)
{
  if (!aElement->HasID())
    return false;

  AttrRelProviderArray* list =
    mDependentIDsHash.Get(nsDependentAtomString(aElement->GetID()));
  if (list) {
    for (uint32_t idx = 0; idx < list->Length(); idx++) {
      if (list->ElementAt(idx)->mRelAttr == nsGkAtoms::aria_owns) {
        Accessible* owner = GetAccessible(list->ElementAt(idx)->mContent);
        if (owner) {
          mNotificationController->ScheduleRelocation(owner);
          return true;
        }
      }
    }
  }

  return false;
}

void
DocAccessible::ValidateARIAOwned()
{
  for (auto it = mARIAOwnsHash.Iter(); !it.Done(); it.Next()) {
    Accessible* owner = it.Key();
    nsTArray<RefPtr<Accessible> >* children = it.UserData();

    // Owner is about to die, put children back if applicable.
    if (!mAccessibleCache.GetWeak(reinterpret_cast<void*>(owner)) ||
        !owner->IsInDocument()) {
      PutChildrenBack(children, 0);
      it.Remove();
      continue;
    }

    for (uint32_t idx = 0; idx < children->Length(); idx++) {
      Accessible* child = children->ElementAt(idx);
      if (!child->IsInDocument()) {
        children->RemoveElementAt(idx);
        idx--;
        continue;
      }

      NS_ASSERTION(child->Parent(), "No parent for ARIA owned?");

      // If DOM node doesn't have a frame anymore then shutdown its accessible.
      if (child->Parent() && !child->GetFrame()) {
        UpdateTreeOnRemoval(child->Parent(), child->GetContent());
        children->RemoveElementAt(idx);
        idx--;
        continue;
      }

      NS_ASSERTION(child->Parent() == owner,
                   "Illigally stolen ARIA owned child!");
    }

    if (children->Length() == 0) {
      it.Remove();
    }
  }
}

void
DocAccessible::DoARIAOwnsRelocation(Accessible* aOwner)
{
  nsTArray<RefPtr<Accessible> >* children = mARIAOwnsHash.LookupOrAdd(aOwner);

  MOZ_ASSERT(aOwner, "aOwner must be a valid pointer");
  MOZ_ASSERT(aOwner->Elm(), "aOwner->Elm() must be a valid pointer");

#ifdef A11Y_LOG
  logging::TreeInfo("aria owns relocation", logging::eVerbose, aOwner);
#endif

  IDRefsIterator iter(this, aOwner->Elm(), nsGkAtoms::aria_owns);
  uint32_t arrayIdx = 0, insertIdx = aOwner->ChildCount() - children->Length();
  while (nsIContent* childEl = iter.NextElem()) {
    Accessible* child = GetAccessible(childEl);

    // Make an attempt to create an accessible if it wasn't created yet.
    if (!child) {
      if (aOwner->IsAcceptableChild(childEl)) {
        child = GetAccService()->CreateAccessible(childEl, aOwner);
        if (child) {
          TreeMutation imut(aOwner);
          aOwner->InsertChildAt(insertIdx, child);
          imut.AfterInsertion(child);
          imut.Done();

          child->SetRelocated(true);
          children->InsertElementAt(arrayIdx, child);

          insertIdx = child->IndexInParent() + 1;
          arrayIdx++;

          CreateSubtree(child);
          FireEventsOnInsertion(aOwner);
        }
      }
      continue;
    }

#ifdef A11Y_LOG
  logging::TreeInfo("aria owns traversal", logging::eVerbose,
                    "candidate", child, nullptr);
#endif

    // Same child on same position, no change.
    if (child->Parent() == aOwner &&
        child->IndexInParent() == static_cast<int32_t>(insertIdx)) {
      NS_ASSERTION(child == children->ElementAt(arrayIdx), "Not in sync!");
      insertIdx++; arrayIdx++;
      continue;
    }

    NS_ASSERTION(children->SafeElementAt(arrayIdx) != child, "Already in place!");

    nsTArray<RefPtr<Accessible> >::index_type idx = children->IndexOf(child);
    if (idx < arrayIdx) {
      continue; // ignore second entry of same ID
    }

    // A new child is found, check for loops.
    if (child->Parent() != aOwner) {
      Accessible* parent = aOwner;
      while (parent && parent != child && !parent->IsDoc()) {
        parent = parent->Parent();
      }
      // A referred child cannot be a parent of the owner.
      if (parent == child) {
        continue;
      }
    }

    if (MoveChild(child, aOwner, insertIdx)) {
      child->SetRelocated(true);
      children->InsertElementAt(arrayIdx, child);
      arrayIdx++;
      insertIdx = child->IndexInParent() + 1;
    }
  }

  // Put back children that are not seized anymore.
  PutChildrenBack(children, arrayIdx);
  if (children->Length() == 0) {
    mARIAOwnsHash.Remove(aOwner);
  }
}

void
DocAccessible::PutChildrenBack(nsTArray<RefPtr<Accessible> >* aChildren,
                               uint32_t aStartIdx)
{
  nsTArray<RefPtr<Accessible> > containers;
  for (auto idx = aStartIdx; idx < aChildren->Length(); idx++) {
    Accessible* child = aChildren->ElementAt(idx);
    if (!child->IsInDocument()) {
      continue;
    }

    // Remove the child from the owner
    Accessible* owner = child->Parent();
    if (!owner) {
      NS_ERROR("Cannot put the child back. No parent, a broken tree.");
      continue;
    }

#ifdef A11Y_LOG
    logging::TreeInfo("aria owns put child back", 0,
                      "old parent", owner, "child", child, nullptr);
#endif

    // Unset relocated flag to find an insertion point for the child.
    child->SetRelocated(false);

    int32_t idxInParent = -1;
    Accessible* origContainer = GetContainerAccessible(child->GetContent());
    if (origContainer) {
      TreeWalker walker(origContainer);
      if (walker.Seek(child->GetContent())) {
        Accessible* prevChild = walker.Prev();
        idxInParent = prevChild ? prevChild->IndexInParent() + 1 : 0;
      }
    }
    MoveChild(child, origContainer, idxInParent);
  }

  aChildren->RemoveElementsAt(aStartIdx, aChildren->Length() - aStartIdx);
}

bool
DocAccessible::MoveChild(Accessible* aChild, Accessible* aNewParent,
                         int32_t aIdxInParent)
{
  MOZ_ASSERT(aChild, "No child");
  MOZ_ASSERT(aChild->Parent(), "No parent");

  Accessible* curParent = aChild->Parent();

#ifdef A11Y_LOG
  logging::TreeInfo("move child", 0,
                    "old parent", curParent, "new parent", aNewParent,
                    "child", aChild, nullptr);
#endif

  // If the child was taken from from an ARIA owns element.
  if (aChild->IsRelocated()) {
    nsTArray<RefPtr<Accessible> >* children = mARIAOwnsHash.Get(curParent);
    children->RemoveElement(aChild);
  }

  if (curParent == aNewParent) {
    MOZ_ASSERT(aChild->IndexInParent() != aIdxInParent, "No move case");
    curParent->MoveChild(aIdxInParent, aChild);

#ifdef A11Y_LOG
    logging::TreeInfo("move child: parent tree after",
                      logging::eVerbose, curParent);
#endif
    return true;
  }

  if (!aNewParent->IsAcceptableChild(aChild->GetContent())) {
    return false;
  }

  TreeMutation rmut(curParent);
  rmut.BeforeRemoval(aChild, TreeMutation::kNoShutdown);
  curParent->RemoveChild(aChild);
  rmut.Done();

  // No insertion point for the child.
  if (aIdxInParent == -1) {
    return true;
  }

  TreeMutation imut(aNewParent);
  aNewParent->InsertChildAt(aIdxInParent, aChild);
  imut.AfterInsertion(aChild);
  imut.Done();

#ifdef A11Y_LOG
  logging::TreeInfo("move child: old parent tree after",
                    logging::eVerbose, curParent);
  logging::TreeInfo("move child: new parent tree after",
                    logging::eVerbose, aNewParent);
#endif

  return true;
}


void
DocAccessible::CacheChildrenInSubtree(Accessible* aRoot,
                                      Accessible** aFocusedAcc)
{
  // If the accessible is focused then report a focus event after all related
  // mutation events.
  if (aFocusedAcc && !*aFocusedAcc &&
      FocusMgr()->HasDOMFocus(aRoot->GetContent()))
    *aFocusedAcc = aRoot;

  Accessible* root = aRoot->IsHTMLCombobox() ? aRoot->FirstChild() : aRoot;
  if (root->KidsFromDOM()) {
#ifdef A11Y_LOG
  logging::TreeInfo("caching children", logging::eVerbose, aRoot);
#endif
    TreeMutation mt(root, TreeMutation::kNoEvents);
    TreeWalker walker(root);
    while (Accessible* child = walker.Next()) {
      if (child->IsBoundToParent()) {
        MoveChild(child, root, root->ChildCount());
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
  if (!aRoot->IsDoc() && (role == roles::DIALOG || role == roles::DOCUMENT)) {
    FireDelayedEvent(nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_COMPLETE, aRoot);
  }
}

void
DocAccessible::UncacheChildrenInSubtree(Accessible* aRoot)
{
  aRoot->mStateFlags |= eIsNotInDocument;
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

    // Don't cross document boundaries. The outerdoc shutdown takes care about
    // its subdocument.
    if (!child->IsDoc())
      ShutdownChildrenInSubtree(child);
  }

  UnbindFromDocument(aAccessible);
}

bool
DocAccessible::IsLoadEventTarget() const
{
  nsCOMPtr<nsIDocShellTreeItem> treeItem = mDocumentNode->GetDocShell();
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
  return (treeItem->ItemType() == nsIDocShellTreeItem::typeContent);
}

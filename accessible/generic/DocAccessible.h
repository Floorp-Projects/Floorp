/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_DocAccessible_h__
#define mozilla_a11y_DocAccessible_h__

#include "nsIAccessiblePivot.h"

#include "HyperTextAccessibleWrap.h"
#include "AccEvent.h"

#include "nsClassHashtable.h"
#include "nsTHashMap.h"
#include "mozilla/UniquePtr.h"
#include "nsIDocumentObserver.h"
#include "nsITimer.h"
#include "nsTHashSet.h"
#include "nsWeakReference.h"

class nsAccessiblePivot;

const uint32_t kDefaultCacheLength = 128;

namespace mozilla {

class EditorBase;
class PresShell;

namespace dom {
class Document;
}

namespace a11y {

class DocManager;
class NotificationController;
class DocAccessibleChild;
class RelatedAccIterator;
template <class Class, class... Args>
class TNotification;

class DocAccessible : public HyperTextAccessibleWrap,
                      public nsIDocumentObserver,
                      public nsSupportsWeakReference,
                      public nsIAccessiblePivotObserver {
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DocAccessible, LocalAccessible)

  NS_DECL_NSIACCESSIBLEPIVOTOBSERVER

 protected:
  typedef mozilla::dom::Document Document;

 public:
  DocAccessible(Document* aDocument, PresShell* aPresShell);

  // nsIDocumentObserver
  NS_DECL_NSIDOCUMENTOBSERVER

  // LocalAccessible
  virtual void Init();
  virtual void Shutdown() override;
  virtual nsIFrame* GetFrame() const override;
  virtual nsINode* GetNode() const override;
  Document* DocumentNode() const { return mDocumentNode; }

  virtual mozilla::a11y::ENameValueFlag Name(nsString& aName) const override;
  virtual void Description(nsString& aDescription) const override;
  virtual LocalAccessible* FocusedChild() override;
  virtual mozilla::a11y::role NativeRole() const override;
  virtual uint64_t NativeState() const override;
  virtual uint64_t NativeInteractiveState() const override;
  virtual bool NativelyUnavailable() const override;
  virtual void ApplyARIAState(uint64_t* aState) const override;
  virtual already_AddRefed<AccAttributes> Attributes() override;

  virtual void TakeFocus() const override;

#ifdef A11Y_LOG
  virtual nsresult HandleAccEvent(AccEvent* aEvent) override;
#endif

  virtual nsRect RelativeBounds(nsIFrame** aRelativeFrame) const override;

  // ActionAccessible
  virtual bool HasPrimaryAction() const override;
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;

  // HyperTextAccessible
  virtual already_AddRefed<EditorBase> GetEditor() const override;

  // DocAccessible

  /**
   * Return document URL.
   */
  void URL(nsAString& aURL) const;

  /**
   * Return DOM document title.
   */
  void Title(nsString& aTitle) const;

  /**
   * Return DOM document mime type.
   */
  void MimeType(nsAString& aType) const;
  /**
   * Return DOM document type.
   */
  void DocType(nsAString& aType) const;

  /**
   * Adds an entry to mQueuedCacheUpdates indicating aAcc requires
   * a cache update on domain aNewDomain. If we've already queued an update
   * for aAcc, aNewDomain is or'd with the existing domain(s)
   * and the map is updated. Otherwise, the entry is simply inserted.
   * This function also schedules processing on the controller.
   * Note that this CANNOT be used for anything which fires events, since events
   * must be fired after their associated cache update.
   */
  void QueueCacheUpdate(LocalAccessible* aAcc, uint64_t aNewDomain);

  /**
   * Return virtual cursor associated with the document.
   */
  nsIAccessiblePivot* VirtualCursor();

  /**
   * Returns true if the instance has shutdown.
   */
  bool HasShutdown() const { return !mPresShell; }

  /**
   * Return presentation shell for this document accessible.
   */
  PresShell* PresShellPtr() const {
    MOZ_DIAGNOSTIC_ASSERT(!HasShutdown());
    return mPresShell;
  }

  /**
   * Return the presentation shell's context.
   */
  nsPresContext* PresContext() const;

  /**
   * Return true if associated DOM document was loaded and isn't unloading.
   */
  bool IsContentLoaded() const;

  bool IsHidden() const;

  bool IsViewportCacheDirty() { return mViewportCacheDirty; }
  void SetViewportCacheDirty(bool aDirty) { mViewportCacheDirty = aDirty; }

  /**
   * Document load states.
   */
  enum LoadState {
    // initial tree construction is pending
    eTreeConstructionPending = 0,
    // initial tree construction done
    eTreeConstructed = 1,
    // DOM document is loaded.
    eDOMLoaded = 1 << 1,
    // document is ready
    eReady = eTreeConstructed | eDOMLoaded,
    // document and all its subdocuments are ready
    eCompletelyLoaded = eReady | 1 << 2
  };

  /**
   * Return true if the document has given document state.
   */
  bool HasLoadState(LoadState aState) const {
    return (mLoadState & static_cast<uint32_t>(aState)) ==
           static_cast<uint32_t>(aState);
  }

  /**
   * Return a native window handler or pointer depending on platform.
   */
  virtual void* GetNativeWindow() const;

  /**
   * Return the parent document.
   */
  DocAccessible* ParentDocument() const {
    return mParent ? mParent->Document() : nullptr;
  }

  /**
   * Return the child document count.
   */
  uint32_t ChildDocumentCount() const { return mChildDocuments.Length(); }

  /**
   * Return the child document at the given index.
   */
  DocAccessible* GetChildDocumentAt(uint32_t aIndex) const {
    return mChildDocuments.SafeElementAt(aIndex, nullptr);
  }

  /**
   * Fire accessible event asynchronously.
   */
  void FireDelayedEvent(AccEvent* aEvent);
  void FireDelayedEvent(uint32_t aEventType, LocalAccessible* aTarget);
  void FireEventsOnInsertion(LocalAccessible* aContainer);

  /**
   * Fire value change event on the given accessible if applicable.
   */
  void MaybeNotifyOfValueChange(LocalAccessible* aAccessible);

  /**
   * Get/set the anchor jump.
   */
  LocalAccessible* AnchorJump() {
    return GetAccessibleOrContainer(mAnchorJumpElm);
  }

  void SetAnchorJump(nsIContent* aTargetNode) { mAnchorJumpElm = aTargetNode; }

  /**
   * Bind the child document to the tree.
   */
  void BindChildDocument(DocAccessible* aDocument);

  /**
   * Process the generic notification.
   *
   * @note  The caller must guarantee that the given instance still exists when
   *          notification is processed.
   * @see   NotificationController::HandleNotification
   */
  template <class Class, class... Args>
  void HandleNotification(
      Class* aInstance,
      typename TNotification<Class, Args...>::Callback aMethod, Args*... aArgs);

  /**
   * Return the cached accessible by the given DOM node if it's in subtree of
   * this document accessible or the document accessible itself, otherwise null.
   *
   * @return the accessible object
   */
  LocalAccessible* GetAccessible(nsINode* aNode) const;

  /**
   * Return an accessible for the given node even if the node is not in
   * document's node map cache (like HTML area element).
   *
   * XXX: it should be really merged with GetAccessible().
   */
  LocalAccessible* GetAccessibleEvenIfNotInMap(nsINode* aNode) const;
  LocalAccessible* GetAccessibleEvenIfNotInMapOrContainer(nsINode* aNode) const;

  /**
   * Return whether the given DOM node has an accessible or not.
   */
  bool HasAccessible(nsINode* aNode) const { return GetAccessible(aNode); }

  /**
   * Return the cached accessible by the given unique ID within this document.
   *
   * @note   the unique ID matches with the uniqueID() of Accessible
   *
   * @param  aUniqueID  [in] the unique ID used to cache the node.
   */
  LocalAccessible* GetAccessibleByUniqueID(void* aUniqueID) {
    return UniqueID() == aUniqueID ? this : mAccessibleCache.GetWeak(aUniqueID);
  }

  /**
   * Return the cached accessible by the given unique ID looking through
   * this and nested documents.
   */
  LocalAccessible* GetAccessibleByUniqueIDInSubtree(void* aUniqueID);

  /**
   * Return an accessible for the given DOM node or container accessible if
   * the node is not accessible. If aNoContainerIfPruned is true it will return
   * null if the node is in a pruned subtree (eg. aria-hidden or unselected deck
   * panel)
   */
  LocalAccessible* GetAccessibleOrContainer(
      nsINode* aNode, bool aNoContainerIfPruned = false) const;

  /**
   * Return a container accessible for the given DOM node.
   */
  LocalAccessible* GetContainerAccessible(nsINode* aNode) const;

  /**
   * Return an accessible for the given node if any, or an immediate accessible
   * container for it.
   */
  LocalAccessible* AccessibleOrTrueContainer(
      nsINode* aNode, bool aNoContainerIfPruned = false) const;

  /**
   * Return an accessible for the given node or its first accessible descendant.
   */
  LocalAccessible* GetAccessibleOrDescendant(nsINode* aNode) const;

  /**
   * Returns aria-owns seized child at the given index.
   */
  LocalAccessible* ARIAOwnedAt(LocalAccessible* aParent,
                               uint32_t aIndex) const {
    nsTArray<RefPtr<LocalAccessible>>* children = mARIAOwnsHash.Get(aParent);
    if (children) {
      return children->SafeElementAt(aIndex);
    }
    return nullptr;
  }
  uint32_t ARIAOwnedCount(LocalAccessible* aParent) const {
    nsTArray<RefPtr<LocalAccessible>>* children = mARIAOwnsHash.Get(aParent);
    return children ? children->Length() : 0;
  }

  /**
   * Return true if the given ID is referred by relation attribute.
   */
  bool IsDependentID(dom::Element* aElement, const nsAString& aID) const {
    return GetRelProviders(aElement, aID);
  }

  /**
   * Initialize the newly created accessible and put it into document caches.
   *
   * @param  aAccessible    [in] created accessible
   * @param  aRoleMapEntry  [in] the role map entry role the ARIA role or
   * nullptr if none
   */
  void BindToDocument(LocalAccessible* aAccessible,
                      const nsRoleMapEntry* aRoleMapEntry);

  /**
   * Remove from document and shutdown the given accessible.
   */
  void UnbindFromDocument(LocalAccessible* aAccessible);

  /**
   * Notify the document accessible that content was inserted.
   */
  void ContentInserted(nsIContent* aStartChildNode, nsIContent* aEndChildNode);

  /**
   * @see nsAccessibilityService::ScheduleAccessibilitySubtreeUpdate
   */
  void ScheduleTreeUpdate(nsIContent* aContent);

  /**
   * Update the tree on content removal.
   */
  void ContentRemoved(LocalAccessible* aAccessible);
  void ContentRemoved(nsIContent* aContentNode);

  /**
   * Updates accessible tree when rendered text is changed.
   */
  void UpdateText(nsIContent* aTextNode);

  /**
   * Recreate an accessible, results in hide/show events pair.
   */
  void RecreateAccessible(nsIContent* aContent);

  /**
   * Schedule ARIA owned element relocation if needed. Return true if relocation
   * was scheduled.
   */
  bool RelocateARIAOwnedIfNeeded(nsIContent* aEl);

  /**
   * Return a notification controller associated with the document.
   */
  NotificationController* Controller() const { return mNotificationController; }

  /**
   * If this document is in a content process return the object responsible for
   * communicating with the main process for it.
   */
  DocAccessibleChild* IPCDoc() const { return mIPCDoc; }

  /**
   * Notify the document that a DOM node has been scrolled. document will
   * dispatch throttled accessibility events for scrolling, and a scroll-end
   * event. This function also queues a cache update for ScrollPosition.
   */
  void HandleScroll(nsINode* aTarget);

  /**
   * Retrieves the scroll frame (if it exists) for the given accessible
   * and returns its scroll position and scroll range. If the given
   * accessible is `this`, return the scroll position and range of
   * the root scroll frame. Return values have been scaled by the
   * PresShell's resolution.
   */
  std::pair<nsPoint, nsRect> ComputeScrollData(LocalAccessible* aAcc);

 protected:
  virtual ~DocAccessible();

  void LastRelease();

  // DocAccessible
  virtual nsresult AddEventListeners();
  virtual nsresult RemoveEventListeners();

  /**
   * Marks this document as loaded or loading.
   */
  void NotifyOfLoad(uint32_t aLoadEventType);
  void NotifyOfLoading(bool aIsReloading);

  friend class DocManager;

  /**
   * Perform initial update (create accessible tree).
   * Can be overridden by wrappers to prepare initialization work.
   */
  virtual void DoInitialUpdate();

  /**
   * Updates root element and picks up ARIA role on it if any.
   */
  void UpdateRootElIfNeeded();

  /**
   * Process document load notification, fire document load and state busy
   * events if applicable.
   */
  void ProcessLoad();

  /**
   * Append the given document accessible to this document's child document
   * accessibles.
   */
  bool AppendChildDocument(DocAccessible* aChildDocument) {
    // XXX(Bug 1631371) Check if this should use a fallible operation as it
    // pretended earlier, or change the return type to void.
    mChildDocuments.AppendElement(aChildDocument);
    return true;
  }

  /**
   * Remove the given document accessible from this document's child document
   * accessibles.
   */
  void RemoveChildDocument(DocAccessible* aChildDocument) {
    mChildDocuments.RemoveElement(aChildDocument);
  }

  /**
   * Add dependent IDs pointed by accessible element by relation attribute to
   * cache. If the relation attribute is missed then all relation attributes
   * are checked.
   *
   * @param aRelProvider [in] accessible that element has relation attribute
   * @param aRelAttr     [in, optional] relation attribute
   */
  void AddDependentIDsFor(LocalAccessible* aRelProvider,
                          nsAtom* aRelAttr = nullptr);

  /**
   * Remove dependent IDs pointed by accessible element by relation attribute
   * from cache. If the relation attribute is absent then all relation
   * attributes are checked.
   *
   * @param aRelProvider [in] accessible that element has relation attribute
   * @param aRelAttr     [in, optional] relation attribute
   */
  void RemoveDependentIDsFor(LocalAccessible* aRelProvider,
                             nsAtom* aRelAttr = nullptr);

  /**
   * Update or recreate an accessible depending on a changed attribute.
   *
   * @param aElement   [in] the element the attribute was changed on
   * @param aAttribute [in] the changed attribute
   * @return            true if an action was taken on the attribute change
   */
  bool UpdateAccessibleOnAttrChange(mozilla::dom::Element* aElement,
                                    nsAtom* aAttribute);

  /**
   * Process ARIA active-descendant attribute change.
   */
  void ARIAActiveDescendantChanged(LocalAccessible* aAccessible);

  /**
   * Update the accessible tree for inserted content.
   */
  void ProcessContentInserted(
      LocalAccessible* aContainer,
      const nsTArray<nsCOMPtr<nsIContent>>* aInsertedContent);
  void ProcessContentInserted(LocalAccessible* aContainer,
                              nsIContent* aInsertedContent);

  /**
   * Used to notify the document to make it process the invalidation list.
   *
   * While children are cached we may encounter the case there's no accessible
   * for referred content by related accessible. Store these related nodes to
   * invalidate their containers later.
   */
  void ProcessInvalidationList();

  /**
   * Process mPendingUpdates
   */
  void ProcessPendingUpdates();

  /**
   * Called from NotificationController to process this doc's
   * mQueuedCacheUpdates list. For each acc in the map, this function
   * sends a cache update with its corresponding CacheDomain.
   */
  void ProcessQueuedCacheUpdates();

  /**
   * Only works in content process documents.
   */
  bool IsAccessibleBeingMoved(LocalAccessible* aAcc) {
    return mMovedAccessibles.Contains(aAcc);
  }

  /**
   * Called from NotificationController before mutation events are processed to
   * notify the parent process which Accessibles are being moved (if any).
   */
  void SendAccessiblesWillMove();

  /**
   * Called from NotificationController after all mutation events have been
   * processed to clear our data about Accessibles that were moved during this
   * tick.
   */
  void ClearMovedAccessibles() {
    mMovedAccessibles.Clear();
    mInsertedAccessibles.Clear();
  }

  /**
   * Steals or puts back accessible subtrees.
   */
  void DoARIAOwnsRelocation(LocalAccessible* aOwner);

  /**
   * Moves children back under their original parents.
   */
  void PutChildrenBack(nsTArray<RefPtr<LocalAccessible>>* aChildren,
                       uint32_t aStartIdx);

  bool MoveChild(LocalAccessible* aChild, LocalAccessible* aNewParent,
                 int32_t aIdxInParent);

  /**
   * Create accessible tree.
   *
   * @param aRoot       [in] a root of subtree to create
   * @param aFocusedAcc [in, optional] a focused accessible under created
   *                      subtree if any
   */
  void CacheChildrenInSubtree(LocalAccessible* aRoot,
                              LocalAccessible** aFocusedAcc = nullptr);
  void CreateSubtree(LocalAccessible* aRoot);

  /**
   * Remove accessibles in subtree from node to accessible map.
   */
  void UncacheChildrenInSubtree(LocalAccessible* aRoot);

  /**
   * Shutdown any cached accessible in the subtree.
   *
   * @param aAccessible  [in] the root of the subrtee to invalidate accessible
   *                      child/parent refs in
   */
  void ShutdownChildrenInSubtree(LocalAccessible* aAccessible);

  /**
   * Return true if the document is a target of document loading events
   * (for example, state busy change or document reload events).
   *
   * Rules: The root chrome document accessible is never an event target
   * (for example, Firefox UI window). If the sub document is loaded within its
   * parent document then the parent document is a target only (aka events
   * coalescence).
   */
  bool IsLoadEventTarget() const;

  /*
   * Set the object responsible for communicating with the main process on
   * behalf of this document.
   */
  void SetIPCDoc(DocAccessibleChild* aIPCDoc);

  friend class DocAccessibleChildBase;

  /**
   * Used to fire scrolling end event after page scroll.
   *
   * @param aTimer    [in] the timer object
   * @param aClosure  [in] the document accessible where scrolling happens
   */
  static void ScrollTimerCallback(nsITimer* aTimer, void* aClosure);

  void DispatchScrollingEvent(nsINode* aTarget, uint32_t aEventType);

  /**
   * Check if an id attribute change affects aria-activedescendant and handle
   * the aria-activedescendant change if appropriate.
   * If the currently focused element has aria-activedescendant and an
   * element's id changes to match this, the id was probably moved from the
   * previous active descendant, thus making this element the new active
   * descendant. In that case, accessible focus must be changed accordingly.
   */
  void ARIAActiveDescendantIDMaybeMoved(LocalAccessible* aAccessible);

  /**
   * Traverse content subtree and for each node do one of 3 things:
   * 1. Check if content node has an accessible that should be removed and
   *    remove it.
   * 2. Check if content node has an accessible that needs to be recreated.
   *    Remove it and schedule it for reinsertion.
   * 3. Check if content node has no accessible but needs one. Schedule one for
   *    insertion.
   *
   * Returns true if the root node should be reinserted.
   */
  bool PruneOrInsertSubtree(nsIContent* aRoot);

 protected:
  /**
   * State and property flags, kept by mDocFlags.
   */
  enum {
    // Whether the document is a top level content document in this process.
    eTopLevelContentDocInProcess = 1 << 0
  };

  /**
   * Cache of accessibles within this document accessible.
   */
  AccessibleHashtable mAccessibleCache;
  nsTHashMap<nsPtrHashKey<const nsINode>, LocalAccessible*>
      mNodeToAccessibleMap;

  Document* mDocumentNode;
  nsCOMPtr<nsITimer> mScrollWatchTimer;
  nsTHashMap<nsPtrHashKey<nsINode>, TimeStamp> mLastScrollingDispatch;

  /**
   * Bit mask of document load states (@see LoadState).
   */
  uint32_t mLoadState : 3;

  /**
   * Bit mask of other states and props.
   */
  uint32_t mDocFlags : 27;

  /**
   * Tracks whether we have seen changes to this document's content that
   * indicate we should re-send the viewport cache we use for hittesting.
   * This value is set in `BundleFieldsForCache` and processed in
   * `ProcessQueuedCacheUpdates`.
   */
  bool mViewportCacheDirty : 1;

  /**
   * Type of document load event fired after the document is loaded completely.
   */
  uint32_t mLoadEventType;

  /**
   * Reference to anchor jump element.
   */
  nsCOMPtr<nsIContent> mAnchorJumpElm;

  /**
   * A generic state (see items below) before the attribute value was changed.
   * @see AttributeWillChange and AttributeChanged notifications.
   */

  // Previous state bits before attribute change
  uint64_t mPrevStateBits;

  nsTArray<RefPtr<DocAccessible>> mChildDocuments;

  /**
   * The virtual cursor of the document.
   */
  RefPtr<nsAccessiblePivot> mVirtualCursor;

  /**
   * A storage class for pairing content with one of its relation attributes.
   */
  class AttrRelProvider {
   public:
    AttrRelProvider(nsAtom* aRelAttr, nsIContent* aContent)
        : mRelAttr(aRelAttr), mContent(aContent) {}

    nsAtom* mRelAttr;
    nsCOMPtr<nsIContent> mContent;

   private:
    AttrRelProvider();
    AttrRelProvider(const AttrRelProvider&);
    AttrRelProvider& operator=(const AttrRelProvider&);
  };

  typedef nsTArray<mozilla::UniquePtr<AttrRelProvider>> AttrRelProviders;
  typedef nsClassHashtable<nsStringHashKey, AttrRelProviders>
      DependentIDsHashtable;

  /**
   * Returns/creates/removes attribute relation providers associated with
   * a DOM document if the element is in uncomposed document or associated
   * with shadow DOM the element is in.
   */
  AttrRelProviders* GetRelProviders(dom::Element* aElement,
                                    const nsAString& aID) const;
  AttrRelProviders* GetOrCreateRelProviders(dom::Element* aElement,
                                            const nsAString& aID);
  void RemoveRelProvidersIfEmpty(dom::Element* aElement, const nsAString& aID);

  /**
   * The cache of IDs pointed by relation attributes.
   */
  nsClassHashtable<nsPtrHashKey<dom::DocumentOrShadowRoot>,
                   DependentIDsHashtable>
      mDependentIDsHashes;

  friend class RelatedAccIterator;

  /**
   * Used for our caching algorithm. We store the list of nodes that should be
   * invalidated.
   *
   * @see ProcessInvalidationList
   */
  nsTArray<RefPtr<nsIContent>> mInvalidationList;

  /**
   * Holds a list of aria-owns relocations.
   */
  nsClassHashtable<nsPtrHashKey<LocalAccessible>,
                   nsTArray<RefPtr<LocalAccessible>>>
      mARIAOwnsHash;

  /**
   * Keeps a list of pending subtrees to update post-refresh.
   */
  nsTArray<RefPtr<nsIContent>> mPendingUpdates;

  /**
   * Used to process notification from core and accessible events.
   */
  RefPtr<NotificationController> mNotificationController;
  friend class EventTree;
  friend class NotificationController;

 private:
  void SetRoleMapEntryForDoc(dom::Element* aElement);

  /**
   * This must be called whenever an Accessible is moved in a content process.
   * It keeps track of Accessibles moved during this tick.
   */
  void TrackMovedAccessible(LocalAccessible* aAcc);

  /**
   * For hidden subtrees, fire a name/description change event if the subtree
   * is a target of aria-labelledby/describedby.
   * This does nothing if it is called on a node which is not part of a hidden
   * aria-labelledby/describedby target.
   */
  void MaybeHandleChangeToHiddenNameOrDescription(nsIContent* aChild);

  PresShell* mPresShell;

  // Exclusively owned by IPDL so don't manually delete it!
  DocAccessibleChild* mIPCDoc;

  // A hash map between LocalAccessibles and CacheDomains, tracking
  // cache updates that have been queued during the current tick
  // but not yet sent. It is possible for this map to contain a reference
  // to the document it lives on. We clear the list in Shutdown() to
  // avoid cyclical references.
  nsTHashMap<RefPtr<LocalAccessible>, uint64_t> mQueuedCacheUpdates;

  // A set of Accessibles moved during this tick. Only used in content
  // processes.
  nsTHashSet<RefPtr<LocalAccessible>> mMovedAccessibles;
  // A set of Accessibles inserted during this tick. Only used in content
  // processes. This is needed to prevent insertions + moves of the same
  // Accessible in the same tick from being tracked as moves.
  nsTHashSet<RefPtr<LocalAccessible>> mInsertedAccessibles;
};

inline DocAccessible* LocalAccessible::AsDoc() {
  return IsDoc() ? static_cast<DocAccessible*>(this) : nullptr;
}

}  // namespace a11y
}  // namespace mozilla

#endif

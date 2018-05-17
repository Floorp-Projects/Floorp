/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_DocAccessible_h__
#define mozilla_a11y_DocAccessible_h__

#include "nsIAccessiblePivot.h"

#include "HyperTextAccessibleWrap.h"
#include "AccEvent.h"

#include "nsAutoPtr.h"
#include "nsClassHashtable.h"
#include "nsDataHashtable.h"
#include "nsIDocument.h"
#include "nsIDocumentObserver.h"
#include "nsIObserver.h"
#include "nsIScrollPositionListener.h"
#include "nsITimer.h"
#include "nsIWeakReference.h"

class nsAccessiblePivot;

const uint32_t kDefaultCacheLength = 128;

namespace mozilla {

class TextEditor;

namespace a11y {

class DocManager;
class NotificationController;
class DocAccessibleChild;
class RelatedAccIterator;
template<class Class, class ... Args>
class TNotification;

class DocAccessible : public HyperTextAccessibleWrap,
                      public nsIDocumentObserver,
                      public nsIObserver,
                      public nsIScrollPositionListener,
                      public nsSupportsWeakReference,
                      public nsIAccessiblePivotObserver
{
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DocAccessible, Accessible)

  NS_DECL_NSIOBSERVER
  NS_DECL_NSIACCESSIBLEPIVOTOBSERVER

public:

  DocAccessible(nsIDocument* aDocument, nsIPresShell* aPresShell);

  // nsIScrollPositionListener
  virtual void ScrollPositionWillChange(nscoord aX, nscoord aY) override {}
  virtual void ScrollPositionDidChange(nscoord aX, nscoord aY) override;

  // nsIDocumentObserver
  NS_DECL_NSIDOCUMENTOBSERVER

  // Accessible
  virtual void Init();
  virtual void Shutdown() override;
  virtual nsIFrame* GetFrame() const override;
  virtual nsINode* GetNode() const override { return mDocumentNode; }
  nsIDocument* DocumentNode() const { return mDocumentNode; }

  virtual mozilla::a11y::ENameValueFlag Name(nsString& aName) const override;
  virtual void Description(nsString& aDescription) override;
  virtual Accessible* FocusedChild() override;
  virtual mozilla::a11y::role NativeRole() const override;
  virtual uint64_t NativeState() const override;
  virtual uint64_t NativeInteractiveState() const override;
  virtual bool NativelyUnavailable() const override;
  virtual void ApplyARIAState(uint64_t* aState) const override;
  virtual already_AddRefed<nsIPersistentProperties> Attributes() override;

  virtual void TakeFocus() const override;

#ifdef A11Y_LOG
  virtual nsresult HandleAccEvent(AccEvent* aEvent) override;
#endif

  virtual nsRect RelativeBounds(nsIFrame** aRelativeFrame) const override;

  // HyperTextAccessible
  virtual already_AddRefed<TextEditor> GetEditor() const override;

  // DocAccessible

  /**
   * Return document URL.
   */
  void URL(nsAString& aURL) const;

  /**
   * Return DOM document title.
   */
  void Title(nsString& aTitle) const { mDocumentNode->GetTitle(aTitle); }

  /**
   * Return DOM document mime type.
   */
  void MimeType(nsAString& aType) const { mDocumentNode->GetContentType(aType); }

  /**
   * Return DOM document type.
   */
  void DocType(nsAString& aType) const;

  /**
   * Return virtual cursor associated with the document.
   */
  nsIAccessiblePivot* VirtualCursor();

  /**
   * Return presentation shell for this document accessible.
   */
  nsIPresShell* PresShell() const { return mPresShell; }

  /**
   * Return the presentation shell's context.
   */
  nsPresContext* PresContext() const { return mPresShell->GetPresContext(); }

  /**
   * Return true if associated DOM document was loaded and isn't unloading.
   */
  bool IsContentLoaded() const
  {
    // eDOMLoaded flag check is used for error pages as workaround to make this
    // method return correct result since error pages do not receive 'pageshow'
    // event and as consequence nsIDocument::IsShowing() returns false.
    return mDocumentNode && mDocumentNode->IsVisible() &&
      (mDocumentNode->IsShowing() || HasLoadState(eDOMLoaded));
  }

  bool IsHidden() const
  {
    return mDocumentNode->Hidden();
  }

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
  bool HasLoadState(LoadState aState) const
    { return (mLoadState & static_cast<uint32_t>(aState)) ==
        static_cast<uint32_t>(aState); }

  /**
   * Return a native window handler or pointer depending on platform.
   */
  virtual void* GetNativeWindow() const;

  /**
   * Return the parent document.
   */
  DocAccessible* ParentDocument() const
    { return mParent ? mParent->Document() : nullptr; }

  /**
   * Return the child document count.
   */
  uint32_t ChildDocumentCount() const
    { return mChildDocuments.Length(); }

  /**
   * Return the child document at the given index.
   */
  DocAccessible* GetChildDocumentAt(uint32_t aIndex) const
    { return mChildDocuments.SafeElementAt(aIndex, nullptr); }

  /**
   * Fire accessible event asynchronously.
   */
  void FireDelayedEvent(AccEvent* aEvent);
  void FireDelayedEvent(uint32_t aEventType, Accessible* aTarget);
  void FireEventsOnInsertion(Accessible* aContainer);

  /**
   * Fire value change event on the given accessible if applicable.
   */
  void MaybeNotifyOfValueChange(Accessible* aAccessible);

  /**
   * Get/set the anchor jump.
   */
  Accessible* AnchorJump()
    { return GetAccessibleOrContainer(mAnchorJumpElm); }

  void SetAnchorJump(nsIContent* aTargetNode)
    { mAnchorJumpElm = aTargetNode; }

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
  template<class Class, class Arg>
  void HandleNotification(Class* aInstance,
                          typename TNotification<Class, Arg>::Callback aMethod,
                          Arg* aArg);

  /**
   * Return the cached accessible by the given DOM node if it's in subtree of
   * this document accessible or the document accessible itself, otherwise null.
   *
   * @return the accessible object
   */
  Accessible* GetAccessible(nsINode* aNode) const
  {
    return aNode == mDocumentNode ?
      const_cast<DocAccessible*>(this) : mNodeToAccessibleMap.Get(aNode);
  }

  /**
   * Return an accessible for the given node even if the node is not in
   * document's node map cache (like HTML area element).
   *
   * XXX: it should be really merged with GetAccessible().
   */
  Accessible* GetAccessibleEvenIfNotInMap(nsINode* aNode) const;
  Accessible* GetAccessibleEvenIfNotInMapOrContainer(nsINode* aNode) const;

  /**
   * Return whether the given DOM node has an accessible or not.
   */
  bool HasAccessible(nsINode* aNode) const
    { return GetAccessible(aNode); }

  /**
   * Return the cached accessible by the given unique ID within this document.
   *
   * @note   the unique ID matches with the uniqueID() of Accessible
   *
   * @param  aUniqueID  [in] the unique ID used to cache the node.
   */
  Accessible* GetAccessibleByUniqueID(void* aUniqueID)
  {
    return UniqueID() == aUniqueID ?
      this : mAccessibleCache.GetWeak(aUniqueID);
  }

  /**
   * Return the cached accessible by the given unique ID looking through
   * this and nested documents.
   */
  Accessible* GetAccessibleByUniqueIDInSubtree(void* aUniqueID);

  /**
   * Return an accessible for the given DOM node or container accessible if
   * the node is not accessible.
   */
  Accessible* GetAccessibleOrContainer(nsINode* aNode) const;

  /**
   * Return a container accessible for the given DOM node.
   */
  Accessible* GetContainerAccessible(nsINode* aNode) const
  {
    return aNode ? GetAccessibleOrContainer(aNode->GetParentNode()) : nullptr;
  }

  /**
   * Return an accessible for the given node if any, or an immediate accessible
   * container for it.
   */
  Accessible* AccessibleOrTrueContainer(nsINode* aNode) const;

  /**
   * Return an accessible for the given node or its first accessible descendant.
   */
  Accessible* GetAccessibleOrDescendant(nsINode* aNode) const;

  /**
   * Returns aria-owns seized child at the given index.
   */
  Accessible* ARIAOwnedAt(Accessible* aParent, uint32_t aIndex) const
  {
    nsTArray<RefPtr<Accessible> >* children = mARIAOwnsHash.Get(aParent);
    if (children) {
      return children->SafeElementAt(aIndex);
    }
    return nullptr;
  }
  uint32_t ARIAOwnedCount(Accessible* aParent) const
  {
    nsTArray<RefPtr<Accessible> >* children = mARIAOwnsHash.Get(aParent);
    return children ? children->Length() : 0;
  }

  /**
   * Return true if the given ID is referred by relation attribute.
   *
   * @note Different elements may share the same ID if they are hosted inside
   *       XBL bindings. Be careful the result of this method may be  senseless
   *       while it's called for XUL elements (where XBL is used widely).
   */
  bool IsDependentID(const nsAString& aID) const
    { return mDependentIDsHash.Get(aID, nullptr); }

  /**
   * Initialize the newly created accessible and put it into document caches.
   *
   * @param  aAccessible    [in] created accessible
   * @param  aRoleMapEntry  [in] the role map entry role the ARIA role or nullptr
   *                          if none
   */
  void BindToDocument(Accessible* aAccessible,
                      const nsRoleMapEntry* aRoleMapEntry);

  /**
   * Remove from document and shutdown the given accessible.
   */
  void UnbindFromDocument(Accessible* aAccessible);

  /**
   * Notify the document accessible that content was inserted.
   */
  void ContentInserted(nsIContent* aContainerNode,
                       nsIContent* aStartChildNode,
                       nsIContent* aEndChildNode);

  /**
   * Update the tree on content removal.
   */
  void ContentRemoved(Accessible* aAccessible);
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
   * Add/remove scroll listeners, @see nsIScrollPositionListener interface.
   */
  void AddScrollListener();
  void RemoveScrollListener();

  /**
   * Append the given document accessible to this document's child document
   * accessibles.
   */
  bool AppendChildDocument(DocAccessible* aChildDocument)
  {
    return mChildDocuments.AppendElement(aChildDocument);
  }

  /**
   * Remove the given document accessible from this document's child document
   * accessibles.
   */
  void RemoveChildDocument(DocAccessible* aChildDocument)
  {
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
  void AddDependentIDsFor(Accessible* aRelProvider,
                          nsAtom* aRelAttr = nullptr);

  /**
   * Remove dependent IDs pointed by accessible element by relation attribute
   * from cache. If the relation attribute is absent then all relation
   * attributes are checked.
   *
   * @param aRelProvider [in] accessible that element has relation attribute
   * @param aRelAttr     [in, optional] relation attribute
   */
  void RemoveDependentIDsFor(Accessible* aRelProvider,
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
   * Fire accessible events when attribute is changed.
   *
   * @param aAccessible   [in] accessible the DOM attribute is changed for
   * @param aNameSpaceID  [in] namespace of changed attribute
   * @param aAttribute    [in] changed attribute
   */
  void AttributeChangedImpl(Accessible* aAccessible,
                            int32_t aNameSpaceID, nsAtom* aAttribute);

  /**
   * Fire accessible events when ARIA attribute is changed.
   *
   * @param aAccessible  [in] accesislbe the DOM attribute is changed for
   * @param aAttribute   [in] changed attribute
   */
  void ARIAAttributeChanged(Accessible* aAccessible, nsAtom* aAttribute);

  /**
   * Process ARIA active-descendant attribute change.
   */
  void ARIAActiveDescendantChanged(Accessible* aAccessible);

  /**
   * Update the accessible tree for inserted content.
   */
  void ProcessContentInserted(Accessible* aContainer,
                              const nsTArray<nsCOMPtr<nsIContent> >* aInsertedContent);
  void ProcessContentInserted(Accessible* aContainer,
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
   * Steals or puts back accessible subtrees.
   */
  void DoARIAOwnsRelocation(Accessible* aOwner);

  /**
   * Moves children back under their original parents.
   */
  void PutChildrenBack(nsTArray<RefPtr<Accessible> >* aChildren,
                       uint32_t aStartIdx);

  bool MoveChild(Accessible* aChild, Accessible* aNewParent,
                 int32_t aIdxInParent);

  /**
   * Create accessible tree.
   *
   * @param aRoot       [in] a root of subtree to create
   * @param aFocusedAcc [in, optional] a focused accessible under created
   *                      subtree if any
   */
  void CacheChildrenInSubtree(Accessible* aRoot,
                              Accessible** aFocusedAcc = nullptr);
  void CreateSubtree(Accessible* aRoot);

  /**
   * Remove accessibles in subtree from node to accessible map.
   */
  void UncacheChildrenInSubtree(Accessible* aRoot);

  /**
   * Shutdown any cached accessible in the subtree.
   *
   * @param aAccessible  [in] the root of the subrtee to invalidate accessible
   *                      child/parent refs in
   */
  void ShutdownChildrenInSubtree(Accessible* aAccessible);

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
  void SetIPCDoc(DocAccessibleChild* aIPCDoc) { mIPCDoc = aIPCDoc; }

  friend class DocAccessibleChildBase;

  /**
   * Used to fire scrolling end event after page scroll.
   *
   * @param aTimer    [in] the timer object
   * @param aClosure  [in] the document accessible where scrolling happens
   */
  static void ScrollTimerCallback(nsITimer* aTimer, void* aClosure);

protected:

  /**
   * State and property flags, kept by mDocFlags.
   */
  enum {
    // Whether scroll listeners were added.
    eScrollInitialized = 1 << 0,

    // Whether the document is a tab document.
    eTabDocument = 1 << 1
  };

  /**
   * Cache of accessibles within this document accessible.
   */
  AccessibleHashtable mAccessibleCache;
  nsDataHashtable<nsPtrHashKey<const nsINode>, Accessible*>
    mNodeToAccessibleMap;

  nsIDocument* mDocumentNode;
    nsCOMPtr<nsITimer> mScrollWatchTimer;
    uint16_t mScrollPositionChangedTicks; // Used for tracking scroll events

  /**
   * Bit mask of document load states (@see LoadState).
   */
  uint32_t mLoadState : 3;

  /**
   * Bit mask of other states and props.
   */
  uint32_t mDocFlags : 28;

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
  union {
    // ARIA attribute value
    nsAtom* mARIAAttrOldValue;

    // True if the accessible state bit was on
    bool mStateBitWasOn;
  };

  nsTArray<RefPtr<DocAccessible> > mChildDocuments;

  /**
   * The virtual cursor of the document.
   */
  RefPtr<nsAccessiblePivot> mVirtualCursor;

  /**
   * A storage class for pairing content with one of its relation attributes.
   */
  class AttrRelProvider
  {
  public:
    AttrRelProvider(nsAtom* aRelAttr, nsIContent* aContent) :
      mRelAttr(aRelAttr), mContent(aContent) { }

    nsAtom* mRelAttr;
    nsCOMPtr<nsIContent> mContent;

  private:
    AttrRelProvider();
    AttrRelProvider(const AttrRelProvider&);
    AttrRelProvider& operator =(const AttrRelProvider&);
  };

  /**
   * The cache of IDs pointed by relation attributes.
   */
  typedef nsTArray<nsAutoPtr<AttrRelProvider> > AttrRelProviderArray;
  nsClassHashtable<nsStringHashKey, AttrRelProviderArray>
    mDependentIDsHash;

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
  nsClassHashtable<nsPtrHashKey<Accessible>, nsTArray<RefPtr<Accessible> > >
    mARIAOwnsHash;

  /**
   * Used to process notification from core and accessible events.
   */
  RefPtr<NotificationController> mNotificationController;
  friend class EventTree;
  friend class NotificationController;

private:

  nsIPresShell* mPresShell;

  // Exclusively owned by IPDL so don't manually delete it!
  DocAccessibleChild* mIPCDoc;
};

inline DocAccessible*
Accessible::AsDoc()
{
  return IsDoc() ? static_cast<DocAccessible*>(this) : nullptr;
}

} // namespace a11y
} // namespace mozilla

#endif

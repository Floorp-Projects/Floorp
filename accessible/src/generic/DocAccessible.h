/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_DocAccessible_h__
#define mozilla_a11y_DocAccessible_h__

#include "nsIAccessibleCursorable.h"
#include "nsIAccessibleDocument.h"
#include "nsIAccessiblePivot.h"

#include "AccEvent.h"
#include "HyperTextAccessibleWrap.h"

#include "nsClassHashtable.h"
#include "nsDataHashtable.h"
#include "nsIDocument.h"
#include "nsIDocumentObserver.h"
#include "nsIEditor.h"
#include "nsIObserver.h"
#include "nsIScrollPositionListener.h"
#include "nsITimer.h"
#include "nsIWeakReference.h"
#include "nsIDocShellTreeNode.h"

class nsAccessiblePivot;

class nsIScrollableView;

const uint32_t kDefaultCacheSize = 256;

namespace mozilla {
namespace a11y {

class DocManager;
class NotificationController;
class RelatedAccIterator;
template<class Class, class Arg>
class TNotification;

class DocAccessible : public HyperTextAccessibleWrap,
                      public nsIAccessibleDocument,
                      public nsIDocumentObserver,
                      public nsIObserver,
                      public nsIScrollPositionListener,
                      public nsSupportsWeakReference,
                      public nsIAccessibleCursorable,
                      public nsIAccessiblePivotObserver
{
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(DocAccessible, Accessible)

  NS_DECL_NSIACCESSIBLEDOCUMENT

  NS_DECL_NSIOBSERVER

  NS_DECL_NSIACCESSIBLECURSORABLE

  NS_DECL_NSIACCESSIBLEPIVOTOBSERVER

public:

  DocAccessible(nsIDocument* aDocument, nsIContent* aRootContent,
                nsIPresShell* aPresShell);
  virtual ~DocAccessible();

  // nsIAccessible
  NS_IMETHOD TakeFocus(void);

  // nsIScrollPositionListener
  virtual void ScrollPositionWillChange(nscoord aX, nscoord aY) {}
  virtual void ScrollPositionDidChange(nscoord aX, nscoord aY);

  // nsIDocumentObserver
  NS_DECL_NSIDOCUMENTOBSERVER

  // nsAccessNode
  virtual void Init();
  virtual void Shutdown();
  virtual nsIFrame* GetFrame() const;
  virtual nsINode* GetNode() const { return mDocumentNode; }
  nsIDocument* DocumentNode() const { return mDocumentNode; }

  // Accessible
  virtual mozilla::a11y::ENameValueFlag Name(nsString& aName);
  virtual void Description(nsString& aDescription);
  virtual Accessible* FocusedChild();
  virtual mozilla::a11y::role NativeRole();
  virtual uint64_t NativeState();
  virtual uint64_t NativeInteractiveState() const;
  virtual bool NativelyUnavailable() const;
  virtual void ApplyARIAState(uint64_t* aState) const;
  virtual already_AddRefed<nsIPersistentProperties> Attributes();

#ifdef A11Y_LOG
  virtual nsresult HandleAccEvent(AccEvent* aEvent);
#endif

  virtual void GetBoundsRect(nsRect& aRect, nsIFrame** aRelativeFrame);

  // HyperTextAccessible
  virtual already_AddRefed<nsIEditor> GetEditor() const;

  // DocAccessible

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
  Accessible* GetAccessible(nsINode* aNode) const;

  /**
   * Return whether the given DOM node has an accessible or not.
   */
  bool HasAccessible(nsINode* aNode) const
    { return GetAccessible(aNode); }

  /**
   * Return the cached accessible by the given unique ID within this document.
   *
   * @note   the unique ID matches with the uniqueID() of nsAccessNode
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
  Accessible* GetAccessibleOrContainer(nsINode* aNode);

  /**
   * Return a container accessible for the given DOM node.
   */
  Accessible* GetContainerAccessible(nsINode* aNode)
  {
    return aNode ? GetAccessibleOrContainer(aNode->GetParentNode()) : nullptr;
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
  bool BindToDocument(Accessible* aAccessible, nsRoleMapEntry* aRoleMapEntry);

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
   * Notify the document accessible that content was removed.
   */
  void ContentRemoved(nsIContent* aContainerNode, nsIContent* aChildNode);

  /**
   * Updates accessible tree when rendered text is changed.
   */
  void UpdateText(nsIContent* aTextNode);

  /**
   * Recreate an accessible, results in hide/show events pair.
   */
  void RecreateAccessible(nsIContent* aContent);

protected:

  void LastRelease();

  // Accessible
  virtual void CacheChildren();

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
  void AddDependentIDsFor(dom::Element* aRelProviderElm,
                          nsIAtom* aRelAttr = nullptr);

  /**
   * Remove dependent IDs pointed by accessible element by relation attribute
   * from cache. If the relation attribute is absent then all relation
   * attributes are checked.
   *
   * @param aRelProvider [in] accessible that element has relation attribute
   * @param aRelAttr     [in, optional] relation attribute
   */
  void RemoveDependentIDsFor(dom::Element* aRelProviderElm,
                             nsIAtom* aRelAttr = nullptr);

  /**
   * Update or recreate an accessible depending on a changed attribute.
   *
   * @param aElement   [in] the element the attribute was changed on
   * @param aAttribute [in] the changed attribute
   * @return            true if an action was taken on the attribute change
   */
  bool UpdateAccessibleOnAttrChange(mozilla::dom::Element* aElement,
                                    nsIAtom* aAttribute);

  /**
   * Fire accessible events when attribute is changed.
   *
   * @param aAccessible   [in] accessible the DOM attribute is changed for
   * @param aNameSpaceID  [in] namespace of changed attribute
   * @param aAttribute    [in] changed attribute
   */
  void AttributeChangedImpl(Accessible* aAccessible,
                            int32_t aNameSpaceID, nsIAtom* aAttribute);

  /**
   * Fire accessible events when ARIA attribute is changed.
   *
   * @param aAccessible  [in] accesislbe the DOM attribute is changed for
   * @param aAttribute   [in] changed attribute
   */
  void ARIAAttributeChanged(Accessible* aAccessible, nsIAtom* aAttribute);

  /**
   * Process ARIA active-descendant attribute change.
   */
  void ARIAActiveDescendantChanged(Accessible* aAccessible);

  /**
   * Update the accessible tree for inserted content.
   */
  void ProcessContentInserted(Accessible* aContainer,
                              const nsTArray<nsCOMPtr<nsIContent> >* aInsertedContent);

  /**
   * Used to notify the document to make it process the invalidation list.
   *
   * While children are cached we may encounter the case there's no accessible
   * for referred content by related accessible. Store these related nodes to
   * invalidate their containers later.
   */
  void ProcessInvalidationList();

  /**
   * Update the accessible tree for content insertion or removal.
   */
  void UpdateTree(Accessible* aContainer, nsIContent* aChildNode,
                  bool aIsInsert);

  /**
   * Helper for UpdateTree() method. Go down to DOM subtree and updates
   * accessible tree. Return one of these flags.
   */
  enum EUpdateTreeFlags {
    eNoAccessible = 0,
    eAccessible = 1,
    eAlertAccessible = 2
  };

  uint32_t UpdateTreeInternal(Accessible* aChild, bool aIsInsert,
                              AccReorderEvent* aReorderEvent);

  /**
   * Create accessible tree.
   */
  void CacheChildrenInSubtree(Accessible* aRoot);

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

    // Whether we support nsIAccessibleCursorable.
    eCursorable = 1 << 1,

    // Whether the document is a tab document.
    eTabDocument = 1 << 2
  };

  /**
   * Cache of accessibles within this document accessible.
   */
  AccessibleHashtable mAccessibleCache;
  nsDataHashtable<nsPtrHashKey<const nsINode>, Accessible*>
    mNodeToAccessibleMap;

    nsCOMPtr<nsIDocument> mDocumentNode;
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
   * Keep the ARIA attribute old value that is initialized by
   * AttributeWillChange and used by AttributeChanged notifications.
   */
  nsIAtom* mARIAAttrOldValue;

  nsTArray<nsRefPtr<DocAccessible> > mChildDocuments;

  /**
   * The virtual cursor of the document when it supports nsIAccessibleCursorable.
   */
  nsRefPtr<nsAccessiblePivot> mVirtualCursor;

  /**
   * A storage class for pairing content with one of its relation attributes.
   */
  class AttrRelProvider
  {
  public:
    AttrRelProvider(nsIAtom* aRelAttr, nsIContent* aContent) :
      mRelAttr(aRelAttr), mContent(aContent) { }

    nsIAtom* mRelAttr;
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
  nsClassHashtable<nsStringHashKey, AttrRelProviderArray> mDependentIDsHash;

  friend class RelatedAccIterator;

  /**
   * Used for our caching algorithm. We store the list of nodes that should be
   * invalidated.
   *
   * @see ProcessInvalidationList
   */
  nsTArray<nsIContent*> mInvalidationList;

  /**
   * Used to process notification from core and accessible events.
   */
  nsRefPtr<NotificationController> mNotificationController;
  friend class NotificationController;

private:

  nsIPresShell* mPresShell;
};

inline DocAccessible*
Accessible::AsDoc()
{
  return IsDoc() ? static_cast<DocAccessible*>(this) : nullptr;
}

} // namespace a11y
} // namespace mozilla

#endif

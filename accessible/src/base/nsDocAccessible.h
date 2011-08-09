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

#ifndef _nsDocAccessible_H_
#define _nsDocAccessible_H_

#include "nsIAccessibleDocument.h"

#include "nsEventShell.h"
#include "nsHyperTextAccessibleWrap.h"
#include "NotificationController.h"

#include "nsClassHashtable.h"
#include "nsDataHashtable.h"
#include "nsIDocument.h"
#include "nsIDocumentObserver.h"
#include "nsIEditor.h"
#include "nsIObserver.h"
#include "nsIScrollPositionListener.h"
#include "nsITimer.h"
#include "nsIWeakReference.h"
#include "nsCOMArray.h"
#include "nsIDocShellTreeNode.h"

class nsIScrollableView;

const PRUint32 kDefaultCacheSize = 256;

#define NS_DOCACCESSIBLE_IMPL_CID                       \
{  /* 5641921c-a093-4292-9dca-0b51813db57d */           \
  0x5641921c,                                           \
  0xa093,                                               \
  0x4292,                                               \
  { 0x9d, 0xca, 0x0b, 0x51, 0x81, 0x3d, 0xb5, 0x7d }    \
}

class nsDocAccessible : public nsHyperTextAccessibleWrap,
                        public nsIAccessibleDocument,
                        public nsIDocumentObserver,
                        public nsIObserver,
                        public nsIScrollPositionListener,
                        public nsSupportsWeakReference
{  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDocAccessible, nsAccessible)

  NS_DECL_NSIACCESSIBLEDOCUMENT
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_DOCACCESSIBLE_IMPL_CID)

  NS_DECL_NSIOBSERVER

public:
  using nsAccessible::GetParent;

  nsDocAccessible(nsIDocument *aDocument, nsIContent *aRootContent,
                  nsIWeakReference* aShell);
  virtual ~nsDocAccessible();

  // nsIAccessible
  NS_IMETHOD GetName(nsAString& aName);
  NS_IMETHOD GetAttributes(nsIPersistentProperties **aAttributes);
  NS_IMETHOD TakeFocus(void);

  // nsIScrollPositionListener
  virtual void ScrollPositionWillChange(nscoord aX, nscoord aY) {}
  virtual void ScrollPositionDidChange(nscoord aX, nscoord aY);

  // nsIDocumentObserver
  NS_DECL_NSIDOCUMENTOBSERVER

  // nsAccessNode
  virtual PRBool Init();
  virtual void Shutdown();
  virtual nsIFrame* GetFrame() const;
  virtual bool IsDefunct() const;
  virtual nsINode* GetNode() const { return mDocument; }
  virtual nsIDocument* GetDocumentNode() const { return mDocument; }

  // nsAccessible
  virtual void Description(nsString& aDescription);
  virtual nsAccessible* FocusedChild();
  virtual PRUint32 NativeRole();
  virtual PRUint64 NativeState();
  virtual void ApplyARIAState(PRUint64* aState);

  virtual void SetRoleMapEntry(nsRoleMapEntry* aRoleMapEntry);

#ifdef DEBUG_ACCDOCMGR
  virtual nsresult HandleAccEvent(AccEvent* aAccEvent);
#endif

  // nsIAccessibleText
  NS_IMETHOD GetAssociatedEditor(nsIEditor **aEditor);

  // nsDocAccessible

  /**
   * Return true if associated DOM document was loaded and isn't unloading.
   */
  PRBool IsContentLoaded() const
  {
    // eDOMLoaded flag check is used for error pages as workaround to make this
    // method return correct result since error pages do not receive 'pageshow'
    // event and as consequence nsIDocument::IsShowing() returns false.
    return mDocument && mDocument->IsVisible() &&
      (mDocument->IsShowing() || HasLoadState(eDOMLoaded));
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
    { return (mLoadState & aState) == aState; }

  /**
   * Return a native window handler or pointer depending on platform.
   */
  virtual void* GetNativeWindow() const;

  /**
   * Return the parent document.
   */
  nsDocAccessible* ParentDocument() const
    { return mParent ? mParent->GetDocAccessible() : nsnull; }

  /**
   * Return the child document count.
   */
  PRUint32 ChildDocumentCount() const
    { return mChildDocuments.Length(); }

  /**
   * Return the child document at the given index.
   */
  nsDocAccessible* GetChildDocumentAt(PRUint32 aIndex) const
    { return mChildDocuments.SafeElementAt(aIndex, nsnull); }

  /**
   * Non-virtual method to fire a delayed event after a 0 length timeout.
   *
   * @param aEventType   [in] the nsIAccessibleEvent event type
   * @param aDOMNode     [in] DOM node the accesible event should be fired for
   * @param aAllowDupes  [in] rule to process an event (see EEventRule constants)
   */
  nsresult FireDelayedAccessibleEvent(PRUint32 aEventType, nsINode *aNode,
                                      AccEvent::EEventRule aAllowDupes = AccEvent::eRemoveDupes,
                                      EIsFromUserInput aIsFromUserInput = eAutoDetect);

  /**
   * Fire accessible event after timeout.
   *
   * @param aEvent  [in] the event to fire
   */
  nsresult FireDelayedAccessibleEvent(AccEvent* aEvent);

  /**
   * Handle anchor jump when page is loaded.
   */
  inline void HandleAnchorJump(nsIContent* aTargetNode)
  {
    HandleNotification<nsDocAccessible, nsIContent>
      (this, &nsDocAccessible::ProcessAnchorJump, aTargetNode);
  }

  /**
   * Bind the child document to the tree.
   */
  inline void BindChildDocument(nsDocAccessible* aDocument)
  {
    mNotificationController->ScheduleChildDocBinding(aDocument);
  }

  /**
   * Process the generic notification.
   *
   * @note  The caller must guarantee that the given instance still exists when
   *          notification is processed.
   * @see   NotificationController::HandleNotification
   */
  template<class Class, class Arg>
  inline void HandleNotification(Class* aInstance,
                                 typename TNotification<Class, Arg>::Callback aMethod,
                                 Arg* aArg)
  {
    if (mNotificationController) {
      mNotificationController->HandleNotification<Class, Arg>(aInstance,
                                                              aMethod, aArg);
    }
  }

  /**
   * Return the cached accessible by the given DOM node if it's in subtree of
   * this document accessible or the document accessible itself, otherwise null.
   *
   * @return the accessible object
   */
  nsAccessible* GetAccessible(nsINode* aNode) const;

  /**
   * Return whether the given DOM node has an accessible or not.
   */
  inline bool HasAccessible(nsINode* aNode) const
    { return GetAccessible(aNode); }

  /**
   * Return true if the given accessible is in document.
   */
  inline bool IsInDocument(nsAccessible* aAccessible) const
  {
    nsAccessible* acc = aAccessible;
    while (acc && !acc->IsPrimaryForNode())
      acc = acc->Parent();

    return acc ? mNodeToAccessibleMap.Get(acc->GetNode()) : false;
  }

  /**
   * Return the cached accessible by the given unique ID within this document.
   *
   * @note   the unique ID matches with the uniqueID() of nsAccessNode
   *
   * @param  aUniqueID  [in] the unique ID used to cache the node.
   */
  inline nsAccessible* GetAccessibleByUniqueID(void* aUniqueID)
  {
    return UniqueID() == aUniqueID ?
      this : mAccessibleCache.GetWeak(aUniqueID);
  }

  /**
   * Return the cached accessible by the given unique ID looking through
   * this and nested documents.
   */
  nsAccessible* GetAccessibleByUniqueIDInSubtree(void* aUniqueID);

  /**
   * Return an accessible for the given DOM node or container accessible if
   * the node is not accessible.
   */
  nsAccessible* GetAccessibleOrContainer(nsINode* aNode);

  /**
   * Return a container accessible for the given DOM node.
   */
  inline nsAccessible* GetContainerAccessible(nsINode* aNode)
  {
    return aNode ? GetAccessibleOrContainer(aNode->GetNodeParent()) : nsnull;
  }

  /**
   * Return true if the given ID is referred by relation attribute.
   *
   * @note Different elements may share the same ID if they are hosted inside
   *       XBL bindings. Be careful the result of this method may be  senseless
   *       while it's called for XUL elements (where XBL is used widely).
   */
  PRBool IsDependentID(const nsAString& aID) const
    { return mDependentIDsHash.Get(aID, nsnull); }

  /**
   * Initialize the newly created accessible and put it into document caches.
   *
   * @param  aAccessible    [in] created accessible
   * @param  aRoleMapEntry  [in] the role map entry role the ARIA role or nsnull
   *                          if none
   */
  bool BindToDocument(nsAccessible* aAccessible, nsRoleMapEntry* aRoleMapEntry);

  /**
   * Remove from document and shutdown the given accessible.
   */
  void UnbindFromDocument(nsAccessible* aAccessible);

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
  inline void UpdateText(nsIContent* aTextNode)
  {
    NS_ASSERTION(mNotificationController, "The document was shut down!");

    // Ignore the notification if initial tree construction hasn't been done yet.
    if (mNotificationController && HasLoadState(eTreeConstructed))
      mNotificationController->ScheduleTextUpdate(aTextNode);
  }

  /**
   * Recreate an accessible, results in hide/show events pair.
   */
  void RecreateAccessible(nsIContent* aContent);

protected:

  // nsAccessible
  virtual void CacheChildren();

  // nsDocAccessible
    virtual void GetBoundsRect(nsRect& aRect, nsIFrame** aRelativeFrame);
    virtual nsresult AddEventListeners();
    virtual nsresult RemoveEventListeners();

  /**
   * Marks this document as loaded or loading.
   */
  inline void NotifyOfLoad(PRUint32 aLoadEventType)
  {
    mLoadState |= eDOMLoaded;
    mLoadEventType = aLoadEventType;
  }

  void NotifyOfLoading(bool aIsReloading);

  friend class nsAccDocManager;

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

    void AddScrollListener();
    void RemoveScrollListener();

  /**
   * Append the given document accessible to this document's child document
   * accessibles.
   */
  bool AppendChildDocument(nsDocAccessible* aChildDocument)
  {
    return mChildDocuments.AppendElement(aChildDocument);
  }

  /**
   * Remove the given document accessible from this document's child document
   * accessibles.
   */
  void RemoveChildDocument(nsDocAccessible* aChildDocument)
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
  void AddDependentIDsFor(nsAccessible* aRelProvider,
                          nsIAtom* aRelAttr = nsnull);

  /**
   * Remove dependent IDs pointed by accessible element by relation attribute
   * from cache. If the relation attribute is absent then all relation
   * attributes are checked.
   *
   * @param aRelProvider [in] accessible that element has relation attribute
   * @param aRelAttr     [in, optional] relation attribute
   */
  void RemoveDependentIDsFor(nsAccessible* aRelProvider,
                             nsIAtom* aRelAttr = nsnull);

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
     * Fires accessible events when attribute is changed.
     *
     * @param aContent - node that attribute is changed for
     * @param aNameSpaceID - namespace of changed attribute
     * @param aAttribute - changed attribute
     */
    void AttributeChangedImpl(nsIContent* aContent, PRInt32 aNameSpaceID, nsIAtom* aAttribute);

    /**
     * Fires accessible events when ARIA attribute is changed.
     *
     * @param aContent - node that attribute is changed for
     * @param aAttribute - changed attribute
     */
    void ARIAAttributeChanged(nsIContent* aContent, nsIAtom* aAttribute);

  /**
   * Process the event when the queue of pending events is untwisted. Fire
   * accessible events as result of the processing.
   */
  void ProcessPendingEvent(AccEvent* aEvent);

  /**
   * Process anchor jump notification and fire scrolling end event.
   */
  void ProcessAnchorJump(nsIContent* aTargetNode);

  /**
   * Update the accessible tree for inserted content.
   */
  void ProcessContentInserted(nsAccessible* aContainer,
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
  void UpdateTree(nsAccessible* aContainer, nsIContent* aChildNode,
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

  PRUint32 UpdateTreeInternal(nsAccessible* aChild, bool aIsInsert);

  /**
   * Create accessible tree.
   */
  void CacheChildrenInSubtree(nsAccessible* aRoot);

  /**
   * Remove accessibles in subtree from node to accessible map.
   */
  void UncacheChildrenInSubtree(nsAccessible* aRoot);

  /**
   * Shutdown any cached accessible in the subtree.
   *
   * @param aAccessible  [in] the root of the subrtee to invalidate accessible
   *                      child/parent refs in
   */
  void ShutdownChildrenInSubtree(nsAccessible *aAccessible);

  /**
   * Return true if accessibility events accompanying document accessible
   * loading should be fired.
   *
   * The rules are: do not fire events for root chrome document accessibles and
   * for sub document accessibles (like HTML frame of iframe) of the loading
   * document accessible.
   *
   * XXX: in general AT expect events for document accessible loading into
   * tabbrowser, events from other document accessibles may break AT. We need to
   * figure out what AT wants to know about loading page (for example, some of
   * them have separate processing of iframe documents on the page and therefore
   * they need a way to distinguish sub documents from page document). Ideally
   * we should make events firing for any loaded document and provide additional
   * info AT are needing.
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
   * Cache of accessibles within this document accessible.
   */
  nsAccessibleHashtable mAccessibleCache;
  nsDataHashtable<nsPtrHashKey<const nsINode>, nsAccessible*>
    mNodeToAccessibleMap;

    nsCOMPtr<nsIDocument> mDocument;
    nsCOMPtr<nsITimer> mScrollWatchTimer;
    PRUint16 mScrollPositionChangedTicks; // Used for tracking scroll events

  /**
   * Bit mask of document load states (@see LoadState).
   */
  PRUint32 mLoadState;

  /**
   * Type of document load event fired after the document is loaded completely.
   */
  PRUint32 mLoadEventType;

  static PRUint64 gLastFocusedAccessiblesState;

  nsTArray<nsRefPtr<nsDocAccessible> > mChildDocuments;

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
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsDocAccessible,
                              NS_DOCACCESSIBLE_IMPL_CID)

inline nsDocAccessible*
nsAccessible::AsDoc()
{
  return mFlags & eDocAccessible ?
    static_cast<nsDocAccessible*>(this) : nsnull;
}

#endif

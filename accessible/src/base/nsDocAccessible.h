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

#include "nsHyperTextAccessibleWrap.h"
#include "nsEventShell.h"

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
  NS_IMETHOD GetDescription(nsAString& aDescription);
  NS_IMETHOD GetAttributes(nsIPersistentProperties **aAttributes);
  NS_IMETHOD GetFocusedChild(nsIAccessible **aFocusedChild);
  NS_IMETHOD TakeFocus(void);

  // nsIScrollPositionListener
  virtual void ScrollPositionWillChange(nscoord aX, nscoord aY) {}
  virtual void ScrollPositionDidChange(nscoord aX, nscoord aY);

  // nsIDocumentObserver
  NS_DECL_NSIDOCUMENTOBSERVER

  // nsAccessNode
  virtual PRBool Init();
  virtual void Shutdown();
  virtual nsIFrame* GetFrame();
  virtual PRBool IsDefunct();
  virtual nsINode* GetNode() const { return mDocument; }

  // nsAccessible
  virtual PRUint32 NativeRole();
  virtual nsresult GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState);
  virtual nsresult GetARIAState(PRUint32 *aState, PRUint32 *aExtraState);

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
    return mDocument && mDocument->IsVisible() &&
      (mDocument->IsShowing() || mIsLoaded);
  }

  /**
   * Marks as loaded, used for error pages as workaround since they do not
   * receive pageshow event and as consequence nsIDocument::IsShowing() returns
   * false.
   */
  void MarkAsLoaded() { mIsLoaded = PR_TRUE; }

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
   * @param aIsAsynch    [in] set to PR_TRUE if this is not being called from
   *                      code synchronous with a DOM event
   */
  nsresult FireDelayedAccessibleEvent(PRUint32 aEventType, nsINode *aNode,
                                      AccEvent::EEventRule aAllowDupes = AccEvent::eRemoveDupes,
                                      PRBool aIsAsynch = PR_FALSE,
                                      EIsFromUserInput aIsFromUserInput = eAutoDetect);

  /**
   * Fire accessible event after timeout.
   *
   * @param aEvent  [in] the event to fire
   */
  nsresult FireDelayedAccessibleEvent(AccEvent* aEvent);

  /**
   * Find the accessible object in the accessibility cache that corresponds to
   * the given node or the first ancestor of it that has an accessible object
   * associated with it. Clear that accessible object's parent's cache of
   * accessible children and remove the accessible object and any descendants
   * from the accessible cache. Fires proper events. New accessible objects will
   * be created and cached again on demand.
   *
   * @param aContent  [in] the child that is changing
   * @param aEvent    [in] the event from nsIAccessibleEvent that caused
   *                   the change.
   */
  void InvalidateCacheSubtree(nsIContent *aContent, PRUint32 aEvent);

  /**
   * Return the cached accessible by the given unique ID if it's in subtree of
   * this document accessible or the document accessible itself, otherwise null.
   *
   * @note   the unique ID matches with the uniqueID attribute on nsIAccessNode
   *
   * @param  aUniqueID  [in] the unique ID used to cache the node.
   *
   * @return the accessible object
   */
  nsAccessible* GetCachedAccessible(void *aUniqueID);

  /**
   * Return the cached accessible by the given unique ID looking through
   * this and nested documents.
   */
  nsAccessible* GetCachedAccessibleInSubtree(void* aUniqueID);

  /**
   * Cache the accessible.
   *
   * @param  aUniquID     [in] the unique identifier of accessible
   * @param  aAccessible  [in] accessible to cache
   *
   * @return true if accessible being cached, otherwise false
   */
  PRBool CacheAccessible(void *aUniqueID, nsAccessible *aAccessible);

  /**
   * Remove the given accessible from document cache.
   */
  void RemoveAccessNodeFromCache(nsAccessible *aAccessible);

  /**
   * Process the event when the queue of pending events is untwisted. Fire
   * accessible events as result of the processing.
   */
  void ProcessPendingEvent(AccEvent* aEvent);

protected:

    virtual void GetBoundsRect(nsRect& aRect, nsIFrame** aRelativeFrame);
    virtual nsresult AddEventListeners();
    virtual nsresult RemoveEventListeners();
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
   * Invalidate parent-child relations for any cached accessible in the DOM
   * subtree. Accessible objects aren't destroyed.
   *
   * @param aStartNode  [in] the root of the subrtee to invalidate accessible
   *                      child/parent refs in
   */
  void InvalidateChildrenInSubtree(nsINode *aStartNode);

  /**
   * Traverse through DOM tree and shutdown accessible objects.
   */
  void RefreshNodes(nsINode *aStartNode);

    static void ScrollTimerCallback(nsITimer *aTimer, void *aClosure);

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
     * Fire text changed event for character data changed. The method is used
     * from nsIMutationObserver methods.
     *
     * @param aContent     the text node holding changed data
     * @param aInfo        info structure describing how the data was changed
     * @param aIsInserted  the flag pointed whether removed or inserted
     *                     characters should be cause of event
     */
    void FireTextChangeEventForText(nsIContent *aContent,
                                    CharacterDataChangeInfo* aInfo,
                                    PRBool aIsInserted);

  /**
   * Create a text change event for a changed node.
   *
   * @param  aContainerAccessible  [in] the parent accessible for the node
   * @param  aChangeNode           [in] the node that is being inserted or
   *                                 removed, or shown/hidden
   * @param  aAccessible           [in] the accessible for that node, or nsnull
   *                                 if none exists
   * @param  aIsInserting          [in] is aChangeNode being created or shown
   *                                 (vs. removed or hidden)
   * @param  aIsAsync              [in] whether casual change is async
   * @param  aIsFromUserInput      [in] the event is known to be from user input
   */
  already_AddRefed<AccEvent>
    CreateTextChangeEventForNode(nsAccessible *aContainerAccessible,
                                 nsIContent *aChangeNode,
                                 nsAccessible *aAccessible,
                                 PRBool aIsInserting,
                                 PRBool aIsAsynch,
                                 EIsFromUserInput aIsFromUserInput = eAutoDetect);

  /**
   * Used to define should the event be fired on a delay.
   */
  enum EEventFiringType {
    eNormalEvent,
    eDelayedEvent
  };

  /**
   * Fire show/hide events for either the current node if it has an accessible,
   * or the first-line accessible descendants of the given node.
   *
   * @param  aDOMNode          [in] the given node
   * @param  aAvoidOnThisNode  [in] call with PR_TRUE the first time to
   *                             prevent event firing on root node for change
   * @param  aEventType        [in] event type to fire an event
   * @param  aDelayedOrNormal  [in] whether to fire the event on a delay
   * @param  aIsAsyncChange    [in] whether casual change is async
   * @param  aIsFromUserInput  [in] the event is known to be from user input
   */
  nsresult FireShowHideEvents(nsINode *aDOMNode, PRBool aAvoidOnThisNode,
                              PRUint32 aEventType,
                              EEventFiringType aDelayedOrNormal,
                              PRBool aIsAsyncChange,
                              EIsFromUserInput aIsFromUserInput = eAutoDetect);

  /**
   * Fire a value change event for the the given accessible if it is a text
   * field (has a ROLE_ENTRY).
   */
  void FireValueChangeForTextFields(nsAccessible *aAccessible);

  /**
   * Cache of accessibles within this document accessible.
   */
  nsAccessibleHashtable mAccessibleCache;

    void *mWnd;
    nsCOMPtr<nsIDocument> mDocument;
    nsCOMPtr<nsITimer> mScrollWatchTimer;
    PRUint16 mScrollPositionChangedTicks; // Used for tracking scroll events

protected:

  nsRefPtr<nsAccEventQueue> mEventQueue;

  /**
   * Specifies if the document was loaded, used for error pages only.
   */
  PRPackedBool mIsLoaded;

    static PRUint32 gLastFocusedAccessiblesState;
    static nsIAtom *gLastFocusedFrameType;

  nsTArray<nsRefPtr<nsDocAccessible> > mChildDocuments;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsDocAccessible,
                              NS_DOCACCESSIBLE_IMPL_CID)

#endif  

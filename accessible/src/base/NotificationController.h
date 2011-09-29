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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
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

#ifndef NotificationController_h_
#define NotificationController_h_

#include "AccEvent.h"
#include "nsCycleCollectionParticipant.h"
#include "nsRefreshDriver.h"

class nsAccessible;
class nsDocAccessible;
class nsIContent;

// Uncomment to log notifications processing.
//#define DEBUG_NOTIFICATIONS

#ifdef DEBUG_NOTIFICATIONS
#define DEBUG_CONTENTMUTATION
#define DEBUG_TEXTCHANGE
#endif

/**
 * Notification interface.
 */
class Notification
{
public:
  virtual ~Notification() { };

  NS_INLINE_DECL_REFCOUNTING(Notification)

  /**
   * Process notification.
   */
  virtual void Process() = 0;

protected:
  Notification() { }

private:
  Notification(const Notification&);
  Notification& operator = (const Notification&);
};


/**
 * Template class for generic notification.
 *
 * @note  Instance is kept as a weak ref, the caller must guarantee it exists
 *        longer than the document accessible owning the notification controller
 *        that this notification is processed by.
 */
template<class Class, class Arg>
class TNotification : public Notification
{
public:
  typedef void (Class::*Callback)(Arg*);

  TNotification(Class* aInstance, Callback aCallback, Arg* aArg) :
    mInstance(aInstance), mCallback(aCallback), mArg(aArg) { }
  virtual ~TNotification() { mInstance = nsnull; }

  virtual void Process()
  {
    (mInstance->*mCallback)(mArg);

    mInstance = nsnull;
    mCallback = nsnull;
    mArg = nsnull;
  }

private:
  TNotification(const TNotification&);
  TNotification& operator = (const TNotification&);

  Class* mInstance;
  Callback mCallback;
  nsCOMPtr<Arg> mArg;
};

/**
 * Used to process notifications from core for the document accessible.
 */
class NotificationController : public nsARefreshObserver
{
public:
  NotificationController(nsDocAccessible* aDocument, nsIPresShell* aPresShell);
  virtual ~NotificationController();

  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(NotificationController)

  /**
   * Shutdown the notification controller.
   */
  void Shutdown();

  /**
   * Put an accessible event into the queue to process it later.
   */
  void QueueEvent(AccEvent* aEvent);

  /**
   * Schedule binding the child document to the tree of this document.
   */
  void ScheduleChildDocBinding(nsDocAccessible* aDocument);

  /**
   * Schedule the accessible tree update because of rendered text changes.
   */
  inline void ScheduleTextUpdate(nsIContent* aTextNode)
  {
    if (mTextHash.PutEntry(aTextNode))
      ScheduleProcessing();
  }

  /**
   * Pend accessible tree update for content insertion.
   */
  void ScheduleContentInsertion(nsAccessible* aContainer,
                                nsIContent* aStartChildNode,
                                nsIContent* aEndChildNode);

  /**
   * Process the generic notification synchronously if there are no pending
   * layout changes and no notifications are pending or being processed right
   * now. Otherwise, queue it up to process asynchronously.
   *
   * @note  The caller must guarantee that the given instance still exists when
   *        the notification is processed.
   */
  template<class Class, class Arg>
  inline void HandleNotification(Class* aInstance,
                                 typename TNotification<Class, Arg>::Callback aMethod,
                                 Arg* aArg)
  {
    if (!IsUpdatePending()) {
#ifdef DEBUG_NOTIFICATIONS
      printf("\nsync notification processing\n");
#endif
      (aInstance->*aMethod)(aArg);
      return;
    }

    nsRefPtr<Notification> notification =
      new TNotification<Class, Arg>(aInstance, aMethod, aArg);
    if (notification && mNotifications.AppendElement(notification))
      ScheduleProcessing();
  }

  /**
   * Schedule the generic notification to process asynchronously.
   *
   * @note  The caller must guarantee that the given instance still exists when
   *        the notification is processed.
   */
  template<class Class, class Arg>
  inline void ScheduleNotification(Class* aInstance,
                                   typename TNotification<Class, Arg>::Callback aMethod,
                                   Arg* aArg)
  {
    nsRefPtr<Notification> notification =
      new TNotification<Class, Arg>(aInstance, aMethod, aArg);
    if (notification && mNotifications.AppendElement(notification))
      ScheduleProcessing();
  }

protected:
  nsAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD

  /**
   * Start to observe refresh to make notifications and events processing after
   * layout.
   */
  void ScheduleProcessing();

  /**
   * Return true if the accessible tree state update is pending.
   */
  bool IsUpdatePending();

private:
  NotificationController(const NotificationController&);
  NotificationController& operator = (const NotificationController&);

  // nsARefreshObserver
  virtual void WillRefresh(mozilla::TimeStamp aTime);

  // Event queue processing
  /**
   * Coalesce redundant events from the queue.
   */
  void CoalesceEvents();

  /**
   * Apply aEventRule to same type event that from sibling nodes of aDOMNode.
   * @param aEventsToFire    array of pending events
   * @param aStart           start index of pending events to be scanned
   * @param aEnd             end index to be scanned (not included)
   * @param aEventType       target event type
   * @param aDOMNode         target are siblings of this node
   * @param aEventRule       the event rule to be applied
   *                         (should be eDoNotEmit or eAllowDupes)
   */
  void ApplyToSiblings(PRUint32 aStart, PRUint32 aEnd,
                       PRUint32 aEventType, nsINode* aNode,
                       AccEvent::EEventRule aEventRule);

  /**
   * Do not emit one of two given reorder events fired for DOM nodes in the case
   * when one DOM node is in parent chain of second one.
   */
  void CoalesceReorderEventsFromSameTree(AccEvent* aAccEvent,
                                         AccEvent* aDescendantAccEvent);

  /**
   * Coalesce text change events caused by sibling hide events.
   */
  void CoalesceTextChangeEventsFor(AccHideEvent* aTailEvent,
                                   AccHideEvent* aThisEvent);
  void CoalesceTextChangeEventsFor(AccShowEvent* aTailEvent,
                                   AccShowEvent* aThisEvent);

  /**
   * Create text change event caused by hide or show event. When a node is
   * hidden/removed or shown/appended, the text in an ancestor hyper text will
   * lose or get new characters.
   */
  void CreateTextChangeEventFor(AccMutationEvent* aEvent);

private:
  /**
   * Indicates whether we're waiting on an event queue processing from our
   * notification controller to flush events.
   */
  enum eObservingState {
    eNotObservingRefresh,
    eRefreshObserving,
    eRefreshProcessingForUpdate
  };
  eObservingState mObservingState;

  /**
   * The document accessible reference owning this queue.
   */
  nsRefPtr<nsDocAccessible> mDocument;

  /**
   * The presshell of the document accessible.
   */
  nsIPresShell* mPresShell;

  /**
   * Child documents that needs to be bound to the tree.
   */
  nsTArray<nsRefPtr<nsDocAccessible> > mHangingChildDocuments;

  /**
   * Storage for content inserted notification information.
   */
  class ContentInsertion
  {
  public:
    ContentInsertion(nsDocAccessible* aDocument, nsAccessible* aContainer);
    virtual ~ContentInsertion() { mDocument = nsnull; }

    NS_INLINE_DECL_REFCOUNTING(ContentInsertion)
    NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(ContentInsertion)

    bool InitChildList(nsIContent* aStartChildNode, nsIContent* aEndChildNode);
    void Process();

  private:
    ContentInsertion();
    ContentInsertion(const ContentInsertion&);
    ContentInsertion& operator = (const ContentInsertion&);

    // The document used to process content insertion, matched to document of
    // the notification controller that this notification belongs to, therefore
    // it's ok to keep it as weak ref.
    nsDocAccessible* mDocument;

    // The container accessible that content insertion occurs within.
    nsRefPtr<nsAccessible> mContainer;

    // Array of inserted contents.
    nsTArray<nsCOMPtr<nsIContent> > mInsertedContent;
  };

  /**
   * A pending accessible tree update notifications for content insertions.
   * Don't make this an nsAutoTArray; we use SwapElements() on it.
   */
  nsTArray<nsRefPtr<ContentInsertion> > mContentInsertions;

  template<class T>
  class nsCOMPtrHashKey : public PLDHashEntryHdr
  {
  public:
    typedef T* KeyType;
    typedef const T* KeyTypePointer;

    nsCOMPtrHashKey(const T* aKey) : mKey(const_cast<T*>(aKey)) {}
    nsCOMPtrHashKey(const nsPtrHashKey<T> &aToCopy) : mKey(aToCopy.mKey) {}
    ~nsCOMPtrHashKey() { }

    KeyType GetKey() const { return mKey; }
    bool KeyEquals(KeyTypePointer aKey) const { return aKey == mKey; }

    static KeyTypePointer KeyToPointer(KeyType aKey) { return aKey; }
    static PLDHashNumber HashKey(KeyTypePointer aKey)
      { return NS_PTR_TO_INT32(aKey) >> 2; }

    enum { ALLOW_MEMMOVE = PR_TRUE };

   protected:
     nsCOMPtr<T> mKey;
  };

  /**
   * A pending accessible tree update notifications for rendered text changes.
   */
  nsTHashtable<nsCOMPtrHashKey<nsIContent> > mTextHash;

  /**
   * Update the accessible tree for pending rendered text change notifications.
   */
  static PLDHashOperator TextEnumerator(nsCOMPtrHashKey<nsIContent>* aEntry,
                                        void* aUserArg);

  /**
   * Other notifications like DOM events. Don't make this an nsAutoTArray; we
   * use SwapElements() on it.
   */
  nsTArray<nsRefPtr<Notification> > mNotifications;

  /**
   * Pending events array. Don't make this an nsAutoTArray; we use
   * SwapElements() on it.
   */
  nsTArray<nsRefPtr<AccEvent> > mEvents;
};

#endif

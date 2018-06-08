/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_NotificationController_h_
#define mozilla_a11y_NotificationController_h_

#include "EventQueue.h"
#include "EventTree.h"

#include "mozilla/Tuple.h"
#include "nsCycleCollectionParticipant.h"
#include "nsRefreshDriver.h"

#include <utility>

#ifdef A11Y_LOG
#include "Logging.h"
#endif

namespace mozilla {
namespace a11y {

class DocAccessible;

/**
 * Notification interface.
 */
class Notification
{
public:
  NS_INLINE_DECL_REFCOUNTING(mozilla::a11y::Notification)

  /**
   * Process notification.
   */
  virtual void Process() = 0;

protected:
  Notification() { }

  /**
   * Protected destructor, to discourage deletion outside of Release():
   */
  virtual ~Notification() { }

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
template<class Class, class ... Args>
class TNotification : public Notification
{
public:
  typedef void (Class::*Callback)(Args* ...);

  TNotification(Class* aInstance, Callback aCallback, Args* ... aArgs) :
    mInstance(aInstance), mCallback(aCallback), mArgs(aArgs...) { }
  virtual ~TNotification() { mInstance = nullptr; }

  virtual void Process() override
    { ProcessHelper(std::index_sequence_for<Args...>{}); }

private:
  TNotification(const TNotification&);
  TNotification& operator = (const TNotification&);

  template <size_t... Indices>
    void ProcessHelper(std::index_sequence<Indices...>)
  {
     (mInstance->*mCallback)(Get<Indices>(mArgs)...);
  }

  Class* mInstance;
  Callback mCallback;
  Tuple<RefPtr<Args> ...> mArgs;
};

/**
 * Used to process notifications from core for the document accessible.
 */
class NotificationController final : public EventQueue,
                                     public nsARefreshObserver
{
public:
  NotificationController(DocAccessible* aDocument, nsIPresShell* aPresShell);

  NS_IMETHOD_(MozExternalRefCountType) AddRef(void) override;
  NS_IMETHOD_(MozExternalRefCountType) Release(void) override;

  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(NotificationController)

  /**
   * Shutdown the notification controller.
   */
  void Shutdown();

  /**
   * Add an accessible event into the queue to process it later.
   */
  void QueueEvent(AccEvent* aEvent)
  {
    if (PushEvent(aEvent)) {
      ScheduleProcessing();
    }
  }

  /**
   * Creates and adds a name change event into the queue for a container of
   * the given accessible, if the accessible is a part of name computation of
   * the container.
   */
  void QueueNameChange(Accessible* aChangeTarget)
  {
    if (PushNameChange(aChangeTarget)) {
      ScheduleProcessing();
    }
  }

  /**
   * Returns existing event tree for the given the accessible or creates one if
   * it doesn't exists yet.
   */
  EventTree* QueueMutation(Accessible* aContainer);

  class MoveGuard final {
  public:
    explicit MoveGuard(NotificationController* aController) :
      mController(aController)
    {
#ifdef DEBUG
      MOZ_ASSERT(!mController->mMoveGuardOnStack,
                 "Move guard is on stack already!");
      mController->mMoveGuardOnStack = true;
#endif
    }
    ~MoveGuard() {
#ifdef DEBUG
      MOZ_ASSERT(mController->mMoveGuardOnStack, "No move guard on stack!");
      mController->mMoveGuardOnStack = false;
#endif
      mController->mPrecedingEvents.Clear();
    }

  private:
    NotificationController* mController;
  };

#ifdef A11Y_LOG
  const EventTree& RootEventTree() const { return mEventTree; };
#endif

  /**
   * Queue a mutation event to emit if not coalesced away.  Returns true if the
   * event was queued and has not yet been coalesced.
   */
  bool QueueMutationEvent(AccTreeMutationEvent* aEvent);

  /**
   * Coalesce all queued mutation events.
   */
  void CoalesceMutationEvents();

  /**
   * Schedule binding the child document to the tree of this document.
   */
  void ScheduleChildDocBinding(DocAccessible* aDocument);

  /**
   * Schedule the accessible tree update because of rendered text changes.
   */
  inline void ScheduleTextUpdate(nsIContent* aTextNode)
  {
    // Make sure we are not called with a node that is not in the DOM tree or
    // not visible.
    MOZ_ASSERT(aTextNode->GetParentNode(), "A text node is not in DOM");
    MOZ_ASSERT(aTextNode->GetPrimaryFrame(), "A text node doesn't have a frame");
    MOZ_ASSERT(aTextNode->GetPrimaryFrame()->StyleVisibility()->IsVisible(),
               "A text node is not visible");

    mTextHash.PutEntry(aTextNode);
    ScheduleProcessing();
  }

  /**
   * Pend accessible tree update for content insertion.
   */
  void ScheduleContentInsertion(Accessible* aContainer,
                                nsIContent* aStartChildNode,
                                nsIContent* aEndChildNode);

  /**
   * Pend an accessible subtree relocation.
   */
  void ScheduleRelocation(Accessible* aOwner)
  {
    if (!mRelocations.Contains(aOwner) && mRelocations.AppendElement(aOwner)) {
      ScheduleProcessing();
    }
  }

  /**
   * Start to observe refresh to make notifications and events processing after
   * layout.
   */
  void ScheduleProcessing();

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
#ifdef A11Y_LOG
      if (mozilla::a11y::logging::IsEnabled(mozilla::a11y::logging::eNotifications))
        mozilla::a11y::logging::Text("sync notification processing");
#endif
      (aInstance->*aMethod)(aArg);
      return;
    }

    RefPtr<Notification> notification =
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
  template<class Class>
  inline void ScheduleNotification(Class* aInstance,
                                   typename TNotification<Class>::Callback aMethod)
  {
    RefPtr<Notification> notification =
      new TNotification<Class>(aInstance, aMethod);
    if (notification && mNotifications.AppendElement(notification))
      ScheduleProcessing();
  }

  template<class Class, class Arg>
  inline void ScheduleNotification(Class* aInstance,
                                   typename TNotification<Class, Arg>::Callback aMethod,
                                   Arg* aArg)
  {
    RefPtr<Notification> notification =
      new TNotification<Class, Arg>(aInstance, aMethod, aArg);
    if (notification && mNotifications.AppendElement(notification)) {
      ScheduleProcessing();
    }
  }

#ifdef DEBUG
  bool IsUpdating() const
    { return mObservingState == eRefreshProcessingForUpdate; }
#endif

protected:
  virtual ~NotificationController();

  nsCycleCollectingAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD

  /**
   * Return true if the accessible tree state update is pending.
   */
  bool IsUpdatePending();

  /**
   * Return true if we should wait for processing from the parent before we can
   * process our own queue.
   */
  bool WaitingForParent();

private:
  NotificationController(const NotificationController&);
  NotificationController& operator = (const NotificationController&);

  // nsARefreshObserver
  virtual void WillRefresh(mozilla::TimeStamp aTime) override;

  /**
   * Set and returns a hide event, paired with a show event, for the move.
   */
  void WithdrawPrecedingEvents(nsTArray<RefPtr<AccHideEvent>>* aEvs)
  {
    if (mPrecedingEvents.Length() > 0) {
      aEvs->AppendElements(std::move(mPrecedingEvents));
    }
  }
  void StorePrecedingEvent(AccHideEvent* aEv)
  {
    MOZ_ASSERT(mMoveGuardOnStack, "No move guard on stack!");
    mPrecedingEvents.AppendElement(aEv);
  }
  void StorePrecedingEvents(nsTArray<RefPtr<AccHideEvent>>&& aEvs)
  {
    MOZ_ASSERT(mMoveGuardOnStack, "No move guard on stack!");
    mPrecedingEvents.InsertElementsAt(0, aEvs);
  }

private:
  /**
   * get rid of a mutation event that is no longer necessary.
   */
  void DropMutationEvent(AccTreeMutationEvent* aEvent);

  /**
   * Fire all necessary mutation events.
   */
  void ProcessMutationEvents();

  /**
   * Indicates whether we're waiting on an event queue processing from our
   * notification controller to flush events.
   */
  enum eObservingState {
    eNotObservingRefresh,
    eRefreshObserving,
    eRefreshProcessing,
    eRefreshProcessingForUpdate
  };
  eObservingState mObservingState;

  /**
   * The presshell of the document accessible.
   */
  nsIPresShell* mPresShell;

  /**
   * Child documents that needs to be bound to the tree.
   */
  nsTArray<RefPtr<DocAccessible> > mHangingChildDocuments;

  /**
   * Pending accessible tree update notifications for content insertions.
   */
  nsClassHashtable<nsRefPtrHashKey<Accessible>,
                   nsTArray<nsCOMPtr<nsIContent>>> mContentInsertions;

  template<class T>
  class nsCOMPtrHashKey : public PLDHashEntryHdr
  {
  public:
    typedef T* KeyType;
    typedef const T* KeyTypePointer;

    explicit nsCOMPtrHashKey(const T* aKey) : mKey(const_cast<T*>(aKey)) {}
    explicit nsCOMPtrHashKey(const nsPtrHashKey<T> &aToCopy) : mKey(aToCopy.mKey) {}
    ~nsCOMPtrHashKey() { }

    KeyType GetKey() const { return mKey; }
    bool KeyEquals(KeyTypePointer aKey) const { return aKey == mKey; }

    static KeyTypePointer KeyToPointer(KeyType aKey) { return aKey; }
    static PLDHashNumber HashKey(KeyTypePointer aKey)
      { return NS_PTR_TO_INT32(aKey) >> 2; }

    enum { ALLOW_MEMMOVE = true };

   protected:
     nsCOMPtr<T> mKey;
  };

  /**
   * Pending accessible tree update notifications for rendered text changes.
   */
  nsTHashtable<nsCOMPtrHashKey<nsIContent> > mTextHash;

  /**
   * Other notifications like DOM events. Don't make this an AutoTArray; we
   * use SwapElements() on it.
   */
  nsTArray<RefPtr<Notification> > mNotifications;

  /**
   * Holds all scheduled relocations.
   */
  nsTArray<RefPtr<Accessible> > mRelocations;

  /**
   * Holds all mutation events.
   */
  EventTree mEventTree;

  /**
   * A temporary collection of hide events that should be fired before related
   * show event. Used by EventTree.
   */
  nsTArray<RefPtr<AccHideEvent>> mPrecedingEvents;

#ifdef DEBUG
  bool mMoveGuardOnStack;
#endif

  friend class MoveGuard;
  friend class EventTree;

  /**
   * A list of all mutation events we may want to emit.  Ordered from the first
   * event that should be emitted to the last one to emit.
   */
  RefPtr<AccTreeMutationEvent> mFirstMutationEvent;
  RefPtr<AccTreeMutationEvent> mLastMutationEvent;

  /**
   * A class to map an accessible and event type to an event.
   */
  class EventMap
  {
  public:
    enum EventType
    {
      ShowEvent = 0x0,
      HideEvent = 0x1,
      ReorderEvent = 0x2,
    };

    void PutEvent(AccTreeMutationEvent* aEvent);
    AccTreeMutationEvent* GetEvent(Accessible* aTarget, EventType aType);
    void RemoveEvent(AccTreeMutationEvent* aEvent);
    void Clear() { mTable.Clear(); }

  private:
    EventType GetEventType(AccTreeMutationEvent* aEvent);

    nsRefPtrHashtable<nsUint64HashKey, AccTreeMutationEvent> mTable;
  };

  EventMap mMutationMap;
  uint32_t mEventGeneration;
};

} // namespace a11y
} // namespace mozilla

#endif // mozilla_a11y_NotificationController_h_

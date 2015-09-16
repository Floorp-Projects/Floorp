/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_NotificationController_h_
#define mozilla_a11y_NotificationController_h_

#include "EventQueue.h"

#include "mozilla/IndexSequence.h"
#include "mozilla/Tuple.h"
#include "nsCycleCollectionParticipant.h"
#include "nsRefreshDriver.h"

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
    { ProcessHelper(typename IndexSequenceFor<Args...>::Type()); }

private:
  TNotification(const TNotification&);
  TNotification& operator = (const TNotification&);

  template <size_t... Indices>
    void ProcessHelper(IndexSequence<Indices...>)
  {
     (mInstance->*mCallback)(Get<Indices>(mArgs)...);
  }

  Class* mInstance;
  Callback mCallback;
  Tuple<nsRefPtr<Args> ...> mArgs;
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
   * Put an accessible event into the queue to process it later.
   */
  void QueueEvent(AccEvent* aEvent)
  {
    if (PushEvent(aEvent))
      ScheduleProcessing();
  }

  /**
   * Schedule binding the child document to the tree of this document.
   */
  void ScheduleChildDocBinding(DocAccessible* aDocument);

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
  void ScheduleContentInsertion(Accessible* aContainer,
                                nsIContent* aStartChildNode,
                                nsIContent* aEndChildNode);

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
  template<class Class>
  inline void ScheduleNotification(Class* aInstance,
                                   typename TNotification<Class>::Callback aMethod)
  {
    nsRefPtr<Notification> notification =
      new TNotification<Class>(aInstance, aMethod);
    if (notification && mNotifications.AppendElement(notification))
      ScheduleProcessing();
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

private:
  NotificationController(const NotificationController&);
  NotificationController& operator = (const NotificationController&);

  // nsARefreshObserver
  virtual void WillRefresh(mozilla::TimeStamp aTime) override;

private:
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
  nsTArray<nsRefPtr<DocAccessible> > mHangingChildDocuments;

  /**
   * Storage for content inserted notification information.
   */
  class ContentInsertion
  {
  public:
    ContentInsertion(DocAccessible* aDocument, Accessible* aContainer);

    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(ContentInsertion)
    NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(ContentInsertion)

    bool InitChildList(nsIContent* aStartChildNode, nsIContent* aEndChildNode);
    void Process();

  protected:
    virtual ~ContentInsertion() { mDocument = nullptr; }

  private:
    ContentInsertion();
    ContentInsertion(const ContentInsertion&);
    ContentInsertion& operator = (const ContentInsertion&);

    // The document used to process content insertion, matched to document of
    // the notification controller that this notification belongs to, therefore
    // it's ok to keep it as weak ref.
    DocAccessible* mDocument;

    // The container accessible that content insertion occurs within.
    nsRefPtr<Accessible> mContainer;

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
   * A pending accessible tree update notifications for rendered text changes.
   */
  nsTHashtable<nsCOMPtrHashKey<nsIContent> > mTextHash;

  /**
   * Other notifications like DOM events. Don't make this an nsAutoTArray; we
   * use SwapElements() on it.
   */
  nsTArray<nsRefPtr<Notification> > mNotifications;
};

} // namespace a11y
} // namespace mozilla

#endif // mozilla_a11y_NotificationController_h_

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_notification_h__
#define mozilla_dom_notification_h__

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/NotificationBinding.h"
#include "mozilla/dom/workers/bindings/WorkerFeature.h"

#include "nsIObserver.h"

#include "nsCycleCollectionParticipant.h"

class nsIPrincipal;
class nsIVariant;

namespace mozilla {
namespace dom {

class NotificationRef;
class WorkerNotificationObserver;
class Promise;

namespace workers {
  class WorkerPrivate;
} // namespace workers

class Notification;
class NotificationFeature final : public workers::WorkerFeature
{
  // Since the feature is strongly held by a Notification, it is ok to hold
  // a raw pointer here.
  Notification* mNotification;

public:
  explicit NotificationFeature(Notification* aNotification);

  bool
  Notify(JSContext* aCx, workers::Status aStatus) override;
};


/*
 * Notifications on workers introduce some lifetime issues. The property we
 * are trying to satisfy is:
 *   Whenever a task is dispatched to the main thread to operate on
 *   a Notification, the Notification should be addrefed on the worker thread
 *   and a feature should be added to observe the worker lifetime. This main
 *   thread owner should ensure it properly releases the reference to the
 *   Notification, additionally removing the feature if necessary.
 *
 * To enforce the correct addref and release, along with managing the feature,
 * we introduce a NotificationRef. Only one object may ever own
 * a NotificationRef, so UniquePtr<> is used throughout.  The NotificationRef
 * constructor calls AddRefObject(). When it is destroyed (on any thread) it
 * releases the Notification on the correct thread.
 *
 * Code should only access the underlying Notification object when it can
 * guarantee that it retains ownership of the NotificationRef while doing so.
 *
 * The one kink in this mechanism is that the worker feature may be Notify()ed
 * if the worker stops running script, even if the Notification's corresponding
 * UI is still visible to the user. We handle this case with the following
 * steps:
 *   a) Close the notification. This is done by blocking the worker on the main
 *   thread. This ensures that there are no main thread holders when the worker
 *   resumes. This also deals with the case where Notify() runs on the worker
 *   before the observer has been created on the main thread. Even in such
 *   a situation, the CloseNotificationRunnable() will only run after the
 *   Show task that was previously queued. Since the show task is only queued
 *   once when the Notification is created, we can be sure that no new tasks
 *   will follow the Notify().
 *
 *   b) Ask the observer to let go of its NotificationRef's underlying
 *   Notification without proper cleanup since the feature will handle the
 *   release. This is only OK because every notification has only one
 *   associated observer. The NotificationRef itself is still owned by the
 *   observer and deleted by the UniquePtr, but it doesn't do anything since
 *   the underlying Notification is null.
 *
 * To unify code-paths, we use the same NotificationRef in the main
 * thread implementation too.
 *
 * Note that the Notification's JS wrapper does it's standard
 * AddRef()/Release() and is not affected by any of this.
 *
 * Since the worker event queue can have runnables that will dispatch events on
 * the Notification, the NotificationRef destructor will first try to release
 * the Notification by dispatching a normal runnable to the worker so that it is
 * queued after any event runnables. If that dispatch fails, it means the worker
 * is no longer running and queued WorkerRunnables will be canceled, so we
 * dispatch a control runnable instead.
 *
 */
class Notification : public DOMEventTargetHelper
{
  friend class CloseNotificationRunnable;
  friend class NotificationTask;
  friend class NotificationPermissionRequest;
  friend class NotificationObserver;
  friend class NotificationStorageCallback;
  friend class WorkerNotificationObserver;

public:
  IMPL_EVENT_HANDLER(click)
  IMPL_EVENT_HANDLER(show)
  IMPL_EVENT_HANDLER(error)
  IMPL_EVENT_HANDLER(close)

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(Notification, DOMEventTargetHelper)

  static bool PrefEnabled(JSContext* aCx, JSObject* aObj);
  // Returns if Notification.get() is allowed for the current global.
  static bool IsGetEnabled(JSContext* aCx, JSObject* aObj);

  static already_AddRefed<Notification> Constructor(const GlobalObject& aGlobal,
                                                    const nsAString& aTitle,
                                                    const NotificationOptions& aOption,
                                                    ErrorResult& aRv);
  void GetID(nsAString& aRetval) {
    aRetval = mID;
  }

  void GetTitle(nsAString& aRetval)
  {
    aRetval = mTitle;
  }

  NotificationDirection Dir()
  {
    return mDir;
  }

  void GetLang(nsAString& aRetval)
  {
    aRetval = mLang;
  }

  void GetBody(nsAString& aRetval)
  {
    aRetval = mBody;
  }

  void GetTag(nsAString& aRetval)
  {
    aRetval = mTag;
  }

  void GetIcon(nsAString& aRetval)
  {
    aRetval = mIconUrl;
  }

  void SetStoredState(bool val)
  {
    mIsStored = val;
  }

  bool IsStored()
  {
    return mIsStored;
  }

  static bool RequestPermissionEnabledForScope(JSContext* aCx, JSObject* /* unused */);

  static void RequestPermission(const GlobalObject& aGlobal,
                                const Optional<OwningNonNull<NotificationPermissionCallback> >& aCallback,
                                ErrorResult& aRv);

  static NotificationPermission GetPermission(const GlobalObject& aGlobal,
                                              ErrorResult& aRv);

  static already_AddRefed<Promise> Get(const GlobalObject& aGlobal,
                                       const GetNotificationOptions& aFilter,
                                       ErrorResult& aRv);

  void Close();

  nsPIDOMWindow* GetParentObject()
  {
    return GetOwner();
  }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void GetData(JSContext* aCx, JS::MutableHandle<JS::Value> aRetval);

  void InitFromJSVal(JSContext* aCx, JS::Handle<JS::Value> aData, ErrorResult& aRv);

  void InitFromBase64(JSContext* aCx, const nsAString& aData, ErrorResult& aRv);

  void AssertIsOnTargetThread() const
  {
    MOZ_ASSERT(IsTargetThread());
  }

  // Initialized on the worker thread, never unset, and always used in
  // a read-only capacity. Used on any thread.
  workers::WorkerPrivate* mWorkerPrivate;

  // Main thread only.
  WorkerNotificationObserver* mObserver;

  // The NotificationTask calls ShowInternal()/CloseInternal() on the
  // Notification. At this point the task has ownership of the Notification. It
  // passes this on to the Notification itself via mTempRef so that
  // ShowInternal()/CloseInternal() may pass it along appropriately (or release
  // it).
  //
  // Main thread only.
  UniquePtr<NotificationRef> mTempRef;

  // Returns true if addref succeeded.
  bool AddRefObject();
  void ReleaseObject();

  static NotificationPermission GetPermissionInternal(nsIPrincipal* aPrincipal,
                                                      ErrorResult& rv);

  bool DispatchClickEvent();
protected:
  Notification(const nsAString& aID, const nsAString& aTitle, const nsAString& aBody,
               NotificationDirection aDir, const nsAString& aLang,
               const nsAString& aTag, const nsAString& aIconUrl,
               const NotificationBehavior& aBehavior, nsIGlobalObject* aGlobal);

  static already_AddRefed<Notification> CreateInternal(nsIGlobalObject* aGlobal,
                                                       const nsAString& aID,
                                                       const nsAString& aTitle,
                                                       const NotificationOptions& aOptions);

  void ShowInternal();
  void CloseInternal();

  static NotificationPermission GetPermissionInternal(nsISupports* aGlobal,
                                                      ErrorResult& rv);

  static const nsString DirectionToString(NotificationDirection aDirection)
  {
    switch (aDirection) {
    case NotificationDirection::Ltr:
      return NS_LITERAL_STRING("ltr");
    case NotificationDirection::Rtl:
      return NS_LITERAL_STRING("rtl");
    default:
      return NS_LITERAL_STRING("auto");
    }
  }

  static const NotificationDirection StringToDirection(const nsAString& aDirection)
  {
    if (aDirection.EqualsLiteral("ltr")) {
      return NotificationDirection::Ltr;
    }
    if (aDirection.EqualsLiteral("rtl")) {
      return NotificationDirection::Rtl;
    }
    return NotificationDirection::Auto;
  }

  static nsresult GetOrigin(nsPIDOMWindow* aWindow, nsString& aOrigin);
  nsresult GetOriginWorker(nsString& aOrigin);

  void GetAlertName(nsAString& aRetval)
  {
    aRetval = mAlertName;
  }

  const nsString mID;
  const nsString mTitle;
  const nsString mBody;
  const NotificationDirection mDir;
  const nsString mLang;
  const nsString mTag;
  const nsString mIconUrl;
  nsString mDataAsBase64;
  const NotificationBehavior mBehavior;

  // It's null until GetData is first called
  JS::Heap<JS::Value> mData;

  nsString mAlertName;

  // Main thread only.
  bool mIsClosed;

  // We need to make a distinction between the notification being closed i.e.
  // removed from any pending or active lists, and the notification being
  // removed from the database. NotificationDB might fail when trying to remove
  // the notification.
  bool mIsStored;

  static uint32_t sCount;

private:
  virtual ~Notification();

  nsIPrincipal* GetPrincipal();

  nsresult PersistNotification();
  void UnpersistNotification();

  bool IsTargetThread() const
  {
    return NS_IsMainThread() == !mWorkerPrivate;
  }

  bool RegisterFeature();
  void UnregisterFeature();

  nsresult ResolveIconAndSoundURL(nsString&, nsString&);

  // Only used for Notifications on Workers, worker thread only.
  UniquePtr<NotificationFeature> mFeature;
  // Target thread only.
  uint32_t mTaskCount;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_notification_h__


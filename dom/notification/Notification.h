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
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/quota/QuotaCommon.h"

#include "nsIObserver.h"
#include "nsISupports.h"

#include "nsCycleCollectionParticipant.h"
#include "nsWeakReference.h"

class nsIPrincipal;
class nsIVariant;

namespace mozilla::dom {

class NotificationRef;
class WorkerNotificationObserver;
class Promise;
class StrongWorkerRef;

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
class Notification : public DOMEventTargetHelper,
                     public nsIObserver,
                     public nsSupportsWeakReference {
  friend class CloseNotificationRunnable;
  friend class NotificationTask;
  friend class NotificationPermissionRequest;
  friend class MainThreadNotificationObserver;
  friend class NotificationStorageCallback;
  friend class ServiceWorkerNotificationObserver;
  friend class WorkerGetRunnable;
  friend class WorkerNotificationObserver;

 public:
  IMPL_EVENT_HANDLER(click)
  IMPL_EVENT_HANDLER(show)
  IMPL_EVENT_HANDLER(error)
  IMPL_EVENT_HANDLER(close)

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(Notification,
                                                         DOMEventTargetHelper)
  NS_DECL_NSIOBSERVER

  static bool PrefEnabled(JSContext* aCx, JSObject* aObj);

  static already_AddRefed<Notification> Constructor(
      const GlobalObject& aGlobal, const nsAString& aTitle,
      const NotificationOptions& aOption, ErrorResult& aRv);

  /**
   * Used when dispatching the ServiceWorkerEvent.
   *
   * Does not initialize the Notification's behavior.
   * This is because:
   * 1) The Notification is not shown to the user and so the behavior
   *    parameters don't matter.
   * 2) The default binding requires main thread for parsing the JSON from the
   *    string behavior.
   */
  static Result<already_AddRefed<Notification>, QMResult> ConstructFromFields(
      nsIGlobalObject* aGlobal, const nsAString& aID, const nsAString& aTitle,
      const nsAString& aDir, const nsAString& aLang, const nsAString& aBody,
      const nsAString& aTag, const nsAString& aIcon, const nsAString& aData,
      const nsAString& aServiceWorkerRegistrationScope);

  void GetID(nsAString& aRetval) { aRetval = mID; }

  void GetTitle(nsAString& aRetval) { aRetval = mTitle; }

  NotificationDirection Dir() { return mDir; }

  void GetLang(nsAString& aRetval) { aRetval = mLang; }

  void GetBody(nsAString& aRetval) { aRetval = mBody; }

  void GetTag(nsAString& aRetval) { aRetval = mTag; }

  void GetIcon(nsAString& aRetval) { aRetval = mIconUrl; }

  void SetStoredState(bool val) { mIsStored = val; }

  bool IsStored() { return mIsStored; }

  static bool RequestPermissionEnabledForScope(JSContext* aCx,
                                               JSObject* /* unused */);

  static already_AddRefed<Promise> RequestPermission(
      const GlobalObject& aGlobal,
      const Optional<OwningNonNull<NotificationPermissionCallback> >& aCallback,
      ErrorResult& aRv);

  static NotificationPermission GetPermission(const GlobalObject& aGlobal,
                                              ErrorResult& aRv);

  static already_AddRefed<Promise> Get(nsPIDOMWindowInner* aWindow,
                                       const GetNotificationOptions& aFilter,
                                       const nsAString& aScope,
                                       ErrorResult& aRv);

  static already_AddRefed<Promise> WorkerGet(
      WorkerPrivate* aWorkerPrivate, const GetNotificationOptions& aFilter,
      const nsAString& aScope, ErrorResult& aRv);

  // Notification implementation of
  // ServiceWorkerRegistration.showNotification.
  //
  //
  // Note that aCx may not be in the compartment of aGlobal, but aOptions will
  // have its JS things in the compartment of aCx.
  static already_AddRefed<Promise> ShowPersistentNotification(
      JSContext* aCx, nsIGlobalObject* aGlobal, const nsAString& aScope,
      const nsAString& aTitle, const NotificationOptions& aOptions,
      const ServiceWorkerRegistrationDescriptor& aDescriptor, ErrorResult& aRv);

  void Close();

  nsPIDOMWindowInner* GetParentObject() { return GetOwner(); }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  bool RequireInteraction() const;

  bool Silent() const;

  void GetVibrate(nsTArray<uint32_t>& aRetval) const;

  void GetData(JSContext* aCx, JS::MutableHandle<JS::Value> aRetval);

  void InitFromJSVal(JSContext* aCx, JS::Handle<JS::Value> aData,
                     ErrorResult& aRv);

  Result<Ok, QMResult> InitFromBase64(const nsAString& aData);

  void AssertIsOnTargetThread() const { MOZ_ASSERT(IsTargetThread()); }

  // Initialized on the worker thread, never unset, and always used in
  // a read-only capacity. Used on any thread.
  CheckedUnsafePtr<WorkerPrivate> mWorkerPrivate;

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

  static NotificationPermission GetPermission(nsIGlobalObject* aGlobal,
                                              ErrorResult& aRv);

  static NotificationPermission GetPermissionInternal(nsIPrincipal* aPrincipal,
                                                      ErrorResult& rv);

  static NotificationPermission TestPermission(nsIPrincipal* aPrincipal);

  bool DispatchClickEvent();

  static nsresult RemovePermission(nsIPrincipal* aPrincipal);
  static nsresult OpenSettings(nsIPrincipal* aPrincipal);

  nsresult DispatchToMainThread(already_AddRefed<nsIRunnable>&& aRunnable);

 protected:
  Notification(nsIGlobalObject* aGlobal, const nsAString& aID,
               const nsAString& aTitle, const nsAString& aBody,
               NotificationDirection aDir, const nsAString& aLang,
               const nsAString& aTag, const nsAString& aIconUrl,
               bool aRequireInteraction, bool aSilent,
               nsTArray<uint32_t>&& aVibrate,
               const NotificationBehavior& aBehavior);

  static already_AddRefed<Notification> CreateInternal(
      nsIGlobalObject* aGlobal, const nsAString& aID, const nsAString& aTitle,
      const NotificationOptions& aOptions, ErrorResult& aRv);

  // Triggers CloseInternal for non-persistent notifications if window goes away
  nsresult MaybeObserveWindowFrozenOrDestroyed();
  bool IsInPrivateBrowsing();
  void ShowInternal();
  void CloseInternal(bool aContextClosed = false);

  static NotificationPermission GetPermissionInternal(
      nsPIDOMWindowInner* aWindow, ErrorResult& rv);

  static nsresult GetOrigin(nsIPrincipal* aPrincipal, nsString& aOrigin);

  void GetAlertName(nsAString& aRetval) {
    AssertIsOnMainThread();
    if (mAlertName.IsEmpty()) {
      SetAlertName();
    }
    aRetval = mAlertName;
  }

  void GetScope(nsAString& aScope) { aScope = mScope; }

  void SetScope(const nsAString& aScope) {
    MOZ_ASSERT(mScope.IsEmpty());
    mScope = aScope;
  }

  const nsString mID;
  const nsString mTitle;
  const nsString mBody;
  const NotificationDirection mDir;
  const nsString mLang;
  const nsString mTag;
  const nsString mIconUrl;
  const bool mRequireInteraction;
  const bool mSilent;
  nsTArray<uint32_t> mVibrate;
  nsString mDataAsBase64;
  const NotificationBehavior mBehavior;

  // It's null until GetData is first called
  JS::Heap<JS::Value> mData;

  nsString mAlertName;
  nsString mScope;

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

  // Creates a Notification and shows it. Returns a reference to the
  // Notification if result is NS_OK. The lifetime of this Notification is tied
  // to an underlying NotificationRef. Do not hold a non-stack raw pointer to
  // it. Be careful about thread safety if acquiring a strong reference.
  //
  // Note that aCx may not be in the compartment of aGlobal, but aOptions will
  // have its JS things in the compartment of aCx.
  static already_AddRefed<Notification> CreateAndShow(
      JSContext* aCx, nsIGlobalObject* aGlobal, const nsAString& aTitle,
      const NotificationOptions& aOptions, const nsAString& aScope,
      ErrorResult& aRv);

  nsIPrincipal* GetPrincipal();

  nsresult PersistNotification();
  void UnpersistNotification();

  void SetAlertName();

  bool IsTargetThread() const { return NS_IsMainThread() == !mWorkerPrivate; }

  bool CreateWorkerRef();

  nsresult ResolveIconAndSoundURL(nsString&, nsString&);

  // Only used for Notifications on Workers, worker thread only.
  RefPtr<StrongWorkerRef> mWorkerRef;
  // Target thread only.
  uint32_t mTaskCount;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_notification_h__

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

#include "nsIObserver.h"
#include "nsISupports.h"

#include "nsCycleCollectionParticipant.h"
#include "nsHashKeys.h"
#include "nsTHashtable.h"
#include "nsWeakReference.h"

#define NOTIFICATIONTELEMETRYSERVICE_CONTRACTID \
  "@mozilla.org/notificationTelemetryService;1"

class nsIPrincipal;
class nsIVariant;

namespace mozilla {
namespace dom {

class NotificationTask;
class Promise;
class WorkerNotificationObserver;
class WorkerPrivate;

// Records telemetry probes at application startup, when a notification is
// shown, and when the notification permission is revoked for a site.
class NotificationTelemetryService final : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  NotificationTelemetryService();

  static already_AddRefed<NotificationTelemetryService> GetInstance();

  nsresult Init();
  void RecordDNDSupported();
  void RecordPermissions();

private:
  virtual ~NotificationTelemetryService();

  bool GetNotificationPermission(nsISupports* aSupports,
                                 uint32_t* aCapability);

  bool mDNDRecorded;
};

/*
 * Notifications on workers introduce some interesting multi-thread issues
 * becuase the real operation is always executed on the main-thread, also when
 * the Notification object is created on a Dedicated Worker or a Service Worker
 * thread.
 *
 * When a notification is shown or is closed (the main 2 actions of this API),
 * we create a NavigationTask object, which is a thread-safe class, created on
 * the owning thread. In case the owning thread is a Worker, this is kept alive
 * using a StrongWorkerRef. The NotificationTask is kept alive by the
 * notification observer and it's released when the alert is dimissed. The
 * observer is a main-thread only object and, only on that thread, the observer
 * is notified when the alert is shown, clicked and dismissed.
 *
 * Only for workers we create a cycle between the NotificationTask and the
 * observer. The reason why we do this cycle is because the ending of the
 * notification can start from both components:
 *
 * - the observer, on the main-thread, is notified when the alert is dismissed.
 *   When this happens, on the main-thread, we break the cycle: the observer is
 *   released and this releases the NotificationTask, the WorkerRef and the
 *   worker.

 * - if the worker goes away, the WorkerRef callback is executed on the
 *   worker thread. Here we dispatch a normal runnable to the main-thread. The
 *   cycle is broken on the main-thread and we release the objects as the
 *   previous scenario.
 *
 * Note that, during the worker shutting down, the observer will not be able to
 * dispatch events on that thread. This is correct, but it will probably
 * generate some warning messages on stderr.
 */
class Notification : public DOMEventTargetHelper
                   , public nsIObserver
                   , public nsSupportsWeakReference
{
  friend class NotificationTask;
  friend class NotificationTaskRunnable;
  friend class NotificationPermissionRequest;
  friend class MainThreadNotificationObserver;
  friend class NotificationStorageCallback;
  friend class ServiceWorkerNotificationObserver;
  friend class WorkerGetRunnable;
  friend class WorkerNotificationObserver;
  friend class NotificationTelemetryService;

public:
  IMPL_EVENT_HANDLER(click)
  IMPL_EVENT_HANDLER(show)
  IMPL_EVENT_HANDLER(error)
  IMPL_EVENT_HANDLER(close)

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(Notification, DOMEventTargetHelper)
  NS_DECL_NSIOBSERVER
  NS_DECL_OWNINGTHREAD

  static bool PrefEnabled(JSContext* aCx, JSObject* aObj);
  // Returns if Notification.get() is allowed for the current global.
  static bool IsGetEnabled(JSContext* aCx, JSObject* aObj);

  static already_AddRefed<Notification> Constructor(const GlobalObject& aGlobal,
                                                    const nsAString& aTitle,
                                                    const NotificationOptions& aOption,
                                                    ErrorResult& aRv);

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
  static already_AddRefed<Notification>
  ConstructFromFields(
    nsIGlobalObject* aGlobal,
    const nsAString& aID,
    const nsAString& aTitle,
    const nsAString& aDir,
    const nsAString& aLang,
    const nsAString& aBody,
    const nsAString& aTag,
    const nsAString& aIcon,
    const nsAString& aData,
    const nsAString& aServiceWorkerRegistrationScope,
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

  static already_AddRefed<Promise>
  RequestPermission(const GlobalObject& aGlobal,
                    const Optional<OwningNonNull<NotificationPermissionCallback> >& aCallback,
                    ErrorResult& aRv);

  static NotificationPermission GetPermission(const GlobalObject& aGlobal,
                                              ErrorResult& aRv);

  static already_AddRefed<Promise>
  Get(nsPIDOMWindowInner* aWindow,
      const GetNotificationOptions& aFilter,
      const nsAString& aScope,
      ErrorResult& aRv);

  static already_AddRefed<Promise> Get(const GlobalObject& aGlobal,
                                       const GetNotificationOptions& aFilter,
                                       ErrorResult& aRv);

  static already_AddRefed<Promise> WorkerGet(WorkerPrivate* aWorkerPrivate,
                                             const GetNotificationOptions& aFilter,
                                             const nsAString& aScope,
                                             ErrorResult& aRv);

  // Notification implementation of
  // ServiceWorkerRegistration.showNotification.
  //
  //
  // Note that aCx may not be in the compartment of aGlobal, but aOptions will
  // have its JS things in the compartment of aCx.
  static already_AddRefed<Promise>
  ShowPersistentNotification(JSContext* aCx,
                             nsIGlobalObject* aGlobal,
                             const nsAString& aScope,
                             const nsAString& aTitle,
                             const NotificationOptions& aOptions,
                             ErrorResult& aRv);

  void Close();

  nsPIDOMWindowInner* GetParentObject()
  {
    return GetOwner();
  }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  bool RequireInteraction() const;

  void GetData(JSContext* aCx, JS::MutableHandle<JS::Value> aRetval);

  void InitFromJSVal(JSContext* aCx, JS::Handle<JS::Value> aData, ErrorResult& aRv);

  void InitFromBase64(const nsAString& aData, ErrorResult& aRv);

  void AssertIsOnTargetThread() const
  {
    NS_ASSERT_OWNINGTHREAD(Notification);
  }

  static NotificationPermission GetPermission(nsIGlobalObject* aGlobal,
                                              ErrorResult& aRv);

  static NotificationPermission GetPermissionInternal(nsIPrincipal* aPrincipal,
                                                      ErrorResult& rv);

  static NotificationPermission TestPermission(nsIPrincipal* aPrincipal);

  bool DispatchClickEvent();

  static nsresult RemovePermission(nsIPrincipal* aPrincipal);
  static nsresult OpenSettings(nsIPrincipal* aPrincipal);

  const NotificationBehavior& Behavior() const
  {
    return mBehavior;
  }

protected:
  Notification(nsIGlobalObject* aGlobal, const nsAString& aID,
               const nsAString& aTitle, const nsAString& aBody,
               NotificationDirection aDir, const nsAString& aLang,
               const nsAString& aTag, const nsAString& aIconUrl,
               bool aRequireNotification,
               const NotificationBehavior& aBehavior);

  static already_AddRefed<Notification> CreateInternal(nsIGlobalObject* aGlobal,
                                                       const nsAString& aID,
                                                       const nsAString& aTitle,
                                                       const NotificationOptions& aOptions);

  nsresult Init();
  void ShowInternal(already_AddRefed<NotificationTask> aTask);
  void CloseInternal(already_AddRefed<NotificationTask> aTask,
                     nsIPrincipal* aPricipal);

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

  static NotificationDirection StringToDirection(const nsAString& aDirection)
  {
    if (aDirection.EqualsLiteral("ltr")) {
      return NotificationDirection::Ltr;
    }
    if (aDirection.EqualsLiteral("rtl")) {
      return NotificationDirection::Rtl;
    }
    return NotificationDirection::Auto;
  }

  static nsresult GetOrigin(nsIPrincipal* aPrincipal, nsString& aOrigin);

  void GetAlertName(nsIPrincipal* aPrincipal, nsAString& aRetval)
  {
    AssertIsOnMainThread();
    if (mAlertName.IsEmpty()) {
      SetAlertName(aPrincipal);
    }
    aRetval = mAlertName;
  }

  void GetScope(nsAString& aScope)
  {
    aScope = mScope;
  }

  void
  SetScope(const nsAString& aScope)
  {
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
  // to an underlying NotificationTask. Do not hold a non-stack raw pointer to
  // it. Be careful about thread safety if acquiring a strong reference.
  //
  // Note that aCx may not be in the compartment of aGlobal, but aOptions will
  // have its JS things in the compartment of aCx.
  static already_AddRefed<Notification>
  CreateAndShow(JSContext* aCx,
                nsIGlobalObject* aGlobal,
                const nsAString& aTitle,
                const NotificationOptions& aOptions,
                const nsAString& aScope,
                ErrorResult& aRv);

  nsresult PersistNotification(nsIPrincipal* aPrincipal);
  void UnpersistNotification(nsIPrincipal* aPrincipal);

  void
  SetAlertName(nsIPrincipal* aPrincipal);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_notification_h__

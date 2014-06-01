/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_serviceworkermanager_h
#define mozilla_dom_workers_serviceworkermanager_h

#include "nsIServiceWorkerManager.h"
#include "nsCOMPtr.h"

#include "mozilla/Attributes.h"
#include "mozilla/LinkedList.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Promise.h"
#include "nsClassHashtable.h"
#include "nsDataHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsTArrayForwardDeclare.h"
#include "nsTWeakRef.h"

namespace mozilla {
namespace dom {
namespace workers {

class ServiceWorker;

/*
 * Wherever the spec treats a worker instance and a description of said worker
 * as the same thing; i.e. "Resolve foo with
 * _GetNewestWorker(serviceWorkerRegistration)", we represent the description
 * by this class and spawn a ServiceWorker in the right global when required.
 */
class ServiceWorkerInfo
{
  const nsCString mScriptSpec;
public:

  bool
  IsValid() const
  {
    return !mScriptSpec.IsVoid();
  }

  const nsCString&
  GetScriptSpec() const
  {
    MOZ_ASSERT(IsValid());
    return mScriptSpec;
  }

  ServiceWorkerInfo()
  { }

  explicit ServiceWorkerInfo(const nsACString& aScriptSpec)
    : mScriptSpec(aScriptSpec)
  { }
};

class ServiceWorkerRegistration
{
public:
  NS_INLINE_DECL_REFCOUNTING(ServiceWorkerRegistration)

  nsCString mScope;
  // The scriptURL for the registration. This may be completely different from
  // the URLs of the following three workers.
  nsCString mScriptSpec;

  ServiceWorkerInfo mCurrentWorker;
  ServiceWorkerInfo mWaitingWorker;
  ServiceWorkerInfo mInstallingWorker;

  bool mHasUpdatePromise;

  void
  AddUpdatePromiseObserver(Promise* aPromise)
  {
    // FIXME(nsm): Same bug, different patch.
  }

  bool
  HasUpdatePromise()
  {
    return mHasUpdatePromise;
  }

  // When unregister() is called on a registration, it is not immediately
  // removed since documents may be controlled. It is marked as
  // pendingUninstall and when all controlling documents go away, removed.
  bool mPendingUninstall;

  explicit ServiceWorkerRegistration(const nsACString& aScope)
    : mScope(aScope),
      mHasUpdatePromise(false),
      mPendingUninstall(false)
  { }

  ServiceWorkerInfo
  Newest() const
  {
    if (mInstallingWorker.IsValid()) {
      return mInstallingWorker;
    } else if (mWaitingWorker.IsValid()) {
      return mWaitingWorker;
    } else {
      return mCurrentWorker;
    }
  }
};

#define NS_SERVICEWORKERMANAGER_IMPL_IID                 \
{ /* f4f8755a-69ca-46e8-a65d-775745535990 */             \
  0xf4f8755a,                                            \
  0x69ca,                                                \
  0x46e8,                                                \
  { 0xa6, 0x5d, 0x77, 0x57, 0x45, 0x53, 0x59, 0x90 }     \
}

/*
 * The ServiceWorkerManager is a per-process global that deals with the
 * installation, querying and event dispatch of ServiceWorkers for all the
 * origins in the process.
 */
class ServiceWorkerManager MOZ_FINAL : public nsIServiceWorkerManager
{
  friend class RegisterRunnable;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISERVICEWORKERMANAGER
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_SERVICEWORKERMANAGER_IMPL_IID)

  static ServiceWorkerManager* FactoryCreate()
  {
    ServiceWorkerManager* res = new ServiceWorkerManager;
    NS_ADDREF(res);
    return res;
  }

  /*
   * This struct is used for passive ServiceWorker management.
   * Actively running ServiceWorkers use the SharedWorker infrastructure in
   * RuntimeService for execution and lifetime management.
   */
  struct ServiceWorkerDomainInfo
  {
    // Scope to registration.
    nsRefPtrHashtable<nsCStringHashKey, ServiceWorkerRegistration> mServiceWorkerRegistrations;

    ServiceWorkerDomainInfo()
    { }

    already_AddRefed<ServiceWorkerRegistration>
    GetRegistration(const nsCString& aScope) const
    {
      nsRefPtr<ServiceWorkerRegistration> reg;
      mServiceWorkerRegistrations.Get(aScope, getter_AddRefs(reg));
      return reg.forget();
    }

    ServiceWorkerRegistration*
    CreateNewRegistration(const nsCString& aScope)
    {
      ServiceWorkerRegistration* registration =
        new ServiceWorkerRegistration(aScope);
      // From now on ownership of registration is with
      // mServiceWorkerRegistrations.
      mServiceWorkerRegistrations.Put(aScope, registration);
      return registration;
    }
  };

  nsClassHashtable<nsCStringHashKey, ServiceWorkerDomainInfo> mDomainMap;

  // FIXME(nsm): What do we do if a page calls for register("/foo_worker.js", { scope: "/*"
  // }) and then another page calls register("/bar_worker.js", { scope: "/*" })
  // while the install is in progress. The async install steps for register
  // bar_worker.js could finish before foo_worker.js, but bar_worker still has
  // to be the winning controller.
  // FIXME(nsm): Move this into per domain?

  static already_AddRefed<ServiceWorkerManager>
  GetInstance();

private:
  ServiceWorkerManager();
  ~ServiceWorkerManager();

  NS_IMETHOD
  Update(ServiceWorkerRegistration* aRegistration, nsPIDOMWindow* aWindow);

  NS_IMETHODIMP
  CreateServiceWorkerForWindow(nsPIDOMWindow* aWindow,
                               const nsACString& aScriptSpec,
                               const nsACString& aScope,
                               ServiceWorker** aServiceWorker);

  static PLDHashOperator
  CleanupServiceWorkerInformation(const nsACString& aDomain,
                                  ServiceWorkerDomainInfo* aDomainInfo,
                                  void *aUnused);
};

NS_DEFINE_STATIC_IID_ACCESSOR(ServiceWorkerManager,
                              NS_SERVICEWORKERMANAGER_IMPL_IID);

} // namespace workers
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_workers_serviceworkermanager_h

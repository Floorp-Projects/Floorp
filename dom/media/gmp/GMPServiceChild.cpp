/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPServiceChild.h"
#include "mozilla/dom/ContentChild.h"
#include "mozIGeckoMediaPluginService.h"
#include "mozIGeckoMediaPluginChromeService.h"
#include "nsCOMPtr.h"
#include "GMPParent.h"
#include "GMPContentParent.h"
#include "nsXPCOMPrivate.h"
#include "mozilla/SyncRunnable.h"
#include "runnable_utils.h"

namespace mozilla {

#ifdef LOG
#undef LOG
#endif

#define LOGD(msg) MOZ_LOG(GetGMPLog(), mozilla::LogLevel::Debug, msg)
#define LOG(level, msg) MOZ_LOG(GetGMPLog(), (level), msg)

#ifdef __CLASS__
#undef __CLASS__
#endif
#define __CLASS__ "GMPService"

namespace gmp {

already_AddRefed<GeckoMediaPluginServiceChild>
GeckoMediaPluginServiceChild::GetSingleton()
{
  MOZ_ASSERT(!XRE_IsParentProcess());
  nsRefPtr<GeckoMediaPluginService> service(
    GeckoMediaPluginService::GetGeckoMediaPluginService());
#ifdef DEBUG
  if (service) {
    nsCOMPtr<mozIGeckoMediaPluginChromeService> chromeService;
    CallQueryInterface(service.get(), getter_AddRefs(chromeService));
    MOZ_ASSERT(!chromeService);
  }
#endif
  return service.forget().downcast<GeckoMediaPluginServiceChild>();
}

class GetContentParentFromDone : public GetServiceChildCallback
{
public:
  GetContentParentFromDone(const nsACString& aNodeId, const nsCString& aAPI,
                           const nsTArray<nsCString>& aTags,
                           UniquePtr<GetGMPContentParentCallback>&& aCallback)
    : mNodeId(aNodeId),
      mAPI(aAPI),
      mTags(aTags),
      mCallback(Move(aCallback))
  {
  }

  virtual void Done(GMPServiceChild* aGMPServiceChild)
  {
    if (!aGMPServiceChild) {
      mCallback->Done(nullptr);
      return;
    }

    nsTArray<base::ProcessId> alreadyBridgedTo;
    aGMPServiceChild->GetAlreadyBridgedTo(alreadyBridgedTo);

    base::ProcessId otherProcess;
    nsCString displayName;
    uint32_t pluginId;
    bool ok = aGMPServiceChild->SendLoadGMP(mNodeId, mAPI, mTags,
                                            alreadyBridgedTo, &otherProcess,
                                            &displayName, &pluginId);
    if (!ok) {
      mCallback->Done(nullptr);
      return;
    }

    nsRefPtr<GMPContentParent> parent;
    aGMPServiceChild->GetBridgedGMPContentParent(otherProcess,
                                                 getter_AddRefs(parent));
    if (!alreadyBridgedTo.Contains(otherProcess)) {
      parent->SetDisplayName(displayName);
      parent->SetPluginId(pluginId);
    }

    mCallback->Done(parent);
  }

private:
  nsCString mNodeId;
  nsCString mAPI;
  const nsTArray<nsCString> mTags;
  UniquePtr<GetGMPContentParentCallback> mCallback;
};

bool
GeckoMediaPluginServiceChild::GetContentParentFrom(const nsACString& aNodeId,
                                                   const nsCString& aAPI,
                                                   const nsTArray<nsCString>& aTags,
                                                   UniquePtr<GetGMPContentParentCallback>&& aCallback)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);

  UniquePtr<GetServiceChildCallback> callback(
    new GetContentParentFromDone(aNodeId, aAPI, aTags, Move(aCallback)));
  GetServiceChild(Move(callback));

  return true;
}

NS_IMETHODIMP
GeckoMediaPluginServiceChild::GetPluginVersionForAPI(const nsACString& aAPI,
                                                     nsTArray<nsCString>* aTags,
                                                     bool* aHasPlugin,
                                                     nsACString& aOutVersion)
{
  dom::ContentChild* contentChild = dom::ContentChild::GetSingleton();
  if (!contentChild) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(NS_IsMainThread());

  nsCString version;
  bool ok = contentChild->SendGetGMPPluginVersionForAPI(nsCString(aAPI), *aTags,
                                                        aHasPlugin, &version);
  aOutVersion = version;
  return ok ? NS_OK : NS_ERROR_FAILURE;
}

class GetNodeIdDone : public GetServiceChildCallback
{
public:
  GetNodeIdDone(const nsAString& aOrigin, const nsAString& aTopLevelOrigin,
                bool aInPrivateBrowsing, UniquePtr<GetNodeIdCallback>&& aCallback)
    : mOrigin(aOrigin),
      mTopLevelOrigin(aTopLevelOrigin),
      mInPrivateBrowsing(aInPrivateBrowsing),
      mCallback(Move(aCallback))
  {
  }

  virtual void Done(GMPServiceChild* aGMPServiceChild)
  {
    if (!aGMPServiceChild) {
      mCallback->Done(NS_ERROR_FAILURE, EmptyCString());
      return;
    }

    nsCString outId;
    if (!aGMPServiceChild->SendGetGMPNodeId(mOrigin, mTopLevelOrigin,
                                            mInPrivateBrowsing, &outId)) {
      mCallback->Done(NS_ERROR_FAILURE, EmptyCString());
      return;
    }

    mCallback->Done(NS_OK, outId);
  }

private:
  nsString mOrigin;
  nsString mTopLevelOrigin;
  bool mInPrivateBrowsing;
  UniquePtr<GetNodeIdCallback> mCallback;
};

NS_IMETHODIMP
GeckoMediaPluginServiceChild::GetNodeId(const nsAString& aOrigin,
                                        const nsAString& aTopLevelOrigin,
                                        bool aInPrivateBrowsing,
                                        UniquePtr<GetNodeIdCallback>&& aCallback)
{
  UniquePtr<GetServiceChildCallback> callback(
    new GetNodeIdDone(aOrigin, aTopLevelOrigin, aInPrivateBrowsing, Move(aCallback)));
  GetServiceChild(Move(callback));
  return NS_OK;
}

NS_IMETHODIMP
GeckoMediaPluginServiceChild::UpdateTrialCreateState(const nsAString& aKeySystem,
                                                     uint32_t aState)
{
  if (NS_GetCurrentThread() != mGMPThread) {
    mGMPThread->Dispatch(NS_NewRunnableMethodWithArgs<nsString, uint32_t>(
      this, &GeckoMediaPluginServiceChild::UpdateTrialCreateState,
      aKeySystem, aState), NS_DISPATCH_NORMAL);
    return NS_OK;
  }

  class Callback : public GetServiceChildCallback
  {
  public:
    Callback(const nsAString& aKeySystem, uint32_t aState)
      : mKeySystem(aKeySystem)
      , mState(aState)
    { }

    virtual void Done(GMPServiceChild* aService) override
    {
      aService->SendUpdateGMPTrialCreateState(mKeySystem, mState);
    }

  private:
    nsString mKeySystem;
    uint32_t mState;
  };

  UniquePtr<GetServiceChildCallback> callback(new Callback(aKeySystem, aState));
  GetServiceChild(Move(callback));
  return NS_OK;
}

NS_IMETHODIMP
GeckoMediaPluginServiceChild::Observe(nsISupports* aSubject,
                                      const char* aTopic,
                                      const char16_t* aSomeData)
{
  LOGD(("%s::%s: %s", __CLASS__, __FUNCTION__, aTopic));
  if (!strcmp(NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID, aTopic)) {
    if (mServiceChild) {
      mozilla::SyncRunnable::DispatchToThread(mGMPThread,
                                              WrapRunnable(mServiceChild.get(),
                                                           &PGMPServiceChild::Close));
      mServiceChild = nullptr;
    }
    ShutdownGMPThread();
  }

  return NS_OK;
}

void
GeckoMediaPluginServiceChild::GetServiceChild(UniquePtr<GetServiceChildCallback>&& aCallback)
{
  MOZ_ASSERT(!NS_IsMainThread());

  if (!mServiceChild) {
    dom::ContentChild* contentChild = dom::ContentChild::GetSingleton();
    if (!contentChild) {
      return;
    }
    mGetServiceChildCallbacks.AppendElement(Move(aCallback));
    if (mGetServiceChildCallbacks.Length() == 1) {
        NS_DispatchToMainThread(WrapRunnable(contentChild,
                                             &dom::ContentChild::SendCreateGMPService));
    }
    return;
  }

  aCallback->Done(mServiceChild.get());
}

void
GeckoMediaPluginServiceChild::SetServiceChild(UniquePtr<GMPServiceChild>&& aServiceChild)
{
  mServiceChild = Move(aServiceChild);
  nsTArray<UniquePtr<GetServiceChildCallback>> getServiceChildCallbacks;
  getServiceChildCallbacks.SwapElements(mGetServiceChildCallbacks);
  for (uint32_t i = 0, length = getServiceChildCallbacks.Length(); i < length; ++i) {
    getServiceChildCallbacks[i]->Done(mServiceChild.get());
  }
}

void
GeckoMediaPluginServiceChild::RemoveGMPContentParent(GMPContentParent* aGMPContentParent)
{
  if (mServiceChild) {
    mServiceChild->RemoveGMPContentParent(aGMPContentParent);
  }
}

GMPServiceChild::GMPServiceChild()
{
}

GMPServiceChild::~GMPServiceChild()
{
  XRE_GetIOMessageLoop()->PostTask(FROM_HERE,
                                   new DeleteTask<Transport>(GetTransport()));
}

PGMPContentParent*
GMPServiceChild::AllocPGMPContentParent(Transport* aTransport,
                                        ProcessId aOtherPid)
{
  MOZ_ASSERT(!mContentParents.GetWeak(aOtherPid));

  nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
  MOZ_ASSERT(mainThread);

  nsRefPtr<GMPContentParent> parent = new GMPContentParent();

  DebugOnly<bool> ok = parent->Open(aTransport, aOtherPid,
                                    XRE_GetIOMessageLoop(),
                                    mozilla::ipc::ParentSide);
  MOZ_ASSERT(ok);

  mContentParents.Put(aOtherPid, parent);
  return parent;
}

void
GMPServiceChild::GetBridgedGMPContentParent(ProcessId aOtherPid,
                                            GMPContentParent** aGMPContentParent)
{
  mContentParents.Get(aOtherPid, aGMPContentParent);
}

void
GMPServiceChild::RemoveGMPContentParent(GMPContentParent* aGMPContentParent)
{
  for (auto iter = mContentParents.Iter(); !iter.Done(); iter.Next()) {
    nsRefPtr<GMPContentParent>& parent = iter.Data();
    if (parent == aGMPContentParent) {
      iter.Remove();
      break;
    }
  }
}

void
GMPServiceChild::GetAlreadyBridgedTo(nsTArray<base::ProcessId>& aAlreadyBridgedTo)
{
  aAlreadyBridgedTo.SetCapacity(mContentParents.Count());
  for (auto iter = mContentParents.Iter(); !iter.Done(); iter.Next()) {
    const uint64_t& id = iter.Key();
    aAlreadyBridgedTo.AppendElement(id);
  }
}

class OpenPGMPServiceChild : public nsRunnable
{
public:
  OpenPGMPServiceChild(UniquePtr<GMPServiceChild>&& aGMPServiceChild,
                       mozilla::ipc::Transport* aTransport,
                       base::ProcessId aOtherPid)
    : mGMPServiceChild(Move(aGMPServiceChild)),
      mTransport(aTransport),
      mOtherPid(aOtherPid)
  {
  }

  NS_IMETHOD Run()
  {
    nsRefPtr<GeckoMediaPluginServiceChild> gmp =
      GeckoMediaPluginServiceChild::GetSingleton();
    MOZ_ASSERT(!gmp->mServiceChild);
    if (mGMPServiceChild->Open(mTransport, mOtherPid, XRE_GetIOMessageLoop(),
                               ipc::ChildSide)) {
      gmp->SetServiceChild(Move(mGMPServiceChild));
    } else {
      gmp->SetServiceChild(nullptr);
    }
    return NS_OK;
  }

private:
  UniquePtr<GMPServiceChild> mGMPServiceChild;
  mozilla::ipc::Transport* mTransport;
  base::ProcessId mOtherPid;
};

/* static */
PGMPServiceChild*
GMPServiceChild::Create(Transport* aTransport, ProcessId aOtherPid)
{
  nsRefPtr<GeckoMediaPluginServiceChild> gmp =
    GeckoMediaPluginServiceChild::GetSingleton();
  MOZ_ASSERT(!gmp->mServiceChild);

  UniquePtr<GMPServiceChild> serviceChild(new GMPServiceChild());

  nsCOMPtr<nsIThread> gmpThread;
  nsresult rv = gmp->GetThread(getter_AddRefs(gmpThread));
  NS_ENSURE_SUCCESS(rv, nullptr);

  GMPServiceChild* result = serviceChild.get();
  rv = gmpThread->Dispatch(new OpenPGMPServiceChild(Move(serviceChild),
                                                    aTransport,
                                                    aOtherPid),
                           NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  return result;
}

} // namespace gmp
} // namespace mozilla

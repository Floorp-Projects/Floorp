/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerLoadInfo.h"
#include "WorkerPrivate.h"

#include "mozilla/dom/TabChild.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/LoadContext.h"
#include "nsContentUtils.h"
#include "nsIContentSecurityPolicy.h"
#include "nsINetworkInterceptController.h"
#include "nsIProtocolHandler.h"
#include "nsITabChild.h"
#include "nsScriptSecurityManager.h"
#include "nsNetUtil.h"

namespace mozilla {

using namespace ipc;

namespace dom {

namespace {

class MainThreadReleaseRunnable final : public Runnable
{
  nsTArray<nsCOMPtr<nsISupports>> mDoomed;
  nsCOMPtr<nsILoadGroup> mLoadGroupToCancel;

public:
  MainThreadReleaseRunnable(nsTArray<nsCOMPtr<nsISupports>>& aDoomed,
                            nsCOMPtr<nsILoadGroup>& aLoadGroupToCancel)
    : mozilla::Runnable("MainThreadReleaseRunnable")
  {
    mDoomed.SwapElements(aDoomed);
    mLoadGroupToCancel.swap(aLoadGroupToCancel);
  }

  NS_INLINE_DECL_REFCOUNTING_INHERITED(MainThreadReleaseRunnable, Runnable)

  NS_IMETHOD
  Run() override
  {
    if (mLoadGroupToCancel) {
      mLoadGroupToCancel->Cancel(NS_BINDING_ABORTED);
      mLoadGroupToCancel = nullptr;
    }

    mDoomed.Clear();
    return NS_OK;
  }

private:
  ~MainThreadReleaseRunnable()
  { }
};

// Specialize this if there's some class that has multiple nsISupports bases.
template <class T>
struct ISupportsBaseInfo
{
  typedef T ISupportsBase;
};

template <template <class> class SmartPtr, class T>
inline void
SwapToISupportsArray(SmartPtr<T>& aSrc,
                     nsTArray<nsCOMPtr<nsISupports> >& aDest)
{
  nsCOMPtr<nsISupports>* dest = aDest.AppendElement();

  T* raw = nullptr;
  aSrc.swap(raw);

  nsISupports* rawSupports =
    static_cast<typename ISupportsBaseInfo<T>::ISupportsBase*>(raw);
  dest->swap(rawSupports);
}

} // anonymous

WorkerLoadInfo::WorkerLoadInfo()
  : mLoadFlags(nsIRequest::LOAD_NORMAL)
  , mWindowID(UINT64_MAX)
  , mReferrerPolicy(net::RP_Unset)
  , mFromWindow(false)
  , mEvalAllowed(false)
  , mReportCSPViolations(false)
  , mXHRParamsAllowed(false)
  , mPrincipalIsSystem(false)
  , mStorageAllowed(false)
  , mFirstPartyStorageAccessGranted(false)
  , mServiceWorkersTestingInWindow(false)
{
  MOZ_COUNT_CTOR(WorkerLoadInfo);
}

WorkerLoadInfo::~WorkerLoadInfo()
{
  MOZ_COUNT_DTOR(WorkerLoadInfo);
}

void
WorkerLoadInfo::StealFrom(WorkerLoadInfo& aOther)
{
  MOZ_ASSERT(!mBaseURI);
  aOther.mBaseURI.swap(mBaseURI);

  MOZ_ASSERT(!mResolvedScriptURI);
  aOther.mResolvedScriptURI.swap(mResolvedScriptURI);

  MOZ_ASSERT(!mPrincipal);
  aOther.mPrincipal.swap(mPrincipal);

  // mLoadingPrincipal can be null if this is a ServiceWorker.
  aOther.mLoadingPrincipal.swap(mLoadingPrincipal);

  MOZ_ASSERT(!mScriptContext);
  aOther.mScriptContext.swap(mScriptContext);

  MOZ_ASSERT(!mWindow);
  aOther.mWindow.swap(mWindow);

  MOZ_ASSERT(!mCSP);
  aOther.mCSP.swap(mCSP);

  MOZ_ASSERT(!mChannel);
  aOther.mChannel.swap(mChannel);

  MOZ_ASSERT(!mLoadGroup);
  aOther.mLoadGroup.swap(mLoadGroup);

  MOZ_ASSERT(!mInterfaceRequestor);
  aOther.mInterfaceRequestor.swap(mInterfaceRequestor);

  MOZ_ASSERT(!mPrincipalInfo);
  mPrincipalInfo = aOther.mPrincipalInfo.forget();

  mDomain = aOther.mDomain;
  mOrigin = aOther.mOrigin;
  mServiceWorkerCacheName = aOther.mServiceWorkerCacheName;
  mServiceWorkerDescriptor = aOther.mServiceWorkerDescriptor;
  mServiceWorkerRegistrationDescriptor = aOther.mServiceWorkerRegistrationDescriptor;
  mLoadFlags = aOther.mLoadFlags;
  mWindowID = aOther.mWindowID;
  mReferrerPolicy = aOther.mReferrerPolicy;
  mFromWindow = aOther.mFromWindow;
  mEvalAllowed = aOther.mEvalAllowed;
  mReportCSPViolations = aOther.mReportCSPViolations;
  mXHRParamsAllowed = aOther.mXHRParamsAllowed;
  mPrincipalIsSystem = aOther.mPrincipalIsSystem;
  mStorageAllowed = aOther.mStorageAllowed;
  mFirstPartyStorageAccessGranted = aOther.mFirstPartyStorageAccessGranted;
  mServiceWorkersTestingInWindow = aOther.mServiceWorkersTestingInWindow;
  mOriginAttributes = aOther.mOriginAttributes;
  mParentController = aOther.mParentController;
}

nsresult
WorkerLoadInfo::SetPrincipalOnMainThread(nsIPrincipal* aPrincipal,
                                         nsILoadGroup* aLoadGroup)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(NS_LoadGroupMatchesPrincipal(aLoadGroup, aPrincipal));

  mPrincipal = aPrincipal;
  mPrincipalIsSystem = nsContentUtils::IsSystemPrincipal(aPrincipal);

  nsresult rv = aPrincipal->GetCsp(getter_AddRefs(mCSP));
  NS_ENSURE_SUCCESS(rv, rv);

  if (mCSP) {
    mCSP->GetAllowsEval(&mReportCSPViolations, &mEvalAllowed);
  } else {
    mEvalAllowed = true;
    mReportCSPViolations = false;
  }

  mLoadGroup = aLoadGroup;

  mPrincipalInfo = new PrincipalInfo();
  mOriginAttributes = nsContentUtils::GetOriginAttributes(aLoadGroup);

  rv = PrincipalToPrincipalInfo(aPrincipal, mPrincipalInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = nsContentUtils::GetUTFOrigin(aPrincipal, mOrigin);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
WorkerLoadInfo::GetPrincipalAndLoadGroupFromChannel(nsIChannel* aChannel,
                                                    nsIPrincipal** aPrincipalOut,
                                                    nsILoadGroup** aLoadGroupOut)
{
  AssertIsOnMainThread();
  MOZ_DIAGNOSTIC_ASSERT(aChannel);
  MOZ_DIAGNOSTIC_ASSERT(aPrincipalOut);
  MOZ_DIAGNOSTIC_ASSERT(aLoadGroupOut);

  // Initial triggering principal should be set
  NS_ENSURE_TRUE(mLoadingPrincipal, NS_ERROR_DOM_INVALID_STATE_ERR);

  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  MOZ_DIAGNOSTIC_ASSERT(ssm);

  nsCOMPtr<nsIPrincipal> channelPrincipal;
  nsresult rv = ssm->GetChannelResultPrincipal(aChannel, getter_AddRefs(channelPrincipal));
  NS_ENSURE_SUCCESS(rv, rv);

  // Every time we call GetChannelResultPrincipal() it will return a different
  // null principal for a data URL.  We don't want to change the worker's
  // principal again, though.  Instead just keep the original null principal we
  // first got from the channel.
  //
  // Note, we don't do this by setting principalToInherit on the channel's
  // load info because we don't yet have the first null principal when we
  // create the channel.
  if (mPrincipal && mPrincipal->GetIsNullPrincipal() &&
                    channelPrincipal->GetIsNullPrincipal()) {
    channelPrincipal = mPrincipal;
  }

  nsCOMPtr<nsILoadGroup> channelLoadGroup;
  rv = aChannel->GetLoadGroup(getter_AddRefs(channelLoadGroup));
  NS_ENSURE_SUCCESS(rv, rv);
  MOZ_ASSERT(channelLoadGroup);

  // If the loading principal is the system principal then the channel
  // principal must also be the system principal (we do not allow chrome
  // code to create workers with non-chrome scripts, and if we ever decide
  // to change this we need to make sure we don't always set
  // mPrincipalIsSystem to true in WorkerPrivate::GetLoadInfo()). Otherwise
  // this channel principal must be same origin with the load principal (we
  // check again here in case redirects changed the location of the script).
  if (nsContentUtils::IsSystemPrincipal(mLoadingPrincipal)) {
    if (!nsContentUtils::IsSystemPrincipal(channelPrincipal)) {
      nsCOMPtr<nsIURI> finalURI;
      rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(finalURI));
      NS_ENSURE_SUCCESS(rv, rv);

      // See if this is a resource URI. Since JSMs usually come from
      // resource:// URIs we're currently considering all URIs with the
      // URI_IS_UI_RESOURCE flag as valid for creating privileged workers.
      bool isResource;
      rv = NS_URIChainHasFlags(finalURI,
                               nsIProtocolHandler::URI_IS_UI_RESOURCE,
                               &isResource);
      NS_ENSURE_SUCCESS(rv, rv);

      if (isResource) {
        // Assign the system principal to the resource:// worker only if it
        // was loaded from code using the system principal.
        channelPrincipal = mLoadingPrincipal;
      } else {
        return NS_ERROR_DOM_BAD_URI;
      }
    }
  }

  // The principal can change, but it should still match the original
  // load group's appId and browser element flag.
  MOZ_ASSERT(NS_LoadGroupMatchesPrincipal(channelLoadGroup, channelPrincipal));

  channelPrincipal.forget(aPrincipalOut);
  channelLoadGroup.forget(aLoadGroupOut);

  return NS_OK;
}

nsresult
WorkerLoadInfo::SetPrincipalFromChannel(nsIChannel* aChannel)
{
  AssertIsOnMainThread();

  nsCOMPtr<nsIPrincipal> principal;
  nsCOMPtr<nsILoadGroup> loadGroup;
  nsresult rv = GetPrincipalAndLoadGroupFromChannel(aChannel,
                                                    getter_AddRefs(principal),
                                                    getter_AddRefs(loadGroup));
  NS_ENSURE_SUCCESS(rv, rv);

  return SetPrincipalOnMainThread(principal, loadGroup);
}

bool
WorkerLoadInfo::FinalChannelPrincipalIsValid(nsIChannel* aChannel)
{
  AssertIsOnMainThread();

  nsCOMPtr<nsIPrincipal> principal;
  nsCOMPtr<nsILoadGroup> loadGroup;
  nsresult rv = GetPrincipalAndLoadGroupFromChannel(aChannel,
                                                    getter_AddRefs(principal),
                                                    getter_AddRefs(loadGroup));
  NS_ENSURE_SUCCESS(rv, false);


  // Verify that the channel is still a null principal.  We don't care
  // if these are the exact same null principal object, though.  From
  // the worker's perspective its the same effect.
  if (principal->GetIsNullPrincipal() && mPrincipal->GetIsNullPrincipal()) {
    return true;
  }

  // Otherwise we require exact equality.  Redirects can happen, but they
  // are not allowed to change our principal.
  if (principal->Equals(mPrincipal)) {
    return true;
  }

  return false;
}

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
bool
WorkerLoadInfo::PrincipalIsValid() const
{
  return mPrincipal && mPrincipalInfo &&
         mPrincipalInfo->type() != PrincipalInfo::T__None &&
         mPrincipalInfo->type() <= PrincipalInfo::T__Last;
}

bool
WorkerLoadInfo::PrincipalURIMatchesScriptURL()
{
  AssertIsOnMainThread();

  nsAutoCString scheme;
  nsresult rv = mBaseURI->GetScheme(scheme);
  NS_ENSURE_SUCCESS(rv, false);

  // A system principal must either be a blob URL or a resource JSM.
  if (mPrincipal->GetIsSystemPrincipal()) {
    if (scheme == NS_LITERAL_CSTRING("blob")) {
      return true;
    }

    bool isResource = false;
    nsresult rv = NS_URIChainHasFlags(mBaseURI,
                                      nsIProtocolHandler::URI_IS_UI_RESOURCE,
                                      &isResource);
    NS_ENSURE_SUCCESS(rv, false);

    return isResource;
  }

  // A null principal can occur for a data URL worker script or a blob URL
  // worker script from a sandboxed iframe.
  if (mPrincipal->GetIsNullPrincipal()) {
    return scheme == NS_LITERAL_CSTRING("data") ||
           scheme == NS_LITERAL_CSTRING("blob");
  }

  // The principal for a blob: URL worker script does not have a matching URL.
  // This is likely a bug in our referer setting logic, but exempt it for now.
  // This is another reason we should fix bug 1340694 so that referer does not
  // depend on the principal URI.
  if (scheme == NS_LITERAL_CSTRING("blob")) {
    return true;
  }

  nsCOMPtr<nsIURI> principalURI;
  rv = mPrincipal->GetURI(getter_AddRefs(principalURI));
  NS_ENSURE_SUCCESS(rv, false);
  NS_ENSURE_TRUE(principalURI, false);

  if (nsScriptSecurityManager::SecurityCompareURIs(mBaseURI, principalURI)) {
    return true;
  }

  // If strict file origin policy is in effect, local files will always fail
  // SecurityCompareURIs unless they are identical. Explicitly check file origin
  // policy, in that case.
  if (nsScriptSecurityManager::GetStrictFileOriginPolicy() &&
      NS_URIIsLocalFile(mBaseURI) &&
      NS_RelaxStrictFileOriginPolicy(mBaseURI, principalURI)) {
    return true;
  }

  return false;
}
#endif // MOZ_DIAGNOSTIC_ASSERT_ENABLED

bool
WorkerLoadInfo::ProxyReleaseMainThreadObjects(WorkerPrivate* aWorkerPrivate)
{
  nsCOMPtr<nsILoadGroup> nullLoadGroup;
  return ProxyReleaseMainThreadObjects(aWorkerPrivate, nullLoadGroup);
}

bool
WorkerLoadInfo::ProxyReleaseMainThreadObjects(WorkerPrivate* aWorkerPrivate,
                                              nsCOMPtr<nsILoadGroup>& aLoadGroupToCancel)
{

  static const uint32_t kDoomedCount = 10;
  nsTArray<nsCOMPtr<nsISupports>> doomed(kDoomedCount);

  SwapToISupportsArray(mWindow, doomed);
  SwapToISupportsArray(mScriptContext, doomed);
  SwapToISupportsArray(mBaseURI, doomed);
  SwapToISupportsArray(mResolvedScriptURI, doomed);
  SwapToISupportsArray(mPrincipal, doomed);
  SwapToISupportsArray(mLoadingPrincipal, doomed);
  SwapToISupportsArray(mChannel, doomed);
  SwapToISupportsArray(mCSP, doomed);
  SwapToISupportsArray(mLoadGroup, doomed);
  SwapToISupportsArray(mInterfaceRequestor, doomed);
  // Before adding anything here update kDoomedCount above!

  MOZ_ASSERT(doomed.Length() == kDoomedCount);

  RefPtr<MainThreadReleaseRunnable> runnable =
    new MainThreadReleaseRunnable(doomed, aLoadGroupToCancel);
  return NS_SUCCEEDED(aWorkerPrivate->DispatchToMainThread(runnable.forget()));
}

WorkerLoadInfo::
InterfaceRequestor::InterfaceRequestor(nsIPrincipal* aPrincipal,
                                       nsILoadGroup* aLoadGroup)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  // Look for an existing LoadContext.  This is optional and it's ok if
  // we don't find one.
  nsCOMPtr<nsILoadContext> baseContext;
  if (aLoadGroup) {
    nsCOMPtr<nsIInterfaceRequestor> callbacks;
    aLoadGroup->GetNotificationCallbacks(getter_AddRefs(callbacks));
    if (callbacks) {
      callbacks->GetInterface(NS_GET_IID(nsILoadContext),
                              getter_AddRefs(baseContext));
    }
    mOuterRequestor = callbacks;
  }

  mLoadContext = new LoadContext(aPrincipal, baseContext);
}

void
WorkerLoadInfo::
InterfaceRequestor::MaybeAddTabChild(nsILoadGroup* aLoadGroup)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!aLoadGroup) {
    return;
  }

  nsCOMPtr<nsIInterfaceRequestor> callbacks;
  aLoadGroup->GetNotificationCallbacks(getter_AddRefs(callbacks));
  if (!callbacks) {
    return;
  }

  nsCOMPtr<nsITabChild> tabChild;
  callbacks->GetInterface(NS_GET_IID(nsITabChild), getter_AddRefs(tabChild));
  if (!tabChild) {
    return;
  }

  // Use weak references to the tab child.  Holding a strong reference will
  // not prevent an ActorDestroy() from being called on the TabChild.
  // Therefore, we should let the TabChild destroy itself as soon as possible.
  mTabChildList.AppendElement(do_GetWeakReference(tabChild));
}

NS_IMETHODIMP
WorkerLoadInfo::
InterfaceRequestor::GetInterface(const nsIID& aIID, void** aSink)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mLoadContext);

  if (aIID.Equals(NS_GET_IID(nsILoadContext))) {
    nsCOMPtr<nsILoadContext> ref = mLoadContext;
    ref.forget(aSink);
    return NS_OK;
  }

  // If we still have an active nsITabChild, then return it.  Its possible,
  // though, that all of the TabChild objects have been destroyed.  In that
  // case we return NS_NOINTERFACE.
  if (aIID.Equals(NS_GET_IID(nsITabChild))) {
    nsCOMPtr<nsITabChild> tabChild = GetAnyLiveTabChild();
    if (!tabChild) {
      return NS_NOINTERFACE;
    }
    tabChild.forget(aSink);
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsINetworkInterceptController)) &&
      mOuterRequestor) {
    // If asked for the network intercept controller, ask the outer requestor,
    // which could be the docshell.
    return mOuterRequestor->GetInterface(aIID, aSink);
  }

  return NS_NOINTERFACE;
}

already_AddRefed<nsITabChild>
WorkerLoadInfo::
InterfaceRequestor::GetAnyLiveTabChild()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Search our list of known TabChild objects for one that still exists.
  while (!mTabChildList.IsEmpty()) {
    nsCOMPtr<nsITabChild> tabChild =
      do_QueryReferent(mTabChildList.LastElement());

    // Does this tab child still exist?  If so, return it.  We are done.  If the
    // PBrowser actor is no longer useful, don't bother returning this tab.
    if (tabChild && !static_cast<TabChild*>(tabChild.get())->IsDestroyed()) {
      return tabChild.forget();
    }

    // Otherwise remove the stale weak reference and check the next one
    mTabChildList.RemoveLastElement();
  }

  return nullptr;
}

NS_IMPL_ADDREF(WorkerLoadInfo::InterfaceRequestor)
NS_IMPL_RELEASE(WorkerLoadInfo::InterfaceRequestor)
NS_IMPL_QUERY_INTERFACE(WorkerLoadInfo::InterfaceRequestor,
                        nsIInterfaceRequestor)

} // dom namespace
} // mozilla namespace

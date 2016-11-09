/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPService.h"
#include "GMPServiceParent.h"
#include "GMPServiceChild.h"
#include "GMPContentParent.h"
#include "prio.h"
#include "mozilla/Logging.h"
#include "GMPParent.h"
#include "GMPVideoDecoderParent.h"
#include "nsIObserverService.h"
#include "GeckoChildProcessHost.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/SyncRunnable.h"
#include "nsXPCOMPrivate.h"
#include "mozilla/Services.h"
#include "nsNativeCharsetUtils.h"
#include "nsIConsoleService.h"
#include "mozilla/Unused.h"
#include "GMPDecryptorParent.h"
#include "GMPAudioDecoderParent.h"
#include "nsComponentManagerUtils.h"
#include "runnable_utils.h"
#include "VideoUtils.h"
#if defined(XP_LINUX) && defined(MOZ_GMP_SANDBOX)
#include "mozilla/SandboxInfo.h"
#endif
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsHashKeys.h"
#include "nsIFile.h"
#include "nsISimpleEnumerator.h"
#include "nsThreadUtils.h"
#include "GMPCrashHelper.h"

#include "mozilla/dom/PluginCrashedEvent.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/Attributes.h"

namespace mozilla {

#ifdef LOG
#undef LOG
#endif

LogModule*
GetGMPLog()
{
  static LazyLogModule sLog("GMP");
  return sLog;
}

#define LOGD(msg) MOZ_LOG(GetGMPLog(), mozilla::LogLevel::Debug, msg)
#define LOG(level, msg) MOZ_LOG(GetGMPLog(), (level), msg)

#ifdef __CLASS__
#undef __CLASS__
#endif
#define __CLASS__ "GMPService"

namespace gmp {

static StaticRefPtr<GeckoMediaPluginService> sSingletonService;

class GMPServiceCreateHelper final : public mozilla::Runnable
{
  RefPtr<GeckoMediaPluginService> mService;

public:
  static already_AddRefed<GeckoMediaPluginService>
  GetOrCreate()
  {
    RefPtr<GeckoMediaPluginService> service;

    if (NS_IsMainThread()) {
      service = GetOrCreateOnMainThread();
    } else {
      nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
      MOZ_ASSERT(mainThread);

      RefPtr<GMPServiceCreateHelper> createHelper = new GMPServiceCreateHelper();

      mozilla::SyncRunnable::DispatchToThread(mainThread, createHelper, true);

      service = createHelper->mService.forget();
    }

    return service.forget();
  }

private:
  GMPServiceCreateHelper()
  {
  }

  ~GMPServiceCreateHelper()
  {
    MOZ_ASSERT(!mService);
  }

  static already_AddRefed<GeckoMediaPluginService>
  GetOrCreateOnMainThread()
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (!sSingletonService) {
      if (XRE_IsParentProcess()) {
        RefPtr<GeckoMediaPluginServiceParent> service =
          new GeckoMediaPluginServiceParent();
        service->Init();
        sSingletonService = service;
      } else {
        RefPtr<GeckoMediaPluginServiceChild> service =
          new GeckoMediaPluginServiceChild();
        service->Init();
        sSingletonService = service;
      }

      ClearOnShutdown(&sSingletonService);
    }

    RefPtr<GeckoMediaPluginService> service = sSingletonService.get();
    return service.forget();
  }

  NS_IMETHOD
  Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    mService = GetOrCreateOnMainThread();
    return NS_OK;
  }
};

already_AddRefed<GeckoMediaPluginService>
GeckoMediaPluginService::GetGeckoMediaPluginService()
{
  return GMPServiceCreateHelper::GetOrCreate();
}

NS_IMPL_ISUPPORTS(GeckoMediaPluginService, mozIGeckoMediaPluginService, nsIObserver)

GeckoMediaPluginService::GeckoMediaPluginService()
  : mMutex("GeckoMediaPluginService::mMutex")
  , mGMPThreadShutdown(false)
  , mShuttingDownOnGMPThread(false)
{
  MOZ_ASSERT(NS_IsMainThread());
}

GeckoMediaPluginService::~GeckoMediaPluginService()
{
}

NS_IMETHODIMP
GeckoMediaPluginService::RunPluginCrashCallbacks(uint32_t aPluginId,
                                                 const nsACString& aPluginName)
{
  MOZ_ASSERT(NS_IsMainThread());
  LOGD(("%s::%s(%i)", __CLASS__, __FUNCTION__, aPluginId));

  nsAutoPtr<nsTArray<RefPtr<GMPCrashHelper>>> helpers;
  {
    MutexAutoLock lock(mMutex);
    mPluginCrashHelpers.RemoveAndForget(aPluginId, helpers);
  }
  if (!helpers) {
    LOGD(("%s::%s(%i) No crash helpers, not handling crash.", __CLASS__, __FUNCTION__, aPluginId));
    return NS_OK;
  }

  for (const auto& helper : *helpers) {
    nsCOMPtr<nsPIDOMWindowInner> window = helper->GetPluginCrashedEventTarget();
    if (NS_WARN_IF(!window)) {
      continue;
    }
    nsCOMPtr<nsIDocument> document(window->GetExtantDoc());
    if (NS_WARN_IF(!document)) {
      continue;
    }

    dom::PluginCrashedEventInit init;
    init.mPluginID = aPluginId;
    init.mBubbles = true;
    init.mCancelable = true;
    init.mGmpPlugin = true;
    CopyUTF8toUTF16(aPluginName, init.mPluginName);
    init.mSubmittedCrashReport = false;
    RefPtr<dom::PluginCrashedEvent> event =
      dom::PluginCrashedEvent::Constructor(document,
                                           NS_LITERAL_STRING("PluginCrashed"),
                                           init);
    event->SetTrusted(true);
    event->WidgetEventPtr()->mFlags.mOnlyChromeDispatch = true;

    EventDispatcher::DispatchDOMEvent(window, nullptr, event, nullptr, nullptr);
  }

  return NS_OK;
}

nsresult
GeckoMediaPluginService::Init()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obsService = mozilla::services::GetObserverService();
  MOZ_ASSERT(obsService);
  MOZ_ALWAYS_SUCCEEDS(obsService->AddObserver(this, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID, false));

  // Kick off scanning for plugins
  nsCOMPtr<nsIThread> thread;
  return GetThread(getter_AddRefs(thread));
}

void
GeckoMediaPluginService::ShutdownGMPThread()
{
  LOGD(("%s::%s", __CLASS__, __FUNCTION__));
  nsCOMPtr<nsIThread> gmpThread;
  {
    MutexAutoLock lock(mMutex);
    mGMPThreadShutdown = true;
    mGMPThread.swap(gmpThread);
    mAbstractGMPThread = nullptr;
  }

  if (gmpThread) {
    gmpThread->Shutdown();
  }
}

nsresult
GeckoMediaPluginService::GMPDispatch(nsIRunnable* event,
                                     uint32_t flags)
{
  nsCOMPtr<nsIRunnable> r(event);
  return GMPDispatch(r.forget());
}

nsresult
GeckoMediaPluginService::GMPDispatch(already_AddRefed<nsIRunnable> event,
                                     uint32_t flags)
{
  nsCOMPtr<nsIRunnable> r(event);
  nsCOMPtr<nsIThread> thread;
  nsresult rv = GetThread(getter_AddRefs(thread));
  if (NS_FAILED(rv)) {
    return rv;
  }
  return thread->Dispatch(r, flags);
}

// always call with getter_AddRefs, because it does
NS_IMETHODIMP
GeckoMediaPluginService::GetThread(nsIThread** aThread)
{
  MOZ_ASSERT(aThread);

  // This can be called from any thread.
  MutexAutoLock lock(mMutex);

  if (!mGMPThread) {
    // Don't allow the thread to be created after shutdown has started.
    if (mGMPThreadShutdown) {
      return NS_ERROR_FAILURE;
    }

    nsresult rv = NS_NewNamedThread("GMPThread", getter_AddRefs(mGMPThread));
    if (NS_FAILED(rv)) {
      return rv;
    }

    mAbstractGMPThread = AbstractThread::CreateXPCOMThreadWrapper(mGMPThread, false);

    // Tell the thread to initialize plugins
    InitializePlugins(mAbstractGMPThread.get());
  }

  nsCOMPtr<nsIThread> copy = mGMPThread;
  copy.forget(aThread);

  return NS_OK;
}

RefPtr<AbstractThread>
GeckoMediaPluginService::GetAbstractGMPThread()
{
  MutexAutoLock lock(mMutex);
  return mAbstractGMPThread;
}

NS_IMETHODIMP
GeckoMediaPluginService::GetGMPAudioDecoder(GMPCrashHelper* aHelper,
                                            nsTArray<nsCString>* aTags,
                                            const nsACString& aNodeId,
                                            UniquePtr<GetGMPAudioDecoderCallback>&& aCallback)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
  NS_ENSURE_ARG(aTags && aTags->Length() > 0);
  NS_ENSURE_ARG(aCallback);

  if (mShuttingDownOnGMPThread) {
    return NS_ERROR_FAILURE;
  }

  GetGMPAudioDecoderCallback* rawCallback = aCallback.release();
  RefPtr<AbstractThread> thread(GetAbstractGMPThread());
  RefPtr<GMPCrashHelper> helper(aHelper);
  GetContentParent(aHelper, aNodeId, NS_LITERAL_CSTRING(GMP_API_AUDIO_DECODER), *aTags)
    ->Then(thread, __func__,
      [rawCallback, helper](RefPtr<GMPContentParent> parent) {
        UniquePtr<GetGMPAudioDecoderCallback> callback(rawCallback);
        GMPAudioDecoderParent* actor = nullptr;
        if (parent && NS_SUCCEEDED(parent->GetGMPAudioDecoder(&actor))) {
          actor->SetCrashHelper(helper);
        }
        callback->Done(actor);
      },
      [rawCallback] {
        UniquePtr<GetGMPAudioDecoderCallback> callback(rawCallback);
        callback->Done(nullptr);
      });

  return NS_OK;
}

NS_IMETHODIMP
GeckoMediaPluginService::GetDecryptingGMPVideoDecoder(GMPCrashHelper* aHelper,
                                                      nsTArray<nsCString>* aTags,
                                                      const nsACString& aNodeId,
                                                      UniquePtr<GetGMPVideoDecoderCallback>&& aCallback,
                                                      uint32_t aDecryptorId)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
  NS_ENSURE_ARG(aTags && aTags->Length() > 0);
  NS_ENSURE_ARG(aCallback);

  if (mShuttingDownOnGMPThread) {
    return NS_ERROR_FAILURE;
  }

  GetGMPVideoDecoderCallback* rawCallback = aCallback.release();
  RefPtr<AbstractThread> thread(GetAbstractGMPThread());
  RefPtr<GMPCrashHelper> helper(aHelper);
  GetContentParent(aHelper, aNodeId, NS_LITERAL_CSTRING(GMP_API_VIDEO_DECODER), *aTags)
    ->Then(thread, __func__,
      [rawCallback, helper, aDecryptorId](RefPtr<GMPContentParent> parent) {
        UniquePtr<GetGMPVideoDecoderCallback> callback(rawCallback);
        GMPVideoDecoderParent* actor = nullptr;
        GMPVideoHostImpl* host = nullptr;
        if (parent && NS_SUCCEEDED(parent->GetGMPVideoDecoder(&actor, aDecryptorId))) {
          host = &(actor->Host());
          actor->SetCrashHelper(helper);
        }
        callback->Done(actor, host);
      },
      [rawCallback] {
        UniquePtr<GetGMPVideoDecoderCallback> callback(rawCallback);
        callback->Done(nullptr, nullptr);
      });

  return NS_OK;
}

NS_IMETHODIMP
GeckoMediaPluginService::GetGMPVideoEncoder(GMPCrashHelper* aHelper,
                                            nsTArray<nsCString>* aTags,
                                            const nsACString& aNodeId,
                                            UniquePtr<GetGMPVideoEncoderCallback>&& aCallback)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
  NS_ENSURE_ARG(aTags && aTags->Length() > 0);
  NS_ENSURE_ARG(aCallback);

  if (mShuttingDownOnGMPThread) {
    return NS_ERROR_FAILURE;
  }

  GetGMPVideoEncoderCallback* rawCallback = aCallback.release();
  RefPtr<AbstractThread> thread(GetAbstractGMPThread());
  RefPtr<GMPCrashHelper> helper(aHelper);
  GetContentParent(aHelper, aNodeId, NS_LITERAL_CSTRING(GMP_API_VIDEO_ENCODER), *aTags)
    ->Then(thread, __func__,
      [rawCallback, helper](RefPtr<GMPContentParent> parent) {
        UniquePtr<GetGMPVideoEncoderCallback> callback(rawCallback);
        GMPVideoEncoderParent* actor = nullptr;
        GMPVideoHostImpl* host = nullptr;
        if (parent && NS_SUCCEEDED(parent->GetGMPVideoEncoder(&actor))) {
          host = &(actor->Host());
          actor->SetCrashHelper(helper);
        }
        callback->Done(actor, host);
      },
      [rawCallback] {
        UniquePtr<GetGMPVideoEncoderCallback> callback(rawCallback);
        callback->Done(nullptr, nullptr);
      });

  return NS_OK;
}

NS_IMETHODIMP
GeckoMediaPluginService::GetGMPDecryptor(GMPCrashHelper* aHelper,
                                         nsTArray<nsCString>* aTags,
                                         const nsACString& aNodeId,
                                         UniquePtr<GetGMPDecryptorCallback>&& aCallback)
{
#if defined(XP_LINUX) && defined(MOZ_GMP_SANDBOX)
  if (!SandboxInfo::Get().CanSandboxMedia()) {
    NS_WARNING("GeckoMediaPluginService::GetGMPDecryptor: "
               "EME decryption not available without sandboxing support.");
    return NS_ERROR_NOT_AVAILABLE;
  }
#endif

  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
  NS_ENSURE_ARG(aTags && aTags->Length() > 0);
  NS_ENSURE_ARG(aCallback);

  if (mShuttingDownOnGMPThread) {
    return NS_ERROR_FAILURE;
  }

  GetGMPDecryptorCallback* rawCallback = aCallback.release();
  RefPtr<AbstractThread> thread(GetAbstractGMPThread());
  RefPtr<GMPCrashHelper> helper(aHelper);
  GetContentParent(aHelper, aNodeId, NS_LITERAL_CSTRING(GMP_API_DECRYPTOR), *aTags)
    ->Then(thread, __func__,
      [rawCallback, helper](RefPtr<GMPContentParent> parent) {
        UniquePtr<GetGMPDecryptorCallback> callback(rawCallback);
        GMPDecryptorParent* actor = nullptr;
        if (parent && NS_SUCCEEDED(parent->GetGMPDecryptor(&actor))) {
          actor->SetCrashHelper(helper);
        }
        callback->Done(actor);
      },
      [rawCallback] {
        UniquePtr<GetGMPDecryptorCallback> callback(rawCallback);
        callback->Done(nullptr);
      });

  return NS_OK;
}

void
GeckoMediaPluginService::ConnectCrashHelper(uint32_t aPluginId, GMPCrashHelper* aHelper)
{
  if (!aHelper) {
    return;
  }
  MutexAutoLock lock(mMutex);
  nsTArray<RefPtr<GMPCrashHelper>>* helpers;
  if (!mPluginCrashHelpers.Get(aPluginId, &helpers)) {
    helpers = new nsTArray<RefPtr<GMPCrashHelper>>();
    mPluginCrashHelpers.Put(aPluginId, helpers);
  } else if (helpers->Contains(aHelper)) {
    return;
  }
  helpers->AppendElement(aHelper);
}

void GeckoMediaPluginService::DisconnectCrashHelper(GMPCrashHelper* aHelper)
{
  if (!aHelper) {
    return;
  }
  MutexAutoLock lock(mMutex);
  for (auto iter = mPluginCrashHelpers.Iter(); !iter.Done(); iter.Next()) {
    nsTArray<RefPtr<GMPCrashHelper>>* helpers = iter.Data();
    if (!helpers->Contains(aHelper)) {
      continue;
    }
    helpers->RemoveElement(aHelper);
    MOZ_ASSERT(!helpers->Contains(aHelper)); // Ensure there aren't duplicates.
    if (helpers->IsEmpty()) {
      iter.Remove();
    }
  }
}

} // namespace gmp
} // namespace mozilla

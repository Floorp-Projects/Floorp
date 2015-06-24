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
#include "mozilla/Preferences.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/SyncRunnable.h"
#include "nsXPCOMPrivate.h"
#include "mozilla/Services.h"
#include "nsNativeCharsetUtils.h"
#include "nsIConsoleService.h"
#include "mozilla/unused.h"
#include "GMPDecryptorParent.h"
#include "GMPAudioDecoderParent.h"
#include "nsComponentManagerUtils.h"
#include "mozilla/Preferences.h"
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

#include "mozilla/dom/PluginCrashedEvent.h"
#include "mozilla/EventDispatcher.h"

namespace mozilla {

#ifdef LOG
#undef LOG
#endif

PRLogModuleInfo*
GetGMPLog()
{
  static PRLogModuleInfo *sLog;
  if (!sLog)
    sLog = PR_NewLogModule("GMP");
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

class GMPServiceCreateHelper final : public nsRunnable
{
  nsRefPtr<GeckoMediaPluginService> mService;

public:
  static already_AddRefed<GeckoMediaPluginService>
  GetOrCreate()
  {
    nsRefPtr<GeckoMediaPluginService> service;

    if (NS_IsMainThread()) {
      service = GetOrCreateOnMainThread();
    } else {
      nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
      MOZ_ASSERT(mainThread);

      nsRefPtr<GMPServiceCreateHelper> createHelper = new GMPServiceCreateHelper();

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
        nsRefPtr<GeckoMediaPluginServiceParent> service =
          new GeckoMediaPluginServiceParent();
        service->Init();
        sSingletonService = service;
      } else {
        nsRefPtr<GeckoMediaPluginServiceChild> service =
          new GeckoMediaPluginServiceChild();
        service->Init();
        sSingletonService = service;
      }

      ClearOnShutdown(&sSingletonService);
    }

    nsRefPtr<GeckoMediaPluginService> service = sSingletonService.get();
    return service.forget();
  }

  NS_IMETHOD
  Run()
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

void
GeckoMediaPluginService::RemoveObsoletePluginCrashCallbacks()
{
  MOZ_ASSERT(NS_IsMainThread());
  for (size_t i = mPluginCrashCallbacks.Length(); i != 0; --i) {
    nsRefPtr<GMPCrashCallback>& callback = mPluginCrashCallbacks[i - 1];
    if (!callback->IsStillValid()) {
      LOGD(("%s::%s - Removing obsolete callback for pluginId %i",
            __CLASS__, __FUNCTION__, callback->GetPluginId()));
      mPluginCrashCallbacks.RemoveElementAt(i - 1);
    }
  }
}

GeckoMediaPluginService::GMPCrashCallback::GMPCrashCallback(const uint32_t aPluginId,
                                                            nsPIDOMWindow* aParentWindow,
                                                            nsIDocument* aDocument)
  : mPluginId(aPluginId)
  , mParentWindowWeakPtr(do_GetWeakReference(aParentWindow))
  , mDocumentWeakPtr(do_GetWeakReference(aDocument))
{
  MOZ_ASSERT(NS_IsMainThread());
}

void
GeckoMediaPluginService::GMPCrashCallback::Run(const nsACString& aPluginName)
{
  dom::PluginCrashedEventInit init;
  init.mPluginID = mPluginId;
  init.mBubbles = true;
  init.mCancelable = true;
  init.mGmpPlugin = true;
  CopyUTF8toUTF16(aPluginName, init.mPluginName);
  init.mSubmittedCrashReport = false;

  // The following PluginCrashedEvent fields stay empty:
  // init.mBrowserDumpID
  // init.mPluginFilename
  // TODO: Can/should we fill them?

  nsCOMPtr<nsPIDOMWindow> parentWindow;
  nsCOMPtr<nsIDocument> document;
  if (!GetParentWindowAndDocumentIfValid(parentWindow, document)) {
    return;
  }

  nsRefPtr<dom::PluginCrashedEvent> event =
    dom::PluginCrashedEvent::Constructor(document, NS_LITERAL_STRING("PluginCrashed"), init);
  event->SetTrusted(true);
  event->GetInternalNSEvent()->mFlags.mOnlyChromeDispatch = true;

  EventDispatcher::DispatchDOMEvent(parentWindow, nullptr, event, nullptr, nullptr);
}

bool
GeckoMediaPluginService::GMPCrashCallback::IsStillValid()
{
  nsCOMPtr<nsPIDOMWindow> parentWindow;
  nsCOMPtr<nsIDocument> document;
  return GetParentWindowAndDocumentIfValid(parentWindow, document);
}

bool
GeckoMediaPluginService::GMPCrashCallback::GetParentWindowAndDocumentIfValid(
  nsCOMPtr<nsPIDOMWindow>& parentWindow,
  nsCOMPtr<nsIDocument>& document)
{
  parentWindow = do_QueryReferent(mParentWindowWeakPtr);
  if (!parentWindow) {
    return false;
  }
  document = do_QueryReferent(mDocumentWeakPtr);
  if (!document) {
    return false;
  }
  nsCOMPtr<nsIDocument> parentWindowDocument = parentWindow->GetExtantDoc();
  if (!parentWindowDocument || document.get() != parentWindowDocument.get()) {
    return false;
  }
  return true;
}

void
GeckoMediaPluginService::AddPluginCrashedEventTarget(const uint32_t aPluginId,
                                                     nsPIDOMWindow* aParentWindow)
{
  LOGD(("%s::%s(%i)", __CLASS__, __FUNCTION__, aPluginId));

  if (NS_WARN_IF(!aParentWindow)) {
    return;
  }
  nsCOMPtr<nsIDocument> doc = aParentWindow->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    return;
  }
  nsRefPtr<GMPCrashCallback> callback(new GMPCrashCallback(aPluginId, aParentWindow, doc));
  RemoveObsoletePluginCrashCallbacks();

  // If the plugin with that ID has already crashed without being handled,
  // just run the handler now.
  for (size_t i = mPluginCrashes.Length(); i != 0; --i) {
    size_t index = i - 1;
    const PluginCrash& crash = mPluginCrashes[index];
    if (crash.mPluginId == aPluginId) {
      LOGD(("%s::%s(%i) - added crash handler for crashed plugin, running handler #%u",
        __CLASS__, __FUNCTION__, aPluginId, index));
      callback->Run(crash.mPluginName);
      mPluginCrashes.RemoveElementAt(index);
    }
  }

  // Remember crash, so if a handler is added for it later, we report the
  // crash to that window too.
  mPluginCrashCallbacks.AppendElement(callback);
}

void
GeckoMediaPluginService::RunPluginCrashCallbacks(const uint32_t aPluginId,
                                                 const nsACString& aPluginName)
{
  MOZ_ASSERT(NS_IsMainThread());
  LOGD(("%s::%s(%i)", __CLASS__, __FUNCTION__, aPluginId));
  RemoveObsoletePluginCrashCallbacks();

  for (size_t i = mPluginCrashCallbacks.Length(); i != 0; --i) {
    nsRefPtr<GMPCrashCallback>& callback = mPluginCrashCallbacks[i - 1];
    if (callback->GetPluginId() == aPluginId) {
      LOGD(("%s::%s(%i) - Running #%u",
          __CLASS__, __FUNCTION__, aPluginId, i - 1));
      callback->Run(aPluginName);
      mPluginCrashCallbacks.RemoveElementAt(i - 1);
    }
  }
  mPluginCrashes.AppendElement(PluginCrash(aPluginId, aPluginName));
  if (mPluginCrashes.Length() > MAX_PLUGIN_CRASHES) {
    mPluginCrashes.RemoveElementAt(0);
  }
}

nsresult
GeckoMediaPluginService::Init()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obsService = mozilla::services::GetObserverService();
  MOZ_ASSERT(obsService);
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(obsService->AddObserver(this, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID, false)));

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
  }

  if (gmpThread) {
    gmpThread->Shutdown();
  }
}

nsresult
GeckoMediaPluginService::GMPDispatch(nsIRunnable* event, uint32_t flags)
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

    // Tell the thread to initialize plugins
    InitializePlugins();
  }

  nsCOMPtr<nsIThread> copy = mGMPThread;
  copy.forget(aThread);

  return NS_OK;
}

class GetGMPContentParentForAudioDecoderDone : public GetGMPContentParentCallback
{
public:
  explicit GetGMPContentParentForAudioDecoderDone(UniquePtr<GetGMPAudioDecoderCallback>&& aCallback)
   : mCallback(Move(aCallback))
  {
  }

  virtual void Done(GMPContentParent* aGMPParent) override
  {
    GMPAudioDecoderParent* gmpADP = nullptr;
    if (aGMPParent) {
      aGMPParent->GetGMPAudioDecoder(&gmpADP);
    }
    mCallback->Done(gmpADP);
  }

private:
  UniquePtr<GetGMPAudioDecoderCallback> mCallback;
};

NS_IMETHODIMP
GeckoMediaPluginService::GetGMPAudioDecoder(nsTArray<nsCString>* aTags,
                                            const nsACString& aNodeId,
                                            UniquePtr<GetGMPAudioDecoderCallback>&& aCallback)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
  NS_ENSURE_ARG(aTags && aTags->Length() > 0);
  NS_ENSURE_ARG(aCallback);

  if (mShuttingDownOnGMPThread) {
    return NS_ERROR_FAILURE;
  }

  UniquePtr<GetGMPContentParentCallback> callback(
    new GetGMPContentParentForAudioDecoderDone(Move(aCallback)));
  if (!GetContentParentFrom(aNodeId, NS_LITERAL_CSTRING(GMP_API_AUDIO_DECODER),
                            *aTags, Move(callback))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

class GetGMPContentParentForVideoDecoderDone : public GetGMPContentParentCallback
{
public:
  explicit GetGMPContentParentForVideoDecoderDone(UniquePtr<GetGMPVideoDecoderCallback>&& aCallback)
   : mCallback(Move(aCallback))
  {
  }

  virtual void Done(GMPContentParent* aGMPParent) override
  {
    GMPVideoDecoderParent* gmpVDP = nullptr;
    GMPVideoHostImpl* videoHost = nullptr;
    if (aGMPParent && NS_SUCCEEDED(aGMPParent->GetGMPVideoDecoder(&gmpVDP))) {
      videoHost = &gmpVDP->Host();
    }
    mCallback->Done(gmpVDP, videoHost);
  }

private:
  UniquePtr<GetGMPVideoDecoderCallback> mCallback;
};

NS_IMETHODIMP
GeckoMediaPluginService::GetGMPVideoDecoder(nsTArray<nsCString>* aTags,
                                            const nsACString& aNodeId,
                                            UniquePtr<GetGMPVideoDecoderCallback>&& aCallback)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
  NS_ENSURE_ARG(aTags && aTags->Length() > 0);
  NS_ENSURE_ARG(aCallback);

  if (mShuttingDownOnGMPThread) {
    return NS_ERROR_FAILURE;
  }

  UniquePtr<GetGMPContentParentCallback> callback(
    new GetGMPContentParentForVideoDecoderDone(Move(aCallback)));
  if (!GetContentParentFrom(aNodeId, NS_LITERAL_CSTRING(GMP_API_VIDEO_DECODER),
                            *aTags, Move(callback))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

class GetGMPContentParentForVideoEncoderDone : public GetGMPContentParentCallback
{
public:
  explicit GetGMPContentParentForVideoEncoderDone(UniquePtr<GetGMPVideoEncoderCallback>&& aCallback)
   : mCallback(Move(aCallback))
  {
  }

  virtual void Done(GMPContentParent* aGMPParent) override
  {
    GMPVideoEncoderParent* gmpVEP = nullptr;
    GMPVideoHostImpl* videoHost = nullptr;
    if (aGMPParent && NS_SUCCEEDED(aGMPParent->GetGMPVideoEncoder(&gmpVEP))) {
      videoHost = &gmpVEP->Host();
    }
    mCallback->Done(gmpVEP, videoHost);
  }

private:
  UniquePtr<GetGMPVideoEncoderCallback> mCallback;
};

NS_IMETHODIMP
GeckoMediaPluginService::GetGMPVideoEncoder(nsTArray<nsCString>* aTags,
                                            const nsACString& aNodeId,
                                            UniquePtr<GetGMPVideoEncoderCallback>&& aCallback)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mGMPThread);
  NS_ENSURE_ARG(aTags && aTags->Length() > 0);
  NS_ENSURE_ARG(aCallback);

  if (mShuttingDownOnGMPThread) {
    return NS_ERROR_FAILURE;
  }

  UniquePtr<GetGMPContentParentCallback> callback(
    new GetGMPContentParentForVideoEncoderDone(Move(aCallback)));
  if (!GetContentParentFrom(aNodeId, NS_LITERAL_CSTRING(GMP_API_VIDEO_ENCODER),
                            *aTags, Move(callback))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

class GetGMPContentParentForDecryptorDone : public GetGMPContentParentCallback
{
public:
  explicit GetGMPContentParentForDecryptorDone(UniquePtr<GetGMPDecryptorCallback>&& aCallback)
   : mCallback(Move(aCallback))
  {
  }

  virtual void Done(GMPContentParent* aGMPParent) override
  {
    GMPDecryptorParent* ksp = nullptr;
    if (aGMPParent) {
      aGMPParent->GetGMPDecryptor(&ksp);
    }
    mCallback->Done(ksp);
  }

private:
  UniquePtr<GetGMPDecryptorCallback> mCallback;
};

NS_IMETHODIMP
GeckoMediaPluginService::GetGMPDecryptor(nsTArray<nsCString>* aTags,
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

  UniquePtr<GetGMPContentParentCallback> callback(
    new GetGMPContentParentForDecryptorDone(Move(aCallback)));
  if (!GetContentParentFrom(aNodeId, NS_LITERAL_CSTRING(GMP_API_DECRYPTOR),
                            *aTags, Move(callback))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
GeckoMediaPluginService::HasPluginForAPI(const nsACString& aAPI,
                                         nsTArray<nsCString>* aTags,
                                         bool* aOutHavePlugin)
{
  nsCString unused;
  return GetPluginVersionForAPI(aAPI, aTags, aOutHavePlugin, unused);
}

} // namespace gmp
} // namespace mozilla

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_workers_h__
#define mozilla_dom_workers_workers_h__

#include "jsapi.h"
#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"
#include <stdint.h>
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsString.h"
#include "nsTArray.h"

#include "nsILoadContext.h"
#include "nsIWeakReferenceUtils.h"
#include "nsIInterfaceRequestor.h"
#include "mozilla/dom/ChannelInfo.h"
#include "mozilla/dom/ServiceWorkerDescriptor.h"
#include "mozilla/net/ReferrerPolicy.h"

#define BEGIN_WORKERS_NAMESPACE \
  namespace mozilla { namespace dom { namespace workers {
#define END_WORKERS_NAMESPACE \
  } /* namespace workers */ } /* namespace dom */ } /* namespace mozilla */
#define USING_WORKERS_NAMESPACE \
  using namespace mozilla::dom::workers;

#define WORKERS_SHUTDOWN_TOPIC "web-workers-shutdown"

class nsIContentSecurityPolicy;
class nsIScriptContext;
class nsIGlobalObject;
class nsPIDOMWindowInner;
class nsIPrincipal;
class nsILoadGroup;
class nsITabChild;
class nsIChannel;
class nsIRunnable;
class nsIURI;

namespace mozilla {
namespace ipc {
class PrincipalInfo;
} // namespace ipc

namespace dom {
// If you change this, the corresponding list in nsIWorkerDebugger.idl needs to
// be updated too.
enum WorkerType
{
  WorkerTypeDedicated,
  WorkerTypeShared,
  WorkerTypeService
};

} // namespace dom
} // namespace mozilla

BEGIN_WORKERS_NAMESPACE

class WorkerPrivate;

struct PrivatizableBase
{ };

#ifdef DEBUG
void
AssertIsOnMainThread();
#else
inline void
AssertIsOnMainThread()
{ }
#endif

struct JSSettings
{
  enum {
    // All the GC parameters that we support.
    JSSettings_JSGC_MAX_BYTES = 0,
    JSSettings_JSGC_MAX_MALLOC_BYTES,
    JSSettings_JSGC_HIGH_FREQUENCY_TIME_LIMIT,
    JSSettings_JSGC_LOW_FREQUENCY_HEAP_GROWTH,
    JSSettings_JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MIN,
    JSSettings_JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MAX,
    JSSettings_JSGC_HIGH_FREQUENCY_LOW_LIMIT,
    JSSettings_JSGC_HIGH_FREQUENCY_HIGH_LIMIT,
    JSSettings_JSGC_ALLOCATION_THRESHOLD,
    JSSettings_JSGC_SLICE_TIME_BUDGET,
    JSSettings_JSGC_DYNAMIC_HEAP_GROWTH,
    JSSettings_JSGC_DYNAMIC_MARK_SLICE,
    // JSGC_MODE not supported

    // This must be last so that we get an accurate count.
    kGCSettingsArraySize
  };

  struct JSGCSetting
  {
    mozilla::Maybe<JSGCParamKey> key;
    uint32_t value;

    JSGCSetting()
    : key(), value(0)
    { }
  };

  // There are several settings that we know we need so it makes sense to
  // preallocate here.
  typedef JSGCSetting JSGCSettingsArray[kGCSettingsArraySize];

  // Settings that change based on chrome/content context.
  struct JSContentChromeSettings
  {
    JS::CompartmentOptions compartmentOptions;
    int32_t maxScriptRuntime;

    JSContentChromeSettings()
    : compartmentOptions(), maxScriptRuntime(0)
    { }
  };

  JSContentChromeSettings chrome;
  JSContentChromeSettings content;
  JSGCSettingsArray gcSettings;
  JS::ContextOptions contextOptions;

#ifdef JS_GC_ZEAL
  uint8_t gcZeal;
  uint32_t gcZealFrequency;
#endif

  JSSettings()
#ifdef JS_GC_ZEAL
  : gcZeal(0), gcZealFrequency(0)
#endif
  {
    for (uint32_t index = 0; index < ArrayLength(gcSettings); index++) {
      new (gcSettings + index) JSGCSetting();
    }
  }

  bool
  ApplyGCSetting(JSGCParamKey aKey, uint32_t aValue)
  {
    JSSettings::JSGCSetting* firstEmptySetting = nullptr;
    JSSettings::JSGCSetting* foundSetting = nullptr;

    for (uint32_t index = 0; index < ArrayLength(gcSettings); index++) {
      JSSettings::JSGCSetting& setting = gcSettings[index];
      if (setting.key.isSome() && *setting.key == aKey) {
        foundSetting = &setting;
        break;
      }
      if (!firstEmptySetting && setting.key.isNothing()) {
        firstEmptySetting = &setting;
      }
    }

    if (aValue) {
      if (!foundSetting) {
        foundSetting = firstEmptySetting;
        if (!foundSetting) {
          NS_ERROR("Not enough space for this value!");
          return false;
        }
      }
      foundSetting->key = mozilla::Some(aKey);
      foundSetting->value = aValue;
      return true;
    }

    if (foundSetting) {
      foundSetting->key.reset();
      return true;
    }

    return false;
  }
};

// Implemented in WorkerPrivate.cpp

struct WorkerLoadInfo
{
  // All of these should be released in WorkerPrivateParent::ForgetMainThreadObjects.
  nsCOMPtr<nsIURI> mBaseURI;
  nsCOMPtr<nsIURI> mResolvedScriptURI;

  // This is the principal of the global (parent worker or a window) loading
  // the worker. It can be null if we are executing a ServiceWorker, otherwise,
  // except for data: URL, it must subsumes the worker principal.
  // If we load a data: URL, mPrincipal will be a null principal.
  nsCOMPtr<nsIPrincipal> mLoadingPrincipal;
  nsCOMPtr<nsIPrincipal> mPrincipal;

  nsCOMPtr<nsIScriptContext> mScriptContext;
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  nsCOMPtr<nsIContentSecurityPolicy> mCSP;
  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsILoadGroup> mLoadGroup;

  // mLoadFailedAsyncRunnable will execute on main thread if script loading
  // fails during script loading.  If script loading is never started due to
  // a synchronous error, then the runnable is never executed.  The runnable
  // is guaranteed to be released on the main thread.
  nsCOMPtr<nsIRunnable> mLoadFailedAsyncRunnable;

  class InterfaceRequestor final : public nsIInterfaceRequestor
  {
    NS_DECL_ISUPPORTS

  public:
    InterfaceRequestor(nsIPrincipal* aPrincipal, nsILoadGroup* aLoadGroup);
    void MaybeAddTabChild(nsILoadGroup* aLoadGroup);
    NS_IMETHOD GetInterface(const nsIID& aIID, void** aSink) override;

  private:
    ~InterfaceRequestor() { }

    already_AddRefed<nsITabChild> GetAnyLiveTabChild();

    nsCOMPtr<nsILoadContext> mLoadContext;
    nsCOMPtr<nsIInterfaceRequestor> mOuterRequestor;

    // Array of weak references to nsITabChild.  We do not want to keep TabChild
    // actors alive for long after their ActorDestroy() methods are called.
    nsTArray<nsWeakPtr> mTabChildList;
  };

  // Only set if we have a custom overriden load group
  RefPtr<InterfaceRequestor> mInterfaceRequestor;

  nsAutoPtr<mozilla::ipc::PrincipalInfo> mPrincipalInfo;
  nsCString mDomain;
  nsString mOrigin; // Derived from mPrincipal; can be used on worker thread.

  nsString mServiceWorkerCacheName;
  Maybe<ServiceWorkerDescriptor> mServiceWorkerDescriptor;

  Maybe<ServiceWorkerDescriptor> mParentController;

  ChannelInfo mChannelInfo;
  nsLoadFlags mLoadFlags;

  uint64_t mWindowID;

  net::ReferrerPolicy mReferrerPolicy;
  bool mFromWindow;
  bool mEvalAllowed;
  bool mReportCSPViolations;
  bool mXHRParamsAllowed;
  bool mPrincipalIsSystem;
  bool mStorageAllowed;
  bool mServiceWorkersTestingInWindow;
  OriginAttributes mOriginAttributes;

  WorkerLoadInfo();
  ~WorkerLoadInfo();

  void StealFrom(WorkerLoadInfo& aOther);

  nsresult
  SetPrincipalOnMainThread(nsIPrincipal* aPrincipal, nsILoadGroup* aLoadGroup);

  nsresult
  GetPrincipalAndLoadGroupFromChannel(nsIChannel* aChannel,
                                      nsIPrincipal** aPrincipalOut,
                                      nsILoadGroup** aLoadGroupOut);

  nsresult
  SetPrincipalFromChannel(nsIChannel* aChannel);

  bool
  FinalChannelPrincipalIsValid(nsIChannel* aChannel);

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  bool
  PrincipalIsValid() const;

  bool
  PrincipalURIMatchesScriptURL();
#endif

  bool
  ProxyReleaseMainThreadObjects(WorkerPrivate* aWorkerPrivate);

  bool
  ProxyReleaseMainThreadObjects(WorkerPrivate* aWorkerPrivate,
                                nsCOMPtr<nsILoadGroup>& aLoadGroupToCancel);
};

// All of these are implemented in RuntimeService.cpp

void
CancelWorkersForWindow(nsPIDOMWindowInner* aWindow);

void
FreezeWorkersForWindow(nsPIDOMWindowInner* aWindow);

void
ThawWorkersForWindow(nsPIDOMWindowInner* aWindow);

void
SuspendWorkersForWindow(nsPIDOMWindowInner* aWindow);

void
ResumeWorkersForWindow(nsPIDOMWindowInner* aWindow);

// A class that can be used with WorkerCrossThreadDispatcher to run a
// bit of C++ code on the worker thread.
class WorkerTask
{
protected:
  WorkerTask()
  { }

  virtual ~WorkerTask()
  { }

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WorkerTask)

  // The return value here has the same semantics as the return value
  // of WorkerRunnable::WorkerRun.
  virtual bool
  RunTask(JSContext* aCx) = 0;
};

class WorkerCrossThreadDispatcher
{
   friend class WorkerPrivate;

  // Must be acquired *before* the WorkerPrivate's mutex, when they're both
  // held.
  Mutex mMutex;
  WorkerPrivate* mWorkerPrivate;

private:
  // Only created by WorkerPrivate.
  explicit WorkerCrossThreadDispatcher(WorkerPrivate* aWorkerPrivate);

  // Only called by WorkerPrivate.
  void
  Forget()
  {
    MutexAutoLock lock(mMutex);
    mWorkerPrivate = nullptr;
  }

  ~WorkerCrossThreadDispatcher() {}

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WorkerCrossThreadDispatcher)

  // Generically useful function for running a bit of C++ code on the worker
  // thread.
  bool
  PostTask(WorkerTask* aTask);
};

// Random unique constant to facilitate JSPrincipal debugging
const uint32_t kJSPrincipalsDebugToken = 0x7e2df9d2;

bool
IsWorkerGlobal(JSObject* global);

bool
IsDebuggerGlobal(JSObject* global);

bool
IsDebuggerSandbox(JSObject* object);

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_workers_h__

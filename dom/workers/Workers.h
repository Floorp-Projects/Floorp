/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_workers_h__
#define mozilla_dom_workers_workers_h__

#include "jsapi.h"
#include "mozilla/Attributes.h"
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
class nsPIDOMWindow;
class nsIPrincipal;
class nsILoadGroup;
class nsITabChild;
class nsIChannel;
class nsIURI;

namespace mozilla {
namespace ipc {
class PrincipalInfo;
}

namespace dom {
// If you change this, the corresponding list in nsIWorkerDebugger.idl needs to
// be updated too.
enum WorkerType
{
  WorkerTypeDedicated,
  WorkerTypeShared,
  WorkerTypeService
};
}
}

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
    JSGCParamKey key;
    uint32_t value;

    JSGCSetting()
    : key(static_cast<JSGCParamKey>(-1)), value(0)
    { }

    bool
    IsSet() const
    {
      return key != static_cast<JSGCParamKey>(-1);
    }

    void
    Unset()
    {
      key = static_cast<JSGCParamKey>(-1);
      value = 0;
    }
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
  JS::RuntimeOptions runtimeOptions;

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
      if (setting.key == aKey) {
        foundSetting = &setting;
        break;
      }
      if (!firstEmptySetting && !setting.IsSet()) {
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
      foundSetting->key = aKey;
      foundSetting->value = aValue;
      return true;
    }

    if (foundSetting) {
      foundSetting->Unset();
      return true;
    }

    return false;
  }
};

enum WorkerPreference
{
  WORKERPREF_DUMP = 0, // browser.dom.window.dump.enabled
  WORKERPREF_DOM_CACHES, // dom.caches.enabled
  WORKERPREF_SERVICEWORKERS, // dom.serviceWorkers.enabled
  WORKERPREF_COUNT
};

// Implemented in WorkerPrivate.cpp

struct WorkerLoadInfo
{
  // All of these should be released in WorkerPrivateParent::ForgetMainThreadObjects.
  nsCOMPtr<nsIURI> mBaseURI;
  nsCOMPtr<nsIURI> mResolvedScriptURI;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsIScriptContext> mScriptContext;
  nsCOMPtr<nsPIDOMWindow> mWindow;
  nsCOMPtr<nsIContentSecurityPolicy> mCSP;
  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsILoadGroup> mLoadGroup;

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
  nsRefPtr<InterfaceRequestor> mInterfaceRequestor;

  nsAutoPtr<mozilla::ipc::PrincipalInfo> mPrincipalInfo;
  nsCString mDomain;

  nsString mServiceWorkerCacheName;

  nsCString mSecurityInfo;

  uint64_t mWindowID;

  bool mFromWindow;
  bool mEvalAllowed;
  bool mReportCSPViolations;
  bool mXHRParamsAllowed;
  bool mPrincipalIsSystem;
  bool mIsInPrivilegedApp;
  bool mIsInCertifiedApp;
  bool mIndexedDBAllowed;

  WorkerLoadInfo();
  ~WorkerLoadInfo();

  void StealFrom(WorkerLoadInfo& aOther);
};

// All of these are implemented in RuntimeService.cpp

void
CancelWorkersForWindow(nsPIDOMWindow* aWindow);

void
FreezeWorkersForWindow(nsPIDOMWindow* aWindow);

void
ThawWorkersForWindow(nsPIDOMWindow* aWindow);

class WorkerTask
{
protected:
  WorkerTask()
  { }

  virtual ~WorkerTask()
  { }

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WorkerTask)

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

WorkerCrossThreadDispatcher*
GetWorkerCrossThreadDispatcher(JSContext* aCx, jsval aWorker);

// Random unique constant to facilitate JSPrincipal debugging
const uint32_t kJSPrincipalsDebugToken = 0x7e2df9d2;

namespace exceptions {

// Implemented in Exceptions.cpp
void
ThrowDOMExceptionForNSResult(JSContext* aCx, nsresult aNSResult);

} // namespace exceptions

nsIGlobalObject*
GetGlobalObjectForGlobal(JSObject* global);

bool
IsWorkerGlobal(JSObject* global);

bool
IsDebuggerGlobal(JSObject* global);

bool
IsDebuggerSandbox(JSObject* object);

// Throws the JSMSG_GETTER_ONLY exception.  This shouldn't be used going
// forward -- getter-only properties should just use JS_PSG for the setter
// (implying no setter at all), which will not throw when set in non-strict
// code but will in strict code.  Old code should use this only for temporary
// compatibility reasons.
extern bool
GetterOnlyJSNative(JSContext* aCx, unsigned aArgc, JS::Value* aVp);

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_workers_h__

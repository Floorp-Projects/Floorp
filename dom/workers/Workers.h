/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
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

#define BEGIN_WORKERS_NAMESPACE \
  namespace mozilla { namespace dom { namespace workers {
#define END_WORKERS_NAMESPACE \
  } /* namespace workers */ } /* namespace dom */ } /* namespace mozilla */
#define USING_WORKERS_NAMESPACE \
  using namespace mozilla::dom::workers;

#define WORKERS_SHUTDOWN_TOPIC "web-workers-shutdown"

class nsIScriptContext;
class nsPIDOMWindow;

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
    JS::ContextOptions options;
    int32_t maxScriptRuntime;

    JSContentChromeSettings()
    : options(), maxScriptRuntime(0)
    { }
  };

  JSContentChromeSettings chrome;
  JSContentChromeSettings content;
  JSGCSettingsArray gcSettings;
  bool jitHardening;
#ifdef JS_GC_ZEAL
  uint8_t gcZeal;
  uint32_t gcZealFrequency;
#endif

  JSSettings()
  : jitHardening(false)
#ifdef JS_GC_ZEAL
  , gcZeal(0), gcZealFrequency(0)
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
  WORKERPREF_PROMISE,  // dom.promise.enabled
  WORKERPREF_COUNT
};

// All of these are implemented in RuntimeService.cpp
bool
ResolveWorkerClasses(JSContext* aCx, JS::Handle<JSObject*> aObj, JS::Handle<jsid> aId,
                     unsigned aFlags, JS::MutableHandle<JSObject*> aObjp);

void
CancelWorkersForWindow(nsPIDOMWindow* aWindow);

void
SuspendWorkersForWindow(nsPIDOMWindow* aWindow);

void
ResumeWorkersForWindow(nsPIDOMWindow* aWindow);

class WorkerTask {
public:
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WorkerTask)

    virtual ~WorkerTask() { }

    virtual bool RunTask(JSContext* aCx) = 0;
};

class WorkerCrossThreadDispatcher {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WorkerCrossThreadDispatcher)

  WorkerCrossThreadDispatcher(WorkerPrivate* aPrivate) :
    mMutex("WorkerCrossThreadDispatcher"), mPrivate(aPrivate) {}
  void Forget()
  {
    mozilla::MutexAutoLock lock(mMutex);
    mPrivate = nullptr;
  }

  /**
   * Generically useful function for running a bit of C++ code on the worker
   * thread.
   */
  bool PostTask(WorkerTask* aTask);

protected:
  friend class WorkerPrivate;

  // Must be acquired *before* the WorkerPrivate's mutex, when they're both held.
  mozilla::Mutex mMutex;
  WorkerPrivate* mPrivate;
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

// Throws the JSMSG_GETTER_ONLY exception.  This shouldn't be used going
// forward -- getter-only properties should just use JS_PSG for the setter
// (implying no setter at all), which will not throw when set in non-strict
// code but will in strict code.  Old code should use this only for temporary
// compatibility reasons.
extern bool
GetterOnlyJSNative(JSContext* aCx, unsigned aArgc, JS::Value* aVp);

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_workers_h__

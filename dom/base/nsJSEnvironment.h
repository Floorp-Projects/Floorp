/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsJSEnvironment_h
#define nsJSEnvironment_h

#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsCOMPtr.h"
#include "prtime.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIArray.h"
#include "mozilla/Attributes.h"
#include "mozilla/TimeStamp.h"
#include "nsThreadUtils.h"
#include "xpcpublic.h"

class nsICycleCollectorListener;
class nsIDocShell;

namespace mozilla {

template <class>
class Maybe;
struct CycleCollectorResults;

static const uint32_t kMajorForgetSkippableCalls = 5;

}  // namespace mozilla

class nsJSContext : public nsIScriptContext {
 public:
  nsJSContext(bool aGCOnDestruction, nsIScriptGlobalObject* aGlobalObject);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsJSContext,
                                                         nsIScriptContext)

  virtual nsIScriptGlobalObject* GetGlobalObject() override;
  inline nsIScriptGlobalObject* GetGlobalObjectRef() {
    return mGlobalObjectRef;
  }

  virtual nsresult SetProperty(JS::Handle<JSObject*> aTarget,
                               const char* aPropName,
                               nsISupports* aVal) override;

  virtual bool GetProcessingScriptTag() override;
  virtual void SetProcessingScriptTag(bool aResult) override;

  virtual nsresult InitClasses(JS::Handle<JSObject*> aGlobalObj) override;

  virtual void SetWindowProxy(JS::Handle<JSObject*> aWindowProxy) override;
  virtual JSObject* GetWindowProxy() override;

  enum IsShrinking { ShrinkingGC, NonShrinkingGC };

  enum IsIncremental { IncrementalGC, NonIncrementalGC };

  // Setup all the statics etc - safe to call multiple times after Startup().
  static void EnsureStatics();

  static void SetLowMemoryState(bool aState);

  static void GarbageCollectNow(JS::GCReason reason,
                                IsIncremental aIncremental = NonIncrementalGC,
                                IsShrinking aShrinking = NonShrinkingGC,
                                int64_t aSliceMillis = 0);

  static void CycleCollectNow(nsICycleCollectorListener* aListener = nullptr);

  // Run a cycle collector slice, using a heuristic to decide how long to run
  // it.
  static void RunCycleCollectorSlice(mozilla::TimeStamp aDeadline);

  // Run a cycle collector slice, using the given work budget.
  static void RunCycleCollectorWorkSlice(int64_t aWorkBudget);

  static void BeginCycleCollectionCallback();
  static void EndCycleCollectionCallback(
      mozilla::CycleCollectorResults& aResults);

  // Return the longest CC slice time since ClearMaxCCSliceTime() was last
  // called.
  static uint32_t GetMaxCCSliceTimeSinceClear();
  static void ClearMaxCCSliceTime();

  // If there is some pending CC or GC timer/runner, this will run it.
  static void RunNextCollectorTimer(
      JS::GCReason aReason,
      mozilla::TimeStamp aDeadline = mozilla::TimeStamp());
  // If user has been idle and aDocShell is for an iframe being loaded in an
  // already loaded top level docshell, this will run a CC or GC
  // timer/runner if there is such pending.
  static void MaybeRunNextCollectorSlice(nsIDocShell* aDocShell,
                                         JS::GCReason aReason);

  // The GC should probably run soon, in the zone of object aObj (if given).
  static void PokeGC(JS::GCReason aReason, JSObject* aObj, uint32_t aDelay = 0);
  static void KillGCTimer();

  static void PokeShrinkingGC();
  static void KillShrinkingGCTimer();

  static void MaybePokeCC();
  static void KillCCRunner();
  static void KillICCRunner();
  static void KillFullGCTimer();
  static void KillInterSliceGCRunner();

  // Calling LikelyShortLivingObjectCreated() makes a GC more likely.
  static void LikelyShortLivingObjectCreated();

  static uint32_t CleanupsSinceLastGC();

  nsIScriptGlobalObject* GetCachedGlobalObject() {
    // Verify that we have a global so that this
    // does always return a null when GetGlobalObject() is null.
    JSObject* global = GetWindowProxy();
    return global ? mGlobalObjectRef.get() : nullptr;
  }

 protected:
  virtual ~nsJSContext();

  // Helper to convert xpcom datatypes to jsvals.
  nsresult ConvertSupportsTojsvals(JSContext* aCx, nsISupports* aArgs,
                                   JS::Handle<JSObject*> aScope,
                                   JS::MutableHandleVector<JS::Value> aArgsOut);

  nsresult AddSupportsPrimitiveTojsvals(JSContext* aCx, nsISupports* aArg,
                                        JS::Value* aArgv);

 private:
  void Destroy();

  JS::Heap<JSObject*> mWindowProxy;

  bool mGCOnDestruction;
  bool mProcessingScriptTag;

  // mGlobalObjectRef ensures that the outer window stays alive as long as the
  // context does. It is eventually collected by the cycle collector.
  nsCOMPtr<nsIScriptGlobalObject> mGlobalObjectRef;

  static bool DOMOperationCallback(JSContext* cx);
};

namespace mozilla {
namespace dom {

class SerializedStackHolder;

void StartupJSEnvironment();
void ShutdownJSEnvironment();

// Runnable that's used to do async error reporting
class AsyncErrorReporter final : public mozilla::Runnable {
 public:
  explicit AsyncErrorReporter(xpc::ErrorReport* aReport);
  // SerializeStack is suitable for main or worklet thread use.
  // Stacks from worker threads are not supported.
  // See https://bugzilla.mozilla.org/show_bug.cgi?id=1578968
  void SerializeStack(JSContext* aCx, JS::Handle<JSObject*> aStack);

  // Set the exception value associated with this error report.
  // Should only be called from the main thread.
  void SetException(JSContext* aCx, JS::Handle<JS::Value> aException);

 protected:
  NS_IMETHOD Run() override;

  // This is only used on main thread!
  JS::PersistentRootedValue mException;
  bool mHasException = false;

  RefPtr<xpc::ErrorReport> mReport;
  // This may be used to marshal a stack from an arbitrary thread/runtime into
  // the main thread/runtime where the console service runs.
  UniquePtr<SerializedStackHolder> mStackHolder;
};

}  // namespace dom
}  // namespace mozilla

// An interface for fast and native conversion to/from nsIArray. If an object
// supports this interface, JS can reach directly in for the argv, and avoid
// nsISupports conversion. If this interface is not supported, the object will
// be queried for nsIArray, and everything converted via xpcom objects.
#define NS_IJSARGARRAY_IID                           \
  {                                                  \
    0xb6acdac8, 0xf5c6, 0x432c, {                    \
      0xa8, 0x6e, 0x33, 0xee, 0xb1, 0xb0, 0xcd, 0xdc \
    }                                                \
  }

class nsIJSArgArray : public nsIArray {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IJSARGARRAY_IID)
  // Bug 312003 describes why this must be "void **", but after calling argv
  // may be cast to JS::Value* and the args found at:
  //    ((JS::Value*)argv)[0], ..., ((JS::Value*)argv)[argc - 1]
  virtual nsresult GetArgs(uint32_t* argc, void** argv) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIJSArgArray, NS_IJSARGARRAY_IID)

#endif /* nsJSEnvironment_h */

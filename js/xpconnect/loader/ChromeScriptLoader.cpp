/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PrecompiledScript.h"

#include "nsIIncrementalStreamLoader.h"
#include "nsIURI.h"
#include "nsIChannel.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"

#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/CompileOptions.h"  // JS::CompileOptions, JS::OwningCompileOptions
#include "js/CompilationAndEvaluation.h"
#include "js/experimental/CompileScript.h"  // JS::CompileGlobalScriptToStencil, JS::NewFrontendContext, JS::DestroyFrontendContext, JS::SetNativeStackQuota, JS::ThreadStackQuotaForSize, JS::HadFrontendErrors, JS::ConvertFrontendErrorsToRuntimeErrors
#include "js/experimental/JSStencil.h"  // JS::Stencil, JS::CompileGlobalScriptToStencil, JS::InstantiateGlobalStencil, JS::CompilationStorage
#include "js/SourceText.h"              // JS::SourceText
#include "js/Utility.h"

#include "mozilla/AlreadyAddRefed.h"  // already_AddRefed
#include "mozilla/Assertions.h"       // MOZ_ASSERT
#include "mozilla/Attributes.h"
#include "mozilla/ClearOnShutdown.h"  // RunOnShutdown
#include "mozilla/EventQueue.h"       // EventQueuePriority
#include "mozilla/Mutex.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPrefs_javascript.h"
#include "mozilla/dom/ChromeUtils.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ScriptLoader.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/RefPtr.h"          // RefPtr
#include "mozilla/TaskController.h"  // TaskController, Task
#include "mozilla/ThreadSafety.h"    // MOZ_GUARDED_BY
#include "mozilla/Utf8.h"            // Utf8Unit
#include "mozilla/Vector.h"
#include "nsCCUncollectableMarker.h"
#include "nsCycleCollectionParticipant.h"

using namespace JS;
using namespace mozilla;
using namespace mozilla::dom;

class AsyncScriptCompileTask final : public Task {
  static mozilla::StaticMutex sOngoingTasksMutex;
  static Vector<AsyncScriptCompileTask*> sOngoingTasks
      MOZ_GUARDED_BY(sOngoingTasksMutex);
  static bool sIsShutdownRegistered;

  // Compilation tasks should be cancelled before calling JS_ShutDown, in order
  // to avoid keeping JS::Stencil and SharedImmutableString pointers alive
  // beyond it.
  //
  // Cancel all ongoing tasks at ShutdownPhase::XPCOMShutdownFinal, which
  // happens before calling JS_ShutDown.
  static bool RegisterTask(AsyncScriptCompileTask* aTask) {
    MOZ_ASSERT(NS_IsMainThread());

    if (!sIsShutdownRegistered) {
      sIsShutdownRegistered = true;

      RunOnShutdown([] {
        StaticMutexAutoLock lock(sOngoingTasksMutex);
        for (auto* task : sOngoingTasks) {
          task->Cancel();
        }
      });
    }

    StaticMutexAutoLock lock(sOngoingTasksMutex);
    return sOngoingTasks.append(aTask);
  }

  static void UnregisterTask(const AsyncScriptCompileTask* aTask) {
    StaticMutexAutoLock lock(sOngoingTasksMutex);
    sOngoingTasks.eraseIfEqual(aTask);
  }

 public:
  explicit AsyncScriptCompileTask(JS::SourceText<Utf8Unit>&& aSrcBuf)
      : Task(Kind::OffMainThreadOnly, EventQueuePriority::Normal),
        mOptions(JS::OwningCompileOptions::ForFrontendContext()),
        mSrcBuf(std::move(aSrcBuf)),
        mMutex("AsyncScriptCompileTask") {}

  ~AsyncScriptCompileTask() {
    if (mFrontendContext) {
      JS::DestroyFrontendContext(mFrontendContext);
    }
    UnregisterTask(this);
  }

  bool Init(const JS::OwningCompileOptions& aOptions) {
    if (!RegisterTask(this)) {
      return false;
    }

    mFrontendContext = JS::NewFrontendContext();
    if (!mFrontendContext) {
      return false;
    }

    if (!mOptions.copy(mFrontendContext, aOptions)) {
      return false;
    }

    return true;
  }

 private:
  void Compile() {
    // NOTE: The stack limit must be set from the same thread that compiles.
    size_t stackSize = TaskController::GetThreadStackSize();
    JS::SetNativeStackQuota(mFrontendContext,
                            JS::ThreadStackQuotaForSize(stackSize));

    JS::CompilationStorage compileStorage;
    mStencil = JS::CompileGlobalScriptToStencil(mFrontendContext, mOptions,
                                                mSrcBuf, compileStorage);
  }

  // Cancel the task.
  // If the task is already running, this waits for the task to finish.
  void Cancel() {
    MOZ_ASSERT(NS_IsMainThread());

    MutexAutoLock lock(mMutex);

    mIsCancelled = true;

    mStencil = nullptr;
  }

 public:
  TaskResult Run() override {
    MutexAutoLock lock(mMutex);

    if (mIsCancelled) {
      return TaskResult::Complete;
    }

    Compile();
    return TaskResult::Complete;
  }

  already_AddRefed<JS::Stencil> StealStencil(JSContext* aCx) {
    JS::FrontendContext* fc = mFrontendContext;
    mFrontendContext = nullptr;

    MOZ_ASSERT(fc);

    if (JS::HadFrontendErrors(fc)) {
      (void)JS::ConvertFrontendErrorsToRuntimeErrors(aCx, fc, mOptions);
      JS::DestroyFrontendContext(fc);
      return nullptr;
    }

    // Report warnings.
    if (!JS::ConvertFrontendErrorsToRuntimeErrors(aCx, fc, mOptions)) {
      JS::DestroyFrontendContext(fc);
      return nullptr;
    }

    JS::DestroyFrontendContext(fc);

    return mStencil.forget();
  }

#ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
  bool GetName(nsACString& aName) override {
    aName.AssignLiteral("AsyncScriptCompileTask");
    return true;
  }
#endif

 private:
  // Owning-pointer for the context associated with the script compilation.
  //
  // The context is allocated on main thread in Init method, and is freed on
  // any thread in the destructor.
  JS::FrontendContext* mFrontendContext = nullptr;

  JS::OwningCompileOptions mOptions;

  RefPtr<JS::Stencil> mStencil;

  JS::SourceText<Utf8Unit> mSrcBuf;

  // This mutex is locked during running the task or cancelling task.
  mozilla::Mutex mMutex;

  bool mIsCancelled MOZ_GUARDED_BY(mMutex) = false;
};

/* static */ mozilla::StaticMutex AsyncScriptCompileTask::sOngoingTasksMutex;
/* static */ Vector<AsyncScriptCompileTask*>
    AsyncScriptCompileTask::sOngoingTasks;
/* static */ bool AsyncScriptCompileTask::sIsShutdownRegistered = false;

class AsyncScriptCompiler;

class AsyncScriptCompilationCompleteTask : public Task {
 public:
  AsyncScriptCompilationCompleteTask(AsyncScriptCompiler* aCompiler,
                                     AsyncScriptCompileTask* aCompileTask)
      : Task(Kind::MainThreadOnly, EventQueuePriority::Normal),
        mCompiler(aCompiler),
        mCompileTask(aCompileTask) {
    MOZ_ASSERT(NS_IsMainThread());
  }

#ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
  bool GetName(nsACString& aName) override {
    aName.AssignLiteral("AsyncScriptCompilationCompleteTask");
    return true;
  }
#endif

  TaskResult Run() override;

 private:
  // NOTE:
  // This field is main-thread only, and this task shouldn't be freed off
  // main thread.
  //
  // This is guaranteed by not having off-thread tasks which depends on this
  // task, because otherwise the off-thread task's mDependencies can be the
  // last reference, which results in freeing this task off main thread.
  //
  // If such task is added, this field must be moved to separate storage.
  RefPtr<AsyncScriptCompiler> mCompiler;

  RefPtr<AsyncScriptCompileTask> mCompileTask;
};

class AsyncScriptCompiler final : public nsIIncrementalStreamLoaderObserver {
 public:
  // Note: References to this class are never held by cycle-collected objects.
  // If at any point a reference is returned to a caller, please update this
  // class to implement cycle collection.
  NS_DECL_ISUPPORTS
  NS_DECL_NSIINCREMENTALSTREAMLOADEROBSERVER

  AsyncScriptCompiler(JSContext* aCx, nsIGlobalObject* aGlobal,
                      const nsACString& aURL, Promise* aPromise)
      : mOptions(aCx),
        mURL(aURL),
        mGlobalObject(aGlobal),
        mPromise(aPromise),
        mScriptLength(0) {}

  [[nodiscard]] nsresult Start(JSContext* aCx,
                               const CompileScriptOptionsDictionary& aOptions,
                               nsIPrincipal* aPrincipal);

  void OnCompilationComplete(AsyncScriptCompileTask* aCompileTask);

 protected:
  virtual ~AsyncScriptCompiler() {
    if (mPromise->State() == Promise::PromiseState::Pending) {
      mPromise->MaybeReject(NS_ERROR_FAILURE);
    }
  }

 private:
  void Reject(JSContext* aCx);
  void Reject(JSContext* aCx, const char* aMxg);

  bool StartCompile(JSContext* aCx);
  bool StartOffThreadCompile(JS::SourceText<Utf8Unit>&& aSrcBuf);
  void FinishCompile(JSContext* aCx);
  void Finish(JSContext* aCx, RefPtr<JS::Stencil>&& aStencil);

  OwningCompileOptions mOptions;
  nsCString mURL;
  nsCOMPtr<nsIGlobalObject> mGlobalObject;
  RefPtr<Promise> mPromise;
  nsString mCharset;
  UniquePtr<Utf8Unit[], JS::FreePolicy> mScriptText;
  size_t mScriptLength;
};

NS_IMPL_ISUPPORTS(AsyncScriptCompiler, nsIIncrementalStreamLoaderObserver)

nsresult AsyncScriptCompiler::Start(
    JSContext* aCx, const CompileScriptOptionsDictionary& aOptions,
    nsIPrincipal* aPrincipal) {
  mCharset = aOptions.mCharset;

  CompileOptions options(aCx);
  options.setFile(mURL.get()).setNoScriptRval(!aOptions.mHasReturnValue);

  if (!aOptions.mLazilyParse) {
    options.setForceFullParse();
  }

  if (NS_WARN_IF(!mOptions.copy(aCx, options))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), mURL);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(
      getter_AddRefs(channel), uri, aPrincipal,
      nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
      nsIContentPolicy::TYPE_INTERNAL_CHROMEUTILS_COMPILED_SCRIPT);
  NS_ENSURE_SUCCESS(rv, rv);

  // allow deprecated HTTP request from SystemPrincipal
  nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();
  loadInfo->SetAllowDeprecatedSystemRequests(true);
  nsCOMPtr<nsIIncrementalStreamLoader> loader;
  rv = NS_NewIncrementalStreamLoader(getter_AddRefs(loader), this);
  NS_ENSURE_SUCCESS(rv, rv);

  return channel->AsyncOpen(loader);
}

bool AsyncScriptCompiler::StartCompile(JSContext* aCx) {
  JS::SourceText<Utf8Unit> srcBuf;
  if (!srcBuf.init(aCx, std::move(mScriptText), mScriptLength)) {
    return false;
  }

  // TODO: This uses the same heuristics and the same threshold as the
  //       JS::CanCompileOffThread, but the heuristics needs to be updated to
  //       reflect the change regarding the Stencil API, and also the thread
  //       management on the consumer side (bug 1846388).
  static constexpr size_t OffThreadMinimumTextLength = 5 * 1000;

  if (StaticPrefs::javascript_options_parallel_parsing() &&
      mScriptLength >= OffThreadMinimumTextLength) {
    if (!StartOffThreadCompile(std::move(srcBuf))) {
      return false;
    }
    return true;
  }

  RefPtr<Stencil> stencil =
      JS::CompileGlobalScriptToStencil(aCx, mOptions, srcBuf);
  if (!stencil) {
    return false;
  }

  Finish(aCx, std::move(stencil));
  return true;
}

bool AsyncScriptCompiler::StartOffThreadCompile(
    JS::SourceText<Utf8Unit>&& aSrcBuf) {
  RefPtr<AsyncScriptCompileTask> compileTask =
      new AsyncScriptCompileTask(std::move(aSrcBuf));

  RefPtr<AsyncScriptCompilationCompleteTask> complationCompleteTask =
      new AsyncScriptCompilationCompleteTask(this, compileTask.get());

  if (!compileTask->Init(mOptions)) {
    return false;
  }

  complationCompleteTask->AddDependency(compileTask.get());

  TaskController::Get()->AddTask(compileTask.forget());
  TaskController::Get()->AddTask(complationCompleteTask.forget());
  return true;
}

Task::TaskResult AsyncScriptCompilationCompleteTask::Run() {
  mCompiler->OnCompilationComplete(mCompileTask.get());
  mCompiler = nullptr;
  mCompileTask = nullptr;
  return TaskResult::Complete;
}

void AsyncScriptCompiler::OnCompilationComplete(
    AsyncScriptCompileTask* aCompileTask) {
  AutoJSAPI jsapi;
  if (!jsapi.Init(mGlobalObject)) {
    mPromise->MaybeReject(NS_ERROR_FAILURE);
    return;
  }

  JSContext* cx = jsapi.cx();
  RefPtr<JS::Stencil> stencil = aCompileTask->StealStencil(cx);
  if (!stencil) {
    Reject(cx);
    return;
  }

  Finish(cx, std::move(stencil));
  return;
}

void AsyncScriptCompiler::Finish(JSContext* aCx,
                                 RefPtr<JS::Stencil>&& aStencil) {
  RefPtr<PrecompiledScript> result =
      new PrecompiledScript(mGlobalObject, aStencil, mOptions);

  mPromise->MaybeResolve(result);
}

void AsyncScriptCompiler::Reject(JSContext* aCx) {
  RootedValue value(aCx, JS::UndefinedValue());
  if (JS_GetPendingException(aCx, &value)) {
    JS_ClearPendingException(aCx);
  }
  mPromise->MaybeReject(value);
}

void AsyncScriptCompiler::Reject(JSContext* aCx, const char* aMsg) {
  nsAutoString msg;
  msg.AppendASCII(aMsg);
  msg.AppendLiteral(": ");
  AppendUTF8toUTF16(mURL, msg);

  RootedValue exn(aCx);
  if (xpc::NonVoidStringToJsval(aCx, msg, &exn)) {
    JS_SetPendingException(aCx, exn);
  }

  Reject(aCx);
}

NS_IMETHODIMP
AsyncScriptCompiler::OnIncrementalData(nsIIncrementalStreamLoader* aLoader,
                                       nsISupports* aContext,
                                       uint32_t aDataLength,
                                       const uint8_t* aData,
                                       uint32_t* aConsumedData) {
  return NS_OK;
}

NS_IMETHODIMP
AsyncScriptCompiler::OnStreamComplete(nsIIncrementalStreamLoader* aLoader,
                                      nsISupports* aContext, nsresult aStatus,
                                      uint32_t aLength, const uint8_t* aBuf) {
  AutoJSAPI jsapi;
  if (!jsapi.Init(mGlobalObject)) {
    mPromise->MaybeReject(NS_ERROR_FAILURE);
    return NS_OK;
  }

  JSContext* cx = jsapi.cx();

  if (NS_FAILED(aStatus)) {
    Reject(cx, "Unable to load script");
    return NS_OK;
  }

  nsresult rv = ScriptLoader::ConvertToUTF8(
      nullptr, aBuf, aLength, mCharset, nullptr, mScriptText, mScriptLength);
  if (NS_FAILED(rv)) {
    Reject(cx, "Unable to decode script");
    return NS_OK;
  }

  if (!StartCompile(cx)) {
    Reject(cx);
  }

  return NS_OK;
}

namespace mozilla {
namespace dom {

/* static */
already_AddRefed<Promise> ChromeUtils::CompileScript(
    GlobalObject& aGlobal, const nsAString& aURL,
    const CompileScriptOptionsDictionary& aOptions, ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  MOZ_ASSERT(global);

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  NS_ConvertUTF16toUTF8 url(aURL);
  RefPtr<AsyncScriptCompiler> compiler =
      new AsyncScriptCompiler(aGlobal.Context(), global, url, promise);

  nsresult rv = compiler->Start(aGlobal.Context(), aOptions,
                                aGlobal.GetSubjectPrincipal());
  if (NS_FAILED(rv)) {
    promise->MaybeReject(rv);
  }

  return promise.forget();
}

PrecompiledScript::PrecompiledScript(nsISupports* aParent,
                                     RefPtr<JS::Stencil> aStencil,
                                     JS::ReadOnlyCompileOptions& aOptions)
    : mParent(aParent),
      mStencil(aStencil),
      mURL(aOptions.filename().c_str()),
      mHasReturnValue(!aOptions.noScriptRval) {
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(aStencil);
#ifdef DEBUG
  JS::InstantiateOptions options(aOptions);
  options.assertDefault();
#endif
};

void PrecompiledScript::ExecuteInGlobal(JSContext* aCx, HandleObject aGlobal,
                                        const ExecuteInGlobalOptions& aOptions,
                                        MutableHandleValue aRval,
                                        ErrorResult& aRv) {
  {
    RootedObject targetObj(aCx, JS_FindCompilationScope(aCx, aGlobal));
    // Use AutoEntryScript for its ReportException method call.
    // This will ensure notified any exception happening in the content script
    // directly to the console, so that exceptions are flagged with the right
    // innerWindowID. It helps these exceptions to appear in the page's web
    // console.
    AutoEntryScript aes(targetObj, "pre-compiled-script execution");
    JSContext* cx = aes.cx();

    // See assertion in constructor.
    JS::InstantiateOptions options;
    Rooted<JSScript*> script(
        cx, JS::InstantiateGlobalStencil(cx, options, mStencil));
    if (!script) {
      aRv.NoteJSContextException(aCx);
      return;
    }

    if (!JS_ExecuteScript(cx, script, aRval)) {
      JS::RootedValue exn(cx);
      if (aOptions.mReportExceptions) {
        // Note that ReportException will consume the exception.
        aes.ReportException();
      } else {
        // Set the exception on our caller's cx.
        aRv.MightThrowJSException();
        aRv.StealExceptionFromJSContext(cx);
      }
      return;
    }
  }

  JS_WrapValue(aCx, aRval);
}

void PrecompiledScript::GetUrl(nsAString& aUrl) { CopyUTF8toUTF16(mURL, aUrl); }

bool PrecompiledScript::HasReturnValue() { return mHasReturnValue; }

JSObject* PrecompiledScript::WrapObject(JSContext* aCx,
                                        HandleObject aGivenProto) {
  return PrecompiledScript_Binding::Wrap(aCx, this, aGivenProto);
}

bool PrecompiledScript::IsBlackForCC(bool aTracingNeeded) {
  return (nsCCUncollectableMarker::sGeneration && HasKnownLiveWrapper() &&
          (!aTracingNeeded || HasNothingToTrace(this)));
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(PrecompiledScript, mParent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PrecompiledScript)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(PrecompiledScript)
  return tmp->IsBlackForCC(false);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(PrecompiledScript)
  return tmp->IsBlackForCC(true);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(PrecompiledScript)
  return tmp->IsBlackForCC(false);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(PrecompiledScript)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PrecompiledScript)

}  // namespace dom
}  // namespace mozilla

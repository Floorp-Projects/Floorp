/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerPrivate.h"

#include "mozIThirdPartyUtil.h"
#include "nsIClassInfo.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIConsoleService.h"
#include "nsIDOMFile.h"
#include "nsIDocument.h"
#include "nsIDocShell.h"
#include "nsIJSContextStack.h"
#include "nsIMemoryReporter.h"
#include "nsIPermissionManager.h"
#include "nsIScriptError.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptSecurityManager.h"
#include "nsPIDOMWindow.h"
#include "nsITextToSubURI.h"
#include "nsITimer.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIXPConnect.h"
#include "nsIXPCScriptNotify.h"

#include "jsfriendapi.h"
#include "jsdbgapi.h"
#include "jsfriendapi.h"
#include "jsprf.h"
#include "js/MemoryMetrics.h"
#include "nsAlgorithm.h"
#include "nsContentUtils.h"
#include "nsError.h"
#include "nsDOMJSUtils.h"
#include "nsGUIEvent.h"
#include "nsJSEnvironment.h"
#include "nsJSUtils.h"
#include "nsNetUtil.h"
#include "nsSandboxFlags.h"
#include "nsThreadUtils.h"
#include "xpcpublic.h"
#include "mozilla/Attributes.h"
#include "mozilla/Likely.h"

#ifdef ANDROID
#include <android/log.h>
#endif

#include "Events.h"
#include "Exceptions.h"
#include "File.h"
#include "ImageData.h"
#include "Principal.h"
#include "RuntimeService.h"
#include "ScriptLoader.h"
#include "Worker.h"
#include "WorkerFeature.h"
#include "WorkerScope.h"

#if 0 // Define to run GC more often.
#define EXTRA_GC
#endif

// GC will run once every thirty seconds during normal execution.
#define NORMAL_GC_TIMER_DELAY_MS 30000

// GC will run five seconds after the last event is processed.
#define IDLE_GC_TIMER_DELAY_MS 5000

using mozilla::MutexAutoLock;
using mozilla::TimeDuration;
using mozilla::TimeStamp;
using mozilla::dom::workers::exceptions::ThrowDOMExceptionForNSResult;

USING_WORKERS_NAMESPACE
using namespace mozilla::dom::workers::events;
using namespace mozilla::dom;

NS_MEMORY_REPORTER_MALLOC_SIZEOF_FUN(JsWorkerMallocSizeOf, "js-worker")

namespace {

const char gErrorChars[] = "error";
const char gMessageChars[] = "message";

template <class T>
class AutoPtrComparator
{
  typedef nsAutoPtr<T> A;
  typedef T* B;

public:
  bool Equals(const A& a, const B& b) const {
    return a && b ? *a == *b : !a && !b ? true : false;
  }
  bool LessThan(const A& a, const B& b) const {
    return a && b ? *a < *b : b ? true : false;
  }
};

template <class T>
inline AutoPtrComparator<T>
GetAutoPtrComparator(const nsTArray<nsAutoPtr<T> >&)
{
  return AutoPtrComparator<T>();
}

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

struct WorkerStructuredCloneCallbacks
{
  static JSObject*
  Read(JSContext* aCx, JSStructuredCloneReader* aReader, uint32_t aTag,
       uint32_t aData, void* aClosure)
  {
    // See if object is a nsIDOMFile pointer.
    if (aTag == DOMWORKER_SCTAG_FILE) {
      JS_ASSERT(!aData);

      nsIDOMFile* file;
      if (JS_ReadBytes(aReader, &file, sizeof(file))) {
        JS_ASSERT(file);

#ifdef DEBUG
        {
          // File should not be mutable.
          nsCOMPtr<nsIMutable> mutableFile = do_QueryInterface(file);
          bool isMutable;
          NS_ASSERTION(NS_SUCCEEDED(mutableFile->GetMutable(&isMutable)) &&
                       !isMutable,
                       "Only immutable file should be passed to worker");
        }
#endif

        // nsIDOMFiles should be threadsafe, thus we will use the same instance
        // in the worker.
        JSObject* jsFile = file::CreateFile(aCx, file);
        return jsFile;
      }
    }
    // See if object is a nsIDOMBlob pointer.
    else if (aTag == DOMWORKER_SCTAG_BLOB) {
      JS_ASSERT(!aData);

      nsIDOMBlob* blob;
      if (JS_ReadBytes(aReader, &blob, sizeof(blob))) {
        JS_ASSERT(blob);

#ifdef DEBUG
        {
          // Blob should not be mutable.
          nsCOMPtr<nsIMutable> mutableBlob = do_QueryInterface(blob);
          bool isMutable;
          NS_ASSERTION(NS_SUCCEEDED(mutableBlob->GetMutable(&isMutable)) &&
                       !isMutable,
                       "Only immutable blob should be passed to worker");
        }
#endif

        // nsIDOMBlob should be threadsafe, thus we will use the same instance
        // in the worker.
        JSObject* jsBlob = file::CreateBlob(aCx, blob);
        return jsBlob;
      }
    }
    // See if the object is an ImageData.
    else if (aTag == SCTAG_DOM_IMAGEDATA) {
      JS_ASSERT(!aData);

      // Read the information out of the stream.
      uint32_t width, height;
      jsval dataArray;
      if (!JS_ReadUint32Pair(aReader, &width, &height) ||
          !JS_ReadTypedArray(aReader, &dataArray))
      {
        return nullptr;
      }
      MOZ_ASSERT(dataArray.isObject());

      // Construct the ImageData.
      JSObject* obj = imagedata::Create(aCx, width, height,
                                        JSVAL_TO_OBJECT(dataArray));
      return obj;
    }

    Error(aCx, 0);
    return nullptr;
  }

  static JSBool
  Write(JSContext* aCx, JSStructuredCloneWriter* aWriter, JSObject* aObj,
        void* aClosure)
  {
    NS_ASSERTION(aClosure, "Null pointer!");

    // We'll stash any nsISupports pointers that need to be AddRef'd here.
    nsTArray<nsCOMPtr<nsISupports> >* clonedObjects =
      static_cast<nsTArray<nsCOMPtr<nsISupports> >*>(aClosure);

    // See if this is a File object.
    {
      nsIDOMFile* file = file::GetDOMFileFromJSObject(aObj);
      if (file) {
        if (JS_WriteUint32Pair(aWriter, DOMWORKER_SCTAG_FILE, 0) &&
            JS_WriteBytes(aWriter, &file, sizeof(file))) {
          clonedObjects->AppendElement(file);
          return true;
        }
      }
    }

    // See if this is a Blob object.
    {
      nsIDOMBlob* blob = file::GetDOMBlobFromJSObject(aObj);
      if (blob) {
        nsCOMPtr<nsIMutable> mutableBlob = do_QueryInterface(blob);
        if (mutableBlob && NS_SUCCEEDED(mutableBlob->SetMutable(false)) &&
            JS_WriteUint32Pair(aWriter, DOMWORKER_SCTAG_BLOB, 0) &&
            JS_WriteBytes(aWriter, &blob, sizeof(blob))) {
          clonedObjects->AppendElement(blob);
          return true;
        }
      }
    }

    // See if this is an ImageData object.
    if (imagedata::IsImageData(aObj)) {
      // Pull the properties off the object.
      uint32_t width = imagedata::GetWidth(aObj);
      uint32_t height = imagedata::GetHeight(aObj);
      JSObject* data = imagedata::GetData(aObj);

      // Write the structured clone.
      return JS_WriteUint32Pair(aWriter, SCTAG_DOM_IMAGEDATA, 0) &&
             JS_WriteUint32Pair(aWriter, width, height) &&
             JS_WriteTypedArray(aWriter, OBJECT_TO_JSVAL(data));
    }

    Error(aCx, 0);
    return false;
  }

  static void
  Error(JSContext* aCx, uint32_t /* aErrorId */)
  {
    ThrowDOMExceptionForNSResult(aCx, NS_ERROR_DOM_DATA_CLONE_ERR);
  }
};

JSStructuredCloneCallbacks gWorkerStructuredCloneCallbacks = {
  WorkerStructuredCloneCallbacks::Read,
  WorkerStructuredCloneCallbacks::Write,
  WorkerStructuredCloneCallbacks::Error
};

struct MainThreadWorkerStructuredCloneCallbacks
{
  static JSObject*
  Read(JSContext* aCx, JSStructuredCloneReader* aReader, uint32_t aTag,
       uint32_t aData, void* aClosure)
  {
    AssertIsOnMainThread();

    // See if object is a nsIDOMFile pointer.
    if (aTag == DOMWORKER_SCTAG_FILE) {
      JS_ASSERT(!aData);

      nsIDOMFile* file;
      if (JS_ReadBytes(aReader, &file, sizeof(file))) {
        JS_ASSERT(file);

#ifdef DEBUG
        {
          // File should not be mutable.
          nsCOMPtr<nsIMutable> mutableFile = do_QueryInterface(file);
          bool isMutable;
          NS_ASSERTION(NS_SUCCEEDED(mutableFile->GetMutable(&isMutable)) &&
                       !isMutable,
                       "Only immutable file should be passed to worker");
        }
#endif

        // nsIDOMFiles should be threadsafe, thus we will use the same instance
        // on the main thread.
        jsval wrappedFile;
        nsresult rv =
          nsContentUtils::WrapNative(aCx, JS_GetGlobalForScopeChain(aCx), file,
                                     &NS_GET_IID(nsIDOMFile), &wrappedFile);
        if (NS_FAILED(rv)) {
          Error(aCx, DATA_CLONE_ERR);
          return nullptr;
        }

        return JSVAL_TO_OBJECT(wrappedFile);
      }
    }
    // See if object is a nsIDOMBlob pointer.
    else if (aTag == DOMWORKER_SCTAG_BLOB) {
      JS_ASSERT(!aData);

      nsIDOMBlob* blob;
      if (JS_ReadBytes(aReader, &blob, sizeof(blob))) {
        JS_ASSERT(blob);

#ifdef DEBUG
        {
          // Blob should not be mutable.
          nsCOMPtr<nsIMutable> mutableBlob = do_QueryInterface(blob);
          bool isMutable;
          NS_ASSERTION(NS_SUCCEEDED(mutableBlob->GetMutable(&isMutable)) &&
                       !isMutable,
                       "Only immutable blob should be passed to worker");
        }
#endif

        // nsIDOMBlobs should be threadsafe, thus we will use the same instance
        // on the main thread.
        jsval wrappedBlob;
        nsresult rv =
          nsContentUtils::WrapNative(aCx, JS_GetGlobalForScopeChain(aCx), blob,
                                     &NS_GET_IID(nsIDOMBlob), &wrappedBlob);
        if (NS_FAILED(rv)) {
          Error(aCx, DATA_CLONE_ERR);
          return nullptr;
        }

        return JSVAL_TO_OBJECT(wrappedBlob);
      }
    }

    JS_ClearPendingException(aCx);
    return NS_DOMReadStructuredClone(aCx, aReader, aTag, aData, nullptr);
  }

  static JSBool
  Write(JSContext* aCx, JSStructuredCloneWriter* aWriter, JSObject* aObj,
        void* aClosure)
  {
    AssertIsOnMainThread();

    NS_ASSERTION(aClosure, "Null pointer!");

    // We'll stash any nsISupports pointers that need to be AddRef'd here.
    nsTArray<nsCOMPtr<nsISupports> >* clonedObjects =
      static_cast<nsTArray<nsCOMPtr<nsISupports> >*>(aClosure);

    // See if this is a wrapped native.
    nsCOMPtr<nsIXPConnectWrappedNative> wrappedNative;
    nsContentUtils::XPConnect()->
      GetWrappedNativeOfJSObject(aCx, aObj, getter_AddRefs(wrappedNative));

    if (wrappedNative) {
      // Get the raw nsISupports out of it.
      nsISupports* wrappedObject = wrappedNative->Native();
      NS_ASSERTION(wrappedObject, "Null pointer?!");

      nsISupports* ccISupports = nullptr;
      wrappedObject->QueryInterface(NS_GET_IID(nsCycleCollectionISupports),
                                    reinterpret_cast<void**>(&ccISupports));
      if (ccISupports) {
        NS_WARNING("Cycle collected objects are not supported!");
      }
      else {
        // See if the wrapped native is a nsIDOMFile.
        nsCOMPtr<nsIDOMFile> file = do_QueryInterface(wrappedObject);
        if (file) {
          nsCOMPtr<nsIMutable> mutableFile = do_QueryInterface(file);
          if (mutableFile && NS_SUCCEEDED(mutableFile->SetMutable(false))) {
            nsIDOMFile* filePtr = file;
            if (JS_WriteUint32Pair(aWriter, DOMWORKER_SCTAG_FILE, 0) &&
                JS_WriteBytes(aWriter, &filePtr, sizeof(filePtr))) {
              clonedObjects->AppendElement(file);
              return true;
            }
          }
        }

        // See if the wrapped native is a nsIDOMBlob.
        nsCOMPtr<nsIDOMBlob> blob = do_QueryInterface(wrappedObject);
        if (blob) {
          nsCOMPtr<nsIMutable> mutableBlob = do_QueryInterface(blob);
          if (mutableBlob && NS_SUCCEEDED(mutableBlob->SetMutable(false))) {
            nsIDOMBlob* blobPtr = blob;
            if (JS_WriteUint32Pair(aWriter, DOMWORKER_SCTAG_BLOB, 0) &&
                JS_WriteBytes(aWriter, &blobPtr, sizeof(blobPtr))) {
              clonedObjects->AppendElement(blob);
              return true;
            }
          }
        }
      }
    }

    JS_ClearPendingException(aCx);
    return NS_DOMWriteStructuredClone(aCx, aWriter, aObj, nullptr);
  }

  static void
  Error(JSContext* aCx, uint32_t aErrorId)
  {
    AssertIsOnMainThread();

    NS_DOMStructuredCloneError(aCx, aErrorId);
  }
};

JSStructuredCloneCallbacks gMainThreadWorkerStructuredCloneCallbacks = {
  MainThreadWorkerStructuredCloneCallbacks::Read,
  MainThreadWorkerStructuredCloneCallbacks::Write,
  MainThreadWorkerStructuredCloneCallbacks::Error
};

struct ChromeWorkerStructuredCloneCallbacks
{
  static JSObject*
  Read(JSContext* aCx, JSStructuredCloneReader* aReader, uint32_t aTag,
       uint32_t aData, void* aClosure)
  {
    return WorkerStructuredCloneCallbacks::Read(aCx, aReader, aTag, aData,
                                                aClosure);
  }

  static JSBool
  Write(JSContext* aCx, JSStructuredCloneWriter* aWriter, JSObject* aObj,
        void* aClosure)
  {
    return WorkerStructuredCloneCallbacks::Write(aCx, aWriter, aObj, aClosure);
  }

  static void
  Error(JSContext* aCx, uint32_t aErrorId)
  {
    return WorkerStructuredCloneCallbacks::Error(aCx, aErrorId);
  }
};

JSStructuredCloneCallbacks gChromeWorkerStructuredCloneCallbacks = {
  ChromeWorkerStructuredCloneCallbacks::Read,
  ChromeWorkerStructuredCloneCallbacks::Write,
  ChromeWorkerStructuredCloneCallbacks::Error
};

struct MainThreadChromeWorkerStructuredCloneCallbacks
{
  static JSObject*
  Read(JSContext* aCx, JSStructuredCloneReader* aReader, uint32_t aTag,
       uint32_t aData, void* aClosure)
  {
    AssertIsOnMainThread();

    JSObject* clone =
      MainThreadWorkerStructuredCloneCallbacks::Read(aCx, aReader, aTag, aData,
                                                     aClosure);
    if (clone) {
      return clone;
    }

    clone =
      ChromeWorkerStructuredCloneCallbacks::Read(aCx, aReader, aTag, aData,
                                                 aClosure);
    if (clone) {
      return clone;
    }

    JS_ClearPendingException(aCx);
    return NS_DOMReadStructuredClone(aCx, aReader, aTag, aData, nullptr);
  }

  static JSBool
  Write(JSContext* aCx, JSStructuredCloneWriter* aWriter, JSObject* aObj,
        void* aClosure)
  {
    AssertIsOnMainThread();

    if (MainThreadWorkerStructuredCloneCallbacks::Write(aCx, aWriter, aObj,
                                                        aClosure) ||
        ChromeWorkerStructuredCloneCallbacks::Write(aCx, aWriter, aObj,
                                                    aClosure) ||
        NS_DOMWriteStructuredClone(aCx, aWriter, aObj, nullptr)) {
      return true;
    }

    return false;
  }

  static void
  Error(JSContext* aCx, uint32_t aErrorId)
  {
    AssertIsOnMainThread();

    NS_DOMStructuredCloneError(aCx, aErrorId);
  }
};

JSStructuredCloneCallbacks gMainThreadChromeWorkerStructuredCloneCallbacks = {
  MainThreadChromeWorkerStructuredCloneCallbacks::Read,
  MainThreadChromeWorkerStructuredCloneCallbacks::Write,
  MainThreadChromeWorkerStructuredCloneCallbacks::Error
};

class MainThreadReleaseRunnable : public nsRunnable
{
  nsCOMPtr<nsIThread> mThread;
  nsTArray<nsCOMPtr<nsISupports> > mDoomed;

public:
  MainThreadReleaseRunnable(nsCOMPtr<nsIThread>& aThread,
                            nsTArray<nsCOMPtr<nsISupports> >& aDoomed)
  {
    mThread.swap(aThread);
    mDoomed.SwapElements(aDoomed);
  }

  MainThreadReleaseRunnable(nsTArray<nsCOMPtr<nsISupports> >& aDoomed)
  {
    mDoomed.SwapElements(aDoomed);
  }

  NS_IMETHOD
  Run()
  {
    mDoomed.Clear();

    if (mThread) {
      RuntimeService* runtime = RuntimeService::GetService();
      NS_ASSERTION(runtime, "This should never be null!");

      runtime->NoteIdleThread(mThread);
    }

    return NS_OK;
  }
};

class WorkerFinishedRunnable : public WorkerControlRunnable
{
  WorkerPrivate* mFinishedWorker;
  nsCOMPtr<nsIThread> mThread;

public:
  WorkerFinishedRunnable(WorkerPrivate* aWorkerPrivate,
                         WorkerPrivate* aFinishedWorker,
                         nsIThread* aFinishedThread)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThread, UnchangedBusyCount),
    mFinishedWorker(aFinishedWorker), mThread(aFinishedThread)
  { }

  bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    // Silence bad assertions.
    return true;
  }

  void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult)
  {
    // Silence bad assertions.
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    nsTArray<nsCOMPtr<nsISupports> > doomed;
    mFinishedWorker->ForgetMainThreadObjects(doomed);

    nsRefPtr<MainThreadReleaseRunnable> runnable =
      new MainThreadReleaseRunnable(mThread, doomed);
    if (NS_FAILED(NS_DispatchToMainThread(runnable, NS_DISPATCH_NORMAL))) {
      NS_WARNING("Failed to dispatch, going to leak!");
    }

    mFinishedWorker->Finish(aCx);

    RuntimeService* runtime = RuntimeService::GetService();
    NS_ASSERTION(runtime, "This should never be null!");

    runtime->UnregisterWorker(aCx, mFinishedWorker);

    mFinishedWorker->Release();
    return true;
  }
};

class TopLevelWorkerFinishedRunnable : public nsRunnable
{
  WorkerPrivate* mFinishedWorker;
  nsCOMPtr<nsIThread> mThread;

public:
  TopLevelWorkerFinishedRunnable(WorkerPrivate* aFinishedWorker,
                                 nsIThread* aFinishedThread)
  : mFinishedWorker(aFinishedWorker), mThread(aFinishedThread)
  {
    aFinishedWorker->AssertIsOnWorkerThread();
  }

  NS_IMETHOD
  Run()
  {
    AssertIsOnMainThread();

    RuntimeService::AutoSafeJSContext cx;

    mFinishedWorker->Finish(cx);

    RuntimeService* runtime = RuntimeService::GetService();
    NS_ASSERTION(runtime, "This should never be null!");

    runtime->UnregisterWorker(cx, mFinishedWorker);

    nsTArray<nsCOMPtr<nsISupports> > doomed;
    mFinishedWorker->ForgetMainThreadObjects(doomed);

    nsRefPtr<MainThreadReleaseRunnable> runnable =
      new MainThreadReleaseRunnable(doomed);
    if (NS_FAILED(NS_DispatchToCurrentThread(runnable))) {
      NS_WARNING("Failed to dispatch, going to leak!");
    }

    if (mThread) {
      runtime->NoteIdleThread(mThread);
    }

    mFinishedWorker->Release();

    return NS_OK;
  }
};

class ModifyBusyCountRunnable : public WorkerControlRunnable
{
  bool mIncrease;

public:
  ModifyBusyCountRunnable(WorkerPrivate* aWorkerPrivate, bool aIncrease)
  : WorkerControlRunnable(aWorkerPrivate, ParentThread, UnchangedBusyCount),
    mIncrease(aIncrease)
  { }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    return aWorkerPrivate->ModifyBusyCount(aCx, mIncrease);
  }

  void
  PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate, bool aRunResult)
  {
    if (mIncrease) {
      WorkerControlRunnable::PostRun(aCx, aWorkerPrivate, aRunResult);
      return;
    }
    // Don't do anything here as it's possible that aWorkerPrivate has been
    // deleted.
  }
};

class CompileScriptRunnable : public WorkerRunnable
{
public:
  CompileScriptRunnable(WorkerPrivate* aWorkerPrivate)
  : WorkerRunnable(aWorkerPrivate, WorkerThread, ModifyBusyCount,
                   SkipWhenClearing)
  { }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    JSObject* global = CreateDedicatedWorkerGlobalScope(aCx);
    if (!global) {
      NS_WARNING("Failed to make global!");
      return false;
    }

    JSAutoCompartment ac(aCx, global);
    JS_SetGlobalObject(aCx, global);

    return scriptloader::LoadWorkerScript(aCx);
  }
};

class CloseEventRunnable : public WorkerRunnable
{
public:
  CloseEventRunnable(WorkerPrivate* aWorkerPrivate)
  : WorkerRunnable(aWorkerPrivate, WorkerThread, UnchangedBusyCount,
                   SkipWhenClearing)
  { }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    JSObject* target = JS_GetGlobalObject(aCx);
    NS_ASSERTION(target, "This must never be null!");

    aWorkerPrivate->CloseHandlerStarted();

    JSString* type = JS_InternString(aCx, "close");
    if (!type) {
      return false;
    }

    JSObject* event = CreateGenericEvent(aCx, type, false, false, false);
    if (!event) {
      return false;
    }

    bool ignored;
    return DispatchEventToTarget(aCx, target, event, &ignored);
  }

  void
  PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate, bool aRunResult)
  {
    // Report errors.
    WorkerRunnable::PostRun(aCx, aWorkerPrivate, aRunResult);

    // Match the busy count increase from NotifyRunnable.
    if (!aWorkerPrivate->ModifyBusyCountFromWorker(aCx, false)) {
      JS_ReportPendingException(aCx);
    }

    aWorkerPrivate->CloseHandlerFinished();
  }
};

class MessageEventRunnable : public WorkerRunnable
{
  uint64_t* mData;
  size_t mDataByteCount;
  nsTArray<nsCOMPtr<nsISupports> > mClonedObjects;

public:
  MessageEventRunnable(WorkerPrivate* aWorkerPrivate, Target aTarget,
                       JSAutoStructuredCloneBuffer& aData,
                       nsTArray<nsCOMPtr<nsISupports> >& aClonedObjects)
  : WorkerRunnable(aWorkerPrivate, aTarget, aTarget == WorkerThread ?
                                                       ModifyBusyCount :
                                                       UnchangedBusyCount,
                   SkipWhenClearing)
  {
    aData.steal(&mData, &mDataByteCount);

    if (!mClonedObjects.SwapElements(aClonedObjects)) {
      NS_ERROR("This should never fail!");
    }
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    JSAutoStructuredCloneBuffer buffer;
    buffer.adopt(mData, mDataByteCount);

    mData = nullptr;
    mDataByteCount = 0;

    bool mainRuntime;
    JSObject* target;
    if (mTarget == ParentThread) {
      // Don't fire this event if the JS object has been disconnected from the
      // private object.
      if (!aWorkerPrivate->IsAcceptingEvents()) {
        return true;
      }

      mainRuntime = !aWorkerPrivate->GetParent();

      target = aWorkerPrivate->GetJSObject();
      NS_ASSERTION(target, "Must have a target!");

      if (aWorkerPrivate->IsSuspended()) {
        aWorkerPrivate->QueueRunnable(this);
        buffer.steal(&mData, &mDataByteCount);
        return true;
      }

      aWorkerPrivate->AssertInnerWindowIsCorrect();
    }
    else {
      NS_ASSERTION(aWorkerPrivate == GetWorkerPrivateFromContext(aCx),
                   "Badness!");
      mainRuntime = false;
      target = JS_GetGlobalObject(aCx);
    }

    NS_ASSERTION(target, "This should never be null!");

    JSObject* event =
      CreateMessageEvent(aCx, buffer, mClonedObjects, mainRuntime);
    if (!event) {
      return false;
    }

    bool dummy;
    return DispatchEventToTarget(aCx, target, event, &dummy);
  }

  void PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate, bool aRunResult)
  {
    // Notify before WorkerRunnable::PostRun, since that can kill aWorkerPrivate
    NotifyScriptExecutedIfNeeded();
    WorkerRunnable::PostRun(aCx, aWorkerPrivate, aRunResult);
  }
};

class NotifyRunnable : public WorkerControlRunnable
{
  Status mStatus;

public:
  NotifyRunnable(WorkerPrivate* aWorkerPrivate, Status aStatus)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThread, UnchangedBusyCount),
    mStatus(aStatus)
  {
    NS_ASSERTION(aStatus == Terminating || aStatus == Canceling ||
                 aStatus == Killing, "Bad status!");
  }

  bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    // Modify here, but not in PostRun! This busy count addition will be matched
    // by the CloseEventRunnable.
    return aWorkerPrivate->ModifyBusyCount(aCx, true);
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    return aWorkerPrivate->NotifyInternal(aCx, mStatus);
  }
};

class CloseRunnable MOZ_FINAL : public WorkerControlRunnable
{
public:
  CloseRunnable(WorkerPrivate* aWorkerPrivate)
  : WorkerControlRunnable(aWorkerPrivate, ParentThread, UnchangedBusyCount)
  { }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    // This busy count will be matched by the CloseEventRunnable.
    return aWorkerPrivate->ModifyBusyCount(aCx, true) &&
           aWorkerPrivate->Close(aCx);
  }
};

class SuspendRunnable : public WorkerControlRunnable
{
public:
  SuspendRunnable(WorkerPrivate* aWorkerPrivate)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThread, UnchangedBusyCount)
  { }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    return aWorkerPrivate->SuspendInternal(aCx);
  }
};

class ResumeRunnable : public WorkerControlRunnable
{
public:
  ResumeRunnable(WorkerPrivate* aWorkerPrivate)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThread, UnchangedBusyCount)
  { }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    return aWorkerPrivate->ResumeInternal(aCx);
  }
};

class ReportErrorRunnable : public WorkerRunnable
{
  nsString mMessage;
  nsString mFilename;
  nsString mLine;
  uint32_t mLineNumber;
  uint32_t mColumnNumber;
  uint32_t mFlags;
  uint32_t mErrorNumber;

public:
  ReportErrorRunnable(WorkerPrivate* aWorkerPrivate, const nsString& aMessage,
                      const nsString& aFilename, const nsString& aLine,
                      uint32_t aLineNumber, uint32_t aColumnNumber,
                      uint32_t aFlags, uint32_t aErrorNumber)
  : WorkerRunnable(aWorkerPrivate, ParentThread, UnchangedBusyCount,
                   SkipWhenClearing),
    mMessage(aMessage), mFilename(aFilename), mLine(aLine),
    mLineNumber(aLineNumber), mColumnNumber(aColumnNumber), mFlags(aFlags),
    mErrorNumber(aErrorNumber)
  { }

  void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult)
  {
    aWorkerPrivate->AssertIsOnWorkerThread();

    // Dispatch may fail if the worker was canceled, no need to report that as
    // an error, so don't call base class PostDispatch.
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    JSObject* target = aWorkerPrivate->IsAcceptingEvents() ?
                       aWorkerPrivate->GetJSObject() :
                       nullptr;

    uint64_t innerWindowId;

    WorkerPrivate* parent = aWorkerPrivate->GetParent();
    if (parent) {
      innerWindowId = 0;
    }
    else {
      AssertIsOnMainThread();

      if (aWorkerPrivate->IsSuspended()) {
        aWorkerPrivate->QueueRunnable(this);
        return true;
      }

      aWorkerPrivate->AssertInnerWindowIsCorrect();

      innerWindowId = aWorkerPrivate->GetInnerWindowId();
    }

    return ReportErrorRunnable::ReportError(aCx, parent, true, target, mMessage,
                                            mFilename, mLine, mLineNumber,
                                            mColumnNumber, mFlags,
                                            mErrorNumber, innerWindowId);
  }

  void PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate, bool aRunResult)
  {
    // Notify before WorkerRunnable::PostRun, since that can kill aWorkerPrivate
    NotifyScriptExecutedIfNeeded();
    WorkerRunnable::PostRun(aCx, aWorkerPrivate, aRunResult);
  }

  static bool
  ReportError(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
              bool aFireAtScope, JSObject* aTarget, const nsString& aMessage,
              const nsString& aFilename, const nsString& aLine,
              uint32_t aLineNumber, uint32_t aColumnNumber, uint32_t aFlags,
              uint32_t aErrorNumber, uint64_t aInnerWindowId)
  {
    if (aWorkerPrivate) {
      aWorkerPrivate->AssertIsOnWorkerThread();
    }
    else {
      AssertIsOnMainThread();
    }

    JSString* message = JS_NewUCStringCopyN(aCx, aMessage.get(),
                                            aMessage.Length());
    if (!message) {
      return false;
    }

    JSString* filename = JS_NewUCStringCopyN(aCx, aFilename.get(),
                                             aFilename.Length());
    if (!filename) {
      return false;
    }

    // We should not fire error events for warnings but instead make sure that
    // they show up in the error console.
    if (!JSREPORT_IS_WARNING(aFlags)) {
      // First fire an ErrorEvent at the worker.
      if (aTarget) {
        JSObject* event =
          CreateErrorEvent(aCx, message, filename, aLineNumber, !aWorkerPrivate);
        if (!event) {
          return false;
        }

        bool preventDefaultCalled;
        if (!DispatchEventToTarget(aCx, aTarget, event, &preventDefaultCalled)) {
          return false;
        }

        if (preventDefaultCalled) {
          return true;
        }
      }

      // Now fire an event at the global object, but don't do that if the error
      // code is too much recursion and this is the same script threw the error.
      if (aFireAtScope && (aTarget || aErrorNumber != JSMSG_OVER_RECURSED)) {
        aTarget = JS_GetGlobalForScopeChain(aCx);
        NS_ASSERTION(aTarget, "This should never be null!");

        bool preventDefaultCalled;
        nsIScriptGlobalObject* sgo;

        if (aWorkerPrivate ||
            !(sgo = nsJSUtils::GetStaticScriptGlobal(aTarget))) {
          // Fire a normal ErrorEvent if we're running on a worker thread.
          JSObject* event =
            CreateErrorEvent(aCx, message, filename, aLineNumber, false);
          if (!event) {
            return false;
          }

          if (!DispatchEventToTarget(aCx, aTarget, event,
                                     &preventDefaultCalled)) {
            return false;
          }
        }
        else {
          // Icky, we have to fire an nsScriptErrorEvent...
          nsScriptErrorEvent event(true, NS_LOAD_ERROR);
          event.lineNr = aLineNumber;
          event.errorMsg = aMessage.get();
          event.fileName = aFilename.get();

          nsEventStatus status = nsEventStatus_eIgnore;
          if (NS_FAILED(sgo->HandleScriptError(&event, &status))) {
            NS_WARNING("Failed to dispatch main thread error event!");
            status = nsEventStatus_eIgnore;
          }

          preventDefaultCalled = status == nsEventStatus_eConsumeNoDefault;
        }

        if (preventDefaultCalled) {
          return true;
        }
      }
    }

    // Now fire a runnable to do the same on the parent's thread if we can.
    if (aWorkerPrivate) {
      nsRefPtr<ReportErrorRunnable> runnable =
        new ReportErrorRunnable(aWorkerPrivate, aMessage, aFilename, aLine,
                                aLineNumber, aColumnNumber, aFlags,
                                aErrorNumber);
      return runnable->Dispatch(aCx);
    }

    // Otherwise log an error to the error console.
    nsCOMPtr<nsIScriptError> scriptError =
      do_CreateInstance(NS_SCRIPTERROR_CONTRACTID);
    NS_WARN_IF_FALSE(scriptError, "Failed to create script error!");

    if (scriptError) {
      if (NS_FAILED(scriptError->InitWithWindowID(aMessage,
                                                  aFilename,
                                                  aLine, aLineNumber,
                                                  aColumnNumber, aFlags,
                                                  "Web Worker",
                                                  aInnerWindowId))) {
        NS_WARNING("Failed to init script error!");
        scriptError = nullptr;
      }
    }

    nsCOMPtr<nsIConsoleService> consoleService =
      do_GetService(NS_CONSOLESERVICE_CONTRACTID);
    NS_WARN_IF_FALSE(consoleService, "Failed to get console service!");

    bool logged = false;

    if (consoleService) {
      if (scriptError) {
        if (NS_SUCCEEDED(consoleService->LogMessage(scriptError))) {
          logged = true;
        }
        else {
          NS_WARNING("Failed to log script error!");
        }
      }
      else if (NS_SUCCEEDED(consoleService->LogStringMessage(aMessage.get()))) {
        logged = true;
      }
      else {
        NS_WARNING("Failed to log script error!");
      }
    }

    if (!logged) {
      NS_ConvertUTF16toUTF8 msg(aMessage);
#ifdef ANDROID
      __android_log_print(ANDROID_LOG_INFO, "Gecko", "%s", msg.get());
#endif
      fputs(msg.get(), stderr);
      fflush(stderr);
    }

    return true;
  }
};

class TimerRunnable : public WorkerRunnable
{
public:
  TimerRunnable(WorkerPrivate* aWorkerPrivate)
  : WorkerRunnable(aWorkerPrivate, WorkerThread, UnchangedBusyCount,
                   SkipWhenClearing)
  { }

  bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    // Silence bad assertions.
    return true;
  }

  void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult)
  {
    // Silence bad assertions.
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    return aWorkerPrivate->RunExpiredTimeouts(aCx);
  }
};

void
DummyCallback(nsITimer* aTimer, void* aClosure)
{
  // Nothing!
}

class WorkerRunnableEventTarget MOZ_FINAL : public nsIEventTarget
{
protected:
  nsRefPtr<WorkerRunnable> mWorkerRunnable;

public:
  WorkerRunnableEventTarget(WorkerRunnable* aWorkerRunnable)
  : mWorkerRunnable(aWorkerRunnable)
  { }

  NS_DECL_ISUPPORTS

  NS_IMETHOD
  Dispatch(nsIRunnable* aRunnable, uint32_t aFlags)
  {
    NS_ASSERTION(aFlags == nsIEventTarget::DISPATCH_NORMAL, "Don't call me!");

    nsRefPtr<WorkerRunnableEventTarget> kungFuDeathGrip = this;

    // Run the runnable we're given now (should just call DummyCallback()),
    // otherwise the timer thread will leak it...  If we run this after
    // dispatch running the event can race against resetting the timer.
    aRunnable->Run();

    // This can fail if we're racing to terminate or cancel, should be handled
    // by the terminate or cancel code.
    mWorkerRunnable->Dispatch(nullptr);

    return NS_OK;
  }

  NS_IMETHOD
  IsOnCurrentThread(bool* aIsOnCurrentThread)
  {
    *aIsOnCurrentThread = false;
    return NS_OK;
  }
};

NS_IMPL_THREADSAFE_ISUPPORTS1(WorkerRunnableEventTarget, nsIEventTarget)

class KillCloseEventRunnable : public WorkerRunnable
{
  nsCOMPtr<nsITimer> mTimer;

  class KillScriptRunnable : public WorkerControlRunnable
  {
  public:
    KillScriptRunnable(WorkerPrivate* aWorkerPrivate)
    : WorkerControlRunnable(aWorkerPrivate, WorkerThread, UnchangedBusyCount)
    { }

    bool
    PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
    {
      // Silence bad assertions.
      return true;
    }

    void
    PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                 bool aDispatchResult)
    {
      // Silence bad assertions.
    }

    bool
    WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
    {
      // Kill running script.
      return false;
    }
  };

public:
  KillCloseEventRunnable(WorkerPrivate* aWorkerPrivate)
  : WorkerRunnable(aWorkerPrivate, WorkerThread, UnchangedBusyCount,
                   SkipWhenClearing)
  { }

  ~KillCloseEventRunnable()
  {
    if (mTimer) {
      mTimer->Cancel();
    }
  }

  bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    NS_NOTREACHED("Not meant to be dispatched!");
    return false;
  }

  bool
  SetTimeout(JSContext* aCx, uint32_t aDelayMS)
  {
    nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID);
    if (!timer) {
      JS_ReportError(aCx, "Failed to create timer!");
      return false;
    }

    nsRefPtr<KillScriptRunnable> runnable =
      new KillScriptRunnable(mWorkerPrivate);

    nsRefPtr<WorkerRunnableEventTarget> target =
      new WorkerRunnableEventTarget(runnable);

    if (NS_FAILED(timer->SetTarget(target))) {
      JS_ReportError(aCx, "Failed to set timer's target!");
      return false;
    }

    if (NS_FAILED(timer->InitWithFuncCallback(DummyCallback, nullptr, aDelayMS,
                                              nsITimer::TYPE_ONE_SHOT))) {
      JS_ReportError(aCx, "Failed to start timer!");
      return false;
    }

    mTimer.swap(timer);
    return true;
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    if (mTimer) {
      mTimer->Cancel();
      mTimer = nullptr;
    }

    return true;
  }
};

class UpdateJSContextOptionsRunnable : public WorkerControlRunnable
{
  uint32_t mOptions;

public:
  UpdateJSContextOptionsRunnable(WorkerPrivate* aWorkerPrivate,
                                 uint32_t aOptions)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThread, UnchangedBusyCount),
    mOptions(aOptions)
  { }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    aWorkerPrivate->UpdateJSContextOptionsInternal(aCx, mOptions);
    return true;
  }
};

class UpdateJSRuntimeHeapSizeRunnable : public WorkerControlRunnable
{
  uint32_t mJSRuntimeHeapSize;

public:
  UpdateJSRuntimeHeapSizeRunnable(WorkerPrivate* aWorkerPrivate,
                                  uint32_t aJSRuntimeHeapSize)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThread, UnchangedBusyCount),
    mJSRuntimeHeapSize(aJSRuntimeHeapSize)
  { }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    aWorkerPrivate->UpdateJSRuntimeHeapSizeInternal(aCx, mJSRuntimeHeapSize);
    return true;
  }
};

#ifdef JS_GC_ZEAL
class UpdateGCZealRunnable : public WorkerControlRunnable
{
  uint8_t mGCZeal;

public:
  UpdateGCZealRunnable(WorkerPrivate* aWorkerPrivate,
                       uint8_t aGCZeal)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThread, UnchangedBusyCount),
    mGCZeal(aGCZeal)
  { }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    aWorkerPrivate->UpdateGCZealInternal(aCx, mGCZeal);
    return true;
  }
};
#endif

class GarbageCollectRunnable : public WorkerControlRunnable
{
protected:
  bool mShrinking;
  bool mCollectChildren;

public:
  GarbageCollectRunnable(WorkerPrivate* aWorkerPrivate, bool aShrinking,
                         bool aCollectChildren)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThread, UnchangedBusyCount),
    mShrinking(aShrinking), mCollectChildren(aCollectChildren)
  { }

  bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    // Silence bad assertions, this can be dispatched from either the main
    // thread or the timer thread..
    return true;
  }

  void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                bool aDispatchResult)
  {
    // Silence bad assertions, this can be dispatched from either the main
    // thread or the timer thread..
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    aWorkerPrivate->GarbageCollectInternal(aCx, mShrinking, mCollectChildren);
    return true;
  }
};

class CollectRuntimeStatsRunnable : public WorkerControlRunnable
{
  typedef mozilla::Mutex Mutex;
  typedef mozilla::CondVar CondVar;

  Mutex mMutex;
  CondVar mCondVar;
  volatile bool mDone;
  bool mIsQuick;
  void* mData;
  bool* mSucceeded;

public:
  CollectRuntimeStatsRunnable(WorkerPrivate* aWorkerPrivate, bool aIsQuick,
                              void* aData, bool* aSucceeded)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThread, UnchangedBusyCount),
    mMutex("CollectRuntimeStatsRunnable::mMutex"),
    mCondVar(mMutex, "CollectRuntimeStatsRunnable::mCondVar"), mDone(false),
    mIsQuick(aIsQuick), mData(aData), mSucceeded(aSucceeded)
  { }

  bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    AssertIsOnMainThread();
    return true;
  }

  void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult)
  {
    AssertIsOnMainThread();
  }

  bool
  DispatchInternal()
  {
    AssertIsOnMainThread();

    if (!WorkerControlRunnable::DispatchInternal()) {
      NS_WARNING("Failed to dispatch runnable!");
      return false;
    }

    {
      MutexAutoLock lock(mMutex);
      while (!mDone) {
        mCondVar.Wait();
      }
    }

    return true;
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    JSRuntime *rt = JS_GetRuntime(aCx);
    if (mIsQuick) {
      *static_cast<int64_t*>(mData) = JS::GetExplicitNonHeapForRuntime(rt, JsWorkerMallocSizeOf);
      *mSucceeded = true;
    } else {
      *mSucceeded = JS::CollectRuntimeStats(rt, static_cast<JS::RuntimeStats*>(mData), nullptr);
    }

    {
      MutexAutoLock lock(mMutex);
      mDone = true;
      mCondVar.Notify();
    }

    return true;
  }
};

} /* anonymous namespace */

struct WorkerJSRuntimeStats : public JS::RuntimeStats
{
  WorkerJSRuntimeStats(nsACString &aRtPath)
   : JS::RuntimeStats(JsWorkerMallocSizeOf), mRtPath(aRtPath) { }

  ~WorkerJSRuntimeStats() {
    for (size_t i = 0; i != compartmentStatsVector.length(); i++) {
      free(compartmentStatsVector[i].extra1);
      // no need to free |extra2|, because it's a static string
    }
  }

  virtual void initExtraCompartmentStats(JSCompartment *c,
                                         JS::CompartmentStats *cstats) MOZ_OVERRIDE
  {
    MOZ_ASSERT(!cstats->extra1);
    MOZ_ASSERT(!cstats->extra2);

    // ReportJSRuntimeExplicitTreeStats expects that cstats->{extra1,extra2}
    // are char pointers.

    // This is the |cJSPathPrefix|.  Each worker has exactly two compartments:
    // one for atoms, and one for everything else.
    nsCString cJSPathPrefix(mRtPath);
    cJSPathPrefix += js::IsAtomsCompartment(c)
                   ? NS_LITERAL_CSTRING("compartment(web-worker-atoms)/")
                   : NS_LITERAL_CSTRING("compartment(web-worker)/");
    cstats->extra1 = strdup(cJSPathPrefix.get());

    // This is the |cDOMPathPrefix|, which should never be used when reporting
    // with workers (hence the "?!").
    cstats->extra2 = (void *)"explicit/workers/?!/";
  }

private:
  nsCString mRtPath;
};

BEGIN_WORKERS_NAMESPACE

class WorkerMemoryReporter MOZ_FINAL : public nsIMemoryMultiReporter
{
  WorkerPrivate* mWorkerPrivate;
  nsCString mAddressString;
  nsCString mRtPath;

public:
  NS_DECL_ISUPPORTS

  WorkerMemoryReporter(WorkerPrivate* aWorkerPrivate)
  : mWorkerPrivate(aWorkerPrivate)
  {
    aWorkerPrivate->AssertIsOnWorkerThread();

    nsCString escapedDomain(aWorkerPrivate->Domain());
    escapedDomain.ReplaceChar('/', '\\');

    NS_ConvertUTF16toUTF8 escapedURL(aWorkerPrivate->ScriptURL());
    escapedURL.ReplaceChar('/', '\\');

    {
      // 64bit address plus '0x' plus null terminator.
      char address[21];
      uint32_t addressSize =
        JS_snprintf(address, sizeof(address), "%p", aWorkerPrivate);
      if (addressSize != uint32_t(-1)) {
        mAddressString.Assign(address, addressSize);
      }
      else {
        NS_WARNING("JS_snprintf failed!");
        mAddressString.AssignLiteral("<unknown address>");
      }
    }

    mRtPath = NS_LITERAL_CSTRING("explicit/workers/workers(") +
              escapedDomain + NS_LITERAL_CSTRING(")/worker(") +
              escapedURL + NS_LITERAL_CSTRING(", ") + mAddressString +
              NS_LITERAL_CSTRING(")/");
  }

  nsresult
  CollectForRuntime(bool aIsQuick, void* aData)
  {
    AssertIsOnMainThread();

    if (!mWorkerPrivate) {
#ifdef DEBUG
      nsAutoCString message("Unable to report memory for ");
      message += NS_LITERAL_CSTRING("Worker (") + mAddressString +
                 NS_LITERAL_CSTRING(")! It is either using ctypes or is in "
                                    "the process of being destroyed");
      NS_WARNING(message.get());
#endif
      *static_cast<int64_t*>(aData) = 0;
      return NS_OK;
    }

    if (!mWorkerPrivate->BlockAndCollectRuntimeStats(aIsQuick, aData)) {
      return NS_ERROR_FAILURE;
    }

    return NS_OK;
  }

  NS_IMETHOD GetName(nsACString &aName)
  {
    aName.AssignLiteral("workers");
    return NS_OK;
  }

  NS_IMETHOD
  CollectReports(nsIMemoryMultiReporterCallback* aCallback,
                 nsISupports* aClosure)
  {
    AssertIsOnMainThread();

    WorkerJSRuntimeStats rtStats(mRtPath);
    nsresult rv = CollectForRuntime(/* isQuick = */false, &rtStats);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Always report, even if we're disabled, so that we at least get an entry
    // in about::memory.
    return xpc::ReportJSRuntimeExplicitTreeStats(rtStats, mRtPath,
                                                 aCallback, aClosure);
  }

  NS_IMETHOD
  GetExplicitNonHeap(int64_t *aAmount)
  {
    AssertIsOnMainThread();

    return CollectForRuntime(/* isQuick = */true, aAmount);
  }

  void Disable()
  {
#ifdef DEBUG
    // Setting mWorkerPrivate to nullptr is safe only because we've locked the
    // worker's mutex on the worker's thread, in the caller.  So we check that.
    //
    // Also, we may have already disabled the reporter (and thus set
    // mWorkerPrivate to nullptr) due to the use of CTypes (see
    // ChromeWorkerScope.cpp).  That's why the NULL check is necessary.
    if (mWorkerPrivate) {
        mWorkerPrivate->mMutex.AssertCurrentThreadOwns();
    }
#endif
    mWorkerPrivate = nullptr;
  }
};

END_WORKERS_NAMESPACE

NS_IMPL_THREADSAFE_ISUPPORTS1(WorkerMemoryReporter, nsIMemoryMultiReporter)

#ifdef DEBUG
void
mozilla::dom::workers::AssertIsOnMainThread()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

WorkerRunnable::WorkerRunnable(WorkerPrivate* aWorkerPrivate, Target aTarget,
                               BusyBehavior aBusyBehavior,
                               ClearingBehavior aClearingBehavior)
: mWorkerPrivate(aWorkerPrivate), mTarget(aTarget),
  mBusyBehavior(aBusyBehavior), mClearingBehavior(aClearingBehavior)
{
  NS_ASSERTION(aWorkerPrivate, "Null worker private!");
}
#endif

NS_IMPL_THREADSAFE_ISUPPORTS1(WorkerRunnable, nsIRunnable)

bool
WorkerRunnable::PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
{
#ifdef DEBUG
  if (mBusyBehavior == ModifyBusyCount) {
    NS_ASSERTION(mTarget == WorkerThread,
                 "Don't set this option unless targeting the worker thread!");
  }
  if (mTarget == ParentThread) {
    aWorkerPrivate->AssertIsOnWorkerThread();
  }
  else {
    aWorkerPrivate->AssertIsOnParentThread();
  }
#endif

  if (mBusyBehavior == ModifyBusyCount && aCx) {
    return aWorkerPrivate->ModifyBusyCount(aCx, true);
  }

  return true;
}

bool
WorkerRunnable::Dispatch(JSContext* aCx)
{
  bool ok;

  if (!aCx) {
    ok = PreDispatch(nullptr, mWorkerPrivate);
    if (ok) {
      ok = DispatchInternal();
    }
    PostDispatch(nullptr, mWorkerPrivate, ok);
    return ok;
  }

  JSAutoRequest ar(aCx);

  JSObject* global = JS_GetGlobalObject(aCx);

  Maybe<JSAutoCompartment> ac;
  if (global) {
    ac.construct(aCx, global);
  }

  ok = PreDispatch(aCx, mWorkerPrivate);

  if (ok && !DispatchInternal()) {
    ok = false;
  }

  PostDispatch(aCx, mWorkerPrivate, ok);

  return ok;
}

// static
bool
WorkerRunnable::DispatchToMainThread(nsIRunnable* aRunnable)
{
  nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
  NS_ASSERTION(mainThread, "This should never fail!");

  return NS_SUCCEEDED(mainThread->Dispatch(aRunnable, NS_DISPATCH_NORMAL));
}

// These DispatchInternal functions look identical but carry important type
// informaton so they can't be consolidated...

#define IMPL_DISPATCH_INTERNAL(_class)                                         \
  bool                                                                         \
  _class ::DispatchInternal()                                                  \
  {                                                                            \
    if (mTarget == WorkerThread) {                                             \
      return mWorkerPrivate->Dispatch(this);                                   \
    }                                                                          \
                                                                               \
    if (mWorkerPrivate->GetParent()) {                                         \
      return mWorkerPrivate->GetParent()->Dispatch(this);                      \
    }                                                                          \
                                                                               \
    return DispatchToMainThread(this);                                         \
  }

IMPL_DISPATCH_INTERNAL(WorkerRunnable)
IMPL_DISPATCH_INTERNAL(WorkerSyncRunnable)
IMPL_DISPATCH_INTERNAL(WorkerControlRunnable)

#undef IMPL_DISPATCH_INTERNAL

void
WorkerRunnable::PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                             bool aDispatchResult)
{
#ifdef DEBUG
  if (mTarget == ParentThread) {
    aWorkerPrivate->AssertIsOnWorkerThread();
  }
  else {
    aWorkerPrivate->AssertIsOnParentThread();
  }
#endif

  if (!aDispatchResult && aCx) {
    if (mBusyBehavior == ModifyBusyCount) {
      aWorkerPrivate->ModifyBusyCount(aCx, false);
    }
    JS_ReportPendingException(aCx);
  }
}

NS_IMETHODIMP
WorkerRunnable::Run()
{
  JSContext* cx;
  JSObject* targetCompartmentObject;
  nsIThreadJSContextStack* contextStack = nullptr;

  nsRefPtr<WorkerPrivate> kungFuDeathGrip;

  if (mTarget == WorkerThread) {
    mWorkerPrivate->AssertIsOnWorkerThread();
    cx = mWorkerPrivate->GetJSContext();
    targetCompartmentObject = JS_GetGlobalObject(cx);
  } else {
    kungFuDeathGrip = mWorkerPrivate;
    mWorkerPrivate->AssertIsOnParentThread();
    cx = mWorkerPrivate->ParentJSContext();
    targetCompartmentObject = mWorkerPrivate->GetJSObject();

    if (!mWorkerPrivate->GetParent()) {
      AssertIsOnMainThread();

      contextStack = nsContentUtils::ThreadJSContextStack();
      NS_ASSERTION(contextStack, "This should never be null!");

      if (NS_FAILED(contextStack->Push(cx))) {
        NS_WARNING("Failed to push context!");
        contextStack = nullptr;
      }
    }
  }

  NS_ASSERTION(cx, "Must have a context!");

  JSAutoRequest ar(cx);

  Maybe<JSAutoCompartment> ac;
  if (targetCompartmentObject) {
    ac.construct(cx, targetCompartmentObject);
  }

  bool result = WorkerRun(cx, mWorkerPrivate);

  PostRun(cx, mWorkerPrivate, result);

  if (contextStack) {
    JSContext* otherCx;
    if (NS_FAILED(contextStack->Pop(&otherCx))) {
      NS_WARNING("Failed to pop context!");
    }
    else if (otherCx != cx) {
      NS_WARNING("Popped a different context!");
    }
  }

  return result ? NS_OK : NS_ERROR_FAILURE;
}

void
WorkerRunnable::PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                        bool aRunResult)
{
#ifdef DEBUG
  if (mTarget == ParentThread) {
    mWorkerPrivate->AssertIsOnParentThread();
  }
  else {
    mWorkerPrivate->AssertIsOnWorkerThread();
  }
#endif

  if (mBusyBehavior == ModifyBusyCount) {
    if (!aWorkerPrivate->ModifyBusyCountFromWorker(aCx, false)) {
      aRunResult = false;
    }
  }

  if (!aRunResult) {
    JS_ReportPendingException(aCx);
  }
}

void
WorkerRunnable::NotifyScriptExecutedIfNeeded() const
{
  // if we're on the main thread notify about the end of our script execution.
  if (mTarget == ParentThread && !mWorkerPrivate->GetParent()) {
    AssertIsOnMainThread();
    if (mWorkerPrivate->GetScriptNotify()) {
      mWorkerPrivate->GetScriptNotify()->ScriptExecuted();
    }
  }
}

struct WorkerPrivate::TimeoutInfo
{
  TimeoutInfo()
  : mTimeoutVal(JS::UndefinedValue()), mLineNumber(0), mId(0), mIsInterval(false),
    mCanceled(false)
  {
    MOZ_COUNT_CTOR(mozilla::dom::workers::WorkerPrivate::TimeoutInfo);
  }

  ~TimeoutInfo()
  {
    MOZ_COUNT_DTOR(mozilla::dom::workers::WorkerPrivate::TimeoutInfo);
  }

  bool operator==(const TimeoutInfo& aOther)
  {
    return mTargetTime == aOther.mTargetTime;
  }

  bool operator<(const TimeoutInfo& aOther)
  {
    return mTargetTime < aOther.mTargetTime;
  }

  JS::Value mTimeoutVal;
  nsTArray<jsval> mExtraArgVals;
  mozilla::TimeStamp mTargetTime;
  mozilla::TimeDuration mInterval;
  nsCString mFilename;
  uint32_t mLineNumber;
  uint32_t mId;
  bool mIsInterval;
  bool mCanceled;
};

template <class Derived>
WorkerPrivateParent<Derived>::WorkerPrivateParent(
                                     JSContext* aCx, JSObject* aObject,
                                     WorkerPrivate* aParent,
                                     JSContext* aParentJSContext,
                                     const nsAString& aScriptURL,
                                     bool aIsChromeWorker,
                                     const nsACString& aDomain,
                                     nsCOMPtr<nsPIDOMWindow>& aWindow,
                                     nsCOMPtr<nsIScriptContext>& aScriptContext,
                                     nsCOMPtr<nsIURI>& aBaseURI,
                                     nsCOMPtr<nsIPrincipal>& aPrincipal,
                                     nsCOMPtr<nsIContentSecurityPolicy>& aCSP,
                                     bool aEvalAllowed)
: EventTarget(aParent ? aCx : NULL), mMutex("WorkerPrivateParent Mutex"),
  mCondVar(mMutex, "WorkerPrivateParent CondVar"),
  mJSObject(aObject), mParent(aParent), mParentJSContext(aParentJSContext),
  mScriptURL(aScriptURL), mDomain(aDomain), mBusyCount(0),
  mParentStatus(Pending), mJSContextOptions(0), mJSRuntimeHeapSize(0),
  mGCZeal(0), mJSObjectRooted(false), mParentSuspended(false),
  mIsChromeWorker(aIsChromeWorker), mPrincipalIsSystem(false),
  mMainThreadObjectsForgotten(false), mEvalAllowed(aEvalAllowed)
{
  MOZ_COUNT_CTOR(mozilla::dom::workers::WorkerPrivateParent);

  if (aWindow) {
    NS_ASSERTION(aWindow->IsInnerWindow(), "Should have inner window here!");
  }

  mWindow.swap(aWindow);
  mScriptContext.swap(aScriptContext);
  mScriptNotify = do_QueryInterface(mScriptContext);
  mBaseURI.swap(aBaseURI);
  mPrincipal.swap(aPrincipal);
  mCSP.swap(aCSP);

  if (aParent) {
    aParent->AssertIsOnWorkerThread();

    NS_ASSERTION(JS_GetOptions(aCx) == aParent->GetJSContextOptions(),
                 "Options mismatch!");
    mJSContextOptions = aParent->GetJSContextOptions();

    NS_ASSERTION(JS_GetGCParameter(JS_GetRuntime(aCx), JSGC_MAX_BYTES) ==
                 aParent->GetJSRuntimeHeapSize(),
                 "Runtime heap size mismatch!");
    mJSRuntimeHeapSize = aParent->GetJSRuntimeHeapSize();

#ifdef JS_GC_ZEAL
    mGCZeal = aParent->GetGCZeal();
#endif
  }
  else {
    AssertIsOnMainThread();

    mJSContextOptions = RuntimeService::GetDefaultJSContextOptions();
    mJSRuntimeHeapSize = RuntimeService::GetDefaultJSRuntimeHeapSize();
#ifdef JS_GC_ZEAL
    mGCZeal = RuntimeService::GetDefaultGCZeal();
#endif
  }
}

template <class Derived>
WorkerPrivateParent<Derived>::~WorkerPrivateParent()
{
  MOZ_COUNT_DTOR(mozilla::dom::workers::WorkerPrivateParent);
}

template <class Derived>
bool
WorkerPrivateParent<Derived>::Start()
{
  // May be called on any thread!
  {
    MutexAutoLock lock(mMutex);

    NS_ASSERTION(mParentStatus != Running, "How can this be?!");

    if (mParentStatus == Pending) {
      mParentStatus = Running;
      return true;
    }
  }

  return false;
}

// aCx is null when called from the finalizer
template <class Derived>
bool
WorkerPrivateParent<Derived>::NotifyPrivate(JSContext* aCx, Status aStatus)
{
  AssertIsOnParentThread();

  bool pending;
  {
    MutexAutoLock lock(mMutex);

    if (mParentStatus >= aStatus) {
      return true;
    }

    pending = mParentStatus == Pending;
    mParentStatus = aStatus;
  }

  if (pending) {
    WorkerPrivate* self = ParentAsWorkerPrivate();
#ifdef DEBUG
    {
      // Silence useless assertions in debug builds.
      nsIThread* currentThread = NS_GetCurrentThread();
      NS_ASSERTION(currentThread, "This should never be null!");

      self->SetThread(currentThread);
    }
#endif
    // Worker never got a chance to run, go ahead and delete it.
    self->ScheduleDeletion(true);
    return true;
  }

  NS_ASSERTION(aStatus != Terminating || mQueuedRunnables.IsEmpty(),
               "Shouldn't have anything queued!");

  // Anything queued will be discarded.
  mQueuedRunnables.Clear();

  nsRefPtr<NotifyRunnable> runnable =
    new NotifyRunnable(ParentAsWorkerPrivate(), aStatus);
  return runnable->Dispatch(aCx);
}

template <class Derived>
bool
WorkerPrivateParent<Derived>::Suspend(JSContext* aCx)
{
  AssertIsOnParentThread();
  NS_ASSERTION(!mParentSuspended, "Suspended more than once!");

  mParentSuspended = true;

  {
    MutexAutoLock lock(mMutex);

    if (mParentStatus >= Terminating) {
      return true;
    }
  }

  nsRefPtr<SuspendRunnable> runnable =
    new SuspendRunnable(ParentAsWorkerPrivate());
  return runnable->Dispatch(aCx);
}

template <class Derived>
bool
WorkerPrivateParent<Derived>::Resume(JSContext* aCx)
{
  AssertIsOnParentThread();
  NS_ASSERTION(mParentSuspended, "Not yet suspended!");

  mParentSuspended = false;

  {
    MutexAutoLock lock(mMutex);

    if (mParentStatus >= Terminating) {
      return true;
    }
  }

  // Dispatch queued runnables before waking up the worker, otherwise the worker
  // could post new messages before we run those that have been queued.
  if (!mQueuedRunnables.IsEmpty()) {
    AssertIsOnMainThread();

    nsTArray<nsRefPtr<WorkerRunnable> > runnables;
    mQueuedRunnables.SwapElements(runnables);

    for (uint32_t index = 0; index < runnables.Length(); index++) {
      nsRefPtr<WorkerRunnable>& runnable = runnables[index];
      if (NS_FAILED(NS_DispatchToCurrentThread(runnable))) {
        NS_WARNING("Failed to dispatch queued runnable!");
      }
    }
  }

  nsRefPtr<ResumeRunnable> runnable =
    new ResumeRunnable(ParentAsWorkerPrivate());
  if (!runnable->Dispatch(aCx)) {
    return false;
  }

  return true;
}

template <class Derived>
void
WorkerPrivateParent<Derived>::_trace(JSTracer* aTrc)
{
  // This should only happen on the parent thread but we can't assert that
  // because it can also happen on the cycle collector thread when this is a
  // top-level worker.
  EventTarget::_trace(aTrc);
}

template <class Derived>
void
WorkerPrivateParent<Derived>::_finalize(JSFreeOp* aFop)
{
  AssertIsOnParentThread();

  MOZ_ASSERT(mJSObject);
  MOZ_ASSERT(!mJSObjectRooted);

  // Clear the JS object.
  mJSObject = nullptr;

  if (!TerminatePrivate(nullptr)) {
    NS_WARNING("Failed to terminate!");
  }

  // Before calling through to the base class we need to grab another reference
  // if we're on the main thread. Otherwise the base class' _Finalize method
  // will call Release, and some of our members cannot be released during
  // finalization. Of course, if those members are already gone then we can skip
  // this mess...
  WorkerPrivateParent<Derived>* extraSelfRef = NULL;

  if (!mParent && !mMainThreadObjectsForgotten) {
    AssertIsOnMainThread();
    NS_ADDREF(extraSelfRef = this);
  }

  EventTarget::_finalize(aFop);

  if (extraSelfRef) {
    nsCOMPtr<nsIRunnable> runnable =
      NS_NewNonOwningRunnableMethod(extraSelfRef,
                                    &WorkerPrivateParent<Derived>::Release);
    if (NS_FAILED(NS_DispatchToCurrentThread(runnable))) {
      NS_WARNING("Failed to proxy release, this will leak!");
    }
  }
}

template <class Derived>
bool
WorkerPrivateParent<Derived>::Close(JSContext* aCx)
{
  AssertIsOnParentThread();

  {
    MutexAutoLock lock(mMutex);

    if (mParentStatus < Closing) {
      mParentStatus = Closing;
    }
  }

  return true;
}

template <class Derived>
bool
WorkerPrivateParent<Derived>::ModifyBusyCount(JSContext* aCx, bool aIncrease)
{
  AssertIsOnParentThread();

  NS_ASSERTION(aIncrease || mBusyCount, "Mismatched busy count mods!");

  if (aIncrease) {
    if (mBusyCount++ == 0 && mJSObject) {
      if (!RootJSObject(aCx, true)) {
        return false;
      }
    }
    return true;
  }

  if (--mBusyCount == 0 && mJSObject) {
    if (!RootJSObject(aCx, false)) {
      return false;
    }

    bool shouldCancel;
    {
      MutexAutoLock lock(mMutex);
      shouldCancel = mParentStatus == Terminating;
    }

    if (shouldCancel && !Cancel(aCx)) {
      return false;
    }
  }

  return true;
}

template <class Derived>
bool
WorkerPrivateParent<Derived>::RootJSObject(JSContext* aCx, bool aRoot)
{
  AssertIsOnParentThread();

  if (aRoot != mJSObjectRooted) {
    if (aRoot) {
      NS_ASSERTION(mJSObject, "Nothing to root?");
      if (!JS_AddNamedObjectRoot(aCx, &mJSObject, "Worker root")) {
        NS_WARNING("JS_AddNamedObjectRoot failed!");
        return false;
      }
    }
    else {
      JS_RemoveObjectRoot(aCx, &mJSObject);
    }

    mJSObjectRooted = aRoot;
  }

  return true;
}

template <class Derived>
void
WorkerPrivateParent<Derived>::ForgetMainThreadObjects(
                                      nsTArray<nsCOMPtr<nsISupports> >& aDoomed)
{
  AssertIsOnParentThread();
  MOZ_ASSERT(!mMainThreadObjectsForgotten);

  aDoomed.SetCapacity(7);

  SwapToISupportsArray(mWindow, aDoomed);
  SwapToISupportsArray(mScriptContext, aDoomed);
  SwapToISupportsArray(mScriptNotify, aDoomed);
  SwapToISupportsArray(mBaseURI, aDoomed);
  SwapToISupportsArray(mScriptURI, aDoomed);
  SwapToISupportsArray(mPrincipal, aDoomed);
  SwapToISupportsArray(mCSP, aDoomed);

  mMainThreadObjectsForgotten = true;
}

template <class Derived>
bool
WorkerPrivateParent<Derived>::PostMessage(JSContext* aCx, jsval aMessage,
                                          jsval aTransferable)
{
  AssertIsOnParentThread();

  {
    MutexAutoLock lock(mMutex);
    if (mParentStatus > Running) {
      return true;
    }
  }

  JSStructuredCloneCallbacks* callbacks;
  if (GetParent()) {
    if (IsChromeWorker()) {
      callbacks = &gChromeWorkerStructuredCloneCallbacks;
    }
    else {
      callbacks = &gWorkerStructuredCloneCallbacks;
    }
  }
  else {
    AssertIsOnMainThread();

    if (IsChromeWorker()) {
      callbacks = &gMainThreadChromeWorkerStructuredCloneCallbacks;
    }
    else {
      callbacks = &gMainThreadWorkerStructuredCloneCallbacks;
    }
  }

  nsTArray<nsCOMPtr<nsISupports> > clonedObjects;

  JSAutoStructuredCloneBuffer buffer;
  if (!buffer.write(aCx, aMessage, aTransferable, callbacks, &clonedObjects)) {
    return false;
  }

  nsRefPtr<MessageEventRunnable> runnable =
    new MessageEventRunnable(ParentAsWorkerPrivate(),
                             WorkerRunnable::WorkerThread, buffer,
                             clonedObjects);
  return runnable->Dispatch(aCx);
}

template <class Derived>
uint64_t
WorkerPrivateParent<Derived>::GetInnerWindowId()
{
  AssertIsOnMainThread();
  NS_ASSERTION(!mWindow || mWindow->IsInnerWindow(), "Outer window?");
  return mWindow ? mWindow->WindowID() : 0;
}

template <class Derived>
void
WorkerPrivateParent<Derived>::UpdateJSContextOptions(JSContext* aCx,
                                                     uint32_t aOptions)
{
  AssertIsOnParentThread();

  mJSContextOptions = aOptions;

  nsRefPtr<UpdateJSContextOptionsRunnable> runnable =
    new UpdateJSContextOptionsRunnable(ParentAsWorkerPrivate(), aOptions);
  if (!runnable->Dispatch(aCx)) {
    NS_WARNING("Failed to update worker context options!");
    JS_ClearPendingException(aCx);
  }
}

template <class Derived>
void
WorkerPrivateParent<Derived>::UpdateJSRuntimeHeapSize(JSContext* aCx,
                                                      uint32_t aMaxBytes)
{
  AssertIsOnParentThread();

  mJSRuntimeHeapSize = aMaxBytes;

  nsRefPtr<UpdateJSRuntimeHeapSizeRunnable> runnable =
    new UpdateJSRuntimeHeapSizeRunnable(ParentAsWorkerPrivate(), aMaxBytes);
  if (!runnable->Dispatch(aCx)) {
    NS_WARNING("Failed to update worker heap size!");
    JS_ClearPendingException(aCx);
  }
}

#ifdef JS_GC_ZEAL
template <class Derived>
void
WorkerPrivateParent<Derived>::UpdateGCZeal(JSContext* aCx, uint8_t aGCZeal)
{
  AssertIsOnParentThread();

  mGCZeal = aGCZeal;

  nsRefPtr<UpdateGCZealRunnable> runnable =
    new UpdateGCZealRunnable(ParentAsWorkerPrivate(), aGCZeal);
  if (!runnable->Dispatch(aCx)) {
    NS_WARNING("Failed to update worker gczeal!");
    JS_ClearPendingException(aCx);
  }
}
#endif

template <class Derived>
void
WorkerPrivateParent<Derived>::GarbageCollect(JSContext* aCx, bool aShrinking)
{
  nsRefPtr<GarbageCollectRunnable> runnable =
    new GarbageCollectRunnable(ParentAsWorkerPrivate(), aShrinking, true);
  if (!runnable->Dispatch(aCx)) {
    NS_WARNING("Failed to update worker heap size!");
    JS_ClearPendingException(aCx);
  }
}

template <class Derived>
void
WorkerPrivateParent<Derived>::SetBaseURI(nsIURI* aBaseURI)
{
  AssertIsOnMainThread();

  mBaseURI = aBaseURI;

  if (NS_FAILED(aBaseURI->GetSpec(mLocationInfo.mHref))) {
    mLocationInfo.mHref.Truncate();
  }

  if (NS_FAILED(aBaseURI->GetHost(mLocationInfo.mHostname))) {
    mLocationInfo.mHostname.Truncate();
  }

  if (NS_FAILED(aBaseURI->GetPath(mLocationInfo.mPathname))) {
    mLocationInfo.mPathname.Truncate();
  }

  nsCString temp;

  nsCOMPtr<nsIURL> url(do_QueryInterface(aBaseURI));
  if (url && NS_SUCCEEDED(url->GetQuery(temp)) && !temp.IsEmpty()) {
    mLocationInfo.mSearch.AssignLiteral("?");
    mLocationInfo.mSearch.Append(temp);
  }

  if (NS_SUCCEEDED(aBaseURI->GetRef(temp)) && !temp.IsEmpty()) {
    nsCOMPtr<nsITextToSubURI> converter =
      do_GetService(NS_ITEXTTOSUBURI_CONTRACTID);
    if (converter) {
      nsCString charset;
      nsAutoString unicodeRef;
      if (NS_SUCCEEDED(aBaseURI->GetOriginCharset(charset)) &&
          NS_SUCCEEDED(converter->UnEscapeURIForUI(charset, temp,
                                                   unicodeRef))) {
        mLocationInfo.mHash.AssignLiteral("#");
        mLocationInfo.mHash.Append(NS_ConvertUTF16toUTF8(unicodeRef));
      }
    }

    if (mLocationInfo.mHash.IsEmpty()) {
      mLocationInfo.mHash.AssignLiteral("#");
      mLocationInfo.mHash.Append(temp);
    }
  }

  if (NS_SUCCEEDED(aBaseURI->GetScheme(mLocationInfo.mProtocol))) {
    mLocationInfo.mProtocol.AppendLiteral(":");
  }
  else {
    mLocationInfo.mProtocol.Truncate();
  }

  int32_t port;
  if (NS_SUCCEEDED(aBaseURI->GetPort(&port)) && port != -1) {
    mLocationInfo.mPort.AppendInt(port);

    nsAutoCString host(mLocationInfo.mHostname);
    host.AppendLiteral(":");
    host.Append(mLocationInfo.mPort);

    mLocationInfo.mHost.Assign(host);
  }
  else {
    mLocationInfo.mHost.Assign(mLocationInfo.mHostname);
  }
}

template <class Derived>
void
WorkerPrivateParent<Derived>::SetPrincipal(nsIPrincipal* aPrincipal)
{
  AssertIsOnMainThread();

  mPrincipal = aPrincipal;
  mPrincipalIsSystem = nsContentUtils::IsSystemPrincipal(aPrincipal);
}

template <class Derived>
JSContext*
WorkerPrivateParent<Derived>::ParentJSContext() const
{
  AssertIsOnParentThread();

  if (!mParent) {
    AssertIsOnMainThread();

    if (!mScriptContext) {
      NS_ASSERTION(!mParentJSContext, "Shouldn't have a parent context!");
      return RuntimeService::AutoSafeJSContext::GetSafeContext();
    }

    NS_ASSERTION(mParentJSContext == mScriptContext->GetNativeContext(),
                 "Native context has changed!");
  }

  return mParentJSContext;
}

WorkerPrivate::WorkerPrivate(JSContext* aCx, JSObject* aObject,
                             WorkerPrivate* aParent,
                             JSContext* aParentJSContext,
                             const nsAString& aScriptURL, bool aIsChromeWorker,
                             const nsACString& aDomain,
                             nsCOMPtr<nsPIDOMWindow>& aWindow,
                             nsCOMPtr<nsIScriptContext>& aParentScriptContext,
                             nsCOMPtr<nsIURI>& aBaseURI,
                             nsCOMPtr<nsIPrincipal>& aPrincipal,
                             nsCOMPtr<nsIContentSecurityPolicy>& aCSP,
                             bool aEvalAllowed,
                             bool aXHRParamsAllowed)
: WorkerPrivateParent<WorkerPrivate>(aCx, aObject, aParent, aParentJSContext,
                                     aScriptURL, aIsChromeWorker, aDomain,
                                     aWindow, aParentScriptContext, aBaseURI,
                                     aPrincipal, aCSP, aEvalAllowed),
  mJSContext(nullptr), mErrorHandlerRecursionCount(0), mNextTimeoutId(1),
  mStatus(Pending), mSuspended(false), mTimerRunning(false),
  mRunningExpiredTimeouts(false), mCloseHandlerStarted(false),
  mCloseHandlerFinished(false), mMemoryReporterRunning(false),
  mXHRParamsAllowed(aXHRParamsAllowed)
{
  MOZ_COUNT_CTOR(mozilla::dom::workers::WorkerPrivate);
}

WorkerPrivate::~WorkerPrivate()
{
  MOZ_COUNT_DTOR(mozilla::dom::workers::WorkerPrivate);
}

// static
already_AddRefed<WorkerPrivate>
WorkerPrivate::Create(JSContext* aCx, JSObject* aObj, WorkerPrivate* aParent,
                      JSString* aScriptURL, bool aIsChromeWorker)
{
  nsCString domain;
  nsCOMPtr<nsIURI> baseURI;
  nsCOMPtr<nsIPrincipal> principal;
  nsCOMPtr<nsIScriptContext> scriptContext;
  nsCOMPtr<nsIDocument> document;
  nsCOMPtr<nsPIDOMWindow> window;
  nsCOMPtr<nsIContentSecurityPolicy> csp;

  bool evalAllowed = true;

  JSContext* parentContext;

  bool xhrParamsAllowed = false;

  if (aParent) {
    aParent->AssertIsOnWorkerThread();

    parentContext = aCx;

    // Domain is the only thing we can touch here. The rest will be handled by
    // the ScriptLoader.
    domain = aParent->Domain();
  }
  else {
    AssertIsOnMainThread();

    nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
    NS_ASSERTION(ssm, "This should never be null!");

    bool isChrome = nsContentUtils::IsCallerChrome();

    // First check to make sure the caller has permission to make a
    // ChromeWorker if they called the ChromeWorker constructor.
    if (aIsChromeWorker && !isChrome) {
      xpc::Throw(aCx, NS_ERROR_DOM_SECURITY_ERR);
      return nullptr;
    }

    // Chrome callers (whether ChromeWorker of Worker) always get the system
    // principal here as they're allowed to load anything. The script loader may
    // change the principal later depending on the script uri.
    if (isChrome &&
        NS_FAILED(ssm->GetSystemPrincipal(getter_AddRefs(principal)))) {
      JS_ReportError(aCx, "Could not get system principal!");
      return nullptr;
    }

    // See if we're being called from a window or from somewhere else.
    nsCOMPtr<nsIScriptGlobalObject> scriptGlobal =
      nsJSUtils::GetStaticScriptGlobal(JS_GetGlobalForScopeChain(aCx));
    if (scriptGlobal) {
      // Window!
      nsCOMPtr<nsPIDOMWindow> globalWindow = do_QueryInterface(scriptGlobal);

      // Only use the current inner window, and only use it if the caller can
      // access it.
      nsPIDOMWindow* outerWindow = globalWindow ?
                                   globalWindow->GetOuterWindow() :
                                   nullptr;
      window = outerWindow ? outerWindow->GetCurrentInnerWindow() : nullptr;
      if (!window ||
          (globalWindow != window &&
           !nsContentUtils::CanCallerAccess(window))) {
        xpc::Throw(aCx, NS_ERROR_DOM_SECURITY_ERR);
        return nullptr;
      }

      scriptContext = scriptGlobal->GetContext();
      if (!scriptContext) {
        JS_ReportError(aCx, "Couldn't get script context for this worker!");
        return nullptr;
      }

      parentContext = scriptContext->GetNativeContext();

      // If we're called from a window then we can dig out the principal and URI
      // from the document.
      document = do_QueryInterface(window->GetExtantDocument());
      if (!document) {
        JS_ReportError(aCx, "No document in this window!");
        return nullptr;
      }

      baseURI = document->GetDocBaseURI();

      // Use the document's NodePrincipal as our principal if we're not being
      // called from chrome.
      if (!principal) {
        if (!(principal = document->NodePrincipal())) {
          JS_ReportError(aCx, "Could not get document principal!");
          return nullptr;
        }

        nsCOMPtr<nsIURI> codebase;
        if (NS_FAILED(principal->GetURI(getter_AddRefs(codebase)))) {
          JS_ReportError(aCx, "Could not determine codebase!");
          return nullptr;
        }

        NS_NAMED_LITERAL_CSTRING(file, "file");

        bool isFile;
        if (NS_FAILED(codebase->SchemeIs(file.get(), &isFile))) {
          JS_ReportError(aCx, "Could not determine if codebase is file!");
          return nullptr;
        }

        if (isFile) {
          // XXX Fix this, need a real domain here.
          domain = file;
        }
        // Workaround for workers needing a string domain - will be fixed
        // in a followup after this lands.
        else if (document->GetSandboxFlags() & SANDBOXED_ORIGIN) {
          if (NS_FAILED(codebase->GetAsciiSpec(domain))) {
            JS_ReportError(aCx, "Could not get URI's spec for sandboxed document!");
            return nullptr;
          }
        } else {
          nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil =
            do_GetService(THIRDPARTYUTIL_CONTRACTID);
          if (!thirdPartyUtil) {
            JS_ReportError(aCx, "Could not get third party helper service!");
            return nullptr;
          }

          if (NS_FAILED(thirdPartyUtil->GetBaseDomain(codebase, domain))) {
            JS_ReportError(aCx, "Could not get domain!");
            return nullptr;
          }
        }
      }

      xhrParamsAllowed = CheckXHRParamsAllowed(window);
    }
    else {
      // Not a window
      NS_ASSERTION(isChrome, "Should be chrome only!");

      parentContext = nullptr;

      // We're being created outside of a window. Need to figure out the script
      // that is creating us in order for us to use relative URIs later on.
      JSScript *script;
      if (JS_DescribeScriptedCaller(aCx, &script, nullptr)) {
        if (NS_FAILED(NS_NewURI(getter_AddRefs(baseURI),
                                JS_GetScriptFilename(aCx, script)))) {
          JS_ReportError(aCx, "Failed to construct base URI!");
          return nullptr;
        }
      }

      xhrParamsAllowed = true;
    }

    NS_ASSERTION(principal, "Must have a principal now!");

    if (!isChrome && domain.IsEmpty()) {
      NS_ERROR("Must be chrome or have an domain!");
      return nullptr;
    }

    if (!GetContentSecurityPolicy(aCx, getter_AddRefs(csp))) {
      return nullptr;
    }

    if (csp && NS_FAILED(csp->GetAllowsEval(&evalAllowed))) {
      NS_ERROR("CSP: failed to get allowsEval");
      return nullptr;
    }
  }

  size_t urlLength;
  const jschar* urlChars = JS_GetStringCharsZAndLength(aCx, aScriptURL,
                                                       &urlLength);
  if (!urlChars) {
    return nullptr;
  }

  nsDependentString scriptURL(urlChars, urlLength);

  nsRefPtr<WorkerPrivate> worker =
    new WorkerPrivate(aCx, aObj, aParent, parentContext, scriptURL,
                      aIsChromeWorker, domain, window, scriptContext, baseURI,
                      principal, csp, evalAllowed, xhrParamsAllowed);

  worker->SetIsDOMBinding();
  worker->SetWrapper(aObj);

  nsRefPtr<CompileScriptRunnable> compiler = new CompileScriptRunnable(worker);
  if (!compiler->Dispatch(aCx)) {
    return nullptr;
  }

  return worker.forget();
}

void
WorkerPrivate::DoRunLoop(JSContext* aCx)
{
  AssertIsOnWorkerThread();

  {
    MutexAutoLock lock(mMutex);
    mJSContext = aCx;

    NS_ASSERTION(mStatus == Pending, "Huh?!");
    mStatus = Running;
  }

  // We need a timer for GC. The basic plan is to run a normal (non-shrinking)
  // GC periodically (NORMAL_GC_TIMER_DELAY_MS) while the worker is running.
  // Once the worker goes idle we set a short (IDLE_GC_TIMER_DELAY_MS) timer to
  // run a shrinking GC. If the worker receives more messages then the short
  // timer is canceled and the periodic timer resumes.
  nsCOMPtr<nsITimer> gcTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  if (!gcTimer) {
    JS_ReportError(aCx, "Failed to create GC timer!");
    return;
  }

  bool normalGCTimerRunning = false;

  // We need to swap event targets below to get different types of GC behavior.
  nsCOMPtr<nsIEventTarget> normalGCEventTarget;
  nsCOMPtr<nsIEventTarget> idleGCEventTarget;

  // We also need to track the idle GC event so that we don't confuse it with a
  // generic event that should re-trigger the idle GC timer.
  nsCOMPtr<nsIRunnable> idleGCEvent;
  {
    nsRefPtr<GarbageCollectRunnable> runnable =
      new GarbageCollectRunnable(this, false, false);
    normalGCEventTarget = new WorkerRunnableEventTarget(runnable);

    runnable = new GarbageCollectRunnable(this, true, false);
    idleGCEventTarget = new WorkerRunnableEventTarget(runnable);

    idleGCEvent = runnable;
  }

  mMemoryReporter = new WorkerMemoryReporter(this);

  if (NS_FAILED(NS_RegisterMemoryMultiReporter(mMemoryReporter))) {
    NS_WARNING("Failed to register memory reporter!");
    mMemoryReporter = nullptr;
  }

  for (;;) {
    Status currentStatus;
    bool scheduleIdleGC;

    WorkerRunnable* event;
    {
      MutexAutoLock lock(mMutex);

      while (!mControlQueue.Pop(event) && !mQueue.Pop(event)) {
        mCondVar.Wait();
      }

      bool eventIsNotIdleGCEvent;
      currentStatus = mStatus;

      {
        MutexAutoUnlock unlock(mMutex);

        if (!normalGCTimerRunning &&
            event != idleGCEvent &&
            currentStatus <= Terminating) {
          // Must always cancel before changing the timer's target.
          if (NS_FAILED(gcTimer->Cancel())) {
            NS_WARNING("Failed to cancel GC timer!");
          }

          if (NS_SUCCEEDED(gcTimer->SetTarget(normalGCEventTarget)) &&
              NS_SUCCEEDED(gcTimer->InitWithFuncCallback(
                                             DummyCallback, nullptr,
                                             NORMAL_GC_TIMER_DELAY_MS,
                                             nsITimer::TYPE_REPEATING_SLACK))) {
            normalGCTimerRunning = true;
          }
          else {
            JS_ReportError(aCx, "Failed to start normal GC timer!");
          }
        }

#ifdef EXTRA_GC
        // Find GC bugs...
        JS_GC(aCx);
#endif

        // Keep track of whether or not this is the idle GC event.
        eventIsNotIdleGCEvent = event != idleGCEvent;

        static_cast<nsIRunnable*>(event)->Run();
        NS_RELEASE(event);
      }

      currentStatus = mStatus;
      scheduleIdleGC = mControlQueue.IsEmpty() &&
                       mQueue.IsEmpty() &&
                       eventIsNotIdleGCEvent;
    }

    // Take care of the GC timer. If we're starting the close sequence then we
    // kill the timer once and for all. Otherwise we schedule the idle timeout
    // if there are no more events.
    if (currentStatus > Terminating || scheduleIdleGC) {
      if (NS_SUCCEEDED(gcTimer->Cancel())) {
        normalGCTimerRunning = false;
      }
      else {
        NS_WARNING("Failed to cancel GC timer!");
      }
    }

    if (scheduleIdleGC) {
      if (NS_SUCCEEDED(gcTimer->SetTarget(idleGCEventTarget)) &&
          NS_SUCCEEDED(gcTimer->InitWithFuncCallback(
                                                    DummyCallback, nullptr,
                                                    IDLE_GC_TIMER_DELAY_MS,
                                                    nsITimer::TYPE_ONE_SHOT))) {
      }
      else {
        JS_ReportError(aCx, "Failed to start idle GC timer!");
      }
    }

#ifdef EXTRA_GC
    // Find GC bugs...
    JS_GC(aCx);
#endif

    if (currentStatus != Running && !HasActiveFeatures()) {
      // If the close handler has finished and all features are done then we can
      // kill this thread.
      if (mCloseHandlerFinished && currentStatus != Killing) {
        if (!NotifyInternal(aCx, Killing)) {
          JS_ReportPendingException(aCx);
        }
#ifdef DEBUG
        {
          MutexAutoLock lock(mMutex);
          currentStatus = mStatus;
        }
        NS_ASSERTION(currentStatus == Killing, "Should have changed status!");
#else
        currentStatus = Killing;
#endif
      }

      // If we're supposed to die then we should exit the loop.
      if (currentStatus == Killing) {
        // Always make sure the timer is canceled.
        if (NS_FAILED(gcTimer->Cancel())) {
          NS_WARNING("Failed to cancel the GC timer!");
        }

        // Call this before unregistering the reporter as we may be racing with
        // the main thread.
        DisableMemoryReporter();

        if (mMemoryReporter) {
          if (NS_FAILED(NS_UnregisterMemoryMultiReporter(mMemoryReporter))) {
            NS_WARNING("Failed to unregister memory reporter!");
          }
          mMemoryReporter = nullptr;
        }

        StopAcceptingEvents();
        return;
      }
    }
  }

  NS_NOTREACHED("Shouldn't get here!");
}

bool
WorkerPrivate::OperationCallback(JSContext* aCx)
{
  AssertIsOnWorkerThread();

  bool mayContinue = true;

  for (;;) {
    // Run all control events now.
    mayContinue = ProcessAllControlRunnables();

    if (!mayContinue || !mSuspended) {
      break;
    }

    // Clean up before suspending.
    JS_GC(JS_GetRuntime(aCx));

    while ((mayContinue = MayContinueRunning())) {
      MutexAutoLock lock(mMutex);
      if (!mControlQueue.IsEmpty()) {
        break;
      }

      mCondVar.Wait(PR_MillisecondsToInterval(RemainingRunTimeMS()));
    }
  }

  if (!mayContinue) {
    // We want only uncatchable exceptions here.
    NS_ASSERTION(!JS_IsExceptionPending(aCx),
                 "Should not have an exception set here!");
    return false;
  }

  return true;
}

void
WorkerPrivate::ScheduleDeletion(bool aWasPending)
{
  AssertIsOnWorkerThread();
  NS_ASSERTION(mChildWorkers.IsEmpty(), "Live child workers!");
  NS_ASSERTION(mSyncQueues.IsEmpty(), "Should have no sync queues here!");

  StopAcceptingEvents();

  nsIThread* currentThread;
  if (aWasPending) {
    // Don't want to close down this thread since we never got to run!
    currentThread = nullptr;
  }
  else {
    currentThread = NS_GetCurrentThread();
    NS_ASSERTION(currentThread, "This should never be null!");
  }

  WorkerPrivate* parent = GetParent();
  if (parent) {
    nsRefPtr<WorkerFinishedRunnable> runnable =
      new WorkerFinishedRunnable(parent, this, currentThread);
    if (!runnable->Dispatch(nullptr)) {
      NS_WARNING("Failed to dispatch runnable!");
    }
  }
  else {
    nsRefPtr<TopLevelWorkerFinishedRunnable> runnable =
      new TopLevelWorkerFinishedRunnable(this, currentThread);
    if (NS_FAILED(NS_DispatchToMainThread(runnable, NS_DISPATCH_NORMAL))) {
      NS_WARNING("Failed to dispatch runnable!");
    }
  }
}

bool
WorkerPrivate::BlockAndCollectRuntimeStats(bool aIsQuick, void* aData)
{
  AssertIsOnMainThread();
  NS_ASSERTION(aData, "Null data!");

  {
    MutexAutoLock lock(mMutex);
    mMemoryReporterRunning = true;
  }

  bool succeeded;

  nsRefPtr<CollectRuntimeStatsRunnable> runnable =
    new CollectRuntimeStatsRunnable(this, aIsQuick, aData, &succeeded);
  if (!runnable->Dispatch(nullptr)) {
    NS_WARNING("Failed to dispatch runnable!");
    succeeded = false;
  }

  {
    MutexAutoLock lock(mMutex);
    mMemoryReporterRunning = false;
  }

  return succeeded;
}

bool
WorkerPrivate::DisableMemoryReporter()
{
  AssertIsOnWorkerThread();

  bool result = true;

  {
    MutexAutoLock lock(mMutex);

    while (mMemoryReporterRunning) {
      MutexAutoUnlock unlock(mMutex);
      result = ProcessAllControlRunnables() && result;
    }

    if (mMemoryReporter) {
      mMemoryReporter->Disable();
    }
  }

  return result;
}

bool
WorkerPrivate::ProcessAllControlRunnables()
{
  AssertIsOnWorkerThread();

  bool result = true;

  for (;;) {
    WorkerRunnable* event;
    {
      MutexAutoLock lock(mMutex);
      if (!mControlQueue.Pop(event)) {
        break;
      }
    }

    if (NS_FAILED(static_cast<nsIRunnable*>(event)->Run())) {
      result = false;
    }

    NS_RELEASE(event);
  }
  return result;
}

bool
WorkerPrivate::CheckXHRParamsAllowed(nsPIDOMWindow* aWindow)
{
  AssertIsOnMainThread();
  NS_ASSERTION(aWindow, "Wrong cannot be null");

  if (!aWindow->GetDocShell()) {
    return false;
  }

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aWindow->GetExtantDocument());
  if (!doc) {
    return false;
  }

  nsCOMPtr<nsIPermissionManager> permMgr = do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
  if (!permMgr) {
    return false;
  }

  uint32_t permission;
  nsresult rv = permMgr->TestPermissionFromPrincipal(doc->NodePrincipal(),
                                                     "systemXHR", &permission);
  if (NS_FAILED(rv) || permission != nsIPermissionManager::ALLOW_ACTION) {
    return false;
  }

  return true;
}

bool
WorkerPrivate::Dispatch(WorkerRunnable* aEvent, EventQueue* aQueue)
{
  nsRefPtr<WorkerRunnable> event(aEvent);

  {
    MutexAutoLock lock(mMutex);

    if (mStatus == Dead) {
      // Nothing may be added after we've set Dead.
      return false;
    }

    if (aQueue == &mQueue) {
      // Check parent status.
      Status parentStatus = ParentStatus();
      if (parentStatus >= Terminating) {
        // Throw.
        return false;
      }

      // Check inner status too.
      if (parentStatus >= Closing || mStatus >= Closing) {
        // Silently eat this one.
        return true;
      }
    }

    if (!aQueue->Push(event)) {
      return false;
    }

    if (aQueue == &mControlQueue && mJSContext) {
      JS_TriggerOperationCallback(JS_GetRuntime(mJSContext));
    }

    mCondVar.Notify();
  }

  event.forget();
  return true;
}

bool
WorkerPrivate::DispatchToSyncQueue(WorkerSyncRunnable* aEvent)
{
  nsRefPtr<WorkerRunnable> event(aEvent);

  {
    MutexAutoLock lock(mMutex);

    NS_ASSERTION(mSyncQueues.Length() > aEvent->mSyncQueueKey, "Bad event!");

    if (!mSyncQueues[aEvent->mSyncQueueKey]->mQueue.Push(event)) {
      return false;
    }

    mCondVar.Notify();
  }

  event.forget();
  return true;
}

void
WorkerPrivate::ClearQueue(EventQueue* aQueue)
{
  AssertIsOnWorkerThread();
  mMutex.AssertCurrentThreadOwns();

  WorkerRunnable* event;
  while (aQueue->Pop(event)) {
    if (event->WantsToRunDuringClear()) {
      MutexAutoUnlock unlock(mMutex);

      static_cast<nsIRunnable*>(event)->Run();
    }
    event->Release();
  }
}

uint32_t
WorkerPrivate::RemainingRunTimeMS() const
{
  if (mKillTime.IsNull()) {
    return UINT32_MAX;
  }
  TimeDuration runtime = mKillTime - TimeStamp::Now();
  double ms = runtime > TimeDuration(0) ? runtime.ToMilliseconds() : 0;
  return ms > double(UINT32_MAX) ? UINT32_MAX : uint32_t(ms);
}

bool
WorkerPrivate::SuspendInternal(JSContext* aCx)
{
  AssertIsOnWorkerThread();

  NS_ASSERTION(!mSuspended, "Already suspended!");

  mSuspended = true;
  return true;
}

bool
WorkerPrivate::ResumeInternal(JSContext* aCx)
{
  AssertIsOnWorkerThread();

  NS_ASSERTION(mSuspended, "Not yet suspended!");

  mSuspended = false;
  return true;
}

void
WorkerPrivate::TraceInternal(JSTracer* aTrc)
{
  AssertIsOnWorkerThread();

  for (uint32_t index = 0; index < mTimeouts.Length(); index++) {
    TimeoutInfo* info = mTimeouts[index];
    JS_CALL_VALUE_TRACER(aTrc, info->mTimeoutVal,
                         "WorkerPrivate timeout value");
    for (uint32_t index2 = 0; index2 < info->mExtraArgVals.Length(); index2++) {
      JS_CALL_VALUE_TRACER(aTrc, info->mExtraArgVals[index2],
                           "WorkerPrivate timeout extra argument value");
    }
  }
}

bool
WorkerPrivate::ModifyBusyCountFromWorker(JSContext* aCx, bool aIncrease)
{
  AssertIsOnWorkerThread();

  {
    MutexAutoLock lock(mMutex);

    // If we're in shutdown then the busy count is no longer being considered so
    // just return now.
    if (mStatus >= Killing) {
      return true;
    }
  }

  nsRefPtr<ModifyBusyCountRunnable> runnable =
    new ModifyBusyCountRunnable(this, aIncrease);
  return runnable->Dispatch(aCx);
}

bool
WorkerPrivate::AddChildWorker(JSContext* aCx, ParentType* aChildWorker)
{
  AssertIsOnWorkerThread();

  Status currentStatus;
  {
    MutexAutoLock lock(mMutex);
    currentStatus = mStatus;
  }

  if (currentStatus > Running) {
    JS_ReportError(aCx, "Cannot create child workers from the close handler!");
    return false;
  }

  NS_ASSERTION(!mChildWorkers.Contains(aChildWorker),
               "Already know about this one!");
  mChildWorkers.AppendElement(aChildWorker);

  return mChildWorkers.Length() == 1 ?
         ModifyBusyCountFromWorker(aCx, true) :
         true;
}

void
WorkerPrivate::RemoveChildWorker(JSContext* aCx, ParentType* aChildWorker)
{
  AssertIsOnWorkerThread();

  NS_ASSERTION(mChildWorkers.Contains(aChildWorker),
               "Didn't know about this one!");
  mChildWorkers.RemoveElement(aChildWorker);

  if (mChildWorkers.IsEmpty() && !ModifyBusyCountFromWorker(aCx, false)) {
    NS_WARNING("Failed to modify busy count!");
  }
}

bool
WorkerPrivate::AddFeature(JSContext* aCx, WorkerFeature* aFeature)
{
  AssertIsOnWorkerThread();

  {
    MutexAutoLock lock(mMutex);

    if (mStatus >= Canceling) {
      return false;
    }
  }

  NS_ASSERTION(!mFeatures.Contains(aFeature), "Already know about this one!");
  mFeatures.AppendElement(aFeature);

  return mFeatures.Length() == 1 ?
         ModifyBusyCountFromWorker(aCx, true) :
         true;
}

void
WorkerPrivate::RemoveFeature(JSContext* aCx, WorkerFeature* aFeature)
{
  AssertIsOnWorkerThread();

  NS_ASSERTION(mFeatures.Contains(aFeature), "Didn't know about this one!");
  mFeatures.RemoveElement(aFeature);

  if (mFeatures.IsEmpty() && !ModifyBusyCountFromWorker(aCx, false)) {
    NS_WARNING("Failed to modify busy count!");
  }
}

void
WorkerPrivate::NotifyFeatures(JSContext* aCx, Status aStatus)
{
  AssertIsOnWorkerThread();

  NS_ASSERTION(aStatus > Running, "Bad status!");

  if (aStatus >= Closing) {
    CancelAllTimeouts(aCx);
  }

  nsAutoTArray<WorkerFeature*, 30> features;
  features.AppendElements(mFeatures);

  for (uint32_t index = 0; index < features.Length(); index++) {
    if (!features[index]->Notify(aCx, aStatus)) {
      NS_WARNING("Failed to notify feature!");
    }
  }

  nsAutoTArray<ParentType*, 10> children;
  children.AppendElements(mChildWorkers);

  for (uint32_t index = 0; index < children.Length(); index++) {
    if (!children[index]->Notify(aCx, aStatus)) {
      NS_WARNING("Failed to notify child worker!");
    }
  }
}

void
WorkerPrivate::CancelAllTimeouts(JSContext* aCx)
{
  AssertIsOnWorkerThread();

  if (mTimerRunning) {
    NS_ASSERTION(mTimer, "Huh?!");
    NS_ASSERTION(!mTimeouts.IsEmpty(), "Huh?!");

    if (NS_FAILED(mTimer->Cancel())) {
      NS_WARNING("Failed to cancel timer!");
    }

    for (uint32_t index = 0; index < mTimeouts.Length(); index++) {
      mTimeouts[index]->mCanceled = true;
    }

    if (!RunExpiredTimeouts(aCx)) {
      JS_ReportPendingException(aCx);
    }

    mTimerRunning = false;
  }
#ifdef DEBUG
  else if (!mRunningExpiredTimeouts) {
    NS_ASSERTION(mTimeouts.IsEmpty(), "Huh?!");
  }
#endif

  mTimer = nullptr;
}

uint32_t
WorkerPrivate::CreateNewSyncLoop()
{
  AssertIsOnWorkerThread();

  NS_ASSERTION(mSyncQueues.Length() < UINT32_MAX,
               "Should have bailed by now!");

  mSyncQueues.AppendElement(new SyncQueue());
  return mSyncQueues.Length() - 1;
}

bool
WorkerPrivate::RunSyncLoop(JSContext* aCx, uint32_t aSyncLoopKey)
{
  AssertIsOnWorkerThread();

  NS_ASSERTION(!mSyncQueues.IsEmpty() ||
               (aSyncLoopKey != mSyncQueues.Length() - 1),
               "Forgot to call CreateNewSyncLoop!");
  if (aSyncLoopKey != mSyncQueues.Length() - 1) {
    return false;
  }

  SyncQueue* syncQueue = mSyncQueues[aSyncLoopKey].get();

  for (;;) {
    WorkerRunnable* event;
    {
      MutexAutoLock lock(mMutex);

      while (!mControlQueue.Pop(event) && !syncQueue->mQueue.Pop(event)) {
        mCondVar.Wait();
      }
    }

#ifdef EXTRA_GC
    // Find GC bugs...
    JS_GC(mJSContext);
#endif

    static_cast<nsIRunnable*>(event)->Run();
    NS_RELEASE(event);

#ifdef EXTRA_GC
    // Find GC bugs...
    JS_GC(mJSContext);
#endif

    if (syncQueue->mComplete) {
      NS_ASSERTION(mSyncQueues.Length() - 1 == aSyncLoopKey,
                   "Mismatched calls!");
      NS_ASSERTION(syncQueue->mQueue.IsEmpty(), "Unprocessed sync events!");

      bool result = syncQueue->mResult;
      DestroySyncLoop(aSyncLoopKey);

#ifdef DEBUG
      syncQueue = nullptr;
#endif

      return result;
    }
  }

  NS_NOTREACHED("Shouldn't get here!");
  return false;
}

void
WorkerPrivate::StopSyncLoop(uint32_t aSyncLoopKey, bool aSyncResult)
{
  AssertIsOnWorkerThread();

  NS_ASSERTION(mSyncQueues.IsEmpty() ||
               (aSyncLoopKey == mSyncQueues.Length() - 1),
               "Forgot to call CreateNewSyncLoop!");
  if (aSyncLoopKey != mSyncQueues.Length() - 1) {
    return;
  }

  SyncQueue* syncQueue = mSyncQueues[aSyncLoopKey].get();

  NS_ASSERTION(!syncQueue->mComplete, "Already called StopSyncLoop?!");

  syncQueue->mResult = aSyncResult;
  syncQueue->mComplete = true;
}

void
WorkerPrivate::DestroySyncLoop(uint32_t aSyncLoopKey)
{
  AssertIsOnWorkerThread();

  mSyncQueues.RemoveElementAt(aSyncLoopKey);
}

bool
WorkerPrivate::PostMessageToParent(JSContext* aCx, jsval aMessage,
                                   jsval aTransferable)
{
  AssertIsOnWorkerThread();

  JSStructuredCloneCallbacks* callbacks =
    IsChromeWorker() ?
    &gChromeWorkerStructuredCloneCallbacks :
    &gWorkerStructuredCloneCallbacks;

  nsTArray<nsCOMPtr<nsISupports> > clonedObjects;

  JSAutoStructuredCloneBuffer buffer;
  if (!buffer.write(aCx, aMessage, aTransferable, callbacks, &clonedObjects)) {
    return false;
  }

  nsRefPtr<MessageEventRunnable> runnable =
    new MessageEventRunnable(this, WorkerRunnable::ParentThread, buffer,
                             clonedObjects);
  return runnable->Dispatch(aCx);
}

bool
WorkerPrivate::NotifyInternal(JSContext* aCx, Status aStatus)
{
  AssertIsOnWorkerThread();

  NS_ASSERTION(aStatus > Running && aStatus < Dead, "Bad status!");

  // Save the old status and set the new status.
  Status previousStatus;
  {
    MutexAutoLock lock(mMutex);

    if (mStatus >= aStatus) {
      return true;
    }

    previousStatus = mStatus;
    mStatus = aStatus;
  }

  // Now that status > Running, no-one can create a new mCrossThreadDispatcher
  // if we don't already have one.
  if (mCrossThreadDispatcher) {
    // Since we'll no longer process events, make sure we no longer allow
    // anyone to post them.
    // We have to do this without mMutex held, since our mutex must be
    // acquired *after* mCrossThreadDispatcher's mutex when they're both held.
    mCrossThreadDispatcher->Forget();
  }

  NS_ASSERTION(previousStatus != Pending, "How is this possible?!");

  NS_ASSERTION(previousStatus >= Canceling || mKillTime.IsNull(),
               "Bad kill time set!");

  // Let all our features know the new status.
  NotifyFeatures(aCx, aStatus);

  // If this is the first time our status has changed then we need to clear the
  // main event queue.
  if (previousStatus == Running) {
    MutexAutoLock lock(mMutex);
    ClearQueue(&mQueue);
  }

  // If we've run the close handler, we don't need to do anything else.
  if (mCloseHandlerFinished) {
    return true;
  }

  // If the worker script never ran, or failed to compile, we don't need to do
  // anything else, except pretend that we ran the close handler.
  if (!JS_GetGlobalObject(aCx)) {
    mCloseHandlerStarted = true;
    mCloseHandlerFinished = true;
    return true;
  }

  // If this is the first time our status has changed we also need to schedule
  // the close handler unless we're being shut down.
  if (previousStatus == Running && aStatus != Killing) {
    NS_ASSERTION(!mCloseHandlerStarted && !mCloseHandlerFinished,
                 "This is impossible!");

    nsRefPtr<CloseEventRunnable> closeRunnable = new CloseEventRunnable(this);

    MutexAutoLock lock(mMutex);

    if (!mQueue.Push(closeRunnable)) {
      NS_WARNING("Failed to push closeRunnable!");
      return false;
    }

    closeRunnable.forget();
  }

  if (aStatus == Closing) {
    // Notify parent to stop sending us messages and balance our busy count.
    nsRefPtr<CloseRunnable> runnable = new CloseRunnable(this);
    if (!runnable->Dispatch(aCx)) {
      return false;
    }

    // Don't abort the script.
    return true;
  }

  if (aStatus == Terminating) {
    // Only abort the script if we're not yet running the close handler.
    return mCloseHandlerStarted;
  }

  if (aStatus == Canceling) {
    // We need to enforce a timeout on the close handler.
    NS_ASSERTION(previousStatus == Running || previousStatus == Closing ||
                 previousStatus == Terminating,
                 "Bad previous status!");

    uint32_t killSeconds = RuntimeService::GetCloseHandlerTimeoutSeconds();
    if (killSeconds) {
      mKillTime = TimeStamp::Now() + TimeDuration::FromSeconds(killSeconds);

      if (!mCloseHandlerFinished && !ScheduleKillCloseEventRunnable(aCx)) {
        return false;
      }
    }

    // Only abort the script if we're not yet running the close handler.
    return mCloseHandlerStarted;
  }

  if (aStatus == Killing) {
    mKillTime = TimeStamp::Now();

    if (!mCloseHandlerFinished && !ScheduleKillCloseEventRunnable(aCx)) {
      return false;
    }

    // Always abort the script.
    return false;
  }

  NS_NOTREACHED("Should never get here!");
  return false;
}

bool
WorkerPrivate::ScheduleKillCloseEventRunnable(JSContext* aCx)
{
  AssertIsOnWorkerThread();
  NS_ASSERTION(!mKillTime.IsNull(), "Must have a kill time!");

  nsRefPtr<KillCloseEventRunnable> killCloseEventRunnable =
    new KillCloseEventRunnable(this);
  if (!killCloseEventRunnable->SetTimeout(aCx, RemainingRunTimeMS())) {
    return false;
  }

  MutexAutoLock lock(mMutex);

  if (!mQueue.Push(killCloseEventRunnable)) {
    NS_WARNING("Failed to push killCloseEventRunnable!");
    return false;
  }

  killCloseEventRunnable.forget();
  return true;
}

void
WorkerPrivate::ReportError(JSContext* aCx, const char* aMessage,
                           JSErrorReport* aReport)
{
  AssertIsOnWorkerThread();

  if (!MayContinueRunning() || mErrorHandlerRecursionCount == 2) {
    return;
  }

  NS_ASSERTION(mErrorHandlerRecursionCount == 0 ||
               mErrorHandlerRecursionCount == 1,
               "Bad recursion logic!");

  JS_ClearPendingException(aCx);

  nsString message, filename, line;
  uint32_t lineNumber, columnNumber, flags, errorNumber;

  if (aReport) {
    if (aReport->ucmessage) {
      message = aReport->ucmessage;
    }
    filename = NS_ConvertUTF8toUTF16(aReport->filename);
    line = aReport->uclinebuf;
    lineNumber = aReport->lineno;
    columnNumber = aReport->uctokenptr - aReport->uclinebuf;
    flags = aReport->flags;
    errorNumber = aReport->errorNumber;
  }
  else {
    lineNumber = columnNumber = errorNumber = 0;
    flags = nsIScriptError::errorFlag | nsIScriptError::exceptionFlag;
  }

  if (message.IsEmpty()) {
    message = NS_ConvertUTF8toUTF16(aMessage);
  }

  mErrorHandlerRecursionCount++;

  // Don't want to run the scope's error handler if this is a recursive error or
  // if there was an error in the close handler or if we ran out of memory.
  bool fireAtScope = mErrorHandlerRecursionCount == 1 &&
                     !mCloseHandlerStarted &&
                     errorNumber != JSMSG_OUT_OF_MEMORY;

  if (!ReportErrorRunnable::ReportError(aCx, this, fireAtScope, nullptr, message,
                                        filename, line, lineNumber,
                                        columnNumber, flags, errorNumber, 0)) {
    JS_ReportPendingException(aCx);
  }

  mErrorHandlerRecursionCount--;
}

bool
WorkerPrivate::SetTimeout(JSContext* aCx, unsigned aArgc, jsval* aVp,
                          bool aIsInterval)
{
  AssertIsOnWorkerThread();
  NS_ASSERTION(aArgc, "Huh?!");

  const uint32_t timerId = mNextTimeoutId++;

  Status currentStatus;
  {
    MutexAutoLock lock(mMutex);
    currentStatus = mStatus;
  }

  // It's a script bug if setTimeout/setInterval are called from a close handler
  // so throw an exception.
  if (currentStatus == Closing) {
    JS_ReportError(aCx, "Cannot schedule timeouts from the close handler!");
  }

  // If the worker is trying to call setTimeout/setInterval and the parent
  // thread has initiated the close process then just silently fail.
  if (currentStatus >= Closing) {
    return false;
  }

  nsAutoPtr<TimeoutInfo> newInfo(new TimeoutInfo());
  newInfo->mIsInterval = aIsInterval;
  newInfo->mId = timerId;

  if (MOZ_UNLIKELY(timerId == UINT32_MAX)) {
    NS_WARNING("Timeout ids overflowed!");
    mNextTimeoutId = 1;
  }

  JS::Value* argv = JS_ARGV(aCx, aVp);

  // Take care of the main argument.
  if (argv[0].isObject()) {
    if (JS_ObjectIsCallable(aCx, &argv[0].toObject())) {
      newInfo->mTimeoutVal = argv[0];
    }
    else {
      JSString* timeoutStr = JS_ValueToString(aCx, argv[0]);
      if (!timeoutStr) {
        return false;
      }
      newInfo->mTimeoutVal.setString(timeoutStr);
    }
  }
  else if (argv[0].isString()) {
    newInfo->mTimeoutVal = argv[0];
  }
  else {
    JS_ReportError(aCx, "Useless %s call (missing quotes around argument?)",
                   aIsInterval ? "setInterval" : "setTimeout");
    return false;
  }

  // See if any of the optional arguments were passed.
  if (aArgc > 1) {
    double intervalMS = 0;
    if (!JS_ValueToNumber(aCx, argv[1], &intervalMS)) {
      return false;
    }
    newInfo->mInterval = TimeDuration::FromMilliseconds(intervalMS);

    if (aArgc > 2 && newInfo->mTimeoutVal.isObject()) {
      nsTArray<jsval> extraArgVals(aArgc - 2);
      for (unsigned index = 2; index < aArgc; index++) {
        extraArgVals.AppendElement(argv[index]);
      }
      newInfo->mExtraArgVals.SwapElements(extraArgVals);
    }
  }

  newInfo->mTargetTime = TimeStamp::Now() + newInfo->mInterval;

  if (newInfo->mTimeoutVal.isString()) {
    const char* filenameChars;
    uint32_t lineNumber;
    if (nsJSUtils::GetCallingLocation(aCx, &filenameChars, &lineNumber)) {
      newInfo->mFilename = filenameChars;
      newInfo->mLineNumber = lineNumber;
    }
    else {
      NS_WARNING("Failed to get calling location!");
    }
  }

  mTimeouts.InsertElementSorted(newInfo.get(), GetAutoPtrComparator(mTimeouts));

  // If the timeout we just made is set to fire next then we need to update the
  // timer.
  if (mTimeouts[0] == newInfo) {
    nsresult rv;

    if (!mTimer) {
      mTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
      if (NS_FAILED(rv)) {
        JS_ReportError(aCx, "Failed to create timer!");
        return false;
      }

      nsRefPtr<TimerRunnable> timerRunnable = new TimerRunnable(this);

      nsCOMPtr<nsIEventTarget> target =
        new WorkerRunnableEventTarget(timerRunnable);
      rv = mTimer->SetTarget(target);
      if (NS_FAILED(rv)) {
        JS_ReportError(aCx, "Failed to set timer's target!");
        return false;
      }
    }

    if (!mTimerRunning) {
      if (!ModifyBusyCountFromWorker(aCx, true)) {
        return false;
      }
      mTimerRunning = true;
    }

    if (!RescheduleTimeoutTimer(aCx)) {
      return false;
    }
  }

  JS_SET_RVAL(aCx, aVp, INT_TO_JSVAL(timerId));

  newInfo.forget();
  return true;
}

bool
WorkerPrivate::ClearTimeout(JSContext* aCx, uint32_t aId)
{
  AssertIsOnWorkerThread();

  if (!mTimeouts.IsEmpty()) {
    NS_ASSERTION(mTimerRunning, "Huh?!");

    for (uint32_t index = 0; index < mTimeouts.Length(); index++) {
      nsAutoPtr<TimeoutInfo>& info = mTimeouts[index];
      if (info->mId == aId) {
        info->mCanceled = true;
        break;
      }
    }
  }

  return true;
}

bool
WorkerPrivate::RunExpiredTimeouts(JSContext* aCx)
{
  AssertIsOnWorkerThread();

  // We may be called recursively (e.g. close() inside a timeout) or we could
  // have been canceled while this event was pending, bail out if there is
  // nothing to do.
  if (mRunningExpiredTimeouts || !mTimerRunning) {
    return true;
  }

  NS_ASSERTION(mTimer, "Must have a timer!");
  NS_ASSERTION(!mTimeouts.IsEmpty(), "Should have some work to do!");

  bool retval = true;

  AutoPtrComparator<TimeoutInfo> comparator = GetAutoPtrComparator(mTimeouts);
  js::RootedObject global(aCx, JS_GetGlobalObject(aCx));
  JSPrincipals* principal = GetWorkerPrincipal();

  // We want to make sure to run *something*, even if the timer fired a little
  // early. Fudge the value of now to at least include the first timeout.
  const TimeStamp now = NS_MAX(TimeStamp::Now(), mTimeouts[0]->mTargetTime);

  nsAutoTArray<TimeoutInfo*, 10> expiredTimeouts;
  for (uint32_t index = 0; index < mTimeouts.Length(); index++) {
    nsAutoPtr<TimeoutInfo>& info = mTimeouts[index];
    if (info->mTargetTime > now) {
      break;
    }
    expiredTimeouts.AppendElement(info);
  }

  // Guard against recursion.
  mRunningExpiredTimeouts = true;

  // Run expired timeouts.
  for (uint32_t index = 0; index < expiredTimeouts.Length(); index++) {
    TimeoutInfo*& info = expiredTimeouts[index];

    if (info->mCanceled) {
      continue;
    }

    // Always call JS_ReportPendingException if something fails, and if
    // JS_ReportPendingException returns false (i.e. uncatchable exception) then
    // break out of the loop.

    if (info->mTimeoutVal.isString()) {
      JSString* expression = info->mTimeoutVal.toString();

      JS::CompileOptions options(aCx);
      options.setPrincipals(principal)
        .setFileAndLine(info->mFilename.get(), info->mLineNumber);

      size_t stringLength;
      const jschar* string = JS_GetStringCharsAndLength(aCx, expression,
                                                        &stringLength);
      if ((!string || !JS::Evaluate(aCx, global, options, string, stringLength, nullptr)) &&
          !JS_ReportPendingException(aCx)) {
        retval = false;
        break;
      }
    }
    else {
      jsval rval;
      if (!JS_CallFunctionValue(aCx, global, info->mTimeoutVal,
                                info->mExtraArgVals.Length(),
                                info->mExtraArgVals.Elements(), &rval) &&
          !JS_ReportPendingException(aCx)) {
        retval = false;
        break;
      }
    }

    NS_ASSERTION(mRunningExpiredTimeouts, "Someone changed this!");
  }

  // No longer possible to be called recursively.
  mRunningExpiredTimeouts = false;

  // Now remove canceled and expired timeouts from the main list.
  // NB: The timeouts present in expiredTimeouts must have the same order
  // with respect to each other in mTimeouts.  That is, mTimeouts is just
  // expiredTimeouts with extra elements inserted.  There may be unexpired
  // timeouts that have been inserted between the expired timeouts if the
  // timeout event handler called setTimeout/setInterval.
  for (uint32_t index = 0, expiredTimeoutIndex = 0,
       expiredTimeoutLength = expiredTimeouts.Length();
       index < mTimeouts.Length(); ) {
    nsAutoPtr<TimeoutInfo>& info = mTimeouts[index];
    if ((expiredTimeoutIndex < expiredTimeoutLength &&
         info == expiredTimeouts[expiredTimeoutIndex] &&
         ++expiredTimeoutIndex) ||
        info->mCanceled) {
      if (info->mIsInterval && !info->mCanceled) {
        // Reschedule intervals.
        info->mTargetTime = info->mTargetTime + info->mInterval;
        // Don't resort the list here, we'll do that at the end.
        ++index;
      }
      else {
        mTimeouts.RemoveElement(info);
      }
    }
    else {
      // If info did not match the current entry in expiredTimeouts, it
      // shouldn't be there at all.
      NS_ASSERTION(!expiredTimeouts.Contains(info),
                   "Our timeouts are out of order!");
      ++index;
    }
  }

  mTimeouts.Sort(comparator);

  // Either signal the parent that we're no longer using timeouts or reschedule
  // the timer.
  if (mTimeouts.IsEmpty()) {
    if (!ModifyBusyCountFromWorker(aCx, false)) {
      retval = false;
    }
    mTimerRunning = false;
  }
  else if (retval && !RescheduleTimeoutTimer(aCx)) {
    retval = false;
  }

  return retval;
}

bool
WorkerPrivate::RescheduleTimeoutTimer(JSContext* aCx)
{
  AssertIsOnWorkerThread();
  NS_ASSERTION(!mTimeouts.IsEmpty(), "Should have some timeouts!");
  NS_ASSERTION(mTimer, "Should have a timer!");

  double delta =
    (mTimeouts[0]->mTargetTime - TimeStamp::Now()).ToMilliseconds();
  uint32_t delay = delta > 0 ? NS_MIN(delta, double(UINT32_MAX)) : 0;

  nsresult rv = mTimer->InitWithFuncCallback(DummyCallback, nullptr, delay,
                                             nsITimer::TYPE_ONE_SHOT);
  if (NS_FAILED(rv)) {
    JS_ReportError(aCx, "Failed to start timer!");
    return false;
  }

  return true;
}

void
WorkerPrivate::UpdateJSContextOptionsInternal(JSContext* aCx, uint32_t aOptions)
{
  AssertIsOnWorkerThread();

  JS_SetOptions(aCx, aOptions);

  for (uint32_t index = 0; index < mChildWorkers.Length(); index++) {
    mChildWorkers[index]->UpdateJSContextOptions(aCx, aOptions);
  }
}

void
WorkerPrivate::UpdateJSRuntimeHeapSizeInternal(JSContext* aCx,
                                               uint32_t aMaxBytes)
{
  AssertIsOnWorkerThread();

  JS_SetGCParameter(JS_GetRuntime(aCx), JSGC_MAX_BYTES, aMaxBytes);

  for (uint32_t index = 0; index < mChildWorkers.Length(); index++) {
    mChildWorkers[index]->UpdateJSRuntimeHeapSize(aCx, aMaxBytes);
  }
}

#ifdef JS_GC_ZEAL
void
WorkerPrivate::UpdateGCZealInternal(JSContext* aCx, uint8_t aGCZeal)
{
  AssertIsOnWorkerThread();

  uint32_t frequency = aGCZeal <= 2 ? JS_DEFAULT_ZEAL_FREQ : 1;
  JS_SetGCZeal(aCx, aGCZeal, frequency);

  for (uint32_t index = 0; index < mChildWorkers.Length(); index++) {
    mChildWorkers[index]->UpdateGCZeal(aCx, aGCZeal);
  }
}
#endif

void
WorkerPrivate::GarbageCollectInternal(JSContext* aCx, bool aShrinking,
                                      bool aCollectChildren)
{
  AssertIsOnWorkerThread();

  JSRuntime *rt = JS_GetRuntime(aCx);
  js::PrepareForFullGC(rt);
  if (aShrinking) {
    js::ShrinkingGC(rt, js::gcreason::DOM_WORKER);
  }
  else {
    js::GCForReason(rt, js::gcreason::DOM_WORKER);
  }

  if (aCollectChildren) {
    for (uint32_t index = 0; index < mChildWorkers.Length(); index++) {
      mChildWorkers[index]->GarbageCollect(aCx, aShrinking);
    }
  }
}

#ifdef DEBUG
template <class Derived>
void
WorkerPrivateParent<Derived>::AssertIsOnParentThread() const
{
  if (GetParent()) {
    GetParent()->AssertIsOnWorkerThread();
  }
  else {
    AssertIsOnMainThread();
  }
}

template <class Derived>
void
WorkerPrivateParent<Derived>::AssertInnerWindowIsCorrect() const
{
  AssertIsOnParentThread();

  // Only care about top level workers from windows.
  if (mParent || !mWindow) {
    return;
  }

  AssertIsOnMainThread();

  nsPIDOMWindow* outer = mWindow->GetOuterWindow();
  NS_ASSERTION(outer && outer->GetCurrentInnerWindow() == mWindow,
               "Inner window no longer correct!");
}

void
WorkerPrivate::AssertIsOnWorkerThread() const
{
  if (mThread) {
    bool current;
    if (NS_FAILED(mThread->IsOnCurrentThread(&current)) || !current) {
      NS_ERROR("Wrong thread!");
    }
  }
  else {
    NS_ERROR("Trying to assert thread identity after thread has been "
             "shutdown!");
  }
}
#endif

WorkerCrossThreadDispatcher*
WorkerPrivate::GetCrossThreadDispatcher()
{
  mozilla::MutexAutoLock lock(mMutex);
  if (!mCrossThreadDispatcher && mStatus <= Running) {
    mCrossThreadDispatcher = new WorkerCrossThreadDispatcher(this);
  }
  return mCrossThreadDispatcher;
}

bool
WorkerPrivate::GetContentSecurityPolicy(JSContext* aCx,
                                        nsIContentSecurityPolicy** aCSP)
{
  AssertIsOnMainThread();

  // Get the security manager
  nsCOMPtr<nsIScriptSecurityManager> ssm = nsContentUtils::GetSecurityManager();

  if (!ssm) {
    NS_ERROR("Failed to get security manager service");
    return false;
  }

  nsCOMPtr<nsIPrincipal> subjectPrincipal = ssm->GetCxSubjectPrincipal(aCx);
  NS_ASSERTION(subjectPrincipal, "Failed to get subjectPrincipal");

  nsCOMPtr<nsIContentSecurityPolicy> csp;
  nsresult rv = subjectPrincipal->GetCsp(getter_AddRefs(csp));
  if (NS_FAILED(rv)) {
    NS_ERROR("CSP: Failed to get CSP from principal.");
    return false;
  }

  csp.forget(aCSP);
  return true;
}

BEGIN_WORKERS_NAMESPACE

// Force instantiation.
template class WorkerPrivateParent<WorkerPrivate>;

WorkerPrivate*
GetWorkerPrivateFromContext(JSContext* aCx)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  return static_cast<WorkerPrivate*>(JS_GetContextPrivate(aCx));
}

JSStructuredCloneCallbacks*
WorkerStructuredCloneCallbacks(bool aMainRuntime)
{
  return aMainRuntime ?
         &gMainThreadWorkerStructuredCloneCallbacks :
         &gWorkerStructuredCloneCallbacks;
}

JSStructuredCloneCallbacks*
ChromeWorkerStructuredCloneCallbacks(bool aMainRuntime)
{
  return aMainRuntime ?
         &gMainThreadChromeWorkerStructuredCloneCallbacks :
         &gChromeWorkerStructuredCloneCallbacks;
}

END_WORKERS_NAMESPACE

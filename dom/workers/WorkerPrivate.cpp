/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerPrivate.h"

#include "amIAddonManager.h"
#include "nsIClassInfo.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIConsoleService.h"
#include "nsIDOMDOMException.h"
#include "nsIDOMEvent.h"
#include "nsIDOMFile.h"
#include "nsIDOMMessageEvent.h"
#include "nsIDocument.h"
#include "nsIDocShell.h"
#include "nsIMemoryReporter.h"
#include "nsIPermissionManager.h"
#include "nsIScriptError.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptSecurityManager.h"
#include "nsPIDOMWindow.h"
#include "nsITextToSubURI.h"
#include "nsIThreadInternal.h"
#include "nsITimer.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIXPConnect.h"

#include <algorithm>
#include "jsfriendapi.h"
#include "js/OldDebugAPI.h"
#include "js/MemoryMetrics.h"
#include "mozilla/Assertions.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/Likely.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/ErrorEvent.h"
#include "mozilla/dom/ErrorEventBinding.h"
#include "mozilla/dom/Exceptions.h"
#include "mozilla/dom/FunctionBinding.h"
#include "mozilla/dom/ImageData.h"
#include "mozilla/dom/ImageDataBinding.h"
#include "mozilla/dom/MessageEventBinding.h"
#include "mozilla/dom/MessagePortList.h"
#include "mozilla/dom/WorkerBinding.h"
#include "mozilla/Preferences.h"
#include "nsAlgorithm.h"
#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "nsError.h"
#include "nsEventDispatcher.h"
#include "nsDOMMessageEvent.h"
#include "nsDOMJSUtils.h"
#include "nsHostObjectProtocolHandler.h"
#include "nsJSEnvironment.h"
#include "nsJSUtils.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "nsProxyRelease.h"
#include "nsSandboxFlags.h"
#include "xpcpublic.h"

#ifdef ANDROID
#include <android/log.h>
#endif

#ifdef DEBUG
#include "nsThreadManager.h"
#endif

#include "File.h"
#include "MessagePort.h"
#include "Navigator.h"
#include "Principal.h"
#include "RuntimeService.h"
#include "ScriptLoader.h"
#include "SharedWorker.h"
#include "WorkerFeature.h"
#include "WorkerRunnable.h"
#include "WorkerScope.h"

// JS_MaybeGC will run once every second during normal execution.
#define PERIODIC_GC_TIMER_DELAY_SEC 1

// A shrinking GC will run five seconds after the last event is processed.
#define IDLE_GC_TIMER_DELAY_SEC 5

#define PREF_WORKERS_ENABLED "dom.workers.enabled"

#ifdef WORKER_LOGGING
#define LOG(_args) do { printf _args ; fflush(stdout); } while (0)
#else
#define LOG(_args) do { } while (0)
#endif

using namespace mozilla;
using namespace mozilla::dom;
USING_WORKERS_NAMESPACE

MOZ_DEFINE_MALLOC_SIZE_OF(JsWorkerMallocSizeOf)

#ifdef DEBUG

BEGIN_WORKERS_NAMESPACE

void
AssertIsOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
}

END_WORKERS_NAMESPACE

#endif

namespace {

#ifdef DEBUG

const nsIID kDEBUGWorkerEventTargetIID = {
  0xccaba3fa, 0x5be2, 0x4de2, { 0xba, 0x87, 0x3b, 0x3b, 0x5b, 0x1d, 0x5, 0xfb }
};

#endif

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

// This class is used to wrap any runnables that the worker receives via the
// nsIEventTarget::Dispatch() method (either from NS_DispatchToCurrentThread or
// from the worker's EventTarget).
class ExternalRunnableWrapper MOZ_FINAL : public WorkerRunnable
{
  nsCOMPtr<nsICancelableRunnable> mWrappedRunnable;

public:
  ExternalRunnableWrapper(WorkerPrivate* aWorkerPrivate,
                          nsICancelableRunnable* aWrappedRunnable)
  : WorkerRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
    mWrappedRunnable(aWrappedRunnable)
  {
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(aWrappedRunnable);
  }

  NS_DECL_ISUPPORTS_INHERITED

private:
  ~ExternalRunnableWrapper()
  { }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    nsresult rv = mWrappedRunnable->Run();
    if (NS_FAILED(rv)) {
      if (!JS_IsExceptionPending(aCx)) {
        Throw(aCx, rv);
      }
      return false;
    }
    return true;
  }

  NS_IMETHOD
  Cancel() MOZ_OVERRIDE
  {
    nsresult rv = mWrappedRunnable->Cancel();
    nsresult rv2 = WorkerRunnable::Cancel();
    return NS_FAILED(rv) ? rv : rv2;
  }
};

struct WindowAction
{
  nsPIDOMWindow* mWindow;
  JSContext* mJSContext;
  bool mDefaultAction;

  WindowAction(nsPIDOMWindow* aWindow, JSContext* aJSContext)
  : mWindow(aWindow), mJSContext(aJSContext), mDefaultAction(true)
  {
    MOZ_ASSERT(aJSContext);
  }

  WindowAction(nsPIDOMWindow* aWindow)
  : mWindow(aWindow), mJSContext(nullptr), mDefaultAction(true)
  { }

  bool
  operator==(const WindowAction& aOther) const
  {
    return mWindow == aOther.mWindow;
  }
};

void
LogErrorToConsole(const nsAString& aMessage,
                  const nsAString& aFilename,
                  const nsAString& aLine,
                  uint32_t aLineNumber,
                  uint32_t aColumnNumber,
                  uint32_t aFlags,
                  uint64_t aInnerWindowId)
{
  AssertIsOnMainThread();

  nsCOMPtr<nsIScriptError> scriptError =
    do_CreateInstance(NS_SCRIPTERROR_CONTRACTID);
  NS_WARN_IF_FALSE(scriptError, "Failed to create script error!");

  if (scriptError) {
    if (NS_FAILED(scriptError->InitWithWindowID(aMessage, aFilename, aLine,
                                                aLineNumber, aColumnNumber,
                                                aFlags, "Web Worker",
                                                aInnerWindowId))) {
      NS_WARNING("Failed to init script error!");
      scriptError = nullptr;
    }
  }

  nsCOMPtr<nsIConsoleService> consoleService =
    do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  NS_WARN_IF_FALSE(consoleService, "Failed to get console service!");

  if (consoleService) {
    if (scriptError) {
      if (NS_SUCCEEDED(consoleService->LogMessage(scriptError))) {
        return;
      }
      NS_WARNING("LogMessage failed!");
    } else if (NS_SUCCEEDED(consoleService->LogStringMessage(
                                                    aMessage.BeginReading()))) {
      return;
    }
    NS_WARNING("LogStringMessage failed!");
  }

  NS_ConvertUTF16toUTF8 msg(aMessage);
  NS_ConvertUTF16toUTF8 filename(aFilename);

  static const char kErrorString[] = "JS error in Web Worker: %s [%s:%u]";

#ifdef ANDROID
  __android_log_print(ANDROID_LOG_INFO, "Gecko", kErrorString, msg.get(),
                      filename.get(), aLineNumber);
#endif

  fprintf(stderr, kErrorString, msg.get(), filename.get(), aLineNumber);
  fflush(stderr);
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
      JS::Rooted<JS::Value> dataArray(aCx);
      if (!JS_ReadUint32Pair(aReader, &width, &height) ||
          !JS_ReadTypedArray(aReader, dataArray.address()))
      {
        return nullptr;
      }
      MOZ_ASSERT(dataArray.isObject());

      // Construct the ImageData.
      nsRefPtr<ImageData> imageData = new ImageData(width, height,
                                                    dataArray.toObject());
      // Wrap it in a JS::Value.
      JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));
      if (!global) {
        return nullptr;
      }
      return imageData->WrapObject(aCx, global);
    }

    Error(aCx, 0);
    return nullptr;
  }

  static bool
  Write(JSContext* aCx, JSStructuredCloneWriter* aWriter,
        JS::Handle<JSObject*> aObj, void* aClosure)
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
    {
      ImageData* imageData = nullptr;
      if (NS_SUCCEEDED(UNWRAP_OBJECT(ImageData, aObj, imageData))) {
        // Prepare the ImageData internals.
        uint32_t width = imageData->Width();
        uint32_t height = imageData->Height();
        JS::Rooted<JSObject*> dataArray(aCx, imageData->GetDataObject());

        // Write the internals to the stream.
        JSAutoCompartment ac(aCx, dataArray);
        return JS_WriteUint32Pair(aWriter, SCTAG_DOM_IMAGEDATA, 0) &&
               JS_WriteUint32Pair(aWriter, width, height) &&
               JS_WriteTypedArray(aWriter, JS::ObjectValue(*dataArray));
      }
    }

    Error(aCx, 0);
    return false;
  }

  static void
  Error(JSContext* aCx, uint32_t /* aErrorId */)
  {
    Throw(aCx, NS_ERROR_DOM_DATA_CLONE_ERR);
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
        JS::Rooted<JS::Value> wrappedFile(aCx);
        JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));
        nsresult rv = nsContentUtils::WrapNative(aCx, global, file,
                                                 &NS_GET_IID(nsIDOMFile),
                                                 &wrappedFile);
        if (NS_FAILED(rv)) {
          Error(aCx, nsIDOMDOMException::DATA_CLONE_ERR);
          return nullptr;
        }

        return &wrappedFile.toObject();
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
        JS::Rooted<JS::Value> wrappedBlob(aCx);
        JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));
        nsresult rv = nsContentUtils::WrapNative(aCx, global, blob,
                                                 &NS_GET_IID(nsIDOMBlob),
                                                 &wrappedBlob);
        if (NS_FAILED(rv)) {
          Error(aCx, nsIDOMDOMException::DATA_CLONE_ERR);
          return nullptr;
        }

        return &wrappedBlob.toObject();
      }
    }

    JS_ClearPendingException(aCx);
    return NS_DOMReadStructuredClone(aCx, aReader, aTag, aData, nullptr);
  }

  static bool
  Write(JSContext* aCx, JSStructuredCloneWriter* aWriter,
        JS::Handle<JSObject*> aObj, void* aClosure)
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

  static bool
  Write(JSContext* aCx, JSStructuredCloneWriter* aWriter,
        JS::Handle<JSObject*> aObj, void* aClosure)
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

  static bool
  Write(JSContext* aCx, JSStructuredCloneWriter* aWriter,
        JS::Handle<JSObject*> aObj, void* aClosure)
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

class MainThreadReleaseRunnable MOZ_FINAL : public nsRunnable
{
  nsTArray<nsCOMPtr<nsISupports>> mDoomed;
  nsTArray<nsCString> mHostObjectURIs;

public:
  MainThreadReleaseRunnable(nsTArray<nsCOMPtr<nsISupports>>& aDoomed,
                            nsTArray<nsCString>& aHostObjectURIs)
  {
    mDoomed.SwapElements(aDoomed);
    mHostObjectURIs.SwapElements(aHostObjectURIs);
  }

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD
  Run() MOZ_OVERRIDE
  {
    mDoomed.Clear();

    for (uint32_t index = 0; index < mHostObjectURIs.Length(); index++) {
      nsHostObjectProtocolHandler::RemoveDataEntry(mHostObjectURIs[index]);
    }

    return NS_OK;
  }

private:
  ~MainThreadReleaseRunnable()
  { }
};

class WorkerFinishedRunnable MOZ_FINAL : public WorkerControlRunnable
{
  WorkerPrivate* mFinishedWorker;

public:
  WorkerFinishedRunnable(WorkerPrivate* aWorkerPrivate,
                         WorkerPrivate* aFinishedWorker)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
    mFinishedWorker(aFinishedWorker)
  { }

private:
  virtual bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    // Silence bad assertions.
    return true;
  }

  virtual void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult) MOZ_OVERRIDE
  {
    // Silence bad assertions.
  }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    nsTArray<nsCOMPtr<nsISupports>> doomed;
    mFinishedWorker->ForgetMainThreadObjects(doomed);

    nsTArray<nsCString> hostObjectURIs;
    mFinishedWorker->StealHostObjectURIs(hostObjectURIs);

    nsRefPtr<MainThreadReleaseRunnable> runnable =
      new MainThreadReleaseRunnable(doomed, hostObjectURIs);
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

class TopLevelWorkerFinishedRunnable MOZ_FINAL : public nsRunnable
{
  WorkerPrivate* mFinishedWorker;

public:
  TopLevelWorkerFinishedRunnable(WorkerPrivate* aFinishedWorker)
  : mFinishedWorker(aFinishedWorker)
  {
    aFinishedWorker->AssertIsOnWorkerThread();
  }

  NS_DECL_ISUPPORTS_INHERITED

private:
  NS_IMETHOD
  Run() MOZ_OVERRIDE
  {
    AssertIsOnMainThread();

    AutoSafeJSContext cx;
    JSAutoRequest ar(cx);

    mFinishedWorker->Finish(cx);

    RuntimeService* runtime = RuntimeService::GetService();
    NS_ASSERTION(runtime, "This should never be null!");

    runtime->UnregisterWorker(cx, mFinishedWorker);

    nsTArray<nsCOMPtr<nsISupports> > doomed;
    mFinishedWorker->ForgetMainThreadObjects(doomed);

    nsTArray<nsCString> hostObjectURIs;
    mFinishedWorker->StealHostObjectURIs(hostObjectURIs);

    nsRefPtr<MainThreadReleaseRunnable> runnable =
      new MainThreadReleaseRunnable(doomed, hostObjectURIs);
    if (NS_FAILED(NS_DispatchToCurrentThread(runnable))) {
      NS_WARNING("Failed to dispatch, going to leak!");
    }

    mFinishedWorker->Release();

    return NS_OK;
  }
};

class ModifyBusyCountRunnable MOZ_FINAL : public WorkerControlRunnable
{
  bool mIncrease;

public:
  ModifyBusyCountRunnable(WorkerPrivate* aWorkerPrivate, bool aIncrease)
  : WorkerControlRunnable(aWorkerPrivate, ParentThreadUnchangedBusyCount),
    mIncrease(aIncrease)
  { }

private:
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    return aWorkerPrivate->ModifyBusyCount(aCx, mIncrease);
  }

  virtual void
  PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate, bool aRunResult)
          MOZ_OVERRIDE
  {
    if (mIncrease) {
      WorkerControlRunnable::PostRun(aCx, aWorkerPrivate, aRunResult);
      return;
    }
    // Don't do anything here as it's possible that aWorkerPrivate has been
    // deleted.
  }
};

class CompileScriptRunnable MOZ_FINAL : public WorkerRunnable
{
public:
  CompileScriptRunnable(WorkerPrivate* aWorkerPrivate)
  : WorkerRunnable(aWorkerPrivate, WorkerThreadModifyBusyCount)
  { }

private:
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    JS::Rooted<JSObject*> global(aCx,
      aWorkerPrivate->CreateGlobalScope(aCx));
    if (!global) {
      NS_WARNING("Failed to make global!");
      return false;
    }

    JSAutoCompartment ac(aCx, global);
    return scriptloader::LoadWorkerScript(aCx);
  }
};

class CloseEventRunnable MOZ_FINAL : public WorkerRunnable
{
public:
  CloseEventRunnable(WorkerPrivate* aWorkerPrivate)
  : WorkerRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
  { }

private:
  virtual bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    MOZ_ASSUME_UNREACHABLE("Don't call Dispatch() on CloseEventRunnable!");
  }

  virtual void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult) MOZ_OVERRIDE
  {
    MOZ_ASSUME_UNREACHABLE("Don't call Dispatch() on CloseEventRunnable!");
  }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    JS::Rooted<JSObject*> target(aCx, JS::CurrentGlobalOrNull(aCx));
    NS_ASSERTION(target, "This must never be null!");

    aWorkerPrivate->CloseHandlerStarted();

    WorkerGlobalScope* globalScope = aWorkerPrivate->GlobalScope();

    nsCOMPtr<nsIDOMEvent> event;
    nsresult rv =
      NS_NewDOMEvent(getter_AddRefs(event), globalScope, nullptr, nullptr);
    if (NS_FAILED(rv)) {
      Throw(aCx, rv);
      return false;
    }

    rv = event->InitEvent(NS_LITERAL_STRING("close"), false, false);
    if (NS_FAILED(rv)) {
      Throw(aCx, rv);
      return false;
    }

    event->SetTrusted(true);

    globalScope->DispatchDOMEvent(nullptr, event, nullptr, nullptr);

    return true;
  }

  virtual void
  PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate, bool aRunResult)
          MOZ_OVERRIDE
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

class MessageEventRunnable MOZ_FINAL : public WorkerRunnable
{
  JSAutoStructuredCloneBuffer mBuffer;
  nsTArray<nsCOMPtr<nsISupports> > mClonedObjects;
  uint64_t mMessagePortSerial;
  bool mToMessagePort;

public:
  MessageEventRunnable(WorkerPrivate* aWorkerPrivate,
                       TargetAndBusyBehavior aBehavior,
                       JSAutoStructuredCloneBuffer& aData,
                       nsTArray<nsCOMPtr<nsISupports> >& aClonedObjects,
                       bool aToMessagePort, uint64_t aMessagePortSerial)
  : WorkerRunnable(aWorkerPrivate, aBehavior),
    mMessagePortSerial(aMessagePortSerial), mToMessagePort(aToMessagePort)
  {
    mBuffer.swap(aData);
    mClonedObjects.SwapElements(aClonedObjects);
  }

  bool
  DispatchDOMEvent(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                   nsDOMEventTargetHelper* aTarget, bool aIsMainThread)
  {
    // Release reference to objects that were AddRef'd for
    // cloning into worker when array goes out of scope.
    nsTArray<nsCOMPtr<nsISupports>> clonedObjects;
    clonedObjects.SwapElements(mClonedObjects);

    JS::Rooted<JS::Value> messageData(aCx);
    if (!mBuffer.read(aCx, &messageData,
                      workers::WorkerStructuredCloneCallbacks(aIsMainThread))) {
      xpc::Throw(aCx, NS_ERROR_DOM_DATA_CLONE_ERR);
      return false;
    }

    nsRefPtr<nsDOMMessageEvent> event =
      new nsDOMMessageEvent(aTarget, nullptr, nullptr);
    nsresult rv =
      event->InitMessageEvent(NS_LITERAL_STRING("message"),
                              false /* non-bubbling */,
                              true /* cancelable */,
                              messageData,
                              EmptyString(),
                              EmptyString(),
                              nullptr);
    if (NS_FAILED(rv)) {
      xpc::Throw(aCx, rv);
      return false;
    }

    event->SetTrusted(true);

    nsCOMPtr<nsIDOMEvent> domEvent = do_QueryObject(event);

    nsEventStatus dummy = nsEventStatus_eIgnore;
    aTarget->DispatchDOMEvent(nullptr, domEvent, nullptr, &dummy);
    return true;
  }

private:
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    MOZ_ASSERT_IF(mToMessagePort, aWorkerPrivate->IsSharedWorker());

    if (mBehavior == ParentThreadUnchangedBusyCount) {
      // Don't fire this event if the JS object has been disconnected from the
      // private object.
      if (!aWorkerPrivate->IsAcceptingEvents()) {
        return true;
      }

      if (mToMessagePort) {
        return
          aWorkerPrivate->DispatchMessageEventToMessagePort(aCx,
                                                            mMessagePortSerial,
                                                            mBuffer,
                                                            mClonedObjects);
      }

      if (aWorkerPrivate->IsSuspended()) {
        aWorkerPrivate->QueueRunnable(this);
        return true;
      }

      aWorkerPrivate->AssertInnerWindowIsCorrect();

      return DispatchDOMEvent(aCx, aWorkerPrivate, aWorkerPrivate,
                              !aWorkerPrivate->GetParent());
    }

    MOZ_ASSERT(aWorkerPrivate == GetWorkerPrivateFromContext(aCx));

    if (mToMessagePort) {
      nsRefPtr<workers::MessagePort> port =
        aWorkerPrivate->GetMessagePort(mMessagePortSerial);
      if (!port) {
        // Must have been closed already.
        return true;
      }
      return DispatchDOMEvent(aCx, aWorkerPrivate, port, false);
    }

    return DispatchDOMEvent(aCx, aWorkerPrivate, aWorkerPrivate->GlobalScope(),
                            false);
  }
};

class NotifyRunnable MOZ_FINAL : public WorkerControlRunnable
{
  Status mStatus;

public:
  NotifyRunnable(WorkerPrivate* aWorkerPrivate, Status aStatus)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
    mStatus(aStatus)
  {
    MOZ_ASSERT(aStatus == Terminating || aStatus == Canceling ||
               aStatus == Killing);
  }

private:
  virtual bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    // Modify here, but not in PostRun! This busy count addition will be matched
    // by the CloseEventRunnable.
    return aWorkerPrivate->ModifyBusyCount(aCx, true);
  }

  virtual void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult) MOZ_OVERRIDE
  {
    if (!aDispatchResult) {
      // We couldn't dispatch to the worker, which means it's already dead.
      // Undo the busy count modification.
      aWorkerPrivate->ModifyBusyCount(aCx, false);
    }
  }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    return aWorkerPrivate->NotifyInternal(aCx, mStatus);
  }
};

class CloseRunnable MOZ_FINAL : public WorkerControlRunnable
{
public:
  CloseRunnable(WorkerPrivate* aWorkerPrivate)
  : WorkerControlRunnable(aWorkerPrivate, ParentThreadUnchangedBusyCount)
  { }

private:
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    // This busy count will be matched by the CloseEventRunnable.
    return aWorkerPrivate->ModifyBusyCount(aCx, true) &&
           aWorkerPrivate->Close(aCx);
  }
};

class SuspendRunnable MOZ_FINAL : public WorkerControlRunnable
{
public:
  SuspendRunnable(WorkerPrivate* aWorkerPrivate)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
  { }

private:
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    return aWorkerPrivate->SuspendInternal(aCx);
  }
};

class ResumeRunnable MOZ_FINAL : public WorkerControlRunnable
{
public:
  ResumeRunnable(WorkerPrivate* aWorkerPrivate)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
  { }

private:
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    return aWorkerPrivate->ResumeInternal(aCx);
  }
};

class ReportErrorRunnable MOZ_FINAL : public WorkerRunnable
{
  nsString mMessage;
  nsString mFilename;
  nsString mLine;
  uint32_t mLineNumber;
  uint32_t mColumnNumber;
  uint32_t mFlags;
  uint32_t mErrorNumber;

public:
  // aWorkerPrivate is the worker thread we're on (or the main thread, if null)
  // aTarget is the worker object that we are going to fire an error at
  // (if any).
  static bool
  ReportError(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
              bool aFireAtScope, WorkerPrivate* aTarget,
              const nsString& aMessage, const nsString& aFilename,
              const nsString& aLine, uint32_t aLineNumber,
              uint32_t aColumnNumber, uint32_t aFlags,
              uint32_t aErrorNumber, uint64_t aInnerWindowId)
  {
    if (aWorkerPrivate) {
      aWorkerPrivate->AssertIsOnWorkerThread();
    }
    else {
      AssertIsOnMainThread();
    }

    JS::Rooted<JSString*> message(aCx, JS_NewUCStringCopyN(aCx, aMessage.get(),
                                                           aMessage.Length()));
    if (!message) {
      return false;
    }

    JS::Rooted<JSString*> filename(aCx, JS_NewUCStringCopyN(aCx, aFilename.get(),
                                                            aFilename.Length()));
    if (!filename) {
      return false;
    }

    // We should not fire error events for warnings but instead make sure that
    // they show up in the error console.
    if (!JSREPORT_IS_WARNING(aFlags)) {
      // First fire an ErrorEvent at the worker.
      if (aTarget) {
        ErrorEventInit init;
        init.mMessage = aMessage;
        init.mFilename = aFilename;
        init.mLineno = aLineNumber;
        init.mCancelable = true;

        nsRefPtr<ErrorEvent> event =
          ErrorEvent::Constructor(aTarget, NS_LITERAL_STRING("error"), init);

        event->SetTrusted(true);

        nsEventStatus status = nsEventStatus_eIgnore;
        aTarget->DispatchDOMEvent(nullptr, event, nullptr, &status);

        if (status == nsEventStatus_eConsumeNoDefault) {
          return true;
        }
      }

      // Now fire an event at the global object, but don't do that if the error
      // code is too much recursion and this is the same script threw the error.
      if (aFireAtScope && (aTarget || aErrorNumber != JSMSG_OVER_RECURSED)) {
        JS::Rooted<JSObject*> target(aCx, JS::CurrentGlobalOrNull(aCx));
        NS_ASSERTION(target, "This should never be null!");

        nsEventStatus status = nsEventStatus_eIgnore;
        nsIScriptGlobalObject* sgo;

        if (aWorkerPrivate) {
          WorkerGlobalScope* globalTarget = aWorkerPrivate->GlobalScope();
          MOZ_ASSERT(target == globalTarget->GetWrapperPreserveColor());

          // Icky, we have to fire an InternalScriptErrorEvent...
          MOZ_ASSERT(!NS_IsMainThread());
          InternalScriptErrorEvent event(true, NS_USER_DEFINED_EVENT);
          event.lineNr = aLineNumber;
          event.errorMsg = aMessage.get();
          event.fileName = aFilename.get();
          event.typeString = NS_LITERAL_STRING("error");

          nsIDOMEventTarget* target = static_cast<nsIDOMEventTarget*>(globalTarget);
          if (NS_FAILED(nsEventDispatcher::Dispatch(target, nullptr, &event,
                                                    nullptr, &status))) {
            NS_WARNING("Failed to dispatch worker thread error event!");
            status = nsEventStatus_eIgnore;
          }
        }
        else if ((sgo = nsJSUtils::GetStaticScriptGlobal(target))) {
          // Icky, we have to fire an InternalScriptErrorEvent...
          MOZ_ASSERT(NS_IsMainThread());
          InternalScriptErrorEvent event(true, NS_LOAD_ERROR);
          event.lineNr = aLineNumber;
          event.errorMsg = aMessage.get();
          event.fileName = aFilename.get();

          if (NS_FAILED(sgo->HandleScriptError(&event, &status))) {
            NS_WARNING("Failed to dispatch main thread error event!");
            status = nsEventStatus_eIgnore;
          }
        }

        // Was preventDefault() called?
        if (status == nsEventStatus_eConsumeNoDefault) {
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
    LogErrorToConsole(aMessage, aFilename, aLine, aLineNumber, aColumnNumber,
                      aFlags, aInnerWindowId);
    return true;
  }

private:
  ReportErrorRunnable(WorkerPrivate* aWorkerPrivate, const nsString& aMessage,
                      const nsString& aFilename, const nsString& aLine,
                      uint32_t aLineNumber, uint32_t aColumnNumber,
                      uint32_t aFlags, uint32_t aErrorNumber)
  : WorkerRunnable(aWorkerPrivate, ParentThreadUnchangedBusyCount),
    mMessage(aMessage), mFilename(aFilename), mLine(aLine),
    mLineNumber(aLineNumber), mColumnNumber(aColumnNumber), mFlags(aFlags),
    mErrorNumber(aErrorNumber)
  { }

  virtual void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult) MOZ_OVERRIDE
  {
    aWorkerPrivate->AssertIsOnWorkerThread();

    // Dispatch may fail if the worker was canceled, no need to report that as
    // an error, so don't call base class PostDispatch.
  }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    // Don't fire this event if the JS object has been disconnected from the
    // private object.
    if (!aWorkerPrivate->IsAcceptingEvents()) {
      return true;
    }

    JS::Rooted<JSObject*> target(aCx, aWorkerPrivate->GetWrapper());

    uint64_t innerWindowId;
    bool fireAtScope = true;

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

      if (aWorkerPrivate->IsSharedWorker()) {
        aWorkerPrivate->BroadcastErrorToSharedWorkers(aCx, mMessage, mFilename,
                                                      mLine, mLineNumber,
                                                      mColumnNumber, mFlags);
        return true;
      }

      aWorkerPrivate->AssertInnerWindowIsCorrect();

      innerWindowId = aWorkerPrivate->GetInnerWindowId();
    }

    return ReportError(aCx, parent, fireAtScope, aWorkerPrivate, mMessage,
                       mFilename, mLine, mLineNumber, mColumnNumber, mFlags,
                       mErrorNumber, innerWindowId);
  }
};

class TimerRunnable MOZ_FINAL : public WorkerRunnable
{
public:
  TimerRunnable(WorkerPrivate* aWorkerPrivate)
  : WorkerRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
  { }

private:
  virtual bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    // Silence bad assertions.
    return true;
  }

  virtual void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult) MOZ_OVERRIDE
  {
    // Silence bad assertions.
  }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    return aWorkerPrivate->RunExpiredTimeouts(aCx);
  }
};

void
DummyCallback(nsITimer* aTimer, void* aClosure)
{
  // Nothing!
}

class TimerThreadEventTarget MOZ_FINAL : public nsIEventTarget
{
  WorkerPrivate* mWorkerPrivate;
  nsRefPtr<WorkerRunnable> mWorkerRunnable;

public:
  TimerThreadEventTarget(WorkerPrivate* aWorkerPrivate,
                         WorkerRunnable* aWorkerRunnable)
  : mWorkerPrivate(aWorkerPrivate), mWorkerRunnable(aWorkerRunnable)
  {
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(aWorkerRunnable);
  }

  NS_DECL_THREADSAFE_ISUPPORTS

protected:
  NS_IMETHOD
  Dispatch(nsIRunnable* aRunnable, uint32_t aFlags) MOZ_OVERRIDE
  {
    // This should only happen on the timer thread.
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(aFlags == nsIEventTarget::DISPATCH_NORMAL);

    nsRefPtr<TimerThreadEventTarget> kungFuDeathGrip = this;

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
  IsOnCurrentThread(bool* aIsOnCurrentThread) MOZ_OVERRIDE
  {
    MOZ_ASSERT(aIsOnCurrentThread);

    nsresult rv = mWorkerPrivate->IsOnCurrentThread(aIsOnCurrentThread);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    return NS_OK;
  }
};

class KillCloseEventRunnable MOZ_FINAL : public WorkerRunnable
{
  nsCOMPtr<nsITimer> mTimer;

  class KillScriptRunnable MOZ_FINAL : public WorkerControlRunnable
  {
  public:
    KillScriptRunnable(WorkerPrivate* aWorkerPrivate)
    : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
    { }

  private:
    virtual bool
    PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
    {
      // Silence bad assertions, this is dispatched from the timer thread.
      return true;
    }

    virtual void
    PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                 bool aDispatchResult) MOZ_OVERRIDE
    {
      // Silence bad assertions, this is dispatched from the timer thread.
    }

    virtual bool
    WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
    {
      // Kill running script.
      return false;
    }
  };

public:
  KillCloseEventRunnable(WorkerPrivate* aWorkerPrivate)
  : WorkerRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
  { }

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

    nsRefPtr<TimerThreadEventTarget> target =
      new TimerThreadEventTarget(mWorkerPrivate, runnable);

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

private:
  ~KillCloseEventRunnable()
  {
    if (mTimer) {
      mTimer->Cancel();
    }
  }

  virtual bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    MOZ_ASSUME_UNREACHABLE("Don't call Dispatch() on KillCloseEventRunnable!");
  }

  virtual void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult) MOZ_OVERRIDE
  {
    MOZ_ASSUME_UNREACHABLE("Don't call Dispatch() on KillCloseEventRunnable!");
  }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    if (mTimer) {
      mTimer->Cancel();
      mTimer = nullptr;
    }

    return true;
  }
};

class UpdateJSContextOptionsRunnable MOZ_FINAL : public WorkerControlRunnable
{
  JS::ContextOptions mContentOptions;
  JS::ContextOptions mChromeOptions;

public:
  UpdateJSContextOptionsRunnable(WorkerPrivate* aWorkerPrivate,
                                 const JS::ContextOptions& aContentOptions,
                                 const JS::ContextOptions& aChromeOptions)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
    mContentOptions(aContentOptions), mChromeOptions(aChromeOptions)
  { }

private:
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    aWorkerPrivate->UpdateJSContextOptionsInternal(aCx, mContentOptions,
                                                   mChromeOptions);
    return true;
  }
};

class UpdatePreferenceRunnable MOZ_FINAL : public WorkerControlRunnable
{
  WorkerPreference mPref;
  bool mValue;

public:
  UpdatePreferenceRunnable(WorkerPrivate* aWorkerPrivate,
                           WorkerPreference aPref,
                           bool aValue)
    : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
      mPref(aPref),
      mValue(aValue)
  { }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    aWorkerPrivate->UpdatePreferenceInternal(aCx, mPref, mValue);
    return true;
  }
};

class UpdateJSWorkerMemoryParameterRunnable MOZ_FINAL :
  public WorkerControlRunnable
{
  uint32_t mValue;
  JSGCParamKey mKey;

public:
  UpdateJSWorkerMemoryParameterRunnable(WorkerPrivate* aWorkerPrivate,
                                        JSGCParamKey aKey,
                                        uint32_t aValue)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
    mValue(aValue), mKey(aKey)
  { }

private:
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    aWorkerPrivate->UpdateJSWorkerMemoryParameterInternal(aCx, mKey, mValue);
    return true;
  }
};

#ifdef JS_GC_ZEAL
class UpdateGCZealRunnable MOZ_FINAL : public WorkerControlRunnable
{
  uint8_t mGCZeal;
  uint32_t mFrequency;

public:
  UpdateGCZealRunnable(WorkerPrivate* aWorkerPrivate,
                       uint8_t aGCZeal,
                       uint32_t aFrequency)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
    mGCZeal(aGCZeal), mFrequency(aFrequency)
  { }

private:
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    aWorkerPrivate->UpdateGCZealInternal(aCx, mGCZeal, mFrequency);
    return true;
  }
};
#endif

class UpdateJITHardeningRunnable MOZ_FINAL : public WorkerControlRunnable
{
  bool mJITHardening;

public:
  UpdateJITHardeningRunnable(WorkerPrivate* aWorkerPrivate, bool aJITHardening)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
    mJITHardening(aJITHardening)
  { }

private:
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    aWorkerPrivate->UpdateJITHardeningInternal(aCx, mJITHardening);
    return true;
  }
};

class GarbageCollectRunnable MOZ_FINAL : public WorkerControlRunnable
{
  bool mShrinking;
  bool mCollectChildren;

public:
  GarbageCollectRunnable(WorkerPrivate* aWorkerPrivate, bool aShrinking,
                         bool aCollectChildren)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
    mShrinking(aShrinking), mCollectChildren(aCollectChildren)
  { }

private:
  virtual bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    // Silence bad assertions, this can be dispatched from either the main
    // thread or the timer thread..
    return true;
  }

  virtual void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                bool aDispatchResult) MOZ_OVERRIDE
  {
    // Silence bad assertions, this can be dispatched from either the main
    // thread or the timer thread..
  }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    aWorkerPrivate->GarbageCollectInternal(aCx, mShrinking, mCollectChildren);
    return true;
  }
};

class CycleCollectRunnable : public WorkerControlRunnable
{
  bool mCollectChildren;

public:
  CycleCollectRunnable(WorkerPrivate* aWorkerPrivate, bool aCollectChildren)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
    mCollectChildren(aCollectChildren)
  { }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    aWorkerPrivate->CycleCollectInternal(aCx, mCollectChildren);
    return true;
  }
};

class OfflineStatusChangeRunnable : public WorkerRunnable
{
public:
  OfflineStatusChangeRunnable(WorkerPrivate* aWorkerPrivate, bool aIsOffline)
    : WorkerRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
      mIsOffline(aIsOffline)
  {
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    aWorkerPrivate->OfflineStatusChangeEventInternal(aCx, mIsOffline);
    return true;
  }

private:
  bool mIsOffline;
};

class WorkerJSRuntimeStats : public JS::RuntimeStats
{
  const nsACString& mRtPath;

public:
  WorkerJSRuntimeStats(const nsACString& aRtPath)
  : JS::RuntimeStats(JsWorkerMallocSizeOf), mRtPath(aRtPath)
  { }

  ~WorkerJSRuntimeStats()
  {
    for (size_t i = 0; i != zoneStatsVector.length(); i++) {
      delete static_cast<xpc::ZoneStatsExtras*>(zoneStatsVector[i].extra);
    }

    for (size_t i = 0; i != compartmentStatsVector.length(); i++) {
      delete static_cast<xpc::CompartmentStatsExtras*>(compartmentStatsVector[i].extra);
    }
  }

  virtual void
  initExtraZoneStats(JS::Zone* aZone,
                     JS::ZoneStats* aZoneStats)
                     MOZ_OVERRIDE
  {
    MOZ_ASSERT(!aZoneStats->extra);

    // ReportJSRuntimeExplicitTreeStats expects that
    // aZoneStats->extra is a xpc::ZoneStatsExtras pointer.
    xpc::ZoneStatsExtras* extras = new xpc::ZoneStatsExtras;
    extras->pathPrefix = mRtPath;
    extras->pathPrefix += nsPrintfCString("zone(0x%p)/", (void *)aZone);
    aZoneStats->extra = extras;
  }

  virtual void
  initExtraCompartmentStats(JSCompartment* aCompartment,
                            JS::CompartmentStats* aCompartmentStats)
                            MOZ_OVERRIDE
  {
    MOZ_ASSERT(!aCompartmentStats->extra);

    // ReportJSRuntimeExplicitTreeStats expects that
    // aCompartmentStats->extra is a xpc::CompartmentStatsExtras pointer.
    xpc::CompartmentStatsExtras* extras = new xpc::CompartmentStatsExtras;

    // This is the |jsPathPrefix|.  Each worker has exactly two compartments:
    // one for atoms, and one for everything else.
    extras->jsPathPrefix.Assign(mRtPath);
    extras->jsPathPrefix += nsPrintfCString("zone(0x%p)/",
                                            (void *)js::GetCompartmentZone(aCompartment));
    extras->jsPathPrefix += js::IsAtomsCompartment(aCompartment)
                            ? NS_LITERAL_CSTRING("compartment(web-worker-atoms)/")
                            : NS_LITERAL_CSTRING("compartment(web-worker)/");

    // This should never be used when reporting with workers (hence the "?!").
    extras->domPathPrefix.AssignLiteral("explicit/workers/?!/");

    aCompartmentStats->extra = extras;
  }
};

class MessagePortRunnable MOZ_FINAL : public WorkerRunnable
{
  uint64_t mMessagePortSerial;
  bool mConnect;

public:
  MessagePortRunnable(WorkerPrivate* aWorkerPrivate,
                      uint64_t aMessagePortSerial,
                      bool aConnect)
  : WorkerRunnable(aWorkerPrivate, aConnect ?
                                   WorkerThreadModifyBusyCount :
                                   WorkerThreadUnchangedBusyCount),
    mMessagePortSerial(aMessagePortSerial), mConnect(aConnect)
  { }

private:
  ~MessagePortRunnable()
  { }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) MOZ_OVERRIDE
  {
    if (mConnect) {
      return aWorkerPrivate->ConnectMessagePort(aCx, mMessagePortSerial);
    }

    aWorkerPrivate->DisconnectMessagePort(mMessagePortSerial);
    return true;
  }
};

#ifdef DEBUG

PRThread*
PRThreadFromThread(nsIThread* aThread)
{
  MOZ_ASSERT(aThread);

  PRThread* result;
  MOZ_ASSERT(NS_SUCCEEDED(aThread->GetPRThread(&result)));
  MOZ_ASSERT(result);

  return result;
}

#endif // DEBUG

} /* anonymous namespace */

NS_IMPL_ISUPPORTS_INHERITED0(MainThreadReleaseRunnable, nsRunnable)

NS_IMPL_ISUPPORTS_INHERITED0(TopLevelWorkerFinishedRunnable, nsRunnable)

NS_IMPL_ISUPPORTS1(TimerThreadEventTarget, nsIEventTarget)

template <class Derived>
class WorkerPrivateParent<Derived>::SynchronizeAndResumeRunnable MOZ_FINAL
  : public nsRunnable
{
  friend class nsRevocableEventPtr<SynchronizeAndResumeRunnable>;

  WorkerPrivate* mWorkerPrivate;
  nsCOMPtr<nsPIDOMWindow> mWindow;
  nsCOMPtr<nsIScriptContext> mScriptContext;

public:
  SynchronizeAndResumeRunnable(WorkerPrivate* aWorkerPrivate,
                               nsPIDOMWindow* aWindow,
                               nsIScriptContext* aScriptContext)
  : mWorkerPrivate(aWorkerPrivate), mWindow(aWindow),
    mScriptContext(aScriptContext)
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(aWindow);
    MOZ_ASSERT(!aWorkerPrivate->GetParent());
  }

private:
  ~SynchronizeAndResumeRunnable()
  { }

  NS_IMETHOD
  Run() MOZ_OVERRIDE
  {
    AssertIsOnMainThread();

    if (mWorkerPrivate) {
      AutoPushJSContext cx(mScriptContext ?
                           mScriptContext->GetNativeContext() :
                           nsContentUtils::GetSafeJSContext());

      if (!mWorkerPrivate->Resume(cx, mWindow)) {
        JS_ReportPendingException(cx);
      }
    }

    return NS_OK;
  }

  void
  Revoke()
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(mWorkerPrivate);
    MOZ_ASSERT(mWindow);

    mWorkerPrivate = nullptr;
    mWindow = nullptr;
    mScriptContext = nullptr;
  }
};

template <class Derived>
class WorkerPrivateParent<Derived>::EventTarget MOZ_FINAL
  : public nsIEventTarget
{
  // This mutex protects mWorkerPrivate and must be acquired *before* the
  // WorkerPrivate's mutex whenever they must both be held.
  mozilla::Mutex mMutex;
  WorkerPrivate* mWorkerPrivate;
  nsIEventTarget* mWeakNestedEventTarget;
  nsCOMPtr<nsIEventTarget> mNestedEventTarget;

public:
  EventTarget(WorkerPrivate* aWorkerPrivate)
  : mMutex("WorkerPrivateParent::EventTarget::mMutex"),
    mWorkerPrivate(aWorkerPrivate), mWeakNestedEventTarget(nullptr)
  {
    MOZ_ASSERT(aWorkerPrivate);
  }

  EventTarget(WorkerPrivate* aWorkerPrivate, nsIEventTarget* aNestedEventTarget)
  : mMutex("WorkerPrivateParent::EventTarget::mMutex"),
    mWorkerPrivate(aWorkerPrivate), mWeakNestedEventTarget(aNestedEventTarget),
    mNestedEventTarget(aNestedEventTarget)
  {
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(aNestedEventTarget);
  }

  void
  Disable()
  {
    nsCOMPtr<nsIEventTarget> nestedEventTarget;
    {
      MutexAutoLock lock(mMutex);

      MOZ_ASSERT(mWorkerPrivate);
      mWorkerPrivate = nullptr;
      mNestedEventTarget.swap(nestedEventTarget);
    }
  }

  nsIEventTarget*
  GetWeakNestedEventTarget() const
  {
    MOZ_ASSERT(mWeakNestedEventTarget);
    return mWeakNestedEventTarget;
  }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIEVENTTARGET

private:
  ~EventTarget()
  { }
};

struct WorkerPrivate::TimeoutInfo
{
  TimeoutInfo()
  : mTimeoutCallable(JS::UndefinedValue()), mLineNumber(0), mId(0),
    mIsInterval(false), mCanceled(false)
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

  JS::Heap<JS::Value> mTimeoutCallable;
  nsString mTimeoutString;
  nsTArray<JS::Heap<JS::Value> > mExtraArgVals;
  mozilla::TimeStamp mTargetTime;
  mozilla::TimeDuration mInterval;
  nsCString mFilename;
  uint32_t mLineNumber;
  int32_t mId;
  bool mIsInterval;
  bool mCanceled;
};

class WorkerPrivate::MemoryReporter MOZ_FINAL : public nsIMemoryReporter
{
  NS_DECL_THREADSAFE_ISUPPORTS

  friend class WorkerPrivate;

  SharedMutex mMutex;
  WorkerPrivate* mWorkerPrivate;
  nsCString mRtPath;
  bool mAlreadyMappedToAddon;

public:
  MemoryReporter(WorkerPrivate* aWorkerPrivate)
  : mMutex(aWorkerPrivate->mMutex), mWorkerPrivate(aWorkerPrivate),
    mAlreadyMappedToAddon(false)
  {
    aWorkerPrivate->AssertIsOnWorkerThread();

    nsCString escapedDomain(aWorkerPrivate->Domain());
    escapedDomain.ReplaceChar('/', '\\');

    NS_ConvertUTF16toUTF8 escapedURL(aWorkerPrivate->ScriptURL());
    escapedURL.ReplaceChar('/', '\\');

    nsAutoCString addressString;
    addressString.AppendPrintf("0x%p", static_cast<void*>(aWorkerPrivate));

    mRtPath = NS_LITERAL_CSTRING("explicit/workers/workers(") +
              escapedDomain + NS_LITERAL_CSTRING(")/worker(") +
              escapedURL + NS_LITERAL_CSTRING(", ") + addressString +
              NS_LITERAL_CSTRING(")/");
  }

  NS_IMETHOD
  CollectReports(nsIMemoryReporterCallback* aCallback,
                 nsISupports* aClosure)
  {
    AssertIsOnMainThread();

    // Assumes that WorkerJSRuntimeStats will hold a reference to mRtPath,
    // and not a copy, as TryToMapAddon() may later modify the string again.
    WorkerJSRuntimeStats rtStats(mRtPath);

    {
      MutexAutoLock lock(mMutex);

      TryToMapAddon();

      if (!mWorkerPrivate ||
          !mWorkerPrivate->BlockAndCollectRuntimeStats(&rtStats)) {
        // Returning NS_OK here will effectively report 0 memory.
        return NS_OK;
      }
    }

    return xpc::ReportJSRuntimeExplicitTreeStats(rtStats, mRtPath,
                                                 aCallback, aClosure);
  }

private:
  ~MemoryReporter()
  { }

  void
  Disable()
  {
    // Called from WorkerPrivate::DisableMemoryReporter.
    mMutex.AssertCurrentThreadOwns();

    NS_ASSERTION(mWorkerPrivate, "Disabled more than once!");
    mWorkerPrivate = nullptr;
  }

  // Only call this from the main thread and under mMutex lock.
  void
  TryToMapAddon()
  {
    AssertIsOnMainThread();
    mMutex.AssertCurrentThreadOwns();

    if (mAlreadyMappedToAddon || !mWorkerPrivate) {
      return;
    }

    nsCOMPtr<nsIURI> scriptURI;
    if (NS_FAILED(NS_NewURI(getter_AddRefs(scriptURI),
                            mWorkerPrivate->ScriptURL()))) {
      return;
    }

    mAlreadyMappedToAddon = true;

    if (XRE_GetProcessType() != GeckoProcessType_Default) {
      // Only try to access the service from the main process.
      return;
    }

    nsAutoCString addonId;
    bool ok;
    nsCOMPtr<amIAddonManager> addonManager =
      do_GetService("@mozilla.org/addons/integration;1");

    if (!addonManager ||
        NS_FAILED(addonManager->MapURIToAddonID(scriptURI, addonId, &ok)) ||
        !ok) {
      return;
    }

    static const size_t explicitLength = strlen("explicit/");
    addonId.Insert(NS_LITERAL_CSTRING("add-ons/"), 0);
    addonId += "/";
    mRtPath.Insert(addonId, explicitLength);
  }
};

NS_IMPL_ISUPPORTS1(WorkerPrivate::MemoryReporter, nsIMemoryReporter)

WorkerPrivate::SyncLoopInfo::SyncLoopInfo(EventTarget* aEventTarget)
: mEventTarget(aEventTarget), mCompleted(false), mResult(false)
#ifdef DEBUG
  , mHasRun(false)
#endif
{
}

// Can't use NS_IMPL_CYCLE_COLLECTION_CLASS(WorkerPrivateParent) because of the
// templates.
template <class Derived>
typename WorkerPrivateParent<Derived>::cycleCollection
  WorkerPrivateParent<Derived>::_cycleCollectorGlobal =
    WorkerPrivateParent<Derived>::cycleCollection();

template <class Derived>
WorkerPrivateParent<Derived>::WorkerPrivateParent(
                                             JSContext* aCx,
                                             WorkerPrivate* aParent,
                                             const nsAString& aScriptURL,
                                             bool aIsChromeWorker,
                                             WorkerType aWorkerType,
                                             const nsAString& aSharedWorkerName,
                                             LoadInfo& aLoadInfo)
: mMutex("WorkerPrivateParent Mutex"),
  mCondVar(mMutex, "WorkerPrivateParent CondVar"),
  mMemoryReportCondVar(mMutex, "WorkerPrivateParent Memory Report CondVar"),
  mParent(aParent), mScriptURL(aScriptURL),
  mSharedWorkerName(aSharedWorkerName), mBusyCount(0), mMessagePortSerial(0),
  mParentStatus(Pending), mRooted(false), mParentSuspended(false),
  mIsChromeWorker(aIsChromeWorker), mMainThreadObjectsForgotten(false),
  mWorkerType(aWorkerType)
{
  SetIsDOMBinding();

  MOZ_ASSERT_IF(IsSharedWorker(), !aSharedWorkerName.IsVoid() &&
                                  NS_IsMainThread());
  MOZ_ASSERT_IF(!IsSharedWorker(), aSharedWorkerName.IsEmpty());

  if (aLoadInfo.mWindow) {
    AssertIsOnMainThread();
    MOZ_ASSERT(aLoadInfo.mWindow->IsInnerWindow(),
               "Should have inner window here!");
    BindToOwner(aLoadInfo.mWindow);
  }

  mLoadInfo.StealFrom(aLoadInfo);

  if (aParent) {
    aParent->AssertIsOnWorkerThread();

    aParent->CopyJSSettings(mJSSettings);
  }
  else {
    AssertIsOnMainThread();

    RuntimeService::GetDefaultJSSettings(mJSSettings);
  }
}

template <class Derived>
WorkerPrivateParent<Derived>::~WorkerPrivateParent()
{
  MOZ_ASSERT(!mRooted);

  DropJSObjects(this);
}

template <class Derived>
JSObject*
WorkerPrivateParent<Derived>::WrapObject(JSContext* aCx,
                                         JS::Handle<JSObject*> aScope)
{
  MOZ_ASSERT(!IsSharedWorker(),
             "We should never wrap a WorkerPrivate for a SharedWorker");

  AssertIsOnParentThread();

  JS::Rooted<JSObject*> obj(aCx, WorkerBinding::Wrap(aCx, aScope, ParentAsWorkerPrivate()));

  if (mRooted) {
    PreserveWrapper(this);
  }

  return obj;
}

template <class Derived>
nsresult
WorkerPrivateParent<Derived>::DispatchPrivate(WorkerRunnable* aRunnable,
                                              nsIEventTarget* aSyncLoopTarget)
{
  // May be called on any thread!

  WorkerPrivate* self = ParentAsWorkerPrivate();

  {
    MutexAutoLock lock(mMutex);

    MOZ_ASSERT_IF(aSyncLoopTarget, self->mThread);

    if (!self->mThread) {
      if (ParentStatus() == Pending || self->mStatus == Pending) {
        mPreStartRunnables.AppendElement(aRunnable);
        return NS_OK;
      }

      NS_WARNING("Using a worker event target after the thread has already"
                 "been released!");
      return NS_ERROR_UNEXPECTED;
    }

    if (self->mStatus == Dead ||
        (!aSyncLoopTarget && ParentStatus() > Running)) {
      NS_WARNING("A runnable was posted to a worker that is already shutting "
                 "down!");
      return NS_ERROR_UNEXPECTED;
    }

    nsCOMPtr<nsIEventTarget> target;
    if (aSyncLoopTarget) {
      target = aSyncLoopTarget;
    }
    else {
      target = self->mThread;
    }

    nsresult rv = target->Dispatch(aRunnable, NS_DISPATCH_NORMAL);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mCondVar.Notify();
  }

  return NS_OK;
}

template <class Derived>
nsresult
WorkerPrivateParent<Derived>::DispatchControlRunnable(
                                  WorkerControlRunnable* aWorkerControlRunnable)
{
  // May be called on any thread!

  MOZ_ASSERT(aWorkerControlRunnable);

  nsRefPtr<WorkerControlRunnable> runnable = aWorkerControlRunnable;

  WorkerPrivate* self = ParentAsWorkerPrivate();

  {
    MutexAutoLock lock(mMutex);

    if (self->mStatus == Dead) {
      NS_WARNING("A control runnable was posted to a worker that is already "
                 "shutting down!");
      return NS_ERROR_UNEXPECTED;
    }

    // Transfer ownership to the control queue.
    self->mControlQueue.Push(runnable.forget().get());

    if (JSContext* cx = self->mJSContext) {
      MOZ_ASSERT(self->mThread);

      JSRuntime* rt = JS_GetRuntime(cx);
      MOZ_ASSERT(rt);

      JS_TriggerOperationCallback(rt);
    }

    mCondVar.Notify();
  }

  return NS_OK;
}

template <class Derived>
already_AddRefed<WorkerRunnable>
WorkerPrivateParent<Derived>::MaybeWrapAsWorkerRunnable(nsIRunnable* aRunnable)
{
  // May be called on any thread!

  MOZ_ASSERT(aRunnable);

  nsRefPtr<WorkerRunnable> workerRunnable =
    WorkerRunnable::FromRunnable(aRunnable);
  if (workerRunnable) {
    return workerRunnable.forget();
  }

  nsCOMPtr<nsICancelableRunnable> cancelable = do_QueryInterface(aRunnable);
  if (!cancelable) {
    MOZ_CRASH("All runnables destined for a worker thread must be cancelable!");
  }

  workerRunnable =
    new ExternalRunnableWrapper(ParentAsWorkerPrivate(), cancelable);
  return workerRunnable.forget();
}

template <class Derived>
already_AddRefed<nsIEventTarget>
WorkerPrivateParent<Derived>::GetEventTarget()
{
  WorkerPrivate* self = ParentAsWorkerPrivate();

  nsCOMPtr<nsIEventTarget> target;

  {
    MutexAutoLock lock(mMutex);

    if (!mEventTarget &&
        ParentStatus() <= Running &&
        self->mStatus <= Running) {
      mEventTarget = new EventTarget(self);
    }

    target = mEventTarget;
  }

  NS_WARN_IF_FALSE(target,
                   "Requested event target for a worker that is already "
                   "shutting down!");

  return target.forget();
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

  if (IsSharedWorker()) {
    RuntimeService* runtime = RuntimeService::GetService();
    MOZ_ASSERT(runtime);

    runtime->ForgetSharedWorker(ParentAsWorkerPrivate());
  }

  if (pending) {
    WorkerPrivate* self = ParentAsWorkerPrivate();

#ifdef DEBUG
    {
      // Fake a thread here just so that our assertions don't go off for no
      // reason.
      nsIThread* currentThread = NS_GetCurrentThread();
      MOZ_ASSERT(currentThread);

      MOZ_ASSERT(!self->mPRThread);
      self->mPRThread = PRThreadFromThread(currentThread);
      MOZ_ASSERT(self->mPRThread);
    }
#endif

    // Worker never got a chance to run, go ahead and delete it.
    self->ScheduleDeletion();
    return true;
  }

  // Only top-level workers should have a synchronize runnable.
  MOZ_ASSERT_IF(mSynchronizeRunnable.get(), !GetParent());
  mSynchronizeRunnable.Revoke();

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
WorkerPrivateParent<Derived>::Suspend(JSContext* aCx, nsPIDOMWindow* aWindow)
{
  AssertIsOnParentThread();
  MOZ_ASSERT(aCx);

  // Shared workers are only suspended if all of their owning documents are
  // suspended.
  if (IsSharedWorker()) {
    AssertIsOnMainThread();
    MOZ_ASSERT(mSharedWorkers.Count());

    struct Closure
    {
      nsPIDOMWindow* mWindow;
      bool mAllSuspended;

      Closure(nsPIDOMWindow* aWindow)
      : mWindow(aWindow), mAllSuspended(true)
      {
        AssertIsOnMainThread();
        // aWindow may be null here.
      }

      static PLDHashOperator
      Suspend(const uint64_t& aKey,
              SharedWorker* aSharedWorker,
              void* aClosure)
      {
        AssertIsOnMainThread();
        MOZ_ASSERT(aSharedWorker);
        MOZ_ASSERT(aClosure);

        auto closure = static_cast<Closure*>(aClosure);

        if (closure->mWindow && aSharedWorker->GetOwner() == closure->mWindow) {
          // Calling Suspend() may change the refcount, ensure that the worker
          // outlives this call.
          nsRefPtr<SharedWorker> kungFuDeathGrip = aSharedWorker;

          aSharedWorker->Suspend();
        } else {
          MOZ_ASSERT_IF(aSharedWorker->GetOwner() && closure->mWindow,
                        !SameCOMIdentity(aSharedWorker->GetOwner(),
                                         closure->mWindow));
          if (!aSharedWorker->IsSuspended()) {
            closure->mAllSuspended = false;
          }
        }
        return PL_DHASH_NEXT;
      }
    };

    Closure closure(aWindow);

    mSharedWorkers.EnumerateRead(Closure::Suspend, &closure);

    if (!closure.mAllSuspended || mParentSuspended) {
      return true;
    }
  }

  MOZ_ASSERT(!mParentSuspended, "Suspended more than once!");

  mParentSuspended = true;

  {
    MutexAutoLock lock(mMutex);

    if (mParentStatus >= Terminating) {
      return true;
    }
  }

  nsRefPtr<SuspendRunnable> runnable =
    new SuspendRunnable(ParentAsWorkerPrivate());
  if (!runnable->Dispatch(aCx)) {
    return false;
  }

  return true;
}

template <class Derived>
bool
WorkerPrivateParent<Derived>::Resume(JSContext* aCx, nsPIDOMWindow* aWindow)
{
  AssertIsOnParentThread();
  MOZ_ASSERT(aCx);
  MOZ_ASSERT_IF(!IsSharedWorker(), mParentSuspended);

  // Shared workers are resumed if any of their owning documents are resumed.
  if (IsSharedWorker()) {
    AssertIsOnMainThread();
    MOZ_ASSERT(mSharedWorkers.Count());

    struct Closure
    {
      nsPIDOMWindow* mWindow;
      bool mAnyRunning;

      Closure(nsPIDOMWindow* aWindow)
      : mWindow(aWindow), mAnyRunning(false)
      {
        AssertIsOnMainThread();
        // aWindow may be null here.
      }

      static PLDHashOperator
      Resume(const uint64_t& aKey,
              SharedWorker* aSharedWorker,
              void* aClosure)
      {
        AssertIsOnMainThread();
        MOZ_ASSERT(aSharedWorker);
        MOZ_ASSERT(aClosure);

        auto closure = static_cast<Closure*>(aClosure);

        if (closure->mWindow && aSharedWorker->GetOwner() == closure->mWindow) {
          // Calling Resume() may change the refcount, ensure that the worker
          // outlives this call.
          nsRefPtr<SharedWorker> kungFuDeathGrip = aSharedWorker;

          aSharedWorker->Resume();
          closure->mAnyRunning = true;
        } else {
          MOZ_ASSERT_IF(aSharedWorker->GetOwner() && closure->mWindow,
                        !SameCOMIdentity(aSharedWorker->GetOwner(),
                                         closure->mWindow));
          if (!aSharedWorker->IsSuspended()) {
            closure->mAnyRunning = true;
          }
        }
        return PL_DHASH_NEXT;
      }
    };

    Closure closure(aWindow);

    mSharedWorkers.EnumerateRead(Closure::Resume, &closure);

    if (!closure.mAnyRunning || !mParentSuspended) {
      return true;
    }
  }

  MOZ_ASSERT(mParentSuspended);

  mParentSuspended = false;

  {
    MutexAutoLock lock(mMutex);

    if (mParentStatus >= Terminating) {
      return true;
    }
  }

  // Only top-level workers should have a synchronize runnable.
  MOZ_ASSERT_IF(mSynchronizeRunnable.get(), !GetParent());
  mSynchronizeRunnable.Revoke();

  // Execute queued runnables before waking up the worker, otherwise the worker
  // could post new messages before we run those that have been queued.
  if (!mQueuedRunnables.IsEmpty()) {
    AssertIsOnMainThread();
    MOZ_ASSERT(IsDedicatedWorker());

    nsTArray<nsCOMPtr<nsIRunnable>> runnables;
    mQueuedRunnables.SwapElements(runnables);

    for (uint32_t index = 0; index < runnables.Length(); index++) {
      runnables[index]->Run();
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
bool
WorkerPrivateParent<Derived>::SynchronizeAndResume(
                                               JSContext* aCx,
                                               nsPIDOMWindow* aWindow,
                                               nsIScriptContext* aScriptContext)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(!GetParent());
  MOZ_ASSERT_IF(IsDedicatedWorker(), mParentSuspended);

  // NB: There may be pending unqueued messages.  If we resume here we will
  // execute those messages out of order.  Instead we post an event to the
  // end of the event queue, allowing all of the outstanding messages to be
  // queued up in order on the worker.  Then and only then we execute all of
  // the messages.

  nsRefPtr<SynchronizeAndResumeRunnable> runnable =
    new SynchronizeAndResumeRunnable(ParentAsWorkerPrivate(), aWindow,
                                     aScriptContext);
  if (NS_FAILED(NS_DispatchToCurrentThread(runnable))) {
    JS_ReportError(aCx, "Failed to dispatch to current thread!");
    return false;
  }

  mSynchronizeRunnable = runnable;
  return true;
}

template <class Derived>
void
WorkerPrivateParent<Derived>::_finalize(JSFreeOp* aFop)
{
  AssertIsOnParentThread();

  MOZ_ASSERT(!mRooted);

  ClearWrapper();

  // Ensure that we're held alive across the TerminatePrivate call, and then
  // release the reference our wrapper held to us.
  nsRefPtr<WorkerPrivateParent<Derived> > kungFuDeathGrip = dont_AddRef(this);

  if (!TerminatePrivate(nullptr)) {
    NS_WARNING("Failed to terminate!");
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
    if (mBusyCount++ == 0) {
      Root(true);
    }
    return true;
  }

  if (--mBusyCount == 0) {
    Root(false);

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
void
WorkerPrivateParent<Derived>::Root(bool aRoot)
{
  AssertIsOnParentThread();

  if (aRoot == mRooted) {
    return;
  }

  if (aRoot) {
    NS_ADDREF_THIS();
    if (GetWrapperPreserveColor()) {
      PreserveWrapper(this);
    }
  }
  else {
    if (GetWrapperPreserveColor()) {
      ReleaseWrapper(this);
    }
    NS_RELEASE_THIS();
  }

  mRooted = aRoot;
}

template <class Derived>
void
WorkerPrivateParent<Derived>::ForgetMainThreadObjects(
                                      nsTArray<nsCOMPtr<nsISupports> >& aDoomed)
{
  AssertIsOnParentThread();
  MOZ_ASSERT(!mMainThreadObjectsForgotten);

  static const uint32_t kDoomedCount = 7;

  aDoomed.SetCapacity(kDoomedCount);

  SwapToISupportsArray(mLoadInfo.mWindow, aDoomed);
  SwapToISupportsArray(mLoadInfo.mScriptContext, aDoomed);
  SwapToISupportsArray(mLoadInfo.mBaseURI, aDoomed);
  SwapToISupportsArray(mLoadInfo.mResolvedScriptURI, aDoomed);
  SwapToISupportsArray(mLoadInfo.mPrincipal, aDoomed);
  SwapToISupportsArray(mLoadInfo.mChannel, aDoomed);
  SwapToISupportsArray(mLoadInfo.mCSP, aDoomed);
  // Before adding anything here update kDoomedCount above!

  MOZ_ASSERT(aDoomed.Length() == kDoomedCount);

  mMainThreadObjectsForgotten = true;
}

template <class Derived>
void
WorkerPrivateParent<Derived>::PostMessageInternal(
                                            JSContext* aCx,
                                            JS::Handle<JS::Value> aMessage,
                                            const Optional<Sequence<JS::Value> >& aTransferable,
                                            bool aToMessagePort,
                                            uint64_t aMessagePortSerial,
                                            ErrorResult& aRv)
{
  AssertIsOnParentThread();

  {
    MutexAutoLock lock(mMutex);
    if (mParentStatus > Running) {
      return;
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

  JS::Rooted<JS::Value> transferable(aCx, JS::UndefinedValue());
  if (aTransferable.WasPassed()) {
    const Sequence<JS::Value>& realTransferable = aTransferable.Value();
    JSObject* array =
      JS_NewArrayObject(aCx, realTransferable.Length(),
                        const_cast<JS::Value*>(realTransferable.Elements()));
    if (!array) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
    transferable.setObject(*array);
  }

  nsTArray<nsCOMPtr<nsISupports> > clonedObjects;

  JSAutoStructuredCloneBuffer buffer;
  if (!buffer.write(aCx, aMessage, transferable, callbacks, &clonedObjects)) {
    aRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
    return;
  }

  nsRefPtr<MessageEventRunnable> runnable =
    new MessageEventRunnable(ParentAsWorkerPrivate(),
                             WorkerRunnable::WorkerThreadModifyBusyCount,
                             buffer, clonedObjects, aToMessagePort,
                             aMessagePortSerial);
  if (!runnable->Dispatch(aCx)) {
    aRv.Throw(NS_ERROR_FAILURE);
  }
}

template <class Derived>
void
WorkerPrivateParent<Derived>::PostMessageToMessagePort(
                             JSContext* aCx,
                             uint64_t aMessagePortSerial,
                             JS::Handle<JS::Value> aMessage,
                             const Optional<Sequence<JS::Value>>& aTransferable,
                             ErrorResult& aRv)
{
  AssertIsOnMainThread();

  PostMessageInternal(aCx, aMessage, aTransferable, true, aMessagePortSerial,
                      aRv);
}

template <class Derived>
bool
WorkerPrivateParent<Derived>::DispatchMessageEventToMessagePort(
                                JSContext* aCx, uint64_t aMessagePortSerial,
                                JSAutoStructuredCloneBuffer& aBuffer,
                                nsTArray<nsCOMPtr<nsISupports>>& aClonedObjects)
{
  AssertIsOnMainThread();

  JSAutoStructuredCloneBuffer buffer;
  buffer.swap(aBuffer);

  nsTArray<nsCOMPtr<nsISupports>> clonedObjects;
  clonedObjects.SwapElements(aClonedObjects);

  SharedWorker* sharedWorker;
  if (!mSharedWorkers.Get(aMessagePortSerial, &sharedWorker)) {
    // SharedWorker has already been unregistered?
    return true;
  }

  nsRefPtr<MessagePort> port = sharedWorker->Port();
  NS_ASSERTION(port, "SharedWorkers always have a port!");

  if (port->IsClosed()) {
    return true;
  }

  nsCOMPtr<nsIScriptGlobalObject> sgo;
  port->GetParentObject(getter_AddRefs(sgo));
  MOZ_ASSERT(sgo, "Should never happen if IsClosed() returned false!");
  MOZ_ASSERT(sgo->GetGlobalJSObject());

  nsCOMPtr<nsIScriptContext> scx = sgo->GetContext();
  MOZ_ASSERT_IF(scx, scx->GetNativeContext());

  AutoPushJSContext cx(scx ? scx->GetNativeContext() : aCx);
  JSAutoCompartment(cx, sgo->GetGlobalJSObject());

  JS::Rooted<JS::Value> data(cx);
  if (!buffer.read(cx, &data, WorkerStructuredCloneCallbacks(true))) {
    return false;
  }

  buffer.clear();

  nsRefPtr<nsDOMMessageEvent> event =
    new nsDOMMessageEvent(port, nullptr, nullptr);

  nsresult rv =
    event->InitMessageEvent(NS_LITERAL_STRING("message"), false, false, data,
                            EmptyString(), EmptyString(), nullptr);
  if (NS_FAILED(rv)) {
    xpc::Throw(cx, rv);
    return false;
  }

  event->SetTrusted(true);

  nsTArray<nsRefPtr<MessagePortBase>> ports;
  ports.AppendElement(port);

  nsRefPtr<MessagePortList> portList = new MessagePortList(port, ports);
  event->SetPorts(portList);

  nsCOMPtr<nsIDOMEvent> domEvent;
  CallQueryInterface(event.get(), getter_AddRefs(domEvent));
  NS_ASSERTION(domEvent, "This should never fail!");

  bool ignored;
  rv = port->DispatchEvent(domEvent, &ignored);
  if (NS_FAILED(rv)) {
    xpc::Throw(cx, rv);
    return false;
  }

  return true;
}

template <class Derived>
uint64_t
WorkerPrivateParent<Derived>::GetInnerWindowId()
{
  AssertIsOnMainThread();
  NS_ASSERTION(!mLoadInfo.mWindow || mLoadInfo.mWindow->IsInnerWindow(),
               "Outer window?");
  return mLoadInfo.mWindow ? mLoadInfo.mWindow->WindowID() : 0;
}

template <class Derived>
void
WorkerPrivateParent<Derived>::UpdateJSContextOptions(
                                      JSContext* aCx,
                                      const JS::ContextOptions& aContentOptions,
                                      const JS::ContextOptions& aChromeOptions)
{
  AssertIsOnParentThread();

  {
    MutexAutoLock lock(mMutex);
    mJSSettings.content.contextOptions = aContentOptions;
    mJSSettings.chrome.contextOptions = aChromeOptions;
  }

  nsRefPtr<UpdateJSContextOptionsRunnable> runnable =
    new UpdateJSContextOptionsRunnable(ParentAsWorkerPrivate(), aContentOptions,
                                       aChromeOptions);
  if (!runnable->Dispatch(aCx)) {
    NS_WARNING("Failed to update worker context options!");
    JS_ClearPendingException(aCx);
  }
}

template <class Derived>
void
WorkerPrivateParent<Derived>::UpdatePreference(JSContext* aCx, WorkerPreference aPref, bool aValue)
{
  AssertIsOnParentThread();
  MOZ_ASSERT(aPref >= 0 && aPref < WORKERPREF_COUNT);

  nsRefPtr<UpdatePreferenceRunnable> runnable =
    new UpdatePreferenceRunnable(ParentAsWorkerPrivate(), aPref, aValue);
  if (!runnable->Dispatch(aCx)) {
    NS_WARNING("Failed to update worker preferences!");
    JS_ClearPendingException(aCx);
  }
}

template <class Derived>
void
WorkerPrivateParent<Derived>::UpdateJSWorkerMemoryParameter(JSContext* aCx,
                                                            JSGCParamKey aKey,
                                                            uint32_t aValue)
{
  AssertIsOnParentThread();

  bool found = false;

  {
    MutexAutoLock lock(mMutex);
    found = mJSSettings.ApplyGCSetting(aKey, aValue);
  }

  if (found) {
    nsRefPtr<UpdateJSWorkerMemoryParameterRunnable> runnable =
      new UpdateJSWorkerMemoryParameterRunnable(ParentAsWorkerPrivate(), aKey,
                                                aValue);
    if (!runnable->Dispatch(aCx)) {
      NS_WARNING("Failed to update memory parameter!");
      JS_ClearPendingException(aCx);
    }
  }
}

#ifdef JS_GC_ZEAL
template <class Derived>
void
WorkerPrivateParent<Derived>::UpdateGCZeal(JSContext* aCx, uint8_t aGCZeal,
                                           uint32_t aFrequency)
{
  AssertIsOnParentThread();

  {
    MutexAutoLock lock(mMutex);
    mJSSettings.gcZeal = aGCZeal;
    mJSSettings.gcZealFrequency = aFrequency;
  }

  nsRefPtr<UpdateGCZealRunnable> runnable =
    new UpdateGCZealRunnable(ParentAsWorkerPrivate(), aGCZeal, aFrequency);
  if (!runnable->Dispatch(aCx)) {
    NS_WARNING("Failed to update worker gczeal!");
    JS_ClearPendingException(aCx);
  }
}
#endif

template <class Derived>
void
WorkerPrivateParent<Derived>::UpdateJITHardening(JSContext* aCx,
                                                 bool aJITHardening)
{
  AssertIsOnParentThread();

  {
    MutexAutoLock lock(mMutex);
    mJSSettings.jitHardening = aJITHardening;
  }

  nsRefPtr<UpdateJITHardeningRunnable> runnable =
    new UpdateJITHardeningRunnable(ParentAsWorkerPrivate(), aJITHardening);
  if (!runnable->Dispatch(aCx)) {
    NS_WARNING("Failed to update worker jit hardening!");
    JS_ClearPendingException(aCx);
  }
}

template <class Derived>
void
WorkerPrivateParent<Derived>::GarbageCollect(JSContext* aCx, bool aShrinking)
{
  AssertIsOnParentThread();

  nsRefPtr<GarbageCollectRunnable> runnable =
    new GarbageCollectRunnable(ParentAsWorkerPrivate(), aShrinking,
                               /* collectChildren = */ true);
  if (!runnable->Dispatch(aCx)) {
    NS_WARNING("Failed to GC worker!");
    JS_ClearPendingException(aCx);
  }
}

template <class Derived>
void
WorkerPrivateParent<Derived>::CycleCollect(JSContext* aCx, bool aDummy)
{
  AssertIsOnParentThread();

  nsRefPtr<CycleCollectRunnable> runnable =
    new CycleCollectRunnable(ParentAsWorkerPrivate(),
                             /* collectChildren = */ true);
  if (!runnable->Dispatch(aCx)) {
    NS_WARNING("Failed to CC worker!");
    JS_ClearPendingException(aCx);
  }
}

template <class Derived>
void
WorkerPrivateParent<Derived>::OfflineStatusChangeEvent(JSContext* aCx, bool aIsOffline)
{
  AssertIsOnParentThread();

  nsRefPtr<OfflineStatusChangeRunnable> runnable =
    new OfflineStatusChangeRunnable(ParentAsWorkerPrivate(), aIsOffline);
  if (!runnable->Dispatch(aCx)) {
    NS_WARNING("Failed to dispatch offline status change event!");
    JS_ClearPendingException(aCx);
  }
}

void
WorkerPrivate::OfflineStatusChangeEventInternal(JSContext* aCx, bool aIsOffline)
{
  AssertIsOnWorkerThread();

  for (uint32_t index = 0; index < mChildWorkers.Length(); ++index) {
    mChildWorkers[index]->OfflineStatusChangeEvent(aCx, aIsOffline);
  }

  mOnLine = !aIsOffline;
  WorkerGlobalScope* globalScope = GlobalScope();
  nsRefPtr<WorkerNavigator> nav = globalScope->GetExistingNavigator();
  if (nav) {
    nav->SetOnLine(mOnLine);
  }

  nsString eventType;
  if (aIsOffline) {
    eventType.AssignLiteral("offline");
  } else {
    eventType.AssignLiteral("online");
  }

  nsCOMPtr<nsIDOMEvent> event;
  nsresult rv =
    NS_NewDOMEvent(getter_AddRefs(event), globalScope, nullptr, nullptr);
  NS_ENSURE_SUCCESS_VOID(rv);

  rv = event->InitEvent(eventType, false, false);
  NS_ENSURE_SUCCESS_VOID(rv);

  event->SetTrusted(true);

  globalScope->DispatchDOMEvent(nullptr, event, nullptr, nullptr);
}

template <class Derived>
bool
WorkerPrivateParent<Derived>::RegisterSharedWorker(JSContext* aCx,
                                                   SharedWorker* aSharedWorker)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aSharedWorker);
  MOZ_ASSERT(IsSharedWorker());
  MOZ_ASSERT(!mSharedWorkers.Get(aSharedWorker->Serial()));

  nsRefPtr<MessagePortRunnable> runnable =
    new MessagePortRunnable(ParentAsWorkerPrivate(), aSharedWorker->Serial(),
                            true);
  if (!runnable->Dispatch(aCx)) {
    return false;
  }

  mSharedWorkers.Put(aSharedWorker->Serial(), aSharedWorker);

  // If there were other SharedWorker objects attached to this worker then they
  // may all have been suspended and this worker would need to be resumed.
  if (mSharedWorkers.Count() > 1 && !Resume(aCx, nullptr)) {
    return false;
  }

  return true;
}

template <class Derived>
void
WorkerPrivateParent<Derived>::UnregisterSharedWorker(
                                                    JSContext* aCx,
                                                    SharedWorker* aSharedWorker)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aSharedWorker);
  MOZ_ASSERT(IsSharedWorker());
  MOZ_ASSERT(mSharedWorkers.Get(aSharedWorker->Serial()));

  nsRefPtr<MessagePortRunnable> runnable =
    new MessagePortRunnable(ParentAsWorkerPrivate(), aSharedWorker->Serial(),
                            false);
  if (!runnable->Dispatch(aCx)) {
    JS_ReportPendingException(aCx);
  }

  mSharedWorkers.Remove(aSharedWorker->Serial());

  // If there are still SharedWorker objects attached to this worker then they
  // may all be suspended and this worker would need to be suspended. Otherwise,
  // if that was the last SharedWorker then it's time to cancel this worker.
  if (mSharedWorkers.Count()) {
    if (!Suspend(aCx, nullptr)) {
      JS_ReportPendingException(aCx);
    }
  } else if (!Cancel(aCx)) {
    JS_ReportPendingException(aCx);
  }
}

template <class Derived>
void
WorkerPrivateParent<Derived>::BroadcastErrorToSharedWorkers(
                                                    JSContext* aCx,
                                                    const nsAString& aMessage,
                                                    const nsAString& aFilename,
                                                    const nsAString& aLine,
                                                    uint32_t aLineNumber,
                                                    uint32_t aColumnNumber,
                                                    uint32_t aFlags)
{
  AssertIsOnMainThread();

  nsAutoTArray<nsRefPtr<SharedWorker>, 10> sharedWorkers;
  GetAllSharedWorkers(sharedWorkers);

  if (sharedWorkers.IsEmpty()) {
    return;
  }

  nsAutoTArray<WindowAction, 10> windowActions;
  nsresult rv;

  // First fire the error event at all SharedWorker objects. This may include
  // multiple objects in a single window as well as objects in different
  // windows.
  for (uint32_t index = 0; index < sharedWorkers.Length(); index++) {
    nsRefPtr<SharedWorker>& sharedWorker = sharedWorkers[index];

    // May be null.
    nsPIDOMWindow* window = sharedWorker->GetOwner();

    uint32_t actionsIndex = windowActions.LastIndexOf(WindowAction(window));

    // Get the context for this window so that we can report errors correctly.
    JSContext* cx;
    rv = NS_OK;

    if (actionsIndex == windowActions.NoIndex) {
      nsIScriptContext* scx = sharedWorker->GetContextForEventHandlers(&rv);
      cx = (NS_SUCCEEDED(rv) && scx) ?
           scx->GetNativeContext() :
           nsContentUtils::GetSafeJSContext();
    } else {
      cx = windowActions[actionsIndex].mJSContext;
    }

    AutoCxPusher autoPush(cx);

    if (NS_FAILED(rv)) {
      Throw(cx, rv);
      JS_ReportPendingException(cx);
      continue;
    }

    ErrorEventInit errorInit;
    errorInit.mBubbles = false;
    errorInit.mCancelable = true;
    errorInit.mMessage = aMessage;
    errorInit.mFilename = aFilename;
    errorInit.mLineno = aLineNumber;
    errorInit.mColumn = aColumnNumber;

    nsRefPtr<ErrorEvent> errorEvent =
      ErrorEvent::Constructor(sharedWorker, NS_LITERAL_STRING("error"),
                              errorInit);
    if (!errorEvent) {
      Throw(cx, NS_ERROR_UNEXPECTED);
      JS_ReportPendingException(cx);
      continue;
    }

    errorEvent->SetTrusted(true);

    bool defaultActionEnabled;
    rv = sharedWorker->DispatchEvent(errorEvent, &defaultActionEnabled);
    if (NS_FAILED(rv)) {
      Throw(cx, rv);
      JS_ReportPendingException(cx);
      continue;
    }

    if (defaultActionEnabled) {
      // Add the owning window to our list so that we will fire an error event
      // at it later.
      if (!windowActions.Contains(window)) {
        windowActions.AppendElement(WindowAction(window, cx));
      }
    } else if (actionsIndex != windowActions.NoIndex) {
      // Any listener that calls preventDefault() will prevent the window from
      // receiving the error event.
      windowActions[actionsIndex].mDefaultAction = false;
    }
  }

  // If there are no windows to consider further then we're done.
  if (windowActions.IsEmpty()) {
    return;
  }

  bool shouldLogErrorToConsole = true;

  // Now fire error events at all the windows remaining.
  for (uint32_t index = 0; index < windowActions.Length(); index++) {
    WindowAction& windowAction = windowActions[index];

    // If there is no window or the script already called preventDefault then
    // skip this window.
    if (!windowAction.mWindow || !windowAction.mDefaultAction) {
      continue;
    }

    JSContext* cx = windowAction.mJSContext;

    AutoCxPusher autoPush(cx);

    nsCOMPtr<nsIScriptGlobalObject> sgo =
      do_QueryInterface(windowAction.mWindow);
    MOZ_ASSERT(sgo);

    MOZ_ASSERT(NS_IsMainThread());
    InternalScriptErrorEvent event(true, NS_LOAD_ERROR);
    event.lineNr = aLineNumber;
    event.errorMsg = aMessage.BeginReading();
    event.fileName = aFilename.BeginReading();

    nsEventStatus status = nsEventStatus_eIgnore;
    rv = sgo->HandleScriptError(&event, &status);
    if (NS_FAILED(rv)) {
      Throw(cx, rv);
      JS_ReportPendingException(cx);
      continue;
    }

    if (status == nsEventStatus_eConsumeNoDefault) {
      shouldLogErrorToConsole = false;
    }
  }

  // Finally log a warning in the console if no window tried to prevent it.
  if (shouldLogErrorToConsole) {
    LogErrorToConsole(aMessage, aFilename, aLine, aLineNumber, aColumnNumber,
                      aFlags, 0);
  }
}

template <class Derived>
void
WorkerPrivateParent<Derived>::GetAllSharedWorkers(
                               nsTArray<nsRefPtr<SharedWorker>>& aSharedWorkers)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(IsSharedWorker());

  struct Helper
  {
    static PLDHashOperator
    Collect(const uint64_t& aKey,
            SharedWorker* aSharedWorker,
            void* aClosure)
    {
      AssertIsOnMainThread();
      MOZ_ASSERT(aSharedWorker);
      MOZ_ASSERT(aClosure);

      auto array = static_cast<nsTArray<nsRefPtr<SharedWorker>>*>(aClosure);
      array->AppendElement(aSharedWorker);

      return PL_DHASH_NEXT;
    }
  };

  if (!aSharedWorkers.IsEmpty()) {
    aSharedWorkers.Clear();
  }

  mSharedWorkers.EnumerateRead(Helper::Collect, &aSharedWorkers);
}

template <class Derived>
void
WorkerPrivateParent<Derived>::CloseSharedWorkersForWindow(
                                                         nsPIDOMWindow* aWindow)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(IsSharedWorker());
  MOZ_ASSERT(aWindow);

  struct Closure
  {
    nsPIDOMWindow* mWindow;
    nsAutoTArray<nsRefPtr<SharedWorker>, 10> mSharedWorkers;

    Closure(nsPIDOMWindow* aWindow)
    : mWindow(aWindow)
    {
      AssertIsOnMainThread();
      MOZ_ASSERT(aWindow);
    }

    static PLDHashOperator
    Collect(const uint64_t& aKey,
            SharedWorker* aSharedWorker,
            void* aClosure)
    {
      AssertIsOnMainThread();
      MOZ_ASSERT(aSharedWorker);
      MOZ_ASSERT(aClosure);

      auto closure = static_cast<Closure*>(aClosure);
      MOZ_ASSERT(closure->mWindow);

      if (aSharedWorker->GetOwner() == closure->mWindow) {
        closure->mSharedWorkers.AppendElement(aSharedWorker);
      } else {
        MOZ_ASSERT(!SameCOMIdentity(aSharedWorker->GetOwner(),
                                    closure->mWindow));
      }

      return PL_DHASH_NEXT;
    }
  };

  Closure closure(aWindow);

  mSharedWorkers.EnumerateRead(Closure::Collect, &closure);

  for (uint32_t index = 0; index < closure.mSharedWorkers.Length(); index++) {
    closure.mSharedWorkers[index]->Close();
  }
}

template <class Derived>
void
WorkerPrivateParent<Derived>::WorkerScriptLoaded()
{
  AssertIsOnMainThread();

  if (IsSharedWorker()) {
    // No longer need to hold references to the window or document we came from.
    mLoadInfo.mWindow = nullptr;
    mLoadInfo.mScriptContext = nullptr;
  }
}

template <class Derived>
void
WorkerPrivateParent<Derived>::SetBaseURI(nsIURI* aBaseURI)
{
  AssertIsOnMainThread();

  if (!mLoadInfo.mBaseURI) {
    NS_ASSERTION(GetParent(), "Shouldn't happen without a parent!");
    mLoadInfo.mResolvedScriptURI = aBaseURI;
  }

  mLoadInfo.mBaseURI = aBaseURI;

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

  mLoadInfo.mPrincipal = aPrincipal;
  mLoadInfo.mPrincipalIsSystem = nsContentUtils::IsSystemPrincipal(aPrincipal);
}

template <class Derived>
JSContext*
WorkerPrivateParent<Derived>::ParentJSContext() const
{
  AssertIsOnParentThread();

  if (mParent) {
    return mParent->GetJSContext();
  }

  AssertIsOnMainThread();

  return mLoadInfo.mScriptContext ?
         mLoadInfo.mScriptContext->GetNativeContext() :
         nsContentUtils::GetSafeJSContext();
}

template <class Derived>
void
WorkerPrivateParent<Derived>::RegisterHostObjectURI(const nsACString& aURI)
{
  AssertIsOnMainThread();
  mHostObjectURIs.AppendElement(aURI);
}

template <class Derived>
void
WorkerPrivateParent<Derived>::UnregisterHostObjectURI(const nsACString& aURI)
{
  AssertIsOnMainThread();
  mHostObjectURIs.RemoveElement(aURI);
}

template <class Derived>
void
WorkerPrivateParent<Derived>::StealHostObjectURIs(nsTArray<nsCString>& aArray)
{
  aArray.SwapElements(mHostObjectURIs);
}

template <class Derived>
NS_IMPL_ADDREF_INHERITED(WorkerPrivateParent<Derived>, nsDOMEventTargetHelper)

template <class Derived>
NS_IMPL_RELEASE_INHERITED(WorkerPrivateParent<Derived>, nsDOMEventTargetHelper)

template <class Derived>
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(WorkerPrivateParent<Derived>)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

template <class Derived>
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(WorkerPrivateParent<Derived>,
                                                  nsDOMEventTargetHelper)
  // Nothing else to traverse
  // The various strong references in LoadInfo are managed manually and cannot
  // be cycle collected.
  tmp->AssertIsOnParentThread();
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

template <class Derived>
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(WorkerPrivateParent<Derived>,
                                                nsDOMEventTargetHelper)
  tmp->AssertIsOnParentThread();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

template <class Derived>
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(WorkerPrivateParent<Derived>,
                                               nsDOMEventTargetHelper)
  tmp->AssertIsOnParentThread();
NS_IMPL_CYCLE_COLLECTION_TRACE_END

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
  if (mParent || !mLoadInfo.mWindow) {
    return;
  }

  AssertIsOnMainThread();

  nsPIDOMWindow* outer = mLoadInfo.mWindow->GetOuterWindow();
  NS_ASSERTION(outer && outer->GetCurrentInnerWindow() == mLoadInfo.mWindow,
               "Inner window no longer correct!");
}

#endif
WorkerPrivate::WorkerPrivate(JSContext* aCx,
                             WorkerPrivate* aParent,
                             const nsAString& aScriptURL,
                             bool aIsChromeWorker, WorkerType aWorkerType,
                             const nsAString& aSharedWorkerName,
                             LoadInfo& aLoadInfo)
: WorkerPrivateParent<WorkerPrivate>(aCx, aParent, aScriptURL,
                                     aIsChromeWorker, aWorkerType,
                                     aSharedWorkerName, aLoadInfo),
  mJSContext(nullptr), mErrorHandlerRecursionCount(0), mNextTimeoutId(1),
  mStatus(Pending), mSuspended(false), mTimerRunning(false),
  mRunningExpiredTimeouts(false), mCloseHandlerStarted(false),
  mCloseHandlerFinished(false), mMemoryReporterRunning(false),
  mBlockedForMemoryReporter(false), mCancelAllPendingRunnables(false),
  mPeriodicGCTimerRunning(false), mIdleGCTimerRunning(false)
#ifdef DEBUG
  , mPRThread(nullptr)
#endif
{
  MOZ_ASSERT_IF(IsSharedWorker(), !aSharedWorkerName.IsVoid());
  MOZ_ASSERT_IF(!IsSharedWorker(), aSharedWorkerName.IsEmpty());

  if (aParent) {
    aParent->AssertIsOnWorkerThread();
    aParent->GetAllPreferences(mPreferences);
    mOnLine = aParent->OnLine();
  }
  else {
    AssertIsOnMainThread();
    RuntimeService::GetDefaultPreferences(mPreferences);
    mOnLine = !NS_IsOffline();
  }
}

WorkerPrivate::~WorkerPrivate()
{
}

// static
already_AddRefed<WorkerPrivate>
WorkerPrivate::Constructor(const GlobalObject& aGlobal,
                           const nsAString& aScriptURL,
                           ErrorResult& aRv)
{
  return WorkerPrivate::Constructor(aGlobal, aScriptURL, false, WorkerTypeDedicated,
                                    EmptyString(), nullptr, aRv);
}

// static
bool
WorkerPrivate::WorkerAvailable(JSContext* /* unused */, JSObject* /* unused */)
{
  // If we're already on a worker workers are clearly enabled.
  if (!NS_IsMainThread()) {
    return true;
  }

  // If our caller is chrome, workers are always available.
  if (nsContentUtils::IsCallerChrome()) {
    return true;
  }

  // Else check the pref.
  return Preferences::GetBool(PREF_WORKERS_ENABLED);
}

// static
already_AddRefed<ChromeWorkerPrivate>
ChromeWorkerPrivate::Constructor(const GlobalObject& aGlobal,
                                 const nsAString& aScriptURL,
                                 ErrorResult& aRv)
{
  return WorkerPrivate::Constructor(aGlobal, aScriptURL, true, WorkerTypeDedicated,
                                    EmptyString(), nullptr, aRv).downcast<ChromeWorkerPrivate>();
}

// static
bool
ChromeWorkerPrivate::WorkerAvailable(JSContext* /* unused */, JSObject* /* unused */)
{
  // Chrome is always allowed to use workers, and content is never allowed to
  // use ChromeWorker, so all we have to check is the caller.
  return nsContentUtils::ThreadsafeIsCallerChrome();
}

// static
already_AddRefed<WorkerPrivate>
WorkerPrivate::Constructor(const GlobalObject& aGlobal,
                           const nsAString& aScriptURL,
                           bool aIsChromeWorker, WorkerType aWorkerType,
                           const nsAString& aSharedWorkerName,
                           LoadInfo* aLoadInfo, ErrorResult& aRv)
{
  WorkerPrivate* parent = NS_IsMainThread() ?
                          nullptr :
                          GetCurrentThreadWorkerPrivate();
  if (parent) {
    parent->AssertIsOnWorkerThread();
  } else {
    AssertIsOnMainThread();
  }

  JSContext* cx = aGlobal.GetContext();

  MOZ_ASSERT_IF(aWorkerType == WorkerTypeShared,
                !aSharedWorkerName.IsVoid());
  MOZ_ASSERT_IF(aWorkerType != WorkerTypeShared,
                aSharedWorkerName.IsEmpty());

  Maybe<LoadInfo> stackLoadInfo;
  if (!aLoadInfo) {
    stackLoadInfo.construct();

    nsresult rv = GetLoadInfo(cx, nullptr, parent, aScriptURL,
                              aIsChromeWorker, stackLoadInfo.addr());
    if (NS_FAILED(rv)) {
      scriptloader::ReportLoadError(cx, aScriptURL, rv, !parent);
      aRv.Throw(rv);
      return nullptr;
    }

    aLoadInfo = stackLoadInfo.addr();
  }

  // NB: This has to be done before creating the WorkerPrivate, because it will
  // attempt to use static variables that are initialized in the RuntimeService
  // constructor.
  RuntimeService* runtimeService;

  if (!parent) {
    runtimeService = RuntimeService::GetOrCreateService();
    if (!runtimeService) {
      JS_ReportError(cx, "Failed to create runtime service!");
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }
  }
  else {
    runtimeService = RuntimeService::GetService();
  }

  MOZ_ASSERT(runtimeService);

  nsRefPtr<WorkerPrivate> worker =
    new WorkerPrivate(cx, parent, aScriptURL, aIsChromeWorker,
                      aWorkerType, aSharedWorkerName, *aLoadInfo);

  if (!runtimeService->RegisterWorker(cx, worker)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<CompileScriptRunnable> compiler = new CompileScriptRunnable(worker);
  if (!compiler->Dispatch(cx)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  // The worker will be owned by its JSObject (via the reference we return from
  // this function), but it also needs to be owned by its thread, so AddRef it
  // again.
  NS_ADDREF(worker.get());

  return worker.forget();
}

// static
nsresult
WorkerPrivate::GetLoadInfo(JSContext* aCx, nsPIDOMWindow* aWindow,
                           WorkerPrivate* aParent, const nsAString& aScriptURL,
                           bool aIsChromeWorker, LoadInfo* aLoadInfo)
{
  using namespace mozilla::dom::workers::scriptloader;

  MOZ_ASSERT(aCx);

  if (aWindow) {
    AssertIsOnMainThread();
  }

  LoadInfo loadInfo;
  nsresult rv;

  if (aParent) {
    aParent->AssertIsOnWorkerThread();

    // If the parent is going away give up now.
    Status parentStatus;
    {
      MutexAutoLock lock(aParent->mMutex);
      parentStatus = aParent->mStatus;
    }

    if (parentStatus > Running) {
      NS_WARNING("Cannot create child workers from the close handler!");
      return NS_ERROR_FAILURE;
    }

    rv = ChannelFromScriptURLWorkerThread(aCx, aParent, aScriptURL,
                                          getter_AddRefs(loadInfo.mChannel));
    NS_ENSURE_SUCCESS(rv, rv);

    // Now that we've spun the loop there's no guarantee that our parent is
    // still alive.  We may have received control messages initiating shutdown.
    {
      MutexAutoLock lock(aParent->mMutex);
      parentStatus = aParent->mStatus;
    }

    if (parentStatus > Running) {
      nsCOMPtr<nsIThread> mainThread;
      if (NS_FAILED(NS_GetMainThread(getter_AddRefs(mainThread))) ||
          NS_FAILED(NS_ProxyRelease(mainThread, loadInfo.mChannel))) {
        NS_WARNING("Failed to proxy release of channel, leaking instead!");
      }
      return NS_ERROR_FAILURE;
    }

    loadInfo.mDomain = aParent->Domain();
  } else {
    AssertIsOnMainThread();

    nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
    MOZ_ASSERT(ssm);

    bool isChrome = nsContentUtils::IsCallerChrome();

    // First check to make sure the caller has permission to make a privileged
    // worker if they called the ChromeWorker/ChromeSharedWorker constructor.
    if (aIsChromeWorker && !isChrome) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }

    // Chrome callers (whether ChromeWorker of Worker) always get the system
    // principal here as they're allowed to load anything. The script loader may
    // change the principal later depending on the script uri.
    if (isChrome) {
      rv = ssm->GetSystemPrincipal(getter_AddRefs(loadInfo.mPrincipal));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // See if we're being called from a window.
    nsCOMPtr<nsPIDOMWindow> globalWindow = aWindow;
    if (!globalWindow) {
      nsCOMPtr<nsIScriptGlobalObject> scriptGlobal =
        nsJSUtils::GetStaticScriptGlobal(JS::CurrentGlobalOrNull(aCx));
      if (scriptGlobal) {
        globalWindow = do_QueryInterface(scriptGlobal);
        MOZ_ASSERT(globalWindow);
      }
    }

    nsCOMPtr<nsIDocument> document;

    if (globalWindow) {
      // Only use the current inner window, and only use it if the caller can
      // access it.
      nsPIDOMWindow* outerWindow = globalWindow->GetOuterWindow();
      if (outerWindow) {
        loadInfo.mWindow = outerWindow->GetCurrentInnerWindow();
      }

      if (!loadInfo.mWindow ||
          (globalWindow != loadInfo.mWindow &&
            !nsContentUtils::CanCallerAccess(loadInfo.mWindow))) {
        return NS_ERROR_DOM_SECURITY_ERR;
      }

      nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(loadInfo.mWindow);
      MOZ_ASSERT(sgo);

      loadInfo.mScriptContext = sgo->GetContext();
      NS_ENSURE_TRUE(loadInfo.mScriptContext, NS_ERROR_FAILURE);

      // If we're called from a window then we can dig out the principal and URI
      // from the document.
      document = loadInfo.mWindow->GetExtantDoc();
      NS_ENSURE_TRUE(document, NS_ERROR_FAILURE);

      loadInfo.mBaseURI = document->GetDocBaseURI();

      // Use the document's NodePrincipal as our principal if we're not being
      // called from chrome.
      if (!loadInfo.mPrincipal) {
        loadInfo.mPrincipal = document->NodePrincipal();
        NS_ENSURE_TRUE(loadInfo.mPrincipal, NS_ERROR_FAILURE);

        // We use the document's base domain to limit the number of workers
        // each domain can create. For sandboxed documents, we use the domain
        // of their first non-sandboxed document, walking up until we find
        // one. If we can't find one, we fall back to using the GUID of the
        // null principal as the base domain.
        if (document->GetSandboxFlags() & SANDBOXED_ORIGIN) {
          nsCOMPtr<nsIDocument> tmpDoc = document;
          do {
            tmpDoc = tmpDoc->GetParentDocument();
          } while (tmpDoc && tmpDoc->GetSandboxFlags() & SANDBOXED_ORIGIN);

          if (tmpDoc) {
            // There was an unsandboxed ancestor, yay!
            nsCOMPtr<nsIPrincipal> tmpPrincipal = tmpDoc->NodePrincipal();
            rv = tmpPrincipal->GetBaseDomain(loadInfo.mDomain);
            NS_ENSURE_SUCCESS(rv, rv);
          } else {
            // No unsandboxed ancestor, use our GUID.
            rv = loadInfo.mPrincipal->GetBaseDomain(loadInfo.mDomain);
            NS_ENSURE_SUCCESS(rv, rv);
          }
        } else {
          // Document creating the worker is not sandboxed.
          rv = loadInfo.mPrincipal->GetBaseDomain(loadInfo.mDomain);
          NS_ENSURE_SUCCESS(rv, rv);
        }
      }

      nsCOMPtr<nsIPermissionManager> permMgr =
        do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      uint32_t perm;
      rv = permMgr->TestPermissionFromPrincipal(loadInfo.mPrincipal, "systemXHR",
                                                &perm);
      NS_ENSURE_SUCCESS(rv, rv);

      loadInfo.mXHRParamsAllowed = perm == nsIPermissionManager::ALLOW_ACTION;
    } else {
      // Not a window
      MOZ_ASSERT(isChrome);

      // We're being created outside of a window. Need to figure out the script
      // that is creating us in order for us to use relative URIs later on.
      JS::Rooted<JSScript*> script(aCx);
      if (JS_DescribeScriptedCaller(aCx, &script, nullptr)) {
        const char* fileName = JS_GetScriptFilename(aCx, script);

        // In most cases, fileName is URI. In a few other cases
        // (e.g. xpcshell), fileName is a file path. Ideally, we would
        // prefer testing whether fileName parses as an URI and fallback
        // to file path in case of error, but Windows file paths have
        // the interesting property that they can be parsed as bogus
        // URIs (e.g. C:/Windows/Tmp is interpreted as scheme "C",
        // hostname "Windows", path "Tmp"), which defeats this algorithm.
        // Therefore, we adopt the opposite convention.
        nsCOMPtr<nsIFile> scriptFile =
          do_CreateInstance("@mozilla.org/file/local;1", &rv);
        if (NS_FAILED(rv)) {
          return rv;
        }

        rv = scriptFile->InitWithPath(NS_ConvertUTF8toUTF16(fileName));
        if (NS_SUCCEEDED(rv)) {
          rv = NS_NewFileURI(getter_AddRefs(loadInfo.mBaseURI),
                             scriptFile);
        }
        if (NS_FAILED(rv)) {
          // As expected, fileName is not a path, so proceed with
          // a uri.
          rv = NS_NewURI(getter_AddRefs(loadInfo.mBaseURI),
                         fileName);
        }
        if (NS_FAILED(rv)) {
          return rv;
        }
      }
      loadInfo.mXHRParamsAllowed = true;
    }

    MOZ_ASSERT(loadInfo.mPrincipal);
    MOZ_ASSERT(isChrome || !loadInfo.mDomain.IsEmpty());

    // XXXbent Use subject principal here instead of the one we already have?
    nsCOMPtr<nsIPrincipal> subjectPrincipal = ssm->GetCxSubjectPrincipal(aCx);
    MOZ_ASSERT(subjectPrincipal);

    if (!nsContentUtils::GetContentSecurityPolicy(aCx,
                                               getter_AddRefs(loadInfo.mCSP))) {
      NS_WARNING("Failed to get CSP!");
      return NS_ERROR_FAILURE;
    }

    if (loadInfo.mCSP) {
      rv = loadInfo.mCSP->GetAllowsEval(&loadInfo.mReportCSPViolations,
                                        &loadInfo.mEvalAllowed);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      loadInfo.mEvalAllowed = true;
      loadInfo.mReportCSPViolations = false;
    }

    rv = ChannelFromScriptURLMainThread(loadInfo.mPrincipal, loadInfo.mBaseURI,
                                        document, aScriptURL,
                                        getter_AddRefs(loadInfo.mChannel));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = NS_GetFinalChannelURI(loadInfo.mChannel,
                               getter_AddRefs(loadInfo.mResolvedScriptURI));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  aLoadInfo->StealFrom(loadInfo);
  return NS_OK;
}

void
WorkerPrivate::DoRunLoop(JSContext* aCx)
{
  AssertIsOnWorkerThread();
  MOZ_ASSERT(mThread);

  {
    MutexAutoLock lock(mMutex);
    mJSContext = aCx;

    MOZ_ASSERT(mStatus == Pending);
    mStatus = Running;
  }

  EnableMemoryReporter();

  InitializeGCTimers();

  Maybe<JSAutoCompartment> workerCompartment;

  for (;;) {
    // Workers lazily create a global object in CompileScriptRunnable. We need
    // to enter the global's compartment as soon as it has been created.
    if (workerCompartment.empty()) {
      if (JSObject* global = js::DefaultObjectForContextOrNull(aCx)) {
        workerCompartment.construct(aCx, global);
      }
    }

    Status currentStatus;
    bool normalRunnablesPending = false;

    {
      MutexAutoLock lock(mMutex);

      while (mControlQueue.IsEmpty() &&
             !(normalRunnablesPending = NS_HasPendingEvents(mThread))) {
        WaitForWorkerEvents();
      }

      ProcessAllControlRunnablesLocked();

      currentStatus = mStatus;
    }

    // If the close handler has finished and all features are done then we can
    // kill this thread.
    if (currentStatus != Running && !HasActiveFeatures()) {
      if (mCloseHandlerFinished && currentStatus != Killing) {
        if (!NotifyInternal(aCx, Killing)) {
          JS_ReportPendingException(aCx);
        }
#ifdef DEBUG
        {
          MutexAutoLock lock(mMutex);
          currentStatus = mStatus;
        }
        MOZ_ASSERT(currentStatus == Killing);
#else
        currentStatus = Killing;
#endif
      }

      // If we're supposed to die then we should exit the loop.
      if (currentStatus == Killing) {
        ShutdownGCTimers();

        DisableMemoryReporter();

        {
          MutexAutoLock lock(mMutex);

          mStatus = Dead;
          mJSContext = nullptr;
        }

        // After mStatus is set to Dead there can be no more
        // WorkerControlRunnables so no need to lock here.
        if (!mControlQueue.IsEmpty()) {
          WorkerControlRunnable* runnable;
          while (mControlQueue.Pop(runnable)) {
            runnable->Cancel();
            runnable->Release();
          }
        }

        // Clear away our MessagePorts.
        mWorkerPorts.Clear();

        // Unroot the global
        mScope = nullptr;

        return;
      }
    }

    // Nothing to do here if we don't have any runnables in the main queue.
    if (!normalRunnablesPending) {
      SetGCTimerMode(IdleTimer);
      continue;
    }

    MOZ_ASSERT(NS_HasPendingEvents(mThread));

    // Start the periodic GC timer if it is not already running.
    SetGCTimerMode(PeriodicTimer);

    // Process a single runnable from the main queue.
    MOZ_ALWAYS_TRUE(NS_ProcessNextEvent(mThread, false));

    if (NS_HasPendingEvents(mThread)) {
      // Now *might* be a good time to GC. Let the JS engine make the decision.
      if (!workerCompartment.empty()) {
        JS_MaybeGC(aCx);
      }
    }
    else {
      // The normal event queue has been exhausted, cancel the periodic GC timer
      // and schedule the idle GC timer.
      SetGCTimerMode(IdleTimer);
    }
  }

  MOZ_ASSUME_UNREACHABLE("Shouldn't get here!");
}

void
WorkerPrivate::OnProcessNextEvent(uint32_t aRecursionDepth)
{
  AssertIsOnWorkerThread();
  MOZ_ASSERT(aRecursionDepth);

  // Normally we process control runnables in DoRunLoop or RunCurrentSyncLoop.
  // However, it's possible that non-worker C++ could spin its own nested event
  // loop, and in that case we must ensure that we continue to process control
  // runnables here.
  if (aRecursionDepth > 1 &&
      mSyncLoopStack.Length() < aRecursionDepth - 1) {
    ProcessAllControlRunnables();
  }
}

void
WorkerPrivate::AfterProcessNextEvent(uint32_t aRecursionDepth)
{
  AssertIsOnWorkerThread();
  MOZ_ASSERT(aRecursionDepth);
}

void
WorkerPrivate::InitializeGCTimers()
{
  AssertIsOnWorkerThread();

  // We need a timer for GC. The basic plan is to run a non-shrinking GC
  // periodically (PERIODIC_GC_TIMER_DELAY_SEC) while the worker is running.
  // Once the worker goes idle we set a short (IDLE_GC_TIMER_DELAY_SEC) timer to
  // run a shrinking GC. If the worker receives more messages then the short
  // timer is canceled and the periodic timer resumes.
  mGCTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  MOZ_ASSERT(mGCTimer);

  nsRefPtr<GarbageCollectRunnable> runnable =
    new GarbageCollectRunnable(this, false, false);
  mPeriodicGCTimerTarget = new TimerThreadEventTarget(this, runnable);

  runnable = new GarbageCollectRunnable(this, true, false);
  mIdleGCTimerTarget = new TimerThreadEventTarget(this, runnable);

  mPeriodicGCTimerRunning = false;
  mIdleGCTimerRunning = false;
}

void
WorkerPrivate::SetGCTimerMode(GCTimerMode aMode)
{
  AssertIsOnWorkerThread();
  MOZ_ASSERT(mGCTimer);
  MOZ_ASSERT(mPeriodicGCTimerTarget);
  MOZ_ASSERT(mIdleGCTimerTarget);

  if ((aMode == PeriodicTimer && mPeriodicGCTimerRunning) ||
      (aMode == IdleTimer && mIdleGCTimerRunning)) {
    return;
  }

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(mGCTimer->Cancel()));

  mPeriodicGCTimerRunning = false;
  mIdleGCTimerRunning = false;

  LOG(("Worker %p canceled GC timer because %s\n", this,
       aMode == PeriodicTimer ?
       "periodic" :
       aMode == IdleTimer ? "idle" : "none"));

  if (aMode == NoTimer) {
    return;
  }

  MOZ_ASSERT(aMode == PeriodicTimer || aMode == IdleTimer);

  nsIEventTarget* target;
  uint32_t delay;
  int16_t type;

  if (aMode == PeriodicTimer) {
    target = mPeriodicGCTimerTarget;
    delay = PERIODIC_GC_TIMER_DELAY_SEC * 1000;
    type = nsITimer::TYPE_REPEATING_SLACK;
  }
  else {
    target = mIdleGCTimerTarget;
    delay = IDLE_GC_TIMER_DELAY_SEC * 1000;
    type = nsITimer::TYPE_ONE_SHOT;
  }

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(mGCTimer->SetTarget(target)));
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(mGCTimer->InitWithFuncCallback(DummyCallback,
                                                              nullptr, delay,
                                                              type)));

  if (aMode == PeriodicTimer) {
    LOG(("Worker %p scheduled periodic GC timer\n", this));
    mPeriodicGCTimerRunning = true;
  }
  else {
    LOG(("Worker %p scheduled idle GC timer\n", this));
    mIdleGCTimerRunning = true;
  }
}

void
WorkerPrivate::ShutdownGCTimers()
{
  AssertIsOnWorkerThread();

  MOZ_ASSERT(mGCTimer);

  // Always make sure the timer is canceled.
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(mGCTimer->Cancel()));

  LOG(("Worker %p killed the GC timer\n", this));

  mGCTimer = nullptr;
  mPeriodicGCTimerTarget = nullptr;
  mIdleGCTimerTarget = nullptr;
  mPeriodicGCTimerRunning = false;
  mIdleGCTimerRunning = false;
}

bool
WorkerPrivate::OperationCallback(JSContext* aCx)
{
  AssertIsOnWorkerThread();

  bool mayContinue = true;
  bool scheduledIdleGC = false;

  for (;;) {
    // Run all control events now.
    mayContinue = ProcessAllControlRunnables();

    bool maySuspend = mSuspended;
    if (maySuspend) {
      MutexAutoLock lock(mMutex);
      maySuspend = mStatus <= Running;
    }

    if (!mayContinue || !maySuspend) {
      break;
    }

    // Cancel the periodic GC timer here before suspending. The idle GC timer
    // will clean everything up once it runs.
    if (!scheduledIdleGC) {
      SetGCTimerMode(IdleTimer);
      scheduledIdleGC = true;
    }

    while ((mayContinue = MayContinueRunning())) {
      MutexAutoLock lock(mMutex);
      if (!mControlQueue.IsEmpty()) {
        break;
      }

      WaitForWorkerEvents(PR_MillisecondsToInterval(RemainingRunTimeMS()));
    }
  }

  if (!mayContinue) {
    // We want only uncatchable exceptions here.
    NS_ASSERTION(!JS_IsExceptionPending(aCx),
                 "Should not have an exception set here!");
    return false;
  }

  // Make sure the periodic timer gets turned back on here.
  SetGCTimerMode(PeriodicTimer);

  return true;
}

nsresult
WorkerPrivate::IsOnCurrentThread(bool* aIsOnCurrentThread)
{
  // May be called on any thread!

  MOZ_ASSERT(aIsOnCurrentThread);

  nsCOMPtr<nsIThread> thread;
  {
    MutexAutoLock lock(mMutex);
    thread = mThread;
  }

  if (!thread) {
    NS_WARNING("Trying to test thread correctness after the worker has "
               "released its thread!");
    return NS_ERROR_FAILURE;
  }

  nsresult rv = thread->IsOnCurrentThread(aIsOnCurrentThread);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void
WorkerPrivate::ScheduleDeletion()
{
  AssertIsOnWorkerThread();
  MOZ_ASSERT(mChildWorkers.IsEmpty());
  MOZ_ASSERT(mSyncLoopStack.IsEmpty());

  ClearMainEventQueue();

  if (WorkerPrivate* parent = GetParent()) {
    nsRefPtr<WorkerFinishedRunnable> runnable =
      new WorkerFinishedRunnable(parent, this);
    if (!runnable->Dispatch(nullptr)) {
      NS_WARNING("Failed to dispatch runnable!");
    }
  }
  else {
    nsRefPtr<TopLevelWorkerFinishedRunnable> runnable =
      new TopLevelWorkerFinishedRunnable(this);
    if (NS_FAILED(NS_DispatchToMainThread(runnable, NS_DISPATCH_NORMAL))) {
      NS_WARNING("Failed to dispatch runnable!");
    }
  }
}

bool
WorkerPrivate::BlockAndCollectRuntimeStats(JS::RuntimeStats* aRtStats)
{
  AssertIsOnMainThread();
  mMutex.AssertCurrentThreadOwns();
  NS_ASSERTION(aRtStats, "Null RuntimeStats!");

  NS_ASSERTION(!mMemoryReporterRunning, "How can we get reentered here?!");

  // This signals the worker that it should block itself as soon as possible.
  mMemoryReporterRunning = true;

  NS_ASSERTION(mJSContext, "This must never be null!");
  JSRuntime* rt = JS_GetRuntime(mJSContext);

  // If the worker is not already blocked (e.g. waiting for a worker event or
  // currently in a ctypes call) then we need to trigger the operation
  // callback to trap the worker.
  if (!mBlockedForMemoryReporter) {
    JS_TriggerOperationCallback(rt);

    // Wait until the worker actually blocks.
    while (!mBlockedForMemoryReporter) {
      mMemoryReportCondVar.Wait();
    }
  }

  bool succeeded = false;

  // If mMemoryReporter is still set then we can do the actual report. Otherwise
  // we're trying to shut down and we don't want to do anything but clean up.
  if (mMemoryReporter) {
    // Don't hold the lock while doing the actual report.
    MutexAutoUnlock unlock(mMutex);
    succeeded = JS::CollectRuntimeStats(rt, aRtStats, nullptr);
  }

  NS_ASSERTION(mMemoryReporterRunning, "This isn't possible!");
  NS_ASSERTION(mBlockedForMemoryReporter, "Somehow we got unblocked!");

  // Tell the worker that it can now continue its execution.
  mMemoryReporterRunning = false;

  // The worker may be waiting so we must notify.
  mMemoryReportCondVar.Notify();

  return succeeded;
}

void
WorkerPrivate::EnableMemoryReporter()
{
  AssertIsOnWorkerThread();
  MOZ_ASSERT(!mMemoryReporter);

  // No need to lock here since the main thread can't race until we've
  // successfully registered the reporter.
  mMemoryReporter = new MemoryReporter(this);

  if (NS_FAILED(RegisterWeakMemoryReporter(mMemoryReporter))) {
    NS_WARNING("Failed to register memory reporter!");
    // No need to lock here since a failed registration means our memory
    // reporter can't start running. Just clean up.
    mMemoryReporter = nullptr;
  }
}

void
WorkerPrivate::DisableMemoryReporter()
{
  AssertIsOnWorkerThread();

  nsRefPtr<MemoryReporter> memoryReporter;
  {
    MutexAutoLock lock(mMutex);

    // There is nothing to do here if the memory reporter was never successfully
    // registered.
    if (!mMemoryReporter) {
      return;
    }

    // We don't need this set any longer. Swap it out so that we can unregister
    // below.
    mMemoryReporter.swap(memoryReporter);

    // Next disable the memory reporter so that the main thread stops trying to
    // signal us.
    memoryReporter->Disable();

    // If the memory reporter is waiting to start then we need to wait for it to
    // finish.
    if (mMemoryReporterRunning) {
      NS_ASSERTION(!mBlockedForMemoryReporter,
                   "Can't be blocked in more than one place at the same time!");
      mBlockedForMemoryReporter = true;

      // Tell the main thread that we're blocked.
      mMemoryReportCondVar.Notify();

      // Wait for it the main thread to finish. Since we swapped out
      // mMemoryReporter above the main thread should respond quickly.
      while (mMemoryReporterRunning) {
        mMemoryReportCondVar.Wait();
      }

      NS_ASSERTION(mBlockedForMemoryReporter, "Somehow we got unblocked!");
      mBlockedForMemoryReporter = false;
    }
  }

  // Finally unregister the memory reporter.
  if (NS_FAILED(UnregisterWeakMemoryReporter(memoryReporter))) {
    NS_WARNING("Failed to unregister memory reporter!");
  }
}

void
WorkerPrivate::WaitForWorkerEvents(PRIntervalTime aInterval)
{
  AssertIsOnWorkerThread();
  mMutex.AssertCurrentThreadOwns();

  NS_ASSERTION(!mBlockedForMemoryReporter,
                "Can't be blocked in more than one place at the same time!");

  // Let the main thread know that the worker is blocked and that memory
  // reporting may proceed.
  mBlockedForMemoryReporter = true;

  // The main thread may be waiting so we must notify.
  mMemoryReportCondVar.Notify();

  // Now wait for an actual worker event.
  mCondVar.Wait(aInterval);

  // We've gotten some kind of signal but we can't continue until the memory
  // reporter has finished. Wait again.
  while (mMemoryReporterRunning) {
    mMemoryReportCondVar.Wait();
  }

  NS_ASSERTION(mBlockedForMemoryReporter, "Somehow we got unblocked!");

  // No need to notify here as the main thread isn't watching for this state.
  mBlockedForMemoryReporter = false;
}

bool
WorkerPrivate::ProcessAllControlRunnablesLocked()
{
  AssertIsOnWorkerThread();
  mMutex.AssertCurrentThreadOwns();

  bool result = true;

  for (;;) {
    // Block here if the memory reporter is trying to run.
    if (mMemoryReporterRunning) {
      MOZ_ASSERT(!mBlockedForMemoryReporter);

      // Let the main thread know that we've received the block request and
      // that memory reporting may proceed.
      mBlockedForMemoryReporter = true;

      // The main thread is almost certainly waiting so we must notify here.
      mMemoryReportCondVar.Notify();

      // Wait for the memory report to finish.
      while (mMemoryReporterRunning) {
        mMemoryReportCondVar.Wait();
      }

      MOZ_ASSERT(mBlockedForMemoryReporter);

      // No need to notify here as the main thread isn't watching for this
      // state.
      mBlockedForMemoryReporter = false;
    }

    WorkerControlRunnable* event;
    if (!mControlQueue.Pop(event)) {
      break;
    }

    MutexAutoUnlock unlock(mMutex);

    MOZ_ASSERT(event);
    if (NS_FAILED(static_cast<nsIRunnable*>(event)->Run())) {
      result = false;
    }

    event->Release();
  }

  return result;
}

void
WorkerPrivate::ClearMainEventQueue()
{
  AssertIsOnWorkerThread();

  nsIThread* currentThread = NS_GetCurrentThread();
  MOZ_ASSERT(currentThread);

  MOZ_ASSERT(!mCancelAllPendingRunnables);
  mCancelAllPendingRunnables = true;

  NS_ProcessPendingEvents(currentThread);
  MOZ_ASSERT(!NS_HasPendingEvents(currentThread));

  MOZ_ASSERT(mCancelAllPendingRunnables);
  mCancelAllPendingRunnables = false;
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
WorkerPrivate::TraceTimeouts(const TraceCallbacks& aCallbacks,
                             void* aClosure) const
{
  AssertIsOnWorkerThread();

  for (uint32_t index = 0; index < mTimeouts.Length(); index++) {
    TimeoutInfo* info = mTimeouts[index];

    if (info->mTimeoutCallable.isUndefined()) {
      continue;
    }

    aCallbacks.Trace(&info->mTimeoutCallable, "mTimeoutCallable", aClosure);
    for (uint32_t index2 = 0; index2 < info->mExtraArgVals.Length(); index2++) {
      aCallbacks.Trace(&info->mExtraArgVals[index2], "mExtraArgVals[i]", aClosure);
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

#ifdef DEBUG
  {
    Status currentStatus;
    {
      MutexAutoLock lock(mMutex);
      currentStatus = mStatus;
    }

    MOZ_ASSERT(currentStatus == Running);
  }
#endif

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

already_AddRefed<nsIEventTarget>
WorkerPrivate::CreateNewSyncLoop()
{
  AssertIsOnWorkerThread();

  nsCOMPtr<nsIThreadInternal> thread = do_QueryInterface(NS_GetCurrentThread());
  MOZ_ASSERT(thread);

  nsCOMPtr<nsIEventTarget> realEventTarget;
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(thread->PushEventQueue(
                                             getter_AddRefs(realEventTarget))));

  nsRefPtr<EventTarget> workerEventTarget =
    new EventTarget(this, realEventTarget);

  {
    // Modifications must be protected by mMutex in DEBUG builds, see comment
    // about mSyncLoopStack in WorkerPrivate.h.
#ifdef DEBUG
    MutexAutoLock lock(mMutex);
#endif

    mSyncLoopStack.AppendElement(new SyncLoopInfo(workerEventTarget));
  }

  return workerEventTarget.forget();
}

bool
WorkerPrivate::RunCurrentSyncLoop()
{
  AssertIsOnWorkerThread();

  JSContext* cx = GetJSContext();
  MOZ_ASSERT(cx);

  // This should not change between now and the time we finish running this sync
  // loop.
  uint32_t currentLoopIndex = mSyncLoopStack.Length() - 1;

  SyncLoopInfo* loopInfo = mSyncLoopStack[currentLoopIndex];

  MOZ_ASSERT(loopInfo);
  MOZ_ASSERT(!loopInfo->mHasRun);
  MOZ_ASSERT(!loopInfo->mCompleted);

#ifdef DEBUG
  loopInfo->mHasRun = true;
#endif

  nsCOMPtr<nsIThreadInternal> thread = do_QueryInterface(mThread);
  MOZ_ASSERT(thread);

  while (!loopInfo->mCompleted) {
    bool normalRunnablesPending = false;

    // Don't block with the periodic GC timer running.
    if (!NS_HasPendingEvents(thread)) {
      SetGCTimerMode(IdleTimer);
    }

    // Wait for something to do.
    {
      MutexAutoLock lock(mMutex);

      for (;;) {
        while (mControlQueue.IsEmpty() &&
               !normalRunnablesPending &&
               !(normalRunnablesPending = NS_HasPendingEvents(thread))) {
          WaitForWorkerEvents();
        }

        ProcessAllControlRunnablesLocked();

        if (normalRunnablesPending) {
          break;
        }
      }
    }

    // Make sure the periodic timer is running before we continue.
    SetGCTimerMode(PeriodicTimer);

    MOZ_ALWAYS_TRUE(NS_ProcessNextEvent(thread, false));

    // Now *might* be a good time to GC. Let the JS engine make the decision.
    JS_MaybeGC(cx);
  }

  // Make sure that the stack didn't change underneath us.
  MOZ_ASSERT(!mSyncLoopStack.IsEmpty());
  MOZ_ASSERT(mSyncLoopStack.Length() - 1 == currentLoopIndex);
  MOZ_ASSERT(mSyncLoopStack[currentLoopIndex] == loopInfo);

  // We're about to delete |loop|, stash its event target and result.
  nsIEventTarget* nestedEventTarget =
    loopInfo->mEventTarget->GetWeakNestedEventTarget();
  MOZ_ASSERT(nestedEventTarget);

  bool result = loopInfo->mResult;

  {
    // Modifications must be protected by mMutex in DEBUG builds, see comment
    // about mSyncLoopStack in WorkerPrivate.h.
#ifdef DEBUG
    MutexAutoLock lock(mMutex);
#endif

    // This will delete |loop|!
    mSyncLoopStack.RemoveElementAt(currentLoopIndex);
  }

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(thread->PopEventQueue(nestedEventTarget)));

  return result;
}

void
WorkerPrivate::StopSyncLoop(nsIEventTarget* aSyncLoopTarget, bool aResult)
{
  AssertIsOnWorkerThread();
  AssertValidSyncLoop(aSyncLoopTarget);

  MOZ_ASSERT(!mSyncLoopStack.IsEmpty());

  for (uint32_t index = mSyncLoopStack.Length(); index > 0; index--) {
    nsAutoPtr<SyncLoopInfo>& loopInfo = mSyncLoopStack[index - 1];
    MOZ_ASSERT(loopInfo);
    MOZ_ASSERT(loopInfo->mEventTarget);

    if (loopInfo->mEventTarget == aSyncLoopTarget) {
      // Can't assert |loop->mHasRun| here because dispatch failures can cause
      // us to bail out early.
      MOZ_ASSERT(!loopInfo->mCompleted);

      loopInfo->mResult = aResult;
      loopInfo->mCompleted = true;

      loopInfo->mEventTarget->Disable();

      return;
    }

    MOZ_ASSERT(!SameCOMIdentity(loopInfo->mEventTarget, aSyncLoopTarget));
  }

  MOZ_CRASH("Unknown sync loop!");
}

#ifdef DEBUG
void
WorkerPrivate::AssertValidSyncLoop(nsIEventTarget* aSyncLoopTarget)
{
  MOZ_ASSERT(aSyncLoopTarget);

  EventTarget* workerTarget;
  nsresult rv =
    aSyncLoopTarget->QueryInterface(kDEBUGWorkerEventTargetIID,
                                    reinterpret_cast<void**>(&workerTarget));
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  MOZ_ASSERT(workerTarget);

  bool valid = false;

  {
    MutexAutoLock lock(mMutex);

    for (uint32_t index = 0; index < mSyncLoopStack.Length(); index++) {
      nsAutoPtr<SyncLoopInfo>& loopInfo = mSyncLoopStack[index];
      MOZ_ASSERT(loopInfo);
      MOZ_ASSERT(loopInfo->mEventTarget);

      if (loopInfo->mEventTarget == aSyncLoopTarget) {
        valid = true;
        break;
      }

      MOZ_ASSERT(!SameCOMIdentity(loopInfo->mEventTarget, aSyncLoopTarget));
    }
  }

  MOZ_ASSERT(valid);
}
#endif

void
WorkerPrivate::PostMessageToParentInternal(
                            JSContext* aCx,
                            JS::Handle<JS::Value> aMessage,
                            const Optional<Sequence<JS::Value>>& aTransferable,
                            bool aToMessagePort,
                            uint64_t aMessagePortSerial,
                            ErrorResult& aRv)
{
  AssertIsOnWorkerThread();

  JS::Rooted<JS::Value> transferable(aCx, JS::UndefinedValue());
  if (aTransferable.WasPassed()) {
    const Sequence<JS::Value>& realTransferable = aTransferable.Value();
    JSObject* array =
      JS_NewArrayObject(aCx, realTransferable.Length(),
                        const_cast<jsval*>(realTransferable.Elements()));
    if (!array) {
      aRv = NS_ERROR_OUT_OF_MEMORY;
      return;
    }
    transferable.setObject(*array);
  }

  JSStructuredCloneCallbacks* callbacks =
    IsChromeWorker() ?
    &gChromeWorkerStructuredCloneCallbacks :
    &gWorkerStructuredCloneCallbacks;

  nsTArray<nsCOMPtr<nsISupports>> clonedObjects;

  JSAutoStructuredCloneBuffer buffer;
  if (!buffer.write(aCx, aMessage, transferable, callbacks, &clonedObjects)) {
    aRv = NS_ERROR_DOM_DATA_CLONE_ERR;
    return;
  }

  nsRefPtr<MessageEventRunnable> runnable =
    new MessageEventRunnable(this,
                             WorkerRunnable::ParentThreadUnchangedBusyCount,
                             buffer, clonedObjects, aToMessagePort,
                             aMessagePortSerial);
  if (!runnable->Dispatch(aCx)) {
    aRv = NS_ERROR_FAILURE;
  }
}

void
WorkerPrivate::PostMessageToParentMessagePort(
                             JSContext* aCx,
                             uint64_t aMessagePortSerial,
                             JS::Handle<JS::Value> aMessage,
                             const Optional<Sequence<JS::Value>>& aTransferable,
                             ErrorResult& aRv)
{
  AssertIsOnWorkerThread();

  if (!mWorkerPorts.GetWeak(aMessagePortSerial)) {
    // This port has been closed from the main thread. There's no point in
    // sending this message so just bail.
    return;
  }

  PostMessageToParentInternal(aCx, aMessage, aTransferable, true,
                              aMessagePortSerial, aRv);
}

bool
WorkerPrivate::NotifyInternal(JSContext* aCx, Status aStatus)
{
  AssertIsOnWorkerThread();

  NS_ASSERTION(aStatus > Running && aStatus < Dead, "Bad status!");

  nsRefPtr<EventTarget> eventTarget;

  // Save the old status and set the new status.
  Status previousStatus;
  {
    MutexAutoLock lock(mMutex);

    if (mStatus >= aStatus) {
      MOZ_ASSERT(!mEventTarget);
      return true;
    }

    previousStatus = mStatus;
    mStatus = aStatus;

    mEventTarget.swap(eventTarget);
  }

  // Now that mStatus > Running, no-one can create a new WorkerEventTarget or
  // WorkerCrossThreadDispatcher if we don't already have one.
  if (eventTarget) {
    // Since we'll no longer process events, make sure we no longer allow anyone
    // to post them. We have to do this without mMutex held, since our mutex
    // must be acquired *after* the WorkerEventTarget's mutex when they're both
    // held.
    eventTarget->Disable();
    eventTarget = nullptr;
  }

  if (mCrossThreadDispatcher) {
    // Since we'll no longer process events, make sure we no longer allow
    // anyone to post them. We have to do this without mMutex held, since our
    // mutex must be acquired *after* mCrossThreadDispatcher's mutex when
    // they're both held.
    mCrossThreadDispatcher->Forget();
    mCrossThreadDispatcher = nullptr;
  }

  MOZ_ASSERT(previousStatus != Pending);

  MOZ_ASSERT(previousStatus >= Canceling || mKillTime.IsNull());

  // Let all our features know the new status.
  NotifyFeatures(aCx, aStatus);

  // If this is the first time our status has changed then we need to clear the
  // main event queue.
  if (previousStatus == Running) {
    ClearMainEventQueue();
  }

  // If we've run the close handler, we don't need to do anything else.
  if (mCloseHandlerFinished) {
    return true;
  }

  // If the worker script never ran, or failed to compile, we don't need to do
  // anything else, except pretend that we ran the close handler.
  if (!JS::CurrentGlobalOrNull(aCx)) {
    mCloseHandlerStarted = true;
    mCloseHandlerFinished = true;
    return true;
  }

  // If this is the first time our status has changed we also need to schedule
  // the close handler unless we're being shut down.
  if (previousStatus == Running && aStatus != Killing) {
    MOZ_ASSERT(!mCloseHandlerStarted && !mCloseHandlerFinished);

    nsRefPtr<CloseEventRunnable> closeRunnable = new CloseEventRunnable(this);
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToCurrentThread(closeRunnable)));
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
    MOZ_ASSERT(previousStatus >= Running && previousStatus <= Terminating);

    uint32_t killSeconds = IsChromeWorker() ?
      RuntimeService::GetChromeCloseHandlerTimeoutSeconds() :
      RuntimeService::GetContentCloseHandlerTimeoutSeconds();

    if (killSeconds) {
      mKillTime = TimeStamp::Now() + TimeDuration::FromSeconds(killSeconds);

      if (!mCloseHandlerFinished && !ScheduleKillCloseEventRunnable(aCx)) {
        return false;
      }
    }

    // Only abort the script if we're not yet running the close handler.
    return mCloseHandlerStarted;
  }

  MOZ_ASSERT(aStatus == Killing);

  mKillTime = TimeStamp::Now();

  if (mCloseHandlerStarted && !mCloseHandlerFinished) {
    ScheduleKillCloseEventRunnable(aCx);
  }

  // Always abort the script.
  return false;
}

bool
WorkerPrivate::ScheduleKillCloseEventRunnable(JSContext* aCx)
{
  AssertIsOnWorkerThread();
  MOZ_ASSERT(!mKillTime.IsNull());

  nsRefPtr<KillCloseEventRunnable> killCloseEventRunnable =
    new KillCloseEventRunnable(this);
  if (!killCloseEventRunnable->SetTimeout(aCx, RemainingRunTimeMS())) {
    return false;
  }

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToCurrentThread(
                                                      killCloseEventRunnable)));

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

int32_t
WorkerPrivate::SetTimeout(JSContext* aCx,
                          Function* aHandler,
                          const nsAString& aStringHandler,
                          int32_t aTimeout,
                          const Sequence<JS::Value>& aArguments,
                          bool aIsInterval,
                          ErrorResult& aRv)
{
  AssertIsOnWorkerThread();

  const int32_t timerId = mNextTimeoutId++;

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
    aRv.Throw(NS_ERROR_FAILURE);
    return 0;
  }

  nsAutoPtr<TimeoutInfo> newInfo(new TimeoutInfo());
  newInfo->mIsInterval = aIsInterval;
  newInfo->mId = timerId;

  if (MOZ_UNLIKELY(timerId == INT32_MAX)) {
    NS_WARNING("Timeout ids overflowed!");
    mNextTimeoutId = 1;
  }

  // Take care of the main argument.
  if (aHandler) {
    newInfo->mTimeoutCallable = JS::ObjectValue(*aHandler->Callable());
  }
  else if (!aStringHandler.IsEmpty()) {
    newInfo->mTimeoutString = aStringHandler;
  }
  else {
    JS_ReportError(aCx, "Useless %s call (missing quotes around argument?)",
                   aIsInterval ? "setInterval" : "setTimeout");
    return 0;
  }

  // See if any of the optional arguments were passed.
  aTimeout = std::max(0, aTimeout);
  newInfo->mInterval = TimeDuration::FromMilliseconds(aTimeout);

  uint32_t argc = aArguments.Length();
  if (argc && !newInfo->mTimeoutCallable.isUndefined()) {
    nsTArray<JS::Heap<JS::Value>> extraArgVals(argc);
    for (uint32_t index = 0; index < argc; index++) {
      extraArgVals.AppendElement(aArguments[index]);
    }
    newInfo->mExtraArgVals.SwapElements(extraArgVals);
  }

  newInfo->mTargetTime = TimeStamp::Now() + newInfo->mInterval;

  if (!newInfo->mTimeoutString.IsEmpty()) {
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

  nsAutoPtr<TimeoutInfo>* insertedInfo =
    mTimeouts.InsertElementSorted(newInfo.forget(), GetAutoPtrComparator(mTimeouts));

  // If the timeout we just made is set to fire next then we need to update the
  // timer.
  if (insertedInfo == mTimeouts.Elements()) {
    nsresult rv;

    if (!mTimer) {
      nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
      if (NS_FAILED(rv)) {
        aRv.Throw(rv);
        return 0;
      }

      nsRefPtr<TimerRunnable> runnable = new TimerRunnable(this);

      nsRefPtr<TimerThreadEventTarget> target =
        new TimerThreadEventTarget(this, runnable);

      rv = timer->SetTarget(target);
      if (NS_FAILED(rv)) {
        aRv.Throw(rv);
        return 0;
      }

      timer.swap(mTimer);
    }

    if (!mTimerRunning) {
      if (!ModifyBusyCountFromWorker(aCx, true)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return 0;
      }
      mTimerRunning = true;
    }

    if (!RescheduleTimeoutTimer(aCx)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return 0;
    }
  }

  return timerId;
}

void
WorkerPrivate::ClearTimeout(int32_t aId)
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
  JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));
  JSPrincipals* principal = GetWorkerPrincipal();

  // We want to make sure to run *something*, even if the timer fired a little
  // early. Fudge the value of now to at least include the first timeout.
  const TimeStamp now = std::max(TimeStamp::Now(), mTimeouts[0]->mTargetTime);

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

    if (!info->mTimeoutCallable.isUndefined()) {
      JS::Rooted<JS::Value> rval(aCx);
      /*
       * unsafeGet() is needed below because the argument is a not a const
       * pointer, even though values are not modified.
       */
      if (!JS_CallFunctionValue(aCx, global, info->mTimeoutCallable,
                                info->mExtraArgVals.Length(),
                                info->mExtraArgVals.Elements()->unsafeGet(),
                                rval.address()) &&
          !JS_ReportPendingException(aCx)) {
        retval = false;
        break;
      }
    }
    else {
      nsString expression = info->mTimeoutString;

      JS::CompileOptions options(aCx);
      options.setPrincipals(principal)
        .setFileAndLine(info->mFilename.get(), info->mLineNumber);

      if ((expression.IsEmpty() ||
           !JS::Evaluate(aCx, global, options, expression.get(),
                         expression.Length(), nullptr)) &&
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
  uint32_t delay = delta > 0 ? std::min(delta, double(UINT32_MAX)) : 0;

  nsresult rv = mTimer->InitWithFuncCallback(DummyCallback, nullptr, delay,
                                             nsITimer::TYPE_ONE_SHOT);
  if (NS_FAILED(rv)) {
    JS_ReportError(aCx, "Failed to start timer!");
    return false;
  }

  return true;
}

void
WorkerPrivate::UpdateJSContextOptionsInternal(JSContext* aCx,
                                              const JS::ContextOptions& aContentOptions,
                                              const JS::ContextOptions& aChromeOptions)
{
  AssertIsOnWorkerThread();

  JS::ContextOptionsRef(aCx) = IsChromeWorker() ? aChromeOptions : aContentOptions;

  for (uint32_t index = 0; index < mChildWorkers.Length(); index++) {
    mChildWorkers[index]->UpdateJSContextOptions(aCx, aContentOptions,
                                                 aChromeOptions);
  }
}

void
WorkerPrivate::UpdatePreferenceInternal(JSContext* aCx, WorkerPreference aPref, bool aValue)
{
  AssertIsOnWorkerThread();
  MOZ_ASSERT(aPref >= 0 && aPref < WORKERPREF_COUNT);

  mPreferences[aPref] = aValue;

  for (uint32_t index = 0; index < mChildWorkers.Length(); index++) {
    mChildWorkers[index]->UpdatePreference(aCx, aPref, aValue);
  }
}

void
WorkerPrivate::UpdateJSWorkerMemoryParameterInternal(JSContext* aCx,
                                                     JSGCParamKey aKey,
                                                     uint32_t aValue)
{
  AssertIsOnWorkerThread();

  // XXX aValue might be 0 here (telling us to unset a previous value for child
  // workers). Calling JS_SetGCParameter with a value of 0 isn't actually
  // supported though. We really need some way to revert to a default value
  // here.
  if (aValue) {
    JS_SetGCParameter(JS_GetRuntime(aCx), aKey, aValue);
  }

  for (uint32_t index = 0; index < mChildWorkers.Length(); index++) {
    mChildWorkers[index]->UpdateJSWorkerMemoryParameter(aCx, aKey, aValue);
  }
}

#ifdef JS_GC_ZEAL
void
WorkerPrivate::UpdateGCZealInternal(JSContext* aCx, uint8_t aGCZeal,
                                    uint32_t aFrequency)
{
  AssertIsOnWorkerThread();

  JS_SetGCZeal(aCx, aGCZeal, aFrequency);

  for (uint32_t index = 0; index < mChildWorkers.Length(); index++) {
    mChildWorkers[index]->UpdateGCZeal(aCx, aGCZeal, aFrequency);
  }
}
#endif

void
WorkerPrivate::UpdateJITHardeningInternal(JSContext* aCx, bool aJITHardening)
{
  AssertIsOnWorkerThread();

  JS_SetJitHardening(JS_GetRuntime(aCx), aJITHardening);

  for (uint32_t index = 0; index < mChildWorkers.Length(); index++) {
    mChildWorkers[index]->UpdateJITHardening(aCx, aJITHardening);
  }
}

void
WorkerPrivate::GarbageCollectInternal(JSContext* aCx, bool aShrinking,
                                      bool aCollectChildren)
{
  AssertIsOnWorkerThread();

  if (!JS::CurrentGlobalOrNull(aCx)) {
    // We haven't compiled anything yet. Just bail out.
    return;
  }

  if (aShrinking || aCollectChildren) {
    JSRuntime* rt = JS_GetRuntime(aCx);
    JS::PrepareForFullGC(rt);

    if (aShrinking) {
      JS::ShrinkingGC(rt, JS::gcreason::DOM_WORKER);

      if (!aCollectChildren) {
        LOG(("Worker %p collected idle garbage\n", this));
      }
    }
    else {
      JS::GCForReason(rt, JS::gcreason::DOM_WORKER);
      LOG(("Worker %p collected garbage\n", this));
    }
  }
  else {
    JS_MaybeGC(aCx);
    LOG(("Worker %p collected periodic garbage\n", this));
  }

  if (aCollectChildren) {
    for (uint32_t index = 0; index < mChildWorkers.Length(); index++) {
      mChildWorkers[index]->GarbageCollect(aCx, aShrinking);
    }
  }
}

void
WorkerPrivate::CycleCollectInternal(JSContext* aCx, bool aCollectChildren)
{
  AssertIsOnWorkerThread();

  nsCycleCollector_collect(nullptr);

  if (aCollectChildren) {
    for (uint32_t index = 0; index < mChildWorkers.Length(); index++) {
      mChildWorkers[index]->CycleCollect(aCx, /* dummy = */ false);
    }
  }
}

void
WorkerPrivate::SetThread(nsIThread* aThread)
{
#ifdef DEBUG
  if (aThread) {
    bool isOnCurrentThread;
    MOZ_ASSERT(NS_SUCCEEDED(aThread->IsOnCurrentThread(&isOnCurrentThread)));
    MOZ_ASSERT(isOnCurrentThread);

    MOZ_ASSERT(!mPRThread);
    mPRThread = PRThreadFromThread(aThread);
    MOZ_ASSERT(mPRThread);
  }
  else {
    MOZ_ASSERT(mPRThread);
  }
#endif

  nsCOMPtr<nsIThread> doomedThread;

  { // Scope so that |doomedThread| is released without holding the lock.
    MutexAutoLock lock(mMutex);

    if (aThread) {
      MOZ_ASSERT(!mThread);
      MOZ_ASSERT(mStatus == Pending);

      mThread = aThread;

      if (!mPreStartRunnables.IsEmpty()) {
        for (uint32_t index = 0; index < mPreStartRunnables.Length(); index++) {
          MOZ_ALWAYS_TRUE(NS_SUCCEEDED(mThread->Dispatch(
                                                      mPreStartRunnables[index],
                                                      NS_DISPATCH_NORMAL)));
        }
        mPreStartRunnables.Clear();
      }
    }
    else {
      MOZ_ASSERT(mThread);
      mThread.swap(doomedThread);
    }
  }
}

WorkerCrossThreadDispatcher*
WorkerPrivate::GetCrossThreadDispatcher()
{
  MutexAutoLock lock(mMutex);

  if (!mCrossThreadDispatcher && mStatus <= Running) {
    mCrossThreadDispatcher = new WorkerCrossThreadDispatcher(this);
  }

  return mCrossThreadDispatcher;
}

void
WorkerPrivate::BeginCTypesCall()
{
  AssertIsOnWorkerThread();

  // Don't try to GC while we're blocked in a ctypes call.
  SetGCTimerMode(NoTimer);

  MutexAutoLock lock(mMutex);

  NS_ASSERTION(!mBlockedForMemoryReporter,
               "Can't be blocked in more than one place at the same time!");

  // Let the main thread know that the worker is effectively blocked while in
  // this ctypes call. It isn't technically true (obviously the call could do
  // non-blocking things), but we're assuming that ctypes can't call back into
  // JSAPI here and therefore any work the ctypes call does will not alter the
  // data structures of this JS runtime.
  mBlockedForMemoryReporter = true;

  // The main thread may be waiting on us so it must be notified.
  mMemoryReportCondVar.Notify();
}

void
WorkerPrivate::EndCTypesCall()
{
  AssertIsOnWorkerThread();

  {
    MutexAutoLock lock(mMutex);

    NS_ASSERTION(mBlockedForMemoryReporter, "Somehow we got unblocked!");

    // Don't continue until the memory reporter has finished.
    while (mMemoryReporterRunning) {
      mMemoryReportCondVar.Wait();
    }

    // No need to notify the main thread here as it shouldn't be waiting to see
    // this state.
    mBlockedForMemoryReporter = false;
  }

  // Make sure the periodic timer is running before we start running JS again.
  SetGCTimerMode(PeriodicTimer);
}

bool
WorkerPrivate::ConnectMessagePort(JSContext* aCx, uint64_t aMessagePortSerial)
{
  AssertIsOnWorkerThread();

  NS_ASSERTION(!mWorkerPorts.GetWeak(aMessagePortSerial),
               "Already have this port registered!");

  WorkerGlobalScope* globalScope = GlobalScope();

  JS::Rooted<JSObject*> jsGlobal(aCx, globalScope->GetWrapper());
  MOZ_ASSERT(jsGlobal);

  nsRefPtr<MessagePort> port = new MessagePort(this, aMessagePortSerial);

  GlobalObject globalObject(aCx, jsGlobal);
  if (globalObject.Failed()) {
    return false;
  }

  RootedDictionary<MessageEventInit> init(aCx);
  init.mBubbles = false;
  init.mCancelable = false;
  init.mSource.SetValue().SetAsMessagePort() = port;

  ErrorResult rv;

  nsRefPtr<nsDOMMessageEvent> event =
    nsDOMMessageEvent::Constructor(globalObject, aCx,
                                   NS_LITERAL_STRING("connect"), init, rv);

  event->SetTrusted(true);

  nsTArray<nsRefPtr<MessagePortBase>> ports;
  ports.AppendElement(port);

  nsRefPtr<MessagePortList> portList =
    new MessagePortList(static_cast<nsIDOMEventTarget*>(globalScope), ports);
  event->SetPorts(portList);

  mWorkerPorts.Put(aMessagePortSerial, port);

  nsCOMPtr<nsIDOMEvent> domEvent = do_QueryObject(event);

  nsEventStatus dummy = nsEventStatus_eIgnore;
  globalScope->DispatchDOMEvent(nullptr, domEvent, nullptr, &dummy);
  return true;
}

void
WorkerPrivate::DisconnectMessagePort(uint64_t aMessagePortSerial)
{
  AssertIsOnWorkerThread();

  mWorkerPorts.Remove(aMessagePortSerial);
}

workers::MessagePort*
WorkerPrivate::GetMessagePort(uint64_t aMessagePortSerial)
{
  AssertIsOnWorkerThread();

  nsRefPtr<MessagePort> port;
  if (mWorkerPorts.Get(aMessagePortSerial, getter_AddRefs(port))) {
    return port;
  }

  return nullptr;
}

JSObject*
WorkerPrivate::CreateGlobalScope(JSContext* aCx)
{
  AssertIsOnWorkerThread();

  nsRefPtr<WorkerGlobalScope> globalScope;
  if (IsSharedWorker()) {
    globalScope = new SharedWorkerGlobalScope(this, SharedWorkerName());
  }
  else {
    globalScope = new DedicatedWorkerGlobalScope(this);
  }

  JS::Rooted<JSObject*> global(aCx, globalScope->WrapGlobalObject(aCx));
  NS_ENSURE_TRUE(global, nullptr);

  JSAutoCompartment ac(aCx, global);

  if (!RegisterBindings(aCx, global)) {
    return nullptr;
  }

  mScope = globalScope.forget();

  JS_FireOnNewGlobalObject(aCx, global);

  return global;
}

#ifdef DEBUG

void
WorkerPrivate::AssertIsOnWorkerThread() const
{
  // This is much more complicated than it needs to be but we can't use mThread
  // because it must be protected by mMutex and sometimes this method is called
  // when mMutex is already locked. This method should always work.
  MOZ_ASSERT(mPRThread,
             "AssertIsOnWorkerThread() called before a thread was assigned!");

  MOZ_ASSERT(nsThreadManager::get());

  nsCOMPtr<nsIThread> thread;
  nsresult rv =
    nsThreadManager::get()->GetThreadFromPRThread(mPRThread,
                                                  getter_AddRefs(thread));
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  MOZ_ASSERT(thread);

  bool current;
  rv = thread->IsOnCurrentThread(&current);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  MOZ_ASSERT(current, "Wrong thread!");
}

#endif // DEBUG

NS_IMPL_ISUPPORTS_INHERITED0(ExternalRunnableWrapper, WorkerRunnable)

template <class Derived>
NS_IMPL_ADDREF(WorkerPrivateParent<Derived>::EventTarget)

template <class Derived>
NS_IMPL_RELEASE(WorkerPrivateParent<Derived>::EventTarget)

template <class Derived>
NS_INTERFACE_MAP_BEGIN(WorkerPrivateParent<Derived>::EventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
#ifdef DEBUG
  // kDEBUGWorkerEventTargetIID is special in that it does not AddRef its
  // result.
  if (aIID.Equals(kDEBUGWorkerEventTargetIID)) {
    *aInstancePtr = this;
    return NS_OK;
  }
  else
#endif
NS_INTERFACE_MAP_END

template <class Derived>
NS_IMETHODIMP
WorkerPrivateParent<Derived>::
EventTarget::Dispatch(nsIRunnable* aRunnable, uint32_t aFlags)
{
  // May be called on any thread!

  // Workers only support asynchronous dispatch for now.
  if (NS_WARN_IF(aFlags != NS_DISPATCH_NORMAL)) {
    return NS_ERROR_UNEXPECTED;
  }

  nsRefPtr<WorkerRunnable> workerRunnable;

  MutexAutoLock lock(mMutex);

  if (!mWorkerPrivate) {
    NS_WARNING("A runnable was posted to a worker that is already shutting "
               "down!");
    return NS_ERROR_UNEXPECTED;
  }

  if (aRunnable) {
    workerRunnable = mWorkerPrivate->MaybeWrapAsWorkerRunnable(aRunnable);
  }

  nsresult rv =
    mWorkerPrivate->DispatchPrivate(workerRunnable, mNestedEventTarget);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

template <class Derived>
NS_IMETHODIMP
WorkerPrivateParent<Derived>::
EventTarget::IsOnCurrentThread(bool* aIsOnCurrentThread)
{
  // May be called on any thread!

  MOZ_ASSERT(aIsOnCurrentThread);

  MutexAutoLock lock(mMutex);

  if (!mWorkerPrivate) {
    NS_WARNING("A worker's event target was used after the worker has !");
    return NS_ERROR_UNEXPECTED;
  }

  nsresult rv = mWorkerPrivate->IsOnCurrentThread(aIsOnCurrentThread);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

BEGIN_WORKERS_NAMESPACE

WorkerCrossThreadDispatcher*
GetWorkerCrossThreadDispatcher(JSContext* aCx, JS::Value aWorker)
{
  if (!aWorker.isObject()) {
    return nullptr;
  }

  WorkerPrivate* w = nullptr;
  UNWRAP_OBJECT(Worker, &aWorker.toObject(), w);
  MOZ_ASSERT(w);
  return w->GetCrossThreadDispatcher();
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

// Force instantiation.
template class WorkerPrivateParent<WorkerPrivate>;

END_WORKERS_NAMESPACE

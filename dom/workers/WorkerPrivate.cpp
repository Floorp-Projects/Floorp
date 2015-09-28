/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "nsIDOMMessageEvent.h"
#include "nsIDocument.h"
#include "nsIDocShell.h"
#include "nsIInterfaceRequestor.h"
#include "nsIMemoryReporter.h"
#include "nsINetworkInterceptController.h"
#include "nsIPermissionManager.h"
#include "nsIScriptError.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptSecurityManager.h"
#include "nsITabChild.h"
#include "nsITextToSubURI.h"
#include "nsIThreadInternal.h"
#include "nsITimer.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIWeakReferenceUtils.h"
#include "nsIWorkerDebugger.h"
#include "nsIXPConnect.h"
#include "nsPerformance.h"
#include "nsPIDOMWindow.h"

#include <algorithm>
#include "ImageContainer.h"
#include "jsfriendapi.h"
#include "js/MemoryMetrics.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/Likely.h"
#include "mozilla/LoadContext.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/ErrorEvent.h"
#include "mozilla/dom/ErrorEventBinding.h"
#include "mozilla/dom/Exceptions.h"
#include "mozilla/dom/FunctionBinding.h"
#include "mozilla/dom/ImageBitmap.h"
#include "mozilla/dom/ImageBitmapBinding.h"
#include "mozilla/dom/ImageData.h"
#include "mozilla/dom/ImageDataBinding.h"
#include "mozilla/dom/MessageEvent.h"
#include "mozilla/dom/MessageEventBinding.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "mozilla/dom/MessagePortList.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseDebugging.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/StructuredClone.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/WebCryptoCommon.h"
#include "mozilla/dom/WorkerBinding.h"
#include "mozilla/dom/WorkerDebuggerGlobalScopeBinding.h"
#include "mozilla/dom/WorkerGlobalScopeBinding.h"
#include "mozilla/dom/indexedDB/IDBFactory.h"
#include "mozilla/dom/ipc/BlobChild.h"
#include "mozilla/dom/ipc/nsIRemoteBlob.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/Preferences.h"
#include "MultipartBlobImpl.h"
#include "nsAlgorithm.h"
#include "nsContentUtils.h"
#include "nsCycleCollector.h"
#include "nsError.h"
#include "nsDOMJSUtils.h"
#include "nsFormData.h"
#include "nsHostObjectProtocolHandler.h"
#include "nsJSEnvironment.h"
#include "nsJSUtils.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "nsProxyRelease.h"
#include "nsQueryObject.h"
#include "nsSandboxFlags.h"
#include "prthread.h"
#include "xpcpublic.h"

#ifdef ANDROID
#include <android/log.h>
#endif

#ifdef DEBUG
#include "nsThreadManager.h"
#endif

#include "MessagePort.h"
#include "Navigator.h"
#include "Principal.h"
#include "RuntimeService.h"
#include "ScriptLoader.h"
#include "ServiceWorkerManager.h"
#include "ServiceWorkerWindowClient.h"
#include "SharedWorker.h"
#include "WorkerDebuggerManager.h"
#include "WorkerFeature.h"
#include "WorkerRunnable.h"
#include "WorkerScope.h"
#include "WorkerStructuredClone.h"
#include "WorkerThread.h"

#ifdef XP_WIN
#undef PostMessage
#endif

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
using namespace mozilla::ipc;

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
class ExternalRunnableWrapper final : public WorkerRunnable
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
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
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
  Cancel() override
  {
    nsresult rv = mWrappedRunnable->Cancel();
    nsresult rv2 = WorkerRunnable::Cancel();
    return NS_FAILED(rv) ? rv : rv2;
  }
};

struct WindowAction
{
  nsPIDOMWindow* mWindow;
  bool mDefaultAction;

  MOZ_IMPLICIT WindowAction(nsPIDOMWindow* aWindow)
  : mWindow(aWindow), mDefaultAction(true)
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

// Recursive!
already_AddRefed<BlobImpl>
EnsureBlobForBackgroundManager(BlobImpl* aBlobImpl,
                               PBackgroundChild* aManager = nullptr)
{
  MOZ_ASSERT(aBlobImpl);

  if (!aManager) {
    aManager = BackgroundChild::GetForCurrentThread();
    MOZ_ASSERT(aManager);
  }

  nsRefPtr<BlobImpl> blobImpl = aBlobImpl;

  const nsTArray<nsRefPtr<BlobImpl>>* subBlobImpls =
    aBlobImpl->GetSubBlobImpls();

  if (!subBlobImpls) {
    if (nsCOMPtr<nsIRemoteBlob> remoteBlob = do_QueryObject(blobImpl)) {
      // Always make sure we have a blob from an actor we can use on this
      // thread.
      BlobChild* blobChild = BlobChild::GetOrCreate(aManager, blobImpl);
      MOZ_ASSERT(blobChild);

      blobImpl = blobChild->GetBlobImpl();
      MOZ_ASSERT(blobImpl);

      DebugOnly<bool> isMutable;
      MOZ_ASSERT(NS_SUCCEEDED(blobImpl->GetMutable(&isMutable)));
      MOZ_ASSERT(!isMutable);
    } else {
      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(blobImpl->SetMutable(false)));
    }

    return blobImpl.forget();
  }

  const uint32_t subBlobCount = subBlobImpls->Length();
  MOZ_ASSERT(subBlobCount);

  nsTArray<nsRefPtr<BlobImpl>> newSubBlobImpls;
  newSubBlobImpls.SetLength(subBlobCount);

  bool newBlobImplNeeded = false;

  for (uint32_t index = 0; index < subBlobCount; index++) {
    const nsRefPtr<BlobImpl>& subBlobImpl = subBlobImpls->ElementAt(index);
    MOZ_ASSERT(subBlobImpl);

    nsRefPtr<BlobImpl>& newSubBlobImpl = newSubBlobImpls[index];

    newSubBlobImpl = EnsureBlobForBackgroundManager(subBlobImpl, aManager);
    MOZ_ASSERT(newSubBlobImpl);

    if (subBlobImpl != newSubBlobImpl) {
      newBlobImplNeeded = true;
    }
  }

  if (newBlobImplNeeded) {
    nsString contentType;
    blobImpl->GetType(contentType);

    if (blobImpl->IsFile()) {
      nsString name;
      blobImpl->GetName(name);

      blobImpl = new MultipartBlobImpl(newSubBlobImpls, name, contentType);
    } else {
      blobImpl = new MultipartBlobImpl(newSubBlobImpls, contentType);
    }

    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(blobImpl->SetMutable(false)));
  }

  return blobImpl.forget();
}

already_AddRefed<Blob>
ReadBlobOrFileNoWrap(JSContext* aCx,
                     JSStructuredCloneReader* aReader,
                     bool aIsMainThread)
{
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aReader);

  nsRefPtr<BlobImpl> blobImpl;
  {
    BlobImpl* rawBlobImpl;
    MOZ_ALWAYS_TRUE(JS_ReadBytes(aReader, &rawBlobImpl, sizeof(rawBlobImpl)));

    MOZ_ASSERT(rawBlobImpl);

    blobImpl = rawBlobImpl;
  }

  blobImpl = EnsureBlobForBackgroundManager(blobImpl);
  MOZ_ASSERT(blobImpl);

  nsCOMPtr<nsISupports> parent;
  if (aIsMainThread) {
    AssertIsOnMainThread();

    nsCOMPtr<nsIScriptGlobalObject> scriptGlobal =
      nsJSUtils::GetStaticScriptGlobal(JS::CurrentGlobalOrNull(aCx));
    parent = do_QueryInterface(scriptGlobal);
  } else {
    MOZ_ASSERT(!NS_IsMainThread());
    WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
    MOZ_ASSERT(workerPrivate);

    WorkerGlobalScope* globalScope = workerPrivate->GlobalScope();
    MOZ_ASSERT(globalScope);

    parent = do_QueryObject(globalScope);
  }

  nsRefPtr<Blob> blob = Blob::Create(parent, blobImpl);
  return blob.forget();
}

void
ReadBlobOrFile(JSContext* aCx,
               JSStructuredCloneReader* aReader,
               bool aIsMainThread,
               JS::MutableHandle<JSObject*> aBlobOrFile)
{
  nsRefPtr<Blob> blob = ReadBlobOrFileNoWrap(aCx, aReader, aIsMainThread);
  aBlobOrFile.set(blob->WrapObject(aCx, nullptr));
}

// See WriteFormData for serialization format.
void
ReadFormData(JSContext* aCx,
             JSStructuredCloneReader* aReader,
             bool aIsMainThread,
             uint32_t aCount,
             JS::MutableHandle<JSObject*> aFormData)
{
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aReader);
  MOZ_ASSERT(!aFormData);

  nsCOMPtr<nsISupports> parent;
  if (aIsMainThread) {
    AssertIsOnMainThread();
    nsCOMPtr<nsIScriptGlobalObject> scriptGlobal =
      nsJSUtils::GetStaticScriptGlobal(JS::CurrentGlobalOrNull(aCx));
    parent = do_QueryInterface(scriptGlobal);
  } else {
    WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
    MOZ_ASSERT(workerPrivate);
    workerPrivate->AssertIsOnWorkerThread();

    WorkerGlobalScope* globalScope = workerPrivate->GlobalScope();
    MOZ_ASSERT(globalScope);

    parent = do_QueryObject(globalScope);
  }

  nsRefPtr<nsFormData> formData = new nsFormData(parent);
  MOZ_ASSERT(formData);

  Optional<nsAString> thirdArg;

  uint32_t isFile;
  uint32_t dummy;
  for (uint32_t i = 0; i < aCount; ++i) {
    MOZ_ALWAYS_TRUE(JS_ReadUint32Pair(aReader, &isFile, &dummy));

    nsAutoString name;
    MOZ_ALWAYS_TRUE(ReadString(aReader, name));

    if (isFile) {
      // Read out the tag since the blob reader isn't expecting it.
      MOZ_ALWAYS_TRUE(JS_ReadUint32Pair(aReader, &dummy, &dummy));
      nsRefPtr<Blob> blob = ReadBlobOrFileNoWrap(aCx, aReader, aIsMainThread);
      MOZ_ASSERT(blob);
      formData->Append(name, *blob, thirdArg);
    } else {
      nsAutoString value;
      MOZ_ALWAYS_TRUE(ReadString(aReader, value));
      formData->Append(name, value);
    }
  }

  aFormData.set(formData->WrapObject(aCx, nullptr));
}

bool
WriteBlobOrFile(JSContext* aCx,
                JSStructuredCloneWriter* aWriter,
                BlobImpl* aBlobOrBlobImpl,
                WorkerStructuredCloneClosure& aClosure)
{
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aWriter);
  MOZ_ASSERT(aBlobOrBlobImpl);

  nsRefPtr<BlobImpl> blobImpl = EnsureBlobForBackgroundManager(aBlobOrBlobImpl);
  MOZ_ASSERT(blobImpl);

  aBlobOrBlobImpl = blobImpl;

  if (NS_WARN_IF(!JS_WriteUint32Pair(aWriter, DOMWORKER_SCTAG_BLOB, 0)) ||
      NS_WARN_IF(!JS_WriteBytes(aWriter,
                                &aBlobOrBlobImpl,
                                sizeof(aBlobOrBlobImpl)))) {
    return false;
  }

  aClosure.mClonedObjects.AppendElement(aBlobOrBlobImpl);
  return true;
}

// A FormData is serialized as:
//  - A pair of ints (tag identifying it as a FormData, number of elements in
//  the FormData)
//  - for each (key, value) pair:
//    - pair of ints (is value a file?, 0). If not a file, value is a string.
//    - string name
//    - if value is a file:
//      - write the file/blob
//    - else:
//      - string value
bool
WriteFormData(JSContext* aCx,
              JSStructuredCloneWriter* aWriter,
              nsFormData* aFormData,
              WorkerStructuredCloneClosure& aClosure)
{
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aWriter);
  MOZ_ASSERT(aFormData);

  if (NS_WARN_IF(!JS_WriteUint32Pair(aWriter, DOMWORKER_SCTAG_FORMDATA, aFormData->Length()))) {
    return false;
  }

  class MOZ_STACK_CLASS Closure {
    JSContext* mCx;
    JSStructuredCloneWriter* mWriter;
    WorkerStructuredCloneClosure& mClones;

  public:
    Closure(JSContext* aCx, JSStructuredCloneWriter* aWriter,
            WorkerStructuredCloneClosure& aClones)
      : mCx(aCx), mWriter(aWriter), mClones(aClones)
    { }

    static bool
    Write(const nsString& aName, bool isFile, const nsString& aValue,
          File* aFile, void* aClosure)
    {
      Closure* closure = static_cast<Closure*>(aClosure);
      if (!JS_WriteUint32Pair(closure->mWriter, /* a file? */ (uint32_t) isFile, 0)) {
        return false;
      }

      if (!WriteString(closure->mWriter, aName)) {
        return false;
      }

      if (isFile) {
        if (!WriteBlobOrFile(closure->mCx, closure->mWriter, aFile->Impl(), closure->mClones)) {
          return false;
        }
      } else {
        if (!WriteString(closure->mWriter, aValue)) {
          return false;
        }
      }

      return true;
    }
  };

  Closure closure(aCx, aWriter, aClosure);
  return aFormData->ForEach(Closure::Write, &closure);
}

struct WorkerStructuredCloneCallbacks
{
  static JSObject*
  Read(JSContext* aCx, JSStructuredCloneReader* aReader, uint32_t aTag,
       uint32_t aData, void* aClosure)
  {
    JS::Rooted<JSObject*> result(aCx);

    // See if object is a nsIDOMBlob pointer.
    if (aTag == DOMWORKER_SCTAG_BLOB) {
      MOZ_ASSERT(!aData);

      JS::Rooted<JSObject*> blobOrFile(aCx);
      ReadBlobOrFile(aCx, aReader, /* aIsMainThread */ false, &blobOrFile);

      return blobOrFile;
    }
    // See if the object is an ImageData.
    else if (aTag == SCTAG_DOM_IMAGEDATA) {
      MOZ_ASSERT(!aData);
      return ReadStructuredCloneImageData(aCx, aReader);
    }
    // See if the object is a FormData.
    else if (aTag == DOMWORKER_SCTAG_FORMDATA) {
      JS::Rooted<JSObject*> formData(aCx);
      // aData is the entry count.
      ReadFormData(aCx, aReader, /* aIsMainThread */ false,  aData, &formData);
      return formData;
    }

    // See if the object is an ImageBitmap.
    if (aTag == SCTAG_DOM_IMAGEBITMAP) {
      NS_ASSERTION(aClosure, "Null pointer!");

      // Get the current global object.
      auto* closure = static_cast<WorkerStructuredCloneClosure*>(aClosure);
      nsCOMPtr<nsIGlobalObject> parent = do_QueryInterface(closure->mParentWindow);
      // aData is the index of the cloned image.
      return ImageBitmap::ReadStructuredClone(aCx, aReader, parent,
                                              closure->mClonedImages, aData);
    }

    Error(aCx, 0);
    return nullptr;
  }

  static bool
  Write(JSContext* aCx, JSStructuredCloneWriter* aWriter,
        JS::Handle<JSObject*> aObj, void* aClosure)
  {
    NS_ASSERTION(aClosure, "Null pointer!");

    auto* closure = static_cast<WorkerStructuredCloneClosure*>(aClosure);

    // See if this is a Blob/File object.
    {
      nsRefPtr<Blob> blob;
      if (NS_SUCCEEDED(UNWRAP_OBJECT(Blob, aObj, blob))) {
        BlobImpl* blobImpl = blob->Impl();
        MOZ_ASSERT(blobImpl);

        if (WriteBlobOrFile(aCx, aWriter, blobImpl, *closure)) {
          return true;
        }
      }
    }

    // See if this is an ImageData object.
    {
      ImageData* imageData = nullptr;
      if (NS_SUCCEEDED(UNWRAP_OBJECT(ImageData, aObj, imageData))) {
        return WriteStructuredCloneImageData(aCx, aWriter, imageData);
      }
    }

    // See if this is a FormData object.
    {
      nsFormData* formData = nullptr;
      if (NS_SUCCEEDED(UNWRAP_OBJECT(FormData, aObj, formData))) {
        if (WriteFormData(aCx, aWriter, formData, *closure)) {
          return true;
        }
      }
    }

    // See if this is an ImageBitmap object.
    {
      ImageBitmap* imageBitmap = nullptr;
      if (NS_SUCCEEDED(UNWRAP_OBJECT(ImageBitmap, aObj, imageBitmap))) {
        return ImageBitmap::WriteStructuredClone(aWriter,
                                                 closure->mClonedImages,
                                                 imageBitmap);
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

  static bool
  ReadTransfer(JSContext* aCx, JSStructuredCloneReader* aReader,
               uint32_t aTag, void* aContent, uint64_t aExtraData,
               void* aClosure, JS::MutableHandle<JSObject*> aReturnObject)
  {
    MOZ_ASSERT(aClosure);

    auto* closure = static_cast<WorkerStructuredCloneClosure*>(aClosure);

    if (aTag == SCTAG_DOM_MAP_MESSAGEPORT) {
      MOZ_ASSERT(!aContent);
      MOZ_ASSERT(aExtraData < closure->mMessagePortIdentifiers.Length());

      ErrorResult rv;
      nsRefPtr<MessagePortBase> port =
        dom::MessagePort::Create(closure->mParentWindow,
                                 closure->mMessagePortIdentifiers[aExtraData],
                                 rv);

      if (NS_WARN_IF(rv.Failed())) {
        return false;
      }

      closure->mMessagePorts.AppendElement(port);

      JS::Rooted<JS::Value> value(aCx);
      if (!GetOrCreateDOMReflector(aCx, port, &value)) {
        JS_ClearPendingException(aCx);
        return false;
      }

      aReturnObject.set(&value.toObject());
      return true;
    }

    return false;
  }

  static bool
  Transfer(JSContext* aCx, JS::Handle<JSObject*> aObj, void* aClosure,
           uint32_t* aTag, JS::TransferableOwnership* aOwnership,
           void** aContent, uint64_t *aExtraData)
  {
    MOZ_ASSERT(aClosure);

    auto* closure = static_cast<WorkerStructuredCloneClosure*>(aClosure);

    MessagePortBase* port;
    nsresult rv = UNWRAP_OBJECT(MessagePort, aObj, port);
    if (NS_SUCCEEDED(rv)) {
      if (NS_WARN_IF(closure->mTransferredPorts.Contains(port))) {
        // No duplicates.
        return false;
      }

      MessagePortIdentifier identifier;
      if (!port->CloneAndDisentangle(identifier)) {
        return false;
      }

      closure->mMessagePortIdentifiers.AppendElement(identifier);
      closure->mTransferredPorts.AppendElement(port);

      *aTag = SCTAG_DOM_MAP_MESSAGEPORT;
      *aOwnership = JS::SCTAG_TMO_CUSTOM;
      *aContent = nullptr;
      *aExtraData = closure->mMessagePortIdentifiers.Length() - 1;

      return true;
    }

    return false;
  }

  static void
  FreeTransfer(uint32_t aTag, JS::TransferableOwnership aOwnership,
               void *aContent, uint64_t aExtraData, void* aClosure)
  {
    if (aTag == SCTAG_DOM_MAP_MESSAGEPORT) {
      MOZ_ASSERT(aClosure);
      MOZ_ASSERT(!aContent);
      auto* closure = static_cast<WorkerStructuredCloneClosure*>(aClosure);

      MOZ_ASSERT(aExtraData < closure->mMessagePortIdentifiers.Length());
      dom::MessagePort::ForceClose(closure->mMessagePortIdentifiers[aExtraData]);
    }
  }
};

const JSStructuredCloneCallbacks gWorkerStructuredCloneCallbacks = {
  WorkerStructuredCloneCallbacks::Read,
  WorkerStructuredCloneCallbacks::Write,
  WorkerStructuredCloneCallbacks::Error,
  WorkerStructuredCloneCallbacks::ReadTransfer,
  WorkerStructuredCloneCallbacks::Transfer,
  WorkerStructuredCloneCallbacks::FreeTransfer
};

struct MainThreadWorkerStructuredCloneCallbacks
{
  static JSObject*
  Read(JSContext* aCx, JSStructuredCloneReader* aReader, uint32_t aTag,
       uint32_t aData, void* aClosure)
  {
    AssertIsOnMainThread();

    // See if object is a Blob/File pointer.
    if (aTag == DOMWORKER_SCTAG_BLOB) {
      MOZ_ASSERT(!aData);

      JS::Rooted<JSObject*> blobOrFile(aCx);
      ReadBlobOrFile(aCx, aReader, /* aIsMainThread */ true, &blobOrFile);

      return blobOrFile;
    }
    // See if the object is a FormData.
    else if (aTag == DOMWORKER_SCTAG_FORMDATA) {
      JS::Rooted<JSObject*> formData(aCx);
      // aData is the entry count.
      ReadFormData(aCx, aReader, /* aIsMainThread */ true,  aData, &formData);
      return formData;
    }

    // See if the object is a ImageBitmap
    if (aTag == SCTAG_DOM_IMAGEBITMAP) {
      NS_ASSERTION(aClosure, "Null pointer!");

      // Get the current global object.
      auto* closure = static_cast<WorkerStructuredCloneClosure*>(aClosure);
      nsCOMPtr<nsIGlobalObject> parent = do_QueryInterface(closure->mParentWindow);
      // aData is the index of the cloned image.
      return ImageBitmap::ReadStructuredClone(aCx, aReader, parent,
                                              closure->mClonedImages, aData);
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

    auto* closure = static_cast<WorkerStructuredCloneClosure*>(aClosure);

    // See if this is a Blob/File object.
    {
      nsRefPtr<Blob> blob;
      if (NS_SUCCEEDED(UNWRAP_OBJECT(Blob, aObj, blob))) {
        BlobImpl* blobImpl = blob->Impl();
        MOZ_ASSERT(blobImpl);

        if (!blobImpl->MayBeClonedToOtherThreads()) {
          NS_WARNING("Not all the blob implementations can be sent between threads.");
        } else if (WriteBlobOrFile(aCx, aWriter, blobImpl, *closure)) {
          return true;
        }
      }
    }

    // Handle imageBitmap cloning
    {
      ImageBitmap* imageBitmap = nullptr;
      if (NS_SUCCEEDED(UNWRAP_OBJECT(ImageBitmap, aObj, imageBitmap))) {
        return ImageBitmap::WriteStructuredClone(aWriter,
                                                 closure->mClonedImages,
                                                 imageBitmap);
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

const JSStructuredCloneCallbacks gMainThreadWorkerStructuredCloneCallbacks = {
  MainThreadWorkerStructuredCloneCallbacks::Read,
  MainThreadWorkerStructuredCloneCallbacks::Write,
  MainThreadWorkerStructuredCloneCallbacks::Error,
  WorkerStructuredCloneCallbacks::ReadTransfer,
  WorkerStructuredCloneCallbacks::Transfer,
  WorkerStructuredCloneCallbacks::FreeTransfer
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

const JSStructuredCloneCallbacks gChromeWorkerStructuredCloneCallbacks = {
  ChromeWorkerStructuredCloneCallbacks::Read,
  ChromeWorkerStructuredCloneCallbacks::Write,
  ChromeWorkerStructuredCloneCallbacks::Error,
  WorkerStructuredCloneCallbacks::ReadTransfer,
  WorkerStructuredCloneCallbacks::Transfer,
  WorkerStructuredCloneCallbacks::FreeTransfer
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

const JSStructuredCloneCallbacks gMainThreadChromeWorkerStructuredCloneCallbacks = {
  MainThreadChromeWorkerStructuredCloneCallbacks::Read,
  MainThreadChromeWorkerStructuredCloneCallbacks::Write,
  MainThreadChromeWorkerStructuredCloneCallbacks::Error,
  nullptr,
  nullptr,
  nullptr
};

class MainThreadReleaseRunnable final : public nsRunnable
{
  nsTArray<nsCOMPtr<nsISupports>> mDoomed;
  nsCOMPtr<nsILoadGroup> mLoadGroupToCancel;

public:
  MainThreadReleaseRunnable(nsTArray<nsCOMPtr<nsISupports>>& aDoomed,
                            nsCOMPtr<nsILoadGroup>& aLoadGroupToCancel)
  {
    mDoomed.SwapElements(aDoomed);
    mLoadGroupToCancel.swap(aLoadGroupToCancel);
  }

  NS_DECL_ISUPPORTS_INHERITED

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

class WorkerFinishedRunnable final : public WorkerControlRunnable
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
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    // Silence bad assertions.
    return true;
  }

  virtual void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult) override
  {
    // Silence bad assertions.
  }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    nsCOMPtr<nsILoadGroup> loadGroupToCancel;
    mFinishedWorker->ForgetOverridenLoadGroup(loadGroupToCancel);

    nsTArray<nsCOMPtr<nsISupports>> doomed;
    mFinishedWorker->ForgetMainThreadObjects(doomed);

    nsRefPtr<MainThreadReleaseRunnable> runnable =
      new MainThreadReleaseRunnable(doomed, loadGroupToCancel);
    if (NS_FAILED(NS_DispatchToMainThread(runnable))) {
      NS_WARNING("Failed to dispatch, going to leak!");
    }

    RuntimeService* runtime = RuntimeService::GetService();
    NS_ASSERTION(runtime, "This should never be null!");

    mFinishedWorker->DisableDebugger();

    runtime->UnregisterWorker(aCx, mFinishedWorker);

    mFinishedWorker->ClearSelfRef();
    return true;
  }
};

class TopLevelWorkerFinishedRunnable final : public nsRunnable
{
  WorkerPrivate* mFinishedWorker;

public:
  explicit TopLevelWorkerFinishedRunnable(WorkerPrivate* aFinishedWorker)
  : mFinishedWorker(aFinishedWorker)
  {
    aFinishedWorker->AssertIsOnWorkerThread();
  }

  NS_DECL_ISUPPORTS_INHERITED

private:
  ~TopLevelWorkerFinishedRunnable() {}

  NS_IMETHOD
  Run() override
  {
    AssertIsOnMainThread();

    RuntimeService* runtime = RuntimeService::GetService();
    MOZ_ASSERT(runtime);

    AutoSafeJSContext cx;
    JSAutoRequest ar(cx);

    mFinishedWorker->DisableDebugger();

    runtime->UnregisterWorker(cx, mFinishedWorker);

    nsCOMPtr<nsILoadGroup> loadGroupToCancel;
    mFinishedWorker->ForgetOverridenLoadGroup(loadGroupToCancel);

    nsTArray<nsCOMPtr<nsISupports> > doomed;
    mFinishedWorker->ForgetMainThreadObjects(doomed);

    nsRefPtr<MainThreadReleaseRunnable> runnable =
      new MainThreadReleaseRunnable(doomed, loadGroupToCancel);
    if (NS_FAILED(NS_DispatchToCurrentThread(runnable))) {
      NS_WARNING("Failed to dispatch, going to leak!");
    }

    mFinishedWorker->ClearSelfRef();
    return NS_OK;
  }
};

class ModifyBusyCountRunnable final : public WorkerControlRunnable
{
  bool mIncrease;

public:
  ModifyBusyCountRunnable(WorkerPrivate* aWorkerPrivate, bool aIncrease)
  : WorkerControlRunnable(aWorkerPrivate, ParentThreadUnchangedBusyCount),
    mIncrease(aIncrease)
  { }

private:
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    return aWorkerPrivate->ModifyBusyCount(aCx, mIncrease);
  }

  virtual void
  PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate, bool aRunResult)
          override
  {
    if (mIncrease) {
      WorkerControlRunnable::PostRun(aCx, aWorkerPrivate, aRunResult);
      return;
    }
    // Don't do anything here as it's possible that aWorkerPrivate has been
    // deleted.
  }
};

class CompileScriptRunnable final : public WorkerRunnable
{
  nsString mScriptURL;

public:
  explicit CompileScriptRunnable(WorkerPrivate* aWorkerPrivate,
                                 const nsAString& aScriptURL)
  : WorkerRunnable(aWorkerPrivate, WorkerThreadModifyBusyCount),
    mScriptURL(aScriptURL)
  { }

private:
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    if (!scriptloader::LoadMainScript(aCx, mScriptURL, WorkerScript)) {
      return false;
    }

    aWorkerPrivate->SetWorkerScriptExecutedSuccessfully();
    return true;
  }
};

class CompileDebuggerScriptRunnable final : public WorkerDebuggerRunnable
{
  nsString mScriptURL;

public:
  CompileDebuggerScriptRunnable(WorkerPrivate* aWorkerPrivate,
                                const nsAString& aScriptURL)
  : WorkerDebuggerRunnable(aWorkerPrivate),
    mScriptURL(aScriptURL)
  { }

private:
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    WorkerDebuggerGlobalScope* globalScope =
      aWorkerPrivate->CreateDebuggerGlobalScope(aCx);
    if (!globalScope) {
      NS_WARNING("Failed to make global!");
      return false;
    }

    JS::Rooted<JSObject*> global(aCx, globalScope->GetWrapper());

    JSAutoCompartment ac(aCx, global);
    return scriptloader::LoadMainScript(aCx, mScriptURL, DebuggerScript);
  }
};

class CloseEventRunnable final : public WorkerRunnable
{
public:
  explicit CloseEventRunnable(WorkerPrivate* aWorkerPrivate)
  : WorkerRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
  { }

private:
  virtual bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_CRASH("Don't call Dispatch() on CloseEventRunnable!");
  }

  virtual void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult) override
  {
    MOZ_CRASH("Don't call Dispatch() on CloseEventRunnable!");
  }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
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

  NS_IMETHOD Cancel() override
  {
    // We need to run regardless.
    Run();
    return WorkerRunnable::Cancel();
  }

  virtual void
  PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate, bool aRunResult)
          override
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

class MessageEventRunnable final : public WorkerRunnable
{
  JSAutoStructuredCloneBuffer mBuffer;
  WorkerStructuredCloneClosure mClosure;
  uint64_t mMessagePortSerial;
  bool mToMessagePort;

  // This is only used for messages dispatched to a service worker.
  nsAutoPtr<ServiceWorkerClientInfo> mEventSource;

public:
  MessageEventRunnable(WorkerPrivate* aWorkerPrivate,
                       TargetAndBusyBehavior aBehavior,
                       bool aToMessagePort, uint64_t aMessagePortSerial)
  : WorkerRunnable(aWorkerPrivate, aBehavior)
  , mMessagePortSerial(aMessagePortSerial)
  , mToMessagePort(aToMessagePort)
  {
  }

  bool
  Write(JSContext* aCx, JS::Handle<JS::Value> aValue,
        JS::Handle<JS::Value> aTransferredValue,
        const JSStructuredCloneCallbacks *aCallbacks)
   {
    bool ok = mBuffer.write(aCx, aValue, aTransferredValue, aCallbacks,
                            &mClosure);
    // This hashtable has to be empty because it could contain MessagePort
    // objects that cannot be freed on a different thread.
    mClosure.mTransferredPorts.Clear();
    return ok;
  }

  void
  SetMessageSource(ServiceWorkerClientInfo* aSource)
  {
    mEventSource = aSource;
  }

  bool
  DispatchDOMEvent(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                   DOMEventTargetHelper* aTarget, bool aIsMainThread)
  {
    // Release reference to objects that were AddRef'd for
    // cloning into worker when array goes out of scope.
    WorkerStructuredCloneClosure closure;
    closure.mClonedObjects.SwapElements(mClosure.mClonedObjects);
    closure.mClonedImages.SwapElements(mClosure.mClonedImages);
    MOZ_ASSERT(mClosure.mMessagePorts.IsEmpty());
    closure.mMessagePortIdentifiers.SwapElements(mClosure.mMessagePortIdentifiers);

    if (aIsMainThread) {
      closure.mParentWindow = do_QueryInterface(aTarget->GetParentObject());
    }

    JS::Rooted<JS::Value> messageData(aCx);
    if (!mBuffer.read(aCx, &messageData,
                      workers::WorkerStructuredCloneCallbacks(aIsMainThread),
                      &closure)) {
      xpc::Throw(aCx, NS_ERROR_DOM_DATA_CLONE_ERR);
      return false;
    }

    nsRefPtr<MessageEvent> event = new MessageEvent(aTarget, nullptr, nullptr);
    nsresult rv =
      event->InitMessageEvent(NS_LITERAL_STRING("message"),
                              false /* non-bubbling */,
                              true /* cancelable */,
                              messageData,
                              EmptyString(),
                              EmptyString(),
                              nullptr);
    if (mEventSource) {
      nsRefPtr<ServiceWorkerClient> client =
        new ServiceWorkerWindowClient(aTarget, *mEventSource);
      event->SetSource(client);
    }

    if (NS_FAILED(rv)) {
      xpc::Throw(aCx, rv);
      return false;
    }

    event->SetTrusted(true);
    event->SetPorts(new MessagePortList(static_cast<dom::Event*>(event.get()),
                                        closure.mMessagePorts));
    nsCOMPtr<nsIDOMEvent> domEvent = do_QueryObject(event);

    nsEventStatus dummy = nsEventStatus_eIgnore;
    aTarget->DispatchDOMEvent(nullptr, domEvent, nullptr, &dummy);
    return true;
  }

private:
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
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
                                                            Move(mBuffer),
                                                            mClosure);
      }

      if (aWorkerPrivate->IsFrozen()) {
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

class DebuggerMessageEventRunnable : public WorkerDebuggerRunnable {
  nsString mMessage;

public:
  DebuggerMessageEventRunnable(WorkerPrivate* aWorkerPrivate,
                               const nsAString& aMessage)
  : WorkerDebuggerRunnable(aWorkerPrivate),
    mMessage(aMessage)
  {
  }

private:
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    WorkerDebuggerGlobalScope* globalScope = aWorkerPrivate->DebuggerGlobalScope();
    MOZ_ASSERT(globalScope);

    JS::Rooted<JSString*> message(aCx, JS_NewUCStringCopyN(aCx, mMessage.get(),
                                                           mMessage.Length()));
    if (!message) {
      return false;
    }
    JS::Rooted<JS::Value> data(aCx, JS::StringValue(message));

    nsRefPtr<MessageEvent> event = new MessageEvent(globalScope, nullptr,
                                                    nullptr);
    nsresult rv =
      event->InitMessageEvent(NS_LITERAL_STRING("message"),
                              false, // canBubble
                              true, // cancelable
                              data,
                              EmptyString(),
                              EmptyString(),
                              nullptr);
    if (NS_FAILED(rv)) {
      xpc::Throw(aCx, rv);
      return false;
    }
    event->SetTrusted(true);

    nsCOMPtr<nsIDOMEvent> domEvent = do_QueryObject(event);
    nsEventStatus status = nsEventStatus_eIgnore;
    globalScope->DispatchDOMEvent(nullptr, domEvent, nullptr, &status);
    return true;
  }
};

class NotifyRunnable final : public WorkerControlRunnable
{
  Status mStatus;

public:
  NotifyRunnable(WorkerPrivate* aWorkerPrivate, Status aStatus)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
    mStatus(aStatus)
  {
    MOZ_ASSERT(aStatus == Closing || aStatus == Terminating ||
               aStatus == Canceling || aStatus == Killing);
  }

private:
  virtual bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    // Modify here, but not in PostRun! This busy count addition will be matched
    // by the CloseEventRunnable.
    return aWorkerPrivate->ModifyBusyCount(aCx, true);
  }

  virtual void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult) override
  {
    if (!aDispatchResult) {
      // We couldn't dispatch to the worker, which means it's already dead.
      // Undo the busy count modification.
      aWorkerPrivate->ModifyBusyCount(aCx, false);
    }
  }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    return aWorkerPrivate->NotifyInternal(aCx, mStatus);
  }
};

class CloseRunnable final : public WorkerControlRunnable
{
public:
  explicit CloseRunnable(WorkerPrivate* aWorkerPrivate)
  : WorkerControlRunnable(aWorkerPrivate, ParentThreadUnchangedBusyCount)
  { }

private:
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    // This busy count will be matched by the CloseEventRunnable.
    return aWorkerPrivate->ModifyBusyCount(aCx, true) &&
           aWorkerPrivate->Close();
  }
};

class FreezeRunnable final : public WorkerControlRunnable
{
public:
  explicit FreezeRunnable(WorkerPrivate* aWorkerPrivate)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
  { }

private:
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    return aWorkerPrivate->FreezeInternal(aCx);
  }
};

class ThawRunnable final : public WorkerControlRunnable
{
public:
  explicit ThawRunnable(WorkerPrivate* aWorkerPrivate)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
  { }

private:
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    return aWorkerPrivate->ThawInternal(aCx);
  }
};

class ReportErrorRunnable final : public WorkerRunnable
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
      RootedDictionary<ErrorEventInit> init(aCx);
      init.mMessage = aMessage;
      init.mFilename = aFilename;
      init.mLineno = aLineNumber;
      init.mCancelable = true;
      init.mBubbles = false;

      if (aTarget) {
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
        JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));
        NS_ASSERTION(global, "This should never be null!");

        nsEventStatus status = nsEventStatus_eIgnore;
        nsIScriptGlobalObject* sgo;

        if (aWorkerPrivate) {
          WorkerGlobalScope* globalScope = nullptr;
          UNWRAP_WORKER_OBJECT(WorkerGlobalScope, global, globalScope);

          if (!globalScope) {
            WorkerDebuggerGlobalScope* globalScope = nullptr;
            UNWRAP_OBJECT(WorkerDebuggerGlobalScope, global, globalScope);

            MOZ_ASSERT_IF(globalScope, globalScope->GetWrapperPreserveColor() == global);
            MOZ_ASSERT_IF(!globalScope, IsDebuggerSandbox(global));

            aWorkerPrivate->ReportErrorToDebugger(aFilename, aLineNumber,
                                                  aMessage);
            return true;
          }

          MOZ_ASSERT(globalScope->GetWrapperPreserveColor() == global);
          nsIDOMEventTarget* target = static_cast<nsIDOMEventTarget*>(globalScope);

          nsRefPtr<ErrorEvent> event =
            ErrorEvent::Constructor(aTarget, NS_LITERAL_STRING("error"), init);
          event->SetTrusted(true);

          if (NS_FAILED(EventDispatcher::DispatchDOMEvent(target, nullptr,
                                                          event, nullptr,
                                                          &status))) {
            NS_WARNING("Failed to dispatch worker thread error event!");
            status = nsEventStatus_eIgnore;
          }
        }
        else if ((sgo = nsJSUtils::GetStaticScriptGlobal(global))) {
          MOZ_ASSERT(NS_IsMainThread());

          if (NS_FAILED(sgo->HandleScriptError(init, &status))) {
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
               bool aDispatchResult) override
  {
    aWorkerPrivate->AssertIsOnWorkerThread();

    // Dispatch may fail if the worker was canceled, no need to report that as
    // an error, so don't call base class PostDispatch.
  }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    JS::Rooted<JSObject*> target(aCx, aWorkerPrivate->GetWrapper());

    uint64_t innerWindowId;
    bool fireAtScope = true;

    bool workerIsAcceptingEvents = aWorkerPrivate->IsAcceptingEvents();

    WorkerPrivate* parent = aWorkerPrivate->GetParent();
    if (parent) {
      innerWindowId = 0;
    }
    else {
      AssertIsOnMainThread();

      if (aWorkerPrivate->IsFrozen()) {
        aWorkerPrivate->QueueRunnable(this);
        return true;
      }

      if (aWorkerPrivate->IsServiceWorker() || aWorkerPrivate->IsSharedWorker()) {
        if (aWorkerPrivate->IsServiceWorker() && !JSREPORT_IS_WARNING(mFlags)) {
          nsRefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
          MOZ_ASSERT(swm);
          bool handled = swm->HandleError(aCx, aWorkerPrivate->GetPrincipal(),
                                          aWorkerPrivate->SharedWorkerName(),
                                          aWorkerPrivate->ScriptURL(),
                                          mMessage,
                                          mFilename, mLine, mLineNumber,
                                          mColumnNumber, mFlags);
          if (handled) {
            return true;
          }
        }

        aWorkerPrivate->BroadcastErrorToSharedWorkers(aCx, mMessage, mFilename,
                                                      mLine, mLineNumber,
                                                      mColumnNumber, mFlags);
        return true;
      }

      // The innerWindowId is only required if we are going to ReportError
      // below, which is gated on this condition. The inner window correctness
      // check is only going to succeed when the worker is accepting events.
      if (workerIsAcceptingEvents) {
        aWorkerPrivate->AssertInnerWindowIsCorrect();
        innerWindowId = aWorkerPrivate->WindowID();
      }
    }

    // Don't fire this event if the JS object has been disconnected from the
    // private object.
    if (!workerIsAcceptingEvents) {
      return true;
    }

    return ReportError(aCx, parent, fireAtScope, aWorkerPrivate, mMessage,
                       mFilename, mLine, mLineNumber, mColumnNumber, mFlags,
                       mErrorNumber, innerWindowId);
  }
};

class TimerRunnable final : public WorkerRunnable
{
public:
  explicit TimerRunnable(WorkerPrivate* aWorkerPrivate)
  : WorkerRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
  { }

private:
  virtual bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    // Silence bad assertions.
    return true;
  }

  virtual void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult) override
  {
    // Silence bad assertions.
  }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    return aWorkerPrivate->RunExpiredTimeouts(aCx);
  }
};

class DebuggerImmediateRunnable : public WorkerRunnable
{
  nsRefPtr<Function> mHandler;

public:
  explicit DebuggerImmediateRunnable(WorkerPrivate* aWorkerPrivate,
                                     Function& aHandler)
  : WorkerRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
    mHandler(&aHandler)
  { }

private:
  virtual bool
  IsDebuggerRunnable() const override
  {
    return true;
  }

  virtual bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    // Silence bad assertions.
    return true;
  }

  virtual void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult) override
  {
    // Silence bad assertions.
  }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));
    JS::Rooted<JS::Value> callable(aCx, JS::ObjectValue(*mHandler->Callable()));
    JS::HandleValueArray args = JS::HandleValueArray::empty();
    JS::Rooted<JS::Value> rval(aCx);
    if (!JS_CallFunctionValue(aCx, global, callable, args, &rval) &&
        !JS_ReportPendingException(aCx)) {
      return false;
    }

    return true;
  }
};

void
DummyCallback(nsITimer* aTimer, void* aClosure)
{
  // Nothing!
}

class KillCloseEventRunnable final : public WorkerRunnable
{
  nsCOMPtr<nsITimer> mTimer;

  class KillScriptRunnable final : public WorkerControlRunnable
  {
  public:
    explicit KillScriptRunnable(WorkerPrivate* aWorkerPrivate)
    : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
    { }

  private:
    virtual bool
    PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
    {
      // Silence bad assertions, this is dispatched from the timer thread.
      return true;
    }

    virtual void
    PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                 bool aDispatchResult) override
    {
      // Silence bad assertions, this is dispatched from the timer thread.
    }

    virtual bool
    WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
    {
      // Kill running script.
      return false;
    }
  };

public:
  explicit KillCloseEventRunnable(WorkerPrivate* aWorkerPrivate)
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
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_CRASH("Don't call Dispatch() on KillCloseEventRunnable!");
  }

  virtual void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult) override
  {
    MOZ_CRASH("Don't call Dispatch() on KillCloseEventRunnable!");
  }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    if (mTimer) {
      mTimer->Cancel();
      mTimer = nullptr;
    }

    return true;
  }

  NS_IMETHOD Cancel() override
  {
    // We need to run regardless.
    Run();
    return WorkerRunnable::Cancel();
  }
};

class UpdateRuntimeOptionsRunnable final : public WorkerControlRunnable
{
  JS::RuntimeOptions mRuntimeOptions;

public:
  UpdateRuntimeOptionsRunnable(
                                    WorkerPrivate* aWorkerPrivate,
                                    const JS::RuntimeOptions& aRuntimeOptions)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
    mRuntimeOptions(aRuntimeOptions)
  { }

private:
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    aWorkerPrivate->UpdateRuntimeOptionsInternal(aCx, mRuntimeOptions);
    return true;
  }
};

class UpdatePreferenceRunnable final : public WorkerControlRunnable
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
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    aWorkerPrivate->UpdatePreferenceInternal(aCx, mPref, mValue);
    return true;
  }
};

class UpdateLanguagesRunnable final : public WorkerRunnable
{
  nsTArray<nsString> mLanguages;

public:
  UpdateLanguagesRunnable(WorkerPrivate* aWorkerPrivate,
                          const nsTArray<nsString>& aLanguages)
    : WorkerRunnable(aWorkerPrivate, WorkerThreadModifyBusyCount),
      mLanguages(aLanguages)
  { }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    aWorkerPrivate->UpdateLanguagesInternal(aCx, mLanguages);
    return true;
  }
};

class UpdateJSWorkerMemoryParameterRunnable final :
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
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    aWorkerPrivate->UpdateJSWorkerMemoryParameterInternal(aCx, mKey, mValue);
    return true;
  }
};

#ifdef JS_GC_ZEAL
class UpdateGCZealRunnable final : public WorkerControlRunnable
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
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    aWorkerPrivate->UpdateGCZealInternal(aCx, mGCZeal, mFrequency);
    return true;
  }
};
#endif

class GarbageCollectRunnable final : public WorkerControlRunnable
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
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    // Silence bad assertions, this can be dispatched from either the main
    // thread or the timer thread..
    return true;
  }

  virtual void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                bool aDispatchResult) override
  {
    // Silence bad assertions, this can be dispatched from either the main
    // thread or the timer thread..
  }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
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
    : WorkerRunnable(aWorkerPrivate, WorkerThreadModifyBusyCount),
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

#ifdef DEBUG
static bool
StartsWithExplicit(nsACString& s)
{
    return StringBeginsWith(s, NS_LITERAL_CSTRING("explicit/"));
}
#endif

class WorkerJSRuntimeStats : public JS::RuntimeStats
{
  const nsACString& mRtPath;

public:
  explicit WorkerJSRuntimeStats(const nsACString& aRtPath)
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
                     override
  {
    MOZ_ASSERT(!aZoneStats->extra);

    // ReportJSRuntimeExplicitTreeStats expects that
    // aZoneStats->extra is a xpc::ZoneStatsExtras pointer.
    xpc::ZoneStatsExtras* extras = new xpc::ZoneStatsExtras;
    extras->pathPrefix = mRtPath;
    extras->pathPrefix += nsPrintfCString("zone(0x%p)/", (void *)aZone);

    MOZ_ASSERT(StartsWithExplicit(extras->pathPrefix));

    aZoneStats->extra = extras;
  }

  virtual void
  initExtraCompartmentStats(JSCompartment* aCompartment,
                            JS::CompartmentStats* aCompartmentStats)
                            override
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

    MOZ_ASSERT(StartsWithExplicit(extras->jsPathPrefix));
    MOZ_ASSERT(StartsWithExplicit(extras->domPathPrefix));

    extras->location = nullptr;

    aCompartmentStats->extra = extras;
  }
};

class MessagePortRunnable final : public WorkerRunnable
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
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    if (mConnect) {
      return aWorkerPrivate->ConnectMessagePort(aCx, mMessagePortSerial);
    }

    aWorkerPrivate->DisconnectMessagePort(mMessagePortSerial);
    return true;
  }
};

class DummyRunnable final
  : public WorkerRunnable
{
public:
  explicit
  DummyRunnable(WorkerPrivate* aWorkerPrivate)
    : WorkerRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
  {
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

private:
  ~DummyRunnable()
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  virtual bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_ASSERT_UNREACHABLE("Should never call Dispatch on this!");
    return true;
  }

  virtual void
  PostDispatch(JSContext* aCx,
               WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult) override
  {
    MOZ_ASSERT_UNREACHABLE("Should never call Dispatch on this!");
  }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    // Do nothing.
    return true;
  }
};

PRThread*
PRThreadFromThread(nsIThread* aThread)
{
  MOZ_ASSERT(aThread);

  PRThread* result;
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(aThread->GetPRThread(&result)));
  MOZ_ASSERT(result);

  return result;
}

} /* anonymous namespace */

NS_IMPL_ISUPPORTS_INHERITED0(MainThreadReleaseRunnable, nsRunnable)

NS_IMPL_ISUPPORTS_INHERITED0(TopLevelWorkerFinishedRunnable, nsRunnable)

TimerThreadEventTarget::TimerThreadEventTarget(WorkerPrivate* aWorkerPrivate,
                                               WorkerRunnable* aWorkerRunnable)
  : mWorkerPrivate(aWorkerPrivate), mWorkerRunnable(aWorkerRunnable)
{
  MOZ_ASSERT(aWorkerPrivate);
  MOZ_ASSERT(aWorkerRunnable);
}

TimerThreadEventTarget::~TimerThreadEventTarget()
{
}

NS_IMETHODIMP
TimerThreadEventTarget::DispatchFromScript(nsIRunnable* aRunnable, uint32_t aFlags)
{
  nsCOMPtr<nsIRunnable> runnable(aRunnable);
  return Dispatch(runnable.forget(), aFlags);
}

NS_IMETHODIMP
TimerThreadEventTarget::Dispatch(already_AddRefed<nsIRunnable>&& aRunnable, uint32_t aFlags)
{
  // This should only happen on the timer thread.
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aFlags == nsIEventTarget::DISPATCH_NORMAL);

  nsRefPtr<TimerThreadEventTarget> kungFuDeathGrip = this;

  // Run the runnable we're given now (should just call DummyCallback()),
  // otherwise the timer thread will leak it...  If we run this after
  // dispatch running the event can race against resetting the timer.
  nsCOMPtr<nsIRunnable> runnable(aRunnable);
  runnable->Run();

  // This can fail if we're racing to terminate or cancel, should be handled
  // by the terminate or cancel code.
  mWorkerRunnable->Dispatch(nullptr);

  return NS_OK;
}

NS_IMETHODIMP
TimerThreadEventTarget::IsOnCurrentThread(bool* aIsOnCurrentThread)
{
  MOZ_ASSERT(aIsOnCurrentThread);

  nsresult rv = mWorkerPrivate->IsOnCurrentThread(aIsOnCurrentThread);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS(TimerThreadEventTarget, nsIEventTarget)

WorkerLoadInfo::WorkerLoadInfo()
  : mWindowID(UINT64_MAX)
  , mServiceWorkerID(0)
  , mFromWindow(false)
  , mEvalAllowed(false)
  , mReportCSPViolations(false)
  , mXHRParamsAllowed(false)
  , mPrincipalIsSystem(false)
  , mIsInPrivilegedApp(false)
  , mIsInCertifiedApp(false)
  , mIndexedDBAllowed(false)
  , mPrivateBrowsing(true)
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

  MOZ_ASSERT(!mLoadFailedAsyncRunnable);
  aOther.mLoadFailedAsyncRunnable.swap(mLoadFailedAsyncRunnable);

  MOZ_ASSERT(!mInterfaceRequestor);
  aOther.mInterfaceRequestor.swap(mInterfaceRequestor);

  MOZ_ASSERT(!mPrincipalInfo);
  mPrincipalInfo = aOther.mPrincipalInfo.forget();

  mDomain = aOther.mDomain;
  mServiceWorkerCacheName = aOther.mServiceWorkerCacheName;
  mWindowID = aOther.mWindowID;
  mServiceWorkerID = aOther.mServiceWorkerID;
  mFromWindow = aOther.mFromWindow;
  mEvalAllowed = aOther.mEvalAllowed;
  mReportCSPViolations = aOther.mReportCSPViolations;
  mXHRParamsAllowed = aOther.mXHRParamsAllowed;
  mPrincipalIsSystem = aOther.mPrincipalIsSystem;
  mIsInPrivilegedApp = aOther.mIsInPrivilegedApp;
  mIsInCertifiedApp = aOther.mIsInCertifiedApp;
  mIndexedDBAllowed = aOther.mIndexedDBAllowed;
  mPrivateBrowsing = aOther.mPrivateBrowsing;
  mServiceWorkersTestingInWindow = aOther.mServiceWorkersTestingInWindow;
}

template <class Derived>
class WorkerPrivateParent<Derived>::EventTarget final
  : public nsIEventTarget
{
  // This mutex protects mWorkerPrivate and must be acquired *before* the
  // WorkerPrivate's mutex whenever they must both be held.
  mozilla::Mutex mMutex;
  WorkerPrivate* mWorkerPrivate;
  nsIEventTarget* mWeakNestedEventTarget;
  nsCOMPtr<nsIEventTarget> mNestedEventTarget;

public:
  explicit EventTarget(WorkerPrivate* aWorkerPrivate)
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
    mTabChildList.RemoveElementAt(mTabChildList.Length() - 1);
  }

  return nullptr;
}

NS_IMPL_ADDREF(WorkerLoadInfo::InterfaceRequestor)
NS_IMPL_RELEASE(WorkerLoadInfo::InterfaceRequestor)
NS_IMPL_QUERY_INTERFACE(WorkerLoadInfo::InterfaceRequestor, nsIInterfaceRequestor)

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

class WorkerPrivate::MemoryReporter final : public nsIMemoryReporter
{
  NS_DECL_THREADSAFE_ISUPPORTS

  friend class WorkerPrivate;

  SharedMutex mMutex;
  WorkerPrivate* mWorkerPrivate;
  bool mAlreadyMappedToAddon;

public:
  explicit MemoryReporter(WorkerPrivate* aWorkerPrivate)
  : mMutex(aWorkerPrivate->mMutex), mWorkerPrivate(aWorkerPrivate),
    mAlreadyMappedToAddon(false)
  {
    aWorkerPrivate->AssertIsOnWorkerThread();

  }

  NS_IMETHOD
  CollectReports(nsIMemoryReporterCallback* aCallback,
                 nsISupports* aClosure, bool aAnonymize) override
  {
    AssertIsOnMainThread();

    // Assumes that WorkerJSRuntimeStats will hold a reference to |path|, and
    // not a copy, as TryToMapAddon() may later modify it.
    nsCString path;
    WorkerJSRuntimeStats rtStats(path);

    {
      MutexAutoLock lock(mMutex);

      if (!mWorkerPrivate) {
        // Returning NS_OK here will effectively report 0 memory.
        return NS_OK;
      }

      path.AppendLiteral("explicit/workers/workers(");
      if (aAnonymize && !mWorkerPrivate->Domain().IsEmpty()) {
        path.AppendLiteral("<anonymized-domain>)/worker(<anonymized-url>");
      } else {
        nsCString escapedDomain(mWorkerPrivate->Domain());
        if (escapedDomain.IsEmpty()) {
          escapedDomain += "chrome";
        } else {
          escapedDomain.ReplaceChar('/', '\\');
        }
        path.Append(escapedDomain);
        path.AppendLiteral(")/worker(");
        NS_ConvertUTF16toUTF8 escapedURL(mWorkerPrivate->ScriptURL());
        escapedURL.ReplaceChar('/', '\\');
        path.Append(escapedURL);
      }
      path.AppendPrintf(", 0x%p)/", static_cast<void*>(mWorkerPrivate));

      TryToMapAddon(path);

      if (!mWorkerPrivate->BlockAndCollectRuntimeStats(&rtStats, aAnonymize)) {
        // Returning NS_OK here will effectively report 0 memory.
        return NS_OK;
      }
    }

    return xpc::ReportJSRuntimeExplicitTreeStats(rtStats, path,
                                                 aCallback, aClosure,
                                                 aAnonymize);
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
  TryToMapAddon(nsACString &path)
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

    if (!XRE_IsParentProcess()) {
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
    path.Insert(addonId, explicitLength);
  }
};

NS_IMPL_ISUPPORTS(WorkerPrivate::MemoryReporter, nsIMemoryReporter)

WorkerPrivate::SyncLoopInfo::SyncLoopInfo(EventTarget* aEventTarget)
: mEventTarget(aEventTarget), mCompleted(false), mResult(false)
#ifdef DEBUG
  , mHasRun(false)
#endif
{
}

struct WorkerPrivate::PreemptingRunnableInfo final
{
  nsCOMPtr<nsIRunnable> mRunnable;
  uint32_t mRecursionDepth;

  PreemptingRunnableInfo()
  {
    MOZ_COUNT_CTOR(WorkerPrivate::PreemptingRunnableInfo);
  }

  ~PreemptingRunnableInfo()
  {
    MOZ_COUNT_DTOR(WorkerPrivate::PreemptingRunnableInfo);
  }
};

template <class Derived>
nsIDocument*
WorkerPrivateParent<Derived>::GetDocument() const
{
  AssertIsOnMainThread();
  if (mLoadInfo.mWindow) {
    return mLoadInfo.mWindow->GetExtantDoc();
  }
  // if we don't have a document, we should query the document
  // from the parent in case of a nested worker
  WorkerPrivate* parent = mParent;
  while (parent) {
    if (parent->mLoadInfo.mWindow) {
      return parent->mLoadInfo.mWindow->GetExtantDoc();
    }
    parent = parent->GetParent();
  }
  // couldn't query a document, give up and return nullptr
  return nullptr;
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
                                           const nsACString& aSharedWorkerName,
                                           WorkerLoadInfo& aLoadInfo)
: mMutex("WorkerPrivateParent Mutex"),
  mCondVar(mMutex, "WorkerPrivateParent CondVar"),
  mMemoryReportCondVar(mMutex, "WorkerPrivateParent Memory Report CondVar"),
  mParent(aParent), mScriptURL(aScriptURL),
  mSharedWorkerName(aSharedWorkerName), mLoadingWorkerScript(false),
  mBusyCount(0), mMessagePortSerial(0),
  mParentStatus(Pending), mParentFrozen(false),
  mIsChromeWorker(aIsChromeWorker), mMainThreadObjectsForgotten(false),
  mWorkerType(aWorkerType),
  mCreationTimeStamp(TimeStamp::Now()),
  mCreationTimeHighRes((double)PR_Now() / PR_USEC_PER_MSEC)
{
  MOZ_ASSERT_IF(!IsDedicatedWorker(),
                !aSharedWorkerName.IsVoid() && NS_IsMainThread());
  MOZ_ASSERT_IF(IsDedicatedWorker(), aSharedWorkerName.IsEmpty());

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

    MOZ_ASSERT(IsDedicatedWorker());
    mNowBaseTimeStamp = aParent->NowBaseTimeStamp();
    mNowBaseTimeHighRes = aParent->NowBaseTimeHighRes();
  }
  else {
    AssertIsOnMainThread();

    RuntimeService::GetDefaultJSSettings(mJSSettings);

    if (IsDedicatedWorker() && mLoadInfo.mWindow &&
        mLoadInfo.mWindow->GetPerformance()) {
      mNowBaseTimeStamp = mLoadInfo.mWindow->GetPerformance()->GetDOMTiming()->
        GetNavigationStartTimeStamp();
      mNowBaseTimeHighRes = mLoadInfo.mWindow->GetPerformance()->GetDOMTiming()->
        GetNavigationStartHighRes();
    } else {
      mNowBaseTimeStamp = CreationTimeStamp();
      mNowBaseTimeHighRes = CreationTimeHighRes();
    }
  }
}

template <class Derived>
WorkerPrivateParent<Derived>::~WorkerPrivateParent()
{
  DropJSObjects(this);
}

template <class Derived>
JSObject*
WorkerPrivateParent<Derived>::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  MOZ_ASSERT(!IsSharedWorker(),
             "We should never wrap a WorkerPrivate for a SharedWorker");

  AssertIsOnParentThread();

  // XXXkhuey this should not need to be rooted, the analysis is dumb.
  // See bug 980181.
  JS::Rooted<JSObject*> wrapper(aCx,
    WorkerBinding::Wrap(aCx, ParentAsWorkerPrivate(), aGivenProto));
  if (wrapper) {
    MOZ_ALWAYS_TRUE(TryPreserveWrapper(wrapper));
  }

  return wrapper;
}

template <class Derived>
nsresult
WorkerPrivateParent<Derived>::DispatchPrivate(already_AddRefed<WorkerRunnable>&& aRunnable,
                                              nsIEventTarget* aSyncLoopTarget)
{
  // May be called on any thread!
  nsRefPtr<WorkerRunnable> runnable(aRunnable);

  WorkerPrivate* self = ParentAsWorkerPrivate();

  {
    MutexAutoLock lock(mMutex);

    MOZ_ASSERT_IF(aSyncLoopTarget, self->mThread);

    if (!self->mThread) {
      if (ParentStatus() == Pending || self->mStatus == Pending) {
        mPreStartRunnables.AppendElement(runnable);
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

    nsresult rv;
    if (aSyncLoopTarget) {
      rv = aSyncLoopTarget->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
    } else {
      rv = self->mThread->DispatchAnyThread(WorkerThreadFriendKey(), runnable.forget());
    }

    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mCondVar.Notify();
  }

  return NS_OK;
}

template <class Derived>
void
WorkerPrivateParent<Derived>::EnableDebugger()
{
  AssertIsOnParentThread();

  WorkerPrivate* self = ParentAsWorkerPrivate();

  MOZ_ASSERT(!self->mDebugger);
  self->mDebugger = new WorkerDebugger(self);

  if (NS_FAILED(RegisterWorkerDebugger(self->mDebugger))) {
    NS_WARNING("Failed to register worker debugger!");
    self->mDebugger = nullptr;
  }
}

template <class Derived>
void
WorkerPrivateParent<Derived>::DisableDebugger()
{
  AssertIsOnParentThread();

  WorkerPrivate* self = ParentAsWorkerPrivate();

  if (!self->mDebugger) {
    return;
  }

  if (NS_FAILED(UnregisterWorkerDebugger(self->mDebugger))) {
    NS_WARNING("Failed to unregister worker debugger!");
  }
}

template <class Derived>
nsresult
WorkerPrivateParent<Derived>::DispatchControlRunnable(
  already_AddRefed<WorkerControlRunnable>&& aWorkerControlRunnable)
{
  // May be called on any thread!
  nsRefPtr<WorkerControlRunnable> runnable(aWorkerControlRunnable);
  MOZ_ASSERT(runnable);

  WorkerPrivate* self = ParentAsWorkerPrivate();

  {
    MutexAutoLock lock(mMutex);

    if (self->mStatus == Dead) {
      NS_WARNING("A control runnable was posted to a worker that is already "
                 "shutting down!");
      return NS_ERROR_UNEXPECTED;
    }

    // Transfer ownership to the control queue.
    self->mControlQueue.Push(runnable.forget().take());

    if (JSContext* cx = self->mJSContext) {
      MOZ_ASSERT(self->mThread);

      JSRuntime* rt = JS_GetRuntime(cx);
      MOZ_ASSERT(rt);

      JS_RequestInterruptCallback(rt);
    }

    mCondVar.Notify();
  }

  return NS_OK;
}

template <class Derived>
nsresult
WorkerPrivateParent<Derived>::DispatchDebuggerRunnable(
  already_AddRefed<WorkerRunnable>&& aDebuggerRunnable)
{
  // May be called on any thread!

  nsRefPtr<WorkerRunnable> runnable(aDebuggerRunnable);

  MOZ_ASSERT(runnable);

  WorkerPrivate* self = ParentAsWorkerPrivate();

  {
    MutexAutoLock lock(mMutex);

    if (self->mStatus == Dead) {
      NS_WARNING("A debugger runnable was posted to a worker that is already "
                 "shutting down!");
      return NS_ERROR_UNEXPECTED;
    }

    // Transfer ownership to the debugger queue.
    self->mDebuggerQueue.Push(runnable.forget().take());

    mCondVar.Notify();
  }

  return NS_OK;
}

template <class Derived>
already_AddRefed<WorkerRunnable>
WorkerPrivateParent<Derived>::MaybeWrapAsWorkerRunnable(already_AddRefed<nsIRunnable>&& aRunnable)
{
  // May be called on any thread!

  nsCOMPtr<nsIRunnable> runnable(aRunnable);
  MOZ_ASSERT(runnable);

  nsRefPtr<WorkerRunnable> workerRunnable =
    WorkerRunnable::FromRunnable(runnable);
  if (workerRunnable) {
    return workerRunnable.forget();
  }

  nsCOMPtr<nsICancelableRunnable> cancelable = do_QueryInterface(runnable);
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

  if (IsSharedWorker() || IsServiceWorker()) {
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
    self->ScheduleDeletion(WorkerPrivate::WorkerNeverRan);
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
WorkerPrivateParent<Derived>::Freeze(JSContext* aCx, nsPIDOMWindow* aWindow)
{
  AssertIsOnParentThread();
  MOZ_ASSERT(aCx);

  // Shared workers are only frozen if all of their owning documents are
  // frozen. It can happen that mSharedWorkers is empty but this thread has
  // not been unregistered yet.
  if ((IsSharedWorker() || IsServiceWorker()) && mSharedWorkers.Count()) {
    AssertIsOnMainThread();

    struct Closure
    {
      nsPIDOMWindow* mWindow;
      bool mAllFrozen;

      explicit Closure(nsPIDOMWindow* aWindow)
      : mWindow(aWindow), mAllFrozen(true)
      {
        AssertIsOnMainThread();
        // aWindow may be null here.
      }

      static PLDHashOperator
      Freeze(const uint64_t& aKey,
              SharedWorker* aSharedWorker,
              void* aClosure)
      {
        AssertIsOnMainThread();
        MOZ_ASSERT(aSharedWorker);
        MOZ_ASSERT(aClosure);

        auto closure = static_cast<Closure*>(aClosure);

        if (closure->mWindow && aSharedWorker->GetOwner() == closure->mWindow) {
          // Calling Freeze() may change the refcount, ensure that the worker
          // outlives this call.
          nsRefPtr<SharedWorker> kungFuDeathGrip = aSharedWorker;

          aSharedWorker->Freeze();
        } else {
          MOZ_ASSERT_IF(aSharedWorker->GetOwner() && closure->mWindow,
                        !SameCOMIdentity(aSharedWorker->GetOwner(),
                                         closure->mWindow));
          if (!aSharedWorker->IsFrozen()) {
            closure->mAllFrozen = false;
          }
        }
        return PL_DHASH_NEXT;
      }
    };

    Closure closure(aWindow);

    mSharedWorkers.EnumerateRead(Closure::Freeze, &closure);

    if (!closure.mAllFrozen || mParentFrozen) {
      return true;
    }
  }

  mParentFrozen = true;

  {
    MutexAutoLock lock(mMutex);

    if (mParentStatus >= Terminating) {
      return true;
    }
  }

  nsRefPtr<FreezeRunnable> runnable =
    new FreezeRunnable(ParentAsWorkerPrivate());
  if (!runnable->Dispatch(aCx)) {
    return false;
  }

  return true;
}

template <class Derived>
bool
WorkerPrivateParent<Derived>::Thaw(JSContext* aCx, nsPIDOMWindow* aWindow)
{
  AssertIsOnParentThread();
  MOZ_ASSERT(aCx);

  if (IsDedicatedWorker() && !mParentFrozen) {
    // If we are in here, it means that this worker has been created when the
    // parent was actually suspended (maybe during a sync XHR), and in this case
    // we don't need to thaw.
    return true;
  }

  // Shared workers are resumed if any of their owning documents are thawed.
  // It can happen that mSharedWorkers is empty but this thread has not been
  // unregistered yet.
  if ((IsSharedWorker() || IsServiceWorker()) && mSharedWorkers.Count()) {
    AssertIsOnMainThread();

    struct Closure
    {
      nsPIDOMWindow* mWindow;
      bool mAnyRunning;

      explicit Closure(nsPIDOMWindow* aWindow)
      : mWindow(aWindow), mAnyRunning(false)
      {
        AssertIsOnMainThread();
        // aWindow may be null here.
      }

      static PLDHashOperator
      Thaw(const uint64_t& aKey,
              SharedWorker* aSharedWorker,
              void* aClosure)
      {
        AssertIsOnMainThread();
        MOZ_ASSERT(aSharedWorker);
        MOZ_ASSERT(aClosure);

        auto closure = static_cast<Closure*>(aClosure);

        if (closure->mWindow && aSharedWorker->GetOwner() == closure->mWindow) {
          // Calling Thaw() may change the refcount, ensure that the worker
          // outlives this call.
          nsRefPtr<SharedWorker> kungFuDeathGrip = aSharedWorker;

          aSharedWorker->Thaw();
          closure->mAnyRunning = true;
        } else {
          MOZ_ASSERT_IF(aSharedWorker->GetOwner() && closure->mWindow,
                        !SameCOMIdentity(aSharedWorker->GetOwner(),
                                         closure->mWindow));
          if (!aSharedWorker->IsFrozen()) {
            closure->mAnyRunning = true;
          }
        }
        return PL_DHASH_NEXT;
      }
    };

    Closure closure(aWindow);

    mSharedWorkers.EnumerateRead(Closure::Thaw, &closure);

    if (!closure.mAnyRunning || !mParentFrozen) {
      return true;
    }
  }

  MOZ_ASSERT(mParentFrozen);

  mParentFrozen = false;

  {
    MutexAutoLock lock(mMutex);

    if (mParentStatus >= Terminating) {
      return true;
    }
  }

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

  nsRefPtr<ThawRunnable> runnable =
    new ThawRunnable(ParentAsWorkerPrivate());
  if (!runnable->Dispatch(aCx)) {
    return false;
  }

  return true;
}

template <class Derived>
bool
WorkerPrivateParent<Derived>::Close()
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
    mBusyCount++;
    return true;
  }

  if (--mBusyCount == 0) {

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
WorkerPrivateParent<Derived>::ForgetOverridenLoadGroup(
                                          nsCOMPtr<nsILoadGroup>& aLoadGroupOut)
{
  AssertIsOnParentThread();

  // If we're not overriden, then do nothing here.  Let the load group get
  // handled in ForgetMainThreadObjects().
  if (!mLoadInfo.mInterfaceRequestor) {
    return;
  }

  mLoadInfo.mLoadGroup.swap(aLoadGroupOut);
}

template <class Derived>
void
WorkerPrivateParent<Derived>::ForgetMainThreadObjects(
                                      nsTArray<nsCOMPtr<nsISupports> >& aDoomed)
{
  AssertIsOnParentThread();
  MOZ_ASSERT(!mMainThreadObjectsForgotten);

  static const uint32_t kDoomedCount = 10;

  aDoomed.SetCapacity(kDoomedCount);

  SwapToISupportsArray(mLoadInfo.mWindow, aDoomed);
  SwapToISupportsArray(mLoadInfo.mScriptContext, aDoomed);
  SwapToISupportsArray(mLoadInfo.mBaseURI, aDoomed);
  SwapToISupportsArray(mLoadInfo.mResolvedScriptURI, aDoomed);
  SwapToISupportsArray(mLoadInfo.mPrincipal, aDoomed);
  SwapToISupportsArray(mLoadInfo.mChannel, aDoomed);
  SwapToISupportsArray(mLoadInfo.mCSP, aDoomed);
  SwapToISupportsArray(mLoadInfo.mLoadGroup, aDoomed);
  SwapToISupportsArray(mLoadInfo.mLoadFailedAsyncRunnable, aDoomed);
  SwapToISupportsArray(mLoadInfo.mInterfaceRequestor, aDoomed);
  // Before adding anything here update kDoomedCount above!

  MOZ_ASSERT(aDoomed.Length() == kDoomedCount);

  mMainThreadObjectsForgotten = true;
}

template <class Derived>
void
WorkerPrivateParent<Derived>::PostMessageInternal(
                                            JSContext* aCx,
                                            JS::Handle<JS::Value> aMessage,
                                            const Optional<Sequence<JS::Value>>& aTransferable,
                                            bool aToMessagePort,
                                            uint64_t aMessagePortSerial,
                                            ServiceWorkerClientInfo* aClientInfo,
                                            ErrorResult& aRv)
{
  AssertIsOnParentThread();

  {
    MutexAutoLock lock(mMutex);
    if (mParentStatus > Running) {
      return;
    }
  }

  const JSStructuredCloneCallbacks* callbacks;
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

    // The input sequence only comes from the generated bindings code, which
    // ensures it is rooted.
    JS::HandleValueArray elements =
      JS::HandleValueArray::fromMarkedLocation(realTransferable.Length(),
                                               realTransferable.Elements());

    JSObject* array =
      JS_NewArrayObject(aCx, elements);
    if (!array) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
    transferable.setObject(*array);
  }

  nsRefPtr<MessageEventRunnable> runnable =
    new MessageEventRunnable(ParentAsWorkerPrivate(),
                             WorkerRunnable::WorkerThreadModifyBusyCount,
                             aToMessagePort, aMessagePortSerial);

  if (!runnable->Write(aCx, aMessage, transferable, callbacks)) {
    aRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
    return;
  }

  runnable->SetMessageSource(aClientInfo);

  if (!runnable->Dispatch(aCx)) {
    aRv.Throw(NS_ERROR_FAILURE);
  }
}

template <class Derived>
void
WorkerPrivateParent<Derived>::PostMessageToServiceWorker(
                             JSContext* aCx, JS::Handle<JS::Value> aMessage,
                             const Optional<Sequence<JS::Value>>& aTransferable,
                             nsAutoPtr<ServiceWorkerClientInfo>& aClientInfo,
                             ErrorResult& aRv)
{
  AssertIsOnMainThread();
  PostMessageInternal(aCx, aMessage, aTransferable, false, 0,
                      aClientInfo.forget(), aRv);
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
                      nullptr, aRv);
}

template <class Derived>
bool
WorkerPrivateParent<Derived>::DispatchMessageEventToMessagePort(
                                JSContext* aCx, uint64_t aMessagePortSerial,
                                JSAutoStructuredCloneBuffer&& aBuffer,
                                WorkerStructuredCloneClosure& aClosure)
{
  AssertIsOnMainThread();

  JSAutoStructuredCloneBuffer buffer(Move(aBuffer));
  const JSStructuredCloneCallbacks* callbacks =
    WorkerStructuredCloneCallbacks(true);

  class MOZ_STACK_CLASS AutoCloneBufferCleaner final
  {
  public:
    AutoCloneBufferCleaner(JSAutoStructuredCloneBuffer& aBuffer,
                           const JSStructuredCloneCallbacks* aCallbacks,
                           WorkerStructuredCloneClosure& aClosure)
      : mBuffer(aBuffer)
      , mCallbacks(aCallbacks)
      , mClosure(aClosure)
    {}

    ~AutoCloneBufferCleaner()
    {
      mBuffer.clear(mCallbacks, &mClosure);
    }

  private:
    JSAutoStructuredCloneBuffer& mBuffer;
    const JSStructuredCloneCallbacks* mCallbacks;
    WorkerStructuredCloneClosure& mClosure;
  };

  WorkerStructuredCloneClosure closure;
  closure.mClonedObjects.SwapElements(aClosure.mClonedObjects);
  closure.mClonedImages.SwapElements(aClosure.mClonedImages);
  MOZ_ASSERT(aClosure.mMessagePorts.IsEmpty());
  closure.mMessagePortIdentifiers.SwapElements(aClosure.mMessagePortIdentifiers);

  AutoCloneBufferCleaner bufferCleaner(buffer, callbacks, closure);

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

  closure.mParentWindow = do_QueryInterface(port->GetParentObject());

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.InitWithLegacyErrorReporting(port->GetParentObject()))) {
    return false;
  }
  JSContext* cx = jsapi.cx();

  JS::Rooted<JS::Value> data(cx);
  if (!buffer.read(cx, &data, WorkerStructuredCloneCallbacks(true),
                   &closure)) {
    return false;
  }

  nsRefPtr<MessageEvent> event = new MessageEvent(port, nullptr, nullptr);
  nsresult rv =
    event->InitMessageEvent(NS_LITERAL_STRING("message"), false, false, data,
                            EmptyString(), EmptyString(), nullptr);
  if (NS_FAILED(rv)) {
    xpc::Throw(cx, rv);
    return false;
  }

  event->SetTrusted(true);

  event->SetPorts(new MessagePortList(port, closure.mMessagePorts));

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
void
WorkerPrivateParent<Derived>::UpdateRuntimeOptions(
                                    JSContext* aCx,
                                    const JS::RuntimeOptions& aRuntimeOptions)
{
  AssertIsOnParentThread();

  {
    MutexAutoLock lock(mMutex);
    mJSSettings.runtimeOptions = aRuntimeOptions;
  }

  nsRefPtr<UpdateRuntimeOptionsRunnable> runnable =
    new UpdateRuntimeOptionsRunnable(ParentAsWorkerPrivate(), aRuntimeOptions);
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
WorkerPrivateParent<Derived>::UpdateLanguages(JSContext* aCx,
                                              const nsTArray<nsString>& aLanguages)
{
  AssertIsOnParentThread();

  nsRefPtr<UpdateLanguagesRunnable> runnable =
    new UpdateLanguagesRunnable(ParentAsWorkerPrivate(), aLanguages);
  if (!runnable->Dispatch(aCx)) {
    NS_WARNING("Failed to update worker languages!");
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

  // The worker is already in this state. No need to dispatch an event.
  if (mOnLine == !aIsOffline) {
    return;
  }

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
  MOZ_ASSERT(IsSharedWorker() || IsServiceWorker());
  MOZ_ASSERT(!mSharedWorkers.Get(aSharedWorker->Serial()));

  if (IsSharedWorker()) {
    nsRefPtr<MessagePortRunnable> runnable =
      new MessagePortRunnable(ParentAsWorkerPrivate(), aSharedWorker->Serial(),
                              true);
    if (!runnable->Dispatch(aCx)) {
      return false;
    }
  }

  mSharedWorkers.Put(aSharedWorker->Serial(), aSharedWorker);

  // If there were other SharedWorker objects attached to this worker then they
  // may all have been frozen and this worker would need to be thawed.
  if (mSharedWorkers.Count() > 1 && !Thaw(aCx, nullptr)) {
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
  MOZ_ASSERT(IsSharedWorker() || IsServiceWorker());
  MOZ_ASSERT(mSharedWorkers.Get(aSharedWorker->Serial()));

  nsRefPtr<MessagePortRunnable> runnable =
    new MessagePortRunnable(ParentAsWorkerPrivate(), aSharedWorker->Serial(),
                            false);
  if (!runnable->Dispatch(aCx)) {
    JS_ReportPendingException(aCx);
  }

  mSharedWorkers.Remove(aSharedWorker->Serial());

  // If there are still SharedWorker objects attached to this worker then they
  // may all be frozen and this worker would need to be frozen. Otherwise,
  // if that was the last SharedWorker then it's time to cancel this worker.
  if (mSharedWorkers.Count()) {
    if (!Freeze(aCx, nullptr)) {
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
  for (size_t index = 0; index < sharedWorkers.Length(); index++) {
    nsRefPtr<SharedWorker>& sharedWorker = sharedWorkers[index];

    // May be null.
    nsPIDOMWindow* window = sharedWorker->GetOwner();

    RootedDictionary<ErrorEventInit> errorInit(aCx);
    errorInit.mBubbles = false;
    errorInit.mCancelable = true;
    errorInit.mMessage = aMessage;
    errorInit.mFilename = aFilename;
    errorInit.mLineno = aLineNumber;
    errorInit.mColno = aColumnNumber;

    nsRefPtr<ErrorEvent> errorEvent =
      ErrorEvent::Constructor(sharedWorker, NS_LITERAL_STRING("error"),
                              errorInit);
    if (!errorEvent) {
      ThrowAndReport(window, NS_ERROR_UNEXPECTED);
      continue;
    }

    errorEvent->SetTrusted(true);

    bool defaultActionEnabled;
    nsresult rv = sharedWorker->DispatchEvent(errorEvent, &defaultActionEnabled);
    if (NS_FAILED(rv)) {
      ThrowAndReport(window, rv);
      continue;
    }

    if (defaultActionEnabled) {
      // Add the owning window to our list so that we will fire an error event
      // at it later.
      if (!windowActions.Contains(window)) {
        windowActions.AppendElement(WindowAction(window));
      }
    } else {
      size_t actionsIndex = windowActions.LastIndexOf(WindowAction(window));
      if (actionsIndex != windowActions.NoIndex) {
        // Any listener that calls preventDefault() will prevent the window from
        // receiving the error event.
        windowActions[actionsIndex].mDefaultAction = false;
      }
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

    nsCOMPtr<nsIScriptGlobalObject> sgo =
      do_QueryInterface(windowAction.mWindow);
    MOZ_ASSERT(sgo);

    MOZ_ASSERT(NS_IsMainThread());
    RootedDictionary<ErrorEventInit> init(aCx);
    init.mLineno = aLineNumber;
    init.mFilename = aFilename;
    init.mMessage = aMessage;
    init.mCancelable = true;
    init.mBubbles = true;

    nsEventStatus status = nsEventStatus_eIgnore;
    rv = sgo->HandleScriptError(init, &status);
    if (NS_FAILED(rv)) {
      ThrowAndReport(windowAction.mWindow, rv);
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
  MOZ_ASSERT(IsSharedWorker() || IsServiceWorker());

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
  MOZ_ASSERT(IsSharedWorker() || IsServiceWorker());
  MOZ_ASSERT(aWindow);

  struct Closure
  {
    nsPIDOMWindow* mWindow;
    nsAutoTArray<nsRefPtr<SharedWorker>, 10> mSharedWorkers;

    explicit Closure(nsPIDOMWindow* aWindow)
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

  if (IsSharedWorker() || IsServiceWorker()) {
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

  mLocationInfo.mHostname.Truncate();
  nsContentUtils::GetHostOrIPv6WithBrackets(aBaseURI, mLocationInfo.mHostname);

  nsCOMPtr<nsIURL> url(do_QueryInterface(aBaseURI));
  if (!url || NS_FAILED(url->GetFilePath(mLocationInfo.mPathname))) {
    mLocationInfo.mPathname.Truncate();
  }

  nsCString temp;

  if (url && NS_SUCCEEDED(url->GetQuery(temp)) && !temp.IsEmpty()) {
    mLocationInfo.mSearch.Assign('?');
    mLocationInfo.mSearch.Append(temp);
  }

  if (NS_SUCCEEDED(aBaseURI->GetRef(temp)) && !temp.IsEmpty()) {
    nsCOMPtr<nsITextToSubURI> converter =
      do_GetService(NS_ITEXTTOSUBURI_CONTRACTID);
    if (converter && nsContentUtils::GettersDecodeURLHash()) {
      nsCString charset;
      nsAutoString unicodeRef;
      if (NS_SUCCEEDED(aBaseURI->GetOriginCharset(charset)) &&
          NS_SUCCEEDED(converter->UnEscapeURIForUI(charset, temp,
                                                   unicodeRef))) {
        mLocationInfo.mHash.Assign('#');
        mLocationInfo.mHash.Append(NS_ConvertUTF16toUTF8(unicodeRef));
      }
    }

    if (mLocationInfo.mHash.IsEmpty()) {
      mLocationInfo.mHash.Assign('#');
      mLocationInfo.mHash.Append(temp);
    }
  }

  if (NS_SUCCEEDED(aBaseURI->GetScheme(mLocationInfo.mProtocol))) {
    mLocationInfo.mProtocol.Append(':');
  }
  else {
    mLocationInfo.mProtocol.Truncate();
  }

  int32_t port;
  if (NS_SUCCEEDED(aBaseURI->GetPort(&port)) && port != -1) {
    mLocationInfo.mPort.AppendInt(port);

    nsAutoCString host(mLocationInfo.mHostname);
    host.Append(':');
    host.Append(mLocationInfo.mPort);

    mLocationInfo.mHost.Assign(host);
  }
  else {
    mLocationInfo.mHost.Assign(mLocationInfo.mHostname);
  }

  nsContentUtils::GetUTFOrigin(aBaseURI, mLocationInfo.mOrigin);
}

template <class Derived>
void
WorkerPrivateParent<Derived>::SetPrincipal(nsIPrincipal* aPrincipal,
                                           nsILoadGroup* aLoadGroup)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(NS_LoadGroupMatchesPrincipal(aLoadGroup, aPrincipal));
  MOZ_ASSERT(!mLoadInfo.mPrincipalInfo);

  mLoadInfo.mPrincipal = aPrincipal;
  mLoadInfo.mPrincipalIsSystem = nsContentUtils::IsSystemPrincipal(aPrincipal);
  uint16_t appStatus = aPrincipal->GetAppStatus();
  mLoadInfo.mIsInPrivilegedApp =
    (appStatus == nsIPrincipal::APP_STATUS_CERTIFIED ||
     appStatus == nsIPrincipal::APP_STATUS_PRIVILEGED);
  mLoadInfo.mIsInCertifiedApp = (appStatus == nsIPrincipal::APP_STATUS_CERTIFIED);

  mLoadInfo.mLoadGroup = aLoadGroup;

  mLoadInfo.mPrincipalInfo = new PrincipalInfo();
  mLoadInfo.mPrivateBrowsing = nsContentUtils::IsInPrivateBrowsing(aLoadGroup);

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
    PrincipalToPrincipalInfo(aPrincipal, mLoadInfo.mPrincipalInfo)));
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
WorkerPrivateParent<Derived>::UpdateOverridenLoadGroup(nsILoadGroup* aBaseLoadGroup)
{
  AssertIsOnMainThread();

  // The load group should have been overriden at init time.
  mLoadInfo.mInterfaceRequestor->MaybeAddTabChild(aBaseLoadGroup);
}

template <class Derived>
NS_IMPL_ADDREF_INHERITED(WorkerPrivateParent<Derived>, DOMEventTargetHelper)

template <class Derived>
NS_IMPL_RELEASE_INHERITED(WorkerPrivateParent<Derived>, DOMEventTargetHelper)

template <class Derived>
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(WorkerPrivateParent<Derived>)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

template <class Derived>
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(WorkerPrivateParent<Derived>,
                                                  DOMEventTargetHelper)
  tmp->AssertIsOnParentThread();

  // The WorkerPrivate::mSelfRef has a reference to itself, which is really
  // held by the worker thread.  We traverse this reference if and only if our
  // busy count is zero and we have not released the main thread reference.
  // We do not unlink it.  This allows the CC to break cycles involving the
  // WorkerPrivate and begin shutting it down (which does happen in unlink) but
  // ensures that the WorkerPrivate won't be deleted before we're done shutting
  // down the thread.

  if (!tmp->mBusyCount && !tmp->mMainThreadObjectsForgotten) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSelfRef)
  }

  // The various strong references in LoadInfo are managed manually and cannot
  // be cycle collected.
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

template <class Derived>
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(WorkerPrivateParent<Derived>,
                                                DOMEventTargetHelper)
  tmp->Terminate(nullptr);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

template <class Derived>
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(WorkerPrivateParent<Derived>,
                                               DOMEventTargetHelper)
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

class ReportDebuggerErrorRunnable final : public nsIRunnable
{
  nsRefPtr<WorkerDebugger> mDebugger;
  nsString mFilename;
  uint32_t mLineno;
  nsString mMessage;

public:
  ReportDebuggerErrorRunnable(WorkerDebugger* aDebugger,
                                const nsAString& aFilename, uint32_t aLineno,
                                const nsAString& aMessage)
  : mDebugger(aDebugger),
    mFilename(aFilename),
    mLineno(aLineno),
    mMessage(aMessage)
  {
  }

  NS_DECL_THREADSAFE_ISUPPORTS

private:
  ~ReportDebuggerErrorRunnable()
  { }

  NS_IMETHOD
  Run() override
  {
    mDebugger->ReportErrorToDebuggerOnMainThread(mFilename, mLineno, mMessage);

    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(ReportDebuggerErrorRunnable, nsIRunnable)

WorkerDebugger::WorkerDebugger(WorkerPrivate* aWorkerPrivate)
: mMutex("WorkerDebugger::mMutex"),
  mCondVar(mMutex, "WorkerDebugger::mCondVar"),
  mWorkerPrivate(aWorkerPrivate),
  mIsEnabled(false),
  mIsInitialized(false),
  mIsFrozen(false)
{
  mWorkerPrivate->AssertIsOnParentThread();
}

WorkerDebugger::~WorkerDebugger()
{
  MOZ_ASSERT(!mWorkerPrivate);
  MOZ_ASSERT(!mIsEnabled);

  if (!NS_IsMainThread()) {
    nsCOMPtr<nsIThread> mainThread;
    if (NS_FAILED(NS_GetMainThread(getter_AddRefs(mainThread)))) {
      NS_WARNING("Failed to proxy release of listeners, leaking instead!");
    }

    for (size_t index = 0; index < mListeners.Length(); ++index) {
      nsIWorkerDebuggerListener* listener = nullptr;
      mListeners[index].forget(&listener);
      if (NS_FAILED(NS_ProxyRelease(mainThread, listener))) {
        NS_WARNING("Failed to proxy release of listener, leaking instead!");
      }
    }
  }
}

NS_IMPL_ISUPPORTS(WorkerDebugger, nsIWorkerDebugger)

NS_IMETHODIMP
WorkerDebugger::GetIsClosed(bool* aResult)
{
  AssertIsOnMainThread();

  MutexAutoLock lock(mMutex);

  *aResult = !mWorkerPrivate;
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::GetIsChrome(bool* aResult)
{
  AssertIsOnMainThread();

  MutexAutoLock lock(mMutex);

  if (!mWorkerPrivate) {
    return NS_ERROR_UNEXPECTED;
  }

  *aResult = mWorkerPrivate->IsChromeWorker();
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::GetIsFrozen(bool* aResult)
{
  AssertIsOnMainThread();

  MutexAutoLock lock(mMutex);

  if (!mWorkerPrivate) {
    return NS_ERROR_UNEXPECTED;
  }

  *aResult = mIsFrozen;
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::GetIsInitialized(bool* aResult)
{
  AssertIsOnMainThread();

  MutexAutoLock lock(mMutex);

  if (!mWorkerPrivate) {
    return NS_ERROR_UNEXPECTED;
  }

  *aResult = mIsInitialized;
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::GetParent(nsIWorkerDebugger** aResult)
{
  AssertIsOnMainThread();

  MutexAutoLock lock(mMutex);

  if (!mWorkerPrivate) {
    return NS_ERROR_UNEXPECTED;
  }

  WorkerPrivate* parent = mWorkerPrivate->GetParent();
  if (!parent) {
    *aResult = nullptr;
    return NS_OK;
  }

  MOZ_ASSERT(mWorkerPrivate->IsDedicatedWorker());

  nsCOMPtr<nsIWorkerDebugger> debugger = parent->Debugger();
  debugger.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::GetType(uint32_t* aResult)
{
  AssertIsOnMainThread();

  MutexAutoLock lock(mMutex);

  if (!mWorkerPrivate) {
    return NS_ERROR_UNEXPECTED;
  }

  *aResult = mWorkerPrivate->Type();
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::GetUrl(nsAString& aResult)
{
  AssertIsOnMainThread();

  MutexAutoLock lock(mMutex);

  if (!mWorkerPrivate) {
    return NS_ERROR_UNEXPECTED;
  }

  aResult = mWorkerPrivate->ScriptURL();
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::GetWindow(nsIDOMWindow** aResult)
{
  AssertIsOnMainThread();

  MutexAutoLock lock(mMutex);

  if (!mWorkerPrivate) {
    return NS_ERROR_UNEXPECTED;
  }

  if (mWorkerPrivate->GetParent() || !mWorkerPrivate->IsDedicatedWorker()) {
    *aResult = nullptr;
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindow> window = mWorkerPrivate->GetWindow();
  window.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::Initialize(const nsAString& aURL, JSContext* aCx)
{
  AssertIsOnMainThread();

  MutexAutoLock lock(mMutex);

  if (!mWorkerPrivate) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!mIsInitialized) {
    nsRefPtr<CompileDebuggerScriptRunnable> runnable =
      new CompileDebuggerScriptRunnable(mWorkerPrivate, aURL);
    if (!runnable->Dispatch(aCx)) {
      return NS_ERROR_FAILURE;
    }

    mIsInitialized = true;
  }

  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::PostMessageMoz(const nsAString& aMessage, JSContext* aCx)
{
  AssertIsOnMainThread();

  MutexAutoLock lock(mMutex);

  if (!mWorkerPrivate || !mIsInitialized) {
    return NS_ERROR_UNEXPECTED;
  }

  nsRefPtr<DebuggerMessageEventRunnable> runnable =
    new DebuggerMessageEventRunnable(mWorkerPrivate, aMessage);
  if (!runnable->Dispatch(aCx)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::AddListener(nsIWorkerDebuggerListener* aListener)
{
  AssertIsOnMainThread();

  if (mListeners.Contains(aListener)) {
    return NS_ERROR_INVALID_ARG;
  }

  mListeners.AppendElement(aListener);
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::RemoveListener(nsIWorkerDebuggerListener* aListener)
{
  AssertIsOnMainThread();

  if (!mListeners.Contains(aListener)) {
    return NS_ERROR_INVALID_ARG;
  }

  mListeners.RemoveElement(aListener);
  return NS_OK;
}

void
WorkerDebugger::WaitIsEnabled(bool aIsEnabled)
{
  MutexAutoLock lock(mMutex);

  while (mIsEnabled != aIsEnabled) {
    mCondVar.Wait();
  }
}

void
WorkerDebugger::NotifyIsEnabled(bool aIsEnabled)
{
  mMutex.AssertCurrentThreadOwns();

  MOZ_ASSERT(mIsEnabled != aIsEnabled);
  mIsEnabled = aIsEnabled;
  mCondVar.Notify();
}

void
WorkerDebugger::Enable()
{
  AssertIsOnMainThread();

  MutexAutoLock lock(mMutex);

  MOZ_ASSERT(mWorkerPrivate);

  NotifyIsEnabled(true);
}

void
WorkerDebugger::Disable()
{
  AssertIsOnMainThread();

  MutexAutoLock lock(mMutex);

  MOZ_ASSERT(mWorkerPrivate);
  mWorkerPrivate = nullptr;

  {
    MutexAutoUnlock unlock(mMutex);

    nsTArray<nsCOMPtr<nsIWorkerDebuggerListener>> listeners(mListeners);
    for (size_t index = 0; index < listeners.Length(); ++index) {
      listeners[index]->OnClose();
    }
  }

  NotifyIsEnabled(false);
}

void
WorkerDebugger::Freeze()
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethod(this, &WorkerDebugger::FreezeOnMainThread);
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
    NS_DispatchToMainThread(runnable, NS_DISPATCH_NORMAL)));
}

void
WorkerDebugger::FreezeOnMainThread()
{
  AssertIsOnMainThread();

  mIsFrozen = true;

  for (size_t index = 0; index < mListeners.Length(); ++index) {
    mListeners[index]->OnFreeze();
  }
}

void
WorkerDebugger::Thaw()
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethod(this, &WorkerDebugger::ThawOnMainThread);
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
    NS_DispatchToMainThread(runnable, NS_DISPATCH_NORMAL)));
}

void
WorkerDebugger::ThawOnMainThread()
{
  AssertIsOnMainThread();

  mIsFrozen = false;

  for (size_t index = 0; index < mListeners.Length(); ++index) {
    mListeners[index]->OnThaw();
  }
}

void
WorkerDebugger::PostMessageToDebugger(const nsAString& aMessage)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethodWithArg<nsString>(this,
      &WorkerDebugger::PostMessageToDebuggerOnMainThread, nsString(aMessage));
  NS_DispatchToMainThread(runnable, NS_DISPATCH_NORMAL);
}

void
WorkerDebugger::PostMessageToDebuggerOnMainThread(const nsAString& aMessage)
{
  AssertIsOnMainThread();

  nsTArray<nsCOMPtr<nsIWorkerDebuggerListener>> listeners;

  {
    MutexAutoLock lock(mMutex);

    listeners.AppendElements(mListeners);
  }

  for (size_t index = 0; index < listeners.Length(); ++index) {
    listeners[index]->OnMessage(aMessage);
  }
}

void
WorkerDebugger::ReportErrorToDebugger(const nsAString& aFilename,
                                      uint32_t aLineno,
                                      const nsAString& aMessage)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  nsCOMPtr<nsIRunnable> runnable =
    new ReportDebuggerErrorRunnable(this, aFilename, aLineno, aMessage);
  if (NS_FAILED(NS_DispatchToMainThread(runnable, NS_DISPATCH_NORMAL))) {
    NS_WARNING("Failed to report error to debugger on main thread!");
  }
}

void
WorkerDebugger::ReportErrorToDebuggerOnMainThread(const nsAString& aFilename,
                                                  uint32_t aLineno,
                                                  const nsAString& aMessage)
{
  AssertIsOnMainThread();

  nsTArray<nsCOMPtr<nsIWorkerDebuggerListener>> listeners;
  {
    MutexAutoLock lock(mMutex);

    listeners.AppendElements(mListeners);
  }

  for (size_t index = 0; index < listeners.Length(); ++index) {
    listeners[index]->OnError(aFilename, aLineno, aMessage);
  }

  LogErrorToConsole(aMessage, aFilename, nsString(), aLineno, 0, 0, 0);
}

WorkerPrivate::WorkerPrivate(JSContext* aCx,
                             WorkerPrivate* aParent,
                             const nsAString& aScriptURL,
                             bool aIsChromeWorker, WorkerType aWorkerType,
                             const nsACString& aSharedWorkerName,
                             WorkerLoadInfo& aLoadInfo)
  : WorkerPrivateParent<WorkerPrivate>(aCx, aParent, aScriptURL,
                                       aIsChromeWorker, aWorkerType,
                                       aSharedWorkerName, aLoadInfo)
  , mJSContext(nullptr)
  , mPRThread(nullptr)
  , mDebuggerEventLoopLevel(0)
  , mErrorHandlerRecursionCount(0)
  , mNextTimeoutId(1)
  , mStatus(Pending)
  , mFrozen(false)
  , mTimerRunning(false)
  , mRunningExpiredTimeouts(false)
  , mCloseHandlerStarted(false)
  , mCloseHandlerFinished(false)
  , mPendingEventQueueClearing(false)
  , mMemoryReporterRunning(false)
  , mBlockedForMemoryReporter(false)
  , mCancelAllPendingRunnables(false)
  , mPeriodicGCTimerRunning(false)
  , mIdleGCTimerRunning(false)
  , mWorkerScriptExecutedSuccessfully(false)
{
  MOZ_ASSERT_IF(!IsDedicatedWorker(), !aSharedWorkerName.IsVoid());
  MOZ_ASSERT_IF(IsDedicatedWorker(), aSharedWorkerName.IsEmpty());

  if (aParent) {
    aParent->AssertIsOnWorkerThread();
    aParent->GetAllPreferences(mPreferences);
    mOnLine = aParent->OnLine();
  }
  else {
    AssertIsOnMainThread();
    RuntimeService::GetDefaultPreferences(mPreferences);
    mOnLine = !NS_IsOffline() && !NS_IsAppOffline(aLoadInfo.mPrincipal);
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
  return WorkerPrivate::Constructor(aGlobal, aScriptURL, false,
                                    WorkerTypeDedicated, EmptyCString(),
                                    nullptr, aRv);
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
  return WorkerPrivate::Constructor(aGlobal, aScriptURL, true,
                                    WorkerTypeDedicated, EmptyCString(),
                                    nullptr, aRv)
                                    .downcast<ChromeWorkerPrivate>();
}

// static
bool
ChromeWorkerPrivate::WorkerAvailable(JSContext* aCx, JSObject* /* unused */)
{
  // Chrome is always allowed to use workers, and content is never
  // allowed to use ChromeWorker, so all we have to check is the
  // caller.  However, chrome workers apparently might not have a
  // system principal, so we have to check for them manually.
  if (NS_IsMainThread()) {
    return nsContentUtils::IsCallerChrome();
  }

  return GetWorkerPrivateFromContext(aCx)->IsChromeWorker();
}

// static
already_AddRefed<WorkerPrivate>
WorkerPrivate::Constructor(const GlobalObject& aGlobal,
                           const nsAString& aScriptURL,
                           bool aIsChromeWorker, WorkerType aWorkerType,
                           const nsACString& aSharedWorkerName,
                           WorkerLoadInfo* aLoadInfo, ErrorResult& aRv)
{
  JSContext* cx = aGlobal.Context();
  return Constructor(cx, aScriptURL, aIsChromeWorker, aWorkerType,
                     aSharedWorkerName, aLoadInfo, aRv);
}

// static
already_AddRefed<WorkerPrivate>
WorkerPrivate::Constructor(JSContext* aCx,
                           const nsAString& aScriptURL,
                           bool aIsChromeWorker, WorkerType aWorkerType,
                           const nsACString& aSharedWorkerName,
                           WorkerLoadInfo* aLoadInfo, ErrorResult& aRv)
{
  WorkerPrivate* parent = NS_IsMainThread() ?
                          nullptr :
                          GetCurrentThreadWorkerPrivate();
  if (parent) {
    parent->AssertIsOnWorkerThread();
  } else {
    AssertIsOnMainThread();
  }

  MOZ_ASSERT_IF(aWorkerType != WorkerTypeDedicated,
                !aSharedWorkerName.IsVoid());
  MOZ_ASSERT_IF(aWorkerType == WorkerTypeDedicated,
                aSharedWorkerName.IsEmpty());

  Maybe<WorkerLoadInfo> stackLoadInfo;
  if (!aLoadInfo) {
    stackLoadInfo.emplace();

    nsresult rv = GetLoadInfo(aCx, nullptr, parent, aScriptURL,
                              aIsChromeWorker, InheritLoadGroup,
                              aWorkerType, stackLoadInfo.ptr());
    if (NS_FAILED(rv)) {
      scriptloader::ReportLoadError(aCx, aScriptURL, rv, !parent);
      aRv.Throw(rv);
      return nullptr;
    }

    aLoadInfo = stackLoadInfo.ptr();
  }

  // NB: This has to be done before creating the WorkerPrivate, because it will
  // attempt to use static variables that are initialized in the RuntimeService
  // constructor.
  RuntimeService* runtimeService;

  if (!parent) {
    runtimeService = RuntimeService::GetOrCreateService();
    if (!runtimeService) {
      JS_ReportError(aCx, "Failed to create runtime service!");
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }
  }
  else {
    runtimeService = RuntimeService::GetService();
  }

  MOZ_ASSERT(runtimeService);

  nsRefPtr<WorkerPrivate> worker =
    new WorkerPrivate(aCx, parent, aScriptURL, aIsChromeWorker,
                      aWorkerType, aSharedWorkerName, *aLoadInfo);

  if (!runtimeService->RegisterWorker(aCx, worker)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  worker->EnableDebugger();

  nsRefPtr<CompileScriptRunnable> compiler =
    new CompileScriptRunnable(worker, aScriptURL);
  if (!compiler->Dispatch(aCx)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  worker->mSelfRef = worker;

  return worker.forget();
}

// static
nsresult
WorkerPrivate::GetLoadInfo(JSContext* aCx, nsPIDOMWindow* aWindow,
                           WorkerPrivate* aParent, const nsAString& aScriptURL,
                           bool aIsChromeWorker,
                           LoadGroupBehavior aLoadGroupBehavior,
                           WorkerType aWorkerType,
                           WorkerLoadInfo* aLoadInfo)
{
  using namespace mozilla::dom::workers::scriptloader;
  using mozilla::dom::indexedDB::IDBFactory;

  MOZ_ASSERT(aCx);
  MOZ_ASSERT_IF(NS_IsMainThread(), aCx == nsContentUtils::GetCurrentJSContext());

  if (aWindow) {
    AssertIsOnMainThread();
  }

  WorkerLoadInfo loadInfo;
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

    // StartAssignment() is used instead getter_AddRefs because, getter_AddRefs
    // does QI in debug build and, if this worker runs in a child process,
    // HttpChannelChild will crash because it's not thread-safe.
    rv = ChannelFromScriptURLWorkerThread(aCx, aParent, aScriptURL,
                                          loadInfo.mChannel.StartAssignment());
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
    loadInfo.mFromWindow = aParent->IsFromWindow();
    loadInfo.mWindowID = aParent->WindowID();
    loadInfo.mIndexedDBAllowed = aParent->IsIndexedDBAllowed();
    loadInfo.mPrivateBrowsing = aParent->IsInPrivateBrowsing();
    loadInfo.mServiceWorkersTestingInWindow =
      aParent->ServiceWorkersTestingInWindow();
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

    // Chrome callers (whether creating a ChromeWorker or Worker) always get the
    // system principal here as they're allowed to load anything. The script
    // loader will refuse to run any script that does not also have the system
    // principal.
    if (isChrome) {
      rv = ssm->GetSystemPrincipal(getter_AddRefs(loadInfo.mPrincipal));
      NS_ENSURE_SUCCESS(rv, rv);

      loadInfo.mPrincipalIsSystem = true;
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
        // TODO: fix this for SharedWorkers with multiple documents (bug 1177935)
        loadInfo.mServiceWorkersTestingInWindow =
          outerWindow->GetServiceWorkersTestingEnabled();
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
      loadInfo.mLoadGroup = document->GetDocumentLoadGroup();

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

      uint16_t appStatus = loadInfo.mPrincipal->GetAppStatus();
      loadInfo.mIsInPrivilegedApp =
        (appStatus == nsIPrincipal::APP_STATUS_CERTIFIED ||
         appStatus == nsIPrincipal::APP_STATUS_PRIVILEGED);
      loadInfo.mIsInCertifiedApp = (appStatus == nsIPrincipal::APP_STATUS_CERTIFIED);
      loadInfo.mFromWindow = true;
      loadInfo.mWindowID = globalWindow->WindowID();
      loadInfo.mIndexedDBAllowed = IDBFactory::AllowedForWindow(globalWindow);
      loadInfo.mPrivateBrowsing = nsContentUtils::IsInPrivateBrowsing(document);
    } else {
      // Not a window
      MOZ_ASSERT(isChrome);

      // We're being created outside of a window. Need to figure out the script
      // that is creating us in order for us to use relative URIs later on.
      JS::AutoFilename fileName;
      if (JS::DescribeScriptedCaller(aCx, &fileName)) {
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

        rv = scriptFile->InitWithPath(NS_ConvertUTF8toUTF16(fileName.get()));
        if (NS_SUCCEEDED(rv)) {
          rv = NS_NewFileURI(getter_AddRefs(loadInfo.mBaseURI),
                             scriptFile);
        }
        if (NS_FAILED(rv)) {
          // As expected, fileName is not a path, so proceed with
          // a uri.
          rv = NS_NewURI(getter_AddRefs(loadInfo.mBaseURI),
                         fileName.get());
        }
        if (NS_FAILED(rv)) {
          return rv;
        }
      }
      loadInfo.mXHRParamsAllowed = true;
      loadInfo.mFromWindow = false;
      loadInfo.mWindowID = UINT64_MAX;
      loadInfo.mIndexedDBAllowed = true;
      loadInfo.mPrivateBrowsing = false;
    }

    MOZ_ASSERT(loadInfo.mPrincipal);
    MOZ_ASSERT(isChrome || !loadInfo.mDomain.IsEmpty());

    if (!nsContentUtils::GetContentSecurityPolicy(getter_AddRefs(loadInfo.mCSP))) {
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

    if (!loadInfo.mLoadGroup || aLoadGroupBehavior == OverrideLoadGroup) {
      OverrideLoadInfoLoadGroup(loadInfo);
    }
    MOZ_ASSERT(NS_LoadGroupMatchesPrincipal(loadInfo.mLoadGroup,
                                            loadInfo.mPrincipal));

    rv = ChannelFromScriptURLMainThread(loadInfo.mPrincipal, loadInfo.mBaseURI,
                                        document, loadInfo.mLoadGroup,
                                        aScriptURL,
                                        ContentPolicyType(aWorkerType),
                                        getter_AddRefs(loadInfo.mChannel));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = NS_GetFinalChannelURI(loadInfo.mChannel,
                               getter_AddRefs(loadInfo.mResolvedScriptURI));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  aLoadInfo->StealFrom(loadInfo);
  return NS_OK;
}

// static
void
WorkerPrivate::OverrideLoadInfoLoadGroup(WorkerLoadInfo& aLoadInfo)
{
  MOZ_ASSERT(!aLoadInfo.mInterfaceRequestor);

  aLoadInfo.mInterfaceRequestor =
    new WorkerLoadInfo::InterfaceRequestor(aLoadInfo.mPrincipal,
                                           aLoadInfo.mLoadGroup);
  aLoadInfo.mInterfaceRequestor->MaybeAddTabChild(aLoadInfo.mLoadGroup);

  nsCOMPtr<nsILoadGroup> loadGroup =
    do_CreateInstance(NS_LOADGROUP_CONTRACTID);

  nsresult rv =
    loadGroup->SetNotificationCallbacks(aLoadInfo.mInterfaceRequestor);
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(rv));

  aLoadInfo.mLoadGroup = loadGroup.forget();
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
    Status currentStatus;
    bool debuggerRunnablesPending = false;
    bool normalRunnablesPending = false;

    {
      MutexAutoLock lock(mMutex);

      while (mControlQueue.IsEmpty() &&
             !(debuggerRunnablesPending = !mDebuggerQueue.IsEmpty()) &&
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
        // Flush uncaught rejections immediately, without
        // waiting for a next tick.
        PromiseDebugging::FlushUncaughtRejections();

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

        mPreemptingRunnableInfos.Clear();

        // Clear away our MessagePorts.
        mWorkerPorts.Clear();

        // Unroot the globals
        mScope = nullptr;
        mDebuggerScope = nullptr;

        return;
      }
    }

    if (debuggerRunnablesPending || normalRunnablesPending) {
      // Start the periodic GC timer if it is not already running.
      SetGCTimerMode(PeriodicTimer);
    }

    if (debuggerRunnablesPending) {
      WorkerRunnable* runnable;

      {
        MutexAutoLock lock(mMutex);

        mDebuggerQueue.Pop(runnable);
        debuggerRunnablesPending = !mDebuggerQueue.IsEmpty();
      }

      MOZ_ASSERT(runnable);
      static_cast<nsIRunnable*>(runnable)->Run();
      runnable->Release();

      if (debuggerRunnablesPending) {
        WorkerDebuggerGlobalScope* globalScope = DebuggerGlobalScope();
        MOZ_ASSERT(globalScope);

        // Now *might* be a good time to GC. Let the JS engine make the decision.
        JSAutoCompartment ac(aCx, globalScope->GetGlobalJSObject());
        JS_MaybeGC(aCx);
      }
    } else if (normalRunnablesPending) {
      MOZ_ASSERT(NS_HasPendingEvents(mThread));

      // Process a single runnable from the main queue.
      MOZ_ALWAYS_TRUE(NS_ProcessNextEvent(mThread, false));

      // Only perform the Promise microtask checkpoint on the outermost event
      // loop.  Don't run it, for example, during sync XHR or importScripts.
      (void)Promise::PerformMicroTaskCheckpoint();

      normalRunnablesPending = NS_HasPendingEvents(mThread);
      if (normalRunnablesPending && GlobalScope()) {
        // Now *might* be a good time to GC. Let the JS engine make the decision.
        JSAutoCompartment ac(aCx, GlobalScope()->GetGlobalJSObject());
        JS_MaybeGC(aCx);
      }
    }

    if (!debuggerRunnablesPending && !normalRunnablesPending) {
      // Both the debugger event queue and the normal event queue has been
      // exhausted, cancel the periodic GC timer and schedule the idle GC timer.
      SetGCTimerMode(IdleTimer);
    }
  }

  MOZ_CRASH("Shouldn't get here!");
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

  // Run any preempting runnables that match this depth.
  if (!mPreemptingRunnableInfos.IsEmpty()) {
    nsTArray<PreemptingRunnableInfo> pendingRunnableInfos;

    for (uint32_t index = 0;
         index < mPreemptingRunnableInfos.Length();
         index++) {
      PreemptingRunnableInfo& preemptingRunnableInfo =
        mPreemptingRunnableInfos[index];

      if (preemptingRunnableInfo.mRecursionDepth == aRecursionDepth) {
        preemptingRunnableInfo.mRunnable->Run();
        preemptingRunnableInfo.mRunnable = nullptr;
      } else {
        PreemptingRunnableInfo* pending = pendingRunnableInfos.AppendElement();
        pending->mRunnable.swap(preemptingRunnableInfo.mRunnable);
        pending->mRecursionDepth = preemptingRunnableInfo.mRecursionDepth;
      }
    }

    mPreemptingRunnableInfos.SwapElements(pendingRunnableInfos);
  }
}

void
WorkerPrivate::AfterProcessNextEvent(uint32_t aRecursionDepth)
{
  AssertIsOnWorkerThread();
  MOZ_ASSERT(aRecursionDepth);
}

bool
WorkerPrivate::RunBeforeNextEvent(nsIRunnable* aRunnable)
{
  AssertIsOnWorkerThread();
  MOZ_ASSERT(aRunnable);
  MOZ_ASSERT_IF(!mPreemptingRunnableInfos.IsEmpty(),
                NS_HasPendingEvents(mThread));

  const uint32_t recursionDepth =
    mThread->RecursionDepth(WorkerThreadFriendKey());

  PreemptingRunnableInfo* preemptingRunnableInfo =
    mPreemptingRunnableInfos.AppendElement();

  preemptingRunnableInfo->mRunnable = aRunnable;

  // Due to the weird way that the thread recursion counter is implemented we
  // subtract one from the recursion level if we have one.
  preemptingRunnableInfo->mRecursionDepth =
    recursionDepth ? recursionDepth - 1 : 0;

  // Ensure that we have a pending event so that the runnable will be guaranteed
  // to run.
  if (mPreemptingRunnableInfos.Length() == 1 && !NS_HasPendingEvents(mThread)) {
    nsRefPtr<DummyRunnable> dummyRunnable = new DummyRunnable(this);
    if (NS_FAILED(Dispatch(dummyRunnable.forget()))) {
      NS_WARNING("RunBeforeNextEvent called after the thread is shutting "
                 "down!");
      mPreemptingRunnableInfos.Clear();
      return false;
    }
  }

  return true;
}

void
WorkerPrivate::MaybeDispatchLoadFailedRunnable()
{
  AssertIsOnWorkerThread();

  nsCOMPtr<nsIRunnable> runnable = StealLoadFailedAsyncRunnable();
  if (!runnable) {
    return;
  }

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(runnable.forget())));
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
WorkerPrivate::InterruptCallback(JSContext* aCx)
{
  AssertIsOnWorkerThread();

  bool mayContinue = true;
  bool scheduledIdleGC = false;

  for (;;) {
    // Run all control events now.
    mayContinue = ProcessAllControlRunnables();

    bool mayFreeze = mFrozen;
    if (mayFreeze) {
      MutexAutoLock lock(mMutex);
      mayFreeze = mStatus <= Running;
    }

    if (!mayContinue || !mayFreeze) {
      break;
    }

    // Cancel the periodic GC timer here before freezing. The idle GC timer
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
  MOZ_ASSERT(mPRThread);

  *aIsOnCurrentThread = PR_GetCurrentThread() == mPRThread;
  return NS_OK;
}

void
WorkerPrivate::ScheduleDeletion(WorkerRanOrNot aRanOrNot)
{
  AssertIsOnWorkerThread();
  MOZ_ASSERT(mChildWorkers.IsEmpty());
  MOZ_ASSERT(mSyncLoopStack.IsEmpty());
  MOZ_ASSERT(!mPendingEventQueueClearing);

  ClearMainEventQueue(aRanOrNot);
#ifdef DEBUG
  if (WorkerRan == aRanOrNot) {
    nsIThread* currentThread = NS_GetCurrentThread();
    MOZ_ASSERT(currentThread);
    MOZ_ASSERT(!NS_HasPendingEvents(currentThread));
  }
#endif

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
    if (NS_FAILED(NS_DispatchToMainThread(runnable))) {
      NS_WARNING("Failed to dispatch runnable!");
    }
  }
}

bool
WorkerPrivate::BlockAndCollectRuntimeStats(JS::RuntimeStats* aRtStats,
                                           bool aAnonymize)
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
  // currently in a ctypes call) then we need to trigger the interrupt
  // callback to trap the worker.
  if (!mBlockedForMemoryReporter) {
    JS_RequestInterruptCallback(rt);

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
    succeeded = JS::CollectRuntimeStats(rt, aRtStats, nullptr, aAnonymize);
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
WorkerPrivate::ClearMainEventQueue(WorkerRanOrNot aRanOrNot)
{
  AssertIsOnWorkerThread();

  MOZ_ASSERT(!mSyncLoopStack.Length());
  MOZ_ASSERT(!mCancelAllPendingRunnables);
  mCancelAllPendingRunnables = true;

  if (WorkerNeverRan == aRanOrNot) {
    for (uint32_t count = mPreStartRunnables.Length(), index = 0;
         index < count;
         index++) {
      nsRefPtr<WorkerRunnable> runnable = mPreStartRunnables[index].forget();
      static_cast<nsIRunnable*>(runnable.get())->Run();
    }
  } else {
    nsIThread* currentThread = NS_GetCurrentThread();
    MOZ_ASSERT(currentThread);

    NS_ProcessPendingEvents(currentThread);
  }

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
WorkerPrivate::FreezeInternal(JSContext* aCx)
{
  AssertIsOnWorkerThread();

  NS_ASSERTION(!mFrozen, "Already frozen!");

  mFrozen = true;
  mDebugger->Freeze();
  return true;
}

bool
WorkerPrivate::ThawInternal(JSContext* aCx)
{
  AssertIsOnWorkerThread();

  NS_ASSERTION(mFrozen, "Not yet frozen!");

  mFrozen = false;
  mDebugger->Thaw();
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

  MOZ_ASSERT(!mFeatures.Contains(aFeature), "Already know about this one!");

  if (mFeatures.IsEmpty() && !ModifyBusyCountFromWorker(aCx, true)) {
    return false;
  }

  mFeatures.AppendElement(aFeature);
  return true;
}

void
WorkerPrivate::RemoveFeature(JSContext* aCx, WorkerFeature* aFeature)
{
  AssertIsOnWorkerThread();

  MOZ_ASSERT(mFeatures.Contains(aFeature), "Didn't know about this one!");
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

  nsTObserverArray<WorkerFeature*>::ForwardIterator iter(mFeatures);
  while (iter.HasMore()) {
    WorkerFeature* feature = iter.GetNext();
    if (!feature->Notify(aCx, aStatus)) {
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

  while (!loopInfo->mCompleted) {
    bool normalRunnablesPending = false;

    // Don't block with the periodic GC timer running.
    if (!NS_HasPendingEvents(mThread)) {
      SetGCTimerMode(IdleTimer);
    }

    // Wait for something to do.
    {
      MutexAutoLock lock(mMutex);

      for (;;) {
        while (mControlQueue.IsEmpty() &&
               !normalRunnablesPending &&
               !(normalRunnablesPending = NS_HasPendingEvents(mThread))) {
          WaitForWorkerEvents();
        }

        ProcessAllControlRunnablesLocked();

        // NB: If we processed a NotifyRunnable, we might have run non-control
        // runnables, one of which may have shut down the sync loop.
        if (normalRunnablesPending || loopInfo->mCompleted) {
          break;
        }
      }
    }

    if (normalRunnablesPending) {
      // Make sure the periodic timer is running before we continue.
      SetGCTimerMode(PeriodicTimer);

      MOZ_ALWAYS_TRUE(NS_ProcessNextEvent(mThread, false));

      // Now *might* be a good time to GC. Let the JS engine make the decision.
      if (JS::CurrentGlobalOrNull(cx)) {
        JS_MaybeGC(cx);
      }
    }
  }

  // Make sure that the stack didn't change underneath us.
  MOZ_ASSERT(mSyncLoopStack[currentLoopIndex] == loopInfo);

  return DestroySyncLoop(currentLoopIndex);
}

bool
WorkerPrivate::DestroySyncLoop(uint32_t aLoopIndex, nsIThreadInternal* aThread)
{
  MOZ_ASSERT(!mSyncLoopStack.IsEmpty());
  MOZ_ASSERT(mSyncLoopStack.Length() - 1 == aLoopIndex);

  if (!aThread) {
    aThread = mThread;
  }

  // We're about to delete the loop, stash its event target and result.
  SyncLoopInfo* loopInfo = mSyncLoopStack[aLoopIndex];
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

    // This will delete |loopInfo|!
    mSyncLoopStack.RemoveElementAt(aLoopIndex);
  }

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(aThread->PopEventQueue(nestedEventTarget)));

  if (!mSyncLoopStack.Length() && mPendingEventQueueClearing) {
    ClearMainEventQueue(WorkerRan);
    mPendingEventQueueClearing = false;
  }

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

    // The input sequence only comes from the generated bindings code, which
    // ensures it is rooted.
    JS::HandleValueArray elements =
      JS::HandleValueArray::fromMarkedLocation(realTransferable.Length(),
                                               realTransferable.Elements());

    JSObject* array = JS_NewArrayObject(aCx, elements);
    if (!array) {
      aRv = NS_ERROR_OUT_OF_MEMORY;
      return;
    }
    transferable.setObject(*array);
  }

  const JSStructuredCloneCallbacks* callbacks =
    IsChromeWorker() ?
    &gChromeWorkerStructuredCloneCallbacks :
    &gWorkerStructuredCloneCallbacks;

  nsRefPtr<MessageEventRunnable> runnable =
    new MessageEventRunnable(this,
                             WorkerRunnable::ParentThreadUnchangedBusyCount,
                             aToMessagePort, aMessagePortSerial);

  if (!runnable->Write(aCx, aMessage, transferable, callbacks)) {
    aRv = NS_ERROR_DOM_DATA_CLONE_ERR;
    return;
  }

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

void
WorkerPrivate::EnterDebuggerEventLoop()
{
  AssertIsOnWorkerThread();

  JSContext* cx = GetJSContext();
  MOZ_ASSERT(cx);

  uint32_t currentEventLoopLevel = ++mDebuggerEventLoopLevel;

  while (currentEventLoopLevel <= mDebuggerEventLoopLevel) {
    bool debuggerRunnablesPending = false;

    {
      MutexAutoLock lock(mMutex);

      debuggerRunnablesPending = !mDebuggerQueue.IsEmpty();
    }

    // Don't block with the periodic GC timer running.
    if (!debuggerRunnablesPending) {
      SetGCTimerMode(IdleTimer);
    }

    // Wait for something to do
    {
      MutexAutoLock lock(mMutex);

      while (mControlQueue.IsEmpty() &&
             !(debuggerRunnablesPending = !mDebuggerQueue.IsEmpty())) {
        WaitForWorkerEvents();
      }

      ProcessAllControlRunnablesLocked();
    }

    if (debuggerRunnablesPending) {
      // Start the periodic GC timer if it is not already running.
      SetGCTimerMode(PeriodicTimer);

      WorkerRunnable* runnable;

      {
        MutexAutoLock lock(mMutex);

        mDebuggerQueue.Pop(runnable);
      }

      MOZ_ASSERT(runnable);
      static_cast<nsIRunnable*>(runnable)->Run();
      runnable->Release();

      // Now *might* be a good time to GC. Let the JS engine make the decision.
      if (JS::CurrentGlobalOrNull(cx)) {
        JS_MaybeGC(cx);
      }
    }
  }
}

void
WorkerPrivate::LeaveDebuggerEventLoop()
{
  AssertIsOnWorkerThread();

  MutexAutoLock lock(mMutex);

  if (mDebuggerEventLoopLevel > 0) {
    --mDebuggerEventLoopLevel;
  }
}

void
WorkerPrivate::PostMessageToDebugger(const nsAString& aMessage)
{
  mDebugger->PostMessageToDebugger(aMessage);
}

void
WorkerPrivate::SetDebuggerImmediate(JSContext* aCx, Function& aHandler,
                                    ErrorResult& aRv)
{
  AssertIsOnWorkerThread();

  nsRefPtr<DebuggerImmediateRunnable> runnable =
    new DebuggerImmediateRunnable(this, aHandler);
  if (!runnable->Dispatch(aCx)) {
    aRv.Throw(NS_ERROR_FAILURE);
  }
}

void
WorkerPrivate::ReportErrorToDebugger(const nsAString& aFilename,
                                     uint32_t aLineno,
                                     const nsAString& aMessage)
{
  mDebugger->ReportErrorToDebugger(aFilename, aLineno, aMessage);
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
    // NB: If we're in a sync loop, we can't clear the queue immediately,
    // because this is the wrong queue. So we have to defer it until later.
    if (mSyncLoopStack.Length()) {
      mPendingEventQueueClearing = true;
    } else {
      ClearMainEventQueue(WorkerRan);
    }
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
    // ErrorEvent objects don't have a |name| field the way ES |Error| objects
    // do. Traditionally (and mostly by accident), the |message| field of
    // ErrorEvent has corresponded to |Name: Message| of the original Error
    // object. Things have been cleaned up in the JS engine, so now we need to
    // format this string explicitly.
    JS::Rooted<JSString*> messageStr(aCx,
                                     js::ErrorReportToString(aCx, aReport));
    if (messageStr) {
      nsAutoJSString autoStr;
      if (autoStr.init(aCx, messageStr)) {
        message = autoStr;
      }
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
                     errorNumber != JSMSG_OUT_OF_MEMORY &&
                     JS::CurrentGlobalOrNull(aCx);

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
    if (!nsJSUtils::GetCallingLocation(aCx, newInfo->mFilename, &newInfo->mLineNumber)) {
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
      JS::HandleValueArray args =
        JS::HandleValueArray::fromMarkedLocation(info->mExtraArgVals.Length(),
                                                 info->mExtraArgVals.Elements()->address());
      JS::Rooted<JS::Value> callable(aCx, info->mTimeoutCallable);
      if (!JS_CallFunctionValue(aCx, global, callable, args, &rval) &&
          !JS_ReportPendingException(aCx)) {
        retval = false;
        break;
      }
    }
    else {
      nsString expression = info->mTimeoutString;

      JS::CompileOptions options(aCx);
      options.setFileAndLine(info->mFilename.get(), info->mLineNumber)
             .setNoScriptRval(true);

      JS::Rooted<JS::Value> unused(aCx);
      if ((expression.IsEmpty() ||
           !JS::Evaluate(aCx, options,
                         expression.get(), expression.Length(), &unused)) &&
          !JS_ReportPendingException(aCx)) {
        retval = false;
        break;
      }
    }

    // Since we might be processing more timeouts, go ahead and flush
    // the promise queue now before we do that.
    Promise::PerformMicroTaskCheckpoint();

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
WorkerPrivate::UpdateRuntimeOptionsInternal(
                                    JSContext* aCx,
                                    const JS::RuntimeOptions& aRuntimeOptions)
{
  AssertIsOnWorkerThread();

  JS::RuntimeOptionsRef(aCx) = aRuntimeOptions;

  for (uint32_t index = 0; index < mChildWorkers.Length(); index++) {
    mChildWorkers[index]->UpdateRuntimeOptions(aCx, aRuntimeOptions);
  }
}

void
WorkerPrivate::UpdateLanguagesInternal(JSContext* aCx,
                                       const nsTArray<nsString>& aLanguages)
{
  WorkerGlobalScope* globalScope = GlobalScope();
  if (globalScope) {
    nsRefPtr<WorkerNavigator> nav = globalScope->GetExistingNavigator();
    if (nav) {
      nav->SetLanguages(aLanguages);
    }
  }

  for (uint32_t index = 0; index < mChildWorkers.Length(); index++) {
    mChildWorkers[index]->UpdateLanguages(aCx, aLanguages);
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
WorkerPrivate::GarbageCollectInternal(JSContext* aCx, bool aShrinking,
                                      bool aCollectChildren)
{
  AssertIsOnWorkerThread();

  if (!GlobalScope()) {
    // We haven't compiled anything yet. Just bail out.
    return;
  }

  if (aShrinking || aCollectChildren) {
    JSRuntime* rt = JS_GetRuntime(aCx);
    JS::PrepareForFullGC(rt);

    if (aShrinking) {
      JS::GCForReason(rt, GC_SHRINK, JS::gcreason::DOM_WORKER);

      if (!aCollectChildren) {
        LOG(("Worker %p collected idle garbage\n", this));
      }
    }
    else {
      JS::GCForReason(rt, GC_NORMAL, JS::gcreason::DOM_WORKER);
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
WorkerPrivate::SetThread(WorkerThread* aThread)
{
  if (aThread) {
#ifdef DEBUG
    {
      bool isOnCurrentThread;
      MOZ_ASSERT(NS_SUCCEEDED(aThread->IsOnCurrentThread(&isOnCurrentThread)));
      MOZ_ASSERT(isOnCurrentThread);
    }
#endif

    MOZ_ASSERT(!mPRThread);
    mPRThread = PRThreadFromThread(aThread);
    MOZ_ASSERT(mPRThread);
  }
  else {
    MOZ_ASSERT(mPRThread);
  }

  const WorkerThreadFriendKey friendKey;

  nsRefPtr<WorkerThread> doomedThread;

  { // Scope so that |doomedThread| is released without holding the lock.
    MutexAutoLock lock(mMutex);

    if (aThread) {
      MOZ_ASSERT(!mThread);
      MOZ_ASSERT(mStatus == Pending);

      mThread = aThread;
      mThread->SetWorker(friendKey, this);

      if (!mPreStartRunnables.IsEmpty()) {
        for (uint32_t index = 0; index < mPreStartRunnables.Length(); index++) {
          MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
            mThread->DispatchAnyThread(friendKey, mPreStartRunnables[index].forget())));
        }
        mPreStartRunnables.Clear();
      }
    }
    else {
      MOZ_ASSERT(mThread);

      mThread->SetWorker(friendKey, nullptr);

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

  nsRefPtr<MessageEvent> event =
    MessageEvent::Constructor(globalObject,
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

WorkerGlobalScope*
WorkerPrivate::GetOrCreateGlobalScope(JSContext* aCx)
{
  AssertIsOnWorkerThread();

  if (!mScope) {
    nsRefPtr<WorkerGlobalScope> globalScope;
    if (IsSharedWorker()) {
      globalScope = new SharedWorkerGlobalScope(this, SharedWorkerName());
    } else if (IsServiceWorker()) {
      globalScope = new ServiceWorkerGlobalScope(this, SharedWorkerName());
    } else {
      globalScope = new DedicatedWorkerGlobalScope(this);
    }

    JS::Rooted<JSObject*> global(aCx);
    NS_ENSURE_TRUE(globalScope->WrapGlobalObject(aCx, &global), nullptr);

    JSAutoCompartment ac(aCx, global);

    // RegisterBindings() can spin a nested event loop so we have to set mScope
    // before calling it, and we have to make sure to unset mScope if it fails.
    mScope = Move(globalScope);

    if (!RegisterBindings(aCx, global)) {
      mScope = nullptr;
      return nullptr;
    }

    JS_FireOnNewGlobalObject(aCx, global);
  }

  return mScope;
}

WorkerDebuggerGlobalScope*
WorkerPrivate::CreateDebuggerGlobalScope(JSContext* aCx)
{
  AssertIsOnWorkerThread();

  MOZ_ASSERT(!mDebuggerScope);

  nsRefPtr<WorkerDebuggerGlobalScope> globalScope =
    new WorkerDebuggerGlobalScope(this);

  JS::Rooted<JSObject*> global(aCx);
  NS_ENSURE_TRUE(globalScope->WrapGlobalObject(aCx, &global), nullptr);

  JSAutoCompartment ac(aCx, global);

  if (!JS_DefineDebuggerObject(aCx, global)) {
    return nullptr;
  }

  JS_FireOnNewGlobalObject(aCx, global);

  mDebuggerScope = globalScope.forget();

  return mDebuggerScope;
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
EventTarget::DispatchFromScript(nsIRunnable* aRunnable, uint32_t aFlags)
{
  nsCOMPtr<nsIRunnable> event(aRunnable);
  return Dispatch(event.forget(), aFlags);
}

template <class Derived>
NS_IMETHODIMP
WorkerPrivateParent<Derived>::
EventTarget::Dispatch(already_AddRefed<nsIRunnable>&& aRunnable, uint32_t aFlags)
{
  // May be called on any thread!
  nsCOMPtr<nsIRunnable> event(aRunnable);

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

  if (event) {
    workerRunnable = mWorkerPrivate->MaybeWrapAsWorkerRunnable(event.forget());
  }

  nsresult rv =
    mWorkerPrivate->DispatchPrivate(workerRunnable.forget(), mNestedEventTarget);
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

const JSStructuredCloneCallbacks*
WorkerStructuredCloneCallbacks(bool aMainRuntime)
{
  return aMainRuntime ?
         &gMainThreadWorkerStructuredCloneCallbacks :
         &gWorkerStructuredCloneCallbacks;
}

const JSStructuredCloneCallbacks*
ChromeWorkerStructuredCloneCallbacks(bool aMainRuntime)
{
  return aMainRuntime ?
         &gMainThreadChromeWorkerStructuredCloneCallbacks :
         &gChromeWorkerStructuredCloneCallbacks;
}

// Force instantiation.
template class WorkerPrivateParent<WorkerPrivate>;

WorkerStructuredCloneClosure::WorkerStructuredCloneClosure()
{}

WorkerStructuredCloneClosure::~WorkerStructuredCloneClosure()
{}

void
WorkerStructuredCloneClosure::Clear()
{
  mParentWindow = nullptr;
  mClonedObjects.Clear();
  mClonedImages.Clear();
  mMessagePorts.Clear();
  mMessagePortIdentifiers.Clear();
  mTransferredPorts.Clear();
}

END_WORKERS_NAMESPACE

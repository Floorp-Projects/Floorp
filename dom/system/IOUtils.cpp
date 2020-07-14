/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include "mozilla/dom/IOUtils.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/Services.h"
#include "mozilla/Span.h"
#include "nspr/prio.h"
#include "nspr/private/pprio.h"
#include "nspr/prtypes.h"
#include "nsIGlobalObject.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsThreadManager.h"

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
#  include <fcntl.h>
#endif

#define REJECT_IF_NULL_EVENT_TARGET(aEventTarget, aJSPromise)  \
  do {                                                         \
    if (!(aEventTarget)) {                                     \
      (aJSPromise)                                             \
          ->MaybeRejectWithAbortError(                         \
              "Could not dispatch task to background thread"); \
      return (aJSPromise).forget();                            \
    }                                                          \
  } while (false)

#define REJECT_IF_SHUTTING_DOWN(aJSPromise)                       \
  do {                                                            \
    if (sShutdownStarted) {                                       \
      (aJSPromise)                                                \
          ->MaybeRejectWithNotAllowedError(                       \
              "Shutting down and refusing additional I/O tasks"); \
      return (aJSPromise).forget();                               \
    }                                                             \
  } while (false)

namespace mozilla {
namespace dom {

/* static */
StaticDataMutex<StaticRefPtr<nsISerialEventTarget>>
    IOUtils::sBackgroundEventTarget("sBackgroundEventTarget");
/* static */
StaticRefPtr<nsIAsyncShutdownClient> IOUtils::sBarrier;
/* static */
Atomic<bool> IOUtils::sShutdownStarted = Atomic<bool>(false);

/* static */
already_AddRefed<Promise> IOUtils::Read(GlobalObject& aGlobal,
                                        const nsAString& aPath,
                                        const Optional<uint32_t>& aMaxBytes) {
  RefPtr<Promise> promise = CreateJSPromise(aGlobal);
  NS_ENSURE_TRUE(!!promise, nullptr);
  REJECT_IF_SHUTTING_DOWN(promise);

  // Process arguments.
  uint32_t toRead = 0;
  if (aMaxBytes.WasPassed()) {
    toRead = aMaxBytes.Value();
    if (toRead == 0) {
      // Resolve with an empty buffer.
      nsTArray<uint8_t> arr(0);
      TypedArrayCreator<Uint8Array> arrCreator(arr);
      promise->MaybeResolve(arrCreator);
      return promise.forget();
    }
  }

  NS_ConvertUTF16toUTF8 path(aPath);

  // Do the IO on a background thread and return the result to this thread.
  RefPtr<nsISerialEventTarget> bg = GetBackgroundEventTarget();
  REJECT_IF_NULL_EVENT_TARGET(bg, promise);

  InvokeAsync(
      bg, __func__,
      [path, toRead]() {
        MOZ_ASSERT(!NS_IsMainThread());

        UniquePtr<PRFileDesc, PR_CloseDelete> fd =
            OpenExistingSync(path.get(), PR_RDONLY);
        if (!fd) {
          return IOReadMozPromise::CreateAndReject(
              nsLiteralCString("Could not open file"), __func__);
        }
        uint32_t bufSize;
        if (toRead == 0) {  // maxBytes was unspecified.
          // Limitation: We cannot read files that are larger than the max size
          //    of a TypedArray (UINT32_MAX bytes). Reject if the file is too
          //    big to be read.
          PRFileInfo64 info;
          if (PR_FAILURE == PR_GetOpenFileInfo64(fd.get(), &info)) {
            return IOReadMozPromise::CreateAndReject(
                nsLiteralCString("Could not get file info"), __func__);
          }
          uint32_t fileSize = info.size;
          if (fileSize > UINT32_MAX) {
            return IOReadMozPromise::CreateAndReject(
                nsLiteralCString("File is too large to read"), __func__);
          }
          bufSize = fileSize;
        } else {
          bufSize = toRead;
        }
        nsTArray<uint8_t> fileContents;
        nsresult er = IOUtils::ReadSync(fd.get(), bufSize, fileContents);

        if (NS_SUCCEEDED(er)) {
          return IOReadMozPromise::CreateAndResolve(std::move(fileContents),
                                                    __func__);
        }
        if (er == NS_ERROR_OUT_OF_MEMORY) {
          return IOReadMozPromise::CreateAndReject(
              nsLiteralCString("Out of memory"), __func__);
        }
        return IOReadMozPromise::CreateAndReject(
            nsLiteralCString("Unexpected error reading file"), __func__);
      })
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [promise = RefPtr(promise)](const nsTArray<uint8_t>& aBuf) {
            AutoJSAPI jsapi;
            if (NS_WARN_IF(!jsapi.Init(promise->GetGlobalObject()))) {
              promise->MaybeReject(NS_ERROR_UNEXPECTED);
              return;
            }

            const TypedArrayCreator<Uint8Array> arrayCreator(aBuf);
            promise->MaybeResolve(arrayCreator);
          },
          [promise = RefPtr(promise)](const nsACString& aMsg) {
            promise->MaybeRejectWithOperationError(aMsg);
          });

  return promise.forget();
}

/* static */
already_AddRefed<Promise> IOUtils::WriteAtomic(
    GlobalObject& aGlobal, const nsAString& aPath, const Uint8Array& aData,
    const WriteAtomicOptions& aOptions) {
  RefPtr<Promise> promise = CreateJSPromise(aGlobal);
  NS_ENSURE_TRUE(!!promise, nullptr);
  REJECT_IF_SHUTTING_DOWN(promise);

  // Process arguments.
  aData.ComputeState();
  FallibleTArray<uint8_t> toWrite;
  if (!toWrite.InsertElementsAt(0, aData.Data(), aData.Length(), fallible)) {
    promise->MaybeRejectWithOperationError("Out of memory");
    return promise.forget();
  }

  // TODO: Implement tmpPath and backupFile options.
  // The data to be written to file might be larger than can be written in any
  // single call, so we must truncate the file and set the write mode to append
  // to the file.
  int32_t flags = PR_WRONLY | PR_TRUNCATE | PR_APPEND;
  bool noOverwrite = false;
  if (aOptions.IsAnyMemberPresent()) {
    if (aOptions.mBackupFile.WasPassed() || aOptions.mTmpPath.WasPassed()) {
      promise->MaybeRejectWithNotSupportedError(
          "Unsupported options were passed");
      return promise.forget();
    }
    if (aOptions.mFlush) {
      flags |= PR_SYNC;
    }
    noOverwrite = aOptions.mNoOverwrite;
  }

  NS_ConvertUTF16toUTF8 path(aPath);

  // Do the IO on a background thread and return the result to this thread.
  RefPtr<nsISerialEventTarget> bg = GetBackgroundEventTarget();
  REJECT_IF_NULL_EVENT_TARGET(bg, promise);

  InvokeAsync(bg, __func__,
              [path, flags, noOverwrite, toWrite = std::move(toWrite)]() {
                MOZ_ASSERT(!NS_IsMainThread());

                UniquePtr<PRFileDesc, PR_CloseDelete> fd =
                    OpenExistingSync(path.get(), flags);
                if (noOverwrite && fd) {
                  return IOWriteMozPromise::CreateAndReject(
                      nsLiteralCString("Refusing to overwrite file"), __func__);
                }
                if (!fd) {
                  fd = CreateFileSync(path.get(), flags);
                }
                if (!fd) {
                  return IOWriteMozPromise::CreateAndReject(
                      nsLiteralCString("Could not open file"), __func__);
                }
                uint32_t result;
                nsresult er = IOUtils::WriteSync(fd.get(), toWrite, result);

                if (NS_SUCCEEDED(er)) {
                  return IOWriteMozPromise::CreateAndResolve(result, __func__);
                }
                return IOWriteMozPromise::CreateAndReject(
                    nsLiteralCString("Unexpected error writing file"),
                    __func__);
              })
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [promise = RefPtr(promise)](const uint32_t& aBytesWritten) {
            AutoJSAPI jsapi;
            if (NS_WARN_IF(!jsapi.Init(promise->GetGlobalObject()))) {
              promise->MaybeReject(NS_ERROR_UNEXPECTED);
              return;
            }
            promise->MaybeResolve(aBytesWritten);
          },
          [promise = RefPtr(promise)](const nsACString& aMsg) {
            promise->MaybeRejectWithOperationError(aMsg);
          });

  return promise.forget();
}

/* static */
already_AddRefed<nsISerialEventTarget> IOUtils::GetBackgroundEventTarget() {
  if (sShutdownStarted) {
    return nullptr;
  }

  auto lockedBackgroundEventTarget = sBackgroundEventTarget.Lock();
  if (!lockedBackgroundEventTarget.ref()) {
    RefPtr<nsISerialEventTarget> et;
    MOZ_ALWAYS_SUCCEEDS(NS_CreateBackgroundTaskQueue(
        "IOUtils::BackgroundIOThread", getter_AddRefs(et)));
    MOZ_ASSERT(et);
    *lockedBackgroundEventTarget = et;

    if (NS_IsMainThread()) {
      IOUtils::SetShutdownHooks();
    } else {
      nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction(
          __func__, []() { IOUtils::SetShutdownHooks(); });
      NS_DispatchToMainThread(runnable.forget());
    }
  }
  return do_AddRef(*lockedBackgroundEventTarget);
}

/* static */
already_AddRefed<nsIAsyncShutdownClient> IOUtils::GetShutdownBarrier() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!sBarrier) {
    nsCOMPtr<nsIAsyncShutdownService> svc = services::GetAsyncShutdownService();
    MOZ_ASSERT(svc);

    nsCOMPtr<nsIAsyncShutdownClient> barrier;
    nsresult rv = svc->GetXpcomWillShutdown(getter_AddRefs(barrier));
    NS_ENSURE_SUCCESS(rv, nullptr);
    sBarrier = barrier;
  }
  return do_AddRef(sBarrier);
}

/* static */
void IOUtils::SetShutdownHooks() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIAsyncShutdownClient> barrier = GetShutdownBarrier();
  nsCOMPtr<nsIAsyncShutdownBlocker> blocker = new IOUtilsShutdownBlocker();

  nsresult rv = barrier->AddBlocker(
      blocker, NS_LITERAL_STRING_FROM_CSTRING(__FILE__), __LINE__,
      u"IOUtils: waiting for pending I/O to finish"_ns);
  // Adding a new shutdown blocker should only fail if the current shutdown
  // phase has completed. Ensure that we have set our shutdown flag to stop
  // accepting new I/O tasks in this case.
  if (NS_FAILED(rv)) {
    sShutdownStarted = true;
  }
  NS_ENSURE_SUCCESS_VOID(rv);
}

/* static */
already_AddRefed<Promise> IOUtils::CreateJSPromise(GlobalObject& aGlobal) {
  ErrorResult er;
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<Promise> promise = Promise::Create(global, er);
  if (er.Failed()) {
    return nullptr;
  }
  MOZ_ASSERT(promise);
  return do_AddRef(promise);
}

/* static */
UniquePtr<PRFileDesc, PR_CloseDelete> IOUtils::OpenExistingSync(
    const char* aPath, int32_t aFlags) {
  // Ensure that CREATE_FILE and EXCL flags were not included, as we do not
  // want to create a new file.
  MOZ_ASSERT((aFlags & (PR_CREATE_FILE | PR_EXCL)) == 0);

  return UniquePtr<PRFileDesc, PR_CloseDelete>(PR_Open(aPath, aFlags, 0));
}

/* static */
UniquePtr<PRFileDesc, PR_CloseDelete> IOUtils::CreateFileSync(const char* aPath,
                                                              int32_t aFlags,
                                                              int32_t aMode) {
  return UniquePtr<PRFileDesc, PR_CloseDelete>(
      PR_Open(aPath, aFlags | PR_CREATE_FILE | PR_EXCL, aMode));
}

/* static */
nsresult IOUtils::ReadSync(PRFileDesc* aFd, const uint32_t aBufSize,
                           nsTArray<uint8_t>& aResult) {
  MOZ_ASSERT(!NS_IsMainThread());

  nsTArray<uint8_t> buffer;
  if (!buffer.SetCapacity(aBufSize, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // If possible, advise the operating system that we will be reading the file
  // pointed to by |aFD| sequentially, in full. This advice is not binding, it
  // informs the OS about our expectations as an application.
#if defined(HAVE_POSIX_FADVISE)
  posix_fadvise(PR_FileDesc2NativeHandle(aFd), 0, 0, POSIX_FADV_SEQUENTIAL);
#endif

  uint32_t totalRead = 0;
  while (totalRead != aBufSize) {
    int32_t nRead =
        PR_Read(aFd, buffer.Elements() + totalRead, aBufSize - totalRead);
    if (nRead == 0) {
      break;
    }
    if (nRead < 0) {
      return NS_ERROR_UNEXPECTED;
    }
    totalRead += nRead;
    DebugOnly<bool> success = buffer.SetLength(totalRead, fallible);
    MOZ_ASSERT(success);
  }
  aResult = std::move(buffer);
  return NS_OK;
}

/* static */
nsresult IOUtils::WriteSync(PRFileDesc* aFd, const nsTArray<uint8_t>& aBytes,
                            uint32_t& aResult) {
  // aBytes comes from a JavaScript TypedArray, which has UINT32_MAX max length.
  MOZ_ASSERT(aBytes.Length() <= UINT32_MAX);
  MOZ_ASSERT(!NS_IsMainThread());

  if (aBytes.Length() == 0) {
    aResult = 0;
    return NS_OK;
  }

  uint32_t bytesWritten = 0;

  // PR_Write can only write up to PR_INT32_MAX bytes at a time, but the data
  // source might be as large as UINT32_MAX bytes.
  uint32_t chunkSize = PR_INT32_MAX;
  uint32_t pendingBytes = aBytes.Length();

  while (pendingBytes > 0) {
    if (pendingBytes < chunkSize) {
      chunkSize = pendingBytes;
    }
    int32_t rv = PR_Write(aFd, aBytes.Elements() + bytesWritten, chunkSize);
    if (rv < 0) {
      return NS_ERROR_FILE_CORRUPTED;
    }
    pendingBytes -= rv;
    bytesWritten += rv;
  }

  aResult = bytesWritten;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(IOUtilsShutdownBlocker, nsIAsyncShutdownBlocker);

NS_IMETHODIMP IOUtilsShutdownBlocker::GetName(nsAString& aName) {
  aName = u"IOUtils Blocker"_ns;
  return NS_OK;
}

NS_IMETHODIMP IOUtilsShutdownBlocker::BlockShutdown(
    nsIAsyncShutdownClient* aBarrierClient) {
  nsCOMPtr<nsISerialEventTarget> et = IOUtils::GetBackgroundEventTarget();

  IOUtils::sShutdownStarted = true;

  if (!IOUtils::sBarrier) {
    return NS_ERROR_NULL_POINTER;
  }

  nsCOMPtr<nsIRunnable> backgroundRunnable =
      NS_NewRunnableFunction(__func__, [self = RefPtr(this)]() {
        nsCOMPtr<nsIRunnable> mainThreadRunnable =
            NS_NewRunnableFunction(__func__, [self = RefPtr(self)]() {
              IOUtils::sBarrier->RemoveBlocker(self);

              auto lockedBackgroundET = IOUtils::sBackgroundEventTarget.Lock();
              *lockedBackgroundET = nullptr;
              IOUtils::sBarrier = nullptr;
            });
        nsresult er = NS_DispatchToMainThread(mainThreadRunnable.forget());
        NS_ENSURE_SUCCESS_VOID(er);
      });

  return et->Dispatch(backgroundRunnable.forget(),
                      nsIEventTarget::DISPATCH_NORMAL);
}

NS_IMETHODIMP IOUtilsShutdownBlocker::GetState(nsIPropertyBag** aState) {
  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla

#undef REJECT_IF_NULL_EVENT_TARGET
#undef REJECT_IF_SHUTTING_DOWN

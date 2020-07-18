/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include "mozilla/dom/IOUtils.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/Result.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Services.h"
#include "mozilla/Span.h"
#include "mozilla/TextUtils.h"
#include "nspr/prio.h"
#include "nspr/private/pprio.h"
#include "nspr/prtypes.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsIGlobalObject.h"
#include "nsNativeCharsetUtils.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsThreadManager.h"
#include "SpecialSystemDirectory.h"

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

// static helper functions

/**
 * Platform-specific (e.g. Windows, Unix) implementations of XPCOM APIs may
 * report I/O errors inconsistently. For convenience, this function will attempt
 * to match a |nsresult| against known results which imply a file cannot be
 * found.
 *
 * @see nsLocalFileWin.cpp
 * @see nsLocalFileUnix.cpp
 */
static bool IsFileNotFound(nsresult aResult) {
  switch (aResult) {
    case NS_ERROR_FILE_NOT_FOUND:
    case NS_ERROR_FILE_TARGET_DOES_NOT_EXIST:
      return true;
    default:
      return false;
  }
}

/**
 * Formats an error message and appends the error name to the end.
 */
template <typename... Args>
static nsCString FormatErrorMessage(nsresult aError, const char* const aMessage,
                                    Args... aArgs) {
  nsPrintfCString msg(aMessage, aArgs...);

  if (const char* errName = GetStaticErrorName(aError)) {
    msg.AppendPrintf(": %s", errName);
  } else {
    // In the exceptional case where there is no error name, print the literal
    // integer value of the nsresult as an upper case hex value so it can be
    // located easily in searchfox.
    msg.AppendPrintf(": 0x%" PRIX32, static_cast<uint32_t>(aError));
  }

  return std::move(msg);
}

static nsCString FormatErrorMessage(nsresult aError,
                                    const char* const aMessage) {
  const char* errName = GetStaticErrorName(aError);
  if (errName) {
    return nsPrintfCString("%s: %s", aMessage, errName);
  }
  // In the exceptional case where there is no error name, print the literal
  // integer value of the nsresult as an upper case hex value so it can be
  // located easily in searchfox.
  return nsPrintfCString("%s: 0x%" PRIX32, aMessage,
                         static_cast<uint32_t>(aError));
}

#ifdef XP_WIN
constexpr char PathSeparator = u'\\';
#else
constexpr char PathSeparator = u'/';
#endif

/* static */
bool IOUtils::IsAbsolutePath(const nsAString& aPath) {
  // NB: This impl is adapted from js::shell::IsAbsolutePath(JSLinearString*).
  const size_t length = aPath.Length();

#ifdef XP_WIN
  // On Windows there are various forms of absolute paths (see
  // http://msdn.microsoft.com/en-us/library/windows/desktop/aa365247%28v=vs.85%29.aspx
  // for details):
  //
  //   "\..."
  //   "\\..."
  //   "C:\..."
  //
  // The first two cases are handled by the common test below so we only need a
  // specific test for the last one here.

  if (length > 3 && mozilla::IsAsciiAlpha(aPath.CharAt(0)) &&
      aPath.CharAt(1) == u':' && aPath.CharAt(2) == u'\\') {
    return true;
  }
#endif
  return length > 0 && aPath.CharAt(0) == PathSeparator;
}

// IOUtils implementation

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

  RefPtr<nsISerialEventTarget> bg = GetBackgroundEventTarget();
  REJECT_IF_NULL_EVENT_TARGET(bg, promise);

  // Process arguments.
  if (!IsAbsolutePath(aPath)) {
    promise->MaybeRejectWithOperationError(
        "Only absolute file paths are permitted");
    return promise.forget();
  }

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

  InvokeAsync(bg, __func__,
              [path = nsAutoString(aPath), toRead]() {
                MOZ_ASSERT(!NS_IsMainThread());

                UniquePtr<PRFileDesc, PR_CloseDelete> fd =
                    OpenExistingSync(path, PR_RDONLY);
                if (!fd) {
                  return IOReadMozPromise::CreateAndReject(
                      nsPrintfCString("Could not open file at %s",
                                      NS_ConvertUTF16toUTF8(path).get()),
                      __func__);
                }
                uint32_t bufSize;
                if (toRead == 0) {  // maxBytes was unspecified.
                  // Limitation: We cannot read files that are larger than the
                  //    max size of a TypedArray (UINT32_MAX bytes).
                  //    Reject if the file is too big to be read.
                  PRFileInfo64 info;
                  if (PR_FAILURE == PR_GetOpenFileInfo64(fd.get(), &info)) {
                    return IOReadMozPromise::CreateAndReject(
                        nsPrintfCString("Could not get info for file at %s",
                                        NS_ConvertUTF16toUTF8(path).get()),
                        __func__);
                  }
                  uint32_t fileSize = info.size;
                  if (fileSize > UINT32_MAX) {
                    return IOReadMozPromise::CreateAndReject(
                        nsPrintfCString("File at %s is too large to read",
                                        NS_ConvertUTF16toUTF8(path).get()),
                        __func__);
                  }
                  bufSize = fileSize;
                } else {
                  bufSize = toRead;
                }
                nsTArray<uint8_t> fileContents;
                nsresult rv =
                    IOUtils::ReadSync(fd.get(), bufSize, fileContents);

                if (NS_SUCCEEDED(rv)) {
                  return IOReadMozPromise::CreateAndResolve(
                      std::move(fileContents), __func__);
                }
                if (rv == NS_ERROR_OUT_OF_MEMORY) {
                  return IOReadMozPromise::CreateAndReject("Out of memory"_ns,
                                                           __func__);
                }
                return IOReadMozPromise::CreateAndReject(
                    nsPrintfCString("Unexpected error reading file at %s",
                                    NS_ConvertUTF16toUTF8(path).get()),
                    __func__);
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

  RefPtr<nsISerialEventTarget> bg = GetBackgroundEventTarget();
  REJECT_IF_NULL_EVENT_TARGET(bg, promise);

  // Process arguments.
  if (!IsAbsolutePath(aPath)) {
    promise->MaybeRejectWithOperationError(
        "Only absolute file paths are permitted");
    return promise.forget();
  }
  aData.ComputeState();
  FallibleTArray<uint8_t> toWrite;
  if (!toWrite.InsertElementsAt(0, aData.Data(), aData.Length(), fallible)) {
    promise->MaybeRejectWithOperationError("Out of memory");
    return promise.forget();
  }

  InvokeAsync(
      bg, __func__,
      [destPath = nsString(aPath), toWrite = std::move(toWrite), aOptions]() {
        MOZ_ASSERT(!NS_IsMainThread());

        // Check if the file exists and test it against the noOverwrite option.
        const bool& noOverwrite = aOptions.mNoOverwrite;
        bool exists = false;
        {
          UniquePtr<PRFileDesc, PR_CloseDelete> fd =
              OpenExistingSync(destPath, PR_RDONLY);
          exists = !!fd;
        }

        if (noOverwrite && exists) {
          return IOWriteMozPromise::CreateAndReject(
              nsPrintfCString("Refusing to overwrite the file at %s",
                              NS_ConvertUTF16toUTF8(destPath).get()),
              __func__);
        }

        // If backupFile was specified, perform the backup as a move.
        if (exists && aOptions.mBackupFile.WasPassed()) {
          const nsString& backupFile(aOptions.mBackupFile.Value());
          nsresult rv = MoveSync(destPath, backupFile, noOverwrite);
          if (NS_FAILED(rv)) {
            return IOWriteMozPromise::CreateAndReject(
                FormatErrorMessage(rv,
                                   "Failed to back up the file from %s to %s",
                                   NS_ConvertUTF16toUTF8(destPath).get(),
                                   NS_ConvertUTF16toUTF8(backupFile).get()),
                __func__);
          }
        }

        // If tmpPath was specified, we will write to there first, then perform
        // a move to ensure the file ends up at the final requested destination.
        nsString tmpPath = destPath;
        if (aOptions.mTmpPath.WasPassed()) {
          tmpPath = aOptions.mTmpPath.Value();
        }
        // The data to be written to file might be larger than can be
        // written in any single call, so we must truncate the file and
        // set the write mode to append to the file.
        int32_t flags = PR_WRONLY | PR_TRUNCATE | PR_APPEND;
        if (aOptions.mFlush) {
          flags |= PR_SYNC;
        }

        // Try to perform the write and ensure that the file is closed before
        // continuing.
        uint32_t result = 0;
        {
          UniquePtr<PRFileDesc, PR_CloseDelete> fd =
              OpenExistingSync(tmpPath, flags);
          if (!fd) {
            fd = CreateFileSync(tmpPath, flags);
          }
          if (!fd) {
            return IOWriteMozPromise::CreateAndReject(
                nsPrintfCString("Could not open the file at %s",
                                NS_ConvertUTF16toUTF8(tmpPath).get()),
                __func__);
          }

          nsresult rv = IOUtils::WriteSync(fd.get(), toWrite, result);
          if (NS_FAILED(rv)) {
            return IOWriteMozPromise::CreateAndReject(
                FormatErrorMessage(
                    rv,
                    "Unexpected error occurred while writing to the file at %s",
                    NS_ConvertUTF16toUTF8(tmpPath).get()),
                __func__);
          }
        }

        // The requested destination was written to, so there is nothing left to
        // do.
        if (destPath == tmpPath) {
          return IOWriteMozPromise::CreateAndResolve(result, __func__);
        }
        // Otherwise, if tmpPath was specified and different from the destPath,
        // then the operation is finished by performing a move.
        nsresult rv = MoveSync(tmpPath, destPath, false);
        if (NS_FAILED(rv)) {
          return IOWriteMozPromise::CreateAndReject(
              FormatErrorMessage(
                  rv, "Error moving temporary file at %s to destination at %s",
                  NS_ConvertUTF16toUTF8(tmpPath).get(),
                  NS_ConvertUTF16toUTF8(destPath).get()),
              __func__);
        }
        return IOWriteMozPromise::CreateAndResolve(result, __func__);
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
already_AddRefed<Promise> IOUtils::Move(GlobalObject& aGlobal,
                                        const nsAString& aSourcePath,
                                        const nsAString& aDestPath,
                                        const MoveOptions& aOptions) {
  RefPtr<Promise> promise = CreateJSPromise(aGlobal);
  NS_ENSURE_TRUE(!!promise, nullptr);
  REJECT_IF_SHUTTING_DOWN(promise);

  RefPtr<nsISerialEventTarget> bg = GetBackgroundEventTarget();
  REJECT_IF_NULL_EVENT_TARGET(bg, promise);

  // Process arguments.
  bool noOverwrite = false;
  if (aOptions.IsAnyMemberPresent()) {
    noOverwrite = aOptions.mNoOverwrite;
  }

  InvokeAsync(bg, __func__,
              [srcPathString = nsAutoString(aSourcePath),
               destPathString = nsAutoString(aDestPath), noOverwrite]() {
                nsresult rv =
                    MoveSync(srcPathString, destPathString, noOverwrite);
                if (NS_FAILED(rv)) {
                  return IOMoveMozPromise::CreateAndReject(rv, __func__);
                }
                return IOMoveMozPromise::CreateAndResolve(true, __func__);
              })
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [promise = RefPtr(promise)](const bool&) {
            AutoJSAPI jsapi;
            if (NS_WARN_IF(!jsapi.Init(promise->GetGlobalObject()))) {
              promise->MaybeReject(NS_ERROR_UNEXPECTED);
              return;
            }
            promise->MaybeResolveWithUndefined();
          },
          [promise = RefPtr(promise)](const nsresult& aError) {
            switch (aError) {
              case NS_ERROR_FILE_ACCESS_DENIED:
                promise->MaybeRejectWithInvalidAccessError("Access denied");
                break;
              case NS_ERROR_FILE_TARGET_DOES_NOT_EXIST:
              case NS_ERROR_FILE_NOT_FOUND:
                promise->MaybeRejectWithNotFoundError(
                    "Source file does not exist");
                break;
              case NS_ERROR_FILE_ALREADY_EXISTS:
                promise->MaybeRejectWithNoModificationAllowedError(
                    "Destination file exists and overwrites are not allowed");
                break;
              case NS_ERROR_FILE_READ_ONLY:
                promise->MaybeRejectWithNoModificationAllowedError(
                    "Destination is read only");
                break;
              case NS_ERROR_FILE_DESTINATION_NOT_DIR:
                promise->MaybeRejectWithInvalidAccessError(
                    "Source is a directory but destination is not");
                break;
              case NS_ERROR_FILE_UNRECOGNIZED_PATH:
                promise->MaybeRejectWithOperationError(
                    "Only absolute file paths are permitted");
                break;
              default: {
                promise->MaybeRejectWithUnknownError(
                    FormatErrorMessage(aError, "Unexpected error moving file"));
              }
            }
          });

  return promise.forget();
}

/* static */
already_AddRefed<Promise> IOUtils::Remove(GlobalObject& aGlobal,
                                          const nsAString& aPath,
                                          const RemoveOptions& aOptions) {
  RefPtr<Promise> promise = CreateJSPromise(aGlobal);
  REJECT_IF_SHUTTING_DOWN(promise);

  // Do the IO on a background thread and return the result to this thread.
  RefPtr<nsISerialEventTarget> bg = GetBackgroundEventTarget();
  REJECT_IF_NULL_EVENT_TARGET(bg, promise);

  // Process arguments.
  if (!IsAbsolutePath(aPath)) {
    promise->MaybeRejectWithOperationError(
        "Only absolute file paths are permitted");
    return promise.forget();
  }

  InvokeAsync(bg, __func__,
              [path = nsAutoString(aPath), aOptions]() {
                nsresult rv = RemoveSync(path, aOptions.mIgnoreAbsent,
                                         aOptions.mRecursive);
                if (NS_FAILED(rv)) {
                  return IORemoveMozPromise::CreateAndReject(rv, __func__);
                }
                return IORemoveMozPromise::CreateAndResolve(true, __func__);
              })
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [promise = RefPtr(promise)](const bool&) {
            AutoJSAPI jsapi;
            if (NS_WARN_IF(!jsapi.Init(promise->GetGlobalObject()))) {
              promise->MaybeReject(NS_ERROR_UNEXPECTED);
              return;
            }
            promise->MaybeResolveWithUndefined();
          },
          [promise = RefPtr(promise)](const nsresult& aError) {
            switch (aError) {
              case NS_ERROR_FILE_TARGET_DOES_NOT_EXIST:
              case NS_ERROR_FILE_NOT_FOUND:
                promise->MaybeRejectWithNotFoundError(
                    "Target file does not exist");
                break;
              case NS_ERROR_FILE_DIR_NOT_EMPTY:
                promise->MaybeRejectWithOperationError(
                    "Could not remove non-empty directory, specify the "
                    "`recursive: true` option to mitigate this error");
                break;
              default:
                promise->MaybeRejectWithUnknownError(FormatErrorMessage(
                    aError, "Unexpected error removing file"));
            }
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
    const nsAString& aPath, int32_t aFlags) {
  // Ensure that CREATE_FILE and EXCL flags were not included, as we do not
  // want to create a new file.
  MOZ_ASSERT((aFlags & (PR_CREATE_FILE | PR_EXCL)) == 0);

  // We open the file descriptor through an nsLocalFile to ensure that the paths
  // are interpreted/encoded correctly on all platforms.
  RefPtr<nsLocalFile> file = new nsLocalFile();
  nsresult rv = file->InitWithPath(aPath);
  NS_ENSURE_SUCCESS(rv, nullptr);

  PRFileDesc* fd;
  rv = file->OpenNSPRFileDesc(aFlags, /* mode */ 0, &fd);
  NS_ENSURE_SUCCESS(rv, nullptr);

  return UniquePtr<PRFileDesc, PR_CloseDelete>(fd);
}

/* static */
UniquePtr<PRFileDesc, PR_CloseDelete> IOUtils::CreateFileSync(
    const nsAString& aPath, int32_t aFlags, int32_t aMode) {
  // We open the file descriptor through an nsLocalFile to ensure that the paths
  // are interpreted/encoded correctly on all platforms.
  RefPtr<nsLocalFile> file = new nsLocalFile();
  nsresult rv = file->InitWithPath(aPath);
  NS_ENSURE_SUCCESS(rv, nullptr);

  PRFileDesc* fd;
  rv = file->OpenNSPRFileDesc(aFlags | PR_CREATE_FILE | PR_EXCL, aMode, &fd);
  NS_ENSURE_SUCCESS(rv, nullptr);

  return UniquePtr<PRFileDesc, PR_CloseDelete>(fd);
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

/* static */
nsresult IOUtils::MoveSync(const nsAString& aSourcePath,
                           const nsAString& aDestPath, bool noOverwrite) {
  MOZ_ASSERT(!NS_IsMainThread());
  nsresult rv = NS_OK;

  nsCOMPtr<nsIFile> srcFile = new nsLocalFile();
  MOZ_TRY(srcFile->InitWithPath(aSourcePath));  // Fails if not absolute.
  MOZ_TRY(srcFile->Normalize());                // Fails if path does not exist.

  nsCOMPtr<nsIFile> destFile = new nsLocalFile();
  MOZ_TRY(destFile->InitWithPath(aDestPath));
  rv = destFile->Normalize();
  // Normalize can fail for a number of reasons, including if the file doesn't
  // exist. It is expected that the file might not exist for a number of calls
  // (e.g. if we want to rename a file to a new location).
  if (!IsFileNotFound(rv)) {  // Deliberately ignore "not found" errors.
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Case 1: Destination is an existing directory. Move source into dest.
  bool destIsDir = false;
  bool destExists = true;

  rv = destFile->IsDirectory(&destIsDir);
  if (NS_SUCCEEDED(rv) && destIsDir) {
    return srcFile->MoveTo(destFile, EmptyString());
  }
  if (IsFileNotFound(rv)) {
    // It's ok if the file doesn't exist. Case 2 handles this below.
    destExists = false;
  } else {
    // Bail out early for any other kind of error though.
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Case 2: Destination is a file which may or may not exist. Try to rename the
  //         source to the destination. This will fail if the source is a not a
  //         regular file.
  if (noOverwrite && destExists) {
    return NS_ERROR_FILE_ALREADY_EXISTS;
  }
  if (destExists && !destIsDir) {
    // If the source file is a directory, but the target is a file, abort early.
    // Different implementations of |MoveTo| seem to handle this error case
    // differently (or not at all), so we explicitly handle it here.
    bool srcIsDir = false;
    MOZ_TRY(srcFile->IsDirectory(&srcIsDir));
    if (srcIsDir) {
      return NS_ERROR_FILE_DESTINATION_NOT_DIR;
    }
  }

  nsCOMPtr<nsIFile> destDir;
  nsAutoString destName;
  MOZ_TRY(destFile->GetLeafName(destName));
  MOZ_TRY(destFile->GetParent(getter_AddRefs(destDir)));

  // NB: if destDir doesn't exist, then MoveTo will create it.
  return srcFile->MoveTo(destDir, destName);
}

/* static */
nsresult IOUtils::RemoveSync(const nsAString& aPath, bool aIgnoreAbsent,
                             bool aRecursive) {
  RefPtr<nsLocalFile> file = new nsLocalFile();
  MOZ_TRY(file->InitWithPath(aPath));

  nsresult rv = file->Remove(aRecursive);
  if (aIgnoreAbsent && IsFileNotFound(rv)) {
    return NS_OK;
  }
  return rv;
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
        nsresult rv = NS_DispatchToMainThread(mainThreadRunnable.forget());
        NS_ENSURE_SUCCESS_VOID(rv);
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

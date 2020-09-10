/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include "ErrorList.h"
#include "mozilla/dom/IOUtils.h"
#include "mozilla/dom/IOUtilsBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/TextUtils.h"
#include "nsIFile.h"
#include "nsIGlobalObject.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsThreadManager.h"
#include "prerror.h"
#include "prio.h"
#include "private/pprio.h"
#include "prtypes.h"

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

#define REJECT_IF_RELATIVE_PATH(aPath, aJSPromise)                    \
  do {                                                                \
    if (!IsAbsolutePath(aPath)) {                                     \
      (aJSPromise)                                                    \
          ->MaybeRejectWithOperationError(nsPrintfCString(            \
              "Refusing to work with path(%s) because only absolute " \
              "file paths are permitted",                             \
              NS_ConvertUTF16toUTF8(aPath).get()));                   \
      return (aJSPromise).forget();                                   \
    }                                                                 \
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

/**
 * Unwraps |aResult| into a |MozPromise|.
 *
 * If the result is an error, a new MozPromise is created and immediately
 * rejected with the unwrapped error. Otherwise, if the result is ok, a new
 * MozPromise is created and immediately resolved with the unwrapped result.
 */
template <class PromiseT, class OkT, class ErrT>
static RefPtr<PromiseT> ToMozPromise(Result<OkT, ErrT>& aResult,
                                     const char* aCallSite) {
  if (aResult.isErr()) {
    return PromiseT::CreateAndReject(aResult.unwrapErr(), aCallSite);
  }
  return PromiseT::CreateAndResolve(aResult.unwrap(), aCallSite);
}

MOZ_MUST_USE inline bool ToJSValue(
    JSContext* aCx, const IOUtils::InternalFileInfo& aInternalFileInfo,
    JS::MutableHandle<JS::Value> aValue) {
  FileInfo info;
  info.mPath.Construct(aInternalFileInfo.mPath);
  info.mType.Construct(aInternalFileInfo.mType);
  info.mSize.Construct(aInternalFileInfo.mSize);
  info.mLastModified.Construct(aInternalFileInfo.mLastModified);
  return ToJSValue(aCx, info, aValue);
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

template <typename MozPromiseT, typename Fn, typename... Args>
static RefPtr<MozPromiseT> InvokeToMozPromise(Fn aFunc, Args... aArgs) {
  MOZ_ASSERT(!NS_IsMainThread());
  auto rv = aFunc(std::forward<Args>(aArgs)...);
  if (rv.isErr()) {
    return MozPromiseT::CreateAndReject(rv.unwrapErr(), __func__);
  }
  return MozPromiseT::CreateAndResolve(rv.unwrap(), __func__);
}

/* static */
template <typename OkT, typename Fn, typename... Args>
already_AddRefed<Promise> IOUtils::RunOnBackgroundThread(
    RefPtr<Promise>& aPromise, Fn aFunc, Args... aArgs) {
  RefPtr<nsISerialEventTarget> bg = GetBackgroundEventTarget();
  REJECT_IF_NULL_EVENT_TARGET(bg, aPromise);

  InvokeAsync(
      bg, __func__,
      [fn = aFunc, argsTuple = std::make_tuple(std::move(aArgs)...)]() mutable {
        return std::apply(
            [fn](Args... args) mutable {
              using MozPromiseT = MozPromise<OkT, IOError, true>;
              return InvokeToMozPromise<MozPromiseT>(
                  fn, std::forward<Args>(args)...);
            },
            std::move(argsTuple));
      })
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [promise = RefPtr(aPromise)](const OkT& ok) {
            if constexpr (std::is_same_v<OkT, nsTArray<uint8_t>>) {
              TypedArrayCreator<Uint8Array> arr(ok);
              promise->MaybeResolve(arr);
            } else if constexpr (std::is_same_v<OkT, Ok>) {
              promise->MaybeResolveWithUndefined();
            } else {
              promise->MaybeResolve(ok);
            }
          },
          [promise = RefPtr(aPromise)](const IOError& err) {
            RejectJSPromise(promise, err);
          });
  return aPromise.forget();
}

/* static */
already_AddRefed<Promise> IOUtils::Read(GlobalObject& aGlobal,
                                        const nsAString& aPath,
                                        const Optional<uint32_t>& aMaxBytes) {
  RefPtr<Promise> promise = CreateJSPromise(aGlobal);
  NS_ENSURE_TRUE(!!promise, nullptr);
  REJECT_IF_SHUTTING_DOWN(promise);

  // Process arguments.
  REJECT_IF_RELATIVE_PATH(aPath, promise);
  nsAutoString path(aPath);
  Maybe<uint32_t> toRead = Nothing();
  if (aMaxBytes.WasPassed()) {
    if (aMaxBytes.Value() == 0) {
      // Resolve with an empty buffer.
      nsTArray<uint8_t> arr(0);
      promise->MaybeResolve(TypedArrayCreator<Uint8Array>(arr));
      return promise.forget();
    }
    toRead.emplace(aMaxBytes.Value());
  }

  return RunOnBackgroundThread<nsTArray<uint8_t>>(promise, &ReadSync, path,
                                                  toRead);
}

/* static */
already_AddRefed<Promise> IOUtils::WriteAtomic(
    GlobalObject& aGlobal, const nsAString& aPath, const Uint8Array& aData,
    const WriteAtomicOptions& aOptions) {
  RefPtr<Promise> promise = CreateJSPromise(aGlobal);
  NS_ENSURE_TRUE(!!promise, nullptr);
  REJECT_IF_SHUTTING_DOWN(promise);

  // Process arguments.
  REJECT_IF_RELATIVE_PATH(aPath, promise);
  aData.ComputeState();
  auto buf = Buffer<uint8_t>::CopyFrom(Span(aData.Data(), aData.Length()));
  if (buf.isNothing()) {
    promise->MaybeRejectWithOperationError("Out of memory");
    return promise.forget();
  }
  nsAutoString destPath(aPath);
  InternalWriteAtomicOpts opts;
  opts.mFlush = aOptions.mFlush;
  opts.mNoOverwrite = aOptions.mNoOverwrite;
  if (aOptions.mBackupFile.WasPassed()) {
    opts.mBackupFile.emplace(aOptions.mBackupFile.Value());
  }
  if (aOptions.mTmpPath.WasPassed()) {
    opts.mTmpPath.emplace(aOptions.mTmpPath.Value());
  }

  return RunOnBackgroundThread<uint32_t>(promise, &WriteAtomicSync, destPath,
                                         std::move(*buf), std::move(opts));
}

/* static */
already_AddRefed<Promise> IOUtils::Move(GlobalObject& aGlobal,
                                        const nsAString& aSourcePath,
                                        const nsAString& aDestPath,
                                        const MoveOptions& aOptions) {
  RefPtr<Promise> promise = CreateJSPromise(aGlobal);
  NS_ENSURE_TRUE(!!promise, nullptr);
  REJECT_IF_SHUTTING_DOWN(promise);

  // Process arguments.
  REJECT_IF_RELATIVE_PATH(aSourcePath, promise);
  REJECT_IF_RELATIVE_PATH(aDestPath, promise);
  nsAutoString sourcePath(aSourcePath);
  nsAutoString destPath(aDestPath);

  return RunOnBackgroundThread<Ok>(promise, &MoveSync, sourcePath, destPath,
                                   aOptions.mNoOverwrite);
}

/* static */
already_AddRefed<Promise> IOUtils::Remove(GlobalObject& aGlobal,
                                          const nsAString& aPath,
                                          const RemoveOptions& aOptions) {
  RefPtr<Promise> promise = CreateJSPromise(aGlobal);
  NS_ENSURE_TRUE(!!promise, nullptr);
  REJECT_IF_SHUTTING_DOWN(promise);

  REJECT_IF_RELATIVE_PATH(aPath, promise);
  nsAutoString path(aPath);

  return RunOnBackgroundThread<Ok>(promise, &RemoveSync, path,
                                   aOptions.mIgnoreAbsent, aOptions.mRecursive);
}

/* static */
already_AddRefed<Promise> IOUtils::MakeDirectory(
    GlobalObject& aGlobal, const nsAString& aPath,
    const MakeDirectoryOptions& aOptions) {
  RefPtr<Promise> promise = CreateJSPromise(aGlobal);
  NS_ENSURE_TRUE(!!promise, nullptr);
  REJECT_IF_SHUTTING_DOWN(promise);

  REJECT_IF_RELATIVE_PATH(aPath, promise);
  nsAutoString path(aPath);

  return RunOnBackgroundThread<Ok>(promise, &CreateDirectorySync, path,
                                   aOptions.mCreateAncestors,
                                   aOptions.mIgnoreExisting, 0777);
}

already_AddRefed<Promise> IOUtils::Stat(GlobalObject& aGlobal,
                                        const nsAString& aPath) {
  RefPtr<Promise> promise = CreateJSPromise(aGlobal);
  NS_ENSURE_TRUE(!!promise, nullptr);
  REJECT_IF_SHUTTING_DOWN(promise);

  REJECT_IF_RELATIVE_PATH(aPath, promise);
  nsAutoString path(aPath);

  return RunOnBackgroundThread<InternalFileInfo>(promise, &StatSync, path);
}

/* static */
already_AddRefed<Promise> IOUtils::Copy(GlobalObject& aGlobal,
                                        const nsAString& aSourcePath,
                                        const nsAString& aDestPath,
                                        const CopyOptions& aOptions) {
  RefPtr<Promise> promise = CreateJSPromise(aGlobal);
  NS_ENSURE_TRUE(!!promise, nullptr);
  REJECT_IF_SHUTTING_DOWN(promise);

  // Process arguments.
  REJECT_IF_RELATIVE_PATH(aSourcePath, promise);
  REJECT_IF_RELATIVE_PATH(aDestPath, promise);
  nsAutoString sourcePath(aSourcePath);
  nsAutoString destPath(aDestPath);

  return RunOnBackgroundThread<Ok>(promise, &CopySync, sourcePath, destPath,
                                   aOptions.mNoOverwrite, aOptions.mRecursive);
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
void IOUtils::RejectJSPromise(const RefPtr<Promise>& aPromise,
                              const IOError& aError) {
  const auto& errMsg = aError.Message();

  switch (aError.Code()) {
    case NS_ERROR_FILE_TARGET_DOES_NOT_EXIST:
    case NS_ERROR_FILE_NOT_FOUND:
      aPromise->MaybeRejectWithNotFoundError(errMsg.refOr("File not found"_ns));
      break;
    case NS_ERROR_FILE_ACCESS_DENIED:
      aPromise->MaybeRejectWithNotAllowedError(
          errMsg.refOr("Access was denied to the target file"_ns));
      break;
    case NS_ERROR_FILE_TOO_BIG:
      aPromise->MaybeRejectWithNotReadableError(
          errMsg.refOr("Target file is too big"_ns));
      break;
    case NS_ERROR_FILE_ALREADY_EXISTS:
      aPromise->MaybeRejectWithNoModificationAllowedError(
          errMsg.refOr("Target file already exists"_ns));
      break;
    case NS_ERROR_FILE_COPY_OR_MOVE_FAILED:
      aPromise->MaybeRejectWithOperationError(
          errMsg.refOr("Failed to copy or move the target file"_ns));
      break;
    case NS_ERROR_FILE_READ_ONLY:
      aPromise->MaybeRejectWithReadOnlyError(
          errMsg.refOr("Target file is read only"_ns));
      break;
    case NS_ERROR_FILE_NOT_DIRECTORY:
    case NS_ERROR_FILE_DESTINATION_NOT_DIR:
      aPromise->MaybeRejectWithInvalidAccessError(
          errMsg.refOr("Target file is not a directory"_ns));
      break;
    case NS_ERROR_FILE_UNRECOGNIZED_PATH:
      aPromise->MaybeRejectWithOperationError(
          errMsg.refOr("Target file path is not recognized"_ns));
      break;
    case NS_ERROR_FILE_DIR_NOT_EMPTY:
      aPromise->MaybeRejectWithOperationError(
          errMsg.refOr("Target directory is not empty"_ns));
      break;
    default:
      aPromise->MaybeRejectWithUnknownError(
          errMsg.refOr(FormatErrorMessage(aError.Code(), "Unexpected error")));
  }
}

/* static */
UniquePtr<PRFileDesc, PR_CloseDelete> IOUtils::OpenExistingSync(
    const nsAString& aPath, int32_t aFlags) {
  MOZ_ASSERT(!NS_IsMainThread());
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
  MOZ_ASSERT(!NS_IsMainThread());
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
Result<nsTArray<uint8_t>, IOUtils::IOError> IOUtils::ReadSync(
    const nsAString& aPath, const Maybe<uint32_t>& aMaxBytes) {
  MOZ_ASSERT(!NS_IsMainThread());

  UniquePtr<PRFileDesc, PR_CloseDelete> fd = OpenExistingSync(aPath, PR_RDONLY);
  if (!fd) {
    return Err(IOError(NS_ERROR_FILE_NOT_FOUND)
                   .WithMessage("Could not open the file at %s",
                                NS_ConvertUTF16toUTF8(aPath).get()));
  }
  uint32_t bufSize;
  if (aMaxBytes.isNothing()) {
    // Limitation: We cannot read files that are larger than the max size of a
    //             TypedArray (UINT32_MAX bytes). Reject if the file is too
    //             big to be read.
    PRFileInfo64 info;
    if (PR_FAILURE == PR_GetOpenFileInfo64(fd.get(), &info)) {
      return Err(IOError(NS_ERROR_FILE_ACCESS_DENIED)
                     .WithMessage("Could not get info for the file at %s",
                                  NS_ConvertUTF16toUTF8(aPath).get()));
    }
    if (static_cast<uint64_t>(info.size) > UINT32_MAX) {
      return Err(
          IOError(NS_ERROR_FILE_TOO_BIG)
              .WithMessage("Could not read the file at %s because it is too "
                           "large(size=%" PRIu64 " bytes)",
                           NS_ConvertUTF16toUTF8(aPath).get(),
                           static_cast<uint64_t>(info.size)));
    }
    bufSize = static_cast<uint32_t>(info.size);
  } else {
    bufSize = aMaxBytes.value();
  }

  nsTArray<uint8_t> buffer;
  if (!buffer.SetCapacity(bufSize, fallible)) {
    return Err(IOError(NS_ERROR_OUT_OF_MEMORY));
  }

  // If possible, advise the operating system that we will be reading the file
  // pointed to by |fd| sequentially, in full. This advice is not binding, it
  // informs the OS about our expectations as an application.
#if defined(HAVE_POSIX_FADVISE)
  posix_fadvise(PR_FileDesc2NativeHandle(fd.get()), 0, 0,
                POSIX_FADV_SEQUENTIAL);
#endif

  uint32_t totalRead = 0;
  while (totalRead != bufSize) {
    int32_t nRead =
        PR_Read(fd.get(), buffer.Elements() + totalRead, bufSize - totalRead);
    if (nRead == 0) {
      break;
    }
    if (nRead < 0) {
      PRErrorCode errNo = PR_GetError();
      const char* errName = PR_ErrorToName(errNo);
      return Err(
          IOError(NS_ERROR_UNEXPECTED)
              .WithMessage(
                  "Encountered an unexpected error while reading file(%s)"
                  "(PRErrorCode: %s)",
                  NS_ConvertUTF16toUTF8(aPath).get(), errName));
    }
    totalRead += nRead;
    DebugOnly<bool> success = buffer.SetLength(totalRead, fallible);
    MOZ_ASSERT(success);
  }
  return std::move(buffer);
}

/* static */
Result<uint32_t, IOUtils::IOError> IOUtils::WriteAtomicSync(
    const nsAString& aDestPath, const Buffer<uint8_t>& aByteArray,
    const IOUtils::InternalWriteAtomicOpts& aOptions) {
  MOZ_ASSERT(!NS_IsMainThread());

  // Check if the file exists and test it against the noOverwrite option.
  const bool& noOverwrite = aOptions.mNoOverwrite;
  bool exists = false;
  {
    UniquePtr<PRFileDesc, PR_CloseDelete> fd =
        OpenExistingSync(aDestPath, PR_RDONLY);
    exists = !!fd;
  }

  if (noOverwrite && exists) {
    return Err(IOError(NS_ERROR_FILE_ALREADY_EXISTS)
                   .WithMessage("Refusing to overwrite the file at %s\n"
                                "Specify `noOverwrite: false` to allow "
                                "overwriting the destination",
                                NS_ConvertUTF16toUTF8(aDestPath).get()));
  }

  // If backupFile was specified, perform the backup as a move.
  if (exists && aOptions.mBackupFile.isSome() &&
      MoveSync(aDestPath, aOptions.mBackupFile.value(), noOverwrite).isErr()) {
    return Err(
        IOError(NS_ERROR_FILE_COPY_OR_MOVE_FAILED)
            .WithMessage(
                "Failed to backup the source file(%s) to %s",
                NS_ConvertUTF16toUTF8(aDestPath).get(),
                NS_ConvertUTF16toUTF8(aOptions.mBackupFile.value()).get()));
  }

  // If tmpPath was specified, we will write to there first, then perform
  // a move to ensure the file ends up at the final requested destination.
  nsAutoString tmpPath;
  if (aOptions.mTmpPath.isSome()) {
    tmpPath = aOptions.mTmpPath.value();
  } else {
    tmpPath = aDestPath;
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
    UniquePtr<PRFileDesc, PR_CloseDelete> fd = OpenExistingSync(tmpPath, flags);
    if (!fd) {
      fd = CreateFileSync(tmpPath, flags);
    }
    if (!fd) {
      return Err(IOError(NS_ERROR_FILE_ACCESS_DENIED)
                     .WithMessage("Could not open the file at %s for writing",
                                  NS_ConvertUTF16toUTF8(tmpPath).get()));
    }

    auto rv = WriteSync(fd.get(), NS_ConvertUTF16toUTF8(tmpPath), aByteArray);
    if (rv.isErr()) {
      return rv.propagateErr();
    }
    result = rv.unwrap();
  }

  // If tmpPath was specified and different from the destPath, then the
  // operation is finished by performing a move.
  if (aDestPath != tmpPath && MoveSync(tmpPath, aDestPath, false).isErr()) {
    return Err(
        IOError(NS_ERROR_FILE_COPY_OR_MOVE_FAILED)
            .WithMessage("Could not move temporary file(%s) to destination(%s)",
                         NS_ConvertUTF16toUTF8(tmpPath).get(),
                         NS_ConvertUTF16toUTF8(aDestPath).get()));
  }
  return result;
}

/* static */
Result<uint32_t, IOUtils::IOError> IOUtils::WriteSync(
    PRFileDesc* aFd, const nsACString& aPath, const Buffer<uint8_t>& aBytes) {
  // aBytes comes from a JavaScript TypedArray, which has UINT32_MAX max
  // length.
  MOZ_ASSERT(aBytes.Length() <= UINT32_MAX);
  MOZ_ASSERT(!NS_IsMainThread());

  if (aBytes.Length() == 0) {
    return 0;
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
    int32_t rv = PR_Write(aFd, aBytes.begin() + bytesWritten, chunkSize);
    if (rv < 0) {
      return Err(IOError(NS_ERROR_FILE_CORRUPTED)
                     .WithMessage("Could not write chunk(size=%" PRIu32
                                  ") to %s\n"
                                  "The file may be corrupt",
                                  chunkSize, aPath.BeginReading()));
    }
    pendingBytes -= rv;
    bytesWritten += rv;
  }

  return bytesWritten;
}

/* static */
Result<Ok, IOUtils::IOError> IOUtils::MoveSync(const nsAString& aSourcePath,
                                               const nsAString& aDestPath,
                                               bool aNoOverwrite) {
  MOZ_ASSERT(!NS_IsMainThread());

  RefPtr<nsLocalFile> srcFile = new nsLocalFile();
  MOZ_TRY(srcFile->InitWithPath(aSourcePath));  // Fails if not absolute.

  // Ensure the source file exists before continuing. If it doesn't exist,
  // subsequent operations can fail in different ways on different platforms.
  bool srcExists;
  MOZ_TRY(srcFile->Exists(&srcExists));
  if (!srcExists) {
    return Err(
        IOError(NS_ERROR_FILE_NOT_FOUND)
            .WithMessage(
                "Could not move source file(%s) because it does not exist",
                NS_ConvertUTF16toUTF8(aSourcePath).get()));
  }

  RefPtr<nsLocalFile> destFile = new nsLocalFile();
  MOZ_TRY(destFile->InitWithPath(aDestPath));  // Fails if not absolute.

  return CopyOrMoveSync(&nsLocalFile::MoveTo, "move", srcFile, destFile,
                        aNoOverwrite);
}

/* static */
Result<Ok, IOUtils::IOError> IOUtils::CopySync(const nsAString& aSourcePath,
                                               const nsAString& aDestPath,
                                               bool aNoOverwrite,
                                               bool aRecursive) {
  MOZ_ASSERT(!NS_IsMainThread());

  RefPtr<nsLocalFile> srcFile = new nsLocalFile();
  MOZ_TRY(srcFile->InitWithPath(aSourcePath));  // Fails if not absolute.

  // Ensure the source file exists before continuing. If it doesn't exist,
  // subsequent operations can fail in different ways on different platforms.
  bool srcExists;
  MOZ_TRY(srcFile->Exists(&srcExists));
  if (!srcExists) {
    return Err(
        IOError(NS_ERROR_FILE_NOT_FOUND)
            .WithMessage(
                "Could not copy source file(%s) because it does not exist",
                NS_ConvertUTF16toUTF8(aSourcePath).get()));
  }

  // If source is a directory, fail immediately unless the recursive option is
  // true.
  bool srcIsDir = false;
  MOZ_TRY(srcFile->IsDirectory(&srcIsDir));
  if (srcIsDir && !aRecursive) {
    return Err(
        IOError(NS_ERROR_FILE_COPY_OR_MOVE_FAILED)
            .WithMessage(
                "Refused to copy source directory(%s) to the destination(%s)\n"
                "Specify the `recursive: true` option to allow copying "
                "directories",
                NS_ConvertUTF16toUTF8(aSourcePath).get(),
                NS_ConvertUTF16toUTF8(aDestPath).get()));
  }

  RefPtr<nsLocalFile> destFile = new nsLocalFile();
  MOZ_TRY(destFile->InitWithPath(aDestPath));  // Fails if not absolute.

  return CopyOrMoveSync(&nsLocalFile::CopyTo, "copy", srcFile, destFile,
                        aNoOverwrite);
}

/* static */
template <typename CopyOrMoveFn>
Result<Ok, IOUtils::IOError> IOUtils::CopyOrMoveSync(
    CopyOrMoveFn aMethod, const char* aMethodName,
    const RefPtr<nsLocalFile>& aSource, const RefPtr<nsLocalFile>& aDest,
    bool aNoOverwrite) {
  nsresult rv = NS_OK;

  // Normalize the file paths.
  MOZ_TRY(aSource->Normalize());
  rv = aDest->Normalize();
  // Normalize can fail for a number of reasons, including if the file doesn't
  // exist. It is expected that the file might not exist for a number of calls
  // (e.g. if we want to copy or move a file to a new location).
  if (NS_FAILED(rv) && !IsFileNotFound(rv)) {
    // Deliberately ignore "not found" errors, but propagate all others.
    return Err(IOError(rv));
  }

  nsAutoString src;
  MOZ_TRY(aSource->GetPath(src));
  auto sourcePath = NS_ConvertUTF16toUTF8(src);
  nsAutoString dest;
  MOZ_TRY(aDest->GetPath(dest));
  auto destPath = NS_ConvertUTF16toUTF8(dest);

  // Case 1: Destination is an existing directory. Copy/move source into dest.
  bool destIsDir = false;
  bool destExists = true;

  rv = aDest->IsDirectory(&destIsDir);
  if (NS_SUCCEEDED(rv) && destIsDir) {
    rv = (aSource->*aMethod)(aDest, EmptyString());
    if (NS_FAILED(rv)) {
      return Err(IOError(rv).WithMessage(
          "Could not %s source file(%s) to destination directory(%s)",
          aMethodName, sourcePath.get(), destPath.get()));
    }
    return Ok();
  }

  if (NS_FAILED(rv)) {
    if (!IsFileNotFound(rv)) {
      // It's ok if the dest file doesn't exist. Case 2 handles this below.
      // Bail out early for any other kind of error though.
      return Err(IOError(rv));
    }
    destExists = false;
  }

  // Case 2: Destination is a file which may or may not exist.
  //         Try to copy or rename the source to the destination.
  //         If the destination exists and the source is not a regular file,
  //         then this may fail.
  if (aNoOverwrite && destExists) {
    return Err(
        IOError(NS_ERROR_FILE_ALREADY_EXISTS)
            .WithMessage(
                "Could not %s source file(%s) to destination(%s) because the "
                "destination already exists and overwrites are not allowed\n"
                "Specify the `noOverwrite: false` option to mitigate this "
                "error",
                aMethodName, sourcePath.get(), destPath.get()));
  }
  if (destExists && !destIsDir) {
    // If the source file is a directory, but the target is a file, abort early.
    // Different implementations of |CopyTo| and |MoveTo| seem to handle this
    // error case differently (or not at all), so we explicitly handle it here.
    bool srcIsDir = false;
    MOZ_TRY(aSource->IsDirectory(&srcIsDir));
    if (srcIsDir) {
      return Err(IOError(NS_ERROR_FILE_DESTINATION_NOT_DIR)
                     .WithMessage("Could not %s the source directory(%s) to "
                                  "the destination(%s) because the destination "
                                  "is not a directory",
                                  aMethodName, sourcePath.get(),
                                  destPath.get()));
    }
  }

  nsCOMPtr<nsIFile> destDir;
  nsAutoString destName;
  MOZ_TRY(aDest->GetLeafName(destName));
  MOZ_TRY(aDest->GetParent(getter_AddRefs(destDir)));

  // NB: if destDir doesn't exist, then |CopyTo| or |MoveTo| will create it.
  rv = (aSource->*aMethod)(destDir, destName);
  if (NS_FAILED(rv)) {
    return Err(IOError(rv).WithMessage(
        "Could not %s the source file(%s) to the destination(%s)", aMethodName,
        sourcePath.get(), destPath.get()));
  }
  return Ok();
}

/* static */
Result<Ok, IOUtils::IOError> IOUtils::RemoveSync(const nsAString& aPath,
                                                 bool aIgnoreAbsent,
                                                 bool aRecursive) {
  MOZ_ASSERT(!NS_IsMainThread());

  RefPtr<nsLocalFile> file = new nsLocalFile();
  MOZ_TRY(file->InitWithPath(aPath));

  nsresult rv = file->Remove(aRecursive);
  if (aIgnoreAbsent && IsFileNotFound(rv)) {
    return Ok();
  }
  if (NS_FAILED(rv)) {
    IOError err(rv);
    if (IsFileNotFound(rv)) {
      return Err(err.WithMessage(
          "Could not remove the file at %s because it does not exist.\n"
          "Specify the `ignoreAbsent: true` option to mitigate this error",
          NS_ConvertUTF16toUTF8(aPath).get()));
    }
    if (rv == NS_ERROR_FILE_DIR_NOT_EMPTY) {
      return Err(err.WithMessage(
          "Could not remove the non-empty directory at %s.\n"
          "Specify the `recursive: true` option to mitigate this error",
          NS_ConvertUTF16toUTF8(aPath).get()));
    }
    return Err(err.WithMessage("Could not remove the file at %s",
                               NS_ConvertUTF16toUTF8(aPath).get()));
  }
  return Ok();
}

/* static */
Result<Ok, IOUtils::IOError> IOUtils::CreateDirectorySync(
    const nsAString& aPath, bool aCreateAncestors, bool aIgnoreExisting,
    int32_t aMode) {
  MOZ_ASSERT(!NS_IsMainThread());

  RefPtr<nsLocalFile> targetFile = new nsLocalFile();
  MOZ_TRY(targetFile->InitWithPath(aPath));

  // nsIFile::Create will create ancestor directories by default.
  // If the caller does not want this behaviour, then check and possibly
  // return an error.
  if (!aCreateAncestors) {
    nsCOMPtr<nsIFile> parent;
    MOZ_TRY(targetFile->GetParent(getter_AddRefs(parent)));
    bool parentExists;
    MOZ_TRY(parent->Exists(&parentExists));
    if (!parentExists) {
      return Err(IOError(NS_ERROR_FILE_NOT_FOUND)
                     .WithMessage("Could not create directory at %s because "
                                  "the path has missing "
                                  "ancestor components",
                                  NS_ConvertUTF16toUTF8(aPath).get()));
    }
  }

  nsresult rv = targetFile->Create(nsIFile::DIRECTORY_TYPE, aMode);
  if (NS_FAILED(rv)) {
    IOError err(rv);
    if (rv == NS_ERROR_FILE_ALREADY_EXISTS) {
      // NB: We may report a success only if the target is an existing
      // directory. We don't want to silence errors that occur if the target is
      // an existing file, since trying to create a directory where a regular
      // file exists may be indicative of a logic error.
      bool isDirectory;
      MOZ_TRY(targetFile->IsDirectory(&isDirectory));
      if (!isDirectory) {
        return Err(IOError(NS_ERROR_FILE_NOT_DIRECTORY)
                       .WithMessage("Could not create directory because the "
                                    "target file(%s) exists "
                                    "and is not a directory",
                                    NS_ConvertUTF16toUTF8(aPath).get()));
      }
      // The directory exists.
      // The caller may suppress this error.
      if (aIgnoreExisting) {
        return Ok();
      }
      // Otherwise, forward it.
      return Err(err.WithMessage(
          "Could not create directory because it already exists at %s\n"
          "Specify the `ignoreExisting: true` option to mitigate this "
          "error",
          NS_ConvertUTF16toUTF8(aPath).get()));
    }
    return Err(err.WithMessage("Could not create directory at %s",
                               NS_ConvertUTF16toUTF8(aPath).get()));
  }
  return Ok();
}

Result<IOUtils::InternalFileInfo, IOUtils::IOError> IOUtils::StatSync(
    const nsAString& aPath) {
  MOZ_ASSERT(!NS_IsMainThread());

  RefPtr<nsLocalFile> file = new nsLocalFile();
  MOZ_TRY(file->InitWithPath(aPath));

  InternalFileInfo info;
  info.mPath = nsString(aPath);

  bool isRegular;
  // IsFile will stat and cache info in the file object. If the file doesn't
  // exist, or there is an access error, we'll discover it here.
  // Any subsequent errors are unexpected and will just be forwarded.
  nsresult rv = file->IsFile(&isRegular);
  if (NS_FAILED(rv)) {
    IOError err(rv);
    if (IsFileNotFound(rv)) {
      return Err(
          err.WithMessage("Could not stat file(%s) because it does not exist",
                          NS_ConvertUTF16toUTF8(aPath).get()));
    }
    return Err(err);
  }

  // Now we can populate the info object by querying the file.
  info.mType = FileType::Regular;
  if (!isRegular) {
    bool isDir;
    MOZ_TRY(file->IsDirectory(&isDir));
    if (isDir) {
      info.mType = FileType::Directory;
    } else {
      info.mType = FileType::Other;
    }
  }

  int64_t size = -1;
  if (info.mType == FileType::Regular) {
    MOZ_TRY(file->GetFileSize(&size));
  }
  info.mSize = size;
  PRTime lastModified = 0;
  MOZ_TRY(file->GetLastModifiedTime(&lastModified));
  info.mLastModified = static_cast<int64_t>(lastModified);

  return info;
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
#undef REJECT_IF_RELATIVE_PATH

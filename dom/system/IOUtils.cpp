/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IOUtils.h"

#include <cstdint>

#include "ErrorList.h"
#include "mozilla/Compression.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Services.h"
#include "mozilla/Span.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TextUtils.h"
#include "mozilla/dom/IOUtilsBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/Unused.h"
#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsFileStreams.h"
#include "nsIDirectoryEnumerator.h"
#include "nsIFile.h"
#include "nsIGlobalObject.h"
#include "nsLocalFile.h"
#include "nsPrintfCString.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nsThreadManager.h"
#include "nsXULAppAPI.h"
#include "prerror.h"
#include "prio.h"
#include "prtime.h"
#include "prtypes.h"

#define REJECT_IF_SHUTTING_DOWN(aJSPromise)                       \
  do {                                                            \
    if (sShutdownStarted) {                                       \
      (aJSPromise)                                                \
          ->MaybeRejectWithNotAllowedError(                       \
              "Shutting down and refusing additional I/O tasks"); \
      return (aJSPromise).forget();                               \
    }                                                             \
  } while (false)

#define REJECT_IF_INIT_PATH_FAILED(_file, _path, _promise)               \
  do {                                                                   \
    if (nsresult _rv = (_file)->InitWithPath((_path)); NS_FAILED(_rv)) { \
      (_promise)->MaybeRejectWithOperationError(                         \
          FormatErrorMessage(_rv, "Could not parse path (%s)",           \
                             NS_ConvertUTF16toUTF8(_path).get()));       \
      return (_promise).forget();                                        \
    }                                                                    \
  } while (0)

namespace mozilla::dom {

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
  return aResult == NS_ERROR_FILE_NOT_FOUND ||
         aResult == NS_ERROR_FILE_TARGET_DOES_NOT_EXIST;
}
/**
 * Like |IsFileNotFound|, but checks for known results that suggest a file
 * is not a directory.
 */
static bool IsNotDirectory(nsresult aResult) {
  return aResult == NS_ERROR_FILE_DESTINATION_NOT_DIR ||
         aResult == NS_ERROR_FILE_NOT_DIRECTORY;
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

MOZ_MUST_USE inline bool ToJSValue(
    JSContext* aCx, const IOUtils::InternalFileInfo& aInternalFileInfo,
    JS::MutableHandle<JS::Value> aValue) {
  FileInfo info;
  info.mPath.Construct(aInternalFileInfo.mPath);
  info.mType.Construct(aInternalFileInfo.mType);
  info.mSize.Construct(aInternalFileInfo.mSize);
  info.mLastModified.Construct(aInternalFileInfo.mLastModified);

  if (aInternalFileInfo.mCreationTime.isSome()) {
    info.mCreationTime.Construct(aInternalFileInfo.mCreationTime.ref());
  }

  info.mPermissions.Construct(aInternalFileInfo.mPermissions);

  return ToJSValue(aCx, info, aValue);
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
template <typename OkT, typename Fn>
void IOUtils::RunOnBackgroundThread(Promise* aPromise, Fn aFunc) {
  nsCOMPtr<nsISerialEventTarget> bg = GetBackgroundEventTarget();
  if (!bg) {
    aPromise->MaybeRejectWithAbortError(
        "Could not dispatch task to background thread");
    return;
  }

  InvokeAsync(
      bg, __func__,
      [func = std::move(aFunc)]() {
        Result<OkT, IOError> result = func();
        if (result.isErr()) {
          return IOPromise<OkT>::CreateAndReject(result.unwrapErr(), __func__);
        }
        return IOPromise<OkT>::CreateAndResolve(result.unwrap(), __func__);
      })
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [promise = RefPtr(aPromise)](const OkT& ok) {
            ResolveJSPromise(promise, ok);
          },
          [promise = RefPtr(aPromise)](const IOError& err) {
            RejectJSPromise(promise, err);
          });
}

/* static */
already_AddRefed<Promise> IOUtils::Read(GlobalObject& aGlobal,
                                        const nsAString& aPath,
                                        const ReadOptions& aOptions) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
  RefPtr<Promise> promise = CreateJSPromise(aGlobal);
  NS_ENSURE_TRUE(!!promise, nullptr);
  REJECT_IF_SHUTTING_DOWN(promise);

  nsCOMPtr<nsIFile> file = new nsLocalFile();
  REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);

  Maybe<uint32_t> toRead = Nothing();
  if (!aOptions.mMaxBytes.IsNull()) {
    if (aOptions.mMaxBytes.Value() == 0) {
      // Resolve with an empty buffer.
      nsTArray<uint8_t> arr(0);
      promise->MaybeResolve(TypedArrayCreator<Uint8Array>(arr));
      return promise.forget();
    }
    toRead.emplace(aOptions.mMaxBytes.Value());
  }

  RunOnBackgroundThread<nsTArray<uint8_t>>(
      promise,
      [file = std::move(file), toRead, decompress = aOptions.mDecompress]() {
        return ReadSync(file, toRead, decompress);
      });
  return promise.forget();
}

/* static */
already_AddRefed<Promise> IOUtils::ReadUTF8(GlobalObject& aGlobal,
                                            const nsAString& aPath,
                                            const ReadUTF8Options& aOptions) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
  RefPtr<Promise> promise = CreateJSPromise(aGlobal);
  NS_ENSURE_TRUE(!!promise, nullptr);

  nsCOMPtr<nsIFile> file = new nsLocalFile();
  REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);
  RunOnBackgroundThread<nsString>(
      promise, [file = std::move(file), decompress = aOptions.mDecompress]() {
        return ReadUTF8Sync(file, decompress);
      });

  return promise.forget();
}

/* static */
already_AddRefed<Promise> IOUtils::Write(GlobalObject& aGlobal,
                                         const nsAString& aPath,
                                         const Uint8Array& aData,
                                         const WriteOptions& aOptions) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
  RefPtr<Promise> promise = CreateJSPromise(aGlobal);
  NS_ENSURE_TRUE(!!promise, nullptr);
  REJECT_IF_SHUTTING_DOWN(promise);

  nsCOMPtr<nsIFile> file = new nsLocalFile();
  REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);

  aData.ComputeState();
  auto buf = Buffer<uint8_t>::CopyFrom(Span(aData.Data(), aData.Length()));
  if (buf.isNothing()) {
    promise->MaybeRejectWithOperationError(
        "Out of memory: Could not allocate buffer while writing to file");
    return promise.forget();
  }

  auto opts = InternalWriteOpts::FromBinding(aOptions);
  if (opts.isErr()) {
    RejectJSPromise(promise, opts.unwrapErr());
    return promise.forget();
  }

  RunOnBackgroundThread<uint32_t>(
      promise, [file = std::move(file), buf = std::move(*buf),
                opts = opts.unwrap()]() { return WriteSync(file, buf, opts); });

  return promise.forget();
}

/* static */
already_AddRefed<Promise> IOUtils::WriteUTF8(GlobalObject& aGlobal,
                                             const nsAString& aPath,
                                             const nsAString& aString,
                                             const WriteOptions& aOptions) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
  RefPtr<Promise> promise = CreateJSPromise(aGlobal);
  NS_ENSURE_TRUE(!!promise, nullptr);
  REJECT_IF_SHUTTING_DOWN(promise);

  nsCOMPtr<nsIFile> file = new nsLocalFile();
  REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);

  nsCString utf8Str;
  if (!CopyUTF16toUTF8(aString, utf8Str, fallible)) {
    promise->MaybeRejectWithOperationError(
        "Out of memory: Could not allocate buffer while writing to file");
    return promise.forget();
  }

  auto opts = InternalWriteOpts::FromBinding(aOptions);
  if (opts.isErr()) {
    RejectJSPromise(promise, opts.unwrapErr());
    return promise.forget();
  }

  RunOnBackgroundThread<uint32_t>(
      promise,
      [file = std::move(file), utf8Str = std::move(utf8Str),
       opts = opts.unwrap()]() { return WriteUTF8Sync(file, utf8Str, opts); });

  return promise.forget();
}

/* static */
already_AddRefed<Promise> IOUtils::Move(GlobalObject& aGlobal,
                                        const nsAString& aSourcePath,
                                        const nsAString& aDestPath,
                                        const MoveOptions& aOptions) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
  RefPtr<Promise> promise = CreateJSPromise(aGlobal);
  NS_ENSURE_TRUE(!!promise, nullptr);
  REJECT_IF_SHUTTING_DOWN(promise);

  nsCOMPtr<nsIFile> sourceFile = new nsLocalFile();
  REJECT_IF_INIT_PATH_FAILED(sourceFile, aSourcePath, promise);

  nsCOMPtr<nsIFile> destFile = new nsLocalFile();
  REJECT_IF_INIT_PATH_FAILED(destFile, aDestPath, promise);

  RunOnBackgroundThread<Ok>(promise, [sourceFile = std::move(sourceFile),
                                      destFile = std::move(destFile),
                                      noOverwrite = aOptions.mNoOverwrite]() {
    return MoveSync(sourceFile, destFile, noOverwrite);
  });

  return promise.forget();
}

/* static */
already_AddRefed<Promise> IOUtils::Remove(GlobalObject& aGlobal,
                                          const nsAString& aPath,
                                          const RemoveOptions& aOptions) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
  RefPtr<Promise> promise = CreateJSPromise(aGlobal);
  NS_ENSURE_TRUE(!!promise, nullptr);
  REJECT_IF_SHUTTING_DOWN(promise);

  nsCOMPtr<nsIFile> file = new nsLocalFile();
  REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);

  RunOnBackgroundThread<Ok>(
      promise, [file = std::move(file), ignoreAbsent = aOptions.mIgnoreAbsent,
                recursive = aOptions.mRecursive]() {
        return RemoveSync(file, ignoreAbsent, recursive);
      });

  return promise.forget();
}

/* static */
already_AddRefed<Promise> IOUtils::MakeDirectory(
    GlobalObject& aGlobal, const nsAString& aPath,
    const MakeDirectoryOptions& aOptions) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
  RefPtr<Promise> promise = CreateJSPromise(aGlobal);
  NS_ENSURE_TRUE(!!promise, nullptr);
  REJECT_IF_SHUTTING_DOWN(promise);

  nsCOMPtr<nsIFile> file = new nsLocalFile();
  REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);

  RunOnBackgroundThread<Ok>(
      promise,
      [file = std::move(file), createAncestors = aOptions.mCreateAncestors,
       ignoreExisting = aOptions.mIgnoreExisting,
       permissions = aOptions.mPermissions]() {
        return MakeDirectorySync(file, createAncestors, ignoreExisting,
                                 permissions);
      });

  return promise.forget();
}

already_AddRefed<Promise> IOUtils::Stat(GlobalObject& aGlobal,
                                        const nsAString& aPath) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
  RefPtr<Promise> promise = CreateJSPromise(aGlobal);
  NS_ENSURE_TRUE(!!promise, nullptr);
  REJECT_IF_SHUTTING_DOWN(promise);

  nsCOMPtr<nsIFile> file = new nsLocalFile();
  REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);

  RunOnBackgroundThread<InternalFileInfo>(
      promise, [file = std::move(file)]() { return StatSync(file); });

  return promise.forget();
}

/* static */
already_AddRefed<Promise> IOUtils::Copy(GlobalObject& aGlobal,
                                        const nsAString& aSourcePath,
                                        const nsAString& aDestPath,
                                        const CopyOptions& aOptions) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
  RefPtr<Promise> promise = CreateJSPromise(aGlobal);
  NS_ENSURE_TRUE(!!promise, nullptr);
  REJECT_IF_SHUTTING_DOWN(promise);

  nsCOMPtr<nsIFile> sourceFile = new nsLocalFile();
  REJECT_IF_INIT_PATH_FAILED(sourceFile, aSourcePath, promise);

  nsCOMPtr<nsIFile> destFile = new nsLocalFile();
  REJECT_IF_INIT_PATH_FAILED(destFile, aDestPath, promise);

  RunOnBackgroundThread<Ok>(
      promise,
      [sourceFile = std::move(sourceFile), destFile = std::move(destFile),
       noOverwrite = aOptions.mNoOverwrite, recursive = aOptions.mRecursive]() {
        return CopySync(sourceFile, destFile, noOverwrite, recursive);
      });

  return promise.forget();
}

/* static */
already_AddRefed<Promise> IOUtils::Touch(
    GlobalObject& aGlobal, const nsAString& aPath,
    const Optional<int64_t>& aModification) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
  RefPtr<Promise> promise = CreateJSPromise(aGlobal);
  NS_ENSURE_TRUE(!!promise, nullptr);

  nsCOMPtr<nsIFile> file = new nsLocalFile();
  REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);

  Maybe<int64_t> newTime = Nothing();
  if (aModification.WasPassed()) {
    newTime = Some(aModification.Value());
  }

  RunOnBackgroundThread<int64_t>(promise, [file = std::move(file), newTime]() {
    return TouchSync(file, newTime);
  });

  return promise.forget();
}

/* static */
already_AddRefed<Promise> IOUtils::GetChildren(GlobalObject& aGlobal,
                                               const nsAString& aPath) {
  MOZ_ASSERT(XRE_IsParentProcess());
  RefPtr<Promise> promise = CreateJSPromise(aGlobal);
  NS_ENSURE_TRUE(!!promise, nullptr);

  nsCOMPtr<nsIFile> file = new nsLocalFile();
  REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);

  RunOnBackgroundThread<nsTArray<nsString>>(
      promise, [file = std::move(file)]() { return GetChildrenSync(file); });

  return promise.forget();
}

/* static */
already_AddRefed<Promise> IOUtils::SetPermissions(GlobalObject& aGlobal,
                                                  const nsAString& aPath,
                                                  const uint32_t aPermissions) {
  MOZ_ASSERT(XRE_IsParentProcess());
  RefPtr<Promise> promise = CreateJSPromise(aGlobal);
  NS_ENSURE_TRUE(!!promise, nullptr);

  nsCOMPtr<nsIFile> file = new nsLocalFile();
  REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);

  RunOnBackgroundThread<Ok>(
      promise, [file = std::move(file), permissions = aPermissions]() {
        return SetPermissionsSync(file, permissions);
      });

  return promise.forget();
}

/* static */
already_AddRefed<Promise> IOUtils::Exists(GlobalObject& aGlobal,
                                          const nsAString& aPath) {
  MOZ_ASSERT(XRE_IsParentProcess());
  RefPtr<Promise> promise = CreateJSPromise(aGlobal);
  NS_ENSURE_TRUE(!!promise, nullptr);

  nsCOMPtr<nsIFile> file = new nsLocalFile();
  REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);

  RunOnBackgroundThread<bool>(
      promise, [file = std::move(file)]() { return ExistsSync(file); });

  return promise.forget();
}

/* static */
already_AddRefed<nsISerialEventTarget> IOUtils::GetBackgroundEventTarget() {
  if (sShutdownStarted) {
    return nullptr;
  }

  auto lockedBackgroundEventTarget = sBackgroundEventTarget.Lock();
  if (!lockedBackgroundEventTarget.ref()) {
    nsCOMPtr<nsISerialEventTarget> et;
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
    nsresult rv = svc->GetProfileBeforeChange(getter_AddRefs(barrier));
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
template <typename T>
void IOUtils::ResolveJSPromise(Promise* aPromise, const T& aValue) {
  if constexpr (std::is_same_v<T, nsTArray<uint8_t>>) {
    TypedArrayCreator<Uint8Array> arr(aValue);
    aPromise->MaybeResolve(arr);
  } else if constexpr (std::is_same_v<T, Ok>) {
    aPromise->MaybeResolveWithUndefined();
  } else {
    aPromise->MaybeResolve(aValue);
  }
}

/* static */
void IOUtils::RejectJSPromise(Promise* aPromise, const IOError& aError) {
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
    case NS_ERROR_FILE_CORRUPTED:
      aPromise->MaybeRejectWithNotReadableError(
          errMsg.refOr("Target file could not be read and may be corrupt"_ns));
      break;
    case NS_ERROR_ILLEGAL_INPUT:
    case NS_ERROR_ILLEGAL_VALUE:
      aPromise->MaybeRejectWithDataError(
          errMsg.refOr("Argument is not allowed"_ns));
      break;
    default:
      aPromise->MaybeRejectWithUnknownError(
          errMsg.refOr(FormatErrorMessage(aError.Code(), "Unexpected error")));
  }
}

/* static */
Result<nsTArray<uint8_t>, IOUtils::IOError> IOUtils::ReadSync(
    nsIFile* aFile, const Maybe<uint32_t>& aMaxBytes, const bool aDecompress) {
  MOZ_ASSERT(!NS_IsMainThread());

  if (aMaxBytes.isSome() && aDecompress) {
    return Err(
        IOError(NS_ERROR_ILLEGAL_INPUT)
            .WithMessage(
                "The `maxBytes` and `decompress` options are not compatible"));
  }

  RefPtr<nsFileStream> stream = new nsFileStream();
  if (nsresult rv =
          stream->Init(aFile, PR_RDONLY | nsIFile::OS_READAHEAD, 0666, 0);
      NS_FAILED(rv)) {
    return Err(IOError(rv).WithMessage("Could not open the file at %s",
                                       aFile->HumanReadablePath().get()));
  }
  int64_t bufSize = 0;

  if (aMaxBytes.isNothing()) {
    // Limitation: We cannot read files that are larger than the max size of a
    //             TypedArray (UINT32_MAX bytes). Reject if the file is too
    //             big to be read.

    int64_t streamSize = -1;
    if (nsresult rv = stream->GetSize(&streamSize); NS_FAILED(rv)) {
      return Err(IOError(NS_ERROR_FILE_ACCESS_DENIED)
                     .WithMessage("Could not get info for the file at %s",
                                  aFile->HumanReadablePath().get()));
    }
    MOZ_RELEASE_ASSERT(streamSize >= 0);

    if (streamSize > static_cast<int64_t>(UINT32_MAX)) {
      return Err(
          IOError(NS_ERROR_FILE_TOO_BIG)
              .WithMessage("Could not read the file at %s because it is too "
                           "large(size=%" PRId64 " bytes)",
                           aFile->HumanReadablePath().get(), streamSize));
    }
    bufSize = static_cast<uint32_t>(streamSize);
  } else {
    bufSize = aMaxBytes.value();
  }

  nsTArray<uint8_t> buffer;
  if (!buffer.SetCapacity(bufSize, fallible)) {
    return Err(IOError(NS_ERROR_OUT_OF_MEMORY)
                   .WithMessage("Could not allocate buffer to read file(%s)",
                                aFile->HumanReadablePath().get()));
  }

  Span<char> toRead(reinterpret_cast<char*>(buffer.Elements()), bufSize);

  // Read the file from disk.
  uint32_t totalRead = 0;
  while (totalRead != bufSize) {
    uint32_t bytesRead = 0;
    if (nsresult rv =
            stream->Read(toRead.Elements(), bufSize - totalRead, &bytesRead);
        NS_FAILED(rv)) {
      return Err(IOError(rv).WithMessage(
          "Encountered an unexpected error while reading file(%s)",
          aFile->HumanReadablePath().get()));
    }
    if (bytesRead == 0) {
      break;
    }
    totalRead += bytesRead;
    toRead = toRead.From(bytesRead);
  }
  DebugOnly<bool> success = buffer.SetLength(totalRead, fallible);
  MOZ_ASSERT(success);

  // Decompress the file contents, if required.
  if (aDecompress) {
    return MozLZ4::Decompress(Span(buffer));
  }
  return std::move(buffer);
}

/* static */
Result<nsString, IOUtils::IOError> IOUtils::ReadUTF8Sync(
    nsIFile* aFile, const bool aDecompress) {
  MOZ_ASSERT(!NS_IsMainThread());

  auto rv = ReadSync(aFile, Nothing(), aDecompress);
  if (rv.isErr()) {
    return Err(rv.unwrapErr());
  }

  const nsTArray<uint8_t> bytes = rv.unwrap();
  auto utf8Span =
      Span(reinterpret_cast<const char*>(bytes.Elements()), bytes.Length());
  if (!IsUtf8(utf8Span)) {
    return Err(
        IOError(NS_ERROR_FILE_CORRUPTED)
            .WithMessage(
                "Could not read file(%s) because it is not UTF-8 encoded",
                aFile->HumanReadablePath().get()));
  }

  nsDependentCSubstring utf8Str(utf8Span.Elements(), utf8Span.Length());
  nsString utf16Str;
  if (!CopyUTF8toUTF16(utf8Str, utf16Str, fallible)) {
    return Err(IOError(NS_ERROR_OUT_OF_MEMORY)
                   .WithMessage("Could not allocate buffer to convert UTF-8 "
                                "contents of file at (%s) to UTF-16",
                                aFile->HumanReadablePath().get()));
  }

  return utf16Str;
}

/* static */
Result<uint32_t, IOUtils::IOError> IOUtils::WriteSync(
    nsIFile* aFile, const Span<const uint8_t>& aByteArray,
    const IOUtils::InternalWriteOpts& aOptions) {
  MOZ_ASSERT(!NS_IsMainThread());

  nsIFile* backupFile = aOptions.mBackupFile;
  nsIFile* tempFile = aOptions.mTmpFile;

  bool exists = false;
  MOZ_TRY(aFile->Exists(&exists));

  if (aOptions.mNoOverwrite && exists) {
    return Err(IOError(NS_ERROR_FILE_ALREADY_EXISTS)
                   .WithMessage("Refusing to overwrite the file at %s\n"
                                "Specify `noOverwrite: false` to allow "
                                "overwriting the destination",
                                aFile->HumanReadablePath().get()));
  }

  // If backupFile was specified, perform the backup as a move.
  if (exists && backupFile) {
    // We copy `destFile` here to a new `nsIFile` because
    // `nsIFile::MoveToFollowingLinks` will update the path of the file. If we
    // did not do this, we would end up having `destFile` point to the same
    // location as `backupFile`. Then, when we went to write to `destFile`, we
    // would end up overwriting `backupFile` and never actually write to the
    // file we were supposed to.
    nsCOMPtr<nsIFile> toMove;
    MOZ_ALWAYS_SUCCEEDS(aFile->Clone(getter_AddRefs(toMove)));

    if (MoveSync(toMove, backupFile, aOptions.mNoOverwrite).isErr()) {
      return Err(IOError(NS_ERROR_FILE_COPY_OR_MOVE_FAILED)
                     .WithMessage("Failed to backup the source file(%s) to %s",
                                  aFile->HumanReadablePath().get(),
                                  backupFile->HumanReadablePath().get()));
    }
  }

  // If tempFile was specified, we will write to there first, then perform a
  // move to ensure the file ends up at the final requested destination.
  nsIFile* writeFile;

  if (tempFile) {
    writeFile = tempFile;
  } else {
    writeFile = aFile;
  }

  int32_t flags = PR_WRONLY | PR_TRUNCATE | PR_CREATE_FILE;
  if (aOptions.mFlush) {
    flags |= PR_SYNC;
  }

  // Try to perform the write and ensure that the file is closed before
  // continuing.
  uint32_t totalWritten = 0;
  {
    // Compress the byte array if required.
    nsTArray<uint8_t> compressed;
    Span<const char> bytes;
    if (aOptions.mCompress) {
      auto rv = MozLZ4::Compress(aByteArray);
      if (rv.isErr()) {
        return rv.propagateErr();
      }
      compressed = rv.unwrap();
      bytes = Span(reinterpret_cast<const char*>(compressed.Elements()),
                   compressed.Length());
    } else {
      bytes = Span(reinterpret_cast<const char*>(aByteArray.Elements()),
                   aByteArray.Length());
    }

    RefPtr<nsFileOutputStream> stream = new nsFileOutputStream();
    if (nsresult rv = stream->Init(writeFile, flags, 0666, 0); NS_FAILED(rv)) {
      return Err(
          IOError(rv).WithMessage("Could not open the file at %s for writing",
                                  writeFile->HumanReadablePath().get()));
    }

    // nsFileStream::Write uses PR_Write under the hood, which accepts a
    // *int32_t* for the chunk size.
    uint32_t chunkSize = INT32_MAX;
    Span<const char> pendingBytes = bytes;

    while (pendingBytes.Length() > 0) {
      if (pendingBytes.Length() < chunkSize) {
        chunkSize = pendingBytes.Length();
      }

      uint32_t bytesWritten = 0;
      if (nsresult rv =
              stream->Write(pendingBytes.Elements(), chunkSize, &bytesWritten);
          NS_FAILED(rv)) {
        return Err(IOError(rv).WithMessage(
            "Could not write chunk (size = %" PRIu32
            ") to file %s. The file may be corrupt.",
            chunkSize, writeFile->HumanReadablePath().get()));
      }
      pendingBytes = pendingBytes.From(bytesWritten);
      totalWritten += bytesWritten;
    }
  }

  // If tempFile was passed, check destFile against writeFile and, if they
  // differ, the operation is finished by performing a move.
  if (tempFile) {
    nsAutoStringN<256> destPath;
    nsAutoStringN<256> writePath;

    MOZ_ALWAYS_SUCCEEDS(aFile->GetPath(destPath));
    MOZ_ALWAYS_SUCCEEDS(writeFile->GetPath(writePath));

    // nsIFile::MoveToFollowingLinks will only update the path of the file if
    // the move succeeds.
    if (destPath != writePath && MoveSync(writeFile, aFile, false).isErr()) {
      return Err(IOError(NS_ERROR_FILE_COPY_OR_MOVE_FAILED)
                     .WithMessage(
                         "Could not move temporary file(%s) to destination(%s)",
                         writeFile->HumanReadablePath().get(),
                         aFile->HumanReadablePath().get()));
    }
  }
  return totalWritten;
}

/* static */
Result<uint32_t, IOUtils::IOError> IOUtils::WriteUTF8Sync(
    nsIFile* aFile, const nsCString& aUTF8String,
    const InternalWriteOpts& aOptions) {
  MOZ_ASSERT(!NS_IsMainThread());

  Span utf8Bytes(reinterpret_cast<const uint8_t*>(aUTF8String.get()),
                 aUTF8String.Length());

  return WriteSync(aFile, utf8Bytes, aOptions);
}

/* static */
Result<Ok, IOUtils::IOError> IOUtils::MoveSync(nsIFile* aSourceFile,
                                               nsIFile* aDestFile,
                                               bool aNoOverwrite) {
  MOZ_ASSERT(!NS_IsMainThread());

  // Ensure the source file exists before continuing. If it doesn't exist,
  // subsequent operations can fail in different ways on different platforms.
  bool srcExists = false;
  MOZ_TRY(aSourceFile->Exists(&srcExists));
  if (!srcExists) {
    return Err(
        IOError(NS_ERROR_FILE_NOT_FOUND)
            .WithMessage(
                "Could not move source file(%s) because it does not exist",
                aSourceFile->HumanReadablePath().get()));
  }

  return CopyOrMoveSync(&nsIFile::MoveToFollowingLinks, "move", aSourceFile,
                        aDestFile, aNoOverwrite);
}

/* static */
Result<Ok, IOUtils::IOError> IOUtils::CopySync(nsIFile* aSourceFile,
                                               nsIFile* aDestFile,
                                               bool aNoOverwrite,
                                               bool aRecursive) {
  MOZ_ASSERT(!NS_IsMainThread());

  // Ensure the source file exists before continuing. If it doesn't exist,
  // subsequent operations can fail in different ways on different platforms.
  bool srcExists;
  MOZ_TRY(aSourceFile->Exists(&srcExists));
  if (!srcExists) {
    return Err(
        IOError(NS_ERROR_FILE_NOT_FOUND)
            .WithMessage(
                "Could not copy source file(%s) because it does not exist",
                aSourceFile->HumanReadablePath().get()));
  }

  // If source is a directory, fail immediately unless the recursive option is
  // true.
  bool srcIsDir = false;
  MOZ_TRY(aSourceFile->IsDirectory(&srcIsDir));
  if (srcIsDir && !aRecursive) {
    return Err(
        IOError(NS_ERROR_FILE_COPY_OR_MOVE_FAILED)
            .WithMessage(
                "Refused to copy source directory(%s) to the destination(%s)\n"
                "Specify the `recursive: true` option to allow copying "
                "directories",
                aSourceFile->HumanReadablePath().get(),
                aDestFile->HumanReadablePath().get()));
  }

  return CopyOrMoveSync(&nsIFile::CopyToFollowingLinks, "copy", aSourceFile,
                        aDestFile, aNoOverwrite);
}

/* static */
template <typename CopyOrMoveFn>
Result<Ok, IOUtils::IOError> IOUtils::CopyOrMoveSync(CopyOrMoveFn aMethod,
                                                     const char* aMethodName,
                                                     nsIFile* aSource,
                                                     nsIFile* aDest,
                                                     bool aNoOverwrite) {
  MOZ_ASSERT(!NS_IsMainThread());

  // Case 1: Destination is an existing directory. Copy/move source into dest.
  bool destIsDir = false;
  bool destExists = true;

  nsresult rv = aDest->IsDirectory(&destIsDir);
  if (NS_SUCCEEDED(rv) && destIsDir) {
    rv = (aSource->*aMethod)(aDest, u""_ns);
    if (NS_FAILED(rv)) {
      return Err(IOError(rv).WithMessage(
          "Could not %s source file(%s) to destination directory(%s)",
          aMethodName, aSource->HumanReadablePath().get(),
          aDest->HumanReadablePath().get()));
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
                aMethodName, aSource->HumanReadablePath().get(),
                aDest->HumanReadablePath().get()));
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
                                  aMethodName,
                                  aSource->HumanReadablePath().get(),
                                  aDest->HumanReadablePath().get()));
    }
  }

  nsCOMPtr<nsIFile> destDir;
  nsAutoString destName;
  MOZ_TRY(aDest->GetLeafName(destName));
  MOZ_TRY(aDest->GetParent(getter_AddRefs(destDir)));

  // NB: if destDir doesn't exist, then |CopyToFollowingLinks| or
  // |MoveToFollowingLinks| will create it.
  rv = (aSource->*aMethod)(destDir, destName);
  if (NS_FAILED(rv)) {
    return Err(IOError(rv).WithMessage(
        "Could not %s the source file(%s) to the destination(%s)", aMethodName,
        aSource->HumanReadablePath().get(), aDest->HumanReadablePath().get()));
  }
  return Ok();
}

/* static */
Result<Ok, IOUtils::IOError> IOUtils::RemoveSync(nsIFile* aFile,
                                                 bool aIgnoreAbsent,
                                                 bool aRecursive) {
  MOZ_ASSERT(!NS_IsMainThread());

  nsresult rv = aFile->Remove(aRecursive);
  if (aIgnoreAbsent && IsFileNotFound(rv)) {
    return Ok();
  }
  if (NS_FAILED(rv)) {
    IOError err(rv);
    if (IsFileNotFound(rv)) {
      return Err(err.WithMessage(
          "Could not remove the file at %s because it does not exist.\n"
          "Specify the `ignoreAbsent: true` option to mitigate this error",
          aFile->HumanReadablePath().get()));
    }
    if (rv == NS_ERROR_FILE_DIR_NOT_EMPTY) {
      return Err(err.WithMessage(
          "Could not remove the non-empty directory at %s.\n"
          "Specify the `recursive: true` option to mitigate this error",
          aFile->HumanReadablePath().get()));
    }
    return Err(err.WithMessage("Could not remove the file at %s",
                               aFile->HumanReadablePath().get()));
  }
  return Ok();
}

/* static */
Result<Ok, IOUtils::IOError> IOUtils::MakeDirectorySync(nsIFile* aFile,
                                                        bool aCreateAncestors,
                                                        bool aIgnoreExisting,
                                                        int32_t aMode) {
  MOZ_ASSERT(!NS_IsMainThread());

  // nsIFile::Create will create ancestor directories by default.
  // If the caller does not want this behaviour, then check and possibly
  // return an error.
  if (!aCreateAncestors) {
    nsCOMPtr<nsIFile> parent;
    MOZ_TRY(aFile->GetParent(getter_AddRefs(parent)));
    bool parentExists = false;
    MOZ_TRY(parent->Exists(&parentExists));
    if (!parentExists) {
      return Err(IOError(NS_ERROR_FILE_NOT_FOUND)
                     .WithMessage("Could not create directory at %s because "
                                  "the path has missing "
                                  "ancestor components",
                                  aFile->HumanReadablePath().get()));
    }
  }

  nsresult rv = aFile->Create(nsIFile::DIRECTORY_TYPE, aMode);
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_FILE_ALREADY_EXISTS) {
      // NB: We may report a success only if the target is an existing
      // directory. We don't want to silence errors that occur if the target is
      // an existing file, since trying to create a directory where a regular
      // file exists may be indicative of a logic error.
      bool isDirectory;
      MOZ_TRY(aFile->IsDirectory(&isDirectory));
      if (!isDirectory) {
        return Err(IOError(NS_ERROR_FILE_NOT_DIRECTORY)
                       .WithMessage("Could not create directory because the "
                                    "target file(%s) exists "
                                    "and is not a directory",
                                    aFile->HumanReadablePath().get()));
      }
      // The directory exists.
      // The caller may suppress this error.
      if (aIgnoreExisting) {
        return Ok();
      }
      // Otherwise, forward it.
      return Err(IOError(rv).WithMessage(
          "Could not create directory because it already exists at %s\n"
          "Specify the `ignoreExisting: true` option to mitigate this "
          "error",
          aFile->HumanReadablePath().get()));
    }
    return Err(IOError(rv).WithMessage("Could not create directory at %s",
                                       aFile->HumanReadablePath().get()));
  }
  return Ok();
}

Result<IOUtils::InternalFileInfo, IOUtils::IOError> IOUtils::StatSync(
    nsIFile* aFile) {
  MOZ_ASSERT(!NS_IsMainThread());

  InternalFileInfo info;
  MOZ_ALWAYS_SUCCEEDS(aFile->GetPath(info.mPath));

  bool isRegular = false;
  // IsFile will stat and cache info in the file object. If the file doesn't
  // exist, or there is an access error, we'll discover it here.
  // Any subsequent errors are unexpected and will just be forwarded.
  nsresult rv = aFile->IsFile(&isRegular);
  if (NS_FAILED(rv)) {
    IOError err(rv);
    if (IsFileNotFound(rv)) {
      return Err(
          err.WithMessage("Could not stat file(%s) because it does not exist",
                          aFile->HumanReadablePath().get()));
    }
    return Err(err);
  }

  // Now we can populate the info object by querying the file.
  info.mType = FileType::Regular;
  if (!isRegular) {
    bool isDir = false;
    MOZ_TRY(aFile->IsDirectory(&isDir));
    info.mType = isDir ? FileType::Directory : FileType::Other;
  }

  int64_t size = -1;
  if (info.mType == FileType::Regular) {
    MOZ_TRY(aFile->GetFileSize(&size));
  }
  info.mSize = size;
  PRTime lastModified = 0;
  MOZ_TRY(aFile->GetLastModifiedTime(&lastModified));
  info.mLastModified = static_cast<int64_t>(lastModified);

  PRTime creationTime = 0;
  if (nsresult rv = aFile->GetCreationTime(&creationTime); NS_SUCCEEDED(rv)) {
    info.mCreationTime.emplace(static_cast<int64_t>(creationTime));
  } else if (NS_FAILED(rv) && rv != NS_ERROR_NOT_IMPLEMENTED) {
    // This field is only supported on some platforms.
    return Err(IOError(rv));
  }

  MOZ_TRY(aFile->GetPermissions(&info.mPermissions));

  return info;
}

/* static */
Result<int64_t, IOUtils::IOError> IOUtils::TouchSync(
    nsIFile* aFile, const Maybe<int64_t>& aNewModTime) {
  MOZ_ASSERT(!NS_IsMainThread());

  int64_t now = aNewModTime.valueOrFrom([]() {
    // NB: PR_Now reports time in microseconds since the Unix epoch
    //     (1970-01-01T00:00:00Z). Both nsLocalFile's lastModifiedTime and
    //     JavaScript's Date primitive values are to be expressed in
    //     milliseconds since Epoch.
    int64_t nowMicros = PR_Now();
    int64_t nowMillis = nowMicros / PR_USEC_PER_MSEC;
    return nowMillis;
  });

  // nsIFile::SetLastModifiedTime will *not* do what is expected when passed 0
  // as an argument. Rather than setting the time to 0, it will recalculate the
  // system time and set it to that value instead. We explicit forbid this,
  // because this side effect is surprising.
  //
  // If it ever becomes possible to set a file time to 0, this check should be
  // removed, though this use case seems rare.
  if (now == 0) {
    return Err(
        IOError(NS_ERROR_ILLEGAL_VALUE)
            .WithMessage(
                "Refusing to set the modification time of file(%s) to 0.\n"
                "To use the current system time, call `touch` with no "
                "arguments",
                aFile->HumanReadablePath().get()));
  }

  nsresult rv = aFile->SetLastModifiedTime(now);

  if (NS_FAILED(rv)) {
    IOError err(rv);
    if (IsFileNotFound(rv)) {
      return Err(
          err.WithMessage("Could not touch file(%s) because it does not exist",
                          aFile->HumanReadablePath().get()));
    }
    return Err(err);
  }
  return now;
}

/* static */
Result<nsTArray<nsString>, IOUtils::IOError> IOUtils::GetChildrenSync(
    nsIFile* aFile) {
  MOZ_ASSERT(!NS_IsMainThread());

  nsCOMPtr<nsIDirectoryEnumerator> iter;
  nsresult rv = aFile->GetDirectoryEntries(getter_AddRefs(iter));
  if (NS_FAILED(rv)) {
    IOError err(rv);
    if (IsFileNotFound(rv)) {
      return Err(err.WithMessage(
          "Could not get children of file(%s) because it does not exist",
          aFile->HumanReadablePath().get()));
    }
    if (IsNotDirectory(rv)) {
      return Err(err.WithMessage(
          "Could not get children of file(%s) because it is not a directory",
          aFile->HumanReadablePath().get()));
    }
    return Err(err);
  }
  nsTArray<nsString> children;

  bool hasMoreElements = false;
  MOZ_TRY(iter->HasMoreElements(&hasMoreElements));
  while (hasMoreElements) {
    nsCOMPtr<nsIFile> child;
    MOZ_TRY(iter->GetNextFile(getter_AddRefs(child)));
    if (child) {
      nsString path;
      MOZ_TRY(child->GetPath(path));
      children.AppendElement(path);
    }
    MOZ_TRY(iter->HasMoreElements(&hasMoreElements));
  }

  return children;
}

/* static */
Result<Ok, IOUtils::IOError> IOUtils::SetPermissionsSync(
    nsIFile* aFile, const uint32_t aPermissions) {
  MOZ_ASSERT(!NS_IsMainThread());

  MOZ_TRY(aFile->SetPermissions(aPermissions));
  return Ok{};
}

/* static */
Result<bool, IOUtils::IOError> IOUtils::ExistsSync(nsIFile* aFile) {
  MOZ_ASSERT(!NS_IsMainThread());

  bool exists = false;
  MOZ_TRY(aFile->Exists(&exists));

  return exists;
}

/* static */
Result<nsTArray<uint8_t>, IOUtils::IOError> IOUtils::MozLZ4::Compress(
    Span<const uint8_t> aUncompressed) {
  nsTArray<uint8_t> result;
  size_t worstCaseSize =
      Compression::LZ4::maxCompressedSize(aUncompressed.Length()) + HEADER_SIZE;
  if (!result.SetCapacity(worstCaseSize, fallible)) {
    return Err(IOError(NS_ERROR_OUT_OF_MEMORY)
                   .WithMessage("Could not allocate buffer to compress data"));
  }
  result.AppendElements(Span(MAGIC_NUMBER.data(), MAGIC_NUMBER.size()));
  std::array<uint8_t, sizeof(uint32_t)> contentSizeBytes{};
  LittleEndian::writeUint32(contentSizeBytes.data(), aUncompressed.Length());
  result.AppendElements(Span(contentSizeBytes.data(), contentSizeBytes.size()));

  if (aUncompressed.Length() == 0) {
    // Don't try to compress an empty buffer.
    // Just return the correctly formed header.
    result.SetLength(HEADER_SIZE);
    return result;
  }

  size_t compressed = Compression::LZ4::compress(
      reinterpret_cast<const char*>(aUncompressed.Elements()),
      aUncompressed.Length(),
      reinterpret_cast<char*>(result.Elements()) + HEADER_SIZE);
  if (!compressed) {
    return Err(
        IOError(NS_ERROR_UNEXPECTED).WithMessage("Could not compress data"));
  }
  result.SetLength(HEADER_SIZE + compressed);
  return result;
}

/* static */
Result<nsTArray<uint8_t>, IOUtils::IOError> IOUtils::MozLZ4::Decompress(
    Span<const uint8_t> aFileContents) {
  if (aFileContents.LengthBytes() < HEADER_SIZE) {
    return Err(
        IOError(NS_ERROR_FILE_CORRUPTED)
            .WithMessage(
                "Could not decompress file because the buffer is too short"));
  }
  auto header = aFileContents.To(HEADER_SIZE);
  if (!std::equal(std::begin(MAGIC_NUMBER), std::end(MAGIC_NUMBER),
                  std::begin(header))) {
    nsCString magicStr;
    uint32_t i = 0;
    for (; i < header.Length() - 1; ++i) {
      magicStr.AppendPrintf("%02X ", header.at(i));
    }
    magicStr.AppendPrintf("%02X", header.at(i));

    return Err(IOError(NS_ERROR_FILE_CORRUPTED)
                   .WithMessage("Could not decompress file because it has an "
                                "invalid LZ4 header (wrong magic number: '%s')",
                                magicStr.get()));
  }
  size_t numBytes = sizeof(uint32_t);
  Span<const uint8_t> sizeBytes = header.Last(numBytes);
  uint32_t expectedDecompressedSize =
      LittleEndian::readUint32(sizeBytes.data());
  if (expectedDecompressedSize == 0) {
    return nsTArray<uint8_t>(0);
  }
  auto contents = aFileContents.From(HEADER_SIZE);
  nsTArray<uint8_t> decompressed;
  if (!decompressed.SetCapacity(expectedDecompressedSize, fallible)) {
    return Err(
        IOError(NS_ERROR_OUT_OF_MEMORY)
            .WithMessage("Could not allocate buffer to decompress data"));
  }
  size_t actualSize = 0;
  if (!Compression::LZ4::decompress(
          reinterpret_cast<const char*>(contents.Elements()), contents.Length(),
          reinterpret_cast<char*>(decompressed.Elements()),
          expectedDecompressedSize, &actualSize)) {
    return Err(
        IOError(NS_ERROR_FILE_CORRUPTED)
            .WithMessage(
                "Could not decompress file contents, the file may be corrupt"));
  }
  decompressed.SetLength(actualSize);
  return decompressed;
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

Result<IOUtils::InternalWriteOpts, IOUtils::IOError>
IOUtils::InternalWriteOpts::FromBinding(const WriteOptions& aOptions) {
  InternalWriteOpts opts;
  opts.mFlush = aOptions.mFlush;
  opts.mNoOverwrite = aOptions.mNoOverwrite;

  if (aOptions.mBackupFile.WasPassed()) {
    opts.mBackupFile = new nsLocalFile();
    if (nsresult rv =
            opts.mBackupFile->InitWithPath(aOptions.mBackupFile.Value());
        NS_FAILED(rv)) {
      return Err(IOUtils::IOError(rv).WithMessage(
          "Could not parse path of backupFile (%s)",
          NS_ConvertUTF16toUTF8(aOptions.mBackupFile.Value()).get()));
    }
  }

  if (aOptions.mTmpPath.WasPassed()) {
    opts.mTmpFile = new nsLocalFile();
    if (nsresult rv = opts.mTmpFile->InitWithPath(aOptions.mTmpPath.Value());
        NS_FAILED(rv)) {
      return Err(IOUtils::IOError(rv).WithMessage(
          "Could not parse path of temp file (%s)",
          NS_ConvertUTF16toUTF8(aOptions.mTmpPath.Value()).get()));
    }
  }

  opts.mCompress = aOptions.mCompress;
  return opts;
}

}  // namespace mozilla::dom

#undef REJECT_IF_SHUTTING_DOWN
#undef REJECT_IF_INIT_PATH_FAILED

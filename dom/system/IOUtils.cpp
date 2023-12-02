/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IOUtils.h"

#include <cstdint>

#include "ErrorList.h"
#include "TypedArray.h"
#include "js/ArrayBuffer.h"
#include "js/ColumnNumber.h"  // JS::ColumnNumberOneOrigin
#include "js/JSON.h"
#include "js/Utility.h"
#include "js/experimental/TypedData.h"
#include "jsfriendapi.h"
#include "mozilla/Assertions.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/Compression.h"
#include "mozilla/Encoding.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/FileUtils.h"
#include "mozilla/Maybe.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Services.h"
#include "mozilla/Span.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TextUtils.h"
#include "mozilla/Try.h"
#include "mozilla/Unused.h"
#include "mozilla/Utf8.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/IOUtilsBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/ipc/LaunchError.h"
#include "PathUtils.h"
#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsFileStreams.h"
#include "nsIDirectoryEnumerator.h"
#include "nsIFile.h"
#include "nsIGlobalObject.h"
#include "nsIInputStream.h"
#include "nsISupports.h"
#include "nsLocalFile.h"
#include "nsNetUtil.h"
#include "nsNSSComponent.h"
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
#include "ScopedNSSTypes.h"
#include "secoidt.h"

#if defined(XP_UNIX) && !defined(ANDROID)
#  include "nsSystemInfo.h"
#endif

#if defined(XP_WIN)
#  include "nsILocalFileWin.h"
#elif defined(XP_MACOSX)
#  include "nsILocalFileMac.h"
#endif

#ifdef XP_UNIX
#  include "base/process_util.h"
#endif

#define REJECT_IF_INIT_PATH_FAILED(_file, _path, _promise)            \
  do {                                                                \
    if (nsresult _rv = PathUtils::InitFileWithPath((_file), (_path)); \
        NS_FAILED(_rv)) {                                             \
      (_promise)->MaybeRejectWithOperationError(                      \
          FormatErrorMessage(_rv, "Could not parse path (%s)",        \
                             NS_ConvertUTF16toUTF8(_path).get()));    \
      return;                                                         \
    }                                                                 \
  } while (0)

static constexpr auto SHUTDOWN_ERROR =
    "IOUtils: Shutting down and refusing additional I/O tasks"_ns;

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
  return aResult == NS_ERROR_FILE_NOT_FOUND;
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

[[nodiscard]] inline bool ToJSValue(
    JSContext* aCx, const IOUtils::InternalFileInfo& aInternalFileInfo,
    JS::MutableHandle<JS::Value> aValue) {
  FileInfo info;
  info.mPath.Construct(aInternalFileInfo.mPath);
  info.mType.Construct(aInternalFileInfo.mType);
  info.mSize.Construct(aInternalFileInfo.mSize);

  if (aInternalFileInfo.mCreationTime.isSome()) {
    info.mCreationTime.Construct(aInternalFileInfo.mCreationTime.ref());
  }
  info.mLastAccessed.Construct(aInternalFileInfo.mLastAccessed);
  info.mLastModified.Construct(aInternalFileInfo.mLastModified);

  info.mPermissions.Construct(aInternalFileInfo.mPermissions);

  return ToJSValue(aCx, info, aValue);
}

template <typename T>
static void ResolveJSPromise(Promise* aPromise, T&& aValue) {
  if constexpr (std::is_same_v<T, Ok>) {
    aPromise->MaybeResolveWithUndefined();
  } else if constexpr (std::is_same_v<T, nsTArray<uint8_t>>) {
    TypedArrayCreator<Uint8Array> array(aValue);
    aPromise->MaybeResolve(array);
  } else {
    aPromise->MaybeResolve(std::forward<T>(aValue));
  }
}

static void RejectJSPromise(Promise* aPromise, const IOUtils::IOError& aError) {
  const auto& errMsg = aError.Message();

  switch (aError.Code()) {
    case NS_ERROR_FILE_UNRESOLVABLE_SYMLINK:
      [[fallthrough]];  // to NS_ERROR_FILE_INVALID_PATH
    case NS_ERROR_FILE_NOT_FOUND:
      [[fallthrough]];  // to NS_ERROR_FILE_INVALID_PATH
    case NS_ERROR_FILE_INVALID_PATH:
      aPromise->MaybeRejectWithNotFoundError(errMsg.refOr("File not found"_ns));
      break;
    case NS_ERROR_FILE_IS_LOCKED:
      [[fallthrough]];  // to NS_ERROR_FILE_ACCESS_DENIED
    case NS_ERROR_FILE_ACCESS_DENIED:
      aPromise->MaybeRejectWithNotAllowedError(
          errMsg.refOr("Access was denied to the target file"_ns));
      break;
    case NS_ERROR_FILE_TOO_BIG:
      aPromise->MaybeRejectWithNotReadableError(
          errMsg.refOr("Target file is too big"_ns));
      break;
    case NS_ERROR_FILE_NO_DEVICE_SPACE:
      aPromise->MaybeRejectWithNotReadableError(
          errMsg.refOr("Target device is full"_ns));
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
      [[fallthrough]];  // to NS_ERROR_FILE_DESTINATION_NOT_DIR
    case NS_ERROR_FILE_DESTINATION_NOT_DIR:
      aPromise->MaybeRejectWithInvalidAccessError(
          errMsg.refOr("Target file is not a directory"_ns));
      break;
    case NS_ERROR_FILE_IS_DIRECTORY:
      aPromise->MaybeRejectWithInvalidAccessError(
          errMsg.refOr("Target file is a directory"_ns));
      break;
    case NS_ERROR_FILE_UNKNOWN_TYPE:
      aPromise->MaybeRejectWithInvalidAccessError(
          errMsg.refOr("Target file is of unknown type"_ns));
      break;
    case NS_ERROR_FILE_NAME_TOO_LONG:
      aPromise->MaybeRejectWithOperationError(
          errMsg.refOr("Target file path is too long"_ns));
      break;
    case NS_ERROR_FILE_UNRECOGNIZED_PATH:
      aPromise->MaybeRejectWithOperationError(
          errMsg.refOr("Target file path is not recognized"_ns));
      break;
    case NS_ERROR_FILE_DIR_NOT_EMPTY:
      aPromise->MaybeRejectWithOperationError(
          errMsg.refOr("Target directory is not empty"_ns));
      break;
    case NS_ERROR_FILE_DEVICE_FAILURE:
      [[fallthrough]];  // to NS_ERROR_FILE_FS_CORRUPTED
    case NS_ERROR_FILE_FS_CORRUPTED:
      aPromise->MaybeRejectWithNotReadableError(
          errMsg.refOr("Target file system may be corrupt or unavailable"_ns));
      break;
    case NS_ERROR_FILE_CORRUPTED:
      aPromise->MaybeRejectWithNotReadableError(
          errMsg.refOr("Target file could not be read and may be corrupt"_ns));
      break;
    case NS_ERROR_ILLEGAL_INPUT:
      [[fallthrough]];  // NS_ERROR_ILLEGAL_VALUE
    case NS_ERROR_ILLEGAL_VALUE:
      aPromise->MaybeRejectWithDataError(
          errMsg.refOr("Argument is not allowed"_ns));
      break;
    case NS_ERROR_NOT_AVAILABLE:
      aPromise->MaybeRejectWithNotFoundError(errMsg.refOr("Unavailable"_ns));
      break;
    case NS_ERROR_ABORT:
      aPromise->MaybeRejectWithAbortError(errMsg.refOr("Operation aborted"_ns));
      break;
    default:
      aPromise->MaybeRejectWithUnknownError(FormatErrorMessage(
          aError.Code(), errMsg.refOr("Unexpected error"_ns).get()));
  }
}

static void RejectShuttingDown(Promise* aPromise) {
  RejectJSPromise(aPromise,
                  IOUtils::IOError(NS_ERROR_ABORT).WithMessage(SHUTDOWN_ERROR));
}

static bool AssertParentProcessWithCallerLocationImpl(GlobalObject& aGlobal,
                                                      nsCString& reason) {
  if (MOZ_LIKELY(XRE_IsParentProcess())) {
    return true;
  }

  AutoJSAPI jsapi;
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  MOZ_ALWAYS_TRUE(global);
  MOZ_ALWAYS_TRUE(jsapi.Init(global));

  JSContext* cx = jsapi.cx();

  JS::AutoFilename scriptFilename;
  uint32_t lineNo = 0;
  JS::ColumnNumberOneOrigin colNo;

  NS_ENSURE_TRUE(
      JS::DescribeScriptedCaller(cx, &scriptFilename, &lineNo, &colNo), false);

  NS_ENSURE_TRUE(scriptFilename.get(), false);

  reason.AppendPrintf(" Called from %s:%d:%d.", scriptFilename.get(), lineNo,
                      colNo.oneOriginValue());
  return false;
}

static void AssertParentProcessWithCallerLocation(GlobalObject& aGlobal) {
  nsCString reason = "IOUtils can only be used in the parent process."_ns;
  if (!AssertParentProcessWithCallerLocationImpl(aGlobal, reason)) {
    MOZ_CRASH_UNSAFE_PRINTF("%s", reason.get());
  }
}

// IOUtils implementation
/* static */
IOUtils::StateMutex IOUtils::sState{"IOUtils::sState"};

/* static */
template <typename Fn>
already_AddRefed<Promise> IOUtils::WithPromiseAndState(GlobalObject& aGlobal,
                                                       ErrorResult& aError,
                                                       Fn aFn) {
  AssertParentProcessWithCallerLocation(aGlobal);

  RefPtr<Promise> promise = CreateJSPromise(aGlobal, aError);
  if (!promise) {
    return nullptr;
  }

  if (auto state = GetState()) {
    aFn(promise, state.ref());
  } else {
    RejectShuttingDown(promise);
  }
  return promise.forget();
}

/* static */
template <typename OkT, typename Fn>
void IOUtils::DispatchAndResolve(IOUtils::EventQueue* aQueue, Promise* aPromise,
                                 Fn aFunc) {
  RefPtr<StrongWorkerRef> workerRef;
  if (!NS_IsMainThread()) {
    // We need to manually keep the worker alive until the promise returned by
    // Dispatch() resolves or rejects.
    workerRef = StrongWorkerRef::CreateForcibly(GetCurrentThreadWorkerPrivate(),
                                                __func__);
  }

  if (RefPtr<IOPromise<OkT>> p = aQueue->Dispatch<OkT, Fn>(std::move(aFunc))) {
    p->Then(
        GetCurrentSerialEventTarget(), __func__,
        [workerRef, promise = RefPtr(aPromise)](OkT&& ok) {
          ResolveJSPromise(promise, std::forward<OkT>(ok));
        },
        [workerRef, promise = RefPtr(aPromise)](const IOError& err) {
          RejectJSPromise(promise, err);
        });
  }
}

/* static */
already_AddRefed<Promise> IOUtils::Read(GlobalObject& aGlobal,
                                        const nsAString& aPath,
                                        const ReadOptions& aOptions,
                                        ErrorResult& aError) {
  return WithPromiseAndState(
      aGlobal, aError, [&](Promise* promise, auto& state) {
        nsCOMPtr<nsIFile> file = new nsLocalFile();
        REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);

        Maybe<uint32_t> toRead = Nothing();
        if (!aOptions.mMaxBytes.IsNull()) {
          if (aOptions.mMaxBytes.Value() == 0) {
            // Resolve with an empty buffer.
            nsTArray<uint8_t> arr(0);
            promise->MaybeResolve(TypedArrayCreator<Uint8Array>(arr));
            return;
          }
          toRead.emplace(aOptions.mMaxBytes.Value());
        }

        DispatchAndResolve<JsBuffer>(
            state->mEventQueue, promise,
            [file = std::move(file), offset = aOptions.mOffset, toRead,
             decompress = aOptions.mDecompress]() {
              return ReadSync(file, offset, toRead, decompress,
                              BufferKind::Uint8Array);
            });
      });
}

/* static */
RefPtr<SyncReadFile> IOUtils::OpenFileForSyncReading(GlobalObject& aGlobal,
                                                     const nsAString& aPath,
                                                     ErrorResult& aRv) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());

  // This API is only exposed to workers, so we should not be on the main
  // thread here.
  MOZ_RELEASE_ASSERT(!NS_IsMainThread());

  nsCOMPtr<nsIFile> file = new nsLocalFile();
  if (nsresult rv = PathUtils::InitFileWithPath(file, aPath); NS_FAILED(rv)) {
    aRv.ThrowOperationError(FormatErrorMessage(
        rv, "Could not parse path (%s)", NS_ConvertUTF16toUTF8(aPath).get()));
    return nullptr;
  }

  RefPtr<nsFileRandomAccessStream> stream = new nsFileRandomAccessStream();
  if (nsresult rv =
          stream->Init(file, PR_RDONLY | nsIFile::OS_READAHEAD, 0666, 0);
      NS_FAILED(rv)) {
    aRv.ThrowOperationError(
        FormatErrorMessage(rv, "Could not open the file at %s",
                           NS_ConvertUTF16toUTF8(aPath).get()));
    return nullptr;
  }

  int64_t size = 0;
  if (nsresult rv = stream->GetSize(&size); NS_FAILED(rv)) {
    aRv.ThrowOperationError(FormatErrorMessage(
        rv, "Could not get the stream size for the file at %s",
        NS_ConvertUTF16toUTF8(aPath).get()));
    return nullptr;
  }

  return new SyncReadFile(aGlobal.GetAsSupports(), std::move(stream), size);
}

/* static */
already_AddRefed<Promise> IOUtils::ReadUTF8(GlobalObject& aGlobal,
                                            const nsAString& aPath,
                                            const ReadUTF8Options& aOptions,
                                            ErrorResult& aError) {
  return WithPromiseAndState(
      aGlobal, aError, [&](Promise* promise, auto& state) {
        nsCOMPtr<nsIFile> file = new nsLocalFile();
        REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);

        DispatchAndResolve<JsBuffer>(
            state->mEventQueue, promise,
            [file = std::move(file), decompress = aOptions.mDecompress]() {
              return ReadUTF8Sync(file, decompress);
            });
      });
}

/* static */
already_AddRefed<Promise> IOUtils::ReadJSON(GlobalObject& aGlobal,
                                            const nsAString& aPath,
                                            const ReadUTF8Options& aOptions,
                                            ErrorResult& aError) {
  return WithPromiseAndState(
      aGlobal, aError, [&](Promise* promise, auto& state) {
        nsCOMPtr<nsIFile> file = new nsLocalFile();
        REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);

        RefPtr<StrongWorkerRef> workerRef;
        if (!NS_IsMainThread()) {
          // We need to manually keep the worker alive until the promise
          // returned by Dispatch() resolves or rejects.
          workerRef = StrongWorkerRef::CreateForcibly(
              GetCurrentThreadWorkerPrivate(), __func__);
        }

        state->mEventQueue
            ->template Dispatch<JsBuffer>(
                [file, decompress = aOptions.mDecompress]() {
                  return ReadUTF8Sync(file, decompress);
                })
            ->Then(
                GetCurrentSerialEventTarget(), __func__,
                [workerRef, promise = RefPtr{promise},
                 file](JsBuffer&& aBuffer) {
                  AutoJSAPI jsapi;
                  if (NS_WARN_IF(!jsapi.Init(promise->GetGlobalObject()))) {
                    promise->MaybeRejectWithUnknownError(
                        "Could not initialize JS API");
                    return;
                  }
                  JSContext* cx = jsapi.cx();

                  JS::Rooted<JSString*> jsonStr(
                      cx,
                      IOUtils::JsBuffer::IntoString(cx, std::move(aBuffer)));
                  if (!jsonStr) {
                    RejectJSPromise(promise, IOError(NS_ERROR_OUT_OF_MEMORY));
                    return;
                  }

                  JS::Rooted<JS::Value> val(cx);
                  if (!JS_ParseJSON(cx, jsonStr, &val)) {
                    JS::Rooted<JS::Value> exn(cx);
                    if (JS_GetPendingException(cx, &exn)) {
                      JS_ClearPendingException(cx);
                      promise->MaybeReject(exn);
                    } else {
                      RejectJSPromise(
                          promise,
                          IOError(NS_ERROR_DOM_UNKNOWN_ERR)
                              .WithMessage(
                                  "ParseJSON threw an uncatchable exception "
                                  "while parsing file(%s)",
                                  file->HumanReadablePath().get()));
                    }

                    return;
                  }

                  promise->MaybeResolve(val);
                },
                [workerRef, promise = RefPtr{promise}](const IOError& aErr) {
                  RejectJSPromise(promise, aErr);
                });
      });
}

/* static */
already_AddRefed<Promise> IOUtils::Write(GlobalObject& aGlobal,
                                         const nsAString& aPath,
                                         const Uint8Array& aData,
                                         const WriteOptions& aOptions,
                                         ErrorResult& aError) {
  return WithPromiseAndState(
      aGlobal, aError, [&](Promise* promise, auto& state) {
        nsCOMPtr<nsIFile> file = new nsLocalFile();
        REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);

        Maybe<Buffer<uint8_t>> buf = aData.CreateFromData<Buffer<uint8_t>>();
        if (buf.isNothing()) {
          promise->MaybeRejectWithOperationError(
              "Out of memory: Could not allocate buffer while writing to file");
          return;
        }

        auto opts = InternalWriteOpts::FromBinding(aOptions);
        if (opts.isErr()) {
          RejectJSPromise(promise, opts.unwrapErr());
          return;
        }

        DispatchAndResolve<uint32_t>(
            state->mEventQueue, promise,
            [file = std::move(file), buf = buf.extract(),
             opts = opts.unwrap()]() { return WriteSync(file, buf, opts); });
      });
}

/* static */
already_AddRefed<Promise> IOUtils::WriteUTF8(GlobalObject& aGlobal,
                                             const nsAString& aPath,
                                             const nsACString& aString,
                                             const WriteOptions& aOptions,
                                             ErrorResult& aError) {
  return WithPromiseAndState(
      aGlobal, aError, [&](Promise* promise, auto& state) {
        nsCOMPtr<nsIFile> file = new nsLocalFile();
        REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);

        auto opts = InternalWriteOpts::FromBinding(aOptions);
        if (opts.isErr()) {
          RejectJSPromise(promise, opts.unwrapErr());
          return;
        }

        DispatchAndResolve<uint32_t>(
            state->mEventQueue, promise,
            [file = std::move(file), str = nsCString(aString),
             opts = opts.unwrap()]() {
              return WriteSync(file, AsBytes(Span(str)), opts);
            });
      });
}

/* static */
already_AddRefed<Promise> IOUtils::WriteJSON(GlobalObject& aGlobal,
                                             const nsAString& aPath,
                                             JS::Handle<JS::Value> aValue,
                                             const WriteOptions& aOptions,
                                             ErrorResult& aError) {
  return WithPromiseAndState(
      aGlobal, aError, [&](Promise* promise, auto& state) {
        nsCOMPtr<nsIFile> file = new nsLocalFile();
        REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);

        auto opts = InternalWriteOpts::FromBinding(aOptions);
        if (opts.isErr()) {
          RejectJSPromise(promise, opts.unwrapErr());
          return;
        }

        if (opts.inspect().mMode == WriteMode::Append ||
            opts.inspect().mMode == WriteMode::AppendOrCreate) {
          promise->MaybeRejectWithNotSupportedError(
              "IOUtils.writeJSON does not support appending to files."_ns);
          return;
        }

        JSContext* cx = aGlobal.Context();
        JS::Rooted<JS::Value> rootedValue(cx, aValue);
        nsString string;
        if (!nsContentUtils::StringifyJSON(cx, aValue, string,
                                           UndefinedIsNullStringLiteral)) {
          JS::Rooted<JS::Value> exn(cx, JS::UndefinedValue());
          if (JS_GetPendingException(cx, &exn)) {
            JS_ClearPendingException(cx);
            promise->MaybeReject(exn);
          } else {
            RejectJSPromise(
                promise,
                IOError(NS_ERROR_DOM_UNKNOWN_ERR)
                    .WithMessage("Could not serialize object to JSON"));
          }
          return;
        }

        DispatchAndResolve<uint32_t>(
            state->mEventQueue, promise,
            [file = std::move(file), string = std::move(string),
             opts = opts.unwrap()]() -> Result<uint32_t, IOError> {
              nsAutoCString utf8Str;
              if (!CopyUTF16toUTF8(string, utf8Str, fallible)) {
                return Err(IOError(NS_ERROR_OUT_OF_MEMORY));
              }
              return WriteSync(file, AsBytes(Span(utf8Str)), opts);
            });
      });
}

/* static */
already_AddRefed<Promise> IOUtils::Move(GlobalObject& aGlobal,
                                        const nsAString& aSourcePath,
                                        const nsAString& aDestPath,
                                        const MoveOptions& aOptions,
                                        ErrorResult& aError) {
  return WithPromiseAndState(
      aGlobal, aError, [&](Promise* promise, auto& state) {
        nsCOMPtr<nsIFile> sourceFile = new nsLocalFile();
        REJECT_IF_INIT_PATH_FAILED(sourceFile, aSourcePath, promise);

        nsCOMPtr<nsIFile> destFile = new nsLocalFile();
        REJECT_IF_INIT_PATH_FAILED(destFile, aDestPath, promise);

        DispatchAndResolve<Ok>(
            state->mEventQueue, promise,
            [sourceFile = std::move(sourceFile), destFile = std::move(destFile),
             noOverwrite = aOptions.mNoOverwrite]() {
              return MoveSync(sourceFile, destFile, noOverwrite);
            });
      });
}

/* static */
already_AddRefed<Promise> IOUtils::Remove(GlobalObject& aGlobal,
                                          const nsAString& aPath,
                                          const RemoveOptions& aOptions,
                                          ErrorResult& aError) {
  return WithPromiseAndState(
      aGlobal, aError, [&](Promise* promise, auto& state) {
        nsCOMPtr<nsIFile> file = new nsLocalFile();
        REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);

        DispatchAndResolve<Ok>(
            state->mEventQueue, promise,
            [file = std::move(file), ignoreAbsent = aOptions.mIgnoreAbsent,
             recursive = aOptions.mRecursive,
             retryReadonly = aOptions.mRetryReadonly]() {
              return RemoveSync(file, ignoreAbsent, recursive, retryReadonly);
            });
      });
}

/* static */
already_AddRefed<Promise> IOUtils::MakeDirectory(
    GlobalObject& aGlobal, const nsAString& aPath,
    const MakeDirectoryOptions& aOptions, ErrorResult& aError) {
  return WithPromiseAndState(
      aGlobal, aError, [&](Promise* promise, auto& state) {
        nsCOMPtr<nsIFile> file = new nsLocalFile();
        REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);

        DispatchAndResolve<Ok>(state->mEventQueue, promise,
                               [file = std::move(file),
                                createAncestors = aOptions.mCreateAncestors,
                                ignoreExisting = aOptions.mIgnoreExisting,
                                permissions = aOptions.mPermissions]() {
                                 return MakeDirectorySync(file, createAncestors,
                                                          ignoreExisting,
                                                          permissions);
                               });
      });
}

already_AddRefed<Promise> IOUtils::Stat(GlobalObject& aGlobal,
                                        const nsAString& aPath,
                                        ErrorResult& aError) {
  return WithPromiseAndState(
      aGlobal, aError, [&](Promise* promise, auto& state) {
        nsCOMPtr<nsIFile> file = new nsLocalFile();
        REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);

        DispatchAndResolve<InternalFileInfo>(
            state->mEventQueue, promise,
            [file = std::move(file)]() { return StatSync(file); });
      });
}

/* static */
already_AddRefed<Promise> IOUtils::Copy(GlobalObject& aGlobal,
                                        const nsAString& aSourcePath,
                                        const nsAString& aDestPath,
                                        const CopyOptions& aOptions,
                                        ErrorResult& aError) {
  return WithPromiseAndState(
      aGlobal, aError, [&](Promise* promise, auto& state) {
        nsCOMPtr<nsIFile> sourceFile = new nsLocalFile();
        REJECT_IF_INIT_PATH_FAILED(sourceFile, aSourcePath, promise);

        nsCOMPtr<nsIFile> destFile = new nsLocalFile();
        REJECT_IF_INIT_PATH_FAILED(destFile, aDestPath, promise);

        DispatchAndResolve<Ok>(
            state->mEventQueue, promise,
            [sourceFile = std::move(sourceFile), destFile = std::move(destFile),
             noOverwrite = aOptions.mNoOverwrite,
             recursive = aOptions.mRecursive]() {
              return CopySync(sourceFile, destFile, noOverwrite, recursive);
            });
      });
}

/* static */
already_AddRefed<Promise> IOUtils::SetAccessTime(
    GlobalObject& aGlobal, const nsAString& aPath,
    const Optional<int64_t>& aAccess, ErrorResult& aError) {
  return SetTime(aGlobal, aPath, aAccess, &nsIFile::SetLastAccessedTime,
                 aError);
}

/* static */
already_AddRefed<Promise> IOUtils::SetModificationTime(
    GlobalObject& aGlobal, const nsAString& aPath,
    const Optional<int64_t>& aModification, ErrorResult& aError) {
  return SetTime(aGlobal, aPath, aModification, &nsIFile::SetLastModifiedTime,
                 aError);
}

/* static */
already_AddRefed<Promise> IOUtils::SetTime(GlobalObject& aGlobal,
                                           const nsAString& aPath,
                                           const Optional<int64_t>& aNewTime,
                                           IOUtils::SetTimeFn aSetTimeFn,
                                           ErrorResult& aError) {
  return WithPromiseAndState(
      aGlobal, aError, [&](Promise* promise, auto& state) {
        nsCOMPtr<nsIFile> file = new nsLocalFile();
        REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);

        int64_t newTime = aNewTime.WasPassed() ? aNewTime.Value()
                                               : PR_Now() / PR_USEC_PER_MSEC;
        DispatchAndResolve<int64_t>(
            state->mEventQueue, promise,
            [file = std::move(file), aSetTimeFn, newTime]() {
              return SetTimeSync(file, aSetTimeFn, newTime);
            });
      });
}

/* static */
already_AddRefed<Promise> IOUtils::GetChildren(
    GlobalObject& aGlobal, const nsAString& aPath,
    const GetChildrenOptions& aOptions, ErrorResult& aError) {
  return WithPromiseAndState(
      aGlobal, aError, [&](Promise* promise, auto& state) {
        nsCOMPtr<nsIFile> file = new nsLocalFile();
        REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);

        DispatchAndResolve<nsTArray<nsString>>(
            state->mEventQueue, promise,
            [file = std::move(file), ignoreAbsent = aOptions.mIgnoreAbsent]() {
              return GetChildrenSync(file, ignoreAbsent);
            });
      });
}

/* static */
already_AddRefed<Promise> IOUtils::SetPermissions(GlobalObject& aGlobal,
                                                  const nsAString& aPath,
                                                  uint32_t aPermissions,
                                                  const bool aHonorUmask,
                                                  ErrorResult& aError) {
  return WithPromiseAndState(
      aGlobal, aError, [&](Promise* promise, auto& state) {
#if defined(XP_UNIX) && !defined(ANDROID)
        if (aHonorUmask) {
          aPermissions &= ~nsSystemInfo::gUserUmask;
        }
#endif

        nsCOMPtr<nsIFile> file = new nsLocalFile();
        REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);

        DispatchAndResolve<Ok>(
            state->mEventQueue, promise,
            [file = std::move(file), permissions = aPermissions]() {
              return SetPermissionsSync(file, permissions);
            });
      });
}

/* static */
already_AddRefed<Promise> IOUtils::Exists(GlobalObject& aGlobal,
                                          const nsAString& aPath,
                                          ErrorResult& aError) {
  return WithPromiseAndState(
      aGlobal, aError, [&](Promise* promise, auto& state) {
        nsCOMPtr<nsIFile> file = new nsLocalFile();
        REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);

        DispatchAndResolve<bool>(
            state->mEventQueue, promise,
            [file = std::move(file)]() { return ExistsSync(file); });
      });
}

/* static */
already_AddRefed<Promise> IOUtils::CreateUniqueFile(GlobalObject& aGlobal,
                                                    const nsAString& aParent,
                                                    const nsAString& aPrefix,
                                                    const uint32_t aPermissions,
                                                    ErrorResult& aError) {
  return CreateUnique(aGlobal, aParent, aPrefix, nsIFile::NORMAL_FILE_TYPE,
                      aPermissions, aError);
}

/* static */
already_AddRefed<Promise> IOUtils::CreateUniqueDirectory(
    GlobalObject& aGlobal, const nsAString& aParent, const nsAString& aPrefix,
    const uint32_t aPermissions, ErrorResult& aError) {
  return CreateUnique(aGlobal, aParent, aPrefix, nsIFile::DIRECTORY_TYPE,
                      aPermissions, aError);
}

/* static */
already_AddRefed<Promise> IOUtils::CreateUnique(GlobalObject& aGlobal,
                                                const nsAString& aParent,
                                                const nsAString& aPrefix,
                                                const uint32_t aFileType,
                                                const uint32_t aPermissions,
                                                ErrorResult& aError) {
  return WithPromiseAndState(
      aGlobal, aError, [&](Promise* promise, auto& state) {
        nsCOMPtr<nsIFile> file = new nsLocalFile();
        REJECT_IF_INIT_PATH_FAILED(file, aParent, promise);

        if (nsresult rv = file->Append(aPrefix); NS_FAILED(rv)) {
          RejectJSPromise(promise,
                          IOError(rv).WithMessage(
                              "Could not append prefix `%s' to parent `%s'",
                              NS_ConvertUTF16toUTF8(aPrefix).get(),
                              file->HumanReadablePath().get()));
          return;
        }

        DispatchAndResolve<nsString>(
            state->mEventQueue, promise,
            [file = std::move(file), aPermissions, aFileType]() {
              return CreateUniqueSync(file, aFileType, aPermissions);
            });
      });
}

/* static */
already_AddRefed<Promise> IOUtils::ComputeHexDigest(
    GlobalObject& aGlobal, const nsAString& aPath,
    const HashAlgorithm aAlgorithm, ErrorResult& aError) {
  const bool nssInitialized = EnsureNSSInitializedChromeOrContent();

  return WithPromiseAndState(
      aGlobal, aError, [&](Promise* promise, auto& state) {
        if (!nssInitialized) {
          RejectJSPromise(promise,
                          IOError(NS_ERROR_UNEXPECTED)
                              .WithMessage("Could not initialize NSS"));
          return;
        }

        nsCOMPtr<nsIFile> file = new nsLocalFile();
        REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);

        DispatchAndResolve<nsCString>(state->mEventQueue, promise,
                                      [file = std::move(file), aAlgorithm]() {
                                        return ComputeHexDigestSync(file,
                                                                    aAlgorithm);
                                      });
      });
}

#if defined(XP_WIN)

/* static */
already_AddRefed<Promise> IOUtils::GetWindowsAttributes(GlobalObject& aGlobal,
                                                        const nsAString& aPath,
                                                        ErrorResult& aError) {
  return WithPromiseAndState(
      aGlobal, aError, [&](Promise* promise, auto& state) {
        nsCOMPtr<nsIFile> file = new nsLocalFile();
        REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);

        RefPtr<StrongWorkerRef> workerRef;
        if (!NS_IsMainThread()) {
          // We need to manually keep the worker alive until the promise
          // returned by Dispatch() resolves or rejects.
          workerRef = StrongWorkerRef::CreateForcibly(
              GetCurrentThreadWorkerPrivate(), __func__);
        }

        state->mEventQueue
            ->template Dispatch<uint32_t>([file = std::move(file)]() {
              return GetWindowsAttributesSync(file);
            })
            ->Then(
                GetCurrentSerialEventTarget(), __func__,
                [workerRef, promise = RefPtr{promise}](const uint32_t aAttrs) {
                  WindowsFileAttributes attrs;

                  attrs.mReadOnly.Construct(aAttrs & FILE_ATTRIBUTE_READONLY);
                  attrs.mHidden.Construct(aAttrs & FILE_ATTRIBUTE_HIDDEN);
                  attrs.mSystem.Construct(aAttrs & FILE_ATTRIBUTE_SYSTEM);

                  promise->MaybeResolve(attrs);
                },
                [workerRef, promise = RefPtr{promise}](const IOError& aErr) {
                  RejectJSPromise(promise, aErr);
                });
      });
}

/* static */
already_AddRefed<Promise> IOUtils::SetWindowsAttributes(
    GlobalObject& aGlobal, const nsAString& aPath,
    const WindowsFileAttributes& aAttrs, ErrorResult& aError) {
  return WithPromiseAndState(
      aGlobal, aError, [&](Promise* promise, auto& state) {
        nsCOMPtr<nsIFile> file = new nsLocalFile();
        REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);

        uint32_t setAttrs = 0;
        uint32_t clearAttrs = 0;

        if (aAttrs.mReadOnly.WasPassed()) {
          if (aAttrs.mReadOnly.Value()) {
            setAttrs |= FILE_ATTRIBUTE_READONLY;
          } else {
            clearAttrs |= FILE_ATTRIBUTE_READONLY;
          }
        }

        if (aAttrs.mHidden.WasPassed()) {
          if (aAttrs.mHidden.Value()) {
            setAttrs |= FILE_ATTRIBUTE_HIDDEN;
          } else {
            clearAttrs |= FILE_ATTRIBUTE_HIDDEN;
          }
        }

        if (aAttrs.mSystem.WasPassed()) {
          if (aAttrs.mSystem.Value()) {
            setAttrs |= FILE_ATTRIBUTE_SYSTEM;
          } else {
            clearAttrs |= FILE_ATTRIBUTE_SYSTEM;
          }
        }

        DispatchAndResolve<Ok>(
            state->mEventQueue, promise,
            [file = std::move(file), setAttrs, clearAttrs]() {
              return SetWindowsAttributesSync(file, setAttrs, clearAttrs);
            });
      });
}

#elif defined(XP_MACOSX)

/* static */
already_AddRefed<Promise> IOUtils::HasMacXAttr(GlobalObject& aGlobal,
                                               const nsAString& aPath,
                                               const nsACString& aAttr,
                                               ErrorResult& aError) {
  return WithPromiseAndState(
      aGlobal, aError, [&](Promise* promise, auto& state) {
        nsCOMPtr<nsIFile> file = new nsLocalFile();
        REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);

        DispatchAndResolve<bool>(
            state->mEventQueue, promise,
            [file = std::move(file), attr = nsCString(aAttr)]() {
              return HasMacXAttrSync(file, attr);
            });
      });
}

/* static */
already_AddRefed<Promise> IOUtils::GetMacXAttr(GlobalObject& aGlobal,
                                               const nsAString& aPath,
                                               const nsACString& aAttr,
                                               ErrorResult& aError) {
  return WithPromiseAndState(
      aGlobal, aError, [&](Promise* promise, auto& state) {
        nsCOMPtr<nsIFile> file = new nsLocalFile();
        REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);

        DispatchAndResolve<nsTArray<uint8_t>>(
            state->mEventQueue, promise,
            [file = std::move(file), attr = nsCString(aAttr)]() {
              return GetMacXAttrSync(file, attr);
            });
      });
}

/* static */
already_AddRefed<Promise> IOUtils::SetMacXAttr(GlobalObject& aGlobal,
                                               const nsAString& aPath,
                                               const nsACString& aAttr,
                                               const Uint8Array& aValue,
                                               ErrorResult& aError) {
  return WithPromiseAndState(
      aGlobal, aError, [&](Promise* promise, auto& state) {
        nsCOMPtr<nsIFile> file = new nsLocalFile();
        REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);

        nsTArray<uint8_t> value;

        if (!aValue.AppendDataTo(value)) {
          RejectJSPromise(
              promise,
              IOError(NS_ERROR_OUT_OF_MEMORY)
                  .WithMessage(
                      "Could not allocate buffer to set extended attribute"));
          return;
        }

        DispatchAndResolve<Ok>(state->mEventQueue, promise,
                               [file = std::move(file), attr = nsCString(aAttr),
                                value = std::move(value)] {
                                 return SetMacXAttrSync(file, attr, value);
                               });
      });
}

/* static */
already_AddRefed<Promise> IOUtils::DelMacXAttr(GlobalObject& aGlobal,
                                               const nsAString& aPath,
                                               const nsACString& aAttr,
                                               ErrorResult& aError) {
  return WithPromiseAndState(
      aGlobal, aError, [&](Promise* promise, auto& state) {
        nsCOMPtr<nsIFile> file = new nsLocalFile();
        REJECT_IF_INIT_PATH_FAILED(file, aPath, promise);

        DispatchAndResolve<Ok>(
            state->mEventQueue, promise,
            [file = std::move(file), attr = nsCString(aAttr)] {
              return DelMacXAttrSync(file, attr);
            });
      });
}

#endif

/* static */
already_AddRefed<Promise> IOUtils::GetFile(
    GlobalObject& aGlobal, const Sequence<nsString>& aComponents,
    ErrorResult& aError) {
  return WithPromiseAndState(
      aGlobal, aError, [&](Promise* promise, auto& state) {
        ErrorResult joinErr;
        nsCOMPtr<nsIFile> file = PathUtils::Join(aComponents, joinErr);
        if (joinErr.Failed()) {
          promise->MaybeReject(std::move(joinErr));
          return;
        }

        nsCOMPtr<nsIFile> parent;
        if (nsresult rv = file->GetParent(getter_AddRefs(parent));
            NS_FAILED(rv)) {
          RejectJSPromise(promise, IOError(rv).WithMessage(
                                       "Could not get parent directory"));
          return;
        }

        state->mEventQueue
            ->template Dispatch<Ok>([parent = std::move(parent)]() {
              return MakeDirectorySync(parent, /* aCreateAncestors = */ true,
                                       /* aIgnoreExisting = */ true, 0755);
            })
            ->Then(
                GetCurrentSerialEventTarget(), __func__,
                [file = std::move(file), promise = RefPtr(promise)](const Ok&) {
                  promise->MaybeResolve(file);
                },
                [promise = RefPtr(promise)](const IOError& err) {
                  RejectJSPromise(promise, err);
                });
      });
}

/* static */
already_AddRefed<Promise> IOUtils::GetDirectory(
    GlobalObject& aGlobal, const Sequence<nsString>& aComponents,
    ErrorResult& aError) {
  return WithPromiseAndState(
      aGlobal, aError, [&](Promise* promise, auto& state) {
        ErrorResult joinErr;
        nsCOMPtr<nsIFile> dir = PathUtils::Join(aComponents, joinErr);
        if (joinErr.Failed()) {
          promise->MaybeReject(std::move(joinErr));
          return;
        }

        state->mEventQueue
            ->template Dispatch<Ok>([dir]() {
              return MakeDirectorySync(dir, /* aCreateAncestors = */ true,
                                       /* aIgnoreExisting = */ true, 0755);
            })
            ->Then(
                GetCurrentSerialEventTarget(), __func__,
                [dir, promise = RefPtr(promise)](const Ok&) {
                  promise->MaybeResolve(dir);
                },
                [promise = RefPtr(promise)](const IOError& err) {
                  RejectJSPromise(promise, err);
                });
      });
}

/* static */
already_AddRefed<Promise> IOUtils::CreateJSPromise(GlobalObject& aGlobal,
                                                   ErrorResult& aError) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<Promise> promise = Promise::Create(global, aError);
  if (aError.Failed()) {
    return nullptr;
  }
  MOZ_ASSERT(promise);
  return do_AddRef(promise);
}

/* static */
Result<IOUtils::JsBuffer, IOUtils::IOError> IOUtils::ReadSync(
    nsIFile* aFile, const uint64_t aOffset, const Maybe<uint32_t> aMaxBytes,
    const bool aDecompress, IOUtils::BufferKind aBufferKind) {
  MOZ_ASSERT(!NS_IsMainThread());

  if (aMaxBytes.isSome() && aDecompress) {
    return Err(
        IOError(NS_ERROR_ILLEGAL_INPUT)
            .WithMessage(
                "The `maxBytes` and `decompress` options are not compatible"));
  }

  if (aOffset > static_cast<uint64_t>(INT64_MAX)) {
    return Err(IOError(NS_ERROR_ILLEGAL_INPUT)
                   .WithMessage("Requested offset is too large (%" PRIu64
                                " > %" PRId64 ")",
                                aOffset, INT64_MAX));
  }

  const int64_t offset = static_cast<int64_t>(aOffset);

  RefPtr<nsFileRandomAccessStream> stream = new nsFileRandomAccessStream();
  if (nsresult rv =
          stream->Init(aFile, PR_RDONLY | nsIFile::OS_READAHEAD, 0666, 0);
      NS_FAILED(rv)) {
    return Err(IOError(rv).WithMessage("Could not open the file at %s",
                                       aFile->HumanReadablePath().get()));
  }

  uint32_t bufSize = 0;

  if (aMaxBytes.isNothing()) {
    // Limitation: We cannot read more than the maximum size of a TypedArray
    //            (UINT32_MAX bytes). Reject if we have been requested to
    //            perform too large of a read.

    int64_t rawStreamSize = -1;
    if (nsresult rv = stream->GetSize(&rawStreamSize); NS_FAILED(rv)) {
      return Err(IOError(NS_ERROR_FILE_ACCESS_DENIED)
                     .WithMessage("Could not get info for the file at %s",
                                  aFile->HumanReadablePath().get()));
    }
    MOZ_RELEASE_ASSERT(rawStreamSize >= 0);

    uint64_t streamSize = static_cast<uint64_t>(rawStreamSize);
    if (aOffset >= streamSize) {
      bufSize = 0;
    } else {
      if (streamSize - offset > static_cast<int64_t>(UINT32_MAX)) {
        return Err(IOError(NS_ERROR_FILE_TOO_BIG)
                       .WithMessage(
                           "Could not read the file at %s with offset %" PRIu32
                           " because it is too large(size=%" PRIu64 " bytes)",
                           aFile->HumanReadablePath().get(), offset,
                           streamSize));
      }

      bufSize = static_cast<uint32_t>(streamSize - offset);
    }
  } else {
    bufSize = aMaxBytes.value();
  }

  if (offset > 0) {
    if (nsresult rv = stream->Seek(PR_SEEK_SET, offset); NS_FAILED(rv)) {
      return Err(IOError(rv).WithMessage(
          "Could not seek to position %" PRId64 " in file %s", offset,
          aFile->HumanReadablePath().get()));
    }
  }

  JsBuffer buffer = JsBuffer::CreateEmpty(aBufferKind);

  if (bufSize > 0) {
    auto result = JsBuffer::Create(aBufferKind, bufSize);
    if (result.isErr()) {
      return result.propagateErr();
    }
    buffer = result.unwrap();
    Span<char> toRead = buffer.BeginWriting();

    // Read the file from disk.
    uint32_t totalRead = 0;
    while (totalRead != bufSize) {
      // Read no more than INT32_MAX on each call to stream->Read, otherwise it
      // returns an error.
      uint32_t bytesToReadThisChunk =
          std::min<uint32_t>(bufSize - totalRead, INT32_MAX);
      uint32_t bytesRead = 0;
      if (nsresult rv =
              stream->Read(toRead.Elements(), bytesToReadThisChunk, &bytesRead);
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

    buffer.SetLength(totalRead);
  }

  // Decompress the file contents, if required.
  if (aDecompress) {
    return MozLZ4::Decompress(AsBytes(buffer.BeginReading()), aBufferKind);
  }

  return std::move(buffer);
}

/* static */
Result<IOUtils::JsBuffer, IOUtils::IOError> IOUtils::ReadUTF8Sync(
    nsIFile* aFile, bool aDecompress) {
  auto result = ReadSync(aFile, 0, Nothing{}, aDecompress, BufferKind::String);
  if (result.isErr()) {
    return result.propagateErr();
  }

  JsBuffer buffer = result.unwrap();
  if (!IsUtf8(buffer.BeginReading())) {
    return Err(
        IOError(NS_ERROR_FILE_CORRUPTED)
            .WithMessage(
                "Could not read file(%s) because it is not UTF-8 encoded",
                aFile->HumanReadablePath().get()));
  }

  return buffer;
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

  if (exists && aOptions.mMode == WriteMode::Create) {
    return Err(IOError(NS_ERROR_FILE_ALREADY_EXISTS)
                   .WithMessage("Refusing to overwrite the file at %s\n"
                                "Specify `mode: \"overwrite\"` to allow "
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

    bool noOverwrite = aOptions.mMode == WriteMode::Create;

    if (MoveSync(toMove, backupFile, noOverwrite).isErr()) {
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

  int32_t flags = PR_WRONLY;

  switch (aOptions.mMode) {
    case WriteMode::Overwrite:
      flags |= PR_TRUNCATE | PR_CREATE_FILE;
      break;

    case WriteMode::Append:
      flags |= PR_APPEND;
      break;

    case WriteMode::AppendOrCreate:
      flags |= PR_APPEND | PR_CREATE_FILE;
      break;

    case WriteMode::Create:
      flags |= PR_CREATE_FILE | PR_EXCL;
      break;

    default:
      MOZ_CRASH("IOUtils: unknown write mode");
  }

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
      // Normalize platform-specific errors for opening a directory to an access
      // denied error.
      if (rv == nsresult::NS_ERROR_FILE_IS_DIRECTORY) {
        rv = NS_ERROR_FILE_ACCESS_DENIED;
      }
      return Err(
          IOError(rv).WithMessage("Could not open the file at %s for writing",
                                  writeFile->HumanReadablePath().get()));
    }

    // nsFileRandomAccessStream::Write uses PR_Write under the hood, which
    // accepts a *int32_t* for the chunk size.
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
    if (destPath != writePath) {
      if (aOptions.mTmpFile) {
        bool isDir = false;
        if (nsresult rv = aFile->IsDirectory(&isDir);
            NS_FAILED(rv) && !IsFileNotFound(rv)) {
          return Err(IOError(rv).WithMessage("Could not stat the file at %s",
                                             aFile->HumanReadablePath().get()));
        }

        // If we attempt to write to a directory *without* a temp file, we get a
        // permission error.
        //
        // However, if we are writing to a temp file first, when we copy the
        // temp file over the destination file, we actually end up copying it
        // inside the directory, which is not what we want. In this case, we are
        // just going to bail out early.
        if (isDir) {
          return Err(
              IOError(NS_ERROR_FILE_ACCESS_DENIED)
                  .WithMessage("Could not open the file at %s for writing",
                               aFile->HumanReadablePath().get()));
        }
      }

      if (MoveSync(writeFile, aFile, /* aNoOverwrite = */ false).isErr()) {
        return Err(
            IOError(NS_ERROR_FILE_COPY_OR_MOVE_FAILED)
                .WithMessage(
                    "Could not move temporary file(%s) to destination(%s)",
                    writeFile->HumanReadablePath().get(),
                    aFile->HumanReadablePath().get()));
      }
    }
  }
  return totalWritten;
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

  // We know `destName` is a file and therefore must have a parent directory.
  MOZ_RELEASE_ASSERT(destDir);

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
                                                 bool aRecursive,
                                                 bool aRetryReadonly) {
  MOZ_ASSERT(!NS_IsMainThread());

  // Prevent an unused variable warning.
  (void)aRetryReadonly;

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

#ifdef XP_WIN

    if (rv == NS_ERROR_FILE_ACCESS_DENIED && aRetryReadonly) {
      MOZ_TRY(SetWindowsAttributesSync(aFile, 0, FILE_ATTRIBUTE_READONLY));
      return RemoveSync(aFile, aIgnoreAbsent, aRecursive,
                        /* aRetryReadonly = */ false);
    }

#endif

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

  nsCOMPtr<nsIFile> parent;
  MOZ_TRY(aFile->GetParent(getter_AddRefs(parent)));
  if (!parent) {
    // If we don't have a parent directory, we were called with a
    // root directory. If the directory doesn't already exist (e.g., asking
    // for a drive on Windows that does not exist), we will not be able to
    // create it.
    //
    // Calling `nsLocalFile::Create()` on Windows can fail with
    // `NS_ERROR_ACCESS_DENIED` trying to create a root directory, but we
    // would rather the call succeed, so return early if the directory exists.
    //
    // Otherwise, we fall through to `nsiFile::Create()` and let it fail there
    // instead.
    bool exists = false;
    MOZ_TRY(aFile->Exists(&exists));
    if (exists) {
      return Ok();
    }
  }

  nsresult rv =
      aFile->Create(nsIFile::DIRECTORY_TYPE, aMode, !aCreateAncestors);
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

  PRTime creationTime = 0;
  if (nsresult rv = aFile->GetCreationTime(&creationTime); NS_SUCCEEDED(rv)) {
    info.mCreationTime.emplace(static_cast<int64_t>(creationTime));
  } else if (NS_FAILED(rv) && rv != NS_ERROR_NOT_IMPLEMENTED) {
    // This field is only supported on some platforms.
    return Err(IOError(rv));
  }

  PRTime lastAccessed = 0;
  MOZ_TRY(aFile->GetLastAccessedTime(&lastAccessed));
  info.mLastAccessed = static_cast<int64_t>(lastAccessed);

  PRTime lastModified = 0;
  MOZ_TRY(aFile->GetLastModifiedTime(&lastModified));
  info.mLastModified = static_cast<int64_t>(lastModified);

  MOZ_TRY(aFile->GetPermissions(&info.mPermissions));

  return info;
}

/* static */
Result<int64_t, IOUtils::IOError> IOUtils::SetTimeSync(
    nsIFile* aFile, IOUtils::SetTimeFn aSetTimeFn, int64_t aNewTime) {
  MOZ_ASSERT(!NS_IsMainThread());

  // nsIFile::SetLastModifiedTime will *not* do what is expected when passed 0
  // as an argument. Rather than setting the time to 0, it will recalculate the
  // system time and set it to that value instead. We explicit forbid this,
  // because this side effect is surprising.
  //
  // If it ever becomes possible to set a file time to 0, this check should be
  // removed, though this use case seems rare.
  if (aNewTime == 0) {
    return Err(
        IOError(NS_ERROR_ILLEGAL_VALUE)
            .WithMessage(
                "Refusing to set the modification time of file(%s) to 0.\n"
                "To use the current system time, call `setModificationTime` "
                "with no arguments",
                aFile->HumanReadablePath().get()));
  }

  nsresult rv = (aFile->*aSetTimeFn)(aNewTime);

  if (NS_FAILED(rv)) {
    IOError err(rv);
    if (IsFileNotFound(rv)) {
      return Err(
          err.WithMessage("Could not set modification time of file(%s) "
                          "because it does not exist",
                          aFile->HumanReadablePath().get()));
    }
    return Err(err);
  }
  return aNewTime;
}

/* static */
Result<nsTArray<nsString>, IOUtils::IOError> IOUtils::GetChildrenSync(
    nsIFile* aFile, bool aIgnoreAbsent) {
  MOZ_ASSERT(!NS_IsMainThread());

  nsTArray<nsString> children;
  nsCOMPtr<nsIDirectoryEnumerator> iter;
  nsresult rv = aFile->GetDirectoryEntries(getter_AddRefs(iter));
  if (aIgnoreAbsent && IsFileNotFound(rv)) {
    return children;
  }
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
Result<nsString, IOUtils::IOError> IOUtils::CreateUniqueSync(
    nsIFile* aFile, const uint32_t aFileType, const uint32_t aPermissions) {
  MOZ_ASSERT(!NS_IsMainThread());

  if (nsresult rv = aFile->CreateUnique(aFileType, aPermissions);
      NS_FAILED(rv)) {
    return Err(IOError(rv).WithMessage("Could not create unique path"));
  }

  nsString path;
  MOZ_ALWAYS_SUCCEEDS(aFile->GetPath(path));

  return path;
}

/* static */
Result<nsCString, IOUtils::IOError> IOUtils::ComputeHexDigestSync(
    nsIFile* aFile, const HashAlgorithm aAlgorithm) {
  static constexpr size_t BUFFER_SIZE = 8192;

  SECOidTag alg;
  switch (aAlgorithm) {
    case HashAlgorithm::Sha1:
      alg = SEC_OID_SHA1;
      break;

    case HashAlgorithm::Sha256:
      alg = SEC_OID_SHA256;
      break;

    case HashAlgorithm::Sha384:
      alg = SEC_OID_SHA384;
      break;

    case HashAlgorithm::Sha512:
      alg = SEC_OID_SHA512;
      break;

    default:
      MOZ_RELEASE_ASSERT(false, "Unexpected HashAlgorithm");
  }

  Digest digest;
  if (nsresult rv = digest.Begin(alg); NS_FAILED(rv)) {
    return Err(IOError(rv).WithMessage("Could not hash file at %s",
                                       aFile->HumanReadablePath().get()));
  }

  RefPtr<nsIInputStream> stream;
  if (nsresult rv = NS_NewLocalFileInputStream(getter_AddRefs(stream), aFile);
      NS_FAILED(rv)) {
    return Err(IOError(rv).WithMessage("Could not open the file at %s",
                                       aFile->HumanReadablePath().get()));
  }

  char buffer[BUFFER_SIZE];
  uint32_t read = 0;
  for (;;) {
    if (nsresult rv = stream->Read(buffer, BUFFER_SIZE, &read); NS_FAILED(rv)) {
      return Err(IOError(rv).WithMessage(
          "Encountered an unexpected error while reading file(%s)",
          aFile->HumanReadablePath().get()));
    }
    if (read == 0) {
      break;
    }

    if (nsresult rv =
            digest.Update(reinterpret_cast<unsigned char*>(buffer), read);
        NS_FAILED(rv)) {
      return Err(IOError(rv).WithMessage("Could not hash file at %s",
                                         aFile->HumanReadablePath().get()));
    }
  }

  AutoTArray<uint8_t, SHA512_LENGTH> rawDigest;
  if (nsresult rv = digest.End(rawDigest); NS_FAILED(rv)) {
    return Err(IOError(rv).WithMessage("Could not hash file at %s",
                                       aFile->HumanReadablePath().get()));
  }

  nsCString hexDigest;
  if (!hexDigest.SetCapacity(2 * rawDigest.Length(), fallible)) {
    return Err(IOError(NS_ERROR_OUT_OF_MEMORY));
  }

  const char HEX[] = "0123456789abcdef";
  for (uint8_t b : rawDigest) {
    hexDigest.Append(HEX[(b >> 4) & 0xF]);
    hexDigest.Append(HEX[b & 0xF]);
  }

  return hexDigest;
}

#if defined(XP_WIN)

Result<uint32_t, IOUtils::IOError> IOUtils::GetWindowsAttributesSync(
    nsIFile* aFile) {
  MOZ_ASSERT(!NS_IsMainThread());

  uint32_t attrs = 0;

  nsCOMPtr<nsILocalFileWin> file = do_QueryInterface(aFile);
  MOZ_ASSERT(file);

  if (nsresult rv = file->GetWindowsFileAttributes(&attrs); NS_FAILED(rv)) {
    return Err(IOError(rv).WithMessage(
        "Could not get Windows file attributes for the file at `%s'",
        aFile->HumanReadablePath().get()));
  }
  return attrs;
}

Result<Ok, IOUtils::IOError> IOUtils::SetWindowsAttributesSync(
    nsIFile* aFile, const uint32_t aSetAttrs, const uint32_t aClearAttrs) {
  MOZ_ASSERT(!NS_IsMainThread());

  nsCOMPtr<nsILocalFileWin> file = do_QueryInterface(aFile);
  MOZ_ASSERT(file);

  if (nsresult rv = file->SetWindowsFileAttributes(aSetAttrs, aClearAttrs);
      NS_FAILED(rv)) {
    return Err(IOError(rv).WithMessage(
        "Could not set Windows file attributes for the file at `%s'",
        aFile->HumanReadablePath().get()));
  }

  return Ok{};
}

#elif defined(XP_MACOSX)

/* static */
Result<bool, IOUtils::IOError> IOUtils::HasMacXAttrSync(
    nsIFile* aFile, const nsCString& aAttr) {
  MOZ_ASSERT(!NS_IsMainThread());

  nsCOMPtr<nsILocalFileMac> file = do_QueryInterface(aFile);
  MOZ_ASSERT(file);

  bool hasAttr = false;
  if (nsresult rv = file->HasXAttr(aAttr, &hasAttr); NS_FAILED(rv)) {
    return Err(IOError(rv).WithMessage(
        "Could not read the extended attribute `%s' from the file `%s'",
        aAttr.get(), aFile->HumanReadablePath().get()));
  }

  return hasAttr;
}

/* static */
Result<nsTArray<uint8_t>, IOUtils::IOError> IOUtils::GetMacXAttrSync(
    nsIFile* aFile, const nsCString& aAttr) {
  MOZ_ASSERT(!NS_IsMainThread());

  nsCOMPtr<nsILocalFileMac> file = do_QueryInterface(aFile);
  MOZ_ASSERT(file);

  nsTArray<uint8_t> value;
  if (nsresult rv = file->GetXAttr(aAttr, value); NS_FAILED(rv)) {
    auto err = IOError(rv);

    if (rv == NS_ERROR_NOT_AVAILABLE) {
      return Err(err.WithMessage(
          "The file `%s' does not have an extended attribute `%s'",
          aFile->HumanReadablePath().get(), aAttr.get()));
    }

    return Err(err.WithMessage(
        "Could not read the extended attribute `%s' from the file `%s'",
        aAttr.get(), aFile->HumanReadablePath().get()));
  }

  return value;
}

/* static */
Result<Ok, IOUtils::IOError> IOUtils::SetMacXAttrSync(
    nsIFile* aFile, const nsCString& aAttr, const nsTArray<uint8_t>& aValue) {
  MOZ_ASSERT(!NS_IsMainThread());

  nsCOMPtr<nsILocalFileMac> file = do_QueryInterface(aFile);
  MOZ_ASSERT(file);

  if (nsresult rv = file->SetXAttr(aAttr, aValue); NS_FAILED(rv)) {
    return Err(IOError(rv).WithMessage(
        "Could not set extended attribute `%s' on file `%s'", aAttr.get(),
        aFile->HumanReadablePath().get()));
  }

  return Ok{};
}

/* static */
Result<Ok, IOUtils::IOError> IOUtils::DelMacXAttrSync(nsIFile* aFile,
                                                      const nsCString& aAttr) {
  MOZ_ASSERT(!NS_IsMainThread());

  nsCOMPtr<nsILocalFileMac> file = do_QueryInterface(aFile);
  MOZ_ASSERT(file);

  if (nsresult rv = file->DelXAttr(aAttr); NS_FAILED(rv)) {
    auto err = IOError(rv);

    if (rv == NS_ERROR_NOT_AVAILABLE) {
      return Err(err.WithMessage(
          "The file `%s' does not have an extended attribute `%s'",
          aFile->HumanReadablePath().get(), aAttr.get()));
    }

    return Err(IOError(rv).WithMessage(
        "Could not delete extended attribute `%s' on file `%s'", aAttr.get(),
        aFile->HumanReadablePath().get()));
  }

  return Ok{};
}

#endif

/* static */
void IOUtils::GetProfileBeforeChange(GlobalObject& aGlobal,
                                     JS::MutableHandle<JS::Value> aClient,
                                     ErrorResult& aRv) {
  return GetShutdownClient(aGlobal, aClient, aRv,
                           ShutdownPhase::ProfileBeforeChange);
}

/* static */
void IOUtils::GetSendTelemetry(GlobalObject& aGlobal,
                               JS::MutableHandle<JS::Value> aClient,
                               ErrorResult& aRv) {
  return GetShutdownClient(aGlobal, aClient, aRv, ShutdownPhase::SendTelemetry);
}

/**
 * Assert that the given phase has a shutdown client exposed by IOUtils
 *
 * There is no shutdown client exposed for XpcomWillShutdown.
 */
static void AssertHasShutdownClient(const IOUtils::ShutdownPhase aPhase) {
  MOZ_RELEASE_ASSERT(aPhase >= IOUtils::ShutdownPhase::ProfileBeforeChange &&
                     aPhase < IOUtils::ShutdownPhase::XpcomWillShutdown);
}

/* static */
void IOUtils::GetShutdownClient(GlobalObject& aGlobal,
                                JS::MutableHandle<JS::Value> aClient,
                                ErrorResult& aRv,
                                const IOUtils::ShutdownPhase aPhase) {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  AssertHasShutdownClient(aPhase);

  if (auto state = GetState()) {
    MOZ_RELEASE_ASSERT(state.ref()->mBlockerStatus !=
                       ShutdownBlockerStatus::Uninitialized);

    if (state.ref()->mBlockerStatus == ShutdownBlockerStatus::Failed) {
      aRv.ThrowAbortError("IOUtils: could not register shutdown blockers");
      return;
    }

    MOZ_RELEASE_ASSERT(state.ref()->mBlockerStatus ==
                       ShutdownBlockerStatus::Initialized);
    auto result = state.ref()->mEventQueue->GetShutdownClient(aPhase);
    if (result.isErr()) {
      aRv.ThrowAbortError("IOUtils: could not get shutdown client");
      return;
    }

    RefPtr<nsIAsyncShutdownClient> client = result.unwrap();
    MOZ_RELEASE_ASSERT(client);
    if (nsresult rv = client->GetJsclient(aClient); NS_FAILED(rv)) {
      aRv.ThrowAbortError("IOUtils: Could not get shutdown jsclient");
    }
    return;
  }

  aRv.ThrowAbortError(
      "IOUtils: profileBeforeChange phase has already finished");
}

/* sstatic */
Maybe<IOUtils::StateMutex::AutoLock> IOUtils::GetState() {
  auto state = sState.Lock();
  if (state->mQueueStatus == EventQueueStatus::Shutdown) {
    return Nothing{};
  }

  if (state->mQueueStatus == EventQueueStatus::Uninitialized) {
    MOZ_RELEASE_ASSERT(!state->mEventQueue);
    state->mEventQueue = new EventQueue();
    state->mQueueStatus = EventQueueStatus::Initialized;

    MOZ_RELEASE_ASSERT(state->mBlockerStatus ==
                       ShutdownBlockerStatus::Uninitialized);
  }

  if (NS_IsMainThread() &&
      state->mBlockerStatus == ShutdownBlockerStatus::Uninitialized) {
    state->SetShutdownHooks();
  }

  return Some(std::move(state));
}

IOUtils::EventQueue::EventQueue() {
  MOZ_ALWAYS_SUCCEEDS(NS_CreateBackgroundTaskQueue(
      "IOUtils::EventQueue", getter_AddRefs(mBackgroundEventTarget)));

  MOZ_RELEASE_ASSERT(mBackgroundEventTarget);
}

void IOUtils::State::SetShutdownHooks() {
  if (mBlockerStatus != ShutdownBlockerStatus::Uninitialized) {
    return;
  }

  if (NS_WARN_IF(NS_FAILED(mEventQueue->SetShutdownHooks()))) {
    mBlockerStatus = ShutdownBlockerStatus::Failed;
  } else {
    mBlockerStatus = ShutdownBlockerStatus::Initialized;
  }

  if (mBlockerStatus != ShutdownBlockerStatus::Initialized) {
    NS_WARNING("IOUtils: could not register shutdown blockers.");
  }
}

nsresult IOUtils::EventQueue::SetShutdownHooks() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  constexpr static auto STACK = u"IOUtils::EventQueue::SetShutdownHooks"_ns;
  constexpr static auto FILE = NS_LITERAL_STRING_FROM_CSTRING(__FILE__);

  nsCOMPtr<nsIAsyncShutdownService> svc = services::GetAsyncShutdownService();
  if (!svc) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIAsyncShutdownBlocker> profileBeforeChangeBlocker;

  // Create a shutdown blocker for the profile-before-change phase.
  {
    profileBeforeChangeBlocker =
        new IOUtilsShutdownBlocker(ShutdownPhase::ProfileBeforeChange);

    nsCOMPtr<nsIAsyncShutdownClient> globalClient;
    MOZ_TRY(svc->GetProfileBeforeChange(getter_AddRefs(globalClient)));
    MOZ_RELEASE_ASSERT(globalClient);

    MOZ_TRY(globalClient->AddBlocker(profileBeforeChangeBlocker, FILE, __LINE__,
                                     STACK));
  }

  // Create the shutdown barrier for profile-before-change so that consumers can
  // register shutdown blockers.
  //
  // The blocker we just created will wait for all clients registered on this
  // barrier to finish.
  {
    nsCOMPtr<nsIAsyncShutdownBarrier> barrier;

    // It is okay for this to fail. The created shutdown blocker won't await
    // anything and shutdown will proceed.
    MOZ_TRY(svc->MakeBarrier(
        u"IOUtils: waiting for profileBeforeChange IO to complete"_ns,
        getter_AddRefs(barrier)));
    MOZ_RELEASE_ASSERT(barrier);

    mBarriers[ShutdownPhase::ProfileBeforeChange] = std::move(barrier);
  }

  // Create a shutdown blocker for the profile-before-change-telemetry phase.
  nsCOMPtr<nsIAsyncShutdownBlocker> sendTelemetryBlocker;
  {
    sendTelemetryBlocker =
        new IOUtilsShutdownBlocker(ShutdownPhase::SendTelemetry);

    nsCOMPtr<nsIAsyncShutdownClient> globalClient;
    MOZ_TRY(svc->GetSendTelemetry(getter_AddRefs(globalClient)));
    MOZ_RELEASE_ASSERT(globalClient);

    MOZ_TRY(
        globalClient->AddBlocker(sendTelemetryBlocker, FILE, __LINE__, STACK));
  }

  // Create the shutdown barrier for profile-before-change-telemetry so that
  // consumers can register shutdown blockers.
  //
  // The blocker we just created will wait for all clients registered on this
  // barrier to finish.
  {
    nsCOMPtr<nsIAsyncShutdownBarrier> barrier;

    MOZ_TRY(svc->MakeBarrier(
        u"IOUtils: waiting for sendTelemetry IO to complete"_ns,
        getter_AddRefs(barrier)));
    MOZ_RELEASE_ASSERT(barrier);

    // Add a blocker on the previous shutdown phase.
    nsCOMPtr<nsIAsyncShutdownClient> client;
    MOZ_TRY(barrier->GetClient(getter_AddRefs(client)));

    MOZ_TRY(
        client->AddBlocker(profileBeforeChangeBlocker, FILE, __LINE__, STACK));

    mBarriers[ShutdownPhase::SendTelemetry] = std::move(barrier);
  }

  // Create a shutdown blocker for the xpcom-will-shutdown phase.
  {
    nsCOMPtr<nsIAsyncShutdownClient> globalClient;
    MOZ_TRY(svc->GetXpcomWillShutdown(getter_AddRefs(globalClient)));
    MOZ_RELEASE_ASSERT(globalClient);

    nsCOMPtr<nsIAsyncShutdownBlocker> blocker =
        new IOUtilsShutdownBlocker(ShutdownPhase::XpcomWillShutdown);
    MOZ_TRY(globalClient->AddBlocker(
        blocker, FILE, __LINE__, u"IOUtils::EventQueue::SetShutdownHooks"_ns));
  }

  // Create a shutdown barrier for the xpcom-will-shutdown phase.
  //
  // The blocker we just created will wait for all clients registered on this
  // barrier to finish.
  //
  // The only client registered on this barrier should be a blocker for the
  // previous phase. This is to ensure that all shutdown IO happens when
  // shutdown phases do not happen (e.g., in xpcshell tests where
  // profile-before-change does not occur).
  {
    nsCOMPtr<nsIAsyncShutdownBarrier> barrier;

    MOZ_TRY(svc->MakeBarrier(
        u"IOUtils: waiting for xpcomWillShutdown IO to complete"_ns,
        getter_AddRefs(barrier)));
    MOZ_RELEASE_ASSERT(barrier);

    // Add a blocker on the previous shutdown phase.
    nsCOMPtr<nsIAsyncShutdownClient> client;
    MOZ_TRY(barrier->GetClient(getter_AddRefs(client)));

    client->AddBlocker(sendTelemetryBlocker, FILE, __LINE__,
                       u"IOUtils::EventQueue::SetShutdownHooks"_ns);

    mBarriers[ShutdownPhase::XpcomWillShutdown] = std::move(barrier);
  }

  return NS_OK;
}

template <typename OkT, typename Fn>
RefPtr<IOUtils::IOPromise<OkT>> IOUtils::EventQueue::Dispatch(Fn aFunc) {
  MOZ_RELEASE_ASSERT(mBackgroundEventTarget);

  auto promise =
      MakeRefPtr<typename IOUtils::IOPromise<OkT>::Private>(__func__);
  mBackgroundEventTarget->Dispatch(
      NS_NewRunnableFunction("IOUtils::EventQueue::Dispatch",
                             [promise, func = std::move(aFunc)] {
                               Result<OkT, IOError> result = func();
                               if (result.isErr()) {
                                 promise->Reject(result.unwrapErr(), __func__);
                               } else {
                                 promise->Resolve(result.unwrap(), __func__);
                               }
                             }),
      NS_DISPATCH_EVENT_MAY_BLOCK);
  return promise;
};

Result<already_AddRefed<nsIAsyncShutdownBarrier>, nsresult>
IOUtils::EventQueue::GetShutdownBarrier(const IOUtils::ShutdownPhase aPhase) {
  if (!mBarriers[aPhase]) {
    return Err(NS_ERROR_NOT_AVAILABLE);
  }

  return do_AddRef(mBarriers[aPhase]);
}

Result<already_AddRefed<nsIAsyncShutdownClient>, nsresult>
IOUtils::EventQueue::GetShutdownClient(const IOUtils::ShutdownPhase aPhase) {
  AssertHasShutdownClient(aPhase);

  if (!mBarriers[aPhase]) {
    return Err(NS_ERROR_NOT_AVAILABLE);
  }

  nsCOMPtr<nsIAsyncShutdownClient> client;
  MOZ_TRY(mBarriers[aPhase]->GetClient(getter_AddRefs(client)));

  return do_AddRef(client);
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
Result<IOUtils::JsBuffer, IOUtils::IOError> IOUtils::MozLZ4::Decompress(
    Span<const uint8_t> aFileContents, IOUtils::BufferKind aBufferKind) {
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
    return JsBuffer::CreateEmpty(aBufferKind);
  }
  auto contents = aFileContents.From(HEADER_SIZE);
  auto result = JsBuffer::Create(aBufferKind, expectedDecompressedSize);
  if (result.isErr()) {
    return result.propagateErr();
  }

  JsBuffer decompressed = result.unwrap();
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

NS_IMPL_ISUPPORTS(IOUtilsShutdownBlocker, nsIAsyncShutdownBlocker,
                  nsIAsyncShutdownCompletionCallback);

NS_IMETHODIMP IOUtilsShutdownBlocker::GetName(nsAString& aName) {
  aName = u"IOUtils Blocker ("_ns;
  aName.Append(PHASE_NAMES[mPhase]);
  aName.Append(')');

  return NS_OK;
}

NS_IMETHODIMP IOUtilsShutdownBlocker::BlockShutdown(
    nsIAsyncShutdownClient* aBarrierClient) {
  using EventQueueStatus = IOUtils::EventQueueStatus;
  using ShutdownPhase = IOUtils::ShutdownPhase;

  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIAsyncShutdownBarrier> barrier;

  {
    auto state = IOUtils::sState.Lock();
    if (state->mQueueStatus == EventQueueStatus::Shutdown) {
      // If the previous blockers have already run, then the event queue is
      // already torn down and we have nothing to do.

      MOZ_RELEASE_ASSERT(mPhase == ShutdownPhase::XpcomWillShutdown);
      MOZ_RELEASE_ASSERT(!state->mEventQueue);

      Unused << NS_WARN_IF(NS_FAILED(aBarrierClient->RemoveBlocker(this)));
      mParentClient = nullptr;

      return NS_OK;
    }

    MOZ_RELEASE_ASSERT(state->mEventQueue);

    mParentClient = aBarrierClient;

    barrier = state->mEventQueue->GetShutdownBarrier(mPhase).unwrapOr(nullptr);
  }

  // We cannot barrier->Wait() while holding the mutex because it will lead to
  // deadlock.
  if (!barrier || NS_WARN_IF(NS_FAILED(barrier->Wait(this)))) {
    // If we don't have a barrier, we still need to flush the IOUtils event
    // queue and disable task submission.
    //
    // Likewise, if waiting on the barrier failed, we are going to make our best
    // attempt to clean up.
    Unused << Done();
  }

  return NS_OK;
}

NS_IMETHODIMP IOUtilsShutdownBlocker::Done() {
  using EventQueueStatus = IOUtils::EventQueueStatus;
  using ShutdownPhase = IOUtils::ShutdownPhase;

  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  bool didFlush = false;

  {
    auto state = IOUtils::sState.Lock();

    if (state->mEventQueue) {
      MOZ_RELEASE_ASSERT(state->mQueueStatus == EventQueueStatus::Initialized);

      // This method is called once we have served all shutdown clients. Now we
      // flush the remaining IO queue. This ensures any straggling IO that was
      // not part of the shutdown blocker finishes before we move to the next
      // phase.
      state->mEventQueue->Dispatch<Ok>([]() { return Ok{}; })
          ->Then(GetMainThreadSerialEventTarget(), __func__,
                 [self = RefPtr(this)]() { self->OnFlush(); });

      // And if we're the last shutdown phase to allow IO, disable the event
      // queue to disallow further IO requests.
      if (mPhase >= LAST_IO_PHASE) {
        state->mQueueStatus = EventQueueStatus::Shutdown;
      }

      didFlush = true;
    }
  }

  // If we have already shut down the event loop, then call OnFlush to stop
  // blocking our parent shutdown client.
  if (!didFlush) {
    MOZ_RELEASE_ASSERT(mPhase == ShutdownPhase::XpcomWillShutdown);
    OnFlush();
  }

  return NS_OK;
}

void IOUtilsShutdownBlocker::OnFlush() {
  if (mParentClient) {
    (void)NS_WARN_IF(NS_FAILED(mParentClient->RemoveBlocker(this)));
    mParentClient = nullptr;

    // If we are past the last shutdown phase that allows IO,
    // we can shutdown the event queue here because no additional IO requests
    // will be allowed (see |Done()|).
    if (mPhase >= LAST_IO_PHASE) {
      auto state = IOUtils::sState.Lock();
      if (state->mEventQueue) {
        state->mEventQueue = nullptr;
      }
    }
  }
}

NS_IMETHODIMP IOUtilsShutdownBlocker::GetState(nsIPropertyBag** aState) {
  return NS_OK;
}

Result<IOUtils::InternalWriteOpts, IOUtils::IOError>
IOUtils::InternalWriteOpts::FromBinding(const WriteOptions& aOptions) {
  InternalWriteOpts opts;
  opts.mFlush = aOptions.mFlush;
  opts.mMode = aOptions.mMode;

  if (aOptions.mBackupFile.WasPassed()) {
    opts.mBackupFile = new nsLocalFile();
    if (nsresult rv = PathUtils::InitFileWithPath(opts.mBackupFile,
                                                  aOptions.mBackupFile.Value());
        NS_FAILED(rv)) {
      return Err(IOUtils::IOError(rv).WithMessage(
          "Could not parse path of backupFile (%s)",
          NS_ConvertUTF16toUTF8(aOptions.mBackupFile.Value()).get()));
    }
  }

  if (aOptions.mTmpPath.WasPassed()) {
    opts.mTmpFile = new nsLocalFile();
    if (nsresult rv = PathUtils::InitFileWithPath(opts.mTmpFile,
                                                  aOptions.mTmpPath.Value());
        NS_FAILED(rv)) {
      return Err(IOUtils::IOError(rv).WithMessage(
          "Could not parse path of temp file (%s)",
          NS_ConvertUTF16toUTF8(aOptions.mTmpPath.Value()).get()));
    }
  }

  opts.mCompress = aOptions.mCompress;
  return opts;
}

/* static */
Result<IOUtils::JsBuffer, IOUtils::IOError> IOUtils::JsBuffer::Create(
    IOUtils::BufferKind aBufferKind, size_t aCapacity) {
  JsBuffer buffer(aBufferKind, aCapacity);
  if (aCapacity != 0 && !buffer.mBuffer) {
    return Err(IOError(NS_ERROR_OUT_OF_MEMORY)
                   .WithMessage("Could not allocate buffer"));
  }
  return buffer;
}

/* static */
IOUtils::JsBuffer IOUtils::JsBuffer::CreateEmpty(
    IOUtils::BufferKind aBufferKind) {
  JsBuffer buffer(aBufferKind, 0);
  MOZ_RELEASE_ASSERT(buffer.mBuffer == nullptr);
  return buffer;
}

IOUtils::JsBuffer::JsBuffer(IOUtils::BufferKind aBufferKind, size_t aCapacity)
    : mBufferKind(aBufferKind), mCapacity(aCapacity), mLength(0) {
  if (mCapacity) {
    if (aBufferKind == BufferKind::String) {
      mBuffer = JS::UniqueChars(
          js_pod_arena_malloc<char>(js::StringBufferArena, mCapacity));
    } else {
      MOZ_RELEASE_ASSERT(aBufferKind == BufferKind::Uint8Array);
      mBuffer = JS::UniqueChars(
          js_pod_arena_malloc<char>(js::ArrayBufferContentsArena, mCapacity));
    }
  }
}

IOUtils::JsBuffer::JsBuffer(IOUtils::JsBuffer&& aOther) noexcept
    : mBufferKind(aOther.mBufferKind),
      mCapacity(aOther.mCapacity),
      mLength(aOther.mLength),
      mBuffer(std::move(aOther.mBuffer)) {
  aOther.mCapacity = 0;
  aOther.mLength = 0;
}

IOUtils::JsBuffer& IOUtils::JsBuffer::operator=(
    IOUtils::JsBuffer&& aOther) noexcept {
  mBufferKind = aOther.mBufferKind;
  mCapacity = aOther.mCapacity;
  mLength = aOther.mLength;
  mBuffer = std::move(aOther.mBuffer);

  // Invalidate aOther.
  aOther.mCapacity = 0;
  aOther.mLength = 0;

  return *this;
}

/* static */
JSString* IOUtils::JsBuffer::IntoString(JSContext* aCx, JsBuffer aBuffer) {
  MOZ_RELEASE_ASSERT(aBuffer.mBufferKind == IOUtils::BufferKind::String);

  if (!aBuffer.mCapacity) {
    return JS_GetEmptyString(aCx);
  }

  if (IsAscii(aBuffer.BeginReading())) {
    // If the string is just plain ASCII, then we can hand the buffer off to
    // JavaScript as a Latin1 string (since ASCII is a subset of Latin1).
    JS::UniqueLatin1Chars asLatin1(
        reinterpret_cast<JS::Latin1Char*>(aBuffer.mBuffer.release()));
    return JS_NewLatin1String(aCx, std::move(asLatin1), aBuffer.mLength);
  }

  const char* ptr = aBuffer.mBuffer.get();
  size_t length = aBuffer.mLength;

  // Strip off a leading UTF-8 byte order marker (BOM) if found.
  if (length >= 3 && Substring(ptr, 3) == "\xEF\xBB\xBF"_ns) {
    ptr += 3;
    length -= 3;
  }

  // If the string is encodable as Latin1, we need to deflate the string to a
  // Latin1 string to account for UTF-8 characters that are encoded as more than
  // a single byte.
  //
  // Otherwise, the string contains characters outside Latin1 so we have to
  // inflate to UTF-16.
  return JS_NewStringCopyUTF8N(aCx, JS::UTF8Chars(ptr, length));
}

/* static */
JSObject* IOUtils::JsBuffer::IntoUint8Array(JSContext* aCx, JsBuffer aBuffer) {
  MOZ_RELEASE_ASSERT(aBuffer.mBufferKind == IOUtils::BufferKind::Uint8Array);

  if (!aBuffer.mCapacity) {
    return JS_NewUint8Array(aCx, 0);
  }

  MOZ_RELEASE_ASSERT(aBuffer.mBuffer);
  JS::Rooted<JSObject*> arrayBuffer(
      aCx, JS::NewArrayBufferWithContents(aCx, aBuffer.mLength,
                                          std::move(aBuffer.mBuffer)));

  if (!arrayBuffer) {
    // aBuffer will be destructed at end of scope, but its destructor does not
    // take into account |mCapacity| or |mLength|, so it is OK for them to be
    // non-zero here with a null |mBuffer|.
    return nullptr;
  }

  return JS_NewUint8ArrayWithBuffer(aCx, arrayBuffer, 0, aBuffer.mLength);
}

[[nodiscard]] bool ToJSValue(JSContext* aCx, IOUtils::JsBuffer&& aBuffer,
                             JS::MutableHandle<JS::Value> aValue) {
  if (aBuffer.mBufferKind == IOUtils::BufferKind::String) {
    JSString* str = IOUtils::JsBuffer::IntoString(aCx, std::move(aBuffer));
    if (!str) {
      return false;
    }

    aValue.setString(str);
    return true;
  }

  JSObject* array = IOUtils::JsBuffer::IntoUint8Array(aCx, std::move(aBuffer));
  if (!array) {
    return false;
  }

  aValue.setObject(*array);
  return true;
}

// SyncReadFile

NS_IMPL_CYCLE_COLLECTING_ADDREF(SyncReadFile)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SyncReadFile)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SyncReadFile)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(SyncReadFile, mParent)

SyncReadFile::SyncReadFile(nsISupports* aParent,
                           RefPtr<nsFileRandomAccessStream>&& aStream,
                           int64_t aSize)
    : mParent(aParent), mStream(std::move(aStream)), mSize(aSize) {
  MOZ_RELEASE_ASSERT(mSize >= 0);
}

SyncReadFile::~SyncReadFile() = default;

JSObject* SyncReadFile::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return SyncReadFile_Binding::Wrap(aCx, this, aGivenProto);
}

void SyncReadFile::ReadBytesInto(const Uint8Array& aDestArray,
                                 const int64_t aOffset, ErrorResult& aRv) {
  if (!mStream) {
    return aRv.ThrowOperationError("SyncReadFile is closed");
  }

  aDestArray.ProcessFixedData([&](const Span<uint8_t>& aData) {
    auto rangeEnd = CheckedInt64(aOffset) + aData.Length();
    if (!rangeEnd.isValid()) {
      return aRv.ThrowOperationError("Requested range overflows i64");
    }

    if (rangeEnd.value() > mSize) {
      return aRv.ThrowOperationError(
          "Requested range overflows SyncReadFile size");
    }

    size_t readLen{aData.Length()};
    if (readLen == 0) {
      return;
    }

    if (nsresult rv = mStream->Seek(PR_SEEK_SET, aOffset); NS_FAILED(rv)) {
      return aRv.ThrowOperationError(
          FormatErrorMessage(rv, "Could not seek to position %lld", aOffset));
    }

    Span<char> toRead = AsWritableChars(aData);

    size_t totalRead = 0;
    while (totalRead != readLen) {
      // Read no more than INT32_MAX on each call to mStream->Read,
      // otherwise it returns an error.
      uint32_t bytesToReadThisChunk =
          std::min(readLen - totalRead, size_t(INT32_MAX));

      uint32_t bytesRead = 0;
      if (nsresult rv = mStream->Read(toRead.Elements(), bytesToReadThisChunk,
                                      &bytesRead);
          NS_FAILED(rv)) {
        return aRv.ThrowOperationError(FormatErrorMessage(
            rv, "Encountered an unexpected error while reading file stream"));
      }
      if (bytesRead == 0) {
        return aRv.ThrowOperationError(
            "Reading stopped before the entire array was filled");
      }
      totalRead += bytesRead;
      toRead = toRead.From(bytesRead);
    }
  });
}

void SyncReadFile::Close() { mStream = nullptr; }

#ifdef XP_UNIX
namespace {

static nsCString FromUnixString(const IOUtils::UnixString& aString) {
  if (aString.IsUTF8String()) {
    return aString.GetAsUTF8String();
  }
  if (aString.IsUint8Array()) {
    nsCString data;
    Unused << aString.GetAsUint8Array().AppendDataTo(data);
    return data;
  }
  MOZ_CRASH("unreachable");
}

}  // namespace

// static
uint32_t IOUtils::LaunchProcess(GlobalObject& aGlobal,
                                const Sequence<UnixString>& aArgv,
                                const LaunchOptions& aOptions,
                                ErrorResult& aRv) {
  // The binding is worker-only, so should always be off-main-thread.
  MOZ_ASSERT(!NS_IsMainThread());

  // This generally won't work in child processes due to sandboxing.
  AssertParentProcessWithCallerLocation(aGlobal);

  std::vector<std::string> argv;
  base::LaunchOptions options;

  for (const auto& arg : aArgv) {
    argv.push_back(FromUnixString(arg).get());
  }

  size_t envLen = aOptions.mEnvironment.Length();
  base::EnvironmentArray envp(new char*[envLen + 1]);
  for (size_t i = 0; i < envLen; ++i) {
    // EnvironmentArray is a UniquePtr instance which will `free`
    // these strings.
    envp[i] = strdup(FromUnixString(aOptions.mEnvironment[i]).get());
  }
  envp[envLen] = nullptr;
  options.full_env = std::move(envp);

  if (aOptions.mWorkdir.WasPassed()) {
    options.workdir = FromUnixString(aOptions.mWorkdir.Value()).get();
  }

  if (aOptions.mFdMap.WasPassed()) {
    for (const auto& fdItem : aOptions.mFdMap.Value()) {
      options.fds_to_remap.push_back({fdItem.mSrc, fdItem.mDst});
    }
  }

#  ifdef XP_MACOSX
  options.disclaim = aOptions.mDisclaim;
#  endif

  base::ProcessHandle pid;
  static_assert(sizeof(pid) <= sizeof(uint32_t),
                "WebIDL long should be large enough for a pid");
  Result<Ok, mozilla::ipc::LaunchError> err =
      base::LaunchApp(argv, std::move(options), &pid);
  if (err.isErr()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return 0;
  }

  MOZ_ASSERT(pid >= 0);
  return static_cast<uint32_t>(pid);
}
#endif  // XP_UNIX

}  // namespace mozilla::dom

#undef REJECT_IF_INIT_PATH_FAILED

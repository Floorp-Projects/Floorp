/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ActorsChild.h"

#include "BackgroundChildImpl.h"
#include "FileHandleBase.h"
#include "FileRequestBase.h"
#include "js/Date.h"
#include "mozilla/dom/EncodingUtils.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/ipc/BlobChild.h"
#include "MutableFileBase.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsString.h"
#include "xpcpublic.h"
#include "mozilla/dom/BindingUtils.h"

namespace mozilla {
namespace dom {

/*******************************************************************************
 * Helpers
 ******************************************************************************/

namespace {

class MOZ_STACK_CLASS AutoSetCurrentFileHandle final
{
  typedef mozilla::ipc::BackgroundChildImpl BackgroundChildImpl;

  FileHandleBase* const mFileHandle;
  FileHandleBase* mPreviousFileHandle;
  FileHandleBase** mThreadLocalSlot;

public:
  explicit AutoSetCurrentFileHandle(FileHandleBase* aFileHandle)
    : mFileHandle(aFileHandle)
    , mPreviousFileHandle(nullptr)
    , mThreadLocalSlot(nullptr)
  {
    if (aFileHandle) {
      BackgroundChildImpl::ThreadLocal* threadLocal =
        BackgroundChildImpl::GetThreadLocalForCurrentThread();
      MOZ_ASSERT(threadLocal);

      // Hang onto this location for resetting later.
      mThreadLocalSlot = &threadLocal->mCurrentFileHandle;

      // Save the current value.
      mPreviousFileHandle = *mThreadLocalSlot;

      // Set the new value.
      *mThreadLocalSlot = aFileHandle;
    }
  }

  ~AutoSetCurrentFileHandle()
  {
    MOZ_ASSERT_IF(mThreadLocalSlot, mFileHandle);
    MOZ_ASSERT_IF(mThreadLocalSlot, *mThreadLocalSlot == mFileHandle);

    if (mThreadLocalSlot) {
      // Reset old value.
      *mThreadLocalSlot = mPreviousFileHandle;
    }
  }

  FileHandleBase*
  FileHandle() const
  {
    return mFileHandle;
  }
};

class MOZ_STACK_CLASS ResultHelper final
  : public FileRequestBase::ResultCallback
{
  FileRequestBase* mFileRequest;
  AutoSetCurrentFileHandle mAutoFileHandle;

  union
  {
    File* mFile;
    const nsCString* mString;
    const FileRequestMetadata* mMetadata;
    const JS::Handle<JS::Value>* mJSValHandle;
  } mResult;

  enum
  {
    ResultTypeFile,
    ResultTypeString,
    ResultTypeMetadata,
    ResultTypeJSValHandle,
  } mResultType;

public:
  ResultHelper(FileRequestBase* aFileRequest,
               FileHandleBase* aFileHandle,
               File* aResult)
    : mFileRequest(aFileRequest)
    , mAutoFileHandle(aFileHandle)
    , mResultType(ResultTypeFile)
  {
    MOZ_ASSERT(aFileRequest);
    MOZ_ASSERT(aFileHandle);
    MOZ_ASSERT(aResult);

    mResult.mFile = aResult;
  }

  ResultHelper(FileRequestBase* aFileRequest,
               FileHandleBase* aFileHandle,
               const nsCString* aResult)
    : mFileRequest(aFileRequest)
    , mAutoFileHandle(aFileHandle)
    , mResultType(ResultTypeString)
  {
    MOZ_ASSERT(aFileRequest);
    MOZ_ASSERT(aFileHandle);
    MOZ_ASSERT(aResult);

    mResult.mString = aResult;
  }

  ResultHelper(FileRequestBase* aFileRequest,
               FileHandleBase* aFileHandle,
               const FileRequestMetadata* aResult)
    : mFileRequest(aFileRequest)
    , mAutoFileHandle(aFileHandle)
    , mResultType(ResultTypeMetadata)
  {
    MOZ_ASSERT(aFileRequest);
    MOZ_ASSERT(aFileHandle);
    MOZ_ASSERT(aResult);

    mResult.mMetadata = aResult;
  }


  ResultHelper(FileRequestBase* aFileRequest,
               FileHandleBase* aFileHandle,
               const JS::Handle<JS::Value>* aResult)
    : mFileRequest(aFileRequest)
    , mAutoFileHandle(aFileHandle)
    , mResultType(ResultTypeJSValHandle)
  {
    MOZ_ASSERT(aFileRequest);
    MOZ_ASSERT(aFileHandle);
    MOZ_ASSERT(aResult);

    mResult.mJSValHandle = aResult;
  }

  FileRequestBase*
  FileRequest() const
  {
    return mFileRequest;
  }

  FileHandleBase*
  FileHandle() const
  {
    return mAutoFileHandle.FileHandle();
  }

  virtual nsresult
  GetResult(JSContext* aCx, JS::MutableHandle<JS::Value> aResult) override
  {
    MOZ_ASSERT(aCx);
    MOZ_ASSERT(mFileRequest);

    switch (mResultType) {
      case ResultTypeFile:
        return GetResult(aCx, mResult.mFile, aResult);

      case ResultTypeString:
        return GetResult(aCx, mResult.mString, aResult);

      case ResultTypeMetadata:
        return GetResult(aCx, mResult.mMetadata, aResult);

      case ResultTypeJSValHandle:
        aResult.set(*mResult.mJSValHandle);
        return NS_OK;

      default:
        MOZ_CRASH("Unknown result type!");
    }

    MOZ_CRASH("Should never get here!");
  }

private:
  nsresult
  GetResult(JSContext* aCx,
            File* aFile,
            JS::MutableHandle<JS::Value> aResult)
  {
    bool ok = GetOrCreateDOMReflector(aCx, aFile, aResult);
    if (NS_WARN_IF(!ok)) {
      return NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR;
    }

    return NS_OK;
  }

  nsresult
  GetResult(JSContext* aCx,
            const nsCString* aString,
            JS::MutableHandle<JS::Value> aResult)
  {
    const nsCString& data = *aString;

    nsresult rv;

    if (!mFileRequest->HasEncoding()) {
      JS::Rooted<JSObject*> arrayBuffer(aCx);
      rv = nsContentUtils::CreateArrayBuffer(aCx, data, arrayBuffer.address());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR;
      }

      aResult.setObject(*arrayBuffer);
      return NS_OK;
    }

    nsAutoCString encoding;
    // The BOM sniffing is baked into the "decode" part of the Encoding
    // Standard, which the File API references.
    if (!nsContentUtils::CheckForBOM(
          reinterpret_cast<const unsigned char *>(data.get()),
          data.Length(),
          encoding)) {
      // BOM sniffing failed. Try the API argument.
      if (!EncodingUtils::FindEncodingForLabel(mFileRequest->GetEncoding(),
                                               encoding)) {
        // API argument failed. Since we are dealing with a file system file,
        // we don't have a meaningful type attribute for the blob available,
        // so proceeding to the next step, which is defaulting to UTF-8.
        encoding.AssignLiteral("UTF-8");
      }
    }

    nsString tmpString;
    rv = nsContentUtils::ConvertStringFromEncoding(encoding, data, tmpString);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR;
    }

    if (NS_WARN_IF(!xpc::StringToJsval(aCx, tmpString, aResult))) {
      return NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR;
    }

    return NS_OK;
  }

  nsresult
  GetResult(JSContext* aCx,
            const FileRequestMetadata* aMetadata,
            JS::MutableHandle<JS::Value> aResult)
  {
    JS::Rooted<JSObject*> obj(aCx, JS_NewPlainObject(aCx));
    if (NS_WARN_IF(!obj)) {
      return NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR;
    }

    const FileRequestSize& size = aMetadata->size();
    if (size.type() != FileRequestSize::Tvoid_t) {
      MOZ_ASSERT(size.type() == FileRequestSize::Tuint64_t);

      JS::Rooted<JS::Value> number(aCx, JS_NumberValue(size.get_uint64_t()));

      if (NS_WARN_IF(!JS_DefineProperty(aCx, obj, "size", number, 0))) {
        return NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR;
      }
    }

    const FileRequestLastModified& lastModified = aMetadata->lastModified();
    if (lastModified.type() != FileRequestLastModified::Tvoid_t) {
      MOZ_ASSERT(lastModified.type() == FileRequestLastModified::Tint64_t);

      JS::Rooted<JSObject*> date(aCx,
        JS::NewDateObject(aCx, JS::TimeClip(lastModified.get_int64_t())));
      if (NS_WARN_IF(!date)) {
        return NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR;
      }

      if (NS_WARN_IF(!JS_DefineProperty(aCx, obj, "lastModified", date, 0))) {
        return NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR;
      }
    }

    aResult.setObject(*obj);
    return NS_OK;
  }
};

already_AddRefed<File>
ConvertActorToFile(FileHandleBase* aFileHandle,
                   const FileRequestGetFileResponse& aResponse)
{
  auto* actor = static_cast<BlobChild*>(aResponse.fileChild());

  MutableFileBase* mutableFile = aFileHandle->MutableFile();
  MOZ_ASSERT(mutableFile);

  const FileRequestMetadata& metadata = aResponse.metadata();

  const FileRequestSize& size = metadata.size();
  MOZ_ASSERT(size.type() == FileRequestSize::Tuint64_t);

  const FileRequestLastModified& lastModified = metadata.lastModified();
  MOZ_ASSERT(lastModified.type() == FileRequestLastModified::Tint64_t);

  actor->SetMysteryBlobInfo(mutableFile->Name(),
                            mutableFile->Type(),
                            size.get_uint64_t(),
                            lastModified.get_int64_t());

  RefPtr<BlobImpl> blobImpl = actor->GetBlobImpl();
  MOZ_ASSERT(blobImpl);

  RefPtr<File> file = mutableFile->CreateFileFor(blobImpl, aFileHandle);
  return file.forget();
}

void
HandleSuccess(ResultHelper* aResultHelper)
{
  MOZ_ASSERT(aResultHelper);

  RefPtr<FileRequestBase> fileRequest = aResultHelper->FileRequest();
  MOZ_ASSERT(fileRequest);
  fileRequest->AssertIsOnOwningThread();

  RefPtr<FileHandleBase> fileHandle = aResultHelper->FileHandle();
  MOZ_ASSERT(fileHandle);

  if (fileHandle->IsAborted()) {
    fileRequest->SetError(NS_ERROR_DOM_FILEHANDLE_ABORT_ERR);
    return;
  }

  MOZ_ASSERT(fileHandle->IsOpen());

  fileRequest->SetResultCallback(aResultHelper);

  MOZ_ASSERT(fileHandle->IsOpen() || fileHandle->IsAborted());
}

void
HandleError(FileRequestBase* aFileRequest,
            nsresult aErrorCode,
            FileHandleBase* aFileHandle)
{
  MOZ_ASSERT(aFileRequest);
  aFileRequest->AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aErrorCode));
  MOZ_ASSERT(NS_ERROR_GET_MODULE(aErrorCode) == NS_ERROR_MODULE_DOM_FILEHANDLE);
  MOZ_ASSERT(aFileHandle);

  RefPtr<FileRequestBase> fileRequest = aFileRequest;
  RefPtr<FileHandleBase> fileHandle = aFileHandle;

  AutoSetCurrentFileHandle ascfh(aFileHandle);

  fileRequest->SetError(aErrorCode);

  MOZ_ASSERT(fileHandle->IsOpen() || fileHandle->IsAborted());
}

} // anonymous namespace

/*******************************************************************************
 * BackgroundMutableFileChildBase
 ******************************************************************************/

BackgroundMutableFileChildBase::BackgroundMutableFileChildBase(
                                             DEBUGONLY(PRThread* aOwningThread))
  : ThreadObject(DEBUGONLY(aOwningThread))
  , mMutableFile(nullptr)
{
  AssertIsOnOwningThread();

  MOZ_COUNT_CTOR(BackgroundMutableFileChildBase);
}

BackgroundMutableFileChildBase::~BackgroundMutableFileChildBase()
{
  AssertIsOnOwningThread();

  MOZ_COUNT_DTOR(BackgroundMutableFileChildBase);
}

void
BackgroundMutableFileChildBase::EnsureDOMObject()
{
  AssertIsOnOwningThread();

  if (mTemporaryStrongMutableFile) {
    return;
  }

  mTemporaryStrongMutableFile = CreateMutableFile();

  MOZ_ASSERT(mTemporaryStrongMutableFile);
  mTemporaryStrongMutableFile->AssertIsOnOwningThread();

  mMutableFile = mTemporaryStrongMutableFile;
}

void
BackgroundMutableFileChildBase::ReleaseDOMObject()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mTemporaryStrongMutableFile);
  mTemporaryStrongMutableFile->AssertIsOnOwningThread();
  MOZ_ASSERT(mMutableFile == mTemporaryStrongMutableFile);

  mTemporaryStrongMutableFile = nullptr;
}

void
BackgroundMutableFileChildBase::SendDeleteMeInternal()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mTemporaryStrongMutableFile);

  if (mMutableFile) {
    mMutableFile->ClearBackgroundActor();
    mMutableFile = nullptr;

    MOZ_ALWAYS_TRUE(PBackgroundMutableFileChild::SendDeleteMe());
  }
}

void
BackgroundMutableFileChildBase::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnOwningThread();

  if (mMutableFile) {
    mMutableFile->ClearBackgroundActor();
    DEBUGONLY(mMutableFile = nullptr;)
  }
}

PBackgroundFileHandleChild*
BackgroundMutableFileChildBase::AllocPBackgroundFileHandleChild(
                                                          const FileMode& aMode)
{
  MOZ_CRASH("PBackgroundFileHandleChild actors should be manually "
            "constructed!");
}

bool
BackgroundMutableFileChildBase::DeallocPBackgroundFileHandleChild(
                                             PBackgroundFileHandleChild* aActor)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aActor);

  delete static_cast<BackgroundFileHandleChild*>(aActor);
  return true;
}

/*******************************************************************************
 * BackgroundFileHandleChild
 ******************************************************************************/

BackgroundFileHandleChild::BackgroundFileHandleChild(
                                             DEBUGONLY(PRThread* aOwningThread,)
                                             FileHandleBase* aFileHandle)
  : ThreadObject(DEBUGONLY(aOwningThread))
  , mTemporaryStrongFileHandle(aFileHandle)
  , mFileHandle(aFileHandle)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aFileHandle);
  aFileHandle->AssertIsOnOwningThread();

  MOZ_COUNT_CTOR(BackgroundFileHandleChild);
}

BackgroundFileHandleChild::~BackgroundFileHandleChild()
{
  AssertIsOnOwningThread();

  MOZ_COUNT_DTOR(BackgroundFileHandleChild);
}

void
BackgroundFileHandleChild::SendDeleteMeInternal()
{
  AssertIsOnOwningThread();

  if (mFileHandle) {
    NoteActorDestroyed();

    MOZ_ALWAYS_TRUE(PBackgroundFileHandleChild::SendDeleteMe());
  }
}

void
BackgroundFileHandleChild::NoteActorDestroyed()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT_IF(mTemporaryStrongFileHandle, mFileHandle);

  if (mFileHandle) {
    mFileHandle->ClearBackgroundActor();

    // Normally this would be DEBUG-only but NoteActorDestroyed is also called
    // from SendDeleteMeInternal. In that case we're going to receive an actual
    // ActorDestroy call later and we don't want to touch a dead object.
    mTemporaryStrongFileHandle = nullptr;
    mFileHandle = nullptr;
  }
}

void
BackgroundFileHandleChild::NoteComplete()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT_IF(mFileHandle, mTemporaryStrongFileHandle);

  mTemporaryStrongFileHandle = nullptr;
}

void
BackgroundFileHandleChild::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnOwningThread();

  NoteActorDestroyed();
}

mozilla::ipc::IPCResult
BackgroundFileHandleChild::RecvComplete(const bool& aAborted)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mFileHandle);

  mFileHandle->HandleCompleteOrAbort(aAborted);

  NoteComplete();
  return IPC_OK();
}

PBackgroundFileRequestChild*
BackgroundFileHandleChild::AllocPBackgroundFileRequestChild(
                                               const FileRequestParams& aParams)
{
  MOZ_CRASH("PBackgroundFileRequestChild actors should be manually "
            "constructed!");
}

bool
BackgroundFileHandleChild::DeallocPBackgroundFileRequestChild(
                                            PBackgroundFileRequestChild* aActor)
{
  MOZ_ASSERT(aActor);

  delete static_cast<BackgroundFileRequestChild*>(aActor);
  return true;
}

/*******************************************************************************
 * BackgroundFileRequestChild
 ******************************************************************************/

BackgroundFileRequestChild::BackgroundFileRequestChild(
                                             DEBUGONLY(PRThread* aOwningThread,)
                                             FileRequestBase* aFileRequest)
  : ThreadObject(DEBUGONLY(aOwningThread))
  , mFileRequest(aFileRequest)
  , mFileHandle(aFileRequest->FileHandle())
  , mActorDestroyed(false)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aFileRequest);
  aFileRequest->AssertIsOnOwningThread();
  MOZ_ASSERT(mFileHandle);
  mFileHandle->AssertIsOnOwningThread();

  MOZ_COUNT_CTOR(BackgroundFileRequestChild);
}

BackgroundFileRequestChild::~BackgroundFileRequestChild()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mFileHandle);

  MOZ_COUNT_DTOR(BackgroundFileRequestChild);
}

void
BackgroundFileRequestChild::HandleResponse(nsresult aResponse)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aResponse));
  MOZ_ASSERT(NS_ERROR_GET_MODULE(aResponse) == NS_ERROR_MODULE_DOM_FILEHANDLE);
  MOZ_ASSERT(mFileHandle);

  HandleError(mFileRequest, aResponse, mFileHandle);
}

void
BackgroundFileRequestChild::HandleResponse(
                                    const FileRequestGetFileResponse& aResponse)
{
  AssertIsOnOwningThread();

  RefPtr<File> file = ConvertActorToFile(mFileHandle, aResponse);

  ResultHelper helper(mFileRequest, mFileHandle, file);

  HandleSuccess(&helper);
}

void
BackgroundFileRequestChild::HandleResponse(const nsCString& aResponse)
{
  AssertIsOnOwningThread();

  ResultHelper helper(mFileRequest, mFileHandle, &aResponse);

  HandleSuccess(&helper);
}

void
BackgroundFileRequestChild::HandleResponse(const FileRequestMetadata& aResponse)
{
  AssertIsOnOwningThread();

  ResultHelper helper(mFileRequest, mFileHandle, &aResponse);

  HandleSuccess(&helper);
}

void
BackgroundFileRequestChild::HandleResponse(JS::Handle<JS::Value> aResponse)
{
  AssertIsOnOwningThread();

  ResultHelper helper(mFileRequest, mFileHandle, &aResponse);

  HandleSuccess(&helper);
}

void
BackgroundFileRequestChild::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnOwningThread();

  MOZ_ASSERT(!mActorDestroyed);

  mActorDestroyed = true;

  if (mFileHandle) {
    mFileHandle->AssertIsOnOwningThread();

    mFileHandle->OnRequestFinished(/* aActorDestroyedNormally */
                                   aWhy == Deletion);

    DEBUGONLY(mFileHandle = nullptr;)
  }
}

mozilla::ipc::IPCResult
BackgroundFileRequestChild::Recv__delete__(const FileRequestResponse& aResponse)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mFileRequest);
  MOZ_ASSERT(mFileHandle);

  if (mFileHandle->IsAborted()) {
    // Always handle an "error" with ABORT_ERR if the file handle was aborted,
    // even if the request succeeded or failed with another error.
    HandleResponse(NS_ERROR_DOM_FILEHANDLE_ABORT_ERR);
  } else {
    switch (aResponse.type()) {
      case FileRequestResponse::Tnsresult:
        HandleResponse(aResponse.get_nsresult());
        break;

      case FileRequestResponse::TFileRequestGetFileResponse:
        HandleResponse(aResponse.get_FileRequestGetFileResponse());
        break;

      case FileRequestResponse::TFileRequestReadResponse:
        HandleResponse(aResponse.get_FileRequestReadResponse().data());
        break;

      case FileRequestResponse::TFileRequestWriteResponse:
        HandleResponse(JS::UndefinedHandleValue);
        break;

      case FileRequestResponse::TFileRequestTruncateResponse:
        HandleResponse(JS::UndefinedHandleValue);
        break;

      case FileRequestResponse::TFileRequestFlushResponse:
        HandleResponse(JS::UndefinedHandleValue);
        break;

      case FileRequestResponse::TFileRequestGetMetadataResponse:
        HandleResponse(aResponse.get_FileRequestGetMetadataResponse()
                                .metadata());
        break;

      default:
        MOZ_CRASH("Unknown response type!");
    }
  }

  mFileHandle->OnRequestFinished(/* aActorDestroyedNormally */ true);

  // Null this out so that we don't try to call OnRequestFinished() again in
  // ActorDestroy.
  mFileHandle = nullptr;

  return IPC_OK();
}

mozilla::ipc::IPCResult
BackgroundFileRequestChild::RecvProgress(const uint64_t& aProgress,
                                         const uint64_t& aProgressMax)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mFileRequest);

  mFileRequest->OnProgress(aProgress, aProgressMax);

  return IPC_OK();
}

} // namespace dom
} // namespace mozilla

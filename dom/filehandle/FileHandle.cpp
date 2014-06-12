/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileHandle.h"

#include "AsyncHelper.h"
#include "FileHelper.h"
#include "FileRequest.h"
#include "FileService.h"
#include "FileStreamWrappers.h"
#include "MemoryStreams.h"
#include "MetadataHelper.h"
#include "mozilla/dom/EncodingUtils.h"
#include "mozilla/dom/FileHandleBinding.h"
#include "mozilla/EventDispatcher.h"
#include "MutableFile.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIAppShell.h"
#include "nsIDOMEvent.h"
#include "nsIDOMFile.h"
#include "nsIEventTarget.h"
#include "nsISeekableStream.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsStringStream.h"
#include "nsThreadUtils.h"
#include "nsWidgetsCID.h"

#define STREAM_COPY_BLOCK_SIZE 32768

namespace mozilla {
namespace dom {

namespace {

NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

class ReadHelper : public FileHelper
{
public:
  ReadHelper(FileHandle* aFileHandle,
             FileRequest* aFileRequest,
             uint64_t aLocation,
             uint64_t aSize)
  : FileHelper(aFileHandle, aFileRequest),
    mLocation(aLocation), mSize(aSize)
  {
    MOZ_ASSERT(mSize, "Passed zero size!");
  }

  nsresult
  Init();

  nsresult
  DoAsyncRun(nsISupports* aStream) MOZ_OVERRIDE;

  nsresult
  GetSuccessResult(JSContext* aCx,
                   JS::MutableHandle<JS::Value> aVal) MOZ_OVERRIDE;

protected:
  uint64_t mLocation;
  uint64_t mSize;

  nsRefPtr<MemoryOutputStream> mStream;
};

class ReadTextHelper : public ReadHelper
{
public:
  ReadTextHelper(FileHandle* aFileHandle,
                 FileRequest* aFileRequest,
                 uint64_t aLocation,
                 uint64_t aSize,
                 const nsAString& aEncoding)
  : ReadHelper(aFileHandle, aFileRequest, aLocation, aSize),
    mEncoding(aEncoding)
  { }

  nsresult
  GetSuccessResult(JSContext* aCx,
                   JS::MutableHandle<JS::Value> aVal) MOZ_OVERRIDE;

private:
  nsString mEncoding;
};

class WriteHelper : public FileHelper
{
public:
  WriteHelper(FileHandle* aFileHandle,
              FileRequest* aFileRequest,
              uint64_t aLocation,
              nsIInputStream* aStream,
              uint64_t aLength)
  : FileHelper(aFileHandle, aFileRequest),
    mLocation(aLocation), mStream(aStream), mLength(aLength)
  {
    MOZ_ASSERT(mLength, "Passed zero length!");
  }

  nsresult
  DoAsyncRun(nsISupports* aStream);

private:
  uint64_t mLocation;
  nsCOMPtr<nsIInputStream> mStream;
  uint64_t mLength;
};

class TruncateHelper : public FileHelper
{
public:
  TruncateHelper(FileHandle* aFileHandle,
                 FileRequest* aFileRequest,
                 uint64_t aOffset)
  : FileHelper(aFileHandle, aFileRequest),
    mOffset(aOffset)
  { }

  nsresult
  DoAsyncRun(nsISupports* aStream);

private:
  class AsyncTruncator : public AsyncHelper
  {
  public:
    AsyncTruncator(nsISupports* aStream, int64_t aOffset)
    : AsyncHelper(aStream),
      mOffset(aOffset)
    { }
  protected:
    nsresult
    DoStreamWork(nsISupports* aStream) MOZ_OVERRIDE;

    uint64_t mOffset;
  };

  uint64_t mOffset;
};

class FlushHelper : public FileHelper
{
public:
  FlushHelper(FileHandle* aFileHandle,
              FileRequest* aFileRequest)
  : FileHelper(aFileHandle, aFileRequest)
  { }

  nsresult
  DoAsyncRun(nsISupports* aStream);

private:
  class AsyncFlusher : public AsyncHelper
  {
  public:
    AsyncFlusher(nsISupports* aStream)
    : AsyncHelper(aStream)
    { }
  protected:
    nsresult
    DoStreamWork(nsISupports* aStream) MOZ_OVERRIDE;
  };
};

class OpenStreamHelper : public FileHelper
{
public:
  OpenStreamHelper(FileHandle* aFileHandle,
                   bool aWholeFile,
                   uint64_t aStart,
                   uint64_t aLength)
  : FileHelper(aFileHandle, nullptr),
    mWholeFile(aWholeFile), mStart(aStart), mLength(aLength)
  { }

  nsresult
  DoAsyncRun(nsISupports* aStream);

  nsCOMPtr<nsIInputStream>&
  Result()
  {
    return mStream;
  }

private:
  bool mWholeFile;
  uint64_t mStart;
  uint64_t mLength;

  nsCOMPtr<nsIInputStream> mStream;
};

already_AddRefed<nsIDOMEvent>
CreateGenericEvent(EventTarget* aEventOwner,
                   const nsAString& aType, bool aBubbles, bool aCancelable)
{
  nsCOMPtr<nsIDOMEvent> event;
  NS_NewDOMEvent(getter_AddRefs(event), aEventOwner, nullptr, nullptr);
  nsresult rv = event->InitEvent(aType, aBubbles, aCancelable);
  NS_ENSURE_SUCCESS(rv, nullptr);

  event->SetTrusted(true);

  return event.forget();
}

} // anonymous namespace

// static
already_AddRefed<FileHandle>
FileHandle::Create(MutableFile* aMutableFile,
                   FileMode aMode,
                   RequestMode aRequestMode)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<FileHandle> fileHandle = new FileHandle();

  fileHandle->BindToOwner(aMutableFile);

  fileHandle->mMutableFile = aMutableFile;
  fileHandle->mMode = aMode;
  fileHandle->mRequestMode = aRequestMode;

  nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
  NS_ENSURE_TRUE(appShell, nullptr);

  nsresult rv = appShell->RunBeforeNextEvent(fileHandle);
  NS_ENSURE_SUCCESS(rv, nullptr);

  fileHandle->mCreating = true;

  FileService* service = FileService::GetOrCreate();
  NS_ENSURE_TRUE(service, nullptr);

  rv = service->Enqueue(fileHandle, nullptr);
  NS_ENSURE_SUCCESS(rv, nullptr);

  return fileHandle.forget();
}

FileHandle::FileHandle()
: mReadyState(INITIAL),
  mMode(FileMode::Readonly),
  mRequestMode(NORMAL),
  mLocation(0),
  mPendingRequests(0),
  mAborted(false),
  mCreating(false)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  SetIsDOMBinding();
}

FileHandle::~FileHandle()
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(FileHandle, DOMEventTargetHelper,
                                   mMutableFile)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(FileHandle)
  NS_INTERFACE_MAP_ENTRY(nsIRunnable)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(FileHandle, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(FileHandle, DOMEventTargetHelper)

nsresult
FileHandle::PreHandleEvent(EventChainPreVisitor& aVisitor)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  aVisitor.mCanHandle = true;
  aVisitor.mParentTarget = mMutableFile;
  return NS_OK;
}

void
FileHandle::OnNewRequest()
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  if (!mPendingRequests) {
    MOZ_ASSERT(mReadyState == INITIAL, "Reusing a file handle!");
    mReadyState = LOADING;
  }
  ++mPendingRequests;
}

void
FileHandle::OnRequestFinished()
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  MOZ_ASSERT(mPendingRequests, "Mismatched calls!");
  --mPendingRequests;
  if (!mPendingRequests) {
    MOZ_ASSERT(mAborted || mReadyState == LOADING, "Bad state!");
    mReadyState = FileHandle::FINISHING;
    Finish();
  }
}

nsresult
FileHandle::CreateParallelStream(nsISupports** aStream)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  if (mMutableFile->IsInvalid()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsISupports> stream =
    mMutableFile->CreateStream(mMutableFile->mFile,
                               mMode == FileMode::Readonly);
  NS_ENSURE_TRUE(stream, NS_ERROR_FAILURE);

  mParallelStreams.AppendElement(stream);

  stream.forget(aStream);
  return NS_OK;
}

nsresult
FileHandle::GetOrCreateStream(nsISupports** aStream)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  if (mMutableFile->IsInvalid()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!mStream) {
    nsCOMPtr<nsISupports> stream =
      mMutableFile->CreateStream(mMutableFile->mFile,
                                 mMode == FileMode::Readonly);
    NS_ENSURE_TRUE(stream, NS_ERROR_FAILURE);

    stream.swap(mStream);
  }

  nsCOMPtr<nsISupports> stream(mStream);
  stream.forget(aStream);

  return NS_OK;
}

already_AddRefed<FileRequest>
FileHandle::GenerateFileRequest()
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  return FileRequest::Create(GetOwner(), this, /* aWrapAsDOMRequest */ false);
}

bool
FileHandle::IsOpen() const
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  // If we haven't started anything then we're open.
  if (mReadyState == INITIAL) {
    MOZ_ASSERT(FileHelper::GetCurrentFileHandle() != this,
               "This should be some other file handle (or null)!");
    return true;
  }

  // If we've already started then we need to check to see if we still have the
  // mCreating flag set. If we do (i.e. we haven't returned to the event loop
  // from the time we were created) then we are open. Otherwise check the
  // currently running file handles to see if it's the same. We only allow other
  // requests to be made if this file handle is currently running.
  if (mReadyState == LOADING) {
    if (mCreating) {
      return true;
    }

    if (FileHelper::GetCurrentFileHandle() == this) {
      return true;
    }
  }

  return false;
}

// virtual
JSObject*
FileHandle::WrapObject(JSContext* aCx)
{
  return FileHandleBinding::Wrap(aCx, this);
}

already_AddRefed<FileRequest>
FileHandle::GetMetadata(const DOMFileMetadataParameters& aParameters,
                        ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  // Common state checking
  if (!CheckState(aRv)) {
    return nullptr;
  }

  // Do nothing if the window is closed
  if (!GetOwner()) {
    return nullptr;
  }

  nsRefPtr<MetadataParameters> params =
    new MetadataParameters(aParameters.mSize, aParameters.mLastModified);
  if (!params->IsConfigured()) {
    aRv.ThrowTypeError(MSG_METADATA_NOT_CONFIGURED);
    return nullptr;
  }

  nsRefPtr<FileRequest> fileRequest = GenerateFileRequest();

  nsRefPtr<MetadataHelper> helper =
    new MetadataHelper(this, fileRequest, params);

  if (NS_WARN_IF(NS_FAILED(helper->Enqueue()))) {
    aRv.Throw(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
    return nullptr;
  }

  return fileRequest.forget();
}

already_AddRefed<FileRequest>
FileHandle::ReadAsArrayBuffer(uint64_t aSize, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  // State and argument checking for read
  if (!CheckStateAndArgumentsForRead(aSize, aRv)) {
    return nullptr;
  }

  // Do nothing if the window is closed
  if (!GetOwner()) {
    return nullptr;
  }

  nsRefPtr<FileRequest> fileRequest = GenerateFileRequest();

  nsRefPtr<ReadHelper> helper =
    new ReadHelper(this, fileRequest, mLocation, aSize);

  if (NS_WARN_IF(NS_FAILED(helper->Init())) ||
      NS_WARN_IF(NS_FAILED(helper->Enqueue()))) {
    aRv.Throw(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
    return nullptr;
  }

  mLocation += aSize;

  return fileRequest.forget();
}

already_AddRefed<FileRequest>
FileHandle::ReadAsText(uint64_t aSize, const nsAString& aEncoding,
                       ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  // State and argument checking for read
  if (!CheckStateAndArgumentsForRead(aSize, aRv)) {
    return nullptr;
  }

  // Do nothing if the window is closed
  if (!GetOwner()) {
    return nullptr;
  }

  nsRefPtr<FileRequest> fileRequest = GenerateFileRequest();

  nsRefPtr<ReadTextHelper> helper =
    new ReadTextHelper(this, fileRequest, mLocation, aSize, aEncoding);

  if (NS_WARN_IF(NS_FAILED(helper->Init())) ||
      NS_WARN_IF(NS_FAILED(helper->Enqueue()))) {
    aRv.Throw(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
    return nullptr;
  }

  mLocation += aSize;

  return fileRequest.forget();
}

already_AddRefed<FileRequest>
FileHandle::Truncate(const Optional<uint64_t>& aSize, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  // State checking for write
  if (!CheckStateForWrite(aRv)) {
    return nullptr;
  }

  // Getting location and additional state checking for truncate
  uint64_t location;
  if (aSize.WasPassed()) {
    // Just in case someone calls us from C++
    MOZ_ASSERT(aSize.Value() != UINT64_MAX, "Passed wrong size!");
    location = aSize.Value();
  } else {
    if (mLocation == UINT64_MAX) {
      aRv.Throw(NS_ERROR_DOM_FILEHANDLE_NOT_ALLOWED_ERR);
      return nullptr;
    }
    location = mLocation;
  }

  // Do nothing if the window is closed
  if (!GetOwner()) {
    return nullptr;
  }

  nsRefPtr<FileRequest> fileRequest = GenerateFileRequest();

  nsRefPtr<TruncateHelper> helper =
    new TruncateHelper(this, fileRequest, location);

  if (NS_WARN_IF(NS_FAILED(helper->Enqueue()))) {
    aRv.Throw(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
    return nullptr;
  }

  if (aSize.WasPassed()) {
    mLocation = aSize.Value();
  }

  return fileRequest.forget();
}

already_AddRefed<FileRequest>
FileHandle::Flush(ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  // State checking for write
  if (!CheckStateForWrite(aRv)) {
    return nullptr;
  }

  // Do nothing if the window is closed
  if (!GetOwner()) {
    return nullptr;
  }

  nsRefPtr<FileRequest> fileRequest = GenerateFileRequest();

  nsRefPtr<FlushHelper> helper = new FlushHelper(this, fileRequest);

  if (NS_WARN_IF(NS_FAILED(helper->Enqueue()))) {
    aRv.Throw(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
    return nullptr;
  }

  return fileRequest.forget();
}

void
FileHandle::Abort(ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  // This method is special enough for not using generic state checking methods.

  // We can't use IsOpen here since we need it to be possible to call Abort()
  // even from outside of transaction callbacks.
  if (mReadyState != FileHandle::INITIAL &&
      mReadyState != FileHandle::LOADING) {
    aRv.Throw(NS_ERROR_DOM_FILEHANDLE_NOT_ALLOWED_ERR);
    return;
  }

  bool needToFinish = mReadyState == INITIAL;

  mAborted = true;
  mReadyState = DONE;

  // Fire the abort event if there are no outstanding requests. Otherwise the
  // abort event will be fired when all outstanding requests finish.
  if (needToFinish) {
    aRv = Finish();
  }
}

NS_IMETHODIMP
FileHandle::Run()
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  // We're back at the event loop, no longer newborn.
  mCreating = false;

  // Maybe set the readyState to DONE if there were no requests generated.
  if (mReadyState == INITIAL) {
    mReadyState = DONE;

    if (NS_FAILED(Finish())) {
      NS_WARNING("Failed to finish!");
    }
  }

  return NS_OK;
}

nsresult
FileHandle::OpenInputStream(bool aWholeFile, uint64_t aStart, uint64_t aLength,
                            nsIInputStream** aResult)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  MOZ_ASSERT(mRequestMode == PARALLEL,
             "Don't call me in other than parallel mode!");

  // Common state checking
  ErrorResult error;
  if (!CheckState(error)) {
    return error.ErrorCode();
  }

  // Do nothing if the window is closed
  if (!GetOwner()) {
    return NS_OK;
  }

  nsRefPtr<OpenStreamHelper> helper =
    new OpenStreamHelper(this, aWholeFile, aStart, aLength);

  nsresult rv = helper->Enqueue();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);

  nsCOMPtr<nsIInputStream>& result = helper->Result();
  NS_ENSURE_TRUE(result, NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);

  result.forget(aResult);
  return NS_OK;
}

bool
FileHandle::CheckState(ErrorResult& aRv)
{
  if (!IsOpen()) {
    aRv.Throw(NS_ERROR_DOM_FILEHANDLE_INACTIVE_ERR);
    return false;
  }

  return true;
}

bool
FileHandle::CheckStateAndArgumentsForRead(uint64_t aSize, ErrorResult& aRv)
{
  // Common state checking
  if (!CheckState(aRv)) {
    return false;
  }

  // Additional state checking for read
  if (mLocation == UINT64_MAX) {
    aRv.Throw(NS_ERROR_DOM_FILEHANDLE_NOT_ALLOWED_ERR);
    return false;
  }

  // Argument checking for read
  if (!aSize) {
    aRv.ThrowTypeError(MSG_INVALID_READ_SIZE);
    return false;
  }

  return true;
}

bool
FileHandle::CheckStateForWrite(ErrorResult& aRv)
{
  // Common state checking
  if (!CheckState(aRv)) {
    return false;
  }

  // Additional state checking for write
  if (mMode != FileMode::Readwrite) {
    aRv.Throw(NS_ERROR_DOM_FILEHANDLE_READ_ONLY_ERR);
    return false;
  }

  return true;
}

already_AddRefed<FileRequest>
FileHandle::WriteInternal(nsIInputStream* aInputStream, uint64_t aInputLength,
                          bool aAppend, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  DebugOnly<ErrorResult> error;
  MOZ_ASSERT(CheckStateForWrite(error));
  MOZ_ASSERT_IF(!aAppend, mLocation != UINT64_MAX);
  MOZ_ASSERT(aInputStream);
  MOZ_ASSERT(aInputLength);
  MOZ_ASSERT(GetOwner());

  nsRefPtr<FileRequest> fileRequest = GenerateFileRequest();

  uint64_t location = aAppend ? UINT64_MAX : mLocation;

  nsRefPtr<WriteHelper> helper =
    new WriteHelper(this, fileRequest, location, aInputStream, aInputLength);

  if (NS_WARN_IF(NS_FAILED(helper->Enqueue()))) {
    aRv.Throw(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);
    return nullptr;
  }

  if (aAppend) {
    mLocation = UINT64_MAX;
  }
  else {
    mLocation += aInputLength;
  }

  return fileRequest.forget();
}

nsresult
FileHandle::Finish()
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<FinishHelper> helper(new FinishHelper(this));

  FileService* service = FileService::Get();
  MOZ_ASSERT(service, "This should never be null");

  nsIEventTarget* target = service->StreamTransportTarget();

  nsresult rv = target->Dispatch(helper, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// static
already_AddRefed<nsIInputStream>
FileHandle::GetInputStream(const ArrayBuffer& aValue, uint64_t* aInputLength,
                           ErrorResult& aRv)
{
  aValue.ComputeLengthAndData();
  const char* data = reinterpret_cast<const char*>(aValue.Data());
  uint32_t length = aValue.Length();

  nsCOMPtr<nsIInputStream> stream;
  aRv = NS_NewByteInputStream(getter_AddRefs(stream), data, length,
                              NS_ASSIGNMENT_COPY);
  if (aRv.Failed()) {
    return nullptr;
  }

  *aInputLength = length;
  return stream.forget();
}

// static
already_AddRefed<nsIInputStream>
FileHandle::GetInputStream(nsIDOMBlob* aValue, uint64_t* aInputLength,
                           ErrorResult& aRv)
{
  uint64_t length;
  aRv = aValue->GetSize(&length);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsCOMPtr<nsIInputStream> stream;
  aRv = aValue->GetInternalStream(getter_AddRefs(stream));
  if (aRv.Failed()) {
    return nullptr;
  }

  *aInputLength = length;
  return stream.forget();
}

// static
already_AddRefed<nsIInputStream>
FileHandle::GetInputStream(const nsAString& aValue, uint64_t* aInputLength,
                           ErrorResult& aRv)
{
  NS_ConvertUTF16toUTF8 cstr(aValue);

  nsCOMPtr<nsIInputStream> stream;
  aRv = NS_NewCStringInputStream(getter_AddRefs(stream), cstr);
  if (aRv.Failed()) {
    return nullptr;
  }

  *aInputLength = cstr.Length();
  return stream.forget();
}

FinishHelper::FinishHelper(FileHandle* aFileHandle)
: mFileHandle(aFileHandle),
  mAborted(aFileHandle->mAborted)
{
  mParallelStreams.SwapElements(aFileHandle->mParallelStreams);
  mStream.swap(aFileHandle->mStream);
}

NS_IMPL_ISUPPORTS(FinishHelper, nsIRunnable)

NS_IMETHODIMP
FinishHelper::Run()
{
  if (NS_IsMainThread()) {
    mFileHandle->mReadyState = FileHandle::DONE;

    FileService* service = FileService::Get();
    if (service) {
      service->NotifyFileHandleCompleted(mFileHandle);
    }

    nsCOMPtr<nsIDOMEvent> event;
    if (mAborted) {
      event = CreateGenericEvent(mFileHandle, NS_LITERAL_STRING("abort"),
                                 true, false);
    }
    else {
      event = CreateGenericEvent(mFileHandle, NS_LITERAL_STRING("complete"),
                                 false, false);
    }
    NS_ENSURE_TRUE(event, NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);

    bool dummy;
    if (NS_FAILED(mFileHandle->DispatchEvent(event, &dummy))) {
      NS_WARNING("Dispatch failed!");
    }

    mFileHandle = nullptr;

    return NS_OK;
  }

  if (mFileHandle->mMutableFile->IsInvalid()) {
    mAborted = true;
  }

  for (uint32_t index = 0; index < mParallelStreams.Length(); index++) {
    nsCOMPtr<nsIInputStream> stream =
      do_QueryInterface(mParallelStreams[index]);

    if (NS_FAILED(stream->Close())) {
      NS_WARNING("Failed to close stream!");
    }

    mParallelStreams[index] = nullptr;
  }

  if (mStream) {
    nsCOMPtr<nsIInputStream> stream = do_QueryInterface(mStream);

    if (NS_FAILED(stream->Close())) {
      NS_WARNING("Failed to close stream!");
    }

    mStream = nullptr;
  }

  return NS_DispatchToMainThread(this);
}

nsresult
ReadHelper::Init()
{
  mStream = MemoryOutputStream::Create(mSize);
  NS_ENSURE_TRUE(mStream, NS_ERROR_FAILURE);

  return NS_OK;
}

nsresult
ReadHelper::DoAsyncRun(nsISupports* aStream)
{
  MOZ_ASSERT(aStream, "Passed a null stream!");

  uint32_t flags = FileStreamWrapper::NOTIFY_PROGRESS;

  nsCOMPtr<nsIInputStream> istream =
    new FileInputStreamWrapper(aStream, this, mLocation, mSize, flags);

  FileService* service = FileService::Get();
  MOZ_ASSERT(service, "This should never be null");

  nsIEventTarget* target = service->StreamTransportTarget();

  nsCOMPtr<nsIAsyncStreamCopier> copier;
  nsresult rv =
    NS_NewAsyncStreamCopier(getter_AddRefs(copier), istream, mStream, target,
                            false, true, STREAM_COPY_BLOCK_SIZE);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = copier->AsyncCopy(this, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  mRequest = do_QueryInterface(copier);

  return NS_OK;
}

nsresult
ReadHelper::GetSuccessResult(JSContext* aCx,
                             JS::MutableHandle<JS::Value> aVal)
{
  JS::Rooted<JSObject*> arrayBuffer(aCx);
  nsresult rv =
    nsContentUtils::CreateArrayBuffer(aCx, mStream->Data(), arrayBuffer.address());
  NS_ENSURE_SUCCESS(rv, rv);

  aVal.setObject(*arrayBuffer);
  return NS_OK;
}

nsresult
ReadTextHelper::GetSuccessResult(JSContext* aCx,
                                 JS::MutableHandle<JS::Value> aVal)
{
  nsAutoCString encoding;
  const nsCString& data = mStream->Data();
  // The BOM sniffing is baked into the "decode" part of the Encoding
  // Standard, which the File API references.
  if (!nsContentUtils::CheckForBOM(
        reinterpret_cast<const unsigned char *>(data.get()),
        data.Length(),
        encoding)) {
    // BOM sniffing failed. Try the API argument.
    if (!EncodingUtils::FindEncodingForLabel(mEncoding, encoding)) {
      // API argument failed. Since we are dealing with a file system file,
      // we don't have a meaningful type attribute for the blob available,
      // so proceeding to the next step, which is defaulting to UTF-8.
      encoding.AssignLiteral("UTF-8");
    }
  }

  nsString tmpString;
  nsresult rv = nsContentUtils::ConvertStringFromEncoding(encoding, data,
                                                          tmpString);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!xpc::StringToJsval(aCx, tmpString, aVal)) {
    NS_WARNING("Failed to convert string!");
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
WriteHelper::DoAsyncRun(nsISupports* aStream)
{
  MOZ_ASSERT(aStream, "Passed a null stream!");

  uint32_t flags = FileStreamWrapper::NOTIFY_PROGRESS;

  nsCOMPtr<nsIOutputStream> ostream =
    new FileOutputStreamWrapper(aStream, this, mLocation, mLength, flags);

  FileService* service = FileService::Get();
  MOZ_ASSERT(service, "This should never be null");

  nsIEventTarget* target = service->StreamTransportTarget();

  nsCOMPtr<nsIAsyncStreamCopier> copier;
  nsresult rv =
    NS_NewAsyncStreamCopier(getter_AddRefs(copier), mStream, ostream, target,
                            true, false, STREAM_COPY_BLOCK_SIZE);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = copier->AsyncCopy(this, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  mRequest = do_QueryInterface(copier);

  return NS_OK;
}

nsresult
TruncateHelper::DoAsyncRun(nsISupports* aStream)
{
  MOZ_ASSERT(aStream, "Passed a null stream!");

  nsRefPtr<AsyncTruncator> truncator = new AsyncTruncator(aStream, mOffset);

  nsresult rv = truncator->AsyncWork(this, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
TruncateHelper::AsyncTruncator::DoStreamWork(nsISupports* aStream)
{
  nsCOMPtr<nsISeekableStream> sstream = do_QueryInterface(aStream);

  nsresult rv = sstream->Seek(nsISeekableStream::NS_SEEK_SET, mOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = sstream->SetEOF();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
FlushHelper::DoAsyncRun(nsISupports* aStream)
{
  MOZ_ASSERT(aStream, "Passed a null stream!");

  nsRefPtr<AsyncFlusher> flusher = new AsyncFlusher(aStream);

  nsresult rv = flusher->AsyncWork(this, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
FlushHelper::AsyncFlusher::DoStreamWork(nsISupports* aStream)
{
  nsCOMPtr<nsIOutputStream> ostream = do_QueryInterface(aStream);

  nsresult rv = ostream->Flush();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
OpenStreamHelper::DoAsyncRun(nsISupports* aStream)
{
  MOZ_ASSERT(aStream, "Passed a null stream!");

  uint32_t flags = FileStreamWrapper::NOTIFY_CLOSE |
                   FileStreamWrapper::NOTIFY_DESTROY;

  mStream = mWholeFile ?
    new FileInputStreamWrapper(aStream, this, 0, mLength, flags) :
    new FileInputStreamWrapper(aStream, this, mStart, mLength, flags);

  return NS_OK;
}

} // namespace dom
} // namespace mozilla

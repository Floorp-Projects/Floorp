/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LockedFile.h"

#include "nsIAppShell.h"
#include "nsIDOMFile.h"
#include "nsIFileStorage.h"
#include "nsISeekableStream.h"

#include "jsfriendapi.h"
#include "nsCharsetAlias.h"
#include "nsEventDispatcher.h"
#include "nsNetUtil.h"
#include "nsDOMClassInfoID.h"
#include "nsDOMEvent.h"
#include "nsJSUtils.h"
#include "nsStringStream.h"
#include "nsWidgetsCID.h"
#include "xpcpublic.h"

#include "AsyncHelper.h"
#include "FileHandle.h"
#include "FileHelper.h"
#include "FileRequest.h"
#include "FileService.h"
#include "FileStreamWrappers.h"
#include "MemoryStreams.h"
#include "MetadataHelper.h"
#include "nsDOMError.h"
#include "nsContentUtils.h"

#define STREAM_COPY_BLOCK_SIZE 32768

USING_FILE_NAMESPACE

namespace {

NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

class ReadHelper : public FileHelper
{
public:
  ReadHelper(LockedFile* aLockedFile,
             FileRequest* aFileRequest,
             PRUint64 aLocation,
             PRUint64 aSize)
  : FileHelper(aLockedFile, aFileRequest),
    mLocation(aLocation), mSize(aSize)
  {
    NS_ASSERTION(mSize, "Passed zero size!");
  }

  nsresult
  Init();

  nsresult
  DoAsyncRun(nsISupports* aStream);

  nsresult
  GetSuccessResult(JSContext* aCx, jsval* aVal);

protected:
  PRUint64 mLocation;
  PRUint64 mSize;

  nsRefPtr<MemoryOutputStream> mStream;
};

class ReadTextHelper : public ReadHelper
{
public:
  ReadTextHelper(LockedFile* aLockedFile,
                 FileRequest* aFileRequest,
                 PRUint64 aLocation,
                 PRUint64 aSize,
                 const nsAString& aEncoding)
  : ReadHelper(aLockedFile, aFileRequest, aLocation, aSize),
    mEncoding(aEncoding)
  { }

  nsresult
  GetSuccessResult(JSContext* aCx, jsval* aVal);

private:
  nsString mEncoding;
};

class WriteHelper : public FileHelper
{
public:
  WriteHelper(LockedFile* aLockedFile,
              FileRequest* aFileRequest,
              PRUint64 aLocation,
              nsIInputStream* aStream,
              PRUint64 aLength)
  : FileHelper(aLockedFile, aFileRequest),
    mLocation(aLocation), mStream(aStream), mLength(aLength)
  {
    NS_ASSERTION(mLength, "Passed zero length!");
  }

  nsresult
  DoAsyncRun(nsISupports* aStream);

private:
  PRUint64 mLocation;
  nsCOMPtr<nsIInputStream> mStream;
  PRUint64 mLength;
};

class TruncateHelper : public FileHelper
{
public:
  TruncateHelper(LockedFile* aLockedFile,
                 FileRequest* aFileRequest,
                 PRUint64 aOffset)
  : FileHelper(aLockedFile, aFileRequest),
    mOffset(aOffset)
  { }

  nsresult
  DoAsyncRun(nsISupports* aStream);

private:
  class AsyncTruncator : public AsyncHelper
  {
  public:
    AsyncTruncator(nsISupports* aStream, PRInt64 aOffset)
    : AsyncHelper(aStream),
      mOffset(aOffset)
    { }
  protected:
    nsresult
    DoStreamWork(nsISupports* aStream);

    PRUint64 mOffset;
  };

  PRUint64 mOffset;
};

class FlushHelper : public FileHelper
{
public:
  FlushHelper(LockedFile* aLockedFile,
               FileRequest* aFileRequest)
  : FileHelper(aLockedFile, aFileRequest)
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
    DoStreamWork(nsISupports* aStream);
  };
};

class OpenStreamHelper : public FileHelper
{
public:
  OpenStreamHelper(LockedFile* aLockedFile,
                   bool aWholeFile,
                   PRUint64 aStart,
                   PRUint64 aLength)
  : FileHelper(aLockedFile, nullptr),
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
  PRUint64 mStart;
  PRUint64 mLength;

  nsCOMPtr<nsIInputStream> mStream;
};

already_AddRefed<nsDOMEvent>
CreateGenericEvent(const nsAString& aType, bool aBubbles, bool aCancelable)
{
  nsRefPtr<nsDOMEvent> event(new nsDOMEvent(nullptr, nullptr));
  nsresult rv = event->InitEvent(aType, aBubbles, aCancelable);
  NS_ENSURE_SUCCESS(rv, nullptr);

  rv = event->SetTrusted(true);
  NS_ENSURE_SUCCESS(rv, nullptr);

  return event.forget();
}

inline nsresult
GetInputStreamForJSVal(const jsval& aValue, JSContext* aCx,
                       nsIInputStream** aInputStream, PRUint64* aInputLength)
{
  nsresult rv;

  if (!JSVAL_IS_PRIMITIVE(aValue)) {
    JSObject* obj = JSVAL_TO_OBJECT(aValue);
    if (JS_IsArrayBufferObject(obj, aCx)) {
      char* data = reinterpret_cast<char*>(JS_GetArrayBufferData(obj, aCx));
      PRUint32 length = JS_GetArrayBufferByteLength(obj, aCx);

      rv = NS_NewByteInputStream(aInputStream, data, length,
                                 NS_ASSIGNMENT_COPY);
      NS_ENSURE_SUCCESS(rv, rv);

      *aInputLength = length;

      return NS_OK;
    }

    nsCOMPtr<nsIDOMBlob> blob = do_QueryInterface(
      nsContentUtils::XPConnect()->GetNativeOfWrapper(aCx, obj));
    if (blob) {
      rv = blob->GetSize(aInputLength);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = blob->GetInternalStream(aInputStream);
      NS_ENSURE_SUCCESS(rv, rv);

      return NS_OK;
    }
  }

  JSString* jsstr;
  if (JSVAL_IS_STRING(aValue)) {
    jsstr = JSVAL_TO_STRING(aValue);
  }
  else {
    jsstr = JS_ValueToString(aCx, aValue);
    NS_ENSURE_TRUE(jsstr, NS_ERROR_XPC_BAD_CONVERT_JS);
  }

  nsDependentJSString str;
  if (!str.init(aCx, jsstr)) {
    return NS_ERROR_FAILURE;
  }

  nsCString cstr;
  CopyUTF16toUTF8(str, cstr);

  nsCOMPtr<nsIInputStream> stream;
  rv = NS_NewCStringInputStream(getter_AddRefs(stream), cstr);
  NS_ENSURE_SUCCESS(rv, rv);

  stream.forget(aInputStream);
  *aInputLength = cstr.Length();

  return NS_OK;
}

} // anonymous namespace

// static
already_AddRefed<LockedFile>
LockedFile::Create(FileHandle* aFileHandle,
                   Mode aMode,
                   RequestMode aRequestMode)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<LockedFile> lockedFile = new LockedFile();

  lockedFile->BindToOwner(aFileHandle);

  lockedFile->mFileHandle = aFileHandle;
  lockedFile->mMode = aMode;
  lockedFile->mRequestMode = aRequestMode;

  nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
  NS_ENSURE_TRUE(appShell, nullptr);

  nsresult rv = appShell->RunBeforeNextEvent(lockedFile);
  NS_ENSURE_SUCCESS(rv, nullptr);

  lockedFile->mCreating = true;

  FileService* service = FileService::GetOrCreate();
  NS_ENSURE_TRUE(service, nullptr);

  rv = service->Enqueue(lockedFile, nullptr);
  NS_ENSURE_SUCCESS(rv, nullptr);

  return lockedFile.forget();
}

LockedFile::LockedFile()
: mReadyState(INITIAL),
  mMode(READ_ONLY),
  mRequestMode(NORMAL),
  mLocation(0),
  mPendingRequests(0),
  mAborted(false),
  mCreating(false)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

LockedFile::~LockedFile()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

NS_IMPL_CYCLE_COLLECTION_CLASS(LockedFile)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(LockedFile,
                                                  nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mFileHandle,
                                                       nsIDOMEventTarget)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(complete)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(abort)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(error)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(LockedFile,
                                                nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mFileHandle)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(complete)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(abort)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(error)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(LockedFile)
  NS_INTERFACE_MAP_ENTRY(nsIDOMLockedFile)
  NS_INTERFACE_MAP_ENTRY(nsIRunnable)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(LockedFile)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(LockedFile, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(LockedFile, nsDOMEventTargetHelper)

DOMCI_DATA(LockedFile, LockedFile)

NS_IMPL_EVENT_HANDLER(LockedFile, complete)
NS_IMPL_EVENT_HANDLER(LockedFile, abort)
NS_IMPL_EVENT_HANDLER(LockedFile, error)

nsresult
LockedFile::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  aVisitor.mCanHandle = true;
  aVisitor.mParentTarget = mFileHandle;
  return NS_OK;
}

void
LockedFile::OnNewRequest()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  if (!mPendingRequests) {
    NS_ASSERTION(mReadyState == INITIAL,
                 "Reusing a locked file!");
    mReadyState = LOADING;
  }
  ++mPendingRequests;
}

void
LockedFile::OnRequestFinished()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(mPendingRequests, "Mismatched calls!");
  --mPendingRequests;
  if (!mPendingRequests) {
    NS_ASSERTION(mAborted || mReadyState == LOADING,
                 "Bad state!");
    mReadyState = LockedFile::FINISHING;
    Finish();
  }
}

nsresult
LockedFile::CreateParallelStream(nsISupports** aStream)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsIFileStorage* fileStorage = mFileHandle->mFileStorage;
  if (fileStorage->IsStorageInvalidated()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsISupports> stream =
    mFileHandle->CreateStream(mFileHandle->mFile, mMode == READ_ONLY);
  NS_ENSURE_TRUE(stream, NS_ERROR_FAILURE);

  mParallelStreams.AppendElement(stream);

  stream.forget(aStream);
  return NS_OK;
}

nsresult
LockedFile::GetOrCreateStream(nsISupports** aStream)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsIFileStorage* fileStorage = mFileHandle->mFileStorage;
  if (fileStorage->IsStorageInvalidated()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!mStream) {
    nsCOMPtr<nsISupports> stream =
      mFileHandle->CreateStream(mFileHandle->mFile, mMode == READ_ONLY);
    NS_ENSURE_TRUE(stream, NS_ERROR_FAILURE);

    stream.swap(mStream);
  }

  nsCOMPtr<nsISupports> stream(mStream);
  stream.forget(aStream);

  return NS_OK;
}

already_AddRefed<FileRequest>
LockedFile::GenerateFileRequest()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  return FileRequest::Create(GetOwner(), this, true);
}

bool
LockedFile::IsOpen() const
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // If we haven't started anything then we're open.
  if (mReadyState == INITIAL) {
    NS_ASSERTION(FileHelper::GetCurrentLockedFile() != this,
                 "This should be some other locked file (or null)!");
    return true;
  }

  // If we've already started then we need to check to see if we still have the
  // mCreating flag set. If we do (i.e. we haven't returned to the event loop
  // from the time we were created) then we are open. Otherwise check the
  // currently running locked files to see if it's the same. We only allow other
  // requests to be made if this locked file is currently running.
  if (mReadyState == LOADING) {
    if (mCreating) {
      return true;
    }

    if (FileHelper::GetCurrentLockedFile() == this) {
      return true;
    }
  }

  return false;
}

NS_IMETHODIMP
LockedFile::GetFileHandle(nsIDOMFileHandle** aFileHandle)
{
  nsCOMPtr<nsIDOMFileHandle> result(mFileHandle);
  result.forget(aFileHandle);
  return NS_OK;
}

NS_IMETHODIMP
LockedFile::GetMode(nsAString& aMode)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  switch (mMode) {
   case READ_ONLY:
     aMode.AssignLiteral("readonly");
     break;
   case READ_WRITE:
     aMode.AssignLiteral("readwrite");
     break;
   default:
     NS_NOTREACHED("Unknown mode!");
  }

  return NS_OK;
}

NS_IMETHODIMP
LockedFile::GetActive(bool* aActive)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  *aActive = IsOpen();
  return NS_OK;
}

NS_IMETHODIMP
LockedFile::GetLocation(JSContext* aCx,
                        jsval* aLocation)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (mLocation == LL_MAXUINT) {
    *aLocation = JSVAL_NULL;
  }
  else {
    *aLocation = JS_NumberValue(double(mLocation));
  }
  return NS_OK;
}

NS_IMETHODIMP
LockedFile::SetLocation(JSContext* aCx,
                        const jsval& aLocation)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // Null means the end-of-file.
  if (JSVAL_IS_NULL(aLocation)) {
    mLocation = LL_MAXUINT;
    return NS_OK;
  }

  uint64_t location;
  if (!JS::ToUint64(aCx, aLocation, &location)) {
    return NS_ERROR_TYPE_ERR;
  }

  mLocation = location;
  return NS_OK;
}

NS_IMETHODIMP
LockedFile::GetMetadata(const jsval& aParameters,
                        JSContext* aCx,
                        nsIDOMFileRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!IsOpen()) {
    return NS_ERROR_DOM_FILEHANDLE_LOCKEDFILE_INACTIVE_ERR;
  }

  // Do nothing if the window is closed
  if (!GetOwner()) {
    return NS_OK;
  }

  nsRefPtr<MetadataParameters> params = new MetadataParameters();

  // Get optional arguments.
  if (!JSVAL_IS_VOID(aParameters) && !JSVAL_IS_NULL(aParameters)) {
    nsresult rv = params->Init(aCx, &aParameters);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);

    if (!params->IsConfigured()) {
      return NS_ERROR_TYPE_ERR;
    }
  }
  else {
    params->Init(true, true);
  }

  nsRefPtr<FileRequest> fileRequest = GenerateFileRequest();
  NS_ENSURE_TRUE(fileRequest, NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);

  nsRefPtr<MetadataHelper> helper =
    new MetadataHelper(this, fileRequest, params);

  nsresult rv = helper->Enqueue();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);

  fileRequest.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
LockedFile::ReadAsArrayBuffer(PRUint64 aSize,
                              JSContext* aCx,
                              nsIDOMFileRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!IsOpen()) {
    return NS_ERROR_DOM_FILEHANDLE_LOCKEDFILE_INACTIVE_ERR;
  }

  if (mLocation == LL_MAXUINT) {
    return NS_ERROR_DOM_FILEHANDLE_NOT_ALLOWED_ERR;
  }

  if (!aSize) {
    return NS_ERROR_TYPE_ERR;
  }

  // Do nothing if the window is closed
  if (!GetOwner()) {
    return NS_OK;
  }

  nsRefPtr<FileRequest> fileRequest = GenerateFileRequest();
  NS_ENSURE_TRUE(fileRequest, NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);

  nsRefPtr<ReadHelper> helper =
    new ReadHelper(this, fileRequest, mLocation, aSize);

  nsresult rv = helper->Init();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);

  rv = helper->Enqueue();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);

  mLocation += aSize;

  fileRequest.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
LockedFile::ReadAsText(PRUint64 aSize,
                       const nsAString& aEncoding,
                       nsIDOMFileRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!IsOpen()) {
    return NS_ERROR_DOM_FILEHANDLE_LOCKEDFILE_INACTIVE_ERR;
  }

  if (mLocation == LL_MAXUINT) {
    return NS_ERROR_DOM_FILEHANDLE_NOT_ALLOWED_ERR;
  }

  if (!aSize) {
    return NS_ERROR_TYPE_ERR;
  }

  // Do nothing if the window is closed
  if (!GetOwner()) {
    return NS_OK;
  }

  nsRefPtr<FileRequest> fileRequest = GenerateFileRequest();
  NS_ENSURE_TRUE(fileRequest, NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);

  nsRefPtr<ReadTextHelper> helper =
    new ReadTextHelper(this, fileRequest, mLocation, aSize, aEncoding);

  nsresult rv = helper->Init();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);

  rv = helper->Enqueue();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);

  mLocation += aSize;

  fileRequest.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
LockedFile::Write(const jsval& aValue,
                  JSContext* aCx,
                  nsIDOMFileRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  return WriteOrAppend(aValue, aCx, _retval, false);
}

NS_IMETHODIMP
LockedFile::Append(const jsval& aValue,
                   JSContext* aCx,
                   nsIDOMFileRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  return WriteOrAppend(aValue, aCx, _retval, true);
}

NS_IMETHODIMP
LockedFile::Truncate(PRUint64 aSize,
                     PRUint8 aOptionalArgCount,
                     nsIDOMFileRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!IsOpen()) {
    return NS_ERROR_DOM_FILEHANDLE_LOCKEDFILE_INACTIVE_ERR;
  }

  if (mMode != READ_WRITE) {
    return NS_ERROR_DOM_FILEHANDLE_READ_ONLY_ERR;
  }

  PRUint64 location;
  if (aOptionalArgCount) {
    // Just in case someone calls us from C++
    NS_ASSERTION(aSize != LL_MAXUINT, "Passed wrong size!");
    location = aSize;
  }
  else {
    if (mLocation == LL_MAXUINT) {
      return NS_ERROR_DOM_FILEHANDLE_NOT_ALLOWED_ERR;
    }
    location = mLocation;
  }

  // Do nothing if the window is closed
  if (!GetOwner()) {
    return NS_OK;
  }

  nsRefPtr<FileRequest> fileRequest = GenerateFileRequest();
  NS_ENSURE_TRUE(fileRequest, NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);

  nsRefPtr<TruncateHelper> helper =
    new TruncateHelper(this, fileRequest, location);

  nsresult rv = helper->Enqueue();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);

  if (aOptionalArgCount) {
    mLocation = aSize;
  }

  fileRequest.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
LockedFile::Flush(nsIDOMFileRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!IsOpen()) {
    return NS_ERROR_DOM_FILEHANDLE_LOCKEDFILE_INACTIVE_ERR;
  }

  if (mMode != READ_WRITE) {
    return NS_ERROR_DOM_FILEHANDLE_READ_ONLY_ERR;
  }

  // Do nothing if the window is closed
  if (!GetOwner()) {
    return NS_OK;
  }

  nsRefPtr<FileRequest> fileRequest = GenerateFileRequest();
  NS_ENSURE_TRUE(fileRequest, NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);

  nsRefPtr<FlushHelper> helper = new FlushHelper(this, fileRequest);

  nsresult rv = helper->Enqueue();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);

  fileRequest.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
LockedFile::Abort()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // We can't use IsOpen here since we need it to be possible to call Abort()
  // even from outside of transaction callbacks.
  if (mReadyState != LockedFile::INITIAL &&
      mReadyState != LockedFile::LOADING) {
    return NS_ERROR_DOM_FILEHANDLE_NOT_ALLOWED_ERR;
  }

  bool needToFinish = mReadyState == INITIAL;

  mAborted = true;
  mReadyState = DONE;

  // Fire the abort event if there are no outstanding requests. Otherwise the
  // abort event will be fired when all outstanding requests finish.
  if (needToFinish) {
    return Finish();
  }

  return NS_OK;
}

NS_IMETHODIMP
LockedFile::Run()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

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
LockedFile::OpenInputStream(bool aWholeFile, PRUint64 aStart, PRUint64 aLength,
                            nsIInputStream** aResult)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(mRequestMode == PARALLEL,
               "Don't call me in other than parallel mode!");

  if (!IsOpen()) {
    return NS_ERROR_DOM_FILEHANDLE_LOCKEDFILE_INACTIVE_ERR;
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

nsresult
LockedFile::WriteOrAppend(const jsval& aValue,
                          JSContext* aCx,
                          nsIDOMFileRequest** _retval,
                          bool aAppend)
{
  if (!IsOpen()) {
    return NS_ERROR_DOM_FILEHANDLE_LOCKEDFILE_INACTIVE_ERR;
  }

  if (mMode != READ_WRITE) {
    return NS_ERROR_DOM_FILEHANDLE_READ_ONLY_ERR;
  }

  if (!aAppend && mLocation == LL_MAXUINT) {
    return NS_ERROR_DOM_FILEHANDLE_NOT_ALLOWED_ERR;
  }

  // Do nothing if the window is closed
  if (!GetOwner()) {
    return NS_OK;
  }

  nsCOMPtr<nsIInputStream> inputStream;
  PRUint64 inputLength;
  nsresult rv =
    GetInputStreamForJSVal(aValue, aCx, getter_AddRefs(inputStream),
                           &inputLength);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!inputLength) {
    return NS_OK;
  }

  nsRefPtr<FileRequest> fileRequest = GenerateFileRequest();
  NS_ENSURE_TRUE(fileRequest, NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);

  PRUint64 location = aAppend ? LL_MAXUINT : mLocation;

  nsRefPtr<WriteHelper> helper =
    new WriteHelper(this, fileRequest, location, inputStream, inputLength);

  rv = helper->Enqueue();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);

  if (aAppend) {
    mLocation = LL_MAXUINT;
  }
  else {
    mLocation += inputLength;
  }

  fileRequest.forget(_retval);
  return NS_OK;
}

nsresult
LockedFile::Finish()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<FinishHelper> helper(new FinishHelper(this));

  FileService* service = FileService::Get();
  NS_ASSERTION(service, "This should never be null");

  nsIEventTarget* target = service->StreamTransportTarget();

  nsresult rv = target->Dispatch(helper, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

FinishHelper::FinishHelper(LockedFile* aLockedFile)
: mLockedFile(aLockedFile),
  mAborted(aLockedFile->mAborted)
{
  mParallelStreams.SwapElements(aLockedFile->mParallelStreams);
  mStream.swap(aLockedFile->mStream);
}

NS_IMPL_THREADSAFE_ISUPPORTS1(FinishHelper, nsIRunnable)

NS_IMETHODIMP
FinishHelper::Run()
{
  if (NS_IsMainThread()) {
    mLockedFile->mReadyState = LockedFile::DONE;

    FileService* service = FileService::Get();
    if (service) {
      service->NotifyLockedFileCompleted(mLockedFile);
    }

    nsCOMPtr<nsIDOMEvent> event;
    if (mAborted) {
      event = CreateGenericEvent(NS_LITERAL_STRING("abort"), true, false);
    }
    else {
      event = CreateGenericEvent(NS_LITERAL_STRING("complete"), false, false);
    }
    NS_ENSURE_TRUE(event, NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR);

    bool dummy;
    if (NS_FAILED(mLockedFile->DispatchEvent(event, &dummy))) {
      NS_WARNING("Dispatch failed!");
    }

    mLockedFile = nullptr;

    return NS_OK;
  }

  nsIFileStorage* fileStorage = mLockedFile->mFileHandle->mFileStorage;
  if (fileStorage->IsStorageInvalidated()) {
    mAborted = true;
  }

  for (PRUint32 index = 0; index < mParallelStreams.Length(); index++) {
    nsCOMPtr<nsIOutputStream> ostream =
      do_QueryInterface(mParallelStreams[index]);

    if (NS_FAILED(ostream->Close())) {
      NS_WARNING("Failed to close stream!");
    }

    mParallelStreams[index] = nullptr;
  }

  if (mStream) {
    nsCOMPtr<nsIOutputStream> ostream = do_QueryInterface(mStream);

    if (NS_FAILED(ostream->Close())) {
      NS_WARNING("Failed to close stream!");
    }

    mStream = nullptr;
  }

  return NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL);
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
  NS_ASSERTION(aStream, "Passed a null stream!");

  PRUint32 flags = FileStreamWrapper::NOTIFY_PROGRESS;

  nsCOMPtr<nsIInputStream> istream =
    new FileInputStreamWrapper(aStream, this, mLocation, mSize, flags);

  FileService* service = FileService::Get();
  NS_ASSERTION(service, "This should never be null");

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
                             jsval* aVal)
{
  JSObject *arrayBuffer;
  nsresult rv =
    nsContentUtils::CreateArrayBuffer(aCx, mStream->Data(), &arrayBuffer);
  NS_ENSURE_SUCCESS(rv, rv);

  *aVal = OBJECT_TO_JSVAL(arrayBuffer);

  return NS_OK;
}

nsresult
ReadTextHelper::GetSuccessResult(JSContext* aCx,
                                 jsval* aVal)
{
  nsresult rv;

  nsCString charsetGuess;
  if (!mEncoding.IsEmpty()) {
    CopyUTF16toUTF8(mEncoding, charsetGuess);
  }
  else {
    const nsCString& data = mStream->Data();
    PRUint32 dataLen = data.Length();
    rv = nsContentUtils::GuessCharset(data.get(), dataLen, charsetGuess);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCString charset;
  rv = nsCharsetAlias::GetPreferred(charsetGuess, charset);
  NS_ENSURE_SUCCESS(rv, rv);

  nsString tmpString;
  rv = nsContentUtils::ConvertStringFromCharset(charset, mStream->Data(),
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
  NS_ASSERTION(aStream, "Passed a null stream!");

  PRUint32 flags = FileStreamWrapper::NOTIFY_PROGRESS;

  nsCOMPtr<nsIOutputStream> ostream =
    new FileOutputStreamWrapper(aStream, this, mLocation, mLength, flags);

  FileService* service = FileService::Get();
  NS_ASSERTION(service, "This should never be null");

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
  NS_ASSERTION(aStream, "Passed a null stream!");

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
  NS_ASSERTION(aStream, "Passed a null stream!");

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
  NS_ASSERTION(aStream, "Passed a null stream!");

  PRUint32 flags = FileStreamWrapper::NOTIFY_CLOSE |
                   FileStreamWrapper::NOTIFY_DESTROY;

  mStream = mWholeFile ?
    new FileInputStreamWrapper(aStream, this, 0, mLength, flags) :
    new FileInputStreamWrapper(aStream, this, mStart, mLength, flags);

  return NS_OK;
}

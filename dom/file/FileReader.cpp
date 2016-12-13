/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileReader.h"

#include "nsIEventTarget.h"
#include "nsIGlobalObject.h"
#include "nsITimer.h"
#include "nsITransport.h"
#include "nsIStreamTransportService.h"

#include "mozilla/Base64.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/dom/DOMError.h"
#include "mozilla/dom/EncodingUtils.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FileReaderBinding.h"
#include "mozilla/dom/ProgressEvent.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDOMJSUtils.h"
#include "nsError.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "xpcpublic.h"

#include "WorkerPrivate.h"
#include "WorkerScope.h"

namespace mozilla {
namespace dom {

using namespace workers;

#define ABORT_STR "abort"
#define LOAD_STR "load"
#define LOADSTART_STR "loadstart"
#define LOADEND_STR "loadend"
#define ERROR_STR "error"
#define PROGRESS_STR "progress"

const uint64_t kUnknownSize = uint64_t(-1);

static NS_DEFINE_CID(kStreamTransportServiceCID, NS_STREAMTRANSPORTSERVICE_CID);

NS_IMPL_CYCLE_COLLECTION_CLASS(FileReader)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(FileReader,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBlob)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mProgressNotifier)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mError)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(FileReader,
                                                DOMEventTargetHelper)
  tmp->Shutdown();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBlob)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mProgressNotifier)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mError)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(FileReader,
                                               DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mResultArrayBuffer)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(FileReader)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsIInputStreamCallback)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(FileReader, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(FileReader, DOMEventTargetHelper)

class MOZ_RAII FileReaderDecreaseBusyCounter
{
  RefPtr<FileReader> mFileReader;
public:
  explicit FileReaderDecreaseBusyCounter(FileReader* aFileReader)
    : mFileReader(aFileReader)
  {}

  ~FileReaderDecreaseBusyCounter()
  {
    mFileReader->DecreaseBusyCounter();
  }
};

void
FileReader::RootResultArrayBuffer()
{
  mozilla::HoldJSObjects(this);
}

//FileReader constructors/initializers

FileReader::FileReader(nsIGlobalObject* aGlobal,
                       WorkerPrivate* aWorkerPrivate)
  : DOMEventTargetHelper(aGlobal)
  , mFileData(nullptr)
  , mDataLen(0)
  , mDataFormat(FILE_AS_BINARY)
  , mResultArrayBuffer(nullptr)
  , mProgressEventWasDelayed(false)
  , mTimerIsActive(false)
  , mReadyState(EMPTY)
  , mTotal(0)
  , mTransferred(0)
  , mTarget(do_GetCurrentThread())
  , mBusyCount(0)
  , mWorkerPrivate(aWorkerPrivate)
{
  MOZ_ASSERT(aGlobal);
  MOZ_ASSERT(NS_IsMainThread() == !mWorkerPrivate);
  SetDOMStringToNull(mResult);
}

FileReader::~FileReader()
{
  Shutdown();
  DropJSObjects(this);
}

/* static */ already_AddRefed<FileReader>
FileReader::Constructor(const GlobalObject& aGlobal, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  WorkerPrivate* workerPrivate = nullptr;

  if (!NS_IsMainThread()) {
    JSContext* cx = aGlobal.Context();
    workerPrivate = GetWorkerPrivateFromContext(cx);
    MOZ_ASSERT(workerPrivate);
  }

  RefPtr<FileReader> fileReader = new FileReader(global, workerPrivate);

  return fileReader.forget();
}

// nsIInterfaceRequestor

NS_IMETHODIMP
FileReader::GetInterface(const nsIID & aIID, void **aResult)
{
  return QueryInterface(aIID, aResult);
}

void
FileReader::GetResult(JSContext* aCx,
                      JS::MutableHandle<JS::Value> aResult,
                      ErrorResult& aRv)
{
  JS::Rooted<JS::Value> result(aCx);

  if (mDataFormat == FILE_AS_ARRAYBUFFER) {
    if (mReadyState == DONE && mResultArrayBuffer) {
      result.setObject(*mResultArrayBuffer);
    } else {
      result.setNull();
    }

    if (!JS_WrapValue(aCx, &result)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    aResult.set(result);
    return;
  }

  nsString tmpResult = mResult;
  if (!xpc::StringToJsval(aCx, tmpResult, aResult)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
}

static nsresult
ReadFuncBinaryString(nsIInputStream* in,
                     void* closure,
                     const char* fromRawSegment,
                     uint32_t toOffset,
                     uint32_t count,
                     uint32_t *writeCount)
{
  char16_t* dest = static_cast<char16_t*>(closure) + toOffset;
  char16_t* end = dest + count;
  const unsigned char* source = (const unsigned char*)fromRawSegment;
  while (dest != end) {
    *dest = *source;
    ++dest;
    ++source;
  }
  *writeCount = count;

  return NS_OK;
}

nsresult
FileReader::DoOnLoadEnd(nsresult aStatus,
                        nsAString& aSuccessEvent,
                        nsAString& aTerminationEvent)
{
  // Make sure we drop all the objects that could hold files open now.
  nsCOMPtr<nsIAsyncInputStream> stream;
  mAsyncStream.swap(stream);

  RefPtr<Blob> blob;
  mBlob.swap(blob);

  // Clear out the data if necessary
  if (NS_FAILED(aStatus)) {
    FreeFileData();
    return NS_OK;
  }

  // In case we read a different number of bytes, we can assume that the
  // underlying storage has changed. We should not continue.
  if (mDataLen != mTotal) {
    DispatchError(NS_ERROR_FAILURE, aTerminationEvent);
    FreeFileData();
    return NS_ERROR_FAILURE;
  }

  aSuccessEvent = NS_LITERAL_STRING(LOAD_STR);
  aTerminationEvent = NS_LITERAL_STRING(LOADEND_STR);

  nsresult rv = NS_OK;
  switch (mDataFormat) {
    case FILE_AS_ARRAYBUFFER: {
      AutoJSAPI jsapi;
      if (!jsapi.Init(GetParentObject())) {
        FreeFileData();
        return NS_ERROR_FAILURE;
      }

      RootResultArrayBuffer();
      mResultArrayBuffer = JS_NewArrayBufferWithContents(jsapi.cx(), mDataLen, mFileData);
      if (!mResultArrayBuffer) {
        JS_ClearPendingException(jsapi.cx());
        rv = NS_ERROR_OUT_OF_MEMORY;
      } else {
        mFileData = nullptr; // Transfer ownership
      }
      break;
    }
    case FILE_AS_BINARY:
      break; //Already accumulated mResult
    case FILE_AS_TEXT:
      if (!mFileData) {
        if (mDataLen) {
          rv = NS_ERROR_OUT_OF_MEMORY;
          break;
        }
        rv = GetAsText(blob, mCharset, "", mDataLen, mResult);
        break;
      }
      rv = GetAsText(blob, mCharset, mFileData, mDataLen, mResult);
      break;
    case FILE_AS_DATAURL:
      rv = GetAsDataURL(blob, mFileData, mDataLen, mResult);
      break;
  }

  mResult.SetIsVoid(false);

  FreeFileData();

  return rv;
}

nsresult
FileReader::DoAsyncWait()
{
  nsresult rv = IncreaseBusyCounter();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mAsyncStream->AsyncWait(this,
                               /* aFlags*/ 0,
                               /* aRequestedCount */ 0,
                               mTarget);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    DecreaseBusyCounter();
    return rv;
  }

  return NS_OK;
}

nsresult
FileReader::DoReadData(uint64_t aCount)
{
  MOZ_ASSERT(mAsyncStream);

  if (mDataFormat == FILE_AS_BINARY) {
    //Continuously update our binary string as data comes in
    uint32_t oldLen = mResult.Length();
    MOZ_ASSERT(mResult.Length() == mDataLen, "unexpected mResult length");
    if (uint64_t(oldLen) + aCount > UINT32_MAX)
      return NS_ERROR_OUT_OF_MEMORY;
    char16_t *buf = nullptr;
    mResult.GetMutableData(&buf, oldLen + aCount, fallible);
    NS_ENSURE_TRUE(buf, NS_ERROR_OUT_OF_MEMORY);

    uint32_t bytesRead = 0;
    mAsyncStream->ReadSegments(ReadFuncBinaryString, buf + oldLen, aCount,
                               &bytesRead);
    MOZ_ASSERT(bytesRead == aCount, "failed to read data");
  }
  else {
    CheckedInt<uint64_t> size = mDataLen;
    size += aCount;

    //Update memory buffer to reflect the contents of the file
    if (!size.isValid() ||
        // PR_Realloc doesn't support over 4GB memory size even if 64-bit OS
        size.value() > UINT32_MAX ||
        size.value() > mTotal) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    if (mDataFormat != FILE_AS_ARRAYBUFFER) {
      mFileData = (char *) realloc(mFileData, mDataLen + aCount);
      NS_ENSURE_TRUE(mFileData, NS_ERROR_OUT_OF_MEMORY);
    }

    uint32_t bytesRead = 0;
    mAsyncStream->Read(mFileData + mDataLen, aCount, &bytesRead);
    MOZ_ASSERT(bytesRead == aCount, "failed to read data");
  }

  mDataLen += aCount;
  return NS_OK;
}

// Helper methods

void
FileReader::ReadFileContent(Blob& aBlob,
                            const nsAString &aCharset,
                            eDataFormat aDataFormat,
                            ErrorResult& aRv)
{
  if (mReadyState == LOADING) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  mError = nullptr;

  SetDOMStringToNull(mResult);
  mResultArrayBuffer = nullptr;

  mAsyncStream = nullptr;

  mTransferred = 0;
  mTotal = 0;
  mReadyState = EMPTY;
  FreeFileData();

  mBlob = &aBlob;
  mDataFormat = aDataFormat;
  CopyUTF16toUTF8(aCharset, mCharset);

  nsresult rv;
  nsCOMPtr<nsIStreamTransportService> sts =
    do_GetService(kStreamTransportServiceCID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  nsCOMPtr<nsIInputStream> stream;
  mBlob->GetInternalStream(getter_AddRefs(stream), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  nsCOMPtr<nsITransport> transport;
  aRv = sts->CreateInputTransport(stream,
                                  /* aStartOffset */ 0,
                                  /* aReadLimit */ -1,
                                  /* aCloseWhenDone */ true,
                                  getter_AddRefs(transport));
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  nsCOMPtr<nsIInputStream> wrapper;
  aRv = transport->OpenInputStream(/* aFlags */ 0,
                                   /* aSegmentSize */ 0,
                                   /* aSegmentCount */ 0,
                                   getter_AddRefs(wrapper));
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  MOZ_ASSERT(!mAsyncStream);
  mAsyncStream = do_QueryInterface(wrapper);
  MOZ_ASSERT(mAsyncStream);

  mTotal = mBlob->GetSize(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  aRv = DoAsyncWait();
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  //FileReader should be in loading state here
  mReadyState = LOADING;
  DispatchProgressEvent(NS_LITERAL_STRING(LOADSTART_STR));

  if (mDataFormat == FILE_AS_ARRAYBUFFER) {
    mFileData = js_pod_malloc<char>(mTotal);
    if (!mFileData) {
      NS_WARNING("Preallocation failed for ReadFileData");
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    }
  }
}

nsresult
FileReader::GetAsText(Blob *aBlob,
                      const nsACString &aCharset,
                      const char *aFileData,
                      uint32_t aDataLen,
                      nsAString& aResult)
{
  // The BOM sniffing is baked into the "decode" part of the Encoding
  // Standard, which the File API references.
  nsAutoCString encoding;
  if (!nsContentUtils::CheckForBOM(
        reinterpret_cast<const unsigned char *>(aFileData),
        aDataLen,
        encoding)) {
    // BOM sniffing failed. Try the API argument.
    if (!EncodingUtils::FindEncodingForLabel(aCharset,
                                             encoding)) {
      // API argument failed. Try the type property of the blob.
      nsAutoString type16;
      aBlob->GetType(type16);
      NS_ConvertUTF16toUTF8 type(type16);
      nsAutoCString specifiedCharset;
      bool haveCharset;
      int32_t charsetStart, charsetEnd;
      NS_ExtractCharsetFromContentType(type,
                                       specifiedCharset,
                                       &haveCharset,
                                       &charsetStart,
                                       &charsetEnd);
      if (!EncodingUtils::FindEncodingForLabel(specifiedCharset, encoding)) {
        // Type property failed. Use UTF-8.
        encoding.AssignLiteral("UTF-8");
      }
    }
  }

  nsDependentCSubstring data(aFileData, aDataLen);
  return nsContentUtils::ConvertStringFromEncoding(encoding, data, aResult);
}

nsresult
FileReader::GetAsDataURL(Blob *aBlob,
                         const char *aFileData,
                         uint32_t aDataLen,
                         nsAString& aResult)
{
  aResult.AssignLiteral("data:");

  nsAutoString contentType;
  aBlob->GetType(contentType);
  if (!contentType.IsEmpty()) {
    aResult.Append(contentType);
  } else {
    aResult.AppendLiteral("application/octet-stream");
  }
  aResult.AppendLiteral(";base64,");

  nsCString encodedData;
  nsresult rv = Base64Encode(Substring(aFileData, aDataLen), encodedData);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!AppendASCIItoUTF16(encodedData, aResult, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

/* virtual */ JSObject*
FileReader::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return FileReaderBinding::Wrap(aCx, this, aGivenProto);
}

void
FileReader::StartProgressEventTimer()
{
  if (!mProgressNotifier) {
    mProgressNotifier = do_CreateInstance(NS_TIMER_CONTRACTID);
  }

  if (mProgressNotifier) {
    mProgressEventWasDelayed = false;
    mTimerIsActive = true;
    mProgressNotifier->Cancel();
    mProgressNotifier->SetTarget(mTarget);
    mProgressNotifier->InitWithCallback(this, NS_PROGRESS_EVENT_INTERVAL,
                                        nsITimer::TYPE_ONE_SHOT);
  }
}

void
FileReader::ClearProgressEventTimer()
{
  mProgressEventWasDelayed = false;
  mTimerIsActive = false;
  if (mProgressNotifier) {
    mProgressNotifier->Cancel();
  }
}

void
FileReader::DispatchError(nsresult rv, nsAString& finalEvent)
{
  // Set the status attribute, and dispatch the error event
  switch (rv) {
  case NS_ERROR_FILE_NOT_FOUND:
    mError = new DOMError(GetOwner(), NS_LITERAL_STRING("NotFoundError"));
    break;
  case NS_ERROR_FILE_ACCESS_DENIED:
    mError = new DOMError(GetOwner(), NS_LITERAL_STRING("SecurityError"));
    break;
  default:
    mError = new DOMError(GetOwner(), NS_LITERAL_STRING("NotReadableError"));
    break;
  }

  // Dispatch error event to signify load failure
  DispatchProgressEvent(NS_LITERAL_STRING(ERROR_STR));
  DispatchProgressEvent(finalEvent);
}

nsresult
FileReader::DispatchProgressEvent(const nsAString& aType)
{
  ProgressEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mLoaded = mTransferred;

  if (mTotal != kUnknownSize) {
    init.mLengthComputable = true;
    init.mTotal = mTotal;
  } else {
    init.mLengthComputable = false;
    init.mTotal = 0;
  }
  RefPtr<ProgressEvent> event =
    ProgressEvent::Constructor(this, aType, init);
  event->SetTrusted(true);

  return DispatchDOMEvent(nullptr, event, nullptr, nullptr);
}

// nsITimerCallback
NS_IMETHODIMP
FileReader::Notify(nsITimer* aTimer)
{
  nsresult rv;
  mTimerIsActive = false;

  if (mProgressEventWasDelayed) {
    rv = DispatchProgressEvent(NS_LITERAL_STRING("progress"));
    NS_ENSURE_SUCCESS(rv, rv);

    StartProgressEventTimer();
  }

  return NS_OK;
}

// InputStreamCallback
NS_IMETHODIMP
FileReader::OnInputStreamReady(nsIAsyncInputStream* aStream)
{
  if (mReadyState != LOADING || aStream != mAsyncStream) {
    return NS_OK;
  }

  // We use this class to decrease the busy counter at the end of this method.
  // In theory we can do it immediatelly but, for debugging reasons, we want to
  // be 100% sure we have a workerHolder when OnLoadEnd() is called.
  FileReaderDecreaseBusyCounter RAII(this);

  uint64_t aCount;
  nsresult rv = aStream->Available(&aCount);

  if (NS_SUCCEEDED(rv) && aCount) {
    rv = DoReadData(aCount);
  }

  if (NS_SUCCEEDED(rv)) {
    rv = DoAsyncWait();
  }

  if (NS_FAILED(rv) || !aCount) {
    if (rv == NS_BASE_STREAM_CLOSED) {
      rv = NS_OK;
    }
    return OnLoadEnd(rv);
  }

  mTransferred += aCount;

  //Notify the timer is the appropriate timeframe has passed
  if (mTimerIsActive) {
    mProgressEventWasDelayed = true;
  } else {
    rv = DispatchProgressEvent(NS_LITERAL_STRING(PROGRESS_STR));
    NS_ENSURE_SUCCESS(rv, rv);

    StartProgressEventTimer();
  }

  return NS_OK;
}

nsresult
FileReader::OnLoadEnd(nsresult aStatus)
{
  // Cancel the progress event timer
  ClearProgressEventTimer();

  // FileReader must be in DONE stage after an operation
  mReadyState = DONE;

  nsAutoString successEvent, termEvent;
  nsresult rv = DoOnLoadEnd(aStatus, successEvent, termEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  // Set the status field as appropriate
  if (NS_FAILED(aStatus)) {
    DispatchError(aStatus, termEvent);
    return NS_OK;
  }

  // Dispatch event to signify end of a successful operation
  DispatchProgressEvent(successEvent);
  DispatchProgressEvent(termEvent);

  return NS_OK;
}

void
FileReader::Abort()
{
  if (mReadyState == EMPTY || mReadyState == DONE) {
    return;
  }

  MOZ_ASSERT(mReadyState == LOADING);

  ClearProgressEventTimer();

  mReadyState = DONE;

  // XXX The spec doesn't say this
  mError = new DOMError(GetOwner(), NS_LITERAL_STRING("AbortError"));

  // Revert status and result attributes
  SetDOMStringToNull(mResult);
  mResultArrayBuffer = nullptr;

  mAsyncStream = nullptr;
  mBlob = nullptr;

  //Clean up memory buffer
  FreeFileData();

  // Dispatch the events
  DispatchProgressEvent(NS_LITERAL_STRING(ABORT_STR));
  DispatchProgressEvent(NS_LITERAL_STRING(LOADEND_STR));
}

nsresult
FileReader::IncreaseBusyCounter()
{
  if (mWorkerPrivate && mBusyCount++ == 0 &&
      !HoldWorker(mWorkerPrivate, Closing)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

void
FileReader::DecreaseBusyCounter()
{
  MOZ_ASSERT_IF(mWorkerPrivate, mBusyCount);
  if (mWorkerPrivate && --mBusyCount == 0) {
    ReleaseWorker();
  }
}

bool
FileReader::Notify(Status aStatus)
{
  MOZ_ASSERT(mWorkerPrivate);
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (aStatus > Running) {
    Shutdown();
  }

  return true;
}

void
FileReader::Shutdown()
{
  FreeFileData();
  mResultArrayBuffer = nullptr;

  if (mAsyncStream) {
    mAsyncStream->Close();
    mAsyncStream = nullptr;
  }

  if (mWorkerPrivate && mBusyCount != 0) {
    ReleaseWorker();
    mWorkerPrivate = nullptr;
    mBusyCount = 0;
  }
}

} // dom namespace
} // mozilla namespace

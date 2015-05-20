/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMFileReader.h"

#include "nsContentCID.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfoID.h"
#include "nsError.h"
#include "nsIFile.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"

#include "nsXPCOM.h"
#include "nsIDOMEventListener.h"
#include "nsJSEnvironment.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Base64.h"
#include "mozilla/dom/EncodingUtils.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FileReaderBinding.h"
#include "xpcpublic.h"
#include "nsDOMJSUtils.h"

#include "jsfriendapi.h"

#include "nsITransport.h"
#include "nsIStreamTransportService.h"

using namespace mozilla;
using namespace mozilla::dom;

#define LOAD_STR "load"
#define LOADSTART_STR "loadstart"
#define LOADEND_STR "loadend"

static NS_DEFINE_CID(kStreamTransportServiceCID, NS_STREAMTRANSPORTSERVICE_CID);

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMFileReader)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDOMFileReader,
                                                  FileIOObject)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBlob)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDOMFileReader,
                                                FileIOObject)
  tmp->mResultArrayBuffer = nullptr;
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBlob)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END


NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(nsDOMFileReader,
                                               DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mResultArrayBuffer)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMFileReader)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIDOMFileReader)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END_INHERITING(FileIOObject)

NS_IMPL_ADDREF_INHERITED(nsDOMFileReader, FileIOObject)
NS_IMPL_RELEASE_INHERITED(nsDOMFileReader, FileIOObject)

NS_IMPL_EVENT_HANDLER(nsDOMFileReader, load)
NS_IMPL_EVENT_HANDLER(nsDOMFileReader, loadend)
NS_IMPL_EVENT_HANDLER(nsDOMFileReader, loadstart)
NS_IMPL_FORWARD_EVENT_HANDLER(nsDOMFileReader, abort, FileIOObject)
NS_IMPL_FORWARD_EVENT_HANDLER(nsDOMFileReader, progress, FileIOObject)
NS_IMPL_FORWARD_EVENT_HANDLER(nsDOMFileReader, error, FileIOObject)

void
nsDOMFileReader::RootResultArrayBuffer()
{
  mozilla::HoldJSObjects(this);
}

//nsDOMFileReader constructors/initializers

nsDOMFileReader::nsDOMFileReader()
  : mFileData(nullptr),
    mDataLen(0), mDataFormat(FILE_AS_BINARY),
    mResultArrayBuffer(nullptr)
{
  SetDOMStringToNull(mResult);
}

nsDOMFileReader::~nsDOMFileReader()
{
  FreeFileData();
  mResultArrayBuffer = nullptr;
  mozilla::DropJSObjects(this);
}


/**
 * This Init method is called from the factory constructor.
 */
nsresult
nsDOMFileReader::Init()
{
  // Instead of grabbing some random global from the context stack,
  // let's use the default one (junk scope) for now.
  // We should move away from this Init...
  BindToOwner(xpc::NativeGlobal(xpc::PrivilegedJunkScope()));
  return NS_OK;
}

/* static */ already_AddRefed<nsDOMFileReader>
nsDOMFileReader::Constructor(const GlobalObject& aGlobal, ErrorResult& aRv)
{
  nsRefPtr<nsDOMFileReader> fileReader = new nsDOMFileReader();

  nsCOMPtr<nsPIDOMWindow> owner = do_QueryInterface(aGlobal.GetAsSupports());
  if (!owner) {
    NS_WARNING("Unexpected owner");
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  fileReader->BindToOwner(owner);
  return fileReader.forget();
}

// nsIInterfaceRequestor

NS_IMETHODIMP
nsDOMFileReader::GetInterface(const nsIID & aIID, void **aResult)
{
  return QueryInterface(aIID, aResult);
}

// nsIDOMFileReader

NS_IMETHODIMP
nsDOMFileReader::GetReadyState(uint16_t *aReadyState)
{
  *aReadyState = ReadyState();
  return NS_OK;
}

void
nsDOMFileReader::GetResult(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                           ErrorResult& aRv)
{
  aRv = GetResult(aCx, aResult);
}

NS_IMETHODIMP
nsDOMFileReader::GetResult(JSContext* aCx, JS::MutableHandle<JS::Value> aResult)
{
  JS::Rooted<JS::Value> result(aCx);
  if (mDataFormat == FILE_AS_ARRAYBUFFER) {
    if (mReadyState == nsIDOMFileReader::DONE && mResultArrayBuffer) {
      result.setObject(*mResultArrayBuffer);
    } else {
      result.setNull();
    }
    if (!JS_WrapValue(aCx, &result)) {
      return NS_ERROR_FAILURE;
    }
    aResult.set(result);
    return NS_OK;
  }

  nsString tmpResult = mResult;
  if (!xpc::StringToJsval(aCx, tmpResult, aResult)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMFileReader::GetError(nsISupports** aError)
{
  NS_IF_ADDREF(*aError = GetError());
  return NS_OK;
}

NS_IMETHODIMP
nsDOMFileReader::ReadAsArrayBuffer(nsIDOMBlob* aBlob, JSContext* aCx)
{
  NS_ENSURE_TRUE(aBlob, NS_ERROR_NULL_POINTER);
  ErrorResult rv;
  nsRefPtr<Blob> blob = static_cast<Blob*>(aBlob);
  ReadAsArrayBuffer(aCx, *blob, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
nsDOMFileReader::ReadAsBinaryString(nsIDOMBlob* aBlob)
{
  NS_ENSURE_TRUE(aBlob, NS_ERROR_NULL_POINTER);
  ErrorResult rv;
  nsRefPtr<Blob> blob = static_cast<Blob*>(aBlob);
  ReadAsBinaryString(*blob, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
nsDOMFileReader::ReadAsText(nsIDOMBlob* aBlob,
                            const nsAString &aCharset)
{
  NS_ENSURE_TRUE(aBlob, NS_ERROR_NULL_POINTER);
  ErrorResult rv;
  nsRefPtr<Blob> blob = static_cast<Blob*>(aBlob);
  ReadAsText(*blob, aCharset, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
nsDOMFileReader::ReadAsDataURL(nsIDOMBlob* aBlob)
{
  NS_ENSURE_TRUE(aBlob, NS_ERROR_NULL_POINTER);
  ErrorResult rv;
  nsRefPtr<Blob> blob = static_cast<Blob*>(aBlob);
  ReadAsDataURL(*blob, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
nsDOMFileReader::Abort()
{
  ErrorResult rv;
  FileIOObject::Abort(rv);
  return rv.StealNSResult();
}

/* virtual */ void
nsDOMFileReader::DoAbort(nsAString& aEvent)
{
  // Revert status and result attributes
  SetDOMStringToNull(mResult);
  mResultArrayBuffer = nullptr;

  mAsyncStream = nullptr;
  mBlob = nullptr;

  //Clean up memory buffer
  FreeFileData();

  // Tell the base class which event to dispatch
  aEvent = NS_LITERAL_STRING(LOADEND_STR);
}

static
NS_METHOD
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
nsDOMFileReader::DoOnLoadEnd(nsresult aStatus,
                             nsAString& aSuccessEvent,
                             nsAString& aTerminationEvent)
{

  // Make sure we drop all the objects that could hold files open now.
  nsCOMPtr<nsIAsyncInputStream> stream;
  mAsyncStream.swap(stream);

  nsRefPtr<Blob> blob;
  mBlob.swap(blob);

  aSuccessEvent = NS_LITERAL_STRING(LOAD_STR);
  aTerminationEvent = NS_LITERAL_STRING(LOADEND_STR);

  // Clear out the data if necessary
  if (NS_FAILED(aStatus)) {
    FreeFileData();
    return NS_OK;
  }

  nsresult rv = NS_OK;
  switch (mDataFormat) {
    case FILE_AS_ARRAYBUFFER: {
      AutoJSAPI jsapi;
      if (NS_WARN_IF(!jsapi.Init(mozilla::DOMEventTargetHelper::GetParentObject()))) {
        return NS_ERROR_FAILURE;
      }

      RootResultArrayBuffer();
      mResultArrayBuffer = JS_NewArrayBufferWithContents(jsapi.cx(), mTotal, mFileData);
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
nsDOMFileReader::DoReadData(nsIAsyncInputStream* aStream, uint64_t aCount)
{
  MOZ_ASSERT(aStream);

  if (mDataFormat == FILE_AS_BINARY) {
    //Continuously update our binary string as data comes in
    uint32_t oldLen = mResult.Length();
    NS_ASSERTION(mResult.Length() == mDataLen,
                 "unexpected mResult length");
    if (uint64_t(oldLen) + aCount > UINT32_MAX)
      return NS_ERROR_OUT_OF_MEMORY;
    char16_t *buf = nullptr;
    mResult.GetMutableData(&buf, oldLen + aCount, fallible);
    NS_ENSURE_TRUE(buf, NS_ERROR_OUT_OF_MEMORY);

    uint32_t bytesRead = 0;
    aStream->ReadSegments(ReadFuncBinaryString, buf + oldLen, aCount,
                          &bytesRead);
    NS_ASSERTION(bytesRead == aCount, "failed to read data");
  }
  else {
    //Update memory buffer to reflect the contents of the file
    if (mDataLen + aCount > UINT32_MAX) {
      // PR_Realloc doesn't support over 4GB memory size even if 64-bit OS
      return NS_ERROR_OUT_OF_MEMORY;
    }
    if (mDataFormat != FILE_AS_ARRAYBUFFER) {
      mFileData = (char *) realloc(mFileData, mDataLen + aCount);
      NS_ENSURE_TRUE(mFileData, NS_ERROR_OUT_OF_MEMORY);
    }

    uint32_t bytesRead = 0;
    aStream->Read(mFileData + mDataLen, aCount, &bytesRead);
    NS_ASSERTION(bytesRead == aCount, "failed to read data");
  }

  mDataLen += aCount;
  return NS_OK;
}

// Helper methods

void
nsDOMFileReader::ReadFileContent(Blob& aBlob,
                                 const nsAString &aCharset,
                                 eDataFormat aDataFormat,
                                 ErrorResult& aRv)
{
  //Implicit abort to clear any other activity going on
  Abort();
  mError = nullptr;
  SetDOMStringToNull(mResult);
  mTransferred = 0;
  mTotal = 0;
  mReadyState = nsIDOMFileReader::EMPTY;
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
  rv = sts->CreateInputTransport(stream,
                                 /* aStartOffset */ 0,
                                 /* aReadLimit */ -1,
                                 /* aCloseWhenDone */ true,
                                 getter_AddRefs(transport));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  nsCOMPtr<nsIInputStream> wrapper;
  rv = transport->OpenInputStream(/* aFlags */ 0,
                                  /* aSegmentSize */ 0,
                                  /* aSegmentCount */ 0,
                                  getter_AddRefs(wrapper));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  MOZ_ASSERT(!mAsyncStream);
  mAsyncStream = do_QueryInterface(wrapper);
  MOZ_ASSERT(mAsyncStream);

  mTotal = mBlob->GetSize(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  rv = DoAsyncWait(mAsyncStream);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  //FileReader should be in loading state here
  mReadyState = nsIDOMFileReader::LOADING;
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
nsDOMFileReader::GetAsText(Blob *aBlob,
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
nsDOMFileReader::GetAsDataURL(Blob *aBlob,
                              const char *aFileData,
                              uint32_t aDataLen,
                              nsAString& aResult)
{
  aResult.AssignLiteral("data:");

  nsString contentType;
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
nsDOMFileReader::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return FileReaderBinding::Wrap(aCx, this, aGivenProto);
}

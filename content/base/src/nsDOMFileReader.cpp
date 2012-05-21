/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMFileReader.h"

#include "nsContentCID.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfoID.h"
#include "nsDOMFile.h"
#include "nsDOMError.h"
#include "nsCharsetAlias.h"
#include "nsICharsetDetector.h"
#include "nsICharsetConverterManager.h"
#include "nsIConverterInputStream.h"
#include "nsIFile.h"
#include "nsIFileStreams.h"
#include "nsIInputStream.h"
#include "nsIMIMEService.h"
#include "nsIPlatformCharset.h"
#include "nsIUnicodeDecoder.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"

#include "plbase64.h"
#include "prmem.h"

#include "nsLayoutCID.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIURI.h"
#include "nsStreamUtils.h"
#include "nsXPCOM.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMEventListener.h"
#include "nsIJSContextStack.h"
#include "nsJSEnvironment.h"
#include "nsIScriptGlobalObject.h"
#include "nsCExternalHandlerService.h"
#include "nsIStreamConverterService.h"
#include "nsCycleCollectionParticipant.h"
#include "nsLayoutStatics.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsBlobProtocolHandler.h"
#include "mozilla/Preferences.h"
#include "xpcpublic.h"
#include "nsIScriptSecurityManager.h"
#include "nsDOMJSUtils.h"
#include "nsDOMEventTargetHelper.h"

#include "jsfriendapi.h"

using namespace mozilla;

#define LOAD_STR "load"
#define LOADSTART_STR "loadstart"
#define LOADEND_STR "loadend"

using mozilla::dom::FileIOObject;

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMFileReader)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDOMFileReader,
                                                  FileIOObject)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mFile)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mPrincipal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mChannel)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(load)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(loadstart)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(loadend)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDOMFileReader,
                                                FileIOObject)
  tmp->mResultArrayBuffer = nsnull;
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mFile)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mPrincipal)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(load)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(loadstart)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(loadend)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END


NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(nsDOMFileReader,
                                               nsDOMEventTargetHelper)
  if(tmp->mResultArrayBuffer) {
    NS_IMPL_CYCLE_COLLECTION_TRACE_JS_CALLBACK(tmp->mResultArrayBuffer,
                                               "mResultArrayBuffer")
  }
NS_IMPL_CYCLE_COLLECTION_TRACE_END

DOMCI_DATA(FileReader, nsDOMFileReader)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMFileReader)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFileReader)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
  NS_INTERFACE_MAP_ENTRY(nsICharsetDetectionObserver)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(FileReader)
NS_INTERFACE_MAP_END_INHERITING(FileIOObject)

NS_IMPL_ADDREF_INHERITED(nsDOMFileReader, FileIOObject)
NS_IMPL_RELEASE_INHERITED(nsDOMFileReader, FileIOObject)

void
nsDOMFileReader::RootResultArrayBuffer()
{
  nsContentUtils::PreserveWrapper(
    static_cast<nsIDOMEventTarget*>(
      static_cast<nsDOMEventTargetHelper*>(this)), this);
}

//nsICharsetDetectionObserver

NS_IMETHODIMP
nsDOMFileReader::Notify(const char *aCharset, nsDetectionConfident aConf)
{
  mCharset = aCharset;
  return NS_OK;
}

//nsDOMFileReader constructors/initializers

nsDOMFileReader::nsDOMFileReader()
  : mFileData(nsnull),
    mDataLen(0), mDataFormat(FILE_AS_BINARY),
    mResultArrayBuffer(nsnull)     
{
  nsLayoutStatics::AddRef();
  SetDOMStringToNull(mResult);
}

nsDOMFileReader::~nsDOMFileReader()
{
  FreeFileData();

  nsLayoutStatics::Release();
}

nsresult
nsDOMFileReader::Init()
{
  nsDOMEventTargetHelper::Init();

  nsIScriptSecurityManager *secMan = nsContentUtils::GetSecurityManager();
  nsCOMPtr<nsIPrincipal> subjectPrincipal;
  if (secMan) {
    nsresult rv = secMan->GetSubjectPrincipal(getter_AddRefs(subjectPrincipal));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  NS_ENSURE_STATE(subjectPrincipal);
  mPrincipal.swap(subjectPrincipal);

  return NS_OK;
}

NS_IMPL_EVENT_HANDLER(nsDOMFileReader, load)
NS_IMPL_EVENT_HANDLER(nsDOMFileReader, loadstart)
NS_IMPL_EVENT_HANDLER(nsDOMFileReader, loadend)
NS_IMPL_FORWARD_EVENT_HANDLER(nsDOMFileReader, abort, FileIOObject)
NS_IMPL_FORWARD_EVENT_HANDLER(nsDOMFileReader, progress, FileIOObject)
NS_IMPL_FORWARD_EVENT_HANDLER(nsDOMFileReader, error, FileIOObject)

NS_IMETHODIMP
nsDOMFileReader::Initialize(nsISupports* aOwner, JSContext* cx, JSObject* obj,
                            PRUint32 argc, jsval *argv)
{
  nsCOMPtr<nsPIDOMWindow> owner = do_QueryInterface(aOwner);
  if (!owner) {
    NS_WARNING("Unexpected nsIJSNativeInitializer owner");
    return NS_OK;
  }

  BindToOwner(owner);

  // This object is bound to a |window|,
  // so reset the principal.
  nsCOMPtr<nsIScriptObjectPrincipal> scriptPrincipal = do_QueryInterface(aOwner);
  NS_ENSURE_STATE(scriptPrincipal);
  mPrincipal = scriptPrincipal->GetPrincipal();

  return NS_OK; 
}

// nsIInterfaceRequestor

NS_IMETHODIMP
nsDOMFileReader::GetInterface(const nsIID & aIID, void **aResult)
{
  return QueryInterface(aIID, aResult);
}

// nsIDOMFileReader

NS_IMETHODIMP
nsDOMFileReader::GetReadyState(PRUint16 *aReadyState)
{
  return FileIOObject::GetReadyState(aReadyState);
}

NS_IMETHODIMP
nsDOMFileReader::GetResult(JSContext* aCx, jsval* aResult)
{
  if (mDataFormat == FILE_AS_ARRAYBUFFER) {
    if (mReadyState == nsIDOMFileReader::DONE && mResultArrayBuffer) {
      JSObject* tmp = mResultArrayBuffer;
      *aResult = OBJECT_TO_JSVAL(tmp);
    } else {
      *aResult = JSVAL_NULL;
    }
    if (!JS_WrapValue(aCx, aResult)) {
      return NS_ERROR_FAILURE;
    }
    return NS_OK;
  }
 
  nsString tmpResult = mResult;
  if (!xpc::StringToJsval(aCx, tmpResult, aResult)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMFileReader::GetError(nsIDOMDOMError** aError)
{
  return FileIOObject::GetError(aError);
}

NS_IMETHODIMP
nsDOMFileReader::ReadAsArrayBuffer(nsIDOMBlob* aFile, JSContext* aCx)
{
  return ReadFileContent(aCx, aFile, EmptyString(), FILE_AS_ARRAYBUFFER);
}

NS_IMETHODIMP
nsDOMFileReader::ReadAsBinaryString(nsIDOMBlob* aFile)
{
  return ReadFileContent(nsnull, aFile, EmptyString(), FILE_AS_BINARY);
}

NS_IMETHODIMP
nsDOMFileReader::ReadAsText(nsIDOMBlob* aFile,
                            const nsAString &aCharset)
{
  return ReadFileContent(nsnull, aFile, aCharset, FILE_AS_TEXT);
}

NS_IMETHODIMP
nsDOMFileReader::ReadAsDataURL(nsIDOMBlob* aFile)
{
  return ReadFileContent(nsnull, aFile, EmptyString(), FILE_AS_DATAURL);
}

NS_IMETHODIMP
nsDOMFileReader::Abort()
{
  return FileIOObject::Abort();
}

nsresult
nsDOMFileReader::DoAbort(nsAString& aEvent)
{
  // Revert status and result attributes
  SetDOMStringToNull(mResult);
  mResultArrayBuffer = nsnull;
    
  // Non-null channel indicates a read is currently active
  if (mChannel) {
    // Cancel request requires an error status
    mChannel->Cancel(NS_ERROR_FAILURE);
    mChannel = nsnull;
  }
  mFile = nsnull;

  //Clean up memory buffer
  FreeFileData();

  // Tell the base class which event to dispatch
  aEvent = NS_LITERAL_STRING(LOADEND_STR);
  return NS_OK;
}

static
NS_METHOD
ReadFuncBinaryString(nsIInputStream* in,
                     void* closure,
                     const char* fromRawSegment,
                     PRUint32 toOffset,
                     PRUint32 count,
                     PRUint32 *writeCount)
{
  PRUnichar* dest = static_cast<PRUnichar*>(closure) + toOffset;
  PRUnichar* end = dest + count;
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
nsDOMFileReader::DoOnDataAvailable(nsIRequest *aRequest,
                                   nsISupports *aContext,
                                   nsIInputStream *aInputStream,
                                   PRUint32 aOffset,
                                   PRUint32 aCount)
{
  if (mDataFormat == FILE_AS_BINARY) {
    //Continuously update our binary string as data comes in
    NS_ASSERTION(mResult.Length() == aOffset,
                 "unexpected mResult length");
    PRUint32 oldLen = mResult.Length();
    PRUnichar *buf = nsnull;
    mResult.GetMutableData(&buf, oldLen + aCount, fallible_t());
    NS_ENSURE_TRUE(buf, NS_ERROR_OUT_OF_MEMORY);

    PRUint32 bytesRead = 0;
    aInputStream->ReadSegments(ReadFuncBinaryString, buf + oldLen, aCount,
                               &bytesRead);
    NS_ASSERTION(bytesRead == aCount, "failed to read data");
  }
  else if (mDataFormat == FILE_AS_ARRAYBUFFER) {
    PRUint32 bytesRead = 0;
    aInputStream->Read((char*)JS_GetArrayBufferData(mResultArrayBuffer, NULL) + aOffset,
                       aCount, &bytesRead);
    NS_ASSERTION(bytesRead == aCount, "failed to read data");
  }
  else {
    //Update memory buffer to reflect the contents of the file
    mFileData = (char *)PR_Realloc(mFileData, aOffset + aCount);
    NS_ENSURE_TRUE(mFileData, NS_ERROR_OUT_OF_MEMORY);

    PRUint32 bytesRead = 0;
    aInputStream->Read(mFileData + aOffset, aCount, &bytesRead);
    NS_ASSERTION(bytesRead == aCount, "failed to read data");

    mDataLen += aCount;
  }

  return NS_OK;
}

nsresult
nsDOMFileReader::DoOnStopRequest(nsIRequest *aRequest,
                                 nsISupports *aContext,
                                 nsresult aStatus,
                                 nsAString& aSuccessEvent,
                                 nsAString& aTerminationEvent)
{
  aSuccessEvent = NS_LITERAL_STRING(LOAD_STR);
  aTerminationEvent = NS_LITERAL_STRING(LOADEND_STR);

  // Clear out the data if necessary
  if (NS_FAILED(aStatus)) {
    FreeFileData();
    return NS_OK;
  }

  nsresult rv = NS_OK;
  switch (mDataFormat) {
    case FILE_AS_ARRAYBUFFER:
      break; //Already accumulated mResultArrayBuffer
    case FILE_AS_BINARY:
      break; //Already accumulated mResult
    case FILE_AS_TEXT:
      rv = GetAsText(mCharset, mFileData, mDataLen, mResult);
      break;
    case FILE_AS_DATAURL:
      rv = GetAsDataURL(mFile, mFileData, mDataLen, mResult);
      break;
  }
  
  mResult.SetIsVoid(false);

  FreeFileData();

  return rv;
}

// Helper methods

nsresult
nsDOMFileReader::ReadFileContent(JSContext* aCx,
                                 nsIDOMBlob* aFile,
                                 const nsAString &aCharset,
                                 eDataFormat aDataFormat)
{
  nsresult rv;
  NS_ENSURE_TRUE(aFile, NS_ERROR_NULL_POINTER);

  //Implicit abort to clear any other activity going on
  Abort();
  mError = nsnull;
  SetDOMStringToNull(mResult);
  mTransferred = 0;
  mTotal = 0;
  mReadyState = nsIDOMFileReader::EMPTY;
  FreeFileData();

  mFile = aFile;
  mDataFormat = aDataFormat;
  CopyUTF16toUTF8(aCharset, mCharset);

  //Establish a channel with our file
  {
    // Hold the internal URL alive only as long as necessary
    // After the channel is created it will own whatever is backing
    // the DOMFile.
    nsDOMFileInternalUrlHolder urlHolder(mFile, mPrincipal);

    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), urlHolder.mUrl);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = NS_NewChannel(getter_AddRefs(mChannel), uri);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  //Obtain the total size of the file before reading
  mTotal = mozilla::dom::kUnknownSize;
  mFile->GetSize(&mTotal);

  rv = mChannel->AsyncOpen(this, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  //FileReader should be in loading state here
  mReadyState = nsIDOMFileReader::LOADING;
  DispatchProgressEvent(NS_LITERAL_STRING(LOADSTART_STR));
  
  if (mDataFormat == FILE_AS_ARRAYBUFFER) {
    RootResultArrayBuffer();
    mResultArrayBuffer = JS_NewArrayBuffer(aCx, mTotal);
    if (!mResultArrayBuffer) {
      NS_WARNING("Failed to create JS array buffer");
      return NS_ERROR_FAILURE;
    }
  }
 
  return NS_OK;
}

nsresult
nsDOMFileReader::GetAsText(const nsACString &aCharset,
                           const char *aFileData,
                           PRUint32 aDataLen,
                           nsAString& aResult)
{
  nsresult rv;
  nsCAutoString charsetGuess;
  if (!aCharset.IsEmpty()) {
    charsetGuess = aCharset;
  } else {
    rv = GuessCharset(aFileData, aDataLen, charsetGuess);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCAutoString charset;
  rv = nsCharsetAlias::GetPreferred(charsetGuess, charset);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ConvertStream(aFileData, aDataLen, charset.get(), aResult);

  return NS_OK;
}

nsresult
nsDOMFileReader::GetAsDataURL(nsIDOMBlob *aFile,
                              const char *aFileData,
                              PRUint32 aDataLen,
                              nsAString& aResult)
{
  aResult.AssignLiteral("data:");

  nsresult rv;
  nsString contentType;
  rv = aFile->GetType(contentType);
  if (NS_SUCCEEDED(rv) && !contentType.IsEmpty()) {
    aResult.Append(contentType);
  } else {
    aResult.AppendLiteral("application/octet-stream");
  }
  aResult.AppendLiteral(";base64,");

  PRUint32 totalRead = 0;
  while (aDataLen > totalRead) {
    PRUint32 numEncode = 4096;
    PRUint32 amtRemaining = aDataLen - totalRead;
    if (numEncode > amtRemaining)
      numEncode = amtRemaining;

    //Unless this is the end of the file, encode in multiples of 3
    if (numEncode > 3) {
      PRUint32 leftOver = numEncode % 3;
      numEncode -= leftOver;
    }

    //Out buffer should be at least 4/3rds the read buf, plus a terminator
    char *base64 = PL_Base64Encode(aFileData + totalRead, numEncode, nsnull);
    AppendASCIItoUTF16(nsDependentCString(base64), aResult);
    PR_Free(base64);

    totalRead += numEncode;
  }

  return NS_OK;
}

nsresult
nsDOMFileReader::ConvertStream(const char *aFileData,
                               PRUint32 aDataLen,
                               const char *aCharset,
                               nsAString &aResult)
{
  nsresult rv;
  nsCOMPtr<nsICharsetConverterManager> charsetConverter = 
    do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIUnicodeDecoder> unicodeDecoder;
  rv = charsetConverter->GetUnicodeDecoder(aCharset, getter_AddRefs(unicodeDecoder));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 destLength;
  rv = unicodeDecoder->GetMaxLength(aFileData, aDataLen, &destLength);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aResult.SetLength(destLength, fallible_t()))
    return NS_ERROR_OUT_OF_MEMORY;

  PRInt32 srcLength = aDataLen;
  rv = unicodeDecoder->Convert(aFileData, &srcLength, aResult.BeginWriting(), &destLength);
  aResult.SetLength(destLength); //Trim down to the correct size

  return rv;
}

nsresult
nsDOMFileReader::GuessCharset(const char *aFileData,
                              PRUint32 aDataLen,
                              nsACString &aCharset)
{
  // First try the universal charset detector
  nsCOMPtr<nsICharsetDetector> detector
    = do_CreateInstance(NS_CHARSET_DETECTOR_CONTRACTID_BASE
                        "universal_charset_detector");
  if (!detector) {
    // No universal charset detector, try the default charset detector
    const nsAdoptingCString& detectorName =
      Preferences::GetLocalizedCString("intl.charset.detector");
    if (!detectorName.IsEmpty()) {
      nsCAutoString detectorContractID;
      detectorContractID.AssignLiteral(NS_CHARSET_DETECTOR_CONTRACTID_BASE);
      detectorContractID += detectorName;
      detector = do_CreateInstance(detectorContractID.get());
    }
  }

  nsresult rv;
  // The charset detector doesn't work for empty (null) aFileData. Testing
  // aDataLen instead of aFileData so that we catch potential errors.
  if (detector && aDataLen != 0) {
    mCharset.Truncate();
    detector->Init(this);

    bool done;

    rv = detector->DoIt(aFileData, aDataLen, &done);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = detector->Done();
    NS_ENSURE_SUCCESS(rv, rv);

    aCharset = mCharset;
  } else {
    // no charset detector available, check the BOM
    unsigned char sniffBuf[3];
    PRUint32 numRead = (aDataLen >= sizeof(sniffBuf) ? sizeof(sniffBuf) : aDataLen);
    memcpy(sniffBuf, aFileData, numRead);

    if (numRead >= 2 &&
               sniffBuf[0] == 0xfe &&
               sniffBuf[1] == 0xff) {
      aCharset = "UTF-16BE";
    } else if (numRead >= 2 &&
               sniffBuf[0] == 0xff &&
               sniffBuf[1] == 0xfe) {
      aCharset = "UTF-16LE";
    } else if (numRead >= 3 &&
               sniffBuf[0] == 0xef &&
               sniffBuf[1] == 0xbb &&
               sniffBuf[2] == 0xbf) {
      aCharset = "UTF-8";
    }
  }

  if (aCharset.IsEmpty()) {
    // no charset detected, default to the system charset
    nsCOMPtr<nsIPlatformCharset> platformCharset =
      do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
      rv = platformCharset->GetCharset(kPlatformCharsetSel_PlainTextInFile,
                                       aCharset);
    }
  }

  if (aCharset.IsEmpty()) {
    // no sniffed or default charset, try UTF-8
    aCharset.AssignLiteral("UTF-8");
  }

  return NS_OK;
}

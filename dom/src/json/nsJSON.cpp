/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"
#include "jsdbgapi.h"
#include "nsIServiceManager.h"
#include "nsJSON.h"
#include "nsIXPConnect.h"
#include "nsIXPCScriptable.h"
#include "nsStreamUtils.h"
#include "nsIInputStream.h"
#include "nsStringStream.h"
#include "nsICharsetConverterManager.h"
#include "nsXPCOMStrings.h"
#include "nsNetUtil.h"
#include "nsContentUtils.h"
#include "nsIScriptError.h"
#include "nsCRTGlue.h"
#include "nsAutoPtr.h"
#include "nsIScriptSecurityManager.h"
#include <algorithm>

static const char kXPConnectServiceCID[] = "@mozilla.org/js/xpc/XPConnect;1";

#define JSON_STREAM_BUFSIZE 4096

NS_INTERFACE_MAP_BEGIN(nsJSON)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIJSON)
  NS_INTERFACE_MAP_ENTRY(nsIJSON)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsJSON)
NS_IMPL_RELEASE(nsJSON)

nsJSON::nsJSON()
{
}

nsJSON::~nsJSON()
{
}

enum DeprecationWarning { EncodeWarning, DecodeWarning };

static nsresult
WarnDeprecatedMethod(DeprecationWarning warning)
{
  return nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                         "DOM Core", nullptr,
                                         nsContentUtils::eDOM_PROPERTIES,
                                         warning == EncodeWarning
                                         ? "nsIJSONEncodeDeprecatedWarning"
                                         : "nsIJSONDecodeDeprecatedWarning");
}

NS_IMETHODIMP
nsJSON::Encode(const JS::Value& aValue, JSContext* cx, uint8_t aArgc,
               nsAString &aJSON)
{
  // This function should only be called from JS.
  nsresult rv = WarnDeprecatedMethod(EncodeWarning);
  if (NS_FAILED(rv))
    return rv;

  if (aArgc == 0) {
    aJSON.Truncate();
    aJSON.SetIsVoid(true);
    return NS_OK;
  }

  nsJSONWriter writer;
  rv = EncodeInternal(cx, aValue, &writer);

  // FIXME: bug 408838. Get exception types sorted out
  if (NS_SUCCEEDED(rv) || rv == NS_ERROR_INVALID_ARG) {
    rv = NS_OK;
    // if we didn't consume anything, it's not JSON, so return null
    if (!writer.DidWrite()) {
      aJSON.Truncate();
      aJSON.SetIsVoid(true);
    } else {
      writer.FlushBuffer();
      aJSON.Append(writer.mOutputString);
    }
  }

  return rv;
}

static const char UTF8BOM[] = "\xEF\xBB\xBF";
static const char UTF16LEBOM[] = "\xFF\xFE";
static const char UTF16BEBOM[] = "\xFE\xFF";

static nsresult CheckCharset(const char* aCharset)
{
  // Check that the charset is permissible
  if (!(strcmp(aCharset, "UTF-8") == 0 ||
        strcmp(aCharset, "UTF-16LE") == 0 ||
        strcmp(aCharset, "UTF-16BE") == 0)) {
    return NS_ERROR_INVALID_ARG;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsJSON::EncodeToStream(nsIOutputStream *aStream,
                       const char* aCharset,
                       const bool aWriteBOM,
                       const JS::Value& val,
                       JSContext* cx,
                       uint8_t aArgc)
{
  // This function should only be called from JS.
  NS_ENSURE_ARG(aStream);
  nsresult rv;

  rv = CheckCharset(aCharset);
  NS_ENSURE_SUCCESS(rv, rv);

  // Check to see if we have a buffered stream
  nsCOMPtr<nsIOutputStream> bufferedStream;
  // FIXME: bug 408514.
  // NS_OutputStreamIsBuffered(aStream) asserts on file streams...
  //if (!NS_OutputStreamIsBuffered(aStream)) {
    rv = NS_NewBufferedOutputStream(getter_AddRefs(bufferedStream),
                                    aStream, 4096);
    NS_ENSURE_SUCCESS(rv, rv);
  //  aStream = bufferedStream;
  //}

  uint32_t ignored;
  if (aWriteBOM) {
    if (strcmp(aCharset, "UTF-8") == 0)
      rv = aStream->Write(UTF8BOM, 3, &ignored);
    else if (strcmp(aCharset, "UTF-16LE") == 0)
      rv = aStream->Write(UTF16LEBOM, 2, &ignored);
    else if (strcmp(aCharset, "UTF-16BE") == 0)
      rv = aStream->Write(UTF16BEBOM, 2, &ignored);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsJSONWriter writer(bufferedStream);
  rv = writer.SetCharset(aCharset);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aArgc == 0) {
    return NS_OK;
  }

  rv = EncodeInternal(cx, val, &writer);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = bufferedStream->Flush();

  return rv;
}

static JSBool
WriteCallback(const jschar *buf, uint32_t len, void *data)
{
  nsJSONWriter *writer = static_cast<nsJSONWriter*>(data);
  nsresult rv =  writer->Write((const PRUnichar*)buf, (uint32_t)len);
  if (NS_FAILED(rv))
    return JS_FALSE;

  return JS_TRUE;
}

NS_IMETHODIMP
nsJSON::EncodeFromJSVal(JS::Value *value, JSContext *cx, nsAString &result)
{
  result.Truncate();

  // Begin a new request
  JSAutoRequest ar(cx);

  mozilla::Maybe<JSAutoCompartment> ac;
  if (value->isObject()) {
    ac.construct(cx, &value->toObject());
  }

  nsJSONWriter writer;
  if (!JS_Stringify(cx, value, NULL, JSVAL_NULL, WriteCallback, &writer)) {
    return NS_ERROR_XPC_BAD_CONVERT_JS;
  }

  NS_ENSURE_TRUE(writer.DidWrite(), NS_ERROR_UNEXPECTED);
  writer.FlushBuffer();
  result.Assign(writer.mOutputString);
  return NS_OK;
}

nsresult
nsJSON::EncodeInternal(JSContext* cx, const JS::Value& aValue,
                       nsJSONWriter* writer)
{
  JSAutoRequest ar(cx);

  // Backward compatibility:
  // nsIJSON does not allow to serialize anything other than objects
  if (!aValue.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }

  JSObject* obj = &aValue.toObject();

  JS::Value val = aValue;

  /* Backward compatibility:
   * Manually call toJSON if implemented by the object and check that
   * the result is still an object
   * Note: It is perfectly fine to not implement toJSON, so it is
   * perfectly fine for GetMethod to fail
   */
  JS::Value toJSON;
  if (JS_GetMethod(cx, obj, "toJSON", NULL, &toJSON) &&
      !JSVAL_IS_PRIMITIVE(toJSON) &&
      JS_ObjectIsCallable(cx, JSVAL_TO_OBJECT(toJSON))) {
    // If toJSON is implemented, it must not throw
    if (!JS_CallFunctionValue(cx, obj, toJSON, 0, NULL, &val)) {
      if (JS_IsExceptionPending(cx))
        // passing NS_OK will throw the pending exception
        return NS_OK;

      // No exception, but still failed
      return NS_ERROR_FAILURE;
    }

    // Backward compatibility:
    // nsIJSON does not allow to serialize anything other than objects
    if (JSVAL_IS_PRIMITIVE(val))
      return NS_ERROR_INVALID_ARG;
  }
  // GetMethod may have thrown
  else if (JS_IsExceptionPending(cx))
    // passing NS_OK will throw the pending exception
    return NS_OK;

  // Backward compatibility:
  // function shall not pass, just "plain" objects and arrays
  JSType type = JS_TypeOfValue(cx, val);
  if (type == JSTYPE_FUNCTION)
    return NS_ERROR_INVALID_ARG;

  // We're good now; try to stringify
  if (!JS_Stringify(cx, &val, NULL, JSVAL_NULL, WriteCallback, writer))
    return NS_ERROR_FAILURE;

  return NS_OK;
}


nsJSONWriter::nsJSONWriter() : mStream(nullptr),
                               mBuffer(nullptr),
                               mBufferCount(0),
                               mDidWrite(false),
                               mEncoder(nullptr)
{
}

nsJSONWriter::nsJSONWriter(nsIOutputStream *aStream) : mStream(aStream),
                                                       mBuffer(nullptr),
                                                       mBufferCount(0),
                                                       mDidWrite(false),
                                                       mEncoder(nullptr)
{
}

nsJSONWriter::~nsJSONWriter()
{
  delete [] mBuffer;
}

nsresult
nsJSONWriter::SetCharset(const char* aCharset)
{
  nsresult rv = NS_OK;
  if (mStream) {
    nsCOMPtr<nsICharsetConverterManager> ccm =
      do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = ccm->GetUnicodeEncoder(aCharset, getter_AddRefs(mEncoder));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mEncoder->SetOutputErrorBehavior(nsIUnicodeEncoder::kOnError_Signal,
                                          nullptr, '\0');
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return rv;
}

nsresult
nsJSONWriter::Write(const PRUnichar *aBuffer, uint32_t aLength)
{
  if (mStream) {
    return WriteToStream(mStream, mEncoder, aBuffer, aLength);
  }

  if (!mDidWrite) {
    mBuffer = new PRUnichar[JSON_STREAM_BUFSIZE];
    if (!mBuffer)
      return NS_ERROR_OUT_OF_MEMORY;
    mDidWrite = true;
  }

  if (JSON_STREAM_BUFSIZE <= aLength + mBufferCount) {
    mOutputString.Append(mBuffer, mBufferCount);
    mBufferCount = 0;
  }

  if (JSON_STREAM_BUFSIZE <= aLength) {
    // we know mBufferCount is 0 because we know we hit the if above
    mOutputString.Append(aBuffer, aLength);
  } else {
    memcpy(&mBuffer[mBufferCount], aBuffer, aLength * sizeof(PRUnichar));
    mBufferCount += aLength;
  }

  return NS_OK;
}

bool nsJSONWriter::DidWrite()
{
  return mDidWrite;
}

void
nsJSONWriter::FlushBuffer()
{
  mOutputString.Append(mBuffer, mBufferCount);
}

nsresult
nsJSONWriter::WriteToStream(nsIOutputStream *aStream,
                            nsIUnicodeEncoder *encoder,
                            const PRUnichar *aBuffer,
                            uint32_t aLength)
{
  nsresult rv;
  int32_t srcLength = aLength;
  uint32_t bytesWritten;

  // The bytes written to the stream might differ from the PRUnichar size
  int32_t aDestLength;
  rv = encoder->GetMaxLength(aBuffer, srcLength, &aDestLength);
  NS_ENSURE_SUCCESS(rv, rv);

  // create the buffer we need
  char* destBuf = (char *) NS_Alloc(aDestLength);
  if (!destBuf)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = encoder->Convert(aBuffer, &srcLength, destBuf, &aDestLength);
  if (NS_SUCCEEDED(rv))
    rv = aStream->Write(destBuf, aDestLength, &bytesWritten);

  NS_Free(destBuf);
  mDidWrite = true;

  return rv;
}

NS_IMETHODIMP
nsJSON::Decode(const nsAString& json, JSContext* cx, JS::Value* aRetval)
{
  nsresult rv = WarnDeprecatedMethod(DecodeWarning);
  if (NS_FAILED(rv))
    return rv;

  const PRUnichar *data;
  uint32_t len = NS_StringGetData(json, &data);
  nsCOMPtr<nsIInputStream> stream;
  rv = NS_NewByteInputStream(getter_AddRefs(stream),
                             reinterpret_cast<const char*>(data),
                             len * sizeof(PRUnichar),
                             NS_ASSIGNMENT_DEPEND);
  NS_ENSURE_SUCCESS(rv, rv);
  return DecodeInternal(cx, stream, len, false, aRetval);
}

NS_IMETHODIMP
nsJSON::DecodeFromStream(nsIInputStream *aStream, int32_t aContentLength,
                         JSContext* cx, JS::Value* aRetval)
{
  return DecodeInternal(cx, aStream, aContentLength, true, aRetval);
}

NS_IMETHODIMP
nsJSON::DecodeToJSVal(const nsAString &str, JSContext *cx, JS::Value *result)
{
  JSAutoRequest ar(cx);

  if (!JS_ParseJSON(cx, static_cast<const jschar*>(PromiseFlatString(str).get()),
                    str.Length(), result)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

nsresult
nsJSON::DecodeInternal(JSContext* cx,
                       nsIInputStream *aStream,
                       int32_t aContentLength,
                       bool aNeedsConverter,
                       JS::Value* aRetval,
                       DecodingMode mode /* = STRICT */)
{
  JSAutoRequest ar(cx);

  // Consume the stream
  nsCOMPtr<nsIChannel> jsonChannel;
  if (!mURI) {
    NS_NewURI(getter_AddRefs(mURI), NS_LITERAL_CSTRING("about:blank"), 0, 0 );
    if (!mURI)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv =
    NS_NewInputStreamChannel(getter_AddRefs(jsonChannel), mURI, aStream,
                             NS_LITERAL_CSTRING("application/json"));
  if (!jsonChannel || NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  nsRefPtr<nsJSONListener> jsonListener =
    new nsJSONListener(cx, aRetval, aNeedsConverter, mode);

  //XXX this stream pattern should be consolidated in netwerk
  rv = jsonListener->OnStartRequest(jsonChannel, nullptr);
  if (NS_FAILED(rv)) {
    jsonChannel->Cancel(rv);
    return rv;
  }

  nsresult status;
  jsonChannel->GetStatus(&status);
  uint64_t offset = 0;
  while (NS_SUCCEEDED(status)) {
    uint64_t available;
    rv = aStream->Available(&available);
    if (rv == NS_BASE_STREAM_CLOSED) {
      rv = NS_OK;
      break;
    }
    if (NS_FAILED(rv)) {
      jsonChannel->Cancel(rv);
      break;
    }
    if (!available)
      break; // blocking input stream has none available when done

    if (available > UINT32_MAX)
      available = UINT32_MAX;

    rv = jsonListener->OnDataAvailable(jsonChannel, nullptr,
                                       aStream,
                                       offset,
                                       (uint32_t)available);
    if (NS_FAILED(rv)) {
      jsonChannel->Cancel(rv);
      break;
    }

    offset += available;
    jsonChannel->GetStatus(&status);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  rv = jsonListener->OnStopRequest(jsonChannel, nullptr, status);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


NS_IMETHODIMP
nsJSON::LegacyDecode(const nsAString& json, JSContext* cx, JS::Value* aRetval)
{
  const PRUnichar *data;
  uint32_t len = NS_StringGetData(json, &data);
  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = NS_NewByteInputStream(getter_AddRefs(stream),
                                      (const char*) data,
                                      len * sizeof(PRUnichar),
                                      NS_ASSIGNMENT_DEPEND);
  NS_ENSURE_SUCCESS(rv, rv);
  return DecodeInternal(cx, stream, len, false, aRetval, LEGACY);
}

NS_IMETHODIMP
nsJSON::LegacyDecodeFromStream(nsIInputStream *aStream, int32_t aContentLength,
                               JSContext* cx, JS::Value* aRetval)
{
  return DecodeInternal(cx, aStream, aContentLength, true, aRetval, LEGACY);
}

NS_IMETHODIMP
nsJSON::LegacyDecodeToJSVal(const nsAString &str, JSContext *cx, JS::Value *result)
{
  JSAutoRequest ar(cx);

  JS::RootedValue reviver(cx, JS::NullValue()), value(cx);

  JS::StableCharPtr chars(static_cast<const jschar*>(PromiseFlatString(str).get()),
                          str.Length());
  if (!js::ParseJSONWithReviver(cx, chars, str.Length(), reviver,
                                &value, LEGACY)) {
    return NS_ERROR_UNEXPECTED;
  }

  *result = value;
  return NS_OK;
}

nsresult
NS_NewJSON(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  nsJSON* json = new nsJSON();
  if (!json)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(json);
  *aResult = json;

  return NS_OK;
}

nsJSONListener::nsJSONListener(JSContext *cx, JS::Value *rootVal,
                               bool needsConverter,
                               DecodingMode mode /* = STRICT */)
  : mNeedsConverter(needsConverter), 
    mCx(cx),
    mRootVal(rootVal),
    mDecodingMode(mode)
{
}

nsJSONListener::~nsJSONListener()
{
}

NS_INTERFACE_MAP_BEGIN(nsJSONListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsJSONListener)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsJSONListener)
NS_IMPL_RELEASE(nsJSONListener)

NS_IMETHODIMP
nsJSONListener::OnStartRequest(nsIRequest *aRequest, nsISupports *aContext)
{
  mSniffBuffer.Truncate();
  mDecoder = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
nsJSONListener::OnStopRequest(nsIRequest *aRequest, nsISupports *aContext,
                              nsresult aStatusCode)
{
  nsresult rv;

  // This can happen with short UTF-8 messages (<4 bytes)
  if (!mSniffBuffer.IsEmpty()) {
    // Just consume mSniffBuffer
    rv = ProcessBytes(nullptr, 0);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  JS::RootedValue reviver(mCx, JS::NullValue()), value(mCx);

  JS::StableCharPtr chars(reinterpret_cast<const jschar*>(mBufferedChars.Elements()),
                          mBufferedChars.Length());
  JSBool ok = js::ParseJSONWithReviver(mCx, chars,
                                       (uint32_t) mBufferedChars.Length(),
                                       reviver, &value,
                                       mDecodingMode);

  *mRootVal = value;
  mBufferedChars.TruncateLength(0);
  return ok ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsJSONListener::OnDataAvailable(nsIRequest *aRequest, nsISupports *aContext,
                                nsIInputStream *aStream,
                                uint64_t aOffset, uint32_t aLength)
{
  nsresult rv = NS_OK;

  if (mNeedsConverter && mSniffBuffer.Length() < 4) {
    uint32_t readCount = (aLength < 4) ? aLength : 4;
    rv = NS_ConsumeStream(aStream, readCount, mSniffBuffer);
    NS_ENSURE_SUCCESS(rv, rv);

    if (mSniffBuffer.Length() < 4)
      return NS_OK;
  }
  
  char buffer[JSON_STREAM_BUFSIZE];
  unsigned long bytesRemaining = aLength - mSniffBuffer.Length();
  while (bytesRemaining) {
    unsigned int bytesRead;
    rv = aStream->Read(buffer,
                       std::min((unsigned long)sizeof(buffer), bytesRemaining),
                       &bytesRead);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = ProcessBytes(buffer, bytesRead);
    NS_ENSURE_SUCCESS(rv, rv);
    bytesRemaining -= bytesRead;
  }

  return rv;
}

nsresult
nsJSONListener::ProcessBytes(const char* aBuffer, uint32_t aByteLength)
{
  nsresult rv;
  // Check for BOM, or sniff charset
  nsAutoCString charset;
  if (mNeedsConverter && !mDecoder) {
    if (!nsContentUtils::CheckForBOM((const unsigned char*) mSniffBuffer.get(),
                                      mSniffBuffer.Length(), charset)) {
      // OK, found no BOM, sniff the first character to see what this is
      // See section 3 of RFC4627 for details on why this works.
      const char *buffer = mSniffBuffer.get();
      if (mSniffBuffer.Length() >= 4) {
        if (buffer[0] == 0x00 && buffer[1] != 0x00 &&
            buffer[2] == 0x00 && buffer[3] != 0x00) {
          charset = "UTF-16BE";
        } else if (buffer[0] != 0x00 && buffer[1] == 0x00 &&
                   buffer[2] != 0x00 && buffer[3] == 0x00) {
          charset = "UTF-16LE";
        } else if (buffer[0] != 0x00 && buffer[1] != 0x00 &&
                   buffer[2] != 0x00 && buffer[3] != 0x00) {
          charset = "UTF-8";
        }
      } else {
        // Not enough bytes to sniff, assume UTF-8
        charset = "UTF-8";
      }
    }

    // We should have a unicode charset by now
    rv = CheckCharset(charset.get());
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsICharsetConverterManager> ccm =
        do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = ccm->GetUnicodeDecoderRaw(charset.get(), getter_AddRefs(mDecoder));
    NS_ENSURE_SUCCESS(rv, rv);

    // consume the sniffed bytes
    rv = ConsumeConverted(mSniffBuffer.get(), mSniffBuffer.Length());
    NS_ENSURE_SUCCESS(rv, rv);
    mSniffBuffer.Truncate();
  }

  if (!aBuffer)
    return NS_OK;

  if (mNeedsConverter) {
    rv = ConsumeConverted(aBuffer, aByteLength);
  } else {
    uint32_t unichars = aByteLength / sizeof(PRUnichar);
    rv = Consume((PRUnichar *) aBuffer, unichars);
  }

  return rv;
}

nsresult
nsJSONListener::ConsumeConverted(const char* aBuffer, uint32_t aByteLength)
{
  nsresult rv;
  int32_t unicharLength = 0;
  int32_t srcLen = aByteLength;

  rv = mDecoder->GetMaxLength(aBuffer, srcLen, &unicharLength);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUnichar* endelems = mBufferedChars.AppendElements(unicharLength);
  int32_t preLength = unicharLength;
  rv = mDecoder->Convert(aBuffer, &srcLen, endelems, &unicharLength);
  if (NS_FAILED(rv))
    return rv;
  NS_ABORT_IF_FALSE(preLength >= unicharLength, "GetMaxLength lied");
  if (preLength > unicharLength)
    mBufferedChars.TruncateLength(mBufferedChars.Length() - (preLength - unicharLength));
  return NS_OK;
}

nsresult
nsJSONListener::Consume(const PRUnichar* aBuffer, uint32_t aByteLength)
{
  if (!mBufferedChars.AppendElements(aBuffer, aByteLength))
    return NS_ERROR_FAILURE;

  return NS_OK;
}

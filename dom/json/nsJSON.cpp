/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"
#include "js/CharacterEncoding.h"
#include "nsJSON.h"
#include "nsIXPConnect.h"
#include "nsIXPCScriptable.h"
#include "nsStreamUtils.h"
#include "nsIInputStream.h"
#include "nsStringStream.h"
#include "nsNetUtil.h"
#include "nsIURI.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsIScriptError.h"
#include "nsCRTGlue.h"
#include "nsIScriptSecurityManager.h"
#include "NullPrincipal.h"
#include "mozilla/Maybe.h"
#include <algorithm>

using namespace mozilla;

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
                                         NS_LITERAL_CSTRING("DOM Core"), nullptr,
                                         nsContentUtils::eDOM_PROPERTIES,
                                         warning == EncodeWarning
                                         ? "nsIJSONEncodeDeprecatedWarning"
                                         : "nsIJSONDecodeDeprecatedWarning");
}

NS_IMETHODIMP
nsJSON::Encode(JS::Handle<JS::Value> aValue, JSContext* cx, uint8_t aArgc,
               nsAString &aJSON)
{
  // This function should only be called from JS.
  nsresult rv = WarnDeprecatedMethod(EncodeWarning);
  if (NS_FAILED(rv))
    return rv;

  if (aArgc == 0) {
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
      aJSON.SetIsVoid(true);
    } else {
      writer.FlushBuffer();
      aJSON.Append(writer.mOutputString);
    }
  }

  return rv;
}

static const char UTF8BOM[] = "\xEF\xBB\xBF";

static nsresult CheckCharset(const char* aCharset)
{
  // Check that the charset is permissible
  if (!(strcmp(aCharset, "UTF-8") == 0)) {
    return NS_ERROR_INVALID_ARG;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsJSON::EncodeToStream(nsIOutputStream *aStream,
                       const char* aCharset,
                       const bool aWriteBOM,
                       JS::Handle<JS::Value> val,
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
    rv = aStream->Write(UTF8BOM, 3, &ignored);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsJSONWriter writer(bufferedStream);

  if (aArgc == 0) {
    return NS_OK;
  }

  rv = EncodeInternal(cx, val, &writer);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = bufferedStream->Flush();

  return rv;
}

static bool
WriteCallback(const char16_t *buf, uint32_t len, void *data)
{
  nsJSONWriter *writer = static_cast<nsJSONWriter*>(data);
  nsresult rv =  writer->Write((const char16_t*)buf, (uint32_t)len);
  if (NS_FAILED(rv))
    return false;

  return true;
}

NS_IMETHODIMP
nsJSON::EncodeFromJSVal(JS::Value *value, JSContext *cx, nsAString &result)
{
  result.Truncate();

  mozilla::Maybe<JSAutoCompartment> ac;
  if (value->isObject()) {
    JS::Rooted<JSObject*> obj(cx, &value->toObject());
    ac.emplace(cx, obj);
  }

  nsJSONWriter writer;
  JS::Rooted<JS::Value> vp(cx, *value);
  if (!JS_Stringify(cx, &vp, nullptr, JS::NullHandleValue, WriteCallback, &writer)) {
    return NS_ERROR_XPC_BAD_CONVERT_JS;
  }
  *value = vp;

  NS_ENSURE_TRUE(writer.DidWrite(), NS_ERROR_UNEXPECTED);
  writer.FlushBuffer();
  result.Assign(writer.mOutputString);
  return NS_OK;
}

nsresult
nsJSON::EncodeInternal(JSContext* cx, const JS::Value& aValue,
                       nsJSONWriter* writer)
{
  // Backward compatibility:
  // nsIJSON does not allow to serialize anything other than objects
  if (!aValue.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }
  JS::Rooted<JSObject*> obj(cx, &aValue.toObject());

  /* Backward compatibility:
   * Manually call toJSON if implemented by the object and check that
   * the result is still an object
   * Note: It is perfectly fine to not implement toJSON, so it is
   * perfectly fine for GetMethod to fail
   */
  JS::Rooted<JS::Value> val(cx, aValue);
  JS::Rooted<JS::Value> toJSON(cx);
  if (JS_GetProperty(cx, obj, "toJSON", &toJSON) &&
      toJSON.isObject() && JS::IsCallable(&toJSON.toObject())) {
    // If toJSON is implemented, it must not throw
    if (!JS_CallFunctionValue(cx, obj, toJSON, JS::HandleValueArray::empty(), &val)) {
      if (JS_IsExceptionPending(cx))
        // passing NS_OK will throw the pending exception
        return NS_OK;

      // No exception, but still failed
      return NS_ERROR_FAILURE;
    }

    // Backward compatibility:
    // nsIJSON does not allow to serialize anything other than objects
    if (val.isPrimitive())
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
  if (!JS_Stringify(cx, &val, nullptr, JS::NullHandleValue, WriteCallback, writer))
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

nsJSONWriter::nsJSONWriter(nsIOutputStream* aStream)
  : mStream(aStream)
  , mBuffer(nullptr)
  , mBufferCount(0)
  , mDidWrite(false)
  , mEncoder(UTF_8_ENCODING->NewEncoder())
{
}

nsJSONWriter::~nsJSONWriter()
{
  delete [] mBuffer;
}

nsresult
nsJSONWriter::Write(const char16_t *aBuffer, uint32_t aLength)
{
  if (mStream) {
    return WriteToStream(mStream, mEncoder.get(), aBuffer, aLength);
  }

  if (!mDidWrite) {
    mBuffer = new char16_t[JSON_STREAM_BUFSIZE];
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
    memcpy(&mBuffer[mBufferCount], aBuffer, aLength * sizeof(char16_t));
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
nsJSONWriter::WriteToStream(nsIOutputStream* aStream,
                            Encoder* encoder,
                            const char16_t* aBuffer,
                            uint32_t aLength)
{
  uint8_t buffer[1024];
  auto dst = MakeSpan(buffer);
  auto src = MakeSpan(aBuffer, aLength);

  for (;;) {
    uint32_t result;
    size_t read;
    size_t written;
    bool hadErrors;
    Tie(result, read, written, hadErrors) =
      encoder->EncodeFromUTF16(src, dst, false);
    Unused << hadErrors;
    src = src.From(read);
    uint32_t ignored;
    nsresult rv =
      aStream->Write(reinterpret_cast<const char*>(buffer), written, &ignored);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (result == kInputEmpty) {
      mDidWrite = true;
      return NS_OK;
    }
  }
}

NS_IMETHODIMP
nsJSON::Decode(const nsAString& json, JSContext* cx,
               JS::MutableHandle<JS::Value> aRetval)
{
  nsresult rv = WarnDeprecatedMethod(DecodeWarning);
  if (NS_FAILED(rv))
    return rv;

  const char16_t *data = json.BeginReading();
  uint32_t len = json.Length();
  nsCOMPtr<nsIInputStream> stream;
  rv = NS_NewByteInputStream(getter_AddRefs(stream),
                             reinterpret_cast<const char*>(data),
                             len * sizeof(char16_t),
                             NS_ASSIGNMENT_DEPEND);
  NS_ENSURE_SUCCESS(rv, rv);
  return DecodeInternal(cx, stream, len, false, aRetval);
}

NS_IMETHODIMP
nsJSON::DecodeFromStream(nsIInputStream *aStream, int32_t aContentLength,
                         JSContext* cx, JS::MutableHandle<JS::Value> aRetval)
{
  return DecodeInternal(cx, aStream, aContentLength, true, aRetval);
}

NS_IMETHODIMP
nsJSON::DecodeToJSVal(const nsAString &str, JSContext *cx,
                      JS::MutableHandle<JS::Value> result)
{
  if (!JS_ParseJSON(cx, static_cast<const char16_t*>(PromiseFlatString(str).get()),
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
                       JS::MutableHandle<JS::Value> aRetval)
{
  // Consume the stream
  nsCOMPtr<nsIChannel> jsonChannel;
  if (!mURI) {
    NS_NewURI(getter_AddRefs(mURI), NS_LITERAL_CSTRING("about:blank"), 0, 0 );
    if (!mURI)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv;
  nsCOMPtr<nsIPrincipal> nullPrincipal = NullPrincipal::Create();

  // The ::Decode function is deprecated [Bug 675797] and the following
  // channel is never openend, so it does not matter what securityFlags
  // we pass to NS_NewInputStreamChannel here.
  rv = NS_NewInputStreamChannel(getter_AddRefs(jsonChannel),
                                mURI,
                                aStream,
                                nullPrincipal,
                                nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED,
                                nsIContentPolicy::TYPE_OTHER,
                                NS_LITERAL_CSTRING("application/json"));

  if (!jsonChannel || NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  RefPtr<nsJSONListener> jsonListener =
    new nsJSONListener(cx, aRetval.address(), aNeedsConverter);

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

nsresult
NS_NewJSON(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  nsJSON* json = new nsJSON();
  NS_ADDREF(json);
  *aResult = json;

  return NS_OK;
}

nsJSONListener::nsJSONListener(JSContext *cx, JS::Value *rootVal,
                               bool needsConverter)
  : mNeedsConverter(needsConverter),
    mCx(cx),
    mRootVal(rootVal)
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
  mDecoder = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
nsJSONListener::OnStopRequest(nsIRequest *aRequest, nsISupports *aContext,
                              nsresult aStatusCode)
{
  JS::Rooted<JS::Value> reviver(mCx, JS::NullValue()), value(mCx);

  JS::ConstTwoByteChars chars(reinterpret_cast<const char16_t*>(mBufferedChars.Elements()),
                              mBufferedChars.Length());
  bool ok = JS_ParseJSONWithReviver(mCx, chars.begin().get(),
                                      uint32_t(mBufferedChars.Length()),
                                      reviver, &value);

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

  char buffer[JSON_STREAM_BUFSIZE];
  unsigned long bytesRemaining = aLength;
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
  if (mNeedsConverter && !mDecoder) {
    // BOM sniffing is built into the decoder.
    mDecoder = UTF_8_ENCODING->NewDecoder();
  }

  if (!aBuffer)
    return NS_OK;

  nsresult rv;
  if (mNeedsConverter) {
    rv = ConsumeConverted(aBuffer, aByteLength);
  } else {
    uint32_t unichars = aByteLength / sizeof(char16_t);
    rv = Consume((char16_t *) aBuffer, unichars);
  }

  return rv;
}

nsresult
nsJSONListener::ConsumeConverted(const char* aBuffer, uint32_t aByteLength)
{
  CheckedInt<size_t> needed = mDecoder->MaxUTF16BufferLength(aByteLength);
  if (!needed.isValid()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  CheckedInt<size_t> total(needed);
  total += mBufferedChars.Length();
  if (!total.isValid()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  char16_t* endelems = mBufferedChars.AppendElements(needed.value(), fallible);
  if (!endelems) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  auto src = AsBytes(MakeSpan(aBuffer, aByteLength));
  auto dst = MakeSpan(endelems, needed.value());
  uint32_t result;
  size_t read;
  size_t written;
  bool hadErrors;
  // Ignoring EOF like the old code
  Tie(result, read, written, hadErrors) =
    mDecoder->DecodeToUTF16(src, dst, false);
  MOZ_ASSERT(result == kInputEmpty);
  MOZ_ASSERT(read == src.Length());
  MOZ_ASSERT(written <= needed.value());
  Unused << hadErrors;
  mBufferedChars.TruncateLength(total.value() - (needed.value() - written));
  return NS_OK;
}

nsresult
nsJSONListener::Consume(const char16_t* aBuffer, uint32_t aByteLength)
{
  if (!mBufferedChars.AppendElements(aBuffer, aByteLength))
    return NS_ERROR_FAILURE;

  return NS_OK;
}

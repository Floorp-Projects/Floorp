/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=79: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Camp <dcamp@mozilla.com>
 *   Robert Sayre <sayrer@gmail.com>
 *   Nils Maier <maierman@web.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
  return nsContentUtils::ReportToConsole(nsContentUtils::eDOM_PROPERTIES,
                                         warning == EncodeWarning
                                         ? "nsIJSONEncodeDeprecatedWarning"
                                         : "nsIJSONDecodeDeprecatedWarning",
                                         nsnull, 0,
                                         nsnull,
                                         EmptyString(), 0, 0,
                                         nsIScriptError::warningFlag,
                                         "DOM Core");
}

NS_IMETHODIMP
nsJSON::Encode(nsAString &aJSON)
{
  // This function should only be called from JS.
  nsresult rv = WarnDeprecatedMethod(EncodeWarning);
  if (NS_FAILED(rv))
    return rv;

  nsJSONWriter writer;
  rv = EncodeInternal(&writer);

  // FIXME: bug 408838. Get exception types sorted out
  if (NS_SUCCEEDED(rv) || rv == NS_ERROR_INVALID_ARG) {
    rv = NS_OK;
    // if we didn't consume anything, it's not JSON, so return null
    if (!writer.DidWrite()) {
      aJSON.Truncate();
      aJSON.SetIsVoid(PR_TRUE);
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
                       const PRBool aWriteBOM)
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

  PRUint32 ignored;
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

  rv = EncodeInternal(&writer);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = bufferedStream->Flush();

  return rv;
}

static JSBool
WriteCallback(const jschar *buf, uint32 len, void *data)
{
  nsJSONWriter *writer = static_cast<nsJSONWriter*>(data);
  nsresult rv =  writer->Write((const PRUnichar*)buf, (PRUint32)len);
  if (NS_FAILED(rv))
    return JS_FALSE;

  return JS_TRUE;
}

NS_IMETHODIMP
nsJSON::EncodeFromJSVal(jsval *value, JSContext *cx, nsAString &result)
{
  result.Truncate();

  // Begin a new request
  JSAutoRequest ar(cx);

  JSAutoEnterCompartment ac;
  JSObject *obj;
  nsIScriptSecurityManager *ssm = nsnull;
  if (JSVAL_IS_OBJECT(*value) && (obj = JSVAL_TO_OBJECT(*value))) {
    if (!ac.enter(cx, obj)) {
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIPrincipal> principal;
    ssm = nsContentUtils::GetSecurityManager();
    nsresult rv = ssm->GetObjectPrincipal(cx, obj, getter_AddRefs(principal));
    NS_ENSURE_SUCCESS(rv, rv);

    JSStackFrame *fp = nsnull;
    rv = ssm->PushContextPrincipal(cx, JS_FrameIterator(cx, &fp), principal);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsJSONWriter writer;
  JSBool ok = JS_Stringify(cx, value, NULL, JSVAL_NULL,
                           WriteCallback, &writer);

  if (ssm) {
    ssm->PopContextPrincipal(cx);
  }

  if (!ok) {
    return NS_ERROR_XPC_BAD_CONVERT_JS;
  }

  NS_ENSURE_TRUE(writer.DidWrite(), NS_ERROR_UNEXPECTED);
  writer.FlushBuffer();
  result.Assign(writer.mOutputString);
  return NS_OK;
}

nsresult
nsJSON::EncodeInternal(nsJSONWriter *writer)
{
  nsresult rv;
  nsIXPConnect *xpc = nsContentUtils::XPConnect();
  if (!xpc)
    return NS_ERROR_FAILURE;

  // Now fish for the JS argument. If it's a call to encode, we'll
  // want the first argument. If it's a call to encodeToStream,
  // we'll want the forth.
  const PRUint32 firstArg = writer->mStream ? 3 : 0;

  nsAXPCNativeCallContext *cc = nsnull;
  rv = xpc->GetCurrentNativeCallContext(&cc);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 argc = 0;
  rv = cc->GetArgc(&argc);
  NS_ENSURE_SUCCESS(rv, rv);

  // If the argument wasn't provided, there's nothing to serialize.
  if (argc <= firstArg)
    return NS_OK;

  JSContext *cx = nsnull;
  rv = cc->GetJSContext(&cx);
  NS_ENSURE_SUCCESS(rv, rv);

  JSAutoRequest ar(cx);

  // Get the object we're going to serialize.
  jsval *argv = nsnull;
  rv = cc->GetArgvPtr(&argv);
  NS_ENSURE_SUCCESS(rv, rv);

  // If the argument wasn't provided, there's nothing to serialize.
  if (argc <= firstArg)
    return NS_ERROR_INVALID_ARG;

  jsval *vp = &argv[firstArg];
  
  // Backward compatibility:
  // nsIJSON does not allow to serialize anything other than objects
  JSObject *obj;
  if (!JSVAL_IS_OBJECT(*vp) || !(obj = JSVAL_TO_OBJECT(*vp)))
    return NS_ERROR_INVALID_ARG;

  /* Backward compatibility:
   * Manually call toJSON if implemented by the object and check that
   * the result is still an object
   * Note: It is perfectly fine to not implement toJSON, so it is
   * perfectly fine for GetMethod to fail
   */
  jsval toJSON;
  if (JS_GetMethod(cx, obj, "toJSON", NULL, &toJSON) &&
      !JSVAL_IS_PRIMITIVE(toJSON) &&
      JS_ObjectIsCallable(cx, JSVAL_TO_OBJECT(toJSON))) {

    // If toJSON is implemented, it must not throw
    if (!JS_CallFunctionValue(cx, obj, toJSON, 0, NULL, vp)) {
      if (JS_IsExceptionPending(cx))
        // passing NS_OK will throw the pending exception
        return NS_OK;

      // No exception, but still failed
      return NS_ERROR_FAILURE;
    }

    // Backward compatibility:
    // nsIJSON does not allow to serialize anything other than objects
    if (JSVAL_IS_PRIMITIVE(*vp))
      return NS_ERROR_INVALID_ARG;
  }
  // GetMethod may have thrown
  else if (JS_IsExceptionPending(cx))
    // passing NS_OK will throw the pending exception
    return NS_OK;

  // Backward compatibility:
  // function/xml shall not pass, just "plain" objects and arrays
  JSType type = JS_TypeOfValue(cx, *vp);
  if (type == JSTYPE_FUNCTION || type == JSTYPE_XML)
    return NS_ERROR_INVALID_ARG;

  // We're good now; try to stringify
  if (!JS_Stringify(cx, vp, NULL, JSVAL_NULL, WriteCallback, writer))
    return NS_ERROR_FAILURE;

  return NS_OK;
}


nsJSONWriter::nsJSONWriter() : mStream(nsnull),
                               mBuffer(nsnull),
                               mBufferCount(0),
                               mDidWrite(PR_FALSE),
                               mEncoder(nsnull)
{
}

nsJSONWriter::nsJSONWriter(nsIOutputStream *aStream) : mStream(aStream),
                                                       mBuffer(nsnull),
                                                       mBufferCount(0),
                                                       mDidWrite(PR_FALSE),
                                                       mEncoder(nsnull)
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
                                          nsnull, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return rv;
}

nsresult
nsJSONWriter::Write(const PRUnichar *aBuffer, PRUint32 aLength)
{
  if (mStream) {
    return WriteToStream(mStream, mEncoder, aBuffer, aLength);
  }

  if (!mDidWrite) {
    mBuffer = new PRUnichar[JSON_STREAM_BUFSIZE];
    if (!mBuffer)
      return NS_ERROR_OUT_OF_MEMORY;
    mDidWrite = PR_TRUE;
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

PRBool nsJSONWriter::DidWrite()
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
                            PRUint32 aLength)
{
  nsresult rv;
  PRInt32 srcLength = aLength;
  PRUint32 bytesWritten;

  // The bytes written to the stream might differ from the PRUnichar size
  PRInt32 aDestLength;
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
  mDidWrite = PR_TRUE;

  return rv;
}

NS_IMETHODIMP
nsJSON::Decode(const nsAString& json)
{
  nsresult rv = WarnDeprecatedMethod(DecodeWarning);
  if (NS_FAILED(rv))
    return rv;

  const PRUnichar *data;
  PRUint32 len = NS_StringGetData(json, &data);
  nsCOMPtr<nsIInputStream> stream;
  rv = NS_NewByteInputStream(getter_AddRefs(stream),
                             reinterpret_cast<const char*>(data),
                             len * sizeof(PRUnichar),
                             NS_ASSIGNMENT_DEPEND);
  NS_ENSURE_SUCCESS(rv, rv);
  return DecodeInternal(stream, len, PR_FALSE);
}

NS_IMETHODIMP
nsJSON::DecodeFromStream(nsIInputStream *aStream, PRInt32 aContentLength)
{
  return DecodeInternal(aStream, aContentLength, PR_TRUE);
}

NS_IMETHODIMP
nsJSON::DecodeToJSVal(const nsAString &str, JSContext *cx, jsval *result)
{
  JSAutoRequest ar(cx);

  if (!JS_ParseJSON(cx, (jschar*)PromiseFlatString(str).get(),
                    (uint32)str.Length(), result)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

nsresult
nsJSON::DecodeInternal(nsIInputStream *aStream,
                       PRInt32 aContentLength,
                       PRBool aNeedsConverter,
                       DecodingMode mode /* = STRICT */)
{
  nsresult rv;
  nsIXPConnect *xpc = nsContentUtils::XPConnect();
  if (!xpc)
    return NS_ERROR_FAILURE;

  nsAXPCNativeCallContext *cc = nsnull;
  rv = xpc->GetCurrentNativeCallContext(&cc);
  NS_ENSURE_SUCCESS(rv, rv);

  jsval *retvalPtr;
  rv = cc->GetRetValPtr(&retvalPtr);
  NS_ENSURE_SUCCESS(rv, rv);

  JSContext *cx = nsnull;
  rv = cc->GetJSContext(&cx);
  NS_ENSURE_SUCCESS(rv, rv);

  JSAutoRequest ar(cx);

  // Consume the stream
  nsCOMPtr<nsIChannel> jsonChannel;
  if (!mURI) {
    NS_NewURI(getter_AddRefs(mURI), NS_LITERAL_CSTRING("about:blank"), 0, 0 );
    if (!mURI)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = NS_NewInputStreamChannel(getter_AddRefs(jsonChannel), mURI, aStream,
                                NS_LITERAL_CSTRING("application/json"));
  if (!jsonChannel || NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  nsRefPtr<nsJSONListener>
    jsonListener(new nsJSONListener(cx, retvalPtr, aNeedsConverter, mode));

  if (!jsonListener)
    return NS_ERROR_OUT_OF_MEMORY;

  //XXX this stream pattern should be consolidated in netwerk
  rv = jsonListener->OnStartRequest(jsonChannel, nsnull);
  if (NS_FAILED(rv)) {
    jsonChannel->Cancel(rv);
    return rv;
  }

  nsresult status;
  jsonChannel->GetStatus(&status);
  PRUint32 offset = 0;
  while (NS_SUCCEEDED(status)) {
    PRUint32 available;
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

    rv = jsonListener->OnDataAvailable(jsonChannel, nsnull,
                                       aStream, offset, available);
    if (NS_FAILED(rv)) {
      jsonChannel->Cancel(rv);
      break;
    }

    offset += available;
    jsonChannel->GetStatus(&status);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  rv = jsonListener->OnStopRequest(jsonChannel, nsnull, status);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = cc->SetReturnValueWasSet(PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


NS_IMETHODIMP
nsJSON::LegacyDecode(const nsAString& json)
{
  const PRUnichar *data;
  PRUint32 len = NS_StringGetData(json, &data);
  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = NS_NewByteInputStream(getter_AddRefs(stream),
                                      (const char*) data,
                                      len * sizeof(PRUnichar),
                                      NS_ASSIGNMENT_DEPEND);
  NS_ENSURE_SUCCESS(rv, rv);
  return DecodeInternal(stream, len, PR_FALSE, LEGACY);
}

NS_IMETHODIMP
nsJSON::LegacyDecodeFromStream(nsIInputStream *aStream, PRInt32 aContentLength)
{
  return DecodeInternal(aStream, aContentLength, PR_TRUE, LEGACY);
}

NS_IMETHODIMP
nsJSON::LegacyDecodeToJSVal(const nsAString &str, JSContext *cx, jsval *result)
{
  JSAutoRequest ar(cx);

  if (!js::ParseJSONWithReviver(cx, (jschar*)PromiseFlatString(str).get(),
                                (uint32)str.Length(), js::NullValue(),
                                js::Valueify(result), LEGACY)) {
    return NS_ERROR_UNEXPECTED;
  }

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

nsJSONListener::nsJSONListener(JSContext *cx, jsval *rootVal,
                               PRBool needsConverter,
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
  mDecoder = nsnull;

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
    rv = ProcessBytes(nsnull, 0);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  const jschar* chars = reinterpret_cast<const jschar*>(mBufferedChars.Elements());
  JSBool ok = js::ParseJSONWithReviver(mCx, chars,
                                       (uint32) mBufferedChars.Length(),
                                       js::NullValue(), js::Valueify(mRootVal),
                                       mDecodingMode);
  mBufferedChars.TruncateLength(0);
  return ok ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsJSONListener::OnDataAvailable(nsIRequest *aRequest, nsISupports *aContext,
                                nsIInputStream *aStream,
                                PRUint32 aOffset, PRUint32 aLength)
{
  PRUint32 contentLength;
  aStream->Available(&contentLength);
  nsresult rv = NS_OK;

  if (mNeedsConverter && mSniffBuffer.Length() < 4) {
    PRUint32 readCount = (aLength < 4) ? aLength : 4;
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
                       NS_MIN((unsigned long)sizeof(buffer), bytesRemaining),
                       &bytesRead);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = ProcessBytes(buffer, bytesRead);
    NS_ENSURE_SUCCESS(rv, rv);
    bytesRemaining -= bytesRead;
  }

  return rv;
}

nsresult
nsJSONListener::ProcessBytes(const char* aBuffer, PRUint32 aByteLength)
{
  nsresult rv;
  // Check for BOM, or sniff charset
  nsCAutoString charset;
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
    PRUint32 unichars = aByteLength / sizeof(PRUnichar);
    rv = Consume((PRUnichar *) aBuffer, unichars);
  }

  return rv;
}

nsresult
nsJSONListener::ConsumeConverted(const char* aBuffer, PRUint32 aByteLength)
{
  nsresult rv;
  PRInt32 unicharLength = 0;
  PRInt32 srcLen = aByteLength;

  rv = mDecoder->GetMaxLength(aBuffer, srcLen, &unicharLength);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUnichar* endelems = mBufferedChars.AppendElements(unicharLength);
  PRInt32 preLength = unicharLength;
  rv = mDecoder->Convert(aBuffer, &srcLen, endelems, &unicharLength);
  if (NS_FAILED(rv))
    return rv;
  NS_ABORT_IF_FALSE(preLength >= unicharLength, "GetMaxLength lied");
  if (preLength > unicharLength)
    mBufferedChars.TruncateLength(mBufferedChars.Length() - (preLength - unicharLength));
  return NS_OK;
}

nsresult
nsJSONListener::Consume(const PRUnichar* aBuffer, PRUint32 aByteLength)
{
  if (!mBufferedChars.AppendElements(aBuffer, aByteLength))
    return NS_ERROR_FAILURE;

  return NS_OK;
}

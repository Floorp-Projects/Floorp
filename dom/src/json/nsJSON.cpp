/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "jsdtoa.h"
#include "jsnum.h"
#include "jsbool.h"
#include "jsarena.h"
#include "jscntxt.h"
#include "jsinterp.h"
#include "jsiter.h"
#include "jstypes.h"
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
#include "nsCRTGlue.h"
#include "nsAutoPtr.h"

static const char kXPConnectServiceCID[] = "@mozilla.org/js/xpc/XPConnect;1";

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

//
// AString encode(in JSObject value, [optional] in JSObject whitelist);
//
NS_IMETHODIMP
nsJSON::Encode(nsAString &aJSON)
{
  // This function should only be called from JS.
  nsresult rv;

  nsAutoPtr<nsJSONWriter> writer(new nsJSONWriter());
  if (!writer)
    return NS_ERROR_OUT_OF_MEMORY;
  
  rv = EncodeInternal(writer);

  // FIXME: bug 408838. Get exception types sorted out
  if (NS_SUCCEEDED(rv) || rv == NS_ERROR_INVALID_ARG) {
    rv = NS_OK;
    // if we didn't consume anything, it's not JSON, so return null
    if (writer->mBuffer.IsEmpty()) {
      aJSON.Truncate();
      aJSON.SetIsVoid(PR_TRUE);
    } else {
      aJSON.Append(writer->mBuffer);
    }
  }

  return rv;
}

static const char UTF8BOM[] = "\xEF\xBB\xBF";
static const char UTF16LEBOM[] = "\xFF\xFE";
static const char UTF16BEBOM[] = "\xFE\xFF";
static const char UTF32LEBOM[] = "\xFF\xFE\0\0";
static const char UTF32BEBOM[] = "\0\0\xFE\xFF";

static nsresult CheckCharset(const char* aCharset)
{
  // Check that the charset is permissible
  if (!(strcmp(aCharset, "UTF-8") == 0 ||
        strcmp(aCharset, "UTF-16LE") == 0 ||
        strcmp(aCharset, "UTF-16BE") == 0 ||
        strcmp(aCharset, "UTF-32LE") == 0 ||
        strcmp(aCharset, "UTF-32BE") == 0)) {
    return NS_ERROR_INVALID_ARG;
  }

  return NS_OK;
}

//
// void EncodeToStream(in nsIOutputStream stream
//                     /* in JSObject value,
//                     /* [optional] in JSObject whitelist */);
//
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
    else if (strcmp(aCharset, "UTF-32LE") == 0)
      rv = aStream->Write(UTF32LEBOM, 4, &ignored);
    else if (strcmp(aCharset, "UTF-32BE") == 0)
      rv = aStream->Write(UTF32BEBOM, 4, &ignored);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsAutoPtr<nsJSONWriter> writer(new nsJSONWriter(bufferedStream));
  if (!writer)
    return NS_ERROR_OUT_OF_MEMORY;
  rv = writer->SetCharset(aCharset);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = EncodeInternal(writer);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = bufferedStream->Flush();

  return rv;
}

nsresult
nsJSON::EncodeInternal(nsJSONWriter *writer)
{
  nsresult rv;
  nsIXPConnect *xpc = nsContentUtils::XPConnect();
  if (!xpc)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIXPCNativeCallContext> cc;
  rv = xpc->GetCurrentNativeCallContext(getter_AddRefs(cc));
  NS_ENSURE_SUCCESS(rv, rv);

  JSContext *cx = nsnull;
  rv = cc->GetJSContext(&cx);
  NS_ENSURE_SUCCESS(rv, rv);

  JSAutoRequest ar(cx);

  PRUint32 argc = 0;
  rv = cc->GetArgc(&argc);
  NS_ENSURE_SUCCESS(rv, rv);

  // Now fish for the JS arguments. If it's a call to encode, we'll
  // want the first two arguments. If it's a call to encodeToStream,
  // we'll want the fourth and fifth;
  PRUint32 firstArg = writer->mStream ? 3 : 0;

  // Get the object we're going to serialize.
  JSObject *inputObj = nsnull;
  jsval *argv = nsnull;
  rv = cc->GetArgvPtr(&argv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (argc <= firstArg ||
      !(JSVAL_IS_OBJECT(argv[firstArg]) &&
        (inputObj = JSVAL_TO_OBJECT(argv[firstArg])))) {
    // return if it's not something we can deal with
    return NS_ERROR_INVALID_ARG;
  }

  JSObject *whitelist = nsnull;

  // If there's a second argument here, it should be an array.
  if (argc >= firstArg + 2 &&
      !(JSVAL_IS_OBJECT(argv[firstArg + 1]) &&
        (whitelist = JSVAL_TO_OBJECT(argv[firstArg + 1])) &&
        JS_IsArrayObject(cx, whitelist))) {
     whitelist = nsnull; // bogus whitelists are ignored
  }

  jsval *vp = &argv[firstArg];

  JSBool ok = ToJSON(cx, vp);
  JSType type;
  if (!(ok && !JSVAL_IS_PRIMITIVE(*vp) &&
        (type = JS_TypeOfValue(cx, *vp)) != JSTYPE_FUNCTION &&
        type != JSTYPE_XML)) {
    return NS_ERROR_INVALID_ARG;
  }

  return EncodeObject(cx, vp, writer, whitelist, 0);
}

// N.B: vp must be rooted.
nsresult
nsJSON::EncodeObject(JSContext *cx, jsval *vp, nsJSONWriter *writer,
                     JSObject *whitelist, PRUint32 depth)
{
  NS_ENSURE_ARG(vp);
  NS_ENSURE_ARG(writer);

  if (depth > JSON_MAX_DEPTH) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  JSObject *obj = JSVAL_TO_OBJECT(*vp);
  JSBool isArray = JS_IsArrayObject(cx, obj);
  PRUnichar output = PRUnichar(isArray ? '[' : '{');
  rv = writer->Write(&output, 1);
  NS_ENSURE_SUCCESS(rv, rv);

  JSBool ok = JS_TRUE;

  ok = js_ValueToIterator(cx, JSITER_ENUMERATE, vp);
  if (!ok)
    return NS_ERROR_FAILURE;

  JSObject *iterObj = JSVAL_TO_OBJECT(*vp);

  jsval outputValue = JSVAL_VOID;
  JSAutoTempValueRooter tvr(cx, 1, &outputValue);

  jsval key;
  PRBool memberWritten = PR_FALSE;
  do {
    outputValue = JSVAL_VOID;
    ok = js_CallIteratorNext(cx, iterObj, &key);

    if (!ok)
      break;

    if (key == JSVAL_HOLE)
      break;

    JSString *ks;
    if (JSVAL_IS_STRING(key)) {
      ks = JSVAL_TO_STRING(key);
    } else {
      ks = JS_ValueToString(cx, key);
      if (!ks) {
        ok = JS_FALSE;
        break;
      }
    }

    ok = JS_GetUCProperty(cx, obj, JS_GetStringChars(ks),
                          JS_GetStringLength(ks), &outputValue);
    if (!ok)
      break;

    // if this is an array, holes are transmitted as null
    if (isArray && outputValue == JSVAL_VOID) {
      outputValue = JSVAL_NULL;
    } else if (JSVAL_IS_OBJECT(outputValue)) {
      ok = ToJSON(cx, &outputValue);
      if (!ok)
        break;
    }
    
    // elide undefined values
    if (outputValue == JSVAL_VOID)
      continue;

    // output a comma unless this is the first member to write
    if (memberWritten) {
      output = PRUnichar(',');
      rv = writer->Write(&output, 1);
    }
    memberWritten = PR_TRUE;
    
    JSType type = JS_TypeOfValue(cx, outputValue);

    // Can't encode these types, so drop them
    if (type == JSTYPE_FUNCTION || type == JSTYPE_XML)
      break;
 
    // Be careful below, this string is weakly rooted.
    JSString *s;
 
    // If this isn't an array, we need to output a key
    if (!isArray) {
      nsAutoString keyOutput;
      s = JS_ValueToString(cx, key);
      if (!s) {
        ok = JS_FALSE;
        break;
      }

      rv = writer->WriteString((PRUnichar*)JS_GetStringChars(s),
                               JS_GetStringLength(s));
      if (NS_FAILED(rv))
        break;
      output = PRUnichar(':');
      rv = writer->Write(&output, 1);
      if (NS_FAILED(rv))
        break;
    }

    if (!JSVAL_IS_PRIMITIVE(outputValue)) {
      // recurse
      rv = EncodeObject(cx, &outputValue, writer, whitelist, depth + 1);
      if (NS_FAILED(rv))
        break;
    } else {
      nsAutoString valueOutput;
      s = JS_ValueToString(cx, outputValue);
      if (!s) {
        ok = JS_FALSE;
        break;
      }

      if (type == JSTYPE_STRING) {
        rv = writer->WriteString((PRUnichar*)JS_GetStringChars(s),
                                 JS_GetStringLength(s));
        continue;
      }

      if (type == JSTYPE_NUMBER) {
        if (JSVAL_IS_DOUBLE(outputValue)) {
          jsdouble d = *JSVAL_TO_DOUBLE(outputValue);
          if (!JSDOUBLE_IS_FINITE(d))
            valueOutput.Append(NS_LITERAL_STRING("null"));
          else
            valueOutput.Append((PRUnichar*)JS_GetStringChars(s));
        } else {
          valueOutput.Append((PRUnichar*)JS_GetStringChars(s));
        }
      } else if (type == JSTYPE_BOOLEAN) {
        valueOutput.Append((PRUnichar*)JS_GetStringChars(s));
      } else if (JSVAL_IS_NULL(outputValue)) {
        valueOutput.Append(NS_LITERAL_STRING("null"));
      } else {
        rv = NS_ERROR_FAILURE; // encoding error
        break;
      }

      rv = writer->Write(valueOutput.get(), valueOutput.Length());
    }

  } while (NS_SUCCEEDED(rv));

  // Always close the iterator, but make sure not to stomp on OK
  ok &= js_CloseIterator(cx, *vp);

  if (!ok)
    rv = NS_ERROR_FAILURE; // encoding error or propagate? FIXME: Bug 408838.
  NS_ENSURE_SUCCESS(rv, rv);

  output = PRUnichar(isArray ? ']' : '}');
  rv = writer->Write(&output, 1);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

JSBool
nsJSON::ToJSON(JSContext *cx, jsval *vp)
{
  // Now we check to see whether the return value implements toJSON()
  JSBool ok = JS_TRUE;
  char *toJSON = "toJSON";

  if (!JSVAL_IS_PRIMITIVE(*vp)) {
    JSObject *obj = JSVAL_TO_OBJECT(*vp);
    jsval toJSONVal = nsnull;
    ok = JS_GetProperty(cx, obj, toJSON, &toJSONVal);
    if (ok && JS_TypeOfValue(cx, toJSONVal) == JSTYPE_FUNCTION) {
      ok = JS_CallFunctionValue(cx, obj, toJSONVal, 0, nsnull, vp);
    }
  }

  return ok;
}

nsJSONWriter::nsJSONWriter() : mStream(nsnull), mEncoder(nsnull)
{
}

nsJSONWriter::nsJSONWriter(nsIOutputStream *aStream) : mStream(aStream),
                                                       mEncoder(nsnull)
{
}

nsJSONWriter::~nsJSONWriter()
{
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

static const PRUnichar quote = PRUnichar('"');
static const PRUnichar backslash = PRUnichar('\\');
static const PRUnichar unicodeEscape[] = {'\\', 'u', '0', '0', '\0'};

nsresult
nsJSONWriter::WriteString(const PRUnichar *aBuffer, PRUint32 aLength)
{
  nsresult rv;
  rv = Write(&quote, 1);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 mark = 0;
  PRUint32 i;
  for (i = 0; i < aLength; ++i) {
    if (aBuffer[i] == quote || aBuffer[i] == backslash) {
      rv = Write(&aBuffer[mark], i - mark);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = Write(&backslash, 1);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = Write(&aBuffer[i], 1);
      NS_ENSURE_SUCCESS(rv, rv);
      mark = i + 1;
    } else if (aBuffer[i] <= 31 || aBuffer[i] == 127) {
      rv = Write(&aBuffer[mark], i - mark);
      NS_ENSURE_SUCCESS(rv, rv);
      nsAutoString unicode;
      unicode.Append(unicodeEscape);
      nsAutoString charCode;
      charCode.AppendInt(aBuffer[i], 16);
      if (charCode.Length() == 1)
        unicode.Append('0');
      unicode.Append(charCode);
      rv = Write(unicode.get(), unicode.Length());
      NS_ENSURE_SUCCESS(rv, rv);
      mark = i + 1;
    }
  }

  if (mark < aLength) {
    rv = Write(&aBuffer[mark], aLength - mark);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = Write(&quote, 1);
  NS_ENSURE_SUCCESS(rv, rv);
  return rv;
}

nsresult
nsJSONWriter::Write(const PRUnichar *aBuffer, PRUint32 aLength)
{
  nsresult rv = NS_OK;
  if (mStream) {
    rv = WriteToStream(mStream, mEncoder, aBuffer, aLength);
  } else {
    mBuffer.Append(aBuffer, aLength);
  }

  return rv;
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

  return rv;
}

NS_IMETHODIMP
nsJSON::Decode(const nsAString& json)
{
  const PRUnichar *data;
  PRUint32 len = NS_StringGetData(json, &data);
  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = NS_NewByteInputStream(getter_AddRefs(stream),
                                      (const char*) data,
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

nsresult
nsJSON::DecodeInternal(nsIInputStream *aStream,
                       PRInt32 aContentLength,
                       PRBool aNeedsConverter)
{
  nsresult rv;
  nsIXPConnect *xpc = nsContentUtils::XPConnect();
  if (!xpc)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIXPCNativeCallContext> cc;
  rv = xpc->GetCurrentNativeCallContext(getter_AddRefs(cc));
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
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), NS_LITERAL_CSTRING("about:blank"), 0, 0 );
  if (!uri)
    return NS_ERROR_OUT_OF_MEMORY;
  rv = NS_NewInputStreamChannel(getter_AddRefs(jsonChannel), uri, aStream,
                                NS_LITERAL_CSTRING("application/json"));
  if (!jsonChannel || NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  nsRefPtr<nsJSONListener>
    jsonListener(new nsJSONListener(cx, retvalPtr, aNeedsConverter));

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
NS_NewJSON(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  nsJSON* json = new nsJSON();
  if (!json)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(json);
  *aResult = json;

  return NS_OK;
}


JS_STATIC_DLL_CALLBACK(void)
trace_json_stack(JSTracer *trc, JSTempValueRooter *tvr)
{
  nsJSONObjectStack *tmp = static_cast<nsJSONObjectStack *>(tvr);

  for (PRUint32 i = 0; i < tmp->Length(); ++i) {
    JS_CALL_OBJECT_TRACER(trc, tmp->ElementAt(i),
                          "JSON decoder stack member");
  }
}

nsJSONListener::nsJSONListener(JSContext *cx, jsval *rootVal,
                               PRBool needsConverter)
  : mLineNum(0),
    mColumn(0),
    mHexChar(0),
    mNumHex(0),
    mCx(cx),
    mRootVal(rootVal),
    mNeedsConverter(needsConverter),
    mStatep(mStateStack)
{
  NS_ASSERTION(mCx, "Must have a JSContext");
  *mStatep = JSON_PARSE_STATE_INIT;
  JS_PUSH_TEMP_ROOT_TRACE(cx, trace_json_stack, &mObjectStack);
}

nsJSONListener::~nsJSONListener()
{
  JS_POP_TEMP_ROOT(mCx, &mObjectStack);
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
  mHexChar = 0;
  mNumHex = 0;
  mSniffBuffer.Truncate();
  mDecoder = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsJSONListener::OnStopRequest(nsIRequest *aRequest, nsISupports *aContext,
                              nsresult aStatusCode)
{
  nsresult rv;

  // This can happen with short UTF-8 messages
  if (!mSniffBuffer.IsEmpty()) {
    rv = ProcessBytes(mSniffBuffer.get(), mSniffBuffer.Length());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!mObjectStack.IsEmpty() || *mStatep != JSON_PARSE_STATE_FINISHED)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

NS_IMETHODIMP
nsJSONListener::OnDataAvailable(nsIRequest *aRequest, nsISupports *aContext,
                                nsIInputStream *aStream,
                                PRUint32 aOffset, PRUint32 aLength)
{
  PRUint32 contentLength;
  aStream->Available(&contentLength);
  nsresult rv;

  if (mNeedsConverter && mSniffBuffer.Length() < 4) {
    PRUint32 readCount = (aLength < 4) ? aLength : 4;
    rv = NS_ConsumeStream(aStream, readCount, mSniffBuffer);
    NS_ENSURE_SUCCESS(rv, rv);

    if (mSniffBuffer.Length() < 4)
      return NS_OK;
  }
  
  char buffer[JSON_PARSER_BUFSIZE];
  unsigned long bytesRemaining = aLength - mSniffBuffer.Length();
  while (bytesRemaining) {
    unsigned int bytesRead;
    rv = aStream->Read(buffer,
                       PR_MIN(sizeof(buffer), bytesRemaining),
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
        if (buffer[0] == 0x00 && buffer[1] == 0x00 &&
            buffer[2] == 0x00 && buffer[3] != 0x00) {
          charset = "UTF-32BE";
        } else if (buffer[0] == 0x00 && buffer[1] != 0x00 &&
                   buffer[2] == 0x00 && buffer[3] != 0x00) {
          charset = "UTF-16BE";
        } else if (buffer[0] != 0x00 && buffer[1] == 0x00 &&
                   buffer[2] == 0x00 && buffer[3] == 0x00) {
          charset = "UTF-32LE";
        } else if (buffer[0] != 0x00 && buffer[1] == 0x00 &&
                   buffer[2] != 0x00 && buffer[3] == 0x00) {
          charset = "UTF-16LE";
        } else if (buffer[0] != 0x00 && buffer[1] != 0x00 &&
                   buffer[2] != 0x00 && buffer[3] != 0x00) {
          charset = "UTF-8";
        }
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
  nsAutoArrayPtr<PRUnichar> ustr(new PRUnichar[unicharLength]);
  NS_ENSURE_TRUE(ustr, NS_ERROR_OUT_OF_MEMORY);
  rv = mDecoder->Convert(aBuffer, &srcLen, ustr, &unicharLength);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = Consume(ustr.get(), unicharLength);

  return rv;
}

nsresult
nsJSONListener::PopState()
{
  mStatep--;
  if (mStatep < mStateStack) {
    mStatep = mStateStack;
    return NS_ERROR_FAILURE;
  } else if (*mStatep == JSON_PARSE_STATE_INIT) {
    *mStatep = JSON_PARSE_STATE_FINISHED;
  }

  return NS_OK;
}

nsresult
nsJSONListener::PushState(JSONParserState state)
{
  if (*mStatep == JSON_PARSE_STATE_FINISHED)
    return NS_ERROR_FAILURE; // extra input

  mStatep++;
  if ((uint32)(mStatep - mStateStack) >= JS_ARRAY_LENGTH(mStateStack))
    return NS_ERROR_FAILURE; // too deep

  *mStatep = state;

  return NS_OK;
}

nsresult
nsJSONListener::Consume(const PRUnichar *data, PRUint32 len)
{
  nsresult rv;
  nsString numchars = NS_LITERAL_STRING("-+0123456789eE.");
  PRUint32 i;

  if (*mStatep == JSON_PARSE_STATE_INIT) {
    PushState(JSON_PARSE_STATE_VALUE);
  }

  for (i = 0; i < len; i++) {
    PRUnichar c = data[i];
    if (c == '\n') {
      mLineNum++;
      mColumn = 0;
    } else {
      mColumn++;
    }

    switch (*mStatep) {
      case JSON_PARSE_STATE_VALUE :
        if (c == '{') {
          *mStatep = JSON_PARSE_STATE_OBJECT;
          rv = this->OpenObject();
          NS_ENSURE_SUCCESS(rv, rv);
          rv = PushState(JSON_PARSE_STATE_OBJECT_PAIR);
          NS_ENSURE_SUCCESS(rv, rv);
        } else if (c == '[') {
          *mStatep = JSON_PARSE_STATE_ARRAY;
          rv = this->OpenArray();
          NS_ENSURE_SUCCESS(rv, rv);
          rv = PushState(JSON_PARSE_STATE_VALUE);
          NS_ENSURE_SUCCESS(rv, rv);
        } else if (c == ']') {
          // empty array
          rv = PopState();
          NS_ENSURE_SUCCESS(rv, rv);
          if (*mStatep != JSON_PARSE_STATE_ARRAY) {
            return NS_ERROR_FAILURE; // unexpected char
          }
          rv = this->CloseArray();
          NS_ENSURE_SUCCESS(rv, rv);
          rv = PopState();
          NS_ENSURE_SUCCESS(rv, rv);
        } else if (c == '}') {
          // we should only find these in OBJECT_KEY state
          return NS_ERROR_FAILURE; // unexpected failure
        } else if (c == '"') {
          *mStatep = JSON_PARSE_STATE_STRING;
        } else if (numchars.FindChar(c) >= 0) {
          *mStatep = JSON_PARSE_STATE_NUMBER;
          mStringBuffer.Append(c);
        } else if (NS_IsAsciiAlpha(c)) {
          *mStatep = JSON_PARSE_STATE_KEYWORD;
          mStringBuffer.Append(c);
        } else if (!NS_IsAsciiWhitespace(c)) {
          return NS_ERROR_FAILURE; // unexpected
        }
        break;
      case JSON_PARSE_STATE_OBJECT :
        if (c == '}') {
          rv = this->CloseObject();
          NS_ENSURE_SUCCESS(rv, rv);
          rv = PopState();
          NS_ENSURE_SUCCESS(rv, rv);
        } else if (c == ',') {
          rv = PushState(JSON_PARSE_STATE_OBJECT_PAIR);
          NS_ENSURE_SUCCESS(rv, rv);
        } else if (c == ']') {
          return NS_ERROR_FAILURE; // unexpected
        } else if (!NS_IsAsciiWhitespace(c)) {
          return NS_ERROR_FAILURE; // unexpected
        }
        break;
      case JSON_PARSE_STATE_ARRAY :
        if (c == ']') {
          rv = this->CloseArray();
          NS_ENSURE_SUCCESS(rv, rv);
          rv = PopState();
          NS_ENSURE_SUCCESS(rv, rv);
        } else if (c == ',') {
          rv = PushState(JSON_PARSE_STATE_VALUE);
          NS_ENSURE_SUCCESS(rv, rv);
        } else if (!NS_IsAsciiWhitespace(c)) {
          return NS_ERROR_FAILURE; // unexpected
        }
        break;
      case JSON_PARSE_STATE_OBJECT_PAIR :
        if (c == '"') {
          // we want to be waiting for a : when the string has been read
          *mStatep = JSON_PARSE_STATE_OBJECT_IN_PAIR;
          PushState(JSON_PARSE_STATE_STRING);
        } else if (c == '}') {
          rv = this->CloseObject();
          NS_ENSURE_SUCCESS(rv, rv);
          // pop off the object_pair state
          rv = PopState();
          NS_ENSURE_SUCCESS(rv, rv);
          // pop off the object state
          rv = PopState();
          NS_ENSURE_SUCCESS(rv, rv);
        } else if (c == ']' || !NS_IsAsciiWhitespace(c)) {
          return NS_ERROR_FAILURE; // unexpected
        }
        break;
      case JSON_PARSE_STATE_OBJECT_IN_PAIR:
        if (c == ':') {
          *mStatep = JSON_PARSE_STATE_VALUE;
        } else if (!NS_IsAsciiWhitespace(c)) {
          return NS_ERROR_FAILURE; // unexpected
        }
        break;
      case JSON_PARSE_STATE_STRING:
        if (c == '"') {
          rv = PopState();
          NS_ENSURE_SUCCESS(rv, rv);
          rv = HandleString();
          NS_ENSURE_SUCCESS(rv, rv);
        } else if (c == '\\') {
          *mStatep = JSON_PARSE_STATE_STRING_ESCAPE;
        } else {
          mStringBuffer.Append(c);
        }
        break;
      case JSON_PARSE_STATE_STRING_ESCAPE:
        switch(c) {
          case '"':
          case '\\':
          case '/':
            break;
          case 'b' : c = '\b'; break;
          case 'f' : c = '\f'; break;
          case 'n' : c = '\n'; break;
          case 'r' : c = '\r'; break;
          case 't' : c = '\t'; break;
          default :
            if (c == 'u') {
              mNumHex = 0;
              mHexChar = 0;
              *mStatep = JSON_PARSE_STATE_STRING_HEX;
              continue;
            } else {
              return NS_ERROR_FAILURE; // unexpected
            }
        }

        mStringBuffer.Append(c);
        *mStatep = JSON_PARSE_STATE_STRING;
        break;
      case JSON_PARSE_STATE_STRING_HEX:
        if (('0' <= c) && (c <= '9')) {
          mHexChar = (mHexChar << 4) | (c - '0');
        } else if (('a' <= c) && (c <= 'f')) {
          mHexChar = (mHexChar << 4) | (c - 'a' + 0x0a);
        } else if (('A' <= c) && (c <= 'F')) {
          mHexChar = (mHexChar << 4) | (c - 'A' + 0x0a);
        } else {
          return NS_ERROR_FAILURE; // unexpected
        }

        if (++(mNumHex) == 4) {
          mStringBuffer.Append(mHexChar);
          mHexChar = 0;
          mNumHex = 0;
          *mStatep = JSON_PARSE_STATE_STRING;
        }
        break;
      case JSON_PARSE_STATE_KEYWORD:
        if (NS_IsAsciiAlpha(c)) {
          mStringBuffer.Append(c);
        } else {
          // this character isn't part of the keyword, process it again
          i--;
          rv = PopState();
          NS_ENSURE_SUCCESS(rv, rv);
          rv = HandleKeyword();
          NS_ENSURE_SUCCESS(rv, rv);
        }
        break;
      case JSON_PARSE_STATE_NUMBER:
        if (numchars.FindChar(c) >= 0) {
          mStringBuffer.Append(c);
        } else {
          // this character isn't part of the number, process it again
          i--;
          rv = PopState();
          NS_ENSURE_SUCCESS(rv, rv);
          rv = HandleNumber();
          NS_ENSURE_SUCCESS(rv, rv);
        }
        break;
      case JSON_PARSE_STATE_FINISHED:
        if (!NS_IsAsciiWhitespace(c)) {
          return NS_ERROR_FAILURE; // extra input
        }
        break;
      default:
        NS_NOTREACHED("Invalid JSON parser state");
      }
    }

    return NS_OK;
}

nsresult
nsJSONListener::PushValue(JSObject *aParent, jsval aValue)
{
  JSAutoTempValueRooter tvr(mCx, 1, &aValue);
  
  JSBool ok;
  if (JS_IsArrayObject(mCx, aParent)) {
    jsuint len;
    ok = JS_GetArrayLength(mCx, aParent, &len);
    if (ok) {
      ok = JS_SetElement(mCx, aParent, len, &aValue);
    }
  } else {
    ok = JS_DefineUCProperty(mCx, aParent, (jschar *) mObjectKey.get(),
                             mObjectKey.Length(), aValue,
                             NULL, NULL, JSPROP_ENUMERATE);
  }

  return ok ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
nsJSONListener::PushObject(JSObject *aObj)
{
  if (mObjectStack.Length() >= JSON_MAX_DEPTH)
    return NS_ERROR_FAILURE; // decoding error

  // Check if this is the root object
  if (mObjectStack.IsEmpty()) {
    *mRootVal = OBJECT_TO_JSVAL(aObj);
    if (!mObjectStack.AppendElement(aObj))
      return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
  }

  nsresult rv;
  JSObject *parent = mObjectStack.ElementAt(mObjectStack.Length() - 1);
  rv = PushValue(parent, OBJECT_TO_JSVAL(aObj));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mObjectStack.AppendElement(aObj))
    return NS_ERROR_OUT_OF_MEMORY;

  return rv;
}

nsresult
nsJSONListener::OpenObject()
{
  if (*mStatep != JSON_PARSE_STATE_VALUE &&
      *mStatep != JSON_PARSE_STATE_OBJECT) {
    return NS_ERROR_FAILURE;
  }

  JSObject *obj = JS_NewObject(mCx, NULL, NULL, NULL);
  if (!obj)
    return NS_ERROR_OUT_OF_MEMORY;

  return PushObject(obj);
}

nsresult
nsJSONListener::OpenArray()
{
  if (*mStatep != JSON_PARSE_STATE_VALUE &&
      *mStatep != JSON_PARSE_STATE_ARRAY) {
    return NS_ERROR_FAILURE;
  }

  // Add an array to an existing array or object
  JSObject *arr = JS_NewArrayObject(mCx, 0, NULL);
  if (!arr)
    return NS_ERROR_OUT_OF_MEMORY;

  return PushObject(arr);
}

nsresult
nsJSONListener::CloseObject()
{
  if (!mObjectStack.SetLength(mObjectStack.Length() - 1))
    return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult
nsJSONListener::CloseArray()
{
  return this->CloseObject();
}

nsresult
nsJSONListener::HandleString()
{
  nsresult rv = NS_OK;
  if (*mStatep == JSON_PARSE_STATE_OBJECT_IN_PAIR) {
    mObjectKey = mStringBuffer;
  } else {
    JSObject *obj = mObjectStack.ElementAt(mObjectStack.Length() - 1);
    JSString *str = JS_NewUCStringCopyN(mCx, (jschar *) mStringBuffer.get(),
                                        mStringBuffer.Length());
    if (!str)
      return NS_ERROR_OUT_OF_MEMORY;
    rv = PushValue(obj, STRING_TO_JSVAL(str));
  }

  mStringBuffer.Truncate();
  return rv;
}

nsresult
nsJSONListener::HandleNumber()
{
  nsresult rv;
  JSObject *obj = mObjectStack.ElementAt(mObjectStack.Length() - 1);

  char *estr;
  int err;
  double val = JS_strtod(NS_ConvertUTF16toUTF8(mStringBuffer).get(),
                         &estr, &err);
  if (err == JS_DTOA_ENOMEM) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  } else if (err || *estr) {
    rv = NS_ERROR_FAILURE; // decode error
  } else {
    // ok
    jsval numVal;
    if (JS_NewNumberValue(mCx, val, &numVal)) {
      rv = PushValue(obj, numVal);
    } else {
      rv = NS_ERROR_FAILURE; // decode error
    }
  }

  mStringBuffer.Truncate();

  return rv;
}

nsresult
nsJSONListener::HandleKeyword()
{
  JSObject *obj = mObjectStack.ElementAt(mObjectStack.Length() - 1);
  jsval keyword;
  if (mStringBuffer.Equals(NS_LITERAL_STRING("null"))) {
    keyword = JSVAL_NULL;
  } else if (mStringBuffer.Equals(NS_LITERAL_STRING("true"))) {
    keyword = JSVAL_TRUE;
  } else if (mStringBuffer.Equals(NS_LITERAL_STRING("false"))) {
    keyword = JSVAL_FALSE;
  } else {
    return NS_ERROR_FAILURE;
  }

  mStringBuffer.Truncate();

  return PushValue(obj, keyword);
}

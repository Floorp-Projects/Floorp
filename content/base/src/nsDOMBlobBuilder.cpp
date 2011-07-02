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
 * The Original Code is Mozilla File API.
 *
 * The Initial Developer of the Original Code is
 *   Kyle Huey <me@kylehuey.com>
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "jstypedarray.h"
#include "nsAutoPtr.h"
#include "nsDOMClassInfo.h"
#include "nsDOMFile.h"
#include "nsIMultiplexInputStream.h"
#include "nsStringStream.h"
#include "nsTArray.h"
#include "nsJSUtils.h"
#include "nsContentUtils.h"
#include "CheckedInt.h"

// XXXkhuey shamelessly stolen from VideoUtils.h.  We should patch NSPR.
#define PR_INT64_MAX (~((PRInt64)(1) << 63))
#define PR_INT64_MIN (-PR_INT64_MAX - 1)

using namespace mozilla;

class nsDOMMultipartBlob : public nsDOMFile
{
public:
  nsDOMMultipartBlob(nsTArray<nsCOMPtr<nsIDOMBlob> > aBlobs,
                     const nsAString& aContentType)
    : nsDOMFile(nsnull, aContentType),
      mBlobs(aBlobs)
  {
    mIsFullFile = false;
    mStart = 0;
    mLength = 0;
  }

  NS_IMETHOD GetSize(PRUint64*);
  NS_IMETHOD GetInternalStream(nsIInputStream**);
  NS_IMETHOD MozSlice(PRInt64 aStart, PRInt64 aEnd,
                      const nsAString& aContentType, PRUint8 optional_argc,
                      nsIDOMBlob **aBlob);

protected:
  nsTArray<nsCOMPtr<nsIDOMBlob> > mBlobs;
};

NS_IMETHODIMP
nsDOMMultipartBlob::GetSize(PRUint64* aLength)
{
  nsresult rv;
  *aLength = 0;

  if (mLength) {
    *aLength = mLength;
    return NS_OK;
  }

  CheckedUint64 length = 0;

  PRUint32 i;
  PRUint32 len = mBlobs.Length();
  for (i = 0; i < len; i++) {
    nsIDOMBlob* blob = mBlobs.ElementAt(i).get();
    PRUint64 l = 0;

    rv = blob->GetSize(&l);
    NS_ENSURE_SUCCESS(rv, rv);

    length += l;
  }

  if (!length.valid())
    return NS_ERROR_FAILURE;

  mLength = length.value();
  *aLength = mLength;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMultipartBlob::GetInternalStream(nsIInputStream** aStream)
{
  nsresult rv;
  *aStream = nsnull;

  nsCOMPtr<nsIMultiplexInputStream> stream =
    do_CreateInstance("@mozilla.org/io/multiplex-input-stream;1");
  NS_ENSURE_TRUE(stream, NS_ERROR_FAILURE);

  PRUint32 i;
  for (i = 0; i < mBlobs.Length(); i++) {
    nsCOMPtr<nsIInputStream> scratchStream;
    nsIDOMBlob* blob = mBlobs.ElementAt(i).get();

    rv = blob->GetInternalStream(getter_AddRefs(scratchStream));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stream->AppendStream(scratchStream);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return CallQueryInterface(stream, aStream);
}

NS_IMETHODIMP
nsDOMMultipartBlob::MozSlice(PRInt64 aStart, PRInt64 aEnd,
                             const nsAString& aContentType,
                             PRUint8 optional_argc,
                             nsIDOMBlob **aBlob)
{
  nsresult rv;
  *aBlob = nsnull;

  // Truncate aStart and aEnd so that we stay within this file.
  PRUint64 thisLength;
  rv = GetSize(&thisLength);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!optional_argc) {
    aEnd = (PRInt64)thisLength;
  }

  // Modifies aStart and aEnd.
  ParseSize((PRInt64)thisLength, aStart, aEnd);

  // If we clamped to nothing we create an empty blob
  nsTArray<nsCOMPtr<nsIDOMBlob> > blobs;

  PRUint64 length = aEnd - aStart;
  PRUint64 skipStart = aStart;

  NS_ABORT_IF_FALSE(PRUint64(aStart) + length <= thisLength, "Er, what?");

  // Prune the list of blobs if we can
  PRUint32 i;
  for (i = 0; length && skipStart && i < mBlobs.Length(); i++) {
    nsIDOMBlob* blob = mBlobs[i].get();

    PRUint64 l;
    rv = blob->GetSize(&l);
    NS_ENSURE_SUCCESS(rv, rv);

    if (skipStart < l) {
      PRUint64 upperBound = NS_MIN<PRUint64>(l - skipStart, length);

      nsCOMPtr<nsIDOMBlob> firstBlob;
      rv = mBlobs.ElementAt(i)->MozSlice(skipStart, skipStart + upperBound,
                                         aContentType, 2,
                                         getter_AddRefs(firstBlob));
      NS_ENSURE_SUCCESS(rv, rv);

      // Avoid wrapping a single blob inside an nsDOMMultipartBlob
      if (length == upperBound) {
        firstBlob.forget(aBlob);
        return NS_OK;
      }

      blobs.AppendElement(firstBlob);
      length -= upperBound;
      i++;
      break;
    }
    skipStart -= l;
  }

  // Now append enough blobs until we're done
  for (; length && i < mBlobs.Length(); i++) {
    nsIDOMBlob* blob = mBlobs[i].get();

    PRUint64 l;
    rv = blob->GetSize(&l);
    NS_ENSURE_SUCCESS(rv, rv);

    if (length < l) {
      nsCOMPtr<nsIDOMBlob> lastBlob;
      rv = mBlobs.ElementAt(i)->MozSlice(0, length, aContentType, 2,
                                         getter_AddRefs(lastBlob));
      NS_ENSURE_SUCCESS(rv, rv);

      blobs.AppendElement(lastBlob);
    } else {
      blobs.AppendElement(blob);
    }
    length -= NS_MIN<PRUint64>(l, length);
  }

  // we can create our blob now
  nsCOMPtr<nsIDOMBlob> blob = new nsDOMMultipartBlob(blobs, aContentType);
  blob.forget(aBlob);
  return NS_OK;
}

class nsDOMBlobBuilder : public nsIDOMMozBlobBuilder
{
public:
  nsDOMBlobBuilder()
    : mData(nsnull), mDataLen(0), mDataBufferLen(0)
  {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMMOZBLOBBUILDER
protected:
  nsresult AppendVoidPtr(void* aData, PRUint32 aLength);
  nsresult AppendString(JSString* aString, JSContext* aCx);
  nsresult AppendBlob(nsIDOMBlob* aBlob);
  nsresult AppendArrayBuffer(JSObject* aBuffer);

  bool ExpandBufferSize(PRUint64 aSize)
  {
    if (mDataBufferLen >= mDataLen + aSize) {
      mDataLen += aSize;
      return true;
    }

    // Start at 1 or we'll loop forever.
    CheckedUint32 bufferLen = NS_MAX<PRUint32>(mDataBufferLen, 1);
    while (bufferLen.valid() && bufferLen.value() < mDataLen + aSize)
      bufferLen *= 2;

    if (!bufferLen.valid())
      return false;

    // PR_ memory functions are still fallible
    void* data = PR_Realloc(mData, bufferLen.value());
    if (!data)
      return false;

    mData = data;
    mDataBufferLen = bufferLen.value();
    mDataLen += aSize;
    return true;
  }

  void Flush() {
    if (mData) {
      // If we have some data, create a blob for it
      // and put it on the stack

      nsCOMPtr<nsIDOMBlob> blob =
        new nsDOMMemoryFile(mData, mDataLen, EmptyString(), EmptyString());
      mBlobs.AppendElement(blob);
      mData = nsnull; // The nsDOMMemoryFile takes ownership of the buffer
      mDataLen = 0;
      mDataBufferLen = 0;
    }
  }

  nsTArray<nsCOMPtr<nsIDOMBlob> > mBlobs;
  void* mData;
  PRUint64 mDataLen;
  PRUint64 mDataBufferLen;
};

DOMCI_DATA(MozBlobBuilder, nsDOMBlobBuilder)

NS_IMPL_ADDREF(nsDOMBlobBuilder)
NS_IMPL_RELEASE(nsDOMBlobBuilder)
NS_INTERFACE_MAP_BEGIN(nsDOMBlobBuilder)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozBlobBuilder)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozBlobBuilder)
NS_INTERFACE_MAP_END

nsresult
nsDOMBlobBuilder::AppendVoidPtr(void* aData, PRUint32 aLength)
{
  NS_ENSURE_ARG_POINTER(aData);

  PRUint64 offset = mDataLen;

  if (!ExpandBufferSize(aLength))
    return NS_ERROR_OUT_OF_MEMORY;

  memcpy((char*)mData + offset, aData, aLength);
  return NS_OK;
}

nsresult
nsDOMBlobBuilder::AppendString(JSString* aString, JSContext* aCx)
{
  nsDependentJSString xpcomStr;
  if (!xpcomStr.init(aCx, aString)) {
    return NS_ERROR_XPC_BAD_CONVERT_JS;
  }

  NS_ConvertUTF16toUTF8 utf8Str(xpcomStr);

  return AppendVoidPtr((void*)utf8Str.Data(),
                       utf8Str.Length());
}

nsresult
nsDOMBlobBuilder::AppendBlob(nsIDOMBlob* aBlob)
{
  NS_ENSURE_ARG_POINTER(aBlob);

  Flush();
  mBlobs.AppendElement(aBlob);

  return NS_OK;
}

nsresult
nsDOMBlobBuilder::AppendArrayBuffer(JSObject* aBuffer)
{
  return AppendVoidPtr(JS_GetArrayBufferData(aBuffer), JS_GetArrayBufferByteLength(aBuffer));
}

/* nsIDOMBlob getBlob ([optional] in DOMString contentType); */
NS_IMETHODIMP
nsDOMBlobBuilder::GetBlob(const nsAString& aContentType,
                          nsIDOMBlob** aBlob)
{
  NS_ENSURE_ARG(aBlob);

  Flush();

  nsCOMPtr<nsIDOMBlob> blob = new nsDOMMultipartBlob(mBlobs,
                                                     aContentType);
  blob.forget(aBlob);

  // NB: This is a willful violation of the spec.  The spec says that
  // the existing contents of the BlobBuilder should be included
  // in the next blob produced.  This seems silly and has been raised
  // on the WHATWG listserv.
  mBlobs.Clear();

  return NS_OK;
}

/* [implicit_jscontext] void append (in jsval data); */
NS_IMETHODIMP
nsDOMBlobBuilder::Append(const jsval& aData, JSContext* aCx)
{
  // We need to figure out what our jsval is

  // Is it an object?
  if (JSVAL_IS_OBJECT(aData)) {
    JSObject* obj = JSVAL_TO_OBJECT(aData);
    NS_ASSERTION(obj, "Er, what?");

    // Is it a Blob?
    nsCOMPtr<nsIDOMBlob> blob = do_QueryInterface(
      nsContentUtils::XPConnect()->
        GetNativeOfWrapper(aCx, obj));
    if (blob)
      return AppendBlob(blob);

    // Is it an array buffer?
    if (js_IsArrayBuffer(obj)) {
      JSObject* buffer = js::ArrayBuffer::getArrayBuffer(obj);
      if (buffer)
        return AppendArrayBuffer(buffer);
    }
  }

  // If it's not a Blob or an ArrayBuffer, coerce it to a string
  JSString* str = JS_ValueToString(aCx, aData);
  NS_ENSURE_TRUE(str, NS_ERROR_FAILURE);

  return AppendString(str, aCx);
}

nsresult NS_NewBlobBuilder(nsISupports* *aSupports)
{
  nsDOMBlobBuilder* builder = new nsDOMBlobBuilder();
  return CallQueryInterface(builder, aSupports);
}

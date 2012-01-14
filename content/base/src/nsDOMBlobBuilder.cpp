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

#include "nsDOMBlobBuilder.h"
#include "jsfriendapi.h"
#include "nsAutoPtr.h"
#include "nsDOMClassInfoID.h"
#include "nsIMultiplexInputStream.h"
#include "nsStringStream.h"
#include "nsTArray.h"
#include "nsJSUtils.h"
#include "nsContentUtils.h"
#include "DictionaryHelpers.h"
#include "nsIScriptError.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS_INHERITED1(nsDOMMultipartFile, nsDOMFileBase,
                             nsIJSNativeInitializer)

NS_IMETHODIMP
nsDOMMultipartFile::GetSize(PRUint64* aLength)
{
  if (mLength == UINT64_MAX) {
    CheckedUint64 length = 0;
  
    PRUint32 i;
    PRUint32 len = mBlobs.Length();
    for (i = 0; i < len; i++) {
      nsIDOMBlob* blob = mBlobs.ElementAt(i).get();
      PRUint64 l = 0;
  
      nsresult rv = blob->GetSize(&l);
      NS_ENSURE_SUCCESS(rv, rv);
  
      length += l;
    }
  
    NS_ENSURE_TRUE(length.valid(), NS_ERROR_FAILURE);

    mLength = length.value();
  }

  *aLength = mLength;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMultipartFile::GetInternalStream(nsIInputStream** aStream)
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

already_AddRefed<nsIDOMBlob>
nsDOMMultipartFile::CreateSlice(PRUint64 aStart, PRUint64 aLength,
                                const nsAString& aContentType)
{
  // If we clamped to nothing we create an empty blob
  nsTArray<nsCOMPtr<nsIDOMBlob> > blobs;

  PRUint64 length = aLength;
  PRUint64 skipStart = aStart;

  // Prune the list of blobs if we can
  PRUint32 i;
  for (i = 0; length && skipStart && i < mBlobs.Length(); i++) {
    nsIDOMBlob* blob = mBlobs[i].get();

    PRUint64 l;
    nsresult rv = blob->GetSize(&l);
    NS_ENSURE_SUCCESS(rv, nsnull);

    if (skipStart < l) {
      PRUint64 upperBound = NS_MIN<PRUint64>(l - skipStart, length);

      nsCOMPtr<nsIDOMBlob> firstBlob;
      rv = blob->Slice(skipStart, skipStart + upperBound,
                       aContentType, 3,
                       getter_AddRefs(firstBlob));
      NS_ENSURE_SUCCESS(rv, nsnull);

      // Avoid wrapping a single blob inside an nsDOMMultipartFile
      if (length == upperBound) {
        return firstBlob.forget();
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
    nsresult rv = blob->GetSize(&l);
    NS_ENSURE_SUCCESS(rv, nsnull);

    if (length < l) {
      nsCOMPtr<nsIDOMBlob> lastBlob;
      rv = blob->Slice(0, length, aContentType, 3,
                       getter_AddRefs(lastBlob));
      NS_ENSURE_SUCCESS(rv, nsnull);

      blobs.AppendElement(lastBlob);
    } else {
      blobs.AppendElement(blob);
    }
    length -= NS_MIN<PRUint64>(l, length);
  }

  // we can create our blob now
  nsCOMPtr<nsIDOMBlob> blob = new nsDOMMultipartFile(blobs, aContentType);
  return blob.forget();
}

/* static */ nsresult
nsDOMMultipartFile::NewBlob(nsISupports* *aNewObject)
{
  nsCOMPtr<nsISupports> file = do_QueryObject(new nsDOMMultipartFile());
  file.forget(aNewObject);
  return NS_OK;
}

static nsIDOMBlob*
GetXPConnectNative(JSContext* aCx, JSObject* aObj) {
  nsCOMPtr<nsIDOMBlob> blob = do_QueryInterface(
    nsContentUtils::XPConnect()->GetNativeOfWrapper(aCx, aObj));
  return blob;
}

NS_IMETHODIMP
nsDOMMultipartFile::Initialize(nsISupports* aOwner,
                               JSContext* aCx,
                               JSObject* aObj,
                               PRUint32 aArgc,
                               jsval* aArgv)
{
  return InitInternal(aCx, aArgc, aArgv, GetXPConnectNative);
}

nsresult
nsDOMMultipartFile::InitInternal(JSContext* aCx,
                                 PRUint32 aArgc,
                                 jsval* aArgv,
                                 UnwrapFuncPtr aUnwrapFunc)
{
  bool nativeEOL = false;
  if (aArgc > 1) {
    mozilla::dom::BlobPropertyBag d;
    nsresult rv = d.Init(aCx, &aArgv[1]);
    NS_ENSURE_SUCCESS(rv, rv);
    mContentType = d.type;
    if (d.endings.EqualsLiteral("native")) {
      nativeEOL = true;
    } else if (!d.endings.EqualsLiteral("transparent")) {
      return NS_ERROR_DOM_INVALID_STATE_ERR;
    }
  }

  if (aArgc > 0) {
    if (!aArgv[0].isObject()) {
      return NS_ERROR_INVALID_ARG; // We're not interested
    }

    JSObject& obj = aArgv[0].toObject();

    if (!JS_IsArrayObject(aCx, &obj)) {
      return NS_ERROR_INVALID_ARG; // We're not interested
    }

    BlobSet blobSet;

    uint32_t length;
    JS_ALWAYS_TRUE(JS_GetArrayLength(aCx, &obj, &length));
    for (uint32_t i = 0; i < length; ++i) {
      jsval element;
      if (!JS_GetElement(aCx, &obj, i, &element))
        return NS_ERROR_INVALID_ARG;

      if (element.isObject()) {
        JSObject& obj = element.toObject();
        nsCOMPtr<nsIDOMBlob> blob = aUnwrapFunc(aCx, &obj);
        if (blob) {
          // Flatten so that multipart blobs will never nest
          nsDOMFileBase* file = static_cast<nsDOMFileBase*>(
              static_cast<nsIDOMBlob*>(blob));
          const nsTArray<nsCOMPtr<nsIDOMBlob> >*
              subBlobs = file->GetSubBlobs();
          if (subBlobs) {
            blobSet.AppendBlobs(*subBlobs);
          } else {
            blobSet.AppendBlob(blob);
          }
        } else if (JS_IsArrayBufferObject(&obj, aCx)) {
          blobSet.AppendArrayBuffer(&obj, aCx);
        } else {
          // neither arraybuffer nor blob
          return NS_ERROR_DOM_INVALID_STATE_ERR;
        }
      } else if (element.isString()) {
        blobSet.AppendString(element.toString(), nativeEOL, aCx);
      } else {
        // neither object nor string
        return NS_ERROR_DOM_INVALID_STATE_ERR;
      }
    }

    mBlobs = blobSet.GetBlobs();
  }

  return NS_OK;
}

nsresult
BlobSet::AppendVoidPtr(const void* aData, PRUint32 aLength)
{
  NS_ENSURE_ARG_POINTER(aData);

  PRUint64 offset = mDataLen;

  if (!ExpandBufferSize(aLength))
    return NS_ERROR_OUT_OF_MEMORY;

  memcpy((char*)mData + offset, aData, aLength);
  return NS_OK;
}

nsresult
BlobSet::AppendString(JSString* aString, bool nativeEOL, JSContext* aCx)
{
  nsDependentJSString xpcomStr;
  if (!xpcomStr.init(aCx, aString)) {
    return NS_ERROR_XPC_BAD_CONVERT_JS;
  }

  nsCString utf8Str = NS_ConvertUTF16toUTF8(xpcomStr);

  if (nativeEOL) {
    if (utf8Str.FindChar('\r') != kNotFound) {
      utf8Str.ReplaceSubstring("\r\n", "\n");
      utf8Str.ReplaceSubstring("\r", "\n");
    }
#ifdef XP_WIN
    utf8Str.ReplaceSubstring("\n", "\r\n");
#endif
  }

  return AppendVoidPtr((void*)utf8Str.Data(),
                       utf8Str.Length());
}

nsresult
BlobSet::AppendBlob(nsIDOMBlob* aBlob)
{
  NS_ENSURE_ARG_POINTER(aBlob);

  Flush();
  mBlobs.AppendElement(aBlob);

  return NS_OK;
}

nsresult
BlobSet::AppendBlobs(const nsTArray<nsCOMPtr<nsIDOMBlob> >& aBlob)
{
  Flush();
  mBlobs.AppendElements(aBlob);

  return NS_OK;
}

nsresult
BlobSet::AppendArrayBuffer(JSObject* aBuffer, JSContext *aCx)
{
  return AppendVoidPtr(JS_GetArrayBufferData(aBuffer, aCx),
                       JS_GetArrayBufferByteLength(aBuffer, aCx));
}

DOMCI_DATA(MozBlobBuilder, nsDOMBlobBuilder)

NS_IMPL_ADDREF(nsDOMBlobBuilder)
NS_IMPL_RELEASE(nsDOMBlobBuilder)
NS_INTERFACE_MAP_BEGIN(nsDOMBlobBuilder)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozBlobBuilder)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMMozBlobBuilder)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozBlobBuilder)
NS_INTERFACE_MAP_END

/* nsIDOMBlob getBlob ([optional] in DOMString contentType); */
NS_IMETHODIMP
nsDOMBlobBuilder::GetBlob(const nsAString& aContentType,
                          nsIDOMBlob** aBlob)
{
  return GetBlobInternal(aContentType, true, aBlob);
}

nsresult
nsDOMBlobBuilder::GetBlobInternal(const nsAString& aContentType,
                                  bool aClearBuffer,
                                  nsIDOMBlob** aBlob)
{
  NS_ENSURE_ARG(aBlob);

  nsTArray<nsCOMPtr<nsIDOMBlob> >& blobs = mBlobSet.GetBlobs();

  nsCOMPtr<nsIDOMBlob> blob = new nsDOMMultipartFile(blobs,
                                                     aContentType);
  blob.forget(aBlob);

  // NB: This is a willful violation of the spec.  The spec says that
  // the existing contents of the BlobBuilder should be included
  // in the next blob produced.  This seems silly and has been raised
  // on the WHATWG listserv.
  if (aClearBuffer) {
    blobs.Clear();
  }

  return NS_OK;
}

/* nsIDOMBlob getFile (in DOMString name, [optional] in DOMString contentType); */
NS_IMETHODIMP
nsDOMBlobBuilder::GetFile(const nsAString& aName,
                          const nsAString& aContentType,
                          nsIDOMFile** aFile)
{
  NS_ENSURE_ARG(aFile);

  nsTArray<nsCOMPtr<nsIDOMBlob> >& blobs = mBlobSet.GetBlobs();

  nsCOMPtr<nsIDOMFile> file = new nsDOMMultipartFile(blobs,
                                                     aName,
                                                     aContentType);
  file.forget(aFile);

  // NB: This is a willful violation of the spec.  The spec says that
  // the existing contents of the BlobBuilder should be included
  // in the next blob produced.  This seems silly and has been raised
  // on the WHATWG listserv.
  blobs.Clear();

  return NS_OK;
}

/* [implicit_jscontext] void append (in jsval data,
                                     [optional] in DOMString endings); */
NS_IMETHODIMP
nsDOMBlobBuilder::Append(const jsval& aData,
                         const nsAString& aEndings, JSContext* aCx)
{
  // We need to figure out what our jsval is

  // Is it an object?
  if (JSVAL_IS_OBJECT(aData)) {
    JSObject* obj = JSVAL_TO_OBJECT(aData);
    if (!obj) {
      // We got passed null.  Just do nothing.
      return NS_OK;
    }

    // Is it a Blob?
    nsCOMPtr<nsIDOMBlob> blob = do_QueryInterface(
      nsContentUtils::XPConnect()->
        GetNativeOfWrapper(aCx, obj));
    if (blob) {
      // Flatten so that multipart blobs will never nest
      nsDOMFileBase* file = static_cast<nsDOMFileBase*>(
          static_cast<nsIDOMBlob*>(blob));
      const nsTArray<nsCOMPtr<nsIDOMBlob> >* subBlobs = file->GetSubBlobs();
      if (subBlobs) {
        return mBlobSet.AppendBlobs(*subBlobs);
      } else {
        return mBlobSet.AppendBlob(blob);
      }
    }

    // Is it an array buffer?
    if (JS_IsArrayBufferObject(obj, aCx)) {
      return mBlobSet.AppendArrayBuffer(obj, aCx);
    }
  }

  // If it's not a Blob or an ArrayBuffer, coerce it to a string
  JSString* str = JS_ValueToString(aCx, aData);
  NS_ENSURE_TRUE(str, NS_ERROR_FAILURE);

  return mBlobSet.AppendString(str, aEndings.EqualsLiteral("native"), aCx);
}

nsresult NS_NewBlobBuilder(nsISupports* *aSupports)
{
  nsDOMBlobBuilder* builder = new nsDOMBlobBuilder();
  return CallQueryInterface(builder, aSupports);
}

NS_IMETHODIMP
nsDOMBlobBuilder::Initialize(nsISupports* aOwner,
                             JSContext* aCx,
                             JSObject* aObj,
                             PRUint32 aArgc,
                             jsval* aArgv)
{
  nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(aOwner));
  if (!window) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocument> doc(do_QueryInterface(window->GetExtantDocument()));
  if (!doc) {
    return NS_OK;
  }

  doc->WarnOnceAbout(nsIDocument::eMozBlobBuilder);
  return NS_OK;
}

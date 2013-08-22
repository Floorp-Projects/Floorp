/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMBlobBuilder.h"
#include "jsfriendapi.h"
#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/FileBinding.h"
#include "nsAutoPtr.h"
#include "nsDOMClassInfoID.h"
#include "nsIMultiplexInputStream.h"
#include "nsStringStream.h"
#include "nsTArray.h"
#include "nsJSUtils.h"
#include "nsContentUtils.h"
#include "nsIScriptError.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ISUPPORTS_INHERITED1(nsDOMMultipartFile, nsDOMFile,
                             nsIJSNativeInitializer)

NS_IMETHODIMP
nsDOMMultipartFile::GetSize(uint64_t* aLength)
{
  if (mLength == UINT64_MAX) {
    CheckedUint64 length = 0;
  
    uint32_t i;
    uint32_t len = mBlobs.Length();
    for (i = 0; i < len; i++) {
      nsIDOMBlob* blob = mBlobs.ElementAt(i).get();
      uint64_t l = 0;
  
      nsresult rv = blob->GetSize(&l);
      NS_ENSURE_SUCCESS(rv, rv);
  
      length += l;
    }
  
    NS_ENSURE_TRUE(length.isValid(), NS_ERROR_FAILURE);

    mLength = length.value();
  }

  *aLength = mLength;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMultipartFile::GetInternalStream(nsIInputStream** aStream)
{
  nsresult rv;
  *aStream = nullptr;

  nsCOMPtr<nsIMultiplexInputStream> stream =
    do_CreateInstance("@mozilla.org/io/multiplex-input-stream;1");
  NS_ENSURE_TRUE(stream, NS_ERROR_FAILURE);

  uint32_t i;
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
nsDOMMultipartFile::CreateSlice(uint64_t aStart, uint64_t aLength,
                                const nsAString& aContentType)
{
  // If we clamped to nothing we create an empty blob
  nsTArray<nsCOMPtr<nsIDOMBlob> > blobs;

  uint64_t length = aLength;
  uint64_t skipStart = aStart;

  // Prune the list of blobs if we can
  uint32_t i;
  for (i = 0; length && skipStart && i < mBlobs.Length(); i++) {
    nsIDOMBlob* blob = mBlobs[i].get();

    uint64_t l;
    nsresult rv = blob->GetSize(&l);
    NS_ENSURE_SUCCESS(rv, nullptr);

    if (skipStart < l) {
      uint64_t upperBound = std::min<uint64_t>(l - skipStart, length);

      nsCOMPtr<nsIDOMBlob> firstBlob;
      rv = blob->Slice(skipStart, skipStart + upperBound,
                       aContentType, 3,
                       getter_AddRefs(firstBlob));
      NS_ENSURE_SUCCESS(rv, nullptr);

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

    uint64_t l;
    nsresult rv = blob->GetSize(&l);
    NS_ENSURE_SUCCESS(rv, nullptr);

    if (length < l) {
      nsCOMPtr<nsIDOMBlob> lastBlob;
      rv = blob->Slice(0, length, aContentType, 3,
                       getter_AddRefs(lastBlob));
      NS_ENSURE_SUCCESS(rv, nullptr);

      blobs.AppendElement(lastBlob);
    } else {
      blobs.AppendElement(blob);
    }
    length -= std::min<uint64_t>(l, length);
  }

  // we can create our blob now
  nsCOMPtr<nsIDOMBlob> blob = new nsDOMMultipartFile(blobs, aContentType);
  return blob.forget();
}

/* static */ nsresult
nsDOMMultipartFile::NewFile(const nsAString& aName, nsISupports* *aNewObject)
{
  nsCOMPtr<nsISupports> file =
    do_QueryObject(new nsDOMMultipartFile(aName));
  file.forget(aNewObject);
  return NS_OK;
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
                               const JS::CallArgs& aArgs)
{
  if (!mIsFile) {
    return InitBlob(aCx, aArgs.length(), aArgs.array(), GetXPConnectNative);
  }
  return InitFile(aCx, aArgs.length(), aArgs.array());
}

nsresult
nsDOMMultipartFile::InitBlob(JSContext* aCx,
                             uint32_t aArgc,
                             JS::Value* aArgv,
                             UnwrapFuncPtr aUnwrapFunc)
{
  bool nativeEOL = false;
  if (aArgc > 1) {
    if (NS_IsMainThread()) {
      BlobPropertyBag d;
      if (!d.Init(aCx, JS::Handle<JS::Value>::fromMarkedLocation(&aArgv[1]))) {
        return NS_ERROR_TYPE_ERR;
      }
      mContentType = d.mType;
      nativeEOL = d.mEndings == EndingTypes::Native;
    } else {
      BlobPropertyBagWorkers d;
      if (!d.Init(aCx, JS::Handle<JS::Value>::fromMarkedLocation(&aArgv[1]))) {
        return NS_ERROR_TYPE_ERR;
      }
      mContentType = d.mType;
      nativeEOL = d.mEndings == EndingTypes::Native;
    }
  }

  if (aArgc > 0) {
    if (!aArgv[0].isObject()) {
      return NS_ERROR_TYPE_ERR; // We're not interested
    }

    JS::Rooted<JSObject*> obj(aCx, &aArgv[0].toObject());
    if (!JS_IsArrayObject(aCx, obj)) {
      return NS_ERROR_TYPE_ERR; // We're not interested
    }

    BlobSet blobSet;

    uint32_t length;
    JS_ALWAYS_TRUE(JS_GetArrayLength(aCx, obj, &length));
    for (uint32_t i = 0; i < length; ++i) {
      JS::Rooted<JS::Value> element(aCx);
      if (!JS_GetElement(aCx, obj, i, &element))
        return NS_ERROR_TYPE_ERR;

      if (element.isObject()) {
        JS::Rooted<JSObject*> obj(aCx, &element.toObject());
        nsCOMPtr<nsIDOMBlob> blob = aUnwrapFunc(aCx, obj);
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
          continue;
        }
        if (JS_IsArrayBufferViewObject(obj)) {
          blobSet.AppendVoidPtr(JS_GetArrayBufferViewData(obj),
                                JS_GetArrayBufferViewByteLength(obj));
          continue;
        }
        if (JS_IsArrayBufferObject(obj)) {
          blobSet.AppendArrayBuffer(obj);
          continue;
        }
        // neither Blob nor ArrayBuffer(View)
      } else if (element.isString()) {
        blobSet.AppendString(element.toString(), nativeEOL, aCx);
        continue;
      }
      // coerce it to a string
      JSString* str = JS_ValueToString(aCx, element);
      NS_ENSURE_TRUE(str, NS_ERROR_TYPE_ERR);
      blobSet.AppendString(str, nativeEOL, aCx);
    }

    mBlobs = blobSet.GetBlobs();
  }

  return NS_OK;
}

nsresult
nsDOMMultipartFile::InitFile(JSContext* aCx,
                             uint32_t aArgc,
                             JS::Value* aArgv)
{
  nsresult rv;

  NS_ASSERTION(!mImmutable, "Something went wrong ...");
  NS_ENSURE_TRUE(!mImmutable, NS_ERROR_UNEXPECTED);

  if (!nsContentUtils::IsCallerChrome()) {
    return NS_ERROR_DOM_SECURITY_ERR; // Real short trip
  }

  NS_ENSURE_TRUE(aArgc > 0, NS_ERROR_UNEXPECTED);

  if (aArgc > 1) {
    FilePropertyBag d;
    if (!d.Init(aCx, JS::Handle<JS::Value>::fromMarkedLocation(&aArgv[1]))) {
      return NS_ERROR_TYPE_ERR;
    }
    mName = d.mName;
    mContentType = d.mType;
  }


  // We expect to get a path to represent as a File object or
  // Blob object, an nsIFile, or an nsIDOMFile.
  nsCOMPtr<nsIFile> file;
  nsCOMPtr<nsIDOMBlob> blob;
  if (!aArgv[0].isString()) {
    // Lets see if it's an nsIFile
    if (!aArgv[0].isObject()) {
      return NS_ERROR_UNEXPECTED; // We're not interested
    }

    JSObject* obj = &aArgv[0].toObject();

    nsISupports* supports =
      nsContentUtils::XPConnect()->GetNativeOfWrapper(aCx, obj);
    if (!supports) {
      return NS_ERROR_UNEXPECTED;
    }

    blob = do_QueryInterface(supports);
    file = do_QueryInterface(supports);
    if (!blob && !file) {
      return NS_ERROR_UNEXPECTED;
    }
  } else {
    // It's a string
    JSString* str = JS_ValueToString(aCx, aArgv[0]);
    NS_ENSURE_TRUE(str, NS_ERROR_XPC_BAD_CONVERT_JS);

    nsDependentJSString xpcomStr;
    if (!xpcomStr.init(aCx, str)) {
      return NS_ERROR_XPC_BAD_CONVERT_JS;
    }

    rv = NS_NewLocalFile(xpcomStr, false, getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (file) {
    bool exists;
    rv = file->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(exists, NS_ERROR_FILE_NOT_FOUND);

    bool isDir;
    rv = file->IsDirectory(&isDir);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_FALSE(isDir, NS_ERROR_FILE_IS_DIRECTORY);

    if (mName.IsEmpty()) {
      file->GetLeafName(mName);
    }

    blob = new nsDOMFileFile(file);
  }

  // XXXkhuey this is terrible
  if (mContentType.IsEmpty()) {
    blob->GetType(mContentType);
  }

  BlobSet blobSet;
  blobSet.AppendBlob(blob);
  mBlobs = blobSet.GetBlobs();

  return NS_OK;
}

nsresult
BlobSet::AppendVoidPtr(const void* aData, uint32_t aLength)
{
  NS_ENSURE_ARG_POINTER(aData);

  uint64_t offset = mDataLen;

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
BlobSet::AppendArrayBuffer(JSObject* aBuffer)
{
  return AppendVoidPtr(JS_GetArrayBufferData(aBuffer),
                       JS_GetArrayBufferByteLength(aBuffer));
}

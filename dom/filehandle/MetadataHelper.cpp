/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MetadataHelper.h"

#include "FileHandle.h"
#include "js/Value.h"
#include "js/RootingAPI.h"
#include "jsapi.h"
#include "mozilla/dom/FileModeBinding.h"
#include "nsDebug.h"
#include "nsIFileStreams.h"
#include "nsIOutputStream.h"

namespace mozilla {
namespace dom {

nsresult
MetadataHelper::DoAsyncRun(nsISupports* aStream)
{
  bool readWrite = mFileHandle->mMode == FileMode::Readwrite;

  nsRefPtr<AsyncMetadataGetter> getter =
    new AsyncMetadataGetter(aStream, mParams, readWrite);

  nsresult rv = getter->AsyncWork(this, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
MetadataHelper::GetSuccessResult(JSContext* aCx,
                                 JS::MutableHandle<JS::Value> aVal)
{
  JS::Rooted<JSObject*> obj(aCx, JS_NewPlainObject(aCx));
  NS_ENSURE_TRUE(obj, NS_ERROR_OUT_OF_MEMORY);

  if (mParams->SizeRequested()) {
    JS::Rooted<JS::Value> val(aCx, JS_NumberValue(mParams->Size()));

    if (!JS_DefineProperty(aCx, obj, "size", val, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
  }

  if (mParams->LastModifiedRequested()) {
    double msec = mParams->LastModified();
    JSObject *date = JS_NewDateObjectMsec(aCx, msec);
    NS_ENSURE_TRUE(date, NS_ERROR_OUT_OF_MEMORY);

    JS::Rooted<JS::Value> dateRoot(aCx, JS::ObjectValue(*date));
    if (!JS_DefineProperty(aCx, obj, "lastModified", dateRoot,
                           JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
  }

  aVal.setObject(*obj);
  return NS_OK;
}

nsresult
MetadataHelper::AsyncMetadataGetter::DoStreamWork(nsISupports* aStream)
{
  nsresult rv;

  if (mReadWrite) {
    // Force a flush (so all pending writes are flushed to the disk and file
    // metadata is updated too).

    nsCOMPtr<nsIOutputStream> ostream = do_QueryInterface(aStream);
    NS_ASSERTION(ostream, "This should always succeed!");

    rv = ostream->Flush();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIFileMetadata> metadata = do_QueryInterface(aStream);

  if (mParams->SizeRequested()) {
    int64_t size;
    rv = metadata->GetSize(&size);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ENSURE_TRUE(size >= 0, NS_ERROR_FAILURE);

    mParams->mSize = uint64_t(size);
  }

  if (mParams->LastModifiedRequested()) {
    int64_t lastModified;
    rv = metadata->GetLastModified(&lastModified);
    NS_ENSURE_SUCCESS(rv, rv);

    mParams->mLastModified = lastModified;
  }

  return NS_OK;
}

} // namespace dom
} // namespace mozilla

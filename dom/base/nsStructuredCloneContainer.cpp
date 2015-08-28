/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsStructuredCloneContainer.h"

#include "nsCOMPtr.h"
#include "nsIGlobalObject.h"
#include "nsIVariant.h"
#include "nsIXPConnect.h"
#include "nsServiceManagerUtils.h"
#include "nsContentUtils.h"
#include "jsapi.h"
#include "xpcpublic.h"

#include "mozilla/Base64.h"
#include "mozilla/dom/StructuredCloneHelper.h"
#include "mozilla/dom/ScriptSettings.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ADDREF(nsStructuredCloneContainer)
NS_IMPL_RELEASE(nsStructuredCloneContainer)

NS_INTERFACE_MAP_BEGIN(nsStructuredCloneContainer)
  NS_INTERFACE_MAP_ENTRY(nsIStructuredCloneContainer)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

nsStructuredCloneContainer::nsStructuredCloneContainer()
  : StructuredCloneHelper(CloningSupported, TransferringNotSupported)
  , mState(eNotInitialized) , mData(nullptr), mSize(0), mVersion(0)
{
}

nsStructuredCloneContainer::~nsStructuredCloneContainer()
{
  if (mData) {
    free(mData);
  }
}

NS_IMETHODIMP
nsStructuredCloneContainer::InitFromJSVal(JS::Handle<JS::Value> aData,
                                          JSContext* aCx)
{
  if (mState != eNotInitialized) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult rv;
  Write(aCx, aData, true, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  mState = eInitializedFromJSVal;
  return NS_OK;
}

NS_IMETHODIMP
nsStructuredCloneContainer::InitFromBase64(const nsAString &aData,
                                           uint32_t aFormatVersion,
                                           JSContext* aCx)
{
  if (mState != eNotInitialized) {
    return NS_ERROR_FAILURE;
  }

  NS_ConvertUTF16toUTF8 data(aData);

  nsAutoCString binaryData;
  nsresult rv = Base64Decode(data, binaryData);
  NS_ENSURE_SUCCESS(rv, rv);

  // Copy the string's data into our own buffer.
  mData = (uint64_t*) malloc(binaryData.Length());
  NS_ENSURE_STATE(mData);
  memcpy(mData, binaryData.get(), binaryData.Length());

  mSize = binaryData.Length();
  mVersion = aFormatVersion;

  mState = eInitializedFromBase64;
  return NS_OK;
}

nsresult
nsStructuredCloneContainer::DeserializeToJsval(JSContext* aCx,
                                               JS::MutableHandle<JS::Value> aValue)
{
  aValue.setNull();
  JS::Rooted<JS::Value> jsStateObj(aCx);

  if (mState == eInitializedFromJSVal) {
    ErrorResult rv;
    Read(nullptr, aCx, &jsStateObj, rv);
    if (NS_WARN_IF(rv.Failed())) {
      return rv.StealNSResult();
    }
  } else {
    MOZ_ASSERT(mState == eInitializedFromBase64);
    MOZ_ASSERT(mData);

    ErrorResult rv;
    ReadFromBuffer(nullptr, aCx, mData, mSize, mVersion, &jsStateObj, rv);
    if (NS_WARN_IF(rv.Failed())) {
      return rv.StealNSResult();
    }
  }

  aValue.set(jsStateObj);
  return NS_OK;
}

NS_IMETHODIMP
nsStructuredCloneContainer::DeserializeToVariant(JSContext* aCx,
                                                 nsIVariant** aData)
{
  NS_ENSURE_ARG_POINTER(aData);
  *aData = nullptr;

  if (mState == eNotInitialized) {
    return NS_ERROR_FAILURE;
  }

  // Deserialize to a JS::Value.
  JS::Rooted<JS::Value> jsStateObj(aCx);
  nsresult rv = DeserializeToJsval(aCx, &jsStateObj);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Now wrap the JS::Value as an nsIVariant.
  nsCOMPtr<nsIVariant> varStateObj;
  nsCOMPtr<nsIXPConnect> xpconnect = do_GetService(nsIXPConnect::GetCID());
  NS_ENSURE_STATE(xpconnect);
  xpconnect->JSValToVariant(aCx, jsStateObj, getter_AddRefs(varStateObj));
  NS_ENSURE_STATE(varStateObj);

  varStateObj.forget(aData);
  return NS_OK;
}

NS_IMETHODIMP
nsStructuredCloneContainer::GetDataAsBase64(nsAString &aOut)
{
  aOut.Truncate();

  if (mState == eNotInitialized) {
    return NS_ERROR_FAILURE;
  }

  uint64_t* data;
  size_t size;

  if (mState == eInitializedFromJSVal) {
    if (HasClonedDOMObjects()) {
      return NS_ERROR_FAILURE;
    }

    data = BufferData();
    size = BufferSize();
  } else {
    MOZ_ASSERT(mState == eInitializedFromBase64);
    MOZ_ASSERT(mData);

    data = mData;
    size = mSize;
  }

  nsAutoCString binaryData(reinterpret_cast<char*>(data), size);
  nsAutoCString base64Data;
  nsresult rv = Base64Encode(binaryData, base64Data);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  CopyASCIItoUTF16(base64Data, aOut);
  return NS_OK;
}

NS_IMETHODIMP
nsStructuredCloneContainer::GetSerializedNBytes(uint64_t* aSize)
{
  NS_ENSURE_ARG_POINTER(aSize);

  if (mState == eNotInitialized) {
    return NS_ERROR_FAILURE;
  }

  if (mState == eInitializedFromJSVal) {
    *aSize = BufferSize();
    return NS_OK;
  }

  MOZ_ASSERT(mState == eInitializedFromBase64);

  // mSize is a size_t, while aSize is a uint64_t.  We rely on an implicit cast
  // here so that we'll get a compile error if a size_t-to-uint64_t cast is
  // narrowing.
  *aSize = mSize;

  return NS_OK;
}

NS_IMETHODIMP
nsStructuredCloneContainer::GetFormatVersion(uint32_t* aFormatVersion)
{
  NS_ENSURE_ARG_POINTER(aFormatVersion);

  if (mState == eNotInitialized) {
    return NS_ERROR_FAILURE;
  }

  if (mState == eInitializedFromJSVal) {
    *aFormatVersion = JS_STRUCTURED_CLONE_VERSION;
    return NS_OK;
  }

  MOZ_ASSERT(mState == eInitializedFromBase64);
  *aFormatVersion = mVersion;
  return NS_OK;
}

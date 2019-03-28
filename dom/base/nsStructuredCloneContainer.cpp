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
#include "mozilla/dom/ScriptSettings.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ADDREF(nsStructuredCloneContainer)
NS_IMPL_RELEASE(nsStructuredCloneContainer)

NS_INTERFACE_MAP_BEGIN(nsStructuredCloneContainer)
  NS_INTERFACE_MAP_ENTRY(nsIStructuredCloneContainer)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

nsStructuredCloneContainer::nsStructuredCloneContainer() : mVersion(0) {}

nsStructuredCloneContainer::~nsStructuredCloneContainer() {}

NS_IMETHODIMP
nsStructuredCloneContainer::InitFromJSVal(JS::Handle<JS::Value> aData,
                                          JSContext* aCx) {
  if (DataLength()) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult rv;
  Write(aCx, aData, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  mVersion = JS_STRUCTURED_CLONE_VERSION;
  return NS_OK;
}

NS_IMETHODIMP
nsStructuredCloneContainer::InitFromBase64(const nsAString& aData,
                                           uint32_t aFormatVersion) {
  if (DataLength()) {
    return NS_ERROR_FAILURE;
  }

  NS_ConvertUTF16toUTF8 data(aData);

  nsAutoCString binaryData;
  nsresult rv = Base64Decode(data, binaryData);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!CopyExternalData(binaryData.get(), binaryData.Length())) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mVersion = aFormatVersion;
  return NS_OK;
}

nsresult nsStructuredCloneContainer::DeserializeToJsval(
    JSContext* aCx, JS::MutableHandle<JS::Value> aValue) {
  aValue.setNull();
  JS::Rooted<JS::Value> jsStateObj(aCx);

  ErrorResult rv;
  Read(aCx, &jsStateObj, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  aValue.set(jsStateObj);
  return NS_OK;
}

NS_IMETHODIMP
nsStructuredCloneContainer::DeserializeToVariant(JSContext* aCx,
                                                 nsIVariant** aData) {
  NS_ENSURE_ARG_POINTER(aData);
  *aData = nullptr;

  if (!DataLength()) {
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
  nsCOMPtr<nsIXPConnect> xpconnect = nsIXPConnect::XPConnect();
  NS_ENSURE_STATE(xpconnect);
  xpconnect->JSValToVariant(aCx, jsStateObj, getter_AddRefs(varStateObj));
  NS_ENSURE_STATE(varStateObj);

  varStateObj.forget(aData);
  return NS_OK;
}

NS_IMETHODIMP
nsStructuredCloneContainer::GetDataAsBase64(nsAString& aOut) {
  aOut.Truncate();

  if (!DataLength()) {
    return NS_ERROR_FAILURE;
  }

  if (HasClonedDOMObjects()) {
    return NS_ERROR_FAILURE;
  }

  auto iter = Data().Start();
  size_t size = Data().Size();
  nsAutoCString binaryData;
  binaryData.SetLength(size);
  if (!Data().ReadBytes(iter, binaryData.BeginWriting(), size)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsAutoCString base64Data;
  nsresult rv = Base64Encode(binaryData, base64Data);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!CopyASCIItoUTF16(base64Data, aOut, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsStructuredCloneContainer::GetSerializedNBytes(uint64_t* aSize) {
  NS_ENSURE_ARG_POINTER(aSize);

  if (!DataLength()) {
    return NS_ERROR_FAILURE;
  }

  *aSize = DataLength();
  return NS_OK;
}

NS_IMETHODIMP
nsStructuredCloneContainer::GetFormatVersion(uint32_t* aFormatVersion) {
  NS_ENSURE_ARG_POINTER(aFormatVersion);

  if (!DataLength()) {
    return NS_ERROR_FAILURE;
  }

  *aFormatVersion = mVersion;
  return NS_OK;
}

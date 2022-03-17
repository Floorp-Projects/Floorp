/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsStructuredCloneContainer.h"

#include <cstddef>
#include <utility>
#include "ErrorList.h"
#include "js/RootingAPI.h"
#include "js/StructuredClone.h"
#include "js/Value.h"
#include "mozilla/Assertions.h"
#include "mozilla/Base64.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/fallible.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIVariant.h"
#include "nsIXPConnect.h"
#include "nsString.h"
#include "nscore.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ADDREF(nsStructuredCloneContainer)
NS_IMPL_RELEASE(nsStructuredCloneContainer)

NS_INTERFACE_MAP_BEGIN(nsStructuredCloneContainer)
  NS_INTERFACE_MAP_ENTRY(nsIStructuredCloneContainer)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

nsStructuredCloneContainer::nsStructuredCloneContainer() : mVersion(0) {}
nsStructuredCloneContainer::nsStructuredCloneContainer(uint32_t aVersion)
    : mVersion(aVersion) {}

nsStructuredCloneContainer::~nsStructuredCloneContainer() = default;

NS_IMETHODIMP
nsStructuredCloneContainer::InitFromJSVal(JS::Handle<JS::Value> aData,
                                          JSContext* aCx) {
  if (DataLength()) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult rv;
  Write(aCx, aData, rv);
  if (NS_WARN_IF(rv.Failed())) {
    // XXX propagate the error message as well.
    // We cannot StealNSResult because we threw a DOM exception.
    rv.SuppressException();
    return NS_ERROR_DOM_DATA_CLONE_ERR;
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
    // XXX propagate the error message as well.
    // We cannot StealNSResult because we threw a DOM exception.
    rv.SuppressException();
    return NS_ERROR_DOM_DATA_CLONE_ERR;
  }

  aValue.set(jsStateObj);
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
  CheckedInt<nsAutoCString::size_type> sizeCheck(size);
  if (!sizeCheck.isValid()) {
    return NS_ERROR_FAILURE;
  }

  nsAutoCString binaryData;
  if (!binaryData.SetLength(size, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  DebugOnly<bool> res = Data().ReadBytes(iter, binaryData.BeginWriting(), size);
  MOZ_ASSERT(res);

  nsresult rv = Base64Encode(binaryData, aOut);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
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

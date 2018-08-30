/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SDBResults.h"

#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

SDBResult::SDBResult(const nsACString& aData)
  : mData(aData)
{
}

NS_IMPL_ISUPPORTS(SDBResult,
                  nsISDBResult)

NS_IMETHODIMP
SDBResult::GetAsArray(uint32_t* aDataLen, uint8_t** aData)
{
  MOZ_ASSERT(aDataLen);
  MOZ_ASSERT(aData);

  if (mData.IsEmpty()) {
    *aDataLen = 0;
    *aData = nullptr;
    return NS_OK;
  }

  uint32_t length = mData.Length();

  uint8_t* data = static_cast<uint8_t*>(moz_xmalloc(length * sizeof(uint8_t)));

  memcpy(data, mData.BeginReading(), length * sizeof(uint8_t));

  *aDataLen = length;
  *aData = data;
  return NS_OK;
}

NS_IMETHODIMP
SDBResult::GetAsArrayBuffer(JSContext* aCx, JS::MutableHandleValue _retval)
{
  JS::Rooted<JSObject*> arrayBuffer(aCx);
  nsresult rv =
    nsContentUtils::CreateArrayBuffer(aCx, mData, arrayBuffer.address());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  _retval.setObject(*arrayBuffer);
  return NS_OK;
}

} // namespace dom
} // namespace mozilla

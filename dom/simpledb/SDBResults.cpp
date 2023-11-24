/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SDBResults.h"

#include <cstdint>
#include <cstring>
#include <new>
#include <utility>
#include "ErrorList.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "mozilla/Assertions.h"
#include "mozilla/MacroForEach.h"
#include "mozilla/dom/TypedArray.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsTArray.h"
#include "nscore.h"

namespace mozilla::dom {

SDBResult::SDBResult(const nsACString& aData) : mData(aData) {}

NS_IMPL_ISUPPORTS(SDBResult, nsISDBResult)

NS_IMETHODIMP
SDBResult::GetAsArray(nsTArray<uint8_t>& aData) {
  uint32_t length = mData.Length();
  aData.SetLength(length);

  if (length != 0) {
    memcpy(aData.Elements(), mData.BeginReading(), length * sizeof(uint8_t));
  }

  return NS_OK;
}

NS_IMETHODIMP
SDBResult::GetAsArrayBuffer(JSContext* aCx,
                            JS::MutableHandle<JS::Value> _retval) {
  ErrorResult rv;
  JS::Rooted<JSObject*> arrayBuffer(aCx, ArrayBuffer::Create(aCx, mData, rv));
  ENSURE_SUCCESS(rv, rv.StealNSResult());

  _retval.setObject(*arrayBuffer);
  return NS_OK;
}

}  // namespace mozilla::dom

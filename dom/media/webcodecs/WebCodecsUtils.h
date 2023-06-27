/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_webcodecs_Utils
#define mozilla_webcodecs_Utils

#include <tuple>

#include "ErrorList.h"
#include "mozilla/Result.h"
#include "mozilla/Span.h"
#include "mozilla/dom/UnionTypes.h"

namespace mozilla::dom {

/*
 * Below are helpers to operate ArrayBuffer or ArrayBufferView.
 */

template <class T>
Result<Span<uint8_t>, nsresult> GetArrayBufferData(const T& aBuffer);

Result<Span<uint8_t>, nsresult> GetSharedArrayBufferData(
    const MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aBuffer);

Result<Span<uint8_t>, nsresult> GetSharedArrayBufferData(
    const OwningMaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aBuffer);

}  // namespace mozilla::dom

#endif  // mozilla_webcodecs_Utils

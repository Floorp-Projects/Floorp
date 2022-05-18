/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ByteStreamHelpers_h
#define mozilla_dom_ByteStreamHelpers_h

#include "js/TypeDecls.h"
#include "mozilla/ErrorResult.h"
#include "UnderlyingSourceCallbackHelpers.h"

namespace mozilla::dom {

class ReadableStream;
class BodyStreamHolder;

// https://streams.spec.whatwg.org/#transfer-array-buffer
//
// As some parts of the specifcation want to use the abrupt completion value,
// this function may leave a pending exception if it returns nullptr.
JSObject* TransferArrayBuffer(JSContext* aCx, JS::Handle<JSObject*> aObject);

bool CanTransferArrayBuffer(JSContext* aCx, JS::Handle<JSObject*> aObject,
                            ErrorResult& aRv);

// If this returns null, it will leave a pending exception on aCx which
// must be handled by the caller (in the spec this is always the case
// currently).
JSObject* CloneAsUint8Array(JSContext* aCx, JS::Handle<JSObject*> aObject);

MOZ_CAN_RUN_SCRIPT void
SetUpReadableByteStreamControllerFromBodyStreamUnderlyingSource(
    JSContext* aCx, ReadableStream* aStream,
    BodyStreamHolder* aUnderlyingSource, ErrorResult& aRv);

}  // namespace mozilla::dom

#endif

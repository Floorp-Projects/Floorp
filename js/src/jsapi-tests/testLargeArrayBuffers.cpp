/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsfriendapi.h"

#include "js/ArrayBuffer.h"
#include "js/ArrayBufferMaybeShared.h"
#include "js/experimental/TypedData.h"
#include "jsapi-tests/tests.h"

using namespace js;

BEGIN_TEST(testLargeArrayBuffers) {
#ifdef JS_64BIT
  constexpr size_t nbytes = size_t(6) * 1024 * 1024 * 1024;  // 6 GB.

  RootedObject buffer(cx, JS::NewArrayBuffer(cx, nbytes));
  CHECK(JS::IsArrayBufferObject(buffer));

  size_t length;
  bool isShared;
  uint8_t* data;

  // ArrayBuffer
  {
    JS::GetArrayBufferLengthAndData(buffer, &length, &isShared, &data);
    CHECK_EQUAL(length, nbytes);

    length = 0;
    JS::GetArrayBufferMaybeSharedLengthAndData(buffer, &length, &isShared,
                                               &data);
    CHECK_EQUAL(length, nbytes);
  }

  // New Uint8Array
  {
    RootedObject tarr(cx, JS_NewUint8Array(cx, nbytes));
    CHECK(JS_IsTypedArrayObject(tarr));

    length = 0;
    js::GetArrayBufferViewLengthAndData(tarr, &length, &isShared, &data);
    CHECK_EQUAL(length, nbytes);

    length = 0;
    js::GetUint8ArrayLengthAndData(tarr, &length, &isShared, &data);
    CHECK_EQUAL(length, nbytes);
  }

  // Int16Array
  {
    RootedObject tarr(cx,
                      JS_NewInt16ArrayWithBuffer(cx, buffer, 0, nbytes / 2));
    CHECK(JS_IsTypedArrayObject(tarr));

    length = 0;
    js::GetArrayBufferViewLengthAndData(tarr, &length, &isShared, &data);
    CHECK_EQUAL(length, nbytes);

    length = 0;
    int16_t* int16Data;
    js::GetInt16ArrayLengthAndData(tarr, &length, &isShared, &int16Data);
    CHECK_EQUAL(length, nbytes / 2);
  }

  // DataView
  {
    RootedObject dv(cx, JS_NewDataView(cx, buffer, 0, nbytes - 10));
    CHECK(JS_IsArrayBufferViewObject(dv));

    length = 0;
    js::GetArrayBufferViewLengthAndData(dv, &length, &isShared, &data);
    CHECK_EQUAL(length, nbytes - 10);
  }
#endif

  return true;
}
END_TEST(testLargeArrayBuffers)

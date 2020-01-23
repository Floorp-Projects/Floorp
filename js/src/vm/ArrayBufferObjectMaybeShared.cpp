/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"  // MOZ_ASSERT

#include <stdint.h>  // uint8_t, uint32_t

#include "jstypes.h"  // JS_PUBLIC_API

#include "js/ArrayBufferMaybeShared.h"
#include "vm/ArrayBufferObject.h"  // js::ArrayBufferObject
#include "vm/JSObject.h"           // JSObject
#include "vm/SharedArrayObject.h"  // js::SharedArrayBufferObject
#include "vm/SharedMem.h"          // SharedMem

using namespace js;

JS_PUBLIC_API bool JS::IsArrayBufferObjectMaybeShared(JSObject* obj) {
  return obj->canUnwrapAs<ArrayBufferObjectMaybeShared>();
}

JS_PUBLIC_API JSObject* JS::UnwrapArrayBufferMaybeShared(JSObject* obj) {
  return obj->maybeUnwrapIf<ArrayBufferObjectMaybeShared>();
}

JS_PUBLIC_API void JS::GetArrayBufferMaybeSharedLengthAndData(
    JSObject* obj, uint32_t* length, bool* isSharedMemory, uint8_t** data) {
  MOZ_ASSERT(obj->is<ArrayBufferObjectMaybeShared>());

  bool isSharedArrayBuffer = obj->is<SharedArrayBufferObject>();
  *length = isSharedArrayBuffer
                ? obj->as<SharedArrayBufferObject>().byteLength()
                : obj->as<ArrayBufferObject>().byteLength();
  *data = isSharedArrayBuffer
              ? obj->as<SharedArrayBufferObject>().dataPointerShared().unwrap()
              : obj->as<ArrayBufferObject>().dataPointer();
  *isSharedMemory = isSharedArrayBuffer;
}

JS_PUBLIC_API uint8_t* JS::GetArrayBufferMaybeSharedData(
    JSObject* obj, bool* isSharedMemory, const JS::AutoRequireNoGC&) {
  MOZ_ASSERT(obj->maybeUnwrapIf<ArrayBufferObjectMaybeShared>());

  if (ArrayBufferObject* aobj = obj->maybeUnwrapIf<ArrayBufferObject>()) {
    *isSharedMemory = false;
    return aobj->dataPointer();
  } else if (SharedArrayBufferObject* saobj =
                 obj->maybeUnwrapIf<SharedArrayBufferObject>()) {
    *isSharedMemory = true;
    return saobj->dataPointerShared().unwrap();
  }

  return nullptr;
}

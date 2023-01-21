/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2023 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef wasm_wasm_gc_object_inl_h
#define wasm_wasm_gc_object_inl_h

#include "wasm/WasmGcObject.h"

#include "gc/Allocator.h"
#include "vm/Shape.h"
#include "wasm/WasmTypeDef.h"

#include "gc/ObjectKind-inl.h"
#include "vm/JSObject-inl.h"

namespace js {

inline bool WasmGcObject::AllocArgs::compute(JSContext* cx,
                                             const wasm::TypeDef* typeDef,
                                             AllocArgs* args) {
  // Get the class and alloc kind for the type definition
  const JSClass* clasp;
  gc::AllocKind allocKind;
  switch (typeDef->kind()) {
    case wasm::TypeDefKind::Struct: {
      clasp = WasmStructObject::classForTypeDef(typeDef);
      allocKind = WasmStructObject::allocKindForTypeDef(typeDef);
      break;
    }
    case wasm::TypeDefKind::Array: {
      clasp = &WasmArrayObject::class_;
      allocKind = WasmArrayObject::allocKind();
      break;
    }
    case wasm::TypeDefKind::Func:
    case wasm::TypeDefKind::None: {
      MOZ_CRASH();
    }
  }

  // Move the alloc kind to background if possible
  if (CanChangeToBackgroundAllocKind(allocKind, clasp)) {
    allocKind = ForegroundToBackgroundAllocKind(allocKind);
  }

  // Find the shape using the class and recursion group
  args->shape = WasmGCShape::getShape(cx, clasp, cx->realm(), TaggedProto(),
                                      &typeDef->recGroup(), ObjectFlags());
  if (!args->shape) {
    return false;
  }
  args->allocKind = allocKind;
  args->initialHeap = GetInitialHeap(GenericObject, clasp);
  args->clasp = clasp;
  return true;
}

}  // namespace js

#endif  // wasm_wasm_gc_object_inl_h

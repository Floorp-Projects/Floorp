/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef wasm_WasmGcObject_inl_h
#define wasm_WasmGcObject_inl_h

#include "wasm/WasmGcObject.h"

#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"

#include "gc/Nursery-inl.h"
#include "vm/JSContext-inl.h"

//=========================================================================
// WasmStructObject inlineable allocation methods

namespace js {

/* static */
template <bool ZeroFields>
MOZ_ALWAYS_INLINE WasmStructObject* WasmStructObject::createStructIL(
    JSContext* cx, wasm::TypeDefInstanceData* typeDefData,
    js::gc::Heap initialHeap) {
  // It is up to our caller to ensure that `typeDefData` refers to a type that
  // doesn't need OOL storage.

  MOZ_ASSERT(IsWasmGcObjectClass(typeDefData->clasp));
  MOZ_ASSERT(!typeDefData->clasp->isNativeObject());
  debugCheckNewObject(typeDefData->shape, typeDefData->allocKind, initialHeap);

  mozilla::DebugOnly<const wasm::TypeDef*> typeDef = typeDefData->typeDef;
  MOZ_ASSERT(typeDef->kind() == wasm::TypeDefKind::Struct);

  // This doesn't need to be rooted, since all we do with it prior to
  // return is to zero out the fields (and then only if ZeroFields is true).
  WasmStructObject* structObj = (WasmStructObject*)cx->newCell<WasmGcObject>(
      typeDefData->allocKind, initialHeap, typeDefData->clasp,
      &typeDefData->allocSite);
  if (MOZ_UNLIKELY(!structObj)) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  MOZ_ASSERT((uintptr_t(&(structObj->inlineData_[0])) % sizeof(uintptr_t)) ==
             0);
  structObj->initShape(typeDefData->shape);
  structObj->superTypeVector_ = typeDefData->superTypeVector;
  structObj->outlineData_ = nullptr;
  if constexpr (ZeroFields) {
    uint32_t totalBytes = typeDefData->structTypeSize;
    MOZ_ASSERT(totalBytes == typeDef->structType().size_);
    MOZ_ASSERT(totalBytes <= WasmStructObject_MaxInlineBytes);
    MOZ_ASSERT((totalBytes % sizeof(uintptr_t)) == 0);
    memset(&(structObj->inlineData_[0]), 0, totalBytes);
  }

  js::gc::gcprobes::CreateObject(structObj);
  probes::CreateObject(cx, structObj);

  return structObj;
}

/* static */
template <bool ZeroFields>
MOZ_ALWAYS_INLINE WasmStructObject* WasmStructObject::createStructOOL(
    JSContext* cx, wasm::TypeDefInstanceData* typeDefData,
    js::gc::Heap initialHeap) {
  // It is up to our caller to ensure that `typeDefData` refers to a type that
  // needs OOL storage.

  MOZ_ASSERT(IsWasmGcObjectClass(typeDefData->clasp));
  MOZ_ASSERT(!typeDefData->clasp->isNativeObject());
  debugCheckNewObject(typeDefData->shape, typeDefData->allocKind, initialHeap);

  mozilla::DebugOnly<const wasm::TypeDef*> typeDef = typeDefData->typeDef;
  MOZ_ASSERT(typeDef->kind() == wasm::TypeDefKind::Struct);

  uint32_t totalBytes = typeDefData->structTypeSize;
  MOZ_ASSERT(totalBytes == typeDef->structType().size_);
  MOZ_ASSERT(totalBytes > WasmStructObject_MaxInlineBytes);
  MOZ_ASSERT((totalBytes % sizeof(uintptr_t)) == 0);

  uint32_t inlineBytes, outlineBytes;
  WasmStructObject::getDataByteSizes(totalBytes, &inlineBytes, &outlineBytes);
  MOZ_ASSERT(inlineBytes == WasmStructObject_MaxInlineBytes);
  MOZ_ASSERT(outlineBytes > 0);

  // Allocate the outline data area before allocating the object so that we can
  // infallibly initialize the outline data area.
  Nursery& nursery = cx->nursery();
  PointerAndUint7 outlineData =
      nursery.mallocedBlockCache().alloc(outlineBytes);
  if (MOZ_UNLIKELY(!outlineData.pointer())) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  // See corresponding comment in WasmArrayObject::createArray.
  Rooted<WasmStructObject*> structObj(cx);
  structObj = (WasmStructObject*)cx->newCell<WasmGcObject>(
      typeDefData->allocKind, initialHeap, typeDefData->clasp,
      &typeDefData->allocSite);
  if (MOZ_UNLIKELY(!structObj)) {
    ReportOutOfMemory(cx);
    if (outlineData.pointer()) {
      nursery.mallocedBlockCache().free(outlineData);
    }
    return nullptr;
  }

  MOZ_ASSERT((uintptr_t(&(structObj->inlineData_[0])) % sizeof(uintptr_t)) ==
             0);
  structObj->initShape(typeDefData->shape);
  structObj->superTypeVector_ = typeDefData->superTypeVector;

  // Initialize the outline data fields
  structObj->outlineData_ = (uint8_t*)outlineData.pointer();
  if constexpr (ZeroFields) {
    memset(&(structObj->inlineData_[0]), 0, inlineBytes);
    memset(outlineData.pointer(), 0, outlineBytes);
  }

  if (MOZ_LIKELY(js::gc::IsInsideNursery(structObj))) {
    // See corresponding comment in WasmArrayObject::createArrayNonEmpty.
    if (MOZ_UNLIKELY(!nursery.registerTrailer(outlineData, outlineBytes))) {
      nursery.mallocedBlockCache().free(outlineData);
      ReportOutOfMemory(cx);
      return nullptr;
    }
  } else {
    // See corresponding comment in WasmArrayObject::createArrayNonEmpty.
    MOZ_ASSERT(structObj->isTenured());
    AddCellMemory(structObj, outlineBytes + wasm::TrailerBlockOverhead,
                  MemoryUse::WasmTrailerBlock);
  }

  js::gc::gcprobes::CreateObject(structObj);
  probes::CreateObject(cx, structObj);

  return structObj;
}

}  // namespace js

#endif /* wasm_WasmGcObject_inl_h */

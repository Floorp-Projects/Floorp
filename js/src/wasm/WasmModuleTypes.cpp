/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2015 Mozilla Foundation
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

#include "wasm/WasmModuleTypes.h"

#include "vm/MallocProvider.h"

using namespace js;
using namespace js::wasm;

size_t Import::serializedSize() const {
  return module.serializedSize() + field.serializedSize() + sizeof(kind);
}

uint8_t* Import::serialize(uint8_t* cursor) const {
  cursor = module.serialize(cursor);
  cursor = field.serialize(cursor);
  cursor = WriteScalar<DefinitionKind>(cursor, kind);
  return cursor;
}

const uint8_t* Import::deserialize(const uint8_t* cursor) {
  (cursor = module.deserialize(cursor)) &&
      (cursor = field.deserialize(cursor)) &&
      (cursor = ReadScalar<DefinitionKind>(cursor, &kind));
  return cursor;
}

size_t Import::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const {
  return module.sizeOfExcludingThis(mallocSizeOf) +
         field.sizeOfExcludingThis(mallocSizeOf);
}

Export::Export(UniqueChars fieldName, uint32_t index, DefinitionKind kind)
    : fieldName_(std::move(fieldName)) {
  pod.kind_ = kind;
  pod.index_ = index;
}

Export::Export(UniqueChars fieldName, DefinitionKind kind)
    : fieldName_(std::move(fieldName)) {
  pod.kind_ = kind;
  pod.index_ = 0;
}

uint32_t Export::funcIndex() const {
  MOZ_ASSERT(pod.kind_ == DefinitionKind::Function);
  return pod.index_;
}

uint32_t Export::globalIndex() const {
  MOZ_ASSERT(pod.kind_ == DefinitionKind::Global);
  return pod.index_;
}

#ifdef ENABLE_WASM_EXCEPTIONS
uint32_t Export::tagIndex() const {
  MOZ_ASSERT(pod.kind_ == DefinitionKind::Tag);
  return pod.index_;
}
#endif

uint32_t Export::tableIndex() const {
  MOZ_ASSERT(pod.kind_ == DefinitionKind::Table);
  return pod.index_;
}

size_t Export::serializedSize() const {
  return fieldName_.serializedSize() + sizeof(pod);
}

uint8_t* Export::serialize(uint8_t* cursor) const {
  cursor = fieldName_.serialize(cursor);
  cursor = WriteBytes(cursor, &pod, sizeof(pod));
  return cursor;
}

const uint8_t* Export::deserialize(const uint8_t* cursor) {
  (cursor = fieldName_.deserialize(cursor)) &&
      (cursor = ReadBytes(cursor, &pod, sizeof(pod)));
  return cursor;
}

size_t Export::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const {
  return fieldName_.sizeOfExcludingThis(mallocSizeOf);
}

size_t GlobalDesc::serializedSize() const {
  size_t size = sizeof(kind_);
  switch (kind_) {
    case GlobalKind::Import:
      size += initial_.serializedSize() + sizeof(offset_) + sizeof(isMutable_) +
              sizeof(isWasm_) + sizeof(isExport_) + sizeof(importIndex_);
      break;
    case GlobalKind::Variable:
      size += initial_.serializedSize() + sizeof(offset_) + sizeof(isMutable_) +
              sizeof(isWasm_) + sizeof(isExport_);
      break;
    case GlobalKind::Constant:
      size += initial_.serializedSize();
      break;
    default:
      MOZ_CRASH();
  }
  return size;
}

uint8_t* GlobalDesc::serialize(uint8_t* cursor) const {
  cursor = WriteBytes(cursor, &kind_, sizeof(kind_));
  switch (kind_) {
    case GlobalKind::Import:
      cursor = initial_.serialize(cursor);
      cursor = WriteBytes(cursor, &offset_, sizeof(offset_));
      cursor = WriteBytes(cursor, &isMutable_, sizeof(isMutable_));
      cursor = WriteBytes(cursor, &isWasm_, sizeof(isWasm_));
      cursor = WriteBytes(cursor, &isExport_, sizeof(isExport_));
      cursor = WriteBytes(cursor, &importIndex_, sizeof(importIndex_));
      break;
    case GlobalKind::Variable:
      cursor = initial_.serialize(cursor);
      cursor = WriteBytes(cursor, &offset_, sizeof(offset_));
      cursor = WriteBytes(cursor, &isMutable_, sizeof(isMutable_));
      cursor = WriteBytes(cursor, &isWasm_, sizeof(isWasm_));
      cursor = WriteBytes(cursor, &isExport_, sizeof(isExport_));
      break;
    case GlobalKind::Constant:
      cursor = initial_.serialize(cursor);
      break;
    default:
      MOZ_CRASH();
  }
  return cursor;
}

const uint8_t* GlobalDesc::deserialize(const uint8_t* cursor) {
  if (!(cursor = ReadBytes(cursor, &kind_, sizeof(kind_)))) {
    return nullptr;
  }
  switch (kind_) {
    case GlobalKind::Import:
      (cursor = initial_.deserialize(cursor)) &&
          (cursor = ReadBytes(cursor, &offset_, sizeof(offset_))) &&
          (cursor = ReadBytes(cursor, &isMutable_, sizeof(isMutable_))) &&
          (cursor = ReadBytes(cursor, &isWasm_, sizeof(isWasm_))) &&
          (cursor = ReadBytes(cursor, &isExport_, sizeof(isExport_))) &&
          (cursor = ReadBytes(cursor, &importIndex_, sizeof(importIndex_)));
      break;
    case GlobalKind::Variable:
      (cursor = initial_.deserialize(cursor)) &&
          (cursor = ReadBytes(cursor, &offset_, sizeof(offset_))) &&
          (cursor = ReadBytes(cursor, &isMutable_, sizeof(isMutable_))) &&
          (cursor = ReadBytes(cursor, &isWasm_, sizeof(isWasm_))) &&
          (cursor = ReadBytes(cursor, &isExport_, sizeof(isExport_)));
      break;
    case GlobalKind::Constant:
      cursor = initial_.deserialize(cursor);
      break;
    default:
      MOZ_CRASH();
  }
  return cursor;
}

size_t GlobalDesc::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const {
  return initial_.sizeOfExcludingThis(mallocSizeOf);
}

bool TagType::computeLayout() {
  StructLayout layout;
  int32_t refCount = 0;
  for (const ValType argType : argTypes) {
    if (argType.isRefRepr()) {
      refCount++;
    }
  }
  int32_t refIndex = 0;
  for (size_t i = 0; i < argTypes.length(); i++) {
    CheckedInt32 offset;
    if (argTypes[i].isRefRepr()) {
      NativeObject::elementsSizeMustNotOverflow();
      // Reference typed elements need to be loaded in reverse order
      // as they are pushed stack-like into the exception's Array.
      offset = (refCount - refIndex - 1) * CheckedInt32(sizeof(Value));
      refIndex++;
    } else {
      offset = layout.addField(FieldType(argTypes[i].packed()));
    }
    if (!offset.isValid()) {
      return false;
    }
    argOffsets[i] = offset.value();
  }
  this->refCount = refCount;

  CheckedInt32 size = layout.close();
  if (!size.isValid()) {
    return false;
  }
  this->bufferSize = size.value();

  return true;
}

size_t ElemSegment::serializedSize() const {
  return sizeof(kind) + sizeof(tableIndex) + sizeof(elemType) +
         SerializedMaybeSize(offsetIfActive) +
         SerializedPodVectorSize(elemFuncIndices);
}

uint8_t* ElemSegment::serialize(uint8_t* cursor) const {
  cursor = WriteBytes(cursor, &kind, sizeof(kind));
  cursor = WriteBytes(cursor, &tableIndex, sizeof(tableIndex));
  cursor = WriteBytes(cursor, &elemType, sizeof(elemType));
  cursor = SerializeMaybe(cursor, offsetIfActive);
  cursor = SerializePodVector(cursor, elemFuncIndices);
  return cursor;
}

const uint8_t* ElemSegment::deserialize(const uint8_t* cursor) {
  (cursor = ReadBytes(cursor, &kind, sizeof(kind))) &&
      (cursor = ReadBytes(cursor, &tableIndex, sizeof(tableIndex))) &&
      (cursor = ReadBytes(cursor, &elemType, sizeof(elemType))) &&
      (cursor = DeserializeMaybe(cursor, &offsetIfActive)) &&
      (cursor = DeserializePodVector(cursor, &elemFuncIndices));
  return cursor;
}

size_t ElemSegment::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const {
  return SizeOfMaybeExcludingThis(offsetIfActive, mallocSizeOf) +
         elemFuncIndices.sizeOfExcludingThis(mallocSizeOf);
}

size_t DataSegment::serializedSize() const {
  return SerializedMaybeSize(offsetIfActive) + SerializedPodVectorSize(bytes);
}

uint8_t* DataSegment::serialize(uint8_t* cursor) const {
  cursor = SerializeMaybe(cursor, offsetIfActive);
  cursor = SerializePodVector(cursor, bytes);
  return cursor;
}

const uint8_t* DataSegment::deserialize(const uint8_t* cursor) {
  (cursor = DeserializeMaybe(cursor, &offsetIfActive)) &&
      (cursor = DeserializePodVector(cursor, &bytes));
  return cursor;
}

size_t DataSegment::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const {
  return SizeOfMaybeExcludingThis(offsetIfActive, mallocSizeOf) +
         bytes.sizeOfExcludingThis(mallocSizeOf);
}

size_t CustomSection::serializedSize() const {
  return SerializedPodVectorSize(name) +
         SerializedPodVectorSize(payload->bytes);
}

uint8_t* CustomSection::serialize(uint8_t* cursor) const {
  cursor = SerializePodVector(cursor, name);
  cursor = SerializePodVector(cursor, payload->bytes);
  return cursor;
}

const uint8_t* CustomSection::deserialize(const uint8_t* cursor) {
  cursor = DeserializePodVector(cursor, &name);
  if (!cursor) {
    return nullptr;
  }

  Bytes bytes;
  cursor = DeserializePodVector(cursor, &bytes);
  if (!cursor) {
    return nullptr;
  }
  payload = js_new<ShareableBytes>(std::move(bytes));
  if (!payload) {
    return nullptr;
  }

  return cursor;
}

size_t CustomSection::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const {
  return name.sizeOfExcludingThis(mallocSizeOf) + sizeof(*payload) +
         payload->sizeOfExcludingThis(mallocSizeOf);
}

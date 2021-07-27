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

#include "wasm/WasmTypes.h"

#include "mozilla/FloatingPoint.h"

#include <algorithm>

#include "jsmath.h"
#include "jit/JitFrames.h"
#include "js/friend/ErrorMessages.h"  // JSMSG_*
#include "js/Printf.h"
#include "util/Memory.h"
#include "vm/ArrayBufferObject.h"
#include "vm/Warnings.h"  // js:WarnNumberASCII
#include "wasm/TypedObject.h"
#include "wasm/WasmBaselineCompile.h"
#include "wasm/WasmInstance.h"
#include "wasm/WasmJS.h"
#include "wasm/WasmSerialize.h"
#include "wasm/WasmStubs.h"

#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

using mozilla::IsPowerOfTwo;
using mozilla::MakeEnumeratedRange;

const JSClass WasmJSExceptionObject::class_ = {
    "WasmJSExnRefObject", JSCLASS_HAS_RESERVED_SLOTS(RESERVED_SLOTS)};

WasmJSExceptionObject* WasmJSExceptionObject::create(JSContext* cx,
                                                     MutableHandleValue value) {
  WasmJSExceptionObject* obj =
      NewObjectWithGivenProto<WasmJSExceptionObject>(cx, nullptr);

  if (!obj) {
    return nullptr;
  }

  obj->setFixedSlot(VALUE_SLOT, value);

  return obj;
}

ArgTypeVector::ArgTypeVector(const FuncType& funcType)
    : args_(funcType.args()),
      hasStackResults_(ABIResultIter::HasStackResults(
          ResultType::Vector(funcType.results()))) {}

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

bool TrapSiteVectorArray::empty() const {
  for (Trap trap : MakeEnumeratedRange(Trap::Limit)) {
    if (!(*this)[trap].empty()) {
      return false;
    }
  }

  return true;
}

void TrapSiteVectorArray::clear() {
  for (Trap trap : MakeEnumeratedRange(Trap::Limit)) {
    (*this)[trap].clear();
  }
}

void TrapSiteVectorArray::swap(TrapSiteVectorArray& rhs) {
  for (Trap trap : MakeEnumeratedRange(Trap::Limit)) {
    (*this)[trap].swap(rhs[trap]);
  }
}

void TrapSiteVectorArray::shrinkStorageToFit() {
  for (Trap trap : MakeEnumeratedRange(Trap::Limit)) {
    (*this)[trap].shrinkStorageToFit();
  }
}

size_t TrapSiteVectorArray::serializedSize() const {
  size_t ret = 0;
  for (Trap trap : MakeEnumeratedRange(Trap::Limit)) {
    ret += SerializedPodVectorSize((*this)[trap]);
  }
  return ret;
}

uint8_t* TrapSiteVectorArray::serialize(uint8_t* cursor) const {
  for (Trap trap : MakeEnumeratedRange(Trap::Limit)) {
    cursor = SerializePodVector(cursor, (*this)[trap]);
  }
  return cursor;
}

const uint8_t* TrapSiteVectorArray::deserialize(const uint8_t* cursor) {
  for (Trap trap : MakeEnumeratedRange(Trap::Limit)) {
    cursor = DeserializePodVector(cursor, &(*this)[trap]);
    if (!cursor) {
      return nullptr;
    }
  }
  return cursor;
}

size_t TrapSiteVectorArray::sizeOfExcludingThis(
    MallocSizeOf mallocSizeOf) const {
  size_t ret = 0;
  for (Trap trap : MakeEnumeratedRange(Trap::Limit)) {
    ret += (*this)[trap].sizeOfExcludingThis(mallocSizeOf);
  }
  return ret;
}

CodeRange::CodeRange(Kind kind, Offsets offsets)
    : begin_(offsets.begin), ret_(0), end_(offsets.end), kind_(kind) {
  MOZ_ASSERT(begin_ <= end_);
  PodZero(&u);
#ifdef DEBUG
  switch (kind_) {
    case FarJumpIsland:
    case TrapExit:
    case Throw:
      break;
    default:
      MOZ_CRASH("should use more specific constructor");
  }
#endif
}

CodeRange::CodeRange(Kind kind, uint32_t funcIndex, Offsets offsets)
    : begin_(offsets.begin), ret_(0), end_(offsets.end), kind_(kind) {
  u.funcIndex_ = funcIndex;
  u.func.lineOrBytecode_ = 0;
  u.func.beginToUncheckedCallEntry_ = 0;
  u.func.beginToTierEntry_ = 0;
  MOZ_ASSERT(isEntry());
  MOZ_ASSERT(begin_ <= end_);
}

CodeRange::CodeRange(Kind kind, CallableOffsets offsets)
    : begin_(offsets.begin), ret_(offsets.ret), end_(offsets.end), kind_(kind) {
  MOZ_ASSERT(begin_ < ret_);
  MOZ_ASSERT(ret_ < end_);
  PodZero(&u);
#ifdef DEBUG
  switch (kind_) {
    case DebugTrap:
    case BuiltinThunk:
      break;
    default:
      MOZ_CRASH("should use more specific constructor");
  }
#endif
}

CodeRange::CodeRange(Kind kind, uint32_t funcIndex, CallableOffsets offsets)
    : begin_(offsets.begin), ret_(offsets.ret), end_(offsets.end), kind_(kind) {
  MOZ_ASSERT(isImportExit() && !isImportJitExit());
  MOZ_ASSERT(begin_ < ret_);
  MOZ_ASSERT(ret_ < end_);
  u.funcIndex_ = funcIndex;
  u.func.lineOrBytecode_ = 0;
  u.func.beginToUncheckedCallEntry_ = 0;
  u.func.beginToTierEntry_ = 0;
}

CodeRange::CodeRange(uint32_t funcIndex, JitExitOffsets offsets)
    : begin_(offsets.begin),
      ret_(offsets.ret),
      end_(offsets.end),
      kind_(ImportJitExit) {
  MOZ_ASSERT(isImportJitExit());
  MOZ_ASSERT(begin_ < ret_);
  MOZ_ASSERT(ret_ < end_);
  u.funcIndex_ = funcIndex;
  u.jitExit.beginToUntrustedFPStart_ = offsets.untrustedFPStart - begin_;
  u.jitExit.beginToUntrustedFPEnd_ = offsets.untrustedFPEnd - begin_;
  MOZ_ASSERT(jitExitUntrustedFPStart() == offsets.untrustedFPStart);
  MOZ_ASSERT(jitExitUntrustedFPEnd() == offsets.untrustedFPEnd);
}

CodeRange::CodeRange(uint32_t funcIndex, uint32_t funcLineOrBytecode,
                     FuncOffsets offsets)
    : begin_(offsets.begin),
      ret_(offsets.ret),
      end_(offsets.end),
      kind_(Function) {
  MOZ_ASSERT(begin_ < ret_);
  MOZ_ASSERT(ret_ < end_);
  MOZ_ASSERT(offsets.uncheckedCallEntry - begin_ <= UINT8_MAX);
  MOZ_ASSERT(offsets.tierEntry - begin_ <= UINT8_MAX);
  u.funcIndex_ = funcIndex;
  u.func.lineOrBytecode_ = funcLineOrBytecode;
  u.func.beginToUncheckedCallEntry_ = offsets.uncheckedCallEntry - begin_;
  u.func.beginToTierEntry_ = offsets.tierEntry - begin_;
}

const CodeRange* wasm::LookupInSorted(const CodeRangeVector& codeRanges,
                                      CodeRange::OffsetInCode target) {
  size_t lowerBound = 0;
  size_t upperBound = codeRanges.length();

  size_t match;
  if (!BinarySearch(codeRanges, lowerBound, upperBound, target, &match)) {
    return nullptr;
  }

  return &codeRanges[match];
}

void wasm::Log(JSContext* cx, const char* fmt, ...) {
  MOZ_ASSERT(!cx->isExceptionPending());

  if (!cx->options().wasmVerbose()) {
    return;
  }

  va_list args;
  va_start(args, fmt);

  if (UniqueChars chars = JS_vsmprintf(fmt, args)) {
    WarnNumberASCII(cx, JSMSG_WASM_VERBOSE, chars.get());
    if (cx->isExceptionPending()) {
      cx->clearPendingException();
    }
  }

  va_end(args);
}

#ifdef WASM_CODEGEN_DEBUG
bool wasm::IsCodegenDebugEnabled(DebugChannel channel) {
  switch (channel) {
    case DebugChannel::Function:
      return JitOptions.enableWasmFuncCallSpew;
    case DebugChannel::Import:
      return JitOptions.enableWasmImportCallSpew;
  }
  return false;
}
#endif

void wasm::DebugCodegen(DebugChannel channel, const char* fmt, ...) {
#ifdef WASM_CODEGEN_DEBUG
  if (!IsCodegenDebugEnabled(channel)) {
    return;
  }
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
#endif
}

#ifdef ENABLE_WASM_SIMD_WORMHOLE
static const int8_t WormholeTrigger[] = {31, 0, 30, 2,  29, 4,  28, 6,
                                         27, 8, 26, 10, 25, 12, 24};
static_assert(sizeof(WormholeTrigger) == 15);

static const int8_t WormholeSignatureBytes[16] = {0xD, 0xE, 0xA, 0xD, 0xD, 0x0,
                                                  0x0, 0xD, 0xC, 0xA, 0xF, 0xE,
                                                  0xB, 0xA, 0xB, 0xE};
static_assert(sizeof(WormholeSignatureBytes) == 16);

bool wasm::IsWormholeTrigger(const V128& shuffleMask) {
  return memcmp(shuffleMask.bytes, WormholeTrigger, sizeof(WormholeTrigger)) ==
         0;
}

jit::SimdConstant wasm::WormholeSignature() {
  return jit::SimdConstant::CreateX16(WormholeSignatureBytes);
}

#endif

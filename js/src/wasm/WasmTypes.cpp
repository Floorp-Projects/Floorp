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

#include "js/Printf.h"
#include "util/Memory.h"
#include "vm/ArrayBufferObject.h"
#include "vm/Warnings.h"  // js:WarnNumberASCII
#include "wasm/WasmBaselineCompile.h"
#include "wasm/WasmInstance.h"
#include "wasm/WasmSerialize.h"
#include "wasm/WasmStubs.h"

#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

using mozilla::IsPowerOfTwo;
using mozilla::MakeEnumeratedRange;

// We have only tested huge memory on x64 and arm64.

#if defined(WASM_SUPPORTS_HUGE_MEMORY)
#  if !(defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_ARM64))
#    error "Not an expected configuration"
#  endif
#endif

// More sanity checks.

static_assert(MaxMemoryInitialPages <=
                  ArrayBufferObject::MaxBufferByteLength / PageSize,
              "Memory sizing constraint");

// All plausible targets must be able to do at least IEEE754 double
// loads/stores, hence the lower limit of 8.  Some Intel processors support
// AVX-512 loads/stores, hence the upper limit of 64.
static_assert(MaxMemoryAccessSize >= 8, "MaxMemoryAccessSize too low");
static_assert(MaxMemoryAccessSize <= 64, "MaxMemoryAccessSize too high");
static_assert((MaxMemoryAccessSize & (MaxMemoryAccessSize - 1)) == 0,
              "MaxMemoryAccessSize is not a power of two");

#if defined(WASM_SUPPORTS_HUGE_MEMORY)
static_assert(HugeMappedSize > ArrayBufferObject::MaxBufferByteLength,
              "Normal array buffer could be confused with huge memory");
#endif

Val::Val(const LitVal& val) {
  type_ = val.type();
  switch (type_.kind()) {
    case ValType::I32:
      u.i32_ = val.i32();
      return;
    case ValType::F32:
      u.f32_ = val.f32();
      return;
    case ValType::I64:
      u.i64_ = val.i64();
      return;
    case ValType::F64:
      u.f64_ = val.f64();
      return;
    case ValType::Ref:
      u.ref_ = val.ref();
      return;
  }
  MOZ_CRASH();
}

void Val::trace(JSTracer* trc) {
  if (type_.isValid() && type_.isReference() && !u.ref_.isNull()) {
    // TODO/AnyRef-boxing: With boxed immediates and strings, the write
    // barrier is going to have to be more complicated.
    ASSERT_ANYREF_IS_JSOBJECT;
    TraceManuallyBarrieredEdge(trc, u.ref_.asJSObjectAddress(),
                               "wasm reference-typed global");
  }
}

void AnyRef::trace(JSTracer* trc) {
  if (value_) {
    TraceManuallyBarrieredEdge(trc, &value_, "wasm anyref referent");
  }
}

const JSClass WasmValueBox::class_ = {
    "WasmValueBox", JSCLASS_HAS_RESERVED_SLOTS(RESERVED_SLOTS)};

WasmValueBox* WasmValueBox::create(JSContext* cx, HandleValue val) {
  WasmValueBox* obj = (WasmValueBox*)NewObjectWithGivenProto(
      cx, &WasmValueBox::class_, nullptr);
  if (!obj) {
    return nullptr;
  }
  obj->setFixedSlot(VALUE_SLOT, val);
  return obj;
}

bool wasm::BoxAnyRef(JSContext* cx, HandleValue val, MutableHandleAnyRef addr) {
  if (val.isNull()) {
    addr.set(AnyRef::null());
    return true;
  }

  if (val.isObject()) {
    JSObject* obj = &val.toObject();
    MOZ_ASSERT(!obj->is<WasmValueBox>());
    MOZ_ASSERT(obj->compartment() == cx->compartment());
    addr.set(AnyRef::fromJSObject(obj));
    return true;
  }

  WasmValueBox* box = WasmValueBox::create(cx, val);
  if (!box) return false;
  addr.set(AnyRef::fromJSObject(box));
  return true;
}

JSObject* wasm::BoxBoxableValue(JSContext* cx, HandleValue val) {
  MOZ_ASSERT(!val.isNull() && !val.isObject());
  return WasmValueBox::create(cx, val);
}

Value wasm::UnboxAnyRef(AnyRef val) {
  // If UnboxAnyRef needs to allocate then we need a more complicated API, and
  // we need to root the value in the callers, see comments in callExport().
  JSObject* obj = val.asJSObject();
  Value result;
  if (obj == nullptr) {
    result.setNull();
  } else if (obj->is<WasmValueBox>()) {
    result = obj->as<WasmValueBox>().value();
  } else {
    result.setObjectOrNull(obj);
  }
  return result;
}

Value wasm::UnboxFuncRef(FuncRef val) {
  JSFunction* fn = val.asJSFunction();
  Value result;
  MOZ_ASSERT_IF(fn, fn->is<JSFunction>());
  result.setObjectOrNull(fn);
  return result;
}

bool js::IsBoxedWasmAnyRef(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  MOZ_ASSERT(args.length() == 1);
  args.rval().setBoolean(args[0].isObject() &&
                         args[0].toObject().is<WasmValueBox>());
  return true;
}

bool js::IsBoxableWasmAnyRef(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  MOZ_ASSERT(args.length() == 1);
  args.rval().setBoolean(!(args[0].isObject() || args[0].isNull()));
  return true;
}

bool js::BoxWasmAnyRef(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  MOZ_ASSERT(args.length() == 1);
  WasmValueBox* box = WasmValueBox::create(cx, args[0]);
  if (!box) return false;
  args.rval().setObject(*box);
  return true;
}

bool js::UnboxBoxedWasmAnyRef(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  MOZ_ASSERT(args.length() == 1);
  WasmValueBox* box = &args[0].toObject().as<WasmValueBox>();
  args.rval().set(box->value());
  return true;
}

bool wasm::IsRoundingFunction(SymbolicAddress callee, jit::RoundingMode* mode) {
  switch (callee) {
    case SymbolicAddress::FloorD:
    case SymbolicAddress::FloorF:
      *mode = jit::RoundingMode::Down;
      return true;
    case SymbolicAddress::CeilD:
    case SymbolicAddress::CeilF:
      *mode = jit::RoundingMode::Up;
      return true;
    case SymbolicAddress::TruncD:
    case SymbolicAddress::TruncF:
      *mode = jit::RoundingMode::TowardsZero;
      return true;
    case SymbolicAddress::NearbyIntD:
    case SymbolicAddress::NearbyIntF:
      *mode = jit::RoundingMode::NearestTiesToEven;
      return true;
    default:
      return false;
  }
}

size_t FuncType::serializedSize() const {
  return SerializedPodVectorSize(results_) + SerializedPodVectorSize(args_);
}

uint8_t* FuncType::serialize(uint8_t* cursor) const {
  cursor = SerializePodVector(cursor, results_);
  cursor = SerializePodVector(cursor, args_);
  return cursor;
}

const uint8_t* FuncType::deserialize(const uint8_t* cursor) {
  cursor = DeserializePodVector(cursor, &results_);
  if (!cursor) {
    return nullptr;
  }
  return DeserializePodVector(cursor, &args_);
}

size_t FuncType::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const {
  return args_.sizeOfExcludingThis(mallocSizeOf);
}

using ImmediateType = uint32_t;  // for 32/64 consistency
static const unsigned sTotalBits = sizeof(ImmediateType) * 8;
static const unsigned sTagBits = 1;
static const unsigned sReturnBit = 1;
static const unsigned sLengthBits = 4;
static const unsigned sTypeBits = 3;
static const unsigned sMaxTypes =
    (sTotalBits - sTagBits - sReturnBit - sLengthBits) / sTypeBits;

static bool IsImmediateType(ValType vt) {
  switch (vt.kind()) {
    case ValType::I32:
    case ValType::I64:
    case ValType::F32:
    case ValType::F64:
      return true;
    case ValType::Ref:
      switch (vt.refTypeKind()) {
        case RefType::Func:
        case RefType::Any:
        case RefType::Null:
          return true;
        case RefType::TypeIndex:
          return false;
      }
      break;
  }
  MOZ_CRASH("bad ValType");
}

static unsigned EncodeImmediateType(ValType vt) {
  static_assert(4 < (1 << sTypeBits), "fits");
  switch (vt.kind()) {
    case ValType::I32:
      return 0;
    case ValType::I64:
      return 1;
    case ValType::F32:
      return 2;
    case ValType::F64:
      return 3;
    case ValType::Ref:
      switch (vt.refTypeKind()) {
        case RefType::Func:
          return 4;
        case RefType::Any:
          return 5;
        case RefType::Null:
          return 6;
        case RefType::TypeIndex:
          break;
      }
      break;
  }
  MOZ_CRASH("bad ValType");
}

/* static */
bool FuncTypeIdDesc::isGlobal(const FuncType& funcType) {
  const ValTypeVector& results = funcType.results();
  const ValTypeVector& args = funcType.args();
  if (results.length() + args.length() > sMaxTypes) {
    return true;
  }

  if (results.length() > 1) {
    return true;
  }

  for (ValType v : results) {
    if (!IsImmediateType(v)) {
      return true;
    }
  }

  for (ValType v : args) {
    if (!IsImmediateType(v)) {
      return true;
    }
  }

  return false;
}

/* static */
FuncTypeIdDesc FuncTypeIdDesc::global(const FuncType& funcType,
                                      uint32_t globalDataOffset) {
  MOZ_ASSERT(isGlobal(funcType));
  return FuncTypeIdDesc(FuncTypeIdDescKind::Global, globalDataOffset);
}

static ImmediateType LengthToBits(uint32_t length) {
  static_assert(sMaxTypes <= ((1 << sLengthBits) - 1), "fits");
  MOZ_ASSERT(length <= sMaxTypes);
  return length;
}

/* static */
FuncTypeIdDesc FuncTypeIdDesc::immediate(const FuncType& funcType) {
  ImmediateType immediate = ImmediateBit;
  uint32_t shift = sTagBits;

  if (funcType.results().length() > 0) {
    MOZ_ASSERT(funcType.results().length() == 1);
    immediate |= (1 << shift);
    shift += sReturnBit;

    immediate |= EncodeImmediateType(funcType.results()[0]) << shift;
    shift += sTypeBits;
  } else {
    shift += sReturnBit;
  }

  immediate |= LengthToBits(funcType.args().length()) << shift;
  shift += sLengthBits;

  for (ValType argType : funcType.args()) {
    immediate |= EncodeImmediateType(argType) << shift;
    shift += sTypeBits;
  }

  MOZ_ASSERT(shift <= sTotalBits);
  return FuncTypeIdDesc(FuncTypeIdDescKind::Immediate, immediate);
}

size_t FuncTypeWithId::serializedSize() const {
  return FuncType::serializedSize() + sizeof(id);
}

uint8_t* FuncTypeWithId::serialize(uint8_t* cursor) const {
  cursor = FuncType::serialize(cursor);
  cursor = WriteBytes(cursor, &id, sizeof(id));
  return cursor;
}

const uint8_t* FuncTypeWithId::deserialize(const uint8_t* cursor) {
  (cursor = FuncType::deserialize(cursor)) &&
      (cursor = ReadBytes(cursor, &id, sizeof(id)));
  return cursor;
}

size_t FuncTypeWithId::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const {
  return FuncType::sizeOfExcludingThis(mallocSizeOf);
}

ArgTypeVector::ArgTypeVector(const FuncType& funcType)
    : args_(funcType.args()),
      hasStackResults_(ABIResultIter::HasStackResults(
          ResultType::Vector(funcType.results()))) {}

// A simple notion of prefix: types and mutability must match exactly.

bool StructType::hasPrefix(const StructType& other) const {
  if (fields_.length() < other.fields_.length()) {
    return false;
  }
  uint32_t limit = other.fields_.length();
  for (uint32_t i = 0; i < limit; i++) {
    if (fields_[i].type != other.fields_[i].type ||
        fields_[i].isMutable != other.fields_[i].isMutable) {
      return false;
    }
  }
  return true;
}

size_t StructType::serializedSize() const {
  return SerializedPodVectorSize(fields_);
}

uint8_t* StructType::serialize(uint8_t* cursor) const {
  cursor = SerializePodVector(cursor, fields_);
  return cursor;
}

const uint8_t* StructType::deserialize(const uint8_t* cursor) {
  (cursor = DeserializePodVector(cursor, &fields_));
  return cursor;
}

size_t StructType::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const {
  return fields_.sizeOfExcludingThis(mallocSizeOf);
}

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

size_t ElemSegment::serializedSize() const {
  return sizeof(kind) + sizeof(tableIndex) + sizeof(elementType) +
         sizeof(offsetIfActive) + SerializedPodVectorSize(elemFuncIndices);
}

uint8_t* ElemSegment::serialize(uint8_t* cursor) const {
  cursor = WriteBytes(cursor, &kind, sizeof(kind));
  cursor = WriteBytes(cursor, &tableIndex, sizeof(tableIndex));
  cursor = WriteBytes(cursor, &elementType, sizeof(elementType));
  cursor = WriteBytes(cursor, &offsetIfActive, sizeof(offsetIfActive));
  cursor = SerializePodVector(cursor, elemFuncIndices);
  return cursor;
}

const uint8_t* ElemSegment::deserialize(const uint8_t* cursor) {
  (cursor = ReadBytes(cursor, &kind, sizeof(kind))) &&
      (cursor = ReadBytes(cursor, &tableIndex, sizeof(tableIndex))) &&
      (cursor = ReadBytes(cursor, &elementType, sizeof(elementType))) &&
      (cursor = ReadBytes(cursor, &offsetIfActive, sizeof(offsetIfActive))) &&
      (cursor = DeserializePodVector(cursor, &elemFuncIndices));
  return cursor;
}

size_t ElemSegment::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const {
  return elemFuncIndices.sizeOfExcludingThis(mallocSizeOf);
}

size_t DataSegment::serializedSize() const {
  return sizeof(offsetIfActive) + SerializedPodVectorSize(bytes);
}

uint8_t* DataSegment::serialize(uint8_t* cursor) const {
  cursor = WriteBytes(cursor, &offsetIfActive, sizeof(offsetIfActive));
  cursor = SerializePodVector(cursor, bytes);
  return cursor;
}

const uint8_t* DataSegment::deserialize(const uint8_t* cursor) {
  (cursor = ReadBytes(cursor, &offsetIfActive, sizeof(offsetIfActive))) &&
      (cursor = DeserializePodVector(cursor, &bytes));
  return cursor;
}

size_t DataSegment::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const {
  return bytes.sizeOfExcludingThis(mallocSizeOf);
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

//  Heap length on ARM should fit in an ARM immediate. We approximate the set
//  of valid ARM immediates with the predicate:
//    2^n for n in [16, 24)
//  or
//    2^24 * n for n >= 1.
bool wasm::IsValidARMImmediate(uint32_t i) {
  bool valid = (IsPowerOfTwo(i) || (i & 0x00ffffff) == 0);

  MOZ_ASSERT_IF(valid, i % PageSize == 0);

  return valid;
}

uint32_t wasm::RoundUpToNextValidARMImmediate(uint32_t i) {
  MOZ_ASSERT(i <= 0xff000000);

  if (i <= 16 * 1024 * 1024) {
    i = i ? mozilla::RoundUpPow2(i) : 0;
  } else {
    i = (i + 0x00ffffff) & ~0x00ffffff;
  }

  MOZ_ASSERT(IsValidARMImmediate(i));

  return i;
}

bool wasm::IsValidBoundsCheckImmediate(uint32_t i) {
#ifdef JS_CODEGEN_ARM
  return IsValidARMImmediate(i);
#else
  return true;
#endif
}

size_t wasm::ComputeMappedSize(uint32_t maxSize) {
  MOZ_ASSERT(maxSize % PageSize == 0);

  // It is the bounds-check limit, not the mapped size, that gets baked into
  // code. Thus round up the maxSize to the next valid immediate value
  // *before* adding in the guard page.

#ifdef JS_CODEGEN_ARM
  uint32_t boundsCheckLimit = RoundUpToNextValidARMImmediate(maxSize);
#else
  uint32_t boundsCheckLimit = maxSize;
#endif
  MOZ_ASSERT(IsValidBoundsCheckImmediate(boundsCheckLimit));

  MOZ_ASSERT(boundsCheckLimit % gc::SystemPageSize() == 0);
  MOZ_ASSERT(GuardSize % gc::SystemPageSize() == 0);
  return boundsCheckLimit + GuardSize;
}

/* static */
DebugFrame* DebugFrame::from(Frame* fp) {
  MOZ_ASSERT(fp->tls->instance->code().metadata().debugEnabled);
  auto* df =
      reinterpret_cast<DebugFrame*>((uint8_t*)fp - DebugFrame::offsetOfFrame());
  MOZ_ASSERT(fp->instance() == df->instance());
  return df;
}

void DebugFrame::alignmentStaticAsserts() {
  // VS2017 doesn't consider offsetOfFrame() to be a constexpr, so we have
  // to use offsetof directly. These asserts can't be at class-level
  // because the type is incomplete.

  static_assert(WasmStackAlignment >= Alignment,
                "Aligned by ABI before pushing DebugFrame");
  static_assert((offsetof(DebugFrame, frame_) + sizeof(Frame)) % Alignment == 0,
                "Aligned after pushing DebugFrame");
#ifdef JS_CODEGEN_ARM64
  // This constraint may or may not be necessary.  If you hit this because
  // you've changed the frame size then feel free to remove it, but be extra
  // aware of possible problems.
  static_assert(sizeof(DebugFrame) % 16 == 0, "ARM64 SP alignment");
#endif
}

GlobalObject* DebugFrame::global() const {
  return &instance()->object()->global();
}

bool DebugFrame::hasGlobal(const GlobalObject* global) const {
  return global == &instance()->objectUnbarriered()->global();
}

JSObject* DebugFrame::environmentChain() const {
  return &global()->lexicalEnvironment();
}

bool DebugFrame::getLocal(uint32_t localIndex, MutableHandleValue vp) {
  ValTypeVector locals;
  size_t argsLength;
  StackResults stackResults;
  if (!instance()->debug().debugGetLocalTypes(funcIndex(), &locals, &argsLength,
                                              &stackResults)) {
    return false;
  }

  ValTypeVector args;
  MOZ_ASSERT(argsLength <= locals.length());
  if (!args.append(locals.begin(), argsLength)) {
    return false;
  }
  ArgTypeVector abiArgs(args, stackResults);

  BaseLocalIter iter(locals, abiArgs, /* debugEnabled = */ true);
  while (!iter.done() && iter.index() < localIndex) {
    iter++;
  }
  MOZ_ALWAYS_TRUE(!iter.done());

  uint8_t* frame = static_cast<uint8_t*>((void*)this) + offsetOfFrame();
  void* dataPtr = frame - iter.frameOffset();
  switch (iter.mirType()) {
    case jit::MIRType::Int32:
      vp.set(Int32Value(*static_cast<int32_t*>(dataPtr)));
      break;
    case jit::MIRType::Int64:
      // Just display as a Number; it's ok if we lose some precision
      vp.set(NumberValue((double)*static_cast<int64_t*>(dataPtr)));
      break;
    case jit::MIRType::Float32:
      vp.set(NumberValue(JS::CanonicalizeNaN(*static_cast<float*>(dataPtr))));
      break;
    case jit::MIRType::Double:
      vp.set(NumberValue(JS::CanonicalizeNaN(*static_cast<double*>(dataPtr))));
      break;
    case jit::MIRType::RefOrNull:
      vp.set(ObjectOrNullValue(*(JSObject**)dataPtr));
      break;
    default:
      MOZ_CRASH("local type");
  }
  return true;
}

bool DebugFrame::updateReturnJSValue() {
  hasCachedReturnJSValue_ = true;
  ValTypeVector results;
  if (!instance()->debug().debugGetResultTypes(funcIndex(), &results)) {
    return false;
  }
  if (results.length() == 0) {
    cachedReturnJSValue_.setUndefined();
    return true;
  }
  MOZ_ASSERT(results.length() == 1, "multi-value return unimplemented");
  switch (results[0].kind()) {
    case ValType::I32:
      cachedReturnJSValue_.setInt32(resultI32_);
      break;
    case ValType::I64:
      // Just display as a Number; it's ok if we lose some precision
      cachedReturnJSValue_.setDouble((double)resultI64_);
      break;
    case ValType::F32:
      cachedReturnJSValue_.setDouble(JS::CanonicalizeNaN(resultF32_));
      break;
    case ValType::F64:
      cachedReturnJSValue_.setDouble(JS::CanonicalizeNaN(resultF64_));
      break;
    case ValType::Ref:
      switch (results[0].refTypeKind()) {
        case RefType::TypeIndex:
          cachedReturnJSValue_ = ObjectOrNullValue((JSObject*)resultRef_);
          break;
        case RefType::Func:
          cachedReturnJSValue_ =
              UnboxFuncRef(FuncRef::fromAnyRefUnchecked(resultAnyRef_));
          break;
        case RefType::Any:
        case RefType::Null:
          cachedReturnJSValue_ = UnboxAnyRef(resultAnyRef_);
          break;
      }
      break;
    default:
      MOZ_CRASH("result type");
  }
  return true;
}

HandleValue DebugFrame::returnValue() const {
  MOZ_ASSERT(hasCachedReturnJSValue_);
  return HandleValue::fromMarkedLocation(&cachedReturnJSValue_);
}

void DebugFrame::clearReturnJSValue() {
  hasCachedReturnJSValue_ = true;
  cachedReturnJSValue_.setUndefined();
}

void DebugFrame::observe(JSContext* cx) {
  if (!observing_) {
    instance()->debug().adjustEnterAndLeaveFrameTrapsState(
        cx, /* enabled = */ true);
    observing_ = true;
  }
}

void DebugFrame::leave(JSContext* cx) {
  if (observing_) {
    instance()->debug().adjustEnterAndLeaveFrameTrapsState(
        cx, /* enabled = */ false);
    observing_ = false;
  }
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
  u.func.beginToNormalEntry_ = 0;
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
  u.func.beginToNormalEntry_ = 0;
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
  MOZ_ASSERT(offsets.normalEntry - begin_ <= UINT8_MAX);
  MOZ_ASSERT(offsets.tierEntry - begin_ <= UINT8_MAX);
  u.funcIndex_ = funcIndex;
  u.func.lineOrBytecode_ = funcLineOrBytecode;
  u.func.beginToNormalEntry_ = offsets.normalEntry - begin_;
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

UniqueTlsData wasm::CreateTlsData(uint32_t globalDataLength) {
  void* allocatedBase = js_calloc(TlsDataAlign + offsetof(TlsData, globalArea) +
                                  globalDataLength);
  if (!allocatedBase) {
    return nullptr;
  }

  auto* tlsData = reinterpret_cast<TlsData*>(
      AlignBytes(uintptr_t(allocatedBase), TlsDataAlign));
  tlsData->allocatedBase = allocatedBase;

  return UniqueTlsData(tlsData);
}

void TlsData::setInterrupt() {
  interrupt = true;
  stackLimit = UINTPTR_MAX;
}

bool TlsData::isInterrupted() const {
  return interrupt || stackLimit == UINTPTR_MAX;
}

void TlsData::resetInterrupt(JSContext* cx) {
  interrupt = false;
  stackLimit = cx->stackLimitForJitCode(JS::StackForUntrustedScript);
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

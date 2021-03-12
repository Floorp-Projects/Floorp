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

using mozilla::CheckedInt32;
using mozilla::IsPowerOfTwo;
using mozilla::MakeEnumeratedRange;

// We have only tested huge memory on x64 and arm64.

#if defined(WASM_SUPPORTS_HUGE_MEMORY)
#  if !(defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_ARM64))
#    error "Not an expected configuration"
#  endif
#endif

// All plausible targets must be able to do at least IEEE754 double
// loads/stores, hence the lower limit of 8.  Some Intel processors support
// AVX-512 loads/stores, hence the upper limit of 64.
static_assert(MaxMemoryAccessSize >= 8, "MaxMemoryAccessSize too low");
static_assert(MaxMemoryAccessSize <= 64, "MaxMemoryAccessSize too high");
static_assert((MaxMemoryAccessSize & (MaxMemoryAccessSize - 1)) == 0,
              "MaxMemoryAccessSize is not a power of two");

#if defined(WASM_SUPPORTS_HUGE_MEMORY)
static_assert(HugeMappedSize > MaxMemory32Bytes,
              "Normal array buffer could be confused with huge memory");
#endif

Val::Val(const LitVal& val) {
  type_ = val.type();
  switch (type_.kind()) {
    case ValType::I32:
      cell_.i32_ = val.i32();
      return;
    case ValType::F32:
      cell_.f32_ = val.f32();
      return;
    case ValType::I64:
      cell_.i64_ = val.i64();
      return;
    case ValType::F64:
      cell_.f64_ = val.f64();
      return;
    case ValType::V128:
      cell_.v128_ = val.v128();
      return;
    case ValType::Rtt:
    case ValType::Ref:
      cell_.ref_ = val.ref();
      return;
  }
  MOZ_CRASH();
}

bool Val::fromJSValue(JSContext* cx, ValType targetType, HandleValue val,
                      MutableHandleVal rval) {
  rval.get().type_ = targetType;
  // No pre/post barrier needed as rval is rooted
  return ToWebAssemblyValue(cx, val, targetType, &rval.get().cell_,
                            targetType.size() == 8);
}

bool Val::toJSValue(JSContext* cx, MutableHandleValue rval) const {
  return ToJSValue(cx, &cell_, type_, rval);
}

void Val::trace(JSTracer* trc) const {
  if (isJSObject()) {
    // TODO/AnyRef-boxing: With boxed immediates and strings, the write
    // barrier is going to have to be more complicated.
    ASSERT_ANYREF_IS_JSOBJECT;
    TraceManuallyBarrieredEdge(trc, asJSObjectAddress(), "wasm val");
  }
}

bool wasm::CheckRefType(JSContext* cx, RefType targetType, HandleValue v,
                        MutableHandleFunction fnval,
                        MutableHandleAnyRef refval) {
  if (!targetType.isNullable() && v.isNull()) {
    JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                             JSMSG_WASM_BAD_REF_NONNULLABLE_VALUE);
    return false;
  }
  switch (targetType.kind()) {
    case RefType::Func:
      if (!CheckFuncRefValue(cx, v, fnval)) {
        return false;
      }
      break;
    case RefType::Extern:
      if (!BoxAnyRef(cx, v, refval)) {
        return false;
      }
      break;
    case RefType::Eq:
      if (!CheckEqRefValue(cx, v, refval)) {
        return false;
      }
      break;
    case RefType::TypeIndex:
      MOZ_CRASH("temporarily unsupported Ref type");
  }
  return true;
}

bool wasm::CheckFuncRefValue(JSContext* cx, HandleValue v,
                             MutableHandleFunction fun) {
  if (v.isNull()) {
    MOZ_ASSERT(!fun);
    return true;
  }

  if (v.isObject()) {
    JSObject& obj = v.toObject();
    if (obj.is<JSFunction>()) {
      JSFunction* f = &obj.as<JSFunction>();
      if (IsWasmExportedFunction(f)) {
        fun.set(f);
        return true;
      }
    }
  }

  JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                           JSMSG_WASM_BAD_FUNCREF_VALUE);
  return false;
}

bool wasm::CheckEqRefValue(JSContext* cx, HandleValue v,
                           MutableHandleAnyRef vp) {
  if (v.isNull()) {
    vp.set(AnyRef::null());
    return true;
  }

  if (v.isObject()) {
    JSObject& obj = v.toObject();
    if (obj.is<TypedObject>()) {
      vp.set(AnyRef::fromJSObject(&obj.as<TypedObject>()));
      return true;
    }
  }

  JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                           JSMSG_WASM_BAD_EQREF_VALUE);
  return false;
}

class wasm::NoDebug {
 public:
  template <typename T>
  static void print(T v) {}
};

class wasm::DebugCodegenVal {
  template <typename T>
  static void print(const char* fmt, T v) {
    DebugCodegen(DebugChannel::Function, fmt, v);
  }

 public:
  static void print(int32_t v) { print(" i32(%d)", v); }
  static void print(int64_t v) { print(" i64(%" PRId64 ")", v); }
  static void print(float v) { print(" f32(%f)", v); }
  static void print(double v) { print(" f64(%lf)", v); }
  static void print(void* v) { print(" ptr(%p)", v); }
};

template bool wasm::ToWebAssemblyValue<NoDebug>(JSContext* cx, HandleValue val,
                                                FieldType type, void* loc,
                                                bool mustWrite64);
template bool wasm::ToWebAssemblyValue<DebugCodegenVal>(JSContext* cx,
                                                        HandleValue val,
                                                        FieldType type,
                                                        void* loc,
                                                        bool mustWrite64);
template bool wasm::ToJSValue<NoDebug>(JSContext* cx, const void* src,
                                       FieldType type, MutableHandleValue dst);
template bool wasm::ToJSValue<DebugCodegenVal>(JSContext* cx, const void* src,
                                               FieldType type,
                                               MutableHandleValue dst);

template bool wasm::ToWebAssemblyValue<NoDebug>(JSContext* cx, HandleValue val,
                                                ValType type, void* loc,
                                                bool mustWrite64);
template bool wasm::ToWebAssemblyValue<DebugCodegenVal>(JSContext* cx,
                                                        HandleValue val,
                                                        ValType type, void* loc,
                                                        bool mustWrite64);
template bool wasm::ToJSValue<NoDebug>(JSContext* cx, const void* src,
                                       ValType type, MutableHandleValue dst);
template bool wasm::ToJSValue<DebugCodegenVal>(JSContext* cx, const void* src,
                                               ValType type,
                                               MutableHandleValue dst);

template <typename Debug = NoDebug>
bool ToWebAssemblyValue_i8(JSContext* cx, HandleValue val, int8_t* loc) {
  bool ok = ToInt8(cx, val, loc);
  Debug::print(*loc);
  return ok;
}
template <typename Debug = NoDebug>
bool ToWebAssemblyValue_i16(JSContext* cx, HandleValue val, int16_t* loc) {
  bool ok = ToInt16(cx, val, loc);
  Debug::print(*loc);
  return ok;
}
template <typename Debug = NoDebug>
bool ToWebAssemblyValue_i32(JSContext* cx, HandleValue val, int32_t* loc,
                            bool mustWrite64) {
  bool ok = ToInt32(cx, val, loc);
  if (ok && mustWrite64) {
#if defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    loc[1] = loc[0] >> 31;
#else
    loc[1] = 0;
#endif
  }
  Debug::print(*loc);
  return ok;
}
template <typename Debug = NoDebug>
bool ToWebAssemblyValue_i64(JSContext* cx, HandleValue val, int64_t* loc,
                            bool mustWrite64) {
  MOZ_ASSERT(mustWrite64);
  JS_TRY_VAR_OR_RETURN_FALSE(cx, *loc, ToBigInt64(cx, val));
  Debug::print(*loc);
  return true;
}
template <typename Debug = NoDebug>
bool ToWebAssemblyValue_f32(JSContext* cx, HandleValue val, float* loc,
                            bool mustWrite64) {
  bool ok = RoundFloat32(cx, val, loc);
  if (ok && mustWrite64) {
    loc[1] = 0.0;
  }
  Debug::print(*loc);
  return ok;
}
template <typename Debug = NoDebug>
bool ToWebAssemblyValue_f64(JSContext* cx, HandleValue val, double* loc,
                            bool mustWrite64) {
  MOZ_ASSERT(mustWrite64);
  bool ok = ToNumber(cx, val, loc);
  Debug::print(*loc);
  return ok;
}
template <typename Debug = NoDebug>
bool ToWebAssemblyValue_externref(JSContext* cx, HandleValue val, void** loc,
                                  bool mustWrite64) {
  RootedAnyRef result(cx, AnyRef::null());
  if (!BoxAnyRef(cx, val, &result)) {
    return false;
  }
  *loc = result.get().forCompiledCode();
#ifndef JS_64BIT
  if (mustWrite64) {
    loc[1] = nullptr;
  }
#endif
  Debug::print(*loc);
  return true;
}
template <typename Debug = NoDebug>
bool ToWebAssemblyValue_eqref(JSContext* cx, HandleValue val, void** loc,
                              bool mustWrite64) {
  RootedAnyRef result(cx, AnyRef::null());
  if (!CheckEqRefValue(cx, val, &result)) {
    return false;
  }
  *loc = result.get().forCompiledCode();
#ifndef JS_64BIT
  if (mustWrite64) {
    loc[1] = nullptr;
  }
#endif
  Debug::print(*loc);
  return true;
}
template <typename Debug = NoDebug>
bool ToWebAssemblyValue_funcref(JSContext* cx, HandleValue val, void** loc,
                                bool mustWrite64) {
  RootedFunction fun(cx);
  if (!CheckFuncRefValue(cx, val, &fun)) {
    return false;
  }
  *loc = fun;
#ifndef JS_64BIT
  if (mustWrite64) {
    loc[1] = nullptr;
  }
#endif
  Debug::print(*loc);
  return true;
}

template <typename Debug>
bool wasm::ToWebAssemblyValue(JSContext* cx, HandleValue val, FieldType type,
                              void* loc, bool mustWrite64) {
  switch (type.kind()) {
    case FieldType::I8:
      return ToWebAssemblyValue_i8<Debug>(cx, val, (int8_t*)loc);
    case FieldType::I16:
      return ToWebAssemblyValue_i16<Debug>(cx, val, (int16_t*)loc);
    case FieldType::I32:
      return ToWebAssemblyValue_i32<Debug>(cx, val, (int32_t*)loc, mustWrite64);
    case FieldType::I64:
      return ToWebAssemblyValue_i64<Debug>(cx, val, (int64_t*)loc, mustWrite64);
    case FieldType::F32:
      return ToWebAssemblyValue_f32<Debug>(cx, val, (float*)loc, mustWrite64);
    case FieldType::F64:
      return ToWebAssemblyValue_f64<Debug>(cx, val, (double*)loc, mustWrite64);
    case FieldType::V128:
      break;
    case FieldType::Rtt:
      break;
    case FieldType::Ref:
#ifdef ENABLE_WASM_FUNCTION_REFERENCES
      if (!type.isNullable() && val.isNull()) {
        JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                                 JSMSG_WASM_BAD_REF_NONNULLABLE_VALUE);
        return false;
      }
#else
      MOZ_ASSERT(type.isNullable());
#endif
      switch (type.refTypeKind()) {
        case RefType::Func:
          return ToWebAssemblyValue_funcref<Debug>(cx, val, (void**)loc,
                                                   mustWrite64);
        case RefType::Extern:
          return ToWebAssemblyValue_externref<Debug>(cx, val, (void**)loc,
                                                     mustWrite64);
        case RefType::Eq:
          return ToWebAssemblyValue_eqref<Debug>(cx, val, (void**)loc,
                                                 mustWrite64);
        case RefType::TypeIndex:
          break;
      }
  }
  MOZ_ASSERT(!type.isExposable());
  JS_ReportErrorNumberUTF8(cx, GetErrorMessage, nullptr,
                           JSMSG_WASM_BAD_VAL_TYPE);
  return false;
}
template <typename Debug>
bool wasm::ToWebAssemblyValue(JSContext* cx, HandleValue val, ValType type,
                              void* loc, bool mustWrite64) {
  return wasm::ToWebAssemblyValue(cx, val, FieldType(type.packed()), loc,
                                  mustWrite64);
}

template <typename Debug = NoDebug>
bool ToJSValue_i8(JSContext* cx, int8_t src, MutableHandleValue dst) {
  dst.set(Int32Value(src));
  Debug::print(src);
  return true;
}
template <typename Debug = NoDebug>
bool ToJSValue_i16(JSContext* cx, int16_t src, MutableHandleValue dst) {
  dst.set(Int32Value(src));
  Debug::print(src);
  return true;
}
template <typename Debug = NoDebug>
bool ToJSValue_i32(JSContext* cx, int32_t src, MutableHandleValue dst) {
  dst.set(Int32Value(src));
  Debug::print(src);
  return true;
}
template <typename Debug = NoDebug>
bool ToJSValue_i64(JSContext* cx, int64_t src, MutableHandleValue dst) {
  // If bi is manipulated other than test & storing, it would need
  // to be rooted here.
  BigInt* bi = BigInt::createFromInt64(cx, src);
  if (!bi) {
    return false;
  }
  dst.set(BigIntValue(bi));
  Debug::print(src);
  return true;
}
template <typename Debug = NoDebug>
bool ToJSValue_f32(JSContext* cx, float src, MutableHandleValue dst) {
  dst.set(JS::CanonicalizedDoubleValue(src));
  Debug::print(src);
  return true;
}
template <typename Debug = NoDebug>
bool ToJSValue_f64(JSContext* cx, double src, MutableHandleValue dst) {
  dst.set(JS::CanonicalizedDoubleValue(src));
  Debug::print(src);
  return true;
}
template <typename Debug = NoDebug>
bool ToJSValue_funcref(JSContext* cx, void* src, MutableHandleValue dst) {
  dst.set(UnboxFuncRef(FuncRef::fromCompiledCode(src)));
  Debug::print(src);
  return true;
}
template <typename Debug = NoDebug>
bool ToJSValue_anyref(JSContext* cx, void* src, MutableHandleValue dst) {
  dst.set(UnboxAnyRef(AnyRef::fromCompiledCode(src)));
  Debug::print(src);
  return true;
}

template <typename Debug>
bool wasm::ToJSValue(JSContext* cx, const void* src, FieldType type,
                     MutableHandleValue dst) {
  switch (type.kind()) {
    case FieldType::I8:
      return ToJSValue_i8<Debug>(cx, *reinterpret_cast<const int8_t*>(src),
                                 dst);
    case FieldType::I16:
      return ToJSValue_i16<Debug>(cx, *reinterpret_cast<const int16_t*>(src),
                                  dst);
    case FieldType::I32:
      return ToJSValue_i32<Debug>(cx, *reinterpret_cast<const int32_t*>(src),
                                  dst);
    case FieldType::I64:
      return ToJSValue_i64<Debug>(cx, *reinterpret_cast<const int64_t*>(src),
                                  dst);
    case FieldType::F32:
      return ToJSValue_f32<Debug>(cx, *reinterpret_cast<const float*>(src),
                                  dst);
    case FieldType::F64:
      return ToJSValue_f64<Debug>(cx, *reinterpret_cast<const double*>(src),
                                  dst);
    case FieldType::V128:
      break;
    case FieldType::Rtt:
      break;
    case FieldType::Ref:
      switch (type.refTypeKind()) {
        case RefType::Func:
          return ToJSValue_funcref<Debug>(
              cx, *reinterpret_cast<void* const*>(src), dst);
        case RefType::Extern:
          return ToJSValue_anyref<Debug>(
              cx, *reinterpret_cast<void* const*>(src), dst);
        case RefType::Eq:
          return ToJSValue_anyref<Debug>(
              cx, *reinterpret_cast<void* const*>(src), dst);
        case RefType::TypeIndex:
          break;
      }
  }
  MOZ_ASSERT(!type.isExposable());
  Debug::print(nullptr);
  dst.setUndefined();
  return true;
}
template <typename Debug>
bool wasm::ToJSValue(JSContext* cx, const void* src, ValType type,
                     MutableHandleValue dst) {
  return wasm::ToJSValue(cx, src, FieldType(type.packed()), dst);
}

void AnyRef::trace(JSTracer* trc) {
  if (value_) {
    TraceManuallyBarrieredEdge(trc, &value_, "wasm anyref referent");
  }
}

const JSClass WasmValueBox::class_ = {
    "WasmValueBox", JSCLASS_HAS_RESERVED_SLOTS(RESERVED_SLOTS)};

WasmValueBox* WasmValueBox::create(JSContext* cx, HandleValue val) {
  WasmValueBox* obj = NewObjectWithGivenProto<WasmValueBox>(cx, nullptr);
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

/* static */
wasm::FuncRef wasm::FuncRef::fromAnyRefUnchecked(AnyRef p) {
#ifdef DEBUG
  Value v = UnboxAnyRef(p);
  if (v.isNull()) {
    return FuncRef(nullptr);
  }
  if (v.toObject().is<JSFunction>()) {
    return FuncRef(&v.toObject().as<JSFunction>());
  }
  MOZ_CRASH("Bad value");
#else
  return FuncRef(&p.asJSObject()->as<JSFunction>());
#endif
}

Value wasm::UnboxFuncRef(FuncRef val) {
  JSFunction* fn = val.asJSFunction();
  Value result;
  MOZ_ASSERT_IF(fn, fn->is<JSFunction>());
  result.setObjectOrNull(fn);
  return result;
}

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
    case ValType::V128:
      return true;
    case ValType::Ref:
      switch (vt.refTypeKind()) {
        case RefType::Func:
        case RefType::Extern:
        case RefType::Eq:
          return true;
        case RefType::TypeIndex:
          return false;
      }
      break;
    case ValType::Rtt:
      return false;
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
    case ValType::V128:
      return 4;
    case ValType::Ref:
      switch (vt.refTypeKind()) {
        case RefType::Func:
          return 5;
        case RefType::Extern:
          return 6;
        case RefType::Eq:
          return 7;
        case RefType::TypeIndex:
          break;
      }
      break;
    case ValType::Rtt:
      break;
  }
  MOZ_CRASH("bad ValType");
}

/* static */
bool TypeIdDesc::isGlobal(const TypeDef& type) {
  if (!type.isFuncType()) {
    return true;
  }
  const FuncType& funcType = type.funcType();
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
TypeIdDesc TypeIdDesc::global(const TypeDef& type, uint32_t globalDataOffset) {
  MOZ_ASSERT(isGlobal(type));
  return TypeIdDesc(TypeIdDescKind::Global, globalDataOffset);
}

static ImmediateType LengthToBits(uint32_t length) {
  static_assert(sMaxTypes <= ((1 << sLengthBits) - 1), "fits");
  MOZ_ASSERT(length <= sMaxTypes);
  return length;
}

/* static */
TypeIdDesc TypeIdDesc::immediate(const TypeDef& type) {
  const FuncType& funcType = type.funcType();

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
  return TypeIdDesc(TypeIdDescKind::Immediate, immediate);
}

size_t TypeDef::serializedSize() const {
  size_t size = sizeof(kind_);
  switch (kind_) {
    case TypeDefKind::Struct: {
      size += sizeof(structType_);
      break;
    }
    case TypeDefKind::Func: {
      size += sizeof(funcType_);
      break;
    }
    case TypeDefKind::None: {
      break;
    }
    default:
      MOZ_ASSERT_UNREACHABLE();
  }
  return size;
}

uint8_t* TypeDef::serialize(uint8_t* cursor) const {
  cursor = WriteBytes(cursor, &kind_, sizeof(kind_));
  switch (kind_) {
    case TypeDefKind::Struct: {
      cursor = structType_.serialize(cursor);
      break;
    }
    case TypeDefKind::Func: {
      cursor = funcType_.serialize(cursor);
      break;
    }
    case TypeDefKind::None: {
      break;
    }
    default:
      MOZ_ASSERT_UNREACHABLE();
  }
  return cursor;
}

const uint8_t* TypeDef::deserialize(const uint8_t* cursor) {
  cursor = ReadBytes(cursor, &kind_, sizeof(kind_));
  switch (kind_) {
    case TypeDefKind::Struct: {
      cursor = structType_.deserialize(cursor);
      break;
    }
    case TypeDefKind::Func: {
      cursor = funcType_.deserialize(cursor);
      break;
    }
    case TypeDefKind::None: {
      break;
    }
    default:
      MOZ_ASSERT_UNREACHABLE();
  }
  return cursor;
}

size_t TypeDef::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const {
  switch (kind_) {
    case TypeDefKind::Struct: {
      return structType_.sizeOfExcludingThis(mallocSizeOf);
    }
    case TypeDefKind::Func: {
      return funcType_.sizeOfExcludingThis(mallocSizeOf);
    }
    case TypeDefKind::None: {
      return 0;
    }
    default:
      break;
  }
  MOZ_ASSERT_UNREACHABLE();
  return 0;
}

size_t TypeDefWithId::serializedSize() const {
  return TypeDef::serializedSize() + sizeof(TypeIdDesc);
}

uint8_t* TypeDefWithId::serialize(uint8_t* cursor) const {
  cursor = TypeDef::serialize(cursor);
  cursor = WriteBytes(cursor, &id, sizeof(id));
  return cursor;
}

const uint8_t* TypeDefWithId::deserialize(const uint8_t* cursor) {
  cursor = TypeDef::deserialize(cursor);
  cursor = ReadBytes(cursor, &id, sizeof(id));
  return cursor;
}

size_t TypeDefWithId::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const {
  return TypeDef::sizeOfExcludingThis(mallocSizeOf);
}

ArgTypeVector::ArgTypeVector(const FuncType& funcType)
    : args_(funcType.args()),
      hasStackResults_(ABIResultIter::HasStackResults(
          ResultType::Vector(funcType.results()))) {}

static inline CheckedInt32 RoundUpToAlignment(CheckedInt32 address,
                                              uint32_t align) {
  MOZ_ASSERT(IsPowerOfTwo(align));

  // Note: Be careful to order operators such that we first make the
  // value smaller and then larger, so that we don't get false
  // overflow errors due to (e.g.) adding `align` and then
  // subtracting `1` afterwards when merely adding `align-1` would
  // not have overflowed. Note that due to the nature of two's
  // complement representation, if `address` is already aligned,
  // then adding `align-1` cannot itself cause an overflow.

  return ((address + (align - 1)) / align) * align;
}

class StructLayout {
  CheckedInt32 sizeSoFar = 0;
  uint32_t structAlignment = 1;

 public:
  // The field adders return the offset of the the field.
  CheckedInt32 addField(FieldType type) {
    uint32_t fieldSize = type.size();
    uint32_t fieldAlignment = type.alignmentInStruct();

    // Alignment of the struct is the max of the alignment of its fields.
    structAlignment = std::max(structAlignment, fieldAlignment);

    // Align the pointer.
    CheckedInt32 offset = RoundUpToAlignment(sizeSoFar, fieldAlignment);
    if (!offset.isValid()) {
      return offset;
    }

    // Allocate space.
    sizeSoFar = offset + fieldSize;
    if (!sizeSoFar.isValid()) {
      return sizeSoFar;
    }

    return offset;
  }

  // The close method rounds up the structure size to the appropriate
  // alignment and returns that size.
  CheckedInt32 close() {
    return RoundUpToAlignment(sizeSoFar, structAlignment);
  }
};

bool StructType::computeLayout() {
  StructLayout layout;
  for (StructField& field : fields_) {
    CheckedInt32 offset = layout.addField(field.type);
    if (!offset.isValid()) {
      return false;
    }
    field.offset = offset.value();
  }

  CheckedInt32 size = layout.close();
  if (!size.isValid()) {
    return false;
  }
  size_ = size.value();

  return true;
}

size_t StructType::serializedSize() const {
  return SerializedPodVectorSize(fields_) + sizeof(size_);
}

uint8_t* StructType::serialize(uint8_t* cursor) const {
  cursor = SerializePodVector(cursor, fields_);
  cursor = WriteBytes(cursor, &size_, sizeof(size_));
  return cursor;
}

const uint8_t* StructType::deserialize(const uint8_t* cursor) {
  (cursor = DeserializePodVector(cursor, &fields_)) &&
      (cursor = ReadBytes(cursor, &size_, sizeof(size_)));
  return cursor;
}

size_t StructType::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const {
  return fields_.sizeOfExcludingThis(mallocSizeOf);
}

TypeResult TypeContext::isRefEquivalent(RefType one, RefType two,
                                        TypeCache* cache) const {
  // Anything's equal to itself.
  if (one == two) {
    return TypeResult::True;
  }

#ifdef ENABLE_WASM_FUNCTION_REFERENCES
  if (features_.functionReferences) {
    // Two references must have the same nullability to be equal
    if (one.isNullable() != two.isNullable()) {
      return TypeResult::False;
    }

    // Non type-index references are equal if they have the same kind
    if (!one.isTypeIndex() && !two.isTypeIndex() && one.kind() == two.kind()) {
      return TypeResult::True;
    }

    // Type-index references can be equal
    if (one.isTypeIndex() && two.isTypeIndex()) {
      return isTypeIndexEquivalent(one.typeIndex(), two.typeIndex(), cache);
    }
  }
#endif
  return TypeResult::False;
}

#ifdef ENABLE_WASM_FUNCTION_REFERENCES
TypeResult TypeContext::isTypeIndexEquivalent(uint32_t one, uint32_t two,
                                              TypeCache* cache) const {
  MOZ_ASSERT(features_.functionReferences);

  // Anything's equal to itself.
  if (one == two) {
    return TypeResult::True;
  }

#  ifdef ENABLE_WASM_GC
  if (features_.gcTypes) {
    // A struct may be equal to a struct
    if (isStructType(one) && isStructType(two)) {
      return isStructEquivalent(one, two, cache);
    }

    // An array may be equal to an array
    if (isArrayType(one) && isArrayType(two)) {
      return isArrayEquivalent(one, two, cache);
    }
  }
#  endif

  return TypeResult::False;
}
#endif

#ifdef ENABLE_WASM_GC
TypeResult TypeContext::isStructEquivalent(uint32_t oneIndex, uint32_t twoIndex,
                                           TypeCache* cache) const {
  if (cache->isEquivalent(oneIndex, twoIndex)) {
    return TypeResult::True;
  }

  const StructType& one = structType(oneIndex);
  const StructType& two = structType(twoIndex);

  // Structs must have the same number of fields to be equal
  if (one.fields_.length() != two.fields_.length()) {
    return TypeResult::False;
  }

  // Assume these structs are equal while checking fields. If any field is
  // not equal then we remove the assumption.
  if (!cache->markEquivalent(oneIndex, twoIndex)) {
    return TypeResult::OOM;
  }

  for (uint32_t i = 0; i < two.fields_.length(); i++) {
    TypeResult result =
        isStructFieldEquivalent(one.fields_[i], two.fields_[i], cache);
    if (result != TypeResult::True) {
      cache->unmarkEquivalent(oneIndex, twoIndex);
      return result;
    }
  }
  return TypeResult::True;
}

TypeResult TypeContext::isStructFieldEquivalent(const StructField one,
                                                const StructField two,
                                                TypeCache* cache) const {
  // Struct fields must share the same mutability to equal
  if (one.isMutable != two.isMutable) {
    return TypeResult::False;
  }
  // Struct field types must be equal
  return isEquivalent(one.type, two.type, cache);
}

TypeResult TypeContext::isArrayEquivalent(uint32_t oneIndex, uint32_t twoIndex,
                                          TypeCache* cache) const {
  if (cache->isEquivalent(oneIndex, twoIndex)) {
    return TypeResult::True;
  }

  const ArrayType& one = arrayType(oneIndex);
  const ArrayType& two = arrayType(twoIndex);

  // Assume these arrays are equal while checking fields. If the array
  // element is not equal then we remove the assumption.
  if (!cache->markEquivalent(oneIndex, twoIndex)) {
    return TypeResult::OOM;
  }

  TypeResult result = isArrayElementEquivalent(one, two, cache);
  if (result != TypeResult::True) {
    cache->unmarkEquivalent(oneIndex, twoIndex);
  }
  return result;
}

TypeResult TypeContext::isArrayElementEquivalent(const ArrayType& one,
                                                 const ArrayType& two,
                                                 TypeCache* cache) const {
  // Array elements must share the same mutability to be equal
  if (one.isMutable_ != two.isMutable_) {
    return TypeResult::False;
  }
  // Array elements must be equal
  return isEquivalent(one.elementType_, two.elementType_, cache);
}
#endif

TypeResult TypeContext::isRefSubtypeOf(RefType one, RefType two,
                                       TypeCache* cache) const {
  // Anything's a subtype of itself.
  if (one == two) {
    return TypeResult::True;
  }

#ifdef ENABLE_WASM_FUNCTION_REFERENCES
  if (features_.functionReferences) {
    // A subtype must have the same nullability as the supertype or the
    // supertype must be nullable.
    if (!(one.isNullable() == two.isNullable() || two.isNullable())) {
      return TypeResult::False;
    }

    // Non type-index references are subtypes if they have the same kind
    if (!one.isTypeIndex() && !two.isTypeIndex() && one.kind() == two.kind()) {
      return TypeResult::True;
    }

    // Structs are subtypes of eqref
    if (isStructType(one) && two.isEq()) {
      return TypeResult::True;
    }

    // Arrays are subtypes of eqref
    if (isArrayType(one) && two.isEq()) {
      return TypeResult::True;
    }

    // Type-index references can be subtypes
    if (one.isTypeIndex() && two.isTypeIndex()) {
      return isTypeIndexSubtypeOf(one.typeIndex(), two.typeIndex(), cache);
    }
  }
#endif
  return TypeResult::False;
}

#ifdef ENABLE_WASM_FUNCTION_REFERENCES
TypeResult TypeContext::isTypeIndexSubtypeOf(uint32_t one, uint32_t two,
                                             TypeCache* cache) const {
  MOZ_ASSERT(features_.functionReferences);

  // Anything's a subtype of itself.
  if (one == two) {
    return TypeResult::True;
  }

#  ifdef ENABLE_WASM_GC
  if (features_.gcTypes) {
    // Structs may be subtypes of structs
    if (isStructType(one) && isStructType(two)) {
      return isStructSubtypeOf(one, two, cache);
    }

    // Arrays may be subtypes of arrays
    if (isArrayType(one) && isArrayType(two)) {
      return isArraySubtypeOf(one, two, cache);
    }
  }
#  endif
  return TypeResult::False;
}
#endif

#ifdef ENABLE_WASM_GC
TypeResult TypeContext::isStructSubtypeOf(uint32_t oneIndex, uint32_t twoIndex,
                                          TypeCache* cache) const {
  if (cache->isSubtypeOf(oneIndex, twoIndex)) {
    return TypeResult::True;
  }

  const StructType& one = structType(oneIndex);
  const StructType& two = structType(twoIndex);

  // A subtype must have at least as many fields as its supertype
  if (one.fields_.length() < two.fields_.length()) {
    return TypeResult::False;
  }

  // Assume these structs are subtypes while checking fields. If any field
  // fails a check then we remove the assumption.
  if (!cache->markSubtypeOf(oneIndex, twoIndex)) {
    return TypeResult::OOM;
  }

  for (uint32_t i = 0; i < two.fields_.length(); i++) {
    TypeResult result =
        isStructFieldSubtypeOf(one.fields_[i], two.fields_[i], cache);
    if (result != TypeResult::True) {
      cache->unmarkSubtypeOf(oneIndex, twoIndex);
      return result;
    }
  }
  return TypeResult::True;
}

TypeResult TypeContext::isStructFieldSubtypeOf(const StructField one,
                                               const StructField two,
                                               TypeCache* cache) const {
  // Mutable fields are invariant w.r.t. field types
  if (one.isMutable && two.isMutable) {
    return isEquivalent(one.type, two.type, cache);
  }
  // Immutable fields are covariant w.r.t. field types
  if (!one.isMutable && !two.isMutable) {
    return isSubtypeOf(one.type, two.type, cache);
  }
  return TypeResult::False;
}

TypeResult TypeContext::isArraySubtypeOf(uint32_t oneIndex, uint32_t twoIndex,
                                         TypeCache* cache) const {
  if (cache->isSubtypeOf(oneIndex, twoIndex)) {
    return TypeResult::True;
  }

  const ArrayType& one = arrayType(oneIndex);
  const ArrayType& two = arrayType(twoIndex);

  // Assume these arrays are subtypes while checking elements. If the elements
  // fail the check then we remove the assumption.
  if (!cache->markSubtypeOf(oneIndex, twoIndex)) {
    return TypeResult::OOM;
  }

  TypeResult result = isArrayElementSubtypeOf(one, two, cache);
  if (result != TypeResult::True) {
    cache->unmarkSubtypeOf(oneIndex, twoIndex);
  }
  return result;
}

TypeResult TypeContext::isArrayElementSubtypeOf(const ArrayType& one,
                                                const ArrayType& two,
                                                TypeCache* cache) const {
  // Mutable elements are invariant w.r.t. field types
  if (one.isMutable_ && two.isMutable_) {
    return isEquivalent(one.elementType_, two.elementType_, cache);
  }
  // Immutable elements are covariant w.r.t. field types
  if (!one.isMutable_ && !two.isMutable_) {
    return isSubtypeOf(one.elementType_, two.elementType_, cache);
  }
  return TypeResult::False;
}
#endif

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
uint32_t Export::eventIndex() const {
  MOZ_ASSERT(pod.kind_ == DefinitionKind::Event);
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

size_t ElemSegment::serializedSize() const {
  return sizeof(kind) + sizeof(tableIndex) + sizeof(elemType) +
         sizeof(offsetIfActive) + SerializedPodVectorSize(elemFuncIndices);
}

uint8_t* ElemSegment::serialize(uint8_t* cursor) const {
  cursor = WriteBytes(cursor, &kind, sizeof(kind));
  cursor = WriteBytes(cursor, &tableIndex, sizeof(tableIndex));
  cursor = WriteBytes(cursor, &elemType, sizeof(elemType));
  cursor = WriteBytes(cursor, &offsetIfActive, sizeof(offsetIfActive));
  cursor = SerializePodVector(cursor, elemFuncIndices);
  return cursor;
}

const uint8_t* ElemSegment::deserialize(const uint8_t* cursor) {
  (cursor = ReadBytes(cursor, &kind, sizeof(kind))) &&
      (cursor = ReadBytes(cursor, &tableIndex, sizeof(tableIndex))) &&
      (cursor = ReadBytes(cursor, &elemType, sizeof(elemType))) &&
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

uint64_t wasm::RoundUpToNextValidARMImmediate(uint64_t i) {
  MOZ_ASSERT(i <= HighestValidARMImmediate);
  static_assert(HighestValidARMImmediate == 0xff000000,
                "algorithm relies on specific constant");

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

size_t wasm::ComputeMappedSize(uint64_t maxSize) {
  MOZ_ASSERT(maxSize % PageSize == 0);

  // It is the bounds-check limit, not the mapped size, that gets baked into
  // code. Thus round up the maxSize to the next valid immediate value
  // *before* adding in the guard page.

#ifdef JS_CODEGEN_ARM
  uint64_t boundsCheckLimit = RoundUpToNextValidARMImmediate(maxSize);
#else
  uint64_t boundsCheckLimit = maxSize;
#endif
  MOZ_ASSERT(IsValidBoundsCheckImmediate(boundsCheckLimit));

  MOZ_ASSERT(boundsCheckLimit % gc::SystemPageSize() == 0);
  MOZ_ASSERT(GuardSize % gc::SystemPageSize() == 0);
  return boundsCheckLimit + GuardSize;
}

/* static */
DebugFrame* DebugFrame::from(Frame* fp) {
  MOZ_ASSERT(
      GetNearestEffectiveTls(fp)->instance->code().metadata().debugEnabled);
  auto* df =
      reinterpret_cast<DebugFrame*>((uint8_t*)fp - DebugFrame::offsetOfFrame());
  MOZ_ASSERT(GetNearestEffectiveTls(fp)->instance == df->instance());
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

Instance* DebugFrame::instance() const {
  return GetNearestEffectiveTls(&frame_)->instance;
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
#ifdef ENABLE_WASM_SIMD
    case jit::MIRType::Simd128:
      vp.set(NumberValue(0));
      break;
#endif
    default:
      MOZ_CRASH("local type");
  }
  return true;
}

bool DebugFrame::updateReturnJSValue(JSContext* cx) {
  MutableHandleValue rval =
      MutableHandleValue::fromMarkedLocation(&cachedReturnJSValue_);
  rval.setUndefined();
  flags_.hasCachedReturnJSValue = true;
  ResultType resultType = instance()->debug().debugGetResultType(funcIndex());
  Maybe<char*> stackResultsLoc;
  if (ABIResultIter::HasStackResults(resultType)) {
    stackResultsLoc = Some(static_cast<char*>(stackResultsPointer_));
  }
  DebugCodegen(DebugChannel::Function,
               "wasm-function[%d] updateReturnJSValue [", funcIndex());
  bool ok =
      ResultsToJSValue(cx, resultType, registerResults_, stackResultsLoc, rval);
  DebugCodegen(DebugChannel::Function, "]\n");
  return ok;
}

HandleValue DebugFrame::returnValue() const {
  MOZ_ASSERT(flags_.hasCachedReturnJSValue);
  return HandleValue::fromMarkedLocation(&cachedReturnJSValue_);
}

void DebugFrame::clearReturnJSValue() {
  flags_.hasCachedReturnJSValue = true;
  cachedReturnJSValue_.setUndefined();
}

void DebugFrame::observe(JSContext* cx) {
  if (!flags_.observing) {
    instance()->debug().adjustEnterAndLeaveFrameTrapsState(
        cx, /* enabled = */ true);
    flags_.observing = true;
  }
}

void DebugFrame::leave(JSContext* cx) {
  if (flags_.observing) {
    instance()->debug().adjustEnterAndLeaveFrameTrapsState(
        cx, /* enabled = */ false);
    flags_.observing = false;
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

UniqueChars wasm::ToString(ValType type) {
  const char* literal = nullptr;
  switch (type.kind()) {
    case ValType::I32:
      literal = "i32";
      break;
    case ValType::I64:
      literal = "i64";
      break;
    case ValType::V128:
      literal = "v128";
      break;
    case ValType::F32:
      literal = "f32";
      break;
    case ValType::F64:
      literal = "f64";
      break;
    case ValType::Ref:
      if (type.isNullable() && !type.isTypeIndex()) {
        switch (type.refTypeKind()) {
          case RefType::Func:
            literal = "funcref";
            break;
          case RefType::Extern:
            literal = "externref";
            break;
          case RefType::Eq:
            literal = "eqref";
            break;
          case RefType::TypeIndex:
            MOZ_ASSERT_UNREACHABLE();
        }
      } else {
        const char* heapType = nullptr;
        switch (type.refTypeKind()) {
          case RefType::Func:
            heapType = "func";
            break;
          case RefType::Extern:
            heapType = "extern";
            break;
          case RefType::Eq:
            heapType = "eq";
            break;
          case RefType::TypeIndex:
            return JS_smprintf("(ref %s%d)", type.isNullable() ? "null " : "",
                               type.refType().typeIndex());
        }
        return JS_smprintf("(ref %s%s)", type.isNullable() ? "null " : "",
                           heapType);
      }
      break;
    case ValType::Rtt:
      return JS_smprintf("(rtt %d %d)", type.rttDepth(), type.typeIndex());
  }
  return JS_smprintf("%s", literal);
}

UniqueChars wasm::ToString(const Maybe<ValType>& type) {
  return type ? ToString(type.ref()) : JS_smprintf("%s", "void");
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

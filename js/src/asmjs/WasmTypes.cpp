/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
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

#include "asmjs/WasmTypes.h"

#include "fdlibm.h"

#include "jslibmath.h"
#include "jsmath.h"

#include "asmjs/WasmInstance.h"
#include "asmjs/WasmSerialize.h"
#include "asmjs/WasmSignalHandlers.h"
#include "jit/MacroAssembler.h"
#include "js/Conversions.h"
#include "vm/Interpreter.h"

#include "vm/Stack-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

void
Val::writePayload(uint8_t* dst) const
{
    switch (type_) {
      case ValType::I32:
      case ValType::F32:
        memcpy(dst, &u.i32_, sizeof(u.i32_));
        return;
      case ValType::I64:
      case ValType::F64:
        memcpy(dst, &u.i64_, sizeof(u.i64_));
        return;
      case ValType::I8x16:
      case ValType::I16x8:
      case ValType::I32x4:
      case ValType::F32x4:
      case ValType::B8x16:
      case ValType::B16x8:
      case ValType::B32x4:
        memcpy(dst, &u, jit::Simd128DataSize);
        return;
      case ValType::Limit:
        MOZ_CRASH("Bad value type");
    }
}

#if defined(JS_CODEGEN_ARM)
extern "C" {

extern MOZ_EXPORT int64_t
__aeabi_idivmod(int, int);

extern MOZ_EXPORT int64_t
__aeabi_uidivmod(int, int);

}
#endif

static void
WasmReportOverRecursed()
{
    ReportOverRecursed(JSRuntime::innermostWasmActivation()->cx(), JSMSG_WASM_OVERRECURSED);
}

static bool
WasmHandleExecutionInterrupt()
{
    WasmActivation* activation = JSRuntime::innermostWasmActivation();
    bool success = CheckForInterrupt(activation->cx());

    // Preserve the invariant that having a non-null resumePC means that we are
    // handling an interrupt.  Note that resumePC has already been copied onto
    // the stack by the interrupt stub, so we can clear it before returning
    // to the stub.
    activation->setResumePC(nullptr);

    return success;
}

static void
HandleTrap(int32_t trapIndex)
{
    JSContext* cx = JSRuntime::innermostWasmActivation()->cx();

    MOZ_ASSERT(trapIndex < int32_t(Trap::Limit) && trapIndex >= 0);
    Trap trap = Trap(trapIndex);

    unsigned errorNumber;
    switch (trap) {
      case Trap::Unreachable:
        errorNumber = JSMSG_WASM_UNREACHABLE;
        break;
      case Trap::IntegerOverflow:
        errorNumber = JSMSG_WASM_INTEGER_OVERFLOW;
        break;
      case Trap::InvalidConversionToInteger:
        errorNumber = JSMSG_WASM_INVALID_CONVERSION;
        break;
      case Trap::IntegerDivideByZero:
        errorNumber = JSMSG_WASM_INT_DIVIDE_BY_ZERO;
        break;
      case Trap::BadIndirectCall:
        errorNumber = JSMSG_WASM_BAD_IND_CALL;
        break;
      case Trap::ImpreciseSimdConversion:
        errorNumber = JSMSG_SIMD_FAILED_CONVERSION;
        break;
      case Trap::OutOfBounds:
        errorNumber = JSMSG_BAD_INDEX;
        break;
      case Trap::UnalignedAccess:
        errorNumber = JSMSG_WASM_UNALIGNED_ACCESS;
        break;
      default:
        MOZ_CRASH("unexpected trap");
    }

    JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, errorNumber);
}

static int32_t
CoerceInPlace_ToInt32(MutableHandleValue val)
{
    JSContext* cx = JSRuntime::innermostWasmActivation()->cx();

    int32_t i32;
    if (!ToInt32(cx, val, &i32))
        return false;
    val.set(Int32Value(i32));

    return true;
}

static int32_t
CoerceInPlace_ToNumber(MutableHandleValue val)
{
    JSContext* cx = JSRuntime::innermostWasmActivation()->cx();

    double dbl;
    if (!ToNumber(cx, val, &dbl))
        return false;
    val.set(DoubleValue(dbl));

    return true;
}

static int64_t
DivI64(uint32_t x_hi, uint32_t x_lo, uint32_t y_hi, uint32_t y_lo)
{
    int64_t x = ((uint64_t)x_hi << 32) + x_lo;
    int64_t y = ((uint64_t)y_hi << 32) + y_lo;
    MOZ_ASSERT(x != INT64_MIN || y != -1);
    MOZ_ASSERT(y != 0);
    return x / y;
}

static int64_t
UDivI64(uint32_t x_hi, uint32_t x_lo, uint32_t y_hi, uint32_t y_lo)
{
    uint64_t x = ((uint64_t)x_hi << 32) + x_lo;
    uint64_t y = ((uint64_t)y_hi << 32) + y_lo;
    MOZ_ASSERT(y != 0);
    return x / y;
}

static int64_t
ModI64(uint32_t x_hi, uint32_t x_lo, uint32_t y_hi, uint32_t y_lo)
{
    int64_t x = ((uint64_t)x_hi << 32) + x_lo;
    int64_t y = ((uint64_t)y_hi << 32) + y_lo;
    MOZ_ASSERT(x != INT64_MIN || y != -1);
    MOZ_ASSERT(y != 0);
    return x % y;
}

static int64_t
UModI64(uint32_t x_hi, uint32_t x_lo, uint32_t y_hi, uint32_t y_lo)
{
    uint64_t x = ((uint64_t)x_hi << 32) + x_lo;
    uint64_t y = ((uint64_t)y_hi << 32) + y_lo;
    MOZ_ASSERT(y != 0);
    return x % y;
}

static int64_t
TruncateDoubleToInt64(double input)
{
    // Note: INT64_MAX is not representable in double. It is actually INT64_MAX + 1.
    // Therefore also sending the failure value.
    if (input >= double(INT64_MAX))
        return 0x8000000000000000;
    if (input < double(INT64_MIN))
        return 0x8000000000000000;
    return int64_t(input);
}

static uint64_t
TruncateDoubleToUint64(double input)
{
    // Note: UINT64_MAX is not representable in double. It is actually UINT64_MAX + 1.
    // Therefore also sending the failure value.
    if (input >= double(UINT64_MAX))
        return 0x8000000000000000;
    if (input <= -1.0)
        return 0x8000000000000000;
    return uint64_t(input);
}

static double
Int64ToFloatingPoint(int32_t x_hi, uint32_t x_lo)
{
    int64_t x = int64_t((uint64_t(x_hi) << 32)) + int64_t(x_lo);
    return double(x);
}

static double
Uint64ToFloatingPoint(int32_t x_hi, uint32_t x_lo)
{
    uint64_t x = (uint64_t(x_hi) << 32) + uint64_t(x_lo);
    return double(x);
}

template <class F>
static inline void*
FuncCast(F* pf, ABIFunctionType type)
{
    void *pv = JS_FUNC_TO_DATA_PTR(void*, pf);
#ifdef JS_SIMULATOR
    pv = Simulator::RedirectNativeFunction(pv, type);
#endif
    return pv;
}

void*
wasm::AddressOf(SymbolicAddress imm, ExclusiveContext* cx)
{
    switch (imm) {
      case SymbolicAddress::Context:
        return cx->contextAddressForJit();
      case SymbolicAddress::InterruptUint32:
        return cx->runtimeAddressOfInterruptUint32();
      case SymbolicAddress::ReportOverRecursed:
        return FuncCast(WasmReportOverRecursed, Args_General0);
      case SymbolicAddress::HandleExecutionInterrupt:
        return FuncCast(WasmHandleExecutionInterrupt, Args_General0);
      case SymbolicAddress::HandleTrap:
        return FuncCast(HandleTrap, Args_General1);
      case SymbolicAddress::CallImport_Void:
        return FuncCast(Instance::callImport_void, Args_General4);
      case SymbolicAddress::CallImport_I32:
        return FuncCast(Instance::callImport_i32, Args_General4);
      case SymbolicAddress::CallImport_I64:
        return FuncCast(Instance::callImport_i64, Args_General4);
      case SymbolicAddress::CallImport_F64:
        return FuncCast(Instance::callImport_f64, Args_General4);
      case SymbolicAddress::CoerceInPlace_ToInt32:
        return FuncCast(CoerceInPlace_ToInt32, Args_General1);
      case SymbolicAddress::CoerceInPlace_ToNumber:
        return FuncCast(CoerceInPlace_ToNumber, Args_General1);
      case SymbolicAddress::ToInt32:
        return FuncCast<int32_t (double)>(JS::ToInt32, Args_Int_Double);
      case SymbolicAddress::DivI64:
        return FuncCast(DivI64, Args_General4);
      case SymbolicAddress::UDivI64:
        return FuncCast(UDivI64, Args_General4);
      case SymbolicAddress::ModI64:
        return FuncCast(ModI64, Args_General4);
      case SymbolicAddress::UModI64:
        return FuncCast(UModI64, Args_General4);
      case SymbolicAddress::TruncateDoubleToUint64:
        return FuncCast(TruncateDoubleToUint64, Args_Int64_Double);
      case SymbolicAddress::TruncateDoubleToInt64:
        return FuncCast(TruncateDoubleToInt64, Args_Int64_Double);
      case SymbolicAddress::Uint64ToFloatingPoint:
        return FuncCast(Uint64ToFloatingPoint, Args_Double_IntInt);
      case SymbolicAddress::Int64ToFloatingPoint:
        return FuncCast(Int64ToFloatingPoint, Args_Double_IntInt);
#if defined(JS_CODEGEN_ARM)
      case SymbolicAddress::aeabi_idivmod:
        return FuncCast(__aeabi_idivmod, Args_General2);
      case SymbolicAddress::aeabi_uidivmod:
        return FuncCast(__aeabi_uidivmod, Args_General2);
      case SymbolicAddress::AtomicCmpXchg:
        return FuncCast(atomics_cmpxchg_asm_callout, Args_General5);
      case SymbolicAddress::AtomicXchg:
        return FuncCast(atomics_xchg_asm_callout, Args_General4);
      case SymbolicAddress::AtomicFetchAdd:
        return FuncCast(atomics_add_asm_callout, Args_General4);
      case SymbolicAddress::AtomicFetchSub:
        return FuncCast(atomics_sub_asm_callout, Args_General4);
      case SymbolicAddress::AtomicFetchAnd:
        return FuncCast(atomics_and_asm_callout, Args_General4);
      case SymbolicAddress::AtomicFetchOr:
        return FuncCast(atomics_or_asm_callout, Args_General4);
      case SymbolicAddress::AtomicFetchXor:
        return FuncCast(atomics_xor_asm_callout, Args_General4);
#endif
      case SymbolicAddress::ModD:
        return FuncCast(NumberMod, Args_Double_DoubleDouble);
      case SymbolicAddress::SinD:
        return FuncCast<double (double)>(sin, Args_Double_Double);
      case SymbolicAddress::CosD:
        return FuncCast<double (double)>(cos, Args_Double_Double);
      case SymbolicAddress::TanD:
        return FuncCast<double (double)>(tan, Args_Double_Double);
      case SymbolicAddress::ASinD:
        return FuncCast<double (double)>(fdlibm::asin, Args_Double_Double);
      case SymbolicAddress::ACosD:
        return FuncCast<double (double)>(fdlibm::acos, Args_Double_Double);
      case SymbolicAddress::ATanD:
        return FuncCast<double (double)>(fdlibm::atan, Args_Double_Double);
      case SymbolicAddress::CeilD:
        return FuncCast<double (double)>(fdlibm::ceil, Args_Double_Double);
      case SymbolicAddress::CeilF:
        return FuncCast<float (float)>(fdlibm::ceilf, Args_Float32_Float32);
      case SymbolicAddress::FloorD:
        return FuncCast<double (double)>(fdlibm::floor, Args_Double_Double);
      case SymbolicAddress::FloorF:
        return FuncCast<float (float)>(fdlibm::floorf, Args_Float32_Float32);
      case SymbolicAddress::TruncD:
        return FuncCast<double (double)>(fdlibm::trunc, Args_Double_Double);
      case SymbolicAddress::TruncF:
        return FuncCast<float (float)>(fdlibm::truncf, Args_Float32_Float32);
      case SymbolicAddress::NearbyIntD:
        return FuncCast<double (double)>(nearbyint, Args_Double_Double);
      case SymbolicAddress::NearbyIntF:
        return FuncCast<float (float)>(nearbyintf, Args_Float32_Float32);
      case SymbolicAddress::ExpD:
        return FuncCast<double (double)>(fdlibm::exp, Args_Double_Double);
      case SymbolicAddress::LogD:
        return FuncCast<double (double)>(fdlibm::log, Args_Double_Double);
      case SymbolicAddress::PowD:
        return FuncCast(ecmaPow, Args_Double_DoubleDouble);
      case SymbolicAddress::ATan2D:
        return FuncCast(ecmaAtan2, Args_Double_DoubleDouble);
      case SymbolicAddress::Limit:
        break;
    }

    MOZ_CRASH("Bad SymbolicAddress");
}

SignalUsage::SignalUsage()
  :
#ifdef ASMJS_MAY_USE_SIGNAL_HANDLERS_FOR_OOB
    // Signal-handling is only used to eliminate bounds checks when the OS page
    // size is an even divisor of the WebAssembly page size.
    forOOB(HaveSignalHandlers() &&
           gc::SystemPageSize() <= PageSize &&
           PageSize % gc::SystemPageSize() == 0 &&
           !JitOptions.wasmExplicitBoundsChecks),
#else
    forOOB(false),
#endif
    forInterrupt(HaveSignalHandlers())
{}

bool
SignalUsage::operator==(SignalUsage rhs) const
{
    return forOOB == rhs.forOOB && forInterrupt == rhs.forInterrupt;
}

static uint32_t
GetCPUID()
{
    enum Arch {
        X86 = 0x1,
        X64 = 0x2,
        ARM = 0x3,
        MIPS = 0x4,
        MIPS64 = 0x5,
        ARCH_BITS = 3
    };

#if defined(JS_CODEGEN_X86)
    MOZ_ASSERT(uint32_t(jit::CPUInfo::GetSSEVersion()) <= (UINT32_MAX >> ARCH_BITS));
    return X86 | (uint32_t(jit::CPUInfo::GetSSEVersion()) << ARCH_BITS);
#elif defined(JS_CODEGEN_X64)
    MOZ_ASSERT(uint32_t(jit::CPUInfo::GetSSEVersion()) <= (UINT32_MAX >> ARCH_BITS));
    return X64 | (uint32_t(jit::CPUInfo::GetSSEVersion()) << ARCH_BITS);
#elif defined(JS_CODEGEN_ARM)
    MOZ_ASSERT(jit::GetARMFlags() <= (UINT32_MAX >> ARCH_BITS));
    return ARM | (jit::GetARMFlags() << ARCH_BITS);
#elif defined(JS_CODEGEN_ARM64)
    MOZ_CRASH("not enabled");
#elif defined(JS_CODEGEN_MIPS32)
    MOZ_ASSERT(jit::GetMIPSFlags() <= (UINT32_MAX >> ARCH_BITS));
    return MIPS | (jit::GetMIPSFlags() << ARCH_BITS);
#elif defined(JS_CODEGEN_MIPS64)
    MOZ_ASSERT(jit::GetMIPSFlags() <= (UINT32_MAX >> ARCH_BITS));
    return MIPS64 | (jit::GetMIPSFlags() << ARCH_BITS);
#elif defined(JS_CODEGEN_NONE)
    return 0;
#else
# error "unknown architecture"
#endif
}

size_t
Sig::serializedSize() const
{
    return sizeof(ret_) +
           SerializedPodVectorSize(args_);
}

uint8_t*
Sig::serialize(uint8_t* cursor) const
{
    cursor = WriteScalar<ExprType>(cursor, ret_);
    cursor = SerializePodVector(cursor, args_);
    return cursor;
}

const uint8_t*
Sig::deserialize(const uint8_t* cursor)
{
    (cursor = ReadScalar<ExprType>(cursor, &ret_)) &&
    (cursor = DeserializePodVector(cursor, &args_));
    return cursor;
}

size_t
Sig::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return args_.sizeOfExcludingThis(mallocSizeOf);
}

typedef uint32_t ImmediateType;  // for 32/64 consistency
static const unsigned sTotalBits = sizeof(ImmediateType) * 8;
static const unsigned sTagBits = 1;
static const unsigned sReturnBit = 1;
static const unsigned sLengthBits = 4;
static const unsigned sTypeBits = 2;
static const unsigned sMaxTypes = (sTotalBits - sTagBits - sReturnBit - sLengthBits) / sTypeBits;

static bool
IsImmediateType(ValType vt)
{
    MOZ_ASSERT(uint32_t(vt) > 0);
    return (uint32_t(vt) - 1) < (1 << sTypeBits);
}

static bool
IsImmediateType(ExprType et)
{
    return et == ExprType::Void || IsImmediateType(NonVoidToValType(et));
}

/* static */ bool
SigIdDesc::isGlobal(const Sig& sig)
{
    unsigned numTypes = (sig.ret() == ExprType::Void ? 0 : 1) +
                        (sig.args().length());
    if (numTypes > sMaxTypes)
        return true;

    if (!IsImmediateType(sig.ret()))
        return true;

    for (ValType v : sig.args()) {
        if (!IsImmediateType(v))
            return true;
    }

    return false;
}

/* static */ SigIdDesc
SigIdDesc::global(const Sig& sig, uint32_t globalDataOffset)
{
    MOZ_ASSERT(isGlobal(sig));
    return SigIdDesc(Kind::Global, globalDataOffset);
}

static ImmediateType
LengthToBits(uint32_t length)
{
    static_assert(sMaxTypes <= ((1 << sLengthBits) - 1), "fits");
    MOZ_ASSERT(length <= sMaxTypes);
    return length;
}

static ImmediateType
TypeToBits(ValType type)
{
    static_assert(3 <= ((1 << sTypeBits) - 1), "fits");
    MOZ_ASSERT(uint32_t(type) >= 1 && uint32_t(type) <= 4);
    return uint32_t(type) - 1;
}

/* static */ SigIdDesc
SigIdDesc::immediate(const Sig& sig)
{
    ImmediateType immediate = ImmediateBit;
    uint32_t shift = sTagBits;

    if (sig.ret() != ExprType::Void) {
        immediate |= (1 << shift);
        shift += sReturnBit;

        immediate |= TypeToBits(NonVoidToValType(sig.ret())) << shift;
        shift += sTypeBits;
    } else {
        shift += sReturnBit;
    }

    immediate |= LengthToBits(sig.args().length()) << shift;
    shift += sLengthBits;

    for (ValType argType : sig.args()) {
        immediate |= TypeToBits(argType) << shift;
        shift += sTypeBits;
    }

    MOZ_ASSERT(shift <= sTotalBits);
    return SigIdDesc(Kind::Immediate, immediate);
}

size_t
SigWithId::serializedSize() const
{
    return Sig::serializedSize() +
           sizeof(id);
}

uint8_t*
SigWithId::serialize(uint8_t* cursor) const
{
    cursor = Sig::serialize(cursor);
    cursor = WriteBytes(cursor, &id, sizeof(id));
    return cursor;
}

const uint8_t*
SigWithId::deserialize(const uint8_t* cursor)
{
    (cursor = Sig::deserialize(cursor)) &&
    (cursor = ReadBytes(cursor, &id, sizeof(id)));
    return cursor;
}

size_t
SigWithId::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return Sig::sizeOfExcludingThis(mallocSizeOf);
}

Assumptions::Assumptions(JS::BuildIdCharVector&& buildId)
  : usesSignal(),
    cpuId(GetCPUID()),
    buildId(Move(buildId)),
    newFormat(false)
{}

Assumptions::Assumptions()
  : usesSignal(),
    cpuId(GetCPUID()),
    buildId(),
    newFormat(false)
{}

bool
Assumptions::initBuildIdFromContext(ExclusiveContext* cx)
{
    if (!cx->buildIdOp() || !cx->buildIdOp()(&buildId)) {
        ReportOutOfMemory(cx);
        return false;
    }
    return true;
}

bool
Assumptions::clone(const Assumptions& other)
{
    usesSignal = other.usesSignal;
    cpuId = other.cpuId;
    newFormat = other.newFormat;
    return buildId.appendAll(other.buildId);
}

bool
Assumptions::operator==(const Assumptions& rhs) const
{
    return usesSignal == rhs.usesSignal &&
           cpuId == rhs.cpuId &&
           buildId.length() == rhs.buildId.length() &&
           PodEqual(buildId.begin(), rhs.buildId.begin(), buildId.length()) &&
           newFormat == rhs.newFormat;
}

size_t
Assumptions::serializedSize() const
{
    return sizeof(usesSignal) +
           sizeof(uint32_t) +
           SerializedPodVectorSize(buildId) +
           sizeof(bool);
}

uint8_t*
Assumptions::serialize(uint8_t* cursor) const
{
    cursor = WriteBytes(cursor, &usesSignal, sizeof(usesSignal));
    cursor = WriteScalar<uint32_t>(cursor, cpuId);
    cursor = SerializePodVector(cursor, buildId);
    cursor = WriteScalar<bool>(cursor, newFormat);
    return cursor;
}

const uint8_t*
Assumptions::deserialize(const uint8_t* cursor)
{
    (cursor = ReadBytes(cursor, &usesSignal, sizeof(usesSignal))) &&
    (cursor = ReadScalar<uint32_t>(cursor, &cpuId)) &&
    (cursor = DeserializePodVector(cursor, &buildId)) &&
    (cursor = ReadScalar<bool>(cursor, &newFormat));
    return cursor;
}

size_t
Assumptions::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return buildId.sizeOfExcludingThis(mallocSizeOf);
}

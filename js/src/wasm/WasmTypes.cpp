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

#include "wasm/WasmTypes.h"

#include "mozilla/MathAlgorithms.h"

#include "fdlibm.h"

#include "jslibmath.h"
#include "jsmath.h"

#include "jit/MacroAssembler.h"
#include "js/Conversions.h"
#include "vm/Interpreter.h"
#include "wasm/WasmBaselineCompile.h"
#include "wasm/WasmInstance.h"
#include "wasm/WasmSerialize.h"
#include "wasm/WasmSignalHandlers.h"

#include "vm/Debugger-inl.h"
#include "vm/Stack-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

using mozilla::IsNaN;
using mozilla::IsPowerOfTwo;

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

static void*
WasmHandleExecutionInterrupt()
{
    WasmActivation* activation = JSContext::innermostWasmActivation();

    // wasm::Compartment requires notification when execution is interrupted in
    // the compartment. Only the innermost compartment has been interrupted;
    // enclosing compartments necessarily exited through an exit stub.
    activation->compartment()->wasm.setInterrupted(true);
    bool success = CheckForInterrupt(activation->cx());
    activation->compartment()->wasm.setInterrupted(false);

    // Preserve the invariant that having a non-null resumePC means that we are
    // handling an interrupt.
    void* resumePC = activation->resumePC();
    activation->setResumePC(nullptr);

    // Return the resumePC if execution can continue or null if execution should
    // jump to the throw stub.
    return success ? resumePC : nullptr;
}

static bool
WasmHandleDebugTrap()
{
    WasmActivation* activation = JSContext::innermostWasmActivation();
    MOZ_ASSERT(activation);
    JSContext* cx = activation->cx();

    FrameIterator iter(activation);
    MOZ_ASSERT(iter.debugEnabled());
    const CallSite* site = iter.debugTrapCallsite();
    MOZ_ASSERT(site);
    if (site->kind() == CallSite::EnterFrame) {
        if (!iter.instance()->enterFrameTrapsEnabled())
            return true;
        DebugFrame* frame = iter.debugFrame();
        frame->setIsDebuggee();
        frame->observe(cx);
        // TODO call onEnterFrame
        JSTrapStatus status = Debugger::onEnterFrame(cx, frame);
        if (status == JSTRAP_RETURN) {
            // Ignoring forced return (JSTRAP_RETURN) -- changing code execution
            // order is not yet implemented in the wasm baseline.
            // TODO properly handle JSTRAP_RETURN and resume wasm execution.
            JS_ReportErrorASCII(cx, "Unexpected resumption value from onEnterFrame");
            return false;
        }
        return status == JSTRAP_CONTINUE;
    }
    if (site->kind() == CallSite::LeaveFrame) {
        DebugFrame* frame = iter.debugFrame();
        frame->updateReturnJSValue();
        bool ok = Debugger::onLeaveFrame(cx, frame, nullptr, true);
        frame->leave(cx);
        return ok;
    }

    DebugFrame* frame = iter.debugFrame();
    Code& code = iter.instance()->code();
    MOZ_ASSERT(code.hasBreakpointTrapAtOffset(site->lineOrBytecode()));
    if (code.stepModeEnabled(frame->funcIndex())) {
        RootedValue result(cx, UndefinedValue());
        JSTrapStatus status = Debugger::onSingleStep(cx, &result);
        if (status == JSTRAP_RETURN) {
            // TODO properly handle JSTRAP_RETURN.
            JS_ReportErrorASCII(cx, "Unexpected resumption value from onSingleStep");
            return false;
        }
        if (status != JSTRAP_CONTINUE)
            return false;
    }
    if (code.hasBreakpointSite(site->lineOrBytecode())) {
        RootedValue result(cx, UndefinedValue());
        JSTrapStatus status = Debugger::onTrap(cx, &result);
        if (status == JSTRAP_RETURN) {
            // TODO properly handle JSTRAP_RETURN.
            JS_ReportErrorASCII(cx, "Unexpected resumption value from breakpoint handler");
            return false;
        }
        if (status != JSTRAP_CONTINUE)
            return false;
    }
    return true;
}

static WasmActivation*
WasmHandleThrow()
{
    JSContext* cx = TlsContext.get();

    WasmActivation* activation = cx->wasmActivationStack();
    MOZ_ASSERT(activation);

    // FrameIterator iterates down wasm frames in the activation starting at
    // WasmActivation::exitFP. Pass Unwind::True to pop WasmActivation::exitFP
    // once each time FrameIterator is incremented, ultimately leaving exitFP
    // null when the FrameIterator is done(). This is necessary to prevent a
    // DebugFrame from being observed again after we just called onLeaveFrame
    // (which would lead to the frame being re-added to the map of live frames,
    // right as it becomes trash).
    FrameIterator iter(activation, FrameIterator::Unwind::True);
    if (iter.done())
        return activation;

    // Live wasm code on the stack is kept alive (in wasm::TraceActivations) by
    // marking the instance of every wasm::Frame found by FrameIterator.
    // However, as explained above, we're popping frames while iterating which
    // means that a GC during this loop could collect the code of frames whose
    // code is still on the stack. This is actually mostly fine: as soon as we
    // return to the throw stub, the entire stack will be popped as a whole,
    // returning to the C++ caller. However, we must keep the throw stub alive
    // itself which is owned by the innermost instance.
    RootedWasmInstanceObject keepAlive(cx, iter.instance()->object());

    for (; !iter.done(); ++iter) {
        if (!iter.debugEnabled())
            continue;

        DebugFrame* frame = iter.debugFrame();
        frame->clearReturnJSValue();

        // Assume JSTRAP_ERROR status if no exception is pending --
        // no onExceptionUnwind handlers must be fired.
        if (cx->isExceptionPending()) {
            JSTrapStatus status = Debugger::onExceptionUnwind(cx, frame);
            if (status == JSTRAP_RETURN) {
                // Unexpected trap return -- raising error since throw recovery
                // is not yet implemented in the wasm baseline.
                // TODO properly handle JSTRAP_RETURN and resume wasm execution.
                JS_ReportErrorASCII(cx, "Unexpected resumption value from onExceptionUnwind");
            }
        }

        bool ok = Debugger::onLeaveFrame(cx, frame, nullptr, false);
        if (ok) {
            // Unexpected success from the handler onLeaveFrame -- raising error
            // since throw recovery is not yet implemented in the wasm baseline.
            // TODO properly handle success and resume wasm execution.
            JS_ReportErrorASCII(cx, "Unexpected success from onLeaveFrame");
        }
        frame->leave(cx);
     }

    return activation;
}

static void
WasmReportTrap(int32_t trapIndex)
{
    JSContext* cx = JSContext::innermostWasmActivation()->cx();

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
      case Trap::IndirectCallToNull:
        errorNumber = JSMSG_WASM_IND_CALL_TO_NULL;
        break;
      case Trap::IndirectCallBadSig:
        errorNumber = JSMSG_WASM_IND_CALL_BAD_SIG;
        break;
      case Trap::ImpreciseSimdConversion:
        errorNumber = JSMSG_SIMD_FAILED_CONVERSION;
        break;
      case Trap::OutOfBounds:
        errorNumber = JSMSG_WASM_OUT_OF_BOUNDS;
        break;
      case Trap::StackOverflow:
        errorNumber = JSMSG_OVER_RECURSED;
        break;
      default:
        MOZ_CRASH("unexpected trap");
    }

    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, errorNumber);
}

static void
WasmReportOutOfBounds()
{
    JSContext* cx = JSContext::innermostWasmActivation()->cx();
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_WASM_OUT_OF_BOUNDS);
}

static void
WasmReportUnalignedAccess()
{
    JSContext* cx = JSContext::innermostWasmActivation()->cx();
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_WASM_UNALIGNED_ACCESS);
}

static int32_t
CoerceInPlace_ToInt32(MutableHandleValue val)
{
    JSContext* cx = JSContext::innermostWasmActivation()->cx();

    int32_t i32;
    if (!ToInt32(cx, val, &i32))
        return false;
    val.set(Int32Value(i32));

    return true;
}

static int32_t
CoerceInPlace_ToNumber(MutableHandleValue val)
{
    JSContext* cx = JSContext::innermostWasmActivation()->cx();

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
    // Note: INT64_MAX is not representable in double. It is actually
    // INT64_MAX + 1.  Therefore also sending the failure value.
    if (input >= double(INT64_MAX) || input < double(INT64_MIN) || IsNaN(input))
        return 0x8000000000000000;
    return int64_t(input);
}

static uint64_t
TruncateDoubleToUint64(double input)
{
    // Note: UINT64_MAX is not representable in double. It is actually UINT64_MAX + 1.
    // Therefore also sending the failure value.
    if (input >= double(UINT64_MAX) || input <= -1.0 || IsNaN(input))
        return 0x8000000000000000;
    return uint64_t(input);
}

static double
Int64ToDouble(int32_t x_hi, uint32_t x_lo)
{
    int64_t x = int64_t((uint64_t(x_hi) << 32)) + int64_t(x_lo);
    return double(x);
}

static float
Int64ToFloat32(int32_t x_hi, uint32_t x_lo)
{
    int64_t x = int64_t((uint64_t(x_hi) << 32)) + int64_t(x_lo);
    return float(x);
}

static double
Uint64ToDouble(int32_t x_hi, uint32_t x_lo)
{
    uint64_t x = (uint64_t(x_hi) << 32) + uint64_t(x_lo);
    return double(x);
}

static float
Uint64ToFloat32(int32_t x_hi, uint32_t x_lo)
{
    uint64_t x = (uint64_t(x_hi) << 32) + uint64_t(x_lo);
    return float(x);
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

bool
wasm::IsRoundingFunction(SymbolicAddress callee, jit::RoundingMode* mode)
{
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

void*
wasm::AddressOf(SymbolicAddress imm)
{
    switch (imm) {
      case SymbolicAddress::HandleExecutionInterrupt:
        return FuncCast(WasmHandleExecutionInterrupt, Args_General0);
      case SymbolicAddress::HandleDebugTrap:
        return FuncCast(WasmHandleDebugTrap, Args_General0);
      case SymbolicAddress::HandleThrow:
        return FuncCast(WasmHandleThrow, Args_General0);
      case SymbolicAddress::ReportTrap:
        return FuncCast(WasmReportTrap, Args_General1);
      case SymbolicAddress::ReportOutOfBounds:
        return FuncCast(WasmReportOutOfBounds, Args_General0);
      case SymbolicAddress::ReportUnalignedAccess:
        return FuncCast(WasmReportUnalignedAccess, Args_General0);
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
      case SymbolicAddress::Uint64ToDouble:
        return FuncCast(Uint64ToDouble, Args_Double_IntInt);
      case SymbolicAddress::Uint64ToFloat32:
        return FuncCast(Uint64ToFloat32, Args_Float32_IntInt);
      case SymbolicAddress::Int64ToDouble:
        return FuncCast(Int64ToDouble, Args_Double_IntInt);
      case SymbolicAddress::Int64ToFloat32:
        return FuncCast(Int64ToFloat32, Args_Float32_IntInt);
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
        return FuncCast<double (double)>(fdlibm::nearbyint, Args_Double_Double);
      case SymbolicAddress::NearbyIntF:
        return FuncCast<float (float)>(fdlibm::nearbyintf, Args_Float32_Float32);
      case SymbolicAddress::ExpD:
        return FuncCast<double (double)>(fdlibm::exp, Args_Double_Double);
      case SymbolicAddress::LogD:
        return FuncCast<double (double)>(fdlibm::log, Args_Double_Double);
      case SymbolicAddress::PowD:
        return FuncCast(ecmaPow, Args_Double_DoubleDouble);
      case SymbolicAddress::ATan2D:
        return FuncCast(ecmaAtan2, Args_Double_DoubleDouble);
      case SymbolicAddress::GrowMemory:
        return FuncCast<uint32_t (Instance*, uint32_t)>(Instance::growMemory_i32, Args_General2);
      case SymbolicAddress::CurrentMemory:
        return FuncCast<uint32_t (Instance*)>(Instance::currentMemory_i32, Args_General1);
      case SymbolicAddress::Limit:
        break;
    }

    MOZ_CRASH("Bad SymbolicAddress");
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
    switch (vt) {
      case ValType::I32:
      case ValType::I64:
      case ValType::F32:
      case ValType::F64:
        return true;
      case ValType::I8x16:
      case ValType::I16x8:
      case ValType::I32x4:
      case ValType::F32x4:
      case ValType::B8x16:
      case ValType::B16x8:
      case ValType::B32x4:
        return false;
    }
    MOZ_CRASH("bad ValType");
}

static unsigned
EncodeImmediateType(ValType vt)
{
    static_assert(3 < (1 << sTypeBits), "fits");
    switch (vt) {
      case ValType::I32:
        return 0;
      case ValType::I64:
        return 1;
      case ValType::F32:
        return 2;
      case ValType::F64:
        return 3;
      case ValType::I8x16:
      case ValType::I16x8:
      case ValType::I32x4:
      case ValType::F32x4:
      case ValType::B8x16:
      case ValType::B16x8:
      case ValType::B32x4:
        break;
    }
    MOZ_CRASH("bad ValType");
}

/* static */ bool
SigIdDesc::isGlobal(const Sig& sig)
{
    unsigned numTypes = (sig.ret() == ExprType::Void ? 0 : 1) +
                        (sig.args().length());
    if (numTypes > sMaxTypes)
        return true;

    if (sig.ret() != ExprType::Void && !IsImmediateType(NonVoidToValType(sig.ret())))
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

/* static */ SigIdDesc
SigIdDesc::immediate(const Sig& sig)
{
    ImmediateType immediate = ImmediateBit;
    uint32_t shift = sTagBits;

    if (sig.ret() != ExprType::Void) {
        immediate |= (1 << shift);
        shift += sReturnBit;

        immediate |= EncodeImmediateType(NonVoidToValType(sig.ret())) << shift;
        shift += sTypeBits;
    } else {
        shift += sReturnBit;
    }

    immediate |= LengthToBits(sig.args().length()) << shift;
    shift += sLengthBits;

    for (ValType argType : sig.args()) {
        immediate |= EncodeImmediateType(argType) << shift;
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

size_t
Import::serializedSize() const
{
    return module.serializedSize() +
           field.serializedSize() +
           sizeof(kind);
}

uint8_t*
Import::serialize(uint8_t* cursor) const
{
    cursor = module.serialize(cursor);
    cursor = field.serialize(cursor);
    cursor = WriteScalar<DefinitionKind>(cursor, kind);
    return cursor;
}

const uint8_t*
Import::deserialize(const uint8_t* cursor)
{
    (cursor = module.deserialize(cursor)) &&
    (cursor = field.deserialize(cursor)) &&
    (cursor = ReadScalar<DefinitionKind>(cursor, &kind));
    return cursor;
}

size_t
Import::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return module.sizeOfExcludingThis(mallocSizeOf) +
           field.sizeOfExcludingThis(mallocSizeOf);
}

Export::Export(UniqueChars fieldName, uint32_t index, DefinitionKind kind)
  : fieldName_(Move(fieldName))
{
    pod.kind_ = kind;
    pod.index_ = index;
}

Export::Export(UniqueChars fieldName, DefinitionKind kind)
  : fieldName_(Move(fieldName))
{
    pod.kind_ = kind;
    pod.index_ = 0;
}

uint32_t
Export::funcIndex() const
{
    MOZ_ASSERT(pod.kind_ == DefinitionKind::Function);
    return pod.index_;
}

uint32_t
Export::globalIndex() const
{
    MOZ_ASSERT(pod.kind_ == DefinitionKind::Global);
    return pod.index_;
}

size_t
Export::serializedSize() const
{
    return fieldName_.serializedSize() +
           sizeof(pod);
}

uint8_t*
Export::serialize(uint8_t* cursor) const
{
    cursor = fieldName_.serialize(cursor);
    cursor = WriteBytes(cursor, &pod, sizeof(pod));
    return cursor;
}

const uint8_t*
Export::deserialize(const uint8_t* cursor)
{
    (cursor = fieldName_.deserialize(cursor)) &&
    (cursor = ReadBytes(cursor, &pod, sizeof(pod)));
    return cursor;
}

size_t
Export::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return fieldName_.sizeOfExcludingThis(mallocSizeOf);
}

size_t
ElemSegment::serializedSize() const
{
    return sizeof(tableIndex) +
           sizeof(offset) +
           SerializedPodVectorSize(elemFuncIndices) +
           SerializedPodVectorSize(elemCodeRangeIndices);
}

uint8_t*
ElemSegment::serialize(uint8_t* cursor) const
{
    cursor = WriteBytes(cursor, &tableIndex, sizeof(tableIndex));
    cursor = WriteBytes(cursor, &offset, sizeof(offset));
    cursor = SerializePodVector(cursor, elemFuncIndices);
    cursor = SerializePodVector(cursor, elemCodeRangeIndices);
    return cursor;
}

const uint8_t*
ElemSegment::deserialize(const uint8_t* cursor)
{
    (cursor = ReadBytes(cursor, &tableIndex, sizeof(tableIndex))) &&
    (cursor = ReadBytes(cursor, &offset, sizeof(offset))) &&
    (cursor = DeserializePodVector(cursor, &elemFuncIndices)) &&
    (cursor = DeserializePodVector(cursor, &elemCodeRangeIndices));
    return cursor;
}

size_t
ElemSegment::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return elemFuncIndices.sizeOfExcludingThis(mallocSizeOf) +
           elemCodeRangeIndices.sizeOfExcludingThis(mallocSizeOf);
}

Assumptions::Assumptions(JS::BuildIdCharVector&& buildId)
  : cpuId(GetCPUID()),
    buildId(Move(buildId))
{}

Assumptions::Assumptions()
  : cpuId(GetCPUID()),
    buildId()
{}

bool
Assumptions::initBuildIdFromContext(JSContext* cx)
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
    cpuId = other.cpuId;
    return buildId.appendAll(other.buildId);
}

bool
Assumptions::operator==(const Assumptions& rhs) const
{
    return cpuId == rhs.cpuId &&
           buildId.length() == rhs.buildId.length() &&
           PodEqual(buildId.begin(), rhs.buildId.begin(), buildId.length());
}

size_t
Assumptions::serializedSize() const
{
    return sizeof(uint32_t) +
           SerializedPodVectorSize(buildId);
}

uint8_t*
Assumptions::serialize(uint8_t* cursor) const
{
    // The format of serialized Assumptions must never change in a way that
    // would cause old cache files written with by an old build-id to match the
    // assumptions of a different build-id.

    cursor = WriteScalar<uint32_t>(cursor, cpuId);
    cursor = SerializePodVector(cursor, buildId);
    return cursor;
}

const uint8_t*
Assumptions::deserialize(const uint8_t* cursor, size_t remain)
{
    (cursor = ReadScalarChecked<uint32_t>(cursor, &remain, &cpuId)) &&
    (cursor = DeserializePodVectorChecked(cursor, &remain, &buildId));
    return cursor;
}

size_t
Assumptions::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return buildId.sizeOfExcludingThis(mallocSizeOf);
}

//  Heap length on ARM should fit in an ARM immediate. We approximate the set
//  of valid ARM immediates with the predicate:
//    2^n for n in [16, 24)
//  or
//    2^24 * n for n >= 1.
bool
wasm::IsValidARMImmediate(uint32_t i)
{
    bool valid = (IsPowerOfTwo(i) ||
                  (i & 0x00ffffff) == 0);

    MOZ_ASSERT_IF(valid, i % PageSize == 0);

    return valid;
}

uint32_t
wasm::RoundUpToNextValidARMImmediate(uint32_t i)
{
    MOZ_ASSERT(i <= 0xff000000);

    if (i <= 16 * 1024 * 1024)
        i = i ? mozilla::RoundUpPow2(i) : 0;
    else
        i = (i + 0x00ffffff) & ~0x00ffffff;

    MOZ_ASSERT(IsValidARMImmediate(i));

    return i;
}

#ifndef WASM_HUGE_MEMORY

bool
wasm::IsValidBoundsCheckImmediate(uint32_t i)
{
#ifdef JS_CODEGEN_ARM
    return IsValidARMImmediate(i);
#else
    return true;
#endif
}

size_t
wasm::ComputeMappedSize(uint32_t maxSize)
{
    MOZ_ASSERT(maxSize % PageSize == 0);

    // It is the bounds-check limit, not the mapped size, that gets baked into
    // code. Thus round up the maxSize to the next valid immediate value
    // *before* adding in the guard page.

# ifdef JS_CODEGEN_ARM
    uint32_t boundsCheckLimit = RoundUpToNextValidARMImmediate(maxSize);
# else
    uint32_t boundsCheckLimit = maxSize;
# endif
    MOZ_ASSERT(IsValidBoundsCheckImmediate(boundsCheckLimit));

    MOZ_ASSERT(boundsCheckLimit % gc::SystemPageSize() == 0);
    MOZ_ASSERT(GuardSize % gc::SystemPageSize() == 0);
    return boundsCheckLimit + GuardSize;
}

#endif  // WASM_HUGE_MEMORY

void
DebugFrame::alignmentStaticAsserts()
{
    // VS2017 doesn't consider offsetOfFrame() to be a constexpr, so we have
    // to use offsetof directly. These asserts can't be at class-level
    // because the type is incomplete.

    static_assert(WasmStackAlignment >= Alignment,
                  "Aligned by ABI before pushing DebugFrame");
    static_assert((offsetof(DebugFrame, frame_) + sizeof(Frame)) % Alignment == 0,
                  "Aligned after pushing DebugFrame");
}

GlobalObject*
DebugFrame::global() const
{
    return &instance()->object()->global();
}

JSObject*
DebugFrame::environmentChain() const
{
    return &global()->lexicalEnvironment();
}

bool
DebugFrame::getLocal(uint32_t localIndex, MutableHandleValue vp)
{
    ValTypeVector locals;
    size_t argsLength;
    if (!instance()->code().debugGetLocalTypes(funcIndex(), &locals, &argsLength))
        return false;

    BaseLocalIter iter(locals, argsLength, /* debugEnabled = */ true);
    while (!iter.done() && iter.index() < localIndex)
        iter++;
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
      default:
          MOZ_CRASH("local type");
    }
    return true;
}

void
DebugFrame::updateReturnJSValue()
{
    hasCachedReturnJSValue_ = true;
    ExprType returnType = instance()->code().debugGetResultType(funcIndex());
    switch (returnType) {
      case ExprType::Void:
          cachedReturnJSValue_.setUndefined();
          break;
      case ExprType::I32:
          cachedReturnJSValue_.setInt32(resultI32_);
          break;
      case ExprType::I64:
          // Just display as a Number; it's ok if we lose some precision
          cachedReturnJSValue_.setDouble((double)resultI64_);
          break;
      case ExprType::F32:
          cachedReturnJSValue_.setDouble(JS::CanonicalizeNaN(resultF32_));
          break;
      case ExprType::F64:
          cachedReturnJSValue_.setDouble(JS::CanonicalizeNaN(resultF64_));
          break;
      default:
          MOZ_CRASH("result type");
    }
}

HandleValue
DebugFrame::returnValue() const
{
    MOZ_ASSERT(hasCachedReturnJSValue_);
    return HandleValue::fromMarkedLocation(&cachedReturnJSValue_);
}

void
DebugFrame::clearReturnJSValue()
{
    hasCachedReturnJSValue_ = true;
    cachedReturnJSValue_.setUndefined();
}

void
DebugFrame::observe(JSContext* cx)
{
   if (!observing_) {
       instance()->code().adjustEnterAndLeaveFrameTrapsState(cx, /* enabled = */ true);
       observing_ = true;
   }
}

void
DebugFrame::leave(JSContext* cx)
{
    if (observing_) {
       instance()->code().adjustEnterAndLeaveFrameTrapsState(cx, /* enabled = */ false);
       observing_ = false;
    }
}

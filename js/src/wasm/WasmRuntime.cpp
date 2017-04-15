/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2017 Mozilla Foundation
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

#include "wasm/WasmRuntime.h"

#include "mozilla/BinarySearch.h"

#include "fdlibm.h"

#include "jslibmath.h"

#include "jit/MacroAssembler.h"

#include "wasm/WasmInstance.h"
#include "wasm/WasmStubs.h"

#include "vm/Debugger-inl.h"
#include "vm/Stack-inl.h"

using namespace js;
using namespace jit;
using namespace wasm;

using mozilla::BinarySearchIf;
using mozilla::IsNaN;

static const unsigned BUILTIN_THUNK_LIFO_SIZE = 128;
static const CodeKind BUILTIN_THUNK_CODEKIND = CodeKind::OTHER_CODE;

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
    MOZ_ASSERT(activation->interrupted());

    if (!CheckForInterrupt(activation->cx())) {
        // If CheckForInterrupt failed, it is time to interrupt execution.
        // Returning nullptr to the caller will jump to the throw stub which
        // will call WasmHandleThrow. The WasmActivation must stay in the
        // interrupted state until then so that stack unwinding works in
        // WasmHandleThrow.
        return nullptr;
    }

    // If CheckForInterrupt succeeded, then execution can proceed and the
    // interrupt is over.
    void* resumePC = activation->resumePC();
    activation->finishInterrupt();
    return resumePC;
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
    if (iter.done()) {
        MOZ_ASSERT(!activation->interrupted());
        return activation;
    }

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

    MOZ_ASSERT(!activation->interrupted(), "unwinding clears the interrupt");
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
FuncCast(F* funcPtr, ABIFunctionType abiType)
{
    void* pf = JS_FUNC_TO_DATA_PTR(void*, funcPtr);
#ifdef JS_SIMULATOR
    pf = Simulator::RedirectNativeFunction(pf, abiType);
#endif
    return pf;
}

static void*
AddressOf(SymbolicAddress imm, ABIFunctionType* abiType)
{
    switch (imm) {
      case SymbolicAddress::HandleExecutionInterrupt:
        *abiType = Args_General0;
        return FuncCast(WasmHandleExecutionInterrupt, *abiType);
      case SymbolicAddress::HandleDebugTrap:
        *abiType = Args_General0;
        return FuncCast(WasmHandleDebugTrap, *abiType);
      case SymbolicAddress::HandleThrow:
        *abiType = Args_General0;
        return FuncCast(WasmHandleThrow, *abiType);
      case SymbolicAddress::ReportTrap:
        *abiType = Args_General1;
        return FuncCast(WasmReportTrap, *abiType);
      case SymbolicAddress::ReportOutOfBounds:
        *abiType = Args_General0;
        return FuncCast(WasmReportOutOfBounds, *abiType);
      case SymbolicAddress::ReportUnalignedAccess:
        *abiType = Args_General0;
        return FuncCast(WasmReportUnalignedAccess, *abiType);
      case SymbolicAddress::CallImport_Void:
        *abiType = Args_General4;
        return FuncCast(Instance::callImport_void, *abiType);
      case SymbolicAddress::CallImport_I32:
        *abiType = Args_General4;
        return FuncCast(Instance::callImport_i32, *abiType);
      case SymbolicAddress::CallImport_I64:
        *abiType = Args_General4;
        return FuncCast(Instance::callImport_i64, *abiType);
      case SymbolicAddress::CallImport_F64:
        *abiType = Args_General4;
        return FuncCast(Instance::callImport_f64, *abiType);
      case SymbolicAddress::CoerceInPlace_ToInt32:
        *abiType = Args_General1;
        return FuncCast(CoerceInPlace_ToInt32, *abiType);
      case SymbolicAddress::CoerceInPlace_ToNumber:
        *abiType = Args_General1;
        return FuncCast(CoerceInPlace_ToNumber, *abiType);
      case SymbolicAddress::ToInt32:
        *abiType = Args_Int_Double;
        return FuncCast<int32_t (double)>(JS::ToInt32, *abiType);
      case SymbolicAddress::DivI64:
        *abiType = Args_General4;
        return FuncCast(DivI64, *abiType);
      case SymbolicAddress::UDivI64:
        *abiType = Args_General4;
        return FuncCast(UDivI64, *abiType);
      case SymbolicAddress::ModI64:
        *abiType = Args_General4;
        return FuncCast(ModI64, *abiType);
      case SymbolicAddress::UModI64:
        *abiType = Args_General4;
        return FuncCast(UModI64, *abiType);
      case SymbolicAddress::TruncateDoubleToUint64:
        *abiType = Args_Int64_Double;
        return FuncCast(TruncateDoubleToUint64, *abiType);
      case SymbolicAddress::TruncateDoubleToInt64:
        *abiType = Args_Int64_Double;
        return FuncCast(TruncateDoubleToInt64, *abiType);
      case SymbolicAddress::Uint64ToDouble:
        *abiType = Args_Double_IntInt;
        return FuncCast(Uint64ToDouble, *abiType);
      case SymbolicAddress::Uint64ToFloat32:
        *abiType = Args_Float32_IntInt;
        return FuncCast(Uint64ToFloat32, *abiType);
      case SymbolicAddress::Int64ToDouble:
        *abiType = Args_Double_IntInt;
        return FuncCast(Int64ToDouble, *abiType);
      case SymbolicAddress::Int64ToFloat32:
        *abiType = Args_Float32_IntInt;
        return FuncCast(Int64ToFloat32, *abiType);
#if defined(JS_CODEGEN_ARM)
      case SymbolicAddress::aeabi_idivmod:
        *abiType = Args_General2;
        return FuncCast(__aeabi_idivmod, *abiType);
      case SymbolicAddress::aeabi_uidivmod:
        *abiType = Args_General2;
        return FuncCast(__aeabi_uidivmod, *abiType);
      case SymbolicAddress::AtomicCmpXchg:
        *abiType = Args_General5;
        return FuncCast(atomics_cmpxchg_asm_callout, *abiType);
      case SymbolicAddress::AtomicXchg:
        *abiType = Args_General4;
        return FuncCast(atomics_xchg_asm_callout, *abiType);
      case SymbolicAddress::AtomicFetchAdd:
        *abiType = Args_General4;
        return FuncCast(atomics_add_asm_callout, *abiType);
      case SymbolicAddress::AtomicFetchSub:
        *abiType = Args_General4;
        return FuncCast(atomics_sub_asm_callout, *abiType);
      case SymbolicAddress::AtomicFetchAnd:
        *abiType = Args_General4;
        return FuncCast(atomics_and_asm_callout, *abiType);
      case SymbolicAddress::AtomicFetchOr:
        *abiType = Args_General4;
        return FuncCast(atomics_or_asm_callout, *abiType);
      case SymbolicAddress::AtomicFetchXor:
        *abiType = Args_General4;
        return FuncCast(atomics_xor_asm_callout, *abiType);
#endif
      case SymbolicAddress::ModD:
        *abiType = Args_Double_DoubleDouble;
        return FuncCast(NumberMod, *abiType);
      case SymbolicAddress::SinD:
        *abiType = Args_Double_Double;
        return FuncCast<double (double)>(sin, *abiType);
      case SymbolicAddress::CosD:
        *abiType = Args_Double_Double;
        return FuncCast<double (double)>(cos, *abiType);
      case SymbolicAddress::TanD:
        *abiType = Args_Double_Double;
        return FuncCast<double (double)>(tan, *abiType);
      case SymbolicAddress::ASinD:
        *abiType = Args_Double_Double;
        return FuncCast<double (double)>(fdlibm::asin, *abiType);
      case SymbolicAddress::ACosD:
        *abiType = Args_Double_Double;
        return FuncCast<double (double)>(fdlibm::acos, *abiType);
      case SymbolicAddress::ATanD:
        *abiType = Args_Double_Double;
        return FuncCast<double (double)>(fdlibm::atan, *abiType);
      case SymbolicAddress::CeilD:
        *abiType = Args_Double_Double;
        return FuncCast<double (double)>(fdlibm::ceil, *abiType);
      case SymbolicAddress::CeilF:
        *abiType = Args_Float32_Float32;
        return FuncCast<float (float)>(fdlibm::ceilf, *abiType);
      case SymbolicAddress::FloorD:
        *abiType = Args_Double_Double;
        return FuncCast<double (double)>(fdlibm::floor, *abiType);
      case SymbolicAddress::FloorF:
        *abiType = Args_Float32_Float32;
        return FuncCast<float (float)>(fdlibm::floorf, *abiType);
      case SymbolicAddress::TruncD:
        *abiType = Args_Double_Double;
        return FuncCast<double (double)>(fdlibm::trunc, *abiType);
      case SymbolicAddress::TruncF:
        *abiType = Args_Float32_Float32;
        return FuncCast<float (float)>(fdlibm::truncf, *abiType);
      case SymbolicAddress::NearbyIntD:
        *abiType = Args_Double_Double;
        return FuncCast<double (double)>(fdlibm::nearbyint, *abiType);
      case SymbolicAddress::NearbyIntF:
        *abiType = Args_Float32_Float32;
        return FuncCast<float (float)>(fdlibm::nearbyintf, *abiType);
      case SymbolicAddress::ExpD:
        *abiType = Args_Double_Double;
        return FuncCast<double (double)>(fdlibm::exp, *abiType);
      case SymbolicAddress::LogD:
        *abiType = Args_Double_Double;
        return FuncCast<double (double)>(fdlibm::log, *abiType);
      case SymbolicAddress::PowD:
        *abiType = Args_Double_DoubleDouble;
        return FuncCast(ecmaPow, *abiType);
      case SymbolicAddress::ATan2D:
        *abiType = Args_Double_DoubleDouble;
        return FuncCast(ecmaAtan2, *abiType);
      case SymbolicAddress::GrowMemory:
        *abiType = Args_General2;
        return FuncCast(Instance::growMemory_i32, *abiType);
      case SymbolicAddress::CurrentMemory:
        *abiType = Args_General1;
        return FuncCast(Instance::currentMemory_i32, *abiType);
      case SymbolicAddress::Limit:
        break;
    }

    MOZ_CRASH("Bad SymbolicAddress");
}

static bool
GenerateBuiltinThunk(JSContext* cx, void* func, ABIFunctionType abiType, ExitReason exitReason,
                     UniqueBuiltinThunk* thunk)
{
    if (!cx->compartment()->ensureJitCompartmentExists(cx))
        return false;

    LifoAlloc lifo(BUILTIN_THUNK_LIFO_SIZE);
    TempAllocator tempAlloc(&lifo);
    MacroAssembler masm(MacroAssembler::WasmToken(), tempAlloc);

    CallableOffsets offsets = GenerateBuiltinNativeExit(masm, abiType, exitReason, func);

    masm.finish();
    if (masm.oom())
        return false;

    // The executable allocator operates on pointer-aligned sizes.
    uint32_t codeLength = AlignBytes(masm.bytesNeeded(), sizeof(void*));

    ExecutablePool* pool = nullptr;
    ExecutableAllocator& allocator = cx->runtime()->jitRuntime()->execAlloc();
    uint8_t* codeBase = (uint8_t*) allocator.alloc(cx, codeLength, &pool, BUILTIN_THUNK_CODEKIND);
    if (!codeBase)
        return false;

    {
        AutoWritableJitCode awjc(cx->runtime(), codeBase, codeLength);
        AutoFlushICache afc("GenerateBuiltinThunk");

        masm.executableCopy(codeBase);
        masm.processCodeLabels(codeBase);
        memset(codeBase + masm.bytesNeeded(), 0, codeLength - masm.bytesNeeded());

#ifdef DEBUG
        if (!masm.oom()) {
            MOZ_ASSERT(masm.callSites().empty());
            MOZ_ASSERT(masm.callFarJumps().empty());
            MOZ_ASSERT(masm.trapSites().empty());
            MOZ_ASSERT(masm.trapFarJumps().empty());
            MOZ_ASSERT(masm.extractMemoryAccesses().empty());
            MOZ_ASSERT(!masm.numSymbolicAccesses());
        }
#endif
    }

    *thunk = js::MakeUnique<BuiltinThunk>(codeBase, codeLength, pool, offsets);
    return !!*thunk;
}

struct BuiltinMatcher
{
    const uint8_t* address;
    explicit BuiltinMatcher(const uint8_t* address) : address(address) {}
    int operator()(const UniqueBuiltinThunk& thunk) const {
        if (address < thunk->base)
            return -1;
        if (uintptr_t(address) >= uintptr_t(thunk->base) + thunk->size)
            return 1;
        return 0;
    }
};

bool
wasm::Runtime::getBuiltinThunk(JSContext* cx, void* funcPtr, ABIFunctionType abiType,
                               ExitReason exitReason, void** thunkPtr)
{
    TypedFuncPtr lookup(funcPtr, abiType);
    auto ptr = builtinThunkMap_.lookupForAdd(lookup);
    if (ptr) {
        *thunkPtr = ptr->value();
        return true;
    }

    UniqueBuiltinThunk thunk;
    if (!GenerateBuiltinThunk(cx, funcPtr, abiType, exitReason, &thunk))
        return false;

    // Maintain sorted order of thunk addresses.
    size_t i;
    size_t size = builtinThunkVector_.length();
    if (BinarySearchIf(builtinThunkVector_, 0, size, BuiltinMatcher(thunk->base), &i))
        MOZ_CRASH("clobbering memory");

    *thunkPtr = thunk->base;

    return builtinThunkVector_.insert(builtinThunkVector_.begin() + i, Move(thunk)) &&
           builtinThunkMap_.add(ptr, lookup, *thunkPtr);
}

bool
wasm::NeedsBuiltinThunk(SymbolicAddress func)
{
    // Some functions don't want to a thunk, because they already have one or
    // they don't have frame info.
    switch (func) {
      case SymbolicAddress::HandleExecutionInterrupt: // GenerateInterruptExit
      case SymbolicAddress::HandleDebugTrap:          // GenerateDebugTrapStub
      case SymbolicAddress::HandleThrow:              // GenerateThrowStub
      case SymbolicAddress::ReportTrap:               // GenerateTrapExit
      case SymbolicAddress::ReportOutOfBounds:        // GenerateOutOfBoundsExit
      case SymbolicAddress::ReportUnalignedAccess:    // GeneratesUnalignedExit
      case SymbolicAddress::CallImport_Void:          // GenerateImportInterpExit
      case SymbolicAddress::CallImport_I32:
      case SymbolicAddress::CallImport_I64:
      case SymbolicAddress::CallImport_F64:
      case SymbolicAddress::CoerceInPlace_ToInt32:    // GenerateImportJitExit
      case SymbolicAddress::CoerceInPlace_ToNumber:
        return false;
      case SymbolicAddress::ToInt32:
      case SymbolicAddress::DivI64:
      case SymbolicAddress::UDivI64:
      case SymbolicAddress::ModI64:
      case SymbolicAddress::UModI64:
      case SymbolicAddress::TruncateDoubleToUint64:
      case SymbolicAddress::TruncateDoubleToInt64:
      case SymbolicAddress::Uint64ToDouble:
      case SymbolicAddress::Uint64ToFloat32:
      case SymbolicAddress::Int64ToDouble:
      case SymbolicAddress::Int64ToFloat32:
#if defined(JS_CODEGEN_ARM)
      case SymbolicAddress::aeabi_idivmod:
      case SymbolicAddress::aeabi_uidivmod:
      case SymbolicAddress::AtomicCmpXchg:
      case SymbolicAddress::AtomicXchg:
      case SymbolicAddress::AtomicFetchAdd:
      case SymbolicAddress::AtomicFetchSub:
      case SymbolicAddress::AtomicFetchAnd:
      case SymbolicAddress::AtomicFetchOr:
      case SymbolicAddress::AtomicFetchXor:
#endif
      case SymbolicAddress::ModD:
      case SymbolicAddress::SinD:
      case SymbolicAddress::CosD:
      case SymbolicAddress::TanD:
      case SymbolicAddress::ASinD:
      case SymbolicAddress::ACosD:
      case SymbolicAddress::ATanD:
      case SymbolicAddress::CeilD:
      case SymbolicAddress::CeilF:
      case SymbolicAddress::FloorD:
      case SymbolicAddress::FloorF:
      case SymbolicAddress::TruncD:
      case SymbolicAddress::TruncF:
      case SymbolicAddress::NearbyIntD:
      case SymbolicAddress::NearbyIntF:
      case SymbolicAddress::ExpD:
      case SymbolicAddress::LogD:
      case SymbolicAddress::PowD:
      case SymbolicAddress::ATan2D:
      case SymbolicAddress::GrowMemory:
      case SymbolicAddress::CurrentMemory:
        return true;
      case SymbolicAddress::Limit:
        break;
    }

    MOZ_CRASH("unexpected symbolic address");
}

bool
wasm::Runtime::getBuiltinThunk(JSContext* cx, SymbolicAddress func, void** thunkPtr)
{
    ABIFunctionType abiType;
    void* funcPtr = AddressOf(func, &abiType);

    if (!NeedsBuiltinThunk(func)) {
        *thunkPtr = funcPtr;
        return true;
    }

    return getBuiltinThunk(cx, funcPtr, abiType, ExitReason(func), thunkPtr);
}

static ABIFunctionType
ToABIFunctionType(const Sig& sig)
{
    const ValTypeVector& args = sig.args();
    ExprType ret = sig.ret();

    uint32_t abiType;
    switch (ret) {
      case ExprType::F32: abiType = ArgType_Float32 << RetType_Shift; break;
      case ExprType::F64: abiType = ArgType_Double << RetType_Shift; break;
      default:            MOZ_CRASH("unhandled ret type");
    }

    for (size_t i = 0; i < args.length(); i++) {
        switch (args[i]) {
          case ValType::F32: abiType |= (ArgType_Float32 << (ArgType_Shift * (i + 1))); break;
          case ValType::F64: abiType |= (ArgType_Double << (ArgType_Shift * (i + 1))); break;
          default:           MOZ_CRASH("unhandled arg type");
        }
    }

    return ABIFunctionType(abiType);
}

bool
wasm::Runtime::getBuiltinThunk(JSContext* cx, void* funcPtr, const Sig& sig, void** thunkPtr)
{
    ABIFunctionType abiType = ToABIFunctionType(sig);
#ifdef JS_SIMULATOR
    funcPtr = Simulator::RedirectNativeFunction(funcPtr, abiType);
#endif
    ExitReason nativeExitReason(ExitReason::Fixed::BuiltinNative);
    return getBuiltinThunk(cx, funcPtr, abiType, nativeExitReason, thunkPtr);
}

BuiltinThunk*
wasm::Runtime::lookupBuiltin(void* pc)
{
    size_t index;
    size_t length = builtinThunkVector_.length();
    if (!BinarySearchIf(builtinThunkVector_, 0, length, BuiltinMatcher((uint8_t*)pc), &index))
        return nullptr;
    return builtinThunkVector_[index].get();
}

void
wasm::Runtime::destroy()
{
    builtinThunkVector_.clear();
    if (builtinThunkMap_.initialized())
        builtinThunkMap_.clear();
}

BuiltinThunk::~BuiltinThunk()
{
    executablePool->release(size, BUILTIN_THUNK_CODEKIND);
}

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

#include "jslibmath.h"
#include "jsmath.h"

#include "asmjs/WasmModule.h"
#include "js/Conversions.h"
#include "vm/Interpreter.h"

#include "vm/Stack-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

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
    ReportOverRecursed(JSRuntime::innermostWasmActivation()->cx());
}

static bool
WasmHandleExecutionInterrupt()
{
    return CheckForInterrupt(JSRuntime::innermostWasmActivation()->cx());
}

static void
OnOutOfBounds()
{
    JSContext* cx = JSRuntime::innermostWasmActivation()->cx();
    JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_BAD_INDEX);
}

static void
OnImpreciseConversion()
{
    JSContext* cx = JSRuntime::innermostWasmActivation()->cx();
    JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_SIMD_FAILED_CONVERSION);
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

// Use an int32_t return type instead of bool since bool does not have a
// specified width and the caller is assuming a word-sized return.
static int32_t
InvokeImport_Void(int32_t importIndex, int32_t argc, Value* argv)
{
    WasmActivation* activation = JSRuntime::innermostWasmActivation();
    JSContext* cx = activation->cx();

    RootedValue rval(cx);
    return activation->module().callImport(cx, importIndex, argc, argv, &rval);
}

// Use an int32_t return type instead of bool since bool does not have a
// specified width and the caller is assuming a word-sized return.
static int32_t
InvokeImport_I32(int32_t importIndex, int32_t argc, Value* argv)
{
    WasmActivation* activation = JSRuntime::innermostWasmActivation();
    JSContext* cx = activation->cx();

    RootedValue rval(cx);
    if (!activation->module().callImport(cx, importIndex, argc, argv, &rval))
        return false;

    int32_t i32;
    if (!ToInt32(cx, rval, &i32))
        return false;

    argv[0] = Int32Value(i32);
    return true;
}

// Use an int32_t return type instead of bool since bool does not have a
// specified width and the caller is assuming a word-sized return.
static int32_t
InvokeImport_F64(int32_t importIndex, int32_t argc, Value* argv)
{
    WasmActivation* activation = JSRuntime::innermostWasmActivation();
    JSContext* cx = activation->cx();

    RootedValue rval(cx);
    if (!activation->module().callImport(cx, importIndex, argc, argv, &rval))
        return false;

    double dbl;
    if (!ToNumber(cx, rval, &dbl))
        return false;

    argv[0] = DoubleValue(dbl);
    return true;
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
      case SymbolicAddress::Runtime:
        return cx->runtimeAddressForJit();
      case SymbolicAddress::RuntimeInterruptUint32:
        return cx->runtimeAddressOfInterruptUint32();
      case SymbolicAddress::StackLimit:
        return cx->stackLimitAddressForJitCode(StackForUntrustedScript);
      case SymbolicAddress::ReportOverRecursed:
        return FuncCast(WasmReportOverRecursed, Args_General0);
      case SymbolicAddress::OnOutOfBounds:
        return FuncCast(OnOutOfBounds, Args_General0);
      case SymbolicAddress::OnImpreciseConversion:
        return FuncCast(OnImpreciseConversion, Args_General0);
      case SymbolicAddress::HandleExecutionInterrupt:
        return FuncCast(WasmHandleExecutionInterrupt, Args_General0);
      case SymbolicAddress::InvokeImport_Void:
        return FuncCast(InvokeImport_Void, Args_General3);
      case SymbolicAddress::InvokeImport_I32:
        return FuncCast(InvokeImport_I32, Args_General3);
      case SymbolicAddress::InvokeImport_F64:
        return FuncCast(InvokeImport_F64, Args_General3);
      case SymbolicAddress::CoerceInPlace_ToInt32:
        return FuncCast(CoerceInPlace_ToInt32, Args_General1);
      case SymbolicAddress::CoerceInPlace_ToNumber:
        return FuncCast(CoerceInPlace_ToNumber, Args_General1);
      case SymbolicAddress::ToInt32:
        return FuncCast<int32_t (double)>(JS::ToInt32, Args_Int_Double);
#if defined(JS_CODEGEN_ARM)
      case SymbolicAddress::aeabi_idivmod:
        return FuncCast(__aeabi_idivmod, Args_General2);
      case SymbolicAddress::aeabi_uidivmod:
        return FuncCast(__aeabi_uidivmod, Args_General2);
      case SymbolicAddress::AtomicCmpXchg:
        return FuncCast<int32_t (int32_t, int32_t, int32_t, int32_t)>(js::atomics_cmpxchg_asm_callout, Args_General4);
      case SymbolicAddress::AtomicXchg:
        return FuncCast<int32_t (int32_t, int32_t, int32_t)>(js::atomics_xchg_asm_callout, Args_General3);
      case SymbolicAddress::AtomicFetchAdd:
        return FuncCast<int32_t (int32_t, int32_t, int32_t)>(js::atomics_add_asm_callout, Args_General3);
      case SymbolicAddress::AtomicFetchSub:
        return FuncCast<int32_t (int32_t, int32_t, int32_t)>(js::atomics_sub_asm_callout, Args_General3);
      case SymbolicAddress::AtomicFetchAnd:
        return FuncCast<int32_t (int32_t, int32_t, int32_t)>(js::atomics_and_asm_callout, Args_General3);
      case SymbolicAddress::AtomicFetchOr:
        return FuncCast<int32_t (int32_t, int32_t, int32_t)>(js::atomics_or_asm_callout, Args_General3);
      case SymbolicAddress::AtomicFetchXor:
        return FuncCast<int32_t (int32_t, int32_t, int32_t)>(js::atomics_xor_asm_callout, Args_General3);
#endif
      case SymbolicAddress::ModD:
        return FuncCast(NumberMod, Args_Double_DoubleDouble);
      case SymbolicAddress::SinD:
#ifdef _WIN64
        // Workaround a VS 2013 sin issue, see math_sin_uncached.
        return FuncCast<double (double)>(js::math_sin_uncached, Args_Double_Double);
#else
        return FuncCast<double (double)>(sin, Args_Double_Double);
#endif
      case SymbolicAddress::CosD:
        return FuncCast<double (double)>(cos, Args_Double_Double);
      case SymbolicAddress::TanD:
        return FuncCast<double (double)>(tan, Args_Double_Double);
      case SymbolicAddress::ASinD:
        return FuncCast<double (double)>(asin, Args_Double_Double);
      case SymbolicAddress::ACosD:
        return FuncCast<double (double)>(acos, Args_Double_Double);
      case SymbolicAddress::ATanD:
        return FuncCast<double (double)>(atan, Args_Double_Double);
      case SymbolicAddress::CeilD:
        return FuncCast<double (double)>(ceil, Args_Double_Double);
      case SymbolicAddress::CeilF:
        return FuncCast<float (float)>(ceilf, Args_Float32_Float32);
      case SymbolicAddress::FloorD:
        return FuncCast<double (double)>(floor, Args_Double_Double);
      case SymbolicAddress::FloorF:
        return FuncCast<float (float)>(floorf, Args_Float32_Float32);
      case SymbolicAddress::ExpD:
        return FuncCast<double (double)>(exp, Args_Double_Double);
      case SymbolicAddress::LogD:
        return FuncCast<double (double)>(log, Args_Double_Double);
      case SymbolicAddress::PowD:
        return FuncCast(ecmaPow, Args_Double_DoubleDouble);
      case SymbolicAddress::ATan2D:
        return FuncCast(ecmaAtan2, Args_Double_DoubleDouble);
      case SymbolicAddress::Limit:
        break;
    }

    MOZ_CRASH("Bad SymbolicAddress");
}

CompileArgs::CompileArgs(ExclusiveContext* cx)
  :
#if defined(ASMJS_MAY_USE_SIGNAL_HANDLERS_FOR_OOB)
    useSignalHandlersForOOB(cx->canUseSignalHandlers()),
#else
    useSignalHandlersForOOB(false),
#endif
    useSignalHandlersForInterrupt(cx->canUseSignalHandlers())
{}

bool
CompileArgs::operator==(CompileArgs rhs) const
{
    return useSignalHandlersForOOB == rhs.useSignalHandlersForOOB &&
           useSignalHandlersForInterrupt == rhs.useSignalHandlersForInterrupt;
}

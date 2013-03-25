/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsmath.h"
#include "jscntxt.h"

#include "jstypedarrayinlines.h"

#include "AsmJS.h"
#include "AsmJSModule.h"
#include "frontend/BytecodeCompiler.h"

#include "Ion.h"

using namespace js;
using namespace js::ion;
using namespace mozilla;

#ifdef JS_ASMJS

static bool
LinkFail(JSContext *cx, const char *str)
{
    JS_ReportErrorFlagsAndNumber(cx, JSREPORT_WARNING, js_GetErrorMessage,
                                 NULL, JSMSG_USE_ASM_LINK_FAIL, str);
    return false;
}

static bool
ValidateGlobalVariable(JSContext *cx, const AsmJSModule &module, AsmJSModule::Global global,
                       HandleValue importVal)
{
    JS_ASSERT(global.which() == AsmJSModule::Global::Variable);

    void *datum = module.globalVarIndexToGlobalDatum(global.varIndex());

    switch (global.varInitKind()) {
      case AsmJSModule::Global::InitConstant: {
        const Value &v = global.varInitConstant();
        if (v.isInt32())
            *(int32_t *)datum = v.toInt32();
        else
            *(double *)datum = v.toDouble();
        break;
      }
      case AsmJSModule::Global::InitImport: {
        RootedPropertyName field(cx, global.varImportField());
        RootedValue v(cx);
        if (!GetProperty(cx, importVal, field, &v))
            return false;

        switch (global.varImportCoercion()) {
          case AsmJS_ToInt32:
            if (!ToInt32(cx, v, (int32_t *)datum))
                return false;
            break;
          case AsmJS_ToNumber:
            if (!ToNumber(cx, v, (double *)datum))
                return false;
            break;
        }
        break;
      }
    }

    return true;
}

static bool
ValidateFFI(JSContext *cx, AsmJSModule::Global global, HandleValue importVal,
            AutoObjectVector *ffis)
{
    RootedPropertyName field(cx, global.ffiField());
    RootedValue v(cx);
    if (!GetProperty(cx, importVal, field, &v))
        return false;

    if (!v.isObject() || !v.toObject().isFunction())
        return LinkFail(cx, "FFI imports must be functions");

    (*ffis)[global.ffiIndex()] = v.toObject().toFunction();
    return true;
}

static bool
ValidateArrayView(JSContext *cx, AsmJSModule::Global global, HandleValue globalVal,
                  HandleValue bufferVal)
{
    RootedPropertyName field(cx, global.viewName());
    RootedValue v(cx);
    if (!GetProperty(cx, globalVal, field, &v))
        return false;

    if (!IsTypedArrayConstructor(v, global.viewType()))
        return LinkFail(cx, "bad typed array constructor");

    return true;
}

static bool
ValidateMathBuiltin(JSContext *cx, AsmJSModule::Global global, HandleValue globalVal)
{
    RootedValue v(cx);
    if (!GetProperty(cx, globalVal, cx->names().Math, &v))
        return false;
    RootedPropertyName field(cx, global.mathName());
    if (!GetProperty(cx, v, field, &v))
        return false;

    Native native = NULL;
    switch (global.mathBuiltin()) {
      case AsmJSMathBuiltin_sin: native = math_sin; break;
      case AsmJSMathBuiltin_cos: native = math_cos; break;
      case AsmJSMathBuiltin_tan: native = math_tan; break;
      case AsmJSMathBuiltin_asin: native = math_asin; break;
      case AsmJSMathBuiltin_acos: native = math_acos; break;
      case AsmJSMathBuiltin_atan: native = math_atan; break;
      case AsmJSMathBuiltin_ceil: native = js_math_ceil; break;
      case AsmJSMathBuiltin_floor: native = js_math_floor; break;
      case AsmJSMathBuiltin_exp: native = math_exp; break;
      case AsmJSMathBuiltin_log: native = math_log; break;
      case AsmJSMathBuiltin_pow: native = js_math_pow; break;
      case AsmJSMathBuiltin_sqrt: native = js_math_sqrt; break;
      case AsmJSMathBuiltin_abs: native = js_math_abs; break;
      case AsmJSMathBuiltin_atan2: native = math_atan2; break;
      case AsmJSMathBuiltin_imul: native = math_imul; break;
    }

    if (!IsNativeFunction(v, native))
        return LinkFail(cx, "bad Math.* builtin");

    return true;
}

static bool
ValidateGlobalConstant(JSContext *cx, AsmJSModule::Global global, HandleValue globalVal)
{
    RootedPropertyName field(cx, global.constantName());
    RootedValue v(cx);
    if (!GetProperty(cx, globalVal, field, &v))
        return false;

    if (!v.isNumber())
        return LinkFail(cx, "global constant value needs to be a number");

    // NaN != NaN
    if (MOZ_DOUBLE_IS_NaN(global.constantValue())) {
        if (!MOZ_DOUBLE_IS_NaN(v.toNumber()))
            return LinkFail(cx, "global constant value needs to be NaN");
    } else {
        if (v.toNumber() != global.constantValue())
            return LinkFail(cx, "global constant value mismatch");
    }

    return true;
}

static bool
DynamicallyLinkModule(JSContext *cx, CallArgs args, AsmJSModule &module)
{
    if (module.isLinked())
        return LinkFail(cx, "As a temporary limitation, modules cannot be linked more than "
                            "once. This limitation should be removed in a future release. To "
                            "work around this, compile a second module (e.g., using the "
                            "Function constructor).");

    RootedValue globalVal(cx, UndefinedValue());
    if (args.length() > 0)
        globalVal = args[0];

    RootedValue importVal(cx, UndefinedValue());
    if (args.length() > 1)
        importVal = args[1];

    RootedValue bufferVal(cx, UndefinedValue());
    if (args.length() > 2)
        bufferVal = args[2];

    Rooted<ArrayBufferObject*> heap(cx);
    if (module.hasArrayView()) {
        if (!IsTypedArrayBuffer(bufferVal))
            return LinkFail(cx, "bad ArrayBuffer argument");

        heap = &bufferVal.toObject().asArrayBuffer();

        if (!IsPowerOfTwo(heap->byteLength()) || heap->byteLength() < AsmJSAllocationGranularity)
            return LinkFail(cx, "ArrayBuffer byteLength must be a power of two greater than or equal to 4096");

        if (!ArrayBufferObject::prepareForAsmJS(cx, heap))
            return LinkFail(cx, "Unable to prepare ArrayBuffer for asm.js use");

#if defined(JS_CPU_X86)
        void *heapOffset = (void*)heap->dataPointer();
        void *heapLength = (void*)heap->byteLength();
        uint8_t *code = module.functionCode();
        for (unsigned i = 0; i < module.numHeapAccesses(); i++) {
            const AsmJSHeapAccess &access = module.heapAccess(i);
            JSC::X86Assembler::setPointer(access.patchLengthAt(code), heapLength);
            JSC::X86Assembler::setPointer(access.patchOffsetAt(code), heapOffset);
        }
#elif defined(JS_CPU_ARM)
        // Now the length of the array is know, patch all of the bounds check sites
        // with the new length.
        ion::IonContext ic(cx, NULL);
        module.patchBoundsChecks(heap->byteLength());

#endif
    }

    AutoObjectVector ffis(cx);
    if (!ffis.resize(module.numFFIs()))
        return false;

    for (unsigned i = 0; i < module.numGlobals(); i++) {
        AsmJSModule::Global global = module.global(i);
        switch (global.which()) {
          case AsmJSModule::Global::Variable:
            if (!ValidateGlobalVariable(cx, module, global, importVal))
                return false;
            break;
          case AsmJSModule::Global::FFI:
            if (!ValidateFFI(cx, global, importVal, &ffis))
                return false;
            break;
          case AsmJSModule::Global::ArrayView:
            if (!ValidateArrayView(cx, global, globalVal, bufferVal))
                return false;
            break;
          case AsmJSModule::Global::MathBuiltin:
            if (!ValidateMathBuiltin(cx, global, globalVal))
                return false;
            break;
          case AsmJSModule::Global::Constant:
            if (!ValidateGlobalConstant(cx, global, globalVal))
                return false;
            break;
        }
    }

    for (unsigned i = 0; i < module.numExits(); i++)
        module.exitIndexToGlobalDatum(i).fun = ffis[module.exit(i).ffiIndex()]->toFunction();

    module.setIsLinked(heap);
    return true;
}

AsmJSActivation::AsmJSActivation(JSContext *cx, const AsmJSModule &module)
  : cx_(cx),
    module_(module),
    errorRejoinSP_(NULL),
    profiler_(NULL),
    resumePC_(NULL)
{
    if (cx->runtime->spsProfiler.enabled()) {
        profiler_ = &cx->runtime->spsProfiler;
        profiler_->enterNative("asm.js code", this);
    }

    prev_ = cx_->runtime->mainThread.asmJSActivationStack_;

    PerThreadData::AsmJSActivationStackLock lock(cx_->runtime->mainThread);
    cx_->runtime->mainThread.asmJSActivationStack_ = this;

    (void) errorRejoinSP_;  // squelch GCC warning
}

AsmJSActivation::~AsmJSActivation()
{
    if (profiler_)
        profiler_->exitNative();

    JS_ASSERT(cx_->runtime->mainThread.asmJSActivationStack_ == this);

    PerThreadData::AsmJSActivationStackLock lock(cx_->runtime->mainThread);
    cx_->runtime->mainThread.asmJSActivationStack_ = prev_;
}

static const unsigned ASM_MODULE_SLOT = 0;
static const unsigned ASM_EXPORT_INDEX_SLOT = 1;

static JSBool
CallAsmJS(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs callArgs = CallArgsFromVp(argc, vp);
    RootedFunction callee(cx, callArgs.callee().toFunction());

    // An asm.js function stores, in its extended slots:
    //  - a pointer to the module from which it was returned
    //  - its index in the ordered list of exported functions
    RootedObject moduleObj(cx, &callee->getExtendedSlot(ASM_MODULE_SLOT).toObject());
    const AsmJSModule &module = AsmJSModuleObjectToModule(moduleObj);

    // An exported function points to the code as well as the exported
    // function's signature, which implies the dynamic coercions performed on
    // the arguments.
    unsigned exportIndex = callee->getExtendedSlot(ASM_EXPORT_INDEX_SLOT).toInt32();
    const AsmJSModule::ExportedFunction &func = module.exportedFunction(exportIndex);

    // The calling convention for an external call into asm.js is to pass an
    // array of 8-byte values where each value contains either a coerced int32
    // (in the low word) or double value, with the coercions specified by the
    // asm.js signature. The external entry point unpacks this array into the
    // system-ABI-specified registers and stack memory and then calls into the
    // internal entry point. The return value is stored in the first element of
    // the array (which, therefore, must have length >= 1).

    Vector<uint64_t, 8> coercedArgs(cx);
    if (!coercedArgs.resize(Max<size_t>(1, func.numArgs())))
        return false;

    RootedValue v(cx);
    for (unsigned i = 0; i < func.numArgs(); ++i) {
        v = i < callArgs.length() ? callArgs[i] : UndefinedValue();
        switch (func.argCoercion(i)) {
          case AsmJS_ToInt32:
            if (!ToInt32(cx, v, (int32_t*)&coercedArgs[i]))
                return false;
            break;
          case AsmJS_ToNumber:
            if (!ToNumber(cx, v, (double*)&coercedArgs[i]))
                return false;
            break;
        }
    }

    {
        AsmJSActivation activation(cx, module);

        // Call into generated code.
#ifdef JS_CPU_ARM
        if (!func.code()(coercedArgs.begin(), module.globalData()))
            return false;
#else
        if (!func.code()(coercedArgs.begin()))
            return false;
#endif
    }

    switch (func.returnType()) {
      case AsmJSModule::Return_Void:
        callArgs.rval().set(UndefinedValue());
        break;
      case AsmJSModule::Return_Int32:
        callArgs.rval().set(Int32Value(*(int32_t*)&coercedArgs[0]));
        break;
      case AsmJSModule::Return_Double:
        callArgs.rval().set(NumberValue(*(double*)&coercedArgs[0]));
        break;
    }

    return true;
}

static JSFunction *
NewExportedFunction(JSContext *cx, const AsmJSModule::ExportedFunction &func,
                    HandleObject moduleObj, unsigned exportIndex)
{
    RootedPropertyName name(cx, func.name());
    JSFunction *fun = NewFunction(cx, NullPtr(), CallAsmJS, func.numArgs(),
                                  JSFunction::NATIVE_FUN, cx->global(), name,
                                  JSFunction::ExtendedFinalizeKind);
    if (!fun)
        return NULL;

    fun->setExtendedSlot(ASM_MODULE_SLOT, ObjectValue(*moduleObj));
    fun->setExtendedSlot(ASM_EXPORT_INDEX_SLOT, Int32Value(exportIndex));
    return fun;
}

static bool
HandleDynamicLinkFailure(JSContext *cx, CallArgs args, AsmJSModule &module, HandlePropertyName name)
{
    if (cx->isExceptionPending())
        return false;

    const AsmJSModule::PostLinkFailureInfo &info = module.postLinkFailureInfo();

    uint32_t length = info.bufEnd_ - info.bufStart_;
    Rooted<JSStableString*> src(cx, info.scriptSource_->substring(cx, info.bufStart_, info.bufEnd_));
    const jschar *chars = src->chars().get();

    RootedFunction fun(cx, NewFunction(cx, NullPtr(), NULL, 0, JSFunction::INTERPRETED,
                                       cx->global(), name));
    if (!fun)
        return false;

    AutoNameVector formals(cx);
    formals.reserve(3);
    if (module.globalArgumentName())
        formals.infallibleAppend(module.globalArgumentName());
    if (module.importArgumentName())
        formals.infallibleAppend(module.importArgumentName());
    if (module.bufferArgumentName())
        formals.infallibleAppend(module.bufferArgumentName());

    if (!frontend::CompileFunctionBody(cx, &fun, info.options_, formals, chars, length,
                                       /* isAsmJSRecompile = */ true))
        return false;

    // Call the function we just recompiled.

    unsigned argc = args.length();

    InvokeArgsGuard args2;
    if (!cx->stack.pushInvokeArgs(cx, argc, &args2))
        return false;

    args2.setCallee(ObjectValue(*fun));
    args2.setThis(args.thisv());
    for (unsigned i = 0; i < argc; i++)
        args2[i] = args[i];

    if (!Invoke(cx, args2))
        return false;

    args.rval().set(args2.rval());

    return true;
}

JSBool
js::LinkAsmJS(JSContext *cx, unsigned argc, JS::Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedFunction fun(cx, args.callee().toFunction());
    RootedObject moduleObj(cx, &AsmJSModuleObject(fun));
    AsmJSModule &module = AsmJSModuleObjectToModule(moduleObj);

    // If linking fails, recompile the function (including emitting bytecode)
    // as if it's normal JS code.
    if (!DynamicallyLinkModule(cx, args, module)) {
        RootedPropertyName name(cx, fun->name());
        return HandleDynamicLinkFailure(cx, args, module, name);
    }

    if (module.numExportedFunctions() == 1) {
        const AsmJSModule::ExportedFunction &func = module.exportedFunction(0);
        if (!func.maybeFieldName()) {
            RootedFunction fun(cx, NewExportedFunction(cx, func, moduleObj, 0));
            if (!fun)
                return false;

            args.rval().set(ObjectValue(*fun));
            return true;
        }
    }

    gc::AllocKind allocKind = gc::GetGCObjectKind(module.numExportedFunctions());
    RootedObject obj(cx, NewBuiltinClassInstance(cx, &ObjectClass, allocKind));
    if (!obj)
        return false;

    for (unsigned i = 0; i < module.numExportedFunctions(); i++) {
        const AsmJSModule::ExportedFunction &func = module.exportedFunction(i);

        RootedFunction fun(cx, NewExportedFunction(cx, func, moduleObj, i));
        if (!fun)
            return false;

        JS_ASSERT(func.maybeFieldName() != NULL);
        RootedId id(cx, NameToId(func.maybeFieldName()));
        RootedValue val(cx, ObjectValue(*fun));
        if (!DefineNativeProperty(cx, obj, id, val, NULL, NULL, JSPROP_ENUMERATE, 0, 0))
            return false;
    }

    args.rval().set(ObjectValue(*obj));
    return true;
}

bool
js::IsAsmJSModuleNative(js::Native native)
{
    return native == LinkAsmJS;
}

#endif  // defined(JS_ASMJS)

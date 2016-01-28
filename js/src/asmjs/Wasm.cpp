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

#include "asmjs/Wasm.h"

#include "jsprf.h"

#include "asmjs/AsmJS.h"
#include "asmjs/WasmGenerator.h"
#include "asmjs/WasmText.h"

using namespace js;
using namespace js::wasm;

using mozilla::PodCopy;

typedef Handle<WasmModuleObject*> HandleWasmModule;
typedef MutableHandle<WasmModuleObject*> MutableHandleWasmModule;

/*****************************************************************************/
// reporting

static bool
Fail(JSContext* cx, const char* str)
{
    JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_FAIL, str);
    return false;
}

static bool
Fail(JSContext* cx, Decoder& d, const char* str)
{
    uint32_t offset = d.currentOffset();
    char offsetStr[sizeof "4294967295"];
    JS_snprintf(offsetStr, sizeof offsetStr, "%" PRIu32, offset);
    JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_DECODE_FAIL, offsetStr, str);
    return false;
}

/*****************************************************************************/
// wasm function body validation

class FunctionDecoder
{
    JSContext* cx_;
    Decoder& d_;
    ModuleGenerator& mg_;
    FunctionGenerator& fg_;
    uint32_t funcIndex_;

  public:
    FunctionDecoder(JSContext* cx, Decoder& d, ModuleGenerator& mg, FunctionGenerator& fg,
                    uint32_t funcIndex)
      : cx_(cx), d_(d), mg_(mg), fg_(fg), funcIndex_(funcIndex)
    {}
    JSContext* cx() const { return cx_; }
    Decoder& d() const { return d_; }
    ModuleGenerator& mg() const { return mg_; }
    FunctionGenerator& fg() const { return fg_; }
    uint32_t funcIndex() const { return funcIndex_; }
    ExprType ret() const { return mg_.funcSig(funcIndex_).ret(); }

    bool fail(const char* str) {
        return Fail(cx_, d_, str);
    }
};

static const char*
ToCString(ExprType type)
{
    switch (type) {
      case ExprType::Void:  return "void";
      case ExprType::I32:   return "i32";
      case ExprType::I64:   return "i64";
      case ExprType::F32:   return "f32";
      case ExprType::F64:   return "f64";
      case ExprType::I32x4: return "i32x4";
      case ExprType::F32x4: return "f32x4";
      case ExprType::B32x4: return "b32x4";
      case ExprType::Limit:;
    }
    MOZ_CRASH("bad expression type");
}

static bool
CheckType(FunctionDecoder& f, ExprType actual, ExprType expected)
{
    if (actual == expected || expected == ExprType::Void)
        return true;

    UniqueChars error(JS_smprintf("type mismatch: expression has type %s but expected %s",
                                  ToCString(actual), ToCString(expected)));
    if (!error)
        return false;

    return f.fail(error.get());
}

static bool
DecodeExpr(FunctionDecoder& f, ExprType expected);

static bool
DecodeValType(JSContext* cx, Decoder& d, ValType *type)
{
    if (!d.readValType(type))
        return Fail(cx, d, "bad value type");

    switch (*type) {
      case ValType::I32:
      case ValType::F32:
      case ValType::F64:
        break;
      case ValType::I64:
      case ValType::I32x4:
      case ValType::F32x4:
      case ValType::B32x4:
        return Fail(cx, d, "value type NYI");
      case ValType::Limit:
        MOZ_CRASH("Limit");
    }

    return true;
}

static bool
DecodeExprType(JSContext* cx, Decoder& d, ExprType *type)
{
    if (!d.readExprType(type))
        return Fail(cx, d, "bad expression type");

    switch (*type) {
      case ExprType::I32:
      case ExprType::F32:
      case ExprType::F64:
      case ExprType::Void:
        break;
      case ExprType::I64:
      case ExprType::I32x4:
      case ExprType::F32x4:
      case ExprType::B32x4:
        return Fail(cx, d, "expression type NYI");
      case ExprType::Limit:
        MOZ_CRASH("Limit");
    }

    return true;
}

static bool
DecodeConst(FunctionDecoder& f, ExprType expected)
{
    if (!f.d().readVarU32())
        return f.fail("unable to read i32.const immediate");

    return CheckType(f, ExprType::I32, expected);
}

static bool
DecodeGetLocal(FunctionDecoder& f, ExprType expected)
{
    uint32_t localIndex;
    if (!f.d().readVarU32(&localIndex))
        return f.fail("unable to read get_local index");

    if (localIndex >= f.fg().locals().length())
        return f.fail("get_local index out of range");

    return CheckType(f, ToExprType(f.fg().locals()[localIndex]), expected);
}

static bool
DecodeSetLocal(FunctionDecoder& f, ExprType expected)
{
    uint32_t localIndex;
    if (!f.d().readVarU32(&localIndex))
        return f.fail("unable to read set_local index");

    if (localIndex >= f.fg().locals().length())
        return f.fail("set_local index out of range");

    ExprType localType = ToExprType(f.fg().locals()[localIndex]);

    if (!DecodeExpr(f, localType))
        return false;

    return CheckType(f, localType, expected);
}

static bool
DecodeBlock(FunctionDecoder& f, ExprType expected)
{
    uint32_t numExprs;
    if (!f.d().readVarU32(&numExprs))
        return f.fail("unable to read block's number of expressions");

    if (numExprs) {
        for (uint32_t i = 0; i < numExprs - 1; i++) {
            if (!DecodeExpr(f, ExprType::Void))
                return false;
        }
        if (!DecodeExpr(f, expected))
            return false;
    } else {
        if (!CheckType(f, ExprType::Void, expected))
            return false;
    }

    return true;
}

static bool
DecodeExpr(FunctionDecoder& f, ExprType expected)
{
    Expr expr;
    if (!f.d().readExpr(&expr))
        return f.fail("unable to read expression");

    switch (expr) {
      case Expr::Nop:
        return CheckType(f, ExprType::Void, expected);
      case Expr::I32Const:
        return DecodeConst(f, expected);
      case Expr::GetLocal:
        return DecodeGetLocal(f, expected);
      case Expr::SetLocal:
        return DecodeSetLocal(f, expected);
      case Expr::Block:
        return DecodeBlock(f, expected);
      default:
        break;
    }

    return f.fail("bad expression code");
}

static bool
DecodeFuncBody(JSContext* cx, Decoder& d, ModuleGenerator& mg, FunctionGenerator& fg,
               uint32_t funcIndex)
{
    const uint8_t* bodyBegin = d.currentPosition();

    FunctionDecoder f(cx, d, mg, fg, funcIndex);
    if (!DecodeExpr(f, f.ret()))
        return false;

    const uint8_t* bodyEnd = d.currentPosition();
    uintptr_t bodyLength = bodyEnd - bodyBegin;
    if (!fg.bytecode().resize(bodyLength))
        return false;

    PodCopy(fg.bytecode().begin(), bodyBegin, bodyLength);
    return true;
}

/*****************************************************************************/
// wasm decoding and generation

static bool
DecodeSignatureSection(JSContext* cx, Decoder& d, ModuleGeneratorData* init)
{
    if (!d.readCStringIf(SigSection))
        return true;

    uint32_t sectionStart;
    if (!d.startSection(&sectionStart))
        return Fail(cx, d, "expected decl section byte size");

    uint32_t numSigs;
    if (!d.readVarU32(&numSigs))
        return Fail(cx, d, "expected number of declarations");

    if (!init->sigs.resize(numSigs))
        return false;

    for (uint32_t sigIndex = 0; sigIndex < numSigs; sigIndex++) {
        uint32_t numArgs;
        if (!d.readVarU32(&numArgs))
            return Fail(cx, d, "bad number of signature args");

        ExprType result;
        if (!DecodeExprType(cx, d, &result))
            return false;

        ValTypeVector args;
        if (!args.resize(numArgs))
            return false;

        for (uint32_t i = 0; i < numArgs; i++) {
            if (!DecodeValType(cx, d, &args[i]))
                return false;
        }

        init->sigs[sigIndex] = Sig(Move(args), result);
    }

    if (!d.finishSection(sectionStart))
        return Fail(cx, d, "decls section byte size mismatch");

    return true;
}

static bool
DecodeDeclarationSection(JSContext* cx, Decoder& d, ModuleGeneratorData* init)
{
    if (!d.readCStringIf(DeclSection))
        return true;

    uint32_t sectionStart;
    if (!d.startSection(&sectionStart))
        return Fail(cx, d, "expected decl section byte size");

    uint32_t numDecls;
    if (!d.readVarU32(&numDecls))
        return Fail(cx, d, "expected number of declarations");

    if (!init->funcSigs.resize(numDecls))
        return false;

    for (uint32_t i = 0; i < numDecls; i++) {
        uint32_t sigIndex;
        if (!d.readVarU32(&sigIndex))
            return Fail(cx, d, "expected declaration signature index");

        if (sigIndex >= init->sigs.length())
            return Fail(cx, d, "declaration signature index out of range");

        init->funcSigs[i] = &init->sigs[sigIndex];
    }

    if (!d.finishSection(sectionStart))
        return Fail(cx, d, "decls section byte size mismatch");

    return true;
}

static bool
DecodeExport(JSContext* cx, Decoder& d, ModuleGenerator& mg, ExportMap* exportMap)
{
    if (!d.readCStringIf(FuncSubsection))
        return Fail(cx, d, "expected 'func' tag");

    uint32_t funcIndex;
    if (!d.readVarU32(&funcIndex))
        return Fail(cx, d, "expected export internal index");

    if (funcIndex >= mg.numFuncSigs())
        return Fail(cx, d, "export function index out of range");

    uint32_t exportIndex;
    if (!mg.declareExport(funcIndex, &exportIndex))
        return false;

    MOZ_ASSERT(exportIndex <= exportMap->exportNames.length());
    if (exportIndex == exportMap->exportNames.length()) {
        UniqueChars funcName(JS_smprintf("%u", unsigned(funcIndex)));
        if (!funcName || !exportMap->exportNames.emplaceBack(Move(funcName)))
            return false;
    }

    if (!exportMap->fieldsToExports.append(exportIndex))
        return false;

    const char* chars;
    if (!d.readCString(&chars))
        return Fail(cx, d, "expected export external name string");

    return exportMap->fieldNames.emplaceBack(DuplicateString(chars));
}

typedef HashSet<const char*, CStringHasher> CStringSet;

static bool
DecodeExportsSection(JSContext* cx, Decoder& d, ModuleGenerator& mg, ExportMap* exportMap)
{
    if (!d.readCStringIf(ExportSection))
        return true;

    uint32_t sectionStart;
    if (!d.startSection(&sectionStart))
        return Fail(cx, d, "expected export section byte size");

    uint32_t numExports;
    if (!d.readVarU32(&numExports))
        return Fail(cx, d, "expected number of exports");

    for (uint32_t i = 0; i < numExports; i++) {
        if (!DecodeExport(cx, d, mg, exportMap))
            return false;
    }

    if (!d.finishSection(sectionStart))
        return Fail(cx, d, "export section byte size mismatch");

    CStringSet dupSet(cx);
    if (!dupSet.init())
        return false;
    for (const UniqueChars& prevName : exportMap->fieldNames) {
        CStringSet::AddPtr p = dupSet.lookupForAdd(prevName.get());
        if (p)
            return Fail(cx, d, "duplicate export");
        if (!dupSet.add(p, prevName.get()))
            return false;
    }

    return true;
}

static bool
DecodeFunc(JSContext* cx, Decoder& d, ModuleGenerator& mg, uint32_t funcIndex)
{
    int64_t before = PRMJ_Now();

    FunctionGenerator fg;
    if (!mg.startFuncDef(d.currentOffset(), &fg))
        return false;

    if (!d.readCStringIf(FuncSubsection))
        return Fail(cx, d, "expected 'func' tag");

    uint32_t sectionStart;
    if (!d.startSection(&sectionStart))
        return Fail(cx, d, "expected func section byte size");

    const DeclaredSig& sig = mg.funcSig(funcIndex);
    for (ValType type : sig.args()) {
        if (!fg.addLocal(type))
            return false;
    }

    uint32_t numVars;
    if (!d.readVarU32(&numVars))
        return Fail(cx, d, "expected number of local vars");

    for (uint32_t i = 0; i < numVars; i++) {
        ValType type;
        if (!DecodeValType(cx, d, &type))
            return false;
        if (!fg.addLocal(type))
            return false;
    }

    if (!DecodeFuncBody(cx, d, mg, fg, funcIndex))
        return false;

    if (!d.finishSection(sectionStart))
        return Fail(cx, d, "func section byte size mismatch");

    int64_t after = PRMJ_Now();
    unsigned generateTime = (after - before) / PRMJ_USEC_PER_MSEC;

    return mg.finishFuncDef(funcIndex, generateTime, &fg);
}

static bool
DecodeCodeSection(JSContext* cx, Decoder& d, ModuleGenerator& mg)
{
    if (!mg.startFuncDefs())
        return false;

    uint32_t funcIndex = 0;
    while (d.readCStringIf(CodeSection)) {
        uint32_t sectionStart;
        if (!d.startSection(&sectionStart))
            return Fail(cx, d, "expected code section byte size");

        uint32_t numFuncs;
        if (!d.readVarU32(&numFuncs))
            return Fail(cx, d, "expected number of functions");

        if (funcIndex + numFuncs > mg.numFuncSigs())
            return Fail(cx, d, "more function definitions than declarations");

        for (uint32_t i = 0; i < numFuncs; i++) {
            if (!DecodeFunc(cx, d, mg, funcIndex++))
                return false;
        }

        if (!d.finishSection(sectionStart))
            return Fail(cx, d, "code section byte size mismatch");
    }

    if (funcIndex != mg.numFuncSigs())
        return Fail(cx, d, "fewer function definitions than declarations");

    if (!mg.finishFuncDefs())
        return false;

    return true;
}

static bool
DecodeUnknownSection(JSContext* cx, Decoder& d)
{
    const char* unused;
    if (!d.readCString(&unused))
        return Fail(cx, d, "failed to read section name");

    if (!d.skipSection())
        return Fail(cx, d, "unable to skip unknown section");

    return true;
}

static bool
DecodeModule(JSContext* cx, UniqueChars filename, const uint8_t* bytes, uint32_t length,
             MutableHandle<WasmModuleObject*> moduleObj, ExportMap* exportMap)
{
    Decoder d(bytes, bytes + length);

    uint32_t u32;
    if (!d.readU32(&u32) || u32 != MagicNumber)
        return Fail(cx, d, "failed to match magic number");

    if (!d.readU32(&u32) || u32 != EncodingVersion)
        return Fail(cx, d, "failed to match binary version");

    UniqueModuleGeneratorData init = MakeUnique<ModuleGeneratorData>();
    if (!init)
        return false;

    if (!DecodeSignatureSection(cx, d, init.get()))
        return false;

    if (!DecodeDeclarationSection(cx, d, init.get()))
        return false;

    ModuleGenerator mg(cx);
    if (!mg.init(Move(init)))
        return false;

    if (!DecodeExportsSection(cx, d, mg, exportMap))
        return false;

    HeapUsage heapUsage = HeapUsage::None;

    if (!DecodeCodeSection(cx, d, mg))
        return false;

    CacheableCharsVector funcNames;

    while (!d.readCStringIf(EndSection)) {
        if (!DecodeUnknownSection(cx, d))
            return false;
    }

    if (!d.done())
        return Fail(cx, d, "failed to consume all bytes of module");

    UniqueModuleData module;
    UniqueStaticLinkData link;
    SlowFunctionVector slowFuncs(cx);
    if (!mg.finish(heapUsage, Move(filename), Move(funcNames), &module, &link, &slowFuncs))
        return false;

    moduleObj.set(WasmModuleObject::create(cx));
    if (!moduleObj)
        return false;

    if (!moduleObj->init(cx->new_<Module>(Move(module))))
        return false;

    return moduleObj->module().staticallyLink(cx, *link);
}

/*****************************************************************************/
// JS entry poitns

static bool
SupportsWasm(JSContext* cx)
{
#if defined(JS_CODEGEN_NONE) || defined(JS_CODEGEN_ARM64)
    return false;
#endif

    if (!cx->jitSupportsFloatingPoint())
        return false;

    if (cx->gcSystemPageSize() != AsmJSPageSize)
        return false;

    return true;
}

static bool
CheckWasmSupport(JSContext* cx)
{
    if (!SupportsWasm(cx)) {
#ifdef JS_MORE_DETERMINISTIC
        fprintf(stderr, "WebAssembly is not supported on the current device.\n");
#endif // JS_MORE_DETERMINISTIC
        JS_ReportError(cx, "WebAssembly is not supported on the current device.");
        return false;
    }
    return true;
}

static bool
WasmIsSupported(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setBoolean(SupportsWasm(cx));
    return true;
}

static bool
WasmEval(JSContext* cx, unsigned argc, Value* vp)
{
    if (!CheckWasmSupport(cx))
        return false;

    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject callee(cx, &args.callee());

    if (args.length() < 1 || args.length() > 2) {
        ReportUsageError(cx, callee, "Wrong number of arguments");
        return false;
    }

    if (!args[0].isObject() || !args[0].toObject().is<ArrayBufferObject>()) {
        ReportUsageError(cx, callee, "First argument must be an ArrayBuffer");
        return false;
    }

    if (!args.get(1).isUndefined() && !args.get(1).isObject()) {
        ReportUsageError(cx, callee, "Second argument, if present, must be an Object");
        return false;
    }

    UniqueChars filename;
    if (!DescribeScriptedCaller(cx, &filename))
        return false;

    Rooted<ArrayBufferObject*> code(cx, &args[0].toObject().as<ArrayBufferObject>());
    const uint8_t* bytes = code->dataPointer();
    uint32_t length = code->byteLength();

    Vector<uint8_t> copy(cx);
    if (code->hasInlineData()) {
        if (!copy.append(bytes, length))
            return false;
        bytes = copy.begin();
    }

    Rooted<WasmModuleObject*> moduleObj(cx);
    ExportMap exportMap;
    if (!DecodeModule(cx, Move(filename), bytes, length, &moduleObj, &exportMap)) {
        if (!cx->isExceptionPending())
            ReportOutOfMemory(cx);
        return false;
    }

    Module& module = moduleObj->module();

    Rooted<ArrayBufferObject*> heap(cx);
    if (module.usesHeap())
        return Fail(cx, "Heap not implemented yet");

    Rooted<FunctionVector> imports(cx, FunctionVector(cx));
    if (module.imports().length() > 0)
        return Fail(cx, "Imports not implemented yet");

    RootedObject exportObj(cx);
    if (!module.dynamicallyLink(cx, moduleObj, heap, imports, exportMap, &exportObj))
        return false;

    args.rval().setObject(*exportObj);
    return true;
}

static bool
WasmTextToBinary(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject callee(cx, &args.callee());

    if (args.length() != 1) {
        ReportUsageError(cx, callee, "Wrong number of arguments");
        return false;
    }

    if (!args[0].isString()) {
        ReportUsageError(cx, callee, "First argument must be a String");
        return false;
    }

    AutoStableStringChars twoByteChars(cx);
    if (!twoByteChars.initTwoByte(cx, args[0].toString()))
        return false;

    UniqueChars error;
    wasm::UniqueBytecode bytes = wasm::TextToBinary(twoByteChars.twoByteChars(), &error);
    if (!bytes) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_TEXT_FAIL,
                             error.get() ? error.get() : "out of memory");
        return false;
    }

    Rooted<ArrayBufferObject*> buffer(cx, ArrayBufferObject::create(cx, bytes->length()));
    if (!buffer)
        return false;

    memcpy(buffer->dataPointer(), bytes->begin(), bytes->length());

    args.rval().setObject(*buffer);
    return true;
}

static const JSFunctionSpecWithHelp WasmTestingFunctions[] = {
    JS_FN_HELP("wasmIsSupported", WasmIsSupported, 0, 0,
"wasmIsSupported()",
"  Returns a boolean indicating whether WebAssembly is supported on the current device."),

    JS_FN_HELP("wasmEval", WasmEval, 2, 0,
"wasmEval(buffer, imports)",
"  Compiles the given binary wasm module given by 'buffer' (which must be an ArrayBuffer)\n"
"  and links the module's imports with the given 'imports' object."),

    JS_FN_HELP("wasmTextToBinary", WasmTextToBinary, 1, 0,
"wasmTextToBinary(str)",
"  Translates the given text wasm module into its binary encoding."),

    JS_FS_HELP_END
};

bool
wasm::DefineTestingFunctions(JSContext* cx, HandleObject globalObj)
{
    return JS_DefineFunctionsWithHelp(cx, globalObj, WasmTestingFunctions);
}

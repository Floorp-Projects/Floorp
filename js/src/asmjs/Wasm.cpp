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

#include "jsatominlines.h"
#include "jsobjinlines.h"

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
DecodeCall(FunctionDecoder& f, ExprType expected)
{
    uint32_t funcIndex;
    if (!f.d().readU32(&funcIndex))
        return f.fail("unable to read import index");

    if (funcIndex >= f.mg().numFuncSigs())
        return f.fail("callee index out of range");

    const DeclaredSig& sig = f.mg().funcSig(funcIndex);

    for (ValType argType : sig.args()) {
        if (!DecodeExpr(f, ToExprType(argType)))
            return false;
    }

    return CheckType(f, sig.ret(), expected);
}

static bool
DecodeCallImport(FunctionDecoder& f, ExprType expected)
{
    uint32_t importIndex;
    if (!f.d().readU32(&importIndex))
        return f.fail("unable to read import index");

    if (importIndex >= f.mg().numImports())
        return f.fail("import index out of range");

    const DeclaredSig& sig = *f.mg().import(importIndex).sig;

    for (ValType argType : sig.args()) {
        if (!DecodeExpr(f, ToExprType(argType)))
            return false;
    }

    return CheckType(f, sig.ret(), expected);
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
DecodeUnaryOperator(FunctionDecoder& f, ExprType expected, ExprType type)
{
    return CheckType(f, type, expected) &&
           DecodeExpr(f, expected);
}

static bool
DecodeBinaryOperator(FunctionDecoder& f, ExprType expected, ExprType type)
{
    return CheckType(f, type, expected) &&
           DecodeExpr(f, type) &&
           DecodeExpr(f, type);
}

static bool
DecodeComparisonOperator(FunctionDecoder& f, ExprType expected, ExprType type)
{
    return CheckType(f, ExprType::I32, expected) &&
           DecodeExpr(f, type) &&
           DecodeExpr(f, type);
}

static bool
DecodeConversionOperator(FunctionDecoder& f, ExprType expected,
                         ExprType dstType, ExprType srcType)
{
    return CheckType(f, dstType, expected) &&
           DecodeExpr(f, srcType);
}

static bool
DecodeIfElse(FunctionDecoder& f, bool hasElse, ExprType expected)
{
    return DecodeExpr(f, ExprType::I32) &&
           DecodeExpr(f, expected) &&
           (hasElse
            ? DecodeExpr(f, expected)
            : CheckType(f, ExprType::Void, expected));
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
      case Expr::Call:
        return DecodeCall(f, expected);
      case Expr::CallImport:
        return DecodeCallImport(f, expected);
      case Expr::I32Const:
        return DecodeConst(f, expected);
      case Expr::GetLocal:
        return DecodeGetLocal(f, expected);
      case Expr::SetLocal:
        return DecodeSetLocal(f, expected);
      case Expr::Block:
        return DecodeBlock(f, expected);
      case Expr::If:
        return DecodeIfElse(f, /* hasElse */ false, expected);
      case Expr::IfElse:
        return DecodeIfElse(f, /* hasElse */ true, expected);
      case Expr::I32Clz:
        return DecodeUnaryOperator(f, expected, ExprType::I32);
      case Expr::I32Ctz:
        return f.fail("NYI: ctz");
      case Expr::I32Popcnt:
        return f.fail("NYI: popcnt");
      case Expr::I64Clz:
      case Expr::I64Ctz:
      case Expr::I64Popcnt:
        return f.fail("NYI: i64") &&
               DecodeUnaryOperator(f, expected, ExprType::I64);
      case Expr::F32Abs:
      case Expr::F32Neg:
      case Expr::F32Ceil:
      case Expr::F32Floor:
      case Expr::F32Sqrt:
        return DecodeUnaryOperator(f, expected, ExprType::F32);
      case Expr::F32Trunc:
        return f.fail("NYI: trunc");
      case Expr::F32Nearest:
        return f.fail("NYI: nearest");
      case Expr::F64Abs:
      case Expr::F64Neg:
      case Expr::F64Ceil:
      case Expr::F64Floor:
      case Expr::F64Sqrt:
        return DecodeUnaryOperator(f, expected, ExprType::F64);
      case Expr::F64Trunc:
        return f.fail("NYI: trunc");
      case Expr::F64Nearest:
        return f.fail("NYI: nearest");
      case Expr::I32Add:
      case Expr::I32Sub:
      case Expr::I32Mul:
      case Expr::I32DivS:
      case Expr::I32DivU:
      case Expr::I32RemS:
      case Expr::I32RemU:
      case Expr::I32And:
      case Expr::I32Or:
      case Expr::I32Xor:
      case Expr::I32Shl:
      case Expr::I32ShrS:
      case Expr::I32ShrU:
        return DecodeBinaryOperator(f, expected, ExprType::I32);
      case Expr::I64Add:
      case Expr::I64Sub:
      case Expr::I64Mul:
      case Expr::I64DivS:
      case Expr::I64DivU:
      case Expr::I64RemS:
      case Expr::I64RemU:
      case Expr::I64And:
      case Expr::I64Or:
      case Expr::I64Xor:
      case Expr::I64Shl:
      case Expr::I64ShrS:
      case Expr::I64ShrU:
        return f.fail("NYI: i64") &&
               DecodeBinaryOperator(f, expected, ExprType::I64);
      case Expr::F32Add:
      case Expr::F32Sub:
      case Expr::F32Mul:
      case Expr::F32Div:
      case Expr::F32Min:
      case Expr::F32Max:
        return DecodeBinaryOperator(f, expected, ExprType::F32);
      case Expr::F32CopySign:
        return f.fail("NYI: copysign");
      case Expr::F64Add:
      case Expr::F64Sub:
      case Expr::F64Mul:
      case Expr::F64Div:
      case Expr::F64Min:
      case Expr::F64Max:
        return DecodeBinaryOperator(f, expected, ExprType::F64);
      case Expr::F64CopySign:
        return f.fail("NYI: copysign");
      case Expr::I32Eq:
      case Expr::I32Ne:
      case Expr::I32LtS:
      case Expr::I32LtU:
      case Expr::I32LeS:
      case Expr::I32LeU:
      case Expr::I32GtS:
      case Expr::I32GtU:
      case Expr::I32GeS:
      case Expr::I32GeU:
        return DecodeComparisonOperator(f, expected, ExprType::I32);
      case Expr::I64Eq:
      case Expr::I64Ne:
      case Expr::I64LtS:
      case Expr::I64LtU:
      case Expr::I64LeS:
      case Expr::I64LeU:
      case Expr::I64GtS:
      case Expr::I64GtU:
      case Expr::I64GeS:
      case Expr::I64GeU:
        return f.fail("NYI: i64") &&
               DecodeComparisonOperator(f, expected, ExprType::I64);
      case Expr::F32Eq:
      case Expr::F32Ne:
      case Expr::F32Lt:
      case Expr::F32Le:
      case Expr::F32Gt:
      case Expr::F32Ge:
        return DecodeComparisonOperator(f, expected, ExprType::F32);
      case Expr::F64Eq:
      case Expr::F64Ne:
      case Expr::F64Lt:
      case Expr::F64Le:
      case Expr::F64Gt:
      case Expr::F64Ge:
        return DecodeComparisonOperator(f, expected, ExprType::F64);
      case Expr::I32WrapI64:
        return f.fail("NYI: i64") &&
               DecodeConversionOperator(f, expected, ExprType::I32, ExprType::I64);
      case Expr::I32TruncSF32:
      case Expr::I32TruncUF32:
        return DecodeConversionOperator(f, expected, ExprType::I32, ExprType::F32);
      case Expr::I32ReinterpretF32:
        return f.fail("NYI: reinterpret");
      case Expr::I32TruncSF64:
      case Expr::I32TruncUF64:
        return DecodeConversionOperator(f, expected, ExprType::I32, ExprType::F64);
      case Expr::I64ExtendSI32:
      case Expr::I64ExtendUI32:
        return f.fail("NYI: i64") &&
               DecodeConversionOperator(f, expected, ExprType::I64, ExprType::I32);
      case Expr::I64TruncSF32:
      case Expr::I64TruncUF32:
        return f.fail("NYI: i64") &&
               DecodeConversionOperator(f, expected, ExprType::I64, ExprType::F32);
      case Expr::I64TruncSF64:
      case Expr::I64TruncUF64:
      case Expr::I64ReinterpretF64:
        return f.fail("NYI: i64") &&
               DecodeConversionOperator(f, expected, ExprType::I64, ExprType::F64);
      case Expr::F32ConvertSI32:
      case Expr::F32ConvertUI32:
        return DecodeConversionOperator(f, expected, ExprType::F32, ExprType::I32);
      case Expr::F32ReinterpretI32:
        return f.fail("NYI: reinterpret");
      case Expr::F32ConvertSI64:
      case Expr::F32ConvertUI64:
        return f.fail("NYI: i64") &&
               DecodeConversionOperator(f, expected, ExprType::F32, ExprType::I64);
      case Expr::F32DemoteF64:
        return DecodeConversionOperator(f, expected, ExprType::F32, ExprType::F64);
      case Expr::F64ConvertSI32:
      case Expr::F64ConvertUI32:
        return DecodeConversionOperator(f, expected, ExprType::F64, ExprType::I32);
      case Expr::F64ConvertSI64:
      case Expr::F64ConvertUI64:
      case Expr::F64ReinterpretI64:
        return f.fail("NYI: i64") &&
               DecodeConversionOperator(f, expected, ExprType::F64, ExprType::I64);
      case Expr::F64PromoteF32:
        return DecodeConversionOperator(f, expected, ExprType::F64, ExprType::F32);
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
// dynamic link data

struct ImportName
{
    UniqueChars module;
    UniqueChars func;

    ImportName(UniqueChars module, UniqueChars func)
      : module(Move(module)), func(Move(func))
    {}
    ImportName(ImportName&& rhs)
      : module(Move(rhs.module)), func(Move(rhs.func))
    {}
};

typedef Vector<ImportName, 0, SystemAllocPolicy> ImportNameVector;

struct DynamicLinkData
{
    ImportNameVector importNames;
    ExportMap exportMap;
};

typedef UniquePtr<DynamicLinkData> UniqueDynamicLinkData;

/*****************************************************************************/
// wasm decoding and generation

static bool
DecodeSignatureSection(JSContext* cx, Decoder& d, ModuleGeneratorData* init)
{
    if (!d.readCStringIf(SigSection))
        return true;

    uint32_t sectionStart;
    if (!d.startSection(&sectionStart))
        return Fail(cx, d, "expected signature section byte size");

    uint32_t numSigs;
    if (!d.readVarU32(&numSigs))
        return Fail(cx, d, "expected number of signatures");

    if (numSigs > MaxSigs)
        return Fail(cx, d, "too many signatures");

    if (!init->sigs.resize(numSigs))
        return false;

    for (uint32_t sigIndex = 0; sigIndex < numSigs; sigIndex++) {
        uint32_t numArgs;
        if (!d.readVarU32(&numArgs))
            return Fail(cx, d, "bad number of signature args");

        if (numArgs > MaxArgsPerFunc)
            return Fail(cx, d, "too many arguments in signature");

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
DecodeSignatureIndex(JSContext* cx, Decoder& d, const ModuleGeneratorData& init,
                     const DeclaredSig** sig)
{
    uint32_t sigIndex;
    if (!d.readVarU32(&sigIndex))
        return Fail(cx, d, "expected signature index");

    if (sigIndex >= init.sigs.length())
        return Fail(cx, d, "signature index out of range");

    *sig = &init.sigs[sigIndex];
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

    if (numDecls > MaxFuncs)
        return Fail(cx, d, "too many functions");

    if (!init->funcSigs.resize(numDecls))
        return false;

    for (uint32_t i = 0; i < numDecls; i++) {
        if (!DecodeSignatureIndex(cx, d, *init, &init->funcSigs[i]))
            return false;
    }

    if (!d.finishSection(sectionStart))
        return Fail(cx, d, "decls section byte size mismatch");

    return true;
}

static bool
DecodeImport(JSContext* cx, Decoder& d, ModuleGeneratorData* init, DynamicLinkData* link)
{
    if (!d.readCStringIf(FuncSubsection))
        return Fail(cx, d, "expected 'func' tag");

    const DeclaredSig* sig;
    if (!DecodeSignatureIndex(cx, d, *init, &sig))
        return false;

    if (!init->imports.emplaceBack(sig))
        return false;

    const char* moduleStr;
    if (!d.readCString(&moduleStr))
        return Fail(cx, d, "expected import module name");

    if (!*moduleStr)
        return Fail(cx, d, "module name cannot be empty");

    UniqueChars moduleName = DuplicateString(moduleStr);
    if (!moduleName)
        return false;

    const char* funcStr;
    if (!d.readCString(&funcStr))
        return Fail(cx, d, "expected import func name");

    UniqueChars funcName = DuplicateString(funcStr);
    if (!funcName)
        return false;

    return link->importNames.emplaceBack(Move(moduleName), Move(funcName));
}

static bool
DecodeImportSection(JSContext* cx, Decoder& d, ModuleGeneratorData* init, DynamicLinkData* link)
{
    if (!d.readCStringIf(ImportSection))
        return true;

    uint32_t sectionStart;
    if (!d.startSection(&sectionStart))
        return Fail(cx, d, "expected import section byte size");

    uint32_t numImports;
    if (!d.readVarU32(&numImports))
        return Fail(cx, d, "expected number of imports");

    if (numImports > MaxImports)
        return Fail(cx, d, "too many imports");

    for (uint32_t i = 0; i < numImports; i++) {
        if (!DecodeImport(cx, d, init, link))
            return false;
    }

    if (!d.finishSection(sectionStart))
        return Fail(cx, d, "import section byte size mismatch");

    return true;
}

static bool
DecodeExport(JSContext* cx, Decoder& d, ModuleGenerator& mg, DynamicLinkData* link)
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

    ExportMap& exportMap = link->exportMap;

    MOZ_ASSERT(exportIndex <= exportMap.exportNames.length());
    if (exportIndex == exportMap.exportNames.length()) {
        UniqueChars funcName(JS_smprintf("%u", unsigned(funcIndex)));
        if (!funcName || !exportMap.exportNames.emplaceBack(Move(funcName)))
            return false;
    }

    if (!exportMap.fieldsToExports.append(exportIndex))
        return false;

    const char* chars;
    if (!d.readCString(&chars))
        return Fail(cx, d, "expected export external name string");

    return exportMap.fieldNames.emplaceBack(DuplicateString(chars));
}

typedef HashSet<const char*, CStringHasher> CStringSet;

static bool
DecodeExportsSection(JSContext* cx, Decoder& d, ModuleGenerator& mg, DynamicLinkData* link)
{
    if (!d.readCStringIf(ExportSection))
        return true;

    uint32_t sectionStart;
    if (!d.startSection(&sectionStart))
        return Fail(cx, d, "expected export section byte size");

    uint32_t numExports;
    if (!d.readVarU32(&numExports))
        return Fail(cx, d, "expected number of exports");

    if (numExports > MaxExports)
        return Fail(cx, d, "too many exports");

    for (uint32_t i = 0; i < numExports; i++) {
        if (!DecodeExport(cx, d, mg, link))
            return false;
    }

    if (!d.finishSection(sectionStart))
        return Fail(cx, d, "export section byte size mismatch");

    CStringSet dupSet(cx);
    if (!dupSet.init())
        return false;
    for (const UniqueChars& prevName : link->exportMap.fieldNames) {
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
    const char* sectionName;
    if (!d.readCString(&sectionName))
        return Fail(cx, d, "failed to read section name");

    if (!strcmp(sectionName, SigSection) ||
        !strcmp(sectionName, ImportSection) ||
        !strcmp(sectionName, DeclSection) ||
        !strcmp(sectionName, ExportSection) ||
        !strcmp(sectionName, CodeSection))
    {
        return Fail(cx, d, "known section out of order");
    }

    if (!d.skipSection())
        return Fail(cx, d, "unable to skip unknown section");

    return true;
}

static bool
DecodeModule(JSContext* cx, UniqueChars filename, const uint8_t* bytes, uint32_t length,
             UniqueDynamicLinkData* dynamicLink, MutableHandle<WasmModuleObject*> moduleObj)
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

    *dynamicLink = MakeUnique<DynamicLinkData>();
    if (!*dynamicLink)
        return false;

    if (!DecodeImportSection(cx, d, init.get(), dynamicLink->get()))
        return false;

    if (!DecodeDeclarationSection(cx, d, init.get()))
        return false;

    ModuleGenerator mg(cx);
    if (!mg.init(Move(init), Move(filename)))
        return false;

    if (!DecodeExportsSection(cx, d, mg, dynamicLink->get()))
        return false;

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
    UniqueStaticLinkData staticLink;
    SlowFunctionVector slowFuncs(cx);
    if (!mg.finish(Move(funcNames), &module, &staticLink, &slowFuncs))
        return false;

    moduleObj.set(WasmModuleObject::create(cx));
    if (!moduleObj)
        return false;

    if (!moduleObj->init(cx->new_<Module>(Move(module))))
        return false;

    return moduleObj->module().staticallyLink(cx, *staticLink);
}

/*****************************************************************************/
// JS entry points

static bool
GetProperty(JSContext* cx, HandleObject obj, const char* utf8Chars, MutableHandleValue v)
{
    JSAtom* atom = AtomizeUTF8Chars(cx, utf8Chars, strlen(utf8Chars));
    if (!atom)
        return false;

    RootedId id(cx, AtomToId(atom));
    return GetProperty(cx, obj, obj, id, v);
}

static bool
ImportFunctions(JSContext* cx, HandleObject importObj, const ImportNameVector& importNames,
                MutableHandle<FunctionVector> imports)
{
    if (!importNames.empty() && !importObj)
        return Fail(cx, "no import object given");

    for (const ImportName& name : importNames) {
        RootedValue v(cx);
        if (!GetProperty(cx, importObj, name.module.get(), &v))
            return false;

        if (*name.func.get()) {
            if (!v.isObject())
                return Fail(cx, "import object field is not an Object");

            RootedObject obj(cx, &v.toObject());
            if (!GetProperty(cx, obj, name.func.get(), &v))
                return false;
        }

        if (!IsFunctionObject(v))
            return Fail(cx, "import object field is not a Function");

        if (!imports.append(&v.toObject().as<JSFunction>()))
            return false;
    }

    return true;
}

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

    Rooted<ArrayBufferObject*> code(cx, &args[0].toObject().as<ArrayBufferObject>());
    const uint8_t* bytes = code->dataPointer();
    uint32_t length = code->byteLength();

    Vector<uint8_t> copy(cx);
    if (code->hasInlineData()) {
        if (!copy.append(bytes, length))
            return false;
        bytes = copy.begin();
    }

    RootedObject importObj(cx);
    if (!args.get(1).isUndefined()) {
        if (!args.get(1).isObject()) {
            ReportUsageError(cx, callee, "Second argument, if present, must be an Object");
            return false;
        }
        importObj = &args[1].toObject();
    }

    UniqueChars filename;
    if (!DescribeScriptedCaller(cx, &filename))
        return false;

    UniqueDynamicLinkData link;
    Rooted<WasmModuleObject*> moduleObj(cx);
    if (!DecodeModule(cx, Move(filename), bytes, length, &link, &moduleObj)) {
        if (!cx->isExceptionPending())
            ReportOutOfMemory(cx);
        return false;
    }

    Module& module = moduleObj->module();

    Rooted<ArrayBufferObject*> heap(cx);
    if (module.usesHeap())
        return Fail(cx, "Heap not implemented yet");

    Rooted<FunctionVector> imports(cx, FunctionVector(cx));
    if (!ImportFunctions(cx, importObj, link->importNames, &imports))
        return false;

    RootedObject exportObj(cx);
    if (!module.dynamicallyLink(cx, moduleObj, heap, imports, link->exportMap, &exportObj))
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

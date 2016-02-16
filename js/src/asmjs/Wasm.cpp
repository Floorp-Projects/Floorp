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

#include "asmjs/WasmGenerator.h"
#include "asmjs/WasmText.h"
#include "vm/ArrayBufferObject.h"

#include "jsatominlines.h"
#include "jsobjinlines.h"

using namespace js;
using namespace js::wasm;

using mozilla::IsNaN;

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
DecodeCallWithSig(FunctionDecoder& f, const Sig& sig, ExprType expected)
{
    for (ValType argType : sig.args()) {
        if (!DecodeExpr(f, ToExprType(argType)))
            return false;
    }

    return CheckType(f, sig.ret(), expected);
}

static bool
DecodeCall(FunctionDecoder& f, ExprType expected)
{
    uint32_t funcIndex;
    if (!f.d().readVarU32(&funcIndex))
        return f.fail("unable to read import index");

    if (funcIndex >= f.mg().numFuncSigs())
        return f.fail("callee index out of range");

    return DecodeCallWithSig(f, f.mg().funcSig(funcIndex), expected);
}

static bool
DecodeCallImport(FunctionDecoder& f, ExprType expected)
{
    uint32_t importIndex;
    if (!f.d().readVarU32(&importIndex))
        return f.fail("unable to read import index");

    if (importIndex >= f.mg().numImports())
        return f.fail("import index out of range");

    return DecodeCallWithSig(f, *f.mg().import(importIndex).sig, expected);
}

static bool
DecodeCallIndirect(FunctionDecoder& f, ExprType expected)
{
    uint32_t sigIndex;
    if (!f.d().readVarU32(&sigIndex))
        return f.fail("unable to read indirect call signature index");

    if (sigIndex >= f.mg().numSigs())
        return f.fail("signature index out of range");

    if (!DecodeExpr(f, ExprType::I32))
        return false;

    return DecodeCallWithSig(f, f.mg().sig(sigIndex), expected);
}

static bool
DecodeConstI32(FunctionDecoder& f, ExprType expected)
{
    if (!f.d().readVarU32())
        return f.fail("unable to read i32.const immediate");

    return CheckType(f, ExprType::I32, expected);
}

static bool
DecodeConstI64(FunctionDecoder& f, ExprType expected)
{
    if (!f.d().readVarU64())
        return f.fail("unable to read i64.const immediate");

    return CheckType(f, ExprType::I64, expected);
}

static bool
DecodeConstF32(FunctionDecoder& f, ExprType expected)
{
    float value;
    if (!f.d().readFixedF32(&value))
        return f.fail("unable to read f32.const immediate");
    if (IsNaN(value)) {
        const float jsNaN = (float)JS::GenericNaN();
        if (memcmp(&value, &jsNaN, sizeof(value)) != 0)
            return f.fail("NYI: NaN literals with custom payloads");
    }

    return CheckType(f, ExprType::F32, expected);
}

static bool
DecodeConstF64(FunctionDecoder& f, ExprType expected)
{
    double value;
    if (!f.d().readFixedF64(&value))
        return f.fail("unable to read f64.const immediate");
    if (IsNaN(value)) {
        const double jsNaN = JS::GenericNaN();
        if (memcmp(&value, &jsNaN, sizeof(value)) != 0)
            return f.fail("NYI: NaN literals with custom payloads");
    }

    return CheckType(f, ExprType::F64, expected);
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
           DecodeExpr(f, type);
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
DecodeLoadStoreAddress(FunctionDecoder &f)
{
    uint32_t offset, align;
    return DecodeExpr(f, ExprType::I32) &&
           f.d().readVarU32(&offset) &&
           f.d().readVarU32(&align) &&
           mozilla::IsPowerOfTwo(align) &&
           (offset == 0 || f.fail("NYI: address offsets")) &&
           f.fail("NYI: wasm loads and stores");
}

static bool
DecodeLoad(FunctionDecoder& f, ExprType expected, ExprType type)
{
    return DecodeLoadStoreAddress(f) &&
           CheckType(f, type, expected);
}

static bool
DecodeStore(FunctionDecoder& f, ExprType expected, ExprType type)
{
    return DecodeLoadStoreAddress(f) &&
           DecodeExpr(f, expected) &&
           CheckType(f, type, expected);
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
      case Expr::CallIndirect:
        return DecodeCallIndirect(f, expected);
      case Expr::I32Const:
        return DecodeConstI32(f, expected);
      case Expr::I64Const:
        return f.fail("NYI: i64") &&
               DecodeConstI64(f, expected);
      case Expr::F32Const:
        return DecodeConstF32(f, expected);
      case Expr::F64Const:
        return DecodeConstF64(f, expected);
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
      case Expr::I32LoadMem:
      case Expr::I32LoadMem8S:
      case Expr::I32LoadMem8U:
      case Expr::I32LoadMem16S:
      case Expr::I32LoadMem16U:
        return DecodeLoad(f, expected, ExprType::I32);
      case Expr::I64LoadMem:
      case Expr::I64LoadMem8S:
      case Expr::I64LoadMem8U:
      case Expr::I64LoadMem16S:
      case Expr::I64LoadMem16U:
      case Expr::I64LoadMem32S:
      case Expr::I64LoadMem32U:
        return DecodeLoad(f, expected, ExprType::I64);
      case Expr::F32LoadMem:
        return DecodeLoad(f, expected, ExprType::F32);
      case Expr::F64LoadMem:
        return DecodeLoad(f, expected, ExprType::F64);
      case Expr::I32StoreMem:
      case Expr::I32StoreMem8:
      case Expr::I32StoreMem16:
        return DecodeStore(f, expected, ExprType::I32);
      case Expr::I64StoreMem:
      case Expr::I64StoreMem8:
      case Expr::I64StoreMem16:
      case Expr::I64StoreMem32:
        return f.fail("NYI: i64") &&
               DecodeStore(f, expected, ExprType::I64);
      case Expr::F32StoreMem:
        return DecodeStore(f, expected, ExprType::F32);
      case Expr::F64StoreMem:
        return DecodeStore(f, expected, ExprType::F64);
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

    memcpy(fg.bytecode().begin(), bodyBegin, bodyLength);
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

/*****************************************************************************/
// wasm decoding and generation

typedef HashSet<const DeclaredSig*, SigHashPolicy> SigSet;

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
    if (!init->sigToTable.resize(numSigs))
        return false;

    SigSet dupSet(cx);
    if (!dupSet.init())
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

        SigSet::AddPtr p = dupSet.lookupForAdd(init->sigs[sigIndex]);
        if (p)
            return Fail(cx, d, "duplicate signature");

        if (!dupSet.add(p, &init->sigs[sigIndex]))
            return false;
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
DecodeTableSection(JSContext* cx, Decoder& d, ModuleGeneratorData* init)
{
    if (!d.readCStringIf(TableSection))
        return true;

    uint32_t sectionStart;
    if (!d.startSection(&sectionStart))
        return Fail(cx, d, "expected table section byte size");

    if (!d.readVarU32(&init->numTableElems))
        return Fail(cx, d, "expected number of table elems");

    if (init->numTableElems > MaxTableElems)
        return Fail(cx, d, "too many table elements");

    Uint32Vector elems;
    if (!elems.resize(init->numTableElems))
        return false;

    for (uint32_t i = 0; i < init->numTableElems; i++) {
        uint32_t funcIndex;
        if (!d.readVarU32(&funcIndex))
            return Fail(cx, d, "expected table element");

        if (funcIndex >= init->funcSigs.length())
            return Fail(cx, d, "table element out of range");

        elems[i] = funcIndex;
    }

    if (!d.finishSection(sectionStart))
        return Fail(cx, d, "table section byte size mismatch");

    // Convert the single (heterogeneous) indirect function table into an
    // internal set of asm.js-like homogeneous tables indexed by signature.
    // Every element in the heterogeneous table is present in only one
    // homogeneous table (as determined by its signature). An element's index in
    // the heterogeneous table is the same as its index in its homogeneous table
    // and all other homogeneous tables are given an entry that will fault if
    // called for at that element's index.

    for (uint32_t elemIndex = 0; elemIndex < elems.length(); elemIndex++) {
        uint32_t funcIndex = elems[elemIndex];
        TableModuleGeneratorData& table = init->sigToTable[init->funcSigIndex(funcIndex)];
        if (table.numElems == 0) {
            table.numElems = elems.length();
            if (!table.elemFuncIndices.appendN(ModuleGenerator::BadIndirectCall, elems.length()))
                return false;
        }
    }

    for (uint32_t elemIndex = 0; elemIndex < elems.length(); elemIndex++) {
        uint32_t funcIndex = elems[elemIndex];
        init->sigToTable[init->funcSigIndex(funcIndex)].elemFuncIndices[elemIndex] = funcIndex;
    }

    return true;
}

static bool
DecodeImport(JSContext* cx, Decoder& d, ModuleGeneratorData* init, ImportNameVector* importNames)
{
    if (!d.readCStringIf(FuncSubsection))
        return Fail(cx, d, "expected 'func' tag");

    const DeclaredSig* sig;
    if (!DecodeSignatureIndex(cx, d, *init, &sig))
        return false;

    if (!init->imports.emplaceBack(sig))
        return false;

    UniqueChars moduleName = d.readCString();
    if (!moduleName)
        return Fail(cx, d, "expected import module name");

    if (!*moduleName.get())
        return Fail(cx, d, "module name cannot be empty");

    UniqueChars funcName = d.readCString();
    if (!funcName)
        return Fail(cx, d, "expected import func name");

    return importNames->emplaceBack(Move(moduleName), Move(funcName));
}

static bool
DecodeImportSection(JSContext* cx, Decoder& d, ModuleGeneratorData* init, ImportNameVector* importNames)
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
        if (!DecodeImport(cx, d, init, importNames))
            return false;
    }

    if (!d.finishSection(sectionStart))
        return Fail(cx, d, "import section byte size mismatch");

    return true;
}

static bool
DecodeMemorySection(JSContext* cx, Decoder& d, ModuleGenerator& mg,
                    MutableHandle<ArrayBufferObject*> heap)
{
    if (!d.readCStringIf(MemorySection))
        return true;

    uint32_t sectionStart;
    if (!d.startSection(&sectionStart))
        return Fail(cx, d, "expected memory section byte size");

    uint32_t initialHeapSize;
    if (!d.readVarU32(&initialHeapSize))
        return Fail(cx, d, "expected initial memory size");

    if (initialHeapSize < PageSize || initialHeapSize % PageSize != 0)
        return Fail(cx, d, "initial memory size not a multiple of 0x10000");

    if (initialHeapSize > INT32_MAX)
        return Fail(cx, d, "initial memory size too big");

    if (!d.finishSection(sectionStart))
        return Fail(cx, d, "memory section byte size mismatch");

    bool signalsForOOB = CompileArgs(cx).useSignalHandlersForOOB;
    heap.set(ArrayBufferObject::createForWasm(cx, initialHeapSize, signalsForOOB));
    if (!heap)
        return false;

    mg.initHeapUsage(HeapUsage::Unshared);
    return true;
}

typedef HashSet<const char*, CStringHasher> CStringSet;

static UniqueChars
DecodeFieldName(JSContext* cx, Decoder& d, CStringSet* dupSet)
{
    UniqueChars fieldName = d.readCString();
    if (!fieldName) {
        Fail(cx, d, "expected export external name string");
        return nullptr;
    }

    CStringSet::AddPtr p = dupSet->lookupForAdd(fieldName.get());
    if (p) {
        Fail(cx, d, "duplicate export");
        return nullptr;
    }

    if (!dupSet->add(p, fieldName.get()))
        return nullptr;

    return Move(fieldName);
}

static bool
DecodeFunctionExport(JSContext* cx, Decoder& d, ModuleGenerator& mg, CStringSet* dupSet)
{
    uint32_t funcIndex;
    if (!d.readVarU32(&funcIndex))
        return Fail(cx, d, "expected export internal index");

    if (funcIndex >= mg.numFuncSigs())
        return Fail(cx, d, "export function index out of range");

    UniqueChars fieldName = DecodeFieldName(cx, d, dupSet);
    if (!fieldName)
        return false;

    return mg.declareExport(Move(fieldName), funcIndex);
}

static bool
DecodeMemoryExport(JSContext* cx, Decoder& d, ModuleGenerator& mg, CStringSet* dupSet)
{
    if (!mg.usesHeap())
        return Fail(cx, d, "cannot export memory with no memory section");

    UniqueChars fieldName = DecodeFieldName(cx, d, dupSet);
    if (!fieldName)
        return false;

    return mg.addMemoryExport(Move(fieldName));
}

static bool
DecodeExportsSection(JSContext* cx, Decoder& d, ModuleGenerator& mg)
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

    CStringSet dupSet(cx);
    if (!dupSet.init(numExports))
        return false;

    for (uint32_t i = 0; i < numExports; i++) {
        if (d.readCStringIf(FuncSubsection)) {
            if (!DecodeFunctionExport(cx, d, mg, &dupSet))
                return false;
        } else if (d.readCStringIf(MemorySubsection)) {
            if (!DecodeMemoryExport(cx, d, mg, &dupSet))
                return false;
        } else {
            return Fail(cx, d, "unknown export type");
        }
    }

    if (!d.finishSection(sectionStart))
        return Fail(cx, d, "export section byte size mismatch");

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
        return Fail(cx, d, "different number of definitions than declarations");

    if (!mg.finishFuncDefs())
        return false;

    return true;
}

static bool
DecodeDataSection(JSContext* cx, Decoder& d, Handle<ArrayBufferObject*> heap)
{
    if (!d.readCStringIf(DataSection))
        return true;

    if (!heap)
        return Fail(cx, d, "data section requires a memory section");

    uint32_t sectionStart;
    if (!d.startSection(&sectionStart))
        return Fail(cx, d, "expected data section byte size");

    uint32_t numSegments;
    if (!d.readVarU32(&numSegments))
        return Fail(cx, d, "expected number of data segments");

    uint8_t* const heapBase = heap->dataPointer();
    uint32_t const heapLength = heap->byteLength();
    uint32_t prevEnd = 0;

    for (uint32_t i = 0; i < numSegments; i++) {
        if (!d.readCStringIf(SegmentSubsection))
            return Fail(cx, d, "expected segment tag");

        uint32_t dstOffset;
        if (!d.readVarU32(&dstOffset))
            return Fail(cx, d, "expected segment destination offset");

        if (dstOffset < prevEnd)
            return Fail(cx, d, "data segments must be disjoint and ordered");

        uint32_t numBytes;
        if (!d.readVarU32(&numBytes))
            return Fail(cx, d, "expected segment size");

        if (dstOffset > heapLength || heapLength - dstOffset < numBytes)
            return Fail(cx, d, "data segment does not fit in memory");

        const uint8_t* src;
        if (!d.readRawData(numBytes, &src))
            return Fail(cx, d, "data segment shorter than declared");

        memcpy(heapBase + dstOffset, src, numBytes);
        prevEnd = dstOffset + numBytes;
    }

    if (!d.finishSection(sectionStart))
        return Fail(cx, d, "data section byte size mismatch");

    return true;
}

static bool
DecodeUnknownSection(JSContext* cx, Decoder& d)
{
    UniqueChars sectionName = d.readCString();
    if (!sectionName)
        return Fail(cx, d, "failed to read section name");

    if (!strcmp(sectionName.get(), SigSection) ||
        !strcmp(sectionName.get(), ImportSection) ||
        !strcmp(sectionName.get(), DeclSection) ||
        !strcmp(sectionName.get(), ExportSection) ||
        !strcmp(sectionName.get(), CodeSection) ||
        !strcmp(sectionName.get(), DataSection) ||
        !strcmp(sectionName.get(), TableSection))
    {
        return Fail(cx, d, "known section out of order");
    }

    if (!d.skipSection())
        return Fail(cx, d, "unable to skip unknown section");

    return true;
}

static bool
DecodeModule(JSContext* cx, UniqueChars file, const uint8_t* bytes, uint32_t length,
             ImportNameVector* importNames, UniqueExportMap* exportMap,
             MutableHandle<ArrayBufferObject*> heap, MutableHandle<WasmModuleObject*> moduleObj)
{
    Decoder d(bytes, bytes + length);

    uint32_t u32;
    if (!d.readFixedU32(&u32) || u32 != MagicNumber)
        return Fail(cx, d, "failed to match magic number");

    if (!d.readFixedU32(&u32) || u32 != EncodingVersion)
        return Fail(cx, d, "failed to match binary version");

    UniqueModuleGeneratorData init = MakeUnique<ModuleGeneratorData>();
    if (!init)
        return false;

    if (!DecodeSignatureSection(cx, d, init.get()))
        return false;

    if (!DecodeImportSection(cx, d, init.get(), importNames))
        return false;

    if (!DecodeDeclarationSection(cx, d, init.get()))
        return false;

    if (!DecodeTableSection(cx, d, init.get()))
        return false;

    ModuleGenerator mg(cx);
    if (!mg.init(Move(init), Move(file)))
        return false;

    if (!DecodeMemorySection(cx, d, mg, heap))
        return false;

    if (!DecodeExportsSection(cx, d, mg))
        return false;

    if (!DecodeCodeSection(cx, d, mg))
        return false;

    if (!DecodeDataSection(cx, d, heap))
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
    if (!mg.finish(Move(funcNames), &module, &staticLink, exportMap, &slowFuncs))
        return false;

    moduleObj.set(WasmModuleObject::create(cx));
    if (!moduleObj)
        return false;

    if (!moduleObj->init(cx->new_<Module>(Move(module))))
        return false;

    return moduleObj->module().staticallyLink(cx, *staticLink);
}

/*****************************************************************************/
// Top-level functions

bool
wasm::HasCompilerSupport(ExclusiveContext* cx)
{
    if (!cx->jitSupportsFloatingPoint())
        return false;

#if defined(JS_CODEGEN_NONE) || defined(JS_CODEGEN_ARM64)
    return false;
#else
    return true;
#endif
}

static bool
WasmIsSupported(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setBoolean(HasCompilerSupport(cx));
    return true;
}

static bool
CheckCompilerSupport(JSContext* cx)
{
    if (!HasCompilerSupport(cx)) {
#ifdef JS_MORE_DETERMINISTIC
        fprintf(stderr, "WebAssembly is not supported on the current device.\n");
#endif
        JS_ReportError(cx, "WebAssembly is not supported on the current device.");
        return false;
    }
    return true;
}

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

bool
wasm::Eval(JSContext* cx, Handle<ArrayBufferObject*> code,
           HandleObject importObj, MutableHandleObject exportObj)
{
    if (!CheckCompilerSupport(cx))
        return false;

    const uint8_t* bytes = code->dataPointer();
    uint32_t length = code->byteLength();

    Vector<uint8_t> copy(cx);
    if (code->hasInlineData()) {
        if (!copy.append(bytes, length))
            return false;
        bytes = copy.begin();
    }

    UniqueChars file;
    if (!DescribeScriptedCaller(cx, &file))
        return false;

    ImportNameVector importNames;
    UniqueExportMap exportMap;
    Rooted<ArrayBufferObject*> heap(cx);
    Rooted<WasmModuleObject*> moduleObj(cx);
    if (!DecodeModule(cx, Move(file), bytes, length, &importNames, &exportMap, &heap, &moduleObj)) {
        if (!cx->isExceptionPending())
            ReportOutOfMemory(cx);
        return false;
    }

    Rooted<FunctionVector> imports(cx, FunctionVector(cx));
    if (!ImportFunctions(cx, importObj, importNames, &imports))
        return false;

    if (!moduleObj->module().dynamicallyLink(cx, moduleObj, heap, imports, *exportMap, exportObj))
        return false;

    return true;
}

static bool
WasmEval(JSContext* cx, unsigned argc, Value* vp)
{
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

    RootedObject importObj(cx);
    if (!args.get(1).isUndefined()) {
        if (!args.get(1).isObject()) {
            ReportUsageError(cx, callee, "Second argument, if present, must be an Object");
            return false;
        }
        importObj = &args[1].toObject();
    }

    Rooted<ArrayBufferObject*> code(cx, &args[0].toObject().as<ArrayBufferObject>());

    RootedObject exportObj(cx);
    if (!Eval(cx, code, importObj, &exportObj))
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
    UniqueBytecode bytes = TextToBinary(twoByteChars.twoByteChars(), &error);
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

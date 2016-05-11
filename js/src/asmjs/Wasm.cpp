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

#include "mozilla/CheckedInt.h"

#include "jsprf.h"

#include "asmjs/WasmBinaryIterator.h"
#include "asmjs/WasmGenerator.h"
#include "vm/ArrayBufferObject.h"
#include "vm/Debugger.h"

#include "jsatominlines.h"
#include "jsobjinlines.h"

#include "vm/Debugger-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

using mozilla::CheckedInt;
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

static bool
IsI64Implemented()
{
#ifdef JS_CPU_X64
    return true;
#else
    return false;
#endif
}

namespace {

class ValidatingPolicy : public ExprIterPolicy
{
    JSContext* cx_;

  public:
    // Validation is what we're all about here.
    static const bool Validate = true;

    // Fail by printing a message, using the contains JSContext.
    bool fail(const char* str, Decoder& d) {
        return Fail(cx_, d, str);
    }

    explicit ValidatingPolicy(JSContext* cx) : cx_(cx) {}
};

typedef ExprIter<ValidatingPolicy> ValidatingExprIter;

class FunctionDecoder
{
    const ModuleGenerator& mg_;
    ValidatingExprIter iter_;
    const ValTypeVector& locals_;
    const DeclaredSig& sig_;

  public:
    FunctionDecoder(JSContext* cx, const ModuleGenerator& mg, Decoder& d,
                    uint32_t funcIndex, const ValTypeVector& locals)
      : mg_(mg),
        iter_(ValidatingPolicy(cx), d),
        locals_(locals),
        sig_(mg.funcSig(funcIndex))
    {}
    const ModuleGenerator& mg() const { return mg_; }
    ValidatingExprIter& iter() { return iter_; }
    const ValTypeVector& locals() const { return locals_; }
    const DeclaredSig& sig() const { return sig_; }

    bool checkI64Support() {
        if (!IsI64Implemented())
            return iter().notYetImplemented("i64 NYI on this platform");
        return true;
    }
};

} // end anonymous namespace

static bool
CheckValType(JSContext* cx, Decoder& d, ValType type)
{
    switch (type) {
      case ValType::I32:
      case ValType::F32:
      case ValType::F64:
        return true;
      case ValType::I64:
#ifndef JS_CPU_X64
        return Fail(cx, d, "i64 NYI on this platform");
#endif
        return true;
      default:
        // Note: it's important not to remove this default since readValType()
        // can return ValType values for which there is no enumerator.
        break;
    }

    return Fail(cx, d, "bad type");
}

static bool
DecodeCallArgs(FunctionDecoder& f, uint32_t arity, const Sig& sig)
{
    if (arity != sig.args().length())
        return f.iter().fail("call arity out of range");

    const ValTypeVector& args = sig.args();
    uint32_t numArgs = args.length();
    for (size_t i = 0; i < numArgs; ++i) {
        ValType argType = args[i];
        if (!f.iter().readCallArg(argType, numArgs, i, nullptr))
            return false;
    }

    return f.iter().readCallArgsEnd(numArgs);
}

static bool
DecodeCallReturn(FunctionDecoder& f, const Sig& sig)
{
    return f.iter().readCallReturn(sig.ret());
}

static bool
DecodeCall(FunctionDecoder& f)
{
    uint32_t calleeIndex;
    uint32_t arity;
    if (!f.iter().readCall(&calleeIndex, &arity))
        return false;

    if (calleeIndex >= f.mg().numFuncSigs())
        return f.iter().fail("callee index out of range");

    const Sig& sig = f.mg().funcSig(calleeIndex);
    return DecodeCallArgs(f, arity, sig) &&
           DecodeCallReturn(f, sig);
}

static bool
DecodeCallIndirect(FunctionDecoder& f)
{
    uint32_t sigIndex;
    uint32_t arity;
    if (!f.iter().readCallIndirect(&sigIndex, &arity))
        return false;

    if (sigIndex >= f.mg().numSigs())
        return f.iter().fail("signature index out of range");

    const Sig& sig = f.mg().sig(sigIndex);
    if (!DecodeCallArgs(f, arity, sig))
        return false;

    if (!f.iter().readCallIndirectCallee(nullptr))
        return false;

    return DecodeCallReturn(f, sig);
}

static bool
DecodeCallImport(FunctionDecoder& f)
{
    uint32_t importIndex;
    uint32_t arity;
    if (!f.iter().readCallImport(&importIndex, &arity))
        return false;

    if (importIndex >= f.mg().numImports())
        return f.iter().fail("import index out of range");

    const Sig& sig = *f.mg().import(importIndex).sig;
    return DecodeCallArgs(f, arity, sig) &&
           DecodeCallReturn(f, sig);
}

static bool
DecodeBrTable(FunctionDecoder& f)
{
    uint32_t tableLength;
    ExprType type;
    if (!f.iter().readBrTable(&tableLength, &type, nullptr, nullptr))
        return false;

    uint32_t depth;
    for (size_t i = 0, e = tableLength; i < e; ++i) {
        if (!f.iter().readBrTableEntry(type, &depth))
            return false;
    }

    // Read the default label.
    return f.iter().readBrTableEntry(type, &depth);
}

static bool
DecodeExpr(FunctionDecoder& f)
{
    Expr expr;
    if (!f.iter().readExpr(&expr))
        return false;

    switch (expr) {
      case Expr::Nop:
        return f.iter().readNullary();
      case Expr::Call:
        return DecodeCall(f);
      case Expr::CallIndirect:
        return DecodeCallIndirect(f);
      case Expr::CallImport:
        return DecodeCallImport(f);
      case Expr::I32Const:
        return f.iter().readI32Const(nullptr);
      case Expr::I64Const:
        return f.checkI64Support() &&
               f.iter().readI64Const(nullptr);
      case Expr::F32Const:
        return f.iter().readF32Const(nullptr);
      case Expr::F64Const:
        return f.iter().readF64Const(nullptr);
      case Expr::GetLocal:
        return f.iter().readGetLocal(f.locals(), nullptr);
      case Expr::SetLocal:
        return f.iter().readSetLocal(f.locals(), nullptr, nullptr);
      case Expr::Select:
        return f.iter().readSelect(nullptr, nullptr, nullptr, nullptr);
      case Expr::Block:
        return f.iter().readBlock();
      case Expr::Loop:
        return f.iter().readLoop();
      case Expr::If:
        return f.iter().readIf(nullptr);
      case Expr::Else:
        return f.iter().readElse(nullptr, nullptr);
      case Expr::End:
        return f.iter().readEnd(nullptr, nullptr, nullptr);
      case Expr::I32Clz:
      case Expr::I32Ctz:
      case Expr::I32Popcnt:
        return f.iter().readUnary(ValType::I32, nullptr);
      case Expr::I64Clz:
      case Expr::I64Ctz:
      case Expr::I64Popcnt:
        return f.checkI64Support() &&
               f.iter().readUnary(ValType::I64, nullptr);
      case Expr::F32Abs:
      case Expr::F32Neg:
      case Expr::F32Ceil:
      case Expr::F32Floor:
      case Expr::F32Sqrt:
        return f.iter().readUnary(ValType::F32, nullptr);
      case Expr::F32Trunc:
        return f.iter().notYetImplemented("trunc");
      case Expr::F32Nearest:
        return f.iter().notYetImplemented("nearest");
      case Expr::F64Abs:
      case Expr::F64Neg:
      case Expr::F64Ceil:
      case Expr::F64Floor:
      case Expr::F64Sqrt:
        return f.iter().readUnary(ValType::F64, nullptr);
      case Expr::F64Trunc:
        return f.iter().notYetImplemented("trunc");
      case Expr::F64Nearest:
        return f.iter().notYetImplemented("nearest");
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
      case Expr::I32Rotl:
      case Expr::I32Rotr:
        return f.iter().readBinary(ValType::I32, nullptr, nullptr);
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
      case Expr::I64Rotl:
      case Expr::I64Rotr:
        return f.checkI64Support() &&
               f.iter().readBinary(ValType::I64, nullptr, nullptr);
      case Expr::F32Add:
      case Expr::F32Sub:
      case Expr::F32Mul:
      case Expr::F32Div:
      case Expr::F32Min:
      case Expr::F32Max:
        return f.iter().readBinary(ValType::F32, nullptr, nullptr);
      case Expr::F32CopySign:
        return f.iter().notYetImplemented("copysign");
      case Expr::F64Add:
      case Expr::F64Sub:
      case Expr::F64Mul:
      case Expr::F64Div:
      case Expr::F64Min:
      case Expr::F64Max:
        return f.iter().readBinary(ValType::F64, nullptr, nullptr);
      case Expr::F64CopySign:
        return f.iter().notYetImplemented("copysign");
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
        return f.iter().readComparison(ValType::I32, nullptr, nullptr);
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
        return f.checkI64Support() &&
               f.iter().readComparison(ValType::I64, nullptr, nullptr);
      case Expr::F32Eq:
      case Expr::F32Ne:
      case Expr::F32Lt:
      case Expr::F32Le:
      case Expr::F32Gt:
      case Expr::F32Ge:
        return f.iter().readComparison(ValType::F32, nullptr, nullptr);
      case Expr::F64Eq:
      case Expr::F64Ne:
      case Expr::F64Lt:
      case Expr::F64Le:
      case Expr::F64Gt:
      case Expr::F64Ge:
        return f.iter().readComparison(ValType::F64, nullptr, nullptr);
      case Expr::I32Eqz:
        return f.iter().readConversion(ValType::I32, ValType::I32, nullptr);
      case Expr::I64Eqz:
      case Expr::I32WrapI64:
        return f.checkI64Support() &&
               f.iter().readConversion(ValType::I64, ValType::I32, nullptr);
      case Expr::I32TruncSF32:
      case Expr::I32TruncUF32:
      case Expr::I32ReinterpretF32:
        return f.iter().readConversion(ValType::F32, ValType::I32, nullptr);
      case Expr::I32TruncSF64:
      case Expr::I32TruncUF64:
        return f.iter().readConversion(ValType::F64, ValType::I32, nullptr);
      case Expr::I64ExtendSI32:
      case Expr::I64ExtendUI32:
        return f.checkI64Support() &&
               f.iter().readConversion(ValType::I32, ValType::I64, nullptr);
      case Expr::I64TruncSF32:
      case Expr::I64TruncUF32:
        return f.checkI64Support() &&
               f.iter().readConversion(ValType::F32, ValType::I64, nullptr);
      case Expr::I64TruncSF64:
      case Expr::I64TruncUF64:
      case Expr::I64ReinterpretF64:
        return f.checkI64Support() &&
               f.iter().readConversion(ValType::F64, ValType::I64, nullptr);
      case Expr::F32ConvertSI32:
      case Expr::F32ConvertUI32:
      case Expr::F32ReinterpretI32:
        return f.iter().readConversion(ValType::I32, ValType::F32, nullptr);
      case Expr::F32ConvertSI64:
      case Expr::F32ConvertUI64:
        return f.checkI64Support() &&
               f.iter().readConversion(ValType::I64, ValType::F32, nullptr);
      case Expr::F32DemoteF64:
        return f.iter().readConversion(ValType::F64, ValType::F32, nullptr);
      case Expr::F64ConvertSI32:
      case Expr::F64ConvertUI32:
        return f.iter().readConversion(ValType::I32, ValType::F64, nullptr);
      case Expr::F64ConvertSI64:
      case Expr::F64ConvertUI64:
      case Expr::F64ReinterpretI64:
        return f.checkI64Support() &&
               f.iter().readConversion(ValType::I64, ValType::F64, nullptr);
      case Expr::F64PromoteF32:
        return f.iter().readConversion(ValType::F32, ValType::F64, nullptr);
      case Expr::I32Load8S:
      case Expr::I32Load8U:
        return f.iter().readLoad(ValType::I32, 1, nullptr);
      case Expr::I32Load16S:
      case Expr::I32Load16U:
        return f.iter().readLoad(ValType::I32, 2, nullptr);
      case Expr::I32Load:
        return f.iter().readLoad(ValType::I32, 4, nullptr);
      case Expr::I64Load8S:
      case Expr::I64Load8U:
        return f.iter().notYetImplemented("i64") &&
               f.iter().readLoad(ValType::I64, 1, nullptr);
      case Expr::I64Load16S:
      case Expr::I64Load16U:
        return f.iter().notYetImplemented("i64") &&
               f.iter().readLoad(ValType::I64, 2, nullptr);
      case Expr::I64Load32S:
      case Expr::I64Load32U:
        return f.iter().notYetImplemented("i64") &&
               f.iter().readLoad(ValType::I64, 4, nullptr);
      case Expr::I64Load:
        return f.iter().notYetImplemented("i64");
      case Expr::F32Load:
        return f.iter().readLoad(ValType::F32, 4, nullptr);
      case Expr::F64Load:
        return f.iter().readLoad(ValType::F64, 8, nullptr);
      case Expr::I32Store8:
        return f.iter().readStore(ValType::I32, 1, nullptr, nullptr);
      case Expr::I32Store16:
        return f.iter().readStore(ValType::I32, 2, nullptr, nullptr);
      case Expr::I32Store:
        return f.iter().readStore(ValType::I32, 4, nullptr, nullptr);
      case Expr::I64Store8:
        return f.iter().notYetImplemented("i64") &&
               f.iter().readStore(ValType::I64, 1, nullptr, nullptr);
      case Expr::I64Store16:
        return f.iter().notYetImplemented("i64") &&
               f.iter().readStore(ValType::I64, 2, nullptr, nullptr);
      case Expr::I64Store32:
        return f.iter().notYetImplemented("i64") &&
               f.iter().readStore(ValType::I64, 4, nullptr, nullptr);
      case Expr::I64Store:
        return f.iter().notYetImplemented("i64");
      case Expr::F32Store:
        return f.iter().readStore(ValType::F32, 4, nullptr, nullptr);
      case Expr::F64Store:
        return f.iter().readStore(ValType::F64, 8, nullptr, nullptr);
      case Expr::Br:
        return f.iter().readBr(nullptr, nullptr, nullptr);
      case Expr::BrIf:
        return f.iter().readBrIf(nullptr, nullptr, nullptr, nullptr);
      case Expr::BrTable:
        return DecodeBrTable(f);
      case Expr::Return:
        return f.iter().readReturn(nullptr);
      case Expr::Unreachable:
        return f.iter().readUnreachable();
      default:
        // Note: it's important not to remove this default since readExpr()
        // can return Expr values for which there is no enumerator.
        break;
    }

    return f.iter().unrecognizedOpcode(expr);
}

/*****************************************************************************/
// wasm decoding and generation

static bool
DecodeTypeSection(JSContext* cx, Decoder& d, ModuleGeneratorData* init)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(TypeSectionId, &sectionStart, &sectionSize))
        return Fail(cx, d, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numSigs;
    if (!d.readVarU32(&numSigs))
        return Fail(cx, d, "expected number of signatures");

    if (numSigs > MaxSigs)
        return Fail(cx, d, "too many signatures");

    if (!init->sigs.resize(numSigs))
        return false;
    if (!init->sigToTable.resize(numSigs))
        return false;

    for (uint32_t sigIndex = 0; sigIndex < numSigs; sigIndex++) {
        uint32_t form;
        if (!d.readVarU32(&form) || form != uint32_t(TypeConstructor::Function))
            return Fail(cx, d, "expected function form");

        uint32_t numArgs;
        if (!d.readVarU32(&numArgs))
            return Fail(cx, d, "bad number of function args");

        if (numArgs > MaxArgsPerFunc)
            return Fail(cx, d, "too many arguments in signature");

        ValTypeVector args;
        if (!args.resize(numArgs))
            return false;

        for (uint32_t i = 0; i < numArgs; i++) {
            if (!d.readValType(&args[i]))
                return Fail(cx, d, "bad value type");

            if (!CheckValType(cx, d, args[i]))
                return false;
        }

        uint32_t numRets;
        if (!d.readVarU32(&numRets))
            return Fail(cx, d, "bad number of function returns");

        if (numRets > 1)
            return Fail(cx, d, "too many returns in signature");

        ExprType result = ExprType::Void;

        if (numRets == 1) {
            ValType type;
            if (!d.readValType(&type))
                return Fail(cx, d, "bad expression type");

            if (!CheckValType(cx, d, type))
                return false;

            result = ToExprType(type);
        }

        init->sigs[sigIndex] = Sig(Move(args), result);
    }

    if (!d.finishSection(sectionStart, sectionSize))
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
DecodeFunctionSection(JSContext* cx, Decoder& d, ModuleGeneratorData* init)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(FunctionSectionId, &sectionStart, &sectionSize))
        return Fail(cx, d, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

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

    if (!d.finishSection(sectionStart, sectionSize))
        return Fail(cx, d, "decls section byte size mismatch");

    return true;
}

static bool
DecodeTableSection(JSContext* cx, Decoder& d, ModuleGeneratorData* init)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(TableSectionId, &sectionStart, &sectionSize))
        return Fail(cx, d, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

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

    if (!d.finishSection(sectionStart, sectionSize))
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
CheckTypeForJS(JSContext* cx, Decoder& d, const Sig& sig)
{
    bool allowI64 = IsI64Implemented() && JitOptions.wasmTestMode;

    for (ValType argType : sig.args()) {
        if (argType == ValType::I64 && !allowI64)
            return Fail(cx, d, "cannot import/export i64 argument");
        if (IsSimdType(argType))
            return Fail(cx, d, "cannot import/export SIMD argument");
    }

    if (sig.ret() == ExprType::I64 && !allowI64)
        return Fail(cx, d, "cannot import/export i64 return type");
    if (IsSimdType(sig.ret()))
        return Fail(cx, d, "cannot import/export SIMD return type");

    return true;
}

struct ImportName
{
    Bytes module;
    Bytes func;

    ImportName(Bytes&& module, Bytes&& func)
      : module(Move(module)), func(Move(func))
    {}
    ImportName(ImportName&& rhs)
      : module(Move(rhs.module)), func(Move(rhs.func))
    {}
};

typedef Vector<ImportName, 0, SystemAllocPolicy> ImportNameVector;

static bool
DecodeImport(JSContext* cx, Decoder& d, ModuleGeneratorData* init, ImportNameVector* importNames)
{
    const DeclaredSig* sig = nullptr;
    if (!DecodeSignatureIndex(cx, d, *init, &sig))
        return false;

    if (!init->imports.emplaceBack(sig))
        return false;

    if (!CheckTypeForJS(cx, d, *sig))
        return false;

    Bytes moduleName;
    if (!d.readBytes(&moduleName))
        return Fail(cx, d, "expected import module name");

    if (moduleName.empty())
        return Fail(cx, d, "module name cannot be empty");

    Bytes funcName;
    if (!d.readBytes(&funcName))
        return Fail(cx, d, "expected import func name");

    return importNames->emplaceBack(Move(moduleName), Move(funcName));
}

static bool
DecodeImportSection(JSContext* cx, Decoder& d, ModuleGeneratorData* init, ImportNameVector* importNames)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(ImportSectionId, &sectionStart, &sectionSize))
        return Fail(cx, d, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numImports;
    if (!d.readVarU32(&numImports))
        return Fail(cx, d, "failed to read number of imports");

    if (numImports > MaxImports)
        return Fail(cx, d, "too many imports");

    for (uint32_t i = 0; i < numImports; i++) {
        if (!DecodeImport(cx, d, init, importNames))
            return false;
    }

    if (!d.finishSection(sectionStart, sectionSize))
        return Fail(cx, d, "import section byte size mismatch");

    return true;
}

static bool
DecodeMemorySection(JSContext* cx, Decoder& d, ModuleGenerator& mg, MutableHandle<ArrayBufferObject*> heap)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(MemorySectionId, &sectionStart, &sectionSize))
        return Fail(cx, d, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t initialSizePages;
    if (!d.readVarU32(&initialSizePages))
        return Fail(cx, d, "expected initial memory size");

    CheckedInt<int32_t> initialSize = initialSizePages;
    initialSize *= PageSize;
    if (!initialSize.isValid())
        return Fail(cx, d, "initial memory size too big");

    uint32_t maxSizePages;
    if (!d.readVarU32(&maxSizePages))
        return Fail(cx, d, "expected initial memory size");

    CheckedInt<int32_t> maxSize = maxSizePages;
    maxSize *= PageSize;
    if (!maxSize.isValid())
        return Fail(cx, d, "initial memory size too big");

    uint8_t exported;
    if (!d.readFixedU8(&exported))
        return Fail(cx, d, "expected exported byte");

    if (exported) {
        UniqueChars fieldName = DuplicateString("memory");
        if (!fieldName || !mg.addMemoryExport(Move(fieldName)))
            return false;
    }

    if (!d.finishSection(sectionStart, sectionSize))
        return Fail(cx, d, "memory section byte size mismatch");

    bool signalsForOOB = CompileArgs(cx).useSignalHandlersForOOB;
    heap.set(ArrayBufferObject::createForWasm(cx, initialSize.value(), signalsForOOB));
    if (!heap)
        return false;

    mg.initHeapUsage(HeapUsage::Unshared, initialSize.value());
    return true;
}

typedef HashSet<const char*, CStringHasher> CStringSet;

static UniqueChars
DecodeExportName(JSContext* cx, Decoder& d, CStringSet* dupSet)
{
    Bytes fieldBytes;
    if (!d.readBytes(&fieldBytes)) {
        Fail(cx, d, "expected export name");
        return nullptr;
    }

    if (memchr(fieldBytes.begin(), 0, fieldBytes.length())) {
        Fail(cx, d, "null in export names not yet supported");
        return nullptr;
    }

    if (!fieldBytes.append(0))
        return nullptr;

    UniqueChars fieldName((char*)fieldBytes.extractOrCopyRawBuffer());
    if (!fieldName)
        return nullptr;

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

    if (!CheckTypeForJS(cx, d, mg.funcSig(funcIndex)))
        return false;

    UniqueChars fieldName = DecodeExportName(cx, d, dupSet);
    if (!fieldName)
        return false;

    return mg.declareExport(Move(fieldName), funcIndex);
}

static bool
DecodeExportSection(JSContext* cx, Decoder& d, ModuleGenerator& mg)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(ExportSectionId, &sectionStart, &sectionSize))
        return Fail(cx, d, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

    CStringSet dupSet(cx);
    if (!dupSet.init())
        return false;

    uint32_t numExports;
    if (!d.readVarU32(&numExports))
        return Fail(cx, d, "failed to read number of exports");

    if (numExports > MaxExports)
        return Fail(cx, d, "too many exports");

    for (uint32_t i = 0; i < numExports; i++) {
        if (!DecodeFunctionExport(cx, d, mg, &dupSet))
            return false;
    }

    if (!d.finishSection(sectionStart, sectionSize))
        return Fail(cx, d, "export section byte size mismatch");

    return true;
}

static bool
DecodeFunctionBody(JSContext* cx, Decoder& d, ModuleGenerator& mg, uint32_t funcIndex)
{
    int64_t before = PRMJ_Now();

    uint32_t bodySize;
    if (!d.readVarU32(&bodySize))
        return Fail(cx, d, "expected number of function body bytes");

    if (d.bytesRemain() < bodySize)
        return Fail(cx, d, "function body length too big");

    const uint8_t* bodyBegin = d.currentPosition();
    const uint8_t* bodyEnd = bodyBegin + bodySize;

    FunctionGenerator fg;
    if (!mg.startFuncDef(d.currentOffset(), &fg))
        return false;

    ValTypeVector locals;
    if (!locals.appendAll(mg.funcSig(funcIndex).args()))
        return false;

    if (!DecodeLocalEntries(d, &locals))
        return Fail(cx, d, "failed decoding local entries");

    for (ValType type : locals) {
        if (!CheckValType(cx, d, type))
            return false;
    }

    FunctionDecoder f(cx, mg, d, funcIndex, locals);

    if (!f.iter().readFunctionStart())
        return false;

    while (d.currentPosition() < bodyEnd) {
        if (!DecodeExpr(f))
            return false;
    }

    if (!f.iter().readFunctionEnd(f.sig().ret(), nullptr))
        return false;

    if (d.currentPosition() != bodyEnd)
        return Fail(cx, d, "function body length mismatch");

    if (!fg.bytes().resize(bodySize))
        return false;

    memcpy(fg.bytes().begin(), bodyBegin, bodySize);

    int64_t after = PRMJ_Now();
    unsigned generateTime = (after - before) / PRMJ_USEC_PER_MSEC;

    return mg.finishFuncDef(funcIndex, generateTime, &fg);
}

static bool
DecodeCodeSection(JSContext* cx, Decoder& d, ModuleGenerator& mg)
{
    if (!mg.startFuncDefs())
        return false;

    uint32_t sectionStart, sectionSize;
    if (!d.startSection(CodeSectionId, &sectionStart, &sectionSize))
        return Fail(cx, d, "failed to start section");

    if (sectionStart == Decoder::NotStarted) {
        if (mg.numFuncSigs() != 0)
            return Fail(cx, d, "expected function bodies");

        return mg.finishFuncDefs();
    }

    uint32_t numFuncBodies;
    if (!d.readVarU32(&numFuncBodies))
        return Fail(cx, d, "expected function body count");

    if (numFuncBodies != mg.numFuncSigs())
        return Fail(cx, d, "function body count does not match function signature count");

    for (uint32_t funcIndex = 0; funcIndex < numFuncBodies; funcIndex++) {
        if (!DecodeFunctionBody(cx, d, mg, funcIndex))
            return false;
    }

    if (!d.finishSection(sectionStart, sectionSize))
        return Fail(cx, d, "function section byte size mismatch");

    return mg.finishFuncDefs();
}

static bool
DecodeDataSection(JSContext* cx, Decoder& d, Handle<ArrayBufferObject*> heap)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(DataSectionId, &sectionStart, &sectionSize))
        return Fail(cx, d, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

    if (!heap)
        return Fail(cx, d, "data section requires a memory section");

    uint32_t numSegments;
    if (!d.readVarU32(&numSegments))
        return Fail(cx, d, "failed to read number of data segments");

    uint8_t* const heapBase = heap->dataPointer();
    uint32_t const heapLength = heap->byteLength();
    uint32_t prevEnd = 0;

    for (uint32_t i = 0; i < numSegments; i++) {
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
        if (!d.readBytesRaw(numBytes, &src))
            return Fail(cx, d, "data segment shorter than declared");

        memcpy(heapBase + dstOffset, src, numBytes);
        prevEnd = dstOffset + numBytes;
    }

    if (!d.finishSection(sectionStart, sectionSize))
        return Fail(cx, d, "data section byte size mismatch");

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

    UniqueModuleGeneratorData init = js::MakeUnique<ModuleGeneratorData>(cx);
    if (!init)
        return false;

    if (!DecodeTypeSection(cx, d, init.get()))
        return false;

    if (!DecodeImportSection(cx, d, init.get(), importNames))
        return false;

    if (!DecodeFunctionSection(cx, d, init.get()))
        return false;

    if (!DecodeTableSection(cx, d, init.get()))
        return false;

    ModuleGenerator mg(cx);
    if (!mg.init(Move(init), Move(file)))
        return false;

    if (!DecodeMemorySection(cx, d, mg, heap))
        return false;

    if (!DecodeExportSection(cx, d, mg))
        return false;

    if (!DecodeCodeSection(cx, d, mg))
        return false;

    if (!DecodeDataSection(cx, d, heap))
        return false;

    CacheableCharsVector funcNames;

    while (!d.done()) {
        if (!d.skipSection())
            return Fail(cx, d, "failed to skip unknown section at end");
    }

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
GetProperty(JSContext* cx, HandleObject obj, const Bytes& bytes, MutableHandleValue v)
{
    JSAtom* atom = AtomizeUTF8Chars(cx, (char*)bytes.begin(), bytes.length());
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
        if (!GetProperty(cx, importObj, name.module, &v))
            return false;

        if (!name.func.empty()) {
            if (!v.isObject())
                return Fail(cx, "import object field is not an Object");

            RootedObject obj(cx, &v.toObject());
            if (!GetProperty(cx, obj, name.func, &v))
                return false;
        }

        if (!IsFunctionObject(v))
            return Fail(cx, "import object field is not a Function");

        if (!imports.append(&v.toObject().as<JSFunction>()))
            return false;
    }

    return true;
}

static const char ExportField[] = "exports";

static bool
CreateInstance(JSContext* cx, HandleObject exportObj, MutableHandleObject instance)
{
    instance.set(JS_NewPlainObject(cx));
    if (!instance)
        return false;

    JSAtom* atom = Atomize(cx, ExportField, strlen(ExportField));
    if (!atom)
        return false;

    RootedId id(cx, AtomToId(atom));
    RootedValue val(cx, ObjectValue(*exportObj));
    if (!JS_DefinePropertyById(cx, instance, id, val, JSPROP_ENUMERATE))
        return false;

    return true;
}

bool
wasm::Eval(JSContext* cx, Handle<TypedArrayObject*> code, HandleObject importObj,
           MutableHandleObject instance)
{
    MOZ_ASSERT(!code->isSharedMemory());

    if (!CheckCompilerSupport(cx))
        return false;

    if (!TypedArrayObject::ensureHasBuffer(cx, code))
        return false;

    const uint8_t* bufferStart = code->bufferUnshared()->dataPointer();
    const uint8_t* bytes = bufferStart + code->byteOffset();
    uint32_t length = code->byteLength();

    Vector<uint8_t> copy(cx);
    if (code->bufferUnshared()->hasInlineData()) {
        if (!copy.append(bytes, length))
            return false;
        bytes = copy.begin();
    }

    JS::AutoFilename filename;
    if (!DescribeScriptedCaller(cx, &filename))
        return false;

    UniqueChars file = DuplicateString(filename.get());
    if (!file)
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

    Module& module = moduleObj->module();

    RootedObject exportObj(cx);
    if (!module.dynamicallyLink(cx, moduleObj, heap, imports, *exportMap, &exportObj))
        return false;

    if (!CreateInstance(cx, exportObj, instance))
        return false;

    if (cx->compartment()->debuggerObservesAsmJS()) {
        Bytes source;
        if (!source.append(bytes, length))
            return false;
        module.setSource(Move(source));
    }

    Debugger::onNewWasmModule(cx, moduleObj);
    return true;
}

static bool
InstantiateModule(JSContext* cx, unsigned argc, Value* vp)
{
    MOZ_ASSERT(cx->runtime()->options().wasm());
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!args.get(0).isObject() || !args.get(0).toObject().is<TypedArrayObject>()) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_BUF_ARG);
        return false;
    }

    Rooted<TypedArrayObject*> code(cx, &args[0].toObject().as<TypedArrayObject>());
    if (code->isSharedMemory()) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_BUF_ARG);
        return false;
    }

    RootedObject importObj(cx);
    if (!args.get(1).isUndefined()) {
        if (!args.get(1).isObject()) {
            JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_IMPORT_ARG);
            return false;
        }
        importObj = &args[1].toObject();
    }

    RootedObject exportObj(cx);
    if (!Eval(cx, code, importObj, &exportObj))
        return false;

    args.rval().setObject(*exportObj);
    return true;
}

#if JS_HAS_TOSOURCE
static bool
wasm_toSource(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setString(cx->names().Wasm);
    return true;
}
#endif

static const JSFunctionSpec wasm_static_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,     wasm_toSource,     0, 0),
#endif
    JS_FN("instantiateModule", InstantiateModule, 1, 0),
    JS_FS_END
};

const Class js::WasmClass = {
    js_Wasm_str,
    JSCLASS_HAS_CACHED_PROTO(JSProto_Wasm)
};

JSObject*
js::InitWasmClass(JSContext* cx, HandleObject global)
{
    MOZ_ASSERT(cx->runtime()->options().wasm());

    RootedObject proto(cx, global->as<GlobalObject>().getOrCreateObjectPrototype(cx));
    if (!proto)
        return nullptr;

    RootedObject Wasm(cx, NewObjectWithGivenProto(cx, &WasmClass, proto, SingletonObject));
    if (!Wasm)
        return nullptr;

    if (!JS_DefineProperty(cx, global, js_Wasm_str, Wasm, JSPROP_RESOLVING))
        return nullptr;

    RootedValue version(cx, Int32Value(EncodingVersion));
    if (!JS_DefineProperty(cx, Wasm, "experimentalVersion", version, JSPROP_RESOLVING))
        return nullptr;

    if (!JS_DefineFunctions(cx, Wasm, wasm_static_methods))
        return nullptr;

    global->as<GlobalObject>().setConstructor(JSProto_Wasm, ObjectValue(*Wasm));
    return Wasm;
}

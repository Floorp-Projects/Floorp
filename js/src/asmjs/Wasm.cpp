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

#include "asmjs/WasmGenerator.h"
#include "asmjs/WasmText.h"
#include "vm/ArrayBufferObject.h"

#include "jsatominlines.h"
#include "jsobjinlines.h"

using namespace js;
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

/*****************************************************************************/
// wasm validation type lattice

// ExprType::Limit is an out-of-band value and has no wasm-semantic meaning. For
// the purpose of recursive validation, we use this value to represent the type
// of branch/return instructions that don't actually return to the parent
// expression and can thus be used in any context.
static const ExprType AnyType = ExprType::Limit;

static ExprType
Unify(ExprType one, ExprType two)
{
    if (one == AnyType)
        return two;
    if (two == AnyType)
        return one;
    if (one == two)
        return one;
    return ExprType::Void;
}

class FunctionDecoder
{
    JSContext* cx_;
    Decoder& d_;
    ModuleGenerator& mg_;
    FunctionGenerator& fg_;
    uint32_t funcIndex_;
    const ValTypeVector& locals_;
    Vector<ExprType> blocks_;

  public:
    FunctionDecoder(JSContext* cx, Decoder& d, ModuleGenerator& mg, FunctionGenerator& fg,
                    uint32_t funcIndex, const ValTypeVector& locals)
      : cx_(cx), d_(d), mg_(mg), fg_(fg), funcIndex_(funcIndex), locals_(locals), blocks_(cx)
    {}
    JSContext* cx() const { return cx_; }
    Decoder& d() const { return d_; }
    ModuleGenerator& mg() const { return mg_; }
    FunctionGenerator& fg() const { return fg_; }
    uint32_t funcIndex() const { return funcIndex_; }
    const ValTypeVector& locals() const { return locals_; }
    const DeclaredSig& sig() const { return mg_.funcSig(funcIndex_); }

    bool fail(const char* str) {
        return Fail(cx_, d_, str);
    }

    MOZ_WARN_UNUSED_RESULT bool pushBlock() {
        return blocks_.append(AnyType);
    }
    ExprType popBlock() {
        return blocks_.popCopy();
    }
    MOZ_WARN_UNUSED_RESULT bool branchWithType(uint32_t depth, ExprType type) {
        if (depth >= blocks_.length())
            return false;
        uint32_t absolute = blocks_.length() - 1 - depth;
        blocks_[absolute] = Unify(blocks_[absolute], type);
        return true;
    }
};

static bool
CheckType(FunctionDecoder& f, ExprType actual, ValType expected)
{
    if (actual == AnyType || actual == ToExprType(expected))
        return true;

    UniqueChars error(JS_smprintf("type mismatch: expression has type %s but expected %s",
                                  ToCString(actual), ToCString(expected)));
    if (!error)
        return false;

    return f.fail(error.get());
}

static bool
CheckType(FunctionDecoder& f, ExprType actual, ExprType expected)
{
    MOZ_ASSERT(expected != AnyType);
    return expected == ExprType::Void ||
           CheckType(f, actual, NonVoidToValType(expected));
}

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

    return Fail(cx, d, "bad value type");
}

static bool
CheckExprType(JSContext* cx, Decoder& d, ExprType type)
{
    return type == ExprType::Void ||
           CheckValType(cx, d, NonVoidToValType(type));
}

static bool
DecodeExpr(FunctionDecoder& f, ExprType* type);

static bool
DecodeNop(FunctionDecoder& f, ExprType* type)
{
    *type = ExprType::Void;
    return true;
}

static bool
DecodeCallWithSig(FunctionDecoder& f, const Sig& sig, ExprType* type)
{
    for (ValType argType : sig.args()) {
        ExprType exprType;
        if (!DecodeExpr(f, &exprType))
            return false;

        if (!CheckType(f, exprType, argType))
            return false;
    }

    *type = sig.ret();
    return true;
}

static bool
DecodeCall(FunctionDecoder& f, ExprType* type)
{
    uint32_t funcIndex;
    if (!f.d().readVarU32(&funcIndex))
        return f.fail("unable to read import index");

    if (funcIndex >= f.mg().numFuncSigs())
        return f.fail("callee index out of range");

    return DecodeCallWithSig(f, f.mg().funcSig(funcIndex), type);
}

static bool
DecodeCallImport(FunctionDecoder& f, ExprType* type)
{
    uint32_t importIndex;
    if (!f.d().readVarU32(&importIndex))
        return f.fail("unable to read import index");

    if (importIndex >= f.mg().numImports())
        return f.fail("import index out of range");

    return DecodeCallWithSig(f, *f.mg().import(importIndex).sig, type);
}

static bool
DecodeCallIndirect(FunctionDecoder& f, ExprType* type)
{
    uint32_t sigIndex;
    if (!f.d().readVarU32(&sigIndex))
        return f.fail("unable to read indirect call signature index");

    if (sigIndex >= f.mg().numSigs())
        return f.fail("signature index out of range");

    ExprType indexType;
    if (!DecodeExpr(f, &indexType))
        return false;

    if (!CheckType(f, indexType, ValType::I32))
        return false;

    return DecodeCallWithSig(f, f.mg().sig(sigIndex), type);
}

static bool
DecodeConstI32(FunctionDecoder& f, ExprType* type)
{
    uint32_t _;
    if (!f.d().readVarU32(&_))
        return f.fail("unable to read i32.const immediate");

    *type = ExprType::I32;
    return true;
}

static bool
DecodeConstI64(FunctionDecoder& f, ExprType* type)
{
    uint64_t _;
    if (!f.d().readVarU64(&_))
        return f.fail("unable to read i64.const immediate");

    *type = ExprType::I64;
    return true;
}

static bool
DecodeConstF32(FunctionDecoder& f, ExprType* type)
{
    float value;
    if (!f.d().readFixedF32(&value))
        return f.fail("unable to read f32.const immediate");

    if (IsNaN(value)) {
        const float jsNaN = (float)JS::GenericNaN();
        if (memcmp(&value, &jsNaN, sizeof(value)) != 0)
            return f.fail("NYI: NaN literals with custom payloads");
    }

    *type = ExprType::F32;
    return true;
}

static bool
DecodeConstF64(FunctionDecoder& f, ExprType* type)
{
    double value;
    if (!f.d().readFixedF64(&value))
        return f.fail("unable to read f64.const immediate");

    if (IsNaN(value)) {
        const double jsNaN = JS::GenericNaN();
        if (memcmp(&value, &jsNaN, sizeof(value)) != 0)
            return f.fail("NYI: NaN literals with custom payloads");
    }

    *type = ExprType::F64;
    return true;
}

static bool
DecodeGetLocal(FunctionDecoder& f, ExprType* type)
{
    uint32_t localIndex;
    if (!f.d().readVarU32(&localIndex))
        return f.fail("unable to read get_local index");

    if (localIndex >= f.locals().length())
        return f.fail("get_local index out of range");

    *type = ToExprType(f.locals()[localIndex]);
    return true;
}

static bool
DecodeSetLocal(FunctionDecoder& f, ExprType* type)
{
    uint32_t localIndex;
    if (!f.d().readVarU32(&localIndex))
        return f.fail("unable to read set_local index");

    if (localIndex >= f.locals().length())
        return f.fail("set_local index out of range");

    *type = ToExprType(f.locals()[localIndex]);

    ExprType rhsType;
    if (!DecodeExpr(f, &rhsType))
        return false;

    return CheckType(f, rhsType, *type);
}

static bool
DecodeBlock(FunctionDecoder& f, bool isLoop, ExprType* type)
{
    if (!f.pushBlock())
        return f.fail("nesting overflow");

    if (isLoop) {
        if (!f.pushBlock())
            return f.fail("nesting overflow");
    }

    uint32_t numExprs;
    if (!f.d().readVarU32(&numExprs))
        return f.fail("unable to read block's number of expressions");

    ExprType exprType = ExprType::Void;

    for (uint32_t i = 0; i < numExprs; i++) {
        if (!DecodeExpr(f, &exprType))
            return false;
    }

    if (isLoop)
        f.popBlock();

    ExprType branchType = f.popBlock();
    *type = Unify(branchType, exprType);
    return true;
}

static bool
DecodeUnaryOperator(FunctionDecoder& f, ValType argType, ExprType *type)
{
    ExprType actual;
    if (!DecodeExpr(f, &actual))
        return false;

    if (!CheckType(f, actual, argType))
        return false;

    *type = ToExprType(argType);
    return true;
}

static bool
DecodeBinaryOperator(FunctionDecoder& f, ValType argType, ExprType* type)
{
    ExprType actual;

    if (!DecodeExpr(f, &actual))
        return false;

    if (!CheckType(f, actual, argType))
        return false;

    if (!DecodeExpr(f, &actual))
        return false;

    if (!CheckType(f, actual, argType))
        return false;

    *type = ToExprType(argType);
    return true;
}

static bool
DecodeComparisonOperator(FunctionDecoder& f, ValType argType, ExprType* type)
{
    ExprType actual;

    if (!DecodeExpr(f, &actual))
        return false;

    if (!CheckType(f, actual, argType))
        return false;

    if (!DecodeExpr(f, &actual))
        return false;

    if (!CheckType(f, actual, argType))
        return false;

    *type = ExprType::I32;
    return true;
}

static bool
DecodeConversionOperator(FunctionDecoder& f, ValType to, ValType argType, ExprType* type)
{
    ExprType actual;
    if (!DecodeExpr(f, &actual))
        return false;

    if (!CheckType(f, actual, argType))
        return false;

    *type = ToExprType(to);
    return true;
}

static bool
DecodeIfElse(FunctionDecoder& f, bool hasElse, ExprType* type)
{
    ExprType condType;
    if (!DecodeExpr(f, &condType))
        return false;

    if (!CheckType(f, condType, ValType::I32))
        return false;

    ExprType thenType;
    if (!DecodeExpr(f, &thenType))
        return false;

    if (hasElse) {
        ExprType elseType;
        if (!DecodeExpr(f, &elseType))
            return false;

        *type = Unify(thenType, elseType);
    } else {
        *type = ExprType::Void;
    }

    return true;
}

static bool
DecodeLoadStoreAddress(FunctionDecoder &f)
{
    uint32_t offset;
    if (!f.d().readVarU32(&offset))
        return f.fail("expected memory access offset");

    uint32_t align;
    if (!f.d().readVarU32(&align))
        return f.fail("expected memory access alignment");

    if (!mozilla::IsPowerOfTwo(align))
        return f.fail("memory access alignment must be a power of two");

    ExprType baseType;
    if (!DecodeExpr(f, &baseType))
        return false;

    return CheckType(f, baseType, ExprType::I32);
}

static bool
DecodeLoad(FunctionDecoder& f, ValType loadType, ExprType* type)
{
    if (!DecodeLoadStoreAddress(f))
        return false;

    *type = ToExprType(loadType);
    return true;
}

static bool
DecodeStore(FunctionDecoder& f, ValType storeType, ExprType* type)
{
    if (!DecodeLoadStoreAddress(f))
        return false;

    ExprType actual;
    if (!DecodeExpr(f, &actual))
        return false;

    if (!CheckType(f, actual, storeType))
        return false;

    *type = ToExprType(storeType);
    return true;
}

static bool
DecodeBr(FunctionDecoder& f, ExprType* type)
{
    uint32_t relativeDepth;
    if (!f.d().readVarU32(&relativeDepth))
        return f.fail("expected relative depth");

    if (!f.branchWithType(relativeDepth, ExprType::Void))
        return f.fail("branch depth exceeds current nesting level");

    *type = AnyType;
    return true;
}

static bool
DecodeBrIf(FunctionDecoder& f, ExprType* type)
{
    uint32_t relativeDepth;
    if (!f.d().readVarU32(&relativeDepth))
        return f.fail("expected relative depth");

    if (!f.branchWithType(relativeDepth, ExprType::Void))
        return f.fail("branch depth exceeds current nesting level");

    ExprType actual;
    if (!DecodeExpr(f, &actual))
        return false;

    if (!CheckType(f, actual, ValType::I32))
        return false;

    *type = ExprType::Void;
    return true;
}

static bool
DecodeBrTable(FunctionDecoder& f, ExprType* type)
{
    uint32_t tableLength;
    if (!f.d().readVarU32(&tableLength))
        return false;

    if (tableLength > MaxBrTableElems)
        return f.fail("too many br_table entries");

    for (uint32_t i = 0; i < tableLength; i++) {
        uint32_t depth;
        if (!f.d().readVarU32(&depth))
            return f.fail("missing br_table entry");

        if (!f.branchWithType(depth, ExprType::Void))
            return f.fail("branch depth exceeds current nesting level");
    }

    uint32_t defaultDepth;
    if (!f.d().readVarU32(&defaultDepth))
        return f.fail("expected default relative depth");

    if (!f.branchWithType(defaultDepth, ExprType::Void))
        return f.fail("branch depth exceeds current nesting level");

    ExprType actual;
    if (!DecodeExpr(f, &actual))
        return false;

    if (!CheckType(f, actual, ExprType::I32))
        return false;

    *type = AnyType;
    return true;
}

static bool
DecodeReturn(FunctionDecoder& f, ExprType* type)
{
    if (f.sig().ret() != ExprType::Void) {
        ExprType actual;
        if (!DecodeExpr(f, &actual))
            return false;

        if (!CheckType(f, actual, f.sig().ret()))
            return false;
    }

    *type = AnyType;
    return true;
}

static bool
DecodeExpr(FunctionDecoder& f, ExprType* type)
{
    Expr expr;
    if (!f.d().readExpr(&expr))
        return f.fail("unable to read expression");

    switch (expr) {
      case Expr::Nop:
        return DecodeNop(f, type);
      case Expr::Call:
        return DecodeCall(f, type);
      case Expr::CallImport:
        return DecodeCallImport(f, type);
      case Expr::CallIndirect:
        return DecodeCallIndirect(f, type);
      case Expr::I32Const:
        return DecodeConstI32(f, type);
      case Expr::I64Const:
        return DecodeConstI64(f, type);
      case Expr::F32Const:
        return DecodeConstF32(f, type);
      case Expr::F64Const:
        return DecodeConstF64(f, type);
      case Expr::GetLocal:
        return DecodeGetLocal(f, type);
      case Expr::SetLocal:
        return DecodeSetLocal(f, type);
      case Expr::Block:
        return DecodeBlock(f, /* isLoop */ false, type);
      case Expr::Loop:
        return DecodeBlock(f, /* isLoop */ true, type);
      case Expr::If:
        return DecodeIfElse(f, /* hasElse */ false, type);
      case Expr::IfElse:
        return DecodeIfElse(f, /* hasElse */ true, type);
      case Expr::I32Clz:
      case Expr::I32Ctz:
      case Expr::I32Popcnt:
        return DecodeUnaryOperator(f, ValType::I32, type);
      case Expr::I64Clz:
      case Expr::I64Ctz:
      case Expr::I64Popcnt:
        return f.fail("NYI: i64") &&
               DecodeUnaryOperator(f, ValType::I64, type);
      case Expr::F32Abs:
      case Expr::F32Neg:
      case Expr::F32Ceil:
      case Expr::F32Floor:
      case Expr::F32Sqrt:
        return DecodeUnaryOperator(f, ValType::F32, type);
      case Expr::F32Trunc:
        return f.fail("NYI: trunc");
      case Expr::F32Nearest:
        return f.fail("NYI: nearest");
      case Expr::F64Abs:
      case Expr::F64Neg:
      case Expr::F64Ceil:
      case Expr::F64Floor:
      case Expr::F64Sqrt:
        return DecodeUnaryOperator(f, ValType::F64, type);
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
        return DecodeBinaryOperator(f, ValType::I32, type);
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
        return DecodeBinaryOperator(f, ValType::I64, type);
      case Expr::F32Add:
      case Expr::F32Sub:
      case Expr::F32Mul:
      case Expr::F32Div:
      case Expr::F32Min:
      case Expr::F32Max:
        return DecodeBinaryOperator(f, ValType::F32, type);
      case Expr::F32CopySign:
        return f.fail("NYI: copysign");
      case Expr::F64Add:
      case Expr::F64Sub:
      case Expr::F64Mul:
      case Expr::F64Div:
      case Expr::F64Min:
      case Expr::F64Max:
        return DecodeBinaryOperator(f, ValType::F64, type);
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
        return DecodeComparisonOperator(f, ValType::I32, type);
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
        return DecodeComparisonOperator(f, ValType::I64, type);
      case Expr::F32Eq:
      case Expr::F32Ne:
      case Expr::F32Lt:
      case Expr::F32Le:
      case Expr::F32Gt:
      case Expr::F32Ge:
        return DecodeComparisonOperator(f, ValType::F32, type);
      case Expr::F64Eq:
      case Expr::F64Ne:
      case Expr::F64Lt:
      case Expr::F64Le:
      case Expr::F64Gt:
      case Expr::F64Ge:
        return DecodeComparisonOperator(f, ValType::F64, type);
      case Expr::I32WrapI64:
        return DecodeConversionOperator(f, ValType::I32, ValType::I64, type);
      case Expr::I32TruncSF32:
      case Expr::I32TruncUF32:
        return DecodeConversionOperator(f, ValType::I32, ValType::F32, type);
      case Expr::I32ReinterpretF32:
        return f.fail("NYI: reinterpret");
      case Expr::I32TruncSF64:
      case Expr::I32TruncUF64:
        return DecodeConversionOperator(f, ValType::I32, ValType::F64, type);
      case Expr::I64ExtendSI32:
      case Expr::I64ExtendUI32:
        return DecodeConversionOperator(f, ValType::I64, ValType::I32, type);
      case Expr::I64TruncSF32:
      case Expr::I64TruncUF32:
        return DecodeConversionOperator(f, ValType::I64, ValType::F32, type);
      case Expr::I64TruncSF64:
      case Expr::I64TruncUF64:
        return DecodeConversionOperator(f, ValType::I64, ValType::F64, type);
      case Expr::I64ReinterpretF64:
        return f.fail("NYI: i64");
      case Expr::F32ConvertSI32:
      case Expr::F32ConvertUI32:
        return DecodeConversionOperator(f, ValType::F32, ValType::I32, type);
      case Expr::F32ReinterpretI32:
        return f.fail("NYI: reinterpret");
      case Expr::F32ConvertSI64:
      case Expr::F32ConvertUI64:
        return f.fail("NYI: i64") &&
               DecodeConversionOperator(f, ValType::F32, ValType::I64, type);
      case Expr::F32DemoteF64:
        return DecodeConversionOperator(f, ValType::F32, ValType::F64, type);
      case Expr::F64ConvertSI32:
      case Expr::F64ConvertUI32:
        return DecodeConversionOperator(f, ValType::F64, ValType::I32, type);
      case Expr::F64ConvertSI64:
      case Expr::F64ConvertUI64:
      case Expr::F64ReinterpretI64:
        return f.fail("NYI: i64") &&
               DecodeConversionOperator(f, ValType::F64, ValType::I64, type);
      case Expr::F64PromoteF32:
        return DecodeConversionOperator(f, ValType::F64, ValType::F32, type);
      case Expr::I32Load:
      case Expr::I32Load8S:
      case Expr::I32Load8U:
      case Expr::I32Load16S:
      case Expr::I32Load16U:
        return DecodeLoad(f, ValType::I32, type);
      case Expr::I64Load:
      case Expr::I64Load8S:
      case Expr::I64Load8U:
      case Expr::I64Load16S:
      case Expr::I64Load16U:
      case Expr::I64Load32S:
      case Expr::I64Load32U:
        return f.fail("NYI: i64") &&
               DecodeLoad(f, ValType::I64, type);
      case Expr::F32Load:
        return DecodeLoad(f, ValType::F32, type);
      case Expr::F64Load:
        return DecodeLoad(f, ValType::F64, type);
      case Expr::I32Store:
      case Expr::I32Store8:
      case Expr::I32Store16:
        return DecodeStore(f, ValType::I32, type);
      case Expr::I64Store:
      case Expr::I64Store8:
      case Expr::I64Store16:
      case Expr::I64Store32:
        return f.fail("NYI: i64") &&
               DecodeStore(f, ValType::I64, type);
      case Expr::F32Store:
        return DecodeStore(f, ValType::F32, type);
      case Expr::F64Store:
        return DecodeStore(f, ValType::F64, type);
      case Expr::Br:
        return DecodeBr(f, type);
      case Expr::BrIf:
        return DecodeBrIf(f, type);
      case Expr::BrTable:
        return DecodeBrTable(f, type);
      case Expr::Return:
        return DecodeReturn(f, type);
      default:
        // Note: it's important not to remove this default since readExpr()
        // can return Expr values for which there is no enumerator.
        break;
    }

    return f.fail("bad expression code");
}

/*****************************************************************************/
// wasm decoding and generation

typedef HashSet<const DeclaredSig*, SigHashPolicy> SigSet;

static bool
DecodeSignatures(JSContext* cx, Decoder& d, ModuleGeneratorData* init)
{
    uint32_t sectionStart;
    if (!d.startSection(SignaturesId, &sectionStart))
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
        if (!d.readExprType(&result))
            return Fail(cx, d, "bad expression type");

        if (!CheckExprType(cx, d, result))
            return false;

        ValTypeVector args;
        if (!args.resize(numArgs))
            return false;

        for (uint32_t i = 0; i < numArgs; i++) {
            if (!d.readValType(&args[i]))
                return Fail(cx, d, "bad value type");

            if (!CheckValType(cx, d, args[i]))
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
DecodeFunctionSignatures(JSContext* cx, Decoder& d, ModuleGeneratorData* init)
{
    uint32_t sectionStart;
    if (!d.startSection(FunctionSignaturesId, &sectionStart))
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

    if (!d.finishSection(sectionStart))
        return Fail(cx, d, "decls section byte size mismatch");

    return true;
}

static bool
DecodeFunctionTable(JSContext* cx, Decoder& d, ModuleGeneratorData* init)
{
    uint32_t sectionStart;
    if (!d.startSection(FunctionTableId, &sectionStart))
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
CheckTypeForJS(JSContext* cx, Decoder& d, const Sig& sig)
{
    for (ValType argType : sig.args()) {
        if (argType == ValType::I64)
            return Fail(cx, d, "cannot import/export i64 argument");
    }

    if (sig.ret() == ExprType::I64)
        return Fail(cx, d, "cannot import/export i64 return type");

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
    const DeclaredSig* sig;
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
DecodeImportTable(JSContext* cx, Decoder& d, ModuleGeneratorData* init, ImportNameVector* importNames)
{
    uint32_t sectionStart;
    if (!d.startSection(ImportTableId, &sectionStart))
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

    if (!d.finishSection(sectionStart))
        return Fail(cx, d, "import section byte size mismatch");

    return true;
}

static bool
DecodeMemory(JSContext* cx, Decoder& d, ModuleGenerator& mg, MutableHandle<ArrayBufferObject*> heap)
{
    uint32_t sectionStart;
    if (!d.startSection(MemoryId, &sectionStart))
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

    if (!d.finishSection(sectionStart))
        return Fail(cx, d, "memory section byte size mismatch");

    bool signalsForOOB = CompileArgs(cx).useSignalHandlersForOOB;
    heap.set(ArrayBufferObject::createForWasm(cx, initialSize.value(), signalsForOOB));
    if (!heap)
        return false;

    mg.initHeapUsage(HeapUsage::Unshared);
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

    UniqueChars fieldName((char*)fieldBytes.extractRawBuffer());
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
DecodeExportTable(JSContext* cx, Decoder& d, ModuleGenerator& mg)
{
    uint32_t sectionStart;
    if (!d.startSection(ExportTableId, &sectionStart))
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

    if (!d.finishSection(sectionStart))
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

    FunctionDecoder f(cx, d, mg, fg, funcIndex, locals);

    ExprType type = ExprType::Void;

    while (d.currentPosition() < bodyEnd) {
        if (!DecodeExpr(f, &type))
            return false;
    }

    if (!CheckType(f, type, f.sig().ret()))
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
DecodeFunctionBodies(JSContext* cx, Decoder& d, ModuleGenerator& mg)
{
    if (!mg.startFuncDefs())
        return false;

    uint32_t sectionStart;
    if (!d.startSection(FunctionBodiesId, &sectionStart))
        return Fail(cx, d, "failed to start section");

    if (sectionStart == Decoder::NotStarted) {
        if (mg.numFuncSigs() != 0)
            return Fail(cx, d, "expected function bodies");

        return mg.finishFuncDefs();
    }

    for (uint32_t funcIndex = 0; funcIndex < mg.numFuncSigs(); funcIndex++) {
        if (!DecodeFunctionBody(cx, d, mg, funcIndex))
            return false;
    }

    if (!d.finishSection(sectionStart))
        return Fail(cx, d, "function section byte size mismatch");

    return mg.finishFuncDefs();
}

static bool
DecodeDataSegments(JSContext* cx, Decoder& d, Handle<ArrayBufferObject*> heap)
{
    uint32_t sectionStart;
    if (!d.startSection(DataSegmentsId, &sectionStart))
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

    if (!d.finishSection(sectionStart))
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

    if (!DecodeSignatures(cx, d, init.get()))
        return false;

    if (!DecodeImportTable(cx, d, init.get(), importNames))
        return false;

    if (!DecodeFunctionSignatures(cx, d, init.get()))
        return false;

    if (!DecodeFunctionTable(cx, d, init.get()))
        return false;

    ModuleGenerator mg(cx);
    if (!mg.init(Move(init), Move(file)))
        return false;

    if (!DecodeMemory(cx, d, mg, heap))
        return false;

    if (!DecodeExportTable(cx, d, mg))
        return false;

    if (!DecodeFunctionBodies(cx, d, mg))
        return false;

    if (!DecodeDataSegments(cx, d, heap))
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

bool
wasm::Eval(JSContext* cx, Handle<TypedArrayObject*> code, HandleObject importObj,
           MutableHandleObject exportObj)
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

    return moduleObj->module().dynamicallyLink(cx, moduleObj, heap, imports, *exportMap, exportObj);
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

    if (!JS_DefineFunctions(cx, Wasm, wasm_static_methods))
        return nullptr;

    global->as<GlobalObject>().setConstructor(JSProto_Wasm, ObjectValue(*Wasm));
    return Wasm;
}


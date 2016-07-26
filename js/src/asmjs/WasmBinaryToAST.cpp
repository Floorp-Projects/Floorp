/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2016 Mozilla Foundation
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

#include "asmjs/WasmBinaryToAST.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/MathAlgorithms.h"

#include "asmjs/WasmBinaryIterator.h"

using namespace js;
using namespace js::wasm;

using mozilla::CheckedInt;
using mozilla::FloorLog2;

enum AstDecodeTerminationKind
{
    Unknown,
    End,
    Else
};

struct AstDecodeStackItem
{
    AstExpr* expr;
    union {
      uint32_t popped;
      AstDecodeTerminationKind terminationKind;
    };

    explicit AstDecodeStackItem(): expr(nullptr), terminationKind(AstDecodeTerminationKind::Unknown) {}
    explicit AstDecodeStackItem(AstDecodeTerminationKind terminationKind): expr(nullptr), terminationKind(terminationKind) {}
    explicit AstDecodeStackItem(AstExpr* expr): expr(expr), popped(0) {}
    explicit AstDecodeStackItem(AstExpr* expr, uint32_t popped): expr(expr), popped(popped) {}
};

struct AstDecodePolicy : ExprIterPolicy
{
    static const bool Output = true;
    typedef AstDecodeStackItem Value;
};

typedef ExprIter<AstDecodePolicy> AstDecodeExprIter;

class AstDecodeContext
{
  public:
    typedef AstVector<uint32_t> AstIndexVector;

    JSContext* cx;
    LifoAlloc& lifo;
    Decoder& d;
    bool generateNames;

  private:
    AstModule& module_;
    AstIndexVector funcSigs_;
    AstDecodeExprIter *iter_;
    const ValTypeVector* locals_;
    AstNameVector blockLabels_;
    uint32_t currentLabelIndex_;

  public:
    AstDecodeContext(JSContext* cx, LifoAlloc& lifo, Decoder& d, AstModule& module, bool generateNames)
     : cx(cx),
       lifo(lifo),
       d(d),
       generateNames(generateNames),
       module_(module),
       funcSigs_(lifo),
       iter_(nullptr),
       locals_(nullptr),
       blockLabels_(lifo),
       currentLabelIndex_(0)
    {}

    AstModule& module() { return module_; }
    AstIndexVector& funcSigs() { return funcSigs_; }
    AstDecodeExprIter& iter() { return *iter_; }
    const ValTypeVector& locals() { return *locals_; }
    AstNameVector& blockLabels() { return blockLabels_; }

    void startFunction(AstDecodeExprIter *iter, const ValTypeVector* locals)
    {
        iter_ = iter;
        locals_ = locals;
        currentLabelIndex_ = 0;
    }
    void endFunction()
    {
        iter_ = nullptr;
        locals_ = nullptr;
        MOZ_ASSERT(blockLabels_.length() == 0);
    }
    uint32_t nextLabelIndex()
    {
        return currentLabelIndex_++;
    }
};

static bool
AstDecodeFail(AstDecodeContext& c, const char* str)
{
    uint32_t offset = c.d.currentOffset();
    char offsetStr[sizeof "4294967295"];
    JS_snprintf(offsetStr, sizeof offsetStr, "%" PRIu32, offset);
    JS_ReportErrorNumber(c.cx, GetErrorMessage, nullptr, JSMSG_WASM_DECODE_FAIL, offsetStr, str);
    return false;
}

static bool
AstDecodeGenerateName(AstDecodeContext& c, const AstName& prefix, uint32_t index, AstName* name)
{
    if (!c.generateNames) {
        *name = AstName();
        return true;
    }

    AstVector<char16_t> result(c.lifo);
    if (!result.append(u'$'))
        return false;
    if (!result.append(prefix.begin(), prefix.length()))
        return false;

    uint32_t tmp = index;
    do {
        if (!result.append(u'0'))
            return false;
        tmp /= 10;
    } while (tmp);

    if (index) {
        char16_t* p = result.end();
        for (tmp = index; tmp; tmp /= 10)
            *(--p) = u'0' + (tmp % 10);
    }

    size_t length = result.length();
    char16_t* begin = result.extractOrCopyRawBuffer();
    if (!begin)
        return false;

    *name = AstName(begin, length);
    return true;
}

static bool
AstDecodeGenerateRef(AstDecodeContext& c, const AstName& prefix, uint32_t index, AstRef* ref)
{
    MOZ_ASSERT(index != AstNoIndex);

    if (!c.generateNames) {
        *ref = AstRef(AstName(), index);
        return true;
    }

    AstName name;
    if (!AstDecodeGenerateName(c, prefix, index, &name))
        return false;
    MOZ_ASSERT(!name.empty());

    *ref = AstRef(name, AstNoIndex);
    ref->setIndex(index);
    return true;
}

static bool
AstDecodeCallArgs(AstDecodeContext& c, uint32_t arity, const AstSig& sig, AstExprVector* funcArgs)
{
    if (arity != sig.args().length())
        return c.iter().fail("call arity out of range");

    const AstValTypeVector& args = sig.args();
    uint32_t numArgs = args.length();

    if (!funcArgs->resize(numArgs))
        return false;

    for (size_t i = 0; i < numArgs; ++i) {
        ValType argType = args[i];
        AstDecodeStackItem item;
        if (!c.iter().readCallArg(argType, numArgs, i, &item))
            return false;
        (*funcArgs)[i] = item.expr;
    }

    return c.iter().readCallArgsEnd(numArgs);
}

static bool
AstDecodeCallReturn(AstDecodeContext& c, const AstSig& sig)
{
    return c.iter().readCallReturn(sig.ret());
}

static bool
AstDecodeExpr(AstDecodeContext& c);

static bool
AstDecodeCall(AstDecodeContext& c)
{
    uint32_t calleeIndex;
    uint32_t arity;
    if (!c.iter().readCall(&calleeIndex, &arity))
        return false;

    if (calleeIndex >= c.funcSigs().length())
        return c.iter().fail("callee index out of range");

    uint32_t sigIndex = c.funcSigs()[calleeIndex];
    const AstSig* sig = c.module().sigs()[sigIndex];

    AstRef funcRef;
    if (!AstDecodeGenerateRef(c, AstName(u"func"), calleeIndex, &funcRef))
        return false;

    AstExprVector args(c.lifo);
    if (!AstDecodeCallArgs(c, arity, *sig, &args))
        return false;

    if (!AstDecodeCallReturn(c, *sig))
        return false;

    uint32_t argsLength = args.length();
    AstCall* call = new(c.lifo) AstCall(Expr::Call, funcRef, Move(args));
    if (!call)
        return false;

    c.iter().setResult(AstDecodeStackItem(call, argsLength));
    return true;
}

static bool
AstDecodeCallIndirect(AstDecodeContext& c)
{
    uint32_t sigIndex;
    uint32_t arity;
    if (!c.iter().readCallIndirect(&sigIndex, &arity))
        return false;

    AstRef sigRef;
    if (!AstDecodeGenerateRef(c, AstName(u"type"), sigIndex, &sigRef))
        return false;

    if (sigIndex >= c.module().sigs().length())
        return c.iter().fail("signature index out of range");

    const AstSig* sig = c.module().sigs()[sigIndex];
    AstExprVector args(c.lifo);
    if (!AstDecodeCallArgs(c, arity, *sig, &args))
        return false;

    AstDecodeStackItem index;
    if (!c.iter().readCallIndirectCallee(&index))
        return false;


    if (!AstDecodeCallReturn(c, *sig))
        return false;

    uint32_t argsLength = args.length();
    AstCallIndirect* call = new(c.lifo) AstCallIndirect(sigRef, index.expr, Move(args));
    if (!call)
        return false;

    c.iter().setResult(AstDecodeStackItem(call, 1 + argsLength));
    return true;
}

static bool
AstDecodeCallImport(AstDecodeContext& c)
{
    uint32_t importIndex;
    uint32_t arity;
    if (!c.iter().readCallImport(&importIndex, &arity))
        return false;

    if (importIndex >= c.module().imports().length())
        return c.iter().fail("import index out of range");

    AstImport* import = c.module().imports()[importIndex];
    AstSig* sig = c.module().sigs()[import->funcSig().index()];
    AstRef funcRef;
    if (!AstDecodeGenerateRef(c, AstName(u"import"), importIndex, &funcRef))
        return false;

    AstExprVector args(c.lifo);
    if (!AstDecodeCallArgs(c, arity, *sig, &args))
        return false;

    if (!AstDecodeCallReturn(c, *sig))
        return false;

    uint32_t argsLength = args.length();
    AstCall* call = new(c.lifo) AstCall(Expr::CallImport, funcRef, Move(args));
    if (!call)
        return false;

    c.iter().setResult(AstDecodeStackItem(call, argsLength));
    return true;
}

static bool
AstDecodeGetBlockRef(AstDecodeContext& c, uint32_t depth, AstRef* ref)
{
    if (!c.generateNames || depth >= c.blockLabels().length()) {
        // Also ignoring if it's a function body label.
        *ref = AstRef(AstName(), depth);
        return true;
    }

    uint32_t index = c.blockLabels().length() - depth - 1;
    if (c.blockLabels()[index].empty()) {
        if (!AstDecodeGenerateName(c, AstName(u"label"), c.nextLabelIndex(), &c.blockLabels()[index]))
            return false;
    }
    *ref = AstRef(c.blockLabels()[index], AstNoIndex);
    ref->setIndex(depth);
    return true;
}

static bool
AstDecodeBrTable(AstDecodeContext& c)
{
    uint32_t tableLength;
    ExprType type;
    AstDecodeStackItem index;
    AstDecodeStackItem value;
    if (!c.iter().readBrTable(&tableLength, &type, &value, &index))
        return false;

    AstRefVector table(c.lifo);
    if (!table.resize(tableLength))
        return false;

    uint32_t depth;
    for (size_t i = 0, e = tableLength; i < e; ++i) {
        if (!c.iter().readBrTableEntry(type, &depth))
            return false;
        if (!AstDecodeGetBlockRef(c, depth, &table[i]))
            return false;
    }

    // Read the default label.
    if (!c.iter().readBrTableEntry(type, &depth))
        return false;

    AstRef def;
    if (!AstDecodeGetBlockRef(c, depth, &def))
        return false;

    AstBranchTable* branchTable = new(c.lifo) AstBranchTable(*index.expr, def, Move(table), value.expr);
    if (!branchTable)
        return false;

    c.iter().setResult(AstDecodeStackItem(branchTable, value.expr ? 2 : 1));
    return true;
}

static bool
AstDecodeBlock(AstDecodeContext& c, Expr expr)
{
    MOZ_ASSERT(expr == Expr::Block || expr == Expr::Loop);

    if (expr == Expr::Loop) {
      if (!c.blockLabels().append(AstName()) || !c.blockLabels().append(AstName()))
          return false;
      if (!c.iter().readLoop())
          return false;
    } else {
      if (!c.blockLabels().append(AstName()))
          return false;
      if (!c.iter().readBlock())
          return false;
    }

    AstExprVector exprs(c.lifo);
    while (true) {
        if (!AstDecodeExpr(c))
            return false;

        AstDecodeStackItem item = c.iter().getResult();
        if (!item.expr) // Expr::End was found
            break;

        exprs.shrinkBy(item.popped);
        if (!exprs.append(item.expr))
            return false;
    }

    AstName continueName;
    if (expr == Expr::Loop)
        continueName = c.blockLabels().popCopy();
    AstName breakName = c.blockLabels().popCopy();
    AstBlock* block = new(c.lifo) AstBlock(expr, breakName, continueName, Move(exprs));
    if (!block)
        return false;

    c.iter().setResult(AstDecodeStackItem(block));
    return true;
}

static bool
AstDecodeIf(AstDecodeContext& c)
{
    AstDecodeStackItem cond;
    if (!c.iter().readIf(&cond))
        return false;

    bool hasElse = false;

    if (!c.blockLabels().append(AstName()))
        return false;
    AstExprVector thenExprs(c.lifo);
    while (true) {
        if (!AstDecodeExpr(c))
            return false;

        AstDecodeStackItem item = c.iter().getResult();
        if (!item.expr) {
            hasElse = item.terminationKind == AstDecodeTerminationKind::Else;
            break;
        }

        thenExprs.shrinkBy(item.popped);
        if (!thenExprs.append(item.expr))
            return false;
    }
    AstName thenName = c.blockLabels().popCopy();

    AstName elseName;
    AstExprVector elseExprs(c.lifo);
    if (hasElse) {
        if (!c.blockLabels().append(AstName()))
            return false;
        while (true) {
            if (!AstDecodeExpr(c))
                return false;

            AstDecodeStackItem item = c.iter().getResult();
            if (!item.expr) // Expr::End was found
                break;

            elseExprs.shrinkBy(item.popped);
            if (!elseExprs.append(item.expr))
                return false;
        }
        elseName = c.blockLabels().popCopy();
    }

    AstIf* if_ = new(c.lifo) AstIf(cond.expr, thenName, Move(thenExprs), elseName, Move(elseExprs));
    if (!if_)
        return false;

    c.iter().setResult(AstDecodeStackItem(if_, 1));
    return true;
}

static bool
AstDecodeEnd(AstDecodeContext& c)
{
    LabelKind kind;
    ExprType type;
    AstDecodeStackItem tmp;
    if (!c.iter().readEnd(&kind, &type, &tmp))
        return false;

    c.iter().setResult(AstDecodeStackItem(AstDecodeTerminationKind::End));
    return true;
}

static bool
AstDecodeElse(AstDecodeContext& c)
{
    ExprType type;
    AstDecodeStackItem tmp;

    if (!c.iter().readElse(&type, &tmp))
        return false;

    c.iter().setResult(AstDecodeStackItem(AstDecodeTerminationKind::Else));
    return true;
}

static bool
AstDecodeUnary(AstDecodeContext& c, ValType type, Expr expr)
{
    AstDecodeStackItem op;
    if (!c.iter().readUnary(type, &op))
        return false;

    AstUnaryOperator* unary = new(c.lifo) AstUnaryOperator(expr, op.expr);
    if (!unary)
        return false;

    c.iter().setResult(AstDecodeStackItem(unary, 1));
    return true;
}

static bool
AstDecodeBinary(AstDecodeContext& c, ValType type, Expr expr)
{
    AstDecodeStackItem lhs;
    AstDecodeStackItem rhs;
    if (!c.iter().readBinary(type, &lhs, &rhs))
        return false;

    AstBinaryOperator* binary = new(c.lifo) AstBinaryOperator(expr, lhs.expr, rhs.expr);
    if (!binary)
        return false;

    c.iter().setResult(AstDecodeStackItem(binary, 2));
    return true;
}

static bool
AstDecodeSelect(AstDecodeContext& c)
{
    ExprType type;
    AstDecodeStackItem cond;
    AstDecodeStackItem selectTrue;
    AstDecodeStackItem selectFalse;
    if (!c.iter().readSelect(&type, &cond, &selectTrue, &selectFalse))
        return false;

    AstTernaryOperator* ternary = new(c.lifo) AstTernaryOperator(Expr::Select, cond.expr, selectTrue.expr, selectFalse.expr);
    if (!ternary)
        return false;

    c.iter().setResult(AstDecodeStackItem(ternary, 3));
    return true;
}

static bool
AstDecodeComparison(AstDecodeContext& c, ValType type, Expr expr)
{
    AstDecodeStackItem lhs;
    AstDecodeStackItem rhs;
    if (!c.iter().readComparison(type, &lhs, &rhs))
        return false;

    AstComparisonOperator* comparison = new(c.lifo) AstComparisonOperator(expr, lhs.expr, rhs.expr);
    if (!comparison)
        return false;

    c.iter().setResult(AstDecodeStackItem(comparison, 2));
    return true;
}

static bool
AstDecodeConversion(AstDecodeContext& c, ValType fromType, ValType toType, Expr expr)
{
    AstDecodeStackItem op;
    if (!c.iter().readConversion(fromType, toType, &op))
        return false;

    AstConversionOperator* conversion = new(c.lifo) AstConversionOperator(expr, op.expr);
    if (!conversion)
        return false;

    c.iter().setResult(AstDecodeStackItem(conversion, 1));
    return true;
}

static AstLoadStoreAddress
AstDecodeLoadStoreAddress(const LinearMemoryAddress<AstDecodeStackItem>& addr)
{
    uint32_t flags = FloorLog2(addr.align);
    return AstLoadStoreAddress(addr.base.expr, flags, addr.offset);
}

static bool
AstDecodeLoad(AstDecodeContext& c, ValType type, uint32_t byteSize, Expr expr)
{
    LinearMemoryAddress<AstDecodeStackItem> addr;
    if (!c.iter().readLoad(type, byteSize, &addr))
        return false;

    AstLoad* load = new(c.lifo) AstLoad(expr, AstDecodeLoadStoreAddress(addr));
    if (!load)
        return false;

    c.iter().setResult(AstDecodeStackItem(load, 1));
    return true;
}

static bool
AstDecodeStore(AstDecodeContext& c, ValType type, uint32_t byteSize, Expr expr)
{
    LinearMemoryAddress<AstDecodeStackItem> addr;
    AstDecodeStackItem value;
    if (!c.iter().readStore(type, byteSize, &addr, &value))
        return false;

    AstStore* store = new(c.lifo) AstStore(expr, AstDecodeLoadStoreAddress(addr), value.expr);
    if (!store)
        return false;

    c.iter().setResult(AstDecodeStackItem(store, 2));
    return true;
}

static bool
AstDecodeBranch(AstDecodeContext& c, Expr expr)
{
    MOZ_ASSERT(expr == Expr::Br || expr == Expr::BrIf);

    uint32_t depth;
    ExprType type;
    AstDecodeStackItem value;
    AstDecodeStackItem cond;
    uint32_t popped;
    if (expr == Expr::Br) {
        if (!c.iter().readBr(&depth, &type, &value))
            return false;
        popped = value.expr ? 1 : 0;
    } else {
        if (!c.iter().readBrIf(&depth, &type, &value, &cond))
            return false;
        popped = value.expr ? 2 : 1;
    }

    AstRef depthRef;
    if (!AstDecodeGetBlockRef(c, depth, &depthRef))
        return false;

    AstBranch* branch = new(c.lifo) AstBranch(expr, cond.expr, depthRef, value.expr);
    if (!branch)
        return false;

    c.iter().setResult(AstDecodeStackItem(branch, popped));
    return true;
}

static bool
AstDecodeGetLocal(AstDecodeContext& c)
{
    uint32_t getLocalId;
    if (!c.iter().readGetLocal(c.locals(), &getLocalId))
        return false;

    AstRef localRef;
    if (!AstDecodeGenerateRef(c, AstName(u"var"), getLocalId, &localRef))
        return false;

    AstGetLocal* getLocal = new(c.lifo) AstGetLocal(localRef);
    if (!getLocal)
        return false;

    c.iter().setResult(AstDecodeStackItem(getLocal));
    return true;
}

static bool
AstDecodeSetLocal(AstDecodeContext& c)
{
    uint32_t setLocalId;
    AstDecodeStackItem setLocalValue;
    if (!c.iter().readSetLocal(c.locals(), &setLocalId, &setLocalValue))
        return false;

    AstRef localRef;
    if (!AstDecodeGenerateRef(c, AstName(u"var"), setLocalId, &localRef))
        return false;

    AstSetLocal* setLocal = new(c.lifo) AstSetLocal(localRef, *setLocalValue.expr);
    if (!setLocal)
        return false;

    c.iter().setResult(AstDecodeStackItem(setLocal, 1));
    return true;
}

static bool
AstDecodeReturn(AstDecodeContext& c)
{
    AstDecodeStackItem result;
    if (!c.iter().readReturn(&result))
        return false;

    AstReturn* ret = new(c.lifo) AstReturn(result.expr);
    if (!ret)
        return false;

    c.iter().setResult(AstDecodeStackItem(ret, result.expr ? 1 : 0));
    return true;
}

static bool
AstDecodeExpr(AstDecodeContext& c)
{
    uint32_t exprOffset = c.iter().currentOffset();
    Expr expr;
    if (!c.iter().readExpr(&expr))
        return false;

    AstExpr* tmp;
    switch (expr) {
      case Expr::Nop:
        if (!c.iter().readNullary())
            return false;
        tmp = new(c.lifo) AstNop();
        if (!tmp)
            return false;
        c.iter().setResult(AstDecodeStackItem(tmp));
        break;
      case Expr::Call:
        if (!AstDecodeCall(c))
            return false;
        break;
      case Expr::CallIndirect:
        if (!AstDecodeCallIndirect(c))
            return false;
        break;
      case Expr::CallImport:
        if (!AstDecodeCallImport(c))
            return false;
        break;
      case Expr::I32Const:
        int32_t i32;
        if (!c.iter().readI32Const(&i32))
            return false;
        tmp = new(c.lifo) AstConst(Val((uint32_t)i32));
        if (!tmp)
            return false;
        c.iter().setResult(AstDecodeStackItem(tmp));
        break;
      case Expr::I64Const:
        int64_t i64;
        if (!c.iter().readI64Const(&i64))
            return false;
        tmp = new(c.lifo) AstConst(Val((uint64_t)i64));
        if (!tmp)
            return false;
        c.iter().setResult(AstDecodeStackItem(tmp));
        break;
      case Expr::F32Const:
        float f32;
        if (!c.iter().readF32Const(&f32))
            return false;
        tmp = new(c.lifo) AstConst(Val(f32));
        if (!tmp)
            return false;
        c.iter().setResult(AstDecodeStackItem(tmp));
        break;
      case Expr::F64Const:
        double f64;
        if (!c.iter().readF64Const(&f64))
            return false;
        tmp = new(c.lifo) AstConst(Val(f64));
        if (!tmp)
            return false;
        c.iter().setResult(AstDecodeStackItem(tmp));
        break;
      case Expr::GetLocal:
        if (!AstDecodeGetLocal(c))
            return false;
        break;
      case Expr::SetLocal:
        if (!AstDecodeSetLocal(c))
            return false;
        break;
      case Expr::Select:
        if (!AstDecodeSelect(c))
            return false;
        break;
      case Expr::Block:
      case Expr::Loop:
        if (!AstDecodeBlock(c, expr))
            return false;
        break;
      case Expr::If:
        if (!AstDecodeIf(c))
            return false;
        break;
      case Expr::Else:
        if (!AstDecodeElse(c))
            return false;
        break;
      case Expr::End:
        if (!AstDecodeEnd(c))
            return false;
        break;
      case Expr::I32Clz:
      case Expr::I32Ctz:
      case Expr::I32Popcnt:
        if (!AstDecodeUnary(c, ValType::I32, expr))
            return false;
        break;
      case Expr::I64Clz:
      case Expr::I64Ctz:
      case Expr::I64Popcnt:
        if (!AstDecodeUnary(c, ValType::I64, expr))
            return false;
        break;
      case Expr::F32Abs:
      case Expr::F32Neg:
      case Expr::F32Ceil:
      case Expr::F32Floor:
      case Expr::F32Sqrt:
      case Expr::F32Trunc:
      case Expr::F32Nearest:
        if (!AstDecodeUnary(c, ValType::F32, expr))
            return false;
        break;
      case Expr::F64Abs:
      case Expr::F64Neg:
      case Expr::F64Ceil:
      case Expr::F64Floor:
      case Expr::F64Sqrt:
      case Expr::F64Trunc:
      case Expr::F64Nearest:
        if (!AstDecodeUnary(c, ValType::F64, expr))
            return false;
        break;
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
        if (!AstDecodeBinary(c, ValType::I32, expr))
            return false;
        break;
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
        if (!AstDecodeBinary(c, ValType::I64, expr))
            return false;
        break;
      case Expr::F32Add:
      case Expr::F32Sub:
      case Expr::F32Mul:
      case Expr::F32Div:
      case Expr::F32Min:
      case Expr::F32Max:
      case Expr::F32CopySign:
        if (!AstDecodeBinary(c, ValType::F32, expr))
            return false;
        break;
      case Expr::F64Add:
      case Expr::F64Sub:
      case Expr::F64Mul:
      case Expr::F64Div:
      case Expr::F64Min:
      case Expr::F64Max:
      case Expr::F64CopySign:
        if (!AstDecodeBinary(c, ValType::F64, expr))
            return false;
        break;
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
        if (!AstDecodeComparison(c, ValType::I32, expr))
            return false;
        break;
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
        if (!AstDecodeComparison(c, ValType::I64, expr))
            return false;
        break;
      case Expr::F32Eq:
      case Expr::F32Ne:
      case Expr::F32Lt:
      case Expr::F32Le:
      case Expr::F32Gt:
      case Expr::F32Ge:
        if (!AstDecodeComparison(c, ValType::F32, expr))
            return false;
        break;
      case Expr::F64Eq:
      case Expr::F64Ne:
      case Expr::F64Lt:
      case Expr::F64Le:
      case Expr::F64Gt:
      case Expr::F64Ge:
        if (!AstDecodeComparison(c, ValType::F64, expr))
            return false;
        break;
      case Expr::I32Eqz:
        if (!AstDecodeConversion(c, ValType::I32, ValType::I32, expr))
            return false;
        break;
      case Expr::I64Eqz:
        if (!AstDecodeConversion(c, ValType::I64, ValType::I32, expr))
            return false;
        break;
      case Expr::I32TruncSF32:
      case Expr::I32TruncUF32:
      case Expr::I32ReinterpretF32:
        if (!AstDecodeConversion(c, ValType::F32, ValType::I32, expr))
            return false;
        break;
      case Expr::I32TruncSF64:
      case Expr::I32TruncUF64:
        if (!AstDecodeConversion(c, ValType::F64, ValType::I32, expr))
            return false;
        break;
      case Expr::I64ExtendSI32:
      case Expr::I64ExtendUI32:
        if (!AstDecodeConversion(c, ValType::I32, ValType::I64, expr))
            return false;
        break;
      case Expr::I64TruncSF32:
      case Expr::I64TruncUF32:
        if (!AstDecodeConversion(c, ValType::F32, ValType::I64, expr))
            return false;
        break;
      case Expr::I64TruncSF64:
      case Expr::I64TruncUF64:
      case Expr::I64ReinterpretF64:
        if (!AstDecodeConversion(c, ValType::F64, ValType::I64, expr))
            return false;
        break;
      case Expr::F32ConvertSI32:
      case Expr::F32ConvertUI32:
      case Expr::F32ReinterpretI32:
        if (!AstDecodeConversion(c, ValType::I32, ValType::F32, expr))
            return false;
        break;
      case Expr::F32ConvertSI64:
      case Expr::F32ConvertUI64:
        if (!AstDecodeConversion(c, ValType::I64, ValType::F32, expr))
            return false;
        break;
      case Expr::F32DemoteF64:
        if (!AstDecodeConversion(c, ValType::F64, ValType::F32, expr))
            return false;
        break;
      case Expr::F64ConvertSI32:
      case Expr::F64ConvertUI32:
        if (!AstDecodeConversion(c, ValType::I32, ValType::F64, expr))
            return false;
        break;
      case Expr::F64ConvertSI64:
      case Expr::F64ConvertUI64:
      case Expr::F64ReinterpretI64:
        if (!AstDecodeConversion(c, ValType::I64, ValType::F64, expr))
            return false;
        break;
      case Expr::F64PromoteF32:
        if (!AstDecodeConversion(c, ValType::F32, ValType::F64, expr))
            return false;
        break;
      case Expr::I32Load8S:
      case Expr::I32Load8U:
        if (!AstDecodeLoad(c, ValType::I32, 1, expr))
            return false;
        break;
      case Expr::I32Load16S:
      case Expr::I32Load16U:
        if (!AstDecodeLoad(c, ValType::I32, 2, expr))
            return false;
        break;
      case Expr::I32Load:
        if (!AstDecodeLoad(c, ValType::I32, 4, expr))
            return false;
        break;
      case Expr::I64Load8S:
      case Expr::I64Load8U:
        if (!AstDecodeLoad(c, ValType::I64, 1, expr))
            return false;
        break;
      case Expr::I64Load16S:
      case Expr::I64Load16U:
        if (!AstDecodeLoad(c, ValType::I64, 2, expr))
            return false;
        break;
      case Expr::I64Load32S:
      case Expr::I64Load32U:
        if (!AstDecodeLoad(c, ValType::I64, 4, expr))
            return false;
        break;
      case Expr::I64Load:
        if (!AstDecodeLoad(c, ValType::I64, 8, expr))
            return false;
        break;
      case Expr::F32Load:
        if (!AstDecodeLoad(c, ValType::F32, 4, expr))
            return false;
        break;
      case Expr::F64Load:
        if (!AstDecodeLoad(c, ValType::F64, 8, expr))
            return false;
        break;
      case Expr::I32Store8:
        if (!AstDecodeStore(c, ValType::I32, 1, expr))
            return false;
        break;
      case Expr::I32Store16:
        if (!AstDecodeStore(c, ValType::I32, 2, expr))
            return false;
        break;
      case Expr::I32Store:
        if (!AstDecodeStore(c, ValType::I32, 4, expr))
            return false;
        break;
      case Expr::I64Store8:
        if (!AstDecodeStore(c, ValType::I64, 1, expr))
            return false;
        break;
      case Expr::I64Store16:
        if (!AstDecodeStore(c, ValType::I64, 2, expr))
            return false;
        break;
      case Expr::I64Store32:
        if (!AstDecodeStore(c, ValType::I64, 4, expr))
            return false;
        break;
      case Expr::I64Store:
        if (!AstDecodeStore(c, ValType::I64, 8, expr))
            return false;
        break;
      case Expr::F32Store:
        if (!AstDecodeStore(c, ValType::F32, 4, expr))
            return false;
        break;
      case Expr::F64Store:
        if (!AstDecodeStore(c, ValType::F64, 8, expr))
            return false;
        break;
      case Expr::Br:
      case Expr::BrIf:
        if (!AstDecodeBranch(c, expr))
            return false;
        break;
      case Expr::BrTable:
        if (!AstDecodeBrTable(c))
            return false;
        break;
      case Expr::Return:
        if (!AstDecodeReturn(c))
            return false;
        break;
      case Expr::Unreachable:
        if (!c.iter().readUnreachable())
            return false;
        tmp = new(c.lifo) AstUnreachable();
        if (!tmp)
            return false;
        c.iter().setResult(AstDecodeStackItem(tmp));
        break;
      default:
        // Note: it's important not to remove this default since readExpr()
        // can return Expr values for which there is no enumerator.
        return c.iter().unrecognizedOpcode(expr);
    }

    AstExpr* lastExpr = c.iter().getResult().expr;
    if (lastExpr)
        lastExpr->setOffset(exprOffset);
    return true;
}

/*****************************************************************************/
// wasm decoding and generation

static bool
AstDecodeTypeSection(AstDecodeContext& c)
{
    uint32_t sectionStart, sectionSize;
    if (!c.d.startSection(TypeSectionId, &sectionStart, &sectionSize))
        return AstDecodeFail(c, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numSigs;
    if (!c.d.readVarU32(&numSigs))
        return AstDecodeFail(c, "expected number of signatures");

    if (numSigs > MaxSigs)
        return AstDecodeFail(c, "too many signatures");

    for (uint32_t sigIndex = 0; sigIndex < numSigs; sigIndex++) {
        uint32_t form;
        if (!c.d.readVarU32(&form) || form != uint32_t(TypeConstructor::Function))
            return AstDecodeFail(c, "expected function form");

        uint32_t numArgs;
        if (!c.d.readVarU32(&numArgs))
            return AstDecodeFail(c, "bad number of function args");

        if (numArgs > MaxArgsPerFunc)
            return AstDecodeFail(c, "too many arguments in signature");

        AstValTypeVector args(c.lifo);
        if (!args.resize(numArgs))
            return false;

        for (uint32_t i = 0; i < numArgs; i++) {
            if (!c.d.readValType(&args[i]))
                return AstDecodeFail(c, "bad value type");
        }

        uint32_t numRets;
        if (!c.d.readVarU32(&numRets))
            return AstDecodeFail(c, "bad number of function returns");

        if (numRets > 1)
            return AstDecodeFail(c, "too many returns in signature");

        ExprType result = ExprType::Void;

        if (numRets == 1) {
            ValType type;
            if (!c.d.readValType(&type))
                return AstDecodeFail(c, "bad expression type");

            result = ToExprType(type);
        }

        AstSig sigNoName(Move(args), result);
        AstName sigName;
        if (!AstDecodeGenerateName(c, AstName(u"type"), sigIndex, &sigName))
            return false;

        AstSig* sig = new(c.lifo) AstSig(sigName, Move(sigNoName));
        if (!sig || !c.module().append(sig))
            return false;
    }

    if (!c.d.finishSection(sectionStart, sectionSize))
        return AstDecodeFail(c, "decls section byte size mismatch");

    return true;
}

static bool
AstDecodeSignatureIndex(AstDecodeContext& c, uint32_t* sigIndex)
{
    if (!c.d.readVarU32(sigIndex))
        return AstDecodeFail(c, "expected signature index");

    if (*sigIndex >= c.module().sigs().length())
        return AstDecodeFail(c, "signature index out of range");

    return true;
}

static bool
AstDecodeFunctionSection(AstDecodeContext& c)
{
    uint32_t sectionStart, sectionSize;
    if (!c.d.startSection(FunctionSectionId, &sectionStart, &sectionSize))
        return AstDecodeFail(c, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numDecls;
    if (!c.d.readVarU32(&numDecls))
        return AstDecodeFail(c, "expected number of declarations");

    if (numDecls > MaxFuncs)
        return AstDecodeFail(c, "too many functions");


    if (!c.funcSigs().resize(numDecls))
        return false;

    for (uint32_t i = 0; i < numDecls; i++) {
        if (!AstDecodeSignatureIndex(c, &c.funcSigs()[i]))
            return false;
    }

    if (!c.d.finishSection(sectionStart, sectionSize))
        return AstDecodeFail(c, "decls section byte size mismatch");

    return true;
}

static bool
AstDecodeTableSection(AstDecodeContext& c)
{
    uint32_t sectionStart, sectionSize;
    if (!c.d.startSection(TableSectionId, &sectionStart, &sectionSize))
        return AstDecodeFail(c, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numElems;
    if (!c.d.readVarU32(&numElems))
        return AstDecodeFail(c, "expected number of table elems");

    if (numElems > MaxTableElems)
        return AstDecodeFail(c, "too many table elements");

    AstRefVector elems(c.lifo);
    if (!elems.resize(numElems))
        return false;

    for (uint32_t i = 0; i < numElems; i++) {
        uint32_t funcIndex;
        if (!c.d.readVarU32(&funcIndex))
            return AstDecodeFail(c, "expected table element");

        if (funcIndex >= c.funcSigs().length())
            return AstDecodeFail(c, "table element out of range");

        elems[i] = AstRef(AstName(), funcIndex);
    }

    AstElemSegment* seg = new(c.lifo) AstElemSegment(0, Move(elems));
    if (!seg || !c.module().append(seg))
        return false;

    if (!c.module().setTable(AstResizable(numElems, Nothing())))
        return AstDecodeFail(c, "already have a table");

    if (!c.d.finishSection(sectionStart, sectionSize))
        return AstDecodeFail(c, "table section byte size mismatch");

    return true;
}

static bool
AstDecodeName(AstDecodeContext& c, AstName* name)
{
    uint32_t length;
    if (!c.d.readVarU32(&length))
        return false;

    const uint8_t* bytes;
    if (!c.d.readBytes(length, &bytes))
        return false;

    char16_t* buffer = static_cast<char16_t *>(c.lifo.alloc(length * sizeof(char16_t)));
    for (size_t i = 0; i < length; i++)
        buffer[i] = bytes[i];

    *name = AstName(buffer, length);
    return true;
}

static bool
AstDecodeImport(AstDecodeContext& c, uint32_t importIndex, AstImport** import)
{
    uint32_t sigIndex = AstNoIndex;
    if (!AstDecodeSignatureIndex(c, &sigIndex))
        return false;

    AstRef sigRef;
    if (!AstDecodeGenerateRef(c, AstName(u"type"), sigIndex, &sigRef))
        return false;

    AstName moduleName;
    if (!AstDecodeName(c, &moduleName))
        return AstDecodeFail(c, "expected import module name");

    if (moduleName.empty())
        return AstDecodeFail(c, "module name cannot be empty");

    AstName funcName;
    if (!AstDecodeName(c, &funcName))
        return AstDecodeFail(c, "expected import func name");

    AstName importName;
    if (!AstDecodeGenerateName(c, AstName(u"import"), importIndex, &importName))
        return false;

    *import = new(c.lifo) AstImport(importName, moduleName, funcName, sigRef);
    if (!*import)
        return false;

    return true;
}

static bool
AstDecodeImportSection(AstDecodeContext& c)
{
    uint32_t sectionStart, sectionSize;
    if (!c.d.startSection(ImportSectionId, &sectionStart, &sectionSize))
        return AstDecodeFail(c, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numImports;
    if (!c.d.readVarU32(&numImports))
        return AstDecodeFail(c, "failed to read number of imports");

    if (numImports > MaxImports)
        return AstDecodeFail(c,  "too many imports");

    for (uint32_t i = 0; i < numImports; i++) {
        AstImport* import;
        if (!AstDecodeImport(c, i, &import))
            return false;
        if (!c.module().append(import))
            return false;
    }

    if (!c.d.finishSection(sectionStart, sectionSize))
        return AstDecodeFail(c, "import section byte size mismatch");

    return true;
}

static bool
AstDecodeMemorySection(AstDecodeContext& c)
{
    uint32_t sectionStart, sectionSize;
    if (!c.d.startSection(MemorySectionId, &sectionStart, &sectionSize))
        return AstDecodeFail(c, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t initialSizePages;
    if (!c.d.readVarU32(&initialSizePages))
        return AstDecodeFail(c, "expected initial memory size");

    CheckedInt<uint32_t> initialSize = initialSizePages;
    initialSize *= PageSize;
    if (!initialSize.isValid())
        return AstDecodeFail(c, "initial memory size too big");

    uint32_t maxSizePages;
    if (!c.d.readVarU32(&maxSizePages))
        return AstDecodeFail(c, "expected initial memory size");

    CheckedInt<uint32_t> maxSize = maxSizePages;
    maxSize *= PageSize;
    if (!maxSize.isValid())
        return AstDecodeFail(c, "maximum memory size too big");

    uint8_t exported;
    if (!c.d.readFixedU8(&exported))
        return AstDecodeFail(c, "expected exported byte");

    if (exported) {
        AstName fieldName(u"memory");
        AstExport* export_ = new(c.lifo) AstExport(fieldName, DefinitionKind::Memory);
        if (!export_ || !c.module().append(export_))
            return false;
    }

    if (!c.d.finishSection(sectionStart, sectionSize))
        return AstDecodeFail(c, "memory section byte size mismatch");

    c.module().setMemory(AstResizable(initialSizePages, Some(maxSizePages)));
    return true;
}

static bool
AstDecodeGlobal(AstDecodeContext& c, uint32_t i, AstGlobal* global)
{
    AstName name;
    if (!AstDecodeGenerateName(c, AstName(u"global"), i, &name))
        return false;

    ValType type;
    if (!c.d.readValType(&type))
        return AstDecodeFail(c, "bad global type");

    uint32_t flags;
    if (!c.d.readVarU32(&flags))
        return AstDecodeFail(c, "expected flags");

    if (flags & ~uint32_t(GlobalFlags::AllowedMask))
        return AstDecodeFail(c, "unexpected bits set in flags");

    if (!AstDecodeExpr(c))
        return false;

    AstDecodeStackItem item = c.iter().getResult();
    MOZ_ASSERT(item.popped == 0);
    if (!item.expr)
        return AstDecodeFail(c, "missing initializer expression");

    AstExpr* init = item.expr;

    if (!AstDecodeEnd(c))
        return AstDecodeFail(c, "missing initializer end");

    item = c.iter().getResult();
    MOZ_ASSERT(item.terminationKind == AstDecodeTerminationKind::End);

    *global = AstGlobal(name, type, flags, Some(init));
    return true;
}

static bool
AstDecodeGlobalSection(AstDecodeContext& c)
{
    uint32_t sectionStart, sectionSize;
    if (!c.d.startSection(GlobalSectionId, &sectionStart, &sectionSize))
        return AstDecodeFail(c, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numGlobals;
    if (!c.d.readVarU32(&numGlobals))
        return AstDecodeFail(c, "expected number of globals");

    for (uint32_t i = 0; i < numGlobals; i++) {
        auto* global = new(c.lifo) AstGlobal;
        if (!AstDecodeGlobal(c, i, global))
            return false;
        if (!c.module().append(global))
            return false;
    }

    if (!c.d.finishSection(sectionStart, sectionSize))
        return AstDecodeFail(c, "globals section byte size mismatch");

    return true;
}

static bool
AstDecodeFunctionExport(AstDecodeContext& c, AstExport** export_)
{
    uint32_t funcIndex;
    if (!c.d.readVarU32(&funcIndex))
        return AstDecodeFail(c, "expected export internal index");

    if (funcIndex >= c.funcSigs().length())
        return AstDecodeFail(c, "export function index out of range");

    AstName fieldName;
    if (!AstDecodeName(c, &fieldName))
        return AstDecodeFail(c, "expected export name");

    *export_ = new(c.lifo) AstExport(fieldName, DefinitionKind::Function,
                                     AstRef(AstName(), funcIndex));
    if (!*export_)
        return false;

    return true;
}

static bool
AstDecodeExportSection(AstDecodeContext& c)
{
    uint32_t sectionStart, sectionSize;
    if (!c.d.startSection(ExportSectionId, &sectionStart, &sectionSize))
        return AstDecodeFail(c, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numExports;
    if (!c.d.readVarU32(&numExports))
        return AstDecodeFail(c, "failed to read number of exports");

    if (numExports > MaxExports)
        return AstDecodeFail(c, "too many exports");

    for (uint32_t i = 0; i < numExports; i++) {
        AstExport* export_;
        if (!AstDecodeFunctionExport(c, &export_))
            return false;
        if (!c.module().append(export_))
            return false;
    }

    if (!c.d.finishSection(sectionStart, sectionSize))
        return AstDecodeFail(c, "export section byte size mismatch");

    return true;
}

static bool
AstDecodeFunctionBody(AstDecodeContext &c, uint32_t funcIndex, AstFunc** func)
{
    uint32_t offset = c.d.currentOffset();
    uint32_t bodySize;
    if (!c.d.readVarU32(&bodySize))
        return AstDecodeFail(c, "expected number of function body bytes");

    if (c.d.bytesRemain() < bodySize)
        return AstDecodeFail(c, "function body length too big");

    const uint8_t* bodyBegin = c.d.currentPosition();
    const uint8_t* bodyEnd = bodyBegin + bodySize;

    AstDecodeExprIter iter(c.d);

    uint32_t sigIndex = c.funcSigs()[funcIndex];
    const AstSig* sig = c.module().sigs()[sigIndex];

    AstValTypeVector vars(c.lifo);
    AstNameVector localsNames(c.lifo);
    AstExprVector body(c.lifo);

    ValTypeVector locals;
    if (!locals.appendAll(sig->args()))
        return false;

    if (!DecodeLocalEntries(c.d, &locals))
        return AstDecodeFail(c, "failed decoding local entries");

    c.startFunction(&iter, &locals);

    AstName funcName;
    if (!AstDecodeGenerateName(c, AstName(u"func"), funcIndex, &funcName))
        return false;

    uint32_t numParams = sig->args().length();
    uint32_t numLocals = locals.length();
    for (uint32_t i = numParams; i < numLocals; i++) {
        if (!vars.append(locals[i]))
            return false;
    }
    for (uint32_t i = 0; i < numLocals; i++) {
        AstName varName;
        if (!AstDecodeGenerateName(c, AstName(u"var"), i, &varName))
            return false;
        if (!localsNames.append(varName))
            return false;
    }

    if (!c.iter().readFunctionStart())
        return false;

    while (c.d.currentPosition() < bodyEnd) {
        if (!AstDecodeExpr(c))
            return false;
        AstDecodeStackItem item = c.iter().getResult();
        body.shrinkBy(item.popped);
        if (!body.append(item.expr))
            return false;
    }

    AstDecodeStackItem tmp;
    if (!c.iter().readFunctionEnd(sig->ret(), &tmp))
        return false;

    c.endFunction();

    if (c.d.currentPosition() != bodyEnd)
        return AstDecodeFail(c, "function body length mismatch");

    AstRef sigRef;
    if (!AstDecodeGenerateRef(c, AstName(u"type"), sigIndex, &sigRef))
        return false;

    *func = new(c.lifo) AstFunc(funcName, sigRef, Move(vars), Move(localsNames), Move(body));
    if (!*func)
        return false;
    (*func)->setOffset(offset);

    return true;
}

static bool
AstDecodeCodeSection(AstDecodeContext &c)
{
    uint32_t sectionStart, sectionSize;
    if (!c.d.startSection(CodeSectionId, &sectionStart, &sectionSize))
        return AstDecodeFail(c, "failed to start section");

    if (sectionStart == Decoder::NotStarted) {
        if (c.funcSigs().length() != 0)
            return AstDecodeFail(c, "expected function bodies");

        return false;
    }

    uint32_t numFuncBodies;
    if (!c.d.readVarU32(&numFuncBodies))
        return AstDecodeFail(c, "expected function body count");

    if (numFuncBodies != c.funcSigs().length())
        return AstDecodeFail(c, "function body count does not match function signature count");

    for (uint32_t funcIndex = 0; funcIndex < numFuncBodies; funcIndex++) {
        AstFunc* func;
        if (!AstDecodeFunctionBody(c, funcIndex, &func))
            return false;
        if (!c.module().append(func))
            return false;
    }

    if (!c.d.finishSection(sectionStart, sectionSize))
        return AstDecodeFail(c, "function section byte size mismatch");

    return true;
}

static bool
AstDecodeDataSection(AstDecodeContext &c)
{
    uint32_t sectionStart, sectionSize;
    if (!c.d.startSection(DataSectionId, &sectionStart, &sectionSize))
        return AstDecodeFail(c, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numSegments;
    if (!c.d.readVarU32(&numSegments))
        return AstDecodeFail(c, "failed to read number of data segments");

    const uint32_t heapLength = c.module().hasMemory() ? c.module().memory().initial() : 0;
    uint32_t prevEnd = 0;

    for (uint32_t i = 0; i < numSegments; i++) {
        uint32_t dstOffset;
        if (!c.d.readVarU32(&dstOffset))
            return AstDecodeFail(c, "expected segment destination offset");

        if (dstOffset < prevEnd)
            return AstDecodeFail(c, "data segments must be disjoint and ordered");

        uint32_t numBytes;
        if (!c.d.readVarU32(&numBytes))
            return AstDecodeFail(c, "expected segment size");

        if (dstOffset > heapLength || heapLength - dstOffset < numBytes)
            return AstDecodeFail(c, "data segment does not fit in memory");

        const uint8_t* src;
        if (!c.d.readBytes(numBytes, &src))
            return AstDecodeFail(c, "data segment shorter than declared");

        char16_t *buffer = static_cast<char16_t *>(c.lifo.alloc(numBytes * sizeof(char16_t)));
        for (size_t i = 0; i < numBytes; i++)
            buffer[i] = src[i];

        AstName name(buffer, numBytes);
        AstDataSegment* segment = new(c.lifo) AstDataSegment(dstOffset, name);
        if (!segment || !c.module().append(segment))
            return false;

        prevEnd = dstOffset + numBytes;
    }

    if (!c.d.finishSection(sectionStart, sectionSize))
        return AstDecodeFail(c, "data section byte size mismatch");

    return true;
}


bool
wasm::BinaryToAst(JSContext* cx, const uint8_t* bytes, uint32_t length,
                  LifoAlloc& lifo, AstModule** module)
{
    AstModule* result = new(lifo) AstModule(lifo);
    if (!result->init())
        return false;

    Decoder d(bytes, bytes + length);
    AstDecodeContext c(cx, lifo, d, *result, true);

    uint32_t u32;
    if (!d.readFixedU32(&u32) || u32 != MagicNumber)
        return AstDecodeFail(c, "failed to match magic number");

    if (!d.readFixedU32(&u32) || u32 != EncodingVersion)
        return AstDecodeFail(c, "failed to match binary version");

    if (!AstDecodeTypeSection(c))
        return false;

    if (!AstDecodeImportSection(c))
        return false;

    if (!AstDecodeFunctionSection(c))
        return false;

    if (!AstDecodeTableSection(c))
        return false;

    if (!AstDecodeMemorySection(c))
        return false;

    if (!AstDecodeGlobalSection(c))
        return false;

    if (!AstDecodeExportSection(c))
        return false;

    if (!AstDecodeCodeSection(c))
        return false;

    if (!AstDecodeDataSection(c))
        return false;

    while (!d.done()) {
        if (!d.skipSection())
            return AstDecodeFail(c, "failed to skip unknown section at end");
    }

    *module = result;
    return true;
}

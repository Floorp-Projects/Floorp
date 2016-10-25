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
#include "mozilla/Sprintf.h"

#include "jscntxt.h"

#include "asmjs/WasmBinaryFormat.h"
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
    AstDecodeTerminationKind terminationKind;
    ExprType type;

    explicit AstDecodeStackItem()
      : expr(nullptr),
        terminationKind(AstDecodeTerminationKind::Unknown),
        type(ExprType::Limit)
    {}
    explicit AstDecodeStackItem(AstDecodeTerminationKind terminationKind, ExprType type)
      : expr(nullptr),
        terminationKind(terminationKind),
        type(type)
    {}
    explicit AstDecodeStackItem(AstExpr* expr)
     : expr(expr),
       terminationKind(AstDecodeTerminationKind::Unknown),
       type(ExprType::Limit)
    {}
};

// We don't define a Value type because ExprIter doesn't push void values, which
// we actually need here because we're building an AST, so we maintain our own
// stack.
struct AstDecodePolicy : ExprIterPolicy
{
    // Enable validation because we can be called from wasmBinaryToText on bytes
    // which are not necessarily valid, and we shouldn't run the decoder in
    // non-validating mode on invalid code.
    static const bool Validate = true;

    static const bool Output = true;
};

typedef ExprIter<AstDecodePolicy> AstDecodeExprIter;

class AstDecodeContext
{
  public:
    typedef AstVector<uint32_t> AstIndexVector;
    typedef AstVector<AstDecodeStackItem> AstDecodeStack;
    typedef AstVector<uint32_t> DepthStack;

    JSContext* cx;
    LifoAlloc& lifo;
    Decoder& d;
    bool generateNames;

  private:
    AstModule& module_;
    AstIndexVector funcSigs_;
    AstDecodeExprIter *iter_;
    AstDecodeStack exprs_;
    DepthStack depths_;
    const ValTypeVector* locals_;
    GlobalDescVector globals_;
    AstNameVector blockLabels_;
    uint32_t currentLabelIndex_;
    ExprType retType_;

  public:
    AstDecodeContext(JSContext* cx, LifoAlloc& lifo, Decoder& d, AstModule& module,
                     bool generateNames)
     : cx(cx),
       lifo(lifo),
       d(d),
       generateNames(generateNames),
       module_(module),
       funcSigs_(lifo),
       iter_(nullptr),
       exprs_(lifo),
       depths_(lifo),
       locals_(nullptr),
       blockLabels_(lifo),
       currentLabelIndex_(0),
       retType_(ExprType::Limit)
    {}

    AstModule& module() { return module_; }
    AstIndexVector& funcSigs() { return funcSigs_; }
    AstDecodeExprIter& iter() { return *iter_; }
    AstDecodeStack& exprs() { return exprs_; }
    DepthStack& depths() { return depths_; }

    AstNameVector& blockLabels() { return blockLabels_; }

    ExprType retType() const { return retType_; }
    const ValTypeVector& locals() const { return *locals_; }

    bool addGlobalDesc(ValType type, bool isMutable, bool isImport) {
        if (isImport)
            return globals_.append(GlobalDesc(type, isMutable, globals_.length()));
        // No need to have the precise init expr value; we just need the right
        // type.
        Val dummy;
        switch (type) {
          case ValType::I32: dummy = Val(uint32_t(0)); break;
          case ValType::I64: dummy = Val(uint64_t(0)); break;
          case ValType::F32: dummy = Val(RawF32(0.f)); break;
          case ValType::F64: dummy = Val(RawF64(0.0)); break;
          default:           return false;
        }
        return globals_.append(GlobalDesc(InitExpr(dummy), isMutable));
    }
    const GlobalDescVector& globalDescs() const { return globals_; }

    void popBack() { return exprs().popBack(); }
    AstDecodeStackItem popCopy() { return exprs().popCopy(); }
    AstDecodeStackItem& top() { return exprs().back(); }
    MOZ_MUST_USE bool push(AstDecodeStackItem item) { return exprs().append(item); }

    bool needFirst() {
        for (size_t i = depths().back(); i < exprs().length(); ++i) {
            if (!exprs()[i].expr->isVoid())
                return true;
        }
        return false;
    }

    AstExpr* handleVoidExpr(AstExpr* voidNode)
    {
        MOZ_ASSERT(voidNode->isVoid());

        // To attach a node that "returns void" to the middle of an AST, wrap it
        // in a first node next to the node it should accompany.
        if (needFirst()) {
            AstExpr *prev = popCopy().expr;

            // If the previous/A node is already a First, reuse it.
            if (prev->kind() == AstExprKind::First) {
                if (!prev->as<AstFirst>().exprs().append(voidNode))
                    return nullptr;
                return prev;
            }

            AstExprVector exprs(lifo);
            if (!exprs.append(prev))
                return nullptr;
            if (!exprs.append(voidNode))
                return nullptr;

            return new(lifo) AstFirst(Move(exprs));
        }

        return voidNode;
    }

    void startFunction(AstDecodeExprIter *iter, const ValTypeVector* locals, ExprType retType)
    {
        iter_ = iter;
        locals_ = locals;
        currentLabelIndex_ = 0;
        retType_ = retType;
    }
    void endFunction()
    {
        iter_ = nullptr;
        locals_ = nullptr;
        retType_ = ExprType::Limit;
        MOZ_ASSERT(blockLabels_.length() == 0);
    }
    uint32_t nextLabelIndex()
    {
        return currentLabelIndex_++;
    }
};

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
        *ref = AstRef(index);
        return true;
    }

    AstName name;
    if (!AstDecodeGenerateName(c, prefix, index, &name))
        return false;
    MOZ_ASSERT(!name.empty());

    *ref = AstRef(name);
    ref->setIndex(index);
    return true;
}

static bool
AstDecodeCallArgs(AstDecodeContext& c, const AstSig& sig, AstExprVector* funcArgs)
{
    MOZ_ASSERT(c.iter().inReachableCode());

    const AstValTypeVector& args = sig.args();
    uint32_t numArgs = args.length();

    if (!funcArgs->resize(numArgs))
        return false;

    for (size_t i = 0; i < numArgs; ++i) {
        ValType argType = args[i];
        AstDecodeStackItem item;
        if (!c.iter().readCallArg(argType, numArgs, i, nullptr))
            return false;
        (*funcArgs)[i] = c.exprs()[c.exprs().length() - numArgs + i].expr;
    }
    c.exprs().shrinkBy(numArgs);

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
AstDecodeDrop(AstDecodeContext& c)
{
    if (!c.iter().readDrop())
        return false;

    AstDecodeStackItem value = c.popCopy();

    AstExpr* tmp = new(c.lifo) AstDrop(*value.expr);
    if (!tmp)
        return false;

    tmp = c.handleVoidExpr(tmp);
    if (!tmp)
        return false;

    if (!c.push(AstDecodeStackItem(tmp)))
        return false;

    return true;
}

static bool
AstDecodeCall(AstDecodeContext& c)
{
    uint32_t calleeIndex;
    if (!c.iter().readCall(&calleeIndex))
        return false;

    if (!c.iter().inReachableCode())
        return true;

    uint32_t sigIndex;
    AstRef funcRef;
    if (calleeIndex < c.module().funcImportNames().length()) {
        AstImport* import = c.module().imports()[calleeIndex];
        sigIndex = import->funcSig().index();
        funcRef = AstRef(import->name());
    } else {
        uint32_t funcDefIndex = calleeIndex - c.module().funcImportNames().length();
        if (funcDefIndex >= c.funcSigs().length())
            return c.iter().fail("callee index out of range");

        sigIndex = c.funcSigs()[funcDefIndex];

        if (!AstDecodeGenerateRef(c, AstName(u"func"), calleeIndex, &funcRef))
            return false;
    }

    const AstSig* sig = c.module().sigs()[sigIndex];

    AstExprVector args(c.lifo);
    if (!AstDecodeCallArgs(c, *sig, &args))
        return false;

    if (!AstDecodeCallReturn(c, *sig))
        return false;

    AstCall* call = new(c.lifo) AstCall(Expr::Call, sig->ret(), funcRef, Move(args));
    if (!call)
        return false;

    AstExpr* result = call;
    if (IsVoid(sig->ret()))
        result = c.handleVoidExpr(call);

    if (!c.push(AstDecodeStackItem(result)))
        return false;

    return true;
}

static bool
AstDecodeCallIndirect(AstDecodeContext& c)
{
    uint32_t sigIndex;
    if (!c.iter().readCallIndirect(&sigIndex, nullptr))
        return false;

    if (!c.iter().inReachableCode())
        return true;

    if (sigIndex >= c.module().sigs().length())
        return c.iter().fail("signature index out of range");

    AstDecodeStackItem index = c.popCopy();

    AstRef sigRef;
    if (!AstDecodeGenerateRef(c, AstName(u"type"), sigIndex, &sigRef))
        return false;

    const AstSig* sig = c.module().sigs()[sigIndex];
    AstExprVector args(c.lifo);
    if (!AstDecodeCallArgs(c, *sig, &args))
        return false;

    if (!AstDecodeCallReturn(c, *sig))
        return false;

    AstCallIndirect* call = new(c.lifo) AstCallIndirect(sigRef, sig->ret(),
                                                        Move(args), index.expr);
    if (!call)
        return false;

    AstExpr* result = call;
    if (IsVoid(sig->ret()))
        result = c.handleVoidExpr(call);

    if (!c.push(AstDecodeStackItem(result)))
        return false;

    return true;
}

static bool
AstDecodeGetBlockRef(AstDecodeContext& c, uint32_t depth, AstRef* ref)
{
    if (!c.generateNames || depth >= c.blockLabels().length()) {
        // Also ignoring if it's a function body label.
        *ref = AstRef(depth);
        return true;
    }

    uint32_t index = c.blockLabels().length() - depth - 1;
    if (c.blockLabels()[index].empty()) {
        if (!AstDecodeGenerateName(c, AstName(u"label"), c.nextLabelIndex(), &c.blockLabels()[index]))
            return false;
    }
    *ref = AstRef(c.blockLabels()[index]);
    ref->setIndex(depth);
    return true;
}

static bool
AstDecodeBrTable(AstDecodeContext& c)
{
    uint32_t tableLength;
    ExprType type;
    if (!c.iter().readBrTable(&tableLength, &type, nullptr, nullptr))
        return false;

    AstRefVector table(c.lifo);
    if (!table.resize(tableLength))
        return false;

    uint32_t depth;
    for (size_t i = 0, e = tableLength; i < e; ++i) {
        if (!c.iter().readBrTableEntry(&type, nullptr, &depth))
            return false;
        if (!AstDecodeGetBlockRef(c, depth, &table[i]))
            return false;
    }

    // Read the default label.
    if (!c.iter().readBrTableDefault(&type, nullptr, &depth))
        return false;

    AstDecodeStackItem index = c.popCopy();
    AstDecodeStackItem value;
    if (!IsVoid(type))
        value = c.popCopy();

    AstRef def;
    if (!AstDecodeGetBlockRef(c, depth, &def))
        return false;

    AstBranchTable* branchTable = new(c.lifo) AstBranchTable(*index.expr,
                                                             def, Move(table), value.expr);
    if (!branchTable)
        return false;

    if (!c.push(AstDecodeStackItem(branchTable)))
        return false;

    return true;
}

static bool
AstDecodeBlock(AstDecodeContext& c, Expr expr)
{
    MOZ_ASSERT(expr == Expr::Block || expr == Expr::Loop);

    if (!c.blockLabels().append(AstName()))
        return false;

    if (expr == Expr::Loop) {
      if (!c.iter().readLoop())
          return false;
    } else {
      if (!c.iter().readBlock())
          return false;
    }

    if (!c.depths().append(c.exprs().length()))
        return false;

    ExprType type;
    while (true) {
        if (!AstDecodeExpr(c))
            return false;

        const AstDecodeStackItem& item = c.top();
        if (!item.expr) { // Expr::End was found
            type = item.type;
            c.popBack();
            break;
        }
    }

    AstExprVector exprs(c.lifo);
    for (auto i = c.exprs().begin() + c.depths().back(), e = c.exprs().end();
         i != e; ++i) {
        if (!exprs.append(i->expr))
            return false;
    }
    c.exprs().shrinkTo(c.depths().popCopy());

    AstName name = c.blockLabels().popCopy();
    AstBlock* block = new(c.lifo) AstBlock(expr, type, name, Move(exprs));
    if (!block)
        return false;

    AstExpr* result = block;
    if (IsVoid(type))
        result = c.handleVoidExpr(block);

    if (!c.push(AstDecodeStackItem(result)))
        return false;

    return true;
}

static bool
AstDecodeIf(AstDecodeContext& c)
{
    if (!c.iter().readIf(nullptr))
        return false;

    AstDecodeStackItem cond = c.popCopy();

    bool hasElse = false;

    if (!c.depths().append(c.exprs().length()))
        return false;

    if (!c.blockLabels().append(AstName()))
        return false;

    ExprType type;
    while (true) {
        if (!AstDecodeExpr(c))
            return false;

        const AstDecodeStackItem& item = c.top();
        if (!item.expr) { // Expr::End was found
            hasElse = item.terminationKind == AstDecodeTerminationKind::Else;
            type = item.type;
            c.popBack();
            break;
        }
    }

    AstExprVector thenExprs(c.lifo);
    for (auto i = c.exprs().begin() + c.depths().back(), e = c.exprs().end();
         i != e; ++i) {
        if (!thenExprs.append(i->expr))
            return false;
    }
    c.exprs().shrinkTo(c.depths().back());

    AstExprVector elseExprs(c.lifo);
    if (hasElse) {
        while (true) {
            if (!AstDecodeExpr(c))
                return false;

            const AstDecodeStackItem& item = c.top();
            if (!item.expr) { // Expr::End was found
                c.popBack();
                break;
            }
        }

        for (auto i = c.exprs().begin() + c.depths().back(), e = c.exprs().end();
             i != e; ++i) {
            if (!elseExprs.append(i->expr))
                return false;
        }
        c.exprs().shrinkTo(c.depths().back());
    }

    c.depths().popBack();

    AstName name = c.blockLabels().popCopy();

    AstIf* if_ = new(c.lifo) AstIf(type, cond.expr, name, Move(thenExprs), Move(elseExprs));
    if (!if_)
        return false;

    AstExpr* result = if_;
    if (IsVoid(type))
        result = c.handleVoidExpr(if_);

    if (!c.push(AstDecodeStackItem(result)))
        return false;

    return true;
}

static bool
AstDecodeEnd(AstDecodeContext& c)
{
    LabelKind kind;
    ExprType type;
    if (!c.iter().readEnd(&kind, &type, nullptr))
        return false;

    if (!c.push(AstDecodeStackItem(AstDecodeTerminationKind::End, type)))
        return false;

    return true;
}

static bool
AstDecodeElse(AstDecodeContext& c)
{
    ExprType type;

    if (!c.iter().readElse(&type, nullptr))
        return false;

    if (!c.push(AstDecodeStackItem(AstDecodeTerminationKind::Else, type)))
        return false;

    return true;
}

static bool
AstDecodeNop(AstDecodeContext& c)
{
    if (!c.iter().readNop())
        return false;

    AstExpr* tmp = new(c.lifo) AstNop();
    if (!tmp)
        return false;

    tmp = c.handleVoidExpr(tmp);
    if (!tmp)
        return false;

    if (!c.push(AstDecodeStackItem(tmp)))
        return false;

    return true;
}

static bool
AstDecodeUnary(AstDecodeContext& c, ValType type, Expr expr)
{
    if (!c.iter().readUnary(type, nullptr))
        return false;

    AstDecodeStackItem op = c.popCopy();

    AstUnaryOperator* unary = new(c.lifo) AstUnaryOperator(expr, op.expr);
    if (!unary)
        return false;

    if (!c.push(AstDecodeStackItem(unary)))
        return false;

    return true;
}

static bool
AstDecodeBinary(AstDecodeContext& c, ValType type, Expr expr)
{
    if (!c.iter().readBinary(type, nullptr, nullptr))
        return false;

    AstDecodeStackItem rhs = c.popCopy();
    AstDecodeStackItem lhs = c.popCopy();

    AstBinaryOperator* binary = new(c.lifo) AstBinaryOperator(expr, lhs.expr, rhs.expr);
    if (!binary)
        return false;

    if (!c.push(AstDecodeStackItem(binary)))
        return false;

    return true;
}

static bool
AstDecodeSelect(AstDecodeContext& c)
{
    ValType type;
    if (!c.iter().readSelect(&type, nullptr, nullptr, nullptr))
        return false;

    AstDecodeStackItem selectFalse = c.popCopy();
    AstDecodeStackItem selectTrue = c.popCopy();
    AstDecodeStackItem cond = c.popCopy();

    AstTernaryOperator* ternary = new(c.lifo) AstTernaryOperator(Expr::Select, cond.expr, selectTrue.expr, selectFalse.expr);
    if (!ternary)
        return false;

    if (!c.push(AstDecodeStackItem(ternary)))
        return false;

    return true;
}

static bool
AstDecodeComparison(AstDecodeContext& c, ValType type, Expr expr)
{
    if (!c.iter().readComparison(type, nullptr, nullptr))
        return false;

    AstDecodeStackItem rhs = c.popCopy();
    AstDecodeStackItem lhs = c.popCopy();

    AstComparisonOperator* comparison = new(c.lifo) AstComparisonOperator(expr, lhs.expr, rhs.expr);
    if (!comparison)
        return false;

    if (!c.push(AstDecodeStackItem(comparison)))
        return false;

    return true;
}

static bool
AstDecodeConversion(AstDecodeContext& c, ValType fromType, ValType toType, Expr expr)
{
    if (!c.iter().readConversion(fromType, toType, nullptr))
        return false;

    AstDecodeStackItem op = c.popCopy();

    AstConversionOperator* conversion = new(c.lifo) AstConversionOperator(expr, op.expr);
    if (!conversion)
        return false;

    if (!c.push(AstDecodeStackItem(conversion)))
        return false;

    return true;
}

static AstLoadStoreAddress
AstDecodeLoadStoreAddress(const LinearMemoryAddress<Nothing>& addr, const AstDecodeStackItem& item)
{
    uint32_t flags = FloorLog2(addr.align);
    return AstLoadStoreAddress(item.expr, flags, addr.offset);
}

static bool
AstDecodeLoad(AstDecodeContext& c, ValType type, uint32_t byteSize, Expr expr)
{
    LinearMemoryAddress<Nothing> addr;
    if (!c.iter().readLoad(type, byteSize, &addr))
        return false;

    AstDecodeStackItem item = c.popCopy();

    AstLoad* load = new(c.lifo) AstLoad(expr, AstDecodeLoadStoreAddress(addr, item));
    if (!load)
        return false;

    if (!c.push(AstDecodeStackItem(load)))
        return false;

    return true;
}

static bool
AstDecodeStore(AstDecodeContext& c, ValType type, uint32_t byteSize, Expr expr)
{
    LinearMemoryAddress<Nothing> addr;
    if (!c.iter().readStore(type, byteSize, &addr, nullptr))
        return false;

    AstDecodeStackItem value = c.popCopy();
    AstDecodeStackItem item = c.popCopy();

    AstStore* store = new(c.lifo) AstStore(expr, AstDecodeLoadStoreAddress(addr, item), value.expr);
    if (!store)
        return false;

    AstExpr* wrapped = c.handleVoidExpr(store);
    if (!wrapped)
        return false;

    if (!c.push(AstDecodeStackItem(wrapped)))
        return false;

    return true;
}

static bool
AstDecodeCurrentMemory(AstDecodeContext& c)
{
    if (!c.iter().readCurrentMemory())
        return false;

    AstCurrentMemory* gm = new(c.lifo) AstCurrentMemory();
    if (!gm)
        return false;

    if (!c.push(AstDecodeStackItem(gm)))
        return false;

    return true;
}

static bool
AstDecodeGrowMemory(AstDecodeContext& c)
{
    if (!c.iter().readGrowMemory(nullptr))
        return false;

    AstDecodeStackItem op = c.popCopy();

    AstGrowMemory* gm = new(c.lifo) AstGrowMemory(op.expr);
    if (!gm)
        return false;

    if (!c.push(AstDecodeStackItem(gm)))
        return false;

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
    if (expr == Expr::Br) {
        if (!c.iter().readBr(&depth, &type, nullptr))
            return false;
        if (!IsVoid(type))
            value = c.popCopy();
    } else {
        if (!c.iter().readBrIf(&depth, &type, nullptr, nullptr))
            return false;
        if (!IsVoid(type))
            value = c.popCopy();
        cond = c.popCopy();
    }

    AstRef depthRef;
    if (!AstDecodeGetBlockRef(c, depth, &depthRef))
        return false;

    if (expr == Expr::Br || !value.expr)
        type = ExprType::Void;
    AstBranch* branch = new(c.lifo) AstBranch(expr, type, cond.expr, depthRef, value.expr);
    if (!branch)
        return false;

    if (!c.push(AstDecodeStackItem(branch)))
        return false;

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

    if (!c.push(AstDecodeStackItem(getLocal)))
        return false;

    return true;
}

static bool
AstDecodeSetLocal(AstDecodeContext& c)
{
    uint32_t setLocalId;
    if (!c.iter().readSetLocal(c.locals(), &setLocalId, nullptr))
        return false;

    AstDecodeStackItem setLocalValue = c.popCopy();

    AstRef localRef;
    if (!AstDecodeGenerateRef(c, AstName(u"var"), setLocalId, &localRef))
        return false;

    AstSetLocal* setLocal = new(c.lifo) AstSetLocal(localRef, *setLocalValue.expr);
    if (!setLocal)
        return false;

    AstExpr* expr = c.handleVoidExpr(setLocal);
    if (!expr)
        return false;

    if (!c.push(AstDecodeStackItem(expr)))
        return false;

    return true;
}

static bool
AstDecodeTeeLocal(AstDecodeContext& c)
{
    uint32_t teeLocalId;
    if (!c.iter().readTeeLocal(c.locals(), &teeLocalId, nullptr))
        return false;

    AstDecodeStackItem teeLocalValue = c.popCopy();

    AstRef localRef;
    if (!AstDecodeGenerateRef(c, AstName(u"var"), teeLocalId, &localRef))
        return false;

    AstTeeLocal* teeLocal = new(c.lifo) AstTeeLocal(localRef, *teeLocalValue.expr);
    if (!teeLocal)
        return false;

    if (!c.push(AstDecodeStackItem(teeLocal)))
        return false;

    return true;
}

static bool
AstDecodeGetGlobal(AstDecodeContext& c)
{
    uint32_t globalId;
    if (!c.iter().readGetGlobal(c.globalDescs(), &globalId))
        return false;

    AstRef globalRef;
    if (!AstDecodeGenerateRef(c, AstName(u"global"), globalId, &globalRef))
        return false;

    auto* getGlobal = new(c.lifo) AstGetGlobal(globalRef);
    if (!getGlobal)
        return false;

    if (!c.push(AstDecodeStackItem(getGlobal)))
        return false;

    return true;
}

static bool
AstDecodeSetGlobal(AstDecodeContext& c)
{
    uint32_t globalId;
    if (!c.iter().readSetGlobal(c.globalDescs(), &globalId, nullptr))
        return false;

    AstDecodeStackItem value = c.popCopy();

    AstRef globalRef;
    if (!AstDecodeGenerateRef(c, AstName(u"global"), globalId, &globalRef))
        return false;

    auto* setGlobal = new(c.lifo) AstSetGlobal(globalRef, *value.expr);
    if (!setGlobal)
        return false;

    AstExpr* expr = c.handleVoidExpr(setGlobal);
    if (!expr)
        return false;

    if (!c.push(AstDecodeStackItem(expr)))
        return false;

    return true;
}

static bool
AstDecodeReturn(AstDecodeContext& c)
{
    if (!c.iter().readReturn(nullptr))
        return false;

    AstDecodeStackItem result;
    if (!IsVoid(c.retType()))
       result = c.popCopy();

    AstReturn* ret = new(c.lifo) AstReturn(result.expr);
    if (!ret)
        return false;

    if (!c.push(AstDecodeStackItem(ret)))
        return false;

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
        if (!AstDecodeNop(c))
            return false;
        break;
      case Expr::Drop:
        if (!AstDecodeDrop(c))
            return false;
        break;
      case Expr::Call:
        if (!AstDecodeCall(c))
            return false;
        break;
      case Expr::CallIndirect:
        if (!AstDecodeCallIndirect(c))
            return false;
        break;
      case Expr::I32Const:
        int32_t i32;
        if (!c.iter().readI32Const(&i32))
            return false;
        tmp = new(c.lifo) AstConst(Val((uint32_t)i32));
        if (!tmp || !c.push(AstDecodeStackItem(tmp)))
            return false;
        break;
      case Expr::I64Const:
        int64_t i64;
        if (!c.iter().readI64Const(&i64))
            return false;
        tmp = new(c.lifo) AstConst(Val((uint64_t)i64));
        if (!tmp || !c.push(AstDecodeStackItem(tmp)))
            return false;
        break;
      case Expr::F32Const: {
        RawF32 f32;
        if (!c.iter().readF32Const(&f32))
            return false;
        tmp = new(c.lifo) AstConst(Val(f32));
        if (!tmp || !c.push(AstDecodeStackItem(tmp)))
            return false;
        break;
      }
      case Expr::F64Const: {
        RawF64 f64;
        if (!c.iter().readF64Const(&f64))
            return false;
        tmp = new(c.lifo) AstConst(Val(f64));
        if (!tmp || !c.push(AstDecodeStackItem(tmp)))
            return false;
        break;
      }
      case Expr::GetLocal:
        if (!AstDecodeGetLocal(c))
            return false;
        break;
      case Expr::SetLocal:
        if (!AstDecodeSetLocal(c))
            return false;
        break;
      case Expr::TeeLocal:
        if (!AstDecodeTeeLocal(c))
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
      case Expr::I32WrapI64:
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
      case Expr::CurrentMemory:
        if (!AstDecodeCurrentMemory(c))
            return false;
        break;
      case Expr::GrowMemory:
        if (!AstDecodeGrowMemory(c))
            return false;
        break;
      case Expr::SetGlobal:
        if (!AstDecodeSetGlobal(c))
            return false;
        break;
      case Expr::GetGlobal:
        if (!AstDecodeGetGlobal(c))
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
        if (!c.push(AstDecodeStackItem(tmp)))
            return false;
        break;
      default:
        // Note: it's important not to remove this default since readExpr()
        // can return Expr values for which there is no enumerator.
        return c.iter().unrecognizedOpcode(expr);
    }

    AstExpr* lastExpr = c.top().expr;
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
    if (!c.d.startSection(SectionId::Type, &sectionStart, &sectionSize, "type"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numSigs;
    if (!c.d.readVarU32(&numSigs))
        return c.d.fail("expected number of signatures");

    if (numSigs > MaxSigs)
        return c.d.fail("too many signatures");

    for (uint32_t sigIndex = 0; sigIndex < numSigs; sigIndex++) {
        uint32_t form;
        if (!c.d.readVarU32(&form) || form != uint32_t(TypeCode::Func))
            return c.d.fail("expected function form");

        uint32_t numArgs;
        if (!c.d.readVarU32(&numArgs))
            return c.d.fail("bad number of function args");

        if (numArgs > MaxArgsPerFunc)
            return c.d.fail("too many arguments in signature");

        AstValTypeVector args(c.lifo);
        if (!args.resize(numArgs))
            return false;

        for (uint32_t i = 0; i < numArgs; i++) {
            if (!c.d.readValType(&args[i]))
                return c.d.fail("bad value type");
        }

        uint32_t numRets;
        if (!c.d.readVarU32(&numRets))
            return c.d.fail("bad number of function returns");

        if (numRets > 1)
            return c.d.fail("too many returns in signature");

        ExprType result = ExprType::Void;

        if (numRets == 1) {
            ValType type;
            if (!c.d.readValType(&type))
                return c.d.fail("bad expression type");

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

    if (!c.d.finishSection(sectionStart, sectionSize, "type"))
        return false;

    return true;
}

static bool
AstDecodeSignatureIndex(AstDecodeContext& c, uint32_t* sigIndex)
{
    if (!c.d.readVarU32(sigIndex))
        return c.d.fail("expected signature index");

    if (*sigIndex >= c.module().sigs().length())
        return c.d.fail("signature index out of range");

    return true;
}

static bool
AstDecodeFunctionSection(AstDecodeContext& c)
{
    uint32_t sectionStart, sectionSize;
    if (!c.d.startSection(SectionId::Function, &sectionStart, &sectionSize, "function"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numDecls;
    if (!c.d.readVarU32(&numDecls))
        return c.d.fail("expected number of declarations");

    if (numDecls > MaxFuncs)
        return c.d.fail("too many functions");

    if (!c.funcSigs().resize(numDecls))
        return false;

    for (uint32_t i = 0; i < numDecls; i++) {
        if (!AstDecodeSignatureIndex(c, &c.funcSigs()[i]))
            return false;
    }

    if (!c.d.finishSection(sectionStart, sectionSize, "function"))
        return false;

    return true;
}

static bool
AstDecodeTableSection(AstDecodeContext& c)
{
    uint32_t sectionStart, sectionSize;
    if (!c.d.startSection(SectionId::Table, &sectionStart, &sectionSize, "table"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numTables;
    if (!c.d.readVarU32(&numTables))
        return c.d.fail("failed to read number of tables");

    if (numTables != 1)
        return c.d.fail("the number of tables must be exactly one");

    uint32_t typeConstructorValue;
    if (!c.d.readVarU32(&typeConstructorValue))
        return c.d.fail("expected type constructor kind");

    if (typeConstructorValue != uint32_t(TypeCode::AnyFunc))
        return c.d.fail("unknown type constructor kind");

    Limits table;
    if (!DecodeLimits(c.d, &table))
        return false;

    if (table.initial > MaxTableElems)
        return c.d.fail("too many table elements");

    if (c.module().hasTable())
        return c.d.fail("already have a table");

    AstName name;
    if (!AstDecodeGenerateName(c, AstName(u"table"), c.module().tables().length(), &name))
        return false;

    if (!c.module().addTable(name, table))
        return false;

    if (!c.d.finishSection(sectionStart, sectionSize, "table"))
        return false;

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
AstDecodeLimitsTable(AstDecodeContext& c, Limits* limits)
{
    uint32_t kind;
    if (!c.d.readVarU32(&kind))
        return false;

    if (kind != uint32_t(TypeCode::AnyFunc))
        return c.d.fail("unknown type constructor kind");

    if (!DecodeLimits(c.d, limits))
        return false;

    return true;
}

static bool
AstDecodeImport(AstDecodeContext& c, uint32_t importIndex, AstImport** import)
{
    AstName moduleName;
    if (!AstDecodeName(c, &moduleName))
        return c.d.fail("expected import module name");

    if (moduleName.empty())
        return c.d.fail("module name cannot be empty");

    AstName fieldName;
    if (!AstDecodeName(c, &fieldName))
        return c.d.fail("expected import field name");

    uint32_t kind;
    if (!c.d.readVarU32(&kind))
        return c.d.fail("expected import kind");

    switch (kind) {
      case uint32_t(DefinitionKind::Function): {
        AstName importName;
        if (!AstDecodeGenerateName(c, AstName(u"import"), importIndex, &importName))
            return false;

        uint32_t sigIndex = AstNoIndex;
        if (!AstDecodeSignatureIndex(c, &sigIndex))
            return false;

        AstRef sigRef;
        if (!AstDecodeGenerateRef(c, AstName(u"type"), sigIndex, &sigRef))
            return false;

        *import = new(c.lifo) AstImport(importName, moduleName, fieldName, sigRef);
        break;
      }
      case uint32_t(DefinitionKind::Global): {
        AstName importName;
        if (!AstDecodeGenerateName(c, AstName(u"global"), importIndex, &importName))
            return false;

        ValType type;
        bool isMutable;
        if (!DecodeGlobalType(c.d, &type, &isMutable))
            return false;

        if (!c.addGlobalDesc(type, isMutable, /* import */ true))
            return false;

        *import = new(c.lifo) AstImport(importName, moduleName, fieldName,
                                        AstGlobal(importName, type, isMutable));
        break;
      }
      case uint32_t(DefinitionKind::Table): {
        if (c.module().hasTable())
            return c.d.fail("already have default table");

        AstName importName;
        if (!AstDecodeGenerateName(c, AstName(u"table"), importIndex, &importName))
            return false;

        Limits table;
        if (!AstDecodeLimitsTable(c, &table))
            return false;

        *import = new(c.lifo) AstImport(importName, moduleName, fieldName,
                                        DefinitionKind::Table, table);
        break;
      }
      case uint32_t(DefinitionKind::Memory): {
        AstName importName;
        if (!AstDecodeGenerateName(c, AstName(u"memory"), importIndex, &importName))
            return false;

        Limits memory;
        if (!DecodeMemoryLimits(c.d, c.module().hasMemory(), &memory))
            return false;

        *import = new(c.lifo) AstImport(importName, moduleName, fieldName,
                                        DefinitionKind::Memory, memory);
        break;
      }
      default:
        return c.d.fail("unknown import kind");
    }

    if (!*import)
        return false;

    return true;
}

static bool
AstDecodeImportSection(AstDecodeContext& c)
{
    uint32_t sectionStart, sectionSize;
    if (!c.d.startSection(SectionId::Import, &sectionStart, &sectionSize, "import"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numImports;
    if (!c.d.readVarU32(&numImports))
        return c.d.fail("failed to read number of imports");

    if (numImports > MaxImports)
        return c.d.fail( "too many imports");

    for (uint32_t i = 0; i < numImports; i++) {
        AstImport* import = nullptr;
        if (!AstDecodeImport(c, i, &import))
            return false;
        if (!c.module().append(import))
            return false;
    }

    if (!c.d.finishSection(sectionStart, sectionSize, "import"))
        return false;

    return true;
}

static bool
AstDecodeMemorySection(AstDecodeContext& c)
{
    bool present;
    Limits memory;
    if (!DecodeMemorySection(c.d, c.module().hasMemory(), &memory, &present))
        return false;

    if (present) {
        AstName name;
        if (!AstDecodeGenerateName(c, AstName(u"memory"), c.module().memories().length(), &name))
            return false;
        if (!c.module().addMemory(name, memory))
            return false;
    }

    return true;
}

static AstExpr*
ToAstExpr(AstDecodeContext& c, const InitExpr& initExpr)
{
    switch (initExpr.kind()) {
      case InitExpr::Kind::Constant: {
        return new(c.lifo) AstConst(Val(initExpr.val()));
      }
      case InitExpr::Kind::GetGlobal: {
        AstRef globalRef;
        if (!AstDecodeGenerateRef(c, AstName(u"global"), initExpr.globalIndex(), &globalRef))
            return nullptr;
        return new(c.lifo) AstGetGlobal(globalRef);
      }
    }
    return nullptr;
}

static bool
AstDecodeInitializerExpression(AstDecodeContext& c, ValType type, AstExpr** init)
{
    InitExpr initExpr;
    if (!DecodeInitializerExpression(c.d, c.globalDescs(), type, &initExpr))
        return false;

    *init = ToAstExpr(c, initExpr);
    return !!*init;
}

static bool
AstDecodeGlobal(AstDecodeContext& c, uint32_t i, AstGlobal* global)
{
    AstName name;
    if (!AstDecodeGenerateName(c, AstName(u"global"), i, &name))
        return false;

    ValType type;
    bool isMutable;
    if (!DecodeGlobalType(c.d, &type, &isMutable))
        return false;

    AstExpr* init;
    if (!AstDecodeInitializerExpression(c, type, &init))
        return false;

    if (!c.addGlobalDesc(type, isMutable, /* import */ false))
        return false;

    *global = AstGlobal(name, type, isMutable, Some(init));
    return true;
}

static bool
AstDecodeGlobalSection(AstDecodeContext& c)
{
    uint32_t sectionStart, sectionSize;
    if (!c.d.startSection(SectionId::Global, &sectionStart, &sectionSize, "global"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numGlobals;
    if (!c.d.readVarU32(&numGlobals))
        return c.d.fail("expected number of globals");

    uint32_t numImported = c.globalDescs().length();

    for (uint32_t i = 0; i < numGlobals; i++) {
        auto* global = new(c.lifo) AstGlobal;
        if (!AstDecodeGlobal(c, i + numImported, global))
            return false;
        if (!c.module().append(global))
            return false;
    }

    if (!c.d.finishSection(sectionStart, sectionSize, "global"))
        return false;

    return true;
}

static bool
AstDecodeExport(AstDecodeContext& c, AstExport** export_)
{
    AstName fieldName;
    if (!AstDecodeName(c, &fieldName))
        return c.d.fail("expected export name");

    uint32_t kindValue;
    if (!c.d.readVarU32(&kindValue))
        return c.d.fail("expected export kind");

    uint32_t index;
    if (!c.d.readVarU32(&index))
        return c.d.fail("expected export internal index");

    *export_ = new(c.lifo) AstExport(fieldName, DefinitionKind(kindValue), AstRef(index));
    if (!*export_)
        return false;

    return true;
}

static bool
AstDecodeExportSection(AstDecodeContext& c)
{
    uint32_t sectionStart, sectionSize;
    if (!c.d.startSection(SectionId::Export, &sectionStart, &sectionSize, "export"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numExports;
    if (!c.d.readVarU32(&numExports))
        return c.d.fail("failed to read number of exports");

    if (numExports > MaxExports)
        return c.d.fail("too many exports");

    for (uint32_t i = 0; i < numExports; i++) {
        AstExport* export_ = nullptr;
        if (!AstDecodeExport(c, &export_))
            return false;
        if (!c.module().append(export_))
            return false;
    }

    if (!c.d.finishSection(sectionStart, sectionSize, "export"))
        return false;

    return true;
}

static bool
AstDecodeFunctionBody(AstDecodeContext &c, uint32_t funcIndex, AstFunc** func)
{
    uint32_t offset = c.d.currentOffset();
    uint32_t bodySize;
    if (!c.d.readVarU32(&bodySize))
        return c.d.fail("expected number of function body bytes");

    if (c.d.bytesRemain() < bodySize)
        return c.d.fail("function body length too big");

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
        return c.d.fail("failed decoding local entries");

    c.startFunction(&iter, &locals, sig->ret());

    AstName funcName;
    if (!AstDecodeGenerateName(c, AstName(u"func"),
                               c.module().funcImportNames().length() + funcIndex,
                               &funcName))
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

    if (!c.iter().readFunctionStart(sig->ret()))
        return false;

    if (!c.depths().append(c.exprs().length()))
        return false;

    while (c.d.currentPosition() < bodyEnd) {
        if (!AstDecodeExpr(c))
            return false;

        const AstDecodeStackItem& item = c.top();
        if (!item.expr) { // Expr::End was found
            c.popBack();
            break;
        }
    }

    for (auto i = c.exprs().begin() + c.depths().back(), e = c.exprs().end();
         i != e; ++i) {
        if (!body.append(i->expr))
            return false;
    }
    c.exprs().shrinkTo(c.depths().popCopy());

    if (!c.iter().readFunctionEnd())
        return false;

    c.endFunction();

    if (c.d.currentPosition() != bodyEnd)
        return c.d.fail("function body length mismatch");

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
    if (!c.d.startSection(SectionId::Code, &sectionStart, &sectionSize, "code"))
        return false;

    if (sectionStart == Decoder::NotStarted) {
        if (c.funcSigs().length() != 0)
            return c.d.fail("expected function bodies");

        return false;
    }

    uint32_t numFuncBodies;
    if (!c.d.readVarU32(&numFuncBodies))
        return c.d.fail("expected function body count");

    if (numFuncBodies != c.funcSigs().length())
        return c.d.fail("function body count does not match function signature count");

    for (uint32_t funcIndex = 0; funcIndex < numFuncBodies; funcIndex++) {
        AstFunc* func;
        if (!AstDecodeFunctionBody(c, funcIndex, &func))
            return false;
        if (!c.module().append(func))
            return false;
    }

    if (!c.d.finishSection(sectionStart, sectionSize, "code"))
        return false;

    return true;
}

static bool
AstDecodeDataSection(AstDecodeContext &c)
{
    DataSegmentVector segments;
    bool hasMemory = c.module().hasMemory();

    MOZ_ASSERT(c.module().memories().length() <= 1, "at most one memory in MVP");
    uint32_t memByteLength = hasMemory ? c.module().memories()[0].limits.initial : 0;

    if (!DecodeDataSection(c.d, hasMemory, memByteLength, c.globalDescs(), &segments))
        return false;

    for (DataSegment& s : segments) {
        const uint8_t* src = c.d.begin() + s.bytecodeOffset;
        char16_t* buffer = static_cast<char16_t*>(c.lifo.alloc(s.length * sizeof(char16_t)));
        for (size_t i = 0; i < s.length; i++)
            buffer[i] = src[i];

        AstExpr* offset = ToAstExpr(c, s.offset);
        if (!offset)
            return false;

        AstName name(buffer, s.length);
        AstDataSegment* segment = new(c.lifo) AstDataSegment(offset, name);
        if (!segment || !c.module().append(segment))
            return false;
    }

    return true;
}

static bool
AstDecodeElemSection(AstDecodeContext &c)
{
    uint32_t sectionStart, sectionSize;
    if (!c.d.startSection(SectionId::Elem, &sectionStart, &sectionSize, "elem"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numElems;
    if (!c.d.readVarU32(&numElems))
        return c.d.fail("failed to read number of table elements");

    for (uint32_t i = 0; i < numElems; i++) {
        uint32_t tableIndex;
        if (!c.d.readVarU32(&tableIndex))
            return c.d.fail("expected table index for element");

        if (tableIndex != 0)
            return c.d.fail("non-zero table index for element");

        AstExpr* offset;
        if (!AstDecodeInitializerExpression(c, ValType::I32, &offset))
            return false;

        uint32_t count;
        if (!c.d.readVarU32(&count))
            return c.d.fail("expected element count");

        AstRefVector elems(c.lifo);
        if (!elems.resize(count))
            return false;

        for (uint32_t i = 0; i < count; i++) {
            uint32_t index;
            if (!c.d.readVarU32(&index))
                return c.d.fail("expected element index");

            elems[i] = AstRef(index);
        }

        AstElemSegment* segment = new(c.lifo) AstElemSegment(offset, Move(elems));
        if (!segment || !c.module().append(segment))
            return false;
    }

    if (!c.d.finishSection(sectionStart, sectionSize, "elem"))
        return false;

    return true;
}

static bool
AstDecodeStartSection(AstDecodeContext &c)
{
    uint32_t sectionStart, sectionSize;
    if (!c.d.startSection(SectionId::Start, &sectionStart, &sectionSize, "start"))
        return false;
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t funcIndex;
    if (!c.d.readVarU32(&funcIndex))
        return c.d.fail("failed to read start func index");

    AstRef funcRef;
    if (!AstDecodeGenerateRef(c, AstName(u"func"), funcIndex, &funcRef))
        return false;

    c.module().setStartFunc(AstStartFunc(funcRef));

    if (!c.d.finishSection(sectionStart, sectionSize, "start"))
        return false;

    return true;
}

bool
wasm::BinaryToAst(JSContext* cx, const uint8_t* bytes, uint32_t length,
                  LifoAlloc& lifo, AstModule** module)
{
    AstModule* result = new(lifo) AstModule(lifo);
    if (!result->init())
        return false;

    UniqueChars error;
    Decoder d(bytes, bytes + length, &error);
    AstDecodeContext c(cx, lifo, d, *result, true);

    if (!DecodePreamble(d) ||
        !AstDecodeTypeSection(c) ||
        !AstDecodeImportSection(c) ||
        !AstDecodeFunctionSection(c) ||
        !AstDecodeTableSection(c) ||
        !AstDecodeMemorySection(c) ||
        !AstDecodeGlobalSection(c) ||
        !AstDecodeExportSection(c) ||
        !AstDecodeStartSection(c) ||
        !AstDecodeElemSection(c) ||
        !AstDecodeCodeSection(c) ||
        !AstDecodeDataSection(c) ||
        !DecodeUnknownSections(c.d))
    {
        if (error) {
            JS_ReportErrorNumberASCII(c.cx, GetErrorMessage, nullptr, JSMSG_WASM_COMPILE_ERROR,
                                      error.get());
            return false;
        }
        ReportOutOfMemory(c.cx);
        return false;
    }

    MOZ_ASSERT(!error, "unreported error in decoding");

    *module = result;
    return true;
}

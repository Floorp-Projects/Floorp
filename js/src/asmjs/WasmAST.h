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

#ifndef wasmast_h
#define wasmast_h

#include "asmjs/WasmTypes.h"

#include "ds/LifoAlloc.h"
#include "js/HashTable.h"
#include "js/Vector.h"

namespace js {
namespace wasm {

const uint32_t AstNoIndex = UINT32_MAX;
const unsigned AST_LIFO_DEFAULT_CHUNK_SIZE = 4096;

/*****************************************************************************/
// wasm AST

class AstExpr;

template <class T>
using AstVector = mozilla::Vector<T, 0, LifoAllocPolicy<Fallible>>;

template <class K, class V, class HP>
using AstHashMap = HashMap<K, V, HP, LifoAllocPolicy<Fallible>>;

class AstName
{
    const char16_t* begin_;
    const char16_t* end_;
  public:
    template <size_t Length>
    explicit AstName(const char16_t (&str)[Length]) : begin_(str), end_(str + Length - 1) {
      MOZ_ASSERT(str[Length - 1] == MOZ_UTF16('\0'));
    }

    AstName(const char16_t* begin, size_t length) : begin_(begin), end_(begin + length) {}
    AstName() : begin_(nullptr), end_(nullptr) {}
    const char16_t* begin() const { return begin_; }
    const char16_t* end() const { return end_; }
    size_t length() const { return end_ - begin_; }
    bool empty() const { return begin_ == nullptr; }

    bool operator==(AstName rhs) const {
        if (length() != rhs.length())
            return false;
        if (begin() == rhs.begin())
            return true;
        return EqualChars(begin(), rhs.begin(), length());
    }
    bool operator!=(AstName rhs) const {
        return !(*this == rhs);
    }
};

class AstRef
{
    AstName name_;
    uint32_t index_;

  public:
    AstRef()
      : index_(AstNoIndex)
    {
        MOZ_ASSERT(isInvalid());
    }
    AstRef(AstName name, uint32_t index)
      : name_(name), index_(index)
    {
        MOZ_ASSERT(name.empty() ^ (index == AstNoIndex));
        MOZ_ASSERT(!isInvalid());
    }
    bool isInvalid() const {
        return name_.empty() && index_ == AstNoIndex;
    }
    AstName name() const {
        return name_;
    }
    size_t index() const {
        MOZ_ASSERT(index_ != AstNoIndex);
        return index_;
    }
    void setIndex(uint32_t index) {
        MOZ_ASSERT(index_ == AstNoIndex);
        index_ = index;
    }
};

struct AstNameHasher
{
    typedef const AstName Lookup;
    static js::HashNumber hash(Lookup l) {
        return mozilla::HashString(l.begin(), l.length());
    }
    static bool match(const AstName key, Lookup lookup) {
        return key == lookup;
    }
};

using AstNameMap = AstHashMap<AstName, uint32_t, AstNameHasher>;

typedef AstVector<ValType> AstValTypeVector;
typedef AstVector<AstExpr*> AstExprVector;
typedef AstVector<AstName> AstNameVector;
typedef AstVector<AstRef> AstRefVector;

struct AstBase
{
    void* operator new(size_t numBytes, LifoAlloc& astLifo) throw() {
        return astLifo.alloc(numBytes);
    }
};

class AstSig : public AstBase
{
    AstName name_;
    AstValTypeVector args_;
    ExprType ret_;

  public:
    explicit AstSig(LifoAlloc& lifo)
      : args_(lifo),
        ret_(ExprType::Void)
    {}
    AstSig(AstValTypeVector&& args, ExprType ret)
      : args_(Move(args)),
        ret_(ret)
    {}
    AstSig(AstName name, AstSig&& rhs)
      : name_(name),
        args_(Move(rhs.args_)),
        ret_(rhs.ret_)
    {}
    void operator=(AstSig&& rhs) {
        args_ = Move(rhs.args_);
        ret_ = rhs.ret_;
    }
    const AstValTypeVector& args() const {
        return args_;
    }
    ExprType ret() const {
        return ret_;
    }
    AstName name() const {
        return name_;
    }
    bool operator==(const AstSig& rhs) const {
        return ret() == rhs.ret() && EqualContainers(args(), rhs.args());
    }

    typedef const AstSig& Lookup;
    static HashNumber hash(Lookup sig) {
        return AddContainerToHash(sig.args(), HashNumber(sig.ret()));
    }
    static bool match(const AstSig* lhs, Lookup rhs) {
        return *lhs == rhs;
    }
};

class AstNode : public AstBase
{};

enum class AstExprKind
{
    BinaryOperator,
    Block,
    Branch,
    BranchTable,
    Call,
    CallIndirect,
    ComparisonOperator,
    Const,
    ConversionOperator,
    GetLocal,
    If,
    Load,
    Nop,
    Return,
    SetLocal,
    Store,
    TernaryOperator,
    UnaryOperator,
    Unreachable
};

class AstExpr : public AstNode
{
    const AstExprKind kind_;

  protected:
    explicit AstExpr(AstExprKind kind)
      : kind_(kind)
    {}

  public:
    AstExprKind kind() const { return kind_; }

    template <class T>
    T& as() {
        MOZ_ASSERT(kind() == T::Kind);
        return static_cast<T&>(*this);
    }
};

struct AstNop : AstExpr
{
    static const AstExprKind Kind = AstExprKind::Nop;
    AstNop()
      : AstExpr(AstExprKind::Nop)
    {}
};

struct AstUnreachable : AstExpr
{
    static const AstExprKind Kind = AstExprKind::Unreachable;
    AstUnreachable()
      : AstExpr(AstExprKind::Unreachable)
    {}
};

class AstConst : public AstExpr
{
    const Val val_;

  public:
    static const AstExprKind Kind = AstExprKind::Const;
    explicit AstConst(Val val)
      : AstExpr(Kind),
        val_(val)
    {}
    Val val() const { return val_; }
};

class AstGetLocal : public AstExpr
{
    AstRef local_;

  public:
    static const AstExprKind Kind = AstExprKind::GetLocal;
    explicit AstGetLocal(AstRef local)
      : AstExpr(Kind),
        local_(local)
    {}
    AstRef& local() {
        return local_;
    }
};

class AstSetLocal : public AstExpr
{
    AstRef local_;
    AstExpr& value_;

  public:
    static const AstExprKind Kind = AstExprKind::SetLocal;
    AstSetLocal(AstRef local, AstExpr& value)
      : AstExpr(Kind),
        local_(local),
        value_(value)
    {}
    AstRef& local() {
        return local_;
    }
    AstExpr& value() const {
        return value_;
    }
};

class AstBlock : public AstExpr
{
    Expr expr_;
    AstName breakName_;
    AstName continueName_;
    AstExprVector exprs_;

  public:
    static const AstExprKind Kind = AstExprKind::Block;
    explicit AstBlock(Expr expr, AstName breakName, AstName continueName, AstExprVector&& exprs)
      : AstExpr(Kind),
        expr_(expr),
        breakName_(breakName),
        continueName_(continueName),
        exprs_(Move(exprs))
    {}

    Expr expr() const { return expr_; }
    AstName breakName() const { return breakName_; }
    AstName continueName() const { return continueName_; }
    const AstExprVector& exprs() const { return exprs_; }
};

class AstBranch : public AstExpr
{
    Expr expr_;
    AstExpr* cond_;
    AstRef target_;
    AstExpr* value_;

  public:
    static const AstExprKind Kind = AstExprKind::Branch;
    explicit AstBranch(Expr expr, AstExpr* cond, AstRef target, AstExpr* value)
      : AstExpr(Kind),
        expr_(expr),
        cond_(cond),
        target_(target),
        value_(value)
    {}

    Expr expr() const { return expr_; }
    AstRef& target() { return target_; }
    AstExpr& cond() const { MOZ_ASSERT(cond_); return *cond_; }
    AstExpr* maybeValue() const { return value_; }
};

class AstCall : public AstExpr
{
    Expr expr_;
    AstRef func_;
    AstExprVector args_;

  public:
    static const AstExprKind Kind = AstExprKind::Call;
    AstCall(Expr expr, AstRef func, AstExprVector&& args)
      : AstExpr(Kind), expr_(expr), func_(func), args_(Move(args))
    {}

    Expr expr() const { return expr_; }
    AstRef& func() { return func_; }
    const AstExprVector& args() const { return args_; }
};

class AstCallIndirect : public AstExpr
{
    AstRef sig_;
    AstExpr* index_;
    AstExprVector args_;

  public:
    static const AstExprKind Kind = AstExprKind::CallIndirect;
    AstCallIndirect(AstRef sig, AstExpr* index, AstExprVector&& args)
      : AstExpr(Kind), sig_(sig), index_(index), args_(Move(args))
    {}
    AstRef& sig() { return sig_; }
    AstExpr* index() const { return index_; }
    const AstExprVector& args() const { return args_; }
};

class AstReturn : public AstExpr
{
    AstExpr* maybeExpr_;

  public:
    static const AstExprKind Kind = AstExprKind::Return;
    explicit AstReturn(AstExpr* maybeExpr)
      : AstExpr(Kind),
        maybeExpr_(maybeExpr)
    {}
    AstExpr* maybeExpr() const { return maybeExpr_; }
};

class AstIf : public AstExpr
{
    AstExpr* cond_;
    AstName thenName_;
    AstExprVector thenExprs_;
    AstName elseName_;
    AstExprVector elseExprs_;

  public:
    static const AstExprKind Kind = AstExprKind::If;
    AstIf(AstExpr* cond, AstName thenName, AstExprVector&& thenExprs,
          AstName elseName, AstExprVector&& elseExprs)
      : AstExpr(Kind),
        cond_(cond),
        thenName_(thenName),
        thenExprs_(Move(thenExprs)),
        elseName_(elseName),
        elseExprs_(Move(elseExprs))
    {}

    AstExpr& cond() const { return *cond_; }
    const AstExprVector& thenExprs() const { return thenExprs_; }
    bool hasElse() const { return elseExprs_.length(); }
    const AstExprVector& elseExprs() const { MOZ_ASSERT(hasElse()); return elseExprs_; }
    AstName thenName() const { return thenName_; }
    AstName elseName() const { return elseName_; }
};

class AstLoadStoreAddress
{
    AstExpr* base_;
    int32_t flags_;
    int32_t offset_;

  public:
    explicit AstLoadStoreAddress(AstExpr* base, int32_t flags, int32_t offset)
      : base_(base),
        flags_(flags),
        offset_(offset)
    {}

    AstExpr& base() const { return *base_; }
    int32_t flags() const { return flags_; }
    int32_t offset() const { return offset_; }
};

class AstLoad : public AstExpr
{
    Expr expr_;
    AstLoadStoreAddress address_;

  public:
    static const AstExprKind Kind = AstExprKind::Load;
    explicit AstLoad(Expr expr, const AstLoadStoreAddress &address)
      : AstExpr(Kind),
        expr_(expr),
        address_(address)
    {}

    Expr expr() const { return expr_; }
    const AstLoadStoreAddress& address() const { return address_; }
};

class AstStore : public AstExpr
{
    Expr expr_;
    AstLoadStoreAddress address_;
    AstExpr* value_;

  public:
    static const AstExprKind Kind = AstExprKind::Store;
    explicit AstStore(Expr expr, const AstLoadStoreAddress &address,
                          AstExpr* value)
      : AstExpr(Kind),
        expr_(expr),
        address_(address),
        value_(value)
    {}

    Expr expr() const { return expr_; }
    const AstLoadStoreAddress& address() const { return address_; }
    AstExpr& value() const { return *value_; }
};

class AstBranchTable : public AstExpr
{
    AstExpr& index_;
    AstRef default_;
    AstRefVector table_;
    AstExpr* value_;

  public:
    static const AstExprKind Kind = AstExprKind::BranchTable;
    explicit AstBranchTable(AstExpr& index, AstRef def, AstRefVector&& table,
                                AstExpr* maybeValue)
      : AstExpr(Kind),
        index_(index),
        default_(def),
        table_(Move(table)),
        value_(maybeValue)
    {}
    AstExpr& index() const { return index_; }
    AstRef& def() { return default_; }
    AstRefVector& table() { return table_; }
    AstExpr* maybeValue() { return value_; }
};

class AstFunc : public AstNode
{
    AstName name_;
    AstRef sig_;
    AstValTypeVector vars_;
    AstNameVector localNames_;
    AstExprVector body_;

  public:
    AstFunc(AstName name, AstRef sig, AstValTypeVector&& vars,
                AstNameVector&& locals, AstExprVector&& body)
      : name_(name),
        sig_(sig),
        vars_(Move(vars)),
        localNames_(Move(locals)),
        body_(Move(body))
    {}
    AstRef& sig() { return sig_; }
    const AstValTypeVector& vars() const { return vars_; }
    const AstNameVector& locals() const { return localNames_; }
    const AstExprVector& body() const { return body_; }
    AstName name() const { return name_; }
};

class AstImport : public AstNode
{
    AstName name_;
    AstName module_;
    AstName func_;
    AstRef sig_;

  public:
    AstImport(AstName name, AstName module, AstName func, AstRef sig)
      : name_(name), module_(module), func_(func), sig_(sig)
    {}
    AstName name() const { return name_; }
    AstName module() const { return module_; }
    AstName func() const { return func_; }
    AstRef& sig() { return sig_; }
};

enum class AstExportKind { Func, Memory };

class AstExport : public AstNode
{
    AstName name_;
    AstExportKind kind_;
    AstRef func_;

  public:
    AstExport(AstName name, AstRef func)
      : name_(name), kind_(AstExportKind::Func), func_(func)
    {}
    explicit AstExport(AstName name)
      : name_(name), kind_(AstExportKind::Memory)
    {}
    AstName name() const { return name_; }
    AstExportKind kind() const { return kind_; }
    AstRef& func() { return func_; }
};

typedef AstVector<AstRef> AstTableElemVector;

class AstTable : public AstNode
{
    AstTableElemVector elems_;

  public:
    explicit AstTable(AstTableElemVector&& elems) : elems_(Move(elems)) {}
    AstTableElemVector& elems() { return elems_; }
};

class AstSegment : public AstNode
{
    uint32_t offset_;
    AstName text_;

  public:
    AstSegment(uint32_t offset, AstName text)
      : offset_(offset), text_(text)
    {}
    uint32_t offset() const { return offset_; }
    AstName text() const { return text_; }
};

typedef AstVector<AstSegment*> AstSegmentVector;

class AstMemory : public AstNode
{
    uint32_t initialSize_;
    Maybe<uint32_t> maxSize_;
    AstSegmentVector segments_;

  public:
    explicit AstMemory(uint32_t initialSize, Maybe<uint32_t> maxSize,
                           AstSegmentVector&& segments)
      : initialSize_(initialSize),
        maxSize_(maxSize),
        segments_(Move(segments))
    {}
    uint32_t initialSize() const { return initialSize_; }
    const Maybe<uint32_t>& maxSize() const { return maxSize_; }
    const AstSegmentVector& segments() const { return segments_; }
};

class AstModule : public AstNode
{
  public:
    typedef AstVector<AstFunc*> FuncVector;
    typedef AstVector<AstImport*> ImportVector;
    typedef AstVector<AstExport*> ExportVector;
    typedef AstVector<AstSig*> SigVector;

  private:
    typedef AstHashMap<AstSig*, uint32_t, AstSig> SigMap;

    LifoAlloc& lifo_;
    AstMemory* memory_;
    SigVector sigs_;
    SigMap sigMap_;
    ImportVector imports_;
    ExportVector exports_;
    AstTable* table_;
    FuncVector funcs_;

  public:
    explicit AstModule(LifoAlloc& lifo)
      : lifo_(lifo),
        memory_(nullptr),
        sigs_(lifo),
        sigMap_(lifo),
        imports_(lifo),
        exports_(lifo),
        table_(nullptr),
        funcs_(lifo)
    {}
    bool init() {
        return sigMap_.init();
    }
    bool setMemory(AstMemory* memory) {
        if (memory_)
            return false;
        memory_ = memory;
        return true;
    }
    AstMemory* maybeMemory() const {
        return memory_;
    }
    bool declare(AstSig&& sig, uint32_t* sigIndex) {
        SigMap::AddPtr p = sigMap_.lookupForAdd(sig);
        if (p) {
            *sigIndex = p->value();
            return true;
        }
        *sigIndex = sigs_.length();
        auto* lifoSig = new (lifo_) AstSig(AstName(), Move(sig));
        return lifoSig &&
               sigs_.append(lifoSig) &&
               sigMap_.add(p, sigs_.back(), *sigIndex);
    }
    bool append(AstSig* sig) {
        uint32_t sigIndex = sigs_.length();
        if (!sigs_.append(sig))
            return false;
        SigMap::AddPtr p = sigMap_.lookupForAdd(*sig);
        return p || sigMap_.add(p, sig, sigIndex);
    }
    const SigVector& sigs() const {
        return sigs_;
    }
    bool append(AstFunc* func) {
        return funcs_.append(func);
    }
    const FuncVector& funcs() const {
        return funcs_;
    }
    const ImportVector& imports() const {
        return imports_;
    }
    bool append(AstImport* imp) {
        return imports_.append(imp);
    }
    bool append(AstExport* exp) {
        return exports_.append(exp);
    }
    const ExportVector& exports() const {
        return exports_;
    }
    bool initTable(AstTable* table) {
        if (table_)
            return false;
        table_ = table;
        return true;
    }
    AstTable* maybeTable() const {
        return table_;
    }
};

class AstUnaryOperator final : public AstExpr
{
    Expr expr_;
    AstExpr* op_;

  public:
    static const AstExprKind Kind = AstExprKind::UnaryOperator;
    explicit AstUnaryOperator(Expr expr, AstExpr* op)
      : AstExpr(Kind),
        expr_(expr), op_(op)
    {}

    Expr expr() const { return expr_; }
    AstExpr* op() const { return op_; }
};

class AstBinaryOperator final : public AstExpr
{
    Expr expr_;
    AstExpr* lhs_;
    AstExpr* rhs_;

  public:
    static const AstExprKind Kind = AstExprKind::BinaryOperator;
    explicit AstBinaryOperator(Expr expr, AstExpr* lhs, AstExpr* rhs)
      : AstExpr(Kind),
        expr_(expr), lhs_(lhs), rhs_(rhs)
    {}

    Expr expr() const { return expr_; }
    AstExpr* lhs() const { return lhs_; }
    AstExpr* rhs() const { return rhs_; }
};

class AstTernaryOperator : public AstExpr
{
    Expr expr_;
    AstExpr* op0_;
    AstExpr* op1_;
    AstExpr* op2_;

  public:
    static const AstExprKind Kind = AstExprKind::TernaryOperator;
    AstTernaryOperator(Expr expr, AstExpr* op0, AstExpr* op1, AstExpr* op2)
      : AstExpr(Kind),
        expr_(expr), op0_(op0), op1_(op1), op2_(op2)
    {}

    Expr expr() const { return expr_; }
    AstExpr* op0() const { return op0_; }
    AstExpr* op1() const { return op1_; }
    AstExpr* op2() const { return op2_; }
};

class AstComparisonOperator final : public AstExpr
{
    Expr expr_;
    AstExpr* lhs_;
    AstExpr* rhs_;

  public:
    static const AstExprKind Kind = AstExprKind::ComparisonOperator;
    explicit AstComparisonOperator(Expr expr, AstExpr* lhs, AstExpr* rhs)
      : AstExpr(Kind),
        expr_(expr), lhs_(lhs), rhs_(rhs)
    {}

    Expr expr() const { return expr_; }
    AstExpr* lhs() const { return lhs_; }
    AstExpr* rhs() const { return rhs_; }
};

class AstConversionOperator final : public AstExpr
{
    Expr expr_;
    AstExpr* op_;

  public:
    static const AstExprKind Kind = AstExprKind::ConversionOperator;
    explicit AstConversionOperator(Expr expr, AstExpr* op)
      : AstExpr(Kind),
        expr_(expr), op_(op)
    {}

    Expr expr() const { return expr_; }
    AstExpr* op() const { return op_; }
};

} // end wasm namespace
} // end js namespace

#endif // namespace wasmast_h

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

#include "ds/LifoAlloc.h"
#include "js/HashTable.h"
#include "js/Vector.h"
#include "wasm/WasmTypes.h"

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
      MOZ_ASSERT(str[Length - 1] == u'\0');
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
    AstRef() : index_(AstNoIndex) {
        MOZ_ASSERT(isInvalid());
    }
    explicit AstRef(AstName name) : name_(name), index_(AstNoIndex) {
        MOZ_ASSERT(!isInvalid());
    }
    explicit AstRef(uint32_t index) : index_(index) {
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
    bool operator==(AstRef rhs) const {
        return name_ == rhs.name_ && index_ == rhs.index_;
    }
    bool operator!=(AstRef rhs) const {
        return !(*this == rhs);
    }
};

class AstValType
{
    // When this type is resolved, which_ becomes IsValType.

    enum { IsValType, IsAstRef } which_;
    ValType type_;
    AstRef  ref_;

  public:
    AstValType() : which_(IsValType) {} // type_ is then !isValid()

    explicit AstValType(ValType type)
      : which_(IsValType),
        type_(type)
    { }

    explicit AstValType(AstRef ref) {
        if (ref.name().empty()) {
            which_ = IsValType;
            type_ = ValType(ValType::Ref, ref.index());
        } else {
            which_ = IsAstRef;
            ref_ = ref;
        }
    }

    bool isRefType() const {
        return code() == ValType::AnyRef || code() == ValType::Ref;
    }

    bool isValid() const {
        return !(which_ == IsValType && !type_.isValid());
    }

    bool isResolved() const {
        return which_ == IsValType;
    }

    AstRef& asRef() {
        return ref_;
    }

    void resolve() {
        MOZ_ASSERT(which_ == IsAstRef);
        which_ = IsValType;
        type_ = ValType(ValType::Ref, ref_.index());
    }

    ValType::Code code() const {
        if (which_ == IsValType)
            return type_.code();
        return ValType::Ref;
    }

    ValType type() const {
        MOZ_ASSERT(which_ == IsValType);
        return type_;
    }

    bool operator==(const AstValType& that) const {
        if (which_ != that.which_)
            return false;
        if (which_ == IsValType)
            return type_ == that.type_;
        return ref_ == that.ref_;
    }

    bool operator!=(const AstValType& that) const {
        return !(*this == that);
    }
};

class AstExprType
{
    // When this type is resolved, which_ becomes IsExprType.

    enum { IsExprType, IsAstValType } which_;
    union {
        ExprType   type_;
        AstValType vt_;
    };

  public:
    MOZ_IMPLICIT AstExprType(ExprType::Code type)
      : which_(IsExprType),
        type_(type)
    {}

    MOZ_IMPLICIT AstExprType(ExprType type)
      : which_(IsExprType),
        type_(type)
    {}

    MOZ_IMPLICIT AstExprType(const AstExprType& type)
      : which_(type.which_)
    {
        switch (which_) {
          case IsExprType:
            type_ = type.type_;
            break;
          case IsAstValType:
            vt_ = type.vt_;
            break;
        }
    }

    explicit AstExprType(AstValType vt)
      : which_(IsAstValType),
        vt_(vt)
    {}

    bool isVoid() const {
        return which_ == IsExprType && type_ == ExprType::Void;
    }

    bool isResolved() const {
        return which_ == IsExprType;
    }

    AstValType& asAstValType() {
        MOZ_ASSERT(which_ == IsAstValType);
        return vt_;
    }

    void resolve() {
        MOZ_ASSERT(which_ == IsAstValType);
        which_ = IsExprType;
        type_ = ExprType(vt_.type());
    }

    ExprType::Code code() const {
        if (which_ == IsExprType)
            return type_.code();
        return ExprType::Ref;
    }

    ExprType type() const {
        if (which_ == IsExprType)
            return type_;
        return ExprType(vt_.type());
    }

    bool operator==(const AstExprType& that) const {
        if (which_ != that.which_)
            return false;
        if (which_ == IsExprType)
            return type_ == that.type_;
        return vt_ == that.vt_;
    }

    bool operator!=(const AstExprType& that) const {
        return !(*this == that);
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

typedef AstVector<AstValType> AstValTypeVector;
typedef AstVector<AstExpr*> AstExprVector;
typedef AstVector<AstName> AstNameVector;
typedef AstVector<AstRef> AstRefVector;

struct AstBase
{
    void* operator new(size_t numBytes, LifoAlloc& astLifo) throw() {
        return astLifo.alloc(numBytes);
    }
};

struct AstNode
{
    void* operator new(size_t numBytes, LifoAlloc& astLifo) throw() {
        return astLifo.alloc(numBytes);
    }
};

class AstFuncType;
class AstStructType;

class AstTypeDef : public AstNode
{
  protected:
    enum class Which { IsFuncType, IsStructType };

  private:
    Which which_;

  public:
    explicit AstTypeDef(Which which) : which_(which) {}

    bool isFuncType() const { return which_ == Which::IsFuncType; }
    bool isStructType() const { return which_ == Which::IsStructType; }
    inline AstFuncType& asFuncType();
    inline AstStructType& asStructType();
    inline const AstFuncType& asFuncType() const;
    inline const AstStructType& asStructType() const;
};

class AstFuncType : public AstTypeDef
{
    AstName name_;
    AstValTypeVector args_;
    AstExprType ret_;

  public:
    explicit AstFuncType(LifoAlloc& lifo)
      : AstTypeDef(Which::IsFuncType),
        args_(lifo),
        ret_(ExprType::Void)
    {}
    AstFuncType(AstValTypeVector&& args, AstExprType ret)
      : AstTypeDef(Which::IsFuncType),
        args_(std::move(args)),
        ret_(ret)
    {}
    AstFuncType(AstName name, AstFuncType&& rhs)
      : AstTypeDef(Which::IsFuncType),
        name_(name),
        args_(std::move(rhs.args_)),
        ret_(rhs.ret_)
    {}
    const AstValTypeVector& args() const {
        return args_;
    }
    AstValTypeVector& args() {
        return args_;
    }
    AstExprType ret() const {
        return ret_;
    }
    AstExprType& ret() {
        return ret_;
    }
    AstName name() const {
        return name_;
    }
    bool operator==(const AstFuncType& rhs) const {
        if (ret() != rhs.ret())
            return false;
        size_t len = args().length();
        if (rhs.args().length() != len)
            return false;
        for (size_t i=0; i < len; i++) {
            if (args()[i] != rhs.args()[i])
                return false;
        }
        return true;
    }

    typedef const AstFuncType& Lookup;
    static HashNumber hash(Lookup ft) {
        HashNumber hn = HashNumber(ft.ret().code());
        for (const AstValType& vt : ft.args())
            hn = mozilla::AddToHash(hn, uint32_t(vt.code()));
        return hn;
    }
    static bool match(const AstFuncType* lhs, Lookup rhs) {
        return *lhs == rhs;
    }
};

class AstStructType : public AstTypeDef
{
    AstName          name_;
    AstNameVector    fieldNames_;
    AstValTypeVector fieldTypes_;

  public:
    explicit AstStructType(LifoAlloc& lifo)
      : AstTypeDef(Which::IsStructType),
        fieldNames_(lifo),
        fieldTypes_(lifo)
    {}
    AstStructType(AstNameVector&& names, AstValTypeVector&& types)
      : AstTypeDef(Which::IsStructType),
        fieldNames_(std::move(names)),
        fieldTypes_(std::move(types))
    {}
    AstStructType(AstName name, AstStructType&& rhs)
      : AstTypeDef(Which::IsStructType),
        name_(name),
        fieldNames_(std::move(rhs.fieldNames_)),
        fieldTypes_(std::move(rhs.fieldTypes_))
    {}
    AstName name() const {
        return name_;
    }
    const AstNameVector& fieldNames() const {
        return fieldNames_;
    }
    const AstValTypeVector& fieldTypes() const {
        return fieldTypes_;
    }
    AstValTypeVector& fieldTypes() {
        return fieldTypes_;
    }
};

inline AstFuncType&
AstTypeDef::asFuncType()
{
    MOZ_ASSERT(isFuncType());
    return *static_cast<AstFuncType*>(this);
}

inline AstStructType&
AstTypeDef::asStructType()
{
    MOZ_ASSERT(isStructType());
    return *static_cast<AstStructType*>(this);
}

inline const AstFuncType&
AstTypeDef::asFuncType() const
{
    MOZ_ASSERT(isFuncType());
    return *static_cast<const AstFuncType*>(this);
}

inline const AstStructType&
AstTypeDef::asStructType() const
{
    MOZ_ASSERT(isStructType());
    return *static_cast<const AstStructType*>(this);
}

enum class AstExprKind
{
    AtomicCmpXchg,
    AtomicLoad,
    AtomicRMW,
    AtomicStore,
    BinaryOperator,
    Block,
    Branch,
    BranchTable,
    Call,
    CallIndirect,
    ComparisonOperator,
    Const,
    ConversionOperator,
    CurrentMemory,
    Drop,
#ifdef ENABLE_WASM_SATURATING_TRUNC_OPS
    ExtraConversionOperator,
#endif
    First,
    GetGlobal,
    GetLocal,
    GrowMemory,
    If,
    Load,
#ifdef ENABLE_WASM_BULKMEM_OPS
    MemCopy,
    MemFill,
#endif
    Nop,
    Pop,
    RefNull,
    Return,
    SetGlobal,
    SetLocal,
    TeeLocal,
    Store,
    TernaryOperator,
    UnaryOperator,
    Unreachable,
    Wait,
    Wake
};

class AstExpr : public AstNode
{
    const AstExprKind kind_;
    AstExprType type_;

  protected:
    AstExpr(AstExprKind kind, AstExprType type)
      : kind_(kind), type_(type)
    {}

  public:
    AstExprKind kind() const { return kind_; }

    bool isVoid() const { return type_.isVoid(); }

    // Note that for nodes other than blocks and block-like things, the
    // underlying type may be ExprType::Limit for nodes with non-void types.
    AstExprType& type() { return type_; }

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
      : AstExpr(AstExprKind::Nop, ExprType::Void)
   {}
};

struct AstUnreachable : AstExpr
{
    static const AstExprKind Kind = AstExprKind::Unreachable;
    AstUnreachable()
      : AstExpr(AstExprKind::Unreachable, ExprType::Void)
    {}
};

class AstDrop : public AstExpr
{
    AstExpr& value_;

  public:
    static const AstExprKind Kind = AstExprKind::Drop;
    explicit AstDrop(AstExpr& value)
      : AstExpr(AstExprKind::Drop, ExprType::Void),
        value_(value)
    {}
    AstExpr& value() const {
        return value_;
    }
};

class AstConst : public AstExpr
{
    const Val val_;

  public:
    static const AstExprKind Kind = AstExprKind::Const;
    explicit AstConst(Val val)
      : AstExpr(Kind, ExprType::Limit),
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
      : AstExpr(Kind, ExprType::Limit),
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
      : AstExpr(Kind, ExprType::Void),
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

class AstGetGlobal : public AstExpr
{
    AstRef global_;

  public:
    static const AstExprKind Kind = AstExprKind::GetGlobal;
    explicit AstGetGlobal(AstRef global)
      : AstExpr(Kind, ExprType::Limit),
        global_(global)
    {}
    AstRef& global() {
        return global_;
    }
};

class AstSetGlobal : public AstExpr
{
    AstRef global_;
    AstExpr& value_;

  public:
    static const AstExprKind Kind = AstExprKind::SetGlobal;
    AstSetGlobal(AstRef global, AstExpr& value)
      : AstExpr(Kind, ExprType::Void),
        global_(global),
        value_(value)
    {}
    AstRef& global() {
        return global_;
    }
    AstExpr& value() const {
        return value_;
    }
};

class AstTeeLocal : public AstExpr
{
    AstRef local_;
    AstExpr& value_;

  public:
    static const AstExprKind Kind = AstExprKind::TeeLocal;
    AstTeeLocal(AstRef local, AstExpr& value)
      : AstExpr(Kind, ExprType::Limit),
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
    Op op_;
    AstName name_;
    AstExprVector exprs_;

  public:
    static const AstExprKind Kind = AstExprKind::Block;
    explicit AstBlock(Op op, AstExprType type, AstName name, AstExprVector&& exprs)
      : AstExpr(Kind, type),
        op_(op),
        name_(name),
        exprs_(std::move(exprs))
    {}

    Op op() const { return op_; }
    AstName name() const { return name_; }
    const AstExprVector& exprs() const { return exprs_; }
};

class AstBranch : public AstExpr
{
    Op op_;
    AstExpr* cond_;
    AstRef target_;
    AstExpr* value_;

  public:
    static const AstExprKind Kind = AstExprKind::Branch;
    explicit AstBranch(Op op, AstExprType type,
                       AstExpr* cond, AstRef target, AstExpr* value)
      : AstExpr(Kind, type),
        op_(op),
        cond_(cond),
        target_(target),
        value_(value)
    {}

    Op op() const { return op_; }
    AstRef& target() { return target_; }
    AstExpr& cond() const { MOZ_ASSERT(cond_); return *cond_; }
    AstExpr* maybeValue() const { return value_; }
};

class AstCall : public AstExpr
{
    Op op_;
    AstRef func_;
    AstExprVector args_;

  public:
    static const AstExprKind Kind = AstExprKind::Call;
    AstCall(Op op, AstExprType type, AstRef func, AstExprVector&& args)
      : AstExpr(Kind, type), op_(op), func_(func), args_(std::move(args))
    {}

    Op op() const { return op_; }
    AstRef& func() { return func_; }
    const AstExprVector& args() const { return args_; }
};

class AstCallIndirect : public AstExpr
{
    AstRef funcType_;
    AstExprVector args_;
    AstExpr* index_;

  public:
    static const AstExprKind Kind = AstExprKind::CallIndirect;
    AstCallIndirect(AstRef funcType, AstExprType type, AstExprVector&& args, AstExpr* index)
      : AstExpr(Kind, type), funcType_(funcType), args_(std::move(args)), index_(index)
    {}
    AstRef& funcType() { return funcType_; }
    const AstExprVector& args() const { return args_; }
    AstExpr* index() const { return index_; }
};

class AstReturn : public AstExpr
{
    AstExpr* maybeExpr_;

  public:
    static const AstExprKind Kind = AstExprKind::Return;
    explicit AstReturn(AstExpr* maybeExpr)
      : AstExpr(Kind, ExprType::Void),
        maybeExpr_(maybeExpr)
    {}
    AstExpr* maybeExpr() const { return maybeExpr_; }
};

class AstIf : public AstExpr
{
    AstExpr* cond_;
    AstName name_;
    AstExprVector thenExprs_;
    AstExprVector elseExprs_;

  public:
    static const AstExprKind Kind = AstExprKind::If;
    AstIf(AstExprType type, AstExpr* cond, AstName name,
          AstExprVector&& thenExprs, AstExprVector&& elseExprs)
      : AstExpr(Kind, type),
        cond_(cond),
        name_(name),
        thenExprs_(std::move(thenExprs)),
        elseExprs_(std::move(elseExprs))
    {}

    AstExpr& cond() const { return *cond_; }
    const AstExprVector& thenExprs() const { return thenExprs_; }
    bool hasElse() const { return elseExprs_.length(); }
    const AstExprVector& elseExprs() const { MOZ_ASSERT(hasElse()); return elseExprs_; }
    AstName name() const { return name_; }
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
    Op op_;
    AstLoadStoreAddress address_;

  public:
    static const AstExprKind Kind = AstExprKind::Load;
    explicit AstLoad(Op op, const AstLoadStoreAddress &address)
      : AstExpr(Kind, ExprType::Limit),
        op_(op),
        address_(address)
    {}

    Op op() const { return op_; }
    const AstLoadStoreAddress& address() const { return address_; }
};

class AstStore : public AstExpr
{
    Op op_;
    AstLoadStoreAddress address_;
    AstExpr* value_;

  public:
    static const AstExprKind Kind = AstExprKind::Store;
    explicit AstStore(Op op, const AstLoadStoreAddress &address, AstExpr* value)
      : AstExpr(Kind, ExprType::Void),
        op_(op),
        address_(address),
        value_(value)
    {}

    Op op() const { return op_; }
    const AstLoadStoreAddress& address() const { return address_; }
    AstExpr& value() const { return *value_; }
};

class AstAtomicCmpXchg : public AstExpr
{
    ThreadOp op_;
    AstLoadStoreAddress address_;
    AstExpr* expected_;
    AstExpr* replacement_;

  public:
    static const AstExprKind Kind = AstExprKind::AtomicCmpXchg;
    explicit AstAtomicCmpXchg(ThreadOp op, const AstLoadStoreAddress &address, AstExpr* expected,
                              AstExpr* replacement)
      : AstExpr(Kind, ExprType::Limit),
        op_(op),
        address_(address),
        expected_(expected),
        replacement_(replacement)
    {}

    ThreadOp op() const { return op_; }
    const AstLoadStoreAddress& address() const { return address_; }
    AstExpr& expected() const { return *expected_; }
    AstExpr& replacement() const { return *replacement_; }
};

class AstAtomicLoad : public AstExpr
{
    ThreadOp op_;
    AstLoadStoreAddress address_;

  public:
    static const AstExprKind Kind = AstExprKind::AtomicLoad;
    explicit AstAtomicLoad(ThreadOp op, const AstLoadStoreAddress &address)
      : AstExpr(Kind, ExprType::Limit),
        op_(op),
        address_(address)
    {}

    ThreadOp op() const { return op_; }
    const AstLoadStoreAddress& address() const { return address_; }
};

class AstAtomicRMW : public AstExpr
{
    ThreadOp op_;
    AstLoadStoreAddress address_;
    AstExpr* value_;

  public:
    static const AstExprKind Kind = AstExprKind::AtomicRMW;
    explicit AstAtomicRMW(ThreadOp op, const AstLoadStoreAddress &address, AstExpr* value)
      : AstExpr(Kind, ExprType::Limit),
        op_(op),
        address_(address),
        value_(value)
    {}

    ThreadOp op() const { return op_; }
    const AstLoadStoreAddress& address() const { return address_; }
    AstExpr& value() const { return *value_; }
};

class AstAtomicStore : public AstExpr
{
    ThreadOp op_;
    AstLoadStoreAddress address_;
    AstExpr* value_;

  public:
    static const AstExprKind Kind = AstExprKind::AtomicStore;
    explicit AstAtomicStore(ThreadOp op, const AstLoadStoreAddress &address, AstExpr* value)
      : AstExpr(Kind, ExprType::Void),
        op_(op),
        address_(address),
        value_(value)
    {}

    ThreadOp op() const { return op_; }
    const AstLoadStoreAddress& address() const { return address_; }
    AstExpr& value() const { return *value_; }
};

class AstWait : public AstExpr
{
    ThreadOp op_;
    AstLoadStoreAddress address_;
    AstExpr* expected_;
    AstExpr* timeout_;

  public:
    static const AstExprKind Kind = AstExprKind::Wait;
    explicit AstWait(ThreadOp op, const AstLoadStoreAddress &address, AstExpr* expected,
                     AstExpr* timeout)
      : AstExpr(Kind, ExprType::I32),
        op_(op),
        address_(address),
        expected_(expected),
        timeout_(timeout)
    {}

    ThreadOp op() const { return op_; }
    const AstLoadStoreAddress& address() const { return address_; }
    AstExpr& expected() const { return *expected_; }
    AstExpr& timeout() const { return *timeout_; }
};

class AstWake : public AstExpr
{
    AstLoadStoreAddress address_;
    AstExpr* count_;

  public:
    static const AstExprKind Kind = AstExprKind::Wake;
    explicit AstWake(const AstLoadStoreAddress &address, AstExpr* count)
      : AstExpr(Kind, ExprType::I32),
        address_(address),
        count_(count)
    {}

    const AstLoadStoreAddress& address() const { return address_; }
    AstExpr& count() const { return *count_; }
};

#ifdef ENABLE_WASM_BULKMEM_OPS
class AstMemCopy : public AstExpr
{
    AstExpr* dest_;
    AstExpr* src_;
    AstExpr* len_;

  public:
    static const AstExprKind Kind = AstExprKind::MemCopy;
    explicit AstMemCopy(AstExpr* dest, AstExpr* src, AstExpr* len)
      : AstExpr(Kind, ExprType::I32),
        dest_(dest),
        src_(src),
        len_(len)
    {}

    AstExpr& dest() const { return *dest_; }
    AstExpr& src()  const { return *src_; }
    AstExpr& len()  const { return *len_; }
};

class AstMemFill : public AstExpr
{
    AstExpr* start_;
    AstExpr* val_;
    AstExpr* len_;

  public:
    static const AstExprKind Kind = AstExprKind::MemFill;
    explicit AstMemFill(AstExpr* start, AstExpr* val, AstExpr* len)
      : AstExpr(Kind, ExprType::I32),
        start_(start),
        val_(val),
        len_(len)
    {}

    AstExpr& start() const { return *start_; }
    AstExpr& val()   const { return *val_; }
    AstExpr& len()   const { return *len_; }
};
#endif

class AstCurrentMemory final : public AstExpr
{
  public:
    static const AstExprKind Kind = AstExprKind::CurrentMemory;
    explicit AstCurrentMemory()
      : AstExpr(Kind, ExprType::I32)
    {}
};

class AstGrowMemory final : public AstExpr
{
    AstExpr* operand_;

  public:
    static const AstExprKind Kind = AstExprKind::GrowMemory;
    explicit AstGrowMemory(AstExpr* operand)
      : AstExpr(Kind, ExprType::I32), operand_(operand)
    {}

    AstExpr* operand() const { return operand_; }
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
      : AstExpr(Kind, ExprType::Void),
        index_(index),
        default_(def),
        table_(std::move(table)),
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
    AstRef funcType_;
    AstValTypeVector vars_;
    AstNameVector localNames_;
    AstExprVector body_;

  public:
    AstFunc(AstName name, AstRef ft, AstValTypeVector&& vars,
            AstNameVector&& locals, AstExprVector&& body)
      : name_(name),
        funcType_(ft),
        vars_(std::move(vars)),
        localNames_(std::move(locals)),
        body_(std::move(body))
    {}
    AstRef& funcType() { return funcType_; }
    const AstValTypeVector& vars() const { return vars_; }
    AstValTypeVector& vars() { return vars_; }
    const AstNameVector& locals() const { return localNames_; }
    const AstExprVector& body() const { return body_; }
    AstName name() const { return name_; }
};

class AstGlobal : public AstNode
{
    AstName name_;
    bool isMutable_;
    AstValType type_;
    Maybe<AstExpr*> init_;

  public:
    AstGlobal() : isMutable_(false), type_(ValType())
    {}

    explicit AstGlobal(AstName name, AstValType type, bool isMutable,
                       const Maybe<AstExpr*>& init = Maybe<AstExpr*>())
      : name_(name), isMutable_(isMutable), type_(type), init_(init)
    {}

    AstName name() const { return name_; }
    bool isMutable() const { return isMutable_; }
    ValType type() const { return type_.type(); }
    AstValType& type() { return type_; }

    bool hasInit() const { return !!init_; }
    AstExpr& init() const { MOZ_ASSERT(hasInit()); return **init_; }
};

typedef AstVector<AstGlobal*> AstGlobalVector;

class AstImport : public AstNode
{
    AstName name_;
    AstName module_;
    AstName field_;
    DefinitionKind kind_;

    AstRef funcType_;
    Limits limits_;
    AstGlobal global_;

  public:
    AstImport(AstName name, AstName module, AstName field, AstRef funcType)
      : name_(name), module_(module), field_(field), kind_(DefinitionKind::Function), funcType_(funcType)
    {}
    AstImport(AstName name, AstName module, AstName field, DefinitionKind kind,
              const Limits& limits)
      : name_(name), module_(module), field_(field), kind_(kind), limits_(limits)
    {}
    AstImport(AstName name, AstName module, AstName field, const AstGlobal& global)
      : name_(name), module_(module), field_(field), kind_(DefinitionKind::Global), global_(global)
    {}

    AstName name() const { return name_; }
    AstName module() const { return module_; }
    AstName field() const { return field_; }

    DefinitionKind kind() const { return kind_; }
    AstRef& funcType() {
        MOZ_ASSERT(kind_ == DefinitionKind::Function);
        return funcType_;
    }
    Limits limits() const {
        MOZ_ASSERT(kind_ == DefinitionKind::Memory || kind_ == DefinitionKind::Table);
        return limits_;
    }
    const AstGlobal& global() const {
        MOZ_ASSERT(kind_ == DefinitionKind::Global);
        return global_;
    }
    AstGlobal& global() {
        MOZ_ASSERT(kind_ == DefinitionKind::Global);
        return global_;
    }
};

class AstExport : public AstNode
{
    AstName name_;
    DefinitionKind kind_;
    AstRef ref_;

  public:
    AstExport(AstName name, DefinitionKind kind, AstRef ref)
      : name_(name), kind_(kind), ref_(ref)
    {}
    explicit AstExport(AstName name, DefinitionKind kind)
      : name_(name), kind_(kind)
    {}
    AstName name() const { return name_; }
    DefinitionKind kind() const { return kind_; }
    AstRef& ref() { return ref_; }
};

class AstDataSegment : public AstNode
{
    AstExpr* offset_;
    AstNameVector fragments_;

  public:
    AstDataSegment(AstExpr* offset, AstNameVector&& fragments)
      : offset_(offset), fragments_(std::move(fragments))
    {}

    AstExpr* offset() const { return offset_; }
    const AstNameVector& fragments() const { return fragments_; }
};

typedef AstVector<AstDataSegment*> AstDataSegmentVector;

class AstElemSegment : public AstNode
{
    AstExpr* offset_;
    AstRefVector elems_;

  public:
    AstElemSegment(AstExpr* offset, AstRefVector&& elems)
      : offset_(offset), elems_(std::move(elems))
    {}

    AstExpr* offset() const { return offset_; }
    AstRefVector& elems() { return elems_; }
    const AstRefVector& elems() const { return elems_; }
};

typedef AstVector<AstElemSegment*> AstElemSegmentVector;

class AstStartFunc : public AstNode
{
    AstRef func_;

  public:
    explicit AstStartFunc(AstRef func)
      : func_(func)
    {}

    AstRef& func() {
        return func_;
    }
};

struct AstResizable
{
    AstName name;
    Limits limits;
    bool imported;

    AstResizable(const Limits& limits, bool imported, AstName name = AstName())
      : name(name),
        limits(limits),
        imported(imported)
    {}
};

class AstModule : public AstNode
{
  public:
    typedef AstVector<AstFunc*> FuncVector;
    typedef AstVector<AstImport*> ImportVector;
    typedef AstVector<AstExport*> ExportVector;
    typedef AstVector<AstTypeDef*> TypeDefVector;
    typedef AstVector<AstName> NameVector;
    typedef AstVector<AstResizable> AstResizableVector;

  private:
    typedef AstHashMap<AstFuncType*, uint32_t, AstFuncType> FuncTypeMap;

    LifoAlloc&           lifo_;
    TypeDefVector        types_;
    FuncTypeMap          funcTypeMap_;
    ImportVector         imports_;
    NameVector           funcImportNames_;
    AstResizableVector   tables_;
    AstResizableVector   memories_;
    ExportVector         exports_;
    Maybe<AstStartFunc>  startFunc_;
    FuncVector           funcs_;
    AstDataSegmentVector dataSegments_;
    AstElemSegmentVector elemSegments_;
    AstGlobalVector      globals_;

    size_t numGlobalImports_;

  public:
    explicit AstModule(LifoAlloc& lifo)
      : lifo_(lifo),
        types_(lifo),
        funcTypeMap_(lifo),
        imports_(lifo),
        funcImportNames_(lifo),
        tables_(lifo),
        memories_(lifo),
        exports_(lifo),
        funcs_(lifo),
        dataSegments_(lifo),
        elemSegments_(lifo),
        globals_(lifo),
        numGlobalImports_(0)
    {}
    bool init() {
        return funcTypeMap_.init();
    }
    bool addMemory(AstName name, const Limits& memory) {
        return memories_.append(AstResizable(memory, false, name));
    }
    bool hasMemory() const {
        return !!memories_.length();
    }
    const AstResizableVector& memories() const {
        return memories_;
    }
    bool addTable(AstName name, const Limits& table) {
        return tables_.append(AstResizable(table, false, name));
    }
    bool hasTable() const {
        return !!tables_.length();
    }
    const AstResizableVector& tables() const {
        return tables_;
    }
    bool append(AstDataSegment* seg) {
        return dataSegments_.append(seg);
    }
    const AstDataSegmentVector& dataSegments() const {
        return dataSegments_;
    }
    bool append(AstElemSegment* seg) {
        return elemSegments_.append(seg);
    }
    const AstElemSegmentVector& elemSegments() const {
        return elemSegments_;
    }
    bool hasStartFunc() const {
        return !!startFunc_;
    }
    bool setStartFunc(AstStartFunc startFunc) {
        if (startFunc_)
            return false;
        startFunc_.emplace(startFunc);
        return true;
    }
    AstStartFunc& startFunc() {
        return *startFunc_;
    }
    bool declare(AstFuncType&& funcType, uint32_t* funcTypeIndex) {
        FuncTypeMap::AddPtr p = funcTypeMap_.lookupForAdd(funcType);
        if (p) {
            *funcTypeIndex = p->value();
            return true;
        }
        *funcTypeIndex = types_.length();
        auto* lifoFuncType = new (lifo_) AstFuncType(AstName(), std::move(funcType));
        return lifoFuncType &&
               types_.append(lifoFuncType) &&
               funcTypeMap_.add(p, static_cast<AstFuncType*>(types_.back()), *funcTypeIndex);
    }
    bool append(AstFuncType* funcType) {
        uint32_t funcTypeIndex = types_.length();
        if (!types_.append(funcType))
            return false;
        FuncTypeMap::AddPtr p = funcTypeMap_.lookupForAdd(*funcType);
        return p || funcTypeMap_.add(p, funcType, funcTypeIndex);
    }
    const TypeDefVector& types() const {
        return types_;
    }
    bool append(AstFunc* func) {
        return funcs_.append(func);
    }
    const FuncVector& funcs() const {
        return funcs_;
    }
    bool append(AstStructType* str) {
        return types_.append(str);
    }
    bool append(AstImport* imp) {
        switch (imp->kind()) {
          case DefinitionKind::Function:
            if (!funcImportNames_.append(imp->name()))
                return false;
            break;
          case DefinitionKind::Table:
            if (!tables_.append(AstResizable(imp->limits(), true)))
                return false;
            break;
          case DefinitionKind::Memory:
            if (!memories_.append(AstResizable(imp->limits(), true)))
                return false;
            break;
          case DefinitionKind::Global:
            numGlobalImports_++;
            break;
        }
        return imports_.append(imp);
    }
    const ImportVector& imports() const {
        return imports_;
    }
    const NameVector& funcImportNames() const {
        return funcImportNames_;
    }
    size_t numFuncImports() const {
        return funcImportNames_.length();
    }
    bool append(AstExport* exp) {
        return exports_.append(exp);
    }
    const ExportVector& exports() const {
        return exports_;
    }
    bool append(AstGlobal* glob) {
        return globals_.append(glob);
    }
    const AstGlobalVector& globals() const {
        return globals_;
    }
    size_t numGlobalImports() const {
        return numGlobalImports_;
    }
};

class AstUnaryOperator final : public AstExpr
{
    Op op_;
    AstExpr* operand_;

  public:
    static const AstExprKind Kind = AstExprKind::UnaryOperator;
    explicit AstUnaryOperator(Op op, AstExpr* operand)
      : AstExpr(Kind, ExprType::Limit),
        op_(op), operand_(operand)
    {}

    Op op() const { return op_; }
    AstExpr* operand() const { return operand_; }
};

class AstBinaryOperator final : public AstExpr
{
    Op op_;
    AstExpr* lhs_;
    AstExpr* rhs_;

  public:
    static const AstExprKind Kind = AstExprKind::BinaryOperator;
    explicit AstBinaryOperator(Op op, AstExpr* lhs, AstExpr* rhs)
      : AstExpr(Kind, ExprType::Limit),
        op_(op), lhs_(lhs), rhs_(rhs)
    {}

    Op op() const { return op_; }
    AstExpr* lhs() const { return lhs_; }
    AstExpr* rhs() const { return rhs_; }
};

class AstTernaryOperator : public AstExpr
{
    Op op_;
    AstExpr* op0_;
    AstExpr* op1_;
    AstExpr* op2_;

  public:
    static const AstExprKind Kind = AstExprKind::TernaryOperator;
    AstTernaryOperator(Op op, AstExpr* op0, AstExpr* op1, AstExpr* op2)
      : AstExpr(Kind, ExprType::Limit),
        op_(op), op0_(op0), op1_(op1), op2_(op2)
    {}

    Op op() const { return op_; }
    AstExpr* op0() const { return op0_; }
    AstExpr* op1() const { return op1_; }
    AstExpr* op2() const { return op2_; }
};

class AstComparisonOperator final : public AstExpr
{
    Op op_;
    AstExpr* lhs_;
    AstExpr* rhs_;

  public:
    static const AstExprKind Kind = AstExprKind::ComparisonOperator;
    explicit AstComparisonOperator(Op op, AstExpr* lhs, AstExpr* rhs)
      : AstExpr(Kind, ExprType::Limit),
        op_(op), lhs_(lhs), rhs_(rhs)
    {}

    Op op() const { return op_; }
    AstExpr* lhs() const { return lhs_; }
    AstExpr* rhs() const { return rhs_; }
};

class AstConversionOperator final : public AstExpr
{
    Op op_;
    AstExpr* operand_;

  public:
    static const AstExprKind Kind = AstExprKind::ConversionOperator;
    explicit AstConversionOperator(Op op, AstExpr* operand)
      : AstExpr(Kind, ExprType::Limit),
        op_(op), operand_(operand)
    {}

    Op op() const { return op_; }
    AstExpr* operand() const { return operand_; }
};

#ifdef ENABLE_WASM_SATURATING_TRUNC_OPS
// Like AstConversionOperator, but for opcodes encoded with the Misc prefix.
class AstExtraConversionOperator final : public AstExpr
{
    MiscOp op_;
    AstExpr* operand_;

  public:
    static const AstExprKind Kind = AstExprKind::ExtraConversionOperator;
    explicit AstExtraConversionOperator(MiscOp op, AstExpr* operand)
      : AstExpr(Kind, ExprType::Limit),
        op_(op), operand_(operand)
    {}

    MiscOp op() const { return op_; }
    AstExpr* operand() const { return operand_; }
};
#endif

class AstRefNull final : public AstExpr
{
    AstValType refType_;
  public:
    static const AstExprKind Kind = AstExprKind::RefNull;
    explicit AstRefNull(AstValType refType)
      : AstExpr(Kind, ExprType::Limit), refType_(refType)
    {}
    AstValType& baseType() {
        return refType_;
    }
};

// This is an artificial AST node which can fill operand slots in an AST
// constructed from parsing or decoding stack-machine code that doesn't have
// an inherent AST structure.
class AstPop final : public AstExpr
{
  public:
    static const AstExprKind Kind = AstExprKind::Pop;
    AstPop()
      : AstExpr(Kind, ExprType::Void)
    {}
};

// This is an artificial AST node which can be used to represent some forms
// of stack-machine code in an AST form. It is similar to Block, but returns the
// value of its first operand, rather than the last.
class AstFirst : public AstExpr
{
    AstExprVector exprs_;

  public:
    static const AstExprKind Kind = AstExprKind::First;
    explicit AstFirst(AstExprVector&& exprs)
      : AstExpr(Kind, ExprType::Limit),
        exprs_(std::move(exprs))
    {}

    AstExprVector& exprs() { return exprs_; }
    const AstExprVector& exprs() const { return exprs_; }
};

} // end wasm namespace
} // end js namespace

#endif // namespace wasmast_h

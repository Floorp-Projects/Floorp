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

#include "asmjs/WasmText.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Maybe.h"

#include "jsdtoa.h"
#include "jsnum.h"
#include "jsprf.h"
#include "jsstr.h"

#include "asmjs/WasmBinary.h"
#include "ds/LifoAlloc.h"
#include "js/CharacterEncoding.h"
#include "js/HashTable.h"

using namespace js;
using namespace js::wasm;

using mozilla::BitwiseCast;
using mozilla::CountLeadingZeroes32;
using mozilla::CheckedInt;
using mozilla::FloatingPoint;
using mozilla::Maybe;
using mozilla::PositiveInfinity;
using mozilla::SpecificNaN;

static const unsigned AST_LIFO_DEFAULT_CHUNK_SIZE = 4096;
static const uint32_t WasmNoIndex = UINT32_MAX;

/*****************************************************************************/
// wasm AST

namespace {

class WasmAstExpr;

template <class T>
using WasmAstVector = mozilla::Vector<T, 0, LifoAllocPolicy<Fallible>>;

template <class K, class V, class HP>
using WasmAstHashMap = HashMap<K, V, HP, LifoAllocPolicy<Fallible>>;

class WasmName
{
    const char16_t* begin_;
    const char16_t* end_;
  public:
    WasmName(const char16_t* begin, size_t length) : begin_(begin), end_(begin + length) {}
    WasmName() : begin_(nullptr), end_(nullptr) {}
    const char16_t* begin() const { return begin_; }
    const char16_t* end() const { return end_; }
    size_t length() const { return end_ - begin_; }
    bool empty() const { return begin_ == nullptr; }

    bool operator==(WasmName rhs) const {
        if (length() != rhs.length())
            return false;
        if (begin() == rhs.begin())
            return true;
        return EqualChars(begin(), rhs.begin(), length());
    }
};

class WasmRef
{
    WasmName name_;
    uint32_t index_;

  public:
    WasmRef()
      : index_(WasmNoIndex)
    {
        MOZ_ASSERT(isInvalid());
    }
    WasmRef(WasmName name, uint32_t index)
      : name_(name), index_(index)
    {
        MOZ_ASSERT(name.empty() ^ (index == WasmNoIndex));
        MOZ_ASSERT(!isInvalid());
    }
    bool isInvalid() const {
        return name_.empty() && index_ == WasmNoIndex;
    }
    WasmName name() const {
        return name_;
    }
    size_t index() const {
        MOZ_ASSERT(index_ != WasmNoIndex);
        return index_;
    }
    void setIndex(uint32_t index) {
        MOZ_ASSERT(index_ == WasmNoIndex);
        index_ = index;
    }
};

struct WasmNameHasher
{
    typedef const WasmName Lookup;
    static js::HashNumber hash(Lookup l) {
        return mozilla::HashString(l.begin(), l.length());
    }
    static bool match(const WasmName key, Lookup lookup) {
        return key == lookup;
    }
};

using WasmNameMap = WasmAstHashMap<WasmName, uint32_t, WasmNameHasher>;

typedef WasmAstVector<ValType> WasmAstValTypeVector;
typedef WasmAstVector<WasmAstExpr*> WasmAstExprVector;
typedef WasmAstVector<WasmName> WasmNameVector;
typedef WasmAstVector<WasmRef> WasmRefVector;

struct WasmAstBase
{
    void* operator new(size_t numBytes, LifoAlloc& astLifo) throw() {
        return astLifo.alloc(numBytes);
    }
};

class WasmAstSig : public WasmAstBase
{
    WasmName name_;
    WasmAstValTypeVector args_;
    ExprType ret_;

  public:
    explicit WasmAstSig(LifoAlloc& lifo)
      : args_(lifo),
        ret_(ExprType::Void)
    {}
    WasmAstSig(WasmAstValTypeVector&& args, ExprType ret)
      : args_(Move(args)),
        ret_(ret)
    {}
    WasmAstSig(WasmName name, WasmAstSig&& rhs)
      : name_(name),
        args_(Move(rhs.args_)),
        ret_(rhs.ret_)
    {}
    void operator=(WasmAstSig&& rhs) {
        args_ = Move(rhs.args_);
        ret_ = rhs.ret_;
    }
    const WasmAstValTypeVector& args() const {
        return args_;
    }
    ExprType ret() const {
        return ret_;
    }
    WasmName name() const {
        return name_;
    }
    bool operator==(const WasmAstSig& rhs) const {
        return ret() == rhs.ret() && EqualContainers(args(), rhs.args());
    }

    typedef const WasmAstSig& Lookup;
    static HashNumber hash(Lookup sig) {
        return AddContainerToHash(sig.args(), HashNumber(sig.ret()));
    }
    static bool match(const WasmAstSig* lhs, Lookup rhs) {
        return *lhs == rhs;
    }
};

class WasmAstNode : public WasmAstBase
{};

enum class WasmAstExprKind
{
    BinaryOperator,
    Block,
    Branch,
    Call,
    CallIndirect,
    ComparisonOperator,
    Const,
    ConversionOperator,
    GetLocal,
    IfElse,
    Load,
    Nop,
    Return,
    SetLocal,
    Store,
    TableSwitch,
    UnaryOperator,
};

class WasmAstExpr : public WasmAstNode
{
    const WasmAstExprKind kind_;

  protected:
    explicit WasmAstExpr(WasmAstExprKind kind)
      : kind_(kind)
    {}

  public:
    WasmAstExprKind kind() const { return kind_; }

    template <class T>
    T& as() {
        MOZ_ASSERT(kind() == T::Kind);
        return static_cast<T&>(*this);
    }
};

struct WasmAstNop : WasmAstExpr
{
    WasmAstNop()
      : WasmAstExpr(WasmAstExprKind::Nop)
    {}
};

class WasmAstConst : public WasmAstExpr
{
    const Val val_;

  public:
    static const WasmAstExprKind Kind = WasmAstExprKind::Const;
    explicit WasmAstConst(Val val)
      : WasmAstExpr(Kind),
        val_(val)
    {}
    Val val() const { return val_; }
};

class WasmAstGetLocal : public WasmAstExpr
{
    WasmRef local_;

  public:
    static const WasmAstExprKind Kind = WasmAstExprKind::GetLocal;
    explicit WasmAstGetLocal(WasmRef local)
      : WasmAstExpr(Kind),
        local_(local)
    {}
    WasmRef& local() {
        return local_;
    }
};

class WasmAstSetLocal : public WasmAstExpr
{
    WasmRef local_;
    WasmAstExpr& value_;

  public:
    static const WasmAstExprKind Kind = WasmAstExprKind::SetLocal;
    WasmAstSetLocal(WasmRef local, WasmAstExpr& value)
      : WasmAstExpr(Kind),
        local_(local),
        value_(value)
    {}
    WasmRef& local() {
        return local_;
    }
    WasmAstExpr& value() const {
        return value_;
    }
};

class WasmAstBlock : public WasmAstExpr
{
    Expr expr_;
    WasmName breakName_;
    WasmName continueName_;
    WasmAstExprVector exprs_;

  public:
    static const WasmAstExprKind Kind = WasmAstExprKind::Block;
    explicit WasmAstBlock(Expr expr, WasmName breakName, WasmName continueName,
                          WasmAstExprVector&& exprs)
      : WasmAstExpr(Kind),
        expr_(expr),
        breakName_(breakName),
        continueName_(continueName),
        exprs_(Move(exprs))
    {}

    Expr expr() const { return expr_; }
    WasmName breakName() const { return breakName_; }
    WasmName continueName() const { return continueName_; }
    const WasmAstExprVector& exprs() const { return exprs_; }
};

class WasmAstBranch : public WasmAstExpr
{
    Expr expr_;
    WasmAstExpr* cond_;
    WasmRef target_;

  public:
    static const WasmAstExprKind Kind = WasmAstExprKind::Branch;
    explicit WasmAstBranch(Expr expr, WasmAstExpr* cond, WasmRef target)
      : WasmAstExpr(Kind),
        expr_(expr),
        cond_(cond),
        target_(target)
    {}
    Expr expr() const { return expr_; }
    WasmRef& target() { return target_; }
    WasmAstExpr& cond() const { MOZ_ASSERT(cond_); return *cond_; }
};

class WasmAstCall : public WasmAstExpr
{
    Expr expr_;
    WasmRef func_;
    WasmAstExprVector args_;

  public:
    static const WasmAstExprKind Kind = WasmAstExprKind::Call;
    WasmAstCall(Expr expr, WasmRef func, WasmAstExprVector&& args)
      : WasmAstExpr(Kind), expr_(expr), func_(func), args_(Move(args))
    {}

    Expr expr() const { return expr_; }
    WasmRef& func() { return func_; }
    const WasmAstExprVector& args() const { return args_; }
};

class WasmAstCallIndirect : public WasmAstExpr
{
    WasmRef sig_;
    WasmAstExpr* index_;
    WasmAstExprVector args_;

  public:
    static const WasmAstExprKind Kind = WasmAstExprKind::CallIndirect;
    WasmAstCallIndirect(WasmRef sig, WasmAstExpr* index, WasmAstExprVector&& args)
      : WasmAstExpr(Kind), sig_(sig), index_(index), args_(Move(args))
    {}
    WasmRef& sig() { return sig_; }
    WasmAstExpr* index() const { return index_; }
    const WasmAstExprVector& args() const { return args_; }
};

class WasmAstReturn : public WasmAstExpr
{
    WasmAstExpr* maybeExpr_;

  public:
    static const WasmAstExprKind Kind = WasmAstExprKind::Return;
    explicit WasmAstReturn(WasmAstExpr* maybeExpr)
      : WasmAstExpr(Kind),
        maybeExpr_(maybeExpr)
    {}
    WasmAstExpr* maybeExpr() const { return maybeExpr_; }
};

class WasmAstIfElse : public WasmAstExpr
{
    Expr expr_;
    WasmAstExpr* cond_;
    WasmAstExpr* ifBody_;
    WasmAstExpr* elseBody_;

  public:
    static const WasmAstExprKind Kind = WasmAstExprKind::IfElse;
    WasmAstIfElse(Expr expr, WasmAstExpr* cond, WasmAstExpr* ifBody,
                  WasmAstExpr* elseBody = nullptr)
      : WasmAstExpr(Kind),
        expr_(expr),
        cond_(cond),
        ifBody_(ifBody),
        elseBody_(elseBody)
    {}

    bool hasElse() const { return expr_ == Expr::IfElse; }
    Expr expr() const { return expr_; }
    WasmAstExpr& cond() const { return *cond_; }
    WasmAstExpr& ifBody() const { return *ifBody_; }
    WasmAstExpr& elseBody() const { return *elseBody_; }
};

class WasmAstLoadStoreAddress
{
    WasmAstExpr* base_;
    int32_t offset_;
    int32_t align_;

  public:
    explicit WasmAstLoadStoreAddress(WasmAstExpr* base, int32_t offset,
                                     int32_t align)
      : base_(base),
        offset_(offset),
        align_(align)
    {}

    WasmAstExpr& base() const { return *base_; }
    int32_t offset() const { return offset_; }
    int32_t align() const { return align_; }
};

class WasmAstLoad : public WasmAstExpr
{
    Expr expr_;
    WasmAstLoadStoreAddress address_;

  public:
    static const WasmAstExprKind Kind = WasmAstExprKind::Load;
    explicit WasmAstLoad(Expr expr, const WasmAstLoadStoreAddress &address)
      : WasmAstExpr(Kind),
        expr_(expr),
        address_(address)
    {}

    Expr expr() const { return expr_; }
    const WasmAstLoadStoreAddress& address() const { return address_; }
};

class WasmAstStore : public WasmAstExpr
{
    Expr expr_;
    WasmAstLoadStoreAddress address_;
    WasmAstExpr* value_;

  public:
    static const WasmAstExprKind Kind = WasmAstExprKind::Store;
    explicit WasmAstStore(Expr expr, const WasmAstLoadStoreAddress &address,
                          WasmAstExpr* value)
      : WasmAstExpr(Kind),
        expr_(expr),
        address_(address),
        value_(value)
    {}

    Expr expr() const { return expr_; }
    const WasmAstLoadStoreAddress& address() const { return address_; }
    WasmAstExpr& value() const { return *value_; }
};

class WasmAstTableSwitch : public WasmAstExpr
{
    WasmAstExpr& index_;
    WasmRef default_;
    WasmRefVector table_;

  public:
    static const WasmAstExprKind Kind = WasmAstExprKind::TableSwitch;
    explicit WasmAstTableSwitch(WasmAstExpr& index, WasmRef def, WasmRefVector&& table)
      : WasmAstExpr(Kind),
        index_(index),
        default_(def),
        table_(Move(table))
    {}
    WasmAstExpr& index() const { return index_; }
    WasmRef& def() { return default_; }
    WasmRefVector& table() { return table_; }
};

class WasmAstFunc : public WasmAstNode
{
    WasmName name_;
    WasmRef sig_;
    WasmAstValTypeVector vars_;
    WasmNameVector localNames_;
    WasmAstExprVector body_;

  public:
    WasmAstFunc(WasmName name, WasmRef sig, WasmAstValTypeVector&& vars,
                WasmNameVector&& locals, WasmAstExprVector&& body)
      : name_(name),
        sig_(sig),
        vars_(Move(vars)),
        localNames_(Move(locals)),
        body_(Move(body))
    {}
    WasmRef& sig() { return sig_; }
    const WasmAstValTypeVector& vars() const { return vars_; }
    const WasmNameVector& locals() const { return localNames_; }
    const WasmAstExprVector& body() const { return body_; }
    WasmName name() const { return name_; }
};

class WasmAstImport : public WasmAstNode
{
    WasmName name_;
    WasmName module_;
    WasmName func_;
    uint32_t sigIndex_;

  public:
    WasmAstImport(WasmName name, WasmName module, WasmName func, uint32_t sigIndex)
      : name_(name), module_(module), func_(func), sigIndex_(sigIndex)
    {}
    WasmName name() const { return name_; }
    WasmName module() const { return module_; }
    WasmName func() const { return func_; }
    uint32_t sigIndex() const { return sigIndex_; }
};

enum class WasmAstExportKind { Func, Memory };

class WasmAstExport : public WasmAstNode
{
    WasmName name_;
    WasmAstExportKind kind_;
    WasmRef func_;

  public:
    WasmAstExport(WasmName name, WasmRef func)
      : name_(name), kind_(WasmAstExportKind::Func), func_(func)
    {}
    explicit WasmAstExport(WasmName name)
      : name_(name), kind_(WasmAstExportKind::Memory)
    {}
    WasmName name() const { return name_; }
    WasmAstExportKind kind() const { return kind_; }
    WasmRef& func() { return func_; }
};

typedef WasmAstVector<WasmRef> WasmAstTableElemVector;

class WasmAstTable : public WasmAstNode
{
    WasmAstTableElemVector elems_;

  public:
    explicit WasmAstTable(WasmAstTableElemVector&& elems) : elems_(Move(elems)) {}
    WasmAstTableElemVector& elems() { return elems_; }
};

class WasmAstSegment : public WasmAstNode
{
    uint32_t offset_;
    WasmName text_;

  public:
    WasmAstSegment(uint32_t offset, WasmName text)
      : offset_(offset), text_(text)
    {}
    uint32_t offset() const { return offset_; }
    WasmName text() const { return text_; }
};

typedef WasmAstVector<WasmAstSegment*> WasmAstSegmentVector;

class WasmAstMemory : public WasmAstNode
{
    uint32_t initialSize_;
    WasmAstSegmentVector segments_;

  public:
    explicit WasmAstMemory(uint32_t initialSize, WasmAstSegmentVector&& segments)
      : initialSize_(initialSize),
        segments_(Move(segments))
    {}
    uint32_t initialSize() const { return initialSize_; }
    const WasmAstSegmentVector& segments() const { return segments_; }
};

class WasmAstModule : public WasmAstNode
{
    typedef WasmAstVector<WasmAstFunc*> FuncVector;
    typedef WasmAstVector<WasmAstImport*> ImportVector;
    typedef WasmAstVector<WasmAstExport*> ExportVector;
    typedef WasmAstVector<WasmAstSig*> SigVector;
    typedef WasmAstHashMap<WasmAstSig*, uint32_t, WasmAstSig> SigMap;

    LifoAlloc& lifo_;
    WasmAstMemory* memory_;
    SigVector sigs_;
    SigMap sigMap_;
    ImportVector imports_;
    ExportVector exports_;
    WasmAstTable* table_;
    FuncVector funcs_;

  public:
    explicit WasmAstModule(LifoAlloc& lifo)
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
    bool setMemory(WasmAstMemory* memory) {
        if (memory_)
            return false;
        memory_ = memory;
        return true;
    }
    WasmAstMemory* maybeMemory() const {
        return memory_;
    }
    bool declare(WasmAstSig&& sig, uint32_t* sigIndex) {
        SigMap::AddPtr p = sigMap_.lookupForAdd(sig);
        if (p) {
            *sigIndex = p->value();
            return true;
        }
        *sigIndex = sigs_.length();
        return sigs_.append(new (lifo_) WasmAstSig(WasmName(), Move(sig))) &&
               sigMap_.add(p, sigs_.back(), *sigIndex);
    }
    bool append(WasmAstSig* sig) {
        uint32_t sigIndex = sigs_.length();
        if (!sigs_.append(sig))
            return false;
        SigMap::AddPtr p = sigMap_.lookupForAdd(*sig);
        return p || sigMap_.add(p, sig, sigIndex);
    }
    const SigVector& sigs() const {
        return sigs_;
    }
    bool append(WasmAstFunc* func) {
        return funcs_.append(func);
    }
    const FuncVector& funcs() const {
        return funcs_;
    }
    const ImportVector& imports() const {
        return imports_;
    }
    bool append(WasmAstImport* imp) {
        return imports_.append(imp);
    }
    bool append(WasmAstExport* exp) {
        return exports_.append(exp);
    }
    const ExportVector& exports() const {
        return exports_;
    }
    bool initTable(WasmAstTable* table) {
        if (table_)
            return false;
        table_ = table;
        return true;
    }
    WasmAstTable* maybeTable() const {
        return table_;
    }
};

class WasmAstUnaryOperator final : public WasmAstExpr
{
    Expr expr_;
    WasmAstExpr* op_;

  public:
    static const WasmAstExprKind Kind = WasmAstExprKind::UnaryOperator;
    explicit WasmAstUnaryOperator(Expr expr, WasmAstExpr* op)
      : WasmAstExpr(Kind),
        expr_(expr), op_(op)
    {}

    Expr expr() const { return expr_; }
    WasmAstExpr* op() const { return op_; }
};

class WasmAstBinaryOperator final : public WasmAstExpr
{
    Expr expr_;
    WasmAstExpr* lhs_;
    WasmAstExpr* rhs_;

  public:
    static const WasmAstExprKind Kind = WasmAstExprKind::BinaryOperator;
    explicit WasmAstBinaryOperator(Expr expr, WasmAstExpr* lhs, WasmAstExpr* rhs)
      : WasmAstExpr(Kind),
        expr_(expr), lhs_(lhs), rhs_(rhs)
    {}

    Expr expr() const { return expr_; }
    WasmAstExpr* lhs() const { return lhs_; }
    WasmAstExpr* rhs() const { return rhs_; }
};

class WasmAstComparisonOperator final : public WasmAstExpr
{
    Expr expr_;
    WasmAstExpr* lhs_;
    WasmAstExpr* rhs_;

  public:
    static const WasmAstExprKind Kind = WasmAstExprKind::ComparisonOperator;
    explicit WasmAstComparisonOperator(Expr expr, WasmAstExpr* lhs, WasmAstExpr* rhs)
      : WasmAstExpr(Kind),
        expr_(expr), lhs_(lhs), rhs_(rhs)
    {}

    Expr expr() const { return expr_; }
    WasmAstExpr* lhs() const { return lhs_; }
    WasmAstExpr* rhs() const { return rhs_; }
};

class WasmAstConversionOperator final : public WasmAstExpr
{
    Expr expr_;
    WasmAstExpr* op_;

  public:
    static const WasmAstExprKind Kind = WasmAstExprKind::ConversionOperator;
    explicit WasmAstConversionOperator(Expr expr, WasmAstExpr* op)
      : WasmAstExpr(Kind),
        expr_(expr), op_(op)
    {}

    Expr expr() const { return expr_; }
    WasmAstExpr* op() const { return op_; }
};

} // end anonymous namespace

/*****************************************************************************/
// wasm text token stream

namespace {

class WasmToken
{
  public:
    enum FloatLiteralKind
    {
        HexNumber,
        DecNumber,
        Infinity,
        NaN
    };

    enum Kind
    {
        Align,
        BinaryOpcode,
        Block,
        Br,
        BrIf,
        Call,
        CallImport,
        CallIndirect,
        CloseParen,
        ComparisonOpcode,
        Const,
        ConversionOpcode,
        EndOfFile,
        Equal,
        Error,
        Export,
        Float,
        Func,
        GetLocal,
        If,
        IfElse,
        Import,
        Index,
        UnsignedInteger,
        SignedInteger,
        Memory,
        Load,
        Local,
        Loop,
        Module,
        Name,
        Nop,
        Offset,
        OpenParen,
        Param,
        Result,
        Return,
        Segment,
        SetLocal,
        Store,
        Table,
        TableSwitch,
        Text,
        Type,
        UnaryOpcode,
        ValueType
    };
  private:
    Kind kind_;
    const char16_t* begin_;
    const char16_t* end_;
    union {
        uint32_t index_;
        uint64_t uint_;
        int64_t sint_;
        FloatLiteralKind floatLiteralKind_;
        ValType valueType_;
        Expr expr_;
    } u;
  public:
    explicit WasmToken() = default;
    WasmToken(Kind kind, const char16_t* begin, const char16_t* end)
      : kind_(kind),
        begin_(begin),
        end_(end)
    {
        MOZ_ASSERT(kind_ != Error);
        MOZ_ASSERT((kind == EndOfFile) == (begin == end));
    }
    explicit WasmToken(uint32_t index, const char16_t* begin, const char16_t* end)
      : kind_(Index),
        begin_(begin),
        end_(end)
    {
        MOZ_ASSERT(begin != end);
        u.index_ = index;
    }
    explicit WasmToken(uint64_t uint, const char16_t* begin, const char16_t* end)
      : kind_(UnsignedInteger),
        begin_(begin),
        end_(end)
    {
        MOZ_ASSERT(begin != end);
        u.uint_ = uint;
    }
    explicit WasmToken(int64_t sint, const char16_t* begin, const char16_t* end)
      : kind_(SignedInteger),
        begin_(begin),
        end_(end)
    {
        MOZ_ASSERT(begin != end);
        u.sint_ = sint;
    }
    explicit WasmToken(FloatLiteralKind floatLiteralKind,
                       const char16_t* begin, const char16_t* end)
      : kind_(Float),
        begin_(begin),
        end_(end)
    {
        MOZ_ASSERT(begin != end);
        u.floatLiteralKind_ = floatLiteralKind;
    }
    explicit WasmToken(Kind kind, ValType valueType, const char16_t* begin, const char16_t* end)
      : kind_(kind),
        begin_(begin),
        end_(end)
    {
        MOZ_ASSERT(begin != end);
        MOZ_ASSERT(kind_ == ValueType || kind_ == Const);
        u.valueType_ = valueType;
    }
    explicit WasmToken(Kind kind, Expr expr, const char16_t* begin, const char16_t* end)
      : kind_(kind),
        begin_(begin),
        end_(end)
    {
        MOZ_ASSERT(begin != end);
        MOZ_ASSERT(kind_ == UnaryOpcode || kind_ == BinaryOpcode || kind_ == ComparisonOpcode ||
                   kind_ == ConversionOpcode || kind_ == Load || kind_ == Store);
        u.expr_ = expr;
    }
    explicit WasmToken(const char16_t* begin)
      : kind_(Error),
        begin_(begin),
        end_(begin)
    {}
    Kind kind() const {
        return kind_;
    }
    const char16_t* begin() const {
        return begin_;
    }
    const char16_t* end() const {
        return end_;
    }
    WasmName text() const {
        MOZ_ASSERT(kind_ == Text);
        MOZ_ASSERT(begin_[0] == '"');
        MOZ_ASSERT(end_[-1] == '"');
        MOZ_ASSERT(end_ - begin_ >= 2);
        return WasmName(begin_ + 1, end_ - begin_ - 2);
    }
    WasmName name() const {
        return WasmName(begin_, end_ - begin_);
    }
    uint32_t index() const {
        MOZ_ASSERT(kind_ == Index);
        return u.index_;
    }
    uint64_t uint() const {
        MOZ_ASSERT(kind_ == UnsignedInteger);
        return u.uint_;
    }
    int64_t sint() const {
        MOZ_ASSERT(kind_ == SignedInteger);
        return u.sint_;
    }
    FloatLiteralKind floatLiteralKind() const {
        MOZ_ASSERT(kind_ == Float);
        return u.floatLiteralKind_;
    }
    ValType valueType() const {
        MOZ_ASSERT(kind_ == ValueType || kind_ == Const);
        return u.valueType_;
    }
    Expr expr() const {
        MOZ_ASSERT(kind_ == UnaryOpcode || kind_ == BinaryOpcode || kind_ == ComparisonOpcode ||
                   kind_ == ConversionOpcode || kind_ == Load || kind_ == Store);
        return u.expr_;
    }
};

} // end anonymous namespace

static bool
IsWasmNewLine(char16_t c)
{
    return c == '\n';
}

static bool
IsWasmSpace(char16_t c)
{
    switch (c) {
      case ' ':
      case '\n':
      case '\r':
      case '\t':
      case '\v':
      case '\f':
        return true;
      default:
        return false;
    }
}

static bool
IsWasmDigit(char16_t c)
{
    return c >= '0' && c <= '9';
}

static bool
IsWasmLetter(char16_t c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool
IsNameAfterDollar(char16_t c)
{
    return IsWasmLetter(c) || IsWasmDigit(c) || c == '_' || c == '$' || c == '-';
}

static bool
IsHexDigit(char c, uint8_t* value)
{
    if (c >= '0' && c <= '9') {
        *value = c - '0';
        return true;
    }

    if (c >= 'a' && c <= 'f') {
        *value = 10 + (c - 'a');
        return true;
    }

    if (c >= 'A' && c <= 'F') {
        *value = 10 + (c - 'A');
        return true;
    }

    return false;
}

static WasmToken
LexHexFloatLiteral(const char16_t* begin, const char16_t* end, const char16_t** curp)
{
    const char16_t* cur = begin;

    if (cur != end && (*cur == '-' || *cur == '+'))
        cur++;

    MOZ_ASSERT(cur != end && *cur == '0');
    cur++;
    MOZ_ASSERT(cur != end && *cur == 'x');
    cur++;

    uint8_t digit;
    while (cur != end && IsHexDigit(*cur, &digit))
        cur++;

    if (cur != end && *cur == '.')
        cur++;

    while (cur != end && IsHexDigit(*cur, &digit))
        cur++;

    if (cur != end && *cur == 'p') {
        cur++;

        if (cur != end && (*cur == '-' || *cur == '+'))
            cur++;

        while (cur != end && IsWasmDigit(*cur))
            cur++;
    }

    *curp = cur;
    return WasmToken(WasmToken::HexNumber, begin, cur);
}

static WasmToken
LexDecFloatLiteral(const char16_t* begin, const char16_t* end, const char16_t** curp)
{
    const char16_t* cur = begin;

    if (cur != end && (*cur == '-' || *cur == '+'))
        cur++;

    while (cur != end && IsWasmDigit(*cur))
        cur++;

    if (cur != end && *cur == '.')
        cur++;

    while (cur != end && IsWasmDigit(*cur))
        cur++;

    if (cur != end && *cur == 'e') {
        cur++;

        if (cur != end && (*cur == '-' || *cur == '+'))
            cur++;

        while (cur != end && IsWasmDigit(*cur))
            cur++;
    }

    *curp = cur;
    return WasmToken(WasmToken::DecNumber, begin, cur);
}

static bool
ConsumeTextByte(const char16_t** curp, const char16_t* end, uint8_t *byte = nullptr)
{
    const char16_t*& cur = *curp;
    MOZ_ASSERT(cur != end);

    if (*cur != '\\') {
        if (byte)
            *byte = *cur;
        cur++;
        return true;
    }

    if (++cur == end)
        return false;

    uint8_t u8;
    switch (*cur) {
      case 'n': u8 = '\n'; break;
      case 't': u8 = '\t'; break;
      case '\\': u8 = '\\'; break;
      case '\"': u8 = '\"'; break;
      case '\'': u8 = '\''; break;
      default: {
        uint8_t lowNibble;
        if (!IsHexDigit(*cur, &lowNibble))
            return false;

        if (++cur == end)
            return false;

        uint8_t highNibble;
        if (!IsHexDigit(*cur, &highNibble))
            return false;

        u8 = lowNibble | (highNibble << 4);
        break;
      }
    }

    if (byte)
        *byte = u8;
    cur++;
    return true;
}

namespace {

class WasmTokenStream
{
    static const uint32_t LookaheadSize = 2;

    const char16_t* cur_;
    const char16_t* const end_;
    const char16_t* lineStart_;
    unsigned line_;
    uint32_t lookaheadIndex_;
    uint32_t lookaheadDepth_;
    WasmToken lookahead_[LookaheadSize];

    bool consume(const char16_t* match) {
        const char16_t* p = cur_;
        for (; *match; p++, match++) {
            if (p == end_ || *p != *match)
                return false;
        }
        cur_ = p;
        return true;
    }
    WasmToken fail(const char16_t* begin) const {
        return WasmToken(begin);
    }

    WasmToken nan(const char16_t* begin);
    WasmToken literal(const char16_t* begin);
    WasmToken next();

  public:
    WasmTokenStream(const char16_t* text, UniqueChars* error)
      : cur_(text),
        end_(text + js_strlen(text)),
        lineStart_(text),
        line_(1),
        lookaheadIndex_(0),
        lookaheadDepth_(0)
    {}
    void generateError(WasmToken token, UniqueChars* error) {
        unsigned column = token.begin() - lineStart_ + 1;
        error->reset(JS_smprintf("parsing wasm text at %u:%u", line_, column));
    }
    WasmToken peek() {
        if (!lookaheadDepth_) {
            lookahead_[lookaheadIndex_] = next();
            lookaheadDepth_ = 1;
        }
        return lookahead_[lookaheadIndex_];
    }
    WasmToken get() {
        static_assert(LookaheadSize == 2, "can just flip");
        if (lookaheadDepth_) {
            lookaheadDepth_--;
            WasmToken ret = lookahead_[lookaheadIndex_];
            lookaheadIndex_ ^= 1;
            return ret;
        }
        return next();
    }
    void unget(WasmToken token) {
        static_assert(LookaheadSize == 2, "can just flip");
        lookaheadDepth_++;
        lookaheadIndex_ ^= 1;
        lookahead_[lookaheadIndex_] = token;
    }

    // Helpers:
    bool getIf(WasmToken::Kind kind, WasmToken* token) {
        if (peek().kind() == kind) {
            *token = get();
            return true;
        }
        return false;
    }
    bool getIf(WasmToken::Kind kind) {
        WasmToken token;
        if (getIf(kind, &token))
            return true;
        return false;
    }
    WasmName getIfName() {
        WasmToken token;
        if (getIf(WasmToken::Name, &token))
            return token.name();
        return WasmName();
    }
    bool getIfRef(WasmRef* ref) {
        WasmToken token = peek();
        if (token.kind() == WasmToken::Name || token.kind() == WasmToken::Index)
            return matchRef(ref, nullptr);
        return false;
    }
    bool match(WasmToken::Kind expect, WasmToken* token, UniqueChars* error) {
        *token = get();
        if (token->kind() == expect)
            return true;
        generateError(*token, error);
        return false;
    }
    bool match(WasmToken::Kind expect, UniqueChars* error) {
        WasmToken token;
        return match(expect, &token, error);
    }
    bool matchRef(WasmRef* ref, UniqueChars* error) {
        WasmToken token = get();
        switch (token.kind()) {
          case WasmToken::Name:
            *ref = WasmRef(token.name(), WasmNoIndex);
            break;
          case WasmToken::Index:
            *ref = WasmRef(WasmName(), token.index());
            break;
          default:
            generateError(token, error);
            return false;
        }
        return true;
    }
};

} // end anonymous namespace

WasmToken
WasmTokenStream::nan(const char16_t* begin)
{
    if (consume(MOZ_UTF16(":"))) {
        if (!consume(MOZ_UTF16("0x")))
            return fail(begin);

        uint8_t digit;
        while (cur_ != end_ && IsHexDigit(*cur_, &digit))
            cur_++;
    }

    return WasmToken(WasmToken::NaN, begin, cur_);
}

WasmToken
WasmTokenStream::literal(const char16_t* begin)
{
    CheckedInt<uint64_t> u = 0;
    if (consume(MOZ_UTF16("0x"))) {
        if (cur_ == end_)
            return fail(begin);

        do {
            if (*cur_ == '.' || *cur_ == 'p')
                return LexHexFloatLiteral(begin, end_, &cur_);

            uint8_t digit;
            if (!IsHexDigit(*cur_, &digit))
                break;

            u *= 16;
            u += digit;
            if (!u.isValid())
                return LexHexFloatLiteral(begin, end_, &cur_);

            cur_++;
        } while (cur_ != end_);

        if (*begin == '-') {
            uint64_t value = u.value();
            if (value > uint64_t(INT64_MIN))
                return LexHexFloatLiteral(begin, end_, &cur_);

            value = -value;
            return WasmToken(int64_t(value), begin, cur_);
        }
    } else {
        while (cur_ != end_) {
            if (*cur_ == '.' || *cur_ == 'e')
                return LexDecFloatLiteral(begin, end_, &cur_);

            if (!IsWasmDigit(*cur_))
                break;

            u *= 10;
            u += *cur_ - '0';
            if (!u.isValid())
                return LexDecFloatLiteral(begin, end_, &cur_);

            cur_++;
        }

        if (*begin == '-') {
            uint64_t value = u.value();
            if (value > uint64_t(INT64_MIN))
                return LexDecFloatLiteral(begin, end_, &cur_);

            value = -value;
            return WasmToken(int64_t(value), begin, cur_);
        }
    }

    CheckedInt<uint32_t> index = u.value();
    if (index.isValid())
        return WasmToken(index.value(), begin, cur_);

    return WasmToken(u.value(), begin, cur_);
}

WasmToken
WasmTokenStream::next()
{
    while (cur_ != end_ && IsWasmSpace(*cur_)) {
        if (IsWasmNewLine(*cur_++)) {
            lineStart_ = cur_;
            line_++;
        }
    }

    if (cur_ == end_)
        return WasmToken(WasmToken::EndOfFile, cur_, cur_);

    const char16_t* begin = cur_;
    switch (*begin) {
      case '"':
        cur_++;
        while (true) {
            if (cur_ == end_)
                return fail(begin);
            if (*cur_ == '"')
                break;
            if (!ConsumeTextByte(&cur_, end_))
                return fail(begin);
        }
        cur_++;
        return WasmToken(WasmToken::Text, begin, cur_);

      case '$':
        cur_++;
        while (cur_ != end_ && IsNameAfterDollar(*cur_))
            cur_++;
        return WasmToken(WasmToken::Name, begin, cur_);

      case '(':
        cur_++;
        return WasmToken(WasmToken::OpenParen, begin, cur_);

      case ')':
        cur_++;
        return WasmToken(WasmToken::CloseParen, begin, cur_);

      case '=':
        cur_++;
        return WasmToken(WasmToken::Equal, begin, cur_);

      case '+': case '-':
        cur_++;
        if (consume(MOZ_UTF16("infinity")))
            return WasmToken(WasmToken::Infinity, begin, cur_);
        if (consume(MOZ_UTF16("nan")))
            return nan(begin);
        if (!IsWasmDigit(*cur_))
            break;
        MOZ_FALLTHROUGH;
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
        return literal(begin);

      case 'a':
        if (consume(MOZ_UTF16("align")))
            return WasmToken(WasmToken::Align, begin, cur_);
        break;

      case 'b':
        if (consume(MOZ_UTF16("block")))
            return WasmToken(WasmToken::Block, begin, cur_);
        if (consume(MOZ_UTF16("br"))) {
            if (consume(MOZ_UTF16("_if")))
                return WasmToken(WasmToken::BrIf, begin, cur_);
            return WasmToken(WasmToken::Br, begin, cur_);
        }
        break;

      case 'c':
        if (consume(MOZ_UTF16("call"))) {
            if (consume(MOZ_UTF16("_indirect")))
                return WasmToken(WasmToken::CallIndirect, begin, cur_);
            if (consume(MOZ_UTF16("_import")))
                return WasmToken(WasmToken::CallImport, begin, cur_);
            return WasmToken(WasmToken::Call, begin, cur_);
        }
        break;

      case 'e':
        if (consume(MOZ_UTF16("export")))
            return WasmToken(WasmToken::Export, begin, cur_);
        break;

      case 'f':
        if (consume(MOZ_UTF16("func")))
            return WasmToken(WasmToken::Func, begin, cur_);

        if (consume(MOZ_UTF16("f32"))) {
            if (!consume(MOZ_UTF16(".")))
                return WasmToken(WasmToken::ValueType, ValType::F32, begin, cur_);

            switch (*cur_) {
              case 'a':
                if (consume(MOZ_UTF16("abs")))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::F32Abs, begin, cur_);
                if (consume(MOZ_UTF16("add")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::F32Add, begin, cur_);
                break;
              case 'c':
                if (consume(MOZ_UTF16("ceil")))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::F32Ceil, begin, cur_);
                if (consume(MOZ_UTF16("const")))
                    return WasmToken(WasmToken::Const, ValType::F32, begin, cur_);
                if (consume(MOZ_UTF16("convert_s/i32")))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::F32ConvertSI32,
                                     begin, cur_);
                if (consume(MOZ_UTF16("convert_u/i32")))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::F32ConvertUI32,
                                     begin, cur_);
                if (consume(MOZ_UTF16("copysign")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::F32CopySign, begin, cur_);
                break;
              case 'd':
                if (consume(MOZ_UTF16("demote/f64")))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::F32DemoteF64,
                                     begin, cur_);
                if (consume(MOZ_UTF16("div")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::F32Div, begin, cur_);
                break;
              case 'e':
                if (consume(MOZ_UTF16("eq")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::F32Eq, begin, cur_);
                break;
              case 'f':
                if (consume(MOZ_UTF16("floor")))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::F32Floor, begin, cur_);
                break;
              case 'g':
                if (consume(MOZ_UTF16("ge")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::F32Ge, begin, cur_);
                if (consume(MOZ_UTF16("gt")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::F32Gt, begin, cur_);
                break;
              case 'l':
                if (consume(MOZ_UTF16("le")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::F32Le, begin, cur_);
                if (consume(MOZ_UTF16("lt")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::F32Lt, begin, cur_);
                if (consume(MOZ_UTF16("load")))
                    return WasmToken(WasmToken::Load, Expr::F32LoadMem, begin, cur_);
                break;
              case 'm':
                if (consume(MOZ_UTF16("max")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::F32Max, begin, cur_);
                if (consume(MOZ_UTF16("min")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::F32Min, begin, cur_);
                if (consume(MOZ_UTF16("mul")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::F32Mul, begin, cur_);
                break;
              case 'n':
                if (consume(MOZ_UTF16("nearest")))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::F32Nearest, begin, cur_);
                if (consume(MOZ_UTF16("neg")))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::F32Neg, begin, cur_);
                if (consume(MOZ_UTF16("ne")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::F32Ne, begin, cur_);
                break;
              case 'r':
                if (consume(MOZ_UTF16("reinterpret/i32")))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::F32ReinterpretI32,
                                     begin, cur_);
                break;
              case 's':
                if (consume(MOZ_UTF16("sqrt")))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::F32Sqrt, begin, cur_);
                if (consume(MOZ_UTF16("sub")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::F32Sub, begin, cur_);
                if (consume(MOZ_UTF16("store")))
                    return WasmToken(WasmToken::Store, Expr::F32StoreMem, begin, cur_);
                break;
              case 't':
                if (consume(MOZ_UTF16("trunc")))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::F32Trunc, begin, cur_);
                break;
            }
            break;
        }
        if (consume(MOZ_UTF16("f64"))) {
            if (!consume(MOZ_UTF16(".")))
                return WasmToken(WasmToken::ValueType, ValType::F64, begin, cur_);

            switch (*cur_) {
              case 'a':
                if (consume(MOZ_UTF16("abs")))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::F64Abs, begin, cur_);
                if (consume(MOZ_UTF16("add")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::F64Add, begin, cur_);
                break;
              case 'c':
                if (consume(MOZ_UTF16("ceil")))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::F64Ceil, begin, cur_);
                if (consume(MOZ_UTF16("const")))
                    return WasmToken(WasmToken::Const, ValType::F64, begin, cur_);
                if (consume(MOZ_UTF16("convert_s/i32")))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::F64ConvertSI32,
                                     begin, cur_);
                if (consume(MOZ_UTF16("convert_u/i32")))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::F64ConvertUI32,
                                     begin, cur_);
                if (consume(MOZ_UTF16("copysign")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::F64CopySign, begin, cur_);
                break;
              case 'd':
                if (consume(MOZ_UTF16("div")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::F64Div, begin, cur_);
                break;
              case 'e':
                if (consume(MOZ_UTF16("eq")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::F64Eq, begin, cur_);
                break;
              case 'f':
                if (consume(MOZ_UTF16("floor")))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::F64Floor, begin, cur_);
                break;
              case 'g':
                if (consume(MOZ_UTF16("ge")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::F64Ge, begin, cur_);
                if (consume(MOZ_UTF16("gt")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::F64Gt, begin, cur_);
                break;
              case 'l':
                if (consume(MOZ_UTF16("le")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::F64Le, begin, cur_);
                if (consume(MOZ_UTF16("lt")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::F64Lt, begin, cur_);
                if (consume(MOZ_UTF16("load")))
                    return WasmToken(WasmToken::Load, Expr::F64LoadMem, begin, cur_);
                break;
              case 'm':
                if (consume(MOZ_UTF16("max")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::F64Max, begin, cur_);
                if (consume(MOZ_UTF16("min")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::F64Min, begin, cur_);
                if (consume(MOZ_UTF16("mul")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::F64Mul, begin, cur_);
                break;
              case 'n':
                if (consume(MOZ_UTF16("nearest")))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::F64Nearest, begin, cur_);
                if (consume(MOZ_UTF16("neg")))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::F64Neg, begin, cur_);
                if (consume(MOZ_UTF16("ne")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::F64Ne, begin, cur_);
                break;
              case 'p':
                if (consume(MOZ_UTF16("promote/f32")))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::F64PromoteF32,
                                     begin, cur_);
                break;
              case 's':
                if (consume(MOZ_UTF16("sqrt")))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::F64Sqrt, begin, cur_);
                if (consume(MOZ_UTF16("sub")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::F64Sub, begin, cur_);
                if (consume(MOZ_UTF16("store")))
                    return WasmToken(WasmToken::Store, Expr::F64StoreMem, begin, cur_);
                break;
              case 't':
                if (consume(MOZ_UTF16("trunc")))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::F64Trunc, begin, cur_);
                break;
            }
            break;
        }
        break;

      case 'g':
        if (consume(MOZ_UTF16("get_local")))
            return WasmToken(WasmToken::GetLocal, begin, cur_);
        break;

      case 'i':
        if (consume(MOZ_UTF16("i32"))) {
            if (!consume(MOZ_UTF16(".")))
                return WasmToken(WasmToken::ValueType, ValType::I32, begin, cur_);

            switch (*cur_) {
              case 'a':
                if (consume(MOZ_UTF16("add")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I32Add, begin, cur_);
                if (consume(MOZ_UTF16("and")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I32And, begin, cur_);
                break;
              case 'c':
                if (consume(MOZ_UTF16("const")))
                    return WasmToken(WasmToken::Const, ValType::I32, begin, cur_);
                if (consume(MOZ_UTF16("clz")))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::I32Clz, begin, cur_);
                if (consume(MOZ_UTF16("ctz")))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::I32Ctz, begin, cur_);
                break;
              case 'd':
                if (consume(MOZ_UTF16("div_s")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I32DivS, begin, cur_);
                if (consume(MOZ_UTF16("div_u")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I32DivU, begin, cur_);
                break;
              case 'e':
                if (consume(MOZ_UTF16("eq")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I32Eq, begin, cur_);
                break;
              case 'g':
                if (consume(MOZ_UTF16("ge_s")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I32GeS, begin, cur_);
                if (consume(MOZ_UTF16("ge_u")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I32GeU, begin, cur_);
                if (consume(MOZ_UTF16("gt_s")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I32GtS, begin, cur_);
                if (consume(MOZ_UTF16("gt_u")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I32GtU, begin, cur_);
                break;
              case 'l':
                if (consume(MOZ_UTF16("le_s")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I32LeS, begin, cur_);
                if (consume(MOZ_UTF16("le_u")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I32LeU, begin, cur_);
                if (consume(MOZ_UTF16("lt_s")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I32LtS, begin, cur_);
                if (consume(MOZ_UTF16("lt_u")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I32LtU, begin, cur_);
                if (consume(MOZ_UTF16("load"))) {
                    if (IsWasmSpace(*cur_))
                        return WasmToken(WasmToken::Load, Expr::I32LoadMem, begin, cur_);
                    if (consume(MOZ_UTF16("8_s")))
                        return WasmToken(WasmToken::Load, Expr::I32LoadMem8S, begin, cur_);
                    if (consume(MOZ_UTF16("8_u")))
                        return WasmToken(WasmToken::Load, Expr::I32LoadMem8U, begin, cur_);
                    if (consume(MOZ_UTF16("16_s")))
                        return WasmToken(WasmToken::Load, Expr::I32LoadMem16S, begin, cur_);
                    if (consume(MOZ_UTF16("16_u")))
                        return WasmToken(WasmToken::Load, Expr::I32LoadMem16U, begin, cur_);
                    break;
                }
                break;
              case 'm':
                if (consume(MOZ_UTF16("mul")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I32Mul, begin, cur_);
                break;
              case 'n':
                if (consume(MOZ_UTF16("ne")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I32Ne, begin, cur_);
                break;
              case 'o':
                if (consume(MOZ_UTF16("or")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I32Or, begin, cur_);
                break;
              case 'p':
                if (consume(MOZ_UTF16("popcnt")))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::I32Popcnt, begin, cur_);
                break;
              case 'r':
                if (consume(MOZ_UTF16("reinterpret/f32")))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::I32ReinterpretF32,
                                     begin, cur_);
                if (consume(MOZ_UTF16("rem_s")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I32RemS, begin, cur_);
                if (consume(MOZ_UTF16("rem_u")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I32RemU, begin, cur_);
                break;
              case 's':
                if (consume(MOZ_UTF16("sub")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I32Sub, begin, cur_);
                if (consume(MOZ_UTF16("shl")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I32Shl, begin, cur_);
                if (consume(MOZ_UTF16("shr_s")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I32ShrS, begin, cur_);
                if (consume(MOZ_UTF16("shr_u")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I32ShrU, begin, cur_);
                if (consume(MOZ_UTF16("store"))) {
                    if (IsWasmSpace(*cur_))
                        return WasmToken(WasmToken::Store, Expr::I32StoreMem, begin, cur_);
                    if (consume(MOZ_UTF16("8")))
                        return WasmToken(WasmToken::Store, Expr::I32StoreMem8, begin, cur_);
                    if (consume(MOZ_UTF16("16")))
                        return WasmToken(WasmToken::Store, Expr::I32StoreMem16, begin, cur_);
                    break;
                }
                break;
              case 't':
                if (consume(MOZ_UTF16("trunc_s/f32")))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::I32TruncSF32,
                                     begin, cur_);
                if (consume(MOZ_UTF16("trunc_s/f64")))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::I32TruncSF64,
                                     begin, cur_);
                if (consume(MOZ_UTF16("trunc_u/f32")))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::I32TruncUF32,
                                     begin, cur_);
                if (consume(MOZ_UTF16("trunc_u/f64")))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::I32TruncUF64,
                                     begin, cur_);
                break;
              case 'w':
                if (consume(MOZ_UTF16("wrap/i64")))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::I32WrapI64,
                                     begin, cur_);
                break;
              case 'x':
                if (consume(MOZ_UTF16("xor")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I32Xor, begin, cur_);
                break;
            }
            break;
        }
        if (consume(MOZ_UTF16("i64"))) {
            if (!consume(MOZ_UTF16(".")))
                return WasmToken(WasmToken::ValueType, ValType::I64, begin, cur_);

            switch (*cur_) {
              case 'a':
                if (consume(MOZ_UTF16("add")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I64Add, begin, cur_);
                if (consume(MOZ_UTF16("and")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I64And, begin, cur_);
                break;
              case 'c':
                if (consume(MOZ_UTF16("const")))
                    return WasmToken(WasmToken::Const, ValType::I64, begin, cur_);
                if (consume(MOZ_UTF16("clz")))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::I64Clz, begin, cur_);
                if (consume(MOZ_UTF16("ctz")))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::I64Ctz, begin, cur_);
                break;
              case 'd':
                if (consume(MOZ_UTF16("div_s")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I64DivS, begin, cur_);
                if (consume(MOZ_UTF16("div_u")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I64DivU, begin, cur_);
                break;
              case 'e':
                if (consume(MOZ_UTF16("eq")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I64Eq, begin, cur_);
                if (consume(MOZ_UTF16("extend_s/i32")))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::I64ExtendSI32,
                                     begin, cur_);
                if (consume(MOZ_UTF16("extend_u/i32")))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::I64ExtendUI32,
                                     begin, cur_);
                break;
              case 'g':
                if (consume(MOZ_UTF16("ge_s")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I64GeS, begin, cur_);
                if (consume(MOZ_UTF16("ge_u")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I64GeU, begin, cur_);
                if (consume(MOZ_UTF16("gt_s")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I64GtS, begin, cur_);
                if (consume(MOZ_UTF16("gt_u")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I64GtU, begin, cur_);
                break;
              case 'l':
                if (consume(MOZ_UTF16("le_s")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I64LeS, begin, cur_);
                if (consume(MOZ_UTF16("le_u")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I64LeU, begin, cur_);
                if (consume(MOZ_UTF16("lt_s")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I64LtS, begin, cur_);
                if (consume(MOZ_UTF16("lt_u")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I64LtU, begin, cur_);
                if (consume(MOZ_UTF16("load"))) {
                    if (IsWasmSpace(*cur_))
                        return WasmToken(WasmToken::Load, Expr::I64LoadMem, begin, cur_);
                    if (consume(MOZ_UTF16("8_s")))
                        return WasmToken(WasmToken::Load, Expr::I64LoadMem8S, begin, cur_);
                    if (consume(MOZ_UTF16("8_u")))
                        return WasmToken(WasmToken::Load, Expr::I64LoadMem8U, begin, cur_);
                    if (consume(MOZ_UTF16("16_s")))
                        return WasmToken(WasmToken::Load, Expr::I64LoadMem16S, begin, cur_);
                    if (consume(MOZ_UTF16("16_u")))
                        return WasmToken(WasmToken::Load, Expr::I64LoadMem16U, begin, cur_);
                    if (consume(MOZ_UTF16("32_s")))
                        return WasmToken(WasmToken::Load, Expr::I64LoadMem32S, begin, cur_);
                    if (consume(MOZ_UTF16("32_u")))
                        return WasmToken(WasmToken::Load, Expr::I64LoadMem32U, begin, cur_);
                    break;
                }
                break;
              case 'm':
                if (consume(MOZ_UTF16("mul")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I64Mul, begin, cur_);
                break;
              case 'n':
                if (consume(MOZ_UTF16("ne")))
                    return WasmToken(WasmToken::ComparisonOpcode, Expr::I64Ne, begin, cur_);
                break;
              case 'o':
                if (consume(MOZ_UTF16("or")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I64Or, begin, cur_);
                break;
              case 'p':
                if (consume(MOZ_UTF16("popcnt")))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::I64Popcnt, begin, cur_);
                break;
              case 'r':
                if (consume(MOZ_UTF16("reinterpret/f64")))
                    return WasmToken(WasmToken::UnaryOpcode, Expr::I64ReinterpretF64,
                                     begin, cur_);
                if (consume(MOZ_UTF16("rem_s")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I64RemS, begin, cur_);
                if (consume(MOZ_UTF16("rem_u")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I64RemU, begin, cur_);
                break;
              case 's':
                if (consume(MOZ_UTF16("sub")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I64Sub, begin, cur_);
                if (consume(MOZ_UTF16("shl")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I64Shl, begin, cur_);
                if (consume(MOZ_UTF16("shr_s")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I64ShrS, begin, cur_);
                if (consume(MOZ_UTF16("shr_u")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I64ShrU, begin, cur_);
                if (consume(MOZ_UTF16("store"))) {
                    if (IsWasmSpace(*cur_))
                        return WasmToken(WasmToken::Store, Expr::I64StoreMem, begin, cur_);
                    if (consume(MOZ_UTF16("8")))
                        return WasmToken(WasmToken::Store, Expr::I64StoreMem8, begin, cur_);
                    if (consume(MOZ_UTF16("16")))
                        return WasmToken(WasmToken::Store, Expr::I64StoreMem16, begin, cur_);
                    if (consume(MOZ_UTF16("32")))
                        return WasmToken(WasmToken::Store, Expr::I64StoreMem32, begin, cur_);
                    break;
                }
                break;
              case 't':
                if (consume(MOZ_UTF16("trunc_s/f32")))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::I64TruncSF32,
                                     begin, cur_);
                if (consume(MOZ_UTF16("trunc_s/f64")))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::I64TruncSF64,
                                     begin, cur_);
                if (consume(MOZ_UTF16("trunc_u/f32")))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::I64TruncUF32,
                                     begin, cur_);
                if (consume(MOZ_UTF16("trunc_u/f64")))
                    return WasmToken(WasmToken::ConversionOpcode, Expr::I64TruncUF64,
                                     begin, cur_);
                break;
              case 'x':
                if (consume(MOZ_UTF16("xor")))
                    return WasmToken(WasmToken::BinaryOpcode, Expr::I64Xor, begin, cur_);
                break;
            }
            break;
        }
        if (consume(MOZ_UTF16("import")))
            return WasmToken(WasmToken::Import, begin, cur_);
        if (consume(MOZ_UTF16("infinity")))
            return WasmToken(WasmToken::Infinity, begin, cur_);
        if (consume(MOZ_UTF16("if"))) {
            if (consume(MOZ_UTF16("_else")))
                return WasmToken(WasmToken::IfElse, begin, cur_);
            return WasmToken(WasmToken::If, begin, cur_);
        }
        break;

      case 'l':
        if (consume(MOZ_UTF16("local")))
            return WasmToken(WasmToken::Local, begin, cur_);
        if (consume(MOZ_UTF16("loop")))
            return WasmToken(WasmToken::Loop, begin, cur_);
        break;

      case 'm':
        if (consume(MOZ_UTF16("module")))
            return WasmToken(WasmToken::Module, begin, cur_);
        if (consume(MOZ_UTF16("memory")))
            return WasmToken(WasmToken::Memory, begin, cur_);
        break;

      case 'n':
        if (consume(MOZ_UTF16("nan")))
            return nan(begin);
        if (consume(MOZ_UTF16("nop")))
            return WasmToken(WasmToken::Nop, begin, cur_);
        break;

      case 'o':
        if (consume(MOZ_UTF16("offset")))
            return WasmToken(WasmToken::Offset, begin, cur_);
        break;

      case 'p':
        if (consume(MOZ_UTF16("param")))
            return WasmToken(WasmToken::Param, begin, cur_);
        break;

      case 'r':
        if (consume(MOZ_UTF16("result")))
            return WasmToken(WasmToken::Result, begin, cur_);
        if (consume(MOZ_UTF16("return")))
            return WasmToken(WasmToken::Return, begin, cur_);
        break;

      case 's':
        if (consume(MOZ_UTF16("set_local")))
            return WasmToken(WasmToken::SetLocal, begin, cur_);
        if (consume(MOZ_UTF16("segment")))
            return WasmToken(WasmToken::Segment, begin, cur_);
        break;

      case 't':
        if (consume(MOZ_UTF16("table"))) {
            if (consume(MOZ_UTF16("switch")))
                return WasmToken(WasmToken::TableSwitch, begin, cur_);
            return WasmToken(WasmToken::Table, begin, cur_);
        }
        if (consume(MOZ_UTF16("type")))
            return WasmToken(WasmToken::Type, begin, cur_);
        break;

      default:
        break;
    }

    return fail(begin);
}

/*****************************************************************************/
// wasm text format parser

namespace {

struct WasmParseContext
{
    WasmTokenStream ts;
    LifoAlloc& lifo;
    UniqueChars* error;
    DtoaState* dtoaState;

    WasmParseContext(const char16_t* text, LifoAlloc& lifo, UniqueChars* error)
      : ts(text, error),
        lifo(lifo),
        error(error),
        dtoaState(NewDtoaState())
    {}

    bool fail(const char* message) {
        error->reset(JS_smprintf(message));
        return false;
    }
    ~WasmParseContext() {
        DestroyDtoaState(dtoaState);
    }
};

} // end anonymous namespace

static WasmAstExpr*
ParseExprInsideParens(WasmParseContext& c);

static WasmAstExpr*
ParseExpr(WasmParseContext& c)
{
    if (!c.ts.match(WasmToken::OpenParen, c.error))
        return nullptr;

    WasmAstExpr* expr = ParseExprInsideParens(c);
    if (!expr)
        return nullptr;

    if (!c.ts.match(WasmToken::CloseParen, c.error))
        return nullptr;

    return expr;
}

static WasmAstBlock*
ParseBlock(WasmParseContext& c, Expr expr)
{
    WasmAstExprVector exprs(c.lifo);

    WasmName breakName = c.ts.getIfName();

    WasmName continueName;
    if (expr == Expr::Loop)
        continueName = c.ts.getIfName();

    while (c.ts.getIf(WasmToken::OpenParen)) {
        WasmAstExpr* expr = ParseExprInsideParens(c);
        if (!expr || !exprs.append(expr))
            return nullptr;
        if (!c.ts.match(WasmToken::CloseParen, c.error))
            return nullptr;
    }

    return new(c.lifo) WasmAstBlock(expr, breakName, continueName, Move(exprs));
}

static WasmAstBranch*
ParseBranch(WasmParseContext& c, Expr expr)
{
    MOZ_ASSERT(expr == Expr::Br || expr == Expr::BrIf);

    WasmRef target;
    if (!c.ts.matchRef(&target, c.error))
        return nullptr;

    WasmAstExpr* cond = nullptr;
    if (expr == Expr::BrIf) {
        cond = ParseExpr(c);
        if (!cond)
            return nullptr;
    }

    return new(c.lifo) WasmAstBranch(expr, cond, target);
}

static bool
ParseArgs(WasmParseContext& c, WasmAstExprVector* args)
{
    while (c.ts.getIf(WasmToken::OpenParen)) {
        WasmAstExpr* arg = ParseExprInsideParens(c);
        if (!arg || !args->append(arg))
            return false;
        if (!c.ts.match(WasmToken::CloseParen, c.error))
            return false;
    }

    return true;
}

static WasmAstCall*
ParseCall(WasmParseContext& c, Expr expr)
{
    MOZ_ASSERT(expr == Expr::Call || expr == Expr::CallImport);

    WasmRef func;
    if (!c.ts.matchRef(&func, c.error))
        return nullptr;

    WasmAstExprVector args(c.lifo);
    if (!ParseArgs(c, &args))
        return nullptr;

    return new(c.lifo) WasmAstCall(expr, func, Move(args));
}

static WasmAstCallIndirect*
ParseCallIndirect(WasmParseContext& c)
{
    WasmRef sig;
    if (!c.ts.matchRef(&sig, c.error))
        return nullptr;

    WasmAstExpr* index = ParseExpr(c);
    if (!index)
        return nullptr;

    WasmAstExprVector args(c.lifo);
    if (!ParseArgs(c, &args))
        return nullptr;

    return new(c.lifo) WasmAstCallIndirect(sig, index, Move(args));
}

static uint_fast8_t
CountLeadingZeroes4(uint8_t x)
{
    MOZ_ASSERT((x & -0x10) == 0);
    return CountLeadingZeroes32(x) - 28;
}

template <typename T>
static T
ushl(T lhs, unsigned rhs)
{
    return rhs < sizeof(T) * CHAR_BIT ? (lhs << rhs) : 0;
}

template <typename T>
static T
ushr(T lhs, unsigned rhs)
{
    return rhs < sizeof(T) * CHAR_BIT ? (lhs >> rhs) : 0;
}

template<typename Float>
static bool
ParseNaNLiteral(const char16_t* cur, const char16_t* end, Float* result)
{
    MOZ_ALWAYS_TRUE(*cur++ == 'n' && *cur++ == 'a' && *cur++ == 'n');
    typedef FloatingPoint<Float> Traits;
    typedef typename Traits::Bits Bits;

    if (cur != end) {
        MOZ_ALWAYS_TRUE(*cur++ == ':' && *cur++ == '0' && *cur++ == 'x');
        if (cur == end)
            return false;
        CheckedInt<Bits> u = 0;
        do {
            uint8_t digit;
            MOZ_ALWAYS_TRUE(IsHexDigit(*cur, &digit));
            u *= 16;
            u += digit;
            cur++;
        } while (cur != end);
        if (!u.isValid())
            return false;
        Bits value = u.value();
        if ((value & ~Traits::kSignificandBits) != 0)
            return false;
        // NaN payloads must contain at least one set bit.
        if (value == 0)
            return false;
        *result = SpecificNaN<Float>(0, value);
    } else {
        // Produce the spec's default NaN.
        *result = SpecificNaN<Float>(0, (Traits::kSignificandBits + 1) >> 1);
    }
    return true;
}

template <typename Float>
static bool
ParseHexFloatLiteral(const char16_t* cur, const char16_t* end, Float* result)
{
    MOZ_ALWAYS_TRUE(*cur++ == '0' && *cur++ == 'x');
    typedef FloatingPoint<Float> Traits;
    typedef typename Traits::Bits Bits;
    static const unsigned numBits = sizeof(Float) * CHAR_BIT;
    static const Bits allOnes = ~Bits(0);
    static const Bits mostSignificantBit = ~(allOnes >> 1);

    // Significand part.
    Bits significand = 0;
    CheckedInt<int32_t> exponent = 0;
    bool sawFirstNonZero = false;
    bool discardedExtraNonZero = false;
    const char16_t* dot = nullptr;
    int significandPos;
    for (; cur != end; cur++) {
        if (*cur == '.') {
            MOZ_ASSERT(!dot);
            dot = cur;
            continue;
        }

        uint8_t digit;
        if (!IsHexDigit(*cur, &digit))
            break;
        if (!sawFirstNonZero) {
            if (digit == 0)
                continue;
            // We've located the first non-zero digit; we can now determine the
            // initial exponent. If we're after the dot, count the number of
            // zeros from the dot to here, and adjust for the number of leading
            // zero bits in the digit. Set up significandPos to put the first
            // nonzero at the most significant bit.
            int_fast8_t lz = CountLeadingZeroes4(digit);
            ptrdiff_t zeroAdjustValue = !dot ? 1 : dot + 1 - cur;
            CheckedInt<ptrdiff_t> zeroAdjust = zeroAdjustValue;
            zeroAdjust *= 4;
            zeroAdjust -= lz + 1;
            if (!zeroAdjust.isValid())
                return false;
            exponent = zeroAdjust.value();
            significandPos = numBits - (4 - lz);
            sawFirstNonZero = true;
        } else {
            // We've already seen a non-zero; just take 4 more bits.
            if (!dot)
                exponent += 4;
            if (significandPos > -4)
                significandPos -= 4;
        }

        // Or the newly parsed digit into significand at signicandPos.
        if (significandPos >= 0) {
            significand |= ushl(Bits(digit), significandPos);
        } else if (significandPos > -4) {
            significand |= ushr(digit, 4 - significandPos);
            discardedExtraNonZero = (digit & ~ushl(allOnes, 4 - significandPos)) != 0;
        } else if (digit != 0) {
            discardedExtraNonZero = true;
        }
    }

    // Exponent part.
    if (cur != end) {
        MOZ_ALWAYS_TRUE(*cur++ == 'p');
        bool isNegated = false;
        if (cur != end && (*cur == '-' || *cur == '+'))
            isNegated = *cur++ == '-';
        CheckedInt<int32_t> parsedExponent = 0;
        while (cur != end && IsWasmDigit(*cur))
            parsedExponent = parsedExponent * 10 + (*cur++ - '0');
        if (isNegated)
            parsedExponent = -parsedExponent;
        exponent += parsedExponent;
    }

    MOZ_ASSERT(cur == end);
    if (!exponent.isValid())
        return false;

    // Create preliminary exponent and significand encodings of the results.
    Bits encodedExponent, encodedSignificand, discardedSignificandBits;
    if (significand == 0) {
        // Zero. The exponent is encoded non-biased.
        encodedExponent = 0;
        encodedSignificand = 0;
        discardedSignificandBits = 0;
    } else if (MOZ_UNLIKELY(exponent.value() <= int32_t(-Traits::kExponentBias))) {
        // Underflow to subnormal or zero.
        encodedExponent = 0;
        encodedSignificand = ushr(significand,
                                  numBits - Traits::kExponentShift -
                                  exponent.value() - Traits::kExponentBias);
        discardedSignificandBits =
            ushl(significand,
                 Traits::kExponentShift + exponent.value() + Traits::kExponentBias);
    } else if (MOZ_LIKELY(exponent.value() <= int32_t(Traits::kExponentBias))) {
        // Normal (non-zero). The significand's leading 1 is encoded implicitly.
        encodedExponent = (Bits(exponent.value()) + Traits::kExponentBias) <<
                          Traits::kExponentShift;
        MOZ_ASSERT(significand & mostSignificantBit);
        encodedSignificand = ushr(significand, numBits - Traits::kExponentShift - 1) &
                             Traits::kSignificandBits;
        discardedSignificandBits = ushl(significand, Traits::kExponentShift + 1);
    } else {
        // Overflow to infinity.
        encodedExponent = Traits::kExponentBits;
        encodedSignificand = 0;
        discardedSignificandBits = 0;
    }
    MOZ_ASSERT((encodedExponent & ~Traits::kExponentBits) == 0);
    MOZ_ASSERT((encodedSignificand & ~Traits::kSignificandBits) == 0);
    MOZ_ASSERT(encodedExponent != Traits::kExponentBits || encodedSignificand == 0);
    Bits bits = encodedExponent | encodedSignificand;

    // Apply rounding. If this overflows the significand, it carries into the
    // exponent bit according to the magic of the IEEE 754 encoding.
    bits += (discardedSignificandBits & mostSignificantBit) &&
            ((discardedSignificandBits & ~mostSignificantBit) ||
             discardedExtraNonZero ||
             // ties to even
             (encodedSignificand & 1));

    *result = BitwiseCast<Float>(bits);
    return true;
}

template <typename Float>
static bool
ParseFloatLiteral(WasmParseContext& c, WasmToken token, Float* result)
{
    switch (token.kind()) {
      case WasmToken::Index:
        *result = token.index();
        return true;
      case WasmToken::UnsignedInteger:
        *result = token.uint();
        return true;
      case WasmToken::SignedInteger:
        *result = token.sint();
        return true;
      case WasmToken::Float:
        break;
      default:
        c.ts.generateError(token, c.error);
        return false;
    }

    const char16_t* begin = token.begin();
    const char16_t* end = token.end();
    const char16_t* cur = begin;

    bool isNegated = false;
    if (*cur == '-' || *cur == '+')
        isNegated = *cur++ == '-';

    switch (token.floatLiteralKind()) {
      case WasmToken::Infinity:
        *result = PositiveInfinity<Float>();
        break;
      case WasmToken::NaN:
        if (!ParseNaNLiteral(cur, end, result)) {
            c.ts.generateError(token, c.error);
            return false;
        }
        break;
      case WasmToken::HexNumber:
        if (!ParseHexFloatLiteral(cur, end, result)) {
            c.ts.generateError(token, c.error);
            return false;
        }
        break;
      case WasmToken::DecNumber: {
        // Call into JS' strtod. Tokenization has already required that the
        // string is well-behaved.
        LifoAlloc::Mark mark = c.lifo.mark();
        char* buffer = c.lifo.newArray<char>(end - begin + 1);
        if (!buffer)
            return false;
        for (ptrdiff_t i = 0; i < end - cur; ++i)
            buffer[i] = char(cur[i]);
        char* strtod_end;
        int err;
        Float d = (Float)js_strtod_harder(c.dtoaState, buffer, &strtod_end, &err);
        if (err != 0 || strtod_end == buffer) {
            c.lifo.release(mark);
            c.ts.generateError(token, c.error);
            return false;
        }
        c.lifo.release(mark);
        *result = d;
        break;
      }
    }

    if (isNegated)
        *result = -*result;

    return true;
}

static WasmAstConst*
ParseConst(WasmParseContext& c, WasmToken constToken)
{
    WasmToken val = c.ts.get();
    switch (constToken.valueType()) {
      case ValType::I32: {
        switch (val.kind()) {
          case WasmToken::Index:
            return new(c.lifo) WasmAstConst(Val(val.index()));
          case WasmToken::SignedInteger: {
            CheckedInt<int32_t> sint = val.sint();
            if (!sint.isValid())
                break;
            return new(c.lifo) WasmAstConst(Val(uint32_t(sint.value())));
          }
          default:
            break;
        }
        break;
      }
      case ValType::I64: {
        switch (val.kind()) {
          case WasmToken::Index:
            return new(c.lifo) WasmAstConst(Val(uint64_t(val.index())));
          case WasmToken::UnsignedInteger:
            return new(c.lifo) WasmAstConst(Val(val.uint()));
          case WasmToken::SignedInteger:
            return new(c.lifo) WasmAstConst(Val(uint64_t(val.sint())));
          default:
            break;
        }
        break;
      }
      case ValType::F32: {
        float result;
        if (!ParseFloatLiteral(c, val, &result))
            break;
        return new(c.lifo) WasmAstConst(Val(result));
      }
      case ValType::F64: {
        double result;
        if (!ParseFloatLiteral(c, val, &result))
            break;
        return new(c.lifo) WasmAstConst(Val(result));
      }
      default:
        break;
    }
    c.ts.generateError(constToken, c.error);
    return nullptr;
}

static WasmAstGetLocal*
ParseGetLocal(WasmParseContext& c)
{
    WasmRef local;
    if (!c.ts.matchRef(&local, c.error))
        return nullptr;

    return new(c.lifo) WasmAstGetLocal(local);
}

static WasmAstSetLocal*
ParseSetLocal(WasmParseContext& c)
{
    WasmRef local;
    if (!c.ts.matchRef(&local, c.error))
        return nullptr;

    WasmAstExpr* value = ParseExpr(c);
    if (!value)
        return nullptr;

    return new(c.lifo) WasmAstSetLocal(local, *value);
}

static WasmAstReturn*
ParseReturn(WasmParseContext& c)
{
    WasmAstExpr* maybeExpr = nullptr;

    if (c.ts.peek().kind() != WasmToken::CloseParen) {
        maybeExpr = ParseExpr(c);
        if (!maybeExpr)
            return nullptr;
    }

    return new(c.lifo) WasmAstReturn(maybeExpr);
}

static WasmAstUnaryOperator*
ParseUnaryOperator(WasmParseContext& c, Expr expr)
{
    WasmAstExpr* op = ParseExpr(c);
    if (!op)
        return nullptr;

    return new(c.lifo) WasmAstUnaryOperator(expr, op);
}

static WasmAstBinaryOperator*
ParseBinaryOperator(WasmParseContext& c, Expr expr)
{
    WasmAstExpr* lhs = ParseExpr(c);
    if (!lhs)
        return nullptr;

    WasmAstExpr* rhs = ParseExpr(c);
    if (!rhs)
        return nullptr;

    return new(c.lifo) WasmAstBinaryOperator(expr, lhs, rhs);
}

static WasmAstComparisonOperator*
ParseComparisonOperator(WasmParseContext& c, Expr expr)
{
    WasmAstExpr* lhs = ParseExpr(c);
    if (!lhs)
        return nullptr;

    WasmAstExpr* rhs = ParseExpr(c);
    if (!rhs)
        return nullptr;

    return new(c.lifo) WasmAstComparisonOperator(expr, lhs, rhs);
}

static WasmAstConversionOperator*
ParseConversionOperator(WasmParseContext& c, Expr expr)
{
    WasmAstExpr* op = ParseExpr(c);
    if (!op)
        return nullptr;

    return new(c.lifo) WasmAstConversionOperator(expr, op);
}

static WasmAstIfElse*
ParseIfElse(WasmParseContext& c, Expr expr)
{
    WasmAstExpr* cond = ParseExpr(c);
    if (!cond)
        return nullptr;

    WasmAstExpr* ifBody = ParseExpr(c);
    if (!ifBody)
        return nullptr;

    WasmAstExpr* elseBody = nullptr;
    if (expr == Expr::IfElse) {
        elseBody = ParseExpr(c);
        if (!elseBody)
            return nullptr;
    }

    return new(c.lifo) WasmAstIfElse(expr, cond, ifBody, elseBody);
}

static bool
ParseLoadStoreAddress(WasmParseContext& c, int32_t* offset, int32_t* align, WasmAstExpr** base)
{
    *offset = 0;
    if (c.ts.getIf(WasmToken::Offset)) {
        if (!c.ts.match(WasmToken::Equal, c.error))
            return false;
        WasmToken val = c.ts.get();
        switch (val.kind()) {
          case WasmToken::Index:
            *offset = val.index();
            break;
          default:
            c.ts.generateError(val, c.error);
            return false;
        }
    }

    *align = 0;
    if (c.ts.getIf(WasmToken::Align)) {
        if (!c.ts.match(WasmToken::Equal, c.error))
            return false;
        WasmToken val = c.ts.get();
        switch (val.kind()) {
          case WasmToken::Index:
            *align = val.index();
            break;
          default:
            c.ts.generateError(val, c.error);
            return false;
        }
    }

    *base = ParseExpr(c);
    if (!*base)
        return false;

    return true;
}

static WasmAstLoad*
ParseLoad(WasmParseContext& c, Expr expr)
{
    int32_t offset;
    int32_t align;
    WasmAstExpr* base;
    if (!ParseLoadStoreAddress(c, &offset, &align, &base))
        return nullptr;

    if (align == 0) {
        switch (expr) {
          case Expr::I32LoadMem8S:
          case Expr::I32LoadMem8U:
          case Expr::I64LoadMem8S:
          case Expr::I64LoadMem8U:
            align = 1;
            break;
          case Expr::I32LoadMem16S:
          case Expr::I32LoadMem16U:
          case Expr::I64LoadMem16S:
          case Expr::I64LoadMem16U:
            align = 2;
            break;
          case Expr::I32LoadMem:
          case Expr::F32LoadMem:
          case Expr::I64LoadMem32S:
          case Expr::I64LoadMem32U:
            align = 4;
            break;
          case Expr::I64LoadMem:
          case Expr::F64LoadMem:
            align = 8;
            break;
          default:
            MOZ_CRASH("Bad load expr");
        }
    }

    return new(c.lifo) WasmAstLoad(expr, WasmAstLoadStoreAddress(base, offset, align));
}

static WasmAstStore*
ParseStore(WasmParseContext& c, Expr expr)
{
    int32_t offset;
    int32_t align;
    WasmAstExpr* base;
    if (!ParseLoadStoreAddress(c, &offset, &align, &base))
        return nullptr;

    if (align == 0) {
        switch (expr) {
          case Expr::I32StoreMem8:
          case Expr::I64StoreMem8:
            align = 1;
            break;
          case Expr::I32StoreMem16:
          case Expr::I64StoreMem16:
            align = 2;
            break;
          case Expr::I32StoreMem:
          case Expr::F32StoreMem:
          case Expr::I64StoreMem32:
            align = 4;
            break;
          case Expr::I64StoreMem:
          case Expr::F64StoreMem:
            align = 8;
            break;
          default:
            MOZ_CRASH("Bad load expr");
        }
    }

    WasmAstExpr* value = ParseExpr(c);
    if (!value)
        return nullptr;

    return new(c.lifo) WasmAstStore(expr, WasmAstLoadStoreAddress(base, offset, align), value);
}

static WasmAstTableSwitch*
ParseTableSwitch(WasmParseContext& c)
{
    WasmAstExpr* index = ParseExpr(c);
    if (!index)
        return nullptr;

    if (!c.ts.match(WasmToken::OpenParen, c.error))
        return nullptr;
    if (!c.ts.match(WasmToken::Table, c.error))
        return nullptr;

    WasmRefVector table(c.lifo);

    while (c.ts.getIf(WasmToken::OpenParen)) {
        if (!c.ts.match(WasmToken::Br, c.error))
            return nullptr;

        WasmRef target;
        if (!c.ts.matchRef(&target, c.error))
            return nullptr;

        if (!table.append(target))
            return nullptr;

        if (!c.ts.match(WasmToken::CloseParen, c.error))
            return nullptr;
    }

    if (!c.ts.match(WasmToken::CloseParen, c.error))
        return nullptr;

    if (!c.ts.match(WasmToken::OpenParen, c.error))
        return nullptr;
    if (!c.ts.match(WasmToken::Br, c.error))
        return nullptr;

    WasmRef def;
    if (!c.ts.matchRef(&def, c.error))
        return nullptr;

    if (!c.ts.match(WasmToken::CloseParen, c.error))
        return nullptr;

    return new(c.lifo) WasmAstTableSwitch(*index, def, Move(table));
}

static WasmAstExpr*
ParseExprInsideParens(WasmParseContext& c)
{
    WasmToken token = c.ts.get();

    switch (token.kind()) {
      case WasmToken::Nop:
        return new(c.lifo) WasmAstNop;
      case WasmToken::BinaryOpcode:
        return ParseBinaryOperator(c, token.expr());
      case WasmToken::Block:
        return ParseBlock(c, Expr::Block);
      case WasmToken::Br:
        return ParseBranch(c, Expr::Br);
      case WasmToken::BrIf:
        return ParseBranch(c, Expr::BrIf);
      case WasmToken::Call:
        return ParseCall(c, Expr::Call);
      case WasmToken::CallImport:
        return ParseCall(c, Expr::CallImport);
      case WasmToken::CallIndirect:
        return ParseCallIndirect(c);
      case WasmToken::ComparisonOpcode:
        return ParseComparisonOperator(c, token.expr());
      case WasmToken::Const:
        return ParseConst(c, token);
      case WasmToken::ConversionOpcode:
        return ParseConversionOperator(c, token.expr());
      case WasmToken::If:
        return ParseIfElse(c, Expr::If);
      case WasmToken::IfElse:
        return ParseIfElse(c, Expr::IfElse);
      case WasmToken::GetLocal:
        return ParseGetLocal(c);
      case WasmToken::Load:
        return ParseLoad(c, token.expr());
      case WasmToken::Loop:
        return ParseBlock(c, Expr::Loop);
      case WasmToken::Return:
        return ParseReturn(c);
      case WasmToken::SetLocal:
        return ParseSetLocal(c);
      case WasmToken::Store:
        return ParseStore(c, token.expr());
      case WasmToken::TableSwitch:
        return ParseTableSwitch(c);
      case WasmToken::UnaryOpcode:
        return ParseUnaryOperator(c, token.expr());
      default:
        c.ts.generateError(token, c.error);
        return nullptr;
    }
}

static bool
ParseValueType(WasmParseContext& c, WasmAstValTypeVector* vec)
{
    WasmToken token;
    return c.ts.match(WasmToken::ValueType, &token, c.error) &&
           vec->append(token.valueType());
}

static bool
ParseValueTypeList(WasmParseContext& c, WasmAstValTypeVector* vec)
{
    WasmToken token;
    while (c.ts.getIf(WasmToken::ValueType, &token)) {
        if (!vec->append(token.valueType()))
            return false;
    }

    return true;
}

static bool
ParseResult(WasmParseContext& c, ExprType* result)
{
    if (*result != ExprType::Void) {
        c.ts.generateError(c.ts.peek(), c.error);
        return false;
    }

    WasmToken token;
    if (!c.ts.match(WasmToken::ValueType, &token, c.error))
        return false;

    *result = ToExprType(token.valueType());
    return true;
}

static bool
ParseLocal(WasmParseContext& c, WasmNameVector* locals, WasmAstValTypeVector* localTypes)
{
    return locals->append(c.ts.getIfName()) &&
           ParseValueType(c, localTypes);
}

static bool
ParseParam(WasmParseContext& c, WasmNameVector* locals, WasmAstValTypeVector* args)
{
    if (c.ts.peek().kind() == WasmToken::Name)
        return ParseLocal(c, locals, args);

    return locals->append(WasmName()) &&
           ParseValueTypeList(c, args);
}

static WasmAstFunc*
ParseFunc(WasmParseContext& c, WasmAstModule* module)
{
    WasmAstValTypeVector vars(c.lifo);
    WasmAstValTypeVector args(c.lifo);
    WasmNameVector locals(c.lifo);

    WasmName funcName = c.ts.getIfName();

    WasmRef sig;

    WasmToken openParen;
    if (c.ts.getIf(WasmToken::OpenParen, &openParen)) {
        if (c.ts.getIf(WasmToken::Type)) {
            if (!c.ts.matchRef(&sig, c.error))
                return nullptr;
            if (!c.ts.match(WasmToken::CloseParen, c.error))
                return nullptr;
        } else {
            c.ts.unget(openParen);
        }
    }

    WasmAstExprVector body(c.lifo);
    ExprType result = ExprType::Void;

    while (c.ts.getIf(WasmToken::OpenParen)) {
        WasmToken token = c.ts.get();
        switch (token.kind()) {
          case WasmToken::Local:
            if (!ParseLocal(c, &locals, &vars))
                return nullptr;
            break;
          case WasmToken::Param:
            if (!vars.empty()) {
                c.ts.generateError(token, c.error);
                return nullptr;
            }
            if (!ParseParam(c, &locals, &args))
                return nullptr;
            break;
          case WasmToken::Result:
            if (!ParseResult(c, &result))
                return nullptr;
            break;
          default:
            c.ts.unget(token);
            WasmAstExpr* expr = ParseExprInsideParens(c);
            if (!expr || !body.append(expr))
                return nullptr;
            break;
        }
        if (!c.ts.match(WasmToken::CloseParen, c.error))
            return nullptr;
    }

    if (sig.isInvalid()) {
        uint32_t sigIndex;
        if (!module->declare(WasmAstSig(Move(args), result), &sigIndex))
            return nullptr;
        sig.setIndex(sigIndex);
    }

    return new(c.lifo) WasmAstFunc(funcName, sig, Move(vars), Move(locals), Move(body));
}

static bool
ParseFuncType(WasmParseContext& c, WasmAstSig* sig)
{
    WasmAstValTypeVector args(c.lifo);
    ExprType result = ExprType::Void;

    while (c.ts.getIf(WasmToken::OpenParen)) {
        WasmToken token = c.ts.get();
        switch (token.kind()) {
          case WasmToken::Param:
            if (!ParseValueTypeList(c, &args))
                return false;
            break;
          case WasmToken::Result:
            if (!ParseResult(c, &result))
                return false;
            break;
          default:
            c.ts.generateError(token, c.error);
            return false;
        }
        if (!c.ts.match(WasmToken::CloseParen, c.error))
            return false;
    }

    *sig = WasmAstSig(Move(args), result);
    return true;
}

static WasmAstSig*
ParseTypeDef(WasmParseContext& c)
{
    WasmName name = c.ts.getIfName();

    if (!c.ts.match(WasmToken::OpenParen, c.error))
        return nullptr;
    if (!c.ts.match(WasmToken::Func, c.error))
        return nullptr;

    WasmAstSig sig(c.lifo);
    if (!ParseFuncType(c, &sig))
        return nullptr;

    if (!c.ts.match(WasmToken::CloseParen, c.error))
        return nullptr;

    return new(c.lifo) WasmAstSig(name, Move(sig));
}

static WasmAstSegment*
ParseSegment(WasmParseContext& c)
{
    if (!c.ts.match(WasmToken::Segment, c.error))
        return nullptr;

    WasmToken dstOffset;
    if (!c.ts.match(WasmToken::Index, &dstOffset, c.error))
        return nullptr;

    WasmToken text;
    if (!c.ts.match(WasmToken::Text, &text, c.error))
        return nullptr;

    return new(c.lifo) WasmAstSegment(dstOffset.index(), text.text());
}

static WasmAstMemory*
ParseMemory(WasmParseContext& c)
{
    WasmToken initialSize;
    if (!c.ts.match(WasmToken::Index, &initialSize, c.error))
        return nullptr;

    WasmAstSegmentVector segments(c.lifo);
    while (c.ts.getIf(WasmToken::OpenParen)) {
        WasmAstSegment* segment = ParseSegment(c);
        if (!segment || !segments.append(segment))
            return nullptr;
        if (!c.ts.match(WasmToken::CloseParen, c.error))
            return nullptr;
    }

    return new(c.lifo) WasmAstMemory(initialSize.index(), Move(segments));
}

static WasmAstImport*
ParseImport(WasmParseContext& c, WasmAstModule* module)
{
    WasmName name = c.ts.getIfName();

    WasmToken moduleName;
    if (!c.ts.match(WasmToken::Text, &moduleName, c.error))
        return nullptr;

    WasmToken funcName;
    if (!c.ts.match(WasmToken::Text, &funcName, c.error))
        return nullptr;

    WasmAstSig sig(c.lifo);
    if (!ParseFuncType(c, &sig))
        return nullptr;

    uint32_t sigIndex;
    if (!module->declare(Move(sig), &sigIndex))
        return nullptr;

    return new(c.lifo) WasmAstImport(name, moduleName.text(), funcName.text(), sigIndex);
}

static WasmAstExport*
ParseExport(WasmParseContext& c)
{
    WasmToken name;
    if (!c.ts.match(WasmToken::Text, &name, c.error))
        return nullptr;

    WasmToken exportee = c.ts.get();
    switch (exportee.kind()) {
      case WasmToken::Index:
        return new(c.lifo) WasmAstExport(name.text(), WasmRef(WasmName(), exportee.index()));
      case WasmToken::Name:
        return new(c.lifo) WasmAstExport(name.text(), WasmRef(exportee.name(), WasmNoIndex));
      case WasmToken::Memory:
        return new(c.lifo) WasmAstExport(name.text());
      default:
        break;
    }

    c.ts.generateError(exportee, c.error);
    return nullptr;

}

static WasmAstTable*
ParseTable(WasmParseContext& c)
{
    WasmAstTableElemVector elems(c.lifo);

    WasmRef elem;
    while (c.ts.getIfRef(&elem)) {
        if (!elems.append(elem))
            return nullptr;
    }

    return new(c.lifo) WasmAstTable(Move(elems));
}

static WasmAstModule*
ParseModule(const char16_t* text, LifoAlloc& lifo, UniqueChars* error)
{
    WasmParseContext c(text, lifo, error);

    if (!c.ts.match(WasmToken::OpenParen, c.error))
        return nullptr;
    if (!c.ts.match(WasmToken::Module, c.error))
        return nullptr;

    auto module = new(c.lifo) WasmAstModule(c.lifo);
    if (!module || !module->init())
        return nullptr;

    while (c.ts.getIf(WasmToken::OpenParen)) {
        WasmToken section = c.ts.get();

        switch (section.kind()) {
          case WasmToken::Type: {
            WasmAstSig* sig = ParseTypeDef(c);
            if (!sig || !module->append(sig))
                return nullptr;
            break;
          }
          case WasmToken::Memory: {
            WasmAstMemory* memory = ParseMemory(c);
            if (!memory)
                return nullptr;
            if (!module->setMemory(memory)) {
                c.ts.generateError(section, c.error);
                return nullptr;
            }
            break;
          }
          case WasmToken::Import: {
            WasmAstImport* imp = ParseImport(c, module);
            if (!imp || !module->append(imp))
                return nullptr;
            break;
          }
          case WasmToken::Export: {
            WasmAstExport* exp = ParseExport(c);
            if (!exp || !module->append(exp))
                return nullptr;
            break;
          }
          case WasmToken::Table: {
            WasmAstTable* table = ParseTable(c);
            if (!table)
                return nullptr;
            if (!module->initTable(table)) {
                c.ts.generateError(section, c.error);
                return nullptr;
            }
            break;
          }
          case WasmToken::Func: {
            WasmAstFunc* func = ParseFunc(c, module);
            if (!func || !module->append(func))
                return nullptr;
            break;
          }
          default:
            c.ts.generateError(section, c.error);
            return nullptr;
        }

        if (!c.ts.match(WasmToken::CloseParen, c.error))
            return nullptr;
    }

    if (!c.ts.match(WasmToken::CloseParen, c.error))
        return nullptr;
    if (!c.ts.match(WasmToken::EndOfFile, c.error))
        return nullptr;

    return module;
}

/*****************************************************************************/
// wasm name resolution

namespace {

class Resolver
{
    UniqueChars* error_;
    WasmNameMap varMap_;
    WasmNameMap sigMap_;
    WasmNameMap funcMap_;
    WasmNameMap importMap_;
    WasmNameVector targetStack_;

    bool registerName(WasmNameMap& map, WasmName name, size_t index) {
        WasmNameMap::AddPtr p = map.lookupForAdd(name);
        if (!p) {
            if (!map.add(p, name, index))
                return false;
        } else {
            return false;
        }
        return true;
    }
    bool resolveName(WasmNameMap& map, WasmName name, size_t* index) {
        WasmNameMap::Ptr p = map.lookup(name);
        if (p) {
            *index = p->value();
            return true;
        }
        return false;
    }
    bool resolveRef(WasmNameMap& map, WasmRef& ref) {
        WasmNameMap::Ptr p = map.lookup(ref.name());
        if (p) {
            ref.setIndex(p->value());
            return true;
        }
        return false;
    }

  public:
    explicit Resolver(LifoAlloc& lifo, UniqueChars* error)
      : error_(error),
        varMap_(lifo),
        sigMap_(lifo),
        funcMap_(lifo),
        importMap_(lifo),
        targetStack_(lifo)
    {}
    bool init() {
        return sigMap_.init() && funcMap_.init() && importMap_.init() && varMap_.init();
    }
    void beginFunc() {
        varMap_.clear();
        MOZ_ASSERT(targetStack_.empty());
    }
    bool registerSigName(WasmName name, size_t index) {
        return name.empty() || registerName(sigMap_, name, index);
    }
    bool registerFuncName(WasmName name, size_t index) {
        return name.empty() || registerName(funcMap_, name, index);
    }
    bool registerImportName(WasmName name, size_t index) {
        return name.empty() || registerName(importMap_, name, index);
    }
    bool registerVarName(WasmName name, size_t index) {
        return name.empty() || registerName(varMap_, name, index);
    }
    bool pushTarget(WasmName name) {
        return targetStack_.append(name);
    }
    void popTarget(WasmName name) {
        MOZ_ASSERT(targetStack_.back() == name);
        targetStack_.popBack();
    }
    bool resolveSigRef(WasmRef& ref) {
        return ref.name().empty() || resolveRef(sigMap_, ref);
    }
    bool resolveFuncRef(WasmRef& ref) {
        return ref.name().empty() || resolveRef(funcMap_, ref);
    }
    bool resolveImportRef(WasmRef& ref) {
        return ref.name().empty() || resolveRef(importMap_, ref);
    }
    bool resolveVarRef(WasmRef& ref) {
        return ref.name().empty() || resolveRef(varMap_, ref);
    }
    bool resolveTarget(WasmRef& ref) {
        for (size_t i = targetStack_.length(); i > 0; i--) {
            uint32_t targetIndex = i - 1;
            if (targetStack_[targetIndex] == ref.name()) {
                ref.setIndex(targetIndex);
                return true;
            }
        }
        return false;
    }
    bool fail(const char*message) {
        error_->reset(JS_smprintf("%s", message));
        return false;
    }
};

} // end anonymous namespace

static bool
ResolveExpr(Resolver& r, WasmAstExpr& expr);

static bool
ResolveBlock(Resolver& r, WasmAstBlock& b)
{
    if (!r.pushTarget(b.breakName()))
        return false;

    if (b.expr() == Expr::Loop) {
        if (!r.pushTarget(b.continueName()))
            return false;
    }

    size_t numExprs = b.exprs().length();
    for (size_t i = 0; i < numExprs; i++) {
        if (!ResolveExpr(r, *b.exprs()[i]))
            return false;
    }

    if (b.expr() == Expr::Loop)
        r.popTarget(b.continueName());
    r.popTarget(b.breakName());
    return true;
}

static bool
ResolveBranch(Resolver& r, WasmAstBranch& br)
{
    if (!br.target().name().empty() && !r.resolveTarget(br.target()))
        return r.fail("label not found");

    if (br.expr() == Expr::BrIf) {
        if (!ResolveExpr(r, br.cond()))
            return false;
    }

    return true;
}

static bool
ResolveArgs(Resolver& r, const WasmAstExprVector& args)
{
    for (WasmAstExpr* arg : args) {
        if (!ResolveExpr(r, *arg))
            return false;
    }

    return true;
}

static bool
ResolveCall(Resolver& r, WasmAstCall& c)
{
    if (!ResolveArgs(r, c.args()))
        return false;

    if (c.expr() == Expr::Call) {
        if (!r.resolveFuncRef(c.func()))
            return r.fail("function not found");
    } else {
        MOZ_ASSERT(c.expr() == Expr::CallImport);
        if (!r.resolveImportRef(c.func()))
            return r.fail("import not found");
    }

    return true;
}

static bool
ResolveCallIndirect(Resolver& r, WasmAstCallIndirect& c)
{
    if (!ResolveExpr(r, *c.index()))
        return false;

    if (!ResolveArgs(r, c.args()))
        return false;

    if (!r.resolveSigRef(c.sig()))
        return r.fail("signature not found");

    return true;
}

static bool
ResolveGetLocal(Resolver& r, WasmAstGetLocal& gl)
{
    if (!r.resolveVarRef(gl.local()))
        return r.fail("local not found");

    return true;
}

static bool
ResolveSetLocal(Resolver& r, WasmAstSetLocal& sl)
{
    if (!ResolveExpr(r, sl.value()))
        return false;

    if (!r.resolveVarRef(sl.local()))
        return r.fail("local not found");

    return true;
}

static bool
ResolveUnaryOperator(Resolver& r, WasmAstUnaryOperator& b)
{
    return ResolveExpr(r, *b.op());
}

static bool
ResolveBinaryOperator(Resolver& r, WasmAstBinaryOperator& b)
{
    return ResolveExpr(r, *b.lhs()) &&
           ResolveExpr(r, *b.rhs());
}

static bool
ResolveComparisonOperator(Resolver& r, WasmAstComparisonOperator& b)
{
    return ResolveExpr(r, *b.lhs()) &&
           ResolveExpr(r, *b.rhs());
}

static bool
ResolveConversionOperator(Resolver& r, WasmAstConversionOperator& b)
{
    return ResolveExpr(r, *b.op());
}

static bool
ResolveIfElse(Resolver& r, WasmAstIfElse& ie)
{
    return ResolveExpr(r, ie.cond()) &&
           ResolveExpr(r, ie.ifBody()) &&
           (!ie.hasElse() || ResolveExpr(r, ie.elseBody()));
}

static bool
ResolveLoadStoreAddress(Resolver& r, const WasmAstLoadStoreAddress &address)
{
    return ResolveExpr(r, address.base());
}

static bool
ResolveLoad(Resolver& r, WasmAstLoad& l)
{
    return ResolveLoadStoreAddress(r, l.address());
}

static bool
ResolveStore(Resolver& r, WasmAstStore& s)
{
    return ResolveLoadStoreAddress(r, s.address()) &&
           ResolveExpr(r, s.value());
}

static bool
ResolveReturn(Resolver& r, WasmAstReturn& ret)
{
    return !ret.maybeExpr() || ResolveExpr(r, *ret.maybeExpr());
}

static bool
ResolveTableSwitch(Resolver& r, WasmAstTableSwitch& ts)
{
    if (!ts.def().name().empty() && !r.resolveTarget(ts.def()))
        return r.fail("switch default not found");

    for (WasmRef& elem : ts.table()) {
        if (!elem.name().empty() && !r.resolveTarget(elem))
            return r.fail("switch element not found");
    }

    return ResolveExpr(r, ts.index());
}

static bool
ResolveExpr(Resolver& r, WasmAstExpr& expr)
{
    switch (expr.kind()) {
      case WasmAstExprKind::Nop:
        return true;
      case WasmAstExprKind::BinaryOperator:
        return ResolveBinaryOperator(r, expr.as<WasmAstBinaryOperator>());
      case WasmAstExprKind::Block:
        return ResolveBlock(r, expr.as<WasmAstBlock>());
      case WasmAstExprKind::Branch:
        return ResolveBranch(r, expr.as<WasmAstBranch>());
      case WasmAstExprKind::Call:
        return ResolveCall(r, expr.as<WasmAstCall>());
      case WasmAstExprKind::CallIndirect:
        return ResolveCallIndirect(r, expr.as<WasmAstCallIndirect>());
      case WasmAstExprKind::ComparisonOperator:
        return ResolveComparisonOperator(r, expr.as<WasmAstComparisonOperator>());
      case WasmAstExprKind::Const:
        return true;
      case WasmAstExprKind::ConversionOperator:
        return ResolveConversionOperator(r, expr.as<WasmAstConversionOperator>());
      case WasmAstExprKind::GetLocal:
        return ResolveGetLocal(r, expr.as<WasmAstGetLocal>());
      case WasmAstExprKind::IfElse:
        return ResolveIfElse(r, expr.as<WasmAstIfElse>());
      case WasmAstExprKind::Load:
        return ResolveLoad(r, expr.as<WasmAstLoad>());
      case WasmAstExprKind::Return:
        return ResolveReturn(r, expr.as<WasmAstReturn>());
      case WasmAstExprKind::SetLocal:
        return ResolveSetLocal(r, expr.as<WasmAstSetLocal>());
      case WasmAstExprKind::Store:
        return ResolveStore(r, expr.as<WasmAstStore>());
      case WasmAstExprKind::TableSwitch:
        return ResolveTableSwitch(r, expr.as<WasmAstTableSwitch>());
      case WasmAstExprKind::UnaryOperator:
        return ResolveUnaryOperator(r, expr.as<WasmAstUnaryOperator>());
    }
    MOZ_CRASH("Bad expr kind");
}

static bool
ResolveFunc(Resolver& r, WasmAstFunc& func)
{
    r.beginFunc();

    size_t numVars = func.locals().length();
    for (size_t i = 0; i < numVars; i++) {
        if (!r.registerVarName(func.locals()[i], i))
            return r.fail("duplicate var");
    }

    for (WasmAstExpr* expr : func.body()) {
        if (!ResolveExpr(r, *expr))
            return false;
    }
    return true;
}

static bool
ResolveModule(LifoAlloc& lifo, WasmAstModule* module, UniqueChars* error)
{
    Resolver r(lifo, error);

    if (!r.init())
        return false;

    size_t numSigs = module->sigs().length();
    for (size_t i = 0; i < numSigs; i++) {
        WasmAstSig* sig = module->sigs()[i];
        if (!r.registerSigName(sig->name(), i))
            return r.fail("duplicate signature");
    }

    size_t numFuncs = module->funcs().length();
    for (size_t i = 0; i < numFuncs; i++) {
        WasmAstFunc* func = module->funcs()[i];
        if (!r.resolveSigRef(func->sig()))
            return r.fail("signature not found");
        if (!r.registerFuncName(func->name(), i))
            return r.fail("duplicate function");
    }

    if (module->maybeTable()) {
        for (WasmRef& ref : module->maybeTable()->elems()) {
            if (!r.resolveFuncRef(ref))
                return r.fail("function not found");
        }
    }

    size_t numImports = module->imports().length();
    for (size_t i = 0; i < numImports; i++) {
        WasmAstImport* imp = module->imports()[i];
        if (!r.registerImportName(imp->name(), i))
            return r.fail("duplicate import");
    }

    for (WasmAstExport* export_ : module->exports()) {
        if (export_->kind() != WasmAstExportKind::Func)
            continue;
        if (!r.resolveFuncRef(export_->func()))
            return r.fail("function not found");
    }

    for (WasmAstFunc* func : module->funcs()) {
        if (!ResolveFunc(r, *func))
            return false;
    }

    return true;
}

/*****************************************************************************/
// wasm function body serialization

static bool
EncodeExpr(Encoder& e, WasmAstExpr& expr);

static bool
EncodeBlock(Encoder& e, WasmAstBlock& b)
{
    if (!e.writeExpr(Expr::Block))
        return false;

    size_t numExprs = b.exprs().length();
    if (!e.writeVarU32(numExprs))
        return false;

    for (size_t i = 0; i < numExprs; i++) {
        if (!EncodeExpr(e, *b.exprs()[i]))
            return false;
    }

    return true;
}

static bool
EncodeBranch(Encoder& e, WasmAstBranch& br)
{
    if (!e.writeExpr(br.expr()))
        return false;

    if (!e.writeVarU32(br.target().index()))
        return false;

    if (br.expr() == Expr::BrIf) {
        if (!EncodeExpr(e, br.cond()))
            return false;
    }

    return true;
}

static bool
EncodeArgs(Encoder& e, const WasmAstExprVector& args)
{
    for (WasmAstExpr* arg : args) {
        if (!EncodeExpr(e, *arg))
            return false;
    }

    return true;
}

static bool
EncodeCall(Encoder& e, WasmAstCall& c)
{
    if (!e.writeExpr(c.expr()))
        return false;

    if (!e.writeVarU32(c.func().index()))
        return false;

    if (!EncodeArgs(e, c.args()))
        return false;

    return true;
}

static bool
EncodeCallIndirect(Encoder& e, WasmAstCallIndirect& c)
{
    if (!e.writeExpr(Expr::CallIndirect))
        return false;

    if (!e.writeVarU32(c.sig().index()))
        return false;

    if (!EncodeExpr(e, *c.index()))
        return false;

    if (!EncodeArgs(e, c.args()))
        return false;

    return true;
}

static bool
EncodeConst(Encoder& e, WasmAstConst& c)
{
    switch (c.val().type()) {
      case ValType::I32:
        return e.writeExpr(Expr::I32Const) &&
               e.writeVarU32(c.val().i32());
      case ValType::I64:
        return e.writeExpr(Expr::I64Const) &&
               e.writeVarU64(c.val().i64());
      case ValType::F32:
        return e.writeExpr(Expr::F32Const) &&
               e.writeFixedF32(c.val().f32());
      case ValType::F64:
        return e.writeExpr(Expr::F64Const) &&
               e.writeFixedF64(c.val().f64());
      default:
        break;
    }
    MOZ_CRASH("Bad value type");
}

static bool
EncodeGetLocal(Encoder& e, WasmAstGetLocal& gl)
{
    return e.writeExpr(Expr::GetLocal) &&
           e.writeVarU32(gl.local().index());
}

static bool
EncodeSetLocal(Encoder& e, WasmAstSetLocal& sl)
{
    return e.writeExpr(Expr::SetLocal) &&
           e.writeVarU32(sl.local().index()) &&
           EncodeExpr(e, sl.value());
}

static bool
EncodeUnaryOperator(Encoder& e, WasmAstUnaryOperator& b)
{
    return e.writeExpr(b.expr()) &&
           EncodeExpr(e, *b.op());
}

static bool
EncodeBinaryOperator(Encoder& e, WasmAstBinaryOperator& b)
{
    return e.writeExpr(b.expr()) &&
           EncodeExpr(e, *b.lhs()) &&
           EncodeExpr(e, *b.rhs());
}

static bool
EncodeComparisonOperator(Encoder& e, WasmAstComparisonOperator& b)
{
    return e.writeExpr(b.expr()) &&
           EncodeExpr(e, *b.lhs()) &&
           EncodeExpr(e, *b.rhs());
}

static bool
EncodeConversionOperator(Encoder& e, WasmAstConversionOperator& b)
{
    return e.writeExpr(b.expr()) &&
           EncodeExpr(e, *b.op());
}

static bool
EncodeIfElse(Encoder& e, WasmAstIfElse& ie)
{
    return e.writeExpr(ie.expr()) &&
           EncodeExpr(e, ie.cond()) &&
           EncodeExpr(e, ie.ifBody()) &&
           (!ie.hasElse() || EncodeExpr(e, ie.elseBody()));
}

static bool
EncodeLoadStoreAddress(Encoder &e, const WasmAstLoadStoreAddress &address)
{
    return e.writeVarU32(address.offset()) &&
           e.writeVarU32(address.align()) &&
           EncodeExpr(e, address.base());
}

static bool
EncodeLoad(Encoder& e, WasmAstLoad& l)
{
    return e.writeExpr(l.expr()) &&
           EncodeLoadStoreAddress(e, l.address());
}

static bool
EncodeStore(Encoder& e, WasmAstStore& s)
{
    return e.writeExpr(s.expr()) &&
           EncodeLoadStoreAddress(e, s.address()) &&
           EncodeExpr(e, s.value());
}

static bool
EncodeReturn(Encoder& e, WasmAstReturn& r)
{
    return e.writeExpr(Expr::Return) &&
           (!r.maybeExpr() || EncodeExpr(e, *r.maybeExpr()));
}

static bool
EncodeTableSwitch(Encoder& e, WasmAstTableSwitch& ts)
{
    if (!e.writeExpr(Expr::TableSwitch))
        return false;

    if (!e.writeVarU32(ts.def().index()))
        return false;

    if (!e.writeVarU32(ts.table().length()))
        return false;

    for (const WasmRef& elem : ts.table()) {
        if (!e.writeVarU32(elem.index()))
            return false;
    }

    return EncodeExpr(e, ts.index());
}

static bool
EncodeExpr(Encoder& e, WasmAstExpr& expr)
{
    switch (expr.kind()) {
      case WasmAstExprKind::Nop:
        return e.writeExpr(Expr::Nop);
      case WasmAstExprKind::BinaryOperator:
        return EncodeBinaryOperator(e, expr.as<WasmAstBinaryOperator>());
      case WasmAstExprKind::Block:
        return EncodeBlock(e, expr.as<WasmAstBlock>());
      case WasmAstExprKind::Branch:
        return EncodeBranch(e, expr.as<WasmAstBranch>());
      case WasmAstExprKind::Call:
        return EncodeCall(e, expr.as<WasmAstCall>());
      case WasmAstExprKind::CallIndirect:
        return EncodeCallIndirect(e, expr.as<WasmAstCallIndirect>());
      case WasmAstExprKind::ComparisonOperator:
        return EncodeComparisonOperator(e, expr.as<WasmAstComparisonOperator>());
      case WasmAstExprKind::Const:
        return EncodeConst(e, expr.as<WasmAstConst>());
      case WasmAstExprKind::ConversionOperator:
        return EncodeConversionOperator(e, expr.as<WasmAstConversionOperator>());
      case WasmAstExprKind::GetLocal:
        return EncodeGetLocal(e, expr.as<WasmAstGetLocal>());
      case WasmAstExprKind::IfElse:
        return EncodeIfElse(e, expr.as<WasmAstIfElse>());
      case WasmAstExprKind::Load:
        return EncodeLoad(e, expr.as<WasmAstLoad>());
      case WasmAstExprKind::Return:
        return EncodeReturn(e, expr.as<WasmAstReturn>());
      case WasmAstExprKind::SetLocal:
        return EncodeSetLocal(e, expr.as<WasmAstSetLocal>());
      case WasmAstExprKind::Store:
        return EncodeStore(e, expr.as<WasmAstStore>());
      case WasmAstExprKind::TableSwitch:
        return EncodeTableSwitch(e, expr.as<WasmAstTableSwitch>());
      case WasmAstExprKind::UnaryOperator:
        return EncodeUnaryOperator(e, expr.as<WasmAstUnaryOperator>());
    }
    MOZ_CRASH("Bad expr kind");
}

/*****************************************************************************/
// wasm AST binary serialization

static bool
EncodeSignatureSection(Encoder& e, WasmAstModule& module)
{
    if (module.sigs().empty())
        return true;

    if (!e.writeCString(SigLabel))
        return false;

    size_t offset;
    if (!e.startSection(&offset))
        return false;

    if (!e.writeVarU32(module.sigs().length()))
        return false;

    for (WasmAstSig* sig : module.sigs()) {
        if (!e.writeVarU32(sig->args().length()))
            return false;

        if (!e.writeExprType(sig->ret()))
            return false;

        for (ValType t : sig->args()) {
            if (!e.writeValType(t))
                return false;
        }
    }

    e.finishSection(offset);
    return true;
}

static bool
EncodeDeclarationSection(Encoder& e, WasmAstModule& module)
{
    if (module.funcs().empty())
        return true;

    if (!e.writeCString(DeclLabel))
        return false;

    size_t offset;
    if (!e.startSection(&offset))
        return false;

    if (!e.writeVarU32(module.funcs().length()))
        return false;

    for (WasmAstFunc* func : module.funcs()) {
        if (!e.writeVarU32(func->sig().index()))
            return false;
    }

    e.finishSection(offset);
    return true;
}

static bool
EncodeCString(Encoder& e, WasmName wasmName)
{
    TwoByteChars range(wasmName.begin(), wasmName.length());
    UniqueChars utf8(JS::CharsToNewUTF8CharsZ(nullptr, range).c_str());
    return utf8 && e.writeCString(utf8.get());
}

static bool
EncodeImport(Encoder& e, WasmAstImport& imp)
{
    if (!e.writeVarU32(imp.sigIndex()))
        return false;

    if (!EncodeCString(e, imp.module()))
        return false;

    if (!EncodeCString(e, imp.func()))
        return false;

    return true;
}

static bool
EncodeImportSection(Encoder& e, WasmAstModule& module)
{
    if (module.imports().empty())
        return true;

    if (!e.writeCString(ImportLabel))
        return false;

    size_t offset;
    if (!e.startSection(&offset))
        return false;

    for (WasmAstImport* imp : module.imports()) {
        if (!e.writeCString(FuncLabel))
            return false;
        if (!EncodeImport(e, *imp))
            return false;
    }

    if (!e.writeCString(EndLabel))
        return false;

    e.finishSection(offset);
    return true;
}

static bool
EncodeMemorySection(Encoder& e, WasmAstModule& module)
{
    if (!module.maybeMemory())
        return true;

    if (!e.writeCString(MemoryLabel))
        return false;

    size_t offset;
    if (!e.startSection(&offset))
        return false;

    WasmAstMemory& memory = *module.maybeMemory();

    if (!e.writeCString(InitialLabel))
        return false;

    if (!e.writeVarU32(memory.initialSize()))
        return false;

    if (!e.writeCString(EndLabel))
        return false;

    e.finishSection(offset);
    return true;
}

static bool
EncodeFunctionExport(Encoder& e, WasmAstExport& exp)
{
    if (!e.writeVarU32(exp.func().index()))
        return false;

    if (!EncodeCString(e, exp.name()))
        return false;

    return true;
}

static bool
EncodeMemoryExport(Encoder& e, WasmAstExport& exp)
{
    if (!EncodeCString(e, exp.name()))
        return false;

    return true;
}

static bool
EncodeExportSection(Encoder& e, WasmAstModule& module)
{
    if (module.exports().empty())
        return true;

    if (!e.writeCString(ExportLabel))
        return false;

    size_t offset;
    if (!e.startSection(&offset))
        return false;

    for (WasmAstExport* exp : module.exports()) {
        switch (exp->kind()) {
          case WasmAstExportKind::Func:
            if (!e.writeCString(FuncLabel))
                return false;
            if (!EncodeFunctionExport(e, *exp))
                return false;
            break;
          case WasmAstExportKind::Memory:
            if (!e.writeCString(MemoryLabel))
                return false;
            if (!EncodeMemoryExport(e, *exp))
                return false;
            break;
        }
    }

    if (!e.writeCString(EndLabel))
        return false;

    e.finishSection(offset);
    return true;
}

static bool
EncodeTableSection(Encoder& e, WasmAstModule& module)
{
    if (!module.maybeTable())
        return true;

    if (!e.writeCString(TableLabel))
        return false;

    size_t offset;
    if (!e.startSection(&offset))
        return false;

    if (!e.writeVarU32(module.maybeTable()->elems().length()))
        return false;

    for (WasmRef& ref : module.maybeTable()->elems()) {
        if (!e.writeVarU32(ref.index()))
            return false;
    }

    e.finishSection(offset);
    return true;
}

static bool
EncodeFunctionSection(Encoder& e, WasmAstFunc& func)
{
    if (!e.writeCString(FuncLabel))
        return false;

    size_t offset;
    if (!e.startSection(&offset))
        return false;

    if (!e.writeVarU32(func.vars().length()))
        return false;

    for (ValType type : func.vars()) {
        if (!e.writeValType(type))
            return false;
    }

    if (!e.writeVarU32(func.body().length()))
        return false;

    for (WasmAstExpr* expr : func.body()) {
        if (!EncodeExpr(e, *expr))
            return false;
    }

    e.finishSection(offset);

    return true;
}

static bool
EncodeDataSegment(Encoder& e, WasmAstSegment& segment)
{
    if (!e.writeVarU32(segment.offset()))
        return false;

    WasmName text = segment.text();

    Vector<uint8_t, 0, SystemAllocPolicy> bytes;
    if (!bytes.reserve(text.length()))
        return false;

    const char16_t* cur = text.begin();
    const char16_t* end = text.end();
    while (cur != end) {
        uint8_t byte;
        MOZ_ALWAYS_TRUE(ConsumeTextByte(&cur, end, &byte));
        bytes.infallibleAppend(byte);
    }

    if (!e.writeVarU32(bytes.length()))
        return false;

    if (!e.writeRawData(bytes.begin(), bytes.length()))
        return false;

    return true;
}

static bool
EncodeDataSection(Encoder& e, WasmAstModule& module)
{
    if (!module.maybeMemory() || module.maybeMemory()->segments().empty())
        return true;

    const WasmAstSegmentVector& segments = module.maybeMemory()->segments();

    if (!e.writeCString(DataLabel))
        return false;

    size_t offset;
    if (!e.startSection(&offset))
        return false;

    for (WasmAstSegment* segment : segments) {
        if (!e.writeCString(SegmentLabel))
            return false;
        if (!EncodeDataSegment(e, *segment))
            return false;
    }

    if (!e.writeCString(EndLabel))
        return false;

    e.finishSection(offset);
    return true;
}

static UniqueBytecode
EncodeModule(WasmAstModule& module)
{
    UniqueBytecode bytecode = MakeUnique<Bytecode>();
    if (!bytecode)
        return nullptr;

    Encoder e(*bytecode);

    if (!e.writeFixedU32(MagicNumber))
        return nullptr;

    if (!e.writeFixedU32(EncodingVersion))
        return nullptr;

    if (!EncodeSignatureSection(e, module))
        return nullptr;

    if (!EncodeImportSection(e, module))
        return nullptr;

    if (!EncodeDeclarationSection(e, module))
        return nullptr;

    if (!EncodeTableSection(e, module))
        return nullptr;

    if (!EncodeMemorySection(e, module))
        return nullptr;

    if (!EncodeExportSection(e, module))
        return nullptr;

    for (WasmAstFunc* func : module.funcs()) {
        if (!EncodeFunctionSection(e, *func))
            return nullptr;
    }

    if (!EncodeDataSection(e, module))
        return nullptr;

    if (!e.writeCString(EndLabel))
        return nullptr;

    return Move(bytecode);
}

/*****************************************************************************/

UniqueBytecode
wasm::TextToBinary(const char16_t* text, UniqueChars* error)
{
    LifoAlloc lifo(AST_LIFO_DEFAULT_CHUNK_SIZE);
    WasmAstModule* module = ParseModule(text, lifo, error);
    if (!module)
        return nullptr;

    if (!ResolveModule(lifo, module, error))
        return nullptr;

    return EncodeModule(*module);
}

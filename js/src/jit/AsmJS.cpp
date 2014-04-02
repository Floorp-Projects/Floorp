/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/AsmJS.h"

#include "mozilla/Move.h"

#ifdef MOZ_VTUNE
# include "vtune/VTuneWrapper.h"
#endif

#include "jsmath.h"
#include "jsprf.h"
#include "jsworkers.h"
#include "prmjtime.h"

#include "assembler/assembler/MacroAssembler.h"
#include "frontend/Parser.h"
#include "jit/AsmJSLink.h"
#include "jit/AsmJSModule.h"
#include "jit/AsmJSSignalHandlers.h"
#include "jit/CodeGenerator.h"
#include "jit/CompileWrappers.h"
#include "jit/MIR.h"
#include "jit/MIRGraph.h"
#ifdef JS_ION_PERF
# include "jit/PerfSpewer.h"
#endif
#include "vm/Interpreter.h"

#include "jsinferinlines.h"
#include "jsobjinlines.h"

#include "frontend/ParseNode-inl.h"
#include "frontend/Parser-inl.h"

using namespace js;
using namespace js::frontend;
using namespace js::jit;

using mozilla::AddToHash;
using mozilla::ArrayLength;
using mozilla::CountLeadingZeroes32;
using mozilla::DebugOnly;
using mozilla::HashGeneric;
using mozilla::IsNaN;
using mozilla::IsNegativeZero;
using mozilla::Maybe;
using mozilla::Move;
using mozilla::PositiveInfinity;
using JS::GenericNaN;

static const size_t LIFO_ALLOC_PRIMARY_CHUNK_SIZE = 1 << 12;

/*****************************************************************************/
// ParseNode utilities

static inline ParseNode *
NextNode(ParseNode *pn)
{
    return pn->pn_next;
}

static inline ParseNode *
UnaryKid(ParseNode *pn)
{
    JS_ASSERT(pn->isArity(PN_UNARY));
    return pn->pn_kid;
}

static inline ParseNode *
ReturnExpr(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_RETURN));
    return UnaryKid(pn);
}

static inline ParseNode *
BinaryRight(ParseNode *pn)
{
    JS_ASSERT(pn->isArity(PN_BINARY));
    return pn->pn_right;
}

static inline ParseNode *
BinaryLeft(ParseNode *pn)
{
    JS_ASSERT(pn->isArity(PN_BINARY));
    return pn->pn_left;
}

static inline ParseNode *
TernaryKid1(ParseNode *pn)
{
    JS_ASSERT(pn->isArity(PN_TERNARY));
    return pn->pn_kid1;
}

static inline ParseNode *
TernaryKid2(ParseNode *pn)
{
    JS_ASSERT(pn->isArity(PN_TERNARY));
    return pn->pn_kid2;
}

static inline ParseNode *
TernaryKid3(ParseNode *pn)
{
    JS_ASSERT(pn->isArity(PN_TERNARY));
    return pn->pn_kid3;
}

static inline ParseNode *
ListHead(ParseNode *pn)
{
    JS_ASSERT(pn->isArity(PN_LIST));
    return pn->pn_head;
}

static inline unsigned
ListLength(ParseNode *pn)
{
    JS_ASSERT(pn->isArity(PN_LIST));
    return pn->pn_count;
}

static inline ParseNode *
CallCallee(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_CALL));
    return ListHead(pn);
}

static inline unsigned
CallArgListLength(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_CALL));
    JS_ASSERT(ListLength(pn) >= 1);
    return ListLength(pn) - 1;
}

static inline ParseNode *
CallArgList(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_CALL));
    return NextNode(ListHead(pn));
}

static inline ParseNode *
VarListHead(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_VAR) || pn->isKind(PNK_CONST));
    return ListHead(pn);
}

static inline ParseNode *
CaseExpr(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_CASE) || pn->isKind(PNK_DEFAULT));
    return BinaryLeft(pn);
}

static inline ParseNode *
CaseBody(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_CASE) || pn->isKind(PNK_DEFAULT));
    return BinaryRight(pn);
}

static inline bool
IsExpressionStatement(ParseNode *pn)
{
    return pn->isKind(PNK_SEMI);
}

static inline ParseNode *
ExpressionStatementExpr(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_SEMI));
    return UnaryKid(pn);
}

static inline PropertyName *
LoopControlMaybeLabel(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_BREAK) || pn->isKind(PNK_CONTINUE));
    JS_ASSERT(pn->isArity(PN_NULLARY));
    return pn->as<LoopControlStatement>().label();
}

static inline PropertyName *
LabeledStatementLabel(ParseNode *pn)
{
    return pn->as<LabeledStatement>().label();
}

static inline ParseNode *
LabeledStatementStatement(ParseNode *pn)
{
    return pn->as<LabeledStatement>().statement();
}

static double
NumberNodeValue(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_NUMBER));
    return pn->pn_dval;
}

static bool
NumberNodeHasFrac(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_NUMBER));
    return pn->pn_u.number.decimalPoint == HasDecimal;
}

static ParseNode *
DotBase(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_DOT));
    JS_ASSERT(pn->isArity(PN_NAME));
    return pn->expr();
}

static PropertyName *
DotMember(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_DOT));
    JS_ASSERT(pn->isArity(PN_NAME));
    return pn->pn_atom->asPropertyName();
}

static ParseNode *
ElemBase(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_ELEM));
    return BinaryLeft(pn);
}

static ParseNode *
ElemIndex(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_ELEM));
    return BinaryRight(pn);
}

static inline JSFunction *
FunctionObject(ParseNode *fn)
{
    JS_ASSERT(fn->isKind(PNK_FUNCTION));
    JS_ASSERT(fn->isArity(PN_CODE));
    return fn->pn_funbox->function();
}

static inline PropertyName *
FunctionName(ParseNode *fn)
{
    if (JSAtom *atom = FunctionObject(fn)->atom())
        return atom->asPropertyName();
    return nullptr;
}

static inline ParseNode *
FunctionStatementList(ParseNode *fn)
{
    JS_ASSERT(fn->pn_body->isKind(PNK_ARGSBODY));
    ParseNode *last = fn->pn_body->last();
    JS_ASSERT(last->isKind(PNK_STATEMENTLIST));
    return last;
}

static inline bool
IsNormalObjectField(ExclusiveContext *cx, ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_COLON));
    return pn->getOp() == JSOP_INITPROP &&
           BinaryLeft(pn)->isKind(PNK_NAME) &&
           BinaryLeft(pn)->name() != cx->names().proto;
}

static inline PropertyName *
ObjectNormalFieldName(ExclusiveContext *cx, ParseNode *pn)
{
    JS_ASSERT(IsNormalObjectField(cx, pn));
    return BinaryLeft(pn)->name();
}

static inline ParseNode *
ObjectFieldInitializer(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_COLON));
    return BinaryRight(pn);
}

static inline bool
IsDefinition(ParseNode *pn)
{
    return pn->isKind(PNK_NAME) && pn->isDefn();
}

static inline ParseNode *
MaybeDefinitionInitializer(ParseNode *pn)
{
    JS_ASSERT(IsDefinition(pn));
    return pn->expr();
}

static inline bool
IsUseOfName(ParseNode *pn, PropertyName *name)
{
    return pn->isKind(PNK_NAME) && pn->name() == name;
}

static inline bool
IsEmptyStatement(ParseNode *pn)
{
    return pn->isKind(PNK_SEMI) && !UnaryKid(pn);
}

static inline ParseNode *
SkipEmptyStatements(ParseNode *pn)
{
    while (pn && IsEmptyStatement(pn))
        pn = pn->pn_next;
    return pn;
}

static inline ParseNode *
NextNonEmptyStatement(ParseNode *pn)
{
    return SkipEmptyStatements(pn->pn_next);
}

static TokenKind
PeekToken(AsmJSParser &parser)
{
    TokenStream &ts = parser.tokenStream;
    while (ts.peekToken(TokenStream::Operand) == TOK_SEMI)
        ts.consumeKnownToken(TOK_SEMI);
    return ts.peekToken(TokenStream::Operand);
}

static bool
ParseVarOrConstStatement(AsmJSParser &parser, ParseNode **var)
{
    TokenKind tk = PeekToken(parser);
    if (tk != TOK_VAR && tk != TOK_CONST) {
        *var = nullptr;
        return true;
    }

    *var = parser.statement();
    if (!*var)
        return false;

    JS_ASSERT((*var)->isKind(PNK_VAR) || (*var)->isKind(PNK_CONST));
    return true;
}

/*****************************************************************************/

namespace {

// Respresents the type of a general asm.js expression.
class Type
{
  public:
    enum Which {
        Double,
        MaybeDouble,
        Float,
        MaybeFloat,
        Floatish,
        Fixnum,
        Int,
        Signed,
        Unsigned,
        Intish,
        Void
    };

  private:
    Which which_;

  public:
    Type() : which_(Which(-1)) {}
    Type(Which w) : which_(w) {}

    bool operator==(Type rhs) const { return which_ == rhs.which_; }
    bool operator!=(Type rhs) const { return which_ != rhs.which_; }

    bool isSigned() const {
        return which_ == Signed || which_ == Fixnum;
    }

    bool isUnsigned() const {
        return which_ == Unsigned || which_ == Fixnum;
    }

    bool isInt() const {
        return isSigned() || isUnsigned() || which_ == Int;
    }

    bool isIntish() const {
        return isInt() || which_ == Intish;
    }

    bool isDouble() const {
        return which_ == Double;
    }

    bool isMaybeDouble() const {
        return isDouble() || which_ == MaybeDouble;
    }

    bool isFloat() const {
        return which_ == Float;
    }

    bool isMaybeFloat() const {
        return isFloat() || which_ == MaybeFloat;
    }

    bool isFloatish() const {
        return isMaybeFloat() || which_ == Floatish;
    }

    bool isVoid() const {
        return which_ == Void;
    }

    bool isExtern() const {
        return isDouble() || isSigned();
    }

    bool isVarType() const {
        return isInt() || isDouble() || isFloat();
    }

    MIRType toMIRType() const {
        switch (which_) {
          case Double:
          case MaybeDouble:
            return MIRType_Double;
          case Float:
          case Floatish:
          case MaybeFloat:
            return MIRType_Float32;
          case Fixnum:
          case Int:
          case Signed:
          case Unsigned:
          case Intish:
            return MIRType_Int32;
          case Void:
            return MIRType_None;
        }
        MOZ_ASSUME_UNREACHABLE("Invalid Type");
    }

    const char *toChars() const {
        switch (which_) {
          case Double:      return "double";
          case MaybeDouble: return "double?";
          case Float:       return "float";
          case Floatish:    return "floatish";
          case MaybeFloat:  return "float?";
          case Fixnum:      return "fixnum";
          case Int:         return "int";
          case Signed:      return "signed";
          case Unsigned:    return "unsigned";
          case Intish:      return "intish";
          case Void:        return "void";
        }
        MOZ_ASSUME_UNREACHABLE("Invalid Type");
    }
};

} /* anonymous namespace */

// Represents the subset of Type that can be used as the return type of a
// function.
class RetType
{
  public:
    enum Which {
        Void = Type::Void,
        Signed = Type::Signed,
        Double = Type::Double,
        Float = Type::Float
    };

  private:
    Which which_;

  public:
    RetType() : which_(Which(-1)) {}
    RetType(Which w) : which_(w) {}
    RetType(AsmJSCoercion coercion) {
        switch (coercion) {
          case AsmJS_ToInt32: which_ = Signed; break;
          case AsmJS_ToNumber: which_ = Double; break;
          case AsmJS_FRound: which_ = Float; break;
        }
    }
    Which which() const {
        return which_;
    }
    Type toType() const {
        return Type::Which(which_);
    }
    AsmJSModule::ReturnType toModuleReturnType() const {
        switch (which_) {
          case Void: return AsmJSModule::Return_Void;
          case Signed: return AsmJSModule::Return_Int32;
          case Float: // will be converted to a Double
          case Double: return AsmJSModule::Return_Double;
        }
        MOZ_ASSUME_UNREACHABLE("Unexpected return type");
    }
    MIRType toMIRType() const {
        switch (which_) {
          case Void: return MIRType_None;
          case Signed: return MIRType_Int32;
          case Double: return MIRType_Double;
          case Float: return MIRType_Float32;
        }
        MOZ_ASSUME_UNREACHABLE("Unexpected return type");
    }
    bool operator==(RetType rhs) const { return which_ == rhs.which_; }
    bool operator!=(RetType rhs) const { return which_ != rhs.which_; }
};

namespace {

// Represents the subset of Type that can be used as a variable or
// argument's type. Note: AsmJSCoercion and VarType are kept separate to
// make very clear the signed/int distinction: a coercion may explicitly sign
// an *expression* but, when stored as a variable, this signedness information
// is explicitly thrown away by the asm.js type system. E.g., in
//
//   function f(i) {
//     i = i | 0;             (1)
//     if (...)
//         i = foo() >>> 0;
//     else
//         i = bar() | 0;
//     return i | 0;          (2)
//   }
//
// the AsmJSCoercion of (1) is Signed (since | performs ToInt32) but, when
// translated to an VarType, the result is a plain Int since, as shown, it
// is legal to assign both Signed and Unsigned (or some other Int) values to
// it. For (2), the AsmJSCoercion is also Signed but, when translated to an
// RetType, the result is Signed since callers (asm.js and non-asm.js) can
// rely on the return value being Signed.
class VarType
{
  public:
    enum Which {
        Int = Type::Int,
        Double = Type::Double,
        Float = Type::Float
    };

  private:
    Which which_;

  public:
    VarType()
      : which_(Which(-1)) {}
    VarType(Which w)
      : which_(w) {}
    VarType(AsmJSCoercion coercion) {
        switch (coercion) {
          case AsmJS_ToInt32: which_ = Int; break;
          case AsmJS_ToNumber: which_ = Double; break;
          case AsmJS_FRound: which_ = Float; break;
        }
    }
    Which which() const {
        return which_;
    }
    Type toType() const {
        return Type::Which(which_);
    }
    MIRType toMIRType() const {
        switch(which_) {
          case Int:     return MIRType_Int32;
          case Double:  return MIRType_Double;
          case Float:   return MIRType_Float32;
        }
        MOZ_ASSUME_UNREACHABLE("VarType can only be Int, Double or Float");
    }
    AsmJSCoercion toCoercion() const {
        switch(which_) {
          case Int:     return AsmJS_ToInt32;
          case Double:  return AsmJS_ToNumber;
          case Float:   return AsmJS_FRound;
        }
        MOZ_ASSUME_UNREACHABLE("VarType can only be Int, Double or Float");
    }
    static VarType FromMIRType(MIRType type) {
        JS_ASSERT(type == MIRType_Int32 || type == MIRType_Double || type == MIRType_Float32);
        switch(type) {
          case MIRType_Int32:   return Int;
          case MIRType_Float32: return Float;
          case MIRType_Double:  return Double;
          default:;
        }
        MOZ_ASSUME_UNREACHABLE("FromMIRType MIR type not handled");
    }
    static VarType FromCheckedType(Type type) {
        JS_ASSERT(type.isInt() || type.isMaybeDouble() || type.isFloatish());
        if (type.isMaybeDouble())
            return Double;
        else if (type.isFloatish())
            return Float;
        else
            return Int;
    }
    bool operator==(VarType rhs) const { return which_ == rhs.which_; }
    bool operator!=(VarType rhs) const { return which_ != rhs.which_; }
};

} /* anonymous namespace */

// Implements <: (subtype) operator when the rhs is an VarType
static inline bool
operator<=(Type lhs, VarType rhs)
{
    switch (rhs.which()) {
      case VarType::Int:    return lhs.isInt();
      case VarType::Double: return lhs.isDouble();
      case VarType::Float:  return lhs.isFloat();
    }
    MOZ_ASSUME_UNREACHABLE("Unexpected rhs type");
}

/*****************************************************************************/

static inline MIRType ToMIRType(MIRType t) { return t; }
static inline MIRType ToMIRType(VarType t) { return t.toMIRType(); }

namespace {

template <class VecT>
class ABIArgIter
{
    ABIArgGenerator gen_;
    const VecT &types_;
    unsigned i_;

    void settle() { if (!done()) gen_.next(ToMIRType(types_[i_])); }

  public:
    ABIArgIter(const VecT &types) : types_(types), i_(0) { settle(); }
    void operator++(int) { JS_ASSERT(!done()); i_++; settle(); }
    bool done() const { return i_ == types_.length(); }

    ABIArg *operator->() { JS_ASSERT(!done()); return &gen_.current(); }
    ABIArg &operator*() { JS_ASSERT(!done()); return gen_.current(); }

    unsigned index() const { JS_ASSERT(!done()); return i_; }
    MIRType mirType() const { JS_ASSERT(!done()); return ToMIRType(types_[i_]); }
    uint32_t stackBytesConsumedSoFar() const { return gen_.stackBytesConsumedSoFar(); }
};

typedef js::Vector<MIRType, 8> MIRTypeVector;
typedef ABIArgIter<MIRTypeVector> ABIArgMIRTypeIter;

typedef js::Vector<VarType, 8, LifoAllocPolicy> VarTypeVector;
typedef ABIArgIter<VarTypeVector> ABIArgTypeIter;

class Signature
{
    VarTypeVector argTypes_;
    RetType retType_;

  public:
    Signature(LifoAlloc &alloc)
      : argTypes_(alloc) {}
    Signature(LifoAlloc &alloc, RetType retType)
      : argTypes_(alloc), retType_(retType) {}
    Signature(VarTypeVector &&argTypes, RetType retType)
      : argTypes_(Move(argTypes)), retType_(Move(retType)) {}
    Signature(Signature &&rhs)
      : argTypes_(Move(rhs.argTypes_)), retType_(Move(rhs.retType_)) {}

    bool copy(const Signature &rhs) {
        if (!argTypes_.resize(rhs.argTypes_.length()))
            return false;
        for (unsigned i = 0; i < argTypes_.length(); i++)
            argTypes_[i] = rhs.argTypes_[i];
        retType_ = rhs.retType_;
        return true;
    }

    bool appendArg(VarType type) { return argTypes_.append(type); }
    VarType arg(unsigned i) const { return argTypes_[i]; }
    const VarTypeVector &args() const { return argTypes_; }
    VarTypeVector &&extractArgs() { return Move(argTypes_); }

    RetType retType() const { return retType_; }
};

} /* namespace anonymous */

static
bool operator==(const Signature &lhs, const Signature &rhs)
{
    if (lhs.retType() != rhs.retType())
        return false;
    if (lhs.args().length() != rhs.args().length())
        return false;
    for (unsigned i = 0; i < lhs.args().length(); i++) {
        if (lhs.arg(i) != rhs.arg(i))
            return false;
    }
    return true;
}

static inline
bool operator!=(const Signature &lhs, const Signature &rhs)
{
    return !(lhs == rhs);
}

/*****************************************************************************/
// Typed array utilities

static Type
TypedArrayLoadType(ArrayBufferView::ViewType viewType)
{
    switch (viewType) {
      case ArrayBufferView::TYPE_INT8:
      case ArrayBufferView::TYPE_INT16:
      case ArrayBufferView::TYPE_INT32:
      case ArrayBufferView::TYPE_UINT8:
      case ArrayBufferView::TYPE_UINT16:
      case ArrayBufferView::TYPE_UINT32:
        return Type::Intish;
      case ArrayBufferView::TYPE_FLOAT32:
        return Type::MaybeFloat;
      case ArrayBufferView::TYPE_FLOAT64:
        return Type::MaybeDouble;
      default:;
    }
    MOZ_ASSUME_UNREACHABLE("Unexpected array type");
}

enum NeedsBoundsCheck {
    NO_BOUNDS_CHECK,
    NEEDS_BOUNDS_CHECK
};

namespace {

typedef js::Vector<PropertyName*,1> LabelVector;
typedef js::Vector<MBasicBlock*,8> BlockVector;

// ModuleCompiler encapsulates the compilation of an entire asm.js module. Over
// the course of an ModuleCompiler object's lifetime, many FunctionCompiler
// objects will be created and destroyed in sequence, one for each function in
// the module.
//
// *** asm.js FFI calls ***
//
// asm.js allows calling out to non-asm.js via "FFI calls". The asm.js type
// system does not place any constraints on the FFI call. In particular:
//  - an FFI call's target is not known or speculated at module-compile time;
//  - a single external function can be called with different signatures.
//
// If performance didn't matter, all FFI calls could simply box their arguments
// and call js::Invoke. However, we'd like to be able to specialize FFI calls
// to be more efficient in several cases:
//
//  - for calls to JS functions which have been jitted, we'd like to call
//    directly into JIT code without going through C++.
//
//  - for calls to certain builtins, we'd like to be call directly into the C++
//    code for the builtin without going through the general call path.
//
// All of this requires dynamic specialization techniques which must happen
// after module compilation. To support this, at module-compilation time, each
// FFI call generates a call signature according to the system ABI, as if the
// callee was a C++ function taking/returning the same types as the caller was
// passing/expecting. The callee is loaded from a fixed offset in the global
// data array which allows the callee to change at runtime. Initially, the
// callee is stub which boxes its arguments and calls js::Invoke.
//
// To do this, we need to generate a callee stub for each pairing of FFI callee
// and signature. We call this pairing an "exit". For example, this code has
// two external functions and three exits:
//
//  function f(global, imports) {
//    "use asm";
//    var foo = imports.foo;
//    var bar = imports.bar;
//    function g() {
//      foo(1);      // Exit #1: (int) -> void
//      foo(1.5);    // Exit #2: (double) -> void
//      bar(1)|0;    // Exit #3: (int) -> int
//      bar(2)|0;    // Exit #3: (int) -> int
//    }
//  }
//
// The ModuleCompiler maintains a hash table (ExitMap) which allows a call site
// to add a new exit or reuse an existing one. The key is an ExitDescriptor
// (which holds the exit pairing) and the value is an index into the
// Vector<Exit> stored in the AsmJSModule.
//
// Rooting note: ModuleCompiler is a stack class that contains unrooted
// PropertyName (JSAtom) pointers.  This is safe because it cannot be
// constructed without a TokenStream reference.  TokenStream is itself a stack
// class that cannot be constructed without an AutoKeepAtoms being live on the
// stack, which prevents collection of atoms.
//
// ModuleCompiler is marked as rooted in the rooting analysis.  Don't add
// non-JSAtom pointers, or this will break!
class MOZ_STACK_CLASS ModuleCompiler
{
  public:
    class Func
    {
        PropertyName *name_;
        bool defined_;
        uint32_t srcOffset_;
        Signature sig_;
        Label *code_;
        unsigned compileTime_;

      public:
        Func(PropertyName *name, Signature &&sig, Label *code)
          : name_(name), defined_(false), srcOffset_(0), sig_(Move(sig)), code_(code), compileTime_(0)
        {}

        PropertyName *name() const { return name_; }
        bool defined() const { return defined_; }
        void define(uint32_t so) { JS_ASSERT(!defined_); defined_ = true; srcOffset_ = so; }
        uint32_t srcOffset() const { JS_ASSERT(defined_); return srcOffset_; }
        Signature &sig() { return sig_; }
        const Signature &sig() const { return sig_; }
        Label *code() const { return code_; }
        unsigned compileTime() const { return compileTime_; }
        void accumulateCompileTime(unsigned ms) { compileTime_ += ms; }
    };

    class Global
    {
      public:
        enum Which {
            Variable,
            ConstantLiteral,
            ConstantImport,
            Function,
            FuncPtrTable,
            FFI,
            ArrayView,
            MathBuiltinFunction
        };

      private:
        Which which_;
        union {
            struct {
                VarType::Which type_;
                uint32_t index_;
                Value literalValue_;
            } varOrConst;
            uint32_t funcIndex_;
            uint32_t funcPtrTableIndex_;
            uint32_t ffiIndex_;
            ArrayBufferView::ViewType viewType_;
            AsmJSMathBuiltinFunction mathBuiltinFunc_;
        } u;

        friend class ModuleCompiler;
        friend class js::LifoAlloc;

        Global(Which which) : which_(which) {}

      public:
        Which which() const {
            return which_;
        }
        VarType varOrConstType() const {
            JS_ASSERT(which_ == Variable || which_ == ConstantLiteral || which_ == ConstantImport);
            return VarType(u.varOrConst.type_);
        }
        uint32_t varOrConstIndex() const {
            JS_ASSERT(which_ == Variable || which_ == ConstantImport);
            return u.varOrConst.index_;
        }
        bool isConst() const {
            return which_ == ConstantLiteral || which_ == ConstantImport;
        }
        Value constLiteralValue() const {
            JS_ASSERT(which_ == ConstantLiteral);
            return u.varOrConst.literalValue_;
        }
        uint32_t funcIndex() const {
            JS_ASSERT(which_ == Function);
            return u.funcIndex_;
        }
        uint32_t funcPtrTableIndex() const {
            JS_ASSERT(which_ == FuncPtrTable);
            return u.funcPtrTableIndex_;
        }
        unsigned ffiIndex() const {
            JS_ASSERT(which_ == FFI);
            return u.ffiIndex_;
        }
        ArrayBufferView::ViewType viewType() const {
            JS_ASSERT(which_ == ArrayView);
            return u.viewType_;
        }
        AsmJSMathBuiltinFunction mathBuiltinFunction() const {
            JS_ASSERT(which_ == MathBuiltinFunction);
            return u.mathBuiltinFunc_;
        }
    };

    typedef js::Vector<const Func*> FuncPtrVector;

    class FuncPtrTable
    {
        Signature sig_;
        uint32_t mask_;
        uint32_t globalDataOffset_;
        FuncPtrVector elems_;

      public:
        FuncPtrTable(ExclusiveContext *cx, Signature &&sig, uint32_t mask, uint32_t gdo)
          : sig_(Move(sig)), mask_(mask), globalDataOffset_(gdo), elems_(cx)
        {}

        FuncPtrTable(FuncPtrTable &&rhs)
          : sig_(Move(rhs.sig_)), mask_(rhs.mask_), globalDataOffset_(rhs.globalDataOffset_),
            elems_(Move(rhs.elems_))
        {}

        Signature &sig() { return sig_; }
        const Signature &sig() const { return sig_; }
        unsigned mask() const { return mask_; }
        unsigned globalDataOffset() const { return globalDataOffset_; }

        bool initialized() const { return !elems_.empty(); }
        void initElems(FuncPtrVector &&elems) { elems_ = Move(elems); JS_ASSERT(initialized()); }
        unsigned numElems() const { JS_ASSERT(initialized()); return elems_.length(); }
        const Func &elem(unsigned i) const { return *elems_[i]; }
    };

    typedef js::Vector<FuncPtrTable> FuncPtrTableVector;

    class ExitDescriptor
    {
        PropertyName *name_;
        Signature sig_;

      public:
        ExitDescriptor(PropertyName *name, Signature &&sig)
          : name_(name), sig_(Move(sig)) {}
        ExitDescriptor(ExitDescriptor &&rhs)
          : name_(rhs.name_), sig_(Move(rhs.sig_))
        {}
        const Signature &sig() const {
            return sig_;
        }

        // ExitDescriptor is a HashPolicy:
        typedef ExitDescriptor Lookup;
        static HashNumber hash(const ExitDescriptor &d) {
            HashNumber hn = HashGeneric(d.name_, d.sig_.retType().which());
            const VarTypeVector &args = d.sig_.args();
            for (unsigned i = 0; i < args.length(); i++)
                hn = AddToHash(hn, args[i].which());
            return hn;
        }
        static bool match(const ExitDescriptor &lhs, const ExitDescriptor &rhs) {
            return lhs.name_ == rhs.name_ && lhs.sig_ == rhs.sig_;
        }
    };

    typedef HashMap<ExitDescriptor, unsigned, ExitDescriptor> ExitMap;

    struct MathBuiltin
    {
        enum Kind { Function, Constant };
        Kind kind;

        union {
            double cst;
            AsmJSMathBuiltinFunction func;
        } u;

        MathBuiltin() : kind(Kind(-1)) {}
        MathBuiltin(double cst) : kind(Constant) {
            u.cst = cst;
        }
        MathBuiltin(AsmJSMathBuiltinFunction func) : kind(Function) {
            u.func = func;
        }
    };

  private:
    struct SlowFunction
    {
        PropertyName *name;
        unsigned ms;
        unsigned line;
        unsigned column;
    };

    typedef HashMap<PropertyName*, MathBuiltin> MathNameMap;
    typedef HashMap<PropertyName*, Global*> GlobalMap;
    typedef js::Vector<Func*> FuncVector;
    typedef js::Vector<AsmJSGlobalAccess> GlobalAccessVector;
    typedef js::Vector<SlowFunction> SlowFunctionVector;

    ExclusiveContext *             cx_;
    AsmJSParser &                  parser_;

    MacroAssembler                 masm_;

    ScopedJSDeletePtr<AsmJSModule> module_;
    LifoAlloc                      moduleLifo_;
    ParseNode *                    moduleFunctionNode_;
    PropertyName *                 moduleFunctionName_;

    GlobalMap                      globals_;
    FuncVector                     functions_;
    FuncPtrTableVector             funcPtrTables_;
    ExitMap                        exits_;
    MathNameMap                    standardLibraryMathNames_;
    GlobalAccessVector             globalAccesses_;
    Label                          stackOverflowLabel_;
    Label                          interruptLabel_;

    char *                         errorString_;
    uint32_t                       errorOffset_;
    bool                           errorOverRecursed_;

    int64_t                        usecBefore_;
    SlowFunctionVector             slowFunctions_;

    DebugOnly<bool>                finishedFunctionBodies_;

    bool addStandardLibraryMathName(const char *name, AsmJSMathBuiltinFunction func) {
        JSAtom *atom = Atomize(cx_, name, strlen(name));
        if (!atom)
            return false;
        MathBuiltin builtin(func);
        return standardLibraryMathNames_.putNew(atom->asPropertyName(), builtin);
    }
    bool addStandardLibraryMathName(const char *name, double cst) {
        JSAtom *atom = Atomize(cx_, name, strlen(name));
        if (!atom)
            return false;
        MathBuiltin builtin(cst);
        return standardLibraryMathNames_.putNew(atom->asPropertyName(), builtin);
    }

  public:
    ModuleCompiler(ExclusiveContext *cx, AsmJSParser &parser)
      : cx_(cx),
        parser_(parser),
        masm_(MacroAssembler::AsmJSToken()),
        moduleLifo_(LIFO_ALLOC_PRIMARY_CHUNK_SIZE),
        moduleFunctionNode_(parser.pc->maybeFunction),
        moduleFunctionName_(nullptr),
        globals_(cx),
        functions_(cx),
        funcPtrTables_(cx),
        exits_(cx),
        standardLibraryMathNames_(cx),
        globalAccesses_(cx),
        errorString_(nullptr),
        errorOffset_(UINT32_MAX),
        errorOverRecursed_(false),
        usecBefore_(PRMJ_Now()),
        slowFunctions_(cx),
        finishedFunctionBodies_(false)
    {
        JS_ASSERT(moduleFunctionNode_->pn_funbox == parser.pc->sc->asFunctionBox());
    }

    ~ModuleCompiler() {
        if (errorString_) {
            JS_ASSERT(errorOffset_ != UINT32_MAX);
            parser_.tokenStream.reportAsmJSError(errorOffset_,
                                                 JSMSG_USE_ASM_TYPE_FAIL,
                                                 errorString_);
            js_free(errorString_);
        }
        if (errorOverRecursed_)
            js_ReportOverRecursed(cx_);

        // Avoid spurious Label assertions on compilation failure.
        if (!stackOverflowLabel_.bound())
            stackOverflowLabel_.bind(0);
        if (!interruptLabel_.bound())
            interruptLabel_.bind(0);
    }

    bool init() {
        if (!globals_.init() || !exits_.init())
            return false;

        if (!standardLibraryMathNames_.init() ||
            !addStandardLibraryMathName("sin", AsmJSMathBuiltin_sin) ||
            !addStandardLibraryMathName("cos", AsmJSMathBuiltin_cos) ||
            !addStandardLibraryMathName("tan", AsmJSMathBuiltin_tan) ||
            !addStandardLibraryMathName("asin", AsmJSMathBuiltin_asin) ||
            !addStandardLibraryMathName("acos", AsmJSMathBuiltin_acos) ||
            !addStandardLibraryMathName("atan", AsmJSMathBuiltin_atan) ||
            !addStandardLibraryMathName("ceil", AsmJSMathBuiltin_ceil) ||
            !addStandardLibraryMathName("floor", AsmJSMathBuiltin_floor) ||
            !addStandardLibraryMathName("exp", AsmJSMathBuiltin_exp) ||
            !addStandardLibraryMathName("log", AsmJSMathBuiltin_log) ||
            !addStandardLibraryMathName("pow", AsmJSMathBuiltin_pow) ||
            !addStandardLibraryMathName("sqrt", AsmJSMathBuiltin_sqrt) ||
            !addStandardLibraryMathName("abs", AsmJSMathBuiltin_abs) ||
            !addStandardLibraryMathName("atan2", AsmJSMathBuiltin_atan2) ||
            !addStandardLibraryMathName("imul", AsmJSMathBuiltin_imul) ||
            !addStandardLibraryMathName("fround", AsmJSMathBuiltin_fround) ||
            !addStandardLibraryMathName("min", AsmJSMathBuiltin_min) ||
            !addStandardLibraryMathName("max", AsmJSMathBuiltin_max) ||
            !addStandardLibraryMathName("E", M_E) ||
            !addStandardLibraryMathName("LN10", M_LN10) ||
            !addStandardLibraryMathName("LN2", M_LN2) ||
            !addStandardLibraryMathName("LOG2E", M_LOG2E) ||
            !addStandardLibraryMathName("LOG10E", M_LOG10E) ||
            !addStandardLibraryMathName("PI", M_PI) ||
            !addStandardLibraryMathName("SQRT1_2", M_SQRT1_2) ||
            !addStandardLibraryMathName("SQRT2", M_SQRT2))
        {
            return false;
        }

        uint32_t funcStart = parser_.pc->maybeFunction->pn_body->pn_pos.begin;
        uint32_t offsetToEndOfUseAsm = parser_.tokenStream.currentToken().pos.end;
        module_ = cx_->new_<AsmJSModule>(parser_.ss, funcStart, offsetToEndOfUseAsm);
        if (!module_)
            return false;

        return true;
    }

    bool failOffset(uint32_t offset, const char *str) {
        JS_ASSERT(!errorString_);
        JS_ASSERT(errorOffset_ == UINT32_MAX);
        JS_ASSERT(str);
        errorOffset_ = offset;
        errorString_ = js_strdup(cx_, str);
        return false;
    }

    bool fail(ParseNode *pn, const char *str) {
        if (pn)
            return failOffset(pn->pn_pos.begin, str);

        // The exact rooting static analysis does not perform dataflow analysis, so it believes
        // that unrooted things on the stack during compilation may still be accessed after this.
        // Since pn is typically only null under OOM, this suppression simply forces any GC to be
        // delayed until the compilation is off the stack and more memory can be freed.
        gc::AutoSuppressGC nogc(cx_);
        return failOffset(parser_.tokenStream.peekTokenPos().begin, str);
    }

    bool failfVA(ParseNode *pn, const char *fmt, va_list ap) {
        JS_ASSERT(!errorString_);
        JS_ASSERT(errorOffset_ == UINT32_MAX);
        JS_ASSERT(fmt);
        errorOffset_ = pn ? pn->pn_pos.begin : parser_.tokenStream.currentToken().pos.end;
        errorString_ = JS_vsmprintf(fmt, ap);
        return false;
    }

    bool failf(ParseNode *pn, const char *fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        failfVA(pn, fmt, ap);
        va_end(ap);
        return false;
    }

    bool failName(ParseNode *pn, const char *fmt, PropertyName *name) {
        // This function is invoked without the caller properly rooting its locals.
        gc::AutoSuppressGC suppress(cx_);
        JSAutoByteString bytes;
        if (AtomToPrintableString(cx_, name, &bytes))
            failf(pn, fmt, bytes.ptr());
        return false;
    }

    bool failOverRecursed() {
        errorOverRecursed_ = true;
        return false;
    }

    static const unsigned SLOW_FUNCTION_THRESHOLD_MS = 250;

    bool maybeReportCompileTime(const Func &func) {
        if (func.compileTime() < SLOW_FUNCTION_THRESHOLD_MS)
            return true;
        SlowFunction sf;
        sf.name = func.name();
        sf.ms = func.compileTime();
        parser_.tokenStream.srcCoords.lineNumAndColumnIndex(func.srcOffset(), &sf.line, &sf.column);
        return slowFunctions_.append(sf);
    }

    /*************************************************** Read-only interface */

    ExclusiveContext *cx() const { return cx_; }
    AsmJSParser &parser() const { return parser_; }
    MacroAssembler &masm() { return masm_; }
    Label &stackOverflowLabel() { return stackOverflowLabel_; }
    Label &interruptLabel() { return interruptLabel_; }
    bool hasError() const { return errorString_ != nullptr; }
    const AsmJSModule &module() const { return *module_.get(); }

    ParseNode *moduleFunctionNode() const { return moduleFunctionNode_; }
    PropertyName *moduleFunctionName() const { return moduleFunctionName_; }

    const Global *lookupGlobal(PropertyName *name) const {
        if (GlobalMap::Ptr p = globals_.lookup(name))
            return p->value();
        return nullptr;
    }
    Func *lookupFunction(PropertyName *name) {
        if (GlobalMap::Ptr p = globals_.lookup(name)) {
            Global *value = p->value();
            if (value->which() == Global::Function)
                return functions_[value->funcIndex()];
        }
        return nullptr;
    }
    unsigned numFunctions() const {
        return functions_.length();
    }
    Func &function(unsigned i) {
        return *functions_[i];
    }
    unsigned numFuncPtrTables() const {
        return funcPtrTables_.length();
    }
    FuncPtrTable &funcPtrTable(unsigned i) {
        return funcPtrTables_[i];
    }
    bool lookupStandardLibraryMathName(PropertyName *name, MathBuiltin *mathBuiltin) const {
        if (MathNameMap::Ptr p = standardLibraryMathNames_.lookup(name)) {
            *mathBuiltin = p->value();
            return true;
        }
        return false;
    }
    ExitMap::Range allExits() const {
        return exits_.all();
    }

    /***************************************************** Mutable interface */

    void initModuleFunctionName(PropertyName *name) { moduleFunctionName_ = name; }

    void initGlobalArgumentName(PropertyName *n) { module_->initGlobalArgumentName(n); }
    void initImportArgumentName(PropertyName *n) { module_->initImportArgumentName(n); }
    void initBufferArgumentName(PropertyName *n) { module_->initBufferArgumentName(n); }

    bool addGlobalVarInit(PropertyName *varName, VarType type, const Value &v, bool isConst) {
        uint32_t index;
        if (!module_->addGlobalVarInit(v, type.toCoercion(), &index))
            return false;

        Global::Which which = isConst ? Global::ConstantLiteral : Global::Variable;
        Global *global = moduleLifo_.new_<Global>(which);
        if (!global)
            return false;
        global->u.varOrConst.index_ = index;
        global->u.varOrConst.type_ = type.which();
        if (isConst)
            global->u.varOrConst.literalValue_ = v;

        return globals_.putNew(varName, global);
    }
    bool addGlobalVarImport(PropertyName *varName, PropertyName *fieldName, AsmJSCoercion coercion,
                            bool isConst) {
        uint32_t index;
        if (!module_->addGlobalVarImport(fieldName, coercion, &index))
            return false;

        Global::Which which = isConst ? Global::ConstantImport : Global::Variable;
        Global *global = moduleLifo_.new_<Global>(which);
        if (!global)
            return false;
        global->u.varOrConst.index_ = index;
        global->u.varOrConst.type_ = VarType(coercion).which();

        return globals_.putNew(varName, global);
    }
    bool addFunction(PropertyName *name, Signature &&sig, Func **func) {
        JS_ASSERT(!finishedFunctionBodies_);
        Global *global = moduleLifo_.new_<Global>(Global::Function);
        if (!global)
            return false;
        global->u.funcIndex_ = functions_.length();
        if (!globals_.putNew(name, global))
            return false;
        Label *code = moduleLifo_.new_<Label>();
        if (!code)
            return false;
        *func = moduleLifo_.new_<Func>(name, Move(sig), code);
        if (!*func)
            return false;
        return functions_.append(*func);
    }
    bool addFuncPtrTable(PropertyName *name, Signature &&sig, uint32_t mask, FuncPtrTable **table) {
        Global *global = moduleLifo_.new_<Global>(Global::FuncPtrTable);
        if (!global)
            return false;
        global->u.funcPtrTableIndex_ = funcPtrTables_.length();
        if (!globals_.putNew(name, global))
            return false;
        uint32_t globalDataOffset;
        if (!module_->addFuncPtrTable(/* numElems = */ mask + 1, &globalDataOffset))
            return false;
        FuncPtrTable tmpTable(cx_, Move(sig), mask, globalDataOffset);
        if (!funcPtrTables_.append(Move(tmpTable)))
            return false;
        *table = &funcPtrTables_.back();
        return true;
    }
    bool addFFI(PropertyName *varName, PropertyName *field) {
        Global *global = moduleLifo_.new_<Global>(Global::FFI);
        if (!global)
            return false;
        uint32_t index;
        if (!module_->addFFI(field, &index))
            return false;
        global->u.ffiIndex_ = index;
        return globals_.putNew(varName, global);
    }
    bool addArrayView(PropertyName *varName, ArrayBufferView::ViewType vt, PropertyName *fieldName) {
        Global *global = moduleLifo_.new_<Global>(Global::ArrayView);
        if (!global)
            return false;
        if (!module_->addArrayView(vt, fieldName))
            return false;
        global->u.viewType_ = vt;
        return globals_.putNew(varName, global);
    }
    bool addMathBuiltinFunction(PropertyName *varName, AsmJSMathBuiltinFunction func, PropertyName *fieldName) {
        if (!module_->addMathBuiltinFunction(func, fieldName))
            return false;
        Global *global = moduleLifo_.new_<Global>(Global::MathBuiltinFunction);
        if (!global)
            return false;
        global->u.mathBuiltinFunc_ = func;
        return globals_.putNew(varName, global);
    }
  private:
    bool addGlobalDoubleConstant(PropertyName *varName, double constant) {
        Global *global = moduleLifo_.new_<Global>(Global::ConstantLiteral);
        if (!global)
            return false;
        global->u.varOrConst.literalValue_ = DoubleValue(constant);
        global->u.varOrConst.type_ = VarType::Double;
        return globals_.putNew(varName, global);
    }
  public:
    bool addMathBuiltinConstant(PropertyName *varName, double constant, PropertyName *fieldName) {
        if (!module_->addMathBuiltinConstant(constant, fieldName))
            return false;
        return addGlobalDoubleConstant(varName, constant);
    }
    bool addGlobalConstant(PropertyName *varName, double constant, PropertyName *fieldName) {
        if (!module_->addGlobalConstant(constant, fieldName))
            return false;
        return addGlobalDoubleConstant(varName, constant);
    }
    bool addExportedFunction(const Func *func, PropertyName *maybeFieldName) {
        AsmJSModule::ArgCoercionVector argCoercions;
        const VarTypeVector &args = func->sig().args();
        if (!argCoercions.resize(args.length()))
            return false;
        for (unsigned i = 0; i < args.length(); i++)
            argCoercions[i] = args[i].toCoercion();
        AsmJSModule::ReturnType retType = func->sig().retType().toModuleReturnType();
        uint32_t line, column;
        parser_.tokenStream.srcCoords.lineNumAndColumnIndex(func->srcOffset(), &line, &column);
        return module_->addExportedFunction(func->name(), line, column, maybeFieldName,
                                            Move(argCoercions), retType);
    }
    bool addExit(unsigned ffiIndex, PropertyName *name, Signature &&sig, unsigned *exitIndex) {
        ExitDescriptor exitDescriptor(name, Move(sig));
        ExitMap::AddPtr p = exits_.lookupForAdd(exitDescriptor);
        if (p) {
            *exitIndex = p->value();
            return true;
        }
        if (!module_->addExit(ffiIndex, exitIndex))
            return false;
        return exits_.add(p, Move(exitDescriptor), *exitIndex);
    }
    bool addGlobalAccess(AsmJSGlobalAccess access) {
        return globalAccesses_.append(access);
    }

    // Note a constraint on the minimum size of the heap.  The heap size is
    // constrained when linking to be at least the maximum of all such constraints.
    void requireHeapLengthToBeAtLeast(uint32_t len) {
        module_->requireHeapLengthToBeAtLeast(len);
    }
    uint32_t minHeapLength() const {
        return module_->minHeapLength();
    }
    LifoAlloc &lifo() {
        return moduleLifo_;
    }

    bool collectAccesses(MIRGenerator &gen) {
        if (!module_->addHeapAccesses(gen.heapAccesses()))
            return false;
        if (!globalAccesses_.appendAll(gen.globalAccesses()))
            return false;
        return true;
    }

#if defined(MOZ_VTUNE) || defined(JS_ION_PERF)
    bool trackProfiledFunction(const Func &func, unsigned endCodeOffset) {
        unsigned lineno = 0U, columnIndex = 0U;
        parser().tokenStream.srcCoords.lineNumAndColumnIndex(func.srcOffset(), &lineno, &columnIndex);
        unsigned startCodeOffset = func.code()->offset();
        return module_->trackProfiledFunction(func.name(), startCodeOffset, endCodeOffset,
                                              lineno, columnIndex);
    }
#endif

#ifdef JS_ION_PERF
    bool trackPerfProfiledBlocks(AsmJSPerfSpewer &perfSpewer, const Func &func, unsigned endCodeOffset) {
        unsigned startCodeOffset = func.code()->offset();
        perfSpewer.noteBlocksOffsets();
        unsigned endInlineCodeOffset = perfSpewer.endInlineCode.offset();
        return module_->trackPerfProfiledBlocks(func.name(), startCodeOffset, endInlineCodeOffset,
                                                endCodeOffset, perfSpewer.basicBlocks());
    }
#endif

    bool addFunctionCounts(IonScriptCounts *counts) {
        return module_->addFunctionCounts(counts);
    }

    void finishFunctionBodies() {
        JS_ASSERT(!finishedFunctionBodies_);
        masm_.align(AsmJSPageSize);
        finishedFunctionBodies_ = true;
        module_->initFunctionBytes(masm_.size());
    }

    void setInterpExitOffset(unsigned exitIndex) {
#if defined(JS_CODEGEN_ARM)
        masm_.flush();
#endif
        module_->exit(exitIndex).initInterpOffset(masm_.size());
    }
    void setIonExitOffset(unsigned exitIndex) {
#if defined(JS_CODEGEN_ARM)
        masm_.flush();
#endif
        module_->exit(exitIndex).initIonOffset(masm_.size());
    }
    void setEntryOffset(unsigned exportIndex) {
#if defined(JS_CODEGEN_ARM)
        masm_.flush();
#endif
        module_->exportedFunction(exportIndex).initCodeOffset(masm_.size());
    }

    void buildCompilationTimeReport(bool storedInCache, ScopedJSFreePtr<char> *out) {
        ScopedJSFreePtr<char> slowFuns;
#ifndef JS_MORE_DETERMINISTIC
        int64_t usecAfter = PRMJ_Now();
        int msTotal = (usecAfter - usecBefore_) / PRMJ_USEC_PER_MSEC;
        if (!slowFunctions_.empty()) {
            slowFuns.reset(JS_smprintf("; %d functions compiled slowly: ", slowFunctions_.length()));
            if (!slowFuns)
                return;
            for (unsigned i = 0; i < slowFunctions_.length(); i++) {
                SlowFunction &func = slowFunctions_[i];
                JSAutoByteString name;
                if (!AtomToPrintableString(cx_, func.name, &name))
                    return;
                slowFuns.reset(JS_smprintf("%s%s:%u:%u (%ums)%s", slowFuns.get(),
                                           name.ptr(), func.line, func.column, func.ms,
                                           i+1 < slowFunctions_.length() ? ", " : ""));
                if (!slowFuns)
                    return;
            }
        }
        out->reset(JS_smprintf("total compilation time %dms; %s%s",
                               msTotal,
                               storedInCache ? "stored in cache" : "not stored in cache",
                               slowFuns ? slowFuns.get() : ""));
#endif
    }

    bool finish(ScopedJSDeletePtr<AsmJSModule> *module)
    {
        module_->initFuncEnd(parser_.tokenStream.currentToken().pos.end,
                             parser_.tokenStream.peekTokenPos().end);

        masm_.finish();
        if (masm_.oom())
            return false;

#if defined(JS_CODEGEN_ARM)
        // Now that compilation has finished, we need to update offsets to
        // reflect actual offsets (an ARM distinction).
        for (unsigned i = 0; i < module_->numHeapAccesses(); i++) {
            AsmJSHeapAccess &a = module_->heapAccess(i);
            a.setOffset(masm_.actualOffset(a.offset()));
        }
#endif

        // The returned memory is owned by module_.
        if (!module_->allocateAndCopyCode(cx_, masm_))
            return false;

        // c.f. JitCode::copyFrom
        JS_ASSERT(masm_.jumpRelocationTableBytes() == 0);
        JS_ASSERT(masm_.dataRelocationTableBytes() == 0);
        JS_ASSERT(masm_.preBarrierTableBytes() == 0);
        JS_ASSERT(!masm_.hasEnteredExitFrame());

#if defined(MOZ_VTUNE) || defined(JS_ION_PERF)
        // Fix up the code offsets.  Note the endCodeOffset should not be
        // filtered through 'actualOffset' as it is generated using 'size()'
        // rather than a label.
        for (unsigned i = 0; i < module_->numProfiledFunctions(); i++) {
            AsmJSModule::ProfiledFunction &func = module_->profiledFunction(i);
            func.pod.startCodeOffset = masm_.actualOffset(func.pod.startCodeOffset);
        }
#endif

#ifdef JS_ION_PERF
        for (unsigned i = 0; i < module_->numPerfBlocksFunctions(); i++) {
            AsmJSModule::ProfiledBlocksFunction &func = module_->perfProfiledBlocksFunction(i);
            func.pod.startCodeOffset = masm_.actualOffset(func.pod.startCodeOffset);
            func.endInlineCodeOffset = masm_.actualOffset(func.endInlineCodeOffset);
            BasicBlocksVector &basicBlocks = func.blocks;
            for (uint32_t i = 0; i < basicBlocks.length(); i++) {
                Record &r = basicBlocks[i];
                r.startOffset = masm_.actualOffset(r.startOffset);
                r.endOffset = masm_.actualOffset(r.endOffset);
            }
        }
#endif

        module_->setInterruptOffset(masm_.actualOffset(interruptLabel_.offset()));

        // CodeLabels produced during codegen
        for (size_t i = 0; i < masm_.numCodeLabels(); i++) {
            CodeLabel src = masm_.codeLabel(i);
            int32_t labelOffset = src.dest()->offset();
            int32_t targetOffset = masm_.actualOffset(src.src()->offset());
            // The patched uses of a label embed a linked list where the
            // to-be-patched immediate is the offset of the next to-be-patched
            // instruction.
            while (labelOffset != LabelBase::INVALID_OFFSET) {
                size_t patchAtOffset = masm_.labelOffsetToPatchOffset(labelOffset);
                AsmJSModule::RelativeLink link;
                link.patchAtOffset = patchAtOffset;
                link.targetOffset = targetOffset;
                if (!module_->addRelativeLink(link))
                    return false;
                labelOffset = *(uintptr_t *)(module_->codeBase() + patchAtOffset);
            }
        }

        // Function-pointer-table entries
        for (unsigned tableIndex = 0; tableIndex < funcPtrTables_.length(); tableIndex++) {
            FuncPtrTable &table = funcPtrTables_[tableIndex];
            unsigned tableBaseOffset = module_->offsetOfGlobalData() + table.globalDataOffset();
            for (unsigned elemIndex = 0; elemIndex < table.numElems(); elemIndex++) {
                AsmJSModule::RelativeLink link;
                link.patchAtOffset = tableBaseOffset + elemIndex * sizeof(uint8_t*);
                link.targetOffset = masm_.actualOffset(table.elem(elemIndex).code()->offset());
                if (!module_->addRelativeLink(link))
                    return false;
            }
        }

#if defined(JS_CODEGEN_X86)
        // Global data accesses in x86 need to be patched with the absolute
        // address of the global. Globals are allocated sequentially after the
        // code section so we can just use an RelativeLink.
        for (unsigned i = 0; i < globalAccesses_.length(); i++) {
            AsmJSGlobalAccess a = globalAccesses_[i];
            AsmJSModule::RelativeLink link;
            link.patchAtOffset = masm_.labelOffsetToPatchOffset(a.patchAt.offset());
            link.targetOffset = module_->offsetOfGlobalData() + a.globalDataOffset;
            if (!module_->addRelativeLink(link))
                return false;
        }
#endif

#if defined(JS_CODEGEN_X64)
        // Global data accesses on x64 use rip-relative addressing and thus do
        // not need patching after deserialization.
        uint8_t *code = module_->codeBase();
        for (unsigned i = 0; i < globalAccesses_.length(); i++) {
            AsmJSGlobalAccess a = globalAccesses_[i];
            masm_.patchAsmJSGlobalAccess(a.patchAt, code, module_->globalData(), a.globalDataOffset);
        }
#endif

        // Absolute links
        for (size_t i = 0; i < masm_.numAsmJSAbsoluteLinks(); i++) {
            AsmJSAbsoluteLink src = masm_.asmJSAbsoluteLink(i);
            AsmJSModule::AbsoluteLink link;
            link.patchAt = masm_.actualOffset(src.patchAt.offset());
            link.target = src.target;
            if (!module_->addAbsoluteLink(link))
                return false;
        }

        *module = module_.forget();
        return true;
    }
};

} /* anonymous namespace */

/*****************************************************************************/
// Numeric literal utilities

namespace {

// Represents the type and value of an asm.js numeric literal.
//
// A literal is a double iff the literal contains an exponent or decimal point
// (even if the fractional part is 0). Otherwise, integers may be classified:
//  fixnum: [0, 2^31)
//  negative int: [-2^31, 0)
//  big unsigned: [2^31, 2^32)
//  out of range: otherwise
// Lastly, a literal may be a float literal which is any double or integer
// literal coerced with Math.fround.
class NumLit
{
  public:
    enum Which {
        Fixnum = Type::Fixnum,
        NegativeInt = Type::Signed,
        BigUnsigned = Type::Unsigned,
        Double = Type::Double,
        Float = Type::Float,
        OutOfRangeInt = -1
    };

  private:
    Which which_;
    Value v_;

  public:
    NumLit() {}

    NumLit(Which w, Value v)
      : which_(w), v_(v)
    {}

    Which which() const {
        return which_;
    }

    int32_t toInt32() const {
        JS_ASSERT(which_ == Fixnum || which_ == NegativeInt || which_ == BigUnsigned);
        return v_.toInt32();
    }

    double toDouble() const {
        JS_ASSERT(which_ == Double);
        return v_.toDouble();
    }

    float toFloat() const {
        JS_ASSERT(which_ == Float);
        return float(v_.toDouble());
    }

    Value value() const {
        JS_ASSERT(which_ != OutOfRangeInt);
        return v_;
    }

    bool hasType() const {
        return which_ != OutOfRangeInt;
    }

    Type type() const {
        JS_ASSERT(hasType());
        return Type::Which(which_);
    }

    VarType varType() const {
        JS_ASSERT(hasType());
        switch (which_) {
          case NumLit::Fixnum:
          case NumLit::NegativeInt:
          case NumLit::BigUnsigned:
            return VarType::Int;
          case NumLit::Double:
            return VarType::Double;
          case NumLit::Float:
            return VarType::Float;
          case NumLit::OutOfRangeInt:;
        }
        MOZ_ASSUME_UNREACHABLE("Unexpected NumLit type");
    }
};

} /* anonymous namespace */

static bool
IsNumericNonFloatLiteral(ParseNode *pn)
{
    // Note: '-' is never rolled into the number; numbers are always positive
    // and negations must be applied manually.
    return pn->isKind(PNK_NUMBER) ||
           (pn->isKind(PNK_NEG) && UnaryKid(pn)->isKind(PNK_NUMBER));
}

static bool
IsFloatCoercion(ModuleCompiler &m, ParseNode *pn, ParseNode **coercedExpr)
{
    if (!pn->isKind(PNK_CALL))
        return false;

    ParseNode *callee = CallCallee(pn);
    if (!callee->isKind(PNK_NAME))
        return false;

    const ModuleCompiler::Global *global = m.lookupGlobal(callee->name());
    if (!global ||
        global->which() != ModuleCompiler::Global::MathBuiltinFunction ||
        global->mathBuiltinFunction() != AsmJSMathBuiltin_fround)
    {
        return false;
    }

    if (CallArgListLength(pn) != 1)
        return false;

    if (coercedExpr)
        *coercedExpr = CallArgList(pn);

    return true;
}

static bool
IsNumericFloatLiteral(ModuleCompiler &m, ParseNode *pn)
{
    ParseNode *coercedExpr;
    if (!IsFloatCoercion(m, pn, &coercedExpr))
        return false;

    return IsNumericNonFloatLiteral(coercedExpr);
}

static bool
IsNumericLiteral(ModuleCompiler &m, ParseNode *pn)
{
    return IsNumericNonFloatLiteral(pn) ||
           IsNumericFloatLiteral(m, pn);
}

// The JS grammar treats -42 as -(42) (i.e., with separate grammar
// productions) for the unary - and literal 42). However, the asm.js spec
// recognizes -42 (modulo parens, so -(42) and -((42))) as a single literal
// so fold the two potential parse nodes into a single double value.
static double
ExtractNumericNonFloatValue(ParseNode **pn)
{
    JS_ASSERT(IsNumericNonFloatLiteral(*pn));

    if ((*pn)->isKind(PNK_NEG)) {
        *pn = UnaryKid(*pn);
        return -NumberNodeValue(*pn);
    }

    return NumberNodeValue(*pn);
}

static NumLit
ExtractNumericLiteral(ModuleCompiler &m, ParseNode *pn)
{
    JS_ASSERT(IsNumericLiteral(m, pn));

    // Float literals are explicitly coerced and thus the coerced literal may be
    // any valid (non-float) numeric literal.
    if (pn->isKind(PNK_CALL)) {
        pn = CallArgList(pn);
        double d = ExtractNumericNonFloatValue(&pn);
        return NumLit(NumLit::Float, DoubleValue(d));
    }

    double d = ExtractNumericNonFloatValue(&pn);

    // The asm.js spec syntactically distinguishes any literal containing a
    // decimal point or the literal -0 as having double type.
    if (NumberNodeHasFrac(pn) || IsNegativeZero(d))
        return NumLit(NumLit::Double, DoubleValue(d));

    // The syntactic checks above rule out these double values.
    JS_ASSERT(!IsNegativeZero(d));
    JS_ASSERT(!IsNaN(d));

    // Although doubles can only *precisely* represent 53-bit integers, they
    // can *imprecisely* represent integers much bigger than an int64_t.
    // Furthermore, d may be inf or -inf. In both cases, casting to an int64_t
    // is undefined, so test against the integer bounds using doubles.
    if (d < double(INT32_MIN) || d > double(UINT32_MAX))
        return NumLit(NumLit::OutOfRangeInt, UndefinedValue());

    // With the above syntactic and range limitations, d is definitely an
    // integer in the range [INT32_MIN, UINT32_MAX] range.
    int64_t i64 = int64_t(d);
    if (i64 >= 0) {
        if (i64 <= INT32_MAX)
            return NumLit(NumLit::Fixnum, Int32Value(i64));
        JS_ASSERT(i64 <= UINT32_MAX);
        return NumLit(NumLit::BigUnsigned, Int32Value(uint32_t(i64)));
    }
    JS_ASSERT(i64 >= INT32_MIN);
    return NumLit(NumLit::NegativeInt, Int32Value(i64));
}

static inline bool
IsLiteralInt(ModuleCompiler &m, ParseNode *pn, uint32_t *u32)
{
    if (!IsNumericLiteral(m, pn))
        return false;

    NumLit literal = ExtractNumericLiteral(m, pn);
    switch (literal.which()) {
      case NumLit::Fixnum:
      case NumLit::BigUnsigned:
      case NumLit::NegativeInt:
        *u32 = uint32_t(literal.toInt32());
        return true;
      case NumLit::Double:
      case NumLit::Float:
      case NumLit::OutOfRangeInt:
        return false;
    }

    MOZ_ASSUME_UNREACHABLE("Bad literal type");
}

/*****************************************************************************/

namespace {

// Encapsulates the compilation of a single function in an asm.js module. The
// function compiler handles the creation and final backend compilation of the
// MIR graph. Also see ModuleCompiler comment.
class FunctionCompiler
{
  public:
    struct Local
    {
        VarType type;
        unsigned slot;
        Local(VarType t, unsigned slot) : type(t), slot(slot) {}
    };

    struct TypedValue
    {
        VarType type;
        Value value;
        TypedValue(VarType t, const Value &v) : type(t), value(v) {}
    };

  private:
    typedef HashMap<PropertyName*, Local> LocalMap;
    typedef js::Vector<TypedValue> VarInitializerVector;
    typedef HashMap<PropertyName*, BlockVector> LabeledBlockMap;
    typedef HashMap<ParseNode*, BlockVector> UnlabeledBlockMap;
    typedef js::Vector<ParseNode*, 4> NodeStack;

    ModuleCompiler &       m_;
    LifoAlloc &            lifo_;
    ParseNode *            fn_;

    LocalMap               locals_;
    VarInitializerVector   varInitializers_;
    Maybe<RetType>         alreadyReturned_;

    TempAllocator *        alloc_;
    MIRGraph *             graph_;
    CompileInfo *          info_;
    MIRGenerator *         mirGen_;
    Maybe<IonContext>      ionContext_;

    MBasicBlock *          curBlock_;

    NodeStack              loopStack_;
    NodeStack              breakableStack_;
    UnlabeledBlockMap      unlabeledBreaks_;
    UnlabeledBlockMap      unlabeledContinues_;
    LabeledBlockMap        labeledBreaks_;
    LabeledBlockMap        labeledContinues_;

  public:
    FunctionCompiler(ModuleCompiler &m, ParseNode *fn, LifoAlloc &lifo)
      : m_(m),
        lifo_(lifo),
        fn_(fn),
        locals_(m.cx()),
        varInitializers_(m.cx()),
        alloc_(nullptr),
        graph_(nullptr),
        info_(nullptr),
        mirGen_(nullptr),
        curBlock_(nullptr),
        loopStack_(m.cx()),
        breakableStack_(m.cx()),
        unlabeledBreaks_(m.cx()),
        unlabeledContinues_(m.cx()),
        labeledBreaks_(m.cx()),
        labeledContinues_(m.cx())
    {}

    ModuleCompiler &    m() const      { return m_; }
    TempAllocator &     alloc() const  { return *alloc_; }
    LifoAlloc &         lifo() const   { return lifo_; }
    ParseNode *         fn() const     { return fn_; }
    ExclusiveContext *  cx() const     { return m_.cx(); }
    const AsmJSModule & module() const { return m_.module(); }

    bool init()
    {
        return locals_.init() &&
               unlabeledBreaks_.init() &&
               unlabeledContinues_.init() &&
               labeledBreaks_.init() &&
               labeledContinues_.init();
    }

    bool fail(ParseNode *pn, const char *str)
    {
        return m_.fail(pn, str);
    }

    bool failf(ParseNode *pn, const char *fmt, ...)
    {
        va_list ap;
        va_start(ap, fmt);
        m_.failfVA(pn, fmt, ap);
        va_end(ap);
        return false;
    }

    bool failName(ParseNode *pn, const char *fmt, PropertyName *name)
    {
        return m_.failName(pn, fmt, name);
    }

    ~FunctionCompiler()
    {
#ifdef DEBUG
        if (!m().hasError() && cx()->isJSContext() && !cx()->asJSContext()->isExceptionPending()) {
            JS_ASSERT(loopStack_.empty());
            JS_ASSERT(unlabeledBreaks_.empty());
            JS_ASSERT(unlabeledContinues_.empty());
            JS_ASSERT(labeledBreaks_.empty());
            JS_ASSERT(labeledContinues_.empty());
            JS_ASSERT(inDeadCode());
        }
#endif
    }

    /***************************************************** Local scope setup */

    bool addFormal(ParseNode *pn, PropertyName *name, VarType type)
    {
        LocalMap::AddPtr p = locals_.lookupForAdd(name);
        if (p)
            return failName(pn, "duplicate local name '%s' not allowed", name);
        return locals_.add(p, name, Local(type, locals_.count()));
    }

    bool addVariable(ParseNode *pn, PropertyName *name, VarType type, const Value &init)
    {
        LocalMap::AddPtr p = locals_.lookupForAdd(name);
        if (p)
            return failName(pn, "duplicate local name '%s' not allowed", name);
        if (!locals_.add(p, name, Local(type, locals_.count())))
            return false;
        return varInitializers_.append(TypedValue(type, init));
    }

    bool prepareToEmitMIR(const VarTypeVector &argTypes)
    {
        JS_ASSERT(locals_.count() == argTypes.length() + varInitializers_.length());

        alloc_  = lifo_.new_<TempAllocator>(&lifo_);
        ionContext_.construct(m_.cx(), alloc_);

        graph_  = lifo_.new_<MIRGraph>(alloc_);
        info_   = lifo_.new_<CompileInfo>(locals_.count(), SequentialExecution);
        const OptimizationInfo *optimizationInfo = js_IonOptimizations.get(Optimization_AsmJS);
        const JitCompileOptions options;
        mirGen_ = lifo_.new_<MIRGenerator>(CompileCompartment::get(cx()->compartment()),
                                           options, alloc_,
                                           graph_, info_, optimizationInfo);

        if (!newBlock(/* pred = */ nullptr, &curBlock_, fn_))
            return false;

        curBlock_->add(MAsmJSCheckOverRecursed::New(alloc(), &m_.stackOverflowLabel()));

        for (ABIArgTypeIter i = argTypes; !i.done(); i++) {
            MAsmJSParameter *ins = MAsmJSParameter::New(alloc(), *i, i.mirType());
            curBlock_->add(ins);
            curBlock_->initSlot(info().localSlot(i.index()), ins);
            if (!mirGen_->ensureBallast())
                return false;
        }
        unsigned firstLocalSlot = argTypes.length();
        for (unsigned i = 0; i < varInitializers_.length(); i++) {
            MConstant *ins = MConstant::NewAsmJS(alloc(), varInitializers_[i].value,
                                                 varInitializers_[i].type.toMIRType());
            curBlock_->add(ins);
            curBlock_->initSlot(info().localSlot(firstLocalSlot + i), ins);
            if (!mirGen_->ensureBallast())
                return false;
        }
        return true;
    }

    /******************************* For consistency of returns in a function */

    bool hasAlreadyReturned() const {
        return !alreadyReturned_.empty();
    }

    RetType returnedType() const {
        return alreadyReturned_.ref();
    }

    void setReturnedType(RetType retType) {
        alreadyReturned_.construct(retType);
    }

    /************************* Read-only interface (after local scope setup) */

    MIRGenerator & mirGen() const     { JS_ASSERT(mirGen_); return *mirGen_; }
    MIRGraph &     mirGraph() const   { JS_ASSERT(graph_); return *graph_; }
    CompileInfo &  info() const       { JS_ASSERT(info_); return *info_; }

    const Local *lookupLocal(PropertyName *name) const
    {
        if (LocalMap::Ptr p = locals_.lookup(name))
            return &p->value();
        return nullptr;
    }

    MDefinition *getLocalDef(const Local &local)
    {
        if (inDeadCode())
            return nullptr;
        return curBlock_->getSlot(info().localSlot(local.slot));
    }

    const ModuleCompiler::Global *lookupGlobal(PropertyName *name) const
    {
        if (locals_.has(name))
            return nullptr;
        return m_.lookupGlobal(name);
    }

    /***************************** Code generation (after local scope setup) */

    MDefinition *constant(Value v, Type t)
    {
        if (inDeadCode())
            return nullptr;
        MConstant *constant = MConstant::NewAsmJS(alloc(), v, t.toMIRType());
        curBlock_->add(constant);
        return constant;
    }

    template <class T>
    MDefinition *unary(MDefinition *op)
    {
        if (inDeadCode())
            return nullptr;
        T *ins = T::NewAsmJS(alloc(), op);
        curBlock_->add(ins);
        return ins;
    }

    template <class T>
    MDefinition *unary(MDefinition *op, MIRType type)
    {
        if (inDeadCode())
            return nullptr;
        T *ins = T::NewAsmJS(alloc(), op, type);
        curBlock_->add(ins);
        return ins;
    }

    template <class T>
    MDefinition *binary(MDefinition *lhs, MDefinition *rhs)
    {
        if (inDeadCode())
            return nullptr;
        T *ins = T::New(alloc(), lhs, rhs);
        curBlock_->add(ins);
        return ins;
    }

    template <class T>
    MDefinition *binary(MDefinition *lhs, MDefinition *rhs, MIRType type)
    {
        if (inDeadCode())
            return nullptr;
        T *ins = T::NewAsmJS(alloc(), lhs, rhs, type);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition *minMax(MDefinition *lhs, MDefinition *rhs, MIRType type, bool isMax) {
        if (inDeadCode())
            return nullptr;
        MMinMax *ins = MMinMax::New(alloc(), lhs, rhs, type, isMax);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition *mul(MDefinition *lhs, MDefinition *rhs, MIRType type, MMul::Mode mode)
    {
        if (inDeadCode())
            return nullptr;
        MMul *ins = MMul::New(alloc(), lhs, rhs, type, mode);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition *div(MDefinition *lhs, MDefinition *rhs, MIRType type, bool unsignd)
    {
        if (inDeadCode())
            return nullptr;
        MDiv *ins = MDiv::NewAsmJS(alloc(), lhs, rhs, type, unsignd);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition *mod(MDefinition *lhs, MDefinition *rhs, MIRType type, bool unsignd)
    {
        if (inDeadCode())
            return nullptr;
        MMod *ins = MMod::NewAsmJS(alloc(), lhs, rhs, type, unsignd);
        curBlock_->add(ins);
        return ins;
    }

    template <class T>
    MDefinition *bitwise(MDefinition *lhs, MDefinition *rhs)
    {
        if (inDeadCode())
            return nullptr;
        T *ins = T::NewAsmJS(alloc(), lhs, rhs);
        curBlock_->add(ins);
        return ins;
    }

    template <class T>
    MDefinition *bitwise(MDefinition *op)
    {
        if (inDeadCode())
            return nullptr;
        T *ins = T::NewAsmJS(alloc(), op);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition *compare(MDefinition *lhs, MDefinition *rhs, JSOp op, MCompare::CompareType type)
    {
        if (inDeadCode())
            return nullptr;
        MCompare *ins = MCompare::NewAsmJS(alloc(), lhs, rhs, op, type);
        curBlock_->add(ins);
        return ins;
    }

    void assign(const Local &local, MDefinition *def)
    {
        if (inDeadCode())
            return;
        curBlock_->setSlot(info().localSlot(local.slot), def);
    }

    MDefinition *loadHeap(ArrayBufferView::ViewType vt, MDefinition *ptr, NeedsBoundsCheck chk)
    {
        if (inDeadCode())
            return nullptr;
        MAsmJSLoadHeap *load = MAsmJSLoadHeap::New(alloc(), vt, ptr);
        curBlock_->add(load);
        if (chk == NO_BOUNDS_CHECK)
            load->setSkipBoundsCheck(true);
        return load;
    }

    void storeHeap(ArrayBufferView::ViewType vt, MDefinition *ptr, MDefinition *v, NeedsBoundsCheck chk)
    {
        if (inDeadCode())
            return;
        MAsmJSStoreHeap *store = MAsmJSStoreHeap::New(alloc(), vt, ptr, v);
        curBlock_->add(store);
        if (chk == NO_BOUNDS_CHECK)
            store->setSkipBoundsCheck(true);
    }

    MDefinition *loadGlobalVar(const ModuleCompiler::Global &global)
    {
        if (inDeadCode())
            return nullptr;

        uint32_t index = global.varOrConstIndex();
        unsigned globalDataOffset = module().globalVarIndexToGlobalDataOffset(index);
        MIRType type = global.varOrConstType().toMIRType();
        MAsmJSLoadGlobalVar *load = MAsmJSLoadGlobalVar::New(alloc(), type, globalDataOffset,
                                                             global.isConst());
        curBlock_->add(load);
        return load;
    }

    void storeGlobalVar(const ModuleCompiler::Global &global, MDefinition *v)
    {
        if (inDeadCode())
            return;
        JS_ASSERT(!global.isConst());
        unsigned globalDataOffset = module().globalVarIndexToGlobalDataOffset(global.varOrConstIndex());
        curBlock_->add(MAsmJSStoreGlobalVar::New(alloc(), globalDataOffset, v));
    }

    /***************************************************************** Calls */

    // The IonMonkey backend maintains a single stack offset (from the stack
    // pointer to the base of the frame) by adding the total amount of spill
    // space required plus the maximum stack required for argument passing.
    // Since we do not use IonMonkey's MPrepareCall/MPassArg/MCall, we must
    // manually accumulate, for the entire function, the maximum required stack
    // space for argument passing. (This is passed to the CodeGenerator via
    // MIRGenerator::maxAsmJSStackArgBytes.) Naively, this would just be the
    // maximum of the stack space required for each individual call (as
    // determined by the call ABI). However, as an optimization, arguments are
    // stored to the stack immediately after evaluation (to decrease live
    // ranges and reduce spilling). This introduces the complexity that,
    // between evaluating an argument and making the call, another argument
    // evaluation could perform a call that also needs to store to the stack.
    // When this occurs childClobbers_ = true and the parent expression's
    // arguments are stored above the maximum depth clobbered by a child
    // expression.

    class Call
    {
        ABIArgGenerator abi_;
        uint32_t prevMaxStackBytes_;
        uint32_t maxChildStackBytes_;
        uint32_t spIncrement_;
        Signature sig_;
        MAsmJSCall::Args regArgs_;
        js::Vector<MAsmJSPassStackArg*> stackArgs_;
        bool childClobbers_;

        friend class FunctionCompiler;

      public:
        Call(FunctionCompiler &f, RetType retType)
          : prevMaxStackBytes_(0),
            maxChildStackBytes_(0),
            spIncrement_(0),
            sig_(f.m().lifo(), retType),
            regArgs_(f.cx()),
            stackArgs_(f.cx()),
            childClobbers_(false)
        {}
        Signature &sig() { return sig_; }
        const Signature &sig() const { return sig_; }
    };

    void startCallArgs(Call *call)
    {
        if (inDeadCode())
            return;
        call->prevMaxStackBytes_ = mirGen().resetAsmJSMaxStackArgBytes();
    }

    bool passArg(MDefinition *argDef, VarType type, Call *call)
    {
        if (!call->sig().appendArg(type))
            return false;

        if (inDeadCode())
            return true;

        uint32_t childStackBytes = mirGen().resetAsmJSMaxStackArgBytes();
        call->maxChildStackBytes_ = Max(call->maxChildStackBytes_, childStackBytes);
        if (childStackBytes > 0 && !call->stackArgs_.empty())
            call->childClobbers_ = true;

        ABIArg arg = call->abi_.next(type.toMIRType());
        if (arg.kind() == ABIArg::Stack) {
            MAsmJSPassStackArg *mir = MAsmJSPassStackArg::New(alloc(), arg.offsetFromArgBase(),
                                                              argDef);
            curBlock_->add(mir);
            if (!call->stackArgs_.append(mir))
                return false;
        } else {
            if (!call->regArgs_.append(MAsmJSCall::Arg(arg.reg(), argDef)))
                return false;
        }
        return true;
    }

    void finishCallArgs(Call *call)
    {
        if (inDeadCode())
            return;
        uint32_t parentStackBytes = call->abi_.stackBytesConsumedSoFar();
        uint32_t newStackBytes;
        if (call->childClobbers_) {
            call->spIncrement_ = AlignBytes(call->maxChildStackBytes_, StackAlignment);
            for (unsigned i = 0; i < call->stackArgs_.length(); i++)
                call->stackArgs_[i]->incrementOffset(call->spIncrement_);
            newStackBytes = Max(call->prevMaxStackBytes_,
                                call->spIncrement_ + parentStackBytes);
        } else {
            call->spIncrement_ = 0;
            newStackBytes = Max(call->prevMaxStackBytes_,
                                Max(call->maxChildStackBytes_, parentStackBytes));
        }
        mirGen_->setAsmJSMaxStackArgBytes(newStackBytes);
    }

  private:
    bool callPrivate(MAsmJSCall::Callee callee, const Call &call, MIRType returnType, MDefinition **def)
    {
        if (inDeadCode()) {
            *def = nullptr;
            return true;
        }
        MAsmJSCall *ins = MAsmJSCall::New(alloc(), callee, call.regArgs_, returnType,
                                          call.spIncrement_);
        if (!ins)
            return false;
        curBlock_->add(ins);
        *def = ins;
        return true;
    }

  public:
    bool internalCall(const ModuleCompiler::Func &func, const Call &call, MDefinition **def)
    {
        MIRType returnType = func.sig().retType().toMIRType();
        return callPrivate(MAsmJSCall::Callee(func.code()), call, returnType, def);
    }

    bool funcPtrCall(const ModuleCompiler::FuncPtrTable &table, MDefinition *index,
                     const Call &call, MDefinition **def)
    {
        if (inDeadCode()) {
            *def = nullptr;
            return true;
        }

        MConstant *mask = MConstant::New(alloc(), Int32Value(table.mask()));
        curBlock_->add(mask);
        MBitAnd *maskedIndex = MBitAnd::NewAsmJS(alloc(), index, mask);
        curBlock_->add(maskedIndex);
        MAsmJSLoadFuncPtr *ptrFun = MAsmJSLoadFuncPtr::New(alloc(), table.globalDataOffset(), maskedIndex);
        curBlock_->add(ptrFun);

        MIRType returnType = table.sig().retType().toMIRType();
        return callPrivate(MAsmJSCall::Callee(ptrFun), call, returnType, def);
    }

    bool ffiCall(unsigned exitIndex, const Call &call, MIRType returnType, MDefinition **def)
    {
        if (inDeadCode()) {
            *def = nullptr;
            return true;
        }

        JS_STATIC_ASSERT(offsetof(AsmJSModule::ExitDatum, exit) == 0);
        unsigned globalDataOffset = module().exitIndexToGlobalDataOffset(exitIndex);

        MAsmJSLoadFFIFunc *ptrFun = MAsmJSLoadFFIFunc::New(alloc(), globalDataOffset);
        curBlock_->add(ptrFun);

        return callPrivate(MAsmJSCall::Callee(ptrFun), call, returnType, def);
    }

    bool builtinCall(AsmJSImmKind builtin, const Call &call, MIRType returnType, MDefinition **def)
    {
        return callPrivate(MAsmJSCall::Callee(builtin), call, returnType, def);
    }

    /*********************************************** Control flow generation */

    inline bool inDeadCode() const {
        return curBlock_ == nullptr;
    }

    void returnExpr(MDefinition *expr)
    {
        if (inDeadCode())
            return;
        MAsmJSReturn *ins = MAsmJSReturn::New(alloc(), expr);
        curBlock_->end(ins);
        curBlock_ = nullptr;
    }

    void returnVoid()
    {
        if (inDeadCode())
            return;
        MAsmJSVoidReturn *ins = MAsmJSVoidReturn::New(alloc());
        curBlock_->end(ins);
        curBlock_ = nullptr;
    }

    bool branchAndStartThen(MDefinition *cond, MBasicBlock **thenBlock, MBasicBlock **elseBlock,
                            ParseNode *thenPn, ParseNode* elsePn)
    {
        if (inDeadCode())
            return true;

        bool hasThenBlock = *thenBlock != nullptr;
        bool hasElseBlock = *elseBlock != nullptr;

        if (!hasThenBlock && !newBlock(curBlock_, thenBlock, thenPn))
            return false;
        if (!hasElseBlock && !newBlock(curBlock_, elseBlock, thenPn))
            return false;

        curBlock_->end(MTest::New(alloc(), cond, *thenBlock, *elseBlock));

        // Only add as a predecessor if newBlock hasn't been called (as it does it for us)
        if (hasThenBlock && !(*thenBlock)->addPredecessor(alloc(), curBlock_))
            return false;
        if (hasElseBlock && !(*elseBlock)->addPredecessor(alloc(), curBlock_))
            return false;

        curBlock_ = *thenBlock;
        mirGraph().moveBlockToEnd(curBlock_);
        return true;
    }

    void assertCurrentBlockIs(MBasicBlock *block) {
        if (inDeadCode())
            return;
        JS_ASSERT(curBlock_ == block);
    }

    bool appendThenBlock(BlockVector *thenBlocks)
    {
        if (inDeadCode())
            return true;
        return thenBlocks->append(curBlock_);
    }

    bool joinIf(const BlockVector &thenBlocks, MBasicBlock *joinBlock)
    {
        if (!joinBlock)
            return true;
        JS_ASSERT_IF(curBlock_, thenBlocks.back() == curBlock_);
        for (size_t i = 0; i < thenBlocks.length(); i++) {
            thenBlocks[i]->end(MGoto::New(alloc(), joinBlock));
            if (!joinBlock->addPredecessor(alloc(), thenBlocks[i]))
                return false;
        }
        curBlock_ = joinBlock;
        mirGraph().moveBlockToEnd(curBlock_);
        return true;
    }

    void switchToElse(MBasicBlock *elseBlock)
    {
        if (!elseBlock)
            return;
        curBlock_ = elseBlock;
        mirGraph().moveBlockToEnd(curBlock_);
    }

    bool joinIfElse(const BlockVector &thenBlocks, ParseNode *pn)
    {
        if (inDeadCode() && thenBlocks.empty())
            return true;
        MBasicBlock *pred = curBlock_ ? curBlock_ : thenBlocks[0];
        MBasicBlock *join;
        if (!newBlock(pred, &join, pn))
            return false;
        if (curBlock_)
            curBlock_->end(MGoto::New(alloc(), join));
        for (size_t i = 0; i < thenBlocks.length(); i++) {
            thenBlocks[i]->end(MGoto::New(alloc(), join));
            if (pred == curBlock_ || i > 0) {
                if (!join->addPredecessor(alloc(), thenBlocks[i]))
                    return false;
            }
        }
        curBlock_ = join;
        return true;
    }

    void pushPhiInput(MDefinition *def)
    {
        if (inDeadCode())
            return;
        JS_ASSERT(curBlock_->stackDepth() == info().firstStackSlot());
        curBlock_->push(def);
    }

    MDefinition *popPhiOutput()
    {
        if (inDeadCode())
            return nullptr;
        JS_ASSERT(curBlock_->stackDepth() == info().firstStackSlot() + 1);
        return curBlock_->pop();
    }

    bool startPendingLoop(ParseNode *pn, MBasicBlock **loopEntry, ParseNode *bodyStmt)
    {
        if (!loopStack_.append(pn) || !breakableStack_.append(pn))
            return false;
        JS_ASSERT_IF(curBlock_, curBlock_->loopDepth() == loopStack_.length() - 1);
        if (inDeadCode()) {
            *loopEntry = nullptr;
            return true;
        }
        *loopEntry = MBasicBlock::NewAsmJS(mirGraph(), info(), curBlock_,
                                           MBasicBlock::PENDING_LOOP_HEADER);
        if (!*loopEntry)
            return false;
        mirGraph().addBlock(*loopEntry);
        noteBasicBlockPosition(*loopEntry, bodyStmt);
        (*loopEntry)->setLoopDepth(loopStack_.length());
        curBlock_->end(MGoto::New(alloc(), *loopEntry));
        curBlock_ = *loopEntry;
        return true;
    }

    bool branchAndStartLoopBody(MDefinition *cond, MBasicBlock **afterLoop, ParseNode *bodyPn, ParseNode *afterPn)
    {
        if (inDeadCode()) {
            *afterLoop = nullptr;
            return true;
        }
        JS_ASSERT(curBlock_->loopDepth() > 0);
        MBasicBlock *body;
        if (!newBlock(curBlock_, &body, bodyPn))
            return false;
        if (cond->isConstant() && cond->toConstant()->valueToBoolean()) {
            *afterLoop = nullptr;
            curBlock_->end(MGoto::New(alloc(), body));
        } else {
            if (!newBlockWithDepth(curBlock_, curBlock_->loopDepth() - 1, afterLoop, afterPn))
                return false;
            curBlock_->end(MTest::New(alloc(), cond, body, *afterLoop));
        }
        curBlock_ = body;
        return true;
    }

  private:
    ParseNode *popLoop()
    {
        ParseNode *pn = loopStack_.popCopy();
        JS_ASSERT(!unlabeledContinues_.has(pn));
        breakableStack_.popBack();
        return pn;
    }

  public:
    bool closeLoop(MBasicBlock *loopEntry, MBasicBlock *afterLoop)
    {
        ParseNode *pn = popLoop();
        if (!loopEntry) {
            JS_ASSERT(!afterLoop);
            JS_ASSERT(inDeadCode());
            JS_ASSERT(!unlabeledBreaks_.has(pn));
            return true;
        }
        JS_ASSERT(loopEntry->loopDepth() == loopStack_.length() + 1);
        JS_ASSERT_IF(afterLoop, afterLoop->loopDepth() == loopStack_.length());
        if (curBlock_) {
            JS_ASSERT(curBlock_->loopDepth() == loopStack_.length() + 1);
            curBlock_->end(MGoto::New(alloc(), loopEntry));
            if (!loopEntry->setBackedgeAsmJS(curBlock_))
                return false;
        }
        curBlock_ = afterLoop;
        if (curBlock_)
            mirGraph().moveBlockToEnd(curBlock_);
        return bindUnlabeledBreaks(pn);
    }

    bool branchAndCloseDoWhileLoop(MDefinition *cond, MBasicBlock *loopEntry, ParseNode *afterLoopStmt)
    {
        ParseNode *pn = popLoop();
        if (!loopEntry) {
            JS_ASSERT(inDeadCode());
            JS_ASSERT(!unlabeledBreaks_.has(pn));
            return true;
        }
        JS_ASSERT(loopEntry->loopDepth() == loopStack_.length() + 1);
        if (curBlock_) {
            JS_ASSERT(curBlock_->loopDepth() == loopStack_.length() + 1);
            if (cond->isConstant()) {
                if (cond->toConstant()->valueToBoolean()) {
                    curBlock_->end(MGoto::New(alloc(), loopEntry));
                    if (!loopEntry->setBackedgeAsmJS(curBlock_))
                        return false;
                    curBlock_ = nullptr;
                } else {
                    MBasicBlock *afterLoop;
                    if (!newBlock(curBlock_, &afterLoop, afterLoopStmt))
                        return false;
                    curBlock_->end(MGoto::New(alloc(), afterLoop));
                    curBlock_ = afterLoop;
                }
            } else {
                MBasicBlock *afterLoop;
                if (!newBlock(curBlock_, &afterLoop, afterLoopStmt))
                    return false;
                curBlock_->end(MTest::New(alloc(), cond, loopEntry, afterLoop));
                if (!loopEntry->setBackedgeAsmJS(curBlock_))
                    return false;
                curBlock_ = afterLoop;
            }
        }
        return bindUnlabeledBreaks(pn);
    }

    bool bindContinues(ParseNode *pn, const LabelVector *maybeLabels)
    {
        bool createdJoinBlock = false;
        if (UnlabeledBlockMap::Ptr p = unlabeledContinues_.lookup(pn)) {
            if (!bindBreaksOrContinues(&p->value(), &createdJoinBlock, pn))
                return false;
            unlabeledContinues_.remove(p);
        }
        return bindLabeledBreaksOrContinues(maybeLabels, &labeledContinues_, &createdJoinBlock, pn);
    }

    bool bindLabeledBreaks(const LabelVector *maybeLabels, ParseNode *pn)
    {
        bool createdJoinBlock = false;
        return bindLabeledBreaksOrContinues(maybeLabels, &labeledBreaks_, &createdJoinBlock, pn);
    }

    bool addBreak(PropertyName *maybeLabel) {
        if (maybeLabel)
            return addBreakOrContinue(maybeLabel, &labeledBreaks_);
        return addBreakOrContinue(breakableStack_.back(), &unlabeledBreaks_);
    }

    bool addContinue(PropertyName *maybeLabel) {
        if (maybeLabel)
            return addBreakOrContinue(maybeLabel, &labeledContinues_);
        return addBreakOrContinue(loopStack_.back(), &unlabeledContinues_);
    }

    bool startSwitch(ParseNode *pn, MDefinition *expr, int32_t low, int32_t high,
                     MBasicBlock **switchBlock)
    {
        if (!breakableStack_.append(pn))
            return false;
        if (inDeadCode()) {
            *switchBlock = nullptr;
            return true;
        }
        curBlock_->end(MTableSwitch::New(alloc(), expr, low, high));
        *switchBlock = curBlock_;
        curBlock_ = nullptr;
        return true;
    }

    bool startSwitchCase(MBasicBlock *switchBlock, MBasicBlock **next, ParseNode *pn)
    {
        if (!switchBlock) {
            *next = nullptr;
            return true;
        }
        if (!newBlock(switchBlock, next, pn))
            return false;
        if (curBlock_) {
            curBlock_->end(MGoto::New(alloc(), *next));
            if (!(*next)->addPredecessor(alloc(), curBlock_))
                return false;
        }
        curBlock_ = *next;
        return true;
    }

    bool startSwitchDefault(MBasicBlock *switchBlock, BlockVector *cases, MBasicBlock **defaultBlock, ParseNode *pn)
    {
        if (!startSwitchCase(switchBlock, defaultBlock, pn))
            return false;
        if (!*defaultBlock)
            return true;
        mirGraph().moveBlockToEnd(*defaultBlock);
        return true;
    }

    bool joinSwitch(MBasicBlock *switchBlock, const BlockVector &cases, MBasicBlock *defaultBlock)
    {
        ParseNode *pn = breakableStack_.popCopy();
        if (!switchBlock)
            return true;
        MTableSwitch *mir = switchBlock->lastIns()->toTableSwitch();
        size_t defaultIndex = mir->addDefault(defaultBlock);
        for (unsigned i = 0; i < cases.length(); i++) {
            if (!cases[i])
                mir->addCase(defaultIndex);
            else
                mir->addCase(mir->addSuccessor(cases[i]));
        }
        if (curBlock_) {
            MBasicBlock *next;
            if (!newBlock(curBlock_, &next, pn))
                return false;
            curBlock_->end(MGoto::New(alloc(), next));
            curBlock_ = next;
        }
        return bindUnlabeledBreaks(pn);
    }

    /*************************************************************************/

    MIRGenerator *extractMIR()
    {
        JS_ASSERT(mirGen_ != nullptr);
        MIRGenerator *mirGen = mirGen_;
        mirGen_ = nullptr;
        return mirGen;
    }

    /*************************************************************************/
  private:
    void noteBasicBlockPosition(MBasicBlock *blk, ParseNode *pn)
    {
#if defined(JS_ION_PERF)
        if (pn) {
            unsigned line = 0U, column = 0U;
            m().parser().tokenStream.srcCoords.lineNumAndColumnIndex(pn->pn_pos.begin, &line, &column);
            blk->setLineno(line);
            blk->setColumnIndex(column);
        }
#endif
    }

    bool newBlockWithDepth(MBasicBlock *pred, unsigned loopDepth, MBasicBlock **block, ParseNode *pn)
    {
        *block = MBasicBlock::NewAsmJS(mirGraph(), info(), pred, MBasicBlock::NORMAL);
        if (!*block)
            return false;
        noteBasicBlockPosition(*block, pn);
        mirGraph().addBlock(*block);
        (*block)->setLoopDepth(loopDepth);
        return true;
    }

    bool newBlock(MBasicBlock *pred, MBasicBlock **block, ParseNode *pn)
    {
        return newBlockWithDepth(pred, loopStack_.length(), block, pn);
    }

    bool bindBreaksOrContinues(BlockVector *preds, bool *createdJoinBlock, ParseNode *pn)
    {
        for (unsigned i = 0; i < preds->length(); i++) {
            MBasicBlock *pred = (*preds)[i];
            if (*createdJoinBlock) {
                pred->end(MGoto::New(alloc(), curBlock_));
                if (!curBlock_->addPredecessor(alloc(), pred))
                    return false;
            } else {
                MBasicBlock *next;
                if (!newBlock(pred, &next, pn))
                    return false;
                pred->end(MGoto::New(alloc(), next));
                if (curBlock_) {
                    curBlock_->end(MGoto::New(alloc(), next));
                    if (!next->addPredecessor(alloc(), curBlock_))
                        return false;
                }
                curBlock_ = next;
                *createdJoinBlock = true;
            }
            JS_ASSERT(curBlock_->begin() == curBlock_->end());
            if (!mirGen_->ensureBallast())
                return false;
        }
        preds->clear();
        return true;
    }

    bool bindLabeledBreaksOrContinues(const LabelVector *maybeLabels, LabeledBlockMap *map,
                                      bool *createdJoinBlock, ParseNode *pn)
    {
        if (!maybeLabels)
            return true;
        const LabelVector &labels = *maybeLabels;
        for (unsigned i = 0; i < labels.length(); i++) {
            if (LabeledBlockMap::Ptr p = map->lookup(labels[i])) {
                if (!bindBreaksOrContinues(&p->value(), createdJoinBlock, pn))
                    return false;
                map->remove(p);
            }
            if (!mirGen_->ensureBallast())
                return false;
        }
        return true;
    }

    template <class Key, class Map>
    bool addBreakOrContinue(Key key, Map *map)
    {
        if (inDeadCode())
            return true;
        typename Map::AddPtr p = map->lookupForAdd(key);
        if (!p) {
            BlockVector empty(m().cx());
            if (!map->add(p, key, Move(empty)))
                return false;
        }
        if (!p->value().append(curBlock_))
            return false;
        curBlock_ = nullptr;
        return true;
    }

    bool bindUnlabeledBreaks(ParseNode *pn)
    {
        bool createdJoinBlock = false;
        if (UnlabeledBlockMap::Ptr p = unlabeledBreaks_.lookup(pn)) {
            if (!bindBreaksOrContinues(&p->value(), &createdJoinBlock, pn))
                return false;
            unlabeledBreaks_.remove(p);
        }
        return true;
    }
};

} /* anonymous namespace */

/*****************************************************************************/
// asm.js type-checking and code-generation algorithm

static bool
CheckIdentifier(ModuleCompiler &m, ParseNode *usepn, PropertyName *name)
{
    if (name == m.cx()->names().arguments || name == m.cx()->names().eval)
        return m.failName(usepn, "'%s' is not an allowed identifier", name);
    return true;
}

static bool
CheckModuleLevelName(ModuleCompiler &m, ParseNode *usepn, PropertyName *name)
{
    if (!CheckIdentifier(m, usepn, name))
        return false;

    if (name == m.moduleFunctionName() ||
        name == m.module().globalArgumentName() ||
        name == m.module().importArgumentName() ||
        name == m.module().bufferArgumentName() ||
        m.lookupGlobal(name))
    {
        return m.failName(usepn, "duplicate name '%s' not allowed", name);
    }

    return true;
}

static bool
CheckFunctionHead(ModuleCompiler &m, ParseNode *fn)
{
    JSFunction *fun = FunctionObject(fn);
    if (fun->hasRest())
        return m.fail(fn, "rest args not allowed");
    if (fun->isExprClosure())
        return m.fail(fn, "expression closures not allowed");
    if (fn->pn_funbox->hasDestructuringArgs)
        return m.fail(fn, "destructuring args not allowed");
    return true;
}

static bool
CheckArgument(ModuleCompiler &m, ParseNode *arg, PropertyName **name)
{
    if (!IsDefinition(arg))
        return m.fail(arg, "duplicate argument name not allowed");

    if (arg->pn_dflags & PND_DEFAULT)
        return m.fail(arg, "default arguments not allowed");

    if (!CheckIdentifier(m, arg, arg->name()))
        return false;

    *name = arg->name();
    return true;
}

static bool
CheckModuleArgument(ModuleCompiler &m, ParseNode *arg, PropertyName **name)
{
    if (!CheckArgument(m, arg, name))
        return false;

    if (!CheckModuleLevelName(m, arg, *name))
        return false;

    return true;
}

static bool
CheckModuleArguments(ModuleCompiler &m, ParseNode *fn)
{
    unsigned numFormals;
    ParseNode *arg1 = FunctionArgsList(fn, &numFormals);
    ParseNode *arg2 = arg1 ? NextNode(arg1) : nullptr;
    ParseNode *arg3 = arg2 ? NextNode(arg2) : nullptr;

    if (numFormals > 3)
        return m.fail(fn, "asm.js modules takes at most 3 argument");

    PropertyName *arg1Name = nullptr;
    if (numFormals >= 1 && !CheckModuleArgument(m, arg1, &arg1Name))
        return false;
    m.initGlobalArgumentName(arg1Name);

    PropertyName *arg2Name = nullptr;
    if (numFormals >= 2 && !CheckModuleArgument(m, arg2, &arg2Name))
        return false;
    m.initImportArgumentName(arg2Name);

    PropertyName *arg3Name = nullptr;
    if (numFormals >= 3 && !CheckModuleArgument(m, arg3, &arg3Name))
        return false;
    m.initBufferArgumentName(arg3Name);

    return true;
}

static bool
CheckPrecedingStatements(ModuleCompiler &m, ParseNode *stmtList)
{
    JS_ASSERT(stmtList->isKind(PNK_STATEMENTLIST));

    if (ListLength(stmtList) != 0)
        return m.fail(ListHead(stmtList), "invalid asm.js statement");

    return true;
}

static bool
CheckGlobalVariableInitConstant(ModuleCompiler &m, PropertyName *varName, ParseNode *initNode,
                                bool isConst)
{
    NumLit literal = ExtractNumericLiteral(m, initNode);
    if (!literal.hasType())
        return m.fail(initNode, "global initializer is out of representable integer range");

    return m.addGlobalVarInit(varName, literal.varType(), literal.value(), isConst);
}

static bool
CheckTypeAnnotation(ModuleCompiler &m, ParseNode *coercionNode, AsmJSCoercion *coercion,
                    ParseNode **coercedExpr = nullptr)
{
    switch (coercionNode->getKind()) {
      case PNK_BITOR: {
        ParseNode *rhs = BinaryRight(coercionNode);
        uint32_t i;
        if (!IsLiteralInt(m, rhs, &i) || i != 0)
            return m.fail(rhs, "must use |0 for argument/return coercion");
        *coercion = AsmJS_ToInt32;
        if (coercedExpr)
            *coercedExpr = BinaryLeft(coercionNode);
        return true;
      }
      case PNK_POS: {
        *coercion = AsmJS_ToNumber;
        if (coercedExpr)
            *coercedExpr = UnaryKid(coercionNode);
        return true;
      }
      case PNK_CALL: {
        *coercion = AsmJS_FRound;
        if (!IsFloatCoercion(m, coercionNode, coercedExpr))
            return m.fail(coercionNode, "call must be to fround coercion");
        return true;
      }
      default:;
    }

    return m.fail(coercionNode, "must be of the form +x, fround(x) or x|0");
}

static bool
CheckGlobalVariableImportExpr(ModuleCompiler &m, PropertyName *varName, AsmJSCoercion coercion,
                              ParseNode *coercedExpr, bool isConst)
{
    if (!coercedExpr->isKind(PNK_DOT))
        return m.failName(coercedExpr, "invalid import expression for global '%s'", varName);

    ParseNode *base = DotBase(coercedExpr);
    PropertyName *field = DotMember(coercedExpr);

    PropertyName *importName = m.module().importArgumentName();
    if (!importName)
        return m.fail(coercedExpr, "cannot import without an asm.js foreign parameter");
    if (!IsUseOfName(base, importName))
        return m.failName(coercedExpr, "base of import expression must be '%s'", importName);

    return m.addGlobalVarImport(varName, field, coercion, isConst);
}

static bool
CheckGlobalVariableInitImport(ModuleCompiler &m, PropertyName *varName, ParseNode *initNode,
                              bool isConst)
{
    AsmJSCoercion coercion;
    ParseNode *coercedExpr;
    if (!CheckTypeAnnotation(m, initNode, &coercion, &coercedExpr))
        return false;
    return CheckGlobalVariableImportExpr(m, varName, coercion, coercedExpr, isConst);
}

static bool
CheckNewArrayView(ModuleCompiler &m, PropertyName *varName, ParseNode *newExpr)
{
    ParseNode *ctorExpr = ListHead(newExpr);
    if (!ctorExpr->isKind(PNK_DOT))
        return m.fail(ctorExpr, "only valid 'new' import is 'new global.*Array(buf)'");

    ParseNode *base = DotBase(ctorExpr);
    PropertyName *field = DotMember(ctorExpr);

    PropertyName *globalName = m.module().globalArgumentName();
    if (!globalName)
        return m.fail(base, "cannot create array view without an asm.js global parameter");
    if (!IsUseOfName(base, globalName))
        return m.failName(base, "expecting '%s.*Array", globalName);

    ParseNode *bufArg = NextNode(ctorExpr);
    if (!bufArg || NextNode(bufArg) != nullptr)
        return m.fail(ctorExpr, "array view constructor takes exactly one argument");

    PropertyName *bufferName = m.module().bufferArgumentName();
    if (!bufferName)
        return m.fail(bufArg, "cannot create array view without an asm.js heap parameter");
    if (!IsUseOfName(bufArg, bufferName))
        return m.failName(bufArg, "argument to array view constructor must be '%s'", bufferName);

    JSAtomState &names = m.cx()->names();
    ArrayBufferView::ViewType type;
    if (field == names.Int8Array)
        type = ArrayBufferView::TYPE_INT8;
    else if (field == names.Uint8Array)
        type = ArrayBufferView::TYPE_UINT8;
    else if (field == names.Int16Array)
        type = ArrayBufferView::TYPE_INT16;
    else if (field == names.Uint16Array)
        type = ArrayBufferView::TYPE_UINT16;
    else if (field == names.Int32Array)
        type = ArrayBufferView::TYPE_INT32;
    else if (field == names.Uint32Array)
        type = ArrayBufferView::TYPE_UINT32;
    else if (field == names.Float32Array)
        type = ArrayBufferView::TYPE_FLOAT32;
    else if (field == names.Float64Array)
        type = ArrayBufferView::TYPE_FLOAT64;
    else
        return m.fail(ctorExpr, "could not match typed array name");

    return m.addArrayView(varName, type, field);
}

static bool
CheckGlobalDotImport(ModuleCompiler &m, PropertyName *varName, ParseNode *initNode)
{
    ParseNode *base = DotBase(initNode);
    PropertyName *field = DotMember(initNode);

    if (base->isKind(PNK_DOT)) {
        ParseNode *global = DotBase(base);
        PropertyName *math = DotMember(base);
        if (!IsUseOfName(global, m.module().globalArgumentName()) || math != m.cx()->names().Math)
            return m.fail(base, "expecting global.Math");

        ModuleCompiler::MathBuiltin mathBuiltin;
        if (!m.lookupStandardLibraryMathName(field, &mathBuiltin))
            return m.failName(initNode, "'%s' is not a standard Math builtin", field);

        switch (mathBuiltin.kind) {
          case ModuleCompiler::MathBuiltin::Function:
            return m.addMathBuiltinFunction(varName, mathBuiltin.u.func, field);
          case ModuleCompiler::MathBuiltin::Constant:
            return m.addMathBuiltinConstant(varName, mathBuiltin.u.cst, field);
          default:
            break;
        }
        MOZ_ASSUME_UNREACHABLE("unexpected or uninitialized math builtin type");
    }

    if (IsUseOfName(base, m.module().globalArgumentName())) {
        if (field == m.cx()->names().NaN)
            return m.addGlobalConstant(varName, GenericNaN(), field);
        if (field == m.cx()->names().Infinity)
            return m.addGlobalConstant(varName, PositiveInfinity<double>(), field);
        return m.failName(initNode, "'%s' is not a standard global constant", field);
    }

    if (IsUseOfName(base, m.module().importArgumentName()))
        return m.addFFI(varName, field);

    return m.fail(initNode, "expecting c.y where c is either the global or foreign parameter");
}

static bool
CheckModuleGlobal(ModuleCompiler &m, ParseNode *var, bool isConst)
{
    if (!IsDefinition(var))
        return m.fail(var, "import variable names must be unique");

    if (!CheckModuleLevelName(m, var, var->name()))
        return false;

    ParseNode *initNode = MaybeDefinitionInitializer(var);
    if (!initNode)
        return m.fail(var, "module import needs initializer");

    if (IsNumericLiteral(m, initNode))
        return CheckGlobalVariableInitConstant(m, var->name(), initNode, isConst);

    if (initNode->isKind(PNK_BITOR) || initNode->isKind(PNK_POS) || initNode->isKind(PNK_CALL))
        return CheckGlobalVariableInitImport(m, var->name(), initNode, isConst);

    if (initNode->isKind(PNK_NEW))
        return CheckNewArrayView(m, var->name(), initNode);

    if (initNode->isKind(PNK_DOT))
        return CheckGlobalDotImport(m, var->name(), initNode);

    return m.fail(initNode, "unsupported import expression");
}

static bool
CheckModuleGlobals(ModuleCompiler &m)
{
    while (true) {
        ParseNode *varStmt;
        if (!ParseVarOrConstStatement(m.parser(), &varStmt))
            return false;
        if (!varStmt)
            break;
        for (ParseNode *var = VarListHead(varStmt); var; var = NextNode(var)) {
            if (!CheckModuleGlobal(m, var, varStmt->isKind(PNK_CONST)))
                return false;
        }
    }

    return true;
}

static bool
ArgFail(FunctionCompiler &f, PropertyName *argName, ParseNode *stmt)
{
    return f.failName(stmt, "expecting argument type declaration for '%s' of the "
                      "form 'arg = arg|0' or 'arg = +arg' or 'arg = fround(arg)'", argName);
}

static bool
CheckArgumentType(FunctionCompiler &f, ParseNode *stmt, PropertyName *name, VarType *type)
{
    if (!stmt || !IsExpressionStatement(stmt))
        return ArgFail(f, name, stmt ? stmt : f.fn());

    ParseNode *initNode = ExpressionStatementExpr(stmt);
    if (!initNode || !initNode->isKind(PNK_ASSIGN))
        return ArgFail(f, name, stmt);

    ParseNode *argNode = BinaryLeft(initNode);
    ParseNode *coercionNode = BinaryRight(initNode);

    if (!IsUseOfName(argNode, name))
        return ArgFail(f, name, stmt);

    ParseNode *coercedExpr;
    AsmJSCoercion coercion;
    if (!CheckTypeAnnotation(f.m(), coercionNode, &coercion, &coercedExpr))
        return false;

    if (!IsUseOfName(coercedExpr, name))
        return ArgFail(f, name, stmt);

    *type = VarType(coercion);
    return true;
}

static bool
CheckArguments(FunctionCompiler &f, ParseNode **stmtIter, VarTypeVector *argTypes)
{
    ParseNode *stmt = *stmtIter;

    unsigned numFormals;
    ParseNode *argpn = FunctionArgsList(f.fn(), &numFormals);

    for (unsigned i = 0; i < numFormals; i++, argpn = NextNode(argpn), stmt = NextNode(stmt)) {
        PropertyName *name;
        if (!CheckArgument(f.m(), argpn, &name))
            return false;

        VarType type;
        if (!CheckArgumentType(f, stmt, name, &type))
            return false;

        if (!argTypes->append(type))
            return false;

        if (!f.addFormal(argpn, name, type))
            return false;
    }

    *stmtIter = stmt;
    return true;
}

static bool
CheckFinalReturn(FunctionCompiler &f, ParseNode *stmt, RetType *retType)
{
    if (stmt && stmt->isKind(PNK_RETURN)) {
        if (ParseNode *coercionNode = UnaryKid(stmt)) {
            if (IsNumericLiteral(f.m(), coercionNode)) {
                switch (ExtractNumericLiteral(f.m(), coercionNode).which()) {
                  case NumLit::BigUnsigned:
                  case NumLit::OutOfRangeInt:
                    return f.fail(coercionNode, "returned literal is out of integer range");
                  case NumLit::Fixnum:
                  case NumLit::NegativeInt:
                    *retType = RetType::Signed;
                    break;
                  case NumLit::Double:
                    *retType = RetType::Double;
                    break;
                  case NumLit::Float:
                    *retType = RetType::Float;
                    break;
                }
                return true;
            }

            AsmJSCoercion coercion;
            if (!CheckTypeAnnotation(f.m(), coercionNode, &coercion))
                return false;

            *retType = RetType(coercion);
            return true;
        }

        *retType = RetType::Void;
        return true;
    }

    *retType = RetType::Void;
    f.returnVoid();
    return true;
}

static bool
CheckVariable(FunctionCompiler &f, ParseNode *var)
{
    if (!IsDefinition(var))
        return f.fail(var, "local variable names must not restate argument names");

    PropertyName *name = var->name();

    if (!CheckIdentifier(f.m(), var, name))
        return false;

    ParseNode *initNode = MaybeDefinitionInitializer(var);
    if (!initNode)
        return f.failName(var, "var '%s' needs explicit type declaration via an initial value", name);

    if (initNode->isKind(PNK_NAME)) {
        PropertyName *initName = initNode->name();
        if (const ModuleCompiler::Global *global = f.lookupGlobal(initName)) {
            if (global->which() != ModuleCompiler::Global::ConstantLiteral)
                return f.failName(initNode, "'%s' isn't a possible global variable initializer, "
                                            "needs to be a const numeric literal", initName);
            return f.addVariable(var, name, global->varOrConstType(), global->constLiteralValue());
        }
        return f.failName(initNode, "'%s' needs to be a global name", initName);
    }

    if (!IsNumericLiteral(f.m(), initNode))
        return f.failName(initNode, "initializer for '%s' needs to be a numeric literal or a global const literal", name);

    NumLit literal = ExtractNumericLiteral(f.m(), initNode);
    if (!literal.hasType())
        return f.failName(initNode, "initializer for '%s' is out of range", name);

    return f.addVariable(var, name, literal.varType(), literal.value());
}

static bool
CheckVariables(FunctionCompiler &f, ParseNode **stmtIter)
{
    ParseNode *stmt = *stmtIter;

    for (; stmt && stmt->isKind(PNK_VAR); stmt = NextNonEmptyStatement(stmt)) {
        for (ParseNode *var = VarListHead(stmt); var; var = NextNode(var)) {
            if (!CheckVariable(f, var))
                return false;
        }
    }

    *stmtIter = stmt;
    return true;
}

static bool
CheckExpr(FunctionCompiler &f, ParseNode *expr, MDefinition **def, Type *type);

static bool
CheckNumericLiteral(FunctionCompiler &f, ParseNode *num, MDefinition **def, Type *type)
{
    NumLit literal = ExtractNumericLiteral(f.m(), num);
    if (!literal.hasType())
        return f.fail(num, "numeric literal out of representable integer range");

    *type = literal.type();
    *def = f.constant(literal.value(), literal.type());
    return true;
}

static bool
CheckVarRef(FunctionCompiler &f, ParseNode *varRef, MDefinition **def, Type *type)
{
    PropertyName *name = varRef->name();

    if (const FunctionCompiler::Local *local = f.lookupLocal(name)) {
        *def = f.getLocalDef(*local);
        *type = local->type.toType();
        return true;
    }

    if (const ModuleCompiler::Global *global = f.lookupGlobal(name)) {
        switch (global->which()) {
          case ModuleCompiler::Global::ConstantLiteral:
            *def = f.constant(global->constLiteralValue(), global->varOrConstType().toType());
            *type = global->varOrConstType().toType();
            break;
          case ModuleCompiler::Global::ConstantImport:
          case ModuleCompiler::Global::Variable:
            *def = f.loadGlobalVar(*global);
            *type = global->varOrConstType().toType();
            break;
          case ModuleCompiler::Global::Function:
          case ModuleCompiler::Global::FFI:
          case ModuleCompiler::Global::MathBuiltinFunction:
          case ModuleCompiler::Global::FuncPtrTable:
          case ModuleCompiler::Global::ArrayView:
            return f.failName(varRef, "'%s' may not be accessed by ordinary expressions", name);
        }
        return true;
    }

    return f.failName(varRef, "'%s' not found in local or asm.js module scope", name);
}

static inline bool
IsLiteralOrConstInt(FunctionCompiler &f, ParseNode *pn, uint32_t *u32)
{
    if (IsLiteralInt(f.m(), pn, u32))
        return true;

    if (pn->getKind() != PNK_NAME)
        return false;

    PropertyName *name = pn->name();
    const ModuleCompiler::Global *global = f.lookupGlobal(name);
    if (!global || global->which() != ModuleCompiler::Global::ConstantLiteral)
        return false;

    const Value &v = global->constLiteralValue();
    if (!v.isInt32())
        return false;

    *u32 = (uint32_t) v.toInt32();
    return true;
}

static bool
FoldMaskedArrayIndex(FunctionCompiler &f, ParseNode **indexExpr, int32_t *mask,
                     NeedsBoundsCheck *needsBoundsCheck)
{
    ParseNode *indexNode = BinaryLeft(*indexExpr);
    ParseNode *maskNode = BinaryRight(*indexExpr);

    uint32_t mask2;
    if (IsLiteralOrConstInt(f, maskNode, &mask2)) {
        // Flag the access to skip the bounds check if the mask ensures that an 'out of
        // bounds' access can not occur based on the current heap length constraint.
        if (mask2 == 0 ||
            CountLeadingZeroes32(f.m().minHeapLength() - 1) <= CountLeadingZeroes32(mask2)) {
            *needsBoundsCheck = NO_BOUNDS_CHECK;
        }
        *mask &= mask2;
        *indexExpr = indexNode;
        return true;
    }

    return false;
}

static bool
CheckArrayAccess(FunctionCompiler &f, ParseNode *elem, ArrayBufferView::ViewType *viewType,
                 MDefinition **def, NeedsBoundsCheck *needsBoundsCheck)
{
    ParseNode *viewName = ElemBase(elem);
    ParseNode *indexExpr = ElemIndex(elem);
    *needsBoundsCheck = NEEDS_BOUNDS_CHECK;

    if (!viewName->isKind(PNK_NAME))
        return f.fail(viewName, "base of array access must be a typed array view name");

    const ModuleCompiler::Global *global = f.lookupGlobal(viewName->name());
    if (!global || global->which() != ModuleCompiler::Global::ArrayView)
        return f.fail(viewName, "base of array access must be a typed array view name");

    *viewType = global->viewType();

    uint32_t pointer;
    if (IsLiteralOrConstInt(f, indexExpr, &pointer)) {
        if (pointer > (uint32_t(INT32_MAX) >> TypedArrayShift(*viewType)))
            return f.fail(indexExpr, "constant index out of range");
        pointer <<= TypedArrayShift(*viewType);
        // It is adequate to note pointer+1 rather than rounding up to the next
        // access-size boundary because access is always aligned and the constraint
        // will be rounded up to a larger alignment later.
        f.m().requireHeapLengthToBeAtLeast(uint32_t(pointer) + 1);
        *needsBoundsCheck = NO_BOUNDS_CHECK;
        *def = f.constant(Int32Value(pointer), Type::Int);
        return true;
    }

    // Mask off the low bits to account for the clearing effect of a right shift
    // followed by the left shift implicit in the array access. E.g., H32[i>>2]
    // loses the low two bits.
    int32_t mask = ~((uint32_t(1) << TypedArrayShift(*viewType)) - 1);

    MDefinition *pointerDef;
    if (indexExpr->isKind(PNK_RSH)) {
        ParseNode *shiftNode = BinaryRight(indexExpr);
        ParseNode *pointerNode = BinaryLeft(indexExpr);

        uint32_t shift;
        if (!IsLiteralInt(f.m(), shiftNode, &shift))
            return f.failf(shiftNode, "shift amount must be constant");

        unsigned requiredShift = TypedArrayShift(*viewType);
        if (shift != requiredShift)
            return f.failf(shiftNode, "shift amount must be %u", requiredShift);

        if (pointerNode->isKind(PNK_BITAND))
            FoldMaskedArrayIndex(f, &pointerNode, &mask, needsBoundsCheck);

        // Fold a 'literal constant right shifted' now, and skip the bounds check if
        // currently possible. This handles the optimization of many of these uses without
        // the need for range analysis, and saves the generation of a MBitAnd op.
        if (IsLiteralOrConstInt(f, pointerNode, &pointer) && pointer <= uint32_t(INT32_MAX)) {
            // Cases: b[c>>n], and b[(c&m)>>n]
            pointer &= mask;
            if (pointer < f.m().minHeapLength())
                *needsBoundsCheck = NO_BOUNDS_CHECK;
            *def = f.constant(Int32Value(pointer), Type::Int);
            return true;
        }

        Type pointerType;
        if (!CheckExpr(f, pointerNode, &pointerDef, &pointerType))
            return false;

        if (!pointerType.isIntish())
            return f.failf(indexExpr, "%s is not a subtype of int", pointerType.toChars());
    } else {
        if (TypedArrayShift(*viewType) != 0)
            return f.fail(indexExpr, "index expression isn't shifted; must be an Int8/Uint8 access");

        JS_ASSERT(mask == -1);
        bool folded = false;

        if (indexExpr->isKind(PNK_BITAND))
            folded = FoldMaskedArrayIndex(f, &indexExpr, &mask, needsBoundsCheck);

        Type pointerType;
        if (!CheckExpr(f, indexExpr, &pointerDef, &pointerType))
            return false;

        if (folded) {
            if (!pointerType.isIntish())
                return f.failf(indexExpr, "%s is not a subtype of intish", pointerType.toChars());
        } else {
            if (!pointerType.isInt())
                return f.failf(indexExpr, "%s is not a subtype of int", pointerType.toChars());
        }
    }

    // Don't generate the mask op if there is no need for it which could happen for
    // a shift of zero.
    if (mask == -1)
        *def = pointerDef;
    else
        *def = f.bitwise<MBitAnd>(pointerDef, f.constant(Int32Value(mask), Type::Int));

    return true;
}

static bool
CheckLoadArray(FunctionCompiler &f, ParseNode *elem, MDefinition **def, Type *type)
{
    ArrayBufferView::ViewType viewType;
    MDefinition *pointerDef;
    NeedsBoundsCheck needsBoundsCheck;
    if (!CheckArrayAccess(f, elem, &viewType, &pointerDef, &needsBoundsCheck))
        return false;

    *def = f.loadHeap(viewType, pointerDef, needsBoundsCheck);
    *type = TypedArrayLoadType(viewType);
    return true;
}

static bool
CheckStoreArray(FunctionCompiler &f, ParseNode *lhs, ParseNode *rhs, MDefinition **def, Type *type)
{
    ArrayBufferView::ViewType viewType;
    MDefinition *pointerDef;
    NeedsBoundsCheck needsBoundsCheck;
    if (!CheckArrayAccess(f, lhs, &viewType, &pointerDef, &needsBoundsCheck))
        return false;

    MDefinition *rhsDef;
    Type rhsType;
    if (!CheckExpr(f, rhs, &rhsDef, &rhsType))
        return false;

    switch (viewType) {
      case ArrayBufferView::TYPE_INT8:
      case ArrayBufferView::TYPE_INT16:
      case ArrayBufferView::TYPE_INT32:
      case ArrayBufferView::TYPE_UINT8:
      case ArrayBufferView::TYPE_UINT16:
      case ArrayBufferView::TYPE_UINT32:
        if (!rhsType.isIntish())
            return f.failf(lhs, "%s is not a subtype of intish", rhsType.toChars());
        break;
      case ArrayBufferView::TYPE_FLOAT32:
        if (rhsType.isMaybeDouble())
            rhsDef = f.unary<MToFloat32>(rhsDef);
        else if (!rhsType.isFloatish())
            return f.failf(lhs, "%s is not a subtype of double? or floatish", rhsType.toChars());
        break;
      case ArrayBufferView::TYPE_FLOAT64:
        if (rhsType.isFloat())
            rhsDef = f.unary<MToDouble>(rhsDef);
        else if (!rhsType.isMaybeDouble())
            return f.failf(lhs, "%s is not a subtype of float or double?", rhsType.toChars());
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("Unexpected view type");
    }

    f.storeHeap(viewType, pointerDef, rhsDef, needsBoundsCheck);

    *def = rhsDef;
    *type = rhsType;
    return true;
}

static bool
CheckAssignName(FunctionCompiler &f, ParseNode *lhs, ParseNode *rhs, MDefinition **def, Type *type)
{
    Rooted<PropertyName *> name(f.cx(), lhs->name());

    MDefinition *rhsDef;
    Type rhsType;
    if (!CheckExpr(f, rhs, &rhsDef, &rhsType))
        return false;

    if (const FunctionCompiler::Local *lhsVar = f.lookupLocal(name)) {
        if (!(rhsType <= lhsVar->type)) {
            return f.failf(lhs, "%s is not a subtype of %s",
                           rhsType.toChars(), lhsVar->type.toType().toChars());
        }
        f.assign(*lhsVar, rhsDef);
    } else if (const ModuleCompiler::Global *global = f.lookupGlobal(name)) {
        if (global->which() != ModuleCompiler::Global::Variable)
            return f.failName(lhs, "'%s' is not a mutable variable", name);
        if (!(rhsType <= global->varOrConstType())) {
            return f.failf(lhs, "%s is not a subtype of %s",
                           rhsType.toChars(), global->varOrConstType().toType().toChars());
        }
        f.storeGlobalVar(*global, rhsDef);
    } else {
        return f.failName(lhs, "'%s' not found in local or asm.js module scope", name);
    }

    *def = rhsDef;
    *type = rhsType;
    return true;
}

static bool
CheckAssign(FunctionCompiler &f, ParseNode *assign, MDefinition **def, Type *type)
{
    JS_ASSERT(assign->isKind(PNK_ASSIGN));
    ParseNode *lhs = BinaryLeft(assign);
    ParseNode *rhs = BinaryRight(assign);

    if (lhs->getKind() == PNK_ELEM)
        return CheckStoreArray(f, lhs, rhs, def, type);

    if (lhs->getKind() == PNK_NAME)
        return CheckAssignName(f, lhs, rhs, def, type);

    return f.fail(assign, "left-hand side of assignment must be a variable or array access");
}

static bool
CheckMathIMul(FunctionCompiler &f, ParseNode *call, RetType retType, MDefinition **def, Type *type)
{
    if (CallArgListLength(call) != 2)
        return f.fail(call, "Math.imul must be passed 2 arguments");

    ParseNode *lhs = CallArgList(call);
    ParseNode *rhs = NextNode(lhs);

    MDefinition *lhsDef;
    Type lhsType;
    if (!CheckExpr(f, lhs, &lhsDef, &lhsType))
        return false;

    MDefinition *rhsDef;
    Type rhsType;
    if (!CheckExpr(f, rhs, &rhsDef, &rhsType))
        return false;

    if (!lhsType.isIntish())
        return f.failf(lhs, "%s is not a subtype of intish", lhsType.toChars());
    if (!rhsType.isIntish())
        return f.failf(rhs, "%s is not a subtype of intish", rhsType.toChars());
    if (retType != RetType::Signed)
        return f.failf(call, "return type is signed, used as %s", retType.toType().toChars());

    *def = f.mul(lhsDef, rhsDef, MIRType_Int32, MMul::Integer);
    *type = Type::Signed;
    return true;
}

static bool
CheckMathAbs(FunctionCompiler &f, ParseNode *call, RetType retType, MDefinition **def, Type *type)
{
    if (CallArgListLength(call) != 1)
        return f.fail(call, "Math.abs must be passed 1 argument");

    ParseNode *arg = CallArgList(call);

    MDefinition *argDef;
    Type argType;
    if (!CheckExpr(f, arg, &argDef, &argType))
        return false;

    if (argType.isSigned()) {
        if (retType != RetType::Signed)
            return f.failf(call, "return type is signed, used as %s", retType.toType().toChars());
        *def = f.unary<MAbs>(argDef, MIRType_Int32);
        *type = Type::Signed;
        return true;
    }

    if (argType.isMaybeDouble()) {
        if (retType != RetType::Double)
            return f.failf(call, "return type is double, used as %s", retType.toType().toChars());
        *def = f.unary<MAbs>(argDef, MIRType_Double);
        *type = Type::Double;
        return true;
    }

    if (argType.isMaybeFloat()) {
        if (retType != RetType::Float)
            return f.failf(call, "return type is float, used as %s", retType.toType().toChars());
        *def = f.unary<MAbs>(argDef, MIRType_Float32);
        *type = Type::Float;
        return true;
    }

    return f.failf(call, "%s is not a subtype of signed, float? or double?", argType.toChars());
}

static bool
CheckMathSqrt(FunctionCompiler &f, ParseNode *call, RetType retType, MDefinition **def, Type *type)
{
    if (CallArgListLength(call) != 1)
        return f.fail(call, "Math.sqrt must be passed 1 argument");

    ParseNode *arg = CallArgList(call);

    MDefinition *argDef;
    Type argType;
    if (!CheckExpr(f, arg, &argDef, &argType))
        return false;

    if (argType.isMaybeDouble()) {
        if (retType != RetType::Double)
            return f.failf(call, "return type is double, used as %s", retType.toType().toChars());
        *def = f.unary<MSqrt>(argDef, MIRType_Double);
        *type = Type::Double;
        return true;
    }

    if (argType.isMaybeFloat()) {
        if (retType != RetType::Float)
            return f.failf(call, "return type is float, used as %s", retType.toType().toChars());
        *def = f.unary<MSqrt>(argDef, MIRType_Float32);
        *type = Type::Float;
        return true;
    }

    return f.failf(call, "%s is neither a subtype of double? nor float?", argType.toChars());
}

static bool
CheckMathMinMax(FunctionCompiler &f, ParseNode *callNode, RetType retType, MDefinition **def, Type *type, bool isMax)
{
    if (CallArgListLength(callNode) < 2)
        return f.fail(callNode, "Math.min/max must be passed at least 2 arguments");

    ParseNode *firstArg = CallArgList(callNode);
    MDefinition *firstDef;
    Type firstType;
    if (!CheckExpr(f, firstArg, &firstDef, &firstType))
        return false;

    bool opIsDouble = firstType.isMaybeDouble();
    bool opIsInteger = firstType.isInt();
    MIRType opType = firstType.toMIRType();

    if (!opIsDouble && !opIsInteger)
        return f.failf(firstArg, "%s is not a subtype of double? or int", firstType.toChars());

    MDefinition *lastDef = firstDef;
    ParseNode *nextArg = NextNode(firstArg);
    for (unsigned i = 1; i < CallArgListLength(callNode); i++, nextArg = NextNode(nextArg)) {
        MDefinition *nextDef;
        Type nextType;
        if (!CheckExpr(f, nextArg, &nextDef, &nextType))
            return false;

        if (opIsDouble && !nextType.isMaybeDouble())
            return f.failf(nextArg, "%s is not a subtype of double?", nextType.toChars());
        if (opIsInteger && !nextType.isInt())
            return f.failf(nextArg, "%s is not a subtype of int", nextType.toChars());

        lastDef = f.minMax(lastDef, nextDef, opType, isMax);
    }

    if (opIsDouble && retType != RetType::Double)
        return f.failf(callNode, "return type is double, used as %s", retType.toType().toChars());
    if (opIsInteger && retType != RetType::Signed)
        return f.failf(callNode, "return type is int, used as %s", retType.toType().toChars());

    *type = opIsDouble ? Type::Double : Type::Signed;
    *def = lastDef;
    return true;
}

typedef bool (*CheckArgType)(FunctionCompiler &f, ParseNode *argNode, Type type);

static bool
CheckCallArgs(FunctionCompiler &f, ParseNode *callNode, CheckArgType checkArg,
              FunctionCompiler::Call *call)
{
    f.startCallArgs(call);

    ParseNode *argNode = CallArgList(callNode);
    for (unsigned i = 0; i < CallArgListLength(callNode); i++, argNode = NextNode(argNode)) {
        MDefinition *def;
        Type type;
        if (!CheckExpr(f, argNode, &def, &type))
            return false;

        if (!checkArg(f, argNode, type))
            return false;

        if (!f.passArg(def, VarType::FromCheckedType(type), call))
            return false;
    }

    f.finishCallArgs(call);
    return true;
}

static bool
CheckSignatureAgainstExisting(ModuleCompiler &m, ParseNode *usepn, const Signature &sig,
                              const Signature &existing)
{
    if (sig.args().length() != existing.args().length()) {
        return m.failf(usepn, "incompatible number of arguments (%u here vs. %u before)",
                       sig.args().length(), existing.args().length());
    }

    for (unsigned i = 0; i < sig.args().length(); i++) {
        if (sig.arg(i) != existing.arg(i)) {
            return m.failf(usepn, "incompatible type for argument %u: (%s here vs. %s before)",
                           i, sig.arg(i).toType().toChars(), existing.arg(i).toType().toChars());
        }
    }

    if (sig.retType() != existing.retType()) {
        return m.failf(usepn, "%s incompatible with previous return of type %s",
                       sig.retType().toType().toChars(), existing.retType().toType().toChars());
    }

    JS_ASSERT(sig == existing);
    return true;
}

static bool
CheckFunctionSignature(ModuleCompiler &m, ParseNode *usepn, Signature &&sig, PropertyName *name,
                       ModuleCompiler::Func **func)
{
    ModuleCompiler::Func *existing = m.lookupFunction(name);
    if (!existing) {
        if (!CheckModuleLevelName(m, usepn, name))
            return false;
        return m.addFunction(name, Move(sig), func);
    }

    if (!CheckSignatureAgainstExisting(m, usepn, sig, existing->sig()))
        return false;

    *func = existing;
    return true;
}

static bool
CheckIsVarType(FunctionCompiler &f, ParseNode *argNode, Type type)
{
    if (!type.isVarType())
        return f.failf(argNode, "%s is not a subtype of int, float or double", type.toChars());
    return true;
}

static bool
CheckInternalCall(FunctionCompiler &f, ParseNode *callNode, PropertyName *calleeName,
                  RetType retType, MDefinition **def, Type *type)
{
    FunctionCompiler::Call call(f, retType);

    if (!CheckCallArgs(f, callNode, CheckIsVarType, &call))
        return false;

    ModuleCompiler::Func *callee;
    if (!CheckFunctionSignature(f.m(), callNode, Move(call.sig()), calleeName, &callee))
        return false;

    if (!f.internalCall(*callee, call, def))
        return false;

    *type = retType.toType();
    return true;
}

static bool
CheckFuncPtrTableAgainstExisting(ModuleCompiler &m, ParseNode *usepn,
                                 PropertyName *name, Signature &&sig, unsigned mask,
                                 ModuleCompiler::FuncPtrTable **tableOut)
{
    if (const ModuleCompiler::Global *existing = m.lookupGlobal(name)) {
        if (existing->which() != ModuleCompiler::Global::FuncPtrTable)
            return m.failName(usepn, "'%s' is not a function-pointer table", name);

        ModuleCompiler::FuncPtrTable &table = m.funcPtrTable(existing->funcPtrTableIndex());
        if (mask != table.mask())
            return m.failf(usepn, "mask does not match previous value (%u)", table.mask());

        if (!CheckSignatureAgainstExisting(m, usepn, sig, table.sig()))
            return false;

        *tableOut = &table;
        return true;
    }

    if (!CheckModuleLevelName(m, usepn, name))
        return false;

    return m.addFuncPtrTable(name, Move(sig), mask, tableOut);
}

static bool
CheckFuncPtrCall(FunctionCompiler &f, ParseNode *callNode, RetType retType, MDefinition **def, Type *type)
{
    ParseNode *callee = CallCallee(callNode);
    ParseNode *tableNode = ElemBase(callee);
    ParseNode *indexExpr = ElemIndex(callee);

    if (!tableNode->isKind(PNK_NAME))
        return f.fail(tableNode, "expecting name of function-pointer array");

    PropertyName *name = tableNode->name();
    if (const ModuleCompiler::Global *existing = f.lookupGlobal(name)) {
        if (existing->which() != ModuleCompiler::Global::FuncPtrTable)
            return f.failName(tableNode, "'%s' is not the name of a function-pointer array", name);
    }

    if (!indexExpr->isKind(PNK_BITAND))
        return f.fail(indexExpr, "function-pointer table index expression needs & mask");

    ParseNode *indexNode = BinaryLeft(indexExpr);
    ParseNode *maskNode = BinaryRight(indexExpr);

    uint32_t mask;
    if (!IsLiteralInt(f.m(), maskNode, &mask) || mask == UINT32_MAX || !IsPowerOfTwo(mask + 1))
        return f.fail(maskNode, "function-pointer table index mask value must be a power of two");

    MDefinition *indexDef;
    Type indexType;
    if (!CheckExpr(f, indexNode, &indexDef, &indexType))
        return false;

    if (!indexType.isIntish())
        return f.failf(indexNode, "%s is not a subtype of intish", indexType.toChars());

    FunctionCompiler::Call call(f, retType);

    if (!CheckCallArgs(f, callNode, CheckIsVarType, &call))
        return false;

    ModuleCompiler::FuncPtrTable *table;
    if (!CheckFuncPtrTableAgainstExisting(f.m(), tableNode, name, Move(call.sig()), mask, &table))
        return false;

    if (!f.funcPtrCall(*table, indexDef, call, def))
        return false;

    *type = retType.toType();
    return true;
}

static bool
CheckIsExternType(FunctionCompiler &f, ParseNode *argNode, Type type)
{
    if (!type.isExtern())
        return f.failf(argNode, "%s is not a subtype of extern", type.toChars());
    return true;
}

static bool
CheckFFICall(FunctionCompiler &f, ParseNode *callNode, unsigned ffiIndex, RetType retType,
             MDefinition **def, Type *type)
{
    PropertyName *calleeName = CallCallee(callNode)->name();

    if (retType == RetType::Float)
        return f.fail(callNode, "FFI calls can't return float");

    FunctionCompiler::Call call(f, retType);
    if (!CheckCallArgs(f, callNode, CheckIsExternType, &call))
        return false;

    unsigned exitIndex;
    if (!f.m().addExit(ffiIndex, calleeName, Move(call.sig()), &exitIndex))
        return false;

    if (!f.ffiCall(exitIndex, call, retType.toMIRType(), def))
        return false;

    *type = retType.toType();
    return true;
}

static bool CheckCall(FunctionCompiler &f, ParseNode *call, RetType retType, MDefinition **def, Type *type);

static bool
CheckFRoundArg(FunctionCompiler &f, ParseNode *arg, MDefinition **def, Type *type)
{
    if (arg->isKind(PNK_CALL))
        return CheckCall(f, arg, RetType::Float, def, type);

    MDefinition *inputDef;
    Type inputType;
    if (!CheckExpr(f, arg, &inputDef, &inputType))
        return false;

    if (inputType.isMaybeDouble() || inputType.isSigned())
        *def = f.unary<MToFloat32>(inputDef);
    else if (inputType.isUnsigned())
        *def = f.unary<MAsmJSUnsignedToFloat32>(inputDef);
    else if (inputType.isFloatish())
        *def = inputDef;
    else
        return f.failf(arg, "%s is not a subtype of signed, unsigned, double? or floatish", inputType.toChars());

    *type = Type::Float;
    return true;
}

static bool
CheckMathFRound(FunctionCompiler &f, ParseNode *callNode, RetType retType, MDefinition **def, Type *type)
{
    ParseNode *argNode = nullptr;
    if (!IsFloatCoercion(f.m(), callNode, &argNode))
        return f.fail(callNode, "invalid call to fround");

    MDefinition *operand;
    Type operandType;
    if (!CheckFRoundArg(f, argNode, &operand, &operandType))
        return false;

    JS_ASSERT(operandType == Type::Float);

    switch (retType.which()) {
      case RetType::Double:
        *def = f.unary<MToDouble>(operand);
        *type = Type::Double;
        return true;
      case RetType::Signed:
        *def = f.unary<MTruncateToInt32>(operand);
        *type = Type::Signed;
        return true;
      case RetType::Float:
        *def = operand;
        *type = Type::Float;
        return true;
      case RetType::Void:
        // definition and return types should be ignored by the caller
        return true;
    }

    MOZ_ASSUME_UNREACHABLE("return value of fround is ignored");
}

static bool
CheckIsMaybeDouble(FunctionCompiler &f, ParseNode *argNode, Type type)
{
    if (!type.isMaybeDouble())
        return f.failf(argNode, "%s is not a subtype of double?", type.toChars());
    return true;
}

static bool
CheckIsMaybeFloat(FunctionCompiler &f, ParseNode *argNode, Type type)
{
    if (!type.isMaybeFloat())
        return f.failf(argNode, "%s is not a subtype of float?", type.toChars());
    return true;
}

static bool
CheckMathBuiltinCall(FunctionCompiler &f, ParseNode *callNode, AsmJSMathBuiltinFunction func,
                     RetType retType, MDefinition **def, Type *type)
{
    unsigned arity = 0;
    AsmJSImmKind doubleCallee, floatCallee;
    switch (func) {
      case AsmJSMathBuiltin_imul:   return CheckMathIMul(f, callNode, retType, def, type);
      case AsmJSMathBuiltin_abs:    return CheckMathAbs(f, callNode, retType, def, type);
      case AsmJSMathBuiltin_sqrt:   return CheckMathSqrt(f, callNode, retType, def, type);
      case AsmJSMathBuiltin_fround: return CheckMathFRound(f, callNode, retType, def, type);
      case AsmJSMathBuiltin_min:    return CheckMathMinMax(f, callNode, retType, def, type, /* isMax = */ false);
      case AsmJSMathBuiltin_max:    return CheckMathMinMax(f, callNode, retType, def, type, /* isMax = */ true);
      case AsmJSMathBuiltin_ceil:   arity = 1; doubleCallee = AsmJSImm_CeilD;  floatCallee = AsmJSImm_CeilF;   break;
      case AsmJSMathBuiltin_floor:  arity = 1; doubleCallee = AsmJSImm_FloorD; floatCallee = AsmJSImm_FloorF;  break;
      case AsmJSMathBuiltin_sin:    arity = 1; doubleCallee = AsmJSImm_SinD;   floatCallee = AsmJSImm_Invalid; break;
      case AsmJSMathBuiltin_cos:    arity = 1; doubleCallee = AsmJSImm_CosD;   floatCallee = AsmJSImm_Invalid; break;
      case AsmJSMathBuiltin_tan:    arity = 1; doubleCallee = AsmJSImm_TanD;   floatCallee = AsmJSImm_Invalid; break;
      case AsmJSMathBuiltin_asin:   arity = 1; doubleCallee = AsmJSImm_ASinD;  floatCallee = AsmJSImm_Invalid; break;
      case AsmJSMathBuiltin_acos:   arity = 1; doubleCallee = AsmJSImm_ACosD;  floatCallee = AsmJSImm_Invalid; break;
      case AsmJSMathBuiltin_atan:   arity = 1; doubleCallee = AsmJSImm_ATanD;  floatCallee = AsmJSImm_Invalid; break;
      case AsmJSMathBuiltin_exp:    arity = 1; doubleCallee = AsmJSImm_ExpD;   floatCallee = AsmJSImm_Invalid; break;
      case AsmJSMathBuiltin_log:    arity = 1; doubleCallee = AsmJSImm_LogD;   floatCallee = AsmJSImm_Invalid; break;
      case AsmJSMathBuiltin_pow:    arity = 2; doubleCallee = AsmJSImm_PowD;   floatCallee = AsmJSImm_Invalid; break;
      case AsmJSMathBuiltin_atan2:  arity = 2; doubleCallee = AsmJSImm_ATan2D; floatCallee = AsmJSImm_Invalid; break;
      default: MOZ_ASSUME_UNREACHABLE("unexpected mathBuiltin function");
    }

    if (retType == RetType::Float && floatCallee == AsmJSImm_Invalid)
        return f.fail(callNode, "math builtin cannot be used as float");
    if (retType != RetType::Double && retType != RetType::Float)
        return f.failf(callNode, "return type of math function is double or float, used as %s", retType.toType().toChars());

    FunctionCompiler::Call call(f, retType);
    if (retType == RetType::Float && !CheckCallArgs(f, callNode, CheckIsMaybeFloat, &call))
        return false;
    if (retType == RetType::Double && !CheckCallArgs(f, callNode, CheckIsMaybeDouble, &call))
        return false;

    if (call.sig().args().length() != arity)
        return f.failf(callNode, "call passed %u arguments, expected %u", call.sig().args().length(), arity);

    if (retType == RetType::Float && !f.builtinCall(floatCallee, call, retType.toMIRType(), def))
        return false;
    if (retType == RetType::Double && !f.builtinCall(doubleCallee, call, retType.toMIRType(), def))
        return false;

    *type = retType.toType();
    return true;
}

static bool
CheckCall(FunctionCompiler &f, ParseNode *call, RetType retType, MDefinition **def, Type *type)
{
    JS_CHECK_RECURSION_DONT_REPORT(f.cx(), return f.m().failOverRecursed());

    ParseNode *callee = CallCallee(call);

    if (callee->isKind(PNK_ELEM))
        return CheckFuncPtrCall(f, call, retType, def, type);

    if (!callee->isKind(PNK_NAME))
        return f.fail(callee, "unexpected callee expression type");

    PropertyName *calleeName = callee->name();

    if (const ModuleCompiler::Global *global = f.lookupGlobal(calleeName)) {
        switch (global->which()) {
          case ModuleCompiler::Global::FFI:
            return CheckFFICall(f, call, global->ffiIndex(), retType, def, type);
          case ModuleCompiler::Global::MathBuiltinFunction:
            return CheckMathBuiltinCall(f, call, global->mathBuiltinFunction(), retType, def, type);
          case ModuleCompiler::Global::ConstantLiteral:
          case ModuleCompiler::Global::ConstantImport:
          case ModuleCompiler::Global::Variable:
          case ModuleCompiler::Global::FuncPtrTable:
          case ModuleCompiler::Global::ArrayView:
            return f.failName(callee, "'%s' is not callable function", callee->name());
          case ModuleCompiler::Global::Function:
            break;
        }
    }

    return CheckInternalCall(f, call, calleeName, retType, def, type);
}

static bool
CheckPos(FunctionCompiler &f, ParseNode *pos, MDefinition **def, Type *type)
{
    JS_ASSERT(pos->isKind(PNK_POS));
    ParseNode *operand = UnaryKid(pos);

    if (operand->isKind(PNK_CALL))
        return CheckCall(f, operand, RetType::Double, def, type);

    MDefinition *operandDef;
    Type operandType;
    if (!CheckExpr(f, operand, &operandDef, &operandType))
        return false;

    if (operandType.isMaybeFloat() || operandType.isSigned())
        *def = f.unary<MToDouble>(operandDef);
    else if (operandType.isUnsigned())
        *def = f.unary<MAsmJSUnsignedToDouble>(operandDef);
    else if (operandType.isMaybeDouble())
        *def = operandDef;
    else
        return f.failf(operand, "%s is not a subtype of signed, unsigned, float or double?", operandType.toChars());

    *type = Type::Double;
    return true;
}

static bool
CheckNot(FunctionCompiler &f, ParseNode *expr, MDefinition **def, Type *type)
{
    JS_ASSERT(expr->isKind(PNK_NOT));
    ParseNode *operand = UnaryKid(expr);

    MDefinition *operandDef;
    Type operandType;
    if (!CheckExpr(f, operand, &operandDef, &operandType))
        return false;

    if (!operandType.isInt())
        return f.failf(operand, "%s is not a subtype of int", operandType.toChars());

    *def = f.unary<MNot>(operandDef);
    *type = Type::Int;
    return true;
}

static bool
CheckNeg(FunctionCompiler &f, ParseNode *expr, MDefinition **def, Type *type)
{
    JS_ASSERT(expr->isKind(PNK_NEG));
    ParseNode *operand = UnaryKid(expr);

    MDefinition *operandDef;
    Type operandType;
    if (!CheckExpr(f, operand, &operandDef, &operandType))
        return false;

    if (operandType.isInt()) {
        *def = f.unary<MAsmJSNeg>(operandDef, MIRType_Int32);
        *type = Type::Intish;
        return true;
    }

    if (operandType.isMaybeDouble()) {
        *def = f.unary<MAsmJSNeg>(operandDef, MIRType_Double);
        *type = Type::Double;
        return true;
    }

    if (operandType.isMaybeFloat()) {
        *def = f.unary<MAsmJSNeg>(operandDef, MIRType_Float32);
        *type = Type::Floatish;
        return true;
    }

    return f.failf(operand, "%s is not a subtype of int, float? or double?", operandType.toChars());
}

static bool
CheckCoerceToInt(FunctionCompiler &f, ParseNode *expr, MDefinition **def, Type *type)
{
    JS_ASSERT(expr->isKind(PNK_BITNOT));
    ParseNode *operand = UnaryKid(expr);

    MDefinition *operandDef;
    Type operandType;
    if (!CheckExpr(f, operand, &operandDef, &operandType))
        return false;

    if (operandType.isMaybeDouble() || operandType.isMaybeFloat()) {
        *def = f.unary<MTruncateToInt32>(operandDef);
        *type = Type::Signed;
        return true;
    }

    if (!operandType.isIntish())
        return f.failf(operand, "%s is not a subtype of double?, float? or intish", operandType.toChars());

    *def = operandDef;
    *type = Type::Signed;
    return true;
}

static bool
CheckBitNot(FunctionCompiler &f, ParseNode *neg, MDefinition **def, Type *type)
{
    JS_ASSERT(neg->isKind(PNK_BITNOT));
    ParseNode *operand = UnaryKid(neg);

    if (operand->isKind(PNK_BITNOT))
        return CheckCoerceToInt(f, operand, def, type);

    MDefinition *operandDef;
    Type operandType;
    if (!CheckExpr(f, operand, &operandDef, &operandType))
        return false;

    if (!operandType.isIntish())
        return f.failf(operand, "%s is not a subtype of intish", operandType.toChars());

    *def = f.bitwise<MBitNot>(operandDef);
    *type = Type::Signed;
    return true;
}

static bool
CheckComma(FunctionCompiler &f, ParseNode *comma, MDefinition **def, Type *type)
{
    JS_ASSERT(comma->isKind(PNK_COMMA));
    ParseNode *operands = ListHead(comma);

    ParseNode *pn = operands;
    for (; NextNode(pn); pn = NextNode(pn)) {
        MDefinition *_1;
        Type _2;
        if (pn->isKind(PNK_CALL)) {
            if (!CheckCall(f, pn, RetType::Void, &_1, &_2))
                return false;
        } else {
            if (!CheckExpr(f, pn, &_1, &_2))
                return false;
        }
    }

    if (!CheckExpr(f, pn, def, type))
        return false;

    return true;
}

static bool
CheckConditional(FunctionCompiler &f, ParseNode *ternary, MDefinition **def, Type *type)
{
    JS_ASSERT(ternary->isKind(PNK_CONDITIONAL));
    ParseNode *cond = TernaryKid1(ternary);
    ParseNode *thenExpr = TernaryKid2(ternary);
    ParseNode *elseExpr = TernaryKid3(ternary);

    MDefinition *condDef;
    Type condType;
    if (!CheckExpr(f, cond, &condDef, &condType))
        return false;

    if (!condType.isInt())
        return f.failf(cond, "%s is not a subtype of int", condType.toChars());

    MBasicBlock *thenBlock = nullptr, *elseBlock = nullptr;
    if (!f.branchAndStartThen(condDef, &thenBlock, &elseBlock, thenExpr, elseExpr))
        return false;

    MDefinition *thenDef;
    Type thenType;
    if (!CheckExpr(f, thenExpr, &thenDef, &thenType))
        return false;

    BlockVector thenBlocks(f.cx());
    if (!f.appendThenBlock(&thenBlocks))
        return false;

    f.pushPhiInput(thenDef);
    f.switchToElse(elseBlock);

    MDefinition *elseDef;
    Type elseType;
    if (!CheckExpr(f, elseExpr, &elseDef, &elseType))
        return false;

    f.pushPhiInput(elseDef);

    if (thenType.isInt() && elseType.isInt()) {
        *type = Type::Int;
    } else if (thenType.isDouble() && elseType.isDouble()) {
        *type = Type::Double;
    } else if (thenType.isFloat() && elseType.isFloat()) {
        *type = Type::Float;
    } else {
        return f.failf(ternary, "then/else branches of conditional must both produce int or double, "
                       "current types are %s and %s", thenType.toChars(), elseType.toChars());
    }

    if (!f.joinIfElse(thenBlocks, elseExpr))
        return false;

    *def = f.popPhiOutput();
    return true;
}

static bool
IsValidIntMultiplyConstant(ModuleCompiler &m, ParseNode *expr)
{
    if (!IsNumericLiteral(m, expr))
        return false;

    NumLit literal = ExtractNumericLiteral(m, expr);
    switch (literal.which()) {
      case NumLit::Fixnum:
      case NumLit::NegativeInt:
        if (abs(literal.toInt32()) < (1<<20))
            return true;
        return false;
      case NumLit::BigUnsigned:
      case NumLit::Double:
      case NumLit::Float:
      case NumLit::OutOfRangeInt:
        return false;
    }

    MOZ_ASSUME_UNREACHABLE("Bad literal");
}

static bool
CheckMultiply(FunctionCompiler &f, ParseNode *star, MDefinition **def, Type *type)
{
    JS_ASSERT(star->isKind(PNK_STAR));
    ParseNode *lhs = BinaryLeft(star);
    ParseNode *rhs = BinaryRight(star);

    MDefinition *lhsDef;
    Type lhsType;
    if (!CheckExpr(f, lhs, &lhsDef, &lhsType))
        return false;

    MDefinition *rhsDef;
    Type rhsType;
    if (!CheckExpr(f, rhs, &rhsDef, &rhsType))
        return false;

    if (lhsType.isInt() && rhsType.isInt()) {
        if (!IsValidIntMultiplyConstant(f.m(), lhs) && !IsValidIntMultiplyConstant(f.m(), rhs))
            return f.fail(star, "one arg to int multiply must be a small (-2^20, 2^20) int literal");
        *def = f.mul(lhsDef, rhsDef, MIRType_Int32, MMul::Integer);
        *type = Type::Intish;
        return true;
    }

    if (lhsType.isMaybeDouble() && rhsType.isMaybeDouble()) {
        *def = f.mul(lhsDef, rhsDef, MIRType_Double, MMul::Normal);
        *type = Type::Double;
        return true;
    }

    if (lhsType.isMaybeFloat() && rhsType.isMaybeFloat()) {
        *def = f.mul(lhsDef, rhsDef, MIRType_Float32, MMul::Normal);
        *type = Type::Floatish;
        return true;
    }

    return f.fail(star, "multiply operands must be both int, both double? or both float?");
}

static bool
CheckAddOrSub(FunctionCompiler &f, ParseNode *expr, MDefinition **def, Type *type,
              unsigned *numAddOrSubOut = nullptr)
{
    JS_CHECK_RECURSION_DONT_REPORT(f.cx(), return f.m().failOverRecursed());

    JS_ASSERT(expr->isKind(PNK_ADD) || expr->isKind(PNK_SUB));
    ParseNode *lhs = BinaryLeft(expr);
    ParseNode *rhs = BinaryRight(expr);

    MDefinition *lhsDef, *rhsDef;
    Type lhsType, rhsType;
    unsigned lhsNumAddOrSub, rhsNumAddOrSub;

    if (lhs->isKind(PNK_ADD) || lhs->isKind(PNK_SUB)) {
        if (!CheckAddOrSub(f, lhs, &lhsDef, &lhsType, &lhsNumAddOrSub))
            return false;
        if (lhsType == Type::Intish)
            lhsType = Type::Int;
    } else {
        if (!CheckExpr(f, lhs, &lhsDef, &lhsType))
            return false;
        lhsNumAddOrSub = 0;
    }

    if (rhs->isKind(PNK_ADD) || rhs->isKind(PNK_SUB)) {
        if (!CheckAddOrSub(f, rhs, &rhsDef, &rhsType, &rhsNumAddOrSub))
            return false;
        if (rhsType == Type::Intish)
            rhsType = Type::Int;
    } else {
        if (!CheckExpr(f, rhs, &rhsDef, &rhsType))
            return false;
        rhsNumAddOrSub = 0;
    }

    unsigned numAddOrSub = lhsNumAddOrSub + rhsNumAddOrSub + 1;
    if (numAddOrSub > (1<<20))
        return f.fail(expr, "too many + or - without intervening coercion");

    if (lhsType.isInt() && rhsType.isInt()) {
        *def = expr->isKind(PNK_ADD)
               ? f.binary<MAdd>(lhsDef, rhsDef, MIRType_Int32)
               : f.binary<MSub>(lhsDef, rhsDef, MIRType_Int32);
        *type = Type::Intish;
    } else if (lhsType.isMaybeDouble() && rhsType.isMaybeDouble()) {
        *def = expr->isKind(PNK_ADD)
               ? f.binary<MAdd>(lhsDef, rhsDef, MIRType_Double)
               : f.binary<MSub>(lhsDef, rhsDef, MIRType_Double);
        *type = Type::Double;
    } else if (lhsType.isMaybeFloat() && rhsType.isMaybeFloat()) {
        *def = expr->isKind(PNK_ADD)
               ? f.binary<MAdd>(lhsDef, rhsDef, MIRType_Float32)
               : f.binary<MSub>(lhsDef, rhsDef, MIRType_Float32);
        *type = Type::Floatish;
    } else {
        return f.failf(expr, "operands to + or - must both be int, float? or double?, got %s and %s",
                       lhsType.toChars(), rhsType.toChars());
    }

    if (numAddOrSubOut)
        *numAddOrSubOut = numAddOrSub;
    return true;
}

static bool
CheckDivOrMod(FunctionCompiler &f, ParseNode *expr, MDefinition **def, Type *type)
{
    JS_ASSERT(expr->isKind(PNK_DIV) || expr->isKind(PNK_MOD));
    ParseNode *lhs = BinaryLeft(expr);
    ParseNode *rhs = BinaryRight(expr);

    MDefinition *lhsDef, *rhsDef;
    Type lhsType, rhsType;
    if (!CheckExpr(f, lhs, &lhsDef, &lhsType))
        return false;
    if (!CheckExpr(f, rhs, &rhsDef, &rhsType))
        return false;

    if (lhsType.isMaybeDouble() && rhsType.isMaybeDouble()) {
        *def = expr->isKind(PNK_DIV)
               ? f.div(lhsDef, rhsDef, MIRType_Double, /* unsignd = */ false)
               : f.mod(lhsDef, rhsDef, MIRType_Double, /* unsignd = */ false);
        *type = Type::Double;
        return true;
    }

    if (lhsType.isMaybeFloat() && rhsType.isMaybeFloat()) {
        if (expr->isKind(PNK_DIV))
            *def = f.div(lhsDef, rhsDef, MIRType_Float32, /* unsignd = */ false);
        else
            return f.fail(expr, "modulo cannot receive float arguments");
        *type = Type::Floatish;
        return true;
    }

    if (lhsType.isSigned() && rhsType.isSigned()) {
        if (expr->isKind(PNK_DIV))
            *def = f.div(lhsDef, rhsDef, MIRType_Int32, /* unsignd = */ false);
        else
            *def = f.mod(lhsDef, rhsDef, MIRType_Int32, /* unsignd = */ false);
        *type = Type::Intish;
        return true;
    }

    if (lhsType.isUnsigned() && rhsType.isUnsigned()) {
        if (expr->isKind(PNK_DIV))
            *def = f.div(lhsDef, rhsDef, MIRType_Int32, /* unsignd = */ true);
        else
            *def = f.mod(lhsDef, rhsDef, MIRType_Int32, /* unsignd = */ true);
        *type = Type::Intish;
        return true;
    }

    return f.failf(expr, "arguments to / or %% must both be double?, float?, signed, or unsigned; "
                   "%s and %s are given", lhsType.toChars(), rhsType.toChars());
}

static bool
CheckComparison(FunctionCompiler &f, ParseNode *comp, MDefinition **def, Type *type)
{
    JS_ASSERT(comp->isKind(PNK_LT) || comp->isKind(PNK_LE) || comp->isKind(PNK_GT) ||
              comp->isKind(PNK_GE) || comp->isKind(PNK_EQ) || comp->isKind(PNK_NE));
    ParseNode *lhs = BinaryLeft(comp);
    ParseNode *rhs = BinaryRight(comp);

    MDefinition *lhsDef, *rhsDef;
    Type lhsType, rhsType;
    if (!CheckExpr(f, lhs, &lhsDef, &lhsType))
        return false;
    if (!CheckExpr(f, rhs, &rhsDef, &rhsType))
        return false;

    if ((lhsType.isSigned() && rhsType.isSigned()) || (lhsType.isUnsigned() && rhsType.isUnsigned())) {
        MCompare::CompareType compareType = (lhsType.isUnsigned() && rhsType.isUnsigned())
                                            ? MCompare::Compare_UInt32
                                            : MCompare::Compare_Int32;
        *def = f.compare(lhsDef, rhsDef, comp->getOp(), compareType);
        *type = Type::Int;
        return true;
    }

    if (lhsType.isDouble() && rhsType.isDouble()) {
        *def = f.compare(lhsDef, rhsDef, comp->getOp(), MCompare::Compare_Double);
        *type = Type::Int;
        return true;
    }

    if (lhsType.isFloat() && rhsType.isFloat()) {
        *def = f.compare(lhsDef, rhsDef, comp->getOp(), MCompare::Compare_Float32);
        *type = Type::Int;
        return true;
    }

    return f.failf(comp, "arguments to a comparison must both be signed, unsigned, floats or doubles; "
                   "%s and %s are given", lhsType.toChars(), rhsType.toChars());
}

static bool
CheckBitwise(FunctionCompiler &f, ParseNode *bitwise, MDefinition **def, Type *type)
{
    ParseNode *lhs = BinaryLeft(bitwise);
    ParseNode *rhs = BinaryRight(bitwise);

    int32_t identityElement;
    bool onlyOnRight;
    switch (bitwise->getKind()) {
      case PNK_BITOR:  identityElement = 0;  onlyOnRight = false; *type = Type::Signed;   break;
      case PNK_BITAND: identityElement = -1; onlyOnRight = false; *type = Type::Signed;   break;
      case PNK_BITXOR: identityElement = 0;  onlyOnRight = false; *type = Type::Signed;   break;
      case PNK_LSH:    identityElement = 0;  onlyOnRight = true;  *type = Type::Signed;   break;
      case PNK_RSH:    identityElement = 0;  onlyOnRight = true;  *type = Type::Signed;   break;
      case PNK_URSH:   identityElement = 0;  onlyOnRight = true;  *type = Type::Unsigned; break;
      default: MOZ_ASSUME_UNREACHABLE("not a bitwise op");
    }

    uint32_t i;
    if (!onlyOnRight && IsLiteralInt(f.m(), lhs, &i) && i == uint32_t(identityElement)) {
        Type rhsType;
        if (!CheckExpr(f, rhs, def, &rhsType))
            return false;
        if (!rhsType.isIntish())
            return f.failf(bitwise, "%s is not a subtype of intish", rhsType.toChars());
        return true;
    }

    if (IsLiteralInt(f.m(), rhs, &i) && i == uint32_t(identityElement)) {
        if (bitwise->isKind(PNK_BITOR) && lhs->isKind(PNK_CALL))
            return CheckCall(f, lhs, RetType::Signed, def, type);

        Type lhsType;
        if (!CheckExpr(f, lhs, def, &lhsType))
            return false;
        if (!lhsType.isIntish())
            return f.failf(bitwise, "%s is not a subtype of intish", lhsType.toChars());
        return true;
    }

    MDefinition *lhsDef;
    Type lhsType;
    if (!CheckExpr(f, lhs, &lhsDef, &lhsType))
        return false;

    MDefinition *rhsDef;
    Type rhsType;
    if (!CheckExpr(f, rhs, &rhsDef, &rhsType))
        return false;

    if (!lhsType.isIntish())
        return f.failf(lhs, "%s is not a subtype of intish", lhsType.toChars());
    if (!rhsType.isIntish())
        return f.failf(rhs, "%s is not a subtype of intish", rhsType.toChars());

    switch (bitwise->getKind()) {
      case PNK_BITOR:  *def = f.bitwise<MBitOr>(lhsDef, rhsDef); break;
      case PNK_BITAND: *def = f.bitwise<MBitAnd>(lhsDef, rhsDef); break;
      case PNK_BITXOR: *def = f.bitwise<MBitXor>(lhsDef, rhsDef); break;
      case PNK_LSH:    *def = f.bitwise<MLsh>(lhsDef, rhsDef); break;
      case PNK_RSH:    *def = f.bitwise<MRsh>(lhsDef, rhsDef); break;
      case PNK_URSH:   *def = f.bitwise<MUrsh>(lhsDef, rhsDef); break;
      default: MOZ_ASSUME_UNREACHABLE("not a bitwise op");
    }

    return true;
}

static bool
CheckUncoercedCall(FunctionCompiler &f, ParseNode *expr, MDefinition **def, Type *type)
{
    JS_ASSERT(expr->isKind(PNK_CALL));

    ParseNode *arg;
    if (!IsFloatCoercion(f.m(), expr, &arg)) {
        return f.fail(expr, "all function calls must either be ignored (via f(); or "
                            "comma-expression), coerced to signed (via f()|0), coerced to float "
                            "(via fround(f())) or coerced to double (via +f())");
    }

    return CheckFRoundArg(f, arg, def, type);
}

static bool
CheckExpr(FunctionCompiler &f, ParseNode *expr, MDefinition **def, Type *type)
{
    JS_CHECK_RECURSION_DONT_REPORT(f.cx(), return f.m().failOverRecursed());

    if (!f.mirGen().ensureBallast())
        return false;

    if (IsNumericLiteral(f.m(), expr))
        return CheckNumericLiteral(f, expr, def, type);

    switch (expr->getKind()) {
      case PNK_NAME:        return CheckVarRef(f, expr, def, type);
      case PNK_ELEM:        return CheckLoadArray(f, expr, def, type);
      case PNK_ASSIGN:      return CheckAssign(f, expr, def, type);
      case PNK_POS:         return CheckPos(f, expr, def, type);
      case PNK_NOT:         return CheckNot(f, expr, def, type);
      case PNK_NEG:         return CheckNeg(f, expr, def, type);
      case PNK_BITNOT:      return CheckBitNot(f, expr, def, type);
      case PNK_COMMA:       return CheckComma(f, expr, def, type);
      case PNK_CONDITIONAL: return CheckConditional(f, expr, def, type);
      case PNK_STAR:        return CheckMultiply(f, expr, def, type);
      case PNK_CALL:        return CheckUncoercedCall(f, expr, def, type);

      case PNK_ADD:
      case PNK_SUB:         return CheckAddOrSub(f, expr, def, type);

      case PNK_DIV:
      case PNK_MOD:         return CheckDivOrMod(f, expr, def, type);

      case PNK_LT:
      case PNK_LE:
      case PNK_GT:
      case PNK_GE:
      case PNK_EQ:
      case PNK_NE:          return CheckComparison(f, expr, def, type);

      case PNK_BITOR:
      case PNK_BITAND:
      case PNK_BITXOR:
      case PNK_LSH:
      case PNK_RSH:
      case PNK_URSH:        return CheckBitwise(f, expr, def, type);

      default:;
    }

    return f.fail(expr, "unsupported expression");
}

static bool
CheckStatement(FunctionCompiler &f, ParseNode *stmt, LabelVector *maybeLabels = nullptr);

static bool
CheckExprStatement(FunctionCompiler &f, ParseNode *exprStmt)
{
    JS_ASSERT(exprStmt->isKind(PNK_SEMI));
    ParseNode *expr = UnaryKid(exprStmt);

    if (!expr)
        return true;

    MDefinition *_1;
    Type _2;

    if (expr->isKind(PNK_CALL))
        return CheckCall(f, expr, RetType::Void, &_1, &_2);

    return CheckExpr(f, UnaryKid(exprStmt), &_1, &_2);
}

static bool
CheckWhile(FunctionCompiler &f, ParseNode *whileStmt, const LabelVector *maybeLabels)
{
    JS_ASSERT(whileStmt->isKind(PNK_WHILE));
    ParseNode *cond = BinaryLeft(whileStmt);
    ParseNode *body = BinaryRight(whileStmt);

    MBasicBlock *loopEntry;
    if (!f.startPendingLoop(whileStmt, &loopEntry, body))
        return false;

    MDefinition *condDef;
    Type condType;
    if (!CheckExpr(f, cond, &condDef, &condType))
        return false;

    if (!condType.isInt())
        return f.failf(cond, "%s is not a subtype of int", condType.toChars());

    MBasicBlock *afterLoop;
    if (!f.branchAndStartLoopBody(condDef, &afterLoop, body, NextNode(whileStmt)))
        return false;

    if (!CheckStatement(f, body))
        return false;

    if (!f.bindContinues(whileStmt, maybeLabels))
        return false;

    return f.closeLoop(loopEntry, afterLoop);
}

static bool
CheckFor(FunctionCompiler &f, ParseNode *forStmt, const LabelVector *maybeLabels)
{
    JS_ASSERT(forStmt->isKind(PNK_FOR));
    ParseNode *forHead = BinaryLeft(forStmt);
    ParseNode *body = BinaryRight(forStmt);

    if (!forHead->isKind(PNK_FORHEAD))
        return f.fail(forHead, "unsupported for-loop statement");

    ParseNode *maybeInit = TernaryKid1(forHead);
    ParseNode *maybeCond = TernaryKid2(forHead);
    ParseNode *maybeInc = TernaryKid3(forHead);

    if (maybeInit) {
        MDefinition *_1;
        Type _2;
        if (!CheckExpr(f, maybeInit, &_1, &_2))
            return false;
    }

    MBasicBlock *loopEntry;
    if (!f.startPendingLoop(forStmt, &loopEntry, body))
        return false;

    MDefinition *condDef;
    if (maybeCond) {
        Type condType;
        if (!CheckExpr(f, maybeCond, &condDef, &condType))
            return false;

        if (!condType.isInt())
            return f.failf(maybeCond, "%s is not a subtype of int", condType.toChars());
    } else {
        condDef = f.constant(Int32Value(1), Type::Int);
    }

    MBasicBlock *afterLoop;
    if (!f.branchAndStartLoopBody(condDef, &afterLoop, body, NextNode(forStmt)))
        return false;

    if (!CheckStatement(f, body))
        return false;

    if (!f.bindContinues(forStmt, maybeLabels))
        return false;

    if (maybeInc) {
        MDefinition *_1;
        Type _2;
        if (!CheckExpr(f, maybeInc, &_1, &_2))
            return false;
    }

    return f.closeLoop(loopEntry, afterLoop);
}

static bool
CheckDoWhile(FunctionCompiler &f, ParseNode *whileStmt, const LabelVector *maybeLabels)
{
    JS_ASSERT(whileStmt->isKind(PNK_DOWHILE));
    ParseNode *body = BinaryLeft(whileStmt);
    ParseNode *cond = BinaryRight(whileStmt);

    MBasicBlock *loopEntry;
    if (!f.startPendingLoop(whileStmt, &loopEntry, body))
        return false;

    if (!CheckStatement(f, body))
        return false;

    if (!f.bindContinues(whileStmt, maybeLabels))
        return false;

    MDefinition *condDef;
    Type condType;
    if (!CheckExpr(f, cond, &condDef, &condType))
        return false;

    if (!condType.isInt())
        return f.failf(cond, "%s is not a subtype of int", condType.toChars());

    return f.branchAndCloseDoWhileLoop(condDef, loopEntry, NextNode(whileStmt));
}

static bool
CheckLabel(FunctionCompiler &f, ParseNode *labeledStmt, LabelVector *maybeLabels)
{
    JS_ASSERT(labeledStmt->isKind(PNK_LABEL));
    PropertyName *label = LabeledStatementLabel(labeledStmt);
    ParseNode *stmt = LabeledStatementStatement(labeledStmt);

    if (maybeLabels) {
        if (!maybeLabels->append(label))
            return false;
        if (!CheckStatement(f, stmt, maybeLabels))
            return false;
        return true;
    }

    LabelVector labels(f.cx());
    if (!labels.append(label))
        return false;

    if (!CheckStatement(f, stmt, &labels))
        return false;

    return f.bindLabeledBreaks(&labels, labeledStmt);
}

static bool
CheckLeafCondition(FunctionCompiler &f, ParseNode *cond, ParseNode *thenStmt, ParseNode *elseOrJoinStmt,
                   MBasicBlock **thenBlock, MBasicBlock **elseOrJoinBlock)
{
    MDefinition *condDef;
    Type condType;
    if (!CheckExpr(f, cond, &condDef, &condType))
        return false;
    if (!condType.isInt())
        return f.failf(cond, "%s is not a subtype of int", condType.toChars());

    if (!f.branchAndStartThen(condDef, thenBlock, elseOrJoinBlock, thenStmt, elseOrJoinStmt))
        return false;
    return true;
}

static bool
CheckIfCondition(FunctionCompiler &f, ParseNode *cond, ParseNode *thenStmt, ParseNode *elseOrJoinStmt,
                 MBasicBlock **thenBlock, MBasicBlock **elseOrJoinBlock);

static bool
CheckIfConditional(FunctionCompiler &f, ParseNode *conditional, ParseNode *thenStmt, ParseNode *elseOrJoinStmt,
                   MBasicBlock **thenBlock, MBasicBlock **elseOrJoinBlock)
{
    JS_ASSERT(conditional->isKind(PNK_CONDITIONAL));

    // a ? b : c <=> (a && b) || (!a && c)
    // b is always referred to the AND condition, as we need A and B to reach this test,
    // c is always referred as the OR condition, as we reach it if we don't have A.
    ParseNode *cond = TernaryKid1(conditional);
    ParseNode *lhs = TernaryKid2(conditional);
    ParseNode *rhs = TernaryKid3(conditional);

    MBasicBlock *maybeAndTest = nullptr, *maybeOrTest = nullptr;
    MBasicBlock **ifTrueBlock = &maybeAndTest, **ifFalseBlock = &maybeOrTest;
    ParseNode *ifTrueBlockNode = lhs, *ifFalseBlockNode = rhs;

    // Try to spot opportunities for short-circuiting in the AND subpart
    uint32_t andTestLiteral = 0;
    bool skipAndTest = false;

    if (IsLiteralInt(f.m(), lhs, &andTestLiteral)) {
        skipAndTest = true;
        if (andTestLiteral == 0) {
            // (a ? 0 : b) is equivalent to !a && b
            // If a is true, jump to the elseBlock directly
            ifTrueBlock = elseOrJoinBlock;
            ifTrueBlockNode = elseOrJoinStmt;
        } else {
            // (a ? 1 : b) is equivalent to a || b
            // If a is true, jump to the thenBlock directly
            ifTrueBlock = thenBlock;
            ifTrueBlockNode = thenStmt;
        }
    }

    // Try to spot opportunities for short-circuiting in the OR subpart
    uint32_t orTestLiteral = 0;
    bool skipOrTest = false;

    if (IsLiteralInt(f.m(), rhs, &orTestLiteral)) {
        skipOrTest = true;
        if (orTestLiteral == 0) {
            // (a ? b : 0) is equivalent to a && b
            // If a is false, jump to the elseBlock directly
            ifFalseBlock = elseOrJoinBlock;
            ifFalseBlockNode = elseOrJoinStmt;
        } else {
            // (a ? b : 1) is equivalent to !a || b
            // If a is false, jump to the thenBlock directly
            ifFalseBlock = thenBlock;
            ifFalseBlockNode = thenStmt;
        }
    }

    // Pathological cases: a ? 0 : 0 (i.e. false) or a ? 1 : 1 (i.e. true)
    // These cases can't be optimized properly at this point: one of the blocks might be
    // created and won't ever be executed. Furthermore, it introduces inconsistencies in the
    // MIR graph (even if we try to create a block by hand, it will have no predecessor, which
    // breaks graph assumptions). The only way we could optimize it is to do it directly in
    // CheckIf by removing the control flow entirely.
    if (skipOrTest && skipAndTest && (!!orTestLiteral == !!andTestLiteral))
        return CheckLeafCondition(f, conditional, thenStmt, elseOrJoinStmt, thenBlock, elseOrJoinBlock);

    if (!CheckIfCondition(f, cond, ifTrueBlockNode, ifFalseBlockNode, ifTrueBlock, ifFalseBlock))
        return false;
    f.assertCurrentBlockIs(*ifTrueBlock);

    // Add supplementary tests, if needed
    if (!skipAndTest) {
        if (!CheckIfCondition(f, lhs, thenStmt, elseOrJoinStmt, thenBlock, elseOrJoinBlock))
            return false;
        f.assertCurrentBlockIs(*thenBlock);
    }

    if (!skipOrTest) {
        f.switchToElse(*ifFalseBlock);
        if (!CheckIfCondition(f, rhs, thenStmt, elseOrJoinStmt, thenBlock, elseOrJoinBlock))
            return false;
        f.assertCurrentBlockIs(*thenBlock);
    }

    // We might not be on the thenBlock in one case
    if (ifTrueBlock == elseOrJoinBlock) {
        JS_ASSERT(skipAndTest && andTestLiteral == 0);
        f.switchToElse(*thenBlock);
    }

    // Check post-conditions
    f.assertCurrentBlockIs(*thenBlock);
    JS_ASSERT_IF(!f.inDeadCode(), *thenBlock && *elseOrJoinBlock);
    return true;
}

/*
 * Recursive function that checks for a complex condition (formed with ternary
 * conditionals) and creates the associated short-circuiting control flow graph.
 *
 * After a call to CheckCondition, the followings are true:
 * - if *thenBlock and *elseOrJoinBlock were non-null on entry, their value is
 *   not changed by this function.
 * - *thenBlock and *elseOrJoinBlock are non-null on exit.
 * - the current block on exit is the *thenBlock.
 */
static bool
CheckIfCondition(FunctionCompiler &f, ParseNode *cond, ParseNode *thenStmt,
                 ParseNode *elseOrJoinStmt, MBasicBlock **thenBlock, MBasicBlock **elseOrJoinBlock)
{
    JS_CHECK_RECURSION_DONT_REPORT(f.cx(), return f.m().failOverRecursed());

    if (cond->isKind(PNK_CONDITIONAL))
        return CheckIfConditional(f, cond, thenStmt, elseOrJoinStmt, thenBlock, elseOrJoinBlock);

    // We've reached a leaf, i.e. an atomic condition
    JS_ASSERT(!cond->isKind(PNK_CONDITIONAL));
    if (!CheckLeafCondition(f, cond, thenStmt, elseOrJoinStmt, thenBlock, elseOrJoinBlock))
        return false;

    // Check post-conditions
    f.assertCurrentBlockIs(*thenBlock);
    JS_ASSERT_IF(!f.inDeadCode(), *thenBlock && *elseOrJoinBlock);
    return true;
}

static bool
CheckIf(FunctionCompiler &f, ParseNode *ifStmt)
{
    // Handle if/else-if chains using iteration instead of recursion. This
    // avoids blowing the C stack quota for long if/else-if chains and also
    // creates fewer MBasicBlocks at join points (by creating one join block
    // for the entire if/else-if chain).
    BlockVector thenBlocks(f.cx());

    ParseNode *nextStmt = NextNode(ifStmt);
  recurse:
    JS_ASSERT(ifStmt->isKind(PNK_IF));
    ParseNode *cond = TernaryKid1(ifStmt);
    ParseNode *thenStmt = TernaryKid2(ifStmt);
    ParseNode *elseStmt = TernaryKid3(ifStmt);

    MBasicBlock *thenBlock = nullptr, *elseBlock = nullptr;
    ParseNode *elseOrJoinStmt = elseStmt ? elseStmt : nextStmt;

    if (!CheckIfCondition(f, cond, thenStmt, elseOrJoinStmt, &thenBlock, &elseBlock))
        return false;

    if (!CheckStatement(f, thenStmt))
        return false;

    if (!f.appendThenBlock(&thenBlocks))
        return false;

    if (!elseStmt) {
        if (!f.joinIf(thenBlocks, elseBlock))
            return false;
    } else {
        f.switchToElse(elseBlock);

        if (elseStmt->isKind(PNK_IF)) {
            ifStmt = elseStmt;
            goto recurse;
        }

        if (!CheckStatement(f, elseStmt))
            return false;

        if (!f.joinIfElse(thenBlocks, nextStmt))
            return false;
    }

    return true;
}

static bool
CheckCaseExpr(FunctionCompiler &f, ParseNode *caseExpr, int32_t *value)
{
    if (!IsNumericLiteral(f.m(), caseExpr))
        return f.fail(caseExpr, "switch case expression must be an integer literal");

    NumLit literal = ExtractNumericLiteral(f.m(), caseExpr);
    switch (literal.which()) {
      case NumLit::Fixnum:
      case NumLit::NegativeInt:
        *value = literal.toInt32();
        break;
      case NumLit::OutOfRangeInt:
      case NumLit::BigUnsigned:
        return f.fail(caseExpr, "switch case expression out of integer range");
      case NumLit::Double:
      case NumLit::Float:
        return f.fail(caseExpr, "switch case expression must be an integer literal");
    }

    return true;
}

static bool
CheckDefaultAtEnd(FunctionCompiler &f, ParseNode *stmt)
{
    for (; stmt; stmt = NextNode(stmt)) {
        JS_ASSERT(stmt->isKind(PNK_CASE) || stmt->isKind(PNK_DEFAULT));
        if (stmt->isKind(PNK_DEFAULT) && NextNode(stmt) != nullptr)
            return f.fail(stmt, "default label must be at the end");
    }

    return true;
}

static bool
CheckSwitchRange(FunctionCompiler &f, ParseNode *stmt, int32_t *low, int32_t *high,
                 int32_t *tableLength)
{
    if (stmt->isKind(PNK_DEFAULT)) {
        *low = 0;
        *high = -1;
        *tableLength = 0;
        return true;
    }

    int32_t i = 0;
    if (!CheckCaseExpr(f, CaseExpr(stmt), &i))
        return false;

    *low = *high = i;

    ParseNode *initialStmt = stmt;
    for (stmt = NextNode(stmt); stmt && stmt->isKind(PNK_CASE); stmt = NextNode(stmt)) {
        int32_t i = 0;
        if (!CheckCaseExpr(f, CaseExpr(stmt), &i))
            return false;

        *low = Min(*low, i);
        *high = Max(*high, i);
    }

    int64_t i64 = (int64_t(*high) - int64_t(*low)) + 1;
    if (i64 > 4*1024*1024)
        return f.fail(initialStmt, "all switch statements generate tables; this table would be too big");

    *tableLength = int32_t(i64);
    return true;
}

static bool
CheckSwitch(FunctionCompiler &f, ParseNode *switchStmt)
{
    JS_ASSERT(switchStmt->isKind(PNK_SWITCH));
    ParseNode *switchExpr = BinaryLeft(switchStmt);
    ParseNode *switchBody = BinaryRight(switchStmt);

    if (!switchBody->isKind(PNK_STATEMENTLIST))
        return f.fail(switchBody, "switch body may not contain 'let' declarations");

    MDefinition *exprDef;
    Type exprType;
    if (!CheckExpr(f, switchExpr, &exprDef, &exprType))
        return false;

    if (!exprType.isSigned())
        return f.failf(switchExpr, "%s is not a subtype of signed", exprType.toChars());

    ParseNode *stmt = ListHead(switchBody);

    if (!CheckDefaultAtEnd(f, stmt))
        return false;

    if (!stmt)
        return true;

    int32_t low = 0, high = 0, tableLength = 0;
    if (!CheckSwitchRange(f, stmt, &low, &high, &tableLength))
        return false;

    BlockVector cases(f.cx());
    if (!cases.resize(tableLength))
        return false;

    MBasicBlock *switchBlock;
    if (!f.startSwitch(switchStmt, exprDef, low, high, &switchBlock))
        return false;

    for (; stmt && stmt->isKind(PNK_CASE); stmt = NextNode(stmt)) {
        int32_t caseValue = ExtractNumericLiteral(f.m(), CaseExpr(stmt)).toInt32();
        unsigned caseIndex = caseValue - low;

        if (cases[caseIndex])
            return f.fail(stmt, "no duplicate case labels");

        if (!f.startSwitchCase(switchBlock, &cases[caseIndex], stmt))
            return false;

        if (!CheckStatement(f, CaseBody(stmt)))
            return false;
    }

    MBasicBlock *defaultBlock;
    if (!f.startSwitchDefault(switchBlock, &cases, &defaultBlock, stmt))
        return false;

    if (stmt && stmt->isKind(PNK_DEFAULT)) {
        if (!CheckStatement(f, CaseBody(stmt)))
            return false;
    }

    return f.joinSwitch(switchBlock, cases, defaultBlock);
}

static bool
CheckReturnType(FunctionCompiler &f, ParseNode *usepn, RetType retType)
{
    if (!f.hasAlreadyReturned()) {
        f.setReturnedType(retType);
        return true;
    }

    if (f.returnedType() != retType) {
        return f.failf(usepn, "%s incompatible with previous return of type %s",
                       retType.toType().toChars(), f.returnedType().toType().toChars());
    }

    return true;
}

static bool
CheckReturn(FunctionCompiler &f, ParseNode *returnStmt)
{
    ParseNode *expr = ReturnExpr(returnStmt);

    if (!expr) {
        if (!CheckReturnType(f, returnStmt, RetType::Void))
            return false;

        f.returnVoid();
        return true;
    }

    MDefinition *def;
    Type type;
    if (!CheckExpr(f, expr, &def, &type))
        return false;

    RetType retType;
    if (type.isSigned())
        retType = RetType::Signed;
    else if (type.isDouble())
        retType = RetType::Double;
    else if (type.isFloat())
        retType = RetType::Float;
    else if (type.isVoid())
        retType = RetType::Void;
    else
        return f.failf(expr, "%s is not a valid return type", type.toChars());

    if (!CheckReturnType(f, expr, retType))
        return false;

    if (retType == RetType::Void)
        f.returnVoid();
    else
        f.returnExpr(def);
    return true;
}

static bool
CheckStatementList(FunctionCompiler &f, ParseNode *stmtList)
{
    JS_ASSERT(stmtList->isKind(PNK_STATEMENTLIST));

    for (ParseNode *stmt = ListHead(stmtList); stmt; stmt = NextNode(stmt)) {
        if (!CheckStatement(f, stmt))
            return false;
    }

    return true;
}

static bool
CheckStatement(FunctionCompiler &f, ParseNode *stmt, LabelVector *maybeLabels)
{
    JS_CHECK_RECURSION_DONT_REPORT(f.cx(), return f.m().failOverRecursed());

    if (!f.mirGen().ensureBallast())
        return false;

    switch (stmt->getKind()) {
      case PNK_SEMI:          return CheckExprStatement(f, stmt);
      case PNK_WHILE:         return CheckWhile(f, stmt, maybeLabels);
      case PNK_FOR:           return CheckFor(f, stmt, maybeLabels);
      case PNK_DOWHILE:       return CheckDoWhile(f, stmt, maybeLabels);
      case PNK_LABEL:         return CheckLabel(f, stmt, maybeLabels);
      case PNK_IF:            return CheckIf(f, stmt);
      case PNK_SWITCH:        return CheckSwitch(f, stmt);
      case PNK_RETURN:        return CheckReturn(f, stmt);
      case PNK_STATEMENTLIST: return CheckStatementList(f, stmt);
      case PNK_BREAK:         return f.addBreak(LoopControlMaybeLabel(stmt));
      case PNK_CONTINUE:      return f.addContinue(LoopControlMaybeLabel(stmt));
      default:;
    }

    return f.fail(stmt, "unexpected statement kind");
}

static bool
ParseFunction(ModuleCompiler &m, ParseNode **fnOut)
{
    TokenStream &tokenStream = m.parser().tokenStream;

    DebugOnly<TokenKind> tk = tokenStream.getToken();
    JS_ASSERT(tk == TOK_FUNCTION);

    RootedPropertyName name(m.cx());

    TokenKind tt = tokenStream.getToken();
    if (tt == TOK_NAME) {
        name = tokenStream.currentName();
    } else if (tt == TOK_YIELD) {
        if (!m.parser().checkYieldNameValidity())
            return false;
        name = m.cx()->names().yield;
    } else {
        return false;  // The regular parser will throw a SyntaxError, no need to m.fail.
    }

    ParseNode *fn = m.parser().handler.newFunctionDefinition();
    if (!fn)
        return false;

    // This flows into FunctionBox, so must be tenured.
    RootedFunction fun(m.cx(), NewFunction(m.cx(), NullPtr(), nullptr, 0, JSFunction::INTERPRETED,
                                           m.cx()->global(), name, JSFunction::FinalizeKind,
                                           TenuredObject));
    if (!fun)
        return false;

    AsmJSParseContext *outerpc = m.parser().pc;

    Directives directives(outerpc);
    FunctionBox *funbox = m.parser().newFunctionBox(fn, fun, outerpc, directives, NotGenerator);
    if (!funbox)
        return false;

    Directives newDirectives = directives;
    AsmJSParseContext funpc(&m.parser(), outerpc, fn, funbox, &newDirectives,
                            outerpc->staticLevel + 1, outerpc->blockidGen,
                            /* blockScopeDepth = */ 0);
    if (!funpc.init(m.parser().tokenStream))
        return false;

    if (!m.parser().functionArgsAndBodyGeneric(fn, fun, Normal, Statement, &newDirectives))
        return false;

    if (tokenStream.hadError() || directives != newDirectives)
        return false;

    outerpc->blockidGen = funpc.blockidGen;
    fn->pn_blockid = outerpc->blockid();

    *fnOut = fn;
    return true;
}

static bool
CheckFunction(ModuleCompiler &m, LifoAlloc &lifo, MIRGenerator **mir, ModuleCompiler::Func **funcOut)
{
    int64_t before = PRMJ_Now();

    // asm.js modules can be quite large when represented as parse trees so pop
    // the backing LifoAlloc after parsing/compiling each function.
    AsmJSParser::Mark mark = m.parser().mark();

    ParseNode *fn;
    if (!ParseFunction(m, &fn))
        return false;

    if (!CheckFunctionHead(m, fn))
        return false;

    FunctionCompiler f(m, fn, lifo);
    if (!f.init())
        return false;

    ParseNode *stmtIter = ListHead(FunctionStatementList(fn));

    VarTypeVector argTypes(m.lifo());
    if (!CheckArguments(f, &stmtIter, &argTypes))
        return false;

    if (!CheckVariables(f, &stmtIter))
        return false;

    if (!f.prepareToEmitMIR(argTypes))
        return false;

    ParseNode *lastNonEmptyStmt = nullptr;
    for (; stmtIter; stmtIter = NextNode(stmtIter)) {
        if (!CheckStatement(f, stmtIter))
            return false;
        if (!IsEmptyStatement(stmtIter))
            lastNonEmptyStmt = stmtIter;
    }

    RetType retType;
    if (!CheckFinalReturn(f, lastNonEmptyStmt, &retType))
        return false;

    if (!CheckReturnType(f, lastNonEmptyStmt, retType))
        return false;

    Signature sig(Move(argTypes), retType);
    ModuleCompiler::Func *func = nullptr;
    if (!CheckFunctionSignature(m, fn, Move(sig), FunctionName(fn), &func))
        return false;

    if (func->defined())
        return m.failName(fn, "function '%s' already defined", FunctionName(fn));

    func->define(fn->pn_pos.begin);
    func->accumulateCompileTime((PRMJ_Now() - before) / PRMJ_USEC_PER_MSEC);

    m.parser().release(mark);

    // Copy the cumulative minimum heap size constraint to the MIR for use in analysis.  The length
    // is also constrained to particular lengths, so firstly round up - a larger 'heap required
    // length' can help range analysis to prove that bounds checks are not needed.
    uint32_t len = js::RoundUpToNextValidAsmJSHeapLength(m.minHeapLength());
    m.requireHeapLengthToBeAtLeast(len);

    *mir = f.extractMIR();
    (*mir)->noteMinAsmJSHeapLength(len);
    *funcOut = func;
    return true;
}

static bool
GenerateCode(ModuleCompiler &m, ModuleCompiler::Func &func, MIRGenerator &mir, LIRGraph &lir)
{
    int64_t before = PRMJ_Now();

    // A single MacroAssembler is reused for all function compilations so
    // that there is a single linear code segment for each module. To avoid
    // spiking memory, a LifoAllocScope in the caller frees all MIR/LIR
    // after each function is compiled. This method is responsible for cleaning
    // out any dangling pointers that the MacroAssembler may have kept.
    m.masm().resetForNewCodeGenerator(mir.alloc());

    m.masm().bind(func.code());

    ScopedJSDeletePtr<CodeGenerator> codegen(jit::GenerateCode(&mir, &lir, &m.masm()));
    if (!codegen)
        return m.fail(nullptr, "internal codegen failure (probably out of memory)");

    if (!m.collectAccesses(mir))
        return false;

    jit::IonScriptCounts *counts = codegen->extractUnassociatedScriptCounts();
    if (counts && !m.addFunctionCounts(counts)) {
        js_delete(counts);
        return false;
    }

#if defined(MOZ_VTUNE) || defined(JS_ION_PERF)
    // Profiling might not be active now, but it may be activated later (perhaps
    // after the module has been cached and reloaded from the cache). Function
    // profiling info isn't huge, so store it always (in --enable-profiling
    // builds, which is only Nightly builds, but default).
    if (!m.trackProfiledFunction(func, m.masm().size()))
        return false;
#endif

#ifdef JS_ION_PERF
    // Per-block profiling info uses significantly more memory so only store
    // this information if it is actively requested.
    if (PerfBlockEnabled()) {
        if (!m.trackPerfProfiledBlocks(mir.perfSpewer(), func, m.masm().size()))
            return false;
    }
#endif

    // Align internal function headers.
    m.masm().align(CodeAlignment);

    func.accumulateCompileTime((PRMJ_Now() - before) / PRMJ_USEC_PER_MSEC);
    if (!m.maybeReportCompileTime(func))
        return false;

    // Unlike regular IonMonkey which links and generates a new JitCode for
    // every function, we accumulate all the functions in the module in a
    // single MacroAssembler and link at end. Linking asm.js doesn't require a
    // CodeGenerator so we can destroy it now.
    return true;
}

static bool
CheckAllFunctionsDefined(ModuleCompiler &m)
{
    for (unsigned i = 0; i < m.numFunctions(); i++) {
        if (!m.function(i).code()->bound())
            return m.failName(nullptr, "missing definition of function %s", m.function(i).name());
    }

    return true;
}

static bool
CheckFunctionsSequential(ModuleCompiler &m)
{
    // Use a single LifoAlloc to allocate all the temporary compiler IR.
    // All allocated LifoAlloc'd memory is released after compiling each
    // function by the LifoAllocScope inside the loop.
    LifoAlloc lifo(LIFO_ALLOC_PRIMARY_CHUNK_SIZE);

    while (PeekToken(m.parser()) == TOK_FUNCTION) {
        LifoAllocScope scope(&lifo);

        MIRGenerator *mir;
        ModuleCompiler::Func *func;
        if (!CheckFunction(m, lifo, &mir, &func))
            return false;

        int64_t before = PRMJ_Now();

        IonContext icx(m.cx(), &mir->alloc());

        IonSpewNewFunction(&mir->graph(), NullPtr());

        if (!OptimizeMIR(mir))
            return m.failOffset(func->srcOffset(), "internal compiler failure (probably out of memory)");

        LIRGraph *lir = GenerateLIR(mir);
        if (!lir)
            return m.failOffset(func->srcOffset(), "internal compiler failure (probably out of memory)");

        func->accumulateCompileTime((PRMJ_Now() - before) / PRMJ_USEC_PER_MSEC);

        if (!GenerateCode(m, *func, *mir, *lir))
            return false;

        IonSpewEndFunction();
    }

    if (!CheckAllFunctionsDefined(m))
        return false;

    return true;
}

#ifdef JS_THREADSAFE

// Currently, only one asm.js parallel compilation is allowed at a time.
// This RAII class attempts to claim this parallel compilation using atomic ops
// on rt->workerThreadState->asmJSCompilationInProgress.
class ParallelCompilationGuard
{
    bool parallelState_;
  public:
    ParallelCompilationGuard() : parallelState_(false) {}
    ~ParallelCompilationGuard() {
        if (parallelState_) {
            JS_ASSERT(WorkerThreadState().asmJSCompilationInProgress == true);
            WorkerThreadState().asmJSCompilationInProgress = false;
        }
    }
    bool claim() {
        JS_ASSERT(!parallelState_);
        if (!WorkerThreadState().asmJSCompilationInProgress.compareExchange(false, true))
            return false;
        parallelState_ = true;
        return true;
    }
};

static bool
ParallelCompilationEnabled(ExclusiveContext *cx)
{
    // If 'cx' isn't a JSContext, then we are already off the main thread so
    // off-thread compilation must be enabled. However, since there are a fixed
    // number of worker threads and one is already being consumed by this
    // parsing task, ensure that there another free thread to avoid deadlock.
    // (Note: there is at most one thread used for parsing so we don't have to
    // worry about general dining philosophers.)
    if (WorkerThreadState().threadCount <= 1)
        return false;

    if (!cx->isJSContext())
        return true;
    return cx->asJSContext()->runtime()->canUseParallelIonCompilation();
}

// State of compilation as tracked and updated by the main thread.
struct ParallelGroupState
{
    js::Vector<AsmJSParallelTask> &tasks;
    int32_t outstandingJobs; // Good work, jobs!
    uint32_t compiledJobs;

    ParallelGroupState(js::Vector<AsmJSParallelTask> &tasks)
      : tasks(tasks), outstandingJobs(0), compiledJobs(0)
    { }
};

// Block until a worker-assigned LifoAlloc becomes finished.
static AsmJSParallelTask *
GetFinishedCompilation(ModuleCompiler &m, ParallelGroupState &group)
{
    AutoLockWorkerThreadState lock;

    while (!WorkerThreadState().asmJSWorkerFailed()) {
        if (!WorkerThreadState().asmJSFinishedList().empty()) {
            group.outstandingJobs--;
            return WorkerThreadState().asmJSFinishedList().popCopy();
        }
        WorkerThreadState().wait(GlobalWorkerThreadState::CONSUMER);
    }

    return nullptr;
}

static bool
GenerateCodeForFinishedJob(ModuleCompiler &m, ParallelGroupState &group, AsmJSParallelTask **outTask)
{
    // Block until a used LifoAlloc becomes available.
    AsmJSParallelTask *task = GetFinishedCompilation(m, group);
    if (!task)
        return false;

    ModuleCompiler::Func &func = *reinterpret_cast<ModuleCompiler::Func *>(task->func);
    func.accumulateCompileTime(task->compileTime);

    {
        // Perform code generation on the main thread.
        IonContext ionContext(m.cx(), &task->mir->alloc());
        if (!GenerateCode(m, func, *task->mir, *task->lir))
            return false;
    }

    group.compiledJobs++;

    // Clear the LifoAlloc for use by another worker.
    TempAllocator &tempAlloc = task->mir->alloc();
    tempAlloc.TempAllocator::~TempAllocator();
    task->lifo.releaseAll();

    *outTask = task;
    return true;
}

static inline bool
GetUnusedTask(ParallelGroupState &group, uint32_t i, AsmJSParallelTask **outTask)
{
    // Since functions are dispatched in order, if fewer than |numLifos| functions
    // have been generated, then the |i'th| LifoAlloc must never have been
    // assigned to a worker thread.
    if (i >= group.tasks.length())
        return false;
    *outTask = &group.tasks[i];
    return true;
}

static bool
CheckFunctionsParallelImpl(ModuleCompiler &m, ParallelGroupState &group)
{
#ifdef DEBUG
    {
        AutoLockWorkerThreadState lock;
        JS_ASSERT(WorkerThreadState().asmJSWorklist().empty());
        JS_ASSERT(WorkerThreadState().asmJSFinishedList().empty());
    }
#endif
    WorkerThreadState().resetAsmJSFailureState();

    for (unsigned i = 0; PeekToken(m.parser()) == TOK_FUNCTION; i++) {
        // Get exclusive access to an empty LifoAlloc from the thread group's pool.
        AsmJSParallelTask *task = nullptr;
        if (!GetUnusedTask(group, i, &task) && !GenerateCodeForFinishedJob(m, group, &task))
            return false;

        // Generate MIR into the LifoAlloc on the main thread.
        MIRGenerator *mir;
        ModuleCompiler::Func *func;
        if (!CheckFunction(m, task->lifo, &mir, &func))
            return false;

        // Perform optimizations and LIR generation on a worker thread.
        task->init(m.cx()->compartment()->runtimeFromAnyThread(), func, mir);
        if (!StartOffThreadAsmJSCompile(m.cx(), task))
            return false;

        group.outstandingJobs++;
    }

    // Block for all outstanding workers to complete.
    while (group.outstandingJobs > 0) {
        AsmJSParallelTask *ignored = nullptr;
        if (!GenerateCodeForFinishedJob(m, group, &ignored))
            return false;
    }

    if (!CheckAllFunctionsDefined(m))
        return false;

    JS_ASSERT(group.outstandingJobs == 0);
    JS_ASSERT(group.compiledJobs == m.numFunctions());
#ifdef DEBUG
    {
        AutoLockWorkerThreadState lock;
        JS_ASSERT(WorkerThreadState().asmJSWorklist().empty());
        JS_ASSERT(WorkerThreadState().asmJSFinishedList().empty());
    }
#endif
    JS_ASSERT(!WorkerThreadState().asmJSWorkerFailed());
    return true;
}

static void
CancelOutstandingJobs(ModuleCompiler &m, ParallelGroupState &group)
{
    // This is failure-handling code, so it's not allowed to fail.
    // The problem is that all memory for compilation is stored in LifoAllocs
    // maintained in the scope of CheckFunctionsParallel() -- so in order
    // for that function to safely return, and thereby remove the LifoAllocs,
    // none of that memory can be in use or reachable by workers.

    JS_ASSERT(group.outstandingJobs >= 0);
    if (!group.outstandingJobs)
        return;

    AutoLockWorkerThreadState lock;

    // From the compiling tasks, eliminate those waiting for worker assignation.
    group.outstandingJobs -= WorkerThreadState().asmJSWorklist().length();
    WorkerThreadState().asmJSWorklist().clear();

    // From the compiling tasks, eliminate those waiting for codegen.
    group.outstandingJobs -= WorkerThreadState().asmJSFinishedList().length();
    WorkerThreadState().asmJSFinishedList().clear();

    // Eliminate tasks that failed without adding to the finished list.
    group.outstandingJobs -= WorkerThreadState().harvestFailedAsmJSJobs();

    // Any remaining tasks are therefore undergoing active compilation.
    JS_ASSERT(group.outstandingJobs >= 0);
    while (group.outstandingJobs > 0) {
        WorkerThreadState().wait(GlobalWorkerThreadState::CONSUMER);

        group.outstandingJobs -= WorkerThreadState().harvestFailedAsmJSJobs();
        group.outstandingJobs -= WorkerThreadState().asmJSFinishedList().length();
        WorkerThreadState().asmJSFinishedList().clear();
    }

    JS_ASSERT(group.outstandingJobs == 0);
    JS_ASSERT(WorkerThreadState().asmJSWorklist().empty());
    JS_ASSERT(WorkerThreadState().asmJSFinishedList().empty());
}

static const size_t LIFO_ALLOC_PARALLEL_CHUNK_SIZE = 1 << 12;

static bool
CheckFunctionsParallel(ModuleCompiler &m)
{
    // If parallel compilation isn't enabled (not enough cores, disabled by
    // pref, etc) or another thread is currently compiling asm.js in parallel,
    // fall back to sequential compilation. (We could lift the latter
    // constraint by hoisting asmJS* state out of WorkerThreadState so multiple
    // concurrent asm.js parallel compilations don't race.)
    ParallelCompilationGuard g;
    if (!ParallelCompilationEnabled(m.cx()) || !g.claim())
        return CheckFunctionsSequential(m);

    IonSpew(IonSpew_Logs, "Can't log asm.js script. (Compiled on background thread.)");

    // Saturate all worker threads plus the main thread.
    size_t numParallelJobs = WorkerThreadState().threadCount + 1;

    // Allocate scoped AsmJSParallelTask objects. Each contains a unique
    // LifoAlloc that provides all necessary memory for compilation.
    js::Vector<AsmJSParallelTask, 0> tasks(m.cx());
    if (!tasks.initCapacity(numParallelJobs))
        return false;

    for (size_t i = 0; i < numParallelJobs; i++)
        tasks.infallibleAppend(LIFO_ALLOC_PARALLEL_CHUNK_SIZE);

    // With compilation memory in-scope, dispatch worker threads.
    ParallelGroupState group(tasks);
    if (!CheckFunctionsParallelImpl(m, group)) {
        CancelOutstandingJobs(m, group);

        // If failure was triggered by a worker thread, report error.
        if (void *maybeFunc = WorkerThreadState().maybeAsmJSFailedFunction()) {
            ModuleCompiler::Func *func = reinterpret_cast<ModuleCompiler::Func *>(maybeFunc);
            return m.failOffset(func->srcOffset(), "allocation failure during compilation");
        }

        // Otherwise, the error occurred on the main thread and was already reported.
        return false;
    }
    return true;
}
#endif // JS_THREADSAFE

static bool
CheckFuncPtrTable(ModuleCompiler &m, ParseNode *var)
{
    if (!IsDefinition(var))
        return m.fail(var, "function-pointer table name must be unique");

    ParseNode *arrayLiteral = MaybeDefinitionInitializer(var);
    if (!arrayLiteral || !arrayLiteral->isKind(PNK_ARRAY))
        return m.fail(var, "function-pointer table's initializer must be an array literal");

    unsigned length = ListLength(arrayLiteral);

    if (!IsPowerOfTwo(length))
        return m.failf(arrayLiteral, "function-pointer table length must be a power of 2 (is %u)", length);

    unsigned mask = length - 1;

    ModuleCompiler::FuncPtrVector elems(m.cx());
    const Signature *firstSig = nullptr;

    for (ParseNode *elem = ListHead(arrayLiteral); elem; elem = NextNode(elem)) {
        if (!elem->isKind(PNK_NAME))
            return m.fail(elem, "function-pointer table's elements must be names of functions");

        PropertyName *funcName = elem->name();
        const ModuleCompiler::Func *func = m.lookupFunction(funcName);
        if (!func)
            return m.fail(elem, "function-pointer table's elements must be names of functions");

        if (firstSig) {
            if (*firstSig != func->sig())
                return m.fail(elem, "all functions in table must have same signature");
        } else {
            firstSig = &func->sig();
        }

        if (!elems.append(func))
            return false;
    }

    Signature sig(m.lifo());
    if (!sig.copy(*firstSig))
        return false;

    ModuleCompiler::FuncPtrTable *table;
    if (!CheckFuncPtrTableAgainstExisting(m, var, var->name(), Move(sig), mask, &table))
        return false;

    table->initElems(Move(elems));
    return true;
}

static bool
CheckFuncPtrTables(ModuleCompiler &m)
{
    while (true) {
        ParseNode *varStmt;
        if (!ParseVarOrConstStatement(m.parser(), &varStmt))
            return false;
        if (!varStmt)
            break;
        for (ParseNode *var = VarListHead(varStmt); var; var = NextNode(var)) {
            if (!CheckFuncPtrTable(m, var))
                return false;
        }
    }

    for (unsigned i = 0; i < m.numFuncPtrTables(); i++) {
        if (!m.funcPtrTable(i).initialized())
            return m.fail(nullptr, "expecting function-pointer table");
    }

    return true;
}

static bool
CheckModuleExportFunction(ModuleCompiler &m, ParseNode *returnExpr)
{
    if (!returnExpr->isKind(PNK_NAME))
        return m.fail(returnExpr, "export statement must be of the form 'return name'");

    PropertyName *funcName = returnExpr->name();

    const ModuleCompiler::Func *func = m.lookupFunction(funcName);
    if (!func)
        return m.failName(returnExpr, "exported function name '%s' not found", funcName);

    return m.addExportedFunction(func, /* maybeFieldName = */ nullptr);
}

static bool
CheckModuleExportObject(ModuleCompiler &m, ParseNode *object)
{
    JS_ASSERT(object->isKind(PNK_OBJECT));

    for (ParseNode *pn = ListHead(object); pn; pn = NextNode(pn)) {
        if (!IsNormalObjectField(m.cx(), pn))
            return m.fail(pn, "only normal object properties may be used in the export object literal");

        PropertyName *fieldName = ObjectNormalFieldName(m.cx(), pn);

        ParseNode *initNode = ObjectFieldInitializer(pn);
        if (!initNode->isKind(PNK_NAME))
            return m.fail(initNode, "initializer of exported object literal must be name of function");

        PropertyName *funcName = initNode->name();

        const ModuleCompiler::Func *func = m.lookupFunction(funcName);
        if (!func)
            return m.failName(initNode, "exported function name '%s' not found", funcName);

        if (!m.addExportedFunction(func, fieldName))
            return false;
    }

    return true;
}

static bool
CheckModuleReturn(ModuleCompiler &m)
{
    if (PeekToken(m.parser()) != TOK_RETURN) {
        TokenKind tk = PeekToken(m.parser());
        if (tk == TOK_RC || tk == TOK_EOF)
            return m.fail(nullptr, "expecting return statement");
        return m.fail(nullptr, "invalid asm.js statement");
    }

    ParseNode *returnStmt = m.parser().statement();
    if (!returnStmt)
        return false;

    ParseNode *returnExpr = ReturnExpr(returnStmt);
    if (!returnExpr)
        return m.fail(returnStmt, "export statement must return something");

    if (returnExpr->isKind(PNK_OBJECT)) {
        if (!CheckModuleExportObject(m, returnExpr))
            return false;
    } else {
        if (!CheckModuleExportFunction(m, returnExpr))
            return false;
    }

    // Function statements are not added to the lexical scope in ParseContext
    // (since cx->tempLifoAlloc is marked/released after each function
    // statement) and thus all the identifiers in the return statement will be
    // mistaken as free variables and added to lexdeps. Clear these now.
    m.parser().pc->lexdeps->clear();
    return true;
}

// All registers except the stack pointer.
static const RegisterSet AllRegsExceptSP =
    RegisterSet(GeneralRegisterSet(Registers::AllMask &
                                   ~(uint32_t(1) << Registers::StackPointer)),
                FloatRegisterSet(FloatRegisters::AllMask));
#if defined(JS_CODEGEN_ARM)
// The ARM system ABI also includes d15 in the non volatile float registers.
static const RegisterSet NonVolatileRegs =
    RegisterSet(GeneralRegisterSet(Registers::NonVolatileMask),
                    FloatRegisterSet(FloatRegisters::NonVolatileMask | (1 << FloatRegisters::d15)));
#else
static const RegisterSet NonVolatileRegs =
    RegisterSet(GeneralRegisterSet(Registers::NonVolatileMask),
                FloatRegisterSet(FloatRegisters::NonVolatileMask));
#endif

static void
LoadAsmJSActivationIntoRegister(MacroAssembler &masm, Register reg)
{
    masm.movePtr(AsmJSImm_Runtime, reg);
    size_t offset = offsetof(JSRuntime, mainThread) +
                    PerThreadData::offsetOfAsmJSActivationStackReadOnly();
    masm.loadPtr(Address(reg, offset), reg);
}

static void
LoadJSContextFromActivation(MacroAssembler &masm, Register activation, Register dest)
{
    masm.loadPtr(Address(activation, AsmJSActivation::offsetOfContext()), dest);
}

static void
AssertStackAlignment(MacroAssembler &masm)
{
    JS_ASSERT((AlignmentAtPrologue + masm.framePushed()) % StackAlignment == 0);
#ifdef DEBUG
    Label ok;
    JS_ASSERT(IsPowerOfTwo(StackAlignment));
    masm.branchTestPtr(Assembler::Zero, StackPointer, Imm32(StackAlignment - 1), &ok);
    masm.assumeUnreachable("Stack should be aligned.");
    masm.bind(&ok);
#endif
}

template <class VectorT>
static unsigned
StackArgBytes(const VectorT &argTypes)
{
    ABIArgIter<VectorT> iter(argTypes);
    while (!iter.done())
        iter++;
    return iter.stackBytesConsumedSoFar();
}

template <class VectorT>
static unsigned
StackDecrementForCall(MacroAssembler &masm, const VectorT &argTypes, unsigned extraBytes = 0)
{
    // Include extra padding so that, after pushing the arguments and
    // extraBytes, the stack is aligned for a call instruction.
    unsigned argBytes = StackArgBytes(argTypes);
    unsigned alreadyPushed = AlignmentAtPrologue + masm.framePushed();
    return AlignBytes(alreadyPushed + extraBytes + argBytes, StackAlignment) - alreadyPushed;
}

static const unsigned FramePushedAfterSave = NonVolatileRegs.gprs().size() * sizeof(intptr_t) +
                                             NonVolatileRegs.fpus().size() * sizeof(double);

static bool
GenerateEntry(ModuleCompiler &m, const AsmJSModule::ExportedFunction &exportedFunc)
{
    MacroAssembler &masm = m.masm();

    // In constrast to the system ABI, the Ion convention is that all registers
    // are clobbered by calls. Thus, we must save the caller's non-volatile
    // registers.
    //
    // NB: GenerateExits assumes that masm.framePushed() == 0 before
    // PushRegsInMask(NonVolatileRegs).
    masm.setFramePushed(0);
    masm.PushRegsInMask(NonVolatileRegs);
    JS_ASSERT(masm.framePushed() == FramePushedAfterSave);

    // Remember the stack pointer in the current AsmJSActivation. This will be
    // used by error exit paths to set the stack pointer back to what it was
    // right after the (C++) caller's non-volatile registers were saved so that
    // they can be restored.
    Register activation = ABIArgGenerator::NonArgReturnVolatileReg0;
    LoadAsmJSActivationIntoRegister(masm, activation);
    masm.storePtr(StackPointer, Address(activation, AsmJSActivation::offsetOfErrorRejoinSP()));

    // ARM has a globally-pinned GlobalReg (x64 uses RIP-relative addressing,
    // x86 uses immediates in effective addresses) and NaN register (used as
    // part of the out-of-bounds handling in heap loads/stores).
#if defined(JS_CODEGEN_ARM)
    masm.movePtr(IntArgReg1, GlobalReg);
    masm.ma_vimm(GenericNaN(), NANReg);
#endif

    // ARM and x64 have a globally-pinned HeapReg (x86 uses immediates in
    // effective addresses).
#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_ARM)
    masm.loadPtr(Address(IntArgReg1, m.module().heapOffset()), HeapReg);
#endif

    // Get 'argv' into a non-arg register and save it on the stack.
    Register argv = ABIArgGenerator::NonArgReturnVolatileReg0;
    Register scratch = ABIArgGenerator::NonArgReturnVolatileReg1;
#if defined(JS_CODEGEN_X86)
    masm.loadPtr(Address(StackPointer, NativeFrameSize + masm.framePushed()), argv);
#else
    masm.movePtr(IntArgReg0, argv);
#endif
    masm.Push(argv);

    // Bump the stack for the call.
    const ModuleCompiler::Func &func = *m.lookupFunction(exportedFunc.name());
    unsigned stackDec = StackDecrementForCall(masm, func.sig().args());
    masm.reserveStack(stackDec);

    // Copy parameters out of argv and into the registers/stack-slots specified by
    // the system ABI.
    for (ABIArgTypeIter iter(func.sig().args()); !iter.done(); iter++) {
        unsigned argOffset = iter.index() * sizeof(uint64_t);
        Address src(argv, argOffset);
        switch (iter->kind()) {
          case ABIArg::GPR:
            masm.load32(src, iter->gpr());
            break;
          case ABIArg::FPU:
            masm.loadDouble(src, iter->fpu());
            break;
          case ABIArg::Stack:
            if (iter.mirType() == MIRType_Int32) {
                masm.load32(src, scratch);
                masm.storePtr(scratch, Address(StackPointer, iter->offsetFromArgBase()));
            } else {
                JS_ASSERT(iter.mirType() == MIRType_Double || iter.mirType() == MIRType_Float32);
                masm.loadDouble(src, ScratchFloatReg);
                masm.storeDouble(ScratchFloatReg, Address(StackPointer, iter->offsetFromArgBase()));
            }
            break;
        }
    }

    // Call into the real function.
    AssertStackAlignment(masm);
    masm.call(func.code());

    // Pop the stack and recover the original 'argv' argument passed to the
    // trampoline (which was pushed on the stack).
    masm.freeStack(stackDec);
    masm.Pop(argv);

    // Store the return value in argv[0]
    switch (func.sig().retType().which()) {
      case RetType::Void:
        break;
      case RetType::Signed:
        masm.storeValue(JSVAL_TYPE_INT32, ReturnReg, Address(argv, 0));
        break;
      case RetType::Float:
        masm.convertFloat32ToDouble(ReturnFloatReg, ReturnFloatReg);
        // Fall through as ReturnFloatReg now contains a Double
      case RetType::Double:
        masm.canonicalizeDouble(ReturnFloatReg);
        masm.storeDouble(ReturnFloatReg, Address(argv, 0));
        break;
    }

    // Restore clobbered non-volatile registers of the caller.
    masm.PopRegsInMask(NonVolatileRegs);

    JS_ASSERT(masm.framePushed() == 0);

    masm.move32(Imm32(true), ReturnReg);
    masm.abiret();
    return true;
}

static inline bool
TryEnablingIon(JSContext *cx, AsmJSModule &module, HandleFunction fun, uint32_t exitIndex,
               int32_t argc, Value *argv)
{
    if (!fun->hasScript())
        return true;

    // Test if the function is Ion compiled
    JSScript *script = fun->nonLazyScript();
    if (!script->hasIonScript())
        return true;

    // Currently we can't rectify arguments. Therefore disabling if argc is too low.
    if (fun->nargs() > size_t(argc))
        return true;

    // Normally the types should corresond, since we just ran with those types,
    // but there are reports this is asserting. Therefore doing it as a check, instead of DEBUG only.
    if (!types::TypeScript::ThisTypes(script)->hasType(types::Type::UndefinedType()))
        return true;
    for(uint32_t i = 0; i < fun->nargs(); i++) {
        types::StackTypeSet *typeset = types::TypeScript::ArgTypes(script, i);
        types::Type type = types::Type::DoubleType();
        if (!argv[i].isDouble())
            type = types::Type::PrimitiveType(argv[i].extractNonDoubleType());
        if (!typeset->hasType(type))
            return true;
    }

    // Enable
    IonScript *ionScript = script->ionScript();
    if (!ionScript->addDependentAsmJSModule(cx, DependentAsmJSModuleExit(&module, exitIndex)))
        return false;

    module.exitIndexToGlobalDatum(exitIndex).exit = module.ionExitTrampoline(module.exit(exitIndex));
    return true;
}

namespace js {

int32_t
InvokeFromAsmJS_Ignore(JSContext *cx, int32_t exitIndex, int32_t argc, Value *argv)
{
    AsmJSModule &module = cx->mainThread().asmJSActivationStackFromOwnerThread()->module();

    RootedFunction fun(cx, module.exitIndexToGlobalDatum(exitIndex).fun);
    RootedValue fval(cx, ObjectValue(*fun));
    RootedValue rval(cx);
    if (!Invoke(cx, UndefinedValue(), fval, argc, argv, &rval))
        return false;

    if (!TryEnablingIon(cx, module, fun, exitIndex, argc, argv))
        return false;

    return true;
}

int32_t
InvokeFromAsmJS_ToInt32(JSContext *cx, int32_t exitIndex, int32_t argc, Value *argv)
{
    AsmJSModule &module = cx->mainThread().asmJSActivationStackFromOwnerThread()->module();

    RootedFunction fun(cx, module.exitIndexToGlobalDatum(exitIndex).fun);
    RootedValue fval(cx, ObjectValue(*fun));
    RootedValue rval(cx);
    if (!Invoke(cx, UndefinedValue(), fval, argc, argv, &rval))
        return false;

    if (!TryEnablingIon(cx, module, fun, exitIndex, argc, argv))
        return false;

    int32_t i32;
    if (!ToInt32(cx, rval, &i32))
        return false;
    argv[0] = Int32Value(i32);

    return true;
}

int32_t
InvokeFromAsmJS_ToNumber(JSContext *cx, int32_t exitIndex, int32_t argc, Value *argv)
{
    AsmJSModule &module = cx->mainThread().asmJSActivationStackFromOwnerThread()->module();

    RootedFunction fun(cx, module.exitIndexToGlobalDatum(exitIndex).fun);
    RootedValue fval(cx, ObjectValue(*fun));
    RootedValue rval(cx);
    if (!Invoke(cx, UndefinedValue(), fval, argc, argv, &rval))
        return false;

    if (!TryEnablingIon(cx, module, fun, exitIndex, argc, argv))
        return false;

    double dbl;
    if (!ToNumber(cx, rval, &dbl))
        return false;
    argv[0] = DoubleValue(dbl);

    return true;
}

}  // namespace js

static void
FillArgumentArray(ModuleCompiler &m, const VarTypeVector &argTypes,
                  unsigned offsetToArgs, unsigned offsetToCallerStackArgs,
                  Register scratch)
{
    MacroAssembler &masm = m.masm();

    for (ABIArgTypeIter i(argTypes); !i.done(); i++) {
        Address dstAddr = Address(StackPointer, offsetToArgs + i.index() * sizeof(Value));
        switch (i->kind()) {
          case ABIArg::GPR:
            masm.storeValue(JSVAL_TYPE_INT32, i->gpr(), dstAddr);
            break;
          case ABIArg::FPU: {
              masm.canonicalizeDouble(i->fpu());
              masm.storeDouble(i->fpu(), dstAddr);
              break;
          }
          case ABIArg::Stack:
            if (i.mirType() == MIRType_Int32) {
                Address src(StackPointer, offsetToCallerStackArgs + i->offsetFromArgBase());
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
                masm.load32(src, scratch);
                masm.storeValue(JSVAL_TYPE_INT32, scratch, dstAddr);
#else
                masm.memIntToValue(src, dstAddr);
#endif
            } else {
                JS_ASSERT(i.mirType() == MIRType_Double);
                Address src(StackPointer, offsetToCallerStackArgs + i->offsetFromArgBase());
                masm.loadDouble(src, ScratchFloatReg);
                masm.canonicalizeDouble(ScratchFloatReg);
                masm.storeDouble(ScratchFloatReg, dstAddr);
            }
            break;
        }
    }
}

static void
GenerateFFIInterpreterExit(ModuleCompiler &m, const ModuleCompiler::ExitDescriptor &exit,
                           unsigned exitIndex, Label *throwLabel)
{
    MacroAssembler &masm = m.masm();
    masm.align(CodeAlignment);
    m.setInterpExitOffset(exitIndex);
    masm.setFramePushed(0);

#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    MIRType typeArray[] = { MIRType_Pointer,   // cx
                            MIRType_Pointer,   // exitDatum
                            MIRType_Int32,     // argc
                            MIRType_Pointer }; // argv
    MIRTypeVector invokeArgTypes(m.cx());
    invokeArgTypes.infallibleAppend(typeArray, ArrayLength(typeArray));

    // Reserve space for a call to InvokeFromAsmJS_* and an array of values
    // passed to this FFI call.
    unsigned arraySize = Max<size_t>(1, exit.sig().args().length()) * sizeof(Value);
    unsigned stackDec = StackDecrementForCall(masm, invokeArgTypes, arraySize);
    masm.reserveStack(stackDec);

    // Fill the argument array.
    unsigned offsetToCallerStackArgs = NativeFrameSize + masm.framePushed();
    unsigned offsetToArgv = StackArgBytes(invokeArgTypes);
    Register scratch = ABIArgGenerator::NonArgReturnVolatileReg0;
    FillArgumentArray(m, exit.sig().args(), offsetToArgv, offsetToCallerStackArgs, scratch);

    // Prepare the arguments for the call to InvokeFromAsmJS_*.
    ABIArgMIRTypeIter i(invokeArgTypes);
    Register activation = ABIArgGenerator::NonArgReturnVolatileReg1;
    LoadAsmJSActivationIntoRegister(masm, activation);

    // argument 0: cx
    if (i->kind() == ABIArg::GPR) {
        LoadJSContextFromActivation(masm, activation, i->gpr());
    } else {
        LoadJSContextFromActivation(masm, activation, scratch);
        masm.storePtr(scratch, Address(StackPointer, i->offsetFromArgBase()));
    }
    i++;

    // argument 1: exitIndex
    if (i->kind() == ABIArg::GPR)
        masm.mov(ImmWord(exitIndex), i->gpr());
    else
        masm.store32(Imm32(exitIndex), Address(StackPointer, i->offsetFromArgBase()));
    i++;

    // argument 2: argc
    unsigned argc = exit.sig().args().length();
    if (i->kind() == ABIArg::GPR)
        masm.mov(ImmWord(argc), i->gpr());
    else
        masm.store32(Imm32(argc), Address(StackPointer, i->offsetFromArgBase()));
    i++;

    // argument 3: argv
    Address argv(StackPointer, offsetToArgv);
    if (i->kind() == ABIArg::GPR) {
        masm.computeEffectiveAddress(argv, i->gpr());
    } else {
        masm.computeEffectiveAddress(argv, scratch);
        masm.storePtr(scratch, Address(StackPointer, i->offsetFromArgBase()));
    }
    i++;
    JS_ASSERT(i.done());

    // Make the call, test whether it succeeded, and extract the return value.
    AssertStackAlignment(masm);
    switch (exit.sig().retType().which()) {
      case RetType::Void:
        masm.call(AsmJSImm_InvokeFromAsmJS_Ignore);
        masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);
        break;
      case RetType::Signed:
        masm.call(AsmJSImm_InvokeFromAsmJS_ToInt32);
        masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);
        masm.unboxInt32(argv, ReturnReg);
        break;
      case RetType::Double:
        masm.call(AsmJSImm_InvokeFromAsmJS_ToNumber);
        masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);
        masm.loadDouble(argv, ReturnFloatReg);
        break;
      case RetType::Float:
        MOZ_ASSUME_UNREACHABLE("Float32 shouldn't be returned from a FFI");
        break;
    }

    // Note: the caller is IonMonkey code which means there are no non-volatile
    // registers to restore.
    masm.freeStack(stackDec);
    masm.ret();
#else
    const unsigned arrayLength = Max<size_t>(1, exit.sig().args().length());
    const unsigned arraySize = arrayLength * sizeof(Value);
    const unsigned reserveSize = AlignBytes(arraySize, StackAlignment) +
        ShadowStackSpace;
    const unsigned callerArgsOffset = reserveSize + NativeFrameSize + sizeof(int32_t);
    masm.setFramePushed(0);
    masm.Push(lr);
    masm.reserveStack(reserveSize + sizeof(int32_t));

    // Store arguments
    FillArgumentArray(m, exit.sig().args(), ShadowStackSpace, callerArgsOffset, IntArgReg0);

    // argument 0: cx
    Register activation = IntArgReg3;
    LoadAsmJSActivationIntoRegister(masm, activation);

    LoadJSContextFromActivation(masm, activation, IntArgReg0);

    // argument 1: exitIndex
    masm.mov(ImmWord(exitIndex), IntArgReg1);

    // argument 2: argc
    masm.mov(ImmWord(exit.sig().args().length()), IntArgReg2);

    // argument 3: argv
    Address argv(StackPointer, ShadowStackSpace);
    masm.lea(Operand(argv), IntArgReg3);

    AssertStackAlignment(masm);
    switch (exit.sig().retType().which()) {
      case RetType::Void:
        masm.call(AsmJSImm_InvokeFromAsmJS_Ignore);
        masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);
        break;
      case RetType::Signed:
        masm.call(AsmJSImm_InvokeFromAsmJS_ToInt32);
        masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);
        masm.unboxInt32(argv, ReturnReg);
        break;
      case RetType::Double:
        masm.call(AsmJSImm_InvokeFromAsmJS_ToNumber);
        masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);
        masm.loadDouble(argv, ReturnFloatReg);
        break;
      case RetType::Float:
        MOZ_ASSUME_UNREACHABLE("Float32 shouldn't be returned from a FFI");
        break;
    }

    masm.freeStack(reserveSize + sizeof(int32_t));
    masm.ret();
#endif
}

static void
GenerateOOLConvert(ModuleCompiler &m, RetType retType, Label *throwLabel)
{
    MacroAssembler &masm = m.masm();

    MIRType typeArray[] = { MIRType_Pointer,   // cx
                            MIRType_Pointer }; // argv
    MIRTypeVector callArgTypes(m.cx());
    callArgTypes.infallibleAppend(typeArray, ArrayLength(typeArray));

    // Reserve space for a call to InvokeFromAsmJS_* and an array of values
    // passed to this FFI call.
    unsigned arraySize = sizeof(Value);
    unsigned stackDec = StackDecrementForCall(masm, callArgTypes, arraySize);
    masm.reserveStack(stackDec);

    // Store value
    unsigned offsetToArgv = StackArgBytes(callArgTypes);
    masm.storeValue(JSReturnOperand, Address(StackPointer, offsetToArgv));

    // Store real arguments
    ABIArgMIRTypeIter i(callArgTypes);
    Register scratch = ABIArgGenerator::NonArgReturnVolatileReg0;

    // argument 0: cx
    Register activation = ABIArgGenerator::NonArgReturnVolatileReg1;
    LoadAsmJSActivationIntoRegister(masm, activation);
    if (i->kind() == ABIArg::GPR) {
        LoadJSContextFromActivation(masm, activation, i->gpr());
    } else {
        LoadJSContextFromActivation(masm, activation, scratch);
        masm.storePtr(scratch, Address(StackPointer, i->offsetFromArgBase()));
    }
    i++;

    // argument 1: argv
    Address argv(StackPointer, offsetToArgv);
    if (i->kind() == ABIArg::GPR) {
        masm.computeEffectiveAddress(argv, i->gpr());
    } else {
        masm.computeEffectiveAddress(argv, scratch);
        masm.storePtr(scratch, Address(StackPointer, i->offsetFromArgBase()));
    }
    i++;
    JS_ASSERT(i.done());

    // Call
    AssertStackAlignment(masm);
    switch (retType.which()) {
      case RetType::Signed:
          masm.call(AsmJSImm_CoerceInPlace_ToInt32);
          masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);
          masm.unboxInt32(Address(StackPointer, offsetToArgv), ReturnReg);
          break;
      case RetType::Double:
          masm.call(AsmJSImm_CoerceInPlace_ToNumber);
          masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);
          masm.loadDouble(Address(StackPointer, offsetToArgv), ReturnFloatReg);
          break;
      default:
          MOZ_ASSUME_UNREACHABLE("Unsupported convert type");
    }

    masm.freeStack(stackDec);
}

static void
GenerateFFIIonExit(ModuleCompiler &m, const ModuleCompiler::ExitDescriptor &exit,
                         unsigned exitIndex, Label *throwLabel)
{
    MacroAssembler &masm = m.masm();
    masm.align(CodeAlignment);
    m.setIonExitOffset(exitIndex);
    masm.setFramePushed(0);

    RegisterSet restoreSet = RegisterSet::Intersect(RegisterSet::All(),
                                                    RegisterSet::Not(RegisterSet::Volatile()));
#if defined(JS_CODEGEN_ARM)
    masm.Push(lr);
#endif
    masm.PushRegsInMask(restoreSet);

    // Arguments are in the following order on the stack:
    // descriptor | callee | argc | this | arg1 | arg2 | ...

    // Reserve and align space for the arguments
    MIRTypeVector emptyVector(m.cx());
    unsigned argBytes = 3 * sizeof(size_t) + (1 + exit.sig().args().length()) * sizeof(Value);
    unsigned extraBytes = 0;
#if defined(JS_CODEGEN_ARM)
    extraBytes += sizeof(size_t);
#endif
    unsigned stackDec = StackDecrementForCall(masm, emptyVector, argBytes + extraBytes);
    masm.reserveStack(stackDec - extraBytes);

    // 1. Descriptor
    uint32_t descriptor = MakeFrameDescriptor(masm.framePushed() + extraBytes, JitFrame_Entry);
    masm.storePtr(ImmWord(uintptr_t(descriptor)), Address(StackPointer, 0));

    // 2. Callee
    Register callee = ABIArgGenerator::NonArgReturnVolatileReg0;
    Register scratch = ABIArgGenerator::NonArgReturnVolatileReg1;

    // 2.1. Get ExitDatum
    unsigned globalDataOffset = m.module().exitIndexToGlobalDataOffset(exitIndex);
#if defined(JS_CODEGEN_X64)
    CodeOffsetLabel label2 = masm.leaRipRelative(callee);
    m.addGlobalAccess(AsmJSGlobalAccess(label2.offset(), globalDataOffset));
#elif defined(JS_CODEGEN_X86)
    CodeOffsetLabel label2 = masm.movlWithPatch(Imm32(0), callee);
    m.addGlobalAccess(AsmJSGlobalAccess(label2.offset(), globalDataOffset));
#else
    masm.lea(Operand(GlobalReg, globalDataOffset), callee);
#endif

    // 2.2. Get callee
    masm.loadPtr(Address(callee, offsetof(AsmJSModule::ExitDatum, fun)), callee);

    // 2.3. Save callee
    masm.storePtr(callee, Address(StackPointer, sizeof(size_t)));

    // 3. Argc
    unsigned argc = exit.sig().args().length();
    masm.storePtr(ImmWord(uintptr_t(argc)), Address(StackPointer, 2 * sizeof(size_t)));

    // 4. |this| value
    masm.storeValue(UndefinedValue(), Address(StackPointer, 3 * sizeof(size_t)));

    // 5. Fill the arguments
    unsigned offsetToArgs = 3 * sizeof(size_t) + sizeof(Value);
    unsigned offsetToCallerStackArgs = masm.framePushed();
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    offsetToCallerStackArgs += NativeFrameSize;
#else
    offsetToCallerStackArgs += ShadowStackSpace;
#endif
    FillArgumentArray(m, exit.sig().args(), offsetToArgs, offsetToCallerStackArgs, scratch);

    // Get the pointer to the ion code
    Label done, oolConvert;
    Label *maybeDebugBreakpoint = nullptr;

#ifdef DEBUG
    Label ionFailed;
    maybeDebugBreakpoint = &ionFailed;
    masm.branchIfFunctionHasNoScript(callee, &ionFailed);
#endif

    masm.loadPtr(Address(callee, JSFunction::offsetOfNativeOrScript()), scratch);
    masm.loadBaselineOrIonNoArgCheck(scratch, scratch, SequentialExecution, maybeDebugBreakpoint);

    LoadAsmJSActivationIntoRegister(masm, callee);
    masm.push(scratch);
    masm.setupUnalignedABICall(1, scratch);
    masm.passABIArg(callee);
    masm.callWithABI(AsmJSImm_EnableActivationFromAsmJS);
    masm.pop(scratch);

    // 2. Call
#if defined(JS_CODEGEN_ARM) && defined(DEBUG)
    // ARM still needs to push, before stack is aligned
    masm.Push(scratch);
#endif
    AssertStackAlignment(masm);
#if defined(JS_CODEGEN_ARM) && defined(DEBUG)
    masm.freeStack(sizeof(size_t));
#endif
    masm.callIon(scratch);
    masm.freeStack(stackDec - extraBytes);

    masm.push(JSReturnReg_Type);
    masm.push(JSReturnReg_Data);
    LoadAsmJSActivationIntoRegister(masm, callee);
    masm.setupUnalignedABICall(1, scratch);
    masm.passABIArg(callee);
    masm.callWithABI(AsmJSImm_DisableActivationFromAsmJS);
    masm.pop(JSReturnReg_Data);
    masm.pop(JSReturnReg_Type);

#ifdef DEBUG
    masm.branchTestMagicValue(Assembler::Equal, JSReturnOperand, JS_ION_ERROR, throwLabel);
    masm.branchTestMagic(Assembler::Equal, JSReturnOperand, &ionFailed);
#else
    masm.branchTestMagic(Assembler::Equal, JSReturnOperand, throwLabel);
#endif

    uint32_t oolConvertFramePushed = masm.framePushed();
    switch (exit.sig().retType().which()) {
      case RetType::Void:
        break;
      case RetType::Signed:
        masm.convertValueToInt32(JSReturnOperand, ReturnFloatReg, ReturnReg, &oolConvert,
                                 /* -0 check */ false);
        break;
      case RetType::Double:
        masm.convertValueToDouble(JSReturnOperand, ReturnFloatReg, &oolConvert);
        break;
      case RetType::Float:
        MOZ_ASSUME_UNREACHABLE("Float shouldn't be returned from a FFI");
        break;
    }

    masm.bind(&done);
    masm.PopRegsInMask(restoreSet);
    masm.ret();

    // oolConvert
    if (oolConvert.used()) {
        masm.bind(&oolConvert);
        masm.setFramePushed(oolConvertFramePushed);
        GenerateOOLConvert(m, exit.sig().retType(), throwLabel);
        masm.setFramePushed(0);
        masm.jump(&done);
    }

#ifdef DEBUG
    masm.bind(&ionFailed);
    masm.assumeUnreachable("AsmJS to IonMonkey call failed.");
#endif
}

// See "asm.js FFI calls" comment above.
static void
GenerateFFIExit(ModuleCompiler &m, const ModuleCompiler::ExitDescriptor &exit, unsigned exitIndex,
                Label *throwLabel)
{
    // Generate the slow path through the interpreter
    GenerateFFIInterpreterExit(m, exit, exitIndex, throwLabel);

    // Generate the fast path
    GenerateFFIIonExit(m, exit, exitIndex, throwLabel);
}

// The stack-overflow exit is called when the stack limit has definitely been
// exceeded. In this case, we can clobber everything since we are about to pop
// all the frames.
static bool
GenerateStackOverflowExit(ModuleCompiler &m, Label *throwLabel)
{
    MacroAssembler &masm = m.masm();
    masm.align(CodeAlignment);
    masm.bind(&m.stackOverflowLabel());

#if defined(JS_CODEGEN_X86)
    // Ensure that at least one slot is pushed for passing 'cx' below.
    masm.push(Imm32(0));
#endif

    // We know that StackPointer is word-aligned, but nothing past that. Thus,
    // we must align StackPointer dynamically. Don't worry about restoring
    // StackPointer since throwLabel will clobber StackPointer immediately.
    masm.andPtr(Imm32(~(StackAlignment - 1)), StackPointer);
    if (ShadowStackSpace)
        masm.subPtr(Imm32(ShadowStackSpace), StackPointer);

    // Prepare the arguments for the call to js_ReportOverRecursed.
#if defined(JS_CODEGEN_X86)
    LoadAsmJSActivationIntoRegister(masm, eax);
    LoadJSContextFromActivation(masm, eax, eax);
    masm.storePtr(eax, Address(StackPointer, 0));
#else
    LoadAsmJSActivationIntoRegister(masm, IntArgReg0);
    LoadJSContextFromActivation(masm, IntArgReg0, IntArgReg0);
#endif
    masm.call(AsmJSImm_ReportOverRecursed);
    masm.jump(throwLabel);

    return !masm.oom();
}

// The operation-callback exit is called from arbitrarily-interrupted asm.js
// code. That means we must first save *all* registers and restore *all*
// registers (except the stack pointer) when we resume. The address to resume to
// (assuming that js::HandleExecutionInterrupt doesn't indicate that the
// execution should be aborted) is stored in AsmJSActivation::resumePC_.
// Unfortunately, loading this requires a scratch register which we don't have
// after restoring all registers. To hack around this, push the resumePC on the
// stack so that it can be popped directly into PC.
static bool
GenerateInterruptExit(ModuleCompiler &m, Label *throwLabel)
{
    MacroAssembler &masm = m.masm();
    masm.align(CodeAlignment);
    masm.bind(&m.interruptLabel());

#ifndef JS_CODEGEN_ARM
    // Be very careful here not to perturb the machine state before saving it
    // to the stack. In particular, add/sub instructions may set conditions in
    // the flags register.
    masm.push(Imm32(0));            // space for resumePC
    masm.pushFlags();               // after this we are safe to use sub
    masm.setFramePushed(0);         // set to zero so we can use masm.framePushed() below
    masm.PushRegsInMask(AllRegsExceptSP); // save all GP/FP registers (except SP)

    Register activation = ABIArgGenerator::NonArgReturnVolatileReg0;
    Register scratch = ABIArgGenerator::NonArgReturnVolatileReg1;

    // Store resumePC into the reserved space.
    LoadAsmJSActivationIntoRegister(masm, activation);
    masm.loadPtr(Address(activation, AsmJSActivation::offsetOfResumePC()), scratch);
    masm.storePtr(scratch, Address(StackPointer, masm.framePushed() + sizeof(void*)));

    // We know that StackPointer is word-aligned, but not necessarily
    // stack-aligned, so we need to align it dynamically.
    masm.mov(StackPointer, ABIArgGenerator::NonVolatileReg);
#if defined(JS_CODEGEN_X86)
    // Ensure that at least one slot is pushed for passing 'cx' below.
    masm.push(Imm32(0));
#endif
    masm.andPtr(Imm32(~(StackAlignment - 1)), StackPointer);
    if (ShadowStackSpace)
        masm.subPtr(Imm32(ShadowStackSpace), StackPointer);

    // argument 0: cx
#if defined(JS_CODEGEN_X86)
    LoadJSContextFromActivation(masm, activation, scratch);
    masm.storePtr(scratch, Address(StackPointer, 0));
#elif defined(JS_CODEGEN_X64)
    LoadJSContextFromActivation(masm, activation, IntArgReg0);
#endif

    masm.call(AsmJSImm_HandleExecutionInterrupt);
    masm.branchIfFalseBool(ReturnReg, throwLabel);

    // Restore the StackPointer to it's position before the call.
    masm.mov(ABIArgGenerator::NonVolatileReg, StackPointer);

    // Restore the machine state to before the interrupt.
    masm.PopRegsInMask(AllRegsExceptSP); // restore all GP/FP registers (except SP)
    masm.popFlags();              // after this, nothing that sets conditions
    masm.ret();                   // pop resumePC into PC
#else
    masm.setFramePushed(0);         // set to zero so we can use masm.framePushed() below
    masm.PushRegsInMask(RegisterSet(GeneralRegisterSet(Registers::AllMask & ~(1<<Registers::sp)), FloatRegisterSet(uint32_t(0))));   // save all GP registers,excep sp

    // Save both the APSR and FPSCR in non-volatile registers.
    masm.as_mrs(r4);
    masm.as_vmrs(r5);
    // Save the stack pointer in a non-volatile register.
    masm.mov(sp,r6);
    // Align the stack.
    masm.ma_and(Imm32(~7), sp, sp);

    // Store resumePC into the return PC stack slot.
    LoadAsmJSActivationIntoRegister(masm, IntArgReg0);
    masm.loadPtr(Address(IntArgReg0, AsmJSActivation::offsetOfResumePC()), IntArgReg1);
    masm.storePtr(IntArgReg1, Address(r6, 14 * sizeof(uint32_t*)));

    // argument 0: cx
    masm.loadPtr(Address(IntArgReg0, AsmJSActivation::offsetOfContext()), IntArgReg0);

    masm.PushRegsInMask(RegisterSet(GeneralRegisterSet(0), FloatRegisterSet(FloatRegisters::AllMask)));   // save all FP registers
    masm.call(AsmJSImm_HandleExecutionInterrupt);
    masm.branchIfFalseBool(ReturnReg, throwLabel);

    // Restore the machine state to before the interrupt. this will set the pc!
    masm.PopRegsInMask(RegisterSet(GeneralRegisterSet(0), FloatRegisterSet(FloatRegisters::AllMask)));   // restore all FP registers
    masm.mov(r6,sp);
    masm.as_vmsr(r5);
    masm.as_msr(r4);
    // Restore all GP registers
    masm.startDataTransferM(IsLoad, sp, IA, WriteBack);
    masm.transferReg(r0);
    masm.transferReg(r1);
    masm.transferReg(r2);
    masm.transferReg(r3);
    masm.transferReg(r4);
    masm.transferReg(r5);
    masm.transferReg(r6);
    masm.transferReg(r7);
    masm.transferReg(r8);
    masm.transferReg(r9);
    masm.transferReg(r10);
    masm.transferReg(r11);
    masm.transferReg(r12);
    masm.transferReg(lr);
    masm.finishDataTransfer();
    masm.ret();

#endif

    return !masm.oom();
}

// If an exception is thrown, simply pop all frames (since asm.js does not
// contain try/catch). To do this:
//  1. Restore 'sp' to it's value right after the PushRegsInMask in GenerateEntry.
//  2. PopRegsInMask to restore the caller's non-volatile registers.
//  3. Return (to CallAsmJS).
static bool
GenerateThrowExit(ModuleCompiler &m, Label *throwLabel)
{
    MacroAssembler &masm = m.masm();
    masm.align(CodeAlignment);
    masm.bind(throwLabel);

    Register activation = ABIArgGenerator::NonArgReturnVolatileReg0;
    LoadAsmJSActivationIntoRegister(masm, activation);

    masm.setFramePushed(FramePushedAfterSave);
    masm.loadPtr(Address(activation, AsmJSActivation::offsetOfErrorRejoinSP()), StackPointer);

    masm.PopRegsInMask(NonVolatileRegs);
    JS_ASSERT(masm.framePushed() == 0);

    masm.mov(ImmWord(0), ReturnReg);
    masm.abiret();

    return !masm.oom();
}

static bool
GenerateStubs(ModuleCompiler &m)
{
    for (unsigned i = 0; i < m.module().numExportedFunctions(); i++) {
        m.setEntryOffset(i);
        if (!GenerateEntry(m, m.module().exportedFunction(i)))
            return false;
    }

    Label throwLabel;

    // The order of the iterations here is non-deterministic, since
    // m.allExits() is a hash keyed by pointer values!
    for (ModuleCompiler::ExitMap::Range r = m.allExits(); !r.empty(); r.popFront()) {
        GenerateFFIExit(m, r.front().key(), r.front().value(), &throwLabel);
        if (m.masm().oom())
            return false;
    }

    if (m.stackOverflowLabel().used()) {
        if (!GenerateStackOverflowExit(m, &throwLabel))
            return false;
    }

    if (!GenerateInterruptExit(m, &throwLabel))
        return false;

    if (!GenerateThrowExit(m, &throwLabel))
        return false;

    return true;
}

static bool
FinishModule(ModuleCompiler &m,
             ScopedJSDeletePtr<AsmJSModule> *module)
{
    LifoAlloc lifo(LIFO_ALLOC_PRIMARY_CHUNK_SIZE);
    TempAllocator alloc(&lifo);
    IonContext ionContext(m.cx(), &alloc);

    m.masm().resetForNewCodeGenerator(alloc);

    if (!GenerateStubs(m))
        return false;

    return m.finish(module);
}

static bool
CheckModule(ExclusiveContext *cx, AsmJSParser &parser, ParseNode *stmtList,
            ScopedJSDeletePtr<AsmJSModule> *moduleOut,
            ScopedJSFreePtr<char> *compilationTimeReport)
{
    if (!LookupAsmJSModuleInCache(cx, parser, moduleOut, compilationTimeReport))
        return false;
    if (*moduleOut)
        return true;

    ModuleCompiler m(cx, parser);
    if (!m.init())
        return false;

    if (PropertyName *moduleFunctionName = FunctionName(m.moduleFunctionNode())) {
        if (!CheckModuleLevelName(m, m.moduleFunctionNode(), moduleFunctionName))
            return false;
        m.initModuleFunctionName(moduleFunctionName);
    }

    if (!CheckFunctionHead(m, m.moduleFunctionNode()))
        return false;

    if (!CheckModuleArguments(m, m.moduleFunctionNode()))
        return false;

    if (!CheckPrecedingStatements(m, stmtList))
        return false;

    if (!CheckModuleGlobals(m))
        return false;

#ifdef JS_THREADSAFE
    if (!CheckFunctionsParallel(m))
        return false;
#else
    if (!CheckFunctionsSequential(m))
        return false;
#endif

    m.finishFunctionBodies();

    if (!CheckFuncPtrTables(m))
        return false;

    if (!CheckModuleReturn(m))
        return false;

    TokenKind tk = PeekToken(m.parser());
    if (tk != TOK_EOF && tk != TOK_RC)
        return m.fail(nullptr, "top-level export (return) must be the last statement");

    ScopedJSDeletePtr<AsmJSModule> module;
    if (!FinishModule(m, &module))
        return false;

    bool storedInCache = StoreAsmJSModuleInCache(parser, *module, cx);
    module->staticallyLink(cx);

    m.buildCompilationTimeReport(storedInCache, compilationTimeReport);
    *moduleOut = module.forget();
    return true;
}

static bool
Warn(AsmJSParser &parser, int errorNumber, const char *str)
{
    parser.reportNoOffset(ParseWarning, /* strict = */ false, errorNumber, str ? str : "");
    return false;
}

static bool
EstablishPreconditions(ExclusiveContext *cx, AsmJSParser &parser)
{
    if (!cx->jitSupportsFloatingPoint())
        return Warn(parser, JSMSG_USE_ASM_TYPE_FAIL, "Disabled by lack of floating point support");

    if (!cx->signalHandlersInstalled())
        return Warn(parser, JSMSG_USE_ASM_TYPE_FAIL, "Platform missing signal handler support");

    if (cx->gcSystemPageSize() != AsmJSPageSize)
        return Warn(parser, JSMSG_USE_ASM_TYPE_FAIL, "Disabled by non 4KiB system page size");

    if (!parser.options().asmJSOption)
        return Warn(parser, JSMSG_USE_ASM_TYPE_FAIL, "Disabled by javascript.options.asmjs in about:config");

    if (!parser.options().compileAndGo)
        return Warn(parser, JSMSG_USE_ASM_TYPE_FAIL, "Temporarily disabled for event-handler and other cloneable scripts");

    if (cx->compartment()->debugMode())
        return Warn(parser, JSMSG_USE_ASM_TYPE_FAIL, "Disabled by debugger");

    if (parser.pc->isGenerator())
        return Warn(parser, JSMSG_USE_ASM_TYPE_FAIL, "Disabled by generator context");

#ifdef JS_THREADSAFE
    if (ParallelCompilationEnabled(cx)) {
        if (!EnsureWorkerThreadsInitialized(cx))
            return Warn(parser, JSMSG_USE_ASM_TYPE_FAIL, "Failed compilation thread initialization");
    }
#endif

    return true;
}

static bool
NoExceptionPending(ExclusiveContext *cx)
{
    return !cx->isJSContext() || !cx->asJSContext()->isExceptionPending();
}

bool
js::CompileAsmJS(ExclusiveContext *cx, AsmJSParser &parser, ParseNode *stmtList, bool *validated)
{
    *validated = false;

    if (!EstablishPreconditions(cx, parser))
        return NoExceptionPending(cx);

    ScopedJSFreePtr<char> compilationTimeReport;
    ScopedJSDeletePtr<AsmJSModule> module;
    if (!CheckModule(cx, parser, stmtList, &module, &compilationTimeReport))
        return NoExceptionPending(cx);

    RootedObject moduleObj(cx, AsmJSModuleObject::create(cx, &module));
    if (!moduleObj)
        return false;

    FunctionBox *funbox = parser.pc->maybeFunction->pn_funbox;
    RootedFunction moduleFun(cx, NewAsmJSModuleFunction(cx, funbox->function(), moduleObj));
    if (!moduleFun)
        return false;

    JS_ASSERT(funbox->function()->isInterpreted());
    funbox->object = moduleFun;

    *validated = true;
    Warn(parser, JSMSG_USE_ASM_TYPE_OK, compilationTimeReport.get());
    return NoExceptionPending(cx);
}

bool
js::IsAsmJSCompilationAvailable(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // See EstablishPreconditions.
    bool available = cx->jitSupportsFloatingPoint() &&
                     cx->signalHandlersInstalled() &&
                     cx->gcSystemPageSize() == AsmJSPageSize &&
                     !cx->compartment()->debugMode() &&
                     cx->runtime()->options().asmJS();

    args.rval().set(BooleanValue(available));
    return true;
}
